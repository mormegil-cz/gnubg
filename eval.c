/*
 * eval.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
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
#include <cache.h>
#include <errno.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <limits.h>
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

typedef void ( *classevalfunc )( int anBoard[ 2 ][ 25 ], float arOutput[] );
typedef void ( *classdumpfunc )( int anBoard[ 2 ][ 25 ], char *szOutput );

/* Race and contact inputs */
enum { I_OFF1 = 100, I_OFF2, I_OFF3, HALF_RACE_INPUTS };

/* Contact inputs -- see Berliner for most of these */
enum { I_BREAK_CONTACT = HALF_RACE_INPUTS, I_BACK_CHEQUER, I_BACK_ANCHOR,
       I_FORWARD_ANCHOR, I_PIPLOSS, I_P1, I_P2, I_BACKESCAPES, I_ACONTAIN,
       I_ACONTAIN2, I_CONTAIN, I_CONTAIN2, I_BUILDERS, I_SLOTTED,
       I_MOBILITY, I_MOMENT2, I_ENTER, I_ENTER2, I_TOP_EVEN, I_TOP2_EVEN,
       HALF_INPUTS };

#define NUM_INPUTS ( HALF_INPUTS * 2 )
#define NUM_RACE_INPUTS ( HALF_RACE_INPUTS * 2 )

#define SEARCH_CANDIDATES 8
#define SEARCH_TOLERANCE 0.16

static int anEscapes[ 0x1000 ];
static neuralnet nnContact, nnRace;
static unsigned char *pBearoff1 = NULL, *pBearoff2;
static cache cEval;
volatile int fInterrupt = FALSE;
static float arGammonPrice[ 4 ] = { 1.0, 1.0, 1.0, 1.0 };

typedef struct _evalcache {
    unsigned char auchKey[ 10 ];
    int nPlies;
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

    return !EqualKeys( p0->auchKey, p1->auchKey ) || p0->nPlies != p1->nPlies;
}

static long EvalCacheHash( evalcache *pec ) {

    long l = 0;
    int i;
    
    l = pec->nPlies << 9;

    for( i = 0; i < 10; i++ )
	l = ( ( l << 8 ) % 8388593 ) ^ pec->auchKey[ i ];

    return l;    
}

