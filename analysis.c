/*
 * analysis.c
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
#include <math.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "positionid.h"
#include "rollout.h"
#include "analysis.h"

#include "i18n.h"

const char *aszRating [ RAT_UNDEFINED + 1 ] = {
  N_("Awful!"), 
  N_("Beginner"), 
  N_("Casual player"), 
  N_("Intermediate"), 
  N_("Advanced"), 
  N_("Expert"),
  N_("World class"), 
  N_("Supernatural"), 
  N_("N/A") };

const char *aszLuckRating[] = {
  N_("Haaa-haaa"),
  N_("Go to bed"), 
  N_("Better luck next time"),
  N_("None"),
  N_("Good dice, man!"),
  N_("Go to Las Vegas immediately"),
  N_("Cheater :-)")
};

static const float arThrsRating [ RAT_SUPERNATURAL + 1 ] = {
  1e38, 0.060, 0.030, 0.025, 0.020, 0.015, 0.010, 0.005 };

evalcontext ecLuck = { TRUE, 0, 0, TRUE, 0.0 };

extern ratingtype
GetRating ( const float rError ) {

  int i;

  for ( i = RAT_SUPERNATURAL; i >= 0; i-- )
    if ( rError < arThrsRating[ i ] ) return i;

  return RAT_UNDEFINED;
}

static float
LuckFirst ( int anBoard[ 2 ][ 25 ], const int n0, const int n1,
            const cubeinfo *pci, const evalcontext *pec ) {

  int anBoardTemp[ 2 ][ 25 ], i, j;
  float aar[ 6 ][ 6 ], ar[ NUM_ROLLOUT_OUTPUTS ], rMean = 0.0f;
  cubeinfo ciOpp;
  movelist ml;
  
  /* first with player pci->fMove on roll */

  memcpy ( &ciOpp, pci, sizeof ( cubeinfo ) );
  ciOpp.fMove = ! pci->fMove;
  
  for( i = 0; i < 6; i++ )
    for( j = 0; j < i; j++ ) {
      memcpy( &anBoardTemp[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
              2 * 25 * sizeof( int ) );
      
      /* Find the best move for each roll at ply 0 only. */
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL,
                              (cubeinfo *) pci, (evalcontext *) pec, 
                              defaultFilters ) < 0 )
        return ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, &ciOpp, 
                                  (evalcontext *) pec ) < 0 )
          return ERR_VAL;

        if ( pec->fCubeful ) {
          if ( pci->nMatchTo )
            aar[ i ][ j ] = - mwc2eq ( ar[ OUTPUT_CUBEFUL_EQUITY ], &ciOpp );
          else
            aar[ i ][ j ] = - ar[ OUTPUT_CUBEFUL_EQUITY ];
        }
        else
          aar[ i ][ j ] = - ar[ OUTPUT_EQUITY ];

      }
      else {
        aar[ i ][ j ] = ml.amMoves[ 0 ].rScore;
        free ( ml.amMoves );
      }

      rMean += aar[ i ][ j ];

    }

  /* with other player on roll */
  
  for( i = 0; i < 6; i++ )
    for( j = i + 1; j < 6; j++ ) {
      memcpy( &anBoardTemp[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
              2 * 25 * sizeof( int ) );
      SwapSides ( anBoardTemp );
      
      /* Find the best move for each roll at ply 0 only. */
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL,
                              (cubeinfo *) pci, (evalcontext *) pec, 
                              defaultFilters ) < 0 )
        return ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, &ciOpp, 
                                  (evalcontext *) pec ) < 0 )
          return ERR_VAL;

        if ( pec->fCubeful ) {
          if ( pci->nMatchTo )
            aar[ i ][ j ] = mwc2eq ( ar[ OUTPUT_CUBEFUL_EQUITY ], &ciOpp );
          else
            aar[ i ][ j ] = ar[ OUTPUT_CUBEFUL_EQUITY ];
        }
        else
          aar[ i ][ j ] = ar[ OUTPUT_EQUITY ];

      }
      else {
        aar[ i ][ j ] = - ml.amMoves[ 0 ].rScore;
        free ( ml.amMoves );
      }

      rMean += aar[ i ][ j ];

    }

  return aar[ n0 ][ n1 ] - rMean / 30.0f;

}

static float
LuckNormal ( int anBoard[ 2 ][ 25 ], const int n0, const int n1,
             const cubeinfo *pci, const evalcontext *pec ) {
  
  int anBoardTemp[ 2 ][ 25 ], i, j;
  float aar[ 6 ][ 6 ], ar[ NUM_OUTPUTS ], rMean = 0.0f;
  cubeinfo ciOpp;
  movelist ml;

  memcpy ( &ciOpp, pci, sizeof ( cubeinfo ) );
  ciOpp.fMove = ! pci->fMove;
  
  for( i = 0; i < 6; i++ )
    for( j = 0; j <= i; j++ ) {
      memcpy( &anBoardTemp[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
              2 * 25 * sizeof( int ) );
      
      /* Find the best move for each roll at ply 0 only. */
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL,
                              (cubeinfo *) pci, (evalcontext *) pec, 
                              defaultFilters ) < 0 )
        return ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, &ciOpp, 
                                  (evalcontext *) pec ) < 0 )
          return ERR_VAL;

        if ( pec->fCubeful ) {
          if ( pci->nMatchTo )
            aar[ i ][ j ] = - mwc2eq ( ar[ OUTPUT_CUBEFUL_EQUITY ], &ciOpp );
          else
            aar[ i ][ j ] = - ar[ OUTPUT_CUBEFUL_EQUITY ];
        }
        else
          aar[ i ][ j ] = - ar[ OUTPUT_EQUITY ];

      }
      else {
        aar[ i ][ j ] = ml.amMoves[ 0 ].rScore;
        free ( ml.amMoves );
      }

      rMean += ( i == j ) ? aar[ i ][ j ] : aar[ i ][j ] * 2.0f;

    }

  return aar[ n0 ][ n1 ] - rMean / 36.0f;

}

static float LuckAnalysis( int anBoard[ 2 ][ 25 ], int n0, int n1,
			   cubeinfo *pci, int fFirstMove ) {

  if( n0-- < n1-- )
    swap( &n0, &n1 );
    
  if ( fFirstMove && n0 != n1 )
    return LuckFirst ( anBoard, n0, n1, pci, &ecLuck );
  else
    return LuckNormal ( anBoard, n0, n1, pci, &ecLuck );

}
  



static lucktype Luck( float r ) {

    if( r >= arLuckLevel[ LUCK_VERYGOOD ] )
	return LUCK_VERYGOOD;
    else if( r >= arLuckLevel[ LUCK_GOOD ] )
	return LUCK_GOOD;
    else if( r <= -arLuckLevel[ LUCK_VERYBAD ] )
	return LUCK_VERYBAD;
    else if( r <= -arLuckLevel[ LUCK_BAD ] )
	return LUCK_BAD;
    else
	return LUCK_NONE;
}

