/*
 * xgame.c
 *
 * by Gary Wong, 1997-1999
 *
 * $Id$
 */

#include "config.h"

#include <ext.h>
#include <extwin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_RAND_R
#include <rand_r.h>
#endif

#include "xboard.h"
#include "xgame.h"
#include "backgammon.h"
#include "drawboard.h"
#include "positionid.h"

typedef enum _statsid {
    STATS_LNAME, STATS_XNAME, STATS_ONAME,
    STATS_LSCORE, STATS_XSCORE, STATS_OSCORE,
    STATS_LMATCH, STATS_MATCH,
    STATS_MOVE, STATS_BOARD
} statsid;

typedef struct _statsdata {
    gamedata *pgd;
    extwindow *paewnd;
    movelist ml; /* list of all legal moves (including incomplete ones), for
		    visual feedback if the user attempts to move */
    move *amMoves, *pm;
} statsdata;

static extquark eq_justification = { "justification", 0 };

static extdefault aedStatsTitleL[] = {
    { &eq_font, "-B&H-Lucida-Bold-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_background, "#000" },
    { &eq_foreground, "#F00" },
    { &eq_justification, "l" },
    { NULL, NULL }
};

static extdefault aedStatsTitleR[] = {
    { &eq_font, "-B&H-Lucida-Bold-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_background, "#000" },
    { &eq_foreground, "#F00" },
    { &eq_justification, "r" },
    { NULL, NULL }
};

static extdefault aedStatsTextL[] = {
    { &eq_font, "-B&H-Lucida-Medium-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_background, "#000" },
    { &eq_foreground, "#FFF" },
    { &eq_justification, "l" },
    { NULL, NULL }
};

static extdefault aedStatsTextR[] = {
    { &eq_font, "-B&H-Lucida-Medium-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_background, "#000" },
    { &eq_foreground, "#FFF" },
    { &eq_justification, "r" },
    { NULL, NULL }
};

static extdefault aedStatsTextC[] = {
    { &eq_font, "-B&H-Lucida-Medium-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_background, "#000" },
    { &eq_foreground, "#FFF" },
    { &eq_justification, "c" },
    { NULL, NULL }
};

static extdefault aedStatsTextPlainC[] = {
    { &eq_font, "-Misc-Fixed-Medium-R-Normal--12-*-*-*-*-*-iso8859-1" },
    { &eq_background, "#000" },
    { &eq_foreground, "#FFF" },
    { &eq_justification, "c" },
    { NULL, NULL }
};

extwindowspec aewsStats[] = {
    { "lname", &ewcText, aedStatsTitleL, "Name", STATS_LNAME },
    { "xname", &ewcText, aedStatsTextL, NULL, STATS_XNAME },
    { "oname", &ewcText, aedStatsTextL, NULL, STATS_ONAME },
    { "lscore", &ewcText, aedStatsTitleR, "Score", STATS_LSCORE },
    { "xscore", &ewcText, aedStatsTextR, NULL, STATS_XSCORE },
    { "oscore", &ewcText, aedStatsTextR, NULL, STATS_OSCORE },
    { "lmatch", &ewcText, aedStatsTitleR, "Match", STATS_LMATCH },
    { "match", &ewcText, aedStatsTextL, NULL, STATS_MATCH },
    { "move", &ewcText, aedStatsTextC, NULL, STATS_MOVE },
    { "board", &ewcText, aedStatsTextPlainC, NULL, STATS_BOARD }
};

static unsigned int nSeed = 1; /* for rand_r */
#define RAND ( ( (unsigned int) rand_r( &nSeed ) ) & RAND_MAX )

static int StatsRedraw( extwindow *pewnd, statsdata *psd, int fClear ) {

    int cxyChequer = psd->pgd->nBoardSize > 2 ? 6 * psd->pgd->nBoardSize : 12;
    
    if( fClear )
	XClearWindow( pewnd->pdsp, pewnd->wnd );

    XCopyPlane( pewnd->pdsp, psd->pgd->pmMask, pewnd->wnd, psd->pgd->gcAnd, 0,
		0, cxyChequer, cxyChequer, 0, 12, 1 );
    XCopyArea( pewnd->pdsp, psd->pgd->pmX, pewnd->wnd, psd->pgd->gcOr, 0, 0,
	       cxyChequer, cxyChequer, 0, 12 );
    
    XCopyPlane( pewnd->pdsp, psd->pgd->pmMask, pewnd->wnd, psd->pgd->gcAnd, 0,
		0, cxyChequer, cxyChequer, 0, cxyChequer + 18, 1 );
    XCopyArea( pewnd->pdsp, psd->pgd->pmO, pewnd->wnd, psd->pgd->gcOr, 0, 0,
	       cxyChequer, cxyChequer, 0, cxyChequer + 18 );

    return 0;
}

static int StatsConfigure( extwindow *pewnd, statsdata *psd,
			   XConfigureEvent *pxcev ) {
    int i;
    static int ax[] = { 6, 112 };
    static int acx[] = { 100, 50 };
    int cxyChequer = psd->pgd->nBoardSize > 2 ? 6 * psd->pgd->nBoardSize : 12;

    for( i = STATS_LNAME / 3; i <= STATS_LSCORE / 3; i++ ) {
	XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ i * 3 ].wnd, ax[ i ] +
			   cxyChequer, 0, acx[ i ], 12 );
	XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ i * 3 + 1 ].wnd, ax[ i ] +
			   cxyChequer, 6 + cxyChequer / 2, acx[ i ], 12 );
	XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ i * 3 + 2 ].wnd, ax[ i ] +
			   cxyChequer, 12 + cxyChequer * 3 / 2, acx[ i ], 12 );
    }

    XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ STATS_LMATCH ].wnd,
		       20, cxyChequer * 2 + 24, 50, 12 );
    XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ STATS_MATCH ].wnd,
		       76, cxyChequer * 2 + 24, 100, 12 );

    XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ STATS_MOVE ].wnd,
		       0, cxyChequer * 2 + 42, pewnd->cx >> 1, 12 );
    XMoveResizeWindow( pewnd->pdsp, psd->paewnd[ STATS_BOARD ].wnd,
		       pewnd->cx >> 1, cxyChequer * 2 + 42,
		       pewnd->cx >> 1, 12 );
    
    StatsRedraw( pewnd, pewnd->pv, True );
    
    return 0;
}