extern int EvalInitialise( char *szWeights, char *szDatabase ) {

    FILE *pfWeights;
    int h;
    char szFileVersion[ 16 ];
    char szPath[ PATH_MAX ];
    
    /* FIXME allow starting without bearoff database (to generate it later!) */

    if( !pBearoff1 ) {
	/* not yet initialised */
	if( ( h = open( szDatabase, O_RDONLY ) ) < 0 ) {
	    sprintf( szPath, PKGDATADIR "/%s", szDatabase );
	    if( ( h = open( szPath, O_RDONLY ) ) < 0 ) {
		perror( szDatabase );
		return -1;
	    }
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

	if( CacheCreate( &cEval, 8192, (cachecomparefunc) EvalCacheCompare ) )
	    return -1;
	    
	ComputeTable();
    }
    
    if( szWeights ) {
	if( !( pfWeights = fopen( szWeights, "r" ) ) ) {
	    sprintf( szPath, PKGDATADIR "/%s", szWeights );
	    if( !( pfWeights = fopen( szPath, "r" ) ) ) {
		perror( szWeights );
		return -1;
	    }
	}
    
	if( fscanf( pfWeights, "GNU Backgammon %15s\n", szFileVersion ) != 1 ||
	    strcmp( szFileVersion, WEIGHTS_VERSION ) ) {
	    fprintf( stderr, "%s: invalid weights file\n", szWeights );
	    return EINVAL;
	}
    
	NeuralNetLoad( &nnContact, pfWeights );
	NeuralNetLoad( &nnRace, pfWeights );

	if( nnContact.cInput != NUM_INPUTS ||
	    nnContact.cOutput != NUM_OUTPUTS )
	    NeuralNetResize( &nnContact, NUM_INPUTS, nnContact.cHidden,
			     NUM_OUTPUTS );

	if( nnRace.cInput != NUM_RACE_INPUTS || nnRace.cOutput != NUM_OUTPUTS )
	    NeuralNetResize( &nnRace, NUM_RACE_INPUTS, nnRace.cHidden,
			     NUM_OUTPUTS );
		
	fclose( pfWeights );
    } else {
	NeuralNetCreate( &nnContact, NUM_INPUTS, 128 /* FIXME */,
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
static int CalculateHalfInputs( int anBoard[ 25 ], int anBoardOpp[ 25 ],
			  float afInput[] ) {

    int i, j, k, l, nOppBack, n, nBack, aHit[ 39 ], nBoard;
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
    
    static struct {
	int fAll; /* if true, all intermediate points (if any) are required;
		     if false, one of two intermediate points are required */
	int anIntermediate[ 3 ]; /* intermediate points required */
	int nFaces, nPips;
    } *pi, aIntermediate[ 39 ] = {
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

    static int anPoint[ 6 ] = { 5, 4, 3, 6, 2, 7 };
    
    struct {
	int nPips; /* count of pips this roll hits */
	int nChequers; /* number of chequers this roll hits */
    } aRoll[ 21 ];
    
    /* FIXME consider -1.0 instead of 0.0, since null inputs do not
       contribute to learning! */
    
    /* Points */
    for( i = 0; i < 24; i++ ) {
	afInput[ i * 4 + 0 ] = anBoard[ i ] == 1;
	afInput[ i * 4 + 1 ] = anBoard[ i ] >= 2;
	afInput[ i * 4 + 2 ] = anBoard[ i ] >= 3;
	afInput[ i * 4 + 3 ] = anBoard[ i ] > 3 ?
	    ( anBoard[ i ] - 3 ) / 2.0 : 0.0;
    }

    /* Bar */
    afInput[ 24 * 4 + 0 ] = anBoard[ 24 ] >= 1;
    afInput[ 24 * 4 + 1 ] = anBoard[ 24 ] >= 2;
    afInput[ 24 * 4 + 2 ] = anBoard[ 24 ] >= 3;
    afInput[ 24 * 4 + 3 ] = anBoard[ 24 ] > 3 ?
	( anBoard[ 24 ] - 3 ) / 2.0 : 0.0;

    /* Men off */
    for( n = 15, i = 0; i < 25; i++ )
	n -= anBoard[ i ];

    if( n > 10 ) {
	afInput[ I_OFF1 ] = 1.0;
	afInput[ I_OFF2 ] = 1.0;
	afInput[ I_OFF3 ] = ( n - 10 ) / 5.0;
    } else if( n > 5 ) {
	afInput[ I_OFF1 ] = 1.0;
	afInput[ I_OFF2 ] = ( n - 5 ) / 5.0;
	afInput[ I_OFF3 ] = 0.0;
    } else {
	afInput[ I_OFF1 ] = n / 5.0;
	afInput[ I_OFF2 ] = 0.0;
	afInput[ I_OFF3 ] = 0.0;
    }
    
    /* Degree of contact */
    for( nOppBack = 0, i = 0; i < 25; i++ )
	if( anBoardOpp[ i ] )
	    nOppBack = i;

    nOppBack = 23 - nOppBack;

    for( n = 0, i = nOppBack + 1; i < 25; i++ )
	if( anBoard[ i ] )
	    n += ( i - nOppBack ) * anBoard[ i ];

    if( !n )
	/* No contact */
	return 1;
    
    afInput[ I_BREAK_CONTACT ] = n / 152.0;

    /* Back chequer */
    for( nBack = 0, i = 0; i < 25; i++ )
	if( anBoard[ i ] )
	    nBack = i;

    afInput[ I_BACK_CHEQUER ] = nBack / 24.0;

    afInput[ I_TOP_EVEN ] = ( anBoard[ nBack ] & 1 ) ? 0.0 : 1.0;

    for( i = nBack - 1; i >= 0; i-- )
	if( anBoard[ i ] ) {
	    afInput[ I_TOP2_EVEN ] = ( ( anBoard[ nBack ] +
					 anBoard[ i ] ) & 1 ) ? 0.0 : 1.0;
	    break;
	}

    if( i < 0 )
	afInput[ I_TOP2_EVEN ] = afInput[ I_TOP_EVEN ];
    
    /* Back anchor */
    for( n = 0, i = 0; i < 24; i++ )
	if( anBoard[ i ] >= 2 )
	    n = i;

    afInput[ I_BACK_ANCHOR ] = n / 24.0;

    /* Forward anchor */
    for( n = 0, i = 23; i >= 18; i-- )
	if( anBoard[ i ] >= 2 )
	    n = 24 - i;

    afInput[ I_FORWARD_ANCHOR ] = n / 6.0;

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
			    /* all the intermediate points are required */
			    for( k = 0; k < 3; k++ )
				if( anBoardOpp[ i - pi->anIntermediate[ k ] ] >
				    1 )
				    /* point is blocked; look for other hits */
				    goto cannot_hit;
			} else
			    /* either of two points are required */
			    if( anBoardOpp[ i - pi->anIntermediate[ 0 ] ] > 1
				&& anBoardOpp[ i - pi->anIntermediate[ 1 ] ] >
				1 )
				/* both are blocked; look for other hits */
				goto cannot_hit;
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
		if( aaRoll[ i ][ j ] < 0 )
		    break;

		if( !aHit[ aaRoll[ i ][ j ] ] )
		    continue;

		pi = aIntermediate + aaRoll[ i ][ j ];
		
		if( pi->nFaces == 1 ) {
		    /* direct shot */
		    for( k = 23; k > 0; k-- )
			if( aHit[ aaRoll[ i ][ j ] ] & ( 1 << k ) ) {
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
				aHit[ aaRoll[ i ][ j ] ] & ~( 1 << k ) )
				aRoll[ i ].nChequers++;
			    
			    break;
			}
		} else {
		    /* indirect shot */
		    if( !aRoll[ i ].nChequers )
			aRoll[ i ].nChequers = 1;

		    /* find the most advanced hitter */
		    for( k = 23; k >= 0; k-- )
			if( aHit[ aaRoll[ i ][ j ] ] & ( 1 << k ) )
			    break;

		    if( k - pi->nPips + 1 > aRoll[ i ].nPips )
			aRoll[ i ].nPips = k - pi->nPips + 1;

		    /* check for blots hit on intermediate points */
		    for( l = 0; l < 3 && pi->anIntermediate[ l ] >= 0; l++ )
			if( anBoardOpp[ 23 - k + pi->anIntermediate[ l ] ] ==
			    1 ) {
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
		if( aaRoll[ i ][ j ] < 0 )
		    break;
		
		if( !aHit[ aaRoll[ i ][ j ] ] )
		    continue;

		pi = aIntermediate + aaRoll[ i ][ j ];
		
		if( pi->nFaces == 1 ) {
		    /* direct shot */
		    for( k = 24; k > 0; k-- )
			if( aHit[ aaRoll[ i ][ j ] ] & ( 1 << k ) ) {
			    /* if we need this die to enter, we can't
			       hit elsewhere */
			    if( n && k != 24 )
				break;
			    
			    /* if this isn't a shot from the bar, the
			       other die must be used to enter */
			    if( k != 24 ) {
				if( anBoardOpp[ aIntermediate[
				    aaRoll[ i ][ 1 - j ] ].nPips - 1 ] > 1 )
				    break;
				
				n = 1;
			    }

			    aRoll[ i ].nChequers++;

			    if( k - pi->nPips + 1 > aRoll[ i ].nPips )
				aRoll[ i ].nPips = k - pi->nPips + 1;
			}
		} else {
		    /* indirect shot -- consider from the bar only */
		    if( !( aHit[ aaRoll[ i ][ j ] ] & ( 1 << 24 ) ) )
			continue;
		    
		    if( !aRoll[ i ].nChequers )
			aRoll[ i ].nChequers = 1;
		    
		    if( 25 - pi->nPips > aRoll[ i ].nPips )
			aRoll[ i ].nPips = 25 - pi->nPips;
		    
		    /* check for blots hit on intermediate points */
		    for( k = 0; k < 3 && pi->anIntermediate[ k ] >= 0; k++ )
			if( anBoardOpp[ pi->anIntermediate[ k ] + 1 ] == 1 ) {
			    aRoll[ i ].nChequers++;
			    break;
			}
		}
	    }
	}
    } else {
	/* we have more than one on the bar -- count only direct shots from
	 point 24 */
	for( i = 0; i < 21; i++ )
	    /* for the first two ways that hit from the bar */
	    for( j = 0; j < 2; j++ ) {
		if( !( aHit[ aaRoll[ i ][ j ] ] & ( 1 << 24 ) ) )
		    continue;

		pi = aIntermediate + aaRoll[ i ][ j ];

		/* only consider direct shots */
		if( pi->nFaces != 1 )
		    continue;

		aRoll[ i ].nChequers++;

		if( 25 - pi->nPips > aRoll[ i ].nPips )
		    aRoll[ i ].nPips = 25 - pi->nPips;
	    }
    }

    for( n = 0, i = 0; i < 21; i++ )
	n += aRoll[ i ].nPips * ( aaRoll[ i ][ 3 ] > 0 ? 1 : 2 );

    afInput[ I_PIPLOSS ] = n / ( 12.0 * 36.0 );

    for( n = 0, i = 0; i < 21; i++ )
	if( aRoll[ i ].nChequers )
	    n += aaRoll[ i ][ 3 ] > 0 ? 1 : 2;

    afInput[ I_P1 ] = n / 36.0;
    
    for( n = 0, i = 0; i < 21; i++ )
	if( aRoll[ i ].nChequers > 1 )
	    n += aaRoll[ i ][ 3 ] > 0 ? 1 : 2;

    afInput[ I_P2 ] = n / 36.0;

    afInput[ I_BACKESCAPES ] = Escapes( anBoard, 23 - nOppBack ) / 36.0;

    for( n = 36, i = 15; i < 24 - nOppBack; i++ )
	if( ( j = Escapes( anBoard, i ) ) < n )
	    n = j;

    afInput[ I_ACONTAIN ] = ( 36 - n ) / 36.0;
    afInput[ I_ACONTAIN2 ] = afInput[ I_ACONTAIN ] * afInput[ I_ACONTAIN ];

    for( n = 36, i = 15; i < 24; i++ )
	if( ( j = Escapes( anBoard, i ) ) < n )
	    n = j;

    afInput[ I_CONTAIN ] = ( 36 - n ) / 36.0;
    afInput[ I_CONTAIN2 ] = afInput[ I_CONTAIN ] * afInput[ I_CONTAIN ];
    
    for( n = 0, i = 0; i < 6; i++ )
	if( anBoard[ j = anPoint[ i ] ] < 2 && anBoardOpp[ 23 - j ] < 2 ) {
	    /* we want to make point j */
	    for( k = 1; k < 7; k++ )
		if( anBoard[ j + k ] == 1 || anBoard[ j + k ] > 2 ||
		    ( j + k > 7 && anBoard[ j + k ] == 2 ) )
		    n++;
	    
	    break;
	}

    afInput[ I_BUILDERS ] = n / 6.0;

    afInput[ I_SLOTTED ] = ( i < 6 && anBoard[ j ] == 1 ) ?
	( 6 - i ) / 6.0 : 0.0;
    
    for( n = 0, i = 6; i < 25; i++ )
	if( anBoard[ i ] )
	    n += ( i - 5 ) * anBoard[ i ] * Escapes( anBoardOpp, i );

    afInput[ I_MOBILITY ] = n / 3600.00;

    for( n = 0, i = 0; i < 25; i++ )
	if( anBoard[ i ] )
	    n += i * anBoard[ i ];

    n /= 15;

    for( j = 0, k = 0, i = n + 1; i < 25; i++ )
	if( anBoard[ i ] ) {
	    j += anBoard[ i ];
	    k += anBoard[ i ] * ( i - n ) * ( i - n );
	}

    if( j )
	k /= j;

    afInput[ I_MOMENT2 ] = k / 400.0;

    for( n = 0, i = 0; i < 6; i++ )
	n += anBoardOpp[ i ] > 1;

    afInput[ I_ENTER ] = ( anBoard[ 24 ] * n ) / 12.0;

    afInput[ I_ENTER2 ] = ( 36 - ( n - 6 ) * ( n - 6 ) ) / 36.0;

    return 0;
}

/* Calculates neural net inputs from the board position.  Returns 0 for contact
   positions, 1 for races. */
static int CalculateInputs( int anBoard[ 2 ][ 25 ], float arInput[] ) {

    float ar[ HALF_INPUTS ];
    int i;
    
    int fRace = CalculateHalfInputs( anBoard[ 1 ], anBoard[ 0 ], ar );

    for( i = fRace ? HALF_RACE_INPUTS - 1 : HALF_INPUTS - 1; i >= 0; i-- )
	arInput[ i << 1 ] = ar[ i ];

    CalculateHalfInputs( anBoard[ 0 ], anBoard[ 1 ], ar );
    
    for( i = fRace ? HALF_RACE_INPUTS - 1 : HALF_INPUTS - 1; i >= 0; i-- )
	arInput[ ( i << 1 ) | 1 ] = ar[ i ];

    return fRace;
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
}

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

    if( nBack + nOppBack > 22 )
	return CLASS_CONTACT;
    else if( nBack > 5 || nOppBack > 5 )
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

static classevalfunc acef[ 5 ] = {
    EvalOver, EvalBearoff2, EvalBearoff1, EvalRace, EvalContact
};

static int EvaluatePositionFull( int anBoard[ 2 ][ 25 ], float arOutput[],
				 int nPlies ) {
    int i, n0, n1;
    positionclass pc;

    if( ( pc = ClassifyPosition( anBoard ) ) > CLASS_PERFECT &&
	nPlies > 0 ) {
	/* internal node; recurse */

	float ar[ NUM_OUTPUTS ];
	int anBoardNew[ 2 ][ 25 ];
	int anMove[ 8 ];

	for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] = 0.0;
	
	for( n0 = 1; n0 <= 6; n0++ )
	    for( n1 = 1; n1 <= n0; n1++ ) {
		for( i = 0; i < 25; i++ ) {
		    anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
		    anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
		}

		if( fInterrupt ) {
		    errno = EINTR;
		    return -1;
		}
	    
		FindBestMove( 0, anMove, n0, n1, anBoardNew );

		SwapSides( anBoardNew );

		if( EvaluatePosition( anBoardNew, ar, nPlies - 1 ) )
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
	/* at leaf node; use static evaluation */

	acef[ pc ]( anBoard, arOutput );

	SanityCheck( anBoard, arOutput );
    }

    return 0;
}

extern int EvaluatePosition( int anBoard[ 2 ][ 25 ], float arOutput[],
			     int nPlies ) {
    evalcache ec, *pec;
    long l;
	
    PositionKey( anBoard, ec.auchKey );
    ec.nPlies = nPlies;

    l = EvalCacheHash( &ec );
    
    if( ( pec = CacheLookup( &cEval, l, &ec ) ) ) {
	memcpy( arOutput, pec->ar, sizeof( pec->ar ) );

	return 0;
    }
    
    if( EvaluatePositionFull( anBoard, arOutput, nPlies ) )
	return -1;

    memcpy( ec.ar, arOutput, sizeof( ec.ar ) );

    return CacheAdd( &cEval, l, &ec, sizeof ec );
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
    int fRace;

    /* FIXME implement per-class training */
    if( ClassifyPosition( anBoard ) < CLASS_RACE ) {
	errno = EDOM;
	return -1;
    }
    
    SanityCheck( anBoard, arDesired );
    
    fRace = CalculateInputs( anBoard, arInput );

    NeuralNetTrain( fRace ? &nnRace : &nnContact, arInput, arOutput, arDesired,
		    2.0 / pow( 100.0 + ( fRace ? nnRace : nnContact ).nTrained,
			       0.25 ) );

    return 0;
}

extern void SetGammonPrice( float rGammon, float rLoseGammon,
			    float rBackgammon, float rLoseBackgammon ) {
    
    arGammonPrice[ 0 ] = rGammon;
    arGammonPrice[ 1 ] = rLoseGammon;
    arGammonPrice[ 2 ] = rBackgammon;
    arGammonPrice[ 3 ] = rLoseBackgammon;
}

extern float Utility( float ar[ NUM_OUTPUTS ] ) {

    return ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
	ar[ OUTPUT_WINGAMMON ] * arGammonPrice[ 0 ] -
	ar[ OUTPUT_LOSEGAMMON ] * arGammonPrice[ 1 ] +
	ar[ OUTPUT_WINBACKGAMMON ] * arGammonPrice[ 2 ] -
	ar[ OUTPUT_LOSEBACKGAMMON ] * arGammonPrice[ 3 ];
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
		       int anBoard[ 2 ][ 25 ] ) {
    int i, j;
    move *pm;
    unsigned char auch[ 10 ];

    if( cMoves < pml->cMaxMoves || cPip < pml->cMaxPips )
	return;

    if( cMoves > pml->cMaxMoves || cPip > pml->cMaxPips )
	pml->cMoves = 0;

    pm = pml->amMoves + pml->cMoves;
    
    pml->cMaxMoves = cMoves;
    pml->cMaxPips = cPip;
    
    PositionKey( anBoard, auch );
    
    for( i = 0; i < pml->cMoves; i++ )
	if( EqualKeys( auch, pml->amMoves[ i ].auch ) ) {
	    /* update moves, just in case cMoves or cPip has increased */
	    for( j = 0; j < cMoves * 2; j++ )
		pml->amMoves[ i ].anMove[ j ] = anMoves[ j ];
    
	    if( cMoves < 4 )
		pml->amMoves[ i ].anMove[ cMoves * 2 ] = -1;
    
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

    assert( pml->cMoves < 3060 );
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
			     int anMoves[] ) {
    int i, iCopy, fUsed = 0;
    int anBoardNew[ 2 ][ 25 ];

    if( nMoveDepth > 3 || !anRoll[ nMoveDepth ] )
	return -1;

    if( anBoard[ 1 ][ 24 ] ) { /* on bar */
	if( anBoard[ 0 ][ anRoll[ nMoveDepth ] - 1 ] >= 2 )
	    return -1;

	anMoves[ nMoveDepth * 2 ] = 24;
	anMoves[ nMoveDepth * 2 + 1 ] = 24 - anRoll[ nMoveDepth ];

	for( i = 0; i < 25; i++ ) {
	    anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	    anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}
	
	ApplySubMove( anBoardNew, 24, anRoll[ nMoveDepth ] );
	
	if( GenerateMovesSub( pml, anRoll, nMoveDepth + 1, 23, cPip +
			      anRoll[ nMoveDepth ], anBoardNew, anMoves ) < 0 )
	    SaveMoves( pml, nMoveDepth + 1, cPip + anRoll[ nMoveDepth ],
		       anMoves, anBoardNew );

	return 0;
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
				   anBoardNew, anMoves ) < 0 ) {
		    SaveMoves( pml, nMoveDepth + 1, cPip +
			       anRoll[ nMoveDepth ], anMoves, anBoardNew );
		}
		
		fUsed = 1;
	    }
    }

    return fUsed ? 0 : -1;
}

