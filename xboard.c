/*
 * xboard.c
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

#include "config.h"

#include <ext.h>
#include <extwin.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>

#include "backgammon.h"
#include "xboard.h"
#include "xgame.h"
#include "boarddim.h"

static extquark eq_cubeFont = { "cubeFont", 0 };

#define POINT_DICE 28
#define POINT_CUBE 29

static int aanPosition[ 28 ][ 3 ] = {
    { 51, 25, 7 },
    { 90, 63, 6 }, { 84, 63, 6 }, { 78, 63, 6 }, { 72, 63, 6 }, { 66, 63, 6 },
    { 60, 63, 6 }, { 42, 63, 6 }, { 36, 63, 6 }, { 30, 63, 6 }, { 24, 63, 6 },
    { 18, 63, 6 }, { 12, 63, 6 }, { 12, 3, -6 }, { 18, 3, -6 }, { 24, 3, -6 },
    { 30, 3, -6 }, { 36, 3, -6 }, { 42, 3, -6 }, { 60, 3, -6 }, { 66, 3, -6 },
    { 72, 3, -6 }, { 78, 3, -6 }, { 84, 3, -6 }, { 90, 3, -6 },
    { 51, 41, -7 }, { 99, 63, 6 }, { 99, 3, -6 }
};

static unsigned int nSeed = 1; /* for rand_r */
#define RAND ( ( (unsigned int) rand_r( &nSeed ) ) & RAND_MAX )

static int Intersects( int x0, int y0, int cx0, int cy0,
		       int x1, int y1, int cx1, int cy1 ) {

    return ( y1 + cy1 > y0 ) && ( y1 < y0 + cy0 ) &&
	( x1 + cx1 > x0 ) && ( x1 < x0 + cx0 );
}

static void BoardRedrawDice( extwindow *pewnd, gamedata *pgd, int i ) {

    GameRedrawDice( pewnd, pgd, pgd->xDice[ i ] * pgd->nBoardSize,
		    pgd->yDice[ i ] * pgd->nBoardSize,
		    pgd->fDiceColour[ i ], pgd->fTurn == pgd->fColour ?
		    pgd->anDice[ i ] : pgd->anDiceOpponent[ i ] );
}

static void BoardRedrawCube( extwindow *pewnd, gamedata *pgd ) {

    int n, x, y, fTwo;
    XCharStruct xcs, xcsFirst;
    char szCube[ 3 ];

    if( pewnd->wnd == None )
	return;
    
    x = 50 * pgd->nBoardSize;
    y = ( 32 - 29 * pgd->fCubeOwner ) * pgd->nBoardSize;
    
    XCopyPlane( pewnd->pdsp, pgd->pmCubeMask, pewnd->wnd,
		pgd->gcAnd, 0, 0, 8 * pgd->nBoardSize,
		8 * pgd->nBoardSize, x, y, 1 );

    XCopyArea( pewnd->pdsp, pgd->pmCube, pewnd->wnd,
	       pgd->gcOr, 0, 0, 8 * pgd->nBoardSize,
	       8 * pgd->nBoardSize, x, y );

    sprintf( szCube, "%d", pgd->nCube > 1 && pgd->nCube < 65 ?
	     pgd->nCube : 64 );

    fTwo = pgd->nCube == 1 || pgd->nCube > 10;
    
    if( pgd->fCubeFontRotated ) {
	XTextExtents( pgd->pxfsCube, szCube, 1, &n, &n, &n, &xcsFirst );
	
	if( fTwo )
	    XTextExtents( pgd->pxfsCube, szCube + 1, 1, &n, &n, &n, &xcs );
	else
	    xcs.lbearing = xcs.descent = xcs.ascent = 0;

	if( pgd->fCubeOwner ) {
	    /* upside down */
	    xcs.lbearing += xcsFirst.lbearing;
	    XDrawString( pewnd->pdsp, pewnd->wnd, pgd->gcCube,
			 x + 4 * pgd->nBoardSize - xcs.lbearing / 2,
			 y + 4 * pgd->nBoardSize - xcsFirst.descent / 2,
			 szCube, 1 );
	    if( fTwo )
		XDrawString( pewnd->pdsp, pewnd->wnd, pgd->gcCube,
			     x + 4 * pgd->nBoardSize - xcs.lbearing / 2 +
			     xcsFirst.lbearing,
			     y + 4 * pgd->nBoardSize - xcs.descent / 2,
			     szCube + 1, 1 );
	} else {
	    /* centred */
	    xcs.ascent += xcsFirst.ascent;
	    XDrawString( pewnd->pdsp, pewnd->wnd, pgd->gcCube,
			 x + 4 * pgd->nBoardSize - xcsFirst.lbearing / 2,
			 y + 4 * pgd->nBoardSize + xcs.ascent / 2, szCube,
			 1 );
	    if( fTwo )
		XDrawString( pewnd->pdsp, pewnd->wnd, pgd->gcCube,
			     x + 4 * pgd->nBoardSize - xcs.lbearing / 2,
			     y + 4 * pgd->nBoardSize +
			     ( xcs.descent - xcsFirst.descent ) / 2,
			     szCube + 1, 1 );
	}
    } else {
	XTextExtents( pgd->pxfsCube, szCube, strlen( szCube ), &n, &n, &n,
		      &xcs );
	
	XDrawString( pewnd->pdsp, pewnd->wnd, pgd->gcCube,
		     x + 4 * pgd->nBoardSize - xcs.width / 2,
		     y + 4 * pgd->nBoardSize + xcs.ascent / 2, szCube,
		     strlen( szCube ) );
    }
}

static void BoardRedrawPoint( extwindow *pewnd, gamedata *pgd, int n ) {

    int i, x, y, cx, cy, fInvert, yChequer, cChequer, iChequer;
    
    if( pewnd->wnd == None )
	return;

    x = aanPosition[ n ][ 0 ] * pgd->nBoardSize;
    y = aanPosition[ n ][ 1 ] * pgd->nBoardSize;
    cx = 6 * pgd->nBoardSize;
    cy = -5 * pgd->nBoardSize * aanPosition[ n ][ 2 ];

    if( ( fInvert = cy < 0 ) ) {
	y += cy * 4 / 5;
	cy = -cy;
    }

    /* FIXME draw straight to screen and return if point is empty */
    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmPoint, pgd->gcCopy, x, y,
	       cx, cy, 0, 0 );

    yChequer = fInvert ? cy - 6 * pgd->nBoardSize : 0;
    
    cChequer = 5; /* FIXME 3 if on bar */
    iChequer = 0;
    
    for( i = abs( pgd->anBoard[ n ] ); i; i-- ) {
	XCopyPlane( pewnd->pdsp, pgd->pmMask, pgd->pmPoint, pgd->gcAnd, 0, 0,
		    6 * pgd->nBoardSize, 6 * pgd->nBoardSize, 0, yChequer, 1 );
	
	XCopyArea( pewnd->pdsp, pgd->anBoard[ n ] > 0 ? pgd->pmO : pgd->pmX,
		   pgd->pmPoint, pgd->gcOr, 0, 0, 6 * pgd->nBoardSize,
		   6 * pgd->nBoardSize, 0, yChequer );

	yChequer -= pgd->nBoardSize * aanPosition[ n ][ 2 ];

	if( ++iChequer == cChequer ) {
	    iChequer = 0;
	    cChequer--;

	    yChequer = fInvert ? cy + ( 3 * cChequer - 21 ) * pgd->nBoardSize :
		( 15 - 3 * cChequer ) * pgd->nBoardSize;
	}
    }

    XCopyArea( pewnd->pdsp, pgd->pmPoint, pewnd->wnd, pgd->gcCopy, 0, 0,
	       cx, cy, x, y );
}

static int BoardRedraw( extwindow *pewnd, gamedata *pgd,
			XExposeEvent *pxeev ) {

    int i;
    
    if( pewnd->wnd == None )
	return 0;

    XCopyArea( pewnd->pdsp, pgd->pmBoard, pewnd->wnd, pgd->gcCopy,
	       pxeev->x, pxeev->y, pxeev->width, pxeev->height,
	       pxeev->x, pxeev->y );

    for( i = 0; i < 28; i++ ) {
	int y, cy;

	y = aanPosition[ i ][ 1 ] * pgd->nBoardSize;
	cy = -5 * pgd->nBoardSize * aanPosition[ i ][ 2 ];

	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}
	
	if( Intersects( pxeev->x, pxeev->y, pxeev->width, pxeev->height,
			aanPosition[ i ][ 0 ] * pgd->nBoardSize,
			y, 6 * pgd->nBoardSize, cy ) )
	    BoardRedrawPoint( pewnd, pgd, i );
    }

    if( Intersects( pxeev->x, pxeev->y, pxeev->width, pxeev->height,
		    pgd->xDice[ 0 ] * pgd->nBoardSize,
		    pgd->yDice[ 0 ] * pgd->nBoardSize,
		    7 * pgd->nBoardSize, 7 * pgd->nBoardSize ) )
	BoardRedrawDice( pewnd, pgd, 0 );

    if( Intersects( pxeev->x, pxeev->y, pxeev->width, pxeev->height,
		    pgd->xDice[ 1 ] * pgd->nBoardSize,
		    pgd->yDice[ 1 ] * pgd->nBoardSize,
		    7 * pgd->nBoardSize, 7 * pgd->nBoardSize ) )
	BoardRedrawDice( pewnd, pgd, 1 );

    if( Intersects( pxeev->x, pxeev->y, pxeev->width, pxeev->height,
		    50 * pgd->nBoardSize, ( 32 - 29 * pgd->fCubeOwner ) *
		    pgd->nBoardSize, 8 * pgd->nBoardSize,
		    8 * pgd->nBoardSize ) )
	BoardRedrawCube( pewnd, pgd );
    
    return 0;
}

