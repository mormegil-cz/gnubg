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
#include "boardpos.h"
#include "matchequity.h"
#include "gtktoolbar.h"
#include "boarddim.h"

#define MASK_INVISIBLE 1
#define MASK_VISIBLE 0

#if !USE_GTK2
#define gtk_image_new_from_pixmap gtk_pixmap_new
#endif

/* minimum time in milliseconds before a drag to the
	same point is considered a real drag rather than a click */
#define CLICK_TIME 250

animation animGUI = ANIMATE_SLIDE;
int nGUIAnimSpeed = 4, fGUIBeep = TRUE, fGUIDiceArea = FALSE,
    fGUIHighDieFirst = TRUE, fGUIIllegal = FALSE, fGUIShowIDs = TRUE,
    fGUIShowGameInfo = TRUE,
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

void board_beep( BoardData *bd )
{
    if( fGUIIllegal )
	gdk_beep();
}

void read_board( BoardData *bd, gint points[ 2 ][ 25 ] ) {

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
    if ( anBoard[ 1 ][ i ] )
      points[ i + 1 ] = anBoard[ 1 ][ i ];
    if ( anBoard[ 0 ][ i ] )
      points[ 24 - i ] = -anBoard[ 0 ][ i ];
  }

  /* Player on bar */
  points[ 25 ] = anBoard[ 1 ][ 24 ];

  anOff[ 0 ] = anOff[ 1 ] = nchequers;
  for( i = 0; i < 25; i++ ) {
    anOff[ 0 ] -= anBoard[ 0 ][ i ];
    anOff[ 1 ] -= anBoard[ 1 ][ i ];
  }
    
  points[ 26 ] = anOff[ 1 ];
  points[ 27 ] = -anOff[ 0 ];

}

void write_board ( BoardData *bd, int anBoard[ 2 ][ 25 ] ) {

  int an[ 2 ][ 25 ];

  memcpy( an, anBoard, sizeof an );

  if ( bd->turn < 0 )
    SwapSides( an );

  write_points( bd->points, bd->turn, bd->nchequers, an );

}

static void chequer_position( int point, int chequer, int *px, int *py ) {

  ChequerPosition( fClockwise, point, chequer, px, py );

}

static void point_area( BoardData *bd, int n, int *px, int *py,
			int *pcx, int *pcy ) {

  PointArea( fClockwise, rdAppearance.nSize, n, px, py, pcx, pcy );
    
}

/* Determine the position and rotation of the cube; *px and *py return the
   position (in board units -- multiply by rdAppearance.nSize to get
   pixels) and *porient returns the rotation (1 = facing the top, 0 = facing
   the side, -1 = facing the bottom). */
static void cube_position( BoardData *bd, int *px, int *py, int *porient ) {

  CubePosition( bd->crawford_game, bd->cube_use, bd->doubled,
                bd->cube_owner, px, py, porient );

}

static void resign_position( BoardData *bd, int *px, int *py, int *porient ) {

  ResignPosition( bd->resigned, px, py, porient );

}

static void Arrow_Position( BoardData *bd, int *px, int *py ) {

  ArrowPosition( fClockwise, rdAppearance.nSize, px, py );

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
    anDice[ 0 ] = bd->diceRoll[ 0 ];
    anDice[ 1 ] = bd->diceRoll[ 1 ];
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

    if( y + cy > BOARD_HEIGHT * rdAppearance.nSize )
	cy = BOARD_HEIGHT * rdAppearance.nSize - y;

    if( x + cx > BOARD_WIDTH * rdAppearance.nSize )
	cx = BOARD_WIDTH * rdAppearance.nSize - x;

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
    cx = DIE_WIDTH * rdAppearance.nSize;
    cy = DIE_HEIGHT * rdAppearance.nSize;

    board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );
    
    x = bd->x_dice[ 1 ] * rdAppearance.nSize;
    y = bd->y_dice[ 1 ] * rdAppearance.nSize;

    board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );
}

static void
board_invalidate_labels( BoardData *bd ) {

  int x, y, cx, cy;
    
  x = 0;
  y = 0;
  cx = BOARD_WIDTH * rdAppearance.nSize;
  cy = BORDER_HEIGHT * rdAppearance.nSize;

  board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );

  x = 0;
  y = ( BOARD_HEIGHT - BORDER_HEIGHT ) * rdAppearance.nSize;
  cx = BOARD_WIDTH * rdAppearance.nSize;
  cy = BORDER_HEIGHT * rdAppearance.nSize;

  board_invalidate_rect( bd->drawing_area, x, y, cx, cy, bd );

}

static void board_invalidate_cube( BoardData *bd ) {

    int x, y, orient;
    
    cube_position( bd, &x, &y, &orient );
    
    board_invalidate_rect( bd->drawing_area, x * rdAppearance.nSize,
			   y * rdAppearance.nSize,
			   CUBE_WIDTH * rdAppearance.nSize,
			   CUBE_HEIGHT * rdAppearance.nSize, bd );
}

static void board_invalidate_resign( BoardData *bd ) {

    int x, y, orient;
    
    resign_position( bd, &x, &y, &orient );
    
    board_invalidate_rect( bd->drawing_area, x * rdAppearance.nSize,
			   y * rdAppearance.nSize,
			   RESIGN_WIDTH * rdAppearance.nSize,
			   RESIGN_HEIGHT * rdAppearance.nSize, bd );
}

static void board_invalidate_arrow( BoardData *bd ) {

    int x, y;
    
    Arrow_Position( bd, &x, &y );

    board_invalidate_rect( bd->drawing_area, x, y,
			   ARROW_WIDTH * rdAppearance.nSize,
			   ARROW_HEIGHT * rdAppearance.nSize, bd );
}

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
    if( intersects( x0, y0, 0, 0, 60, 37, 36, 8 ) )
	return POINT_RIGHT;
    else if( intersects( x0, y0, 0, 0, 12, 37, 36, 8 ) )
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
                      MatchID( bd->diceRoll,
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


void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] ) {

    gtk_entry_set_text( GTK_ENTRY( bd->position_id ), PositionID( points ) );
}

static char *
ReturnHits( int anBoard[ 2 ][ 25 ] ) {

  int aiHit[ 15 ];
  int i, j, k, l, m, n, c;
  movelist ml;
  int aiDiceHit[ 6 ][ 6 ];

  memset( aiHit, 0, sizeof ( aiHit ) );
  memset( aiDiceHit, 0, sizeof ( aiDiceHit ) );

  SwapSides( anBoard );

  /* find blots */

  for ( i = 0; i < 6; ++i )
    for ( j = 0; j <= i; ++j ) {

      if ( ! ( c = GenerateMoves( &ml, anBoard, i + 1, j + 1, FALSE ) ) )
        /* no legal moves */
        continue;
      
      k = 0;
      for ( l = 0; l < 24; ++l )
        if ( anBoard[ 0 ][ l ] == 1 ) {
          for ( m = 0; m < c; ++m ) {
            move *pm = &ml.amMoves[ m ];
            for ( n = 0; n < 4 && pm->anMove[ 2 * n ] > -1; ++n )
              if ( pm->anMove[ 2 * n + 1 ] == ( 23 - l ) ) {
                /* hit */
                aiHit[ k ] += 2 - ( i == j );
                ++aiDiceHit[ i ][ j ];
                /* next point */
                goto nextpoint;
              }
          }
        nextpoint:
          ++k;
        }

    }

  for ( j = 14; j >= 0; --j )
    if ( aiHit[ j ] )
      break;

  if ( j >= 0 ) {
    char *pch = g_malloc( 3 * ( j + 1 ) + 200 );
    strcpy( pch, "" );
    for ( i = 0; i <= j; ++i )
      if ( aiHit[ i ] )
        sprintf( strchr( pch, 0 ), "%d ", aiHit[ i ] );

    for ( n = 0, i = 0; i < 6; ++i )
      for ( j = 0; j <= i; ++j )
        n += ( aiDiceHit[ i ][ j ] > 0 ) * ( 2 - ( i == j ) );

    sprintf( strchr( pch, 0 ), _("(no hit: %d rolls)"), 36 - n );
    return pch;
  }

  return NULL;

}

