/*
 * hash.c
 *
 * by Gary Wong, 1997-2000
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cache.h"
#include "hash.h"

static int ac[] = { 2, 3, 7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093,
		    8191, 16381, 32749, 65521, 131071, 262139, 524287,
		    1048573, 2097143, 4194301, 8388593, 16777213, 33554393,
		    67108859, 134217689, 268435399, 536870909, 1073741789, 0 };

extern int HashCreate( hash *ph, int c, hashcomparefunc phcf ) {

    int i;
    
    for( i = 0; ac[ i + 1 ] && c > ac[ i ]; i++ )
	;
    
    if( !( ph->aphn = calloc( ac[ i ], sizeof( hashnode * ) ) ) )
	return -1;
    
    ph->c = 0;
    ph->icp = i;
    ph->phcf = phcf;
    ph->cSize = ac[ i ];
    ph->cHits = ph->cMisses = ph->cLookups = 0;

    return 0;
}

extern int HashDestroy( hash *ph ) {

    hashnode *phn, *phnNext;
    int i;
    
    for( i = 0; i < ac[ ph->icp ]; i++ ) {
	phn = ph->aphn[ i ];

	while( phn ) {
	    phnNext = phn->phnNext;

	    free( phn );

	    phn = phnNext;
	}
    }

    free( ph->aphn );
    
    return 0;
}

extern int HashAdd( hash *ph, unsigned long l, void *p ) {

    /* FIXME check for rehash */

    hashnode *phn = (hashnode *) malloc( sizeof( *phn ) ),
	**pphn = ph->aphn + l % ac[ ph->icp ];
    
    phn->phnNext = *pphn;
    phn->l = l;
    phn->p = p;
    
    *pphn = phn;
    ph->c++;

    return 0;
}

extern int HashDelete( hash *ph, unsigned long l, void *p ) {

    hashnode **pphn, *phnNext;

    for( pphn = ph->aphn + l % ac[ ph->icp ]; *pphn;
	 pphn = &( ( *pphn )->phnNext ) )
	if( ( ( *pphn )->l == l ) && ( !ph->phcf ||
	    !( ph->phcf )( p, ( *pphn )->p ) ) ) {
	    phnNext = ( *pphn )->phnNext;
	    free( *pphn );
	    *pphn = phnNext;

	    ph->c--;
	    
	    /* FIXME check for rehash */

	    return 0;
	}

    return -1;
}

extern void *HashLookup( hash *ph, unsigned long l, void *p ) {

    hashnode **pphn;

    ++ph->cLookups;

    for( pphn = ph->aphn + l % ac[ ph->icp ]; *pphn;
	 pphn = &( ( *pphn )->phnNext ) )
	if( ( ( *pphn )->l == l ) && ( !ph->phcf ||
            !( ph->phcf )( p, ( *pphn )->p ) ) ) {
            ++ph->cHits;
	    return ( *pphn )->p;
        }
    
    ++ph->cMisses;
    return NULL;
}

extern long StringHash( char *sz ) {

    long l = 0;
    char *pch;

    for( pch = sz; *pch; pch++ )
	l = ( ( l << 8 ) % 8388593 ) ^ *pch;

    return l;
}

#if defined( GARY_CACHE )

extern int CacheCreate( cache *pc, int c, cachecomparefunc pccf ) {

    int i;
    
    for( i = 0; ac[ i + 1 ] && c > ac[ i ]; i++ )
	;
    
    if( !( pc->apcn = calloc( ac[ i ], sizeof( cachenode * ) ) ) )
	return -1;
    
    pc->c = 0;
    pc->icp = i;
    pc->pccf = pccf;
    pc->cLookup = pc->cHit = 0;
    
    return 0;
}

extern int CacheFlush( cache *pc ) {

    int i;

    if( pc->apcn )
	for( i = 0; i < ac[ pc->icp ]; i++ )
	    if( pc->apcn[ i ] ) {
		free( pc->apcn[ i ]->p );
		free( pc->apcn[ i ] );
		pc->apcn[ i ] = NULL;
	    }
    
    return 0;
}

extern int CacheDestroy( cache *pc ) {

    CacheFlush( pc );

    if( pc->apcn )
	free( pc->apcn );

    return 0;
}

extern int CacheAdd( cache *pc, unsigned long l, void *p, size_t cb ) {

    cachenode **ppcn, *pcn;

    if( !pc->apcn ) {
	errno = EINVAL;
	return -1;
    }
    
    ppcn = pc->apcn + l % ac[ pc->icp ];
    pcn = *ppcn;

    if( pcn )
	free( pcn->p );
    else
	if( !( pcn = (cachenode *) malloc( sizeof( *pcn ) ) ) )
	    return -1;

    pcn->l = l;
    if( !( pcn->p = malloc( cb ) ) ) {
	free( pcn );
	*ppcn = NULL;
	return -1;
    } else
	pc->c++;

    memcpy( pcn->p, p, cb );
    *ppcn = pcn;

    return 0;
}

