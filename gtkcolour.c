/*
 * gtkcolour.c
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2002.
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

#include "gtkcolour.h"
#include "i18n.h"

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_color_selection_set_has_opacity_control \
    gtk_color_selection_set_opacity
#define gtk_color_selection_get_has_opacity_control \
    gtk_color_selection_get_opacity
#endif

#define COLOUR_SEL_DIA( pcp ) GTK_COLOR_SELECTION_DIALOG( (pcp)->pwColourSel )
#define COLOUR_SEL( pcp ) GTK_COLOR_SELECTION( COLOUR_SEL_DIA(pcp)->colorsel )

static gpointer parent_class = NULL;

static void set_gc_colour( GdkGC *gc, GdkColormap *pcm, GdkColor *col ) {
    
#if USE_GTK2
    gdk_gc_set_rgb_fg_color( gc, col );
#else
    /* FIXME this is ugly -- we never free the allocated colour.  But
       that only matters on pseudo-colour visuals. */
    gdk_colormap_alloc_color( pcm, col, FALSE, TRUE );
    gdk_gc_set_foreground( gc, col );
#endif
}

extern GtkWidget *gtk_colour_picker_new( void ) {

    GtkColourPicker *pcp;

    pcp = gtk_type_new( GTK_TYPE_COLOUR_PICKER );

    return GTK_WIDGET( pcp );
}

