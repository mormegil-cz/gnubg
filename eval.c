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
#include "matchequity.h"

/* From pub_eval.c: */
extern float pubeval( int race, int pos[] );
static int CompareRedEvalData( const void *p0, const void *p1 );

extern int
EvaluatePositionCubeful1( int anBoard[ 2 ][ 25 ], float *prOutput, 
                          cubeinfo *pci, evalcontext *pec, 
                          int nPlies, int fCheckAutoRedoubles);

extern float
Cl2CfMoney ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci );

extern float
Cl2CfMatch ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci );

extern float
Cl2CfMatchOwned ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci );

extern float
Cl2CfMatchCentered ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci );

extern float
Cl2CfMatchUnavailable ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci );

extern float
EvalEfficiency( int anBoard[2][25], positionclass pc );

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

  I_BACKG, 
  
  I_BACKG1,
  
  I_FREEPIP,
  
  I_BACKRESCAPES,
  
  HALF_INPUTS };


#define NUM_INPUTS ( HALF_INPUTS * 2 )
#define NUM_RACE_INPUTS ( HALF_RACE_INPUTS * 2 )

static int anEscapes[ 0x1000 ];
static int anEscapes1[ 0x1000 ];

static neuralnet nnContact, nnBPG, nnRace;
static unsigned char *pBearoff1 = NULL, *pBearoff2 = NULL;
static cache cEval;
volatile int fInterrupt = FALSE, fAction = FALSE;
void ( *fnAction )( void ) = NULL;
int fMove, fCubeOwner, fJacoby = TRUE, fCrawford = FALSE;
int fPostCrawford = FALSE, nMatchTo, anScore[ 2 ], fBeavers = 1;
int nCube;
int fOutputMWC = TRUE;
float rCubeX = 2.0/3.0;

static cubeinfo ciBasic = { 1, 0, 0, { 1.0, 1.0, 1.0, 1.0 } };

static float arGammonPrice[ 4 ] = { 1.0, 1.0, 1.0, 1.0 };

static evalcontext ecBasic = { 0, 0, 0, 0, FALSE, FALSE };

typedef struct _evalcache {
    unsigned char auchKey[ 10 ];
    int nEvalContext;
    positionclass pc;
    float ar[ NUM_OUTPUTS ];
} evalcache;

