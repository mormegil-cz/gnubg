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
    RNG_ANSI, RNG_BSD, RNG_ISAAC, RNG_MANUAL, RNG_MD5, RNG_MERSENNE, 
    RNG_RANDOM_DOT_ORG, RNG_USER
} rng;

extern char *aszRNG[];

extern rng rngCurrent;

extern int InitRNG( int *pnSeed, int fSet );
extern void PrintRNGSeed( void );
extern void InitRNGSeed( int n );
extern int RollDice( int anDice[ 2 ] );

extern void UserRNGClose();
extern int UserRNGOpen();

#endif
