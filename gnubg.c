/*
 * gnubg.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998, 1999, 2000.
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
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#if HAVE_PWD_H
#include <pwd.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
static int fReadingOther;
#endif

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "getopt.h"
#include "positionid.h"
#include "rollout.h"
#include "matchequity.h"
#include "analysis.h"

#if USE_GTK
#include <gtk/gtk.h>
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#include "gtkboard.h"
#include "gtkgame.h"
#elif USE_EXT
#include <ext.h>
#include <extwin.h>
#include <stdio.h>
#include "xgame.h"

extwindow ewnd;
event evNextTurn;
#endif

#if USE_GUI
int fX = TRUE; /* use X display */
int nDelay = 0;
int fNeedPrompt = FALSE;
#if HAVE_LIBREADLINE
int fReadingCommand;
#endif
#endif

#ifndef SIGIO
#define SIGIO SIGPOLL /* The System V equivalent */
#endif

char szDefaultPrompt[] = "(\\p) ",
    *szPrompt = szDefaultPrompt;
static int fInteractive, cOutputDisabled;

int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn = -1, fDisplay = TRUE,
  fAutoBearoff = FALSE, fAutoGame = TRUE, fAutoMove = FALSE,
  fResigned = FALSE, nPliesEval = 1, fAutoCrawford = 1,
  fAutoRoll = TRUE, cGames = 0, fDoubled = FALSE, cAutoDoubles = 0,
  fCubeUse = TRUE, fNackgammon = FALSE, fVarRedn = FALSE,
  nRollouts = 1296, nRolloutTruncate = 7, fNextTurn = FALSE,
  fConfirm = TRUE, fShowProgress;

evalcontext ecTD = { 0, 8, 0.16, 0, FALSE }, ecEval = { 1, 8, 0.16, 0, FALSE },
    ecRollout = { 0, 8, 0.16, 0, FALSE };

player ap[ 2 ] = {
    { "gnubg", PLAYER_GNU, { 0, 8, 0.16, 0, FALSE } },
    { "user", PLAYER_HUMAN, { 0, 8, 0.16, 0, FALSE } }
};

/* Usage strings */
static char szDICE[] = "<die> <die>",
    szFILENAME[] = "<filename>",
    szLENGTH[] = "<length>",
    szLIMIT[] = "<limit>",
    szMILLISECONDS[] = "<milliseconds>",
    szMOVE[] = "<from> <to> ...",
    szONOFF[] = "on|off",
    szOPTCOMMAND[] = "[command]",
    szOPTPOSITION[] = "[position]",
    szOPTSEED[] = "[seed]",
    szPLAYER[] = "<player>",
    szPLIES[] = "<plies>",
    szPOSITION[] = "<position>",
    szPROMPT[] = "<prompt>",
    szSCORE[] = "<score>",
    szSEED[] = "<seed>",
    szSIZE[] = "<size>",
    szTRIALS[] = "<trials>",
    szVALUE[] = "<value>",
    szOPTVALUE[] = "[value]";

