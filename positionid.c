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
 * $Id$
 */

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "positionid.h"

static
#if defined( __GNUC__ )
inline
#endif
void
addBits(unsigned char auchKey[10], int bitPos, int nBits)
{
  int const k = bitPos / 8;
  int const r = (bitPos & 0x7);

  unsigned int b = (((unsigned int)0x1 << nBits) - 1) << r;

  auchKey[k] |= b;

  if( k < 8 ) {
    auchKey[k+1] |= b >> 8;
    auchKey[k+2] |= b >> 16;
  } else if( k == 8 ) {
    auchKey[k+1] |= b >> 8;
  }
}

extern void
PositionKey(int anBoard[2][25], unsigned char auchKey[10])
{
  int i, iBit = 0;
  const int* j;

  memset(auchKey, 0, 10 * sizeof(*auchKey));

  for(i = 0; i < 2; ++i) {
    const int* const b = anBoard[i];
    for(j = b; j < b + 25; ++j) {
      int const nc = *j;

      if( nc ) {
        addBits(auchKey, iBit, nc);
        iBit += nc + 1;
      } else {
        ++iBit;
      }
    }
  }
}

extern char *PositionIDFromKey( unsigned char auchKey[ 10 ] ) {

    unsigned char *puch = auchKey;
    /*static*/ char szID[ 15 ];		/* olivier: no need for static here */
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

extern int CheckPosition( int anBoard[ 2 ][ 25 ] ) {

    int ac[ 2 ], i;

    /* Check for a point with a negative number of chequers */
    for( i = 0; i < 25; i++ )
	if( anBoard[ 0 ][ i ] < 0 ||
	    anBoard[ 1 ][ i ] < 0 ) {
            errno = EINVAL;
            return -1;
	}
    
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
        if( anBoard[ 0 ][ i ] < 2 || anBoard[ 1 ][ i ] < 2 )
            return 0;

    if( !anBoard[ 0 ][ 24 ] || !anBoard[ 1 ][ 24 ] )
        return 0;
    
    errno = EINVAL;
    return -1;
}

extern void ClosestLegalPosition( int anBoard[ 2 ][ 25 ] ) {

    int i, j, ac[ 2 ] = { 15, 15 };

    /* Force a non-negative number of chequers on all points */
    for( i = 0; i < 2; i++ )
	for( j = 0; j < 25; j++ )
	    if( anBoard[ i ][ j ] < 0 )
		anBoard[ i ][ j ] = 0;

    /* Limit each player to 15 chequers */
    for( i = 0; i < 2; i++ )
	for( j = 0; j < 25; j++ )
	    if( ( ac[ i ] -= anBoard[ i ][ j ] ) < 0 ) {
		anBoard[ i ][ j ] += ac[ i ];
		ac[ i ] = 0;
	    }

    /* Forbid both players having a chequer on the same point */
    for( i = 0; i < 24; i++ )
	if( anBoard[ 0 ][ i ] )
	    anBoard[ 1 ][ 23 - i ] = 0;

    /* If both players have closed boards, let at least one of them off
       the bar */
    for( i = 0; i < 6; i++ )
        if( anBoard[ 0 ][ i ] < 2 || anBoard[ 1 ][ i ] < 2 )
	    /* open board */
            return;

    if( anBoard[ 0 ][ 24 ] )
	anBoard[ 1 ][ 24 ] = 0;
}

extern void
PositionFromKey(int anBoard[2][25], unsigned char* pauch)
{
  int i = 0, j  = 0, k;
  unsigned char* a;

  memset(anBoard[0], 0, sizeof(anBoard[0]));
  memset(anBoard[1], 0, sizeof(anBoard[1]));
  
  for(a = pauch; a < pauch + 10; ++a) {
    unsigned char cur = *a;
    
    for(k = 0; k < 8; ++k) {
      if( (cur & 0x1) ) {
	++anBoard[i][j];
      } else {
	if( ++j == 25 ) {
	  ++i;
	  j = 0;
	}
      }
      cur >>= 1;
    }
  }
}

extern int
Base64( const char ch ) {

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

extern int PositionFromID( int anBoard[ 2 ][ 25 ], const char *pchEnc ) {

  unsigned char auchKey[ 10 ], ach[ 15 ], *pch = ach, *puch = auchKey;
  int i;

  memset ( ach, 0, 15 );

  for( i = 0; i < 14 && pchEnc[ i ]; i++ )
    pch[ i ] = Base64( pchEnc[ i ] );

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

extern int 
EqualKeys( const unsigned char auch0[ 10 ], const unsigned char auch1[ 10 ] ) {

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

#define MAX_N 40
#define MAX_R 25

static int anCombination[ MAX_N ][ MAX_R ], fCalculated = 0;

static int InitCombination( void ) {

    int i, j;

    for( i = 0; i < MAX_N; i++ )
        anCombination[ i ][ 0 ] = i + 1;
    
    for( j = 1; j < MAX_R; j++ )
        anCombination[ 0 ][ j ] = 0;

    for( i = 1; i < MAX_N; i++ )
        for( j = 1; j < MAX_R; j++ )
            anCombination[ i ][ j ] = anCombination[ i - 1 ][ j - 1 ] +
                anCombination[ i - 1 ][ j ];

    fCalculated = 1;
    
    return 0;
}

extern int Combination( const int n, const int r ) {

    assert( n > 0 );
    assert( r > 0 );
    assert( n <= MAX_N );
    assert( r <= MAX_R );

    if( !fCalculated )
        InitCombination();

    return anCombination[ n - 1 ][ r - 1 ];
}

static int PositionF( const int fBits, const int n, const int r ) {

    if( n == r )
        return 0;

    return ( fBits & ( 1 << ( n - 1 ) ) ) ? Combination( n - 1, r ) +
        PositionF( fBits, n - 1, r - 1 ) : PositionF( fBits, n - 1, r );
}

extern 
unsigned int PositionBearoff( const int anBoard[],
                              const int nPoints,
                              const int nChequers ) {

    int i, fBits, j;

    for( j = nPoints - 1, i = 0; i < nPoints; i++ )
        j += anBoard[ i ];

    fBits = 1 << j;
    
    for( i = 0; i < nPoints; i++ ) {
        j -= anBoard[ i ] + 1;
        fBits |= ( 1 << j );

    }

    return PositionF( fBits, nChequers + nPoints, nPoints );
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

extern void PositionFromBearoff( int anBoard[], const unsigned int usID,
                                 const int nPoints, const int nChequers ) {
    
    int fBits = PositionInv( usID, nChequers + nPoints, nPoints );
    int i, j;

    for( i = 0; i < nPoints; i++ )
        anBoard[ i ] = 0;
    
    for( j = nPoints - 1, i = 0; j >= 0 && i < ( nChequers + nPoints ); i++ ) {
        if( fBits & ( 1 << i ) )
            j--;
        else
            anBoard[ j ]++;
    }
}

extern unsigned short
PositionIndex(int g, int anBoard[6])
{
  int i, fBits;
  int j = g - 1;

  for(i = 0; i < g; i++ )
    j += anBoard[ i ];

  fBits = 1 << j;
    
  for(i = 0; i < g; i++) {
    j -= anBoard[ i ] + 1;
    fBits |= ( 1 << j );
  }

  /* FIXME: 15 should be replaced by nChequers, but the function is
     only called from bearoffgammon, so this should be fine. */
  return PositionF( fBits, 15, g );
}
