/*
 * play.c
 *
 * by Gary Wong, 1999-2000
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "positionid.h"

#include <sys/time.h>
#include <unistd.h>

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
    
 reroll:
    RollDice( anDice );

    if( fInterrupt ) {
	free( plGame );
	ListDelete( lMatch.plPrev );
	fMove = fTurn = -1;
	return;
    }
    
    if( fDisplay )
	printf( "%s rolls %d, %s rolls %d.\n", ap[ 0 ].szName, anDice[ 0 ],
		ap[ 1 ].szName, anDice[ 1 ] );

    if( anDice[ 0 ] == anDice[ 1 ] && nCube < MAX_CUBE ) {
	if( !nMatchTo && nCube < ( 1 << cAutoDoubles ) && fCubeUse )
	    printf( "The cube is now at %d.\n", nCube <<= 1 );

	goto reroll;
    }

#if !X_DISPLAY_MISSING
	if( fX && fDisplay )
	    ShowBoard();
#endif
	
    fMove = fTurn = anDice[ 1 ] > anDice[ 0 ];
    CalcGammonPrice ( nCube, fCubeOwner );
}


extern void TurnDone( void ) {

#if !X_DISPLAY_MISSING
    if( fX )
        EventPending( &evNextTurn, TRUE );
    else
#endif
        fNextTurn = TRUE;
}


static int ComputerTurn( void ) {

    movenormal *pmn;
    float arDouble[ 4 ];

    int i;
    struct timeval tv0;
    struct timeval tv1;
    
    switch( ap[ fTurn ].pt ) {
    case PLAYER_GNU:
	if( fResigned ) {
	    float ar[ NUM_OUTPUTS ];

	    /* FIXME: use EvaluatePositionCubeful instead */

	    if( EvaluatePosition( anBoard, ar, &ap[ fTurn ].ec ) )
		return -1;

	    if( -fResigned <= Utility ( ar ) ) {
		CommandAgree( NULL );
		return 0;
	    } else {
		CommandDecline( NULL );
		return 0;
	    }
	} else if( fDoubled ) {
	  /* consider cube action */
	  
	  gettimeofday ( &tv0, NULL );
	  if ( EvaluatePositionCubeful( anBoard, nCube, fCubeOwner, 
					fMove, arDouble, &ap [ fTurn ].ec,
					ap [ fTurn ].ec.nPlies ) < 0 ) 
	    return -1;
	  gettimeofday ( &tv1, NULL );
	  printf ("Time for EvaluatePositionCubeful: %10.6f seconds\n",
		  1.0 * tv1.tv_sec + 0.000001 * tv1.tv_usec -
		  (1.0 * tv0.tv_sec + 0.000001 * tv0.tv_usec ) ) ;
	  printf ( "equity(new code) for take decision"
		   " %+6.3f %+6.3f %+6.3f\n",  
		   arDouble[ 0 ], arDouble[ 1 ], arDouble[ 2 ]);
	  
	  if ( ( arDouble[ 2 ] < 0.0 ) && ( ! nMatchTo ) ) {

	    /* FIXME: proper code should evaluate position 
	       with opponent owning cube */

	    CommandRedouble ( NULL );
	  }
	  else if ( arDouble[ 2 ] <= 1.0 )
	    CommandTake( NULL );
	  else
	    CommandReject ( NULL );

	    return 0;
	} else {
	  
	  int anBoardMove[ 2 ][ 25 ];

	  /* Don't use the global board for this call, to avoid
	     race conditions with updating the board and aborting the
	     move with an interrupt. */
	  memcpy( anBoardMove, anBoard, sizeof( anBoardMove ) );

	  /* consider doubling */
	  
	  if ( ( ( fCubeOwner == fTurn ) || ( fCubeOwner < 0 ) )
	       && ( ! fCrawford ) && ( fCubeUse ) && ( ! anDice[0] ) ) {

	    gettimeofday ( &tv0, NULL );

	    if ( EvaluatePositionCubeful( anBoardMove, nCube,
					  fCubeOwner, 
					  fMove, arDouble, 
					  &ap [ fTurn ].ec,
					  ap [ fTurn ].ec.nPlies ) 
		 < 0 )  
	      return -1;
	    gettimeofday ( &tv1, NULL );
	    printf ("Time for EvaluatePositionCubeful: %10.6f seconds\n",
		    1.0 * tv1.tv_sec + 0.000001 * tv1.tv_usec -
		    (1.0 * tv0.tv_sec + 0.000001 * tv0.tv_usec ) ) ;
	    printf ( "equity(new code) for double decision"
		     " %+6.3f %+6.3f %+6.3f\n",  
		     arDouble[ 0 ], arDouble[ 1 ], arDouble[ 2 ]);

	    if ( ( arDouble[ 1 ] <= 1.0 ) && 
		 ( arDouble[ 2 ] >= arDouble[ 1 ] ) ) {
	      CommandDouble ( NULL );
	      return 0;
	    }

	  }

	  /* roll dice and move */
	  
	  if ( ! anDice[ 0 ] )
	    RollDice( anDice );

	  if( fDisplay )
	    ShowBoard();

	  pmn = malloc( sizeof( *pmn ) );
	  pmn->mt = MOVE_NORMAL;
	  pmn->anRoll[ 0 ] = anDice[ 0 ];
	  pmn->anRoll[ 1 ] = anDice[ 1 ];
	  pmn->fPlayer = fTurn;
	  ListInsert( plGame, pmn );

	  gettimeofday ( &tv0, NULL );
	  if( FindBestMove( pmn->anMove, anDice[ 0 ], anDice[ 1 ],
			    anBoardMove, &ap[ fTurn ].ec ) < 0 )
	    return -1;
	  gettimeofday ( &tv1, NULL );

	  printf ("Time for FindBestMove: %10.6f seconds\n",
		  1.0 * tv1.tv_sec + 0.000001 * tv1.tv_usec -
		  (1.0 * tv0.tv_sec + 0.000001 * tv0.tv_usec ) ) ;
	  
	  /* The move has been determined without an interrupt.  It's
	     too late to go back now; go ahead and update the board, and
	     block SIGINTs, to make sure that the rest of the move
	     processing proceeds atomically.  On POSIX systems, use
	     sigprocmask() to do it; on older BSD systems, sigblock()
	     will have to do.  If their system is broken and doesn't
	     have either of these, we can't do anything to avoid the
	     race condition. */
	  
#if HAVE_SIGPROCMASK
	  {
	    sigset_t ss;
	    
	    sigemptyset( &ss );
	    sigaddset( &ss, SIGINT );
	    sigprocmask( SIG_BLOCK, &ss, NULL );
	  }
#elif HAVE_SIGBLOCK
	  sigblock( sigmask( SIGINT ) );
#endif
	  fInterrupt = FALSE;
	  
	  memcpy( anBoard, anBoardMove, sizeof( anBoardMove ) );
	  
	  return 0;
	  
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
	
	if ( ! anDice[ 0 ] )
	  RollDice( anDice );

	if( fDisplay )
	  ShowBoard();

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
    
    GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );

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

