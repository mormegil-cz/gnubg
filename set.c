/*
 * set.c
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

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif /* HAVE_LIMITS_H */

#include <math.h>

#if HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */

#ifndef WIN32

#if HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#else /* #ifndef WIN32 */
#include <winsock.h>
#endif /* #ifndef WIN32 */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#include "external.h"
#include "export.h"

#if USE_GTK
#include "gtkgame.h"
#include "gtkprefs.h"
#endif /* USE_GTK */

#include "matchequity.h"
#include "positionid.h"
#include "renderprefs.h"
#include "export.h"
#include "drawboard.h"
#include "format.h"
#include "boarddim.h"

#include "i18n.h"

#include "sound.h"

#if USE_TIMECONTROL
#include "timecontrol.h"
#endif

static int iPlayerSet, iPlayerLateSet;

static evalcontext *pecSet;
static char *szSet, *szSetCommand;
static rolloutcontext *prcSet;

static evalsetup *pesSet;

static rng *rngSet;

movefilter *aamfSet[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];


static char szFILENAME[] = N_ ("<filename>"),
    szNAME[] = N_ ("<name>"),
    szNUMBER[] = N_ ("<number>"),
    szONOFF[] = N_ ("on|off"),
    szPLIES[] = N_ ("<plies>"),
    szSTDDEV[] = N_ ("<std dev>"),
    szFILTER[] = N_ ( "<ply> <num. to accept (0 = skip)> "
                      "[<num. of extra moves to accept> <tolerance>]");
command acSetEvaluation[] = {
    { "cubeful", CommandSetEvalCubeful, N_("Cubeful evaluations"), szONOFF,
      &cOnOff },
    { "deterministic", CommandSetEvalDeterministic, N_("Specify whether added "
      "noise is determined by position"), szONOFF, &cOnOff },
    { "noise", CommandSetEvalNoise, N_("Distort evaluations with noise"),
      szSTDDEV, NULL },
    { "plies", CommandSetEvalPlies, N_("Choose how many plies to look ahead"),
      szPLIES, NULL },
    { "reduced", CommandSetEvalReduced,
      N_("Control how thoroughly deep plies are searched"), szNUMBER, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetPlayer[] = {
    { "chequerplay", CommandSetPlayerChequerplay, N_("Control chequerplay "
      "parameters when gnubg plays"), NULL, acSetEvalParam },
    { "cubedecision", CommandSetPlayerCubedecision, N_("Control cube decision "
      "parameters when gnubg plays"), NULL, acSetEvalParam },
    { "external", CommandSetPlayerExternal, N_("Have another process make all "
      "moves for a player"), szFILENAME, &cFilename },
    { "gnubg", CommandSetPlayerGNU, 
      N_("Have gnubg make all moves for a player"),
      NULL, NULL },
    { "human", CommandSetPlayerHuman, N_("Have a human make all moves for a "
      "player"), NULL, NULL },
    { "movefilter", CommandSetPlayerMoveFilter, 
      N_("Set parameters for choosing moves to evaluate"), 
      szFILTER, NULL},
    { "name", CommandSetPlayerName, 
      N_("Change a player's name"), szNAME, NULL },
    { "pubeval", CommandSetPlayerPubeval, 
      N_("Have pubeval make all moves for a "
      "player"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
};

static void
SetSeed ( const rng rngx, char *sz ) {
    
    if( rngx == RNG_MANUAL || rngx == RNG_RANDOM_DOT_ORG ) {
	outputl( _("You can't set a seed "
                   "if you're using manual dice generation or random.org") );
	return;
    }

    if( *sz ) {
#if HAVE_LIBGMP
	if( InitRNGSeedLong( sz, rngx ) )
	    outputl( _("You must specify a valid seed -- try `help set "
		       "seed'.") );
	else
	    outputf( _("Seed set to %s.\n"), sz );
#else
	int n;

	n = ParseNumber( &sz );

	if( n < 0 ) {
	    outputl( _("You must specify a valid seed -- try `help set seed'.") );

	    return;
	}

	InitRNGSeed( n, rngx );
	outputf( _("Seed set to %d.\n"), n );
#endif /* HAVE_LIBGMP */
    } else
	outputl( InitRNG( NULL, TRUE, rngx ) ?
		 _("Seed initialised from system random data.") :
		 _("Seed initialised by system clock.") );
}

static void SetRNG( rng *prng, rng rngNew, char *szSeed ) {

    if( *prng == rngNew && !*szSeed ) {
	outputf( _("You are already using the %s generator.\n"),
		gettext ( aszRNG[ rngNew ] ) );
	return;
    }
    
    /* Dispose dynamically linked user module if necesary */
    if ( *prng == RNG_USER )
#if HAVE_LIBDL
	UserRNGClose();
#else
        abort();
#endif /* HAVE_LIBDL */
    
    /* close file if necesary */    
    if ( *prng == RNG_FILE )
      CloseDiceFile();

    /* check for RNG-dependent pre-initialisation */
    switch( rngNew ) {
    case RNG_BBS:
#if HAVE_LIBGMP
	  {
		char *sz, *sz1;
		int fInit;

		fInit = FALSE;
	
		if( *szSeed ) {
		  if( !strncasecmp( szSeed, "modulus", strcspn( szSeed,
												" \t\n\r\v\f" ) ) ) {
			NextToken( &szSeed ); /* skip "modulus" keyword */
			if( InitRNGBBSModulus( NextToken( &szSeed ) ) ) {
			  outputf( _("You must specify a valid modulus (see `help "
						 "set rng bbs').") );
			  return;
			}
			fInit = TRUE;
		  } else if( !strncasecmp( szSeed, "factors",
				     strcspn( szSeed, " \t\n\r\v\f" ) ) ) {
			NextToken( &szSeed ); /* skip "modulus" keyword */
			sz = NextToken( &szSeed );
			sz1 = NextToken( &szSeed );
			if( InitRNGBBSFactors( sz, sz1 ) ) {
			  outputf( _("You must specify two valid factors (see `help "
						 "set rng bbs').") );
			  return;
			}
			fInit = TRUE;
		  }
		}
	
		if( !fInit )
		  /* use default modulus, with factors
			 148028650191182616877187862194899201391 and
			 315270837425234199477225845240496832591. */
		  InitRNGBBSModulus( "46669116508701198206463178178218347698370"
							 "262771368237383789001446050921334081" );
		break;
	  }
#else
	abort();
#endif /* HAVE_LIBGMP */
	
    case RNG_USER:
	/* Load dynamic library with user RNG */
#if HAVE_LIBDL
	  {
		char *sz = NULL;
	
		if( *szSeed ) {
		  char *pch;
	    
		  for( pch = szSeed; isdigit( *pch ); pch++ )
			;
	    
		  if( *pch && !isspace( *pch ) )
			/* non-numeric char found; assume it's the user RNG file */
			sz = NextToken( &szSeed );
		}
	
		if ( !UserRNGOpen( sz ? sz : "userrng.so" ) ) {
		  outputf( _("You are still using the %s generator.\n"),
				   gettext ( aszRNG[ *prng ] ) );
		  return;
		}
	  }
#else
	abort();
#endif /* HAVE_LIBDL */
	break;

    case RNG_FILE: 
      {
        char *sz = NextToken( &szSeed );

        if ( !sz || !*sz ) {
          outputl( _("Please enter filename!") );
          return;
        }

        if ( OpenDiceFile( sz ) < 0 ) {
          outputf( _("File %s does not exist or is not readable"), sz );
          return;
        }

      }
      break;
      

    default:
	;
    }
	    
    outputf( _("GNU Backgammon will now use the %s generator.\n"),
	     gettext ( aszRNG[ rngNew ] ) );
    
    switch ( ( *prng = rngNew ) ) {
    case RNG_MANUAL:
    case RNG_RANDOM_DOT_ORG:
    case RNG_FILE:
      /* no-op */
      break;

    default:
      SetSeed( *prng, szSeed );
      break;
    }

}


static void
SetMoveFilter(char* sz, 
              movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int		  ply = ParseNumber( &sz );
  int		  level;
  int		  accept;
  movefilter *pmfFilter; 
  int		  extras;
  float 	  tolerance;

  if (ply < 0) {
	outputl ( N_("You must specify for which ply you want to set a filter") );
	return;
  }

  if ( ! ( 0 < ply &&  ply <= MAX_FILTER_PLIES ) ) {
	outputf( _("You must specify a valid ply for setting move filters -- try "
			   "help set %s movefilter"), szSetCommand );
	return;
  }

  if (((level = ParseNumber( &sz ) ) < 0) || (level >= ply)) {
	outputf( _("You must specify a valid level 0..%d for the filter -- try "
			   "help set %s movefilter"), ply - 1, szSetCommand );
	return;
  }

  pmfFilter = &aamf[ply-1][level];

  if ((accept = ParseNumber( &sz ) ) == INT_MIN ) {
	outputf (N_ ("You must specify a number of moves to accept (or a negative number to skip "
			 "this level -- try help set %s movefilter "), szSetCommand);
	return;
  }

  if (accept < 0 ) {
	pmfFilter->Accept = -1;
	pmfFilter->Extra = 0;
	pmfFilter->Threshold = 0.0;
	return;
  }

  if ( ( ( extras = ParseNumber( &sz ) ) < 0 )  || 
	   ( ( tolerance = ParseReal( &sz ) ) < 0.0 )) {
	outputf (N_ ("You must set a count of extra moves and a search tolerance "
				 "-- try help set %s movefilter plies level accept"), 
			 szSetCommand);
	return;
  }

  pmfFilter->Accept = accept;
  pmfFilter->Extra = extras;
  pmfFilter->Threshold = tolerance;
}




extern void CommandSetAnalysisCube( char *sz ) {

    if( SetToggle( "analysis cube", &fAnalyseCube, sz,
		   _("Cube action will be analysed."),
		   _("Cube action will not be analysed.") ) >= 0 )
	UpdateSetting( &fAnalyseCube );
}

extern void CommandSetAnalysisLimit( char *sz ) {
    
    int n;
    
    if( ( n = ParseNumber( &sz ) ) <= 0 ) {
	cAnalysisMoves = -1;
	outputl( _("Every legal move will be analysed.") );
    } else if( n >= 2 ) {
	cAnalysisMoves = n;
	outputf( _("Up to %d moves will be analysed.\n"), n );
    } else {
	outputl( _("If you specify a limit on the number of moves to analyse, "
		 "it must be at least 2.") );
	return;
    }

    if( !fAnalyseMove )
	outputl( _("(Note that no moves will be analysed until enable chequer "
		 "play analysis -- see\n`help set analysis moves'.)") );
}

extern void CommandSetAnalysisLuck( char *sz ) {

    if( SetToggle( "analysis luck", &fAnalyseDice, sz,
		   _("Dice rolls will be analysed."),
		   _("Dice rolls will not be analysed.") ) >= 0 )
	UpdateSetting( &fAnalyseDice );
}

extern void
CommandSetAnalysisLuckAnalysis ( char *sz ) {


    szSet = _("luck analysis");
    szSetCommand = "set analysis luckanalysis";
    pecSet = &ecLuck;
    HandleCommand( sz, acSetEvaluation );

}


extern void CommandSetAnalysisMoves( char *sz ) {

    if( SetToggle( "analysis moves", &fAnalyseMove, sz,
		   _("Chequer play will be analysed."),
		   _("Chequer play will not be analysed.") ) >= 0 )
	UpdateSetting( &fAnalyseMove );
}

static void SetLuckThreshold( lucktype lt, char *sz ) {

    double r = ParseReal( &sz );
    char *szCommand = gettext ( aszLuckTypeCommand[ lt ] );

    if( r <= 0.0 ) {
	outputf( _("You must specify a positive number for the threshold (see "
		 "`help set analysis\nthreshold %s').\n"), szCommand );
	return;
    }

    arLuckLevel[ lt ] = r;

    outputf( _("`%s' threshold set to %.3f.\n"), szCommand, r );
}

static void SetSkillThreshold( skilltype lt, char *sz ) {

    double r = ParseReal( &sz );
    char *szCommand = gettext ( aszSkillTypeCommand[ lt ] );

    if( r < 0.0 ) {
	outputf( _("You must specify a semi-positive number for the threshold (see "
		 "`help set analysis\nthreshold %s').\n"), szCommand );
	return;
    }

    arSkillLevel[ lt ] = r;

    outputf( _("`%s' threshold set to %.3f.\n"), szCommand, r );
}

extern void CommandSetAnalysisThresholdBad( char *sz ) {

    SetSkillThreshold( SKILL_BAD, sz );
}

/* extern void CommandSetAnalysisThresholdGood( char *sz ) { */

/*     SetSkillThreshold( SKILL_GOOD, sz ); */
/* } */

extern void CommandSetAnalysisThresholdDoubtful( char *sz ) {

    SetSkillThreshold( SKILL_DOUBTFUL, sz );
}

/* extern void CommandSetAnalysisThresholdInteresting( char *sz ) { */

/*     SetSkillThreshold( SKILL_INTERESTING, sz ); */
/* } */

extern void CommandSetAnalysisThresholdLucky( char *sz ) {

    SetLuckThreshold( LUCK_GOOD, sz );
}

extern void CommandSetAnalysisThresholdUnlucky( char *sz ) {

    SetLuckThreshold( LUCK_BAD, sz );
}

extern void CommandSetAnalysisThresholdVeryBad( char *sz ) {

    SetSkillThreshold( SKILL_VERYBAD, sz );
}

/* extern void CommandSetAnalysisThresholdVeryGood( char *sz ) { */

/*     SetSkillThreshold( SKILL_VERYGOOD, sz ); */
/* } */

extern void CommandSetAnalysisThresholdVeryLucky( char *sz ) {

    SetLuckThreshold( LUCK_VERYGOOD, sz );
}

extern void CommandSetAnalysisThresholdVeryUnlucky( char *sz ) {

    SetLuckThreshold( LUCK_VERYBAD, sz );
}

extern void CommandSetAnnotation( char *sz ) {

    if( SetToggle( "annotation", &fAnnotation, sz,
		   _("Move analysis and commentary will be displayed."),
		   _("Move analysis and commentary will not be displayed.") )
	>= 0 )
	/* Force an update, even if the setting has not changed. */
	UpdateSetting( &fAnnotation );
}

extern void CommandSetMessage( char *sz ) {

    if( SetToggle( "message", &fMessage, sz,
		   _("Show window with messages"),
		   _("Do not show window with message.") )
	>= 0 )
	/* Force an update, even if the setting has not changed. */
	UpdateSetting( &fMessage );
}

extern void CommandSetAutoBearoff( char *sz ) {

    SetToggle( "automatic bearoff", &fAutoBearoff, sz, _("Will automatically "
	       "bear off as many chequers as possible."), _("Will not "
	       "automatically bear off chequers.") );
}

extern void CommandSetAutoCrawford( char *sz ) {

    SetToggle( "automatic crawford", &fAutoCrawford, sz, _("Will enable the "
	       "Crawford game according to match score."), _("Will not "
	       "enable the Crawford game according to match score.") );
}

extern void CommandSetAutoDoubles( char *sz ) {

    int n;
    
    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	outputl( _("You must specify how many automatic doubles to use "
	      "(try `help set automatic double').") );
	return;
    }

    if( n > 12 ) {
	outputl( _("Please specify a smaller limit (up to 12 automatic "
	      "doubles).") );
	return;
    }
	
    if( ( cAutoDoubles = n ) > 1 )
	outputf( _("Automatic doubles will be used "
                   "(up to a limit of %d).\n"), n );
    else if( cAutoDoubles )
	outputl( _("A single automatic double will be permitted.") );
    else
	outputl( _("Automatic doubles will not be used.") );

    UpdateSetting( &cAutoDoubles );
    
    if( cAutoDoubles ) {
	if( ms.nMatchTo > 0 )
	    outputl( _("(Note that automatic doubles will have "
                       "no effect until you "
                       "start session play.)") );
	else if( !ms.fCubeUse )
	    outputl( _("(Note that automatic doubles will have no effect "
                       "until you "
                       "enable cube use --\nsee `help set cube use'.)") );
    }
}

extern void CommandSetAutoGame( char *sz ) {

    SetToggle( "automatic game", &fAutoGame, sz, 
               _("Will automatically start games after wins."), 
               _("Will not automatically start games.") );
}

extern void CommandSetAutoMove( char *sz ) {

    SetToggle( "automatic move", &fAutoMove, sz, 
               _("Forced moves will be made automatically."), 
               _("Forced moves will not be made automatically.") );
}

extern void CommandSetAutoRoll( char *sz ) {

    SetToggle( "automatic roll", &fAutoRoll, sz, 
               _("Will automatically roll the "
                 "dice when no cube action is possible."), 
               _("Will not automatically roll the dice.") );
}

extern void CommandSetBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    movesetboard *pmsb;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("There must be a game in progress to set the board.") );

	return;
    }

    if( !*sz ) {
	outputl( _("You must specify a position -- see `help set board'.") );

	return;
    }

    /* FIXME how should =n notation be handled? */
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

    pmsb = malloc( sizeof( *pmsb ) );
    pmsb->mt = MOVE_SETBOARD;
    pmsb->sz = NULL;
    
    if( ms.fMove )
	SwapSides( an );
    PositionKey( an, pmsb->auchKey );
    
    AddMoveRecord( pmsb );
    
    ShowBoard();
}

