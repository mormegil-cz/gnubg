/*
 * play.c
 *
 * by Gary Wong, 1999
 *
 */

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "positionid.h"

char *aszGameResult[] = { "single game", "gammon", "backgammon" };
enum _gameover { GAME_NORMAL, GAME_RESIGNED, GAME_DROP } go;
list lMatch, *plGame;

static void NewGame( void ) {

    InitBoard( anBoard );

    plGame = malloc( sizeof( *plGame ) );
    ListCreate( plGame );

    ListInsert( &lMatch, plGame );
    
    fResigned = fDoubled = FALSE;
    nCube = 1;
    fCubeOwner = -1;
    go = GAME_NORMAL;
    
    do {
	RollDice( anDice );
	if( fDisplay )
	    printf( "%s rolls %d, %s rolls %d.\n", ap[ 0 ].szName, anDice[ 0 ],
		    ap[ 1 ].szName, anDice[ 1 ] );
    } while( anDice[ 0 ] == anDice[ 1 ] );

    fMove = fTurn = anDice[ 1 ] > anDice[ 0 ];
}

static int ComputerTurn( void ) {

    movenormal *pmn;
    
    if( !fResigned && !anDice[ 0 ] ) {
	RollDice( anDice );

	if( fDisplay )
	    ShowBoard();
    }
    
    switch( ap[ fTurn ].pt ) {
    case PLAYER_GNU:
	/* FIXME if on roll, consider doubling/resigning */
	if( fResigned ) {
	    float ar[ NUM_OUTPUTS ];

	    EvaluatePosition( anBoard, ar, ap[ fTurn ].nPlies );

	    if( -fResigned <= ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
		ar[ OUTPUT_WINGAMMON ] + ar[ OUTPUT_WINBACKGAMMON ] -
		ar[ OUTPUT_LOSEGAMMON ] - ar[ OUTPUT_LOSEBACKGAMMON ] ) {
		CommandAgree( NULL );
		return 0;
	    } else {
		CommandDecline( NULL );
		return 0;
	    }
	} else if( fDoubled ) {
	    /* FIXME consider cube action */
	    CommandTake( NULL );
	    return 0;
	} else {
	    pmn = malloc( sizeof( *pmn ) );
	    pmn->mt = MOVE_NORMAL;
	    pmn->anRoll[ 0 ] = anDice[ 0 ];
	    pmn->anRoll[ 1 ] = anDice[ 1 ];
	    pmn->fPlayer = fTurn;
	    ListInsert( plGame, pmn );
	    return FindBestMove( ap[ fTurn ].nPlies, pmn->anMove, anDice[ 0 ],
				 anDice[ 1 ], anBoard );
	}

    case PLAYER_PUBEVAL:
	if( fResigned == 3 ) {
	    CommandAgree( NULL );
	    return 0;
	} else if( fResigned ) {
	    CommandDecline( NULL );
	    return 0;
	} else if( fDoubled ) {
	    CommandTake( NULL );
	    return 0;
	}
	/* FIXME save move */
	return FindPubevalMove( anDice[ 0 ], anDice[ 1 ], anBoard );

    default:
	assert( FALSE );
    }
}

/* Try to automatically bear off as many chequers as possible.  Only do it
   if it's a non-contact position, and each die can be used to bear off
   a chequer. */
static int TryBearoff( void ) {

    movelist ml;
    int i, iMove, cMoves;
    movenormal *pmn;
    
    if( ClassifyPosition( anBoard ) > CLASS_RACE )
	/* It's a contact position; don't automatically bear off */
	return -1;
    
    GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ] );

    cMoves = ( anDice[ 0 ] == anDice[ 1 ] ) ? 4 : 2;
    
    for( i = 0; i < ml.cMoves; i++ )
	for( iMove = 0; iMove < cMoves; iMove++ )
	    if( ( ml.amMoves[ i ].anMove[ iMove << 1 ] < 0 ) ||
		( ml.amMoves[ i ].anMove[ ( iMove << 1 ) + 1 ] != -1 ) )
		break;
	    else if( iMove == cMoves - 1 ) {
		/* All dice bear off */
		pmn = malloc( sizeof( *pmn ) );
		pmn->mt = MOVE_NORMAL;
		pmn->anRoll[ 0 ] = anDice[ 0 ];
		pmn->anRoll[ 1 ] = anDice[ 1 ];
		pmn->fPlayer = fTurn;
		memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
			sizeof( pmn->anMove ) );
		ListInsert( plGame, pmn );
		
		PositionFromKey( anBoard, ml.amMoves[ i ].auch );
		
		return 0;
	    }

    return -1;
}

