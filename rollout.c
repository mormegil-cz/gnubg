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

#include "config.h"

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "matchid.h"
#include "positionid.h"
#include "rollout.h"


static void
initRolloutstat ( rolloutstat *prs );

static int QuasiRandomDice( int iTurn, int iGame, int cGames,
                            int fInitial,
                            int anDice[ 2 ] ) {

  if ( fInitial ) {

    /* rollout of initial position: no doubles allowed */

    if( !iTurn && !( cGames % 30 ) ) {
      anDice[ 1 ] = ( ( iGame / 5 ) % 6 ) + 1;
      anDice[ 0 ] = ( iGame % 5 ) + 1;
      if ( anDice[ 0 ] >= anDice[ 1 ] ) 
        anDice[ 0 ]++;
      
      return 0;
    } 
    else if( iTurn == 1 && !( cGames % 1080 ) ) {
      anDice[ 0 ] = ( ( iGame / 30 ) % 6 ) + 1;
      anDice[ 1 ] = ( ( iGame / 180 ) % 6 ) + 1;

      return 0;
    } 
    else {

      int n;
     
    reroll:
      n = RollDice( anDice );

      if ( fInitial && ! iTurn && anDice[ 0 ] == anDice[ 1 ] )
        goto reroll;

      return n;

    }

  }
  else {

    /* normal rollout: doubles allow on first roll */

    if( !iTurn && !( cGames % 36 ) ) {
      anDice[ 0 ] = ( iGame % 6 ) + 1;
      anDice[ 1 ] = ( ( iGame / 6 ) % 6 ) + 1;
      return 0;
    } else if( iTurn == 1 && !( cGames % 1296 ) ) {
      anDice[ 0 ] = ( ( iGame / 36 ) % 6 ) + 1;
      anDice[ 1 ] = ( ( iGame / 216 ) % 6 ) + 1;
      return 0;
    } else
      return RollDice( anDice );

  }

}

/* Upon return, anBoard contains the board position after making the best
   play considering BOTH players' positions.  The evaluation of the play
   considering ONLY the position of the player in roll is stored in ar.
   The move used for anBoard and ar will usually (but not always) be the
   same.  Returning both values is necessary for performing variance
   reduction. */

static int 
FindBestBearoff( int anBoard[ 2 ][ 25 ], int nDice0, int nDice1,
                 float ar[ NUM_OUTPUTS ] ) {

  int i, j, anBoardTemp[ 2 ][ 25 ], iMinRolls = 0;
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
	
    if( ( ml.amMoves[ i ].rScore = -Utility( ar, &ciCubeless ) ) >
        ml.rBestScore ) {
	    ml.iMoveBest = i;
	    ml.rBestScore = ml.amMoves[ i ].rScore;
    }
  }

  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  PositionFromKey( anBoardTemp, ml.amMoves[ iMinRolls ].auch );

  SwapSides( anBoardTemp );
    
  EvalBearoff1( anBoardTemp, ar, 0 );
    
  return 0;
}

/* Rollouts of bearoff positions, with race database variance reduction and no
   lookahead. */
static int 
BearoffRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
                int nTruncate, int iTurn, int iGame, int cGames ) {

  int anDice[ 2 ], i;
  float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ], arMin[ NUM_OUTPUTS ];

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] = 0.0f;

  while( ( !nTruncate || iTurn < nTruncate ) &&
	 ClassifyPosition( anBoard ) > CLASS_PERFECT ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, FALSE, anDice ) < 0 )
	    return -1;
	
    if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

    if( EvaluatePosition( anBoard, arMean, &ciCubeless, 0 ) < 0 )
	    return -1;
	
    if( iTurn & 1 )
	    InvertEvaluation( arMean );
	    
    FindBestBearoff( anBoard, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
                     arMin );

    if( !( iTurn & 1 ) )
	    InvertEvaluation( arMin );
	
    SwapSides( anBoard );

    for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] - arMin[ i ];

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    iTurn++;
  }

  if( EvaluatePosition( anBoard, ar, &ciCubeless, 0 ) )
    return -1;

  if( iTurn & 1 )
    InvertEvaluation( ar );

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] += ar[ i ];

  return 0;
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

