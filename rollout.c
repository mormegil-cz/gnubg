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
#include "positionid.h"
#include "rollout.h"


static void
initRolloutstat ( rolloutstat *prs );




/*
 * Get number of cube turns for cube to reach value of nCube.
 *
 * Input: 
 *    - nCube: current value of cube
 *
 * Output:
 *    None.
 *
 * Returns:
 *   number of cube turns for cube to reach value of nCube.
 * 
 */

static int getCubeTurns ( const int nCube ) {

  int i;
  int n;

  /* return log ( nCube ) / log ( 2 ) */

  if ( nCube < 1 ) return -1;

  i = 0; 
  n = 1;
  while ( n < nCube ) { i++; n *= 2; }

  return i;


}


static int QuasiRandomDice( int iTurn, int iGame, int cGames,
                            int anDice[ 2 ] ) {
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

/* Upon return, anBoard contains the board position after making the best
   play considering BOTH players' positions.  The evaluation of the play
   considering ONLY the position of the player in roll is stored in ar.
   The move used for anBoard and ar will usually (but not always) be the
   same.  Returning both values is necessary for performing variance
   reduction. */

static int 
FindBestBearoff( int anBoard[ 2 ][ 25 ], int nDice0, int nDice1,
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
	
    if( ( ml.amMoves[ i ].rScore = -Utility( ar, &ciCubeless ) ) >
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
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
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

/* Rollout with no variance reduction, until a bearoff position is reached. */
static int 
BasicRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
              int nTruncate, int iTurn, int iGame, int cGames,
              cubeinfo *pci, evalcontext *pec ) {
  
  positionclass pc;
  int anDice[ 2 ];
    
  while( ( !nTruncate || iTurn < nTruncate ) &&
	 ( pc = ClassifyPosition( anBoard ) ) > CLASS_BEAROFF1 ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
	    return -1;

    FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ], anBoard, pci, pec );

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    SwapSides( anBoard );

    iTurn++;
  }

  if( ( !nTruncate || iTurn < nTruncate ) && pc == CLASS_BEAROFF1 )
    return BearoffRollout( anBoard, arOutput, nTruncate, iTurn, iGame,
                           cGames );

  if( EvaluatePosition( anBoard, arOutput, pci, pec ) )
    return -1;

  if( iTurn & 1 )
    InvertEvaluation( arOutput );

  return 0;
}

/* Variance reduction rollout with lookahead. */
static int VarRednRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
                           int nTruncate, int iTurn, int iGame, int cGames,
                           cubeinfo *pci, evalcontext *pec ) {

  int aaanBoard[ 6 ][ 6 ][ 2 ][ 25 ],	anDice[ 2 ], i, j, n;
  float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ],
    aaar[ 6 ][ 6 ][ NUM_OUTPUTS ];
  positionclass pc;
  evalcontext ec;

  /* Create an evaluation context for evaluating positions one ply deep. */
  memcpy( &ec, pec, sizeof( ec ) );
  if( ec.nPlies ) {
    ec.nPlies--;
    ec.nSearchCandidates >>= 1;
    ec.rSearchTolerance /= 2.0;
  }
    
  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] = 0.0f;

  while( ( !nTruncate || iTurn < nTruncate ) &&
	 ( pc = ClassifyPosition( anBoard ) ) > CLASS_BEAROFF1 ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
	    return -1;
	
    if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

    for( n = 0; n < NUM_OUTPUTS; n++ )
	    arMean[ n ] = 0.0f;
		
    for( i = 0; i < 6; i++ )
	    for( j = 0; j <= i; j++ ) {
        memcpy( &aaanBoard[ i ][ j ][ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
                2 * 25 * sizeof( int ) );

        /* Find the best move for each roll at ply 0 only. */
        if( FindBestMove( NULL, i + 1, j + 1, aaanBoard[ i ][ j ],
                          pci, NULL ) < 0 )
          return -1;
		
        SwapSides( aaanBoard[ i ][ j ] );

        /* Re-evaluate the chosen move at ply n-1. */
        if( EvaluatePosition( aaanBoard[ i ][ j ], aaar[ i ][ j ],
                              pci, &ec ) )
          return -1;
			
        if( !( iTurn & 1 ) )
          InvertEvaluation( aaar[ i ][ j ] );

        /* Calculate arMean; it will be the evaluation of the
           current position at ply n. */
        for( n = 0; n < NUM_OUTPUTS; n++ )
          arMean[ n ] += ( ( i == j ) ? aaar[ i ][ j ][ n ] :
                           ( aaar[ i ][ j ][ n ] * 2.0 ) );
	    }
		
    for( n = 0; n < NUM_OUTPUTS; n++ )
	    arMean[ n ] /= 36.0;

    /* Chose which move to play for the given roll. */
    if( pec->nPlies ) {
	    /* User requested n-ply play.  Another call to FindBestMove
	       is required. */
	    if( FindBestMove( NULL, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
                        anBoard, pci, pec ) < 0 )
        return -1;
	    
	    SwapSides( anBoard );
    } else
	    /* 0-ply play; best move is already recorded. */
	    memcpy( &anBoard[ 0 ][ 0 ], &aaanBoard[ anDice[ 0 ] ]
              [ anDice[ 1 ]  ][ 0 ][ 0 ], 2 * 25 * sizeof( int ) );

    /* Accumulate variance reduction term. */
    for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] -
        aaar[ anDice[ 0 ] ][ anDice[ 1 ] ][ i ];

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    iTurn++;
  }

  /* Evaluate the resulting position accordingly. */
  if( ( !nTruncate || iTurn < nTruncate ) && pc == CLASS_BEAROFF1 ) {
    if( BearoffRollout( anBoard, ar, nTruncate, iTurn,
                        iGame, cGames ) )
	    return -1;
  } else {
    if( EvaluatePosition( anBoard, ar, pci, pec ) )
	    return -1;

    if( iTurn & 1 )
	    InvertEvaluation( ar );
  }
    
  /* The final output is the sum of the resulting evaluation and all
     variance reduction terms. */
  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] += ar[ i ];
    
  return 0;
}