static void BoardExposePoint( extwindow *pewnd, gamedata *pgd, int n ) {
    
    XExposeEvent xeev;

    if( pewnd->wnd == None )
        return;

    xeev.count = 0;
    xeev.x = aanPosition[ n ][ 0 ] * pgd->nBoardSize;
    xeev.y = aanPosition[ n ][ 1 ] * pgd->nBoardSize;
    xeev.width = 6 * pgd->nBoardSize;
    xeev.height = -5 * pgd->nBoardSize *
        aanPosition[ n ][ 2 ];
    if( xeev.height < 0 ) {
        xeev.y += xeev.height * 4 / 5;
        xeev.height = -xeev.height;
    }
    
    BoardRedraw( pewnd, pgd, &xeev );
}

static int BoardPoint( extwindow *pewnd, gamedata *pgd, int x0, int y0 ) {

    int i, y, cy;

    x0 /= pgd->nBoardSize;
    y0 /= pgd->nBoardSize;

    if( Intersects( x0, y0, 0, 0, pgd->xDice[ 0 ], pgd->yDice[ 0 ], 7, 7 ) ||
	Intersects( x0, y0, 0, 0, pgd->xDice[ 1 ], pgd->yDice[ 1 ], 7, 7 ) )
	return POINT_DICE;

    if( Intersects( x0, y0, 0, 0, 50, 30 - 29 * pgd->fCubeOwner, 8, 8 ) )
	return POINT_CUBE;
    
    for( i = 0; i < 28; i++ ) {
	y = aanPosition[ i ][ 1 ];
	cy = -5 * aanPosition[ i ][ 2 ];
	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}

	if( Intersects( x0, y0, 0, 0, aanPosition[ i ][ 0 ],
			y, 6, cy ) )
	    return i;
    }

    return -1;
}

static void BoardPointer( extwindow *pewnd, gamedata *pgd, XEvent *pxev ) {

    Pixmap pmSwap;
    int n, nDest, xEvent, yEvent, nBar;

    if( fBusy ) {
	if( pxev->type == ButtonPress )
	    XBell( pewnd->pdsp, 100 );

	return;
    }
    
    switch( pxev->type ) {
    case ButtonPress:
    case ButtonRelease:
	xEvent = pxev->xbutton.x;
	yEvent = pxev->xbutton.y;
	break;

    case MotionNotify:
	if( pxev->xmotion.is_hint ) {
	    Window wIgnore;
	    int nIgnore;
	    
	    if( !XQueryPointer( pewnd->pdsp, pewnd->wnd, &wIgnore, &wIgnore,
				&nIgnore, &nIgnore, &xEvent, &yEvent,
				(unsigned int *) &nIgnore ) )
		return;
	} else {
	    xEvent = pxev->xmotion.x;
	    yEvent = pxev->xmotion.y;
	}
	break;

    default:
	abort();
    }
    
    switch( pxev->type ) {
    case ButtonPress:
	pgd->nDragPoint = BoardPoint( pewnd, pgd, xEvent, yEvent );

	/* FIXME if the dice are set, and nDragPoint is between 1 and 24 and
	   contains no chequers of the player on roll, then scan through all
	   the legal moves looking for the one which makes that point with
	   the smallest pip count, make it and return. */

	/* FIXME if the dice are set, and nDragPoint is 26 or 27 (i.e. off),
	   then bear off as many chequers as possible, and return. */
	
	if( ( pgd->nDragPoint = BoardPoint( pewnd, pgd, xEvent, yEvent ) ) < 0
	    || ( pgd->nDragPoint < 28 && !pgd->anBoard[ pgd->nDragPoint ] ) ) {
	    /* Click on empty point, or not on a point at all */
	    XBell( pewnd->pdsp, 100 );

	    pgd->nDragPoint = -1;
	    
	    return;
	}

	if( pgd->nDragPoint == POINT_CUBE ) {
	    /* Clicked on cube; double. */
	    pgd->nDragPoint = -1;
	    UserCommand( "double" );
	    return;
	}
	
	if( pgd->nDragPoint == POINT_DICE ) {
	    /* Clicked on dice. */
	    pgd->nDragPoint = -1;
	    
	    if( pxev->xbutton.button == Button1 )
		/* Button 1 on dice confirms move. */
		StatsConfirm( &pgd->ewndStats );
	    else {
		/* Other buttons on dice swaps positions. */
		n = pgd->anDice[ 0 ];
		pgd->anDice[ 0 ] = pgd->anDice[ 1 ];
		pgd->anDice[ 1 ] = n;

		n = pgd->anDiceOpponent[ 0 ];
		pgd->anDiceOpponent[ 0 ] = pgd->anDiceOpponent[ 1 ];
		pgd->anDiceOpponent[ 1 ] = n;

		BoardRedrawDice( pewnd, pgd, 0 );
		BoardRedrawDice( pewnd, pgd, 1 );
	    }
	    
	    return;
	}
	
	pgd->fDragColour = pgd->anBoard[ pgd->nDragPoint ] < 0 ? -1 : 1;
	
	pgd->anBoard[ pgd->nDragPoint ] -= pgd->fDragColour;

	BoardExposePoint( pewnd, pgd, pgd->nDragPoint );

	if( pxev->xbutton.button != Button1 ) {
	    /* Automatically place chequer on destination point
	       (as opposed to starting a drag). */
	    nDest = pgd->nDragPoint - ( pxev->xbutton.button == Button2 ?
					pgd->anDice[ 0 ] :
					pgd->anDice[ 1 ] ) * pgd->fDragColour;

	    if( ( nDest <= 0 ) || ( nDest >= 25 ) )
		/* bearing off */
		nDest = pgd->fDragColour > 0 ? 26 : 27;
	    
	    goto PlaceChequer;
	}
	
	pgd->xDrag = xEvent;
	pgd->yDrag = yEvent;

	XCopyArea( pewnd->pdsp, pewnd->wnd, pgd->pmSaved, pgd->gcCopy,
		   pgd->xDrag - 3 * pgd->nBoardSize,
		   pgd->yDrag - 3 * pgd->nBoardSize,
		   6 * pgd->nBoardSize, 6 * pgd->nBoardSize, 0, 0 );
	
	/* fall through */
	
    case MotionNotify:
	if( pgd->nDragPoint < 0 )
	    break;
	
	XCopyArea( pewnd->pdsp, pewnd->wnd, pgd->pmTempSaved, pgd->gcCopy,
		   xEvent - 3 * pgd->nBoardSize,
		   yEvent - 3 * pgd->nBoardSize,
		   6 * pgd->nBoardSize, 6 * pgd->nBoardSize, 0, 0 );
		   
	XCopyArea( pewnd->pdsp, pgd->pmSaved, pgd->pmTempSaved, pgd->gcCopy,
		   0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
		   pgd->xDrag - xEvent, pgd->yDrag - yEvent );
		   
	XCopyArea( pewnd->pdsp, pgd->pmTempSaved, pgd->pmTemp, pgd->gcCopy,
		   0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize, 0, 0 );
	
	XCopyPlane( pewnd->pdsp, pgd->pmMask, pgd->pmTemp,
		    pgd->gcAnd, 0, 0, 6 * pgd->nBoardSize,
		    6 * pgd->nBoardSize, 0, 0, 1 );

	XCopyArea( pewnd->pdsp, pgd->fDragColour > 0 ? pgd->pmO :
		   pgd->pmX, pgd->pmTemp, pgd->gcOr, 0, 0,
		   6 * pgd->nBoardSize, 6 * pgd->nBoardSize, 0, 0 );
	
	XCopyArea( pewnd->pdsp, pgd->pmSaved, pewnd->wnd, pgd->gcCopy,
		   0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
		   pgd->xDrag - 3 * pgd->nBoardSize,
		   pgd->yDrag - 3 * pgd->nBoardSize );
	
	pgd->xDrag = xEvent;
	pgd->yDrag = yEvent;

	XCopyArea( pewnd->pdsp, pgd->pmTemp, pewnd->wnd, pgd->gcCopy,
		   0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
		   pgd->xDrag - 3 * pgd->nBoardSize,
		   pgd->yDrag - 3 * pgd->nBoardSize );

	pmSwap = pgd->pmSaved;
	pgd->pmSaved = pgd->pmTempSaved;
	pgd->pmTempSaved = pmSwap;
	
	break;
	
    case ButtonRelease:
	if( pgd->nDragPoint < 0 )
	    break;
	
	XCopyArea( pewnd->pdsp, pgd->pmSaved, pewnd->wnd, pgd->gcCopy,
		   0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
		   pgd->xDrag - 3 * pgd->nBoardSize,
		   pgd->yDrag - 3 * pgd->nBoardSize );

	nDest = BoardPoint( pewnd, pgd, xEvent, yEvent );
    PlaceChequer:
	nBar = pgd->fDragColour == pgd->fColour ? 25 - pgd->nBar : pgd->nBar;
	    
	if( nDest == -1 || ( pgd->fDragColour > 0 ? pgd->anBoard[ nDest ] < -1
			     : pgd->anBoard[ nDest ] > 1 ) || nDest == nBar ||
	    nDest > 27 ) {
	    /* FIXME check with owner that move is legal */
	    XBell( pewnd->pdsp, 100 );
	    
	    nDest = pgd->nDragPoint;
	}

	if( pgd->anBoard[ nDest ] == -pgd->fDragColour ) {
	    pgd->anBoard[ nDest ] = 0;
	    pgd->anBoard[ nBar ] -= pgd->fDragColour;

	    BoardExposePoint( pewnd, pgd, nBar );
	}
	
	pgd->anBoard[ nDest ] += pgd->fDragColour;

	BoardExposePoint( pewnd, pgd, nDest );

	if( pgd->nDragPoint != nDest )
	    StatsMove( &pgd->ewndStats );
	
	pgd->nDragPoint = -1;

	break;
    }
}

