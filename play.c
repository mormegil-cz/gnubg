/*
 * play.c
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

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "analysis.h"
#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "external.h"
#include "eval.h"
#include "positionid.h"
#include "matchequity.h"

#ifndef HUGE_VALF
#define HUGE_VALF (-1e38)
#endif

char *aszGameResult[] = { "single game", "gammon", "backgammon" },
    *aszSkillType[] = { "very bad", "bad", "doubtful", NULL,
			"interesting", "good", "very good" },
    *aszSkillTypeAbbr[] = { "??", "?", "?!", "", "!?", "!", "!!" },
    *aszLuckType[] = { "very unlucky", "unlucky", NULL, "lucky",
		       "very lucky" },
    *aszLuckTypeAbbr[] = { "--", "-", "", "+", "++" };
list lMatch, *plGame, *plLastMove;
statcontext scMatch;
static int fComputerDecision = FALSE;

#if USE_GTK
#include "gtkboard.h"
#include "gtkgame.h"
#endif

static void PlayMove( matchstate *pms, int anMove[ 8 ], int fPlayer ) {

    int i, nSrc, nDest;
    
    if( pms->fMove != -1 && fPlayer != pms->fMove )
	SwapSides( pms->anBoard );
    
    for( i = 0; i < 8; i += 2 ) {
	nSrc = anMove[ i ];
	nDest = anMove[ i | 1 ];

	if( nSrc < 0 )
	    /* move is finished */
	    break;
	
	if( !pms->anBoard[ 1 ][ nSrc ] )
	    /* source point is empty; ignore */
	    continue;

	pms->anBoard[ 1 ][ nSrc ]--;
	if( nDest >= 0 )
	    pms->anBoard[ 1 ][ nDest ]++;

	if( nDest >= 0 && nDest <= 23 ) {
	    pms->anBoard[ 0 ][ 24 ] += pms->anBoard[ 0 ][ 23 - nDest ];
	    pms->anBoard[ 0 ][ 23 - nDest ] = 0;
	}
    }

    pms->fMove = pms->fTurn = !fPlayer;
    SwapSides( pms->anBoard );    
}

static void ApplyGameOver( matchstate *pms ) {

    movegameinfo *pmgi = plGame->plNext->p;

    assert( pmgi->mt == MOVE_GAMEINFO );

    if( pmgi->fWinner < 0 )
	return;
    
    pms->anScore[ pmgi->fWinner ] += pmgi->nPoints;
}

extern void ApplyMoveRecord( matchstate *pms, moverecord *pmr ) {

    int n;
    movegameinfo *pmgi = plGame->plNext->p;

    assert( pmr->mt == MOVE_GAMEINFO || pmgi->mt == MOVE_GAMEINFO );
    
    pms->fResigned = FALSE;
    pms->gs = GAME_PLAYING;
    
    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	InitBoard( pms->anBoard );

	pms->nMatchTo = pmr->g.nMatch;
	pms->anScore[ 0 ] = pmr->g.anScore[ 0 ];
	pms->anScore[ 1 ] = pmr->g.anScore[ 1 ];
	
	pms->gs = GAME_NONE;
	pms->fMove = pms->fTurn = pms->fCubeOwner = -1;
	pms->anDice[ 0 ] = pms->anDice[ 1 ] = 0;
	pms->fResigned = pms->fDoubled = FALSE;
	pms->nCube = 1 << pmr->g.nAutoDoubles;
	pms->fCrawford = pmr->g.fCrawfordGame;
	pms->fPostCrawford = !pms->fCrawford &&
	    ( pms->anScore[ 0 ] == pms->nMatchTo - 1 ||
	      pms->anScore[ 1 ] == pms->nMatchTo - 1 );
	break;
	
    case MOVE_DOUBLE:
	if( pms->fMove < 0 )
	    pms->fMove = pmr->d.fPlayer;
	
	if( pms->nCube >= MAX_CUBE )
	    break;
	
	if( pms->fDoubled ) {
	    pms->nCube <<= 1;
	    pms->fCubeOwner = !pms->fMove;
	} else
	    pms->fDoubled = TRUE;
	
	pms->fTurn = !pmr->d.fPlayer;
	break;

    case MOVE_TAKE:
	if( !pms->fDoubled )
	    break;
	
	pms->nCube <<= 1;
	pms->fDoubled = FALSE;
	pms->fCubeOwner = !pms->fMove;
	pms->fTurn = pms->fMove;
	break;

    case MOVE_DROP:
	if( !pms->fDoubled )
	    break;
	
	pms->fDoubled = FALSE;
	pms->gs = GAME_DROP;
	pmgi->nPoints = pms->nCube;
	pmgi->fWinner = !pmr->d.fPlayer;
	pmgi->fResigned = FALSE;
	
	ApplyGameOver( pms );
	break;

    case MOVE_NORMAL:
	pms->fDoubled = FALSE;
#if USE_GTK
	if( fX )
	    game_set_old_dice( BOARD( pwBoard ), pmr->n.anRoll[ 0 ],
			       pmr->n.anRoll[ 1 ] );
#endif
	PlayMove( pms, pmr->n.anMove, pmr->n.fPlayer );
	pms->anDice[ 0 ] = pms->anDice[ 1 ] = 0;

	if( ( n = GameStatus( pms->anBoard ) ) ) {
	    pms->gs = GAME_OVER;
	    pmgi->nPoints = pms->nCube * n;
	    pmgi->fWinner = pmr->n.fPlayer;
	    pmgi->fResigned = FALSE;
	    ApplyGameOver( pms );
	}
	
	break;

    case MOVE_RESIGN:
	pms->gs = GAME_RESIGNED;
	pmgi->nPoints = pms->nCube * ( pms->fResigned = pmr->r.nResigned );
	pmgi->fWinner = !pmr->r.fPlayer;
	pmgi->fResigned = TRUE;
	
	ApplyGameOver( pms );
	break;
	
    case MOVE_SETBOARD:
	PositionFromKey( pms->anBoard, pmr->sb.auchKey );

	if( pms->fMove < 0 )
	    pms->fTurn = pms->fMove = 0;
	
	if( pms->fMove )
	    SwapSides( pms->anBoard );

	break;
	
    case MOVE_SETDICE:
	pms->anDice[ 0 ] = pmr->sd.anDice[ 0 ];
	pms->anDice[ 1 ] = pmr->sd.anDice[ 1 ];
	pms->fTurn = pms->fMove = pmr->sd.fPlayer;
	pms->fDoubled = FALSE;
	break;
	
    case MOVE_SETCUBEVAL:
	if( pms->fMove < 0 )
	    pms->fMove = 0;
	
	pms->nCube = pmr->scv.nCube;
	pms->fDoubled = FALSE;
	pms->fTurn = pms->fMove;
	break;
	
    case MOVE_SETCUBEPOS:
	if( pms->fMove < 0 )
	    pms->fMove = 0;
	
	pms->fCubeOwner = pmr->scp.fCubeOwner;
	pms->fDoubled = FALSE;
	pms->fTurn = pms->fMove;
	break;
    }
}

