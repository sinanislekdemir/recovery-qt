/*

    File: list_add_sorted.h

    Copyright (C) 2006-2008 Christophe GRENIER <grenier@cgsecurity.org>

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
#ifndef _LIST_ADD_SORTED_UNIQ_H
#define _LIST_ADD_SORTED_UNIQ_H

/*
  X requires finite(head);
  X ensures  finite(head);
  X ensures  finite(newe);
  X ensures  reachable(head,newe);
  */

static inline void td_list_add_sorted(struct td_list_head *newe, struct td_list_head *head,
    int (*compar)(const struct td_list_head *a, const struct td_list_head *b))
{
  struct td_list_head *pos;
  
/*
    X loop invariant finite(head->prev);
    X loop invariant finite(head);
*/
  td_list_for_each(pos, head)
  {
    
    
    
    if(compar(newe,pos)<0)
      break;
  }
  if(pos != head)
  {
      __td_list_add(newe, pos->prev, pos);
  }
  else
  {
    
    
    
    
    td_list_add_tail(newe, head);
  }
  /*X assert finite(head); */
  /*X assert finite(newe); */
}
#endif