static void BoardSetCubeFont( extwindow *pewnd, gamedata *pgd ) {

    char szFont[ 256 ];
    int i, fFound;
    int anSizes[ 20 ] = { 34, 33, 26, 25, 24, 20, 19, 18, 17, 16, 15, 14, 13,
			  12, 11, 10, 9, 8, 7, 6 };
    
    /* FIXME query resource for font name and method */

    pgd->fCubeFontRotated = FALSE;
    
    switch( 1 ) {
    case 1:
	if( pgd->fCubeOwner != -1 ) {
	    /* FIXME get right matrix for cube pos'n */
	    sprintf( szFont, pgd->fCubeOwner ?
		     "-adobe-utopia-bold-r-normal--[~%d 0 0 ~%d]-"
		     "*-*-*-p-*-iso8859-1[49_52 54 56]" :
		     "-adobe-utopia-bold-r-normal--[0 %d ~%d 0]-"
		     "*-*-*-p-*-iso8859-1[49_52 54 56]", pgd->nBoardSize * 5,
		     pgd->nBoardSize * 5 );
	
	    if( ( pgd->pxfsCube = ExtWndAttachFont( pewnd, &eq_cubeFont,
						    &eq_Font,
						    szFont ) ) ) {
		pgd->fCubeFontRotated = TRUE;
		break;
	    }
	}
	
	/* fall through */
	
    case 2:
	sprintf( szFont, "-adobe-utopia-bold-r-normal--%d-*-*-*-p-*-"
		 "iso8859-1", pgd->nBoardSize * 5 );
	if( ( pgd->pxfsCube = ExtWndAttachFont( pewnd, &eq_cubeFont, &eq_Font,
						szFont ) ) )
	    break;

	/* fall through */
	
    case 3:
	fFound = FALSE;
	    
	for( i = 0; i < 20 && anSizes[ i ] > pgd->nBoardSize * 5; i++ )
	    ;

	for( ; i < 20; i++ ) {
	    sprintf( szFont, "-adobe-utopia-bold-r-normal--%d-*-*-*-p-*-"
		     "iso8859-1", anSizes[ i ] );

	    if( ( pgd->pxfsCube = ExtWndAttachFont( pewnd, &eq_cubeFont,
						    &eq_Font, szFont ) ) ) {
		fFound = TRUE;
		break;
	    }
	}

	if( fFound )
	    break;
	
	/* fall through */

    default:
	pgd->pxfsCube = ExtWndAttachFont( pewnd, &eq_cubeFont, &eq_Font,
					  "fixed" );
    }
    
    XSetFont( pewnd->pdsp, pgd->gcCube, pgd->pxfsCube->fid );
}

extern int BoardSet( extwindow *pewnd, char *pch ) {

    gamedata *pgd = pewnd->pv;
    char *pchDest;
    int i, *pn, **ppn;
    int anBoardOld[ 28 ];
    int fDirectionOld;
    XExposeEvent xeev;
#if __GNUC__
    int *apnMatch[] = { &pgd->nMatchTo, &pgd->nScore, &pgd->nScoreOpponent };
    int *apnGame[] = { &pgd->fTurn, pgd->anDice, pgd->anDice + 1,
		       pgd->anDiceOpponent, pgd->anDiceOpponent + 1,
		       &pgd->nCube, &pgd->fDouble, &pgd->fDoubleOpponent,
		       &pgd->fDoubled, &pgd->fColour, &pgd->fDirection,
		       &pgd->nHome, &pgd->nBar, &pgd->nOff, &pgd->nOffOpponent,
		       &pgd->nOnBar, &pgd->nOnBarOpponent, &pgd->nToMove,
		       &pgd->nForced, &pgd->nCrawford, &pgd->nRedoubles };
    int anDiceOld[] = { pgd->anDice[ 0 ], pgd->anDice[ 1 ],
			pgd->anDiceOpponent[ 0 ], pgd->anDiceOpponent[ 1 ] };
#else
    int *apnMatch[ 3 ], *apnGame[ 21 ], anDiceOld[ 4 ];

    apnMatch[ 0 ] = &pgd->nMatchTo;
    apnMatch[ 1 ] = &pgd->nScore;
    apnMatch[ 2 ] = &pgd->nScoreOpponent;

    apnGame[ 0 ] = &pgd->fTurn;
    apnGame[ 1 ] = pgd->anDice;
    apnGame[ 2 ] = pgd->anDice + 1;
    apnGame[ 3 ] = pgd->anDiceOpponent;
    apnGame[ 4 ] = pgd->anDiceOpponent + 1;
    apnGame[ 5 ] = &pgd->nCube;
    apnGame[ 6 ] = &pgd->fDouble;
    apnGame[ 7 ] = &pgd->fDoubleOpponent;
    apnGame[ 8 ] = &pgd->fDoubled;
    apnGame[ 9 ] = &pgd->fColour;
    apnGame[ 10 ] = &pgd->fDirection;
    apnGame[ 11 ] = &pgd->nHome;
    apnGame[ 12 ] = &pgd->nBar;
    apnGame[ 13 ] = &pgd->nOff;
    apnGame[ 14 ] = &pgd->nOffOpponent;
    apnGame[ 15 ] = &pgd->nOnBar;
    apnGame[ 16 ] = &pgd->nOnBarOpponent;
    apnGame[ 17 ] = &pgd->nToMove;
    apnGame[ 18 ] = &pgd->nForced;
    apnGame[ 19 ] = &pgd->nCrawford;
    apnGame[ 20 ] = &pgd->nRedoubles;

    anDiceOld[ 0 ] = pgd->anDice[ 0 ];
    anDiceOld[ 1 ] = pgd->anDice[ 1 ];
    anDiceOld[ 2 ] = pgd->anDiceOpponent[ 0 ];
    anDiceOld[ 3 ] = pgd->anDiceOpponent[ 1 ];
#endif
    
    if( strncmp( pch, "board:", 6 ) )
	return -1;
    
    pch += 6;

    for( pchDest = pgd->szName, i = 31; i && *pch && *pch != ':'; i-- )
	*pchDest++ = *pch++;

    *pchDest = 0;

    if( !pch )
	return -1;

    pch++;
    
    for( pchDest = pgd->szNameOpponent, i = 31; i && *pch && *pch != ':'; i-- )
	*pchDest++ = *pch++;

    *pchDest = 0;

    if( !pch )
	return -1;

    for( i = 3, ppn = apnMatch; i--; ) {
	if( *pch++ != ':' ) /* FIXME should really check end of string */
	    return -1;

	**ppn++ = strtol( pch, &pch, 10 );
    }

    for( i = 0, pn = pgd->anBoard; i < 26; i++ ) {
	anBoardOld[ i ] = *pn;
	if( *pch++ != ':' )
	    return -1;

	*pn++ = strtol( pch, &pch, 10 );
    }

    anBoardOld[ 26 ] = pgd->anBoard[ 26 ];
    anBoardOld[ 27 ] = pgd->anBoard[ 27 ];

    fDirectionOld = pgd->fDirection;
    
    for( i = 21, ppn = apnGame; i--; ) {
	if( *pch++ != ':' )
	    return -1;

	**ppn++ = strtol( pch, &pch, 10 );
    }

    if( pgd->fColour < 0 )
	pgd->nOff = -pgd->nOff;
    else
	pgd->nOffOpponent = -pgd->nOffOpponent;
    
    if( pgd->fDirection < 0 ) {
	pgd->anBoard[ 26 ] = pgd->nOff;
	pgd->anBoard[ 27 ] = pgd->nOffOpponent;
    } else {
	pgd->anBoard[ 26 ] = pgd->nOffOpponent;
	pgd->anBoard[ 27 ] = pgd->nOff;
    }

    if( pewnd->wnd == None )
	return 0;

    if( pgd->anDice[ 0 ] != anDiceOld[ 0 ] ||
	pgd->anDice[ 1 ] != anDiceOld[ 1 ] ||
	pgd->anDiceOpponent[ 0 ] != anDiceOld[ 2 ] ||
	pgd->anDiceOpponent[ 1 ] != anDiceOld[ 3 ] ) {
	if( pgd->xDice[ 0 ] > 0 ) {
	    /* dice were visible before; now they're not */
	    xeev.count = 0;
	    xeev.x = pgd->xDice[ 0 ] * pgd->nBoardSize;
	    xeev.y = pgd->yDice[ 0 ] * pgd->nBoardSize;
	    xeev.width = xeev.height = 7 * pgd->nBoardSize;

	    pgd->xDice[ 0 ] = -10 * pgd->nBoardSize;
	    
	    BoardRedraw( pewnd, pgd, &xeev );
	    
	    xeev.x = pgd->xDice[ 1 ] * pgd->nBoardSize;
	    xeev.y = pgd->yDice[ 1 ] * pgd->nBoardSize;

	    pgd->xDice[ 1 ] = -10 * pgd->nBoardSize;

	    BoardRedraw( pewnd, pgd, &xeev );
	}

	if( ( pgd->fTurn == pgd->fColour ? pgd->anDice[ 0 ] :
	       pgd->anDiceOpponent[ 0 ] ) <= 0 )
	    /* dice have not been rolled */
	    pgd->xDice[ 0 ] = pgd->xDice[ 1 ] = -10 * pgd->nBoardSize;
	else {
	    /* FIXME different dice for first turn */
	    /* FIXME avoid cocked dice if possible */
	    pgd->xDice[ 0 ] = RAND % 21 + 13;
	    pgd->xDice[ 1 ] = RAND % ( 34 - pgd->xDice[ 0 ] ) +
		pgd->xDice[ 0 ] + 8;
	    
	    if( pgd->fColour == pgd->fTurn ) {
		pgd->xDice[ 0 ] += 48;
		pgd->xDice[ 1 ] += 48;
	    }
	    
	    pgd->yDice[ 0 ] = RAND % 10 + 28;
	    pgd->yDice[ 1 ] = RAND % 10 + 28;
	    pgd->fDiceColour[ 0 ] = pgd->fDiceColour[ 1 ] = pgd->fTurn;
	}
    }
    
    if( pgd->fCubeOwner != pgd->fDoubleOpponent - pgd->fDouble ) {
	xeev.count = 0;
	xeev.x = 50 * pgd->nBoardSize;
	xeev.y = ( 32 - 29 * pgd->fCubeOwner ) * pgd->nBoardSize;
	xeev.width = xeev.height = 8 * pgd->nBoardSize;

	ExtWndDetachFont( pewnd, pgd->pxfsCube->fid );
	pgd->fCubeOwner = pgd->fDoubleOpponent - pgd->fDouble;
	BoardSetCubeFont( pewnd, pgd );

	BoardRedraw( pewnd, pgd, &xeev );	
    }
    
    if( pgd->fDirection != fDirectionOld ) {
	for( i = 0; i < 28; i++ ) {
	    aanPosition[ i ][ 0 ] = 102 - aanPosition[ i ][ 0 ];
	    aanPosition[ i ][ 1 ] = 66 - aanPosition[ i ][ 1 ];
	    aanPosition[ i ][ 2 ] = -aanPosition[ i ][ 2 ];
	}
	
	xeev.count = xeev.x = xeev.y = 0;
	xeev.width = pewnd->cx;
	xeev.height = pewnd->cy;
	
	BoardRedraw( pewnd, pgd, &xeev );
    } else {
	for( i = 0; i < 28; i++ )
	    if( pgd->anBoard[ i ] != anBoardOld[ i ] )
		BoardRedrawPoint( pewnd, pgd, i );
	
	/* FIXME only redraw changed points/dice/cube */
	BoardRedrawDice( pewnd, pgd, 0 );
	BoardRedrawDice( pewnd, pgd, 1 );
	BoardRedrawCube( pewnd, pgd );
    }
    
    return 0;
}

