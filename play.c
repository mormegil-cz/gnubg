/*
 * play.c
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
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#define USES_badSkill
#include "analysis.h"
#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "external.h"
#include "eval.h"
#include "positionid.h"
#include "matchid.h"
#include "matchequity.h"
#include "rollout.h"
#include "i18n.h"
#include "sound.h"
#include "renderprefs.h"
#if USE_GTK
#include "gtkboard.h"
#endif

char *aszGameResult[] = { 
  N_ ("single game"), 
  N_ ("gammon"), 
  N_ ("backgammon") 
};
char *aszSkillType[] = { 
  N_("very bad"), 
  N_("bad"), 
  N_("doubtful"), 
  NULL,
  N_("good"), 
/*   N_("interesting"),  */
/*   N_("good"),  */
/*   N_("very good")  */
};
char *aszSkillTypeCommand[] = { 
  "verybad", 
  "bad", 
  "doubtful", 
  "none",
  "good",
/*   "interesting",  */
/*   "good",  */
/*   "verygood" */
 };
char* aszSkillTypeAbbr[] = { "??", "?", "?!", "", ""};

char* aszLuckTypeCommand[] = { 
  "veryunlucky", 
  "unlucky", 
  "none",
  "lucky",
  "verylucky" };
char *aszLuckType[] = { 
  N_("very unlucky"), 
  N_("unlucky"), 
  NULL, 
  N_("lucky"),
  N_("very lucky") };
char *aszLuckTypeAbbr[] = { "--", "-", "", "+", "++" };
list lMatch, *plGame, *plLastMove;
statcontext scMatch;
static int fComputerDecision = FALSE;

typedef enum _annotatetype {
  ANNOTATE_ACCEPT, ANNOTATE_CUBE, ANNOTATE_DOUBLE, ANNOTATE_DROP,
  ANNOTATE_MOVE, ANNOTATE_ROLL, ANNOTATE_RESIGN, ANNOTATE_REJECT 
} annotatetype;

static annotatetype at;

static int
CheatDice ( int anDice[ 2 ], matchstate *pms, const int fBest );

#if USE_GTK
#include "gtkgame.h"

static int anLastMove[ 8 ], fLastMove, fLastPlayer;
#endif

static void
ec2es ( evalsetup *pes, const evalcontext *pec ) {

  pes->et = EVAL_EVAL;
  memcpy ( &pes->ec, pec, sizeof ( evalcontext ) );

}

static int
cmp_matchstate ( const matchstate *pms1, const matchstate *pms2 ) {

  /* check if the match state is for the same move */

  return memcmp ( pms1, pms2, sizeof ( matchstate ) );

}