extern void NextTurn( void ) {

    int n, fWinner;
#if !X_DISPLAY_MISSING
    static struct timeval tvLast, tv;
#endif
    
#if !X_DISPLAY_MISSING
    if( fX )
	EventPending( &evNextTurn, FALSE );
    else
#endif
	fNextTurn = FALSE;
    
    fTurn = !fTurn;
	
    if( go == GAME_NORMAL && !fResigned && !fDoubled && fTurn != fMove ) {
	fMove = !fMove;
	anDice[ 0 ] = anDice[ 1 ] = 0;
	    
	SwapSides( anBoard );
    }

#if !X_DISPLAY_MISSING
    if( fX && nDelay ) {
	/* FIXME this is awful... it does delay, but it shouldn't do it by
	   sleeping here -- it should set a timeout and return to the main
	   event loop.

	   Another problem with this implementation is that the delay
	   will not work the first time NextTurn is called, because
	   tvLast will be initialised to zero and this code will assume
	   the delay has already elapsed. */
	gettimeofday( &tv, NULL );
	if( ( tvLast.tv_usec += 1000 * nDelay ) >= 1000000 ) {
	    tvLast.tv_sec += tvLast.tv_usec / 1000000;
	    tvLast.tv_usec %= 1000000;
	}
	
	if( tvLast.tv_sec > tv.tv_sec || ( tvLast.tv_sec == tv.tv_sec &&
					   tvLast.tv_usec > tv.tv_usec ) ) {
	    tvLast.tv_sec -= tv.tv_sec;
	    if( ( tvLast.tv_usec -= tv.tv_usec ) < 0 ) {
		tvLast.tv_usec += 1000000;
		tvLast.tv_sec--;
	    }
	    
	    select( 0, NULL, NULL, NULL, &tvLast );
	}
	
	gettimeofday( &tvLast, NULL );
    }
#endif
    
    if( go == GAME_NORMAL && ( fDisplay ||
			       ap[ fTurn ].pt == PLAYER_HUMAN ) )
	ShowBoard();
    
    if( ( n = GameStatus( anBoard ) ) ||
	( go == GAME_DROP && ( n = 1 ) ) ||
	( go == GAME_RESIGNED && ( n = fResigned ) ) ) {
	fWinner = !fTurn;
	
	if( fJacoby && fCubeOwner == -1 && !nMatchTo )
	    /* gammons do not count on a centred cube during money
	       sessions under the Jacoby rule */
	    n = 1;
	
	anScore[ fWinner ] += n * nCube;
	cGames++;
	
	fTurn = fMove = -1;
	anDice[ 0 ] = anDice[ 1 ] = 0;
	
	go = GAME_NORMAL;
	
	printf( "%s wins a %s and %d point%s.\n", ap[ fWinner ].szName,
		aszGameResult[ n - 1 ], n * nCube,
		n * nCube > 1 ? "s" : "" );
	
#if !X_DISPLAY_MISSING
	if( fX && fDisplay )
	    ShowBoard();
#endif
	
	if( nMatchTo && fAutoCrawford ) {
	    fCrawford = anScore[ fWinner ] == nMatchTo - 1 &&
		anScore[ !fWinner ] < nMatchTo - 1;
	    fPostCrawford = anScore[ !fWinner ] == nMatchTo - 1;
	}
	
	CommandShowScore( NULL );
	
	if( nMatchTo && anScore[ fWinner ] >= nMatchTo ) {
	    printf( "%s has won the match.\n", ap[ fWinner ].szName );
	    return;
	}
	
	if( fAutoGame ) {
	    NewGame();
	    
	    if( ap[ fTurn ].pt == PLAYER_HUMAN )
		ShowBoard();
	} else
	    return;
    }
    
    if( fTurn == fMove )
	fResigned = 0;
    
    if( fInterrupt )
	return;
    
    if( ap[ fTurn ].pt == PLAYER_HUMAN ) {
	if( fAutoRoll && !anDice[ 0 ] &&
	    ( !fCubeUse || ( fCubeOwner >= 0 && fCubeOwner != fTurn &&
			     !fDoubled ) ) )
	    CommandRoll( NULL );
	return;
    } else
#if !X_DISPLAY_MISSING
	if( fX )
	    EventPending( &evNextTurn, !ComputerTurn() );
	else
#endif
	    fNextTurn = !ComputerTurn();
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

    TurnDone();
}