extern void CalculateBoard( void ) {

    list *pl;

    pl = plGame;
    do {
	pl = pl->plNext;

	assert( pl->p );

	ApplyMoveRecord( &ms, pl->p );
    } while( pl != plLastMove );
}

static void FreeMoveRecord( moverecord *pmr ) {

    switch( pmr->mt ) {
    case MOVE_NORMAL:
	if( pmr->n.ml.cMoves )
	    free( pmr->n.ml.amMoves );
	break;

    default:
	break;
    }

    if( pmr->a.sz )
	free( pmr->a.sz );
    
    free( pmr );
}

static void FreeGame( list *pl ) {

    while( pl->plNext != pl ) {
	FreeMoveRecord( pl->plNext->p );
	ListDelete( pl->plNext );
    }

    free( pl );
}

static int PopGame( list *plDelete, int fInclusive ) {

    list *pl;
    int i;
    
    for( i = 0, pl = lMatch.plNext; pl != &lMatch && pl->p != plDelete;
	 pl = pl->plNext, i++ )
	;

    if( pl->p && !fInclusive ) {
	pl = pl->plNext;
	i++;
    }
    
    if( !pl->p )
	/* couldn't find node to delete to */
	return -1;

#if USE_GTK
    if( fX )
	GTKPopGame( i );
#endif

    do {
	pl = pl->plNext;
	FreeGame( pl->plPrev->p );
	ListDelete( pl->plPrev );
	ms.cGames--;
    } while( pl->p );

    return 0;
}

static int PopMoveRecord( list *plDelete ) {

    list *pl;
    
    for( pl = plGame->plNext; pl != plGame && pl != plDelete; pl = pl->plNext )
	;

    if( pl == plGame )
	/* couldn't find node to delete to */
	return -1;

#if USE_GTK
    if( fX ) {
	GTKPopMoveRecord( pl->p );
	GTKSetMoveRecord( pl->plPrev->p );
    }
#endif
    
    pl = pl->plPrev;

    while( pl->plNext->p ) {
	if( pl->plNext == plLastMove )
	    plLastMove = pl;
	FreeMoveRecord( pl->plNext->p );
	ListDelete( pl->plNext );
    }

    return 0;
}

extern void AddMoveRecord( void *pv ) {

    moverecord *pmr = pv, *pmrOld;

    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	assert( pmr->g.nMatch >= 0 );
	assert( pmr->g.i >= 0 );
	if( pmr->g.nMatch ) {
	    assert( pmr->g.i <= pmr->g.nMatch * 2 + 1 );
	    assert( pmr->g.anScore[ 0 ] < pmr->g.nMatch );
	    assert( pmr->g.anScore[ 1 ] < pmr->g.nMatch );
	}
	if( !pmr->g.fCrawford )
	    assert( !pmr->g.fCrawfordGame );
	
	break;
	
    case MOVE_NORMAL:
	assert( pmr->n.fPlayer >= 0 && pmr->n.fPlayer <= 1 );
	assert( pmr->n.etDouble >= EVAL_NONE &&
		pmr->n.etDouble <= EVAL_ROLLOUT );
	assert( pmr->n.ml.cMoves >= 0 && pmr->n.ml.cMoves < MAX_MOVES );
	if( pmr->n.ml.cMoves )
	    assert( pmr->n.iMove >= 0 && pmr->n.iMove <= pmr->n.ml.cMoves );
	assert( pmr->n.lt >= LUCK_VERYBAD && pmr->n.lt <= LUCK_VERYGOOD );
	assert( pmr->n.st >= SKILL_VERYBAD && pmr->n.st <= SKILL_VERYGOOD );
	break;
	
    case MOVE_DOUBLE:
    case MOVE_TAKE:
    case MOVE_DROP:
	assert( pmr->d.fPlayer >= 0 && pmr->d.fPlayer <= 1 );
	assert( pmr->d.etDouble >= EVAL_NONE &&
		pmr->d.etDouble <= EVAL_ROLLOUT );
	assert( pmr->d.st >= SKILL_VERYBAD && pmr->d.st <= SKILL_VERYGOOD );
	break;
	
    case MOVE_RESIGN:
	assert( pmr->r.fPlayer >= 0 && pmr->r.fPlayer <= 1 );
	assert( pmr->r.nResigned >= 1 && pmr->r.nResigned <= 3 );
	break;
	
    case MOVE_SETDICE:
	assert( pmr->sd.fPlayer >= 0 && pmr->sd.fPlayer <= 1 );	
	assert( pmr->sd.lt >= LUCK_VERYBAD && pmr->sd.lt <= LUCK_VERYGOOD );
	break;
	
    case MOVE_SETBOARD:
    case MOVE_SETCUBEVAL:
    case MOVE_SETCUBEPOS:
	break;
	
    default:
	assert( FALSE );
    }
    
    /* Delete all games after plGame, and all records after plLastMove. */
    PopGame( plGame, FALSE );
    /* FIXME when we can handle variations, we should save the old moves
       as an alternate variation. */
    PopMoveRecord( plLastMove->plNext );

    if( pmr->mt == MOVE_NORMAL &&
	( pmrOld = plLastMove->p )->mt == MOVE_SETDICE &&
	pmrOld->sd.fPlayer == pmr->n.fPlayer )
	PopMoveRecord( plLastMove );
    
    /* FIXME perform other elision (e.g. consecutive "set" records) */

#if USE_GTK
    if( fX )
	GTKAddMoveRecord( pmr );
#endif
    
    ApplyMoveRecord( &ms, pmr );
    
    plLastMove = ListInsert( plGame, pmr );

    SetMoveRecord( pmr );
}

extern void SetMoveRecord( void *pv ) {

#if USE_GTK
    if( fX )
	GTKSetMoveRecord( pv );
#endif
}

extern void ClearMoveRecord( void ) {
    
#if USE_GTK
    if( fX )
	GTKClearMoveRecord();
#endif
    
    plLastMove = plGame = malloc( sizeof( *plGame ) );
    ListCreate( plGame );
}

#if USE_GTK
static guint nTimeout, fDelaying;

static gint DelayTimeout( gpointer p ) {

    if( fDelaying )
	fEndDelay = TRUE;

    nTimeout = 0;
    
    return FALSE;
}

static void ResetDelayTimer( void ) {

    if( fX && nDelay && fDisplay ) {
	if( nTimeout )
	    gtk_timeout_remove( nTimeout );

	nTimeout = gtk_timeout_add( nDelay, DelayTimeout, NULL );
    }
}
#elif USE_EXT && HAVE_SELECT
static struct timeval tvLast;

static void ResetDelayTimer( void ) {
    
    if( fX && nDelay && fDisplay )
	gettimeofday( &tvLast, NULL );
}
#else
#define ResetDelayTimer()
#endif

