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
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "eval.h"
#include "positionid.h"
#include "getopt.h"
#include "i18n.h"
#include "bearoff.h"

typedef struct _hashent {
  void *p;
  unsigned int iKey;
} hashent;

typedef struct _hash {
  long int nQueries, nHits, nEntries, nOverwrites;
  int nHashSize;
  hashent *phe;
} hash;


static long int
HashPosition ( hash *ph, const int iKey ) {

  return iKey % ph->nHashSize;

}


static void
HashStatus ( hash *ph ) {

  fprintf ( stderr, "Hash status:\n" );
  fprintf ( stderr, "Size:    %d elements\n", ph->nHashSize );
  fprintf ( stderr, "Queries: %ld (hits: %ld)\n", ph->nQueries, ph->nHits );
  fprintf ( stderr, "Entries: %ld (overwrites: %ld)\n",
            ph->nEntries, ph->nOverwrites );


}


static int
HashCreate ( hash *ph, const int nHashSize ) {

  int i;

  if ( ! ( ph->phe =
           (hashent *) malloc ( nHashSize * sizeof ( hashent ) ) ) ) {
    perror ( "hashtable" );
    return -1;
  }

  ph->nQueries = 0;
  ph->nHits = 0;
  ph->nEntries = 0;
  ph->nOverwrites = 0;
  ph->nHashSize = nHashSize;

  for ( i = 0; i < nHashSize; ++i )
    ph->phe[ i ].p = NULL;

  return 0;

}


static void
HashDestroy ( hash *ph ) {

  int i;

  for ( i = 0; i < ph->nHashSize; ++i )
    if ( ph->phe[ i ].p )
      free ( ph->phe[ i ].p );

}


static void
HashAdd ( hash *ph, const unsigned int iKey,
          const void *data, const int size ) {

  int l = HashPosition ( ph, iKey );

  if ( ph->phe[ l ].p ) {
    /* occupied */
    free ( ph->phe[ l ].p );
    ph->nOverwrites++;
  }
  else {

    /* free */
    ph->nEntries++;

  }

  ph->phe[ l ].iKey = iKey;
  ph->phe[ l ].p = malloc ( size );
  memcpy ( ph->phe[ l ].p, data, size );

}


static void *
HashLookup ( hash *ph, const unsigned int iKey,
             void **data ) {


  int l = HashPosition ( ph, iKey );

  ++ph->nQueries;

  if ( ph->phe[ l ].p && ph->phe[ l ].iKey == iKey ) {
    /* hit */
    ++ph->nHits;
    return (*data = ph->phe[ l].p);
  }
  else
    /* miss */
    return NULL;

}


static void
CalcIndex ( const unsigned short int aProb[ 32 ],
            unsigned int *piIdx, unsigned int *pnNonZero ) {

  int i;
  int j = 32;


  /* find max non-zero element */

  for ( i = 31; i >= 0; --i )
    if ( aProb[ i ] ) {
      j = i;
      break;
    }

  /* find min non-zero element */

  *piIdx = 0;
  for ( i = 0; i <= j; ++i ) 
    if ( aProb[ i ] ) {
      *piIdx = i;
      break;
    }


  *pnNonZero = j - *piIdx + 1;

}


static unsigned int
RollsOS ( const unsigned short int aus[ 32 ] ) {

  int i;
  unsigned int j;

  for( j = 0, i = 1; i < 32; i++ )
    j += i * aus[ i ];

  return j;

}