static int CompareMoves( const move *pm0, const move *pm1 ) {

    return pm1->rScore > pm0->rScore ? 1 : -1;
}

typedef int ( *cfunc )( const void *, const void * );

static int ScoreMoves( movelist *pml, int nPlies ) {

    int i, anBoardTemp[ 2 ][ 25 ];
    float arEval[ NUM_OUTPUTS ];
    
    pml->rBestScore = -99999.9;

    for( i = 0; i < pml->cMoves; i++ ) {
	PositionFromKey( anBoardTemp, pml->amMoves[ i ].auch );

	SwapSides( anBoardTemp );
	
	if( EvaluatePosition( anBoardTemp, arEval, nPlies ) )
	    return -1;
	
	InvertEvaluation( arEval );
	
	if( pml->amMoves[ i ].pEval )
	    memcpy( pml->amMoves[ i ].pEval, arEval, sizeof( arEval ) );
	
	if( ( pml->amMoves[ i ].rScore = Utility( arEval ) ) >
	    pml->rBestScore ) {
	    pml->iMoveBest = i;
	    pml->rBestScore = pml->amMoves[ i ].rScore;
	}
    }

    return 0;
}

extern int GenerateMoves( movelist *pml, int anBoard[ 2 ][ 25 ],
			  int n0, int n1 ) {
    int anRoll[ 4 ], anMoves[ 8 ];
    static move amMoves[ 3060 ];

    anRoll[ 0 ] = n0;
    anRoll[ 1 ] = n1;

    anRoll[ 2 ] = anRoll[ 3 ] = ( ( n0 == n1 ) ? n0 : 0 );

    pml->cMoves = pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
    pml->amMoves = amMoves; /* use static array for top-level search, since
			       it doesn't need to be re-entrant */
    
    GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves );

    if( anRoll[ 0 ] != anRoll[ 1 ] ) {
	swap( anRoll, anRoll + 1 );

	GenerateMovesSub( pml, anRoll, 0, 23, 0, anBoard, anMoves );
    }

    return pml->cMoves;
}

