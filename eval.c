/*
 * eval.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-2000.
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

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <assert.h>
#include <cache.h>
#include <errno.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <neuralnet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dice.h"
#include "eval.h"
#include "positionid.h"

/* From pub_eval.c: */
extern float pubeval( int race, int pos[] );
static int CompareRedEvalData( const void *p0, const void *p1 );

extern int
EvaluatePositionCubeful1( int anBoard[ 2 ][ 25 ], float *prOutput, 
                          cubeinfo *pci, evalcontext *pec, 
                          int nPlies);

typedef void ( *classevalfunc )( int anBoard[ 2 ][ 25 ], float arOutput[] );
typedef void ( *classdumpfunc )( int anBoard[ 2 ][ 25 ], char *szOutput );
typedef int ( *cfunc )( const void *, const void * );

/* Race inputs */
enum {
  /* In a race position, bar and the 24 points are always empty, so only */
  /* 23*4 (92) are needed */

  /* (0 <= k < 14), RI_OFF + k = */
  /*                       1 if more than k checkers are off, 0 otherwise */

  RI_OFF = 92,

  /* Number of cross-overs by outside checkers */
  
  RI_NCROSS = 92 + 14,

  /* Pip-count of checkers outside homeboard */
  
  RI_OPIP,

  /* total Pip-count */

  RI_PIP,
  
  HALF_RACE_INPUTS
};


/* Contact inputs -- see Berliner for most of these */
enum {
  /* n - number of checkers off
     
     off1 -  1         n >= 5
             n/5       otherwise
	     
     off2 -  1         n >= 10
             (n-5)/5   n < 5 < 10
	     0         otherwise
	     
     off3 -  (n-10)/5  n > 10
             0         otherwise
  */
     
  I_OFF1 = 100, I_OFF2, I_OFF3,
  
  /* Minimum number of pips required to break contact.

     For each checker x, N(x) is checker location,
     C(x) is max({forall o : N(x) - N(o)}, 0)

     Break Contact : (sum over x of C(x)) / 152

     152 is dgree of contact of start position.
  */
  I_BREAK_CONTACT,

  /* Location of back checker (Normalized to [01])
   */
  I_BACK_CHEQUER,

  /* Location of most backward anchor.  (Normalized to [01])
   */
  I_BACK_ANCHOR,

  /* Forward anchor in opponents home.

     Normalized in the following way:  If there is an anchor in opponents
     home at point k (1 <= k <= 6), value is k/6. Otherwise, if there is an
     anchor in points (7 <= k <= 12), take k/6 as well. Otherwise set to 2.
     
     This is an attempt for some continuity, since a 0 would be the "same" as
     a forward anchor at the bar.
   */
  I_FORWARD_ANCHOR,

  /* Average number of pips opponent loses from hits.
     
     Some heuristics are required to estimate it, since we have no idea what
     the best move actually is.

     1. If board is weak (less than 3 anchors), don't consider hitting on
        points 22 and 23.
     2. Dont break anchors inside home to hit.
   */
  I_PIPLOSS,

  /* Number of rolls that hit at least one checker.
   */
  I_P1,

  /* Number of rolls that hit at least two checkers.
   */
  I_P2,

  /* How many rolls permit the back checker to escape (Normalized to [01])
   */
  I_BACKESCAPES,

  /* Maximum containment of opponent checkers, from our points 9 to op back 
     checker.
     
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  I_ACONTAIN,
  
  /* Above squared */
  I_ACONTAIN2,

  /* Maximum containment, from our point 9 to home.
     Value is (1 - n/36), where n is number of rolls to escape.
   */
  I_CONTAIN,

  /* Above squared */
  I_CONTAIN2,

  /* For all checkers out of home, 
     sum (Number of rolls that let x escape * distance from home)

     Normalized by dividing by 3600.
  */
  I_MOBILITY,

  /* One sided moment.
     Let A be the point of weighted average: 
     A = sum of N(x) for all x) / nCheckers.
     
     Then for all x : A < N(x), M = (average (N(X) - A)^2)

     Diveded by 400 to normalize. 
   */
  I_MOMENT2,

  /* Average number of pips lost when on the bar.
     Normalized to [01]
  */
  I_ENTER,

  /* Probablity of one checker not entering from bar.
     1 - (1 - n/6)^2, where n is number of closed points in op home.
   */
  I_ENTER2,

  I_TIMING,
  
  I_BACKBONE,

  HALF_INPUTS };


#define NUM_INPUTS ( HALF_INPUTS * 2 )
#define NUM_RACE_INPUTS ( HALF_RACE_INPUTS * 2 )

static int anEscapes[ 0x1000 ];
static neuralnet nnContact, nnBPG, nnRace;
static unsigned char *pBearoff1 = NULL, *pBearoff2 = NULL;
static cache cEval;
volatile int fInterrupt = FALSE, fAction = FALSE;
void ( *fnAction )( void ) = NULL;
int fMove, fCubeOwner, fJacoby = 1, fCrawford;
int fPostCrawford, nMatchTo, anScore[ 2 ], fBeavers = 1;
int nCube;
float rCubeX = 2.0/3.0;

static cubeinfo ciBasic = { 1, 0, 0, { 1.0, 1.0, 1.0, 1.0 } };

static float arGammonPrice[ 4 ] = { 1.0, 1.0, 1.0, 1.0 };

static evalcontext ecBasic = { 0, 0, 0, 0, FALSE };

typedef struct _evalcache {
    unsigned char auchKey[ 10 ];
    int nEvalContext;
    positionclass pc;
    float ar[ NUM_OUTPUTS ];
} evalcache;

static void ComputeTable( void ) {

    int i, c, n0, n1;

    for( i = 0; i < 0x1000; i++ ) {
	c = 0;
	
	for( n0 = 0; n0 <= 5; n0++ )
	    for( n1 = 0; n1 <= n0; n1++ )
		if( !( i & ( 1 << ( n0 + n1 + 1 ) ) ) &&
		    !( ( i & ( 1 << n0 ) ) && ( i & ( 1 << n1 ) ) ) )
		    c += ( n0 == n1 ) ? 1 : 2;
	
	anEscapes[ i ] = c;
    }
}

static int EvalCacheCompare( evalcache *p0, evalcache *p1 ) {

    return !EqualKeys( p0->auchKey, p1->auchKey ) ||
	p0->nEvalContext != p1->nEvalContext ||
	p0->pc != p1->pc;
}

static long EvalCacheHash( evalcache *pec ) {

    long l = 0;
    int i;
    
    l = ( pec->nEvalContext << 9 ) ^ ( pec->pc << 4 );

    for( i = 0; i < 10; i++ )
	l = ( ( l << 8 ) % 8388593 ) ^ pec->auchKey[ i ];

    return l;    
}

/* Open a file for reading with the search path "(szDir):.:(PKGDATADIR)". */
static int PathOpen( char *szFile, char *szDir ) {

    int h, idFirstError = 0;
    char szPath[ PATH_MAX ];
    
    if( szDir ) {
	sprintf( szPath, "%s/%s", szDir, szFile );
	if( ( h = open( szPath, O_RDONLY ) ) >= 0 )
	    return h;

	/* Try to report the more serious error (ENOENT is less
           important than, say, EACCESS). */
	if( errno != ENOENT )
	    idFirstError = errno;
    }

    if( ( h = open( szFile, O_RDONLY ) ) >= 0 )
	return h;

    if( !idFirstError && errno != ENOENT )
	idFirstError = errno;

    sprintf( szPath, PKGDATADIR "/%s", szFile );
    if( ( h = open( szPath, O_RDONLY ) ) >= 0 )
	return h;

    if( idFirstError )
	errno = idFirstError;

    return -1;
}

extern int EvalInitialise( char *szWeights, char *szWeightsBinary,
			   char *szDatabase, char *szDir ) {
    FILE *pfWeights;
    int h, fReadWeights = FALSE;
    char szFileVersion[ 16 ];
    float r;
    static int fInitialised = FALSE;

    if( !fInitialised ) {
	if( CacheCreate( &cEval, 8192, (cachecomparefunc) EvalCacheCompare ) )
	    return -1;
	    
	ComputeTable();
    }
    
    if( szDatabase ) {
	if( ( h = PathOpen( szDatabase, szDir ) ) < 0 ) {
	    perror( szDatabase );
	    return -1;
	}

#if HAVE_MMAP
	if( !( pBearoff1 = mmap( NULL, 54264 * 32 * 2 + 924 * 924 * 2,
				 PROT_READ, MAP_SHARED, h, 0 ) ) ) {
#endif
	    if( !( pBearoff1 = malloc( 54264 * 32 * 2 + 924 * 924 * 2 ) ) ) {
		perror( "malloc" );
		return -1;
	    }

	    if( read( h, pBearoff1, 54264 * 32 * 2 + 924 * 924 * 2 ) < 0 ) {
		perror( szDatabase );
		free( pBearoff1 );

		return -1;
	    }

	    close( h );
#if HAVE_MMAP
	}
#endif
	pBearoff2 = pBearoff1 + 54264 * 32 * 2;
    }

    if( szWeightsBinary &&
	( h = PathOpen( szWeightsBinary, szDir ) ) >= 0 &&
	( pfWeights = fdopen( h, "rb" ) ) ) {
	if( fread( &r, sizeof r, 1, pfWeights ) < 1 ||
	    r != WEIGHTS_MAGIC_BINARY ||
	    fread( &r, sizeof r, 1, pfWeights ) < 1 ||
	    r != WEIGHTS_VERSION_BINARY )
	    fprintf( stderr, "%s: Invalid weights file\n",
		     szWeightsBinary );
	else {
	    if( !( fReadWeights =
		   !NeuralNetLoadBinary(&nnContact, pfWeights ) &&
		   !NeuralNetLoadBinary(&nnRace, pfWeights ) ) ) {
		perror( szWeightsBinary );
	    }
	    
	    if( fReadWeights ) {
		if( NeuralNetLoadBinary( &nnBPG, pfWeights ) == -1 ) {
		    /* HACK */
		    nnBPG.cInput = 0;
		}
	    }
	    
	    fclose( pfWeights );
	}
    }

    if( !fReadWeights && szWeights ) {
	if( ( h = PathOpen( szWeights, szDir ) ) < 0 ||
	    !( pfWeights = fdopen( h, "r" ) ) )
	    perror( szWeights );

	if( pfWeights ) {
	    if( fscanf( pfWeights, "GNU Backgammon %15s\n",
			szFileVersion ) != 1 ||
		strcmp( szFileVersion, WEIGHTS_VERSION ) )
		fprintf( stderr, "%s: Invalid weights file\n", szWeights );
	    else {
		if( !( fReadWeights = !NeuralNetLoad( &nnContact,
						      pfWeights ) &&
		       !NeuralNetLoad( &nnRace, pfWeights ) ) )
		    perror( szWeights );

		if( fReadWeights ) {
		  if( NeuralNetLoad( &nnBPG, pfWeights ) == -1 ) {
		    /* HACK */
		    nnBPG.cInput = 0;
		  }
		}
		
		fclose( pfWeights );
	    }
	}
    }

    if( fReadWeights ) {
	if( nnContact.cInput != NUM_INPUTS ||
	    nnContact.cOutput != NUM_OUTPUTS )
	    NeuralNetResize( &nnContact, NUM_INPUTS, nnContact.cHidden,
			     NUM_OUTPUTS );
	
	if( nnRace.cInput != NUM_RACE_INPUTS ||
	    nnRace.cOutput != NUM_OUTPUTS )
	    NeuralNetResize( &nnRace, NUM_RACE_INPUTS, nnRace.cHidden,
			     NUM_OUTPUTS );	
    } else {
	/* FIXME eval.c shouldn't write to stdout */
	puts( "Creating random neural net weights..." );
	NeuralNetCreate( &nnContact, NUM_INPUTS, 128 /* FIXME */,
			 NUM_OUTPUTS, 0.1, 1.0 );
	
	NeuralNetCreate( &nnBPG, NUM_INPUTS, 128 /* FIXME */,
			 NUM_OUTPUTS, 0.1, 1.0 );
	
	NeuralNetCreate( &nnRace, NUM_RACE_INPUTS, 128 /* FIXME */,
			 NUM_OUTPUTS, 0.1, 1.0 );
    }
    
    return 0;
}

extern int EvalSave( char *szWeights ) {
    
  FILE *pfWeights;
    
  if( !( pfWeights = fopen( szWeights, "w" ) ) )
    return -1;
    
  fprintf( pfWeights, "GNU Backgammon %s\n", WEIGHTS_VERSION );

  NeuralNetSave( &nnContact, pfWeights );
  NeuralNetSave( &nnRace, pfWeights );
    
  if( nnBPG.cInput != 0 ) { /* HACK */
    NeuralNetSave( &nnBPG, pfWeights );
  }
    
  fclose( pfWeights );

  return 0;
}

static int Escapes( int anBoard[ 25 ], int n ) {
    
    int i, af = 0;
    
    for( i = 0; i < 12 && i < n; i++ )
	if( anBoard[ 24 - n + i ] > 1 )
	    af |= ( 1 << i );
    
    return anEscapes[ af ];
}

/* Calculates the inputs for one player only.  Returns 0 for contact
   positions, 1 for races. */
