/*
 * analysis.h
 *
 * by Joern Thyssen <joern@thyssen.nu>, 2000
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "positionid.h"
#include "analysis.h"

#ifndef HUGE_VALF
#define HUGE_VALF (-1e38)
#endif

extern int
ComputeStatGame ( list *plGame, statcontext *pscGame );

extern void
DumpStatcontext ( statcontext *psc, int fCompleteAnalysis, char * sz );

extern void
IniStatcontext ( statcontext *psc );

extern void
AddStatcontext ( statcontext *pscA, statcontext *pscB );

const char *aszRating [ RAT_EXTRA_TERRESTRIAL + 1 ] = {
  "Beginner", "Novice", "Intermediate", "Advanced", "Expert",
  "World class", "Extra-terrestrial" };

const float arThrsRating [ RAT_EXTRA_TERRESTRIAL + 1 ] = {
  1e38, 0.030, 0.025, 0.020, 0.015, 0.010, 0.005 };

static float LuckAnalysis( int anBoard[ 2 ][ 25 ], int n0, int n1,
			   cubeinfo *pci, int fFirstMove ) {

    int anBoardTemp[ 2 ][ 25 ], i, j;
    float aar[ 6 ][ 6 ], ar[ NUM_OUTPUTS ], rMean = 0.0f;

    if( n0-- < n1-- )
	swap( &n0, &n1 );
    
    for( i = 0; i < 6; i++ )
	for( j = 0; j <= i; j++ ) {
	    memcpy( &anBoardTemp[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
		    2 * 25 * sizeof( int ) );
	    
	    /* Find the best move for each roll at ply 0 only. */
	    if( FindBestMove( NULL, i + 1, j + 1, anBoardTemp, pci,
			      NULL ) < 0 )
		return -HUGE_VALF;
	    
	    SwapSides( anBoardTemp );
	    
	    if( EvaluatePosition( anBoardTemp, ar, pci, NULL ) )
		return -HUGE_VALF;

	    InvertEvaluation( ar );
	    aar[ i ][ j ] = Utility( ar, pci );
	    
	    rMean += ( i == j ) ? aar[ i ][ j ] : aar[ i ][ j ] * 2.0f;
	}
    
    return fFirstMove ? aar[ n0 ][ n1 ] : aar[ n0 ][ n1 ] - rMean / 36.0f;
}

static void
AnalyzeGame ( list *plGame ) {

    list *pl;
    moverecord *pmr, *pmrPrev;
    int i = 0, nCube = 1, anBoard[ 2 ][ 25 ];
    int fCrawfordLocal = fCrawford, fPostCrawfordLocal = fPostCrawford,
	fJacobyLocal = fJacoby, fBeaversLocal = fBeavers,
	nMatchToLocal = nMatchTo;
    int anBoardMove[ 2 ][ 25 ];
    int anScore[ 2 ];
    int fCubeOwner = -1, fPlayer = -1;
    int anDice[ 2 ], j;
    int fFirstMove = 1;
    unsigned char auch[ 10 ];
    cubeinfo ci;
    float arDouble[ 4 ], arOutput [ NUM_OUTPUTS ];
    int fWinner, nPoints;
#if DEBUG_ANALYSIS
    char sz[ 2048 ];
    char *apch [ 7 ], szPlayer0[ 32 ], szPlayer1[ 32 ], szText[ 32 ];
    char szCube[ 32 ];
#endif

    InitBoard( anBoard );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
      pmr = pl->p;

      Progress();
      
#if DEBUG_ANALYSIS
      /* set apch string for call to drawboard */

      apch[ 0 ] = szPlayer0;
      apch[ 1 ] = NULL;
      apch[ 2 ] = NULL;
      apch[ 3 ] = NULL;
      apch[ 4 ] = NULL;
      apch[ 5 ] = NULL;
      apch[ 6 ] = szPlayer1;

      sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
      sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

      if ( pmr->mt != MOVE_DOUBLE ) {

        if( fCubeOwner < 0 ) {
          apch[ 3 ] = szCube;
        
          sprintf( szCube, "(Cube: %d)", nCube );
        } else {
          int cch = strlen( ap[ fCubeOwner ].szName );
        
          if( cch > 20 )
            cch = 20;
        
          sprintf( szCube, "%*s (Cube: %d)", cch,
                   ap[ fCubeOwner ].szName, nCube );
        
          apch[ fCubeOwner ? 6 : 0 ] = szCube;
        }
      }
#endif
      
      switch( pmr->mt ) {
      case MOVE_NORMAL:

        anDice[ 0 ] = pmr->n.anRoll[ 0 ];
        anDice[ 1 ] = pmr->n.anRoll[ 1 ];
        fPlayer = pmr->n.fPlayer;

#if DEBUG_ANALYSIS
        apch [ fPlayer ? 5 : 1 ] = szText;
        sprintf( szText, "Rolled %d%d", anDice[ 0 ], anDice[ 1 ] );

	DrawBoard( sz, anBoard, TRUE, apch );
        puts ( sz );
#endif
	
        if ( ! fPlayer )
          SwapSides( anBoard );

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
		      anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

        /* cube action? */

        if ( fAnalyseCube && !fFirstMove ) {

          float arDouble[ 4 ];

          if ( GetDPEq ( NULL, NULL, &ci ) ) {
            
            if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput, &ci,
                                           &ecEval, ecEval.nPlies ) < 0 ) 
              return;

            pmr->n.etDouble = EVAL_EVAL;
            pmr->n.esDouble.ec = ecEval;
            for ( j = 0; j < 4; j++ ) 
              pmr->n.arDouble[ j ] = arDouble[ j ];

#if DEBUG_ANALYSIS
            GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );

            puts ( sz );
#endif
          }
          else {
#if DEBUG_ANALYSIS
            printf ( "cube not available\n" );
#endif
            pmr->n.etDouble = EVAL_NONE;

          }

        }

	if( fAnalyseDice )
	    pmr->n.rLuck = LuckAnalysis( anBoard, anDice[ 0 ], anDice[ 1 ],
					 &ci, fFirstMove );
	
        fFirstMove = 0;

        /* evaluate move */

        /* find auch for move */

        memcpy( anBoardMove, anBoard, sizeof( anBoardMove ) );
        ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
        PositionKey ( anBoardMove, auch );

	if( fAnalyseMove ) {
	    if( pmr->n.ml.cMoves )
		free( pmr->n.ml.amMoves );
	
	    /* find best moves */
	    
	    FindnSaveBestMoves ( &(pmr->n.ml), anDice[ 0 ], anDice[ 1 ],
				 anBoard, auch, &ci, &ecEval );
	    
	    for( pmr->n.iMove = 0; pmr->n.iMove < pmr->n.ml.cMoves;
		 pmr->n.iMove++ ) {
#if DEBUG_ANALYSIS
		printf( "%i %s %7.3f\n", pmr->n.iMove,
			FormatMove( sz, anBoard,
				    pmr->n.ml.amMoves[ pmr->n.iMove ].anMove ),
			pmr->n.ml.amMoves[ pmr->n.iMove ].rScore );
#endif
		if( EqualKeys( auch, pmr->n.ml.amMoves[ pmr->n.iMove ].auch ) )
		    break;
	    }

	    if( cAnalysisMoves >= 2 && pmr->n.ml.cMoves > cAnalysisMoves ) {
		/* There are more legal moves than we want; throw some away. */
		if( pmr->n.iMove >= cAnalysisMoves ) {
		    /* The move made wasn't in the top n; move it up so it
		       won't be discarded. */
		    memcpy( pmr->n.ml.amMoves + cAnalysisMoves - 1,
			    pmr->n.ml.amMoves + pmr->n.iMove, sizeof( move ) );
		    pmr->n.iMove = cAnalysisMoves - 1;
		}
		
		realloc( pmr->n.ml.amMoves, cAnalysisMoves * sizeof( move ) );
		pmr->n.ml.cMoves = cAnalysisMoves;
	    }
	
#if DEBUG_ANALYSIS
	    if( !pmr->n.ml.cMoves )
		printf ("no legal moves..\n" );
#endif
	}
	
        /* copy board back */
        memcpy ( anBoard, anBoardMove, sizeof ( anBoard ) );
        if ( ! fPlayer )
          SwapSides( anBoard );

        break;
      case MOVE_DOUBLE:

        fPlayer = pmr->d.fPlayer;

        if ( ! fPlayer )
          SwapSides( anBoard );

        /* cube action */

	if( fAnalyseCube ) {
	    SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			  anScore, fCrawfordLocal, fJacobyLocal,
			  fBeaversLocal );

	    if ( GetDPEq ( NULL, NULL, &ci ) ) {
		if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput,
					       &ci, &ecEval,
					       ecEval.nPlies ) < 0 ) 
		    return;

		pmr->d.etDouble = EVAL_EVAL;
		pmr->d.esDouble.ec = ecEval;
		for ( j = 0; j < 4; j++ ) 
		    pmr->d.arDouble[ j ] = arDouble[ j ];
