#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"

static command acSetPlayer[] = {
    { "gnubg", CommandSetPlayerGNU, "Have gnubg make all moves for a player",
      NULL },
    { "human", CommandSetPlayerHuman, "Have a human make all moves for a "
      "player", NULL },
    { "name", CommandSetPlayerName, "Change a player's name", NULL },
    { "plies", CommandSetPlayerPlies, "Set the lookahead depth when gnubg "
      "plays", NULL },
    { "pubeval", CommandSetPlayerPubeval, "Have pubeval make all moves for a "
      "player", NULL },
    { NULL, NULL, NULL, NULL }
};

static void SetRNG( rng rngNew, char *szSeed ) {

    static char *aszRNG[] = {
	"ANSI", "BSD", "ISAAC", "manual", "Mersenne Twister"
    };
    
    if( rngCurrent == rngNew ) {
	printf( "You are already using the %s generator.\n",
		aszRNG[ rngNew ] );
	if( szSeed )
	    CommandSetSeed( szSeed );
    } else {
	printf( "GNU Backgammon will now use the %s generator.\n",
		aszRNG[ rngNew ] );
	rngCurrent = rngNew;
	CommandSetSeed( szSeed );
    }
}

extern void CommandSetAutoGame( char *sz ) {

    SetToggle( "autogame", &fAutoGame, sz, "Will automatically start games "
	       "after wins.", "Will not automatically start games." );
}

extern void CommandSetAutoMove( char *sz ) {

    SetToggle( "automove", &fAutoMove, sz, "Forced moves will be made "
	       "automatically.", "Forced moves will not be made "
	       "automatically." );
}

extern void CommandSetAutoRoll( char *sz ) {

    SetToggle( "autoroll", &fAutoRoll, sz, "Will automatically roll the "
	       "dice when no cube action is possible.", "Will not "
	       "automatically roll the dice." );
}

extern void CommandSetBoard( char *sz ) {

    if( fTurn < 0 ) {
	puts( "There must be a game in progress to set the board." );

	return;
    }

    if( !*sz ) {
	puts( "You must specify a position -- see `help set board'." );

	return;
    }

    ParsePosition( anBoard, sz );

    /* FIXME check errors */

    ShowBoard();
}

extern void CommandSetDice( char *sz ) {

    int n0, n1;
    
    if( fTurn < 0 ) {
	puts( "There must be a game in progress to set the dice." );

	return;
    }

    if( ( n0 = ParseNumber( &sz ) ) < 1 || n0 > 6 ||
	( n1 = ParseNumber( &sz ) ) < 1 || n1 > 6 ) {
	puts( "You must specify two numbers from 1 to 6 for the dice." );

	return;
    }

    printf( "The dice have been set to %d and %d.\n", anDice[ 0 ] = n0,
	    anDice[ 1 ] = n1 );
}

extern void CommandSetDisplay( char *sz ) {

    SetToggle( "display", &fDisplay, sz, "Will display boards for computer "
	       "moves.", "Will not display boards for computer moves." );
}

static int iPlayerSet;

extern void CommandSetPlayerGNU( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_GNU;

    printf( "Moves for %s will now be played by GNU Backgammon.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerHuman( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_HUMAN;

    printf( "Moves for %s must now be entered manually.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerName( char *sz ) {

    char *pch = NextToken( &sz );
    
    if( !pch ) {
	puts( "You must specify a name to use." );

	return;
    }

    if( ( *pch == '0' || *pch == '1' ) && !pch[ 1 ] ) {
	printf( "`%c' is not a legal name.\n", *pch );

	return;
    }

    if( !strncasecmp( pch, "both", strlen( pch ) ) ) {
	puts( "`both' is a reserved word; you can't call a player that.\n" );

	return;
    }

    if( !strncasecmp( pch, ap[ !iPlayerSet ].szName,
		      sizeof( ap[ 0 ].szName ) ) ) {
	puts( "That name is already in use by the other player." );

	return;
    }

    if( strlen( pch ) > 31 )
	pch[ 31 ] = 0;
    
    strcpy( ap[ iPlayerSet ].szName, pch );

    printf( "Player %d is now knwon as `%s'.\n", iPlayerSet, pch );
}

extern void CommandSetPlayerPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	puts( "You must specify a vaid ply depth to look ahead -- try `help "
	      "set player plies'." );

	return;
    }

    ap[ iPlayerSet ].nPlies = n;
    
    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	printf( "Moves for %s will be played with %d ply lookahead (note that "
		"this will\nhave no effect yet, because gnubg is not moving "
		"for this player).\n", ap[ iPlayerSet ].szName, n );
    else
	printf( "Moves for %s will be played with %d ply lookahead.\n",
		ap[ iPlayerSet ].szName, n );
}