static void BearOff( int nId, int nPoints, 
                     unsigned short int aOutProb[ 64 ],
                     const int fGammon,
                     hash *ph, bearoffcontext *pbc ) {

    int i, iBest, iMode, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ],
      anBoardTemp[ 2 ][ 25 ], aProb[ 64 ];
    movelist ml;
    int k;
    unsigned int us;
    unsigned int usBest;
    unsigned short int *pusj;
    unsigned short int ausj[ 64 ];
    unsigned short int ausBest[ 64 ];

    /* get board for given position */

    PositionFromBearoff( anBoard[ 1 ], nId, nPoints );

    /* initialise probabilities */

    for( i = 0; i < 64; i++ )
        aProb[ i ] = aOutProb[ i ] = 0;

    /* all chequers off is easy :-) */

    if ( ! nId ) {
      aOutProb[  0 ] = 0xFFFF;
      aOutProb[ 32 ] = 0xFFFF;
      return;
    }

    /* initialise the remainder of the board */
    
    for( i = nPoints; i < 25; i++ )
	anBoard[ 1 ][ i ] = 0;
    
    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = 0;

    for ( i = 0, k = 0; i < nPoints; ++i )
      k += anBoard[ 1 ][ i ];

    /* look for position in existing bearoff file */
    
    if ( pbc ) {
      for ( i = 24; i >= 0 && ! anBoard[ 1 ][ i ]; --i )
        ;
      
      if ( i < pbc->nPoints && k <= pbc->nChequers ) {
        unsigned int nPosID = PositionBearoff ( anBoard[ 1 ], pbc->nPoints );
        BearoffDist ( pbc, nPosID, NULL, NULL, NULL, aOutProb, aOutProb + 32 );

        return;
      }
      
    }
    
    if ( k < 15 )
      aProb[ 32 ] = 0xFFFF * 36;
	    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    usBest = 0xFFFFFFFF; iBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], nPoints );

		assert( j >= 0 );
		assert( j < nId );
		
                if ( ! HashLookup ( ph, j, (void **) &pusj ) ) {
                  BearOff ( j, nPoints, pusj = ausj, fGammon, ph, pbc );
                  HashAdd ( ph, j, pusj, fGammon ? 128 : 64 );
                }

                if ( ( us = RollsOS ( pusj ) ) < usBest ) {
                  iBest = j;
                  usBest = us;
                  memcpy ( ausBest, pusj, fGammon ? 128 : 64 );
                }
                  
	    }

	    assert( iBest >= 0 );

	    if( anRoll[ 0 ] == anRoll[ 1 ] ) {
              for( i = 0; i < 31; i++ ) {
                aProb[ i + 1 ] += ausBest[ i ];
                if ( k == 15 && fGammon )
                  aProb[ 32 + i + 1 ] += ausBest[ 32 + i ];
              }
            }
	    else {
              for( i = 0; i < 31; i++ ) {
                aProb[ i + 1 ] += 2 * ausBest[ i ];
                if ( k == 15 && fGammon )
                  aProb[ 32 + i + 1 ] += 2 * ausBest[ 32 + i ];
              }
            }
	}
    
    for( i = 0, j = 0, iMode = 0; i < 32; i++ ) {
	j += ( aOutProb[ i ] = ( aProb[ i ] + 18 ) / 36 );
	if( aOutProb[ i ] > aOutProb[ iMode ] )
	    iMode = i;
    }

    aOutProb[ iMode ] -= ( j - 0xFFFF );
    
    /* gammon probs */

    if ( fGammon ) {
      for( i = 0, j = 0, iMode = 0; i < 32; i++ ) {
	j += ( aOutProb[ 32 + i ] = ( aProb[ 32 + i ] + 18 ) / 36 );
	if( aOutProb[ 32 + i ] > aOutProb[ 32 + iMode ] )
          iMode = i;
      }
      
      aOutProb[ 32 + iMode ] -= ( j - 0xFFFF );
    }

}


static void
WriteOS ( const unsigned short int aus[ 32 ], 
          const int fCompress, FILE *output ) {

  unsigned int iIdx, nNonZero;
  int j;

  if ( fCompress )
    CalcIndex ( aus, &iIdx, &nNonZero );
  else {
    iIdx = 0;
    nNonZero = 32;
  }

  for( j = iIdx; j < iIdx + nNonZero; j++ ) {
    putc ( aus[ j ] & 0xFF, output );
    putc ( aus[ j ] >> 8, output );
  }

}

static void
WriteIndex ( unsigned int *pnpos,
             const unsigned short int aus[ 64 ],
             const int fGammon, FILE *output ) {

  unsigned int iIdx, nNonZero;

  /* write offset */

  putc ( *pnpos & 0xFF, output );
  putc ( ( *pnpos >> 8 ) & 0xFF, output );
  putc ( ( *pnpos >> 16 ) & 0xFF, output );
  putc ( ( *pnpos >> 24 ) & 0xFF, output );
  
  /* write index and number of non-zero elements */
        
  CalcIndex ( aus, &iIdx, &nNonZero );

  putc ( nNonZero & 0xFF, output );
  putc ( iIdx & 0xFF, output );

  *pnpos += nNonZero;

  /* gammon probs: write index and number of non-zero elements */

  if ( fGammon ) 
    CalcIndex ( aus + 32, &iIdx, &nNonZero );
  else {
    nNonZero = 0;
    iIdx = 0;
  }

  putc ( nNonZero & 0xFF, output );
  putc ( iIdx & 0xFF, output );

  *pnpos += nNonZero;

}

