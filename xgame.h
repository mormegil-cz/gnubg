/*
 * xgame.h
 *
 * by Gary Wong, 1997-1999
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

#ifndef _GAME_H_
#define _GAME_H_

#if USE_EXT
#include <ext.h>
#endif

#include "eval.h"

typedef struct _gamedata {
    extwindow ewndBoard, ewndDice, ewndStats, ewndGame;
    GC gcAnd, gcOr, gcCopy, gcCube;
    Pixmap pmBoard, pmX, pmO, pmMask, pmXDice, pmODice, pmDiceMask, pmXPip,
	pmOPip, pmCube, pmCubeMask, pmSaved, pmTemp, pmTempSaved, pmPoint;
    Window wndKey;
    int nBoardSize; /* basic unit of board size, in pixels -- a chequer's
		       diameter is 6 of these units (and is 2 units thick) */
    XStandardColormap *pxscm;
    XFontStruct *pxfsCube;
    
    int nDragPoint, fDragColour, xDrag, yDrag, xDice[ 2 ], yDice[ 2 ],
	fDiceColour[ 2 ], fCubeFontRotated;
    int anBoardOld[ 2 ][ 25 ]; /* board before user made any moves */
    int fCubeOwner; /* -1 = bottom, 0 = centred, 1 = top */ 
    
    /* remainder is from FIBS board: data */
    char szName[ MAX_NAME_LEN ], szNameOpponent[ MAX_NAME_LEN ];
    int nMatchTo, nScore, nScoreOpponent;
    int anBoard[ 28 ]; /* 0 and 25 are the bars */
    int fTurn; /* -1 is X, 1 is O, 0 if game over */
    int anDice[ 2 ], anDiceOpponent[ 2 ]; /* 0, 0 if not rolled */
    int nCube;
    int fDouble, fDoubleOpponent; /* allowed to double */
    int fDoubled; /* opponent just doubled */
    int fColour; /* -1 for player X, 1 for player O */
    int fDirection; /* -1 playing from 24 to 1, 1 playing from 1 to 24 */
    int nHome, nBar; /* 0 or 25 depending on fDirection */
    int nOff, nOffOpponent; /* number of men borne off */
    int nOnBar, nOnBarOpponent; /* number of men on bar */
    int nToMove; /* 0 to 4 -- number of pieces to move */
    int nForced, nCrawford; /* unused */
    int nRedoubles; /* number of instant redoubles allowed */
} gamedata;

/* Flag set to indicate re-entrant calls (i.e. processing a new X event
   before an old one has completed).  It would be nicer if this was part of
   gamedata, but never mind... */
extern int fBusy;

extern extwindowclass ewcGame;

/* FIXME these shouldn't be part of the interface... Board windows should
   send client messages to their owners instead */
extern int StatsConfirm( extwindow *pewnd );
extern int StatsMove( extwindow *pewnd );

extern void GameRedrawDice( extwindow *pewnd, gamedata *pgd, int x, int y,
			     int fColour, int i );
extern int GameSet( extwindow *pewnd, int anBoard[ 2 ][ 25 ], int fRoll,
		    char *szPlayer, char *szOpp, int nMatchTo,
		    int nScore, int nOpponent, int nDice0, int nDice1 );
extern void RunExt( void );
#endif
