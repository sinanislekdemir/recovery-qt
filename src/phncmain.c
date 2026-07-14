/*

    File: phncmain.c

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
#if defined(__CYGWIN__) || defined(__MINGW32__) || defined(DJGPP) || !defined(HAVE_GETEUID)
#undef SUDO_BIN
#endif
#if defined(DISABLED_FOR_FRAMAC)
#undef HAVE_NCURSES
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include "types.h"
#include "common.h"
#include "intrf.h"
#include "fnctdsk.h"
#include "intrfn.h"
#include "hdaccess.h"
#include "hdcache.h"
#include "log.h"
#include "nodisk.h"
#ifdef SUDO_BIN
#include "sudo.h"
#endif
#include "hidden.h"
#include "hiddenn.h"
#include "autoset.h"
#include "partauto.h"
#include "misc.h"
#include "photorec_nc.h"
#include "filegen.h"
#include "luksnc.h"
#include "luks.h"
#include "pbackup.h"
#include "askloc.h"

extern const arch_fnct_t arch_none;

#if defined(HAVE_NCURSES)
static partition_t *new_whole_disk(const disk_t *disk_car)
{
  partition_t *p = partition_new(disk_car->arch);
  p->part_offset = 0;
  p->part_size = disk_car->disk_size;
  strncpy(p->fsname, "Whole disk", sizeof(p->fsname) - 1);
  return p;
}

extern file_enable_t array_file_enable[];

#define FMT_PRIORITY_LOW 999

static const char *fmt_priority[] = {
  "jpg", "png", "gif", "bmp", "tif", "webp", "ico", "svg",
  "mov", "mp4", "avi", "mpg", "mkv", "3gp", "flv", "wmv",
  "mp3", "wav", "flac", "ogg", "wma", "m4a", "aac",
  "pdf", "doc", "xls", "ppt", "docx", "xlsx", "pptx",
  "zip", "rar", "7z", "gz", "tar", "bz2", "xz",
  "txt", "rtf", "html", "xml", "json", "csv", "log",
  "exe", "dll", "elf", "so",
  "psd", "ai", "skp", "dwg",
  "db", "mdb", "sqlite",
  NULL
};

static int fmt_get_priority(const char *ext)
{
  int i;
  if (ext == NULL)
    return FMT_PRIORITY_LOW;
  for (i = 0; fmt_priority[i] != NULL; i++)
  {
    if (strcmp(ext, fmt_priority[i]) == 0)
      return i;
  }
  return FMT_PRIORITY_LOW;
}

typedef struct {
  file_enable_t *fe;
  int priority;
  int orig_enable;
} sel_entry_t;

static int sel_entry_cmp(const void *a, const void *b)
{
  const sel_entry_t *fa;
  const sel_entry_t *fb;
  fa = (const sel_entry_t *)a;
  fb = (const sel_entry_t *)b;
  if (fa->priority != fb->priority)
    return fa->priority - fb->priority;
  if (fa->fe->file_hint->description == NULL)
    return -1;
  if (fb->fe->file_hint->description == NULL)
    return 1;
  return strcmp(fa->fe->file_hint->description,
                fb->fe->file_hint->description);
}

static const char *carver_format_selector(void)
{
  sel_entry_t *entries;
  unsigned int count;
  unsigned int cursor;
  unsigned int scroll;
  unsigned int visible_rows;
  unsigned int selected_count;
  unsigned int i;
  unsigned int v;
  int running;
  static char filter[8192];
  file_enable_t *fe;
  WINDOW *win;
  int win_h;
  int win_w;
  int start_y;
  int start_x;
  int title_line;
  int list_start;
  int status_line;
  int hint_line;
  int col_ext;
  int col_desc;
  int avail_w;

  count = 0;
  for (fe = array_file_enable; fe->file_hint != NULL; fe++)
    count++;

  entries = (sel_entry_t *)MALLOC(count * sizeof(sel_entry_t));
  if (entries == NULL)
    return "*";

  i = 0;
  for (fe = array_file_enable; fe->file_hint != NULL; fe++)
  {
    entries[i].fe = fe;
    entries[i].priority = fmt_get_priority(fe->file_hint->extension);
    entries[i].orig_enable = (int)fe->enable;
    i++;
  }
  qsort(entries, count, sizeof(sel_entry_t), sel_entry_cmp);

  selected_count = 0;
  for (i = 0; i < count; i++)
  {
    entries[i].fe->enable = entries[i].fe->file_hint->enable_by_default
        ? 1 : 0;
    if (entries[i].fe->enable)
      selected_count++;
  }

  win_h = LINES - 3;
  if (win_h < 10)
    win_h = 10;
  win_w = COLS - 2;
  if (win_w < 70)
    win_w = 70;
  start_y = 1;
  start_x = 1;

  title_line = 0;
  list_start = 2;
  status_line = win_h - 3;
  hint_line = win_h - 2;

  avail_w = win_w - 6;
  if (avail_w < 50)
    avail_w = 50;
  col_ext = avail_w / 6;
  col_desc = avail_w - col_ext - 12;

  cursor = 0;
  scroll = 0;
  visible_rows = (unsigned int)win_h - 5;
  running = 1;

  win = newwin(win_h, win_w, start_y, start_x);
  if (win == NULL)
  {
    free(entries);
    return "*";
  }
  keypad(win, TRUE);

  while (running)
  {
    unsigned int first;
    unsigned int last;
    int r;

    first = scroll;
    last = scroll + visible_rows;
    if (last > count)
      last = count;

    werase(win);
    wborder(win, '|', '|', '-', '-', '+', '+', '+', '+');

    if (has_colors())
      wbkgdset(win, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvwprintw(win, title_line, 2,
        " Select File Formats (well-known first) ");
    if (has_colors())
      wbkgdset(win, ' ' | COLOR_PAIR(CP_NORMAL));

    mvwprintw(win, title_line + 1, 2, "   %-*s  %-*s Max Size",
        col_ext, "Ext", col_desc, "Description");

    for (v = first; v < last; v++)
    {
      const file_hint_t *hint;
      char size_buf[16];
      char line_buf[256];
      int attr;
      uint64_t ms;
      int line_len;

      hint = entries[v].fe->file_hint;
      r = list_start + (int)(v - first);
      ms = hint->max_filesize;

      if (ms >= 1024ULL * 1024 * 1024)
        snprintf(size_buf, sizeof(size_buf), "%llu GB",
            (unsigned long long)(ms / (1024ULL * 1024 * 1024)));
      else if (ms >= 1024 * 1024)
        snprintf(size_buf, sizeof(size_buf), "%llu MB",
            (unsigned long long)(ms / (1024 * 1024)));
      else if (ms >= 1024)
        snprintf(size_buf, sizeof(size_buf), "%llu KB",
            (unsigned long long)(ms / 1024));
      else
        snprintf(size_buf, sizeof(size_buf), "%llu B",
            (unsigned long long)ms);

      snprintf(line_buf, sizeof(line_buf), " [%c] %-*s  %-*.*s %s",
          entries[v].fe->enable ? 'x' : ' ',
          col_ext,
          hint->extension ? hint->extension : "?",
          col_desc, col_desc,
          hint->description ? hint->description : "",
          size_buf);
      line_len = (int)strlen(line_buf);
      if (line_len > avail_w + 4)
        line_len = avail_w + 4;

      attr = CP_NORMAL;
      if (entries[v].fe->enable)
        attr = CP_MARKED;
      if (v == cursor)
        attr = CP_SELECTED;

      if (has_colors())
        wbkgdset(win, ' ' | COLOR_PAIR(attr));
      mvwaddnstr(win, r, 2, line_buf, win_w - 4);
      if (has_colors())
        wbkgdset(win, ' ' | COLOR_PAIR(CP_NORMAL));
    }

    {
      char status[96];
      snprintf(status, sizeof(status), " %u/%u formats enabled",
          selected_count, count);
      if (has_colors())
        wbkgdset(win, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
      mvwaddnstr(win, status_line, 2, status, win_w - 4);
      if (has_colors())
        wbkgdset(win, ' ' | COLOR_PAIR(CP_NORMAL));

      mvwaddnstr(win, hint_line, 2,
          " SPACE:Toggle  *:All  /:None  ^A:Audio  ^P:Photo  ^V:Video  Enter:OK  Esc:Back",
          win_w - 4);
    }

    wnoutrefresh(win);
    doupdate();

    {
      int ch;
      ch = wgetch(win);

      switch (ch)
      {
        case KEY_UP:
        case 'k':
          if (cursor > 0)
            cursor--;
          break;

        case KEY_DOWN:
        case 'j':
          if (cursor + 1 < count)
            cursor++;
          break;

        case KEY_PPAGE:
          if (cursor > visible_rows)
            cursor -= visible_rows;
          else
            cursor = 0;
          break;

        case KEY_NPAGE:
          if (cursor + visible_rows < count)
            cursor += visible_rows;
          else
            cursor = count - 1;
          break;

        case KEY_HOME:
          cursor = 0;
          break;

        case KEY_END:
          if (count > 0)
            cursor = count - 1;
          break;

        case ' ':
          if (entries[cursor].fe->enable)
          {
            entries[cursor].fe->enable = 0;
            selected_count--;
          }
          else
          {
            entries[cursor].fe->enable = 1;
            selected_count++;
          }
          break;

        case '*':
          selected_count = 0;
          for (i = 0; i < count; i++)
          {
            entries[i].fe->enable = 1;
            selected_count++;
          }
          break;

        case '/':
          for (i = 0; i < count; i++)
            entries[i].fe->enable = 0;
          selected_count = 0;
          break;

        case 1:  /* Ctrl-A: Audio */
          selected_count = 0;
          for (i = 0; i < count; i++)
          {
            const char *ext;
            ext = entries[i].fe->file_hint->extension;
            if (ext && (strcmp(ext, "mp3") == 0 ||
                strcmp(ext, "wav") == 0 ||
                strcmp(ext, "flac") == 0 ||
                strcmp(ext, "ogg") == 0 ||
                strcmp(ext, "wma") == 0 ||
                strcmp(ext, "m4a") == 0 ||
                strcmp(ext, "aac") == 0))
            {
              entries[i].fe->enable = 1;
              selected_count++;
            }
            else
              entries[i].fe->enable = 0;
          }
          break;

        case 16: /* Ctrl-P: Photo */
          selected_count = 0;
          for (i = 0; i < count; i++)
          {
            const char *ext;
            ext = entries[i].fe->file_hint->extension;
            if (ext && (strcmp(ext, "jpg") == 0 ||
                strcmp(ext, "tif") == 0 ||
                strcmp(ext, "png") == 0 ||
                strcmp(ext, "gif") == 0 ||
                strcmp(ext, "bmp") == 0 ||
                strcmp(ext, "webp") == 0 ||
                strcmp(ext, "svg") == 0))
            {
              entries[i].fe->enable = 1;
              selected_count++;
            }
            else
              entries[i].fe->enable = 0;
          }
          break;

        case 22: /* Ctrl-V: Video */
          selected_count = 0;
          for (i = 0; i < count; i++)
          {
            const char *ext;
            ext = entries[i].fe->file_hint->extension;
            if (ext && (strcmp(ext, "mov") == 0 ||
                strcmp(ext, "mp4") == 0 ||
                strcmp(ext, "mpg") == 0 ||
                strcmp(ext, "avi") == 0 ||
                strcmp(ext, "mkv") == 0 ||
                strcmp(ext, "3gp") == 0 ||
                strcmp(ext, "flv") == 0 ||
                strcmp(ext, "wmv") == 0))
            {
              entries[i].fe->enable = 1;
              selected_count++;
            }
            else
              entries[i].fe->enable = 0;
          }
          break;

        case KEY_ENTER:
        case '\n':
        case '\r':
          if (selected_count > 0)
            running = 0;
          break;

        case 27:
        case 'q':
        case 'Q':
          for (i = 0; i < count; i++)
            entries[i].fe->enable = (unsigned int)entries[i].orig_enable;
          free(entries);
          delwin(win);
          return NULL;

        default:
          break;
      }

      if (cursor < scroll)
        scroll = cursor;
      if (cursor >= scroll + visible_rows)
        scroll = cursor - visible_rows + 1;
    }
  }

  delwin(win);

  {
    unsigned int pos;
    unsigned int checked;
    unsigned int fi;

    checked = 0;
    for (fi = 0; fi < count; fi++)
      if (entries[fi].fe->enable)
        checked++;

    if (checked == count)
    {
      snprintf(filter, sizeof(filter), "*");
    }
    else
    {
      pos = 0;
      filter[0] = '\0';
      for (fi = 0; fi < count && pos < sizeof(filter) - 4; fi++)
      {
        const char *ext;
        if (!entries[fi].fe->enable)
          continue;
        ext = entries[fi].fe->file_hint->extension;
        if (ext == NULL)
          continue;
        if (pos > 0)
        {
          filter[pos++] = ',';
        }
        while (*ext && pos < sizeof(filter) - 2)
        {
          filter[pos++] = *ext;
          ext++;
        }
        filter[pos] = '\0';
      }
    }
  }

  free(entries);
  return filter;
}
#endif

