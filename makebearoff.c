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
#include <errno.h>

#include "eval.h"
#include "positionid.h"
#include "getopt.h"
#include "i18n.h"
#include "bearoff.h"

#if WIN32
#include <windows.h>
#include <commctrl.h>
#define DLG_MAKEBEAROFF 100
BOOL CALLBACK
DlgProc (HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

HWND hdlg;
int CancelPressed = FALSE;
#endif

typedef struct _xhashent {
  void *p;
  unsigned int iKey;
} xhashent;

typedef struct _xhash {
  unsigned long int nQueries, nHits, nEntries, nOverwrites;
  int nHashSize;
  xhashent *phe;
} xhash;

/* ugly fixes */
char *aszRNG[]; 
char *aszSkillType[ 1 ]; 
int exsExport;
int ap;
/* end ugly fixes */

#if WIN32
static void
dlgprintf(int id, const char *fmt, ... ){

    va_list val;
    char buf[256]; /* FIXME allocate with malloc */
    
    va_start( val, fmt );
    vsprintf(buf, fmt, val);
    SendDlgItemMessage(hdlg, id, WM_SETTEXT, 0, (LPARAM) buf);
    
    va_end( val );
}
#endif

static void dsplerr ( const char *fmt, ... ){

    va_list val;

    char buf[256]; /* FIXME allocate with malloc */
    va_start( val, fmt );
    vsprintf(buf, fmt, val);
#if WIN32
    MessageBox (NULL, buf, "Makebearoff", MB_ICONERROR | MB_OK);
#else
    fprintf( stderr, "%s", buf);
#endif
    va_end( val );
}

static long cLookup;

static long int
XhashPosition ( xhash *ph, const int iKey ) {

  return iKey % ph->nHashSize;

}


static void
XhashStatus ( xhash *ph ) {

#if !WIN32
  fprintf ( stderr, "Xhash status:\n" );
  fprintf ( stderr, "Size:    %d elements\n", ph->nHashSize );
  fprintf ( stderr, "Queries: %lu (hits: %ld)\n", ph->nQueries, ph->nHits );
  fprintf ( stderr, "Entries: %lu (overwrites: %lu)\n",
            ph->nEntries, ph->nOverwrites );
#endif

}


static int
XhashCreate ( xhash *ph, const int nHashSize ) {

  int i;

  if ( ! ( ph->phe =
           (xhashent *) malloc ( nHashSize * sizeof ( xhashent ) ) ) ) {
    perror ( "xhashtable" );
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
XhashDestroy ( xhash *ph ) {

  int i;

  for ( i = 0; i < ph->nHashSize; ++i )
    if ( ph->phe[ i ].p )
      free ( ph->phe[ i ].p );

}


static void
XhashAdd ( xhash *ph, const unsigned int iKey,
          const void *data, const int size ) {

  int l = XhashPosition ( ph, iKey );

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
XhashLookup ( xhash *ph, const unsigned int iKey,
             void **data ) {


  int l = XhashPosition ( ph, iKey );

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


static int
OSLookup ( const unsigned int iPos,
           const int nPoints,
           unsigned short int aProb[ 64 ],
           const int fGammon, const int fCompress,
           FILE *pfOutput, FILE *pfTmp ) {

  int i, j;
  unsigned char ac[ 128 ];

  assert ( pfOutput != stdin );

  if ( fCompress ) {

    unsigned char ac[ 128 ];
    int i, j;
    off_t iOffset;
    int nBytes;
    unsigned int ioff, nz, ioffg, nzg;
    unsigned short int us;

    /* find offsets and no. of non-zero elements */

    if ( fseek ( pfOutput, 40 + iPos * 8, SEEK_SET ) < 0 ) {
      perror ( "output file" );
      exit(-1);
    }

    if ( fread ( ac, 1, 8, pfOutput ) < 8 ) {
      if ( errno )
        perror ( "output file" );
      else
        dsplerr (  "error reading output file\n" );
      exit(-1);
    }

    iOffset = 
      ac[ 0 ] | 
      ac[ 1 ] << 8 |
      ac[ 2 ] << 16 |
      ac[ 3 ] << 24;
    
    nz = ac[ 4 ];
    ioff = ac[ 5 ];
    nzg = ac[ 6 ];
    ioffg = ac[ 7 ];

    /* re-position at EOF */

    if ( fseek ( pfOutput, 0L, SEEK_END ) < 0 ) {
      perror ( "output file" );
      exit(-1);
    }
      
    /* read distributions */

    iOffset = 2 * iOffset;

    nBytes = 2 * ( nz + nzg );

    if ( fseek ( pfTmp, iOffset, SEEK_SET ) < 0 ) {
      perror ( "fseek'ing temp file" );
      exit(-1);
    }

    /* read data */

    if ( fread ( ac, 1, nBytes, pfTmp ) < nBytes ) {
      if ( errno )
        perror ( "reading temp file" );
      else
        dsplerr ( "error reading temp file" );
      exit(-1);
    }
    
    memset ( aProb, 0, 128 );

    i = 0;
    for ( j = 0; j < nz; ++j, i += 2 ) {
      us = ac[ i ] | ac[ i + 1 ] << 8;
      aProb[ ioff + j ] = us;
    }

    if ( fGammon ) 
      for ( j = 0; j < nzg; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i+1 ] << 8;
        aProb[ 32 + ioffg + j ] = us;
      }

    /* re-position at EOF */

    if ( fseek ( pfTmp, 0L, SEEK_END ) < 0 ) {
      perror ( "fseek'ing to EOF on temp file" );
      exit(-1);
    }

  }
  else {

    /* look up position by seeking */

    if ( fseek ( pfOutput, 
                 40 + iPos * ( fGammon ? 128 : 64 ), SEEK_SET ) < 0 ) {
      dsplerr (  "error seeking in pfOutput\n" );
      exit(-1);
    }

    /* read distribution */

    if ( fread ( ac, 1, fGammon ? 128 : 64, pfOutput ) < 
         ( fGammon ? 128 : 64 ) ) {
      dsplerr (  "error readung from pfOutput\n" );
      exit(-1);
    }

    /* save distribution */

    i = 0;
    for ( j = 0; j < 32; ++j, i+=2 )
      aProb[ j ] = ac[ i ] | ac[ i + 1 ] << 8;

    if ( fGammon )
      for ( j = 0; j < 32; ++j, i+=2 )
        aProb[ j + 32 ] = ac[ i ] | ac[ i + 1 ] << 8;

    /* position cursor at end of file */

    if ( fseek ( pfOutput, 0L, SEEK_END ) < 0 ) {
      dsplerr (  "error seeking to end!\n" );
      exit(-1);
    }

  }

  ++cLookup;

  return 0;

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
                     xhash *ph, bearoffcontext *pbc,
                     const int fCompress, 
                     FILE *pfOutput, FILE *pfTmp ) {

    int i, iBest, iMode, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ],
      anBoardTemp[ 2 ][ 25 ], aProb[ 64 ];
    movelist ml;
    int k;
    unsigned int us;
    unsigned int usBest;
    unsigned short int *pusj;
    unsigned short int ausj[ 64 ];
    unsigned short int ausBest[ 32 ];

    unsigned int usGammonBest;
    unsigned short int ausGammonBest[ 32 ];
    int iGammonBest;
    int nBack;

    /* get board for given position */

    PositionFromBearoff( anBoard[ 1 ], nId, nPoints, 15 );

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

    nBack = 0;
    for ( i = 0, k = 0; i < nPoints; ++i ) {
      if ( anBoard[ 1 ][ i ] )
         nBack = i;
      k += anBoard[ 1 ][ i ];
    }

    /* look for position in existing bearoff file */
    
    if ( pbc && nBack < pbc->nPoints ) {
      unsigned int nPosID = PositionBearoff ( anBoard[ 1 ], 
                                              pbc->nPoints, pbc->nChequers );
      BearoffDist ( pbc, nPosID, NULL, NULL, NULL, aOutProb, aOutProb + 32 );
      return;
    }
    
    if ( k < 15 )
      aProb[ 32 ] = 0xFFFF * 36;
	    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

	    usBest = 0xFFFFFFFF; iBest = -1;
            usGammonBest = 0xFFFFFFFF; iGammonBest = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], nPoints, 15 );

		assert( j >= 0 );
		assert( j < nId );


                if ( ! j ) {

                  memset ( pusj = ausj, 0, fGammon ? 128 : 64 );
                  pusj[  0 ] = 0xFFFF;
                  pusj[ 32 ] = 0xFFFF;

                }
                else if ( ! XhashLookup ( ph, j, (void **) &pusj ) ) {
                  /* look up in file generated so far */
                  OSLookup ( j, nPoints, pusj = ausj, fGammon, fCompress,
                             pfOutput, pfTmp );

                  XhashAdd ( ph, j, pusj, fGammon ? 128 : 64 );
                }

                /* find best move to win */

                if ( ( us = RollsOS ( pusj ) ) < usBest ) {
                  iBest = j;
                  usBest = us;
                  memcpy ( ausBest, pusj, 64 );
                }

                /* find best move to save gammon */
                  
                if ( fGammon && 
                     ( ( us = RollsOS ( pusj + 32 ) ) < usGammonBest ) ) {
                  iGammonBest = j;
                  usGammonBest = us;
                  memcpy ( ausGammonBest, pusj + 32, 64 );
                }

	    }

	    assert( iBest >= 0 );
            assert( iGammonBest >= 0 );

	    if( anRoll[ 0 ] == anRoll[ 1 ] ) {
              for( i = 0; i < 31; i++ ) {
                aProb[ i + 1 ] += ausBest[ i ];
                if ( k == 15 && fGammon )
                  aProb[ 32 + i + 1 ] += ausGammonBest[ i ];
              }
            }
	    else {
              for( i = 0; i < 31; i++ ) {
                aProb[ i + 1 ] += 2 * ausBest[ i ];
                if ( k == 15 && fGammon )
                  aProb[ 32 + i + 1 ] += 2 * ausGammonBest[ i ];
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
              const int nHashSize, bearoffcontext *pbc,
			  FILE *output) {

  int i;
  int n;
  unsigned short int aus[ 64 ];
  xhash h;
  FILE *pfTmp = NULL;
  time_t t;
  unsigned int npos;
  char szTmp[ 11 ];
#if WIN32
  HINSTANCE hInstance = (HINSTANCE) GetModuleHandle(NULL);
  HWND hwndPB;
  if( hdlg != NULL)
    ShowWindow(hdlg, SW_SHOW);

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&InitCtrlEx);

  hwndPB = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		  12, 300, 470, 20, hdlg, NULL, hInstance, NULL);
#endif

  /* initialise xhash */

#if WIN32
  dlgprintf(127, "Creating cache memory." );
#endif
  if ( XhashCreate ( &h, nHashSize /  ( fGammon ? 128 : 64 ) ) ) {
    dsplerr (  _("Error creating xhash with %d elements\n"),
              nHashSize /  fGammon ? 128 : 64 );
    exit(2);
  }

  XhashStatus ( &h );

  /* write header */

  if ( fHeader ) {
    char sz[ 41 ];
#if WIN32
    dlgprintf(127, "Writing header information." );
#endif
    sprintf ( sz, "gnubg-OS-%02d-15-%1d-%1d-0xxxxxxxxxxxxxxxxxxx\n", 
              nOS, fGammon, fCompress );
    fputs ( sz, output );
  }

  if ( fCompress ) {
#if WIN32
    dlgprintf(127, "Opening temporary file." );
#endif
    time ( &t );
    sprintf ( szTmp, "t%06ld.bd", t % 100000 );
    if ( ! ( pfTmp = fopen ( szTmp, "w+b" ) ) ) {
      perror ( szTmp );
      exit ( 2 );
    }

  }
    

  /* loop trough bearoff positions */

  n = Combination ( nOS + 15, nOS );
  npos = 0;
  
#if WIN32
  SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, n / 100));
  SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0);
  dlgprintf(127, "Calculating bearoff data." );
