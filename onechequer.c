/*
 * onechequer.c
 *
 * by Joern Thyssen, <jthyssen@dk.ibm.com>
 *
 * based on code published by Douglas Zare on bug-gnubg@gnu.org
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

#ifdef STANDALONE
#include <stdio.h>
#endif
#include <math.h>
#include <glib.h>

#include "onechequer.h"
#include "bearoff.h"


static void 
AverageRolls ( const float arProb[ 32 ], float *ar ) {

  float sx;
  float sx2;
  int i;

  sx = sx2 = 0.0f;

  for ( i = 0; i < 32; ++i ) {
    sx += ( i + 1 ) * arProb[ i ];
    sx2 += ( i + 1 ) * ( i + 1  ) * arProb[ i ];
  }


  ar[ 0 ] = sx;
  ar[ 1 ] = (float)sqrt ( sx2 - sx *sx );

}


static float
getDist( const int nPips, const int nRoll, float *table ) {

  float r;
  int i;
  const int aai[ 13 ][ 2 ] = {
     { 2,  3 },
     { 3,  4 },
     { 4,  5 },
     { 4,  6 },
     { 6,  7 },
     { 5,  8 },
     { 4,  9 },
     { 2, 10 },
     { 2, 11 },
     { 1, 12 },
     { 1, 16 },
     { 1, 20 },
     { 1, 24 }
   };

  if ( nPips <= 0 )
    return 0.0f;

  if ( table[ nPips * 32 + nRoll ] >= 0.0f ) 
    /* cache hit */
    return table[ nPips * 32 + nRoll ];

  /* calculate value */

  if ( !nRoll ) {

    /*
     * The chance of bearing the chequer off in 1 roll with x pips
     * to go is simply the probability of rolling more than x pips.
     *
     * f(y) = aai[ i ][ 0 ] / 36, where i is found where y = aai[ i ][ 1 ]
     *
     * P(x,1) = \sum_{y=x}^{y=24} f(y)
     *
     */

    r = 0.0f;
    for ( i = 0; i < 13; ++i )
      if ( nPips <= aai[ i ][ 1 ] )
        r += ( 1.0f * aai[ i ][ 0 ] ) / 36.0f;
    return ( table[ nPips * 32 + nRoll ] = r );
  }
  else {

    /*
     * Use recursion formulae as the distribution for n rolls
     * can be calculated from n-1 rolls, i.e.,
     *
     * P(x,k+1) = \sum_{y=3}^{y=24} f(y) P( x - y, k )
     *
     */

    r = 0.0f;
    for ( i = 0; i < 13; ++i )
      r += ( 1.0f * aai[ i ][ 0 ] ) / 36.0f * 
        getDist( nPips - aai[ i ][ 1 ], nRoll - 1, table );

    return ( table[ nPips * 32 + nRoll ] = r );
  }

}

static void
DistFromPipCount( const int nPips, float arDist[ 32 ], float *table ) {

  int i;

  for ( i = 0; i < 32; ++i ) 
    arDist[ i ] = getDist( nPips, i, table );

}


/*
 * The usual formulae for calculating gwc:
 *
 * x_A = pip count for player on roll (anPips[1])
 * x_B = pip count for opponent (anPips[0])
 *
 * p = \sum_{i=1}^{i=\infty} 
 *          \left( P(x_A,i) \cdot \sum_{j=i}^{j=\infty} P(x_B,j) \right)
 *
 */

extern float
GWCFromDist( const float arDist0[], const float arDist1[], const int n ) {

  int i, j;
  float r = 0.0f;

  for ( i = 0; i < n; ++i )
    for ( j = i; j < n; ++j )
      r += arDist0[ i ] * arDist1[ j ];

  return r;

}


/*
 * Calculate the cubeless GWC based on pip counts
 *
 * Player 1 is on roll
 *
 */

extern float
GWCFromPipCount( const int anPips[ 2 ], float *arMu, float *arSigma ) {

  float *table;
  float aarDist[ 2 ][ 32 ];
  int i;
  int j;
  float r = 0.0f;
  int nMaxPips;
  float ar[ 2 ];

  /* initialise table */

  nMaxPips = anPips[ 0 ] > anPips[ 1 ] ? anPips[ 0 ] : anPips[ 1 ];

  table = (float *) malloc( ( nMaxPips + 1 ) * 32 * sizeof( float ) );

  for ( i = 0; i < nMaxPips + 1; ++i )
    for ( j = 0; j < 32; ++j )
      table[ i * 32 + j ] = -1.0f;

  /* Calculate distributions.
   * 
   * Use formulae from Zadeh and Kobliska, "On optimal doubling in
   * backgammon", Management Science, 853-858, vol 23, 1977.
   *
   */

  for ( i = 0; i < 2; ++i ) {
    DistFromPipCount( anPips[ i ], aarDist[ i ], table );
  }

  r = GWCFromDist( aarDist[ 1 ], aarDist[ 0 ], 32 );
  
  /* garbage collect */

  free( table );

  /* calculate mu and sigma */

  for ( i = 0; i < 2; ++i ) {
    AverageRolls( aarDist[ i ], ar );
    if ( arMu )
      arMu[ i ] = ar[ 0 ];
    if ( arSigma )
      arSigma[ i ] = ar[ 1 ];
  }

  return r;

}


extern float
GWCFromMuSigma( const float arMu[ 2 ], const float arSigma[ 2 ] ) {

  float aarDist[ 2 ][ 32 ];
  int i, j;

  /* calculate distribution */

  for ( i = 0; i < 2; ++i ) 
    for ( j = 0; j < 32; ++j )
      aarDist[ i ][ j ] = fnd ( 1.0f * j, arMu[ i ], arSigma[ i ] );
  
  return GWCFromDist( aarDist[ 1 ], aarDist[ 0 ], 32 );

}
