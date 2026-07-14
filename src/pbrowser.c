/*

    File: pbrowser.c

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
#ifdef HAVE_NCURSES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "types.h"
#include "common.h"
#include "intrf.h"
#include "intrfn.h"
#include "dir.h"
#include "askloc.h"
#include "log.h"
#include "hdaccess.h"
#include "photorec_nc.h"

char g_browser_info[256] = "";

#define MAX_VISIBLE_ITEMS (LINES - 10)

typedef struct {
    file_node_t *node;
    unsigned int depth;
} flat_entry_t;

typedef struct {
    flat_entry_t *entries;
    unsigned int count;
    unsigned int capacity;
    unsigned int scroll;
    unsigned int cursor;
} flat_list_t;

static void flat_list_init(flat_list_t *fl)
{
  fl->entries = NULL;
  fl->count = 0;
  fl->capacity = 0;
  fl->scroll = 0;
  fl->cursor = 0;
}

static void flat_list_add(flat_list_t *fl, file_node_t *node, unsigned int depth)
{
  if (fl->count >= fl->capacity)
  {
    fl->capacity = fl->capacity == 0 ? 4096 : fl->capacity * 2;
    fl->entries = (flat_entry_t *)realloc(fl->entries,
        fl->capacity * sizeof(flat_entry_t));
    if (fl->entries == NULL)
      return;
  }
  fl->entries[fl->count].node = node;
  fl->entries[fl->count].depth = depth;
  fl->count++;
}

static void flatten_tree(file_node_t *node, flat_list_t *fl, unsigned int depth,
    unsigned int show_all)
{
  struct td_list_head *pos;
  td_list_for_each(pos, &node->children)
  {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    if (!show_all && child->type == NODE_FILE && !child->deleted)
      continue;
    flat_list_add(fl, child, depth);
    if (child->type == NODE_DIR && child->expanded)
      flatten_tree(child, fl, depth + 1, show_all);
  }
}

static void toggle_expand(file_node_t *node)
{
  node->expanded = !node->expanded;
}

static void mark_subtree(file_node_t *node, unsigned int mark)
{
  struct td_list_head *pos;
  td_list_for_each(pos, &node->children)
  {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    child->marked = mark;
    if (child->type == NODE_DIR)
      mark_subtree(child, mark);
  }
}

static void invert_dir(file_node_t *node)
{
  struct td_list_head *pos;
  td_list_for_each(pos, &node->children)
  {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    child->marked = !child->marked;
    if (child->type == NODE_DIR)
      invert_dir(child);
  }
}

static void draw_item(WINDOW *window, int ypos, const flat_entry_t *entry,
    int is_cursor)
{
  const file_node_t *node = entry->node;
  unsigned int depth = entry->depth;
  char size_buf[32];
  char date_buf[64];
  int line_attr;

  line_attr = CP_NORMAL;
  if (node->marked)
    line_attr = CP_MARKED;
  else if (node->backup_modified)
    line_attr = CP_WARN;
  else if (node->deleted)
    line_attr = CP_DELETED;

  if (is_cursor)
    line_attr = CP_SELECTED;

  if (has_colors())
    wbkgdset(window, ' ' | COLOR_PAIR(line_attr));

  wmove(window, ypos, 0);
  wclrtoeol(window);

  waddstr(window, node->marked ? "[X] " : "[ ] ");

  {
    unsigned int i;
    for (i = 0; i < depth; i++)
      waddstr(window, "  ");
  }

  if (node->type == NODE_DIR)
  {
    waddstr(window, node->expanded ? "[-] " : "[+] ");
    waddstr(window, node->name);
    waddstr(window, "/");
  }
  else
  {
    waddstr(window, "    ");
    waddstr(window, node->name);
  }

  tree_format_size(node->size, size_buf, sizeof(size_buf));

  if (node->mtime > 0)
  {
    struct tm *tm_p;
#ifdef __MINGW32__
    tm_p = localtime(&node->mtime);
#else
    struct tm tmp;
    tm_p = localtime_r(&node->mtime, &tmp);
#endif
    if (tm_p)
      snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %02d:%02d",
          1900 + tm_p->tm_year, tm_p->tm_mon + 1, tm_p->tm_mday,
          tm_p->tm_hour, tm_p->tm_min);
    else
      snprintf(date_buf, sizeof(date_buf), "                    ");
  }
  else
  {
    snprintf(date_buf, sizeof(date_buf), "                    ");
  }

  {
    int info_col;
    info_col = COLS > 80 ? COLS - 35 : 50;
    if (info_col > 0)
    {
      wmove(window, ypos, info_col);
      wprintw(window, "%8s  %s", size_buf, date_buf);
    }
  }

  if (has_colors())
    wbkgdset(window, ' ' | COLOR_PAIR(CP_NORMAL));
}

static void draw_status(WINDOW *window, const file_node_t *root,
    unsigned int show_all)
{
  uint64_t marked_size = 0;
  uint64_t marked_count = tree_count_marked(root, &marked_size);
  char marked_size_buf[32];
  int status_line;

  tree_format_size(marked_size, marked_size_buf, sizeof(marked_size_buf));

  status_line = LINES - 1;
  wmove(window, status_line, 0);
  if (has_colors())
    wbkgdset(window, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
  wclrtoeol(window);
  wprintw(window, " %llu files  %s  %s",
      (unsigned long long)marked_count, marked_size_buf,
      show_all ? "All files" : "Deleted only");
  waddstr(window, "  |  Space:Mark  F5:Restore  Tab:View  /:Search  *:Invert  F10:Quit");
  if (has_colors())
    wbkgdset(window, ' ' | COLOR_PAIR(CP_NORMAL));
}

static char *get_node_path(const file_node_t *node)
{
  char buf[4096];
  const file_node_t *n;
  char *parts[256];
  int count;
  int i;

  buf[0] = '\0';
  n = node;
  count = 0;

  if (n->type == NODE_DIR && n->name[0] == '/' && n->name[1] == '\0')
    return strdup("/");

  while (n && n->parent)
  {
    parts[count++] = n->name;
    n = n->parent;
  }

  if (count == 0)
  {
    strcpy(buf, "/");
  }
  else
  {
    for (i = count - 1; i >= 0; i--)
    {
      strcat(buf, "/");
      strcat(buf, parts[i]);
    }
  }
  return strdup(buf);
}

int browser_run(scan_tree_t *tree, disk_t *disk, const partition_t *partition)
{
  WINDOW *window;
  flat_list_t fl;
  int running;
  char search_buf[256];
  unsigned int show_all;
  int range_from;

  window = newwin(LINES, COLS, 0, 0);
  running = 1;
  search_buf[0] = '\0';
  show_all = 1;
  range_from = -1;

  flat_list_init(&fl);

  while (running)
  {
    unsigned int i;
    int car;

    fl.count = 0;
    flatten_tree(tree->root, &fl, 0, show_all);

    aff_copy(window);
    if (has_colors())
      wbkgdset(window, ' ' | COLOR_PAIR(CP_HEADER) | A_BOLD);
    wmove(window, 0, 0);
    wclrtoeol(window);
    wprintw(window, " photorec_nc  Disk: %s  Partition: %s",
        disk->description_short(disk), partition->fsname);
    wmove(window, 1, 0);
    wclrtoeol(window);
    {
      const file_node_t *path_node;
      char *path;
      path_node = (fl.cursor < fl.count) ? fl.entries[fl.cursor].node : tree->root;
      path = get_node_path(path_node);
      wprintw(window, " %s", path);
      free(path);
    }
    if (has_colors())
      wbkgdset(window, ' ' | COLOR_PAIR(CP_NORMAL));

    if (g_browser_info[0])
    {
      wmove(window, 2, 0);
      wclrtoeol(window);
      if (has_colors())
        wbkgdset(window, ' ' | COLOR_PAIR(CP_WARN));
      wprintw(window, " %s", g_browser_info);
      if (has_colors())
        wbkgdset(window, ' ' | COLOR_PAIR(CP_NORMAL));
    }

    if (fl.cursor >= fl.count && fl.count > 0)
      fl.cursor = fl.count - 1;

    if (fl.cursor < fl.scroll)
      fl.scroll = fl.cursor;
    if (fl.cursor >= fl.scroll + MAX_VISIBLE_ITEMS)
      fl.scroll = fl.cursor - MAX_VISIBLE_ITEMS + 1;

    for (i = 0; i < (unsigned int)MAX_VISIBLE_ITEMS && fl.scroll + i < fl.count; i++)
    {
      const flat_entry_t *entry;
      int is_cursor;
      entry = &fl.entries[fl.scroll + i];
      is_cursor = (fl.scroll + i == fl.cursor);
      draw_item(window, 3 + i, entry, is_cursor);
    }

    if (search_buf[0] || range_from >= 0)
    {
      wmove(window, LINES - 3, 0);
      wclrtoeol(window);
      if (search_buf[0])
      {
        wprintw(window, " Search: %s", search_buf);
        if (range_from >= 0 && (unsigned int)range_from < fl.count)
        {
          wprintw(window, "  Range from: %s",
              fl.entries[(unsigned int)range_from].node->name);
        }
      }
      else if (range_from >= 0 && (unsigned int)range_from < fl.count)
      {
        wprintw(window, " Range from: %s",
            fl.entries[(unsigned int)range_from].node->name);
      }
    }

    draw_status(window, tree->root, show_all);
    wrefresh(window);

    car = wgetch(window);

    switch (car)
    {
      case KEY_UP:
      case 'k':
        if (fl.cursor > 0)
          fl.cursor--;
        break;
      case KEY_DOWN:
      case 'j':
        if (fl.cursor + 1 < fl.count)
          fl.cursor++;
        break;
      case KEY_PPAGE:
        {
          unsigned int amt;
          amt = MAX_VISIBLE_ITEMS - 1;
          if (amt > fl.cursor)
            fl.cursor = 0;
          else
            fl.cursor -= amt;
        }
        break;
      case KEY_NPAGE:
        {
          unsigned int amt;
          amt = MAX_VISIBLE_ITEMS - 1;
          if (fl.cursor + amt < fl.count)
            fl.cursor += amt;
          else
            fl.cursor = fl.count > 0 ? fl.count - 1 : 0;
        }
        break;
      case KEY_HOME:
        fl.cursor = 0;
        break;
      case KEY_END:
        fl.cursor = fl.count > 0 ? fl.count - 1 : 0;
        break;
      case KEY_RIGHT:
      case KEY_ENTER:
      case '\n':
      case '\r':
        if (fl.cursor < fl.count)
        {
          file_node_t *node = fl.entries[fl.cursor].node;
          if (node->type == NODE_DIR)
            toggle_expand(node);
        }
        break;
      case KEY_LEFT:
      case 'h':
        if (fl.cursor < fl.count)
        {
          file_node_t *node = fl.entries[fl.cursor].node;
          if (node->type == NODE_DIR && node->expanded)
            toggle_expand(node);
          else if (node->parent && node->parent != tree->root)
          {
            node->parent->expanded = 0;
          }
        }
        break;
      case 'i':
        if (fl.cursor < fl.count)
        {
          file_node_t *node = fl.entries[fl.cursor].node;
          char info[1024];
          const char *type_str;
          int off;
          type_str = node->type == NODE_DIR ? "DIR" : "FILE";
          off = snprintf(info, sizeof(info),
              "%s: %s\n"
              "Size: %llu  Sectors: %llu  First: %llu\n"
              "Deleted: %d  Modified: %d  Orphan: %d",
              type_str, node->name,
              (unsigned long long)node->size,
              (unsigned long long)node->num_sectors,
              (unsigned long long)node->first_sector,
              node->deleted, node->backup_modified, node->orphan);
          if (node->cluster_count > 0)
          {
            uint32_t k;
            off += snprintf(info + off, sizeof(info) - off,
                "\nCluster size: %u  Count: %u",
                node->cluster_size, node->cluster_count);
            if (node->cluster_list)
            {
              for (k = 0; k < node->cluster_count && k < 8; k++)
              {
                off += snprintf(info + off, sizeof(info) - off,
                    "\n  cl[%u]=%llu  offset=%llu",
                    k,
                    (unsigned long long)(node->cluster_list[k]),
                    (unsigned long long)(partition->part_offset +
                        node->cluster_list[k]));
              }
              if (node->cluster_count > 8)
                off += snprintf(info + off, sizeof(info) - off,
                    "\n  ... +%u more", node->cluster_count - 8);
            }
          }
          display_message(info);
        }
        break;
        if (fl.cursor < fl.count)
          range_from = (int)fl.cursor;
        break;
      case 't':
        if (range_from < 0 || fl.cursor >= fl.count)
        {
          range_from = -1;
          break;
        }
        {
          const file_node_t *a;
          const file_node_t *b;
          int start;
          int end;
          int k;
          a = fl.entries[(unsigned int)range_from].node;
          b = fl.entries[fl.cursor].node;
          if (a->parent != b->parent)
          {
            range_from = -1;
            break;
          }
          start = range_from < (int)fl.cursor ? range_from : (int)fl.cursor;
          end = range_from > (int)fl.cursor ? range_from : (int)fl.cursor;
          for (k = start; k <= end; k++)
          {
            file_node_t *node = fl.entries[(unsigned int)k].node;
            node->marked = 1;
            if (node->type == NODE_DIR)
              mark_subtree(node, 1);
          }
          range_from = -1;
        }
        break;
      case ' ':
        if (fl.cursor < fl.count)
        {
          file_node_t *node = fl.entries[fl.cursor].node;
          if (node->type == NODE_DIR)
          {
            node->marked = !node->marked;
            mark_subtree(node, node->marked);
          }
          else
          {
            node->marked = !node->marked;
          }
        }
        break;
      case '*':
        if (fl.cursor < fl.count)
        {
          file_node_t *node = fl.entries[fl.cursor].node;
          if (node->type == NODE_DIR)
            invert_dir(node);
        }
        break;
      case '\t':
        show_all = !show_all;
        break;
      case '/':
        {
          const char *input;
          input = ask_string_ncurses("Search:");
          if (input)
          {
            unsigned int j;
            strncpy(search_buf, input, sizeof(search_buf) - 1);
            search_buf[sizeof(search_buf) - 1] = '\0';
            for (j = 0; j < fl.count; j++)
            {
              if (strstr(fl.entries[j].node->name, search_buf))
              {
                fl.cursor = j;
                break;
              }
            }
          }
        }
        break;
      case KEY_F(5):
        {
          uint64_t marked_size = 0;
          uint64_t marked_count;
          marked_count = tree_count_marked(tree->root, &marked_size);
          if (marked_count == 0)
          {
            display_message("No files marked. Use Space to mark files.");
          }
          else
          {
            char dst_dir[4096];
            char *default_dir;
            default_dir = get_default_location();
            if (default_dir)
            {
              strncpy(dst_dir, default_dir, sizeof(dst_dir) - 1);
              dst_dir[sizeof(dst_dir) - 1] = '\0';
              free(default_dir);
            }
            else
            {
              dst_dir[0] = '\0';
            }
            ask_location(dst_dir, sizeof(dst_dir), "Choose destination", dst_dir, 0);
            if (dst_dir[0])
            {
              int r;
              r = restore_files(tree, disk, partition, dst_dir, NULL);
              if (r == 0)
                display_message("Restore completed.");
              else
                display_message("Restore failed — filesystem could not be re-opened.");
              touchwin(window);
              wrefresh(window);
            }
          }
        }
        break;
      case KEY_F(10):
      case 'q':
      case 'Q':
        running = 0;
        break;
    }
  }

  free(fl.entries);
  delwin(window);
  return 0;
}
#endif
