/*
 * board.c
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2000.
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "positionid.h"

#define POINT_DICE 28
#define POINT_CUBE 29

static unsigned int nSeed = 1; /* for rand_r */
#define RAND ( ( (unsigned int) rand_r( &nSeed ) ) & RAND_MAX )

typedef struct _BoardData {
    GtkWidget *drawing_area, *hbox_move, *table, *hbox_match, *move,
	*position_id;
    GdkGC *gc_and, *gc_or, *gc_copy, *gc_cube;
    GdkPixmap *pm_board, *pm_x, *pm_o, *pm_x_dice, *pm_o_dice, *pm_x_pip,
	*pm_o_pip, *pm_cube, *pm_saved, *pm_temp, *pm_temp_saved, *pm_point;
    GdkBitmap *bm_mask, *bm_dice_mask, *bm_cube_mask;
    GdkFont *cube_font;
    gint board_size; /* basic unit of board size, in pixels -- a chequer's
			diameter is 6 of these units (and is 2 units thick) */
    gint drag_point, drag_colour, x_drag, y_drag, x_dice[ 2 ], y_dice[ 2 ],
	dice_colour[ 2 ], cube_font_rotated, old_board[ 2 ][ 25 ];
    gint cube_owner; /* -1 = bottom, 0 = centred, 1 = top */
    move *valid_move;
    movelist move_list;
    
    /* remainder is from FIBS board: data */
    char name[ 32 ], name_opponent[ 32 ];
    gint match_to, score, score_opponent;
    gint points[ 28 ]; /* 0 and 25 are the bars */
    gint turn; /* -1 is X, 1 is O, 0 if game over */
    gint dice[ 2 ], dice_opponent[ 2 ]; /* 0, 0 if not rolled */
    gint cube;
    gint can_double, opponent_can_double; /* allowed to double */
    gint doubled; /* opponent just doubled */
    gint colour; /* -1 for player X, 1 for player O */
    gint direction; /* -1 playing from 24 to 1, 1 playing from 1 to 24 */
    gint home, bar; /* 0 or 25 depending on fDirection */
    gint off, off_opponent; /* number of men borne off */
    gint on_bar, on_bar_opponent; /* number of men on bar */
    gint to_move; /* 0 to 4 -- number of pieces to move */
    gint forced, crawford; /* unused */
    gint redoubles; /* number of instant redoubles allowed */
} BoardData;

static int positions[ 28 ][ 3 ] = {
    { 51, 25, 7 },
    { 90, 63, 6 }, { 84, 63, 6 }, { 78, 63, 6 }, { 72, 63, 6 }, { 66, 63, 6 },
    { 60, 63, 6 }, { 42, 63, 6 }, { 36, 63, 6 }, { 30, 63, 6 }, { 24, 63, 6 },
    { 18, 63, 6 }, { 12, 63, 6 }, { 12, 3, -6 }, { 18, 3, -6 }, { 24, 3, -6 },
    { 30, 3, -6 }, { 36, 3, -6 }, { 42, 3, -6 }, { 60, 3, -6 }, { 66, 3, -6 },
    { 72, 3, -6 }, { 78, 3, -6 }, { 84, 3, -6 }, { 90, 3, -6 },
    { 51, 41, -7 }, { 99, 63, 6 }, { 99, 3, -6 }
};

static GtkVBoxClass *parent_class = NULL;

extern GtkWidget *board_new( void ) {

    return GTK_WIDGET( gtk_type_new( board_get_type() ) );
}

static int intersects( int x0, int y0, int cx0, int cy0,
		       int x1, int y1, int cx1, int cy1 ) {

    return ( y1 + cy1 > y0 ) && ( y1 < y0 + cy0 ) &&
	( x1 + cx1 > x0 ) && ( x1 < x0 + cx0 );
}

static void draw_bitplane( GdkDrawable *drawable, GdkGC *gc, GdkDrawable *src,
			   gint xsrc, gint ysrc, gint xdest, gint ydest,
			   gint width, gint height, gulong plane ) {

    XCopyPlane( GDK_WINDOW_XDISPLAY( drawable ), GDK_WINDOW_XWINDOW( src ),
		GDK_WINDOW_XWINDOW( drawable ), GDK_GC_XGC( gc ), xsrc, ysrc,
		width, height, xdest, ydest, plane );
}

static void board_redraw_point( GtkWidget *board, BoardData *bd, int n ) {

    int i, x, y, cx, cy, invert, y_chequer, c_chequer, i_chequer;
    
    if( bd->board_size <= 0 )
	return;

    c_chequer = ( !n || n == 25 ) ? 3 : 5;
    
    x = positions[ n ][ 0 ] * bd->board_size;
    y = positions[ n ][ 1 ] * bd->board_size;
    cx = 6 * bd->board_size;
    cy = -c_chequer * bd->board_size * positions[ n ][ 2 ];

    if( ( invert = cy < 0 ) ) {
	y += cy * 4 / 5;
	cy = -cy;
    }

    /* FIXME draw straight to screen and return if point is empty */
    gdk_draw_pixmap( bd->pm_point, bd->gc_copy, bd->pm_board, x, y, 0, 0,
		     cx, cy );

    y_chequer = invert ? cy - 6 * bd->board_size : 0;
    
    i_chequer = 0;
    
    for( i = abs( bd->points[ n ] ); i; i-- ) {
	draw_bitplane( bd->pm_point, bd->gc_and, bd->bm_mask, 0, 0, 0,
		       y_chequer, 6 * bd->board_size, 6 * bd->board_size, 1 );

	gdk_draw_pixmap( bd->pm_point, bd->gc_or, bd->points[ n ] > 0 ?
			 bd->pm_o : bd->pm_x, 0, 0, 0, y_chequer,
			 6 * bd->board_size, 6 * bd->board_size );

	y_chequer -= bd->board_size * positions[ n ][ 2 ];

	if( ++i_chequer == c_chequer ) {
	    i_chequer = 0;
	    c_chequer--;
	    
	    y_chequer = invert ? cy + ( 3 * c_chequer - 21 ) * bd->board_size :
		( 15 - 3 * c_chequer ) * bd->board_size;
	}
    }

    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_point, 0, 0, x, y,
		     cx, cy );
}

static void board_redraw_die( GtkWidget *board, BoardData *bd, gint x, gint y,
			      gint colour, gint n ) {
    int pip[ 9 ];
    int ix, iy;
    
    if( bd->board_size <= 0 || !n )
	return;

    draw_bitplane( board->window, bd->gc_and, bd->bm_dice_mask,
		   0, 0, x, y, 7 * bd->board_size, 7 * bd->board_size, 1 );

    gdk_draw_pixmap( board->window, bd->gc_or, colour < 0 ? bd->pm_x_dice :
		     bd->pm_o_dice, 0, 0, x, y, 7 * bd->board_size,
		     7 * bd->board_size );

    pip[ 0 ] = pip[ 8 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
	( ( ( n == 2 ) || ( n == 3 ) ) && x & 1 );
    pip[ 1 ] = pip[ 7 ] = n == 6 && !( x & 1 );
    pip[ 2 ] = pip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 ) ||
	( ( ( n == 2 ) || ( n == 3 ) ) && !( x & 1 ) );
    pip[ 3 ] = pip[ 5 ] = n == 6 && x & 1;
    pip[ 4 ] = n & 1;

    for( iy = 0; iy < 3; iy++ )
	 for( ix = 0; ix < 3; ix++ )
	     if( pip[ iy * 3 + ix ] )
		 gdk_draw_pixmap( board->window, bd->gc_copy, colour < 0 ?
				  bd->pm_x_pip : bd->pm_o_pip, 0, 0,
				  x + ( 1 + 2 * ix ) * bd->board_size,
				  y + ( 1 + 2 * iy ) * bd->board_size,
				  bd->board_size, bd->board_size );
}