#endif

  for ( i = 0; i < n; ++i ) {
    
    if ( i )
      BearOff ( i, nOS, aus, fGammon, &h, pbc, fCompress, output, pfTmp );
    else {
      memset ( aus, 0, 128 );
      aus[  0 ] = 0xFFFF;
      aus[ 32 ] = 0xFFFF;
    }
#if WIN32
    if (!((i+1) % 100))
      SendMessage(hwndPB, PBM_STEPIT, 0, 0);
    if (CancelPressed)
      break;
#else
    if (!(i % 100))
      fprintf (stderr, "1:%d/%d        \r", i, n);
#endif

    WriteOS ( aus, fCompress, fCompress ? pfTmp : output );
    if ( fGammon )
      WriteOS ( aus + 32, fCompress, fCompress ? pfTmp : output );

    XhashAdd ( &h, i, aus, fGammon ? 128 : 64 );

    if ( fCompress ) 
      WriteIndex ( &npos, aus, fGammon, output );

  }

  if ( fCompress ) {

    char ac[ 256 ];
    int n;
#if WIN32
    dlgprintf(127, "Rewriting to compressed database." );
#endif

    /* write contents of pfTmp to output */

    rewind ( pfTmp );

    while ( ! feof ( pfTmp ) && ( n = fread ( ac, 1, sizeof ( ac ), pfTmp ) ) )
      fwrite ( ac, 1, n, output );

    fclose ( pfTmp );

    unlink ( szTmp );

  }
