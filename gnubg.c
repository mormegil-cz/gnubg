/*
 * gnubg.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
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
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
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
#endif

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "getopt.h"
#include "positionid.h"
#include "rollout.h"

#if !X_DISPLAY_MISSING
#include <ext.h>
#include <extwin.h>
#include <stdio.h>
#include "xgame.h"

static extdisplay edsp;
extwindow ewnd;
int fX = TRUE; /* use X display */
int nDelay = 0;
event evNextTurn;
static int fNeedPrompt = FALSE;
#if HAVE_LIBREADLINE
static int fReadingCommand, fReadingOther;
#endif
#endif

char szDefaultPrompt[] = "(\\p) ",
    *szPrompt = szDefaultPrompt;
static int fInteractive;

int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn = -1, fDisplay = TRUE,
    fAutoBearoff = FALSE, fAutoGame = TRUE, fAutoMove = FALSE,
    fResigned = FALSE, fMove = -1, nPliesEval = 1, anScore[ 2 ] = { 0, 0 },
    cGames = 0, fDoubled = FALSE, nCube = 1, fCubeOwner = -1,
    fAutoRoll = TRUE, nMatchTo = 0, fJacoby = FALSE, fCrawford = FALSE,
    fPostCrawford = FALSE, fAutoCrawford = TRUE, cAutoDoubles = 1,
    fCubeUse = TRUE, fNackgammon = FALSE, fVarRedn = FALSE,
    nRollouts = 1296, nRolloutTruncate = 7, fNextTurn = FALSE,
    fConfirm = TRUE, fShowProgress;

evalcontext ecTD = { 0, 8, 0.16 }, ecEval = { 1, 8, 0.16 },
    ecRollout = { 0, 8, 0.16 };

player ap[ 2 ] = {
    { "gnubg", PLAYER_GNU, { 0, 8, 0.16 } },
    { "user", PLAYER_HUMAN, { 0, 8, 0.16 } }
};