extern void CommandDecline( char *sz ) {

    if( !fResigned ) {
	puts( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	printf( "%s declines the %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    TurnDone();
}

extern void CommandDouble( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( fCrawford ) {
	puts( "Doubling is forbidden by the Crawford rule (see `help set "
	      "crawford')." );

	return;
    }

    if( !fCubeUse ) {
	puts( "The doubling cube has been disabled (see `help set cube "
	      "use')." );

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
    
    TurnDone();
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
    
    TurnDone();
}

extern void CommandMove( char *sz ) {

    int c, i, j, anBoardNew[ 2 ][ 25 ], anBoardTest[ 2 ][ 25 ],
	an[ 8 ];
    movelist ml;
    movenormal *pmn;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }

    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	puts( "It is the computer's turn -- type `play' to force it to "
	      "move immediately." );
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
	GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );

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

	    TurnDone();

	    return;
	}

	if( fAutoBearoff && !TryBearoff() ) {
	    TurnDone();

	    return;
	}
	
	puts( "You must specify a move (type `help move' for instructions)." );

	return;
    }
    
    if( ( c = ParseMove( sz, an ) ) > 0 ) {
	for( i = 0; i < 25; i++ ) {
	    anBoardNew[ 0 ][ i ] = anBoard[ 0 ][ i ];
	    anBoardNew[ 1 ][ i ] = anBoard[ 1 ][ i ];
	}
	
	for( i = 0; i < c; i++ ) {
	    anBoardNew[ 1 ][ an[ i << 1 ] - 1 ]--;
	    if( an[ ( i << 1 ) | 1 ] > 0 ) {
		anBoardNew[ 1 ][ an[ ( i << 1 ) | 1 ] - 1 ]++;
		
		anBoardNew[ 0 ][ 24 ] +=
		    anBoardNew[ 0 ][ 24 - an[ ( i << 1 ) | 1 ] ];
		
		anBoardNew[ 0 ][ 24 - an[ ( i << 1 ) | 1 ] ] = 0;
	    }
	}
	
	GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );
	
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
		
		TurnDone();
		
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

    char *pch;
    
    if( nMatchTo && ( anScore[ 0 ] >= nMatchTo ||
		      anScore[ 1 ] >= nMatchTo ) ) {
	puts( "The match is already over." );

	return;
    }

    if( fTurn != -1 ) {
	while( fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    pch = GetInput( "Are you sure you want to start a new game, "
			    "and discard the one in progress? " );

	    if( fInterrupt )
		return;

	    if( pch && ( *pch == 'y' || *pch == 'Y' ) )
		break;

	    if( pch && ( *pch == 'n' || *pch == 'N' ) )
		return;

	    puts( "Please answer 'y' or 'n'." );
	}

	/* The last game of the match should always be the current one. */
	assert( lMatch.plPrev->p == plGame );

	ListDelete( lMatch.plPrev );
	
	FreeGame( plGame );
    }
    
    NewGame();

    if( fInterrupt )
	return;
    
    if( ap[ fTurn ].pt == PLAYER_HUMAN )
	ShowBoard();
    else
	if( !ComputerTurn() )
	    TurnDone();
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

    cGames = anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fCrawford = 0;
    fPostCrawford = 0;

    printf( "A new %d point match has been started.\n", n );

#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif
}