static void StatsPreCreate( extwindow *pewnd ) {

    statsdata *psd = malloc( sizeof( *psd ) );

    psd->pgd = pewnd->pv;
    pewnd->pv = psd;

    psd->paewnd = NULL;
    psd->amMoves = NULL;
}

static void StatsCreate( extwindow *pewnd, statsdata *psd ) {

    psd->paewnd = ExtWndCreateWindows( pewnd, aewsStats, DIM( aewsStats ) );

    XMapSubwindows( pewnd->pdsp, pewnd->wnd );
}

static int StatsHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    StatsRedraw( pewnd, pewnd->pv, False );

	break;
	
    case ConfigureNotify:
	ExtHandler( pewnd, pxev );
	return StatsConfigure( pewnd, pewnd->pv, &pxev->xconfigure );

    case ExtPreCreateNotify:
	StatsPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	StatsCreate( pewnd, pewnd->pv );
	break;
    }

    return ExtHandler( pewnd, pxev );
}

extern int StatsConfirm( extwindow *pewnd ) {

    char sz[ 40 ];
    statsdata *psd = pewnd->pv;

    if( psd->pm && psd->pm->cMoves == psd->ml.cMaxMoves &&
	psd->pm->cPips == psd->ml.cMaxPips ) {
	FormatMove( sz, NULL, psd->pm->anMove );
    
	CommandMove( sz ); /* FIXME output from this command (if any) looks a
			      bit grotty, because no linefeed was typed after
			      the prompt */
    } else
	/* Illegal move */
	XBell( pewnd->pdsp, 100 );
    
    return 0;
}

static void StatsReadBoard( extwindow *pewnd, statsdata *psd,
			    int anBoard[ 2 ][ 25 ] ) {
    gamedata *pgd = psd->pgd;
    int i;
    
    for( i = 0; i < 24; i++ ) {
	anBoard[ pgd->fTurn <= 0 ][ i ] = pgd->anBoard[ 24 - i ] < 0 ?
	    abs( pgd->anBoard[ 24 - i ] ) : 0;
	anBoard[ pgd->fTurn > 0 ][ i ] = pgd->anBoard[ i + 1 ] > 0 ?
	    abs( pgd->anBoard[ i + 1 ] ) : 0;
    }

    anBoard[ pgd->fTurn <= 0 ][ 24 ] = abs( pgd->anBoard[ 0 ] );
    anBoard[ pgd->fTurn > 0 ][ 24 ] = abs( pgd->anBoard[ 25 ] );
}