static int
CalculateHalfInputs( int anBoard[ 25 ],
		     int anBoardOpp[ 25 ],
		     float afInput[] ) {

  int i, j, k, l, nOppBack, n, aHit[ 39 ], nBoard;
    
  /* aanCombination[n] -
     How many ways to hit from a distance of n pips.
     Each number is an index into aIntermediate below. 
  */
  static int aanCombination[ 24 ][ 5 ] = {
    {  0, -1, -1, -1, -1 }, /*  1 */
    {  1,  2, -1, -1, -1 }, /*  2 */
    {  3,  4,  5, -1, -1 }, /*  3 */
    {  6,  7,  8,  9, -1 }, /*  4 */
    { 10, 11, 12, -1, -1 }, /*  5 */
    { 13, 14, 15, 16, 17 }, /*  6 */
    { 18, 19, 20, -1, -1 }, /*  7 */
    { 21, 22, 23, 24, -1 }, /*  8 */
    { 25, 26, 27, -1, -1 }, /*  9 */
    { 28, 29, -1, -1, -1 }, /* 10 */
    { 30, -1, -1, -1, -1 }, /* 11 */
    { 31, 32, 33, -1, -1 }, /* 12 */
    { -1, -1, -1, -1, -1 }, /* 13 */
    { -1, -1, -1, -1, -1 }, /* 14 */
    { 34, -1, -1, -1, -1 }, /* 15 */
    { 35, -1, -1, -1, -1 }, /* 16 */
    { -1, -1, -1, -1, -1 }, /* 17 */
    { 36, -1, -1, -1, -1 }, /* 18 */
    { -1, -1, -1, -1, -1 }, /* 19 */
    { 37, -1, -1, -1, -1 }, /* 20 */
    { -1, -1, -1, -1, -1 }, /* 21 */
    { -1, -1, -1, -1, -1 }, /* 22 */
    { -1, -1, -1, -1, -1 }, /* 23 */
    { 38, -1, -1, -1, -1 } /* 24 */
  };
    
  /* One way to hit */ 
  static struct {
    /* if true, all intermediate points (if any) are required;
       if false, one of two intermediate points are required.
       Set to true for a direct hit, but that can be checked with
       nFaces == 1,
    */
    int fAll;

    /* Intermediate points required */
    int anIntermediate[ 3 ];

    /* Number of faces used in hit (1 to 4) */
    int nFaces;

    /* Number of pips used to hit */
    int nPips;
  } *pi,

      /* All ways to hit */
	
      aIntermediate[ 39 ] = {
	{ 1, { 0, 0, 0 }, 1, 1 }, /*  0: 1x hits 1 */
	{ 1, { 0, 0, 0 }, 1, 2 }, /*  1: 2x hits 2 */
	{ 1, { 1, 0, 0 }, 2, 2 }, /*  2: 11 hits 2 */
	{ 1, { 0, 0, 0 }, 1, 3 }, /*  3: 3x hits 3 */
	{ 0, { 1, 2, 0 }, 2, 3 }, /*  4: 21 hits 3 */
	{ 1, { 1, 2, 0 }, 3, 3 }, /*  5: 11 hits 3 */
	{ 1, { 0, 0, 0 }, 1, 4 }, /*  6: 4x hits 4 */
	{ 0, { 1, 3, 0 }, 2, 4 }, /*  7: 31 hits 4 */
	{ 1, { 2, 0, 0 }, 2, 4 }, /*  8: 22 hits 4 */
	{ 1, { 1, 2, 3 }, 4, 4 }, /*  9: 11 hits 4 */
	{ 1, { 0, 0, 0 }, 1, 5 }, /* 10: 5x hits 5 */
	{ 0, { 1, 4, 0 }, 2, 5 }, /* 11: 41 hits 5 */
	{ 0, { 2, 3, 0 }, 2, 5 }, /* 12: 32 hits 5 */
	{ 1, { 0, 0, 0 }, 1, 6 }, /* 13: 6x hits 6 */
	{ 0, { 1, 5, 0 }, 2, 6 }, /* 14: 51 hits 6 */
	{ 0, { 2, 4, 0 }, 2, 6 }, /* 15: 42 hits 6 */
	{ 1, { 3, 0, 0 }, 2, 6 }, /* 16: 33 hits 6 */
	{ 1, { 2, 4, 0 }, 3, 6 }, /* 17: 22 hits 6 */
	{ 0, { 1, 6, 0 }, 2, 7 }, /* 18: 61 hits 7 */
	{ 0, { 2, 5, 0 }, 2, 7 }, /* 19: 52 hits 7 */
	{ 0, { 3, 4, 0 }, 2, 7 }, /* 20: 43 hits 7 */
	{ 0, { 2, 6, 0 }, 2, 8 }, /* 21: 62 hits 8 */
	{ 0, { 3, 5, 0 }, 2, 8 }, /* 22: 53 hits 8 */
	{ 1, { 4, 0, 0 }, 2, 8 }, /* 23: 44 hits 8 */
	{ 1, { 2, 4, 6 }, 4, 8 }, /* 24: 22 hits 8 */
	{ 0, { 3, 6, 0 }, 2, 9 }, /* 25: 63 hits 9 */
	{ 0, { 4, 5, 0 }, 2, 9 }, /* 26: 54 hits 9 */
	{ 1, { 3, 6, 0 }, 3, 9 }, /* 27: 33 hits 9 */
	{ 0, { 4, 6, 0 }, 2, 10 }, /* 28: 64 hits 10 */
	{ 1, { 5, 0, 0 }, 2, 10 }, /* 29: 55 hits 10 */
	{ 0, { 5, 6, 0 }, 2, 11 }, /* 30: 65 hits 11 */
	{ 1, { 6, 0, 0 }, 2, 12 }, /* 31: 66 hits 12 */
	{ 1, { 4, 8, 0 }, 3, 12 }, /* 32: 44 hits 12 */
	{ 1, { 3, 6, 9 }, 4, 12 }, /* 33: 33 hits 12 */
	{ 1, { 5, 10, 0 }, 3, 15 }, /* 34: 55 hits 15 */
	{ 1, { 4, 8, 12 }, 4, 16 }, /* 35: 44 hits 16 */
	{ 1, { 6, 12, 0 }, 3, 18 }, /* 36: 66 hits 18 */
	{ 1, { 5, 10, 15 }, 4, 20 }, /* 37: 55 hits 20 */
	{ 1, { 6, 12, 18 }, 4, 24 }  /* 38: 66 hits 24 */
      };

  /* aaRoll[n] - All ways to hit with the n'th roll
     Each entry is an index into aIntermediate above.
  */
    
  static int aaRoll[ 21 ][ 4 ] = {
    {  0,  2,  5,  9 }, /* 11 */
    {  0,  1,  4, -1 }, /* 21 */
    {  1,  8, 17, 24 }, /* 22 */
    {  0,  3,  7, -1 }, /* 31 */
    {  1,  3, 12, -1 }, /* 32 */
    {  3, 16, 27, 33 }, /* 33 */
    {  0,  6, 11, -1 }, /* 41 */
    {  1,  6, 15, -1 }, /* 42 */
    {  3,  6, 20, -1 }, /* 43 */
    {  6, 23, 32, 35 }, /* 44 */
    {  0, 10, 14, -1 }, /* 51 */
    {  1, 10, 19, -1 }, /* 52 */
    {  3, 10, 22, -1 }, /* 53 */
    {  6, 10, 26, -1 }, /* 54 */
    { 10, 29, 34, 37 }, /* 55 */
    {  0, 13, 18, -1 }, /* 61 */
    {  1, 13, 21, -1 }, /* 62 */
    {  3, 13, 25, -1 }, /* 63 */
    {  6, 13, 28, -1 }, /* 64 */
    { 10, 13, 30, -1 }, /* 65 */
    { 13, 31, 36, 38 } /* 66 */
  };

  /* Points we want to make, in order of importance */
/*  static int anPoint[ 6 ] = { 5, 4, 3, 6, 2, 7 }; */

  /* One roll stat */
  
  struct {
    /* count of pips this roll hits */
    int nPips;
      
    /* number of chequers this roll hits */
    int nChequers;
  } aRoll[ 21 ];
    
  /* FIXME consider -1.0 instead of 0.0, since null inputs do not
       contribute to learning!

       Tried that (Joseph). Does not work well.
  */
    
  /* Points */
  for( i = 0; i < 25; i++ ) {
    int nc = anBoard[ i ];
      
    afInput[ i * 4 + 0 ] = nc == 1;
    afInput[ i * 4 + 1 ] = nc >= 2;
    afInput[ i * 4 + 2 ] = nc >= 3;
    afInput[ i * 4 + 3 ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
  }

  /* Bar */
  afInput[ 24 * 4 + 0 ] = anBoard[ 24 ] >= 1;

  /* Men off */
  {
    int menOff = 15;
      
    for(i = 0; i < 25; i++ ) {
      menOff -= anBoard[i];
    }

    if( menOff > 10 ) {
      afInput[ I_OFF1 ] = 1.0;
      afInput[ I_OFF2 ] = 1.0;
      afInput[ I_OFF3 ] = ( menOff - 10 ) / 5.0;
    } else if( menOff > 5 ) {
      afInput[ I_OFF1 ] = 1.0;
      afInput[ I_OFF2 ] = ( menOff - 5 ) / 5.0;
      afInput[ I_OFF3 ] = 0.0;
    } else {
      afInput[ I_OFF1 ] = menOff / 5.0;
      afInput[ I_OFF2 ] = 0.0;
      afInput[ I_OFF3 ] = 0.0;
    }
  }
    
  for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
    if( anBoardOpp[nOppBack] ) {
      break;
    }
  }
    
  nOppBack = 23 - nOppBack;

  n = 0;
  for( i = nOppBack + 1; i < 25; i++ )
    if( anBoard[ i ] )
      n += ( i + 1 - nOppBack ) * anBoard[ i ];

  if( !n ) {
    /* No contact */

    return 1;
  }
    
  afInput[ I_BREAK_CONTACT ] = n / (15 + 152.0);

  {
    /* timing in pips */
    int t = 0;

    int no = 0;
      
    t += 24 * anBoard[24];
    no += anBoard[24];
      
    for( i = 23;  i >= 18; --i ) {
      if( anBoard[i] != 2 ) {
	int n = ((anBoard[i] > 2) ? (anBoard[i] - 2) : 1);
	no += n;
	t += i * n;
      }
    }

    for( i = 17;  i >= 6; --i ) {
      if( anBoard[i] > 2 ) {
	no += (anBoard[i] - 2);
	  
	t += i * (anBoard[i] - 2);
      }
    }

    for( i = 5;  i >= 0; --i ) {
      if( anBoard[i] > 2 ) {
	t += i * (anBoard[i] - 2);
	no += (anBoard[i] - 2);
      } else if( anBoard[i] < 2 ) {
	int n = (2 - anBoard[i]);

	if( no >= n ) {
	  t -= i * n;
	  no -= n;
	}
      }
    }

    if( t < 0 ) {
      t = 0;
    }

    afInput[ I_TIMING ] = t / 200.0;
  }


  /* Back chequer */

  {
    int nBack;
    
    for( nBack = 24; nBack >= 0; --nBack ) {
      if( anBoard[nBack] ) {
	break;
      }
    }
    
    afInput[ I_BACK_CHEQUER ] = nBack / 24.0;

    /* Back anchor */

    for( i = nBack == 24 ? 23 : nBack; i >= 0; --i ) {
      if( anBoard[i] >= 2 ) {
	break;
      }
    }
    
    afInput[ I_BACK_ANCHOR ] = i / 24.0;
    
    /* Forward anchor */

    n = 0;
    for( j = 18; j <= i; ++j ) {
      if( anBoard[j] >= 2 ) {
	n = 24 - j;
	break;
      }
    }

    if( n == 0 ) {
      for( j = 17; j >= 12 ; --j ) {
	if( anBoard[j] >= 2 ) {
	  n = 24 - j;
	  break;
	}
      }
    }
	
    afInput[ I_FORWARD_ANCHOR ] = n == 0 ? 2.0 : n / 6.0;
  }
    

  /* Piploss */
    
  nBoard = 0;
  for( i = 0; i < 6; i++ )
    if( anBoard[ i ] )
      nBoard++;

  for( i = 0; i < 39; i++ )
    aHit[ i ] = 0;
    
    /* for every point we'd consider hitting a blot on, */
    
  for( i = ( nBoard > 2 ) ? 23 : 21; i >= 0; i-- )
    /* if there's a blot there, then */
      
    if( anBoardOpp[ i ] == 1 )
      /* for every point beyond */
	
      for( j = 24 - i; j < 25; j++ )
	/* if we have a hitter and are willing to hit */
	  
	if( anBoard[ j ] && !( j < 6 && anBoard[ j ] == 2 ) )
	  /* for every roll that can hit from that point */
	    
	  for( n = 0; n < 5; n++ ) {
	    if( aanCombination[ j - 24 + i ][ n ] == -1 )
	      break;

	    /* find the intermediate points required to play */
	      
	    pi = aIntermediate + aanCombination[ j - 24 + i ][ n ];

	    if( pi->fAll ) {
	      /* if nFaces is 1, there are no intermediate points */
		
	      if( pi->nFaces > 1 ) {
		/* all the intermediate points are required */
		  
		for( k = 0; k < 3 && pi->anIntermediate[k] > 0; k++ )
		  if( anBoardOpp[ i - pi->anIntermediate[ k ] ] > 1 )
		    /* point is blocked; look for other hits */
		    goto cannot_hit;
	      }
	    } else {
	      /* either of two points are required */
		
	      if( anBoardOpp[ i - pi->anIntermediate[ 0 ] ] > 1
		  && anBoardOpp[ i - pi->anIntermediate[ 1 ] ] > 1 ) {
				/* both are blocked; look for other hits */
		goto cannot_hit;
	      }
	    }
	      
	    /* enter this shot as available */
	      
	    aHit[ aanCombination[ j - 24 + i ][ n ] ] |= 1 << j;
	    cannot_hit:
	  }

  for( i = 0; i < 21; i++ )
    aRoll[ i ].nPips = aRoll[ i ].nChequers = 0;
    
  if( !anBoard[ 24 ] ) {
    /* we're not on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = -1; /* (hitter used) */
	
      /* for each way that roll hits, */
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;

	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  for( k = 23; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* select the most advanced blot; if we still have
		 a chequer that can hit there */
		      
	      if( n != k || anBoard[ k ] > 1 )
		aRoll[ i ].nChequers++;

	      n = k;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
		
	      /* if rolling doubles, check for multiple
		 direct shots */
		      
	      if( aaRoll[ i ][ 3 ] >= 0 &&
		  aHit[ r ] & ~( 1 << k ) )
		aRoll[ i ].nChequers++;
			    
	      break;
	    }
	  }
	} else {
	  /* indirect shot */
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;

	  /* find the most advanced hitter */
	    
	  for( k = 23; k >= 0; k-- )
	    if( aHit[ r ] & ( 1 << k ) )
	      break;

	  if( k - pi->nPips + 1 > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = k - pi->nPips + 1;

	  /* check for blots hit on intermediate points */
		    
	  for( l = 0; l < 3 && pi->anIntermediate[ l ] > 0; l++ )
	    if( anBoardOpp[ 23 - k + pi->anIntermediate[ l ] ] == 1 ) {
		
	      aRoll[ i ].nChequers++;
	      break;
	    }
	}
      }
    }
  } else if( anBoard[ 24 ] == 1 ) {
    /* we have one on the bar; for each roll, */
      
    for( i = 0; i < 21; i++ ) {
      n = 0; /* (free to use either die to enter) */
	
      for( j = 0; j < 4; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( r < 0 )
	  break;
		
	if( !aHit[ r ] )
	  continue;

	pi = aIntermediate + r;
		
	if( pi->nFaces == 1 ) {
	  /* direct shot */
	  
	  for( k = 24; k > 0; k-- ) {
	    if( aHit[ r ] & ( 1 << k ) ) {
	      /* if we need this die to enter, we can't hit elsewhere */
	      
	      if( n && k != 24 )
		break;
			    
	      /* if this isn't a shot from the bar, the
		 other die must be used to enter */
	      
	      if( k != 24 ) {
		int npip = aIntermediate[aaRoll[ i ][ 1 - j ] ].nPips;
		
		if( anBoardOpp[npip - 1] > 1 )
		  break;
				
		n = 1;
	      }

	      aRoll[ i ].nChequers++;

	      if( k - pi->nPips + 1 > aRoll[ i ].nPips )
		aRoll[ i ].nPips = k - pi->nPips + 1;
	    }
	  }
	} else {
	  /* indirect shot -- consider from the bar only */
	  if( !( aHit[ r ] & ( 1 << 24 ) ) )
	    continue;
		    
	  if( !aRoll[ i ].nChequers )
	    aRoll[ i ].nChequers = 1;
		    
	  if( 25 - pi->nPips > aRoll[ i ].nPips )
	    aRoll[ i ].nPips = 25 - pi->nPips;
		    
	  /* check for blots hit on intermediate points */
	  for( k = 0; k < 3 && pi->anIntermediate[ k ] > 0; k++ )
	    if( anBoardOpp[ pi->anIntermediate[ k ] + 1 ] == 1 ) {
		
	      aRoll[ i ].nChequers++;
	      break;
	    }
	}
      }
    }
  } else {
    /* we have more than one on the bar --
       count only direct shots from point 24 */
      
    for( i = 0; i < 21; i++ ) {
      /* for the first two ways that hit from the bar */
	
      for( j = 0; j < 2; j++ ) {
	int r = aaRoll[ i ][ j ];
	
	if( !( aHit[r] & ( 1 << 24 ) ) )
	  continue;

	pi = aIntermediate + r;

	/* only consider direct shots */
	
	if( pi->nFaces != 1 )
	  continue;

	aRoll[ i ].nChequers++;

	if( 25 - pi->nPips > aRoll[ i ].nPips )
	  aRoll[ i ].nPips = 25 - pi->nPips;
      }
    }
  }

  {
    int np = 0;
    int n1 = 0;
    int n2 = 0;
      
    for(i = 0; i < 21; i++) {
      int w = aaRoll[i][3] > 0 ? 1 : 2;
      int nc = aRoll[i].nChequers;
	
      np += aRoll[i].nPips * w;
	
      if( nc > 0 ) {
	n1 += w;

	if( nc > 1 ) {
	  n2 += w;
	}
      }
    }

    afInput[ I_PIPLOSS ] = np / ( 12.0 * 36.0 );
      
    afInput[ I_P1 ] = n1 / 36.0;
    afInput[ I_P2 ] = n2 / 36.0;
  }

  afInput[ I_BACKESCAPES ] = Escapes( anBoard, 23 - nOppBack ) / 36.0;

  for( n = 36, i = 15; i < 24 - nOppBack; i++ )
    if( ( j = Escapes( anBoard, i ) ) < n )
      n = j;

  afInput[ I_ACONTAIN ] = ( 36 - n ) / 36.0;
  afInput[ I_ACONTAIN2 ] = afInput[ I_ACONTAIN ] * afInput[ I_ACONTAIN ];

  if( nOppBack < 0 ) {
    /* restart loop, point 24 should not be included */
    i = 15;
    n = 36;
  }
    
  for( ; i < 24; i++ )
    if( ( j = Escapes( anBoard, i ) ) < n )
      n = j;

    
  afInput[ I_CONTAIN ] = ( 36 - n ) / 36.0;
  afInput[ I_CONTAIN2 ] = afInput[ I_CONTAIN ] * afInput[ I_CONTAIN ];
    
  for( n = 0, i = 6; i < 25; i++ )
    if( anBoard[ i ] )
      n += ( i - 5 ) * anBoard[ i ] * Escapes( anBoardOpp, i );

  afInput[ I_MOBILITY ] = n / 3600.00;

  j = 0;
  n = 0; 
  for(i = 0; i < 25; i++ ) {
    int ni = anBoard[ i ];
      
    if( ni ) {
      j += ni;
      n += i * ni;
    }
  }

  if( j ) {
    n = (n + j - 1) / j;
  }

  j = 0;
  for(k = 0, i = n + 1; i < 25; i++ ) {
    int ni = anBoard[ i ];

    if( ni ) {
      j += ni;
      k += ni * ( i - n ) * ( i - n );
    }
  }

  if( j ) {
    k = (k + j - 1) / j;
  }

  afInput[ I_MOMENT2 ] = k / 400.0;

  if( anBoard[ 24 ] > 0 ) {
    int loss = 0;
    int two = anBoard[ 24 ] > 1;
      
    for(i = 0; i < 6; ++i) {
      if( anBoardOpp[ i ] > 1 ) {
	/* any double loses */
	  
	loss += 4*(i+1);

	for(j = i+1; j < 6; ++j) {
	  if( anBoardOpp[ j ] > 1 ) {
	    loss += 2*(i+j+2);
	  } else {
	    if( two ) {
	      loss += 2*(i+1);
	    }
	  }
	}
      } else {
	if( two ) {
	  for(j = i+1; j < 6; ++j) {
	    if( anBoardOpp[ j ] > 1 ) {
	      loss += 2*(j+1);
	    }
	  }
	}
      }
    }
      
    afInput[ I_ENTER ] = loss / (36.0 * (49.0/6.0));
  } else {
    afInput[ I_ENTER ] = 0.0;
  }

  n = 0;
  for(i = 0; i < 6; i++ ) {
    n += anBoardOpp[ i ] > 1;
  }
    
  afInput[ I_ENTER2 ] = ( 36 - ( n - 6 ) * ( n - 6 ) ) / 36.0; 

  {
    int pa = -1;
    int w = 0;
    int tot = 0;
    int np;
    
    for(np = 23; np > 0; --np) {
      if( anBoard[np] >= 2 ) {
	if( pa == -1 ) {
	  pa = np;
	  continue;
	}

	{
	  int d = pa - np;
	  int c = 0;
	
	  if( d <= 6 ) {
	    c = 11;
	  } else if( d <= 11 ) {
	    c = 13 - d;
	  }

	  w += c * anBoard[pa];
	  tot += anBoard[pa];
	}
      }
    }

    if( tot ) {
      afInput[I_BACKBONE] = 1 - (w / (tot * 11.0));
    } else {
      afInput[I_BACKBONE] = 0;
    }
  }
 
  return 0;
}

