/*
 * makehyper.c
 *
 * by Jørn Thyssen <jth@gnubg.org>, 2003
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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "eval.h"
#include "positionid.h"
#include "getopt.h"
#include "i18n.h"
#include "bearoff.h"
#include "drawboard.h"

static cubeinfo ci;
static cubeinfo ciJacoby;

typedef enum _hyperclass {
  HYPER_OVER,
  HYPER_BEAROFF,
  HYPER_CONTACT,
  HYPER_ILLEGAL

} hyperclass;

enum _xxx {
  EQUITY_CUBELESS = 0,
  EQUITY_OWNED = 1,
  EQUITY_CENTER = 2,
  EQUITY_CENTER_JACOBY = 3,
  EQUITY_OPPONENT = 4
};

typedef struct _hyperequity {

  float arOutput[ NUM_OUTPUTS ];
  float arEquity[ 5 ];

} hyperequity;

int aiNorm[ 10 ];


static hyperclass
ClassifyHyper ( int anBoard[ 2 ][ 25 ] ) {

  int i;
  int nOppBack, nBack;

  /* check if both players has chequers on the same point
     (exclusive the bar) */

  for ( i = 0; i < 24; ++i )
    if ( anBoard[ 0 ][ i ] && anBoard[ 1 ][ 23 - i ] )
      return HYPER_ILLEGAL;

  /* which chequer is furthest back */

  for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
    if( anBoard[0][nOppBack] ) {
      break;
    }
  }
  
  for(nBack = 24; nBack >= 0; --nBack) {
    if( anBoard[1][nBack] ) {
      break;
    }
  }
  
  if( nBack < 0 || nOppBack < 0 )
    return HYPER_OVER;

  if ( nBack + nOppBack > 22 )
    return HYPER_CONTACT;

  return HYPER_BEAROFF;

}

static void
HyperOver ( int anBoard[ 2 ][ 25 ], float ar[ NUM_OUTPUTS ], const int nC ) {

  EvalOver( anBoard, ar, VARIATION_HYPERGAMMON_1 + nC - 1 );

}


static void
StartGuessHyper ( hyperequity ahe[], const int nC, bearoffcontext *pbc ) {

  int i, j, k;
  int nPos = Combination( 25 + nC, nC );
  int anBoard[ 2 ][ 25 ];

  int ai[ 4 ] = { 0,0,0,0 };

  for ( i = 0; i < nPos; ++i ) {
    for ( j = 0; j < nPos; ++j ) {

      PositionFromBearoff ( anBoard[ 0 ], j, 25, nC );
      PositionFromBearoff ( anBoard[ 1 ], i, 25, nC );

      switch ( ClassifyHyper ( anBoard ) ) {
      case HYPER_OVER:
        HyperOver ( anBoard, ahe[ i * nPos + j ].arOutput, nC );

        for ( k = 0; k < 5; ++k )
          ahe[ i * nPos + j ].arEquity[ k ] = 
            Utility ( ahe[ i * nPos + j ].arOutput, &ci );

        /* special calc for Jacoby rule */
        
        ahe[ i * nPos + j ].arEquity[ EQUITY_CENTER_JACOBY ] = 
          Utility ( ahe[ i * nPos + j ].arOutput, &ciJacoby );

        ++ai[0];

        break;

      case HYPER_CONTACT:
        ++ai[ 1 ];
        --ai[2];
      case HYPER_BEAROFF:

        memset ( &ahe[ i * nPos + j  ], 0, sizeof ( hyperequity ) );

        ahe[ i * nPos + j ].arOutput[ 0 ] = 1.0;
        ahe[ i * nPos + j ].arOutput[ 1 ] = 1.0;
        ahe[ i * nPos + j ].arOutput[ 2 ] = 0.0;
        ahe[ i * nPos + j ].arOutput[ 3 ] = 0.0;
        ahe[ i * nPos + j ].arOutput[ 4 ] = 0.0;
        ahe[ i * nPos + j ].arEquity[ 0 ] = 
          Utility ( ahe[ i * nPos + j ].arOutput, &ci );

        ++ai[2];

        break;

      case HYPER_ILLEGAL:

        memset ( &ahe[ i * nPos + j  ], 0, sizeof ( hyperequity ) );
        ++ai[3];

        break;

      default:

        assert ( FALSE );
        break;

      }

    }
  }

  printf( _("Number of game-over positions:   %10d\n"
            "Number of non-contact positions: %10d\n"
            "Number of contact positions:     %10d\n"
            "Total number of legal positions: %10d\n"
            "Number of illegal positions:     %10d\n"
            "Total number of positions in file%10d\n"),
          ai[ 0 ], ai[ 2 ], ai[ 1 ], 
          ai[ 0 ] + ai[ 2 ] + ai[ 1 ], 
          ai[ 3 ],
          nPos * nPos );
            
}


