/*
 * positionid.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
 *
 * An implementation of the position key/IDs at:
 *
 *   http://www.cs.arizona.edu/~gary/backgammon/positionid.html
 *
 * Please see that page for more information.
 *
 *
 * This library also calculates bearoff IDs, which are enumerations of the
 *
 *    c+6
 *       C
 *        6
 *
 * combinations of (up to c) chequers among 6 points.
 *
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

#include <assert.h>
#include <errno.h>
#include "positionid.h"

static void AddBit( int b, unsigned char **ppuch, int *piBit ) {

    if( b )
	**ppuch |= 1 << *piBit;

    if( ++*piBit > 7 ) {
	*piBit = 0;
	++*ppuch;
    }
}

extern void PositionKey( int anBoard[ 2 ][ 25 ],
			 unsigned char auchKey[ 10 ] ) {
    
    unsigned char *puch;
    int i, j, iBit = 0, iChequer;
    
    for( puch = auchKey; puch < &auchKey[ 10 ]; puch++ )
	*puch = 0;
    
    puch = auchKey;

    for( i = 0; i < 2; i++ )
	for( j = 0; j < 25; j++ ) {
	    for( iChequer = 0; iChequer < anBoard[ i ][ j ]; iChequer++ )
		AddBit( 1, &puch, &iBit );

	    AddBit( 0, &puch, &iBit );
	}    
}

extern char *PositionIDFromKey( unsigned char auchKey[ 10 ] ) {

    unsigned char *puch = auchKey;
    static char szID[ 15 ];
    char *pch = szID;
    static char aszBase64[ 64 ] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i;
    
    for( i = 0; i < 3; i++ ) {
	*pch++ = aszBase64[ puch[ 0 ] >> 2 ];
	*pch++ = aszBase64[ ( ( puch[ 0 ] & 0x03 ) << 4 ) |
			  ( puch[ 1 ] >> 4 ) ];
	*pch++ = aszBase64[ ( ( puch[ 1 ] & 0x0F ) << 2 ) |
			  ( puch[ 2 ] >> 6 ) ];
	*pch++ = aszBase64[ puch[ 2 ] & 0x3F ];

	puch += 3;
    }

    *pch++ = aszBase64[ *puch >> 2 ];
    *pch++ = aszBase64[ ( *puch & 0x03 ) << 4 ];

    *pch = 0;

    return szID;
}

extern char *PositionID( int anBoard[ 2 ][ 25 ] ) {

    unsigned char auch[ 10 ];
    
    PositionKey( anBoard, auch );

    return PositionIDFromKey( auch );
}

static int ReadBit( unsigned char **ppuch, int *piBit ) {

    int n = **ppuch & ( 1 << *piBit );

    if( ++*piBit > 7 ) {
	*piBit = 0;
	++*ppuch;
    }

    return n != 0;
}

static int CheckPosition( int anBoard[ 2 ][ 25 ] ) {

    int ac[ 2 ], i;

    /* Check for a player with over 15 chequers */
    for( i = ac[ 0 ] = ac[ 1 ] = 0; i < 25; i++ )
	if( ( ac[ 0 ] += anBoard[ 0 ][ i ] ) > 15 ||
	    ( ac[ 1 ] += anBoard[ 1 ][ i ] ) > 15 ) {
	    errno = EINVAL;
	    return -1;
	}

    /* Check for both players having chequers on the same point */
    for( i = 0; i < 24; i++ )
	if( anBoard[ 0 ][ i ] && anBoard[ 1 ][ 23 - i ] ) {
	    errno = EINVAL;
	    return -1;
	}

    /* Check for both players on the bar against closed boards */
    for( i = 0; i < 6; i++ )
	if( anBoard[ 0 ][ i ] || anBoard[ 1 ][ i ] )
	    return 0;

    if( !anBoard[ 0 ][ 24 ] || !anBoard[ 1 ][ 24 ] )
	return 0;
    
    errno = EINVAL;
    return -1;
}

extern int PositionFromKey( int anBoard[ 2 ][ 25 ],
			     unsigned char *puch ) {

    int i, j, iBit = 0;
    unsigned char *puchInit = puch;

    for( i = 0; i < 2; i++ )
	for( j = 0; j < 25; j++ ) {
	    anBoard[ i ][ j ] = 0;
	    
	    while( puch < puchInit + 10 && ReadBit( &puch, &iBit ) )
		anBoard[ i ][ j ]++;
	}

    return CheckPosition( anBoard );
}