static void render_pixmap( GtkColourPicker *pcp ) {

    GdkGC *gc0, *gc1;
    GdkColor col;
    gdouble ar[ 4 ];
    
    if( !pcp->ppm )
	return;

    /* FIXME different appearance if insensitive */
    
    gc0 = gdk_gc_new( pcp->ppm );
    gc1 = gdk_gc_new( pcp->ppm );

    gtk_color_selection_get_color( COLOUR_SEL( pcp ), ar );
    if( !gtk_color_selection_get_has_opacity_control( COLOUR_SEL( pcp ) ) )
	ar[ 3 ] = 1.0;

    col.red = ( ar[ 0 ] * ar[ 3 ] + 0.66667 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;
    col.green = ( ar[ 1 ] * ar[ 3 ] + 0.66667 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;
    col.blue = ( ar[ 2 ] * ar[ 3 ] + 0.66667 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;

    set_gc_colour( gc0, gtk_widget_get_colormap( GTK_WIDGET( pcp ) ), &col );
    
    col.red = ( ar[ 0 ] * ar[ 3 ] + 0.33333 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;
    col.green = ( ar[ 1 ] * ar[ 3 ] + 0.33333 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;
    col.blue = ( ar[ 2 ] * ar[ 3 ] + 0.33333 * ( 1 - ar[ 3 ] ) ) * 0xFFFF;

    set_gc_colour( gc1, gtk_widget_get_colormap( GTK_WIDGET( pcp ) ), &col );
    
    gdk_draw_rectangle( pcp->ppm, gc0, TRUE, 0, 0, 8, 8 );
    gdk_draw_rectangle( pcp->ppm, gc1, TRUE, 0, 8, 8, 8 );
    gdk_draw_rectangle( pcp->ppm, gc1, TRUE, 8, 0, 8, 8 );
    gdk_draw_rectangle( pcp->ppm, gc0, TRUE, 8, 8, 8, 8 );
    
    gdk_window_clear( pcp->pwDraw->window );
}

extern void gtk_colour_picker_set_has_opacity_control(
    GtkColourPicker *pcp, gboolean f ) {

    gtk_color_selection_set_has_opacity_control( COLOUR_SEL( pcp ), f );

    render_pixmap( pcp );
}

extern void gtk_colour_picker_set_colour( GtkColourPicker *pcp,
					  gdouble *ar ) {
    
    gtk_color_selection_set_color( COLOUR_SEL( pcp ), ar );

    render_pixmap( pcp );
}

extern void gtk_colour_picker_get_colour( GtkColourPicker *pcp,
					  gdouble *ar ) {
    
    gtk_color_selection_get_color( COLOUR_SEL( pcp ), ar );

    render_pixmap( pcp );
}

static void realize( GtkWidget *pwDraw, GtkColourPicker *pcp ) {

    pcp->ppm = gdk_pixmap_new( pwDraw->window, 16, 16, -1 );
    gdk_window_set_back_pixmap( pwDraw->window, pcp->ppm, FALSE );

    render_pixmap( pcp );
}

static void colour_changed( GtkWidget *pw, GtkColourPicker *pcp ) {

    render_pixmap( pcp );
}

static gboolean delete_event( GtkWidget *pw, GdkEvent *pev,
			      GtkColourPicker *pcp ) {
    
    gtk_color_selection_set_color( COLOUR_SEL( pcp ), pcp->arOrig );

    gtk_window_set_modal( GTK_WINDOW( COLOUR_SEL_DIA( pcp ) ), FALSE );
    gtk_widget_hide( GTK_WIDGET( COLOUR_SEL_DIA( pcp ) ) );

    return TRUE;
}

static void ok( GtkWidget *pw, GtkColourPicker *pcp ) {

    gtk_window_set_modal( GTK_WINDOW( COLOUR_SEL_DIA( pcp ) ), FALSE );
    gtk_widget_hide( GTK_WIDGET( COLOUR_SEL_DIA( pcp ) ) );
}

static void cancel( GtkWidget *pw, GtkColourPicker *pcp ) {

    gtk_color_selection_set_color( COLOUR_SEL( pcp ), pcp->arOrig );
    
    gtk_window_set_modal( GTK_WINDOW( COLOUR_SEL_DIA( pcp ) ), FALSE );
    gtk_widget_hide( GTK_WIDGET( COLOUR_SEL_DIA( pcp ) ) );
}

static void gtk_colour_picker_init( GtkColourPicker *pcp ) {

    pcp->ppm = NULL;
    pcp->pwColourSel = gtk_color_selection_dialog_new( _("Choose a colour") );
    
    pcp->pwDraw = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( pcp->pwDraw ), 32, 16 );

    g_signal_connect( pcp->pwDraw, "realize", G_CALLBACK( realize ), pcp );    
    g_signal_connect( COLOUR_SEL( pcp ), "color-changed",
		      G_CALLBACK( colour_changed ), pcp );    
    g_signal_connect( COLOUR_SEL_DIA( pcp ), "delete-event",
		      G_CALLBACK( delete_event ), pcp );
    g_signal_connect( COLOUR_SEL_DIA( pcp )->ok_button, "clicked",
		      G_CALLBACK( ok ), pcp );
    g_signal_connect( COLOUR_SEL_DIA( pcp )->cancel_button, "clicked",
		      G_CALLBACK( cancel ), pcp );
		      
    gtk_container_add( GTK_CONTAINER( pcp ), pcp->pwDraw );
    gtk_widget_show( pcp->pwDraw );
}

static void clicked( GtkButton *pb ) {

    GtkColourPicker *pcp = GTK_COLOUR_PICKER( pb );
    GtkWidget *pwDia = GTK_WIDGET( COLOUR_SEL_DIA( pcp ) );

    /* remember original colour, in case they cancel */
    gtk_color_selection_get_color( COLOUR_SEL( pcp ), pcp->arOrig );
    
    gtk_window_set_transient_for( GTK_WINDOW( pcp->pwColourSel ),
				  GTK_WINDOW( gtk_widget_get_toplevel(
						  GTK_WIDGET( pcp ) ) ) );
    
    gtk_widget_show( pwDia );

    /* FIXME raise window? */
    
    /* If there is a grabbed window, set new dialog as modal */
    if( gtk_grab_get_current() )
	gtk_window_set_modal( GTK_WINDOW( pwDia ), TRUE );
}

static void gtk_colour_picker_class_init( GtkColourPickerClass *pc ) {

    parent_class = gtk_type_class( GTK_TYPE_BUTTON );

    GTK_BUTTON_CLASS( pc )->clicked = clicked;
}

extern GtkType gtk_colour_picker_get_type( void ) {

    static GtkType t;
    static const GtkTypeInfo ti = {
	"GtkColourPicker",
	sizeof( GtkColourPicker ),
	sizeof( GtkColourPickerClass ),
	(GtkClassInitFunc) gtk_colour_picker_class_init,
	(GtkObjectInitFunc) gtk_colour_picker_init,
	NULL, NULL, NULL
    };
    
    if( !t )
	t = gtk_type_unique( GTK_TYPE_BUTTON, &ti );

    return t;
}
