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
#define GDK_ENABLE_BROKEN /* for gdk_image_new_bitmap */
#include <gtk/gtk.h>
#include <isaac.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "gdkgetrgb.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtk-multiview.h"
#include "gtkprefs.h"
#include "positionid.h"
#include "sound.h"
#include "matchid.h"
#include "i18n.h"

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_style_get_font(s) ((s)->font)
#endif

#define POINT_UNUSED0 28 /* the top unused bearoff tray */
#define POINT_UNUSED1 29 /* the bottom unused bearoff tray */
#define POINT_DICE 30
#define POINT_CUBE 31
#define POINT_RIGHT 32
#define POINT_LEFT 33

#define CLICK_TIME 200 /* minimum time in milliseconds before a drag to the
			  same point is considered a real drag rather than a
			  click */
#define MASK_INVISIBLE 1
#define MASK_VISIBLE 0

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

static void copy_rgb( guchar *src, guchar *dest, int xsrc, int ysrc,
		      int xdest, int ydest, int width, int height,
		      int xdestmax, int ydestmax,
		      int srcstride, int deststride ) {
    int y;

    if( xsrc < 0 ) {
	width += xsrc;
	xdest -= xsrc;
	xsrc = 0;
    }

    if( ysrc < 0 ) {
	height += ysrc;
	ydest -= ysrc;
	ysrc = 0;
    }

    if( xdest < 0 ) {
	width += xdest;
	xsrc -= xdest;
	xdest = 0;
    }

    if( ydest < 0 ) {
	height += ydest;
	ysrc -= ydest;
	ydest = 0;
    }

    if( xdest + width > xdestmax )
	width = xdestmax - xdest;
    
    if( ydest + height > ydestmax )
	height = ydestmax - ydest;

    if( height < 0 || width < 0 )
	return;
    
    src += 3 * xsrc;
    dest += 3 * xdest;

    src += srcstride * ysrc;
    dest += deststride * ydest;

    for( y = 0; y < height; y++ ) {
	memcpy( dest, src, 3 * width );
	dest += deststride;
	src += srcstride;
    }
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

    if( bd->beep_illegal )
	gdk_beep();
}

static void board_label_chequers( GtkWidget *board, BoardData *bd,
				  int x, int y, int c ) {
    int cx, cy0, cy1;
    char sz[ 3 ];
	
    gtk_draw_box( board->style, board->window, GTK_STATE_NORMAL,
		  GTK_SHADOW_OUT, x + bd->board_size, y + bd->board_size,
		  4 * bd->board_size, 4 * bd->board_size );

    /* FIXME it would be nice to scale the font to the board size, but
       that's too much work. */
	
    sprintf( sz, "%d", c );
    gdk_string_extents( gtk_style_get_font( board->style ), sz, NULL, NULL,
			&cx, &cy0, &cy1 );
	
    gtk_draw_string( board->style, board->window, GTK_STATE_NORMAL,
		     x + 3 * bd->board_size - cx / 2,
		     y + 3 * bd->board_size + cy0 / 2 - cy1, sz );
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
    
    *px = positions[ fClockwise ][ n ][ 0 ] * bd->board_size;
    *py = positions[ fClockwise ][ n ][ 1 ] * bd->board_size;
    *pcx = 6 * bd->board_size;
    *pcy = positions[ fClockwise ][ n ][ 2 ] * bd->board_size;
    
    if( *pcy > 0 ) {
	*pcy = *pcy * ( c_chequer - 1 ) + 6 * bd->board_size;
	*py += 6 * bd->board_size - *pcy;
    } else
	*pcy = -*pcy * ( c_chequer - 1 ) + 6 * bd->board_size;
}

static void board_redraw_translucent( GtkWidget *board, BoardData *bd,
				      int n ) {
    int i, x, y, cx, cy, xpix, ypix, invert, y_chequer, c_chequer, i_chequer,
	i_refract;
    guchar rgb_under[ 30 * bd->board_size ][ 6 * bd->board_size ][ 3 ],
	rgb[ 30 * bd->board_size ][ 6 * bd->board_size ][ 3 ],
	*p, *p_src, *rgba;
    short *refract;
    
    c_chequer = ( !n || n == 25 ) ? 3 : 5;

    if( bd->points[ n ] > 0 ) {
	rgba = bd->rgba_o;
	refract = bd->ai_refract[ 1 ];
    } else {
	rgba = bd->rgba_x;
	refract = bd->ai_refract[ 0 ];
    }

    point_area( bd, n, &x, &y, &cx, &cy );
    invert = positions[ fClockwise ][ n ][ 2 ] > 0;

    /* copy empty point image */
    if( !n || n == 25 )
	/* on bar */
	memcpy( rgb_under[ 0 ][ 0 ], n ? bd->rgb_bar1 : bd->rgb_bar0,
		cy * cx * 3 );
    else if( n > 25 )
	/* bear off tray */
	memcpy( rgb_under[ 0 ][ 0 ], bd->rgb_empty, sizeof( rgb_under ) );
    else {
	/* on board; copy saved image */
	p_src = bd->rgb_points;
	if( !( ( n ^ invert ^ fClockwise ) & 1 ) )
	    p_src += 6 * bd->board_size * 3;
	if( invert )
	    p_src += 36 * bd->board_size * 12 * bd->board_size * 3;
	
	p = rgb_under[ 0 ][ 0 ];
	for( ypix = 0; ypix < 30 * bd->board_size; ypix++ ) {
	    memcpy( p, p_src, 6 * bd->board_size * 3 );
	    p_src += 12 * bd->board_size * 3;
	    p += 6 * bd->board_size * 3;
	}
    }

    memcpy( rgb, rgb_under, sizeof( rgb ) );
    
    y_chequer = invert ? cy - 6 * bd->board_size : 0;
    
    i_chequer = 0;
    
    for( i = abs( bd->points[ n ] ); i; i-- ) {
	for( ypix = 0; ypix < 6 * bd->board_size; ypix++ )
	    for( xpix = 0; xpix < 6 * bd->board_size; xpix++ ) {
		i_refract = refract[ ypix * 6 * bd->board_size + xpix ];
		
		rgb[ ypix + y_chequer ][ xpix ][ 0 ] = clamp(
		    ( ( rgb_under[ y_chequer ][ i_refract ][ 0 ] *
			rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4 + 3 ] )
		      >> 8 ) + rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4
				   + 0 ] );
		rgb[ ypix + y_chequer ][ xpix ][ 1 ] = clamp(
		    ( ( rgb_under[ y_chequer ][ i_refract ][ 1 ] *
			rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4 + 3 ] )
		      >> 8 ) + rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4
				   + 1 ] );
		rgb[ ypix + y_chequer ][ xpix ][ 2 ] = clamp(
		    ( ( rgb_under[ y_chequer ][ i_refract ][ 2 ] *
			rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4 + 3 ] )
		      >> 8 ) + rgba[ ( ypix * 6 * bd->board_size + xpix ) * 4
				   + 2 ] );
	    }

	if( ++i_chequer == c_chequer )
	    break;
	
	y_chequer -= bd->board_size * positions[ fClockwise ][ n ][ 2 ];
    }

    gdk_draw_rgb_image( board->window, bd->gc_copy, x, y, cx, cy,
			GDK_RGB_DITHER_MAX, rgb[ 0 ][ 0 ],
			6 * bd->board_size * 3 );

    if( abs( bd->points[ n ] ) > c_chequer )
	board_label_chequers( board, bd, x, y + y_chequer,
			      abs( bd->points[ n ] ) );
}

static void board_redraw_point( GtkWidget *board, BoardData *bd, int n ) {

    int i, x, y, cx, cy, invert, y_chequer, c_chequer, i_chequer;
    
    if( bd->board_size <= 0 )
	return;

    if( bd->points[ n ] && bd->translucent ) {
	board_redraw_translucent( board, bd, n );
	return;
    }
    
    c_chequer = ( !n || n == 25 ) ? 3 : 5;

    point_area( bd, n, &x, &y, &cx, &cy );
    invert = positions[ fClockwise ][ n ][ 2 ] > 0;
    
    if( !bd->points[ n ] ) {
	/* point is empty; draw straight to screen and return */
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_board, x, y, x, y,
			 cx, cy );
	return;
    }
    
    gdk_draw_pixmap( bd->pm_point, bd->gc_copy, bd->pm_board, x, y, 0, 0,
		     cx, cy );

    y_chequer = invert ? cy - 6 * bd->board_size : 0;
    
    i_chequer = 0;
    
    gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_mask );
    
    for( i = abs( bd->points[ n ] ); i; i-- ) {
	gdk_gc_set_clip_origin( bd->gc_copy, 0, y_chequer );
	
	gdk_draw_pixmap( bd->pm_point, bd->gc_copy, bd->points[ n ] > 0 ?
			 bd->pm_o : bd->pm_x, 0, 0, 0, y_chequer,
			 6 * bd->board_size, 6 * bd->board_size );
	
	if( ++i_chequer == c_chequer )
	    break;

	y_chequer -= bd->board_size * positions[ fClockwise ][ n ][ 2 ];
    }

    gdk_gc_set_clip_mask( bd->gc_copy, NULL );
    
    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_point, 0, 0, x, y,
		     cx, cy );
    
    if( abs( bd->points[ n ] ) > c_chequer )
	board_label_chequers( board, bd, x, y + y_chequer,
			      abs( bd->points[ n ] ) );
}

static void board_redraw_die( GtkWidget *board, BoardData *bd, gint x, gint y,
			      gint colour, gint n ) {
    int pip[ 9 ];
    int ix, iy;
    
    if( bd->board_size <= 0 || !n || !GTK_WIDGET_MAPPED( board ) )
	return;

    gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_dice_mask );
    gdk_gc_set_clip_origin( bd->gc_copy, x, y );

    gdk_draw_pixmap( board->window, bd->gc_copy, colour < 0 ? bd->pm_x_dice :
		     bd->pm_o_dice, 0, 0, x, y, 7 * bd->board_size,
		     7 * bd->board_size );

    gdk_gc_set_clip_mask( bd->gc_copy, NULL );
    
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

/* Determine the position and rotation of the cube; *px and *py return the
   position (in board units -- multiply by bd->board_size to get
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

static void board_redraw_cube( GtkWidget *board, BoardData *bd ) {
    
    int x, y, orient;
#if !USE_GTK2
    int two_chars, lbearing[ 2 ], width[ 2 ], ascent[ 2 ], descent[ 2 ], n;
    char cube_text[ 3 ];
#endif
    
    if( bd->board_size <= 0 || bd->crawford_game || !bd->cube_use )
	return;

    cube_position( bd, &x, &y, &orient );
    x *= bd->board_size;
    y *= bd->board_size;

    gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_cube_mask );
    gdk_gc_set_clip_origin( bd->gc_copy, x, y );

    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_cube, 0, 0, x, y,
		     8 * bd->board_size, 8 * bd->board_size );

    gdk_gc_set_clip_mask( bd->gc_copy, NULL );

#if !USE_GTK2
    n = bd->doubled ? bd->cube << 1 : bd->cube;    
    sprintf( cube_text, "%d", n > 1 && n < 65 ? n : 64 );

    two_chars = n == 1 || n > 10;
    
    if( bd->cube_font_rotated ) {
	gdk_text_extents( bd->cube_font, cube_text, 1, &lbearing[ 0 ],
			  NULL, NULL, &ascent[ 0 ], &descent[ 0 ] );
	
	if( two_chars )
	    gdk_text_extents( bd->cube_font, cube_text + 1, 1, &lbearing[ 1 ],
			      NULL, NULL, &ascent[ 1 ], &descent[ 1 ] );
	else
	    lbearing[ 1 ] = ascent[ 1 ] = descent[ 1 ] = 0;

	if( orient ) {
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
			  NULL, NULL, &width[ 0 ], &ascent[ 0 ],
			  &descent[ 0 ] );

	gdk_draw_text( board->window, bd->cube_font, bd->gc_cube, x + 4 *
		       bd->board_size - width[ 0 ] / 2,
		       y + 4 * bd->board_size + ascent[ 0 ] / 2 - descent[ 0 ],
		       cube_text, two_chars + 1 );
    }
#endif
}

static gboolean board_expose( GtkWidget *board, GdkEventExpose *event,
			      BoardData *bd ) {
    int i, xCube, yCube;
    
    if( !bd->pm_board )
	return TRUE;

    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_board, event->area.x,
		     event->area.y, event->area.x, event->area.y,
		     event->area.width, event->area.height );

    for( i = 0; i < 28; i++ ) {
	int x, cx, y, cy;

	point_area( bd, i, &x, &y, &cx, &cy );
	
	if( intersects( event->area.x, event->area.y, event->area.width,
			event->area.height, x, y, cx, cy ) ) {
	    board_redraw_point( board, bd, i );
	    /* Redrawing the point might have drawn outside the exposed
	       area; enlarge the invalid area in case the dice now need
	       redrawing as well. */
	    if( event->area.x > x ) {
		event->area.width += event->area.x - x;
		event->area.x = x;
	    }
	    
	    if( event->area.x + event->area.width < x + cx )
		event->area.width = x + cx - event->area.x;
	    
	    if( event->area.y > y ) {
		event->area.height += event->area.y - y;
		event->area.y = y;
	    }
	    
	    if( event->area.y + event->area.height < y + cy )
		event->area.height = y + cy - event->area.y;
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

    cube_position( bd, &xCube, &yCube, NULL );
    xCube *= bd->board_size;
    yCube *= bd->board_size;
    
    if( intersects( event->area.x, event->area.y, event->area.width,
		    event->area.height, xCube, yCube,
		    8 * bd->board_size, 8 * bd->board_size ) )
	board_redraw_cube( board, bd );
    
    return TRUE;
}

static void board_expose_point( GtkWidget *board, BoardData *bd, int n ) {

    int x, y, cx, cy;
    
    if( bd->board_size <= 0 )
	return;

    point_area( bd, n, &x, &y, &cx, &cy );
    
#if GTK_CHECK_VERSION(2,0,0)
    {
	GdkRectangle r;
	
	r.x = x;
	r.y = y;
	r.width = cx;
	r.height = cy;
	
	gdk_window_invalidate_rect( board->window, &r, FALSE );
    }
#else
    {
	GdkEventExpose event;
    
	event.count = 0;
	event.area.x = x;
	event.area.y = y;
	event.area.width = cx;
	event.area.height = cy;
	
	board_expose( board, &event, bd );
    }
#endif
}

