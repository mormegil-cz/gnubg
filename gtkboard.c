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
#if WIN32
#include <windows.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
#include "gdkgetrgb.h"
#include "gtkboard.h"
#include "gtk-multiview.h"
#include "gtkprefs.h"
#include "positionid.h"

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_style_get_font(s) ((s)->font)
#endif

#define POINT_DICE 28
#define POINT_CUBE 29
#define POINT_RIGHT 30
#define POINT_LEFT 31

#define CLICK_TIME 200 /* minimum time in milliseconds before a drag to the
			  same point is considered a real drag rather than a
			  click */

#if WIN32
static int fWine; /* TRUE if we're running under Wine */
/* The Win32 port of GDK wants clip masks to be inverted, for some reason... */
#define MASK_INVISIBLE 0
#define MASK_VISIBLE 1
#else
#define MASK_INVISIBLE 1
#define MASK_VISIBLE 0
#endif


static int positions[ 2 ][ 28 ][ 3 ] = { {
    { 51, 25, 7 },
    { 90, 63, 6 }, { 84, 63, 6 }, { 78, 63, 6 }, { 72, 63, 6 }, { 66, 63, 6 },
    { 60, 63, 6 }, { 42, 63, 6 }, { 36, 63, 6 }, { 30, 63, 6 }, { 24, 63, 6 },
    { 18, 63, 6 }, { 12, 63, 6 },
    { 12, 3, -6 }, { 18, 3, -6 }, { 24, 3, -6 }, { 30, 3, -6 }, { 36, 3, -6 },
    { 42, 3, -6 }, { 60, 3, -6 }, { 66, 3, -6 }, { 72, 3, -6 }, { 78, 3, -6 },
    { 84, 3, -6 }, { 90, 3, -6 },
    { 51, 41, -7 }, { 99, 63, 6 }, { 99, 3, -6 }
}, {
    { 51, 25, 7 },
    { 12, 63, 6 }, { 18, 63, 6 }, { 24, 63, 6 }, { 30, 63, 6 }, { 36, 63, 6 },
    { 42, 63, 6 }, { 60, 63, 6 }, { 66, 63, 6 }, { 72, 63, 6 }, { 78, 63, 6 },
    { 84, 63, 6 }, { 90, 63, 6 },
    { 90, 3, -6 }, { 84, 3, -6 }, { 78, 3, -6 }, { 72, 3, -6 }, { 66, 3, -6 },
    { 60, 3, -6 }, { 42, 3, -6 }, { 36, 3, -6 }, { 30, 3, -6 }, { 24, 3, -6 },
    { 18, 3, -6 }, { 12, 3, -6 },
    { 51, 41, -7 }, { 3, 63, 6 }, { 3, 3, -6 }
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
    
    x = positions[ fClockwise ][ n ][ 0 ] * bd->board_size;
    y = positions[ fClockwise ][ n ][ 1 ] * bd->board_size;
    cx = 6 * bd->board_size;
    cy = -c_chequer * bd->board_size * positions[ fClockwise ][ n ][ 2 ];

    if( ( invert = cy < 0 ) ) {
	y += cy * 4 / 5;
	cy = -cy;
    }

    /* copy empty point image */
    if( !n || n == 25 )
	/* on bar */
	memcpy( rgb_under[ 0 ][ 0 ], bd->rgb_bar, sizeof( rgb_under ) );
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
    
    x = positions[ fClockwise ][ n ][ 0 ] * bd->board_size;
    y = positions[ fClockwise ][ n ][ 1 ] * bd->board_size;
    cx = 6 * bd->board_size;
    cy = -c_chequer * bd->board_size * positions[ fClockwise ][ n ][ 2 ];

    if( ( invert = cy < 0 ) ) {
	y += cy * 4 / 5;
	cy = -cy;
    }

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
    int x, y, two_chars, lbearing[ 2 ], width[ 2 ], ascent[ 2 ], descent[ 2 ],
	orient, n;
    char cube_text[ 3 ];
    
    if( bd->board_size <= 0 || bd->crawford_game || !bd->cube_use )
	return;

    n = bd->doubled ? bd->cube << 1 : bd->cube;
    
    cube_position( bd, &x, &y, &orient );
    x *= bd->board_size;
    y *= bd->board_size;

    gdk_gc_set_clip_mask( bd->gc_copy, bd->bm_cube_mask );
    gdk_gc_set_clip_origin( bd->gc_copy, x, y );

    gdk_draw_pixmap( board->window, bd->gc_copy, bd->pm_cube, 0, 0, x, y,
		     8 * bd->board_size, 8 * bd->board_size );

    gdk_gc_set_clip_mask( bd->gc_copy, NULL );
    
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
	int y, cy;

	y = positions[ fClockwise ][ i ][ 1 ] * bd->board_size;
	cy = -5 * bd->board_size * positions[ fClockwise ][ i ][ 2 ];

	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}

	if( intersects( event->area.x, event->area.y, event->area.width,
			event->area.height, positions[ fClockwise ][ i ][ 0 ] *
			bd->board_size, y, 6 * bd->board_size, cy ) ) {
	    board_redraw_point( board, bd, i );
	    /* Redrawing the point might have drawn outside the exposed
	       area; enlarge the invalid area in case the dice now need
	       redrawing as well. */
	    if( event->area.x > positions[ fClockwise ][ i ][ 0 ] *
		bd->board_size ) {
		event->area.width += event->area.x -
		    positions[ fClockwise ][ i ][ 0 ] * bd->board_size;
		event->area.x = positions[ fClockwise ][ i ][ 0 ] *
		    bd->board_size;
	    }
	    
	    if( event->area.x + event->area.width <
		( positions[ fClockwise ][ i ][ 0 ] + 6 ) * bd->board_size )
		event->area.width = ( positions[ fClockwise ][ i ][ 0 ] + 6 ) *
		    bd->board_size - event->area.x;
	    
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
    
    GdkEventExpose event;
    gint cy;
    
    if( bd->board_size <= 0 )
	return;

    event.count = 0;
    event.area.x = positions[ fClockwise ][ n ][ 0 ] * bd->board_size;
    event.area.y = positions[ fClockwise ][ n ][ 1 ] * bd->board_size;
    event.area.width = 6 * bd->board_size;
    cy = -5 * bd->board_size * positions[ fClockwise ][ n ][ 2 ];
    if( cy < 0 ) {
        event.area.y += cy * 4 / 5;
        event.area.height = -cy;
    } else
	event.area.height = cy;
    
    board_expose( board, &event, bd );
}

static int board_point( GtkWidget *board, BoardData *bd, int x0, int y0 ) {

    int i, y, cy, xCube, yCube;

    x0 /= bd->board_size;
    y0 /= bd->board_size;

    if( intersects( x0, y0, 0, 0, bd->x_dice[ 0 ], bd->y_dice[ 0 ], 7, 7 ) ||
	intersects( x0, y0, 0, 0, bd->x_dice[ 1 ], bd->y_dice[ 1 ], 7, 7 ) )
	return POINT_DICE;
    
    /* jsc: support for faster rolling of dice by clicking board
      *
      * These arguments should be dynamically calculated instead 
      * of hardcoded, but it's too painful right now.
      */
    if( intersects( x0, y0, 0, 0, 60, 33, 36, 6 ) )
	return POINT_RIGHT;
    else if( intersects( x0, y0, 0, 0, 12, 33, 36, 6 ) )
	return POINT_LEFT;

    cube_position( bd, &xCube, &yCube, NULL );
    
    if( intersects( x0, y0, 0, 0, xCube, yCube, 8, 8 ) )
	return POINT_CUBE;
    
    for( i = 0; i < 28; i++ ) {
	y = positions[ fClockwise ][ i ][ 1 ];
	cy = -5 * positions[ fClockwise ][ i ][ 2 ];
	if( cy < 0 ) {
	    y += cy * 4 / 5;
	    cy = -cy;
	}

	if( intersects( x0, y0, 0, 0, positions[ fClockwise ][ i ][ 0 ],
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

    gtk_entry_set_text( GTK_ENTRY( bd->position_id ), PositionID( points ) );
}

/* A chequer has been moved or the board has been updated -- update the
   move and position ID labels. */
static int update_move( BoardData *bd ) {
    
    char *move = "Illegal move", move_buf[ 40 ];
    gint i, points[ 2 ][ 25 ];
    guchar key[ 10 ];
    int fIncomplete = TRUE, fIllegal = TRUE;
    
    read_board( bd, points );
    update_position_id( bd, points );

    bd->valid_move = NULL;
    
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) &&
	bd->playing ) {
	move = "(Editing)";
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
	
	gdk_get_rgb_image( board->window,
			   gdk_window_get_colormap( board->window ),
			   x - 3 * bd->board_size, y - 3 * bd->board_size,
			   6 * bd->board_size, 6 * bd->board_size,
			   bd->rgb_temp_saved, 6 * bd->board_size * 3 );
	
	copy_rgb( bd->rgb_saved, bd->rgb_temp_saved, 0, 0,
		  bd->x_drag - x, bd->y_drag - y,
		  6 * bd->board_size, 6 * bd->board_size,
		  6 * bd->board_size, 6 * bd->board_size,
		  6 * bd->board_size * 3, 6 * bd->board_size * 3 );
	
	memcpy( bd->rgb_temp, bd->rgb_temp_saved, 6 * bd->board_size *
		6 * bd->board_size * 3 );
	
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
	
	gdk_draw_pixmap( bd->pm_saved, bd->gc_copy, board->window,
			 x - 3 * bd->board_size, y - 3 * bd->board_size,
			 0, 0, 6 * bd->board_size, 6 * bd->board_size );
	
	gdk_draw_rgb_image( board->window, bd->gc_copy,
			    x - 3 * bd->board_size,
			    y - 3 * bd->board_size,
			    6 * bd->board_size, 6 * bd->board_size,
			    GDK_RGB_DITHER_MAX, bd->rgb_temp,
			    6 * bd->board_size * 3 );
	
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
	
	if( ( bd->drag_point = board_point( board, bd, x, y ) ) < 0 ) {
	    /* Click on illegal area. */
	    board_beep( bd );
	    
	    return TRUE;
	}

	bd->click_time = gdk_event_get_time( event );
	bd->drag_button = event->button.button;
	
	if( bd->drag_point == POINT_CUBE ) {
	    /* Clicked on cube; double. */
	    bd->drag_point = -1;
	    
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
		board_beep( bd );
	    else
		UserCommand( "double" );
	    
	    return TRUE;
	}
	
	if( bd->drag_point == POINT_DICE ) {
	    /* Clicked on dice; end move. */
	    bd->drag_point = -1;
	    
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bd->edit ) ) )
		board_beep( bd );
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
		( bd->drag_point == POINT_LEFT  && bd->turn == -1 ) )
		UserCommand( "roll" );
	    else
		board_beep( bd );
	    
	    bd->drag_point = -1;
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
		
		if( !place_chequer_or_revert( board, bd, dest ) ) {
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
		    
		    if( !place_chequer_or_revert( board, bd, dest ) ) {
			board_expose_point( board, bd, bd->drag_point );
			board_beep( bd );
		    }
		}
	    }
	} else {
	    /* This is from a normal drag release */
	    if( !place_chequer_or_revert( board, bd, dest ) )
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
}