static void
PlayMove(matchstate* pms, int const anMove[ 8 ], int const fPlayer)
{
  int i, nSrc, nDest;

#if USE_GTK
  if( pms == &ms ) {
    memcpy( anLastMove, anMove, sizeof anLastMove );
    CanonicalMoveOrder( anLastMove );
    fLastPlayer = fPlayer;
    fLastMove = anMove[ 0 ] >= 0;
  }
#endif
    
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

static void
ApplyGameOver(matchstate* pms, const list* plGame)
{
  movegameinfo* pmgi = plGame->plNext->p;

  assert( pmgi->mt == MOVE_GAMEINFO );

  if( pmgi->fWinner < 0 )
    return;
    
  pms->anScore[ pmgi->fWinner ] += pmgi->nPoints;
  pms->cGames++;
}

extern void
ApplyMoveRecord(matchstate* pms, const list* plGame, const moverecord* pmr)
{
    int n;
    movegameinfo *pmgi = plGame->plNext->p;
    /* FIXME this is wrong -- plGame is not necessarily the right game */

    assert( pmr->mt == MOVE_GAMEINFO || pmgi->mt == MOVE_GAMEINFO );
    
    pms->fResigned = pms->fResignationDeclined = 0;
    pms->gs = GAME_PLAYING;
#if USE_GTK
    if( pms == &ms )
	fLastMove = FALSE;
#endif
    
    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	InitBoard( pms->anBoard, pmr->g.bgv );

	pms->nMatchTo = pmr->g.nMatch;
	pms->anScore[ 0 ] = pmr->g.anScore[ 0 ];
	pms->anScore[ 1 ] = pmr->g.anScore[ 1 ];
	pms->cGames = pmr->g.i;
	
	pms->gs = GAME_NONE;
	pms->fMove = pms->fTurn = pms->fCubeOwner = -1;
	pms->anDice[ 0 ] = pms->anDice[ 1 ] = pms->cBeavers = 0;
	pms->fDoubled = FALSE;
	pms->nCube = 1 << pmr->g.nAutoDoubles;
	pms->fCrawford = pmr->g.fCrawfordGame;
	pms->fPostCrawford = !pms->fCrawford &&
	    ( pms->anScore[ 0 ] == pms->nMatchTo - 1 ||
	      pms->anScore[ 1 ] == pms->nMatchTo - 1 );
        pms->bgv = pmr->g.bgv;
        pms->fCubeUse = pmr->g.fCubeUse;
        pms->fJacoby = pmr->g.fJacoby;
	break;
	
    case MOVE_DOUBLE:

	if( pms->fMove < 0 )
	    pms->fMove = pmr->d.fPlayer;
	
	if( pms->nCube >= MAX_CUBE )
	    break;
	
	if( pms->fDoubled ) {
	    pms->cBeavers++;
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
	pms->cBeavers = 0;
	pms->fDoubled = FALSE;
	pms->fCubeOwner = !pms->fMove;
	pms->fTurn = pms->fMove;
	break;

    case MOVE_DROP:
	if( !pms->fDoubled )
	    break;
	
	pms->fDoubled = FALSE;
	pms->cBeavers = 0;
	pms->gs = GAME_DROP;
	pmgi->nPoints = pms->nCube;
	pmgi->fWinner = !pmr->d.fPlayer;
	pmgi->fResigned = FALSE;
	
	ApplyGameOver( pms, plGame );
	break;

    case MOVE_NORMAL:
	pms->fDoubled = FALSE;

	PlayMove( pms, pmr->n.anMove, pmr->n.fPlayer );
	pms->anDice[ 0 ] = pms->anDice[ 1 ] = 0;

	if( ( n = GameStatus( pms->anBoard, pms->bgv ) ) ) {

            if( pms->fJacoby && pms->fCubeOwner == -1 && ! pms->nMatchTo )
              /* gammons do not count on a centred cube during money
                 sessions under the Jacoby rule */
                n = 1;

	    pms->gs = GAME_OVER;
	    pmgi->nPoints = pms->nCube * n;
	    pmgi->fWinner = pmr->n.fPlayer;
	    pmgi->fResigned = FALSE;
	    ApplyGameOver( pms, plGame );
	}
	
	break;

    case MOVE_RESIGN:
	pms->gs = GAME_RESIGNED;
	pmgi->nPoints = pms->nCube * ( pms->fResigned = pmr->r.nResigned );
	pmgi->fWinner = !pmr->r.fPlayer;
	pmgi->fResigned = TRUE;
	
	ApplyGameOver( pms, plGame );
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
	if( pms->fMove != pmr->sd.fPlayer )
	    SwapSides( pms->anBoard );
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

extern void
CalculateBoard( void )
{
  list *pl;

  pl = plGame;
  do {
    pl = pl->plNext;

    assert( pl->p );

    ApplyMoveRecord( &ms, plGame, pl->p );

    if ( pl->plNext && pl->plNext->p )
      FixMatchState ( &ms, pl->plNext->p );

        
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
        assert ( pmr->g.bgv >= VARIATION_STANDARD &&
                 pmr->g.bgv < NUM_VARIATIONS );
	
	break;
	
    case MOVE_NORMAL:
        assert( pmr->n.esChequer.et >= EVAL_NONE &&
                pmr->n.esChequer.et <= EVAL_ROLLOUT );
        assert( pmr->n.esDouble.et >= EVAL_NONE &&
                pmr->n.esDouble.et <= EVAL_ROLLOUT );
	assert( pmr->n.fPlayer >= 0 && pmr->n.fPlayer <= 1 );
	assert( pmr->n.ml.cMoves >= 0 && pmr->n.ml.cMoves < MAX_MOVES );
	if( pmr->n.ml.cMoves )
	    assert( pmr->n.iMove >= 0 && pmr->n.iMove <= pmr->n.ml.cMoves );
	assert( pmr->n.lt >= LUCK_VERYBAD && pmr->n.lt <= LUCK_VERYGOOD );
	assert( 0 <= pmr->n.stMove && pmr->n.stMove < N_SKILLS );
	assert( 0 <= pmr->n.stCube && pmr->n.stCube < N_SKILLS );
	break;
	
    case MOVE_DOUBLE:
    case MOVE_TAKE:
    case MOVE_DROP:
        assert( pmr->d.CubeDecPtr->esDouble.et >= EVAL_NONE &&
                pmr->d.CubeDecPtr->esDouble.et <= EVAL_ROLLOUT );
	assert( pmr->d.fPlayer >= 0 && pmr->d.fPlayer <= 1 );
	assert( 0 <= pmr->d.st && pmr->d.st < N_SKILLS );
	break;
	
    case MOVE_RESIGN:
	assert( pmr->r.fPlayer >= 0 && pmr->r.fPlayer <= 1 );
	assert( pmr->r.nResigned >= 1 && pmr->r.nResigned <= 3 );
        assert( pmr->r.esResign.et >= EVAL_NONE &&
                pmr->r.esResign.et <= EVAL_ROLLOUT );
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

    FixMatchState ( &ms, pmr );
    
    ApplyMoveRecord( &ms, plGame, pmr );
    
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
   
    assert( pmr->mt == MOVE_GAMEINFO );

#if USE_GTK
    if( fX )
	GTKAddGame( pmr );
#endif
}

void DiceRolled()
{
	playSound ( SOUND_ROLL );
    
#if USE_GUI
	if (fX && fDisplay)
	{
		BoardData *bd = BOARD(pwBoard)->board_data;
		/* Make sure dice are updated */
		bd->diceRoll[0] = 0;
		ShowBoard();
	}
#endif
}

static int NewGame( void ) {

    moverecord *pmr;
    int fError;
    list *pl;
    
    if( !fRecord && !ms.nMatchTo && lMatch.plNext->p ) {
	/* only recording the active game of a session; discard any others */
	if( fConfirm ) {
	    if( fInterrupt )
		return -1;
	    
	    if( !GetInputYN( _("Are you sure you want to start a new game, "
			     "and discard the one in progress? ") ) )
		return -1;
	}

	PopGame( lMatch.plNext->p, TRUE );
    }
    
    InitBoard( ms.anBoard, ms.bgv );

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
    pmr->g.fJacoby = ms.fJacoby && !ms.nMatchTo;
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    pmr->g.bgv = ms.bgv;
    pmr->g.fCubeUse = ms.fCubeUse;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
    

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );

#if USE_BOARD3D
    if (fX) {
	BoardData* bd = BOARD(pwBoard)->board_data;
	InitBoardData(bd);
    }
#endif

 reroll:
    fError = RollDice( ms.anDice, rngCurrent );

    if( fInterrupt || fError ) {
	PopMoveRecord( plGame->plNext );

	free( plGame );
	ListDelete( lMatch.plPrev );

        for ( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
          plGame = pl->p;

        plLastMove = plGame->plPrev;

	return -1;
    }
    
    if( fDisplay ) {
      outputnew();
      outputf( _("%s rolls %d, %s rolls %d.\n"), ap[ 0 ].szName, ms.anDice[ 0 ],
               ap[ 1 ].szName, ms.anDice[ 1 ] );
    }

    if( ms.anDice[ 0 ] == ms.anDice[ 1 ] && ms.nCube < MAX_CUBE ) {
	if( !ms.nMatchTo && ms.nCube < ( 1 << cAutoDoubles ) && ms.fCubeUse ) {
	    pmr->g.nAutoDoubles++;
	    if( fDisplay )
		outputf( _("The cube is now at %d.\n"), ms.nCube <<= 1 );
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
    pmr->sd.rLuck = ERR_VAL;
    AddMoveRecord( pmr );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    /* Play sound after initial dice decided */
	DiceRolled();
#if USE_GUI
    ResetDelayTimer();
#endif

    return 0;
}

static void ShowAutoMove( int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    char sz[ 40 ];

    if( anMove[ 0 ] == -1 )
	outputf( _("%s cannot move.\n"), ap[ ms.fTurn ].szName );
    else
	outputf( _("%s moves %s.\n"), ap[ ms.fTurn ].szName,
		 FormatMove( sz, anBoard, anMove ) );
}

extern int ComputerTurn( void ) {

  movenormal *pmn;
  cubeinfo ci;
  float arDouble[ 4 ], arOutput[ NUM_ROLLOUT_OUTPUTS ], rDoublePoint;
#if HAVE_SOCKETS
  char szBoard[ 256 ], szResponse[ 256 ];
  int i, c, fTurnOrig;
#endif

  if( fAction )
      fnAction();

  if( fInterrupt || ms.gs != GAME_PLAYING )
      return -1;

  GetMatchStateCubeInfo( &ci, &ms );

  switch( ap[ ms.fTurn ].pt ) {
  case PLAYER_GNU:
    if( ms.fResigned ) {

      float rEqBefore, rEqAfter;
      const float epsilon = 1.0e-6;
      const evalsetup* ces = &ap[ ms.fTurn ].esCube;;
      
      ProgressStart( _("Considering resignation...") );
      getResignation( arOutput, ms.anBoard, &ci, ces );
      ProgressEnd();

      getResignEquities ( arOutput, &ci, ms.fResigned,
                          &rEqBefore, &rEqAfter );

      fComputerDecision = TRUE;

      if( ( rEqAfter - epsilon ) < rEqBefore )
        CommandAgree( NULL );
      else
        CommandDecline( NULL );
      
      fComputerDecision = FALSE;
      return 0;
    } else if( ms.fDoubled ) {

      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
      rolloutstat aarsStatistics[ 2 ][ 2 ];
      cubedecision cd;

      /* Consider cube action */

      /* 
       * We may get here in three different scenarios: 
       * (1) normal double by opponent: fMove != fTurn and fCubeOwner is
       *     either -1 (centered cube) or = fMove.
       * (2) beaver by opponent: fMove = fTurn and fCubeOwner = !
       *     fMove
       * (3) raccoon by opponent: fMove != fTurn and fCubeOwner =
       *     fTurn.
       *
       */

      if ( ms.fMove != ms.fTurn && ms.fCubeOwner == ms.fTurn ) {

        /* raccoon: consider this a normal double, i.e. 
             fCubeOwner = fMove */
        
        SetCubeInfo ( &ci, ci.nCube,
                      ci.fMove, ci.fMove,
                      ci.nMatchTo, ci.anScore, ci.fCrawford,
                      ci.fJacoby, ci.fBeavers, ci.bgv );

      }
      
      if ( ms.fMove == ms.fTurn && ms.fCubeOwner != ms.fMove ) {

        /* opponent beavered: consider this a normal double by me */

        SetCubeInfo ( &ci, ci.nCube,
                      ci.fMove, ci.fMove,
                      ci.nMatchTo, ci.anScore, ci.fCrawford,
                      ci.fJacoby, ci.fBeavers, ci.bgv );

      }

      /* Evaluate cube decision */
      ProgressStart( _("Considering cube action...") );
      if ( GeneralCubeDecision ( aarOutput, aarStdDev, aarsStatistics,
                                 ms.anBoard, &ci, &ap [ ms.fTurn ].esCube,
                                 NULL, NULL ) < 0 ) {
	  ProgressEnd();
	  return -1;
      }
      ProgressEnd();

      cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );

      fComputerDecision = TRUE;

      if ( ms.fTurn == ms.fMove ) {

        /* opponent has beavered */

        switch ( cd ) {

        case DOUBLE_TAKE:
        case REDOUBLE_TAKE:
        case TOOGOOD_TAKE:
        case TOOGOODRE_TAKE:
        case DOUBLE_PASS:
        case TOOGOOD_PASS:
        case REDOUBLE_PASS:
        case TOOGOODRE_PASS:
        case OPTIONAL_DOUBLE_TAKE:
        case OPTIONAL_REDOUBLE_TAKE:
        case OPTIONAL_DOUBLE_PASS:
        case OPTIONAL_REDOUBLE_PASS:

          /* Opponent out of his right mind: Raccoon if possible */

          if ( ms.cBeavers < nBeavers && ! ms.nMatchTo &&
	       ms.nCube < ( MAX_CUBE >> 1 ) ) 
            /* he he: raccoon */
            CommandRedouble ( NULL );
          else
            /* Damn, no raccoons allowed */
            CommandTake ( NULL );

          break;

          
        case NODOUBLE_TAKE:
        case NO_REDOUBLE_TAKE:

          /* hmm, oops: opponent beavered us:
             consider dropping the beaver */

          /* Note, this should not happen as the computer plays
             "perfectly"!! */

          if ( arDouble[ OUTPUT_TAKE ] <= -1.0 )
            /* drop beaver */
            CommandDrop ( NULL );
          else
            /* take */
            CommandTake ( NULL );
          
          break;


        case DOUBLE_BEAVER:
        case NODOUBLE_BEAVER:
        case NO_REDOUBLE_BEAVER:
        case OPTIONAL_DOUBLE_BEAVER:

          /* opponent beaver was correct */

          CommandTake ( NULL );
          break;

        default:

          assert ( FALSE );
          
        } /* switch cubedecision */

      } /* consider beaver */
      else {

        /* normal double by opponent */

        switch ( cd ) {
        
        case DOUBLE_TAKE:
        case NODOUBLE_TAKE:
        case TOOGOOD_TAKE:
        case REDOUBLE_TAKE:
        case NO_REDOUBLE_TAKE:
        case TOOGOODRE_TAKE:
        case NODOUBLE_DEADCUBE:
        case NO_REDOUBLE_DEADCUBE:
        case OPTIONAL_DOUBLE_TAKE:
        case OPTIONAL_REDOUBLE_TAKE:

          CommandTake ( NULL );
          break;

        case DOUBLE_PASS:
        case TOOGOOD_PASS:
        case REDOUBLE_PASS:
        case TOOGOODRE_PASS:
        case OPTIONAL_DOUBLE_PASS:
        case OPTIONAL_REDOUBLE_PASS:

          CommandDrop ( NULL );
          break;

        case DOUBLE_BEAVER:
        case NODOUBLE_BEAVER:
        case NO_REDOUBLE_BEAVER:
        case OPTIONAL_DOUBLE_BEAVER:

          if ( ms.cBeavers < nBeavers && ! ms.nMatchTo &&
	       ms.nCube < ( MAX_CUBE >> 1 ) ) 
            /* Beaver all night! */
            CommandRedouble ( NULL );
          else
            /* Damn, no beavers allowed */
            CommandTake ( NULL );

          break;

        default:

          assert ( FALSE );

        } /* switch cubedecision */

      } /* normal cube */
      
      fComputerDecision = FALSE;
      
      return 0;

    } else {
      int anBoardMove[ 2 ][ 25 ];
      float arResign[ NUM_ROLLOUT_OUTPUTS ];
      int nResign;
      static char achResign[ 3 ] = { 'n', 'g', 'b' };
      char ach[ 2 ];
	 
      /* Don't use the global board for this call, to avoid
	 race conditions with updating the board and aborting the
	 move with an interrupt. */
      memcpy( anBoardMove, ms.anBoard, sizeof( anBoardMove ) );

      /* Consider resigning -- no point wasting time over the decision,
         so only evaluate at 0 plies. */

      if( ClassifyPosition( ms.anBoard, ms.bgv ) <= CLASS_RACE ) {

          evalcontext ecResign = { FALSE, 0, 0, TRUE, 0.0 };
          evalsetup esResign;

          esResign.et = EVAL_EVAL;
          esResign.ec = ecResign;

          nResign = getResignation ( arResign, anBoardMove, &ci,
                                     &esResign );

          if ( nResign > 0 && nResign > ms.fResignationDeclined ) {

	      fComputerDecision = TRUE;
	      ach[ 0 ] = achResign[ nResign - 1 ];
	      ach[ 1 ] = 0;
	      CommandResign( ach );	  
	      fComputerDecision = FALSE;
	      
	      return 0;

          }
          
      }
      
      /* Consider doubling */

      if ( ms.fCubeUse && !ms.anDice[ 0 ] && ms.nCube < MAX_CUBE &&
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

          float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
          float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
          rolloutstat aarsStatistics[ 2 ][ 2 ];
          cubedecision cd;

          /* Consider cube action */
	  ProgressStart( _("Considering cube action...") );
          if ( GeneralCubeDecision ( aarOutput, aarStdDev, aarsStatistics,
                                     ms.anBoard, &ci, &ap [ ms.fTurn ].esCube,
                                     NULL, NULL ) < 0 ) {
	      ProgressEnd();
	      return -1;
	  }
	  ProgressEnd();

          cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );

          switch ( cd ) {

          case DOUBLE_TAKE:
          case REDOUBLE_TAKE:
          case DOUBLE_PASS:
          case REDOUBLE_PASS:
          case DOUBLE_BEAVER:

            /* Double */

            fComputerDecision = TRUE;
            CommandDouble ( NULL );
            fComputerDecision = FALSE;
            return 0;
            
          case NODOUBLE_TAKE:
          case TOOGOOD_TAKE:
          case NO_REDOUBLE_TAKE:
          case TOOGOODRE_TAKE:
          case TOOGOOD_PASS:
          case TOOGOODRE_PASS:
          case NODOUBLE_BEAVER:
          case NO_REDOUBLE_BEAVER:

            /* better leave cube where it is: no op */
            break;

          case OPTIONAL_DOUBLE_BEAVER:
          case OPTIONAL_DOUBLE_TAKE:
          case OPTIONAL_REDOUBLE_TAKE:
          case OPTIONAL_DOUBLE_PASS:
          case OPTIONAL_REDOUBLE_PASS:

            if ( ap [ ms.fTurn ].esCube.et == EVAL_EVAL &&
                 ap [ ms.fTurn ].esCube.ec.nPlies == 0 ) {
		/* double if 0-ply */
		fComputerDecision = TRUE;
		CommandDouble ( NULL );
		fComputerDecision = FALSE;
		return 0;
	    }
	    break;
	    
          default:

            assert ( FALSE );
            break;

          }

        } /* market window */
      } /* access to cube */

      /* Roll dice and move */
      if ( !ms.anDice[ 0 ] ) {
        if ( ! fCheat || CheatDice ( ms.anDice, 
                                     &ms, afCheatRoll[ ms.fMove ] ) )
	  if( RollDice ( ms.anDice, rngCurrent ) < 0 )
            return -1;

	  DiceRolled();      
	  /* write line to status bar if using GTK */
#if USE_GTK        
	  if ( fX ) {

	      outputnew ();
	      outputf ( _("%s rolls %1i and %1i.\n"),
			ap [ ms.fTurn ].szName, ms.anDice[ 0 ],
			ms.anDice[ 1 ] );
	      outputx ();
	      
	  }
#endif
	  ResetDelayTimer(); /* Start the timer again -- otherwise the time
				we spent contemplating the cube could replace
				the delay. */
      }

      pmn = malloc( sizeof( *pmn ) );
      pmn->mt = MOVE_NORMAL;
      pmn->sz = NULL;
      pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
      pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
      pmn->fPlayer = ms.fTurn;
      pmn->ml.cMoves = 0;
      pmn->ml.amMoves = NULL;
      pmn->esDouble.et = EVAL_NONE;
      pmn->esChequer.et = EVAL_NONE;
      pmn->lt = LUCK_NONE;
      pmn->rLuck = ERR_VAL;
      pmn->stMove = SKILL_NONE;
      pmn->stCube = SKILL_NONE;

      ProgressStart( _("Considering move...") );
      if( FindBestMove( pmn->anMove, ms.anDice[ 0 ], ms.anDice[ 1 ],
                        anBoardMove, &ci,
			&ap[ ms.fTurn ].esChequer.ec, 
                        ap[ ms.fTurn ].aamf ) < 0 ) {
	  ProgressEnd();
	  free( pmn );
	  return -1;
      }
      ProgressEnd();
      
      /* write move to status bar if using GTK */
#if USE_GTK        
      if ( fX ) {
	  
	  outputnew ();
	  ShowAutoMove( ms.anBoard, pmn->anMove );
	  outputx ();
      }
#endif

      if( pmn->anMove[ 0 ] < 0 )
	  playSound ( SOUND_BOT_DANCE );
      
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
      if ( ! fCheat || CheatDice ( ms.anDice, &ms, afCheatRoll[ ms.fMove ] ) )
        if( RollDice ( ms.anDice, rngCurrent ) < 0 )
          return -1;

		DiceRolled();      
    }
    
    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->esDouble.et = EVAL_NONE;
    pmn->esChequer.et = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = ERR_VAL;
    pmn->stMove = SKILL_NONE;
    pmn->stCube = SKILL_NONE;
    
    FindPubevalMove( ms.anDice[ 0 ], ms.anDice[ 1 ], ms.anBoard, 
                     pmn->anMove, ms.bgv );
    
    if( pmn->anMove[ 0 ] < 0 )
	playSound ( SOUND_BOT_DANCE );
      
    AddMoveRecord( pmn );
    return 0;

  case PLAYER_EXTERNAL:
#if HAVE_SOCKETS
      fTurnOrig = ms.fTurn;
      
#if 1
      /* FIXME handle resignations -- there must be some way of indicating
	 that a resignation has been offered to the external player. */
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
      }
#endif

      if( !ms.anDice[ 0 ] && !ms.fDoubled && !ms.fResigned &&
	  ( !ms.fCubeUse || ms.nCube >= MAX_CUBE ||
	    !GetDPEq( NULL, NULL, &ci ) ) ) {
	  if( RollDice( ms.anDice, rngCurrent ) < 0 )
	      return -1;
	      
	  DiceRolled();      
	 }
      FIBSBoard( szBoard, ms.anBoard, ms.fMove, ap[ 1 ].szName,
		 ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		 ms.anScore[ 0 ], ms.anDice[ 0 ], ms.anDice[ 1 ], ms.nCube,
		 ms.fCubeOwner, ms.fDoubled, ms.fTurn, ms.fCrawford,
                 anChequers [ ms.bgv ] );
      strcat( szBoard, "\n" );
      
      if( ExternalWrite( ap[ ms.fTurn ].h, szBoard,
			 strlen( szBoard ) + 1 ) < 0 )
	  return -1;

      if( ExternalRead( ap[ ms.fTurn ].h, szResponse,
			sizeof( szResponse ) ) < 0 )
	  return -1;

#if 0
      /* Resignations for external players -- not yet implemented. */
      if( ms.fResigned ) {
	  fComputerDecision = TRUE;
	  
	  if( tolower( *szResponse ) == 'a' ) /* accept or agree */
	      CommandAgree( NULL );
	  else if( tolower( *szResponse ) == 'd' ||
		   tolower( *szResponse ) == 'r' ) /* decline or reject */
	      CommandDecline( NULL );
	  else {
	      outputl( _("Warning: badly formed resignation response from "
		       "external player") );
	      fComputerDecision = FALSE;
	      return -1;
	  }

	  fComputerDecision = FALSE;
	  
	  return ms.fTurn == fTurnOrig ? -1 : 0;
      }
#endif
      
      if( ms.fDoubled ) {
	  fComputerDecision = TRUE;
	  
	  if( tolower( *szResponse ) == 'a' ||
	      tolower( *szResponse ) == 't' ) /* accept or take */
	      CommandTake( NULL );
	  else if( tolower( *szResponse ) == 'd' || /* drop */
		   tolower( *szResponse ) == 'p' || /* pass */
		   !strncasecmp( szResponse, "rej", 3 ) ) /* reject */
	      CommandDrop( NULL );
	  else if( tolower( *szResponse ) == 'b' || /* beaver */
		   !strncasecmp( szResponse, "red", 3 ) ) /* redouble */
	      CommandRedouble( NULL );
	  else {
	      outputl( _("Warning: badly formed cube response from "
		       "external player") );
	      fComputerDecision = FALSE;
	      return -1;
	  }	      
	  
	  fComputerDecision = FALSE;
	  
	  return ms.fTurn != fTurnOrig || ms.gs >= GAME_OVER ? 0 : -1;
      } else if ( tolower( szResponse[0] ) == 'r' &&
		  tolower( szResponse[1] ) == 'e' ) { /* resign */
	char* r = szResponse;
	NextToken(&r);

	fComputerDecision = TRUE;
	CommandResign(r);
	fComputerDecision = FALSE;
	
	return ms.fTurn == fTurnOrig ? -1 : 0;
      } else if( !ms.anDice[ 0 ] ) {
	  if( tolower( *szResponse ) == 'r' ) { /* roll */
	      if( RollDice( ms.anDice, rngCurrent ) < 0 )
		  return -1;
	      
		  DiceRolled();      

	      return 0; /* we'll end up back here to play the roll, but
			   that's OK */
	  } else if( tolower( *szResponse ) == 'd' ) { /* double */
	      fComputerDecision = TRUE;
	      CommandDouble( NULL );
	      fComputerDecision = FALSE;
	  }

	  return ms.fTurn == fTurnOrig ? -1 : 0;
      } else {
	  pmn = malloc( sizeof( *pmn ) );
	  pmn->mt = MOVE_NORMAL;
	  pmn->sz = NULL;
	  pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
	  pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
	  pmn->fPlayer = ms.fTurn;
	  pmn->ml.cMoves = 0;
	  pmn->ml.amMoves = NULL;
	  pmn->esDouble.et = EVAL_NONE;
	  pmn->esChequer.et = EVAL_NONE;
	  pmn->lt = LUCK_NONE;
	  pmn->rLuck = ERR_VAL;
	  pmn->stMove = SKILL_NONE;
	  pmn->stCube = SKILL_NONE;
	  
	  if( ( c = ParseMove( szResponse, pmn->anMove ) ) < 0 ) {
	      pmn->anMove[ 0 ] = 0;
	      outputl( _("Warning: badly formed move from external player") );
	      return -1;
	  } else
	      for( i = 0; i < 4; i++ )
		  if( i < c ) {
		      pmn->anMove[ i << 1 ]--;
		      pmn->anMove[ ( i << 1 ) + 1 ]--;
		  } else {
		      pmn->anMove[ i << 1 ] = -1;
		      pmn->anMove[ ( i << 1 ) + 1 ] = -1;
		  }
      
	  if( pmn->anMove[ 0 ] < 0 )
	      playSound ( SOUND_BOT_DANCE );
      
	  AddMoveRecord( pmn );
	  return 0;
      }
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
	outputf( _("(%s's double has been cancelled.)\n"),
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
    
    if( ClassifyPosition( ms.anBoard, VARIATION_STANDARD ) > CLASS_RACE )
	/* It's a contact position; don't automatically bear off */
        /* note that we use VARIATION_STANDARD instead of ms.bgv in order
           to avoid automatic bearoff in contact positions for hypergammon */
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
		memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
			sizeof( pmn->anMove ) );
                if ( cmp_matchstate ( &ms, &sm.ms ) ) {
		  pmn->ml.cMoves = 0;
		  pmn->ml.amMoves = NULL;
		} else {
                  CopyMoveList ( &pmn->ml, &sm.ml );
                  pmn->iMove = locateMove ( ms.anBoard, pmn->anMove, 
                                            &pmn->ml );
		  if ((pmn->iMove < 0) || (pmn->iMove > pmn->ml.cMoves)) {
		    free (pmn->ml.amMoves);
		    pmn->ml.cMoves = 0;
		    pmn->ml.amMoves = NULL;
		  }
                }

                pmn->esDouble.et = EVAL_NONE;
                pmn->esChequer.et = EVAL_NONE;
		pmn->lt = LUCK_NONE;
		pmn->rLuck = ERR_VAL;
		pmn->stMove = SKILL_NONE;
		pmn->stCube = SKILL_NONE;
		
		
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

    assert( !fComputing );
	
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

    fComputing = TRUE;
    
#if USE_GTK
    if( fX && fDisplay ) {
	if( nDelay && nTimeout ) {
	    fDelaying = TRUE;
	    GTKDelay();
	    fDelaying = FALSE;
	    ResetDelayTimer();
	}
	
	if( fLastMove ) {
	    GTKDisallowStdin();
	    board_animate( BOARD( pwBoard ), anLastMove, fLastPlayer );
	    playSound ( SOUND_MOVE );
	    GTKAllowStdin();
	    fLastMove = FALSE;
	}
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
    
    if( ( n = GameStatus( ms.anBoard, ms.bgv ) ) ||
	( ms.gs == GAME_DROP && ( ( n = 1 ) ) ) ||
	( ms.gs == GAME_RESIGNED && ( ( n = ms.fResigned ) ) ) ) {
	movegameinfo *pmgi = plGame->plNext->p;
	
	if( ms.fJacoby && ms.fCubeOwner == -1 && !ms.nMatchTo )
	    /* gammons do not count on a centred cube during money
	       sessions under the Jacoby rule */
	    n = 1;
	
        playSound ( ap[ pmgi->fWinner ].pt == PLAYER_HUMAN ? 
                    SOUND_HUMAN_WIN_GAME : SOUND_BOT_WIN_GAME );

	outputf((pmgi->nPoints == 1 ? _("%s wins a %s and %d point.\n")
		 : _("%s wins a %s and %d points.\n")),
		ap[ pmgi->fWinner ].szName,
		gettext ( aszGameResult[ n - 1 ] ), pmgi->nPoints);

#if USE_GUI
	if( fX ) {
	    if( fDisplay )
		{
#if USE_BOARD3D
			if (ms.fResigned && rdAppearance.fDisplayType == DT_3D)
				StopIdle3d(BOARD(pwBoard)->board_data);	/* Stop flag waving */
#endif
			ShowBoard();
		}
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

            playSound ( ap[ pmgi->fWinner ].pt == PLAYER_HUMAN ? 
                        SOUND_HUMAN_WIN_MATCH : SOUND_BOT_WIN_MATCH );
          

	    outputf( _("%s has won the match.\n"), ap[ pmgi->fWinner ].szName );
	    outputx();
	    fComputing = FALSE;
	    return -1;
	}

	outputx();
	
	if( fAutoGame ) {
	    if( NewGame() < 0 ) {
		fComputing = FALSE;
		return -1;
	    }
	    
	    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN )
		ShowBoard();
	} else {
	    fComputing = FALSE;
	    return -1;
	}
    }

    assert( ms.gs == GAME_PLAYING );
    
    if( fDisplay || ap[ ms.fTurn ].pt == PLAYER_HUMAN )
	ShowBoard();

    /* We have reached a safe point to check for interrupts.  Until now,
       the board could have been in an inconsistent state. */
    if( fAction )
	fnAction();
	
    if( fInterrupt || !fPlayNext ) {
	fComputing = FALSE;
	return -1;
    }
    
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
	    ( !ms.fCubeUse || ms.fCrawford ||
	      ( ms.fCubeOwner >= 0 && ms.fCubeOwner != ms.fTurn ) ||
	      ( ms.nMatchTo > 0 && ms.anScore[ ms.fTurn ] +
		ms.nCube >= ms.nMatchTo ) ) )
	    CommandRoll( NULL );

	fComputing = FALSE;
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

    fComputing = FALSE;
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
	outputl( _("You can only accept if the cube or a resignation has been "
		 "offered.") );
}

extern void CommandAgree( char *sz ) {

    moveresign *pmr;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    if( !ms.fResigned ) {
	outputl( _("No resignation was offered.") );

	return;
    }

    if( fDisplay )
	outputf( _("%s accepts and wins a %s.\n"), ap[ ms.fTurn ].szName,
		gettext ( aszGameResult[ ms.fResigned - 1 ] ) );

    playSound ( SOUND_AGREE );

    pmr = malloc( sizeof( *pmr ) );
    pmr->mt = MOVE_RESIGN;
    pmr->sz = NULL;
    pmr->fPlayer = !ms.fTurn;
    pmr->nResigned = ms.fResigned;
    pmr->esResign.et = EVAL_NONE;
    
    AddMoveRecord( pmr );

    TurnDone();
}

static void AnnotateMove( skilltype st ) {

    moverecord *pmr;

    if( !( pmr = plLastMove->plNext->p ) ) {
	outputl( _("You must select a move to annotate first.") );
	return;
    }

    switch( pmr->mt ) {
    case MOVE_NORMAL:

      switch ( at ) {
      case ANNOTATE_MOVE:
	pmr->n.stMove = st; /* fixme */
	break;
      case ANNOTATE_CUBE:
      case ANNOTATE_DOUBLE:
        pmr->n.stCube = st; /* fixme */
        break;
      default:
        outputl ( _("Invalid annotation.") );
        break;
      }

      break;
	
    case MOVE_DOUBLE:

      switch ( at ) {
      case ANNOTATE_CUBE:
      case ANNOTATE_DOUBLE:
        pmr->d.st = st;
        break;
      default:
        outputl ( _("Invalid annotation") );
        break;
      }

      break;

    case MOVE_TAKE:
        
      switch ( at ) {
      case ANNOTATE_CUBE:
      case ANNOTATE_ACCEPT:
        pmr->d.st = st;
        break;
      default:
        outputl ( _("Invalid annotation") );
        break;
      }

      break;

    case MOVE_DROP:
        
      switch ( at ) {
      case ANNOTATE_CUBE:
      case ANNOTATE_REJECT:
      case ANNOTATE_DROP:
        pmr->d.st = st;
        break;
      default:
        outputl ( _("Invalid annotation") );
        break;
      }

      break;

    case MOVE_RESIGN:

      switch ( at ) {
      case ANNOTATE_RESIGN:
        pmr->r.stResign = st;
        break;
      case ANNOTATE_ACCEPT:
      case ANNOTATE_REJECT:
        pmr->r.stAccept = st;
        break;
      default:
        outputl ( _("Invalid annotation") );
        break;
      }

      break;

    default:
	outputl( _("You cannot annotate this move.") );
	return;
    }

    if( st == SKILL_NONE )
	outputl( _("Skill annotation cleared.") );
    else
	outputf( _("Move marked as %s.\n"), gettext ( aszSkillType[ st ] ) );
    
#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif

}

static void AnnotateRoll( lucktype lt ) {

    moverecord *pmr;

    if( !( pmr = plLastMove->plNext->p ) ) {
	outputl( _("You must select a move to annotate first.") );
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
	outputl( _("You cannot annotate this move.") );
	return;
    }

    if( lt == LUCK_NONE )
	outputl( _("Luck annotation cleared.") );
    else
	outputf( _("Roll marked as %s.\n"), gettext ( aszLuckType[ lt ] ) );

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif

}


extern void
CommandAnnotateAccept ( char *sz ) {

  at = ANNOTATE_ACCEPT;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateCube ( char *sz ) {

  at = ANNOTATE_CUBE;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateDouble ( char *sz ) {

  at = ANNOTATE_DOUBLE;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateDrop ( char *sz ) {

  at = ANNOTATE_DROP;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateMove ( char *sz ) {

  at = ANNOTATE_MOVE;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateReject ( char *sz ) {

  at = ANNOTATE_REJECT;
  HandleCommand( sz, acAnnotateMove );

}

extern void
CommandAnnotateResign ( char *sz ) {

  at = ANNOTATE_RESIGN;
  HandleCommand( sz, acAnnotateMove );

}

extern void CommandAnnotateBad( char *sz ) {

    AnnotateMove( SKILL_BAD );
}

extern void CommandAnnotateClearComment( char *sz ) {

    moverecord *pmr;

    if( !( pmr = plLastMove->plNext->p ) ) {
	outputl( _("You must select a move to clear the comment from.") );
	return;
    }

    if( pmr->a.sz )
	free( pmr->a.sz );

    pmr->a.sz = NULL;
    
    outputl( _("Commentary for this move cleared.") );

#if USE_GTK
  if( fX )
    GTKUpdateAnnotations();
#endif

}

extern void CommandAnnotateClearLuck( char *sz ) {

    AnnotateRoll( LUCK_NONE );
}

extern void CommandAnnotateClearSkill( char *sz ) {

    AnnotateMove( SKILL_NONE );
}

extern void CommandAnnotateDoubtful( char *sz ) {

    AnnotateMove( SKILL_DOUBTFUL );
}

extern void CommandAnnotateGood( char *sz ) { 

  AnnotateMove( SKILL_GOOD ); 
}

/* extern void CommandAnnotateInteresting( char *sz ) { */

/*     AnnotateMove( SKILL_INTERESTING ); */
/* } */

extern void CommandAnnotateLucky( char *sz ) {

    AnnotateRoll( LUCK_GOOD );
}

extern void CommandAnnotateUnlucky( char *sz ) {

    AnnotateRoll( LUCK_BAD );
}

extern void CommandAnnotateVeryBad( char *sz ) {

    AnnotateMove( SKILL_VERYBAD );
}

/* extern void CommandAnnotateVeryGood( char *sz ) { */

/*     AnnotateMove( SKILL_VERYGOOD ); */
/* } */

extern void CommandAnnotateVeryLucky( char *sz ) {

    AnnotateRoll( LUCK_VERYGOOD );
}

extern void CommandAnnotateVeryUnlucky( char *sz ) {

    AnnotateRoll( LUCK_VERYBAD );
}

extern void CommandDecline( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    if( !ms.fResigned ) {
	outputl( _("No resignation was offered.") );

	return;
    }

    if( fDisplay )
	outputf( _("%s declines the %s.\n"), ap[ ms.fTurn ].szName,
		gettext ( aszGameResult[ ms.fResigned - 1 ] ) );

    ms.fResignationDeclined = ms.fResigned;
    ms.fResigned = FALSE;
    ms.fTurn = !ms.fTurn;
    
    TurnDone();
}

static skilltype GoodDouble (int fisRedouble, moverecord *pmr )
{
  float arDouble[ 4 ], aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo ci;
  cubedecision cd;
  float rDeltaEquity;
  int      fAnalyseCubeSave = fAnalyseCube;
  evalcontext *pec;
  evalsetup   *pes;
  monitor m;
  evalsetup es;

  /* reasons that doubling is not an issue */
  if( (ms.gs != GAME_PLAYING) || 
		ms.anDice[ 0 ]         || 
		(ms.fDoubled && !fisRedouble) || 
		(!ms.fDoubled && fisRedouble) ||
		ms.fResigned ||
		(ap[ ms.fTurn ].pt != PLAYER_HUMAN )) {

      return (SKILL_NONE);
  }

  if (fTutorAnalysis) {
    pes = &esAnalysisCube;
  } else {
    pes = &esEvalCube;
  }

  pec = &pes->ec;


  GetMatchStateCubeInfo( &ci, &ms );

  fAnalyseCube = TRUE;
  if( !GetDPEq ( NULL, NULL, &ci ) ) {
    fAnalyseCube = fAnalyseCubeSave;
    return (SKILL_NONE);
  }

  /* Give hint on cube action */


  SuspendInput ( &m );

  ProgressStart( _("Considering cube action...") );
  if (GeneralCubeDecisionE( aarOutput, ms.anBoard, &ci, pec, pes) < 0 ) {
    ResumeInput ( &m );
    ProgressEnd();
    fAnalyseCube = fAnalyseCubeSave;
    return (SKILL_NONE);;
  }
  ProgressEnd();

  /* update analysis for hint */

  ec2es ( &es, pec );
  UpdateStoredCube ( aarOutput, aarOutput /* whatever */, &es, &ms );

  ResumeInput ( &m );
	    
  /* store cube decision for annotation */

  ec2es ( &pmr->d.CubeDecPtr->esDouble, pec );
  memcpy ( pmr->d.CubeDecPtr->aarOutput, aarOutput, 
	   2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

  memset ( pmr->d.CubeDecPtr->aarStdDev, 0,
	   2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  pmr->d.st = SKILL_NONE;

  /* find skill */

  cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );  

  switch ( cd ) {
	case NODOUBLE_TAKE:
	case NODOUBLE_BEAVER:
	case TOOGOOD_TAKE:
	case TOOGOOD_PASS:
	case NO_REDOUBLE_TAKE:
	case NO_REDOUBLE_BEAVER:
	case TOOGOODRE_TAKE:

	  rDeltaEquity = arDouble [OUTPUT_TAKE] - arDouble [OUTPUT_NODOUBLE];
	  break;
  
	default:
	  return (SKILL_NONE);
	}

	return ( pmr->d.st = Skill ( rDeltaEquity ) );
}

extern void CommandDouble( char *sz ) {

    moverecord *pmr;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    if( ms.fCrawford ) {
	outputl( _("Doubling is forbidden by the Crawford rule (see `help set "
	      "crawford').") );

	return;
    }

    if( !ms.fCubeUse ) {
	outputl( _("The doubling cube has been disabled (see `help set cube "
	      "use').") );

	return;
    }

    if( ms.fDoubled ) {
	outputl( _("The `double' command is for offering the cube, not "
		 "accepting it.  Use\n`redouble' to immediately offer the "
		 "cube back at a higher value.") );

	return;
    }
    
    if( ms.fTurn != ms.fMove ) {
	outputl( _("You are only allowed to double if you are on roll.") );

	return;
    }
    
    if( ms.anDice[ 0 ] ) {
	outputl( _("You can't double after rolling the dice -- wait until your "
	      "next turn.") );

	return;
    }

    if( ms.fCubeOwner >= 0 && ms.fCubeOwner != ms.fTurn ) {
	outputl( _("You do not own the cube.") );

	return;
    }

    if( ms.nCube >= MAX_CUBE ) {
	outputf( _("The cube is already at %d; you can't double any more.\n"), 
                 MAX_CUBE );
	return;
    }

    playSound ( SOUND_DOUBLE );
    
    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_DOUBLE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    if( !LinkToDouble( pmr ) ) {
    pmr->d.CubeDecPtr = &pmr->d.CubeDec;
    pmr->d.CubeDecPtr->esDouble.et = EVAL_NONE;
      pmr->d.nAnimals = 0;
    }
    pmr->d.st = SKILL_NONE;

    if ( fTutor && fTutorCube && !GiveAdvice( GoodDouble( FALSE, pmr ) ))
      return;

    if( fDisplay )
	outputf( _("%s doubles.\n"), ap[ ms.fTurn ].szName );

#if USE_GTK
    /* There's no point delaying here. */
    if( nTimeout ) {
	gtk_timeout_remove( nTimeout );
	nTimeout = 0;
    }
#endif
    
    AddMoveRecord( pmr );
    
    TurnDone();
}

static skilltype ShouldDrop (int fIsDrop, moverecord *pmr) {

    float arDouble[ 4 ], aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    cubeinfo ci;
	cubedecision cd;
    float rDeltaEquity;
	int      fAnalyseCubeSave = fAnalyseCube;
	evalcontext *pec;
	evalsetup   *pes;
    monitor m;
    evalsetup es;

	/* reasons that doubling is not an issue */
    if( (ms.gs != GAME_PLAYING) || 
		ms.anDice[ 0 ]         || 
		!ms.fDoubled || 
		ms.fResigned ||
		(ap[ ms.fTurn ].pt != PLAYER_HUMAN )) {

      return (SKILL_NONE);
    }

	if (fTutorAnalysis)
	  pes = &esAnalysisCube;
	else 
	  pes = &esEvalCube;

	pec = &pes->ec;

	GetMatchStateCubeInfo( &ci, &ms );

	fAnalyseCube = TRUE;
	if( !GetDPEq ( NULL, NULL, &ci ) ) {
	  fAnalyseCube = fAnalyseCubeSave;
	  return (SKILL_NONE);
	}

	/* Give hint on cube action */

        SuspendInput ( &m );
	ProgressStart( _("Considering cube action...") );
	if ( GeneralCubeDecisionE( aarOutput, ms.anBoard, &ci, pec, pes) < 0) {
	  ProgressEnd();
          ResumeInput ( &m );
	  fAnalyseCube = fAnalyseCubeSave;
	  return (SKILL_NONE);
	}

        /* stored cube decision for hint */

        ec2es ( &es, pec );
        UpdateStoredCube ( aarOutput,
                           aarOutput, /* whatever */
                           &es, &ms );

	ProgressEnd();
        ResumeInput ( &m );
	    
        /* store cube decision for annotation */

        ec2es ( &pmr->d.CubeDecPtr->esDouble, pec );
        memcpy ( pmr->d.CubeDecPtr->aarOutput, aarOutput, 
                 2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
        memset ( pmr->d.CubeDecPtr->aarStdDev, 0,
                 2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
        pmr->d.st = SKILL_NONE;

	    
	cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );  

	switch ( cd ) {
	case DOUBLE_TAKE:
	case DOUBLE_BEAVER:
	case REDOUBLE_TAKE:
	case NODOUBLE_TAKE:
	case NODOUBLE_BEAVER:
	case NO_REDOUBLE_TAKE:
	case NO_REDOUBLE_BEAVER:
	case TOOGOODRE_TAKE:
        case OPTIONAL_DOUBLE_BEAVER:
        case OPTIONAL_DOUBLE_TAKE:
        case OPTIONAL_REDOUBLE_TAKE:
	  /* best response is take, player took - good move */
	  if ( !fIsDrop )
		return ( SKILL_GOOD );

	  /* equity loss for dropping when you shouldn't */
 	  rDeltaEquity =  arDouble [OUTPUT_DROP] - arDouble [OUTPUT_TAKE];
	  break;

	case DOUBLE_PASS:
	case REDOUBLE_PASS:
	case TOOGOOD_PASS:
	case TOOGOODRE_PASS:
        case OPTIONAL_DOUBLE_PASS:
        case OPTIONAL_REDOUBLE_PASS:
	  /* best response is drop, player dropped - good move */
	  if ( fIsDrop )
		return (SKILL_GOOD);

 	  rDeltaEquity = arDouble [OUTPUT_TAKE] - arDouble [OUTPUT_DROP];
	  break;


	default:
	  return (SKILL_NONE);
	}

	/* equity is for doubling player, invert for response */
	return ( pmr->d.st = Skill (-rDeltaEquity) );
}


extern void CommandDrop( char *sz ) {
      moverecord *pmr;
    
    if( ms.gs != GAME_PLAYING || !ms.fDoubled ) {
	outputl( _("The cube must have been offered before you can drop it.") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    playSound ( SOUND_DROP );

    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_DROP;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    if( !LinkToDouble( pmr ) ) {
      free (pmr);
      return;
    }
    pmr->d.st = SKILL_NONE;

    if ( fTutor && fTutorCube && !GiveAdvice ( ShouldDrop ( TRUE, pmr ) )) {
      free ( pmr ); /* garbage collect */
      return;
    }


    if( fDisplay )
       outputf((ms.nCube == 1
		? _("%s refuses the cube and gives up %d point.\n")
		: _("%s refuses the cube and gives up %d points.\n")),
	       ap[ ms.fTurn ].szName, ms.nCube);
    
    AddMoveRecord( pmr );
    
    TurnDone();
}

static void DumpGameList(char *szOut, list *plGame) {

    list *pl;
    moverecord *pmr;
    char sz[ 128 ];
    int i = 0, nFileCube = 1, anBoard[ 2 ][ 25 ], fWarned = FALSE;

    InitBoard( anBoard, ms.bgv );
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;

	switch( pmr->mt ) {
	case MOVE_GAMEINFO:
	    /* FIXME */
          continue;
	    break;
	case MOVE_NORMAL:
	    sprintf( sz, "%d%d%-2s: ", pmr->n.anRoll[ 0 ],
                     pmr->n.anRoll[ 1 ],
                     aszLuckTypeAbbr[ pmr->n.lt ] );
	    FormatMove( sz + 6, anBoard, pmr->n.anMove );
            strcat(sz, " ");
            strcat(sz, aszSkillTypeAbbr[ pmr->n.stMove ]);
            strcat(sz, aszSkillTypeAbbr[ pmr->n.stCube ]);
	    ApplyMove( anBoard, pmr->n.anMove, FALSE );
	    SwapSides( anBoard );
         /*   if (( sz[ strlen(sz)-1 ] == ' ') && (strlen(sz) > 5 ))
              sz[ strlen(sz) - 1 ] = 0;  Don't need this..  */
	    break;
	case MOVE_DOUBLE:
	    sprintf( sz, _("      Doubles to %d"), nFileCube <<= 1 );
	    break;
	case MOVE_TAKE:
	    strcpy( sz, _("      Takes") ); /* FIXME beavers? */
	    break;
	case MOVE_DROP:
            sprintf( sz, _("      Drop") );
	  /*  if( anScore )
		anScore[ ( i + 1 ) & 1 ] += nFileCube / 2; */
	    break;
        case MOVE_RESIGN:
            sprintf( sz, _("      Resigns") );
            break;
	case MOVE_SETDICE:
	    sprintf( sz, "%d%d%-2s: %s", 
                     pmr->sd.anDice[ 0 ],
                     pmr->sd.anDice[ 1 ],
                     aszLuckTypeAbbr[ pmr->n.lt ],
                     _("Rolled") );
	    break;
	case MOVE_SETBOARD:
	case MOVE_SETCUBEVAL:
	case MOVE_SETCUBEPOS:
	    if( !fWarned ) {
		fWarned = TRUE;
		outputl( "FIXME!!! (I'm DumpGameList in play.c)" );
	    }
	    break;
        default:
            printf("\n");
	}

	if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) {
	    printf( "  1|                                " );
	    i++;
	}

	if(( i & 1 ) || (pmr->mt == MOVE_RESIGN)) {
	    printf( "%s\n", sz );
	} else 
	    printf( "%3d| %-30s ", ( i >> 1 ) + 1, sz );

        if ( pmr->mt == MOVE_DROP ) {
          printf( "\n" );
          if ( ! ( i & 1 ) )
            printf( "\n" );
        }

	i++;
    }
}

extern void CommandListGame( char *sz ) {

    char szOut[4098];
#if USE_GTK
    if( fX ) {
	ShowGameWindow();
	return;
    }
#endif
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	
	return;
    }

    DumpGameList(szOut, plGame);
    /* FIXME  outputl(szOut);
       DumpListGame will soon return szOut */
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

static skilltype GoodMove (movenormal *p) {

  matchstate msx;
  int        fAnalyseMoveSaved = fAnalyseMove;
  moverecord *pmr = (moverecord *) p;
  evalsetup *pesCube, *pesChequer;
  monitor m;

  /* should never happen, but if it does, tutoring
   * is a non-starter
   */
  if ((pmr == 0) ||
      (pmr->mt != MOVE_NORMAL) ||
      (ap[ pmr->n.fPlayer ].pt != PLAYER_HUMAN))
    return SKILL_NONE;
	 
  /* ensure we're analyzing moves */
  fAnalyseMove = 1;
  memcpy ( &msx, &ms, sizeof ( matchstate ) );

  if (fTutorAnalysis) {
    pesCube = &esAnalysisCube;
    pesChequer = &esAnalysisChequer;
  } else {
    pesCube = &esEvalCube;
    pesChequer = &esEvalChequer;
  }

  SuspendInput ( &m );
  ProgressStart( _("Considering move...") );
  if (AnalyzeMove ( pmr, &msx, plGame, NULL, pesChequer, pesChequer,
                    fTutorAnalysis ? aamfAnalysis : aamfEval, 
		    FALSE, NULL ) < 0) {
    fAnalyseMove = fAnalyseMoveSaved;
    ProgressEnd();
    ResumeInput ( &m );
    return SKILL_NONE;
  }

  fAnalyseMove = fAnalyseMoveSaved;

  /* update move list for hint */

  UpdateStoredMoves ( &p->ml, &ms );

  ProgressEnd ();
  ResumeInput ( &m );

  return pmr->n.stMove;
}

extern void 
CommandMove( char *sz ) {

    int c, i, j, anBoardNew[ 2 ][ 25 ], anBoardTest[ 2 ][ 25 ], an[ 8 ];
    movelist ml;
    movenormal *pmn;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	
	return;
    }
    
    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }
    
    if( !ms.anDice[ 0 ] ) {
	outputl( _("You must roll the dice before you can move.") );
	
	return;
    }
    
    if( ms.fResigned ) {
	outputf( _("Please wait for %s to consider the resignation before "
		 "moving.\n"), ap[ ms.fTurn ].szName );
	
	return;
    }
    
    if( ms.fDoubled ) {
	outputf( _("Please wait for %s to consider the cube before "
		 "moving.\n"), ap[ ms.fTurn ].szName );
	
	return;
    }
    
    if( !*sz ) {
	GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ],
		       FALSE );
	
	if( ml.cMoves <= 1 ) {

            if ( ! ml.cMoves )
              playSound ( ap[ ms.fTurn ].pt == PLAYER_HUMAN ? 
                          SOUND_HUMAN_DANCE : SOUND_BOT_DANCE );
            else 
              playSound ( SOUND_MOVE );


	    pmn = malloc( sizeof( *pmn ) );
	    pmn->mt = MOVE_NORMAL;
	    pmn->sz = NULL;
	    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
	    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
	    pmn->fPlayer = ms.fTurn;
            if ( cmp_matchstate ( &ms, &sm.ms ) ) {
	      pmn->ml.cMoves = 0;
              pmn->ml.amMoves = NULL;
            } else {
              CopyMoveList ( &pmn->ml, &sm.ml );
              pmn->iMove = locateMove ( ms.anBoard, pmn->anMove, 
                                        &pmn->ml );
              if ((pmn->iMove < 0) || (pmn->iMove > pmn->ml.cMoves)) {
		free (pmn->ml.amMoves);
		pmn->ml.cMoves = 0;
		pmn->ml.amMoves = NULL;
	      }
	    }

	    pmn->esDouble.et = EVAL_NONE;
	    pmn->esChequer.et = EVAL_NONE;
	    pmn->lt = LUCK_NONE;
	    pmn->rLuck = ERR_VAL;
	    pmn->stMove = SKILL_NONE;
	    pmn->stCube = SKILL_NONE;
	    
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
	
	outputl( _("You must specify a move (type `help move' for "
		 "instructions).") );
	
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
	
	GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ],
		       FALSE );
	
	for( i = 0; i < ml.cMoves; i++ ) {
	    PositionFromKey( anBoardTest, ml.amMoves[ i ].auch );
	    
	    for( j = 0; j < 25; j++ )
		if( anBoardTest[ 0 ][ j ] != anBoardNew[ 0 ][ j ] ||
		    anBoardTest[ 1 ][ j ] != anBoardNew[ 1 ][ j ] )
		    break;
	    
	    if( j == 25 ) {
		/* we have a legal move! */
                playSound ( SOUND_MOVE );

		pmn = malloc( sizeof( *pmn ) );
		pmn->mt = MOVE_NORMAL;
		pmn->sz = NULL;
		pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
		pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
		pmn->fPlayer = ms.fTurn;
                if ( cmp_matchstate ( &ms, &sm.ms ) ) { 
		  pmn->ml.cMoves = 0;
		  pmn->ml.amMoves = NULL;
		} else {
                  CopyMoveList ( &pmn->ml, &sm.ml );
                  pmn->iMove = locateMove ( ms.anBoard, pmn->anMove, 
                                            &pmn->ml );
		  if ((pmn->iMove < 0) || (pmn->iMove > pmn->ml.cMoves)) {
		    free (pmn->ml.amMoves);
		    pmn->ml.cMoves = 0;
		    pmn->ml.amMoves = NULL;
		  }
                }

		pmn->esDouble.et = EVAL_NONE;
		pmn->esChequer.et = EVAL_NONE;
		pmn->lt = LUCK_NONE;
		pmn->rLuck = ERR_VAL;
		pmn->stCube = SKILL_NONE;
		pmn->stMove = SKILL_NONE;
		memcpy( pmn->anMove, ml.amMoves[ i ].anMove,
			sizeof( pmn->anMove ) );

		if ( fTutor && fTutorChequer && !GiveAdvice ( GoodMove( pmn ) )) {
                  free ( pmn ); /* garbage collect */
                  return;
                }

#if USE_GTK        
		/* There's no point delaying here. */
		if( nTimeout ) {
		    gtk_timeout_remove( nTimeout );
		    nTimeout = 0;
		}
		
		if ( fX ) {
		    outputnew ();
		    ShowAutoMove( ms.anBoard, pmn->anMove );
		    outputx ();
		}
#endif
		
		AddMoveRecord( pmn );
#if USE_GTK
		/* Don't animate this move. */
		fLastMove = FALSE;
#endif
		TurnDone();
		
		return;
	    }
	}
    }
    
    outputl( _("Illegal move.") );
}

extern void CommandNewGame( char *sz ) {

    if( ms.nMatchTo && ( ms.anScore[ 0 ] >= ms.nMatchTo ||
			 ms.anScore[ 1 ] >= ms.nMatchTo ) ) {
	outputl( _("The match is already over.") );

	return;
    }

    if( ms.gs == GAME_PLAYING ) {
	if( fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( _("Are you sure you want to start a new game, "
			     "and discard the one in progress? ") ) )
		return;
	}
    
        PopGame( plGame, TRUE );
    
    }
    
    fComputing = TRUE;
    
    NewGame();

    if( fInterrupt )
	return;

    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN )
	ShowBoard();
    else
	if( !ComputerTurn() )
	    TurnDone();

    fComputing = FALSE;
}

extern void ClearMatch( void ) {

    char ***pppch;
    static char **appch[] = { &mi.pchRating[ 0 ], &mi.pchRating[ 1 ],
			      &mi.pchEvent, &mi.pchRound, &mi.pchPlace,
			      &mi.pchAnnotator, &mi.pchComment, NULL };
    
    ms.nMatchTo = 0;

    ms.cGames = ms.anScore[ 0 ] = ms.anScore[ 1 ] = 0;
    ms.fMove = ms.fTurn = -1;
    ms.fCrawford = FALSE;
    ms.fPostCrawford = FALSE;
    ms.gs = GAME_NONE;
    IniStatcontext( &scMatch );

    for( pppch = appch; *pppch; pppch++ )
	if( **pppch ) {
	    free( **pppch );
	    **pppch = NULL;
	}

    mi.nYear = 0;
}

extern void FreeMatch( void ) {

    PopGame( lMatch.plNext->p, TRUE );
    IniStatcontext( &scMatch );
}

extern void SetMatchDate( matchinfo *pmi ) {

    time_t t = time( NULL );
    struct tm *ptm = localtime( &t );
    
    pmi->nYear = ptm->tm_year + 1900;;
    pmi->nMonth = ptm->tm_mon + 1;
    pmi->nDay = ptm->tm_mday;
}

extern void CommandNewMatch( char *sz ) {

    int n;

    if( !sz || !*sz )
	n = nDefaultLength;
    else
	n = ParseNumber( &sz );

    if( n < 1 ) {
	outputl( _("You must specify a valid match length (1 or longer).") );

	return;
    }

    /* Check that match equity table is large enough */

    if ( n > MAXSCORE ) {
       outputf ( _("GNU Backgammon is compiled with support only for "
                 "matches of length %i\n"
                 "and below\n"),
                 MAXSCORE );
       return;
    }

    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( _("Are you sure you want to start a new match, "
			 "and discard the game in progress? ") ) )
	    return;
    }
    
    FreeMatch();
    ClearMatch();
    
    plLastMove = NULL;

    ms.nMatchTo = n;
    ms.bgv = bgvDefault;
    ms.fCubeUse = fCubeUse;
    ms.fJacoby = fJacoby;

    SetMatchDate( &mi );
    
    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    outputf( _("A new %d point match has been started.\n"), n );

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
	    
	if( !GetInputYN( _("Are you sure you want to start a new session, "
			 "and discard the game in progress? ") ) )
	    return;
    }
    
    FreeMatch();
    ClearMatch();
    
    ms.bgv = bgvDefault;
    ms.fCubeUse = fCubeUse;
    ms.fJacoby = fJacoby;
    plLastMove = NULL;

    SetMatchDate( &mi );
    
    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    outputl( _("A new session has been started.") );
    