static int board_point( GtkWidget *board, BoardData *bd, int x0, int y0 ) {

    int i, x, y, cx, cy, xCube, yCube;


    x0 /= bd->board_size;
    y0 /= bd->board_size;

    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;
    
    cube_position( bd, &xCube, &yCube, NULL );
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_CUBE;
    
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

	x /= bd->board_size;
	y /= bd->board_size;
	cx /= bd->board_size;
	cy /= bd->board_size;
	
	if( intersects( x0, y0, 0, 0, x, y, cx, cy ) )
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

  if ( bd->show_pips ) {

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
        FormatMove( move, bd->old_board, bd->valid_move->anMove );
    
        UserCommand( move );
    } else
        /* Illegal move */
	board_beep( bd );
}

static void board_start_drag( GtkWidget *board, BoardData *bd,
			      int drag_point, int x, int y ) {

    bd->drag_point = drag_point;
    bd->drag_colour = bd->points[ drag_point ] < 0 ? -1 : 1;

    bd->points[ drag_point ] -= bd->drag_colour;

    board_expose_point( board, bd, drag_point );

#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( board->window, FALSE );
#endif
    
    bd->x_drag = x;
    bd->y_drag = y;

    if( bd->translucent )
	gdk_get_rgb_image( board->window,
			   gdk_window_get_colormap( board->window ),
			   bd->x_drag - 3 * bd->board_size,
			   bd->y_drag - 3 * bd->board_size,
			   6 * bd->board_size, 6 * bd->board_size,
			   bd->rgb_saved, 6 * bd->board_size * 3 );
    
    gdk_draw_pixmap( bd->pm_saved, bd->gc_copy, board->window,
		     bd->x_drag - 3 * bd->board_size,
		     bd->y_drag - 3 * bd->board_size,
		     0, 0, 6 * bd->board_size, 6 * bd->board_size );
}

static void board_drag( GtkWidget *board, BoardData *bd, int x, int y ) {
    
    GdkPixmap *pm_swap;
    guchar *rgb_swap;

    if( bd->translucent ) {
	int c, j;
	guchar *psrc, *pdest;
	short *refract = bd->drag_colour > 0 ? bd->ai_refract[ 1 ] :
	    bd->ai_refract[ 0 ];
	
#if GTK_CHECK_VERSION(2,0,0)
	GdkRegion *pr;
	GdkRectangle r;
	
	gdk_window_process_updates( board->window, FALSE );
#endif
	
	gdk_get_rgb_image( board->window,
			   gdk_window_get_colormap( board->window ),
			   x - 3 * bd->board_size, y - 3 * bd->board_size,
			   6 * bd->board_size, 6 * bd->board_size,
			   bd->rgb_temp_saved, 6 * bd->board_size * 3 );
	
#if GTK_CHECK_VERSION(2,0,0)
	r.x = x - 3 * bd->board_size;
	r.y = y - 3 * bd->board_size;
	r.width = 6 * bd->board_size;
	r.height = 6 * bd->board_size;
	
	pr = gdk_region_rectangle( &r );
	
	r.x = bd->x_drag - 3 * bd->board_size;
	r.y = bd->y_drag - 3 * bd->board_size;
	
	gdk_region_union_with_rect( pr, &r );
	
	gdk_window_begin_paint_region( board->window, pr );

	gdk_region_destroy( pr );
#endif
	
	copy_rgb( bd->rgb_saved, bd->rgb_temp_saved, 0, 0,
		  bd->x_drag - x, bd->y_drag - y,
		  6 * bd->board_size, 6 * bd->board_size,
		  6 * bd->board_size, 6 * bd->board_size,
		  6 * bd->board_size * 3, 6 * bd->board_size * 3 );

	c = 6 * bd->board_size * 6 * bd->board_size;
	for( j = 0, psrc = bd->drag_colour > 0 ? bd->rgba_o : bd->rgba_x,
		 pdest = bd->rgb_temp; j < c; j++ ) {
	    int r = refract[ j ] * 3;
	    
	    pdest[ 0 ] = clamp( ( ( bd->rgb_temp_saved[ r + 0 ] *
				    psrc[ 3 ] ) >> 8 ) + psrc[ 0 ] );
	    pdest[ 1 ] = clamp( ( ( bd->rgb_temp_saved[ r + 1 ] *
				    psrc[ 3 ] ) >> 8 ) + psrc[ 1 ] );
	    pdest[ 2 ] = clamp( ( ( bd->rgb_temp_saved[ r + 2 ] *
				    psrc[ 3 ] ) >> 8 ) + psrc[ 2 ] );
	    pdest += 3;
	    psrc += 4;
	}
	
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );
	
#if !GTK_CHECK_VERSION(2,0,0)
	gdk_draw_pixmap( bd->pm_saved, bd->gc_copy, board->window,
			 x - 3 * bd->board_size, y - 3 * bd->board_size,
			 0, 0, 6 * bd->board_size, 6 * bd->board_size );
#endif
	
	gdk_draw_rgb_image( board->window, bd->gc_copy,
			    x - 3 * bd->board_size,
			    y - 3 * bd->board_size,
			    6 * bd->board_size, 6 * bd->board_size,
			    GDK_RGB_DITHER_MAX, bd->rgb_temp,
			    6 * bd->board_size * 3 );
	
#if GTK_CHECK_VERSION(2,0,0)
	gdk_draw_rgb_image( bd->pm_saved, bd->gc_copy, 0, 0,
			    6 * bd->board_size, 6 * bd->board_size,
			    GDK_RGB_DITHER_MAX, bd->rgb_temp_saved,
			    6 * bd->board_size * 3 );
	
	gdk_window_end_paint( board->window );
#endif
	
	rgb_swap = bd->rgb_saved;
	bd->rgb_saved = bd->rgb_temp_saved;
	bd->rgb_temp_saved = rgb_swap;
    } else {
	gdk_draw_pixmap( bd->pm_temp_saved, bd->gc_copy, board->window,
			 x - 3 * bd->board_size, y - 3 * bd->board_size,
			 0, 0, 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_pixmap( bd->pm_temp_saved, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - x, bd->y_drag - y,
			 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_pixmap( bd->pm_temp, bd->gc_copy, bd->pm_temp_saved,
			 0, 0, 0, 0, 6 * bd->board_size,
			 6 * bd->board_size );
	
	gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_mask );
	gdk_gc_set_clip_origin( bd->gc_copy, 0, 0 );
	
	gdk_draw_pixmap( bd->pm_temp, bd->gc_copy, bd->drag_colour > 0 ?
			 bd->pm_o : bd->pm_x,
			 0, 0, 0, 0, 6 * bd->board_size,
			 6 * bd->board_size );
	
	gdk_gc_set_clip_mask( bd->gc_copy, NULL );
	
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_saved,
			 0, 0, bd->x_drag - 3 * bd->board_size,
			 bd->y_drag - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_temp,
			 0, 0, x - 3 * bd->board_size,
			 y - 3 * bd->board_size,
			 6 * bd->board_size, 6 * bd->board_size );
	
	pm_swap = bd->pm_saved;
	bd->pm_saved = bd->pm_temp_saved;
	bd->pm_temp_saved = pm_swap;
    }
    
    bd->x_drag = x;
    bd->y_drag = y;
}

static void board_end_drag( GtkWidget *board, BoardData *bd ) {
    
#if GTK_CHECK_VERSION(2,0,0)
    gdk_window_process_updates( board->window, FALSE );
#endif
    
    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_saved,
		     0, 0, bd->x_drag - 3 * bd->board_size,
		     bd->y_drag - 3 * bd->board_size,
		     6 * bd->board_size, 6 * bd->board_size );
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
	
	board_expose_point( board, bd, bar );
    }
    
    bd->points[ dest ] += bd->drag_colour;
    
    board_expose_point( board, bd, dest );
    
    if( bd->drag_point != dest )
	if( update_move( bd ) && !bd->permit_illegal ) {
	    /* the move was illegal; undo it */
	    bd->points[ dest ] -= bd->drag_colour;
	    bd->points[ bd->drag_point ] += bd->drag_colour;
	    if( hit ) {
		bd->points[ bar ] += bd->drag_colour;
		bd->points[ dest ] = -bd->drag_colour;
		board_expose_point( board, bd, bar );
	    }
		    
	    board_expose_point( board, bd, bd->drag_point );
	    board_expose_point( board, bd, dest );
	    update_move( bd );
	    placed = FALSE;
	}

    return placed;
}

/* jsc: Special version :( of board_point which also allows clicking on a
   small border and all bearoff trays */
