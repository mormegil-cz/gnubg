/*
 * rollout.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999, 2000, 2001.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif

#if HAVE_ALLOCA
#ifndef alloca
#define alloca __builtin_alloca
#endif
#endif

#include <errno.h>
#include <isaac.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "matchid.h"
#include "positionid.h"
#include "rollout.h"
#include "i18n.h"
#include "format.h"

int log_rollouts = 0;
static FILE *logfp;
char *log_file_name = 0;

/* make sgf files of rollouts if log_rollouts is true and we have a file 
 * name template to work with
 */

void 
log_cube (char *action, int player) {
  fprintf (logfp, ";%s[%s]\n", player ? "B" : "W", action);
}


void
log_move (int *anMove, int player, int die0, int die1) {
  int i;

  fprintf (logfp, ";%s[%d%d", player ? "B" : "W", die0, die1);

  for (i = 0; i < 8; i += 2) {
    if (anMove[i] < 0) 
      break;

    if (anMove[i] > 23) 
      fprintf (logfp, "y");
    else if (!player)
      fprintf (logfp, "%c", 'a' + anMove[i]  );
    else
      fprintf (logfp, "%c", 'x' - anMove[i] );

    if (anMove[i + 1] < 0)
      fprintf (logfp, "z");
    else if (!player)
      fprintf (logfp, "%c", 'a' + anMove[i + 1]  );
    else
      fprintf (logfp, "%c", 'x' - anMove[i + 1] );

  }

  fprintf (logfp, "%s", "]\n");
     
}

static void board_to_sgf (int anBoard[25], int direction) {
  int	i, j;
  int   c = direction > 0 ? 'a' : 'x';

  for (i = 0; i < 24; ++i) {
    for (j = 0; j < anBoard[i]; ++j)
      fprintf (logfp, "[%c]", c);
    
    c += direction;
  }

  for (j = 0; j < anBoard[24]; ++j) 
    fprintf (logfp, "[y]");
}

    

void log_game_start (char *name, const cubeinfo *pci, int fCubeful,
		     int anBoard[2][25]) {
  time_t     t = time (0);
  struct tm  *now = localtime (&t);
  char       *rule;

  if (pci->nMatchTo == 0) {
    if (!fCubeful)
      rule = "RU[NoCube:Jacoby]";
    else if (!pci -> fJacoby) {
      rule = "";
    }
    else {
      rule = "RU[Jacoby]";
    }
  } else {
    if (!fCubeful) {
      rule = "RU[NoCube:Crawford]";
    } else if (fAutoCrawford) {
      rule = (ms.fCrawford) ? "RU[Crawford:CrawfordGame]" : "RU[Crawford]";
    } else {
      rule = "";
    }
  }

  if ((logfp = fopen (name, "w")) == 0) {
    log_rollouts = 0;
    return;
  }

  fprintf (logfp, "(;FF[4]GM[6]CA[UTF-8]AP[GNU Backgammon:0.14-devel]MI"
	   "[length:%d][game:0][ws:%d][bs:%d][wtime:0][btime:0]"
	   "[wtimeouts:0][btimeouts:0]PW[White]PB[Black]DT[%d-%02d-%02d]"
	   "%s\n", pci->nMatchTo, pci->anScore[0], pci->anScore[1], 
	   1900 + now->tm_year, 1 + now->tm_mon, now->tm_mday, rule);

  /* set the rest of the things up */
  fprintf (logfp, ";PL[%s]\n", pci->fMove ? "B" : "W" );
  fprintf (logfp, ";CP[%s]\n", pci->fCubeOwner == 0 ? "w" :
	   pci->fCubeOwner == 1 ? "b" : "c");
  fprintf (logfp, ";CV[%d]\n", pci->nCube);
  fprintf (logfp, ";AE[a:y]AW");
  board_to_sgf (anBoard[1],  1);
  fprintf (logfp, "AB");
  board_to_sgf (anBoard[0], -1);
  fprintf (logfp, "\n");
}

void log_game_over (void) {
  if (logfp) {
    fprintf (logfp, ")" );
    fclose (logfp);
  }

  logfp = 0;
}

static void
initRolloutstat ( rolloutstat *prs );

/* Quasi-random permutation array: the first index is the "generation" of the
   permutation (0 permutes each set of 36 rolls, 1 permutes those sets of 36
   into 1296, etc.); the second is the roll within the game (limited to 128,
   so we use pseudo-random dice after that); the last is the permutation
   itself.  6 generations are enough for 36^6 > 2^31 trials. */
static unsigned char aaanPermutation[ 6 ][ 128 ][ 36 ];
static int nPermutationSeed = -1;

static void QuasiRandomSeed( int n ) {

    int i, j, k, r, t;
    randctx rc;
    
    if( nPermutationSeed == n )
      return;

    for( i = 0; i < RANDSIZ; i++ )
	rc.randrsl[ i ] = n;

    irandinit( &rc, TRUE );
    
    for( i = 0; i < 6; i++ )
	for( j = i /* no need for permutations below the diagonal */; j < 128;
	     j++ ) {
	    for( k = 0; k < 36; k++ )
		aaanPermutation[ i ][ j ][ k ] = k;
	    for( k = 0; k < 35; k++ ) {
		r = irand( &rc ) % ( 36 - k );
		t = aaanPermutation[ i ][ j ][ k + r ];
		aaanPermutation[ i ][ j ][ k + r ] =
		    aaanPermutation[ i ][ j ][ k ];
		aaanPermutation[ i ][ j ][ k ] = t;
	    }
	}

    nPermutationSeed = n;
}

static int nSkip;

static int RolloutDice( int iTurn, int iGame, int cGames,
                            int fInitial,
                            int anDice[ 2 ],
                            const rng rngx,
                            const int fRotate ) {

  if ( fInitial && !iTurn ) {
      /* rollout of initial position: no doubles allowed */
      if( fRotate ) {
	  int j;
      
	  if( !iGame )
	      nSkip = 0;

	  for( ; ; nSkip++ ) {
	      j = aaanPermutation[ 0 ][ 0 ][ ( iGame + nSkip ) % 36 ];
	      
	      anDice[ 0 ] = j / 6 + 1;
	      anDice[ 1 ] = j % 6 + 1;

	      if( anDice[ 0 ] != anDice[ 1 ] )
		  break;
	  }
	  
	  return 0;
      } else {
	  int n;
     
      reroll:
	  if( ( n = RollDice( anDice, rngx ) ) )
	      return n;
	  
	  if ( fInitial && ! iTurn && anDice[ 0 ] == anDice[ 1 ] )
	      goto reroll;

	  return 0;
      }
  } else if( fRotate && iTurn < 128 ) {
      int i, /* the "generation" of the permutation */
	  j, /* the number we're permuting */
	  k; /* 36**i */
      
      for( i = 0, j = 0, k = 1; i < 6 && i <= iTurn; i++, k *= 36 )
	  j = aaanPermutation[ i ][ iTurn ][ ( (iGame + nSkip) / k + j ) % 36 ];
      
      anDice[ 0 ] = j / 6 + 1;
      anDice[ 1 ] = j % 6 + 1;
#if 0
	  printf ("Game: %d Turn: %d %d%d\n", iGame, iTurn, anDice[ 0 ],anDice[ 1 ]);
#endif
      return 0;
  } else 
      return RollDice( anDice, rngx );
}


static void
ClosedBoard ( int afClosedBoard[ 2 ], int anBoard[ 2 ][ 25 ] ) {

  int i, j, n;

  for ( i = 0; i < 2; i++ ) {

    n = 0;
    for( j = 0; j < 6; j++ )
      n += anBoard[ i ][ j ] > 1;

    afClosedBoard[ i ] = ( n == 6 );

  }

}

/* called with 
               cube decision                  move rollout
aanBoard       2 copies of same board         1 board
aarOutput      2 arrays for eval              1 array
iTurn          player on roll                 same
iGame          game number                    same
cubeinfo       2 structs for double/nodouble  1 cubeinfo
               or take/pass
CubeDecTop     array of 2 boolean             1 boolean
               (TRUE if a cube decision is valid on turn 0)
cci            2 (number of rollouts to do)   1
prc            1 rollout context              same
aarsStatistics 2 arrays of stats for the      NULL
               two alternatives of 
               cube rollouts 

returns -1 on error/interrupt, fInterrupt TRUE if stopped by user
aarOutput array(s) contain results
*/