extern void CommandNewSession( char *sz ) {

    FreeMatch();

    cGames = nMatchTo = anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fCrawford = 0;
    fPostCrawford = 0;

    puts( "A new session has been started." );
    
#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif
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

    if( !ComputerTurn() )
	TurnDone();
}

extern void CommandRedouble( char *sz ) {

    movetype *pmt;

    if( nMatchTo > 0 ) {
	puts( "Redoubles are not permitted during match play." );

	return;
    }
    
    if( fTurn < 0 || !fDoubled ) {
	puts( "The cube must have been offered before you can redouble it." );

	return;
    }
    
    nCube <<= 1;

    if( fDisplay )
	printf( "%s accepts and immediately redoubles to %d.\n",
		ap[ fTurn ].szName, nCube << 1 );
    
    fCubeOwner = !fMove;
    CalcGammonPrice ( nCube, fCubeOwner );

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DOUBLE;
    ListInsert( plGame, pmt );
    
    TurnDone();

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

    TurnDone();
}

extern void CommandRoll( char *sz ) {

    movelist ml;
    movenormal *pmn;
    
    if( fTurn < 0 ) {
	puts( "No game in progress (type `new' to start one)." );

	return;
    }
    
    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	puts( "It is the computer's turn -- type `play' to force it to "
	      "move immediately." );
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
    
    if( RollDice( anDice ) < 0 )
	return;

    ShowBoard();

    if( !GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE ) ) {
	pmn = malloc( sizeof( *pmn ) );
	pmn->mt = MOVE_NORMAL;
	pmn->anRoll[ 0 ] = anDice[ 0 ];
	pmn->anRoll[ 1 ] = anDice[ 1 ];
	pmn->fPlayer = fTurn;
	pmn->anMove[ 0 ] = -1;
	ListInsert( plGame, pmn );
	
	puts( "No legal moves." );

	TurnDone();
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
	
	TurnDone();
    } else
	if( fAutoBearoff && !TryBearoff() )
	    TurnDone();
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

    fCubeOwner = !fMove;
    CalcGammonPrice ( nCube, fCubeOwner );

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_TAKE;
    ListInsert( plGame, pmt );
    
    TurnDone();
}

extern void 
SetCube ( int nNewCube, int fNewCubeOwner ) {

  nCube = nNewCube;
  fCubeOwner = fNewCubeOwner;

  CalcGammonPrice ( nCube, fCubeOwner );

}