static int
BasicCubefulRollout ( int aanBoard[][ 2 ][ 25 ],
                      float aarOutput[][ NUM_ROLLOUT_OUTPUTS ], 
                      int iTurn, int iGame,
                      cubeinfo aci[], int afCubeDecTop[], int cci,
                      rolloutcontext *prc,
                      rolloutstat aarsStatistics[][ 2 ] ) {

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

  float rDP;

  int nTruncate = prc->nTruncate;
  int cGames = prc->nTrials;

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
  float aaar[ 6 ][ 6 ][ NUM_ROLLOUT_OUTPUTS ];

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
        ec.nSearchCandidates >>= 1;
        ec.rSearchTolerance /= 2.0;
      }

    }

  }

  for ( ici = 0; ici < cci; ici++ )
    pfFinished[ ici ] = TRUE;

  memcpy ( pciLocal, aci, cci * sizeof (cubeinfo) );
  
  while ( ( !nTruncate || iTurn < nTruncate ) && cUnfinished ) {
    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, &rDP, pci ) &&
             ( iTurn > 0 || ( afCubeDecTop[ ici ] && ! prc->fInitial ) ) ) {

          if ( GeneralCubeDecisionE ( aar, aanBoard[ ici ],
                                      pci,
                                      &prc->aecCube[ pci->fMove ] ) < 0 ) 
            return -1;

          cd = FindCubeDecision ( arDouble, aar, pci );

          switch ( cd ) {

          case DOUBLE_TAKE:
          case DOUBLE_BEAVER:
          case REDOUBLE_TAKE:

            /* update statistics */
	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleTake[ LogCube ( pci->nCube ) ]++; 

            SetCubeInfo ( pci, 2 * pci->nCube, ! pci->fMove, pci->fMove,
			  pci->nMatchTo,
                          pci->anScore, pci->fCrawford, pci->fJacoby,
                          pci->fBeavers);

            break;
        
          case DOUBLE_PASS:
          case REDOUBLE_PASS:
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
          default:

            /* no op */
            break;
            
          }
        } /* cube */
      }
    } /* loop over ci */

    /* Chequer play */

    if( QuasiRandomDice( iTurn, iGame, cGames, prc->fInitial, anDice ) < 0 )
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

	pcBefore = ClassifyPosition ( aanBoard[ ici ] );
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

              memcpy ( &aaanBoard[ i ][ j ][ 0 ][ 0 ], 
                       &aanBoard[ ici ][ 0 ][ 0 ],
                       2 * 25 * sizeof ( int )  );

              /* Find the best move for each roll on ply 0 only */

              if ( FindBestMove ( NULL, i + 1, j + 1, aaanBoard[ i ][ j ],
                                  pci, NULL ) < 0 )
                return -1;

              SwapSides ( aaanBoard[ i ][ j ] );

              /* re-evaluate the chosen move at ply n-1 */

              GeneralEvaluationE ( aaar[ i ][ j ],
                                   aaanBoard[ i ][ j ],
                                   pci, &aecVarRedn[ pci->fMove ] );

              if ( ! ( iTurn & 1 ) ) InvertEvaluationR ( aaar[ i ][ j ], pci );

              /* Calculate arMean: the n-ply evaluation of the position */

              for ( k = 0; k < NUM_ROLLOUT_OUTPUTS; k++ )
                arMean[ k ] += ( ( i == j ) ? aaar[ i ][ j ][ k ] :
                                 ( aaar[ i ][ j ][ k ] * 2.0 ) );

            }

          for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
            arMean[ i ] /= 36.0f;

          /* Find best move */

          if ( prc->aecChequer[ pci->fMove ].nPlies &&
               prc->fCubeful == prc->aecChequer[ pci->fMove ].fCubeful )

            /* the user requested n-ply (n>0). Another call to
               FindBestMove is required */

            FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                          aanBoard[ ici ], pci,
                          &prc->aecChequer [ pci->fMove ] );

          else {

            /* 0-ply play: best move is already recorded */

            memcpy ( &aanBoard[ ici ][ 0 ][ 0 ], 
                     &aaanBoard[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][0][0],
                     2 * 25 * sizeof ( int ) );

            SwapSides ( aanBoard[ ici ] );

          }


          /* Accumulate variance reduction terms */

          for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
            aarVarRedn[ ici ][ i ] += arMean[ i ] -
              aaar[ anDice[ 0 ] - 1 ][ anDice[ 1 ] - 1 ][ i ];

        }
        else {

          /* no variance reduction */
              
          FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                        aanBoard[ ici ], pci,
                        &prc->aecChequer [ pci->fMove ] );

        }

        /* Save hit statistics */

        /* FIXME: record double hit, triple hits etc. ? */

	if( aarsStatistics )
	    for ( i = 0; i < 2; i++ )
		aarsStatistics[ ici ][ i ].
		  acHit[ (iTurn < MAXHIT ) ? iTurn : MAXHIT ] +=
		  aiBar[ ! i ] != aanBoard[ ici][ ! i ][ 24 ];  
          

        if( fAction )
          fnAction();
    
        if( fInterrupt )
          return -1;

	/* Calculate number of wasted pips */

	pc = ClassifyPosition ( aanBoard[ ici ] );

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
	      [ pci->fMove ].nOpponentClosedOutMove += iTurn;
	    afClosedOut[ pci->fMove ] = TRUE;
	  }

	}


        /* check if game is over */

        if ( pc == CLASS_OVER ) {

          GeneralEvaluationE ( aarOutput[ ici ],
                               aanBoard[ ici ],
                               pci, &prc->aecCube[ pci->fMove ] );

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
	      switch ( GameStatus ( aanBoard[ ici ] ) ) {
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
                      pci->fJacoby, pci->fBeavers );

      }
    }

    iTurn++;

  } /* loop truncate */


  /* evaluation at truncation */

  for ( ici = 0, pci = pciLocal, pf = pfFinished;
        ici < cci; ici++, pci++, pf++ ) {

    if ( *pf ) {

      /* ensure cubeful evaluation at truncation */

      memcpy ( &ec, &prc->aecCube[ pci->fMove ], sizeof ( ec ) );
      ec.fCubeful = prc->fCubeful;

      /* evaluation at truncation */

      GeneralEvaluationE ( aarOutput[ ici ],
                           aanBoard[ ici ],
                           pci, &ec );

      if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );
          
    }

    /* the final output is the sum of the resulting evaluation and
       all variance reduction terms */

    if ( prc->fVarRedn )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarOutput[ ici ][ i ] += aarVarRedn[ ici ][ i ];

      /* convert to MWC or normalize against old cube value. */