static int
BasicCubefulRollout ( int aanBoard[][ 2 ][ 25 ],
                      float aarOutput[][ NUM_ROLLOUT_OUTPUTS ], 
                      int iTurn, int iGame,
                      const cubeinfo aci[], int afCubeDecTop[], int cci,
                      rolloutcontext *prc,
                      rolloutstat aarsStatistics[][ 2 ],
		      int nBasisCube) {

  int anDice [ 2 ], cUnfinished = cci;
  cubeinfo *pci;
  cubedecision cd;
  int *pf, ici, i, j, k;
  evalcontext ec;

  positionclass pc, pcBefore;
  int nPipsBefore = 0, nPipsAfter, nPipsDice;
  int anPips [ 2 ];
  int afClosedBoard[ 2 ];

  float arDouble[ NUM_CUBEFUL_OUTPUTS ];
  float aar[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

  int aiBar[ 2 ];

  int afClosedOut[ 2 ] = { FALSE, FALSE };
  int afHit[ 2 ] = { FALSE, FALSE };

  float rDP;
  float r;
  
  int nTruncate = prc->fDoTruncate ? prc->nTruncate: 0x7fffffff;
  int cGames = prc->nTrials;

  int nLateEvals = prc->fLateEvals ? prc->nLate : 0x7fffffff;

  /* Make local copy of cubeinfo struct, since it
     may be modified */
#if __GNUC__
  cubeinfo pciLocal[ cci ];
  int pfFinished[ cci ];
  float aarVarRedn[ cci ][ NUM_ROLLOUT_OUTPUTS ];
#elif HAVE_ALLOCA
  cubeinfo *pciLocal = alloca( cci * sizeof ( cubeinfo ) );
  int *pfFinished = alloca( cci * sizeof( int ) );
  float (*aarVarRedn)[ NUM_ROLLOUT_OUTPUTS ] =
    alloca ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );;
#else
  cubeinfo pciLocal[ MAX_ROLLOUT_CUBEINFO ];
  int pfFinished[ MAX_ROLLOUT_CUBEINFO ];
  float aarVarRedn[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
#endif

  /* variables for variance reduction */

  evalcontext aecVarRedn[ 2 ];
  float arMean[ NUM_ROLLOUT_OUTPUTS ];
  int aaanBoard[ 6 ][ 6 ][ 2 ][ 25 ];
  int aanMoves[ 6 ][ 6 ][ 8 ];
  float aaar[ 6 ][ 6 ][ NUM_ROLLOUT_OUTPUTS ];

  evalcontext ecCubeless0ply = { FALSE, 0, 0, TRUE, 0.0 };
  evalcontext ecCubeful0ply = { TRUE, 0, 0, TRUE, 0.0 };

  /* local pointers to the eval contexts to use */
  evalcontext *pecCube[2], *pecChequer[2];

  if ( prc->fVarRedn ) {

    /*
     * Create evaluation context one ply deep
     */

    for ( ici = 0; ici < cci; ici++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarVarRedn[ ici ][ i ] = 0.0f;
    
    for ( i = 0; i < 2; i++ ) {
      
      memcpy ( &aecVarRedn[ i ], &prc->aecChequer[ i ],
               sizeof ( evalcontext ) );

      if ( prc->fCubeful )
        /* other no var. redn. on cubeful equities */
        aecVarRedn[ i ].fCubeful = TRUE;

      if ( aecVarRedn[ i ].nPlies ) {
        aecVarRedn[ i ].nPlies--;
      }

    }

  }

  for ( ici = 0; ici < cci; ici++ )
    pfFinished[ ici ] = TRUE;

  memcpy ( pciLocal, aci, cci * sizeof (cubeinfo) );
  
  while ( ( !nTruncate || iTurn < nTruncate ) && cUnfinished ) {
	if (iTurn < nLateEvals) {
	  pecCube[0] = prc->aecCube;
	  pecCube[1] = prc->aecCube + 1;
	  pecChequer[0] = prc->aecChequer;
	  pecChequer[1] = prc->aecChequer + 1;
	} else {
	  pecCube[0] = prc->aecCubeLate;
	  pecCube[1] = prc->aecCubeLate + 1;
	  pecChequer[0] = prc->aecChequerLate;
	  pecChequer[1] = prc->aecChequerLate + 1;
	}

    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      /* check for truncation at bearoff databases */

      pc = ClassifyPosition ( aanBoard[ ici ], pci->bgv );

      if ( prc->fTruncBearoff2 && pc <= CLASS_PERFECT &&
           prc->fCubeful && *pf && ! pci->nMatchTo &&
           ( ( afCubeDecTop[ ici ] && ! prc->fInitial ) || iTurn > 0 ) ) {

        /* truncate at two sided bearoff if money game */

        GeneralEvaluationE( aarOutput[ ici ], aanBoard[ ici ],
                            pci, &ecCubeful0ply );

        if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

        *pf = FALSE;
        cUnfinished--;

      }
      else if ( ( ( prc->fTruncBearoff2 && pc <= CLASS_PERFECT ) ||
                  ( prc->fTruncBearoffOS && pc <= CLASS_BEAROFF_OS ) ) &&
                ! prc->fCubeful && *pf ) {
          
        /* cubeless rollout, requested to truncate at bearoff db */

        GeneralEvaluationE ( aarOutput[ ici ],
                             aanBoard[ ici ],
                             pci, &ecCubeless0ply );

	/* rollout result is for player on play (even iTurn).
	   This point is pre play, so if opponent is on roll, invert */
	
        if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

        *pf = FALSE;
        cUnfinished--;

      }
      
      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, &rDP, pci ) &&
             ( iTurn > 0 || ( afCubeDecTop[ ici ] && ! prc->fInitial ) ) ) {

          if ( GeneralCubeDecisionE ( aar, aanBoard[ ici ],
                                      pci,
                                      pecCube[ pci->fMove ], 0 ) < 0 ) 
            return -1;

          cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aar, pci );

          switch ( cd ) {

          case DOUBLE_TAKE:
          case DOUBLE_BEAVER:
          case REDOUBLE_TAKE:
	    if (log_rollouts) {
	      log_cube ("double",  pci->fMove);
	      log_cube ("take",   !pci->fMove);
	    }

            /* update statistics */
	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleTake[ LogCube ( pci->nCube ) ]++; 

            SetCubeInfo ( pci, 2 * pci->nCube, ! pci->fMove, pci->fMove,
			  pci->nMatchTo,
                          pci->anScore, pci->fCrawford, pci->fJacoby,
                          pci->fBeavers, pci->bgv );

            break;
        
          case DOUBLE_PASS:
          case REDOUBLE_PASS:
	    if (log_rollouts) {
	      log_cube ("double", pci->fMove);
	      log_cube ("drop",  !pci->fMove);
	    }

            *pf = FALSE;
	    cUnfinished--;
	    
            /* assign outputs */

            for ( i = 0; i <= OUTPUT_EQUITY; i++ )
              aarOutput[ ici ][ i ] = aar[ 0 ][ i ];

            /* 
             * assign equity for double, pass:
             * - mwc for match play
             * - normalized equity for money play (i.e, rDP=1)
             */

            aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = rDP;

            /* invert evaluations if required */

            if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

            /* update statistics */

	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleDrop[ LogCube ( pci->nCube ) ]++; 

            break;

          case NODOUBLE_TAKE:
          case TOOGOOD_TAKE:
          case TOOGOOD_PASS:
          case NODOUBLE_BEAVER:
          case NO_REDOUBLE_TAKE:
          case TOOGOODRE_TAKE:
          case TOOGOODRE_PASS:
          case NO_REDOUBLE_BEAVER:
          case OPTIONAL_DOUBLE_BEAVER:
          case OPTIONAL_DOUBLE_TAKE:
          case OPTIONAL_REDOUBLE_TAKE:
          case OPTIONAL_DOUBLE_PASS:
          case OPTIONAL_REDOUBLE_PASS:
          default:

            /* no op */
            break;
            
          }
        } /* cube */
      }
    } /* loop over ci */

    /* Chequer play */

    if( RolloutDice( iTurn, iGame, cGames, prc->fInitial, anDice,
                         prc->rngRollout, prc->fRotate ) < 0 )
      return -1;

    if( anDice[ 0 ] < anDice[ 1 ] )
	    swap( anDice, anDice + 1 );


    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        /* Save number of chequers on bar */

        for ( i = 0; i < 2; i++ )
          aiBar[ i ] = aanBoard[ ici ][ i ][ 24 ];

	/* Save number of pips (for bearoff only) */

	pcBefore = ClassifyPosition ( aanBoard[ ici ], pci->bgv );
	if ( aarsStatistics && pcBefore <= CLASS_BEAROFF1 ) {
	  PipCount ( aanBoard[ ici ], anPips );
	  nPipsBefore = anPips[ 1 ];
	}

        /* Find best move :-) */

        if ( prc->fVarRedn ) {

          /* Variance reduction */

          for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
            arMean[ i ] = 0.0f;

          for ( i = 0; i < 6; i++ )
            for ( j = 0; j <= i; j++ ) {

              if ( prc->fInitial && ! iTurn && j == i )
                /* no doubles possible for first roll when rolling
                   out as initial position */
                continue;

              memcpy ( &aaanBoard[ i ][ j ][ 0 ][ 0 ], 
                       &aanBoard[ ici ][ 0 ][ 0 ],
                       2 * 25 * sizeof ( int )  );

              /* Find the best move for each roll on ply 0 only */

	      if (FindBestMove ( aanMoves[ i ][ j ], i + 1, j + 1, 
				 aaanBoard[ i ][ j ],
				 pci, NULL, defaultFilters ) < 0 )
                return -1;

              SwapSides ( aaanBoard[ i ][ j ] );

              /* re-evaluate the chosen move at ply n-1 */

              pci->fMove = ! pci->fMove;
              GeneralEvaluationE ( aaar[ i ][ j ],
                                   aaanBoard[ i ][ j ],
                                   pci, &aecVarRedn[ pci->fMove ] );
              pci->fMove = ! pci->fMove;

              if ( ! ( iTurn & 1 ) ) InvertEvaluationR ( aaar[ i ][ j ], pci );

              /* Calculate arMean: the n-ply evaluation of the position */

              for ( k = 0; k < NUM_ROLLOUT_OUTPUTS; k++ )
                arMean[ k ] += ( ( i == j ) ? aaar[ i ][ j ][ k ] :
                                 ( aaar[ i ][ j ][ k ] * 2.0 ) );

            }

          if ( prc->fInitial && ! iTurn && j == i )
            /* no doubles ... */
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
              arMean[ i ] /= 30.0f;
          else
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
              arMean[ i ] /= 36.0f;

          /* Find best move */

          if ( pecChequer[ pci->fMove ]->nPlies ||
               prc->fCubeful != pecChequer[ pci->fMove ]->fCubeful )

            /* the user requested n-ply (n>0). Another call to
               FindBestMove is required */

	    FindBestMove( aanMoves[ anDice[ 0 ] - 1][ anDice[ 1 ] - 1], 
			  anDice[ 0 ], anDice[ 1 ],
                          aanBoard[ ici ], pci,
                          pecChequer [ pci->fMove ],
                          ( iTurn < nLateEvals ) ? 
                          prc->aaamfChequer[ pci->fMove ] : 
                          prc->aaamfLate[ pci->fMove ] );

          else {

            /* 0-ply play: best move is already recorded */

            memcpy ( &aanBoard[ ici ][ 0 ][ 0 ], 
                     &aaanBoard[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][0][0],
                     2 * 25 * sizeof ( int ) );

            SwapSides ( aanBoard[ ici ] );

          }


          /* Accumulate variance reduction terms */

          if ( pci->nMatchTo )
            for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) 
              aarVarRedn[ ici ][ i ] += arMean[ i ] -
                aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][ i ];
          else {
            for ( i = 0; i <= OUTPUT_EQUITY; i++ ) 
              aarVarRedn[ ici ][ i ] += arMean[ i ] -
                aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][ i ];

            r = arMean[ OUTPUT_CUBEFUL_EQUITY ] -
              aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ]
              [ OUTPUT_CUBEFUL_EQUITY ];
            aarVarRedn[ ici ][ OUTPUT_CUBEFUL_EQUITY ] += 
              r * pci->nCube / aci[ ici ].nCube;
          }

        }
        else {

          /* no variance reduction */
              
	  FindBestMove( aanMoves[ anDice[ 0 ] - 1][ anDice[ 1 ] - 1],
			anDice[ 0 ], anDice[ 1 ],
                        aanBoard[ ici ], pci,
                        pecChequer [ pci->fMove ],
                        ( iTurn < nLateEvals ) ? 
                        prc->aaamfChequer[ pci->fMove ] : 
                        prc->aaamfLate[ pci->fMove ] );

        }

	if (log_rollouts) {
	  log_move (aanMoves[ anDice[ 0 ] - 1][ anDice[ 1 ] - 1], 
		    pci->fMove, anDice[0], anDice[1]);
	}

        /* Save hit statistics */

        /* FIXME: record double hit, triple hits etc. ? */

	if( aarsStatistics && ! afHit [ pci->fMove ] && 
            ( aiBar[ 0 ] < aanBoard[ ici ][ 0 ][ 24 ]  ) ) {
#if 0
	  printf ("Game %d Turn %d Player %d hit\n",iGame, iTurn, pci->fMove);
#endif
          aarsStatistics[ ici ][ pci->fMove ].nOpponentHit++;
          aarsStatistics[ ici ][ pci->fMove ].rOpponentHitMove += iTurn;
          afHit[ pci->fMove ] = TRUE;

        }

        if( fAction )
          fnAction();
    
        if( fInterrupt )
          return -1;

	/* Calculate number of wasted pips */

	pc = ClassifyPosition ( aanBoard[ ici ], pci->bgv );

	if ( aarsStatistics &&
	     pc <= CLASS_BEAROFF1 && pcBefore <= CLASS_BEAROFF1 ) {

	  PipCount ( aanBoard[ ici ], anPips );
	  nPipsAfter = anPips[ 1 ];
	  nPipsDice = anDice[ 0 ] + anDice[ 1 ];
	  if ( anDice[ 0 ] == anDice[ 1 ] ) nPipsDice *= 2;

	  aarsStatistics[ ici ][ pci->fMove ].nBearoffMoves++;
	  aarsStatistics[ ici ][ pci->fMove ].nBearoffPipsLost +=
	    nPipsDice - ( nPipsBefore - nPipsAfter );

	}

	/* Opponent closed out */

	if ( aarsStatistics && ! afClosedOut[ pci->fMove ] 
	     && aanBoard[ ici ][ 0 ][ 24 ] ) {

	  /* opponent is on bar */

	  ClosedBoard ( afClosedBoard, aanBoard[ ici ] );

	  if ( afClosedBoard[ pci->fMove ] ) {
	    aarsStatistics[ ici ][ pci->fMove ].nOpponentClosedOut++;
	    aarsStatistics[ ici ]
	      [ pci->fMove ].rOpponentClosedOutMove += iTurn;
	    afClosedOut[ pci->fMove ] = TRUE;
	  }

	}


        /* check if game is over */

        if ( pc == CLASS_OVER ) {
          GeneralEvaluationE ( aarOutput[ ici ],
                               aanBoard[ ici ],
                               pci, pecCube[ pci->fMove ] );

          /* Since the game is over: cubeless equity = cubeful equity
             (convert to mwc for match play) */

          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
            ( pci->nMatchTo ) ?
            eq2mwc ( aarOutput[ ici ][ OUTPUT_EQUITY ], pci ) :
            aarOutput[ ici ][ OUTPUT_EQUITY ];

          if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

          *pf = FALSE;
          cUnfinished--;

          /* update statistics */

	  if( aarsStatistics )
	      switch ( GameStatus ( aanBoard[ ici ], pci->bgv ) ) {
	      case 1:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWin[ LogCube ( pci->nCube )]++;
		  break;
	      case 2:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinGammon[ LogCube ( pci->nCube )]++;
		  break;
	      case 3:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinBackgammon[ LogCube ( pci->nCube )]++;
		  break;
	      }

        }

        /* Invert board and more */
	
        SwapSides( aanBoard[ ici ] );

        SetCubeInfo ( pci, pci->nCube, pci->fCubeOwner,
                      ! pci->fMove, pci->nMatchTo,
                      pci->anScore, pci->fCrawford,
                      pci->fJacoby, pci->fBeavers, pci->bgv );

        assert ( cUnfinished >= 0 );


      }
    }

    iTurn++;

  } /* loop truncate */


  /* evaluation at truncation */

  for ( ici = 0, pci = pciLocal, pf = pfFinished;
        ici < cci; ici++, pci++, pf++ ) {

    if ( *pf ) {

      /* ensure cubeful evaluation at truncation */

      memcpy ( &ec, &prc->aecCubeTrunc, sizeof ( ec ) );
      ec.fCubeful = prc->fCubeful;

      /* evaluation at truncation */

      GeneralEvaluationE ( aarOutput[ ici ], aanBoard[ ici ], pci, &ec );

      if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );
          
    }

    /* the final output is the sum of the resulting evaluation and
       all variance reduction terms */

    if ( ! pci->nMatchTo ) 
      aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *=
        pci->nCube / aci [ ici ].nCube;
    
    if ( prc->fVarRedn )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarOutput[ ici ][ i ] += aarVarRedn[ ici ][ i ];

    /* multiply money equities */

    if ( ! pci->nMatchTo )
      aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *=
        aci [ ici ].nCube / nBasisCube;

    
    