void update_pipcount ( BoardData *bd, gint points[ 2 ][ 25 ] ) {

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

#if USE_TIMECONTROL
extern void board_set_clock(Board *board, gchar *c0, gchar *c1)
{
	gtk_label_set_text ( GTK_LABEL(((BoardData *) board->board_data)->clock0), c0 );  
	gtk_label_set_text ( GTK_LABEL(((BoardData *) board->board_data)->clock1), c1 );
}

extern void board_set_scores(Board *board,int s0, int s1)
{
    char buf[16];
    BoardData *bd = (BoardData *) board->board_data;
    if ( bd->match_to ) {
	if ( (bd->score_opponent=s0) >= bd->match_to )
	    sprintf( buf, "%d (won match)", bd->score_opponent );
	else
	    sprintf( buf, "%d (%d-away)", bd->score_opponent,
		bd->match_to - bd->score_opponent );
	gtk_label_set_text( GTK_LABEL( bd->lscore0 ), buf );

	if ( (bd->score=s1) >= bd->match_to )
	    sprintf( buf, "%d (won match)", bd->score );
	else
	    sprintf( buf, "%d (%d-away)", bd->score,
		bd->match_to - bd->score );
	gtk_label_set_text( GTK_LABEL( bd->lscore1 ), buf );
    } else {
	sprintf( buf, "%d", bd->score_opponent = s0 );
	gtk_label_set_text( GTK_LABEL( bd->lscore0 ), buf );
	sprintf( buf, "%d", bd->score = s1 );
	gtk_label_set_text( GTK_LABEL( bd->lscore1 ), buf );
    }
}
#endif

/* A chequer has been moved or the board has been updated -- update the
   move and position ID labels. */
int update_move(BoardData *bd)
{
    char *move = _("Illegal move"), move_buf[ 40 ];
    gint i, points[ 2 ][ 25 ];
    guchar key[ 10 ];
    int fIncomplete = TRUE, fIllegal = TRUE;
    
    read_board( bd, points );
    update_position_id( bd, points );
    update_pipcount ( bd, points );

    bd->valid_move = NULL;
    
    if( ToolbarIsEditing( pwToolbar ) && bd->playing ) {
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

        /* show number of return hits */

        if ( bd->valid_move ) {
          int anBoard[ 2 ][ 25 ];
          char *pch;
          PositionFromKey( anBoard, bd->valid_move->auch );
          if ( ( pch = ReturnHits( anBoard ) ) ) {
            outputf( _("Return hits: %s\n"), pch );
            outputx();
            g_free( pch );
          }
          else {
            outputl( "" ); 
            outputx();
          }
        }

    }

    gtk_widget_set_state( bd->wmove, fIncomplete ? GTK_STATE_ACTIVE :
			  GTK_STATE_NORMAL );
    gtk_label_set_text( GTK_LABEL( bd->wmove ), move );

    return fIllegal ? -1 : 0;
}

void Confirm( BoardData *bd ) {

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

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		SetMovingPieceRotation(bd, bd->drag_point);
		updatePieceOccPos(bd);
	}
	else
#endif
	{
	    board_invalidate_point( bd, drag_point );

#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( bd->drawing_area->window, FALSE );
#endif
    
		bd->x_drag = x;
		bd->y_drag = y;
	}
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

	if ( bd->diceRoll[0] == bd->diceRoll[1] ) {
	/* double roll: up to 4 possibilities to move, but only in multiples of dice[0] */
		for ( i = 0; i <= 3; ++i ) {
			if ( ( (i + 1) * bd->diceRoll[0] > anCurPipCount[ bd->drag_colour == -1 ? 0 : 1 ] - anPipsBeforeMove[ bd->drag_colour == -1 ? 0 : 1 ] + bd->move_list.cMaxPips )	/* no moves left*/
			     || ( i && bd->points[ bar ] ) )		/* moving with chequer just entered not allowed if more chequers on the bar */
				break;
			iDestLegal = TRUE;
			if ( !i || iCanMove & ( 1 << ( i - 1 ) ) ) {
			/* only if moving first chequer or if last part-move succeeded */
				iDestPt = bd->drag_point - bd->diceRoll[0] * ( i + 1 ) * bd->drag_colour;
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
								if ( ! PointsAreEmpty( bd, bd->drag_point - i * bd->diceRoll[0] + 1, 6, bd->drag_colour ) ) {
									iDestPt = -1;
									iDestLegal = FALSE;
								}
							}
							else {
								if ( ! PointsAreEmpty( bd, bd->drag_point + i * bd->diceRoll[0] - 1, 19, bd->drag_colour ) ) {
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
			if ( ( iUnusedPips < bd->diceRoll[i] ) ||		/* not possible to move with this die (anymore) */
			     ( ( bd->valid_move ) && ( bd->diceRoll[i] == ( bd->valid_move->anMove[0] - bd->valid_move->anMove[1] ) ) ) ||		/* this die has been used already */
			     ( ( bd->valid_move ) && ( bd->valid_move->cMoves > 1 ) )		/* move already completed */ /* && ( bd->diceRoll[i] != iUnusedPips ) && ( iUnusedPips != bd->move_list.cMaxPips ) */ )
				continue;
			iDestLegal = TRUE;
			iDestPt = bd->drag_point - bd->diceRoll[i] * bd->drag_colour;
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
			iDestPt = bd->drag_point - ( bd->diceRoll[0] + bd->diceRoll[1] ) * bd->drag_colour;
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
						if ( ! PointsAreEmpty( bd, bd->drag_point - ( bd->diceRoll[0] < bd->diceRoll[1] ? bd->diceRoll[0] : bd->diceRoll[1] ) + 1, 6, bd->drag_colour ) ) {
							iDestPt = -1;
							iDestLegal = FALSE;
						}
					}
					else {
						if ( ! PointsAreEmpty( bd, bd->drag_point + ( bd->diceRoll[0] < bd->diceRoll[1] ? bd->diceRoll[0] : bd->diceRoll[1] ) - 1, 19, bd->drag_colour ) ) {
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
    
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		if (MouseMove3d(bd, x, y))
			gtk_widget_queue_draw(widget);
		return;
	}
#endif

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

/* This code is called on a button release event
 *
 * Since this code has side-effects, it should only be called from
 * button_release_event().  Mainly, it assumes that a checker
 * has been picked up.  If the move fails, that pickup will be reverted.
 */
gboolean place_chequer_or_revert(BoardData *bd, 
					 int dest )
{
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

#if USE_BOARD3D
	if (placed && rdAppearance.fDisplayType == DT_3D)
		PlaceMovingPieceRotation(bd, dest, bd->drag_point);
#endif
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

static void updateBoard(GtkWidget *board, BoardData* bd)
{
	int points[2][25];
	read_board(bd, points);
	update_position_id(bd, points);
	update_pipcount(bd, points);

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		gtk_widget_queue_draw(board);
		updatePieceOccPos(bd);
	}
#endif
}

/* Snowie style editing: we will try to place i chequers of the
   indicated colour on point n.  The x,y coordinates will be used to
   map to a point n and a checker position i.

   Clicking on a point occupied by the opposite color clears the point
   first.  If not enough chequers are available in the bearoff tray,
   we try to add what we can.  So if there are no chequers in the
   bearoff tray, no chequers will be added.  This may be a point of
   confusion during usage.  Clicking on the outside border of a point
   corresponds to i=0, i.e. remove all chequers from that point. */
void board_quick_edit(GtkWidget *board, BoardData *bd, int x, int y, int dragging)
{
    int current, delta, c_chequer;
    int off, opponent_off;
    int bar, opponent_bar;
    int n, i;

	int colour = bd->drag_button == 1 ? 1 : -1;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
		n = BoardPoint3d(bd, x, y, -1);
    else
#endif
    /* Map (x,y) to a point from 0..27 using a version of
       board_point() that ignores the dice and cube and which allows
       clicking on a narrow border */
    n = board_point_with_border( board, bd, x, y );

    if (!dragging && (n == POINT_UNUSED0 || n == POINT_UNUSED1 || n == 26 || n == 27))
	{
		if (n == 26 || n == 27)
		{	/* click on bearoff tray in edit mode -- bear off all chequers */
			for( i = 0; i < 26; i++ )
			{
				bd->points[i] = 0;
			}
			bd->points[26] = bd->nchequers;
			bd->points[27] = -bd->nchequers;
		}
		else /* if (n == POINT_UNUSED0 || n == POINT_UNUSED1) */
		{	/* click on unused bearoff tray in edit mode -- reset to starting position */
			int anBoard[ 2 ][ 25 ];
			InitBoard( anBoard, ms.bgv );
			write_board( bd, anBoard );
		}
#if USE_BOARD3D
		if (rdAppearance.fDisplayType == DT_2D)
#endif
			for( i = 0; i < 28; i++ )
				board_invalidate_point( bd, i );

		updateBoard(board, bd);
    }

    /* Only points or bar allowed */
    if (n < 0 || n > 25)
		return;

    /* Make sure that if we drag across a bar, we started on that bar.
       This is to make sure that if you drag a prime across say point
       4 to point 9, you don't inadvertently add chequers to the bar */
    if (dragging && (n == 0 || n == 25) && n != bd->qedit_point)
		return;

	bd->qedit_point = n;

    off          = (colour == 1) ? 26 : 27;
    opponent_off = (colour == 1) ? 27 : 26;

    bar          = (colour == 1) ? 25 : 0;
    opponent_bar = (colour == 1) ? 0 : 25;

    /* Can't add checkers to the wrong bar */
    if( n == opponent_bar )
		return;

    c_chequer = (n == 0 || n == 25) ? 3 : 5;

    current = abs(bd->points[n]);
    
    /* Given n, map (x, y) to the ith checker position on point n*/
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
		i = BoardPoint3d(bd, x, y, n);
	else
#endif
	    i = board_chequer_number( board, bd, n, x, y );
    if( i < 0 )
		return;

    /* We are at the maximum position in the point, so we should not
       respond to dragging.  Each click will try to add one more
       chequer */
    if (!dragging && i >= c_chequer && current >= c_chequer)
		i = current + 1;
    
    /* Clear chequers of the other colour from this point */
    if (current && (bd->points[n] / abs(bd->points[n])) != colour)
	{
		bd->points[opponent_off] += current * -colour;
		bd->points[n] = 0;
		current = 0;

#if USE_BOARD3D
		if (rdAppearance.fDisplayType == DT_3D)
		{
			gtk_widget_queue_draw(board);
			updatePieceOccPos(bd);
		}
		else
#endif
		{
			board_invalidate_point( bd, n );
			board_invalidate_point( bd, opponent_off );
		}
    }

    delta = i - current;
    
    /* No chequers of our colour added or removed */
    if (delta == 0)
		return;

    if (delta < 0)
	{	/* Need to remove some chequers of the same colour */
		bd->points[off] -= delta * colour;
		bd->points[n] += delta * colour;
    }
	else
	{
		int mv, avail = abs(bd->points[off]);
		/* any free chequers? */
		if (!avail)
			return;
		/* move up to delta chequers, from avail chequers to point n */
		mv = (avail > delta) ? delta : avail;

		bd->points[off] -= mv * colour;
		bd->points[n] += mv * colour;
    }

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_2D)
#endif
	{
	    board_invalidate_point(bd, n);
		board_invalidate_point(bd, off);
	}

	updateBoard(board, bd);
}

int ForcedMove ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] ) {

  movelist ml;

  GenerateMoves ( &ml, anBoard, anDice[ 0 ], anDice[ 1 ], FALSE );

  if ( ml.cMoves == 1 ) {

    ApplyMove ( anBoard, ml.amMoves[ 0 ].anMove, TRUE );
    return TRUE;
     
  }
  else
    return FALSE;

}

int GreadyBearoff ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] ) {

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



extern int
UpdateMove( BoardData *bd, int anBoard[ 2 ][ 25 ] ) {

  int old_points[ 28 ];
  int i, j;
  int an[ 28 ];
  int rc;

  memcpy( old_points, bd->points, sizeof old_points );

  write_board ( bd, anBoard );

  for ( i = 0, j = 0; i < 28; ++i )
    if ( old_points[ i ] != bd->points[ i ] )
      an[ j++ ] = i;

  if ( ( rc = update_move ( bd ) ) )
    /* illegal move */
    memcpy( bd->points, old_points, sizeof old_points );

	/* Show move */
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		updatePieceOccPos(bd);
		gtk_widget_queue_draw(bd->drawing_area3d);
	}
	else
#endif
	{
		for ( i = 0; i < j; ++i )
			board_invalidate_point( bd, an[ i ] );
	}

  return rc;

}