static void board_redraw_dice( GtkWidget *board, BoardData *bd, int i ) {

    board_redraw_die( board, bd, bd->x_dice[ i ] * bd->board_size,
		      bd->y_dice[ i ] * bd->board_size,
		      bd->dice_colour[ i ], bd->turn == bd->colour ?
		      bd->dice[ i ] : bd->dice_opponent[ i ] );
}

static void board_redraw_cube( GtkWidget *board, BoardData *bd ) {
    int x, y, two_chars, lbearing[ 2 ], width[ 2 ], ascent[ 2 ], descent[ 2 ];
    char cube_text[ 3 ];

    if( bd->board_size <= 0 )
	return;
    
    x = 50 * bd->board_size;
    y = ( 32 - 29 * bd->cube_owner ) * bd->board_size;

    draw_bitplane( board->window, bd->gc_and, bd->bm_cube_mask, 0, 0, x, y,
		   8 * bd->board_size, 8 * bd->board_size, 1 );
    
    gdk_draw_pixmap( board->window, bd->gc_or, bd->pm_cube, 0, 0, x, y,
		     8 * bd->board_size, 8 * bd->board_size );

    sprintf( cube_text, "%d", bd->cube > 1 && bd->cube < 65 ?
	     bd->cube : 64 );

    two_chars = bd->cube == 1 || bd->cube > 10;
    
    if( bd->cube_font_rotated ) {
	gdk_text_extents( bd->cube_font, cube_text, 1, &lbearing[ 0 ],
			  NULL, NULL, &ascent[ 0 ], &descent[ 0 ] );
	
	if( two_chars )
	    gdk_text_extents( bd->cube_font, cube_text + 1, 1, &lbearing[ 1 ],
			      NULL, NULL, &ascent[ 1 ], &descent[ 1 ] );
	else
	    lbearing[ 1 ] = ascent[ 1 ] = descent[ 1 ] = 0;

	if( bd->cube_owner ) {
	    /* upside down */
	    lbearing[ 1 ] += lbearing[ 0 ];

	    gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x + 4 *
			   bd->board_size - lbearing[ 1 ] / 2,
			   y + 4 * bd->board_size - descent[ 0 ] / 2,
			   cube_text, 1 );

	    if( two_chars )
		gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x +
			       4 * bd->board_size - lbearing[ 1 ] / 2 +
			       lbearing[ 0 ],
			       y + 4 * bd->board_size - descent[ 0 ] / 2,
			       cube_text + 1, 1 );
	} else {
	    /* centred */
	    ascent[ 1 ] += ascent[ 0 ];

	    gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x + 4 *
			   bd->board_size - lbearing[ 0 ] / 2,
			   y + 4 * bd->board_size + ascent[ 1 ] / 2,
			   cube_text, 1 );
	    
	    if( two_chars )
		gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x +
			       4 * bd->board_size - lbearing[ 1 ] / 2,
			       y + 4 * bd->board_size + ( descent[ 1 ] -
							  descent[ 0 ] ) / 2,
			       cube_text + 1, 1 );
	}
    } else {
	gdk_text_extents( bd->cube_font, cube_text, two_chars + 1,
			  NULL, NULL, &width[ 0 ], &ascent[ 0 ], NULL );

	gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x + 4 *
		       bd->board_size - width[ 0 ] / 2,
		       y + 4 * bd->board_size + ascent[ 0 ] / 2,
		       cube_text, two_chars + 1 );
    }
}

static gboolean board_expose( GtkWidget *board, GdkEventExpose *event,
			      BoardData *bd ) {
    int i;
    
    if( !bd->pm_board )
	return TRUE;

    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_board, event->area.x,
		     event->area.y, event->area.x, event->area.y,
		     event->area.width, event->area.height );

    for( i = 0; i < 28; i++ ) {
	int y, cy;

	y = positions[ i ][ 1 ] * bd->board_size;
	cy = -5 * bd->board_size * positions[ i ][ 2 ];

	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}
	
	if( intersects( event->area.x, event->area.y, event->area.width,
			event->area.height, positions[ i ][ 0 ] *
			bd->board_size, y, 6 * bd->board_size, cy ) ) {
	    board_redraw_point( board, bd, i );
	    /* Redrawing the point might have drawn outside the exposed
	       area; enlarge the invalid area in case the dice now need
	       redrawing as well. */
	    if( event->area.x > positions[ i ][ 0 ] * bd->board_size )
		event->area.x = positions[ i ][ 0 ] * bd->board_size;
	    if( event->area.x + event->area.width < ( positions[ i ][ 0 ] + 6 )
		* bd->board_size )
		event->area.width = ( positions[ i ][ 0 ] + 6 ) *
		    bd->board_size - event->area.x;
	    if( event->area.y > y )
		event->area.y = y;
	    if( event->area.y + event->area.width < y + cy )
		event->area.width = y + cy - event->area.y;
	}
    }

    if( intersects( event->area.x, event->area.y, event->area.width,
		    event->area.height, bd->x_dice[ 0 ] * bd->board_size,
		    bd->y_dice[ 0 ] * bd->board_size,
		    7 * bd->board_size, 7 * bd->board_size ) )
	board_redraw_dice( board, bd, 0 );

    if( intersects( event->area.x, event->area.y, event->area.width,
		    event->area.height, bd->x_dice[ 1 ] * bd->board_size,
		    bd->y_dice[ 1 ] * bd->board_size,
		    7 * bd->board_size, 7 * bd->board_size ) )
	board_redraw_dice( board, bd, 1 );

    if( intersects( event->area.x, event->area.y, event->area.width,
		    event->area.height, 50 * bd->board_size,
		    ( 32 - 29 * bd->cube_owner ) * bd->board_size,
		    8 * bd->board_size, 8 * bd->board_size ) )
	board_redraw_cube( board, bd );
    
    return TRUE;
}

static void board_expose_point( GtkWidget *board, BoardData *bd, int n ) {
    
    GdkEventExpose event;
    gint cy;
    
    if( bd->board_size <= 0 )
	return;

    event.count = 0;
    event.area.x = positions[ n ][ 0 ] * bd->board_size;
    event.area.y = positions[ n ][ 1 ] * bd->board_size;
    event.area.width = 6 * bd->board_size;
    cy = -5 * bd->board_size * positions[ n ][ 2 ];
    if( cy < 0 ) {
        event.area.y += cy * 4 / 5;
        event.area.height = -cy;
    } else
	event.area.height = cy;
    
    board_expose( board, &event, bd );
}

static int board_point( GtkWidget *board, BoardData *bd, int x0, int y0 ) {

    int i, y, cy;

    x0 /= bd->board_size;
    y0 /= bd->board_size;

    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;

    if( intersects( x0, y0, 0, 0, 50, 30 - 29 * bd->cube_owner, 8, 8 ) )
	return POINT_CUBE;
    
    for( i = 0; i < 28; i++ ) {
	y = positions[ i ][ 1 ];
	cy = -5 * positions[ i ][ 2 ];
	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}

	if( intersects( x0, y0, 0, 0, positions[ i ][ 0 ],
			y, 6, cy ) )
	    return i;
    }

    return -1;
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

static void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] ) {

    gtk_label_set_text( GTK_LABEL( bd->position_id ), PositionID( points ) );
}

/* A chequer has been moved or the board has been updated -- update the
   move and position ID labels. */
