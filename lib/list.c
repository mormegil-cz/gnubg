/*
 * list.c
 *
 * by Gary Wong, 1996
 */

#include <list.h>
#include <stdlib.h>

int ListCreate( list *pl ) {
    
    pl->plPrev = pl->plNext = pl;
    pl->p = NULL;

    return 0;
}

list *ListInsert( list *pl, void *p ) {

    list *plNew;

    if( !( plNew = malloc( sizeof( *plNew ) ) ) )
	return NULL;

    plNew->p = p;

    plNew->plNext = pl;
    plNew->plPrev = pl->plPrev;

    pl->plPrev = plNew;
    plNew->plPrev->plNext = plNew;

    return plNew;
}

int ListDelete( list *pl ) {

    pl->plPrev->plNext = pl->plNext;
    pl->plNext->plPrev = pl->plPrev;

    free( pl );

    return 0;
}
