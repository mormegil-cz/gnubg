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
#include "matchequity.h"

char *aszGameResult[] = { "single game", "gammon", "backgammon" };
enum _gameover { GAME_NORMAL, GAME_RESIGNED, GAME_DROP } go;
list lMatch, *plGame;

#if USE_GTK
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#endif

#if USE_GUI
static struct timeval tvLast;

void ResetDelayTimer( void ) {
    
    if( fX && nDelay && fDisplay )
	gettimeofday( &tvLast, NULL );
}
#else
#define ResetDelayTimer()
#endif

static void AddMoveRecord( void *pmr ) {

    /* FIXME delete all records after the current position */
    
    ListInsert( plGame, pmr );
}

static void NewGame( void ) {

    moverecord *pmr;
    
    InitBoard( anBoard );

    plGame = malloc( sizeof( *plGame ) );
    ListCreate( plGame );

    ListInsert( &lMatch, plGame );

    pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.i = cGames;
    pmr->g.nMatch = nMatchTo;
    pmr->g.anScore[ 0 ] = anScore[ 0 ];
    pmr->g.anScore[ 1 ] = anScore[ 1 ];
    pmr->g.fCrawford = fAutoCrawford && nMatchTo > 1;
    pmr->g.fCrawfordGame = fCrawford;
    pmr->g.fJacoby = fJacoby && !nMatchTo;
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    AddMoveRecord( pmr );
        
    fResigned = fDoubled = FALSE;
    nCube = 1;
    fCubeOwner = -1;
    UpdateSetting( &nCube );
    UpdateSetting( &fCubeOwner );
    
    go = GAME_NORMAL;
    
 reroll:
    RollDice( anDice );

    if( fInterrupt ) {
	ListDelete( plGame->plNext );
	free( pmr );
	free( plGame );
	ListDelete( lMatch.plPrev );
	fMove = fTurn = -1;
	UpdateSetting( &fTurn );

	return;
    }
    
    if( fDisplay ) {
      outputnew();
      outputf( "%s rolls %d, %s rolls %d.\n", ap[ 0 ].szName, anDice[ 0 ],
               ap[ 1 ].szName, anDice[ 1 ] );
      outputx();
    }

    if( anDice[ 0 ] == anDice[ 1 ] && nCube < MAX_CUBE ) {
	if( !nMatchTo && nCube < ( 1 << cAutoDoubles ) && fCubeUse ) {
	    pmr->g.nAutoDoubles++;
	    outputf( "The cube is now at %d.\n", nCube <<= 1 );
	    UpdateSetting( &nCube );
	}
	
	goto reroll;
    }

    fMove = fTurn = anDice[ 1 ] > anDice[ 0 ];
    UpdateSetting( &fTurn );
    
#if USE_GUI
    if( fX && fDisplay )
	ShowBoard();

    ResetDelayTimer();
#endif
}

static void ShowAutoMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    char sz[ 40 ];

    outputf( "%s moves %s\n",
             ap [ fTurn ].szName, FormatMove( sz, anBoard, anMove ) );
}


