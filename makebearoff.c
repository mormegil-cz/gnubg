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
 * $Id$
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "eval.h"
#include "positionid.h"
#include "getopt.h"

static unsigned int ausRolls[ 3268760];
static unsigned short aaProb[ 3268760 ][ 32 ];
static double aaEquity[ 924 ][ 924 ];

static void BearOff( int nId, int nPoint ) {

    int i, iBest, iMode, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ],
	anBoardTemp[ 2 ][ 25 ], aProb[ 32 ];
    movelist ml;
    unsigned int us;

    PositionFromBearoff( anBoard[ 1 ], nId, nPoint );

    for( i = 0; i < 32; i++ )
	aProb[ i ] = 0;
    
    for( i = nPoint; i < 25; i++ )
	anBoard[ 1 ][ i ] = 0;

    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = 0;

    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    us = 0xFFFF; iBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], nPoint );

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

    PositionFromBearoff( anBoard[ 0 ], nThem, 6 );
    PositionFromBearoff( anBoard[ 1 ], nUs, 6 );

    for( i = 6; i < 25; i++ )
	anBoard[ 1 ][ i ] = anBoard[ 0 ][ i ] = 0;

    rTotal = 0.0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    r = -1.0; iBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], 6 );

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

static void
generate ( const int nTSP, const int nTSC, 
           const int nOS, const int fMagicBytes ) {

    int i, j, k;
    int n;
#ifdef STDOUT_FILENO 
    FILE *output;

    if( !( output = fdopen( STDOUT_FILENO, "wb" ) ) ) {
	perror( "(stdout)" );
	return EXIT_FAILURE;
    }
#else
#define output stdout
#endif

    /* one sided database */

    if ( nOS ) {

      aaProb[ 0 ][ 0 ] = 0xFFFF;
      for( i = 1; i < 32; i++ )
	aaProb[ 0 ][ i ] = 0;

      ausRolls[ 0 ] = 0;

      n = Combination ( nOS + 15, nOS );

      for( i = 1; i < n; i++ ) {
	BearOff( i, nOS );

	if( !( i % 100 ) )
          fprintf( stderr, "1:%d/%d        \r", i, n );
      }

      if ( fMagicBytes ) {
        char sz[ 21 ];
        sprintf ( sz, "%02dxxxxxxxxxxxxxxxxxxx", nOS );
        puts ( sz );
      }

      for( i = 0; i < n; i++ )
	for( j = 0; j < 32; j++ ) {
          putc( aaProb[ i ][ j ] & 0xFF, output );
          putc( aaProb[ i ][ j ] >> 8, output );
	}

    }

    if ( nTSP && nTSC ) {

      /* two-sided database */

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
	    
          putc( k & 0xFF, output );
          putc( k >> 8, output );
	}
    }    
}


static void
usage ( char *arg0 ) {

  printf ( "Usage: %s [options]\n"
           "Options:\n"
           "  -t, --two-sided PxC Number of points and number of chequers\n"
           "                      for two-sided database\n"
           "  -o, --one-sided P   Number of points for one-sided database\n"
           "  -m, --magic-bytes   Write magic bytes\n"
           "  -v, --version       Show version information and exit\n"
           "  -h, --help          Display usage and exit\n"
           "\n"
           "To generate gnubg.bd:\n"
           "%s -t 6x6 -o 6 > gnubg.bd\n"
           "\n"
           "To generate gnubg_huge_os.bd:\n"
           "%s -t 0x0 -o 10 -m > gnubg_huge_os.bd\n"
           "\n",
           arg0, arg0, arg0 );

}

static void
version ( void ) {

  printf ( "makebearoff $Revision$\n" );

}


extern int main( int argc, char **argv ) {

  int nTSP = 6, nTSC = 6;
  int nOS = 6;
  int fMagicBytes = FALSE;
  char ch;

  static struct option ao[] = {
    { "two-sided", required_argument, NULL, 't' },
    { "one-sided", required_argument, NULL, 'o' },
    { "magic-bytes", no_argument, NULL, 'm' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };

  while ( ( ch = getopt_long ( argc, argv, "t:o:mhv", ao, NULL ) ) !=
          (char) -1 ) {
    switch ( ch ) {
    case 't': /* size of two-sided */
      sscanf ( optarg, "%dx%d", &nTSP, &nTSC );
      break;
    case 'o': /* size of one-sided */
      nOS = atoi ( optarg );
      break;
    case 'm': /* magic bytes */
      fMagicBytes = TRUE;
      break;
    case 'h': /* help */
      usage ( argv[ 0 ] );
      exit ( 0 );
      break;
    case 'v': /* version */
      version ();
      exit ( 0 );
      break;
    default:
      usage ( argv[ 0 ] );
      exit ( 1 );
    }
  }

  if ( nOS > 18 ) {
    fprintf ( stderr, 
              "Size of one-sided bearoff database must be between "
              "0 and 18\n" );
    exit ( 2 );
  }

  if ( nOS )
    fprintf ( stderr, 
              "One-sided database:\n"
              "Number of points   : %12d\n"
              "Number of positions: %'12d\n"
              "Size of file       : %'12d bytes\n",
              nOS, 
              Combination ( nOS + 15, nOS ),
              Combination ( nOS + 15, nOS ) * 64 );
  else
    fprintf ( stderr, 
              "No one-sided database created.\n" );

  if ( nTSC && nTSP ) 
    fprintf ( stderr,
              "Two-sided database:\n"
              "Number of points   : %12d\n"
              "Number of chequers : %12d\n"
              "Number of positions: %'12d\n"
              "Size of file       : %'12d\n bytes",
              nTSP, nTSC, -1, -1 );
  else
    fprintf ( stderr, 
              "No two-sided database created.\n" );
      
  generate ( nTSP, nTSC, nOS, fMagicBytes );

  return 0;

}