static command acDatabase[] = {
    { "dump", CommandDatabaseDump, "List the positions in the database",
      NULL },
    { "rollout", CommandDatabaseRollout, "Evaluate positions in database "
      "for future training", NULL },
    { "generate", CommandDatabaseGenerate, "Generate database positions by "
      "self-play", NULL },
    { "train", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL },
    { NULL, NULL, NULL, NULL }
}, acNew[] = {
    { "game", CommandNewGame, "Start a new game within the current match or "
      "session", NULL },
    { "match", CommandNewMatch, "Play a new match to some number of points",
      NULL },
    { "session", CommandNewSession, "Start a new (money) session", NULL },
    { NULL, NULL, NULL, NULL }
}, acSave[] = {
    { "game", CommandNotImplemented, "Record a log of the game so far to a "
      "file", NULL },
    { "match", CommandSaveMatch, "Record a log of the match so far to a file",
      NULL },
    { "weights", CommandSaveWeights, "Write the neural net weights to a file",
      NULL },
    { NULL, NULL, NULL, NULL }
}, acSetAutomatic[] = {
    { "bearoff", CommandSetAutoBearoff, "Automatically bear off as many "
      "chequers as possible", NULL },
    { "crawford", CommandSetAutoCrawford, "Enable the Crawford game "
      "based on match score", NULL },
    { "doubles", CommandSetAutoDoubles, "Control automatic doubles "
      "during (money) session play", NULL },
    { "game", CommandSetAutoGame, "Select whether to start new games "
      "after wins", NULL },
    { "move", CommandSetAutoMove, "Select whether forced moves will be "
      "made automatically", NULL },
    { "roll", CommandSetAutoRoll, "Control whether dice will be rolled "
      "automatically", NULL },
    { NULL, NULL, NULL, NULL }
}, acSetCube[] = {
    { "center", CommandSetCubeCentre, "The U.S.A. spelling of `centre'",
      NULL },
    { "centre", CommandSetCubeCentre, "Allow both players access to the "
      "cube", NULL },
    { "owner", CommandSetCubeOwner, "Allow only one player to double",
      NULL },
    { "use", CommandSetCubeUse, "Enable use of the doubling cube", NULL },
    { "value", CommandSetCubeValue, "Fix what the cube has been set to",
      NULL },
    { NULL, NULL, NULL, NULL }
}, acSetRNG[] = {
    { "ansi", CommandSetRNGAnsi, "Use the ANSI C rand() (usually linear "
      "congruential) generator", NULL },
    { "bsd", CommandSetRNGBsd, "Use the BSD random() non-linear additive "
      "feedback generator", NULL },
    { "isaac", CommandSetRNGIsaac, "Use the I.S.A.A.C. generator", NULL },
    { "manual", CommandSetRNGManual, "Enter all dice rolls manually", NULL },
    { "mersenne", CommandSetRNGMersenne, "Use the Mersenne Twister generator",
      NULL },
    { "user", CommandSetRNGUser, "Specify an external generator", NULL },
    { NULL, NULL, NULL, NULL }
}, acSetRollout[] = {
    { "evaluation", CommandSetRolloutEvaluation, "Specify parameters "
      "for evaluation during rollouts", NULL },
    { "seed", CommandNotImplemented, "Specify the base pseudo-random seed "
      "to use for rollouts", NULL },
    { "trials", CommandSetRolloutTrials, "Control how many rollouts to "
      "perform", NULL },
    { "truncation", CommandSetRolloutTruncation, "End rollouts at a "
      "particular depth", NULL },
    { "varredn", CommandSetRolloutVarRedn, "Use lookahead during rollouts "
      "to reduce variance", NULL },
    /* FIXME add commands for cubeful rollouts, cube variance reduction,
       quasi-random dice, settlements... */
    { NULL, NULL, NULL, NULL }
}, acSet[] = {
    { "automatic", NULL, "Perform certain functions without user input",
      acSetAutomatic },
    { "board", CommandSetBoard, "Set up the board in a particular "
      "position", NULL },
    { "cache", CommandSetCache, "Set the size of the evaluation cache", NULL },
    { "confirm", CommandSetConfirm, "Ask for confirmation before aborting "
      "a game in progress", NULL },
    { "crawford", CommandSetCrawford, 
      "Set whether this is the Crawford game", NULL },
    { "cube", NULL, "Set the cube owner and/or value", acSetCube },
    { "delay", CommandSetDelay, "Limit the speed at which moves are made",
      NULL },
    { "dice", CommandSetDice, "Select the roll for the current move",
      NULL },
    { "display", CommandSetDisplay, "Select whether the board is updated on "
      "the computer's turn", NULL },
    { "evaluation", CommandSetEvaluation, "Control position evaluation "
      "parameters", NULL },
    { "jacoby", CommandSetJacoby, "Set whether to use the Jacoby rule in"
      "money game", NULL },
    { "nackgammon", CommandSetNackgammon, "Set the starting position", NULL },
    { "player", CommandSetPlayer, "Change options for one or both "
      "players", NULL },
    { "postcrawford", CommandSetPostCrawford, 
      "Set whether this is post-Crawford games", NULL },
    { "prompt", CommandSetPrompt, "Customise the prompt gnubg prints when "
      "ready for commands", NULL },
    { "rng", NULL, "Select the random number generator algorithm", acSetRNG },
    { "rollout", NULL, "Control rollout parameters", acSetRollout },
    { "score", CommandSetScore, "Set the match or session score ",
      NULL },
    { "seed", CommandSetSeed, "Set the dice generator seed", NULL },
    { "turn", CommandSetTurn, "Set which player is on roll", NULL },
    { NULL, NULL, NULL, NULL }
}, acShow[] = {
    { "automatic", CommandShowAutomatic, "List which functions will be "
      "performed without user input", NULL },
    { "board", CommandShowBoard, "Redisplay the board position", NULL },
    { "cache", CommandShowCache, "Display statistics on the evaluation "
      "cache", NULL },
    { "copying", CommandShowCopying, "Conditions for redistributing copies "
      "of GNU Backgammon", NULL },
    /* FIXME show cube */
    { "crawford", CommandShowCrawford, 
      "See if this is the Crawford game", NULL },
    /* FIXME show delay */
    { "dice", CommandShowDice, "See what the current dice roll is", NULL },
    /* FIXME show display */
    { "evaluation", CommandShowEvaluation, "Display evaluation settings "
      "and statistics", NULL },
    { "jacoby", CommandShowJacoby, 
      "See if the Jacoby rule is used in money sessions", NULL },
    { "pipcount", CommandShowPipCount, "Count the number of pips each player "
      "must move to bear off", NULL },
    { "player", CommandShowPlayer, "View per-player options", NULL },
    { "postcrawford", CommandShowCrawford, 
      "See if this is post-Crawford play", NULL },
    { "rng", CommandShowRNG, "Display which random number generator "
      "is being used", NULL },
    { "rollout", CommandShowRollout, "Display the evaluation settings used "
      "during rollouts", NULL },
    { "score", CommandShowScore, "View the match or session score ",
      NULL },
    { "turn", CommandShowTurn, "Show which player is on roll", NULL },
    { "warranty", CommandShowWarranty, "Various kinds of warranty you do "
      "not have", NULL },
    { NULL, NULL, NULL, NULL }    
}, acTrain[] = {
    { "database", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL },
    { "td", CommandTrainTD, "Train the network by TD(0) zero-knowledge "
      "self-play", NULL },
    { NULL, NULL, NULL, NULL }
}, acTop[] = {
    { "accept", CommandAccept, "Accept a cube or resignation",
      NULL },
    { "agree", CommandAgree, "Agree to a resignation", NULL },
    { "beaver", CommandRedouble, "Synonym for `redouble'", NULL },
    { "database", NULL, "Manipulate a database of positions", acDatabase },
    { "decline", CommandDecline, "Decline a resignation", NULL },
    { "double", CommandDouble, "Offer a double", NULL },
    { "drop", CommandDrop, "Decline an offered double", NULL },
    { "eval", CommandEval, "Display evaluation of a position", NULL },
    { "exit", CommandQuit, "Leave GNU Backgammon", NULL },
    { "help", CommandHelp, "Describe commands", NULL },
    { "hint", CommandHint, "Show the best evaluations of legal "
      "moves", NULL },
    { "load", CommandNotImplemented, "Read data from a file", NULL },
    { "move", CommandMove, "Make a backgammon move", NULL },
    { "new", NULL, "Start a new game", acNew },
    { "play", CommandPlay, "Force the computer to move", NULL },
    { "quit", CommandQuit, "Leave GNU Backgammon", NULL },
    { "r", CommandRoll, NULL, NULL },
    { "redouble", CommandRedouble, "Accept the cube one level higher "
      "than it was offered", NULL },
    { "reject", CommandReject, "Reject a cube or resignation", NULL },
    { "resign", CommandResign, "Offer to end the current game", NULL },
    { "roll", CommandRoll, "Roll the dice", NULL },
    { "rollout", CommandRollout, "Have gnubg perform rollouts of a position",
      NULL },
    { "save", NULL, "Write data to a file", acSave },
    { "set", NULL, "Modify program parameters", acSet },
    { "show", NULL, "View program parameters", acShow },
    { "take", CommandTake, "Agree to an offered double", NULL },
    { "train", NULL, "Update gnubg's weights from "
      "training data", acTrain },
    { "?", CommandHelp, "Describe commands", NULL },
    { NULL, NULL, NULL, NULL }
};

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

    return PositionFromID( an, sz );
}

extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff ) {

    char *pch = NextToken( &sz );
    int cch;
    
    if( !pch ) {
	printf( "You must specify whether to set %s on or off (see `help set "
		"%s').\n", szName, szName );

	return -1;
    }

    cch = strlen( pch );
    
    if( !strcasecmp( "on", pch ) || !strncasecmp( "yes", pch, cch ) ||
	!strncasecmp( "true", pch, cch ) ) {
	*pf = TRUE;

	puts( szOn );

	return TRUE;
    }

    if( !strcasecmp( "off", pch ) || !strncasecmp( "no", pch, cch ) ||
	!strncasecmp( "false", pch, cch ) ) {
	*pf = FALSE;

	puts( szOff );

	return FALSE;
    }

    printf( "Illegal keyword `%s' -- try `help set %s'.\n", pch, szName );

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

extern void HandleCommand( char *sz, command *ac ) {

    command *pc;
    char *pch, *pchShell;
    int cch;
    pid_t pid;
    
    if( ac == acTop ) {
	if( *sz == '!' ) {
	    /* Shell escape */
	    for( pch = sz + 1; isspace( *pch ); pch++ )
		;

	    if( *pch ) {
		/* Command specified; system() is more portable than
		   fork/exec */
		/* FIXME it would be nice to handle X events while waiting
		   for the child, but then system() wouldn't be good enough. */
		system( pch );
		return;
	    } else if( ( pid = fork() ) < 0 ) {
		/* Error */
		perror( "fork" );
		return;
	    } else if( pid ) {
		/* Parent */
		/* FIXME it would be nice to handle X events while waiting
		   for the child. */
		waitpid( pid, NULL, 0 );
		return;
	    } else {
		/* Child */
		if( !( pchShell = getenv( "SHELL" ) ) )
		    pchShell = "/bin/sh";
		execl( pchShell, pchShell, NULL );
		_exit( EXIT_FAILURE );
	    }
	} else if( *sz == ':' ) {
	    /* Guile escape */
	    puts( "This installation of GNU Backgammon was compiled without "
		  "Guile support." );
	    return;
	}
    }
    
    if( !( pch = NextToken( &sz ) ) ) {
	if( ac != acTop )
	    puts( "Incomplete command -- try `help'." );

	return;
    }

    cch = strlen( pch );

    if( ac == acTop && ( isdigit( *pch ) ||
			 !strncasecmp( pch, "bar/", cch > 4 ? 4 : cch ) ) ) {
	if( pch + cch < sz )
	    pch[ cch ] = ' ';
	
	CommandMove( pch );
	
	return;
    }

    for( pc = ac; pc->sz; pc++ )
	if( !strncasecmp( pch, pc->sz, cch ) )
	    break;

    if( !pc->sz ) {
	printf( "Unknown keyword `%s' -- try `help'.\n", pch );
	
	return;
    }

    if( pc->pf )
	pc->pf( sz );
    else
	HandleCommand( sz, pc->pc );
}

/* Reset the SIGINT handler, on return to the main command loop.  Notify
   the user if processing had been interrupted. */
static void ResetInterrupt( void ) {
    
    if( fInterrupt ) {
	puts( "(Interrupted)" );

	fInterrupt = FALSE;
#if !X_DISPLAY_MISSING
	EventPending( &evNextTurn, FALSE );
#endif
    }
    
#if HAVE_SIGPROCMASK
    {
	sigset_t ss;

	sigemptyset( &ss );
	sigaddset( &ss, SIGINT );
	sigprocmask( SIG_UNBLOCK, &ss, NULL );
    }
#elif HAVE_SIGBLOCK
    sigsetmask( 0 );
#endif
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

    if( fTurn == -1 ) {
#if !X_DISPLAY_MISSING
	if( fX ) {
	    InitBoard( anBoard );
	    GameSet( &ewnd, anBoard, 0, ap[ 1 ].szName, ap[ 0 ].szName,
		     nMatchTo, anScore[ 1 ], anScore[ 0 ], -1, -1 );
	} else
#endif
	    puts( "No game in progress." );
	
	return;
    }
    
#if !X_DISPLAY_MISSING
    if( !fX ) {
#endif
	if( fDoubled ) {
	    apch[ fTurn ? 5 : 1 ] = szCube;

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
	
	puts( DrawBoard( szBoard, anBoard, fMove, apch ) );
	
	if( !fMove )
	    SwapSides( anBoard );
#if !X_DISPLAY_MISSING
    } else {
	if( !fMove )
	    SwapSides( anBoard );
    
	GameSet( &ewnd, anBoard, fMove, ap[ 1 ].szName, ap[ 0 ].szName,
		 nMatchTo, anScore[ 1 ], anScore[ 0 ], anDice[ 0 ],
		 anDice[ 1 ] );
	
	if( !fMove )
	    SwapSides( anBoard );

	XFlush( ewnd.pdsp );
    }    
#endif    
}

static char *FormatPrompt( void ) {

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
	puts( "No position specified and no game in progress." );
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	puts( "Illegal position." );

	return;
    }

    if( !DumpPosition( an, szOutput, &ecEval ) )
	puts( szOutput );
}

extern void CommandHelp( char *sz ) {

    command *pc;
    char *pch;

    /* FIXME show help for each command (not just those with subcommands) */
    
    for( pch = sz + strlen( sz ) - 1; pch >= sz && isspace( *pch ); pch-- )
	*pch = 0;

    if( !( pc = FindContext( acTop, sz, pch - sz + 1, TRUE ) ) ) {
	printf( "No help available for topic `%s' -- try `help' for a list of "
		"topics.\n", sz );

	return;
    }
    
    for( ; pc->sz; pc++ )
	if( pc->szHelp )
	    printf( "%-15s\t%s\n", pc->sz, pc->szHelp );
}

extern void CommandHint( char *sz ) {

    movelist ml;
    int i;
    char szMove[ 32 ];
    float aar[ 32 ][ NUM_OUTPUTS ];
    
    if( fTurn < 0 ) {
	puts( "You must set up a board first." );

	return;
    }

    if( !anDice[ 0 ] ) {
	puts( "You must roll (or set) the dice first." );

	return;
    }

    FindBestMoves( &ml, aar, anDice[ 0 ], anDice[ 1 ], anBoard,
		   ecEval.nSearchCandidates, ecEval.rSearchTolerance,
		   &ecEval );

    if( fInterrupt )
	return;
    
    puts( "Win  \tW(g) \tW(bg)\tL(g) \tL(bg)\tEquity  \tMove" );
    
    for( i = 0; i < ml.cMoves; i++ ) {
	float *ar = ml.amMoves[ i ].pEval;
	printf( "%5.3f\t%5.3f\t%5.3f\t%5.3f\t%5.3f\t(%+6.3f)\t",
		ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ], ar[ 4 ],
		ar[ 0 ] * 2.0 + ar[ 1 ] + ar[ 2 ] - ar[ 3 ] - ar[ 4 ] - 1.0 );
	puts( FormatMove( szMove, anBoard, ml.amMoves[ i ].anMove ) );
    }
}

