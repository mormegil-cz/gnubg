/*
 * html.c
 *
 * by Joern Thyssen  <jthyssen@dk.ibm.com>, 2002
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#include "backgammon.h"
#include "drawboard.h"
#include "export.h"
#include "eval.h"
#include "positionid.h"
#include "matchid.h"

#include "i18n.h"


/* "Color" of chequers */

static char *aszColorName[] = { "O", "X" };


static void
printTextBoard ( FILE *pf, const matchstate *pms ) {

  int anBoard[ 2 ][ 25 ];
  char szBoard[ 2048 ];
  char sz[ 32 ], szCube[ 32 ], szPlayer0[ 35 ], szPlayer1[ 35 ],
    szScore0[ 35 ], szScore1[ 35 ], szMatch[ 35 ];
  char *apch[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  apch[ 0 ] = szPlayer0;
  apch[ 6 ] = szPlayer1;

  if ( pms->anScore[ 0 ] == 1 )
    sprintf( apch[ 1 ] = szScore0, _("%d point"), pms->anScore[ 0 ] );
  else
    sprintf( apch[ 1 ] = szScore0, _("%d points"), pms->anScore[ 0 ] );

  if ( pms->anScore[ 1 ] == 1 )
    sprintf( apch[ 5 ] = szScore1, _("%d point"), pms->anScore[ 1 ] );
  else
    sprintf( apch[ 5 ] = szScore1, _("%d points"), pms->anScore[ 1 ] );

  if( pms->fDoubled ) {
    apch[ pms->fTurn ? 4 : 2 ] = szCube;

    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );
    sprintf( szCube, _("Cube offered at %d"), pms->nCube << 1 );
  } else {
    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

    apch[ pms->fMove ? 4 : 2 ] = sz;
	
    if( pms->anDice[ 0 ] )
      sprintf( sz, 
               _("Rolled %d%d"), pms->anDice[ 0 ], pms->anDice[ 1 ] );
    else if( !GameStatus( anBoard ) )
      strcpy( sz, _("On roll") );
    else
      sz[ 0 ] = 0;
	    
    if( pms->fCubeOwner < 0 ) {
      apch[ 3 ] = szCube;

      if( pms->nMatchTo )
        sprintf( szCube, 
                 _("%d point match (Cube: %d)"), pms->nMatchTo,
                 pms->nCube );
      else
        sprintf( szCube, _("(Cube: %d)"), pms->nCube );
    } else {
      int cch = strlen( ap[ pms->fCubeOwner ].szName );
		
      if( cch > 20 )
        cch = 20;
		
      sprintf( szCube, _("%c: %*s (Cube: %d)"), pms->fCubeOwner ? 'X' :
               'O', cch, ap[ pms->fCubeOwner ].szName, pms->nCube );

      apch[ pms->fCubeOwner ? 6 : 0 ] = szCube;

      if( pms->nMatchTo )
        sprintf( apch[ 3 ] = szMatch, _("%d point match"),
                 pms->nMatchTo );
    }
  }
    
  if( pms->fResigned )
    sprintf( strchr( sz, 0 ), _(", resigns %s"),
             gettext ( aszGameResult[ pms->fResigned - 1 ] ) );
	
  if( !pms->fMove )
    SwapSides( anBoard );
	

  fputs ( DrawBoard( szBoard, anBoard, pms->fMove, apch,
                     MatchIDFromMatchState ( &ms ) ),
          pf);
  fputs ( "\n", pf );

}


/*
 * Print html header for board: move or cube decision 
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *   iMove: move no.
 *
 */