static void 
CalculateRaceInputs(int anBoard[2][25], float inputs[])
{
  unsigned int side;
  
  for(side = 0; side < 2; ++side) {
    const int* const board = anBoard[side];
    float* const afInput = inputs + side * HALF_RACE_INPUTS;
    int i;
    
    /* assert( board[24] == 0 ); */
    /* assert( board[23] == 0 ); */
    
    /* Points */
    for(i = 0; i < 23; ++i) {
      unsigned int const nc = board[ i ];

      unsigned int k = i * 4;
      
      afInput[ k++ ] = nc == 1;
      afInput[ k++ ] = nc >= 2;
      afInput[ k++ ] = nc >= 3;
      afInput[ k ] = nc > 3 ? ( nc - 3 ) / 2.0 : 0.0;
    }

    /* Men off */
    {
      int menOff = 15;
      int i;
      
      for(i = 0; i < 25; ++i) {
	menOff -= board[i];
      }

      {
	int k;
	for(k = 0; k < 14; ++k) {
	  afInput[ RI_OFF + k ] = menOff > k ? 1.0 : 0.0;
	}
      }
    }

    {
      unsigned int nCross = 0;
      unsigned int np = 0;
      unsigned int k;
      unsigned int i;
      
      for(k = 1; k < 4; ++k) {
      
	for(i = 6*k; i < 6*k + 6; ++i) {
	  unsigned int const nc = board[i];

	  if( nc ) {
	    nCross += nc * k;

	    np += nc * (i+1);
	  }
	}
      }
      
      afInput[RI_NCROSS] = nCross / 10.0;
      
      afInput[RI_OPIP] = np / 50.0;

      for(i = 0; i < 6; ++i) {
	unsigned int const nc = board[i];

	if( nc ) {
	  np += nc * (i+1);
	}
      }
      
      afInput[RI_PIP] = np / 100.0;
    }
  }
}

static int
isRace(int anBoard[2][25])
{
  int nOppBack;
  int i;
  
  for(nOppBack = 24; nOppBack >= 0; --nOppBack) {
    if( anBoard[1][nOppBack] ) {
      break;
    }
  }
    
  nOppBack = 23 - nOppBack;

  for(i = nOppBack + 1; i < 25; i++ ) {
    if( anBoard[0][i] ) {
      return 0;
    }
  }

  return 1;
}

/* Calculates neural net inputs from the board position.
   Returns 0 for contact positions, 1 for races. */

int
CalculateInputs(int anBoard[2][25], float arInput[])
{
  if( isRace(anBoard) ) {
    CalculateRaceInputs(anBoard, arInput);
    
    return 1;
  } else {

    float ar[ HALF_INPUTS ];
    int i, l;
    
    CalculateHalfInputs( anBoard[ 1 ], anBoard[ 0 ], ar );

    l = HALF_INPUTS - 1;
     
    for( i = l; i >= 0; i-- )
      arInput[ i << 1 ] = ar[ i ];

    CalculateHalfInputs( anBoard[ 0 ], anBoard[ 1 ], ar );
    
    for( i = l; i >= 0; i-- )
      arInput[ ( i << 1 ) | 1 ] = ar[ i ];
  }

  return 0;
}

extern void swap( int *p0, int *p1 ) {
    int n = *p0;

    *p0 = *p1;
    *p1 = n;
}

extern void SwapSides( int anBoard[ 2 ][ 25 ] ) {

    int i, n;

    for( i = 0; i < 25; i++ ) {
	n = anBoard[ 0 ][ i ];
	anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ];
	anBoard[ 1 ][ i ] = n;
    }
}

extern void SanityCheck( int anBoard[ 2 ][ 25 ], float arOutput[] ) {

    int i, j, ac[ 2 ], anBack[ 2 ], fContact;

    ac[ 0 ] = ac[ 1 ] = anBack[ 0 ] = anBack[ 1 ] = 0;
    
    for( i = 0; i < 25; i++ )
	for( j = 0; j < 2; j++ )
	    if( anBoard[ j ][ i ] ) {
		anBack[ j ] = i;
		ac[ j ] += anBoard[ j ][ i ];
	    }

    fContact = anBack[ 0 ] + anBack[ 1 ] >= 24;
    
    if( ac[ 0 ] < 15 )
	/* Opponent has borne off; no gammons or backgammons possible */
	arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;
    else if( !fContact && anBack[ 0 ] < 18 )
	/* Opponent is out of home board; no backgammons possible */
	arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

    if( ac[ 1 ] < 15 )
	/* Player has borne off; no gammons or backgammons possible */
	arOutput[ OUTPUT_LOSEGAMMON ] = arOutput[ OUTPUT_LOSEBACKGAMMON ] =
	    0.0;
    else if( !fContact && anBack[ 1 ] < 18 )
	/* Player is out of home board; no backgammons possible */
	arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

    /* gammons must be less than wins */    
    if( arOutput[ OUTPUT_WINGAMMON ] > arOutput[ OUTPUT_WIN ] ) {
      arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_WIN ];
    }

    {
      float lose = 1.0 - arOutput[ OUTPUT_WIN ];
      if( arOutput[ OUTPUT_LOSEGAMMON ] > lose ) {
	arOutput[ OUTPUT_LOSEGAMMON ] = lose;
      }
    }

    /* Backgammons cannot exceed gammons */
    if( arOutput[ OUTPUT_WINBACKGAMMON ] > arOutput[ OUTPUT_WINGAMMON ] )
	arOutput[ OUTPUT_WINBACKGAMMON ] = arOutput[ OUTPUT_WINGAMMON ];
    
    if( arOutput[ OUTPUT_LOSEBACKGAMMON ] > arOutput[ OUTPUT_LOSEGAMMON ] )
	arOutput[ OUTPUT_LOSEBACKGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ];
}