static void NextTurn( void ) {

    int n, fWinner;
    static int fReentered = 0, fShouldRecurse = 0;

    if( fReentered ) {
	fShouldRecurse++;
	return;
    }

    fReentered++;
    
    for(;;) {
	fShouldRecurse = 0;
	fTurn = !fTurn;
	
	if( go == GAME_NORMAL && !fResigned && !fDoubled && fTurn != fMove ) {
	    fMove = !fMove;
	    anDice[ 0 ] = anDice[ 1 ] = 0;
	    
	    SwapSides( anBoard );
	}

	if( go == GAME_NORMAL && ( fDisplay ||
				   ap[ fTurn ].pt == PLAYER_HUMAN ) )
	    ShowBoard();

	if( ( n = GameStatus( anBoard ) ) ||
	    ( go == GAME_DROP && ( n = 1 ) ) ||
	    ( go == GAME_RESIGNED && ( n = fResigned ) ) ) {
	    fWinner = !fTurn;
	    
	    anScore[ fWinner ] += n * nCube;
	    cGames++;

	    fTurn = fMove = -1;
	    anDice[ 0 ] = anDice[ 1 ] = 0;
	    
	    go = GAME_NORMAL;
	    
	    printf( "%s wins a %s and %d point%s.\n", ap[ fWinner ].szName,
		    aszGameResult[ n - 1 ], n * nCube,
		    n * nCube > 1 ? "s" : "" );
	    
	    if( nMatchTo && fAutoCrawford ) {
		fCrawford = anScore[ fWinner ] == nMatchTo - 1 &&
		    anScore[ !fWinner ] < nMatchTo - 1;
		fPostCrawford = anScore[ !fWinner ] == nMatchTo - 1;
	    }

	    CommandShowScore( NULL );

	    if( nMatchTo && anScore[ fWinner ] >= nMatchTo ) {
		printf( "%s has won the match.\n", ap[ fWinner ].szName );
		break;
	    }

	    if( fAutoGame ) {
		NewGame();

		if( ap[ fTurn ].pt == PLAYER_HUMAN )
		    ShowBoard();
	    } else
		break;
	}

	if( fTurn == fMove )
	    fResigned = 0;

	if( fInterrupt )
	    break;

	if( fAutoRoll && fCubeOwner >= 0 && fCubeOwner != fTurn &&
	    !anDice[ 0 ] ) {
	    CommandRoll( NULL );

	    if( fShouldRecurse )
		continue;
	}
	
	if( ap[ fTurn ].pt == PLAYER_HUMAN )
	    break;
	
	if( ComputerTurn() < 0 )
	    break;
    }

    fReentered = 0;
}

extern void CommandAccept( char *sz ) {

    if( fResigned )
	CommandAgree( sz );
    else if( fDoubled )
	CommandTake( sz );
    else
	puts( "You can only accept if the cube or a resignation has been "
	      "offered." );
}