static void 
TextBoardHeader ( FILE *pf, const matchstate *pms, 
                  const int iGame, const int iMove ) {

  fprintf ( pf, _("Move number %d: "), iMove + 1 );

  if ( pms->fResigned ) 
    
    /* resignation */

    fprintf ( pf,
              _("%s resigns %d points\n\n"), 
              aszColorName[ pms->fTurn ],
              pms->fResigned * pms->nCube
            );
  
  else if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] )

    /* chequer play decision */

    fprintf ( pf,
              _("%s to play %d%d\n\n"),
              aszColorName[ pms->fTurn ],
              pms->anDice[ 0 ], pms->anDice[ 1 ] 
            );

  else if ( pms->fDoubled )

    /* take decision */

    fprintf ( pf,
              _("%s doubles to %d\n\n"),
              aszColorName[ pms->fMove ],
              pms->nCube * 2
            );

  else

    /* cube decision */

    fprintf ( pf,
              _("%s on roll, cube decision?\n\n"),
              aszColorName[ pms->fTurn ]
            );


}


/*
 * Print html header: dtd, head etc.
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *
 */

static void 
TextPrologue ( FILE *pf, const matchstate *pms, const int iGame ) {

  if ( pms->nMatchTo )
    fprintf ( pf,
              _("%s (O, %d pts) vs. %s (X, %d pts) (Match to %d)\n\n"),
              ap [ 0 ].szName, pms->anScore[ 0 ],
              ap [ 1 ].szName, pms->anScore[ 1 ],
              pms->nMatchTo );
  else
    fprintf ( pf,
              _("%s (O, %d pts) vs. %s (X, %d pts) (money game)\n\n"),
              ap [ 0 ].szName, pms->anScore[ 0 ],
              ap [ 1 ].szName, pms->anScore[ 1 ] );

}


/*
 * Print html header: dtd, head etc.
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *
 */

static void 
TextEpilogue ( FILE *pf, const matchstate *pms ) {

  time_t t;

  const char szVersion[] = "$Revision$";
  int iMajor, iMinor;

  iMajor = atoi ( strchr ( szVersion, ' ' ) );
  iMinor = atoi ( strchr ( szVersion, '.' ) + 1 );

  time ( &t );

  fprintf ( pf, 
            _("Output generated %s"
              "by GNU Backgammon %s ") ,
            ctime ( &t ), VERSION );
            
  fprintf ( pf,
            _("(Text Export version %d.%d)\n\n"),
            iMajor, iMinor );

}

/*
 * is this a close cubedecision?
 *
 * Input:
 *   arDouble: equities for cube decisions
 *
 */

static int
isCloseCubedecision ( const float arDouble[] ) {
  
  const float rThr = 0.05;

  /* too good positions */

  if ( arDouble[ OUTPUT_NODOUBLE ] > 1.0 ) return 1;

  /* almost a double */

  if ( fabs ( arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ] ) < rThr )
    return 1;

  /* almost a pass */

  if ( fabs ( arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ] ) < rThr )
    return 1;

  return 0;

}


/*
 * is this a missed double?
 *
 * Input:
 *   arDouble: equities for cube decisions
 *   fDouble: did the player double
 *   pci: cubeinfo
 *
 */

static int
isMissedDouble ( float arDouble[], int fDouble, cubeinfo *pci ) {

  cubedecision cd = FindBestCubeDecision ( arDouble, pci );

  switch ( cd ) {
    
  case DOUBLE_TAKE:
  case DOUBLE_PASS:
  case DOUBLE_BEAVER:
  case REDOUBLE_TAKE:
  case REDOUBLE_PASS:

    return ! fDouble;
    break;

  default:

    return 0;
    break;


  }

}
                 


/*
 * Print cube analysis
 *
 * Input:
 *  pf: output file
 *  arDouble: equitites for cube decisions
 *  fPlayer: player who doubled
 *  esDouble: eval setup
 *  pci: cubeinfo
 *  fDouble: double/no double
 *  fTake: take/drop
 *
 */

