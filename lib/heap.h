/*
 * heap.h
 *
 * by Gary Wong, 1997
 */

#ifndef _HEAP_H_
#define _HEAP_H_

typedef int ( *heapcomparefunc )( void *p0, void *p1 );

typedef struct _heap {
    void **ap;
    int cp, cpAlloc;
    heapcomparefunc phcf;
} heap;

extern int HeapCreate( heap *ph, int c, heapcomparefunc phcf );
extern int HeapDestroy( heap *ph );
extern int HeapInsert( heap *ph, void *p );
extern void *HeapDelete( heap *ph );
extern int HeapDeleteItem( heap *ph, void *p );
extern void *HeapLookup( heap *ph );

#endif