extern void AddGame( moverecord *pmr ) {
    
#if USE_GTK
    char sz[ 32 ];
    
    if( fX ) {
	sprintf( sz, "Game %d: %d-%d", pmr->g.i + 1, pmr->g.anScore[ 0 ],
		 pmr->g.anScore[ 1 ] );
	GTKAddGame( sz );
    }
#endif
    
    assert( pmr->mt == MOVE_GAMEINFO );
    
    ms.cGames++;
}

static void NewGame( void ) {

    moverecord *pmr;
    
    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.sz = NULL;
    pmr->g.i = ms.cGames;
    pmr->g.nMatch = ms.nMatchTo;
    pmr->g.anScore[ 0 ] = ms.anScore[ 0 ];
    pmr->g.anScore[ 1 ] = ms.anScore[ 1 ];
    pmr->g.fCrawford = fAutoCrawford && ms.nMatchTo > 1;
    pmr->g.fCrawfordGame = ms.fCrawford;
    pmr->g.fJacoby = fJacoby && !ms.nMatchTo;
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
        
    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    
 reroll:
    RollDice( ms.anDice );

    if( fInterrupt ) {
	PopMoveRecord( plGame->plNext );

	free( plGame );
	ListDelete( lMatch.plPrev );

	return;
    }
    
    if( fDisplay ) {
      outputnew();
      outputf( "%s rolls %d, %s rolls %d.\n", ap[ 0 ].szName, ms.anDice[ 0 ],
               ap[ 1 ].szName, ms.anDice[ 1 ] );
    }

    if( ms.anDice[ 0 ] == ms.anDice[ 1 ] && ms.nCube < MAX_CUBE ) {
	if( !ms.nMatchTo && ms.nCube < ( 1 << cAutoDoubles ) && fCubeUse ) {
	    pmr->g.nAutoDoubles++;
	    if( fDisplay )
		outputf( "The cube is now at %d.\n", ms.nCube <<= 1 );
	    UpdateSetting( &ms.nCube );
	}
	
	goto reroll;
    }

    outputx();
    
    AddGame( pmr );
    
    pmr = malloc( sizeof( pmr->sd ) );
    pmr->mt = MOVE_SETDICE;
    pmr->sd.sz = NULL;
    pmr->sd.anDice[ 0 ] = ms.anDice[ 0 ];
    pmr->sd.anDice[ 1 ] = ms.anDice[ 1 ];
    pmr->sd.fPlayer = ms.anDice[ 1 ] > ms.anDice[ 0 ];
    pmr->sd.lt = LUCK_NONE;
    pmr->sd.rLuck = -HUGE_VALF;
    AddMoveRecord( pmr );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    
#if USE_GUI
    if( fX && fDisplay )
	ShowBoard();

    ResetDelayTimer();
#endif
}

static void ShowAutoMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    char sz[ 40 ];

    if( anMove[ 0 ] == -1 )
	outputf( "%s cannot move.\n", ap[ ms.fTurn ].szName );
    else
	outputf( "%s moves %s.\n", ap[ ms.fTurn ].szName,
		 FormatMove( sz, anBoard, anMove ) );
}

