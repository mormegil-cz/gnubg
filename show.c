/*
 * show.c
 *
 * by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001, 2002.
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

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "export.h"
#include "format.h"
#include "dice.h"
#include "matchequity.h"
#include "matchid.h"
#include "i18n.h"
#include "sound.h"
#include "onechequer.h"
#include "osr.h"
#include "positionid.h"
#include "boarddim.h"

#if USE_GTK
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkpath.h"
#include "gtktheory.h"
#include "gtkrace.h"
#include "gtkexport.h"
#include "gtkmet.h"
#include "gtkrolls.h"
#include "gtktempmap.h"
#include "gtkbearoff.h"
#elif USE_EXT
#include "xgame.h"
#endif

extern char *aszCopying[], *aszWarranty[]; /* from copying.c */

static void ShowMoveFilter ( const movefilter *pmf, const int ply) {
  
  if (pmf->Accept < 0 ) {
	outputf( _("Skip pruning for %d-ply moves.\n"), ply );
	return;
  }
  
  if (pmf->Accept == 1)
	outputf ( _("keep the best %d-ply move"), ply );
  else
	outputf ( _("keep the first %d %d-ply moves"), pmf->Accept, ply);

  if (pmf->Extra == 0) {
	  outputf ("\n");
	  return;
  }
	
  outputf ( _(" and up to %d more moves within equity %0.3g\n" ),
			pmf->Extra, pmf->Threshold);
}	  


static void
ShowMoveFilters ( const movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int i, j;

  for ( i = 0; i < MAX_FILTER_PLIES; ++i ) {

    outputf ( "      Move filter for %d ply:\n", i + 1 );
  
    for ( j = 0; j <= i; ++j ) {
      outputf ( "        " );
      ShowMoveFilter ( &aamf[i ][ j ], j );
    }
  }

  outputl ( "" );
  
}
	  
static void 
ShowEvaluation( const evalcontext *pec ) {
  
  outputf( _("        %d-ply evaluation.\n"
             "        %d%% speed.\n"
             "        %s evaluations.\n"),
           pec->nPlies, 
           (pec->nReduced) ? 100 / pec->nReduced : 100,
           pec->fCubeful ? _("Cubeful") : _("Cubeless") );

  if( pec->rNoise )
    outputf( _("        Noise standard deviation %5.3f"), pec->rNoise );
  else
    output( _("        Noiseless evaluations") );
  
  outputl( pec->fDeterministic ? _(" (deterministic noise).\n") :
           _(" (pseudo-random noise).\n") );

}

extern int
EvalCmp ( const evalcontext *E1, const evalcontext *E2, 
          const int nElements) {

  int  i, cmp = 0;
 
  if (nElements < 1)
    return 0;

  for (i = 0;  i < nElements; ++i, ++E1, ++E2) {
    cmp = memcmp ((void *) E1, (void *) E2, sizeof (evalcontext));
    if (cmp != 0)
      break;
  }

  return cmp;
}

/* all the sundry displays of evaluations. Deals with identical/different
   player settings, displays late evaluations where needed */

static void 
show_evals ( const char *text, 
             const evalcontext *early, const evalcontext *late,
             const int fPlayersAreSame, const int fLateEvals, 
             const int nLate ) { 

  int  i;

  if (fLateEvals) 
    outputf (_("%s for first %d plies:\n"), text, nLate);
  else
    outputf ("%s\n", text);

  if (fPlayersAreSame)
    ShowEvaluation (early );
  else {
    for (i = 0; i < 2; i++ ) {
      outputf (_("Player %d:\n"), i);
      ShowEvaluation (early + i );
    }
  }

  if (!fLateEvals)
    return;

  outputf (_("%s after %d plies:\n"), text, nLate);
  if (fPlayersAreSame)
    ShowEvaluation (late );
  else {
    for (i = 0; i < 2; i++ ) {
      outputf (_("Player %d:\n"), i);
      ShowEvaluation (late + i );
    }
  }
}