static void StatsUpdateBoardID( extwindow *pewnd, statsdata *psd,
				int anBoard[ 2 ][ 25 ] ) {

    ExtChangePropertyHandler( psd->paewnd + STATS_BOARD, TP_TEXT, 8,
			      PositionID( anBoard ), 15 );
}

extern int StatsMove( extwindow *pewnd, int nSource, int nDest, int fHit ) {

    char sz[ 40 ], *pch = "Illegal move";
    statsdata *psd = pewnd->pv;
    int i, anBoard[ 2 ][ 25 ];
    unsigned char auch[ 10 ];
    
    StatsReadBoard( pewnd, psd, anBoard );
    StatsUpdateBoardID( pewnd, psd, anBoard );

    psd->pm = NULL;
    
    if( EqualBoards( anBoard, psd->pgd->anBoardOld ) )
	/* no move has been made */
	pch = "";
    else {
	PositionKey( anBoard, auch );

	for( i = 0; i < psd->ml.cMoves; i++ )
	    if( EqualKeys( psd->ml.amMoves[ i ].auch, auch ) ) {
		/* FIXME do something different if the move is complete */
		psd->pm = psd->ml.amMoves + i;
		FormatMove( pch = sz, psd->pgd->anBoardOld, psd->pm->anMove );
		break;
	    }
    }
    
    ExtChangePropertyHandler( psd->paewnd + STATS_MOVE, TP_TEXT, 8, pch,
			      strlen( pch ) + 1 );

    return 0;
}

extern int StatsSet( extwindow *pewnd, int anBoard[ 2 ][ 25 ], int nDice0,
		     int nDice1 ) {

    statsdata *psd = pewnd->pv;
    gamedata *pgd = psd->pgd;
    char szScore[ 32 ];
    
    if( psd->paewnd ) {
	ExtChangePropertyHandler( psd->paewnd +
				  ( pgd->fColour < 0 ? STATS_XNAME :
				    STATS_ONAME ), TP_TEXT, 8,
				  pgd->szName, strlen( pgd->szName ) );
	ExtChangePropertyHandler( psd->paewnd +
				  ( pgd->fColour > 0 ? STATS_XNAME :
				    STATS_ONAME ), TP_TEXT, 8,
				  pgd->szNameOpponent,
				  strlen( pgd->szNameOpponent ) );

	sprintf( szScore, "%d", pgd->nScore );
	ExtChangePropertyHandler( psd->paewnd +
				  ( pgd->fColour < 0 ? STATS_XSCORE :
				    STATS_OSCORE ), TP_TEXT, 8,
				  szScore, strlen( szScore ) );
	
	sprintf( szScore, "%d", pgd->nScoreOpponent );
	ExtChangePropertyHandler( psd->paewnd +
				  ( pgd->fColour > 0 ? STATS_XSCORE :
				    STATS_OSCORE ), TP_TEXT, 8,
				  szScore, strlen( szScore ) );

	if( pgd->nMatchTo == 0 )
	    strcpy( szScore, "unlimited" );
	else
	    sprintf( szScore, "%d", pgd->nMatchTo );

	ExtChangePropertyHandler( psd->paewnd + STATS_MATCH, TP_TEXT, 8,
				  szScore, strlen( szScore ) );
	
	ExtChangePropertyHandler( psd->paewnd + STATS_MOVE, TP_TEXT, 8, "",
				  0 );
	
	StatsUpdateBoardID( pewnd, psd, anBoard );

	if( pgd->anDice[ 0 ] || pgd->anDiceOpponent[ 0 ] ) {
	    GenerateMoves( &psd->ml, anBoard, nDice0, nDice1, TRUE );

	    /* psd->ml contains pointers to static data, so we need to
	       copy the actual moves into private storage. */
	    psd->amMoves = realloc( psd->amMoves,
				    psd->ml.cMoves * sizeof( move ) );

	    psd->ml.amMoves = memcpy( psd->amMoves, psd->ml.amMoves,
				      psd->ml.cMoves * sizeof( move ) );

	    psd->pm = NULL;
	}
    }
    
    return 0;
}