#if !WIN32
  putc ( '\n', stderr );
#else
  dlgprintf(127, "Clearing cache memory." );
#endif

  XhashStatus ( &h );

  XhashDestroy ( &h );

  return 0;

}


static void
NDBearoff ( const int iPos, const int nPoints, float ar[ 4 ], xhash *ph,
            bearoffcontext *pbc) {

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

  int iGammonBest;
  float arGammonBest[ 4 ];
  float rGammonBest;

  for ( i = 0; i < 4; ++i )
    ar[ i ] = 0.0f;

  if ( ! iPos ) 
    return;

  PositionFromBearoff( anBoard[ 1 ], iPos, nPoints, 15 );

  for( i = nPoints; i < 25; i++ )
    anBoard[ 1 ][ i ] = 0;

  /* 
   * look for position in existing bearoff file 
   */

  if ( pbc ) {
    for ( i = 24; i >= 0 && ! anBoard[ 1 ][ i ]; --i )
      ;

    if ( i < pbc->nPoints ) {
      unsigned int nPosID = PositionBearoff ( anBoard[ 1 ], 
                                              pbc->nPoints, pbc->nChequers );
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
      rGammonBest = 1e10;
      iBest = -1;
      iGammonBest = -1;

      for ( i = 0; i < ml.cMoves; ++i ) {

        PositionFromKey ( anBoardTemp, ml.amMoves[ i ].auch );

        j = PositionBearoff ( anBoardTemp[ 1 ], nPoints, 15 );

        if ( ! XhashLookup ( ph, j, (void **) &prj ) ) {
          NDBearoff ( j, nPoints, prj = arj, ph, pbc );
          XhashAdd ( ph, j, prj, 16 );
        }

        /* find best move to win */

        if ( prj[ 0 ] < rBest ) {
          iBest = j;
          rBest = prj[ 0 ];
          memcpy ( arBest, prj, 4 * sizeof ( float ) );
        }

        /* find best move to save gammon */

        if ( prj[ 2 ] < rGammonBest ) {
          iGammonBest = j;
          rGammonBest = prj[ 2 ];
          memcpy ( arGammonBest, prj, 4 * sizeof ( float ) );
        }

      }

      assert ( iBest >= 0 );
      assert ( iGammonBest >= 0 );
      
      rMean = 1.0f + arBest[ 0 ];

      ar[ 0 ] += ( d0 == d1 ) ? rMean : 2.0f * rMean;

      rMean = arBest[ 1 ] * arBest[ 1 ] + rMean * rMean;

      rVarSum += ( d0 == d1 ) ? rMean : 2.0f * rMean;
      
      if ( k == 15 ) {

        rMean = 1.0f + arGammonBest[ 2 ];

        ar[ 2 ] += ( d0 == d1 ) ? rMean : 2.0f * rMean;

        rMean = 
          arGammonBest[ 3 ] * arGammonBest[ 3 ] + rMean * rMean;

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
              bearoffcontext *pbc, FILE *output ) {

  int n = Combination ( nPoints + 15, nPoints );

  int i, j;
  char sz[ 41 ];
  float ar[ 4 ];

  xhash h;
#if WIN32
  HINSTANCE hInstance = (HINSTANCE) GetModuleHandle(NULL);
  HWND hwndPB;
  if( hdlg != NULL)
    ShowWindow(hdlg, SW_SHOW);

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&InitCtrlEx);

  hwndPB = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		  12, 300, 470, 20, hdlg, NULL, hInstance, NULL);
#endif

  /* initialise xhash */
  
#if WIN32
    dlgprintf(127, "Creating cache memory." );
#endif
 
  if ( XhashCreate ( &h, nHashSize / ( 4 * sizeof ( float ) ) ) ) {
    dsplerr (  "Error creating cache\n" );
    return;
  }

  XhashStatus ( &h );

  if ( fHeader ) {
#if WIN32
    dlgprintf(127, "Writing header information." );
#endif
    sprintf ( sz, "gnubg-OS-%02d-15-1-0-1xxxxxxxxxxxxxxxxxxx\n", nPoints );
    fputs ( sz, output );
  }

#if WIN32
  SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, n / 100));
  SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0);
  dlgprintf(127, "Calculating bearoff data." );
