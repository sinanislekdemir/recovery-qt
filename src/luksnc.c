/*

    File: luksnc.c

    Copyright (C) 2024 Christophe GRENIER <grenier@cgsecurity.org>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "types.h"
#include "common.h"
#include "intrfn.h"
#include "log.h"
#include "luksnc.h"

#ifdef HAVE_NCURSES
char *ask_luks_passphrase(void)
{
  WINDOW *win;
  static char passphrase[256];
  int pos;
  int y, x;

  y = (LINES - 6) / 2;
  x = (COLS - 60) / 2;
  if (y < 0) y = 0;
  if (x < 0) x = 0;
  win = newwin(6, 60, y, x);
  if (win == NULL)
  {
    passphrase[0] = '\0';
    return passphrase;
  }

  wbkgd(win, ' ' | COLOR_PAIR(CP_SPECIAL));
  wattrset(win, COLOR_PAIR(CP_SPECIAL) | A_BOLD);
  wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');
  wattroff(win, COLOR_PAIR(CP_SPECIAL) | A_BOLD);
  mvwprintw(win, 1, 2, "LUKS encrypted partition detected.");
  mvwprintw(win, 2, 2, "Enter passphrase:");
  mvwaddch(win, 3, 2, ' ');
  keypad(win, TRUE);
  nodelay(win, FALSE);
  wrefresh(win);
  redrawwin(win);

  noecho();
  curs_set(1);
  pos = 0;
  passphrase[0] = '\0';

  while (1)
  {
    int car;
    car = wgetch(win);
    if (car == '\n' || car == '\r' || car == KEY_ENTER)
      break;
    if (car == 27)
    {
      passphrase[0] = '\0';
      break;
    }
    if ((car == KEY_BACKSPACE || car == 127 || car == '\b') && pos > 0)
    {
      pos--;
      passphrase[pos] = '\0';
    }
    else if (car >= 32 && car < 127 && pos < 250)
    {
      passphrase[pos++] = (char)car;
      passphrase[pos] = '\0';
    }
    else
    {
      continue;
    }
    mvwprintw(win, 3, 2, "%-50s", "");
    {
      int i;
      for (i = 0; i < pos; i++)
        mvwaddch(win, 3, 2 + i, '*');
    }
    wmove(win, 3, 2 + pos);
    wrefresh(win);
  }

  noecho();
  curs_set(0);
  delwin(win);
  (void) clearok(stdscr, TRUE);
  touchwin(stdscr);
  wrefresh(stdscr);

  return passphrase;
}
#endif

static char g_loop_device[256] = "";

int luks_open(const char *device, uint64_t offset, const char *passphrase, char *mapper_name, size_t mapper_name_size)
{
  const char *cryptsetup_paths[] = {
    "/usr/sbin/cryptsetup",
    "/sbin/cryptsetup",
    "cryptsetup",
    NULL
  };
  char cmd[2048];
  char name[64];
  int i;
  int found;
  struct stat st;

  log_info("luks_open: ENTER device=%s offset=%llu passlen=%zu\n",
      device, (unsigned long long)offset, strlen(passphrase));

  g_loop_device[0] = '\0';

  found = 0;
  for (i = 0; cryptsetup_paths[i] != NULL; i++)
  {
    if (stat(cryptsetup_paths[i], &st) == 0)
    {
      found = 1;
      break;
    }
  }
  if (!found && system("which cryptsetup >/dev/null 2>&1") == 0)
  {
    i = 0;
    found = 1;
  }
  if (!found)
  {
    log_error("luks_open: cryptsetup not found\n");
    return -1;
  }

  snprintf(name, sizeof(name), "recovery_qt_%ld", (long)time(NULL));
  {
    char errfile[256];
    const char *cryptsetup_bin;
    char device_to_open[1024];
    char loop_dev[64];
    int use_loop;

    cryptsetup_bin = cryptsetup_paths[found ? i : 0];
    snprintf(errfile, sizeof(errfile), "/tmp/recovery_qt_luks_%ld.err",
        (long)time(NULL));
    log_info("luks_open: using cryptsetup=%s errfile=%s\n",
        cryptsetup_bin, errfile);

    use_loop = 0;
    loop_dev[0] = '\0';

    if (offset > 0)
    {
      char losetup_cmd[2048];
      FILE *lfp;

      log_info("luks_open: offset=%llu, creating loop device\n",
          (unsigned long long)offset);
      snprintf(losetup_cmd, sizeof(losetup_cmd),
          "losetup --find --show --offset %llu '%s' 2>>%s",
          (unsigned long long)offset, device, errfile);
      log_info("luks_open: %s\n", losetup_cmd);
      lfp = popen(losetup_cmd, "r");
      if (lfp)
      {
        char *p;
        if (fgets(loop_dev, sizeof(loop_dev) - 1, lfp))
        {
          p = strchr(loop_dev, '\n');
          if (p) *p = '\0';
          if (loop_dev[0] == '/' && strlen(loop_dev) > 5)
          {
            use_loop = 1;
            snprintf(device_to_open, sizeof(device_to_open), "%s", loop_dev);
            snprintf(g_loop_device, sizeof(g_loop_device), "%s", loop_dev);
            {
              char marker[256];
              FILE *mf;
              snprintf(marker, sizeof(marker),
                  "/tmp/recovery_qt_loop_%d", (int)getpid());
              mf = fopen(marker, "w");
              if (mf)
              {
                fprintf(mf, "%s\n", g_loop_device);
                fclose(mf);
              }
            }
          }
        }
        pclose(lfp);
        log_info("luks_open: losetup result: use_loop=%d loop_dev=%s\n",
            use_loop, loop_dev);
      }
      else
      {
        log_error("luks_open: losetup popen failed: %s\n", strerror(errno));
      }
    }

    if (!use_loop)
    {
      snprintf(device_to_open, sizeof(device_to_open), "%s", device);
      log_info("luks_open: no loop device, opening directly: %s\n", device_to_open);
    }

    snprintf(cmd, sizeof(cmd),
        "%s luksOpen '%s' '%s' --key-file=- 2>>%s",
        cryptsetup_bin, device_to_open, name, errfile);

    log_info("luks_open: %s\n", cmd);

    {
      FILE *fp;
      int ret;
      fp = popen(cmd, "w");
      if (fp == NULL)
      {
        log_error("luks_open: popen failed: %s\n", strerror(errno));
        unlink(errfile);
        return -1;
      }
      fprintf(fp, "%s", passphrase);
      fflush(fp);
      ret = pclose(fp);
      if (ret != 0)
      {
        FILE *ef;
        char errbuf[1024];
        size_t n;
        ef = fopen(errfile, "r");
        errbuf[0] = '\0';
        if (ef)
        {
          n = fread(errbuf, 1, sizeof(errbuf) - 1, ef);
          if (n > 0)
            errbuf[n] = '\0';
          fclose(ef);
        }
        log_error("luks_open: cryptsetup failed (exit %d): %s\n",
            WEXITSTATUS(ret), errbuf);
#ifdef HAVE_NCURSES
        {
          char msg[1536];
          snprintf(msg, sizeof(msg), "cryptsetup failed: %s", errbuf);
          display_message(msg);
        }
#endif
        unlink(errfile);
        snprintf(cmd, sizeof(cmd), "%s close '%s'",
            cryptsetup_paths[found ? i : 0], name);
        if (system(cmd)) {}
        if (g_loop_device[0])
        {
          char detach_cmd[512];
          snprintf(detach_cmd, sizeof(detach_cmd), "losetup -d '%s'",
              g_loop_device);
          if (system(detach_cmd)) {}
          g_loop_device[0] = '\0';
        }
        return -2;
      }
      unlink(errfile);
    }
  }

  snprintf(mapper_name, mapper_name_size, "/dev/mapper/%s", name);
  return 0;
}

int luks_close(const char *mapper_name)
{
  const char *cryptsetup_paths[] = {
    "/usr/sbin/cryptsetup",
    "/sbin/cryptsetup",
    "cryptsetup",
    NULL
  };
  char cmd[512];
  int i;
  const char *name;

  name = strrchr(mapper_name, '/');
  if (name)
    name++;
  else
    name = mapper_name;

  for (i = 0; cryptsetup_paths[i] != NULL; i++)
  {
    struct stat st;
    if (stat(cryptsetup_paths[i], &st) == 0)
      break;
  }
  if (cryptsetup_paths[i] == NULL)
    i = 0;

  snprintf(cmd, sizeof(cmd), "%s close '%s'",
      cryptsetup_paths[i], name);
  if (system(cmd)) {}

  if (g_loop_device[0])
  {
    char marker[256];
    snprintf(cmd, sizeof(cmd), "losetup -d '%s'", g_loop_device);
  if (system(cmd)) {}

    snprintf(marker, sizeof(marker),
        "/tmp/recovery_qt_loop_%d", (int)getpid());
    unlink(marker);
    g_loop_device[0] = '\0';
  }

  return 0;
}

void luks_cleanup_orphans(void)
{
  FILE *fp;
  char line[256];
  const char *cryptsetup_bin;

  cryptsetup_bin = "/usr/sbin/cryptsetup";
  if (access(cryptsetup_bin, X_OK) != 0)
    cryptsetup_bin = "/sbin/cryptsetup";
  if (access(cryptsetup_bin, X_OK) != 0)
    cryptsetup_bin = "cryptsetup";

  fp = popen("dmsetup ls --target crypt 2>/dev/null | grep recovery_qt", "r");
  if (fp)
  {
    while (fgets(line, sizeof(line), fp))
    {
      char *p;
      char close_cmd[512];
      p = strchr(line, '\t');
      if (p) *p = '\0';
      p = strchr(line, '\n');
      if (p) *p = '\0';
      p = strchr(line, ' ');
      if (p) *p = '\0';
      if (line[0] == '\0')
        continue;
      snprintf(close_cmd, sizeof(close_cmd), "%s close '%s' 2>/dev/null",
          cryptsetup_bin, line);
      if (system(close_cmd)) {}
    }
    pclose(fp);
  }

  {
    FILE *ls_fp;
    ls_fp = popen("ls /tmp/recovery_qt_loop_* 2>/dev/null", "r");
    if (ls_fp)
    {
      char marker_path[256];
      while (fgets(marker_path, sizeof(marker_path), ls_fp))
      {
        char *p;
        char loop_path[256];
        FILE *mf;
        p = strchr(marker_path, '\n');
        if (p) *p = '\0';
        if (marker_path[0] == '\0')
          continue;
        mf = fopen(marker_path, "r");
        if (mf)
        {
          if (fgets(loop_path, sizeof(loop_path), mf))
          {
            p = strchr(loop_path, '\n');
            if (p) *p = '\0';
            if (loop_path[0])
            {
              char detach_cmd[512];
              snprintf(detach_cmd, sizeof(detach_cmd),
                  "losetup -d '%s' 2>/dev/null", loop_path);
          if (system(detach_cmd)) {}
            }
          }
          fclose(mf);
          unlink(marker_path);
        }
      }
      pclose(ls_fp);
    }
  }
}