/*        if ( pci->nMatchTo ) */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = */
/*            eq2mwc ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], pci ); */
/*        else */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= */
/*            pci->nCube / aci [ ici ].nCube; */
      
    }

    return 0;
}


/* called with a collection of moves or a cube decision to be rolled out.
   when called with a cube decision, the number of alternatives is always 2
   (nodouble/double or take/drop). Otherwise the number of moves is
   a parameter supplied (alternatives)

anBoard - an array[alternatives] of explicit pointers to Boards - the
    individual boards are not in and of themselves a contiguous set of
    arrays and can't be treated as int x[alternative][2][25]. 2 copies
    of the same board for cube decisions, 1 per move for move rollouts
asz an array of pointers to strings. These will be a contiguous array of
    text labels for displaying results. 2 pointers for cube decisions, 
    1 per move for move rollouts
aarOutput - an array[alternatives] of explicit pointers to arrays for the
    results of the rollout. Again, these may not be contiguous. 2 arrays for 
    cube decisions, 1 per move for move rollouts
aarStdDev - as above for std's of rollout
aarsStatistics - array of statistics used when rolling out cube decisions,
    not maintained when doing move rollouts
pprc - an array of explicit pointers to rollout contexts. There will be
    2 pointers to the same context for cube decisions, 1 per move for move 
    rollouts 
aci  - an array of explicit pointers cubeinfo's. 2 for cube decisions, one
    per move for move rollouts
alternatives - a count of the number of things to be rolled out. 2 for 
    cube decisions, number of different moves for move rollouts
fInvert - flag if equities should be inverted (used when doing take/drop
    decisions, we evaluate the double/nodouble and invert the equities
    to get take/drop
fCubeRollout - set if this is a cube decision rollout. This is needed if 
    we use RolloutGeneral to rollout an arbitrary list of moves (where not
    all the moves correspond to a given game state, so that some moves will
    have been made with a different cube owner or value or even come from
    different games and have different match scores. If this happens,
    calls to mwc2eq and se_mwc2eq need to be passed a pointer to the current
    cubeinfo structure. If we're rolling out a cube decision, we need to 
    pass the cubeinfo structure before the double is given. This won't be
    available

returns:
-1 on error or if no games were rolled out
no of games rolled out otherwise. aarOutput, aarStdDev aarsStatistic arrays
   will contain results.
pprc rollout contexts will be updated with the number of games rolled out for 
that position.
*/