static void ComputeTable0( void )
{
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

static int Escapes( int anBoard[ 25 ], int n ) {
    
    int i, af = 0;
    
    for( i = 0; i < 12 && i < n; i++ )
	if( anBoard[ 24 - n + i ] > 1 )
	    af |= ( 1 << i );
    
    return anEscapes[ af ];
}

static void ComputeTable1( void )
{
  int i, c, n0, n1, low;

  anEscapes1[ 0 ] = 0;
  
  for( i = 1; i < 0x1000; i++ ) {
    c = 0;

    low = 0;
    while( ! (i & (1 << low)) ) {
      ++low;
    }
    
    for( n0 = 0; n0 <= 5; n0++ )
      for( n1 = 0; n1 <= n0; n1++ ) {
	
	if( (n0 + n1 + 1 > low) &&
	    !( i & ( 1 << ( n0 + n1 + 1 ) ) ) &&
	    !( ( i & ( 1 << n0 ) ) && ( i & ( 1 << n1 ) ) ) ) {
	  c += ( n0 == n1 ) ? 1 : 2;
	}
      }
	
    anEscapes1[ i ] = c;
  }
}

static int Escapes1( int anBoard[ 25 ], int n ) {
    
    int i, af = 0;
    
    for( i = 0; i < 12 && i < n; i++ )
	if( anBoard[ 24 - n + i ] > 1 )
	    af |= ( 1 << i );
    
    return anEscapes1[ af ];
}


static void ComputeTable( void )
{
  ComputeTable0();
  ComputeTable1();
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

/* Make a plausible bearoff move (used to create approximate bearoff
   database). */
static int HeuristicBearoff( int anBoard[ 6 ], int anRoll[ 2 ] ) {

    int i, /* current die being played */
	c, /* number of dice to play */
	nMax, /* highest occupied point */
	anDice[ 4 ],
	n, /* point to play from */
	j, iSearch, nTotal;

    if( anRoll[ 0 ] == anRoll[ 1 ] ) {
	/* doubles */
	anDice[ 0 ] = anDice[ 1 ] = anDice[ 2 ] = anDice[ 3 ] = anRoll[ 0 ];
	c = 4;
    } else {
	/* non-doubles */
	assert( anRoll[ 0 ] > anRoll[ 1 ] );
	
	anDice[ 0 ] = anRoll[ 0 ];
	anDice[ 1 ] = anRoll[ 1 ];
	c = 2;
    }

    for( i = 0; i < c; i++ ) {
	for( nMax = 5; nMax >= 0 && !anBoard[ nMax ]; nMax-- )
	    ;

	if( nMax < 0 )
	    /* finished bearoff */
	    break;
    
	if( anBoard[ anDice[ i ] - 1 ] ) {
	    /* bear off exactly */
	    n = anDice[ i ] - 1;
	    goto move;
	}

	if( anDice[ i ] - 1 > nMax ) {
	    /* bear off highest chequer */
	    n = nMax;
	    goto move;
	}
	
	nTotal = anDice[ i ] - 1;
	for( j = i + 1; j < c; j++ ) {
	    nTotal += anDice[ j ];
	    if( nTotal < 6 && anBoard[ nTotal ] ) {
		/* there's a chequer we can bear off with subsequent dice;
		   do it */
		n = nTotal;
		goto move;
	    }
	}

	for( n = -1, iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
	    if( anBoard[ iSearch ] >= 2 && /* at least 2 on source point */
		!anBoard[ iSearch - anDice[ i ] ] && /* dest empty */
		( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ) )
		n = iSearch;
	if( n >= 0 )
	    goto move;

	/* find the point with the most on it (or least on dest) */
	for( iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
	    if( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ||
		( anBoard[ iSearch ] == anBoard[ n ] &&
		  anBoard[ iSearch - anDice[ i ] ] <
		  anBoard[ n - anDice[ i ] ] ) )
		n = iSearch;

    move:
	assert( n >= 0 );
	assert( anBoard[ n ] );
	anBoard[ n ]--;

	if( n >= anDice[ i ] )
	    anBoard[ n - anDice[ i ] ]++;
    }

    return PositionBearoff( anBoard );
}

static void GenerateBearoff( unsigned char *p, int nId ) {

    int i, iBest, anRoll[ 2 ], anBoard[ 6 ], aProb[ 32 ];
    unsigned short us;

    for( i = 0; i < 32; i++ )
	aProb[ i ] = 0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    PositionFromBearoff( anBoard, nId );
	    iBest = HeuristicBearoff( anBoard, anRoll );

	    assert( iBest >= 0 );
	    assert( iBest < nId );
	    
	    if( anRoll[ 0 ] == anRoll[ 1 ] )
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += p[ ( iBest << 6 ) | ( i << 1 ) ] +
			( p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 );
	    else
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += ( p[ ( iBest << 6 ) | ( i << 1 ) ] +
			( p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 ) ) << 1;
	}

    for( i = 0; i < 32; i++ ) {
	us = ( aProb[ i ] + 18 ) / 36;
	p[ ( nId << 6 ) | ( i << 1 ) ] = us & 0xFF;
	p[ ( nId << 6 ) | ( i << 1 ) | 1 ] = us >> 8;
    }
}

static unsigned char *HeuristicDatabase( int fProgress ) {

    unsigned char *p = malloc( 54264 * 32 * 2 );
    int i;
    
    if( !p )
	return NULL;

    p[ 0 ] = p[ 1 ] = 0xFF;
    for( i = 2; i < 64; i++ )
	p[ i ] = 0;

    for( i = 1; i < 54264; i++ ) {
	GenerateBearoff( p, i );
	if( fProgress && !( i % 1000 ) ) {
	    /* FIXME this progress indicator is not very good for GTK
	       (especially if fTTY is FALSE)... what else could be done? */
	    putchar( "\\|/-"[ ( i / 1000 ) % 4 ] );
	    putchar( '\r' );
	    fflush( stdout );
	}
    }

    return p;
}

extern int EvalInitialise( char *szWeights, char *szWeightsBinary,
			   char *szDatabase, char *szDir, int fProgress ) {
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

    pBearoff1 = NULL;
    
    if( szDatabase ) {
	if( ( h = PathOpen( szDatabase, szDir ) ) >= 0 ) {
	    /* FIXME fstat() h, to see what size database is available */
#if HAVE_MMAP
	    if( !( pBearoff1 = mmap( NULL, 54264 * 32 * 2 + 924 * 924 * 2,
				     PROT_READ, MAP_SHARED, h, 0 ) ) ) {
#endif
		if( !( pBearoff1 = malloc( 54264 * 32 * 2 +
					   924 * 924 * 2 ) ) ) {
		    perror( "malloc" );
		    return -1;
		}

		/* FIXME check for early EOF */
		if( read( h, pBearoff1, 54264 * 32 * 2 +
			  924 * 924 * 2 ) < 0 ) {
		    perror( szDatabase );
		    free( pBearoff1 );
		    pBearoff1 = NULL;
		}
#if HAVE_MMAP
	    }
#endif
	    close( h );
	}

	if( pBearoff1 )
	    pBearoff2 = pBearoff1 + 54264 * 32 * 2;
	else {
	    pBearoff1 = HeuristicDatabase( fProgress );
	    pBearoff2 = NULL;
	}
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
	else {
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


/* Calculates the inputs for one player only.  Returns 0 for contact
   positions, 1 for races. */
static void
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
    { 13, 31, 36, 38 }  /* 66 */
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

    exit(1);
  }
    
  afInput[ I_BREAK_CONTACT ] = n / (15 + 152.0);

  {
    unsigned int p  = 0;
    
    for( i = 0; i < nOppBack; i++ ) {
      if( anBoard[i] )
	p += (i+1) * anBoard[i];
    }
    
    afInput[I_FREEPIP] = p / 100.0;
  }
  
  {
    int t = 0;
    
    int no = 0;
      
    t += 24 * anBoard[24];
    no += anBoard[24];
      
    for( i = 23;  i >= 12 && i > nOppBack; --i ) {
      if( anBoard[i] && anBoard[i] != 2 ) {
	int n = ((anBoard[i] > 2) ? (anBoard[i] - 2) : 1);
	no += n;
	t += i * n;
      }
    }

    for( ; i >= 6; --i ) {
      if( anBoard[i] ) {
	int n = anBoard[i];
	no += n;
	t += i * n;
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

    afInput[ I_TIMING ] = t / 100.0;
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

  afInput[ I_BACKRESCAPES ] = Escapes1( anBoard, 23 - nOppBack ) / 36.0;
  
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

  {
    unsigned int nAc = 0;
    
    for( i = 18; i < 24; ++i ) {
      if( anBoard[i] > 1 ) {
	++nAc;
      }
    }
    
    afInput[I_BACKG] = 0.0;
    afInput[I_BACKG1] = 0.0;

    if( nAc >= 1 ) {
      unsigned int tot = 0;
      for( i = 18; i < 25; ++i ) {
	tot += anBoard[i];
      }

      if( nAc > 1 ) {
	/* assert( tot >= 4 ); */
      
	afInput[I_BACKG] = (tot - 3) / 4.0;
      } else if( nAc == 1 ) {
	afInput[I_BACKG1] = tot / 8.0;
      }
    }
  }
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

    {
      float noise = 1/10000.0;

      for(i = OUTPUT_WINGAMMON; i < 5; ++i) {
	if( arOutput[i] < noise ) {
	  arOutput[i] = 0.0;
	}
      }
    }
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
	PositionBearoff( anBoard[ 1 ] ) > 923 ||
	!pBearoff2 )
	return CLASS_BEAROFF1;

    return CLASS_BEAROFF2;
}

static void
EvalBearoff2( int anBoard[ 2 ][ 25 ], float arOutput[] )
{
  int n, nOpp;

  assert( pBearoff2 );
  
  nOpp = PositionBearoff( anBoard[ 0 ] );
  n = PositionBearoff( anBoard[ 1 ] );
  
  arOutput[ OUTPUT_WINGAMMON ] = arOutput[ OUTPUT_LOSEGAMMON ] =
    arOutput[ OUTPUT_WINBACKGAMMON ] =
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;
  
  arOutput[ OUTPUT_WIN ] =
    ( pBearoff2[ ( n * 924 + nOpp ) << 1 ] |
      ( pBearoff2[ ( ( n * 924 + nOpp ) << 1 ) | 1 ] << 8 ) ) / 65535.0;
}

/* Fill aaProb with one sided bearoff probabilities for position with id n */

#if defined( __GNUC__ )
inline
#endif
static void
getBearoffProbs(int n, int aaProb[32])
{
  int i;

  for( i = 0; i < 32; i++ )
    aaProb[ i ] = pBearoff1[ ( n << 6 ) | ( i << 1 ) ] +
      ( pBearoff1[ ( n << 6 ) | ( i << 1 ) | 1 ] << 8 );
}

/* Probability to get 1 checker off in 1,2 and 3 turns, when just */
/* considering which points are empty and which are not.          */
/* For example,                                                   */
/*   1 1 1 0 0 0  34/36  72/36^2  0/36^2                          */
/*                                                                */   
/* Is where points 1 to 3 are empty, and so a 1-2 would fail      */
/* to bear in the first turn.                                     */

static float oneCheckerOffTable[63][3] = {
/* 1 1 1 1 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 1 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 1 0 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 1 0 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 0 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 0 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 0 0 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 1 1 1 0 0 0  34/36  72/36^2  0/36^2 */
{ 0.9444444, 0.0555556, 0.0000000} ,
/* 1 1 0 1 1 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 1 1 0 1 1 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 1 1 0 1 0 1  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 1 1 0 1 0 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 1 1 0 0 1 1  33/36  106/36^2  2/36^2 */
{ 0.9166667, 0.0817901, 0.0015432} ,
/* 1 1 0 0 1 0  31/36  175/36^2  5/36^2 */
{ 0.8611111, 0.1350309, 0.0038580} ,
/* 1 1 0 0 0 1  33/36  103/36^2  5/36^2 */
{ 0.9166667, 0.0794753, 0.0038580} ,
/* 1 1 0 0 0 0  28/36  275/36^2  13/36^2 */
{ 0.7777778, 0.2121914, 0.0100309} ,
/* 1 0 1 1 1 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 1 0 1 1 1 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 1 0 1 1 0 1  33/36  106/36^2  2/36^2 */
{ 0.9166667, 0.0817901, 0.0015432} ,
/* 1 0 1 1 0 0  33/36  103/36^2  5/36^2 */
{ 0.9166667, 0.0794753, 0.0038580} ,
/* 1 0 1 0 1 1  33/36  106/36^2  2/36^2 */
{ 0.9166667, 0.0817901, 0.0015432} ,
/* 1 0 1 0 1 0  33/36  103/36^2  5/36^2 */
{ 0.9166667, 0.0794753, 0.0038580} ,
/* 1 0 1 0 0 1  29/36  238/36^2  14/36^2 */
{ 0.8055556, 0.1836420, 0.0108025} ,
/* 1 0 1 0 0 0  27/36  303/36^2  21/36^2 */
{ 0.7500000, 0.2337963, 0.0162037} ,
/* 1 0 0 1 1 1  32/36  144/36^2  0/36^2 */
{ 0.8888889, 0.1111111, 0.0000000} ,
/* 1 0 0 1 1 0  30/36  202/36^2  14/36^2 */
{ 0.8333333, 0.1558642, 0.0108025} ,
/* 1 0 0 1 0 1  30/36  198/36^2  18/36^2 */
{ 0.8333333, 0.1527778, 0.0138889} ,
/* 1 0 0 1 0 0  28/36  260/36^2  28/36^2 */
{ 0.7777778, 0.2006173, 0.0216049} ,
/* 1 0 0 0 1 1  28/36  272/36^2  16/36^2 */
{ 0.7777778, 0.2098765, 0.0123457} ,
/* 1 0 0 0 1 0  24/36  376/36^2  56/36^2 */
{ 0.6666667, 0.2901235, 0.0432099} ,
/* 1 0 0 0 0 1  24/36  368/36^2  64/36^2 */
{ 0.6666667, 0.2839506, 0.0493827} ,
/* 1 0 0 0 0 0  17/36  576/36^2  108/36^2 */
{ 0.4722222, 0.4444444, 0.0833333} ,
/* 0 1 1 1 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 1 1 1 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 1 1 1 0 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 1 1 1 0 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 1 1 0 1 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 0 1 1 0 1 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 0 1 1 0 0 1  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 0 1 1 0 0 0  33/36  107/36^2  1/36^2 */
{ 0.9166667, 0.0825617, 0.0007716} ,
/* 0 1 0 1 1 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 0 1 0 1 1 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 0 1 0 1 0 1  32/36  144/36^2  0/36^2 */
{ 0.8888889, 0.1111111, 0.0000000} ,
/* 0 1 0 1 0 0  32/36  128/36^2  16/36^2 */
{ 0.8888889, 0.0987654, 0.0123457} ,
/* 0 1 0 0 1 1  32/36  144/36^2  0/36^2 */
{ 0.8888889, 0.1111111, 0.0000000} ,
/* 0 1 0 0 1 0  30/36  202/36^2  14/36^2 */
{ 0.8333333, 0.1558642, 0.0108025} ,
/* 0 1 0 0 0 1  29/36  228/36^2  24/36^2 */
{ 0.8055556, 0.1759259, 0.0185185} ,
/* 0 1 0 0 0 0  24/36  394/36^2  38/36^2 */
{ 0.6666667, 0.3040123, 0.0293210} ,
/* 0 0 1 1 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 1 1 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 1 1 0 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 1 1 0 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 1 0 1 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 0 0 1 0 1 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 0 0 1 0 0 1  33/36  106/36^2  2/36^2 */
{ 0.9166667, 0.0817901, 0.0015432} ,
/* 0 0 1 0 0 0  31/36  175/36^2  5/36^2 */
{ 0.8611111, 0.1350309, 0.0038580} ,
/* 0 0 0 1 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 0 1 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 0 1 0 1  35/36  36/36^2  0/36^2 */
{ 0.9722222, 0.0277778, 0.0000000} ,
/* 0 0 0 1 0 0  35/36  35/36^2  1/36^2 */
{ 0.9722222, 0.0270062, 0.0007716} ,
/* 0 0 0 0 1 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 0 0 1 0  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
/* 0 0 0 0 0 1  36/36  0/36^2  0/36^2 */
{ 1.0000000, 0.0000000, 0.0000000} ,
};


/* Set gammon probabilities for bearoff position in anBoard, where at least */
/* one side has 15 chequers. bp0 and bp1 are the bearoff indices of sides 0 */
/* and 1, and g0/g1 are filled with gammon rate for side 0/1 respectivly    */

/* The method used is not perfect, but has very small error. The error comes */
/* from the assumption that 3 rolls are always enough to bear one men off,   */
/* and from considering only which points are empty and which are not.       */

static void
setGammonProb(int anBoard[ 2 ][ 25 ], int bp0, int bp1,
              float* g0, float* g1)
{
  int i; float* r;
  int prob[32];

  /* total checkers to side 0/1 */
  int tot0 = 0;
  int tot1 = 0;

  /* index into oneCheckerOffTable for side 0/1 */
  int t0 = 0;
  int t1 = 0;

  /* true when side 0 has 15 checkers left, false when side 1. When both has */
  /* more than 12 checkers left (at least 3 turns for both), gammon% is 0    */
	
  int side0;
  
  for(i = 5; i >= 0; --i) {
    t0 = 2*t0 + (anBoard[0][i] == 0);
    t1 = 2*t1 + (anBoard[1][i] == 0);
    
    tot0 += anBoard[0][i];
    tot1 += anBoard[1][i];
  }

  /* assert( tot0 == 15 || tot1 == 15 ); */
    
  /* assume that 3 turns are enough to get 1 checker off from any position */
  
  *g0 = 0.0;
  *g1 = 0.0;
  
  if( tot0 >= 12 && tot1 >= 12 ) {
    return;
  }

  side0 = tot0 == 15 ? 1 : 0;
  
  r = oneCheckerOffTable[side0 ? t0 : t1];

  getBearoffProbs(side0 ? bp1 : bp0, prob);

  {
    if( side0 ) {
      *g1 = ((prob[1] / 65535.0) +
	     (1 - r[0]) * (prob[2] / 65535.0) +
	     r[2] * (prob[3] / 65535.0));
    } else {
      *g0 = (prob[1] / 65535.0) * (1 - r[0]) + r[2] * (prob[2]/65535.0);
    }
  }
}  

extern unsigned long
EvalBearoff1Full( int anBoard[ 2 ][ 25 ], float arOutput[] )
{
  int i, j, n, nOpp, aaProb[ 2 ][ 32 ];
  unsigned long x;

  assert( pBearoff1 );
    
  nOpp = PositionBearoff( anBoard[ 0 ] );
  n = PositionBearoff( anBoard[ 1 ] );

  if( n > 38760 || nOpp > 38760 ) {
    /* a player has no chequers off; gammons are possible */
			
    setGammonProb(anBoard, nOpp, n,
		  &arOutput[ OUTPUT_LOSEGAMMON ],
		  &arOutput[ OUTPUT_WINGAMMON ] );
  } else {
    arOutput[ OUTPUT_WINGAMMON ] =
    arOutput[ OUTPUT_LOSEGAMMON ] =
    arOutput[ OUTPUT_WINBACKGAMMON ] =
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0;
  }

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

extern void
EvalBearoff1( int anBoard[ 2 ][ 25 ], float arOutput[] )
{
  EvalBearoff1Full( anBoard, arOutput );
}

enum {
  /* gammon possible by side on roll */
  G_POSSIBLE = 0x1,
  /* backgammon possible by side on roll */
  BG_POSSIBLE = 0x2,
  
  /* gammon possible by side not on roll */
  OG_POSSIBLE = 0x4,
  
  /* backgammon possible by side not on roll */
  OBG_POSSIBLE = 0x8,
};

static void
EvalRace(int anBoard[ 2 ][ 25 ], float arOutput[])
{
  float arInput[ NUM_INPUTS ];

  CalculateRaceInputs( anBoard, arInput );
    
  NeuralNetEvaluate( &nnRace, arInput, arOutput );

  /* anBoard[1] is on roll */
  {
    /* total men for side not on roll */
    int totMen0 = 0;
    
    /* total men for side on roll */
    int totMen1 = 0;

    /* a set flag for every possible outcome */
    int any = 0;
    
    int i;
    
    for(i = 23; i >= 0; --i) {
      totMen0 += anBoard[0][i];
      totMen1 += anBoard[1][i];
    }

    if( totMen1 == 15 ) {
      any |= OG_POSSIBLE;
    }
    
    if( totMen0 == 15 ) {
      any |= G_POSSIBLE;
    }

    if( any ) {
      if( any & OG_POSSIBLE ) {
	for(i = 23; i >= 18; --i) {
	  if( anBoard[1][i] > 0 ) {
	    break;
	  }
	}

	if( i >= 18 ) {
	  any |= OBG_POSSIBLE;
	}
      }

      if( any & G_POSSIBLE ) {
	for(i = 23; i >= 18; --i) {
	  if( anBoard[0][i] > 0 ) {
	    break;
	  }
	}

	if( i >= 18 ) {
	  any |= BG_POSSIBLE;
	}
      }
    }
    
    if( any & (BG_POSSIBLE | OBG_POSSIBLE) ) {
      /* side that can have the backgammon */
      
      int side = (any & BG_POSSIBLE) ? 1 : 0;

      int totMenHome = 0;
      int totPipsOp = 0;

      for(i = 0; i < 6; ++i) {
	totMenHome += anBoard[side][i];
      }
      
      for(i = 23; i >= 18; --i) {
	totPipsOp += anBoard[1-side][i] * (i-17);
      }

      if( (totMenHome + 3) / 4 - (side == 1 ? 1 : 0) <= (totPipsOp + 2) / 3 ) {
	int dummy[2][25];
	float p[5];
	
	for(i = 0; i < 25; ++i) {
	  dummy[side][i] = anBoard[side][i];
	}

	for(i = 0; i < 6; ++i) {
	  dummy[1-side][i] = anBoard[1-side][18+i];
	}
	for(i = 6; i < 25; ++i) {
	  dummy[1-side][i] = 0;
	}

	if( PositionBearoff( dummy[ 0 ] ) > 923 ||
	    PositionBearoff( dummy[ 1 ] ) > 923 ) {
	  EvalBearoff1(dummy, p);
	} else {
	  EvalBearoff2(dummy, p);
	}

	if( side == 1 ) {
	  arOutput[OUTPUT_WINBACKGAMMON] = p[0];
	} else {
	  arOutput[OUTPUT_LOSEBACKGAMMON] = 1 - p[0];
	}
  
      } else {
	if( side == 1 ) {
	  arOutput[OUTPUT_WINBACKGAMMON] = 0.0;
	} else {
	  arOutput[OUTPUT_LOSEBACKGAMMON] = 0.0;
	}
      }
    }  
  }
  
  /* sanity check will take care of rest */
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
     *     - evaluate all 21 rolls at 0-ply
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

  if ( ! ( nMatchTo && nPlies ) ) {

    /* only cache 0-ply evals for match play */
    /* FIXME: cache 0+ ply evals for match play */

    l = EvalCacheHash( &ec );
    
    if( ( pec = CacheLookup( &cEval, l, &ec ) ) ) {
      memcpy( arOutput, pec->ar, sizeof( pec->ar ) );

      return 0;
    }
  }
    
  if( EvaluatePositionFull( anBoard, arOutput, pci, pecx, nPlies, pc ) )
    return -1;

  memcpy( ec.ar, arOutput, sizeof( ec.ar ) );

  if ( ! ( nMatchTo && nPlies ) ) 
    return CacheAdd( &cEval, l, &ec, sizeof ec );
  else
    return 0;
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


extern void InvertEvaluationCf( float ar[ 4 ] ) {		

  int i;

  for ( i = 0; i < 4; i++ ) {

    ar[ i ] = -ar[ i ];

  }
  
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

    return
      ar [ OUTPUT_WIN ] * 2.0 - 1.0
      + ar[ OUTPUT_WINGAMMON ] *
      pci -> arGammonPrice[ pci -> fMove ] 
      - ar[ OUTPUT_LOSEGAMMON ] *
      pci -> arGammonPrice[ ! pci -> fMove ] 
      + ar[ OUTPUT_WINBACKGAMMON ] *
      pci -> arGammonPrice[ 2 + pci -> fMove ] 
      - ar[ OUTPUT_LOSEBACKGAMMON ] *
      pci -> arGammonPrice[ 2 + ! pci -> fMove ];

  }
		
}



extern float 
mwc2eq ( float rMwc, cubeinfo *ci ) {


  int nCube = ci->nCube;
  int fWho  = ci->fMove;

  /* normalized score */

  int nScore0 = nMatchTo - anScore[ fWho ];
  int nScore1 = nMatchTo - anScore[ ! fWho ];
  
  /* mwc if I win/lose */

  float rMwcWin, rMwcLose;

  if ( fCrawford || fPostCrawford ) {
    if ( nScore0 == 1 ) {
      rMwcWin = 1.0;
      rMwcLose = 1.0 - GET_METPostCrawford ( nScore1 - nCube - 1, afMETPostCrawford );
    }
    else {
      rMwcWin = GET_METPostCrawford ( nScore0 - nCube - 1, afMETPostCrawford );
      rMwcLose = 0.0;
    }
  }
  else {
    rMwcWin = GET_MET ( nScore0 - nCube - 1, nScore1 - 1, aafMET );
    rMwcLose = GET_MET ( nScore0 - 1, nScore1 - nCube - 1, aafMET );
  }

  /* 
   * make linear inter- or extrapolation:
   * equity       mwc
   *  -1          rMwcLose
   *  +1          rMwcWin
   *
   * Interpolation formula:
   *
   *       2 * rMwc - ( rMwcWin + rMwcLose )
   * rEq = ---------------------------------
   *            rMwcWin - rMwcLose
   *
   * FIXME: numerical problems?
   * If you are trailing 30-away, 1-away the difference between
   * 29-away, 1-away and 30-away, 0-away is not very large, and it may
   * give numerical problems.
   *
   */

  return ( 2.0 * rMwc - ( rMwcWin + rMwcLose ) ) / 
    ( rMwcWin - rMwcLose );

}

extern float 
eq2mwc ( float rEq, cubeinfo *ci ) {

  int nCube = ci->nCube;
  int fWho  = ci->fMove;

  /* normalized score */

  int nScore0 = nMatchTo - anScore[ fWho ];
  int nScore1 = nMatchTo - anScore[ ! fWho ];

  /* mwc if I win/lose */

  float rMwcWin, rMwcLose;

  if ( fCrawford || fPostCrawford ) {
    if ( nScore0 == 1 ) {
      rMwcWin = 1.0;
      rMwcLose = 1.0 - GET_METPostCrawford ( nScore1 - nCube - 1, afMETPostCrawford );
    }
    else {
      rMwcWin = GET_METPostCrawford ( nScore0 - nCube - 1, afMETPostCrawford );
      rMwcLose = 0.0;
    }
  }
  else {
    rMwcWin = GET_MET ( nScore0 - nCube - 1, nScore1 - 1, aafMET );
    rMwcLose = GET_MET ( nScore0 - 1, nScore1 - nCube - 1, aafMET );
  }
  
  /*
   * Linear inter- or extrapolation.
   * Solve the formula in the routine above (mwc2eq):
   *
   *        rEq * ( rMwcWin - rMwcLose ) + ( rMwcWin + rMwcLose )
   * rMwc = -----------------------------------------------------
   *                                   2
   */

  return 
    0.5 * ( rEq * ( rMwcWin - rMwcLose ) + ( rMwcWin + rMwcLose ) );

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

    for ( i = 0; i < NUM_OUTPUTS; i++ )
      pm->arEvalMove[ i ] = 0.0;
    
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
    float arEvalCf[ 4 ];
    cubeinfo ci;
    
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

      /* swap fMove in cubeinfo */

      memcpy ( &ci, pci, sizeof (ci) );
      ci.fMove = ! ci.fMove;

      if ( pec->fCubeful ) {

        if ( EvaluatePositionCubeful ( anBoardTemp, arEvalCf, &ci,
                                       pec, nPlies ) ) 
          return -1;

        // InvertEvaluation ( arEval );
        InvertEvaluationCf ( arEvalCf );
        arEval[ 0 ] = arEval[ 1 ] = arEval[ 2 ] = arEval[ 3 ] =
          arEval[ 4 ] = 0.0;
                                       

      } else {

        if( EvaluatePositionCache( anBoardTemp, arEval, &ci, pec, nPlies,
                                   pec->fRelativeAccuracy ? pcGeneral :
                                   ClassifyPosition( anBoardTemp ) ) )
          return -1;

        InvertEvaluation( arEval );
        arEvalCf[ 0 ] = 0.0;

      }

      /* Save evaluations */
      
      memcpy( pml->amMoves[ i ].arEvalMove, arEval,
              sizeof( arEval ) );

      /* Save evaluation setup */

      pml->amMoves[ i ].etMove = EVAL_EVAL;
      pml->amMoves[ i ].esMove.ec = *pec;
      pml->amMoves[ i ].esMove.ec.nPlies = nPlies;

      /* Score for move:
         rScore is the primary score (cubeful/cubeless)
         rScore2 is the secondary score (cubeless) */
      
      pml->amMoves[ i ].rScore = 
        (pec->fCubeful) ? arEvalCf[ 0 ] : Utility ( arEval, pci );
      pml->amMoves[ i ].rScore2 = Utility ( arEval, pci );

      if( ( pml->amMoves[ i ].rScore > pml->rBestScore ) || 
          ( ( pml->amMoves[ i ].rScore == pml->rBestScore ) 
            && ( pml->amMoves[ i ].rScore2 > 
                 pml->amMoves[ pml->iMoveBest ].rScore2 ) ) ) {
        pml->iMoveBest = i;
        pml->rBestScore = pml->amMoves[ i ].rScore;
      };
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
FindBestMoves( movelist *pml,
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


extern int 
FindnSaveBestMoves( movelist *pml,
                  int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
                  unsigned char *auchMove,
                  cubeinfo *pci, evalcontext *pec ) {

  /* Find best moves. 
     Ensure that auchMove is present in the list. */

  int i, j, nMoves, iPly;
  positionclass pc;
  move *pm;
#if __GNUC__
  move amCandidates[ pec->nSearchCandidates ];
#elif HAVE_ALLOCA
  move* amCandidates = alloca( pec->nSearchCandidates * sizeof( move ) );
#else
  move amCandidates[ MAX_SEARCH_CANDIDATES ];
#endif
    
  /* Find all moves -- note that pml contains internal pointers to static
     data, so we can't call GenerateMoves again (or anything that calls
     it, such as ScoreMoves at more than 0 plies) until we have saved
     the moves we want to keep in amCandidates. */
  
  GenerateMoves( pml, anBoard, nDice0, nDice1, FALSE );

  /* Save moves */

  pm = (move *) malloc ( pml->cMoves * sizeof ( move ) );
  memcpy( pm, pml->amMoves, pml->cMoves * sizeof( move ) );    
  pml->amMoves = pm;

  nMoves = pml->cMoves;

  if ( pml->cMoves == 0 ) 
    /* no legal moves */
    return 0;

  /* Evaluate all moves at 0-ply */

  if( ( pc = ScoreMoves( pml, pci, pec, 0, CLASS_ANY ) ) < 0 )
    return -1;

  for ( iPly = 0; iPly < pec->nPlies; iPly++ ) {

    /* search tolerance at iPly */

    float rTol = pec->rSearchTolerance / ( 1 << iPly );

    /* larger threshold for 0-ply evaluations */

    if ( pec->nPlies == 0 && rTol < 0.25 )
      rTol = 0.25;

    /* Eliminate moves whose scores are below the threshold from the
       best except for auchMove */

    for( i = 0, j = 0; i < pml->cMoves; i++ ) {

      int fMustHave = 
        ( auchMove && EqualKeys ( auchMove, pml->amMoves[ i ].auch ) );
      
      if( fMustHave ||
          pml->amMoves[ i ].rScore >= pml->rBestScore - rTol ) {

        if( i != j ) {
          move m = pml->amMoves[ j ];
          pml->amMoves[ j ] = pml->amMoves[ i ];
          pml->amMoves[ i ] = m;
        }
        
        j++;
      }
    }

    /* Sort moves in descending order */
    qsort( pml->amMoves, j, sizeof( move ), (cfunc) CompareMoves );

    pml->iMoveBest = 0;

    /* Consider only those better than the threshold or as many as were
       requested, whichever is less */

    pml->cMoves = ( j < ( pec->nSearchCandidates >> iPly ) ? j :
                    ( pec->nSearchCandidates >> iPly ) );

    /* Calculate the full evaluations at the search depth requested */
    if( ScoreMoves( pml, pci, pec, iPly + 1, pc ) < 0 )
	return -1;

  }

  /* Using a deeper search might change the order of the evaluations;
     reorder them according to the latest evaluation */

  pml->cMoves = nMoves;

  if( pec->nPlies  )
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
           "MOBILITY       \t%5.3f             \t%6.3f\n"
           "MOMENT2        \t%5.3f             \t%6.3f\n"
           "ENTER          \t%5.3f (%5.3f/12) \t%6.3f\n"
					 
					 "ENTER2         \t%5.3f \n"
					 "TIMING         \t%5.3f \n"
					 "BACKBONE       \t%5.3f \n"
					 "BACKGAME       \t%5.3f \n"
					 "BACKGAME1      \t%5.3f \n"
					 "FREEPIP        \t%5.3f \n"
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
           arInput[ I_MOBILITY << 1 ], ardEdI[ I_MOBILITY << 1 ],
           arInput[ I_MOMENT2 << 1 ], ardEdI[ I_MOMENT2 << 1 ],
           arInput[ I_ENTER << 1 ], arInput[ I_ENTER << 1 ] * 12.0,
	         ardEdI[ I_ENTER << 1 ],

					 arInput[ I_ENTER2 ],
					 arInput[ I_TIMING ],
					 arInput[ I_BACKBONE ],
					 arInput[ I_BACKG ],
					 arInput[ I_BACKG1 ],
					 arInput[ I_FREEPIP ]

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

  if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
    strcpy( szOutput, "\n       \tWin  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
	    "Equity  (cubeful)\n" );
  else
    strcpy( szOutput, "\n       \tWin  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
	    "Mwc     (cubeful)\n" );

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
	
    if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
      sprintf( szOutput,
	       ":\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t"
	       "(%+6.3f  (%+6.3f))\n",
	       arOutput[ 0 ], arOutput[ 1 ],
	       arOutput[ 2 ], arOutput[ 3 ],
	       arOutput[ 4 ], Utility ( arOutput, &ci ), 
	       arCfOutput[ 0] ); 
    else {
      sprintf( szOutput,
	       ":\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t"
	       "(%6.2f%% (%6.2f%%))\n",
	       arOutput[ 0 ], arOutput[ 1 ],
	       arOutput[ 2 ], arOutput[ 3 ],
	       arOutput[ 4 ], 
	       100.0 * eq2mwc ( Utility ( arOutput, &ci ), &ci ), 
	       100.0 * eq2mwc ( arCfOutput[ 0], &ci ) ); 
    }
  }

  /* if cube is available, output cube action */

  SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

  if ( GetDPEq ( NULL, NULL, &ci ) ) {

    szOutput = strchr( szOutput, 0 );    
    sprintf( szOutput, "\n\n" );
    szOutput = strchr( szOutput, 0 );    
    GetCubeActionSz ( arCfOutput, szOutput, &ci );

  }
  
  return 0;
}


extern int 
GetCubeActionSz ( float arDouble[ 4 ], char *szOutput, cubeinfo *pci ) {

  /* write string with cube action */

  /* FIXME: write equity and mwc for match play */

  if ( arDouble[ 2 ] >= arDouble[ 1 ] && arDouble[ 3 ] >= arDouble[ 1 ] ) {

    /* it's correct to double */

    if ( arDouble[ 2 ] < arDouble[ 3 ] ) {

      /* Optimal     : Double, take
         Best for me : Double, pass
         Worst for me: No Double */

      if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
	sprintf ( szOutput,
		  "Double, take  : %+6.3f\n"
		  "Double, pass  : %+6.3f   (%+6.3f)\n"
		  "No double     : %+6.3f   (%+6.3f)\n\n"
		  "Correct cube action: Double, take",
		  arDouble[ 2 ],
		  arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 2 ],
		  arDouble[ 1 ], arDouble[ 1 ] - arDouble[ 2 ] );
      else
	sprintf ( szOutput,
		  "Double, take  : %6.2f%%\n"
		  "Double, pass  : %6.2f%%   (%+6.2f%%)\n"
		  "No double     : %6.2f%%   (%+6.2f%%)\n\n"
		  "Correct cube action: Double, take",
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ) -
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ), 
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ) - 
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ) );

    }
    else {

      /* Optimal     : Double, pass
         BEst for me : Double, take
         Worst for me: no double */

      if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
	sprintf ( szOutput,
		  "Double, pass  : %+6.3f\n"
		  "Double, take  : %+6.3f   (%+6.3f)\n"
		  "No double     : %+6.3f   (%+6.3f)\n\n"
		  "Correct cube action: Double, pass",
		  arDouble[ 3 ],
		  arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 3 ],
		  arDouble[ 1 ], arDouble[ 1 ] - arDouble[ 3 ] );
      else
	sprintf ( szOutput,
		  "Double, pass  : %+6.2f%%\n"
		  "Double, take  : %+6.2f%%   (%+6.2f%%)\n"
		  "No double     : %+6.2f%%   (%+6.2f%%)\n\n"
		  "Correct cube action: Double, pass",
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ) -
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ), 
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ) - 
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ) );
    }

  } 
  else {

    /* it's correct not to double */

    if ( arDouble[ 1 ] > arDouble[ 3 ] ) {

      if ( arDouble[ 2 ] > arDouble[ 3 ] ) {

        /* Optimal     : no double
           Best for me : Double, take
           Worst for me: Double, pass */

	if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
	  sprintf ( szOutput,
		    "No double     : %+6.3f\n"
		    "Double, take  : %+6.3f   (%+6.3f)\n"
		    "Double, pass  : %+6.3f   (%+6.3f)\n\n"
		    "Correct cube action: too good to double, pass",
		    arDouble[ 1 ],
		    arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ],
		    arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ] );
	else
	  sprintf ( szOutput,
		    "No double     : %+6.2f%%\n"
		    "Double, take  : %+6.2f%%   (%+6.2f%%)\n"
		    "Double, pass  : %+6.2f%%   (%+6.2f%%)\n\n"
		    "Correct cube action: too good to double, pass",
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 2 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 2 ], pci ) -
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 3 ], pci ), 
		    100.0 * eq2mwc ( arDouble[ 3 ], pci ) - 
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ) );

      }
      else {
        
        /* Optimal     : no double
           Best for me : Double, pass
           Worst for me: Double, take */

        /* This situation may arise in match play. */

	if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
	  sprintf ( szOutput,
		    "No double     : %+6.3f\n"
		    "Double, pass  : %+6.3f   (%+6.3f)\n"
		    "Double, take  : %+6.3f   (%+6.3f)\n\n"
		    "Correct cube action: too good to double, take(!)",
		    arDouble[ 1 ],
		    arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ],
		    arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ] );
	else
	  sprintf ( szOutput,
		    "No double     : %+6.2f%%\n"
		    "Double, pass  : %+6.2f%%   (%+6.2f%%)\n"
		    "Double, take  : %+6.2f%%   (%+6.2f%%)\n\n"
		    "Correct cube action: too good to double, take(!)",
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 3 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 3 ], pci ) -
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		    100.0 * eq2mwc ( arDouble[ 2 ], pci ), 
		    100.0 * eq2mwc ( arDouble[ 2 ], pci ) - 
		    100.0 * eq2mwc ( arDouble[ 1 ], pci ) );

      }

    }
    else { /* arDouble[ 2 ] < arDouble[ 1 ] */
    
      /* Optimal     : no double
         Best for me : Double, pass
         Worst for me: Double, take */

      if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) )
        sprintf ( szOutput,
                  "No double     : %+6.3f\n"
                  "Double, pass  : %+6.3f   (%+6.3f)\n"
                  "Double, take  : %+6.3f   (%+6.3f)\n\n"
                  "Correct cube action: No double, take",
                  arDouble[ 1 ],
                  arDouble[ 3 ], arDouble[ 3 ] - arDouble[ 1 ],
                  arDouble[ 2 ], arDouble[ 2 ] - arDouble[ 1 ] );
      else
        sprintf ( szOutput,
                  "No double     : %+6.2f%%\n"
                  "Double, pass  : %+6.2f%%   (%+6.2f%%)\n"
                  "Double, take  : %+6.2f%%   (%+6.2f%%)\n\n"
                  "Correct cube action: No double, take",
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 3 ], pci ) -
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ),
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ), 
		  100.0 * eq2mwc ( arDouble[ 2 ], pci ) - 
		  100.0 * eq2mwc ( arDouble[ 1 ], pci ) );
    }

  }

  return 0;
}