static void
TextPrintCubeAnalysisTable ( FILE *pf, float arDouble[],
                             float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                             float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                             int fPlayer,
                             evalsetup *pes, cubeinfo *pci,
                             int fDouble, int fTake,
                             skilltype stDouble,
                             skilltype stTake ) {
  const char *aszCube[] = {
    NULL, 
    N_("No double"), 
    N_("Double, take"), 
    N_("Double, pass") 
  };

  int i;
  int ai[ 3 ];
  float r;

  int fActual, fClose, fMissed;
  int fDisplay;
  int fAnno = FALSE;

  /* check if cube analysis should be printed */

  if ( pes->et == EVAL_NONE ) return; /* no evaluation */
  if ( ! GetDPEq ( NULL, NULL, pci ) ) return; /* cube not available */

  fActual = fDouble;
  fClose = isCloseCubedecision ( arDouble ); 
  fMissed = isMissedDouble ( arDouble, fDouble, pci );

  fDisplay = 
    ( fActual && exsExport.afCubeDisplay[ EXPORT_CUBE_ACTUAL ] ) ||
    ( fClose && exsExport.afCubeDisplay[ EXPORT_CUBE_CLOSE ] ) ||
    ( fMissed && exsExport.afCubeDisplay[ EXPORT_CUBE_MISSED ] ) ||
    ( exsExport.afCubeDisplay[ stDouble ] ) ||
    ( exsExport.afCubeDisplay[ stTake ] );

  if ( ! fDisplay )
    return;

  /* print alerts */

  if ( fMissed ) {

    fAnno = TRUE;

    /* missed double */

    fprintf ( pf, "%s", _("Alert: missed double") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fputs ( "\n", pf );

    }
    else {


      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc( arDouble[ OUTPUT_DROP ], pci ) -
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc( arDouble[ OUTPUT_TAKE ], pci ) -
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fputs ( "\n", pf );

    }

  }

  r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_DROP ];

  if ( fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong take */

    fprintf ( pf, _("Alert: wrong take") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fputs ( "\n", pf );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fputs ( "\n", pf );

    }

  }

  r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && ! fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong pass */

    fprintf ( pf, _("Alert: wrong pass") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fputs ( "\n", pf );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fputs ( "\n", pf );

    }

  }


  if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ];
  else
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong double */

    fprintf ( pf, _("Alert: wrong double") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fputs ( "\n", pf );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fputs ( "\n", pf );

    }

  }

  if ( ( stDouble != SKILL_NONE || stTake != SKILL_NONE ) && ! fAnno ) {
    
    if ( stDouble != SKILL_NONE ) {
      fprintf ( pf, _("Alert: double decision marked %s"),
                gettext ( aszSkillType[ stDouble ] ) );
      fputs ( "\n", pf );
    }

    if ( stTake != SKILL_NONE ) {
      fprintf ( pf, _("Alert: take decision marked %s"),
                gettext ( aszSkillType[ stTake ] ) );
      fputs ( "\n", pf );
    }

  }

  /* header */

  fputs ( _("\nCube analysis\n"), pf );

  /* ply & cubeless equity */

  switch ( pes->et ) {
  case EVAL_NONE:
    fputs ( _("n/a"), pf );
    break;
  case EVAL_EVAL:
    fprintf ( pf, 
              _("%s %d-ply equity: "), 
              pes->ec.fCubeful ? _("Cubeful") : _("Cubeless"), pes->ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    fprintf ( pf, 
              _("%s rollout equity: "), 
              pes->rc.fCubeful ? _("Cubeful") : _("Cubeless") );
    break;
  }

  /* cubeless equity */
  
  if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) )
    fprintf ( pf, "%+7.3f\n",
              aarOutput[ 0 ][ OUTPUT_EQUITY ] );
  else
    fprintf ( pf, "%+7.3f\n",
              eq2mwc ( aarOutput[ 0 ][ OUTPUT_EQUITY ], pci ) );
              
  /* percentages */

  if ( exsExport.fCubeDetailProb ) {

    /* FIXME: include in movenormal and movedouble */

    float *ar = aarOutput[ 0 ];

    fprintf ( pf,
              "  "
              " %6.2f%% "
              " %6.2f%% "
              " %6.2f%% "
              "-"
              " %6.2f%% "
              " %6.2f%% "
              " %6.2f%% "
              "\n",
              100.0 * ar[ OUTPUT_WINBACKGAMMON ],
              100.0 * ar[ OUTPUT_WINGAMMON ],
              100.0 * ar[ OUTPUT_WIN ],
              100.0 * ( 1.0  - ar[ OUTPUT_WIN ] ),
              100.0 * ar [ OUTPUT_LOSEGAMMON ],
              100.0 * ar [ OUTPUT_LOSEBACKGAMMON ] );

  }

  /* equities */

  getCubeDecisionOrdering ( ai, arDouble, pci );

  for ( i = 0; i < 3; i++ ) {

    fprintf ( pf,
              "%d. %-20s", i + 1, 
              gettext ( aszCube[ ai[ i ] ] ) );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {
      if ( i ) 
        fprintf ( pf,
                  "%+7.3f  %+7.3f\n",
                  arDouble[ ai[ i ] ],
                  arDouble[ ai[ i ] ] - arDouble[ OUTPUT_OPTIMAL ] );
      else
        fprintf ( pf,
                  "%+7.3f\n",
                  arDouble [ ai[ i ] ] );

    }
    else {
      if ( i )
        fprintf ( pf,
                  "%7.3f%%  %+7.3f%%\n",
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ), 
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ) -
                  100.0f * eq2mwc( arDouble[ OUTPUT_OPTIMAL ], pci ) );
      else
        fprintf ( pf,
                  "%7.3f%%\n",
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ) );
    }

  }

  /* cube decision */

  fprintf ( pf,
            "%s %s",
            _("Proper cube action:"),
            GetCubeRecommendation ( FindBestCubeDecision ( arDouble, pci ) ) );

  if ( ( r = getPercent ( FindBestCubeDecision ( arDouble, pci ), 
                          arDouble ) ) >= 0.0 )
    fprintf ( pf, " (%.1f%%)", 100.0f * r );


  fputs ( "\n\n", pf );


}