static int 
comp_jsdinfo_equity (const void *a, const void *b) {
  const jsdinfo *aa = a;
  const jsdinfo *bb = b;

  if (aa->rEquity < bb->rEquity)
    return 1;
  else if (aa->rEquity > bb->rEquity)
    return -1;

  return 0;
}

static int
comp_jsdinfo_order (const void *a, const void *b) {
  const jsdinfo *aa = a;
  const jsdinfo *bb = b;

  if (aa->nOrder < bb->nOrder)
    return -1;
  else if (aa->nOrder > bb->nOrder)
    return 1;
  
  return 0;
}

extern int
RolloutGeneral( int (* apBoard[])[ 2 ][ 25 ], 
                float (* apOutput[])[ NUM_ROLLOUT_OUTPUTS ],
                float (* apStdDev[])[ NUM_ROLLOUT_OUTPUTS ],
                rolloutstat aarsStatistics[][2],
                evalsetup (* apes[]),
                const cubeinfo (* apci[]), 
                int (* apCubeDecTop[]), int alternatives, 
                int fInvert, int fCubeRollout,
                rolloutprogressfunc *pfProgress, void *pUserData ) {   
  
#if HAVE_ALLOCA
  int (* aanBoardEval )[ 2 ][ 25 ] = 
    alloca( alternatives * 50 * sizeof( int ) );
  float (* aar )[ NUM_ROLLOUT_OUTPUTS ] = 
	alloca( alternatives * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  float (* aarMu )[ NUM_ROLLOUT_OUTPUTS ] = 
	alloca( alternatives * NUM_ROLLOUT_OUTPUTS *sizeof ( float ) );
  float (* aarSigma )[ NUM_ROLLOUT_OUTPUTS ] = 
	alloca( alternatives * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  double (* aarResult )[ NUM_ROLLOUT_OUTPUTS ] = 
	alloca( alternatives * NUM_ROLLOUT_OUTPUTS * sizeof ( double ) );
  double (* aarVariance )[ NUM_ROLLOUT_OUTPUTS ] = 
	alloca( alternatives * NUM_ROLLOUT_OUTPUTS *  sizeof ( double ) );
  cubeinfo *aciLocal = alloca ( alternatives * sizeof ( cubeinfo ) );
  jsdinfo * ajiJSD = alloca ( alternatives * sizeof ( jsdinfo ));
  int *nGamesDone = alloca ( alternatives * sizeof ( int ));
  int *fNoMore =  alloca ( alternatives * sizeof ( int ));

#else
  int aanBoardEval[ MAX_ROLLOUT_CUBEINFO ][ 2 ][ 25 ];
  float aar[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo aciLocal[ MAX_ROLLOUT_CUBEINFO ];
  jsdinfo ajiJSD[MAX_ROLLOUT_CUBEINFO];
  int nGamesDone[MAX_ROLLOUT_CUBEINFO];
  int fNoMore[MAX_ROLLOUT_CUBEINFO];

#endif
  
  int i, j, alt;
  int anBoardOrig[ 2 ][ 25 ];
  int     err_too_big;
  double    v, s;
  unsigned int nFirstTrial;
  evalsetup *pes;
  rolloutcontext *prc = NULL, rcRolloutSave;
  int   cGames;
  int nIsCubeless = 0;
  int nIsCubeful = 0;
  int fOutputMWCSave = fOutputMWC;
  int active_alternatives;
  int show_jsds = 1;
  char *log_name = 0;

#if !(__GNUC__ || HAVE_ALLOCA)
  if (alternatives > MAX_ROLLOUT_CUBEINFO) {
    errno = EINVAL;
    return -1;
  }
#endif

  if( alternatives < 1 ) {
    errno = EINVAL;
    return -1;
  }

  if (! ms.nMatchTo)
    fOutputMWC = 0;

  memcpy (&rcRolloutSave, &rcRollout, sizeof (rcRollout));
  if (alternatives < 2) {
    rcRollout.fStopMoveOnJsd = 0;
    rcRollout.fStopOnJsd = 0;
  }

  /* make sure cube decisions are rolled out cubeful */
  if (fCubeRollout) {
	rcRollout.fCubeful = rcRollout.aecCubeTrunc.fCubeful =
	  rcRollout.aecChequerTrunc.fCubeful = 1;
	for (i = 0; i < 2; ++i) 
	  rcRollout.aecCube[ i ].fCubeful = rcRollout.aecChequer[ i ].fCubeful =
	  rcRollout.aecCubeLate[ i ].fCubeful = 
		rcRollout.aecChequerLate[ i] .fCubeful = 1;
  }

  /* initialise internal variables and figure out what the first 
     trial will be */

  if (log_rollouts && log_file_name) {
    log_name = malloc (strlen (log_file_name) + 6 + 2 + 4 + 1);
  }

  cGames = rcRollout.nTrials;
  nFirstTrial = ~0 ;

  for ( alt = 0; alt < alternatives; ++alt) {
    pes = apes[ alt ];
    prc = &pes->rc;

    /* fill out the JSD stuff */
    ajiJSD[ alt ].rEquity = ajiJSD[ alt ].rJSD = 0.0f;
    ajiJSD[ alt ].nRank = 0;
    ajiJSD[ alt ].nOrder = alt;

    /* save input cubeinfo */
    memcpy ( &aciLocal[ alt ], apci [ alt ], sizeof ( cubeinfo ) );

    /* Invert cubeinfo */

    if ( fInvert )
      aciLocal[ alt ].fMove = ! aciLocal[ alt ].fMove;

    if ((pes->et != EVAL_ROLLOUT) || (prc->nGamesDone == 0)) {

      /* later the saved context may to be stored with the move, so
	 cubeful/cubeless must be made consistent */
      rcRolloutSave.fCubeful = rcRolloutSave.aecCubeTrunc.fCubeful =
	rcRolloutSave.aecChequerTrunc.fCubeful = 
	(fCubeRollout || rcRolloutSave.fCubeful);
      for (i = 0; i < 2; ++i)
          rcRolloutSave.aecCube[ i ].fCubeful = 
	    rcRolloutSave.aecChequer[ i ].fCubeful =
	    rcRolloutSave.aecCubeLate[ i ].fCubeful =
	    rcRolloutSave.aecChequerLate[ i] .fCubeful = 
	    (fCubeRollout || rcRolloutSave.fCubeful);

      memcpy (prc, &rcRollout, sizeof (rolloutcontext));
      nGamesDone[ alt ] = prc->nGamesDone = 0;
      prc->nSkip = 0;
      nFirstTrial = 0;

      if (aarsStatistics) {
        initRolloutstat ( &aarsStatistics[ alt ][ 0 ] );
        initRolloutstat ( &aarsStatistics[ alt ][ 1 ] );
      }

      /* initialise internal variables */
      for (j = 0; j < NUM_ROLLOUT_OUTPUTS; ++j) {
        aarResult[ alt ][ j ] = aarVariance[ alt ][ j ] =
          aarMu[ alt ][ j ] = 0.0f;
      }
    } else {
      int nGames = prc->nGamesDone;
      double r;

      /* make sure the saved rollout contexts are consistent for
	 cubeful/not cubeful */
      prc->fCubeful = prc->aecCubeTrunc.fCubeful =
	  prc->aecChequerTrunc.fCubeful = (prc->fCubeful || fCubeRollout);
      for (i = 0; i < 2; ++i) 
	prc->aecCube[ i ].fCubeful = prc->aecChequer[ i ].fCubeful =
	  prc->aecCubeLate[ i ].fCubeful = 
	  prc->aecChequerLate[ i] .fCubeful = (prc->fCubeful || fCubeRollout);

      if ((nGamesDone[ alt ] = nGames) < nFirstTrial) 
        nFirstTrial = nGames;
      /* restore internal variables from input values */
      for ( j = 0; j < NUM_ROLLOUT_OUTPUTS; ++j) {
        r = aarMu[ alt ][ j ] = (*apOutput[ alt ])[ j ];
        aarResult[ alt ][ j ] = r * nGames;
        r = aarSigma[ alt ][ j ] = (*apStdDev[ alt ])[ j ];
        aarVariance[ alt ][ j ] = r * r * nGames;
      }
    }

    /* force all moves/cube decisions to be considered 
       and reset the upper bound on trials */
    fNoMore[ alt ] = 0;
    prc->nTrials = cGames;

    pes->et = EVAL_ROLLOUT;
    if (prc->fCubeful)
      ++nIsCubeful;
    else
      ++nIsCubeless;

    /* we can't do JSD tricks on initial positions */
    if (prc->fInitial) {
      rcRollout.fStopMoveOnJsd = 0;
      show_jsds = 0;
    }

  }
  
  /* we can't do JSD tricks if some rollouts are cubeful and some not */
  if (nIsCubeful && nIsCubeless)
    rcRollout.fStopMoveOnJsd = 0;

  /* if we're using stop on JSD, turn off stop on STD error */
  if (rcRollout.fStopMoveOnJsd)
    rcRollout.fStopOnSTD = 0;

  /* ============ begin rollout loop ============= */

  for( i = nFirstTrial; i < cGames; i++ ) {
    active_alternatives = alternatives;
    err_too_big = 1;

    for (alt = 0; alt < alternatives; ++alt) {
      pes = apes[ alt ];
      prc = &pes->rc;

      /* skip this one if it's already been done this far */
      if ((nGamesDone[ alt ] > i) || fNoMore[ alt ])
        continue;

      /* save the board we're working on */
      memcpy (anBoardOrig, apBoard[ alt ], sizeof (anBoardOrig));

      /* get the dice generator set up... */
      if( prc->fRotate ) 
        QuasiRandomSeed( prc->nSeed );

      nSkip = prc->nSkip;

      /* ... and the RNG */
      if( prc->rngRollout != RNG_MANUAL )
        InitRNGSeed( prc->nSeed + ( i << 8 ), prc->rngRollout );

      memcpy ( &aanBoardEval[alt][0][0], apBoard[ alt ], 
               sizeof( anBoardOrig ));

      /* roll something out */
#if 0
      printf ("rollout game %d alt %d\n", i, alt);
#endif

      if (log_rollouts && log_name) {
	sprintf (log_name, "%s-%5.5d-%c.sgf", log_file_name, i, alt + 'a');
	log_game_start (log_name, apci[ alt ], prc->fCubeful, 
			(int (*)[25]) (aanBoardEval + alt));
	if ( !log_rollouts) {
	  /* open failed */
	  log_rollouts = 0;
	  free (log_name);
	  log_name = 0;
	} 
      }

      BasicCubefulRollout( aanBoardEval + alt, aar + alt, 0, i, apci[ alt ], 
                           apCubeDecTop[ alt ], 1, prc, 
			   aarsStatistics ? aarsStatistics + alt : NULL,
			   aciLocal[ fCubeRollout ? 0 : alt ].nCube);

      if (log_rollouts) {
	log_game_over ();
      }

      if( fInterrupt )
        break;


      nGamesDone[ alt ] = pes->rc.nGamesDone = i + 1;

      if( fInvert ) 
        InvertEvaluationR( aar[ alt ], apci[ alt ] );
    
      /* apply the results */
      for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
        float rMuNew, rDelta;
      
        aarResult[ alt ][ j ] += aar[ alt ][ j ];
        rMuNew = aarResult[ alt ][ j ] / ( i + 1 );
      
        if ( i ) {
          /* for i == 0 aarVariance is not defined */

          rDelta = rMuNew - aarMu[ alt ][ j ];
      
          aarVariance[ alt ][ j ] =
            aarVariance[ alt ][ j ] * ( 1.0 - 1.0 / i ) +
            ( i + 1 ) * rDelta * rDelta;
        }
      
        aarMu[ alt ][ j ] = rMuNew;
      
        if( j < OUTPUT_EQUITY ) {
          if( aarMu[ alt ][ j ] < 0.0f )
            aarMu[ alt ][ j ] = 0.0f;
          else if( aarMu[ alt ][ j ] > 1.0f )
            aarMu[ alt ][ j ] = 1.0f;
        }
      
        aarSigma[ alt ][ j ] = sqrt( aarVariance[ alt ][ j ] / ( i + 1 ) );
      } /* for (j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) */

      if( fShowProgress && pfProgress ) {
        (*pfProgress)( aarMu, aarSigma, prc, aciLocal,
                       i, alt, ajiJSD[ alt ].nRank + 1,
		       ajiJSD[ alt ].rJSD, fNoMore[ alt ], show_jsds,
		       pUserData );
      }
	  
    } /* for (alt = 0; alt < alternatives; ++alt) */

    /* we've rolled everything out for this trial, check stopping
       conditions
    */

    if( fInterrupt )
      break;
      
    /* Stop rolling out moves whose Equity is more than a user selected
       multiple of the joint standard deviation of the equity difference 
       with the best move in the list. 
    */
    if (show_jsds) {
      float v, s, denominator;
      int  reset_to = i;

      for (alt = 0; alt < alternatives; ++alt) {

        /* 1) For each move, calculate the cubeful (or cubeless if
           that's what we're doing) equity  */

        if (rcRollout.fCubeful) {
          v = aarMu[ alt ][ OUTPUT_CUBEFUL_EQUITY ];
          s = aarSigma[ alt ][ OUTPUT_CUBEFUL_EQUITY ];
          
          /* if we're doing a cube rollout, we need aciLocal[0] for
             generating the equity. If we're doing moves, we use the
             cubeinfo that goes with this move.
          */
          if (ms.nMatchTo && !fOutputMWC) {
            v = mwc2eq (v, &aciLocal[ (fCubeRollout ? 0: alt) ]);
            s = se_mwc2eq (s, &aciLocal[ (fCubeRollout ? 0: alt) ] );
          }
        } else {
          v = aarMu[ alt ][ OUTPUT_EQUITY ];
          s = aarSigma[ alt ][ OUTPUT_EQUITY ];
          
          if (! ms.nMatchTo ) {
            if (fCubeRollout) {
              v *=  aciLocal[ alt ].nCube/aciLocal[ 0 ].nCube;
              s *=  aciLocal[ alt ].nCube/aciLocal[ 0 ].nCube;
            }
          } else {
            v = eq2mwc ( v, &aciLocal[ (fCubeRollout ? 0: alt) ]);
            s = se_eq2mwc ( s, &aciLocal[ (fCubeRollout ? 0: alt) ]);

            if (!fOutputMWC) {
              v = mwc2eq (v, &aciLocal[ (fCubeRollout ? 0: alt) ] );
              s = se_mwc2eq (s, &aciLocal[ (fCubeRollout ? 0: alt) ] );
            }
          }
        }
        ajiJSD[ alt ].rEquity = v;
        ajiJSD[ alt ].rJSD = s;
      }

      /* 2 sort the list in order of decreasing equity (best move first) */
      qsort ((void *) ajiJSD, alternatives, sizeof (jsdinfo), 
             comp_jsdinfo_equity);

      /* 3 replace the equities with the equity difference from the
         best move (ajiJSD[0]), the JSDs with the number of JSDs
         the equity difference represents and decide if we should 
         either stop or resume rolling a move out
      */
      v = ajiJSD[ 0 ].rEquity;
      s = ajiJSD[ 0 ].rJSD;
      s *= s;
      for (alt = alternatives - 1; alt > 0; --alt) {

        ajiJSD[ alt ].nRank = alt;
        ajiJSD[ alt ].rEquity = v - ajiJSD[ alt ].rEquity;
        
	denominator = sqrt (s + ajiJSD[ alt ].rJSD * ajiJSD[ alt ].rJSD);

	if (denominator < 1e-8)
	  denominator = 1e-8;

        ajiJSD[ alt ].rJSD = ajiJSD[ alt ].rEquity / denominator;
        
        if ((rcRollout.fStopMoveOnJsd || rcRollout.fStopOnJsd) &&
            (i >= (rcRollout.nMinimumJsdGames - 1))) {
          if (ajiJSD[ alt ].rJSD > rcRollout.rJsdLimit) {
            /* This move is no longer worth rolling out */

            fNoMore[ ajiJSD[alt].nOrder ] = 1;
            active_alternatives--;

          } else {
            /* this move needs to roll out further. It may need to
               be caught up with other moves, because it's been stopped
               for a few trials
            */
            if (fNoMore[ ajiJSD[ alt ].nOrder ]) {
              /* it was stopped, catch it up to the other moves and
                 resume rolling it out.
                 While we're catching up, we don't want to do these 
                 calculations any more so we'll change the minimum games
                 to do
              */
              fNoMore[ ajiJSD[ alt ].nOrder ] = 0;
              if (rcRollout.nMinimumJsdGames <= i)
                rcRollout.nMinimumJsdGames = i ;
          
              /* set the rollout loop index back to wherever this game was
                 when it stopped being rolled out
              */
              if (reset_to > (nGamesDone[ ajiJSD[ alt ].nOrder ] -1)) {
                reset_to = nGamesDone [ ajiJSD[ alt ].nOrder ] - 1;
              }
            }
          }
        }
      }
      
      if (rcRollout.fStopMoveOnJsd || rcRollout.fStopOnJsd) {
        i = reset_to;   
      }

      /* fill out details of best move */
      ajiJSD[ 0 ].rEquity = ajiJSD[ 0 ].rJSD = 0.0f;
      ajiJSD[ 0 ].nRank = 0;

      /* rearrange ajiJSD in move order rather than equity order */
      qsort ((void *) ajiJSD, alternatives, sizeof (jsdinfo), 
             comp_jsdinfo_order);

    }
 
    if( fShowProgress && pfProgress ) 
      for (alt = 0; alt < alternatives; ++alt) {
        (*pfProgress)( aarMu, aarSigma, prc, aciLocal,
                       i, alt, ajiJSD[ alt ].nRank + 1,
					   ajiJSD[ alt ].rJSD, fNoMore[ alt ], show_jsds,
					   pUserData );
      }

    /* see if we can quit because the answers are good enough */
    if (rcRollout.fStopOnSTD && ( i >= (rcRollout.nMinimumGames - 1))) {
      err_too_big = 0;

      for (alt = 0; alt < alternatives; ++alt) {
        int output;

        pes = apes[alt];
        prc = &pes->rc;

        for (output = 0; output < NUM_ROLLOUT_OUTPUTS; output++) {
          if ( output < OUTPUT_EQUITY ) {
            v = fabs (aarMu[ alt ][ output ]);
            s = fabs (aarSigma[ alt ][ output ]);
          } else if ( output == OUTPUT_EQUITY ) {
            if ( ! ms.nMatchTo ) { /* money game */
              v = fabs (aarMu[ alt ][ output ]);
              s = fabs (aarSigma[ alt ][ output ]);
              if (fCubeRollout) {
                v *= aciLocal[ alt ].nCube / aciLocal[ 0 ].nCube ;
                s *= aciLocal[ alt ].nCube / aciLocal[ 0 ].nCube;
              }
            } else { /* match play */
              v = fabs (mwc2eq ( eq2mwc ( aarMu[ alt ][ output ], 
                                          &aciLocal[ alt] ), 
                                 &aciLocal[ (fCubeRollout? 0 : alt) ] ));
              s = fabs (se_mwc2eq ( se_eq2mwc ( aarSigma[ alt ][ output ], 
                 &aciLocal[ alt ] ), &aciLocal[ (fCubeRollout? 0 : alt) ] ));
            }
          } else {
            if (!prc->fCubeful) 
              continue;

            if ( ! ms.nMatchTo ) { /* money game */
              v = fabs (aarMu[ alt ][ output ]);
              s = fabs (aarSigma[ alt ][ output ]); 
            } else {
              v = fabs (mwc2eq ( aarMu[ alt ][ output ], 
                                 &aciLocal [ alt ] ));
              s = fabs(se_mwc2eq ( aarSigma[ alt ][ output ], 
                                   &aciLocal[ (fCubeRollout? 0 : alt) ] ) );
            }
          }

          if ((v >= .0001) && (v * rcRollout.rStdLimit < s)) {
            err_too_big = 1;
            break;
          }
        } /* for (output = 0; output < NUM_ROLLOUT_OUTPUTS; output++) */

        if (!err_too_big) {
          fNoMore[ alt ] = 1;
        }

      } /* alt = 0; alt < alternatives; ++alt) */

    } /* if (rcRollout.fStopOnSTD && (i >= rcRollout.nMinimumGames)) */

    if (((active_alternatives < 2) && rcRollout.fStopOnJsd) || !err_too_big)
      break;
  } /* for( i = nFirstTrial; i < cGames; i++ ) */

  if (log_rollouts && log_name) {
    free (log_name);
    log_name = 0;
  }

  cGames = i;

  memcpy (&rcRollout, &rcRolloutSave, sizeof (rcRollout));
  fOutputMWC = fOutputMWCSave;

  /* return -1 if no games rolled out */
  if( ! cGames)
    return -1;

  /* store results */
  for (alt = 0; alt < alternatives; alt++) {
    
    if( apOutput[alt] )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        (*apOutput[ alt ])[ i ] = aarMu[ alt ][ i ];

    if( apStdDev[alt] )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        (*apStdDev[ alt ])[ i ] = aarSigma[ alt ][ i ]; 
  }

  if( fShowProgress
#if USE_GTK
      && !fX
#endif
      ) {
    for( i = 0; i < 79; i++ )
      outputc( ' ' );

    outputc( '\r' );
    fflush( stdout );
  }

  return cGames;
}


/*
 * General evaluation functions.
 */

extern int
GeneralEvaluation ( float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
                    rolloutstat arsStatistics[ 2 ],
                    int anBoard[ 2 ][ 25 ],
                    const cubeinfo* pci, const evalsetup* pes,
                    rolloutprogressfunc *pf, void *p ) {

  int i;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arStdDev[ i ] = 0.0f;

    return GeneralEvaluationE ( arOutput, anBoard, pci, &pes->ec );
    break;

  case EVAL_ROLLOUT:

    return GeneralEvaluationR ( arOutput, arStdDev, arsStatistics,
                                anBoard, pci, &pes->rc, pf, p );
    break;

  case EVAL_NONE:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arOutput[ i ] = arStdDev[ i ] = 0.0f;

    break;
  }

  return 0;
}

extern int
GeneralEvaluationR ( float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                     float arStdDev [ NUM_ROLLOUT_OUTPUTS ],
                     rolloutstat arsStatistics[ 2 ],
                     int anBoard[ 2 ][ 25 ],
                     const cubeinfo* pci, const rolloutcontext* prc,
                     rolloutprogressfunc *pf, void *p ) {

  int (* apBoard[1])[2][25];
  float (*apOutput[1])[NUM_ROLLOUT_OUTPUTS];
  float (*apStdDev[1])[NUM_ROLLOUT_OUTPUTS];
  evalsetup  es;
  evalsetup (* apes[1]);
  const cubeinfo (* apci[1]);
  int false = 0;
  int (* apCubeDecTop[1]);

  apBoard[0] =  (int (*)[2][25]) anBoard;
  apOutput[0] = (float (*)[NUM_ROLLOUT_OUTPUTS]) arOutput;
  apStdDev[0] = (float (*)[NUM_ROLLOUT_OUTPUTS]) arStdDev;
  apes[0] = &es;
  apci[0] = pci;
  apCubeDecTop[0] = &false;

  es.et = EVAL_NONE;
  memcpy (&es.rc, prc, sizeof (rolloutcontext));
  
  if ( RolloutGeneral ( apBoard, 
			apOutput, apStdDev, ( rolloutstat (*)[ 2 ]) arsStatistics, 
            apes, apci, apCubeDecTop, 1, FALSE, FALSE, pf, p ) < 0 )
    return -1;
  
  return 0;
}

extern int
GeneralCubeDecision ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      rolloutstat aarsStatistics[ 2 ][ 2 ],
                      int anBoard[ 2 ][ 25 ],
                      cubeinfo *pci, evalsetup *pes, 
                      rolloutprogressfunc *pf, void *p ) {

  int i, j;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( j = 0; j < 2; j++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ j ][ i ] = 0.0f;

    return GeneralCubeDecisionE ( aarOutput, anBoard, pci, &pes->ec, pes );
    break;

  case EVAL_ROLLOUT:

    return GeneralCubeDecisionR ( aarOutput, aarStdDev, aarsStatistics, 
                                  anBoard, pci, &pes->rc, pes, pf, p );
    break;

  case EVAL_NONE:

    for ( j = 0; j < 2; j++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ j ][ i ] = 0.0f;

    break;
  }

  return 0;
}