#ifdef HAVE_SIGACTION
static struct sigaction action;
int need_to_stop = 0;

static void sighup_hdlr(int sig)
{
  if (need_to_stop == 1)
  {
    end_ncurses();
    action.sa_handler = SIG_DFL;
    sigaction(sig, &action, NULL);
    kill(0, sig);
    return;
  }
  need_to_stop = 1;
}
#endif

#if defined(HAVE_NCURSES)
#define NR_DISK_MAX (LINES - 6 - 8)
#define INTER_DISK_X 0
#define INTER_DISK_Y (8 + NR_DISK_MAX)
#define INTER_NOTE_Y (LINES - 4)

static int photorec_nc_disk_selection(const list_disk_t *list_disk,
    disk_t **selected_disk)
{
  int command;
  int real_key;
  unsigned int menu = 0;
  int offset = 0;
  int pos_num = 0;
  const list_disk_t *current_disk = list_disk;
#ifdef SUDO_BIN
  int use_sudo = 0;
#endif
  static const struct MenuItem menuDisk[] = {
    { 'P', "Previous", "" },
    { 'N', "Next", "" },
    { 'O', "Proceed", "" },
#ifdef SUDO_BIN
    { 'S', "Sudo", "Use sudo to restart as root" },
#endif
    { 'Q', "Quit", "" },
    { 0, NULL, NULL }
  };

  if (list_disk == NULL)
  {
    log_critical("No disk found\n");
    return intrf_no_disk_ncurses("photorec-nc");
  }

  while (1)
  {
    const char *menu_options;
    int i;
    const list_disk_t *element_disk;
    int menu_y;
    int rows_per_page;
    aff_copy(stdscr);
    wmove(stdscr, 4, 0);
    if (has_colors())
      wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_DIM));
    wprintw(stdscr, "  photorec_nc is free software, licensed under GPL v2+.");
    if (has_colors())
      wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));

    if (has_colors())
      wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));

    {
      int tbl_width;
      int tbl_x;
      int tbl_y;
      int si;
      int ri;

      tbl_width = COLS - 6;
      tbl_x = (COLS - tbl_width) / 2;
      tbl_y = 7;

      if (has_colors())
        wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
      wmove(stdscr, 5, tbl_x);
      wclrtoeol(stdscr);
      wprintw(stdscr, "Select a disk and choose 'Proceed':");
      if (has_colors())
        wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));

      wmove(stdscr, tbl_y, tbl_x);
      waddch(stdscr, ACS_ULCORNER);
      for (si = 0; si < tbl_width - 2; si++)
        waddch(stdscr, ACS_HLINE);
      waddch(stdscr, ACS_URCORNER);
      tbl_y++;

      {
        int col_dev;
        int col_size;
        int col_flag;
        int col_model;

        col_dev = 1;
        col_size = col_dev + 34;
        col_flag = col_size + 22;
        col_model = col_flag + 6;

        if (has_colors())
          wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
        wmove(stdscr, tbl_y, tbl_x);
        waddch(stdscr, ACS_VLINE);
        wmove(stdscr, tbl_y, tbl_x + col_dev + 1);
        wprintw(stdscr, "%-*s", col_size - col_dev - 1, "Device");
        wmove(stdscr, tbl_y, tbl_x + col_size + 1);
        wprintw(stdscr, "%-*s", col_flag - col_size - 1, "Size");
        wmove(stdscr, tbl_y, tbl_x + col_flag + 1);
        wprintw(stdscr, "%-*s", 7, "Perm");
        wmove(stdscr, tbl_y, tbl_x + col_model + 1);
        wprintw(stdscr, "%-*s", tbl_width - col_model - 3, "Model");
        wmove(stdscr, tbl_y, tbl_x + tbl_width - 1);
        waddch(stdscr, ACS_VLINE);
        if (has_colors())
          wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));
        tbl_y++;
      }

      i = 0;
      for (element_disk = list_disk; element_disk != NULL;
          element_disk = element_disk->next, i++) {}

      {
        int visible_rows;
        int col_dev;
        int col_size;
        int col_flag;
        int col_model;
        col_dev = 1;
        col_size = col_dev + 34;
        col_flag = col_size + 22;
        col_model = col_flag + 6;

        visible_rows = i - offset;
        if (visible_rows > NR_DISK_MAX - 2)
          visible_rows = NR_DISK_MAX - 2;
        if (visible_rows < 1) visible_rows = 1;
        rows_per_page = visible_rows;

      for (ri = 0; ri < visible_rows; ri++)
      {
        int item_idx;
        item_idx = offset + ri;
        wmove(stdscr, tbl_y, tbl_x);
        if (item_idx == pos_num)
        {
          if (has_colors())
            wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_SELECTED));
          waddch(stdscr, ACS_VLINE);
          wprintw(stdscr, " ▶");
        }
        else
        {
          if (has_colors())
            wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));
          waddch(stdscr, ACS_VLINE);
          wprintw(stdscr, "  ");
        }
        {
          int k;
          const list_disk_t *ed;
          const char *desc;
          ed = list_disk;
          for (k = 0; k < item_idx && ed != NULL; k++)
            ed = ed->next;
          if (ed && ed->disk)
          {
            char dev[64];
            char size_str[64];
            char perm[16];
            char model[256];
            const char *p;
            const char *size_start;
            const char *dev_end;
            int len;

            desc = ed->disk->description_short(ed->disk);
            dev[0] = '\0';
            size_str[0] = '\0';
            perm[0] = '\0';
            model[0] = '\0';

            if (desc)
            {
              p = desc;
              while (*p == ' ')
                p++;
              if (strncmp(p, "Disk ", 5) == 0)
                p += 5;

              dev_end = NULL;
              size_start = p;
              while (*size_start)
              {
                if (*size_start == ' ' && *(size_start + 1) == '-' &&
                    *(size_start + 2) == ' ' &&
                    (*(size_start + 3) >= '0' && *(size_start + 3) <= '9'))
                {
                  dev_end = size_start;
                  size_start += 3;
                  break;
                }
                size_start++;
              }
              if (dev_end == NULL)
                dev_end = size_start;

              len = (int)(dev_end - p);
              if (len > 50) len = 50;
              strncpy(dev, p, len);
              dev[len] = '\0';

              if (*size_start >= '0' && *size_start <= '9')
              {
                const char *s;
                s = size_start;
                while (*s && *s != '(')
                  s++;
                while (s > size_start && *(s-1) == ' ')
                  s--;
                len = (int)(s - size_start);
                if (len > 40) len = 40;
                strncpy(size_str, size_start, len);
                size_str[len] = '\0';
                p = s;
              }
              else
              {
                p = size_start;
              }

              if (*p == '(')
              {
                p++;
                {
                  const char *ps;
                  ps = p;
                  while (*ps && *ps != ')')
                    ps++;
                  len = (int)(ps - p);
                  if (len > 10) len = 10;
                  strncpy(perm, p, len);
                  perm[len] = '\0';
                }
                p++;
                while (*p == ')') p++;
              }

              while (*p == ' ')
                p++;
              if (*p == '-')
              {
                p++;
                while (*p == ' ')
                  p++;
              }
              if (*p)
              {
                strncpy(model, p, 200);
                model[200] = '\0';
              }
            }

            wmove(stdscr, tbl_y, tbl_x + col_dev + 1);
            wprintw(stdscr, "%-*.*s", col_size - col_dev - 1,
                col_size - col_dev - 1, dev);
            wmove(stdscr, tbl_y, tbl_x + col_size + 1);
            wprintw(stdscr, "%-*.*s", col_flag - col_size - 1,
                col_flag - col_size - 1, size_str);
            wmove(stdscr, tbl_y, tbl_x + col_flag + 1);
            wprintw(stdscr, "%-*.*s", 7, 7, perm);
            wmove(stdscr, tbl_y, tbl_x + col_model + 1);
            wprintw(stdscr, "%-*.*s", tbl_width - col_model - 3,
                tbl_width - col_model - 3, model);
          }
        }
        wmove(stdscr, tbl_y, tbl_x + tbl_width - 1);
        if (has_colors())
          wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));
        waddch(stdscr, ACS_VLINE);
        tbl_y++;
      }
      }

      wmove(stdscr, tbl_y, tbl_x);
      waddch(stdscr, ACS_LLCORNER);
      for (si = 0; si < tbl_width - 2; si++)
        waddch(stdscr, ACS_HLINE);
      waddch(stdscr, ACS_LRCORNER);
      tbl_y += 2;
      menu_y = tbl_y;
    }

    mvwaddstr(stdscr, menu_y, 0, "Note: ");