extdefault aedStats[] = {
    { &eq_background, "#000" },
    { NULL, NULL }
};

extwindowclass ewcStats = {
    ExposureMask | StructureNotifyMask, /* button? key? */
    1, 1, 1, 1, /* FIXME ??? */
    StatsHandler,
    "Stats",
    aedStats
};

typedef struct _dicedata {
    gamedata *pgd;
    int an[ 2 ];
} dicedata;

static int DiceRedraw( extwindow *pewnd, dicedata *pdd ) {

    gamedata *pgd = pdd->pgd;

    GameRedrawDice( pewnd, pgd, 0, 0, pgd->fTurn, pdd->an[ 0 ] );
    GameRedrawDice( pewnd, pgd, 8 * pgd->nBoardSize, 0, pgd->fTurn,
		    pdd->an[ 1 ] );
    
    return 0;
}

static int DiceUnmapNotify( extwindow *pewnd, dicedata *pdd ) {

    pdd->an[ 0 ] = ( RAND % 6 ) + 1;
    pdd->an[ 1 ] = ( RAND % 6 ) + 1;
    
    return 0;
}

static int DicePreCreate( extwindow *pewnd ) {

    dicedata *pdd = malloc( sizeof( *pdd ) );

    pdd->pgd = pewnd->pv;
    pewnd->pv = pdd;

    return DiceUnmapNotify( pewnd, pdd );
}

static int DiceHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    DiceRedraw( pewnd, pewnd->pv );

	break;

    case UnmapNotify:
	DiceUnmapNotify( pewnd, pewnd->pv );
	break;
	
    case ButtonPress:
	CommandRoll( NULL );
	break;
	
    case ExtPreCreateNotify:
	DicePreCreate( pewnd );
	break;
    }

    return ExtHandler( pewnd, pxev );
}

extdefault aedDice[] = {
    { &eq_background, "#000" }, /* FIXME */
    { NULL, NULL }
};

extwindowclass ewcDice = {
    ExposureMask | StructureNotifyMask | ButtonPressMask,
    1, 1, 1, 1, /* FIXME ??? */
    DiceHandler,
    "Dice",
    aedDice
};

typedef enum _gameid {
    GAME_BOARD, GAME_DICE, GAME_STATS
} gameid;

static int GamePositionBoard( extwindow *pewnd, int *px, int *py,
			      int *pcx, int *pcy ) {

    int nBoardSize;

    nBoardSize = ( pewnd->cx - 16 ) / 108;
    if( ( pewnd->cy - 12 /* FIXME */ ) / 102 < nBoardSize )
	nBoardSize = ( pewnd->cy - 12 /* FIXME */ ) / 102;

    if( !nBoardSize )
	return -1;
    
    *pcx = nBoardSize * 108;
    *pcy = nBoardSize * 72;
    *px = ( pewnd->cx - *pcx ) / 2;
    *py = nBoardSize * 6;
    
    return 0;
}

static int GameConfigure( extwindow *pewnd, gamedata *pgd,
			   XConfigureEvent *pxcev ) {

    int x, y, cx, cy;

    if( !GamePositionBoard( pewnd, &x, &y, &cx, &cy ) ) {
	XMoveResizeWindow( pewnd->pdsp, pgd->ewndBoard.wnd,
			   x, y, cx, cy );
	XMapWindow( pewnd->pdsp, pgd->ewndBoard.wnd );

	XMoveResizeWindow( pewnd->pdsp, pgd->ewndDice.wnd,
			   x + cx - cx / 108 * 15, y + cy / 72 * 73,
			   cx / 108 * 15, cy / 72 * 7 );
	
	XMoveResizeWindow( pewnd->pdsp, pgd->ewndStats.wnd,
			   y, y + cy / 72 * 81, pewnd->cx - y * 2,
			   cy / 6 + 54 );
    } else
	XUnmapWindow( pewnd->pdsp, pgd->ewndBoard.wnd );

    return 0;
}