static int board_point_with_border( GtkWidget *board, BoardData *bd, 
				    int x0, int y0 ) {
    int i, x, y, cx, cy, xCube, yCube;

    x0 /= bd->board_size;
    y0 /= bd->board_size;
    
    /* Similar to board_point, but adds the nasty y-=3 cy+=3 border
       allowances */
    
    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;
    
    for( i = 0; i < 30; i++ ) {
	point_area( bd, i, &x, &y, &cx, &cy );

	x /= bd->board_size;
	y /= bd->board_size;
	cx /= bd->board_size;
	cy /= bd->board_size;

	if( y < 36 )
	    y -= 3;

	cy += 3;

	if( intersects( x0, y0, 0, 0, x, y, cx, cy ) )
	    return i;
    }

    cube_position( bd, &xCube, &yCube, NULL );
    
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_CUBE;
    
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

    if( bd->board_size <= 0 )
	return -1;

    x0 /= bd->board_size;
    y0 /= bd->board_size;

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
	    board_expose_point( board, bd, i );
	}
	
	bd->points[ 26 ] = 15;
	bd->points[ 27 ] = -15;
	board_expose_point( board, bd, 26 );
	board_expose_point( board, bd, 27 );

	read_board( bd, points );
	update_position_id( bd, points );
        update_pipcount ( bd, points );

	return;
    } else if ( !dragging && ( n == POINT_UNUSED0 || n == POINT_UNUSED1 ) ) {
	/* click on unused bearoff tray in edit mode -- reset to starting
	   position */
	for( i = 0; i < 28; i++ )
	    bd->points[ i ] = 0;

	/* FIXME use appropriate position if playing nackgammon */
	
	bd->points[ 1 ] = -2;
	bd->points[ 6 ] = 5;
	bd->points[ 8 ] = 3;
	bd->points[ 12 ] = -5;
	bd->points[ 13 ] = 5;
	bd->points[ 17 ] = -3;
	bd->points[ 19 ] = -5;
	bd->points[ 24 ] = 2;
	
	for( i = 0; i < 28; i++ )
	    board_expose_point( board, bd, i );
	    
	read_board( bd, points );
	update_position_id( bd, points );
        update_pipcount ( bd, points );
    } else if( !dragging && n == POINT_CUBE ) {
	GTKSetCube( NULL, 0, NULL );
	return;
    } else if( !dragging && n == POINT_DICE ) {
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
	
	board_expose_point( board, bd, n );
	board_expose_point( board, bd, opponent_off );
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

    board_expose_point( board, bd, n );
    board_expose_point( board, bd, off );

    read_board( bd, points );
    update_position_id( bd, points );
    update_pipcount ( bd, points );
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
		
		board_redraw_dice( board, bd, 0 );
		board_redraw_dice( board, bd, 1 );
	    }
	    
	    return TRUE;
	}

	/* jsc: If playing, but not editing, and dice not rolled yet,
	   this code handles rolling the dice if bottom player clicks
	   the right side of the board, or the top player clicks the
	   left side of the board (his/her right side).  */
	if( bd->playing && 
	    ( !gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) ) 
	    && bd->dice[ 0 ] <= 0 ) {
	    if( ( bd->drag_point == POINT_RIGHT && bd->turn == 1 ) ||
		( bd->drag_point == POINT_LEFT  && bd->turn == -1 ) ) {
		/* NB: the UserCommand() call may cause reentrancies,
		   so it is vital to reset bd->drag_point first! */
		bd->drag_point = -1;
		UserCommand( "roll" );
	    } else {
		bd->drag_point = -1;
		board_beep( bd );
	    }
	    
	    return TRUE;
	}
	
	/* jsc: since we added new POINT_RIGHT and POINT_LEFT, we want
	   to make sure no ghost checkers can be dragged out of this *
	   region!  */
	if( ( bd->drag_point == POINT_RIGHT || 
	      bd->drag_point == POINT_LEFT ) ) {
	    bd->drag_point = -1;
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
	
	/* FIXME if the dice are set, and nDragPoint is 26 or 27 (i.e. off),
	   then bear off as many chequers as possible, and return. */
	
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
		    board_expose_point( board, bd, n[ 0 ] );
		    board_expose_point( board, bd, n[ 1 ] );
		    board_expose_point( board, bd, bd->drag_point );
		    board_expose_point( board, bd, bar );
			
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

		board_expose_point( board, bd, bd->drag_point );
		board_expose_point( board, bd, dest );
		
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

	board_drag( board, bd, x, y );
	
	break;
	
    case GDK_BUTTON_RELEASE:
	if( bd->drag_point < 0 )
	    break;
	
	board_end_drag( board, bd );
	
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
		    board_expose_point( board, bd, bd->drag_point );
		    
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
			board_expose_point( board, bd, bd->drag_point );
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

static void board_set_cube_font( GtkWidget *widget, BoardData *bd ) {
#if USE_GTK2
    PangoFontDescription *pfd;
    PangoLayout *pl;
    char sz[ 64 ];
    int cx, cy, n, orient;
    static void board_draw_cube( GtkWidget *widget, BoardData *bd );
    
    pl = gtk_widget_create_pango_layout( widget, "" );
    pfd = pango_font_description_new();
    pango_font_description_set_family_static( pfd, "utopia,serif" );
    pango_font_description_set_weight( pfd, PANGO_WEIGHT_BOLD );
    pango_font_description_set_size( pfd, bd->board_size * 5 * PANGO_SCALE );
    pango_layout_set_font_description( pl, pfd );
    
    n = bd->doubled ? bd->cube << 1 : bd->cube;    
    sprintf( sz, "%d", n > 1 && n < 65 ? n : 64 );

    pango_layout_set_text( pl, sz, -1 );

    pango_layout_get_pixel_size( pl, &cx, &cy );

    /* this is a bit grotty, but we need to erase the old cube digits */
    gdk_pixmap_unref( bd->pm_cube );
    board_draw_cube( widget, bd );
    
    gdk_draw_layout( bd->pm_cube, bd->gc_cube, ( bd->board_size * 8 - cx ) / 2,
		     ( bd->board_size * 8 - cy ) / 2, pl );
    
    cube_position( bd, NULL, NULL, &orient );
    if( orient != -1 ) {
	/* rotate the cube */
	GdkPixbuf *ppb;
	guchar *puch0, *puch1, *puch;
	int x, y, cx;
	
	if( ( ppb = gdk_pixbuf_get_from_drawable( NULL, bd->pm_cube, NULL,
						  bd->board_size,
						  bd->board_size, 0, 0,
						  6 * bd->board_size,
						  6 * bd->board_size ) ) ) {
	    cx = gdk_pixbuf_get_rowstride( ppb );
	    puch0 = gdk_pixbuf_get_pixels( ppb );
	    puch = puch1 = g_alloca( 6 * 6 * bd->board_size *
				     bd->board_size * 3 );

	    if( orient )
		/* upside down; start from bottom right */
		puch0 += ( 6 * bd->board_size - 1 ) * cx +
		    ( 6 * bd->board_size - 1 ) * 3;
	    else
		/* sideways; start from top right */
		puch0 += ( 6 * bd->board_size - 1 ) * 3;
	    
	    for( y = 0; y < 6 * bd->board_size; y++ ) {
		for( x = 0; x < 6 * bd->board_size; x++ ) {
		    *puch++ = puch0[ 0 ];
		    *puch++ = puch0[ 1 ];
		    *puch++ = puch0[ 2 ];

		    if( orient )
			/* upside down; move left */
			puch0 -= 3;
		    else
			/* sideways; move down */
			puch0 += cx;
		}
		if( orient )
		    /* upside down; move right and up */
		    puch0 += 6 * bd->board_size * 3 - cx;
		else
		    /* sideways; move up and left */
		    puch0 -= 6 * bd->board_size * cx + 3;
	    }
		    
	    g_object_unref( ppb );

	    gdk_draw_rgb_image( bd->pm_cube, bd->gc_cube, bd->board_size,
				bd->board_size, 6 * bd->board_size,
				6 * bd->board_size, GDK_RGB_DITHER_NONE,
				puch1, 6 * bd->board_size * 3 );
	}
    }

    pango_font_description_free( pfd );
    g_object_unref( pl );
#else
    char font_name[ 256 ];
    int i, orient, sizes[ 20 ] = { 34, 33, 26, 25, 24, 20, 19, 18, 17, 16, 15,
				   14, 13, 12, 11, 10, 9, 8, 7, 6 };
    
    /* FIXME query resource for font name and method */

    bd->cube_font_rotated = FALSE;

    cube_position( bd, NULL, NULL, &orient );
    
    /* First attempt (applies only if cube is not owned by the player at
       the bottom of the screen): try scaled, rotated font. */
#if !WIN32
    /* This #if is horribly ugly, but necessary -- the Win32 port of GTK+
       doesn't support rotated fonts, and not only fails, but crashes! */
    if( orient != -1 ) {
	sprintf( font_name, orient ?
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
#endif
    
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
    gdk_font_ref( bd->cube_font = gtk_style_get_font( widget->style ) );

 done:
    gdk_gc_set_font( bd->gc_cube, bd->cube_font );
#endif
}

static gint board_set( Board *board, const gchar *board_text ) {

    BoardData *bd = board->board_data;
    gchar *dest, buf[ 32 ];
    gint i, *pn, **ppn;
    gint old_board[ 28 ];
    int old_cube, old_doubled, old_crawford, old_xCube, old_yCube, editing;
    GdkEventExpose event;
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

    old_cube = bd->cube;
    old_doubled = bd->doubled;
    old_crawford = bd->crawford_game;
    
    cube_position( bd, &old_xCube, &old_yCube, NULL );
    
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
	
    if( bd->board_size <= 0 )
	return 0;

    if( bd->higher_die_first ) {
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
	    
	    for( iPoint = 1; iPoint <= 24; iPoint++ )
		if( abs( bd->points[ iPoint ] ) >= 5 ) {
		    point_area( bd, iPoint, &x, &y, &cx, &cy );
		    x /= bd->board_size;
		    y /= bd->board_size;
		    cx /= bd->board_size;
		    cy /= bd->board_size;
		    
		    if( ( intersects( bd->x_dice[ 0 ], bd->y_dice[ 0 ],
				      7, 7, x, y, cx, cy ) ||
			  intersects( bd->x_dice[ 1 ], bd->y_dice[ 1 ],
				      7, 7, x, y, cx, cy ) ) &&
			iAttempt++ < 0x80 )
			goto cocked;
		}
	    
	    bd->dice_colour[ 0 ] = bd->dice_colour[ 1 ] = bd->turn;
	}
    }

    if( bd->doubled != old_doubled || bd->cube != old_cube ||
	bd->cube_owner != bd->opponent_can_double - bd->can_double ||
	fCubeUse != bd->cube_use || bd->crawford_game != old_crawford ) {
	int xCube, yCube;

	bd->cube_owner = bd->opponent_can_double - bd->can_double;
	bd->cube_use = fCubeUse;
		
	/* erase old cube */
	event.count = 0;
	event.area.x = old_xCube * bd->board_size;
	event.area.y = old_yCube * bd->board_size;
	event.area.width = event.area.height = 8 * bd->board_size;
	
	board_expose( bd->drawing_area, &event, bd );

	/* draw new cube */
	cube_position( bd, &xCube, &yCube, NULL );
	
	event.area.x = xCube * bd->board_size;
	event.area.y = yCube * bd->board_size;

#if !USE_GTK2
	gdk_font_unref( bd->cube_font );
#endif
	board_set_cube_font( GTK_WIDGET( board ), bd );

	board_expose( bd->drawing_area, &event, bd );
    }

    for( i = 0; i < 28; i++ )
	if( ( bd->points[ i ] != old_board[ i ] ) ||
	    ( fClockwise != bd->clockwise ) )
	    board_redraw_point( bd->drawing_area, bd, i );

    /* FIXME only redraw dice/cube if changed */
    board_redraw_dice( bd->drawing_area, bd, 0 );
    board_redraw_dice( bd->drawing_area, bd, 1 );
    board_redraw_cube( bd->drawing_area, bd );

    if( fClockwise != bd->clockwise ) {
	/* This is complete overkill, but we need to recalculate the
	   board pixmap if the points are numbered and the direction
	   changes. */
	board_free_pixmaps( bd );
	board_create_pixmaps( pwBoard, bd );
	gtk_widget_queue_draw( bd->drawing_area );	
    }
    
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
		board_expose_point( pbd->drawing_area, pbd,
				    animate_player ? 0 : 25 );
	    }
	}
	
    	pbd->points[ src ] -= colour;
	pbd->points[ dest ] += colour;
    }

    board_expose_point( pbd->drawing_area, pbd, src );
    board_expose_point( pbd->drawing_area, pbd, dest );

    if( !( blink_count & 1 ) && blink_count < 4 ) {
	pbd->points[ src ] = src_cheq;
	pbd->points[ dest ] = dest_cheq;
    }
    
    if( blink_count++ >= 4 ) {
	blink_count = 0;	
	blink_move += 2;	
    }

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
	board_start_drag( pbd->drawing_area, pbd, src, x * pbd->board_size,
			  y * pbd->board_size );
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
		board_expose_point( pbd->drawing_area, pbd,
				    animate_player ? 0 : 25 );
	    }
	    
	    pbd->points[ dest ] += colour;
	    board_expose_point( pbd->drawing_area, pbd, dest );
	    pbd->drag_point = -1;
	    slide_phase = 0;
	    slide_move += 2;

	    playSound( SOUND_CHEQUER );

	    return TRUE;
	}
	break;
	
    default:
	g_assert_not_reached();
    }

    board_drag( pbd->drawing_area, pbd, x * pbd->board_size,
		y * pbd->board_size );
    
    return TRUE;
}

extern void board_animate( Board *board, int move[ 8 ], int player ) {

    BoardData *pbd = board->board_data;
    int n;
    monitor m;
	
    if( pbd->animate_computer_moves == ANIMATE_NONE )
	return;

    animate_move_list = move;
    animate_player = player;

    animation_finished = FALSE;
    
    if( pbd->animate_computer_moves == ANIMATE_BLINK )
	n = gtk_timeout_add( 0x300 >> pbd->animate_speed, board_blink_timeout,
			     board );
    else /* ANIMATE_SLIDE */
	n = gtk_timeout_add( 0x100 >> pbd->animate_speed, board_slide_timeout,
			     board );

    while( !animation_finished ) {
	SuspendInput( &m );
	gtk_main_iteration();
	ResumeInput( &m );
    }
}

static gboolean dice_expose( GtkWidget *dice, GdkEventExpose *event,
			     BoardData *bd ) {
    
    if( !bd->pm_board )
	return TRUE;

    /* FIXME only redraw if xexpose.count == 0 */

    board_redraw_die( dice, bd, 0, 0, bd->turn, bd->dice_roll[ 0 ] );
    board_redraw_die( dice, bd, 8 * bd->board_size, 0, bd->turn,
		      bd->dice_roll[ 1 ] );

    return TRUE;
}

extern gint game_set_old_dice( Board *board, gint die0, gint die1 ) {

    BoardData *pbd = board->board_data;
    
    pbd->dice_roll[ 0 ] = die0;
    pbd->dice_roll[ 1 ] = die1;

    if( pbd->higher_die_first && pbd->dice_roll[ 0 ] < pbd->dice_roll[ 1 ] )
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
    

    if ( pbd->usedicearea ) {

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
		      gint computer_turn ) {
    gchar board_str[ 256 ];
    BoardData *pbd = board->board_data;
    int old_points[ 2 ][ 25 ];
    
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
	       ms.fTurn, ms.fCrawford);
    
    board_set( board, board_str );
    
    /* FIXME update names, score, match length */
    if( pbd->board_size <= 0 )
	return 0;

    pbd->computer_turn = computer_turn;
    
    if( die0 ) {
	if( pbd->higher_die_first && die0 < die1 )
	    swap( &die0, &die1 );
	
	pbd->dice_roll[ 0 ] = die0;
	pbd->dice_roll[ 1 ] = die1;
    }

    update_buttons( pbd );

    /* FIXME it's overkill to do this every time, but if we don't do it,
       then "set turn <player>" won't redraw the dice in the other colour. */

    dice_expose( pbd->dice_area, NULL, pbd );

    return 0;
}

extern void board_free_pixmaps( BoardData *bd ) {

    if( bd->translucent ) {
	free( bd->rgba_x );
	free( bd->rgba_o );
	free( bd->ai_refract[ 0 ] );
	free( bd->ai_refract[ 1 ] );
	free( bd->rgb_points );
	free( bd->rgb_empty );
	free( bd->rgb_bar0 );
	free( bd->rgb_bar1 );
	free( bd->rgb_saved );
	free( bd->rgb_temp );
	free( bd->rgb_temp_saved );
    } else {
	gdk_pixmap_unref( bd->pm_x );
	gdk_pixmap_unref( bd->pm_o );
	gdk_bitmap_unref( bd->bm_mask );
	gdk_pixmap_unref( bd->pm_point );
	gdk_pixmap_unref( bd->pm_temp );
	gdk_pixmap_unref( bd->pm_temp_saved );
    }
    
    gdk_pixmap_unref( bd->pm_saved );
    gdk_pixmap_unref( bd->pm_board );
    gdk_pixmap_unref( bd->pm_x_dice );
    gdk_pixmap_unref( bd->pm_o_dice );
    gdk_pixmap_unref( bd->pm_x_pip );
    gdk_pixmap_unref( bd->pm_o_pip );
    gdk_pixmap_unref( bd->pm_cube );
    gdk_bitmap_unref( bd->bm_dice_mask );
    gdk_bitmap_unref( bd->bm_cube_mask );

#if !USE_GTK2
    gdk_font_unref( bd->cube_font );
#endif
}

#if USE_GTK2
static void board_draw_labels( GtkWidget *pw, BoardData *bd ) {

    PangoFontDescription *pfd;
    PangoLayout *pl;
    GdkGC *gc;
    GdkGCValues gcv;
    int i, cx, cy;
    char sz[ 64 ];
    
    pl = gtk_widget_create_pango_layout( pw, "" );
    pfd = pango_font_description_new();
    pango_font_description_set_size( pfd, bd->board_size * 2 * PANGO_SCALE );
    pango_layout_set_font_description( pl, pfd );
    
    gcv.foreground.pixel = gdk_rgb_xpixel_from_rgb( 0xFFFFFF );
    gc = gtk_gc_get( pw->style->depth, pw->style->colormap, &gcv,
		     GDK_GC_FOREGROUND );

    for( i = 1; i <= 24; i++ ) {
        sprintf( sz, "%d", i );

	pango_layout_set_text( pl, sz, -1 );

	pango_layout_get_pixel_size( pl, &cx, &cy );
	
	gdk_draw_layout( bd->pm_board, gc,
			 ( positions[ fClockwise ][ i ][ 0 ] + 3 ) *
			 bd->board_size - cx / 2,
			 ( ( i > 12 ? 3 : 141 ) * bd->board_size - cy ) / 2,
			 pl );
    }

    gtk_gc_release( gc );
    pango_font_description_free( pfd );
    g_object_unref( pl );
}
#else
static void board_draw_labels( BoardData *bd ) {

    int i;
    GdkGC *gc;
    GdkGCValues gcv;
    char sz[ 64 ];
    int cx, cy0, cy1;
    
    gcv.foreground.pixel = gdk_rgb_xpixel_from_rgb( 0xFFFFFF );

    for( i = bd->board_size * 2; i; i-- ) {
	sprintf( sz, "-*-helvetica-medium-r-normal-*-%d-*-*-*-p-*-"
		 "iso8859-1", i );
	if( ( gcv.font = gdk_font_load( sz ) ) )
	    break;
    }

    if( !gcv.font ) {
	for( i = bd->board_size * 2; i; i-- ) {
	    sprintf( sz, "-bitstream-charter-medium-r-normal--%d-*-*-*-p-*-"
		     "iso8859-1", i );
	    if( ( gcv.font = gdk_font_load( sz ) ) )
		break;
	}

	if( !gcv.font )
	    return;
    }

    gc = gdk_gc_new_with_values( bd->pm_board, &gcv,
				 GDK_GC_FOREGROUND | GDK_GC_FONT );
    
    for( i = 1; i <= 24; i++ ) {
        sprintf( sz, "%d", i );

        gdk_string_extents( gcv.font, sz, NULL, NULL, &cx, &cy0, &cy1 );
	
        gdk_draw_string( bd->pm_board, gcv.font, gc,
	   ( positions[ fClockwise ][ i ][ 0 ] + 3 ) * bd->board_size - cx / 2,
	   ( ( i > 12 ? 3 : 141 ) * bd->board_size + cy0 ) / 2 -
	   cy1, sz );
    }

    gdk_gc_unref( gc );
    gdk_font_unref( gcv.font );
}
#endif