/* Called on various exit commands -- e.g. EOF on stdin, "quit" command,
   etc.  If stdin is not a TTY, this should always exit immediately (to
   avoid enless loops on EOF).  If stdin is a TTY, and fConfirm is set,
   and a game is in progress, then we ask the user if they're sure. */
static void PromptForExit( void ) {

    static int fExiting;
    char *sz;
    
    while( !fExiting && fInteractive && fConfirm && fTurn != -1 ) {
	fExiting = TRUE;

	sz = GetInput( "Are you sure you want to exit and abort the game in "
		       "progress? " );

	fExiting = FALSE;
	
	if( sz ) {
	    if( *sz == 'y' || *sz == 'Y' )
		break;
	    else if( *sz == 'n' || *sz == 'N' ) {
		fInterrupt = FALSE;
		return;
	    }
	}

	puts( "Please answer `y' or `n'." );
    }

    exit( EXIT_SUCCESS );
}

extern void CommandNotImplemented( char *sz ) {

    puts( "That command is not yet implemented." );
}

extern void CommandQuit( char *sz ) {

    PromptForExit();
}

extern void CommandRollout( char *sz ) {
    
    float ar[ NUM_ROLLOUT_OUTPUTS ], arStdDev[ NUM_ROLLOUT_OUTPUTS ];
    int c, an[ 2 ][ 25 ];
    
    if( !*sz && fTurn == -1 ) {
	puts( "No position specified." );
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	puts( "Illegal position." );

	return;
    }

    if( ( c = Rollout( an, ar, arStdDev, nRolloutTruncate, nRollouts,
		       fVarRedn, &ecRollout ) ) < 0 )
	return;

    printf( "Result (after %d trials):\n\n"
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
    int i = 0, n, nCube = 1, anBoard[ 2 ][ 25 ];

    fprintf( pf, " Game %d\n", iGame + 1 );
    
    sprintf( sz, "%s : %d", ap[ 0 ].szName, anScore[ 0 ] );
    fprintf( pf, " %-33s%s : %d\n", sz, ap[ 1 ].szName, anScore[ 1 ] );

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
	    sprintf( sz, " Doubles => %d", nCube <<= 1 );
	    break;
	case MOVE_TAKE:
	    strcpy( sz, " Takes" ); /* FIXME beavers? */
	    break;
	case MOVE_DROP:
	    strcpy( sz, " Drops" );
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
		   n * nCube, n * nCube > 1 ? "s" : "",
		   "" /* FIXME " and the match" if appropriate */ );

	    anScore[ i & 1 ] += n * nCube;
	}
	
	i++;
    }
}

