/*
 * eval.h
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-2001.
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

#include "dice.h"
#include "bearoff.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define WEIGHTS_VERSION "0.12"
#define WEIGHTS_VERSION_BINARY 0.12f
#define WEIGHTS_MAGIC_BINARY 472.3782f

#define NUM_OUTPUTS 5
#define NUM_ROLLOUT_OUTPUTS 7
#define NUM_CUBEFUL_OUTPUTS 4

#define BETA_HIDDEN 0.1
#define BETA_OUTPUT 1.0

#define OUTPUT_WIN 0
#define OUTPUT_WINGAMMON 1
#define OUTPUT_WINBACKGAMMON 2
#define OUTPUT_LOSEGAMMON 3
#define OUTPUT_LOSEBACKGAMMON 4

#define OUTPUT_EQUITY 5 /* NB: neural nets do not output equity, only
			   rollouts do. */
#define OUTPUT_CUBEFUL_EQUITY 6

#define OUTPUT_OPTIMAL 0 /* Cubeful evalutions */
#define OUTPUT_NODOUBLE 1
#define OUTPUT_TAKE 2
#define OUTPUT_DROP 3

#define GNUBG_WEIGHTS "gnubg.weights"
#define GNUBG_WEIGHTS_BINARY "gnubg.wd"
#define GNUBG_BEAROFF "gnubg.bd"
#define GNUBG_BEAROFF_OS "gnubg_os.bd"

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

/* If this is increased, the nSearchCandidates member in struct _evalcontext
   must also be enlarged. */
#define MAX_SEARCH_CANDIDATES 127


typedef struct movefilter_s {
  int   Accept;    /* always allow this many moves. 0 means don't use this */
		   /* level, since at least 1 is needed when used. */
  int   Extra;     /* and add up to this many more... */
  float Threshold; /* ...if they are within this equity */
} movefilter;

/* we'll have filters for 1..4 ply evaluation */
#define MAX_FILTER_PLIES	4
extern movefilter defaultFilters[MAX_FILTER_PLIES][MAX_FILTER_PLIES];

typedef struct _evalcontext {
    /* FIXME expand this... e.g. different settings for different position
       classes */
    unsigned int nSearchCandidates : 7;
    unsigned int fCubeful : 1; /* cubeful evaluation */
    unsigned int nPlies : 3;
    unsigned int nReduced : 2; /* this will need to be expanded if we add
				  support for nReduced != 3 */
    unsigned int fDeterministic : 1;
    unsigned int fNoOnePlyPrune : 1;
    float        rSearchTolerance;
    float        rNoise; /* standard deviation */
} evalcontext;


typedef struct _rolloutcontext {

  evalcontext aecCube[ 2 ], aecChequer [ 2 ]; /* evaluation parameters */
  evalcontext aecCubeLate[ 2 ], aecChequerLate [ 2 ]; /* ... for later moves */
  evalcontext aecCubeTrunc, aecChequerTrunc; /* ... at truncation point */
  unsigned int fCubeful : 1; /* Cubeful rollout */
  unsigned int fVarRedn : 1; /* variance reduction */
  unsigned int fInitial: 1;  /* roll out as opening position */
  unsigned int fRotate : 1;  /* rotate dice of first two rolls */
  unsigned int fLateEvals; /* enable different evals for later moves */
  unsigned int fDoTruncate; /* enable truncated rollouts */
  unsigned short nTruncate; /* truncation */
  unsigned int nTrials; /* number of rollouts */
  unsigned int fTruncBearoff2 : 1; /* cubeless rollout: trunc at BEAROFF2 */
  unsigned int fTruncBearoffOS: 1; /* cubeless rollout: trunc at BEAROFF_OS */
  unsigned short nLate; /* switch evaluations on move nLate of game */
  rng rngRollout;
  int nSeed;

} rolloutcontext;


typedef enum _evaltype {
  EVAL_NONE, EVAL_EVAL, EVAL_ROLLOUT
} evaltype;

typedef struct _evalsetup {
  evaltype et;
  evalcontext ec;
  rolloutcontext rc;
} evalsetup;

typedef enum _cubedecision {
  DOUBLE_TAKE, DOUBLE_PASS, NODOUBLE_TAKE, TOOGOOD_TAKE, TOOGOOD_PASS,
  DOUBLE_BEAVER, NODOUBLE_BEAVER,
  REDOUBLE_TAKE, REDOUBLE_PASS, NO_REDOUBLE_TAKE,
  TOOGOODRE_TAKE, TOOGOODRE_PASS,
  NO_REDOUBLE_BEAVER,
  NODOUBLE_DEADCUBE,     /* cube is dead (match play only) */
  NO_REDOUBLE_DEADCUBE,     /* cube is dead (match play only) */
  NOT_AVAILABLE, /* Cube not available */
  OPTIONAL_DOUBLE_TAKE,
  OPTIONAL_REDOUBLE_TAKE,
  OPTIONAL_DOUBLE_BEAVER,
  OPTIONAL_DOUBLE_PASS,
  OPTIONAL_REDOUBLE_PASS
} cubedecision;