static int CheckCubeAllowed( void ) {
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("There must be a game in progress to set the cube.") );
	return -1;
    }

    if( ms.fCrawford ) {
	outputl( _("The cube is disabled during the Crawford game.") );
	return -1;
    }
    
    if( !ms.fCubeUse ) {
	outputl( _("The cube is disabled (see `help set cube use').") );
	return -1;
    }

    return 0;
}

extern void CommandSetCache( char *sz ) {

    int n;

    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	outputl( _("You must specify the number of cache entries to use.") );

	return;
    }

    if( EvalCacheResize( n ) )
	outputerr( "EvalCacheResize" );
    else {
      if ( n == 1 )
	outputf( _("The position cache has been sized to %d entry.\n"), n );
      else
	outputf( _("The position cache has been sized to %d entries.\n"), n );
    }

}

extern void CommandSetCalibration( char *sz ) {

    float r;

    if( !sz || !*sz ) {
	rEvalsPerSec = -1.0f;
	outputl( _("The evaluation speed has been cleared.") );
	return;
    }
    
    if( ( r = ParseReal( &sz ) ) <= 2.0f ) {
	outputl( _("If you give a parameter to `set calibration', it must "
		   "be a legal number of evaluations per second.") );
	return;
    }

    rEvalsPerSec = r;

    outputf( _("The speed estimate has been set to %.0f static "
	       "evaluations per second.\n"), rEvalsPerSec );
}

extern void CommandSetClockwise( char *sz ) {

    SetToggle( "clockwise", &fClockwise, sz, 
               _("Player 1 moves clockwise (and "
                 "player 0 moves anticlockwise)."), 
               _("Player 1 moves anticlockwise (and "
                 "player 0 moves clockwise).") );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetAppearance( char *sz ) {
    
    char *apch[ 2 ];
    
#if USE_GTK
    if( fX )
	BoardPreferencesStart( pwBoard );
#endif /* USE_GTK */
    
	while( ParseKeyValue( &sz, apch ) )
		RenderPreferencesParam( &rdAppearance, apch[ 0 ], apch[ 1 ] );

#if USE_GTK
	if( fX )
		BoardPreferencesDone( pwBoard );	    
#endif /* USE_GTK */
}

extern void CommandSetConfirmNew( char *sz ) {
    
    SetToggle( "confirm new", &fConfirm, sz, 
               _("Will ask for confirmation before "
                 "aborting games in progress."), 
               _("Will not ask for confirmation "
	       "before aborting games in progress.") );
}

extern void CommandSetConfirmSave( char *sz ) {
    
    SetToggle( "confirm save", &fConfirmSave, sz, 
               _("Will ask for confirmation before "
	       "overwriting existing files."), 
               _("Will not ask for confirmation "
	       "overwriting existing files.") );
}

extern void CommandSetCubeCentre( char *sz ) {

    movesetcubepos *pmscp;
    
    if( CheckCubeAllowed() )
	return;
    
    pmscp = malloc( sizeof( *pmscp ) );
    pmscp->mt = MOVE_SETCUBEPOS;
    pmscp->sz = NULL;
    pmscp->fCubeOwner = -1;
    
    AddMoveRecord( pmscp );
    
    outputl( _("The cube has been centred (either player may double).") );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetCubeOwner( char *sz ) {

    movesetcubepos *pmscp;
    
    int i;
    
    if( CheckCubeAllowed() )
	return;

    switch( i = ParsePlayer( NextToken( &sz ) ) ) {
    case 0:
    case 1:
	break;
	
    case 2:
	/* "set cube owner both" is the same as "set cube centre" */
	CommandSetCubeCentre( NULL );
	return;

    default:
	outputl( _("You must specify which player owns the cube "
                   "(see `help set cube owner').") );
	return;
    }

    pmscp = malloc( sizeof( *pmscp ) );
    pmscp->mt = MOVE_SETCUBEPOS;
    pmscp->sz = NULL;
    pmscp->fCubeOwner = i;
    
    AddMoveRecord( pmscp );
    
    outputf( _("%s now owns the cube.\n"), ap[ ms.fCubeOwner ].szName );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetCubeUse( char *sz ) {

    if( SetToggle( "cube use", &fCubeUse, sz, 
                   _("Use of the doubling cube is permitted."),
		   _("Use of the doubling cube is disabled.") ) < 0 )
	return;

    if( !ms.nMatchTo && ms.fJacoby && !fCubeUse )
	outputl( _("(Note that you'll have to disable the Jacoby rule "
                   "if you want gammons and\nbackgammons to be scored "
                   "-- see `help set jacoby').") );
    
    if( ms.fCrawford && fCubeUse )
	outputl( _("(But the Crawford rule is in effect, "
                   "so you won't be able to use it during\nthis game.)") );
    else if( ms.gs == GAME_PLAYING && !fCubeUse ) {
	/* The cube was being used and now it isn't; reset it to 1,
	   centred. */
	ms.nCube = 1;
	ms.fCubeOwner = -1;
	UpdateSetting( &ms.nCube );
	UpdateSetting( &ms.fCubeOwner );
	CancelCubeAction();
    }

    ms.fCubeUse = fCubeUse;
	
#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetCubeValue( char *sz ) {

    int i, n;
    movesetcubeval *pmscv;
    
    if( CheckCubeAllowed() )
	return;
    
    n = ParseNumber( &sz );

    for( i = MAX_CUBE; i; i >>= 1 )
	if( n == i ) {
	    pmscv = malloc( sizeof( *pmscv ) );
	    pmscv->mt = MOVE_SETCUBEVAL;
	    pmscv->sz = NULL;
	    pmscv->nCube = n;

	    AddMoveRecord( pmscv );
	    
	    outputf( _("The cube has been set to %d.\n"), n );
	    
#if USE_GUI
	    if( fX )
		ShowBoard();
#endif /* USE_GUI */
	    return;
	}

    outputl( _("You must specify a legal cube value (see `help set cube "
	     "value').") );
}

extern void CommandSetDelay( char *sz ) {
#if USE_GUI
    int n;

    if( fX ) {
	if( *sz && !strncasecmp( sz, "none", strlen( sz ) ) )
	    n = 0;
	else if( ( n = ParseNumber( &sz ) ) < 0 || n > 10000 ) {
	    outputl( _("You must specify a legal move delay (see `help set "
		  "delay').") );
	    return;
	}

	if( n ) {
	    outputf(( n == 1
		      ? _("All moves will be shown for at least %d millisecond.\n")
		      : _("All moves will be shown for at least %d milliseconds.\n")),
		    n );
	    if( !fDisplay )
		outputl( _("(You will also need to use `set display' to turn "
		      "board updates on -- see `help set display'.)") );
	} else
	    outputl( _("Moves will not be delayed.") );
	
	nDelay = n;
	UpdateSetting( &nDelay );
    } else
#endif /* USE_GUI */
	outputl( _("The `set delay' command applies only when using a window "
	      "system.") );
}

extern void CommandSetDice( char *sz ) {

    int n0, n1;
    movesetdice *pmsd;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("There must be a game in progress to set the dice.") );

	return;
    }

    n0 = ParseNumber( &sz );

    if( n0 > 10 ) {
	/* assume a 2-digit number; n0 first digit, n1 second */
	n1 = n0 % 10;
	n0 /= 10;
    } else
	n1 = ParseNumber( &sz );
    
    if( n0  < 1 || n0 > 6 || n1 < 1 || n1 > 6 ) {
	outputl( _("You must specify two numbers from 1 to 6 for the dice.") );

	return;
    }

    pmsd = malloc( sizeof( *pmsd ) );
    pmsd->mt = MOVE_SETDICE;
    pmsd->sz = NULL;
    pmsd->fPlayer = ms.fMove;
    pmsd->anDice[ 0 ] = n0;
    pmsd->anDice[ 1 ] = n1;
    pmsd->lt = LUCK_NONE;
    pmsd->rLuck = ERR_VAL;
    
    AddMoveRecord( pmsd );

    outputf( _("The dice have been set to %d and %d.\n"), n0, n1 );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetDisplay( char *sz ) {

    SetToggle( "display", &fDisplay, sz, _("Will display boards for computer "
	       "moves."), _("Will not display boards for computer moves.") );
}

extern void 
CommandSetEvalCubeful( char *sz ) {

    char asz[ 2 ][ 128 ], szCommand[ 64 ];
    int f = pecSet->fCubeful;
    
    sprintf( asz[ 0 ], _("%s will use cubeful evaluation.\n"), szSet );
    sprintf( asz[ 1 ], _("%s will use cubeless evaluation.\n"), szSet );
    sprintf( szCommand, "%s cubeful", szSetCommand );
    SetToggle( szCommand, &f, sz, asz[ 0 ], asz[ 1 ] );
    pecSet->fCubeful = f;
}

extern void CommandSetEvalDeterministic( char *sz ) {

    char asz[ 2 ][ 128 ], szCommand[ 64 ];
    int f = pecSet->fDeterministic;
    
    sprintf( asz[ 0 ], _("%s will use deterministic noise.\n"), szSet );
    sprintf( asz[ 1 ], _("%s will use pseudo-random noise.\n"), szSet );
    sprintf( szCommand, "%s deterministic", szSetCommand );
    SetToggle( szCommand, &f, sz, asz[ 0 ], asz[ 1 ] );
    pecSet->fDeterministic = f;
    
    if( !pecSet->rNoise )
	outputl( _("(Note that this setting will have no effect unless you "
		 "set noise to some non-zero value.)") );
}

extern void CommandSetEvalNoise( char *sz ) {
    
    double r = ParseReal( &sz );

    if( r < 0.0 ) {
	outputf( _("You must specify a valid amount of noise to use -- "
		"try `help set\n%s noise'.\n"), szSetCommand );

	return;
    }

    pecSet->rNoise = r;

    if( pecSet->rNoise )
	outputf( _("%s will use noise with standard deviation %5.3f.\n"),
		 szSet, pecSet->rNoise );
    else
	outputf( _("%s will use noiseless evaluations.\n"), szSet );
}

extern void CommandSetEvalPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 || n > 7 ) {
	outputf( _("You must specify a valid number of plies to look ahead -- "
		"try `help set %s plies'.\n"), szSetCommand );

	return;
    }

    pecSet->nPlies = n;

    outputf( _("%s will use %d ply evaluation.\n"), szSet, pecSet->nPlies );
}

extern void CommandSetEvalReduced( char *sz ) {

    int n = ParseNumber( &sz );

    switch ( n ) {
    case 0:
    case 1:
    case 21: 
    case 100: /* full search */
      pecSet->nReduced = 0;
      break;
    case 2:
    case 50: /* 50% */
      pecSet->nReduced = 2;
      break;
    case 4:
    case 25: /* 25% */
      pecSet->nReduced = 4;
      break;
    case 3:
    case 33: /* 33% */
      pecSet->nReduced = 3;
      break;
    default:
      outputf( _("You must specify a valid number -- "
                 "try `help set %s reduced'.\n"), szSetCommand );
      return;
      break;
    }

    outputf( _("%s will use %d%% speed multiple ply evaluation.\n"), 
	     szSet, 
	     pecSet->nReduced ? 100 / pecSet->nReduced : 100 );

    if( !pecSet->nPlies )
	outputl( _("(Note that this setting will have no effect until you "
		 "choose evaluations with ply > 0.)") );
}

extern void CommandSetEvaluation( char *sz ) {

    szSet = _("`eval' and `hint'");
    szSetCommand = "evaluation";
    pecSet = &esEvalChequer.ec;
    HandleCommand( sz, acSetEvaluation );
}

#if USE_GUI
extern void CommandSetGUIAnimationBlink( char *sz ) {

    animGUI = ANIMATE_BLINK;
}

extern void CommandSetGUIAnimationNone( char *sz ) {

    animGUI = ANIMATE_NONE;
}

extern void CommandSetGUIAnimationSlide( char *sz ) {

    animGUI = ANIMATE_SLIDE;
}

extern void CommandSetGUIAnimationSpeed( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 0 || n > 7 ) {
	outputl( _("You must specify a speed between 0 and 7 -- try "
		   "`help set speed'.") );

	return;
    }

    nGUIAnimSpeed = n;

    outputf( _("Animation speed set to %d.\n"), n );
}

extern void CommandSetGUIBeep( char *sz ) {

    SetToggle( "gui beep", &fGUIBeep, sz,
	       _("GNU Backgammon will beep on illegal input."),
	       _("GNU Backgammon will not beep on illegal input.") );
}