/*        if ( pci->nMatchTo ) */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = */
/*            eq2mwc ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], pci ); */
/*        else */
/*          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= */
/*            pci->nCube / aci [ ici ].nCube; */
      if ( ! pci->nMatchTo ) 
        aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *=
          pci->nCube / aci [ ici ].nCube;
      
    }

/*
  printf ( "rollout %i %f %f %f %f %f %f %f\n",
           iGame,
           aarOutput[ 0 ][ 0 ],
           aarOutput[ 0 ][ 1 ],
           aarOutput[ 0 ][ 2 ],
           aarOutput[ 0 ][ 3 ],
           aarOutput[ 0 ][ 4 ],
           aarOutput[ 0 ][ 5 ],
           aarOutput[ 0 ][ 6 ] );
 */
           

    return 0;
}

extern int
RolloutGeneral( int anBoard[ 2 ][ 25 ], char asz[][ 40 ],
                float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                rolloutstat aarsStatistics[][ 2 ],
                rolloutcontext *prc,
                cubeinfo aci[], int afCubeDecTop[], int cci, int fInvert ) {

#if __GNUC__
  int aanBoardEval[ cci ][ 2 ][ 25 ];
  float aar[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo aciLocal[ cci ];
#elif HAVE_ALLOCA
  int ( *aanBoardEval )[ 2 ][ 25 ] = alloca( cci * 50 * sizeof int );
  float ( *aar )[ NUM_ROLLOUT_OUTPUTS ] = alloca( cci * NUM_ROLLOUT_OUTPUTS *
						  sizeof float );
  float ( *aarMu )[ NUM_ROLLOUT_OUTPUTS ] = alloca( cci * NUM_ROLLOUT_OUTPUTS *
						    sizeof float );
  float ( *aarSigma )[ NUM_ROLLOUT_OUTPUTS ] = alloca( cci *
						       NUM_ROLLOUT_OUTPUTS *
						       sizeof float );
  double ( *aarResult )[ NUM_ROLLOUT_OUTPUTS ] = alloca( cci *
							 NUM_ROLLOUT_OUTPUTS *
							 sizeof double );
  double ( *aarVariance )[ NUM_ROLLOUT_OUTPUTS ] = alloca(
      cci * NUM_ROLLOUT_OUTPUTS * sizeof double );
  cubeinfo *aciLocal = alloca ( cci * sizeof ( cubeinfo ) );
#else
  int aanBoardEval[ MAX_ROLLOUT_CUBEINFO ][ 2 ][ 25 ];
  float aar[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo aciLocal[ MAX_ROLLOUT_CUBEINFO ];
#endif
  
  int i, j, ici;
  int anBoardOrig[ 2 ][ 25 ];

  enum _rollouttype { BEAROFF, BASIC, VARREDN } rt;

  int cGames = prc->nTrials;
    
  if( cGames < 1 ) {
    errno = EINVAL;
    return -1;
  }


  if( cci < 1 ) {
    errno = EINVAL;
    return -1;
  }

  if( ClassifyPosition( anBoard ) == CLASS_BEAROFF1 ) 
    rt = BEAROFF;
  else if ( prc->fVarRedn )
    rt = VARREDN;
  else
    rt = BASIC;
    
  for ( ici = 0; ici < cci; ici++ ) {

    /* initialise result matrices */

    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      aarResult[ ici ][ i ] =
        aarVariance[ ici ][ i ] =
        aarMu[ ici ][ i ] = 0.0f;

    /* Initialise statistics */

    if( aarsStatistics ) {
	initRolloutstat ( &aarsStatistics[ ici ][ 0 ] );
	initRolloutstat ( &aarsStatistics[ ici ][ 1 ] );
    }

    /* save input cubeinfo */

    memcpy ( &aciLocal[ ici ], &aci[ ici ], sizeof ( cubeinfo ) );

    /* Invert cubeinfo */

    if ( fInvert )
      aciLocal[ ici ].fMove = ! aciLocal[ ici ].fMove;
  }


  memcpy( &anBoardOrig[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	  sizeof( anBoardOrig ) );


  if( fInvert )
      SwapSides( anBoardOrig );

  
  for( i = 0; i < cGames; i++ ) {
      if( rngCurrent != RNG_MANUAL )
	  InitRNGSeed( rcRollout.nSeed + ( i << 8 ) );
      
      for ( ici = 0; ici < cci; ici++ )
        memcpy ( &aanBoardEval[ ici ][ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
                 sizeof ( anBoardOrig ) );
      /*  memcpy ( &aanBoardEval[ ici ][ 0 ][ 0 ], &anBoardOrig[ 0 ][ 0 ],
          sizeof ( anBoardOrig ) ); */

      /* FIXME: old code was:
         
         memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
         sizeof( anBoardEval ) );

         should it be anBoardOrig? */

      switch( rt ) {
      case BEAROFF:
        if ( ! prc->fCubeful )
	  BearoffRollout( *aanBoardEval, *aar, prc->nTruncate,
                          0, i, prc->nTrials );
        else
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aci, afCubeDecTop, cci, prc,
                               aarsStatistics );
        break;
      case BASIC:
      case VARREDN:
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aci, afCubeDecTop, cci, prc,
                               aarsStatistics );
	  break;
      }
      
      if( fInterrupt )
	  break;
      
      for ( ici = 0; ici < cci ; ici++ ) {

        // aar[ ici ][ OUTPUT_EQUITY ] = Utility ( aar[ ici ], &aci [ ici ]);


        if( fInvert ) InvertEvaluationR( aar[ ici ], &aci[ ici ] );

      
        for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	  float rMuNew, rDelta;
	  
	  aarResult[ ici ][ j ] += aar[ ici ][ j ];
	  rMuNew = aarResult[ ici ][ j ] / ( i + 1 );
	  
	  rDelta = rMuNew - aarMu[ ici ][ j ];
	  
	  aarVariance[ ici ][ j ] =
            aarVariance[ ici ][ j ] * ( 1.0 - 1.0 / ( i + 1 ) ) +
	      ( i + 2 ) * rDelta * rDelta;
	  
	  aarMu[ ici ][ j ] = rMuNew;
	  
	  if( j < OUTPUT_EQUITY ) {
	      if( aarMu[ ici ][ j ] < 0.0f )
		  aarMu[ ici ][ j ] = 0.0f;
	      else if( aarMu[ ici ][ j ] > 1.0f )
		  aarMu[ ici ][ j ] = 1.0f;
	  }
	  
	  aarSigma[ ici ][ j ] = sqrt( aarVariance[ ici ][ j ] / ( i + 1 ) );
        }

        SanityCheck( anBoardOrig, aarMu[ ici ] );

      }
      
      if( fShowProgress ) {
#if USE_GTK
	if( fX ) 
	      GTKRolloutUpdate( aarMu, aarSigma, i, cGames,
                                prc->fCubeful, cci, aciLocal );
	  else
#endif
	      {
#if 0
		  /* FIXME this output is too wide to fit in 80 columns */
		  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
			   "Cubeful: %6.3f %5.3f %5d\r", asz[ 0 ],
                           aarMu[ 0 ][ 0 ], aarMu[ 0 ][ 1 ], aarMu[ 0 ][ 2 ],
			   aarMu[ 0 ][ 3 ], aarMu[ 0 ][ 4 ], aarMu[ 0 ][ 5 ],
                           aarSigma[ 0 ][ 5 ],
                           aarMu[ 0 ][ 6 ], aarSigma[ 0 ][ 6 ],
			   i + 1 );
#endif
		  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
			   "%5d\r", asz[ 0 ], aarMu[ 0 ][ 0 ],
			   aarMu[ 0 ][ 1 ], aarMu[ 0 ][ 2 ],
			   aarMu[ 0 ][ 3 ], aarMu[ 0 ][ 4 ],
			   aarMu[ 0 ][ 5 ], aarSigma[ 0 ][ 5 ], i + 1 );
		  fflush( stdout );
	      }
      }
  }

  if( !( cGames = i ) )
    return -1;

  if( aarOutput )
    for ( ici = 0; ici < cci; ici++ )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarOutput[ ici ][ i ] = aarMu[ ici ][ i ];

  if( aarStdDev )
    for ( ici = 0; ici < cci; ici++ )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ ici ][ i ] = aarSigma[ ici ][ i ];	

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

  /* print statistics */

