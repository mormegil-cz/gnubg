/*
 * dice.h
 *
 * by Gary Wong, 1999
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

#ifndef _DICE_H_
#define _DICE_H_

#include <sys/param.h>

typedef enum _rng {
    RNG_ANSI, RNG_BSD, RNG_ISAAC, RNG_MANUAL, RNG_MERSENNE, RNG_USER
} rng;

extern rng rngCurrent;

extern int InitRNG( void );
extern void InitRNGSeed( int n );
extern void RollDice( int anDice[ 2 ] );
extern void GetManualDice( int anDice[ 2 ] );

void (*pfUserRNGSeed) (unsigned long int);
long int (*pfUserRNGRandom) (void);
void *pvUserRNGHandle;

char szUserRNGSeed[ 32 ];
char szUserRNGRandom[ 32 ];
char szUserRNG[ MAXPATHLEN ];

extern void UserRNGClose();
extern int UserRNGOpen();

#endif