command acDatabase[] = {
    { "dump", CommandDatabaseDump, "List the positions in the database",
      NULL, NULL },
    { "generate", CommandDatabaseGenerate, "Generate database positions by "
      "self-play", NULL, NULL },
    { "rollout", CommandDatabaseRollout, "Evaluate positions in database "
      "for future training", NULL, NULL },
    { "train", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acLoad[] = {
    { "commands", CommandLoadCommands, "Read commands from a script file",
      szFILENAME, NULL },
    /* FIXME add equivalents to the other save commands here */
    { NULL, NULL, NULL, NULL, NULL }
}, acNew[] = {
    { "game", CommandNewGame, "Start a new game within the current match or "
      "session", NULL, NULL },
    { "match", CommandNewMatch, "Play a new match to some number of points",
      szLENGTH, NULL },
    { "session", CommandNewSession, "Start a new (money) session", NULL,
      NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSave[] = {
    { "game", CommandSaveGame, "Record a log of the game so far to a "
      "file", szFILENAME, NULL },
    { "match", CommandSaveMatch, "Record a log of the match so far to a file",
      szFILENAME, NULL },
    { "settings", CommandSaveSettings, "Use the current settings in future "
      "sessions", NULL, NULL },
    { "weights", CommandSaveWeights, "Write the neural net weights to a file",
      szFILENAME, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetAutomatic[] = {
    { "bearoff", CommandSetAutoBearoff, "Automatically bear off as many "
      "chequers as possible", szONOFF, NULL },
    { "crawford", CommandSetAutoCrawford, "Enable the Crawford game "
      "based on match score", szONOFF, NULL },
    { "doubles", CommandSetAutoDoubles, "Control automatic doubles "
      "during (money) session play", szLIMIT, NULL },
    { "game", CommandSetAutoGame, "Select whether to start new games "
      "after wins", szONOFF, NULL },
    { "move", CommandSetAutoMove, "Select whether forced moves will be "
      "made automatically", szONOFF, NULL },
    { "roll", CommandSetAutoRoll, "Control whether dice will be rolled "
      "automatically", szONOFF, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetCube[] = {
    { "center", CommandSetCubeCentre, "The U.S.A. spelling of `centre'",
      NULL, NULL },
    { "centre", CommandSetCubeCentre, "Allow both players access to the "
      "cube", NULL, NULL },
    { "owner", CommandSetCubeOwner, "Allow only one player to double",
      szPLAYER, NULL },
    { "use", CommandSetCubeUse, "Control use of the doubling cube", szONOFF,
      NULL },
    { "value", CommandSetCubeValue, "Fix what the cube stake has been set to",
      szVALUE, NULL },
    { NULL, NULL, NULL, NULL }
}, acSetRNG[] = {
    { "ansi", CommandSetRNGAnsi, "Use the ANSI C rand() (usually linear "
      "congruential) generator", szOPTSEED, NULL },
    { "bsd", CommandSetRNGBsd, "Use the BSD random() non-linear additive "
      "feedback generator", szOPTSEED, NULL },
    { "isaac", CommandSetRNGIsaac, "Use the I.S.A.A.C. generator", szOPTSEED,
      NULL },
    { "manual", CommandSetRNGManual, "Enter all dice rolls manually", NULL,
      NULL },
    { "md5", CommandSetRNGMD5, "Use the MD5 generator", szOPTSEED, NULL },
    { "mersenne", CommandSetRNGMersenne, "Use the Mersenne Twister generator",
      szOPTSEED, NULL },
    { "user", CommandSetRNGUser, "Specify an external generator", szOPTSEED,
      NULL,},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRollout[] = {
    { "evaluation", CommandSetRolloutEvaluation, "Specify parameters "
      "for evaluation during rollouts", NULL, acSetEvaluation },
    { "seed", CommandNotImplemented, "Specify the base pseudo-random seed "
      "to use for rollouts", szSEED, NULL },
    { "trials", CommandSetRolloutTrials, "Control how many rollouts to "
      "perform", szTRIALS, NULL },
    { "truncation", CommandSetRolloutTruncation, "End rollouts at a "
      "particular depth", szPLIES, NULL },
    { "varredn", CommandSetRolloutVarRedn, "Use lookahead during rollouts "
      "to reduce variance", szONOFF, NULL },
    /* FIXME add commands for cubeful rollouts, cube variance reduction,
       quasi-random dice, settlements... */
    { NULL, NULL, NULL, NULL, NULL }
}, acSet[] = {
    { "automatic", NULL, "Perform certain functions without user input",
      NULL, acSetAutomatic },
    { "board", CommandSetBoard, "Set up the board in a particular "
      "position", szPOSITION, NULL },
    { "beavers", CommandSetBeavers, 
      "Set whether beavers are allowed in money game or not", 
      szONOFF, NULL },
    { "cache", CommandSetCache, "Set the size of the evaluation cache",
      szSIZE, NULL },
    { "confirm", CommandSetConfirm, "Ask for confirmation before aborting "
      "a game in progress", szONOFF, NULL },
    { "crawford", CommandSetCrawford, 
      "Set whether this is the Crawford game", szONOFF, NULL },
    { "cube", NULL, "Set the cube owner and/or value", NULL, acSetCube },
    { "delay", CommandSetDelay, "Limit the speed at which moves are made",
      szMILLISECONDS, NULL },
    { "dice", CommandSetDice, "Select the roll for the current move",
      szDICE, NULL },
    { "display", CommandSetDisplay, "Select whether the board is updated on "
      "the computer's turn", szONOFF, NULL },
    { "evaluation", CommandSetEvaluation, "Control position evaluation "
      "parameters", NULL, acSetEvaluation },
    { "jacoby", CommandSetJacoby, "Set whether to use the Jacoby rule in "
      "money games", szONOFF, NULL },
    { "nackgammon", CommandSetNackgammon, "Set the starting position",
      szONOFF, NULL },
    { "outputmwc", CommandSetOutputMWC, "Show output in MWC (on) or "
      "equity (off) (match play only)", szONOFF, NULL },
    { "player", CommandSetPlayer, "Change options for one or both "
      "players", szPLAYER, acSetPlayer },
    { "postcrawford", CommandSetPostCrawford, 
      "Set whether this is a post-Crawford game", szONOFF, NULL },
    { "prompt", CommandSetPrompt, "Customise the prompt gnubg prints when "
      "ready for commands", szPROMPT, NULL },
    { "rng", NULL, "Select the random number generator algorithm", NULL,
      acSetRNG },
    { "rollout", NULL, "Control rollout parameters", NULL, acSetRollout },
    { "score", CommandSetScore, "Set the match or session score ",
      szSCORE, NULL },
    { "seed", CommandSetSeed, "Set the dice generator seed", szSEED, NULL },
    { "turn", CommandSetTurn, "Set which player is on roll", szPLAYER, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acShow[] = {
    { "automatic", CommandShowAutomatic, "List which functions will be "
      "performed without user input", NULL, NULL },
    { "board", CommandShowBoard, "Redisplay the board position", szOPTPOSITION,
      NULL },
    { "beavers", CommandShowBeavers, 
      "Show whether beavers are allowed in money game or not", 
      NULL, NULL },
    { "cache", CommandShowCache, "Display statistics on the evaluation "
      "cache", NULL, NULL },
    { "confirm", CommandShowConfirm, "Show whether confirmation is required "
      "before aborting a game in progress", NULL, NULL },
    { "copying", CommandShowCopying, "Conditions for redistributing copies "
      "of GNU Backgammon", NULL, NULL },
    { "crawford", CommandShowCrawford, 
      "See if this is the Crawford game", NULL, NULL },
    { "cube", CommandShowCube, "Display the current cube value and owner",
      NULL, NULL },
    { "delay", CommandShowDelay, "See what the current delay setting is", 
      NULL, NULL },
    { "dice", CommandShowDice, "See what the current dice roll is", NULL,
      NULL },
    { "display", CommandShowDisplay, "Show whether the board will be updated "
      "on the computer's turn", NULL, NULL },
    { "evaluation", CommandShowEvaluation, "Display evaluation settings "
      "and statistics", NULL, acSetEvaluation },
    { "gammonprice", CommandShowGammonPrice, "Show gammon price",
      NULL, NULL },
    { "jacoby", CommandShowJacoby, 
      "See if the Jacoby rule is used in money sessions", NULL, NULL },
    { "kleinman", CommandShowKleinman, "Calculate Kleinman count for "
      "position", szOPTPOSITION, NULL },
    { "matchequitytable", CommandShowMatchEquityTable, 
      "Show match equity table", szOPTVALUE, NULL },
    { "nackgammon", CommandShowNackgammon, "Display which starting position "
      "will be used", NULL, NULL },
    { "outputmwc", CommandShowOutputMWC, "Show whether output is in MWC or "
      "equity (match play only)", NULL, NULL },
    { "pipcount", CommandShowPipCount, "Count the number of pips each player "
      "must move to bear off", szOPTPOSITION, NULL },
    { "player", CommandShowPlayer, "View per-player options", NULL, NULL },
    { "postcrawford", CommandShowCrawford, 
      "See if this is post-Crawford play", NULL, NULL },
    { "prompt", CommandShowPrompt, "Show the prompt that will be printed "
      "when ready for commands", NULL, NULL },
    { "rng", CommandShowRNG, "Display which random number generator "
      "is being used", NULL, NULL },
    { "rollout", CommandShowRollout, "Display the evaluation settings used "
      "during rollouts", NULL, NULL },
    { "score", CommandShowScore, "View the match or session score ",
      NULL, NULL },
    { "seed", CommandShowSeed, "Show the dice generator seed", NULL, NULL },
    { "thorp", CommandShowThorp, "Calculate Thorp Count for "
      "position", szOPTPOSITION, NULL },
    { "turn", CommandShowTurn, "Show which player is on roll", NULL, NULL },
    { "warranty", CommandShowWarranty, "Various kinds of warranty you do "
      "not have", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acTrain[] = {
    { "database", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL, NULL },
    { "td", CommandTrainTD, "Train the network by TD(0) zero-knowledge "
      "self-play", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acTop[] = {
    { "accept", CommandAccept, "Accept a cube or resignation",
      NULL, NULL },
    { "agree", CommandAgree, "Agree to a resignation", NULL, NULL },
    { "analysis", CommandAnalysis, "Run analysis", szFILENAME, NULL },
    { "beaver", CommandRedouble, "Synonym for `redouble'", NULL, NULL },
    { "database", NULL, "Manipulate a database of positions", NULL,
      acDatabase },
    { "decline", CommandDecline, "Decline a resignation", NULL, NULL },
    { "double", CommandDouble, "Offer a double", NULL, NULL },
    { "drop", CommandDrop, "Decline an offered double", NULL, NULL },
    { "eval", CommandEval, "Display evaluation of a position", szOPTPOSITION,
      NULL },
    { "exit", CommandQuit, "Leave GNU Backgammon", NULL, NULL },
    { "help", CommandHelp, "Describe commands", szOPTCOMMAND, NULL },
    { "hint", CommandHint, "Show the best evaluations of legal "
      "moves", NULL, NULL },
    { "load", NULL, "Read data from a file", NULL, acLoad },
    { "move", CommandMove, "Make a backgammon move", szMOVE, NULL },
    { "new", NULL, "Start a new game, match or session", NULL, acNew },
    { "pass", CommandDrop, "Synonum for `drop'", NULL, NULL },
    { "play", CommandPlay, "Force the computer to move", NULL, NULL },
    { "quit", CommandQuit, "Leave GNU Backgammon", NULL, NULL },
    { "r", CommandRoll, NULL, NULL, NULL },
    { "redouble", CommandRedouble, "Accept the cube one level higher "
      "than it was offered", NULL, NULL },
    { "reject", CommandReject, "Reject a cube or resignation", NULL, NULL },
    { "resign", CommandResign, "Offer to end the current game", szVALUE,
      NULL },
    { "roll", CommandRoll, "Roll the dice", NULL, NULL },
    { "rollout", CommandRollout, "Have gnubg perform rollouts of a position",
      szOPTPOSITION, NULL },
    { "save", NULL, "Write data to a file", NULL, acSave },
    { "set", NULL, "Modify program parameters", NULL, acSet },
    { "show", NULL, "View program parameters", NULL, acShow },
    { "take", CommandTake, "Agree to an offered double", NULL, NULL },
    { "train", NULL, "Update gnubg's weights from training data", NULL,
      acTrain },
    { "?", CommandHelp, "Describe commands", szOPTCOMMAND, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, cTop = { NULL, NULL, NULL, NULL, acTop };

static char szCommandSeparators[] = " \t\n\r\v\f";

extern char *NextToken( char **ppch ) {

    char *pch;

    if( !*ppch )
	return NULL;
    
    while( isspace( **ppch ) )
	( *ppch )++;

    if( !*( pch = *ppch ) )
	return NULL;

    while( **ppch && !isspace( **ppch ) )
	( *ppch )++;

    if( **ppch )
	*( *ppch )++ = 0;

    while( isspace( **ppch ) )
	( *ppch )++;

    return pch;
}

extern double ParseReal( char **ppch ) {

    char *pch, *pchOrig;
    double r;
    
    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return -HUGE_VAL;

    r = strtod( pchOrig, &pch );

    return *pch ? -HUGE_VAL : r;
}

extern int ParseNumber( char **ppch ) {

    char *pch, *pchOrig;

    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return INT_MIN;

    for( pch = pchOrig; *pch; pch++ )
	if( !isdigit( *pch ) && *pch != '-' )
	    return INT_MIN;

    return atoi( pchOrig );
}

extern int ParsePlayer( char *sz ) {

    int i;
    
    if( ( *sz == '0' || *sz == '1' ) && !sz[ 1 ] )
	return *sz - '0';

    for( i = 0; i < 2; i++ )
	if( !strcasecmp( sz, ap[ i ].szName ) )
	    return i;

    if( !strncasecmp( sz, "both", strlen( sz ) ) )
	return 2;

    return -1;
}

extern int ParsePosition( int an[ 2 ][ 25 ], char *sz ) {
 
    /* FIXME allow more formats */

    if( !sz || !*sz ) {
	memcpy( an, anBoard, sizeof( anBoard ) );

	return 0;
    }

    /* FIXME be more strict than this -- fail if both players are on the
       bar against closed boards, or if either player has no chequers
       remaining. */
    
    return PositionFromID( an, sz );
}

extern void UpdateSetting( void *p ) {
#if USE_GTK
    if( fX )
	GTKSet( p );
#endif
}

extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff ) {

    char *pch = NextToken( &sz );
    int cch;
    
    if( !pch ) {
	outputf( "You must specify whether to set %s on or off (see `help set "
		"%s').\n", szName, szName );

	return -1;
    }

    cch = strlen( pch );
    
    if( !strcasecmp( "on", pch ) || !strncasecmp( "yes", pch, cch ) ||
	!strncasecmp( "true", pch, cch ) ) {
	if( !*pf ) {
	    *pf = TRUE;
	    UpdateSetting( pf );
	}
	
	outputl( szOn );

	return TRUE;
    }

    if( !strcasecmp( "off", pch ) || !strncasecmp( "no", pch, cch ) ||
	!strncasecmp( "false", pch, cch ) ) {
	if( pf ) {
	    *pf = FALSE;
	    UpdateSetting( pf );
	}
	
	outputl( szOff );

	return FALSE;
    }

    outputf( "Illegal keyword `%s' -- try `help set %s'.\n", pch, szName );

    return -1;
}

static command *FindContext( command *pc, char *sz, int ich, int fDeep ) {

    int i = 0, c;

    do {
        while( i < ich && isspace( sz[ i ] ) )
            i++;

        if( i == ich )
            /* no command */
            return pc;

        c = strcspn( sz + i, szCommandSeparators );

        if( i + c >= ich && !fDeep )
            /* incomplete command */
            return pc;

        while( pc && pc->sz ) {
            if( !strncasecmp( sz + i, pc->sz, c ) ) {
                pc = pc->pc;

                if( i + c >= ich )
                    return pc;
                
                i += c;
                break;
            }

            pc++;
        }
    } while( pc && pc->sz );

    return NULL;
}

#if HAVE_SIGACTION
typedef struct sigaction psighandler;
#elif HAVE_SIGVEC
typedef struct sigvec psighandler;
#else
typedef RETSIGTYPE (*psighandler)( int );
#endif

static void PortableSignal( int nSignal, RETSIGTYPE (*p)(int),
			     psighandler *pOld ) {
#if HAVE_SIGACTION
    struct sigaction sa;

    sa.sa_handler = p;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags =
# if SA_RESTART
	SA_RESTART |
# endif
# if SA_NOCLDSTOP
	SA_NOCLDSTOP |
# endif
	0;
    sigaction( nSignal, &sa, pOld );
#elif HAVE_SIGVEC
    struct sigvec sv;

    sv.sv_handler = p;
    sigemptyset( &sv.sv_mask );
    sv.sv_flags = 0;

    sigvec( nSignal, &sv, pOld );
#else
    if( pOld )
	*pOld = signal( nSignal, p );
    else
	signal( nSignal, p );
#endif
}

static void PortableSignalRestore( int nSignal, psighandler *p ) {
#if HAVE_SIGACTION
    sigaction( nSignal, p, NULL );
#elif HAVE_SIGVEC
    sigvec( nSignal, p, NULL );
#else
    signal( nSignal, p );
#endif
}

/* Reset the SIGINT handler, on return to the main command loop.  Notify
   the user if processing had been interrupted. */
extern void ResetInterrupt( void ) {
    
    if( fInterrupt ) {
	outputl( "(Interrupted)" );
	outputx();
	
	fInterrupt = FALSE;
	
#if USE_GTK
	if( nNextTurn ) {
	    gtk_idle_remove( nNextTurn );
	    nNextTurn = 0;
	}
#elif USE_EXT
        EventPending( &evNextTurn, FALSE );
#endif
    }
}

#if USE_GUI
static int fChildDied;

static RETSIGTYPE HandleChild( int n ) {

    fChildDied = TRUE;
}
#endif

void ShellEscape( char *pch ) {

    pid_t pid;
    char *pchShell;
    psighandler shQuit;
#if USE_GUI
    psighandler shChild;
    
    PortableSignal( SIGCHLD, HandleChild, &shChild );
#endif
    PortableSignal( SIGQUIT, SIG_IGN, &shQuit );
    
    if( ( pid = fork() ) < 0 ) {
	/* Error */
	perror( "fork" );

#if USE_GUI
	PortableSignalRestore( SIGCHLD, &shChild );
#endif
	PortableSignalRestore( SIGQUIT, &shQuit );
	
	return;
    } else if( !pid ) {
	/* Child */
	PortableSignalRestore( SIGQUIT, &shQuit );	
	
	if( pch && *pch )
	    execl( "/bin/sh", "sh", "-c", pch, NULL );
	else {
	    if( !( pchShell = getenv( "SHELL" ) ) )
		pchShell = "/bin/sh";
	    execl( pchShell, pchShell, NULL );
	}
	_exit( EXIT_FAILURE );
    }
    
    /* Parent */
#if USE_GUI
 TryAgain:
#if HAVE_SIGPROCMASK
    {
	sigset_t ss, ssOld;

	sigemptyset( &ss );
	sigaddset( &ss, SIGCHLD );
	sigaddset( &ss, SIGIO );
	sigprocmask( SIG_BLOCK, &ss, &ssOld );
	
	while( !fChildDied ) {
	    sigsuspend( &ssOld );
	    if( fAction )
	    HandleXAction();
	}

	fChildDied = FALSE;
	sigprocmask( SIG_UNBLOCK, &ss, NULL );
    }
#elif HAVE_SIGBLOCK
    {
	int n;

	n = sigblock( sigmask( SIGCHLD ) | sigmask( SIGIO ) );

	while( !fChildDied ) {
	    sigpause( n );
	    if( fAction )
		HandleXAction();
	}
	fChildDied = FALSE;
	sigsetmask( n );
    }
#else
    /* NB: Without reliable signal handling semantics (sigsuspend or
       sigpause), we can't avoid a race condition here because the
       test of fChildDied and pause() are not atomic. */
    while( !fChildDied ) {
	pause();
	if( fAction )
	    HandleXAction();
    }
    fChildDied = FALSE;
#endif
    
    if( !waitpid( pid, NULL, WNOHANG ) )
	/* Presumably the child is stopped and not dead. */
	goto TryAgain;
    
    PortableSignalRestore( SIGCHLD, &shChild );
#else
    while( !waitpid( pid, NULL, 0 ) )
	;
#endif

    PortableSignalRestore( SIGCHLD, &shQuit );
    
    return;
}

extern void HandleCommand( char *sz, command *ac ) {

    command *pc;
    char *pch;
    int cch;
    
    if( ac == acTop ) {
	outputnew();
    
	if( *sz == '!' ) {
	    /* Shell escape */
	    for( pch = sz + 1; isspace( *pch ); pch++ )
		;

	    ShellEscape( pch );
	    outputx();
	    return;
	} else if( *sz == ':' ) {
	    /* Guile escape */
	    outputl( "This installation of GNU Backgammon was compiled without "
		  "Guile support." );
	    outputx();
	    return;
	}
    }
    
    if( !( pch = NextToken( &sz ) ) ) {
	if( ac != acTop )
	    outputl( "Incomplete command -- try `help'." );

	outputx();
	return;
    }

    cch = strlen( pch );

    if( ac == acTop && ( isdigit( *pch ) ||
			 !strncasecmp( pch, "bar/", cch > 4 ? 4 : cch ) ) ) {
	if( pch + cch < sz )
	    pch[ cch ] = ' ';
	
	CommandMove( pch );

	outputx();
	return;
    }

    for( pc = ac; pc->sz; pc++ )
	if( !strncasecmp( pch, pc->sz, cch ) )
	    break;

    if( !pc->sz ) {
	outputf( "Unknown keyword `%s' -- try `help'.\n", pch );

	outputx();
	return;
    }

    if( pc->pf ) {
	pc->pf( sz );
	
	outputx();
    } else
	HandleCommand( sz, pc->pc );
}

extern void InitBoard( int anBoard[ 2 ][ 25 ] ) {

    int i;
    
    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ] = 0;

    anBoard[ 0 ][ 5 ] = anBoard[ 1 ][ 5 ] =
	anBoard[ 0 ][ 12 ] = anBoard[ 1 ][ 12 ] = fNackgammon ? 4 : 5;
    anBoard[ 0 ][ 7 ] = anBoard[ 1 ][ 7 ] = 3;
    anBoard[ 0 ][ 23 ] = anBoard[ 1 ][ 23 ] = 2;

    if( fNackgammon )
	anBoard[ 0 ][ 22 ] = anBoard[ 1 ][ 22 ] = 2;
}

extern void ShowBoard( void ) {

    char szBoard[ 2048 ];
    char sz[ 32 ], szCube[ 32 ], szPlayer0[ 35 ], szPlayer1[ 35 ];
    char *apch[ 7 ] = { szPlayer0, NULL, NULL, NULL, NULL, NULL, szPlayer1 };

    if( cOutputDisabled )
	return;
    
    if( fTurn == -1 ) {
#if USE_GUI
	if( fX ) {
	    InitBoard( anBoard );
#if USE_GTK
	    game_set( BOARD( pwBoard ), anBoard, 0, ap[ 1 ].szName,
		      ap[ 0 ].szName, nMatchTo, anScore[ 1 ], anScore[ 0 ],
		      -1, -1 );
#else
            GameSet( &ewnd, anBoard, 0, ap[ 1 ].szName, ap[ 0 ].szName,
                     nMatchTo, anScore[ 1 ], anScore[ 0 ], -1, -1 );
#endif
	} else
#endif
	    outputl( "No game in progress." );
	
	return;
    }
    
#if USE_GUI
    if( !fX ) {
#endif
	if( fDoubled ) {
	    apch[ fTurn ? 5 : 1 ] = szCube;

	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );
	    sprintf( szCube, "Cube offered at %d", nCube << 1 );
	} else {
	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

	    apch[ fMove ? 5 : 1 ] = sz;
	
	    if( anDice[ 0 ] )
		sprintf( sz, "Rolled %d%d", anDice[ 0 ], anDice[ 1 ] );
	    else if( !GameStatus( anBoard ) )
		strcpy( sz, "On roll" );
	    else
		sz[ 0 ] = 0;
	    
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
    
	if( fResigned )
	    sprintf( strchr( sz, 0 ), ", resigns %s",
		     aszGameResult[ fResigned - 1 ] );
	
	if( !fMove )
	    SwapSides( anBoard );
	
	outputl( DrawBoard( szBoard, anBoard, fMove, apch ) );
	
	if( !fMove )
	    SwapSides( anBoard );
#if USE_GUI
    } else {
	if( !fMove )
	    SwapSides( anBoard );

#if USE_GTK
	game_set( BOARD( pwBoard ), anBoard, fMove, ap[ 1 ].szName,
		  ap[ 0 ].szName, nMatchTo, anScore[ 1 ], anScore[ 0 ],
		  anDice[ 0 ], anDice[ 1 ] );
#else
        GameSet( &ewnd, anBoard, fMove, ap[ 1 ].szName, ap[ 0 ].szName,
                 nMatchTo, anScore[ 1 ], anScore[ 0 ], anDice[ 0 ],
                 anDice[ 1 ] );	
#endif
	if( !fMove )
	    SwapSides( anBoard );
#if USE_GTK
	gdk_flush();
#else
	XFlush( ewnd.pdsp );
#endif
    }    
#endif    
}

extern char *FormatPrompt( void ) {

    static char sz[ 128 ]; /* FIXME check for overflow in rest of function */
    char *pch = szPrompt, *pchDest = sz;
    int anPips[ 2 ];

    while( *pch )
	if( *pch == '\\' ) {
	    pch++;
	    switch( *pch ) {
	    case 0:
		goto done;
		
	    case 'c':
	    case 'C':
		/* Pip count */
		if( fTurn < 0 )
		    strcpy( pchDest, "No game" );
		else {
		    PipCount( anBoard, anPips );
		    sprintf( pchDest, "%d:%d", anPips[ 1 ], anPips[ 0 ] );
		}
		break;

	    case 'p':
	    case 'P':
		/* Player on roll */
		strcpy( pchDest, fTurn < 0 ? "No game" : ap[ fTurn ].szName );
		break;
		
	    case 's':
	    case 'S':
		/* Match score */
		if( fTurn < 0 )
		    strcpy( pchDest, "No game" );
		else
		    sprintf( pchDest, "%d:%d", anScore[ fTurn ],
			     anScore[ !fTurn ] );
		break;

	    case 'v':
	    case 'V':
		/* Version */
		strcpy( pchDest, VERSION );
		break;
		
	    default:
		*pchDest++ = *pch;
		*pchDest = 0;
	    }

	    pchDest = strchr( pchDest, 0 );
	    pch++;
	} else
	    *pchDest++ = *pch++;
    
 done:
    *pchDest = 0;

    return sz;
}

extern void CommandEval( char *sz ) {

    char szOutput[ 2048 ];
    int an[ 2 ][ 25 ];

    if( !*sz && fTurn == -1 ) {
	outputl( "No position specified and no game in progress." );
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	outputl( "Illegal position." );

	return;
    }

    if( !DumpPosition( an, szOutput, &ecEval ) )
	outputl( szOutput );
}

static command *FindHelpCommand( command *pcBase, char *sz,
				 char *pchCommand, char *pchUsage ) {

    command *pc;
    char *pch;
    int cch;
    
    if( !( pch = NextToken( &sz ) ) )
	return pcBase;

    cch = strlen( pch );

    for( pc = pcBase->pc; pc && pc->sz; pc++ )
	if( !strncasecmp( pch, pc->sz, cch ) )
	    break;

    if( !pc || !pc->sz )
	return NULL;

    pch = pc->sz;
    while( *pch )
	*pchCommand++ = *pchUsage++ = *pch++;
    *pchCommand++ = ' '; *pchCommand = 0;
    *pchUsage++ = ' '; *pchUsage = 0;

    if( pc->szUsage ) {
	pch = pc->szUsage;
	while( *pch )
	    *pchUsage++ = *pch++;
	*pchUsage++ = ' '; *pchUsage = 0;	
    }
    
    if( pc->pc )
	/* subcommand */
	return FindHelpCommand( pc, sz, pchCommand, pchUsage );
    else
	/* terminal command */
	return pc;
}

extern void CommandHelp( char *sz ) {

    command *pc;
    char szCommand[ 128 ], szUsage[ 128 ];
    
    if( !( pc = FindHelpCommand( &cTop, sz, szCommand, szUsage ) ) ) {
	outputf( "No help available for topic `%s' -- try `help' for a list of "
		"topics.\n", sz );

	return;
    }

    if( pc->szHelp ) {
	outputf( "%s- %s\n\nUsage: %s", szCommand, pc->szHelp,
		szUsage );

	if( pc->pc )
	    outputl( "<subcommand>\n" );
	else
	    outputc( '\n' );
    }

    if( pc->pc ) {
	outputl( pc == &cTop ? "Available commands:" : "Available subcommands:" );

	pc = pc->pc;
	
	for( ; pc->sz; pc++ )
	    if( pc->szHelp )
		outputf( "%-15s\t%s\n", pc->sz, pc->szHelp );
    }
}

extern void CommandHint( char *sz ) {

    movelist ml;
    int i;
    char szMove[ 32 ], szTemp[ 1024 ];
    float aar[ 32 ][ NUM_OUTPUTS ], arDouble[ 4 ];
    cubeinfo ci;
    
    if( fTurn < 0 ) {
      outputl( "You must set up a board first." );
      
      return;
    }

    if( !anDice[ 0 ] && !fDoubled ) {

      /* Give hint on cube action */

      SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

      if ( EvaluatePositionCubeful ( anBoard, arDouble, &ci, &ecEval,
                                     ecEval.nPlies ) < 0 )
        return;

      GetCubeActionSz ( arDouble, szTemp, &ci );

#if USE_GTK
      /*
      if ( fX ) {

         GTKHint( cube action );

         return;

      }
      */
#endif
      outputl ( szTemp );

      return;
    }

    if ( fDoubled ) {

      /* Give hint on take decision */

      SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

      if ( EvaluatePositionCubeful ( anBoard, arDouble, &ci, &ecEval,
                                     ecEval.nPlies ) < 0 )
        return;

#if USE_GTK
      /*
      if ( fX ) {

        GTKHint( take decision );

        return;

      }
      */
#endif

      outputl ( "Take decision:\n" );

      if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) ) {

        outputf ( "Equity for take: %+6.3f\n", -arDouble[ 2 ] );
        outputf ( "Equity for pass: %+6.3f\n\n", -arDouble[ 3 ] );

      }
      else {
	outputf ( "Mwc for take: %6.2f%%\n", 
		  100.0 * ( 1.0 - eq2mwc ( arDouble[ 2 ], &ci ) ) );
	outputf ( "Mwc for pass: %6.2f%%\n", 
		  100.0 * ( 1.0 - eq2mwc ( arDouble[ 3 ], &ci ) ) );
      }

      if ( arDouble[ 2 ] < 0 && ! nMatchTo && fBeavers )
        outputl ( "Your proper cube action: Beaver!\n" );
      else if ( arDouble[ 2 ] <= arDouble[ 3 ] )
        outputl ( "Your proper cube action: Take.\n" );
      else
        outputl ( "Your proper cube action: Pass.\n" );

      return;

    }

    if ( anDice[ 0 ] ) {

      /* Give hints on move */

      SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

      FindBestMoves( &ml, aar, anDice[ 0 ], anDice[ 1 ], anBoard,
                     ecEval.nSearchCandidates, ecEval.rSearchTolerance,
                     &ci, &ecEval );
    
      if( fInterrupt )
        return;
    
#if USE_GTK
      if( fX ) {
        GTKHint( &ml );
        return;
      }
#endif

      if ( ! nMatchTo || ( nMatchTo && ! fOutputMWC ) ) {

	/* output in equity */
    
	outputl( "Win  \tW(g) \tW(bg)\tL(g) \tL(bg)\tEquity  \tMove" );
    
	for( i = 0; i < ml.cMoves; i++ ) {
	  float *ar = ml.amMoves[ i ].pEval;
	  outputf( "%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t(%+6.3f)\t",
		   ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ], ar[ 4 ],
		   Utility ( ar, &ci ) );
	  outputl( FormatMove( szMove, anBoard, ml.amMoves[ i ].anMove
			       ) );

	}

      }
      else {

	/* output in mwc */

	outputl( "Win  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
		 " Mwc\tMove" );
    
	for( i = 0; i < ml.cMoves; i++ ) {
	  float *ar = ml.amMoves[ i ].pEval;
	  float rMwc = Utility ( ar, &ci );
	  outputf( "%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%6.2f%%\t",
		   ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ], ar[ 4 ],
		   100.0 * eq2mwc ( Utility ( ar, &ci ), &ci ) );
	  outputl( FormatMove( szMove, anBoard, ml.amMoves[ i ].anMove
			       ) );
	}
      }
    }
}

/* Called on various exit commands -- e.g. EOF on stdin, "quit" command,
   etc.  If stdin is not a TTY, this should always exit immediately (to
   avoid enless loops on EOF).  If stdin is a TTY, and fConfirm is set,
   and a game is in progress, then we ask the user if they're sure. */
extern void PromptForExit( void ) {

    static int fExiting;
    
    if( !fExiting && fInteractive && fConfirm && fTurn != -1 ) {
	fExiting = TRUE;
	fInterrupt = FALSE;
	
	if( !GetInputYN( "Are you sure you want to exit and abort the game in "
			 "progress? " ) ) {
	    fInterrupt = FALSE;
	    fExiting = FALSE;
	    return;
	}
    }

    exit( EXIT_SUCCESS );
}

extern void CommandNotImplemented( char *sz ) {

    outputl( "That command is not yet implemented." );
}

extern void CommandQuit( char *sz ) {

    PromptForExit();
}

extern void 
CommandRollout( char *sz ) {
    
  float ar[ NUM_ROLLOUT_OUTPUTS ], arStdDev[ NUM_ROLLOUT_OUTPUTS ];
  int c, an[ 2 ][ 25 ];
  cubeinfo ci;
    
  if( !*sz && fTurn == -1 ) {
    outputl( "No position specified." );
    return;
  }
    
  if( ParsePosition( an, sz ) ) {
    outputl( "Illegal position." );

    return;
  }

  SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

  if( ( c = Rollout( an, ar, arStdDev, nRolloutTruncate, nRollouts,
                     fVarRedn, &ci, &ecRollout ) ) < 0 )
    return;

#if USE_GTK
  if( fX ) {
    GTKRolloutDone();
    return;
  }
#endif
    
  outputf( "Result (after %d trials):\n\n"
           "               \tWin  \tW(g) \tW(bg)\tL(g) \tL(bg)\t"
           "Equity\n"
           "          Mean:\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t"
           "(%+6.3f)\n"
           "Standard error:\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t"
           "(%6.3f)\n\n",
           c, ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ], ar[ 4 ], ar[ 5 ],
           arStdDev[ 0 ], arStdDev[ 1 ], arStdDev[ 2 ], arStdDev[ 3 ],
           arStdDev[ 4 ], arStdDev[ 5 ] );
}

static void SaveGame( FILE *pf, list *plGame, int iGame, int anScore[ 2 ] ) {

    list *pl;
    moverecord *pmr;
    char sz[ 40 ];
    int i = 0, n, nFileCube = 1, anBoard[ 2 ][ 25 ];

    if( iGame >= 0 )
	fprintf( pf, " Game %d\n", iGame + 1 );

    if( anScore ) {
	sprintf( sz, "%s : %d", ap[ 0 ].szName, anScore[ 0 ] );
	fprintf( pf, " %-31s%s : %d\n", sz, ap[ 1 ].szName, anScore[ 1 ] );
    } else
	fprintf( pf, " %-31s%s\n", ap[ 0 ].szName, ap[ 1 ].szName );
    
    InitBoard( anBoard );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    sprintf( sz, "%d%d: ", pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );
	    FormatMovePlain( sz + 4, anBoard, pmr->n.anMove );
	    ApplyMove( anBoard, pmr->n.anMove );
	    SwapSides( anBoard );
	    break;
	case MOVE_DOUBLE:
	    sprintf( sz, " Doubles => %d", nFileCube <<= 1 );
	    break;
	case MOVE_TAKE:
	    strcpy( sz, " Takes" ); /* FIXME beavers? */
	    break;
	case MOVE_DROP:
	    strcpy( sz, " Drops\n" );
	    anScore[ ( i + 1 ) & 1 ] += nFileCube / 2;
	    break;
	case MOVE_RESIGN:
	    /* FIXME how does JF do it? */
	    break;
	}

	if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) {
	    fputs( "  1)                             ", pf );
	    i++;
	}

	if( i & 1 ) {
	    fputs( sz, pf );
	    fputc( '\n', pf );
	} else
	    fprintf( pf, "%3d) %-28s", ( i >> 1 ) + 1, sz );

	if( ( n = GameStatus( anBoard ) ) ) {
	    fprintf( pf, "%sWins %d point%s%s\n\n",
		   i & 1 ? "                                  " : "\n     ",
		   n * nFileCube, n * nFileCube > 1 ? "s" : "",
		   "" /* FIXME " and the match" if appropriate */ );

	    anScore[ i & 1 ] += n * nFileCube;
	}
	
	i++;
    }
}

static void LoadCommands( FILE *pf, char *szFile ) {
    
    char sz[ 2048 ], *pch;

    for(;;) {
	sz[ 0 ] = 0;
	
	/* FIXME shouldn't restart sys calls on signals during this fgets */
	fgets( sz, sizeof( sz ), pf );

	if( ( pch = strchr( sz, '\n' ) ) )
	    *pch = 0;

	if( ferror( pf ) ) {
	    perror( szFile );
	    return;
	}
	
	if( feof( pf ) || fInterrupt )
	    return;

	if( *sz == '#' ) /* Comment */
	    continue;
	
	HandleCommand( sz, acTop );

	/* FIXME handle NextTurn events? */
    }
}

extern void CommandLoadCommands( char *sz ) {

    FILE *pf;

    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "commands')." );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    } else
	perror( sz );
}

static void LoadRCFiles( void ) {

    char sz[ PATH_MAX ], *pch = getenv( "HOME" );
    FILE *pf;

    outputoff();
    
    sprintf( sz, "%s/.gnubgautorc", pch ? pch : "" );

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    }
    
    sprintf( sz, "%s/.gnubgrc", pch ? pch : "" );

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    }

    outputon();
}

extern void CommandSaveGame( char *sz ) {
    
    FILE *pf;
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to save to (see `help save game')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    SaveGame( pf, plGame, -1, NULL );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandSaveMatch( char *sz ) {

    FILE *pf;
    int i, anScore[ 2 ];
    list *pl;

    /* FIXME what should be done if nMatchTo == 0? */
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to save to (see `help save match')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    fprintf( pf, " %d point match\n\n", nMatchTo );

    anScore[ 0 ] = anScore[ 1 ] = 0;
    
    for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext )
	SaveGame( pf, pl->p, i, anScore );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandSaveSettings( char *szParam ) {

    char sz[ PATH_MAX ], *pch = getenv( "HOME" );
    FILE *pf;
    
    sprintf( sz, "%s/.gnubgautorc", pch ? pch : "" ); /* FIXME accept param */

    if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    errno = 0;

    fprintf( pf, "#\n"
	     "# GNU Backgammon command file\n"
	     "#   generated by GNU Backgammon " VERSION "\n"
	     "#\n"
	     "# WARNING: The file `.gnubgautorc' is automatically generated "
	     "by the\n"
	     "# `save settings' command, and will be overwritten the next "
	     "time settings\n"
	     "# are saved.  If you want to add startup commands manually, "
	     "you should\n"
	     "# use `.gnubgrc' instead.\n"
	     "\n"
	     "set automatic bearoff %s\n"
	     "set automatic crawford %s\n"
	     "set automatic game %s\n"
	     "set automatic move %s\n"
	     "set automatic roll %s\n",
	     fAutoBearoff ? "on" : "off",
	     fAutoCrawford ? "on" : "off",
	     fAutoGame ? "on" : "off",
	     fAutoMove ? "on" : "off",
	     fAutoRoll ? "on" : "off" );
    /* FIXME save cache settings */
    fprintf( pf, "set confirm %s\n"
#if USE_GUI
	     "set delay %d\n"
#endif
	     "set display %s\n",
	     fConfirm ? "on" : "off",
#if USE_GUI
	     nDelay,
#endif
	     fDisplay ? "on" : "off" );
    /* FIXME save eval settings */
    fprintf( pf, "set jacoby %s\n"
	     "set nackgammon %s\n",
	     fJacoby ? "on" : "off",
	     fNackgammon ? "on" : "off" );
    /* FIXME save player settings */
    fprintf( pf, "set prompt %s\n", szPrompt );
    /* FIXME save rollout settings */

    fclose( pf );
    
    if( errno )
	perror( sz );
    else
	outputl( "Settings saved." );
}

extern void CommandSaveWeights( char *sz ) {

  if( EvalSave( GNUBG_WEIGHTS /* FIXME accept file name parameter */ ) )
    perror( GNUBG_WEIGHTS );
  else
    outputl( "Evaluator weights saved." );
}

extern void CommandTrainTD( char *sz ) {

  int c = 0, i;
  int anBoardTrain[ 2 ][ 25 ], anBoardOld[ 2 ][ 25 ];
  int anDiceTrain[ 2 ];
  float ar[ NUM_OUTPUTS ];
  cubeinfo ciTD;
    
  while( !fInterrupt ) {
    InitBoard( anBoardTrain );
	
    do {    
	    if( !( ++c % 100 ) && fShowProgress ) {
        outputf( "%6d\r", c );
        fflush( stdout );
	    }
	    
	    RollDice( anDiceTrain );

	    if( fInterrupt )
        return;
	    
	    memcpy( anBoardOld, anBoardTrain, sizeof( anBoardOld ) );

      SetCubeInfo ( &ciTD, 0, 0, 0 );
      for ( i = 0; i < 4; i++ )
        ciTD.arGammonPrice[ i ] = 1.0;
	    
	    FindBestMove( NULL, anDiceTrain[ 0 ], anDiceTrain[ 1 ],
                    anBoardTrain, &ciTD, &ecTD );

	    if( fInterrupt )
        return;
	    
	    SwapSides( anBoardTrain );
	    
	    EvaluatePosition( anBoardTrain, ar, &ciTD, &ecTD );

	    InvertEvaluation( ar );
	    if( TrainPosition( anBoardOld, ar ) )
        break;
	    
      /* FIXME can stop as soon as perfect */
    } while( !fInterrupt && !GameStatus( anBoardTrain ) );
  }
}

#if HAVE_LIBREADLINE
static command *pcCompleteContext;

static char *NullGenerator( char *sz, int nState ) {

  return NULL;
}

static char *GenerateKeywords( char *sz, int nState ) {

    static int cch;
    static command *pc;
    char *szDup;

    if( fReadingOther )
      return NULL;
    
    if( !nState ) {
      cch = strlen( sz );
      pc = pcCompleteContext;
    }

    while( pc && pc->sz ) {
      if( !strncasecmp( sz, pc->sz, cch ) && pc->szHelp ) {
        if( !( szDup = malloc( strlen( pc->sz ) + 1 ) ) )
          return NULL;

        strcpy( szDup, pc->sz );
        
        pc++;
	    
        return szDup;
      }
	
      pc++;
    }
    
    return NULL;
}

static char **CompleteKeyword( char *szText, int iStart, int iEnd ) {

    pcCompleteContext = FindContext( acTop, rl_line_buffer, iStart, FALSE );

    if( !pcCompleteContext )
	return NULL;
    
    return completion_matches( szText, GenerateKeywords );
}
#else
extern void Prompt( void ) {

    if( !fInteractive )
	return;

    output( FormatPrompt() );
    fflush( stdout );    
}
#endif

#if USE_GUI
#if HAVE_LIBREADLINE
static void ProcessInput( char *sz, int fFree ) {
    
    rl_callback_handler_remove();
    fReadingCommand = FALSE;
    
    if( !sz ) {
	outputc( '\n' );
	PromptForExit();
	sz = "";
    }
    
    fInterrupt = FALSE;
    
    if( *sz )
	add_history( sz );
	
    HandleCommand( sz, acTop );

    ResetInterrupt();

    if( fFree )
	free( sz );

    /* Recalculate prompt -- if we call nothing, then readline will
       redisplay the old prompt.  This isn't what we want: we either
       want no prompt at all, yet (if NextTurn is going to be called),
       or if we do want to prompt immediately, we recalculate it in
       case the information in the old one is out of date. */
#if USE_GTK
    if( nNextTurn )
#else
    if( evNextTurn.fPending )
#endif
	fNeedPrompt = TRUE;
    else {
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	fReadingCommand = TRUE;
    }
}

extern void HandleInput( char *sz ) {

    ProcessInput( sz, TRUE );
}

static char *szInput;
static int fInputAgain;

void HandleInputRecursive( char *sz ) {

    if( !sz ) {
	outputc( '\n' );
	PromptForExit();
	szInput = NULL;
	fInputAgain = TRUE;
	return;
    }

    szInput = sz;

    rl_callback_handler_remove();
}
#endif

/* Handle a command as if it had been typed by the user. */
extern void UserCommand( char *szCommand ) {

#if HAVE_LIBREADLINE
    int nOldEnd;
#endif
    int cch = strlen( szCommand ) + 1;
#if __GNUC__
    char sz[ cch ];
#elif HAVE_ALLOCA
    char *sz = alloca( cch );
#else
    char sz[ 1024 ];
    assert( cch <= 1024 );
#endif
    
    /* Unfortunately we need to copy the command, because it might be in
       read-only storage and HandleCommand might want to modify it. */
    strcpy( sz, szCommand );

    /* Note that the command is always echoed to stdout; the output*()
       functions are bypassed. */
#if HAVE_LIBREADLINE
    nOldEnd = rl_end;
    rl_end = 0;
    rl_redisplay();
    puts( sz );
    ProcessInput( sz, FALSE );
    return;
#else
    putchar( '\n' );
    Prompt();
    puts( sz );

    fInterrupt = FALSE;
    
    HandleCommand( sz, acTop );

    ResetInterrupt();
    
#if USE_GTK
    if( nNextTurn )
#else
    if( evNextTurn.fPending )
#endif
	Prompt();
    else
	fNeedPrompt = TRUE;
#endif
}

#if USE_GTK
extern gint NextTurnNotify( gpointer p )
#else
extern int NextTurnNotify( event *pev, void *p )
#endif
{

    NextTurn();

    ResetInterrupt();

#if USE_GTK
    if( fNeedPrompt )
#else
    if( !pev->fPending && fNeedPrompt )
#endif
    {
#if HAVE_LIBREADLINE
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	fReadingCommand = TRUE;
#else
	Prompt();
#endif
	fNeedPrompt = FALSE;
    }
    
    return FALSE; /* remove idle handler, if GTK */
}
#endif

/* Read a line from stdin, and handle X and readline input if
 * appropriate.  This function blocks until a line is ready, and does
 * not call HandleEvents(), and because fBusy will be set some X
 * commands will not work.  Therefore, it should not be used for
 * reading top level commands.  The line it returns has been allocated
 * with malloc (as with readline()). */
extern char *GetInput( char *szPrompt ) {

    /* FIXME handle interrupts and EOF in this function. */
    
    char *sz;
#if !HAVE_LIBREADLINE
    char *pch;
#endif
#if USE_GUI
    fd_set fds;
    
    if( fX ) {
#if HAVE_LIBREADLINE
	/* Using readline and X. */
	char *szOldPrompt, *szOldInput;
	int nOldEnd, nOldMark, nOldPoint, fWasReadingCommand;
	
	if( fInterrupt )
	    return NULL;

	fReadingOther = TRUE;
	
	if( ( fWasReadingCommand = fReadingCommand ) ) {
	    /* Save old readline context. */
	    szOldPrompt = rl_prompt;
	    szOldInput = rl_copy_text( 0, rl_end );
	    nOldEnd = rl_end;
	    nOldMark = rl_mark;
	    nOldPoint = rl_point;
	    fReadingCommand = FALSE;
	    /* FIXME this is unnecessary when handling an X command! */
	    outputc( '\n' );
	}

	szInput = FALSE;

	rl_callback_handler_install( szPrompt, HandleInputRecursive );

	while( !szInput ) {
	    FD_ZERO( &fds );
	    FD_SET( STDIN_FILENO, &fds );
#ifdef ConnectionNumber
	    FD_SET( ConnectionNumber( DISPLAY ), &fds );

	    select( ConnectionNumber( DISPLAY ) + 1, &fds, NULL, NULL,
		    NULL );
#else
	    select( STDIN_FILENO + 1, &fds, NULL, NULL, NULL );
#endif
	    if( fInterrupt ) {
		outputc( '\n' );
		break;
	    }
	    
	    if( FD_ISSET( STDIN_FILENO, &fds ) ) {
		rl_callback_read_char();
		if( fInputAgain ) {
		    rl_callback_handler_install( szPrompt,
						 HandleInputRecursive );
		    szInput = NULL;
		    fInputAgain = FALSE;
		}
	    }
#ifdef ConnectionNumber
	    if( FD_ISSET( ConnectionNumber( DISPLAY ), &fds ) )
		HandleXAction();
#endif
	}

	if( fWasReadingCommand ) {
	    /* Restore old readline context. */
	    rl_callback_handler_install( szOldPrompt, HandleInput );
	    rl_insert_text( szOldInput );
	    free( szOldInput );
	    rl_end = nOldEnd;
	    rl_mark = nOldMark;
	    rl_point = nOldPoint;
	    rl_redisplay();
	    fReadingCommand = TRUE;
	} else
	    rl_callback_handler_remove();	

	fReadingOther = FALSE;
	
	return szInput;
#else
	/* Using X, but not readline. */
	if( fInterrupt )
	    return NULL;

	outputc( '\n' );
	output( szPrompt );
	fflush( stdout );

	do {
	    FD_ZERO( &fds );
	    FD_SET( STDIN_FILENO, &fds );
#ifdef ConnectionNumber
	    FD_SET( ConnectionNumber( DISPLAY ), &fds );

	    select( ConnectionNumber( DISPLAY ) + 1, &fds, NULL, NULL,
		    NULL );
#else
	    select( STDIN_FILENO + 1, &fds, NULL, NULL, NULL );
#endif
	    if( fInterrupt ) {
		outputc( '\n' );
		return NULL;
	    }
	    
#ifdef ConnectionNumber
	    if( FD_ISSET( ConnectionNumber( DISPLAY ), &fds ) )
		HandleXAction();
#endif
	} while( !FD_ISSET( STDIN_FILENO, &fds ) );

	goto ReadDirect;
#endif
    }
#endif
#if HAVE_LIBREADLINE
    /* Using readline, but not X. */
    if( fInterrupt )
	return NULL;

    fReadingOther = TRUE;
    
    while( !( sz = readline( szPrompt ) ) ) {
	outputc( '\n' );
	PromptForExit();
    }

    fReadingOther = FALSE;
    
    if( fInterrupt )
	return NULL;

    return sz;
#else
    /* Not using readline or X. */
    if( fInterrupt )
	return NULL;

    output( szPrompt );
    fflush( stdout );

#if USE_GUI
 ReadDirect:
#endif
    sz = malloc( 256 ); /* FIXME it would be nice to handle longer strings */
    
    fgets( sz, 256, stdin );
    
    if( fInterrupt ) {
	free( sz );
	return NULL;
    }
    
    if( ( pch = strchr( sz, '\n' ) ) )
	*pch = 0;
    
    while( feof( stdin ) && !*sz ) {
	outputc( '\n' );
	PromptForExit();
    }
    
    return sz;
#endif
}

/* Ask a yes/no question.  Interrupting the question is considered a "no"
   answer. */
extern int GetInputYN( char *szPrompt ) {

    char *pch;

#if USE_GTK
    if( fX )
	return GTKGetInputYN( szPrompt );
#endif
    
    if( fInterrupt )
	return FALSE;

    while( 1 ) {
	pch = GetInput( szPrompt );

	if( pch )
	    switch( *pch ) {
	    case 'y':
	    case 'Y':
		free( pch );
		return TRUE;
	    case 'n':
	    case 'N':
		free( pch );
		return FALSE;
	    default:
		free( pch );
	    }

	if( fInterrupt )
	    return FALSE;
	
	outputl( "Please answer `y' or `n'." );
    }
}

/* Write a string to stdout/status bar/popup window */
extern void output( char *sz ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput) {
	GTKOutput( g_strdup( sz ) );
	return;
    }
#endif
    fputs( sz, stdout );
}

/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( char *sz ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput ) {
	int cch;
	char *pch;

	cch = strlen( sz );
	pch = g_malloc( cch + 2 );
	strcpy( pch, sz );
	pch[ cch ] = '\n';
	pch[ cch + 1 ] = 0;
	GTKOutput( pch );
	return;
    }
#endif
    puts( sz );
}
    
/* Write a character to stdout/status bar/popup window */
extern void outputc( char ch ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput ) {
	char *pch;

	pch = g_malloc( 2 );
	*pch = ch;
	pch[ 1 ] = 0;
	GTKOutput( pch );
	return;
    }
#endif
    putchar( ch );
}
    
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( char *sz, ... ) {

    va_list val;

    va_start( val, sz );
    outputv( sz, val );
    va_end( val );
}

/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( char *sz, va_list val ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput ) {
	GTKOutput( g_strdup_vprintf( sz, val ));
	return;
    }
#endif
    vprintf( sz, val );
}