#endif

  for ( i = 0; i < n; ++i ) {

    if ( i )
      NDBearoff ( i, nPoints, ar, &h, pbc );
    else
      ar[ 0 ] = ar[ 1 ] = ar[ 2 ] = ar[ 3 ] = 0.0f;

    for ( j = 0; j < 4; ++j )
      WriteFloat ( ar[ j ], output );

    XhashAdd ( &h, i, ar, 16 );
#if WIN32
    if (!((i+1) % 100))
      SendMessage(hwndPB, PBM_STEPIT, 0, 0);
#else
    if (!(i % 100))
      fprintf (stderr, "1:%d/%d        \r", i, n);
#endif

  }
#if !WIN32
  putc ( '\n', stderr );
#else
  dlgprintf(127, "Clearing cache memory." );
#endif
  XhashStatus ( &h );

  XhashDestroy ( &h );

}


static short int
CubeEquity ( const short int siND, const short int siDT,
             const short int siDP ) {

  if ( siDT >= (siND/2) && siDP >= siND ) {
    /* it's a double */

    if ( siDT >= (siDP/2) )
      /* double, pasi */
      return siDP;
    else
      /* double, take */
      return 2 * siDT;

  }
  else

    /* no double */

    return siND;

}

static int
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
TSLookup ( const int nUs, const int nThem,
           const int nTSP, const int nTSC,
           short int arEquity[ 4 ], 
           const int n, const int fCubeful,
           FILE *pfTmp ) {

  int iPos = CalcPosition ( nUs, nThem, n );
  unsigned char  ac[ 8 ];
  int i;
  unsigned short int us;

  /* seek to position */

  if ( fseek ( pfTmp, iPos * ( fCubeful ? 8 : 2 ), SEEK_SET ) < 0 ) {
    perror ( "temp file" );
    exit(-1);
  }

  if ( fread ( ac, 1, 8, pfTmp ) < 8 ) {
      if ( errno )
        perror ( "temp file" );
      else
        dsplerr (  "error reading temp file\n" );
      exit(-1);
  }

  /* re-position at EOF */
  
  if ( fseek ( pfTmp, 0L, SEEK_END ) < 0 ) {
    perror ( "temp file" );
    exit(-1);
  }
  
  for ( i = 0; i < ( fCubeful ? 4 : 1 ); ++i ) {
    us = ac[ 2 * i ] | ac[ 2 * i + 1 ] << 8;
    arEquity[ i ] = us - 0x8000;
  }

  ++cLookup;
    
}
  

/*
 * Calculate exact equity for position.
 *
 * We store the equity in two bytes:
 * 0x0000 meaning equity=-1 and 0xFFFF meaning equity=+1.
 *
 */


