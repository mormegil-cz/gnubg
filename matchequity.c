/*
 * matchequity.c
 * by Joern Thyssen, 1999
 *
 * Calculate matchequity table based on 
 * formulas from [insert the Managent Science ref here].
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

#include <stdio.h>
#include "matchequity.h"

/* FIXME: rename A1, A2, and Btilde to something more descriptive */

/*
 * A1 (A2) is the match equity of player 1 (2)
 * Btilde is the post-crawford match equities.
 */

float aafA1 [ MAXSCORE ][ MAXSCORE ];
float aafA2 [ MAXSCORE ][ MAXSCORE ];

float afBtilde [ MAXSCORE ];

met metCurrent = MET_ZADEH;
int nMaxScore = MAXSCORE;

int 
GetCubePrimeValue ( int i, int j, int nCubeValue );

void
InitMatchEquityZadeh ();


void
InitMatchEquity ( met metInit ) {

  metCurrent = metInit;
  nMaxScore = MAXSCORE;

  switch ( metInit ) {
  case MET_ZADEH:
    InitMatchEquityZadeh ();
    break;
  default:
    break;
  }

}

void
InitMatchEquityZadeh () {

  int i,j,k;
  int nCube;
  int nCubeValue, nCubePrimeValue;

  /*
   * D1bar (D2bar) is the equity of player 1 (2) at the drop point
   * of player 2 (1) assuming dead cubes. 
   * D1 (D2) is the equity of player 1 (2) at the drop point
   * of player 2 (1) assuming semiefficient recubes.
   */
  
  float aaafD1 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD2 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD1bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  float aaafD2bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
  
  /*
   * Calculate post-crawford match equities
   */

  for ( i = 0; i < MAXSCORE; i++ ) {

    afBtilde[ i ] = 
      GAMMONRATE * 0.5 * 
      ( (i-4 >=0) ? afBtilde[ i-4 ] : 1.0 )
      + (1.0 - GAMMONRATE) * 0.5 * 
      ( (i-2 >=0) ? afBtilde[ i-2 ] : 1.0 );

    /*
     * add 1.5% at 1-away, 2-away for the free drop
     * add 0.4% at 1-away, 4-away for the free drop
     */

    if ( i == 1 )
      afBtilde[ i ] -= 0.015;

    if ( i == 3 )
      afBtilde[ i ] -= 0.004;

  }

  /*
   * Calculate 1-away,n-away match equities
   */

  for ( i = 0; i < MAXSCORE; i++ ) {

    aafA1[ i ][ 0 ] = 
      GAMMONRATE * 0.5 *
      ( (i-2 >=0) ? afBtilde[ i-2 ] : 1.0 )
      + (1.0 - GAMMONRATE) * 0.5 * 
      ( (i-1 >=0) ? afBtilde[ i-1 ] : 1.0 );
    aafA1[ 0 ][ i ] = 1.0 - aafA1[ i ][ 0 ];

    aafA2[ 0 ][ i ] = 
      GAMMONRATE * 0.5 *
      ( (i-2 >=0) ? afBtilde[ i-2 ] : 1.0 )
      + (1.0 - GAMMONRATE) * 0.5 * 
      ( (i-1 >=0) ? afBtilde[ i-1 ] : 1.0 );
    aafA2[ i ][ 0 ] = 1.0 - aafA2[ 0 ][ i ];

  }

  for ( i = 0; i < MAXSCORE ; i++ ) {
    
    for ( j = 0; j <= i ; j++ ) {
      
      for ( nCube = MAXCUBELEVEL-1; nCube >=0 ; nCube-- ) {

	nCubeValue = 1;
	for ( k = 0; k < nCube ; k++ )
	  nCubeValue *= 2;

				/* Calculate D1bar */
	
	nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );

	aaafD1bar[ i ][ j ][ nCube ] = 
	  ( GET_A1 ( i - nCubeValue, j, aafA1 )
	    - G2 * GET_A1 ( i, j - 4 * nCubePrimeValue, aafA1 )
	    - (1.0-G2) * GET_A1 ( i, j - 2 * nCubePrimeValue, aafA1 ) )
	  /
	  ( G1 * GET_A1( i - 4 * nCubePrimeValue, j, aafA1 )
	    + (1.0-G1) * GET_A1 ( i - 2 * nCubePrimeValue, j, aafA1 )
	    - G2 * GET_A1 ( i, j - 4 * nCubePrimeValue, aafA1 )
	    - (1.0-G2) * GET_A1 ( i, j - 2 * nCubePrimeValue, aafA1 ) );



	if ( i != j ) {

	  nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );

	  aaafD1bar[ j ][ i ][ nCube ] = 
	    ( GET_A1 ( j - nCubeValue, i, aafA1 )
	      - G2 * GET_A1 ( j, i - 4 * nCubePrimeValue, aafA1 )
	      - (1.0-G2) * GET_A1 ( j, i - 2 * nCubePrimeValue, aafA1 ) )
	    /
	    ( G1 * GET_A1( j - 4 * nCubePrimeValue, i, aafA1 )
	      + (1.0-G1) * GET_A1 ( j - 2 * nCubePrimeValue, i, aafA1 )
	      - G2 * GET_A1 ( j, i - 4 * nCubePrimeValue, aafA1 )
	      - (1.0-G2) * GET_A1 ( j, i - 2 * nCubePrimeValue, aafA1 ) );


	  
	}

				/* Calculate D2bar */

	nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );
	
	aaafD2bar[ i ][ j ][ nCube ] = 
	  ( GET_A2 ( i, j - nCubeValue, aafA2 )
	    - G2 * GET_A2 ( i - 4 * nCubePrimeValue, j, aafA2 )
	    - (1.0-G2) * GET_A2 ( i - 2 * nCubePrimeValue, j, aafA2 ) )
	  /
	  ( G1 * GET_A2( i, j - 4 * nCubePrimeValue, aafA2 )
	    + (1.0-G1) * GET_A2 ( i, j - 2 * nCubePrimeValue, aafA2 )
	    - G2 * GET_A2 ( i - 4 * nCubePrimeValue, j, aafA2 )
	    - (1.0-G2) * GET_A2 ( i - 2 * nCubePrimeValue, j, aafA2 ) );


	if ( i != j ) {

	  nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );
	  
	  aaafD2bar[ j ][ i ][ nCube ] = 
	    ( GET_A2 ( j, i - nCubeValue, aafA2 )
	      - G2 * GET_A2 ( j - 4 * nCubePrimeValue, i, aafA2 )
	      - (1.0-G2) * GET_A2 ( j - 2 * nCubePrimeValue, i, aafA2 ) )
	    /
	    ( G1 * GET_A2( j, i - 4 * nCubePrimeValue, aafA2 )
	      + (1.0-G1) * GET_A2 ( j, i - 2 * nCubePrimeValue, aafA2 )
	      - G2 * GET_A2 ( j - 4 * nCubePrimeValue, i, aafA2 )
	      - (1.0-G2) * GET_A2 ( j - 2 * nCubePrimeValue, i, aafA2 ) );
	  


	}

				/* Calculate D1 */

	if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

	  aaafD1[ i ][ j ][ nCube ] = aaafD1bar[ i ][ j ][ nCube ];
	  if ( i != j )
	    aaafD1[ j ][ i ][ nCube ] = aaafD1bar[ j ][ i ][ nCube ];

	} 
	else {
	  
	  aaafD1[ i ][ j ][ nCube ] = 
	    1.0 + 
	    ( aaafD2[ i ][ j ][ nCube+1 ] + DELTA )
	    * ( aaafD1bar[ i ][ j ][ nCube ] - 1.0 );
	  if ( i != j )
	    aaafD1[ j ][ i ][ nCube ] = 
	      1.0 + 
	      ( aaafD2[ j ][ i ][ nCube+1 ] + DELTA )
	      * ( aaafD1bar[ j ][ i ][ nCube ] - 1.0 );
	  
	}


				/* Calculate D2 */

	if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

	  aaafD2[ i ][ j ][ nCube ] = aaafD2bar[ i ][ j ][ nCube ];
	  if ( i != j )
	    aaafD2[ j ][ i ][ nCube ] = aaafD2bar[ j ][ i ][ nCube ];

	} 
	else {
	  
	  aaafD2[ i ][ j ][ nCube ] = 
	    1.0 + 
	    ( aaafD1[ i ][ j ][ nCube+1 ] + DELTA )
	    * ( aaafD2bar[ i ][ j ][ nCube ] - 1.0 );
	
	  if ( i != j ) 
	    aaafD2[ j ][ i ][ nCube ] = 
	      1.0 + 
	      ( aaafD1[ j ][ i ][ nCube+1 ] + DELTA )
	      * ( aaafD2bar[ j ][ i ][ nCube ] - 1.0 );
	}


				/* Calculate A1 & A2 */

	if ( (nCube == 0) && ( i > 0 ) && ( j > 0 ) ) {

	  aafA1[ i ][ j ] = 
	    ( 
	     ( aaafD2[ i ][ j ][ 0 ] + DELTABAR - 0.5 )
	     * GET_A1( i - 1, j, aafA1 ) 
	     + ( aaafD1[ i ][ j ][ 0 ] + DELTABAR - 0.5 )
	     * GET_A1( i, j - 1, aafA1 ) )
	    /
	    (
	     aaafD1[ i ][ j ][ 0 ] + DELTABAR +
	     aaafD2[ i ][ j ][ 0 ] + DELTABAR - 1.0 );


	  if ( i != j )
	    aafA1[ j ][ i ] = 1.0 - aafA1[ i ][ j ];


	  aafA2[ i ][ j ] =
	    (
	     ( aaafD2[ i ][ j ][ 0 ] + DELTABAR - 0.5 )
	     * GET_A2( i - 1, j, aafA2 )
	     + ( aaafD1[ i ][ j ][ 0 ] + DELTABAR - 0.5 )
	     * GET_A2( i, j - 1, aafA2 ) )
	    /
	    ( 
	     aaafD1[ i ][ j ][ 0 ] + DELTABAR +
	     aaafD2[ i ][ j ][ 0 ] + DELTABAR - 1.0 );

	  if ( i != j )
	    aafA2[ j ][ i ] = 1.0 - aafA2[ i ][ j ];
	}
	
      }
      
    }
    
  }
  
}