extern void *CacheLookup( cache *pc, unsigned long l, void *p ) {

    cachenode **ppcn;

    if( !pc->apcn )
	return NULL;
    
    ppcn = pc->apcn + l % ac[ pc->icp ];

    pc->cLookup++;
    
    return *ppcn && ( *ppcn )->l ==l && ( !pc->pccf  ||
					 !( pc->pccf )( p, ( *ppcn )->p ) ) ?
	pc->cHit++, ( *ppcn )->p : NULL;
}

extern int CacheResize( cache *pc, int cNew ) {

    /* FIXME would be nice to save old cache entries (by rehashing), but
       it's easier just to throw them out and start again */
    
    cachecomparefunc pccf = pc->pccf;
    int icp = pc->icp;
    
    CacheDestroy( pc );

    if( CacheCreate( pc, cNew, pccf ) ) {
	/* resize failed; try to recreate the original size */
	CacheCreate( pc, ac[ icp ], pccf );
	return -1;
    }
    
    return 0;
}

extern int CacheStats( cache *pc, int *pcLookup, int *pcHit ) {

    if( pcLookup )
	*pcLookup = pc->cLookup;

    if( pcHit )
	*pcHit = pc->cHit;

    return 0;
}


#else

/* Adapted from
   http://burtleburtle.net/bob/c/lookup2.c
   
--------------------------------------------------------------------
lookup2.c, by Bob Jenkins, December 1996, Public Domain.
hash(), hash2(), hash3, and mix() are externally useful functions.
Routines to test the hash are included if SELF_TEST is defined.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------

*/

typedef  unsigned long   ub4;

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

extern unsigned long
keyToLong(char k[10], int np)
{
  ub4 a = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  ub4 b = a;
  ub4 c = 11+np;

  c += ((ub4)k[9]<<16);
  c += ((ub4)k[8]<<8);

  b += ((ub4)k[7]<<24);
  b += ((ub4)k[6]<<16);
  b += ((ub4)k[5]<<8);
  b += k[4];

  a += ((ub4)k[3]<<24);
  a += ((ub4)k[2]<<16);
  a += ((ub4)k[1]<<8);
  a += k[0];

  mix(a,b,c);

  return c;
}


int
CacheCreate(cache* pc, unsigned int s)
{
  pc->cLookup = 0;
  pc->cHit = 0;
  pc->nAdds = 0;

  /* adjust size ot smallest power of 2 GE to s */
  pc->size = s;

  while( (s & (s-1)) != 0 ) {
    s &= (s-1);
  }

  pc->size = ( s < pc->size ) ? 2*s : s;
  
  pc->m = malloc(pc->size * sizeof(*pc->m));

  if( pc->m == 0 ) {
    return -1;
  }
  
  CacheFlush(pc);

  return 0;
}

cacheNode*
CacheLookup(cache* pc, cacheNode* e, unsigned long* m)
{
  unsigned int size = pc->size;
  unsigned long l = keyToLong(e->auchKey, e->nEvalContext);
  
  l = (l & ((size >> 1)-1)) << 1;

  if( m ) {
    *m = l;
  }

  ++pc->cLookup;

  {
  cacheNode* ck1 = pc->m + l;
  if( (ck1->nEvalContext == e->nEvalContext
       && memcmp(e->auchKey, ck1->auchKey, sizeof(e->auchKey)) == 0) ) {
    // found at first slot
    ++pc->cHit;
    return ck1;
  }

  if( ck1->nEvalContext != (unsigned int)-1 ) {
    cacheNode* ck2 = pc->m + (l+1);
    if( (ck2->nEvalContext == e->nEvalContext &&
	 memcmp(e->auchKey, ck2->auchKey, sizeof(e->auchKey)) == 0) ) {
      // found at second slot. Make it primary
      cacheNode tmp = *ck1;
      *ck1 = *ck2;
      *ck2 = tmp;

      ++pc->cHit;
      return ck1;
    }
  }
  }
  
  return 0;
}

void
CacheAdd(cache* pc, cacheNode* e, unsigned long l)
{
  cacheNode* ck1 = pc->m + l;

  ++pc->nAdds;
  
  if( ck1->nEvalContext != (unsigned int)-1 ) {
    pc->m[l+1] = *ck1;
  }
  *ck1 = *e;
}


void
CacheDestroy(cache* pc)
{
  free(pc->m);
}

void
CacheFlush(cache* pc)
{
  int k;
  for(k = 0; k < pc->size; ++k) {
    pc->m[k].nEvalContext = (unsigned int)-1;
  }
}

int
CacheResize( cache *pc, int cNew )
{
  if( cNew == pc->size ) {
    return 0;
  }
  
  CacheDestroy(pc);
  return CacheCreate(pc, cNew);
}

void
CacheStats(cache* pc, int* pcLookup, int* pcHit)
{
   if ( pcLookup )
      *pcLookup = pc->cLookup;

   if ( pcHit )
      *pcHit = pc->cHit;
}


#endif