extern void CommandSaveMatch( char *sz ) {

    FILE *pf;
    int i, anScore[ 2 ];
    list *pl;
    
    if( !sz || !*sz ) {
	puts( "You must specify a file to save to (see `help save match')." );
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

extern void CommandSaveWeights( char *sz ) {

    if( EvalSave( GNUBG_WEIGHTS /* FIXME accept file name parameter */ ) )
	perror( GNUBG_WEIGHTS );
    else
	puts( "Evaluator weights saved." );
}

extern void CommandTrainTD( char *sz ) {

    int c = 0, anBoardTrain[ 2 ][ 25 ], anBoardOld[ 2 ][ 25 ],
	anDiceTrain[ 2 ];
    float ar[ NUM_OUTPUTS ];
    
    while( !fInterrupt ) {
	InitBoard( anBoardTrain );
	
	do {    
	    if( !( ++c % 100 ) && fShowProgress ) {
		printf( "%6d\r", c );
		fflush( stdout );
	    }
	    
	    RollDice( anDiceTrain );

	    if( fInterrupt )
		return;
	    
	    memcpy( anBoardOld, anBoardTrain, sizeof( anBoardOld ) );
	    
	    FindBestMove( NULL, anDiceTrain[ 0 ], anDiceTrain[ 1 ],
			  anBoardTrain, &ecTD );

	    if( fInterrupt )
		return;
	    
	    SwapSides( anBoardTrain );
	    
	    EvaluatePosition( anBoardTrain, ar, &ecTD );

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
static void Prompt( void ) {

    if( !fInteractive )
	return;

    fputs( FormatPrompt(), stdout );
    fflush( stdout );    
}
#endif

#if !X_DISPLAY_MISSING
#if HAVE_LIBREADLINE
static void HandleInput( char *sz );

static void ProcessInput( char *sz, int fFree ) {
    
    rl_callback_handler_remove();
    fReadingCommand = FALSE;
    
    if( !sz ) {
	putchar( '\n' );
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
    if( evNextTurn.fPending )
	fNeedPrompt = TRUE;
    else {
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	fReadingCommand = TRUE;
    }
}

static void HandleInput( char *sz ) {

    ProcessInput( sz, TRUE );
}

static char *szInput;
static int fInputAgain;

void HandleInputRecursive( char *sz ) {

    if( !sz ) {
	putchar( '\n' );
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
extern void UserCommand( char *sz ) {

#if HAVE_LIBREADLINE
    int nOldEnd;
    
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
    
    if( !evNextTurn.fPending )
	Prompt();
    else
	fNeedPrompt = TRUE;
#endif
}

int StdinReadNotify( event *pev, void *p ) {
#if HAVE_LIBREADLINE
    rl_callback_read_char();
#else
    char sz[ 2048 ];

    sz[ 0 ] = 0;
	
    fgets( sz, sizeof( sz ), stdin );
	
    if( feof( stdin ) ) {
	PromptForExit();
	return 0;
    }	

    fInterrupt = FALSE;

    HandleCommand( sz, acTop );

    ResetInterrupt();

    if( evNextTurn.fPending )
	fNeedPrompt = TRUE;
    else
	Prompt();
#endif

    EventPending( pev, FALSE ); /* FIXME is this necessary? */

    return 0;
}

int NextTurnNotify( event *pev, void *p ) {

    EventPending( pev, FALSE );

    /* FIXME should we really abort now?  If we've gotten this far, it might
       be too late... */
#if 1
    if( fInterrupt ) {
	puts( "(Interrupted)" );

	fInterrupt = FALSE;
    } else
#endif
	NextTurn();

    ResetInterrupt();
    
    if( !pev->fPending && fNeedPrompt ) {
#if HAVE_LIBREADLINE
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	fReadingCommand = TRUE;
#else
	Prompt();
#endif
	fNeedPrompt = FALSE;
    }
    
    return 0;
}

static eventhandler StdinReadHandler = {
    StdinReadNotify, NULL
}, NextTurnHandler = {
    NextTurnNotify, NULL
};

static void HandleXAction( void ) {

    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    /* Set flag so that the board window knows this is a re-entrant
       call, and won't allow commands like roll, move or double. */
    fBusy = TRUE;

    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( XEventsQueued( edsp.pdsp, QueuedAfterReading ) )
	EventProcess( &edsp.ev );

    /* Now we need to commit (the timeout will call ExtDspCommit). */
    EventTimeout( &edsp.ev );

    fBusy = FALSE;
}

void RunX( void ) {
    /* Attempt to execute under X Window System.  Returns on error (for
       fallback to TTY), or executes until exit() if successful. */
    Display *pdsp;
    XrmDatabase rdb;
    XSizeHints xsh;
    char *pch;
    event ev;
    int n;
    
    XrmInitialize();
    
    if( InitEvents() )
	return;
    
    if( InitExt() )
	return;

    /* FIXME allow command line options */
    if( !( pdsp = XOpenDisplay( NULL ) ) )
	return;

    if( ExtDspCreate( &edsp, pdsp ) ) {
	XCloseDisplay( pdsp );
	return;
    }

    /* FIXME check if XResourceManagerString works! */
    if( !( pch = XResourceManagerString( pdsp ) ) )
	pch = "";

    rdb = XrmGetStringDatabase( pch );

    /* FIXME override with $XENVIRONMENT and ~/.gnubgrc */

    /* FIXME get colourmap here; specify it for the new window */
    
    ExtWndCreate( &ewnd, NULL, "game", &ewcGame, rdb, NULL, NULL );
    ExtWndRealise( &ewnd, pdsp, DefaultRootWindow( pdsp ),
                   "540x480+100+100", None, 0 );

    ShowBoard();

    /* FIXME all this should be done in Ext somehow */
    XStoreName( pdsp, ewnd.wnd, "GNU Backgammon" );
    XSetIconName( pdsp, ewnd.wnd, "GNU Backgammon" );
    xsh.flags = PMinSize | PAspect;
    xsh.min_width = 124;
    xsh.min_height = 132;
    xsh.min_aspect.x = 108;
    xsh.min_aspect.y = 102;
    xsh.max_aspect.x = 162;
    xsh.max_aspect.y = 102;
    XSetWMNormalHints( pdsp, ewnd.wnd, &xsh );

    XMapRaised( pdsp, ewnd.wnd );

    EventCreate( &ev, &StdinReadHandler, NULL );
    ev.h = STDIN_FILENO;
    ev.fWrite = FALSE;
    EventHandlerReady( &ev, TRUE, -1 );

    EventCreate( &evNextTurn, &NextTurnHandler, NULL );
    evNextTurn.h = -1;
    EventHandlerReady( &evNextTurn, TRUE, -1 );

    /* FIXME F_SETOWN is a BSDism... use SIOCSPGRP if necessary. */
    fnAction = HandleXAction;
    if( ( n = fcntl( ConnectionNumber( pdsp ), F_GETFL ) ) != -1 ) {
	fcntl( ConnectionNumber( pdsp ), F_SETOWN, getpid() );
	fcntl( ConnectionNumber( pdsp ), F_SETFL, n | FASYNC );
    }
    
#if HAVE_LIBREADLINE
    fReadingCommand = TRUE;
    rl_callback_handler_install( FormatPrompt(), HandleInput );
    atexit( rl_callback_handler_remove );
#else
    Prompt();
#endif
    
    HandleEvents();

    /* Should never return. */
    
    abort();
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
#if !X_DISPLAY_MISSING
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
	    putchar( '\n' );
	}

	szInput = FALSE;

	rl_callback_handler_install( szPrompt, HandleInputRecursive );

	while( !szInput ) {
	    FD_ZERO( &fds );
	    FD_SET( STDIN_FILENO, &fds );
	    FD_SET( ConnectionNumber( ewnd.pdsp ), &fds );

	    select( ConnectionNumber( ewnd.pdsp ) + 1, &fds, NULL, NULL,
		    NULL );

	    if( fInterrupt ) {
		putchar( '\n' );
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

	    if( FD_ISSET( ConnectionNumber( ewnd.pdsp ), &fds ) )
		HandleXAction();
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

	putchar( '\n' );
	fputs( szPrompt, stdout );
	fflush( stdout );

	do {
	    FD_ZERO( &fds );
	    FD_SET( STDIN_FILENO, &fds );
	    FD_SET( ConnectionNumber( ewnd.pdsp ), &fds );

	    select( ConnectionNumber( ewnd.pdsp ) + 1, &fds, NULL, NULL,
		    NULL );

	    if( fInterrupt ) {
		putchar( '\n' );
		return NULL;
	    }
	    
	    if( FD_ISSET( ConnectionNumber( ewnd.pdsp ), &fds ) )
		HandleXAction();
	} while( !FD_ISSET( STDIN_FILENO, &fds ) );

	goto ReadDirect;
#endif
    }
#endif
#if HAVE_LIBREADLINE
    /* Using readline, but not X. */
    if( fInterrupt )
	return NULL;

    while( !( sz = readline( szPrompt ) ) ) {
	putchar( '\n' );
	PromptForExit();
    }
    
    if( fInterrupt )
	return NULL;

    return sz;
#else
    /* Not using readline or X. */
    if( fInterrupt )
	return NULL;

    fputs( szPrompt, stdout );
    fflush( stdout );

 ReadDirect:
    sz = malloc( 256 ); /* FIXME it would be nice to handle longer strings */
    
    fgets( sz, 256, stdin );
    
    if( fInterrupt ) {
	free( sz );
	return NULL;
    }
    
    if( ( pch = strchr( sz, '\n' ) ) )
	*pch = 0;
    
    while( feof( stdin ) && !*sz ) {
	putchar( '\n' );
	PromptForExit();
    }
    
    return sz;
#endif
}

static RETSIGTYPE HandleInterrupt( int idSignal ) {

    /* NB: It is safe to write to fInterrupt even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    fInterrupt = TRUE;
}

#if !X_DISPLAY_MISSING
static RETSIGTYPE HandleIO( int idSignal ) {

    /* NB: It is safe to write to fAction even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    if( fX )
	fAction = TRUE;
}
#endif

static void usage( char *argv0 ) {

    printf(
"Usage: %s [options]\n"
"Options:\n"
"  -h, --help                Display usage and exit\n"
"  -n, --no-weights          Do not load existing neural net weights\n"
#if !X_DISPLAY_MISSING
"  -t, --tty                 Start on tty instead of using X\n"
#endif
"  -v, --version             Show version information and exit\n"
"\n"
"For more information, type `help' from within gnubg.\n"
"Please report bugs to <bug-gnubg@gnu.org>.\n", argv0 );
}

extern int main( int argc, char *argv[] ) {

    char ch, *pch;
    static int fNoWeights = FALSE;
    static struct option ao[] = {
        { "help", no_argument, NULL, 'h' },
	{ "no-weights", no_argument, NULL, 'n' },
        { "tty", no_argument, NULL, 't' },
        { "version", no_argument, NULL, 'v' },
        { NULL, 0, NULL, 0 }
    };
#if HAVE_GETPWUID
    struct passwd *ppwd;
#endif
#if HAVE_LIBREADLINE
    char *sz;
#endif
    
    fInteractive = isatty( STDIN_FILENO );
    fShowProgress = isatty( STDOUT_FILENO );
    
    while( ( ch = getopt_long( argc, argv, "hntv", ao, NULL ) ) !=
           (char) -1 )
	switch( ch ) {
	case 'h': /* help */
            usage( argv[ 0 ] );
            return EXIT_SUCCESS;
	case 'n':
	    fNoWeights = TRUE;
	    break;
	case 't': /* tty */
#if !X_DISPLAY_MISSING
	    fX = FALSE;
#else
	    /* Silently ignore */
#endif
	    break;
	case 'v': /* version */
	    puts( "GNU Backgammon " VERSION );
#if !X_DISPLAY_MISSING
	    puts( "X Window System supported." );
#endif
#if HAVE_LIBGDBM
	    puts( "Position databases supported." );
#endif
	    return EXIT_SUCCESS;
	default:
	    usage( argv[ 0 ] );
	    return EXIT_FAILURE;
	}

    puts( "GNU Backgammon " VERSION "  Copyright 1999 Gary Wong.\n"
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
    
    if( EvalInitialise( fNoWeights ? NULL : GNUBG_WEIGHTS,
			fNoWeights ? NULL : GNUBG_WEIGHTS_BINARY,
			GNUBG_BEAROFF ) )
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

#if HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_handler = HandleInterrupt;
	sigemptyset( &sa.sa_mask );
# if SA_RESTART
	sa.sa_flags = SA_RESTART;
# else
	sa.sa_flags = 0;
# endif
	sigaction( SIGINT, &sa, NULL );

# if !X_DISPLAY_MISSING
	sa.sa_handler = HandleIO;
	sigaction( SIGIO, &sa, NULL );
# endif
    }
#elif HAVE_SIGVEC
    {
	struct sigvec sv;

	sv.sv_handler = HandleInterrupt;
	sigemptyset( &sv.sv_mask );
	sv.sv_flags = 0;

	sigvec( SIGINT, &sv, NULL );
# if !X_DISPLAY_MISSING
	sv.sv_handler = HandleIO;
	sigaction( SIGIO, &sv, NULL );
# endif
    }
#else
    signal( SIGINT, HandleInterrupt );
# if !X_DISPLAY_MISSING
    signal( SIGIO, HandleIO );
# endif
#endif

#if HAVE_LIBREADLINE
    rl_readline_name = "gnubg";
    rl_basic_word_break_characters = szCommandSeparators;
    rl_attempted_completion_function = (CPPFunction *) CompleteKeyword;
    rl_completion_entry_function = (Function *) NullGenerator;
#endif
    
#if !X_DISPLAY_MISSING
    if( fX ) {
	RunX();

	fputs( "Could not open X display.  Continuing on TTY.\n", stderr );
	fX = FALSE;
    }
#endif
    
    for(;;) {
#if HAVE_LIBREADLINE
	if( !( sz = readline( FormatPrompt() ) ) ) {
	    putchar( '\n' );
	    return EXIT_SUCCESS;
	}
	
	fInterrupt = FALSE;
	
	if( *sz )
	    add_history( sz );
	
	HandleCommand( sz, acTop );
	
	free( sz );
#else
	char sz[ 2048 ];
	
	sz[ 0 ] = 0;
	
	Prompt();

	/* FIXME shouldn't restart sys calls on signals during this fgets */
	fgets( sz, sizeof( sz ), stdin );
	
	if( feof( stdin ) ) {
	    putchar( '\n' );
	    
	    if( !sz[ 0 ] )
		return EXIT_SUCCESS;

	    continue;
	}	

	fInterrupt = FALSE;
	
	HandleCommand( sz, acTop );
#endif

	while( !fInterrupt && fNextTurn )
	    NextTurn();

	ResetInterrupt();
    }
}