#if USE_GUI
    if( fX )
	ShowBoard();
#endif
    
    if( fAutoGame )
	CommandNewGame( NULL );
}

static void UpdateGame( int fShowBoard ) {
    
    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );

#if USE_GUI
    if( fX || ( fShowBoard && fDisplay ) )
	ShowBoard();
#else
    if( fShowBoard && fDisplay )
	ShowBoard();
#endif
}

#if USE_GTK
static int GameIndex( list *plGame ) {

    list *pl;
    int i;

    for( i = 0, pl = lMatch.plNext; pl->p != plGame && pl != &lMatch;
	 pl = pl->plNext, i++ )
	;

    if( pl == &lMatch )
	return -1;
    else
	return i;
}
#endif

extern void ChangeGame( list *plGameNew ) {

#if USE_GTK
    list *pl;
#endif
    
    plLastMove = ( plGame = plGameNew )->plNext;
    
#if USE_GTK
    if( fX ) {
	GTKFreeze();
	GTKClearMoveRecord();

	for( pl = plGame->plNext; pl->p; pl = pl->plNext ) {
	    GTKAddMoveRecord( pl->p );
            FixMatchState ( &ms, pl->p );
	    ApplyMoveRecord( &ms, plGame, pl->p );
	}

	GTKSetGame( GameIndex( plGame ) );
	GTKThaw();
    }
#endif
    
    CalculateBoard();
    
    UpdateGame( FALSE );

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
	outputl( _("If you specify a parameter to the `next game' command, it "
		 "must be a positive number (the count of games to step "
		 "ahead).") );
	return;
    }

    for( pl = lMatch.plNext; pl->p != plGame && pl != &lMatch; 
         pl = pl->plNext )
	;

    if ( pl->p != plGame )
      /* current game not found */
      return;
    
    for( ; n && pl->plNext->p; n--, pl = pl->plNext )
	;

    if( pl->p == plGame )
	return;

    ChangeGame( pl->p );
}

