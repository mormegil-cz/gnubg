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
 * $Id$
 */

#include "config.h"
#include <glib.h>
#include <errno.h>
#include <string.h>
#include "positionid.h"

static inline void addBits(unsigned char auchKey[10], unsigned int bitPos, unsigned int nBits)
{
  unsigned int k = bitPos / 8;
  unsigned int r = (bitPos & 0x7);
  unsigned int b = (((unsigned int)0x1 << nBits) - 1) << r;

  auchKey[k] |= (unsigned char)b;

  if( k < 8 ) {
    auchKey[k+1] |= (unsigned char)(b >> 8);
    auchKey[k+2] |= (unsigned char)(b >> 16);
  } else if( k == 8 ) {
    auchKey[k+1] |= (unsigned char)(b >> 8);
  }
}

extern void
PositionKey(const TanBoard anBoard, unsigned char auchKey[10])
{
  unsigned int i, iBit = 0;
  const unsigned int* j;

  memset(auchKey, 0, 10 * sizeof(*auchKey));

  for(i = 0; i < 2; ++i) {
    const unsigned int* const b = anBoard[i];
    for(j = b; j < b + 25; ++j)
	{
      const unsigned int nc = *j;

      if( nc ) {
        addBits(auchKey, iBit, nc);
        iBit += nc + 1;
      } else {
        ++iBit;
      }
    }
  }
}