extern int EvalCacheResize( int cNew ) {

  return CacheResize( &cEval, cNew );
}

extern int EvalCacheStats( int *pc, int *pcLookup, int *pcHit ) {

  *pc = cEval.c;
    
  return CacheStats( &cEval, pcLookup, pcHit );
}

extern int FindPubevalMove( int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
			    int anMove[ 8 ] ) {

  movelist ml;
  int i, j, anBoardTemp[ 2 ][ 25 ], anPubeval[ 28 ], fRace;

  for( i = 0; i < 8; i++ )
      anMove[ i ] = -1;
  
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

  if( anMove )
      for( i = 0; i < ml.cMaxMoves * 2; i++ )
	  anMove[ i ] = ml.amMoves[ ml.iMoveBest ].anMove[ i ];
  
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

    /*
     * Match play.
     * FIXME: calculate gammon price when initializing program
     * instead of recalculating it again and again, or cache it.
     */
              
    int nScore0 = NORM_SCORE ( anScore[ 0 ] );
    int nScore1 = NORM_SCORE ( anScore[ 1 ] );

    if ( fCrawford || fPostCrawford ) {

      if ( nScore0 == 1 ) {

	float rLose = 1.0 - GET_METPostCrawford ( nScore1 - nCube - 1, afMETPostCrawford );
	float rLoseGammon = 
	  1.0 - GET_METPostCrawford ( nScore1 - nCube * 2 - 1, afMETPostCrawford );
	float rLoseBG =
	  1.0 - GET_METPostCrawford ( nScore1 - nCube * 3 - 1, afMETPostCrawford );

	float rCenter = ( 1.0 + rLose ) / 2.0;

	ci->arGammonPrice[ 0 ] = 0.0; 
	ci->arGammonPrice[ 2 ] = 0.0;

	ci->arGammonPrice[ 1 ] = 
	  ( rCenter - rLoseGammon ) / ( 1.0 - rCenter ) - 1.0;

	ci->arGammonPrice[ 3 ] = 
	  ( rCenter - rLoseBG ) / ( 1.0 - rCenter ) - 
	  ( ci->arGammonPrice[ 1 ] + 1.0 );

      }
      else {

	float rWin = GET_METPostCrawford ( nScore0 - nCube - 1, afMETPostCrawford );
	float rWinGammon = 
	  GET_METPostCrawford ( nScore0 - nCube * 2 - 1, afMETPostCrawford );
	float rWinBG = 
	  GET_METPostCrawford ( nScore0 - nCube * 3 - 1, afMETPostCrawford );

	ci->arGammonPrice[ 0 ] =
	  2.0 * rWinGammon / rWin - 2.0;

	ci->arGammonPrice[ 2 ] =
	  2.0 * rWinBG / rWin - ( ci->arGammonPrice[ 0 ] + 2.0 );

	ci->arGammonPrice[ 1 ] = 0.0;
	ci->arGammonPrice[ 3 ] = 0.0;
	
      }

    }
    else {
      float rWin = 
	GET_MET ( nScore0 - nCube - 1, nScore1 - 1, aafMET );
      float rLose =
	GET_MET ( nScore0 - 1, nScore1 - nCube - 1, aafMET );
      float rWinGammon =
	GET_MET ( nScore0 - nCube * 2 - 1, nScore1 - 1, aafMET );
      float rLoseGammon =
	GET_MET ( nScore0 - 1, nScore1 - nCube * 2 - 1, aafMET );
      float rWinBG =
	GET_MET ( nScore0 - nCube * 3 - 1, nScore1 - 1, aafMET );
      float rLoseBG =
	GET_MET ( nScore0 - 1, nScore1 - nCube * 3 - 1, aafMET );

      float rCenter = ( rWin + rLose ) / 2.0;

      /* FIXME: correct numerical problems in a better way, than done
         below. If cube is dead gammon or backgammon price might be a
         small negative number. For example, at -2,-3 with cube on 2
         the current code gives: 0.9090..., 0, -2.7e-8, 0 instead
         of the correct 0.9090..., 0, 0, 0. */

      ci->arGammonPrice[ 0 ] = 
        ( rWinGammon - rCenter ) / ( rWin - rCenter ) - 1.0;
      ci->arGammonPrice[ 1 ] = 
	( rCenter - rLoseGammon ) / ( rWin - rCenter ) - 1.0;
      ci->arGammonPrice[ 2 ] = 
	( rWinBG - rCenter ) / ( rWin - rCenter ) - 
	( ci->arGammonPrice[ 0 ] + 1.0 );
      ci->arGammonPrice[ 3 ] = 
	( rCenter - rLoseBG ) / ( rWin - rCenter ) - 
	( ci->arGammonPrice[ 1 ] + 1.0 );

    } /* Crawford || PostCrawford */

  } /* match play */

  /* Correct numerical problems */

  if ( ci->arGammonPrice[ 0 ] < 0 )
    ci->arGammonPrice[ 0 ] = 0.0;
  if ( ci->arGammonPrice[ 1 ] < 0 )
    ci->arGammonPrice[ 1 ] = 0.0;
  if ( ci->arGammonPrice[ 2 ] < 0 )
    ci->arGammonPrice[ 2 ] = 0.0;
  if ( ci->arGammonPrice[ 3 ] < 0 )
    ci->arGammonPrice[ 3 ] = 0.0;

  if ( ci->arGammonPrice[ 0 ] < 0 )
     abort();
  if ( ci->arGammonPrice[ 1 ] < 0 )
     abort();
  if ( ci->arGammonPrice[ 2 ] < 0 )
     abort();
  if ( ci->arGammonPrice[ 3 ] < 0 )
     abort();

  return 0;
}


