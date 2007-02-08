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

#include <glib.h>
#include <string.h>
#include <stdlib.h>

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
#include <glib/gi18n.h>
#if USE_MULTITHREAD
#include "multithread.h"
#endif

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
  1e38f, 0.035f, 0.026f, 0.018f, 0.012f, 0.008f, 0.005f, 0.002f };
/* 1e38, 0.060, 0.030, 0.025, 0.020, 0.015, 0.010, 0.005 }; */

int afAnalysePlayers[ 2 ] = { TRUE, TRUE };

#if defined (REDUCTION_CODE)
evalcontext ecLuck = { TRUE, 0, 0, TRUE, 0.0 };
#else
evalcontext ecLuck = { TRUE, 0, FALSE, TRUE, 0.0 };
#endif

extern ratingtype
GetRating ( const float rError ) {

  int i;

  for ( i = RAT_SUPERNATURAL; i >= 0; i-- )
    if ( rError < arThrsRating[ i ] )
		return (ratingtype)i;

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
        return (float)ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, &ciOpp, 
                                  (evalcontext *) pec ) < 0 )
          return (float)ERR_VAL;

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
        return (float)ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, (cubeinfo *) pci, 
                                  (evalcontext *) pec ) < 0 )
          return (float)ERR_VAL;

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
  float aar[ 6 ][ 6 ], ar[ NUM_ROLLOUT_OUTPUTS ], rMean = 0.0f;
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
        return (float)ERR_VAL;

      if ( ! ml.cMoves ) {
      
        SwapSides( anBoardTemp );
      
        if ( GeneralEvaluationE ( ar, anBoardTemp, &ciOpp, 
                                  (evalcontext *) pec ) < 0 )
          return (float)ERR_VAL;

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
Skill( float r )
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
		  const matchstate*  pms,
                  const list *plGame )
{
  cubeinfo ci;
  static unsigned char auch[ 10 ];
  float rSkill, rChequerSkill, rCost;
  unsigned int i;
  int anBoardMove[ 2 ][ 25 ];
  float arDouble[ 4 ];
  const xmovegameinfo* pmgi = &((moverecord *) plGame->plNext->p)->g;

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
        psc->arActualResult[ pmr->g.fWinner ] = (float)pmr->g.nPoints;
        psc->arActualResult[ ! pmr->g.fWinner ] = (float)-pmr->g.nPoints;
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
      
    if ( pmr->CubeDecPtr->esDouble.et != EVAL_NONE && 
         fAnalyseCube && pmgi->fCubeUse ) {

      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->CubeDecPtr->aarOutput, &ci );

      psc->anTotalCube[ pmr->fPlayer ]++;

      /* Count doubles less than very bad */
      if ( isCloseCubedecision ( arDouble ) ) psc->anCloseCube[ pmr->fPlayer ]++;

      if( arDouble[ OUTPUT_NODOUBLE ] <
          arDouble[ OUTPUT_OPTIMAL ] ) {
        /* it was a double */

        rSkill = arDouble[ OUTPUT_NODOUBLE ] -
          arDouble[ OUTPUT_OPTIMAL ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;

        if ( arDouble[ OUTPUT_TAKE ] > 1.0f ) {
          /* missed double above cash point */
          psc->anCubeMissedDoubleTG[ pmr->fPlayer ]++;
          psc->arErrorMissedDoubleTG[ pmr->fPlayer ][ 0 ] -=
            rSkill;
          psc->arErrorMissedDoubleTG[ pmr->fPlayer ][ 1 ] -=
            rCost;
        } 
        else {
          /* missed double below cash point */
          psc->anCubeMissedDoubleDP[ pmr->fPlayer ]++;
          psc->arErrorMissedDoubleDP[ pmr->fPlayer ][ 0 ] -= rSkill;
          psc->arErrorMissedDoubleDP[ pmr->fPlayer ][ 1 ] -= rCost;
        }

      } /* missed double */	

    } /* EVAL_NONE */


    /*
     * update luck statistics for roll
     */

    if ( fAnalyseDice && pmr->rLuck != (float)ERR_VAL ) {

      float r = pms->nMatchTo ?
        eq2mwc( pmr->rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
        pms->nCube * pmr->rLuck;

      psc->arLuck[ pmr->fPlayer ][ 0 ] += pmr->rLuck;
      psc->arLuck[ pmr->fPlayer ][ 1 ] += r;

      psc->arLuckAdj[ pmr->fPlayer ] -= r;
      psc->arLuckAdj[ ! pmr->fPlayer ] += r;
      
      psc->anLuck[ pmr->fPlayer ][ pmr->lt ]++;

    }


    /*
     * update chequerplay statistics 
     */

    /* Count total regradless of analysis */
    psc->anTotalMoves[ pmr->fPlayer ]++;

    /* hmm, MOVE_NORMALs which has no legal moves have
       pmr->n.esChequer.et == EVAL_NONE
       (Joseph) But we can detect them by checking if a move was actually
       made or not.
    */
    
    if( fAnalyseMove &&
	 (pmr->esChequer.et != EVAL_NONE || pmr->n.anMove[0] < 0) ) {

      /* find skill */

      memcpy( anBoardMove, pms->anBoard, sizeof( anBoardMove ) );
      ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
      PositionKey ( anBoardMove, auch );
      rChequerSkill = 0.0f;
	  
      for( i = 0; i < pmr->ml.cMoves; i++ ) 

        if( EqualKeys(auch, pmr->ml.amMoves[ i ].auch) ) {

          rChequerSkill =
            pmr->ml.amMoves[ i ].rScore - pmr->ml.amMoves[ 0 ].rScore;
          
          break;
        }

      /* update statistics */
	  
      rCost = pms->nMatchTo ? eq2mwc( rChequerSkill, &ci ) -
        eq2mwc( 0.0f, &ci ) : pms->nCube * rChequerSkill;

      psc->anMoves[ pmr->fPlayer ][ Skill( rChequerSkill ) ]++;
	  
      if( pmr->ml.cMoves > 1 ) {
        psc->anUnforcedMoves[ pmr->fPlayer ]++;
        psc->arErrorCheckerplay[ pmr->fPlayer ][ 0 ] -= rChequerSkill;
        psc->arErrorCheckerplay[ pmr->fPlayer ][ 1 ] -= rCost;
      }

    } else {
      /* unmarked move */
      psc->anMoves[ pmr->fPlayer ][ SKILL_NONE ] ++;
    }

    break;

  case MOVE_DOUBLE:

    GetMatchStateCubeInfo ( &ci, pms );
    if ( fAnalyseCube && pmgi->fCubeUse && 
         pmr->CubeDecPtr->esDouble.et != EVAL_NONE ) {

      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->CubeDecPtr->aarOutput, &ci );

      rSkill = arDouble[ OUTPUT_TAKE ] <
        arDouble[ OUTPUT_DROP ] ?
        arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
        arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];
	      
      psc->anTotalCube[ pmr->fPlayer ]++;
      psc->anDouble[ pmr->fPlayer ]++;
      psc->anCloseCube[ pmr->fPlayer ]++;

      if ( rSkill < 0.0f ) {
        /* it was not a double */

        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;

        if ( arDouble[ OUTPUT_NODOUBLE ] > 1.0f ) {
          /* wrong double above too good point */
          psc->anCubeWrongDoubleTG[ pmr->fPlayer ]++;
          psc->arErrorWrongDoubleTG[ pmr->fPlayer ][ 0 ] -= rSkill;
          psc->arErrorWrongDoubleTG[ pmr->fPlayer ][ 1 ] -= rCost;
        }
        else {
          /* wrong double below double point */
          psc->anCubeWrongDoubleDP[ pmr->fPlayer ]++;
          psc->arErrorWrongDoubleDP[ pmr->fPlayer ][ 0 ] -= rSkill;
          psc->arErrorWrongDoubleDP[ pmr->fPlayer ][ 1 ] -= rCost;
        }
      }

    }

    break;

  case MOVE_TAKE:

    GetMatchStateCubeInfo ( &ci, pms );
    if ( fAnalyseCube && pmgi->fCubeUse && 
         pmr->CubeDecPtr->esDouble.et != EVAL_NONE ) {

      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->CubeDecPtr->aarOutput, &ci );

      psc->anTotalCube[ pmr->fPlayer ]++;
      psc->anTake[ pmr->fPlayer ]++;
      psc->anCloseCube[ pmr->fPlayer ]++;
	  
      if( -arDouble[ OUTPUT_TAKE ] < -arDouble[ OUTPUT_DROP ] ) {

        /* it was a drop */

        rSkill = -arDouble[ OUTPUT_TAKE ] - -arDouble[ OUTPUT_DROP ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
	      
        psc->anCubeWrongTake[ pmr->fPlayer ]++;
        psc->arErrorWrongTake[ pmr->fPlayer ][ 0 ] -= rSkill;
        psc->arErrorWrongTake[ pmr->fPlayer ][ 1 ] -= rCost;
      }
    }

    break;

  case MOVE_DROP:

    GetMatchStateCubeInfo ( &ci, pms );
    if( fAnalyseCube && pmgi->fCubeUse && 
        pmr->CubeDecPtr->esDouble.et != EVAL_NONE ) {
	  
      FindCubeDecision( arDouble, 
                        GCCCONSTAHACK pmr->CubeDecPtr->aarOutput, &ci );

      psc->anTotalCube[ pmr->fPlayer ]++;
      psc->anPass[ pmr->fPlayer ]++;
      psc->anCloseCube[ pmr->fPlayer ]++;
	  
      if( -arDouble[ OUTPUT_DROP ] < -arDouble[ OUTPUT_TAKE ] ) {

        /* it was a take */
        
        rSkill = -arDouble[ OUTPUT_DROP ] - -arDouble[ OUTPUT_TAKE ];
	      
        rCost = pms->nMatchTo ? eq2mwc( rSkill, &ci ) -
          eq2mwc( 0.0f, &ci ) : pms->nCube * rSkill;
	      
        psc->anCubeWrongPass[ pmr->fPlayer ]++;
        psc->arErrorWrongPass[ pmr->fPlayer ][ 0 ] -= rSkill;
        psc->arErrorWrongPass[ pmr->fPlayer ][ 1 ] -= rCost;
      }
    }

    break;

  case MOVE_SETDICE:

    /*
     * update luck statistics for roll
     */


    GetMatchStateCubeInfo ( &ci, pms );
    if ( fAnalyseDice && pmr->rLuck != (float)ERR_VAL ) {

      float r = pms->nMatchTo ?
        eq2mwc( pmr->rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
        pms->nCube * pmr->rLuck;

      psc->arLuck[ pmr->fPlayer ][ 0 ] += pmr->rLuck;
      psc->arLuck[ pmr->fPlayer ][ 1 ] += r;

      psc->arLuckAdj[ pmr->fPlayer ] -= r;
      psc->arLuckAdj[ ! pmr->fPlayer ] += r;
      
      psc->anLuck[ pmr->fPlayer ][ pmr->lt ]++;

    }

    break;

  case MOVE_TIME:

#if USE_TIMECONTROL

    if ( pmr->t.es.et != EVAL_NONE ) {

      float d = 
        pmr->t.aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] -
        pmr->t.aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ];
      float r;

      GetMatchStateCubeInfo ( &ci, pms );

      if ( pms->nMatchTo )
        r = 
          mwc2eq( pmr->t.aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ], &ci ) -
          mwc2eq( pmr->t.aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ], &ci );
      else
        r = pms->nCube * d;

      psc->anTimePenalties[ pmr->fPlayer ]++;

      psc->aarTimeLoss[ pmr->fPlayer ][ 0 ] += r;
      psc->aarTimeLoss[ pmr->fPlayer ][ 1 ] += d;

    }