static int GamePreCreate( extwindow *pewnd ) {

    gamedata *pgd = malloc( sizeof( *pgd ) );
    char *pchWindowID;
    
    pewnd->pv = pgd;
    
    pgd->fDirection = -1;

    pchWindowID = getenv( "WINDOWID" );
    pgd->wndKey = pchWindowID ? atoi( pchWindowID ) : None;
    
    ExtWndCreate( &pgd->ewndBoard, &pewnd->eres, "main",
		  &ewcBoard, pewnd->eres.rdb, NULL, pgd );
    ExtWndCreate( &pgd->ewndDice, &pewnd->eres, "dice",
		  &ewcDice, pewnd->eres.rdb, NULL, pgd );
    ExtWndCreate( &pgd->ewndStats, &pewnd->eres, "stats",
		  &ewcStats, pewnd->eres.rdb, NULL, pgd );
    
    return 0;
}

static int GameCreate( extwindow *pewnd, gamedata *pgd ) {

    char szGeometry[ 32 ];
    int x, y, cx, cy, f;

    if( ( f = !GamePositionBoard( pewnd, &x, &y, &cx, &cy ) ) )
	sprintf( szGeometry, "%dx%d+%d+%d", cx, cy, x, y );

    ExtWndRealise( &pgd->ewndBoard, pewnd->pdsp, pewnd->wnd, f ? szGeometry :
		   "108x72+8+8", pewnd->wnd, GAME_BOARD );
    
    sprintf( szGeometry, "%dx%d+%d+%d", 32, 14, 8, 160 ); /* FIXME */
    ExtWndRealise( &pgd->ewndDice, pewnd->pdsp, pewnd->wnd, szGeometry,
		   pewnd->wnd, GAME_DICE );

    ExtWndRealise( &pgd->ewndStats, pewnd->pdsp, pewnd->wnd, szGeometry,
		   pewnd->wnd, GAME_STATS );
    
    if( f ) {
	XMapWindow( pewnd->pdsp, pgd->ewndBoard.wnd );
	XMapWindow( pewnd->pdsp, pgd->ewndStats.wnd );
    }
    
    return 0;
}

static int GameHandler( extwindow *pewnd, XEvent *pxev ) {

    gamedata *pgd = pewnd->pv;
    
    switch( pxev->type ) {
    case ClientMessage:
#if 0
	/* Let xterm (or other client) know it should accept the focus
	   on our behalf.  NB: seems to crash xterm! */
	if( ( pxev->xclient.message_type ==
	      XInternAtom( pewnd->pdsp, "WM_PROTOCOLS", False ) ) &&
	    ( pxev->xclient.data.l[ 0 ] ==
	      XInternAtom( pewnd->pdsp, "WM_TAKE_FOCUS", False ) ) &&
	      pgd->wndKey ) {
	    pxev->xclient.window = pgd->wndKey;
	    XSendEvent( pewnd->pdsp, pgd->wndKey, False, 0, pxev );
	}
#endif
	break;

    case ConfigureNotify:
	ExtHandler( pewnd, pxev );
	return GameConfigure( pewnd, pgd, &pxev->xconfigure );

    case KeyPress:
    case KeyRelease:
	/* FIXME handle undo/double/roll keys somewhere */

	if( pgd->wndKey ) {
	    /* We're running under an xterm (or similar); forward keyboard
	       input we can't process ourselves onto it.  Note that xterm
	       by default will ignore `SendEvent's; other terminals
	       (e.g. gnome-terminal) may accept them.  Ignore errors
	       from XSendEvent (we could get `BadWindow's if the user
	       has a bad $WINDOWID lying around). */
	    ExtDspHandleNextError( ExtDspFind( pewnd->pdsp ), NULL );
	    pxev->xkey.window = pgd->wndKey;
	    pxev->xkey.subwindow = None;
	    XSendEvent( pewnd->pdsp, pgd->wndKey, False, KeyPressMask |
			KeyReleaseMask, pxev );
	}
	break;
	
    case ExtPreCreateNotify:
	GamePreCreate( pewnd );
	break;

    case ExtCreateNotify:
	GameCreate( pewnd, pgd );
	break;
    }

    return ExtHandler( pewnd, pxev );
}

