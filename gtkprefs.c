/*
 * gtkprefs.c
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

#include "config.h"

#include <gtk/gtk.h>
#include <math.h>

#include "backgammon.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"

static GtkAdjustment *apadj[ 2 ], *paAzimuth, *paElevation,
    *apadjCoefficient[ 2 ], *apadjExponent[ 2 ], *apadjPoint[ 2 ],
    *padjBoard;
static GtkWidget *apwColour[ 2 ], *apwPoint[ 2 ], *pwBoard, *pwTranslucent;
static int fTranslucent;

static GtkWidget *ChequerPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;

    pw = gtk_vbox_new( FALSE, 0 );

    apadj[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new( bd->arRefraction[ f ],
						     1.0, 3.5, 0.1, 1.0,
						     0.0 ) );
    apadjCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->arCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->arExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_container_add( GTK_CONTAINER( pw ),
		       apwColour[ f ] = gtk_color_selection_new() );
    gtk_color_selection_set_opacity( GTK_COLOR_SELECTION( apwColour[ f ] ),
				     fTranslucent );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwColour[ f ] ),
				   bd->aarColour[ f ] );
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( "Refractive Index:" ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Dull" ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Shiny" ), FALSE,
			FALSE, 4 );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Diffuse" ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Specular" ), FALSE,
			FALSE, 4 );
    
    return pw;
}

static GtkWidget *PointPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i;
    
    pw = gtk_vbox_new( FALSE, 0 );

    apadjPoint[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->aSpeckle[ 2 + f ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ 2 + f ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwPoint[ f ] =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwPoint[ f ] ),
				   ar );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Smooth" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjPoint[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Speckled" ),
			FALSE, FALSE, 4 );

    return pw;
}

static GtkWidget *BoardPage( BoardData *bd ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i;
    
    pw = gtk_vbox_new( FALSE, 0 );

    padjBoard = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->aSpeckle[ 0 ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ 0 ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), pwBoard =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( pwBoard ),
				   ar );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Smooth" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	padjBoard ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Speckled" ),
			FALSE, FALSE, 4 );

    return pw;
}

static void ToggleTranslucent( GtkWidget *pw ) {

    fTranslucent = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );

    gtk_color_selection_set_opacity( GTK_COLOR_SELECTION( apwColour[ 0 ] ),
				     fTranslucent );
    gtk_color_selection_set_opacity( GTK_COLOR_SELECTION( apwColour[ 1 ] ),
				     fTranslucent );
}

static GtkWidget *GeneralPage( BoardData *bd ) {

    GtkWidget *pw, *pwTable;
    float rAzimuth, rElevation;
    
    pw = gtk_vbox_new( FALSE, 0 );
    
    pwTranslucent = gtk_check_button_new_with_label( "Translucent chequers" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwTranslucent ),
				  fTranslucent );
    gtk_signal_connect( GTK_OBJECT( pwTranslucent ), "toggled",
			GTK_SIGNAL_FUNC( ToggleTranslucent ), NULL );
    gtk_box_pack_start( GTK_BOX( pw ), pwTranslucent, FALSE, FALSE, 4 );

    pwTable = gtk_table_new( 2, 2, FALSE );
    gtk_box_pack_start( GTK_BOX( pw ), pwTable, FALSE, FALSE, 0 );
    
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( "Light azimuth" ),
		      0, 1, 0, 1, 0, 0, 4, 2 );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( "Light elevation" ),
		      0, 1, 1, 2, 0, 0, 4, 2 );

    rElevation = asinf( bd->arLight[ 2 ] ) * 180 / M_PI;
    rAzimuth = acosf( bd->arLight[ 0 ] / sqrt( 1.0 - bd->arLight[ 2 ] *
					   bd->arLight[ 2 ] ) ) * 180 / M_PI;
    if( bd->arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;
    
    paAzimuth = GTK_ADJUSTMENT( gtk_adjustment_new( rAzimuth, 0.0, 360.0, 1.0,
						    30.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_hscale_new( paAzimuth ),
		      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 4, 2 );

    paElevation = GTK_ADJUSTMENT( gtk_adjustment_new( rElevation, 0.0, 90.0,
						      1.0, 10.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_hscale_new( paElevation ),
		      1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
    return pw;
}

static void BoardPrefsOK( GtkWidget *pw, BoardData *bd ) {

    int i;
    gdouble ar[ 4 ];
    
    for( i = 0; i < 2; i++ ) {
	bd->arRefraction[ i ] = apadj[ i ]->value;
	bd->arCoefficient[ i ] = apadjCoefficient[ i ]->value;
	bd->arExponent[ i ] = apadjExponent[ i ]->value;
    }
    
    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 0 ] ),
				   bd->aarColour[ 0 ] );
    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 1 ] ),
				   bd->aarColour[ 1 ] );

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( pwBoard ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 0 ][ i ] = ar[ i ] * 0xFF;

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwPoint[ 0 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 2 ][ i ] = ar[ i ] * 0xFF;
    
    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwPoint[ 1 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 3 ][ i ] = ar[ i ] * 0xFF;

    bd->aSpeckle[ 0 ] = padjBoard->value * 0x80;
    bd->aSpeckle[ 2 ] = apadjPoint[ 0 ]->value * 0x80;
    bd->aSpeckle[ 3 ] = apadjPoint[ 1 ]->value * 0x80;
    
    bd->arLight[ 2 ] = sinf( paElevation->value / 180 * M_PI );
    bd->arLight[ 0 ] = cosf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
    bd->arLight[ 1 ] = sinf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
    
    board_free_pixmaps( bd );
    bd->translucent = fTranslucent;
    board_create_pixmaps( bd->widget, bd );
    
    gtk_widget_queue_draw( bd->drawing_area );
    gtk_widget_queue_draw( bd->dice_area );
    gtk_widget_queue_draw( bd->table );
			   
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

extern void BoardPreferences( BoardData *bd ) {

    GtkWidget *pwDialog, *pwNotebook;

    fTranslucent = bd->translucent;
    
    pwDialog = CreateDialog( "Board appearance", TRUE,
			     GTK_SIGNAL_FUNC( BoardPrefsOK ), bd );

    pwNotebook = gtk_notebook_new();
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwNotebook );

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      ChequerPrefs( bd, 0 ),
			      gtk_label_new( "Chequers (0)" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      ChequerPrefs( bd, 1 ),
			      gtk_label_new( "Chequers (1)" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      BoardPage( bd ),
			      gtk_label_new( "Board" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PointPrefs( bd, 0 ),
			      gtk_label_new( "Points (0)" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PointPrefs( bd, 1 ),
			      gtk_label_new( "Points (1)" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      GeneralPage( bd ),
			      gtk_label_new( "General" ) );

    /* FIXME add settings for ambient light, and border and cube colours */
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    gtk_main();
}