#if 0
  /* FIXME: make output prettier -- and move to GTKRollout*. */

  if( aarsStatistics )
      for ( ici = 0; ici < cci; ici++ ) {
	  outputf ( "\n\n rollout no %d\n", ici );
	  for ( i = 0; i < 2; i++ ) {
	      outputf ( "\n\nplayer %d\n\n", i );
	      
	      printRolloutstat ( NULL, &aarsStatistics[ ici ][ i ], cGames );
	      
	  }
	  
      }
#endif

  return cGames;
}

/*
 * General evaluation functions.
 */

extern int
GeneralEvaluation ( char *sz, 
                    float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
                    rolloutstat arsStatistics[ 2 ],
                    int anBoard[ 2 ][ 25 ],
                    cubeinfo *pci, evalsetup *pes ) {

  int i;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arStdDev[ i ] = 0.0f;

    return GeneralEvaluationE ( arOutput, anBoard,
                                pci, &pes->ec );
    break;

  case EVAL_ROLLOUT:

    return GeneralEvaluationR ( sz, arOutput, arStdDev, arsStatistics,
                                anBoard, pci, &pes->rc );
    break;

  case EVAL_NONE:

    for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      arOutput[ i ] = arStdDev[ i ] = 0.0f;

    break;
  }

  return 0;
}

