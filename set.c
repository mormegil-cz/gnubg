/*
 * set.c
 *
 * by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001.
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

#include <ctype.h>
#include <errno.h>
#include <math.h>
#if HAVE_SYS_SOCKET_H
#include <sys/types.h>

#if defined(sun)
#define AF_LOCAL AF_UNIX
#define PF_LOCAL PF_UNIX
#endif

#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkprefs.h"
#endif
#include "matchequity.h"
#include "positionid.h"
#include "drawboard.h"

#ifndef HUGE_VALF
#define HUGE_VALF (-1e38)
#endif

#if defined(AF_UNIX) && !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#define PF_LOCAL PF_UNIX
#endif
#ifndef SUN_LEN
#define SUN_LEN(x) (sizeof *(x))
#endif

static int iPlayerSet;

static char szEQUITY[] = "<equity>",
    szNAME[] = "<name>",
    szNUMBER[] = "<number>",
    szONOFF[] = "on|off",
    szPLIES[] = "<plies>",
    szSTDDEV[] = "<std dev>";
    
command acSetEvaluation[] = {
    { "candidates", CommandSetEvalCandidates, "Limit the number of moves "
      "for deep evaluation", szNUMBER, NULL },
    { "cubeful", CommandSetEvalCubeful, "Cubeful evaluations", szONOFF, NULL },
    { "deterministic", CommandSetEvalDeterministic, "Specify whether added "
      "noise is determined by position", szONOFF, NULL },
    { "noise", CommandSetEvalNoise, "Distort evaluations with noise",
      szSTDDEV, NULL },
    { "plies", CommandSetEvalPlies, "Choose how many plies to look ahead",
      szPLIES, NULL },
    { "reduced", CommandSetEvalReduced,
      "Control how thoroughly deep plies are searched", szNUMBER, NULL },
    { "tolerance", CommandSetEvalTolerance, "Control the equity range "
      "of moves for deep evaluation", szEQUITY, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetPlayer[] = {
    { "chequerplay", CommandSetPlayerChequerplay, "Control chequerplay "
      "parameters when gnubg plays", NULL, acSetEvalParam },
    { "cubedecision", CommandSetPlayerCubedecision, "Control cube decision "
      "parameters when gnubg plays", NULL, acSetEvalParam },
    { "external", CommandSetPlayerExternal, "Have another process make all "
      "moves for a player", NULL, NULL },
    { "gnubg", CommandSetPlayerGNU, "Have gnubg make all moves for a player",
      NULL, NULL },
    { "human", CommandSetPlayerHuman, "Have a human make all moves for a "
      "player", NULL, NULL },
    { "name", CommandSetPlayerName, "Change a player's name", szNAME, NULL },
    { "pubeval", CommandSetPlayerPubeval, "Have pubeval make all moves for a "
      "player", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
};

static void SetRNG( rng rngNew, char *szSeed ) {

    static char *aszRNG[] = {
	"ANSI", "BSD", "ISAAC", "manual", "MD5", "Mersenne Twister",
	"user supplied"
    };
    
    /* FIXME: read name of file with user RNG */

    if( rngCurrent == rngNew ) {
	outputf( "You are already using the %s generator.\n",
		aszRNG[ rngNew ] );
	if( szSeed )
	    CommandSetSeed( szSeed );
    } else {
	outputf( "GNU Backgammon will now use the %s generator.\n",
		aszRNG[ rngNew ] );

        /* Dispose dynamically linked user module if necesary */

        if ( rngCurrent == RNG_USER )
#if HAVE_LIBDL
	  UserRNGClose();
#else
	  abort();
#endif
	  
	/* Load dynamic library with user RNG */

        if ( rngNew == RNG_USER ) {
#if HAVE_LIBDL
	  if ( !UserRNGOpen() ) {
	      outputl( "Error loading shared library." );
	      outputf( "You are still using the %s generator.\n",
		     aszRNG[ rngCurrent ] );
            return;
	  }
#else
	  abort();
#endif
	}
	    
	if( ( rngCurrent = rngNew ) != RNG_MANUAL )
	    CommandSetSeed( szSeed );
	
	UpdateSetting( &rngCurrent );
    }
}

extern void CommandSetAnalysisCube( char *sz ) {

    if( SetToggle( "analysis cube", &fAnalyseCube, sz,
		   "Cube action will be analysed.",
		   "Cube action will not be analysed." ) >= 0 )
	UpdateSetting( &fAnalyseCube );
}

extern void CommandSetAnalysisLimit( char *sz ) {
    
    int n;
    
    if( ( n = ParseNumber( &sz ) ) <= 0 ) {
	cAnalysisMoves = -1;
	outputl( "Every legal move will be analysed." );
    } else if( n >= 2 ) {
	cAnalysisMoves = n;
	outputf( "Up to %d moves will be analysed.\n", n );
    } else {
	outputl( "If you specify a limit on the number of moves to analyse, "
		 "it must be at least 2." );
	return;
    }

    if( !fAnalyseMove )
	outputl( "(Note that no moves will be analysed until enable chequer "
		 "play analysis -- see\n`help set analysis moves'.)" );
}