static void
WriteFloat ( const float r, FILE *output ) {

  int j;
  unsigned char *pc;

  pc = (char *) &r;

  for ( j = 0; j < 4; ++j )
    putc ( *(pc++), output );

}



/*
 * Generate one sided bearoff database
 *
 * ! fCompress:
 *   write database directly to stdout
 *
 * fCompress:
 *   write index directly to stdout
 *   and write actual db to tmp file
 *   the tmp file is concatenated to the index when
 *   the generation is done.
 *
 */


static int
generate_os ( const int nOS, const int fHeader,
              const int fCompress, const int fGammon,
              const int nHashSize, bearoffcontext *pbc ) {

  int i;
  int n;
  unsigned short int aus[ 64 ];
  hash h;
  FILE *fTmp;
  time_t t;
  unsigned int npos;
  char szTmp[ 11 ];

#ifdef STDOUT_FILENO 
  FILE *output;

  if( !( output = fdopen( STDOUT_FILENO, "wb" ) ) ) {
    perror( "(stdout)" );
    return EXIT_FAILURE;
  }
#else
#define output stdout
#endif

  /* initialise hash */

  if ( HashCreate ( &h, nHashSize /  ( fGammon ? 128 : 64 ) ) ) {
    fprintf ( stderr, _("Error creating hash with %d elements\n"),
              nHashSize /  fGammon ? 128 : 64 );
    exit(2);
  }

  HashStatus ( &h );

  /* write header */

  if ( fHeader ) {
    char sz[ 41 ];
    sprintf ( sz, "gnubg-OS-%02d-15-%1d-%1d-0xxxxxxxxxxxxxxxxxxx\n", 
              nOS, fGammon, fCompress );
    fputs ( sz, output );
  }

  if ( fCompress ) {
    time ( &t );
    sprintf ( szTmp, "t%06ld.bd", t % 100000 );
    if ( ! ( fTmp = fopen ( szTmp, "w+b" ) ) ) {
      perror ( szTmp );
      exit ( 2 );
    }

  }
    

  /* loop trough bearoff positions */

  n = Combination ( nOS + 15, nOS );
  npos = 0;
  
  for ( i = 0; i < n; ++i ) {
    
    if ( i )
      BearOff ( i, nOS, aus, fGammon, &h, pbc );
    else {
      memset ( aus, 0, 128 );
      aus[  0 ] = 0xFFFF;
      aus[ 32 ] = 0xFFFF;
    }

    if( !( i % 100 ) )
      fprintf( stderr, "1:%d/%d        \r", i, n );

    WriteOS ( aus, fCompress, fCompress ? fTmp : output );
    if ( fGammon )
      WriteOS ( aus + 32, fCompress, fCompress ? fTmp : output );

    HashAdd ( &h, i, aus, fGammon ? 128 : 64 );

    if ( fCompress ) 
      WriteIndex ( &npos, aus, fGammon, output );

  }

  if ( fCompress ) {

    char ac[ 256 ];
    int n;

    /* write contents of fTmp to output */

    rewind ( fTmp );

    while ( ! feof ( fTmp ) && ( n = fread ( ac, 1, sizeof ( ac ), fTmp ) ) )
      fwrite ( ac, 1, n, output );

    /* FIXME */

    fclose ( fTmp );

    unlink ( szTmp );

  }

  putc ( '\n', stderr );
  
  HashStatus ( &h );

  HashDestroy ( &h );

  return 0;

}