static int ComputerTurn( void ) {

  movenormal *pmn;
  cubeinfo ci;
  float arDouble[ 4 ], arOutput[ NUM_OUTPUTS ], rDoublePoint;
  char sz [ 1024 ];
    
  SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

  switch( ap[ fTurn ].pt ) {
  case PLAYER_GNU:
    if( fResigned ) {

      if( EvaluatePosition( anBoard, arOutput, &ci, &ap[ fTurn ].ec ) )
        return -1;

      if( -fResigned <= Utility ( arOutput, &ci ) ) {
        CommandAgree( NULL );
        return 0;
      } else {
        CommandDecline( NULL );
        return 0;
      }

    } else if( fDoubled ) {

      /* Consider cube action */

      if ( EvaluatePositionCubeful ( anBoard, arDouble, &ci,
                                     &ap [ fTurn ].ec,
                                     ap [ fTurn ].ec.nPlies ) < 0 )
        return -1;

      if ( fBeavers && ! nMatchTo && arDouble[ 2 ] <= 0.0 ) {
        /* It's a beaver... beaver all night! */
        CommandRedouble ( NULL );
      }
      else if ( arDouble[ 2 ] <= arDouble[ 3 ] )
        CommandTake ( NULL );
      else
        CommandReject ( NULL );

      return 0;

    } else {
      int anBoardMove[ 2 ][ 25 ];
	    

      /* Don't use the global board for this call, to avoid
	 race conditions with updating the board and aborting the
	 move with an interrupt. */
      memcpy( anBoardMove, anBoard, sizeof( anBoardMove ) );

      /* Consider doubling */

      if ( fCubeUse && ! anDice[ 0 ] && GetDPEq ( NULL, NULL, &ci ) ) {

        static evalcontext ecDH = { 1, 8, 0.16, 0, FALSE }; 
        
        /* We have access to the cube */

        /* Determine market window */

        if ( EvaluatePosition ( anBoardMove, arOutput, &ci, &ecDH ) )
          return -1;

        rDoublePoint = 
          GetDoublePointDeadCube ( arOutput, anScore, nMatchTo, &ci );

        if ( arOutput[ 0 ] >= rDoublePoint ) {

          /* We're in market window */

          if ( EvaluatePositionCubeful ( anBoard, arDouble, &ci,
                                         &ap [ fTurn ].ec,
                                         ap [ fTurn ].ec.nPlies ) < 0 )
            return -1;

          if ( ( arDouble[ 3 ] >= arDouble[ 1 ] ) &&
               ( arDouble[ 2 ] >= arDouble[ 1 ] ) ) {
            CommandDouble ( NULL );
            return 0;
          }
        } /* market window */
      } /* access to cube */

      /* Roll dice and move */

      if ( ! anDice[ 0 ] ) {

        RollDice ( anDice );

        /* write line to status bar if using GTK */

#ifdef USE_GTK        
        if ( fX ) {

          outputnew ();
          outputf ( "%s rolls %1i and %1i\n",
                    ap [ fTurn ].szName, anDice[ 0 ], anDice[ 1 ] );
          outputx ();

        }
#endif
      }


      if ( fDisplay )
	  ShowBoard();

      pmn = malloc( sizeof( *pmn ) );
      pmn->mt = MOVE_NORMAL;
      pmn->anRoll[ 0 ] = anDice[ 0 ];
      pmn->anRoll[ 1 ] = anDice[ 1 ];
      pmn->fPlayer = fTurn;
      
      if( FindBestMove( pmn->anMove, anDice[ 0 ], anDice[ 1 ],
                        anBoardMove, &ci, &ap[ fTurn ].ec ) < 0 ) {
        free( pmn );
        return -1;
      }
      
      memcpy( anBoard, anBoardMove, sizeof( anBoardMove ) );
      AddMoveRecord( pmn );      
      
      /* write move to status bar if using GTK */

#ifdef USE_GTK        
      if ( fX ) {
	  
	  outputnew ();
	  outputf ( "%s moves %s\n",
		    ap [ fTurn ].szName,
		    FormatMove( sz, anBoardMove, pmn->anMove ) );
	  outputx ();
      }
#endif
      
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
    } else if( !anDice[ 0 ] ) {
      if( RollDice( anDice ) < 0 )
	    return -1;
      
      if( fDisplay )
        ShowBoard();
    }
    
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->anRoll[ 0 ] = anDice[ 0 ];
    pmn->anRoll[ 1 ] = anDice[ 1 ];
    pmn->fPlayer = fTurn;
    AddMoveRecord( pmn );
    
    return FindPubevalMove( anDice[ 0 ], anDice[ 1 ], anBoard, pmn->anMove );

  default:
    assert( FALSE );
    return -1;
  }
}

extern void CancelCubeAction( void ) {
    
    if( fDoubled ) {
	outputf( "(%s's double has been cancelled.)\n", ap[ fMove ].szName );
	fDoubled = FALSE;
	fNextTurn = TRUE;

	if( fDisplay )
	    ShowBoard();
	
	/* FIXME delete all MOVE_DOUBLE records */
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
		AddMoveRecord( pmn );
		
		ShowAutoMove( anBoard, pmn->anMove );
		
		PositionFromKey( anBoard, ml.amMoves[ i ].auch );
		
		return 0;
	    }

    return -1;
}

