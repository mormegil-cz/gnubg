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

#ifndef _ROLLOUT_H_
#define _ROLLOUT_H_

#define MAXHIT 50 /* for statistics */
#define STAT_MAXCUBE 10

typedef struct _rolloutstat {

  /* Regular win statistics (dimension is cube turns) */

  int acWin[ STAT_MAXCUBE ];
  int acWinGammon[ STAT_MAXCUBE ];
  int acWinBackgammon[ STAT_MAXCUBE ]; 

  /* Cube statistics (dimension is cube turns) */

  int acDoubleDrop[ STAT_MAXCUBE ]; /* # of Double, drop */
  int acDoubleTake[ STAT_MAXCUBE ]; /* # of Double, takes */

  /* Chequer hit statistics (dimension is move number) */
  
  /* Opponent closed out */

  int nOpponentHit;
  int rOpponentHitMove;

  /* Average loss of pips in bear-off */

  int nBearoffMoves; /* number of moves with bearoff */
  int nBearoffPipsLost; /* number of pips lost in these moves */

  /* Opponent closed out */

  int nOpponentClosedOut;
  int rOpponentClosedOutMove;

  /* FIXME: add more stuff */

} rolloutstat;

extern int nSkip;

typedef void
(rolloutprogressfunc) ( float arOutput[][ NUM_ROLLOUT_OUTPUTS ],
                        float arStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                        const rolloutcontext *prc,
                        const cubeinfo aci[],
                        const int iGame,
                        const int iAlternative,
                        void *pUserData );

extern int
RolloutGeneral( int (* apBoard[])[ 2 ][ 25 ], 
                float (* apOutput[])[ NUM_ROLLOUT_OUTPUTS ],
                float (* apStdDev[])[ NUM_ROLLOUT_OUTPUTS ],
                rolloutstat (* apStatistics[])[2],
                evalsetup (* apes[]),
                cubeinfo (* apci[]), 
                int (* apCubeDecTop[]), int alternatives, 
		int fInvert,
                rolloutprogressfunc *pfRolloutProgress,
                void *pUserData );

extern int
GeneralEvaluation ( float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
                    rolloutstat arsStatistics[ 2 ],
                    int anBoard[ 2 ][ 25 ],
                    cubeinfo *pci, evalsetup *pes,
                    rolloutprogressfunc *pfRolloutProgress,
                    void *pUserData );

extern int
GeneralEvaluationR ( float arOutput[ NUM_ROLLOUT_OUTPUTS ],
                     float arStdDev[ NUM_ROLLOUT_OUTPUTS ],
                     rolloutstat arsStatistics[ 2 ],
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, rolloutcontext *prc,
                     rolloutprogressfunc *pfRolloutProgress,
                     void *pUserData );

extern int
GeneralCubeDecision ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      rolloutstat aarsStatistics[ 2 ][ 2 ],
                      int anBoard[ 2 ][ 25 ],
                      cubeinfo *pci, evalsetup *pes,
                      rolloutprogressfunc *pfRolloutProgress,
                      void *pUserData );
                      

extern int
GeneralCubeDecisionR ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       rolloutstat aarsStatistics[ 2 ][ 2 ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, rolloutcontext *prc, evalsetup *pes,
                       rolloutprogressfunc *pfRolloutProgress,
                       void *pUserData );

/* operations on rolloutstat */

extern char *
printRolloutstat ( char *sz, const rolloutstat *prs,
                   const int cGames );

/* Resignations */

extern int
getResignation ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                 int anBoard[ 2 ][ 25 ],
                 cubeinfo *pci, 
                 evalsetup *pesResign );

extern void
getResignEquities ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                    cubeinfo *pci, 
                    int nResigned,
                    float *prBefore, float *prAfter );

extern int
ScoreMoveRollout ( move **ppm, cubeinfo **ppci, int cMoves,
                   rolloutprogressfunc *pfRolloutProgress,
                   void *pUserData );

extern int
ScoreMoveGeneral ( move *pm, cubeinfo *pci, evalsetup *pes,
                   rolloutprogressfunc *pfRolloutProgress,
                   void *pUserData );

#endif