extern void CommandSetGUIDiceArea( char *sz ) {

    if( SetToggle( "gui dicearea", &fGUIDiceArea, sz,
		   _("A dice icon will be shown below the board when a human "
		     "player is on roll."),
		   _("No dice icon will be shown.") ) >= 0 )
	UpdateSetting( &fGUIDiceArea );
}

extern void CommandSetGUIHighDieFirst( char *sz ) {

    SetToggle( "gui highdiefirst", &fGUIHighDieFirst, sz,
	       _("The higher die will be shown on the left."),
	       _("The dice will be shown in the order rolled.") );
}

extern void CommandSetGUIIllegal( char *sz ) {

    SetToggle( "gui illegal", &fGUIIllegal, sz,
	       _("Chequers may be dragged to illegal points."),
	       _("Chequers may not be dragged to illegal points.") );
}

extern void CommandSetGUIShowIDs( char *sz ) {

    if( SetToggle( "gui showids", &fGUIShowIDs, sz,
		   _("The position and match IDs will be shown above the "
		     "board."),
		   _("The position and match IDs will not be shown.") ) )
	UpdateSetting( &fGUIShowIDs );
}

extern void CommandSetGUIDragTargetHelp( char *sz ) {

    if( SetToggle( "gui dragtargethelp", &fGUIDragTargetHelp, sz,
		   _("The target help while dragging a chequer will "
		     "be shown."),
		   _("The target help while dragging a chequer will "
		     "not be shown.") ) )
	UpdateSetting( &fGUIDragTargetHelp );
}

extern void CommandSetGUIUseStatsPanel( char *sz ) {

    SetToggle( "gui usestatspanel", &fGUIUseStatsPanel, sz,
		   _("The match statistics will be shown in a panel"),
		   _("The match statistics will be shown in a list") );
}

extern void CommandSetGUIShowPips( char *sz ) {

    if( SetToggle( "gui showpips", &fGUIShowPips, sz,
		   _("The pip counts will be shown below the board."),
		   _("The pip counts will not be shown.") ) )
	UpdateSetting( &fGUIShowPips );
}

extern void CommandSetGUIWindowPositions( char *sz ) {

    SetToggle( "gui windowpositions", &fGUISetWindowPos, sz,
	       _("Saved window positions will be applied to new windows."),
	       _("Saved window positions will not be applied to new "
		 "windows.") );
}
#else
static void NoGUI( void ) {

    outputl( _("This installation of GNU Backgammon was compiled without GUI "
	       "support.") );
}