static void
show_movefilters ( const movefilter aaamf[ 2 ][ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  if ( equal_movefilters ( (movefilter (*)[]) aaamf[ 0 ], 
                           (movefilter (*)[]) aaamf[ 1 ] ) ) 
    ShowMoveFilters ( aaamf[ 0 ] );
  else {
    int i;
    for (i = 0; i < 2; i++ ) {
      outputf (_("Player %d:\n"), i);
      ShowMoveFilters ( aaamf[ i ] );
    }
  }
}

static void
ShowRollout ( const rolloutcontext *prc ) {

  int fDoTruncate = 0;
  int fLateEvals = 0;
  int nTruncate = prc->nTruncate;
  int nLate = prc->nLate;
  int fPlayersAreSame = 1;
  int fCubeEqualChequer = 1;

  if (prc->fDoTruncate && (nTruncate > 0))
    fDoTruncate = 1;

  if (prc->fLateEvals && 
      (!fDoTruncate || ((nTruncate > nLate) && (nLate > 2))))
    fLateEvals = 1;

  if ( prc->nTrials == 1 )
    outputf ( _("%d game will be played per rollout.\n"),
              prc->nTrials );
  else
    outputf ( _("%d games will be played per rollout.\n"),
              prc->nTrials );

  if( fDoTruncate && (nTruncate > 1 ) )
      outputf( _("Truncation after %d plies.\n"), nTruncate );
  else if( fDoTruncate && (nTruncate == 1) )
      outputl( _("Truncation after 1 ply.") );
  else
      outputl( _("No truncation.") );

  outputl ( prc->fTruncBearoff2 ?
            _("Will truncate cubeful money game rollouts when reaching "
              "exact bearoff database.") :
            _("Will not truncate cubeful money game rollouts when reaching "
              "exact bearoff database.") );

  outputl ( prc->fTruncBearoff2 ?
            _("Will truncate money game rollouts when reaching "
              "exact bearoff database.") :
            _("Will not truncate money game rollouts when reaching "
              "exact bearoff database.") );

  outputl ( prc->fTruncBearoffOS ?
            _("Will truncate *cubeless* rollouts when reaching "
              "one-sided bearoff database.") :
            _("Will not truncate *cubeless* rollouts when reaching "
              "one-sided bearoff database.") );

  outputl ( prc->fVarRedn ? 
            _("Lookahead variance reduction is enabled.") :
            _("Lookahead variance reduction is disabled.") );
  outputl ( prc->fRotate ? 
            _("Quasi-random dice are enabled.") :
            _("Quasi-random dice are disabled.") );
  outputl ( prc->fCubeful ?
            _("Cubeful rollout.") :
            _("Cubeless rollout.") );
  outputl ( prc->fInitial ?
            _("Rollout as opening move enabled.") :
            _("Rollout as opening move disabled.") );
  outputf ( _("%s dice generator with seed %u.\n"),
            gettext ( aszRNG[ prc->rngRollout ] ), prc->nSeed );
            
  /* see if the players settings are the same */
  if (EvalCmp (prc->aecChequer, prc->aecChequer + 1, 1) ||
      EvalCmp (prc->aecCube, prc->aecCube + 1, 1) ||
     (fLateEvals &&
        (EvalCmp (prc->aecChequerLate, prc->aecChequerLate + 1, 1) ||
         EvalCmp (prc->aecCubeLate, prc->aecCubeLate + 1, 1))))
     fPlayersAreSame = 0;
    
 /* see if the cube and chequer evals are the same */
  if (EvalCmp (prc->aecChequer, prc->aecCube, 2) ||
      (fLateEvals &&
       (EvalCmp (prc->aecChequerLate, prc->aecCubeLate, 2))))
    fCubeEqualChequer = 0;
  
  if (fCubeEqualChequer) {
    /* simple summary - show_evals will deal with player differences */
    show_evals (_("Evaluation parameters:"), prc->aecChequer,
                prc->aecChequerLate, fPlayersAreSame, fLateEvals, nLate);
  } else {
    /* Cube different from Chequer */
    show_evals ( _( "Chequer play parameters:"), prc->aecChequer,
                 prc->aecChequerLate, fPlayersAreSame, fLateEvals, nLate);
    show_evals ( _( "Cube decision parameters:"), prc->aecCube,
                 prc->aecCubeLate, fPlayersAreSame, fLateEvals, nLate);
  }

  if ( fLateEvals ) {
    outputf ( _("Move filter for first %d plies:\n"), nLate );
    show_movefilters ( prc->aaamfChequer );
    outputf ( _("Move filter after %d plies:\n"), nLate );
    show_movefilters ( prc->aaamfLate );
  }
  else {
    outputf ( _("Move filter:\n") );
    show_movefilters ( prc->aaamfChequer );
  }

  if (fDoTruncate) {
    show_evals (_("Truncation point Chequer play evaluation:"), 
                &prc->aecChequerTrunc, 0, 1, 0, 0 );
    show_evals (_("Truncation point Cube evaluation:"), 
                &prc->aecCubeTrunc, 0, 1, 0, 0 );
  }

  if (prc->fStopOnSTD) {
    outputf ( _("Rollouts may stop after %d games if the ratios |value/STD|\n"
		"are all less than< %5.4f\n"), prc->nMinimumGames, 
		prc->rStdLimit);
  }
}

static void
ShowEvalSetup ( const evalsetup *pes ) {

  switch ( pes->et ) {

  case EVAL_NONE:
    outputl ( _("      No evaluation.") );
    break;
  case EVAL_EVAL:
    outputl ( _("      Neural net evaluation:") );
    ShowEvaluation ( &pes->ec);
    break;
  case EVAL_ROLLOUT:
    outputl ( _("      Rollout:") );
    ShowRollout ( &pes->rc );
    break;
  default:
    assert ( FALSE );

  }

}


static void ShowPaged( char **ppch ) {

    int i, nRows = 0;
    char *pchLines;
#ifdef TIOCGWINSZ
    struct winsize ws;
#endif

#if HAVE_ISATTY
    if( isatty( STDIN_FILENO ) ) {
#endif
#ifdef TIOCGWINSZ
	if( !( ioctl( STDIN_FILENO, TIOCGWINSZ, &ws ) ) )
	    nRows = ws.ws_row;
#endif
	if( !nRows && ( pchLines = getenv( "LINES" ) ) )
	    nRows = atoi( pchLines );

	/* FIXME we could try termcap-style tgetnum( "li" ) here, but it
	   hardly seems worth it */
	
	if( !nRows )
	    nRows = 24;

	i = 0;

	while( *ppch ) {
	    outputl( *ppch++ );
	    if( ++i >= nRows - 1 ) {
		GetInput( _("-- Press <return> to continue --") );
		
		if( fInterrupt )
		    return;
		
		i = 0;
	    }
	}
#if HAVE_ISATTY
    } else
#endif
	while( *ppch )
	    outputl( *ppch++ );
}

extern void CommandShowAnalysis( char *sz ) {

    int i;

    outputl( fAnalyseCube ? _("Cube action will be analysed.") :
	     _("Cube action will not be analysed.") );

    outputl( fAnalyseDice ? _("Dice rolls will be analysed.") :
	     _("Dice rolls will not be analysed.") );

    if( fAnalyseMove ) {
	outputl( _("Chequer play will be analysed.") );
	if( cAnalysisMoves < 0 )
	    outputl( _("Every legal move will be analysed.") );
	else
	    outputf( _("Up to %d moves will be analysed.\n"), cAnalysisMoves );
    } else
	outputl( _("Chequer play will not be analysed.") );

    outputl( "" );
    for ( i = 0; i < 2; ++i )
      outputf( _("Analyse %s's chequerplay and cube decisions: %s\n"),
               ap[ i ].szName, afAnalysePlayers[ i ] ? _("yes") : _("no") );

    outputl( _("\nAnalysis thresholds:") );
    outputf( /*"  +%.3f %s\n"
	     "  +%.3f %s\n" */
	     "  -%.3f %s\n"
	     "  -%.3f %s\n"
	     "  -%.3f %s\n"
	     "\n"
	     "  +%.3f %s\n"
	     "  +%.3f %s\n"
	     "  -%.3f %s\n"
	     "  -%.3f %s\n",
/* 	     arSkillLevel[ SKILL_VERYGOOD ],  */
/*              gettext ( aszSkillType[ SKILL_VERYGOOD ] ), */
/*              arSkillLevel[ SKILL_GOOD ],  */
/*              gettext ( aszSkillType[ SKILL_GOOD ] ), */
	     arSkillLevel[ SKILL_DOUBTFUL ], 
             gettext ( aszSkillType[ SKILL_DOUBTFUL ] ),
	     arSkillLevel[ SKILL_BAD ], 
             gettext ( aszSkillType[ SKILL_BAD ] ),
             arSkillLevel[ SKILL_VERYBAD ],
             gettext ( aszSkillType[ SKILL_VERYBAD ] ),
	     arLuckLevel[ LUCK_VERYGOOD ], 
             gettext ( aszLuckType[ LUCK_VERYGOOD ] ),
             arLuckLevel[ LUCK_GOOD ],
             gettext ( aszLuckType[ LUCK_GOOD ] ),
	     arLuckLevel[ LUCK_BAD ], 
             gettext ( aszLuckType[ LUCK_BAD ] ),
             arLuckLevel[ LUCK_VERYBAD ],
             gettext ( aszLuckType[ LUCK_VERYBAD ] ) );

    outputl( _("\n"
               "Analysis will be performed with the "
             "following evaluation parameters:") );
  outputl( _("    Chequer play:") );
  ShowEvalSetup ( &esAnalysisChequer );
  ShowMoveFilters ( (const movefilter (*)[]) aamfAnalysis );
  outputl( _("    Cube decisions:") );
  ShowEvalSetup ( &esAnalysisCube );

  outputl( _("    Luck analysis:") );
  ShowEvaluation ( &ecLuck );

}

extern void CommandShowAutomatic( char *sz ) {

    static char *szOn = N_("On"), *szOff = N_("Off");
    
    outputf( _( 
              "bearoff \t(Play certain non-contact bearoff moves):      \t%s\n"
              "crawford\t(Enable the Crawford rule as appropriate):     \t%s\n"
              "doubles \t(Turn the cube when opening roll is a double): \t%d\n"
              "game    \t(Start a new game after each one is completed):\t%s\n"
              "move    \t(Play the forced move when there is no choice):\t%s\n"
              "roll    \t(Roll the dice if no double is possible):      \t%s\n"),
	    fAutoBearoff ? gettext ( szOn ) : gettext ( szOff ),
	    fAutoCrawford ? gettext ( szOn ) : gettext ( szOff ),
	    cAutoDoubles,
	    fAutoGame ? gettext ( szOn ) : gettext ( szOff ),
	    fAutoMove ? gettext ( szOn ) : gettext ( szOff ),
	    fAutoRoll ? gettext ( szOn ) : gettext ( szOff ) );
}

extern void CommandShowBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    char szOut[ 2048 ];
    char *ap[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    
    if( !*sz ) {
	if( ms.gs == GAME_NONE )
	    outputl( _("No position specified and no game in progress.") );
	else
	    ShowBoard();
	
	return;
    }

    /* FIXME handle =n notation */
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

#if USE_GUI
    if( fX )
#if USE_GTK
      game_set( BOARD( pwBoard ), an, TRUE, "", "", 0, 0, 0, -1, -1, FALSE,
                anChequers[ ms.bgv ] );
#else
        GameSet( &ewnd, an, TRUE, "", "", 0, 0, 0, -1, -1 );    
#endif
    else
#endif
        outputl( DrawBoard( szOut, an, TRUE, ap, 
                            MatchIDFromMatchState ( &ms ),
                            anChequers[ ms.bgv ] ) );
}

extern 
void CommandShowFullBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    char szOut[ 2048 ];
    char *apch[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    
    if( !*sz ) {
	if( ms.gs == GAME_NONE )
	    outputl( _("No position specified and no game in progress.") );
	else
	    ShowBoard();
	
	return;
    }

    /* FIXME handle =n notation */
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

#if USE_GUI
    if( fX )
#if USE_GTK
	game_set( BOARD( pwBoard ), an, ms.fTurn, 
                  ap[ 1 ].szName, ap[ 0 ].szName , ms.nMatchTo, 
                  ms.anScore[ 1 ], ms.anScore[ 0 ], 
                  ms.anDice[ 0 ], ms.anDice[ 1 ], FALSE,
                  anChequers[ ms.bgv ] );
#else
        GameSet( &ewnd, an, TRUE, "", "", 0, 0, 0, -1, -1 );    
#endif
    else
#endif
        outputl( DrawBoard( szOut, an, TRUE, apch, 
                            MatchIDFromMatchState ( &ms ), 
                            anChequers[ ms.bgv ] ) );
}


extern void CommandShowDelay( char *sz ) {
#if USE_GUI
    if( nDelay )
	outputf( _("The delay is set to %d ms.\n"),nDelay);
    else
	outputl( _("No delay is being used.") );
#else
    outputl( _("The `show delay' command applies only when using a window "
	  "system.") );
#endif
}

extern void CommandShowCache( char *sz ) {

    int c, cLookup, cHit;

    EvalCacheStats( &c, NULL, &cLookup, &cHit );

    outputf( _("%d cache entries have been used.  %d lookups, %d hits"),
	    c, cLookup, cHit );

    if( cLookup > 0x01000000 ) /* calculate carefully to avoid overflow */
	outputf( " (%d%%).", ( cHit + ( cLookup / 200 ) ) /
		 ( cLookup / 100 ) );
    else if( cLookup )
	outputf( " (%d%%).", ( cHit * 100 + cLookup / 2 ) / cLookup );
    else
	outputc( '.' );

    outputc( '\n' );
}

extern void CommandShowCalibration( char *sz ) {

#if USE_GTK
    if( fX ) {
	GTKShowCalibration();
	return;
    }
#endif
    
    if( rEvalsPerSec > 0 )
	outputf( _("Evaluation speed has been set to %.0f evaluations per "
		   "second.\n"), rEvalsPerSec );
    else
	outputl( _("No evaluation speed has been recorded." ) );
}

extern void CommandShowClockwise( char *sz ) {

    if( fClockwise )
	outputl( _("Player 1 moves clockwise (and player 0 moves "
		 "anticlockwise).") );
    else
	outputl( _("Player 1 moves anticlockwise (and player 0 moves "
		 "clockwise).") );
}

static void ShowCommands( command *pc, char *szPrefix ) {

    char sz[ 128 ], *pch;

    strcpy( sz, szPrefix );
    pch = strchr( sz, 0 );

    for( ; pc->sz; pc++ ) {
	if( !pc->szHelp )
	    continue;

	strcpy( pch, pc->sz );

	if( pc->pc && pc->pc->pc != pc->pc ) {
	    strcat( pch, " " );
	    ShowCommands( pc->pc, sz );
	} else
	    outputl( sz );
    }
}

extern void CommandShowCommands( char *sz ) {

    ShowCommands( acTop, "" );
}

extern void CommandShowConfirm( char *sz ) {

    if( fConfirm )
	outputl( _("GNU Backgammon will ask for confirmation before "
	       "aborting games in progress.") );
    else
	outputl( _("GNU Backgammon will not ask for confirmation "
	       "before aborting games in progress.") );

    if( fConfirmSave )
	outputl( _("GNU Backgammon will ask for confirmation before "
	       "overwriting existing files.") );
    else
	outputl( _("GNU Backgammon will not ask for confirmation "
	       "overwriting existing files.") );

}

extern void CommandShowCopying( char *sz ) {

#if USE_GTK
    if( fX )
	ShowList( aszCopying, _("Copying") );
    else
#endif
	ShowPaged( aszCopying );
}

extern void CommandShowCrawford( char *sz ) {

  if( ms.nMatchTo > 0 ) 
    outputl( ms.fCrawford ?
	  _("This game is the Crawford game.") :
	  _("This game is not the Crawford game") );
  else if ( !ms.nMatchTo )
    outputl( _("Crawford rule is not used in money sessions.") );
  else
    outputl( _("No match is being played.") );

}

extern void CommandShowCube( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("There is no game in progress.") );
	return;
    }

    if( ms.fCrawford ) {
	outputl( _("The cube is disabled during the Crawford game.") );
	return;
    }
    
    if( !ms.fCubeUse ) {
	outputl( _("The doubling cube is disabled.") );
	return;
    }
	
    if( ms.fCubeOwner == -1 )
	outputf( _("The cube is at %d, and is centred."), ms.nCube );
    else
	outputf( _("The cube is at %d, and is owned by %s."), 
                 ms.nCube, ap[ ms.fCubeOwner ].szName );
}