static int Base64( char ch ) {

    if( ch >= 'A' && ch <= 'Z' )
	return ch - 'A';

    if( ch >= 'a' && ch <= 'z' )
	return ch - 'a' + 26;

    if( ch >= '0' && ch <= '9' )
	return ch - '0' + 52;

    if( ch == '+' )
	return 62;

    return 63;
}

extern int PositionFromID( int anBoard[ 2 ][ 25 ], char *pchEnc ) {

    unsigned char auchKey[ 10 ], ach[ 15 ], *pch = ach, *puch = auchKey;
    int i;

    for( i = 0; i < 14 && pchEnc[ i ]; i++ )
	pch[ i ] = Base64( pchEnc[ i ] );

    pch[ i ] = 0;
    
    for( i = 0; i < 3; i++ ) {
	*puch++ = ( pch[ 0 ] << 2 ) | ( pch[ 1 ] >> 4 );
	*puch++ = ( pch[ 1 ] << 4 ) | ( pch[ 2 ] >> 2 );
	*puch++ = ( pch[ 2 ] << 6 ) | pch[ 3 ];

	pch += 4;
    }

    *puch = ( pch[ 0 ] << 2 ) | ( pch[ 1 ] >> 4 );

    PositionFromKey( anBoard, auchKey );

    return CheckPosition( anBoard );
}

extern int EqualKeys( unsigned char auch0[ 10 ], unsigned char auch1[ 10 ] ) {

    int i;

    for( i = 0; i < 10; i++ )
	if( auch0[ i ] != auch1[ i ] )
	    return 0;
    
    return 1;
}

extern int EqualBoards( int anBoard0[ 2 ][ 25 ], int anBoard1[ 2 ][ 25 ] ) {

    int i;

    for( i = 0; i < 25; i++ )
	if( anBoard0[ 0 ][ i ] != anBoard1[ 0 ][ i ] ||
	    anBoard0[ 1 ][ i ] != anBoard1[ 1 ][ i ] )
	    return 0;

    return 1;
}

static int anCombination[ 21 ][ 6 ], fCalculated = 0;

static int InitCombination( void ) {

    int i, j;

    for( i = 0; i < 21; i++ )
	anCombination[ i ][ 0 ] = i + 1;
    
    for( j = 1; j < 6; j++ )
	anCombination[ 0 ][ j ] = 0;

    for( i = 1; i < 21; i++ )
	for( j = 1; j < 6; j++ )
	    anCombination[ i ][ j ] = anCombination[ i - 1 ][ j - 1 ] +
		anCombination[ i - 1 ][ j ];

    fCalculated = 1;
    
    return 0;
}

static int Combination( int n, int r ) {

    assert( n > 0 );
    assert( r > 0 );
    assert( n <= 21 );
    assert( r <= 6 );

    if( !fCalculated )
	InitCombination();
    
    return anCombination[ n - 1 ][ r - 1 ];
}

static int PositionF( int fBits, int n, int r ) {

    if( n == r )
	return 0;

    return ( fBits & ( 1 << ( n - 1 ) ) ) ? Combination( n - 1, r ) +
	PositionF( fBits, n - 1, r - 1 ) : PositionF( fBits, n - 1, r );
}

extern unsigned short PositionBearoff( int anBoard[ 6 ] ) {

    int i, fBits, j;

    for( j = 5, i = 0; i < 6; i++ )
	j += anBoard[ i ];

    fBits = 1 << j;
    
    for( i = 0; i < 6; i++ ) {
	j -= anBoard[ i ] + 1;
	fBits |= ( 1 << j );
    }

    return PositionF( fBits, 21, 6 );
}

static int PositionInv( int nID, int n, int r ) {

    int nC;

    if( !r )
	return 0;
    else if( n == r )
	return ( 1 << n ) - 1;

    nC = Combination( n - 1, r );

    return ( nID >= nC ) ? ( 1 << ( n - 1 ) ) |
	PositionInv( nID - nC, n - 1, r - 1 ) : PositionInv( nID, n - 1, r );
}

extern void PositionFromBearoff( int anBoard[ 6 ], unsigned short usID ) {
    
    int fBits = PositionInv( usID, 21, 6 );
    int i, j;

    for( i = 0; i < 6; i++ )
	anBoard[ i ] = 0;
    
    for( j = 5, i = 0; j >= 0 && i < 21; i++ ) {
	if( fBits & ( 1 << i ) )
	    j--;
	else
	    anBoard[ j ]++;
    }
}