extern int ComputerTurn( void ) {

  movenormal *pmn;
  cubeinfo ci;
  float arDouble[ 4 ], arOutput[ NUM_OUTPUTS ], rDoublePoint;
  char szBoard[ 256 ], szResponse[ 256 ];
  int i, c;

  if( fAction )
      fnAction();

  if( fInterrupt || ms.gs != GAME_PLAYING )
      return -1;

  GetMatchStateCubeInfo( &ci, &ms );

  switch( ap[ ms.fTurn ].pt ) {
  case PLAYER_GNU:
    if( ms.fResigned ) {

      float rEqBefore, rEqAfter;

      if( EvaluatePosition( ms.anBoard, arOutput, &ci,
			    &ap[ ms.fTurn ].esCube.ec ) )
        return -1;

      rEqBefore = -Utility ( arOutput, &ci );

      /* I win 100% if opponent resigns */
      arOutput[ 0 ] = 1.0; 
      arOutput[ 1 ] = arOutput[ 2 ] =
        arOutput[ 3 ] = arOutput[ 4 ] = 0.0;

      if( ms.fResigned >= 2 )
	  /* resigned at least a gammon */
	  arOutput[ 1 ] = 1.0;

      if( ms.fResigned == 3 )
	  /* resigned a backgammon */
	  arOutput[ 2 ] = 1.0;

      InvertEvaluation ( arOutput );
      
      rEqAfter = -Utility ( arOutput, &ci );

      /*
      printf ("equity before resignation: %7.3f\n"
              "equity after resignation : %7.3f\n",
              rEqBefore, rEqAfter );
      */

      fComputerDecision = TRUE;

      if( rEqAfter >= rEqBefore )
        CommandAgree( NULL );
      else
        CommandDecline( NULL );
      
      fComputerDecision = FALSE;
      return 0;
    } else if( ms.fDoubled ) {

      /* Consider cube action */

      if ( EvaluatePositionCubeful ( ms.anBoard, arDouble, arOutput, &ci,
                                     &ap [ ms.fTurn ].esCube.ec,
                                     ap [ ms.fTurn ].esCube.ec.nPlies ) < 0 )
        return -1;

      fComputerDecision = TRUE;
      
      if ( fBeavers && ! ms.nMatchTo && arDouble[ 2 ] <= 0.0 &&
	   ms.nCube < ( MAX_CUBE >> 1 ) ) 
        /* It's a beaver... beaver all night! */
        CommandRedouble ( NULL );
      else if ( arDouble[ 2 ] <= arDouble[ 3 ] )
        CommandTake ( NULL );
      else
        CommandDrop ( NULL );
      
      fComputerDecision = FALSE;
      
      return 0;

    } else {
      int anBoardMove[ 2 ][ 25 ];
	    

      /* Don't use the global board for this call, to avoid
	 race conditions with updating the board and aborting the
	 move with an interrupt. */
      memcpy( anBoardMove, ms.anBoard, sizeof( anBoardMove ) );

      /* Consider doubling */

      if ( fCubeUse && !ms.anDice[ 0 ] && ms.nCube < MAX_CUBE &&
	   GetDPEq ( NULL, NULL, &ci ) ) {
	  evalcontext ecDH;

	  memcpy( &ecDH, &ap[ ms.fTurn ].esCube.ec, sizeof ecDH );
	  ecDH.fCubeful = FALSE;
          if ( ecDH.nPlies ) ecDH.nPlies--;
        
        /* We have access to the cube */

        /* Determine market window */

        if ( EvaluatePosition ( anBoardMove, arOutput, &ci, &ecDH ) )
          return -1;

        rDoublePoint = 
          GetDoublePointDeadCube ( arOutput, &ci );

        if ( arOutput[ 0 ] >= rDoublePoint ) {

          /* We're in market window */

          if( EvaluatePositionCubeful( ms.anBoard, arDouble, arOutput, &ci,
				       &ap [ ms.fTurn ].esCube.ec,
				       ap [ ms.fTurn ].esCube.ec.nPlies ) < 0 )
            return -1;

          if ( ( arDouble[ 3 ] >= arDouble[ 1 ] ) &&
               ( arDouble[ 2 ] >= arDouble[ 1 ] ) ) {
	      fComputerDecision = TRUE;
	      CommandDouble ( NULL );
	      fComputerDecision = FALSE;
	      return 0;
          }
        } /* market window */
      } /* access to cube */

      /* Roll dice and move */
      if ( !ms.anDice[ 0 ] ) {
	  if( RollDice ( ms.anDice ) < 0 )
	      return -1;
	  
	  ResetDelayTimer(); /* Start the timer again -- otherwise the time
				we spent contemplating the cube could replace
				the delay. */
	
	  /* write line to status bar if using GTK */
#ifdef USE_GTK        
	  if ( fX ) {

	      outputnew ();
	      outputf ( "%s rolls %1i and %1i.\n",
			ap [ ms.fTurn ].szName, ms.anDice[ 0 ],
			ms.anDice[ 1 ] );
	      outputx ();
	      
	  }
#endif
      }

      
      if ( fDisplay )
	  ShowBoard();

      pmn = malloc( sizeof( *pmn ) );
      pmn->mt = MOVE_NORMAL;
      pmn->sz = NULL;
      pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
      pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
      pmn->fPlayer = ms.fTurn;
      pmn->ml.cMoves = 0;
      pmn->ml.amMoves = NULL;
      pmn->etDouble = EVAL_NONE;
      pmn->lt = LUCK_NONE;
      pmn->rLuck = -HUGE_VALF;
      pmn->st = SKILL_NONE;
      
      if( FindBestMove( pmn->anMove, ms.anDice[ 0 ], ms.anDice[ 1 ],
                        anBoardMove, &ci,
			&ap[ ms.fTurn ].esChequer.ec ) < 0 ) {
        free( pmn );
        return -1;
      }
      
      /* write move to status bar if using GTK */
#ifdef USE_GTK        
      if ( fX ) {
	  
	  outputnew ();
	  ShowAutoMove( anBoardMove, pmn->anMove );
	  outputx ();
      }
#endif
      
      AddMoveRecord( pmn );      
      
      return 0;
    }
    
  case PLAYER_PUBEVAL:
    if( ms.fResigned == 3 ) {
      fComputerDecision = TRUE;
      CommandAgree( NULL );
      fComputerDecision = FALSE;
      return 0;
    } else if( ms.fResigned ) {
      fComputerDecision = TRUE;
      CommandDecline( NULL );
      fComputerDecision = FALSE;
      return 0;
    } else if( ms.fDoubled ) {
      fComputerDecision = TRUE;
      CommandTake( NULL );
      fComputerDecision = FALSE;
      return 0;
    } else if( !ms.anDice[ 0 ] ) {
      if( RollDice( ms.anDice ) < 0 )
	    return -1;
      
      if( fDisplay )
        ShowBoard();
    }
    
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->etDouble = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = -HUGE_VALF;
    pmn->st = SKILL_NONE;
    
    FindPubevalMove( ms.anDice[ 0 ], ms.anDice[ 1 ], ms.anBoard, pmn->anMove );
    
    AddMoveRecord( pmn );
    return 0;

  case PLAYER_EXTERNAL:
#if HAVE_SOCKETS
      if( ms.fResigned == 3 ) {
	  /* FIXME get resignation decision */
	  fComputerDecision = TRUE;
	  CommandAgree( NULL );
	  fComputerDecision = FALSE;
	  return 0;
      } else if( ms.fResigned ) {
	  /* FIXME get resignation decision */
	  fComputerDecision = TRUE;
	  CommandDecline( NULL );
	  fComputerDecision = FALSE;
	  return 0;
      } else if( ms.fDoubled ) {
	  /* FIXME get take decision */
	  fComputerDecision = TRUE;
	  CommandTake( NULL );
	  fComputerDecision = FALSE;
	  return 0;
      } else if( !ms.anDice[ 0 ] ) {
	  /* FIXME get double decision (check cube use on, cube access, and
	     Crawford) */
	  if( RollDice( ms.anDice ) < 0 )
	      return -1;
	  
	  if( fDisplay )
	      ShowBoard();
      }

      FIBSBoard( szBoard, ms.anBoard, ms.fMove, ap[ 1 ].szName,
		 ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		 ms.anScore[ 0 ], ms.anDice[ 0 ], ms.anDice[ 1 ], ms.nCube,
		 ms.fCubeOwner, ms.fDoubled, ms.fTurn, ms.fCrawford );
      strcat( szBoard, "\n" );
      
      if( ExternalWrite( ap[ ms.fTurn ].h, szBoard,
			 strlen( szBoard ) + 1 ) < 0 )
	  return -1;

      if( ExternalRead( ap[ ms.fTurn ].h, szResponse,
			sizeof( szResponse ) ) < 0 )
	  return -1;
      
      pmn = malloc( sizeof( *pmn ) );
      pmn->mt = MOVE_NORMAL;
      pmn->sz = NULL;
      pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
      pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
      pmn->fPlayer = ms.fTurn;
      pmn->ml.cMoves = 0;
      pmn->ml.amMoves = NULL;
      pmn->etDouble = EVAL_NONE;
      pmn->lt = LUCK_NONE;
      pmn->rLuck = -HUGE_VALF;
      pmn->st = SKILL_NONE;
      
      if( ( c = ParseMove( szResponse, pmn->anMove ) ) < 0 ) {
	  pmn->anMove[ 0 ] = 0;
	  outputl( "Warning: badly formed move from external player" );
      } else
	  for( i = 0; i < 4; i++ )
	      if( i < c ) {
		  pmn->anMove[ i << 1 ]--;
		  pmn->anMove[ ( i << 1 ) + 1 ]--;
	      } else {
		  pmn->anMove[ i << 1 ] = -1;
		  pmn->anMove[ ( i << 1 ) + 1 ] = -1;
	      }
      
      AddMoveRecord( pmn );
      return 0;
#else
      /* fall through */
#endif
      
  case PLAYER_HUMAN:
      /* fall through */
      ;
  }
  
  assert( FALSE );
  return -1;
}