extern int 
EvaluatePositionCubeful( int anBoard[ 2 ][ 25 ], float arCfOutput[],
                         cubeinfo *pci, evalcontext *pec, int nPlies ) {

  /* Calculate cubeful equity. 
     
     Output is: arCfOutput[ 0 ]: equity for optimal cube action,
                arCfOutput[ 1 ]: equity for no double,
                arCfOutput[ 2 ]: equity for double, take,
                arCfOutput[ 3 ]: equity for double, pass.

     This is the top-level routine. If it is a n-ply (n>1) evaluation
     we are going to do a "no double" evaluation and a "double
     evaluation". The recursion is done in EvaluatePositionCubeful1.

     The cubeful equity is the optimal (assuming both players play
     optimal) of the "no double", "double, take", and "double, pass"
     equities. 

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
  arCfOutput[ 2 ] = 0.0;

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
                                        nPlies - 1, FALSE ) < 0 )
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
    
    EvaluatePositionCubeful1 ( anBoard, &arCfOutput[ 1 ], pci,
                               pec, 0, FALSE );
  }

  /* Double branch */

  if ( fCube ) {

    /* nCube is doubled; Cube owner is opponent */

    SetCubeInfo ( &ciD, 2 * pci -> nCube, ! pci -> fMove, pci -> fMove );
    SetCubeInfo ( &ciR, 2 * pci -> nCube, ! pci -> fMove, ! pci -> fMove );

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

          FindBestMovePlied ( NULL, n0, n1, anBoardNew, &ciD, pec, 0 );

          SwapSides ( anBoardNew );

          if ( EvaluatePositionCubeful1 ( anBoardNew, &r, &ciR, pec,
                                          nPlies - 1, TRUE ) < 0 )
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
                                 pec, 0, TRUE ); 

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

  /* convert mwc to equity */

  if ( nMatchTo )
    for ( i = 0; i < 4; i++ )
      arCfOutput[ i ] = mwc2eq ( arCfOutput[ i ], pci );

  return 0;

}


