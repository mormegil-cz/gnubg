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

typedef struct _cacheNode {
  unsigned char auchKey[10];
  int nEvalContext;
  float ar[7 /*NUM_ROLLOUT_OUTPUTS*/];
} cacheNode;

/* name used in eval.c */
typedef cacheNode evalcache;

typedef struct _cache
{
  cacheNode*	m;
  int *locks;
  
  unsigned int size;
  unsigned long hashMask;

#if CACHE_STATS
  unsigned int nAdds;
  unsigned int cLookup;
  unsigned int cHit;
#endif
} cache;

/* Cache size will be adjusted to a power of 2 */
int CacheCreate(cache* pc, unsigned int size);
int CacheResize(cache *pc, unsigned int cNew);

#define CACHEHIT ((unsigned int)-1)
/* returns a value which is passed to CacheAdd (if a miss) */
unsigned int CacheLookup(cache* pc, const cacheNode* e, float *arOut, float *arCubeful);

void CacheAdd(cache* pc, const cacheNode* e, unsigned long l);
void CacheAddNoKey(cache* pc, const cacheNode* e);
void CacheFlush(const cache* pc);
void CacheDestroy(const cache* pc);
void CacheStats(const cache* pc, unsigned int* pcLookup, unsigned int* pcHit, unsigned int* pcUsed);

#endif