static unsigned long MatchColour( XStandardColormap *pxscm,
				  int *an ) {
    int i, nR, nG, nB;

    for( i = 0; i < 3; i++ )
	if( an[ i ] < 0 )
	    an[ i ] = 0;
	else if( an[ i ] > 0x100 )
	    an[ i ] = 0x100;
    
    nR = ( 0x80 + pxscm->red_max * an[ 0 ] ) >> 8;
    nG = ( 0x80 + pxscm->green_max * an[ 1 ] ) >> 8;
    nB = ( 0x80 + pxscm->blue_max * an[ 2 ] ) >> 8;

    if( pxscm->red_max )
	an[ 0 ] -= ( nR << 8 ) / pxscm->red_max;
    if( pxscm->green_max )
	an[ 1 ] -= ( nG << 8 ) / pxscm->green_max;
    if( pxscm->blue_max )
	an[ 2 ] -= ( nB << 8 ) / pxscm->blue_max;
  
    return pxscm->base_pixel + nR * pxscm->red_mult + nG * pxscm->green_mult +
	nB * pxscm->blue_mult;
}

static void BoardDrawBorder( Display *pdsp, Pixmap pm, GC gc, int x0, int y0,
			     int x1, int y1, int nBoardSize,
			     unsigned long *apix, int f ) {
#define APIX( ipix, iEdge ) ( *( apix + (ipix) * 2 + (iEdge) ) )

    int i;
    XPoint axp[ 5 ];

    axp[ 0 ].x = axp[ 1 ].x = axp[ 4 ].x = x0 * nBoardSize;
    axp[ 2 ].x = axp[ 3 ].x = x1 * nBoardSize - 1;

    axp[ 1 ].y = axp[ 2 ].y = y0 * nBoardSize;
    axp[ 0 ].y = axp[ 3 ].y = axp[ 4 ].y = y1 * nBoardSize - 1;
    
    for( i = 0; i < nBoardSize; i++ ) {
	XSetForeground( pdsp, gc, f ? APIX( nBoardSize - i - 1, 0 ) :
			APIX( i, 1 ) );
	XDrawLines( pdsp, pm, gc, axp, 3, CoordModeOrigin );

	XSetForeground( pdsp, gc, f ? APIX( nBoardSize - i - 1, 1 ) :
			APIX( i, 0 ) );
	
	XDrawLines( pdsp, pm, gc, axp + 2, 3, CoordModeOrigin );

	axp[ 0 ].x = axp[ 1 ].x = ++axp[ 4 ].x;
	axp[ 2 ].x = --axp[ 3 ].x;

	axp[ 1 ].y = ++axp[ 2 ].y;
	axp[ 0 ].y = axp[ 3 ].y = --axp[ 4 ].y;
    }
}