extern int FindBestMove( int nPlies, int anMove[ 8 ], int nDice0, int nDice1,
			 int anBoard[ 2 ][ 25 ] ) {

    int i, j, iPly;
    movelist ml;
    move amCandidates[ SEARCH_CANDIDATES ];

    /* FIXME return -1 and do not change anBoard on interrupt */
    
    if( anMove )
	for( i = 0; i < 8; i++ )
	    anMove[ i ] = -1;

    GenerateMoves( &ml, anBoard, nDice0, nDice1 );
    
    if( !ml.cMoves )
	/* no legal moves */
	return 0;
    else if( ml.cMoves == 1 )
	/* forced move */
	ml.iMoveBest = 0;
    else {
	/* choice of moves */
	if( ScoreMoves( &ml, 0 ) )
	    return -1;

	for( iPly = 0; iPly < nPlies; iPly++ ) {
	    for( i = 0, j = 0; i < ml.cMoves; i++ )
		if( ml.amMoves[ i ].rScore >= ml.rBestScore -
		    ( SEARCH_TOLERANCE / ( 1 << iPly ) ) ) {
		    if( i != j )
			ml.amMoves[ j ] = ml.amMoves[ i ];
		    
		    j++;
		}
	    
	    if( j == 1 )
		break;

	    qsort( ml.amMoves, j, sizeof( move ), (cfunc) CompareMoves );

	    ml.iMoveBest = 0;
	    
	    ml.cMoves = ( j < ( SEARCH_CANDIDATES >> iPly ) ? j :
			  ( SEARCH_CANDIDATES >> iPly ) );

	    if( ml.amMoves != amCandidates ) {
		memcpy( amCandidates, ml.amMoves, ml.cMoves * sizeof( move ) );
	    
		ml.amMoves = amCandidates;
	    }

	    if( ScoreMoves( &ml, iPly + 1 ) )
		return -1;
	}
    }

    if( anMove )
	for( i = 0; i < ml.cMaxMoves * 2; i++ )
	    anMove[ i ] = ml.amMoves[ ml.iMoveBest ].anMove[ i ];
	
    PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

    return ml.cMaxMoves * 2;
}

