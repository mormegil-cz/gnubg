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
#include "sound.h"
#include "matchequity.h"
#include "export.h"
#include "formatgs.h"

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
  1e38, 0.035, 0.026, 0.018, 0.012, 0.008, 0.005, 0.002 };
/* 1e38, 0.060, 0.030, 0.025, 0.020, 0.015, 0.010, 0.005 }; */

int afAnalysePlayers[ 2 ] = { TRUE, TRUE };

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
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL, 0.0f,
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
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL, 0.0f,
                              &ciOpp, (evalcontext *) pec, 
                              defaultFilters ) < 0 )
        return ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, (cubeinfo *) pci, 
                                  (evalcontext *) pec ) < 0 )
          return ERR_VAL;

        if ( pec->fCubeful ) {
          if ( pci->nMatchTo )
            aar[ i ][ j ] = mwc2eq ( ar[ OUTPUT_CUBEFUL_EQUITY ], pci );
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

  if ( n0 > n1 )
    return aar[ n0 ][ n1 ] - rMean / 30.0f;
  else
    return aar[ n1 ][ n0 ] - rMean / 30.0f;

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
      if( FindnSaveBestMoves( &ml, i + 1, j + 1, anBoardTemp, NULL, 0.0f,
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

    if( r > arLuckLevel[ LUCK_VERYGOOD ] )
	return LUCK_VERYGOOD;
    else if( r > arLuckLevel[ LUCK_GOOD ] )
	return LUCK_GOOD;
    else if( r < -arLuckLevel[ LUCK_VERYBAD ] )
	return LUCK_VERYBAD;
    else if( r < -arLuckLevel[ LUCK_BAD ] )
	return LUCK_BAD;
    else
	return LUCK_NONE;
}

extern skilltype
Skill( float const r )
{
  if( r < -arSkillLevel[ SKILL_VERYBAD ] )
    return SKILL_VERYBAD;
  else if( r < -arSkillLevel[ SKILL_BAD ] )
    return SKILL_BAD;
  else if( r < -arSkillLevel[ SKILL_DOUBTFUL ] )
    return SKILL_DOUBTFUL;
  else
    return SKILL_GOOD;
}

/*
 * get mean of double point and cash point
 * and cash point and too good point
 */

static void
getMarketWindowDividersEq( float arMarketWindowDividersEq[ 2 ],
			   const matchstate* pms )
{
    cubeinfo ci;
    evalcontext ec = { FALSE, 0, 0, TRUE, 0.0 };
    float aarRates[ 2 ][ 2 ];
    float arOutput[ NUM_OUTPUTS ];
    int afAutoRedouble[ 2 ];
    int afDead[ 2 ];

    GetMatchStateCubeInfo( &ci, pms );

    getCurrentGammonRates( aarRates, arOutput, (void *) pms->anBoard,
			   &ci, &ec );

    if ( ci.nMatchTo ) {
    	/* Match play */
	float aaarPointsMatch[ 2 ][ 4 ][ 2 ];
	int nIndex;

	getMatchPoints( aaarPointsMatch, afAutoRedouble, afDead,
		    &ci, aarRates );

	nIndex = ! afDead[ ci.fMove ];

	/* calculate divider between DP and CP: DP+(CP-DP)/2 */
	arMarketWindowDividersEq[ 0 ] =
		aaarPointsMatch[ ci.fMove ][ 1 ][ nIndex ] +
		( aaarPointsMatch[ ci.fMove ][ 2 ][ nIndex ] -
		aaarPointsMatch[ ci.fMove ][ 1 ][ nIndex ] ) / 2;
	/* calculate divider between CP and TG: CP+(TG-CP)/2 */
	arMarketWindowDividersEq[ 1 ] =
		aaarPointsMatch[ ci.fMove ][ 2 ][ nIndex ] +
		( aaarPointsMatch[ ci.fMove ][ 3 ][ 0 ] -
		aaarPointsMatch[ ci.fMove ][ 2 ][ nIndex ] ) / 2;
    }
    else {
    	/* Money play */
    	float aaarPointsMoney[ 2 ][ 7 ][ 2 ];

	getMoneyPoints( aaarPointsMoney, ci.fJacoby, ci.fBeavers, aarRates );

	/* calculate divider between DP and CP: DP+(CP-DP)/2 */
	arMarketWindowDividersEq[ 0 ] =
		aaarPointsMoney[ ci.fMove ][ 3 ][ 0 ] +
		( aaarPointsMoney[ ci.fMove ][ 5 ][ 0 ] -
		aaarPointsMoney[ ci.fMove ][ 3 ][ 0 ] ) / 2;
	/* calculate divider between CP and TG: CP+(TG-CP)/2 */
	arMarketWindowDividersEq[ 1 ] =
		aaarPointsMoney[ ci.fMove ][ 5 ][ 0 ] +
		( aaarPointsMoney[ ci.fMove ][ 6 ][ 0 ] -
		aaarPointsMoney[ ci.fMove ][ 5 ][ 0 ] ) / 2;
    }
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
updateStatcontext(statcontext*       psc,
		  const moverecord*  pmr,
		  const matchstate*  pms)
{
  cubeinfo ci;
  static unsigned char auch[ 10 ];
  float rSkill, rChequerSkill, rCost;
  int i;
  int anBoardMove[ 2 ][ 25 ];
  float arDouble[ 4 ];

  switch ( pmr->mt ) {

  case MOVE_GAMEINFO:
    /* update luck adjusted result */

    psc->arActualResult[ 0 ] = psc->arActualResult[ 1 ] = 0.0f;

    if ( pmr->g.fWinner != -1 ) {

      if ( pmr->g.nMatch ) {

        psc->arActualResult[ pmr->g.fWinner ] = 
          getME( pmr->g.anScore[ 0 ], pmr->g.anScore[ 1 ], pmr->g.nMatch,
                 pmr->g.fWinner, pmr->g.nPoints, pmr->g.fWinner,
                 pmr->g.fCrawfordGame,
                 aafMET, aafMETPostCrawford ) - 
          getMEAtScore( pmr->g.anScore[ 0 ], pmr->g.anScore[ 1 ], 
                        pmr->g.nMatch,
                        pmr->g.fWinner, 
                        pmr->g.fCrawfordGame,
                        aafMET, aafMETPostCrawford );

        psc->arActualResult[ ! pmr->g.fWinner ] =
          - psc->arActualResult[ pmr->g.fWinner ];
      }
      else {
        psc->arActualResult[ pmr->g.fWinner ] = pmr->g.nPoints;
        psc->arActualResult[ ! pmr->g.fWinner ] = -pmr->g.nPoints;
      }

    }

    for ( i = 0; i < 2; ++i )
      psc->arLuckAdj[ i ] = psc->arActualResult[ i ];

    break;

  case MOVE_NORMAL:

    /* 
     * Cube analysis; check for
     *   - missed doubles
     */

    GetMatchStateCubeInfo ( &ci, pms );
      
    if ( pmr->n.esDouble.et != EVAL_NONE && fAnalyseCube ) {

      FindCubeDecision( arDouble, pmr->n.aarOutput, &ci );

      psc->anTotalCube[ pmr->n.fPlayer ]++;

      if ( isCloseCubedecision ( arDouble ) || 
           isMissedDouble ( arDouble, GCCCONSTAHACK pmr->n.aarOutput,
			    FALSE, &ci ) )
        psc->anCloseCube[ pmr->n.fPlayer ]++;
	  
      if( arDouble[ OUTPUT_NODOUBLE ] <
          arDouble[ OUTPUT_OPTIMAL ] ) {
        /* it was a double */

	float arMarketWindowDividersEq[ 2 ] = { 0.0, 0.0 };

        rSkill = arDouble[ OUTPUT_NODOUBLE ] -
          arDouble[ OUTPUT_OPTIMAL ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;

	getMarketWindowDividersEq( arMarketWindowDividersEq, pms );

        if ( pmr->n.aarOutput[ 0 ][ OUTPUT_WIN ] >
	     arMarketWindowDividersEq[ 1 ] ) {
          /* around too good point */
          psc->anCubeMissedDoubleTG[ pmr->n.fPlayer ]++;
          psc->arErrorMissedDoubleTG[ pmr->n.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorMissedDoubleTG[ pmr->n.fPlayer ][ 1 ] -=
            rCost;
        } 
        else if ( pmr->n.aarOutput[ 0 ][ OUTPUT_WIN ] >
		  arMarketWindowDividersEq[ 0 ] ) {
          /* around cash point */
          psc->anCubeMissedDoubleCP[ pmr->n.fPlayer ]++;
          psc->arErrorMissedDoubleCP[ pmr->n.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorMissedDoubleCP[ pmr->n.fPlayer ][ 1 ] -=
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

      float r = pms->nMatchTo ?
        eq2mwc( pmr->n.rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
        pms->nCube * pmr->n.rLuck;

      psc->arLuck[ pmr->n.fPlayer ][ 0 ] += pmr->n.rLuck;
      psc->arLuck[ pmr->n.fPlayer ][ 1 ] += r;

      psc->arLuckAdj[ pmr->n.fPlayer ] -= r;
      psc->arLuckAdj[ ! pmr->n.fPlayer ] += r;
      
      psc->anLuck[ pmr->n.fPlayer ][ pmr->n.lt ]++;

    }


    /*
     * update chequerplay statistics 
     */

    /* Count total regradless of analysis */
    psc->anTotalMoves[ pmr->n.fPlayer ]++;

    /* hmm, MOVE_NORMALs which has no legal moves have
       pmr->n.esChequer.et == EVAL_NONE
       (Joseph) But we can detect them by checking if a move was actually
       made or not.
    */
    
    if( fAnalyseMove &&
	 (pmr->n.esChequer.et != EVAL_NONE || pmr->n.anMove[0] < 0) ) {

      /* find skill */

      memcpy( anBoardMove, pms->anBoard, sizeof( anBoardMove ) );
      ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
      PositionKey ( anBoardMove, auch );
      rChequerSkill = 0.0f;
	  
      for( i = 0; i < pmr->n.ml.cMoves; i++ ) 

        if( EqualKeys(auch, pmr->n.ml.amMoves[ i ].auch) ) {

          rChequerSkill =
            pmr->n.ml.amMoves[ i ].rScore - pmr->n.ml.amMoves[ 0 ].rScore;
          
          break;
        }

      /* update statistics */
	  
      rCost = pms->nMatchTo ? eq2mwc( rChequerSkill, &ci ) -
        eq2mwc( 0.0f, &ci ) : pms->nCube * rChequerSkill;

      psc->anMoves[ pmr->n.fPlayer ][ Skill( rChequerSkill ) ]++;
	  
      if( pmr->n.ml.cMoves > 1 ) {
        psc->anUnforcedMoves[ pmr->n.fPlayer ]++;
        psc->arErrorCheckerplay[ pmr->n.fPlayer ][ 0 ] -= rChequerSkill;
        psc->arErrorCheckerplay[ pmr->n.fPlayer ][ 1 ] -= rCost;
      }

    } else {
      /* unmarked move */
      psc->anMoves[ pmr->n.fPlayer ][ SKILL_NONE ] ++;
    }

    break;

  case MOVE_DOUBLE:

    GetMatchStateCubeInfo ( &ci, pms );
    if ( fAnalyseCube && pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE ) {

      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->d.CubeDecPtr->aarOutput, &ci );

      rSkill = arDouble[ OUTPUT_TAKE ] <
        arDouble[ OUTPUT_DROP ] ?
        arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
        arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];
	      
      psc->anTotalCube[ pmr->d.fPlayer ]++;
      psc->anDouble[ pmr->d.fPlayer ]++;
      psc->anCloseCube[ pmr->d.fPlayer ]++;

      if ( rSkill < 0.0f ) {
        /* it was not a double */

	float arMarketWindowDividersEq[ 2 ] = { 0.0, 0.0 };

        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;

	getMarketWindowDividersEq( arMarketWindowDividersEq, pms );

        if ( pmr->d.CubeDecPtr->aarOutput[ 0 ][ OUTPUT_WIN ] >
	     arMarketWindowDividersEq[ 1 ] ) {
          /* around too good point */
          psc->anCubeWrongDoubleTG[ pmr->d.fPlayer ]++;
          psc->arErrorWrongDoubleTG[ pmr->d.fPlayer ][ 0 ] -= rSkill;
          psc->arErrorWrongDoubleTG[ pmr->d.fPlayer ][ 1 ] -= rCost;
        }
        else if ( pmr->d.CubeDecPtr->aarOutput[ 0 ][ OUTPUT_WIN ] >
		  arMarketWindowDividersEq[ 0 ] ) {
          /* around cash point */
          psc->anCubeWrongDoubleCP[ pmr->n.fPlayer ]++;
          psc->arErrorWrongDoubleCP[ pmr->n.fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorWrongDoubleCP[ pmr->n.fPlayer ][ 1 ] -=
            rCost;
        }
        else {
          /* around double point */
          psc->anCubeWrongDoubleDP[ pmr->d.fPlayer ]++;
          psc->arErrorWrongDoubleDP[ pmr->d.fPlayer ][ 0 ] -= rSkill;
          psc->arErrorWrongDoubleDP[ pmr->d.fPlayer ][ 1 ] -= rCost;
        }
      }

    }

    break;

  case MOVE_TAKE:

    GetMatchStateCubeInfo ( &ci, pms );
    if ( fAnalyseCube && pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE ) {

      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->d.CubeDecPtr->aarOutput, &ci );

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

    GetMatchStateCubeInfo ( &ci, pms );
    if( fAnalyseCube && pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE ) {
	  
      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->d.CubeDecPtr->aarOutput, &ci );

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

  case MOVE_SETDICE:

    /*
     * update luck statistics for roll
     */

    if ( fAnalyseDice && pmr->sd.rLuck != ERR_VAL ) {

      float r = pms->nMatchTo ?
        eq2mwc( pmr->sd.rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
        pms->nCube * pmr->sd.rLuck;

      psc->arLuck[ pmr->sd.fPlayer ][ 0 ] += pmr->sd.rLuck;
      psc->arLuck[ pmr->sd.fPlayer ][ 1 ] += r;

      psc->arLuckAdj[ pmr->sd.fPlayer ] -= r;
      psc->arLuckAdj[ ! pmr->sd.fPlayer ] += r;
      
      psc->anLuck[ pmr->sd.fPlayer ][ pmr->sd.lt ]++;

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
	      int fUpdateStatistics, const int afAnalysePlayers[ 2 ] ) {

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

        if ( fUpdateStatistics ) {
          IniStatcontext( psc );
          updateStatcontext ( psc, pmr, pms );
        }

	break;
      
    case MOVE_NORMAL:
	if( pmr->n.fPlayer != pms->fMove ) {
	    SwapSides( pms->anBoard );
	    pms->fMove = pmr->n.fPlayer;
	}

        if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->n.fPlayer ] ) {
          /* we do not analyse this player */
          fFirstMove = 0;
          break;
        }
      
	rSkill = rChequerSkill = 0.0f;
	GetMatchStateCubeInfo( &ci, pms );
      
	/* cube action? */
      
	if ( fAnalyseCube && !fFirstMove &&
	     GetDPEq ( NULL, NULL, &ci ) ) {
          
          if ( cmp_evalsetup ( pesCube, &pmr->n.esDouble ) > 0 ) {
            
	    if ( GeneralCubeDecision ( aarOutput, aarStdDev, aarsStatistics, 
				       pms->anBoard, &ci,
				       pesCube, NULL, NULL  ) < 0 )
              return -1;
            
            
	    pmr->n.esDouble = *pesCube;

            memcpy ( pmr->n.aarOutput, aarOutput, sizeof ( aarOutput ) );
            memcpy ( pmr->n.aarStdDev, aarStdDev, sizeof ( aarStdDev ) );
            
          }
          
          FindCubeDecision( arDouble, 
                            GCCCONSTAHACK pmr->n.aarOutput, &ci );
          
          rSkill = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_OPTIMAL ];
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
                                       pms->anBoard, auch, 
                                       arSkillLevel[ SKILL_DOUBTFUL ],
                                       &ci, &pesChequer->ec, aamf ) < 0 )
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

      /* always analyse MOVE_DOUBLEs as they are shared with the subsequent
         MOVE_TAKEs or MOVE_DROPs. */

        dt = DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );

        if ( dt != DT_NORMAL )
          break;

	/* cube action */	    
	if( fAnalyseCube ) {
	    GetMatchStateCubeInfo( &ci, pms );
	  
	    if ( GetDPEq ( NULL, NULL, &ci ) ||
                 ci.fCubeOwner < 0 || ci.fCubeOwner == ci.fMove ) {
	      
              if ( cmp_evalsetup ( pesCube, 
				   &pmr->d.CubeDecPtr->esDouble ) > 0 ) {

		if ( GeneralCubeDecision ( aarOutput, aarStdDev, 
                                           aarsStatistics, pms->anBoard, &ci,
					   pesCube, NULL, NULL ) < 0 )
		    return -1;
		
		pmr->d.CubeDecPtr->esDouble = *pesCube;

              }
              else {
                memcpy ( aarOutput, pmr->d.CubeDecPtr->aarOutput, 
						 sizeof ( aarOutput ) );
                memcpy ( aarStdDev, pmr->d.CubeDecPtr->aarStdDev, 
						 sizeof ( aarStdDev ) );
              }
	      
                FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );
	      
		esDouble = pmr->d.CubeDecPtr->esDouble;
	      
                memcpy ( pmr->d.CubeDecPtr->aarOutput, aarOutput, 
						 sizeof ( aarOutput ) );
                memcpy ( pmr->d.CubeDecPtr->aarStdDev, aarStdDev, 
						 sizeof ( aarStdDev ) );
	      
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

        if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->d.fPlayer ] )
          /* we do not analyse this player */
          break;
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;
      
	if( fAnalyseCube && esDouble.et != EVAL_NONE ) {

	    GetMatchStateCubeInfo( &ci, pms );
	  
            pmr->d.st = Skill ( -arDouble[ OUTPUT_TAKE ] - 
                                -arDouble[ OUTPUT_DROP ] );
	      
	}

        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

	break;
      
    case MOVE_DROP:
      
        if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->d.fPlayer ] )
          /* we do not analyse this player */
          break;
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;
      
	if( fAnalyseCube && esDouble.et != EVAL_NONE ) {
	    GetMatchStateCubeInfo( &ci, pms );
	  
	    pmr->d.st = Skill( rSkill = -arDouble[ OUTPUT_DROP ] -
                               -arDouble[ OUTPUT_TAKE ] );
	      
	}


        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

	break;
      
    case MOVE_RESIGN:

      /* swap board if player not on roll resigned */

      if( pmr->r.fPlayer != pms->fMove ) {
        SwapSides( pms->anBoard );
        pms->fMove = pmr->n.fPlayer;
      }
      
      if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->r.fPlayer ] )
        /* we do not analyse this player */
        break;
      
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
          pmr->r.stAccept = SKILL_GOOD; //VERYGOOD;
        }

        if ( rBefore < rAfter ) {
          /* wrong accept */
          pmr->r.stAccept = Skill ( rBefore - rAfter );
          pmr->r.stResign = SKILL_GOOD; // VERYGOOD;
        }


      }

      break;
      
    case MOVE_SETDICE:
	if( pmr->sd.fPlayer != pms->fMove ) {
	    SwapSides( pms->anBoard );
	    pms->fMove = pmr->sd.fPlayer;
	}
      
        if ( afAnalysePlayers && ! afAnalysePlayers[ pmr->sd.fPlayer ] )
          /* we do not analyse this player */
          break;
      
	GetMatchStateCubeInfo( &ci, pms );
      
	if( fAnalyseDice ) {
	    pmr->sd.rLuck = LuckAnalysis( pms->anBoard,
					  pmr->sd.anDice[ 0 ],
					  pmr->sd.anDice[ 1 ],
					  &ci, fFirstMove );
	    pmr->sd.lt = Luck( pmr->sd.rLuck );
	}

        if ( fUpdateStatistics )
          updateStatcontext ( psc, pmr, pms );

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
  
    return fInterrupt ? -1 : 0;
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
                          &esAnalysisCube, aamfAnalysis, TRUE, 
                          afAnalysePlayers ) < 0 ) {
	    /* analysis incomplete; erase partial summary */
	    IniStatcontext( &pmgi->sc );
 	    return -1;
	}
    }
    
    return 0;
}