static void update_move( BoardData *bd ) {
    
    char move[ 40 ] = "Illegal move";
    gint i, points[ 2 ][ 25 ];
    guchar key[ 10 ];

    read_board( bd, points );
    update_position_id( bd, points );

    bd->valid_move = NULL;
    
    if( EqualBoards( points, bd->old_board ) )
        /* no move has been made */
	move[ 0 ] = 0;
    else {
        PositionKey( points, key );

        for( i = 0; i < bd->move_list.cMoves; i++ )
            if( EqualKeys( bd->move_list.amMoves[ i ].auch, key ) ) {
                /* FIXME do something different if the move is complete */
                bd->valid_move = bd->move_list.amMoves + i;
                FormatMove( move, bd->old_board, bd->valid_move->anMove );
                break;
            }
    }

    gtk_label_set_text( GTK_LABEL( bd->move ), move );
}

static gboolean board_pointer( GtkWidget *board, GdkEvent *event,
			       BoardData *bd ) {
    GdkPixmap *pm_swap;
    int n, dest, x, y, bar;

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
	bd->drag_point = board_point( board, bd, x, y );

	/* FIXME if the dice are set, and nDragPoint is between 1 and 24 and
	   contains no chequers of the player on roll, then scan through all
	   the legal moves looking for the one which makes that point with
	   the smallest pip count, make it and return. */

	/* FIXME if the dice are set, and nDragPoint is 26 or 27 (i.e. off),
	   then bear off as many chequers as possible, and return. */
	
	if( ( bd->drag_point = board_point( board, bd, x, y ) ) < 0
	    || ( bd->drag_point < 28 && !bd->points[ bd->drag_point ] ) ) {
	    /* Click on empty point, or not on a point at all */
	    gdk_beep();

	    bd->drag_point = -1;
	    
	    return TRUE;
	}

	if( bd->drag_point == POINT_CUBE ) {
	    /* Clicked on cube; double. */
	    bd->drag_point = -1;
	    
	    UserCommand( "double" ); /* FIXME emit signal */
	    return TRUE;
	}
	
	if( bd->drag_point == POINT_DICE ) {
	    /* Clicked on dice. */
	    bd->drag_point = -1;
	    
	    if( event->button.button == 1 )
		/* Button 1 on dice confirms move. */
/* 		StatsConfirm( &bd->ewndStats ); */ /* FIXME emit signal */
		;
	    else {
		/* Other buttons on dice swaps positions. */
		n = bd->dice[ 0 ];
		bd->dice[ 0 ] = bd->dice[ 1 ];
		bd->dice[ 1 ] = n;

		n = bd->dice_opponent[ 0 ];
		bd->dice_opponent[ 0 ] = bd->dice_opponent[ 1 ];
		bd->dice_opponent[ 1 ] = n;

		board_redraw_dice( board, bd, 0 );
		board_redraw_dice( board, bd, 1 );
	    }
	    
	    return TRUE;
	}
	
	bd->drag_colour = bd->points[ bd->drag_point ] < 0 ? -1 : 1;
	
	bd->points[ bd->drag_point ] -= bd->drag_colour;

	board_expose_point( board, bd, bd->drag_point );

	if( event->button.button != 1 ) {
	    /* Automatically place chequer on destination point
	       (as opposed to starting a drag). */
	    dest = bd->drag_point - ( event->button.button == 2 ?
					bd->dice[ 0 ] :
					bd->dice[ 1 ] ) * bd->drag_colour;

	    if( ( dest <= 0 ) || ( dest >= 25 ) )
		/* bearing off */
		dest = bd->drag_colour > 0 ? 26 : 27;
	    
	    goto place_chequer;
	}
	
	bd->x_drag = x;
	bd->y_drag = y;

	gdk_draw_pixmap( bd->pm_saved, bd->gc_copy, board->window,
			 bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 0, 0, 6 * bd->board_size, 6 * bd->board_size );
	
	/* fall through */
	
    case GDK_MOTION_NOTIFY:
	if( bd->drag_point < 0 )
	    break;
	
	gdk_draw_pixmap( bd->pm_temp_saved, bd->gc_copy, board->window,
			 x- 3 * bd->board_size, y - 3 * bd->board_size,
			 0, 0, 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_pixmap( bd->pm_temp_saved, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - x, bd->y_drag - y,
			 6 * bd->board_size, 6 * bd->board_size );
		   
	gdk_draw_pixmap( bd->pm_temp, bd->gc_copy, bd->pm_temp_saved,
			 0, 0, 0, 0, 6 * bd->board_size, 6 * bd->board_size );
		   
	draw_bitplane( bd->pm_temp, bd->gc_and, bd->bm_mask,
		       0, 0, 0, 0, 6 * bd->board_size, 6 * bd->board_size, 1 );
		       
	gdk_draw_pixmap( bd->pm_temp, bd->gc_or, bd->drag_colour > 0 ?
			 bd->pm_o : bd->pm_x,
			 0, 0, 0, 0, 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );
		
	bd->x_drag = x;
	bd->y_drag = y;

	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_temp,
			 0, 0, bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );

	pm_swap = bd->pm_saved;
	bd->pm_saved = bd->pm_temp_saved;
	bd->pm_temp_saved = pm_swap;
	
	break;
	
    case GDK_BUTTON_RELEASE:
	if( bd->drag_point < 0 )
	    break;
	
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );

	dest = board_point( board, bd, x, y );
    place_chequer:
	bar = bd->drag_colour == bd->colour ? 25 - bd->bar : bd->bar;
	    
	if( dest == -1 || ( bd->drag_colour > 0 ? bd->points[ dest ] < -1
			     : bd->points[ dest ] > 1 ) || dest == bar ||
	    dest > 27 ) {
	    /* FIXME check with owner that move is legal */
	    gdk_beep();
	    
	    dest = bd->drag_point;
	}

	if( bd->points[ dest ] == -bd->drag_colour ) {
	    bd->points[ dest ] = 0;
	    bd->points[ bar ] -= bd->drag_colour;

	    board_expose_point( board, bd, bar );
	}
	
	bd->points[ dest ] += bd->drag_colour;

	board_expose_point( board, bd, dest );

	if( bd->drag_point != dest )
	    update_move( bd );
	
	bd->drag_point = -1;

	break;
    default:
	g_assert_not_reached();
    }

    return TRUE;
}

static void board_set_cube_font( GtkWidget *widget, BoardData *bd ) {

    char font_name[ 256 ];
    int i, sizes[ 20 ] = { 34, 33, 26, 25, 24, 20, 19, 18, 17, 16, 15, 14, 13,
			   12, 11, 10, 9, 8, 7, 6 };
    
    /* FIXME query resource for font name and method */

    bd->cube_font_rotated = FALSE;

    /* First attempt (applies only if cube is not owned by the player at
       the bottom of the screen): try scaled, rotated font. */
    if( bd->cube_owner != -1 ) {
	sprintf( font_name, bd->cube_owner ?
		 "-adobe-utopia-bold-r-normal--[~%d 0 0 ~%d]-"
		 "*-*-*-p-*-iso8859-1[49_52 54 56]" :
		 "-adobe-utopia-bold-r-normal--[0 %d ~%d 0]-"
		 "*-*-*-p-*-iso8859-1[49_52 54 56]", bd->board_size * 5,
		 bd->board_size * 5 );

	if( ( bd->cube_font = gdk_font_load( font_name ) ) ) {
	    bd->cube_font_rotated = TRUE;
	    goto done;
	}
    }

    /* Second attempt: try scaled font. */
    sprintf( font_name, "-adobe-utopia-bold-r-normal--%d-*-*-*-p-*-"
	     "iso8859-1", bd->board_size * 5 );
    if( ( bd->cube_font = gdk_font_load( font_name ) ) )
	goto done;

    /* Third attempt: try the closest standard font size to what we want,
       and every standard size smaller than that... */
    for( i = 0; i < 20 && sizes[ i ] > bd->board_size * 5; i++ )
	;
    
    for( ; i < 20; i++ ) {
	sprintf( font_name, "-adobe-utopia-bold-r-normal--%d-*-*-*-p-*-"
		 "iso8859-1", sizes[ i ] );
	
	if( ( bd->cube_font = gdk_font_load( font_name ) ) )
	    goto done;
    }

    /* Last ditch attempt: fall back to font from widget style. */
    gdk_font_ref( bd->cube_font = widget->style->font );

 done:
    gdk_gc_set_font( bd->gc_cube, bd->cube_font );    
}