static void BoardDraw( extwindow *pewnd, gamedata *pgd ) {

    XImage *pxim;
    int ix, iy, nAntialias;
    unsigned long pix, *apix = malloc( 2 * pgd->nBoardSize * sizeof( long ) );
    float x, z, rCosTheta, rDiffuse, rSpecular;
    int anCurrent[ 3 ];
    GC gc;
    XGCValues xgcv;
    
    pxim = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      12 * pgd->nBoardSize, 66 * pgd->nBoardSize,
			      8, 0 );

    pxim->data = malloc( 66 * pgd->nBoardSize * pxim->bytes_per_line );

    pgd->pmBoard = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				  pgd->nBoardSize * BOARD_WIDTH,
				  pgd->nBoardSize * BOARD_HEIGHT, pewnd->nDepth );

    anCurrent[ 0 ] = 0x09; /* 0.9^30 x 0.8 */
    anCurrent[ 1 ] = 0x35; /* 0.92 x 0x30 + 0.9^30 x 0.8 */
    anCurrent[ 2 ] = 0x09;

    pix = MatchColour( pgd->pxscm, anCurrent );
    xgcv.foreground = pix;
    
    gc = XCreateGC( pewnd->pdsp, pgd->pmBoard, GCForeground, &xgcv );
    
    XFillRectangle( pewnd->pdsp, pgd->pmBoard, gc, 0, 0, pgd->nBoardSize * BOARD_WIDTH,
		    pgd->nBoardSize * BOARD_HEIGHT );

    for( ix = 0; ix < pgd->nBoardSize; ix++ ) {
	x = 1.0 - ( (float) ix / pgd->nBoardSize );
	z = sqrt( 1.001 - x * x );
	rCosTheta = 0.9 * z - 0.3 * x;
	rDiffuse = 0.8 * rCosTheta + 0.2;
	rSpecular = pow( rCosTheta, 30 ) * 0.8;
	
	anCurrent[ 0 ] = rSpecular * 256.0;
	anCurrent[ 1 ] = anCurrent[ 0 ] + (int) ( 0x30 * rDiffuse );
	anCurrent[ 2 ] = anCurrent[ 0 ];

	APIX( ix, 0 ) = MatchColour( pgd->pxscm, anCurrent );

	x = -x;
	rCosTheta = 0.9 * z - 0.3 * x;
	rDiffuse = 0.8 * rCosTheta + 0.2;
	rSpecular = pow( rCosTheta, 30 ) * 0.8;
	
	anCurrent[ 0 ] = rSpecular * 256.0;
	anCurrent[ 1 ] = anCurrent[ 0 ] + (int) ( 0x30 * rDiffuse );
	anCurrent[ 2 ] = anCurrent[ 0 ];

	APIX( ix, 1 ) = MatchColour( pgd->pxscm, anCurrent );
    }

    for( ix = 0; ix < pgd->nBoardSize; ix++ ) {
	XSetForeground( pewnd->pdsp, gc, APIX( ix, 0 ) );
	XDrawLine( pewnd->pdsp, pgd->pmBoard, gc, ix, ix, ix + 100, ix );

	XSetForeground( pewnd->pdsp, gc, APIX( ix, 1 ) );
	XDrawLine( pewnd->pdsp, pgd->pmBoard, gc, ix, ix, ix, ix + 100 );
    }

    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 0, 0, 54, BOARD_HEIGHT,
		     pgd->nBoardSize, apix, 0 );
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 54, 0, BOARD_WIDTH, BOARD_HEIGHT,
		     pgd->nBoardSize, apix, 0 );
    
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 2, 2, 10, 34,
		     pgd->nBoardSize, apix, 1 );
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 2, 38, 10, 70,
		     pgd->nBoardSize, apix, 1 );
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 98, 2, 106, 34,
		     pgd->nBoardSize, apix, 1 );
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 98, 38, 106, 70,
		     pgd->nBoardSize, apix, 1 );

    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 11, 2, 49, 70,
		     pgd->nBoardSize, apix, 1 );
    BoardDrawBorder( pewnd->pdsp, pgd->pmBoard, gc, 59, 2, 97, 70,
		     pgd->nBoardSize, apix, 1 );
    
    /* FIXME shade hinges, using AN for error diffusion, and XPutPixel */
    
    for( iy = 0; iy < 30 * pgd->nBoardSize; iy++ )
	for( ix = 0; ix < 6 * pgd->nBoardSize; ix++ ) {
	    /* <= 0 is board; >= 20 is on a point; interpolate in between */
	    nAntialias = 2 * ( 30 * pgd->nBoardSize - iy ) + 1 - 20 *
		abs( 3 * pgd->nBoardSize - ix );

	    if( nAntialias < 0 )
		nAntialias = 0;
	    else if( nAntialias > 20 )
		nAntialias = 20;
	    
	    anCurrent[ 0 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			       ( 20 - nAntialias ) +
			       ( 0x80 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;
		      
	    anCurrent[ 1 ] = ( ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       ( 20 - nAntialias ) +
			       ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;
		      
	    anCurrent[ 2 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			       ( 20 - nAntialias ) +
			       ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;

	    pix = MatchColour( pgd->pxscm, anCurrent );

	    XPutPixel( pxim, ix + 6 * pgd->nBoardSize, iy, pix );
	    XPutPixel( pxim, ix, 66 * pgd->nBoardSize - iy - 1, pix );

	    anCurrent[ 0 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			       ( 20 - nAntialias ) +
			       ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;
		      
	    anCurrent[ 1 ] = ( ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       ( 20 - nAntialias ) +
			       ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;
		      
	    anCurrent[ 2 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			       ( 20 - nAntialias ) +
			       ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			       nAntialias ) / 20;

	    pix = MatchColour( pgd->pxscm, anCurrent );
	    
	    XPutPixel( pxim, ix, iy, pix );
	    XPutPixel( pxim, ix + 6 * pgd->nBoardSize,
		       66 * pgd->nBoardSize - iy - 1, pix );
	}

    for( iy = 0; iy < 6 * pgd->nBoardSize; iy++ )
	for( ix = 0; ix < 12 * pgd->nBoardSize; ix++ ) {
	    anCurrent[ 0 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );
	    anCurrent[ 1 ] = ( RAND & 0x3F ) + ( RAND & 0x3F );
	    anCurrent[ 2 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );

	    pix = MatchColour( pgd->pxscm, anCurrent );
	    
	    XPutPixel( pxim, ix, 30 * pgd->nBoardSize + iy, pix );
	}

    XPutImage( pewnd->pdsp, pgd->pmBoard, gc, pxim, 0, 0,
	       12 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       12 * pgd->nBoardSize, 66 * pgd->nBoardSize );

    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       12 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       12 * pgd->nBoardSize, 66 * pgd->nBoardSize,
	       24 * pgd->nBoardSize, 3 * pgd->nBoardSize );
    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       12 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       12 * pgd->nBoardSize, 66 * pgd->nBoardSize,
	       36 * pgd->nBoardSize, 3 * pgd->nBoardSize );
    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       12 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       36 * pgd->nBoardSize, 66 * pgd->nBoardSize,
	       60 * pgd->nBoardSize, 3 * pgd->nBoardSize );

    for( iy = 0; iy < 30 * pgd->nBoardSize; iy++ )
	for( ix = 0; ix < 6 * pgd->nBoardSize; ix++ ) {
	    anCurrent[ 0 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );
	    anCurrent[ 1 ] = ( RAND & 0x3F ) + ( RAND & 0x3F );
	    anCurrent[ 2 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );

	    pix = MatchColour( pgd->pxscm, anCurrent );

	    XPutPixel( pxim, ix, iy, pix );
	}
    
    XPutImage( pewnd->pdsp, pgd->pmBoard, gc, pxim, 0, 0,
	       3 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       6 * pgd->nBoardSize, 30 * pgd->nBoardSize );

    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       3 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       6 * pgd->nBoardSize, 30 * pgd->nBoardSize,
	       99 * pgd->nBoardSize, 3 * pgd->nBoardSize );
    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       3 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       6 * pgd->nBoardSize, 30 * pgd->nBoardSize,
	       3 * pgd->nBoardSize, 39 * pgd->nBoardSize );
    XCopyArea( pewnd->pdsp, pgd->pmBoard, pgd->pmBoard, gc,
	       3 * pgd->nBoardSize, 3 * pgd->nBoardSize,
	       6 * pgd->nBoardSize, 30 * pgd->nBoardSize,
	       99 * pgd->nBoardSize, 39 * pgd->nBoardSize );

    free( apix );
    
    XFreeGC( pewnd->pdsp, gc );
    
    XDestroyImage( pxim );
#undef APIX    
}

