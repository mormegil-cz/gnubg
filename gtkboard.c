/*
 * gtkboard.c
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2001.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <gtk/gtk.h>
#include <isaac.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtk-multiview.h"
#include "gtkprefs.h"
#include "positionid.h"
#include "render.h"
#include "renderprefs.h"
#include "sound.h"
#include "matchid.h"
#include "i18n.h"

#define POINT_UNUSED0 28 /* the top unused bearoff tray */
#define POINT_UNUSED1 29 /* the bottom unused bearoff tray */
#define POINT_DICE 30
#define POINT_CUBE 31
#define POINT_RIGHT 32
#define POINT_LEFT 33
#define POINT_RESIGN 34

#define CLICK_TIME 200 /* minimum time in milliseconds before a drag to the
			  same point is considered a real drag rather than a
			  click */
#define MASK_INVISIBLE 1
#define MASK_VISIBLE 0

#if !USE_GTK2
#define gtk_image_new_from_pixmap gtk_pixmap_new
#endif

static int positions[ 2 ][ 30 ][ 3 ] = { {
    { 51, 25, 7 },
    { 90, 63, 6 }, { 84, 63, 6 }, { 78, 63, 6 }, { 72, 63, 6 }, { 66, 63, 6 },
    { 60, 63, 6 }, { 42, 63, 6 }, { 36, 63, 6 }, { 30, 63, 6 }, { 24, 63, 6 },
    { 18, 63, 6 }, { 12, 63, 6 },
    { 12, 3, -6 }, { 18, 3, -6 }, { 24, 3, -6 }, { 30, 3, -6 }, { 36, 3, -6 },
    { 42, 3, -6 }, { 60, 3, -6 }, { 66, 3, -6 }, { 72, 3, -6 }, { 78, 3, -6 },
    { 84, 3, -6 }, { 90, 3, -6 },
    { 51, 41, -7 }, { 99, 63, 6 }, { 99, 3, -6 }, { 3, 63, 6 }, { 3, 3, -6 }
}, {
    { 51, 25, 7 },
    { 12, 63, 6 }, { 18, 63, 6 }, { 24, 63, 6 }, { 30, 63, 6 }, { 36, 63, 6 },
    { 42, 63, 6 }, { 60, 63, 6 }, { 66, 63, 6 }, { 72, 63, 6 }, { 78, 63, 6 },
    { 84, 63, 6 }, { 90, 63, 6 },
    { 90, 3, -6 }, { 84, 3, -6 }, { 78, 3, -6 }, { 72, 3, -6 }, { 66, 3, -6 },
    { 60, 3, -6 }, { 42, 3, -6 }, { 36, 3, -6 }, { 30, 3, -6 }, { 24, 3, -6 },
    { 18, 3, -6 }, { 12, 3, -6 },
    { 51, 41, -7 }, { 3, 63, 6 }, { 3, 3, -6 }, { 99, 63, 6 }, { 99, 3, -6 }
} };

animation animGUI = ANIMATE_SLIDE;
int nGUIAnimSpeed = 4, fGUIBeep = TRUE, fGUIDiceArea = FALSE,
    fGUIHighDieFirst = TRUE, fGUIIllegal = FALSE, fGUIShowIDs = TRUE,
    fGUIShowPips = TRUE, fGUIDragTargetHelp = TRUE;

#if !GTK_CHECK_VERSION(1,3,0)
#define g_alloca alloca
#endif

static GtkVBoxClass *parent_class = NULL;

static randctx rc;
#define RAND irand( &rc )

extern GtkWidget *board_new( void ) {

    return GTK_WIDGET( gtk_type_new( board_get_type() ) );
}

static int intersects( int x0, int y0, int cx0, int cy0,
		       int x1, int y1, int cx1, int cy1 ) {

    return ( y1 + cy1 > y0 ) && ( y1 < y0 + cy0 ) &&
	( x1 + cx1 > x0 ) && ( x1 < x0 + cx0 );
}

static inline double ssqrt( double x ) {

    return x < 0.0 ? 0.0 : sqrt( x );
}

static inline guchar clamp( gint n ) {

    if( n < 0 )
	return 0;
    else if( n > 0xFF )
	return 0xFF;
    else
	return n;
}

static void board_beep( BoardData *bd ) {

    if( fGUIIllegal )
	gdk_beep();
}

static void read_board( BoardData *bd, gint points[ 2 ][ 25 ] ) {

    gint i;

    for( i = 0; i < 24; i++ ) {
        points[ bd->turn <= 0 ][ i ] = bd->points[ 24 - i ] < 0 ?
            abs( bd->points[ 24 - i ] ) : 0;
        points[ bd->turn > 0 ][ i ] = bd->points[ i + 1 ] > 0 ?
            abs( bd->points[ i + 1 ] ) : 0;
    }
    
    points[ bd->turn <= 0 ][ 24 ] = abs( bd->points[ 0 ] );
    points[ bd->turn > 0 ][ 24 ] = abs( bd->points[ 25 ] );
}


static void
write_points ( gint points[ 28 ], const gint turn, const gint nchequers, 
               int anBoard[ 2 ][ 25 ] ) {

  gint i;
  gint anOff[ 2 ];

  for ( i = 0; i < 28; ++i )
    points[ i ] = 0;

  /* Opponent on bar */
  points[ 0 ] = -anBoard[ 0 ][ 24 ];

  /* Board */
  for( i = 0; i < 24; i++ ) {
    if ( anBoard[ turn > 0 ][ i ] )
      points[ i + 1 ] = turn * anBoard[ turn > 0 ][ i ];
    if ( anBoard[ turn <= 0 ][ i ] )
      points[ 24 - i ] = -turn * anBoard[ turn <= 0 ][ i ];
  }

  /* Player on bar */
  points[ 25 ] = anBoard[ 1 ][ 24 ];

  anOff[ 0 ] = anOff[ 1 ] = nchequers;
  for( i = 0; i < 25; i++ ) {
    anOff[ 0 ] -= anBoard[ 0 ][ i ];
    anOff[ 1 ] -= anBoard[ 1 ][ i ];
  }
    
  points[ 26 ] = anOff[ turn >= 0  ];
  points[ 27 ] = anOff[ turn < 0 ];

}

static void
write_board ( BoardData *bd, int anBoard[ 2 ][ 25 ] ) {

  write_points( bd->points, bd->turn, bd->nchequers, anBoard );

}

static void chequer_position( int point, int chequer, int *px, int *py ) {

    int c_chequer;

    c_chequer = ( !point || point == 25 ) ? 3 : 5;

    if( chequer > c_chequer )
	chequer = c_chequer;
    
    *px = positions[ fClockwise ][ point ][ 0 ];
    *py = positions[ fClockwise ][ point ][ 1 ] - ( chequer - 1 ) *
	positions[ fClockwise ][ point ][ 2 ];
}

static void point_area( BoardData *bd, int n, int *px, int *py,
			int *pcx, int *pcy ) {
    
    int c_chequer = ( !n || n == 25 ) ? 3 : 5;
    
    *px = positions[ fClockwise ][ n ][ 0 ] * rdAppearance.nSize;
    *py = positions[ fClockwise ][ n ][ 1 ] * rdAppearance.nSize;
    *pcx = 6 * rdAppearance.nSize;
    *pcy = positions[ fClockwise ][ n ][ 2 ] * rdAppearance.nSize;
    
    if( *pcy > 0 ) {
	*pcy = *pcy * ( c_chequer - 1 ) + 6 * rdAppearance.nSize;
	*py += 6 * rdAppearance.nSize - *pcy;
    } else
	*pcy = -*pcy * ( c_chequer - 1 ) + 6 * rdAppearance.nSize;
}

/* Determine the position and rotation of the cube; *px and *py return the
   position (in board units -- multiply by rdAppearance.nSize to get
   pixels) and *porient returns the rotation (1 = facing the top, 0 = facing
   the side, -1 = facing the bottom). */
static void cube_position( BoardData *bd, int *px, int *py, int *porient ) {

    if( bd->crawford_game || !bd->cube_use ) {
	/* no cube */
	if( px ) *px = -32768;
	if( py ) *py = -32768;
	if( porient ) *porient = -1;
    } else if( bd->doubled ) {
	if( px ) *px = 50 - 20 * bd->doubled;
	if( py ) *py = 32;
	if( porient ) *porient = bd->doubled;
    } else {
	if( px ) *px = 50;
	if( py ) *py = 32 - 29 * bd->cube_owner;
	if( porient ) *porient = bd->cube_owner;
    }
}

static void resign_position( BoardData *bd, int *px, int *py, int *porient ) {

  if( bd->resigned ) {
    if ( px ) *px = 50 + 30 * bd->resigned / abs ( bd->resigned );
    if ( py ) *py = 32;
    if( porient ) *porient = - bd->resigned / abs ( bd->resigned );
  }
  else {
    /* no resignation */
    if( px ) *px = -32768;
    if( py ) *py = -32768;
    if( porient ) *porient = -1;
  }

}

#define ARROW_SIZE 5

static void Arrow_Position( BoardData *bd, int *px, int *py ) {
/* calculate the position of the arrow to indicate
   player on turn and direction of play;  *px and *py
   return the position of the upper left corner in pixels,
   NOT board units */

    int Point28_x, Point28_y, Point28_dx, Point28_dy;
    int Point29_x, Point29_y, Point29_dx, Point29_dy;
    point_area( bd, POINT_UNUSED0, &Point28_x, &Point28_y, &Point28_dx, &Point28_dy );
    point_area( bd, POINT_UNUSED1, &Point29_x, &Point29_y, &Point29_dx, &Point29_dy );

    assert( Point28_x == Point29_x );
    assert( Point28_dx == Point29_dx );
    assert( Point28_dy == Point29_dy );

    if ( px ) *px = Point29_x + Point29_dx / 2
			- rdAppearance.nSize * ARROW_SIZE / 2;
    if ( py ) *py = Point29_y + Point29_dy + Point29_dx / 2
			- rdAppearance.nSize * ARROW_SIZE / 2;
}

static void RenderArea( BoardData *bd, unsigned char *puch, int x, int y,
			int cx, int cy ) {
    
    int anBoard[ 2 ][ 25 ], anOff[ 2 ], anDice[ 2 ], anDicePosition[ 2 ][ 2 ],
	anCubePosition[ 2 ], anArrowPosition[ 2 ], nOrient;
    int anResignPosition[ 2 ], nResignOrientation;

    read_board( bd, anBoard );
    if( bd->colour != bd->turn )
	SwapSides( anBoard );
    anOff[ 0 ] = -bd->points[ 27 ];
    anOff[ 1 ] = bd->points[ 26 ];
    anDice[ 0 ] = bd->colour == bd->turn ? bd->dice[ 0 ] :
	bd->dice_opponent[ 0 ];
    anDice[ 1 ] = bd->colour == bd->turn ? bd->dice[ 1 ] :
	bd->dice_opponent[ 1 ];
    anDicePosition[ 0 ][ 0 ] = bd->x_dice[ 0 ];
    anDicePosition[ 0 ][ 1 ] = bd->y_dice[ 0 ];
    anDicePosition[ 1 ][ 0 ] = bd->x_dice[ 1 ];
    anDicePosition[ 1 ][ 1 ] = bd->y_dice[ 1 ];
    cube_position( bd, anCubePosition, anCubePosition + 1, &nOrient );
    resign_position( bd, anResignPosition, anResignPosition + 1, 
                     &nResignOrientation );
    Arrow_Position( bd, &anArrowPosition[ 0 ], &anArrowPosition[ 1 ] );
    CalculateArea( &rdAppearance, puch, cx * 3, &bd->ri, anBoard, anOff,
		   anDice, anDicePosition, bd->colour == bd->turn,
		   anCubePosition, LogCube( bd->cube ) + ( bd->doubled != 0 ),
		   nOrient, anResignPosition,
		   abs(bd->resigned), nResignOrientation,
		   anArrowPosition, bd->playing, bd->turn == 1,
                   x, y, cx, cy );
}

static gboolean board_expose( GtkWidget *drawing_area, GdkEventExpose *event,
			      BoardData *bd ) {
    
    int x, y, cx, cy;
    unsigned char *puch;

    assert( GTK_IS_DRAWING_AREA( drawing_area ) );
    
    if( rdAppearance.nSize <= 0 )
	return TRUE;

    x = event->area.x;
    y = event->area.y;
    cx = event->area.width;
    cy = event->area.height;

    if( x < 0 ) {
	cx += x;
	x = 0;
    }

    if( y < 0 ) {
	cy += y;
	y = 0;
    }

    if( y + cy > 72 * rdAppearance.nSize )
	cy = 72 * rdAppearance.nSize - y;

    if( x + cx > 108 * rdAppearance.nSize )
	cx = 108 * rdAppearance.nSize - x;

    if( cx <= 0 || cy <= 0 )
	return TRUE;
    
    puch = malloc( cx * cy * 3 );

    RenderArea( bd, puch, x, y, cx, cy );

    /* FIXME use dithalign */
    gdk_draw_rgb_image( drawing_area->window, bd->gc_copy, x, y, cx, cy,
			GDK_RGB_DITHER_MAX, puch, cx * 3 );
    
    free(puch);
    
    return TRUE;
}

static void board_invalidate_rect( GtkWidget *drawing_area, int x, int y,
				   int cx, int cy, BoardData *bd ) {
    
    assert( GTK_IS_DRAWING_AREA( drawing_area ) );
    
#if GTK_CHECK_VERSION(2,0,0)
    {
	GdkRectangle r;
	
	r.x = x;
	r.y = y;
	r.width = cx;
	r.height = cy;
	
	gdk_window_invalidate_rect( drawing_area->window, &r, FALSE );
    }
#else
    {
	GdkEventExpose event;
    
	event.count = 0;
	event.area.x = x;
	event.area.y = y;
	event.area.width = cx;
	event.area.height = cy;
	
	board_expose( drawing_area, &event, bd );
    }
#endif    
}

static void board_invalidate_point( BoardData *bd, int n ) {

    int x, y, cx, cy;

    if( rdAppearance.nSize <= 0 )
	return;

    point_area( bd, n, &x, &y, &cx, &cy );

    board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );
}

static void board_invalidate_dice( BoardData *bd ) {

    int x, y, cx, cy;
    
    x = bd->x_dice[ 0 ] * rdAppearance.nSize;
    y = bd->y_dice[ 0 ] * rdAppearance.nSize;
    cx = 7 * rdAppearance.nSize;
    cy = 7 * rdAppearance.nSize;

    board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );
    
    x = bd->x_dice[ 1 ] * rdAppearance.nSize;
    y = bd->y_dice[ 1 ] * rdAppearance.nSize;

    board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );
}

static void board_invalidate_cube( BoardData *bd ) {

    int x, y, orient;
    
    cube_position( bd, &x, &y, &orient );
    
    board_invalidate_rect( bd->drawing_area, x * rdAppearance.nSize,
			   y * rdAppearance.nSize,
			   8 * rdAppearance.nSize, 8 * rdAppearance.nSize, bd );
}

static void board_invalidate_resign( BoardData *bd ) {

    int x, y, orient;
    
    resign_position( bd, &x, &y, &orient );
    
    board_invalidate_rect( bd->drawing_area, x * rdAppearance.nSize,
			   y * rdAppearance.nSize,
			   8 * rdAppearance.nSize, 8 * rdAppearance.nSize, bd );
}

static void board_invalidate_arrow( BoardData *bd ) {

    int x, y;
    
    Arrow_Position( bd, &x, &y );

    board_invalidate_rect( bd->drawing_area, x, y,
			   ARROW_SIZE * rdAppearance.nSize, ARROW_SIZE * rdAppearance.nSize, bd );
}

#undef ARROW_SIZE