extern void CommandSetAnalysisLuck( char *sz ) {

    if( SetToggle( "analysis luck", &fAnalyseDice, sz,
		   "Dice rolls will be analysed.",
		   "Dice rolls will not be analysed." ) >= 0 )
	UpdateSetting( &fAnalyseDice );
}

extern void CommandSetAnalysisMoves( char *sz ) {

    if( SetToggle( "analysis moves", &fAnalyseMove, sz,
		   "Chequer play will be analysed.",
		   "Chequer play will not be analysed." ) >= 0 )
	UpdateSetting( &fAnalyseMove );
}

static void SetLuckThreshold( char *szCommand, lucktype lt, char *sz ) {

    double r = ParseReal( &sz );

    if( r <= 0.0 ) {
	outputf( "You must specify a positive number for the threshold (see "
		 "`help set analysis\nthreshold %s').\n", szCommand );
	return;
    }

    arLuckLevel[ lt ] = r;

    outputf( "`%s' threshold set to %.3f.\n", szCommand, r );
}

static void SetSkillThreshold( char *szCommand, skilltype lt, char *sz ) {

    double r = ParseReal( &sz );

    if( r <= 0.0 ) {
	outputf( "You must specify a positive number for the threshold (see "
		 "`help set analysis\nthreshold %s').\n", szCommand );
	return;
    }

    arSkillLevel[ lt ] = r;

    outputf( "`%s' threshold set to %.3f.\n", szCommand, r );
}

extern void CommandSetAnalysisThresholdBad( char *sz ) {

    SetSkillThreshold( "bad", SKILL_BAD, sz );
}

extern void CommandSetAnalysisThresholdGood( char *sz ) {

    SetSkillThreshold( "good", SKILL_GOOD, sz );
}

extern void CommandSetAnalysisThresholdLucky( char *sz ) {

    SetLuckThreshold( "lucky", LUCK_GOOD, sz );
}

extern void CommandSetAnalysisThresholdUnlucky( char *sz ) {

    SetLuckThreshold( "unlucky", LUCK_BAD, sz );
}

extern void CommandSetAnalysisThresholdVeryBad( char *sz ) {

    SetSkillThreshold( "very bad", SKILL_VERYBAD, sz );
}

extern void CommandSetAnalysisThresholdVeryGood( char *sz ) {

    SetSkillThreshold( "very good", SKILL_VERYGOOD, sz );
}

extern void CommandSetAnalysisThresholdVeryLucky( char *sz ) {

    SetLuckThreshold( "very lucky", LUCK_VERYGOOD, sz );
}

extern void CommandSetAnalysisThresholdVeryUnlucky( char *sz ) {

    SetLuckThreshold( "very unlucky", LUCK_VERYBAD, sz );
}

extern void CommandSetAnnotation( char *sz ) {

    if( SetToggle( "annotation", &fAnnotation, sz,
		   "Move analysis and commentary will be displayed.",
		   "Move analysis and commentary will not be displayed." )
	>= 0 )
	/* Force an update, even if the setting has not changed. */
	UpdateSetting( &fAnnotation );
}

extern void CommandSetAutoBearoff( char *sz ) {

    SetToggle( "automatic bearoff", &fAutoBearoff, sz, "Will automatically "
	       "bear off as many chequers as possible.", "Will not "
	       "automatically bear off chequers." );
}

extern void CommandSetAutoCrawford( char *sz ) {

    SetToggle( "automatic crawford", &fAutoCrawford, sz, "Will enable the "
	       "Crawford game according to match score.", "Will not "
	       "enable the Crawford game according to match score." );
}

extern void CommandSetAutoDoubles( char *sz ) {

    int n;
    
    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	outputl( "You must specify how many automatic doubles to use "
	      "(try `help set automatic double')." );
	return;
    }

    if( n > 12 ) {
	outputl( "Please specify a smaller limit (up to 12 automatic "
	      "doubles." );
	return;
    }
	
    if( ( cAutoDoubles = n ) > 1 )
	outputf( "Automatic doubles will be used (up to a limit of %d).\n", n );
    else if( cAutoDoubles )
	outputl( "A single automatic double will be permitted." );
    else
	outputl( "Automatic doubles will not be used." );

    UpdateSetting( &cAutoDoubles );
    
    if( cAutoDoubles ) {
	if( ms.nMatchTo > 0 )
	    outputl( "(Note that automatic doubles will have no effect until you "
		  "start session play.)" );
	else if( !fCubeUse )
	    outputl( "(Note that automatic doubles will have no effect until you "
		  "enable cube use --\nsee `help set cube use'.)" );
    }
}

extern void CommandSetAutoGame( char *sz ) {

    SetToggle( "automatic game", &fAutoGame, sz, "Will automatically start "
	       "games after wins.", "Will not automatically start games." );
}

extern void CommandSetAutoMove( char *sz ) {

    SetToggle( "automatic move", &fAutoMove, sz, "Forced moves will be made "
	       "automatically.", "Forced moves will not be made "
	       "automatically." );
}

extern void CommandSetAutoRoll( char *sz ) {

    SetToggle( "automatic roll", &fAutoRoll, sz, "Will automatically roll the "
	       "dice when no cube action is possible.", "Will not "
	       "automatically roll the dice." );
}