extern void
CommandFirstGame( char *sz ) {

  ChangeGame( lMatch.plNext->p );

}

static void CommandNextRoll( char *sz ) {

    moverecord *pmr;

#if USE_GTK
    BoardData *bd = NULL;

    if (fX) {
      bd = BOARD( pwBoard )->board_data;
	/* Make sure dice aren't rolled */
	bd->diceShown = DICE_ON_BOARD;
    }
#endif
    
    if( !plLastMove || !plLastMove->plNext ||
	!( pmr = plLastMove->plNext->p ) )
	/* silently ignore */
	return;

    if( pmr->mt != MOVE_NORMAL || ms.anDice[ 0 ] )
	/* to skip over the "dice roll" for anything other than a normal
	   move, or if the dice are already rolled, just skip the entire
	   move */
	return CommandNext( NULL );

    CalculateBoard();

    ms.gs = GAME_PLAYING;
    ms.fMove = ms.fTurn = pmr->n.fPlayer;
	
    ms.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
    ms.anDice[ 1 ] = pmr->n.anRoll[ 1 ];

#if USE_GTK
	/* Make sure dice are shown */
    if( fX ) {      
	bd->diceRoll[0] = !ms.anDice[0];
    }
#endif

    if ( plLastMove->plNext && plLastMove->plNext->p )
      FixMatchState ( &ms, plLastMove->plNext->p );

    UpdateGame( FALSE );
}