static void BearOff2( int nUs, int nThem,
                      const int nTSP, const int nTSC,
                      short int asiEquity[ 4 ],
                      const int n, const int fCubeful,
                      xhash *ph, bearoffcontext *pbc, FILE *pfTmp ) {

    int i, j, anRoll[ 2 ], anBoard[ 2 ][ 25 ], anBoardTemp[ 2 ][ 25 ];
    movelist ml;
    int aiBest[ 4 ];
    int asiBest[ 4 ];
    int aiTotal[ 4 ];
    short int k;
    short int *psij;
    short int asij[ 4 ];
    const short int EQUITY_P1 = 0x7FFF;
    const short int EQUITY_M1 = ~EQUITY_P1;

    if ( ! nUs ) {

      /* we have won */
      asiEquity[ 0 ] = asiEquity[ 1 ] = 
        asiEquity[ 2 ] = asiEquity[ 3 ] = EQUITY_P1;
      return;

    }

    if ( ! nThem ) {

      /* we have lost */
      asiEquity[ 0 ] = asiEquity[ 1 ] = 
        asiEquity[ 2 ] = asiEquity[ 3 ] = EQUITY_M1;
      return;

    }

    PositionFromBearoff( anBoard[ 0 ], nThem, nTSP, nTSC );
    PositionFromBearoff( anBoard[ 1 ], nUs, nTSP, nTSC );

    for( i = nTSP; i < 25; i++ )
	anBoard[ 1 ][ i ] = anBoard[ 0 ][ i ] = 0;

    /* look for position in bearoff file */

    if ( pbc && isBearoff ( pbc, anBoard ) ) {
      unsigned short int nUsL = 
        PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
      unsigned short int nThemL = 
        PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
      int nL = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
      unsigned int iPos = nUsL * nL + nThemL;
      unsigned short int aus[ 4 ];
      
      BearoffCubeful ( pbc, iPos, NULL, aus );

      for ( i = 0; i < 4; ++i )
        asiEquity[ i ] = aus[ i ] - 0x8000;
      
      return;
    }

    aiTotal[ 0 ] = aiTotal[ 1 ] = aiTotal[ 2 ] = aiTotal[ 3 ] = 0.0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    GenerateMoves( &ml, anBoard, anRoll[ 0 ], anRoll[ 1 ], FALSE );

            asiBest [ 0 ] = asiBest[ 1 ] = 
              asiBest[ 2 ] = asiBest [ 3 ] = -0xFFFF;
            aiBest [ 0 ] = aiBest[ 1 ] = aiBest[ 2 ] = aiBest [ 3 ] = -1;
	    
	    for( i = 0; i < ml.cMoves; i++ ) {
		PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

		j = PositionBearoff( anBoardTemp[ 1 ], nTSP, nTSC );

		assert( j >= 0 );
		assert( j < nUs ); 

                if ( ! nThem ) {
                  psij = asij;
                  asij[ 0 ] = asij[ 1 ] = asij[ 2 ] = asij[ 3 ] = EQUITY_P1;
                }
                else if ( ! j ) {
                  psij = asij;
                  asij[ 0 ] = asij[ 1 ] = asij[ 2 ] = asij[ 3 ] = EQUITY_M1;
                }
                if ( ! XhashLookup ( ph, n * nThem + j, (void **) &psij ) ) {
                  /* lookup in file */
                  TSLookup ( nThem, j, nTSP, nTSC, psij = asij, n,
                             fCubeful, pfTmp );
                  XhashAdd ( ph, n * nThem + j, psij, fCubeful ? 8 : 2 );
                }

                /* cubeless */

                if ( psij[ 0 ] < -asiBest[ 0 ] ) { 
                  aiBest [ 0 ] = j;
                  asiBest [ 0 ] = ~psij[ 0 ];
                }

                if ( fCubeful ) {

                  /* I own cube:
                     from opponent's view he doesn't own cube */
                  
                  if ( psij[ 3 ] < -asiBest[ 1 ] ) {
                    aiBest [ 1 ] = j;
                    asiBest [ 1 ] = ~psij[ 3 ];
                  }

                  /* Centered cube (so centered for opponent too) */

                  k = CubeEquity ( psij[ 2 ], psij[ 3 ], EQUITY_P1 ); 
                  if ( ~k > asiBest[ 2 ] ) {
                    aiBest [ 2 ] = j;
                    asiBest [ 2 ] = ~k;
                  }

                  /* Opponent owns cube:
                     from opponent's view he owns cube */

                  k = CubeEquity ( psij[ 1 ], psij[ 3 ], EQUITY_P1 ); 
                  if ( ~k > asiBest[ 3 ] ) {
                    aiBest [ 3 ] = j;
                    asiBest [ 3 ] = ~k;
                  }

                }

	    }

	    assert( aiBest[ 0 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 1 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 2 ] >= 0 );
	    assert( ! fCubeful || aiBest[ 3 ] >= 0 );
	    
            if ( anRoll[ 0 ] == anRoll[ 1 ] )
              for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
                aiTotal [ k ] += asiBest[ k ];
            else
              for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
                aiTotal [ k ] += 2 * asiBest[ k ];

	}

    /* final equities */
    for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
      asiEquity[ k ] = aiTotal[ k ] / 36;

}



static void
WriteEquity ( FILE *pf, const short int si ) {
  
  unsigned short int us = si + 0x8000;

  putc( us & 0xFF, pf );
  putc( (us >> 8) & 0xFF, pf );

}

static void
generate_ts ( const int nTSP, const int nTSC, 
              const int fHeader, const int fCubeful,
              const int nHashSize, bearoffcontext *pbc, FILE *output ) {

    int i, j, k;
    int iPos;
    int n;
    short int asiEquity[ 4 ];
    xhash h;
    char szTmp[ 11 ];
    FILE *pfTmp;
    time_t t;
    unsigned char ac[ 8 ];
#if WIN32
  HINSTANCE hInstance = (HINSTANCE) GetModuleHandle(NULL);
  HWND hwndPB;
  if( hdlg != NULL)
    ShowWindow(hdlg, SW_SHOW);

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&InitCtrlEx);

  hwndPB = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		  12, 300, 470, 20, hdlg, NULL, hInstance, NULL);