extern void CancelCubeAction( void ) {
    
    if( ms.fDoubled ) {
	outputf( "(%s's double has been cancelled.)\n",
		 ap[ ms.fMove ].szName );
	ms.fDoubled = FALSE;

	if( fDisplay )
	    ShowBoard();

	/* FIXME should fTurn be set to fMove? */
	TurnDone(); /* FIXME is this right? */
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
    
    if( ClassifyPosition( ms.anBoard ) > CLASS_RACE )
	/* It's a contact position; don't automatically bear off */
	return -1;
    
    GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ], FALSE );

    cMoves = ( ms.anDice[ 0 ] == ms.anDice[ 1 ] ) ? 4 : 2;
    
    for( i = 0; i < ml.cMoves; i++ )
	for( iMove = 0; iMove < cMoves; iMove++ )
	    if( ( ml.amMoves[ i ].anMove[ iMove << 1 ] < 0 ) ||
		( ml.amMoves[ i ].anMove[ ( iMove << 1 ) + 1 ] != -1 ) )
		break;
	    else if( iMove == cMoves - 1 ) {
		/* All dice bear off */
		pmn = malloc( sizeof( *pmn ) );
		pmn->mt = MOVE_NORMAL;
		pmn->sz = NULL;
		pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
		pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
		pmn->fPlayer = ms.fTurn;
		pmn->ml.cMoves = 0;
		pmn->ml.amMoves = NULL;
		pmn->etDouble = EVAL_NONE;
		pmn->lt = LUCK_NONE;
		pmn->rLuck = -HUGE_VALF;
		pmn->st = SKILL_NONE;
		
		memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
			sizeof( pmn->anMove ) );
		
		ShowAutoMove( ms.anBoard, pmn->anMove );
		
		AddMoveRecord( pmn );

		return 0;
	    }

    return -1;
}

extern int NextTurn( int fPlayNext ) {

    int n;
#if USE_EXT && HAVE_SELECT
    struct timeval tv;
    fd_set fds;
#endif
    
#if USE_GUI
    if( fX ) {
#if USE_GTK
	if( nNextTurn ) {
	    gtk_idle_remove( nNextTurn );
	    nNextTurn = 0;
	} else
	    return -1;
#else
	if( evNextTurn.fPending )
	    EventPending( &evNextTurn, FALSE );
	else
	    return -1;
#endif
    } else
#endif
	if( fNextTurn )
	    fNextTurn = FALSE;
	else
	    return -1;
    
#if USE_GTK
    if( fX && nDelay && fDisplay && nTimeout ) {
	fDelaying = TRUE;
	GTKDelay();
	fDelaying = FALSE;
	ResetDelayTimer();
    }    
#elif USE_EXT && HAVE_SELECT
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
		FD_SET( ConnectionNumber( ewnd.pdsp ), &fds );
		if( select( ConnectionNumber( ewnd.pdsp ) + 1, &fds, NULL,
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

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    
    if( ( n = GameStatus( ms.anBoard ) ) ||
	( ms.gs == GAME_DROP && ( ( n = 1 ) ) ) ||
	( ms.gs == GAME_RESIGNED && ( ( n = ms.fResigned ) ) ) ) {
	movegameinfo *pmgi = plGame->plNext->p;
	
	if( fJacoby && ms.fCubeOwner == -1 && !ms.nMatchTo )
	    /* gammons do not count on a centred cube during money
	       sessions under the Jacoby rule */
	    n = 1;
	
	outputf( "%s wins a %s and %d point%s.\n", ap[ pmgi->fWinner ].szName,
		 aszGameResult[ n - 1 ], pmgi->nPoints,
		 pmgi->nPoints > 1 ? "s" : "" );

#if USE_GUI
	if( fX ) {
	    if( fDisplay )
		ShowBoard();
	    else
		outputx();
	}
#endif
	
	if( ms.nMatchTo && fAutoCrawford ) {
	    ms.fPostCrawford |= ms.fCrawford &&
		ms.anScore[ pmgi->fWinner ] < ms.nMatchTo;
	    ms.fCrawford = !ms.fPostCrawford && !ms.fCrawford &&
		ms.anScore[ pmgi->fWinner ] == ms.nMatchTo - 1 &&
		ms.anScore[ !pmgi->fWinner ] != ms.nMatchTo - 1;
	}

#if USE_GUI
	if( !fX || fDisplay )
#endif
	    CommandShowScore( NULL );
	
	if( ms.nMatchTo && ms.anScore[ pmgi->fWinner ] >= ms.nMatchTo ) {
	    outputf( "%s has won the match.\n", ap[ pmgi->fWinner ].szName );
	    outputx();
	    return -1;
	}

	outputx();
	
	if( fAutoGame ) {
	    NewGame();
	    
	    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN )
		ShowBoard();
	} else
	    return -1;
    }

    assert( ms.gs == GAME_PLAYING );
    
    if( fDisplay || ap[ ms.fTurn ].pt == PLAYER_HUMAN )
	ShowBoard();

    /* We have reached a safe point to check for interrupts.  Until now,
       the board could have been in an inconsistent state. */
    if( fAction )
	fnAction();
	
    if( fInterrupt || !fPlayNext )
	return -1;
    
    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN ) {
	/* Roll for them, if:

	   * "auto roll" is on;
	   * they haven't already rolled;
	   * they haven't just been doubled;
	   * somebody hasn't just resigned;
	   * at least one of the following:
	     - cube use is disabled;
	     - it's the Crawford game;
	     - the cube is dead. */
	if( fAutoRoll && !ms.anDice[ 0 ] && !ms.fDoubled && !ms.fResigned &&
	    ( !fCubeUse || ms.fCrawford ||
	      ( ms.fCubeOwner >= 0 && ms.fCubeOwner != ms.fTurn ) ||
	      ( ms.nMatchTo > 0 && ms.anScore[ ms.fTurn ] +
		ms.nCube >= ms.nMatchTo ) ) )
	    CommandRoll( NULL );
	return -1;
    }
    
#if USE_GUI
    if( fX ) {
#if USE_GTK
	if( !ComputerTurn() && !nNextTurn )
	    nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
#else
	EventPending( &evNextTurn, !ComputerTurn() );	    
#endif
    } else
#endif
	fNextTurn = !ComputerTurn();

    return 0;
}

extern void TurnDone( void ) {

#if USE_GUI
    if( fX ) {
#if USE_GTK
	if( !nNextTurn )
	    nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
#else
	EventPending( &evNextTurn, TRUE );    
#endif
    } else
#endif
	fNextTurn = TRUE;

    outputx();
}

extern void CommandAccept( char *sz ) {

    if( ms.fResigned )
	CommandAgree( sz );
    else if( ms.fDoubled )
	CommandTake( sz );
    else
	outputl( "You can only accept if the cube or a resignation has been "
		 "offered." );
}