gboolean button_press_event(GtkWidget *board, GdkEventButton *event, BoardData* bd)
{
	int x = event->x;
	int y = event->y;
	int editing = ToolbarIsEditing( pwToolbar );
	int numOnPoint;

	/* Ignore double-clicks and multiple presses and any clicks when not playing */
	if (event->type != GDK_BUTTON_PRESS || bd->drag_point >= 0 || !bd->playing)
		return TRUE;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{	/* Ignore clicks when animating */
		if (bd->shakingDice || bd->moving)
				return TRUE;

		/* Reverse screen y coords for openGL */
		y = board->allocation.height - y;

		bd->drag_point = BoardPoint3d(bd, x, y, -1);
	}
	else
#endif
	if (editing && !(event->state & GDK_CONTROL_MASK))
		bd->drag_point = board_point_with_border(board, bd, x, y);
	else
		bd->drag_point = board_point(board, bd, x, y);

	bd->click_time = gdk_event_get_time((GdkEvent*)event);
	bd->drag_button = event->button;
	bd->DragTargetHelp = 0;

	/* Set dice in editing mode */
    if (editing && (bd->drag_point == POINT_DICE ||
		bd->drag_point == POINT_LEFT || bd->drag_point == POINT_RIGHT))
	{
		if (bd->drag_point == POINT_LEFT && bd->turn != 0)
			UserCommand( "set turn 0" );
		else if (bd->drag_point == POINT_RIGHT && bd->turn != 1)
			UserCommand( "set turn 1" );

		/* -2 to avoid motion causing immediate edit */
		bd->drag_point = -2;
		GTKSetDice( NULL, 0, NULL );
		return TRUE;
	}

	switch (bd->drag_point)
	{
	case -1:
	    /* Click on illegal area. */
	    board_beep(bd);
	    return TRUE;

	case POINT_CUBE:
	    /* Clicked on cube; double. */
	    bd->drag_point = -1;
	    
            if(editing)
              GTKSetCube(NULL, 0, NULL);
            else if (bd->doubled) {
              switch( event->button ) {
              case 1:
                /* left */
                UserCommand( "take" );
                break;
              case 2:
                /* center */
                if ( ! bd->match_to )
                  UserCommand( "redouble" );
                else
                  UserCommand( "take" );
                break;
              case 3:
              default:
                /* right */
                UserCommand( "drop" );
                break;

              }
            }
            else
              UserCommand("double");
	    
	    return TRUE;

	case POINT_RESIGN:
#if USE_BOARD3D
		if (rdAppearance.fDisplayType == DT_3D)
		{
			StopIdle3d(bd);
			/* clicked on resignation symbol */
			updateFlagOccPos(bd);
		}
#endif

		bd->drag_point = -1;
		if (bd->resigned && !editing)
                  UserCommand(event->button == 1 ? "accept" : "reject" );

		return TRUE;

	case POINT_DICE:
#if USE_BOARD3D
		if (rdAppearance.fDisplayType == DT_3D && bd->diceShown == DICE_BELOW_BOARD)
		{	/* Click on dice (below board) - shake */
			bd->drag_point = -1;
			UserCommand("roll");
			return TRUE;
		}
#endif
	    bd->drag_point = -1;
		if (event->button == 1)
		{	/* Clicked on dice; end move. */
			Confirm(bd);
		}
	    else 
		{	/* Other buttons on dice swaps positions. */
			swap(bd->diceRoll, bd->diceRoll + 1);

			/* Display swapped dice */
#if USE_BOARD3D
			if (rdAppearance.fDisplayType == DT_3D)
				gtk_widget_queue_draw(board);
			else
#endif
				board_invalidate_dice( bd );
	    }
	    return TRUE;

	case POINT_LEFT:
	case POINT_RIGHT:
	/* If playing and dice not rolled yet, this code handles
	   rolling the dice if bottom player clicks the right side of
	   the board, or the top player clicks the left side of the
	   board (his/her right side). */
		if (bd->diceShown == DICE_BELOW_BOARD &&
			((bd->drag_point == POINT_RIGHT && bd->turn == 1) ||
			(bd->drag_point == POINT_LEFT && bd->turn == -1)))
		{
			/* NB: the UserCommand() call may cause reentrancies,
			   so it is vital to reset bd->drag_point first! */
			bd->drag_point = -1;
			UserCommand("roll");
			return TRUE;
		}
		bd->drag_point = -1;
		return TRUE;

	default:	/* Points */
		/* Don't let them move chequers unless the dice have been
		   rolled, or they're editing the board. */
		if (bd->diceShown != DICE_ON_BOARD && !editing)
		{
			board_beep(bd);
			bd->drag_point = -1;
			
			return TRUE;
		}
		if (editing && !(event->state & GDK_CONTROL_MASK))
		{
			board_quick_edit(board, bd, x, y, 0);
		    bd->drag_point = -1;
		    return TRUE;
		}

		if ((bd->drag_point == POINT_UNUSED0) || (bd->drag_point == POINT_UNUSED1))
		{	/* Clicked in un-used bear-off tray */
			bd->drag_point = -1;
			return TRUE;
		}

		/* if nDragPoint is 26 or 27 (i.e. off), bear off as many chequers as possible. */
        if (!editing && bd->drag_point == (53 - bd->turn) / 2)
		{
          /* user clicked on bear-off tray: try to bear-off chequers or
             show forced move */
          int anBoard[ 2 ][ 25 ];
          
          memcpy ( anBoard, ms.anBoard, sizeof anBoard );

          bd->drag_colour = bd->turn;
          bd->drag_point = -1;
          
          if (ForcedMove(anBoard, bd->diceRoll) ||
               GreadyBearoff(anBoard, bd->diceRoll)) 
		  {
            /* we've found a move: update board  */
            if ( UpdateMove( bd, anBoard ) ) {
              /* should not happen as ForcedMove and GreadyBearoff
                 always return legal moves */
              assert(FALSE);
            }
          }

          return TRUE;
        }

		/* How many chequers on clicked point */
		numOnPoint = bd->points[bd->drag_point];

	    /* Click on an empty point or opponent blot; try to make the point. */
		if (!editing && bd->drag_point <= 24 &&
			(numOnPoint == 0 || numOnPoint == -bd->turn))
		{
			int n[2], bar, i;
			int old_points[ 28 ], points[ 2 ][ 25 ];
			unsigned char key[ 10 ];
			
			memcpy( old_points, bd->points, sizeof old_points );
			
			bd->drag_colour = bd->turn;
			bar = bd->drag_colour == bd->colour ? 25 - bd->bar : bd->bar;
			
			if( bd->diceRoll[ 0 ] == bd->diceRoll[ 1 ] ) {
			/* Rolled a double; find the two closest chequers to make
			   the point. */
			int c = 0;

			n[ 0 ] = n[ 1 ] = -1;
			
			for( i = 0; i < 4 && c < 2; i++ )
			{
				int j = bd->drag_point + bd->diceRoll[ 0 ] * bd->drag_colour *
				( i + 1 );

				if (j < 0 || j > 25)
					break;

				while( c < 2 && bd->points[ j ] * bd->drag_colour > 0 )
				{
					/* temporarily take chequer, so it's not used again */
					bd->points[ j ] -= bd->drag_colour;
					n[ c++ ] = j;
				}
			}
			
			/* replace chequers removed above */
			for (i = 0; i < c; i++)
				bd->points[n[i]] += bd->drag_colour;
			}
			else
			{
				/* Rolled a non-double; take one chequer from each point
				   indicated by the dice. */
				n[ 0 ] = bd->drag_point + bd->diceRoll[ 0 ] * bd->drag_colour;
				n[ 1 ] = bd->drag_point + bd->diceRoll[ 1 ] * bd->drag_colour;
			}
			
			if (n[ 0 ] >= 0 && n[ 0 ] <= 25 && n[ 1 ] >= 0 && n[ 1 ] <= 25 &&
				bd->points[ n[ 0 ] ] * bd->drag_colour > 0 &&
				bd->points[ n[ 1 ] ] * bd->drag_colour > 0 )
			{
				/* the point can be made */
				if( bd->points[ bd->drag_point ] )
					/* hitting the opponent in the process */
					bd->points[bar] -= bd->drag_colour;
			
				bd->points[ n[ 0 ] ] -= bd->drag_colour;
				bd->points[ n[ 1 ] ] -= bd->drag_colour;
				bd->points[ bd->drag_point ] = bd->drag_colour << 1;
			
				read_board( bd, points );
				PositionKey( points, key );

				if (!update_move(bd))
				{	/* Show Move */
#if USE_BOARD3D
					if (rdAppearance.fDisplayType == DT_3D)
					{
						updatePieceOccPos(bd);
						gtk_widget_queue_draw(board);
					}
					else
#endif
					{
						board_invalidate_point( bd, n[ 0 ] );
						board_invalidate_point( bd, n[ 1 ] );
						board_invalidate_point( bd, bd->drag_point );
						board_invalidate_point( bd, bar );
					}
					
					playSound( SOUND_CHEQUER );
				}
				else
				{
					/* the move to make the point wasn't legal; undo it. */
					memcpy( bd->points, old_points, sizeof bd->points );
					update_move( bd );
					board_beep( bd );
				}
			}
			else
				board_beep( bd );
			
			bd->drag_point = -1;
			return TRUE;
		}
		
		if (numOnPoint == 0)
		{	/* clicked on empty point */
			board_beep( bd );
			bd->drag_point = -1;
			return TRUE;
		}

		bd->drag_colour = numOnPoint < 0 ? -1 : 1;

		/* trying to move opponent's chequer  */
		if (!editing && bd->drag_colour != bd->turn)
		{
			board_beep( bd );
			bd->drag_point = -1;
			return TRUE;
		}

		/* Start Dragging piece */
		board_start_drag(board, bd, bd->drag_point, x, y);
		board_drag(board, bd, x, y);

		return TRUE;
	}
	return FALSE;
}