extern gint board_set( Board *board, const gchar *board_text ) {

    BoardData *bd = board->board_data;
    gchar *dest;
    gint i, *pn, **ppn;
    gint old_board[ 28 ];
    int old_direction;
    GdkEventExpose event;
#if __GNUC__
    int *match_settings[] = { &bd->match_to, &bd->score,
			      &bd->score_opponent };
    int *game_settings[] = { &bd->turn, bd->dice, bd->dice + 1,
		       bd->dice_opponent, bd->dice_opponent + 1,
		       &bd->cube, &bd->can_double, &bd->opponent_can_double,
		       &bd->doubled, &bd->colour, &bd->direction,
		       &bd->home, &bd->bar, &bd->off, &bd->off_opponent,
		       &bd->on_bar, &bd->on_bar_opponent, &bd->to_move,
		       &bd->forced, &bd->crawford, &bd->redoubles };
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
    game_settings[ 19 ] = &bd->crawford;
    game_settings[ 20 ] = &bd->redoubles;

    old_dice[ 0 ] = bd->dice[ 0 ];
    old_dice[ 1 ] = bd->dice[ 1 ];
    old_dice[ 2 ] = bd->dice_opponent[ 0 ];
    old_dice[ 3 ] = bd->dice_opponent[ 1 ];
#endif
    
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

    old_direction = bd->direction;
    
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

    read_board( bd, bd->old_board );
    update_position_id( bd, bd->old_board );

    if( bd->dice[ 0 ] || bd->dice_opponent[ 0 ] ) {
	GenerateMoves( &bd->move_list, bd->old_board,
		       bd->dice[ 0 ] | bd->dice_opponent[ 0 ],
		       bd->dice[ 1 ] | bd->dice_opponent[ 1 ], TRUE );

	bd->valid_move = NULL;
    }
	
    if( bd->board_size <= 0 )
	return 0;

    if( bd->dice[ 0 ] != old_dice[ 0 ] ||
	bd->dice[ 1 ] != old_dice[ 1 ] ||
	bd->dice_opponent[ 0 ] != old_dice[ 2 ] ||
	bd->dice_opponent[ 1 ] != old_dice[ 3 ] ) {
	if( bd->x_dice[ 0 ] > 0 ) {
	    /* dice were visible before; now they're not */
	    event.count = 0;
	    event.area.x = bd->x_dice[ 0 ] * bd->board_size;
	    event.area.y = bd->y_dice[ 0 ] * bd->board_size;
	    event.area.width = event.area.height = 7 * bd->board_size;

	    bd->x_dice[ 0 ] = -10 * bd->board_size;
	    
	    board_expose( bd->drawing_area, &event, bd );
	    
	    event.area.x = bd->x_dice[ 1 ] * bd->board_size;
	    event.area.y = bd->y_dice[ 1 ] * bd->board_size;

	    bd->x_dice[ 1 ] = -10 * bd->board_size;

	    board_expose( bd->drawing_area, &event, bd );
	}

	if( ( bd->turn == bd->colour ? bd->dice[ 0 ] :
	       bd->dice_opponent[ 0 ] ) <= 0 )
	    /* dice have not been rolled */
	    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10 * bd->board_size;
	else {
	    /* FIXME different dice for first turn */
	    /* FIXME avoid cocked dice if possible */
	    bd->x_dice[ 0 ] = RAND % 21 + 13;
	    bd->x_dice[ 1 ] = RAND % ( 34 - bd->x_dice[ 0 ] ) +
		bd->x_dice[ 0 ] + 8;
	    
	    if( bd->colour == bd->turn ) {
		bd->x_dice[ 0 ] += 48;
		bd->x_dice[ 1 ] += 48;
	    }
	    
	    bd->y_dice[ 0 ] = RAND % 10 + 28;
	    bd->y_dice[ 1 ] = RAND % 10 + 28;
	    bd->dice_colour[ 0 ] = bd->dice_colour[ 1 ] = bd->turn;
	}
    }
    
    if( bd->cube_owner != bd->opponent_can_double - bd->can_double ) {
	event.count = 0;
	event.area.x = 50 * bd->board_size;
	event.area.y = ( 32 - 29 * bd->cube_owner ) * bd->board_size;
	event.area.width = event.area.height = 8 * bd->board_size;

	gdk_font_unref( bd->cube_font );
	bd->cube_owner = bd->opponent_can_double - bd->can_double;
	board_set_cube_font( GTK_WIDGET( board ), bd );

	board_expose( bd->drawing_area, &event, bd );
    }
    
    if( bd->direction != old_direction ) {
	for( i = 0; i < 28; i++ ) {
	    /* FIXME this will break if there are multiple board widgets */
	    positions[ i ][ 0 ] = 102 - positions[ i ][ 0 ];
	    positions[ i ][ 1 ] = 66 - positions[ i ][ 1 ];
	    positions[ i ][ 2 ] = -positions[ i ][ 2 ];
	}
	
	event.count = event.area.x = event.area.y = 0;
	event.area.width = 108 * bd->board_size;
	event.area.height = 72 * bd->board_size;
	
	board_expose( bd->drawing_area, &event, bd );
    } else {
	for( i = 0; i < 28; i++ )
	    if( bd->points[ i ] != old_board[ i ] )
		board_redraw_point( bd->drawing_area, bd, i );
	
	/* FIXME only redraw changed points/dice/cube */
	board_redraw_dice( bd->drawing_area, bd, 0 );
	board_redraw_dice( bd->drawing_area, bd, 1 );
	board_redraw_cube( bd->drawing_area, bd );
    }
    
    return 0;
}

extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1 ) {
    gchar board_str[ 256 ];
    int i, off[ 2 ], old_dice;
    BoardData *pbd = board->board_data;

    memcpy( pbd->old_board, points, sizeof( pbd->old_board ) );
    old_dice = pbd->dice[ 0 ];
    
    /* Names and match length/score */
    sprintf( board_str, "board:%s:%s:%d:%d:%d:", name, opp_name, match, score,
	     opp_score );

    /* Opponent on bar */
    sprintf( strchr( board_str, 0 ), "%d:", -points[ 0 ][ 24 ] );

    /* Board */
    for( i = 0; i < 24; i++ )
	sprintf( strchr( board_str, 0 ), "%d:", points[ 0 ][ 23 - i ] ?
		 -points[ 0 ][ 23 - i ] : points[ 1 ][ i ] );

    /* Player on bar */
    sprintf( strchr( board_str, 0 ), "%d:", points[ 1 ][ 24 ] );

    /* Whose turn */
    strcat( strchr( board_str, 0 ), roll ? "1:" : "-1:" );

    off[ 0 ] = off[ 1 ] = 15;
    for( i = 0; i < 25; i++ ) {
	off[ 0 ] -= points[ 0 ][ i ];
	off[ 1 ] -= points[ 1 ][ i ];
    }
    
    sprintf( strchr( board_str, 0 ), "%d:%d:%d:%d:%d:%d:%d:%d:1:-1:0:25:%d:%d:0:0:0:"
	     "0:0:0", die0, die1, die0, die1, nCube, fCubeOwner != 0,
	     fCubeOwner != 1, fDoubled, off[ 1 ], off[ 0 ] );
    
    board_set( board, board_str );
    
#if 0
    /* FIXME */
    StatsSet( &pbd->ewndStats, points, die0, die1 );

    if( !pewnd->pdsp )
	return 0;
    
    if( pbd->anDice[ 0 ] )
	XUnmapWindow( pewnd->pdsp, pbd->ewndDice.wnd );
    else if( old_dice )
	XMapWindow( pewnd->pdsp, pbd->ewndDice.wnd );
    else
	DiceRedraw( &pbd->ewndDice, pbd->ewndDice.pv );
#endif
    return 0;
}