extern void CommandAgree( char *sz ) {

    moveresign *pmr;
    
    if( !fResigned ) {
	puts( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	printf( "%s accepts and wins a %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    go = GAME_RESIGNED;

    pmr = malloc( sizeof( *pmr ) );
    pmr->mt = MOVE_RESIGN;
    pmr->fPlayer = !fTurn;
    pmr->nResigned = fResigned;
    ListInsert( plGame, pmr );
    
    NextTurn();
}

extern void CommandDecline( char *sz ) {

    if( !fResigned ) {
	puts( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	printf( "%s declines the %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    NextTurn();
}

extern void CommandDouble( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }

    if( fDoubled ) {
	puts( "The `double' command is for offering the cube, not accepting "
	      "it.  Use\n`redouble' to immediately offer the cube back at a "
	      "higher value." );

	return;
    }
    
    if( fTurn != fMove ) {
	puts( "You are only allowed to double if you are on roll." );

	return;
    }
    
    if( anDice[ 0 ] ) {
	puts( "You can't double after rolling the dice -- wait until your "
	      "next turn." );

	return;
    }

    if( fCubeOwner >= 0 && fCubeOwner != fTurn ) {
	puts( "You do not own the cube." );

	return;
    }
    
    fDoubled = 1;

    if( fDisplay )
	printf( "%s doubles.\n", ap[ fTurn ].szName );
    
    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DOUBLE;
    ListInsert( plGame, pmt );
    
    NextTurn();
}

extern void CommandDrop( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 || !fDoubled ) {
	puts( "The cube must have been offered before you can drop it." );

	return;
    }

    if( fDisplay )
	printf( "%s refuses the cube and gives up %d points.\n",
		ap[ fTurn ].szName, nCube );
    
    fDoubled = FALSE;

    go = GAME_DROP;

    fTurn = !fTurn;

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DROP;
    ListInsert( plGame, pmt );
    
    NextTurn();
}

extern void CommandMove( char *sz ) {

    int c, i, j, anBoardNew[ 2 ][ 25 ], anBoardTest[ 2 ][ 25 ],
	an[ 8 ], fError = FALSE, fDone = FALSE;
    char *pch;
    movelist ml;
    movenormal *pmn;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }

    if( !anDice[ 0 ] ) {
	puts( "You must roll the dice before you can move." );

	return;
    }

    if( fResigned ) {
	printf( "Please wait for %s to consider the resignation before "
		"moving.\n", ap[ fTurn ].szName );

	return;
    }

    if( fDoubled ) {
	printf( "Please wait for %s to consider the cube before "
		"moving.\n", ap[ fTurn ].szName );

	return;
    }
    
    if( !*sz ) {
	GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ] );

	if( ml.cMoves == 1 ) {
	    pmn = malloc( sizeof( *pmn ) );
	    pmn->mt = MOVE_NORMAL;
	    pmn->anRoll[ 0 ] = anDice[ 0 ];
	    pmn->anRoll[ 1 ] = anDice[ 1 ];
	    pmn->fPlayer = fTurn;
	    memcpy( pmn->anMove, ml.amMoves[ 0 ].anMove,
		    sizeof( pmn->anMove ) );
	    ListInsert( plGame, pmn );
	    
	    PositionFromKey( anBoard, ml.amMoves[ 0 ].auch );

	    NextTurn();

	    return;
	}

	if( fAutoBearoff && !TryBearoff() ) {
	    NextTurn();

	    return;
	}
	
	puts( "You must specify a move (type `help move' for instructions)." );

	return;
    }
    
    /* FIXME allow abbreviating multiple moves (e.g. "8/7(2) 6/5(2)") */
	
    for( i = 0; *sz && i < 8; i++ ) {
	pch = sz;

	while( *sz && !isspace( *sz ) && *sz != '/' && *sz != '*' )
	    sz++;

	if( !*sz )
	    fDone = TRUE;
	else
	    *sz++ = 0;

	if( *pch == 'b' || *pch == 'B' )
	    an[ i ] = 24;
	else
	    an[ i ] = atoi( pch ) - 1;

	if( an[ i ] > 24 || an[ i ] < -1 ||
	    ( an[ i ] < 0 && !( i & 1 ) ) ) {
	    fError = TRUE;
	    break;
	}
	
	if( fDone ) {
	    i++;
	    break;
	}

	while( *sz && ( isspace( *sz ) || *sz == '/' || *sz == '*' ) )
	    sz++;
    }

    if( !fError && !( i & 1 ) && ( i != 8 || !*sz ) ) {
	c = i >> 1;

	for( i = 0; i < 25; i++ ) {
	    anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	    anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}
	
	for( i = 0; i < c; i++ ) {
	    anBoardNew[ 1 ][ an[ i << 1 ] ]--;
	    if( an[ ( i << 1 ) | 1 ] >= 0 ) {
		anBoardNew[ 1 ][ an[ ( i << 1 ) | 1 ] ]++;

		anBoardNew[ 0 ][ 24 ] +=
		    anBoardNew[ 0 ][ 23 - an[ ( i << 1 ) | 1 ] ];
	    
		anBoardNew[ 0 ][ 23 - an[ ( i << 1 ) | 1 ] ] = 0;
	    }
	}

	GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ] );

	for( i = 0; i < ml.cMoves; i++ ) {
	    PositionFromKey( anBoardTest, ml.amMoves[ i ].auch );

	    for( j = 0; j < 25; j++ )
		if( anBoardTest[ 0 ][ j ] != anBoardNew[ 0 ][ j ] ||
		    anBoardTest[ 1 ][ j ] != anBoardNew[ 1 ][ j ] )
		    break;

	    if( j == 25 ) {
		/* we have a legal move! */
		pmn = malloc( sizeof( *pmn ) );
		pmn->mt = MOVE_NORMAL;
		pmn->anRoll[ 0 ] = anDice[ 0 ];
		pmn->anRoll[ 1 ] = anDice[ 1 ];
		pmn->fPlayer = fTurn;
		memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
			sizeof( pmn->anMove ) );
		ListInsert( plGame, pmn );
		
		memcpy( anBoard, anBoardNew, sizeof( anBoard ) );

		NextTurn();
		
		return;
	    }
	}
    }

    puts( "Illegal move." );
}