#endif /* USE_TIMECONTROL */

    break;

  default:

    break;

  } /* switch */

}

extern int
AnalyzeMove (moverecord *pmr, matchstate *pms, const list *plParentGame,
		statcontext *psc, const evalsetup *pesChequer, evalsetup *pesCube,
		/* const */ movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ], const int analysePlayers[ 2 ],
		float *doubleError)
{
    int anBoardMove[ 2 ][ 25 ];
    cubeinfo ci;
    float rSkill, rChequerSkill;
    float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

    doubletype dt;
    taketype tt;
    const xmovegameinfo* pmgi = &((moverecord *) plParentGame->plNext->p)->g;
    int    is_initial_position = 1;

    /* analyze this move */

    FixMatchState ( pms, pmr );

    /* check if it's the initial position: no cube analysis and special
       luck analysis */

    if ( pmr->mt != MOVE_GAMEINFO ) {
      InitBoard( anBoardMove, pms->bgv );
      is_initial_position = 
        ! memcmp( anBoardMove, pms->anBoard, 2 * 25 * sizeof ( int ) );
    }

    switch( pmr->mt ) {
    case MOVE_GAMEINFO:

        if ( psc ) {
          IniStatcontext( psc );
          updateStatcontext ( psc, pmr, pms, plParentGame );
        }
		break;
      
    case MOVE_NORMAL:
		if( pmr->fPlayer != pms->fMove ) {
			SwapSides( pms->anBoard );
			pms->fMove = pmr->fPlayer;
		}

		if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
			break;

		rSkill = rChequerSkill = 0.0f;
		GetMatchStateCubeInfo( &ci, pms );

		/* cube action? */

		if ( ! is_initial_position && fAnalyseCube && 
				pmgi->fCubeUse && GetDPEq ( NULL, NULL, &ci ) )
		{
			float arDouble[ NUM_CUBEFUL_OUTPUTS ];

			if ( cmp_evalsetup ( pesCube, &pmr->CubeDecPtr->esDouble ) > 0 )
			{
				if ( GeneralCubeDecision ( aarOutput, aarStdDev, NULL, 
					pms->anBoard, &ci, pesCube, NULL, NULL  ) < 0 )
					return -1;

				pmr->CubeDecPtr->esDouble = *pesCube;

				memcpy ( pmr->CubeDecPtr->aarOutput, 
					aarOutput, sizeof ( aarOutput ) );
				memcpy ( pmr->CubeDecPtr->aarStdDev, 
					aarStdDev, sizeof ( aarStdDev ) );
			}

			FindCubeDecision( arDouble, GCCCONSTAHACK pmr->CubeDecPtr->aarOutput, &ci );

			rSkill = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_OPTIMAL ];
			pmr->stCube = Skill( rSkill );

		}
		else
			pmr->CubeDecPtr->esDouble.et = EVAL_NONE;

        /* luck analysis */
      
		if( fAnalyseDice )
		{
			pmr->rLuck = LuckAnalysis( pms->anBoard,
						pmr->anDice[ 0 ],
						pmr->anDice[ 1 ],
						&ci, is_initial_position );
			pmr->lt = Luck( pmr->rLuck );
		}
      
		/* evaluate move */
	      
		if( fAnalyseMove )
		{
			unsigned char auch[ 10 ];

			/* evaluate move */

			memcpy( anBoardMove, pms->anBoard,
				sizeof( anBoardMove ) );
			ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
			PositionKey ( anBoardMove, auch );
		  
				if ( cmp_evalsetup ( pesChequer,
									&pmr->esChequer ) > 0 ) {

				if( pmr->ml.cMoves )
					free( pmr->ml.amMoves );
		  
				/* find best moves */
		  
				if( FindnSaveBestMoves ( &(pmr->ml), pmr->anDice[ 0 ],
										pmr->anDice[ 1 ],
										pms->anBoard, auch, 
										arSkillLevel[ SKILL_DOUBTFUL ],
										&ci, &pesChequer->ec, aamf ) < 0 )
						return -1;

				}
		  
			for( pmr->n.iMove = 0; pmr->n.iMove < pmr->ml.cMoves;
			pmr->n.iMove++ )
			if( EqualKeys( auch,
					pmr->ml.amMoves[ pmr->n.iMove ].auch ) ) {
				rChequerSkill = pmr->ml.amMoves[ pmr->n.iMove ].
				rScore - pmr->ml.amMoves[ 0 ].rScore;
			  
				break;
			}
		  
			pmr->n.stMove = Skill( rChequerSkill );
		  
			if( cAnalysisMoves >= 2 &&
			pmr->ml.cMoves > cAnalysisMoves ) {
				/* There are more legal moves than we want;
				throw some away. */
				if( pmr->n.iMove >= cAnalysisMoves ) {
					/* The move made wasn't in the top n; move it up so it
					won't be discarded. */
					memcpy( pmr->ml.amMoves + cAnalysisMoves - 1,
						pmr->ml.amMoves + pmr->n.iMove,
						sizeof( move ) );
					pmr->n.iMove = cAnalysisMoves - 1;
				}
			      
				pmr->ml.amMoves = (move *)realloc( pmr->ml.amMoves,
								cAnalysisMoves * sizeof( move ) );
				pmr->ml.cMoves = cAnalysisMoves;
			}

			pmr->esChequer = *pesChequer;
		}
      
		if ( psc )
		      updateStatcontext ( psc, pmr, pms, plParentGame );
	  
		break;
      
    case MOVE_DOUBLE:

		/* always analyse MOVE_DOUBLEs as they are shared with the subsequent
		MOVE_TAKEs or MOVE_DROPs. */

		dt = DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );

		if ( dt != DT_NORMAL )
			break;

		/* cube action */	    
		if( fAnalyseCube && pmgi->fCubeUse )
		{
			GetMatchStateCubeInfo( &ci, pms );
		  
			if ( GetDPEq ( NULL, NULL, &ci ) ||
					ci.fCubeOwner < 0 || ci.fCubeOwner == ci.fMove )
			{
				float arDouble[ NUM_CUBEFUL_OUTPUTS ];

				if ( cmp_evalsetup ( pesCube, &pmr->CubeDecPtr->esDouble ) > 0 )
				{
					if ( GeneralCubeDecision ( aarOutput, aarStdDev, 
							NULL, pms->anBoard, &ci,
							pesCube, NULL, NULL ) < 0 )
						return -1;

					pmr->CubeDecPtr->esDouble = *pesCube;
				}
				else
				{
					memcpy ( aarOutput, pmr->CubeDecPtr->aarOutput, 
						sizeof ( aarOutput ) );
					memcpy ( aarStdDev, pmr->CubeDecPtr->aarStdDev, 
						sizeof ( aarStdDev ) );
				}

				FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );
				if (doubleError)
					*doubleError = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_DROP ];

				memcpy ( pmr->CubeDecPtr->aarOutput, aarOutput, 
					sizeof ( aarOutput ) );
				memcpy ( pmr->CubeDecPtr->aarStdDev, aarStdDev, 
					sizeof ( aarStdDev ) );

				rSkill = arDouble[ OUTPUT_TAKE ] <
					arDouble[ OUTPUT_DROP ] ?
					arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
					arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];

				pmr->stCube = Skill( rSkill );
			}
			else if (doubleError)
				*doubleError = (float)ERR_VAL;
		}

		if ( psc )
			updateStatcontext ( psc, pmr, pms, plParentGame );
	
		break;
      
    case MOVE_TAKE:

        if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
          /* we do not analyse this player */
          break;
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;

		if( fAnalyseCube && pmgi->fCubeUse && doubleError && (*doubleError != (float)ERR_VAL))
		{
			GetMatchStateCubeInfo( &ci, pms );
			pmr->stCube = Skill (-*doubleError);
		}

		if ( psc )
			updateStatcontext ( psc, pmr, pms, plParentGame );

		break;
      
    case MOVE_DROP:
      
        if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
          /* we do not analyse this player */
          break;
      
        tt = (taketype) DoubleType ( pms->fDoubled, pms->fMove, pms->fTurn );
        if ( tt != TT_NORMAL )
          break;

		if( fAnalyseCube && pmgi->fCubeUse && doubleError && (*doubleError != (float)ERR_VAL) )
		{
			GetMatchStateCubeInfo( &ci, pms );
			pmr->stCube = Skill( *doubleError );
		}

        if ( psc )
          updateStatcontext ( psc, pmr, pms, plParentGame );

		break;
      
    case MOVE_RESIGN:

      /* swap board if player not on roll resigned */

      if( pmr->fPlayer != pms->fMove ) {
        SwapSides( pms->anBoard );
        pms->fMove = pmr->fPlayer;
      }
      
      if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
        /* we do not analyse this player */
        break;
      
      if ( pesCube->et != EVAL_NONE ) {
        
        float rBefore, rAfter;

        GetMatchStateCubeInfo ( &ci, pms );

        if ( cmp_evalsetup ( pesCube, &pmr->r.esResign ) > 0 ) {

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
          pmr->r.stAccept = SKILL_GOOD; /* VERYGOOD */
        }

        if ( rBefore < rAfter ) {
          /* wrong accept */
          pmr->r.stAccept = Skill ( rBefore - rAfter );
          pmr->r.stResign = SKILL_GOOD; /* VERYGOOD */
        }


      }

      break;
      
    case MOVE_SETDICE:
		if( pmr->fPlayer != pms->fMove ) {
			SwapSides( pms->anBoard );
			pms->fMove = pmr->fPlayer;
		}
      
        if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
          /* we do not analyse this player */
          break;
      
		GetMatchStateCubeInfo( &ci, pms );
	      
		if( fAnalyseDice ) {
			pmr->rLuck = LuckAnalysis( pms->anBoard,
						pmr->anDice[ 0 ],
						pmr->anDice[ 1 ],
						&ci, is_initial_position );
			pmr->lt = Luck( pmr->rLuck );
		}

        if ( psc )
          updateStatcontext ( psc, pmr, pms, plParentGame);

		break;

    case MOVE_TIME: 

      {
        
#if USE_TIMECONTROL
        if ( analysePlayers && ! analysePlayers[ pmr->fPlayer ] )
          /* we do not analyse this player */
          break;

        /* analyse position before loss of points */

        memcpy( &pmr->t.es, pesCube, sizeof ( evalsetup ) );

        GetMatchStateCubeInfo ( &ci, pms );

        if ( GeneralEvaluation ( pmr->t.aarOutput[ 0 ], pmr->t.aarStdDev[ 0 ],
                                 NULL,
                                 pms->anBoard,
                                 &ci, pesCube, NULL, NULL ) < 0 )
          return -1;

        /* analyse position after loss of points */
      
        ci.anScore[ !pmr->fPlayer ] += pmr->t.nPoints;

        if ( ci.anScore[ !pmr->fPlayer ] >= ci.nMatchTo ) {

          /* the match is lost */

          /* copy w/g/bg distribution from "before" */

          memcpy( pmr->t.aarOutput[ 1 ], pmr->t.aarOutput[ 0 ], 
                  NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
          memcpy( pmr->t.aarStdDev[ 1 ], pmr->t.aarStdDev[ 0 ], 
                  NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

          /* MWC after loss is nil */

          if ( pms->nMatchTo ) 
                                 pmr->t.aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] = 
            ( pmr->fPlayer == ci.fMove ) ? 0.0f : 1.0f;
          pmr->t.aarStdDev[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] = 0.0;

        }
        else {

          /* the match continues */

          SetCubeInfo( &ci, ci.nCube, ci.fCubeOwner, ci.fMove, ci.nMatchTo,
                       ci.anScore, ci.fCrawford, 
                       ci.fJacoby, ci.fBeavers, ci.bgv );
      
          if ( GeneralEvaluation ( pmr->t.aarOutput[ 1 ], 
                                   pmr->t.aarStdDev[ 1 ],
                                   NULL,
                                   pms->anBoard,
                                   &ci, pesCube, NULL, NULL ) < 0 )
            return -1;

        }
      
        /* update statistics */
      
        if ( psc )
          updateStatcontext ( psc, pmr, pms, plParentGame );
      
#endif /* USE_TIMECONTROL */
      
      }

      break;
      
    case MOVE_SETBOARD:	  
    case MOVE_SETCUBEVAL:
    case MOVE_SETCUBEPOS:
		break;
    }
  
    ApplyMoveRecord( pms, plParentGame, pmr );
  
    if ( psc ) {
      psc->fMoves = fAnalyseMove;
      psc->fCube = fAnalyseCube;
      psc->fDice = fAnalyseDice;
    }
  
    return fInterrupt ? -1 : 0;
}