static void board_free_pixmaps( BoardData *bd ) {
    gdk_pixmap_unref( bd->pm_board );
    gdk_pixmap_unref( bd->pm_x );
    gdk_pixmap_unref( bd->pm_o );
    gdk_pixmap_unref( bd->pm_x_dice );
    gdk_pixmap_unref( bd->pm_o_dice );
    gdk_pixmap_unref( bd->pm_x_pip );
    gdk_pixmap_unref( bd->pm_o_pip );
    gdk_pixmap_unref( bd->pm_cube );
    gdk_pixmap_unref( bd->pm_saved );
    gdk_pixmap_unref( bd->pm_temp );
    gdk_pixmap_unref( bd->pm_temp_saved );
    gdk_pixmap_unref( bd->pm_point );
    gdk_bitmap_unref( bd->bm_mask );
    gdk_bitmap_unref( bd->bm_dice_mask );
    gdk_bitmap_unref( bd->bm_cube_mask );
}

static void board_draw_border( GdkPixmap *pm, GdkGC *gc, int x0, int y0,
			       int x1, int y1, int board_size,
			       GdkColor *colours, gboolean invert ) {
#define COLOURS( ipix, edge ) ( *( colours + (ipix) * 2 + (edge) ) )

    int i;
    GdkPoint points[ 5 ];

    points[ 0 ].x = points[ 1 ].x = points[ 4 ].x = x0 * board_size;
    points[ 2 ].x = points[ 3 ].x = x1 * board_size - 1;

    points[ 1 ].y = points[ 2 ].y = y0 * board_size;
    points[ 0 ].y = points[ 3 ].y = points[ 4 ].y = y1 * board_size - 1;
    
    for( i = 0; i < board_size; i++ ) {
	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 0 ) :
			       &COLOURS( i, 1 ) );
	gdk_draw_lines( pm, gc, points, 3 );

	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 1 ) :
			       &COLOURS( i, 0 ) );
	
	gdk_draw_lines( pm, gc, points + 2, 3 );

	points[ 0 ].x = points[ 1 ].x = ++points[ 4 ].x;
	points[ 2 ].x = --points[ 3 ].x;

	points[ 1 ].y = ++points[ 2 ].y;
	points[ 0 ].y = points[ 3 ].y = --points[ 4 ].y;
    }
}

static void board_draw( GtkWidget *widget, BoardData *bd ) {

    guchar buf[ 66 * bd->board_size ][ 12 * bd->board_size ][ 3 ];
    gint ix, iy, antialias;
    GdkGC *gc;
    GdkGCValues gcv;
    float x, z, cos_theta, diffuse, specular;
    GdkColor colours[ 2 * bd->board_size ];
    
    bd->pm_board = gdk_pixmap_new( widget->window, 108 * bd->board_size,
				   72 * bd->board_size, -1 );

    /* R, B = 0.9^30 x 0.8; G = 0.92 x 0x30 + 0.9^30 x 0.8 */
    gcv.foreground.pixel = gdk_rgb_xpixel_from_rgb( 0x093509 );
    gc = gdk_gc_new_with_values( widget->window, &gcv, GDK_GC_FOREGROUND );
    
    gdk_draw_rectangle( bd->pm_board, gc, TRUE, 0, 0, bd->board_size * 108,
			bd->board_size * 72 );

    for( ix = 0; ix < bd->board_size; ix++ ) {
	x = 1.0 - ( (float) ix / bd->board_size );
	z = sqrt( 1.001 - x * x );
	cos_theta = 0.9 * z - 0.3 * x;
	diffuse = 0.8 * cos_theta + 0.2;
	specular = pow( cos_theta, 30 ) * 0.8;

	COLOURS( ix, 0 ).pixel = gdk_rgb_xpixel_from_rgb(
	    ( (int) ( specular * 0x100 ) << 16 ) |
	    ( (int) ( specular * 0x100 + diffuse * 0x30 ) << 8 ) |
	    ( (int) ( specular * 0x100 ) ) );

	x = -x;
	cos_theta = 0.9 * z - 0.3 * x;
	diffuse = 0.8 * cos_theta + 0.2;
	specular = pow( cos_theta, 30 ) * 0.8;

	COLOURS( ix, 1 ).pixel = gdk_rgb_xpixel_from_rgb(
	    ( (int) ( specular * 0x100 ) << 16 ) |
	    ( (int) ( specular * 0x100 + diffuse * 0x30 ) << 8 ) |
	    ( (int) ( specular * 0x100 ) ) );
    }
#undef COLOURS

    board_draw_border( bd->pm_board, gc, 0, 0, 54, 72,
		       bd->board_size, colours, 0 );
    board_draw_border( bd->pm_board, gc, 54, 0, 108, 72,
		       bd->board_size, colours, 0 );
    
    board_draw_border( bd->pm_board, gc, 2, 2, 10, 34,
		       bd->board_size, colours, 1 );
    board_draw_border( bd->pm_board, gc, 2, 38, 10, 70,
		       bd->board_size, colours, 1 );
    board_draw_border( bd->pm_board, gc, 98, 2, 106, 34,
		       bd->board_size, colours, 1 );
    board_draw_border( bd->pm_board, gc, 98, 38, 106, 70,
		       bd->board_size, colours, 1 );

    board_draw_border( bd->pm_board, gc, 11, 2, 49, 70,
		       bd->board_size, colours, 1 );
    board_draw_border( bd->pm_board, gc, 59, 2, 97, 70,
		       bd->board_size, colours, 1 );
    
    /* FIXME shade hinges */
    
    for( iy = 0; iy < 30 * bd->board_size; iy++ )
	for( ix = 0; ix < 6 * bd->board_size; ix++ ) {
	    /* <= 0 is board; >= 20 is on a point; interpolate in between */
	    antialias = 2 * ( 30 * bd->board_size - iy ) + 1 - 20 *
		abs( 3 * bd->board_size - ix );

	    if( antialias < 0 )
		antialias = 0;
	    else if( antialias > 20 )
		antialias = 20;

	    buf[ iy ][ ix + 6 * bd->board_size ][ 0 ] =
		buf[ 66 * bd->board_size - iy - 1 ][ ix ][ 0 ] =
		( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) * ( 20 - antialias ) +
		  ( 0x80 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
		  antialias ) / 20;
		      
	    buf[ iy ][ ix + 6 * bd->board_size ][ 1 ] =
		buf[ 66 * bd->board_size - iy - 1 ][ ix ][ 1 ] =
		( ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) * ( 20 - antialias ) +
		  ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
		  antialias ) / 20;
		      
	    buf[ iy ][ ix + 6 * bd->board_size ][ 2 ] =
		buf[ 66 * bd->board_size - iy - 1 ][ ix ][ 2 ] =
		( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) * ( 20 - antialias ) +
		  ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
		  antialias ) / 20;

	    buf[ iy ][ ix ][ 0 ] =
	    buf[ 66 * bd->board_size - iy - 1 ][ ix + 6 * bd->board_size ]
		[ 0 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			  ( 20 - antialias ) +
			  ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			  antialias ) / 20;
		      
	    buf[ iy ][ ix ][ 1 ] =
		buf[ 66 * bd->board_size - iy - 1 ][ ix + 6 * bd->board_size ]
		[ 1 ] = ( ( ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			  ( 20 - antialias ) +
			  ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			  antialias ) / 20;
		      
	    buf[ iy ][ ix ][ 2 ] =
		buf[ 66 * bd->board_size - iy - 1 ][ ix + 6 * bd->board_size ]
		[ 2 ] = ( ( ( RAND & 0x1F ) + ( RAND & 0x1F ) ) *
			  ( 20 - antialias ) +
			  ( 0x40 + ( RAND & 0x3F ) + ( RAND & 0x3F ) ) *
			  antialias ) / 20;
	}
    
    for( iy = 0; iy < 6 * bd->board_size; iy++ )
	for( ix = 0; ix < 12 * bd->board_size; ix++ ) {
	    buf[ 30 * bd->board_size + iy ][ ix ][ 0 ] =
		( RAND & 0x1F ) + ( RAND & 0x1F );
	    buf[ 30 * bd->board_size + iy ][ ix ][ 1 ] =
		( RAND & 0x3F ) + ( RAND & 0x3F );
	    buf[ 30 * bd->board_size + iy ][ ix ][ 2 ] =
		( RAND & 0x1F ) + ( RAND & 0x1F );
	}

    gdk_draw_rgb_image( bd->pm_board, gc, 12 * bd->board_size,
			3 * bd->board_size, 12 * bd->board_size,
			66 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf[ 0 ][ 0 ][ 0 ], 12 * bd->board_size * 3 );

    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     12 * bd->board_size, 3 * bd->board_size,
		     24 * bd->board_size, 3 * bd->board_size,
		     12 * bd->board_size, 66 * bd->board_size );
    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     12 * bd->board_size, 3 * bd->board_size,
		     36 * bd->board_size, 3 * bd->board_size,
		     12 * bd->board_size, 66 * bd->board_size );
    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     12 * bd->board_size, 3 * bd->board_size,
		     60 * bd->board_size, 3 * bd->board_size,
		     36 * bd->board_size, 66 * bd->board_size );
		     
    for( iy = 0; iy < 30 * bd->board_size; iy++ )
	for( ix = 0; ix < 6 * bd->board_size; ix++ ) {
	    buf[ iy ][ ix ][ 0 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );
	    buf[ iy ][ ix ][ 1 ] = ( RAND & 0x3F ) + ( RAND & 0x3F );
	    buf[ iy ][ ix ][ 2 ] = ( RAND & 0x1F ) + ( RAND & 0x1F );
	}

    gdk_draw_rgb_image( bd->pm_board, gc, 3 * bd->board_size,
			3 * bd->board_size, 6 * bd->board_size,
			30 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf[ 0 ][ 0 ][ 0 ], 12 * bd->board_size * 3 );
    
    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     3 * bd->board_size, 3 * bd->board_size,
		     99 * bd->board_size, 3 * bd->board_size,
		     6 * bd->board_size, 30 * bd->board_size );
    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     3 * bd->board_size, 3 * bd->board_size,
		     3 * bd->board_size, 39 * bd->board_size,
		     6 * bd->board_size, 30 * bd->board_size );
    gdk_draw_pixmap( bd->pm_board, gc, bd->pm_board,
		     3 * bd->board_size, 3 * bd->board_size,
		     99 * bd->board_size, 39 * bd->board_size,
		     6 * bd->board_size, 30 * bd->board_size );

    gdk_gc_unref( gc );
}