extern int
fDoCubeful ( cubeinfo *pci ) {

  int fNoCF;

  /* cube is dead */

  fNoCF = 
    ( anScore[ pci -> fMove ] + pci -> nCube >= nMatchTo );
  fNoCF = fNoCF ||
    ( anScore[ ! pci -> fMove ] + pci -> nCube >= nMatchTo );

  return ! fNoCF;
  /* score is -2,-2 */

  fNoCF = fNoCF ||
    ( ( anScore[ pci -> fMove ] == nMatchTo - 2 ) &&
      ( anScore[ ! pci -> fMove ] == nMatchTo - 2 ) );

  return ! fNoCF;

}

    
extern int
EvaluatePositionCubeful1( int anBoard[ 2 ][ 25 ], float *prOutput, 
                          cubeinfo *pci, evalcontext *pec, 
                          int nPlies, int fCheckAutoRedoubles) { 

  /* Calculate cubeful equity.

     nPlies >0 and the game isn't over:
     ----------------------------------

     To save computer time, we don't take both the "no double" and
     "double, take" branch as we did on the top level. Instead, we
     first do a 0-ply cube action decision, and decide which branch to
     take. 

     Then do a normal evaluation:

     Loop over 21 dice rolls
        Find Best Move
        Swap Sides
        EvaluatePositionCubeful1 ( nPlies - 1 )
     End Loop


     nPlies = 0:
     -----------

     Use Rick Janowskis formulae for converting cubeless equity to
     cubeful equity. 


     Game over:
     ----------

     When the game is over you're not very likely to use the cube
     (unless you throw it at your opponent, if you believe she has
     been very lucky), so

     Equity (cubeless) = Equity ( cubeful ).

  */
    

  positionclass pc;

  int anBoardNew[ 2 ][ 25 ];
  
  if ( (  pc = ClassifyPosition( anBoard ) ) != CLASS_OVER &&
       nPlies > 0 ) {
    
    /* internal node; recurse */

    /* Do a 0-ply cube decision */

    float rND, rDT, rDP, r;
    int fCube, n0, n1, i, fDoubleBranch;
    cubeinfo ciD, *pciE, ciND, ciR;

    GetDPEq ( &fCube, &rDP, pci );

    SetCubeInfo ( &ciND, pci -> nCube, pci -> fCubeOwner,
                  ! pci -> fMove );

    fDoubleBranch = 0;

    if ( fCube ) {

      /* we have access a non-dead cube */

      if ( EvaluatePositionCubeful1 ( anBoard, &rND, pci,
                                      pec, 0, TRUE ) < 0 )
        return -1;

      SetCubeInfo ( &ciD, 2 * pci -> nCube, ! pci -> fMove, 
                    pci -> fMove );

      if ( EvaluatePositionCubeful1 ( anBoard, &rDT, &ciD,
                                      pec, 0, TRUE ) 
           < 0 )
        return -1;

      if ( ! nMatchTo )
        rDT *= 2.0;

      if ( ( rDT >= rND ) && ( rDP >= rND ) ) {

        /* we have a double */

	if ( rDT > rDP ) {

	  /* Double, pass:
	     No need to do recursion. Equity is just +1.0. */

	  *prOutput = rDP;
	  return 0;

	}
	else {

	  /* Double, take:
	     Take "double, take" branch in evaluation below. */

	  pciE = &ciD;
	  SetCubeInfo ( &ciR, 2 * pci -> nCube, 
			! pci -> fMove, ! pci -> fMove );
	  fDoubleBranch = 1;

	}
      }
      else {
        /* no double:
	   Take "no double" branch in the evaluation below. */
        pciE = &ciND;
        SetCubeInfo ( &ciR, pci -> nCube, 
                      pci -> fCubeOwner, ! pci -> fMove );
        fDoubleBranch = 0;
      }

    } 
    else {
      /* We can't double, so do the "no double" evaluation. */
      pciE = &ciND;
      SetCubeInfo ( &ciR, pci -> nCube, 
                    pci -> fCubeOwner, ! pci -> fMove );
    }

    *prOutput = 0.0;

    /* Do normal evaluation and recurse. */

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

        if ( EvaluatePositionCubeful1 ( anBoardNew, &r, &ciR, pec,
                                        nPlies - 1, fDoubleBranch ) < 0 )
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

    /* At leaf node or game over: use static evaluation.
       Call EvaluatePostion to ensure that the evaluation
       is cached. */

    float arOutput [ NUM_OUTPUTS ], rEq;

    /* check for automatic redouble */

    if ( nMatchTo && ! fCrawford && fCheckAutoRedoubles ) {

      /* Does the opponent has an automatic (re)double? */

      if ( ( nMatchTo - anScore[ pci -> fMove ] - pci -> nCube <= 0 )
           && ( nMatchTo - anScore[ ! pci -> fMove ] - pci -> nCube > 0 )
           && ( pci -> fCubeOwner != pci -> fMove ) ) {

        SetCubeInfo ( pci, pci -> nCube * 2, 
                      pci -> fMove, pci -> fMove );

      }

      /* Do I have an automatic (re)double? */

      /*

      if ( ( nMatchTo - anScore[ ! pci -> fMove ] - pci -> nCube <= 0 )
           && ( nMatchTo - anScore[ pci -> fMove ] - pci -> nCube > 0 )
           && ( pci -> fCubeOwner != ! pci -> fMove ) ) {

        SetCubeInfo ( pci, pci -> nCube * 2, 
                      ! pci -> fMove, pci -> fMove );

      }
      */

    }

    EvaluatePosition ( anBoard, arOutput, pci, 0 );

    SanityCheck ( anBoard, arOutput );
    //4NvBwQDg5+ABIQ
    rEq = Utility ( arOutput, pci );

    if ( pc == CLASS_OVER || ( nMatchTo && ! fDoCubeful( pci ) ) ) {

      /* if the game is over, there is very little value
         of holding the cube */

      if ( ! nMatchTo )
        *prOutput = rEq;
      else
        *prOutput = eq2mwc ( rEq, pci );

    }
    else {

      rCubeX = EvalEfficiency ( anBoard, pc );

      if ( ! nMatchTo )
	*prOutput = Cl2CfMoney ( arOutput, pci );
      else
	*prOutput = Cl2CfMatch ( arOutput, pci );

    }/* CLASS_OVER */
      
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

    if ( prDPEq ) {

      if ( fPostCrawford || fCrawford ) {
	if ( nMatchTo - anScore[ pci -> fMove ]  == 1 )
	  *prDPEq = 1.0;
	else
	  *prDPEq =
	    GET_METPostCrawford ( nMatchTo - anScore [ pci -> fMove ] - 1 - pci -> nCube,
			 afMETPostCrawford );
      }
      else
	*prDPEq =
	  GET_MET ( nMatchTo - anScore[ pci -> fMove ] - 1 - pci -> nCube,
		   nMatchTo - anScore[ ! pci -> fMove ] - 1, aafMET );
    }

    if ( pfCube )
      *pfCube = fCube;
      
  }

  return fCube;

}