static guchar board_pixel( BoardData *bd, int i, int antialias, int j ) {

    return clamp( ( ( (int) bd->aanBoardColour[ 0 ][ j ] -
		      (int) bd->aSpeckle[ 0 ] / 2 +
		      (int) RAND % ( bd->aSpeckle[ 0 ] + 1 ) ) *
		    ( 20 - antialias ) +
		    ( (int) bd->aanBoardColour[ i ][ j ] -
		      (int) bd->aSpeckle[ i ] / 2 +
		      (int) RAND % ( bd->aSpeckle[ i ] + 1 ) ) *
		    antialias ) * ( bd->arLight[ 2 ] * 0.8 + 0.2 ) / 20 );
}

static float wood_hash( float r ) {
    /* A quick and dirty hash function for floating point numbers; returns
       a value in the range [0,1). */

    int n;
    float x;

    if( !r )
	return 0;
    
    x = frexp( r, &n );

    return fabs( frexp( x * 131073.1294427 + n, &n ) ) * 2 - 1;
}

static void wood_pixel( float x, float y, float z, unsigned char auch[ 3 ],
			BoardWood wood ) {

    float r;
    int grain, figure;

    r = sqrt( x * x + y * y );
    r -= z / 60;

    switch( wood ) {
    case WOOD_ALDER:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2;
	    auch[ 1 ] = 100 - grain;
	    auch[ 2 ] = 20 - grain / 2;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 210 + ( grain - 10 ) * 2;
	    auch[ 1 ] = 90 + ( grain - 10 );
	    auch[ 2 ] = 15 + ( grain - 10 ) / 2;
	} else {
	    auch[ 0 ] = 230 + ( grain % 3 );
	    auch[ 1 ] = 100 + ( grain % 3 );
	    auch[ 2 ] = 20 + ( grain % 3 );
	}
	
	figure = r / 29 + x / 15 + y / 17 + z / 29;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= wood_hash( figure + grain ) * 8;
	    auch[ 1 ] -= wood_hash( figure + grain ) * 4;
	    auch[ 2 ] -= wood_hash( figure + grain ) * 2;
	}
	
	break;
	
    case WOOD_ASH:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= wood_hash( (int) r / 60 ) * 0.7 + 0.3;
	
	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 125 - grain / 2;
	auch[ 2 ] = 20 - grain / 8;

	figure = r / 53 + x / 5 + y / 7 + z / 50;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= wood_hash( figure + grain ) * 16;
	    auch[ 1 ] -= wood_hash( figure + grain ) * 8;
	    auch[ 2 ] -= wood_hash( figure + grain ) * 4;
	}

	break;
	
    case WOOD_BASSWOOD:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain > 50 )
	    grain = 60 - grain;
	else if( grain > 40 )
	    grain = 10;
	else if( grain > 30 )
	    grain -= 30;
	else
	    grain = 0;
 
	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 205 - grain;
	auch[ 2 ] = 150 - grain;
	
	break;
	
    case WOOD_BEECH:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 230 - grain;
	auch[ 1 ] = 125 - grain / 2;
	auch[ 2 ] = 20 - grain / 8;

	figure = r / 29 + x / 15 + y / 17 + z / 29;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= wood_hash( figure + grain ) * 16;
	    auch[ 1 ] -= wood_hash( figure + grain ) * 8;
	    auch[ 2 ] -= wood_hash( figure + grain ) * 4;
	}

	break;
	
    case WOOD_CEDAR:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 + grain;
	    auch[ 1 ] = 135 + grain;
	    auch[ 2 ] = 85 + grain / 2;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 240 - ( grain - 10 ) * 3;
	    auch[ 1 ] = 145 - ( grain - 10 ) * 3;
	    auch[ 2 ] = 90 - ( grain - 10 ) * 3 / 2;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 200 + grain;
	    auch[ 1 ] = 105 + grain;
	    auch[ 2 ] = 70 + grain / 2;
	} else {
	    auch[ 0 ] = 230 + ( grain % 3 );
	    auch[ 1 ] = 135 + ( grain % 3 );
	    auch[ 2 ] = 85 + ( grain % 3 );
	}
	
	break;
	
    case WOOD_EBONY:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 30 + grain / 4;
	auch[ 1 ] = 10 + grain / 8;
	auch[ 2 ] = 0;

	break;
	
    case WOOD_FIR:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2 + grain % 3 * 3;
	    auch[ 1 ] = 100 - grain * 2 + grain % 3 * 3;
	    auch[ 2 ] = 20 - grain + grain % 3 * 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 210 + grain % 3 * 3;
	    auch[ 1 ] = 80 + grain % 3 * 3;
	    auch[ 2 ] = 10 + grain % 3 * 3;
	} else if( grain < 40 ) {
	    auch[ 0 ] = 210 + ( grain - 30 ) * 2 + grain % 3 * 3;
	    auch[ 1 ] = 80 + ( grain - 30 ) * 2 + grain % 3 * 3;
	    auch[ 2 ] = 10 + ( grain - 30 ) + grain % 3 * 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3 * 5;
	    auch[ 1 ] = 100 + grain % 3 * 5;
	    auch[ 2 ] = 20 + grain % 3 * 5;
	}
	
	break;
	
    case WOOD_MAPLE:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 - grain * 2 + grain % 3;
	    auch[ 1 ] = 180 - grain * 2 + grain % 3;
	    auch[ 2 ] = 50 - grain + grain % 3;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 210 + grain % 3;
	    auch[ 1 ] = 160 + grain % 3;
	    auch[ 2 ] = 40 + grain % 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 210 + ( grain - 20 ) * 2 + grain % 3;
	    auch[ 1 ] = 160 + ( grain - 20 ) * 2 + grain % 3;
	    auch[ 2 ] = 40 + ( grain - 20 ) + grain % 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3;
	    auch[ 1 ] = 180 + grain % 3;
	    auch[ 2 ] = 50 + grain % 3;
	}

	break;
	    
    case WOOD_OAK:
	r *= 4;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= wood_hash( (int) r / 60 ) * 0.7 + 0.3;
	
	auch[ 0 ] = 230 + grain / 2;
	auch[ 1 ] = 125 + grain / 3;
	auch[ 2 ] = 20 + grain / 8;

	figure = r / 53 + x / 5 + y / 7 + z / 30;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= wood_hash( figure + grain ) * 32;
	    auch[ 1 ] -= wood_hash( figure + grain ) * 16;
	    auch[ 2 ] -= wood_hash( figure + grain ) * 8;
	}

	break;
	
    case WOOD_PINE:
	r *= 2;
	grain = ( (int) r % 60 );

	if( grain < 10 ) {
	    auch[ 0 ] = 230 + grain * 2 + grain % 3 * 3;
	    auch[ 1 ] = 160 + grain * 2 + grain % 3 * 3;
	    auch[ 2 ] = 50 + grain + grain % 3 * 3;
	} else if( grain < 20 ) {
	    auch[ 0 ] = 250 + grain % 3;
	    auch[ 1 ] = 180 + grain % 3;
	    auch[ 2 ] = 60 + grain % 3;
	} else if( grain < 30 ) {
	    auch[ 0 ] = 250 - ( grain - 20 ) * 2 + grain % 3;
	    auch[ 1 ] = 180 - ( grain - 20 ) * 2 + grain % 3;
	    auch[ 2 ] = 50 - ( grain - 20 ) + grain % 3;
	} else {
	    auch[ 0 ] = 230 + grain % 3 * 3;
	    auch[ 1 ] = 160 + grain % 3 * 3;
	    auch[ 2 ] = 50 + grain % 3 * 3;
	}

	break;
	
    case WOOD_REDWOOD:
	r *= 5;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 220 - grain;
	auch[ 1 ] = 70 - grain / 2;
	auch[ 2 ] = 40 - grain / 4;

	break;
	    
    case WOOD_WALNUT:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	grain *= wood_hash( (int) r / 60 ) * 0.7 + 0.3;
 
	auch[ 0 ] = 80 + ( grain * 3 / 2 );
	auch[ 1 ] = 40 + grain;
	auch[ 2 ] = grain / 2;

	break;
	
    case WOOD_WILLOW:
	r *= 3;
	grain = ( (int) r % 60 );

	if( grain > 40 )
	    grain = 120 - 2 * grain;

	auch[ 0 ] = 230 + grain / 3;
	auch[ 1 ] = 100 + grain / 5;
	auch[ 2 ] = 20 + grain / 10;

	figure = r / 60 + z / 30;
	
	if( figure % 3 == ( grain / 3 ) % 3 ) {
	    auch[ 0 ] -= wood_hash( figure + grain ) * 16;
	    auch[ 1 ] -= wood_hash( figure + grain ) * 8;
	    auch[ 2 ] -= wood_hash( figure + grain ) * 4;
	}

	break;
	
    default:
	assert( FALSE );
    }
}