/*
 * prefined settings
 *
 */

#define NUM_SETTINGS            7
#define SETTINGS_WORLDCLASSPP   6
#define SETTINGS_WORLDCLASS     5
#define SETTINGS_ADVANCED       4
#define SETTINGS_EXPERT         3
#define SETTINGS_INTERMEDIATE   2
#define SETTINGS_NOVICE         1
#define SETTINGS_BEGINNER       0

extern evalcontext aecSettings[ NUM_SETTINGS  ];
extern const char *aszSettings[ NUM_SETTINGS ];

#define NUM_SEARCHSPACES        8

extern const char *aszSearchSpaces[ NUM_SEARCHSPACES ];
extern const int anSearchCandidates[ NUM_SEARCHSPACES ];
extern const float arSearchTolerances[ NUM_SEARCHSPACES ];


typedef struct _move {
  int anMove[ 8 ];
  unsigned char auch[ 10 ];
  int cMoves, cPips;
  /* scores for this move */
  float rScore, rScore2; 
  /* evaluation for this move */
  float arEvalMove[ NUM_ROLLOUT_OUTPUTS ];
  float arEvalStdDev[ NUM_ROLLOUT_OUTPUTS ];
  evalsetup esMove;
} move;

typedef struct _cubeinfo {
    /*
     * nCube: the current value of the cube,
     * fCubeOwner: the owner of the cube,
     * fMove: the player for which we are
     *        calculating equity for,
     * fCrawford, fJacoby, fBeavers: optional rules in effect,
     * arGammonPrice: the gammon prices;
     *   [ 0 ] = gammon price for player 0,
     *   [ 1 ] = gammon price for player 1,
     *   [ 2 ] = backgammon price for player 0,
     *   [ 3 ] = backgammon price for player 1.
     *
     */
    int nCube, fCubeOwner, fMove, nMatchTo, anScore[ 2 ], fCrawford, fJacoby,
	fBeavers;
    float arGammonPrice[ 4 ];
} cubeinfo;

extern volatile int fInterrupt, fAction;
extern void ( *fnAction )( void );
extern void ( *fnTick )( void );
extern cubeinfo ciCubeless;
extern char *aszEvalType[ EVAL_ROLLOUT + 1 ];
extern int fEgyptian;

extern bearoffcontext *pbc1;
extern bearoffcontext *pbc2;
extern bearoffcontext *pbcOS;
extern bearoffcontext *pbcTS;

typedef struct _movelist {
    int cMoves; /* and current move when building list */
    int cMaxMoves, cMaxPips;
    int iMoveBest;
    float rBestScore;
    move *amMoves;
} movelist;

typedef enum _positionclass {
    CLASS_OVER = 0,   /* Game already finished */
    CLASS_BEAROFF2,   /* Two-sided bearoff database (in memory) */
    CLASS_BEAROFF_TS, /* Two-sided bearoff database (on disk) */
    CLASS_BEAROFF1,   /* One-sided bearoff database (in memory) */
    CLASS_BEAROFF_OS, /* One-sided bearoff database /on disk) */
    CLASS_RACE,       /* Race neural network */
    CLASS_CRASHED,    /* Contact, one side has less than 7 active checkers */
    CLASS_CONTACT     /* Contact neural network */
} positionclass;

#define N_CLASSES (CLASS_CONTACT + 1)

#define CLASS_PERFECT CLASS_BEAROFF_TS

extern char *PathSearch( const char *szFile, const char *szDir );
			      
extern int
EvalInitialise( char *szWeights, char *szWeightsBinary,
		int fNoBearoff, 
                char *szDir, int nSize,
		void (*pfProgress)( int ) );

extern int
EvalShutdown( void );

extern void EvalStatus( char *szOutput );

extern int EvalNewWeights( int nSize );

extern int 
EvalSave( char *szWeights );

extern int 
EvaluatePosition( int anBoard[ 2 ][ 25 ], float arOutput[],
                  cubeinfo *pci, evalcontext *pec );

extern void
InvertEvaluationR ( float ar[ NUM_ROLLOUT_OUTPUTS],
                    cubeinfo *pci );

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
TrainPosition( int anBoard[ 2 ][ 25 ], float arDesired[], float rAlpha,
	       float rAnneal );

extern int 
PipCount( int anBoard[ 2 ][ 25 ], int anPips[ 2 ] );

extern int 
DumpPosition( int anBoard[ 2 ][ 25 ], char *szOutput,
              evalcontext *pec, cubeinfo *pci, int fOutputMWC,
	      int fOutputWinPC, int fOutputInvert );

extern void 
SwapSides( int anBoard[ 2 ][ 25 ] );

extern int 
GameStatus( int anBoard[ 2 ][ 25 ] );

extern int 
EvalCacheResize( int cNew );

extern int 
EvalCacheStats( int *pcUsed, int *pcSize, int *pcLookup, int *pcHit );

extern int 
GenerateMoves( movelist *pml, int anBoard[ 2 ][ 25 ],
               int n0, int n1, int fPartial );

extern int 
ApplyMove( int anBoard[ 2 ][ 25 ], const int anMove[ 8 ],
           const int fCheckLegal );