extern float
Cl2CfMoney ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci ) {

  float rW, rL;
  float rOppTG, rOppCP, rCP, rTG;
  float rOppIDP, rIDP;
  float rk, rOppk;
  float rEq;

  /* money game */

      /* Transform cubeless 0-ply equity to cubeful 0-ply equity using
	 Rick Janowski's formulas [insert ref here]. */

      /* First calculate average win and loss W and L: */

  if ( arOutput[ 0 ] > 0.0 )
    rW = 1.0 + ( arOutput[ 1 ] + arOutput[ 2 ] ) / arOutput[ 0 ];
  else
    rW = 1.0;

  if ( arOutput[ 0 ] < 1.0 )
    rL = 1.0 + ( arOutput[ 3 ] + arOutput[ 4 ] ) /
      ( 1.0 - arOutput [ 0 ] );
  else
    rL = 1.0;

      /* Calculate too good and cash points for opponent and
	 myself. These points are used for doing linear
	 interpolations. */

  rOppTG = 1.0 - ( rW + 1.0 ) / ( rW + rL + 0.5 * rCubeX );
  rOppCP = 1.0 -
    ( rW + 0.5 + 0.5 * rCubeX ) /
    ( rW + rL + 0.5 * rCubeX );
  rCP =
    ( rL + 0.5 + 0.5 * rCubeX ) /
    ( rW + rL + 0.5 * rCubeX );
  rTG = ( rL + 1.0 ) / ( rW + rL + 0.5 * rCubeX );
      
  if ( pci->fCubeOwner == -1 ) {

    /* Centered cube.  */

    if ( arOutput[ 0 ] <= rOppTG ) {

      /* Opponent is too good to double.
         Linear interpolation between: 
         p=OppTG, E = -1
         p=0, E = -L
      */

      if ( fJacoby )
        rEq = -1.0; /* gammons don't count: rL = 1 */
      else
        rEq = -rL + ( rL - 1.0 ) * arOutput[ 0 ] / rOppTG;

    } else if ( arOutput[0 ] > rOppTG && arOutput[ 0 ] < rOppCP ) {
      /* Opp cashes.  */
      rEq = -1.0;
    } else if ( arOutput[ 0 ] > rOppCP && arOutput[ 0 ] < rCP ) {
      /* In market window.
	 We do linear interpolation, but the initial double points
	 are dependent on Jacoby rule and beavers. */
          
      if ( ! fJacoby ) {

	/* Constants used in formulae for initial double points are
	   1 for money game without Jacoby rule. */
            
	rOppk = 1.0;
	rk = 1.0;
            
      }
      else {
            
	/* Constants used in formulae for initial double points
	   with playing with the Jacoby rule. */
            
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

      /* Initial double points: */

      rOppIDP = 1.0 - rOppk *
	( rW +
	  rCubeX / 2.0 * ( 3.0 - rCubeX )  / ( 2.0 - rCubeX ) )
	/ ( rL +  rW + 0.5 * rCubeX );

      rIDP = rk *
	( rL +
	  rCubeX / 2.0 * ( 3.0 - rCubeX )  / ( 2.0 - rCubeX ) )
	/ ( rL +  rW + 0.5 * rCubeX );

      if ( arOutput[ 0 ] < rOppIDP ) {

	/* Winning chance is
	   rOppCP < p < rOppIDP
	   Do linear interpolation between

	   p = rOppCP, E = -1, and 
	   p = rOppIDP, E = rE2 (given below) */

	float rE2 =
	  2.0 * ( rOppIDP * ( rW + rL + 0.5 * rCubeX ) - rL );

	rEq = -1.0 +  ( rE2 + 1.0 ) *
	  ( arOutput[ 0 ] - rOppCP ) / ( rOppIDP - rOppCP );

      }
      else if ( arOutput[ 0 ] < rIDP ) {

	/* Winning chance is
	   rOppIDP < p < rIDP

	   Do linear interpolation between

	   p = rOppIDP, E = rE2, and
	   p = rIDP, E = rE3 */

	float rE2 =
	  2.0 * ( rOppIDP * ( rW + rL + 0.5 * rCubeX ) - rL );
	float rE3 =
	  2.0 * ( rIDP * ( rW + rL + 0.5 * rCubeX ) -
		  rL - 0.5 * rCubeX );

	rEq = rE2 + ( rE3 - rE2 ) *
	  ( arOutput[ 0 ] - rOppIDP ) / ( rIDP - rOppIDP );

      }
      else {

	/* Winning chance is
	   rIDP < p < rCP

	   Do linear interpolation between

	   p = rIDP, E = rE3, and
	   p = CP, E = +1 */

	float rE3 =
	  2.0 * ( rIDP * ( rW + rL + 0.5 * rCubeX ) -
		  rL - 0.5 * rCubeX );

	rEq = rE3 + ( 1.0 - rE3 ) *
	  ( arOutput[ 0 ] - rIDP ) / ( rCP - rIDP );

      }

    } else if ( arOutput[ 0 ] > rCP && arOutput[ 0 ] < rTG ) {
      /* We cash one point */
      rEq = 1.0;
    } else {

      /* we are too good to double */

      if ( fJacoby )
        rEq = 1.0; /* gammons don't count: rW = 1 */
      else
        rEq = rW + 
          ( rW - 1.0 ) * ( arOutput[ 0 ] - 1.0 ) / ( rTG - 1.0 ) ;
    }

  }
  else if ( pci->fCubeOwner == pci->fMove ) {

    /* I own cube */

    if ( arOutput[ 0 ] < rTG ) {
	  
      /* Use formula (5) in Rick Janowski's article. 
	 Linear interpolation between
	  
	 p = 0, E = -L, and
	 p = rTG, E = +1 */

      rEq =
	( arOutput[ 0 ] * ( rW + rL + 0.5 * rCubeX ) - rL );
    }
    else {

      /* Linear interpolation between

         p = rTG, E = +1, and
         p = 1, E = +W */

      rEq = 1.0 + ( rW - 1.0 ) * ( arOutput[ 0 ] - rTG ) / ( 1.0 - rTG );

    }

  } else {

    /* Opp own Cube */

    if ( arOutput[ 0 ] > rOppTG ) {
	  
      /* Use formula (6) in Rick Janowski's article. 
	     Linear interpolation between
	  
	     p = rOppTG, E = -1, and
	     p = 1, E = +W */

      rEq = ( arOutput[ 0 ] * ( rW + rL + 0.5 * rCubeX )
	      - rL - 0.5 * rCubeX );
    }
    else {

      /* Use linear interpolation between

         p = 0, E = -L, and
         p = rOppTG, E = -1 */

      rEq = -rL + ( rL - 1.0 ) * arOutput[ 0 ] / rOppTG;

    }

  }

  return rEq;

}  


extern float
Cl2CfMatch ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci ) {

  if ( pci->fCubeOwner == -1 ) {
    return Cl2CfMatchCentered ( arOutput, pci );
  } 
  else if ( pci->fCubeOwner == pci->fMove ) {

    return Cl2CfMatchOwned ( arOutput, pci );

  } 
  else {

    return Cl2CfMatchUnavailable ( arOutput, pci );

  }

}
  