static void board_draw_frame_wood( BoardData *bd, GdkGC *gc ) {

    int i, x, y, nSpecularTop, anSpecular[ 4 ][ bd->board_size ], nSpecular,
	s = bd->board_size;
    unsigned char left[ s * 72 ][ s * 3 ][ 3 ], right[ s * 72 ][ s * 3 ][ 3 ],
	top[ s * 3 ][ s * 108 ][ 3 ], bottom[ s * 3 ][ s * 108 ][ 3 ],
	lbar[ s * 72 ][ s * 6 ][ 3 ], rbar[ s * 72 ][ s * 6 ][ 3 ],
	lsep[ s * 68 ][ s * 3 ][ 3 ], rsep[ s * 68 ][ s * 3 ][ 3 ],
	ldiv[ s * 6 ][ s * 8 ][ 3 ], rdiv[ s * 6 ][ s * 8 ][ 3 ],
	a[ 3 ];
    float rx, rz, arDiffuse[ 4 ][ s ], cos_theta, rDiffuseTop,
	arHeight[ s ], rHeight, rDiffuse;

    nSpecularTop = pow( bd->arLight[ 2 ], 20 ) * 0.6 * 0x100;
    rDiffuseTop = 0.8 * bd->arLight[ 2 ] + 0.2;
	
    for( x = 0; x < s; x++ ) {
	rx = 1.0 - ( (float) x / s );
	rz = ssqrt( 1.0 - rx * rx );
	arHeight[ x ] = rz * s;
	    
	for( i = 0; i < 4; i++ ) {
	    cos_theta = bd->arLight[ 2 ] * rz + bd->arLight[ i & 1 ] * rx;
	    if( cos_theta < 0 )
		cos_theta = 0;
	    arDiffuse[ i ][ x ] = 0.8 * cos_theta + 0.2;
	    cos_theta = 2 * rz * cos_theta - bd->arLight[ 2 ];
	    anSpecular[ i ][ x ] = pow( cos_theta, 20 ) * 0.6 * 0x100;

	    if( !( i & 1 ) )
		rx = -rx;
	}
    }

    /* Top and bottom edges */
    for( y = 0; y < s * 3; y++ )
	for( x = 0; x < s * 108; x++ ) {
	    if( y < s ) {
		rDiffuse = arDiffuse[ 3 ][ y ];
		nSpecular = anSpecular[ 3 ][ y ];
		rHeight = arHeight[ y ];
	    } else if( y < 2 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 1 ][ 3 * s - y - 1 ];
		nSpecular = anSpecular[ 1 ][ 3 * s - y - 1 ];
		rHeight = arHeight[ 3 * s - y - 1 ];
	    }
	    
	    wood_pixel( 100 - y * 0.85 + x * 0.1, rHeight - x * 0.11,
			200 + x * 0.93 + y * 0.16, a, bd->wood );
	    
	    for( i = 0; i < 3; i++ )
		top[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	    
	    wood_pixel( 123 + y * 0.87 - x * 0.08, rHeight + x * 0.06,
			-100 - x * 0.94 - y * 0.11, a, bd->wood );
	    
	    for( i = 0; i < 3; i++ )
		bottom[ y ][ x ][ i ] =
		    clamp( a[ i ] * rDiffuse + nSpecular );
	}
    
    gdk_draw_rgb_image( bd->pm_board, gc, 0, 0, 108 * s,
			3 * s, GDK_RGB_DITHER_MAX,
			top[ 0 ][ 0 ], 108 * s * 3 );
    gdk_draw_rgb_image( bd->pm_board, gc, 0, 69 * s,
			108 * s,
			3 * s, GDK_RGB_DITHER_MAX,
			bottom[ 0 ][ 0 ], 108 * s * 3 );
    
    /* Left and right edges */
    for( y = 0; y < s * 72; y++ )
	for( x = 0; x < s * 3; x++ ) {
	    if( x < s ) {
		rDiffuse = arDiffuse[ 2 ][ x ];
		nSpecular = anSpecular[ 2 ][ x ];
		rHeight = arHeight[ x ];
	    } else if( x < 2 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 0 ][ 3 * s - x - 1 ];
		nSpecular = anSpecular[ 0 ][ 3 * s - x - 1 ];
		rHeight = arHeight[ 3 * s - x - 1 ];
	    }
    
	    wood_pixel( 300 + x * 0.9 + y * 0.1, rHeight + y * 0.06,
			200 - y * 0.9 + x * 0.1, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		left[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	    
	    wood_pixel( -100 - x * 0.86 + y * 0.13, rHeight - y * 0.07,
			300 + y * 0.92 + x * 0.08, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		right[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	}

    /* Four outer corners */
    for( y = 0; y < s * 3; y++ )
	for( x = y; x < s * 3; x++ )
	    for( i = 0; i < 3; i++ ) {
		left[ y ][ x ][ i ] = top[ y ][ x ][ i ];
		left[ 72 * s - y - 1 ][ x ][ i ] =
		    bottom[ 3 * s - y - 1 ][ x ][ i ];
		right[ y ][ 3 * s - x - 1 ][ i ] =
		    top[ y ][ 108 * s - x - 1 ][ i ];
		right[ 72 * s - y - 1 ][ 3 * s - x - 1 ][ i ] =
		    bottom[ 3 * s - y - 1 ][ 108 * s - x - 1 ][ i ];
	    }
    
    gdk_draw_rgb_image( bd->pm_board, gc, 0, 0, 3 * s,
			72 * s, GDK_RGB_DITHER_MAX,
			left[ 0 ][ 0 ], 3 * s * 3 );
    
    gdk_draw_rgb_image( bd->pm_board, gc, 105 * s, 0, 3 * s,
			72 * s, GDK_RGB_DITHER_MAX,
			right[ 0 ][ 0 ], 3 * s * 3 );
    
    /* Bar */
    for( y = 0; y < s * 72; y++ )
	for( x = 0; x < s * 6; x++ ) {
	    if( y < s && y < x && y < s * 6 - x - 1 ) {
		rDiffuse = arDiffuse[ 3 ][ y ];
		nSpecular = anSpecular[ 3 ][ y ];
		rHeight = arHeight[ y ];
	    } else if( y > 71 * s && s * 72 - y - 1 < x &&
		       s * 72 - y - 1 < s * 6 - x - 1 ) {
		rDiffuse = arDiffuse[ 1 ][ 72 * s - y - 1 ];
		nSpecular = anSpecular[ 1 ][ 72 * s - y - 1 ];
		rHeight = arHeight[ 72 * s - y - 1 ];
	    } else if( x < s ) {
		rDiffuse = arDiffuse[ 2 ][ x ];
		nSpecular = anSpecular[ 2 ][ x ];
		rHeight = arHeight[ x ];
	    } else if( x < 5 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 0 ][ 6 * s - x - 1 ];
		nSpecular = anSpecular[ 0 ][ 6 * s - x - 1 ];
		rHeight = arHeight[ 6 * s - x - 1 ];
	    }
    
	    wood_pixel( 100 - x * 0.88 + y * 0.08, 50 + rHeight - y * 0.1,
			-200 + y * 0.99 - x * 0.12, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		lbar[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	    
	    wood_pixel( 100 - x * 0.86 + y * 0.02, 50 + rHeight - y * 0.07,
			200 - y * 0.92 + x * 0.03, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		rbar[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	}

    /* Four bar corners */
    for( y = 0; y < s * 3; y++ )
	for( x = 0; x < 3 * s - y - 1; x++ )
	    for( i = 0; i < 3; i++ ) {
		lbar[ y ][ x ][ i ] = top[ y ][ 48 * s + x ][ i ];
		lbar[ 72 * s - y - 1 ][ x ][ i ] =
		    bottom[ 3 * s - y - 1 ][ 48 * s + x ][ i ];
		rbar[ y ][ 6 * s - x - 1 ][ i ] =
		    top[ y ][ 60 * s - x - 1 ][ i ];
		rbar[ 72 * s - y - 1 ][ 6 * s - x - 1 ][ i ] =
		    bottom[ 3 * s - y - 1 ][ 60 * s - x - 1 ][ i ];
	    }
    
    gdk_draw_rgb_image( bd->pm_board, gc, 48 * s, 0, 6 * s,
			72 * s, GDK_RGB_DITHER_MAX,
			lbar[ 0 ][ 0 ], 6 * s * 3 );
    
    gdk_draw_rgb_image( bd->pm_board, gc, 54 * s, 0,
			6 * s, 72 * s, GDK_RGB_DITHER_MAX,
			rbar[ 0 ][ 0 ], 6 * s * 3 );

    /* Left and right separators (between board and bearoff tray) */
    for( y = 0; y < s * 68; y++ )
	for( x = 0; x < s * 3; x++ ) {
	    if( x < s ) {
		rDiffuse = arDiffuse[ 2 ][ x ];
		nSpecular = anSpecular[ 2 ][ x ];
		rHeight = arHeight[ x ];
	    } else if( x < 2 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 0 ][ 3 * s - x - 1 ];
		nSpecular = anSpecular[ 0 ][ 3 * s - x - 1 ];
		rHeight = arHeight[ 3 * s - x - 1 ];
	    }
    
	    wood_pixel( -300 - x * 0.91 + y * 0.1, rHeight + y * 0.02,
			-200 + y * 0.94 - x * 0.06, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		lsep[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	    
	    wood_pixel( 100 - x * 0.89 - y * 0.07, rHeight + y * 0.05,
			300 - y * 0.94 + x * 0.11, a, bd->wood );

	    for( i = 0; i < 3; i++ )
		rsep[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	}

    /* Eight separator corners */
    for( y = 0; y < s; y++ )
	for( x = s - y - 1; x >= 0; x-- )
	    for( i = 0; i < 3; i++ ) {
		lsep[ y ][ x ][ i ] = top[ y + 2 * s ][ x + 9 * s ][ i ];
		lsep[ y ][ 3 * s - x - 1 ][ i ] =
		    top[ y + 2 * s ][ 12 * s - x - 1 ][ i ];
		lsep[ 68 * s - y - 1 ][ x ][ i ] =
		    bottom[ s - y - 1 ][ x + 9 * s ][ i ];
		lsep[ 68 * s - y - 1 ][ 3 * s - x - 1 ][ i ] =
		    bottom[ s - y - 1 ][ 12 * s - x - 1 ][ i ];
		
		rsep[ y ][ x ][ i ] = top[ y + 2 * s ][ x + 96 * s ][ i ];
		rsep[ y ][ 3 * s - x - 1 ][ i ] =
		    top[ y + 2 * s ][ 99 * s - x - 1 ][ i ];
		rsep[ 68 * s - y - 1 ][ x ][ i ] =
		    bottom[ s - y - 1 ][ x + 96 * s ][ i ];
		rsep[ 68 * s - y - 1 ][ 3 * s - x - 1 ][ i ] =
		    bottom[ s - y - 1 ][ 99 * s - x - 1 ][ i ];
	    }
    
    gdk_draw_rgb_image( bd->pm_board, gc, 9 * s, 2 * s, 3 * s,
			68 * s, GDK_RGB_DITHER_MAX,
			lsep[ 0 ][ 0 ], 3 * s * 3 );
    
    gdk_draw_rgb_image( bd->pm_board, gc, 96 * s, 2 * s, 3 * s,
			68 * s, GDK_RGB_DITHER_MAX,
			rsep[ 0 ][ 0 ], 3 * s * 3 );

    /* Left and right dividers (between the bearoff trays) */
    for( y = 0; y < s * 6; y++ )
	for( x = 0; x < s * 8; x++ ) {
	    if( y < s ) {
		rDiffuse = arDiffuse[ 3 ][ y ];
		nSpecular = anSpecular[ 3 ][ y ];
		rHeight = arHeight[ y ];
	    } else if( y < 5 * s ) {
		rDiffuse = rDiffuseTop;
		nSpecular = nSpecularTop;
		rHeight = s;
	    } else {
		rDiffuse = arDiffuse[ 1 ][ 6 * s - y - 1 ];
		nSpecular = anSpecular[ 1 ][ 6 * s - y - 1 ];
		rHeight = arHeight[ 6 * s - y - 1 ];
	    }
	    
	    wood_pixel( -100 - y * 0.85 + x * 0.11, rHeight - x * 0.04,
			-100 - x * 0.93 + y * 0.08, a, bd->wood );
	    
	    for( i = 0; i < 3; i++ )
		ldiv[ y ][ x ][ i ] = clamp( a[ i ] * rDiffuse +
					     nSpecular );
	    
	    wood_pixel( -123 - y * 0.93 - x * 0.12, rHeight + x * 0.11,
			-150 + x * 0.88 - y * 0.07, a, bd->wood );
	    
	    for( i = 0; i < 3; i++ )
		rdiv[ y ][ x ][ i ] =
		    clamp( a[ i ] * rDiffuse + nSpecular );
	}
    
    /* Eight divider corners */
    for( y = 0; y < s; y++ )
	for( x = s - y - 1; x >= 0; x-- )
	    for( i = 0; i < 3; i++ ) {
		ldiv[ y ][ x ][ i ] = left[ y + 33 * s ][ x + 2 * s ][ i ];
		ldiv[ y ][ 8 * s - x - 1 ][ i ] =
		    lsep[ y + 31 * s ][ s - x - 1 ][ i ];
		ldiv[ 6 * s - y - 1 ][ x ][ i ] =
		    left[ 39 * s - y - 1 ][ x + 2 * s ][ i ];
		ldiv[ 6 * s - y - 1 ][ 8 * s - x - 1 ][ i ] =
		    lsep[ 37 * s - y - 1 ][ s - x - 1 ][ i ];

		rdiv[ y ][ x ][ i ] = rsep[ y + 31 * s ][ x + 2 * s ][ i ];
		rdiv[ y ][ 8 * s - x - 1 ][ i ] =
		    right[ y + 33 * s ][ s - x - 1 ][ i ];
		rdiv[ 6 * s - y - 1 ][ x ][ i ] =
		    rsep[ 37 * s - y - 1 ][ x + 2 * s ][ i ];
		rdiv[ 6 * s - y - 1 ][ 8 * s - x - 1 ][ i ] =
		    right[ 39 * s - y - 1 ][ s - x - 1 ][ i ];
	    }
    
    gdk_draw_rgb_image( bd->pm_board, gc, 2 * s, 33 * s, 8 * s,
			6 * s, GDK_RGB_DITHER_MAX,
			ldiv[ 0 ][ 0 ], 8 * s * 3 );
    gdk_draw_rgb_image( bd->pm_board, gc, 98 * s, 33 * s, 8 * s,
			6 * s, GDK_RGB_DITHER_MAX,
			rdiv[ 0 ][ 0 ], 8 * s * 3 );
}

static void board_draw_border( GdkPixmap *pm, GdkGC *gc, int x0, int y0,
			       int x1, int y1, int board_size,
			       GdkColor *colours, gboolean invert ) {
#define COLOURS( ipix, edge ) ( *( colours + (ipix) * 4 + (edge) ) )

    int i;
    GdkPoint points[ 5 ];

    points[ 0 ].x = points[ 1 ].x = points[ 4 ].x = x0 * board_size;
    points[ 2 ].x = points[ 3 ].x = x1 * board_size - 1;

    points[ 1 ].y = points[ 2 ].y = y0 * board_size;
    points[ 0 ].y = points[ 3 ].y = points[ 4 ].y = y1 * board_size - 1;
    
    for( i = 0; i < board_size; i++ ) {
	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 0 ) :
			       &COLOURS( i, 2 ) );
	gdk_draw_lines( pm, gc, points, 2 );

	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 1 ) :
			       &COLOURS( i, 3 ) );
	gdk_draw_lines( pm, gc, points + 1, 2 );

	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 2 ) :
			       &COLOURS( i, 0 ) );
	gdk_draw_lines( pm, gc, points + 2, 2 );

	gdk_gc_set_foreground( gc, invert ? &COLOURS( board_size - i - 1, 3 ) :
			       &COLOURS( i, 1 ) );
	gdk_draw_lines( pm, gc, points + 3, 2 );

	points[ 0 ].x = points[ 1 ].x = ++points[ 4 ].x;
	points[ 2 ].x = --points[ 3 ].x;

	points[ 1 ].y = ++points[ 2 ].y;
	points[ 0 ].y = points[ 3 ].y = --points[ 4 ].y;
    }
}

static void board_draw_frame_painted( BoardData *bd ) {

    gint i, ix;
    GdkGC *gc;
    GdkGCValues gcv;
    float x, z, cos_theta, diffuse, specular;
    GdkColor colours[ 4 * bd->board_size ];
    
    diffuse = 0.8 * bd->arLight[ 2 ] + 0.2;
    specular = pow( bd->arLight[ 2 ], 20 ) * 0.6;
    gcv.foreground.pixel = gdk_rgb_xpixel_from_rgb(
	( (int) clamp( specular * 0x100 +
		       diffuse * bd->aanBoardColour[ 1 ][ 0 ] ) << 16 ) |
	( (int) clamp( specular * 0x100 +
		       diffuse * bd->aanBoardColour[ 1 ][ 1 ] ) << 8 ) |
	( (int) clamp( specular * 0x100 +
		       diffuse * bd->aanBoardColour[ 1 ][ 2 ] ) ) );
    gc = gdk_gc_new_with_values( bd->widget->window, &gcv, GDK_GC_FOREGROUND );
    
    gdk_draw_rectangle( bd->pm_board, gc, TRUE, 0, 0, bd->board_size * 108,
			bd->board_size * 72 );

    for( ix = 0; ix < bd->board_size; ix++ ) {
	x = 1.0 - ( (float) ix / bd->board_size );
	z = ssqrt( 1.0 - x * x );

	for( i = 0; i < 4; i++ ) {
	    cos_theta = bd->arLight[ 2 ] * z + bd->arLight[ i & 1 ] * x;
	    if( cos_theta < 0 )
		cos_theta = 0;
	    diffuse = 0.8 * cos_theta + 0.2;
	    cos_theta = 2 * z * cos_theta - bd->arLight[ 2 ];
	    specular = pow( cos_theta, 20 ) * 0.6;

	    COLOURS( ix, i ).pixel = gdk_rgb_xpixel_from_rgb(
		( (int) clamp( specular * 0x100 + diffuse *
			       bd->aanBoardColour[ 1 ][ 0 ] ) << 16 ) |
		( (int) clamp( specular * 0x100 + diffuse *
			       bd->aanBoardColour[ 1 ][ 1 ] ) << 8 ) |
		( (int) clamp( specular * 0x100 + diffuse *
			       bd->aanBoardColour[ 1 ][ 2 ] ) ) );

	    if( !( i & 1 ) )
		x = -x;
	}
    }
#undef COLOURS

    board_draw_border( bd->pm_board, gc, 0, 0, 54, 72,
		       bd->board_size, colours, FALSE );
    board_draw_border( bd->pm_board, gc, 54, 0, 108, 72,
		       bd->board_size, colours, FALSE );
    
    board_draw_border( bd->pm_board, gc, 2, 2, 10, 34,
		       bd->board_size, colours, TRUE );
    board_draw_border( bd->pm_board, gc, 2, 38, 10, 70,
		       bd->board_size, colours, TRUE );
    board_draw_border( bd->pm_board, gc, 98, 2, 106, 34,
		       bd->board_size, colours, TRUE );
    board_draw_border( bd->pm_board, gc, 98, 38, 106, 70,
		       bd->board_size, colours, TRUE );

    board_draw_border( bd->pm_board, gc, 11, 2, 49, 70,
		       bd->board_size, colours, TRUE );
    board_draw_border( bd->pm_board, gc, 59, 2, 97, 70,
		       bd->board_size, colours, TRUE );

    gdk_gc_unref( gc );
}

static void hinge_pixel( BoardData *bd, float xNorm, float yNorm,
			 float xEye, float yEye, unsigned char auch[ 3 ] ) {

    float arReflection[ 3 ], arAuxLight[ 2 ][ 3 ] = {
	{ 0.6, 0.7, 0.5 },
	{ 0.5, -0.6, 0.7 } };
    float *arLight[ 3 ] = { bd->arLight, arAuxLight[ 0 ], arAuxLight[ 1 ] };
    float zNorm, zEye;
    float diffuse, specular = 0, cos_theta;
    float l;
    int i;

    zNorm = ssqrt( 1.0 - xNorm * xNorm - yNorm * yNorm );
    
    if( ( cos_theta = xNorm * arLight[ 0 ][ 0 ] +
	  yNorm * arLight[ 0 ][ 1 ] +
	  zNorm * arLight[ 0 ][ 2 ] ) < 0 )
	diffuse = 0.2;
    else {
	diffuse = cos_theta * 0.8 + 0.2;

	for( i = 0; i < 3; i++ ) {
	    if( ( cos_theta = xNorm * arLight[ i ][ 0 ] +
		  yNorm * arLight[ i ][ 1 ] +
		  zNorm * arLight[ i ][ 2 ] ) < 0 )
		cos_theta = 0;
	    
	    arReflection[ 0 ] = arLight[ i ][ 0 ] - 2 * xNorm * cos_theta;
	    arReflection[ 1 ] = arLight[ i ][ 1 ] - 2 * yNorm * cos_theta;
	    arReflection[ 2 ] = arLight[ i ][ 2 ] - 2 * zNorm * cos_theta;
	    
	    l = sqrt( arReflection[ 0 ] * arReflection[ 0 ] +
		      arReflection[ 1 ] * arReflection[ 1 ] +
		      arReflection[ 2 ] * arReflection[ 2 ] );
	    
	    arReflection[ 0 ] /= l;
	    arReflection[ 1 ] /= l;
	    arReflection[ 2 ] /= l;
	    
	    zEye = ssqrt( 1.0 - xEye * xEye - yEye * yEye );
	    cos_theta = arReflection[ 0 ] * xEye + arReflection[ 1 ] * yEye +
		arReflection[ 2 ] * zEye;
	    
	    specular += pow( cos_theta, 30 ) * 0.7;
	}
    }
    
    auch[ 0 ] = clamp( 200 * diffuse + specular * 0x100 );
    auch[ 1 ] = clamp( 230 * diffuse + specular * 0x100 );
    auch[ 2 ] = clamp( 20 * diffuse + specular * 0x100 );
}

static void board_draw_hinges( BoardData *bd, GdkGC *gc ) {
    
    int x, y, s = bd->board_size;
    unsigned char top[ s * 12 ][ s * 2 ][ 3 ], bottom[ s * 12 ][ s * 2 ][ 3 ];
    float xNorm, yNorm;
    
    for( y = 0; y < 12 * s; y++ )
	for( x = 0; x < 2 * s; x++ ) {
	    if( s < 5 && y && !( y % ( 2 * s ) ) )
		yNorm = 0.5;
	    else if( y % ( 2 * s ) < s / 5 )
		yNorm = ( s / 5 - y % ( 2 * s ) ) * ( 2.5 / s );
	    else if( y % ( 2 * s ) >= ( 2 * s - s / 5 ) )
		yNorm = ( y % ( 2 * s ) - ( 2 * s - s / 5 - 1 ) ) *
		    ( -2.5 / s );
	    else
		yNorm = 0;

	    xNorm = ( x - s ) / (float) s * ( 1.0 - yNorm * yNorm );
		    
	    hinge_pixel( bd, xNorm, yNorm,
			 ( s - x ) / ( 40 * s ), ( y - 20 * s ) / ( 40 * s ),
			 top[ y ][ x ] );
	    
	    hinge_pixel( bd, xNorm, yNorm,
			 ( s - x ) / ( 40 * s ), ( y + 20 * s ) / ( 40 * s ),
			 bottom[ y ][ x ] );
	}
    
    gdk_draw_rgb_image( bd->pm_board, gc, 53 * s, 12 * s, 2 * s,
			12 * s, GDK_RGB_DITHER_MAX,
			top[ 0 ][ 0 ], 2 * s * 3 );
    
    gdk_draw_rgb_image( bd->pm_board, gc, 53 * s, 48 * s, 2 * s,
			12 * s, GDK_RGB_DITHER_MAX,
			bottom[ 0 ][ 0 ], 2 * s * 3 );
}

static void board_draw( GtkWidget *widget, BoardData *bd ) {

    gint ix, iy, antialias;
    GdkGC *gc = gdk_gc_new( bd->widget->window );
    
    bd->pm_board = gdk_pixmap_new( widget->window, 108 * bd->board_size,
				   72 * bd->board_size, -1 );

    bd->rgb_points = malloc( 66 * bd->board_size * 12 * bd->board_size * 3 );
    bd->rgb_empty = malloc( 30 * bd->board_size * 6 * bd->board_size * 3 );

#define buf( y, x, i ) ( bd->rgb_points[ ( ( (y) * 12 * bd->board_size + (x) )\
					   * 3 ) + (i) ] )
