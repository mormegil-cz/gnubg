/*
 * gdkgetrgb.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000.
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

#include <gdk/gdk.h>
#include "gdkgetrgb.h"

void
gdk_get_rgb_image( GdkDrawable *drawable,
		   GdkColormap *colormap,
		   gint x,
		   gint y,
		   gint width,
		   gint height,
		   guchar *rgb_buf,
		   gint rowstride ) {
    
    GdkImage *image;
    gint xpix, ypix;
    guchar *p;
    guint32 pixel;
    GdkVisual *visual;
    int r, g, b, dwidth, dheight;

    gdk_window_get_geometry( drawable, NULL, NULL, &dwidth, &dheight, NULL );
    
    if( x <= -width || x >= dwidth )
	return;
    else if( x < 0 ) {
	rgb_buf -= 3 * x;
	width += x;
	x = 0;
    } else if( x + width > dwidth )
	width = dwidth - x;

    if( y <= -height || y >= dheight )
	return;
    else if( y < 0 ) {
	rgb_buf -= rowstride * y;
	height += y;
	y = 0;
    } else if( y + height > dheight )
	height = dheight - y;
    
    visual = gdk_colormap_get_visual( colormap );

    if( visual->type == GDK_VISUAL_TRUE_COLOR ) {
	r = visual->red_shift + visual->red_prec - 8;
	g = visual->green_shift + visual->green_prec - 8;
	b = visual->blue_shift + visual->blue_prec - 8;
    }
    
    image = gdk_image_get( drawable, x, y, width, height );

    for( ypix = 0; ypix < height; ypix++ ) {
	p = rgb_buf;
	
	for( xpix = 0; xpix < width; xpix++ ) {
	    pixel = gdk_image_get_pixel( image, xpix, ypix );

	    switch( visual->type ) {
	    case GDK_VISUAL_STATIC_GRAY:
	    case GDK_VISUAL_GRAYSCALE:
	    case GDK_VISUAL_STATIC_COLOR:
	    case GDK_VISUAL_PSEUDO_COLOR:
		*p++ = colormap->colors[ pixel ].red >> 8;
		*p++ = colormap->colors[ pixel ].green >> 8;
		*p++ = colormap->colors[ pixel ].blue >> 8;
		break;
		
	    case GDK_VISUAL_TRUE_COLOR:
		*p++ = r > 0 ? ( pixel & visual->red_mask ) >> r :
		    ( pixel & visual->red_mask ) << -r;
		*p++ = g > 0 ? ( pixel & visual->green_mask ) >> g :
		    ( pixel & visual->green_mask ) << -g;
		*p++ = b > 0 ? ( pixel & visual->blue_mask ) >> b :
		    ( pixel & visual->blue_mask ) << -b;
		break;
		
	    case GDK_VISUAL_DIRECT_COLOR:
		*p++ = colormap->colors[ ( pixel & visual->red_mask )
				       >> visual->red_shift ].red >> 8;
		*p++ = colormap->colors[ ( pixel & visual->green_mask )
				       >> visual->green_shift ].green >> 8;
		*p++ = colormap->colors[ ( pixel & visual->blue_mask )
				       >> visual->blue_shift ].blue >> 8;
		break;
	    }
	}

	rgb_buf += rowstride;
    }
}