static void
UpdateVariance( float *prVariance,
                const float rSum,
                const float rSumAdd,
                const int nGames ) {

  if ( ! nGames || nGames == 1 ) {
    *prVariance = 0;
    return;
  }
  else {

    /* See <URL:http://mathworld.wolfram.com/SampleVarianceComputation.html>
       for formula */

    float rDelta = rSumAdd;
    float rMuNew = rSum/nGames;
    float rMuOld = ( rSum - rDelta ) / ( nGames - 1 );
    float rDeltaMu = rMuNew - rMuOld;

    *prVariance = *prVariance * ( 1.0 - 1.0 / ( nGames - 1.0f ) ) +
      nGames * rDeltaMu * rDeltaMu;

    return;

  }

}
      	
extern void
AddStatcontext ( const statcontext *pscA, statcontext *pscB ) {

  /* pscB = pscB + pscA */

  int i, j;

  pscB->nGames++;

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

    for ( j = 0; j < N_SKILLS; j++ )
      pscB->anMoves[ i ][ j ] += pscA->anMoves[ i ][ j ];

    for ( j = 0; j < N_SKILLS; j++ )
      pscB->anLuck[ i ][ j ] += pscA->anLuck[ i ][ j ];

    pscB->anCubeMissedDoubleDP [ i ] += pscA->anCubeMissedDoubleDP [ i ];
    pscB->anCubeMissedDoubleCP [ i ] += pscA->anCubeMissedDoubleCP [ i ];
    pscB->anCubeMissedDoubleTG [ i ] += pscA->anCubeMissedDoubleTG [ i ];
    pscB->anCubeWrongDoubleDP [ i ] += pscA->anCubeWrongDoubleDP [ i ];
    pscB->anCubeWrongDoubleCP [ i ] += pscA->anCubeWrongDoubleCP [ i ];
    pscB->anCubeWrongDoubleTG [ i ] += pscA->anCubeWrongDoubleTG [ i ];
    pscB->anCubeWrongTake [ i ] += pscA->anCubeWrongTake [ i ];
    pscB->anCubeWrongPass [ i ] += pscA->anCubeWrongPass [ i ];

    for ( j = 0; j < 2; j++ ) {

      pscB->arErrorCheckerplay [ i ][ j ] +=
        pscA->arErrorCheckerplay[ i ][ j ];
      pscB->arErrorMissedDoubleDP [ i ][ j ] +=
        pscA->arErrorMissedDoubleDP[ i ][ j ];
      pscB->arErrorMissedDoubleCP [ i ][ j ] +=
        pscA->arErrorMissedDoubleCP[ i ][ j ];
      pscB->arErrorMissedDoubleTG [ i ][ j ] +=
        pscA->arErrorMissedDoubleTG[ i ][ j ];
      pscB->arErrorWrongDoubleDP [ i ][ j ] +=
        pscA->arErrorWrongDoubleDP[ i ][ j ];
      pscB->arErrorWrongDoubleCP [ i ][ j ] +=
        pscA->arErrorWrongDoubleCP[ i ][ j ];
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

  if ( pscA->arActualResult[ 0 ] >= 0.0f || 
       pscA->arActualResult[ 1 ] >= 0.0f ) {
    /* actual result is calculated */

    for ( i = 0; i < 2; ++i ) {
      /* separate loop, else arLuck[ 1 ] is not calculated for i=0 */
      
      pscB->arActualResult[ i ] += pscA->arActualResult[ i ];
      pscB->arLuckAdj[ i ] += pscA->arLuckAdj[ i ];
      UpdateVariance( &pscB->arVarianceActual[ i ], 
                      pscB->arActualResult[ i ],
                      pscA->arActualResult[ i ],
                      pscB->nGames );
      UpdateVariance( &pscB->arVarianceLuckAdj[ i ], 
                      pscB->arLuckAdj[ i ], pscA->arLuckAdj[ i ],
                      pscB->nGames );
      
      
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

  playSound( SOUND_ANALYSIS_FINISHED );

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

  playSound( SOUND_ANALYSIS_FINISHED );

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

    for ( j = 0; j < N_SKILLS; j++ )
      psc->anMoves[ i ][ j ] = 0;

    for ( j = 0; j <= LUCK_VERYGOOD; j++ )
      psc->anLuck[ i ][ j ] = 0;

    psc->anCubeMissedDoubleDP [ i ] = 0;
    psc->anCubeMissedDoubleCP [ i ] = 0;
    psc->anCubeMissedDoubleTG [ i ] = 0;
    psc->anCubeWrongDoubleDP [ i ] = 0;
    psc->anCubeWrongDoubleCP [ i ] = 0;
    psc->anCubeWrongDoubleTG [ i ] = 0;
    psc->anCubeWrongTake [ i ] = 0;
    psc->anCubeWrongPass [ i ] = 0;

    for ( j = 0; j < 2; j++ ) {

      psc->arErrorCheckerplay [ i ][ j ] = 0.0;
      psc->arErrorMissedDoubleDP [ i ][ j ] = 0.0;
      psc->arErrorMissedDoubleCP [ i ][ j ] = 0.0;
      psc->arErrorMissedDoubleTG [ i ][ j ] = 0.0;
      psc->arErrorWrongDoubleDP [ i ][ j ] = 0.0;
      psc->arErrorWrongDoubleCP [ i ][ j ] = 0.0;
      psc->arErrorWrongDoubleTG [ i ][ j ] = 0.0;
      psc->arErrorWrongTake[ i ][ j ] = 0.0;
      psc->arErrorWrongPass[ i ][ j ] = 0.0;
      psc->arLuck[ i ][ j ] = 0.0;

    }

    psc->arActualResult[ i ] = 0.0f;
    psc->arLuckAdj[ i ] = 0.0f;
    psc->arVarianceActual[ i ] = 0.0f;
    psc->arVarianceLuckAdj[ i ] = 0.0f;

  }

  psc->nGames = 0;
}



extern float
relativeFibsRating ( const float r, const int n )
{
  float const x = - 2000.0 / sqrt ( 1.0 * n ) * log10 ( 1.0 / r - 1.0 );

  return ( x < -2100 ) ? -2100 : x;
}

/*
 * Calculated the amount of rating lost by chequer play errors.
 *
 * a2(N) * EPM
 *
 * where a2(N) = 8798 + 25526/N
 *
 */ 

extern float
absoluteFibsRatingChequer( const float rChequer, const int n ) {

  return rChequer * ( 8798.0f + 25526.0f/( n ) );

}


/*
 * Calculated the amount of rating lost by cube play errors.
 *
 * b(N) * EPM
 *
 * where b(N) = 863 - 519/N.
 *
 */ 

extern float
absoluteFibsRatingCube( const float rCube, const int n ) {

  return rCube * ( 863.0f - 519.0f / n );

}


/*
 * Calculate an estimated rating based Kees van den Doels work.
 * (see manual for details)
 *
 * absolute rating = R0 + a2(N)*EPM+b(N)*EPC,
 * where EPM is error rate per move, EPC is the error per cubedecision
 * and a2(N) = 8798 + 2526/N and b(N) = 863 - 519/N.
 *
 */

extern float
absoluteFibsRating ( const float rChequer, const float rCube, 
                     const int n, const float rOffset ) {

  return rOffset - ( absoluteFibsRatingChequer( rChequer, n ) +
                     absoluteFibsRatingCube( rCube, n ) );

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
        + psc->arErrorMissedDoubleCP[ i ][ j ]
        + psc->arErrorMissedDoubleTG[ i ][ j ]
        + psc->arErrorWrongDoubleDP[ i ][ j ]
        + psc->arErrorWrongDoubleCP[ i ][ j ]
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

extern float
calcFibsRating( const float rMWC, const int nMatchTo ) {

  float r;

  r = 0.50f - rMWC;
  if ( r < 0.0f )
    r = 0.0f;

  return 2100 + relativeFibsRating( r, nMatchTo );

}


extern void
DumpStatcontext ( char *szOutput, const statcontext *psc, const char * sz,
                  const int fIsMatch ) {

  /* header */

  sprintf( szOutput, "%-31s %-23s %-23s\n\n",
           _("Player"), ap[ 0 ].szName, ap[ 1 ].szName );

  if ( psc->fMoves ) {
    GList *list = formatGS( psc, &ms, fIsMatch, FORMATGS_CHEQUER );
    GList *pl;

    strcat( szOutput, _("Chequerplay statistics") );
    strcat( szOutput, "\n\n" );

    for ( pl = g_list_first( list ); pl; pl = g_list_next( pl ) ) {

      char **asz = pl->data;

      sprintf( strchr( szOutput, 0 ),
               "%-31s %-23s %-23s\n",
               asz[ 0 ], asz[ 1 ], asz[ 2 ] );

    }

    strcat( szOutput, "\n\n" );

    freeGS( list );

  }


  if ( psc->fDice ) {
    GList *list = formatGS( psc, &ms, fIsMatch, FORMATGS_LUCK );
    GList *pl;

    strcat( szOutput, _("Luck statistics") );
    strcat( szOutput, "\n\n" );

    for ( pl = g_list_first( list ); pl; pl = g_list_next( pl ) ) {

      char **asz = pl->data;

      sprintf( strchr( szOutput, 0 ),
               "%-31s %-23s %-23s\n",
               asz[ 0 ], asz[ 1 ], asz[ 2 ] );

    }

    strcat( szOutput, "\n\n" );

    freeGS( list );

  }


  if ( psc->fCube ) {
    GList *list = formatGS( psc, &ms, fIsMatch, FORMATGS_CUBE );
    GList *pl;

    strcat( szOutput, _("Cube statistics") );
    strcat( szOutput, "\n\n" );

    for ( pl = g_list_first( list ); pl; pl = g_list_next( pl ) ) {

      char **asz = pl->data;

      sprintf( strchr( szOutput, 0 ),
               "%-31s %-23s %-23s\n",
               asz[ 0 ], asz[ 1 ], asz[ 2 ] );

    }

    strcat( szOutput, "\n\n" );

    freeGS( list );

  }

  if ( psc->fCube && psc->fCube ) {
    GList *list = formatGS( psc, &ms, fIsMatch, FORMATGS_OVERALL );
    GList *pl;

    strcat( szOutput, _("Overall statistics") );
    strcat( szOutput, "\n\n" );

    for ( pl = g_list_first( list ); pl; pl = g_list_next( pl ) ) {

      char **asz = pl->data;

      sprintf( strchr( szOutput, 0 ),
               "%-31s %-23s %-23s\n",
               asz[ 0 ], asz[ 1 ], asz[ 2 ] );

    }

    strcat( szOutput, "\n\n" );

    freeGS( list );

  }

}


extern void
CommandShowStatisticsMatch ( char *sz ) {

    char szOutput[4096];

    updateStatisticsMatch ( &lMatch );

#if USE_GTK
    if ( fX ) {
	GTKDumpStatcontext ( 0 );
	return;
    }
#endif

    DumpStatcontext ( szOutput, &scMatch, 
                      _("Statistics for all games"), TRUE );
    outputl(szOutput);
}


extern void
CommandShowStatisticsSession ( char *sz ) {

  CommandShowStatisticsMatch ( sz );

}


extern void
CommandShowStatisticsGame ( char *sz )
{

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
    GTKDumpStatcontext ( getGameNumber (plGame) + 1 );
    return;
  }
#endif

  DumpStatcontext ( szOutput, &pmgi->sc, 
                    _("Statistics for current game"), FALSE );
  outputl( szOutput );
}


extern void
CommandAnalyseMove ( char *sz )
{
  matchstate msx;

  if( ms.gs == GAME_NONE ) {
    outputl( _("No game in progress (type `new game' to start one).") );
    return;
  }

  if ( plLastMove && plLastMove->plNext && plLastMove->plNext->p ) {

    ProgressStart( _("Analysing move...") );

    /* analyse move */

    memcpy ( &msx, &ms, sizeof ( matchstate ) );
    AnalyzeMove ( plLastMove->plNext->p, &msx, plGame, NULL, 
                  &esAnalysisChequer, &esAnalysisCube, aamfAnalysis, 
                  FALSE, NULL );
    ProgressEnd();

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif
  }
  else
    outputl ( _("Sorry, cannot analyse move!") );
}


static void
updateStatisticsMove( const moverecord* pmr,
		      matchstate* pms, const list* plGame,
		      statcontext* psc )
{
  FixMatchState ( pms, pmr );

  switch ( pmr->mt ) {
  case MOVE_GAMEINFO:
    IniStatcontext ( psc );
    updateStatcontext( psc, pmr, pms );
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
updateStatisticsGame ( const list* plGame ) {

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

    pmr->d.CubeDecPtr->esDouble.et = EVAL_NONE;
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