static void
NDBearoff ( const int iPos, const int nPoints, float ar[ 4 ], hash *ph,
            bearoffcontext *pbc ) {

  int d0, d1;
  movelist ml;
  int anBoard[ 2 ][ 25 ];
  int anBoardTemp[ 2 ][ 25 ];
  int i, j, k;
  int iBest;
  float rBest;
  float rMean;
  float rVarSum, rGammonVarSum;
  float *prj;
  float arj[ 4 ];
  float arBest[ 4 ];

  for ( i = 0; i < 4; ++i )
    ar[ i ] = 0.0f;

  if ( ! iPos ) 
    return;

  PositionFromBearoff( anBoard[ 1 ], iPos, nPoints );

  for( i = nPoints; i < 25; i++ )
    anBoard[ 1 ][ i ] = 0;

  /* 
   * look for position in existing bearoff file 
   */

  if ( pbc ) {
    for ( i = 24; i >= 0 && ! anBoard[ 1 ][ i ]; --i )
      ;

    if ( i < pbc->nPoints ) {
      unsigned int nPosID = PositionBearoff ( anBoard[ 1 ], pbc->nPoints );
      BearoffDist ( pbc, nPosID, NULL, NULL, ar, NULL, NULL );
      return;
    }

  }

  memset ( anBoard[ 0 ], 0, 25 * sizeof ( int ) );

  for ( i = 0, k = 0; i < nPoints; ++i )
    k += anBoard[ 1 ][ i ];

  /* loop over rolls */

  rVarSum = rGammonVarSum = 0.0f;

  for ( d0 = 1; d0 <= 6; ++d0 ) 
    for ( d1 = 1; d1 <= d0; ++d1 ) {

      GenerateMoves ( &ml, anBoard, d0, d1, FALSE );

      rBest = 1e10;

      for ( i = 0; i < ml.cMoves; ++i ) {

        PositionFromKey ( anBoardTemp, ml.amMoves[ i ].auch );

        j = PositionBearoff ( anBoardTemp[ 1 ], nPoints );

        if ( ! HashLookup ( ph, j, (void **) &prj ) ) {
          NDBearoff ( j, nPoints, prj = arj, ph, pbc );
          HashAdd ( ph, j, prj, 16 );
        }

        if ( prj[ 0 ] < rBest ) {
          iBest = j;
          rBest = prj[ 0 ];
          memcpy ( arBest, prj, 4 * sizeof ( float ) );
        }

      }
      
      rMean = 1.0f + arBest[ 0 ];

      ar[ 0 ] += ( d0 == d1 ) ? rMean : 2.0f * rMean;

      rMean = arBest[ 1 ] * arBest[ 1 ] + rMean * rMean;

      rVarSum += ( d0 == d1 ) ? rMean : 2.0f * rMean;
      
      if ( k == 15 ) {

        rMean = 1.0f + arBest[ 2 ];

        ar[ 2 ] += ( d0 == d1 ) ? rMean : 2.0f * rMean;

        rMean = 
          arBest[ 3 ] * arBest[ 3 ] + rMean * rMean;

        rGammonVarSum += ( d0 == d1 ) ? rMean : 2.0f * rMean;

      }

    }

  
  ar[ 0 ] /= 36.0f;
  ar[ 1 ]  = sqrt ( rVarSum / 36.0f - ar[ 0 ] * ar[ 0 ] );

  ar[ 2 ] /= 36.0f;
  ar[ 3 ]  = 
    sqrt ( rGammonVarSum / 36.0f - ar[ 2 ] * ar[ 2 ] );

}



static void
generate_nd ( const int nPoints,const int nHashSize, const int fHeader,
              bearoffcontext *pbc ) {

  int n = Combination ( nPoints + 15, nPoints );

  int i, j;
  char sz[ 41 ];
  float ar[ 4 ];

  hash h;
 
#ifdef STDOUT_FILENO 
  FILE *output;
    
  if( !( output = fdopen( STDOUT_FILENO, "wb" ) ) ) {
    perror( "(stdout)" );
    return;
  }
#else
#define output stdout
#endif

  if ( HashCreate ( &h, nHashSize / ( 4 * sizeof ( float ) ) ) ) {
    fprintf ( stderr, "Error creating cache\n" );
    return;
  }

  HashStatus ( &h );

  if ( fHeader ) {
    sprintf ( sz, "gnubg-OS-%02d-15-1-0-1xxxxxxxxxxxxxxxxxxx\n", nPoints );
    fputs ( sz, output );
  }

  for ( i = 0; i < n; ++i ) {

    if ( i )
      NDBearoff ( i, nPoints, ar, &h, pbc );
    else
      ar[ 0 ] = ar[ 1 ] = ar[ 2 ] = ar[ 3 ] = 0.0f;

    for ( j = 0; j < 4; ++j )
      WriteFloat ( ar[ j ], output );

    HashAdd ( &h, i, ar, 16 );

    if( !( i % 100 ) )
      fprintf( stderr, "1:%d/%d        \r", i, n );

  }

  putc ( '\n', stderr );

  HashStatus ( &h );

  HashDestroy ( &h );

}