extern skilltype Skill( float r ) {
    /* FIXME if the move is correct according to the selected evaluator
       but incorrect according to 0-ply, then return SKILL_GOOD or
       SKILL_VERYGOOD */

    if( r <= -arSkillLevel[ SKILL_VERYBAD ] )
	return SKILL_VERYBAD;
    else if( r <= -arSkillLevel[ SKILL_BAD ] )
	return SKILL_BAD;
    else if( r <= -arSkillLevel[ SKILL_DOUBTFUL ] )
	return SKILL_DOUBTFUL;
    else
	return SKILL_NONE;
}

/*
 * update statcontext for given move
 *
 * Input:
 *   psc: the statcontext to update
 *   pmr: the given move
 *   pms: the current match state
 *
 * Output:
 *   psc: the updated stat context
 *
 */

static void
updateStatcontext ( statcontext *psc,
                    moverecord *pmr,
                    matchstate *pms ) {

  cubeinfo ci;
  static unsigned char auch[ 10 ];
  float rSkill, rChequerSkill, rCost;
  int i;
  int anBoardMove[ 2 ][ 25 ];

  GetMatchStateCubeInfo ( &ci, pms );

  switch ( pmr->mt ) {

  case MOVE_GAMEINFO:
    /* no-op */
    break;

  case MOVE_NORMAL:

    /* 
     * Cube analysis; check for
     *   - missed doubles
     */

    if ( pmr->n.esDouble.et != EVAL_NONE && fAnalyseCube ) {

      float *arDouble = pmr->n.arDouble;

      psc->anTotalCube[ pmr->n.fPlayer ]++;

      if ( isCloseCubedecision ( arDouble ) || 
           isMissedDouble ( arDouble, pmr->n.aarOutput, FALSE, &ci ) )
        psc->anCloseCube[ pmr->n.fPlayer ]++;
	  
      if( arDouble[ OUTPUT_NODOUBLE ] <
          arDouble[ OUTPUT_OPTIMAL ] ) {
        /* it was a double */
        rSkill = arDouble[ OUTPUT_NODOUBLE ] -
          arDouble[ OUTPUT_OPTIMAL ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
	      
        if( arDouble[ OUTPUT_NODOUBLE ] >= 0.95 ) {
          /* around too good point */
          psc->anCubeMissedDoubleTG[ pmr->n.fPlayer ]++;
          psc->arErrorMissedDoubleTG[ pmr->n.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorMissedDoubleTG[ pmr->n.fPlayer ][ 1 ] -=
            rCost;
        } 
        else {
          /* around double point */
          psc->anCubeMissedDoubleDP[ pmr->n.fPlayer ]++;
          psc->arErrorMissedDoubleDP[ pmr->n.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorMissedDoubleDP[ pmr->n.fPlayer ][ 1 ] -=
            rCost;
        }
      
      } /* missed double */	

    } /* EVAL_NONE */


    /*
     * update luck statistics for roll
     */

    if ( fAnalyseDice && pmr->n.rLuck != ERR_VAL ) {

      psc->arLuck[ pmr->n.fPlayer ][ 0 ] += pmr->n.rLuck;
      psc->arLuck[ pmr->n.fPlayer ][ 1 ] += pms->nMatchTo ?
        eq2mwc( pmr->n.rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
        pms->nCube * pmr->n.rLuck;
      
      psc->anLuck[ pmr->n.fPlayer ][ pmr->n.lt ]++;

    }


    /*
     * update chequerplay statistics 
     */

    if ( fAnalyseMove && pmr->n.esChequer.et != EVAL_NONE ) {

      /* find skill */

      memcpy( anBoardMove, pms->anBoard, sizeof( anBoardMove ) );
      ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
      PositionKey ( anBoardMove, auch );
      rChequerSkill = 0.0f;
	  
      for( i = 0; i < pmr->n.ml.cMoves; i++ ) 

        if( EqualKeys( auch,
                       pmr->n.ml.amMoves[ i ].auch ) ) {

          rChequerSkill =
            pmr->n.ml.amMoves[ i ].rScore - pmr->n.ml.amMoves[ 0 ].rScore;
          
          break;
        }

      /* update statistics */
	  
      rCost = pms->nMatchTo ? eq2mwc( rChequerSkill, &ci ) -
        eq2mwc( 0.0f, &ci ) : pms->nCube * rChequerSkill;
	  
      psc->anTotalMoves[ pmr->n.fPlayer ]++;
      psc->anMoves[ pmr->n.fPlayer ][ Skill( rChequerSkill ) ]++;
	  
      if( pmr->n.ml.cMoves > 1 ) {
        psc->anUnforcedMoves[ pmr->n.fPlayer ]++;
        psc->arErrorCheckerplay[ pmr->n.fPlayer ][ 0 ] -=
          rChequerSkill;
        psc->arErrorCheckerplay[ pmr->n.fPlayer ][ 1 ] -=
          rCost;
      }

    }

    break;

  case MOVE_DOUBLE:
 

    if ( fAnalyseCube && pmr->d.esDouble.et != EVAL_NONE ) {

      float *arDouble = pmr->d.arDouble;

      rSkill = arDouble[ OUTPUT_TAKE ] <
        arDouble[ OUTPUT_DROP ] ?
        arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
        arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];
	      
      psc->anTotalCube[ pmr->d.fPlayer ]++;
      psc->anDouble[ pmr->d.fPlayer ]++;
      psc->anCloseCube[ pmr->d.fPlayer ]++;
	      
      if( rSkill < 0.0f ) {
        /* it was not a double */
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
		  
        if( arDouble[ OUTPUT_NODOUBLE ] >= 0.95f ) {
          /* around too good point */
          psc->anCubeWrongDoubleTG[ pmr->d.fPlayer ]++;
          psc->arErrorWrongDoubleTG[ pmr->d.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorWrongDoubleTG[ pmr->d.fPlayer ][ 1 ] -=
            rCost;
        } else {
          /* around double point */
          psc->anCubeWrongDoubleDP[ pmr->d.fPlayer ]++;
          psc->arErrorWrongDoubleDP[ pmr->d.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorWrongDoubleDP[ pmr->d.fPlayer ][ 1 ] -=
            rCost;
        }
      }

    }

    break;

  case MOVE_TAKE:

    if ( fAnalyseCube && pmr->d.esDouble.et != EVAL_NONE ) {

      float *arDouble = pmr->d.arDouble;

      psc->anTotalCube[ pmr->d.fPlayer ]++;
      psc->anTake[ pmr->d.fPlayer ]++;
      psc->anCloseCube[ pmr->d.fPlayer ]++;
	  
      if( -arDouble[ OUTPUT_TAKE ] < -arDouble[ OUTPUT_DROP ] ) {

        /* it was a drop */

        rSkill = -arDouble[ OUTPUT_TAKE ] - -arDouble[ OUTPUT_DROP ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
	      
        psc->anCubeWrongTake[ pmr->d.fPlayer ]++;
        psc->arErrorWrongTake[ pmr->d.fPlayer ][ 0 ] -= rSkill;
        psc->arErrorWrongTake[ pmr->d.fPlayer ][ 1 ] -= rCost;

      }

    }

    break;

  case MOVE_DROP:

    if( fAnalyseCube && pmr->d.esDouble.et != EVAL_NONE ) {
	  
      float *arDouble = pmr->d.arDouble;

      psc->anTotalCube[ pmr->d.fPlayer ]++;
      psc->anPass[ pmr->d.fPlayer ]++;
      psc->anCloseCube[ pmr->d.fPlayer ]++;
	  
      if( -arDouble[ OUTPUT_DROP ] < -arDouble[ OUTPUT_TAKE ] ) {

        /* it was a take */
        
        rSkill = -arDouble[ OUTPUT_DROP ] - -arDouble[ OUTPUT_TAKE ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
	      
        psc->anCubeWrongPass[ pmr->d.fPlayer ]++;
        psc->arErrorWrongPass[ pmr->d.fPlayer ][ 0 ] -= rSkill;
        psc->arErrorWrongPass[ pmr->d.fPlayer ][ 1 ] -= rCost;

      }

    }

    break;


  default:

    break;

  } /* switch */

}


extern int
AnalyzeMove ( moverecord *pmr, matchstate *pms, list *plGame, statcontext *psc,
              evalsetup *pesChequer, evalsetup *pesCube,
              movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
	      int fUpdateStatistics ) {

    static int anBoardMove[ 2 ][ 25 ];
    static int fFirstMove;
    static unsigned char auch[ 10 ];
    static cubeinfo ci;
    static float rSkill, rChequerSkill;
    static float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    static float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    static rolloutstat aarsStatistics[ 2 ][ 2 ];
    static evalsetup esDouble; /* shared between the
				  double and subsequent take/drop */
    static float arDouble[ NUM_CUBEFUL_OUTPUTS ]; /* likewise */
    doubletype dt;
    taketype tt;

    /* analyze this move */

    FixMatchState ( pms, pmr );

    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	fFirstMove = 1;

        if ( fUpdateStatistics )
          IniStatcontext( psc );
      
	break;
      
    case MOVE_NORMAL:
	if( pmr->n.fPlayer != pms->fMove ) {
	    SwapSides( pms->anBoard );
	    pms->fMove = pmr->n.fPlayer;
	}
      
	rSkill = rChequerSkill = 0.0f;
	GetMatchStateCubeInfo( &ci, pms );
      
	/* cube action? */
      
	if ( fAnalyseCube && !fFirstMove &&
	     GetDPEq ( NULL, NULL, &ci ) ) {
          
          if ( cmp_evalsetup ( pesCube, &pmr->n.esDouble ) > 0 ) {
            
	    if ( GeneralCubeDecision ( "",
				       aarOutput, aarStdDev, aarsStatistics, 
				       pms->anBoard, &ci,
				       pesCube ) < 0 )
              return -1;
            
            
	    FindCubeDecision ( arDouble, aarOutput, &ci );
            
	    pmr->n.esDouble = *pesCube;

            memcpy ( pmr->n.arDouble, arDouble, sizeof ( arDouble ) );
            memcpy ( pmr->n.aarOutput, aarOutput, sizeof ( aarOutput ) );
            memcpy ( pmr->n.aarStdDev, aarStdDev, sizeof ( aarStdDev ) );
            
          }
          
          rSkill = pmr->n.arDouble[ OUTPUT_NODOUBLE ] -
            pmr->n.arDouble[ OUTPUT_OPTIMAL ];
	      
	    pmr->n.stCube = Skill( rSkill );

	} else
          pmr->n.esDouble.et = EVAL_NONE;

        /* luck analysis */
      
	if( fAnalyseDice ) {
	    pmr->n.rLuck = LuckAnalysis( pms->anBoard,
					 pmr->n.anRoll[ 0 ],
					 pmr->n.anRoll[ 1 ],
					 &ci, fFirstMove );
	    pmr->n.lt = Luck( pmr->n.rLuck );
	  
	}
      
	/* evaluate move */
      
	if( fAnalyseMove ) {
	    /* evaluate move */

	    memcpy( anBoardMove, pms->anBoard,
		    sizeof( anBoardMove ) );
	    ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
	    PositionKey ( anBoardMove, auch );
	  
            if ( cmp_evalsetup ( pesChequer,
                                 &pmr->n.esChequer ) > 0 ) {

              if( pmr->n.ml.cMoves )
		free( pmr->n.ml.amMoves );
	  
              /* find best moves */
	  
              if( FindnSaveBestMoves ( &(pmr->n.ml), pmr->n.anRoll[ 0 ],
                                       pmr->n.anRoll[ 1 ],
                                       pms->anBoard, auch, &ci,
                                       &pesChequer->ec, aamf ) < 0 )
		return -1;

            }
	  
	    for( pmr->n.iMove = 0; pmr->n.iMove < pmr->n.ml.cMoves;
		 pmr->n.iMove++ )
		if( EqualKeys( auch,
			       pmr->n.ml.amMoves[ pmr->n.iMove ].auch ) ) {
		    rChequerSkill = pmr->n.ml.amMoves[ pmr->n.iMove ].
			rScore - pmr->n.ml.amMoves[ 0 ].rScore;
		  
		    break;
		}
	  
	    pmr->n.stMove = Skill( rChequerSkill );
	  
	    if( cAnalysisMoves >= 2 &&
		pmr->n.ml.cMoves > cAnalysisMoves ) {
		/* There are more legal moves than we want;
		   throw some away. */
		if( pmr->n.iMove >= cAnalysisMoves ) {
		    /* The move made wasn't in the top n; move it up so it
		       won't be discarded. */
		    memcpy( pmr->n.ml.amMoves + cAnalysisMoves - 1,
			    pmr->n.ml.amMoves + pmr->n.iMove,
			    sizeof( move ) );
		    pmr->n.iMove = cAnalysisMoves - 1;
		}
	      
                pmr->n.ml.amMoves = (move *)
                  realloc( pmr->n.ml.amMoves,
                           cAnalysisMoves * sizeof( move ) );
		pmr->n.ml.cMoves = cAnalysisMoves;
	    }

            pmr->n.esChequer = *pesChequer;
            
	}
      
        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );
	  
	fFirstMove = 0;
      
	break;
      
    case MOVE_DOUBLE:

        dt = DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );

        if ( dt != DT_NORMAL )
          break;

	/* cube action */	    
	if( fAnalyseCube ) {
	    GetMatchStateCubeInfo( &ci, pms );
	  
	    if ( GetDPEq ( NULL, NULL, &ci ) ||
                 ci.fCubeOwner < 0 || ci.fCubeOwner == ci.fMove ) {
	      
              if ( cmp_evalsetup ( pesCube, &pmr->d.esDouble ) > 0 ) {

		if ( GeneralCubeDecision ( "",
					   aarOutput, aarStdDev, aarsStatistics, 
					   pms->anBoard, &ci,
					   pesCube ) < 0 )
		    return -1;

              }
              else {
                memcpy ( aarOutput, pmr->d.aarOutput, sizeof ( aarOutput ) );
                memcpy ( aarStdDev, pmr->d.aarStdDev, sizeof ( aarStdDev ) );
              }
	      
                FindCubeDecision ( arDouble, aarOutput, &ci );
	      
		esDouble = pmr->d.esDouble = *pesCube;
	      
                memcpy ( pmr->d.arDouble, arDouble, sizeof ( arDouble ) );
                memcpy ( pmr->d.aarOutput, aarOutput, sizeof ( aarOutput ) );
                memcpy ( pmr->d.aarStdDev, aarStdDev, sizeof ( aarStdDev ) );
	      
		rSkill = arDouble[ OUTPUT_TAKE ] <
		    arDouble[ OUTPUT_DROP ] ?
		    arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
		    arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];
	      
		pmr->d.st = Skill( rSkill );
	      
	    } else
		esDouble.et = EVAL_NONE;
	}
      
        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

	break;
      
    case MOVE_TAKE:

        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;
      
	if( fAnalyseCube && esDouble.et != EVAL_NONE ) {

	    GetMatchStateCubeInfo( &ci, pms );
	  
	    pmr->d.esDouble = esDouble;

            memcpy ( pmr->d.arDouble, arDouble, sizeof ( arDouble ) );
            memcpy ( pmr->d.aarOutput, aarOutput, sizeof ( aarOutput ) );
            memcpy ( pmr->d.aarStdDev, aarStdDev, sizeof ( aarStdDev ) );
	  
            pmr->d.st = Skill ( -arDouble[ OUTPUT_TAKE ] - 
                                -arDouble[ OUTPUT_DROP ] );
	      
	}
        else
          pmr->d.esDouble.et = EVAL_NONE;

        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

	break;
      
    case MOVE_DROP:
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;
      
	if( fAnalyseCube && esDouble.et != EVAL_NONE ) {
	    GetMatchStateCubeInfo( &ci, pms );
	  
	    pmr->d.esDouble = esDouble;

	    memcpy( pmr->d.arDouble, arDouble, sizeof( arDouble ) );
            memcpy ( pmr->d.aarOutput, aarOutput, sizeof ( aarOutput ) );
            memcpy ( pmr->d.aarStdDev, aarStdDev, sizeof ( aarStdDev ) );
	  
            pmr->d.st = Skill( rSkill = -arDouble[ OUTPUT_DROP ] -
                               -arDouble[ OUTPUT_TAKE ] );
	      
	}
        else
          pmr->d.esDouble.et = EVAL_NONE;

        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

	break;
      
    case MOVE_RESIGN:

      /* swap board if player not on roll resigned */

      if( pmr->r.fPlayer != pms->fMove ) {
        SwapSides( pms->anBoard );
        pms->fMove = pmr->n.fPlayer;
      }
      
      if ( pesCube->et != EVAL_NONE ) {
        
        int nResign;
        float rBefore, rAfter;

        GetMatchStateCubeInfo ( &ci, pms );

        if ( cmp_evalsetup ( pesCube, &pmr->r.esResign ) > 0 ) {
          nResign =
            getResignation ( pmr->r.arResign, pms->anBoard, 
                             &ci, pesCube );
          
        }

        getResignEquities ( pmr->r.arResign, &ci, pmr->r.nResigned,
                            &rBefore, &rAfter );

        pmr->r.esResign = *pesCube;

        pmr->r.stResign = pmr->r.stAccept = SKILL_NONE;

        if ( rAfter < rBefore ) {
          /* wrong resign */
          pmr->r.stResign = Skill ( rAfter - rBefore );
          pmr->r.stAccept = SKILL_VERYGOOD;
        }

        if ( rBefore < rAfter ) {
          /* wrong accept */
          pmr->r.stAccept = Skill ( rBefore - rAfter );
          pmr->r.stResign = SKILL_VERYGOOD;
        }


      }

      break;
      
    case MOVE_SETDICE:
	if( pmr->sd.fPlayer != pms->fMove ) {
	    SwapSides( pms->anBoard );
	    pms->fMove = pmr->sd.fPlayer;
	}
      
	GetMatchStateCubeInfo( &ci, pms );
      
	if( fAnalyseDice ) {
	    pmr->sd.rLuck = LuckAnalysis( pms->anBoard,
					  pmr->sd.anDice[ 0 ],
					  pmr->sd.anDice[ 1 ],
					  &ci, fFirstMove );
	    pmr->sd.lt = Luck( pmr->sd.rLuck );
	}
      
	break;
      
    case MOVE_SETBOARD:	  
    case MOVE_SETCUBEVAL:
    case MOVE_SETCUBEPOS:
	break;
    }
  
    ApplyMoveRecord( pms, plGame, pmr );
  
    if ( fUpdateStatistics ) {
      psc->fMoves = fAnalyseMove;
      psc->fCube = fAnalyseCube;
      psc->fDice = fAnalyseDice;
    }
  
    return 0;
}


static int
AnalyzeGame ( list *plGame ) {

    list *pl;
    moverecord *pmr;
    movegameinfo *pmgi = plGame->plNext->p;
    matchstate msAnalyse;

    assert( pmgi->mt == MOVE_GAMEINFO );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;

	ProgressValueAdd( 1 );

        if( AnalyzeMove ( pmr, &msAnalyse, plGame, &pmgi->sc, 
                          &esAnalysisChequer,
                          &esAnalysisCube, aamfAnalysis, TRUE ) < 0 ) {
	    /* analysis incomplete; erase partial summary */
	    IniStatcontext( &pmgi->sc );
 	    return -1;
	}
    }
    
    return 0;
}

      	
extern void
AddStatcontext ( statcontext *pscA, statcontext *pscB ) {

  /* pscB = pscB + pscA */

  int i, j;

  pscB->fMoves |= pscA->fMoves;
  pscB->fDice |= pscA->fDice;
  pscB->fCube |= pscA->fCube;
  
  for ( i = 0; i < 2; i++ ) {

    pscB->anUnforcedMoves[ i ] += pscA->anUnforcedMoves[ i ];
    pscB->anTotalMoves[ i ] += pscA->anTotalMoves[ i ];
    
    pscB->anTotalCube[ i ] += pscA->anTotalCube[ i ];
    pscB->anCloseCube[ i ] += pscA->anCloseCube[ i ];
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

static int CheckSettings( void ) {

    if( !fAnalyseCube && !fAnalyseDice && !fAnalyseMove ) {
	outputl( _("You must specify at least one type of analysis to perform "
		 "(see `help set\nanalysis').") );
	return -1;
    }

    return 0;
}

static int
NumberMovesGame ( list *plGame ) {

  int nMoves = 0;
  list *pl;

  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) 
    nMoves++;

  return nMoves;

}


static int
NumberMovesMatch ( list *plMatch ) {

  int nMoves = 0;
  list *pl;

  for ( pl = plMatch->plNext; pl != plMatch; pl = pl->plNext )
    nMoves += NumberMovesGame ( pl->p );

  return nMoves;

}


extern void CommandAnalyseGame( char *sz ) {

  int nMoves;
  
  if( !plGame ) {
    outputl( _("No game is being played.") );
    return;
  }
    
  if( CheckSettings() )
    return;
  
  nMoves = NumberMovesGame ( plGame );

  ProgressStartValue( _("Analysing game; move:"), nMoves );
    
  AnalyzeGame( plGame );
  
  ProgressEnd();

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif
}


extern void CommandAnalyseMatch( char *sz ) {

  list *pl;
  movegameinfo *pmgi;
  int nMoves;
  
  if( ListEmpty( &lMatch ) ) {
      outputl( _("No match is being played.") );
      return;
  }

  if( CheckSettings() )
      return;

  nMoves = NumberMovesMatch ( &lMatch );

  ProgressStartValue( _("Analysing match; move:"), nMoves );

  IniStatcontext( &scMatch );
  
  for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext ) {
      if( AnalyzeGame( pl->p ) < 0 ) {
	  /* analysis incomplete; erase partial summary */
	  IniStatcontext( &scMatch );
	  break;
      }
      pmgi = ( (list *) pl->p )->plNext->p;
      assert( pmgi->mt == MOVE_GAMEINFO );
      AddStatcontext( &pmgi->sc, &scMatch );
  }
  
  ProgressEnd();

#if USE_GTK
  if( fX )
      GTKUpdateAnnotations();
#endif
}

extern void CommandAnalyseSession( char *sz ) {

    CommandAnalyseMatch( sz );
}



extern void
IniStatcontext ( statcontext *psc ) {

  /* Initialize statcontext with all zeroes */

  int i, j;

  psc->fMoves = psc->fCube = psc->fDice = FALSE;
  
  for ( i = 0; i < 2; i++ ) {

    psc->anUnforcedMoves[ i ] = 0;
    psc->anTotalMoves[ i ] = 0;
    
    psc->anTotalCube[ i ] = 0;
    psc->anCloseCube[ i ] = 0;
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



extern float
relativeFibsRating ( const float r, const int n ) {

  return - 2000.0 / sqrt ( 1.0 * n ) * log10 ( 1.0 / r - 1.0 );

} 


/*
 * Calculate an estimated rating based on some empirical estimate
 * using linear interpolation.
 *
 * Input
 *    r: error rate pr. move
 *    n: match length
 *
 */

extern float
absoluteFibsRating ( const float r, const int n ) {

  static float arRating[] =
    {  500, 1500,  1600,  1750,  1850,  1950,  2050, 2200 };
  static float arErrorRate[] =
    { 1e38, 0.030, 0.025, 0.020, 0.015, 0.010, 0.005, 0.0 };
  int i;
  float a;

  if ( r < 0 )
    return 2200;

  /* use linear interpolation between 
     (0.030,1500) and (0.0,2200) */

  for ( i = 6; i >= 1; i-- ) {

    if ( r < arErrorRate[ i ] ) {

      a = ( arRating[ i + 1 ] -  arRating[ i ] ) / 
        ( arErrorRate[ i + 1 ] - arErrorRate[ i ] );

      return arRating[ i ] + a * ( r - arErrorRate[ i ] );

    }

  }

  /* error rate above 0.030 */
  /* use exponential interpolation */

  return 500.0f + 1000.0f * exp ( 0.30f - 10.0 * r );

}


extern float
getMWCFromError ( const statcontext *psc, float aaaar[ 3 ][ 2 ][ 2 ][ 2 ] ) {

  int i, j;
  float r;

  for ( i = 0; i < 2; i++ ) 
    for ( j = 0; j < 2; j++ ) {

      /* chequer play */

      aaaar[ CHEQUERPLAY ][ TOTAL ][ i ][ j ] = psc->arErrorCheckerplay[ i ][ j ];

      if ( psc->anUnforcedMoves[ i ] )
        aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ j ] = 
          aaaar[ 0 ][ 0 ][ i ][ j ] / psc->anUnforcedMoves[ i ];
      else
        aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ j ] = 0.0f;

      /* cube decisions */

      aaaar[ CUBEDECISION ][ TOTAL ][ i ][ j ] = 
        psc->arErrorMissedDoubleDP[ i ][ j ]
        + psc->arErrorMissedDoubleTG[ i ][ j ]
        + psc->arErrorWrongDoubleDP[ i ][ j ]
        + psc->arErrorWrongDoubleTG[ i ][ j ]
        + psc->arErrorWrongTake[ i ][ j ]
        + psc->arErrorWrongPass[ i ][ j ];
      
      if ( psc->anCloseCube[ i ] )
        aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ j ] =
          aaaar[ CUBEDECISION ][ TOTAL ][ i ][ j ] / psc->anCloseCube[ i ];
      else
        aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ j ] = 0.0f;

      /* sum chequer play and cube decisions */
      /* FIXME: what average should be used? */

      aaaar[ COMBINED ][ TOTAL ][ i ][ j ] =
        aaaar[ CHEQUERPLAY ][ TOTAL ][ i ][ j ] + aaaar[ CUBEDECISION ][ TOTAL ][ i ][ j ];


      if ( psc->anUnforcedMoves[ i ] + psc->anCloseCube[ i ] )
        aaaar[ COMBINED ][ PERMOVE ][ i ][ j ] =
          aaaar[ COMBINED ][ TOTAL ][ i ][ j ] / 
          ( psc->anUnforcedMoves[ i ] + psc->anCloseCube[ i ] );
      else
        aaaar[ COMBINED ][ PERMOVE ][ i ][ j ] = 0.0f;

    }

  r =0.50 
    - aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] 
    + aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ];

  if ( r < 0.0f )
    return 0.0f;
  else if ( r > 1.0f )
    return 1.0f;
  else
    return r;

}


