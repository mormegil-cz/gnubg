/*
 * matchequity.c
 * by Joern Thyssen, 1999
 *
 * Calculate matchequity table based on 
 * formulas from xxx.
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


/*
 * A1 (A2) is the match equity of player 1 (2)
 * Btilde is the post-crawford match equities.
 */

float aafA1 [ MAXSCORE ][ MAXSCORE ];
float aafA2 [ MAXSCORE ][ MAXSCORE ];

float afBtilde [ MAXSCORE ];

/*
 * D1 (D2) is the equity of player 1 (2) at the drop point
 * of player 2 (1) assuming only you have access to the cube.
 * D1bar is the same except that both players have access to
 * the cube.
 */

float aaafD1 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
float aaafD2 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
float aaafD1bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
float aaafD2bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];



int 
GetCubePrimeValue ( int i, int j, int nCubeValue );

void
CalcMatchEq () {

  int i,j,k;
  int nCube;
  int nCubeValue, nCubePrimeValue;

  /*
   * Calculate post-crawford match equities
   */

  for ( i = 0; i < MAXSCORE; i++ ) {

    afBtilde[ i ] = 
      GAMMONRATE * 0.5 * 
      ( (i-4 >=0) ? afBtilde[ i-4 ] : 1.0 )
      + (1.0 - GAMMONRATE) * 0.5 * 
      ( (i-2 >=0) ? afBtilde[ i-2 ] : 1.0 );

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

    /*
      printf("A1[%2d][%2d] = %f\n",i,0,aafA1[i][0]);
      printf("A1[%2d][%2d] = %f\n",0,i,aafA1[0][i]);
      printf("A2[%2d][%2d] = %f\n",i,0,aafA2[i][0]);
      printf("A2[%2d][%2d] = %f\n",0,i,aafA2[0][i]);
    */

  }

  for ( i = 0; i < MAXSCORE ; i++ ) {
    
    for ( j = 0; j <= i ; j++ ) {
      

      /*
	printf("-------------------\n");
      */

      for ( nCube = MAXCUBELEVEL-1; nCube >=0 ; nCube-- ) {

	/*
	  printf("\n");
	*/

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

	/*
	  printf("D1bar[%2d][%2d][%2d] = %f\n",
	  i, j, nCube, aaafD1bar[ i ][ j ][ nCube ]);
	*/

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

	  /*
	  printf("D1bar[%2d][%2d][%2d] = %f\n",
		 j, i, nCube, aaafD1bar[ j ][ i ][ nCube ]);
	  */
	  
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

	/*
	printf("D2bar[%2d][%2d][%2d] = %f\n",
	       i, j, nCube, aaafD2bar[ i ][ j ][ nCube ]);
	*/

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
	  
	  /*
	  printf("D2bar[%2d][%2d][%2d] = %f\n",
		 j, i, nCube, aaafD2bar[ j ][ i ][ nCube ]);
	  */

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

	/*
	printf("D1[%2d][%2d][%2d] = %f\n",
	       i, j, nCube, aaafD1[ i ][ j ][ nCube ]);
	if ( i != j )
	  printf("D1[%2d][%2d][%2d] = %f\n",
		 j, i, nCube, aaafD1[ j ][ i ][ nCube ]);
	*/


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

	/*
	printf("D2[%2d][%2d][%2d] = %f\n",
	       i, j, nCube, aaafD2[ i ][ j ][ nCube ]);
	if ( i != j )
	  printf("D2[%2d][%2d][%2d] = %f\n",
		 j, i, nCube, aaafD2[ j ][ i ][ nCube ]);
	*/



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

	  /*
	  printf("A1[%2d][%2d] = %f\n",
		 i, j, aafA1[ i ][ j ]);
	  */

	  if ( i != j ) {

	    aafA1[ j ][ i ] = 1.0 - aafA1[ i ][ j ];

	    /*
	    printf("A1[%2d][%2d] = %f\n",
		   j, i, aafA1[ j ][ i ]);
	    */

	  }


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

	  /*
	    printf("A2[%2d][%2d] = %f\n",
	    i, j, aafA2[ i ][ j ]);
	  */

	     
	    
	  if ( i != j ) {

	    aafA2[ j ][ i ] = 1.0 - aafA2[ i ][ j ];

	    /*
	    printf("A2[%2d][%2d] = %f\n",
		   j, i, aafA2[ j ][ i ]);
	    */

	  }
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


#ifdef UNDEF
int main ()

{

  int i,j;

  CalcMatchEq ();

  puts ( "      " );
  for ( j = 0; j < MAXSCORE; j++ )
    printf ( "%9d", j );
  printf ( "\n" );

  for ( i = 0; i < MAXSCORE; i++ ) {

    printf ( "%6d", i );

    for ( j = 0; j < MAXSCORE; j++ )
      printf ( "%9.6f", aafA1[ i ][ j ] );

    printf ( "\n" );
  }


}
#endif
