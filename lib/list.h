/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * list.h
 *
 * by Gary Wong, 1996
 * $Id$
 */

#ifndef _LIST_H_
#define _LIST_H_

typedef struct _list {
  struct _list* plPrev;
  struct _list* plNext;
  void* p;
} list;

extern int ListCreate( list *pl );
/* #define ListDestroy( pl ) ( assert( ListEmpty( pl ) ) ) */

#define ListEmpty( pl ) ( (pl)->plNext == (pl) )
extern list* ListInsert( list* pl, void* p );
extern void ListDelete( list* pl );
extern void ListDeleteAll( const list *pl );

#endif