static gint board_set( Board *board, const gchar *board_text ) {

    BoardData *bd = board->board_data;
    gchar *dest, buf[ 32 ];
    gint i, *pn, **ppn;
    gint old_board[ 28 ];
    int old_cube, old_doubled, old_crawford, old_xCube, old_yCube;
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

    gtk_entry_set_text( GTK_ENTRY( bd->name0 ), bd->name_opponent );
    gtk_entry_set_text( GTK_ENTRY( bd->name1 ), bd->name );
    gtk_label_set_text( GTK_LABEL( bd->lname0 ), bd->name_opponent );
    gtk_label_set_text( GTK_LABEL( bd->lname1 ), bd->name );
    
    if( bd->match_to ) {
	sprintf( buf, "%d", bd->match_to );
	gtk_label_set_text( GTK_LABEL( bd->match ), buf );
    } else
	gtk_label_set_text( GTK_LABEL( bd->match ), "unlimited" );

    padj0 = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( bd->score0 ) );
    padj1 = gtk_spin_button_get_adjustment( GTK_SPIN_BUTTON( bd->score1 ) );
    padj0->upper = padj1->upper = bd->match_to ? bd->match_to : 65535;
    gtk_adjustment_changed( padj0 );
    gtk_adjustment_changed( padj1 );
    
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score0 ),
			       bd->score_opponent );
    gtk_spin_button_set_value( GTK_SPIN_BUTTON( bd->score1 ),
			       bd->score );
    sprintf( buf, "%d", bd->score_opponent );
    gtk_label_set_text( GTK_LABEL( bd->lscore0 ), buf );
    sprintf( buf, "%d", bd->score );
    gtk_label_set_text( GTK_LABEL( bd->lscore1 ), buf );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->crawford ),
				  bd->crawford_game );
    gtk_widget_set_sensitive( bd->crawford, bd->match_to > 1 &&
			      ( bd->score == bd->match_to - 1 ||
				bd->score_opponent == bd->match_to - 1 ) );
				  
    read_board( bd, bd->old_board );
    update_position_id( bd, bd->old_board );
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

	gdk_font_unref( bd->cube_font );
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
    
    bd->clockwise = fClockwise;
    