static int NumberMovesGame ( list *plGame );

#if USE_MULTITHREAD

static void UpdateProgressBar()
{
	ProgressValue(MT_GetDoneTasks());
}

static int AnalyzeGame ( list *plGame )
{
	int result;
	unsigned int i;
    list *pl = plGame->plNext;
    moverecord *pmr = pl->p;
	statcontext *psc = &pmr->g.sc;
    matchstate msAnalyse;
	unsigned int numMoves = NumberMovesGame(plGame);
	AnalyseMoveTask *pt = NULL, *pParentTask = NULL;

	void ( *fnOld )( void ) = fnTick;
	fnTick = NULL;

	/* Analyse first move record (gameinfo) */
    g_assert( pmr->mt == MOVE_GAMEINFO );
    if	(AnalyzeMove(pmr, &msAnalyse, plGame, psc,
                        &esAnalysisChequer, &esAnalysisCube, aamfAnalysis, afAnalysePlayers, NULL ) < 0 )
		return -1;	/* Interupted */

	numMoves--;	/* Done one - the gameinfo */


    for (i = 0; i < numMoves; i++)
	{
		pl = pl->plNext;
		pmr = pl->p;

		if (!pParentTask)
			pt = (AnalyseMoveTask*)malloc(sizeof(AnalyseMoveTask));

		pt->task.type = TT_ANALYSEMOVE;
		pt->task.pLinkedTask = NULL;
		pt->pmr = pmr;
		pt->plGame = plGame;
		pt->psc = psc;
		memcpy(&pt->ms, &msAnalyse, sizeof(msAnalyse));

		if (pmr->mt == MOVE_DOUBLE)
		{
		    moverecord *pNextmr = (moverecord *)pl->plNext->p;
			if (pNextmr &&
				(pNextmr->mt == MOVE_DROP || pNextmr->mt == MOVE_TAKE))
			{	/* Need to link the two tasks so executed together */
				pParentTask = pt;
				pt = (AnalyseMoveTask*)malloc(sizeof(AnalyseMoveTask));
				pParentTask->task.pLinkedTask = (Task*)pt;
			}
		}
		else
		{
			if (pParentTask)
			{
				pt = pParentTask;
				pParentTask = NULL;
			}
			MT_AddTask((Task*)pt);
		}

		FixMatchState(&msAnalyse, pmr);
		if ((pmr->fPlayer != msAnalyse.fMove) && (pmr->mt == MOVE_NORMAL || pmr->mt == MOVE_RESIGN || pmr->mt == MOVE_SETDICE))
		{
			SwapSides( msAnalyse.anBoard );
			msAnalyse.fMove = pmr->fPlayer;
		}
		ApplyMoveRecord(&msAnalyse, plGame, pmr);
	}
	g_assert(pl->plNext == plGame);

	result = MT_WaitForTasks(UpdateProgressBar, 250);

	fnTick = fnOld;

	if (result == -1)
	    IniStatcontext( psc );

    return result;
}