extern void CommandAgree( char *sz ) {

    moveresign *pmr;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( !ms.fResigned ) {
	outputl( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	outputf( "%s accepts and wins a %s.\n", ap[ ms.fTurn ].szName,
		aszGameResult[ ms.fResigned - 1 ] );

    pmr = malloc( sizeof( *pmr ) );
    pmr->mt = MOVE_RESIGN;
    pmr->sz = NULL;
    pmr->fPlayer = !ms.fTurn;
    pmr->nResigned = ms.fResigned;
    AddMoveRecord( pmr );

    TurnDone();
}

static void AnnotateMove( skilltype st ) {

    moverecord *pmr;

    if( !( pmr = plLastMove->plNext->p ) ) {
	outputl( "You must select a move to annotate first." );
	return;
    }

    switch( pmr->mt ) {
    case MOVE_NORMAL:
	pmr->n.st = st;
	break;
	
    case MOVE_DOUBLE:
    case MOVE_TAKE:
    case MOVE_DROP:
	pmr->d.st = st;
	break;
	
    default:
	outputl( "You cannot annotate this move." );
	return;
    }

    outputf( "Move marked as %s.\n", aszSkillType[ st ] );
}

static void AnnotateRoll( lucktype lt ) {

    moverecord *pmr;

    if( !( pmr = plLastMove->plNext->p ) ) {
	outputl( "You must select a move to annotate first." );
	return;
    }

    switch( pmr->mt ) {
    case MOVE_NORMAL:
	pmr->n.lt = lt;
	break;
	
    case MOVE_SETDICE:
	pmr->sd.lt = lt;
	break;
	
    default:
	outputl( "You cannot annotate this move." );
	return;
    }

    outputf( "Roll marked as %s.\n", aszLuckType[ lt ] );
}

extern void CommandAnnotateBad( char *sz ) {

    AnnotateMove( SKILL_BAD );
}

extern void CommandAnnotateClear( char *sz ) {

    CommandNotImplemented( NULL ); /* FIXME */
}

extern void CommandAnnotateDoubtful( char *sz ) {

    AnnotateMove( SKILL_DOUBTFUL );
}

extern void CommandAnnotateGood( char *sz ) {

    AnnotateMove( SKILL_GOOD );
}

extern void CommandAnnotateInteresting( char *sz ) {

    AnnotateMove( SKILL_INTERESTING );
}

extern void CommandAnnotateLucky( char *sz ) {

    AnnotateRoll( LUCK_GOOD );
}

extern void CommandAnnotateUnlucky( char *sz ) {

    AnnotateRoll( LUCK_BAD );
}

extern void CommandAnnotateVeryBad( char *sz ) {

    AnnotateMove( SKILL_BAD );
}

extern void CommandAnnotateVeryGood( char *sz ) {

    AnnotateMove( SKILL_GOOD );
}

extern void CommandAnnotateVeryLucky( char *sz ) {

    AnnotateRoll( LUCK_GOOD );
}

extern void CommandAnnotateVeryUnlucky( char *sz ) {

    AnnotateRoll( LUCK_BAD );
}

extern void CommandDecline( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( !ms.fResigned ) {
	outputl( "No resignation was offered." );

	return;
    }

    if( fDisplay )
	outputf( "%s declines the %s.\n", ap[ ms.fTurn ].szName,
		aszGameResult[ ms.fResigned - 1 ] );

    ms.fResigned = FALSE;
    ms.fTurn = !ms.fTurn;
    
    TurnDone();
}

extern void CommandDouble( char *sz ) {

    moverecord *pmr;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( ms.fCrawford ) {
	outputl( "Doubling is forbidden by the Crawford rule (see `help set "
	      "crawford')." );

	return;
    }

    if( !fCubeUse ) {
	outputl( "The doubling cube has been disabled (see `help set cube "
	      "use')." );

	return;
    }

    if( ms.fDoubled ) {
	outputl( "The `double' command is for offering the cube, not "
		 "accepting it.  Use\n`redouble' to immediately offer the "
		 "cube back at a higher value." );

	return;
    }
    
    if( ms.fTurn != ms.fMove ) {
	outputl( "You are only allowed to double if you are on roll." );

	return;
    }
    
    if( ms.anDice[ 0 ] ) {
	outputl( "You can't double after rolling the dice -- wait until your "
	      "next turn." );

	return;
    }

    if( ms.fCubeOwner >= 0 && ms.fCubeOwner != ms.fTurn ) {
	outputl( "You do not own the cube." );

	return;
    }

    if( ms.nCube >= MAX_CUBE ) {
	outputl( "The cube is already at " MAX_CUBE_STR "; you can't double "
		 "any more." );
	return;
    }
    
    if( fDisplay )
	outputf( "%s doubles.\n", ap[ ms.fTurn ].szName );

#if USE_GTK
    /* There's no point delaying here. */
    if( nTimeout ) {
	gtk_timeout_remove( nTimeout );
	nTimeout = 0;
    }
#endif
    
    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_DOUBLE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    pmr->d.etDouble = EVAL_NONE;
    pmr->d.st = SKILL_NONE;
    AddMoveRecord( pmr );
    
    TurnDone();
}

extern void CommandDrop( char *sz ) {

    moverecord *pmr;
    
    if( ms.gs != GAME_PLAYING || !ms.fDoubled ) {
	outputl( "The cube must have been offered before you can drop it." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( fDisplay )
	outputf( "%s refuses the cube and gives up %d point%s.\n",
		ap[ ms.fTurn ].szName, ms.nCube, ms.nCube == 1 ? "" : "s" );
    
    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_DROP;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    pmr->d.etDouble = EVAL_NONE;
    pmr->d.st = SKILL_NONE;

    AddMoveRecord( pmr );
    
    TurnDone();
}

extern void CommandListGame( char *sz ) {
#if USE_GTK
    if( fX ) {
	ShowGameWindow();
	return;
    }
#endif

    /* FIXME */
}

extern void CommandListMatch( char *sz ) {
#if USE_GTK
    if( fX ) {
	ShowGameWindow();
	return;
    }
#endif

    /* FIXME */
}

extern void 
CommandMove( char *sz ) {

  int c, i, j, anBoardNew[ 2 ][ 25 ], anBoardTest[ 2 ][ 25 ],
    an[ 8 ];
  movelist ml;
  movenormal *pmn;
    
  if( ms.gs != GAME_PLAYING ) {
    outputl( "No game in progress (type `new game' to start one)." );

    return;
  }

  if( ap[ ms.fTurn ].pt != PLAYER_HUMAN ) {
    outputl( "It is the computer's turn -- type `play' to force it to "
             "move immediately." );
    return;
  }

  if( !ms.anDice[ 0 ] ) {
    outputl( "You must roll the dice before you can move." );

    return;
  }

  if( ms.fResigned ) {
    outputf( "Please wait for %s to consider the resignation before "
             "moving.\n", ap[ ms.fTurn ].szName );

    return;
  }

  if( ms.fDoubled ) {
    outputf( "Please wait for %s to consider the cube before "
             "moving.\n", ap[ ms.fTurn ].szName );

    return;
  }
    
  if( !*sz ) {
    GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ], FALSE );

    if( ml.cMoves <= 1 ) {
	pmn = malloc( sizeof( *pmn ) );
	pmn->mt = MOVE_NORMAL;
	pmn->sz = NULL;
	pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
	pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
	pmn->fPlayer = ms.fTurn;
	pmn->ml.cMoves = 0;
	pmn->ml.amMoves = NULL;
	pmn->etDouble = EVAL_NONE;
	pmn->lt = LUCK_NONE;
	pmn->rLuck = -HUGE_VALF;
	pmn->st = SKILL_NONE;
	
	if( ml.cMoves )
	    memcpy( pmn->anMove, ml.amMoves[ 0 ].anMove,
		    sizeof( pmn->anMove ) );
	else
	    pmn->anMove[ 0 ] = -1;
	
	ShowAutoMove( ms.anBoard, pmn->anMove );
	
	AddMoveRecord( pmn );
	
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
	    anBoardNew[ 0 ][ i ] = ms.anBoard[ 0 ][ i ];
	    anBoardNew[ 1 ][ i ] = ms.anBoard[ 1 ][ i ];
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
	
    GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ], FALSE );
	
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
	pmn->sz = NULL;
        pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
        pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
        pmn->fPlayer = ms.fTurn;
	pmn->ml.cMoves = 0;
	pmn->ml.amMoves = NULL;
	pmn->etDouble = EVAL_NONE;
	pmn->lt = LUCK_NONE;
	pmn->rLuck = -HUGE_VALF;
	pmn->st = SKILL_NONE;
        memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
                sizeof( pmn->anMove ) );
		
#ifdef USE_GTK        
        if ( fX ) {

          outputnew ();
	  ShowAutoMove( ms.anBoard, pmn->anMove );
          outputx ();
	}
#endif

        AddMoveRecord( pmn );
        TurnDone();
		
        return;
	    }
    }
  }

  outputl( "Illegal move." );
}