#if DEBUG_ANALYSIS
		GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );
		
		puts ( sz );
#endif
	    }
	}
	
	if ( ! fPlayer )
	    SwapSides( anBoard );
#if DEBUG_ANALYSIS
	sprintf( sz, " Doubles => %d", nCube <<= 1 );
#endif
        
        nCube <<= 1;
        fCubeOwner = ! fPlayer;

        break;
      case MOVE_TAKE:

        /* Check if the previous move was a MOVE_DOUBLE */

        pmrPrev = pl->plPrev->p;

        if ( pmrPrev->mt == MOVE_DOUBLE ) {

          /* copy evaluation from the previous MOVE_DOUBLE */

          pmr->d.etDouble = pmrPrev->d.etDouble;
          pmr->d.esDouble.ec = pmrPrev->d.esDouble.ec;
          for ( j = 0; j < 4; j++ ) 
            pmr->d.arDouble[ j ] = - pmrPrev->d.arDouble[ j ];

        } 
        else {

          /* we have to perform the evaluation */

          assert ( FALSE ); /* FIXME: could this ever happen??? */

        }

        break;
      case MOVE_DROP:

        /* Check if the previous move was a MOVE_DOUBLE */

        pmrPrev = pl->plPrev->p;

        if ( pmrPrev->mt == MOVE_DOUBLE ) {

          /* copy evaluation from the previous MOVE_DOUBLE */

          pmr->d.etDouble = pmrPrev->d.etDouble;
          pmr->d.esDouble.ec = pmrPrev->d.esDouble.ec;
          for ( j = 0; j < 4; j++ ) 
            pmr->d.arDouble[ j ] = - pmrPrev->d.arDouble[ j ];

        } 
        else {

          /* we have to perform the evaluation */

          assert ( FALSE ); /* FIXME: could this ever happen??? */

        }

        anScore[ ( i + 1 ) & 1 ] += nCube / 2;
        break;
      case MOVE_RESIGN:
        /* FIXME how does JF do it? */
        /* FIXME: evaluate is resignation is OK */
        break;
      case MOVE_GAMEINFO:

        fWinner = pmr->g.fWinner;
        nPoints = pmr->g.nPoints;

        anScore[ 0 ] = pmr->g.anScore[ 0 ];
        anScore[ 1 ] = pmr->g.anScore[ 1 ];
        fCrawfordLocal = pmr->g.fCrawfordGame;
        nMatchToLocal = pmr->g.nMatch;

        if ( ! fCrawfordLocal &&
             ( nMatchToLocal - anScore[ 0 ] == 1 ||
               nMatchToLocal - anScore[ 1 ] == 1 ) )
          fPostCrawfordLocal = 1;

#if DEBUG_ANALYSIS
        if( iGame >= 0 )
          printf( " Game %d\n", iGame + 1 );

        printf ("Score:\n"
                "%s: %d\n" "%s: %d (match to %i)\n",
                ap[ 0 ].szName, anScore[ 0 ],
                ap[ 1 ].szName, anScore[ 1 ],
                nMatchToLocal );

        printf ("Crawford    : %i\n"
                "PostCrawford: %i\n", fCrawfordLocal, fPostCrawfordLocal );
