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

#define WEIGHTS_VERSION "0.10"
#define WEIGHTS_VERSION_BINARY 0.10f
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
    int nReduced;
    int fRelativeAccuracy; /* evaluate all positions according to the most
			      general positionclass, to decrease relative
			      error */
  /* cubeful evaluation */
  int fCubeful;
} evalcontext;

typedef enum _evaltype {
  EVAL_NONE, EVAL_EVAL, EVAL_ROLLOUT
} evaltype;

typedef union _evalsetup {
  evalcontext ec;
  /* rolloutcontext rc; */
} evalsetup;


typedef struct _move {
  int anMove[ 8 ];
  unsigned char auch[ 10 ];
  int cMoves, cPips;
  /* scores for this move */
  float rScore, rScore2; 
  /* evaluation for this move */
  float arEvalMove[ NUM_OUTPUTS ];
  evaltype etMove;
  evalsetup esMove;
} move;

typedef struct _cubeinfo {

  /*
   * nCube: the current value of the cube,
   * fCubeOwner: the owner of the cube,
   * fMove: the player for which we are
   *        calculating equity for,
   * arGammonPrice: the gammon prices;
   *   [ 0 ] = gammon price for player 0,
   *   [ 1 ] = gammon price for player 1,
   *   [ 2 ] = backgammon price for player 0,
   *   [ 3 ] = backgammon price for player 1.
   *
   */

  int nCube, fCubeOwner, fMove;
  float arGammonPrice[ 4 ];

} cubeinfo;

#define NORM_SCORE(n) ( nMatchTo - ( n ) ) 


extern volatile int fInterrupt, fAction;
extern int fMove, fCubeOwner, fJacoby, fCrawford;
extern int fPostCrawford, nMatchTo, anScore[ 2 ], fBeavers;
extern int nCube, fOutputMWC;
extern float rCubeX;

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
    CLASS_RACE, /* Race neural network */
    CLASS_BPG, /* On Bar, Back game, or Prime */
    CLASS_CONTACT /* Contact neural network */
} positionclass;

#define N_CLASSES (CLASS_CONTACT + 1)

#define CLASS_PERFECT CLASS_BEAROFF2
#define CLASS_ANY CLASS_OVER /* no class restrictions (for rel. accuracy) */
#define CLASS_GLOBAL CLASS_CONTACT /* class containing all positions */
				      
typedef struct _redevaldata {
  float arOutput[ NUM_OUTPUTS ];
  float rScore;
  float rWeight;
  unsigned char auch[ 10 ];
} RedEvalData;

extern int 
EvalInitialise( char *szWeights, char *szWeightsBinary,
                char *szDatabase, char *szDir, int fProgress );

extern int 
EvalSave( char *szWeights );

extern void 
SetGammonPrice( float rGammon, float rLoseGammon,
                float rBackgammon, float rLoseBackgammon );

extern int 
EvaluatePosition( int anBoard[ 2 ][ 25 ], float arOutput[],
                  cubeinfo *pci, evalcontext *pec );

extern void 
InvertEvaluation( float ar[ NUM_OUTPUTS ] );

extern void 
InvertEvaluationCf( float ar[ 4 ] );

extern int 
FindBestMove( int anMove[ 8 ], int nDice0, int nDice1,
              int anBoard[ 2 ][ 25 ], cubeinfo *pci, evalcontext *pec );

extern int 
FindnSaveBestMoves( movelist *pml,
                    int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
                    unsigned char *auchMove,
                    cubeinfo *pci, evalcontext *pec );

extern int 
FindPubevalMove( int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
		 int anMove[ 8 ] );

extern int 
TrainPosition( int anBoard[ 2 ][ 25 ], float arDesired[] );

extern int 
PipCount( int anBoard[ 2 ][ 25 ], int anPips[ 2 ] );

extern int 
DumpPosition( int anBoard[ 2 ][ 25 ], char *szOutput,
              evalcontext *pec );

extern void 
SwapSides( int anBoard[ 2 ][ 25 ] );

extern int 
GameStatus( int anBoard[ 2 ][ 25 ] );

extern int 
EvalCacheResize( int cNew );

extern int 
EvalCacheStats( int *pc, int *pcLookup, int *pcHit );

extern int 
GenerateMoves( movelist *pml, int anBoard[ 2 ][ 25 ],
               int n0, int n1, int fPartial );
extern int 
FindBestMoves( movelist *pml, 
               int nDice0, int nDice1, int anBoard[ 2 ][ 25 ],
               int c, float d, cubeinfo *pci, evalcontext *pec );

extern int 
ApplyMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] );

extern positionclass 
ClassifyPosition( int anBoard[ 2 ][ 25 ] );

/* internal use only */
extern unsigned long EvalBearoff1Full( int anBoard[ 2 ][ 25 ],
                                       float arOutput[] );

extern float
Utility( float ar[ NUM_OUTPUTS ], cubeinfo *pci );


extern int 
SetCubeInfo ( cubeinfo *ci, int nCube, int fCubeOwner,
							int fMove );
 
extern void 
swap( int *p0, int *p1 );

extern void 
SanityCheck( int anBoard[ 2 ][ 25 ], float arOutput[] );

extern void 
EvalBearoff1( int anBoard[ 2 ][ 25 ], float arOutput[] );

extern float 
KleinmanCount (int nPipOnRoll, int nPipNotOnRoll);

extern int 
EvaluatePositionCubeful( int anBoard[ 2 ][ 25 ], float arCfOutput[],
                         cubeinfo *pci, evalcontext *pec, int nPlies );

extern int
GetDPEq ( int *pfCube, float *prDPEq, cubeinfo *pci );

extern int 
GetCubeActionSz ( float arDouble[ 4 ], char *szOutput, cubeinfo *pci );

extern float
mwc2eq ( float rMwc, cubeinfo *ci );

extern float
eq2mwc ( float rEq, cubeinfo *ci );
 
extern char 
*FormatEval ( char *sz, evaltype et, evalsetup es );

#endif