extern void CommandNewGame( char *sz ) {

    list *pl;
    
    if( ms.nMatchTo && ( ms.anScore[ 0 ] >= ms.nMatchTo ||
			 ms.anScore[ 1 ] >= ms.nMatchTo ) ) {
	outputl( "The match is already over." );

	return;
    }

    if( ms.gs == GAME_PLAYING ) {
	if( fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to start a new game, "
			     "and discard the one in progress? " ) )
		return;
	}

	/* Delete all games at the _end_ of the match, back to and including
	   the current one. */
	do {
	    pl = lMatch.plPrev->p;
	    ListDelete( lMatch.plPrev );
	    FreeGame( pl );
	    ms.cGames--;
	} while( pl != plGame );
    }
    
    NewGame();

    if( fInterrupt )
	return;
    
    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN )
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

    IniStatcontext( &scMatch );
}

extern void CommandNewMatch( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( "You must specify a valid match length (1 or longer)." );

	return;
    }

    /* Check that match equity table is large enough */

    if ( n > MAXSCORE ) {
       outputf ( "GNU Backgammon is compiled with support only for "
                 "matches of length %i\n"
                 "and below\n",
                 MAXSCORE );
       return;
    }

    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( "Are you sure you want to start a new match, "
			 "and discard the game in progress? " ) )
	    return;
    }
    
    FreeMatch();

    ms.nMatchTo = n;

    ms.cGames = ms.anScore[ 0 ] = ms.anScore[ 1 ] = 0;
    ms.fTurn = -1;
    ms.gs = GAME_NONE;
    ms.fCrawford = FALSE;
    ms.fPostCrawford = FALSE;

    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    outputf( "A new %d point match has been started.\n", n );

#if USE_GUI
    if( fX )
	ShowBoard();
#endif

    if( fAutoGame )
	CommandNewGame( NULL );
}

extern void CommandNewSession( char *sz ) {

    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( "Are you sure you want to start a new session, "
			 "and discard the game in progress? " ) )
	    return;
    }
    
    FreeMatch();

    ms.cGames = ms.nMatchTo = ms.anScore[ 0 ] = ms.anScore[ 1 ] = 0;
    ms.fTurn = -1;
    ms.gs = GAME_NONE;
    ms.fCrawford = 0;
    ms.fPostCrawford = 0;

    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    outputl( "A new session has been started." );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
    
    if( fAutoGame )
	CommandNewGame( NULL );
}

static void UpdateGame( void ) {
    
    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    
    ShowBoard();
}

extern void ChangeGame( list *plGameNew ) {

#if USE_GTK
    list *pl;
#endif
    
    plLastMove = ( plGame = plGameNew )->plNext;
    
#if USE_GTK
    if( fX ) {
	GTKClearMoveRecord();

	for( pl = plGame->plNext; pl->p; pl = pl->plNext ) {
	    GTKAddMoveRecord( pl->p );
	    ApplyMoveRecord( &ms, pl->p );
	}

	GTKSetGame( ( (moverecord *) plGame->plNext->p )->g.i );
    }
#endif
    
    CalculateBoard();
    
    UpdateGame();

    SetMoveRecord( plLastMove->p );
}

static void CommandNextGame( char *sz ) {

    int n;
    char *pch;
    list *pl;
    
    if( ( pch = NextToken( &sz ) ) )
	n = ParseNumber( &pch );
    else
	n = 1;

    if( n < 1 ) {
	outputl( "If you specify a parameter to the `next game' command, it "
		 "must be a positive number (the count of games to step "
		 "ahead)." );
	return;
    }

    for( pl = lMatch.plNext; pl->p != plGame; pl = pl->plNext )
	;
    
    for( ; n && pl->plNext->p; n--, pl = pl->plNext )
	;

    if( pl->p == plGame )
	return;

    ChangeGame( pl->p );
}

extern void CommandNext( char *sz ) {

    int n;
    char *pch;
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( ( pch = NextToken( &sz ) ) ) {
	if( !strncasecmp( pch, "game", strlen( pch ) ) ) {
	    CommandNextGame( sz );
	    return;
	} else
	    n = ParseNumber( &pch );
    } else
	n = 1;
    
    if( n < 1 ) {
	outputl( "If you specify a parameter to the `next' command, it must "
		 "be a positive number (the count of moves to step ahead)." );
	return;
    }
    
    while( n-- && plLastMove->plNext->p ) {
	plLastMove = plLastMove->plNext;
	ApplyMoveRecord( &ms, plLastMove->p );
    }
    
    UpdateGame();

    SetMoveRecord( plLastMove->p );
}

extern void CommandPlay( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN ) {
	outputl( "It's not the computer's turn to play." );

	return;
    }

    if( !ComputerTurn() )
	TurnDone();
}

static void CommandPreviousGame( char *sz ) {

    int n;
    char *pch;
    list *pl;
    
    if( ( pch = NextToken( &sz ) ) )
	n = ParseNumber( &pch );
    else
	n = 1;

    if( n < 1 ) {
	outputl( "If you specify a parameter to the `previous game' command, "
		 "it must be a positive number (the count of games to step "
		 "back)." );
	return;
    }
    
    for( pl = lMatch.plNext; pl->p != plGame; pl = pl->plNext )
	;
    
    for( ; n && pl->plPrev->p; n--, pl = pl->plPrev )
	;

    if( pl->p == plGame )
	return;

    ChangeGame( pl->p );
}