static int board_point( GtkWidget *board, BoardData *bd, int x0, int y0 ) {

    int i, x, y, cx, cy, xCube, yCube;

    x0 /= rdAppearance.nSize;
    y0 /= rdAppearance.nSize;

    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;
    
    cube_position( bd, &xCube, &yCube, NULL );
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_CUBE;

    resign_position( bd, &xCube, &yCube, NULL );
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_RESIGN;
    
    /* jsc: support for faster rolling of dice by clicking board
      *
      * These arguments should be dynamically calculated instead 
      * of hardcoded, but it's too painful right now.
      */
    if( intersects( x0, y0, 0, 0, 60, 33, 36, 6 ) )
	return POINT_RIGHT;
    else if( intersects( x0, y0, 0, 0, 12, 33, 36, 6 ) )
	return POINT_LEFT;


    for( i = 0; i < 28; i++ ) {
	point_area( bd, i, &x, &y, &cx, &cy );

	x /= rdAppearance.nSize;
	y /= rdAppearance.nSize;
	cx /= rdAppearance.nSize;
	cy /= rdAppearance.nSize;
	
	if( intersects( x0, y0, 0, 0, x, y, cx, cy ) )
	    return i;
    }

    return -1;
}

static void update_match_id( BoardData *bd ) {
  
  int anScore[ 2 ];
  int fCubeOwner;

  anScore[ 0 ] = bd->score_opponent;
  anScore[ 1 ] = bd->score;

  if ( bd->can_double ) {
    if ( bd->opponent_can_double )
      fCubeOwner = -1;
    else
      fCubeOwner = 1;
  }
  else 
    fCubeOwner = 0;

  gtk_entry_set_text( GTK_ENTRY( bd->match_id ), 
                      MatchID( ( bd->turn == 1 ) ? bd->dice :
                               bd->dice_opponent,
                               ms.fTurn,
                               ms.fResigned,
                               ms.fDoubled,
                               ms.fMove,
                               fCubeOwner,
                               bd->crawford_game,
                               bd->match_to,
                               anScore,
                               bd->cube,
                               ms.gs ) );

}


static void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] ) {

    gtk_entry_set_text( GTK_ENTRY( bd->position_id ), PositionID( points ) );
}


static void
update_pipcount ( BoardData *bd, gint points[ 2 ][ 25 ] ) {

  int anPip[ 2 ];
  char *pc;
  int f;

  if ( fGUIShowPips ) {

    /* show pip count */

    PipCount ( points, anPip );

    f = ( bd->turn > 0 );
    
    pc = g_strdup_printf ( "%d (%+d)", anPip[ !f ], anPip[ !f ] - anPip[ f ] );
    gtk_label_set_text ( GTK_LABEL ( bd->pipcount0 ), pc );
    g_free ( pc );
    
    pc = g_strdup_printf ( "%d (%+d)", anPip[ f ], anPip[ f ] - anPip[ ! f ] );
    gtk_label_set_text ( GTK_LABEL ( bd->pipcount1 ), pc );
    g_free ( pc );

  }
  else {

    /* don't show pip count */

    gtk_label_set_text ( GTK_LABEL ( bd->pipcount0 ), _("n/a") );
    gtk_label_set_text ( GTK_LABEL ( bd->pipcount1 ), _("n/a") );

  }


}

/* A chequer has been moved or the board has been updated -- update the
   move and position ID labels. */
static int update_move( BoardData *bd ) {
    
    char *move = _("Illegal move"), move_buf[ 40 ];
    gint i, points[ 2 ][ 25 ];
    guchar key[ 10 ];
    int fIncomplete = TRUE, fIllegal = TRUE;
    
    read_board( bd, points );
    update_position_id( bd, points );
    update_pipcount ( bd, points );

    bd->valid_move = NULL;
    
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	bd->playing ) {
	move = _("(Editing)");
	fIncomplete = fIllegal = FALSE;
    } else if( EqualBoards( points, bd->old_board ) ) {
        /* no move has been made */
	move = NULL;
	fIncomplete = fIllegal = FALSE;
    } else {
        PositionKey( points, key );

        for( i = 0; i < bd->move_list.cMoves; i++ )
            if( EqualKeys( bd->move_list.amMoves[ i ].auch, key ) ) {
                bd->valid_move = bd->move_list.amMoves + i;
		fIncomplete = bd->valid_move->cMoves < bd->move_list.cMaxMoves
		    || bd->valid_move->cPips < bd->move_list.cMaxPips;
		fIllegal = FALSE;
                FormatMove( move = move_buf, bd->old_board,
			    bd->valid_move->anMove );
                break;
            }
    }

    gtk_widget_set_state( bd->move, fIncomplete ? GTK_STATE_ACTIVE :
			  GTK_STATE_NORMAL );
    gtk_label_set_text( GTK_LABEL( bd->move ), move );

    return fIllegal ? -1 : 0;
}

static void Confirm( BoardData *bd ) {

    char move[ 40 ];
    int points[ 2 ][ 25 ];
    
    read_board( bd, points );

    if( !bd->move_list.cMoves && EqualBoards( points, bd->old_board ) )
	UserCommand( "move" );
    else if( bd->valid_move &&
	     bd->valid_move->cMoves == bd->move_list.cMaxMoves &&
	     bd->valid_move->cPips == bd->move_list.cMaxPips ) {
        FormatMovePlain( move, bd->old_board, bd->valid_move->anMove );
    
        UserCommand( move );
    } else
        /* Illegal move */
	board_beep( bd );
}

static void board_start_drag( GtkWidget *widget, BoardData *bd,
			      int drag_point, int x, int y ) {

    bd->drag_point = drag_point;
    bd->drag_colour = bd->points[ drag_point ] < 0 ? -1 : 1;

    bd->points[ drag_point ] -= bd->drag_colour;

    board_invalidate_point( bd, drag_point );

#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( bd->drawing_area->window, FALSE );
#endif
    
    bd->x_drag = x;
    bd->y_drag = y;
}

/* CurrentPipCount: calculates pip counts for both players
   count for player 0 (colour == -1) is always in anPips[0],
   for player 1 (colour == 1) in anPips[1]
   assumes that a chequer has been picked up already from bd->drag_point,
   works on board representation in bd->points[] */
gboolean CurrentPipCount( BoardData *bd, int anPips[ 2 ] ) {

	int i;
	
	anPips[ 0 ] = 0;
	anPips[ 1 ] = 0;
	
	for( i = 1; i < 25; i++ ) {
		if ( bd->points[ 25 - i ] < 0 )
			anPips[ 0 ] -= bd->points[ 25 - i ] * i;
		if ( bd->points[ i ] > 0 )
			anPips[ 1 ] += bd->points[ i ] * i;
	}
	/* bars */
	anPips[ 1 ] += bd->points[ 25 ] * 25;
	anPips[ 0 ] -= bd->points[ 0 ] * 25;
	/* add count for missing chequer */
	if ( bd->drag_point != -1 ) {
		if ( bd->drag_colour < 0 )
			anPips[ 0 ] += 25 - bd->drag_point;
		else
			anPips[ 1 ] += bd->drag_point;
	}
	
	return 0;
}

/* SwapInts: swap two integer values */
gboolean SwapInts( int *val1, int *val2 ) {

	int tmp = *val1;
	*val1 = *val2;
	*val2 = tmp;

	return 0;
}

/* PointsAreEmpty: checks if there are chequers of player <iColour> between two points */
gboolean PointsAreEmpty( BoardData *bd, int iStartPoint, int iEndPoint, int iColour ) {

	int i;

	if ( iColour > 0 ) {
		if ( iStartPoint > iEndPoint )
			SwapInts( &iStartPoint, &iEndPoint );
		for ( i = iStartPoint; i <= iEndPoint; ++i )
			if ( bd->points[i] > 0 ) {
				return FALSE;
			}
	}
	else {
		if ( iStartPoint < iEndPoint )
			SwapInts( &iStartPoint, &iEndPoint );
		for ( i = iStartPoint; i >= iEndPoint; --i )
			if ( bd->points[i] < 0 ) {
				return FALSE;
			}
	}
	return TRUE;
}

/* LegalDestPoints: determine destination points for one chequer
   assumes that a chequer has been picked up already from bd->drag_point,
   works on board representation in bd->points[],
   returns TRUE if there are possible moves and fills iDestPoints[] with
   the destination points or -1 */