#if defined(HAVE_GETEUID) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(DJGPP)
    if (geteuid() != 0)
    {
      if (has_colors())
        wbkgdset(stdscr, ' ' | A_BOLD | COLOR_PAIR(CP_DELETED));
      wprintw(stdscr, "Some disks won't appear unless you're root user.");
      wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));
#ifdef SUDO_BIN
      use_sudo = 1;
#endif
    }
    else
#endif
    if (current_disk != NULL && current_disk->disk->serial_no != NULL)
    {
      if (has_colors())
        wbkgdset(stdscr, ' ' | A_BOLD | COLOR_PAIR(CP_MARKED));
      wprintw(stdscr, "Serial number %s", current_disk->disk->serial_no);
      wbkgdset(stdscr, ' ' | COLOR_PAIR(CP_NORMAL));
    }

#ifdef SUDO_BIN
    if (use_sudo > 0)
      menu_options = i <= rows_per_page ? "OSQ" : "PNOSQ";
    else
#endif
      menu_options = i <= rows_per_page ? "OQ" : "PNOQ";

    command = wmenuSelect_ext(stdscr, menu_y + 1, menu_y + 3,
        0, menuDisk, 8, menu_options,
        MENU_HORIZ | MENU_BUTTON | MENU_ACCEPT_OTHERS, &menu, &real_key);

    switch (command)
    {
      case KEY_UP:
      case 'P':
        if (current_disk->prev != NULL)
        { current_disk = current_disk->prev; pos_num--; }
        break;
      case KEY_DOWN:
      case 'N':
        if (current_disk->next != NULL)
        { current_disk = current_disk->next; pos_num++; }
        break;
      case KEY_PPAGE:
        for (i = 0; i < NR_DISK_MAX - 1 && current_disk->prev != NULL; i++)
        { current_disk = current_disk->prev; pos_num--; }
        break;
      case KEY_NPAGE:
        for (i = 0; i < NR_DISK_MAX - 1 && current_disk->next != NULL; i++)
        { current_disk = current_disk->next; pos_num++; }
        break;
      case 'O':
        {
          disk_t *disk = current_disk->disk;
          int hpa_dco = is_hpa_or_dco(disk);
          autodetect_arch(disk, &arch_none);
          autoset_unit(disk);
          if ((hpa_dco == 0 ||
              interface_check_hidden_ncurses(disk, hpa_dco) == 0))
          {
            *selected_disk = disk;
            return 0;
          }
        }
        break;
#ifdef SUDO_BIN
      case 'S':
        return 1;
#endif
      case 'Q':
        return -1;
    }
    if (pos_num < offset) offset = pos_num;
    if (pos_num >= offset + rows_per_page) offset = pos_num - rows_per_page + 1;
  }
}
#endif