#endif

    time ( &t );
    sprintf ( szTmp, "t%06ld.bd", t % 100000 );
    if ( ! ( pfTmp = fopen ( szTmp, "w+b" ) ) ) {
      perror ( szTmp );
      exit ( 2 );
    }

    /* initialise xhash */
    
#if WIN32
    dlgprintf(127, "Initialising xhash." );
#endif
    
    if ( XhashCreate ( &h, nHashSize /  ( fCubeful ? 8 : 2 ) ) ) {
      dsplerr (  _("Error creating xhash with %d elements\n"),
                nHashSize /  fCubeful ? 8 : 2 );
      exit(2);
    }
    
    XhashStatus ( &h );

    /* write header information */

    if ( fHeader ) {
      char sz[ 41 ];
#if WIN32
      dlgprintf(127, "Writing header information." );
#endif
      sprintf ( sz, "gnubg-TS-%02d-%02d-%1dxxxxxxxxxxxxxxxxxxxxxxx\n", 
                nTSP, nTSC, fCubeful );
      fputs ( sz, output );
    }


    /* generate bearoff database */

    n = Combination ( nTSP + nTSC, nTSC );
    iPos = 0;
    
#if WIN32
    SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, n ));
    SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0);
#endif

    /* positions above diagonal */
#if WIN32
    dlgprintf(127, "Calculating positions above diagonal." );
#endif

    for( i = 0; i < n; i++ ) {
      for( j = 0; j <= i; j++, ++iPos ) {

        BearOff2( i - j, j, nTSP, nTSC, asiEquity, n, fCubeful, 
                  &h, pbc, pfTmp );

        for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
          WriteEquity ( pfTmp, asiEquity[ k ] );

        XhashAdd ( &h, ( i - j ) * n + j, asiEquity, fCubeful ? 8 : 2 );

      }
#if WIN32
      SendMessage(hwndPB, PBM_STEPIT, 0, 0);
#else
      fprintf( stderr, "%d/%d     \r", iPos, n * n );
#endif
    }

    /* positions below diagonal */
#if WIN32
    dlgprintf(127, "Calculating positions below diagonal." );
    SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, n ));
    SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0);
#endif

    for( i = 0; i < n; i++ ) {
      for( j = i + 1; j < n; j++, ++iPos ) {
        
        BearOff2( i + n  - j, j, nTSP, nTSC, asiEquity, n, fCubeful,
                  &h, pbc, pfTmp );
        
        for ( k = 0; k < ( fCubeful ? 4 : 1 ); ++k )
          WriteEquity ( pfTmp, asiEquity[ k ] );
        
        XhashAdd ( &h, ( i + n - j ) * n + j, asiEquity, fCubeful ? 8 : 2 );
        
      }
#if WIN32
      SendMessage(hwndPB, PBM_STEPIT, 0, 0);
#else
      fprintf( stderr, "%d/%d     \r", iPos, n * n );
#endif
    }

#if !WIN32
    putc ( '\n', stderr );
#else
    dlgprintf(127, "Clearing xhash." );
#endif
    XhashStatus ( &h );
    
    XhashDestroy ( &h );
    
    /* sort file from ordering:

       136       123
       258  to   456 
       479       789 

    */
#if WIN32
    dlgprintf(127, "Sorting file." );
    SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, n ));
    SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0);
#endif

    for ( i = 0; i < n; ++i ){ 
      for ( j = 0; j < n; ++j ) {

        k = CalcPosition ( i, j, n );

        fseek ( pfTmp, ( fCubeful ? 8 : 2 ) * k, SEEK_SET );
        fread ( ac, 1, fCubeful ? 8 : 2, pfTmp );
        fwrite ( ac, 1, fCubeful ? 8 : 2, output );
      }
#if WIN32
      SendMessage(hwndPB, PBM_STEPIT, 0, 0);
#endif

    }

#if WIN32
    dlgprintf(127, "Closing and unlinking." );
#endif
    fclose ( pfTmp );

    unlink ( szTmp ); 

}