static inline guchar clamp( gint n ) {

    if( n < 0 )
	return 0;
    else if( n > 0xFF )
	return 0xFF;
    else
	return n;
}

static void board_draw_chequers( GtkWidget *widget, BoardData *bd ) {

    guchar buf_x[ 6 * bd->board_size ][ 6 * bd->board_size ][ 3 ],
	buf_o[ 6 * bd->board_size ][ 6 * bd->board_size ][ 3 ],
	*buf_mask;
    int ix, iy, in, fx, fy, i;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta;
    GdkImage *img;
    GdkGC *gc;

    /* We need to use malloc() for this, since Xlib will free() it. */
    buf_mask = malloc( ( 6 * bd->board_size ) * ( 6 * bd->board_size + 7 )
		       >> 3 );
    
    bd->pm_x = gdk_pixmap_new( widget->window, 6 * bd->board_size,
			       6 * bd->board_size, -1 );
    bd->pm_o = gdk_pixmap_new( widget->window, 6 * bd->board_size,
			       6 * bd->board_size, -1 );
    bd->bm_mask = gdk_pixmap_new( NULL, 6 * bd->board_size,
			       6 * bd->board_size, 1 );

    img = gdk_image_new_bitmap( gdk_window_get_visual( widget->window ),
				buf_mask, 6 * bd->board_size,
				6 * bd->board_size );
    
    for( iy = 0, y_loop = -1.0; iy < 6 * bd->board_size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < 6 * bd->board_size; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			in++;
			diffuse += 0.2;
			z = sqrt( z ) * 5;
			if( ( cos_theta = ( 0.9 * z - 0.316 * x - 0.3 * y ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    diffuse += cos_theta * 0.8;
			    specular_x += pow( cos_theta, 10 ) * 0.4;
			    specular_o += pow( cos_theta, 30 ) * 0.8;
			}
		    }
		    x += 1.0 / ( 6 * bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( 6 * bd->board_size );
	    } while( !fy++ );
	    
	    if( in < 3 ) {
		for( i = 0; i < 3; i++ )
		    buf_x[ iy ][ ix ][ i ] = buf_o[ iy ][ ix ][ i ] = 0;
		gdk_image_put_pixel( img, ix, iy, 0 );
	    } else {
		gdk_image_put_pixel( img, ix, iy, ~0L );

		buf_x[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.8 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.1 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.1 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );

		buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.01 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.01 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.02 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
	    }
	    x_loop += 2.0 / ( 6 * bd->board_size );
	}
	y_loop += 2.0 / ( 6 * bd->board_size );
    }

    gdk_draw_rgb_image( bd->pm_x, bd->gc_copy, 0, 0, 6 * bd->board_size,
			6 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_x[ 0 ][ 0 ][ 0 ], 6 * bd->board_size * 3 );
    gdk_draw_rgb_image( bd->pm_o, bd->gc_copy, 0, 0, 6 * bd->board_size,
			6 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_o[ 0 ][ 0 ][ 0 ], 6 * bd->board_size * 3 );

    gc = gdk_gc_new( bd->bm_mask );
    gdk_draw_image( bd->bm_mask, gc, img, 0, 0, 0, 0, 6 * bd->board_size,
		    6 * bd->board_size );
    gdk_gc_unref( gc );
    
    gdk_image_destroy( img );
}