extern void CommandSetBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    movesetboard *pmsb;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "There must be a game in progress to set the board." );

	return;
    }

    if( !*sz ) {
	outputl( "You must specify a position -- see `help set board'." );

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
	outputl( "There must be a game in progress to set the cube." );
	return -1;
    }

    if( ms.fCrawford ) {
	outputl( "The cube is disabled during the Crawford game." );
	return -1;
    }
    
    if( !fCubeUse ) {
	outputl( "The cube is disabled (see `help set cube use')." );
	return -1;
    }

    return 0;
}

extern void CommandSetCache( char *sz ) {

    int n;

    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	outputl( "You must specify the number of cache entries to use." );

	return;
    }

    if( EvalCacheResize( n ) )
	perror( "EvalCacheResize" );
    else
	outputf( "The position cache has been sized to %d entr%s.\n", n,
		n == 1 ? "y" : "ies" );
}

extern void CommandSetClockwise( char *sz ) {

    SetToggle( "clockwise", &fClockwise, sz, "Player 1 moves clockwise (and "
	"player 0 moves anticlockwise).", "Player 1 moves anticlockwise (and "
	"player 0 moves clockwise)." );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetColours( char *sz ) {
    
#if USE_GTK
    if( fX ) {
	char *apch[ 2 ];

	BoardPreferencesStart( pwBoard );
	    
	while( ParseKeyValue( &sz, apch ) )
	    BoardPreferencesParam( pwBoard, apch[ 0 ], apch[ 1 ] );

	BoardPreferencesDone( pwBoard );	    
    } else
#endif
	outputl( "The colours may not be changed when using this user "
		 "interface." );
}

extern void CommandSetConfirm( char *sz ) {
    
    SetToggle( "confirm", &fConfirm, sz, "Will ask for confirmation before "
	       "aborting games in progress.", "Will not ask for confirmation "
	       "before aborting games in progress." );
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
    
    outputl( "The cube has been centred (either player may double)." );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetCubeOwner( char *sz ) {

    movesetcubepos *pmscp;
    
    int i;
    
    if( CheckCubeAllowed() )
	return;

    switch( i = ParsePlayer( sz ) ) {
    case 0:
    case 1:
	break;
	
    case 2:
	/* "set cube owner both" is the same as "set cube centre" */
	CommandSetCubeCentre( NULL );
	return;

    default:
	outputl( "You must specify which player owns the cube (see `help set "
	      "cube owner')." );
	return;
    }

    pmscp = malloc( sizeof( *pmscp ) );
    pmscp->mt = MOVE_SETCUBEPOS;
    pmscp->sz = NULL;
    pmscp->fCubeOwner = i;
    
    AddMoveRecord( pmscp );
    
    outputf( "%s now owns the cube.\n", ap[ ms.fCubeOwner ].szName );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetCubeUse( char *sz ) {

    if( SetToggle( "cube use", &fCubeUse, sz, "Use of the doubling cube is "
		   "permitted.",
		   "Use of the doubling cube is disabled." ) < 0 )
	return;

    if( !ms.nMatchTo && fJacoby && !fCubeUse )
	outputl( "(Note that you'll have to disable the Jacoby rule if you want "
	      "gammons and\nbackgammons to be scored -- see `help set "
	      "jacoby')." );
    
    if( ms.fCrawford && fCubeUse )
	outputl( "(But the Crawford rule is in effect, so you won't be able to "
	      "use it during\nthis game.)" );
    else if( ms.gs == GAME_PLAYING && !fCubeUse ) {
	/* The cube was being used and now it isn't; reset it to 1,
	   centred. */
	ms.nCube = 1;
	ms.fCubeOwner = -1;
	UpdateSetting( &ms.nCube );
	UpdateSetting( &ms.fCubeOwner );
	
#if USE_GUI
	if( fX )
	    ShowBoard();
#endif

	CancelCubeAction();
    }
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
	    
	    outputf( "The cube has been set to %d.\n", n );
	    
#if USE_GUI
	    if( fX )
		ShowBoard();
#endif
	    return;
	}

    outputl( "You must specify a legal cube value (see `help set cube "
	     "value')." );
}

extern void CommandSetDelay( char *sz ) {
#if USE_GUI
    int n;

    if( fX ) {
	if( *sz && !strncasecmp( sz, "none", strlen( sz ) ) )
	    n = 0;
	else if( ( n = ParseNumber( &sz ) ) < 0 || n > 10000 ) {
	    outputl( "You must specify a legal move delay (see `help set "
		  "delay')." );
	    return;
	}

	if( n ) {
	    outputf( "All moves will be shown for at least %d millisecond%s.\n",
		    n, n > 1 ? "s" : "" );
	    if( !fDisplay )
		outputl( "(You will also need to use `set display' to turn "
		      "board updates on -- see `help set display'.)" );
	} else
	    outputl( "Moves will not be delayed." );
	
	nDelay = n;
	UpdateSetting( &nDelay );
    } else
#endif
	outputl( "The `set delay' command applies only when using a window "
	      "system." );
}

extern void CommandSetDice( char *sz ) {

    int n0, n1;
    movesetdice *pmsd;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "There must be a game in progress to set the dice." );

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
	outputl( "You must specify two numbers from 1 to 6 for the dice." );

	return;
    }

    pmsd = malloc( sizeof( *pmsd ) );
    pmsd->mt = MOVE_SETDICE;
    pmsd->sz = NULL;
    pmsd->fPlayer = ms.fMove;
    pmsd->anDice[ 0 ] = n0;
    pmsd->anDice[ 1 ] = n1;
    pmsd->lt = LUCK_NONE;
    pmsd->rLuck = -HUGE_VALF;
    
    AddMoveRecord( pmsd );

    outputf( "The dice have been set to %d and %d.\n", n0, n1 );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetDisplay( char *sz ) {

    SetToggle( "display", &fDisplay, sz, "Will display boards for computer "
	       "moves.", "Will not display boards for computer moves." );
}

static evalcontext *pecSet;
static char *szSet, *szSetCommand;
static rolloutcontext *prcSet;

static evalsetup *pesSet;

static rng *rngSet;

extern void CommandSetEvalCandidates( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 2 || n > MAX_SEARCH_CANDIDATES ) {
	outputf( "You must specify a valid number of moves to consider -- try "
	      "`help set %sevaluation candidates'.\n", szSetCommand );

	return;
    }

    pecSet->nSearchCandidates = n;

    outputf( "%s will consider up to %d moves for evaluation at deeper "
	     "plies.\n", szSet, pecSet->nSearchCandidates );
}