extern void CommandSetGUIAnimationBlink( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIAnimationNone( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIAnimationSlide( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIAnimationSpeed( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIBeep( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIDiceArea( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIHighDieFirst( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIIllegal( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIShowIDs( char *sz ) {

    NoGUI();
}

extern void CommandSetGUIShowPips( char *sz ) {

    NoGUI();
}
 
extern void CommandSetGUIWindowPositions( char *sz ) {

    NoGUI();
}
#endif

extern void
CommandSetPlayerMoveFilter ( char *sz ) {

  SetMoveFilter ( sz, ap[ iPlayerSet ].aamf );

}

extern void 
CommandSetPlayerChequerplay( char *sz ) {

    szSet = ap[ iPlayerSet ].szName;
    szSetCommand = "player chequerplay evaluation";
    pesSet = &ap[ iPlayerSet ].esChequer;

    outputpostpone();
    
    HandleCommand( sz, acSetEvalParam );

    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( _("(Note that this setting will have no effect until you "
		"`set player %s gnu'.)\n"), ap[ iPlayerSet ].szName );

    outputresume();
}


extern void 
CommandSetPlayerCubedecision( char *sz ) {

    szSet = ap[ iPlayerSet ].szName;
    szSetCommand = "player cubedecision evaluation";
    pesSet = &ap[ iPlayerSet ].esCube;

    outputpostpone();
    
    HandleCommand( sz, acSetEvalParam );

    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( _("(Note that this setting will have no effect until you "
		"`set player %s gnu'.)\n"), ap[ iPlayerSet ].szName );

    outputresume();
}


extern void CommandSetPlayerExternal( char *sz ) {

#if !HAVE_SOCKETS
    outputl( _("This installation of GNU Backgammon was compiled without\n"
	     "socket support, and does not implement external players.") );
#else
    int h, cb;
    struct sockaddr *psa;
    char *pch;
    
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify the name of the socket to the external\n"
		 "player -- try `help set player external'.") );
	return;
    }

    pch = strcpy( malloc( strlen( sz ) + 1 ), sz );
    
    if( ( h = ExternalSocket( &psa, &cb, sz ) ) < 0 ) {
	outputerr( pch );
	free( pch );
	return;
    }

    while( connect( h, psa, cb ) < 0 ) {
	if( errno == EINTR ) {
	    if( fAction )
		fnAction();

	    if( fInterrupt ) {
		close( h );
		free( psa );
		free( pch );
		return;
	    }
	    
	    continue;
	}
	
	outputerr( pch );
	close( h );
	free( psa );
	free( pch );
	return;
    }
    
    ap[ iPlayerSet ].pt = PLAYER_EXTERNAL;
    ap[ iPlayerSet ].h = h;
    if( ap[ iPlayerSet ].szSocket )
	free( ap[ iPlayerSet ].szSocket );
    ap[ iPlayerSet ].szSocket = pch;
    
    free( psa );
#endif /* !HAVE_SOCKETS */
}

extern void CommandSetPlayerGNU( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_GNU;

    outputf( _("Moves for %s will now be played by GNU Backgammon.\n"),
	    ap[ iPlayerSet ].szName );
    
#if USE_GTK
    if( fX )
	/* The "play" button might now be required; update the board. */
	ShowBoard();
#endif /* USE_GTK */
}

extern void CommandSetPlayerHuman( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_HUMAN;

    outputf( _("Moves for %s must now be entered manually.\n"),
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerName( char *sz ) {

    if( !sz || !*sz ) {
	outputl( _("You must specify a name to use.") );

	return;
    }

    if( strlen( sz ) > 31 )
	sz[ 31 ] = 0;

    if( ( *sz == '0' || *sz == '1' ) && !sz[ 1 ] ) {
	outputf( _("`%c' is not a valid name.\n"), *sz );

	return;
    }

    if( !strcasecmp( sz, "both" ) ) {
	outputl( _("`both' is a reserved word; you can't call a player "
		 "that.\n") );

	return;
    }

    if( !CompareNames( sz, ap[ !iPlayerSet ].szName ) ) {
	outputl( _("That name is already in use by the other player.") );

	return;
    }

    strcpy( ap[ iPlayerSet ].szName, sz );

    outputf( _("Player %d is now known as `%s'.\n"), iPlayerSet, sz );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetPlayerPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	outputl( _("You must specify a valid ply depth to look ahead -- try "
		 "`help set player plies'.") );

	return;
    }

    ap[ iPlayerSet ].esChequer.ec.nPlies = n;
    
    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( _("Moves for %s will be played with %d ply lookahead (note that "
		"this will\nhave no effect yet, because gnubg is not moving "
		"for this player).\n"), ap[ iPlayerSet ].szName, n );
    else
	outputf( _("Moves for %s will be played with %d ply lookahead.\n"),
		ap[ iPlayerSet ].szName, n );
}

extern void CommandSetPlayerPubeval( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_PUBEVAL;

    outputf( _("Moves for %s will now be played by pubeval.\n"),
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayer( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputl( _("You must specify a player -- try `help set player'.") );
	
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerSet = i;

	HandleCommand( sz, acSetPlayer );
	UpdateSetting( ap );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    outputl( _("Insufficient memory.") );
		
	    return;
	}

	strcpy( pchCopy, sz );

	outputpostpone();
	
	iPlayerSet = 0;
	HandleCommand( sz, acSetPlayer );

	iPlayerSet = 1;
	HandleCommand( pchCopy, acSetPlayer );

	outputresume();
	
	UpdateSetting( ap );
	
	free( pchCopy );
	
	return;
    }
    
    outputf( _("Unknown player `%s' -- try `help set player'.\n"), pch );
}

extern void CommandSetPrompt( char *szParam ) {

    static char sz[ 128 ]; /* FIXME check overflow */

    szPrompt = ( szParam && *szParam ) ? strcpy( sz, szParam ) :
	szDefaultPrompt;
    
    outputf( _("The prompt has been set to `%s'.\n"), szPrompt );
}

extern void CommandSetRecord( char *sz ) {

    SetToggle( "record", &fRecord, sz,
	       _("All games in a session will be recorded."),
	       _("Only the active game in a session will be recorded.") );
}

extern void CommandSetRNG ( char *sz ) {

  rngSet = &rngCurrent;
  HandleCommand ( sz, acSetRNG );

}

extern void CommandSetRNGAnsi( char *sz ) {

    SetRNG( rngSet, RNG_ANSI, sz );
}

extern void CommandSetRNGFile( char *sz ) {

    SetRNG( rngSet, RNG_FILE, sz );
}

extern void CommandSetRNGBBS( char *sz ) {
#if HAVE_LIBGMP
    SetRNG( rngSet, RNG_BBS, sz );
#else
    outputl( _("This installation of GNU Backgammon was compiled without the"
               "Blum, Blum and Shub generator.") );
#endif /* HAVE_LIBGMP */
}

extern void CommandSetRNGBsd( char *sz ) {
#if HAVE_RANDOM
    SetRNG( rngSet, RNG_BSD, sz );
#else
    outputl( _("This installation of GNU Backgammon was compiled without the"
               "BSD generator.") );
#endif /* HAVE_RANDOM */
}

extern void CommandSetRNGIsaac( char *sz ) {

    SetRNG( rngSet, RNG_ISAAC, sz );
}

extern void CommandSetRNGManual( char *sz ) {

    SetRNG ( rngSet, RNG_MANUAL, sz );
}

extern void CommandSetRNGMD5( char *sz ) {

    SetRNG ( rngSet, RNG_MD5, sz );
}

extern void CommandSetRNGMersenne( char *sz ) {

    SetRNG( rngSet, RNG_MERSENNE, sz );
}

extern void CommandSetRNGRandomDotOrg( char *sz ) {

#if HAVE_SOCKETS
    SetRNG( rngSet, RNG_RANDOM_DOT_ORG, sz );
#else
    outputl( _("This installation of GNU Backgammon was compiled without "
               "support for sockets needed for fetching\n"
               "random numbers from <www.random.org>") );
#endif /* HAVE_SOCKETS */

}

extern void CommandSetRNGUser( char *sz ) {

#if HAVE_LIBDL
    SetRNG( rngSet, RNG_USER, sz );
#else
    outputl( _("This installation of GNU Backgammon was compiled without the"
               "dynamic linking library needed for user RNG's.") );
#endif /* HAVE_LIBDL */

}

extern void CommandSetRolloutLate ( char *sz ) {

  HandleCommand ( sz, acSetRolloutLate );

}

extern void CommandSetRolloutLogEnable (char *sz) {
  int f = log_rollouts;

  SetToggle ( "rollout .sgf files", &f, sz, 
	      _("Create an .sgf file for each game rolled out"),
	      _("Do not create an .sgf file for each game rolled out") );

  log_rollouts = f;
}

extern void CommandSetRolloutLogFile (char *sz) {
   
  if (log_file_name) {
    free (log_file_name);
  }
   
  log_file_name = strdup (sz);
}

extern void
CommandSetRolloutLateEnable ( char *sz )
{
  int l = prcSet->fLateEvals;
  if( SetToggle( "separate evaluation for later plies", &l, sz,
		 _("Use different evaluation for later moves of rollout."),
		 _("Do not change evaluations during rollout.") ) != -1 ) {
    prcSet->fLateEvals = l;
  }
}

extern void CommandSetRolloutLatePlies ( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( _("You must specify a valid ply at which to change evaluations  "
        "-- try `help set rollout late plies'.") );

	return;
    }

    prcSet->nLate = n;

    if( !n )
	   outputl( _("No evaluations changes will be made during rollouts.") );
    else
      outputf( _("Evaluations will change after %d plies in rollouts.\n"), n );

}


extern void CommandSetRolloutTruncation  ( char *sz ) {

  HandleCommand ( sz, acSetTruncation );
}

extern void CommandSetRolloutLimit ( char *sz ) {

  HandleCommand ( sz, acSetRolloutLimit );

}

extern void
CommandSetRolloutLimitEnable ( char *sz ) {
  int s = prcSet->fStopOnSTD;
  
  if( SetToggle( "stop when the STD's are small enough", &s, sz,
	     _("Stop rollout when STD's are small enough"),
		 _("Do not stop rollout based on STDs")) != -1 ) {
    prcSet->fStopOnSTD = s;
  }
}

extern void CommandSetRolloutLimitMinGames ( char *sz ) {

  int n = ParseNumber( &sz );

  if (n < 1) {
    outputl( _("You must specify a valid minimum number of games to rollout"
               "-- try 'help set rollout limit minimumgames'.") );
    return;
  }

  prcSet->nMinimumGames = n;

  outputf( _("After %d games, rollouts will stop if the STDs are small enough"
	     ".\n"), n);
}

extern void CommandSetRolloutMaxError ( char *sz ) {

    double r = ParseReal( &sz );

    if( r < 0.0001 ) {
      outputl( _("You must set a valid fraction for the ratio "
		 "STD/value where rollouts can stop "
		 "-- try 'help set rollout limit maxerror'." ) );
      return;
    }

    prcSet->rStdLimit = r;

    outputf ( _("Rollouts can stop when the ratio |STD/value| is less than "
		"%5.4f for every value (win/gammon/backgammon/...equity\n"),
	      r);
}

extern void CommandSetRolloutJsd ( char *sz ) {

  HandleCommand ( sz, acSetRolloutJsd );

}

extern void
CommandSetRolloutJsdEnable ( char *sz )
{
  int s = prcSet->fStopOnJsd;
  if( SetToggle( "stop rollout when one move appears "
		 "to have a higher equity", &s, sz,
	     _("Stop rollout based on J.S.D.s"),
		 _("Do not stop rollout based on J.S.D.s")) != -1 ) {
    prcSet->fStopOnJsd = s;
  }
}

extern void CommandSetRolloutJsdMoveEnable ( char *sz ) {
  int s = prcSet->fStopMoveOnJsd;
  
  if( SetToggle( "stop rollout of moves which appear to  "
		 "to have a lowerer equity", &s, sz,
		 _("Stop rollout of moves based on J.S.D.s"),
		 _("Do not stop rollout of moves based on J.S.D.s")) != -1 ) {
    prcSet->fStopMoveOnJsd = s;
  }
}

extern void CommandSetRolloutJsdMinGames ( char *sz ) {

  int n = ParseNumber( &sz );

  if (n < 1) {
    outputl( _("You must specify a valid minimum number of games to rollout"
               "-- try 'help set rollout jsd minimumgames'.") );
    return;
  }
  prcSet->nMinimumJsdGames = n;

  outputf( _("After %d games, rollouts will stop if the J.S.D.s are large enough"
	     ".\n"), n);
}


extern void CommandSetRolloutJsdLimit ( char *sz ) {

    double r = ParseReal( &sz );

    if( r < 0.0001 ) {
      outputl( 
  _("You must set a number of joint standard deviations for the equity"
    " difference with the best move being rolled out "
   "-- try 'help set rollout jsd limit'." ) );
      return;
    }

    prcSet->rJsdLimit = r;

    outputf ( 
  _("Rollouts (or rollouts of moves) may  stop when the equity is more "
 "than %5.3f joint standard deviations from the best move being rolled out\n"),
	      r);
}

extern void CommandSetRollout ( char *sz ) {

  prcSet = &rcRollout;
  HandleCommand ( sz, acSetRollout );

}


extern void
CommandSetRolloutRNG ( char *sz ) {

  rngSet = &prcSet->rngRollout;

  HandleCommand ( sz, acSetRNG );

}
  
/* set an eval context, then copy to other player's settings */
static void
SetRolloutEvaluationContextBoth ( char *sz, evalcontext  *pec[] ) {

  assert (pec [0] != 0);
  assert (pec [1] != 0);

  pecSet = pec[ 0 ];

  HandleCommand ( sz, acSetEvaluation );

  /* copy to both players */
  /* FIXME don't copy if there was an error setting player 0 */  
  memcpy ( pec [1] , pec[ 0 ], sizeof ( evalcontext ) );  

}

extern void
CommandSetRolloutChequerplay ( char *sz ) {

  evalcontext *pec[2];

  szSet = _("Chequer play in rollouts");
  szSetCommand = "rollout chequerplay";
  
  pec[0] = prcSet->aecChequer;
  pec[1] = prcSet->aecChequer + 1;

  SetRolloutEvaluationContextBoth (sz, pec);

}

extern void
CommandSetRolloutMoveFilter ( char *sz ) {

  SetMoveFilter ( sz, prcSet->aaamfChequer[ 0 ] );
  SetMoveFilter ( sz, prcSet->aaamfChequer[ 1 ] );

}  

extern void
CommandSetRolloutLateMoveFilter ( char *sz ) {

  SetMoveFilter ( sz, prcSet->aaamfLate[ 0 ] );
  SetMoveFilter ( sz, prcSet->aaamfLate[ 1 ] );

}  

extern void
CommandSetRolloutPlayerMoveFilter ( char *sz ) {

  SetMoveFilter ( sz, prcSet->aaamfChequer[ iPlayerSet ] );

}  

extern void
CommandSetRolloutPlayerLateMoveFilter ( char *sz ) {

  SetMoveFilter ( sz, prcSet->aaamfLate[ iPlayerLateSet ] );

}  

extern void
CommandSetRolloutLateChequerplay ( char *sz ) {

  evalcontext *pec[2];

  szSet = _("Chequer play for later moves in rollouts");
  szSetCommand = "rollout late chequerplay";

  pec[0] = prcSet->aecChequerLate;
  pec[1] = prcSet->aecChequerLate + 1;

  SetRolloutEvaluationContextBoth (sz, pec);

}


static void
SetRolloutEvaluationContext ( char *sz, evalcontext *pec[], int iPlayer ) {

    assert ((iPlayer == 0) || (iPlayer == 1));
    assert (pec [ iPlayer ] != 0);

    pecSet = pec [ iPlayer ];

    HandleCommand ( sz, acSetEvaluation );
}

extern void
CommandSetRolloutPlayerChequerplay ( char *sz ) {

    evalcontext *pec[2];

    szSet = iPlayerSet ? _("Chequer play in rollouts (for player 1)") :
	_("Chequer play in rollouts (for player 0)");
    szSetCommand = iPlayerSet ? "rollout player 1 chequerplay" :
	"rollout player 0 chequerplay";
  
    pec[0] = prcSet->aecChequer;
	pec[1] = prcSet->aecChequer + 1;
	SetRolloutEvaluationContext ( sz, pec, iPlayerSet);
}

extern void
CommandSetRolloutPlayerLateChequerplay ( char *sz ) {

    evalcontext *pec[2];

    szSet = iPlayerLateSet ? 
	  _("Chequer play for later moves in rollouts (for player 1)") :
	_("Chequer play for later moves in rollouts (for player 0)");
    szSetCommand = iPlayerLateSet ? "rollout late player 1 chequerplay" :
	"rollout late player 0 chequerplay";
  
    pec[0] = prcSet->aecChequerLate;
	pec[1] = prcSet->aecChequerLate + 1;
	SetRolloutEvaluationContext ( sz, pec, iPlayerLateSet);
}


extern void
CommandSetRolloutCubedecision ( char *sz ) {

  evalcontext *pec[2];

  szSet = _("Cube decisions in rollouts");
  szSetCommand = "rollout cubedecision";

  pec[0] = prcSet->aecCube;
  pec[1] = prcSet->aecCube + 1;

  SetRolloutEvaluationContextBoth (sz, pec);

}

extern void
CommandSetRolloutLateCubedecision ( char *sz ) {

  evalcontext *pec[2];

  szSet = _("Cube decisions for later plies in rollouts");
  szSetCommand = "rollout late cubedecision";

  pec[0] = prcSet->aecCubeLate;
  pec[1] = prcSet->aecCubeLate + 1;

  SetRolloutEvaluationContextBoth (sz, pec);

}

extern void
CommandSetRolloutPlayerCubedecision ( char *sz ) {

    evalcontext *pec[2];

    szSet = iPlayerSet ? _("Cube decisions in rollouts (for player 1)") :
	_("Cube decisions in rollouts (for player 0)");
    szSetCommand = iPlayerSet ? "rollout player 1 cubedecision" :
	"rollout player 0 cubedecision";
  
    pec[0] = prcSet->aecCube;
	pec[1] = prcSet->aecCube + 1;
	SetRolloutEvaluationContext ( sz, pec, iPlayerSet);

}

extern void
CommandSetRolloutPlayerLateCubedecision ( char *sz ) {

    evalcontext *pec[2];

    szSet = iPlayerLateSet ? _("Cube decisions for later plies of rollouts (for player 1)") :
	_("Cube decisions in later plies of rollouts (for player 0)");
    szSetCommand = iPlayerLateSet ? "rollout late player 1 cubedecision" :
	"rollout late player 0 cubedecision";
  
    pec[0] = prcSet->aecCubeLate;
	pec[1] = prcSet->aecCubeLate + 1;
	SetRolloutEvaluationContext ( sz, pec, iPlayerLateSet);

}

extern void
CommandSetRolloutBearoffTruncationExact ( char *sz ) {

  int f = prcSet->fTruncBearoff2;

  SetToggle ( "rollout bearofftruncation exact", &f, sz,
              _("Will truncate *cubeless* rollouts when reaching"
                " exact bearoff database"),
              _("Will not truncate *cubeless* rollouts when reaching"
                " exact bearoff database") );

  prcSet->fTruncBearoff2 = f;

}


extern void
CommandSetRolloutBearoffTruncationOS ( char *sz ) {

  int f = prcSet->fTruncBearoffOS;

  SetToggle ( "rollout bearofftruncation onesided", &f, sz,
              _("Will truncate *cubeless* rollouts when reaching"
                " one-sided bearoff database"),
              _("Will not truncate *cubeless* rollouts when reaching"
                " one-sided bearoff database") );

  prcSet->fTruncBearoffOS = f;


}


extern void CommandSetRolloutInitial( char *sz ) {
    
    int f = prcSet->fCubeful;
    
    SetToggle( "rollout initial", &f, sz, 
               _("Rollouts will be made as the initial position of a game."),
	       _("Rollouts will be made for normal (non-opening) positions.") );

    prcSet->fInitial = f;
}

extern void CommandSetRolloutSeed( char *sz ) {

    int n;
    
    if( prcSet->rngRollout == RNG_MANUAL ) {
	outputl( _("You can't set a seed if you're using manual dice "
		 "generation.") );
	return;
    }

    if( *sz ) {
	n = ParseNumber( &sz );

	if( n < 0 ) {
	    outputl( _("You must specify a valid seed -- try `help set seed'.") );

	    return;
	}

	prcSet->nSeed = n;
	outputf( _("Rollout seed set to %d.\n"), n );
    } else
        outputl( InitRNG( &prcSet->nSeed, FALSE, prcSet->rngRollout ) ?
		 _("Rollout seed initialised from system random data.") :
		 _("Rollout seed initialised by system clock.") );    
}

extern void CommandSetRolloutTrials( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( _("You must specify a valid number of trials to make -- "
		"try `help set rollout trials'.") );

	return;
    }

    prcSet->nTrials = n;

    if ( n == 1 )
      outputf( _("%d game will be played per rollout.\n"), n );
    else
      outputf( _("%d games will be played per rollout.\n"), n );

}

extern void CommandSetRolloutTruncationEnable ( char *sz ) {
  int t = prcSet->fDoTruncate;
  
  if( SetToggle( "rollout truncation enable", &t, sz,
		 _("Games in rollouts will be stopped after"
		   " a fixed number of moves."),
		 _("Games in rollouts will be played out"
		   " until the end.")) != -1 ) {
    prcSet->fDoTruncate = t;
  }
}

extern void CommandSetRolloutCubeEqualChequer ( char *sz ) {

  SetToggle( "rollout cube-equal-chequer", &fCubeEqualChequer, sz,
 _("Rollouts use same settings for cube and chequer play."),
 _("Rollouts use separate settings for cube and chequer play.") );
}

extern void CommandSetRolloutPlayersAreSame ( char *sz ) {

  SetToggle( "rollout players-are-same", &fPlayersAreSame, sz,
 _("Rollouts use same settings for both players."),
 _("Rollouts use separate settings for both players.") );
}

extern void CommandSetRolloutTruncationEqualPlayer0 ( char *sz ) {

  SetToggle( "rollout truncate-equal-player0", &fTruncEqualPlayer0, sz,
 _("Evaluation of rollouts at truncation point will be same as player 0."),
 _("Evaluation of rollouts at truncation point are separately specified.") );
}

extern void CommandSetRolloutTruncationPlies ( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 0 ) {
       outputl( _("You must specify a valid ply at which to truncate rollouts  -- "
		"try `help set rollout'.") );

	return;
    }

    prcSet->nTruncate = n;

    if( ( n == 0 ) || !prcSet->fDoTruncate )
	outputl( _("Rollouts will not be truncated.") );
    else if ( n == 1 )
      outputf( _("Rollouts will be truncated after %d ply.\n"), n );
    else
      outputf( _("Rollouts will be truncated after %d plies.\n"), n );

}

extern void CommandSetRolloutTruncationChequer ( char *sz ) {

    szSet = _("Chequer play evaluations at rollout truncation point");
    szSetCommand = "rollout truncation chequerplay";

	pecSet = &prcSet->aecChequerTrunc;

	HandleCommand ( sz, acSetEvaluation );
}

extern void CommandSetRolloutTruncationCube ( char *sz ) {

    szSet = _("Cube decisions at rollout truncation point");
    szSetCommand = "rollout truncation cubedecision";

	pecSet = &prcSet->aecCubeTrunc;

	HandleCommand ( sz, acSetEvaluation );
}


extern void CommandSetRolloutVarRedn( char *sz ) {

    int f = prcSet->fVarRedn;
    
    SetToggle( "rollout varredn", &f, sz,
               _("Will lookahead during rollouts to reduce variance."),
               _("Will not use lookahead variance "
               "reduction during rollouts.") );

    prcSet->fVarRedn = f;
}


extern void
CommandSetRolloutRotate ( char *sz ) {

  int f = prcSet->fRotate;

  SetToggle ( "rollout quasirandom", &f, sz,
              _("Use quasi-random dice in rollouts"),
              _("Do not use quasi-random dice in rollouts") );

  prcSet->fRotate = f;

}
  

    
extern void 
CommandSetRolloutCubeful( char *sz ) {

    int f = prcSet->fCubeful;
    
    SetToggle( "rollout cubeful", &f, sz, 
               _("Cubeful rollouts will be performed."),
	       _("Cubeless rollouts will be performed.") );

    prcSet->fCubeful = f;
}


extern void
CommandSetRolloutPlayer ( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputf( _("You must specify a player -- try `help set %s player'.\n"),
		 szSetCommand );
	
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerSet = i;

	HandleCommand( sz, acSetRolloutPlayer );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    outputl( _("Insufficient memory.") );
		
	    return;
	}

	strcpy( pchCopy, sz );

	outputpostpone();
	
	iPlayerSet = 0;
	HandleCommand( sz, acSetRolloutPlayer );

	iPlayerSet = 1;
	HandleCommand( pchCopy, acSetRolloutPlayer );

	outputresume();
	
	free( pchCopy );
	
	return;
    }
    
    outputf( _("Unknown player `%s' -- try\n"
             "`help set %s player'.\n"), pch, szSetCommand );
}

extern void
CommandSetRolloutLatePlayer ( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputf( _("You must specify a player -- try `help set %s player'.\n"),
		 szSetCommand );
	
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerLateSet = i;

	HandleCommand( sz, acSetRolloutLatePlayer );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    outputl( _("Insufficient memory.") );
		
	    return;
	}

	strcpy( pchCopy, sz );

	outputpostpone();
	
	iPlayerLateSet = 0;
	HandleCommand( sz, acSetRolloutLatePlayer );

	iPlayerLateSet = 1;
	HandleCommand( pchCopy, acSetRolloutLatePlayer );

	outputresume();
	
	free( pchCopy );
	
	return;
    }
    
    outputf( _("Unknown player `%s' -- try\n"
             "`help set %s player'.\n"), pch, szSetCommand );
}