extern void CommandShowDice( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("The dice will not be rolled until a game is started.") );

	return;
    }

    if( ms.anDice[ 0 ] < 1 )
	outputf( _("%s has not yet rolled the dice.\n"), ap[ ms.fMove ].szName );
    else
	outputf( _("%s has rolled %d and %d.\n"), ap[ ms.fMove ].szName,
		 ms.anDice[ 0 ], ms.anDice[ 1 ] );
}

extern void CommandShowDisplay( char *sz ) {

    if( fDisplay )
	outputl( _("GNU Backgammon will display boards for computer moves.") );
    else
	outputl( _("GNU Backgammon will not display boards for computer moves.") );
}

extern void CommandShowEngine( char *sz ) {

    char szBuffer[ 4096 ];
    
    EvalStatus( szBuffer );

    output( szBuffer );
}

extern void CommandShowEvaluation( char *sz ) {

    outputl( _("`eval' and `hint' will use:") );
    outputl( _("    Chequer play:") );
    ShowEvalSetup ( &esEvalChequer );
    outputl( _("    Move filters:") );
    ShowMoveFilters ( (const movefilter (*)[]) aamfEval );
    outputl( _("    Cube decisions:") );
    ShowEvalSetup ( &esEvalCube );

}

extern void CommandShowEgyptian( char *sz ) {

    if ( fEgyptian )
      outputl( _("Sessions are played with the Egyptian rule.") );
    else
      outputl( _("Sessions are played without the Egyptian rule.") );

}

extern void CommandShowJacoby( char *sz ) {

  if ( ! ms.nMatchTo )
    outputl( ms.fJacoby ? 
             _("This money session is play with the Jacoby rule."
               " Default is:") :
             _("This money session is play without the Jacoby rule."
               " Default is:") );

  if ( fJacoby ) 
    outputl( _("Money sessions are played with the Jacoby rule.") );
  else
    outputl( _("Money sessions are played without the Jacoby rule.") );

}

extern void CommandShowLang( char *sz ) {

  outputf( _("Your language preference is set to %s.\n"), szLang );
    
}

extern void CommandShowMatchInfo( char *sz ) {

#if USE_GTK
    if( fX ) {
	GTKMatchInfo();
	return;
    }
#endif
    
    outputf( _("%s (%s) vs. %s (%s)"), ap[ 0 ].szName,
	     mi.pchRating[ 0 ] ? mi.pchRating[ 0 ] : _("unknown rating"),
	     ap[ 1 ].szName, mi.pchRating[ 1 ] ? mi.pchRating[ 1 ] :
	     _("unknown rating") );

    if( mi.nYear )
	outputf( ", %04d-%02d-%02d\n", mi.nYear, mi.nMonth, mi.nDay );
    else
	outputc( '\n' );

    if( mi.pchEvent )
	outputf( _("Event: %s\n"), mi.pchEvent );

    if( mi.pchRound )
	outputf( _("Round: %s\n"), mi.pchRound );

    if( mi.pchPlace )
	outputf( _("Place: %s\n"), mi.pchPlace );

    if( mi.pchAnnotator )
	outputf( _("Annotator: %s\n"), mi.pchAnnotator );

    if( mi.pchComment )
	outputf( "\n%s\n", mi.pchComment );    
}

extern void CommandShowMatchLength( char *sz ) {
    
    outputf( nDefaultLength == 1 ? _("New matches default to %d point.\n") :
	     _("New matches default to %d points.\n"), nDefaultLength );
}

extern void CommandShowPipCount( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];

    if( !*sz && ms.gs == GAME_NONE ) {
	outputl( _("No position specified and no game in progress.") );
	return;
    }
    
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;
    
    PipCount( an, anPips );
    
    outputf( _("The pip counts are: %s %d, %s %d.\n"), ap[ ms.fMove ].szName,
	    anPips[ 1 ], ap[ !ms.fMove ].szName, anPips[ 0 ] );
}

extern void CommandShowPlayer( char *sz ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	outputf( _("Player %d:\n"
		"  Name: %s\n"
		"  Type: "), i, ap[ i ].szName );

	switch( ap[ i ].pt ) {
	case PLAYER_EXTERNAL:
	    outputf( _("external: %s\n\n"), ap[ i ].szSocket );
	    break;
	case PLAYER_GNU:
	    outputf( _("gnubg:\n") );
            outputl( _("    Checker play:") );
            ShowEvalSetup ( &ap[ i ].esChequer );
            outputl( _("    Move filters:") );
            ShowMoveFilters ( (const movefilter (*)[]) ap[ i ].aamf );
            outputl( _("    Cube decisions:") );
            ShowEvalSetup ( &ap[ i ].esCube );
	    break;
	case PLAYER_PUBEVAL:
	    outputl( _("pubeval\n") );
	    break;
	case PLAYER_HUMAN:
	    outputl( _("human\n") );
	    break;
	}
    }
}

extern void CommandShowPostCrawford( char *sz ) {

  if( ms.nMatchTo > 0 ) 
    outputl( ms.fPostCrawford ?
	  _("This is post-Crawford play.") :
	  _("This is not post-Crawford play.") );
  else if ( !ms.nMatchTo )
    outputl( _("Crawford rule is not used in money sessions.") );
  else
    outputl( _("No match is being played.") );

}

extern void CommandShowPrompt( char *sz ) {

    outputf( _("The prompt is set to `%s'.\n"), szPrompt );
}

extern void CommandShowRNG( char *sz ) {

  outputf( _("You are using the %s generator.\n"),
	  gettext ( aszRNG[ rngCurrent ] ) );
    
}

extern void CommandShowRollout( char *sz ) {

  outputl( _("`rollout' will use:") );
  ShowRollout ( &rcRollout );

}

extern void CommandShowScore( char *sz ) {

    outputf((ms.cGames == 1
	     ? _("The score (after %d game) is: %s %d, %s %d")
	     : _("The score (after %d games) is: %s %d, %s %d")),
	    ms.cGames,
	    ap[ 0 ].szName, ms.anScore[ 0 ],
	    ap[ 1 ].szName, ms.anScore[ 1 ] );

    if ( ms.nMatchTo > 0 ) {
        outputf ( ms.nMatchTo == 1 ? 
	         _(" (match to %d point%s).\n") :
	         _(" (match to %d points%s).\n"),
                 ms.nMatchTo,
		 ms.fCrawford ? 
		 _(", Crawford game") : ( ms.fPostCrawford ?
					 _(", post-Crawford play") : ""));
    } 
    else {
        if ( ms.fJacoby )
	    outputl ( _(" (money session,\nwith Jacoby rule).") );
        else
	    outputl ( _(" (money session,\nwithout Jacoby rule).") );
    }

}