static void FreeGame( list *pl ) {

    while( pl->plNext != pl ) {
	free( pl->plNext->p );
	ListDelete( pl->plNext );
    }

    free( pl );
}

extern void CommandNewGame( char *sz ) {

    if( nMatchTo && ( anScore[ 0 ] >= nMatchTo ||
		      anScore[ 1 ] >= nMatchTo ) ) {
	puts( "The match is already over." );

	return;
    }

    if( fTurn != -1 ) {
	/* FIXME ask the user if they're sure?  Perhaps we need a "set confirm"
	   command to let the user be paranoid about shooting themselves in
	   the foot. */

	/* The last game of the match should always be the current one. */
	assert( lMatch.plPrev->p == plGame );

	ListDelete( lMatch.plPrev );
	
	FreeGame( plGame );
    }
    
    NewGame();
    
    if( ap[ fTurn ].pt == PLAYER_HUMAN )
	ShowBoard();
    else {
	if( ComputerTurn() < 0 )
	    return;
	
	NextTurn();
    }
}

static void FreeMatch( void ) {

    list *plMatch;

    while( ( plMatch = lMatch.plNext ) != &lMatch ) {
	FreeGame( plMatch->p );
	ListDelete( plMatch );
    }
}

extern void CommandNewMatch( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 1 ) {
	puts( "You must specify a valid match length (1 or longer)." );

	return;
    }

    FreeMatch();

    nMatchTo = n;

    anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fJacoby = 0;
    fCrawford = 0;
    fPostCrawford = 0;

    printf( "A new %d point match has been started.\n", n );

    return;
}

extern void CommandNewSession( char *sz ) {

    FreeMatch();

    nMatchTo = anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fJacoby = 0;
    fCrawford = 0;
    fPostCrawford = 0;

    puts( "A new session has been started." );
}

extern void CommandPlay( char *sz ) {

    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }

    if( ap[ fTurn ].pt == PLAYER_HUMAN ) {
	puts( "It's not the computer's turn to play." );

	return;
    }
    
    if( ComputerTurn() >= 0 )
	NextTurn();
}