extern positionclass 
ClassifyPosition( int anBoard[ 2 ][ 25 ] );

/* internal use only */
extern unsigned long EvalBearoff1Full( int anBoard[ 2 ][ 25 ],
                                       float arOutput[] );

extern float
Utility( float ar[ NUM_OUTPUTS ], cubeinfo *pci );


extern int SetCubeInfoMoney( cubeinfo *pci, int nCube, int fCubeOwner,
			     int fMove, int fJacoby, int fBeavers );
extern int SetCubeInfoMatch( cubeinfo *pci, int nCube, int fCubeOwner,
			     int fMove, int nMatchTo, int anScore[ 2 ],
			     int fCrawford );
extern int 
SetCubeInfo ( cubeinfo *pci, int nCube, int fCubeOwner, int fMove,
	      int nMatchTo, int anScore[ 2 ], int fCrawford, int fJacoby,
	      int fBeavers );
 
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
                         float arClOutput[ NUM_OUTPUTS ],
                         cubeinfo *pci, evalcontext *pec, int nPlies );

extern int
GetDPEq ( int *pfCube, float *prDPEq, const cubeinfo *pci );

extern int 
GetCubeActionSz ( float arDouble[ 4 ], 
                  float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                  char *szOutput, cubeinfo *pci,
		  int fOutputMWC, int fOutputInvert );

extern float
mwc2eq ( const float rMwc, const cubeinfo *ci );

extern float
eq2mwc ( const float rEq, const cubeinfo *ci );
 
extern float
se_mwc2eq ( const float rMwc, const cubeinfo *ci );

extern float
se_eq2mwc ( const float rEq, const cubeinfo *ci );
 
extern char 
*FormatEval ( char *sz, evalsetup *pes );

extern cubedecision
FindCubeDecision ( float arDouble[],
                   float aarOutput[][ NUM_ROLLOUT_OUTPUTS ], cubeinfo *pci );

extern int
GeneralCubeDecisionE ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, evalcontext *pec );

extern int
GeneralEvaluationE ( float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, evalcontext *pec );

extern int
GeneralEvaluationEPlied ( float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                          int anBoard[ 2 ][ 25 ],
                          cubeinfo *pci, evalcontext *pec, int nPlies );

extern int 
EvaluatePositionCubeful3( int anBoard[ 2 ][ 25 ],
                          float arOutput[ NUM_OUTPUTS ],
                          float arCubeful[],
                          cubeinfo aciCubePos[], int cci, 
                          cubeinfo *pciMove,
                          evalcontext *pec, int nPlies, int fTop );

extern int 
GeneralEvaluationEPliedCubeful ( float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                                 int anBoard[ 2 ][ 25 ],
                                 cubeinfo *pci, evalcontext *pec,
                                 int nPlies );
extern int
cmp_evalsetup ( const evalsetup *pes1, const evalsetup *pes2 );

extern int
cmp_evalcontext ( const evalcontext *pec1, const evalcontext *pec2 );

extern int
cmp_rolloutcontext ( const rolloutcontext *prc1, const rolloutcontext *prc2 );

extern char 
*GetCubeRecommendation ( const cubedecision cd );

extern cubedecision
FindBestCubeDecision ( float arDouble[], 
                       float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       const cubeinfo *pci );

extern int
getCurrentGammonRates ( float aarRates[ 2 ][ 2 ], 
                        float arOutput[], 
                        int anBoard[ 2 ][ 25 ],
                        cubeinfo *pci,
                        evalcontext *pec );

extern void
getMoneyPoints ( float aaarPoints[ 2 ][ 7 ][ 2 ],
                 const int fJacoby, const int fBeavers,
                 float aarRates[ 2 ][ 2 ] );

extern void
getMatchPoints ( float aaarPoints[ 2 ][ 4 ][ 2 ],
                 int afAutoRedouble[ 2 ],
                 int afDead[ 2 ],
                 cubeinfo *pci,
                 float aarRates[ 2 ][ 2 ] );

extern void
getCubeDecisionOrdering ( int aiOrder[ 3 ],
                          float arDouble[ 4 ], 
                          float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                          cubeinfo *pci );

extern float
getPercent ( const cubedecision cd,
             const float arDouble[] );

extern void
RefreshMoveList ( movelist *pml, int *ai );

extern int 
ScoreMove( move *pm, cubeinfo *pci, evalcontext *pec, int nPlies );

extern void
CopyMoveList ( movelist *pmlDest, const movelist *pmlSrc );

extern int
isCloseCubedecision ( const float arDouble[] );

extern int
isMissedDouble ( float arDouble[], 
                 float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                 const int fDouble, 
                 const cubeinfo *pci );

extern int
locateMove ( int anBoard[ 2 ][ 25 ], 
             const int anMove[ 8 ], const movelist *pml );

extern int
MoveKey ( int anBoard[ 2 ][ 25 ], const int anMove[ 8 ], 
          unsigned char auch[ 10 ] );

extern int 
PathOpen( const char *szFile, const char *szDir, const int f );

#endif