extern void NextTurn( void ) {

    int n;
#if USE_GUI
    struct timeval tv;
    fd_set fds;
#endif
    
#if USE_GUI
    if( fX ) {
#if USE_GTK
	if( nNextTurn ) {
	    gtk_idle_remove( nNextTurn );
	    nNextTurn = 0;
	}
#else
        EventPending( &evNextTurn, FALSE );	
#endif
    } else
#endif
	fNextTurn = FALSE;
    
    fTurn = !fTurn;
    UpdateSetting( &fTurn );
	
    if( go == GAME_NORMAL && !fResigned && !fDoubled && fTurn != fMove ) {
	fMove = !fMove;
	anDice[ 0 ] = anDice[ 1 ] = 0;
	    
	SwapSides( anBoard );
    }

#if USE_GUI
    if( fX && nDelay && fDisplay ) {
	if( tvLast.tv_sec ) {
	    if( ( tvLast.tv_usec += 1000 * nDelay ) >= 1000000 ) {
		tvLast.tv_sec += tvLast.tv_usec / 1000000;
		tvLast.tv_usec %= 1000000;
	    }
	
	restart:
	    gettimeofday( &tv, NULL );
		
	    if( tvLast.tv_sec > tv.tv_sec ||
		( tvLast.tv_sec == tv.tv_sec &&
		  tvLast.tv_usec > tv.tv_usec ) ) {
		tv.tv_sec = tvLast.tv_sec - tv.tv_sec;
		if( ( tv.tv_usec = tvLast.tv_usec - tv.tv_usec ) < 0 ) {
		    tv.tv_usec += 1000000;
		    tv.tv_sec--;
		}

#ifdef ConnectionNumber /* FIXME use configure for this */
		FD_ZERO( &fds );
		FD_SET( ConnectionNumber( DISPLAY ), &fds );
		/* GTK-FIXME: use timeout */
		if( select( ConnectionNumber( DISPLAY ) + 1, &fds, NULL,
			    NULL, &tv ) > 0 ) {
		    HandleXAction();
		    if( !fInterrupt )
			goto restart;
		}
#else
		if( select( 0, NULL, NULL, NULL, &tv ) > 0 && !fInterrupt )
		    goto restart;
#endif
		 
	    }
	}
	ResetDelayTimer();
    }
#endif
    
    if( go == GAME_NORMAL && ( fDisplay ||
			       ap[ fTurn ].pt == PLAYER_HUMAN ) )
	ShowBoard();
    
    if( ( n = GameStatus( anBoard ) ) ||
	( go == GAME_DROP && ( ( n = 1 ) ) ) ||
	( go == GAME_RESIGNED && ( ( n = fResigned ) ) ) ) {
	movegameinfo *pmgi = plGame->plNext->p;
	
	pmgi->fWinner = !fTurn;
	pmgi->fResigned = go == GAME_RESIGNED;
	
	if( fJacoby && fCubeOwner == -1 && !nMatchTo )
	    /* gammons do not count on a centred cube during money
	       sessions under the Jacoby rule */
	    n = 1;
	
	anScore[ pmgi->fWinner ] += ( pmgi->nPoints = n * nCube );
	cGames++;
	
	fTurn = fMove = -1;
	anDice[ 0 ] = anDice[ 1 ] = 0;
	
	UpdateSetting( &fTurn );

	go = GAME_NORMAL;
	
	outputf( "%s wins a %s and %d point%s.\n", ap[ pmgi->fWinner ].szName,
		aszGameResult[ n - 1 ], n * nCube,
		n * nCube > 1 ? "s" : "" );
	
#if USE_GUI
	if( fX && fDisplay )
	    ShowBoard();
#endif
	
	if( nMatchTo && fAutoCrawford ) {
	    fPostCrawford = fCrawford && anScore[ pmgi->fWinner ] < nMatchTo;
	    fCrawford = !fPostCrawford && !fCrawford &&
		anScore[ pmgi->fWinner ] == nMatchTo - 1;
	}
	
	CommandShowScore( NULL );
	
	if( nMatchTo && anScore[ pmgi->fWinner ] >= nMatchTo ) {
	    outputf( "%s has won the match.\n", ap[ pmgi->fWinner ].szName );
	    outputx();
	    return;
	}

	outputx();
	
	if( fAutoGame ) {
	    NewGame();
	    
	    if( ap[ fTurn ].pt == PLAYER_HUMAN )
		ShowBoard();
	} else
	    return;
    }
    
    if( fTurn == fMove )
	fResigned = 0;

    /* We have reached a safe point to check for interrupts.  Until now,
       the board could have been in an inconsistent state. */
    if( fInterrupt )
	return;
    
    if( ap[ fTurn ].pt == PLAYER_HUMAN ) {
	if( fAutoRoll && !anDice[ 0 ] &&
	    ( !fCubeUse || ( fCubeOwner >= 0 && fCubeOwner != fTurn &&
			     !fDoubled ) ) )
	    CommandRoll( NULL );
	return;
    } else
#if USE_GUI
	if( fX ) {
#if USE_GTK
	    if( !ComputerTurn() )
		nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
#else
            EventPending( &evNextTurn, !ComputerTurn() );	    
#endif
	} else
#endif
	    fNextTurn = !ComputerTurn();
}