/* Signifies that all output for the current command is complete */
extern void outputx( void ) {
    
    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput )
	GTKOutputX();
#endif
}

/* Signifies that subsequent output is for a new command */
extern void outputnew( void ) {
    
    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX && fGTKOutput )
	GTKOutputNew();
#endif
}

/* Disable output */
extern void outputoff( void ) {

    cOutputDisabled++;
}

/* Enable output */
extern void outputon( void ) {

    assert( cOutputDisabled );

    cOutputDisabled--;
}

static RETSIGTYPE HandleInterrupt( int idSignal ) {

    /* NB: It is safe to write to fInterrupt even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    fInterrupt = TRUE;
}

#if USE_GUI
static RETSIGTYPE HandleIO( int idSignal ) {

    /* NB: It is safe to write to fAction even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    if( fX )
	fAction = TRUE;
}
#endif

static void usage( char *argv0 ) {

    outputf(
"Usage: %s [options]\n"
"Options:\n"
"  -b, --no-bearoff          Do not use bearoff database\n"
"  -d DIR, --datadir DIR     Read database and weight files from direcotry "
"DIR\n"
"  -h, --help                Display usage and exit\n"
"  -n, --no-weights          Do not load existing neural net weights\n"
#if USE_GUI
"  -t, --tty                 Start on tty instead of using X\n"
#endif
"  -v, --version             Show version information and exit\n"
"\n"
"For more information, type `help' from within gnubg.\n"
"Please report bugs to <bug-gnubg@gnu.org>.\n", argv0 );
}

extern int main( int argc, char *argv[] ) {

    char ch, *pch, *pchDataDir = NULL;
    static int fNoWeights = FALSE, fNoBearoff = FALSE;
    static struct option ao[] = {
	{ "datadir", required_argument, NULL, 'd' },
        { "help", no_argument, NULL, 'h' },
	{ "no-bearoff", no_argument, NULL, 'b' },
	{ "no-weights", no_argument, NULL, 'n' },
        { "version", no_argument, NULL, 'v' },
	/* `tty' must be the last option -- see below. */
        { "tty", no_argument, NULL, 't' },
        { NULL, 0, NULL, 0 }
    };