int 
GetCubePrimeValue ( int i, int j, int nCubeValue ) {

  if ( (i < 2 * nCubeValue) && (j >= 2 * nCubeValue ) )

    /* automatic double */

    return 2*nCubeValue;
  else
    return nCubeValue;

}


extern int
GetPoints ( float arOutput [ 5 ], int   anScore[ 2 ], int nMatchTo,
	    cubeinfo *pci, float arCP[ 2 ] ) {

  /*

   * Input:
   * - arOutput: we need the gammon and backgammon ratios
   *   (we assume arOutput is evaluate for pci -> fMove)
   * - anScore: the current score.
   * - nMatchTo: matchlength
   * - pci: value of cube, who's turn is it
   * 
   *
   * Output:
   * - arCP : cash points with live cube
   * These points are necesary for the linear
   * interpolation used in cubeless -> cubeful equity 
   * transformation.
   */

  /* Match play */

  /* normalize score */

  int i = nMatchTo - anScore[ 0 ] - 1;
  int j = nMatchTo - anScore[ 1 ] - 1;



  int nCube = pci -> nCube;

  float arCPLive [ 2 ][ MAXCUBELEVEL ];
  float arCPDead [ 2 ][ MAXCUBELEVEL ];
  float rG0, rG1, rBG0, rBG1;

  int nDead, n, nMax, nCubeValue, nCubePrimeValue;

  /* Gammon and backgammon ratio's. 
     Avoid division by zero in extreme cases. */

  if ( ! pci -> fMove ) {

    /* arOutput evaluated for player 0 */

    if ( arOutput[ 0 ] > 0.0 ) {
      rG0 = arOutput[ 1 ] / arOutput[ 0 ];
      rBG0 = arOutput[ 2 ] / arOutput[ 0 ];
    }
    else {
      rG0 = 0.0;
      rBG0 = 0.0;
    }

    if ( arOutput[ 0 ] < 1.0 ) {
      rG1 = arOutput[ 3 ] / ( 1.0 - arOutput[ 0 ] );
      rBG1 = arOutput[ 4 ] / ( 1.0 - arOutput[ 0 ] );
    }
    else {
      rG1 = 0.0;
      rBG1 = 0.0;
    }
  }
  else {

    /* arOutput evaluated for player 1 */

    if ( arOutput[ 0 ] > 0.0 ) {
      rG1 = arOutput[ 1 ] / arOutput[ 0 ];
      rBG1 = arOutput[ 2 ] / arOutput[ 0 ];
    }
    else {
      rG1 = 0.0;
      rBG1 = 0.0;
    }

    if ( arOutput[ 0 ] < 1.0 ) {
      rG0 = arOutput[ 3 ] / ( 1.0 - arOutput[ 0 ] );
      rBG0 = arOutput[ 4 ] / ( 1.0 - arOutput[ 0 ] );
    }
    else {
      rG0 = 0.0;
      rBG0 = 0.0;
    }
  }

  /* Find out what value the cube has when you or your
     opponent give a dead cube. */

  nDead = nCube;
  nMax = 0;

  while ( ( i >= 2 * nDead ) && ( j >= 2 * nDead ) ) {
    nMax ++;
    nDead *= 2;
  }

  for ( nCubeValue = nDead, n = nMax; 
	nCubeValue >= nCube; 
	nCubeValue >>= 1, n-- ) {

    /* Calculate dead and live cube cash points.
       See notes by me (Joern Thyssen) available from the
       'doc' directory.  (FIXME: write notes :-) ) */

    /* Even though it's a dead cube we take account of the opponents
       automatic redouble. */

    nCubePrimeValue = GetCubePrimeValue ( i, j, nCubeValue );

    /* Dead cube cash point for player 0 */
    
    arCPDead[ 0 ][ n ] = 
      ( ( 1.0 - rG1 - rBG1 ) * 
       GET_A1 ( i, j - 2 * nCubePrimeValue, aafA1 ) 
       + rG1 * GET_A1 ( i, j - 4 * nCubePrimeValue, aafA1 ) 
       + rBG1 * GET_A1 ( i, j - 6 * nCubePrimeValue, aafA1 ) 
       - GET_A1 ( i - nCubeValue, j, aafA1 ) )
     /
      ( ( 1.0 - rG1 - rBG1 ) * 
       GET_A1 ( i, j - 2 * nCubePrimeValue, aafA1 ) 
       + rG1 * GET_A1 ( i, j - 4 * nCubePrimeValue, aafA1 ) 
       + rBG1 * GET_A1 ( i, j - 6 * nCubePrimeValue, aafA1 ) 
	- ( 1.0 - rG0 - rBG0 ) *
	GET_A1( i - 2 * nCubePrimeValue, j, aafA1 )
	- rG0 * GET_A1 ( i - 4 * nCubePrimeValue, j, aafA1 )
	- rBG0 * GET_A1 ( i - 6 * nCubePrimeValue, j, aafA1 ) );

    /* Dead cube cash point for player 1 */
    
    nCubePrimeValue = GetCubePrimeValue ( j, i, nCubeValue );

    arCPDead[ 1 ][ n ] = 
      ( ( 1.0 - rG0 - rBG0 ) * 
       GET_A1 ( j, i - 2 * nCubePrimeValue, aafA1 ) 
       + rG0 * GET_A1 ( j, i - 4 * nCubePrimeValue, aafA1 ) 
       + rBG0 * GET_A1 ( j, i - 6 * nCubePrimeValue, aafA1 ) 
       - GET_A1 ( j - nCubeValue, i, aafA1 ) )
     /
      ( ( 1.0 - rG0 - rBG0 ) * 
       GET_A1 ( j, i - 2 * nCubePrimeValue, aafA1 ) 
       + rG0 * GET_A1 ( j, i - 4 * nCubePrimeValue, aafA1 ) 
       + rBG0 * GET_A1 ( j, i - 6 * nCubePrimeValue, aafA1 ) 
	- ( 1.0 - rG1 - rBG1 ) *
	GET_A1( j - 2 * nCubePrimeValue, i, aafA1 )
	- rG1 * GET_A1 ( j - 4 * nCubePrimeValue, i, aafA1 )
	- rBG1 * GET_A1 ( j - 6 * nCubePrimeValue, i, aafA1 ) );

    /* Live cube cash point for player 0 and 1 */

    if ( ( i < 2 * nCubeValue ) || ( j < 2 * nCubeValue ) ) {

      /* The doubled cube is going to be dead */

      arCPLive[ 0 ][ n ] = arCPDead[ 0 ][ n ];
      arCPLive[ 1 ][ n ] = arCPDead[ 1 ][ n ];

    } 
    else {

      /* Doubled cube is alive */

      arCPLive[ 0 ][ n ] = 1.0 - arCPLive[ 1 ][ n + 1] *
	( GET_A1 ( i - nCubeValue, j, aafA1 )   
	  - ( 1.0 - rG0 - rBG0 ) *
	  GET_A1( i - 2 * nCubeValue, j, aafA1 )
	  - rG0 * GET_A1 ( i - 4 * nCubeValue, j, aafA1 )
	  - rBG0 * GET_A1 ( i - 6 * nCubeValue, j, aafA1 ) )
	/
	( GET_A1 ( i, j - 2 * nCubeValue, aafA1 )
	  - ( 1.0 - rG0 - rBG0 ) *
	  GET_A1( i - 2 * nCubeValue, j, aafA1 )
	  - rG0 * GET_A1 ( i - 4 * nCubeValue, j, aafA1 )
	  - rBG0 * GET_A1 ( i - 6 * nCubeValue, j, aafA1 ) );

      arCPLive[ 1 ][ n ] = 1.0 - arCPLive[ 0 ][ n + 1 ] *
	( GET_A1 ( j - nCubeValue, i, aafA1 )   
	  - ( 1.0 - rG1 - rBG1 ) *
	  GET_A1( j - 2 * nCubeValue, i, aafA1 )
	  - rG1 * GET_A1 ( j - 4 * nCubeValue, i, aafA1 )
	  - rBG1 * GET_A1 ( j - 6 * nCubeValue, i, aafA1 ) )
	/
	( GET_A1 ( j, i - 2 * nCubeValue, aafA1 )
	  - ( 1.0 - rG1 - rBG1 ) *
	  GET_A1( j - 2 * nCubeValue, i, aafA1 )
	  - rG1 * GET_A1 ( j - 4 * nCubeValue, i, aafA1 )
	  - rBG1 * GET_A1 ( j - 6 * nCubeValue, i, aafA1 ) );

    }

  }

  /* return cash point for current cube level */

  arCP[ 0 ] = arCPLive[ 0 ][ 0 ];
  arCP[ 1 ] = arCPLive[ 1 ][ 0 ];

  /* debug output 
  for ( n = nMax; n >= 0; n-- ) {
    
    printf ("Cube %i\n"
	    "Dead cube:    cash point 0 %6.3f\n"
	    "              cash point 1 %6.3f\n"
	    "Live cube:    cash point 0 %6.3f\n"
	    "              cash point 1 %6.3f\n\n", 
	    n,
	    arCPDead[ 0 ][ n ], arCPDead[ 1 ][ n ],
	    arCPLive[ 0 ][ n ], arCPLive[ 1 ][ n ] );
    
  }
  */

  return 0;
  
}