gboolean button_release_event(GtkWidget *board, GdkEventButton *event, BoardData* bd)
{
	int x = event->x;
	int y = event->y;
	int editing = ToolbarIsEditing( pwToolbar );
	int dest;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{	/* Reverse screen y coords for openGL */
		y = board->allocation.height - y;
	}
#endif

	if (bd->drag_point < 0)
		return TRUE;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		dest = BoardPoint3d(bd, x, y, -1);
	}
	else
#endif
	{
		board_end_drag( board, bd );

		/* undo drag target help */
		if (fGUIDragTargetHelp && bd->DragTargetHelp)
		{
			int i;
			for ( i = 0; i <= 3; ++i )
			{
				if (bd->iTargetHelpPoints[i] != -1)
					board_invalidate_point(bd, bd->iTargetHelpPoints[i]);
			}
		}

		dest = board_point(board, bd, x, y);
	}
	bd->DragTargetHelp = 0;

	if (!editing && dest == bd->drag_point &&
		gdk_event_get_time( (GdkEvent*)event ) - bd->click_time < CLICK_TIME)
	{	/* Automatically place chequer on destination point */
		if( bd->drag_colour != bd->turn )
		{
			/* can't move the opponent's chequers */
			board_beep(bd);
			dest = bd->drag_point;
			place_chequer_or_revert(bd, dest);
		}
		else
		{
			gint left = bd->diceRoll[0];
			gint right = bd->diceRoll[1];

			/* Button 1 tries the left roll first.
			   other buttons try the right dice roll first */
			dest = bd->drag_point - ( bd->drag_button == 1 ? 
						  left :
						  right ) * bd->drag_colour;

			if( ( dest <= 0 ) || ( dest >= 25 ) )
				/* bearing off */
				dest = bd->drag_colour > 0 ? 26 : 27;
			
			if( place_chequer_or_revert(bd, dest ) )
				playSound( SOUND_CHEQUER );
			else
			{
				/* First roll was illegal.  We are going to 
				   try the second roll next. First, we have 
				   to redo the pickup since we got reverted. */
				bd->points[ bd->drag_point ] -= bd->drag_colour;
#if USE_BOARD3D
				if (rdAppearance.fDisplayType == DT_2D)
#endif
					board_invalidate_point(bd, bd->drag_point);
				
				/* Now we try the other die roll. */
				dest = bd->drag_point - ( bd->drag_button == 1 ?
							  right :
							  left ) * bd->drag_colour;
				
				if( ( dest <= 0 ) || ( dest >= 25 ) )
				/* bearing off */
				dest = bd->drag_colour > 0 ? 26 : 27;
				
				if (place_chequer_or_revert(bd, dest))
					playSound( SOUND_CHEQUER );
				else 
				{
#if USE_BOARD3D
					if (rdAppearance.fDisplayType == DT_2D)
#endif
						board_invalidate_point(bd, bd->drag_point);
					board_beep(bd);
				}
			}
		}
	}
	else
	{	/* This is from a normal drag release */
		if (place_chequer_or_revert(bd, dest))
			playSound(SOUND_CHEQUER);
		else
			board_beep(bd);
	}

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{
		/* Update 3d Display */
		updatePieceOccPos(bd);
		gtk_widget_queue_draw(board);
	}