#endif
    
        break;
	
      case MOVE_SETBOARD:
	  PositionFromKey( anBoard, pmr->sb.auchKey );
	  
	  if( fMove < 0 )
	      fTurn = fMove = 0;
	  
	  if( fMove )
	      SwapSides( anBoard );
	  
	  break;
	  
      case MOVE_SETDICE:
	  anDice[ 0 ] = pmr->sd.anDice[ 0 ];
	  anDice[ 1 ] = pmr->sd.anDice[ 1 ];
	  fPlayer = pmr->sd.fPlayer;
	  
	  if ( ! fPlayer )
	      SwapSides( anBoard );

	  if( fAnalyseDice )
	      pmr->sd.rLuck = LuckAnalysis( anBoard, anDice[ 0 ], anDice[ 1 ],
					    &ci, fFirstMove );

	  if ( ! fPlayer )
	      SwapSides( anBoard );
	  
	  fDoubled = FALSE;
	  break;
	  
      case MOVE_SETCUBEVAL:
	  if( fPlayer < 0 )
	      fPlayer = 0;
	  
	  nCube = pmr->scv.nCube;
	  fDoubled = FALSE;
	  break;
	  
      case MOVE_SETCUBEPOS:
	  if( fPlayer < 0 )
	      fPlayer = 0;
	  
	  fCubeOwner = pmr->scp.fCubeOwner;
	  fDoubled = FALSE;
	  break;
      }
      
      if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) i++;

      i++;
    }

    if ( fWinner != -1 )
      anScore[ fWinner ] += nPoints;

#if DEBUG_ANALYSIS
    printf ( "%s wins %i points...\n",
             ap [ fWinner ].szName, nPoints );
#endif
}

extern void CommandAnalyseGame( char *sz ) {

    if( !plGame ) {
	outputl( "No game is being played." );
	return;
    }
    
    ProgressStart( "Analysing game..." );
    
    AnalyzeGame( plGame );

    ProgressEnd();
}

extern void CommandAnalyseMatch( char *sz ) {

  int i;
  list *pl;

#if DEBUG_ANALYSIS
  printf( " %d point match\n\n", nMatchTo );
#endif
  
  ProgressStart( "Analysing match..." );
  
  for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext )
    AnalyzeGame( pl->p );

  ProgressEnd();
}

extern void CommandAnalyseSession( char *sz ) {

    CommandAnalyseMatch( sz );
}



extern void
IniStatcontext ( statcontext *psc ) {

  /* Initialize statcontext with all zeroes */

  int i, j;

  for ( i = 0; i < 2; i++ ) {

    psc->anUnforcedMoves[ i ] = 0;
    psc->anTotalMoves[ i ] = 0;
    
    psc->anTotalCube[ i ] = 0;
    psc->anDouble[ i ] = 0;
    psc->anTake[ i ] = 0;
    psc->anPass[ i ] = 0;

    for ( j = 0; j <= SKILL_VERYGOOD; j++ )
      psc->anMoves[ i ][ j ] = 0;

    for ( j = 0; j <= LUCK_VERYGOOD; j++ )
      psc->anLuck[ i ][ j ] = 0;

    psc->anCubeMissedDoubleDP [ i ] = 0;
    psc->anCubeMissedDoubleTG [ i ] = 0;
    psc->anCubeWrongDoubleDP [ i ] = 0;
    psc->anCubeWrongDoubleTG [ i ] = 0;
    psc->anCubeWrongTake [ i ] = 0;
    psc->anCubeWrongPass [ i ] = 0;

    for ( j = 0; j < 2; j++ ) {

      psc->arErrorCheckerplay [ i ][ j ] = 0.0;
      psc->arErrorMissedDoubleDP [ i ][ j ] = 0.0;
      psc->arErrorMissedDoubleTG [ i ][ j ] = 0.0;
      psc->arErrorWrongDoubleDP [ i ][ j ] = 0.0;
      psc->arErrorWrongDoubleTG [ i ][ j ] = 0.0;
      psc->arErrorWrongTake[ i ][ j ] = 0.0;
      psc->arErrorWrongPass[ i ][ j ] = 0.0;
      psc->arLuck[ i ][ j ] = 0.0;

    }

  }

}


extern void
AddStatcontext ( statcontext *pscA, statcontext *pscB ) {

  /* pscB = pscB + pscA */

  int i, j;

  for ( i = 0; i < 2; i++ ) {

    pscB->anUnforcedMoves[ i ] += pscA->anUnforcedMoves[ i ];
    pscB->anTotalMoves[ i ] += pscA->anTotalMoves[ i ];
    
    pscB->anTotalCube[ i ] += pscA->anTotalCube[ i ];
    pscB->anDouble[ i ] += pscA->anDouble[ i ];
    pscB->anTake[ i ] += pscA->anTake[ i ];
    pscB->anPass[ i ] += pscA->anPass[ i ];

    for ( j = 0; j <= SKILL_VERYGOOD; j++ )
      pscB->anMoves[ i ][ j ] += pscA->anMoves[ i ][ j ];

    for ( j = 0; j <= LUCK_VERYGOOD; j++ )
      pscB->anLuck[ i ][ j ] += pscA->anLuck[ i ][ j ];

    pscB->anCubeMissedDoubleDP [ i ] += pscA->anCubeMissedDoubleDP [ i ];
    pscB->anCubeMissedDoubleTG [ i ] += pscA->anCubeMissedDoubleTG [ i ];
    pscB->anCubeWrongDoubleDP [ i ] += pscA->anCubeWrongDoubleDP [ i ];
    pscB->anCubeWrongDoubleTG [ i ] += pscA->anCubeWrongDoubleTG [ i ];
    pscB->anCubeWrongTake [ i ] += pscA->anCubeWrongTake [ i ];
    pscB->anCubeWrongPass [ i ] += pscA->anCubeWrongPass [ i ];

    for ( j = 0; j < 2; j++ ) {

      pscB->arErrorCheckerplay [ i ][ j ] +=
        pscA->arErrorCheckerplay[ i ][ j ];
      pscB->arErrorMissedDoubleDP [ i ][ j ] +=
        pscA->arErrorMissedDoubleDP[ i ][ j ];
      pscB->arErrorMissedDoubleTG [ i ][ j ] +=
        pscA->arErrorMissedDoubleTG[ i ][ j ];
      pscB->arErrorWrongDoubleDP [ i ][ j ] +=
        pscA->arErrorWrongDoubleDP[ i ][ j ];
      pscB->arErrorWrongDoubleTG [ i ][ j ] +=
        pscA->arErrorWrongDoubleTG[ i ][ j ];
      pscB->arErrorWrongTake [ i ][ j ] +=
        pscA->arErrorWrongTake[ i ][ j ];
      pscB->arErrorWrongPass [ i ][ j ] +=
        pscA->arErrorWrongPass [ i ][ j ];
      pscB->arLuck [ i ][ j ] +=
        pscA->arLuck [ i ][ j ];

    }

  }

}