extern void
DumpStatcontext ( char *szOutput, const statcontext *psc, const char * sz ) {

  int i;
  ratingtype rt[ 2 ];
  char szTemp[1024];
  float aaaar[ 3 ][ 2 ][ 2 ][ 2 ];
  float r = getMWCFromError ( psc, aaaar );
  float rFac = ms.nMatchTo ? 100.0f : 1.0f;


  /* nice human readable dump */

  /* FIXME: make tty output shorter */
  /* FIXME: the code below is only for match play */
  /* FIXME: honour fOutputMWC etc. */
  /* FIXME: calculate ratings (ET, World class, etc.) */
  /* FIXME: use output*() functions, not printf */

  sprintf ( szTemp, "Player\t\t\t\t%-15s\t\t%-15s\n\n",
           ap[ 0 ].szName, ap [ 1 ].szName );
  strcpy ( szOutput, szTemp);
 
  if( psc->fMoves ) {

    sprintf ( strchr ( szOutput, 0 ),
              "%s\n\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n" 
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n",
              _("Chequerplay statistics"),
              _("Total moves"), 
	      psc->anTotalMoves[ 0 ], psc->anTotalMoves[ 1 ],
              _("Unforced moves"),
	      psc->anUnforcedMoves[ 0 ], psc->anUnforcedMoves[ 1 ],
              _("Moves marked very good"),
	      psc->anMoves[ 0 ][ SKILL_VERYGOOD ],
	      psc->anMoves[ 1 ][ SKILL_VERYGOOD ],
              _("Moves marked good"),
	      psc->anMoves[ 0 ][ SKILL_GOOD ],
	      psc->anMoves[ 1 ][ SKILL_GOOD ],
              _("Moves marked interesting"),
	      psc->anMoves[ 0 ][ SKILL_INTERESTING ],
	      psc->anMoves[ 1 ][ SKILL_INTERESTING ],
              _("Moves unmarked"),
	      psc->anMoves[ 0 ][ SKILL_NONE ],
	      psc->anMoves[ 1 ][ SKILL_NONE ],
              _("Moves marked doubtful"),
	      psc->anMoves[ 0 ][ SKILL_DOUBTFUL ],
	      psc->anMoves[ 1 ][ SKILL_DOUBTFUL ],
              _("Moves marked bad"),
	      psc->anMoves[ 0 ][ SKILL_BAD ],
	      psc->anMoves[ 1 ][ SKILL_BAD ],
              _("Moves marked very bad"),
	      psc->anMoves[ 0 ][ SKILL_VERYBAD ],
	      psc->anMoves[ 1 ][ SKILL_VERYBAD ] );

    sprintf ( strchr ( szOutput, 0 ),
              ms.nMatchTo ?
              "%-31s %+6.3f (%+7.3f%%)       %+6.3f (%+7.3f%%)\n"
              "%-31s %+6.3f (%+7.3f%%)       %+6.3f (%+7.3f%%)\n" :
              "%-31s %+6.3f (%+7.3f)        %+6.3f (%+7.3f)\n"
              "%-31s %+6.3f (%+7.3f)        %+6.3f (%+7.3f)\n",
              _("Error rate (total)"),
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] * rFac,
                    
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] * rFac,
              _("Error rate (per move)"),
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ] * rFac,
              
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
              -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] * rFac
              );


      for ( i = 0 ; i < 2; i++ )
        rt[ i ] = GetRating ( aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ] );
      
      sprintf ( strchr ( szOutput, 0 ),
                "%-31s %-23.23s %-23.23s\n\n",
                _("Chequer play rating"),
                psc->anUnforcedMoves[ 0 ] ? 
                gettext ( aszRating[ rt [ 0 ] ] ) : _("n/a"), 
                psc->anUnforcedMoves[ 1 ] ? 
                gettext ( aszRating[ rt [ 1 ] ] ) : _("n/a") );

  }

  if( psc->fDice ) {

      sprintf ( strchr ( szOutput, 0 ),
                "%-31s %3d                     %3d\n"
                "%-31s %3d                     %3d\n" 
                "%-31s %3d                     %3d\n"
                "%-31s %3d                     %3d\n"
                "%-31s %3d                     %3d\n",
                _("Rolls marked very lucky"),
	       psc->anLuck[ 0 ][ LUCK_VERYGOOD ],
	       psc->anLuck[ 1 ][ LUCK_VERYGOOD ],
                _("Rolls marked lucky"),
	       psc->anLuck[ 0 ][ LUCK_GOOD ],
	       psc->anLuck[ 1 ][ LUCK_GOOD ],
                _("Rolls unmarked"),
	       psc->anLuck[ 0 ][ LUCK_NONE ],
	       psc->anLuck[ 1 ][ LUCK_NONE ],
                _("Rolls marked unlucky"),
	       psc->anLuck[ 0 ][ LUCK_BAD ],
	       psc->anLuck[ 1 ][ LUCK_BAD ],
                _("Rolls marked very unlucky"),
	       psc->anLuck[ 0 ][ LUCK_VERYBAD ],
	       psc->anLuck[ 1 ][ LUCK_VERYBAD ] );

      sprintf ( strchr ( szOutput, 0 ),
                ms.nMatchTo ?
                "%-31s %+6.3f (%+7.3f%%)       %+6.3f (%+7.3f%%)\n" :
                "%-31s %+6.3f (%+7.3f)        %+6.3f (%+7.3f)\n",
                _("Luck rate (total)"),
                psc->arLuck[ 0 ][ 0 ],
                psc->arLuck[ 0 ][ 1 ] * rFac,
                psc->arLuck[ 1 ][ 0 ],
                psc->arLuck[ 1 ][ 1 ] * rFac );

      sprintf ( strchr ( szOutput, 0 ),
                "%-31s ", 
                _("Luck rate (per move)") );

      for ( i = 0; i < 2; ++i ) 
        if ( psc->anTotalMoves[ i ] ) {
          sprintf ( strchr ( szOutput, 0 ),
                    ms.nMatchTo ?
                    "%+6.3f (%+7.3f%%)" :
                    "%+6.3f (%+7.3f) ",
                    psc->arLuck[ i ][ 0 ] / psc->anTotalMoves[ i ],
                    psc->arLuck[ i ][ 1 ] * rFac / psc->anTotalMoves[ i ] );
          if ( ! i ) 
            strcat ( szOutput, "       " );
        }
        else
          sprintf ( strchr ( szOutput, 0 ),
                    "%-23.23s ", _("n/a") );

      strcat ( szOutput, "\n" );

      /* luck rating */


      for ( i = 0; i < 2; i++ ) 
        rt[ i ] = getLuckRating ( psc->arLuck[ i ][ 0 ] /
                                  psc->anTotalMoves[ i ] );

      sprintf ( strchr ( szOutput, 0 ),
                "%-31s %-23.23s %-23.23s\n\n\n",
                _("Luck rating"),
                psc->anTotalMoves[ 0 ] ? 
                gettext ( aszLuckRating[ rt [ 0 ] ] ) : _("n/a"), 
                psc->anTotalMoves[ 1 ] ? 
                gettext ( aszLuckRating[ rt [ 1 ] ] ) : _("n/a") );

  }

  /* cube decision */

  if( psc->fCube ) {

    sprintf ( strchr ( szOutput, 0 ),
              "%s\n\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n" 
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n"
              "%-31s %3d                     %3d\n",
              _("Cube decisions statistics:"),
              _("Total cube decisions"),
              psc->anTotalCube[ 0 ],
              psc->anTotalCube[ 1 ],
              _("Close or actual cube decisions"),
              psc->anCloseCube[ 0 ],
              psc->anCloseCube[ 1 ],
              _("Doubles"),
              psc->anDouble[ 0 ], 
              psc->anDouble[ 1 ], 
              _("Takes"),
              psc->anTake[ 0 ],
              psc->anTake[ 1 ],
              _("Pass"),
              psc->anPass[ 0 ], 
              psc->anPass[ 1 ] );

    sprintf ( strchr ( szOutput, 0 ),
              ms.nMatchTo ?
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" 
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" 
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" 
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" 
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" 
              "%-31s %3d (%+6.3f (%+7.3f%%)  %3d (%+6.3f (%+7.3f%%))\n" :
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              "%-31s %3d (%+6.3f (%+7.3f)   %3d (%+6.3f (%+7.3f))\n" 
              ,
              _("Missed doubles around DP"),
              psc->anCubeMissedDoubleDP[ 0 ],
              -psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
              -psc->arErrorMissedDoubleDP[ 0 ][ 1 ] * rFac,
              psc->anCubeMissedDoubleDP[ 1 ],
              -psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
              -psc->arErrorMissedDoubleDP[ 1 ][ 1 ] * rFac,
              _("Missed doubles around TG"),
              psc->anCubeMissedDoubleTG[ 0 ],
              -psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
              -psc->arErrorMissedDoubleTG[ 0 ][ 1 ] * rFac,
              psc->anCubeMissedDoubleTG[ 1 ],
              -psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
              -psc->arErrorMissedDoubleTG[ 1 ][ 1 ] * rFac,
              _("Wrong doubles around DP"),
              psc->anCubeWrongDoubleDP[ 0 ],
              -psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
              -psc->arErrorWrongDoubleDP[ 0 ][ 1 ] * rFac,
              psc->anCubeWrongDoubleDP[ 1 ],
              -psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
              -psc->arErrorWrongDoubleDP[ 1 ][ 1 ] * rFac,
              _("Wrong doubles around TG"),
              psc->anCubeWrongDoubleTG[ 0 ],
              -psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
              -psc->arErrorWrongDoubleTG[ 0 ][ 1 ] * rFac,
              psc->anCubeWrongDoubleTG[ 1 ],
              -psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
              -psc->arErrorWrongDoubleTG[ 1 ][ 1 ] * rFac,
              _("Wrong takes"),
              psc->anCubeWrongTake[ 0 ],
              -psc->arErrorWrongTake[ 0 ][ 0 ],
              -psc->arErrorWrongTake[ 0 ][ 1 ] * rFac,
              psc->anCubeWrongTake[ 1 ],
              -psc->arErrorWrongTake[ 1 ][ 0 ],
              -psc->arErrorWrongTake[ 1 ][ 1 ] * rFac,
              _("Wrong passes"),
              psc->anCubeWrongPass[ 0 ],
              -psc->arErrorWrongPass[ 0 ][ 0 ],
              -psc->arErrorWrongPass[ 0 ][ 1 ] * rFac,
              psc->anCubeWrongPass[ 1 ],
              -psc->arErrorWrongPass[ 1 ][ 0 ],
              -psc->arErrorWrongPass[ 1 ][ 1 ] * rFac );
      

      sprintf ( strchr ( szOutput, 0 ),
                ms.nMatchTo ?
                "%-31s %+6.3f (%+7.3f%%)       %+6.3f (%+7.3f%%)\n" :
                "%-31s %+6.3f (%+7.3f)        %+6.3f (%+7.3f)\n",
                _("Error rate (total)"),
                -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] * rFac,
                    
                -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] * rFac );

      sprintf ( strchr ( szOutput, 0 ),
                "%-31s ", 
                _("Error rate (per cube decision)") );

      for ( i = 0; i < 2; ++i ) 
        if ( psc->anCloseCube[ i ] ) {
          sprintf ( strchr ( szOutput, 0 ),
                    ms.nMatchTo ?
                    "%+6.3f (%+7.3f%%)" :
                    "%+6.3f (%+7.3f) ",
                      -aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ],
                      -aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ UNNORMALISED ] * rFac );
          if ( ! i ) 
            strcat ( szOutput, "       " );
        }
        else
          sprintf ( strchr ( szOutput, 0 ),
                    "%-23.23s ", _("n/a") );

      strcat ( szOutput, "\n" );

      /* luck rating */


      for ( i = 0 ; i < 2; i++ )
        rt[ i ] = GetRating ( aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ] );
      
      sprintf ( strchr ( szOutput, 0 ),
                "%-31s %-23.23s %-23.23s\n\n\n",
                _("Cube decision rating"),
                psc->anCloseCube[ 0 ] ? 
                gettext ( aszRating[ rt [ 0 ] ] ) : _("n/a"), 
                psc->anCloseCube[ 1 ] ? 
                gettext ( aszRating[ rt [ 1 ] ] ) : _("n/a") );

  }
  

  /* overall rating */
  
  if( psc->fMoves && psc->fCube ) {
    
    strcat ( szOutput, _("Overall statistics:\n\n") );

      sprintf ( strchr ( szOutput, 0 ),
                ms.nMatchTo ?
                "%-31s %+6.3f (%+7.3f%%)       %+6.3f (%+7.3f%%)\n" :
                "%-31s %+6.3f (%+7.3f)        %+6.3f (%+7.3f)\n",
                _("Error rate (total)"),
                -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] * rFac,
                    
                -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] * rFac );

      sprintf ( strchr ( szOutput, 0 ),
                "%-31s ", 
                _("Error rate (per decision)") );

      for ( i = 0; i < 2; ++i ) 
        if ( psc->anCloseCube[ i ] ) {
          sprintf ( strchr ( szOutput, 0 ),
                    ms.nMatchTo ?
                    "%+6.3f (%+7.3f%%)" :
                    "%+6.3f (%+7.3f) ",
                  -aaaar[ COMBINED ][ PERMOVE ][ i ][ NORMALISED ],
                  -aaaar[ COMBINED ][ PERMOVE ][ i ][ UNNORMALISED ] * rFac );
          if ( ! i ) 
            strcat ( szOutput, "       " );
        }
        else
          sprintf ( strchr ( szOutput, 0 ),
                    "%-23.23s ", _("n/a") );

      strcat ( szOutput, "\n" );

  }

  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( aaaar[ COMBINED ][ PERMOVE ][ i ][ NORMALISED ] );
      
  sprintf ( strchr ( szOutput, 0 ),
            "%-31s %-23.23s %-23.23s\n\n",
            _("Cube decision rating"),
            ( psc->anUnforcedMoves[ 0 ] + psc->anCloseCube[ 0 ] ) ?
            gettext ( aszRating[ rt [ 0 ] ] ) : _("n/a"), 
            ( psc->anUnforcedMoves[ 1 ] + psc->anCloseCube[ 1 ] ) ?
            gettext ( aszRating[ rt [ 1 ] ] ) : _("n/a") );
  
  
  /* calculate total error */
  
  if ( ms.nMatchTo ) {

  sprintf ( strchr ( szOutput, 0 ),
            "%s\n"
            "%-31s %7.2f%%                %7.2f%%\n"
            "%-31s %7.2f                 %7.2f\n",
            _("Match winning chance"),
            _("against opponent"),
            100.0 * r, 100.0 * ( 1.0 - r ),
            _("Guestimated abs. rating"),
            absoluteFibsRating ( aaaar[ COMBINED ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ], 
                                 ms.nMatchTo ),
            absoluteFibsRating ( aaaar[ COMBINED ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ], 
                                 ms.nMatchTo ) );

  }

}