#if 0
static int
barPrimeBackGame(int anBoard[ 2 ][ 25 ])
{
  int side, i, k;
  
  /* on bar */
  if( anBoard[0][24] || anBoard[1][24] ) {
    return 1;
  }

  /* two or more anchors in opponent home */
  
  for(side = 0; side < 2; ++side) {
    unsigned int n = 0;
    for(i = 18; i < 24; ++i) {
      if( anBoard[side][i] > 1 ) {
	++n;
      }
    }
    if( n > 1 ) {
      return 1;
    }
  }

  for(side = 0; side < 2; ++side) {
    int first;
    
    for(first = 23; first >= 0; --first) {
      if( anBoard[side][first] > 0 ) {
	break;
      }
    }
    {                                                  assert( first >= 0 ); }

    for(i = 24 - first; i < 20; ++i) {
      unsigned int n = 0;
      for(k = i; k <= i+3; ++k) {
	if( anBoard[1-side][k] > 1 ) {
	  ++n;
	}
      }

      if( n == 4 ) {
	/* 4 prime */
	return 1;
      }

      if( n == 3 && i < 20 && anBoard[1-side][i+4] > 1 ) {
	/* broken 5 prime */
	return 1;
      }
    }
  }
  return 0;
}
#endif

extern positionclass ClassifyPosition( int anBoard[ 2 ][ 25 ] ) {

    int i, nOppBack = -1, nBack = -1;
    
    for( i = 0; i < 25; i++ ) {
	if( anBoard[ 0 ][ i ] )
	    nOppBack = i;
	if( anBoard[ 1 ][ i ] )
	    nBack = i;
    }

    if( nBack < 0 || nOppBack < 0 )
	return CLASS_OVER;

    if( nBack + nOppBack > 22 ) {
      /* Disable BPG */
      
/*        if( barPrimeBackGame(anBoard) ) { */
/*  	return CLASS_BPG; */
/*        } */

      return CLASS_CONTACT;
    }
    else if( nBack > 5 || nOppBack > 5 || !pBearoff1 )
	return CLASS_RACE;

    if( PositionBearoff( anBoard[ 0 ] ) > 923 ||
	PositionBearoff( anBoard[ 1 ] ) > 923 )
	return CLASS_BEAROFF1;

    return CLASS_BEAROFF2;
}

static void EvalRace( int anBoard[ 2 ][ 25 ], float arOutput[] ) {
    
    float arInput[ NUM_INPUTS ];

    CalculateInputs( anBoard, arInput );
    
    NeuralNetEvaluate( &nnRace, arInput, arOutput );
}

static void EvalBPG( int anBoard[ 2 ][ 25 ], float arOutput[] )
{
  float arInput[ NUM_INPUTS ];

  CalculateInputs(anBoard, arInput);

  /* HACK - if not loaded, use contact */
  
  NeuralNetEvaluate(nnBPG.cInput != 0 ? &nnBPG : &nnContact,
		    arInput, arOutput);
}

static void EvalContact( int anBoard[ 2 ][ 25 ], float arOutput[] ) {
    
    float arInput[ NUM_INPUTS ];

    CalculateInputs( anBoard, arInput );
    
    NeuralNetEvaluate( &nnContact, arInput, arOutput );
}

static void EvalOver( int anBoard[ 2 ][ 25 ], float arOutput[] ) {

    int i, c;
    
    for( i = 0; i < 25; i++ )
	if( anBoard[ 0 ][ i ] )
	    break;

    if( i == 25 ) {
	/* opponent has no pieces on board; player has lost */
	arOutput[ OUTPUT_WIN ] = arOutput[ OUTPUT_WINGAMMON ] =
	    arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

	for( i = 0, c = 0; i < 25; i++ )
	    c += anBoard[ 1 ][ i ];

	if( c > 14 ) {
	    /* player still has all pieces on board; loses gammon */
	    arOutput[ OUTPUT_LOSEGAMMON ] = 1.0;

	    for( i = 18; i < 25; i++ )
		if( anBoard[ 1 ][ i ] ) {
		    /* player still has pieces in opponent's home board;
		       loses backgammon */
		    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 1.0;

		    return;
		}
	    
	    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

	    return;
	}

	arOutput[ OUTPUT_LOSEGAMMON ] =
	    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

	return;
    }
    
    for( i = 0; i < 25; i++ )
	if( anBoard[ 1 ][ i ] )
	    break;

    if( i == 25 ) {
	/* player has no pieces on board; wins */
	arOutput[ OUTPUT_WIN ] = 1.0;
	arOutput[ OUTPUT_LOSEGAMMON ] =
	    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

	for( i = 0, c = 0; i < 25; i++ )
	    c += anBoard[ 0 ][ i ];

	if( c > 14 ) {
	    /* opponent still has all pieces on board; win gammon */
	    arOutput[ OUTPUT_WINGAMMON ] = 1.0;

	    for( i = 18; i < 25; i++ )
		if( anBoard[ 0 ][ i ] ) {
		    /* opponent still has pieces in player's home board;
		       win backgammon */
		    arOutput[ OUTPUT_WINBACKGAMMON ] = 1.0;

		    return;
		}
	    
	    arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;

	    return;
	}

	arOutput[ OUTPUT_WINGAMMON ] =
	    arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0;
    }
}

static void EvalBearoff2( int anBoard[ 2 ][ 25 ], float arOutput[] ) {

    int n, nOpp;

    assert( pBearoff2 );
    
    nOpp = PositionBearoff( anBoard[ 0 ] );
    n = PositionBearoff( anBoard[ 1 ] );

    arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] =
	arOutput[ OUTPUT_WINBACKGAMMON ] =
	arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

    arOutput[ OUTPUT_WIN ] = ( pBearoff2[ ( n * 924 + nOpp ) << 1 ] |
	( pBearoff2[ ( ( n * 924 + nOpp ) << 1 ) | 1 ] << 8 ) ) / 65535.0;
}

extern unsigned long EvalBearoff1Full( int anBoard[ 2 ][ 25 ],
				       float arOutput[] ) {
    
    int i, j, n, nOpp, aaProb[ 2 ][ 32 ];
    unsigned long x;

    assert( pBearoff1 );
    
    nOpp = PositionBearoff( anBoard[ 0 ] );
    n = PositionBearoff( anBoard[ 1 ] );

    if( n > 38760 || nOpp > 38760 )
	/* a player has no chequers off; gammons are possible */
	EvalRace( anBoard, arOutput );
    else
	arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] =
	    arOutput[ OUTPUT_WINBACKGAMMON ] =
	    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;

    for( i = 0; i < 32; i++ )
	aaProb[ 0 ][ i ] = pBearoff1[ ( n << 6 ) | ( i << 1 ) ] +
	    ( pBearoff1[ ( n << 6 ) | ( i << 1 ) | 1 ] << 8 );

    for( i = 0; i < 32; i++ )
	aaProb[ 1 ][ i ] = pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) ] +
	    ( pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) | 1 ] << 8 );

    x = 0;
    for( i = 0; i < 32; i++ )
	for( j = i; j < 32; j++ )
	    x += aaProb[ 0 ][ i ] * aaProb[ 1 ][ j ];

    arOutput[ OUTPUT_WIN ] = x / ( 65535.0 * 65535.0 );

    x = 0;
    for( i = 0; i < 32; i++ )
	x += i * aaProb[ 1 ][ i ];

    /* return value is the expected number of rolls for opponent to bear off
       multiplied by 0xFFFF */
    return x;
}

extern void EvalBearoff1( int anBoard[ 2 ][ 25 ], float arOutput[] ) {

    EvalBearoff1Full( anBoard, arOutput );
}

static classevalfunc acef[ N_CLASSES ] = {
    EvalOver, EvalBearoff2, EvalBearoff1, EvalRace, EvalBPG, EvalContact
};

static int CompareRedEvalData( const void *p0, const void *p1 ) {

  return 
    ( ( ( RedEvalData * ) p0 ) -> rScore ) <
    ( ( ( RedEvalData * ) p1 ) -> rScore );
  

}

static int 
EvaluatePositionCache( int anBoard[ 2 ][ 25 ], float arOutput[],
                       cubeinfo *pci, evalcontext *pecx, int nPlies,
                       positionclass pc );

static int 
FindBestMovePlied( int anMove[ 8 ], int nDice0, int nDice1,
                   int anBoard[ 2 ][ 25 ], cubeinfo *pci, 
                   evalcontext *pec, int nPlies );

static int 
EvaluatePositionFull( int anBoard[ 2 ][ 25 ], float arOutput[],
                      cubeinfo *pci, evalcontext *pec, int nPlies,
                      positionclass pc ) {
	int i, n0, n1;

	if( pc > CLASS_PERFECT && nPlies > 0 ) {
		/* internal node; recurse */

		float ar[ NUM_OUTPUTS ];
		int anBoardNew[ 2 ][ 25 ];
		int anMove[ 8 ];
		cubeinfo ciOpp;

		for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] = 0.0;


		/* 
		 * If nPlies = 2 there are two branches:
		 * (1) Full 2-ply search: all 21 rolls evaluated.
		 * (2) Reduced 2-ply search: 
		 *     - evaluate all 21 rolls
		 *     - sort, take a reduced set, and evaluate only these at
		 *       1-ply.
		 */

		if ( ! ( ( nPlies == 2 ) && ( pec->nReduced > 0 ) ) ) {

			/* full search */
	 
			for( n0 = 1; n0 <= 6; n0++ )
				for( n1 = 1; n1 <= n0; n1++ ) {
					for( i = 0; i < 25; i++ ) {
						anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
						anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
					}
				
					if( fInterrupt ) {
						errno = EINTR;
						return -1;
					} else if( fAction )
						fnAction();
	      
					FindBestMovePlied( anMove, n0, n1, anBoardNew, pci, pec, 0 );
	      
					SwapSides( anBoardNew );

					SetCubeInfo ( &ciOpp, pci->nCube, pci->fCubeOwner, ! pci->fMove );
	      
					if( EvaluatePositionCache( anBoardNew, ar, &ciOpp, pec, nPlies - 1,
																		 pec->fRelativeAccuracy ? pc :
																		 ClassifyPosition( anBoardNew ) ) )
						return -1;
	      
					if( n0 == n1 )
						for( i = 0; i < NUM_OUTPUTS; i++ )
							arOutput[ i ] += ar[ i ];
					else
						for( i = 0; i < NUM_OUTPUTS; i++ )
							arOutput[ i ] += ar[ i ] * 2.0;
				}

			arOutput[ OUTPUT_WIN ] = 1.0 - arOutput[ OUTPUT_WIN ] / 36.0;
	  
			ar[ 0 ] = arOutput[ OUTPUT_WINGAMMON ] / 36.0;
			arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] / 36.0;
			arOutput[ OUTPUT_LOSEGAMMON ] = ar[ 0 ];
	  
			ar[ 0 ] = arOutput[ OUTPUT_WINBACKGAMMON ] / 36.0;
			arOutput[ OUTPUT_WINBACKGAMMON ] =
				arOutput[ OUTPUT_LOSEBACKGAMMON ] / 36.0;
			arOutput[ OUTPUT_LOSEBACKGAMMON ] = ar[ 0 ];

		} else {

			/* reduced search */

			/* 
			 * - Loop over all 21 dice rolls
			 * - evaluate at 0-ply
			 * - sort
			 * - do 1-ply on a subset
			 * - calculate corrected probabilities
			 */

			RedEvalData ad0ply[ 21 ];
			int anRed7[ 7 ] = { 2, 5, 8, 10, 12, 15, 18 };
			int anRed11[ 11 ] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 };
			int anRed14[ 14 ] = 
			{ 1, 2, 3, 5, 6, 7, 9, 11, 12, 13, 15, 17, 18, 19 }; 
			int *pnRed;
			int nRed;
			int k;
			float rDiff, arDiff[ NUM_OUTPUTS ];

			k = 0;

			/* Loop over all 21 dice rolls */

			for( n0 = 1; n0 <= 6; n0++ )
				for( n1 = 1; n1 <= n0; n1++ ) {

					for( i = 0; i < 25; i++ ) {
						anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
						anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
					}

					if( fInterrupt ) {
						errno = EINTR;
						return -1;
					} else if( fAction )
						fnAction();
	      
					FindBestMovePlied( anMove, n0, n1, anBoardNew, pci, pec, 0 );
	      
					SwapSides( anBoardNew );

					SetCubeInfo ( &ciOpp, pci->nCube, pci->fCubeOwner,
												! pci->fMove );

					/* Evaluate at 0-ply */
	      
					if( EvaluatePositionCache( anBoardNew, ad0ply[ k ].arOutput, 
																		 &ciOpp, pec, 0, 
																		 pec->fRelativeAccuracy ? pc :
																		 ClassifyPosition( anBoardNew ) ) )
						return -1;

					ad0ply[ k ].rScore = Utility ( ad0ply[ k ].arOutput, &ciOpp );
					PositionKey ( anBoardNew, ad0ply[ k ].auch);

					if( n0 == n1 ) {

						for( i = 0; i < NUM_OUTPUTS; i++ )
							arOutput[ i ] += ad0ply[ k ].arOutput[ i ];

						ad0ply[ k ].rWeight = 1.0;

					} else {

						for( i = 0; i < NUM_OUTPUTS; i++ )
							arOutput[ i ] += 2.0 * ad0ply[ k ].arOutput[ i ];

						ad0ply[ k ].rWeight = 2.0;

					}

					k++;
	      
				}

			/* sort */

			qsort ( ad0ply, 21, sizeof ( RedEvalData ), 
							(cfunc) CompareRedEvalData );

			for ( i = 0; i < NUM_OUTPUTS; i++ )
				arDiff[ i ] = 0.0;
			rDiff = 0.0;

			/* do 1-ply on subset */

			switch ( pec->nReduced ) {
			case 7:
				nRed = 7;
				pnRed = anRed7;
				break;

			case 11:
				nRed = 11;
				pnRed = anRed11;
				break;

			case 14:
				nRed = 14;
				pnRed = anRed14;
				break;

			default:
				assert ( FALSE );
				break;

			}

			for ( k = 0; k < nRed; pnRed++, k++ ) {

				PositionFromKey ( anBoardNew, ad0ply[ *pnRed ].auch );

				if( EvaluatePositionCache( anBoardNew, ar, &ciOpp, pec, nPlies - 1,
																	 pec->fRelativeAccuracy ? pc :
																	 ClassifyPosition( anBoardNew ) ) )
					return -1;

				for ( i = 0; i < NUM_OUTPUTS; i++ ) {

					arDiff[ i ] +=
						ad0ply[ *pnRed ].rWeight * 
						( ar[ i ] - ad0ply[ *pnRed ].arOutput[ i ] );

				}

				rDiff += ad0ply[ *pnRed ].rWeight;

			}
	    
	  
			for ( i = 0; i < NUM_OUTPUTS; i++ )

				arOutput[ i ] += 36. * arDiff[ i ] / rDiff;

			arOutput[ OUTPUT_WIN ] = 1.0 - arOutput[ OUTPUT_WIN ] / 36.0;
	  
			ar[ 0 ] = arOutput[ OUTPUT_WINGAMMON ] / 36.0;
			arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] / 36.0;
			arOutput[ OUTPUT_LOSEGAMMON ] = ar[ 0 ];
	  
			ar[ 0 ] = arOutput[ OUTPUT_WINBACKGAMMON ] / 36.0;
			arOutput[ OUTPUT_WINBACKGAMMON ] =
				arOutput[ OUTPUT_LOSEBACKGAMMON ] / 36.0;
			arOutput[ OUTPUT_LOSEBACKGAMMON ] = ar[ 0 ];

		}
	     

	} else {
		/* at leaf node; use static evaluation */
      
		acef[ pc ]( anBoard, arOutput );
      
		SanityCheck( anBoard, arOutput );
	}

	return 0;
}