static void
StartFromDatabase( hyperequity ahe[], const int nC, 
                   const char *szFilename ) {

  FILE *pf;
  int nPos = Combination( 25 + nC, nC );
  unsigned char ac[ 28 ];
  unsigned int us;
  int i, j, k;
  float r;

  if ( ! ( pf = fopen ( szFilename, "r+b" ) ) ) {
    perror ( szFilename );
    exit(2);
  }

  /* skip header */

  fseek( pf, 40, SEEK_SET );

  for ( i = 0; i < nPos; ++i )
    for ( j = 0; j < nPos; ++j ) {

      fread( ac, 1, 28, pf );

      for ( k = 0; k < NUM_OUTPUTS; ++k ) {
        us = ac[ 3 * k ] | ( ac[ 3 * k + 1 ] ) << 8 | ( ac[ 3 * k + 2 ] ) << 16;
        r = us / 16777215.0;
        assert( r >= 0 && r <= 1 );
        ahe[ i * nPos + j ].arOutput[ k ] = r;
      }

      for ( k = 0; k < 4; ++k ) {
        us = ac[ 15 + 3 * k ] | ( ac[ 15 + 3 * k + 1 ] ) << 8 |  
           ( ac[ 15 + 3 * k + 2 ] ) << 16;
        r = ( us / 16777215.0f - 0.5f ) * 6.0f;
        assert( r >= -3 && r <= 3 );
        ahe[ i * nPos + j ].arEquity[ k + 1 ] = r;
      }

      ahe[ i * nPos + j ].arEquity[ 0 ] = 
        Utility ( ahe[ i * nPos  + j ].arOutput, &ci );

    }
  
                         

  fclose( pf );

}



/*
 * Calculate oo-norm the difference of two vectors.
 *
 */

static float
NormOO ( const float ar[], const int n ) {

  int i;
  float r = 0;

  for ( i = 0; i < n; ++i )
     if ( r < ar[ i ] )
        r = ar[ i ];

  return r;

}


static float
CubeEquity ( const float rND, const float rDT, const float rDP ) {

  if ( ( 2.0 * rDT ) >= rND && rDP >= rND ) {
    /* it's a double */

    if ( ( 2.0 * rDT ) >= rDP )
      /* double, pass */
      return rDP;
    else
      /* double, take */
      return 2.0 * rDT;

  }
  else

    /* no double */

    return rND;

}