static void CommandNextRolled( char *sz ) {

    moverecord *pmr;

    /* goto next move */

    CommandNext ( NULL );

    if( plLastMove && plLastMove->plNext &&
        ( pmr = plLastMove->plNext->p ) && pmr->mt == MOVE_NORMAL )
       CommandNextRoll ( NULL );

}

static int MoveIsMarked (moverecord *pmr) {
  if (pmr == 0)
	return FALSE;

  switch( pmr->mt ) {
  case MOVE_NORMAL:
    return badSkill(pmr->n.stMove) || badSkill(pmr->n.stCube);

  case MOVE_DOUBLE:
  case MOVE_TAKE:
  case MOVE_DROP:
    return badSkill(pmr->d.st);

  default:
    break;
  }

  return FALSE;
}

static void ShowMark( moverecord *pmr ) {

#if USE_GTK  
     BoardData *bd = NULL;

     if ( fX )
       bd = BOARD( pwBoard )->board_data;
#endif
    /* Show the dice roll, if the chequer play is marked but the cube
       decision is not. */
    if( pmr->mt == MOVE_NORMAL && pmr->n.stCube == SKILL_NONE &&
	pmr->n.stMove != SKILL_NONE ) {
	ms.gs = GAME_PLAYING;
	ms.fMove = ms.fTurn = pmr->n.fPlayer;
	
	ms.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
	ms.anDice[ 1 ] = pmr->n.anRoll[ 1 ];
    }

#if USE_GTK
    if( fX ) {
      /* Don't roll dice */
      bd->diceShown = DICE_ON_BOARD;
      /* Make sure dice are shown */
      bd->diceRoll[0] = !ms.anDice[0];
    }
#endif

}