static float
CubeEquity ( const float rND, const float rDT, const float rDP ) {

  if ( rDT >= rND && rDP >= rND ) {
    /* it's a float */
    
    if ( rDT >= rDP )
      /* float, pass */
      return rDP;
    else
      /* float, take */
      return rDT;

  }
  else

    /* no float */

    return rND;

}
  


static void BearOff2( int nUs, int nThem,
                      const int nTSP, const int nTSC,
                      float arEquity[ 4 ], const int n, const int fCubeful,
                      hash *ph, bearoffcontext *pbc ) {

    int i, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ], anBoardTemp[ 2 ][ 25 ];
    movelist ml;
    int aiBest[ 4 ];
    float arBest[ 4 ];
    float arTotal[ 4 ];
    int k;
    float *prj;
    float arj[ 4 ];

    if ( ! nUs ) {

      /* we have won */
      arEquity[ 0 ] = arEquity[ 1 ] = arEquity[ 2 ] = arEquity[ 3 ] = +1.0;
      return;

    }

    if ( ! nThem ) {

      /* we have lost */
      arEquity[ 0 ] = arEquity[ 1 ] = arEquity[ 2 ] = arEquity[ 3 ] = -1.0;
      return;

    }

    PositionFromBearoff( anBoard[ 0 ], nThem, nTSP );
    PositionFromBearoff( anBoard[ 1 ], nUs, nTSP );

    for( i = nTSP; i < 25; i++ )
	anBoard[ 1 ][ i ] = anBoard[ 0 ][ i ] = 0;

    arTotal[ 0 ] = arTotal[ 1 ] = arTotal[ 2 ] = arTotal[ 3 ] = 0.0f;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

            arBest [ 0 ] = arBest[ 1 ] = arBest[ 2 ] = arBest [ 3 ] = -999.0;
            aiBest [ 0 ] = aiBest[ 1 ] = aiBest[ 2 ] = aiBest [ 3 ] = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], nTSP );

		assert( j >= 0 );
		assert( j < nUs );

                if ( ! HashLookup ( ph, n * nThem + j, (void **) &prj ) ) {
                  assert ( FALSE );
                  BearOff2 ( nThem, j, nTSP, nTSC, prj = arj, n,
                             fCubeful, ph, pbc );
                  HashAdd ( ph, n * nThem + j, prj, 
                            sizeof ( float ) * ( fCubeful ? 4 : 1 ) );
                }

                /* cubeless */

                if ( -prj[ 0 ] > arBest[ 0 ] ) { 
                  aiBest [ 0 ] = j;
                  arBest [ 0 ] = -prj[ 0 ];
                }

                if ( fCubeful ) {

                  /* I own cube:
                     from opponent's view he doesn't own cube */
                  
                  if ( -prj[ 3 ] > arBest[ 1 ] ) {
                    aiBest [ 1 ] = j;
                    arBest [ 1 ] = -prj[ 3 ];
                  }

                  /* Centered cube (so centered for opponent too) */

                  if ( -prj[ 2 ] > arBest[ 2 ] ) {
                    aiBest [ 2 ] = j;
                    arBest [ 2 ] = -prj[ 2 ];
                  }

                  /* Opponent owns cube:
                     from opponent's view he owns cube */

                  if ( -prj[ 1 ] > arBest[ 3 ] ) {
                    aiBest [ 3 ] = j;
                    arBest [ 3 ] = -prj[ 1 ];
                  }

                }

	    }

	    assert( aiBest[ 0 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 1 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 2 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 3 ] >= 0 );
	    
            if ( anRoll[ 0 ] == anRoll[ 1 ] )
              for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
                arTotal [ k ] += arBest[ k ];
            else
              for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
                arTotal [ k ] += 2.0f * arBest[ k ];

	}

    /* cubeless equity */
    arEquity[ 0 ] = arTotal[ 0 ] / 36.0;

    if ( fCubeful ) {

      for ( k = 1; k < 4; ++k )
        arTotal[ k ] /= 36.0f;

      /* I own cube */

      arEquity[ 1 ] = 
        CubeEquity ( arTotal[ 1 ], 2.0 * arTotal[ 3 ], 1.0 );

      /* Centered cube */

      arEquity[ 2 ] = 
        CubeEquity ( arTotal[ 2 ], 2.0 * arTotal[ 3 ], 1.0 );

      /* Opp own cube */

      arEquity[ 3 ] = arTotal[ 3 ];

    }


}