gboolean LegalDestPoints( BoardData *bd, int iDestPoints[4] ) {

	int i;
	int anPipsBeforeMove[ 2 ];
	int anCurPipCount[ 2 ];
	int iCanMove = 0;		/* bits set => could make a move with this die */
	int iDestCount = 0;
	int iDestPt = -1;
	int iDestLegal = TRUE;
	int bar = bd->drag_colour == bd->colour ? bd->bar : 25 - bd->bar; /* determine point number of bar */

	/* initialise */
	for (i = 0; i <= 3; ++i)
		iDestPoints[i] = -1;

	if ( ap[ bd->drag_colour == -1 ? 0 : 1 ].pt != PLAYER_HUMAN )
		return FALSE;

	/* pip count before move */
	PipCount( bd->old_board, anPipsBeforeMove );
	if ( bd->turn < 0 )
		SwapInts( &anPipsBeforeMove[ 0 ], &anPipsBeforeMove[ 1 ] );

	/* current pip count */
	CurrentPipCount( bd, anCurPipCount );

	if ( bd->dice[0] == bd->dice[1] ) {
	/* double roll: up to 4 possibilities to move, but only in multiples of dice[0] */
		for ( i = 0; i <= 3; ++i ) {
			if ( ( (i + 1) * bd->dice[0] > anCurPipCount[ bd->drag_colour == -1 ? 0 : 1 ] - anPipsBeforeMove[ bd->drag_colour == -1 ? 0 : 1 ] + bd->move_list.cMaxPips )	/* no moves left*/
			     || ( i && bd->points[ bar ] ) )		/* moving with chequer just entered not allowed if more chequers on the bar */
				break;
			iDestLegal = TRUE;
			if ( !i || iCanMove & ( 1 << ( i - 1 ) ) ) {
			/* only if moving first chequer or if last part-move succeeded */
				iDestPt = bd->drag_point - bd->dice[0] * ( i + 1 ) * bd->drag_colour;
				if( ( iDestPt <= 0 ) || ( iDestPt >= 25 ) ) {
				/* bearing off */
					/* all chequers in home board? */
					if ( bd->drag_colour > 0 ) {
						if ( ! PointsAreEmpty( bd, 7, 25, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
					else {
						if ( ! PointsAreEmpty( bd, 18, 0, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
					if ( iDestLegal && ( (iDestPt < 0 ) || (iDestPt > 25 ) ) ) {
					/* bearing off with roll bigger than point */
						if ( iCanMove ) {
						/* prevent bearoff in more than 1 move if there are still chequers on points bigger than destination of the part-move before */
							if ( bd->drag_colour > 0 ) {
								if ( ! PointsAreEmpty( bd, bd->drag_point - i * bd->dice[0] + 1, 6, bd->drag_colour ) ) {
									iDestPt = -1;
									iDestLegal = FALSE;
								}
							}
							else {
								if ( ! PointsAreEmpty( bd, bd->drag_point + i * bd->dice[0] - 1, 19, bd->drag_colour ) ) {
									iDestPt = -1;
									iDestLegal = FALSE;
								}
							}
						}
						/* chequers on higher points? */
						if ( bd->drag_colour > 0 ) {
							if ( ! PointsAreEmpty( bd, bd->drag_point + 1, 6, bd->drag_colour ) ) {
								iDestPt = -1;
								iDestLegal = FALSE;
							}
						}
						else {
							if ( ! PointsAreEmpty( bd, bd->drag_point - 1, 19, bd->drag_colour ) ) {
								iDestPt = -1;
								iDestLegal = FALSE;
							}
						}
					}
					if ( iDestLegal ) {
						iDestPt = bd->drag_colour > 0 ? 26 : 27;
					}
					else {
						iDestPt = -1;
					}
				}
			}
			else {
				iDestPt = -1;
				iDestLegal = FALSE;
				break;
			}

			/* check if destination (il)legal */
			if ( !iDestLegal
			    || ( iDestPt == -1 ) || ( iDestPt > 27 )			/* illegal points */
			    || ( bd->drag_colour > 0 ? bd->points[ iDestPt ] < -1
						   : bd->points[ iDestPt ] > 1 )		/* blocked by opponent*/
			    || ( iDestPt == bar )					/* bar */
			    || ( ( bd->drag_colour > 0 ? bd->points[ bar ] > 0
			    			       : bd->points[ bar ] < 0 )	/* when on bar ... */
				&& ( bd->drag_point != bar ) )				/* ... not playing from bar */
			   )
			{
				iDestPoints[ iDestCount ] = -1;
			}
			else {		/* legal move */
				iCanMove |= ( 1 << i );		/* set flag that this move could be made */
				iDestPoints[ iDestCount++ ] = iDestPt;
				iDestPt = -1;
			}
		}
	}
	else {
	/* normal roll: up to 3 possibilities */
		int iUnusedPips = anCurPipCount[ bd->drag_colour == -1 ? 0 : 1 ] - anPipsBeforeMove[ bd->drag_colour == -1 ? 0 : 1 ] + bd->move_list.cMaxPips;
		for ( i = 0; i <= 1; ++i ) {
			if ( ( iUnusedPips < bd->dice[i] ) ||		/* not possible to move with this die (anymore) */
			     ( ( bd->valid_move ) && ( bd->dice[i] == ( bd->valid_move->anMove[0] - bd->valid_move->anMove[1] ) ) ) ||		/* this die has been used already */
			     ( ( bd->valid_move ) && ( bd->valid_move->cMoves > 1 ) )		/* move already completed */ /* && ( bd->dice[i] != iUnusedPips ) && ( iUnusedPips != bd->move_list.cMaxPips ) */ )
				continue;
			iDestLegal = TRUE;
			iDestPt = bd->drag_point - bd->dice[i] * bd->drag_colour;
			if( ( iDestPt <= 0 ) || ( iDestPt >= 25 ) ) {
			/* bearing off */
				/* all chequers in home board? */
				if ( bd->drag_colour > 0 ) {
					if ( ! PointsAreEmpty( bd, 7, 25, bd->drag_colour ) ) {
						iDestPt = -1;
						iDestLegal = FALSE;
					}
				}
				else {
					if ( ! PointsAreEmpty( bd, 18, 0, bd->drag_colour ) ) {
						iDestPt = -1;
						iDestLegal = FALSE;
					}
				}
				if ( ( iDestLegal ) && ( (iDestPt < 0 ) || (iDestPt > 25 ) ) ) {
				/* bearing off with roll bigger than point */
					if ( bd->drag_colour > 0 ) {
						if ( ! PointsAreEmpty( bd, bd->drag_point + 1, 6, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
					else {
						if ( ! PointsAreEmpty( bd, bd->drag_point - 1, 19, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
				}
				if ( iDestLegal ) {
					iDestPt = bd->drag_colour > 0 ? 26 : 27;
				}
				else {
					iDestPt = -1;
				}
			}
			/* check if destination (il)legal */
			if ( !iDestLegal
			    || ( iDestPt == -1 ) || ( iDestPt > 27 )			/* illegal points */
			    || ( bd->drag_colour > 0 ? bd->points[ iDestPt ] < -1
						   : bd->points[ iDestPt ] > 1 )		/* blocked by opponent*/
			    || ( iDestPt == bar )					/* bar */
			    || ( ( bd->drag_colour > 0 ? bd->points[ bar ] > 0
			    			       : bd->points[ bar ] < 0 )	/* when on bar ... */
				&& ( bd->drag_point != bar ) )				/* ... not playing from bar */
			   )
			{
				iDestPoints[ iDestCount ] = -1;
			}
			else {		/* legal move */
				iCanMove |= ( 1 << i );		/* set flag that this move could be made */
				iDestPoints[ iDestCount++ ] = iDestPt;
				iDestPt = -1;
			}
		}
		/* check for moving twice with same chequer */
		if ( iCanMove &&				/* only if at least one first half-move could be made, */
		     ( bd->move_list.cMaxMoves > 1 ) &&		/* there is a legal move with 2 half-moves, */
		     ( anCurPipCount[ bd->drag_colour == -1 ? 0 : 1 ] == anPipsBeforeMove[ bd->drag_colour == -1 ? 0 : 1 ] ) &&		/* we didn't move yet, */
		     ( ! bd->points[ bar ] ) )			/* and don't have any more chequers on the bar */
		{
			iDestLegal = TRUE;
			iDestPt = bd->drag_point - ( bd->dice[0] + bd->dice[1] ) * bd->drag_colour;
			if( ( iDestPt <= 0 ) || ( iDestPt >= 25 ) ) {
			/* bearing off */
				/* all chequers in home board? */
				if ( bd->drag_colour > 0 ) {
					if ( ! PointsAreEmpty( bd, 7, 25, bd->drag_colour ) ) {
						iDestPt = -1;
						iDestLegal = FALSE;
					}
				}
				else {
					if ( ! PointsAreEmpty( bd, 18, 0, bd->drag_colour ) ) {
						iDestPt = -1;
						iDestLegal = FALSE;
					}
				}
				if ( iDestLegal && ( (iDestPt < 0 ) || (iDestPt > 25 ) ) ) {
				/* bearing off with roll bigger than point */
					/* prevent bearoff in more than 1 move if there are still chequers on points bigger than destination of first half-move */
					if ( bd->drag_colour > 0 ) {
						if ( ! PointsAreEmpty( bd, bd->drag_point - ( bd->dice[0] < bd->dice[1] ? bd->dice[0] : bd->dice[1] ) + 1, 6, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
					else {
						if ( ! PointsAreEmpty( bd, bd->drag_point + ( bd->dice[0] < bd->dice[1] ? bd->dice[0] : bd->dice[1] ) - 1, 19, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
				}
				if ( iDestLegal ) {
					iDestPt = bd->drag_colour > 0 ? 26 : 27;
				}
				else {
					iDestPt = -1;
				}
			}
	
			/* check if destination (il)legal */
			if ( !iDestLegal
			    || ( iDestPt == -1 ) || ( iDestPt > 27 )			/* illegal points */
			    || ( bd->drag_colour > 0 ? bd->points[ iDestPt ] < -1
						   : bd->points[ iDestPt ] > 1 )		/* blocked by opponent*/
			    || ( iDestPt == bar )					/* bar */
			    || ( ( bd->drag_colour > 0 ? bd->points[ bar ] > 0
			    			       : bd->points[ bar ] < 0 )	/* when on bar ... */
				&& ( bd->drag_point != bar ) )				/* ... not playing from bar */
			   )
			{
				iDestPoints[ iDestCount ] = -1;
			}
			else {		/* legal move */
				iDestPoints[ iDestCount++ ] = iDestPt;
				iDestPt = -1;
			}
		}
	}

	return iDestCount ? TRUE : FALSE;
}

static void board_drag( GtkWidget *widget, BoardData *bd, int x, int y ) {

    unsigned char *puch, *puchNew, *puchChequer;
    int s = rdAppearance.nSize;
    
#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( bd->drawing_area->window, FALSE );
#endif

    puch = g_alloca( 6 * s * 6 * s * 3 );
    puchNew = g_alloca( 6 * s * 6 * s * 3 );
    puchChequer = g_alloca( 6 * s * 6 * s * 3 );
    
    RenderArea( bd, puch, bd->x_drag - 3 * s, bd->y_drag - 3 * s,
		6 * s, 6 * s );
    RenderArea( bd, puchNew, x - 3 * s, y - 3 * s,
		6 * s, 6 * s );
    RefractBlendClip( puchChequer, 6 * s * 3, 0, 0, 6 * s, 6 * s, puchNew,
		      6 * s * 3, 0, 0,
		      bd->ri.achChequer[ bd->drag_colour > 0 ], 6 * s * 4, 0,
		      0, bd->ri.asRefract[ bd->drag_colour > 0 ], 6 * s, 6 * s,
		      6 * s );

    /* FIXME use dithalign */
#if USE_GTK2
    {
	GdkRegion *pr;
	GdkRectangle r;

	r.x = bd->x_drag - 3 * s;
	r.y = bd->y_drag - 3 * s;
	r.width = 6 * s;
	r.height = 6 * s;
	pr = gdk_region_rectangle( &r );
	
	r.x = x - 3 * s;
	r.y = y - 3 * s;
	gdk_region_union_with_rect( pr, &r );
	
	gdk_window_begin_paint_region( bd->drawing_area->window, pr );
	
	gdk_region_destroy( pr );
    }
#endif
    
    gdk_draw_rgb_image( bd->drawing_area->window, bd->gc_copy,
			bd->x_drag - 3 * s, bd->y_drag - 3 * s, 6 * s, 6 * s,
			GDK_RGB_DITHER_MAX, puch, 6 * s * 3 );
    gdk_draw_rgb_image( bd->drawing_area->window, bd->gc_copy,
			x - 3 * s, y - 3 * s, 6 * s, 6 * s,
			GDK_RGB_DITHER_MAX, puchChequer, 6 * s * 3 );
#if USE_GTK2
    gdk_window_end_paint( bd->drawing_area->window );
#endif
    
    bd->x_drag = x;
    bd->y_drag = y;
}

static void board_end_drag( GtkWidget *widget, BoardData *bd ) {
    
    unsigned char *puch;
    int s = rdAppearance.nSize;
    
#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( bd->drawing_area->window, FALSE );
#endif

    puch = g_alloca( 6 * s * 6 * s * 3 );
    
    RenderArea( bd, puch, bd->x_drag - 3 * s, bd->y_drag - 3 * s,
		6 * s, 6 * s );

    /* FIXME use dithalign */
    gdk_draw_rgb_image( bd->drawing_area->window, bd->gc_copy,
			bd->x_drag - 3 * s, bd->y_drag - 3 * s, 6 * s, 6 * s,
			GDK_RGB_DITHER_MAX, puch, 6 * s * 3 );
}

/* jsc: This is the code that was at the end of case
 * GDK_BUTTON_RELEASE: in board_pointer(), but was moved to this
 * procedure so that it could be called twice for trying high die
 * first then low die.  If illegal move, it no longer beeps, but
 * instead returns FALSE.
 *
 * Since this has side-effects, it should only be called from
 * GDK_BUTTON_RELEASE in board_pointer().  Mainly, it assumes
 * that a previous call to board_pointer() set up bd->drag_colour,
 * bd->drag_point, and also picked up a checker:
 *     bd->points[ bd->drag_point ] -= bd->drag_colour;
 *
 * If the move fails, that pickup will be reverted.
 */
static gboolean place_chequer_or_revert( GtkWidget *board, BoardData *bd, 
					 int dest ) {
    int bar, hit;
    gboolean placed = TRUE;
    int unhit;
    int oldpoints[ 28 ];

    bar = bd->drag_colour == bd->colour ? 25 - bd->bar : bd->bar;
    
    if( dest == -1 || ( bd->drag_colour > 0 ? bd->points[ dest ] < -1
			: bd->points[ dest ] > 1 ) || dest == bar ||
	dest > 27 ) {
	/* FIXME check move is legal */
	placed = FALSE;
	dest = bd->drag_point;
    } else if( dest >= 26 )
	/* bearing off */
	dest = bd->drag_colour > 0 ? 26 : 27;
		
    if( ( hit = bd->points[ dest ] == -bd->drag_colour ) ) {
	bd->points[ dest ] = 0;
	bd->points[ bar ] -= bd->drag_colour;
	
	board_invalidate_point( bd, bar );
    }

    /* 
     * Check for taking chequer off point where we hit 
     */

    /* check if the opponent had a chequer on the drag point (source),
       and that it's not pick'n'pass */

    /* FIXME: this does not detect undoing pick'n'pass */
    
    write_points( oldpoints, bd->turn, bd->nchequers, bd->old_board );

    if ( ( unhit = ( ( oldpoints[ bd->drag_point ] == -bd->drag_colour ) && 
                     ( dest < 26 ) &&
                     ( ( bd->drag_point - dest ) * bd->drag_colour < 0 ) ) ) ) {
      bd->points[ bar ] += bd->drag_colour;
      bd->points[ bd->drag_point ] -= bd->drag_colour;
      board_invalidate_point( bd, bar );
      board_invalidate_point( bd, bd->drag_point );

    }

          
    
    bd->points[ dest ] += bd->drag_colour;
    
    board_invalidate_point( bd, dest );
    
    if( bd->drag_point != dest ) {
	if( update_move( bd ) && !fGUIIllegal ) {
	    /* the move was illegal; undo it */
	    bd->points[ dest ] -= bd->drag_colour;
	    bd->points[ bd->drag_point ] += bd->drag_colour;
	    if( hit ) {
		bd->points[ bar ] += bd->drag_colour;
		bd->points[ dest ] = -bd->drag_colour;
		board_invalidate_point( bd, bar );
	    }

            if ( unhit ) {
              bd->points[ bar ] -= bd->drag_colour;
              bd->points[ bd->drag_point ] += bd->drag_colour;
              board_invalidate_point( bd, bar );
            }
              
		    
	    board_invalidate_point( bd, bd->drag_point );
	    board_invalidate_point( bd, dest );
	    update_move( bd );
	    placed = FALSE;
	}
    }

    return placed;
}

/* jsc: Special version :( of board_point which also allows clicking on a
   small border and all bearoff trays */
static int board_point_with_border( GtkWidget *board, BoardData *bd, 
				    int x0, int y0 ) {
    int i, x, y, cx, cy, xCube, yCube;

    x0 /= rdAppearance.nSize;
    y0 /= rdAppearance.nSize;
    
    /* Similar to board_point, but adds the nasty y-=3 cy+=3 border
       allowances */
    
    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;
    
    if( intersects( x0, y0, 0, 0, 60, 33, 36, 6 ) )
	return POINT_RIGHT;
    else if( intersects( x0, y0, 0, 0, 12, 33, 36, 6 ) )
	return POINT_LEFT;
    
    for( i = 0; i < 30; i++ ) {
	point_area( bd, i, &x, &y, &cx, &cy );

	x /= rdAppearance.nSize;
	y /= rdAppearance.nSize;
	cx /= rdAppearance.nSize;
	cy /= rdAppearance.nSize;

	if( y < 36 )
	    y -= 3;

	cy += 3;

	if( intersects( x0, y0, 0, 0, x, y, cx, cy ) )
	    return i;
    }

    cube_position( bd, &xCube, &yCube, NULL );
    
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_CUBE;
    
    resign_position( bd, &xCube, &yCube, NULL );
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_RESIGN;
    
    /* Could not find an overlapping point */
    return -1;
}

/* jsc: Given (x0,y0) which is ASSUMED to intersect point, return a
   non-negative integer i representing the ith chequer position which
   intersects (x0, y0).  On failure, return -1 */
static int board_chequer_number( GtkWidget *board, BoardData *bd, 
				 int point, int x0, int y0 ) {
    int i, x, y, cx, cy, dy, c_chequer;

    if( point < 0 || point > 27 )
	return -1;

    if( rdAppearance.nSize <= 0 )
	return -1;

    x0 /= rdAppearance.nSize;
    y0 /= rdAppearance.nSize;

    c_chequer = ( !point || point == 25 ) ? 3 : 5;

    x = positions[ fClockwise ][ point ][ 0 ]; 
    cx = 6;

    y = positions[ fClockwise ][ point ][ 1 ];
    dy = -positions[ fClockwise ][ point ][ 2 ];

    if( dy < 0 ) cy = -dy;
    else         cy = dy;

    /* FIXME: test for border overlap before for() loop to see if we
       should return 0, and return -1 if the for() loop completes */

    for( i = 1; i <= c_chequer; ++i ) {
	/* We use cx+1 and cy+1 because intersects may return 0 at boundary */
	if( intersects( x0, y0, 0, 0, 
			positions[ fClockwise ][ point ][ 0 ], y, 
			cx+1, cy+1 ) )
	    return i;
	
	y += dy;
    }

    /* Didn't intersect any position on the point */
    return 0;
}

/* jsc: Snowie style editing: we will try to place i chequers of the
   indicated colour on point n.  The x,y coordinates will be used to
   map to a point n and a checker position i.

   Clicking on a point occupied by the opposite color clears the point
   first.  If not enough chequers are available in the bearoff tray,
   we try to add what we can.  So if there are no chequers in the
   bearoff tray, no chequers will be added.  This may be a point of
   confusion during usage.  Clicking on the outside border of a point
   corresponds to i=0, i.e. remove all chequers from that point. */
static void board_quick_edit( GtkWidget *board, BoardData *bd, 
			      int x, int y, int colour, int dragging ) {

    int current, delta, c_chequer, avail;
    int off, opponent_off;
    int bar, opponent_bar;
    int n, i;
    int points[ 2 ][ 25 ];
    
    /* These should be asserted */
    if( board == NULL || bd == NULL || bd->points == NULL )
	return;

    /* Map (x,y) to a point from 0..27 using a version of
       board_point() that ignores the dice and cube and which allows
       clicking on a narrow border */
    n = board_point_with_border( board, bd, x, y );

    if( !dragging && ( n == 26 || n == 27 ) ) {
	/* click on bearoff tray in edit mode -- bear off all chequers */
	for( i = 0; i < 26; i++ ) {
	    bd->points[ i ] = 0;
	    board_invalidate_point( bd, i );
	}
	
	bd->points[ 26 ] = bd->nchequers;
	bd->points[ 27 ] = -bd->nchequers;
	board_invalidate_point( bd, 26 );
	board_invalidate_point( bd, 27 );

	read_board( bd, points );
	update_position_id( bd, points );
        update_pipcount ( bd, points );

	return;
    } else if ( !dragging && ( n == POINT_UNUSED0 || n == POINT_UNUSED1 ) ) {
	/* click on unused bearoff tray in edit mode -- reset to starting
	   position */

        int anBoard[ 2 ][ 25 ];

        InitBoard( anBoard, ms.bgv );
        write_board( bd, anBoard );
	
	for( i = 0; i < 28; i++ )
	    board_invalidate_point( bd, i );
	    
	read_board( bd, points );
	update_position_id( bd, points );
        update_pipcount ( bd, points );
    } else if( !dragging && n == POINT_CUBE ) {
	GTKSetCube( NULL, 0, NULL );
	return;
    } else if( !dragging && ( n == POINT_DICE || n == POINT_LEFT ||
			      n == POINT_RIGHT ) ) {
	GTKSetDice( NULL, 0, NULL );
	return;
    }
    
    /* Only points or bar allowed */
    if( n < 0 || n > 25 )
	return;

    /* Make sure that if we drag across a bar, we started on that bar.
       This is to make sure that if you drag a prime across say point
       4 to point 9, you don't inadvertently add chequers to the bar */
    if( dragging && ( n == 0 || n == 25 ) && n != bd->qedit_point )
	return;
    else
	bd->qedit_point = n;

    off          = (colour == 1) ? 26 : 27;
    opponent_off = (colour == 1) ? 27 : 26;

    bar          = (colour == 1) ? 25 : 0;
    opponent_bar = (colour == 1) ? 0 : 25;

    /* Can't add checkers to the wrong bar */
    if( n == opponent_bar )
	return;

    c_chequer = ( n == 0 || n == 25 ) ? 3 : 5;

    current = bd->points[ n ];
    
    /* Given n, map (x, y) to the ith checker position on point n*/
    i = board_chequer_number( board, bd, n, x, y );

    if( i < 0 )
	return;

    /* We are at the maximum position in the point, so we should not
       respond to dragging.  Each click will try to add one more
       chequer */
    if( !dragging && i == c_chequer && current*colour >= c_chequer )
	i = current*colour + 1;
    
    /* Clear chequers of the other colour from this point */
    if( current*colour < 0 ) {
	bd->points[ opponent_off ] += current;
	bd->points[ n ] = 0;
	current = 0;
	
	board_invalidate_point( bd, n );
	board_invalidate_point( bd, opponent_off );
    }
    
    delta = i*colour - current;
    
    /* No chequers of our colour added or removed */
    if( delta == 0 )
	return;

    /* g_assert( bd->points[ off ]*colour >= 0 ); */

    if( delta*colour < 0 ) {
	/* Need to remove some chequers of the same colour */
	bd->points[ off ] -= delta;
	bd->points[ n ] += delta;
    } else if( ( avail = bd->points[ off ] ) == 0 ) {
	/* No more possible updates since not enough free chequers */
	return;
    } else if( delta*colour > avail*colour ) {
	/* Not enough free chequers in bearoff, so move all of them */
	bd->points[ off ] -= avail;
	bd->points[ n ] += avail;
	/* g_assert( bd->points[ off ] == 0 ); */
    } else {
	/* Have enough free chequers, move needed number to point n */
	bd->points[ off ] -= delta;
	bd->points[ n ] += delta;
	/* g_assert( bd->points[ off ]*colour >= 0 ); */
    }

    board_invalidate_point( bd, n );
    board_invalidate_point( bd, off );

    read_board( bd, points );
    update_position_id( bd, points );
    update_pipcount ( bd, points );
}


static int
ForcedMove ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] ) {

  movelist ml;

  GenerateMoves ( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );

  if ( ml.cMoves == 1 ) {

    ApplyMove ( anBoard, ml.amMoves[ 0 ].anMove, TRUE );
    return TRUE;
     
  }
  else
    return FALSE;

}

static int
GreadyBearoff ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] ) {

  movelist ml;
  int i, iMove, cMoves;
  
  /* check for all chequers inside home quadrant */

  for ( i = 6; i < 25; ++i )
    if ( anBoard[ 1 ][ i ] )
      return FALSE;

  cMoves = ( anDice[ 0 ] == anDice[ 1 ] ) ? 4 : 2;

  GenerateMoves( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );

  for( i = 0; i < ml.cMoves; i++ )
    for( iMove = 0; iMove < cMoves; iMove++ )
      if( ( ml.amMoves[ i ].anMove[ iMove << 1 ] < 0 ) ||
          ( ml.amMoves[ i ].anMove[ ( iMove << 1 ) + 1 ] != -1 ) )
        /* not a bearoff move */
        break;
      else if( iMove == cMoves - 1 ) {
        /* All dice bear off */
        ApplyMove ( anBoard, ml.amMoves[ i ].anMove, TRUE );
        return TRUE;
      }

  return FALSE;

}




static gboolean board_pointer( GtkWidget *board, GdkEvent *event,
			       BoardData *bd ) {
    int i, n, dest, x, y, bar;

    switch( event->type ) {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
	x = event->button.x;
	y = event->button.y;
	break;

    case GDK_MOTION_NOTIFY:
	if( event->motion.is_hint ) {
	    if( gdk_window_get_pointer( board->window, &x, &y, NULL ) !=
		board->window )
		return TRUE;	    
	} else {
	    x = event->motion.x;
	    y = event->motion.y;
	}
	break;

    default:
	return TRUE;
    }
    
    switch( event->type ) {
    case GDK_BUTTON_PRESS:

	if( bd->drag_point >= 0 )
	    /* Ignore subsequent button presses once we're dragging. */
	    break;
	
	/* We don't need the focus ourselves, but we want to steal it
	   from any other widgets (e.g. the player name entry fields)
	   so that those other widgets won't intercept global
	   accelerator keys. */
	gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( board ) ),
			      NULL );
	
	bd->click_time = gdk_event_get_time( event );
	bd->drag_button = event->button.button;
	
	/* jsc: In editing mode, clicking on a point or a bar. */
	if( bd->playing &&
	    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    !( event->button.state & GDK_CONTROL_MASK ) ) {
	    int dragging = 0;
	    int colour = bd->drag_button == 1 ? 1 : -1;
	    
	    board_quick_edit( board, bd, x, y, colour, dragging );
	    
	    bd->drag_point = -1;
	    return TRUE;
	}
	
	if( ( bd->drag_point = board_point( board, bd, x, y ) ) < 0 ) {
	    /* Click on illegal area. */
	    board_beep( bd );
	    
	    return TRUE;
	}

	if( bd->drag_point == POINT_CUBE ) {
	    /* Clicked on cube; double. */
	    bd->drag_point = -1;
	    
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
		GTKSetCube( NULL, 0, NULL );
	    else if ( bd->doubled )
                UserCommand ( "take" );
            else
		UserCommand( "double" );
	    
	    return TRUE;
	}

        if( bd->drag_point == POINT_RESIGN ) {
          /* clicked on resignation symbol */
          bd->drag_point = -1;
          if ( bd->resigned && 
               ! gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
            UserCommand ( "accept" );

          return TRUE;

        }

	if( bd->drag_point == POINT_DICE ) {
	    /* Clicked on dice; end move. */
	    bd->drag_point = -1;
	    
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
		GTKSetDice( NULL, 0, NULL );
	    else if( event->button.button == 1 )
		Confirm( bd );
	    else {
		/* Other buttons on dice swaps positions. */
		n = bd->dice[ 0 ];
		bd->dice[ 0 ] = bd->dice[ 1 ];
		bd->dice[ 1 ] = n;
		
		n = bd->dice_opponent[ 0 ];
		bd->dice_opponent[ 0 ] = bd->dice_opponent[ 1 ];
		bd->dice_opponent[ 1 ] = n;

		board_invalidate_dice( bd );
	    }
	    
	    return TRUE;
	}

	/* If playing and dice not rolled yet, this code handles
	   rolling the dice if bottom player clicks the right side of
	   the board, or the top player clicks the left side of the
	   board (his/her right side).  In edit mode, set the roll
	   instead. */
	if( bd->playing && bd->dice[ 0 ] <= 0 &&
	    ( ( bd->drag_point == POINT_RIGHT && bd->turn == 1 ) ||
	      ( bd->drag_point == POINT_LEFT  && bd->turn == -1 ) ) ) {
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
		GTKSetDice( NULL, 0, NULL );
	    else {
		/* NB: the UserCommand() call may cause reentrancies,
		   so it is vital to reset bd->drag_point first! */
		bd->drag_point = -1;
		UserCommand( "roll" );
	    }
	    
	    return TRUE;
	}
	
	/* jsc: since we added new POINT_RIGHT and POINT_LEFT, we want
	   to make sure no ghost checkers can be dragged out of this *
	   region!  */
	if( ( bd->drag_point == POINT_RIGHT || 
	      bd->drag_point == POINT_LEFT ) ) {
	    bd->drag_point = -1;
	    board_beep( bd );
	    return TRUE;
	}

	if( !bd->playing || ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
	    bd->edit ) ) && bd->dice[ 0 ] <= 0 ) ) {
	    /* Don't let them move chequers unless the dice have been
	       rolled, or they're editing the board. */
	    board_beep( bd );
	    bd->drag_point = -1;
	    
	    return TRUE;
	}
	
	/* if the dice are set, and nDragPoint is 26 or 27 (i.e. off),
	   then bear off as many chequers as possible, and return. */

        if( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
            bd->drag_point == ( ( 53 - bd->turn ) / 2 ) &&
            bd->dice[ 0 ] ) {

          /* user clicked on bear-off tray: try to bear-off chequers or
             show forced move */

          int anBoard[ 2 ][ 25 ];
          
          memcpy ( anBoard, ms.anBoard, sizeof anBoard );

          bd->drag_colour = bd->turn;
          bd->drag_point = -1;
          
          if ( ForcedMove ( anBoard, bd->dice ) ||
               GreadyBearoff ( anBoard, bd->dice ) ) {

            /* we've found a move: update board  */

            int old_points[ 28 ];
            int i, j;
            int an[ 28 ];

	    memcpy( old_points, bd->points, sizeof old_points );

            write_board ( bd, anBoard );

            for ( i = 0, j = 0; i < 28; ++i )
              if ( old_points[ i ] != bd->points[ i ] )
                an[ j++ ] = i;

            if ( !update_move ( bd ) ) {

              /* redraw points */

              for ( i = 0; i < j; ++i )
                board_invalidate_point( bd, an[ i ] );

            }
            else {

              /* whoops: this should not happen as ForcedMove and GreadyBearoff
                 only returns legal moves */

              assert ( FALSE );

            }
            
          }

          return TRUE;

        }
	
	if( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    bd->drag_point > 0 && bd->drag_point <= 24 &&
	    ( !bd->points[ bd->drag_point ] ||
	    bd->points[ bd->drag_point ] == -bd->turn ) ) {
	    /* Click on an empty point or opponent blot; try to make the
	       point. */
	    int n[ 2 ];
	    int old_points[ 28 ], points[ 2 ][ 25 ];
	    unsigned char key[ 10 ];
	    
	    memcpy( old_points, bd->points, sizeof old_points );
	    
	    bd->drag_colour = bd->turn;
	    bar = bd->drag_colour == bd->colour ? 25 - bd->bar : bd->bar;
	    
	    if( bd->dice[ 0 ] == bd->dice[ 1 ] ) {
		/* Rolled a double; find the two closest chequers to make
		   the point. */
		int c = 0;

		n[ 0 ] = n[ 1 ] = -1;
		
		for( i = 0; i < 4 && c < 2; i++ ) {
		    int j = bd->drag_point + bd->dice[ 0 ] * bd->drag_colour *
			( i + 1 );

		    if( j < 0 || j > 25 )
			break;

		    while( c < 2 && bd->points[ j ] * bd->drag_colour > 0 ) {
			/* temporarily take chequer, so it's not used again */
			bd->points[ j ] -= bd->drag_colour;
			n[ c++ ] = j;
		    }
		}
		
		/* replace chequers removed above */
		for( i = 0; i < c; i++ )
		    bd->points[ n[ i ] ] += bd->drag_colour;
	    } else {
		/* Rolled a non-double; take one chequer from each point
		   indicated by the dice. */
		n[ 0 ] = bd->drag_point + bd->dice[ 0 ] * bd->drag_colour;
		n[ 1 ] = bd->drag_point + bd->dice[ 1 ] * bd->drag_colour;
	    }
	    
	    if( n[ 0 ] >= 0 && n[ 0 ] <= 25 && n[ 1 ] >= 0 && n[ 1 ] <= 25 &&
		bd->points[ n[ 0 ] ] * bd->drag_colour > 0 &&
		bd->points[ n[ 1 ] ] * bd->drag_colour > 0 ) {
		/* the point can be made */
		if( bd->points[ bd->drag_point ] )
		    /* hitting the opponent in the process */
		    bd->points[ bar ] -= bd->drag_colour;
		
		bd->points[ n[ 0 ] ] -= bd->drag_colour;
		bd->points[ n[ 1 ] ] -= bd->drag_colour;
		bd->points[ bd->drag_point ] = bd->drag_colour << 1;
		
		read_board( bd, points );
		PositionKey( points, key );

		if( !update_move( bd ) ) {
		    board_invalidate_point( bd, n[ 0 ] );
		    board_invalidate_point( bd, n[ 1 ] );
		    board_invalidate_point( bd, bd->drag_point );
		    board_invalidate_point( bd, bar );
			
		    playSound( SOUND_CHEQUER );

		    goto finished;
		}

		/* the move to make the point wasn't legal; undo it. */
		memcpy( bd->points, old_points, sizeof bd->points );
		update_move( bd );
	    }
	    
	    board_beep( bd );
	    
	finished:
	    bd->drag_point = -1;
	    return TRUE;
	}

	if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    event->button.button != 1 ) {
	    /* Button 2 or more in edit mode; modify point directly. */
	    if( bd->drag_button == 2 ) {
		if( bd->points[ bd->drag_point ] >= 0 ) {
		    /* Add player 1 */
		    bd->drag_colour = 1;
		    dest = bd->drag_point;
		    bd->drag_point = 26;
		} else {
		    /* Remove player -1 */
		    bd->drag_colour = -1;
		    dest = 27;
		}
	    } else {
		if( bd->points[ bd->drag_point ] <= 0 ) {
		    /* Add player -1 */
		    bd->drag_colour = -1;
		    dest = bd->drag_point;
		    bd->drag_point = 27;
		} else {
		    /* Remove player 1 */
		    bd->drag_colour = 1;
		    dest = 26;
		}
	    }

	    if( bd->points[ bd->drag_point ] * bd->drag_colour < 1 ||
		bd->drag_point == dest ||
		( bd->drag_colour == 1 && ( !bd->drag_point ||
					    !dest ||
					    bd->drag_point == 27 ||
					    dest == 27 ) ) ||
		( bd->drag_colour == -1 && ( bd->drag_point == 25 ||
					     dest == 25 ||
					     bd->drag_point == 26 ||
					     dest == 26 ) ) )
		board_beep( bd );
	    else {
		int points[ 2 ][ 25 ];
		
		bd->points[ bd->drag_point ] -= bd->drag_colour;
		bd->points[ dest ] += bd->drag_colour;

		board_invalidate_point( bd, bd->drag_point );
		board_invalidate_point( bd, dest );
		
		read_board( bd, points );
		update_position_id( bd, points );
                update_pipcount ( bd, points );
	    }
		
	    bd->drag_point = -1;
	    return TRUE;
	}

	if( !bd->points[ bd->drag_point ] ) {
	    /* click on empty bearoff tray */
	    board_beep( bd );
	    
	    bd->drag_point = -1;
	    return TRUE;
	}
	
	bd->drag_colour = bd->points[ bd->drag_point ] < 0 ? -1 : 1;

	if( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    bd->drag_point && bd->drag_point != 25 &&
	    bd->drag_colour != bd->turn ) {
	    /* trying to move opponent's chequer (except off the bar, which
	       is OK) */
	    board_beep( bd );
	    
	    bd->drag_point = -1;
	    return TRUE;
	}

	board_start_drag( board, bd, bd->drag_point, x, y );
	
	/* fall through */
	
    case GDK_MOTION_NOTIFY:
	/* jsc: In quick editing mode, dragging across point, but not a bar. */
	if( bd->playing && bd->drag_point < 0 &&
	    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    !( event->button.state & GDK_CONTROL_MASK ) ) {
	    int dragging = 1;
	    int colour = bd->drag_button == 1 ? 1 : -1;
	    
	    board_quick_edit( board, bd, x, y, colour, dragging );
	    
	    bd->drag_point = -1;
	    return TRUE;
	}
	
	if( bd->drag_point < 0 )
	    break;

	if ( fGUIDragTargetHelp ) {
		gint iDestPoints[4];
		gint i, ptx, pty, ptcx, ptcy;
		GdkColor *TargetHelpColor;
		
		if ( ( ap[ bd->drag_colour == -1 ? 0 : 1 ].pt == PLAYER_HUMAN )		/* not for computer turn */
			&& ( bd->drag_point != board_point( board, bd, x, y ) )		/* dragged some distance */
			&& LegalDestPoints( bd, iDestPoints ) )				/* can move */
		{
			TargetHelpColor = (GdkColor *) malloc( sizeof(GdkColor) );
			/* values of RGB components within GdkColor are
			* taken from 0 to 65535, not 0 to 255. */
			TargetHelpColor->red   =    0 * ( 65535 / 255 );
			TargetHelpColor->green =  255 * ( 65535 / 255 );
			TargetHelpColor->blue  =    0 * ( 65535 / 255 );
			TargetHelpColor->pixel = (gulong)( TargetHelpColor->red * 65536 +
							   TargetHelpColor->green * 256 +
							   TargetHelpColor->blue );
			/* get the closest color available in the colormap if no 24-bit*/
			gdk_color_alloc(gtk_widget_get_colormap( board ), TargetHelpColor);
			gdk_gc_set_foreground(bd->gc_copy, TargetHelpColor);
	
			/* draw help rectangles around target points */
			for ( i = 0; i <= 3; ++i ) {
				if ( iDestPoints[i] != -1 ) {
					/* calculate region coordinates for point */
					point_area( bd, iDestPoints[i], &ptx, &pty, &ptcx, &ptcy );
					gdk_draw_rectangle( board->window, bd->gc_copy, FALSE, ptx + 1, pty + 1, ptcx - 2, ptcy - 2 );
				}
			}
	
			free( TargetHelpColor );
		}
	}

	board_drag( board, bd, x, y );
	
	break;
	
    case GDK_BUTTON_RELEASE:
	if( bd->drag_point < 0 )
	    break;
	
	board_end_drag( board, bd );

	/* undo drag target help */
	if ( fGUIDragTargetHelp ) {
		int i;
		gint iDestPoints[4];
		
		if ( LegalDestPoints( bd, iDestPoints ) )
			for ( i = 0; i <= 3; ++i ) {
				if ( iDestPoints[i] != -1 )
					board_invalidate_point( bd, iDestPoints[i] );
			}
	}
	
	dest = board_point( board, bd, x, y );

	if( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	    dest == bd->drag_point && gdk_event_get_time( event ) -
	    bd->click_time < CLICK_TIME ) {
	    /* Automatically place chequer on destination point
	       (as opposed to a full drag). */

	    if( bd->drag_colour != bd->turn ) {
		/* can't move the opponent's chequers */
		board_beep( bd );

		dest = bd->drag_point;

    		place_chequer_or_revert( board, bd, dest );		
	    } else {
		gint left = bd->dice[0];
		gint right = bd->dice[1];

		/* Button 1 tries the left roll first.
		   other buttons try the right dice roll first */
		dest = bd->drag_point - ( bd->drag_button == 1 ? 
					  left :
					  right ) * bd->drag_colour;

		if( ( dest <= 0 ) || ( dest >= 25 ) )
		    /* bearing off */
		    dest = bd->drag_colour > 0 ? 26 : 27;
		
		if( place_chequer_or_revert( board, bd, dest ) )
		    playSound( SOUND_CHEQUER );
		else {
		    /* First roll was illegal.  We are going to 
		       try the second roll next. First, we have 
		       to redo the pickup since we got reverted. */
		    bd->points[ bd->drag_point ] -= bd->drag_colour;
		    board_invalidate_point( bd, bd->drag_point );
		    
		    /* Now we try the other die roll. */
		    dest = bd->drag_point - ( bd->drag_button == 1 ?
					      right :
					      left ) * bd->drag_colour;
		    
		    if( ( dest <= 0 ) || ( dest >= 25 ) )
			/* bearing off */
			dest = bd->drag_colour > 0 ? 26 : 27;
		    
		    if( place_chequer_or_revert( board, bd, dest ) )
			playSound( SOUND_CHEQUER );
		    else {
			board_invalidate_point( bd, bd->drag_point );
			board_beep( bd );
		    }
		}
	    }
	} else {
	    /* This is from a normal drag release */
	    if( place_chequer_or_revert( board, bd, dest ) )
		playSound( SOUND_CHEQUER );
	    else
		board_beep( bd );
	}
	
	bd->drag_point = -1;

	break;
	
    default:
	g_assert_not_reached();
    }

    return TRUE;
}

static gint board_set( Board *board, const gchar *board_text,
                       const gint resigned, const gint cube_use ) {

    BoardData *bd = board->board_data;
    gchar *dest, buf[ 32 ];
    gint i, *pn, **ppn;
    gint old_board[ 28 ];
    int old_cube, old_doubled, old_crawford, old_xCube, old_yCube, editing;
    int old_resigned;
    int old_xResign, old_yResign;
    GtkAdjustment *padj0, *padj1;
    
#if __GNUC__
    int *match_settings[] = { &bd->match_to, &bd->score,
			      &bd->score_opponent };
    int *game_settings[] = { &bd->turn, bd->dice, bd->dice + 1,
		       bd->dice_opponent, bd->dice_opponent + 1,
		       &bd->cube, &bd->can_double, &bd->opponent_can_double,
		       &bd->doubled, &bd->colour, &bd->direction,
		       &bd->home, &bd->bar, &bd->off, &bd->off_opponent,
		       &bd->on_bar, &bd->on_bar_opponent, &bd->to_move,
		       &bd->forced, &bd->crawford_game, &bd->redoubles };
    int old_dice[] = { bd->dice[ 0 ], bd->dice[ 1 ],
			bd->dice_opponent[ 0 ], bd->dice_opponent[ 1 ] };
#else
    int *match_settings[ 3 ], *game_settings[ 21 ], old_dice[ 4 ];

    match_settings[ 0 ] = &bd->match_to;
    match_settings[ 1 ] = &bd->score;
    match_settings[ 2 ] = &bd->score_opponent;

    game_settings[ 0 ] = &bd->turn;
    game_settings[ 1 ] = bd->dice;
    game_settings[ 2 ] = bd->dice + 1;
    game_settings[ 3 ] = bd->dice_opponent;
    game_settings[ 4 ] = bd->dice_opponent + 1;
    game_settings[ 5 ] = &bd->cube;
    game_settings[ 6 ] = &bd->can_double;
    game_settings[ 7 ] = &bd->opponent_can_double;
    game_settings[ 8 ] = &bd->doubled;
    game_settings[ 9 ] = &bd->colour;
    game_settings[ 10 ] = &bd->direction;
    game_settings[ 11 ] = &bd->home;
    game_settings[ 12 ] = &bd->bar;
    game_settings[ 13 ] = &bd->off;
    game_settings[ 14 ] = &bd->off_opponent;
    game_settings[ 15 ] = &bd->on_bar;
    game_settings[ 16 ] = &bd->on_bar_opponent;
    game_settings[ 17 ] = &bd->to_move;
    game_settings[ 18 ] = &bd->forced;
    game_settings[ 19 ] = &bd->crawford_game;
    game_settings[ 20 ] = &bd->redoubles;

    old_dice[ 0 ] = bd->dice[ 0 ];
    old_dice[ 1 ] = bd->dice[ 1 ];
    old_dice[ 2 ] = bd->dice_opponent[ 0 ];
    old_dice[ 3 ] = bd->dice_opponent[ 1 ];
#endif
    
    editing = bd->playing &&
	gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) );

    if( strncmp( board_text, "board:", 6 ) )
	return -1;
    
    board_text += 6;

    for( dest = bd->name, i = 31; i && *board_text && *board_text != ':';
	 i-- )
	*dest++ = *board_text++;

    *dest = 0;

    if( !board_text )
	return -1;

    board_text++;
    
    for( dest = bd->name_opponent, i = 31;
	 i && *board_text && *board_text != ':'; i-- )
	*dest++ = *board_text++;

    *dest = 0;

    if( !board_text )
	return -1;

    for( i = 3, ppn = match_settings; i--; ) {
	if( *board_text++ != ':' ) /* FIXME should check end of string */
	    return -1;

	**ppn++ = strtol( board_text, (char **) &board_text, 10 );
    }

    for( i = 0, pn = bd->points; i < 26; i++ ) {
	old_board[ i ] = *pn;
	if( *board_text++ != ':' )
	    return -1;

	*pn++ = strtol( board_text, (char **) &board_text, 10 );
    }

    old_board[ 26 ] = bd->points[ 26 ];
    old_board[ 27 ] = bd->points[ 27 ];

    /* calculate number of chequers */
    
    bd->nchequers = 0;
    for ( i = 0; i < 28; ++i )
      if ( bd->points[ i ] > 0 )
        bd->nchequers += bd->points[ i ];
    


    old_cube = bd->cube;
    old_doubled = bd->doubled;
    old_crawford = bd->crawford_game;
    old_resigned = bd->resigned;

    cube_position( bd, &old_xCube, &old_yCube, NULL );

    resign_position( bd, &old_xResign, &old_yResign, NULL );
    bd->resigned = resigned;
    
    for( i = 21, ppn = game_settings; i--; ) {
	if( *board_text++ != ':' )
	    return -1;

	**ppn++ = strtol( board_text, (char **) &board_text, 10 );
    }

    if( bd->colour < 0 )
	bd->off = -bd->off;
    else
	bd->off_opponent = -bd->off_opponent;
    
    if( bd->direction < 0 ) {
	bd->points[ 26 ] = bd->off;
	bd->points[ 27 ] = bd->off_opponent;
    } else {
	bd->points[ 26 ] = bd->off_opponent;
	bd->points[ 27 ] = bd->off;
    }

    if( !editing ) {
	gtk_entry_set_text( GTK_ENTRY( bd->name0 ), bd->name_opponent );
	gtk_entry_set_text( GTK_ENTRY( bd->name1 ), bd->name );
	gtk_label_set_text( GTK_LABEL( bd->lname0 ), bd->name_opponent );
	gtk_label_set_text( GTK_LABEL( bd->lname1 ), bd->name );
    
	if( bd->match_to ) {
	    sprintf( buf, "%d", bd->match_to );
	    gtk_label_set_text( GTK_LABEL( bd->match ), buf );
	} else
	    gtk_label_set_text( GTK_LABEL( bd->match ), _("unlimited") );

	padj0 = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( bd->score0 ) );
	padj1 = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( bd->score1 ) );
	padj0->upper = padj1->upper = bd->match_to ? bd->match_to : 65535;
	gtk_adjustment_changed( padj0 );
	gtk_adjustment_changed( padj1 );
	
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score0 ),
				   bd->score_opponent );
	gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score1 ),
				   bd->score );

        if ( bd->match_to ) {

          if ( bd->score_opponent >= bd->match_to )
            sprintf( buf, "%d (won match)", bd->score_opponent );
          else
            sprintf( buf, "%d (%d-away)", bd->score_opponent,
                     bd->match_to - bd->score_opponent );
          gtk_label_set_text( GTK_LABEL( bd->lscore0 ), buf );

          if ( bd->score >= bd->match_to )
            sprintf( buf, "%d (won match)", bd->score );
          else
            sprintf( buf, "%d (%d-away)", bd->score,
                     bd->match_to - bd->score );
          gtk_label_set_text( GTK_LABEL( bd->lscore1 ), buf );

        }
        else {

          /* money game */

          sprintf( buf, "%d", bd->score_opponent );
          gtk_label_set_text( GTK_LABEL( bd->lscore0 ), buf );
          sprintf( buf, "%d", bd->score );
          gtk_label_set_text( GTK_LABEL( bd->lscore1 ), buf );

        }

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->crawford ),
				      bd->crawford_game );
	gtk_widget_set_sensitive( bd->crawford, bd->match_to > 1 &&
				  ( ( bd->score == bd->match_to - 1 ) ^
				    ( bd->score_opponent == bd->match_to - 1 ) ));

	read_board( bd, bd->old_board );
	update_position_id( bd, bd->old_board );
        update_pipcount ( bd, bd->old_board );
    }
    
    update_match_id ( bd );
    update_move( bd );

    if( bd->dice[ 0 ] || bd->dice_opponent[ 0 ] ) {
	GenerateMoves( &bd->move_list, bd->old_board,
		       bd->dice[ 0 ] | bd->dice_opponent[ 0 ],
		       bd->dice[ 1 ] | bd->dice_opponent[ 1 ], TRUE );

	/* bd->move_list contains pointers to static data, so we need to
	   copy the actual moves into private storage. */
	if( bd->all_moves )
	    free( bd->all_moves );
	bd->all_moves = malloc( bd->move_list.cMoves * sizeof( move ) );
	bd->move_list.amMoves = memcpy( bd->all_moves, bd->move_list.amMoves,
				 bd->move_list.cMoves * sizeof( move ) );
	bd->valid_move = NULL;
    }
	
    if( fGUIHighDieFirst ) {
	if( bd->dice[ 0 ] < bd->dice[ 1 ] )
	    swap( bd->dice, bd->dice + 1 );
	
	if( bd->dice_opponent[ 0 ] < bd->dice_opponent[ 1 ] )
	    swap( bd->dice_opponent, bd->dice_opponent + 1 );
    }
    
    if( bd->dice[ 0 ] != old_dice[ 0 ] ||
	bd->dice[ 1 ] != old_dice[ 1 ] ||
	bd->dice_opponent[ 0 ] != old_dice[ 2 ] ||
	bd->dice_opponent[ 1 ] != old_dice[ 3 ] ) {
	if( bd->x_dice[ 0 ] > 0 ) {
	    /* dice were visible before; now they're not */
	    int ax[ 2 ] = { bd->x_dice[ 0 ], bd->x_dice[ 1 ] };
	    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;
            if ( rdAppearance.nSize > 0 ) {
              board_invalidate_rect( bd->drawing_area,
                                     ax[ 0 ] * rdAppearance.nSize,
                                     bd->y_dice[ 0 ] * rdAppearance.nSize,
                                     7 * rdAppearance.nSize, 
                                     7 * rdAppearance.nSize, bd );
              board_invalidate_rect( bd->drawing_area,
                                     ax[ 1 ] * rdAppearance.nSize,
                                     bd->y_dice[ 1 ] * rdAppearance.nSize,
                                     7 * rdAppearance.nSize, 
                                     7 * rdAppearance.nSize, bd );
            }
	}

	if( ( bd->turn == bd->colour ? bd->dice[ 0 ] :
	       bd->dice_opponent[ 0 ] ) <= 0 )
	    /* dice have not been rolled */
	    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;
	else {
	    /* FIXME different dice for first turn */
	    int iAttempt = 0, iPoint, x, y, cx, cy;

	cocked:
	    bd->x_dice[ 0 ] = RAND % 21 + 13;
	    bd->x_dice[ 1 ] = RAND % ( 34 - bd->x_dice[ 0 ] ) +
		bd->x_dice[ 0 ] + 8;
	    
	    if( bd->colour == bd->turn ) {
		bd->x_dice[ 0 ] += 48;
		bd->x_dice[ 1 ] += 48;
	    }
	    
	    bd->y_dice[ 0 ] = RAND % 10 + 28;
	    bd->y_dice[ 1 ] = RAND % 10 + 28;

	    if( rdAppearance.nSize > 0 ) {
		for( iPoint = 1; iPoint <= 24; iPoint++ )
		    if( abs( bd->points[ iPoint ] ) >= 5 ) {
			point_area( bd, iPoint, &x, &y, &cx, &cy );
			x /= rdAppearance.nSize;
			y /= rdAppearance.nSize;
			cx /= rdAppearance.nSize;
			cy /= rdAppearance.nSize;
			
			if( ( intersects( bd->x_dice[ 0 ], bd->y_dice[ 0 ],
					  7, 7, x, y, cx, cy ) ||
			      intersects( bd->x_dice[ 1 ], bd->y_dice[ 1 ],
					  7, 7, x, y, cx, cy ) ) &&
			    iAttempt++ < 0x80 )
			    goto cocked;
		    }
	    }
	}
    }

    if( rdAppearance.nSize <= 0 )
	return 0;

    if( bd->doubled != old_doubled || 
        bd->cube != old_cube ||
	bd->cube_owner != bd->opponent_can_double - bd->can_double ||
	cube_use != bd->cube_use || 
        bd->crawford_game != old_crawford ) {
	int xCube, yCube;

	bd->cube_owner = bd->opponent_can_double - bd->can_double;
	bd->cube_use = cube_use;
		
	/* erase old cube */
	board_invalidate_rect( bd->drawing_area, old_xCube * rdAppearance.nSize,
			       old_yCube * rdAppearance.nSize, 8 * rdAppearance.nSize,
			       8 * rdAppearance.nSize, bd );
	
	cube_position( bd, &xCube, &yCube, NULL );
	/* draw new cube */
	board_invalidate_rect( bd->drawing_area, xCube * rdAppearance.nSize,
			       yCube * rdAppearance.nSize, 8 * rdAppearance.nSize,
			       8 * rdAppearance.nSize, bd );
    }

    if ( bd->resigned != old_resigned ) {
	int xResign, yResign;

	/* erase old resign */
	board_invalidate_rect( bd->drawing_area, 
                               old_xResign * rdAppearance.nSize,
			       old_yResign * rdAppearance.nSize, 
                               8 * rdAppearance.nSize,
			       8 * rdAppearance.nSize, bd );
	
	resign_position( bd, &xResign, &yResign, NULL );
	/* draw new resign */
	board_invalidate_rect( bd->drawing_area, 
                               xResign * rdAppearance.nSize,
			       yResign * rdAppearance.nSize, 
                               8 * rdAppearance.nSize,
			       8 * rdAppearance.nSize, bd );

    }

    if( fClockwise != rdAppearance.fClockwise ) {
	/* This is complete overkill, but we need to recalculate the
	   board pixmap if the points are numbered and the direction
	   changes. */
	board_free_pixmaps( bd );
	rdAppearance.fClockwise = fClockwise;
	board_create_pixmaps( pwBoard, bd );
	gtk_widget_queue_draw( bd->drawing_area );
	return 0;
    }
    
    for( i = 0; i < 28; i++ )
	if( bd->points[ i ] != old_board[ i ] )
	    board_invalidate_point( bd, i );

    /* FIXME only redraw dice/cube if changed */
    board_invalidate_dice( bd );
    board_invalidate_cube( bd );
    board_invalidate_resign( bd );
    board_invalidate_arrow( bd );

    return 0;
}