extern float
Cl2CfMatchOwned ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci ) {

  /* normalized score */

  int nScore0 = nMatchTo - anScore[ pci->fMove ];
  int nScore1 = nMatchTo - anScore[ ! pci->fMove ];

  float rG0, rBG0, rG1, rBG1;
  float arCP[ 2 ];

  float rMWCDead, rMWCLive, rMWCWin, rMWCLose;
  float rMWCCash, rTG;

  /* I own cube */

  /* Calculate normal, gammon, and backgammon ratios */

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

  /* MWC(dead cube) = cubeless equity */

  rMWCDead = eq2mwc ( Utility ( arOutput, pci ), pci );

  /* Get live cube cash points */

  GetPoints ( arOutput, anScore, nMatchTo, pci, arCP );

  rMWCCash = GET_MET ( nScore0 - pci->nCube - 1, nScore1 - 1, aafMET );

  rTG = arCP[ pci->fMove ];

  if ( arOutput[ 0 ] <= rTG ) {
    
    /* MWC(live cube) linear interpolation between the
       points:

       p = 0, MWC = I lose (normal, gammon, or backgammon)
       p = TG, MWC = I win 1 point
	 
    */

    rMWCLose = 
      ( 1.0 - rG1 - rBG1 ) * 
      GET_MET ( nScore0 - 1, nScore1 - pci->nCube - 1, aafMET ) +
      rG1 * GET_MET ( nScore0 - 1, nScore1 - 2 * pci->nCube - 1, aafMET ) +
      rBG1 * GET_MET ( nScore0 - 1, nScore1 - 3 * pci->nCube - 1, aafMET );

    if ( rTG > 0.0 )
      rMWCLive = rMWCLose + 
        ( rMWCCash - rMWCLose ) * arOutput[ 0 ] / rTG;
    else
      rMWCLive = rMWCLose;

    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  } 
  else {

    /* we are too good to double */

    /* MWC(live cube) linear interpolation between the
       points:

       p = TG, MWC = I win 1 point
       p = 1, MWC = I win (normal, gammon, or backgammon)
	 
    */

    rMWCWin = 
      ( 1.0 - rG0 - rBG0 ) * 
      GET_MET ( nScore0 - pci->nCube - 1, nScore1 - 1, aafMET ) +
      rG0 * GET_MET ( nScore0 - 2 * pci->nCube - 1, nScore1 - 1, aafMET ) +
      rBG0 * GET_MET ( nScore0 - 3 * pci->nCube - 1, nScore1 - 1, aafMET );
    

    if ( rTG < 1.0 )
      rMWCLive = rMWCCash + 
        ( rMWCWin - rMWCCash ) * ( arOutput[ 0 ] - rTG ) / ( 1.0 - rTG );
    else
      rMWCLive = rMWCWin;
      
    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  }

}