extern int
GeneralEvaluationR ( char *sz,
                     float arOutput [ NUM_ROLLOUT_OUTPUTS ],
                     float arStdDev [ NUM_ROLLOUT_OUTPUTS ],
                     rolloutstat arsStatistics[ 2 ],
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, rolloutcontext *prc ) {

  int fCubeDecTop = TRUE;
  
  if ( RolloutGeneral ( anBoard, ( char (*)[ 40 ] ) sz,
			( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) arOutput,
			( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) arStdDev,
                        ( rolloutstat (*)[ 2 ]) arsStatistics,
                        prc, pci, &fCubeDecTop, 1, FALSE ) < 0 )
    return -1;

  return 0;
}

extern int
GeneralCubeDecision ( char *sz, 
                      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      rolloutstat aarsStatistics[ 2 ][ 2 ],
                      int anBoard[ 2 ][ 25 ],
                      cubeinfo *pci, evalsetup *pes ) {

  int i, j;

  switch ( pes->et ) {
  case EVAL_EVAL:

    for ( j = 0; j < 2; j++ )
      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ j ][ i ] = 0.0f;

    return GeneralCubeDecisionE ( aarOutput, anBoard,
                                  pci, &pes->ec );
    break;

  case EVAL_ROLLOUT:

    return GeneralCubeDecisionR ( sz, aarOutput, aarStdDev, aarsStatistics, anBoard, 
                                  pci, &pes->rc );
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
GeneralCubeDecisionR ( char *sz, 
                       float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                       rolloutstat aarsStatistics[ 2 ][ 2 ],
                       int anBoard[ 2 ][ 25 ],
                       cubeinfo *pci, rolloutcontext *prc ) {

  cubeinfo aci[ 2 ];

  char aach[ 2 ][ 40 ];

  int i, cGames;
  int afCubeDecTop[] = { FALSE, FALSE }; /* no cube decision in 
                                            iTurn = 0 */

  SetCubeInfo ( &aci[ 0 ], pci->nCube, pci->fCubeOwner, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, pci->fJacoby, pci->fBeavers );

  FormatCubePosition ( aach[ 0 ], &aci[ 0 ] );

  SetCubeInfo ( &aci[ 1 ], 2 * pci->nCube, ! pci->fMove, pci->fMove,
                pci->nMatchTo, pci->anScore, pci->fCrawford, pci->fJacoby, pci->fBeavers );

  FormatCubePosition ( aach[ 1 ], &aci[ 1 ] );

  if ( ! GetDPEq ( NULL, NULL, &aci[ 0 ] ) ) {
    outputl ( "Cube not available!" );
    return -1;
  }

  if ( ! prc->fCubeful ) {
    outputl ( "Setting cubeful on" );
    prc->fCubeful = TRUE;
  }


#if USE_GTK
  if( fX )
    GTKRollout( 2, aach, prc->nTrials, aarsStatistics );
  else
#endif
    outputl( "                               Win  W(g) W(bg)  L(g) L(bg) "
             "Equity                    Trials" );
	
#if USE_GTK
  if( fX )
    GTKRolloutRow( 0 );
#endif
  if( ( cGames = RolloutGeneral( anBoard, aach, aarOutput, aarStdDev, aarsStatistics,
                                 prc, aci, afCubeDecTop, 2, FALSE ) ) <= 0 )
    return -1;

#if USE_GTK
	if( !fX )
#endif
          for ( i = 0; i < 2; i++ )
	    outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) "
                     "Cubeful: %6.3f %12d\n"
		     "              Standard error %5.3f %5.3f %5.3f %5.3f"
		     " %5.3f (%6.3f)         %6.3f\n\n",
		     aach[ i ],
                     aarOutput[ i ][ 0 ], aarOutput[ i ][ 1 ],
                     aarOutput[ i ][ 2 ], aarOutput[ i ][ 3 ],
                     aarOutput[ i ][ 4 ], aarOutput[ i ][ 5 ],
                     aarOutput[ i ][ 6 ],
                     cGames,
                     aarStdDev[ i ][ 0 ], aarStdDev[ i ][ 1 ],
                     aarStdDev[ i ][ 2 ], aarStdDev[ i ][ 3 ],
                     aarStdDev[ i ][ 4 ], aarStdDev[ i ][ 5 ],
                     aarStdDev[ i ][ 6 ] ); 
    