extern void 
CommandSetEvalCubeful( char *sz ) {

    char asz[ 2 ][ 128 ], szCommand[ 64 ];
    
    sprintf( asz[ 0 ], "%s will use cubeful evaluation.\n", szSet );
    sprintf( asz[ 1 ], "%s will use cubeless evaluation.\n", szSet );
    sprintf( szCommand, "%sevaluation cubeful", szSetCommand );
    SetToggle( szCommand, &pecSet->fCubeful, sz, asz[ 0 ], asz[ 1 ] );
}

extern void CommandSetEvalDeterministic( char *sz ) {

    char asz[ 2 ][ 128 ], szCommand[ 64 ];
    
    sprintf( asz[ 0 ], "%s will use deterministic noise.\n", szSet );
    sprintf( asz[ 1 ], "%s will use pseudo-random noise.\n", szSet );
    sprintf( szCommand, "%sevaluation deterministic", szSetCommand );
    SetToggle( szCommand, &pecSet->fDeterministic, sz, asz[ 0 ], asz[ 1 ] );

    if( !pecSet->rNoise )
	outputl( "(Note that this setting will have no effect unless you "
		 "set noise to some non-zero value.)" );
}

extern void CommandSetEvalNoise( char *sz ) {
    
    double r = ParseReal( &sz );

    if( r < 0.0 ) {
	outputf( "You must specify a valid amount of noise to use -- "
		"try `help set\n%sevaluation noise'.\n", szSetCommand );

	return;
    }

    pecSet->rNoise = r;

    if( pecSet->rNoise )
	outputf( "%s will use noise with standard deviation %5.3f.\n",
		 szSet, pecSet->rNoise );
    else
	outputf( "%s will use noiseless evaluations.\n", szSet );
}

extern void CommandSetEvalPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	outputf( "You must specify a valid number of plies to look ahead -- "
		"try `help set %sevaluation plies'.\n", szSetCommand );

	return;
    }

    pecSet->nPlies = n;

    outputf( "%s will use %d ply evaluation.\n", szSet, pecSet->nPlies );
}

extern void CommandSetEvalReduced( char *sz ) {

    int n = ParseNumber( &sz );

    if( ( n < 0 ) || ( n > 21 ) ) {
	outputf( "You must specify a valid number -- "
		"try `help set %sevaluation reduced'.\n", szSetCommand );

	return;
    }

    if ( n == 0 || n == 21 )
      pecSet->nReduced = 0;
    else
      pecSet->nReduced = 3;

    outputf( "%s will use %d%% speed 2 ply evaluation.\n", 
	     szSet, 
	     pecSet->nReduced ? 100 / pecSet->nReduced : 100 );

    if( pecSet->nPlies != 2 )
	outputl( "(Note that this setting will have no effect until you "
		 "choose 2 ply evaluation.)" );
}

extern void CommandSetEvalTolerance( char *sz ) {

    double r = ParseReal( &sz );

    if( r < 0.0 ) {
	outputf( "You must specify a valid cubeless equity tolerance to use -- "
		"try `help set\n%sevaluation tolerance'.\n", szSetCommand );

	return;
    }

    pecSet->rSearchTolerance = r;

    outputf( "%s will select moves within %0.3g cubeless equity for\n"
	    "evaluation at deeper plies.\n", szSet, pecSet->rSearchTolerance );
}

extern void CommandSetEvaluation( char *sz ) {

    szSet = "`eval' and `hint'";
    szSetCommand = "";
    pecSet = &esEvalChequer.ec;
    HandleCommand( sz, acSetEvaluation );
}

extern void CommandSetNackgammon( char *sz ) {
    
    SetToggle( "nackgammon", &fNackgammon, sz, "New games will use the "
	       "Nackgammon starting position.", "New games will use the "
	       "standard backgammon starting position." );

#if USE_GUI
    if( fX && ms.gs == GAME_NONE )
	ShowBoard();
#endif
}