static int 
EvaluatePositionCache( int anBoard[ 2 ][ 25 ], float arOutput[],
											 cubeinfo *pci, evalcontext *pecx, int nPlies,
											 positionclass pc ) {

	evalcache ec, *pec;
	long l;
	
	PositionKey( anBoard, ec.auchKey );
	ec.nEvalContext = nPlies ^ ( pecx->nSearchCandidates << 2 ) ^
		( ( (int) ( pecx->rSearchTolerance * 100 ) ) << 6 ) ^
		( pecx->nReduced << 12 ) ^
		( pecx->fRelativeAccuracy << 17 );
	ec.pc = pc;

	l = EvalCacheHash( &ec );
    
	if( ( pec = CacheLookup( &cEval, l, &ec ) ) ) {
		memcpy( arOutput, pec->ar, sizeof( pec->ar ) );

		return 0;
	}
    
	if( EvaluatePositionFull( anBoard, arOutput, pci, pecx, nPlies, pc ) )
		return -1;

	memcpy( ec.ar, arOutput, sizeof( ec.ar ) );
    
	return CacheAdd( &cEval, l, &ec, sizeof ec );
}

extern int 
EvaluatePosition( int anBoard[ 2 ][ 25 ], float arOutput[],
									cubeinfo *pci, evalcontext *pec ) {
    
	positionclass pc = ClassifyPosition( anBoard );
    
	return EvaluatePositionCache( anBoard, arOutput, pci, 
																pec ? pec : &ecBasic,
																pec ? pec->nPlies : 0, pc );
}

extern void InvertEvaluation( float ar[ NUM_OUTPUTS ] ) {		

	float r;
    
	ar[ OUTPUT_WIN ] = 1.0 - ar[ OUTPUT_WIN ];
	
	r = ar[ OUTPUT_WINGAMMON ];
	ar[ OUTPUT_WINGAMMON ] = ar[ OUTPUT_LOSEGAMMON ];
	ar[ OUTPUT_LOSEGAMMON ] = r;
	
	r = ar[ OUTPUT_WINBACKGAMMON ];
	ar[ OUTPUT_WINBACKGAMMON ] = ar[ OUTPUT_LOSEBACKGAMMON ];
	ar[ OUTPUT_LOSEBACKGAMMON ] = r;
}

extern int GameStatus( int anBoard[ 2 ][ 25 ] ) {

	float ar[ NUM_OUTPUTS ];
    
	if( ClassifyPosition( anBoard ) != CLASS_OVER )
		return 0;

	EvalOver( anBoard, ar );

	if( ar[ OUTPUT_WINBACKGAMMON ] || ar[ OUTPUT_LOSEBACKGAMMON ] )
		return 3;

	if( ar[ OUTPUT_WINGAMMON ] || ar[ OUTPUT_LOSEGAMMON ] )
		return 2;

	return 1;
}

extern int TrainPosition( int anBoard[ 2 ][ 25 ], float arDesired[] ) {

  float arInput[ NUM_INPUTS ], arOutput[ NUM_OUTPUTS ];

  int pc = ClassifyPosition( anBoard );
  
  neuralnet* nn;
  
  switch( pc ) {
	case CLASS_CONTACT: nn = &nnContact; break;
	case CLASS_RACE:    nn = &nnRace; break;
	case CLASS_BPG:     nn = &nnBPG; break;
	default:
    {
      errno = EDOM;
      return -1;
    }
  }

  SanityCheck( anBoard, arDesired );
    
  CalculateInputs( anBoard, arInput );

  NeuralNetTrain( nn, arInput, arOutput, arDesired,
									2.0 / pow( 100.0 + nn->nTrained, 0.25 ) );

  return 0;
}

extern void SetGammonPrice( float rGammon, float rLoseGammon,
			    float rBackgammon, float rLoseBackgammon ) {
    
    arGammonPrice[ 0 ] = rGammon;
    arGammonPrice[ 1 ] = rLoseGammon;
    arGammonPrice[ 2 ] = rBackgammon;
    arGammonPrice[ 3 ] = rLoseBackgammon;
}


extern float
Utility( float ar[ NUM_OUTPUTS ], cubeinfo *pci ) {

	if ( ! nMatchTo ) {

		/* equity calculation for money game */

		/* For money game the gammon price is the same for both
			 players, so there is no need to use pci->fMove. */

    return 
      ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
      ( ar[ OUTPUT_WINGAMMON ] - ar[ OUTPUT_LOSEGAMMON ] ) * 
      pci -> arGammonPrice[ 0 ] +
      ( ar[ OUTPUT_WINBACKGAMMON ] - ar[ OUTPUT_LOSEBACKGAMMON ] ) *
      pci -> arGammonPrice[ 1 ];

	} 
	else {

		/* equity calculation for match play */

		/* FIXME! */

		assert ( FALSE );

	}

		
}

static int ApplySubMove( int anBoard[ 2 ][ 25 ], int iSrc, int nRoll ) {

    int iDest = iSrc - nRoll;

    if( nRoll < 1 || nRoll > 6 || iSrc < 0 || iSrc > 24 ||
	anBoard[ 1 ][ iSrc ] < 1 ) {
	/* Invalid dice roll, invalid point number, or source point is
	   empty */
	errno = EINVAL;
	return -1;
    }
    
    anBoard[ 1 ][ iSrc ]--;

    if( iDest < 0 )
	return 0;
    
    if( anBoard[ 0 ][ 23 - iDest ] ) {
	if( anBoard[ 0 ][ 23 - iDest ] > 1 ) {
	    /* Trying to move to a point already made by the opponent */
	    errno = EINVAL;
	    return -1;
	}
	anBoard[ 1 ][ iDest ] = 1;
	anBoard[ 0 ][ 23 - iDest ] = 0;
	anBoard[ 0 ][ 24 ]++;
    } else
	anBoard[ 1 ][ iDest ]++;

    return 0;
}

extern int ApplyMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    int i;
    
    for( i = 0; i < 8 && anMove[ i ] >= 0; i += 2 )
	if( ApplySubMove( anBoard, anMove[ i ],
			  anMove[ i ] - anMove[ i + 1 ] ) )
	    return -1;
    
    return 0;
}

static void SaveMoves( movelist *pml, int cMoves, int cPip, int anMoves[],
		       int anBoard[ 2 ][ 25 ], int fPartial ) {
    int i, j;
    move *pm;
    unsigned char auch[ 10 ];

    if( fPartial ) {
	/* Save all moves, even incomplete ones */
	if( cMoves > pml->cMaxMoves )
	    pml->cMaxMoves = cMoves;
	
	if( cPip > pml->cMaxPips )
	    pml->cMaxPips = cPip;
    } else {
	/* Save only legal moves: if the current move moves plays less
	   chequers or pips than those already found, it is illegal; if
	   it plays more, the old moves are illegal. */
	if( cMoves < pml->cMaxMoves || cPip < pml->cMaxPips )
	    return;

	if( cMoves > pml->cMaxMoves || cPip > pml->cMaxPips )
	    pml->cMoves = 0;
	
	pml->cMaxMoves = cMoves;
	pml->cMaxPips = cPip;
    }
    
    pm = pml->amMoves + pml->cMoves;
    
    PositionKey( anBoard, auch );
    
    for( i = 0; i < pml->cMoves; i++ )
	if( EqualKeys( auch, pml->amMoves[ i ].auch ) ) {
	    if( cMoves > pml->amMoves[ i ].cMoves ||
		cPip > pml->amMoves[ i ].cPips ) {
		for( j = 0; j < cMoves * 2; j++ )
		    pml->amMoves[ i ].anMove[ j ] = anMoves[ j ] > -1 ?
			anMoves[ j ] : -1;
    
		if( cMoves < 4 )
		    pml->amMoves[ i ].anMove[ cMoves * 2 ] = -1;

		pml->amMoves[ i ].cMoves = cMoves;
		pml->amMoves[ i ].cPips = cPip;
	    }
	    
	    return;
	}
    
    for( i = 0; i < cMoves * 2; i++ )
	pm->anMove[ i ] = anMoves[ i ] > -1 ? anMoves[ i ] : -1;
    
    if( cMoves < 4 )
	pm->anMove[ cMoves * 2 ] = -1;
    
    for( i = 0; i < 10; i++ )
	pm->auch[ i ] = auch[ i ];

    pm->cMoves = cMoves;
    pm->cPips = cPip;

    pm->pEval = NULL;
    
    pml->cMoves++;

    assert( pml->cMoves < MAX_INCOMPLETE_MOVES );
}

static int LegalMove( int anBoard[ 2 ][ 25 ], int iSrc, int nPips ) {

    int i, nBack = 0, iDest = iSrc - nPips;

    if( iDest >= 0 )
	return ( anBoard[ 0 ][ 23 - iDest ] < 2 );

    /* otherwise, attempting to bear off */

    for( i = 1; i < 25; i++ )
	if( anBoard[ 1 ][ i ] > 0 )
	    nBack = i;

    return ( nBack <= 5 && ( iSrc == nBack || iDest == -1 ) );
}

static int GenerateMovesSub( movelist *pml, int anRoll[], int nMoveDepth,
			     int iPip, int cPip, int anBoard[ 2 ][ 25 ],
			     int anMoves[], int fPartial ) {
    int i, iCopy, fUsed = 0;
    int anBoardNew[ 2 ][ 25 ];

    if( nMoveDepth > 3 || !anRoll[ nMoveDepth ] )
	return TRUE;

    if( anBoard[ 1 ][ 24 ] ) { /* on bar */
	if( anBoard[ 0 ][ anRoll[ nMoveDepth ] - 1 ] >= 2 )
	    return TRUE;

	anMoves[ nMoveDepth * 2 ] = 24;
	anMoves[ nMoveDepth * 2 + 1 ] = 24 - anRoll[ nMoveDepth ];

	for( i = 0; i < 25; i++ ) {
	    anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	    anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}
	
	ApplySubMove( anBoardNew, 24, anRoll[ nMoveDepth ] );
	
	if( GenerateMovesSub( pml, anRoll, nMoveDepth + 1, 23, cPip +
			      anRoll[ nMoveDepth ], anBoardNew, anMoves,
			      fPartial ) )
	    SaveMoves( pml, nMoveDepth + 1, cPip + anRoll[ nMoveDepth ],
		       anMoves, anBoardNew, fPartial );

	return fPartial;
    } else {
	for( i = iPip; i >= 0; i-- )
	    if( anBoard[ 1 ][ i ] && LegalMove( anBoard, i,
						anRoll[ nMoveDepth ] ) ) {
		anMoves[ nMoveDepth * 2 ] = i;
		anMoves[ nMoveDepth * 2 + 1 ] = i -
		    anRoll[ nMoveDepth ];
		
		for( iCopy = 0; iCopy < 25; iCopy++ ) {
		    anBoardNew[ 0 ][ iCopy ] = anBoard[ 0 ][ iCopy ];
		    anBoardNew[ 1 ][ iCopy ] = anBoard[ 1 ][ iCopy ];
		}
    
		ApplySubMove( anBoardNew, i, anRoll[ nMoveDepth ] );
		
		if( GenerateMovesSub( pml, anRoll, nMoveDepth + 1,
				   anRoll[ 0 ] == anRoll[ 1 ] ? i : 23,
				   cPip + anRoll[ nMoveDepth ],
				   anBoardNew, anMoves, fPartial ) )
		    SaveMoves( pml, nMoveDepth + 1, cPip +
			       anRoll[ nMoveDepth ], anMoves, anBoardNew,
			       fPartial );
		
		fUsed = 1;
	    }
    }

    return !fUsed || fPartial;
}