extern int GameSet( extwindow *pewnd, int anBoard[ 2 ][ 25 ], int fRoll,
		    char *szPlayer, char *szOpp, int nMatchTo,
		    int nScore, int nOpponent, int nDice0, int nDice1 ) {

    char sz[ 256 ];
    int i, anOff[ 2 ];
    gamedata *pgd = pewnd->pv;

    memcpy( pgd->anBoardOld, anBoard, sizeof( pgd->anBoardOld ) );
    
    /* Names and match length/score */
    sprintf( sz, "board:%s:%s:%d:%d:%d:", szPlayer, szOpp, nMatchTo, nScore,
	     nOpponent );

    /* Opponent on bar */
    sprintf( strchr( sz, 0 ), "%d:", -anBoard[ 0 ][ 24 ] );

    /* Board */
    for( i = 0; i < 24; i++ )
	sprintf( strchr( sz, 0 ), "%d:", anBoard[ 0 ][ 23 - i ] ?
		 -anBoard[ 0 ][ 23 - i ] : anBoard[ 1 ][ i ] );

    /* Player on bar */
    sprintf( strchr( sz, 0 ), "%d:", anBoard[ 1 ][ 24 ] );

    /* Whose turn */
    strcat( strchr( sz, 0 ), fRoll ? "1:" : "-1:" );

    anOff[ 0 ] = anOff[ 1 ] = 15;
    for( i = 0; i < 25; i++ ) {
	anOff[ 0 ] -= anBoard[ 0 ][ i ];
	anOff[ 1 ] -= anBoard[ 1 ][ i ];
    }
    
    sprintf( strchr( sz, 0 ), "%d:%d:%d:%d:%d:%d:%d:%d:1:-1:0:25:%d:%d:0:0:0:"
	     "0:0:0", nDice0, nDice1, nDice0, nDice1, nCube, fCubeOwner != 0,
	     fCubeOwner != 1, fDoubled, anOff[ 1 ], anOff[ 0 ] );
    
    BoardSet( &pgd->ewndBoard, sz );
    StatsSet( &pgd->ewndStats, anBoard, nDice0, nDice1 );

    if( !pewnd->pdsp )
	return 0;
    
    if( pgd->anDice[ 0 ] )
	XUnmapWindow( pewnd->pdsp, pgd->ewndDice.wnd );
    else
	XMapWindow( pewnd->pdsp, pgd->ewndDice.wnd );
    
    return 0;
}

extern void GameRedrawDice( extwindow *pewnd, gamedata *pgd, int x, int y,
			     int fColour, int n ) {

    int fPip[ 9 ];
    int ix, iy;
    
    if( pewnd->wnd == None || !n )
	return;
    
    XCopyPlane( pewnd->pdsp, pgd->pmDiceMask, pewnd->wnd,
		pgd->gcAnd, 0, 0, 7 * pgd->nBoardSize,
		7 * pgd->nBoardSize, x, y, 1 );

    XCopyArea( pewnd->pdsp, fColour < 0 ? pgd->pmXDice :
	       pgd->pmODice, pewnd->wnd,
	       pgd->gcOr, 0, 0, 7 * pgd->nBoardSize,
	       7 * pgd->nBoardSize, x, y );

    fPip[ 0 ] = fPip[ 8 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
	( ( ( n == 2 ) || ( n == 3 ) ) && x & 1 );
    fPip[ 1 ] = fPip[ 7 ] = n == 6 && !( x & 1 );
    fPip[ 2 ] = fPip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
	( ( ( n == 2 ) || ( n == 3 ) ) && !( x & 1 ) );
    fPip[ 3 ] = fPip[ 5 ] = n == 6 && x & 1;
    fPip[ 4 ] = n & 1;

    for( iy = 0; iy < 3; iy++ )
	 for( ix = 0; ix < 3; ix++ )
	     if( fPip[ iy * 3 + ix ] )
		 XCopyArea( pewnd->pdsp, fColour < 0 ? pgd->pmXPip :
			    pgd->pmOPip, pewnd->wnd, pgd->gcCopy,
			    0, 0, pgd->nBoardSize, pgd->nBoardSize,
			    x + ( 1 + 2 * ix ) * pgd->nBoardSize,
			    y + ( 1 + 2 * iy ) * pgd->nBoardSize );
}

extdefault aedGame[] = {
    { &eq_background, "#000" },
    { NULL, NULL }
};

extwindowclass ewcGame = {
    StructureNotifyMask | KeyPressMask | KeyReleaseMask,
    1, 1, 124, 96,
    GameHandler,
    "Game",
    aedGame
};