static void
HyperEquity ( const int nUs, const int nThem,
              hyperequity *phe, const int nC, const hyperequity aheOld[],
              float arNorm[] ) {

  int anBoard[ 2 ][ 25 ];
  int anBoardTemp[ 2 ][ 25 ];
  movelist ml;
  int i, j, k;
  int nUsNew, nThemNew;
  hyperequity heBest;
  hyperequity heOld;
  hyperequity heNew;
  int nPos = Combination ( 25 + nC, nC );
  const hyperequity *phex;
  float r;
  hyperclass hc;

  /* save old hyper equity */

  memcpy ( &heOld, phe, sizeof ( hyperequity ) );

  /* generate board for position */
  
  PositionFromBearoff ( anBoard[ 0 ], nThem, 25, nC );
  PositionFromBearoff ( anBoard[ 1 ], nUs, 25, nC );

  switch ( ( hc = ClassifyHyper ( anBoard ) ) ) {
  case HYPER_OVER:

    HyperOver ( anBoard, phe->arOutput, nC );

    for ( k = 0; k < 5; ++k )
      phe->arEquity[ k ] = Utility ( phe->arOutput, &ci );

    /* special calc for Jacoby rule */

    phe->arEquity[ EQUITY_CENTER_JACOBY ] = 
      Utility ( phe->arOutput, &ciJacoby );

    return;
    break;

  case HYPER_ILLEGAL:

    return;
    break;

  case HYPER_BEAROFF:
  case HYPER_CONTACT:

    for ( k = 0; k < NUM_OUTPUTS; ++k )
      heNew.arOutput[ k ] = 0.0f;
    for ( k = 0; k < 5; ++k )
      heNew.arEquity[ k ] = 0.0f;

    for ( i = 1; i <= 6; ++i ) 
      for ( j = 1; j <= i; ++j ) {
      
        GenerateMoves ( &ml, anBoard, i, j, FALSE );

        if ( ml.cMoves ) {

          /* at least one legal move: find the equity of the best move */

          for ( k = 0; k < 5; ++k )
            heBest.arEquity[ k ] = -10000.0f;
        
          for ( k = 0; k < ml.cMoves; ++k ) {
          
            PositionFromKey ( anBoardTemp, ml.amMoves[ k ].auch );
          
            nUsNew = PositionBearoff( anBoardTemp[ 1 ], 25, nC );
            nThemNew = PositionBearoff( anBoardTemp[ 0 ], 25, nC );
          
            assert ( nUsNew >= 0 );
            assert ( nUsNew < nUs );
            assert ( nThemNew >= 0 );

            /* cubeless */

            phex = &aheOld[ nPos * nThemNew + nUsNew ];

            r = - phex->arEquity[ EQUITY_CUBELESS ];
 
            if ( r >= heBest.arEquity[ EQUITY_CUBELESS ] ) {
              memcpy ( heBest.arOutput, phex->arOutput, 
                       NUM_OUTPUTS * sizeof ( float ) );
              InvertEvaluation ( heBest.arOutput );
              heBest.arEquity[ EQUITY_CUBELESS ] = r;
            }

            /* I own cube:
               from opponent's view he doesn't own cube */

            r = - phex->arEquity[ EQUITY_OPPONENT ];
          
            if ( r >= heBest.arEquity[ EQUITY_OWNED ] )
              heBest.arEquity[ EQUITY_OWNED ] = r;

            /* Centered cube, i.e., centered for opponent too */

            r = - CubeEquity ( phex->arEquity[ EQUITY_CENTER ],
                               phex->arEquity[ EQUITY_OPPONENT ],
                               +1.0 );
            
            if ( r >= heBest.arEquity[ EQUITY_CENTER ] )
              heBest.arEquity[ EQUITY_CENTER ] = r;

            /* centered cube with Jacoby rule */

            r = - CubeEquity ( phex->arEquity[ EQUITY_CENTER_JACOBY ],
                               phex->arEquity[ EQUITY_OPPONENT ],
                               +1.0 );
            
            if ( r >= heBest.arEquity[ EQUITY_CENTER_JACOBY ] )
              heBest.arEquity[ EQUITY_CENTER_JACOBY ] = r;

            /* Opponent owns cube: from opponent's view he owns cube */

            r = - CubeEquity ( phex->arEquity[ EQUITY_OWNED ],
                               phex->arEquity[ EQUITY_OPPONENT ],
                               +1.0 );

            if ( r >= heBest.arEquity[ EQUITY_OPPONENT ] )
              heBest.arEquity[ EQUITY_OPPONENT ] = r;

          }

        }
        else {

          /* no legal moves: equity is minus the equity of the reverse
             position, which has the opponent on roll */
          
          memcpy( &heBest, &aheOld[ nPos * nThem  + nUs ], 
                  sizeof ( hyperequity ) );

          InvertEvaluation ( heBest.arOutput );
          
          for ( k = 0; k < 5; ++k )
            heBest.arEquity[ k ] = - heBest.arEquity[ k ];

        }

        /* sum up equities */

        for ( k = 0; k < NUM_OUTPUTS; ++k )
          heNew.arOutput[ k ] += 
            ( i == j ) ? heBest.arOutput[ k ] : 2.0 * heBest.arOutput[ k ];
        for ( k = 0; k < 5; ++k )
          heNew.arEquity[ k ] += 
            ( i == j ) ? heBest.arEquity[ k ] : 2.0 * heBest.arEquity[ k ];

      }
  
    /* normalise */

    for ( k = 0; k < NUM_OUTPUTS; ++k )
      phe->arOutput[ k ] = heNew.arOutput[ k ] / 36.0;
    for ( k = 0; k < 5; ++k )
      phe->arEquity[ k ] = heNew.arEquity[ k ] / 36.0;

    break;

  }

  /* calculate contribution to norm */

  for ( k = 0; k < NUM_OUTPUTS; ++k ) {
    r = fabsf ( phe->arOutput[ k ] - heOld.arOutput[ k ] );
    if ( r > arNorm[ k ] ) {
       arNorm[ k ] = r;
       aiNorm[ k ] = nUs * nPos + nThem;
    }
  }
  for ( k = 0; k < 5; ++k ) {
    r = fabsf( phe->arEquity[ k ] - heOld.arEquity[ k ] );
    if ( r > arNorm[ 5 + k ] ) {
       arNorm[ 5 + k ] = r;
       aiNorm[ 5 + k ] = nUs * nPos + nThem;
    }
  }


}