extern void CommandSetScore( char *sz ) {

    long int n0, n1;
    movegameinfo *pmgi;
    char *pch0, *pch1, *pchEnd0, *pchEnd1;
    int fCrawford0, fCrawford1, fPostCrawford0, fPostCrawford1;

    if( !( pch0 = NextToken( &sz ) ) )
	pch0 = "";
    
    if( !( pch1 = NextToken( &sz ) ) )
	pch1 = "";
    
    n0 = strtol( pch0, &pchEnd0, 10 );
    if( pch0 == pchEnd0 )
	n0 = INT_MIN;
    
    n1 = strtol( pch1, &pchEnd1, 10 );
    if( pch1 == pchEnd1 )
	n1 = INT_MIN;

    if( ( ( fCrawford0 = *pchEnd0 == '*' ||
	    ( *pchEnd0 && !strncasecmp( pchEnd0, "crawford",
					strlen( pchEnd0 ) ) ) ) &&
	  n0 != INT_MIN && n0 != -1 && n0 != ms.nMatchTo - 1 ) ||
	( ( fCrawford1 = *pchEnd1 == '*' ||
	    ( *pchEnd1 && !strncasecmp( pchEnd1, "crawford",
					strlen( pchEnd1 ) ) ) ) &&
	  n1 != INT_MIN && n1 != -1 && n1 != ms.nMatchTo - 1 ) ) {
	outputl( _("The Crawford rule applies only in match play when a "
		 "player's score is 1-away.") );
	return;
    }

    if( ( ( fPostCrawford0 = ( *pchEnd0 && !strncasecmp(
	pchEnd0,"postcrawford", strlen( pchEnd0 ) ) ) ) &&
	  n0 != INT_MIN && n0 != -1 && n0 != ms.nMatchTo - 1 ) ||
	( ( fPostCrawford1 = ( *pchEnd1 && !strncasecmp(
	pchEnd1, "postcrawford", strlen( pchEnd1 ) ) ) ) &&
	  n1 != INT_MIN && n1 != -1 && n1 != ms.nMatchTo - 1 ) ) {
	outputl( _("The Crawford rule applies only in match play when a "
		 "player's score is 1-away.") );
	return;
    }

    if( !ms.nMatchTo && ( fCrawford0 || fCrawford1 || fPostCrawford0 ||
			  fPostCrawford1 ) ) {
	outputl( _("The Crawford rule applies only in match play when a "
		 "player's score is 1-away.") );
	return;
    }
    
    if( fCrawford0 && fCrawford1 ) {
	outputl( _("You cannot set the Crawford rule when both players' scores "
		 "are 1-away.") );
	return;
    }

    if( ( fCrawford0 && fPostCrawford1 ) ||
	( fCrawford1 && fPostCrawford0 ) ) {
	outputl( _("You cannot set both Crawford and post-Crawford "
		 "simultaneously.") );
	return;
    }
    
    /* silently ignore the case where both players are set post-Crawford;
       assume that is unambiguous and means double match point */

    if( fCrawford0 || fPostCrawford0 )
	n0 = -1;
    
    if( fCrawford1 || fPostCrawford1 )
	n1 = -1;
    
    if( n0 < 0 ) /* -n means n-away */
	n0 += ms.nMatchTo;

    if( n1 < 0 )
	n1 += ms.nMatchTo;

    if( ( fPostCrawford0 && !n1 ) || ( fPostCrawford1 && !n0 ) ) {
	outputl( _("You cannot set post-Crawford play if the trailer has yet "
		 "to score.") );
	return;
    }
    
    if( n0 < 0 || n1 < 0 ) {
	outputl( _("You must specify two valid scores.") );
	return;
    }

    if( ms.nMatchTo && n0 >= ms.nMatchTo && n1 >= ms.nMatchTo ) {
	outputl( _("Only one player may win the match.") );
	return;
    }

    if( ( fCrawford0 || fCrawford1 ) &&
	( n0 >= ms.nMatchTo || n1 >= ms.nMatchTo ) ) {
	outputl( _("You cannot play the Crawford game once the match is "
		 "already over.") );
	return;
    }
    
    /* allow scores above the match length, since that doesn't really
       hurt anything */

    CancelCubeAction();
    
    ms.anScore[ 0 ] = n0;
    ms.anScore[ 1 ] = n1;

    if( ms.nMatchTo ) {
	if( n0 != ms.nMatchTo - 1 && n1 != ms.nMatchTo - 1 )
	    /* must be pre-Crawford */
	    ms.fCrawford = ms.fPostCrawford = FALSE;
	else if( ( n0 == ms.nMatchTo - 1 && n1 == ms.nMatchTo - 1 ) ||
		 fPostCrawford0 || fPostCrawford1 ) {
	    /* must be post-Crawford */
	    ms.fCrawford = FALSE;
	    ms.fPostCrawford = ms.nMatchTo > 1;
	} else {
	    /* possibly the Crawford game */
	    if( n0 >= ms.nMatchTo || n1 >= ms.nMatchTo )
		ms.fCrawford = FALSE;
	    else if( fCrawford0 || fCrawford1 || !n0 || !n1 )
		ms.fCrawford = TRUE;
	    
	    ms.fPostCrawford = !ms.fCrawford;
	}
    }
	     
    if( ms.gs < GAME_OVER && plGame && ( pmgi = plGame->plNext->p ) ) {
	assert( pmgi->mt == MOVE_GAMEINFO );
	pmgi->anScore[ 0 ] = ms.anScore[ 0 ];
	pmgi->anScore[ 1 ] = ms.anScore[ 1 ];
	pmgi->fCrawfordGame = ms.fCrawford;
#if USE_GTK
	/* The score this game was started at is displayed in the option
	   menu, and is now out of date. */
	if( fX )
	    GTKRegenerateGames();
#endif /* USE_GTK */
    }
    
    CommandShowScore( NULL );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
}

extern void CommandSetSeed( char *sz ) {

    SetSeed ( rngCurrent, sz );

}

extern void CommandSetTrainingAlpha( char *sz ) {

    float r = ParseReal( &sz );

    if( r <= 0.0f || r > 1.0f ) {
	outputl( _("You must specify a value for alpha which is greater than\n"
		 "zero, and no more than one.") );
	return;
    }

    rAlpha = r;
    outputf( _("Alpha set to %f.\n"), r );
}

extern void CommandSetTrainingAnneal( char *sz ) {

    double r = ParseReal( &sz );

    if( r == ERR_VAL ) {
	outputl( _("You must specify a valid annealing rate.") );
	return;
    }

    rAnneal = r;
    outputf( _("Annealing rate set to %f.\n"), r );
}

extern void CommandSetTrainingThreshold( char *sz ) {

    float r = ParseReal( &sz );

    if( r < 0.0f ) {
	outputl( _("You must specify a valid error threshold.") );
	return;
    }

    rThreshold = r;

    if( rThreshold )
	outputf( _("Error threshold set to %f.\n"), r );
    else
	outputl( _("Error threshold disabled.") );
}

extern void CommandSetTurn( char *sz ) {

    char *pch = NextToken( &sz );
    int i;

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("There must be a game in progress to set a player on roll.") );

	return;
    }
    
    if( ms.fResigned ) {
	outputl( _("Please resolve the resignation first.") );

	return;
    }
    
    if( !pch ) {
	outputl( _("Which player do you want to set on roll?") );

	return;
    }

    if( ( i = ParsePlayer( pch ) ) < 0 ) {
	outputf( _("Unknown player `%s' -- try `help set turn'.\n"), pch );

	return;
    }

    if( i == 2 ) {
	outputl( _("You can't set both players on roll.") );

	return;
    }

    if( ms.fTurn != i )
	SwapSides( ms.anBoard );
    
    ms.fTurn = ms.fMove = i;
    CancelCubeAction();
    fNextTurn = FALSE;
    ms.anDice[ 0 ] = ms.anDice[ 1 ] = 0;

#if USE_TIMECONTROL
    CheckGameClock(&ms, 0);
    HitGameClock(&ms);
#endif

    UpdateSetting( &ms.fTurn );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif /* USE_GUI */
    
    outputf( _("`%s' is now on roll.\n"), ap[ i ].szName );
}

extern void CommandSetEgyptian( char *sz ) {

    if( SetToggle( "egyptian", &fEgyptian, sz,
                   _("Will use the Egyptian rule."),
                   _("Will not use the Egyptian rule.") ) ) {
        outputl( _("Note: most likely the database and weights are not "
                 "tuned for Egyptian play.\nRegenerating them with "
                 "the rule set may give better results.") );
        return;
    } ;
}

extern void CommandSetJacoby( char *sz ) {

    if( SetToggle( "jacoby", &fJacoby, sz, 
		   _("Will use the Jacoby rule for money sessions."),
		   _("Will not use the Jacoby rule for money sessions.") ) )
	return;

    if( fJacoby && !ms.fCubeUse )
	outputl( _("(Note that you'll have to enable the cube if you want "
		 "gammons and backgammons\nto be scored -- see `help set "
		 "cube use'.)") );

    ms.fJacoby = fJacoby;

}

extern void CommandSetCrawford( char *sz ) {

  movegameinfo *pmgi;
    
  if ( ms.nMatchTo > 0 ) {
    if ( ( ms.nMatchTo - ms.anScore[ 0 ] == 1 ) || 
	 ( ms.nMatchTo - ms.anScore[ 1 ] == 1 ) ) {

      if( SetToggle( "crawford", &ms.fCrawford, sz, 
		 _("This game is the Crawford game (no doubling allowed)."),
		 _("This game is not the Crawford game.") ) < 0 )
	  return;

      /* sanity check */
      ms.fPostCrawford = !ms.fCrawford;

      if( ms.fCrawford )
	  CancelCubeAction();
      
      if( plGame && ( pmgi = plGame->plNext->p ) ) {
	  assert( pmgi->mt == MOVE_GAMEINFO );
	  pmgi->fCrawfordGame = ms.fCrawford;
      }
    } else {
      outputl( _("Cannot set whether this is the Crawford game\n"
	    "as none of the players are 1-away from winning.") );
    }
  }
  else if ( !ms.nMatchTo ) 
      outputl( _("Cannot set Crawford play for money sessions.") );
  else
      outputl( _("No match in progress (type `new match n' to start one).") );
}

extern void CommandSetPostCrawford( char *sz ) {

  movegameinfo *pmgi;
  
  if ( ms.nMatchTo > 0 ) {
    if ( ( ms.nMatchTo - ms.anScore[ 0 ] == 1 ) || 
	 ( ms.nMatchTo - ms.anScore[ 1 ] == 1 ) ) {

      SetToggle( "postcrawford", &ms.fPostCrawford, sz, 
		 _("This is post-Crawford play (doubling allowed)."),
		 _("This is not post-Crawford play.") );

      /* sanity check */
      ms.fCrawford = !ms.fPostCrawford;

      if( ms.fCrawford )
	  CancelCubeAction();
      
      if( plGame && ( pmgi = plGame->plNext->p ) ) {
	  assert( pmgi->mt == MOVE_GAMEINFO );
	  pmgi->fCrawfordGame = ms.fCrawford;
      }
    } else {
      outputl( _("Cannot set whether this is post-Crawford play\n"
	    "as none of the players are 1-away from winning.") );
    }
  }
  else if ( !ms.nMatchTo ) 
      outputl( _("Cannot set post-Crawford play for money sessions.") );
  else
      outputl( _("No match in progress (type `new match n' to start one).") );

}

#if USE_GTK
warnings ParseWarning(char* str)
{
	int i;

	while(*str == ' ')
		str++;

	for (i = 0; i < WARN_NUM_WARNINGS; i++)
	{
		if (!strcasecmp(str, warningNames[i]))
			return i;
	}

	return -1;
}

extern void CommandSetWarning( char *sz )
{
	char buf[100];
	warnings warning;
	char* pValue = strchr(sz, ' ');

	if (!pValue)
	{
		outputl( _("Incorrect syntax for set warning command.") );
		return;
	}
	*pValue++ = '\0';

	warning = ParseWarning(sz);
	if ((int)warning < 0)
	{
		sprintf(buf, _("Unknown warning %s."), sz);
		outputl(buf);
		return;
	}

	while(*pValue == ' ')
		pValue++;

	if (!strcasecmp(pValue, "on"))
	{
		warningEnabled[warning] = TRUE;
	}
	else if (!strcasecmp(pValue, "off"))
	{
		warningEnabled[warning] = FALSE;
	}
	else
	{
		sprintf(buf, _("Unknown value %s."), pValue);
		outputl(buf);
		return;
	}
	sprintf(buf, _("Warning %s set %s."), sz, pValue);
	outputl(buf);
}

static void PrintWarning(int warning)
{
	char buf[100];
	sprintf(buf, _("Warning %s (%s) is %s"), warningNames[warning], warningStrings[warning],
		warningEnabled[warning] ? "on" : "off");
	outputl(buf);
}

extern void CommandShowWarning( char *sz )
{
	warnings warning;

	while(*sz == ' ')
		sz++;

	if (!*sz)
	{	/* Show all warnings */
		for (warning = 0; warning < WARN_NUM_WARNINGS; warning++)
			PrintWarning(warning);
	}
	else
	{	/* Show specific warning */
		warning = ParseWarning(sz);
		if ((int)warning < 0)
		{
			char buf[100];
			sprintf(buf, _("Unknown warning %s."), sz);
			outputl(buf);
			return;
		}
		PrintWarning(warning);
	}
}
#endif

extern void CommandSetBeavers( char *sz ) {

    int n;

    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	outputl( _("You must specify the number of beavers to allow.") );

	return;
    }

    nBeavers = n;

    if( nBeavers > 1 )
	outputf( _("%d beavers/raccoons allowed in money sessions.\n"), nBeavers );
    else if( nBeavers == 1 )
	outputl( _("1 beaver allowed in money sessions.") );
    else
	outputl( _("No beavers allowed in money sessions.") );
}

extern void
CommandSetOutputDigits( char *sz ) {

  int n = ParseNumber( &sz );

  if ( n < 0 || n > 6 ) {
    outputl( _("You must specify a number between 1 and 6.\n") );
    return;
  }

  fOutputDigits = n;

  outputf( _("Probabilities and equities will be shown with %d digits "
             "after the decimal separator\n"), fOutputDigits );

}
      

extern void CommandSetOutputMatchPC( char *sz ) {

    SetToggle( "output matchpc", &fOutputMatchPC, sz,
	       _("Match winning chances will be shown as percentages."),
	       _("Match winning chances will be shown as probabilities.") );
}

extern void CommandSetOutputMWC( char *sz ) {

    SetToggle( "output mwc", &fOutputMWC, sz,
	       _("Match evaluations will be shown as match winning chances."),
	       _("Match evaluations will be shown as equivalent money equity.") );
}

extern void CommandSetOutputRawboard( char *sz ) {

    SetToggle( "output rawboard", &fOutputRawboard, sz,
	       _("TTY boards will be given in raw format."),
	       _("TTY boards will be given in ASCII.") );
}

extern void CommandSetOutputWinPC( char *sz ) {

    SetToggle( "output winpc", &fOutputWinPC, sz,
	       _("Game winning chances will be shown as percentages."),
	       _("Game winning chances will be shown as probabilities.") );
}

extern void CommandSetMET( char *sz ) {

  sz = NextToken ( &sz );

  if ( !sz || !*sz ) {
    outputl ( _("You must specify a filename. "
              "See \"help set met\". ") );
    return;
  }

  InitMatchEquity ( sz, szDataDirectory );
  setDefaultPath ( sz, PATH_MET );

  /* Cubeful evaluation get confused withh entries from another table */
  EvalCacheFlush();
  
  outputf( _("GNU Backgammon will now use the %s match equity table.\n"),
           miCurrent.szName );

  if ( miCurrent.nLength < MAXSCORE ) {
    
    outputf (_("\n"
             "Note that this match equity table only supports "
             "matches of length %i and below.\n"
             "For scores above %i-away an extrapolation "
             "scheme is used.\n"),
             miCurrent.nLength, miCurrent.nLength );

  }

}



extern void
CommandSetEvalParamType ( char *sz ) {

  switch ( sz[ 0 ] ) {
    
  case 'r':
    pesSet->et = EVAL_ROLLOUT;
    break;

  case 'e':
    pesSet->et = EVAL_EVAL;
    break;

  default:
    outputf (_("Unknown evaluation type: %s -- see\n"
             "`help set %s type'\n"), sz, szSetCommand );
    return;
    break;

  }

  outputf ( _("%s will now use %s.\n"),
            szSet, gettext ( aszEvalType[ pesSet->et ] ) );

}