#define empty( y, x, i ) ( bd->rgb_empty[ ( ( (y) * 6 * bd->board_size + (x) )\
					    * 3 ) + (i) ] )

    if( bd->wood == WOOD_PAINT )
	board_draw_frame_painted( bd );
    else
	board_draw_frame_wood( bd, gc );

    if( bd->hinges )
        board_draw_hinges( bd, gc );
    
    if( bd->labels )
#if USE_GTK2
	board_draw_labels( widget, bd );
#else
	board_draw_labels( bd );
#endif
    
    if( bd->translucent ) {
	bd->rgb_bar0 = malloc( 6 * bd->board_size * 20 * bd->board_size * 3 );
	bd->rgb_bar1 = malloc( 6 * bd->board_size * 20 * bd->board_size * 3 );

	gdk_get_rgb_image( bd->pm_board,
			   gdk_window_get_colormap( widget->window ),
			   51 * bd->board_size, 11 * bd->board_size,
			   6 * bd->board_size, 20 * bd->board_size,
			   bd->rgb_bar0, 6 * bd->board_size * 3 );
	gdk_get_rgb_image( bd->pm_board,
			   gdk_window_get_colormap( widget->window ),
			   51 * bd->board_size, 41 * bd->board_size,
			   6 * bd->board_size, 20 * bd->board_size,
			   bd->rgb_bar1, 6 * bd->board_size * 3 );
    }
    
    for( iy = 0; iy < 30 * bd->board_size; iy++ )
	for( ix = 0; ix < 6 * bd->board_size; ix++ ) {
	    /* <= 0 is board; >= 20 is on a point; interpolate in between */
	    antialias = 2 * ( 30 * bd->board_size - iy ) + 1 - 20 *
		abs( 3 * bd->board_size - ix );

	    if( antialias < 0 )
		antialias = 0;
	    else if( antialias > 20 )
		antialias = 20;

	    buf( iy, ix + 6 * bd->board_size, 0 ) =
		buf( 66 * bd->board_size - iy - 1, ix, 0 ) =
		board_pixel( bd, 2, antialias, 0 );
	    
	    buf( iy, ix + 6 * bd->board_size, 1 ) =
		buf( 66 * bd->board_size - iy - 1, ix, 1 ) =
		board_pixel( bd, 2, antialias, 1 );
	    
	    buf( iy, ix + 6 * bd->board_size, 2 ) =
		buf( 66 * bd->board_size - iy - 1, ix, 2 ) =
		board_pixel( bd, 2, antialias, 2 );
	    
	    buf( iy, ix, 0 ) =
		buf( 66 * bd->board_size - iy - 1, ix + 6 * bd->board_size,
		     0 ) = board_pixel( bd, 3, antialias, 0 );

	    buf( iy, ix, 1 ) =
		buf( 66 * bd->board_size - iy - 1, ix + 6 * bd->board_size,
		     1 ) = board_pixel( bd, 3, antialias, 1 );

	    buf( iy, ix, 2 ) =
		buf( 66 * bd->board_size - iy - 1, ix + 6 * bd->board_size,
		     2 ) = board_pixel( bd, 3, antialias, 2 );
	}
    
    for( iy = 0; iy < 6 * bd->board_size; iy++ )
	for( ix = 0; ix < 12 * bd->board_size; ix++ ) {
	    buf( 30 * bd->board_size + iy, ix, 0 ) =
		board_pixel( bd, 0, 0, 0 );
	    buf( 30 * bd->board_size + iy, ix, 1 ) =
		board_pixel( bd, 0, 0, 1 );
	    buf( 30 * bd->board_size + iy, ix, 2 ) =
		board_pixel( bd, 0, 0, 2 );
	}

#if WIN32
    /* gdk_draw_pixmap doesn't seem to work when copying within a pixmap
       on the Win32 port of GDK.  We have to do things the hard way... */
    {
	int iPoints, aPoints[] = { 12, 24, 36, 60, 72, 84 };

	for( iPoints = 0; iPoints < sizeof( aPoints ) / sizeof( aPoints[ 0 ] );
	     iPoints++ )
	    gdk_draw_rgb_image( bd->pm_board, gc, aPoints[ iPoints ] *
				bd->board_size, 3 * bd->board_size,
				12 * bd->board_size, 66 * bd->board_size,
				GDK_RGB_DITHER_MAX, bd->rgb_points,
				12 * bd->board_size * 3 );
    }
#else
    gdk_draw_rgb_image( bd->pm_board, gc, 12 * bd->board_size,
			3 * bd->board_size, 12 * bd->board_size,
			66 * bd->board_size, GDK_RGB_DITHER_MAX,
			bd->rgb_points, 12 * bd->board_size * 3 );

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
#endif
    
    for( iy = 0; iy < 30 * bd->board_size; iy++ )
	for( ix = 0; ix < 6 * bd->board_size; ix++ ) {
	    empty( iy, ix, 0 ) = board_pixel( bd, 0, 0, 0 );
	    empty( iy, ix, 1 ) = board_pixel( bd, 0, 0, 1 );
	    empty( iy, ix, 2 ) = board_pixel( bd, 0, 0, 2 );
	}

    gdk_draw_rgb_image( bd->pm_board, gc, 3 * bd->board_size,
			3 * bd->board_size, 6 * bd->board_size,
			30 * bd->board_size, GDK_RGB_DITHER_MAX,
			bd->rgb_empty, 6 * bd->board_size * 3 );

#if WIN32
    /* Same problem as above... */
    gdk_draw_rgb_image( bd->pm_board, gc, 99 * bd->board_size,
			3 * bd->board_size, 6 * bd->board_size,
			30 * bd->board_size, GDK_RGB_DITHER_MAX,
			bd->rgb_empty, 6 * bd->board_size * 3 );
    gdk_draw_rgb_image( bd->pm_board, gc, 3 * bd->board_size,
			39 * bd->board_size, 6 * bd->board_size,
			30 * bd->board_size, GDK_RGB_DITHER_MAX,
			bd->rgb_empty, 6 * bd->board_size * 3 );
    gdk_draw_rgb_image( bd->pm_board, gc, 99 * bd->board_size,
			39 * bd->board_size, 6 * bd->board_size,
			30 * bd->board_size, GDK_RGB_DITHER_MAX,
			bd->rgb_empty, 6 * bd->board_size * 3 );
#else
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
#endif
    
    if( !bd->translucent ) {
	free( bd->rgb_points );
	free( bd->rgb_empty );
    }
#undef buf
#undef empty
    
    bd->clockwise = fClockwise;

    gdk_gc_unref( gc );
}