static void BoardDrawChequers( extwindow *pewnd, gamedata *pgd ) {

    GC gc;
    XImage *pximX, *pximO, *pximMask;
    int ix, iy, nIn, fx, fy, iComponent, *an, anCurrent[ 3 ];
    float x, y, z, xLoop, yLoop, rDiffuse, rSpecularX, rSpecularO, rCosTheta;

#define AN( x, y, iImage, iComponent ) \
    ( *( an + ( ( ( ( 6 * pgd->nBoardSize + 2 ) * (y) + (x) ) * 2 + (iImage) )\
		* 3 + (iComponent) ) ) )
    	
    pximX = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
			      8, 0 );

    pximO = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
			      8, 0 );

    pximMask = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      1, XYBitmap, 0, NULL,
			      6 * pgd->nBoardSize, 6 * pgd->nBoardSize,
			      8, 0 );

    pximX->data = malloc( 6 * pgd->nBoardSize * pximX->bytes_per_line );
    pximO->data = malloc( 6 * pgd->nBoardSize * pximO->bytes_per_line );
    pximMask->data = malloc( 6 * pgd->nBoardSize * pximMask->bytes_per_line );
    an = calloc( ( 6 * pgd->nBoardSize + 2 ) * ( 6 * pgd->nBoardSize + 1 ) * 6,
		 sizeof( int ) );
	
    pgd->pmX = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 6,
			      pgd->nBoardSize * 6, pewnd->nDepth );
    pgd->pmO = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 6,
			      pgd->nBoardSize * 6, pewnd->nDepth );
    pgd->pmMask = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 6,
			      pgd->nBoardSize * 6, 1 );

    for( iy = 0, yLoop = -1.0; iy < 6 * pgd->nBoardSize; iy++ ) {
	for( ix = 0, xLoop = -1.0; ix < 6 * pgd->nBoardSize; ix++ ) {
	    nIn = 0;
	    rDiffuse = rSpecularX = rSpecularO = 0.0;
	    fy = 0;
	    y = yLoop;
	    do {
		fx = 0;
		x = xLoop;
		do {
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			nIn++;
			rDiffuse += 0.2;
			z = sqrt( z ) * 5;
			if( ( rCosTheta = ( 0.9 * z - 0.316 * x - 0.3 * y ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    rDiffuse += rCosTheta * 0.8;
			    rSpecularX += pow( rCosTheta, 10 ) * 0.4;
			    rSpecularO += pow( rCosTheta, 30 ) * 0.8;
			}
		    }
		    x += 1.0 / ( 6 * pgd->nBoardSize );
		} while( !fx++ );
		y += 1.0 / ( 6 * pgd->nBoardSize );
	    } while( !fy++ );
	    
	    if( nIn < 3 ) {
		XPutPixel( pximMask, ix, iy, 0 );
		XPutPixel( pximX, ix, iy, 0 );
		XPutPixel( pximO, ix, iy, 0 );
	    } else {
		XPutPixel( pximMask, ix, iy, 1 );

		anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.8 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 0, 0 );
		anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.1 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 0, 1 );
		anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.1 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 0, 2 );

		XPutPixel( pximX, ix, iy, MatchColour( pgd->pxscm,
						       anCurrent ) );

		for( iComponent = 0; iComponent < 3; iComponent++ ) {
		    AN( ix + 1, iy, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 7 ) >> 4;
		    AN( ix - 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 3 ) >> 4;
		    AN( ix, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 5 ) >> 4;
		    AN( ix + 1, iy + 1, 0, iComponent ) +=
			anCurrent[ iComponent ] >> 4;
		}
		
		anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.01 + rSpecularO ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 1, 0 );
		anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.01 + rSpecularO ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 1, 1 );
		anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.02 + rSpecularO ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
					 AN( ix, iy, 1, 2 );

		XPutPixel( pximO, ix, iy, MatchColour( pgd->pxscm,
						       anCurrent ) );

		for( iComponent = 0; iComponent < 3; iComponent++ ) {
		    AN( ix + 1, iy, 1, iComponent ) +=
			( anCurrent[ iComponent ] * 7 ) >> 4;
		    AN( ix - 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 3 ) >> 4;
		    AN( ix, iy + 1, 1, iComponent ) +=
			( anCurrent[ iComponent ] * 5 ) >> 4;
		    AN( ix + 1, iy + 1, 1, iComponent ) +=
			anCurrent[ iComponent ] >> 4;
		}
	    }
	    xLoop += 2.0 / ( 6 * pgd->nBoardSize );
	}
	yLoop += 2.0 / ( 6 * pgd->nBoardSize );
    }

    XPutImage( pewnd->pdsp, pgd->pmX, pgd->gcCopy, pximX, 0,
	       0, 0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize );
    XPutImage( pewnd->pdsp, pgd->pmO, pgd->gcCopy, pximO, 0,
	       0, 0, 0, 6 * pgd->nBoardSize, 6 * pgd->nBoardSize );
    
    gc = XCreateGC( pewnd->pdsp, pgd->pmMask, 0, NULL );
    XPutImage( pewnd->pdsp, pgd->pmMask, gc,
	       pximMask, 0, 0, 0, 0, 6 * pgd->nBoardSize,
	       6 * pgd->nBoardSize );
    XFreeGC( pewnd->pdsp, gc );

    free( an );
    
    XDestroyImage( pximX );
    XDestroyImage( pximO );
    XDestroyImage( pximMask );
#undef AN
}

static void BoardDrawDice( extwindow *pewnd, gamedata *pgd ) {

    GC gc;
    XImage *pximX, *pximO, *pximMask;
    int ix, iy, nIn, fx, fy, iComponent, *an, anCurrent[ 3 ];
    float x, y, xLoop, yLoop, rDiffuse, rSpecularX, rSpecularO, rCosTheta,
	xNorm, yNorm, zNorm;
#define AN( x, y, iImage, iComponent ) \
    ( *( an + ( ( ( ( 7 * pgd->nBoardSize + 2 ) * (y) + (x) ) * 2 + (iImage) )\
		* 3 + (iComponent) ) ) )
    	
    pximX = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      7 * pgd->nBoardSize, 7 * pgd->nBoardSize,
			      8, 0 );

    pximO = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      7 * pgd->nBoardSize, 7 * pgd->nBoardSize,
			      8, 0 );

    pximMask = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      1, XYBitmap, 0, NULL,
			      7 * pgd->nBoardSize, 7 * pgd->nBoardSize,
			      8, 0 );

    pximX->data = malloc( 7 * pgd->nBoardSize * pximX->bytes_per_line );
    pximO->data = malloc( 7 * pgd->nBoardSize * pximO->bytes_per_line );
    pximMask->data = malloc( 7 * pgd->nBoardSize * pximMask->bytes_per_line );
    an = calloc( ( 7 * pgd->nBoardSize + 2 ) * ( 7 * pgd->nBoardSize + 1 ) * 6,
		 sizeof( int ) );
	
    pgd->pmXDice = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 7,
			      pgd->nBoardSize * 7, pewnd->nDepth );
    pgd->pmODice = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 7,
			      pgd->nBoardSize * 7, pewnd->nDepth );
    pgd->pmDiceMask = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				     pgd->nBoardSize * 7,
				     pgd->nBoardSize * 7, 1 );

    for( iy = 0, yLoop = -1.0; iy < 7 * pgd->nBoardSize; iy++ ) {
	for( ix = 0, xLoop = -1.0; ix < 7 * pgd->nBoardSize; ix++ ) {
	    nIn = 0;
	    rDiffuse = rSpecularX = rSpecularO = 0.0;
	    fy = 0;
	    y = yLoop;
	    do {
		fx = 0;
		x = xLoop;
		do {
		    if( fabs( x ) < 6.0 / 7.0 &&
			fabs( y ) < 6.0 / 7.0 ) {
			nIn++;
			rDiffuse += 0.92; /* 0.9 x 0.8 + 0.2 (ambient) */
			rSpecularX += 0.139; /* 0.9^10 x 0.4 */
			rSpecularO += 0.034; /* 0.9^30 x 0.8 */
		    } else {
			if( fabs( x ) < 6.0 / 7.0 ) {
			    xNorm = 0.0;
			    yNorm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    zNorm = sqrt( 1.001 - yNorm * yNorm );
			} else if( fabs( y ) < 6.0 / 7.0 ) {
			    xNorm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    yNorm = 0.0;
			    zNorm = sqrt( 1.001 - xNorm * xNorm );
			} else {
			    xNorm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    yNorm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( zNorm = 1 - xNorm * xNorm -
				  yNorm * yNorm ) < 0.0 )
				goto missed;
			    zNorm = sqrt( zNorm );
			}
			
			nIn++;
			rDiffuse += 0.2;
			if( ( rCosTheta = 0.9 * zNorm - 0.316 * xNorm -
			      0.3 * yNorm ) > 0.0 ) {
			    rDiffuse += rCosTheta * 0.8;
			    rSpecularX += pow( rCosTheta, 10 ) * 0.4;
			    rSpecularO += pow( rCosTheta, 30 ) * 0.8;
			}
		    }
		missed:		    
		    x += 1.0 / ( 7 * pgd->nBoardSize );
		} while( !fx++ );
		y += 1.0 / ( 7 * pgd->nBoardSize );
	    } while( !fy++ );
	    
	    if( nIn < 2 ) {
		XPutPixel( pximMask, ix, iy, 0 );
		XPutPixel( pximX, ix, iy, 0 );
		XPutPixel( pximO, ix, iy, 0 );
	    } else {
		XPutPixel( pximMask, ix, iy, 1 );

		anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.8 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 0 );
		anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.1 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 1 );
		anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.1 + rSpecularX ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 2 );

		XPutPixel( pximX, ix, iy, MatchColour( pgd->pxscm,
						       anCurrent ) );

		for( iComponent = 0; iComponent < 3; iComponent++ ) {
		    AN( ix + 1, iy, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 7 ) >> 4;
		    AN( ix - 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 3 ) >> 4;
		    AN( ix, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 5 ) >> 4;
		    AN( ix + 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 1 ) >> 4;
		}
		
		anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.01 + rSpecularO ) *
		    64.0 + ( 4 - nIn ) * 32.0 ) + AN( ix, iy, 1, 0 );
		anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.01 + rSpecularO ) *
		    64.0 + ( 4 - nIn ) * 32.0 ) + AN( ix, iy, 1, 1 );
		anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.02 + rSpecularO ) *
		    64.0 + ( 4 - nIn ) * 32.0 ) + AN( ix, iy, 1, 2 );

		XPutPixel( pximO, ix, iy, MatchColour( pgd->pxscm,
						       anCurrent ) );

		for( iComponent = 0; iComponent < 3; iComponent++ ) {
		    AN( ix + 1, iy, 1, iComponent ) +=
			( anCurrent[ iComponent ] * 7 ) >> 4;
		    AN( ix - 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 3 ) >> 4;
		    AN( ix, iy + 1, 1, iComponent ) +=
			( anCurrent[ iComponent ] * 5 ) >> 4;
		    AN( ix + 1, iy + 1, 1, iComponent ) +=
			( anCurrent[ iComponent ] * 1 ) >> 4;
		}
	    }
	    xLoop += 2.0 / ( 7 * pgd->nBoardSize );
	}
	yLoop += 2.0 / ( 7 * pgd->nBoardSize );
    }

    XPutImage( pewnd->pdsp, pgd->pmXDice, pgd->gcCopy, pximX,
	       0, 0, 0, 0, 7 * pgd->nBoardSize, 7 * pgd->nBoardSize );
    XPutImage( pewnd->pdsp, pgd->pmODice, pgd->gcCopy, pximO,
	       0, 0, 0, 0, 7 * pgd->nBoardSize, 7 * pgd->nBoardSize );
    
    gc = XCreateGC( pewnd->pdsp, pgd->pmDiceMask, 0, NULL );
    XPutImage( pewnd->pdsp, pgd->pmDiceMask, gc,
	       pximMask, 0, 0, 0, 0, 7 * pgd->nBoardSize,
	       7 * pgd->nBoardSize );
    XFreeGC( pewnd->pdsp, gc );

    free( an );
    
    XDestroyImage( pximX );
    XDestroyImage( pximO );
    XDestroyImage( pximMask );