extern int FindBestMoves( movelist *pml, float ar[][ NUM_OUTPUTS ], int nPlies,
			  int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
			  int c, float d ) {
    int i, j;
    static move amCandidates[ 32 ];

    /* amCandidates is only 32 elements long, so we silently truncate any
       request for more moves to 32 */
    if( c > 32 )
	c = 32;

    /* Find all moves -- note that pml contains internal pointers to static
       data, so we can't call GenerateMoves again (or anything that calls
       it, such as ScoreMoves at more than 0 plies) until we have saved
       the moves we want to keep in amCandidates. */
    GenerateMoves( pml, anBoard, nDice0, nDice1 );

    if( ScoreMoves( pml, 0 ) )
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
    if( ScoreMoves( pml, nPlies ) )
	return -1;

    /* Using a deeper search might change the order of the evaluations;
       reorder them according to the latest evaluation */
    if( nPlies )
	qsort( pml->amMoves, pml->cMoves, sizeof( move ),
	       (cfunc) CompareMoves );

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

    /* FIXME */
}

static void DumpBearoff1( int anBoard[ 2 ][ 25 ], char *szOutput ) {

    /* FIXME */
}

static void DumpRace( int anBoard[ 2 ][ 25 ], char *szOutput ) {

    /* no-op -- nothing much we can say, really (pip count?) */
}