/*
 * Wrapper for print cube analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *  szImageDir: URI to images
 *  szExtension: extension of images
 *  fTake: take/drop
 *
 */

static void
TextPrintCubeAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr ) {

  cubeinfo ci;

  GetMatchStateCubeInfo ( &ci, pms );

  switch ( pmr->mt ) {

  case MOVE_NORMAL:

    /* cube analysis from move */

    TextPrintCubeAnalysisTable ( pf, pmr->n.arDouble, 
                                 pmr->n.aarOutput, pmr->n.aarStdDev,
                                 pmr->n.fPlayer,
                                 &pmr->n.esDouble, &ci, FALSE, FALSE,
                                 pmr->n.stCube, SKILL_NONE );

    break;

  case MOVE_DOUBLE:

    TextPrintCubeAnalysisTable ( pf, pmr->d.arDouble, 
                                 pmr->d.aarOutput, pmr->d.aarStdDev,
                                 pmr->d.fPlayer,
                                 &pmr->d.esDouble, &ci, TRUE, FALSE,
                                 pmr->d.st, SKILL_NONE );

    break;

  case MOVE_TAKE:
  case MOVE_DROP:

    /* cube analysis from double, {take, drop, beaver} */

    TextPrintCubeAnalysisTable ( pf, pmr->d.arDouble, 
                                 pmr->d.aarOutput, pmr->d.aarStdDev,
                                 pmr->d.fPlayer,
                                 &pmr->d.esDouble, &ci, TRUE, 
                                 pmr->mt == MOVE_TAKE,
                                 SKILL_NONE, /* FIXME: skill from prev. cube */
                                 pmr->d.st );

    break;

  default:

    assert ( FALSE );


  }

  return;

}


/*
 * Print move analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *  szImageDir: URI to images
 *  szExtension: extension of images
 *
 */