#define NR_PART_MAX (LINES - 6 - 8)

#if defined(HAVE_NCURSES)
static int photorec_nc_partition_selection(disk_t *disk, partition_t **selected_part)
{
  int insert_error = 0;
  list_part_t *list_part;
  list_part_t *element_part;
  list_part_t *current_part;
  int command;
  int real_key;
  unsigned int menu = 0;
  int offset = 0;
  int pos_num = 0;
  static const struct MenuItem menuPart[] = {
    { 'P', "Previous", "" },
    { 'N', "Next", "" },
    { 'S', "Scan", "Scan for deleted files" },
    { '1', "Carve", "Carve for file signatures" },
    { '2', "Archeology", "Deep forensic carve (slow)" },
    { 'B', "Backup",  "Create .dsk backup of live filesystem" },
    { 'R', "Restore", "Restore from .dsk backup file" },
    { 'Q', "Quit", "" },
    { 0, NULL, NULL }
  };

  aff_copy(stdscr);
  wmove(stdscr, 5, 0);
  wprintw(stdscr, "Reading partitions, please wait...");
  wrefresh(stdscr);

  list_part = disk->arch->read_part(disk, 0, 0);
  {
    partition_t *wd = partition_new(disk->arch);
    wd->part_offset = 0;
    wd->part_size = disk->disk_size;
    strncpy(wd->fsname, "Whole disk", sizeof(wd->fsname) - 1);
    list_part = insert_new_partition(list_part, wd, 0, &insert_error);
    if (insert_error > 0)
      free(wd);
  }

  if (list_part == NULL)
  {
    display_message("No partition found.");
    return -1;
  }

  for (element_part = list_part; element_part != NULL;
      element_part = element_part->next)
  {
    unsigned char head[8];
    if (check_LUKS(disk, element_part->part) == 0 ||
        (disk->pread(disk, head, sizeof(head),
         element_part->part->part_offset) == (int)sizeof(head) &&
         memcmp(head, "LUKS\xba\xbe", 6) == 0))
    {
      strncpy(element_part->part->fsname, "LUKS encrypted",
          sizeof(element_part->part->fsname) - 1);
    }
  }

  {
    const uint64_t alignment_offsets[] = {2048, 4096, 63};
    unsigned int k;
    for (k = 0; k < sizeof(alignment_offsets) / sizeof(alignment_offsets[0]); k++)
    {
      unsigned char head[8];
      uint64_t align_offs = alignment_offsets[k] * 512;
      if (align_offs >= disk->disk_size)
        continue;
      if (disk->pread(disk, head, sizeof(head), align_offs) ==
          (int)sizeof(head) && memcmp(head, "LUKS\xba\xbe", 6) == 0)
      {
        partition_t *luks_part;
        int insert_error2 = 0;
        luks_part = partition_new(disk->arch);
        luks_part->part_offset = align_offs;
        luks_part->part_size = disk->disk_size - align_offs;
        strncpy(luks_part->fsname, "LUKS encrypted",
            sizeof(luks_part->fsname) - 1);
        list_part = insert_new_partition(list_part, luks_part,
            0, &insert_error2);
        if (insert_error2 > 0)
          free(luks_part);
        break;
      }
    }
  }

  current_part = list_part;
  while (1)
  {
    int i;
    const char *menu_options;
    aff_copy(stdscr);
    wmove(stdscr, 3, 0);
    wprintw(stdscr, "Disk: %s", disk->description_short(disk));
    wmove(stdscr, 5, 0);
    wprintw(stdscr, "Select a partition and choose 'Scan':");

    for (i = 0, element_part = list_part;
        element_part != NULL && i < offset + NR_PART_MAX;
        i++, element_part = element_part->next)
    {
      if (i < offset) continue;
      wmove(stdscr, 7 + i - offset, 0);
      if (element_part != current_part)
      {
        aff_part(stdscr, AFF_PART_ORDER | AFF_PART_STATUS,
            disk, element_part->part);
      }
      else
      {
        wattrset(stdscr, A_REVERSE);
        aff_part(stdscr, AFF_PART_ORDER | AFF_PART_STATUS,
            disk, element_part->part);
        wattroff(stdscr, A_REVERSE);
      }
    }

    menu_options = i <= NR_PART_MAX ? "PS12BRQ" : "PNS12BRQ";
    command = wmenuSelect_ext(stdscr, 6 + NR_PART_MAX + 2,
        7 + NR_PART_MAX + 2, 0, menuPart, 7, menu_options,
        MENU_HORIZ | MENU_BUTTON | MENU_ACCEPT_OTHERS, &menu, &real_key);

    switch (command)
    {
      case KEY_UP:
      case 'P':
        if (current_part->prev != NULL)
        { current_part = current_part->prev; pos_num--; }
        break;
      case KEY_DOWN:
      case 'N':
        if (current_part->next != NULL)
        { current_part = current_part->next; pos_num++; }
        break;
      case KEY_PPAGE:
        for (i = 0; i < NR_PART_MAX - 1 && current_part->prev != NULL; i++)
        { current_part = current_part->prev; pos_num--; }
        break;
      case KEY_NPAGE:
        for (i = 0; i < NR_PART_MAX - 1 && current_part->next != NULL; i++)
        { current_part = current_part->next; pos_num++; }
        break;
      case 'S':
        *selected_part = current_part->part;
        part_free_list(list_part);
        return 0;
      case '1':
        strncpy(current_part->part->fsname, "Carve",
            sizeof(current_part->part->fsname) - 1);
        *selected_part = current_part->part;
        part_free_list_only(list_part);
        return 0;
      case '2':
        strncpy(current_part->part->fsname, "Arch",
            sizeof(current_part->part->fsname) - 1);
        *selected_part = current_part->part;
        part_free_list_only(list_part);
        return 0;
      case 'B':
        strncpy(current_part->part->fsname, "Backup",
            sizeof(current_part->part->fsname) - 1);
        *selected_part = current_part->part;
        part_free_list_only(list_part);
        return 0;
      case 'R':
        strncpy(current_part->part->fsname, "Restore",
            sizeof(current_part->part->fsname) - 1);
        *selected_part = current_part->part;
        part_free_list_only(list_part);
        return 0;
      case 'Q':
        part_free_list(list_part);
        return -1;
    }
    if (pos_num < offset) offset = pos_num;
    if (pos_num >= offset + NR_PART_MAX) offset = pos_num - NR_PART_MAX + 1;
  }
}
#endif