extern void TurnDone( void ) {

#if USE_GUI
    if( fX )
#if USE_GTK
	nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
#else
        EventPending( &evNextTurn, TRUE );    
#endif
    else
#endif
	fNextTurn = TRUE;

    outputx();
}

extern void CommandAccept( char *sz ) {

    if( fResigned )
	CommandAgree( sz );
    else if( fDoubled )
	CommandTake( sz );
    else
	outputl( "You can only accept if the cube or a resignation has been "
		 "offered." );
}

extern void CommandAgree( char *sz ) {

    moveresign *pmr;
    
    if( !fResigned ) {
	outputl( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	outputf( "%s accepts and wins a %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    go = GAME_RESIGNED;

    pmr = malloc( sizeof( *pmr ) );
    pmr->mt = MOVE_RESIGN;
    pmr->fPlayer = !fTurn;
    pmr->nResigned = fResigned;
    AddMoveRecord( pmr );

    TurnDone();
}

extern void CommandDecline( char *sz ) {

    if( !fResigned ) {
	outputl( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	outputf( "%s declines the %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    TurnDone();
}

extern void CommandDouble( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( fCrawford ) {
	outputl( "Doubling is forbidden by the Crawford rule (see `help set "
	      "crawford')." );

	return;
    }

    if( !fCubeUse ) {
	outputl( "The doubling cube has been disabled (see `help set cube "
	      "use')." );

	return;
    }

    if( fDoubled ) {
	outputl( "The `double' command is for offering the cube, not "
		 "accepting it.  Use\n`redouble' to immediately offer the "
		 "cube back at a higher value." );

	return;
    }
    
    if( fTurn != fMove ) {
	outputl( "You are only allowed to double if you are on roll." );

	return;
    }
    
    if( anDice[ 0 ] ) {
	outputl( "You can't double after rolling the dice -- wait until your "
	      "next turn." );

	return;
    }

    if( fCubeOwner >= 0 && fCubeOwner != fTurn ) {
	outputl( "You do not own the cube." );

	return;
    }
    
    fDoubled = 1;

    if( fDisplay )
	outputf( "%s doubles.\n", ap[ fTurn ].szName );
    
    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DOUBLE;
    AddMoveRecord( pmt );
    
    TurnDone();
}

extern void CommandDrop( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 || !fDoubled ) {
	outputl( "The cube must have been offered before you can drop it." );

	return;
    }

    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( fDisplay )
	outputf( "%s refuses the cube and gives up %d points.\n",
		ap[ fTurn ].szName, nCube );
    
    fDoubled = FALSE;

    go = GAME_DROP;

    fTurn = !fTurn;
    UpdateSetting( &fTurn );

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DROP;
    AddMoveRecord( pmt );
    
    TurnDone();
}


extern void 
CommandMove( char *sz ) {

  int c, i, j, anBoardNew[ 2 ][ 25 ], anBoardTest[ 2 ][ 25 ],
    an[ 8 ];
  movelist ml;
  movenormal *pmn;
  char szTemp[ 1024 ];
    
  if( fTurn < 0 ) {
    outputl( "No game in progress (type `new' to start one)." );

    return;
  }

  if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
    outputl( "It is the computer's turn -- type `play' to force it to "
             "move immediately." );
    return;
  }

  if( !anDice[ 0 ] ) {
    outputl( "You must roll the dice before you can move." );

    return;
  }

  if( fResigned ) {
    outputf( "Please wait for %s to consider the resignation before "
             "moving.\n", ap[ fTurn ].szName );

    return;
  }

  if( fDoubled ) {
    outputf( "Please wait for %s to consider the cube before "
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
	    AddMoveRecord( pmn );

	    ShowAutoMove( anBoard, pmn->anMove );
	    
	    PositionFromKey( anBoard, ml.amMoves[ 0 ].auch );

	    TurnDone();

	    return;
    }

    if( fAutoBearoff && !TryBearoff() ) {
	    TurnDone();

	    return;
    }
	
    outputl( "You must specify a move (type `help move' for "
             "instructions)." );

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
        AddMoveRecord( pmn );
		
        memcpy( anBoard, anBoardNew, sizeof( anBoard ) );
		
#ifdef USE_GTK        
        if ( fX ) {

          outputnew ();
          outputf ( "%s moves %s\n",
                    ap [ fTurn ].szName,
                    FormatMove( szTemp, anBoard, pmn->anMove ) );
          outputx ();
        
      }
#endif

        TurnDone();
		
        return;
	    }
    }
  }

  outputl( "Illegal move." );
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
	outputl( "The match is already over." );

	return;
    }

    if( fTurn != -1 ) {
	if( fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to start a new game, "
			     "and discard the one in progress? " ) )
		return;
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

extern void FreeMatch( void ) {

    list *plMatch;

    while( ( plMatch = lMatch.plNext ) != &lMatch ) {
	FreeGame( plMatch->p );
	ListDelete( plMatch );
    }
}

extern void CommandNewMatch( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( "You must specify a valid match length (1 or longer)." );

	return;
    }

    /* Check that match equity table is large enough */

    if ( n > nMaxScore ) {

      outputf ( "The current match equity table does not support "
                "matches of length %i\n"
                "(see `help set matchequitytable')\n", n );
      return;
    }

    if( fTurn != -1 && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( "Are you sure you want to start a new match, "
			 "and discard the game in progress? " ) )
	    return;
    }
    
    FreeMatch();

    nMatchTo = n;

    cGames = anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fCrawford = FALSE;
    fPostCrawford = FALSE;

    UpdateSetting( &nMatchTo );
    UpdateSetting( &fTurn );
    UpdateSetting( &fCrawford );
    
    outputf( "A new %d point match has been started.\n", n );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif

    if( fAutoGame )
	CommandNewGame( NULL );
}