extern float
Cl2CfMatchUnavailable ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci ) {

  /* normalized score */

  int nScore0 = nMatchTo - anScore[ pci->fMove ];
  int nScore1 = nMatchTo - anScore[ ! pci->fMove ];

  float rG0, rBG0, rG1, rBG1;
  float arCP[ 2 ];

  float rMWCDead, rMWCLive, rMWCWin, rMWCLose;
  float rMWCOppCash, rOppTG;

  /* I own cube */

  /* Calculate normal, gammon, and backgammon ratios */

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

  /* MWC(dead cube) = cubeless equity */

  rMWCDead = eq2mwc ( Utility ( arOutput, pci ), pci );

  /* Get live cube cash points */

  GetPoints ( arOutput, anScore, nMatchTo, pci, arCP );

  rMWCOppCash = GET_MET ( nScore0 - 1, nScore1 - pci -> nCube - 1, aafMET );

  rOppTG = 1.0 - arCP[ ! pci->fMove ];

  if ( arOutput[ 0 ] <= rOppTG ) {

    /* Opponent is too good to double.

       MWC(live cube) linear interpolation between the
       points:

       p = 0, MWC = opp win normal, gammon, backgammon
       p = OppTG, MWC = opp cashes
	 
    */

    rMWCLose = 
      ( 1.0 - rG1 - rBG1 ) * 
      GET_MET ( nScore0 - 1, nScore1 - pci->nCube - 1, aafMET ) +
      rG1 * GET_MET ( nScore0 - 1, nScore1 - 2 * pci->nCube - 1, aafMET ) +
      rBG1 * GET_MET ( nScore0 - 1, nScore1 - 3 * pci->nCube - 1, aafMET );

    if ( rOppTG > 0.0 ) 
      /* avoid division by zero */
      rMWCLive = rMWCLose + 
        ( rMWCOppCash - rMWCLose ) * arOutput[ 0 ] / rOppTG;
    else
      rMWCLive = rMWCLose;
      
    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  }
  else {

    /* MWC(live cube) linear interpolation between the
       points:

       p = OppTG, MWC = opponent cashes
       p = 1, MWC = I win (normal, gammon, or backgammon)
	 
    */

    rMWCWin = 
      ( 1.0 - rG0 - rBG0 ) * 
      GET_MET ( nScore0 - pci->nCube - 1, nScore1 - 1, aafMET ) +
      rG0 * GET_MET ( nScore0 - 2 * pci->nCube - 1, nScore1 - 1, aafMET ) +
      rBG0 * GET_MET ( nScore0 - 3 * pci->nCube - 1, nScore1 - 1, aafMET );

    if ( arOutput[ 0 ] != rOppTG )
      rMWCLive = rMWCOppCash + 
        ( rMWCWin - rMWCOppCash ) * ( arOutput[ 0 ] - rOppTG ) 
        / ( 1.0 - rOppTG );
    else
      rMWCLive = rMWCWin;

    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  } 

}


extern float
Cl2CfMatchCentered ( float arOutput [ NUM_OUTPUTS ], cubeinfo *pci ) {

  /* normalized score */

  int nScore0 = nMatchTo - anScore[ pci->fMove ];
  int nScore1 = nMatchTo - anScore[ ! pci->fMove ];

  float rG0, rBG0, rG1, rBG1;
  float arCP[ 2 ];

  float rMWCDead, rMWCLive, rMWCWin, rMWCLose;
  float rMWCOppCash, rMWCCash, rOppTG, rTG;

  /* Centered cube */

  /* Calculate normal, gammon, and backgammon ratios */

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

  /* MWC(dead cube) = cubeless equity */

  rMWCDead = eq2mwc ( Utility ( arOutput, pci ), pci );

  /* Get live cube cash points */

  GetPoints ( arOutput, anScore, nMatchTo, pci, arCP );

  rMWCOppCash = GET_MET ( nScore0 - 1, nScore1 - pci -> nCube - 1, aafMET );
  rMWCCash = GET_MET ( nScore0 - pci -> nCube - 1, nScore1 - 1, aafMET );

  rOppTG = 1.0 - arCP[ ! pci->fMove ];
  rTG = arCP[ pci->fMove ];

  if ( arOutput[ 0 ] <= rOppTG ) {

    /* Opp too good to double */

    rMWCLose = 
      ( 1.0 - rG1 - rBG1 ) * 
      GET_MET ( nScore0 - 1, nScore1 - pci->nCube - 1, aafMET ) +
      rG1 * GET_MET ( nScore0 - 1, nScore1 - 2 * pci->nCube - 1, aafMET ) +
      rBG1 * GET_MET ( nScore0 - 1, nScore1 - 3 * pci->nCube - 1, aafMET );

    if ( rOppTG > 0.0 ) 
      /* avoid division by zero */
      rMWCLive = rMWCLose + 
        ( rMWCOppCash - rMWCLose ) * arOutput[ 0 ] / rOppTG;
    else
      rMWCLive = rMWCLose;
      
    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  }
  else if ( rOppTG < arOutput[ 0 ] && arOutput[ 0 ] <= rTG ) {

    /* In double window */

    rMWCLive = 
      rMWCOppCash + 
      (rMWCCash - rMWCOppCash) * ( arOutput[ 0 ] - rOppTG ) / ( rTG - rOppTG); 
    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  } else {

    /* I'm too good to double */

    rMWCWin = 
      ( 1.0 - rG0 - rBG0 ) * 
      GET_MET ( nScore0 - pci->nCube - 1, nScore1 - 1, aafMET ) +
      rG0 * GET_MET ( nScore0 - 2 * pci->nCube - 1, nScore1 - 1, aafMET ) +
      rBG0 * GET_MET ( nScore0 - 3 * pci->nCube - 1, nScore1 - 1, aafMET );

    if ( arOutput[ 0 ] == rOppTG )
      rMWCLive = rMWCOppCash + 
        ( rMWCWin - rMWCOppCash ) * ( arOutput[ 0 ] - rOppTG ) 
        / ( 1.0 - rOppTG );
    else
      rMWCLive = rMWCWin;

    /* (1-x) MWC(dead) + x MWC(live) */

    return  rMWCDead * ( 1.0 - rCubeX ) + rMWCLive * rCubeX;

  } 

}


#define RACEFACTOR 0.00125
#define RACECOEFF 0.55
#define RACE_MAX_EFF 0.7
#define RACE_MIN_EFF 0.6

extern float
EvalEfficiency( int anBoard[2][25], positionclass pc ){

  /* Since it's somewhat costly to call CalcInputs, the 
     inputs should preferebly be cached to same time. */

  if (pc == CLASS_OVER)
    return -1.0;

  if (pc == CLASS_BEAROFF1)

    /* FIXME: calculate based on #rolls to get off.
       For example, 15 rolls probably have cube eff. of
       0.7, and 1.25 rolls have cube eff. of 1.0.

       It's not so important to have cube eff. correct here as an
       n-ply evaluation will take care of last-roll and 2nd-last-roll
       situations. */

    return 0.60;

  if (pc == CLASS_BEAROFF2)

    /* FIXME: use linear interpolation based on pip count */

    return 0.60;

  if (pc == CLASS_RACE)
    {
      int anPips[2];

      float rEff;

      PipCount(anBoard, anPips);

      rEff = anPips[1]*RACEFACTOR + RACECOEFF;
      if (rEff > RACE_MAX_EFF)
        return RACE_MAX_EFF;
      else
       {
        if (rEff < RACE_MIN_EFF)
          return RACE_MIN_EFF;
        else
          return rEff;
       }
     }
  if (pc == CLASS_CONTACT)
    {
      
      /* FIXME: use Øystein's values published in rec.games.backgammon,
         or work some other semiempirical values */

      /* FIXME: very important: use opponents inputs as well */

	float arInput[ NUM_INPUTS ] /* , arOutput[ NUM_OUTPUTS ] */;

      return 0.68;

      CalculateInputs( anBoard, arInput );

      if ( arInput [ I_BREAK_CONTACT ] < 0.05 ) {
        /* less than 8 pips to break contact, i.e. it must
           be close to a race */
        return 0.65;
      } else if ( arInput[ I_FORWARD_ANCHOR ] >= 0.5 &&
                  arInput[ I_FORWARD_ANCHOR ] <= 1.5 ) {
        /* I have a forward anchor, i.e. it's a holding game */
        return 0.5;
      } else {
        
        int side, i, n;

        for(side = 0; side < 2; ++side) {
          n = 0;
          for(i = 18; i < 24; ++i) {
            if( anBoard[side][i] > 1 ) {
              ++n;
            }
          }
        }

        if ( n >= 2 ) {
          /* backgame */
          return 0.4;
        }

        /* FIXME: if attack position */

        /* else, ... use `typical' value of: */

        return 0.68;

      }
    
    }

  assert( FALSE );
  return 0;
}


extern char *FormatEval ( char *sz, evaltype et, evalsetup es ) {

  switch ( et ) {
  case EVAL_NONE:
    strcpy ( sz, "" );
    break;
  case EVAL_EVAL:
    sprintf ( sz, "%s %1i-ply", 
              es.ec.fCubeful ? "Cubeful" : "Cubeless",
              es.ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    sprintf ( sz, "%s", "Rollout" );
    break;
  default:
    printf ("et: %i\n", et );
    assert (FALSE);
    break;
  }

  return sz;

}