extern char *PositionIDFromKey( const unsigned char auchKey[ 10 ] ) {

    const unsigned char *puch = auchKey;
    static char szID[ L_POSITIONID + 1 ];
    char *pch = szID;
    static char aszBase64[ 65 ] =
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

extern char *PositionID( const TanBoard anBoard ) {

    unsigned char auch[ 10 ];
    
    PositionKey( anBoard, auch );

    return PositionIDFromKey( auch );
}

extern int
CheckPosition( const TanBoard anBoard )
{
    unsigned int ac[ 2 ], i;

    /* Check for a player with over 15 chequers */
    for( i = ac[ 0 ] = ac[ 1 ] = 0; i < 25; i++ )
        if( ( ac[ 0 ] += anBoard[ 0 ][ i ] ) > 15 ||
            ( ac[ 1 ] += anBoard[ 1 ][ i ] ) > 15 ) {
            errno = EINVAL;
            return 0;
        }

    /* Check for both players having chequers on the same point */
    for( i = 0; i < 24; i++ )
        if( anBoard[ 0 ][ i ] && anBoard[ 1 ][ 23 - i ] ) {
            errno = EINVAL;
            return 0;
        }

    /* Check for both players on the bar against closed boards */
    for( i = 0; i < 6; i++ )
        if( anBoard[ 0 ][ i ] < 2 || anBoard[ 1 ][ i ] < 2 )
            return 1;

    if( !anBoard[ 0 ][ 24 ] || !anBoard[ 1 ][ 24 ] )
        return 1;
    
    errno = EINVAL;
    return 0;
}

extern void ClosestLegalPosition( TanBoard anBoard )
{
    unsigned int i, j, ac[ 2 ];

    /* Limit each player to 15 chequers */
    for( i = 0; i < 2; i++ )
	{
		ac[i] = 15;
		for( j = 0; j < 25; j++ )
		{
			if (anBoard[i][j] <= ac[i])
				ac[i] -= anBoard[i][j];
			else
			{
				anBoard[i][j] = ac[i];
				ac[i] = 0;
			}
		}
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
PositionFromKey(TanBoard anBoard, const unsigned char* pauch)
{
  int i = 0, j  = 0, k;
  const unsigned char* a;

  memset(anBoard[0], 0, sizeof(anBoard[0]));
  memset(anBoard[1], 0, sizeof(anBoard[1]));
  
  for(a = pauch; a < pauch + 10; ++a) {
    unsigned char cur = *a;
    
    for(k = 0; k < 8; ++k)
	{
      if( (cur & 0x1) )
	  {
        if (i >= 2 || j >= 25)
        {	/* Error, so return - will probably show error message */
          return;
        }
        ++anBoard[i][j];
      }
	  else
	  {
		if( ++j == 25 )
		{
		  ++i;
		  j = 0;
		}
      }
      cur >>= 1;
    }
  }
}

extern unsigned char Base64( const unsigned char ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - 'A';

    if( ch >= 'a' && ch <= 'z' )
        return (ch - 'a') + 26;

    if( ch >= '0' && ch <= '9' )
        return (ch - '0') + 52;

    if( ch == '+' )
        return 62;

    if( ch == '/' )
        return 63;

    return 255;
}

extern int
PositionFromID(TanBoard anBoard, const char* pchEnc)
{
  unsigned char auchKey[ 10 ], ach[ L_POSITIONID +1 ], *pch = ach, *puch = auchKey;
  int i;

  memset ( ach, 0, L_POSITIONID +1 );

  for( i = 0; i < L_POSITIONID && pchEnc[ i ]; i++ )
    pch[ i ] = Base64( (unsigned char)pchEnc[ i ] );

  for( i = 0; i < 3; i++ ) {
    *puch++ = (unsigned char)( pch[ 0 ] << 2 ) | ( pch[ 1 ] >> 4 );
    *puch++ = (unsigned char)( pch[ 1 ] << 4 ) | ( pch[ 2 ] >> 2 );
    *puch++ = (unsigned char)( pch[ 2 ] << 6 ) | pch[ 3 ];

    pch += 4;
  }

  *puch = (unsigned char)( pch[ 0 ] << 2 ) | ( pch[ 1 ] >> 4 );

  PositionFromKey( anBoard, auchKey );

  return CheckPosition( (ConstTanBoard)anBoard );
}

extern int 
EqualKeys( const unsigned char auch0[ 10 ], const unsigned char auch1[ 10 ] ) {

    int i;

    for( i = 0; i < 10; i++ )
        if( auch0[ i ] != auch1[ i ] )
            return 0;
    
    return 1;
}
 
extern int EqualBoards( const TanBoard anBoard0, const TanBoard anBoard1 ) {

    int i;

    for( i = 0; i < 25; i++ )
        if( anBoard0[ 0 ][ i ] != anBoard1[ 0 ][ i ] ||
            anBoard0[ 1 ][ i ] != anBoard1[ 1 ][ i ] )
            return 0;

    return 1;
}

#define MAX_N 40
#define MAX_R 25

static unsigned int anCombination[ MAX_N ][ MAX_R ], fCalculated = 0;

static void InitCombination( void )
{
    unsigned int i, j;

    for( i = 0; i < MAX_N; i++ )
        anCombination[ i ][ 0 ] = i + 1;
    
    for( j = 1; j < MAX_R; j++ )
        anCombination[ 0 ][ j ] = 0;

    for( i = 1; i < MAX_N; i++ )
        for( j = 1; j < MAX_R; j++ )
            anCombination[ i ][ j ] = anCombination[ i - 1 ][ j - 1 ] +
                anCombination[ i - 1 ][ j ];

    fCalculated = 1;
}

extern unsigned int Combination( const unsigned int n, const unsigned int r )
{
    g_assert( n <= MAX_N && r <= MAX_R );

    if( !fCalculated )
        InitCombination();

    return anCombination[ n - 1 ][ r - 1 ];
}

static unsigned int PositionF( unsigned int fBits, unsigned int n, unsigned int r )
{
    if( n == r )
        return 0;

    return ( fBits & ( 1u << ( n - 1 ) ) ) ? Combination( n - 1, r ) +
        PositionF( fBits, n - 1, r - 1 ) : PositionF( fBits, n - 1, r );
}

extern unsigned int PositionBearoff(const unsigned int anBoard[], unsigned int nPoints, unsigned int nChequers)
{
    unsigned int i, fBits, j;

    for( j = nPoints - 1, i = 0; i < nPoints; i++ )
        j += anBoard[ i ];

    fBits = 1u << j;
    
    for( i = 0; i < nPoints; i++ ) {
        j -= anBoard[ i ] + 1;
        fBits |= ( 1u << j );

    }

    return PositionF( fBits, nChequers + nPoints, nPoints );
}

static unsigned int PositionInv( unsigned int nID, unsigned int n, unsigned int r )
{
    unsigned int nC;

    if( !r )
        return 0;
    else if( n == r )
        return ( 1u << n ) - 1;

    nC = Combination( n - 1, r );

    return ( nID >= nC ) ? ( 1u << ( n - 1 ) ) |
        PositionInv( nID - nC, n - 1, r - 1 ) : PositionInv( nID, n - 1, r );
}

extern void PositionFromBearoff( unsigned int anBoard[], unsigned int usID,
                                 unsigned int nPoints, unsigned int nChequers )
{
    unsigned int fBits = PositionInv( usID, nChequers + nPoints, nPoints );
    unsigned int i, j;

    for( i = 0; i < nPoints; i++ )
        anBoard[ i ] = 0;

    j = nPoints - 1;
    for( i = 0; i < ( nChequers + nPoints ); i++ )
	{
        if( fBits & ( 1u << i ) )
		{
			if (j == 0)
				break;
            j--;
		}
        else
            anBoard[ j ]++;
    }
}

extern unsigned short PositionIndex(unsigned int g, const unsigned int anBoard[6])
{
  unsigned int i, fBits;
  unsigned int j = g - 1;

  for(i = 0; i < g; i++ )
    j += anBoard[ i ];

  fBits = 1u << j;
    
  for(i = 0; i < g; i++)
  {
    j -= anBoard[ i ] + 1;
    fBits |= ( 1u << j );
  }

  /* FIXME: 15 should be replaced by nChequers, but the function is
     only called from bearoffgammon, so this should be fine. */
  return (unsigned short)PositionF( fBits, 15, g );
}