extern void CommandRedouble( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 || !fDoubled ) {
	puts( "The cube must have been offered before you can redouble it." );

	return;
    }
    
    nCube <<= 1;

    if( fDisplay )
	printf( "%s accepts and immediately redoubles to %d.\n",
		ap[ fTurn ].szName, nCube << 1 );
    
    fCubeOwner = !fMove;

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DOUBLE;
    ListInsert( plGame, pmt );
    
    NextTurn();
}

extern void CommandReject( char *sz ) {

    if( fResigned )
	CommandDecline( sz );
    else if( fDoubled )
	CommandDrop( sz );
    else
	puts( "You can only reject if the cube or a resignation has been "
	      "offered." );
}

extern void CommandResign( char *sz ) {

    char *pch;
    int cch;
    
    if( fTurn < 0 ) {
	puts( "You must be playing a game if you want to resign it." );

	return;
    }

    if( ( pch = NextToken( &sz ) ) ) {
	cch = strlen( pch );

	if( !strncasecmp( "single", pch, cch ) ||
	    !strncasecmp( "normal", pch, cch ) )
	    fResigned = 1;
	else if( !strncasecmp( "gammon", pch, cch ) )
	    fResigned = 2;
	else if( !strncasecmp( "backgammon", pch, cch ) )
	    fResigned = 3;
	else
	    fResigned = atoi( pch );
    } else
	fResigned = 1;

    if( fResigned < 1 || fResigned > 3 ) {
	fResigned = 0;

	printf( "Unknown keyword `%s' -- try `help resign'.\n", pch );
	
	return;
    }
    
    if( fDisplay )
	printf( "%s offers to resign a %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    NextTurn();
}

extern void CommandRoll( char *sz ) {

    movelist ml;
    movenormal *pmn;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }
    
    if( fDoubled ) {
	printf( "Please wait for %s to consider the cube before "
		"moving.\n", ap[ fTurn ].szName );

	return;
    }    

    if( fResigned ) {
	puts( "Please resolve the resignation first." );

	return;
    }

    if( anDice[ 0 ] ) {
	puts( "You already did roll the dice." );

	return;
    }
    
    RollDice( anDice );

    ShowBoard();

    if( !GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ] ) ) {
	pmn = malloc( sizeof( *pmn ) );
	pmn->mt = MOVE_NORMAL;
	pmn->anRoll[ 0 ] = anDice[ 0 ];
	pmn->anRoll[ 1 ] = anDice[ 1 ];
	pmn->fPlayer = fTurn;
	pmn->anMove[ 0 ] = -1;
	ListInsert( plGame, pmn );
	
	puts( "No legal moves." );

	NextTurn();
    } else if( ml.cMoves == 1 && ( fAutoMove || ( ClassifyPosition( anBoard )
						  <= CLASS_BEAROFF1 &&
						  fAutoBearoff ) ) ) {
	pmn = malloc( sizeof( *pmn ) );
	pmn->mt = MOVE_NORMAL;
	pmn->anRoll[ 0 ] = anDice[ 0 ];
	pmn->anRoll[ 1 ] = anDice[ 1 ];
	pmn->fPlayer = fTurn;
	memcpy( pmn->anMove, ml.amMoves[ 0 ].anMove, sizeof( pmn->anMove ) );
	ListInsert( plGame, pmn );
	
	PositionFromKey( anBoard, ml.amMoves[ 0 ].auch );
	
	NextTurn();
    } else if( fAutoBearoff && !TryBearoff() )
	NextTurn();
}

extern void CommandTake( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 || !fDoubled ) {
	puts( "The cube must have been offered before you can take it." );

	return;
    }

    nCube <<= 1;

    if( fDisplay )
	printf( "%s accepts the cube at %d.\n", ap[ fTurn ].szName, nCube );
    
    fDoubled = FALSE;

    fTurn = fCubeOwner = !fMove;

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_TAKE;
    ListInsert( plGame, pmt );
    
    NextTurn();
}
