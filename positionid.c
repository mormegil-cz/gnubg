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


extern int CheckPosition( int anBoard[ 2 ][ 25 ] ) {

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
        if( anBoard[ 0 ][ i ] < 2 || anBoard[ 1 ][ i ] < 2 )
            return 0;

    if( !anBoard[ 0 ][ 24 ] || !anBoard[ 1 ][ 24 ] )
        return 0;
    
    errno = EINVAL;
    return -1;
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

/*
 * Calculate log2 of Cube value.
 *
 * Input:
 *   n: cube value
 *
 * Returns:
 *   log(n)/log(2)
 *
 */

extern int 
LogCube( const int n ) {

    int i;

    for( i = 0; ; i++ )
	if( n <= ( 1 << i ) )
	    return i;
}


static void
SetBit ( unsigned char *pc, int bitPos, int iBit ) {

  const int k = bitPos / 8;
  const int r = bitPos % 8;

  unsigned char c,d;

  c = ( iBit << r );
  d = 0xFF ^ ( 0x1 << r );
  pc [ k ] = ( pc[ k ] & d ) | c;

}

static void
SetBits ( unsigned char *pc, int bitPos, int nBits, int iContent ) {

  int i, j;

  /* FIXME: rewrite SetBit, SetBits to be faster */


  for ( i = 0, j = bitPos; i < nBits; i++, j++ ) {

    SetBit ( pc, j, ( ( 0x1 << i ) & iContent ) != 0 );
    
  }

}


static int
GetBits ( const unsigned char *pc, const int bitPos, 
          const int nBits, int *piContent ) {


  int i, j;
  int k, r;

  unsigned char c[2];

  /* FIXME: rewrite GetBits to be faster */

  c[ 0 ] = 0;
  c[ 1 ] = 0;

  for ( i = 0, j = bitPos; i < nBits; i++, j++ ) {

    k = j / 8;
    r = j % 8;

    SetBit ( c, i, ( pc [ k ] & ( 0x1 << r ) ) != 0 );

  }

  *piContent = c[ 0 ] | ( c[ 1 ] << 8 );

  return 0;
}


extern char 
*MatchIDFromKey( unsigned char auchKey[ 9 ] ) {

    unsigned char *puch = auchKey;
    static char szID[ 12 ];
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

    *pch = 0;

    return szID;
}


extern char*
MatchID ( const int anDice[ 2 ],
          const int fTurn,
          const int fResigned,
          const int fDoubled,
          const int fMove,
          const int fCubeOwner,
          const int fCrawford,
          const int nMatchTo,
          const int anScore[ 2 ],
          const int nCube, 
          const int gs ) {

  unsigned char auchKey[ 9 ];

  memset ( auchKey, 0, 9 );

  SetBits ( auchKey, 0, 4, LogCube ( nCube ) );
  SetBits ( auchKey, 4, 2, fCubeOwner & 0x3 );
  SetBits ( auchKey, 6, 1, fMove );
  SetBits ( auchKey, 7, 1, fCrawford );
  SetBits ( auchKey, 8, 3, gs );
  SetBits ( auchKey, 11, 1, fTurn );
  SetBits ( auchKey, 12, 1, fDoubled );
  SetBits ( auchKey, 13, 2, fResigned );
  SetBits ( auchKey, 15, 3, anDice[ 0 ] & 0x7 );
  SetBits ( auchKey, 18, 3, anDice[ 1 ] & 0x7 );
  SetBits ( auchKey, 21, 15, nMatchTo & 0x8FFF );
  SetBits ( auchKey, 36, 15, anScore[ 0 ] & 0x8FFF );
  SetBits ( auchKey, 51, 15, anScore[ 1 ] & 0x8FFF );

  return MatchIDFromKey ( auchKey );


}

extern int
MatchFromKey ( int anDice[ 2 ],
               int *pfTurn,
               int *pfResigned,
               int *pfDoubled,
               int *pfMove,
               int *pfCubeOwner,
               int *pfCrawford,
               int *pnMatchTo,
               int anScore[ 2 ],
               int *pnCube,
               int *pgs,
               const unsigned char *auchKey ) {

  GetBits ( auchKey, 0, 4, pnCube );
  *pnCube = 0x1 << *pnCube;

  GetBits ( auchKey, 4, 2, pfCubeOwner );
  if ( *pfCubeOwner && *pfCubeOwner != 1 )
    *pfCubeOwner = -1;

  GetBits ( auchKey, 6, 1, pfMove );
  GetBits ( auchKey, 7, 1, pfCrawford );
  GetBits ( auchKey, 8, 3, pgs );
  GetBits ( auchKey, 11, 1, pfTurn );
  GetBits ( auchKey, 12, 1, pfDoubled );
  GetBits ( auchKey, 13, 2, pfResigned );
  GetBits ( auchKey, 15, 3, &anDice[ 0 ] );
  GetBits ( auchKey, 18, 3, &anDice[ 1 ] );
  GetBits ( auchKey, 21, 15, pnMatchTo );
  GetBits ( auchKey, 36, 15, &anScore[ 0 ] );
  GetBits ( auchKey, 51, 15, &anScore[ 1 ] );

  /* FIXME: implement a consistency check */

  return 0;

}


extern int
MatchFromID ( int anDice[ 2 ],
              int *pfTurn,
              int *pfResigned,
              int *pfDoubled,
              int *pfMove,
              int *pfCubeOwner,
              int *pfCrawford,
              int *pnMatchTo,
              int anScore[ 2 ],
              int *pnCube,
              int *pgs,
              const char *szMatchID ) {

  unsigned char auchKey[ 9 ];
  unsigned char *puch = auchKey;
  unsigned char ach[ 12 ];
  unsigned char *pch = ach;
  int i;

  /* decode base64 into key */

  for( i = 0; i < 12 && szMatchID[ i ]; i++ )
    pch[ i ] = Base64( szMatchID[ i ] );

  pch[ i ] = 0;

  for( i = 0; i < 3; i++ ) {
    *puch++ = ( pch[ 0 ] << 2 ) | ( pch[ 1 ] >> 4 );
    *puch++ = ( pch[ 1 ] << 4 ) | ( pch[ 2 ] >> 2 );
    *puch++ = ( pch[ 2 ] << 6 ) | pch[ 3 ];

    pch += 4;
  }

  /* get matchstate info from the key */

  return MatchFromKey ( anDice, pfTurn, pfResigned, pfDoubled,
                        pfMove, pfCubeOwner, pfCrawford, pnMatchTo,  
                        anScore, pnCube, pgs, auchKey );

}