int main(int argc, char **argv)
{
  list_disk_t *list_disk = NULL;
  int i;
  int create_log = TD_LOG_APPEND;
  int testdisk_mode = TESTDISK_O_RDONLY | TESTDISK_O_READAHEAD_32K;
  const char *logfile = "photorec-nc.log";
  int log_opened = 0;
  int log_errno = 0;
  list_disk_t *element_disk;
  int run_setlocale = 1;

  srand(time(NULL) & (long)0xffffffff);

#ifdef HAVE_SIGACTION
  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGINT);
  sigaddset(&action.sa_mask, SIGHUP);
  sigaddset(&action.sa_mask, SIGTERM);
  action.sa_handler = &sighup_hdlr;
  action.sa_flags = 0;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGHUP, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
#endif

  printf("photorec-nc %s, Deleted File Browser and Recovery, %s\n",
      VERSION, TESTDISKDATE);
  printf("https://www.cgsecurity.org\n");

  for (i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "/log") == 0 || strcmp(argv[i], "-log") == 0)
    {
      if (create_log == TD_LOG_NONE)
        create_log = TD_LOG_APPEND;
    }
    else if (strcmp(argv[i], "/debug") == 0 || strcmp(argv[i], "-debug") == 0)
    {
      if (create_log == TD_LOG_NONE)
        create_log = TD_LOG_APPEND;
    }
    else if (strcmp(argv[i], "/all") == 0 || strcmp(argv[i], "-all") == 0)
    {
      testdisk_mode |= TESTDISK_O_ALL;
    }
    else if (strcmp(argv[i], "/direct") == 0 || strcmp(argv[i], "-direct") == 0)
    {
      testdisk_mode |= TESTDISK_O_DIRECT;
    }
    else if (strcmp(argv[i], "/nosetlocale") == 0 ||
        strcmp(argv[i], "-nosetlocale") == 0)
    {
      run_setlocale = 0;
    }
    else
    {
      disk_t *disk_car = file_test_availability(argv[i], 0, testdisk_mode);
      if (disk_car == NULL)
      {
        printf("Unable to open file or device %s\n", argv[i]);
        return 1;
      }
      list_disk = insert_new_disk(list_disk, disk_car);
    }
  }

  if (create_log != TD_LOG_NONE)
    log_opened = log_open(logfile, create_log, &log_errno);

