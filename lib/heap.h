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
 * heap.h
 *
 * by Gary Wong, 1997
 * $Id$
 */

#ifndef _HEAP_H_
#define _HEAP_H_

typedef int ( *heapcomparefunc )( void *p0, void *p1 );

typedef struct _heap {
    void **ap;
    unsigned int cp, cpAlloc;
    heapcomparefunc phcf;
} heap;

extern int HeapCreate( heap *ph, unsigned int c, heapcomparefunc phcf );
extern void HeapDestroy( const heap *ph );
extern int HeapInsert( heap *ph, void *p );
extern void *HeapDelete( heap *ph );
extern int HeapDeleteItem( heap *ph, const void *p );
extern void *HeapLookup( const heap *ph );

#endif