static void
TextPrintMoveAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr ) {

  char szBuf[ 1024 ];
  char sz[ 64 ];
  int i;

  cubeinfo ci;

  GetMatchStateCubeInfo( &ci, pms );

  /* check if move should be printed */

  if ( ! exsExport.afMovesDisplay [ pmr->n.stMove ] )
    return;

  /* print alerts */

  if ( pmr->n.stMove <= SKILL_BAD || pmr->n.stMove > SKILL_NONE ) {

    /* blunder or error */

    fprintf ( pf, 
              _("Alert: %s move"),
              gettext ( aszSkillType[ pmr->n.stMove ] ) );
    
    if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) )
      fprintf ( pf, " (%+7.3f)\n",
                pmr->n.ml.amMoves[ pmr->n.iMove ].rScore -
                pmr->n.ml.amMoves[ 0 ].rScore  );
    else
      fprintf ( pf, " (%+6.3f%%)\n",
                100.0f *
                eq2mwc ( pmr->n.ml.amMoves[ pmr->n.iMove ].rScore, &ci ) - 
                100.0f *
                eq2mwc ( pmr->n.ml.amMoves[ 0 ].rScore, &ci ) );

  }

  if ( pmr->n.lt != LUCK_NONE ) {

    /* joker */

    fprintf ( pf, 
              _("Alert: %s roll!"),
              gettext ( aszLuckType[ pmr->n.lt ] ) );
    
    if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) )
      fprintf ( pf, " (%+7.3f)\n", pmr->n.rLuck );
    else
      fprintf ( pf, " (%+6.3f%%)\n",
                100.0f * eq2mwc ( pmr->n.rLuck, &ci ) - 
                100.0f * eq2mwc ( 0.0f, &ci ) );

  }

  fputs ( "\n", pf );

  fprintf( pf, _("Rolled %d%d"), pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );

  if( pmr->n.rLuck != ERR_VAL )
    fprintf( pf, " (%s):\n", GetLuckAnalysis( &ms, pmr->n.rLuck ) );
  else
    fprintf( pf, ":" );

  if ( pmr->n.ml.cMoves ) {
	
    for( i = 0; i < pmr->n.ml.cMoves; i++ ) {
      if( i >= exsExport.nMoves /* FIXME allow user to choose limit */ &&
          i != pmr->n.iMove )
        continue;

      fputc( i == pmr->n.iMove ? '*' : ' ', pf );
      fputs( FormatMoveHint( szBuf, &ms, &pmr->n.ml, i,
                             i != pmr->n.iMove ||
                             i != pmr->n.ml.cMoves - 1 ), pf );

      /*
       * Write row with move parameters 
       */
      
      if ( exsExport.afMovesParameters 
           [ pmr->n.ml.amMoves[ i ].esMove.et - 1 ] ) {

        evalsetup *pes = &pmr->n.ml.amMoves[ i ].esMove;

        switch ( pes->et ) {
        case EVAL_EVAL: 

          fprintf ( pf, 
                    _("%d-ply %s "
                    "(%d%% speed, %d cand., %0.3g tol., noise %0.3g )\n"),
                    pes->ec.nPlies,
                    pes->ec.fCubeful ? _("cubeful") : _("cubeless"),
                    (pes->ec.nReduced) ? 100 / pes->ec.nReduced : 100,
                    pes->ec.nSearchCandidates,
                    pes->ec.rSearchTolerance,
                    pes->ec.rNoise );
          break;

        case EVAL_ROLLOUT:

          fprintf ( pf, "write me" );
          break;

        default:

          break;

        }

      }


    }

  }
  else {

    if ( pmr->n.anMove[ 0 ] >= 0 )
      /* no movelist saved */
      fprintf ( pf,
                "*    %s\n",
                FormatMove ( sz, pms->anBoard, pmr->n.anMove ) );
    else 
      /* no legal moves */
      /* FIXME: output equity?? */
      fprintf ( pf,
                "*    %s\n",
                _("Cannot move" ) );

  }

  fputs ( "\n\n", pf );

  return;

}





