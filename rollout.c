/*
 * rollout.h
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999.
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

#include <errno.h>
#include <math.h>
#include <stdio.h>

#include "dice.h"
#include "eval.h"
#include "positionid.h"
#include "rollout.h"

static int FindBestBearoff( int anBoard[ 2 ][ 25 ], int nDice0, int nDice1,
			    float ar[ NUM_OUTPUTS ] ) {

    int i, j, anBoardTemp[ 2 ][ 25 ], iMinRolls;
    unsigned long cBestRolls;
    movelist ml;

    GenerateMoves( &ml, anBoard, nDice0, nDice1, FALSE );
    
    ml.rBestScore = -99999.9;
    cBestRolls = -1;
    
    for( i = 0; i < ml.cMoves; i++ ) {
	PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

	SwapSides( anBoardTemp );

	if( ( j = EvalBearoff1Full( anBoardTemp, ar ) ) < cBestRolls ) {
	    iMinRolls = i;
	    cBestRolls = j;
	}
	
	if( ( ml.amMoves[ i ].rScore = -Utility( ar ) ) >
	    ml.rBestScore ) {
	    ml.iMoveBest = i;
	    ml.rBestScore = ml.amMoves[ i ].rScore;
	}
    }

    PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

    PositionFromKey( anBoardTemp, ml.amMoves[ iMinRolls ].auch );

    SwapSides( anBoardTemp );
    
    EvalBearoff1( anBoardTemp, ar );
    
    return 0;
}

static int RolloutGame( int anBoard[ 2 ][ 25 ], float arOutput[],
			int nPlies, int nTruncate, int iGame, int cGames ) {

    int anBoardEval[ 2 ][ 25 ], aaanBoard[ 6 ][ 6 ][ 2 ][ 25 ],	anDice[ 2 ],
	i, j, n, iTurn;
    float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ],
	aaar[ 6 ][ 6 ][ NUM_OUTPUTS ];
    positionclass pc;

    for( i = 0; i < NUM_OUTPUTS; i++ )
	arOutput[ i ] = 0.0;

    memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	    sizeof( anBoardEval ) );
    
    for( iTurn = 0; iTurn < nTruncate &&
	     ( pc = ClassifyPosition( anBoardEval ) ) > CLASS_PERFECT;
	 iTurn++ ) {
	if( !iTurn && !( cGames % 36 ) ) {
	    anDice[ 0 ] = ( iGame % 6 ) + 1;
	    anDice[ 1 ] = ( ( iGame / 6 ) % 6 ) + 1;
	} else if( iTurn == 1 && !( cGames % 1296 ) ) {
	    anDice[ 0 ] = ( ( iGame / 36 ) % 6 ) + 1;
	    anDice[ 1 ] = ( ( iGame / 216 ) % 6 ) + 1;
	} else
	    RollDice( anDice );
	
	if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

	if( pc == CLASS_BEAROFF1 ) {
	    EvaluatePosition( anBoardEval, arMean, 0 );
	    if( iTurn & 1 )
		InvertEvaluation( arMean );
	    
	    FindBestBearoff( anBoardEval, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
		aaar[ anDice[ 0 ] ][ anDice[ 1 ] ] );

	    if( !( iTurn & 1 ) )
		InvertEvaluation( aaar[ anDice[ 0 ] ][ anDice[ 1 ] ] );
	} else {
	    for( n = 0; n < NUM_OUTPUTS; n++ )
		arMean[ n ] = 0.0;
    
	    for( i = 0; i < 6; i++ )
		for( j = 0; j <= i; j++ ) {
		    memcpy( aaanBoard[ i ][ j ], anBoardEval,
			    sizeof( anBoardEval ) );
		
		    if( FindBestMove( nPlies - 1, NULL, i + 1, j + 1,
				      aaanBoard[ i ][ j ] ) < 0 )
			return -1;

		    SwapSides( aaanBoard[ i ][ j ] );
		
		    EvaluatePosition( aaanBoard[ i ][ j ], aaar[ i ][ j ],
				      nPlies - 1 ); /* probably cached */
		    /* FIXME probably _NOT_ cached!  add param to FBM */
		    
		    if( !( iTurn & 1 ) )
			InvertEvaluation( aaar[ i ][ j ] );

		    for( n = 0; n < NUM_OUTPUTS; n++ )
			arMean[ n ] += ( ( i == j ) ? aaar[ i ][ j ][ n ] :
					 ( aaar[ i ][ j ][ n ] * 2.0 ) );
		}

	    for( n = 0; n < NUM_OUTPUTS; n++ )
		arMean[ n ] /= 36.0;

	    if( FindBestMove( nPlies, NULL, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
			      anBoardEval ) < 0 )
		return -1;
	}

	SwapSides( anBoardEval );
	    
	for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] -
		aaar[ anDice[ 0 ] ][ anDice[ 1 ] ][ i ];

	if( fInterrupt )
	    return -1;
    }

    if( EvaluatePosition( anBoardEval, ar, nPlies ) )
	return -1;

    if( iTurn & 1 )
	InvertEvaluation( ar );

    for( i = 0; i < NUM_OUTPUTS; i++ )
	arOutput[ i ] += ar[ i ];

    SanityCheck( anBoard, arOutput );
    
    return 0;
}

extern int Rollout( int anBoard[ 2 ][ 25 ], float arOutput[], float arStdDev[],
		    int nPlies, int nTruncate, int cGames ) {
    int i, j;
    float ar[ NUM_ROLLOUT_OUTPUTS ];
    double arResult[ NUM_ROLLOUT_OUTPUTS ], arVariance[ NUM_ROLLOUT_OUTPUTS ];

    if( cGames < 1 ) {
	errno = EINVAL;
	return -1;
    }
	
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	arResult[ i ] = arVariance[ i ] = 0.0;
    
    for( i = 0; i < cGames; i++ ) {
	if( RolloutGame( anBoard, ar, nPlies, nTruncate, i, cGames ) )
	    break;

	printf( "%6d\r", i + 1 );
	fflush( stdout );

	/* ar[ NUM_OUTPUTS ] is used to store cubeless equity */
	ar[ OUTPUT_EQUITY ] = ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
	    ar[ OUTPUT_WINGAMMON ] +
	    ar[ OUTPUT_WINBACKGAMMON ] -
	    ar[ OUTPUT_LOSEGAMMON ] -
	    ar[ OUTPUT_LOSEBACKGAMMON ];
	
	for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	    arResult[ j ] += ar[ j ];
	    arVariance[ j ] += ar[ j ] * ar[ j ];
	}
    }

    if( !( cGames = i ) )
	return -1;

    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {
	if( arOutput ) {
	    arOutput[ i ] = arResult[ i ] / cGames;

	    if( i < OUTPUT_EQUITY ) {
		if( arOutput[ i ] < 0.0 )
		    arOutput[ i ] = 0.0;
		else if( arOutput[ i ] > 1.0 )
		    arOutput[ i ] = 1.0;
	    }
	}
	
	if( arStdDev ) {
	    if( cGames == 1 )
		arStdDev[ i ] = 0.0;
	    else {
		arVariance[ i ] -= cGames * arOutput[ i ] * arOutput[ i ];
		if( arVariance[ i ] < 0.0 )
		    arVariance[ i ] = 0.0;
		arStdDev[ i ] = sqrt( arVariance[ i ] / ( cGames - 1 ) );
	    }
	}
    }

    return cGames;
}