extern void
CommandShowStatisticsMatch ( char *sz ) {

    char szOutput[4096];

    updateStatisticsMatch ( &lMatch );

#if USE_GTK
    if ( fX ) {
	GTKDumpStatcontext ( &scMatch, &ms, _("Statistics for all games") );
	return;
    }
#endif

    DumpStatcontext ( szOutput, &scMatch, _("Statistics for all games"));
    outputl(szOutput);
}


extern void
CommandShowStatisticsSession ( char *sz ) {

  CommandShowStatisticsMatch ( sz );

}


extern void
CommandShowStatisticsGame ( char *sz ) {

    movegameinfo *pmgi;
    char szOutput[4096];
    
    if( !plGame ) {
	outputl( _("No game is being played.") );
	return;
    }

    updateStatisticsGame ( plGame );

    pmgi = plGame->plNext->p;

    assert( pmgi->mt == MOVE_GAMEINFO );
    
#if USE_GTK
    if ( fX ) {
	GTKDumpStatcontext ( &pmgi->sc, &ms, _("Statistics for current game") );
	return;
    }
#endif

    DumpStatcontext ( szOutput, &pmgi->sc, _("Statistics for current game"));
    outputl( szOutput );
}


extern void CommandAnalyseMove ( char *sz ) {

  matchstate msx;

  if( ms.gs == GAME_NONE ) {
    outputl( _("No game in progress (type `new game' to start one).") );
    return;
  }
    

  if ( plLastMove && plLastMove->plNext && plLastMove->plNext->p ) {

    /* analyse move */

    memcpy ( &msx, &ms, sizeof ( matchstate ) );
    AnalyzeMove ( plLastMove->plNext->p, &msx, plGame, NULL, 
                  &esAnalysisChequer, &esAnalysisCube, aamfAnalysis, FALSE );

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif
  }
  else
    outputl ( _("Sorry, cannot analyse move!") );

}