#endif

	bd->drag_point = -1;

	return TRUE;
}

gboolean motion_notify_event(GtkWidget *board, GdkEventMotion *event, BoardData* bd)
{
	int x = event->x;
	int y = event->y;
	int editing = ToolbarIsEditing( pwToolbar );

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{	/* Reverse screen y coords for openGL */
		y = board->allocation.height - y;
	}
#endif

	/* In quick editing mode, dragging across points */
	if (editing && !(event->state & GDK_CONTROL_MASK) && bd->drag_point == -1)
	{
		board_quick_edit(board, bd, x, y, 1);
	    return TRUE;
	}

	if (bd->drag_point < 0)
		return TRUE;

	if (fGUIDragTargetHelp && !bd->DragTargetHelp && !editing)
	{	/* Decide if drag targets should be shown (shown after small pause) */
		if ((ap[bd->drag_colour == -1 ? 0 : 1].pt == PLAYER_HUMAN)		/* not for computer turn */
			&& gdk_event_get_time((GdkEvent*)event) - bd->click_time > CLICK_TIME)
		{
			bd->DragTargetHelp = LegalDestPoints(bd, bd->iTargetHelpPoints);
		}
	}
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_2D)
#endif
	{
		if (bd->DragTargetHelp)
		{	/* Display 2d drag target help */
			gint i, ptx, pty, ptcx, ptcy;
			GdkColor *TargetHelpColor;
			
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
				if ( bd->iTargetHelpPoints[i] != -1 ) {
					/* calculate region coordinates for point */
					point_area( bd, bd->iTargetHelpPoints[i], &ptx, &pty, &ptcx, &ptcy );
					gdk_draw_rectangle( board->window, bd->gc_copy, FALSE, ptx + 1, pty + 1, ptcx - 2, ptcy - 2 );
				}
			}
		
			free( TargetHelpColor );
		}
	}

	board_drag(board, bd, x, y);

	return TRUE;
}

static void
score_changed( GtkAdjustment *adj, BoardData *bd ) {

  gchar buf[ 32 ];

  bd->ascore0->upper = bd->ascore1->upper = 
    bd->match_to ? bd->match_to : 65535;
  gtk_adjustment_changed( bd->ascore0 );
  gtk_adjustment_changed( bd->ascore1 );
	
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

}

void RollDice2d(BoardData* bd)
{
	int iAttempt = 0, iPoint, x, y, cx, cy;

	bd->x_dice[ 0 ] = RAND % 21 + 13;
	bd->x_dice[ 1 ] = RAND % ( 34 - bd->x_dice[ 0 ] ) +
	bd->x_dice[ 0 ] + 8;

	if( bd->colour == bd->turn )
	{
		bd->x_dice[ 0 ] += 48;
		bd->x_dice[ 1 ] += 48;
	}

	bd->y_dice[ 0 ] = RAND % 9 + 33;
	bd->y_dice[ 1 ] = RAND % 9 + 33;

	if( rdAppearance.nSize > 0 )
	{
		for( iPoint = 1; iPoint <= 24; iPoint++ )
		{
			if( abs( bd->points[ iPoint ] ) >= 5 )
			{
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
				{	/* Try placing again */
					RollDice2d(bd);
				}
			}
		}
	}
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
    int old_turn;
	int redrawNeeded = 0;
	int dummy;
    
#if __GNUC__
    int *match_settings[] = { &bd->match_to, &bd->score,
			      &bd->score_opponent };
    int *game_settings[] = { &bd->turn, 
		       bd->diceRoll, bd->diceRoll + 1, &dummy, &dummy,
		       &bd->cube, &bd->can_double, &bd->opponent_can_double,
		       &bd->doubled, &bd->colour, &bd->direction,
		       &bd->home, &bd->bar, &bd->off, &bd->off_opponent,
		       &bd->on_bar, &bd->on_bar_opponent, &bd->to_move,
		       &bd->forced, &bd->crawford_game, &bd->redoubles };
    int old_dice[] = {bd->diceRoll[ 0 ], bd->diceRoll[ 1 ]};
#else
    int *match_settings[ 3 ], *game_settings[ 21 ], old_dice[ 2 ];

    match_settings[ 0 ] = &bd->match_to;
    match_settings[ 1 ] = &bd->score;
    match_settings[ 2 ] = &bd->score_opponent;

    game_settings[ 0 ] = &bd->turn;
    game_settings[ 1 ] = bd->diceRoll;
    game_settings[ 2 ] = bd->diceRoll + 1;
    game_settings[ 3 ] = &dummy;
    game_settings[ 4 ] = &dummy;
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

    old_dice[ 0 ] = bd->diceRoll[ 0 ];
    old_dice[ 1 ] = bd->diceRoll[ 1 ];
#endif
    old_turn = bd->turn;
    
    editing = bd->playing && ToolbarIsEditing( pwToolbar );

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

    /* calculate number of chequers */
    
    bd->nchequers = 0;
    for ( i = 0; i < 28; ++i )
      if ( bd->points[ i ] > 0 )
        bd->nchequers += bd->points[ i ];

    if( !editing ) {
	gtk_entry_set_text( GTK_ENTRY( bd->name0 ), bd->name_opponent );
	gtk_entry_set_text( GTK_ENTRY( bd->name1 ), bd->name );
	gtk_label_set_text( GTK_LABEL( bd->lname0 ), bd->name_opponent );
	gtk_label_set_text( GTK_LABEL( bd->lname1 ), bd->name );
    
	if( bd->match_to ) {
	    sprintf( buf, "%d", bd->match_to );
	    gtk_label_set_text( GTK_LABEL( bd->lmatch ), buf );
	} else
	    gtk_label_set_text( GTK_LABEL( bd->lmatch ), _("unlimited") );

        gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score0 ),
                                   bd->score_opponent );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score1 ),
                                   bd->score );
        gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->match ), 
                                   bd->match_to );

        score_changed( NULL, bd );

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
	
    if (fGUIHighDieFirst && bd->diceRoll[ 0 ] < bd->diceRoll[ 1 ] )
	    swap( bd->diceRoll, bd->diceRoll + 1 );

	if (bd->diceRoll[0] != old_dice[0] ||
		bd->diceRoll[1] != old_dice[1] ||
		bd->drag_point == -2)	/* editing */
	{
		redrawNeeded = 1;

		if( bd->x_dice[ 0 ] > 0
#if USE_BOARD3D
			 && rdAppearance.fDisplayType == DT_2D
#endif
			)
		{
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

		if (bd->diceRoll[0] <= 0)
		{	/* Dice not on board */
			bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;

			if (bd->diceRoll[ 0 ] == 0)
			{
				bd->diceShown = DICE_BELOW_BOARD;
				/* Keep showing shaken values */
				bd->diceRoll[ 0 ] = old_dice[ 0 ];
				bd->diceRoll[ 1 ] = old_dice[ 1 ];
			}
			else
				bd->diceShown = DICE_NOT_SHOWN;
		}
		else
		{
#if USE_BOARD3D
			if (rdAppearance.fDisplayType == DT_3D)
			{
				/* Has been shaken, so roll dice (if not editing) */
				if (bd->diceShown != DICE_ON_BOARD && !ToolbarIsEditing( pwToolbar ))
				{
					bd->diceShown = DICE_ON_BOARD;
					RollDice3d(bd);
					redrawNeeded = 0;
				}
				else
				{	/* Move dice to new position */
					setDicePos(bd);
				}
			}
			else
#endif
			{
				RollDice2d(bd);
			}
			bd->diceShown = DICE_ON_BOARD;
		}
	}

    if (bd->diceShown == DICE_ON_BOARD )
	{
		GenerateMoves( &bd->move_list, bd->old_board,
				   bd->diceRoll[ 0 ], bd->diceRoll[ 1 ], TRUE );

		/* bd->move_list contains pointers to static data, so we need to
		   copy the actual moves into private storage. */
		if( bd->all_moves )
			free( bd->all_moves );
		bd->all_moves = malloc( bd->move_list.cMoves * sizeof( move ) );
		bd->move_list.amMoves = memcpy( bd->all_moves, bd->move_list.amMoves,
					 bd->move_list.cMoves * sizeof( move ) );
		bd->valid_move = NULL;
    }

    if( rdAppearance.nSize <= 0 )
	return 0;

	if( bd->turn != old_turn )
	  redrawNeeded = 1;

    if( bd->doubled != old_doubled || 
        bd->cube != old_cube ||
	bd->cube_owner != bd->opponent_can_double - bd->can_double ||
	cube_use != bd->cube_use || 
        bd->crawford_game != old_crawford ) {
	int xCube, yCube;
	redrawNeeded = 1;

	bd->cube_owner = bd->opponent_can_double - bd->can_double;
	bd->cube_use = cube_use;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
		SetupViewingVolume3d(bd, &rdAppearance);	/* Cube may be out of top of screen */
	else
#endif
	{
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
	}

    if ( bd->resigned != old_resigned ) {
	int xResign, yResign;

	redrawNeeded = 1;
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
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
		ShowFlag3d(bd);
#endif
    }

    if( fClockwise != rdAppearance.fClockwise ) {
	/* This is complete overkill, but we need to recalculate the
	   board pixmap if the points are numbered and the direction
	   changes. */
	board_free_pixmaps( bd );
	rdAppearance.fClockwise = fClockwise;
	board_create_pixmaps( pwBoard, bd );
	gtk_widget_queue_draw( bd->drawing_area );

        ToolbarSetClockwise( pwToolbar, fClockwise );
        
	return 0;
    }

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D && redrawNeeded)
		gtk_widget_queue_draw(bd->drawing_area3d);
	else
#endif
	{
		for( i = 0; i < 28; i++ )
		if( bd->points[ i ] != old_board[ i ] )
		{
			board_invalidate_point( bd, i );
		}

		if (redrawNeeded)
		{
			board_invalidate_dice( bd );
			board_invalidate_labels( bd );
			board_invalidate_cube( bd );
			board_invalidate_resign( bd );
			board_invalidate_arrow( bd );
		}
	}

    return 0;
}