extern void CommandSetPlayerPubeval( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_PUBEVAL;

    printf( "Moves for %s will now be played by pubeval.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayer( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	puts( "You must specify a player -- try `help set player'." );
	
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerSet = i;

	HandleCommand( sz, acSetPlayer );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    puts( "Insufficient memory." );
		
	    return;
	}

	strcpy( pchCopy, sz );

	iPlayerSet = 0;
	HandleCommand( sz, acSetPlayer );

	iPlayerSet = 1;
	HandleCommand( pchCopy, acSetPlayer );

	free( pchCopy );
	
	return;
    }
    
    printf( "Unknown player `%s' -- try `help set player'.\n", pch );
}

extern void CommandSetPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	puts( "You must specify a valid number of plies to look ahead -- try "
	      "`help set plies'." );

	return;
    }

    nPliesEval = n;

    printf( "`eval' and `hint' will use %d ply evaluation.\n", nPliesEval );
}

extern void CommandSetRNGAnsi( char *sz ) {

    SetRNG( RNG_ANSI, sz );
}

extern void CommandSetRNGBsd( char *sz ) {
#if HAVE_RANDOM
    SetRNG( RNG_BSD, sz );
#else
    puts( "This installation of GNU Backgammon was compiled without the" );
    puts( "BSD generator." );
#endif
}

extern void CommandSetRNGIsaac( char *sz ) {

    SetRNG( RNG_ISAAC, sz );
}

extern void CommandSetRNGManual( char *sz ) {

    CommandNotImplemented( sz );
}

extern void CommandSetRNGMersenne( char *sz ) {

    SetRNG( RNG_MERSENNE, sz );
}

extern void CommandSetSeed( char *sz ) {

    /* FIXME allow setting very long seeds */
    int n;
    
    if( rngCurrent == RNG_MANUAL ) {
	puts( "You can't set a seed if you're using manual dice generation." );
	return;
    }

    if( *sz ) {
	n = ParseNumber( &sz );

	if( n < 0 ) {
	    puts( "You must specify a vaid seed -- try `help set seed'." );

	    return;
	}

	InitRNGSeed( n );
	printf( "Seed set to %d.\n", n );
    } else {
	InitRNG();
	puts( "Seed initialised by system clock." );
    }
}

extern void CommandSetTurn( char *sz ) {

    char *pch = NextToken( &sz );
    int i;

    if( fResigned ) {
	puts( "Please resolve the resignation first." );

	return;
    }
    
    if( !pch ) {
	puts( "Which player do you want to set on roll?" );

	return;
    }

    if( ( i = ParsePlayer( pch ) ) < 0 ) {
	printf( "Unknown player `%s' -- try `help set turn'.\n", pch );

	return;
    }

    if( i == 2 ) {
	puts( "You can't set both players on roll." );

	return;
    }

    if( fTurn != i )
	SwapSides( anBoard );
    
    fTurn = fMove = i;
    fDoubled = 0;

    anDice[ 0 ] = anDice[ 1 ] = 0;

    printf( "`%s' is now on roll.\n", ap[ i ].szName );
}
