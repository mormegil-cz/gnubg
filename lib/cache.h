/*
 * cache.h
 *
 * by Gary Wong, 1997-2000
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdlib.h>

#if PROCESSING_UNITS
#include <pthread.h>
#endif

#if defined( GARY_CACHE )
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

#else

typedef struct _cacheNode {
  unsigned char auchKey[10];
  int nEvalContext;
  float ar[7 /*NUM_ROLLOUT_OUTPUTS*/];
} cacheNode;

/* name used in eval.c */
typedef cacheNode evalcache;

typedef struct _cache {
  cacheNode*	m;
  
  unsigned int size;
  unsigned int nAdds;
  unsigned int cLookup;
  unsigned int cHit;
#if PROCESSING_UNITS
  pthread_mutex_t mutex;
#endif
} cache;

/* Cache size will be adjusted to a power of 2 */
int
CacheCreate(cache* pc, unsigned int size);

int
CacheResize(cache *pc, int cNew);

/* l is filled with a value which is passed to CacheAdd */
cacheNode*
CacheLookup(cache* pc, cacheNode* e, unsigned long* l);

void CacheAdd(cache* pc, cacheNode* e, unsigned long l);
void CacheFlush(cache* pc);
void CacheDestroy(cache* pc);
void CacheStats(cache* pc, int* pcLookup, int* pcHit);

extern unsigned long
keyToLong(char k[10], int np);

#endif


#endif