#else

static int AnalyzeGame ( list *plGame )
{
    list *pl;
    moverecord *pmr;
    moverecord *pmrx = (moverecord *) plGame->plNext->p; 
    matchstate msAnalyse;
	float doubleError;

    g_assert( pmrx->mt == MOVE_GAMEINFO );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;

	ProgressValueAdd( 1 );

        if( AnalyzeMove ( pmr, &msAnalyse, plGame, &pmrx->g.sc, 
                          &esAnalysisChequer,
                          &esAnalysisCube, aamfAnalysis,
                          afAnalysePlayers, &doubleError ) < 0 ) {
	    /* analysis incomplete; erase partial summary */
	    IniStatcontext( &pmrx->g.sc );
 	    return -1;
	}
    }
    
    return 0;
}
#endif


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

    *prVariance = *prVariance * ( 1.0f - 1.0f / ( nGames - 1.0f ) ) +
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

#if USE_TIMECONTROL
    pscB->anTimePenalties[ i ] += pscA->anTimePenalties[ i ];
#endif /* USE_TIMECONTROL */

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

#if USE_TIMECONTROL
      pscB->aarTimeLoss[ i ][ j ] += pscA->aarTimeLoss[ i ][ j ];
#endif /* USE_TIMECONTROL */

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


extern void CommandAnalyseMatch( char *sz )
{
  list *pl;
  moverecord *pmr;
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
      pmr = (moverecord *) ( (list *) pl->p )->plNext->p;
      g_assert( pmr->mt == MOVE_GAMEINFO );
      AddStatcontext( &pmr->g.sc, &scMatch );
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

#if USE_TIMECONTROL
      psc->aarTimeLoss[ i ][ j ] = 0.0f;
#endif /* USE_TIMECONTROL */

    }

    psc->arActualResult[ i ] = 0.0f;
    psc->arLuckAdj[ i ] = 0.0f;
    psc->arVarianceActual[ i ] = 0.0f;
    psc->arVarianceLuckAdj[ i ] = 0.0f;