static void
usage ( char *arg0 ) {

  printf ( "Usage: %s [options] -f filename\n"
           "Options:\n"
           "  -f, --outfile filename\n"
           "                      Output to file\n"
           "  -t, --two-sided PxC Number of points and number of chequers\n"
           "                      for two-sided database\n"
           "  -o, --one-sided P   Number of points for one-sided database\n"
           "  -s, --xhash-size N   Use cache of size N bytes\n"
           "  -O, --old-bearoff filename\n"
           "                      Reuse already generated bearoff database\n"
           "  -H, --no-header     Do not write header\n"
           "  -C, --no-cubeful    Do not calculate cubeful equities for\n"
           "                      two-sided databases\n"
           "  -c, --no-compress   Do not use compression scheme "
                                  "for one-sided databases\n"
           "  -g, --no-gammons    Include gammon distribution for one-sided"
                                  " databases\n"
           "  -n, --normal-dist   Approximate one-sided bearoff database\n"
           "                      with normal distributions\n"
           "  -v, --version       Show version information and exit\n"
           "  -h, --help          Display usage and exit\n"
           "\n"
           "To generate gnubg_os0.bd:\n"
           "%s -o 6 -f gnubg_os0.bd\n"
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
  FILE *output = stdout;
  char *szOutput = NULL;
  double r;
#if WIN32
  int i;
  char *aszOS[] = {"Number of points:",
		 "Number of chequers:",
		 "Number of positions:",
		 "Approximate by normal distribution:",
		 "Include gammon distributions:",
		 "Use compression scheme:",
		 "Write header:",
		 "Size of cache:",
		 "Reuse old bearoff database:"};
		 
  char *aszTS[]= { "Number of points:", 
                "Number of chequers:",
                "Calculate equities:",
                "Write header:",
                "Number of one-sided positions:",
                "Total number of positions:",
                "Size of resulting file:",
                "Size of xhash:",
                "Reuse old bearoff database:",
                " ", " "};
#endif

  static struct option ao[] = {
    { "two-sided", required_argument, NULL, 't' },
    { "one-sided", required_argument, NULL, 'o' },
    { "xhash-size", required_argument, NULL, 's' },
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

  while ( ( ch = getopt_long ( argc, argv, "t:o:s:O:f:HCcgnhv", ao, NULL ) ) !=
          (char) -1 ) {
    switch ( ch ) {
    case 't': /* size of two-sided */
      sscanf ( optarg, "%dx%d", &nTSP, &nTSC );
      break;
    case 'o': /* size of one-sided */
      nOS = atoi ( optarg );
      break;
    case 's': /* xhash size */
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
    case 'f':
      szOutput = strdup ( optarg );
      break;
    default:
      usage ( argv[ 0 ] );
      exit ( 1 );
    }
  }

  /* open output file */

  if ( ! szOutput ) {
    usage ( argv[ 0 ] );
    return EXIT_FAILURE;
  }

  if ( ! ( output = fopen ( szOutput, "w+b" ) ) ) {
    perror ( szOutput );
    return EXIT_FAILURE;
  }

  /* one sided database */

  if ( nOS ) {

    if ( nOS > 18 ) {
      dsplerr (  _("Size of one-sided bearoff database must be between "
                  "0 and 18\n") );
      exit ( 2 );
    }
#if WIN32 
    hdlg = CreateDialog(NULL, MAKEINTRESOURCE (DLG_MAKEBEAROFF), NULL, DlgProc);
    /* error if NULL */
    for (i = 0; i < 9; i++)
       SendDlgItemMessage(hdlg, 101 + i, WM_SETTEXT, 0, (LPARAM) aszOS[i]);
      
    dlgprintf( 116, "%d", nOS);
    dlgprintf( 117, "%d", 15); 
    dlgprintf( 118, "%d", Combination ( nOS + 15, nOS ));
    dlgprintf( 119, "%s", fND ? "yes" : "no");
    dlgprintf( 120, "%s", fGammon ? "yes" : "no"); 
    dlgprintf( 121, "%s", fCompress ? "yes" : "no"); 
    dlgprintf( 122, "%s", fHeader ? "yes" : "no");
    dlgprintf( 123, "%d", nHashSize);
    dlgprintf( 124, "%s", szOldBearoff ? "yes" : "no");
    dlgprintf(130, "Generating one-sided bearoff database. Please wait." );
    dlgprintf(131, "makebearoff $Revision$" );
#else
    fprintf ( stderr, 
              _("One-sided database:\n"
                "Number of points                  : %12d\n"
                "Number of chequers                : %12d\n"
                "Number of positions               : %12d\n"
                "Approximate by normal distribution: %s\n"
                "Include gammon distributions      : %s\n"
                "Use compression scheme            : %s\n"
                "Write header                      : %s\n"
                "Size of cache                     : %12d\n"
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
#endif

    if ( fND ) {
      r = Combination ( nOS + 15, nOS ) * 16.0;
#if WIN32
      dlgprintf(110, "Size of database:");
      dlgprintf(111, "");
      dlgprintf(125, "%.0f (%.1f MB)", r, r / 1048576.0);
      dlgprintf(126, "");
#else
      fprintf ( stderr, 
                _("Size of database                  : %.0f (%.1f MB)\n"), 
                r, r / 1048576.0 );
#endif
    }
    else {
      r = Combination ( nOS + 15, nOS ) * ( fGammon ? 128.0f : 64.0f );
#if WIN32
      dlgprintf(110, "Size of database (uncompressed):");
      dlgprintf(125, "%.0f (%.1f MB)", r, r / 1048576.0);
#else
      fprintf ( stderr, 
                _("Size of database (uncompressed)   : %.0f (%.1f MB)\n"), 
                r, r / 1048576.0 );
#endif
      if ( fCompress ) {
        r = Combination ( nOS + 15, nOS ) * ( fGammon ? 32.0f : 16.0f );
#if WIN32
        dlgprintf(111, "Estimated size of compressed db:");
        dlgprintf(126, "%.0f (%.1f MB)", r, r / 1048576.0);
#else
        fprintf ( stderr, 
                  _("Estimated size of compressed db   : %.0f (%.1f MB)\n"), 
                  r, r / 1048576.0 );
#endif
      }
    }

    if ( szOldBearoff &&
         ! ( pbc = BearoffInit ( szOldBearoff, NULL, BO_NONE, NULL ) ) ) {
      dsplerr ( _("Error initialising old bearoff database!\n" ) );
      exit( 2 );
    }

    /* bearoff database must be of the same kind as the request one-sided
       database */

    if ( pbc && 
         ( pbc->bt != BEAROFF_ONESIDED || pbc->fND != fND || 
           pbc->fGammon < fGammon ) ) {
      dsplerr ( _("The old database is not of the same kind as the"
                  " requested database\n") );
      exit( 2 );
    }
    
    if ( fND ) {
      generate_nd ( nOS, nHashSize, fHeader, pbc, output );
    }
    else {
      generate_os ( nOS, fHeader, fCompress, fGammon, nHashSize, pbc, output );
    }

    if ( pbc ) {
#if !WIN32
      fprintf ( stderr, "Number of reads in old database: %lu\n",
                pbc->nReads );
#endif
      BearoffClose ( &pbc );
    }

#if !WIN32
    fprintf ( stderr, "Number of re-reads while generating: %ld\n", 
              cLookup );
#endif
  }
  
  /*
   * Two-sided database
   */

  if ( nTSC && nTSP ) {

    int n = Combination ( nTSP + nTSC, nTSC );

    r = n;
    r = r * r * ( fCubeful ? 8.0 : 2.0 );
#if WIN32 
    hdlg = CreateDialog(NULL, MAKEINTRESOURCE (DLG_MAKEBEAROFF), NULL, DlgProc);
    /* error if NULL */
    for (i = 0; i < 11; i++)
       SendDlgItemMessage(hdlg, 101 + i, WM_SETTEXT, 0, (LPARAM) aszTS[i]);
      
    dlgprintf(116, "%d", nTSP);
    dlgprintf(117, "%d", nTSC);
    dlgprintf(118, "%s", 
	fCubeful ? _("cubeless and cubeful") : _("cubeless only"));
    dlgprintf(119, "%s",  fHeader ? _("yes") : ("no"));
    dlgprintf(120, "%d",  n);
    dlgprintf(121, "%d",  n * n);
    dlgprintf(122, "%.0f bytes (%.1f MB)", r, r / 1048576.0);
    dlgprintf(123, "%d bytes",  nHashSize);
    dlgprintf(124, "%s",  szOldBearoff ? szOldBearoff : "No" );
    dlgprintf(125, "" );
    dlgprintf(126, "" );
    dlgprintf(130, "Generating two-sided bearoff database. Please wait." );
    dlgprintf(131, "makebearoff $Revision$" );
#else 
    fprintf ( stderr,
              _("Two-sided database:\n"
                "Number of points             : %12d\n"
                "Number of chequers           : %12d\n"
                "Calculate equities           : %s\n"
                "Write header                 : %s\n"
                "Number of one-sided positions: %12d\n"
                "Total number of positions    : %12d\n"
                "Size of resulting file       : %.0f bytes (%.1f MB)\n"
                "Size of xhash                : %12d bytes\n"
                "Reuse old bearoff database   : %s %s\n"),
              nTSP, nTSC,
              fCubeful ? _("cubeless and cubeful") : _("cubeless only"),
              fHeader ? _("yes") : ("no"),
              n,
              n * n,
              r, r / 1048576.0,
              nHashSize,
              szOldBearoff ? "yes" : "no",
              szOldBearoff ? szOldBearoff : "" );
#endif
    /* initialise old bearoff database */
#if WIN32
    dlgprintf(127, "Initialising old bearoff database." );
#endif
    if ( szOldBearoff &&
         ! ( pbc = BearoffInit ( szOldBearoff, NULL, BO_NONE, NULL ) ) ) {
      dsplerr ( _("Error initialising old bearoff database!\n" ) );
      exit( 2 );
    }
    
    /* bearoff database must be of the same kind as the request one-sided
       database */
    
    if ( pbc && ( pbc->bt != BEAROFF_TWOSIDED || pbc->fCubeful != fCubeful ) ) {
      dsplerr ( _("The old database is not of the same kind as the"
                  " requested database\n") );
      exit( 2 );
    }
    
    generate_ts ( nTSP, nTSC, fHeader, fCubeful, nHashSize, pbc, output );

    /* close old bearoff database */

    if ( pbc ) {
#if !WIN32
      fprintf ( stderr, "Number of reads in old database: %lu\n",
                pbc->nReads );
#endif
      BearoffClose ( &pbc );
    }
#if !WIN32
    fprintf ( stderr, "Number of re-reads while generating: %ld\n", 
              cLookup );
#endif

  }

  fclose ( output );
#if WIN32
  EndDialog(hdlg, 0);
#endif
  return 0;

}

extern bearoffcontext *
BearoffInitBuiltin ( void ) {

  printf ( "Make makebearoff build (avoid resursive rules Makefile :-)\n" );
  return NULL;

}

#if WIN32
BOOL CALLBACK
DlgProc (HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam){
  
   switch (Message)
   {
   case WM_INITDIALOG:
	   return TRUE;

   case WM_COMMAND:
	switch ( LOWORD(wParam) )
	{
	case IDCANCEL:
           CancelPressed = TRUE;		
	   EndDialog(hwnd, 0);
	   exit(2);
	   return TRUE;
	}
	break;
   }
   return FALSE;
}
#endif

	   