extern void CommandShowSeed( char *sz ) {

    PrintRNGSeed( rngCurrent );
}

extern void CommandShowTurn( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game is being played.") );

	return;
    }
    
    if ( ms.anDice[ 0 ] )
      outputf ( _("%s in on move.\n"),
                ap[ ms.fMove ].szName );
    else
      outputf ( _("%s in on roll.\n"),
                ap[ ms.fMove ].szName );

    if( ms.fResigned )
	outputf( _("%s has offered to resign a %s.\n"), ap[ ms.fMove ].szName,
		gettext ( aszGameResult[ ms.fResigned - 1 ] ) );
}

extern void CommandShowWarranty( char *sz ) {

#if USE_GTK
    if( fX )
	ShowList( aszWarranty, _("Warranty") );
    else
#endif
	ShowPaged( aszWarranty );
}

extern void CommandShowKleinman( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];
    float fKC;

    if( !*sz && ms.gs == GAME_NONE ) {
        outputl( _("No position specified and no game in progress.") );
        return;
    }
 
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

#if USE_GTK
    if ( fX ) {
      GTKShowRace ( 0, an );
      return;
    }
#endif
     
    PipCount( an, anPips );
 
    fKC = KleinmanCount (anPips[1], anPips[0]);
    if (fKC == -1.0)
        outputf (_("Pipcount unsuitable for Kleinman Count.\n"));
    else
        outputf (_("Cubeless Winning Chance (%s on roll): %.4f\n"), 
                 ap[ ms.fMove ].szName, fKC );
 }

extern void CommandShowThorp( char *sz ) {

    int an[ 2 ][ 25 ];
    int nLeader, nTrailer;
    int nDiff;

    if( !*sz && ms.gs == GAME_NONE ) {
        outputl( _("No position specified and no game in progress.") );
        return;
    }

    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

#if USE_GTK
    if ( fX ) {
      GTKShowRace ( 2, an );
      return;
    }
#endif
     
    ThorpCount ( an, &nLeader, &nTrailer );

    outputf("L = %d  T = %d  -> ", nLeader, nTrailer);

    if (nTrailer >= (nLeader - 1))
      output(_("Redouble, "));
    else if (nTrailer >= (nLeader - 2))
      output(_("Double, "));
    else
      output(_("No double, "));
    
    if (nTrailer >= (nLeader + 2))
      outputl(_("drop"));
    else
      outputl(_("take"));
    
    if( ( nDiff = nTrailer - nLeader ) > 13 )
      nDiff = 13;
    else if( nDiff < -37 )
      nDiff = -37;
    
    outputf(_("Bower's interpolation: %d%% cubeless winning "
              "chance (%s on roll)\n"), 
            74 + 2 * nDiff, ap[ ms.fMove ].szName );

}

extern void CommandShowBeavers( char *sz ) {

    if( nBeavers > 1 )
	outputf( _("%d beavers/raccoons allowed in money sessions.\n"), nBeavers );
    else if( nBeavers == 1 )
	outputl( _("1 beaver allowed in money sessions.") );
    else
	outputl( _("No beavers allowed in money sessions.") );
}

extern void CommandShowGammonValues ( char *sz ) {

  cubeinfo ci;
  int i;

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("No game in progress (type `new game' to start one).") );

    return;
  }

#if USE_GTK
  if ( fX ) {
    GTKShowTheory ( 1 );
    return;
  }
#endif

  GetMatchStateCubeInfo( &ci, &ms );

  output ( _("Player        Gammon value    Backgammon value\n") );

  for ( i = 0; i < 2; i++ ) {

    outputf ("%-12s     %7.5f         %7.5f\n",
	     ap[ i ].szName,
	     0.5f * ci.arGammonPrice[ i ], 
             0.5f * ( ci.arGammonPrice[ 2 + i ] + ci.arGammonPrice[ i ] ) );
  }

}

static void
writeMET ( float aafMET[][ MAXSCORE ],
           const int nRows, const int nCols, const int fInvert ) {

  int i,j;

  output ( "          " );
  for ( j = 0; j < nCols; j++ )
    outputf ( _(" %3i-away "), j + 1 );
  output ( "\n" );

  for ( i = 0; i < nRows; i++ ) {
    
    outputf ( _(" %3i-away "), i + 1 );
    
    for ( j = 0; j < nCols; j++ )
      outputf ( " %8.4f ", 
                fInvert ? 100.0f * ( 1.0 - GET_MET ( i, j, aafMET ) ) :
                GET_MET ( i, j, aafMET ) * 100.0 );
    output ( "\n" );
  }
  output ( "\n" );

}


static void
EffectivePipCount( const float arMu[ 2 ], const int anPips[ 2 ] ) {

  int i;
  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
              5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
              1 * 16 + 1 * 20 + 1 * 24 ) / 36.0;

  outputl ( "" );
  outputl ( _("Effective pip count:") );

  outputl ( _("                       "
              "EPC      Wastage") );
  for ( i = 0; i < 2; ++i )
    outputf ( _("%-20.20s   %7.3f  %7.3f\n"),
              ap[ i ].szName,
              arMu[ ms.fMove ? i : !i ] * x,
              arMu[ ms.fMove ? i : !i ] * x - anPips[ ms.fMove ? i : !i ] );

  outputf( _("\n"
             "EPC = Avg. rolls * %5.3f\n" 
             "Wastage = EPC - Pips\n\n" ), x );
           
}


extern void
CommandShowOneChequer ( char *sz ) {

  int anBoard[ 2 ][ 25 ];
  int anPips[ 2 ];
  float arMu[ 2 ];
  float arSigma[ 2 ];
  int i, j;
  float r;
  float aarProb[ 2 ][ 100 ];

  if( !*sz && ms.gs == GAME_NONE ) {
    outputl( _("No position specified and no game in progress.") );
    return;
  }
  
  if( ParsePosition( anBoard, &sz, NULL ) < 0 )
    return;
     

#if USE_GTK
  if ( fX ) {
    GTKShowRace ( 1, anBoard );
    return;
  }
#endif

  /* calculate one chequer bearoff */

  PipCount ( anBoard, anPips );

  for ( i = 0; i < 2; ++i )
    OneChequer ( anPips[ i ], &arMu[ i ], &arSigma[ i ] );

  for ( j = 0; j < 2; ++j )
    for ( i = 0; i < 100; ++i ) 
      aarProb[ j ][ i ] = fnd ( 1.0f * i, arMu[ j ], arSigma[ j ] );
  
  r = 0;
  for ( i = 0; i < 100; ++i )
    for ( j = i; j < 100; ++j )
      r += aarProb[ 1 ][ i ] * aarProb[ 0 ][ j ];

  outputl ( _("Number of rolls to bear off, assuming each player has one "
              "chequer only." ) );
  outputl ( _("                       "
              "Pips     Avg. rolls   Std.dev.") );
  for ( i = 0; i < 2; ++i ) {
    j = ms.fMove ? i : !i;
    outputf ( _("%-20.20s   %4d     %7.3f       %7.3f\n"),
              ap[ i ].szName,
              anPips[ j ], arMu[ j ], arSigma[ j ] );
  }

  outputf ( _("Estimated cubeless gwc (%s on roll): %8.4f%%\n\n"), 
            ap[ ms.fMove ].szName, r * 100.0f );

  /* effective pip count */

  EffectivePipCount( arMu, anPips );

}


extern void
CommandShowOneSidedRollout ( char *sz ) {

  int anBoard[ 2 ][ 25 ];
  int nTrials = 576;
  float arMu[ 2 ];
  float ar[ 5 ];
  int anPips[ 2 ];

  if( !*sz && ms.gs == GAME_NONE ) {
    outputl( _("No position specified and no game in progress.") );
    return;
  }
  
  if( ParsePosition( anBoard, &sz, NULL ) < 0 )
    return;
     

#if USE_GTK
  if ( fX ) {
    GTKShowRace ( 3, anBoard );
    return;
  }
#endif

  outputf ( _("One sided rollout with %d trials (%s on roll):\n"), 
            nTrials, ap[ ms.fMove ].szName );

  raceProbs ( anBoard, nTrials, ar, arMu );
  outputl ( OutputPercents ( ar, TRUE ) );

  PipCount ( anBoard, anPips );
  EffectivePipCount( arMu, anPips );

}