extern void CommandNewSession( char *sz ) {

    if( fTurn != -1 && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( "Are you sure you want to start a new session, "
			 "and discard the game in progress? " ) )
	    return;
    }
    
    FreeMatch();

    cGames = nMatchTo = anScore[ 0 ] = anScore[ 1 ] = 0;
    fTurn = -1;
    fCrawford = 0;
    fPostCrawford = 0;

    UpdateSetting( &nMatchTo );
    UpdateSetting( &fTurn );
    UpdateSetting( &fCrawford );
    
    outputl( "A new session has been started." );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
    
    if( fAutoGame )
	CommandNewGame( NULL );
}

extern void CommandPlay( char *sz ) {

    if( fTurn < 0 ) {
	outputl( "No game in progress (type `new' to start one)." );

	return;
    }

    if( ap[ fTurn ].pt == PLAYER_HUMAN ) {
	outputl( "It's not the computer's turn to play." );

	return;
    }

    if( !ComputerTurn() )
	TurnDone();
}

extern void CommandRedouble( char *sz ) {

    movetype *pmt;

    if( nMatchTo > 0 ) {
	outputl( "Redoubles are not permitted during match play." );

	return;
    }
    
    if( fTurn < 0 || !fDoubled ) {
	outputl( "The cube must have been offered before you can redouble "
		 "it." );

	return;
    }
    
    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    nCube <<= 1;
    UpdateSetting( &nCube );    

    if( fDisplay )
	outputf( "%s accepts and immediately redoubles to %d.\n",
		ap[ fTurn ].szName, nCube << 1 );
    
    fCubeOwner = !fMove;
    UpdateSetting( &fCubeOwner );
    
    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_DOUBLE;
    AddMoveRecord( pmt );
    
    TurnDone();
}