static int convert_point( int i, int player ) {

    if( player )
	return ( i < 0 ) ? 26 : i + 1;
    else
	return ( i < 0 ) ? 27 : 24 - i;
}

static int animate_player, *animate_move_list, animation_finished;

static gint board_blink_timeout( gpointer p ) {

    Board *board = p;
    BoardData *pbd = board->board_data;
    int src, dest, src_cheq = 0, dest_cheq = 0, colour;
    static int blink_move, blink_count;

    if( blink_move >= 8 || animate_move_list[ blink_move ] < 0 ||
	fInterrupt ) {
	blink_move = 0;
	animation_finished = TRUE;
	return FALSE;
    }

    src = convert_point( animate_move_list[ blink_move ], animate_player );
    dest = convert_point( animate_move_list[ blink_move + 1 ],
			  animate_player );
    colour = animate_player ? 1 : -1;

    if( !( blink_count & 1 ) ) {
	src_cheq = pbd->points[ src ];
	dest_cheq = pbd->points[ dest ];

	if( pbd->points[ dest ] == -colour ) {
	    pbd->points[ dest ] = 0;
	    
	    if( blink_count == 4 ) {
		pbd->points[ animate_player ? 0 : 25 ] -= colour;
		board_invalidate_point( pbd, animate_player ? 0 : 25 );
	    }
	}
	
    	pbd->points[ src ] -= colour;
	pbd->points[ dest ] += colour;
    }

    board_invalidate_point( pbd, src );
    board_invalidate_point( pbd, dest );

    if( !( blink_count & 1 ) && blink_count < 4 ) {
	pbd->points[ src ] = src_cheq;
	pbd->points[ dest ] = dest_cheq;
    }
    
    if( blink_count++ >= 4 ) {
	blink_count = 0;	
	blink_move += 2;	
    }
    
#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( pbd->drawing_area->window, FALSE );
#endif

    return TRUE;
}