static void
updateStatisticsMove ( moverecord *pmr, matchstate *pms, list *plGame,
                       statcontext *psc ) {

  FixMatchState ( pms, pmr );

  switch ( pmr->mt ) {
  case MOVE_GAMEINFO:
    IniStatcontext ( psc );
    break;

  case MOVE_NORMAL:
    if( pmr->n.fPlayer != pms->fMove ) {
      SwapSides( pms->anBoard );
      pms->fMove = pmr->n.fPlayer;
    }
      
    updateStatcontext ( psc, pmr, pms );
    break;

  case MOVE_DOUBLE:
    if( pmr->d.fPlayer != pms->fMove ) {
      SwapSides( pms->anBoard );
      pms->fMove = pmr->d.fPlayer;
    }
      
      
    updateStatcontext ( psc, pmr, pms );
    break;

  case MOVE_TAKE:
  case MOVE_DROP:

    updateStatcontext ( psc, pmr, pms );
    break;

  default:
    break;

  }

  ApplyMoveRecord( pms, plGame, pmr );
  
  psc->fMoves = fAnalyseMove;
  psc->fCube = fAnalyseCube;
  psc->fDice = fAnalyseDice;
  
}



extern void
updateStatisticsGame ( list *plGame ) {

  list *pl;
  moverecord *pmr;
  movegameinfo *pmgi = plGame->plNext->p;
  matchstate msAnalyse;
  
  assert( pmgi->mt == MOVE_GAMEINFO );
    
  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
    
    pmr = pl->p;
    
    updateStatisticsMove ( pmr, &msAnalyse, plGame, &pmgi->sc );

  }
    
}