extern void CommandReject( char *sz ) {

    if( fResigned )
	CommandDecline( sz );
    else if( fDoubled )
	CommandDrop( sz );
    else
	outputl( "You can only reject if the cube or a resignation has been "
	      "offered." );
}

extern void CommandResign( char *sz ) {

    char *pch;
    int cch;
    
    if( fTurn < 0 ) {
	outputl( "You must be playing a game if you want to resign it." );

	return;
    }

    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
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

	outputf( "Unknown keyword `%s' -- try `help resign'.\n", pch );
	
	return;
    }
    
    if( fDisplay )
	outputf( "%s offers to resign a %s.\n", ap[ fTurn ].szName,
		aszGameResult[ fResigned - 1 ] );

    TurnDone();
}


extern void 
CommandRoll( char *sz ) {

  movelist ml;
  movenormal *pmn;
    
  if( fTurn < 0 ) {
    outputl( "No game in progress (type `new' to start one)." );

    return;
  }
    
  if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
    outputl( "It is the computer's turn -- type `play' to force it to "
             "move immediately." );
    return;
  }

  if( fDoubled ) {
    outputf( "Please wait for %s to consider the cube before "
             "moving.\n", ap[ fTurn ].szName );

    return;
  }    

  if( fResigned ) {
    outputl( "Please resolve the resignation first." );

    return;
  }

  if( anDice[ 0 ] ) {
    outputl( "You already did roll the dice." );

    return;
  }
    
  if( RollDice( anDice ) < 0 )
    return;

  ShowBoard();

#ifdef USE_GTK        
  if ( fX ) {

    outputnew ();
    outputf ( "%s rolls %1i and %1i\n",
              ap [ fTurn ].szName, anDice[ 0 ], anDice[ 1 ] );
    outputx ();

  }
#endif

  ResetDelayTimer();
    
  if( !GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE ) ) {
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->anRoll[ 0 ] = anDice[ 0 ];
    pmn->anRoll[ 1 ] = anDice[ 1 ];
    pmn->fPlayer = fTurn;
    pmn->anMove[ 0 ] = -1;
    AddMoveRecord( pmn );
	
    outputl( "No legal moves." );

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

    ShowAutoMove( anBoard, pmn->anMove );
	
    AddMoveRecord( pmn );
	
    PositionFromKey( anBoard, ml.amMoves[ 0 ].auch );
	
    TurnDone();
  } else
    if( fAutoBearoff && !TryBearoff() )
	    TurnDone();
}


extern void CommandTake( char *sz ) {

    movetype *pmt;
    
    if( fTurn < 0 || !fDoubled ) {
	outputl( "The cube must have been offered before you can take it." );

	return;
    }

    if( ap[ fTurn ].pt != PLAYER_HUMAN ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    nCube <<= 1;
    UpdateSetting( &nCube );
    
    if( fDisplay )
	outputf( "%s accepts the cube at %d.\n", ap[ fTurn ].szName, nCube );
    
    fDoubled = FALSE;

    fTurn = fCubeOwner = !fMove;
    UpdateSetting( &fTurn );
    UpdateSetting( &fCubeOwner );

    pmt = malloc( sizeof( *pmt ) );
    *pmt = MOVE_TAKE;
    AddMoveRecord( pmt );
    
    TurnDone();
}