static int CompareMoves( const move *pm0, const move *pm1 ) {

    return pm1->rScore > pm0->rScore ? 1 : -1;
}

static positionclass 
ScoreMoves( movelist *pml, cubeinfo *pci, evalcontext *pec, int nPlies,
            positionclass pcGeneral ) {

    int i, anBoardTemp[ 2 ][ 25 ];
    float arEval[ NUM_OUTPUTS ];
    
    pml->rBestScore = -99999.9;

    if( pec->fRelativeAccuracy && pcGeneral == CLASS_ANY ) {
      positionclass pc;
      
      /* Search for the most general class containing all positions */
      for( i = 0; i < pml->cMoves; i++ ) {
        PositionFromKey( anBoardTemp, pml->amMoves[ i ].auch );
        
        if( ( pc = ClassifyPosition( anBoardTemp ) ) > pcGeneral )
          if( ( pcGeneral = pc ) == CLASS_GLOBAL )
            break;
      }
    }
    
    for( i = 0; i < pml->cMoves; i++ ) {
      PositionFromKey( anBoardTemp, pml->amMoves[ i ].auch );
      
      SwapSides( anBoardTemp );

      if( EvaluatePositionCache( anBoardTemp, arEval, pci, pec, nPlies,
                                 pec->fRelativeAccuracy ? pcGeneral :
                                 ClassifyPosition( anBoardTemp ) ) )
        return -1;
      
      InvertEvaluation( arEval );
      
      if( pml->amMoves[ i ].pEval )
        memcpy( pml->amMoves[ i ].pEval, arEval, sizeof( arEval ) );
      
      if( ( pml->amMoves[ i ].rScore = Utility( arEval, pci ) ) >
          pml->rBestScore ) {
        pml->iMoveBest = i;
        pml->rBestScore = pml->amMoves[ i ].rScore;
      }
    }
    
    return pcGeneral;
}

extern int 
GenerateMoves( movelist *pml, int anBoard[ 2 ][ 25 ],
               int n0, int n1, int fPartial ) {

  int anRoll[ 4 ], anMoves[ 8 ];
  static move amMoves[ MAX_INCOMPLETE_MOVES ];

    anRoll[ 0 ] = n0;
    anRoll[ 1 ] = n1;

    anRoll[ 2 ] = anRoll[ 3 ] = ( ( n0 == n1 ) ? n0 : 0 );

    pml->cMoves = pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
    pml->amMoves = amMoves; /* use static array for top-level search, since
			       it doesn't need to be re-entrant */
    
    GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves,fPartial );

    if( anRoll[ 0 ] != anRoll[ 1 ] ) {
	swap( anRoll, anRoll + 1 );

	GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves, fPartial );
    }

    return pml->cMoves;
}

static int FindBestMovePlied( int anMove[ 8 ], int nDice0, int nDice1,
			      int anBoard[ 2 ][ 25 ], cubeinfo *pci, evalcontext *pec,
			      int nPlies ) {
  int i, j, iPly;
  positionclass pc;
  movelist ml;
#if __GNUC__
  move amCandidates[ pec->nSearchCandidates ];
#elif HAVE_ALLOCA
  move* amCandidates = alloca( pec->nSearchCandidates * sizeof( move ) );
#else
  move amCandidates[ MAX_SEARCH_CANDIDATES ];
#endif

  if( anMove ) {
    for( i = 0; i < 8; i++ ) {
      anMove[ i ] = -1;
    }
  }

  GenerateMoves( &ml, anBoard, nDice0, nDice1, FALSE );
    
  if( !ml.cMoves )
    /* no legal moves */
    return 0;
  else if( ml.cMoves == 1 )
    /* forced move */
    ml.iMoveBest = 0;
  else {
    /* choice of moves */
    if( ( pc = ScoreMoves( &ml, pci, pec, 0, CLASS_ANY ) ) < 0 )
      return -1;

    for( iPly = 0; iPly < nPlies; iPly++ ) {

      float tol = pec->rSearchTolerance / ( 1 << iPly );
      
      if( iPly == 0 && tol < 0.24 ) {
	/* Set a minimum tolerance for ply 0. The differences between
           1 and 0ply are sometimes large enough so that a small
           tolerance eliminates the better moves.  Those situations
           are more likely to happen when different moves are
           evaluated with different neural nets. This is a drawback of
           using several neural nets, and we probably want a better
           solution in the future. */

	tol = 0.24;
      }
      
      for( i = 0, j = 0; i < ml.cMoves; i++ )
	if( ml.amMoves[ i ].rScore >= ml.rBestScore - tol ) {
	  if( i != j )
	    ml.amMoves[ j ] = ml.amMoves[ i ];
		    
	  j++;
	}
	    
      if( j == 1 )
	break;

      qsort( ml.amMoves, j, sizeof( move ), (cfunc) CompareMoves );

      ml.iMoveBest = 0;
	    
      ml.cMoves = ( j < ( pec->nSearchCandidates >> iPly ) ? j :
		    ( pec->nSearchCandidates >> iPly ) );

      if( ml.amMoves != amCandidates ) {
	memcpy( amCandidates, ml.amMoves, ml.cMoves * sizeof( move ) );
	    
	ml.amMoves = amCandidates;
      }

      if( ScoreMoves( &ml, pci, pec, iPly + 1, pc ) < 0 )
	return -1;
    }
  }

  if( anMove ) {
    for( i = 0; i < ml.cMaxMoves * 2; i++ ) {
      anMove[ i ] = ml.amMoves[ ml.iMoveBest ].anMove[ i ];
    }
  }
	
  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  return ml.cMaxMoves * 2;
}

extern 
int FindBestMove( int anMove[ 8 ], int nDice0, int nDice1,
									int anBoard[ 2 ][ 25 ], cubeinfo *pci,
									evalcontext *pec ) {

    return FindBestMovePlied( anMove, nDice0, nDice1, anBoard, 
															pci, pec ? pec :
                              &ecBasic, pec ? pec->nPlies : 0 );
}

extern int 
FindBestMoves( movelist *pml, float ar[][ NUM_OUTPUTS ],
               int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
               int c, float d, cubeinfo *pci, evalcontext *pec ) {

    int i, j;
    static move amCandidates[ 32 ];
    positionclass pc;
    
    /* amCandidates is only 32 elements long, so we silently truncate any
       request for more moves to 32 */
    if( c > 32 )
	c = 32;

    /* Find all moves -- note that pml contains internal pointers to static
       data, so we can't call GenerateMoves again (or anything that calls
       it, such as ScoreMoves at more than 0 plies) until we have saved
       the moves we want to keep in amCandidates. */
    GenerateMoves( pml, anBoard, nDice0, nDice1, FALSE );

    if( ( pc = ScoreMoves( pml, pci, pec, 0, CLASS_ANY ) ) < 0 )
	return -1;

    /* Eliminate moves whose scores are below the threshold from the best */
    for( i = 0, j = 0; i < pml->cMoves; i++ )
	if( pml->amMoves[ i ].rScore >= pml->rBestScore - d ) {
	    if( i != j )
		pml->amMoves[ j ] = pml->amMoves[ i ];

	    j++;
	}

    /* Sort the others in descending order */
    qsort( pml->amMoves, j, sizeof( move ), (cfunc) CompareMoves );

    pml->iMoveBest = 0;

    /* Consider only those better than the threshold or as many as were
       requested, whichever is less */
    pml->cMoves = ( j < c ) ? j : c;

    /* Copy the moves under consideration into our private array, so we
       can safely call ScoreMoves without them being clobbered */
    memcpy( amCandidates, pml->amMoves, pml->cMoves * sizeof( move ) );    
    pml->amMoves = amCandidates;

    /* Arrange for the full evaluations to be stored */
    for( i = 0; i < pml->cMoves; i++ )
	amCandidates[ i ].pEval = ar[ i ];

    /* Calculate the full evaluations at the search depth requested */
    if( ScoreMoves( pml, pci, pec, pec->nPlies, pc ) < 0 )
	return -1;

    /* Using a deeper search might change the order of the evaluations;
       reorder them according to the latest evaluation */
    if( pec->nPlies )
	qsort( pml->amMoves, pml->cMoves, sizeof( move ),
	       (cfunc) CompareMoves );

    return 0;
}

extern int PipCount( int anBoard[ 2 ][ 25 ], int anPips[ 2 ] ) {

    int i;
    
    anPips[ 0 ] = 0;
    anPips[ 1 ] = 0;
    
    for( i = 0; i < 25; i++ ) {
	anPips[ 0 ] += anBoard[ 0 ][ i ] * ( i + 1 );
	anPips[ 1 ] += anBoard[ 1 ][ i ] * ( i + 1 );
    }

    return 0;
}

static void DumpOver( int anBoard[ 2 ][ 25 ], char *pchOutput ) {

    float ar[ NUM_OUTPUTS ];
    
    EvalOver( anBoard, ar );

    if( ar[ OUTPUT_WIN ] > 0.0 )
	strcpy( pchOutput, "Win " );
    else
	strcpy( pchOutput, "Loss " );

    if( ar[ OUTPUT_WINBACKGAMMON ] > 0.0 ||
	ar[ OUTPUT_LOSEBACKGAMMON ] > 0.0 )
	strcat( pchOutput, "(backgammon)\n" );
    else if( ar[ OUTPUT_WINGAMMON ] > 0.0 ||
	ar[ OUTPUT_LOSEGAMMON ] > 0.0 )
	strcat( pchOutput, "(gammon)\n" );
    else
	strcat( pchOutput, "(single)\n" );
}

static void DumpBearoff2( int anBoard[ 2 ][ 25 ], char *szOutput ) {

    assert( pBearoff2 );
    
    /* no-op -- nothing much we can say */
}

static void DumpBearoff1( int anBoard[ 2 ][ 25 ], char *szOutput ) {

    int i, n, nOpp, an[ 2 ], f0 = FALSE, f1 = FALSE;

    assert( pBearoff1 );
    
    nOpp = PositionBearoff( anBoard[ 0 ] );
    n = PositionBearoff( anBoard[ 1 ] );

    strcpy( szOutput, "Rolls\tPlayer\tOpponent\n" );
    
    for( i = 0; i < 32; i++ ) {
	an[ 0 ] = pBearoff1[ ( n << 6 ) | ( i << 1 ) ] +
	    ( pBearoff1[ ( n << 6 ) | ( i << 1 ) | 1 ] << 8 );

	an[ 1 ] = pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) ] +
	    ( pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) | 1 ] << 8 );

	if( an[ 0 ] )
	    f0 = TRUE;

	if( an[ 1 ] )
	    f1 = TRUE;

	if( f0 || f1 ) {
    if( f0 && f1 && !an[ 0 ] && !an[ 1 ] )
      break;

    szOutput = strchr( szOutput, 0 );
	
    sprintf( szOutput, "%5d\t%6.3f\t%8.3f\n", i, an[ 0 ] / 655.35,
             an[ 1 ] / 655.35 );
	}
    }
}

static void DumpRace( int anBoard[ 2 ][ 25 ], char *szOutput ) {

  /* no-op -- nothing much we can say, really (pip count?) */
}