#if WIN32
    if( fWine )
	/* For some reason, Wine manages to completely mess up updates
	   of the board window.  We assume this is a bug in GTK+ and/or
	   Wine, and work around it by redrawing the entire window. */
	gtk_widget_queue_draw( gtk_widget_get_toplevel( bd->drawing_area ) );
#endif
    
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
    int src, dest, src_cheq, dest_cheq, colour;
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
    int n, f;
	
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
	if( ( f = !GTK_WIDGET_HAS_GRAB( pbd->stop ) ) )
	    gtk_grab_add( pbd->stop );
	
	gtk_main_iteration();

	if( f )
	    gtk_grab_remove( pbd->stop );
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
}

static void update_buttons( BoardData *pbd ) {

    enum _control { C_NONE, C_ROLLDOUBLE, C_TAKEDROP, C_AGREEDECLINE } c;

    c = C_NONE;

    if( !pbd->dice[ 0 ] )
	c = C_ROLLDOUBLE;

    if( ms.fDoubled )
	c = C_TAKEDROP;

    if( ms.fResigned )
	c = C_AGREEDECLINE;

    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pbd->edit ) ) ||
	!pbd->playing )
	c = C_NONE;
    
    if( c == C_ROLLDOUBLE )
	gtk_widget_show_all( pbd->usedicearea ? pbd->dice_area :
			     pbd->rolldouble );
    else
	gtk_widget_hide( pbd->usedicearea ? pbd->dice_area :
			     pbd->rolldouble );

    if( c == C_TAKEDROP )
	gtk_widget_show_all( pbd->takedrop );
    else
	gtk_widget_hide( pbd->takedrop );

    if( c == C_AGREEDECLINE )
	gtk_widget_show_all( pbd->agreedecline );
    else
	gtk_widget_hide( pbd->agreedecline );
}

extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1 ) {
    gchar board_str[ 256 ];
    BoardData *pbd = board->board_data;
    
    memcpy( pbd->old_board, points, sizeof( pbd->old_board ) );

    FIBSBoard( board_str, points, roll, name, opp_name, match, score,
	       opp_score, die0, die1, ms.nCube, ms.fCubeOwner, ms.fDoubled,
	       ms.fTurn, ms.fCrawford );
    
    board_set( board, board_str );
    
    /* FIXME update names, score, match length */
    if( pbd->board_size <= 0 )
	return 0;

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
	free( bd->rgb_bar );
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

    gdk_font_unref( bd->cube_font );
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

static guchar board_pixel( BoardData *bd, int i, int antialias, int j ) {

    return clamp( ( ( (int) bd->aanBoardColour[ 0 ][ j ] -
		      (int) bd->aSpeckle[ 0 ] / 2 +
		      (int) RAND % ( bd->aSpeckle[ 0 ] + 1 ) ) *
		    ( 20 - antialias ) +
		    ( (int) bd->aanBoardColour[ i ][ j ] -
		      (int) bd->aSpeckle[ i ] / 2 +
		      (int) RAND % ( bd->aSpeckle[ i ] + 1 ) ) *
		    antialias ) / 20 );
}

static void board_draw( GtkWidget *widget, BoardData *bd ) {

    gint ix, iy, antialias;
    GdkGC *gc;
    GdkGCValues gcv;
    float x, z, cos_theta, diffuse, specular;
    GdkColor colours[ 2 * bd->board_size ];
    
    bd->pm_board = gdk_pixmap_new( widget->window, 108 * bd->board_size,
				   72 * bd->board_size, -1 );

    bd->rgb_points = malloc( 66 * bd->board_size * 12 * bd->board_size * 3 );
    bd->rgb_empty = malloc( 30 * bd->board_size * 6 * bd->board_size * 3 );
    bd->rgb_bar = malloc( 6 * bd->board_size * 21 * bd->board_size * 3 );
#define buf( y, x, i ) ( bd->rgb_points[ ( ( (y) * 12 * bd->board_size + (x) )\
					   * 3 ) + (i) ] )
#define empty( y, x, i ) ( bd->rgb_empty[ ( ( (y) * 6 * bd->board_size + (x) )\
					    * 3 ) + (i) ] )
    
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

    if( bd->labels )
	board_draw_labels( bd );
    
    /* FIXME shade hinges */
    
    gdk_get_rgb_image( bd->pm_board,
		       gdk_window_get_colormap( widget->window ),
		       51 * bd->board_size, 4 * bd->board_size,
		       6 * bd->board_size, 21 * bd->board_size,
		       bd->rgb_bar, 6 * bd->board_size * 3 );
	
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

    gdk_gc_unref( gc );
}