extern void
CommandSetEvalParamEvaluation ( char *sz ) {

  pecSet = &pesSet->ec;

  HandleCommand ( sz, acSetEvaluation );

  if ( pesSet->et != EVAL_EVAL )
    outputf ( _("(Note that this setting will have no effect until you\n"
              "`set %s type evaluation'.)\n"),
              szSetCommand );
}


extern void
CommandSetEvalParamRollout ( char *sz ) {

  prcSet = &pesSet->rc;

  HandleCommand (sz, acSetRollout );

  if ( pesSet->et != EVAL_ROLLOUT )
    outputf ( _("(Note that this setting will have no effect until you\n"
              "`set %s type rollout.)'\n"),
              szSetCommand );

}


extern void
CommandSetAnalysisPlayer( char *sz ) {

  char *pch = NextToken( &sz ), *pchCopy;
  int i;

  if( !pch ) {
    outputl( _("You must specify a player "
               "-- try `help set analysis player'.") );
    return;
  }

  if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
    iPlayerSet = i;

    HandleCommand( sz, acSetAnalysisPlayer );
	
    return;
  }

  if( i == 2 ) {
    if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
      outputl( _("Insufficient memory.") );
      
      return;
	}
    
    strcpy( pchCopy, sz );
    
    outputpostpone();
    
    iPlayerSet = 0;
    HandleCommand( sz, acSetAnalysisPlayer );
    
    iPlayerSet = 1;
    HandleCommand( pchCopy, acSetAnalysisPlayer );
    
    outputresume();
	
    free( pchCopy );
    
    return;
  }
  
  outputf( _("Unknown player `%s' -- try\n"
             "`help set analysis player'.\n"), pch );

}


extern void
CommandSetAnalysisPlayerAnalyse( char *sz ) {

  char sz1[ 100 ];
  char sz2[ 100 ];

  sprintf( sz1, _("Analyse %s's chequerplay and cube decisions."),
           ap[ iPlayerSet ].szName );

  sprintf( sz2, _("Do not analyse %s's chequerplay and cube decisions."),
           ap[ iPlayerSet ].szName );

  SetToggle( "analysis player", &afAnalysePlayers[ iPlayerSet ], sz,
             sz1, sz2 );

}


extern void
CommandSetAnalysisChequerplay ( char *sz ) {

  pesSet = &esAnalysisChequer;

  szSet = _("Analysis chequerplay");
  szSetCommand = "analysis chequerplay";

  HandleCommand( sz, acSetEvalParam );

}

extern void
CommandSetAnalysisCubedecision ( char *sz ) {


  pesSet = &esAnalysisCube;

  szSet = _("Analysis cubedecision");
  szSetCommand = "analysis cubedecision";

  HandleCommand( sz, acSetEvalParam );

}


extern void
CommandSetEvalChequerplay ( char *sz ) {

  pesSet = &esEvalChequer;

  szSet = _("`eval' and `hint' chequerplay");
  szSetCommand = "evaluation chequerplay ";

  HandleCommand( sz, acSetEvalParam );

}

extern void
CommandSetEvalCubedecision ( char *sz ) {


  pesSet = &esEvalCube;

  szSet = _("`eval' and `hint' cube decisions");
  szSetCommand = "evaluation cubedecision ";

  HandleCommand( sz, acSetEvalParam );

}


extern void CommandSetEvalMoveFilter( char *sz ) {

  SetMoveFilter ( sz, aamfEval );

}

extern void CommandSetAnalysisMoveFilter( char *sz ) {

  SetMoveFilter ( sz, aamfAnalysis );

}


  

extern void 
SetMatchInfo( char **ppch, char *sz, char *szMessage ) {

    if( *ppch )
	free( *ppch );

    if( sz && *sz ) {
	*ppch = strdup( sz );
        if ( szMessage )
   	   outputf( _("%s set to: %s\n"), szMessage, sz );
    } else {
	*ppch = NULL;
        if ( szMessage )
	   outputf( _("%s cleared.\n"), szMessage );
    }
}

extern void CommandSetMatchAnnotator( char *sz ) {

    SetMatchInfo( &mi.pchAnnotator, sz, _("Match annotator") );
}

extern void CommandSetMatchComment( char *sz ) {
    
    SetMatchInfo( &mi.pchComment, sz, _("Match comment") );
}

static int DaysInMonth( int nYear, int nMonth ) {

    static int an[ 12 ] = { 31, -1, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    if( nMonth < 1 || nMonth > 12 )
	return -1;
    else if( nMonth != 2 )
	return an[ nMonth - 1 ];
    else if( nYear % 4 || ( !( nYear % 100 ) && nYear % 400 ) )
	return 28;
    else
	return 29;
}
 
extern void CommandSetMatchDate( char *sz ) {

    int nYear, nMonth, nDay;

    if( !sz || !*sz ) {
	mi.nYear = 0;
	outputl( _("Match date cleared." ) );
	return;
    }
    
    if( sscanf( sz, "%d-%d-%d", &nYear, &nMonth, &nDay ) < 3 ||
	nYear < 1753 || nYear > 9999 || nMonth < 1 || nMonth > 12 ||
	nDay < 1 || nDay > DaysInMonth( nYear, nMonth ) ) {
	outputf( _("%s is not a valid date (see `help set matchinfo "
		   "date').\n"), sz );
	return;
    }

    mi.nYear = nYear;
    mi.nMonth = nMonth;
    mi.nDay = nDay;

    outputf( _("Match date set to %04d-%02d-%02d.\n"), nYear, nMonth, nDay );
}

extern void CommandSetMatchEvent( char *sz ) {

    SetMatchInfo( &mi.pchEvent, sz, _("Match event") );
}

extern void CommandSetMatchLength( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( _("You must specify a valid match length (1 or longer).") );

	return;
    }

    nDefaultLength = n;

    outputf( n == 1 ? _("New matches will default to %d point.\n") :
	     _("New matches will default to %d points.\n"), n );
}

extern void CommandSetMatchPlace( char *sz ) {

    SetMatchInfo( &mi.pchPlace, sz, _("Match place") );
}

extern void CommandSetMatchRating( char *sz ) {

    int n;
    char szMessage[ 64 ];
    
    if( ( n = ParsePlayer( NextToken( &sz ) ) ) < 0 ) {
	outputl( _("You must specify which player's rating to set (see `help "
		   "set matchinfo rating').") );
	return;
    }

    sprintf( szMessage, _("Rating for %s"), ap[ n ].szName );
    
    SetMatchInfo( &mi.pchRating[ n ], sz, szMessage );
}

extern void CommandSetMatchRound( char *sz ) {

    SetMatchInfo( &mi.pchRound, sz, _("Match round") );
}

extern void
CommandSetMatchID ( char *sz ) {

  SetMatchID ( sz );

}


extern void
CommandSetExportIncludeAnnotations ( char *sz ) {

  SetToggle( "annotations", &exsExport.fIncludeAnnotation, sz,
             _("Include annotations in exports"),
             _("Do not include annotations in exports") );

}

extern void
CommandSetExportIncludeAnalysis ( char *sz ) {

  SetToggle( "analysis", &exsExport.fIncludeAnalysis, sz,
             _("Include analysis in exports"),
             _("Do not include analysis in exports") );

}

extern void
CommandSetExportIncludeStatistics ( char *sz ) {

  SetToggle( "statistics", &exsExport.fIncludeStatistics, sz,
             _("Include statistics in exports"),
             _("Do not include statistics in exports") );

}

extern void
CommandSetExportIncludeLegend ( char *sz ) {

  SetToggle( "annotations", &exsExport.fIncludeLegend, sz,
             _("Include legend in exports"),
             _("Do not include legend in exports") );

}

extern void
CommandSetExportIncludeMatchInfo ( char *sz ) {

  SetToggle( "matchinfo", &exsExport.fIncludeMatchInfo, sz,
             _("Include match information in exports"),
             _("Do not include match information in exports") );

}

extern void
CommandSetExportShowBoard ( char *sz ) {

  int n;

  if( ( n = ParseNumber( &sz ) ) < 0 ) {
    outputl( _("You must specify a semi-positive number.") );

    return;
  }

  exsExport.fDisplayBoard = n;

  if ( ! n )
    output ( _("The board will never been shown in exports.") );
  else
    outputf ( _("The board will be shown every %d. move in exports."), n );

}


extern void
CommandSetExportShowPlayer ( char *sz ) {

  int i;

  if( ( i = ParsePlayer( sz ) ) < 0 ) {
    outputf( _("Unknown player `%s' "
             "-- try `help set export show player'.\n"), sz );
    return;
  }

  exsExport.fSide = i + 1;

  if ( i == 2 )
    outputl ( _("Analysis, boards etc will be "
              "shown for both players in exports.") );
  else
    outputf ( _("Analysis, boards etc will only be shown for "
              "player %s in exports.\n"), ap[ i ].szName );

}


extern void
CommandSetExportMovesNumber ( char *sz ) {

  int n;

  if( ( n = ParseNumber( &sz ) ) < 0 ) {
    outputl( _("You must specify a semi-positive number.") );

    return;
  }

  exsExport.nMoves = n;

  outputf ( _("Show at most %d moves in exports.\n"), n );

}

extern void
CommandSetExportMovesProb ( char *sz ) {

  SetToggle( "probabilities", &exsExport.fMovesDetailProb, sz,
             _("Show detailed probabilities for moves"),
             _("Do not show detailed probabilities for moves") );

}

static int *pParameter;
static char *szParameter;

extern void
CommandSetExportMovesParameters ( char *sz ) {

  pParameter = exsExport.afMovesParameters;
  szParameter = "moves";
  HandleCommand ( sz, acSetExportParameters );

}

extern void
CommandSetExportCubeProb ( char *sz ) {

  SetToggle( "probabilities", &exsExport.fCubeDetailProb, sz,
             _("Show detailed probabilities for cube decisions"),
             _("Do not show detailed probabilities for cube decisions") );

}

extern void
CommandSetExportCubeParameters ( char *sz ) {


  pParameter = exsExport.afCubeParameters;
  szParameter = "cube";
  HandleCommand ( sz, acSetExportParameters );

}


extern void
CommandSetExportParametersEvaluation ( char *sz ) {

  SetToggle( "evaluation", &pParameter[ 0 ], sz,
             _("Show detailed parameters for evaluations"),
             _("Do not show detailed parameters for evaluations") );

}

extern void
CommandSetExportParametersRollout ( char *sz ) {

  SetToggle( "rollout", &pParameter[ 1 ], sz,
             _("Show detailed parameters for rollouts"),
             _("Do not show detailed parameters for rollouts") );

}


extern void
CommandSetExportMovesDisplayVeryBad ( char *sz ) {
  
  SetToggle( "export moves display very bad", 
             &exsExport.afMovesDisplay[ SKILL_VERYBAD ], sz,
             _("Export moves marked 'very bad'."),
             _("Do not export moves marked 'very bad'.") );

}
    
extern void
CommandSetExportMovesDisplayBad ( char *sz ) {
  
  SetToggle( "export moves display bad", 
             &exsExport.afMovesDisplay[ SKILL_BAD ], sz,
             _("Export moves marked 'bad'."),
             _("Do not export moves marked 'bad'.") );

}
    
extern void
CommandSetExportMovesDisplayDoubtful ( char *sz ) {
  
  SetToggle( "export moves display doubtful", 
             &exsExport.afMovesDisplay[ SKILL_DOUBTFUL ], sz,
             _("Export moves marked 'doubtful'."),
             _("Do not export moves marked 'doubtful'.") );

}
    
extern void
CommandSetExportMovesDisplayUnmarked ( char *sz ) {
  
  SetToggle( "export moves display unmarked", 
             &exsExport.afMovesDisplay[ SKILL_NONE ], sz,
             _("Export unmarked moves."),
             _("Do not export unmarked.") );

}
    
/* extern void */
/* CommandSetExportMovesDisplayInteresting ( char *sz ) { */

/*   SetToggle( "export moves display interesting",  */
/*              &exsExport.afMovesDisplay[ SKILL_INTERESTING ], sz, */
/*              _("Export moves marked 'interesting'."), */
/*              _("Do not export moves marked 'interesting'.") ); */
  
/* } */
    
extern void
CommandSetExportMovesDisplayGood ( char *sz ) {
  
  SetToggle( "export moves display good",
             &exsExport.afMovesDisplay[ SKILL_GOOD ], sz,
             _("Export moves marked 'good'."),
             _("Do not export moves marked 'good'.") );
  
}
    
/* extern void */
/* CommandSetExportMovesDisplayVeryGood ( char *sz ) { */
  
/*   SetToggle( "export moves display very good",  */
/*              &exsExport.afMovesDisplay[ SKILL_VERYGOOD ], sz, */
/*              _("Export moves marked 'very good'."), */
/*              _("Do not export moves marked 'very good'.") ); */
  
/* } */
    

extern void
CommandSetExportCubeDisplayVeryBad ( char *sz ) {
  
  SetToggle( "export cube display very bad", 
             &exsExport.afCubeDisplay[ SKILL_VERYBAD ], sz,
             _("Export cube decisions marked 'very bad'."),
             _("Do not export cube decisions marked 'very bad'.") );

}
    
extern void
CommandSetExportCubeDisplayBad ( char *sz ) {
  
  SetToggle( "export cube display bad", 
             &exsExport.afCubeDisplay[ SKILL_BAD ], sz,
             _("Export cube decisions marked 'bad'."),
             _("Do not export cube decisions marked 'bad'.") );

}
    
extern void
CommandSetExportCubeDisplayDoubtful ( char *sz ) {
  
  SetToggle( "export cube display doubtful", 
             &exsExport.afCubeDisplay[ SKILL_DOUBTFUL ], sz,
             _("Export cube decisions marked 'doubtful'."),
             _("Do not export cube decisions marked 'doubtful'.") );

}
    
extern void
CommandSetExportCubeDisplayUnmarked ( char *sz ) {
  
  SetToggle( "export cube display unmarked", 
             &exsExport.afCubeDisplay[ SKILL_NONE ], sz,
             _("Export unmarked cube decisions."),
             _("Do not export unmarked cube decisions.") );

}
    
/* extern void */
/* CommandSetExportCubeDisplayInteresting ( char *sz ) { */
  
/*   SetToggle( "export cube display interesting",  */
/*              &exsExport.afCubeDisplay[ SKILL_INTERESTING ], sz, */
/*              _("Export cube decisions marked 'interesting'."), */
/*              _("Do not export cube decisions marked 'interesting'.") ); */

/* } */
    
/* extern void */
/* CommandSetExportCubeDisplayGood ( char *sz ) { */
  
/*   SetToggle( "export cube display good",  */
/*              &exsExport.afCubeDisplay[ SKILL_GOOD ], sz, */
/*              _("Export cube decisions marked 'good'."), */
/*              _("Do not export cube decisions marked 'good'.") ); */