static void DumpContact( int anBoard[ 2 ][ 25 ], char *szOutput ) {

  float arInput[ NUM_INPUTS ], arOutput[ NUM_OUTPUTS ],
    arDerivative[ NUM_INPUTS * NUM_OUTPUTS ],
    ardEdI[ NUM_INPUTS ], *p;
  int i, j;
    
  CalculateInputs( anBoard, arInput );
    
  NeuralNetDifferentiate( &nnContact, arInput, arOutput, arDerivative );

  for( i = 0; i < NUM_INPUTS; i++ ) {
    for( j = 0, p = arDerivative + i; j < NUM_OUTPUTS; p += NUM_INPUTS )
	    arOutput[ j++ ] = *p;

    ardEdI[ i ] = Utility( arOutput, &ciBasic ) + 1.0f; 
    /* FIXME this is a bit grotty -- need to
       eliminate the constant 1 added by Utility */
  }
    
  sprintf( szOutput,
           "Input          \tValue             \t dE/dI\n"
           "OFF1           \t%5.3f             \t%6.3f\n"
           "OFF2           \t%5.3f             \t%6.3f\n"
           "OFF3           \t%5.3f             \t%6.3f\n"
           "BREAK_CONTACT  \t%5.3f             \t%6.3f\n"
           "BACK_CHEQUER   \t%5.3f             \t%6.3f\n"
           "BACK_ANCHOR    \t%5.3f             \t%6.3f\n"
           "FORWARD_ANCHOR \t%5.3f             \t%6.3f\n"
           "PIPLOSS        \t%5.3f (%5.3f avg)\t%6.3f\n"
           "P1             \t%5.3f (%5.3f/36) \t%6.3f\n"
           "P2             \t%5.3f (%5.3f/36) \t%6.3f\n"
           "BACKESCAPES    \t%5.3f (%5.3f/36) \t%6.3f\n"
           "ACONTAIN       \t%5.3f (%5.3f/36) \t%6.3f\n"
           "CONTAIN        \t%5.3f (%5.3f/36) \t%6.3f\n"
           // "BUILDERS       \t%5.3f (%5.3f/6)  \t%6.3f\n"
           //"SLOTTED        \t%5.3f (%5.3f/6)  \t%6.3f\n"
           "MOBILITY       \t%5.3f             \t%6.3f\n"
           "MOMENT2        \t%5.3f             \t%6.3f\n"
           "ENTER          \t%5.3f (%5.3f/12) \t%6.3f\n"
           //"TOP_EVEN       \t%5.3f             \t%6.3f\n"
           //"TOP2_EVEN      \t%5.3f             \t%6.3f\n"
           ,
           arInput[ I_OFF1 << 1 ], ardEdI[ I_OFF1 << 1 ],
           arInput[ I_OFF2 << 1 ], ardEdI[ I_OFF2 << 1 ],
           arInput[ I_OFF3 << 1 ], ardEdI[ I_OFF3 << 1 ],
           arInput[ I_BREAK_CONTACT << 1 ], ardEdI[ I_BREAK_CONTACT << 1 ],
           arInput[ I_BACK_CHEQUER << 1 ], ardEdI[ I_BACK_CHEQUER << 1 ],
           arInput[ I_BACK_ANCHOR << 1 ], ardEdI[ I_BACK_ANCHOR << 1 ],
           arInput[ I_FORWARD_ANCHOR << 1 ], ardEdI[ I_FORWARD_ANCHOR << 1 ],
           arInput[ I_PIPLOSS << 1 ], 
           arInput[ I_P1 << 1 ] ? arInput[ I_PIPLOSS << 1 ] /
	         arInput[ I_P1 << 1 ] * 12.0 : 0.0, ardEdI[ I_PIPLOSS << 1 ],
           arInput[ I_P1 << 1 ], arInput[ I_P1 << 1 ] * 36.0,
	         ardEdI[ I_P1 << 1 ],
           arInput[ I_P2 << 1 ], arInput[ I_P2 << 1 ] * 36.0,
	         ardEdI[ I_P2 << 1 ],
           arInput[ I_BACKESCAPES << 1 ], arInput[ I_BACKESCAPES << 1 ] *
	         36.0, ardEdI[ I_BACKESCAPES << 1 ],
           arInput[ I_ACONTAIN << 1 ], arInput[ I_ACONTAIN << 1 ] * 36.0,
	         ardEdI[ I_ACONTAIN << 1 ],
           arInput[ I_CONTAIN << 1 ], arInput[ I_CONTAIN << 1 ] * 36.0,
	         ardEdI[ I_CONTAIN << 1 ],
           /*  	     arInput[ I_BUILDERS << 1 ], arInput[ I_BUILDERS << 1 ] * 6.0, */
           /*  	         ardEdI[ I_BUILDERS << 1 ], */
           /*  	     arInput[ I_SLOTTED << 1 ], arInput[ I_SLOTTED << 1 ] * 6.0, */
           /*  	         ardEdI[ I_SLOTTED << 1 ], */
           arInput[ I_MOBILITY << 1 ], ardEdI[ I_MOBILITY << 1 ],
           arInput[ I_MOMENT2 << 1 ], ardEdI[ I_MOMENT2 << 1 ],
           arInput[ I_ENTER << 1 ], arInput[ I_ENTER << 1 ] * 12.0,
	         ardEdI[ I_ENTER << 1 ]
           /*  	     ,arInput[ I_TOP_EVEN << 1 ], ardEdI[ I_TOP_EVEN << 1 ], */
           /*  	     arInput[ I_TOP2_EVEN << 1 ], ardEdI[ I_TOP2_EVEN << 1 ]  */
           );
}

static classdumpfunc acdf[ N_CLASSES ] = {
  DumpOver, DumpBearoff2, DumpBearoff1, DumpRace, DumpContact, DumpContact
};

extern int DumpPosition( int anBoard[ 2 ][ 25 ], char *szOutput,
                         evalcontext *pec ) {

  float arOutput[ NUM_OUTPUTS ], arCfOutput[ 4 ];
  positionclass pc = ClassifyPosition( anBoard );
  int i, nPlies;
  cubeinfo ci;
    
  strcpy( szOutput, "Evaluator: \t" );
    
  switch( pc ) {
  case CLASS_OVER: /* Game already finished */
    strcat( szOutput, "OVER" );
    break;

  case CLASS_BEAROFF2: /* Two-sided bearoff database */
    strcat( szOutput, "BEAROFF2" );
    break;

  case CLASS_BEAROFF1: /* One-sided bearoff database */
    strcat( szOutput, "BEAROFF1" );
    break;

  case CLASS_RACE: /* Race neural network */
    strcat( szOutput, "RACE" );
    break;

  case CLASS_CONTACT: /* Contact neural network */
    strcat( szOutput, "CONTACT" );
    break;
	
  case CLASS_BPG: 
    strcat( szOutput, "BPG" );
    break;
  }

  strcat( szOutput, "\n\n" );

  acdf[ pc ]( anBoard, strchr( szOutput, 0 ) );
  szOutput = strchr( szOutput, 0 );    

  strcpy( szOutput, "\n       \tWin  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
          "Equity  (cubeful)\n" );

  SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );
    
  nPlies = pec->nPlies > 9 ? 9 : pec->nPlies;
    
  for( i = 0; i <= nPlies; i++ ) {
    szOutput = strchr( szOutput, 0 );
	
    if( EvaluatePositionCache( anBoard, arOutput, &ci, pec, i, pc ) < 0 )
	    return -1;

    if ( EvaluatePositionCubeful ( anBoard, arCfOutput, &ci, pec, i )
         < 0 )
      return -1;

    if( !i )
	    strcpy( szOutput, "static" );
    else
	    sprintf( szOutput, "%2d ply", i );

    szOutput = strchr( szOutput, 0 );
	
    sprintf( szOutput,
             ":\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t(%+6.3f  (%+6.3f))\n",
             arOutput[ 0 ], arOutput[ 1 ], arOutput[ 2 ], arOutput[ 3 ],
             arOutput[ 4 ], Utility ( arOutput, &ci ), arCfOutput[ 0 ] );
  }

  /* if cube is available, output cube action */

  if ( GetDPEq ( NULL, NULL, &ci ) ) {

    szOutput = strchr( szOutput, 0 );    
    sprintf( szOutput, "\n\n" );
    szOutput = strchr( szOutput, 0 );    
    GetCubeActionSz ( arCfOutput, szOutput );

  }
  
  return 0;
}


extern int 
GetCubeActionSz ( float arDouble[ 4 ], char *szOutput ) {

  /* write string with cube action */

  /* FIXME: write equity and mwc for match play */

  if ( arDouble[ 2 ] >= arDouble[ 1 ] && arDouble[ 3 ] >= arDouble[ 1 ] ) {

    /* it's correct to double */

    if ( arDouble[ 2 ] < arDouble[ 3 ] ) {

      /* Optimal     : Double, take
         Best for me : Double, pass
         Worst for me: No Double */

      sprintf ( szOutput,
                "Double, take  : %+6.3f\n"
                "Double, pass  : %+6.3f   (%+6.3f)\n"
                "No double     : %+6.3f   (%+6.3f)\n\n"
                "Correct cube action: Double, take",
                arDouble[ 2 ],
                arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 2 ],
                arDouble[ 1 ], arDouble[ 1 ] - arDouble[ 2 ] );

    }
    else {

      /* Optimal     : Double, pass
         BEst for me : Double, take
         Worst for me: no double */

      sprintf ( szOutput,
                "Double, pass  : %+6.3f\n"
                "Double, take  : %+6.3f   (%+6.3f)\n"
                "No double     : %+6.3f   (%+6.3f)\n\n"
                "Correct cube action: Double, pass",
                arDouble[ 3 ],
                arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 3 ],
                arDouble[ 1 ], arDouble[ 1 ] - arDouble[ 3 ] );

    }

  } 
  else {

    /* it's correct not to double */

    if ( arDouble[ 1 ] > arDouble[ 3 ] ) {

      if ( arDouble[ 2 ] > arDouble[ 3 ] ) {

        /* Optimal     : no double
           Best for me : Double, take
           Worst for me: Double, pass */

        sprintf ( szOutput,
                  "No double     : %+6.3f\n"
                  "Double, take  : %+6.3f   (%+6.3f)\n"
                  "Double, pass  : %+6.3f   (%+6.3f)\n\n"
                  "Correct cube action: too good to double, pass",
                  arDouble[ 1 ],
                  arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ],
                  arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ] );

      }
      else {
        
        /* Optimal     : no double
           Best for me : Double, pass
           Worst for me: Double, take */

        /* This situation may arise in match play. */

        sprintf ( szOutput,
                  "No double     : %+6.3f\n"
                  "Double, pass  : %+6.3f   (%+6.3f)\n"
                  "Double, take  : %+6.3f   (%+6.3f)\n\n"
                  "Correct cube action: too good to double, take(!)",
                  arDouble[ 1 ],
                  arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ],
                  arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ] );

      }

    }
    else { /* arDouble[ 2 ] < arDouble[ 1 ] */
    
      /* Optimal     : no double
         Best for me : Double, pass
         Worst for me: Double, take */

        sprintf ( szOutput,
                  "No double     : %+6.3f\n"
                  "Double, pass  : %+6.3f   (%+6.3f)\n"
                  "Double, take  : %+6.3f   (%+6.3f)\n\n"
                  "Correct cube action: No double, take",
                  arDouble[ 1 ],
                  arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ],
                  arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ] );

    }

  }

}


extern int EvalCacheResize( int cNew ) {

  return CacheResize( &cEval, cNew );
}

extern int EvalCacheStats( int *pc, int *pcLookup, int *pcHit ) {

  *pc = cEval.c;
    
  return CacheStats( &cEval, pcLookup, pcHit );
}

extern int FindPubevalMove( int nDice0, int nDice1, int anBoard[ 2 ][ 25 ] ) {

  movelist ml;
  int i, j, anBoardTemp[ 2 ][ 25 ], anPubeval[ 28 ], fRace;

  fRace = ClassifyPosition( anBoard ) <= CLASS_RACE;
    
  GenerateMoves( &ml, anBoard, nDice0, nDice1, FALSE );
    
  if( !ml.cMoves )
    /* no legal moves */
    return 0;
  else if( ml.cMoves == 1 )
    /* forced move */
    ml.iMoveBest = 0;
  else {
    /* choice of moves */
    ml.rBestScore = -99999.9;

    for( i = 0; i < ml.cMoves; i++ ) {
	    PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

	    for( j = 0; j < 24; j++ )
        if( anBoardTemp[ 1 ][ j ] )
          anPubeval[ j + 1 ] = anBoardTemp[ 1 ][ j ];
        else
          anPubeval[ j + 1 ] = -anBoardTemp[ 0 ][ 23 - j ];

	    anPubeval[ 0 ] = -anBoardTemp[ 0 ][ 24 ];
	    anPubeval[ 25 ] = anBoardTemp[ 1 ][ 24 ];

	    for( anPubeval[ 26 ] = 15, j = 0; j < 24; j++ )
        anPubeval[ 26 ] -= anBoardTemp[ 1 ][ j ];

	    for( anPubeval[ 27 ] = -15, j = 0; j < 24; j++ )
        anPubeval[ 27 ] += anBoardTemp[ 0 ][ j ];

	    if( ( ml.amMoves[ i ].rScore = pubeval( fRace, anPubeval ) ) >
          ml.rBestScore ) {
        ml.iMoveBest = i;
        ml.rBestScore = ml.amMoves[ i ].rScore;
	    }
    }
  }
	
  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  return 0;
}


extern int
SetCubeInfo ( cubeinfo *ci, int nCube, int fCubeOwner,
							int fMove ) {

  ci->nCube = nCube;
  ci->fCubeOwner = fCubeOwner;
  ci->fMove = fMove;

  if ( ! nMatchTo ) {

    if ( fCubeOwner == -1 && fJacoby ) {

      ci->arGammonPrice[ 0 ] = 0.0;
      ci->arGammonPrice[ 1 ] = 0.0;
      ci->arGammonPrice[ 2 ] = 0.0;
      ci->arGammonPrice[ 3 ] = 0.0;

    }
    else {

      ci->arGammonPrice[ 0 ] = 1.0;
      ci->arGammonPrice[ 1 ] = 1.0;
      ci->arGammonPrice[ 2 ] = 1.0;
      ci->arGammonPrice[ 3 ] = 1.0;

    }

  }
  else {

		/* match play gammon prices */

		/* FIXME */

		assert ( FALSE );

	}

}


