/*
 * dynarray.h
 *
 * by Gary Wong, 1996
 */

#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

typedef struct _dynarray {
    void **ap;
    int c, cp, iFinish;
    int fCompact;
} dynarray;

extern int DynArrayCreate( dynarray *pda, int c, int fCompact );
extern int DynArrayDestroy( dynarray *pda );
extern int DynArrayAdd( dynarray *pda, void *p );
extern int DynArrayDelete( dynarray *pda, int i );
extern int DynArrayRemove( dynarray *pda, void *p );
extern int DynArraySet( dynarray *pda, int i, void *p );

#endif