static gint board_slide_timeout( gpointer p ) {

    Board *board = p;
    BoardData *pbd = board->board_data;
    int src, dest, colour;
    static int slide_move, slide_phase, x, y, x_mid, x_dest, y_dest, y_lift;
    
    if( fInterrupt && pbd->drag_point >= 0 ) {
	board_end_drag( pbd->drawing_area, pbd );
	pbd->drag_point = -1;
    }

    if( slide_move >= 8 || animate_move_list[ slide_move ] < 0 ||
	fInterrupt ) {
	slide_move = slide_phase = 0;
	animation_finished = TRUE;
	return FALSE;
    }
    
    src = convert_point( animate_move_list[ slide_move ], animate_player );
    dest = convert_point( animate_move_list[ slide_move + 1 ],
			  animate_player );
    colour = animate_player ? 1 : -1;

    switch( slide_phase ) {
    case 0:
	/* start slide */
	chequer_position( src, abs( pbd->points[ src ] ), &x, &y );
	x += 3;
	y += 3;
	if( y < 18 )
	    y_lift = 18;
	else if( y < 36 )
	    y_lift = y + 3;
	else if( y > 54 )
	    y_lift = 54;
	else
	    y_lift = y - 3;
	chequer_position( dest, abs( pbd->points[ dest ] ) + 1, &x_dest,
			  &y_dest );
	x_dest += 3;
	y_dest += 3;
	x_mid = ( x + x_dest ) >> 1;
	board_start_drag( pbd->drawing_area, pbd, src, x * rdAppearance.nSize,
			  y * rdAppearance.nSize );
	slide_phase++;
	/* fall through */
    case 1:
	/* lift */
	if( y < 36 && y < y_lift ) {
	    y += 2;
	    break;
	} else if( y > 36 && y > y_lift ) {
	    y -= 2;
	    break;
	} else
	    slide_phase++;
	/* fall through */
    case 2:
	/* to mid point */
	if( y > 36 )
	    y--;
	else if( y < 36 )
	    y++;
	
	if( x > x_mid + 2 ) {
	    x -= 3;
	    break;
	} else if( x < x_mid - 2 ) {
	    x += 3;
	    break;
	} else
	    slide_phase++;
	/* fall through */
    case 3:
	/* from mid point */
	if( y > y_dest + 1 )
	    y -= 2;
	else if( y < y_dest - 1 )
	    y += 2;
	
	if( x < x_dest - 2 ) {
	    x += 3;
	    break;
	} else if( x > x_dest + 2 ) {
	    x -= 3;
	    break;
	} else
	    slide_phase++;
	/* fall through */
    case 4:
	/* drop */
	if( y > y_dest + 2 )
	    y -= 3;
	else if( y < y_dest - 2 )
	    y += 3;
	else {
	    board_end_drag( pbd->drawing_area, pbd );
	    
	    if( pbd->points[ dest ] == -colour ) {
		pbd->points[ dest ] = 0;
		pbd->points[ animate_player ? 0 : 25 ] -= colour;
		board_invalidate_point( pbd, animate_player ? 0 : 25 );
	    }
	    
	    pbd->points[ dest ] += colour;
	    board_invalidate_point( pbd, dest );
	    pbd->drag_point = -1;
	    slide_phase = 0;
	    slide_move += 2;

#if GTK_CHECK_VERSION(2,0,0)
	    gdk_window_process_updates( pbd->drawing_area->window, FALSE );
#endif
	    
	    playSound( SOUND_CHEQUER );

	    return TRUE;
	}
	break;
	
    default:
	g_assert_not_reached();
    }

    board_drag( pbd->drawing_area, pbd, x * rdAppearance.nSize,
		y * rdAppearance.nSize );
    
    return TRUE;
}