static void DumpContact( int anBoard[ 2 ][ 25 ], char *szOutput ) {

    float arInput[ HALF_INPUTS ];
    
    CalculateHalfInputs( anBoard[ 1 ], anBoard[ 0 ], arInput );
    
    sprintf( szOutput,
	     "OFF1           \t%5.3f\n"
	     "OFF2           \t%5.3f\n"
	     "OFF3           \t%5.3f\n"
	     "BREAK_CONTACT  \t%5.3f\n"
	     "BACK_CHEQUER   \t%5.3f\n"
	     "BACK_ANCHOR    \t%5.3f\n"
	     "FORWARD_ANCHOR \t%5.3f\n"
	     "PIPLOSS        \t%5.3f (%5.3f avg)\n"
	     "P1             \t%5.3f (%5.3f/36)\n"
	     "P2             \t%5.3f (%5.3f/36)\n"
	     "BACKESCAPES    \t%5.3f (%5.3f/36)\n"
	     "ACONTAIN       \t%5.3f (%5.3f/36)\n"
	     "CONTAIN        \t%5.3f (%5.3f/36)\n"
	     "BUILDERS       \t%5.3f (%5.3f/6)\n"
	     "SLOTTED        \t%5.3f (%5.3f/6)\n"
	     "MOBILITY       \t%5.3f\n"
	     "MOMENT2        \t%5.3f\n"
	     "ENTER          \t%5.3f (%5.3f/12)\n"
	     "TOP_EVEN       \t%5.3f\n"
	     "TOP2_EVEN      \t%5.3f\n",
	     arInput[ I_OFF1 ], arInput[ I_OFF2 ], arInput[ I_OFF3 ],
	     arInput[ I_BREAK_CONTACT ], arInput[ I_BACK_CHEQUER ],
	     arInput[ I_BACK_ANCHOR ], arInput[ I_FORWARD_ANCHOR ],
	     arInput[ I_PIPLOSS ],
	     arInput[ I_P1 ] ? arInput[ I_PIPLOSS ] / arInput[ I_P1 ] * 12.0 :
		0.0,
	     arInput[ I_P1 ], arInput[ I_P1 ] * 36.0,
	     arInput[ I_P2 ], arInput[ I_P2 ] * 36.0,
	     arInput[ I_BACKESCAPES ], arInput[ I_BACKESCAPES ] * 36.0,
	     arInput[ I_ACONTAIN ], arInput[ I_ACONTAIN ] * 36.0,
	     arInput[ I_CONTAIN ], arInput[ I_CONTAIN ] * 36.0,
	     arInput[ I_BUILDERS ], arInput[ I_BUILDERS ] * 6.0,
	     arInput[ I_SLOTTED ], arInput[ I_SLOTTED ] * 6.0,
	     arInput[ I_MOBILITY ], arInput[ I_MOMENT2 ],
	     arInput[ I_ENTER ], arInput[ I_ENTER ] * 12.0,
	     arInput[ I_TOP_EVEN ], arInput[ I_TOP2_EVEN ] );
}