static void
WriteEquity ( FILE *pf, const float r ) {

  int k;
  k = ( r + 1.0f ) * 32767.5;

  putc( k & 0xFF, pf );
  putc( k >> 8, pf );

}

extern int
CalcPosition ( const int i, const int j, const int n ) {

  int max;
  int k;

  max = ( i > j ) ? i : j;

  if ( i + j < n )
    k = ( i + j ) * ( i + j + 1 ) / 2 + j;
  else
    k = n * n - CalcPosition ( n - 1 - i, n - 1 - j, n - 1 ) - 1;

#if 0
  if ( n == 6 ) {
    fprintf ( stderr, "%2d ", k );
    if ( j == n - 1 )
      fprintf ( stderr, "\n" );
  }
#endif

  return k;

}

static void
generate_ts ( const int nTSP, const int nTSC, 
              const int fHeader, const int fCubeful,
              const int nHashSize, bearoffcontext *pbc ) {

    int i, j, k;
    int iPos;
    int n;
    float arEquity[ 4 ];
    hash h;
    char szTmp[ 11 ];
    FILE *pfTmp;
    time_t t;
    unsigned char ac[ 8 ];

#ifdef STDOUT_FILENO 
    FILE *output;

    if( !( output = fdopen( STDOUT_FILENO, "wb" ) ) ) {
	perror( "(stdout)" );
	return;
    }
#else
#define output stdout
#endif

    time ( &t );
    sprintf ( szTmp, "t%06ld.bd", t % 100000 );
    if ( ! ( pfTmp = fopen ( szTmp, "w+b" ) ) ) {
      perror ( szTmp );
      exit ( 2 );
    }

    /* initialise hash */
    
    if ( HashCreate ( &h, nHashSize /  ( fCubeful ? 8 : 2 ) ) ) {
      fprintf ( stderr, _("Error creating hash with %d elements\n"),
                nHashSize /  fCubeful ? 8 : 2 );
      exit(2);
    }
    
    HashStatus ( &h );

    /* write header information */

    if ( fHeader ) {
      char sz[ 41 ];
      sprintf ( sz, "gnubg-TS-%02d-%02d-%1dxxxxxxxxxxxxxxxxxxxxxxx\n", 
                nTSP, nTSC, fCubeful );
      fputs ( sz, output );
    }


    /* generate bearoff database */

    n = Combination ( nTSP + nTSC, nTSC );
    iPos = 0;

    /* positions above diagonal */

    for( i = 0; i < n; i++ ) {
      for( j = 0; j <= i; j++, ++iPos ) {

        BearOff2( i - j, j, nTSP, nTSC, arEquity, n, fCubeful, 
                  &h, pbc );

        for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
          WriteEquity ( pfTmp, arEquity[ k ] );

        HashAdd ( &h, ( i - j ) * n + j, arEquity, 
                  sizeof ( float ) * ( fCubeful ? 4 : 1 ) );

      }
      
      fprintf( stderr, "%d/%d     \r", iPos, n * n );

    }

    /* positions below diagonal */

    for( i = 0; i < n; i++ ) {
      for( j = i + 1; j < n; j++, ++iPos ) {
        
        BearOff2( i + n  - j, j, nTSP, nTSC, arEquity, n, fCubeful,
                  &h, pbc );
        
        for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
          WriteEquity ( pfTmp, arEquity[ k ] );
        
        HashAdd ( &h, ( i + n - j ) * n + j, arEquity, 
                  sizeof ( float ) * ( fCubeful ? 4 : 1 ) );
        
      }
      
      fprintf( stderr, "%d/%d     \r", iPos, n * n );
      
    }
    
    putc ( '\n', stderr );
    
    HashStatus ( &h );
    
    HashDestroy ( &h );
    
    
    /* sort file from ordering:

       136       123
       258  to   456 
       479       789 

    */

    for ( i = 0; i < n; ++i ) 
      for ( j = 0; j < n; ++j ) {

        k = CalcPosition ( i, j, n );

        fseek ( pfTmp, ( fCubeful ? 8 : 2 ) * k, SEEK_SET );
        fread ( ac, 1, fCubeful ? 8 : 2, pfTmp );

        for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
          arEquity[ k ] = 
            ( ac[ 2 * k ] | ac[ 2 * k + 1 ] << 8 ) / 32767.5 - 1.0f;

        fwrite ( ac, 1, fCubeful ? 8 : 2, output );

      }

    fclose ( pfTmp );

    unlink ( szTmp );

}