extern void board_animate( Board *board, int move[ 8 ], int player ) {

    int n;
    monitor m;
	
    if( animGUI == ANIMATE_NONE )
	return;

    animate_move_list = move;
    animate_player = player;

    animation_finished = FALSE;
    
    if( animGUI == ANIMATE_BLINK )
	n = gtk_timeout_add( 0x300 >> nGUIAnimSpeed, board_blink_timeout,
			     board );
    else /* ANIMATE_SLIDE */
	n = gtk_timeout_add( 0x100 >> nGUIAnimSpeed, board_slide_timeout,
			     board );

    while( !animation_finished ) {
	SuspendInput( &m );
	gtk_main_iteration();
	ResumeInput( &m );
    }
}

extern gint game_set_old_dice( Board *board, gint die0, gint die1 ) {

    BoardData *pbd = board->board_data;
    
    pbd->dice_roll[ 0 ] = die0;
    pbd->dice_roll[ 1 ] = die1;

    if( fGUIHighDieFirst && pbd->dice_roll[ 0 ] < pbd->dice_roll[ 1 ] )
	swap( pbd->dice_roll, pbd->dice_roll + 1 );
    
    return 0;
}

extern void board_set_playing( Board *board, gboolean f ) {

    BoardData *pbd = board->board_data;

    pbd->playing = f;
    gtk_widget_set_sensitive( pbd->position_id, f );
    gtk_widget_set_sensitive( pbd->reset, f );
    gtk_widget_set_sensitive( pbd->match_id, f );
}

static void update_buttons( BoardData *pbd ) {

    enum _control { C_NONE, C_ROLLDOUBLE, C_TAKEDROP, C_AGREEDECLINE,
		    C_PLAY } c;

    c = C_NONE;

    if( !pbd->dice[ 0 ] )
	c = C_ROLLDOUBLE;

    if( ms.fDoubled )
	c = C_TAKEDROP;

    if( ms.fResigned )
	c = C_AGREEDECLINE;

    if( pbd->computer_turn )
	c = C_PLAY;
    
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pbd->edit ) ) ||
	!pbd->playing )
	c = C_NONE;
    

    if ( fGUIDiceArea ) {

      if ( c == C_ROLLDOUBLE ) 
	gtk_widget_show_all( pbd->dice_area );
      else 
	gtk_widget_hide( pbd->dice_area );

    } 

    gtk_widget_set_sensitive ( pbd->roll, c == C_ROLLDOUBLE );
    gtk_widget_set_sensitive ( pbd->doub, c == C_ROLLDOUBLE );

    gtk_widget_set_sensitive ( pbd->play, c == C_PLAY );

    gtk_widget_set_sensitive ( pbd->take, 
                               c == C_TAKEDROP || c == C_AGREEDECLINE );
    gtk_widget_set_sensitive ( pbd->drop, 
                               c == C_TAKEDROP || c == C_AGREEDECLINE );
    /* FIXME: max beavers etc */
    gtk_widget_set_sensitive ( pbd->redouble, 
                               c == C_TAKEDROP && ! ms.nMatchTo );

}

extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1,
		      gint computer_turn, gint nchequers ) {
    gchar board_str[ 256 ];
    BoardData *pbd = board->board_data;
    int old_points[ 2 ][ 25 ];
    
    board_invalidate_arrow( pbd );

    /* Treat a reset of the position to old_board as a no-op while
       in edit mode. */
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pbd->edit ) ) &&
	pbd->playing && EqualBoards( pbd->old_board, points ) ) {
	read_board( pbd, old_points );
	if( pbd->turn < 0 )
	    SwapSides( old_points );
	points = old_points;
    } else
	memcpy( pbd->old_board, points, sizeof( pbd->old_board ) );

    FIBSBoard( board_str, points, roll, name, opp_name, match, score,
	       opp_score, die0, die1, ms.nCube, ms.fCubeOwner, ms.fDoubled,
	       ms.fTurn, ms.fCrawford, nchequers );

    board_set( board, board_str, -pbd->turn * ms.fResigned, ms.fCubeUse );
    
    /* FIXME update names, score, match length */
    if( rdAppearance.nSize <= 0 )
	return 0;

    pbd->computer_turn = computer_turn;
    
    if( die0 ) {
	if( fGUIHighDieFirst && die0 < die1 )
	    swap( &die0, &die1 );
	
	pbd->dice_roll[ 0 ] = die0;
	pbd->dice_roll[ 1 ] = die1;
    }

    update_buttons( pbd );

    /* FIXME it's overkill to do this every time, but if we don't do it,
       then "set turn <player>" won't redraw the dice in the other colour. */
    gtk_widget_queue_draw( pbd->dice_area );

    return 0;
}

/* Create all of the size/colour-dependent pixmaps. */
extern void board_create_pixmaps( GtkWidget *board, BoardData *bd ) {

    unsigned char auch[ 20 * 20 * 3 ], auchBoard[ 108 * 3 * 72 * 3 * 3 ],
	auchChequers[ 2 ][ 6 * 3 * 6 * 3 * 4 ];
    unsigned short asRefract[ 2 ][ 6 * 3 * 6 * 3 ];
    int i, nSizeReal;

    RenderImages( &rdAppearance, &bd->ri );
    nSizeReal = rdAppearance.nSize;
    rdAppearance.nSize = 3;
    RenderBoard( &rdAppearance, auchBoard, 108 * 3 );
    RenderChequers( &rdAppearance, auchChequers[ 0 ], auchChequers[ 1 ],
		    asRefract[ 0 ], asRefract[ 1 ], 6 * 3 * 4 );
    rdAppearance.nSize = nSizeReal;

    for( i = 0; i < 2; i++ ) {
	CopyArea( auch, 20 * 3, auchBoard + 3 * 3 * 108 * 3 + 3 * 3 * 3,
		  108 * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3, 20 * 3, auchBoard + 3 * 3 * 108 * 3 +
		  3 * 3 * 3, 108 * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3 * 20, 20 * 3, auchBoard + 3 * 3 * 108 * 3 +
		  3 * 3 * 3, 108 * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3 + 10 * 3 * 20, 20 * 3, auchBoard +
		  3 * 3 * 108 * 3 + 3 * 3 * 3, 108 * 3 * 3, 10, 10 );

	RefractBlend( auch + 20 * 3 + 3, 20 * 3,
		      auchBoard + 3 * 3 * 108 * 3 + 3 * 3 * 3, 108 * 3 * 3,
		      auchChequers[ i ], 6 * 3 * 4,
		      asRefract[ i ], 6 * 3, 6 * 3, 6 * 3 );
	
	gdk_draw_rgb_image( bd->appmKey[ i ], bd->gc_copy, 0, 0, 20, 20,
			    GDK_RGB_DITHER_MAX, auch, 20 * 3 );
    }
}

extern void board_free_pixmaps( BoardData *bd ) {

    FreeImages( &bd->ri );
}

static void board_size_allocate( GtkWidget *board,
				 GtkAllocation *allocation ) {
    BoardData *bd = BOARD( board )->board_data;
    gint old_size = rdAppearance.nSize, new_size;
    GtkAllocation child_allocation;
    GtkRequisition requisition;
    int cx;
    
    memcpy( &board->allocation, allocation, sizeof( GtkAllocation ) );

    /* toolbar: on top in the allocation */
    
    gtk_widget_get_child_requisition( bd->vbox_toolbar, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    allocation->y += requisition.height;
    gtk_widget_size_allocate( bd->vbox_toolbar, &child_allocation );

    /* position ID, match ID: just below toolbar */

    if ( fGUIShowIDs ) {

      gtk_widget_get_child_requisition( bd->vbox_ids, &requisition );
      allocation->height -= requisition.height;
      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y;
      child_allocation.width = allocation->width;
      child_allocation.height = requisition.height;
      allocation->y += requisition.height;
      gtk_widget_size_allocate( bd->vbox_ids, &child_allocation );
      
    }

    gtk_widget_get_child_requisition( bd->table, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->table, &child_allocation );
    


    /* ensure there is room for the dice area or the move, whichever is
       bigger */
    if ( fGUIDiceArea ) {

      new_size = MIN( allocation->width / 108,
                      ( allocation->height - 2 ) / 79 );

      /* subtract pixels used */
      allocation->height -= new_size * 7 + 2;

    }
    else {
      new_size = MIN( allocation->width / 108,
                      ( allocation->height - 2 ) / 72 );
    }
    
    /* FIXME what should we do if new_size < 1?  If the window manager
       honours our minimum size this won't happen, but... */
    
    if( ( rdAppearance.nSize = new_size ) != old_size &&
	GTK_WIDGET_REALIZED( board ) ) {
	board_free_pixmaps( bd );
	board_create_pixmaps( board, bd );
    }
    
    child_allocation.width = 108 * rdAppearance.nSize;
    cx = child_allocation.x = allocation->x + ( ( allocation->width -
					     child_allocation.width ) >> 1 );
    child_allocation.height = 72 * rdAppearance.nSize;
    child_allocation.y = allocation->y + ( ( allocation->height -
					     72 * rdAppearance.nSize ) >> 1 );
    gtk_widget_size_allocate( bd->drawing_area, &child_allocation );

    /* allocation for dice area */

    if ( fGUIDiceArea ) {
      child_allocation.width = 15 * rdAppearance.nSize;
      child_allocation.x += ( 108 - 15 ) * rdAppearance.nSize;
      child_allocation.height = 7 * rdAppearance.nSize;
      child_allocation.y += 72 * rdAppearance.nSize + 1;
      gtk_widget_size_allocate( bd->dice_area, &child_allocation );
    }

}

static void AddChild( GtkWidget *pw, GtkRequisition *pr ) {

    GtkRequisition r;

    gtk_widget_size_request( pw, &r );

    if( r.width > pr->width )
	pr->width = r.width;

    pr->height += r.height;
}

static void board_size_request( GtkWidget *pw, GtkRequisition *pr ) {

    BoardData *bd;
    
    if( !pw || !pr )
	return;

    pr->width = pr->height = 0;

    bd = BOARD( pw )->board_data;

    AddChild( bd->vbox_toolbar, pr );

    if ( fGUIShowIDs )
      AddChild( bd->vbox_ids, pr );

    AddChild( bd->table, pr );

    if ( fGUIDiceArea )
      pr->height += 7;

    if( pr->width < 108 )
	pr->width = 108;

    pr->height += 74;
}

static void board_realize( GtkWidget *board ) {

    BoardData *bd = BOARD( board )->board_data;
    
    if( GTK_WIDGET_CLASS( parent_class )->realize )
	GTK_WIDGET_CLASS( parent_class )->realize( board );

    board_create_pixmaps( board, bd );
}

static void board_set_position( GtkWidget *pw, BoardData *bd ) {

  char *sz = g_strdup_printf ( "set board %s",
                  gtk_entry_get_text( GTK_ENTRY( bd->position_id ) ) );
  UserCommand( sz );
  g_free ( sz );

}

static void board_set_matchid( GtkWidget *pw, BoardData *bd ) {

  char *sz = g_strdup_printf ( "set matchid %s",
            gtk_entry_get_text ( GTK_ENTRY ( bd->match_id ) ) );
  UserCommand ( sz );
  g_free ( sz );

}

static void board_show_child( GtkWidget *pwChild, BoardData *pbd ) {

  if( pwChild != pbd->dice_area && !GTK_IS_HBUTTON_BOX( pwChild ) ) 
	gtk_widget_show_all( pwChild );

}

/* Show all children except the dice area; that one hides and shows
   itself. */
static void board_show_all( GtkWidget *pw ) {
    
    BoardData *bd = BOARD( pw )->board_data;

    gtk_container_foreach( GTK_CONTAINER( pw ), (GtkCallback) board_show_child,
			   bd );
    gtk_widget_show( pw );
}

static void board_set_crawford( GtkWidget *pw, BoardData *bd ) {

    char sz[ 17 ]; /* "set crawford off" */
    int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->crawford ) );

    if( f != bd->crawford_game ) {
	sprintf( sz, "set crawford %s", f ? "on" : "off" );

	UserCommand( sz );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->crawford ),
				      bd->crawford_game );
    }
}