#if HAVE_GETPWUID
    struct passwd *ppwd;
#endif
#if HAVE_LIBREADLINE
    char *sz;
#endif
    
#if USE_GUI
    /* The GTK interface is fairly grotty; it makes it impossible to
       separate argv handling from attempting to open the display, so
       we have to check for -t before the other options to avoid connecting
       to the X server if it is specified.

       We use the last element of ao to get the "--tty" option only. */
    
    opterr = 0;
    
    while( ( ch = getopt_long( argc, argv, "t", ao + sizeof( ao ) /
			       sizeof( ao[ 0 ] ) - 2, NULL ) ) != (char) -1 )
	if( ch == 't' ) {
	    fX = FALSE;
	    break;
	}
    
    optind = 0;
    opterr = 1;

    if( fX )
#if USE_GTK
	fX = gtk_init_check( &argc, &argv );
#else
        if( !getenv( "DISPLAY" ) )
	    fX = FALSE;
#endif
#endif
    
    fInteractive = isatty( STDIN_FILENO );
    fShowProgress = isatty( STDOUT_FILENO );
    
    while( ( ch = getopt_long( argc, argv, "bd:hntv", ao, NULL ) ) !=
           (char) -1 )
	switch( ch ) {
	case 'b': /* no-bearoff */
	    fNoBearoff = TRUE;
	    break;
	case 'd': /* datadir */
	    pchDataDir = optarg;
	    break;
	case 'h': /* help */
            usage( argv[ 0 ] );
            return EXIT_SUCCESS;
	case 'n':
	    fNoWeights = TRUE;
	    break;
	case 't':
	    /* silently ignore (if it was relevant, it was handled earlier). */
	    break;
	case 'v': /* version */
	    outputl( "GNU Backgammon " VERSION );
#if USE_GUI
	    outputl( "X Window System supported." );
#endif
#if HAVE_LIBGDBM
	    outputl( "Position databases supported." );
#endif
	    return EXIT_SUCCESS;
	default:
	    usage( argv[ 0 ] );
	    return EXIT_FAILURE;
	}

    outputl( "GNU Backgammon " VERSION "  Copyright 1999, 2000 Gary Wong.\n"
	  "GNU Backgammon is free software, covered by the GNU "
	  "General Public License\n"
	  "version 2, and you are welcome to change it and/or distribute "
	  "copies of it\n"
	  "under certain conditions.  Type \"show copying\" to see "
	  "the conditions.\n"
	  "There is absolutely no warranty for GNU Backgammon.  "
	  "Type \"show warranty\" for\n"
	  "details." );

    InitRNG();

    InitMatchEquity ();
    
    if( EvalInitialise( fNoWeights ? NULL : GNUBG_WEIGHTS,
			fNoWeights ? NULL : GNUBG_WEIGHTS_BINARY,
			fNoBearoff ? NULL : GNUBG_BEAROFF, pchDataDir ) )
	return EXIT_FAILURE;

    if( ( pch = getenv( "LOGNAME" ) ) )
	strcpy( ap[ 1 ].szName, pch );
    else if( ( pch = getenv( "USER" ) ) )
	strcpy( ap[ 1 ].szName, pch );