extern void CommandShowMatchEquityTable ( char *sz ) {

  /* Read a number n. */

  int n = ParseNumber ( &sz );
  int i;
  int anScore[ 2 ];

  /* If n > 0 write n x n match equity table,
     else if match write nMatchTo x nMatchTo table,
     else write full table (may be HUGE!) */

  if ( ( n <= 0 ) || ( n > MAXSCORE ) ) {
    if ( ms.nMatchTo )
      n = ms.nMatchTo;
    else
      n = MAXSCORE;
  }

  if ( ms.nMatchTo && ms.anScore[ 0 ] <= n && ms.anScore[ 1 ] <= n ) {
     anScore[ 0 ] = ms.anScore[ 0 ];
     anScore[ 1 ] = ms.anScore[ 1 ];
  }
  else 
     anScore[ 0 ] = anScore[ 1 ] = -1;


#if USE_GTK
  if( fX ) {
      GTKShowMatchEquityTable( n, anScore );
      return;
  }
#endif

  output ( _("Match equity table: ") );
  outputl( miCurrent.szName );
  outputf( "(%s)\n", miCurrent.szFileName );
  outputl( miCurrent.szDescription );
  outputl( "" );
  
  /* write tables */

  output ( _("Pre-Crawford table:\n\n") );
  writeMET ( aafMET, n, n, FALSE );

  for ( i = 0; i < 2; i++ ) {
    outputf ( _("Post-Crawford table for player %d (%s):\n\n"),
              i, ap[ i ].szName );
  writeMET ( (float (*)[MAXSCORE] ) aafMETPostCrawford[ i ], 1, n, TRUE );
  }
  
}

extern void CommandShowOutput( char *sz ) {

    outputf( fOutputMatchPC ? 
             _("Match winning chances will be shown as percentages.\n") :
             _("Match winning chances will be shown as probabilities.\n") );

    if ( fOutputMWC )
	outputl( _("Match equities shown in MWC (match winning chance) "
	      "(match play only).") ); 
    else
	outputl( _("Match equities shown in EMG (normalized money game equity) "
	      "(match play only).") ); 

    outputf( fOutputWinPC ? 
             _("Game winning chances will be shown as percentages.\n") :
             _("Game winning chances will be shown as probabilities.\n") );

#if USE_GUI
    if( !fX )
#endif
      outputf( fOutputRawboard ? 
               _("Boards will be shown in raw format.\n") :
               _("Boards will be shown in ASCII.\n") );
}

extern void CommandShowTraining( char *sz ) {

    outputf( _("Learning rate (alpha) %f,\n"), rAlpha );

    if( rAnneal )
	outputf( _("Annealing rate %f,\n"), rAnneal );
    else
	outputl( _("Annealing disabled,") );

    if( rThreshold )
	outputf( _("Error threshold %f.\n"), rThreshold );
    else
	outputl( _("Error threshold disabled.") );
}

extern void CommandShowVersion( char *sz ) {

    char **ppch = aszVersion;
    extern char *aszCredits[];
    int i = 0, cch, cCol = 0;
    
#if USE_GTK
    if( fX ) {
	GTKShowVersion();
	return;
    }
#endif

    while( *ppch )
	outputl( gettext ( *ppch++ ) );

    outputc( '\n' );

    /* To avoid gnubg.pot include non US-ASCII char. */
    outputf( _("GNU Backgammon was written by %s, %s, %s\n%s, %s and %s.\n\n"),
	       "Joseph Heled", "Øystein Johansen", "David Montgomery",
	       "Jim Segrave", "Jørn Thyssen", "Gary Wong");

    outputl( _("Special thanks to:") );

    cCol = 80;

    for( ppch = aszCredits; *ppch; ppch++ ) {
	i += ( cch = strlen( *ppch ) + 2 );
	if( i >= cCol ) {
	    outputc( '\n' );
	    i = cch;
	}
	
	outputf( "%s%c ", *ppch, *( ppch + 1 ) ? ',' : '.' );
    }

    outputc( '\n' );
}

extern void CommandShowMarketWindow ( char * sz ) {

  cubeinfo ci;

  float arOutput[ NUM_OUTPUTS ];
  float arDP1[ 2 ], arDP2[ 2 ],arCP1[ 2 ], arCP2[ 2 ];
  float rDTW, rDTL, rNDW, rNDL, rDP, rRisk, rGain, r;

  float aarRates[ 2 ][ 2 ];

  int i, fAutoRedouble[ 2 ], afDead[ 2 ], anNormScore[ 2 ];

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("No game in progress (type `new game' to start one).") );

    return;
  }
      
#if USE_GTK
    if( fX ) {
      GTKShowTheory( 0 ); 
      return;
    }