static void
CalcNewEquity ( hyperequity ahe[], const int nC, float arNorm[] ) {

  int i, j;
  int nPos = Combination( 25 + nC, nC );

  for ( i = 0; i < 10; ++i )
    arNorm[ i ] = 0.0f;

  for ( i = 0; i < nPos; ++i ) {

    printf ( "\r%d/%d              ", i, nPos );
    fflush ( stdout );

    for ( j = 0; j < nPos; ++j ) {

      HyperEquity ( i, j, &ahe[ i * nPos + j ], nC, ahe, arNorm );

    }

  }

  printf ( "\n" );

}

static void
WriteEquity ( FILE *pf, const float r ) {

  unsigned int us;
  us = ( r / 6.0f + 0.5f ) * 0xFFFFFF;

  putc( us & 0xFF, pf );
  putc( (us >> 8) & 0xFF, pf );
  putc( (us >> 16) & 0xFF, pf );


}

static void
WriteProb ( FILE *pf, const float r ) {

  unsigned int us;

  us = r * 0xFFFFFF;

  putc( us & 0xFF, pf );
  putc( (us >> 8) & 0xFF, pf );
  putc( (us >> 16) & 0xFF, pf );

}


static void
WriteFile ( const char *szFilename, const hyperequity ahe[], 
            const int nC ) {

  int nPos = Combination( 25 + nC, nC );
  int i, j, k;
  char sz[ 41 ];
  FILE *pf;

  
  if ( ! ( pf = fopen ( szFilename, "w+b" ) ) ) {
    perror ( szFilename );
    return;
  }

  sprintf ( sz, "gnubg-H%dxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n", nC );
  fputs ( sz, pf );


  for ( i = 0; i < nPos; ++i ) 
    for ( j = 0; j < nPos; ++j ) {
      for ( k = 0; k < NUM_OUTPUTS; ++k )
        WriteProb ( pf, ahe[ i * nPos + j ].arOutput[ k ] );
      for ( k = 1; k < 5; ++k )
        WriteEquity ( pf, ahe[ i * nPos + j ].arEquity[ k ] );
      putc(0,pf); /* padding */

    }

  fclose ( pf );
  
}

static void
version ( void ) {

  printf ( "$id\n" );

}

static void
usage( const char *arg0 ) {

  printf ( _("Usage: %s [options] -f filename\n"
             "Options:\n"
             "  -f, --outfile filename\n"
             "                      Output to file.\n"
             "  -r, --restart filename\n"
             "                      Restart calculation of database from "
             "given file.\n"
             "                      Default start guess is zero-vector.\n"
             "  -c, --chequers number\n"
             "                      The number of chequers. Default is 3 "
             "corresponding to\n"
             "                      normal 3-chequer hypergammon.\n"
             "  -n, --no-checkpoint\n"
             "                      Do not write a checkpoint file after "
             "each iteration.\n"
             "  -t, --threshold     The convergens threshold. Default is "
             "1e-5\n"
             "  -v, --version       Show version information and exit\n"
             "  -h, --help          Display usage and exit\n"),
           arg0 );

}