extern int
ComputeStatGame ( list *plGame, statcontext *pscGame ) {

  /* Gather statistics about this game */

  /* FIXME: move threshold to struct */

#define THRS_LUCK_VERYGOOD 0.5
#define THRS_LUCK_GOOD     0.25
#define THRS_LUCK_BAD     -0.25
#define THRS_LUCK_VERYBAD -0.50

#define THRS_SKILL_VERYGOOD    0.005
#define THRS_SKILL_GOOD        0.010
#define THRS_SKILL_INTERESTING 0.020
#define THRS_SKILL_NONE        0.030
#define THRS_SKILL_DOUBTFUL    0.040
#define THRS_SKILL_BAD         0.080


  list *pl;
  moverecord *pmr;

  float rLuck, rND, rDT, rDP, rError, rErrorMWC;

  int i = 0, nCube = 1, anBoard[ 2 ][ 25 ];
  int fCrawfordLocal = fCrawford, fPostCrawfordLocal = fPostCrawford,
    fJacobyLocal = fJacoby, fBeaversLocal = fBeavers,
    nMatchToLocal = nMatchTo;
  int anScore[ 2 ];
  int fCubeOwner = -1, fPlayer = -1;
  int anDice[ 2 ], j;
  cubeinfo ci;
  int fWinner, nPoints;

  lucktype lt;
  skilltype st;

  int fComplete = 1;

  IniStatcontext ( pscGame );
  
  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
    pmr = pl->p;

    switch( pmr->mt ) {
    case MOVE_NORMAL:

      fPlayer = pmr->n.fPlayer;

      /*
       * cube action statistics 
       */


      if ( pmr->n.etDouble != EVAL_NONE ) {

        rND = pmr->n.arDouble[ 1 ];
        rDT = pmr->n.arDouble[ 2 ];
        rDP = pmr->n.arDouble[ 3 ];

        pscGame->anTotalCube[ fPlayer ]++;

        if ( rDP >= rND && rDT >= rND ) {

          /* it was a double */

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          /* error for not doubling */

          if ( rDT >= rDP )
            rError = rDP - rND; /* pass */
          else
            rError = rDT - rND; /* take */

          rErrorMWC = 
            ( nMatchToLocal ) ?
            eq2mwc ( rError, &ci ) - eq2mwc ( 0.0f, &ci ) :
            nCube * rError;

          if ( rND >= 0.950 ) {

            /* around too good point */

            pscGame->anCubeMissedDoubleTG[ fPlayer ]++;
            pscGame->arErrorMissedDoubleTG[ fPlayer ][ 0 ] += rError;
            pscGame->arErrorMissedDoubleTG[ fPlayer ][ 1 ] +=
              rErrorMWC;

          }
          else {

            /* around double point */

            pscGame->anCubeMissedDoubleDP[ fPlayer ]++;
            pscGame->arErrorMissedDoubleDP[ fPlayer ][ 0 ] += rError;
            pscGame->arErrorMissedDoubleDP[ fPlayer ][ 1 ] +=
              rErrorMWC;

          }

        } /* it was a double */

      } /* cube action stat */


      /* 
       * checkerplay statistics 
       */


      pscGame->anTotalMoves[ fPlayer ]++;

      if ( pmr->n.ml.amMoves ) {

        if ( pmr->n.ml.cMoves > 1 ) {

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
                        anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          pscGame->anUnforcedMoves[ fPlayer ]++;

          rError =
            pmr->n.ml.amMoves[ 0 ].rScore - 
            pmr->n.ml.amMoves[ pmr->n.iMove ].rScore;

          rErrorMWC =
            ( nMatchToLocal ) ?
            eq2mwc ( pmr->n.ml.amMoves[ 0 ].rScore, &ci ) - 
            eq2mwc ( pmr->n.ml.amMoves[ pmr->n.iMove ].rScore, &ci ) :
            nCube * rError;

          if ( rError < THRS_SKILL_VERYGOOD )
            st = SKILL_VERYGOOD;
          else if ( rError < THRS_SKILL_GOOD )
            st = SKILL_GOOD;
          else if ( rError < THRS_SKILL_INTERESTING )
            st = SKILL_INTERESTING;
          else if ( rError < THRS_SKILL_NONE )
            st = SKILL_NONE;
          else if ( rError < THRS_SKILL_DOUBTFUL )
            st = SKILL_DOUBTFUL;
          else if ( rError < THRS_SKILL_BAD )
            st = SKILL_BAD;
          else
            st = SKILL_VERYBAD;

          pscGame->anMoves[ fPlayer ][ st ]++;

          pscGame->arErrorCheckerplay[ fPlayer ][ 0 ] += rError;
          pscGame->arErrorCheckerplay[ fPlayer ][ 1 ] += rErrorMWC;

        }

      } 
      else {

        fComplete = 0;

      } /* checkerplay stat */


      /* Luck statistics */

      rLuck = pmr->n.rLuck;

      if ( rLuck != -HUGE_VALF ) {

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
		      anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

        pscGame->arLuck[ fPlayer ][ 0 ] += rLuck;
        pscGame->arLuck[ fPlayer ][ 1 ] +=
          ( nMatchToLocal ) ?
          eq2mwc ( rLuck, &ci ) - eq2mwc ( 0.0f, &ci ) :
          nCube * rLuck;


        if ( rLuck >= THRS_LUCK_VERYGOOD )
          lt = LUCK_VERYGOOD;
        else if ( rLuck >= THRS_LUCK_GOOD )
          lt = LUCK_GOOD;
        else if ( rLuck >= THRS_LUCK_BAD )
          lt = LUCK_NONE;
        else if ( rLuck >= THRS_LUCK_VERYBAD )
          lt = LUCK_BAD;
        else
          lt = LUCK_VERYBAD;

        pscGame->anLuck[ fPlayer ][ lt ]++;

      } /* luck stat */

      break; /* MOVE_NORMAL */

    case MOVE_DOUBLE:

      fPlayer = pmr->d.fPlayer;

      /*
       * cube action statistics 
       */

      if ( pmr->d.etDouble != EVAL_NONE ) {

        rND = pmr->d.arDouble[ 1 ];
        rDT = pmr->d.arDouble[ 2 ];
        rDP = pmr->d.arDouble[ 3 ];

        pscGame->anTotalCube[ fPlayer ]++;
        pscGame->anDouble[ fPlayer ]++;

        if ( rDP < rND && rDT < rND ) {

          /* it was not a double */

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          /* error for doubling */

          if ( rND >= rDP )
            rError = rND - rDP; /* pass */
          else
            rError = rND - rDT; /* take */

          rErrorMWC = 
            ( nMatchToLocal ) ?
            eq2mwc ( rError, &ci ) - eq2mwc ( 0.0f, &ci ) :
            nCube * rError;


          if ( rND >= 0.950 ) {

            /* around too good point */

            pscGame->anCubeWrongDoubleTG[ fPlayer ]++;
            pscGame->arErrorWrongDoubleTG[ fPlayer ][ 0 ] += rError;
            pscGame->arErrorWrongDoubleTG[ fPlayer ][ 1 ] +=
              rErrorMWC;

          }
          else {

            /* around double point */

            pscGame->anCubeWrongDoubleDP[ fPlayer ]++;
            pscGame->arErrorWrongDoubleDP[ fPlayer ][ 0 ] += rError;
            pscGame->arErrorWrongDoubleDP[ fPlayer ][ 1 ] +=
              rErrorMWC;

          }

        } /* it was not a double */

      } 
      else 
        fComplete = 0;

      fCubeOwner = ! fPlayer;

      break; /* MOVE_DOUBLE */

    case MOVE_TAKE:

      fPlayer = pmr->d.fPlayer;

      /*
       * cube action statistics 
       */

      if ( pmr->d.etDouble != EVAL_NONE ) {

        rND = pmr->d.arDouble[ 1 ];
        rDT = pmr->d.arDouble[ 2 ];
        rDP = pmr->d.arDouble[ 3 ];

        pscGame->anTotalCube[ fPlayer ]++;
        pscGame->anTake[ fPlayer ]++;

        if ( rDP > rDT ) {

          /* it was not a take */

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          /* error for doubling */

          rError = rDP - rDT; /* pass */

          rErrorMWC = 
            ( nMatchToLocal ) ?
            eq2mwc ( rError, &ci ) - eq2mwc ( 0.0f, &ci ) :
            nCube * rError;

          pscGame->anCubeWrongTake[ fPlayer ]++;
          pscGame->arErrorWrongTake[ fPlayer ][ 0 ] += rError;
          pscGame->arErrorWrongTake[ fPlayer ][ 1 ] += rErrorMWC;

        } /* it was not a take */

      }
      else
        fComplete = 0;

      break; /* MOVE_TAKE */


    case MOVE_DROP:

      fPlayer = pmr->d.fPlayer;

      /*
       * cube action statistics 
       */

      if ( pmr->d.etDouble != EVAL_NONE ) {

        rND = pmr->d.arDouble[ 1 ];
        rDT = pmr->d.arDouble[ 2 ];
        rDP = pmr->d.arDouble[ 3 ];

        pscGame->anTotalCube[ fPlayer ]++;
        pscGame->anTake[ fPlayer ]++;

        if ( rDP < rDT ) {

          /* it was a take */

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          /* error for dropping */

          rError = rDT - rDP;

          rErrorMWC = 
            ( nMatchToLocal ) ?
            eq2mwc ( rError, &ci ) - eq2mwc ( 0.0f, &ci ) :
            nCube * rError;

          pscGame->anCubeWrongPass[ fPlayer ]++;
          pscGame->arErrorWrongPass[ fPlayer ][ 0 ] += rError;
          pscGame->arErrorWrongPass[ fPlayer ][ 1 ] += rErrorMWC;

        } /* it was a take */

      }
      else
        fComplete = 0;

      anScore[ ( i + 1 ) & 1 ] += nCube / 2;

      break; /* MOVE_DROP */

    case MOVE_RESIGN:

      /* fixme */

      break;

    case MOVE_GAMEINFO:

        fWinner = pmr->g.fWinner;
        nPoints = pmr->g.nPoints;

        anScore[ 0 ] = pmr->g.anScore[ 0 ];
        anScore[ 1 ] = pmr->g.anScore[ 1 ];
        fCrawfordLocal = pmr->g.fCrawfordGame;
        nMatchToLocal = pmr->g.nMatch;

        if ( ! fCrawfordLocal &&
             ( nMatchToLocal - anScore[ 0 ] == 1 ||
               nMatchToLocal - anScore[ 1 ] == 1 ) )
          fPostCrawfordLocal = 1;

        break;

    case MOVE_SETBOARD:

      if( fMove < 0 )
        fTurn = fMove = 0;

      break;

      /* FIXME: perhaps save luck stats??? */

    case MOVE_SETDICE:

      anDice[ 0 ] = pmr->sd.anDice[ 0 ];
      anDice[ 1 ] = pmr->sd.anDice[ 1 ];
      fPlayer = pmr->sd.fPlayer;
      fDoubled = FALSE;

      /* FIXME: perhaps save luck stats??? */

      break;
      
    case MOVE_SETCUBEVAL:
      if( fPlayer < 0 )
        fPlayer = 0;
	  
      nCube = pmr->scv.nCube;
      fDoubled = FALSE;

      /* FIXME: perhaps save luck stats??? */

      break;
	  
    case MOVE_SETCUBEPOS:
      if( fPlayer < 0 )
        fPlayer = 0;
	  
      fCubeOwner = pmr->scp.fCubeOwner;
      fDoubled = FALSE;

      /* FIXME: perhaps save luck stats??? */

      break;
    
    } /* switch */
      
    if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) i++;

    i++;
  } /* move */

  if ( fWinner != -1 )
    anScore[ fWinner ] += nPoints;


  return fComplete;

}