/* } */
    
/* extern void */
/* CommandSetExportCubeDisplayVeryGood ( char *sz ) { */
  
/*   SetToggle( "export cube display very good",  */
/*              &exsExport.afCubeDisplay[ SKILL_VERYGOOD ], sz, */
/*              _("Export cube decisions marked 'very good'."), */
/*              _("Do not export cube decisions marked 'very good'.") ); */

/* } */
    
extern void
CommandSetExportCubeDisplayActual ( char *sz ) {
  
  SetToggle( "export cube display actual", 
             &exsExport.afCubeDisplay[ EXPORT_CUBE_ACTUAL ], sz,
             _("Export actual cube decisions."),
             _("Do not export actual cube decisions.") );

}
    
extern void
CommandSetExportCubeDisplayClose ( char *sz ) {
  
  SetToggle( "export cube display close", 
             &exsExport.afCubeDisplay[ EXPORT_CUBE_CLOSE ], sz,
             _("Export close cube decisions."),
             _("Do not export close cube decisions.") );

}
    
extern void
CommandSetExportCubeDisplayMissed ( char *sz ) {
  
  SetToggle( "export cube display missed", 
             &exsExport.afCubeDisplay[ EXPORT_CUBE_MISSED ], sz,
             _("Export missed cube decisions."),
             _("Do not export missed cube decisions.") );

}

static void
SetExportHTMLType ( const htmlexporttype het,
                    const char *szExtension ) {

  if ( exsExport.szHTMLExtension )
    free ( exsExport.szHTMLExtension );

  exsExport.het = het;
  exsExport.szHTMLExtension = strdup ( szExtension );

  outputf ( _("HTML export type is now: \n"
              "%s\n"), 
            aszHTMLExportType[ exsExport.het ] );

}

extern void 
CommandSetExportHTMLTypeBBS ( char *sz ) {

  SetExportHTMLType ( HTML_EXPORT_TYPE_BBS, "gif" );

}

extern void 
CommandSetExportHTMLTypeFibs2html ( char *sz ) {

  SetExportHTMLType ( HTML_EXPORT_TYPE_FIBS2HTML, "gif" );

}

extern void 
CommandSetExportHTMLTypeGNU ( char *sz ) {

  SetExportHTMLType ( HTML_EXPORT_TYPE_GNU, "png" );

}


static void
SetExportHTMLCSS ( const htmlexportcss hecss ) {

  if ( exsExport.hecss == hecss )
    return;

  if ( exsExport.hecss == HTML_EXPORT_CSS_EXTERNAL )
    CommandNotImplemented ( NULL );

  exsExport.hecss = hecss;

  outputf ( _("CSS stylesheet for HTML export: %s\n"),
            gettext ( aszHTMLExportCSS[ hecss ] ) );

}


extern void
CommandSetExportHTMLCSSHead ( char *sz ) {

  SetExportHTMLCSS ( HTML_EXPORT_CSS_HEAD );

}

extern void
CommandSetExportHTMLCSSInline ( char *sz ) {

  SetExportHTMLCSS ( HTML_EXPORT_CSS_INLINE );

}

extern void
CommandSetExportHTMLCSSExternal ( char *sz ) {

  SetExportHTMLCSS ( HTML_EXPORT_CSS_EXTERNAL );

}


extern void 
CommandSetExportHTMLPictureURL ( char *sz ) {

  if ( ! sz || ! *sz ) {
    outputl ( _("You must specify a URL. "
              "See `help set export html pictureurl'.") );
    return;
  }

  if ( exsExport.szHTMLPictureURL )
    free ( exsExport.szHTMLPictureURL );

  sz = NextToken ( &sz );
  exsExport.szHTMLPictureURL = strdup ( sz );

  outputf ( _("URL for picture in HTML export is now: \n"
            "%s\n"), 
            exsExport.szHTMLPictureURL );

}

    
     
extern void
CommandSetInvertMatchEquityTable ( char *sz ) {

  int fOldInvertMET = fInvertMET;

  if( SetToggle( "invert matchequitytable", &fInvertMET, sz,
                 _("Match equity table will be used inverted."),
                 _("Match equity table will not be use inverted.") ) >= 0 )
    UpdateSetting( &fInvertMET );

  if ( fOldInvertMET != fInvertMET )
    invertMET ();


}


static void
SetPath ( char *sz, pathformat f ) {

  sz = NextToken ( &sz );

  if ( ! sz || ! *sz ) {
    outputl ( _("You must specify a path.") );
    return;
  }

  strcpy ( aaszPaths[ f ][ 0 ], sz );

}


extern void
CommandSetPathSnowieTxt ( char *sz ) {

  SetPath ( sz, PATH_SNOWIE_TXT );

}


extern void
CommandSetPathGam ( char *sz ) {

  SetPath ( sz, PATH_GAM );

}


extern void
CommandSetPathHTML ( char *sz ) {

  SetPath ( sz, PATH_HTML );

}


extern void
CommandSetPathLaTeX ( char *sz ) {

  SetPath ( sz, PATH_LATEX );

}


extern void
CommandSetPathMat ( char *sz ) {

  SetPath ( sz, PATH_MAT );

}


extern void
CommandSetPathOldMoves ( char *sz ) {

  SetPath ( sz, PATH_OLDMOVES );

}


extern void
CommandSetPathPDF ( char *sz ) {

  SetPath ( sz, PATH_PDF );

}


extern void
CommandSetPathPos ( char *sz ) {

  SetPath ( sz, PATH_POS );

}


extern void
CommandSetPathEPS ( char *sz ) {

  SetPath ( sz, PATH_EPS );

}


extern void
CommandSetPathPostScript ( char *sz ) {

  SetPath ( sz, PATH_POSTSCRIPT );

}


extern void
CommandSetPathSGF ( char *sz ) {

  SetPath ( sz, PATH_SGF );

}


extern void
CommandSetPathSGG ( char *sz ) {

  SetPath ( sz, PATH_SGG );

}


extern void
CommandSetPathMET ( char *sz ) {

  SetPath ( sz, PATH_MET );

}

extern void
CommandSetPathTMG ( char *sz ) {

  SetPath ( sz, PATH_TMG );

}

extern void
CommandSetPathText ( char *sz ) {

  SetPath ( sz, PATH_TEXT );

}

extern void
CommandSetPathBKG ( char *sz ) {

  SetPath ( sz, PATH_BKG );

}


extern void CommandSetTutorMode( char *sz) {

    SetToggle( "tutor-mode", &fTutor, sz, 
               _("Warn about possibly bad play."),
			   _("No warnings for possibly bad play.") );
}

extern void CommandSetTutorCube( char * sz) {

  SetToggle ( "tutor-cube", &fTutorCube, sz,
			  _("Include advice on cube decisions in tutor mode."),
			  _("Exclude advice on cube decisions from tutor mode.") );
}

extern void CommandSetTutorChequer( char * sz) {

  SetToggle ( "tutor-chequer", &fTutorChequer, sz,
			  _("Include advice on chequer play in tutor mode."),
			  _("Exclude advice on chequer play from tutor mode.") );
}

extern void CommandSetTutorEval( char * sz) {

  SetToggle ( "tutor-eval", &fTutorAnalysis, sz,
			  _("Use Analysis settings to generate advice."),
			  _("Use Evaluation settings to generate advice.") );
}

static void _set_tutor_skill (skilltype Skill, int skillno, char *skill) {

  nTutorSkillCurrent = skillno;
  TutorSkill = Skill;
  outputf ( _("Tutor warnings will be give for play marked `%s'.\n"), skill);
}

extern void CommandSetTutorSkillDoubtful( char * sz) {

  _set_tutor_skill (SKILL_DOUBTFUL, 0, _("doubtful") );
}

extern void CommandSetTutorSkillBad( char * sz) {

  _set_tutor_skill (SKILL_BAD, 1, _("bad") );
}

extern void CommandSetTutorSkillVeryBad( char * sz) {

  _set_tutor_skill (SKILL_VERYBAD, 2, _("very bad") );
}


static gnubgwindow gwSet;

extern void
CommandSetGeometryAnnotation ( char *sz ) {

  gwSet = WINDOW_ANNOTATION;
  szSet = "annotation";
  szSetCommand = "annotation";

  HandleCommand ( sz, acSetGeometryValues );

}


extern void
CommandSetGeometryHint ( char *sz ) {

  gwSet = WINDOW_HINT;
  szSet = "hint";
  szSetCommand = "hint";

  HandleCommand ( sz, acSetGeometryValues );

}


extern void
CommandSetGeometryGame ( char *sz ) {

  gwSet = WINDOW_GAME;
  szSet = "game-list";
  szSetCommand = "game";

  HandleCommand ( sz, acSetGeometryValues );

}


extern void
CommandSetGeometryMain ( char *sz ) {

  gwSet = WINDOW_MAIN;
  szSet = "main";
  szSetCommand = "main";

  HandleCommand ( sz, acSetGeometryValues );

}


extern void
CommandSetGeometryMessage ( char *sz ) {

  gwSet = WINDOW_MESSAGE;
  szSet = "message";
  szSetCommand = "message";

  HandleCommand ( sz, acSetGeometryValues );

}


extern void
CommandSetGeometryWidth ( char *sz ) {

  int n;

  if ( ( n = ParseNumber( &sz ) ) == INT_MIN )
    outputf ( "Illegal value. "
              "See 'help set geometry %s width'.\n", szSetCommand );
  else {

    awg[ gwSet ].nWidth = n;
    outputf ( "Width of %s window set to %d.\n", szSet, n );

#if USE_GTK
    if ( fX )
      UpdateGeometry ( gwSet );
#endif /* USE_GTK */

  }

}

extern void
CommandSetGeometryHeight ( char *sz ) {

  int n;

  if ( ( n = ParseNumber( &sz ) ) == INT_MIN )
    outputf ( "Illegal value. "
              "See 'help set geometry %s height'.\n", szSetCommand );
  else {

    awg[ gwSet ].nHeight = n;
    outputf ( "Height of %s window set to %d.\n", szSet, n );

#if USE_GTK
    if ( fX )
      UpdateGeometry ( gwSet );
#endif /* USE_GTK */

  }

}

extern void
CommandSetGeometryPosX ( char *sz ) {

  int n;

  if ( ( n = ParseNumber( &sz ) ) == INT_MIN )
    outputf ( "Illegal value. "
              "See 'help set geometry %s xpos'.\n", szSetCommand );
  else {

    awg[ gwSet ].nPosX = n;
    outputf ( "X-position of %s window set to %d.\n", szSet, n );

#if USE_GTK
    if ( fX )
      UpdateGeometry ( gwSet );
#endif /* USE_GTK */

  }

}

extern void
CommandSetGeometryPosY ( char *sz ) {

  int n;

  if ( ( n = ParseNumber( &sz ) ) == INT_MIN )
    outputf ( "Illegal value. "
              "See 'help set geometry %s ypos'.\n", szSetCommand );
  else {

    awg[ gwSet ].nPosY = n;
    outputf ( "Y-position of %s window set to %d.\n", szSet, n );

#if USE_GTK
    if ( fX )
      UpdateGeometry ( gwSet );
#endif /* USE_GTK */

  }

}

int *Highlightrgb = HighlightColourTable[13].rgbs[0];

extern void
CommandSetHighlightColour ( char *sz ) {

    char *pch = NextToken( &sz );
    int i, j, n;

    if( !pch ) {
	outputf( _("You must specify a colour "
			   "-- try `help set %s'.\n"), szSetCommand );
	
	return;
    }

  if( (i = ParseHighlightColour( pch )) < 0 ) {
    outputf (_("Unknown colour '%s' -- try `help set %s colour'.\n"), 
			 sz, szSetCommand );
	return;
  }

  HighlightColour = &HighlightColourTable[i];
  Highlightrgb = &HighlightColourTable[i].rgbs[HighlightIntensity][0];
  if (i == 0) {
	/* custom rgb */
	for (j = 0; j < 3; ++j) {
	  if (( n = ParseNumber( &sz ) ) == INT_MIN )
		return;

	  Highlightrgb[j] = n;
	}
  }
}

extern void
CommandSetHighlight ( char *sz ) {

  HandleCommand ( sz, acSetHighlightIntensity );

}


extern void
CommandSetHighlightLight ( char *sz ) {

    szSetCommand = "highlightcolour light";

    HighlightIntensity = 0;
    CommandSetHighlightColour ( sz );
}

extern void
CommandSetHighlightMedium ( char *sz ) {

    szSetCommand = "highlightcolour medium";
    
    HighlightIntensity = 1;
    CommandSetHighlightColour ( sz );
}

extern void
CommandSetHighlightDark ( char *sz ) {

    szSetCommand = "highlightcolour dark";
    
    HighlightIntensity = 2;
    CommandSetHighlightColour ( sz );
}


/*
 * Sounds
 */

#if USE_SOUND

/* enable/disable sounds */

extern void
CommandSetSoundEnable ( char *sz ) {

  SetToggle( "sound enable", &fSound, sz, 
             _("Enable sounds."),
             _("Disable sounds.") );

}

/* sound system */

extern void
CommandSetSoundSystemArtsc ( char *sz ) {

#if HAVE_ARTSC

  ssSoundSystem = SOUND_SYSTEM_ARTSC;
  outputl ( _("GNU Backgammon will use the ArtsC sound system" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "the ArtsC sound system" ) );

#endif /* HAVE_ARTSC */

}

extern void
CommandSetSoundSystemCommand ( char *sz ) {

  if ( ! sz || ! *sz ) {
    outputl ( _("You must specify a command. "
                "See `help set sound system command'") );
    return;
  }

  strncpy ( szSoundCommand, sz, sizeof ( szSoundCommand ) - 1 );
  szSoundCommand[ sizeof ( szSoundCommand ) - 1 ] = 0;

  ssSoundSystem = SOUND_SYSTEM_COMMAND;
  outputf ( _("GNU Backgammon will use an external command to play sounds:\n"
              "%s\n"), sz );

}

extern void
CommandSetSoundSystemESD ( char *sz ) {

#if HAVE_ESD

  ssSoundSystem = SOUND_SYSTEM_ESD;
  outputl ( _("GNU Backgammon will use the ESD sound system" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "the ESD sound system" ) );

#endif /* HAVE_ESD */

}

extern void
CommandSetSoundSystemNAS ( char *sz ) {

#if HAVE_NAS

  ssSoundSystem = SOUND_SYSTEM_NAS;
  outputl ( _("GNU Backgammon will use the NAS sound system" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "the NAS sound system" ) );

#endif /* HAVE_NAS */

}

extern void
CommandSetSoundSystemNormal ( char *sz ) {

#ifndef WIN32

  ssSoundSystem = SOUND_SYSTEM_NORMAL;
  outputl ( _("GNU Backgammon will play sounds to /dev/dsp" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "playing sounds to /dev/dsp" ) );

#endif /* !WIN32 */

}