extern void 
CommandSetPlayerChequerplay( char *sz ) {

    szSet = ap[ iPlayerSet ].szName;
    szSetCommand = "player chequerplay ";
    pesSet = &ap[ iPlayerSet ].esChequer;

    HandleCommand( sz, acSetEvalParam );

    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( "(Note that this setting will have no effect until you "
		"`set player %s gnu'.)\n", ap[ iPlayerSet ].szName );

}


extern void 
CommandSetPlayerCubedecision( char *sz ) {

    szSet = ap[ iPlayerSet ].szName;
    szSetCommand = "player cubedecision ";
    pesSet = &ap[ iPlayerSet ].esCube;

    HandleCommand( sz, acSetEvalParam );

    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( "(Note that this setting will have no effect until you "
		"`set player %s gnu'.)\n", ap[ iPlayerSet ].szName );

}


extern void CommandSetPlayerExternal( char *sz ) {

#if !HAVE_SOCKETS
    outputl( "This installation of GNU Backgammon was compiled without\n"
	     "socket support, and does not implement external players." );
#else
    int h;
    struct sockaddr_un socsun;
       
    if( !sz || !*sz ) {
	outputl( "You must specify the name of the socket to the external\n"
		 "player -- try `help set player external'." );
	return;
    }

    if( ( h = socket( PF_LOCAL, SOCK_STREAM, 0 ) ) < 0 ) {
	perror( "socket" );
	return;
    }

    /* FIXME allow some syntax for TCP connections instead of AF_LOCAL */
    
    socsun.sun_family = AF_LOCAL;
    strcpy( socsun.sun_path, sz ); /* yuck!  opportunities for buffer overflow
				    here... but we didn't write the broken
				    interface */
    
    while( connect( h, (struct sockaddr *) &socsun, sizeof(socsun) ) < 0 ) {
	if( errno == EINTR ) {
	    if( fAction )
		fnAction();

	    if( fInterrupt ) {
		close( h );
		return;
	    }
	    
	    continue;
	}
	
	perror( sz );
	close( h );
	return;
    }
    
    ap[ iPlayerSet ].pt = PLAYER_EXTERNAL;
    ap[ iPlayerSet ].h = h;
#endif
}