#ifdef HAVE_SETLOCALE
  if (run_setlocale > 0)
    setlocale(LC_ALL, "");
#endif

  if (create_log != TD_LOG_NONE && log_opened == 0)
    log_opened = log_open_default(logfile, create_log, &log_errno);

#ifdef HAVE_NCURSES
  if (start_ncurses("photorec-nc", argv[0]))
  {
    log_close();
    return 1;
  }

  luks_cleanup_orphans();

  {
    const char *filename = logfile;
    while (create_log != TD_LOG_NONE && log_opened == 0)
    {
      filename = ask_log_location(filename, log_errno);
      if (filename != NULL)
        log_opened = log_open(filename, create_log, &log_errno);
      else
        create_log = TD_LOG_NONE;
    }
  }
  aff_copy(stdscr);
  wmove(stdscr, 5, 0);
  wprintw(stdscr, "Disk identification, please wait...\n");
  wrefresh(stdscr);
#endif

  {
    time_t my_time;
    my_time = time(NULL);
    log_info("\n\n%s", ctime(&my_time));
  }
  log_info("photorec-nc %s, Deleted File Browser and Recovery, %s\n",
      VERSION, TESTDISKDATE);
  log_info("OS: %s\n", get_os());
  log_info("Compiler: %s\n", get_compiler());
  log_flush();

  if (list_disk == NULL)
    list_disk = hd_parse(list_disk, 0, testdisk_mode);
  hd_update_all_geometry(list_disk, 0);

  for (element_disk = list_disk; element_disk != NULL;
      element_disk = element_disk->next)
  {
    element_disk->disk = new_diskcache(element_disk->disk, testdisk_mode);
  }
  log_disk_list(list_disk);