#if USE_TIMECONTROL
    psc->anTimePenalties[ i ] = 0;
#endif /* USE_TIMECONTROL */

  }

  psc->nGames = 0;

}



extern float
relativeFibsRating ( float r, int n )
{
  float const x = - 2000.0f / (float) sqrt ( 1.0 * n ) * 
    (float) log10 ( 1.0 / r - 1.0 );

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


extern void
getMWCFromError ( const statcontext *psc, float aaaar[ 3 ][ 2 ][ 2 ][ 2 ] ) {

  int i, j;

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
DumpStatcontext ( char *szOutput, const statcontext *psc, const char * pl, const char * op, const char * sz,
                  const int fIsMatch ) {

  /* header */

  sprintf( szOutput, "%-31s %-23s %-23s\n\n",
           _("Player"), pl, op );

  if ( psc->fMoves ) {
    GList *list = formatGS( psc, ms.nMatchTo, fIsMatch, FORMATGS_CHEQUER );
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
    GList *list = formatGS( psc, ms.nMatchTo, fIsMatch, FORMATGS_LUCK );
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
    GList *list = formatGS( psc, ms.nMatchTo, fIsMatch, FORMATGS_CUBE );
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

  {

    GList *list = formatGS( psc, ms.nMatchTo, fIsMatch, FORMATGS_OVERALL );
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

    DumpStatcontext ( szOutput, &scMatch, ap[0].szName, ap[1].szName,
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

  moverecord *pmr;
  char szOutput[4096];
    
  if( !plGame ) {
    outputl( _("No game is being played.") );
    return;
  }

  updateStatisticsGame ( plGame );

  pmr = plGame->plNext->p;

  g_assert( pmr->mt == MOVE_GAMEINFO );
    
#if USE_GTK
  if ( fX ) {
    GTKDumpStatcontext ( getGameNumber (plGame) + 1 );
    return;
  }
#endif

  DumpStatcontext ( szOutput, &pmr->g.sc, ap[0].szName, ap[1].szName,
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
                  NULL, NULL );
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
    updateStatcontext( psc, pmr, pms, plGame );
    break;

  case MOVE_NORMAL:
    if( pmr->fPlayer != pms->fMove ) {
      SwapSides( pms->anBoard );
      pms->fMove = pmr->fPlayer;
    }
      
    updateStatcontext ( psc, pmr, pms, plGame );
    break;

  case MOVE_DOUBLE:
    if( pmr->fPlayer != pms->fMove ) {
      SwapSides( pms->anBoard );
      pms->fMove = pmr->fPlayer;
    }
      
      
    updateStatcontext ( psc, pmr, pms, plGame );
    break;

  case MOVE_TAKE:
  case MOVE_DROP:
  case MOVE_TIME:

    updateStatcontext ( psc, pmr, pms, plGame );
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
  moverecord *pmrx = plGame->plNext->p;
  matchstate msAnalyse;
  
  g_assert( pmrx->mt == MOVE_GAMEINFO );
    
  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
    
    pmr = pl->p;
    
    updateStatisticsMove ( pmr, &msAnalyse, plGame, &pmrx->g.sc );

  }
    
}





extern void
updateStatisticsMatch ( list *plMatch ) {

  list *pl;
  moverecord *pmr;

  if( ListEmpty( plMatch ) ) 
    /* no match in progress */
    return;

  IniStatcontext( &scMatch );
  
  for( pl = plMatch->plNext; pl != plMatch; pl = pl->plNext ) {
    
    updateStatisticsGame( pl->p );
    
    pmr = ( (list *) pl->p )->plNext->p;
    g_assert( pmr->mt == MOVE_GAMEINFO );
    AddStatcontext( &pmr->g.sc, &scMatch );

  }

}



extern int getLuckRating ( float rLuck ) {

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

    pmr->CubeDecPtr->esDouble.et = pmr->esChequer.et = EVAL_NONE;
    pmr->n.stMove = pmr->stCube = SKILL_NONE;
    pmr->rLuck = (float)ERR_VAL;
    pmr->lt = LUCK_NONE;
    if ( pmr->ml.amMoves ) {
      free ( pmr->ml.amMoves );
      pmr->ml.amMoves = NULL;
    }
    pmr->ml.cMoves = 0;
    break;

  case MOVE_DOUBLE:
  case MOVE_TAKE:
  case MOVE_DROP:

    pmr->CubeDecPtr->esDouble.et = EVAL_NONE;
    pmr->stCube = SKILL_NONE;
    break;
      
  case MOVE_RESIGN:

    pmr->r.esResign.et = EVAL_NONE;
    pmr->r.stResign = pmr->r.stAccept = SKILL_NONE;
    break;

  case MOVE_SETDICE:

    pmr->lt = LUCK_NONE;
    pmr->rLuck = (float)ERR_VAL;
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