extern void
DumpStatcontext ( statcontext *psc, int fCompleteAnalysis,
                  char * sz ) {

  int i;
  ratingtype rt[ 2 ];

#if 0

  /* dump the contents of statcontext to stdin,
     mainly for debugging */

#define PAN( a, b )   printf ( "%s[%1i] = %i\n", # a, b, a [ b ] )
#define PAR( a, b )   printf ( "%s[%1i] = %f\n", # a, b, a [ b ] )
#define PAAN( a, b, c )   printf ( "%s[%1i,%1i] = %i\n", # a, b, c, a [ b ][ c ] )
#define PAAR( a, b, c )   printf ( "%s[%1i,%1i] = %f\n", # a, b, c, a [ b ][ c ] )
  
  int j;

  for ( i = 0; i < 2; i++ ) {

    
    PAN ( psc->anUnforcedMoves, i );
    PAN ( psc->anTotalMoves, i );
    
    PAN ( psc->anTotalCube, i );
    PAN ( psc->anDouble, i );
    PAN ( psc->anTake, i );
    PAN ( psc->anPass, i );

    for ( j = 0; j <= SKILL_VERYGOOD; j++ )
      PAAN ( psc->anMoves, i, j );

    for ( j = 0; j <= LUCK_VERYGOOD; j++ )
      PAAN ( psc->anLuck, i, j );

    PAN ( psc->anCubeMissedDoubleDP, i );
    PAN ( psc->anCubeMissedDoubleTG, i );
    PAN ( psc->anCubeWrongDoubleDP, i );
    PAN ( psc->anCubeWrongDoubleTG, i );
    PAN ( psc->anCubeWrongTake, i );
    PAN ( psc->anCubeWrongPass, i );

    for ( j = 0; j < 2; j++ ) {

      PAAR ( psc->arErrorCheckerplay, i, j );
      PAAR ( psc->arErrorMissedDoubleDP, i, j );
      PAAR ( psc->arErrorMissedDoubleTG, i, j );
      PAAR ( psc->arErrorWrongDoubleDP, i, j );
      PAAR ( psc->arErrorWrongDoubleTG, i, j );
      PAAR ( psc->arErrorWrongTake, i, j );
      PAAR ( psc->arErrorWrongPass, i, j );
      PAAR ( psc->arLuck, i, j );

    }

  }

#endif

  /* nice human readable dump */

  /* FIXME: make GTK+ version */
  /* FIXME: make tty output shorter */
  /* FIXME: the code below is only for match play */
  /* FIXME: honour fOutputMWC etc. */
  /* FIXME: calculate ratings (ET, World class, etc.) */

  printf ( "Player\t\t\t\t%-15s\t\t%-15s\n\n"
           "Checkerplay statistics:\n\n"
           "Total moves:\t\t\t%3d\t\t\t%3d\n"
           "Unforced moves:\t\t\t%3d\t\t\t%3d\n\n"
           "Moves rated perfect\t\t%3d\t\t\t%3d\n"
           "Moves rated very good\t\t%3d\t\t\t%3d\n"
           "Moves rated good\t\t%3d\t\t\t%3d\n"
           "Moves rated small error\t\t%3d\t\t\t%3d\n"
           "Moves rated error\t\t%3d\t\t\t%3d\n"
           "Moves rated blunder\t\t%3d\t\t\t%3d\n"
           "Moves rated Zzz Zzz\t\t%3d\t\t\t%3d\n\n",
           ap[ 0 ].szName, ap [ 1 ].szName,
           psc->anTotalMoves[ 0 ], psc->anTotalMoves[ 1 ],
           psc->anUnforcedMoves[ 0 ], psc->anUnforcedMoves[ 1 ],
           psc->anMoves[ 0 ][ SKILL_VERYGOOD ],
           psc->anMoves[ 1 ][ SKILL_VERYGOOD ],
           psc->anMoves[ 0 ][ SKILL_GOOD ],
           psc->anMoves[ 1 ][ SKILL_GOOD ],
           psc->anMoves[ 0 ][ SKILL_INTERESTING ],
           psc->anMoves[ 1 ][ SKILL_INTERESTING ],
           psc->anMoves[ 0 ][ SKILL_NONE ],
           psc->anMoves[ 1 ][ SKILL_NONE ],
           psc->anMoves[ 0 ][ SKILL_DOUBTFUL ],
           psc->anMoves[ 1 ][ SKILL_DOUBTFUL ],
           psc->anMoves[ 0 ][ SKILL_BAD ],
           psc->anMoves[ 1 ][ SKILL_BAD ],
           psc->anMoves[ 0 ][ SKILL_VERYBAD ],
           psc->anMoves[ 1 ][ SKILL_VERYBAD ] );

  if ( nMatchTo )
    printf ("Error rate (total)\t\t"
            "%+6.3f (%+7.3f%%)\t%+6.3f (%+7.3f%%)\n"
            "Error rate (pr. move)\t\t"
            "%+6.3f (%+7.3f%%)\t%+6.3f (%+7.3f%%)\n\n",
            psc->arErrorCheckerplay[ 0 ][ 0 ],
            psc->arErrorCheckerplay[ 0 ][ 1 ] * 100.0f,
            psc->arErrorCheckerplay[ 1 ][ 0 ],
            psc->arErrorCheckerplay[ 1 ][ 1 ] * 100.0f,
            psc->arErrorCheckerplay[ 0 ][ 0 ] /
            psc->anUnforcedMoves[ 0 ],
            psc->arErrorCheckerplay[ 0 ][ 1 ] * 100.0f /
            psc->anUnforcedMoves[ 0 ],
            psc->arErrorCheckerplay[ 1 ][ 0 ] /
            psc->anUnforcedMoves[ 1 ],
            psc->arErrorCheckerplay[ 1 ][ 1 ] * 100.0f /
            psc->anUnforcedMoves[ 1 ] );
  else
    printf ("Error rate (total)\t\t"
            "%+6.3f (%+7.3f%)\t%+6.3f (%+7.3f%)\n"
            "Error rate (pr. move)\t\t"
            "%+6.3f (%+7.3f%)\t%+6.3f (%+7.3f%)\n\n",
            psc->arErrorCheckerplay[ 0 ][ 0 ],
            psc->arErrorCheckerplay[ 0 ][ 1 ],
            psc->arErrorCheckerplay[ 1 ][ 0 ],
            psc->arErrorCheckerplay[ 1 ][ 1 ],
            psc->arErrorCheckerplay[ 0 ][ 0 ] /
            psc->anUnforcedMoves[ 0 ],
            psc->arErrorCheckerplay[ 0 ][ 1 ] /
            psc->anUnforcedMoves[ 0 ],
            psc->arErrorCheckerplay[ 1 ][ 0 ] /
            psc->anUnforcedMoves[ 1 ],
            psc->arErrorCheckerplay[ 1 ][ 1 ] /
            psc->anUnforcedMoves[ 1 ] );

  printf ( "Super-jokers\t\t\t%3d\t\t\t%3d\n"
           "Jokers\t\t\t\t%3d\t\t\t%3d\n"
           "Average\t\t\t\t%3d\t\t\t%3d\n"
           "Anti-jokers\t\t\t%3d\t\t\t%3d\n"
           "Super anti-jokers\t\t%3d\t\t\t%3d\n",
           psc->anLuck[ 0 ][ LUCK_VERYGOOD ],
           psc->anLuck[ 1 ][ LUCK_VERYGOOD ],
           psc->anLuck[ 0 ][ LUCK_GOOD ],
           psc->anLuck[ 1 ][ LUCK_GOOD ],
           psc->anLuck[ 0 ][ LUCK_NONE ],
           psc->anLuck[ 1 ][ LUCK_NONE ],
           psc->anLuck[ 0 ][ LUCK_BAD ],
           psc->anLuck[ 1 ][ LUCK_BAD ],
           psc->anLuck[ 0 ][ LUCK_VERYBAD ],
           psc->anLuck[ 1 ][ LUCK_VERYBAD ] );
           

  if ( nMatchTo )
    printf ("Luck rate (total)\t\t"
            "%+6.3f (%+7.3f%%)\t%+6.3f (%+7.3f%%)\n"
            "Luck rate (pr. move)\t\t"
            "%+6.3f (%+7.3f%%)\t%+6.3f (%+7.3f%%)\n\n",
            psc->arLuck[ 0 ][ 0 ],
            psc->arLuck[ 0 ][ 1 ] * 100.0f,
            psc->arLuck[ 1 ][ 0 ],
            psc->arLuck[ 1 ][ 1 ] * 100.0f,
            psc->arLuck[ 0 ][ 0 ] /
            psc->anTotalMoves[ 0 ],
            psc->arLuck[ 0 ][ 1 ] * 100.0f /
            psc->anTotalMoves[ 0 ],
            psc->arLuck[ 1 ][ 0 ] /
            psc->anTotalMoves[ 1 ],
            psc->arLuck[ 1 ][ 1 ] * 100.0f /
            psc->anTotalMoves[ 1 ] );
  else
    printf ("Luck rate (total)\t\t"
            "%+6.3f (%+7.3f%)\t%+6.3f (%+7.3f%)\n"
            "Luck rate (pr. move)\t\t"
            "%+6.3f (%+7.3f%)\t%+6.3f (%+7.3f%)\n\n",
            psc->arLuck[ 0 ][ 0 ],
            psc->arLuck[ 0 ][ 1 ],
            psc->arLuck[ 1 ][ 0 ],
            psc->arLuck[ 1 ][ 1 ],
            psc->arLuck[ 0 ][ 0 ] /
            psc->anTotalMoves[ 0 ],
            psc->arLuck[ 0 ][ 1 ] /
            psc->anTotalMoves[ 0 ],
            psc->arLuck[ 1 ][ 0 ] /
            psc->anTotalMoves[ 1 ],
            psc->arLuck[ 1 ][ 1 ] /
            psc->anTotalMoves[ 1 ] );

  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( psc->arErrorCheckerplay[ i ][ 0 ] /
                          psc->anUnforcedMoves[ i ] );

  printf ( "Checker play rating:\t\t%-15s\t\t%-15s\n\n",
           aszRating[ rt [ 0 ] ], aszRating[ rt [ 1 ] ] );

  printf ( "\nCube decisions statistics:\n\n" );

  printf ( 
           "Total cube decisions\t\t%3d\t\t\t%3d\n"
           "Doubles\t\t\t\t%3d\t\t\t%3d\n"
           "Takes\t\t\t\t%3d\t\t\t%3d\n"
           "Pass\t\t\t\t%3d\t\t\t%3d\n\n",
           psc->anTotalCube[ 0 ],
           psc->anTotalCube[ 1 ],
           psc->anDouble[ 0 ], 
           psc->anDouble[ 1 ], 
           psc->anTake[ 0 ],
           psc->anTake[ 1 ],
           psc->anPass[ 0 ], 
           psc->anPass[ 1 ] );

  if ( nMatchTo )
    printf ("Missed doubles around DP\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Missed doubles around TG\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong doubles around DP\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong doubles around TG\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong takes\t\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong passes\t\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n",
            psc->anCubeMissedDoubleDP[ 0 ],
            psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
            psc->arErrorMissedDoubleDP[ 0 ][ 1 ] * 100.0f,
            psc->anCubeMissedDoubleDP[ 1 ],
            psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
            psc->arErrorMissedDoubleDP[ 1 ][ 1 ] * 100.0f,
            psc->anCubeMissedDoubleTG[ 0 ],
            psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
            psc->arErrorMissedDoubleTG[ 0 ][ 1 ] * 100.0f,
            psc->anCubeMissedDoubleTG[ 1 ],
            psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
            psc->arErrorMissedDoubleTG[ 1 ][ 1 ] * 100.0f,
            psc->anCubeWrongDoubleDP[ 0 ],
            psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
            psc->arErrorWrongDoubleDP[ 0 ][ 1 ] * 100.0f,
            psc->anCubeWrongDoubleDP[ 1 ],
            psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
            psc->arErrorWrongDoubleDP[ 1 ][ 1 ] * 100.0f,
            psc->anCubeWrongDoubleTG[ 0 ],
            psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
            psc->arErrorWrongDoubleTG[ 0 ][ 1 ] * 100.0f,
            psc->anCubeWrongDoubleTG[ 1 ],
            psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
            psc->arErrorWrongDoubleTG[ 1 ][ 1 ] * 100.0f,
            psc->anCubeWrongTake[ 0 ],
            psc->arErrorWrongTake[ 0 ][ 0 ],
            psc->arErrorWrongTake[ 0 ][ 1 ] * 100.0f,
            psc->anCubeWrongTake[ 1 ],
            psc->arErrorWrongTake[ 1 ][ 0 ],
            psc->arErrorWrongTake[ 1 ][ 1 ] * 100.0f,
            psc->anCubeWrongPass[ 0 ],
            psc->arErrorWrongPass[ 0 ][ 0 ],
            psc->arErrorWrongPass[ 0 ][ 1 ] * 100.0f,
            psc->anCubeWrongPass[ 1 ],
            psc->arErrorWrongPass[ 1 ][ 0 ],
            psc->arErrorWrongPass[ 1 ][ 1 ] * 100.0f );
  else
    printf ("Missed doubles around DP\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Missed doubles around TG\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong doubles around DP\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong doubles around TG\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong takes\t\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n"
            "Wrong passes\t\t\t"
            "%3d (%+6.3f (%+7.3f%%)\t%3d (%+6.3f (%+7.3f%%)\n",
            psc->anCubeMissedDoubleDP[ 0 ],
            psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
            psc->arErrorMissedDoubleDP[ 0 ][ 1 ],
            psc->anCubeMissedDoubleDP[ 1 ],
            psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
            psc->arErrorMissedDoubleDP[ 1 ][ 1 ],
            psc->anCubeMissedDoubleTG[ 0 ],
            psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
            psc->arErrorMissedDoubleTG[ 0 ][ 1 ],
            psc->anCubeMissedDoubleTG[ 1 ],
            psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
            psc->arErrorMissedDoubleTG[ 1 ][ 1 ],
            psc->anCubeWrongDoubleDP[ 0 ],
            psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
            psc->arErrorWrongDoubleDP[ 0 ][ 1 ],
            psc->anCubeWrongDoubleDP[ 1 ],
            psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
            psc->arErrorWrongDoubleDP[ 1 ][ 1 ],
            psc->anCubeWrongDoubleTG[ 0 ],
            psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
            psc->arErrorWrongDoubleTG[ 0 ][ 1 ],
            psc->anCubeWrongDoubleTG[ 1 ],
            psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
            psc->arErrorWrongDoubleTG[ 1 ][ 1 ],
            psc->anCubeWrongTake[ 0 ],
            psc->arErrorWrongTake[ 0 ][ 0 ],
            psc->arErrorWrongTake[ 0 ][ 1 ],
            psc->anCubeWrongTake[ 1 ],
            psc->arErrorWrongTake[ 1 ][ 0 ],
            psc->arErrorWrongTake[ 1 ][ 1 ],
            psc->anCubeWrongPass[ 0 ],
            psc->arErrorWrongPass[ 0 ][ 0 ],
            psc->arErrorWrongPass[ 0 ][ 1 ],
            psc->anCubeWrongPass[ 1 ],
            psc->arErrorWrongPass[ 1 ][ 0 ],
            psc->arErrorWrongPass[ 1 ][ 1 ] );

  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                            + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                            + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongTake[ i ][ 0 ]
                            + psc->arErrorWrongPass[ i ][ 0 ] ) /
                          psc->anTotalCube[ i ] );

  printf ( "\nCube decision rating:\t\t%-15s\t%-15s\n\n",
           aszRating[ rt [ 0 ] ], aszRating[ rt [ 1 ] ] );

  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                            + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                            + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongTake[ i ][ 0 ]
                            + psc->arErrorWrongPass[ i ][ 0 ] ) /
                          psc->anTotalCube[ i ] +
                          psc->arErrorCheckerplay[ i ][ 0 ] /
                          psc->anUnforcedMoves[ i ] );

  printf ( "Overall rating:\t\t\t%-15s\t\t%-15s\n\n",
           aszRating[ rt [ 0 ] ], aszRating[ rt [ 1 ] ] );

  if ( ! fCompleteAnalysis )
    printf ( "\nWarning: due to incomplete analysis"
             " the results may be incomplete!\n\n" );

}


