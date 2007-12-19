/* This program is free software; you can redistribute it and/or modify
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
 *
 * cache.c
 *
 * by Gary Wong, 1997-2000
 * $Id$
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <stdio.h>
#endif

#include "cache.h"

#if USE_MULTITHREAD
#include "multithread.h"

#ifdef GLIB_THREADS

static void cache_unlock(volatile evalCache* pc, volatile unsigned long lock)
{
	(void)MT_SafeDec(&pc->locks[lock]);
}

static int cache_addlock(volatile evalCache* pc, volatile unsigned long lock)
{
	return (MT_SafeInc(&pc->locks[lock])> 1);
}
static int cache_lock(volatile evalCache* pc, volatile unsigned long lock)
{
	while (cache_addlock(pc, lock))
		cache_unlock(pc, lock);
}
#else

extern void MT_Lock(int *lock);
extern void MT_Unlock(int *lock);
#define cache_lock(pc, l) MT_Lock(&pc->locks[l]);
#define cache_unlock(pc, l) MT_Unlock(&pc->locks[l]);

#endif
#endif

/* Adapted from
   http://burtleburtle.net/bob/c/lookup2.c
   
--------------------------------------------------------------------
lookup2.c, by Bob Jenkins, December 1996, Public Domain.
hash(), hash2(), hash3, and hashmix() are externally useful functions.
Routines to test the hash are included if SELF_TEST is defined.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------

*/

typedef  unsigned long   ub4;

#define hashmix(a,b,c) \
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


int
CacheCreate(evalCache* pc, unsigned int s)
{
#if CACHE_STATS
  pc->cLookup = 0;
  pc->cHit = 0;
  pc->nAdds = 0;
#endif

  /* adjust size ot smallest power of 2 GE to s */
  while( (s & (s-1)) != 0 ) {
    s &= (s-1);
  }

  pc->size = ( s < pc->size ) ? 2*s : s;
  pc->hashMask = (pc->size >> 1) - 1;

  pc->m = (cacheNode*)malloc(pc->size * sizeof(*pc->m));
  pc->locks = (int*)malloc(pc->size * sizeof(*pc->locks));

  if( pc->m == 0 ) {
    return -1;
  }

  CacheFlush(pc);

  return 0;
}

static unsigned long GetHashKey(unsigned long hashMask, const cacheNode* e)
{
  ub4 a = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  ub4 b = a;
  ub4 c = 11 + (unsigned int)e->nEvalContext;

  c += ((ub4)e->auchKey[9]<<16);
  c += ((ub4)e->auchKey[8]<<8);

  b += ((ub4)e->auchKey[7]<<24);
  b += ((ub4)e->auchKey[6]<<16);
  b += ((ub4)e->auchKey[5]<<8);
  b += e->auchKey[4];

  a += ((ub4)e->auchKey[3]<<24);
  a += ((ub4)e->auchKey[2]<<16);
  a += ((ub4)e->auchKey[1]<<8);
  a += e->auchKey[0];

  hashmix(a,b,c);

  return (c & hashMask) << 1;
}

unsigned int CacheLookup(evalCache* pc, const cacheNode* e, float *arOut, float *arCubeful)
{
	unsigned long l = GetHashKey(pc->hashMask, e);

#if CACHE_STATS
	++pc->cLookup;
#endif
#if USE_MULTITHREAD
	cache_lock(pc, l);
#endif
	if ((pc->m[l].nEvalContext != e->nEvalContext ||
		memcmp(pc->m[l].auchKey, e->auchKey, sizeof(e->auchKey)) != 0))
	{	/* Not in first slot */
		if ((pc->m[l + 1].nEvalContext != e->nEvalContext ||
			memcmp(pc->m[l + 1].auchKey, e->auchKey, sizeof(e->auchKey)) != 0))
		{	/* Cache miss */
#if USE_MULTITHREAD
			cache_unlock(pc, l);
#endif
			return l;
		}
		else
		{	/* Found in second slot, promote "hot" entry */
			cacheNode tmp = pc->m[l];
			pc->m[l] = pc->m[l + 1];
			pc->m[l + 1] = tmp;
		}
	}
	/* Cache hit */
#if CACHE_STATS
    ++pc->cHit;
#endif

	memcpy(arOut, pc->m[l].ar, sizeof(float) * 5/*NUM_OUTPUTS*/ );
	if (arCubeful)
		*arCubeful = pc->m[l].ar[6/*OUTPUT_CUBEFUL_EQUITY*/];

#if USE_MULTITHREAD
	cache_unlock(pc, l);
#endif

    return CACHEHIT;
}

void CacheAdd(evalCache* pc, const cacheNode* e, unsigned long l)
{
#if USE_MULTITHREAD
	cache_lock(pc, l);
#endif

	pc->m[l + 1] = pc->m[l];
	pc->m[l] = *e;

#if USE_MULTITHREAD
	cache_unlock(pc, l);
#endif

#if CACHE_STATS
  ++pc->nAdds;
#endif
}

void CacheAddNoKey(evalCache* pc, const cacheNode* e)
{
	CacheAdd(pc, e, GetHashKey(pc->hashMask, e));
}

void
CacheDestroy(const evalCache* pc)
{
  free(pc->m);
}

void
CacheFlush(const evalCache* pc)
{
  unsigned int k;
  for(k = 0; k < pc->size; ++k) {
    pc->m[k].nEvalContext = -1;
	pc->locks[k] = 0;
  }
}

int
CacheResize( evalCache *pc, unsigned int cNew )
{
  if( cNew == pc->size ) {
    return 0;
  }
  
  CacheDestroy(pc);
  return CacheCreate(pc, cNew);
}

void
CacheStats(const evalCache* pc, unsigned int* pcLookup, unsigned int* pcHit, unsigned int* pcUsed)
{
#if CACHE_STATS
   if ( pcLookup )
      *pcLookup = pc->cLookup;

   if ( pcHit )
      *pcHit = pc->cHit;

   if( pcUsed )
      *pcUsed = pc->nAdds;
#else
   if ( pcLookup )
      *pcLookup = 0;

   if ( pcHit )
      *pcHit = 0;

   if ( pcUsed )
      *pcUsed = 0;
#endif
}