extern int
GeneralCubeDecisionR ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       rolloutstat aarsStatistics[ 2 ][ 2 ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, rolloutcontext *prc, evalsetup *pes,
                       rolloutprogressfunc *pf, void *p ) {
  

  evalsetup esLocal;
  int (* apBoard[2])[2][25] = { (int (*)[2][25]) anBoard, 
				(int (*)[2][25]) anBoard};
  float (* apOutput[2])[ NUM_ROLLOUT_OUTPUTS ] = 
	 { (float (*)[NUM_ROLLOUT_OUTPUTS]) aarOutput, 
	   (float (*)[NUM_ROLLOUT_OUTPUTS]) aarOutput + 1};
  float (* apStdDev[2])[ NUM_ROLLOUT_OUTPUTS ] = 
	 { (float (*)[NUM_ROLLOUT_OUTPUTS]) aarStdDev[0], 
	   (float (*)[NUM_ROLLOUT_OUTPUTS]) aarStdDev[1]};
  evalsetup (* apes[2]);
  cubeinfo aci[ 2 ];
  const cubeinfo (* apci[2]) = { &aci[ 0 ], &aci[ 1 ] };


  int cGames;
  int afCubeDecTop[] = { FALSE, FALSE }; /* no cube decision in 
                                            iTurn = 0 */
  int (* apCubeDecTop[2]) = { afCubeDecTop, afCubeDecTop};

  if (pes == 0) {
    /* force rollout from sratch */
    pes = &esLocal;
    memcpy (&pes->rc, &rcRollout, sizeof (rcRollout));
    pes->et = EVAL_NONE;
    pes->rc.nGamesDone=0;
  }

  apes[ 0 ] = apes[ 1 ] = pes;

  SetCubeInfo ( &aci[ 0 ], pci->nCube, pci->fCubeOwner, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  SetCubeInfo ( &aci[ 1 ], 2 * pci->nCube, ! pci->fMove, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, 
                pci->fJacoby, pci->fBeavers, pci->bgv );

  if ( ! GetDPEq ( NULL, NULL, &aci[ 0 ] ) ) {
    outputl ( _("Cube not available!") );
    return -1;
  }

  if ( ! prc->fCubeful ) {
    outputl ( _("Setting cubeful on") );
    prc->fCubeful = TRUE;
  }


  if( ( cGames = RolloutGeneral( apBoard, apOutput, apStdDev, 
                                 aarsStatistics, apes, apci,
                                 apCubeDecTop, 2, FALSE, TRUE, pf, p )) <= 0 )
    return -1;

  pes->rc.nGamesDone = cGames;
  pes->rc.nSkip = nSkip;

  return 0;

}



/*
 * Initialise rollout stat with zeroes.
 *
 * Input: 
 *    - prs: rollout stat to initialize
 *
 * Output:
 *    None.
 *
 * Returns:
 *    void.
 *
 */

static void
initRolloutstat ( rolloutstat *prs ) {

  memset ( prs, 0, sizeof ( rolloutstat ) );

}



/*
 * Construct nicely formatted string with rollout statistics.
 *
 * Input: 
 *    - prs: rollout stat 
 *    - sz : string to fill in
 *
 * Output:
 *    - sz : nicely formatted string with rollout statistics.
 *
 * Returns:
 *    sz
 *
 *
 * FIXME: hopeless outdated!
 */

extern char *
printRolloutstat ( char *sz, const rolloutstat *prs, const int cGames ) {

  static char szTemp[ 32000 ];
  static char *pc;
  int i;
  int nSum;
  int nCube;
  long nSumPoint;
  long nSumPartial;

  pc = szTemp;

  nSumPoint = 0;

  sprintf ( pc, _("Points won:\n\n") );
  pc = strchr ( pc, 0 );
  
  /* single win statistics */

  nSum = 0;
  nCube = 1;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of wins %4d-cube          %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWin[ i ],
              100.0f * prs->acWin[ i ] / cGames,
              nCube * prs->acWin[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWin[ i ];
    nCube *= 2;
    nSumPartial += nCube * prs->acWin[ i ];

  }


  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;


  /* gammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of gammon wins %4d-cube   %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWinGammon[ i ],
              100.0f * prs->acWinGammon[ i ] / cGames,
              nCube * 2 * prs->acWinGammon[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWinGammon[ i ];
    nCube *= 2;
    nSumPartial += nCube * 2 * prs->acWinGammon[ i ];

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of gammon wins       %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* backgammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of bg wins %4d-cube       %8d   %7.3f%%     %8d\n"),
              nCube,
              prs->acWinBackgammon[ i ],
              100.0f * prs->acWinBackgammon[ i ] / cGames,
	      nCube * 3 * prs->acWinBackgammon[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWinBackgammon[ i ];
    nCube *= 2;

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of bg wins           %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* double drop statistics */

  nSum = 0;
  nCube = 2;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of %4d-cube double, drop  %8d   %7.3f%%    %8d\n"),
              nCube,
              prs->acDoubleDrop[ i ],
              100.0f * prs->acDoubleDrop[ i ] / cGames,
              nCube / 2 * prs->acDoubleDrop[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acDoubleDrop[ i ];
    nCube *= 2;
    nSumPartial += nCube / 2 * prs->acDoubleDrop[ i ];

  }

  sprintf ( pc,
            _("------------------------------------------------------------------\n"
            "Total number of double, drop      %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n"),
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  sprintf ( pc,
            _("==================================================================\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "==================================================================\n\n"),
            0, 100.0f * 0 / cGames, nSumPoint  );
  pc = strchr ( pc, 0 );


  /* double take statistics */

  sprintf ( pc, _("\n\nOther statistics:\n\n") );
  pc = strchr ( pc, 0 );
  


  nCube = 2;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, _("Number of %4d-cube double, take  %8d   %7.3f%%\n"),
              nCube,
              prs->acDoubleTake[ i ],
              100.0f * prs->acDoubleTake[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nCube *= 2;

  }

  sprintf ( pc, "\n\n" );
  pc = strchr ( pc, 0 );

#if 0

 FIXME: rewrite

  /* hitting statistics */

  nSum = 0;

  for ( i = 0; i < MAXHIT; i++ ) {

    sprintf ( pc, _("Number of hits move %4d          %8d   %7.3f%%\n"),
              i + 1, 
              prs->acHit[ i ],
              100.0f * prs->acHit[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acHit[ i ];

  }

  sprintf ( pc, _("Total number of hits              %8d   %7.3f%%\n\n"),
            nSum, 100.0f * nSum / cGames  );
  pc = strchr ( pc, 0 );

#endif


  outputl ( szTemp );

  return sz;

}

/*
 * Calculate whether we should resign or not
 *
 * Input:
 *    anBoard   - current board
 *    pci       - current cube info
 *    pesResign - evaluation parameters
 *
 * Output:
 *    arResign  - evaluation
 *
 * Returns:
 *    -1 on error
 *     0 if we should not resign
 *     1,2, or 3 if we should resign normal, gammon, or backgammon,
 *     respectively.  
 *
 */

extern int
getResignation ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                 int anBoard[ 2 ][ 25 ],
                 const cubeinfo* pci, 
                 const evalsetup* pesResign) {

  float arStdDev[ NUM_ROLLOUT_OUTPUTS ];
  rolloutstat arsStatistics[ 2 ];
  float ar[ NUM_OUTPUTS ] = { 0.0, 0.0, 0.0, 1.0, 1.0 };

  float rPlay;

  /* Evaluate current position */

  if ( GeneralEvaluation ( arResign, arStdDev,
                           arsStatistics,
                           anBoard,
                           pci, pesResign, NULL, NULL ) < 0 )
    return -1;

  /* check if we want to resign */

  rPlay = Utility ( arResign, pci );

  if ( arResign [ OUTPUT_LOSEBACKGAMMON ] > 0.0f &&
       Utility ( ar, pci ) == rPlay )
    /* resign backgammon */
    return ( !pci->nMatchTo && pci->fJacoby && pci->fCubeOwner == -1 ) ? 1 : 3;
  else {

    /* worth trying to escape the backgammon */

    ar[ OUTPUT_LOSEBACKGAMMON ] = 0.0f;

    if ( arResign[ OUTPUT_LOSEGAMMON ] > 0.0f &&
         Utility ( ar, pci ) == rPlay )
      /* resign gammon */
      return ( !pci->nMatchTo && pci->fJacoby && pci->fCubeOwner == -1 ) ? 1 : 2; 
    else {

      /* worth trying to escape gammon */

      ar[ OUTPUT_LOSEGAMMON ] = 0.0f;

      return Utility ( ar, pci ) == rPlay;

    }

  }

}


extern void
getResignEquities ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                    cubeinfo *pci, 
                    int nResigned,
                    float *prBefore, float *prAfter ) {

  float ar[ NUM_OUTPUTS ] = { 0, 0, 0, 0, 0 };

  *prBefore = Utility ( arResign, pci );

  if ( nResigned > 1 )
    ar[ OUTPUT_LOSEGAMMON ] = 1.0f;
  if ( nResigned > 2 )
    ar[ OUTPUT_LOSEBACKGAMMON ] = 1.0f;

  *prAfter = Utility ( ar, pci );

}


extern int
ScoreMoveRollout ( move **ppm, const cubeinfo** ppci, int cMoves,
                   rolloutprogressfunc *pf, void *p ) {

  const cubeinfo *pci;
  int fCubeDecTop = TRUE;
  int	i;
  int nGamesDone;
  rolloutcontext *prc;

#if HAVE_ALLOCA
  int (* anBoard)[ 2 ][ 25 ] = alloca (cMoves * 2 * 25 * sizeof (int));
  int (** apBoard)[2][25] = alloca (cMoves * sizeof (float));
  float (** apOutput)[ NUM_ROLLOUT_OUTPUTS ] = 
    alloca (cMoves * NUM_ROLLOUT_OUTPUTS * sizeof (float));
  float (** apStdDev)[ NUM_ROLLOUT_OUTPUTS ] =
    alloca (cMoves * NUM_ROLLOUT_OUTPUTS * sizeof (float));
  evalsetup (** apes) = alloca (cMoves * sizeof (evalsetup *));
  const cubeinfo (** apci) = alloca (cMoves * sizeof (cubeinfo *));
  cubeinfo (* aci) = alloca (cMoves * sizeof (cubeinfo));
  int (** apCubeDecTop) = alloca (cMoves * sizeof (int *));
#else
  int 	      anBoard[10][2][25];
  int         (*apBoard[10])[2][25];
  float       (*apOutput[10])[2][25];
  float       (*apStdDev[10])[2][25];
  evalsetup   (*apes[10]);
  const cubeinfo* apci[10];
  cubeinfo    aci[ 10 ];
  int         (*apCubeDecTop[10]);

  if (cMoves > 10)
    cMoves = 10;

#endif
  
  /* initialise the arrays we'll need */
  for (i = 0; i < cMoves; ++i) {
    apBoard[ i ] = anBoard + i;
    apOutput[ i ] = &ppm[i]->arEvalMove;
    apStdDev[ i ] = &ppm[i]->arEvalStdDev;
    apes[ i ] = &ppm[i]->esMove;
    apci[ i ] = aci + i;
    memcpy (aci + i, ppci[ i ], sizeof (cubeinfo));
    apCubeDecTop[ i ] = &fCubeDecTop;

    PositionFromKey( anBoard[ i ], ppm[ i ]->auch );
      
    SwapSides( anBoard[ i ] );

    /* swap fMove in cubeinfo */
    aci[ i ].fMove = ! aci[ i ].fMove;
  
  }

  nGamesDone = RolloutGeneral ( apBoard, 
				apOutput, apStdDev, NULL,
				apes, apci, apCubeDecTop, cMoves, TRUE, FALSE,
                                pf, p );
  /* put fMove back again */
  for ( i = 0; i < cMoves; ++i) {
    aci[ i ].fMove = ! aci[ i ].fMove;
  }

  if (nGamesDone < 0)
    return -1;

  for ( i = 0; i < cMoves; ++i) {

    /* Score for move:
       rScore is the primary score (cubeful/cubeless)
       rScore2 is the secondary score (cubeless) */
    prc = &apes[ i ]->rc;
    pci = apci[ i ];

    if ( prc->fCubeful ) {
      if ( pci->nMatchTo )
	ppm[i]->rScore = 
	  mwc2eq ( ppm[ i ]->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ], pci );
      else
	ppm[ i ]->rScore = ppm[ i ]->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ];
    }
    else
      ppm[ i ]->rScore = ppm[ i ]->arEvalMove[ OUTPUT_EQUITY ];

    ppm[ i ]->rScore2 = ppm[ i ]->arEvalMove[ OUTPUT_EQUITY ];
  }

  return 0;
}


extern int
ScoreMoveGeneral ( move *pm, const cubeinfo* pci, const evalsetup* pes,
                   rolloutprogressfunc* pf, void *p )
{

  switch ( pes->et ) {
  case EVAL_EVAL:
    return ScoreMove ( pm, pci, &pes->ec, pes->ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    return ScoreMoveRollout ( &pm, &pci, 1, pf, p );
    break;
  default:
    return -1;
    break;
  }

}