static void board_draw_dice( GtkWidget *widget, BoardData *bd ) {

    GdkGC *gc;
    guchar buf_x[ 7 * bd->board_size ][ 7 * bd->board_size ][ 3 ],
	buf_o[ 7 * bd->board_size ][ 7 * bd->board_size ][ 3 ],
	*buf_mask;
    GdkImage *img;
    int ix, iy, in, fx, fy, i;
    float x, y, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	x_norm, y_norm, z_norm;

    /* We need to use malloc() for this, since Xlib will free() it. */
    buf_mask = malloc( ( 7 * bd->board_size ) * ( 7 * bd->board_size + 7 )
		       >> 3 );
    
    bd->pm_x_dice = gdk_pixmap_new( widget->window, 7 * bd->board_size,
				    7 * bd->board_size, -1 );
    bd->pm_o_dice = gdk_pixmap_new( widget->window, 7 * bd->board_size,
				    7 * bd->board_size, -1 );
    bd->bm_dice_mask = gdk_pixmap_new( NULL, 7 * bd->board_size,
				       7 * bd->board_size, 1 );
    
    img = gdk_image_new_bitmap( gdk_window_get_visual( widget->window ),
				buf_mask, 7 * bd->board_size,
				7 * bd->board_size );
    	
    for( iy = 0, y_loop = -1.0; iy < 7 * bd->board_size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < 7 * bd->board_size; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( fabs( x ) < 6.0 / 7.0 &&
			fabs( y ) < 6.0 / 7.0 ) {
			in++;
			diffuse += 0.92; /* 0.9 x 0.8 + 0.2 (ambient) */
			specular_x += 0.139; /* 0.9^10 x 0.4 */
			specular_o += 0.034; /* 0.9^30 x 0.8 */
		    } else {
			if( fabs( x ) < 6.0 / 7.0 ) {
			    x_norm = 0.0;
			    y_norm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = sqrt( 1.001 - y_norm * y_norm );
			} else if( fabs( y ) < 6.0 / 7.0 ) {
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = sqrt( 1.001 - x_norm * x_norm );
			} else {
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = 0.9 * z_norm - 0.316 * x_norm -
			      0.3 * y_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    specular_x += pow( cos_theta, 10 ) * 0.4;
			    specular_o += pow( cos_theta, 30 ) * 0.8;
			}
		    }
		missed:		    
		    x += 1.0 / ( 7 * bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( 7 * bd->board_size );
	    } while( !fy++ );
	    
	    if( in < 2 ) {
		for( i = 0; i < 3; i++ )
		    buf_x[ iy ][ ix ][ i ] = buf_o[ iy ][ ix ][ i ] = 0;
		gdk_image_put_pixel( img, ix, iy, 0 );
	    } else {
		gdk_image_put_pixel( img, ix, iy, 1 );

		buf_x[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.8 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.1 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.1 + specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		
		buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.01 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.01 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.02 + specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
	    }
	    x_loop += 2.0 / ( 7 * bd->board_size );
	}
	y_loop += 2.0 / ( 7 * bd->board_size );
    }

    gdk_draw_rgb_image( bd->pm_x_dice, bd->gc_copy, 0, 0, 7 * bd->board_size,
			7 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_x[ 0 ][ 0 ][ 0 ], 7 * bd->board_size * 3 );
    gdk_draw_rgb_image( bd->pm_o_dice, bd->gc_copy, 0, 0, 7 * bd->board_size,
			7 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_o[ 0 ][ 0 ][ 0 ], 7 * bd->board_size * 3 );

    gc = gdk_gc_new( bd->bm_dice_mask );
    gdk_draw_image( bd->bm_dice_mask, gc, img, 0, 0, 0, 0, 7 * bd->board_size,
		    7 * bd->board_size );
    gdk_gc_unref( gc );
    
    gdk_image_destroy( img );
}

static void board_draw_pips( GtkWidget *widget, BoardData *bd ) {

    guchar buf_x[ bd->board_size ][ bd->board_size ][ 3 ],
	buf_o[ bd->board_size ][ bd->board_size ][ 3 ];
    int ix, iy, in, fx, fy;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta;

    bd->pm_x_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );
    bd->pm_o_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );
    
    for( iy = 0, y_loop = -1.0; iy < bd->board_size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < bd->board_size; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			in++;
			diffuse += 0.2;
			z = sqrt( z ) * 5;
			if( ( cos_theta = ( 0.316 * x + 0.3 * y + 0.9 * z ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    diffuse += cos_theta * 0.8;
			    specular_x += pow( cos_theta, 10 ) * 0.4;
			    specular_o += pow( cos_theta, 30 ) * 0.8;
			}
		    }
		    x += 1.0 / ( bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( bd->board_size );
	    } while( !fy++ );

	    buf_x[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.8 + 0.139 ) * 64.0 );
	    buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.1 + 0.139 ) * 64.0 );
	    buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.1 + 0.139 ) * 64.0 );
	    
	    buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.01 + 0.034 ) * 64.0 );
	    buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.01 + 0.034 ) * 64.0 );
	    buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    ( 0.92 * 0.02 + 0.034 ) * 64.0 );
	    x_loop += 2.0 / ( bd->board_size );
	}
	y_loop += 2.0 / ( bd->board_size );
    }

    gdk_draw_rgb_image( bd->pm_x_pip, bd->gc_copy, 0, 0, bd->board_size,
			bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_x[ 0 ][ 0 ][ 0 ], bd->board_size * 3 );
    gdk_draw_rgb_image( bd->pm_o_pip, bd->gc_copy, 0, 0, bd->board_size,
			bd->board_size, GDK_RGB_DITHER_MAX,
			&buf_o[ 0 ][ 0 ][ 0 ], bd->board_size * 3 );
}

static void board_draw_cube( GtkWidget *widget, BoardData *bd ) {

    GdkGC *gc;
    guchar buf[ 8 * bd->board_size ][ 8 * bd->board_size ][ 3 ],
	*buf_mask;
    GdkImage *img;
    int ix, iy, in, fx, fy, i;
    float x, y, x_loop, y_loop, diffuse, specular, cos_theta,
	x_norm, y_norm, z_norm;

    /* We need to use malloc() for this, since Xlib will free() it. */
    buf_mask = malloc( 8 * bd->board_size * bd->board_size );
    
    bd->pm_cube = gdk_pixmap_new( widget->window, 8 * bd->board_size,
				  8 * bd->board_size, -1 );
    bd->bm_cube_mask = gdk_pixmap_new( NULL, 8 * bd->board_size,
				       8 * bd->board_size, 1 );
    
    img = gdk_image_new_bitmap( gdk_window_get_visual( widget->window ),
				buf_mask, 8 * bd->board_size,
				8 * bd->board_size );
    	
    for( iy = 0, y_loop = -1.0; iy < 8 * bd->board_size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < 8 * bd->board_size; ix++ ) {
	    in = 0;
	    diffuse = specular = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    if( fabs( x ) < 7.0 / 8.0 &&
			fabs( y ) < 7.0 / 8.0 ) {
			in++;
			diffuse += 0.92; /* 0.9 x 0.8 + 0.2 (ambient) */
			specular += 0.139; /* 0.9^10 x 0.4 */
		    } else {
			if( fabs( x ) < 7.0 / 8.0 ) {
			    x_norm = 0.0;
			    y_norm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = sqrt( 1.001 - y_norm * y_norm );
			} else if( fabs( y ) < 7.0 / 8.0 ) {
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = sqrt( 1.001 - x_norm * x_norm );
			} else {
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 7.0 * y + ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = 0.9 * z_norm - 0.316 * x_norm -
			      0.3 * y_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    specular += pow( cos_theta, 10 ) * 0.4;
			}
		    }
		missed:		    
		    x += 1.0 / ( 8 * bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( 8 * bd->board_size );
	    } while( !fy++ );
	    
	    if( in < 2 ) {
		for( i = 0; i < 3; i++ )
		    buf[ iy ][ ix ][ i ] = 0;
		gdk_image_put_pixel( img, ix, iy, 0 );
	    } else {
		gdk_image_put_pixel( img, ix, iy, 1 );

		buf[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.8 + specular ) *
					      64.0 + ( 4 - in ) * 32.0 );
		buf[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.8 + specular ) *
					      64.0 + ( 4 - in ) * 32.0 );
		buf[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.8 + specular ) *
					      64.0 + ( 4 - in ) * 32.0 );
	    }
	    x_loop += 2.0 / ( 8 * bd->board_size );
	}
	y_loop += 2.0 / ( 8 * bd->board_size );
    }

    gdk_draw_rgb_image( bd->pm_cube, bd->gc_copy, 0, 0, 8 * bd->board_size,
			8 * bd->board_size, GDK_RGB_DITHER_MAX,
			&buf[ 0 ][ 0 ][ 0 ], 8 * bd->board_size * 3 );

    gc = gdk_gc_new( bd->bm_cube_mask );
    gdk_draw_image( bd->bm_cube_mask, gc, img, 0, 0, 0, 0, 8 * bd->board_size,
		    8 * bd->board_size );
    gdk_gc_unref( gc );
    
    gdk_image_destroy( img );
}