#undef AN
}

static void BoardDrawCube( extwindow *pewnd, gamedata *pgd ) {

    GC gc;
    XImage *pximCube, *pximMask;
    int ix, iy, nIn, fx, fy, iComponent, *an, anCurrent[ 3 ];
    float x, y, xLoop, yLoop, rDiffuse, rSpecular, rCosTheta,
	xNorm, yNorm, zNorm;
#define AN( x, y, iImage, iComponent ) \
    ( *( an + ( ( ( ( 8 * pgd->nBoardSize + 2 ) * (y) + (x) ) * 2 + (iImage) )\
		* 3 + (iComponent) ) ) )
    	
    pximCube = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      8 * pgd->nBoardSize, 8 * pgd->nBoardSize,
			      8, 0 );

    pximMask = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      1, XYBitmap, 0, NULL,
			      8 * pgd->nBoardSize, 8 * pgd->nBoardSize,
			      8, 0 );

    pximCube->data = malloc( 8 * pgd->nBoardSize * pximCube->bytes_per_line );
    pximMask->data = malloc( 8 * pgd->nBoardSize * pximMask->bytes_per_line );
    an = calloc( ( 8 * pgd->nBoardSize + 2 ) * ( 8 * pgd->nBoardSize + 1 ) * 6,
		 sizeof( int ) );
	
    pgd->pmCube = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize * 8,
				 pgd->nBoardSize * 8, pewnd->nDepth );
    pgd->pmCubeMask = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				     pgd->nBoardSize * 8,
				     pgd->nBoardSize * 8, 1 );

    for( iy = 0, yLoop = -1.0; iy < 8 * pgd->nBoardSize; iy++ ) {
	for( ix = 0, xLoop = -1.0; ix < 8 * pgd->nBoardSize; ix++ ) {
	    nIn = 0;
	    rDiffuse = rSpecular = 0.0;
	    fy = 0;
	    y = yLoop;
	    do {
		fx = 0;
		x = xLoop;
		do {
		    if( fabs( x ) < 7.0 / 8.0 &&
			fabs( y ) < 7.0 / 8.0 ) {
			nIn++;
			rDiffuse += 0.92; /* 0.9 x 0.8 + 0.2 (ambient) */
			rSpecular += 0.139; /* 0.9^10 x 0.4 */
		    } else {
			if( fabs( x ) < 7.0 / 8.0 ) {
			    xNorm = 0.0;
			    yNorm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    zNorm = sqrt( 1.001 - yNorm * yNorm );
			} else if( fabs( y ) < 7.0 / 8.0 ) {
			    xNorm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    yNorm = 0.0;
			    zNorm = sqrt( 1.001 - xNorm * xNorm );
			} else {
			    xNorm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    yNorm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( zNorm = 1 - xNorm * xNorm -
				  yNorm * yNorm ) < 0.0 )
				goto missed;
			    zNorm = sqrt( zNorm );
			}
			
			nIn++;
			rDiffuse += 0.2;
			if( ( rCosTheta = 0.9 * zNorm - 0.316 * xNorm -
			      0.3 * yNorm ) > 0.0 ) {
			    rDiffuse += rCosTheta * 0.8;
			    rSpecular += pow( rCosTheta, 10 ) * 0.4;
			}
		    }
		missed:		    
		    x += 1.0 / ( 8 * pgd->nBoardSize );
		} while( !fx++ );
		y += 1.0 / ( 8 * pgd->nBoardSize );
	    } while( !fy++ );
	    
	    if( nIn < 2 ) {
		XPutPixel( pximMask, ix, iy, 0 );
		XPutPixel( pximCube, ix, iy, 0 );
	    } else {
		XPutPixel( pximMask, ix, iy, 1 );

		anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.8 + rSpecular ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 0 );
		anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.8 + rSpecular ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 1 );
		anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.8 + rSpecular ) *
					 64.0 + ( 4 - nIn ) * 32.0 ) +
		    AN( ix, iy, 0, 2 );

		XPutPixel( pximCube, ix, iy, MatchColour( pgd->pxscm,
						       anCurrent ) );

		for( iComponent = 0; iComponent < 3; iComponent++ ) {
		    AN( ix + 1, iy, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 7 ) >> 4;
		    AN( ix - 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 3 ) >> 4;
		    AN( ix, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 5 ) >> 4;
		    AN( ix + 1, iy + 1, 0, iComponent ) +=
			( anCurrent[ iComponent ] * 1 ) >> 4;
		}
	    }
	    xLoop += 2.0 / ( 8 * pgd->nBoardSize );
	}
	yLoop += 2.0 / ( 8 * pgd->nBoardSize );
    }

    XPutImage( pewnd->pdsp, pgd->pmCube, pgd->gcCopy, pximCube,
	       0, 0, 0, 0, 8 * pgd->nBoardSize, 8 * pgd->nBoardSize );
    
    gc = XCreateGC( pewnd->pdsp, pgd->pmCubeMask, 0, NULL );
    XPutImage( pewnd->pdsp, pgd->pmCubeMask, gc,
	       pximMask, 0, 0, 0, 0, 8 * pgd->nBoardSize,
	       8 * pgd->nBoardSize );
    XFreeGC( pewnd->pdsp, gc );

    free( an );
    
    XDestroyImage( pximCube );
    XDestroyImage( pximMask );
#undef AN
}