extern void CommandSetPlayerGNU( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_GNU;

    outputf( "Moves for %s will now be played by GNU Backgammon.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerHuman( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_HUMAN;

    outputf( "Moves for %s must now be entered manually.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerName( char *sz ) {

    char *pch;
    
    if( !sz || !*sz ) {
	outputl( "You must specify a name to use." );

	return;
    }

    if( strlen( sz ) > 31 )
	sz[ 31 ] = 0;

    for( pch = sz; *pch; pch++ )
	if( isspace( *pch ) )
	    *pch = '_';
    
    if( ( *sz == '0' || *sz == '1' ) && !sz[ 1 ] ) {
	outputf( "`%c' is not a valid name.\n", *sz );

	return;
    }

    if( !strcasecmp( sz, "both" ) ) {
	outputl( "`both' is a reserved word; you can't call a player "
		 "that.\n" );

	return;
    }

    if( !strncasecmp( sz, ap[ !iPlayerSet ].szName,
		      sizeof( ap[ 0 ].szName ) ) ) {
	outputl( "That name is already in use by the other player." );

	return;
    }

    strcpy( ap[ iPlayerSet ].szName, sz );

    outputf( "Player %d is now known as `%s'.\n", iPlayerSet, sz );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetPlayerPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	outputl( "You must specify a vaid ply depth to look ahead -- try "
		 "`help set player plies'." );

	return;
    }

    ap[ iPlayerSet ].esChequer.ec.nPlies = n;
    
    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	outputf( "Moves for %s will be played with %d ply lookahead (note that "
		"this will\nhave no effect yet, because gnubg is not moving "
		"for this player).\n", ap[ iPlayerSet ].szName, n );
    else
	outputf( "Moves for %s will be played with %d ply lookahead.\n",
		ap[ iPlayerSet ].szName, n );
}

extern void CommandSetPlayerPubeval( char *sz ) {

    if( ap[ iPlayerSet ].pt == PLAYER_EXTERNAL )
	close( ap[ iPlayerSet ].h );
    
    ap[ iPlayerSet ].pt = PLAYER_PUBEVAL;

    outputf( "Moves for %s will now be played by pubeval.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayer( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputl( "You must specify a player -- try `help set player'." );
	
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
	    outputl( "Insufficient memory." );
		
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
    
    outputf( "Unknown player `%s' -- try `help set player'.\n", pch );
}

extern void CommandSetPrompt( char *szParam ) {

    static char sz[ 128 ]; /* FIXME check overflow */

    szPrompt = ( szParam && *szParam ) ? strcpy( sz, szParam ) :
	szDefaultPrompt;
    
    outputf( "The prompt has been set to `%s'.\n", szPrompt );
}

extern void CommandSetRNGAnsi( char *sz ) {

    SetRNG( RNG_ANSI, sz );
}

extern void CommandSetRNGBsd( char *sz ) {
#if HAVE_RANDOM
    SetRNG( RNG_BSD, sz );
#else
    outputl( "This installation of GNU Backgammon was compiled without the" );
    outputl( "BSD generator." );
#endif
}

extern void CommandSetRNGIsaac( char *sz ) {

    SetRNG( RNG_ISAAC, sz );
}

extern void CommandSetRNGManual( char *sz ) {

    SetRNG ( RNG_MANUAL, sz );
}

extern void CommandSetRNGMD5( char *sz ) {

    SetRNG ( RNG_MD5, sz );
}

extern void CommandSetRNGMersenne( char *sz ) {

    SetRNG( RNG_MERSENNE, sz );
}

extern void CommandSetRNGUser( char *sz ) {

#if HAVE_LIBDL
    SetRNG( RNG_USER, sz );
#else
    outputl( "This installation of GNU Backgammon was compiled without the" );
    outputl( "dynamic linking library needed for user RNG's." );
#endif

}


extern void CommandSetRollout ( char *sz ) {

  szSet = "Rollouts";
  szSetCommand = "rollout";
  
  prcSet = &rcRollout;
  HandleCommand ( sz, acSetRollout );

}


extern void
CommandSetRolloutRNG ( char *sz ) {

  rngSet = &prcSet->rngRollout;

  HandleCommand ( sz, acSetRNG );

  CommandNotImplemented ( sz );

}
  

extern void
CommandSetRolloutChequerplay ( char *sz ) {

  pecSet = &prcSet->aecChequer[ 0 ];

  HandleCommand ( sz, acSetEvaluation );

  /* copy to both players */
  memcpy ( &prcSet->aecChequer[ 1 ], &prcSet->aecChequer[ 0 ],
           sizeof ( evalcontext ) );  

}


extern void
CommandSetRolloutPlayerChequerplay ( char *sz ) {

  pecSet = &prcSet->aecChequer[ iPlayerSet ];

  HandleCommand ( sz, acSetEvaluation );

}


extern void
CommandSetRolloutCubedecision ( char *sz ) {

  pecSet = &prcSet->aecCube[ 0 ];

  HandleCommand ( sz, acSetEvaluation );

  /* copy to both players */
  memcpy ( &prcSet->aecCube[ 1 ], &prcSet->aecCube[ 0 ],
           sizeof ( evalcontext ) );  

}


extern void
CommandSetRolloutPlayerCubedecision ( char *sz ) {

  pecSet = &prcSet->aecCube[ iPlayerSet ];

  HandleCommand ( sz, acSetEvaluation );

}


extern void CommandSetRolloutSeed( char *sz ) {

    int n;
    
    if( prcSet->rngRollout == RNG_MANUAL ) {
	outputl( "You can't set a seed if you're using manual dice "
		 "generation." );
	return;
    }

    if( *sz ) {
	n = ParseNumber( &sz );

	if( n < 0 ) {
	    outputl( "You must specify a vaid seed -- try `help set seed'." );

	    return;
	}

	prcSet->nSeed = n;
	outputf( "Rollout seed set to %d.\n", n );
    } else
	outputl( InitRNG( &prcSet->nSeed, FALSE ) ?
		 "Rollout seed initialised from system random data." :
		 "Rollout seed initialised by system clock." );    
}

extern void CommandSetRolloutTrials( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( "You must specify a valid number of trials to make -- "
		"try `help set rollout trials'." );

	return;
    }

    prcSet->nTrials = n;

    outputf( "%d game%s will be played per rollout.\n", n,
	    n == 1 ? "" : "s" );
}

extern void CommandSetRolloutTruncation( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( "You must specify a valid ply at which to truncate  -- "
		"try `help set rollout truncation'." );

	return;
    }

    prcSet->nTruncate = n;

    outputf( "Rollouts will be truncated after %d pl%s.\n", n,
	    n == 1 ? "y" : "ies" );
}

extern void CommandSetRolloutVarRedn( char *sz ) {
    
    SetToggle( "rollout varredn", &prcSet->fVarRedn, sz,
               "Will lookahead during rollouts to reduce variance.",
               "Will not use lookahead variance "
               "reduction during rollouts." );
}

    
extern void 
CommandSetRolloutCubeful( char *sz ) {
    
    SetToggle( "rollout cubeful", &prcSet->fCubeful, sz, 
               "Cubeful rollout", "Cubeless rollout" );

}