int convert_point( int i, int player ) {

    if( player )
	return ( i < 0 ) ? 26 : i + 1;
    else
	return ( i < 0 ) ? 27 : 24 - i;
}

int animate_player, *animate_move_list, animation_finished;

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
	
    if (animGUI == ANIMATE_NONE || ms.fResigned)
	return;

    animate_move_list = move;
    animate_player = player;

    animation_finished = FALSE;

#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
		AnimateMove3d(board->board_data);
	else
#endif
{    
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
}

extern void board_set_playing( Board *board, gboolean f ) {

    BoardData *pbd = board->board_data;

    pbd->playing = f;
    gtk_widget_set_sensitive( pbd->position_id, f );
    gtk_widget_set_sensitive( pbd->match_id, f );
}

static void update_buttons( BoardData *pbd ) {

	toolbarcontrol c;

  c = ToolbarUpdate( pwToolbar, &ms, pbd->diceShown, 
                     pbd->computer_turn, pbd->playing );

    if ( fGUIDiceArea 
#if USE_BOARD3D
    && (rdAppearance.fDisplayType == DT_2D)
#endif
     ) {
      if ( c == C_ROLLDOUBLE ) 
	gtk_widget_show_all( pbd->dice_area );
      else 
	gtk_widget_hide( pbd->dice_area );

    } 
}

extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1,
		      gint computer_turn, gint nchequers ) {
    gchar board_str[ 256 ];
    BoardData *pbd = board->board_data;
    int old_points[ 2 ][ 25 ];
    
    /* Treat a reset of the position to old_board as a no-op while
       in edit mode. */
    if( ToolbarIsEditing( pwToolbar ) &&
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
    }

    update_buttons( pbd );

#if USE_BOARD3D
if (rdAppearance.fDisplayType == DT_3D)
{
	gtk_widget_queue_draw(pbd->drawing_area3d);
	updateOccPos(pbd);	/* Make sure shadows are in correct place */
}
else
#endif
{
    /* FIXME it's overkill to do this every time, but if we don't do it,
       then "set turn <player>" won't redraw the dice in the other colour. */
    gtk_widget_queue_draw( pbd->dice_area );
}

    return 0;
}

/* Create all of the size/colour-dependent pixmaps. */
extern void board_create_pixmaps( GtkWidget *board, BoardData *bd ) {

    unsigned char auch[ 20 * 20 * 3 ],
	auchBoard[ BOARD_WIDTH * 3 * BOARD_HEIGHT * 3 * 3 ],
	auchChequers[ 2 ][ CHEQUER_WIDTH * 3 * CHEQUER_HEIGHT * 3 * 4 ];
    unsigned short asRefract[ 2 ][ CHEQUER_WIDTH * 3 * CHEQUER_HEIGHT * 3 ];
    int i, nSizeReal;

#if USE_BOARD3D
	int j;
	double aarColourTemp[ 2 ][ 4 ];
	double aanBoardColourTemp[4];
	double arCubeColourTemp[4];
	double aarDiceColourTemp[ 2 ][ 4 ];
	double aarDiceDotColourTemp[ 2 ][ 4 ];

	if (rdAppearance.fDisplayType == DT_3D)
	{	/* As settings currently separate, copy 3d colours so 2d dialog colours are correct */
		memcpy(aarColourTemp, rdAppearance.aarColour, sizeof(rdAppearance.aarColour));
		memcpy(aanBoardColourTemp, rdAppearance.aanBoardColour[0], sizeof(double[4]));
		memcpy(arCubeColourTemp, rdAppearance.arCubeColour, sizeof(rdAppearance.arCubeColour));
		memcpy(aarDiceColourTemp, rdAppearance.aarDiceColour, sizeof(rdAppearance.aarDiceColour));
		memcpy(aarDiceDotColourTemp, rdAppearance.aarDiceDotColour, sizeof(rdAppearance.aarDiceDotColour));

		for (j = 0; j < 4; j++)
		{
			rdAppearance.aanBoardColour[0][j] = rdAppearance.rdBaseMat.ambientColour[j] * 0xff;
			rdAppearance.arCubeColour[j] = rdAppearance.rdCubeMat.ambientColour[j];

			for (i = 0; i < 2; i++)
			{
				rdAppearance.aarColour[i][j] = rdAppearance.rdChequerMat[i].ambientColour[j];
				rdAppearance.aarDiceColour[i][j] = rdAppearance.rdDiceMat[i].ambientColour[j];
				rdAppearance.aarDiceDotColour[i][j] = rdAppearance.rdDiceDotMat[i].ambientColour[j];
			}
		}
	}
#endif

    RenderImages( &rdAppearance, &bd->ri );
    nSizeReal = rdAppearance.nSize;
    rdAppearance.nSize = 3;
    RenderBoard( &rdAppearance, auchBoard, BOARD_WIDTH * 3 );
    RenderChequers( &rdAppearance, auchChequers[ 0 ], auchChequers[ 1 ],
		    asRefract[ 0 ], asRefract[ 1 ], CHEQUER_WIDTH * 3 * 4 );
    rdAppearance.nSize = nSizeReal;

    for( i = 0; i < 2; i++ ) {
	CopyArea( auch, 20 * 3,
		  auchBoard + 3 * 3 * BOARD_WIDTH * 3 + 3 * 3 * 3,
		  BOARD_WIDTH * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3, 20 * 3,
		  auchBoard + 3 * 3 * BOARD_WIDTH * 3 + 3 * 3 * 3,
		  BOARD_WIDTH * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3 * 20, 20 * 3,
		  auchBoard + 3 * 3 * BOARD_WIDTH * 3 + 3 * 3 * 3,
		  BOARD_WIDTH * 3 * 3, 10, 10 );
	CopyArea( auch + 10 * 3 + 10 * 3 * 20, 20 * 3,
		  auchBoard + 3 * 3 * BOARD_WIDTH * 3 + 3 * 3 * 3,
		  BOARD_WIDTH * 3 * 3, 10, 10 );

	RefractBlend( auch + 20 * 3 + 3, 20 * 3,
		      auchBoard + 3 * 3 * BOARD_WIDTH * 3 + 3 * 3 * 3,
		      BOARD_WIDTH * 3 * 3,
		      auchChequers[ i ], CHEQUER_WIDTH * 3 * 4,
		      asRefract[ i ], CHEQUER_WIDTH * 3,
		      CHEQUER_WIDTH * 3, CHEQUER_HEIGHT * 3 );
	
	gdk_draw_rgb_image( bd->appmKey[ i ], bd->gc_copy, 0, 0, 20, 20,
			    GDK_RGB_DITHER_MAX, auch, 20 * 3 );
    }
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{	/* Restore 2d colours */
		memcpy(rdAppearance.aarColour, aarColourTemp, sizeof(rdAppearance.aarColour));
		memcpy(rdAppearance.aanBoardColour[0], aanBoardColourTemp, sizeof(double[4]));
		memcpy(rdAppearance.arCubeColour, arCubeColourTemp, sizeof(rdAppearance.arCubeColour));
		memcpy(rdAppearance.aarDiceColour, aarDiceColourTemp, sizeof(rdAppearance.aarDiceColour));
		memcpy(rdAppearance.aarDiceDotColour, aarDiceDotColourTemp, sizeof(rdAppearance.aarDiceDotColour));
	}
#endif
}