static void
usage ( char *arg0 ) {

  printf ( "Usage: %s [options]\n"
           "Options:\n"
           "  -t, --two-sided PxC Number of points and number of chequers\n"
           "                      for two-sided database\n"
           "  -o, --one-sided P   Number of points for one-sided database\n"
           "  -s, --hash-size N   Use cache of size N bytes\n"
           "  -O, --old-bearoff filename\n"
           "                      Reuse already generated bearoff database\n"
           "  -H, --no-header     Do not write header\n"
           "  -C, --no-cubeful    Do not calculate cubeful equities for\n"
           "                      one-sided databases\n"
           "  -c, --no-compress   Do not use compression scheme "
                                  "for one-sided databases\n"
           "  -g, --no-gammons    Include gammon distribution for one-sided"
                                  " databases\n"
           "  -n, --normal-dist   Approximate one-sided bearoff database\n"
           "                      with normal distributions\n"
           "  -v, --version       Show version information and exit\n"
           "  -h, --help          Display usage and exit\n"
           "\n"
           "To generate gnubg.bd:\n"
           "%s -t 6x6 -o 6 > gnubg.bd\n"
           "\n",
           arg0, arg0 );

}

static void
version ( void ) {

  printf ( "makebearoff $Revision$\n" );

}


extern int main( int argc, char **argv ) {

  int nTSP = 0, nTSC = 0;
  int nOS = 0;
  int fHeader = TRUE;
  char ch;
  int fCompress = TRUE;
  int fGammon = TRUE;
  int nHashSize = 100000000;
  int fCubeful = TRUE;
  char *szOldBearoff = NULL;
  int fND = FALSE;
  bearoffcontext *pbc = NULL;

  static struct option ao[] = {
    { "two-sided", required_argument, NULL, 't' },
    { "one-sided", required_argument, NULL, 'o' },
    { "hash-size", required_argument, NULL, 's' },
    { "old-bearoff", required_argument, NULL, 'O' },
    { "no-header", no_argument, NULL, 'H' },
    { "no-cubeful", no_argument, NULL, 'C' },
    { "no-compress", no_argument, NULL, 'c' },
    { "no-gammon", no_argument, NULL, 'g' },
    { "normal-dist", no_argument, NULL, 'n' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };

  while ( ( ch = getopt_long ( argc, argv, "t:o:s:O:HCcgnhv", ao, NULL ) ) !=
          (char) -1 ) {
    switch ( ch ) {
    case 't': /* size of two-sided */
      sscanf ( optarg, "%dx%d", &nTSP, &nTSC );
      break;
    case 'o': /* size of one-sided */
      nOS = atoi ( optarg );
      break;
    case 's': /* hash size */
      nHashSize = atoi ( optarg );
      break;
    case 'O': /* old database */
      szOldBearoff = strdup ( optarg );
      break;
    case 'H': /* no header */
      fHeader = FALSE;
      break;
    case 'C': /* cubeful */
      fCubeful = FALSE;
      break;
    case 'c': /* compress */
      fCompress = FALSE;
      break;
    case 'g': /* no gammons */
      fGammon = FALSE;
      break;
    case 'n': /* normal dist */
      fND = TRUE;
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

  /* one sided database */

  if ( nOS ) {

    if ( nOS > 18 ) {
      fprintf ( stderr, 
                _("Size of one-sided bearoff database must be between "
                  "0 and 18\n") );
      exit ( 2 );
    }

    fprintf ( stderr, 
              _("One-sided database:\n"
                "Number of points                  : %12d\n"
                "Number of chequers                : %12d\n"
                "Number of positions               : %'12d\n"
                "Approximate by normal distribution: %s\n"
                "Include gammon distributions      : %s\n"
                "Use compression scheme            : %s\n"
                "Write header                      : %s\n"
                "Size of cache                     : %'12d\n"
                "Reuse old bearoff database        : %s %s\n"),
              nOS, 
              15, 
              Combination ( nOS + 15, nOS ),
              fND ? "yes" : "no",
              fGammon ? "yes" : "no", 
              fCompress ? "yes" : "no", 
              fHeader ? "yes" : "no", 
              nHashSize,
              szOldBearoff ? "yes" : "no",
              szOldBearoff ? szOldBearoff : "" );

    if ( fND )
      fprintf ( stderr, 
                _("Size of database                  : %12d\n"),
                Combination ( nOS + 15, nOS ) * 16 );
    else {
      fprintf ( stderr, 
                _("Size of database (uncompressed)   : %12d\n"),
                ( fGammon ) ?
                Combination ( nOS + 15, nOS ) * 128:
                Combination ( nOS + 15, nOS ) * 64 );
      if ( fCompress )
      fprintf ( stderr, 
                _("Estimated size of compressed db   : %12d\n"),
                ( fGammon ) ?
                Combination ( nOS + 15, nOS ) * 32 :
                Combination ( nOS + 15, nOS ) * 16 );
    }

    if ( szOldBearoff &&
         ! ( pbc = BearoffInit ( szOldBearoff, NULL, BO_NONE, NULL ) ) ) {
      fprintf ( stderr, 
                _("Error initialising old bearoff database!\n" ) );
      exit( 2 );
    }

    /* bearoff database must be of the same kind as the request one-sided
       database */

    if ( pbc && 
         ( pbc->fTwoSided || pbc->fND != fND || pbc->fGammon < fGammon ) ) {
      fprintf ( stderr,
                _("The old database is not of the same kind as the"
                  " requested database\n") );
      exit( 2 );
    }
    
    if ( fND ) {
      generate_nd ( nOS, nHashSize, fHeader, pbc );
    }
    else {
      generate_os ( nOS, fHeader, fCompress, fGammon, nHashSize, pbc );
    }

    if ( pbc ) {
      fprintf ( stderr, "Number of reads in old database: %ld\n",
                pbc->nReads );
      BearoffClose ( pbc );
    }

  }
  
  /*
   * Two-sided database
   */

  if ( nTSC && nTSP ) {

    int n = Combination ( nTSP + nTSC, nTSC );

    fprintf ( stderr,
              _("Two-sided database:\n"
                "Number of points             : %12d\n"
                "Number of chequers           : %12d\n"
                "Calculate equities           : %s\n"
                "Write header                 : %s\n"
                "Number of one-sided positions: %'12d\n"
                "Total number of positions    : %'12d\n"
                "Size of resulting file       : %'12d bytes\n"
                "Size of hash                 : %'12d bytes\n"
                "Reuse old bearoff database   : %s %s\n"),
              nTSP, nTSC,
              fCubeful ? _("cubeless and cubeful") : _("cubeless only"),
              fHeader ? _("yes") : ("no"),
              n,
              n * n,
              n * n * 2 * ( fCubeful ? 4 : 1 ),
              nHashSize,
              szOldBearoff ? "yes" : "no",
              szOldBearoff ? szOldBearoff : "" );

    /* initialise old bearoff database */

    if ( szOldBearoff &&
         ! ( pbc = BearoffInit ( szOldBearoff, NULL, BO_NONE, NULL ) ) ) {
      fprintf ( stderr, 
                _("Error initialising old bearoff database!\n" ) );
      exit( 2 );
    }
    
    /* bearoff database must be of the same kind as the request one-sided
       database */
    
    if ( pbc && ( ! pbc->fTwoSided || pbc->fCubeful != fCubeful ) ) {
      fprintf ( stderr,
                _("The old database is not of the same kind as the"
                  " requested database\n") );
      exit( 2 );
    }
    
    generate_ts ( nTSP, nTSC, fHeader, fCubeful, nHashSize, pbc );

    /* close old bearoff database */

    if ( pbc ) {
      fprintf ( stderr, "Number of reads in old database: %ld\n",
                pbc->nReads );
      BearoffClose ( pbc );
    }

  }

      
  return 0;

}

extern bearoffcontext *
BearoffInitBuiltin ( void ) {

  printf ( "Make makebearoff build (avoid resursive rules Makefile :-)\n" );

}