static void board_draw_chequers( GtkWidget *widget, BoardData *bd, int fKey ) {

    int size = fKey ? 16 : 6 * bd->board_size;
    guchar *buf_x, *buf_o, *buf_mask;
    int ix, iy, in, fx, fy, i;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	r, x1, y1, len;
    GdkImage *img = NULL;
    GdkGC *gc;
    short *refract_x = 0, *refract_o = 0;
    
#define BUFX( y, x, i ) buf_x[ ( (y) * size + (x) ) * 4 + (i) ]
#define BUFO( y, x, i ) buf_o[ ( (y) * size + (x) ) * 4 + (i) ]

    if( fKey ) {
	buf_x = bd->rgba_x_key = malloc( size * size * 4 );
	buf_o = bd->rgba_o_key = malloc( size * size * 4 );
    } else {
	buf_x = bd->rgba_x = malloc( size * size * 4 );
	buf_o = bd->rgba_o = malloc( size * size * 4 );
    }
    
    if( bd->translucent ) {
	if( !fKey ) {
	    refract_x = bd->ai_refract[ 0 ] = malloc( size * size *
						      sizeof( short ) );
	    refract_o = bd->ai_refract[ 1 ] = malloc( size * size *
						      sizeof( short ) );
	}
    } else {
	/* We need to use malloc() for this, since Xlib will free() it. */
	buf_mask = malloc( ( size ) * ( size + 7 ) >> 3 );

	if( fKey ) {
	    bd->pm_x_key = gdk_pixmap_new( widget->window, size, size, -1 );
	    bd->pm_o_key = gdk_pixmap_new( widget->window, size, size, -1 );
	    bd->bm_key_mask = gdk_pixmap_new( NULL, size, size, 1 );
	} else {
	    bd->pm_x = gdk_pixmap_new( widget->window, size, size, -1 );
	    bd->pm_o = gdk_pixmap_new( widget->window, size, size, -1 );
	    bd->bm_mask = gdk_pixmap_new( NULL, size, size, 1 );
	}
    
	img = gdk_image_new_bitmap( gdk_window_get_visual( widget->window ),
				    buf_mask, size,
				    size );
    }
    
    for( iy = 0, y_loop = -1.0; iy < size; iy++ ) {
	for( ix = 0, x_loop = -1.0; ix < size; ix++ ) {
	    in = 0;
	    diffuse = specular_x = specular_o = 0.0;
	    fy = 0;
	    y = y_loop;
	    do {
		fx = 0;
		x = x_loop;
		do {
		    r = sqrt( x * x + y * y );
		    if( r < bd->round )
			x1 = y1 = 0.0;
		    else {
			x1 = x * ( r / ( 1 - bd->round ) -
				   1 / ( 1 - bd->round ) + 1 );
			y1 = y * ( r / ( 1 - bd->round ) -
				   1 / ( 1 - bd->round ) + 1 );
		    }
		    if( ( z = 1.0 - x1 * x1 - y1 * y1 ) > 0.0 ) {
			in++;
			diffuse += 0.3;
			z = sqrt( z ) * 1.5;
			len = sqrt( x1 * x1 + y1 * y1 + z * z );
			if( ( cos_theta = ( bd->arLight[ 0 ] * x1 +
					    bd->arLight[ 1 ] * -y1 +
					    bd->arLight[ 2 ] * z ) / len )
			    > 0 ) {
			    diffuse += cos_theta;
			    if( ( cos_theta = 2 * z * cos_theta / len -
				  bd->arLight[ 2 ] ) > 0 ) {
				specular_x += pow( cos_theta,
						   bd->arExponent[ 0 ] ) *
				    bd->arCoefficient[ 0 ];
				specular_o += pow( cos_theta,
						   bd->arExponent[ 1 ] ) *
				    bd->arCoefficient[ 1 ];
			    }
			}
		    }
		    x += 1.0 / ( size );
		} while( !fx++ );
		y += 1.0 / ( size );
	    } while( !fy++ );

	    if( in < ( bd->translucent || fKey ? 1 : 3 ) ) {
		/* pixel is outside chequer */
		for( i = 0; i < 3; i++ )
		    BUFX( iy, ix, i ) = BUFO( iy, ix, i ) = 0;

		if( bd->translucent && !fKey )
		    *refract_x++ = *refract_o++ = iy * size + ix;
		
		if( bd->translucent )
		    BUFX( iy, ix, 3 ) = BUFO( iy, ix, 3 ) = 0xFF;
		else
		    gdk_image_put_pixel( img, ix, iy, MASK_INVISIBLE );
	    } else {
		/* pixel is inside chequer */
		if( bd->translucent ) {
		    if( !fKey ) {
			float r, s, r1, s1, theta, a, b, p, q;
			int f;
			
			r = sqrt( x_loop * x_loop + y_loop * y_loop );
			if( r < bd->round )
			    r1 = 0.0;
			else
			    r1 = r / ( 1 - bd->round ) -
				1 / ( 1 - bd->round ) + 1;
			s = ssqrt( 1 - r * r );
			s1 = ssqrt( 1 - r1 * r1 );

			theta = atanf( r1 / s1 );

			for( f = 0; f < 2; f++ ) {
			    b = asinf( sinf( theta ) / bd->arRefraction[ f ] );
			    a = theta - b;
			    p = r - s * tanf( a );
			    q = p / r;
			    
			    /* write the comparison this strange way to pick up
			       NaNs as well */
			    if( !( q >= -1.0f && q <= 1.0f ) )
				q = 1.0f;

			    *( f ? refract_o++ : refract_x++ ) =
				lrint( iy * q + size / 2 * ( 1.0 - q ) ) *
				size +
				lrint( ix * q + size / 2 * ( 1.0 - q ) );
			}
		    }
		    
		    BUFX( iy, ix, 0 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 0 ] *
						 bd->aarColour[ 0 ][ 3 ] +
						 specular_x ) * 64.0 );
		    BUFX( iy, ix, 1 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 1 ] *
						 bd->aarColour[ 0 ][ 3 ] +
						 specular_x ) * 64.0 );
		    BUFX( iy, ix, 2 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 2 ] *
						 bd->aarColour[ 0 ][ 3 ] +
						 specular_x ) * 64.0 );
		    
		    BUFO( iy, ix, 0 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 0 ] *
						 bd->aarColour[ 1 ][ 3 ] +
						 specular_o ) * 64.0 );
		    BUFO( iy, ix, 1 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 1 ] *
						 bd->aarColour[ 1 ][ 3 ] +
						 specular_o ) * 64.0 );
		    BUFO( iy, ix, 2 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 2 ] *
						 bd->aarColour[ 1 ][ 3 ] +
						 specular_o ) * 64.0 );
		    
		    BUFX( iy, ix, 3 ) = clamp(
			0xFF * 0.25 * ( ( 4 - in ) +
					( ( 1.0 - bd->aarColour[ 0 ][ 3 ] ) *
					  diffuse ) ) );
		    BUFO( iy, ix, 3 ) = clamp(
			0xFF * 0.25 * ( ( 4 - in ) +
					( ( 1.0 - bd->aarColour[ 1 ][ 3 ] ) *
					  diffuse ) ) );
		} else {
		    /* antialias */
		    BUFX( iy, ix, 0 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 0 ] +
						 specular_x )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    BUFX( iy, ix, 1 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 1 ] +
						 specular_x )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    BUFX( iy, ix, 2 ) = clamp( ( diffuse *
						 bd->aarColour[ 0 ][ 2 ] +
						 specular_x )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    
		    BUFO( iy, ix, 0 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 0 ] +
						 specular_o )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    BUFO( iy, ix, 1 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 1 ] +
						 specular_o )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    BUFO( iy, ix, 2 ) = clamp( ( diffuse *
						 bd->aarColour[ 1 ][ 2 ] +
						 specular_o )
					       * 64.0 + ( 4 - in ) * 32.0 );
		    
		    gdk_image_put_pixel( img, ix, iy, MASK_VISIBLE );
		}
	    }
	    x_loop += 2.0 / ( size );
	}
	y_loop += 2.0 / ( size );
    }

    if( !bd->translucent ) {
	gdk_draw_rgb_32_image( fKey ? bd->pm_x_key : bd->pm_x, bd->gc_copy,
			       0, 0, size, size, GDK_RGB_DITHER_MAX,
			       buf_x, size * 4 );
	gdk_draw_rgb_32_image( fKey ? bd->pm_o_key : bd->pm_o, bd->gc_copy,
			       0, 0, size, size, GDK_RGB_DITHER_MAX,
			       buf_o, size * 4 );

	gc = gdk_gc_new( fKey ? bd->bm_key_mask : bd->bm_mask );
	gdk_draw_image( fKey ? bd->bm_key_mask : bd->bm_mask, gc, img,
			0, 0, 0, 0, size, size );
	gdk_gc_unref( gc );
    
	gdk_image_destroy( img );

	free( buf_x );
	free( buf_o );
    }
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

    gdouble aarDiceColour[ 2 ][ 4 ];
    gfloat arDiceCoefficient[ 2 ];
    gfloat arDiceExponent[ 2 ];

    for ( i = 0; i < 2; ++i ) {

      if ( bd->afDieColor[ i ] ) {
        /* die same color as chequers */
        memcpy ( aarDiceColour[ i ], bd->aarColour[ i ], 
                 4 * sizeof ( gdouble ) );
        arDiceCoefficient[ i ] = bd->arCoefficient[ i ];
        arDiceExponent[ i ] = bd->arExponent[ i ];
      }
      else {
        /* user color */
        memcpy ( aarDiceColour[ i ], bd->aarDiceColour[ i ], 
                 4 * sizeof ( gdouble ) );
        arDiceCoefficient[ i ] = bd->arDiceCoefficient[ i ];
        arDiceExponent[ i ] = bd->arDiceExponent[ i ];
      }
    }

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
			/* flat surface */
			in++;
			diffuse += bd->arLight[ 2 ] * 0.8 + 0.2;
			specular_x += pow( bd->arLight[ 2 ],
					   arDiceExponent[ 0 ] ) *
			    arDiceCoefficient[ 0 ];
			specular_o += pow( bd->arLight[ 2 ],
					   arDiceExponent[ 1 ] ) *
			    arDiceCoefficient[ 1 ];
		    } else {
			if( fabs( x ) < 6.0 / 7.0 ) {
			    /* top/bottom edge */
			    x_norm = 0.0;
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = ssqrt( 1.0 - y_norm * y_norm );
			} else if( fabs( y ) < 6.0 / 7.0 ) {
			    /* left/right edge */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = ssqrt( 1.0 - x_norm * x_norm );
			} else {
			    /* corner */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = bd->arLight[ 0 ] * x_norm +
			      bd->arLight[ 1 ] * y_norm +
			      bd->arLight[ 2 ] * z_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    cos_theta = 2 * z_norm * cos_theta -
				bd->arLight[ 2 ];
			    specular_x += pow( cos_theta,
					       arDiceExponent[ 0 ] ) *
				arDiceCoefficient[ 0 ];
			    specular_o += pow( cos_theta,
					       arDiceExponent[ 1 ] ) *
				arDiceCoefficient[ 1 ];
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
              gdk_image_put_pixel( img, ix, iy, MASK_INVISIBLE );

	    } else {
		gdk_image_put_pixel( img, ix, iy, MASK_VISIBLE );

		buf_x[ iy ][ ix ][ 0 ] = clamp( ( diffuse *
						  aarDiceColour[ 0 ][ 0 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse *
						  aarDiceColour[ 0 ][ 1 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse *
						  aarDiceColour[ 0 ][ 2 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );

		buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse *
						  aarDiceColour[ 1 ][ 0 ] +
						  specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse *
						  aarDiceColour[ 1 ][ 1 ] +
						  specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse *
						  aarDiceColour[ 1 ][ 2 ] +
						  specular_o )
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
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	dice_top[ 2 ][ 3 ];

    gdouble aarDiceColour[ 2 ][ 4 ];
    gfloat arDiceCoefficient[ 2 ];
    gfloat arDiceExponent[ 2 ];
    int i;

    /* setup colors */

    for ( i = 0; i < 2; ++i ) {

      if ( bd->afDieColor[ i ] ) {
        /* die same color as chequers */
        memcpy ( aarDiceColour[ i ], bd->aarColour[ i ], 
                 4 * sizeof ( gdouble ) );
        arDiceCoefficient[ i ] = bd->arCoefficient[ i ];
        arDiceExponent[ i ] = bd->arExponent[ i ];
      }
      else {
        /* user color */
        memcpy ( aarDiceColour[ i ], bd->aarDiceColour[ i ], 
                 4 * sizeof ( gdouble ) );
        arDiceCoefficient[ i ] = bd->arDiceCoefficient[ i ];
        arDiceExponent[ i ] = bd->arDiceExponent[ i ];
      }
    }

    /* draw pips */

    bd->pm_x_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );
    bd->pm_o_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );

    diffuse = bd->arLight[ 2 ] * 0.8 + 0.2;
    specular_x = pow( bd->arLight[ 2 ], arDiceExponent[ 0 ] ) *
	arDiceCoefficient[ 0 ];
    specular_o = pow( bd->arLight[ 2 ], arDiceExponent[ 1 ] ) *
	arDiceCoefficient[ 1 ];
    dice_top[ 0 ][ 0 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 0 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 1 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 1 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 2 ] = 
      ( diffuse * aarDiceColour[ 0 ][ 2 ] + specular_x ) * 64.0;
    dice_top[ 1 ][ 0 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 0 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 1 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 1 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 2 ] = 
      ( diffuse * aarDiceColour[ 1 ][ 2 ] + specular_o ) * 64.0;

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
			if( ( cos_theta = ( -bd->arLight[ 0 ] * x +
					    bd->arLight[ 1 ] * y +
					    bd->arLight[ 2 ] * z ) /
			      sqrt( x * x + y * y + z * z ) ) > 0 ) {
			    diffuse += cos_theta * 0.8;
			    cos_theta = 2 * z / 5 * cos_theta -
				bd->arLight[ 2 ];
			    specular_x += pow( cos_theta,
					       arDiceExponent[ 0 ] ) *
				arDiceCoefficient[ 0 ];
			    specular_o += pow( cos_theta,
					       arDiceExponent[ 1 ] ) *
				arDiceCoefficient[ 1 ];
			}
		    }
		    x += 1.0 / ( bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( bd->board_size );
	    } while( !fy++ );


	    buf_x[ iy ][ ix ][ 0 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 0 ][ 0 ] + specular_x ) 
                     * 64.0 + ( 4 - in ) * dice_top[ 0 ][ 0 ] );
	    buf_x[ iy ][ ix ][ 1 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 0 ][ 1 ] + specular_x ) 
                       * 64.0 + ( 4 - in ) * dice_top[ 0 ][ 1 ] );
	    buf_x[ iy ][ ix ][ 2 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 0 ][ 2 ] + specular_x ) 
                     * 64.0 + ( 4 - in ) * dice_top[ 0 ][ 2 ] );
	    
	    buf_o[ iy ][ ix ][ 0 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 1 ][ 0 ] + specular_o ) 
                     * 64.0 + ( 4 - in ) * dice_top[ 1 ][ 0 ] );
	    buf_o[ iy ][ ix ][ 1 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 1 ][ 1 ] + specular_o ) 
                     * 64.0 + ( 4 - in ) * dice_top[ 1 ][ 1 ] );
	    buf_o[ iy ][ ix ][ 2 ] = 
              clamp( ( diffuse * bd->aarDiceDotColour[ 1 ][ 2 ] + specular_o ) 
                     * 64.0 + ( 4 - in ) * dice_top[ 1 ][ 2 ] );

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
			/* flat surface */
			in++;
			diffuse += bd->arLight[ 2 ] * 0.8 + 0.2;
			specular += pow( bd->arLight[ 2 ], 10 ) * 0.4;
		    } else {
			if( fabs( x ) < 7.0 / 8.0 ) {
			    /* top/bottom edge */
			    x_norm = 0.0;
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    z_norm = ssqrt( 1.0 - y_norm * y_norm );
			} else if( fabs( y ) < 7.0 / 8.0 ) {
			    /* left/right edge */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = 0.0;
			    z_norm = ssqrt( 1.0 - x_norm * x_norm );
			} else {
			    /* corner */
			    x_norm = 7.0 * x + ( x > 0.0 ? -6.0 : 6.0 );
			    y_norm = -7.0 * y - ( y > 0.0 ? -6.0 : 6.0 );
			    if( ( z_norm = 1 - x_norm * x_norm -
				  y_norm * y_norm ) < 0.0 )
				goto missed;
			    z_norm = sqrt( z_norm );
			}
			
			in++;
			diffuse += 0.2;
			if( ( cos_theta = bd->arLight[ 0 ] * x_norm +
			      bd->arLight[ 1 ] * y_norm +
			      bd->arLight[ 2 ] * z_norm ) > 0.0 ) {
			    diffuse += cos_theta * 0.8;
			    cos_theta = 2 * z_norm * cos_theta -
				bd->arLight[ 2 ];
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
		gdk_image_put_pixel( img, ix, iy, MASK_INVISIBLE );
	    } else {
		gdk_image_put_pixel( img, ix, iy, MASK_VISIBLE );

		buf[ iy ][ ix ][ 0 ] = 
                  clamp( ( diffuse * bd->arCubeColour[ 0 ] + specular ) * 64.0
                         + ( 4 - in ) * 32.0 );
		buf[ iy ][ ix ][ 1 ] = 
                  clamp( ( diffuse * bd->arCubeColour[ 1 ] + specular ) * 64.0
                         + ( 4 - in ) * 32.0 );
		buf[ iy ][ ix ][ 2 ] = 
                  clamp( ( diffuse * bd->arCubeColour[ 2 ] + specular ) * 64.0 
                         + ( 4 - in ) * 32.0 );
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

/* Create all of the size/colour-dependent pixmaps. */
extern void board_create_pixmaps( GtkWidget *board, BoardData *bd ) {
    
    board_draw( board, bd );
    board_draw_chequers( board, bd, FALSE );
    board_draw_chequers( board, bd, TRUE );
    board_draw_dice( board, bd );
    board_draw_pips( board, bd );
    board_draw_cube( board, bd );
    board_set_cube_font( board, bd );

    bd->pm_saved = gdk_pixmap_new( board->window,
				   6 * bd->board_size,
				   6 * bd->board_size, -1 );
    if( bd->translucent ) {
	bd->pm_saved = gdk_pixmap_new( board->window,
				       6 * bd->board_size,
				       6 * bd->board_size, -1 );
	bd->rgb_saved = malloc( 6 * bd->board_size * 6 * bd->board_size * 3 );
	bd->rgb_temp = malloc( 6 * bd->board_size * 6 * bd->board_size * 3 );
	bd->rgb_temp_saved = malloc( 6 * bd->board_size * 6 * bd->board_size *
				     3 );
	gtk_object_set_user_data( GTK_OBJECT( bd->key0 ), &bd->rgba_x_key );
	gtk_object_set_user_data( GTK_OBJECT( bd->key1 ), &bd->rgba_o_key );
    } else {
	bd->pm_temp = gdk_pixmap_new( board->window,
				      6 * bd->board_size,
				      6 * bd->board_size, -1 );
	bd->pm_temp_saved = gdk_pixmap_new( board->window,
					    6 * bd->board_size,
					    6 * bd->board_size, -1 );
	bd->pm_point = gdk_pixmap_new( board->window,
				       6 * bd->board_size,
				       35 * bd->board_size, -1 );
	gtk_object_set_user_data( GTK_OBJECT( bd->key0 ), &bd->pm_x_key );
	gtk_object_set_user_data( GTK_OBJECT( bd->key1 ), &bd->pm_o_key );
    }    
}

static void board_size_allocate( GtkWidget *board,
				 GtkAllocation *allocation ) {
    BoardData *bd = BOARD( board )->board_data;
    gint old_size = bd->board_size, new_size;
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

    if ( bd->show_ids ) {

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
    if ( bd->usedicearea ) {

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
    
    if( ( bd->board_size = new_size ) != old_size &&
	GTK_WIDGET_REALIZED( board ) ) {
	board_free_pixmaps( bd );
	board_create_pixmaps( board, bd );
    }
    
    child_allocation.width = 108 * bd->board_size;
    cx = child_allocation.x = allocation->x + ( ( allocation->width -
					     child_allocation.width ) >> 1 );
    child_allocation.height = 72 * bd->board_size;
    child_allocation.y = allocation->y + ( ( allocation->height -
					     72 * bd->board_size ) >> 1 );
    gtk_widget_size_allocate( bd->drawing_area, &child_allocation );

    /* allocation for dice area */

    if ( bd->usedicearea ) {
      child_allocation.width = 15 * bd->board_size;
      child_allocation.x += ( 108 - 15 ) * bd->board_size;
      child_allocation.height = 7 * bd->board_size;
      child_allocation.y += 72 * bd->board_size + 1;
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

    if ( bd->show_ids )
      AddChild( bd->vbox_ids, pr );

    AddChild( bd->table, pr );

    if ( bd->usedicearea )
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

static gboolean dice_press( GtkWidget *dice, GdkEvent *event,
			    BoardData *bd ) {

    UserCommand( "roll" );
    
    return TRUE;
}

static gboolean key_expose( GtkWidget *pw, GdkEventExpose *event,
			    BoardData *bd ) {
    
    if( bd->translucent ) {
	guchar aaauch[ 20 ][ 20 ][ 3 ], *p = *(
	    (char **) gtk_object_get_user_data( GTK_OBJECT( pw ) ) );
	int x, y;

	for( y = 0; y < 20; y++ )
	    for( x = 0; x < 20; x++ ) {
		if( x < 2 || x >= 18 || y < 2 || y >= 18 ) {
		    aaauch[ y ][ x ][ 0 ] =
			bd->rgb_empty[ ( y * bd->board_size * 6 + x ) * 3 ];
		    aaauch[ y ][ x ][ 1 ] =
			bd->rgb_empty[ ( y * bd->board_size * 6 + x ) * 3 +
				     1 ];
		    aaauch[ y ][ x ][ 2 ] =
			bd->rgb_empty[ ( y * bd->board_size * 6 + x ) * 3 +
				     2 ];
		} else {
		    aaauch[ y ][ x ][ 0 ] = clamp(
			( ( bd->rgb_empty[ ( y * bd->board_size * 6 + x ) *
					 3 ] * (int) p[ 3 ] ) >> 8 ) +
			p[ 0 ] );
		    aaauch[ y ][ x ][ 1 ] = clamp(
			( ( bd->rgb_empty[ ( y * bd->board_size * 6 + x ) *
					 3 + 1 ] * (int) p[ 3 ] ) >> 8 ) +
			p[ 1 ] );
		    aaauch[ y ][ x ][ 2 ] = clamp(
			( ( bd->rgb_empty[ ( y * bd->board_size * 6 + x ) *
					 3 + 2 ] * (int) p[ 3 ] ) >> 8 ) +
			p[ 2 ] );
		    
		    p += 4;
		}
	    }		    
	
	gdk_draw_rgb_image( pw->window, bd->gc_copy,
			    0, 0, 20, 20, GDK_RGB_DITHER_MAX,
			    (guchar *) aaauch, 60 );
    } else {
	gdk_draw_pixmap( pw->window, bd->gc_copy, bd->pm_board,
			 3 * bd->board_size, 3 * bd->board_size, 0, 0,
			 20, 20 );
	
	gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_key_mask );
	gdk_gc_set_clip_origin( bd->gc_copy, 2, 2 );

	gdk_draw_pixmap( pw->window, bd->gc_copy,
			 *( (GdkPixmap **) gtk_object_get_user_data(
			     GTK_OBJECT( pw ) ) ), 0, 0, 2, 2,
			 16, 16 );
	
	gdk_gc_set_clip_mask( bd->gc_copy, NULL );
    }
    
    return TRUE;
}

static gboolean key_press( GtkWidget *pw, GdkEvent *event,
			   BoardData *bd ) {

    BoardPreferences( bd->widget );
    
    return TRUE;
}

/* Create a drawing area to display a single chequer (used as a key in the
   player table). */
static GtkWidget *chequer_key_new( int iPlayer, BoardData *bd ) {

    GtkWidget *pw = gtk_drawing_area_new();

    gtk_drawing_area_size( GTK_DRAWING_AREA( pw ), 20, 20 );

    if( bd->translucent )
	gtk_object_set_user_data( GTK_OBJECT( pw ),
				  iPlayer ? &bd->rgba_o_key :
				  &bd->rgba_x_key );
    else
	gtk_object_set_user_data( GTK_OBJECT( pw ), 
				  iPlayer ? &bd->pm_o_key : &bd->pm_x_key );
    
    gtk_widget_set_events( pw, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
			   GDK_STRUCTURE_MASK );
    
    gtk_signal_connect( GTK_OBJECT( pw ), "expose_event",
			GTK_SIGNAL_FUNC( key_expose ), bd );
    gtk_signal_connect( GTK_OBJECT( pw ), "button_press_event",
			GTK_SIGNAL_FUNC( key_press ), bd );
    
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
	
    bd->drag_point = bd->board_size = -1;
    bd->dice_roll[ 0 ] = bd->dice_roll[ 1 ] = 0;
    bd->crawford_game = FALSE;
    bd->playing = FALSE;
    bd->cube_use = TRUE;
    
    bd->all_moves = NULL;
#if WIN32
    /* I've seen some Windows 95 systems give warnings with translucent
       chequer, so let's turn translucent off by default on Win32 systems. */ 
    bd->translucent = FALSE;
#else
    bd->translucent = TRUE;
#endif
    bd->wood = WOOD_ALDER;
    bd->hinges = TRUE;
    bd->labels = TRUE;
    bd->usedicearea = FALSE;
    bd->permit_illegal = FALSE;
    bd->beep_illegal = TRUE;
    bd->higher_die_first = TRUE;
    bd->animate_computer_moves = ANIMATE_SLIDE;
    bd->animate_speed = 4;
    bd->show_ids = TRUE;
    bd->show_pips = TRUE;
    bd->round = 0.5;
    bd->arLight[ 0 ] = -0.55667;
    bd->arLight[ 1 ] = 0.32139;
    bd->arLight[ 2 ] = 0.76604;
    bd->arRefraction[ 0 ] = bd->arRefraction[ 1 ] = 1.5;
    bd->arCoefficient[ 0 ] = 0.2;
    bd->arExponent[ 0 ] = 3.0;
    bd->arCoefficient[ 1 ] = 1.0;
    bd->arExponent[ 1 ] = 30.0;
    bd->aarColour[ 0 ][ 0 ] = 1.0;
    bd->aarColour[ 0 ][ 1 ] = 0.20;
    bd->aarColour[ 0 ][ 2 ] = 0.20;
    bd->aarColour[ 0 ][ 3 ] = 0.9;
    bd->aarColour[ 1 ][ 0 ] = 0.05;
    bd->aarColour[ 1 ][ 1 ] = 0.05;
    bd->aarColour[ 1 ][ 2 ] = 0.10;
    bd->aarColour[ 1 ][ 3 ] = 0.5;
    bd->arDiceCoefficient[ 0 ] = 0.2;
    bd->arDiceExponent[ 0 ] = 3.0;
    bd->arDiceCoefficient[ 1 ] = 1.0;
    bd->arDiceExponent[ 1 ] = 30.0;
    bd->aarDiceColour[ 0 ][ 0 ] = 1.0;
    bd->aarDiceColour[ 0 ][ 1 ] = 0.2;
    bd->aarDiceColour[ 0 ][ 2 ] = 0.2;
    bd->aarDiceColour[ 0 ][ 3 ] = 0.9;
    bd->aarDiceColour[ 1 ][ 0 ] = 0.05;
    bd->aarDiceColour[ 1 ][ 1 ] = 0.05;
    bd->aarDiceColour[ 1 ][ 2 ] = 0.1;
    bd->aarDiceColour[ 1 ][ 3 ] = 0.5;
    bd->afDieColor[ 0 ] = TRUE;
    bd->afDieColor[ 1 ] = TRUE;
    bd->aarDiceDotColour[ 0 ][ 0 ] = 0.7;
    bd->aarDiceDotColour[ 0 ][ 1 ] = 0.7;
    bd->aarDiceDotColour[ 0 ][ 2 ] = 0.7;
    bd->aarDiceDotColour[ 0 ][ 3 ] = 0.0; /* unused */
    bd->aarDiceDotColour[ 1 ][ 0 ] = 0.7;
    bd->aarDiceDotColour[ 1 ][ 1 ] = 0.7;
    bd->aarDiceDotColour[ 1 ][ 2 ] = 0.7;
    bd->aarDiceDotColour[ 1 ][ 3 ] = 0.0; /*unused */
    bd->arCubeColour[ 0 ] = 0.90;
    bd->arCubeColour[ 1 ] = 0.90;
    bd->arCubeColour[ 2 ] = 0.90;
    bd->arCubeColour[ 3 ] = 0.0; /*unused */
    bd->aanBoardColour[ 0 ][ 0 ] = 0x30;
    bd->aanBoardColour[ 0 ][ 1 ] = 0x60;
    bd->aanBoardColour[ 0 ][ 2 ] = 0x30;
    bd->aanBoardColour[ 1 ][ 0 ] = 0x00;
    bd->aanBoardColour[ 1 ][ 1 ] = 0x40;
    bd->aanBoardColour[ 1 ][ 2 ] = 0x00;
    bd->aanBoardColour[ 2 ][ 0 ] = 0xFF;
    bd->aanBoardColour[ 2 ][ 1 ] = 0x60;
    bd->aanBoardColour[ 2 ][ 2 ] = 0x60;
    bd->aanBoardColour[ 3 ][ 0 ] = 0xC0;
    bd->aanBoardColour[ 3 ][ 1 ] = 0xC0;
    bd->aanBoardColour[ 3 ][ 2 ] = 0xC0;
    bd->aSpeckle[ 0 ] = 25;
    bd->aSpeckle[ 1 ] = 25;
    bd->aSpeckle[ 2 ] = 25;
    bd->aSpeckle[ 3 ] = 25;
    
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
    
    bd->pm_board = NULL;

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
    gtk_widget_set_events( GTK_WIDGET( bd->drawing_area ), GDK_EXPOSURE_MASK |
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

    bd->key0 = chequer_key_new( 0, bd );
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

    bd->key1 = chequer_key_new( 1, bd );
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
    gtk_widget_set_events( GTK_WIDGET( bd->dice_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK | GDK_STRUCTURE_MASK );

    /* setup initial board */

    board_set( board, "board:::1:0:0:"
              "0:-2:0:0:0:0:5:0:3:0:0:0:-5:5:0:0:0:-3:0:-5:0:0:0:0:2:0:"
              "0:0:0:0:0:1:1:1:0:1:-1:0:25:0:0:0:0:0:0:0:0" );
    
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

static gboolean dice_widget_expose( GtkWidget *dice, GdkEventExpose *event,
				    BoardData *bd ) {

    int n = (int) gtk_object_get_user_data( GTK_OBJECT( dice ) );
    
    board_redraw_die( dice, bd, 0, 0, bd->turn, n % 6 + 1 );
    board_redraw_die( dice, bd, 7 * bd->board_size, 0, bd->turn, n / 6 + 1 );
    
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
				   14 * bd->board_size, 7 * bd->board_size );
	    gtk_widget_set_events( pwDice, GDK_EXPOSURE_MASK |
				   GDK_BUTTON_PRESS_MASK |
				   GDK_STRUCTURE_MASK );
	    gtk_signal_connect( GTK_OBJECT( pwDice ), "expose_event",
				GTK_SIGNAL_FUNC( dice_widget_expose ), bd );
	    gtk_signal_connect( GTK_OBJECT( pwDice ), "button_press_event",
				GTK_SIGNAL_FUNC( dice_widget_press ), bd );
	    gtk_table_attach_defaults( GTK_TABLE( pw ), pwDice,
				       x, x + 1, y, y + 1 );
        }

    gtk_table_set_row_spacings( GTK_TABLE( pw ), 4 * bd->board_size );
    gtk_table_set_col_spacings( GTK_TABLE( pw ), 2 * bd->board_size );
    gtk_container_set_border_width( GTK_CONTAINER( pw ), bd->board_size );
    
    return pw;	    

}
