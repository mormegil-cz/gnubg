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

#include "onechequer.h"
#include "bearoff.h"


static float 
onecheck ( const int npips, float *prSigma, float arMu[], float arSigma[] ) {

  float rSigma, rMean, r;

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
  int i;

  if ( npips <= 0 ) {
     *prSigma = 0.0f;
     return 0.0f;
  }
  else if ( arMu[ npips ] >= 0.0f ) {
     *prSigma = arSigma[ npips ];
     return arMu[ npips ];
  }
  else

    rMean = *prSigma = 0.0f;

    for ( i = 0; i < 13; ++i ) {

       r = 1.0f + onecheck ( npips - aai[ i ][ 1 ], &rSigma, arMu, arSigma );

       rMean += aai[ i ][ 0 ] * r;
       *prSigma += aai[ i ][ 0 ] * ( rSigma * rSigma + r * r );


    }

    arMu[ npips ] = rMean /= 36.0f;
    arSigma[ npips ] = *prSigma = sqrt ( *prSigma / 36.0f - rMean * rMean );
    
    return rMean;

}


extern int
OneChequer ( const int nPips, float *prMu, float *prSigma ) {

   float *arMu, *arSigma;
   int i;

   if ( ! ( arMu = (float *) malloc ( ( nPips + 1 ) * sizeof ( float ) ) ) )
      return -1;
   if ( ! ( arSigma = (float *) malloc ( ( nPips + 1 ) * sizeof ( float ) ) ) )
      return -1;

   for ( i = 0; i < nPips + 1; ++i )
      arMu[ i ] = arSigma[ i ] = -1.0f;

   *prMu = onecheck ( nPips, prSigma, arMu, arSigma );

   free ( arMu );
   free ( arSigma );

   return 0;

}


#ifdef STANDALONE


extern int
main ( int argc, char **argv ) {

   int npips;
   int i;
   float rSigma, rMu;

   if ( argc != 2 ) {
      printf ( "Usage: %s number-of-pips\n", argv[ 0 ] );
      return 1;
   }

   npips =  atoi ( argv[ 1 ] );

   printf ( "Average number of rolls to bearoff %d pips\n", npips );   

   OneChequer ( npips, &rMu, &rSigma );

   printf ( "%7.4f\n", rMu );
   printf ( "%7.4f\n", rSigma );

   return 0;

}
#endif
