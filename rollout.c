/*
 * rollout.c
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

  while( iTurn < nTruncate && ClassifyPosition( anBoard ) > CLASS_PERFECT ) {
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
    
  while( iTurn < nTruncate && ( pc = ClassifyPosition( anBoard ) ) >
         CLASS_BEAROFF1 ) {
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

  if( iTurn < nTruncate && pc == CLASS_BEAROFF1 )
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

  while( iTurn < nTruncate && ( pc = ClassifyPosition( anBoard ) ) >
         CLASS_BEAROFF1 ) {
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
  if( iTurn < nTruncate && pc == CLASS_BEAROFF1 ) {
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
	  InitRNGSeed( nRolloutSeed + ( i << 8 ) );
      
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
	      GTKRolloutUpdate( &arMu, &arSigma, i, cGames, FALSE, 1 );
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
                      rolloutcontext *prc ) {

  int anDice [ 2 ];
  cubeinfo *pci;
  cubedecision cd;
  int *pf, ici, i;
  evalcontext ec;

  float arDouble[ NUM_CUBEFUL_OUTPUTS ];
  float aar[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

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

  while ( iTurn < nTruncate ) {

    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, NULL, pci ) &&
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

            SetCubeInfo ( pci, 2 * pci->nCube, ! pci->fMove, pci->fMove,
			  pci->nMatchTo,
                          pci->anScore, pci->fCrawford, pci->fJacoby,
                          pci->fBeavers);
            break;
        
          case DOUBLE_PASS:
          case REDOUBLE_PASS:

            /* FIXME: we may check if all pfFinished are false,
               in which case we are finished */

            *pf = FALSE;

            /* assign outputs */

            for ( i = 0; i <= OUTPUT_EQUITY; i++ )
              aarOutput[ ici ][ i ] = aar[ 0 ][ i ];

            aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
              arDouble[ OUTPUT_OPTIMAL ];

            /* invert evaluations if required */

            if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

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

        FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                      aanBoard[ ici ], pci,
                      &prc->aecChequer [ pci->fMove ] );

        if( fAction )
          fnAction();
    
        if( fInterrupt )
          return -1;

        /* check if game is over */

        if ( ClassifyPosition ( aanBoard[ ici ] ) == CLASS_OVER ) {

          GeneralEvaluationE ( aarOutput[ ici ],
                               aanBoard[ ici ],
                               pci, &prc->aecCube[ pci->fMove ] );

          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
            aarOutput[ ici ][ OUTPUT_EQUITY ];

          if ( iTurn & 1 ) InvertEvaluationR ( aarOutput[ ici ], pci );

          *pf = FALSE;
          
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

  printf ( "rollout %i %f %f %f %f %f %f %f\n",
           iGame,
           aarOutput[ 0 ][ 0 ],
           aarOutput[ 0 ][ 1 ],
           aarOutput[ 0 ][ 2 ],
           aarOutput[ 0 ][ 3 ],
           aarOutput[ 0 ][ 4 ],
           aarOutput[ 0 ][ 5 ],
           aarOutput[ 0 ][ 6 ] );
           

    return 0;
}


extern int
RolloutGeneral( int anBoard[ 2 ][ 25 ], char asz[][ 40 ],
                float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                rolloutcontext *prc,
                cubeinfo aci[], int afCubeDecTop[], int cci, int fInvert ) {

#if __GNUC__
  int aanBoardEval[ cci ][ 2 ][ 25 ];
  float aar[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ cci ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ cci ][ NUM_ROLLOUT_OUTPUTS ];
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
#else
  int aanBoardEval[ MAX_ROLLOUT_CUBEINFO ][ 2 ][ 25 ];
  float aar[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarMu[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  float aarSigma[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarResult[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
  double aarVariance[ MAX_ROLLOUT_CUBEINFO ][ NUM_ROLLOUT_OUTPUTS ];
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
    
  for ( ici = 0; ici < cci; ici++ )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      aarResult[ ici ][ i ] =
        aarVariance[ ici ][ i ] =
        aarMu[ ici ][ i ] = 0.0f;


  memcpy( &anBoardOrig[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	  sizeof( anBoardOrig ) );


  if( fInvert )
      SwapSides( anBoardOrig );

  
  for( i = 0; i < cGames; i++ ) {
      if( rngCurrent != RNG_MANUAL )
	  InitRNGSeed( nRolloutSeed + ( i << 8 ) );
      
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
                               0, i, aci, afCubeDecTop, cci, prc );
        break;
      case BASIC:
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aci, afCubeDecTop, cci, prc );
	  break;
      case VARREDN:

        if ( ! prc->fCubeful )
	  VarRednRollout( *aanBoardEval, *aar, prc->nTruncate,
                          0, i, cGames, &aci[ 0 ],
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
          if ( aci[ ici ].nMatchTo )
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
              1.0 - aar [ ici ][ OUTPUT_CUBEFUL_EQUITY ];
          else
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= -1.0f;

        }

        aar[ ici ][ OUTPUT_EQUITY ] = Utility ( aar[ ici ], &aci [ ici ]);
      
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
                                prc->fCubeful, cci );
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

  return cGames;
}


/*
 * General evaluation functions.
 */

extern int
GeneralEvaluation ( char *sz, 
                    float arOutput[ NUM_ROLLOUT_OUTPUTS ], 
                    float arStdDev[ NUM_ROLLOUT_OUTPUTS ], 
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

    return GeneralEvaluationR ( sz, arOutput, arStdDev, anBoard,
                                pci, &pes->rc );
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
                     int anBoard[ 2 ][ 25 ],
                     cubeinfo *pci, rolloutcontext *prc ) {

  int fCubeDecTop = TRUE;
  
  if ( RolloutGeneral ( anBoard, ( char (*)[ 40 ] ) sz,
			( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) arOutput,
			( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) arStdDev,
                        prc, pci, &fCubeDecTop, 1, FALSE ) < 0 )
    return -1;

  return 0;
}


extern int
GeneralCubeDecision ( char *sz, 
                      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
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

    return GeneralCubeDecisionR ( sz, aarOutput, aarStdDev, anBoard,
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
  if( ( cGames = RolloutGeneral( anBoard, aach, aarOutput, aarStdDev,
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