static void board_stop( GtkWidget *pw, BoardData *bd ) {

    fInterrupt = TRUE;
}

static void board_edit( GtkWidget *pw, BoardData *bd ) {

    int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
									
    update_move( bd );
    update_buttons( bd );
    
    if( f ) {
	/* Entering edit mode: enable entry fields for names and scores */
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname0 ), bd->name0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname1 ), bd->name1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore0 ), bd->score0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore1 ), bd->score1 );
    } else {
	/* Editing complete; set board. */
	int points[ 2 ][ 25 ], anScoreNew[ 2 ];
	const char *pch0, *pch1;
	char sz[ 64 ]; /* "set board XXXXXXXXXXXXXX" */

	/* We need to query all the widgets before issuing any commands,
	   since those commands have side effects which disturb other
	   widgets. */
	pch0 = gtk_entry_get_text( GTK_ENTRY( bd->name0 ) );
	pch1 = gtk_entry_get_text( GTK_ENTRY( bd->name1 ) );
	anScoreNew[ 0 ] = GTK_SPIN_BUTTON( bd->score0 )->adjustment->value;
	anScoreNew[ 1 ] = GTK_SPIN_BUTTON( bd->score1 )->adjustment->value;
	read_board( bd, points );

	outputpostpone();

	/* NB: these comparisons are case-sensitive, and do not use
	   CompareNames(), so that the user can modify the case of names. */
	if( strcmp( pch0, ap[ 0 ].szName ) ) {
	    sprintf( sz, "set player 0 name %s", pch0 );
	    UserCommand( sz );
	}
	
	if( strcmp( pch1, ap[ 1 ].szName ) ) {
	    sprintf( sz, "set player 1 name %s", pch1 );
	    UserCommand( sz );
	}

	if( anScoreNew[ 0 ] != ms.anScore[ 0 ] ||
	    anScoreNew[ 1 ] != ms.anScore[ 1 ] ) {
	    sprintf( sz, "set score %d %d", anScoreNew[ 0 ],
		     anScoreNew[ 1 ] );
	    UserCommand( sz );
	}

	if( bd->playing && !EqualBoards( ms.anBoard, points ) ) {
	    sprintf( sz, "set board %s", PositionID( points ) );
	    UserCommand( sz );
	}
	
	outputresume();

	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname0 ), bd->lname0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname1 ), bd->lname1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore0 ), bd->lscore0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore1 ), bd->lscore1 );
    }
}

static void DrawAlphaImage( GdkDrawable *pd, int x, int y,
			    unsigned char *puchSrc, int nStride,
			    int cx, int cy ) {
    
#if USE_GTK2
    unsigned char *puch, *puchDest, *auch = g_alloca( cx * cy * 4 );
    int ix, iy;
    GdkPixbuf *ppb;

    puchDest = auch;
    puch = puchSrc;
    nStride -= cx * 4;
    
    for( iy = 0; iy < cy; iy++ ) {
	for( ix = 0; ix < cx; ix++ ) {
	    puchDest[ 0 ] = puch[ 0 ] * 0x100 / ( 0x100 - puch[ 3 ] );
	    puchDest[ 1 ] = puch[ 2 ] * 0x100 / ( 0x100 - puch[ 3 ] );
	    puchDest[ 2 ] = puch[ 2 ] * 0x100 / ( 0x100 - puch[ 3 ] );
	    puchDest[ 3 ] = 0xFF - puch[ 3 ];

	    puch += 4;
	    puchDest += 4;
	}
	puch += nStride;
    }
	
    ppb = gdk_pixbuf_new_from_data( auch, GDK_COLORSPACE_RGB, TRUE, 8,
				    cx, cy, cx * 4, NULL, NULL );
    
    gdk_pixbuf_render_to_drawable_alpha( ppb, pd, 0, 0, x, y, cx, cy,
					 GDK_PIXBUF_ALPHA_FULL, 128,
					 GDK_RGB_DITHER_MAX, 0, 0 );
    g_object_unref( G_OBJECT( ppb ) );
#else
    /* according to the API documentation "mask" should be freed again,
       when the image is freed, so no explicit call to "free" here 
       <URL: http://developer.gnome.org/doc/API/2.0/gdk/gdk-Images.html#
       gdk-image-new-bitmap>
    */

    guchar *mask = malloc( ( cy ) * ( cx + 7 ) >> 3 );
    GdkImage *pi = gdk_image_new_bitmap( gdk_window_get_visual( pd ),
					  mask, cx, cy );
    GdkBitmap *pbm = gdk_pixmap_new( NULL, cx, cy, 1 );
    GdkGC *gc;
    int ix, iy;
    
    for( iy = 0; iy < cy; iy++ )
	for( ix = 0; ix < cx; ix++ )
	    gdk_image_put_pixel( pi, ix, iy, puchSrc[ iy * nStride +
						      ix * 4 + 3 ] & 0x80 ?
				 MASK_INVISIBLE : MASK_VISIBLE );
    gc = gdk_gc_new( pbm );
    gdk_draw_image( pbm, gc, pi, 0, 0, 0, 0, cx, cy );
    gdk_gc_unref( gc );
    gc = gdk_gc_new( pd );
    gdk_gc_set_clip_mask( gc, pbm );
    gdk_gc_set_clip_origin( gc, x, y );
    gdk_draw_rgb_32_image( pd, gc, x, y, cx, cy, GDK_RGB_DITHER_MAX, puchSrc,
			   nStride );
    gdk_gc_unref( gc );
    gdk_image_destroy( pi );
    gdk_pixmap_unref( pbm );
#endif
}    

static void DrawDie( GdkDrawable *pd, BoardData *pbd, int x, int y,
		     int fColour, int n ) {

    int s = rdAppearance.nSize;
    int ix, iy, afPip[ 9 ];

    DrawAlphaImage( pd, x, y, pbd->ri.achDice[ fColour ], 7 * s * 4,
		    7 * s, 7 * s );
    
    afPip[ 0 ] = afPip[ 8 ] = ( n == 2 ) || ( n == 3 ) || ( n == 4 ) ||
	( n == 5 ) || ( n == 6 );
    afPip[ 1 ] = afPip[ 7 ] = 0;
    afPip[ 2 ] = afPip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 );
    afPip[ 3 ] = afPip[ 5 ] = n == 6;
    afPip[ 4 ] = n & 1;

    for( iy = 0; iy < 3; iy++ )
	for( ix = 0; ix < 3; ix++ )
	    if( afPip[ iy * 3 + ix ] )
		gdk_draw_rgb_image( pd, pbd->gc_copy, x + s + 2 * s * ix,
				    y + s + 2 * s * iy, s, s,
				    GDK_RGB_DITHER_MAX,
				    pbd->ri.achPip[ fColour ], s * 3 );
}

static gboolean dice_expose( GtkWidget *dice, GdkEventExpose *event,
                             BoardData *bd ) {
    
    if( rdAppearance.nSize <= 0 || event->count || !bd->dice_roll[ 0 ] )
	return TRUE;

    DrawDie( dice->window, bd, 0, 0, bd->turn > 0, bd->dice_roll[ 0 ] );
    DrawDie( dice->window, bd, 8 * rdAppearance.nSize, 0, bd->turn > 0,
	     bd->dice_roll[ 1 ] );
    
    return TRUE;
}

static gboolean dice_press( GtkWidget *dice, GdkEvent *event,
			    BoardData *bd ) {

    UserCommand( "roll" );
    
    return TRUE;
}

static gboolean key_press( GtkWidget *pw, GdkEvent *event,
			   void *p ) {

    UserCommand( p ? "set turn 1" : "set turn 0" );
    
    return TRUE;
}

/* Create a widget to display a single chequer (used as a key in the
   player table). */
static GtkWidget *chequer_key_new( int iPlayer, Board *board ) {

    GtkWidget *pw = gtk_event_box_new(), *pwImage;
    BoardData *bd = board->board_data;
    GdkPixmap *ppm;
    char sz[ 128 ];
    
    ppm = bd->appmKey[ iPlayer ] = gdk_pixmap_new(
	NULL, 20, 20, gtk_widget_get_visual( GTK_WIDGET( board ) )->depth );

    pwImage = gtk_image_new_from_pixmap( ppm, NULL ); 

    gtk_container_add( GTK_CONTAINER( pw ), pwImage );
    
    gtk_widget_add_events( pw, GDK_BUTTON_PRESS_MASK );
    
    gtk_signal_connect( GTK_OBJECT( pw ), "button_press_event",
			GTK_SIGNAL_FUNC( key_press ), iPlayer ? pw : NULL );

    sprintf( sz, _("Set player %d on roll."), iPlayer );
    gtk_tooltips_set_tip( ptt, pw, sz, NULL );

    return pw;
}

static void ButtonClicked( GtkWidget *pw, char *sz ) {

    UserCommand( sz );
}


static void ButtonClickedYesNo( GtkWidget *pw, char *sz ) {

  if ( ms.fResigned ) {
    UserCommand ( ! strcmp ( sz, "yes" ) ? "accept" : "decline" );
    return;
  }

  if ( ms.fDoubled ) {
    UserCommand ( ! strcmp ( sz, "yes" ) ? "take" : "drop" );
    return;
  }

}


extern GtkWidget *
image_from_xpm_d ( char **xpm, GtkWidget *pw ) {

  GdkPixmap *ppm;
  GdkBitmap *mask;

  ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
                 gtk_widget_get_colormap( pw ), &mask, NULL,
                                               xpm );
  if ( ! ppm )
    return NULL;
  
  return gtk_pixmap_new( ppm, mask );

}


static GtkWidget *
button_from_image ( GtkWidget *pwImage ) {

  GtkWidget *pw = gtk_button_new ();

  gtk_container_add ( GTK_CONTAINER ( pw ), pwImage );

  return pw;

}
  
static GtkWidget *
toggle_button_from_image ( GtkWidget *pwImage ) {

  GtkWidget *pw = gtk_toggle_button_new ();

  gtk_container_add ( GTK_CONTAINER ( pw ), pwImage );

  return pw;

}
  


static void board_init( Board *board ) {

    BoardData *bd = g_malloc( sizeof( *bd ) );
    GdkColormap *cmap = gtk_widget_get_colormap( GTK_WIDGET( board ) );
    GdkVisual *vis = gtk_widget_get_visual( GTK_WIDGET( board ) );
    GdkGCValues gcval;
    GtkWidget *pw;
    GtkWidget *pwFrame;
    GtkWidget *pwvbox;

#include "xpm/tb_roll.xpm"
#include "xpm/tb_double.xpm"
#include "xpm/tb_redouble.xpm"
#include "xpm/tb_edit.xpm"
#ifndef USE_GTK2
#include "xpm/tb_no.xpm"
#include "xpm/tb_yes.xpm"
#include "xpm/tb_stop.xpm"
#include "xpm/tb_undo.xpm"
#endif
    
    board->board_data = bd;
    bd->widget = GTK_WIDGET( board );
	
    bd->drag_point = rdAppearance.nSize = -1;
    bd->dice_roll[ 0 ] = bd->dice_roll[ 1 ] = 0;
    bd->crawford_game = FALSE;
    bd->playing = FALSE;
    bd->cube_use = TRUE;    
    bd->all_moves = NULL;

    gcval.function = GDK_AND;
    gcval.foreground.pixel = ~0L; /* AllPlanes */
    gcval.background.pixel = 0;
    bd->gc_and = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FOREGROUND |
			     GDK_GC_BACKGROUND | GDK_GC_FUNCTION );

    gcval.function = GDK_OR;
    bd->gc_or = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FUNCTION );

    bd->gc_copy = gtk_gc_get( vis->depth, cmap, &gcval, 0 );

    gcval.foreground.pixel = gdk_rgb_xpixel_from_rgb( 0x000080 );
    /* ^^^ use gdk_get_color  and gdk_gc_set_foreground.... */
    bd->gc_cube = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FOREGROUND );

    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;    
    
    /* 
     * Create tool bar 
     */

    bd->vbox_toolbar = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( board ), bd->vbox_toolbar );
    

#if USE_GTK2
    bd->toolbar = gtk_toolbar_new ();
    gtk_toolbar_set_orientation ( GTK_TOOLBAR ( bd->toolbar ),
                                  GTK_ORIENTATION_HORIZONTAL );
    gtk_toolbar_set_style ( GTK_TOOLBAR ( bd->toolbar ),
                            GTK_TOOLBAR_ICONS );
#else
    bd->toolbar = gtk_toolbar_new ( GTK_ORIENTATION_HORIZONTAL,
                                    GTK_TOOLBAR_ICONS );