extern int Rollout( int anBoard[ 2 ][ 25 ], char *sz, float arOutput[],
		    float arStdDev[], int nTruncate, int cGames, int fVarRedn,
                    cubeinfo *pci, evalcontext *pec, int fInvert ) {

  int i, j, anBoardEval[ 2 ][ 25 ], anBoardOrig[ 2 ][ 25 ];
  float ar[ NUM_ROLLOUT_OUTPUTS ];
  double arResult[ NUM_ROLLOUT_OUTPUTS ], arVariance[ NUM_ROLLOUT_OUTPUTS ];
  float arMu[ NUM_ROLLOUT_OUTPUTS ], arSigma[ NUM_ROLLOUT_OUTPUTS ];
  enum _rollouttype { BEAROFF, BASIC, VARREDN } rt;
    
  if( cGames < 1 ) {
    errno = EINVAL;
    return -1;
  }

  if( ClassifyPosition( anBoard ) == CLASS_BEAROFF1 )
    rt = BEAROFF;
  else if( fVarRedn )
    rt = VARREDN;
  else
    rt = BASIC;
    
  for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
    arResult[ i ] = arVariance[ i ] = arMu[ i ] = 0.0f;

  memcpy( &anBoardOrig[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	  sizeof( anBoardOrig ) );

  if( fInvert )
      SwapSides( anBoardOrig );
  
  for( i = 0; i < cGames; i++ ) {
      if( rngCurrent != RNG_MANUAL )
	  InitRNGSeed( rcRollout.nSeed + ( i << 8 ) );
      
      memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	      sizeof( anBoardEval ) );

      switch( rt ) {
      case BEAROFF:
	  BearoffRollout( anBoardEval, ar, nTruncate, 0, i, cGames );
	  break;
      case BASIC:
	  BasicRollout( anBoardEval, ar, nTruncate, 0, i, cGames, pci, pec ); 
	  break;
      case VARREDN:
	  VarRednRollout( anBoardEval, ar, nTruncate, 0, i, cGames, pci, pec );
	  break;
      }
      
      if( fInterrupt )
	  break;
      
      if( fInvert )
	  InvertEvaluation( ar );
      
      ar[ OUTPUT_EQUITY ] = ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
	  ar[ OUTPUT_WINGAMMON ] +
	  ar[ OUTPUT_WINBACKGAMMON ] -
	  ar[ OUTPUT_LOSEGAMMON ] -
	  ar[ OUTPUT_LOSEBACKGAMMON ];
      
      for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	  float rMuNew, rDelta;
	  
	  arResult[ j ] += ar[ j ];
	  rMuNew = arResult[ j ] / ( i + 1 );
	  
	  rDelta = rMuNew - arMu[ j ];
	  
	  arVariance[ j ] = arVariance[ j ] * ( 1.0 - 1.0 / ( i + 1 ) ) +
	      ( i + 2 ) * rDelta * rDelta;
	  
	  arMu[ j ] = rMuNew;
	  
	  if( j < OUTPUT_EQUITY ) {
	      if( arMu[ j ] < 0.0f )
		  arMu[ j ] = 0.0f;
	      else if( arMu[ j ] > 1.0f )
		  arMu[ j ] = 1.0f;
	  }
	  
	  arSigma[ j ] = sqrt( arVariance[ j ] / ( i + 1 ) );
      }

      SanityCheck( anBoardOrig, arMu );
      
      if( fShowProgress ) {
#if USE_GTK
	  if( fX )
	      GTKRolloutUpdate( &arMu, &arSigma, i, cGames, FALSE, 1, pci );
	  else
#endif
	      {
		  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
			   "%5d\r", sz, arMu[ 0 ], arMu[ 1 ], arMu[ 2 ],
			   arMu[ 3 ], arMu[ 4 ], arMu[ 5 ], arSigma[ 5 ],
			   i + 1 );
		  fflush( stdout );
	      }
      }
  }

  if( !( cGames = i ) )
    return -1;

  if( arOutput )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	    arOutput[ i ] = arMu[ i ];

  if( arStdDev )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	    arStdDev[ i ] = arSigma[ i ];	

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
  int *pf, ici, i;
  evalcontext ec;

  float arDouble[ NUM_CUBEFUL_OUTPUTS ];
  float aar[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

  int aiBar[ 2 ];

  float rDP;

  int nTruncate = prc->nTruncate;
  int cGames = prc->nTrials;

  /* Make local copy of cubeinfo struct, since it
     may be modified */
#if __GNUC__
  cubeinfo pciLocal[ cci ];
  int pfFinished[ cci ];
#elif HAVE_ALLOCA
  cubeinfo *pciLocal = alloca( cci * sizeof ( cubeinfo ) );
  int *pfFinished = alloca( cci * sizeof( int ) );
#else
  cubeinfo pciLocal[ MAX_ROLLOUT_CUBEINFO ];
  int pfFinished[ MAX_ROLLOUT_CUBEINFO ];
#endif

  
  for ( ici = 0; ici < cci; ici++ )
    pfFinished[ ici ] = TRUE;

  memcpy ( pciLocal, aci, cci * sizeof (cubeinfo) );
  
  while ( ( !nTruncate || iTurn < nTruncate ) && cUnfinished ) {
    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, &rDP, pci ) &&
             ( iTurn > 0 || afCubeDecTop[ ici ] ) ) {

          if ( GeneralCubeDecisionE ( aar, aanBoard[ ici ],
                                      pci,
                                      &prc->aecCube[ pci->fMove ] ) < 0 ) 
            return -1;

          cd = FindCubeDecision ( arDouble, aar, pci );

          switch ( cd ) {

          case DOUBLE_TAKE:
          case DOUBLE_BEAVER:
          case REDOUBLE_TAKE:
          case REDOUBLE_BEAVER:

            /* update statistics */
	    if( aarsStatistics )
		aarsStatistics[ ici ]
		    [ pci->fMove ].acDoubleTake[ getCubeTurns ( pci->nCube ) ]++; 

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
		    [ pci->fMove ].acDoubleDrop[ getCubeTurns ( pci->nCube ) ]++; 

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

    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
      return -1;

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        /* Save number of chequers on bar */

        for ( i = 0; i < 2; i++ )
          aiBar[ i ] = aanBoard[ ici ][ i ][ 25 ];

        /* Find best move :-) */

        FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                      aanBoard[ ici ], pci,
                      &prc->aecChequer [ pci->fMove ] );

        /* Save hit statistics */

        /* FIXME: record double hit, triple hits etc. ? */

	if( aarsStatistics )
	    for ( i = 0; i < 2; i++ )
		aarsStatistics[ ici ][ i ].acHit[ (iTurn < MAXHIT ) ? iTurn : MAXHIT ] +=
		    aiBar[ ! i ] != aanBoard[ ici][ ! i ][ 25 ]; 
          

        if( fAction )
          fnAction();
    
        if( fInterrupt )
          return -1;

        /* check if game is over */

        if ( ClassifyPosition ( aanBoard[ ici ] ) == CLASS_OVER ) {

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
		      acWin[ getCubeTurns ( pci->nCube )]++;
		  break;
	      case 2:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinGammon[ getCubeTurns ( pci->nCube )]++;
		  break;
	      case 3:
		  aarsStatistics[ ici ][ pci->fMove ].
		      acWinBackgammon[ getCubeTurns ( pci->nCube )]++;
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
                               0, i, aciLocal, afCubeDecTop, cci, prc,
                               aarsStatistics );
        break;
      case BASIC:
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aciLocal, afCubeDecTop, cci, prc,
                               aarsStatistics );
	  break;
      case VARREDN:

        if ( ! prc->fCubeful )
	  VarRednRollout( *aanBoardEval, *aar, prc->nTruncate,
                          0, i, cGames, &aciLocal[ 0 ],
                          &prc->aecChequer[ 0 ] );
        else
        /* FIXME: write me
           VarRednCubefulRollout( aanBoardEval, aar, 
                                  0, i, pci, prc ); */
          printf ( "not implemented yet...\n" );
        break;
      }
      
      if( fInterrupt )
	  break;
      
      for ( ici = 0; ici < cci ; ici++ ) {

        if( fInvert ) {

	  InvertEvaluation( aar[ ici ] );
          if ( aciLocal[ ici ].nMatchTo )
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
              1.0 - aar [ ici ][ OUTPUT_CUBEFUL_EQUITY ];
          else
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= -1.0f;

        }

        aar[ ici ][ OUTPUT_EQUITY ] = Utility ( aar[ ici ], &aciLocal [ ici ]);
      
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
                                prc->fCubeful, cci, aci );
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
    GTKRollout( 2, aach, prc->nTrials );
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