/*
 * Print cube analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *
 */

static void
TextAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr ) {


  char sz[ 1024 ];

  switch ( pmr->mt ) {

  case MOVE_NORMAL:

    if ( pmr->n.anMove[ 0 ] >= 0 )
      fprintf ( pf,
                _("* %s moves %s"),
                ap[ pmr->n.fPlayer ].szName,
                FormatMove ( sz, pms->anBoard, pmr->n.anMove ) );
    else
      fprintf ( pf,
                _("* %s cannot move"),
                ap[ pmr->n.fPlayer ].szName );

    fputs ( "\n", pf );

    TextPrintCubeAnalysis ( pf, pms, pmr );

    TextPrintMoveAnalysis ( pf, pms, pmr );

    break;

  case MOVE_DOUBLE:

    /* no-op: we assume the following move is MOVE_TAKE or MOVE_DROP */

    break;

  case MOVE_TAKE:
  case MOVE_DROP:

    fprintf ( pf,
              "* %s %s\n\n",
              ap[ pmr->d.fPlayer ].szName,
              ( pmr->mt == MOVE_TAKE ) ? _("accepts") : _("rejects") );

    TextPrintCubeAnalysis ( pf, pms, pmr );

    break;

  default:
    break;

  }

}


extern void TextDumpStatcontext ( FILE *pf, const statcontext *psc,
                                  matchstate *pms, const int iGame ) {

  char sz[ 4096 ];

  if ( iGame >= 0 ) {
    fprintf ( pf, _("Game statistics for game %d\n\n"), iGame + 1 );
  }
  else {
    
    if ( pms->nMatchTo )
      fprintf ( pf, _( "Match statistics\n\n" ) );
    else
      fprintf ( pf, _( "Session statistics\n\n" ) );

  }

  DumpStatcontext ( sz, psc, NULL );

  fputs ( sz, pf );

  fputs ( "\n\n", pf );
}



static void
TextPrintComment ( FILE *pf, const moverecord *pmr ) {

  char *sz = NULL;

  switch ( pmr->mt ) {

  case MOVE_GAMEINFO:
    sz = pmr->g.sz;
    break;
  case MOVE_DOUBLE:
  case MOVE_TAKE:
  case MOVE_DROP:
    sz = pmr->d.sz;
    break;
  case MOVE_NORMAL:
    sz = pmr->n.sz;
    break;
  case MOVE_RESIGN:
    sz = pmr->r.sz;
    break;
  case MOVE_SETBOARD:
    sz = pmr->sb.sz;
    break;
  case MOVE_SETDICE:
    sz = pmr->sd.sz;
    break;
  case MOVE_SETCUBEVAL:
    sz = pmr->scv.sz;
    break;
  case MOVE_SETCUBEPOS:
    sz = pmr->scp.sz;
    break;

  }

  if ( sz ) {

    fputs ( _("Annotation:\n"), pf );
    fputs ( sz, pf );
    fputs ( "\n", pf );

  }


}



/*
 * Export a game in HTML
 *
 * Input:
 *   pf: output file
 *   plGame: list of moverecords for the current game
 *
 */

