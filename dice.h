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

typedef enum _rng {
    RNG_ANSI, RNG_BBS, RNG_BSD, RNG_ISAAC, RNG_MANUAL, RNG_MD5, RNG_MERSENNE, 
    RNG_RANDOM_DOT_ORG, RNG_USER, RNG_FILE,
    NUM_RNGS
} rng;

extern char *aszRNG[ NUM_RNGS ];

extern char szDiceFilename[];

extern rng rngCurrent;

extern int InitRNG( int *pnSeed, int fSet, const rng rngx );
extern void PrintRNGSeed( const rng rngx );
extern void InitRNGSeed( int n, const rng rngx );

extern int 
RollDice( int anDice[ 2 ], const rng rngx );

#if HAVE_LIBGMP
extern int InitRNGSeedLong( char *sz, rng rng );
extern int InitRNGBBSModulus( char *sz );
extern int InitRNGBBSFactors( char *sz0, char *sz1 );
#endif

#if HAVE_LIBDL
extern int UserRNGOpen( char * );
extern void UserRNGClose( void );
#endif

extern int
OpenDiceFile( const char *sz );

extern void
CloseDiceFile( void );

#endif
