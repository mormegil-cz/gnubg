/*
 * list.h
 *
 * by Gary Wong, 1996
 */

#ifndef _LIST_H_
#define _LIST_H_

#ifndef DIM
#define DIM( x ) ( sizeof( x ) / sizeof( (x)[ 0 ] ) )
#endif

typedef struct _list {
    struct _list *plPrev, *plNext;
    void *p;
} list;

extern int ListCreate( list *pl );
/* #define ListDestroy( pl ) ( assert( ListEmpty( pl ) ) ) */

#define ListEmpty( pl ) ( (pl)->plNext == (pl) )
extern list *ListInsert( list *pl, void *p );
extern int ListDelete( list *pl );

#endif