static void ExportGameText ( FILE *pf, list *plGame, 
                             const int iGame, const int fLastGame ) {

    list *pl;
    moverecord *pmr;
    matchstate msExport;
    matchstate msOrig;
    int iMove = 0;
    statcontext *psc = NULL;
    static statcontext scTotal;
    movegameinfo *pmgi = NULL;

    if ( ! iGame )
      IniStatcontext ( &scTotal );

    updateStatisticsGame ( plGame );

    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {

      pmr = pl->p;
      
      switch( pmr->mt ) {

      case MOVE_GAMEINFO:

        ApplyMoveRecord ( &msExport, pmr );

        TextPrologue( pf, &msExport, iGame );

        msOrig = msExport;
        pmgi = &pmr->g;

        psc = &pmr->g.sc;

        AddStatcontext ( psc, &scTotal );
    
        /* FIXME: game introduction */
        break;

      case MOVE_NORMAL:

	if( pmr->n.fPlayer != msExport.fMove ) {
	    SwapSides( msExport.anBoard );
	    msExport.fMove = pmr->n.fPlayer;
	}
      
        msExport.fTurn = msExport.fMove = pmr->n.fPlayer;
        msExport.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
        msExport.anDice[ 1 ] = pmr->n.anRoll[ 1 ];

        TextBoardHeader ( pf, &msExport, iGame, iMove );

        printTextBoard( pf, &msExport );
        TextAnalysis ( pf, &msExport, pmr );
        
        iMove++;

        break;

      case MOVE_TAKE:
      case MOVE_DROP:

        TextBoardHeader ( pf,&msExport, iGame, iMove );

        printTextBoard( pf, &msExport );
        
        TextAnalysis ( pf, &msExport, pmr );
        
        iMove++;

        break;


      default:

        break;
        
      }

      TextPrintComment ( pf, pmr );

      ApplyMoveRecord ( &msExport, pmr );

    }

    if( pmgi && pmgi->fWinner != -1 ) {

      /* print game result */

      if ( pmgi->nPoints > 1 )
        fprintf ( pf, 
                  _("%s wins %d points\n\n"),
                  ap[ pmgi->fWinner ].szName, 
                  pmgi->nPoints );
      else
        fprintf ( pf, 
                  _("%s wins %d point\n\n"),
                  ap[ pmgi->fWinner ].szName,
                  pmgi->nPoints );

    }

    if ( psc )
      TextDumpStatcontext ( pf, psc, &msOrig, iGame );


    if ( fLastGame ) {

      TextDumpStatcontext ( pf, &scTotal, &msOrig, -1 );

    }

    TextEpilogue( pf, &ms );


    
}

extern void CommandExportGameText( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export"
		 "game text').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    ExportGameText( pf, plGame,
                    getGameNumber ( plGame ), FALSE );

    if( pf != stdout )
	fclose( pf );

    // setDefaultFileName ( sz, PATH_TEXT );
    
}




extern void CommandExportMatchText( char *sz ) {
    
    FILE *pf;
    list *pl;
    int nGames;
    char *szCurrent;
    int i;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export "
		 "match text').") );
	return;
    }

    /* Find number of games in match */

    for( pl = lMatch.plNext, nGames = 0; pl != &lMatch; 
         pl = pl->plNext, nGames++ )
      ;

    for( pl = lMatch.plNext, i = 0; pl != &lMatch; pl = pl->plNext, i++ ) {

      szCurrent = HTMLFilename ( sz, i );

      if ( !i ) {

        if ( ! confirmOverwrite ( sz, fConfirmSave ) )
          return;

        // setDefaultFileName ( sz, PATH_TEXT );

      }


      if( !strcmp( szCurrent, "-" ) )
	pf = stdout;
      else if( !( pf = fopen( szCurrent, "w" ) ) ) {
	perror( szCurrent );
	return;
      }

      ExportGameText ( pf, pl->p, 
                       i, i == nGames - 1 );

      if( pf != stdout )
        fclose( pf );

    }
    
}



extern void CommandExportPositionText( char *sz ) {

    FILE *pf;
	
    sz = NextToken( &sz );
    
    if( ms.gs == GAME_NONE ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export "
		 "position text').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    TextPrologue ( pf, &ms, getGameNumber ( plGame ) );

    TextBoardHeader ( pf, &ms, 
                      getGameNumber ( plGame ),
                      getMoveNumber ( plGame, plLastMove->plNext->p ) - 1 );

    printTextBoard( pf, &ms );

    if( plLastMove->plNext->p != NULL)
      TextAnalysis ( pf, &ms, plLastMove->plNext->p );
    
    TextEpilogue ( pf, &ms );

    if( pf != stdout )
	fclose( pf );

    // setDefaultFileName ( sz, PATH_TEXT );

}