static void board_create_pixmaps( GtkWidget *board, BoardData *bd ) {
    
    if( bd->pm_board )
	board_free_pixmaps( bd );

    board_draw( board /* ->drawing_area */, bd );
    board_draw_chequers( board /* ->drawing_area */, bd );
    board_draw_dice( board /* ->drawing_area */, bd );
    board_draw_pips( board /* ->drawing_area */, bd );
    board_draw_cube( board /* ->drawing_area */, bd );
    board_set_cube_font( board /* ->drawing_area */, bd );
    
    bd->pm_saved = gdk_pixmap_new( board /* ->drawing_area */->window,
				   6 * bd->board_size,
				   6 * bd->board_size, -1 );
    bd->pm_temp = gdk_pixmap_new( board /* ->drawing_area */->window,
				  6 * bd->board_size,
				  6 * bd->board_size, -1 );
    bd->pm_temp_saved = gdk_pixmap_new( board /* ->drawing_area */->window,
					6 * bd->board_size,
					6 * bd->board_size, -1 );
    bd->pm_point = gdk_pixmap_new( board /* ->drawing_area */->window,
				   6 * bd->board_size,
				   35 * bd->board_size, -1 );
}

static void board_size_allocate( GtkWidget *board,
				 GtkAllocation *allocation ) {
    BoardData *bd = BOARD( board )->board_data;
    gint old_size = bd->board_size;
    GtkAllocation child_allocation;
    GtkRequisition requisition;

    memcpy( &board->allocation, allocation, sizeof( GtkAllocation ) );
    
    gtk_widget_get_child_requisition( bd->hbox_match, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->hbox_match, &child_allocation );
    
    gtk_widget_get_child_requisition( bd->table, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->table, &child_allocation );
    
    gtk_widget_get_child_requisition( bd->hbox_move, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->hbox_move, &child_allocation );

    if( ( bd->board_size = MIN( allocation->width / 108,
				allocation->height / 72 ) ) != old_size &&
	GTK_WIDGET_REALIZED( board ) )
	board_create_pixmaps( board, bd );

    child_allocation.width = 108 * bd->board_size;
    child_allocation.x = allocation->x + ( ( allocation->width -
					     child_allocation.width ) >> 1 );
    child_allocation.height = 72 * bd->board_size;
    child_allocation.y = allocation->y + ( ( allocation->height -
					     child_allocation.height ) >> 1 );
    
    gtk_widget_size_allocate( bd->drawing_area, &child_allocation );
}

static void board_realize( GtkWidget *board ) {

    BoardData *bd = BOARD( board )->board_data;
    
    if( GTK_WIDGET_CLASS( parent_class )->realize )
	GTK_WIDGET_CLASS( parent_class )->realize( board );

    board_create_pixmaps( board, bd );
}

static void board_init( Board *board ) {

    BoardData *bd = g_malloc( sizeof( *bd ) );
    GdkColormap *cmap = gtk_widget_get_colormap( GTK_WIDGET( board ) );
    GdkVisual *vis = gtk_widget_get_visual( GTK_WIDGET( board ) );
    GdkGCValues gcval;
    
    board->board_data = bd;
    
    bd->drag_point = bd->board_size = -1;

    gcval.function = GDK_AND;
    gcval.foreground.pixel = ~0L; /* AllPlanes */
    gcval.background.pixel = 0;
    bd->gc_and = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FOREGROUND |
			     GDK_GC_BACKGROUND | GDK_GC_FUNCTION );

    gcval.function = GDK_OR;
    bd->gc_or = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FUNCTION );

    bd->gc_copy = gtk_gc_get( vis->depth, cmap, &gcval, 0 );

    gcval.foreground.pixel = gdk_rgb_xpixel_from_rgb( 0x000080 );
    bd->gc_cube = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FOREGROUND );

    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;    
    
    bd->pm_board = NULL;

    bd->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->drawing_area ), 108 * 3,
			   72 * 3 );
    gtk_widget_set_events( GTK_WIDGET( bd->drawing_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK | GDK_STRUCTURE_MASK );
    gtk_container_add( GTK_CONTAINER( board ), bd->drawing_area );

    bd->hbox_move = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( board ), bd->hbox_move, FALSE, FALSE, 0 );

    bd->table = gtk_table_new( 3, 3, FALSE );
    gtk_box_pack_start( GTK_BOX( board ), bd->table, FALSE, FALSE, 0 );
    
    bd->hbox_match = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( board ), bd->hbox_match, FALSE, FALSE, 0 );
    
    /* FIXME */ {
	gtk_box_pack_start( GTK_BOX( bd->hbox_move ), bd->move =
			    gtk_label_new( NULL ), TRUE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( bd->hbox_move ), bd->position_id =
			    gtk_label_new( NULL ), TRUE, FALSE, 0 );
	gtk_table_attach( GTK_TABLE( bd->table ), gtk_label_new( "Name" ),
			  1, 2, 0, 1, 0, 0, 4, 0 );
	gtk_table_attach( GTK_TABLE( bd->table ), gtk_label_new( "Score" ),
			  2, 3, 0, 1, 0, 0, 4, 0 );
#if 0
	/* FIXME needs gnome */
	gtk_table_attach( GTK_TABLE( bd->table ), gnome_color_picker_new(),
			  0, 1, 1, 2, 0, 0, 0, 0 );
	gtk_table_attach( GTK_TABLE( bd->table ), gnome_color_picker_new(),
			  0, 1, 2, 3, 0, 0, 0, 0 );
#endif
	gtk_table_attach( GTK_TABLE( bd->table ), gtk_label_new( "0" ),
			  2, 3, 1, 2, 0, 0, 4, 0 );
	gtk_table_attach( GTK_TABLE( bd->table ), gtk_label_new( "0" ),
			  2, 3, 2, 3, 0, 0, 4, 0 );
	gtk_box_pack_start( GTK_BOX( bd->hbox_match ),
			    gtk_label_new( "Match:" ), FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( bd->hbox_match ),
			    gtk_label_new( "unlimited" ), FALSE, FALSE, 0 );
    }

    board_set( board, "board:::1:0:0:"
              "0:-2:0:0:0:0:5:0:3:0:0:0:-5:5:0:0:0:-3:0:-5:0:0:0:0:2:0:"
              "1:3:1:0:0:1:1:1:0:1:-1:0:25:0:0:0:0:2:0:0:3" );
    
    gtk_widget_show_all( GTK_WIDGET( board ) );
    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "expose_event",
			GTK_SIGNAL_FUNC( board_expose ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "button_press_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "button_release_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );    
    gtk_signal_connect( GTK_OBJECT( bd->drawing_area ), "motion_notify_event",
			GTK_SIGNAL_FUNC( board_pointer ), bd );    
}

static void board_class_init( BoardClass *c ) {

    parent_class = gtk_type_class( GTK_TYPE_VBOX );
    
    ( (GtkWidgetClass *) c )->size_allocate = board_size_allocate;
    ( (GtkWidgetClass *) c )->realize = board_realize;
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
   
   if( !board_type )
       board_type = gtk_type_unique( GTK_TYPE_VBOX, &board_info );

   return board_type;
}
