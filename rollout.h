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

#if __GNUC__ || HAVE_ALLOCA
#define MAX_ROLLOUT_CUBEINFO (-1)
#else
#define MAX_ROLLOUT_CUBEINFO 16
#endif

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

extern int
RolloutGeneral( int anBoard[ 2 ][ 25 ], char asz[][ 40 ],
                float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                rolloutstat aarsStatistics[][ 2 ],
                rolloutcontext *prc,
                cubeinfo aci[], int afCubeDecTop[], int cci, int fInvert );

extern int
GeneralEvaluation ( char *sz,
                    float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
                    rolloutstat arsStatistics[ 2 ],
                    int anBoard[ 2 ][ 25 ],
                    cubeinfo *pci, evalsetup *pes );

extern int
GeneralEvaluationR ( char *sz,
                     float arOutput[ NUM_ROLLOUT_OUTPUTS ],
                     float arStdDev[ NUM_ROLLOUT_OUTPUTS ],
                     rolloutstat arsStatistics[ 2 ],
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, rolloutcontext *prc );

extern int
GeneralCubeDecision ( char *sz, 
                      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      rolloutstat aarsStatistics[ 2 ][ 2 ],
                      int anBoard[ 2 ][ 25 ],
                      cubeinfo *pci, evalsetup *pes );

extern int
GeneralCubeDecisionR ( char *sz, 
                       float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       rolloutstat aarsStatistics[ 2 ][ 2 ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, rolloutcontext *prc );

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



#endif
