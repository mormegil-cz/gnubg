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

#define cache_lock(pc, k) \
if (MT_SafeIncCheck(&(pc->entries[k].lock))) \
	WaitForLock(&(pc->entries[k].lock))

#define cache_unlock(pc, k) MT_SafeDec(&(pc->entries[k].lock))

static void WaitForLock(int *lock)
{
	do
	{
		MT_SafeDec(lock);
	} while (MT_SafeIncCheck(lock));
}
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

int CacheCreate(evalCache* pc, unsigned int s)
{
#if CACHE_STATS
	pc->cLookup = 0;
	pc->cHit = 0;
	pc->nAdds = 0;
#endif

	pc->size = s;
	/* adjust size to smallest power of 2 GE to s */
	while ((s & (s-1)) != 0)
		s &= (s-1);

	pc->size = (s < pc->size) ? 2 * s : s;
	pc->hashMask = (pc->size >> 1) - 1;

	pc->entries = (cacheNode*)malloc((pc->size / 2) * sizeof(*pc->entries));
	if (pc->entries == 0)
		return -1;

	CacheFlush(pc);
	return 0;
}

extern unsigned long GetHashKey(unsigned long hashMask, const cacheNodeDetail* e)
{
	ub4 a = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	ub4 b = a;
	ub4 c = 11 + (unsigned int)e->nEvalContext;

	c = c + (((short)e->key.data[2]) << 8);
	b = b + e->key.data[1];
	a = a + e->key.data[0];

	/* hashmix macro expanded here */
	a = (a - b - c) ^ (c >> 13);
	b = (b - c - a) ^ (a << 8);
	c = (c - a - b) ^ (b >> 13);
	a = (a - b - c) ^ (c >> 12);
	b = (b - c - a) ^ (a << 16);
	c = (c - a - b) ^ (b >> 5);
	a = (a - b - c) ^ (c >> 3);
	b = (b - c - a) ^ (a << 10);
	c = (c - a - b) ^ (b >> 15);

	return (c & hashMask);
}

unsigned int CacheLookup(evalCache* pc, const cacheNodeDetail* e, float *arOut, float *arCubeful)
{
	unsigned long l = GetHashKey(pc->hashMask, e);

#if CACHE_STATS
	++pc->cLookup;
#endif
#if USE_MULTITHREAD
	cache_lock(pc, l);
#endif
	if ((pc->entries[l].nd_primary.nEvalContext != e->nEvalContext ||
		memcmp(pc->entries[l].nd_primary.key.auch, e->key.auch, sizeof(e->key.auch)) != 0))
	{	/* Not in primary slot */
		if ((pc->entries[l].nd_secondary.nEvalContext != e->nEvalContext ||
			memcmp(pc->entries[l].nd_secondary.key.auch, e->key.auch, sizeof(e->key.auch)) != 0))
		{	/* Cache miss */
#if USE_MULTITHREAD
			cache_unlock(pc, l);
#endif
			return l;
		}
		else
		{	/* Found in second slot, promote "hot" entry */
			cacheNodeDetail tmp = pc->entries[l].nd_primary;

			pc->entries[l].nd_primary = pc->entries[l].nd_secondary;
			pc->entries[l].nd_secondary = tmp;
		}
	}
	/* Cache hit */
#if CACHE_STATS
    ++pc->cHit;
#endif

	memcpy(arOut, pc->entries[l].nd_primary.ar, sizeof(float) * 5/*NUM_OUTPUTS*/ );
	if (arCubeful)
		*arCubeful = pc->entries[l].nd_primary.ar[5];	/* Cubeful equity stored in slot 5 */

#if USE_MULTITHREAD
	cache_unlock(pc, l);
#endif

    return CACHEHIT;
}

void CacheAdd(evalCache* pc, const cacheNodeDetail* e, unsigned long l)
{
#if USE_MULTITHREAD
	cache_lock(pc, l);
#endif

	pc->entries[l].nd_secondary = pc->entries[l].nd_primary;
	pc->entries[l].nd_primary = *e;

#if USE_MULTITHREAD
	cache_unlock(pc, l);
#endif

#if CACHE_STATS
  ++pc->nAdds;
#endif
}

void CacheDestroy(const evalCache* pc)
{
  free(pc->entries);
}

void CacheFlush(const evalCache* pc)
{
  unsigned int k;
  for(k = 0; k < pc->size / 2; ++k) {
    pc->entries[k].nd_primary.nEvalContext = -1;
    pc->entries[k].nd_secondary.nEvalContext = -1;
#if USE_MULTITHREAD
	pc->entries[k].lock = 0;
#endif
  }
}

int CacheResize(evalCache *pc, unsigned int cNew)
{
	if (cNew != pc->size)
	{
		CacheDestroy(pc);
		if (CacheCreate(pc, cNew) != 0)
			return -1;
	}

	return (int)pc->size;
}

void CacheStats(const evalCache* pc, unsigned int* pcLookup, unsigned int* pcHit, unsigned int* pcUsed)
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