static void board_draw_chequers( GtkWidget *widget, BoardData *bd, int fKey ) {

    int size = fKey ? 16 : 6 * bd->board_size;
    guchar *buf_x, *buf_o, *buf_mask;
    int ix, iy, in, fx, fy, i;
    float x, y, z, x_loop, y_loop, diffuse, specular_x, specular_o, cos_theta,
	len;
    GdkImage *img;
    GdkGC *gc;
    short *refract_x, *refract_o;
    
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
		    if( ( z = 1.0 - x * x - y * y ) > 0.0 ) {
			in++;
			diffuse += 0.3;
			z = sqrt( z ) * 1.5;
			len = sqrt( x * x + y * y + z * z );
			if( ( cos_theta = ( bd->arLight[ 0 ] * x +
					    bd->arLight[ 1 ] * -y +
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
			float r, s, theta, a, b, p, q;
			int f;
			
			r = sqrt( x_loop * x_loop + y_loop * y_loop );
			s = sqrt( 1 - r * r );

			theta = atanf( r / s );

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
			specular_x += pow( 0.9, bd->arExponent[ 0 ] ) *
			    bd->arCoefficient[ 0 ];
			specular_o += pow( 0.9, bd->arExponent[ 1 ] ) *
			    bd->arCoefficient[ 1 ];
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
			    specular_x += pow( cos_theta,
					       bd->arExponent[ 0 ] ) *
				bd->arCoefficient[ 0 ];
			    specular_o += pow( cos_theta,
					       bd->arExponent[ 1 ] ) *
				bd->arCoefficient[ 1 ];
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
						  bd->aarColour[ 0 ][ 0 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse *
						  bd->aarColour[ 0 ][ 1 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse *
						  bd->aarColour[ 0 ][ 2 ] +
						  specular_x )
						* 64.0 + ( 4 - in ) * 32.0 );

		buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse *
						  bd->aarColour[ 1 ][ 0 ] +
						  specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse *
						  bd->aarColour[ 1 ][ 1 ] +
						  specular_o )
						* 64.0 + ( 4 - in ) * 32.0 );
		buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse *
						  bd->aarColour[ 1 ][ 2 ] +
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

    bd->pm_x_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );
    bd->pm_o_pip = gdk_pixmap_new( widget->window, bd->board_size,
				   bd->board_size, -1 );

    specular_x = pow( 0.9, bd->arExponent[ 0 ] ) * bd->arCoefficient[ 0 ];
    specular_o = pow( 0.9, bd->arExponent[ 1 ] ) * bd->arCoefficient[ 1 ];
    /* 0.92 = 0.9 x 0.8 + 0.2 (ambient) */
    dice_top[ 0 ][ 0 ] = (0.92 * bd->aarColour[ 0 ][ 0 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 1 ] = (0.92 * bd->aarColour[ 0 ][ 1 ] + specular_x ) * 64.0;
    dice_top[ 0 ][ 2 ] = (0.92 * bd->aarColour[ 0 ][ 2 ] + specular_x ) * 64.0;
    dice_top[ 1 ][ 0 ] = (0.92 * bd->aarColour[ 1 ][ 0 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 1 ] = (0.92 * bd->aarColour[ 1 ][ 1 ] + specular_o ) * 64.0;
    dice_top[ 1 ][ 2 ] = (0.92 * bd->aarColour[ 1 ][ 2 ] + specular_o ) * 64.0;
    
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
			    specular_x += pow( cos_theta,
					       bd->arExponent[ 0 ] ) *
				bd->arCoefficient[ 0 ];
			    specular_x += pow( cos_theta,
					       bd->arExponent[ 1 ] ) *
				bd->arCoefficient[ 1 ];
			}
		    }
		    x += 1.0 / ( bd->board_size );
		} while( !fx++ );
		y += 1.0 / ( bd->board_size );
	    } while( !fy++ );

	    buf_x[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 0 ][ 0 ] );
	    buf_x[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 0 ][ 1 ] );
	    buf_x[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.7 + specular_x ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 0 ][ 2 ] );
	    
	    buf_o[ iy ][ ix ][ 0 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 1 ][ 0 ] );
	    buf_o[ iy ][ ix ][ 1 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 1 ][ 1 ] );
	    buf_o[ iy ][ ix ][ 2 ] = clamp( ( diffuse * 0.7 + specular_o ) *
					    64.0 + ( 4 - in ) *
					    dice_top[ 1 ][ 2 ] );
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
		gdk_image_put_pixel( img, ix, iy, MASK_INVISIBLE );
	    } else {
		gdk_image_put_pixel( img, ix, iy, MASK_VISIBLE );

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
    int cy;
    
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
    
    gtk_widget_get_child_requisition( bd->hbox_pos, &requisition );
    allocation->height -= requisition.height;
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->hbox_pos, &child_allocation );

    /* find the tallest out of the move widget and the button boxes */
    gtk_widget_get_child_requisition( bd->move, &requisition );
    cy = requisition.height;
    gtk_widget_get_child_requisition( bd->rolldouble, &requisition );
    if( requisition.height > cy )
	cy = requisition.height;
    gtk_widget_get_child_requisition( bd->takedrop, &requisition );
    if( requisition.height > cy )
	cy = requisition.height;
    gtk_widget_get_child_requisition( bd->agreedecline, &requisition );
    if( requisition.height > cy )
	cy = requisition.height;
    
    /* ensure there is room for the dice area or the move, whichever is
       bigger */
    new_size = MIN( allocation->width / 108,
		    MIN( ( allocation->height - cy - 2 ) / 72,
			 ( allocation->height - 2 ) / 79 ) );

    if( new_size * 7 > cy )
	cy = new_size * 7;
    
    gtk_widget_get_child_requisition( bd->move, &requisition );
    child_allocation.x = allocation->x;
    child_allocation.y = allocation->y + allocation->height - cy + 1;
    child_allocation.width = allocation->width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->move, &child_allocation );

    allocation->height -= cy + 2;
    
    /* FIXME what should we do if new_size < 1?  If the window manager
       honours our minimum size this won't happen, but... */
    
    if( ( bd->board_size = new_size ) != old_size &&
	GTK_WIDGET_REALIZED( board ) ) {
	board_free_pixmaps( bd );
	board_create_pixmaps( board, bd );
    }
    
    child_allocation.width = 108 * bd->board_size;
    child_allocation.x = allocation->x + ( ( allocation->width -
					     child_allocation.width ) >> 1 );
    child_allocation.height = 72 * bd->board_size;
    child_allocation.y = allocation->y + ( ( allocation->height -
					     72 * bd->board_size ) >> 1 );
    gtk_widget_size_allocate( bd->drawing_area, &child_allocation );

    child_allocation.width = 15 * bd->board_size;
    child_allocation.x += ( 108 - 15 ) * bd->board_size;
    child_allocation.height = 7 * bd->board_size;
    child_allocation.y += 72 * bd->board_size + 1;
    gtk_widget_size_allocate( bd->dice_area, &child_allocation );

    gtk_widget_get_child_requisition( bd->rolldouble, &requisition );
    child_allocation.x = MAX( 0, allocation->x + allocation->width / 2 +
	54 * bd->board_size - requisition.width );
    child_allocation.width = requisition.width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->rolldouble, &child_allocation );

    gtk_widget_get_child_requisition( bd->takedrop, &requisition );
    child_allocation.x = MAX( 0, allocation->x + allocation->width / 2 +
	54 * bd->board_size - requisition.width );
    child_allocation.width = requisition.width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->takedrop, &child_allocation );

    gtk_widget_get_child_requisition( bd->agreedecline, &requisition );
    child_allocation.x = MAX( 0, allocation->x + allocation->width / 2 +
	54 * bd->board_size - requisition.width );
    child_allocation.width = requisition.width;
    child_allocation.height = requisition.height;
    gtk_widget_size_allocate( bd->agreedecline, &child_allocation );
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
    GtkRequisition r;
    int cy;
    
    if( !pw || !pr )
	return;

    pr->width = pr->height = 0;

    bd = BOARD( pw )->board_data;

    AddChild( bd->hbox_pos, pr );
    AddChild( bd->table, pr );
    AddChild( bd->hbox_match, pr );

    gtk_widget_size_request( bd->move, &r );
    cy = MAX( r.height, 7 ); /* move or dice_area, whichever is taller */

    gtk_widget_size_request( bd->rolldouble, &r );
    if( r.height > cy )
	cy = r.height;

    gtk_widget_size_request( bd->takedrop, &r );
    if( r.height > cy )
	cy = r.height;

    gtk_widget_size_request( bd->agreedecline, &r );
    if( r.height > cy )
	cy = r.height;

    pr->height += cy;

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

    char sz[ 25 ]; /* "set board XXXXXXXXXXXXXX" */

    sprintf( sz, "set board %s", gtk_entry_get_text( GTK_ENTRY(
	bd->position_id ) ) );
    
    UserCommand( sz );
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