extern void board_free_pixmaps( BoardData *bd ) {

    FreeImages( &bd->ri );
}

#if USE_BOARD3D

void DisplayCorrectBoardType()
{
	BoardData* bd = BOARD(pwBoard )->board_data;

	if (rdAppearance.fDisplayType == DT_3D)
	{
		gtk_widget_hide(bd->drawing_area);
		gtk_widget_show(bd->drawing_area3d);
		gtk_widget_queue_draw( bd->drawing_area3d );
		updateHingeOccPos(bd);
	}
	else
	{
		gtk_widget_hide(bd->drawing_area3d);
		gtk_widget_show(bd->drawing_area);
		gtk_widget_queue_draw( bd->drawing_area );
	}
}
#endif

static void board_size_allocate( GtkWidget *board,
				 GtkAllocation *allocation ) {
    BoardData *bd = BOARD( board )->board_data;
    gint old_size = rdAppearance.nSize, new_size;
    GtkAllocation child_allocation;
    GtkRequisition requisition;
    int cx;
    
    memcpy( &board->allocation, allocation, sizeof( GtkAllocation ) );

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
    if ( fGUIShowGameInfo ) {
      gtk_widget_get_child_requisition( bd->table, &requisition );
      allocation->height -= requisition.height;
      child_allocation.x = allocation->x;
      child_allocation.y = allocation->y + allocation->height;
      child_allocation.width = allocation->width;
      child_allocation.height = requisition.height;
      gtk_widget_size_allocate( bd->table, &child_allocation );
    }

    /* ensure there is room for the dice area or the move, whichever is
       bigger */
    if ( fGUIDiceArea 
#if USE_BOARD3D
    && (rdAppearance.fDisplayType == DT_2D)
#endif
	) {
      new_size = MIN( allocation->width / BOARD_WIDTH,
                      ( allocation->height - 2 ) / 89 );    /* FIXME: is 89 correct? */

      /* subtract pixels used */
      allocation->height -= new_size * DIE_HEIGHT + 2;

    }
    else {
      new_size = MIN( allocation->width / BOARD_WIDTH,
                      ( allocation->height - 2 ) / BOARD_HEIGHT );
    }
    /* If the window manager honours our minimum size this won't happen, but... */
	if (new_size < 1)
		new_size = 1;
    
    if( ( rdAppearance.nSize = new_size ) != old_size &&
	GTK_WIDGET_REALIZED( board ) ) {
	board_free_pixmaps( bd );
	board_create_pixmaps( board, bd );
    }

#if USE_BOARD3D
	child_allocation.width = allocation->width;
	child_allocation.height = allocation->height;
	child_allocation.x = allocation->x;
	child_allocation.y = allocation->y;
	gtk_widget_size_allocate( bd->drawing_area3d, &child_allocation );
#endif

    child_allocation.width = BOARD_WIDTH * rdAppearance.nSize;
    cx = child_allocation.x = allocation->x + ( ( allocation->width -
					     child_allocation.width ) >> 1 );
    child_allocation.height = BOARD_HEIGHT * rdAppearance.nSize;
    child_allocation.y = allocation->y + ( ( allocation->height -
					     BOARD_HEIGHT * rdAppearance.nSize ) >> 1 );
    gtk_widget_size_allocate( bd->drawing_area, &child_allocation );

    /* allocation for dice area */

    if ( fGUIDiceArea
#if USE_BOARD3D
    && (rdAppearance.fDisplayType == DT_2D)
#endif
	) {
      child_allocation.width = ( 2 * DIE_WIDTH + 1 ) * rdAppearance.nSize;
      child_allocation.x += ( BOARD_WIDTH - ( 2 * DIE_WIDTH + 1 ) ) *
			    rdAppearance.nSize;
      child_allocation.height = DIE_HEIGHT * rdAppearance.nSize;
      child_allocation.y += BOARD_HEIGHT * rdAppearance.nSize + 1;
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

    if ( fGUIShowIDs )
      AddChild( bd->vbox_ids, pr );

    AddChild( bd->table, pr );

    if ( fGUIDiceArea )
      pr->height += DIE_HEIGHT;

    if( pr->width < BOARD_WIDTH )
	pr->width = BOARD_WIDTH;

    pr->height += BOARD_HEIGHT + 2;
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

void board_edit( BoardData *bd ) {

    int f = ToolbarIsEditing( pwToolbar );
									
    update_move( bd );
    update_buttons( bd );
    
    if( f ) {
	/* Entering edit mode: enable entry fields for names and scores */
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname0 ), bd->name0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname1 ), bd->name1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore0 ), bd->score0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore1 ), bd->score1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mmatch ), bd->match );
    } else {
	/* Editing complete; set board. */
        int points[ 2 ][ 25 ], anScoreNew[ 2 ], nMatchToNew;
	const char *pch0, *pch1;
	char sz[ 64 ]; /* "set board XXXXXXXXXXXXXX" */

	/* We need to query all the widgets before issuing any commands,
	   since those commands have side effects which disturb other
	   widgets. */
	pch0 = gtk_entry_get_text( GTK_ENTRY( bd->name0 ) );
	pch1 = gtk_entry_get_text( GTK_ENTRY( bd->name1 ) );
	anScoreNew[ 0 ] = GTK_SPIN_BUTTON( bd->score0 )->adjustment->value;
	anScoreNew[ 1 ] = GTK_SPIN_BUTTON( bd->score1 )->adjustment->value;
        nMatchToNew = GTK_SPIN_BUTTON( bd->match )->adjustment->value;
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

	if( bd->playing && !EqualBoards( ms.anBoard, points ) ) {
	    sprintf( sz, "set board %s", PositionID( points ) );
	    UserCommand( sz );
	}

        if ( nMatchToNew != ms.nMatchTo ) {
          /* new match length; issue "set matchid ..." command */
          gchar *sz;
          int i;

          if ( nMatchToNew )
            for ( i = 0; i < 2; ++i )
              if ( anScoreNew[ i ] > nMatchToNew )
                anScoreNew[ i ] = 0;
          sz = g_strdup_printf( "set matchid %s", 
                                MatchID(bd->diceRoll,
                                         ms.fTurn,
                                         ms.fResigned,
                                         ms.fDoubled,
                                         ms.fMove,
                                         ms.fCubeOwner,
                                         bd->crawford_game,
                                         nMatchToNew,
                                         anScoreNew,
                                         bd->cube,
                                         ms.gs ) );
          UserCommand( sz );
          g_free( sz );

        }
        else if( anScoreNew[ 0 ] != ms.anScore[ 0 ] ||
	    anScoreNew[ 1 ] != ms.anScore[ 1 ] ) {
          /* only score changed; no need for "set matchid ..." command */
          sprintf( sz, "set score %d %d", anScoreNew[ 0 ],
                   anScoreNew[ 1 ] );
          UserCommand( sz );
	}

	outputresume();

	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname0 ), bd->lname0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mname1 ), bd->lname1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore0 ), bd->lscore0 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mscore1 ), bd->lscore1 );
	gtk_multiview_set_current( GTK_MULTIVIEW( bd->mmatch ), 
                                   bd->lmatch );
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

extern void
DrawDie( GdkDrawable *pd, 
         unsigned char *achDice[ 2 ], unsigned char *achPip[ 2 ],
         const int s, GdkGC *gc, int x, int y, int fColour, int n ) {

    int ix, iy, afPip[ 9 ];

    DrawAlphaImage( pd, x, y, achDice[ fColour ], DIE_WIDTH * s * 4,
		    DIE_WIDTH * s, DIE_HEIGHT * s );
    
    afPip[ 0 ] = afPip[ 8 ] = ( n == 2 ) || ( n == 3 ) || ( n == 4 ) ||
	( n == 5 ) || ( n == 6 );
    afPip[ 1 ] = afPip[ 7 ] = 0;
    afPip[ 2 ] = afPip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 );
    afPip[ 3 ] = afPip[ 5 ] = n == 6;
    afPip[ 4 ] = n & 1;

    for( iy = 0; iy < 3; iy++ )
	for( ix = 0; ix < 3; ix++ )
	    if( afPip[ iy * 3 + ix ] )
		gdk_draw_rgb_image( pd, gc, x + s + 2 * s * ix,
				    y + s + 2 * s * iy, s, s,
				    GDK_RGB_DITHER_MAX,
				    achPip[ fColour ], s * 3 );
}