extern int 
EvaluatePositionCubeful( int anBoard[ 2 ][ 25 ], float arCfOutput[],
                         cubeinfo *pci, evalcontext *pec, int nPlies ) {

  /* Calculate cubeful equity. 
     
     Output is: arCfOutput[ 0 ]: equity for optimal cube action,
                arCfOutput[ 1 ]: equity for no double,
                arCfOutput[ 2 ]: equity for double, take,
                arCfOutput[ 3 ]: equity for double, pass.

  */

  cubeinfo ciD, ciR;
  int fCube, n0, n1, anBoardNew[ 2 ][ 25 ], i;
  float r;
  positionclass pc;

  pc = ClassifyPosition( anBoard );

  GetDPEq ( &fCube, &arCfOutput[ 3 ], pci );

  /* No double branch */

  SetCubeInfo ( &ciR, pci -> nCube, pci -> fCubeOwner, 
                ! pci -> fMove );

  arCfOutput[ 1 ] = 0.0;

  if ( pc != CLASS_OVER && nPlies > 0 ) {

    for ( n0 = 1; n0 <= 6; n0++ ) 
      for ( n1 = 1; n1 <= n0; n1++ ) {

        for( i = 0; i < 25; i++ ) {
          anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
          anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
        }

        if( fInterrupt ) {
          errno = EINTR;
          return -1;
        }

        FindBestMovePlied ( NULL, n0, n1, anBoardNew, pci, pec, 0 );

        SwapSides ( anBoardNew );

        if ( EvaluatePositionCubeful1 ( anBoardNew, &r, &ciR, pec,
                                        nPlies - 1 ) < 0 )
          return -1;

        arCfOutput[ 1 ] += ( n0 == n1 ) ? r : 2.0 * r;

      }

    if ( ! nMatchTo ) {
      arCfOutput[ 1 ] /= -36.0;
    }
    else
      arCfOutput[ 1 ] = 1.0 - arCfOutput[ 1 ] / 36.0;

  }
  else {
    
    EvaluatePositionCubeful1 ( anBoard, &arCfOutput[ 1 ], pci, pec, 0
                              );
  }

  /* Double branch */

  if ( fCube ) {

    /* nCube is doubled; Cube owner is opponent */

    SetCubeInfo ( &ciD, 2 * pci -> nCube, ! fMove, fMove );
    SetCubeInfo ( &ciR, 2 * pci -> nCube, ! fMove, ! fMove );

    if ( pc != CLASS_OVER && nPlies > 0 ) {

      arCfOutput[ 2 ] = 0.0;

      for ( n0 = 1; n0 <= 6; n0++ ) 
        for ( n1 = 1; n1 <= n0; n1++ ) {

          for( i = 0; i < 25; i++ ) {
            anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
            anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
          }
        
          if( fInterrupt ) {
            errno = EINTR;
            return -1;
          }

          FindBestMovePlied ( NULL, n0, n1, anBoardNew, &ciD, pec, 0 );

          SwapSides ( anBoardNew );

          if ( EvaluatePositionCubeful1 ( anBoardNew, &r, &ciR, pec,
                                          nPlies - 1 ) < 0 )
            return -1;
        
          arCfOutput[ 2 ] += ( n0 == n1 ) ? r : 2.0 * r;

        }

      if ( ! nMatchTo ) {
        arCfOutput[ 2 ] /= -18.0;
      }
      else
        arCfOutput[ 2 ] = 1.0 - arCfOutput[ 2 ] / 36.0;

    } 
    else {

      EvaluatePositionCubeful1 ( anBoard, &arCfOutput[ 2 ], &ciD,
                                 pec, 0 ); 

      if ( ! nMatchTo )
        arCfOutput[ 2 ] *= 2.0;

    }
        

    /* Find optimal cube action */

    if ( ( arCfOutput[ 2 ] >= arCfOutput[ 1 ] ) &&
         ( arCfOutput[ 3 ] >= arCfOutput[ 1 ] ) ) {

      /* we have a double */

      if ( arCfOutput[ 2 ] < arCfOutput[ 3 ] )
        /* ...take */
        arCfOutput[ 0 ] = arCfOutput[ 2 ];
      else
        /* ...pass */
        arCfOutput[ 0 ] = arCfOutput[ 3 ];

    }
    else
      /* no double (either no double, take or too good, pass) */
      arCfOutput[ 0 ] = arCfOutput[ 1 ];

  }
  else {

    /* We can't or wont double */

    arCfOutput[ 0 ] = arCfOutput[ 1 ];

  }

  return 0;

}

    
extern int
EvaluatePositionCubeful1( int anBoard[ 2 ][ 25 ], float *prOutput, 
                          cubeinfo *pci, evalcontext *pec, 
                          int nPlies) { 

  positionclass pc;

  int anBoardNew[ 2 ][ 25 ];
  
  if ( (  pc = ClassifyPosition( anBoard ) ) != CLASS_OVER &&
       nPlies > 0 ) {
    
    /* internal node; recurse */

    /* Do a 0-ply cube decision */

    float rND, rDT, rDP, r;
    int fCube, n0, n1, i, fDoubleBranch;
    cubeinfo ciD, *pciE, ciND;

    GetDPEq ( &fCube, &rDP, pci );

    SetCubeInfo ( &ciND, pci -> nCube, pci -> fCubeOwner,
                  ! pci -> fMove );

    if ( fCube ) {

      /* we have access a non-dead cube */

      if ( EvaluatePositionCubeful1 ( anBoard, &rND, pci, pec, 0 ) 
           < 0 )
        return -1;

      SetCubeInfo ( &ciD, 2 * pci -> nCube, ! pci -> fMove, 
                    pci -> fMove );

      if ( EvaluatePositionCubeful1 ( anBoard, &rDT, &ciD, pec, 0 ) 
           < 0 )
        return -1;

      if ( ! nMatchTo )
        rDT *= 2.0;

      if ( ( rDT >= rND ) && ( rDP >= rND ) ) {
        /* we have a double */
        pciE = &ciD;
        fDoubleBranch = 1;
      }
      else {
        /* no double */
        pciE = &ciND;
        fDoubleBranch = 0;
      }

    } 
    else
      pciE = &ciND;

    for ( n0 = 1; n0 <= 6; n0++ ) 
      for ( n1 = 1; n1 <= n0; n1++ ) {

        for( i = 0; i < 25; i++ ) {
          anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
          anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
        }

        if( fInterrupt ) {
          errno = EINTR;
          return -1;
        }

        FindBestMovePlied ( NULL, n0, n1, anBoardNew, 
                            pciE, pec, 0 );

        SwapSides ( anBoardNew );

        if ( EvaluatePositionCubeful1 ( anBoardNew, &r, pciE, pec,
                                        nPlies - 1 ) < 0 )
          return -1;

        *prOutput += ( n0 == n1 ) ? r : 2.0 * r;

      }

    if ( ! nMatchTo ) {
      if ( fDoubleBranch ) 
        *prOutput /= -18.0;
      else
        *prOutput /= -36.0;
    }
    else
      *prOutput = 1.0 - *prOutput / 36.0;

  } /* end recurse */
  else {

    /* At leaf node; use static evaluation.
       Call EvaluatePostion to ensure that the evaluation
       is cached.
    */

    float arOutput [ NUM_OUTPUTS ], rEq;

    EvaluatePosition ( anBoard, arOutput, pci, 0 );

    SanityCheck ( anBoard, arOutput );

    rEq = Utility ( arOutput, pci );

    if ( pc == CLASS_OVER ) {

      /* if the game is over, there is very little value
         of holding the cube */

      *prOutput = rEq;

    }
    else {

      float rW, rL;
      float rOppTG, rOppCP, rCP, rTG;
      float rOppIDP, rIDP;
      float rk, rOppk;

      /* money game */

      if ( arOutput[ 0 ] > 0.0 )
        rW = 1.0 + ( arOutput[ 1 ] + arOutput[ 2 ] ) / arOutput[ 0 ];
      else
        rW = 1.0;

      if ( arOutput[ 0 ] < 1.0 )
        rL = 1.0 + ( arOutput[ 3 ] + arOutput[ 4 ] ) /
          ( 1.0 - arOutput [ 0 ] );
      else
        rL = 1.0;

      rOppTG = 1.0 - ( rW + 1.0 ) / ( rW + rL + 0.5 * rCubeX );
      rOppCP = 1.0 -
        ( rW + 0.5 + 0.5 * rCubeX ) /
        ( rW + rL + 0.5 * rCubeX );
      rCP =
        ( rL + 0.5 + 0.5 * rCubeX ) /
        ( rW + rL + 0.5 * rCubeX );
      rTG = ( rL + 1.0 ) / ( rW + rL + 0.5 * rCubeX );
      
      if ( pci->fCubeOwner == -1 ) {

        /* centered cube */

        if ( arOutput[0 ] > rOppTG && arOutput[ 0 ] < rOppCP ) {
          /* Opp cashes. Don't award him full bonus, as I might
             have a joker-roll. */
          rEq = -0.9999;
        } else if ( arOutput[ 0 ] > rOppCP && arOutput[ 0 ] < rCP ) {
          /* in market window */
          
          if ( ! fJacoby ) {

            /* initial double point for money game without
               Jacoby rule. */
            
            rOppk = 1.0;
            rk = 1.0;
            
          }
          else {
            
            /* initial double points for money game with
               Jacoby rule. */
            
            if ( ! fBeavers ) {

              /* no beavers and other critters */

              rOppk =
                ( rW + rL ) * ( rW - 0.5 * ( 1.0 - rCubeX ) ) /
                ( rW * ( rW + rL - ( 1.0 - rCubeX ) ) );

              rk =
                ( rW + rL ) * ( rL - 0.5 * ( 1.0 - rCubeX ) ) /
                ( rL * ( rW + rL - ( 1.0 - rCubeX ) ) );

            }
            else {

              /* with beavers and other critters */

              rOppk =
                ( rW + rL ) * ( rW - 0.25 * ( 1.0 - rCubeX ) ) /
                ( rW * ( rW + rL - 0.5 * ( 1.0 - rCubeX ) ) );

              rk =
                ( rW + rL ) * ( rL - 0.25 * ( 1.0 - rCubeX ) ) /
                ( rL * ( rW + rL - 0.5 * ( 1.0 - rCubeX ) ) );

            }
          }

          rOppIDP = 1.0 - rOppk *
            ( rW +
              rCubeX / 2.0 * ( 3.0 - rCubeX )  / ( 2.0 - rCubeX ) )
            / ( rL +  rW + 0.5 * rCubeX );

          rIDP = rk *
            ( rL +
              rCubeX / 2.0 * ( 3.0 - rCubeX )  / ( 2.0 - rCubeX ) )
            / ( rL +  rW + 0.5 * rCubeX );

          if ( arOutput[ 0 ] < rOppIDP ) {

            float rE2 =
              2.0 * ( rOppIDP * ( rW + rL + 0.5 * rCubeX ) - rL );

            rEq = -1.0 +  ( rE2 + 1.0 ) *
              ( arOutput[ 0 ] - rOppCP ) / ( rOppIDP - rOppCP );

          }
          else if ( arOutput[ 0 ] < rIDP ) {

            float rE2 =
              2.0 * ( rOppIDP * ( rW + rL + 0.5 * rCubeX ) - rL );
            float rE3 =
              2.0 * ( rIDP * ( rW + rL + 0.5 * rCubeX ) -
                      rL - 0.5 * rCubeX );

            rEq = rE2 + ( rE3 - rE2 ) *
              ( arOutput[ 0 ] - rOppIDP ) / ( rIDP - rOppIDP );

          }
          else {

            float rE3 =
              2.0 * ( rIDP * ( rW + rL + 0.5 * rCubeX ) -
                      rL - 0.5 * rCubeX );

            rEq = rE3 + ( 1.0 - rE3 ) *
              ( arOutput[ 0 ] - rIDP ) / ( rCP - rIDP );

          }

        } else if ( arOutput[ 0 ] > rCP && arOutput[ 0 ] < rTG ) {
          /* cash */
          rEq = 1.0;
        }

      }
      else if ( pci->fCubeOwner == pci->fMove ) {

        /* I own cube */

        if ( arOutput[ 0 ] < rCP ) {
          rEq =
            ( arOutput[ 0 ] * ( rW + rL + 0.5 * rCubeX ) - rL );
        }
        else if ( arOutput[ 0 ] > rCP && arOutput[ 0 ] < rTG ) {
          /* cash */
          rEq = 1.0;
        }

      } else {

        /* Opp own Cube */

        if ( arOutput[0 ] > rOppTG && arOutput[ 0 ] < rOppCP ) {
          /* Opp cashes. Don't award him full bonus, as I might
             have a joker-roll. */
          rEq = -0.9999;
        } else if ( arOutput[ 0 ] > rOppCP ) {
          rEq = ( arOutput[ 0 ] * ( rW + rL + 0.5 * rCubeX )
                  - rL - 0.5 * rCubeX );
        }

      }

      *prOutput = rEq;

    } /* CLASS_OVER */
      
  } /* internal node */

  return 0;

}

extern int
GetDPEq ( int *pfCube, float *prDPEq, cubeinfo *pci ) {

  int fCube;

  if ( ! nMatchTo ) {

    /* Money game:
       Double, pass equity for money game is 1.0 points, since we always
       calculate equity normed to a 1-cube.
       Take the double branch if the cube is centered or I own the cube. */

    if ( prDPEq ) 
      *prDPEq = 1.0;

    fCube = 
      ( pci -> fCubeOwner == -1 ) || ( pci -> fCubeOwner == pci -> fMove );

    if ( pfCube )
      *pfCube = fCube;

  }
  else {

    /* Match play:
       Equity for double, pass is found from the match equity table.
       Take the double branch is I can/will use cube:
       - if it is not the Crawford game,
       - and if the cube is not dead,
       - and if it is post-Crawford and I'm trailing
       - and if I have access to the cube.
    */

    /* FIXME: equity for double, pass */
  
    fCube = ( ! fCrawford ) &&
      ( anScore[ pci -> fMove ] + pci -> nCube < nMatchTo ) &&
      ( ! ( fPostCrawford && ( anScore[ pci -> fMove ] == nMatchTo - 1
                               ) ) ) &&
      ( ( pci -> fCubeOwner == -1 ) || ( pci -> fCubeOwner == pci ->
                                         fMove ) );   

    if ( pfCube )
      *pfCube = fCube;
      
  }

  return fCube;

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
        return ( rL - 0.25 ) / ( rW + rL + 0.5 );
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

    /* FIXME! */

  }

}