extern float
GetDoublePointDeadCube ( float arOutput [ 5 ],
                         int   anScore[ 2 ], int nMatchTo,
                         cubeinfo *pci ) {

  /*
   * Calculate double point for dead cubes
   */

  if ( ! nMatchTo ) {

    /* Use Rick Janowski's formulas
       [insert proper reference here] */

    float rW, rL;

    if ( arOutput[ 0 ] > 0.0 )
      rW = 1.0 + ( arOutput[ 1 ] + arOutput[ 2 ] ) / arOutput[ 0 ];
    else
      rW = 1.0;

    if ( arOutput[ 0 ] < 1.0 )
      rL = 1.0 + ( arOutput[ 3 ] + arOutput[ 4 ] ) /
        ( 1.0 - arOutput [ 0 ] );
    else
      rL = 1.0;

    if ( pci->fCubeOwner == -1 && fJacoby) {

      /* centered cube */

      if ( fBeavers ) {
        return ( rL - 0.25 ) / ( rW + rL - 0.5 );
      }
      else {
        return ( rL - 0.5 ) / ( rW + rL - 1.0 );
      }
    }
    else {

      /* redoubles or Jacoby rule not in effect */

      return rL / ( rL + rW );
    }

  }
  else {

    /* Match play */

    /* normalize score */

    int i = nMatchTo - anScore[ pci->fMove ] - 1;
    int j = nMatchTo - anScore[ ! pci->fMove ] - 1;
    int nCube = pci->nCube;

    /* FIXME: avoid division by zero */
    float rG1 = arOutput[ 1 ] / arOutput[ 0 ];
    float rBG1 = arOutput[ 2 ] / arOutput[ 0 ];
    float rG2 = arOutput[ 3 ] / ( 1.0 - arOutput[ 0 ] );
    float rBG2 = arOutput[ 4 ] / ( 1.0 - arOutput[ 0 ] );

    /* double point */

    /* FIXME: use the BG ratio */
    /* match equity for double, take; win */
    float rDTW = (1.0 - rG1) * GET_A1 ( i - 2 * nCube, j, aafA1 ) +
      rG1 * GET_A1 ( i - 4 * nCube, j, aafA1 );

    /* match equity for no double; win */
    float rNDW = (1.0 - rG1) * GET_A1 ( i - nCube, j, aafA1 ) +
      rG1 * GET_A1 ( i - 2 * nCube, j, aafA1 );

    /* match equity for double, take; loose */
    float rDTL = (1.0 - rG2) * GET_A1 ( i, j - 2 * nCube, aafA1 ) +
      rG2 * GET_A1 ( i, j - 4 * nCube, aafA1 );

    /* match equity for double, take; loose */
    float rNDL = (1.0 - rG2) * GET_A1 ( i, j - nCube, aafA1 ) +
      rG2 * GET_A1 ( i, j - 2 * nCube, aafA1 );

    /* risk & gain */

    float rRisk = rNDL - rDTL;
    float rGain = rDTW - rNDW;

    return rRisk / ( rRisk +  rGain );

  }

}