#if USE_GTK
    if( fX )
	GTKRolloutDone();
#endif	
  
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

  sprintf ( pc, "Points won:\n\n" );
  pc = strchr ( pc, 0 );
  
  /* single win statistics */

  nSum = 0;
  nCube = 1;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, "Number of wins %4d-cube          %8d   %7.3f%%     %8d\n",
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
            "------------------------------------------------------------------\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n",
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;


  /* gammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, "Number of gammon wins %4d-cube   %8d   %7.3f%%     %8d\n",
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
            "------------------------------------------------------------------\n"
            "Total number of gammon wins       %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n",
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* backgammon win statistics */

  nSum = 0;
  nCube = 1;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, "Number of bg wins %4d-cube       %8d   %7.3f%%     %8d\n",
              nCube,
              prs->acWinBackgammon[ i ],
              100.0f * prs->acWinBackgammon[ i ] / cGames,
	      nCube * 3 * prs->acWinBackgammon[ i ] );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acWinBackgammon[ i ];
    nCube *= 2;

  }

  sprintf ( pc,
            "------------------------------------------------------------------\n"
            "Total number of bg wins           %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n",
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  /* double drop statistics */

  nSum = 0;
  nCube = 2;
  nSumPartial = 0;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, "Number of %4d-cube double, drop  %8d   %7.3f%%    %8d\n",
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
            "------------------------------------------------------------------\n"
            "Total number of double, drop      %8d   %7.3f%%     %8ld\n"
            "------------------------------------------------------------------\n\n",
            nSum, 100.0f * nSum / cGames, nSumPartial  );
  pc = strchr ( pc, 0 );

  nSumPoint += nSumPartial;

  sprintf ( pc,
            "==================================================================\n"
            "Total number of wins              %8d   %7.3f%%     %8ld\n"
            "==================================================================\n\n",
            0, 100.0f * 0 / cGames, nSumPoint  );
  pc = strchr ( pc, 0 );


  /* double take statistics */

  sprintf ( pc, "\n\nOther statistics:\n\n" );
  pc = strchr ( pc, 0 );
  


  nCube = 2;

  for ( i = 0; i < 10; i++ ) {

    sprintf ( pc, "Number of %4d-cube double, take  %8d   %7.3f%%\n",
              nCube,
              prs->acDoubleTake[ i ],
              100.0f * prs->acDoubleTake[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nCube *= 2;

  }

  sprintf ( pc, "\n\n" );
  pc = strchr ( pc, 0 );


  /* hitting statistics */

  nSum = 0;

  for ( i = 0; i < MAXHIT; i++ ) {

    sprintf ( pc, "Number of hits move %4d          %8d   %7.3f%%\n",
              i + 1, 
              prs->acHit[ i ],
              100.0f * prs->acHit[ i ] / cGames  );
    pc = strchr ( pc, 0 );
    
    nSum += prs->acHit[ i ];

  }

  sprintf ( pc, "Total number of hits              %8d   %7.3f%%\n\n",
            nSum, 100.0f * nSum / cGames  );
  pc = strchr ( pc, 0 );


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
                 cubeinfo *pci, 
                 evalsetup *pesResign ) {

  float arStdDev[ NUM_ROLLOUT_OUTPUTS ];
  rolloutstat arsStatistics[ 2 ];
  float ar[ NUM_OUTPUTS ] = { 0.0, 0.0, 0.0, 1.0, 1.0 };

  float rPlay;

  /* Evaluate current position */

  if ( GeneralEvaluation ( NULL,
                           arResign, arStdDev,
                           arsStatistics,
                           anBoard,
                           pci, pesResign ) < 0 )
    return -1;

  /* check if we want to resign */

  rPlay = Utility ( arResign, pci );

  if ( arResign [ OUTPUT_LOSEBACKGAMMON ] > 0.0f &&
       Utility ( ar, pci ) == rPlay )
    return 3; /* resign backgammon */
  else {

    /* worth trying to escape the backgammon */

    ar[ OUTPUT_LOSEBACKGAMMON ] = 0.0f;

    if ( arResign[ OUTPUT_LOSEGAMMON ] > 0.0f &&
         Utility ( ar, pci ) == rPlay )
      return 2; /* resign gammon */

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