int
InternalCommandNext(int const fMarkedMoves, int n)
{
  int done = 0;
  
  if( fMarkedMoves ) {
    list* p = plLastMove;
    list* orig_p = p;
    matchstate SaveMs;
    moverecord* pmr = 0;
      
    memcpy( &SaveMs, &ms, sizeof( matchstate ) );
    /* we need to increment the count if we're pointing to a marked
       move */
    if ( p->plNext->p && MoveIsMarked( (moverecord *) p->plNext->p ) )
      ++n;
	
    while( p->plNext->p ) {
      p = p->plNext;
      pmr = (moverecord*) p->p;
      FixMatchState ( &ms, pmr);
      ApplyMoveRecord( &ms, plGame, pmr);
      if( MoveIsMarked (pmr) && ( --n <= 0 ) )
	break;
    }

    if ((p->plNext->p == 0)  /* ran out of moves to examine */
	&& (!MoveIsMarked (pmr) /* last move is not an error */
	    || (p == orig_p)    /* or there were no moves to look at on */
            || (p == orig_p->plNext))) /* or we were on the last move */   
      {
	/* didn't find the requested move, put things back */
	memcpy( &ms, &SaveMs, sizeof( matchstate ) );
	plLastMove = orig_p;
	FixMatchState ( &ms, plLastMove->p );
	ApplyMoveRecord( &ms, plGame, plLastMove->p );
	return 0;
      }
	
    plLastMove = p->plPrev;
    CalculateBoard();
    ShowMark( pmr );		
  } else {
    while( n-- && plLastMove->plNext->p ) {
      plLastMove = plLastMove->plNext;
      FixMatchState ( &ms, plLastMove->p );
      ApplyMoveRecord( &ms, plGame, plLastMove->p );

      ++done;
    }
  }
    
  UpdateGame( FALSE );

  if ( plLastMove->plNext && plLastMove->plNext->p )
    FixMatchState ( &ms, plLastMove->plNext->p );

  SetMoveRecord( plLastMove->p );

  return done;
}
			 
extern void
CommandNext( char* sz )
{
  int n;
  char* pch;
  int fMarkedMoves = FALSE;
    
  if( !plGame ) {
    outputl( _("No game in progress (type `new game' to start one).") );
    return;
  }
    
  n = 1;

  if( ( pch = NextToken( &sz ) ) ) {
    if( !strncasecmp( pch, "game", strlen( pch ) ) ) {
      CommandNextGame( sz );
      return;
    } else if( !strncasecmp( pch, "roll", strlen( pch ) ) ) {
      CommandNextRoll( sz );
      return;
    } else if( !strncasecmp( pch, "rolled", strlen( pch ) ) ) {
      CommandNextRolled( sz );
      return;
    } else if( !strncasecmp( pch, "marked", strlen( pch ) ) ) {
      fMarkedMoves = TRUE;
      if( ( pch = NextToken( &sz ) ) ) {
	n = ParseNumber( &pch );
      }
    } else
      n = ParseNumber( &pch );
  }
    
  if( n < 1 ) {
    outputl( _("If you specify a parameter to the `next' command, it must "
	       "be a positive number (the count of moves to step ahead).") );
    return;
  }
    
  InternalCommandNext(fMarkedMoves, n);
}

extern void CommandPlay( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( _("No game in progress (type `new game' to start one).") );

	return;
    }

    if( ap[ ms.fTurn ].pt == PLAYER_HUMAN ) {
	outputl( _("It's not the computer's turn to play.") );

	return;
    }

    fComputing = TRUE;
    
    if( !ComputerTurn() )
	TurnDone();

    fComputing = FALSE;
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
	outputl( _("If you specify a parameter to the `previous game' command, "
		 "it must be a positive number (the count of games to step "
		 "back).") );
	return;
    }
    
    for( pl = lMatch.plNext; pl->p != plGame && pl != &lMatch; 
         pl = pl->plNext )
	;
    
    if ( pl->p != plGame )
      /* current game not found */
      return;
    
    for( ; n && pl->plPrev->p; n--, pl = pl->plPrev )
	;

    if( pl->p == plGame )
	return;

    ChangeGame( pl->p );
}

static void CommandPreviousRoll( char *sz ) {

    moverecord *pmr;
    
    if( !plLastMove || !plLastMove->p )
	/* silently ignore */
	return;

    if( !( pmr = plLastMove->plNext->p ) || pmr->mt != MOVE_NORMAL ||
	!ms.anDice[ 0 ] ) {
	/* to skip back over the "dice roll" for anything other than a normal
	   move, or if the dice haven't been rolled, just skip the entire
	   move */
	CommandPrevious( NULL );

	if( plLastMove->plNext->p != pmr ) {
	    pmr = plLastMove->plNext->p;

	    if( pmr && pmr->mt == MOVE_NORMAL )
		/* We've stepped back a whole move; now we need to recover
		   the previous dice roll. */
		CommandNextRoll( NULL );
	}
	
	return;
    }

    CalculateBoard();
    
    ms.anDice[ 0 ] = 0;
    ms.anDice[ 1 ] = 0;
    
    FixMatchState ( &ms, pmr );

    UpdateGame( FALSE );
}

static void CommandPreviousRolled( char *sz ) {

    moverecord *pmr;

    if( !plLastMove || !plLastMove->p )
        /* silently ignore */
        return;

    /* step back and boogie */

    CommandPrevious ( NULL );

    if ( plLastMove && plLastMove->plNext &&
         ( pmr = plLastMove->plNext->p ) && pmr->mt == MOVE_NORMAL )
       CommandNextRoll ( NULL );

}

