/*
 * progress.h
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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
 * $Id$
 */

#ifndef _PROGRESS_H_
#define _PROGRESS_H_

#include "eval.h"
#include "rollout.h"

extern void
RolloutProgressStart( const cubeinfo *pci, const int n,
                      rolloutstat aars[][ 2 ],
                      rolloutcontext *pes, char asz[][ 40 ], void **pp );

extern void
RolloutProgress( float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                 float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                 const rolloutcontext *prc,
                 const cubeinfo aci[],
                 const int iGame,
                 const int iAlternative,
				 const int nRank,
				 const float rJsd,
				 const int fStopped,
				 const int fShowRanks,
                 void *pUserData );

extern void
RolloutProgressEnd( void **pp );

#endif /* _PROGRESS_H_ */