#if HAVE_GETLOGIN
    else if( ( pch = getlogin() ) )
	strcpy( ap[ 1 ].szName, pch );
#endif
#if HAVE_GETPWUID
    else if( ( ppwd = getpwuid( getuid() ) ) )
	strcpy( ap[ 1 ].szName, ppwd->pw_name );
#endif
    
    ListCreate( &lMatch );
    
    srandom( time( NULL ) );

    PortableSignal( SIGINT, HandleInterrupt, NULL );
#if USE_GUI
    PortableSignal( SIGIO, HandleIO, NULL );
#endif
    
#if HAVE_LIBREADLINE
    rl_readline_name = "gnubg";
    rl_basic_word_break_characters = szCommandSeparators;
    rl_attempted_completion_function = (CPPFunction *) CompleteKeyword;
    rl_completion_entry_function = (Function *) NullGenerator;
#endif

    /* FIXME if( optind < argc )
       CommandLoad( argv[ optind ] ); */

    LoadRCFiles();
    
#if USE_GTK
    if( fX ) {
	RunGTK();

	return EXIT_SUCCESS;
    }
#elif USE_EXT
    if( fX ) {
        RunExt();

	fputs( "Could not open X display.  Continuing on TTY.\n", stderr );
        fX = FALSE;
    }
#endif
    
    for(;;) {
#if HAVE_LIBREADLINE
	while( !( sz = readline( FormatPrompt() ) ) ) {
	    outputc( '\n' );
	    PromptForExit();
	}
	
	fInterrupt = FALSE;
	
	if( *sz )
	    add_history( sz );
	
	HandleCommand( sz, acTop );
	
	free( sz );
#else
	char sz[ 2048 ], *pch;
	
	sz[ 0 ] = 0;
	
	Prompt();

	/* FIXME shouldn't restart sys calls on signals during this fgets */
	fgets( sz, sizeof( sz ), stdin );

        if( ( pch = strchr( sz, '\n' ) ) )
           *pch = 0;
    
	
	while( feof( stdin ) ) {
	    outputc( '\n' );
	    
	    if( !sz[ 0 ] )
		PromptForExit();

	    continue;
	}	

	fInterrupt = FALSE;
	
	HandleCommand( sz, acTop );
#endif

	while( fNextTurn )
	    NextTurn();

	ResetInterrupt();
    }
}
