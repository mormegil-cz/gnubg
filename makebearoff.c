/*
 * makebearoff.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1997-1999.
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
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "eval.h"
#include "positionid.h"

static unsigned short ausRolls[ 54264 ], aaProb[ 54264 ][ 32 ];
static double aaEquity[ 924 ][ 924 ];

static void BearOff( int nId ) {

    int i, iBest, iMode, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ],
	anBoardTemp[ 2 ][ 25 ], aProb[ 32 ];
    movelist ml;
    unsigned short us;

    PositionFromBearoff( anBoard[ 1 ], nId );

    for( i = 0; i < 32; i++ )
	aProb[ i ] = 0;
    
    for( i = 6; i < 25; i++ )
	anBoard[ 1 ][ i ] = 0;

    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = 0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    us = 0xFFFF; iBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ] );

		assert( j >= 0 );
		assert( j < nId );
		
		if( ausRolls[ j ] < us ) {
		    iBest = j;
		    us = ausRolls[ j ];
		}
	    }

	    assert( iBest >= 0 );
	    
	    if( anRoll[ 0 ] == anRoll[ 1 ] )
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += aaProb[ iBest ][ i ];
	    else
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += ( aaProb[ iBest ][ i ] << 1 );
	}
    
    for( i = 0, j = 0, iMode = 0; i < 32; i++ ) {
	j += ( aaProb[ nId ][ i ] = ( aProb[ i ] + 18 ) / 36 );
	if( aaProb[ nId ][ i ] > aaProb[ nId ][ iMode ] )
	    iMode = i;
    }

    aaProb[ nId ][ iMode ] -= ( j - 0xFFFF );
    
    for( j = 0, i = 1; i < 32; i++ )
	j += i * aProb[ i ];

    ausRolls[ nId ] = j / 2359;
}

static void BearOff2( int nUs, int nThem ) {

    int i, iBest, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ],
	anBoardTemp[ 2 ][ 25 ];
    movelist ml;
    double r, rTotal;

    PositionFromBearoff( anBoard[ 0 ], nThem );
    PositionFromBearoff( anBoard[ 1 ], nUs );

    for( i = 6; i < 25; i++ )
	anBoard[ 1 ][ i ] = anBoard[ 0 ][ i ] = 0;

    rTotal = 0.0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    r = -1.0; iBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ] );

		assert( j >= 0 );
		assert( j < nUs );

		if( 1.0 - aaEquity[ nThem ][ j ] > r ) {
		    iBest = j;
		    r = 1.0 - aaEquity[ nThem ][ j ];
		}
	    }

	    assert( iBest >= 0 );
	    
	    if( anRoll[ 0 ] == anRoll[ 1 ] )
		rTotal += r;
	    else
		rTotal += r * 2.0;
	}

    aaEquity[ nUs ][ nThem ] = rTotal / 36.0;
}

extern int main( void ) {

    int i, j, k;
    
    aaProb[ 0 ][ 0 ] = 0xFFFF;
    for( i = 1; i < 32; i++ )
	aaProb[ 0 ][ i ] = 0;

    ausRolls[ 0 ] = 0;

    for( i = 1; i < 54264; i++ ) {
	BearOff( i );

	if( !( i % 100 ) )
	    fprintf( stderr, "1:%d\r", i );
    }

    for( i = 0; i < 54264; i++ )
	for( j = 0; j < 32; j++ ) {
	    putchar( aaProb[ i ][ j ] & 0xFF );
	    putchar( aaProb[ i ][ j ] >> 8 );
	}

    for( i = 0; i < 924; i++ ) {
	aaEquity[ i ][ 0 ] = 0.0;
	aaEquity[ 0 ][ i ] = 1.0;
    }

    for( i = 1; i < 924; i++ ) {
	for( j = 0; j < i; j++ )
	    BearOff2( i - j, j + 1 );

	fprintf( stderr, "2a:%d   \r", i );
    }

    for( i = 0; i < 924; i++ ) {
	for( j = i + 2; j < 924; j++ )
	    BearOff2( i + 925 - j, j );

	fprintf( stderr, "2b:%d   \r", i );
    }

    for( i = 0; i < 924; i++ )
	for( j = 0; j < 924; j++ ) {
	    k = aaEquity[ i ][ j ] * 65535.5;
	    
	    putchar( k & 0xFF );
	    putchar( k >> 8 );
	}
    
    return 0;
}