extern void
CommandShowStatisticsMatch ( char *sz ) {

  statcontext scStatMatch;
  int fCompleteAnalysis;

  StatMatch ( &scStatMatch, &fCompleteAnalysis );

#if USE_GTK && 0
  if ( fX ) {
    GTKDumpStatcontext ( &scStatMatch, fCompleteAnalysis,
                         "Statistics for all games" );
    return;
  }
#endif

  DumpStatcontext ( &scStatMatch, fCompleteAnalysis,
                    "Statistics for all games");

}


extern void
CommandShowStatisticsSession ( char *sz ) {

  CommandShowStatisticsMatch ( sz );

}


extern void
CommandShowStatisticsGame ( char *sz ) {

  statcontext scStatGame;
  int fCompleteAnalysis;

  if( !plGame ) {
    outputl( "No game is being played." );
    return;
  }
    
  StatGame ( &scStatGame, &fCompleteAnalysis );

#if USE_GTK && 0
  if ( fX ) {
    GTKDumpStatcontext ( &scStatGame, fCompleteAnalysis,
                         "Statistics for current game" );
    return;
  }
#endif

  DumpStatcontext ( &scStatGame, fCompleteAnalysis,
                    "Statistics for current game");

}
  

extern int
StatMatch ( statcontext *pscStatMatch, int *pfCompleteAnalysis ) {


  list *pl;
  int i;
  statcontext sc;

  *pfCompleteAnalysis = TRUE;

  IniStatcontext ( pscStatMatch );
  
  for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext ) {   

    *pfCompleteAnalysis =
      ComputeStatGame( pl->p, &sc ) && *pfCompleteAnalysis;

    AddStatcontext ( &sc, pscStatMatch );

  }

}


extern int
StatGame ( statcontext *pscStatGame, int *pfCompleteAnalysis ) {

  *pfCompleteAnalysis = ComputeStatGame ( plGame, pscStatGame );

}


extern ratingtype
GetRating ( const float rError ) {

  int i;

  for ( i = RAT_EXTRA_TERRESTRIAL; i >= 0; i-- )
    if ( rError <= arThrsRating[ i ] ) return i;

  return RAT_BEGINNER;

}
