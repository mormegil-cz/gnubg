/*
 * cache.h
 *
 * by Gary Wong, 1997-2000
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdlib.h>

typedef int ( *cachecomparefunc )( void *p0, void *p1 );

typedef struct _cachenode {
    long l;
    void *p;
} cachenode;

typedef struct _cache {
    cachenode **apcn;
    int c, icp, cLookup, cHit;
    cachecomparefunc pccf;
} cache;

extern int CacheCreate( cache *pc, int c, cachecomparefunc pccf );
extern int CacheDestroy( cache *pc );
extern int CacheAdd( cache *pc, unsigned long l, void *p, size_t cb );
extern void *CacheLookup( cache *pc, unsigned long l, void *p );
extern int CacheFlush( cache *pc );
extern int CacheResize( cache *pc, int cNew );
extern int CacheStats( cache *pc, int *pcLookup, int *pcHit );

#endif