static classdumpfunc acdf[ 5 ] = {
    DumpOver, DumpBearoff2, DumpBearoff1, DumpRace, DumpContact
};

extern int DumpPosition( int anBoard[ 2 ][ 25 ], char *szOutput,
			  int nPlies ) {

    float arOutput[ NUM_OUTPUTS ];
    positionclass pc = ClassifyPosition( anBoard );
    int i;
    
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
    }

    strcat( szOutput, "\n\n" );

    acdf[ pc ]( anBoard, strchr( szOutput, 0 ) );
    szOutput = strchr( szOutput, 0 );    

    strcpy( szOutput, "\n       \tWin  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
	    "Equity\n" );
    
    if( nPlies > 9 )
	nPlies = 9;
    
    for( i = 0; i <= nPlies; i++ ) {
	szOutput = strchr( szOutput, 0 );
	
	if( EvaluatePosition( anBoard, arOutput, i ) < 0 )
	    return -1;

	if( !i )
	    strcpy( szOutput, "static" );
	else
	    sprintf( szOutput, "%2d ply", i );

	szOutput = strchr( szOutput, 0 );
	
	sprintf( szOutput, ":\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t(%+6.3f)\n",
	     arOutput[ 0 ], arOutput[ 1 ], arOutput[ 2 ], arOutput[ 3 ],
	     arOutput[ 4 ], arOutput[ 0 ] * 2.0 - 1.0 + arOutput[ 1 ] +
	     arOutput[ 2 ] - arOutput[ 3 ] - arOutput[ 4 ] );
    }

    return 0;
}

extern int EvalCacheStats( int *pc, int *pcLookup, int *pcHit ) {

    *pc = cEval.c;
    
    return CacheStats( &cEval, pcLookup, pcHit );
}

extern int FindPubevalMove( int nDice0, int nDice1, int anBoard[ 2 ][ 25 ] ) {

    movelist ml;
    int i, j, anBoardTemp[ 2 ][ 25 ], anPubeval[ 28 ], fRace;

    fRace = ClassifyPosition( anBoard ) <= CLASS_RACE;
    
    GenerateMoves( &ml, anBoard, nDice0, nDice1 );
    
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