static void board_init( Board *board ) {

    BoardData *bd = g_malloc( sizeof( *bd ) );
    GdkColormap *cmap = gtk_widget_get_colormap( GTK_WIDGET( board ) );
    GdkVisual *vis = gtk_widget_get_visual( GTK_WIDGET( board ) );
    GdkGCValues gcval;
    GtkWidget *pw;
    
    board->board_data = bd;
    bd->widget = GTK_WIDGET( board );
	
    bd->drag_point = bd->board_size = -1;
    bd->dice_roll[ 0 ] = bd->dice_roll[ 1 ] = 0;
    bd->crawford_game = FALSE;
    bd->playing = FALSE;
    bd->cube_use = TRUE;
    
    bd->all_moves = NULL;

    bd->translucent = TRUE;
    bd->labels = FALSE;
    bd->usedicearea = TRUE;
    bd->permit_illegal = FALSE;
    bd->beep_illegal = TRUE;
    bd->higher_die_first = FALSE;
    bd->animate_computer_moves = ANIMATE_SLIDE;
    bd->animate_speed = 4;
    bd->arLight[ 0 ] = -0.5;
    bd->arLight[ 1 ] = 0.5;
    bd->arLight[ 2 ] = 0.707;
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
    bd->aanBoardColour[ 0 ][ 0 ] = 0x20;
    bd->aanBoardColour[ 0 ][ 1 ] = 0x40;
    bd->aanBoardColour[ 0 ][ 2 ] = 0x20;
    bd->aanBoardColour[ 2 ][ 0 ] = 0xC0;
    bd->aanBoardColour[ 2 ][ 1 ] = 0x40;
    bd->aanBoardColour[ 2 ][ 2 ] = 0x40;
    bd->aanBoardColour[ 3 ][ 0 ] = 0x80;
    bd->aanBoardColour[ 3 ][ 1 ] = 0x80;
    bd->aanBoardColour[ 3 ][ 2 ] = 0x80;
    bd->aSpeckle[ 0 ] = 25;
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
    bd->gc_cube = gtk_gc_get( vis->depth, cmap, &gcval, GDK_GC_FOREGROUND );

    bd->x_dice[ 0 ] = bd->x_dice[ 1 ] = -10;    
    
    bd->pm_board = NULL;

    bd->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->drawing_area ), 108,
			   72 );
    gtk_widget_set_events( GTK_WIDGET( bd->drawing_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK | GDK_STRUCTURE_MASK );
    gtk_container_add( GTK_CONTAINER( board ), bd->drawing_area );

    gtk_container_add( GTK_CONTAINER( board ),
		       bd->move = gtk_label_new( NULL ) );
    gtk_widget_set_name( bd->move, "move" );

    bd->takedrop = gtk_hbutton_box_new();
    pw = gtk_button_new_with_label( "Take" );
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "take" );
    gtk_container_add( GTK_CONTAINER( bd->takedrop ), pw );
    pw = gtk_button_new_with_label( "Drop" );
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "drop" );
    gtk_container_add( GTK_CONTAINER( bd->takedrop ), pw );
    bd->redouble = gtk_button_new_with_label( "Redouble" );
    gtk_signal_connect( GTK_OBJECT( bd->redouble ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "redouble" );
    gtk_container_add( GTK_CONTAINER( bd->takedrop ), bd->redouble );
    gtk_container_add( GTK_CONTAINER( board ), bd->takedrop );
    
    bd->rolldouble = gtk_hbutton_box_new();
    pw = gtk_button_new_with_label( "Roll" );
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "roll" );
    gtk_container_add( GTK_CONTAINER( bd->rolldouble ), pw );
    bd->doub = gtk_button_new_with_label( "Double" );
    gtk_signal_connect( GTK_OBJECT( bd->doub ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "double" );
    gtk_container_add( GTK_CONTAINER( bd->rolldouble ), bd->doub );
    gtk_container_add( GTK_CONTAINER( board ), bd->rolldouble );

    bd->agreedecline = gtk_hbutton_box_new();
    pw = gtk_button_new_with_label( "Agree" );
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "agree" );
    gtk_container_add( GTK_CONTAINER( bd->agreedecline ), pw );
    pw = gtk_button_new_with_label( "Decline" );
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked",
			GTK_SIGNAL_FUNC( ButtonClicked ), "decline" );
    gtk_container_add( GTK_CONTAINER( bd->agreedecline ), pw );
    gtk_container_add( GTK_CONTAINER( board ), bd->agreedecline );
    
    bd->dice_area = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( bd->dice_area ), 15, 8 );
    gtk_widget_set_events( GTK_WIDGET( bd->dice_area ), GDK_EXPOSURE_MASK |
			   GDK_BUTTON_PRESS_MASK | GDK_STRUCTURE_MASK );
    gtk_container_add( GTK_CONTAINER( board ), bd->dice_area );

    bd->hbox_pos = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( board ), bd->hbox_pos, FALSE, FALSE, 0 );

    bd->table = gtk_table_new( 3, 4, FALSE );
    gtk_box_pack_start( GTK_BOX( board ), bd->table, FALSE, FALSE, 0 );
    
    bd->hbox_match = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( board ), bd->hbox_match, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( bd->hbox_pos ), bd->stop =
			gtk_button_new_with_label( "Stop" ),
			FALSE, FALSE, 8 );
    gtk_box_pack_end( GTK_BOX( bd->hbox_pos ), bd->edit =
		      gtk_toggle_button_new_with_label( "Edit" ),
		      FALSE, FALSE, 8 );
    gtk_box_pack_end( GTK_BOX( bd->hbox_pos ), bd->reset =
		      gtk_button_new_with_label( "Reset" ),
		      FALSE, FALSE, 0 );
    gtk_box_pack_end( GTK_BOX( bd->hbox_pos ), bd->position_id =
		      gtk_entry_new(), FALSE, FALSE, 8 );
    gtk_entry_set_max_length( GTK_ENTRY( bd->position_id ), 14 );
    gtk_box_pack_end( GTK_BOX( bd->hbox_pos ),
		      gtk_label_new( "Position:" ), FALSE, FALSE, 0 );
    
    gtk_table_attach( GTK_TABLE( bd->table ), pw = gtk_label_new( "Name" ),
		      1, 2, 0, 1, GTK_FILL, 0, 4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_table_attach( GTK_TABLE( bd->table ), gtk_label_new( "Score" ),
		      2, 3, 0, 1, 0, 0, 8, 0 );
    
    gtk_table_attach( GTK_TABLE( bd->table ), bd->mname0 =
		      gtk_multiview_new(), 1, 2, 1, 2, 0, 0, 4, 0 );
    gtk_table_attach( GTK_TABLE( bd->table ), bd->mname1 =
		      gtk_multiview_new(), 1, 2, 2, 3, 0, 0, 4, 0 );
    bd->name0 = gtk_entry_new_with_max_length( 32 );
    bd->lname0 = gtk_label_new( NULL );
    gtk_misc_set_alignment( GTK_MISC( bd->lname0 ), 0, 0.5 );
    bd->name1 = gtk_entry_new_with_max_length( 32 );
    bd->lname1 = gtk_label_new( NULL );
    gtk_misc_set_alignment( GTK_MISC( bd->lname1 ), 0, 0.5 );
    gtk_container_add( GTK_CONTAINER( bd->mname0 ), bd->lname0 );
    gtk_container_add( GTK_CONTAINER( bd->mname0 ), bd->name0 );
    gtk_container_add( GTK_CONTAINER( bd->mname1 ), bd->lname1 );
    gtk_container_add( GTK_CONTAINER( bd->mname1 ), bd->name1 );

    gtk_table_attach( GTK_TABLE( bd->table ), bd->key0 =
		      chequer_key_new( 0, bd ), 0, 1, 1, 2, 0, 0, 8, 1 );
    gtk_table_attach( GTK_TABLE( bd->table ), bd->key1 =
		      chequer_key_new( 1, bd ), 0, 1, 2, 3, 0, 0, 8, 0 );
    
    gtk_table_attach( GTK_TABLE( bd->table ), bd->mscore0 =
		      gtk_multiview_new(), 2, 3, 1, 2, 0, 0, 4, 0 );
    gtk_table_attach( GTK_TABLE( bd->table ), bd->mscore1 =
		      gtk_multiview_new(), 2, 3, 2, 3, 0, 0, 4, 0 );
    bd->score0 = gtk_spin_button_new( GTK_ADJUSTMENT(
	gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) ), 1, 0 );
    bd->score1 = gtk_spin_button_new( GTK_ADJUSTMENT(
	gtk_adjustment_new( 0, 0, 65535, 1, 1, 1 ) ), 1, 0 );
    bd->lscore0 = gtk_label_new( NULL );
    bd->lscore1 = gtk_label_new( NULL );
    gtk_container_add( GTK_CONTAINER( bd->mscore0 ), bd->lscore0 );
    gtk_container_add( GTK_CONTAINER( bd->mscore0 ), bd->score0 );
    gtk_container_add( GTK_CONTAINER( bd->mscore1 ), bd->lscore1 );
    gtk_container_add( GTK_CONTAINER( bd->mscore1 ), bd->score1 );
    
    gtk_table_attach( GTK_TABLE( bd->table ), bd->crawford =
		      gtk_check_button_new_with_label( "Crawford game" ),
		      3, 4, 1, 3, 0, 0, 0, 0 );
    gtk_signal_connect( GTK_OBJECT( bd->crawford ), "toggled",
			GTK_SIGNAL_FUNC( board_set_crawford ), bd );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( bd->score0 ), TRUE );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( bd->score1 ), TRUE );
    
    gtk_box_pack_start( GTK_BOX( bd->hbox_match ),
			gtk_label_new( "Match:" ), FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( bd->hbox_match ), bd->match =
			gtk_label_new( NULL ), FALSE, FALSE, 0 );

    board_set( board, "board:::1:0:0:"
              "0:-2:0:0:0:0:5:0:3:0:0:0:-5:5:0:0:0:-3:0:-5:0:0:0:0:2:0:"
              "0:0:0:0:0:1:1:1:0:1:-1:0:25:0:0:0:0:0:0:0:0" );
    
    gtk_widget_show_all( GTK_WIDGET( board ) );
	
    gtk_signal_connect( GTK_OBJECT( bd->position_id ), "activate",
			GTK_SIGNAL_FUNC( board_set_position ), bd );
    gtk_signal_connect( GTK_OBJECT( bd->reset ), "clicked",
			GTK_SIGNAL_FUNC( ShowBoard ), NULL );

    gtk_signal_connect( GTK_OBJECT( bd->stop ), "clicked",
			GTK_SIGNAL_FUNC( board_stop ), NULL );
    
    gtk_signal_connect( GTK_OBJECT( bd->edit ), "toggled",
			GTK_SIGNAL_FUNC( board_edit ), bd );
    
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

    /* FIXME also trap keys in the board window; redirect them to xterm */
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
#if WIN32
	{
	    HKEY hkey;
	    char sz[ 256 ];
	    LONG cch = 256;
	    
	    fWine = !RegOpenKey( HKEY_LOCAL_MACHINE,
				 "HARDWARE\\DESCRIPTION\\System", &hkey ) &&
		!RegQueryValueEx( hkey, "Identifier", NULL, NULL, sz, &cch ) &&
		!strcmp( sz, "SystemType WINE" );
	}	
#endif
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