extern void
CommandSetSoundSystemWindows ( char *sz ) {

#ifdef WIN32

  ssSoundSystem = SOUND_SYSTEM_WINDOWS;
  outputl ( _("GNU Backgammon will use the MS Windows sound system" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "the MS Windows sound system" ) );

#endif /* WIN32 */

}

extern void
CommandSetSoundSystemQuickTime ( char *sz ) {

#ifdef __APPLE__

  ssSoundSystem = SOUND_SYSTEM_QUICKTIME;
  outputl ( _("GNU Backgammon will use the Apple QuickTime sound system" ) );

#else

  outputl ( _("GNU Backgammon was compiled without support for "
              "the Apple QuickTime sound system" ) );

#endif /* __APPLE__ */

}

static void
SetSound ( const gnubgsound gs, const char *szFilename ) {

  SoundFlushCache( gs );
    
  if ( ! szFilename || ! *szFilename ) {

    strcpy ( aszSound[ gs ], "" );
    outputf ( _("No sound played for: %s\n"), 
              gettext ( aszSoundDesc[ gs ] ) );

  }
  else {

    strncpy ( aszSound[ gs ], szFilename, sizeof ( aszSound[ gs ] ) - 1 );
    aszSound[ gs ][ sizeof ( aszSound[ gs ] ) - 1 ] = 0;
    outputf ( _("Sound for: %s: %s\n"), 
              gettext ( aszSoundDesc[ gs ] ),
              aszSound[ gs ] );
  }

}


extern void
CommandSetSoundSoundAgree ( char *sz ) {

  SetSound ( SOUND_AGREE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundAnalysisFinished ( char *sz ) {

  SetSound ( SOUND_ANALYSIS_FINISHED, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundBotDance ( char *sz ) {

  SetSound ( SOUND_BOT_DANCE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundBotWinGame ( char *sz ) {

  SetSound ( SOUND_BOT_WIN_GAME, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundBotWinMatch ( char *sz ) {

  SetSound ( SOUND_BOT_WIN_MATCH, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundDouble ( char *sz ) {

  SetSound ( SOUND_DOUBLE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundDrop ( char *sz ) {

  SetSound ( SOUND_DROP, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundExit ( char *sz ) {

  SetSound ( SOUND_EXIT, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundHumanDance ( char *sz ) {

  SetSound ( SOUND_HUMAN_DANCE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundHumanWinGame ( char *sz ) {

  SetSound ( SOUND_HUMAN_WIN_GAME, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundHumanWinMatch ( char *sz ) {

  SetSound ( SOUND_HUMAN_WIN_MATCH, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundMove ( char *sz ) {

  SetSound ( SOUND_MOVE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundRedouble ( char *sz ) {

  SetSound ( SOUND_REDOUBLE, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundResign ( char *sz ) {

  SetSound ( SOUND_RESIGN, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundRoll ( char *sz ) {

  SetSound ( SOUND_ROLL, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundStart ( char *sz ) {

  SetSound ( SOUND_START, NextToken ( &sz ) );

}

extern void
CommandSetSoundSoundTake ( char *sz ) {

  SetSound ( SOUND_TAKE, NextToken ( &sz ) );

}

#endif /* USE_SOUND */

static void SetPriority( int n ) {

#if HAVE_SETPRIORITY
    if( setpriority( PRIO_PROCESS, getpid(), n ) )
	outputerr( "setpriority" );
    else {
	outputf( _("Scheduling priority set to %d.\n"), n );
	nThreadPriority = n;
    }
#elif WIN32
    int tp;
    char *pch;
       
    if( n < -19 ) {
	tp = THREAD_PRIORITY_TIME_CRITICAL;
	pch = N_("time critical");
    } else if( n < -10 ) {
	tp = THREAD_PRIORITY_HIGHEST;
	pch = N_("highest");
    } else if( n < 0 ) {
	tp = THREAD_PRIORITY_ABOVE_NORMAL;
	pch = N_("above normal");
    } else if( !n ) {
	tp = THREAD_PRIORITY_NORMAL;
	pch = N_("normal");
    } else if( n < 19 ) {
	tp = THREAD_PRIORITY_BELOW_NORMAL;
	pch = N_("below normal");
    } else {
	tp = THREAD_PRIORITY_IDLE;
	pch = N_("idle");
    }

    if ( SetThreadPriority(GetCurrentThread(), tp ) ) {
	outputf ( _("Priority of program set to: %s\n"), pch );
	nThreadPriority = n;
    } else
	outputerrf( _("Changing priority failed (trying to set priority "
		      "%s)\n"), pch );
#else
    outputerrf( _("Priority changes are not supported on this platform.\n") );
#endif /* HAVE_SETPRIORITY */
}

extern void CommandSetPriorityAboveNormal ( char *sz ) {

    SetPriority( -10 );
}

extern void CommandSetPriorityBelowNormal ( char *sz ) {

    SetPriority( 10 );
}

extern void CommandSetPriorityHighest ( char *sz ) {

    SetPriority( -19 );
}

extern void CommandSetPriorityIdle ( char *sz ) {

    SetPriority( 19 );
}

extern void CommandSetPriorityNice ( char *sz ) {

    int n;
    
    if( ( n = ParseNumber( &sz ) ) < -20 || n > 20 ) {
	outputl( _("You must specify a priority between -20 and 20.") );
	return;
    }

    SetPriority( n );
}

extern void CommandSetPriorityNormal ( char *sz ) {

    SetPriority( 0 );
}

extern void CommandSetPriorityTimeCritical ( char *sz ) {

    SetPriority( -20 );
}


extern void
CommandSetCheatEnable ( char *sz ) {

  SetToggle( "cheat enable", &fCheat, sz, 
             _("Allow GNU Backgammon to manipulate the dice."),
             _("Disallow GNU Backgammon to manipulate the dice.") );

}


extern void
CommandSetCheatPlayer( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputl( _("You must specify a player "
                   "-- try `help set cheat player'.") );
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerSet = i;

	HandleCommand( sz, acSetCheatPlayer );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    outputl( _("Insufficient memory.") );
		
	    return;
	}

	strcpy( pchCopy, sz );

	outputpostpone();
	
	iPlayerSet = 0;
	HandleCommand( sz, acSetCheatPlayer );

	iPlayerSet = 1;
	HandleCommand( pchCopy, acSetCheatPlayer );

	outputresume();
	
	free( pchCopy );
	
	return;
    }
    
    outputf( _("Unknown player `%s' -- try\n"
             "`help set %s player'.\n"), pch, szSetCommand );

}

extern void
PrintCheatRoll( const int fPlayer, const int n ) {

  static char *aszNumber[ 21 ] = {
    N_("best"), N_("second best"), N_("third best"), 
    N_("4th best"), N_("5th best"), N_("6th best"),
    N_("7th best"), N_("8th best"), N_("9th best"),
    N_("10th best"),
    N_("median"),
    N_("10th worst"),
    N_("9th worst"), N_("8th worst"), N_("7th worst"),
    N_("6th worst"), N_("5th worst"), N_("4th worst"),
    N_("third worst"), N_("second worst"), N_("worst")
  };
    
  outputf( _("%s will get the %s roll on each turn.\n"), 
           ap[ fPlayer ].szName, gettext ( aszNumber[ n ] ) );

}

extern void
CommandSetCheatPlayerRoll( char *sz ) {

  int n;
  if( ( n = ParseNumber( &sz ) ) < 1 || n > 21 ) {
    outputl( _("You must specify a size between 1 and 21.") );
    return;
  }
  
  afCheatRoll[ iPlayerSet ] = n - 1;

  PrintCheatRoll( iPlayerSet, afCheatRoll[ iPlayerSet ] );

}



extern void
CommandSetExportPNGSize ( char *sz ) {

    int n;
    
    if( ( n = ParseNumber( &sz ) ) < 1 || n > 20 ) {
	outputl( _("You must specify a size between 1 and 20.") );
	return;
    }

    exsExport.nPNGSize = n;

    outputf ( _("Size of generated PNG images are %dx%d pixels\n"),
              n * BOARD_WIDTH, n * BOARD_HEIGHT );


}

static void
SetVariation( const bgvariation bgvx ) {

  bgvDefault = bgvx;
  CommandShowVariation( NULL );

#if USE_GUI
    if( fX && ms.gs == GAME_NONE )
	ShowBoard();
#endif /* USE_GUI */

}

extern void
CommandSetVariation1ChequerHypergammon( char *sz ) {

  SetVariation( VARIATION_HYPERGAMMON_1 );

}

extern void
CommandSetVariation2ChequerHypergammon( char *sz ) {

  SetVariation( VARIATION_HYPERGAMMON_2 );

}

extern void
CommandSetVariation3ChequerHypergammon( char *sz ) {

  SetVariation( VARIATION_HYPERGAMMON_3 );

}

extern void
CommandSetVariationNackgammon( char *sz ) {

  SetVariation( VARIATION_NACKGAMMON );

}

extern void
CommandSetVariationStandard( char *sz ) {

  SetVariation( VARIATION_STANDARD );

}


extern void
CommandSetGotoFirstGame( char *sz ) {

  SetToggle( "gotofirstgame", &fGotoFirstGame, sz, 
             _("Goto first game when loading matches or sessions."),
             _("Goto last game when loading matches or sessions.") );

}


static void
SetEfficiency( const char *szText, char *sz, float *prX ) {

  float r = ParseReal( &sz );

  if ( r >= 0.0f && r <= 1.0f ) {
    *prX = r;
    outputf( "%s: %7.5f\n", szText, *prX );
  }
  else
    outputl( _("Cube efficiency must be between 0 and 1") );

}

extern void
CommandSetCubeEfficiencyOS( char *sz ) {

  SetEfficiency( _("Cube efficiency for one sided bearoff positions"), 
                 sz, &rOSCubeX );

}

extern void
CommandSetCubeEfficiencyCrashed( char *sz ) {

  SetEfficiency( _("Cube efficiency for crashed positions"), sz, &rCrashedX );

}

extern void
CommandSetCubeEfficiencyContact( char *sz ) {

  SetEfficiency( _("Cube efficiency for contact positions"), sz, &rContactX );

}

extern void
CommandSetCubeEfficiencyRaceFactor( char *sz ) {

  float r = ParseReal( &sz );

  if ( r >= 0 ) {
    rRaceFactorX = r;
    outputf( _("Cube efficiency race factor set to %7.5f\n"), rRaceFactorX );
  }
  else
    outputl( _("Cube efficiency race factor must be larger than 0.") );

}

extern void
CommandSetCubeEfficiencyRaceMax( char *sz ) {

  SetEfficiency( _("Cube efficiency race max"), sz, &rRaceMax );

}

extern void
CommandSetCubeEfficiencyRaceMin( char *sz ) {

  SetEfficiency( _("Cube efficiency race min"), sz, &rRaceMin );

}

extern void
CommandSetCubeEfficiencyRaceCoefficient( char *sz ) {

  float r = ParseReal( &sz );

  if ( r >= 0 ) {
    rRaceCoefficientX = r;
    outputf( _("Cube efficiency race coefficienct set to %7.5f\n"), 
             rRaceCoefficientX );
  }
  else
    outputl( _("Cube efficiency race coefficient must be larger than 0.") );

}


extern void
CommandSetBearoffSconyers15x15DVDEnable( char *sz ) {

  SetToggle( "bearoff sconyers 15x15 dvd enable", 
             &fSconyers15x15DVD, sz, 
             _("Enable use of Hugh Sconyers' full bearoff "
               "databases (browse only)"),
             _("Disable use of Hugh Sconyers' full bearoff "
               "databases (browse only)") );

}

extern void
CommandSetBearoffSconyers15x15DVDPath( char *sz ) {

  sz = NextToken( &sz );

  if ( ! sz || ! *sz ) {
    outputl ( _("You must specify a path.") );
    return;
  }

  strcpy( szPathSconyers15x15DVD, sz );

  outputf( _("Path to Hugh Sconyers' full bearoff databases on DVD is: %s\n"),
           szPathSconyers15x15DVD );

  /* update path in bearoffcontext */

  if ( pbc15x15_dvd ) {
    if ( pbc15x15_dvd->szDir )
      free( pbc15x15_dvd->szDir );
    pbc15x15_dvd->szDir = strdup( sz );
  }
  
}

extern void
CommandSetBearoffSconyers15x15DiskEnable( char *sz ) {

  CommandNotImplemented ( NULL );
  SetToggle( "bearoff sconyers 15x15 disk enable", 
             &fSconyers15x15Disk, sz, 
             _("Enable use of Hugh Sconyers' full bearoff "
               "databases for analysis and evaluations"),
             _("Disable use of Hugh Sconyers' full bearoff "
               "databases for analysis and evaluations") );

}

extern void
CommandSetBearoffSconyers15x15DiskPath( char *sz ) {

  CommandNotImplemented ( NULL );

  sz = NextToken( &sz );

  if ( ! sz || ! *sz ) {
    outputl ( _("You must specify a path.") );
    return;
  }

  strcpy( szPathSconyers15x15Disk, sz );

  outputf( _("Path to Hugh Sconyers' full bearoff databases on disk is: %s\n"),
           szPathSconyers15x15Disk );
  
}


extern void
CommandSetRatingOffset( char *sz ) {

  float r = ParseReal( &sz );

  if ( r < 0 ) {
    outputl( _("Please provide a positive rating offset\n" ) );
    return;
  }

  rRatingOffset = r;

  outputf( _("The rating offset for estimating absolute ratings is: %.1f\n"),
           rRatingOffset );

}

extern void CommandSetLang( char *sz ) {

    if( !sz || !*sz ) {
	outputl( _( "You must give `system' or a language code "
		    "as an argument." ) );
	return;
    }

    if( strlen( sz ) > 31 )
	sz[ 31 ] = 0;

    if( ! strcmp( sz, szLang ) ) {
	outputf( _("The current language preference is already set to "
		   "%s.\n"), sz );
	return;
    }

    strcpy( szLang, sz );

    outputf( _( "The language preference has been set to `%s'.\n"
		"Please remember to save settings.\n"
		"The new setting will only take effect on the next start "
		"of gnubg.\n" ), sz );

}

extern void CommandSetDisplayPanels( char *sz ) {

  SetToggle ("panels", &fDisplayPanels, sz, 
  _("Game list, Annotation and Message panels/windows will be displayed."),
  _("Game list, Annotation and Message panels/windows will not be displayed.")
	     );

#if USE_GUI && USE_GTK
  if (fX) {
    if (fDisplayPanels) 
      ShowAllPanels (0, 0, 0);
    else
      HideAllPanels (0, 0, 0);
  }
#endif
    
}


extern void
CommandSetOutputErrorRateFactor( char *sz ) {

  float r = ParseReal( &sz );

  if ( r < 0 ) {
    outputl( _("Please provide a positive number\n" ) );
    return;
  }

  rErrorRateFactor = r;

  outputf( _("The factor used for multiplying error rates is: %.1f\n"),
           rErrorRateFactor );



}
