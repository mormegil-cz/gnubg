/*
 * dice.c
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

#include "config.h"

#include <stdlib.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include "dice.h"
#include "mt19937int.h"
#include "isaac.h"

rng rngCurrent = RNG_MERSENNE;

randctx rc;

extern void InitRNGSeed( int n ) {
    
    switch( rngCurrent ) {
    case RNG_ANSI:
	srand( n );
	break;

    case RNG_BSD:
	srandom( n );
	break;

    case RNG_ISAAC: {
	int i;

	for( i = 0; i < RANDSIZ; i++ )
	    rc.randrsl[ i ] = n;

	irandinit( &rc, TRUE );
	
	break;
    }

    case RNG_MERSENNE:
	sgenrand( n );
	break;

    case RNG_USER:
	/* FIXME */
	break;

    case RNG_MANUAL:
	/* no-op */
    }
}

extern void InitRNG( void ) {

    int n;
    
#if HAVE_GETTIMEOFDAY
    struct timeval tv;
    struct timezone tz;

    if( !gettimeofday( &tv, &tz ) )
	n = tv.tv_sec ^ tv.tv_usec;
    else
#endif
	n = time( NULL );

    InitRNGSeed( n );
}

extern void RollDice( int anDice[ 2 ] ) {

    switch( rngCurrent ) {
    case RNG_ANSI:
	anDice[ 0 ] = ( rand() % 6 ) + 1;
	anDice[ 1 ] = ( rand() % 6 ) + 1;
	break;
	
    case RNG_BSD:
#if HAVE_RANDOM
	anDice[ 0 ] = ( random() % 6 ) + 1;
	anDice[ 1 ] = ( random() % 6 ) + 1;
#else
	abort();
#endif
	break;
	
    case RNG_ISAAC:
	anDice[ 0 ] = ( irand( &rc ) % 6 ) + 1;
	anDice[ 1 ] = ( irand( &rc ) % 6 ) + 1;
	break;
	
    case RNG_MANUAL:
	abort();
	
    case RNG_MERSENNE:
	anDice[ 0 ] = ( genrand() % 6 ) + 1;
	anDice[ 1 ] = ( genrand() % 6 ) + 1;
	break;
	
    case RNG_USER:
	abort();
    }
}