#ifdef HAVE_NCURSES
  {
    disk_t *selected_disk = NULL;
    partition_t *selected_part = NULL;
    int skip_disk_selection = 0;
    char luks_mapper_name[256] = "";
    disk_t *luks_decrypted_disk = NULL;

    if (list_disk != NULL && list_disk->next == NULL)
    {
      selected_disk = list_disk->disk;
      skip_disk_selection = 1;
    }

    if (!skip_disk_selection)
    {
      if (photorec_nc_disk_selection(list_disk, &selected_disk) != 0 ||
          selected_disk == NULL)
      {
        end_ncurses();
        log_close();
        delete_list_disk(list_disk);
        return 0;
      }
    }

    if (photorec_nc_partition_selection(selected_disk, &selected_part) == 0 &&
        selected_part != NULL)
    {
      disk_t *scan_disk;
      partition_t *scan_part;
      int is_carve;
      int is_arch;

      scan_disk = selected_disk;
      scan_part = selected_part;

      is_carve  = (strncmp(scan_part->fsname, "Carve",  5) == 0);
      is_arch   = (strncmp(scan_part->fsname, "Arch",   4) == 0);

      {
        unsigned char head[8];

        if (scan_disk->pread(scan_disk, head, sizeof(head),
            scan_part->part_offset) == (int)sizeof(head) &&
            memcmp(head, "LUKS\xba\xbe", 6) == 0)
        {
          int do_decrypt;

          do_decrypt = 1;
          if (is_carve || is_arch)
          {
            int key;
            aff_copy(stdscr);
            wmove(stdscr, LINES / 2, (COLS - 50) / 2);
            wprintw(stdscr,
                "Decrypt LUKS volume before carving? (y/N)");
            wrefresh(stdscr);
            nodelay(stdscr, FALSE);
            key = wgetch(stdscr);
            if (key != 'y' && key != 'Y')
              do_decrypt = 0;
          }

          if (do_decrypt)
          {
            char *passphrase;
            passphrase = ask_luks_passphrase();
            if (passphrase && passphrase[0])
            {
              aff_copy(stdscr);
              wmove(stdscr, 7, 0);
              wclrtoeol(stdscr);
              wprintw(stdscr, "  Opening LUKS device, please wait...");
              wrefresh(stdscr);

              if (luks_open(scan_disk->device, scan_part->part_offset,
                  passphrase, luks_mapper_name,
                  sizeof(luks_mapper_name)) == 0)
              {
                disk_t *dec_disk;
                dec_disk = file_test_availability(luks_mapper_name, 0,
                    TESTDISK_O_RDONLY);
                if (dec_disk)
                {
                  autodetect_arch(dec_disk, &arch_none);
                  autoset_unit(dec_disk);
                  dec_disk = new_diskcache(dec_disk,
                      TESTDISK_O_RDONLY | TESTDISK_O_READAHEAD_32K);
                  scan_disk = dec_disk;
                  scan_part = new_whole_disk(scan_disk);
                  luks_decrypted_disk = dec_disk;
                }
                else
                {
                  luks_close(luks_mapper_name);
                  luks_mapper_name[0] = '\0';
                }
              }
              else
              {
                display_message("Failed to unlock LUKS device.");
                luks_mapper_name[0] = '\0';
              }
            }
          }
        }
      }

      {
        scan_tree_t *tree;

        tree = tree_new();

        if (strncmp(scan_part->fsname, "Backup", 6) == 0)
        {
          char dest[512];
          dest[0] = '\0';
          ask_location(dest, sizeof(dest),
              "Choose destination directory for backup", "/", 0);
          if (dest[0])
          {
            int bk_res;
            aff_copy(stdscr);
            wmove(stdscr, 5, 0);
            wprintw(stdscr, "Creating backup. Please wait...");
            wrefresh(stdscr);
            bk_res = backup_create(scan_disk, scan_part, dest);
            if (bk_res == 0)
              display_message("Backup created successfully.");
            else
              display_message("Backup failed. Check log for details.");
          }
          goto done;
        }
        if (strncmp(scan_part->fsname, "Restore", 7) == 0)
        {
          char path[512];
          int rst_res;
          FILE *test_f;
          path[0] = '\0';
          ask_location(path, sizeof(path),
              "Choose .dsk backup file to restore", "/", 1);
          if (!path[0])
            goto done;
          test_f = fopen(path, "rb");
          if (test_f == NULL)
          {
            char msg[512];
            snprintf(msg, sizeof(msg),
                "Cannot open: %.400s", path);
            display_message(msg);
            goto done;
          }
          fclose(test_f);
          rst_res = backup_restore(tree, scan_disk, scan_part, path);
          if (rst_res == 0)
          {
            uint64_t del_count;
            uint64_t mod_count;
            uint64_t chg_size;
            char size_buf[32];
            tree_count_changes(tree->root, &del_count,
                &mod_count, &chg_size);
            tree_format_size(chg_size, size_buf, sizeof(size_buf));
            snprintf(g_browser_info, sizeof(g_browser_info),
                "Backup restore: %llu deleted, %llu modified (%s)",
                (unsigned long long)del_count,
                (unsigned long long)mod_count, size_buf);
            browser_run(tree, scan_disk, scan_part);
            g_browser_info[0] = '\0';
          }
          else
          {
            display_message("Restore failed. Check photorec-nc.log.");
          }
          goto done;
        }

        if (is_carve)
        {
          const char *exts;

          exts = carver_format_selector();
          if (exts == NULL)
            exts = "*";

          aff_copy(stdscr);
          wmove(stdscr, 5, 0);
          wprintw(stdscr, "Carving for file signatures. Please wait...");
          wrefresh(stdscr);

          carver_run(tree, scan_disk, scan_part, exts, 0);

          scan_part->upart_type = UP_UNK;

          aff_copy(stdscr);
          wmove(stdscr, 7, 0);
          wprintw(stdscr,
              "  Carve complete. Check photorec-nc.log for stats.");
          wmove(stdscr, 9, 0);
          wprintw(stdscr,
              "  Press any key to browse recovered files...");
          wrefresh(stdscr);
          wgetch(stdscr);

          browser_run(tree, scan_disk, scan_part);
        }
        else if (is_arch)
        {
          const char *exts;
          int key;

          aff_copy(stdscr);
          wmove(stdscr, LINES / 2, (COLS - 55) / 2);
          wprintw(stdscr,
              "Archeology: byte-level deep carve. "
              "Takes ~512x longer. Continue? (y/N)");
          wrefresh(stdscr);
          nodelay(stdscr, FALSE);
          key = wgetch(stdscr);
          if (key != 'y' && key != 'Y')
          {
            goto done;
          }

          exts = carver_format_selector();
          if (exts == NULL)
            goto done;

          aff_copy(stdscr);
          wmove(stdscr, 5, 0);
          wprintw(stdscr,
              "Deep-carving for file signatures (byte-level).");
          wmove(stdscr, 6, 0);
          wprintw(stdscr,
              "This may take a very long time. Please wait...");
          wrefresh(stdscr);

          carver_run(tree, scan_disk, scan_part, exts, 1);

          scan_part->upart_type = UP_UNK;

          aff_copy(stdscr);
          wmove(stdscr, 7, 0);
          wprintw(stdscr,
              "  Archeology complete. Check photorec-nc.log for stats.");
          wmove(stdscr, 9, 0);
          wprintw(stdscr,
              "  Press any key to browse recovered files...");
          wrefresh(stdscr);
          wgetch(stdscr);

          browser_run(tree, scan_disk, scan_part);
        }
        else
        {

        aff_copy(stdscr);
        wmove(stdscr, 5, 0);
        wprintw(stdscr, "Scanning deleted files. Please wait...");
        wrefresh(stdscr);

        if (scanner_run(tree, scan_disk, scan_part) != 0)
        {
          uint64_t luks_offsets[] = {
              scan_part->part_offset,
              2048ULL * 512,
              4096ULL * 512,
              63ULL * 512
          };
          unsigned int k;
          uint64_t luks_offset = (uint64_t)-1;

          log_info("scanner failed, checking for LUKS at %d offsets\n",
              (int)(sizeof(luks_offsets)/sizeof(luks_offsets[0])));

          for (k = 0; k < sizeof(luks_offsets) / sizeof(luks_offsets[0]); k++)
          {
            unsigned char head[8];
            if (luks_offsets[k] >= scan_disk->disk_size)
              continue;
            log_info("  checking offset %llu\n", (unsigned long long)luks_offsets[k]);
            if (scan_disk->pread(scan_disk, head, sizeof(head),
                luks_offsets[k]) == (int)sizeof(head) &&
                memcmp(head, "LUKS\xba\xbe", 6) == 0)
            {
              luks_offset = luks_offsets[k];
              log_info("  LUKS found at offset %llu\n", (unsigned long long)luks_offset);
              break;
            }
          }

          if (luks_offset != (uint64_t)-1)
          {
            char *passphrase;
            log_info("LUKS found, prompting for passphrase\n");
            passphrase = ask_luks_passphrase();
            if (passphrase && passphrase[0])
            {
              aff_copy(stdscr);
              wmove(stdscr, 7, 0);
              wclrtoeol(stdscr);
              wprintw(stdscr, "  Opening LUKS device, please wait...");
              wrefresh(stdscr);

              if (luks_open(scan_disk->device, luks_offset,
                  passphrase, luks_mapper_name,
                  sizeof(luks_mapper_name)) == 0)
              {
                scan_disk = file_test_availability(luks_mapper_name, 0,
                    TESTDISK_O_RDONLY);
                if (scan_disk)
                {
                  autodetect_arch(scan_disk, &arch_none);
                  autoset_unit(scan_disk);
                  scan_disk = new_diskcache(scan_disk,
                      TESTDISK_O_RDONLY | TESTDISK_O_READAHEAD_32K);
                  scan_part = new_whole_disk(scan_disk);
                  luks_decrypted_disk = scan_disk;

                  if (scanner_run(tree, scan_disk, scan_part) == 0)
                  {
                     browser_run(tree, scan_disk, scan_part);
                     goto done;
                  }
                }
                else
                {
                  luks_close(luks_mapper_name);
                  luks_mapper_name[0] = '\0';
                }
              }
              else
              {
                display_message("Failed to unlock LUKS device.");
                luks_mapper_name[0] = '\0';
              }
            }
          }
          display_message(
              "This filesystem is not supported or could not be read.");
        }
        else
        {
          browser_run(tree, scan_disk, scan_part);
        }
        }

done:
        tree_free(tree);
      }

      if (luks_decrypted_disk)
      {
        luks_decrypted_disk->clean(luks_decrypted_disk);
      }
      if (luks_mapper_name[0])
      {
        luks_close(luks_mapper_name);
      }
    }
  }

  end_ncurses();
#endif

  log_info("photorec-nc exited normally.\n");
  log_close();
  delete_list_disk(list_disk);
  return 0;
}