extern void CommandPrevious( char *sz ) {

    int n;
    char *pch;
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( ( pch = NextToken( &sz ) ) ) {
	if( !strncasecmp( pch, "game", strlen( pch ) ) ) {
	    CommandPreviousGame( sz );
	    return;
	} else
	    n = ParseNumber( &pch );
    } else
	n = 1;
    
    if( n < 1 ) {
	outputl( "If you specify a parameter to the `previous' command, it "
		 "must be a positive number (the count of moves to step "
		 "back)." );
	return;
    }

    while( n-- && plLastMove->plPrev->p )
	plLastMove = plLastMove->plPrev;

    CalculateBoard();

    UpdateGame();

    SetMoveRecord( plLastMove->p );
}

extern void CommandRedouble( char *sz ) {

    moverecord *pmr;

    if( ms.nMatchTo > 0 ) {
	outputl( "Redoubles are not permitted during match play." );

	return;
    }
    
    if( ms.gs != GAME_PLAYING || !ms.fDoubled ) {
	outputl( "The cube must have been offered before you can redouble "
		 "it." );

	return;
    }
    
    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( ms.nCube >= ( MAX_CUBE >> 1 ) ) {
	outputl( "The cube is already at " MAX_CUBE_STR "; you can't double "
		 "any more." );
	return;
    }
    
    if( fDisplay )
	outputf( "%s accepts and immediately redoubles to %d.\n",
		ap[ ms.fTurn ].szName, ms.nCube << 2 );
    
    ms.fCubeOwner = !ms.fMove;
    UpdateSetting( &ms.fCubeOwner );
    
    pmr = malloc( sizeof( pmr->d ) );
    pmr->mt = MOVE_DOUBLE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    pmr->d.etDouble = EVAL_NONE;
    pmr->d.st = SKILL_NONE;
    AddMoveRecord( pmr );
    
    TurnDone();
}

extern void CommandReject( char *sz ) {

    if( ms.fResigned )
	CommandDecline( sz );
    else if( ms.fDoubled )
	CommandDrop( sz );
    else
	outputl( "You can only reject if the cube or a resignation has been "
	      "offered." );
}

extern void CommandResign( char *sz ) {

    char *pch;
    int cch;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "You must be playing a game if you want to resign it." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    /* FIXME cancel cube action?  or refuse resignations while doubled?
     or treat resignations while doubled as drops? */
    
    if( ( pch = NextToken( &sz ) ) ) {
	cch = strlen( pch );

	if( !strncasecmp( "single", pch, cch ) ||
	    !strncasecmp( "normal", pch, cch ) )
	    ms.fResigned = 1;
	else if( !strncasecmp( "gammon", pch, cch ) )
	    ms.fResigned = 2;
	else if( !strncasecmp( "backgammon", pch, cch ) )
	    ms.fResigned = 3;
	else
	    ms.fResigned = atoi( pch );
    } else
	ms.fResigned = 1;

    if( ms.fResigned < 1 || ms.fResigned > 3 ) {
	ms.fResigned = 0;

	outputf( "Unknown keyword `%s' -- try `help resign'.\n", pch );
	
	return;
    }
    
    if( fDisplay )
	outputf( "%s offers to resign a %s.\n", ap[ ms.fTurn ].szName,
		aszGameResult[ ms.fResigned - 1 ] );

    ms.fTurn = !ms.fTurn;
    
    TurnDone();
}


extern void 
CommandRoll( char *sz ) {

  movelist ml;
  movenormal *pmn;
  moverecord *pmr;
  
  if( ms.gs != GAME_PLAYING ) {
    outputl( "No game in progress (type `new game' to start one)." );

    return;
  }
    
  if( ap[ ms.fTurn ].pt != PLAYER_HUMAN ) {
    outputl( "It is the computer's turn -- type `play' to force it to "
             "move immediately." );
    return;
  }

  if( ms.fDoubled ) {
    outputf( "Please wait for %s to consider the cube before "
             "moving.\n", ap[ ms.fTurn ].szName );

    return;
  }    

  if( ms.fResigned ) {
    outputl( "Please resolve the resignation first." );

    return;
  }

  if( ms.anDice[ 0 ] ) {
    outputl( "You already did roll the dice." );

    return;
  }
    
  if( RollDice( ms.anDice ) < 0 )
    return;

  pmr = malloc( sizeof( pmr->sd ) );
  pmr->mt = MOVE_SETDICE;
  pmr->sd.sz = NULL;
  pmr->sd.anDice[ 0 ] = ms.anDice[ 0 ];
  pmr->sd.anDice[ 1 ] = ms.anDice[ 1 ];
  pmr->sd.fPlayer = ms.fTurn;
  pmr->sd.lt = LUCK_NONE;
  pmr->sd.rLuck = -HUGE_VALF;
  AddMoveRecord( pmr );
  
  ShowBoard();

#ifdef USE_GTK        
  if ( fX ) {

    outputnew ();
    outputf ( "%s rolls %1i and %1i.\n",
              ap [ ms.fTurn ].szName, ms.anDice[ 0 ], ms.anDice[ 1 ] );
    outputx ();

  }
#endif

  ResetDelayTimer();
    
  if( !GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ],
		      FALSE ) ) {
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->anMove[ 0 ] = -1;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->etDouble = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = -HUGE_VALF;
    pmn->st = SKILL_NONE;
    
    ShowAutoMove( ms.anBoard, pmn->anMove );

    AddMoveRecord( pmn );
    TurnDone();
  } else if( ml.cMoves == 1 && ( fAutoMove || ( ClassifyPosition( ms.anBoard )
                                                <= CLASS_BEAROFF1 &&
                                                fAutoBearoff ) ) ) {
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->etDouble = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = -HUGE_VALF;
    pmn->st = SKILL_NONE;
    memcpy( pmn->anMove, ml.amMoves[ 0 ].anMove, sizeof( pmn->anMove ) );

    ShowAutoMove( ms.anBoard, pmn->anMove );
	
    AddMoveRecord( pmn );
    TurnDone();
  } else
    if( fAutoBearoff && !TryBearoff() )
	    TurnDone();
}


extern void CommandTake( char *sz ) {

    moverecord *pmr;
    
    if( ms.gs != GAME_PLAYING || !ms.fDoubled ) {
	outputl( "The cube must have been offered before you can take it." );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( "It is the computer's turn -- type `play' to force it to "
		 "move immediately." );
	return;
    }

    if( fDisplay )
	outputf( "%s accepts the cube at %d.\n", ap[ ms.fTurn ].szName,
		 ms.nCube << 1 );
    
    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_TAKE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    pmr->d.etDouble = EVAL_NONE;
    pmr->d.st = SKILL_NONE;

    AddMoveRecord( pmr );

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    
    TurnDone();
}