extern void CommandPrevious( char *sz ) {

    int n;
    char *pch;
    int fMarkedMoves = FALSE;
    list *p;
    moverecord *pmr = NULL;
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
	n = 1;

    if( ( pch = NextToken( &sz ) ) ) {
	if( !strncasecmp( pch, "game", strlen( pch ) ) ) {
	    CommandPreviousGame( sz );
	    return;
	} else if( !strncasecmp( pch, "rolled", strlen( pch ) ) ) {
	    CommandPreviousRolled( sz );
	    return;
	} else if( !strncasecmp( pch, "roll", strlen( pch ) ) ) {
	    CommandPreviousRoll( sz );
	    return;
	} else if( !strncasecmp( pch, "marked", strlen( pch ) ) ) {
	  fMarkedMoves = TRUE;
	  if( ( pch = NextToken( &sz ) ) ) {
	    n = ParseNumber( &pch );
	  }
    } else
	  n = ParseNumber( &pch );	
    }
    
    if( n < 1 ) {
	outputl( _("If you specify a parameter to the `previous' command, it "
		 "must be a positive number (the count of moves to step "
		 "back).") );
	return;
    }

    if( fMarkedMoves ) {
	p =  plLastMove;
	while( ( p->p ) != 0 ) {
	    pmr = (moverecord *) p->p;
	    if( MoveIsMarked (pmr) && ( --n <= 0 ) )
		break;
	    
	    p = p->plPrev;
	}

	if (p->p == 0)
	    return;

	plLastMove = p->plPrev;
    } else {
	while( n-- && plLastMove->plPrev->p )
	    plLastMove = plLastMove->plPrev;
    }

    CalculateBoard();
    
    if( pmr )
	ShowMark( pmr );

    UpdateGame( FALSE );

    if ( plLastMove->plNext && plLastMove->plNext->p )
      FixMatchState ( &ms, plLastMove->plNext->p );

    SetMoveRecord( plLastMove->p );
}

extern void CommandRedouble( char *sz ) {

    moverecord *pmr;

    if( ms.nMatchTo > 0 ) {
	outputl( _("Redoubles are not permitted during match play.") );

	return;
    }

    if( !nBeavers ) {
	outputl( _("Beavers are disabled (see `help set beavers').") );

	return;
    }

    if( ms.cBeavers >= nBeavers ) {
	if( nBeavers == 1 )
	    outputl( _("Only one beaver is permitted (see `help set "
		     "beavers').") );
	else
	    outputf( _("Only %d beavers are permitted (see `help set "
		     "beavers').\n"), nBeavers );

	return;
    }
    
    if( ms.gs != GAME_PLAYING || !ms.fDoubled ) {
	outputl( _("The cube must have been offered before you can redouble "
		 "it.") );

	return;
    }
    
    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    if( ms.nCube >= ( MAX_CUBE >> 1 ) ) {
	outputf( _("The cube is already at %d; you can't double any more.\n"),
                 MAX_CUBE );
	return;
    }
    
    pmr = malloc( sizeof( pmr->d ) );

#if 0

    removed until gnubg/GoodDouble handles beavers and raccoons correctly.

    if ( fTutor && fTutorCube && !GiveAdvice( GoodDouble( TRUE, pmr ) ))
      return;
#endif

    playSound ( SOUND_REDOUBLE );

    if( fDisplay )
	outputf( _("%s accepts and immediately redoubles to %d.\n"),
		ap[ ms.fTurn ].szName, ms.nCube << 2 );
    
    ms.fCubeOwner = !ms.fMove;
    UpdateSetting( &ms.fCubeOwner );
    
    pmr->mt = MOVE_DOUBLE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    if( !LinkToDouble( pmr ) ) {
	pmr->d.CubeDecPtr = &pmr->d.CubeDec;
      pmr->d.nAnimals = 0;
    pmr->d.CubeDecPtr->esDouble.et = EVAL_NONE;
    }
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
	outputl( _("You can only reject if the cube or a resignation has been "
	      "offered.") );
}

extern void CommandResign( char *sz ) {

    char *pch;
    int cch;
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( _("You must be playing a game if you want to resign it.") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
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

	outputf( _("Unknown keyword `%s' -- try `help resign'.\n"), pch );
	
	return;
    }

    if( ms.fResigned <= ms.fResignationDeclined ) {
	ms.fResigned = 0;
	
	outputf( _("%s has already declined your offer of a %s.\n"),
		 ap[ !ms.fTurn ].szName,
		 gettext ( aszGameResult[ ms.fResignationDeclined - 1 ] ) );

	return;
    }
    
    if( fDisplay )
	outputf( _("%s offers to resign a %s.\n"), ap[ ms.fTurn ].szName,
		gettext ( aszGameResult[ ms.fResigned - 1 ] ) );

    ms.fTurn = !ms.fTurn;

    playSound ( SOUND_RESIGN );
    
    TurnDone();
}

/* evaluate wisdom of not having doubled/redoubled */

static skilltype ShouldDouble ( void ) {

    float arDouble[ 4 ], aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    cubeinfo ci;
    cubedecision cd;
    float rDeltaEquity;
    int      fAnalyseCubeSave = fAnalyseCube;
    evalcontext *pec;
    evalsetup es, *pes;

    /* reasons that doubling is not an issue */
    if( (ms.gs != GAME_PLAYING) || 
		ms.anDice[ 0 ]         || 
		ms.fDoubled || 
		ms.fResigned ||
                ! ms.fCubeUse ||
		(ap[ ms.fTurn ].pt != PLAYER_HUMAN )) {

      return (SKILL_NONE);
    }

	if (fTutorAnalysis)
	  pes = &esAnalysisCube;
	else 
	  pes = &esEvalCube;
	
	pec = &pes->ec;

	GetMatchStateCubeInfo( &ci, &ms );

	fAnalyseCube = TRUE;
	if( !GetDPEq ( NULL, NULL, &ci ) ) {
	  fAnalyseCube = fAnalyseCubeSave;
	  return (SKILL_NONE);
	}

	/* Give hint on cube action */

	ProgressStart( _("Considering cube action...") );
	if ( GeneralCubeDecisionE( aarOutput, ms.anBoard, &ci, pec, pes) < 0) {
	  ProgressEnd();
	  fAnalyseCube = fAnalyseCubeSave;
	  return (SKILL_NONE);;
	}
	ProgressEnd();

        /* stored cube decision for hint */

        ec2es ( &es, pec );
        UpdateStoredCube ( aarOutput,
                           aarOutput, /* whatever */
                           &es, &ms );

	    
	cd = FindCubeDecision ( arDouble, GCCCONSTAHACK aarOutput, &ci );  

	switch ( cd ) {
	case DOUBLE_TAKE:
	case DOUBLE_BEAVER:
	case REDOUBLE_TAKE:

 	  rDeltaEquity = arDouble [OUTPUT_NODOUBLE] - arDouble [OUTPUT_TAKE];
	  break;

	case DOUBLE_PASS:
	case REDOUBLE_PASS:

	  rDeltaEquity = arDouble [OUTPUT_NODOUBLE] - arDouble [OUTPUT_DROP];
	  break;

	default:
	  return (SKILL_NONE);
	}

	return Skill (rDeltaEquity);

}

extern void 
CommandRoll( char *sz ) {

  movelist ml;
  movenormal *pmn;
  moverecord *pmr;

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("No game in progress (type `new game' to start one).") );

    return;
  }
    
  if( ap[ ms.fTurn ].pt != PLAYER_HUMAN ) {
    outputl( _("It is the computer's turn -- type `play' to force it to "
             "move immediately.") );
    return;
  }

  if( ms.fDoubled ) {
    outputf( _("Please wait for %s to consider the cube before "
             "moving.\n"), ap[ ms.fTurn ].szName );

    return;
  }    

  if( ms.fResigned ) {
    outputl( _("Please resolve the resignation first.") );

    return;
  }

  if( ms.anDice[ 0 ] ) {
    outputl( _("You have already rolled the dice.") );

    return;
  }
    
  if ( fTutor && fTutorCube && !GiveAdvice( ShouldDouble() ))
	  return;

  if ( ! fCheat || CheatDice ( ms.anDice, &ms, afCheatRoll[ ms.fMove ] ) )
    if( RollDice ( ms.anDice, rngCurrent ) < 0 )
      return;

  pmr = malloc( sizeof( pmr->sd ) );
  pmr->mt = MOVE_SETDICE;
  pmr->sd.sz = NULL;
  pmr->sd.anDice[ 0 ] = ms.anDice[ 0 ];
  pmr->sd.anDice[ 1 ] = ms.anDice[ 1 ];
  pmr->sd.fPlayer = ms.fTurn;
  pmr->sd.lt = LUCK_NONE;
  pmr->sd.rLuck = ERR_VAL;
  AddMoveRecord( pmr );

  InvalidateStoredMoves();
  
  DiceRolled();      

#if USE_GTK        
  if ( fX ) {

    outputnew ();
    outputf ( _("%s rolls %1i and %1i.\n"),
              ap [ ms.fTurn ].szName, ms.anDice[ 0 ], ms.anDice[ 1 ] );
    outputx ();

  }
#endif

  ResetDelayTimer();
    
  if( !GenerateMoves( &ml, ms.anBoard, ms.anDice[ 0 ], ms.anDice[ 1 ],
		      FALSE ) ) {

    playSound ( ap[ ms.fTurn ].pt == PLAYER_HUMAN ? 
                SOUND_HUMAN_DANCE : SOUND_BOT_DANCE );

    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->anMove[ 0 ] = -1;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->esDouble.et = EVAL_NONE;
    pmn->esChequer.et = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = ERR_VAL;
    pmn->stCube = SKILL_NONE;
    pmn->stMove = SKILL_NONE;
    
    ShowAutoMove( ms.anBoard, pmn->anMove );

    AddMoveRecord( pmn );

    TurnDone();
  } else if( ml.cMoves == 1 && ( fAutoMove || ( ClassifyPosition( ms.anBoard, 
                                                                  ms.bgv )
                                                <= CLASS_BEAROFF1 &&
                                                fAutoBearoff ) ) ) {

    playSound ( SOUND_MOVE );

    pmn = malloc( sizeof( *pmn ) );
    pmn->mt = MOVE_NORMAL;
    pmn->sz = NULL;
    pmn->anRoll[ 0 ] = ms.anDice[ 0 ];
    pmn->anRoll[ 1 ] = ms.anDice[ 1 ];
    pmn->fPlayer = ms.fTurn;
    pmn->ml.cMoves = 0;
    pmn->ml.amMoves = NULL;
    pmn->esDouble.et = EVAL_NONE;
    pmn->esChequer.et = EVAL_NONE;
    pmn->lt = LUCK_NONE;
    pmn->rLuck = ERR_VAL;
    pmn->stCube = SKILL_NONE;
    pmn->stMove = SKILL_NONE;
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
	outputl( _("The cube must have been offered before you can take it.") );

	return;
    }

    if( ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputerDecision ) {
	outputl( _("It is the computer's turn -- type `play' to force it to "
		 "move immediately.") );
	return;
    }

    playSound ( SOUND_TAKE );

    pmr = malloc( sizeof( pmr->d ) );
    pmr->d.mt = MOVE_TAKE;
    pmr->d.sz = NULL;
    pmr->d.fPlayer = ms.fTurn;
    if( !LinkToDouble( pmr ) ) {
      free (pmr);
      return;
    }

    pmr->d.st = SKILL_NONE;

    if ( fTutor && fTutorCube && !GiveAdvice ( ShouldDrop ( FALSE, pmr ) )) {
        free ( pmr ); /* garbage collect */
        return;
    }

    if( fDisplay )
	outputf( _("%s accepts the cube at %d.\n"), ap[ ms.fTurn ].szName,
		 ms.nCube << 1 );
    
    AddMoveRecord( pmr );

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    
    TurnDone();
}