#endif


  /* Show market window */

  /* First, get gammon and backgammon percentages */
  GetMatchStateCubeInfo( &ci, &ms );

  /* see if ratios are given on command line */

  aarRates[ 0 ][ 0 ] = ParseReal ( &sz );

  if ( aarRates[ 0 ][ 0 ] >= 0 ) {

    /* read the others */

    aarRates[ 1 ][ 0 ]  = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;
    aarRates[ 0 ][ 1 ] = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;
    aarRates[ 1 ][ 1 ] = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;

    /* If one of the ratios are larger than 1 we assume the user
       has entered 25.1 instead of 0.251 */

    if ( aarRates[ 0 ][ 0 ] > 1.0 || aarRates[ 1 ][ 0 ] > 1.0 ||
         aarRates[ 1 ][ 1 ] > 1.0 || aarRates[ 1 ][ 1 ] > 1.0 ) {
      aarRates[ 0 ][ 0 ]  /= 100.0;
      aarRates[ 1 ][ 0 ]  /= 100.0;
      aarRates[ 0 ][ 1 ] /= 100.0;
      aarRates[ 1 ][ 1 ] /= 100.0;
    }

    /* Check that ratios are 0 <= ratio <= 1 */

    for ( i = 0; i < 2; i++ ) {
      if ( aarRates[ i ][ 0 ] > 1.0 ) {
        outputf ( _("illegal gammon ratio for player %i: %f\n"),
                  i, aarRates[ i ][ 0 ] );
        return ;
      }
      if ( aarRates[ i ][ 1 ] > 1.0 ) {
        outputf ( _("illegal backgammon ratio for player %i: %f\n"),
                  i, aarRates[ i ][ 1 ] );
        return ;
      }
    }

    /* Transfer rations to arOutput
       (used in call to GetPoints below)*/ 

    arOutput[ OUTPUT_WIN ] = 0.5;
    arOutput[ OUTPUT_WINGAMMON ] =
      ( aarRates[ ms.fMove ][ 0 ] + aarRates[ ms.fMove ][ 1 ] ) * 0.5;
    arOutput[ OUTPUT_LOSEGAMMON ] =
      ( aarRates[ !ms.fMove ][ 0 ] + aarRates[ !ms.fMove ][ 1 ] ) * 0.5;
    arOutput[ OUTPUT_WINBACKGAMMON ] = aarRates[ ms.fMove ][ 1 ] * 0.5;
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = aarRates[ !ms.fMove ][ 1 ] * 0.5;

  } else {

    /* calculate them based on current position */

    if ( getCurrentGammonRates ( aarRates, arOutput, ms.anBoard, 
                                 &ci, &esEvalCube.ec ) < 0 ) 
      return;

  }



  for ( i = 0; i < 2; i++ ) 
    outputf ( _("Player %-25s: gammon rate %6.2f%%, bg rate %6.2f%%\n"),
              ap[ i ].szName, 
              aarRates[ i ][ 0 ] * 100.0, aarRates[ i ][ 1 ] * 100.0);


  if ( ms.nMatchTo ) {

    for ( i = 0; i < 2; i++ )
      anNormScore[ i ] = ms.nMatchTo - ms.anScore[ i ];

    GetPoints ( arOutput, &ci, arCP2 );

    for ( i = 0; i < 2; i++ ) {

      fAutoRedouble [ i ] =
        ( anNormScore[ i ] - 2 * ms.nCube <= 0 ) &&
        ( anNormScore[ ! i ] - 2 * ms.nCube > 0 );

      afDead[ i ] =
        ( anNormScore[ ! i ] - 2 * ms.nCube <=0 );

      /* MWC for "double, take; win" */

      rDTW =
        (1.0 - aarRates[ i ][ 0 ] - aarRates[ i ][ 1 ]) *
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                2 * ms.nCube, i, ms.fCrawford,
                aafMET, aafMETPostCrawford )
        + aarRates[ i ][ 0 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                4 * ms.nCube, i, ms.fCrawford, 
                aafMET, aafMETPostCrawford )
        + aarRates[ i ][ 1 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                6 * ms.nCube, i, ms.fCrawford, 
                aafMET, aafMETPostCrawford );

      /* MWC for "no double, take; win" */

      rNDW =
        (1.0 - aarRates[ i ][ 0 ] - aarRates[ i ][ 1 ]) *
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                ms.nCube, i, ms.fCrawford,
                aafMET, aafMETPostCrawford )
        + aarRates[ i ][ 0 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                2 * ms.nCube, i, ms.fCrawford, 
                aafMET, aafMETPostCrawford )
        + aarRates[ i ][ 1 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                3 * ms.nCube, i, ms.fCrawford, 
                aafMET, aafMETPostCrawford );

      /* MWC for "Double, take; lose" */

      rDTL =
        (1.0 - aarRates[ ! i ][ 0 ] - aarRates[ ! i ][ 1 ]) *
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                2 * ms.nCube, ! i, ms.fCrawford,
                aafMET, aafMETPostCrawford )
        + aarRates[ ! i ][ 0 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                4 * ms.nCube, ! i, ms.fCrawford, 
                aafMET, aafMETPostCrawford )
        + aarRates[ ! i ][ 1 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                6 * ms.nCube, ! i, ms.fCrawford, 
                aafMET, aafMETPostCrawford );

      /* MWC for "No double; lose" */

      rNDL =
        (1.0 - aarRates[ ! i ][ 0 ] - aarRates[ ! i ][ 1 ]) *
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                1 * ms.nCube, ! i, ms.fCrawford,
                aafMET, aafMETPostCrawford )
        + aarRates[ ! i ][ 0 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                2 * ms.nCube, ! i, ms.fCrawford, 
                aafMET, aafMETPostCrawford )
        + aarRates[ ! i ][ 1 ] * 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                3 * ms.nCube, ! i, ms.fCrawford, 
                aafMET, aafMETPostCrawford );

      /* MWC for "Double, pass" */

      rDP = 
        getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                ms.nCube, i, ms.fCrawford,
                aafMET, aafMETPostCrawford );

      /* Double point */

      rRisk = rNDL - rDTL;
      rGain = rDTW - rNDW;

      arDP1 [ i ] = rRisk / ( rRisk + rGain );
      arDP2 [ i ] = arDP1 [ i ];

      /* Dead cube take point without redouble */

      rRisk = rDTW - rDP;
      rGain = rDP - rDTL;

      arCP1 [ i ] = 1.0 - rRisk / ( rRisk + rGain );

      if ( fAutoRedouble[ i ] ) {

        /* With redouble */

        rDTW =
          (1.0 - aarRates[ i ][ 0 ] - aarRates[ i ][ 1 ]) *
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  4 * ms.nCube, i, ms.fCrawford,
                  aafMET, aafMETPostCrawford )
          + aarRates[ i ][ 0 ] * 
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  8 * ms.nCube, i, ms.fCrawford,
                  aafMET, aafMETPostCrawford )
          + aarRates[ i ][ 1 ] * 
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  12 * ms.nCube, i, ms.fCrawford,
                  aafMET, aafMETPostCrawford );

        rDTL =
          (1.0 - aarRates[ ! i ][ 0 ] - aarRates[ ! i ][ 1 ]) *
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  4 * ms.nCube, ! i, ms.fCrawford,
                  aafMET, aafMETPostCrawford )
          + aarRates[ ! i ][ 0 ] * 
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  8 * ms.nCube, ! i, ms.fCrawford,
                  aafMET, aafMETPostCrawford )
          + aarRates[ ! i ][ 1 ] * 
          getME ( ms.anScore[ 0 ], ms.anScore[ 1 ], ms.nMatchTo, i,
                  12 * ms.nCube, ! i, ms.fCrawford,
                  aafMET, aafMETPostCrawford );

        rRisk = rDTW - rDP;
        rGain = rDP - rDTL;

        arCP2 [ i ] = 1.0 - rRisk / ( rRisk + rGain );

        /* Double point */

        rRisk = rNDL - rDTL;
        rGain = rDTW - rNDW;
      
        arDP2 [ i ] = rRisk / ( rRisk + rGain );

      }
    }

    /* output */

    output ( "\n\n" );

    for ( i = 0; i < 2; i++ ) {

      outputf (_("Player %s market window:\n\n"), ap[ i ].szName );

      if ( fAutoRedouble[ i ] )
        output (_("Dead cube (opponent doesn't redouble): "));
      else
        output (_("Dead cube: "));

      outputf ("%6.2f%% - %6.2f%%\n", 100. * arDP1[ i ], 100. * arCP1[
                                                                      i ] );

      if ( fAutoRedouble[ i ] ) 
        outputf (_("Dead cube (opponent redoubles):"
                 "%6.2f%% - %6.2f%%\n\n"),
                 100. * arDP2[ i ], 100. * arCP2[ i ] );
      else if ( ! afDead[ i ] )
        outputf (_("Live cube:"
                 "%6.2f%% - %6.2f%%\n\n"),
                 100. * arDP2[ i ], 100. * arCP2[ i ] );

    }

  } /* ms.nMatchTo */
  else {

    /* money play: use Janowski's formulae */

    /* 
     * FIXME's: (1) make GTK version
     *          (2) make output for "current" value of X
     *          (3) improve layout?
     */

    const char *aszMoneyPointLabel[] = {
      N_("Take Point (TP)"),
      N_("Beaver Point (BP)"),
      N_("Raccoon Point (RP)"),
      N_("Initial Double Point (IDP)"),
      N_("Redouble Point (RDP)"),
      N_("Cash Point (CP)"),
      N_("Too good Point (TP)")
    };

    float aaarPoints[ 2 ][ 7 ][ 2 ];

    int i, j;

    getMoneyPoints ( aaarPoints, ci.fJacoby, ci.fBeavers, aarRates );

    for ( i = 0; i < 2; i++ ) {

      outputf (_("\nPlayer %s cube parameters:\n\n"), ap[ i ].szName );
      outputl ( _("Cube parameter               Dead Cube    Live Cube\n") );

      for ( j = 0; j < 7; j++ ) 
        outputf ( "%-27s  %7.3f%%     %7.3f%%\n",
                  gettext ( aszMoneyPointLabel[ j ] ),
                  aaarPoints[ i ][ j ][ 0 ] * 100.0f,
                  aaarPoints[ i ][ j ][ 1 ] * 100.0f );

    }

  }

}