static void BoardDrawPips( extwindow *pewnd, gamedata *pgd ) {

    XImage *pximX, *pximO;
    int ix, iy, nIn, fx, fy, iComponent, *an, anCurrent[ 3 ];
    float x, y, z, xLoop, yLoop, rDiffuse, rSpecularX, rSpecularO, rCosTheta;
#define AN( x, y, iImage, iComponent ) \
    ( *( an + ( ( ( ( pgd->nBoardSize + 2 ) * (y) + (x) ) * 2 + (iImage) )\
		* 3 + (iComponent) ) ) )
    	
    pximX = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      pgd->nBoardSize, pgd->nBoardSize,
			      8, 0 );

    pximO = XCreateImage( pewnd->pdsp,
			     /* FIXME this is horrible */ DefaultVisual(
				 pewnd->pdsp, 0 ),
			      pewnd->nDepth, ZPixmap, 0, NULL,
			      pgd->nBoardSize, pgd->nBoardSize,
			      8, 0 );

    pximX->data = malloc( pgd->nBoardSize * pximX->bytes_per_line );
    pximO->data = malloc( pgd->nBoardSize * pximO->bytes_per_line );
    an = calloc( ( pgd->nBoardSize + 2 ) * ( pgd->nBoardSize + 1 ) * 6,
		 sizeof( int ) );
	
    pgd->pmXPip = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize,
			      pgd->nBoardSize, pewnd->nDepth );
    pgd->pmOPip = XCreatePixmap( pewnd->pdsp, pewnd->wnd, pgd->nBoardSize,
			      pgd->nBoardSize, pewnd->nDepth );

    for( iy = 0, yLoop = -1.0; iy < pgd->nBoardSize; iy++ ) {
	for( ix = 0, xLoop = -1.0; ix < pgd->nBoardSize; ix++ ) {
	    nIn = 0;
	    rDiffuse = rSpecularX = rSpecularO = 0.0;
	    fy = 0;
	    y = yLoop;
	    do {
		fx = 0;
		x = xLoop;
		do {
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			nIn++;
			rDiffuse += 0.2;
			z = sqrt( z ) * 5;
			if( ( rCosTheta = ( 0.316 * x + 0.3 * y + 0.9 * z ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    rDiffuse += rCosTheta * 0.8;
			    rSpecularX += pow( rCosTheta, 10 ) * 0.4;
			    rSpecularO += pow( rCosTheta, 30 ) * 0.8;
			}
		    }
		    x += 1.0 / ( pgd->nBoardSize );
		} while( !fx++ );
		y += 1.0 / ( pgd->nBoardSize );
	    } while( !fy++ );
	    
	    anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.8 + 0.139 ) * 64.0 ) +
		AN( ix, iy, 0, 0 );
	    anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.1 + 0.139 ) * 64.0 ) +
		AN( ix, iy, 0, 1 );
	    anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.1 + 0.139 ) * 64.0 ) +
		AN( ix, iy, 0, 2 );

	    XPutPixel( pximX, ix, iy, MatchColour( pgd->pxscm,
						   anCurrent ) );

	    for( iComponent = 0; iComponent < 3; iComponent++ ) {
		AN( ix + 1, iy, 0, iComponent ) +=
		    ( anCurrent[ iComponent ] * 7 ) >> 4;
		AN( ix - 1, iy + 1, 0, iComponent ) +=
		    ( anCurrent[ iComponent ] * 3 ) >> 4;
		AN( ix, iy + 1, 0, iComponent ) +=
		    ( anCurrent[ iComponent ] * 5 ) >> 4;
		AN( ix + 1, iy + 1, 0, iComponent ) +=
		    ( anCurrent[ iComponent ] * 1 ) >> 4;
	    }
	    
	    anCurrent[ 0 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.01 + 0.034 ) * 64.0 ) +
		AN( ix, iy, 1, 0 );
	    anCurrent[ 1 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.01 + 0.034 ) * 64.0 ) +
		AN( ix, iy, 1, 1 );
	    anCurrent[ 2 ] = (int) ( ( rDiffuse * 0.7 + rSpecularX ) * 64.0 +
		( 4 - nIn ) * ( 0.92 * 0.02 + 0.034 ) * 64.0 ) +
		AN( ix, iy, 1, 2 );

	    XPutPixel( pximO, ix, iy, MatchColour( pgd->pxscm,
						   anCurrent ) );
	    
	    for( iComponent = 0; iComponent < 3; iComponent++ ) {
		AN( ix + 1, iy, 1, iComponent ) +=
		    ( anCurrent[ iComponent ] * 7 ) >> 4;
		AN( ix - 1, iy + 1, 0, iComponent ) +=
		    ( anCurrent[ iComponent ] * 3 ) >> 4;
		AN( ix, iy + 1, 1, iComponent ) +=
		    ( anCurrent[ iComponent ] * 5 ) >> 4;
		AN( ix + 1, iy + 1, 1, iComponent ) +=
		    ( anCurrent[ iComponent ] * 1 ) >> 4;
	    }
	    xLoop += 2.0 / ( pgd->nBoardSize );
	}
	yLoop += 2.0 / ( pgd->nBoardSize );
    }

    XPutImage( pewnd->pdsp, pgd->pmXPip, pgd->gcCopy, pximX,
	       0, 0, 0, 0, pgd->nBoardSize, pgd->nBoardSize );
    XPutImage( pewnd->pdsp, pgd->pmOPip, pgd->gcCopy, pximO,
	       0, 0, 0, 0, pgd->nBoardSize, pgd->nBoardSize );

    free( an );
    
    XDestroyImage( pximX );
    XDestroyImage( pximO );
#undef AN
}

static void BoardFreePixmaps( extwindow *pewnd, gamedata *pgd ) {

    XFreePixmap( pewnd->pdsp, pgd->pmBoard );
    XFreePixmap( pewnd->pdsp, pgd->pmX );
    XFreePixmap( pewnd->pdsp, pgd->pmO );
    XFreePixmap( pewnd->pdsp, pgd->pmMask );
    XFreePixmap( pewnd->pdsp, pgd->pmXDice );
    XFreePixmap( pewnd->pdsp, pgd->pmODice );
    XFreePixmap( pewnd->pdsp, pgd->pmDiceMask );
    XFreePixmap( pewnd->pdsp, pgd->pmXPip );
    XFreePixmap( pewnd->pdsp, pgd->pmOPip );
    XFreePixmap( pewnd->pdsp, pgd->pmCube );
    XFreePixmap( pewnd->pdsp, pgd->pmCubeMask );    
    XFreePixmap( pewnd->pdsp, pgd->pmSaved );
    XFreePixmap( pewnd->pdsp, pgd->pmTemp );
    XFreePixmap( pewnd->pdsp, pgd->pmTempSaved );
    XFreePixmap( pewnd->pdsp, pgd->pmPoint );
    ExtWndDetachFont( pewnd, pgd->pxfsCube->fid );
    
    /* FIXME unload cube font */
}

static void BoardConfigure( extwindow *pewnd, gamedata *pgd,
			   XConfigureEvent *pxcev ) {

    int nOldSize = pgd->nBoardSize;
    
    if( ( pgd->nBoardSize = pxcev->width / BOARD_WIDTH < pxcev->height / BOARD_HEIGHT ?
	  pxcev->width / BOARD_WIDTH : pxcev->height / BOARD_HEIGHT ) != nOldSize ) {

	BoardFreePixmaps( pewnd, pgd );
	
	BoardDraw( pewnd, pgd );
	BoardDrawChequers( pewnd, pgd );
	BoardDrawDice( pewnd, pgd );
	BoardDrawPips( pewnd, pgd );
	BoardDrawCube( pewnd, pgd );
	BoardSetCubeFont( pewnd, pgd );
	
	pgd->pmSaved = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				      6 * pgd->nBoardSize,
				      6 * pgd->nBoardSize, pewnd->nDepth );

	pgd->pmTemp = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				      6 * pgd->nBoardSize,
				      6 * pgd->nBoardSize, pewnd->nDepth );
	
	pgd->pmTempSaved = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				      6 * pgd->nBoardSize,
				      6 * pgd->nBoardSize, pewnd->nDepth );

	pgd->pmPoint = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				      6 * pgd->nBoardSize,
				      35 * pgd->nBoardSize, pewnd->nDepth );
    }
}

static void BoardCreate( extwindow *pewnd, gamedata *pgd ) {

    XGCValues xgcv;
    int anBlue[] = { 0, 0, 128 };
    pgd->nDragPoint = -1;
    
    pgd->nBoardSize = pewnd->cx / BOARD_WIDTH < pewnd->cy / BOARD_HEIGHT ?
	pewnd->cx / BOARD_WIDTH : pewnd->cy / BOARD_HEIGHT;

    /* FIXME this should be done before the window is created, so a
       different colourmap can be chosen if necessary */
    pgd->pxscm = ExtGetColourmap( pewnd->pdsp, pewnd->wndRoot, pewnd->vid );

    xgcv.function = GXand;
    xgcv.foreground = AllPlanes;
    xgcv.background = 0;
    pgd->gcAnd = ExtWndAttachGC( pewnd, GCFunction | GCForeground |
				 GCBackground, &xgcv );
    
    xgcv.function = GXor;
    pgd->gcOr = ExtWndAttachGC( pewnd, GCFunction, &xgcv );

    /* FIXME ExtWndAttachGC( pewnd, 0, NULL ) gives a bad GC, why? */
    pgd->gcCopy = XDefaultGC( pewnd->pdsp, 0 );

    xgcv.foreground = MatchColour( pgd->pxscm, anBlue );
    pgd->gcCube = ExtWndAttachGC( pewnd, GCForeground, &xgcv );
    
    pgd->xDice[ 0 ] = pgd->xDice[ 1 ] = -10 * pgd->nBoardSize;
    
    BoardDraw( pewnd, pgd );
    BoardDrawChequers( pewnd, pgd );
    BoardDrawDice( pewnd, pgd );
    BoardDrawPips( pewnd, pgd );
    BoardDrawCube( pewnd, pgd );
    BoardSetCubeFont( pewnd, pgd );
    
    pgd->pmSaved = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				  6 * pgd->nBoardSize,
				  6 * pgd->nBoardSize, pewnd->nDepth );

    pgd->pmTemp = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				  6 * pgd->nBoardSize,
				  6 * pgd->nBoardSize, pewnd->nDepth );

    pgd->pmTempSaved = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				  6 * pgd->nBoardSize,
				  6 * pgd->nBoardSize, pewnd->nDepth );

    pgd->pmPoint = XCreatePixmap( pewnd->pdsp, pewnd->wnd,
				  6 * pgd->nBoardSize,
				  35 * pgd->nBoardSize, pewnd->nDepth );
}

static int BoardHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	BoardRedraw( pewnd, pewnd->pv, &pxev->xexpose );
	break;

    case ConfigureNotify:
	BoardConfigure( pewnd, pewnd->pv, &pxev->xconfigure );
	break;

    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	BoardPointer( pewnd, pewnd->pv, pxev );
	break;

    case DestroyNotify:
	/* FIXME BoardDestroyNotify( pewnd, pewnd->pv ); */
	break;
	
    case ExtCreateNotify:
	BoardCreate( pewnd, pewnd->pv );
	break;
    }
    
    return ExtHandler( pewnd, pxev );
}

extwindowclass ewcBoard = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
    Button1MotionMask | PointerMotionHintMask,
    BOARD_WIDTH, BOARD_HEIGHT, 0, 0,
    BoardHandler,
    "Board",
    NULL
};