extern void
SetMatchID ( const char *szMatchID ) {

  int anScore[ 2 ], anDice[ 2 ];
  int nMatchTo, fCubeOwner, fMove, fCrawford, nCube;
  int fTurn, fDoubled, fResigned;
  gamestate gs;

  char szID[ 15 ];

  moverecord *pmr;

  if ( ! szMatchID || ! *szMatchID )
     return;

  if( ms.gs == GAME_PLAYING && fConfirm ) {
    if( fInterrupt )
      return;
	    
    if( !GetInputYN( _("Are you sure you want to setup a new match id, "
			 "and discard the game in progress? ") ) )
      return;
  }
    

  if ( ms.gs == GAME_PLAYING )
    strcpy ( szID, PositionID ( ms.anBoard ) );
  else
    strcpy ( szID, "" );

  if ( MatchFromID ( anDice, &fTurn, &fResigned, &fDoubled, &fMove,
                     &fCubeOwner, &fCrawford, 
                     &nMatchTo, anScore, &nCube, (int *) &gs, 
                     szMatchID ) < 0 ) {

    outputf( _("Illegal match ID '%s'\n"), szMatchID );
    outputf( _("Dice %d %d, player on roll %d (turn %d), resigned %d,\n"
               "doubled %d, cube owner %d, crawford game %d,\n"
               "match length %d, score %d-%d, cube %d, game state %d\n"),
             anDice[ 0 ], anDice[ 1 ], fMove, fTurn, fResigned, fDoubled,
             fCubeOwner, fCrawford, nMatchTo, anScore[ 0 ], anScore[ 1 ],
             nCube, (int) gs );
    outputx();
    return;

  }

#if 0
  printf ( "%d %d %d %d %d %d %d %d %d\n",
           nCube, fCubeOwner, fMove, nMatchTo, anScore[ 0 ], anScore[ 1 ],
           fCrawford, anDice[ 0 ], anDice[ 1 ] );
#endif
  
  /* start new match or session */

  FreeMatch();

  ms.cGames = 0;
  ms.nMatchTo = nMatchTo;
  ms.anScore[ 0 ] = anScore[ 0 ];
  ms.anScore[ 1 ] = anScore[ 1 ];
  ms.fCrawford = fCrawford;
  ms.fPostCrawford = ! fCrawford && 
    ( ( anScore[ 0 ] == nMatchTo - 1 ) || 
      ( anScore[ 1] == nMatchTo - 1 ) );
  ms.bgv = bgvDefault; /* FIXME: include bgv in match ID */
  ms.fCubeUse = fCubeUse; /* FIXME: include cube use in match ID */
  ms.fJacoby = fJacoby; /* FIXME: include Jacoby in match ID */
  
  /* start new game */
    
  PopGame ( plGame, TRUE );

  InitBoard ( ms.anBoard, ms.bgv );

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
  pmr->g.fJacoby = ms.fJacoby && !ms.nMatchTo;
  pmr->g.fWinner = -1;
  pmr->g.nPoints = 0;
  pmr->g.fResigned = FALSE;
  pmr->g.nAutoDoubles = 0;
  pmr->g.bgv = ms.bgv;
  pmr->g.fCubeUse = ms.fCubeUse;
  IniStatcontext( &pmr->g.sc );
  AddMoveRecord( pmr );

  ms.gs = gs;
  ms.fMove = fMove;
  ms.fTurn = fTurn;
  ms.fResigned = fResigned;
  ms.fDoubled = fDoubled;
  
  /* Set dice */

  if ( anDice[ 0 ] ) {

    char sz[ 10 ];
  
    sprintf ( sz, "%d %d", anDice[ 0 ], anDice[ 1 ] );
    CommandSetDice ( sz );

  }

  /* set cube */

  if ( fCubeOwner != -1 ) {

    movesetcubepos *pmscp;

    pmscp = malloc( sizeof( *pmscp ) );
    pmscp->mt = MOVE_SETCUBEPOS;
    pmscp->sz = NULL;
    pmscp->fCubeOwner = fCubeOwner;
    
    AddMoveRecord( pmscp );

  }
    
  if ( nCube != 1 ) {

    char sz[ 10 ];

    sprintf ( sz, "%d", nCube );

    CommandSetCubeValue ( sz );

  }

  /* set board to old value */

  if ( strlen ( szID ) )
    CommandSetBoard ( szID );

  /* the following is needed to get resignations correct */

  ms.gs = gs;
  ms.fMove = fMove;
  ms.fTurn = fTurn;
  ms.fResigned = fResigned;
  ms.fDoubled = fDoubled;
  
  UpdateSetting( &ms.gs );
  UpdateSetting( &ms.nCube );
  UpdateSetting( &ms.fCubeOwner );
  UpdateSetting( &ms.fTurn );
  UpdateSetting( &ms.fCrawford );

  /* show board */

  ShowBoard();

}


extern void
FixMatchState(matchstate* pms, const moverecord* pmr )
{
  switch ( pmr->mt ) {
  case MOVE_NORMAL:
    if ( pms->fTurn != pmr->n.fPlayer ) {
      /* previous moverecord is missing */
      SwapSides ( pms->anBoard );
      pms->fMove = pms->fTurn = pmr->n.fPlayer;
    }
    break;
  case MOVE_DOUBLE:
    if ( pms->fTurn != pmr->d.fPlayer ) {
      /* previous record is missing: this must be an normal double */
      SwapSides ( pms->anBoard );
      pms->fMove = pms->fTurn = pmr->d.fPlayer;
    }
    break;
  default:
    /* no-op */
    break;
  }
    
}


extern moverecord *
getCurrentMoveRecord ( int *pfHistory ) {

  static moverecord mrHint;

  /* FIXME: introduce a mrHint that "Hint" and "Eval" fills */

  if ( plLastMove && plLastMove->plNext && plLastMove->plNext->p ) {
    *pfHistory = TRUE;
    return plLastMove->plNext->p;
  }
  else {
    *pfHistory = FALSE;

    if ( ! cmp_matchstate ( &ms, &sm.ms ) ) {

      mrHint.n.mt = MOVE_NORMAL;
      mrHint.n.sz = NULL;
      mrHint.n.anRoll[ 0 ] = ms.anDice[ 0 ];
      mrHint.n.anRoll[ 1 ] = ms.anDice[ 1 ];
      mrHint.n.fPlayer = ms.fTurn;
      mrHint.n.ml = sm.ml;
      mrHint.n.lt = LUCK_NONE;
      mrHint.n.rLuck = ERR_VAL;
      mrHint.n.stMove = SKILL_NONE;

      mrHint.n.iMove = -1;
      mrHint.n.anMove[ 0 ] = -1;

#if USE_GTK
      if ( fX )
        if ( GTKGetMove ( mrHint.n.anMove ) )
          mrHint.n.iMove = locateMove ( ms.anBoard, mrHint.n.anMove, &sm.ml );
#endif


      /* add cube */

      if ( ! memcmp ( &ms.anBoard, &sc.ms.anBoard, sizeof ( ms.anBoard ) ) &&
           ms.fTurn == sc.ms.fTurn && 
           ms.fMove == sc.ms.fMove &&
           ms.fCubeOwner == sc.ms.fCubeOwner &&
           ms.fCrawford == sc.ms.fCrawford &&
           ms.fPostCrawford == sc.ms.fPostCrawford &&
           ms.nCube == sc.ms.nCube &&
           ms.nMatchTo == sc.ms.nMatchTo &&
           ( ! ms.nMatchTo || ( ms.anScore[ 0 ] == sc.ms.anScore[ 0 ] &&
                                ms.anScore[ 1 ] == sc.ms.anScore[ 1 ] ) ) ) {

        mrHint.n.esDouble = sc.es;
        mrHint.n.stCube = SKILL_NONE;
        memcpy ( mrHint.n.aarOutput, sc.aarOutput, sizeof ( sc.aarOutput ) );
        memcpy ( mrHint.n.aarStdDev, sc.aarStdDev, sizeof ( sc.aarStdDev ) );

      }
      else {
        mrHint.n.esDouble.et = EVAL_NONE;
      }

      return &mrHint;

    }
    else if ( ! cmp_matchstate ( &ms, &sc.ms ) ) {

      mrHint.d.mt = MOVE_DOUBLE;
      mrHint.d.sz = NULL;
      mrHint.d.fPlayer = ms.fTurn;
      mrHint.d.nAnimals = 0;
      mrHint.d.CubeDecPtr = &mrHint.d.CubeDec;
      mrHint.d.CubeDecPtr->esDouble = sc.es;
      mrHint.d.st = SKILL_NONE;
      memcpy ( mrHint.d.CubeDecPtr->aarOutput, sc.aarOutput, 
               sizeof ( sc.aarOutput ) );
      memcpy ( mrHint.d.CubeDecPtr->aarStdDev, sc.aarStdDev, 
               sizeof ( sc.aarStdDev ) );
      
      return &mrHint;

    }
    else
      return NULL;

  }

}


static int
CheatDice ( int anDice[ 2 ], matchstate *pms, const int fBest ) {

  static evalcontext ec0ply = { FALSE, 0, 0, TRUE, 0.0 };
  static cubeinfo ci;
    
  GetMatchStateCubeInfo( &ci, pms );
  
  OptimumRoll ( pms->anBoard, &ci, &ec0ply, fBest, anDice );
  
  if ( ! anDice[ 0 ] )
    return -1;

  return 0;

}

/* 
 * find best (or worst roll possible)
 *
 */

typedef struct _rollequity {
  float r;
  int anDice[ 2 ];
} rollequity;

static int
CompareRollEquity( const void *p1, const void *p2 ) {

  const rollequity *per1 = (rollequity *) p1;
  const rollequity *per2 = (rollequity *) p2;

  if ( per1->r > per2->r )
    return -1;
  else if ( per1->r < per2->r )
    return +1;
  else
    return 0;

}

extern void
OptimumRoll ( int anBoard[ 2 ][ 25 ], 
              const cubeinfo *pci, const evalcontext *pec,
              const int fBest, int anDice[ 2 ] ) {

  int anBoardTemp[ 2 ][ 25 ], i, j, k;
  float ar[ NUM_ROLLOUT_OUTPUTS ];
  cubeinfo ciOpp;
  rollequity are[ 21 ];
  float r;

  memcpy ( &ciOpp, pci, sizeof ( cubeinfo ) );
  ciOpp.fMove = ! pci->fMove;

  anDice[ 0 ] = anDice[ 1 ] = -1;

  for( i = 0, k = 0; i < 6; i++ )
    for( j = 0; j <= i; j++, ++k ) {
      memcpy( &anBoardTemp[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
              2 * 25 * sizeof( int ) );
      
      /* Find the best move for each roll at ply 0 only. */
      if( FindBestMove( NULL, i + 1, j + 1, anBoardTemp, 
                        (cubeinfo *) pci, NULL, defaultFilters ) < 0 )
        return;
      
      SwapSides( anBoardTemp );
      
      /* FIXME should we use EvaluatePositionCubeful here? */

      if ( GeneralEvaluationE( ar, anBoardTemp, &ciOpp, (evalcontext *) pec ) )
        return;

      r = 
        ( pec->fCubeful ) ? ar[ OUTPUT_CUBEFUL_EQUITY ] : ar[ OUTPUT_EQUITY ];

      if ( pci->nMatchTo )
        r = 1.0 - r;
      else
        r = -r;

      are[ k ].r = r;
      are[ k ].anDice[ 0 ] = i + 1;
      are[ k ].anDice[ 1 ] = j + 1;

    }

  qsort( are, 21, sizeof ( rollequity ), CompareRollEquity );

  anDice[ 0 ] = are[ fBest ].anDice[ 0 ];
  anDice[ 1 ] = are[ fBest ].anDice[ 1 ];

}

/* routine to link doubles/beavers/raccoons/etc and their eventual
   take/drop decisions into a single evaluation data block
   returns NULL if there is no preceding double record to link to
*/

moverecord *LinkToDouble( moverecord *pmr) {

  moverecord *prev;
  

  if( !plLastMove || ( ( prev = plLastMove->p ) == 0 ) ||
	  prev->mt != MOVE_DOUBLE )
	return 0;

  /* link the evaluation data */
  pmr->d.CubeDecPtr = prev->d.CubeDecPtr;

  /* if this is part of a chain of doubles, add to the count
     nAnimals will be 0 for the first double, 1 for the beaver,
     2 for the racoon, etc.
  */
  if( pmr->d.mt == MOVE_DOUBLE )
    pmr->d.nAnimals = 1 + prev->d.nAnimals;

  return pmr;
}

/*
 * getFinalScore:
 * fills anScore[ 2 ] with the final score of the match/session
 */

extern int
getFinalScore( int* anScore )
{
	list* plGame;

	/* find last game */
	for( plGame = lMatch.plNext; plGame->plNext->p; plGame = plGame->plNext )
		;

	if ( plGame->p && ( (list *) plGame->p )->plNext &&
	     ( (list *) plGame->p )->plNext->p &&
	     ( (moverecord *) ( (list *) plGame->p )->plNext->p )->mt == MOVE_GAMEINFO &&
	     ( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.mt == MOVE_GAMEINFO
	   )
	{
		anScore[ 0 ] = ( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.anScore[ 0 ];
		anScore[ 1 ] = ( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.anScore[ 1 ];
		if ( ( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.fWinner != -1 )
			anScore[ ( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.fWinner ] +=
				( (moverecord *) ( (list *) plGame->p )->plNext->p )->g.nPoints;
		return TRUE;
	}
	
	return FALSE;
}