extern void
CommandShowExport ( char *sz ) {

  int i;

#if USE_GTK 
  if( fX ) {
    GTKShowExport( &exsExport ); 
    return;
  }
#endif

  output ( _("\n" 
           "Export settings: \n\n"
           "WARNING: not all settings are honoured in the export!\n"
           "         Do not expect to much!\n\n"
           "Include: \n\n") );

  output ( _("- annotations") );
  outputf ( "\r\t\t\t\t\t\t: %s\n",
            exsExport.fIncludeAnnotation ? _("yes") : _("no") );
  output ( _("- analysis") );
  outputf ( "\r\t\t\t\t\t\t: %s\n",
            exsExport.fIncludeAnalysis ? _("yes") : _("no") );
  output ( _("- statistics") );
  outputf ( "\r\t\t\t\t\t\t: %s\n",
            exsExport.fIncludeStatistics ? _("yes") : _("no") );
  output ( _("- legend") );
  outputf ( "\r\t\t\t\t\t\t: %s\n\n",
            exsExport.fIncludeLegend ? _("yes") : _("no") );
  output ( _("- match information") );
  outputf ( "\r\t\t\t\t\t\t: %s\n\n",
            exsExport.fIncludeMatchInfo ? _("yes") : _("no") );

  outputl ( _("Show: \n") );
  output ( _("- board" ) );
  output ( "\r\t\t\t\t\t\t: " );
  if ( ! exsExport.fDisplayBoard )
    outputl ( _("never") );
  else
    outputf ( _("on every %d move\n"), 
              exsExport.fDisplayBoard );

  output ( _("- players" ) );
  output ( "\r\t\t\t\t\t\t: " );
  if ( exsExport.fSide == 3 )
    outputl ( _("both") );
  else
    outputf ( "%s\n", 
              ap[ exsExport.fSide - 1 ].szName );

  outputl ( _("\nOutput move analysis:\n") );

  output ( _("- show at most" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputf ( _("%d moves\n"), exsExport.nMoves );

  output ( _("- show detailed probabilities" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.fMovesDetailProb ? _("yes") : _("no") );
  
  output ( _("- show evaluation parameters" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afMovesParameters[ 0 ] ? _("yes") : _("no") );

  output ( _("- show rollout parameters" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afMovesParameters[ 1 ] ? _("yes") : _("no") );

  for ( i = 0; i < N_SKILLS; i++ ) {
    if ( i == SKILL_NONE ) 
      output ( _("- for unmarked moves" ) );
    else
      outputf ( _("- for moves marked '%s'" ), gettext ( aszSkillType[ i ] ) );
    
    output ( "\r\t\t\t\t\t\t: " );
    outputl ( exsExport.afMovesDisplay[ i ] ? _("yes") : _("no") );
    
  }

  outputl ( _("\nOutput cube decision analysis:\n") );

  output ( _("- show detailed probabilities" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.fCubeDetailProb ? _("yes") : _("no") );
  
  output ( _("- show evaluation parameters" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afCubeParameters[ 0 ] ? _("yes") : _("no") );

  output ( _("- show rollout parameters" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afCubeParameters[ 1 ] ? _("yes") : _("no") );

  for ( i = 0; i < N_SKILLS; i++ ) {
    if ( i == SKILL_NONE ) 
      output ( _("- for unmarked cube decisions" ) );
    else
      outputf ( _("- for cube decisions marked '%s'" ), 
                gettext ( aszSkillType[ i ] ) );
    
    output ( "\r\t\t\t\t\t\t: " );
    outputl ( exsExport.afCubeDisplay[ i ] ? _("yes") : _("no") );

  }
  
  output ( _("- actual cube decisions" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afCubeDisplay[ EXPORT_CUBE_ACTUAL ] ? 
            _("yes") : _("no") );

  output ( _("- missed cube decisions" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afCubeDisplay[ EXPORT_CUBE_MISSED ] ? 
            _("yes") : _("no") );

  output ( _("- close cube decisions" ) );
  output ( "\r\t\t\t\t\t\t: " );
  outputl ( exsExport.afCubeDisplay[ EXPORT_CUBE_CLOSE ] ? 
            _("yes") : _("no") );

  outputl ( _("\nHTML options:\n") );

  outputf ( _("- HTML export type used in export\n"
            "\t%s\n"),
            aszHTMLExportType[ exsExport.het ] );


  outputf ( _("- URL to pictures used in export\n"
            "\t%s\n"),
            exsExport.szHTMLPictureURL ? exsExport.szHTMLPictureURL :
            _("not defined") );

  /* PNG options */

  outputl ( _("PNG options:\n") );

  outputf ( _("- size of exported PNG pictures: %dx%d\n"),
            exsExport.nPNGSize * BOARD_WIDTH, exsExport.nPNGSize * BOARD_HEIGHT );

  outputl ( "\n" );

}


extern void
CommandShowPath ( char *sz ) {

  int i;

  char *aszPathNames[] = {
    N_("Export of Encapsulated PostScript .eps files"),
    N_("Import or export of Jellyfish .gam files"),
    N_("Export of HTML files"),
    N_("Export of LaTeX files"),
    N_("Import or export of Jellyfish .mat files"),
    N_("Import of FIBS oldmoves files"),
    N_("Export of PDF files"),
    N_("Import of Jellyfish .pos files"),
    N_("Export of PostScript files"),
    N_("Load and save of SGF files"),
    N_("Import of GamesGrid SGG files"),
    N_("Export of text files"),
    N_("Loading of match equity files (.xml)"),
    N_("Import of TrueMoneyGames TMG files"),
    N_("Import of Snowie .txt files"),
  };

  /* make GTK widget that allows editing of paths */

#if USE_GTK
  if ( fX ) {
    GTKShowPath ();
    return;
  }
#endif

  outputl ( _("Default and current paths "
            "for load, save, import, and export: \n") );

  for ( i = 0; i < NUM_PATHS; ++i ) {

    outputf ( "%s:\n", gettext ( aszPathNames[ i ] ) );
    if ( ! strcmp ( aaszPaths[ i ][ 0 ], "" ) )
      outputl ( _("   Default : not defined") );
    else
      outputf ( _("   Default : \"%s\"\n"), aaszPaths[ i ][ 0 ] );

    if ( ! strcmp ( aaszPaths[ i ][ 1 ], "" ) )
      outputl ( _("   Current : not defined") );
    else
      outputf ( _("   Current : \"%s\"\n"), aaszPaths[ i ][ 1 ] );



  }

}

extern void CommandShowTutor( char *sz ) {

  char *level;

  if( fTutor )
	outputl( _("Warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' moves.") );
  else
	outputl( _("No warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' moves.") );

  if( fTutorCube )
	outputl( _("Warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' cube decisions.") );
  else
	outputl( _("No warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' cube decisions.") );

  if( fTutorChequer )
	outputl( _("Warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' chequer moves.") );
  else
	outputl( _("No warnings are given for \'doubtful\', \'bad\', or "
			   "\'very bad\' chequer moves.") );
  
  if (fTutorAnalysis)
outputl(_("Tutor mode evaluates moves using the same settings as Analysis.") );
  else
	outputl(
	_("Tutor mode evaluates moves using the same settings as Evaluation.") );


  switch (TutorSkill) {
  default:
  case SKILL_DOUBTFUL:
	level = _("doubtful");
	break;

  case SKILL_BAD:
	level = _("bad");
	break;

  case SKILL_VERYBAD:
	level = _("very bad");
	break;
  }

  if ( TutorSkill == SKILL_VERYBAD )
     outputf( _("Warnings are given for '%s' play.\n"), level );
  else
     outputf( _("Warnings are given for '%s' or worse plays.\n"), level );

}

extern void
CommandShowGeometry ( char *sz ) {

  outputf ( _("Default geometries:\n\n"
              "Main window       : size %dx%d, position (%d,%d)\n"
              "Annotation window : size %dx%d, position (%d,%d)\n"
              "Game list window  : size %dx%d, position (%d,%d)\n"
              "Hint window       : size %dx%d, position (%d,%d)\n"
              "Message window    : size %dx%d, position (%d,%d)\n" ),
            awg[ WINDOW_MAIN ].nWidth,
            awg[ WINDOW_MAIN ].nHeight,
            awg[ WINDOW_MAIN ].nPosX,
            awg[ WINDOW_MAIN ].nPosY,
            awg[ WINDOW_ANNOTATION ].nWidth,
            awg[ WINDOW_ANNOTATION ].nHeight,
            awg[ WINDOW_ANNOTATION ].nPosX,
            awg[ WINDOW_ANNOTATION ].nPosY,
            awg[ WINDOW_GAME ].nWidth,
            awg[ WINDOW_GAME ].nHeight,
            awg[ WINDOW_GAME ].nPosX,
            awg[ WINDOW_GAME ].nPosY,
            awg[ WINDOW_HINT ].nWidth,
            awg[ WINDOW_HINT ].nHeight,
            awg[ WINDOW_HINT ].nPosX,
            awg[ WINDOW_HINT ].nPosY,
            awg[ WINDOW_MESSAGE ].nWidth,
            awg[ WINDOW_MESSAGE ].nHeight,
            awg[ WINDOW_MESSAGE ].nPosX,
            awg[ WINDOW_MESSAGE ].nPosY );

}


extern void CommandShowHighlightColour ( char *sz ) {

  outputf ( _("Moves will be highlighted in %s %s.\n"),
              HighlightIntensity == 2 ? "dark" :
              HighlightIntensity == 1 ? "medium" : "",
			  HighlightColour->colourname);
}



#if USE_SOUND

extern void
CommandShowSound ( char *sz ) {

  int i;

#if USE_GTK

  if ( fX ) {
    /* GTKSound(); */
    /* return */;
  }

#endif /* USE_GTK */

  outputf ( _("Sounds are enabled          : %s\n"
              "Sound system                : %s\n"),
            fSound ? _("yes") : _("no"),
            gettext ( aszSoundSystem[ ssSoundSystem ] ) );

  outputl ( _("Sounds for:") );

  for ( i = 0; i < NUM_SOUNDS; ++i ) 
    if ( ! *aszSound[ i ] )
      outputf ( _("   %-30.30s : no sound\n"),
                gettext ( aszSoundDesc[ i ] ) );
    else
      outputf ( _("   %-30.30s : \"%s\"\n"),
                gettext ( aszSoundDesc[ i ] ),
                aszSound[ i ] );
}

#endif /* USE_SOUND */


extern void
CommandShowRolls ( char *sz ) {

#if USE_GTK2
  int nDepth = ParseNumber ( &sz );
#endif

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("No game in progress (type `new game' to start one).") );

    return;
  }

#if USE_GTK2

  if ( fX ) {
    static evalcontext ec0ply = { TRUE, 0, 0, TRUE, 0.0 };
    GTKShowRolls( nDepth, &ec0ply, &ms );
    return;
  }
#endif

  CommandNotImplemented( NULL );

}



extern void
CommandShowTemperatureMap( char *sz ) {

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("No game in progress (type `new game' to start one).") );

    return;
  }

#if USE_GTK

  if ( fX ) {

    if ( sz && *sz && ! strncmp( sz, "=cube", 5 ) ) {

      cubeinfo ci;
      GetMatchStateCubeInfo( &ci, &ms );
      if ( GetDPEq( NULL, NULL, &ci ) ) {
        
        /* cube is available */
        
        matchstate ams[ 2 ];
        int i;
        gchar *asz[ 2 ];
        
        for ( i = 0; i < 2; ++i )
          memcpy( &ams[ i ], &ms, sizeof( matchstate ) );
        
        ams[ 1 ].nCube *= 2;
        ams[ 1 ].fCubeOwner = ! ams[ 1 ].fMove;
        
        for ( i = 0; i < 2; ++i ) {
          asz[ i ] = g_malloc( 200 );
          GetMatchStateCubeInfo( &ci, &ams[ i ] );
          FormatCubePosition( asz[ i ], &ci );
        }
        
        GTKShowTempMap( ams, 2, (const gchar **) asz, FALSE );
        
        for ( i = 0; i < 2; ++i )
          g_free( asz[ i ] );
        
      }
      else
        outputl( _("Cube is not available.") );
      
    }
    else
      GTKShowTempMap( &ms, 1, NULL, FALSE );
    
    return;
  }
#endif

  CommandNotImplemented( NULL );

}  

extern void
CommandShowVariation( char *sz ) {

  if ( ms.gs != GAME_NONE )
    outputf( _("You are playing: %s\n"), aszVariations[ ms.bgv ] );

  outputf( _("Default variation is: %s\n"), aszVariations[ bgvDefault ] );

}

extern void
CommandShowCheat( char *sz ) {

  outputf( _("Manipulation with dice is %s.\n"),
           fCheat ? _("enabled") : _("disabled") );
  PrintCheatRoll( 0, afCheatRoll[ 0 ] );
  PrintCheatRoll( 1, afCheatRoll[ 1 ] );

}


extern void
CommandShowCubeEfficiency( char *sz ) {


  outputf( _("Parameters for cube evaluations:\n"
             "Cube efficiency for crashed positions           : %7.4f\n"
             "Cube efficiency for context positions           : %7.4f\n"
             "Cube efficiency for one sided bearoff positions : %7.4f\n"
             "Cube efficiency for race: x = pips * %.5f + %.5f\n"
             "(min value %.4f, max value %.4f)\n"),
           rCrashedX, rContactX, rOSCubeX,
           rRaceFactorX, rRaceCoefficientX, rRaceMin, rRaceMax );

}


extern void
CommandShowBearoff( char *sz ) {

  char szTemp[ 2048 ];

#if USE_GTK
  if ( fX ) {
    GTKShowBearoff( &ms );
    return;
  }

#endif  


  switch( ms.bgv ) {
  case VARIATION_STANDARD:
  case VARIATION_NACKGAMMON:
    
    /* Sconyer's huge database */
    if ( isBearoff( pbc15x15_dvd, ms.anBoard ) ) {
      strcpy( szTemp, "" );
      ShowBearoff( szTemp, &ms, pbc15x15_dvd );
      outputl( szTemp );
    } 
    
    /* gnubg's own databases */
    if ( isBearoff( pbcTS, ms.anBoard ) ) {
      strcpy( szTemp, "" );
      ShowBearoff( szTemp, &ms, pbcTS );
      outputl( szTemp );
    } 
    else if ( isBearoff( pbc2, ms.anBoard ) ) {
      strcpy( szTemp, "" );
      ShowBearoff( szTemp, &ms, pbc2 );
      outputl( szTemp );
    }

    break;

  case VARIATION_HYPERGAMMON_1:
  case VARIATION_HYPERGAMMON_2:
  case VARIATION_HYPERGAMMON_3:

    if ( isBearoff( apbcHyper[ ms.bgv - VARIATION_HYPERGAMMON_1 ], 
                    ms.anBoard ) ) {
      strcpy( szTemp, "" );
      ShowBearoff( szTemp, &ms, 
                    apbcHyper[ ms.bgv - VARIATION_HYPERGAMMON_1 ] );
      outputl( szTemp );
    }

    break;

  default:

    assert( FALSE );

  }
    
}

extern void
ShowBearoff( char *sz, matchstate *pms, bearoffcontext *pbc ) {

  if ( pms->anDice[ 0 ] <= 0 ) {
    /* no dice rolled */
    if ( isBearoff( pbc, pms->anBoard ) )
      BearoffDump( pbc, pms->anBoard, sz );
    else
      sprintf( sz, _("Position not in database") );
  }
  else {
    /* dice rolled */
    movelist ml;
    int aiBest[ 4 ];
    float arBest[ 4 ];
    float arEquity[ 4 ];
    int anBoard[ 2 ][ 25 ];
    int i, j;
    static char *aszCube[] = {
      N_("No cube"),
      N_("Owned cube"),
      N_("Centered cube"),
      N_("Opponent owns cube")
    };
    char szMove[ 33 ];
    static int aiPerm[ 4 ] = { 0, 3, 2, 1 };

    assert ( pms->bgv <= VARIATION_NACKGAMMON );
    
    GenerateMoves ( &ml, pms->anBoard, 
                    pms->anDice[ 0 ], pms->anDice[ 1 ], FALSE );

    if ( ! ml.cMoves ) {
      sprintf( sz, _("No legal moves!") );
      return;
    }

    /* find best move */

    for ( i = 0; i < 4; ++i ) {
      aiBest[ i ] = -1;
      arBest[ i ] = -9999.0f;
    }
    
    for ( i = 0; i < ml.cMoves; ++i ) {

      PositionFromKey( anBoard, ml.amMoves[ i ].auch );
      SwapSides( anBoard );

      if ( isBearoff( pbc, anBoard ) ) {
        if ( PerfectCubeful( pbc, anBoard, arEquity ) ) {
          sprintf( sz, _("Interrupted!") );
          return;
        }
      }
      else {
        /* whoops, the resulting position is not in the bearoff database */
        sprintf( sz, _("The position %s is not in the bearoff database"),
                 PositionID( anBoard ) );
        return;
      }

      for ( j = 0; j < 4; ++j )
        if ( -arEquity[ j ] > arBest[ j ] ) {
          arBest[ j ] = - arEquity[ j ];
          aiBest[ j ] = i;
        }
    }

    /* output */

    sprintf( sz, _("Equities after rolling %d%d:\n\n"),
             pms->anDice[ 0 ], pms->anDice[ 1 ] );
    sprintf( strchr( sz, 0 ),
             "%-20.20s %-10.10s %-32.32s\n",
             _("Cube Position"), _("Equity"), _("Best Move") );

    for ( i = 0; i < 4; ++i ) {

      j = aiPerm[ i ];
      sprintf( strchr( sz, 0 ), "%-20.20s %+7.4f    %-32.32s\n",
               gettext( aszCube[ i ] ),
               arBest[ j ],
               FormatMove( szMove, pms->anBoard, 
                           ml.amMoves[ aiBest[ j ] ].anMove ) );

    }

  }

}


extern void
CommandShowMatchResult( char *sz ) {

  float arSum[ 2 ] = { 0.0f, 0.0f };
  float arSumSquared[ 2 ] = { 0.0f, 0.0f };
  int n = 0;
  movegameinfo *pmgi;
  statcontext *psc;
  list *pl;
  float r;

  updateStatisticsMatch ( &lMatch );

  outputf( _("Actual and luck adjusted results for %s\n\n"),
           ap[ 0 ].szName );
  outputl( _("Game   Actual     Luck adj.\n") );

  for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext, ++n ) {
  
      pmgi = ( (list *) pl->p )->plNext->p;
      assert( pmgi->mt == MOVE_GAMEINFO );
      
      psc = &pmgi->sc;
      
      if ( psc->fDice ) {
        if ( ms.nMatchTo ) 
          outputf( "%4d   %8.2f%%    %8.2f%%\n",
                   n,
                   100.0 * ( 0.5f + psc->arActualResult[ 0 ] ),
                   100.0 * ( 0.5f + psc->arActualResult[ 0 ] - 
                             psc->arLuck[ 0 ][ 1 ] + psc->arLuck[ 1 ][ 1 ] ) );
        else
          outputf( "%4d   %+9.3f   %+9.3f\n",
                   n, 
                   psc->arActualResult[ 0 ],
                   psc->arActualResult[ 0 ] - 
                   psc->arLuck[ 0 ][ 1 ] + psc->arLuck[ 1 ][ 1 ] );
      }
      else
        outputf( _("%d   no info avaiable\n"), n );
      
      r = psc->arActualResult[ 0 ];
      arSum[ 0 ] += r;
      arSumSquared[ 0 ] += r * r;

      r = psc->arActualResult[ 0 ] - 
        psc->arLuck[ 0 ][ 1 ] + psc->arLuck[ 1 ][ 1 ];
      arSum[ 1 ] += r;
      arSumSquared[ 1 ] += r * r;
  
      
  }

  if ( ms.nMatchTo )
    outputf( _("Final  %8.2f%%   %8.2f%%\n"),
             100.0 * ( 0.5f + arSum[ 0 ] ),
             100.0 * ( 0.5f + arSum[ 1 ] ) );
  else
    outputf( _("Sum    %+9.3f   %+9.3f\n"),
             arSum[ 0 ], arSum[ 1 ] );

  if ( n && ! ms.nMatchTo ) {
    outputf( _("Ave.   %+9.3f   %+9.3f\n"),
             arSum[ 0 ] / n , arSum[ 1 ] / n );
    outputf( _("95%%CI  %9.3f   %9.3f\n" ),
             1.95996f *
             sqrt ( arSumSquared[ 0 ] / n - 
                    arSum[ 0 ] * arSum[ 0 ] / ( n * n ) ) / sqrt( n ),
             1.95996f *
             sqrt ( arSumSquared[ 1 ] / n - 
                    arSum[ 1 ] * arSum[ 1 ] / ( n * n ) ) / sqrt( n ) );
  }
            
}

#if USE_TIMECONTROL

#include "tctutorial.h"

extern void CommandShowTCTutorial( char *sz ) {
#if USE_GTK
    if ( fX )
	ShowList( aszTcTutorial, _("Time Control Tutorial") );
    else
#endif
	ShowPaged(aszTcTutorial);
}
#endif


extern void CommandShowDisplayPanels( char *sz ) {
  outputf( _("Game list, Annotation and Message panels/windows "
             "will%s be displayed."), fDisplayPanels ? "": " not");
}