extern void
updateStatisticsMatch ( list *plMatch ) {

  list *pl;
  movegameinfo *pmgi;

  if( ListEmpty( plMatch ) ) 
    /* no match in progress */
    return;

  IniStatcontext( &scMatch );
  
  for( pl = plMatch->plNext; pl != plMatch; pl = pl->plNext ) {
    
    updateStatisticsGame( pl->p );
    
    pmgi = ( (list *) pl->p )->plNext->p;
    assert( pmgi->mt == MOVE_GAMEINFO );
    AddStatcontext( &pmgi->sc, &scMatch );

  }

}



extern int getLuckRating ( const float rLuck ) {

  if ( rLuck < -0.10 )
    return 0;
  else if ( rLuck < -0.06 )
    return 1;
  else if ( rLuck < -0.02 )
    return 2;
  else if ( rLuck < +0.02 )
    return 3;
  else if ( rLuck < +0.06 )
    return 4;
  else
    return 5;

}


static void
AnalyseClearMove ( moverecord *pmr ) {

  if ( ! pmr )
    return;

  switch ( pmr->mt ) {
  case MOVE_GAMEINFO:
    IniStatcontext ( &pmr->g.sc );
    break;

  case MOVE_NORMAL:

    pmr->n.esDouble.et = pmr->n.esChequer.et = EVAL_NONE;
    pmr->n.stMove = pmr->n.stCube = SKILL_NONE;
    pmr->n.rLuck = ERR_VAL;
    pmr->n.lt = LUCK_NONE;
    if ( pmr->n.ml.amMoves ) {
      free ( pmr->n.ml.amMoves );
      pmr->n.ml.amMoves = NULL;
    }
    pmr->n.ml.cMoves = 0;
    break;

  case MOVE_DOUBLE:
  case MOVE_TAKE:
  case MOVE_DROP:

    pmr->d.esDouble.et = EVAL_NONE;
    pmr->d.st = SKILL_NONE;
    break;
      
  case MOVE_RESIGN:

    pmr->r.esResign.et = EVAL_NONE;
    pmr->r.stResign = pmr->r.stAccept = SKILL_NONE;
    break;

  case MOVE_SETDICE:

    pmr->sd.lt = LUCK_NONE;
    pmr->sd.rLuck = ERR_VAL;
    break;

  default:
    /* no-op */
    break;

  }

}

static void
AnalyseClearGame ( list *plGame ) {

  list *pl;

  if ( ! plGame || ListEmpty ( plGame ) )
    return;

  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) 
    AnalyseClearMove ( pl->p );

}


extern void
CommandAnalyseClearMove ( char *sz ) {

  if ( plLastMove && plLastMove->plNext && plLastMove->plNext->p ) {
    AnalyseClearMove ( plLastMove->plNext->p );
#if USE_GTK
    if( fX )
      GTKUpdateAnnotations();
#endif
  }
  else
    outputl ( _("Cannot clear analysis on this move" ) );

}

extern void
CommandAnalyseClearGame ( char *sz ) {

  if( !plGame ) {
    outputl( _("No game is being played.") );
    return;
  }

  AnalyseClearGame ( plGame );

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif

}

extern void
CommandAnalyseClearMatch ( char *sz ) {

  list *pl;

  if( ListEmpty( &lMatch ) ) {
    outputl( _("No match is being played.") );
    return;
  }

  for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext ) 
    AnalyseClearGame ( pl->p );

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif

}