extern void
CommandSetRolloutPlayer ( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	outputf( "You must specify a player -- try `help set %s player'.\n",
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
	    outputl( "Insufficient memory." );
		
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
    
    outputf( "Unknown player `%s' -- try\n"
             "`help set %s player'.\n", pch, szSetCommand );
}

extern void CommandSetScore( char *sz ) {

    int n0, n1;

    /* FIXME Allow specifying Crawford here, e.g. "set score crawford -3" or
       "set score 4C 3". */

    n0 = ParseNumber( &sz );
    n1 = ParseNumber( &sz );

    if( n0 < 0 ) /* -n means n-away */
	n0 += ms.nMatchTo;

    if( n1 < 0 )
	n1 += ms.nMatchTo;
    
    if( n0 < 0 || n1 < 0 ) {
	outputf( "You must specify two scores between 0 and %d.\n",
		 ms.nMatchTo );
	return;
    }

    ms.anScore[ 0 ] = n0;
    ms.anScore[ 1 ] = n1;

    ms.fCrawford = ( n0 == ms.nMatchTo - 1 ) != ( n1 == ms.nMatchTo - 1 );
    ms.fPostCrawford = ( n0 == ms.nMatchTo - 1 ) && ( n1 == ms.nMatchTo - 1 );
    
    CommandShowScore( NULL );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetSeed( char *sz ) {

    int n;
    
    if( rngCurrent == RNG_MANUAL ) {
	outputl( "You can't set a seed if you're using manual dice generation." );
	return;
    }

    if( *sz ) {
	n = ParseNumber( &sz );

	if( n < 0 ) {
	    outputl( "You must specify a vaid seed -- try `help set seed'." );

	    return;
	}

	InitRNGSeed( n );
	outputf( "Seed set to %d.\n", n );
    } else
	outputl( InitRNG( NULL, TRUE ) ?
		 "Seed initialised from system random data." :
		 "Seed initialised by system clock." );
}

extern void CommandSetTrainingAlpha( char *sz ) {

    float r = ParseReal( &sz );

    if( r <= 0.0f || r > 1.0f ) {
	outputl( "You must specify a value for alpha which is greater than\n"
		 "zero, and no more than one." );
	return;
    }

    rAlpha = r;
    outputf( "Alpha set to %f.\n", r );
}

extern void CommandSetTrainingAnneal( char *sz ) {

    double r = ParseReal( &sz );

    if( r == -HUGE_VAL ) {
	outputl( "You must specify a valid annealing rate." );
	return;
    }

    rAnneal = r;
    outputf( "Annealing rate set to %f.\n", r );
}

extern void CommandSetTrainingThreshold( char *sz ) {

    float r = ParseReal( &sz );

    if( r < 0.0f ) {
	outputl( "You must specify a valid error threshold." );
	return;
    }

    rThreshold = r;

    if( rThreshold )
	outputf( "Error threshold set to %f.\n", r );
    else
	outputl( "Error threshold disabled." );
}

extern void CommandSetTurn( char *sz ) {

    char *pch = NextToken( &sz );
    int i;

    if( ms.gs != GAME_PLAYING ) {
	outputl( "There must be a game in progress to set a player on roll." );

	return;
    }
    
    if( ms.fResigned ) {
	outputl( "Please resolve the resignation first." );

	return;
    }
    
    if( !pch ) {
	outputl( "Which player do you want to set on roll?" );

	return;
    }

    if( ( i = ParsePlayer( pch ) ) < 0 ) {
	outputf( "Unknown player `%s' -- try `help set turn'.\n", pch );

	return;
    }

    if( i == 2 ) {
	outputl( "You can't set both players on roll." );

	return;
    }

    if( ms.fTurn != i )
	SwapSides( ms.anBoard );
    
    ms.fTurn = ms.fMove = i;
    CancelCubeAction();
    fNextTurn = FALSE;
    ms.anDice[ 0 ] = ms.anDice[ 1 ] = 0;

    UpdateSetting( &ms.fTurn );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif
    
    outputf( "`%s' is now on roll.\n", ap[ i ].szName );
}

extern void CommandSetEgyptian( char *sz ) {

    if( SetToggle( "egyptian", &fEgyptian, sz,
                   "Will use the Egyptian rule.",
                   "Will not use the Egyptian rule." ) ) {
        outputl( "Note: most likely the database and weights are not "
                 "tuned for Egyptian play.\nRegenerating them with "
                 "the rule set may give better results." );
        return;
    } ;
}

extern void CommandSetJacoby( char *sz ) {

    if( SetToggle( "jacoby", &fJacoby, sz, 
		   "Will use the Jacoby rule for money sessions.",
		   "Will not use the Jacoby rule for money sessions." ) )
	return;

    if( fJacoby && !fCubeUse )
	outputl( "(Note that you'll have to enable the cube if you want "
		 "gammons and backgammons\nto be scored -- see `help set "
		 "cube use'.)" );
}

extern void CommandSetCrawford( char *sz ) {

  if ( ms.nMatchTo > 0 ) {
    if ( ( ms.nMatchTo - ms.anScore[ 0 ] == 1 ) || 
	 ( ms.nMatchTo - ms.anScore[ 1 ] == 1 ) ) {

      if( SetToggle( "crawford", &ms.fCrawford, sz, 
		 "This game is the Crawford game (no doubling allowed).",
		 "This game is not the Crawford game." ) < 0 )
	  return;

      /* sanity check */
      ms.fPostCrawford = !ms.fCrawford;

      if( ms.fCrawford )
	  CancelCubeAction();
    } else {
      outputl( "Cannot set whether this is the Crawford game\n"
	    "as none of the players are 1-away from winning." );
    }
  }
  else if ( !ms.nMatchTo ) 
      outputl( "Cannot set Crawford play for money sessions." );
  else
      outputl( "No match in progress (type `new match n' to start one)." );
}

extern void CommandSetPostCrawford( char *sz ) {

  if ( ms.nMatchTo > 0 ) {
    if ( ( ms.nMatchTo - ms.anScore[ 0 ] == 1 ) || 
	 ( ms.nMatchTo - ms.anScore[ 1 ] == 1 ) ) {

      SetToggle( "postcrawford", &ms.fPostCrawford, sz, 
		 "This is post-Crawford play (doubling allowed).",
		 "This is not post-Crawford play." );

      /* sanity check */
      ms.fCrawford = !ms.fPostCrawford;

      if( ms.fCrawford )
	  CancelCubeAction();
    } else {
      outputl( "Cannot set whether this is post-Crawford play\n"
	    "as none of the players are 1-away from winning." );
    }
  }
  else if ( !ms.nMatchTo ) 
      outputl( "Cannot set post-Crawford play for money sessions." );
  else
      outputl( "No match in progress (type `new match n' to start one)." );

}


extern void CommandSetBeavers( char *sz ) {

  SetToggle( "beavers", &fBeavers, sz,
             "Beavers/racoons allowed in money sessions.",
             "Beavers/raccons not allowed in money sessions." ); 

}

extern void CommandSetOutputMatchPC( char *sz ) {

    SetToggle( "output matchpc", &fOutputMatchPC, sz,
	       "Match winning chances will be shown as percentages.",
	       "Match winning chances will be shown as probabilities." );
}

extern void CommandSetOutputMWC( char *sz ) {

    SetToggle( "output mwc", &fOutputMWC, sz,
	       "Match evaluations will be shown as match winning chances.",
	       "Match evaluations will be shown as equivalent money equity." );
}

extern void CommandSetOutputRawboard( char *sz ) {

    SetToggle( "output rawboard", &fOutputRawboard, sz,
	       "TTY boards will be given in raw format.",
	       "TTY boards will be given in ASCII." );
}

extern void CommandSetOutputWinPC( char *sz ) {

    SetToggle( "output winpc", &fOutputWinPC, sz,
	       "Game winning chances will be shown as percentages.",
	       "Game winning chances will be shown as probabilities." );
}

static void SetMET( met metNew ) {

    static char *aszMET[] = {
      "Zadeh", "Snowie", "Woolsey", "Jacobs"
    };

    int nMaxScore;
    
    if( metCurrent == metNew ) {
	outputf( "You are already using the %s match equity table.\n",
		aszMET[ metNew ] );
    } else {
	outputf( "GNU Backgammon will now use the %s match equity table.\n",
		aszMET[ metNew ] );

        nMaxScore = GetMaxScore ( metNew );

        if ( nMaxScore < MAXSCORE ) {

          outputf ("\n"
                   "Note that this match equity table only supports "
                   "matches of length %i and below.\n"
                   "For scores above %i-away an extrapolation "
		   "scheme is used.\n",
                   nMaxScore, nMaxScore );

        }

        InitMatchEquity ( metNew );

	UpdateSetting( &metCurrent );
    }
}


extern void
CommandSetMETZadeh ( char *sz ) {

  SetMET ( MET_ZADEH );

}


extern void
CommandSetMETWoolsey ( char *sz ) {

  SetMET ( MET_WOOLSEY );

}


extern void
CommandSetMETSnowie ( char *sz ) {

  SetMET ( MET_SNOWIE );

}


extern void
CommandSetMETJacobs ( char *sz ) {

  SetMET ( MET_JACOBS );

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

  case 'n':
    pesSet->et = EVAL_NONE;
    break;

  default:
    outputf ("Unknown evaluation type: %s -- see\n"
             "`help set %s type'\n", sz, szSetCommand );
    return;
    break;

  }

  outputf ( "%s will now use %s.\n",
            szSet, aszEvalType[ pesSet->et ] );

}


extern void
CommandSetEvalParamEvaluation ( char *sz ) {

  pecSet = &pesSet->ec;

  HandleCommand ( sz, acSetEvaluation );

  if ( pesSet->et != EVAL_EVAL )
    outputf ( "(Note that this setting will have no effect until you\n"
              "`set %s type evaluation'\n",
              szSetCommand );
}


extern void
CommandSetEvalParamRollout ( char *sz ) {

  prcSet = &pesSet->rc;

  HandleCommand (sz, acSetRollout );

  if ( pesSet->et != EVAL_ROLLOUT )
    outputf ( "(Note that this setting will have no effect until you\n"
              "`set %s type rollout'\n",
              szSetCommand );

}


extern void
CommandSetAnalysisChequerplay ( char *sz ) {

  pesSet = &esAnalysisChequer;

  szSet = "Analysis chequerplay";
  szSetCommand = "analysis chequerplay";

  HandleCommand( sz, acSetEvalParam );

}

extern void
CommandSetAnalysisCubedecision ( char *sz ) {


  pesSet = &esAnalysisCube;

  szSet = "Analysis cubedecision";
  szSetCommand = "analysis cubedecision";

  HandleCommand( sz, acSetEvalParam );

}


extern void
CommandSetEvalChequerplay ( char *sz ) {

  pesSet = &esEvalChequer;

  szSet = "`eval' and `hint' chequerplay";
  szSetCommand = "evaluation chequerplay ";

  HandleCommand( sz, acSetEvalParam );

}

extern void
CommandSetEvalCubedecision ( char *sz ) {


  pesSet = &esEvalCube;

  szSet = "`eval' and `hint' cube decisions";
  szSetCommand = "evaluation cubedecisions ";

  HandleCommand( sz, acSetEvalParam );

}