static gboolean dice_expose( GtkWidget *dice, GdkEventExpose *event,
                             BoardData *bd ) {

	if (rdAppearance.nSize <= 0 || event->count || bd->diceShown == DICE_NOT_SHOWN)
		return TRUE;

    DrawDie( dice->window, bd->ri.achDice, bd->ri.achPip,
	     rdAppearance.nSize, bd->gc_copy,
             0, 0, bd->turn > 0, bd->diceRoll[ 0 ] );
    DrawDie( dice->window, bd->ri.achDice, bd->ri.achPip,
	     rdAppearance.nSize, bd->gc_copy,
             ( DIE_WIDTH + 1 ) * rdAppearance.nSize, 0,
	     bd->turn > 0, bd->diceRoll[ 1 ] );

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

static void board_init( Board *board ) {

    BoardData *bd = g_malloc( sizeof( *bd ) );
    GdkColormap *cmap = gtk_widget_get_colormap( GTK_WIDGET( board ) );
    GdkVisual *vis = gtk_widget_get_visual( GTK_WIDGET( board ) );
    GdkGCValues gcval;
    GtkWidget *pw;
    GtkWidget *pwFrame;
    GtkWidget *pwvbox;

    
    board->board_data = bd;
    bd->widget = GTK_WIDGET( board );
	
    bd->drag_point = rdAppearance.nSize = -1;

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

    /* horizontal separator */

    pw = gtk_hseparator_new ();

    /* board drawing area */

    bd->drawing_area = gtk_drawing_area_new();
    /* gtk_widget_set_name(GTK_WIDGET(bd->drawing_area), "background"); */
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->drawing_area ), BOARD_WIDTH,
			   BOARD_HEIGHT );
    gtk_widget_add_events( GTK_WIDGET( bd->drawing_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK | GDK_STRUCTURE_MASK );
    gtk_container_add( GTK_CONTAINER( board ), bd->drawing_area );

#if USE_BOARD3D
	/* 3d board drawing area */
	CreateBoard3d(bd, &bd->drawing_area3d);
	gtk_container_add(GTK_CONTAINER(board), bd->drawing_area3d);
#endif

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
    gtk_box_pack_start( GTK_BOX ( board ), bd->table, FALSE, TRUE, 0 );

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

    bd->ascore0 = GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) );
    bd->score0 = gtk_spin_button_new( GTK_ADJUSTMENT( bd->ascore0 ), 1, 0 );
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
#if USE_TIMECONTROL 
    /* third row: clock */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* clock label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Clock:") ), FALSE, FALSE, 0 );

    /* clock */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->clock0 = gtk_label_new ( NULL ), 
                         FALSE, FALSE, 0 );
#endif

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

    bd->ascore1 = GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) );
    bd->score1 = gtk_spin_button_new( GTK_ADJUSTMENT( bd->ascore1 ), 1, 0 );
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

#if USE_TIMECONTROL 
    /* third row: clock */

    pw = gtk_hbox_new ( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), pw, FALSE, FALSE, 0 );

    /* clock label */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         gtk_label_new ( _("Clock:") ), FALSE, FALSE, 0 );

    /* clock */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         bd->clock1 = gtk_label_new ( NULL ), 
                         FALSE, FALSE, 0 );
#endif


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
                         bd->wmove = gtk_label_new( NULL ),
                         FALSE, FALSE, 0 );
    gtk_widget_set_name( bd->wmove, "move" );

    /* match length */

    gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                         pw = gtk_hbox_new ( FALSE, 0 ),
                         FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pw ),
			gtk_label_new( _("Match:") ), FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ),
                        bd->mmatch = gtk_multiview_new (),
                        FALSE, FALSE, 0 );

    gtk_container_add( GTK_CONTAINER( bd->mmatch ), 
                       bd->lmatch = gtk_label_new( NULL ) );

    bd->amatch = 
      GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, MAXSCORE, 1, 1, 1 ) );
    bd->match = gtk_spin_button_new( GTK_ADJUSTMENT( bd->amatch ), 1, 0 );
    gtk_container_add( GTK_CONTAINER( bd->mmatch ), bd->match );

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
    /* gtk_widget_set_name( GTK_WIDGET(bd->dice_area), "dice_area"); */
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
			GTK_SIGNAL_FUNC( button_press_event ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "button_release_event",
			GTK_SIGNAL_FUNC( button_release_event ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "motion_notify_event",
			GTK_SIGNAL_FUNC( motion_notify_event ), bd );

    gtk_signal_connect( GTK_OBJECT( bd->dice_area ), "expose_event",
			GTK_SIGNAL_FUNC( dice_expose ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->dice_area ), "button_press_event",
			GTK_SIGNAL_FUNC( dice_press ), bd );

    gtk_signal_connect( GTK_OBJECT( bd->amatch ), "value-changed",
                        GTK_SIGNAL_FUNC( score_changed ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->ascore0 ), "value-changed",
                        GTK_SIGNAL_FUNC( score_changed ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->ascore1 ), "value-changed",
                        GTK_SIGNAL_FUNC( score_changed ), bd );

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
    
    puch = g_alloca( CUBE_LABEL_WIDTH * rdAppearance.nSize *
		     CUBE_LABEL_HEIGHT * rdAppearance.nSize * 3 );
    CopyAreaRotateClip( puch, CUBE_LABEL_WIDTH * rdAppearance.nSize * 3, 0, 0,
			CUBE_LABEL_WIDTH * rdAppearance.nSize,
			CUBE_LABEL_HEIGHT * rdAppearance.nSize,
			bd->ri.achCubeFaces,
			CUBE_LABEL_WIDTH * rdAppearance.nSize * 3,
			0, CUBE_LABEL_HEIGHT * rdAppearance.nSize * nValue,
			CUBE_LABEL_WIDTH * rdAppearance.nSize,
			CUBE_LABEL_HEIGHT * rdAppearance.nSize,
			2 - n / 13 );
    
    DrawAlphaImage( cube->window, 0, 0,
		    bd->ri.achCube, CUBE_WIDTH * rdAppearance.nSize * 4,
		    CUBE_WIDTH * rdAppearance.nSize,
		    CUBE_HEIGHT * rdAppearance.nSize );
    gdk_draw_rgb_image( cube->window, bd->gc_copy,
			rdAppearance.nSize, rdAppearance.nSize,
			CUBE_LABEL_WIDTH * rdAppearance.nSize,
			CUBE_LABEL_HEIGHT * rdAppearance.nSize,
			GDK_RGB_DITHER_MAX,
			puch, CUBE_LABEL_WIDTH * rdAppearance.nSize * 3 );

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
				   CUBE_WIDTH * rdAppearance.nSize,
				   CUBE_HEIGHT * rdAppearance.nSize );
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
    
    DrawDie( dice->window, bd->ri.achDice, bd->ri.achPip, rdAppearance.nSize, bd->gc_copy,
             0, 0, bd->turn > 0, n % 6 + 1 );
    DrawDie( dice->window, bd->ri.achDice, bd->ri.achPip, rdAppearance.nSize, bd->gc_copy,
             DIE_WIDTH * rdAppearance.nSize, 0, bd->turn > 0, n / 6 + 1 );
    
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
				   2 * DIE_WIDTH * rdAppearance.nSize,
				   DIE_HEIGHT * rdAppearance.nSize );
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

#if USE_BOARD3D
void InitBoardData(BoardData* bd)
{	/* Initialize some BoardData settings on new game start */
	/* Only needed for 3d board */
	if (rdAppearance.fDisplayType == DT_3D)
	{
		/* Move cube back to center */
		bd->cube = 0;
		bd->cube_owner = 0;
		bd->doubled = 0;

		bd->resigned = 0;

		/* Set dice so 3d roll happens */
		bd->diceShown = DICE_NOT_SHOWN;
		bd->diceRoll[0] = bd->diceRoll[1] = -1;

		updateOccPos(bd);
		updateFlagOccPos(bd);
		SetupViewingVolume3d(bd, &rdAppearance);
	}
}
#endif