extern int
main ( int argc, char **argv ) {

  int nC = 3;
  hyperequity *aheEquity;
  int nPos;
  float rNorm;
  float rEpsilon = 1.0e-5;
  bearoffcontext *pbc;
  int it;
  char szFilename[ 20 ];
  float arNorm[ 10 ];
  time_t t0, t1, t2, t3;
  char *szOutput = NULL;
  char *szRestart = NULL;
  char ch;
  int fCheckPoint = TRUE;

  static struct option ao[] = {
    { "chequers", required_argument, NULL, 'c' },
    { "restart", required_argument, NULL, 'r' },
    { "outfile", required_argument, NULL, 'f' },
    { "threshold", required_argument, NULL, 't' },
    { "no-checkpoint", no_argument, NULL, 'n' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };


  /* i18n */

#if HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* parse options */

  while ( ( ch = getopt_long ( argc, argv, "c:r:f:t:nhv", ao, NULL ) ) !=
          (char) -1 ) {
    switch ( ch ) {
    case 'h': /* help */
      usage ( argv[ 0 ] );
      exit ( 0 );
      break;
    case 'v': /* version */
      version ();
      exit ( 0 );
      break;
    case 'f': /* output file */
      szOutput = strdup ( optarg );
      break;
    case 'r': /* restart file */
      szRestart = strdup ( optarg );
      break;
    case 'c': /* number of chequers */
      nC = atoi( optarg );
      break;
    case 'n': /* no check point files */
      fCheckPoint = FALSE;
      break;
    case 't': /* convergense threshold */
      rEpsilon = atof ( optarg );
      break;
    default:
      usage ( argv[ 0 ] );
      exit ( 1 );
    }
  }

  if ( ! szOutput || nC < 1 || nC > 3 ) {
    /* illegal options */
    usage( argv[ 0 ] );
    exit(1);
  }

  /* start calculation */

  time ( &t2 );

  printf ( _("Number of chequers: %d\n"), nC );
  
  nPos = Combination( 25 + nC, nC );

  printf ( _("Total number of one sided positions: %d\n"), nPos );
  printf ( _("Total number of two sided positions: %d\n"), nPos * nPos );
  printf ( _("Estimated size of file             : %d bytes\n"),
           nPos * nPos * 28 + 40 );
  printf ( _("Output file                        : '%s'\n"), szOutput );
  printf ( _("Convergens threshold               : %e\n"), rEpsilon );

  /* Iteration 0 */

  time ( &t0 );

  printf ( _("*** Obtain start guess ***\n") );

  SetCubeInfo ( &ci, 1, -1, 0, 0, NULL, FALSE, FALSE, FALSE, 
                VARIATION_HYPERGAMMON_1 + nC - 1 );
  SetCubeInfo ( &ciJacoby, 1, -1, 0, 0, NULL, FALSE, TRUE, FALSE,
                VARIATION_HYPERGAMMON_1 + nC - 1 );

  aheEquity = 
    (hyperequity *) malloc ( nPos * nPos * sizeof ( hyperequity ) );

  if ( ! szRestart ) {
    printf( _("0-vector start guess\n") );
    StartGuessHyper ( aheEquity, nC, pbc );
  }
  else {
    printf( _("Start from file\n") );
    StartFromDatabase( aheEquity, nC, szRestart );
  }

  time ( &t1 );

  printf ( _("Time for start guess: %d seconds\n"), (int) (t1 - t0) );
  
  it = 0;

  do {

    time ( &t0 );

    printf ( _("*** Iteration %03d *** \n"), it );

    CalcNewEquity ( aheEquity, nC, arNorm );

    /*
    for ( i = 0; i < 10; ++i ) 
    printf( "Norm[%d]=%f (%d)\n", i, arNorm[ i ], aiNorm[ i ] ); */

    rNorm = NormOO ( arNorm, 10 );

    printf ( _("norm of delta: %f\n"), rNorm );

    if ( fCheckPoint ) {

      sprintf ( szFilename, "%s.tmp", szOutput );
      if ( rNorm > rEpsilon )
        WriteFile ( szFilename, aheEquity, nC );
      else
        unlink( szFilename );

    }

    time ( &t1 );
    printf ( _("Time for iteration %03d: %d seconds\n"), it, (int) (t1 - t0) );

    ++it;

  } while ( rNorm > rEpsilon );

  time ( &t0 );

  WriteFile ( szOutput, aheEquity, nC );

  time ( &t1 );

  printf ( _("Time for writing final file: %d seconds\n"), (int) (t1 - t0) );

  time ( &t3 );

  printf ( _("Total time: %d seconds\n"), (int) (t3 - t2) );

  return 0;

}

