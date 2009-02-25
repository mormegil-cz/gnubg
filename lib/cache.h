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
 * cache.h
 *
 * by Gary Wong, 1997-2000
 * $Id$
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdlib.h>

#define CACHE_STATS 1	/* Calculate simple cache stats */

typedef struct _cacheNodeDetail {
  unsigned char auchKey[10];
  int nEvalContext;
  float ar[6];
} cacheNodeDetail;

typedef struct _cacheNode {
  cacheNodeDetail nd_primary;
  cacheNodeDetail nd_secondary;
#if USE_MULTITHREAD
  int lock;
#endif
} cacheNode;

/* name used in eval.c */
typedef cacheNodeDetail evalcache;

typedef struct _cache
{
  cacheNode*	entries;
  
  unsigned int size;
  unsigned long hashMask;

#if CACHE_STATS
  unsigned int nAdds;
  unsigned int cLookup;
  unsigned int cHit;
#endif
} evalCache;

/* Cache size will be adjusted to a power of 2 */
int CacheCreate(evalCache* pc, unsigned int size);
int CacheResize(evalCache *pc, unsigned int cNew);

#define CACHEHIT ((unsigned int)-1)
/* returns a value which is passed to CacheAdd (if a miss) */
unsigned int CacheLookup(evalCache* pc, const cacheNodeDetail* e, float *arOut, float *arCubeful);

void CacheAdd(evalCache* pc, const cacheNodeDetail* e, unsigned long l);
void CacheFlush(const evalCache* pc);
void CacheDestroy(const evalCache* pc);
void CacheStats(const evalCache* pc, unsigned int* pcLookup, unsigned int* pcHit, unsigned int* pcUsed);

unsigned long GetHashKey(unsigned long hashMask, const cacheNodeDetail* e);

#endif