#endif /* ! USE_GTK2 */


    gtk_box_pack_start( GTK_BOX( bd->vbox_toolbar ), bd->toolbar, 
                        FALSE, FALSE, 0 );

    gtk_toolbar_set_tooltips ( GTK_TOOLBAR ( bd->toolbar ), TRUE );

    /* roll button */

    bd->roll = button_from_image( image_from_xpm_d ( tb_roll_xpm,
						     bd->toolbar ) );
    gtk_signal_connect( GTK_OBJECT( bd->roll ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "roll" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->roll ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->roll,
                                _("Roll dice"),
                                _("private") );

    /* double button */

    bd->doub = button_from_image( image_from_xpm_d ( tb_double_xpm,
						     bd->toolbar ) );
    gtk_signal_connect( GTK_OBJECT( bd->doub ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "double" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->doub ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->doub,
                                _("Double"),
                                _("private") );

    /* take button */

    gtk_toolbar_append_space ( GTK_TOOLBAR ( bd->toolbar ) );

#if USE_GTK2
    bd->take =
      button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_YES, 
                                                     GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
    bd->take =
      button_from_image ( image_from_xpm_d ( tb_yes_xpm,
                                             bd->toolbar ) );
#endif

    gtk_signal_connect( GTK_OBJECT( bd->take ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClickedYesNo ), "yes" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->take ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->take,
                                _("Take the offered cube or "
                                  "accept the offered resignation"),
                                _("private") );

    /* drop button */

#if USE_GTK2
    bd->drop =
      button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_NO, 
                                                     GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
    bd->drop =
      button_from_image ( image_from_xpm_d ( tb_no_xpm,
                                             bd->toolbar ) );
#endif

    gtk_signal_connect( GTK_OBJECT( bd->drop ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClickedYesNo ), "no" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->drop ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->drop,
                                _("Drop the offered cube or "
                                  "decline offered resignation"),
                                _("private") );

    /* redouble button */

    bd->redouble = button_from_image( image_from_xpm_d ( tb_redouble_xpm,
							 bd->toolbar ) );
    gtk_signal_connect( GTK_OBJECT( bd->redouble ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "redouble" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->redouble ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->redouble,
                                _("Redouble immediately (beaver)"),
                                _("private") );

    /* play button */

    gtk_toolbar_append_space ( GTK_TOOLBAR ( bd->toolbar ) );

#if USE_GTK2
    bd->play = 
      button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_EXECUTE, 
                                                     GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
    bd->play = gtk_button_new_with_label ( _("Play" ) );
#endif
    gtk_signal_connect( GTK_OBJECT( bd->play ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "play" );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->play ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->play,
                                _("Force computer to play"),
                                _("private") );

    /* reset button */

    gtk_toolbar_append_space ( GTK_TOOLBAR ( bd->toolbar ) );

#if USE_GTK2
    bd->reset = 
      button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_UNDO, 
                                                     GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
    bd->reset = 
      button_from_image ( image_from_xpm_d ( tb_undo_xpm,
                                             bd->toolbar ) );
#endif
    gtk_signal_connect( GTK_OBJECT( bd->reset ), "clicked",
			GTK_SIGNAL_FUNC( ShowBoard ), NULL );
    gtk_widget_set_sensitive ( GTK_WIDGET ( bd->play ), FALSE );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->reset,
                                _("Undo moves"),
                                _("private") );

    /* edit button */

    gtk_toolbar_append_space ( GTK_TOOLBAR ( bd->toolbar ) );

    bd->edit =
      toggle_button_from_image ( image_from_xpm_d ( tb_edit_xpm,
                                             bd->toolbar ) );
    gtk_signal_connect( GTK_OBJECT( bd->edit ), "toggled",
			GTK_SIGNAL_FUNC( board_edit ), bd );
    
    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->edit,
                                _("Edit position"),
                                _("private") );

    /* stop button */

    gtk_toolbar_append_space ( GTK_TOOLBAR ( bd->toolbar ) );

    bd->stopparent = gtk_event_box_new();
#if USE_GTK2
    bd->stop =
      button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_STOP, 
                                                     GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
    bd->stop =
      button_from_image ( image_from_xpm_d ( tb_stop_xpm,
                                             bd->toolbar ) );
#endif
    gtk_container_add( GTK_CONTAINER( bd->stopparent ), bd->stop );

    gtk_signal_connect( GTK_OBJECT( bd->stop ), "clicked",
			GTK_SIGNAL_FUNC( board_stop ), NULL );

    gtk_toolbar_append_widget ( GTK_TOOLBAR ( bd->toolbar ),
                                bd->stopparent,
                                _("Stop the current operation"),
                                _("private") );

    /* horisontal separator */

    pw = gtk_hseparator_new ();
    gtk_box_pack_start( GTK_BOX( bd->vbox_toolbar ), pw, 
                        FALSE, FALSE, 0 );


    /* board drawing area */

    bd->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->drawing_area ), 108,
			   72 );
    gtk_widget_add_events( GTK_WIDGET( bd->drawing_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK | GDK_STRUCTURE_MASK );
    gtk_container_add( GTK_CONTAINER( board ), bd->drawing_area );


    /* Position and match ID */

    bd->vbox_ids = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( board ), bd->vbox_ids, FALSE, FALSE, 0 );

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bd->vbox_ids ), pw, FALSE, FALSE, 0 );

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Position ID: ") ),
                         FALSE, FALSE, 4 );

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->position_id = gtk_entry_new(),
                         FALSE, FALSE, 0 );
    gtk_entry_set_max_length( GTK_ENTRY( bd->position_id ), 14 );

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Match ID: ") ),
                         FALSE, FALSE, 8 );

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->match_id = gtk_entry_new(),
                         FALSE, FALSE, 0 );
    gtk_entry_set_max_length( GTK_ENTRY( bd->match_id ), 12 );


    pw = gtk_hseparator_new ();
    gtk_box_pack_start( GTK_BOX( bd->vbox_ids ), pw, 
                        FALSE, FALSE, 0 );

    /* the rest */

    bd->table = gtk_hbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( board ), bd->table );

    /* 
     * player 0 
     */

    pwFrame = gtk_frame_new ( NULL );
    gtk_box_pack_start ( GTK_BOX ( bd->table ), pwFrame,
                         FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    /* first row: picture of chequer + name of player */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* picture of chequer */

    bd->key0 = chequer_key_new( 0, board );
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->key0, FALSE, FALSE, 0 );

    /* name of player */

    bd->mname0 = gtk_multiview_new ();
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->mname0, FALSE, FALSE, 8 );

    bd->name0 = gtk_entry_new_with_max_length( 32 );
    bd->lname0 = gtk_label_new( NULL );
    gtk_misc_set_alignment( GTK_MISC( bd->lname0 ), 0, 0.5 );
    gtk_container_add( GTK_CONTAINER( bd->mname0 ), bd->lname0 );
    gtk_container_add( GTK_CONTAINER( bd->mname0 ), bd->name0 );

    /* second row: "Score" + score */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* score label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Score:") ), FALSE, FALSE, 0 );

    /* score */

    bd->mscore0 = gtk_multiview_new();
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->mscore0, FALSE, FALSE, 8 );

    bd->score0 = gtk_spin_button_new( GTK_ADJUSTMENT(
	gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) ), 1, 0 );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( bd->score0 ), TRUE );
    bd->lscore0 = gtk_label_new( NULL );
    gtk_container_add( GTK_CONTAINER( bd->mscore0 ), bd->lscore0 );
    gtk_container_add( GTK_CONTAINER( bd->mscore0 ), bd->score0 );

    /* third row: pip count */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* pip count label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Pips:") ), FALSE, FALSE, 0 );

    /* pip count */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->pipcount0 = gtk_label_new ( NULL ), 
                         FALSE, FALSE, 0 );

    /* 
     * player 1
     */

    pwFrame = gtk_frame_new ( NULL );
    gtk_box_pack_start ( GTK_BOX ( bd->table ), pwFrame,
                       FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    /* first row: picture of chequer + name of player */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* picture of chequer */

    bd->key1 = chequer_key_new( 1, board );
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->key1, FALSE, FALSE, 0 );

    /* name of player */

    bd->mname1 = gtk_multiview_new ();
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->mname1, FALSE, FALSE, 8 );

    bd->name1 = gtk_entry_new_with_max_length( 32 );
    bd->lname1 = gtk_label_new( NULL );
    gtk_misc_set_alignment( GTK_MISC( bd->lname1 ), 0, 0.5 );
    gtk_container_add( GTK_CONTAINER( bd->mname1 ), bd->lname1 );
    gtk_container_add( GTK_CONTAINER( bd->mname1 ), bd->name1 );

    /* second row: "Score" + score */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* score label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Score:") ), FALSE, FALSE, 0 );

    /* score */

    bd->mscore1 = gtk_multiview_new();
    gtk_box_pack_start ( GTK_BOX ( pw ), bd->mscore1, FALSE, FALSE, 8 );

    bd->score1 = gtk_spin_button_new( GTK_ADJUSTMENT(
	gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) ), 1, 0 );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( bd->score1 ), TRUE );
    bd->lscore1 = gtk_label_new( NULL );
    gtk_container_add( GTK_CONTAINER( bd->mscore1 ), bd->lscore1 );
    gtk_container_add( GTK_CONTAINER( bd->mscore1 ), bd->score1 );

    /* third row: pip count */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* pip count label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Pips:") ), FALSE, FALSE, 0 );

    /* pip count */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->pipcount1 = gtk_label_new ( NULL ), 
                         FALSE, FALSE, 0 );


    /* 
     * move string, match length, crawford flag, dice
     */

    pwFrame = gtk_frame_new ( NULL );
    gtk_box_pack_end ( GTK_BOX ( bd->table ), pwFrame,
                         TRUE, TRUE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    /* move string */

    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                         bd->move = gtk_label_new( NULL ),
                         FALSE, FALSE, 0 );
    gtk_widget_set_name( bd->move, "move" );

    /* match length */

    gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                         pw = gtk_hbox_new ( FALSE, 0 ),
                         FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pw ),
			gtk_label_new( _("Match:") ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pw ),
                        bd->match = gtk_label_new( NULL ), 
                        FALSE, FALSE, 0 );

    /* crawford flag */

    gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                        bd->crawford =
                        gtk_check_button_new_with_label( _("Crawford game") ),
                        FALSE, FALSE, 0 );
    gtk_signal_connect( GTK_OBJECT( bd->crawford ), "toggled",
			GTK_SIGNAL_FUNC( board_set_crawford ), bd );


    /* dice drawing area */

    gtk_container_add ( GTK_CONTAINER ( board ),
                        bd->dice_area = gtk_drawing_area_new() );
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->dice_area ), 15, 8 );
    gtk_widget_add_events( GTK_WIDGET( bd->dice_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK | GDK_STRUCTURE_MASK );

    /* setup initial board */

    board_set( board, "board:::1:0:0:"
              "0:-2:0:0:0:0:5:0:3:0:0:0:-5:5:0:0:0:-3:0:-5:0:0:0:0:2:0:"
              "0:0:0:0:0:1:1:1:0:1:-1:0:25:0:0:0:0:0:0:0:0", 0, TRUE );
    
    gtk_widget_show_all( GTK_WIDGET( board ) );
	
    gtk_signal_connect( GTK_OBJECT( bd->position_id ), "activate",
			GTK_SIGNAL_FUNC( board_set_position ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->match_id ), "activate",
			GTK_SIGNAL_FUNC( board_set_matchid ), bd );

    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "expose_event",
			GTK_SIGNAL_FUNC( board_expose ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "button_press_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "button_release_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "motion_notify_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );

    gtk_signal_connect( GTK_OBJECT( bd->dice_area ), "expose_event",
			GTK_SIGNAL_FUNC( dice_expose ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->dice_area ), "button_press_event",
			GTK_SIGNAL_FUNC( dice_press ), bd );

}

static void board_class_init( BoardClass *c ) {

    parent_class = gtk_type_class( GTK_TYPE_VBOX );
    
    ( (GtkWidgetClass *) c )->size_allocate = board_size_allocate;
    ( (GtkWidgetClass *) c )->size_request = board_size_request;
    ( (GtkWidgetClass *) c )->realize = board_realize;
    ( (GtkWidgetClass *) c )->show_all = board_show_all;
}

extern GtkType board_get_type( void ) {

    static GtkType board_type = 0;
    static const GtkTypeInfo board_info = {
        "Board",
	sizeof( Board ),
	sizeof( BoardClass ),
	(GtkClassInitFunc) board_class_init,
	(GtkObjectInitFunc) board_init,
	NULL, NULL, NULL
    };
    
    if( !board_type ) {
	irandinit( &rc, FALSE );
	board_type = gtk_type_unique( GTK_TYPE_VBOX, &board_info );
    }
    
    return board_type;
}

static gboolean cube_widget_expose( GtkWidget *cube, GdkEventExpose *event,
				    BoardData *bd ) {

    int n, nValue;
    unsigned char *puch;
    
    if( event->count )
	return TRUE;

    n = (int) gtk_object_get_user_data( GTK_OBJECT( cube ) );
    if( ( nValue = n % 13 - 1 ) == -1 )
	nValue = 5; /* use 64 cube for 1 */
    
    puch = g_alloca( 6 * rdAppearance.nSize * 6 * rdAppearance.nSize * 3 );
    CopyAreaRotateClip( puch, 6 * rdAppearance.nSize * 3, 0, 0,
			6 * rdAppearance.nSize, 6 * rdAppearance.nSize,
			bd->ri.achCubeFaces, 6 * rdAppearance.nSize * 3, 0,
			6 * rdAppearance.nSize * nValue,
			6 * rdAppearance.nSize, 6 * rdAppearance.nSize,
			2 - n / 13 );
    
    DrawAlphaImage( cube->window, 0, 0, bd->ri.achCube, 8 * rdAppearance.nSize * 4,
		    8 * rdAppearance.nSize, 8 * rdAppearance.nSize );
    gdk_draw_rgb_image( cube->window, bd->gc_copy, rdAppearance.nSize, rdAppearance.nSize,
			6 * rdAppearance.nSize, 6 * rdAppearance.nSize, GDK_RGB_DITHER_MAX,
			puch, 6 * rdAppearance.nSize * 3 );
    
    return TRUE;
}

static gboolean cube_widget_press( GtkWidget *cube, GdkEvent *event,
				   BoardData *bd ) {

    GtkWidget *pwTable = cube->parent;
    int n = (int) gtk_object_get_user_data( GTK_OBJECT( cube ) );
    int *an = gtk_object_get_user_data( GTK_OBJECT( pwTable ) );

    an[ 0 ] = n % 13; /* value */
    if( n < 13 )
	an[ 1 ] = 0; /* top player */
    else if( n < 26 )
	an[ 1 ] = -1; /* centred */
    else
	an[ 1 ] = 1; /* bottom player */

    gtk_widget_destroy( pwTable );
    
    return TRUE;
}

extern GtkWidget *board_cube_widget( Board *board ) {

    GtkWidget *pw = gtk_table_new( 3, 13, TRUE ), *pwCube;
    BoardData *bd = board->board_data;    
    int x, y;

    for( y = 0; y < 3; y++ )
	for( x = 0; x < 13; x++ ) {
	    pwCube = gtk_drawing_area_new();
	    gtk_object_set_user_data( GTK_OBJECT( pwCube ),
				      (gpointer) ( y * 13 + x ) );
	    gtk_drawing_area_size( GTK_DRAWING_AREA( pwCube ),
				   8 * rdAppearance.nSize, 8 * rdAppearance.nSize );
	    gtk_widget_add_events( pwCube, GDK_EXPOSURE_MASK |
				   GDK_BUTTON_PRESS_MASK |
				   GDK_STRUCTURE_MASK );
	    gtk_signal_connect( GTK_OBJECT( pwCube ), "expose_event",
				GTK_SIGNAL_FUNC( cube_widget_expose ), bd );
	    gtk_signal_connect( GTK_OBJECT( pwCube ), "button_press_event",
				GTK_SIGNAL_FUNC( cube_widget_press ), bd );
	    gtk_table_attach_defaults( GTK_TABLE( pw ), pwCube,
				       x, x + 1, y, y + 1 );
        }

    gtk_table_set_row_spacings( GTK_TABLE( pw ), 4 * rdAppearance.nSize );
    gtk_table_set_col_spacings( GTK_TABLE( pw ), 2 * rdAppearance.nSize );
    gtk_container_set_border_width( GTK_CONTAINER( pw ), rdAppearance.nSize );
    
    return pw;	    
}

static gboolean dice_widget_expose( GtkWidget *dice, GdkEventExpose *event,
				    BoardData *bd ) {

    int n = (int) gtk_object_get_user_data( GTK_OBJECT( dice ) );

    if( event->count )
	return TRUE;
    
    DrawDie( dice->window, bd, 0, 0, bd->turn > 0, n % 6 + 1 );
    DrawDie( dice->window, bd, 7 * rdAppearance.nSize, 0, bd->turn > 0, n / 6 + 1 );
    
    return TRUE;
}

static gboolean dice_widget_press( GtkWidget *dice, GdkEvent *event,
			    BoardData *bd ) {

    GtkWidget *pwTable = dice->parent;
    int n = (int) gtk_object_get_user_data( GTK_OBJECT( dice ) );
    int *an = gtk_object_get_user_data( GTK_OBJECT( pwTable ) );

    an[ 0 ] = n % 6 + 1;
    an[ 1 ] = n / 6 + 1;

    gtk_widget_destroy( pwTable );
    
    return TRUE;
}

extern GtkWidget *board_dice_widget( Board *board ) {

    GtkWidget *pw = gtk_table_new( 6, 6, TRUE ), *pwDice;
    BoardData *bd = board->board_data;    
    int x, y;

    for( y = 0; y < 6; y++ )
	for( x = 0; x < 6; x++ ) {
	    pwDice = gtk_drawing_area_new();
	    gtk_object_set_user_data( GTK_OBJECT( pwDice ),
				      (gpointer) ( y * 6 + x ) );
	    gtk_drawing_area_size( GTK_DRAWING_AREA( pwDice ),
				   14 * rdAppearance.nSize, 7 * rdAppearance.nSize );
	    gtk_widget_add_events( pwDice, GDK_EXPOSURE_MASK |
				   GDK_BUTTON_PRESS_MASK |
				   GDK_STRUCTURE_MASK );
	    gtk_signal_connect( GTK_OBJECT( pwDice ), "expose_event",
				GTK_SIGNAL_FUNC( dice_widget_expose ), bd );
	    gtk_signal_connect( GTK_OBJECT( pwDice ), "button_press_event",
				GTK_SIGNAL_FUNC( dice_widget_press ), bd );
	    gtk_table_attach_defaults( GTK_TABLE( pw ), pwDice,
				       x, x + 1, y, y + 1 );
        }

    gtk_table_set_row_spacings( GTK_TABLE( pw ), 4 * rdAppearance.nSize );
    gtk_table_set_col_spacings( GTK_TABLE( pw ), 2 * rdAppearance.nSize );
    gtk_container_set_border_width( GTK_CONTAINER( pw ), rdAppearance.nSize );
    
    return pw;	    
}
