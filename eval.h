/*
 * eval.h
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

#ifndef _EVAL_H_
#define _EVAL_H_

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define WEIGHTS_VERSION "0.0"
#define WEIGHTS_VERSION_BINARY 0.0f
#define WEIGHTS_MAGIC_BINARY 472.3782f

#define NUM_OUTPUTS 5
#define NUM_ROLLOUT_OUTPUTS 6 /* Includes equity */

#define BETA_HIDDEN 0.1
#define BETA_OUTPUT 1.0

#define OUTPUT_WIN 0
#define OUTPUT_WINGAMMON 1
#define OUTPUT_WINBACKGAMMON 2
#define OUTPUT_LOSEGAMMON 3
#define OUTPUT_LOSEBACKGAMMON 4

#define OUTPUT_EQUITY 5 /* NB: neural nets do not output equity, only
			   rollouts do. */
			   
#define GNUBG_WEIGHTS "gnubg.weights"
#define GNUBG_WEIGHTS_BINARY "gnubg.wd"
#define GNUBG_BEAROFF "gnubg.bd"

/* A trivial upper bound on the number of (complete or incomplete)
 * legal moves of a single roll: if all 15 chequers are spread out,
 * then there are 18 C 4 + 17 C 3 + 16 C 2 + 15 C 1 = 3875
 * combinations in which a roll of 11 could be played (up to 4 choices from
 * 15 chequers, and a chequer may be chosen more than once).  The true
 * bound will be lower than this (because there are only 26 points,
 * some plays of 15 chequers must "overlap" and map to the same
 * resulting position), but that would be more difficult to
 * compute. */
#define MAX_INCOMPLETE_MOVES 3875
#define MAX_MOVES 3060

#if __GNUC__ || HAVE_ALLOCA
#define MAX_SEARCH_CANDIDATES MAX_MOVES
#else
#define MAX_SEARCH_CANDIDATES 64
#endif

typedef struct _evalcontext {
    /* FIXME expand this... e.g. different settings for different position
       classes */
    int nPlies;
    int nSearchCandidates;
    float rSearchTolerance;
} evalcontext;

typedef struct _move {
    int anMove[ 8 ];
    unsigned char auch[ 10 ];
    int cMoves, cPips;
    float rScore, *pEval;
} move;

extern volatile int fInterrupt, fAction;
extern void ( *fnAction )( void );

typedef struct _movelist {
    int cMoves; /* and current move when building list */
    int cMaxMoves, cMaxPips;
    int iMoveBest;
    float rBestScore;
    move *amMoves;
} movelist;

typedef enum _positionclass {
    CLASS_OVER = 0, /* Game already finished */
    CLASS_BEAROFF2, /* Two-sided bearoff database */
    CLASS_BEAROFF1, /* One-sided bearoff database */
    CLASS_RACE,     /* Race neural network */
    CLASS_CONTACT,  /* Contact neural network */
    CLASS_BPG       /* On Bar, Back game, or Prime */
} positionclass;

#define N_CLASSES (CLASS_BPG + 1)

#define CLASS_PERFECT CLASS_BEAROFF2

extern int EvalInitialise( char *szWeights, char *szWeightsBinary,
			   char *szDatabase, char *szDir );
extern int EvalSave( char *szWeights );

extern void SetGammonPrice( float rGammon, float rLoseGammon,
			    float rBackgammon, float rLoseBackgammon );
extern int EvaluatePosition( int anBoard[ 2 ][ 25 ], float arOutput[],
			     evalcontext *pec );
extern void InvertEvaluation( float ar[ NUM_OUTPUTS ] );
extern int FindBestMove( int anMove[ 8 ], int nDice0, int nDice1,
			 int anBoard[ 2 ][ 25 ], evalcontext *pec );
extern int FindPubevalMove( int nDice0, int nDice1, int anBoard[ 2 ][ 25 ] );

extern int TrainPosition( int anBoard[ 2 ][ 25 ], float arDesired[] );

extern int PipCount( int anBoard[ 2 ][ 25 ], int anPips[ 2 ] );

extern int DumpPosition( int anBoard[ 2 ][ 25 ], char *szOutput,
			 evalcontext *pec );

extern void SwapSides( int anBoard[ 2 ][ 25 ] );
extern int GameStatus( int anBoard[ 2 ][ 25 ] );

extern int EvalCacheResize( int cNew );
extern int EvalCacheStats( int *pc, int *pcLookup, int *pcHit );

extern int GenerateMoves( movelist *pml, int anBoard[ 2 ][ 25 ],
			  int n0, int n1, int fPartial );
extern int FindBestMoves( movelist *pml, float ar[][ NUM_OUTPUTS ],
			  int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
			  int c, float d, evalcontext *pec );
extern int ApplyMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] );
extern positionclass ClassifyPosition( int anBoard[ 2 ][ 25 ] );

/* internal use only */
extern unsigned long EvalBearoff1Full( int anBoard[ 2 ][ 25 ],
				       float arOutput[] );
extern float Utility( float ar[ NUM_OUTPUTS ] );
extern void swap( int *p0, int *p1 );
extern void SanityCheck( int anBoard[ 2 ][ 25 ], float arOutput[] );
extern void EvalBearoff1( int anBoard[ 2 ][ 25 ], float arOutput[] );

#endif
