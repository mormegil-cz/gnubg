/*
 * gtkprefs.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001.
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

#include <ctype.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_color_selection_set_has_opacity_control \
    gtk_color_selection_set_opacity
#endif

static GtkAdjustment *apadj[ 2 ], *paAzimuth, *paElevation,
    *apadjCoefficient[ 2 ], *apadjExponent[ 2 ], *apadjPoint[ 2 ],
    *apadjBoard[ 2 ], *padjSpeed;
static GtkWidget *apwColour[ 2 ], *apwPoint[ 2 ], *apwBoard[ 2 ],
    *pwTranslucent, *pwLabels, *pwUseDiceIcon, *pwPermitIllegal,
    *pwBeepIllegal, *pwHigherDieFirst, *pwAnimateNone, *pwAnimateBlink,
    *pwAnimateSlide, *pwSpeed;
static int fTranslucent, fLabels, fUseDiceIcon, fPermitIllegal, fBeepIllegal,
    fHigherDieFirst;
static animation anim;

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
    
    gtk_box_pack_start( GTK_BOX( pw ),
			apwColour[ f ] = gtk_color_selection_new(),
			FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
	GTK_COLOR_SELECTION( apwColour[ f ] ), fTranslucent );
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

static GtkWidget *BoardPage( BoardData *bd, int iCol ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i;
    
    pw = gtk_vbox_new( FALSE, 0 );

    apadjBoard[ iCol ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->aSpeckle[ iCol ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ iCol ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ iCol ] =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwBoard[ iCol ] ),
				   ar );

    if( !iCol ) {
	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Smooth" ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	    apadjBoard[ iCol ] ), TRUE, TRUE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( "Speckled" ),
			    FALSE, FALSE, 4 );
    }
    
    return pw;
}

static void ToggleTranslucent( GtkWidget *pw, BoardData *bd ) {

    gdouble ar[ 4 ];
    
    fTranslucent = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );

    gtk_color_selection_set_has_opacity_control( GTK_COLOR_SELECTION(
	apwColour[ 0 ] ), fTranslucent );
    gtk_color_selection_set_has_opacity_control( GTK_COLOR_SELECTION(
	apwColour[ 1 ] ), fTranslucent );

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 0 ] ), ar );
    ar[ 3 ] = bd->aarColour[ 0 ][ 3 ];
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwColour[ 0 ] ), ar );

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 1 ] ), ar );
    ar[ 3 ] = bd->aarColour[ 1 ][ 3 ];
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwColour[ 1 ] ), ar );
}

static void ToggleAnimation( GtkWidget *pw, void *p ) {

    gtk_widget_set_sensitive( pwSpeed,
			      !gtk_toggle_button_get_active(
				  GTK_TOGGLE_BUTTON( pw ) ) );
}

static GtkWidget *GeneralPage( BoardData *bd ) {

    GtkWidget *pw, *pwTable, *pwBox, *pwAnimBox, *pwFrame, *pwScale;
    float rAzimuth, rElevation;
    
    pw = gtk_vbox_new( FALSE, 0 );
    
    pwTranslucent = gtk_check_button_new_with_label( "Translucent chequers" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwTranslucent ),
				  fTranslucent );
    gtk_signal_connect( GTK_OBJECT( pwTranslucent ), "toggled",
			GTK_SIGNAL_FUNC( ToggleTranslucent ), bd );
    gtk_box_pack_start( GTK_BOX( pw ), pwTranslucent, FALSE, FALSE, 4 );

    pwLabels = gtk_check_button_new_with_label( "Numbered point labels" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwLabels ), fLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwLabels, FALSE, FALSE, 0 );
    
    pwUseDiceIcon = gtk_check_button_new_with_label( "Click dice icon to "
						     "roll" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwUseDiceIcon ),
				  fUseDiceIcon );
    gtk_box_pack_start( GTK_BOX( pw ), pwUseDiceIcon, FALSE, FALSE, 4 );
    
    pwPermitIllegal = gtk_check_button_new_with_label(
	"Allow dragging to illegal points" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwPermitIllegal ),
				  fPermitIllegal );
    gtk_box_pack_start( GTK_BOX( pw ), pwPermitIllegal, FALSE, FALSE, 0 );

    pwBeepIllegal = gtk_check_button_new_with_label(
	"Beep on illegal input" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwBeepIllegal ),
				  fBeepIllegal );
    gtk_box_pack_start( GTK_BOX( pw ), pwBeepIllegal, FALSE, FALSE, 0 );

    pwHigherDieFirst = gtk_check_button_new_with_label(
	"Show higher die on left" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHigherDieFirst ),
				  fHigherDieFirst );
    gtk_box_pack_start( GTK_BOX( pw ), pwHigherDieFirst, FALSE, FALSE, 0 );

    pwAnimBox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pw ), pwAnimBox, FALSE, FALSE, 0 );
    
    pwFrame = gtk_frame_new( "Animation" );
    gtk_box_pack_start( GTK_BOX( pwAnimBox ), pwFrame, FALSE, FALSE, 4 );

    pwBox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame ), pwBox );

    pwAnimateNone = gtk_radio_button_new_with_label( NULL, "None" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateNone ),
				  anim == ANIMATE_NONE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateNone, FALSE, FALSE, 0 );
    
    pwAnimateBlink = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pwAnimateNone ), "Blink moving chequers" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateBlink ),
				  anim == ANIMATE_BLINK );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateBlink, FALSE, FALSE, 0 );
    
    pwAnimateSlide = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pwAnimateNone ), "Slide moving chequers" );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateSlide ),
				  anim == ANIMATE_SLIDE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateSlide, FALSE, FALSE, 0 );

    pwSpeed = gtk_hbox_new( FALSE, 0 );
    
    padjSpeed = GTK_ADJUSTMENT( gtk_adjustment_new( bd->animate_speed, 0, 7,
						    1, 1, 0 ) );
    pwScale = gtk_hscale_new( padjSpeed );
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 0 );

    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( "Speed:" ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( "Slow" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( "Fast" ),
			FALSE, FALSE, 4 );
    
    gtk_signal_connect( GTK_OBJECT( pwAnimateNone ), "toggled",
			GTK_SIGNAL_FUNC( ToggleAnimation ), bd );
    ToggleAnimation( pwAnimateNone, NULL );

    gtk_container_add( GTK_CONTAINER( pwAnimBox ), pwSpeed );
    
    pwTable = gtk_table_new( 2, 2, FALSE );
    gtk_box_pack_start( GTK_BOX( pw ), pwTable, FALSE, FALSE, 4 );
    
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

extern void BoardPreferencesStart( GtkWidget *pwBoard ) {

    BoardData *bd = BOARD( pwBoard )->board_data;

    if( GTK_WIDGET_REALIZED( pwBoard ) )
	board_free_pixmaps( bd );
}

extern void BoardPreferencesDone( GtkWidget *pwBoard ) {
    
    BoardData *bd = BOARD( pwBoard )->board_data;
    
    if( GTK_WIDGET_REALIZED( pwBoard ) ) {
	board_create_pixmaps( pwBoard, bd );

	if( GTK_WIDGET_VISIBLE( bd->dice_area ) && !bd->usedicearea ) {
	    gtk_widget_hide( bd->dice_area );
	    gtk_widget_show_all( bd->rolldouble );
	}

	if( GTK_WIDGET_VISIBLE( bd->rolldouble ) && bd->usedicearea ) {
	    gtk_widget_show_all( bd->dice_area );
	    gtk_widget_hide( bd->rolldouble );
	}
	
	gtk_widget_queue_draw( bd->drawing_area );
	gtk_widget_queue_draw( bd->dice_area );
	gtk_widget_queue_draw( bd->table );
    }
}

static void BoardPrefsOK( GtkWidget *pw, BoardData *bd ) {

    int i, fTranslucentSaved;
    gdouble ar[ 4 ];
    char sz[ 256 ];
    
    fLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwLabels ) );
    fUseDiceIcon = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwUseDiceIcon ) );
    fPermitIllegal = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwPermitIllegal ) );
    fBeepIllegal = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwBeepIllegal ) );
    fHigherDieFirst = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwHigherDieFirst ) );
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwAnimateBlink ) ) )
	anim = ANIMATE_BLINK;
    else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
	pwAnimateSlide ) ) )
	anim = ANIMATE_SLIDE;
    else
	anim = ANIMATE_NONE;

    bd->animate_speed = (int) ( padjSpeed->value + 0.5 );
    
    for( i = 0; i < 2; i++ ) {
	bd->arRefraction[ i ] = apadj[ i ]->value;
	bd->arCoefficient[ i ] = apadjCoefficient[ i ]->value;
	bd->arExponent[ i ] = apadjExponent[ i ]->value;
    }
    
    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 0 ] ), ar );
    for( i = 0; i < 3; i++ )
	bd->aarColour[ 0 ][ i ] = ar[ i ];
    if( fTranslucent )
	bd->aarColour[ 0 ][ 3 ] = ar[ 3 ];

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwColour[ 1 ] ), ar );
    for( i = 0; i < 3; i++ )
	bd->aarColour[ 1 ][ i ] = ar[ i ];
    if( fTranslucent )
	bd->aarColour[ 1 ][ 3 ] = ar[ 3 ];

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwBoard[ 0 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 0 ][ i ] = ar[ i ] * 0xFF;

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwBoard[ 1 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 1 ][ i ] = ar[ i ] * 0xFF;

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwPoint[ 0 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 2 ][ i ] = ar[ i ] * 0xFF;
    
    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwPoint[ 1 ] ),
				   ar );
    for( i = 0; i < 3; i++ )
	bd->aanBoardColour[ 3 ][ i ] = ar[ i ] * 0xFF;

    bd->aSpeckle[ 0 ] = apadjBoard[ 0 ]->value * 0x80;
    bd->aSpeckle[ 1 ] = apadjBoard[ 1 ]->value * 0x80;
    bd->aSpeckle[ 2 ] = apadjPoint[ 0 ]->value * 0x80;
    bd->aSpeckle[ 3 ] = apadjPoint[ 1 ]->value * 0x80;
    
    bd->arLight[ 2 ] = sinf( paElevation->value / 180 * M_PI );
    bd->arLight[ 0 ] = cosf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
    bd->arLight[ 1 ] = sinf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );

    bd->labels = fLabels;
    bd->usedicearea = fUseDiceIcon;
    bd->permit_illegal = fPermitIllegal;
    bd->beep_illegal = fBeepIllegal;
    bd->higher_die_first = fHigherDieFirst;
    bd->animate_computer_moves = anim;
    
    /* This is a horrible hack, but we need translucency set to the new
       value to call BoardPreferencesCommand(), so we get the correct
       output; but then we reset it to the _old_ value before we change,
       so the old pixmaps can be deallocated. */
    fTranslucentSaved = bd->translucent;
    
    bd->translucent = fTranslucent;
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

    BoardPreferencesCommand( bd->widget, sz );

    bd->translucent = fTranslucentSaved;
    
    UserCommand( sz );
}

extern void BoardPreferences( GtkWidget *pwBoard ) {

    GtkWidget *pwDialog, *pwNotebook;
    BoardData *bd = BOARD( pwBoard )->board_data;
    
    fTranslucent = bd->translucent;
    fLabels = bd->labels;
    fUseDiceIcon = bd->usedicearea;
    fPermitIllegal = bd->permit_illegal;
    fBeepIllegal = bd->beep_illegal;
    fHigherDieFirst = bd->higher_die_first;
    anim = bd->animate_computer_moves;
    
    pwDialog = CreateDialog( "GNU Backgammon - Appearance", TRUE,
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
			      BoardPage( bd, 0 ),
			      gtk_label_new( "Board" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      BoardPage( bd, 1 ),
			      gtk_label_new( "Border" ) );
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

static int SetColour( char *sz, guchar anColour[] ) {
    
    GdkColor col;
    
    if( gdk_color_parse( sz, &col ) ) {
	anColour[ 0 ] = col.red >> 8;
	anColour[ 1 ] = col.green >> 8;
	anColour[ 2 ] = col.blue >> 8;
	
	return 0;
    }

    return -1;
}

static int SetColourSpeckle( char *sz, guchar anColour[], int *pnSpeckle ) {
    
    char *pch;
    GdkColor col;
    
    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( gdk_color_parse( sz, &col ) ) {
	anColour[ 0 ] = col.red >> 8;
	anColour[ 1 ] = col.green >> 8;
	anColour[ 2 ] = col.blue >> 8;
	
	if( pch ) {
	    *pnSpeckle = atof( pch ) * 128;
	    
	    if( *pnSpeckle < 0 )
		*pnSpeckle = 0;
	    else if( *pnSpeckle > 128 )
		*pnSpeckle = 128;
	}

	return 0;
    }

    return -1;
}

/* Set colour, alpha, refraction, shine, specular. */
static int SetColourARSS( BoardData *bd, char *sz, int i ) {

    char *pch;
    GdkColor col;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( gdk_color_parse( sz, &col ) ) {
	bd->aarColour[ i ][ 0 ] = col.red / 65535.0f;
	bd->aarColour[ i ][ 1 ] = col.green / 65535.0f;
	bd->aarColour[ i ][ 2 ] = col.blue / 65535.0f;

	if( pch ) {
	    /* alpha */
	    bd->aarColour[ i ][ 3 ] = atof( pch );

	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    bd->aarColour[ i ][ 3 ] = 1.0f; /* opaque */

	if( pch ) {
	    /* refraction */
	    bd->arRefraction[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    bd->arRefraction[ i ] = 1.5f;

	if( pch ) {
	    /* shine */
	    bd->arCoefficient[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    bd->arCoefficient[ i ] = 0.5f;

	if( pch ) {
	    /* specular */
	    bd->arExponent[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    bd->arExponent[ i ] = 10.0f;	

	return 0;
    }

    return -1;
}

extern void BoardPreferencesParam( GtkWidget *pwBoard, char *szParam,
				   char *szValue ) {

    int c, fValueError = FALSE;
    BoardData *bd = BOARD( pwBoard )->board_data;
    
    if( !szParam || !*szParam )
	return;

    if( !szValue )
	szValue = "";
    
    c = strlen( szParam );
    
    if( !g_strncasecmp( szParam, "board", c ) )
	/* board=colour;speckle */
	fValueError = SetColourSpeckle( szValue, bd->aanBoardColour[ 0 ],
					&bd->aSpeckle[ 0 ] );
    else if( !g_strncasecmp( szParam, "border", c ) )
	/* border=colour */
	fValueError = SetColour( szValue, bd->aanBoardColour[ 1 ] );
    else if( !g_strncasecmp( szParam, "translucent", c ) )
	/* translucent=bool */
	bd->translucent = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "labels", c ) )
	/* labels=bool */
	bd->labels = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "diceicon", c ) )
	bd->usedicearea = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "illegal", c ) )
	bd->permit_illegal = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "beep", c ) )
	bd->beep_illegal = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "highdie", c ) )
	bd->higher_die_first = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "animate", c ) ) {
	switch( toupper( *szValue ) ) {
	case 'B':
	    bd->animate_computer_moves = ANIMATE_BLINK;
	    break;
	case 'S':
	    bd->animate_computer_moves = ANIMATE_SLIDE;
	    break;
	default:
	    bd->animate_computer_moves = ANIMATE_NONE;
	    break;
	}
    } else if( !g_strncasecmp( szParam, "speed", c ) ) {
	int n= atoi( szValue );

	if( n < 0 || n > 7 )
	    fValueError = TRUE;
	else
	    bd->animate_speed = n;
    } else if( !g_strncasecmp( szParam, "light", c ) ) {
	/* light=azimuth;elevation */
	float rAzimuth, rElevation;

	if( sscanf( szValue, "%f;%f", &rAzimuth, &rElevation ) < 2 )
	    fValueError = TRUE;
	else {
	    if( rElevation < 0.0f )
		rElevation = 0.0f;
	    else if( rElevation > 90.0f )
		rElevation = 90.0f;
	    
	    bd->arLight[ 2 ] = sinf( rElevation / 180 * M_PI );
	    bd->arLight[ 0 ] = cosf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
	    bd->arLight[ 1 ] = sinf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
	}
    } else if( c > 1 &&
	       ( !g_strncasecmp( szParam, "chequers", c - 1 ) ||
		 !g_strncasecmp( szParam, "checkers", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* chequers=colour;alpha;refrac;shine;spec */
	fValueError = SetColourARSS( bd, szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 && !g_strncasecmp( szParam, "points", c - 1 ) &&
	     ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* pointsn=colour;speckle */
	fValueError = SetColourSpeckle( szValue,
					bd->aanBoardColour[
					    szParam[ c - 1 ] - '0' + 2 ],
					&bd->aSpeckle[
					    szParam[ c - 1 ] - '0' + 2 ] );
    else
	outputf( "Unknown setting `%s'.\n", szParam );

    if( fValueError )
	outputf( "`%s' is not a legal value for parameter `%s'.\n", szValue,
		 szParam );
}

extern char *BoardPreferencesCommand( GtkWidget *pwBoard, char *sz ) {

    BoardData *bd = BOARD( pwBoard )->board_data;
    float rAzimuth, rElevation;
    static char *aszAnim[ ANIMATE_SLIDE + 1 ] = { "none", "blink", "slide" };
    rElevation = asinf( bd->arLight[ 2 ] ) * 180 / M_PI;
    rAzimuth = acosf( bd->arLight[ 0 ] / sqrt( 1.0 - bd->arLight[ 2 ] *
					       bd->arLight[ 2 ] ) ) *
	180 / M_PI;
    if( bd->arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;
    
    sprintf( sz, "set appearance board=#%02X%02X%02X;%0.2f "
	     "border=#%02X%02X%02X "
	     "translucent=%c labels=%c diceicon=%c illegal=%c "
	     "beep=%c highdie=%c animate=%s speed=%d light=%0.0f;%0.0f " 
	     "chequers0=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "chequers1=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "points0=#%02X%02X%02X;%0.2f "
	     "points1=#%02X%02X%02X;%0.2f",
	     bd->aanBoardColour[ 0 ][ 0 ], bd->aanBoardColour[ 0 ][ 1 ], 
	     bd->aanBoardColour[ 0 ][ 2 ], bd->aSpeckle[ 0 ] / 128.0f,
	     bd->aanBoardColour[ 1 ][ 0 ], bd->aanBoardColour[ 1 ][ 1 ], 
	     bd->aanBoardColour[ 1 ][ 2 ],
	     bd->translucent ? 'y' : 'n', bd->labels ? 'y' : 'n',
	     bd->usedicearea ? 'y' : 'n', bd->permit_illegal ? 'y' : 'n',
	     bd->beep_illegal ? 'y' : 'n', bd->higher_die_first ? 'y' : 'n',
	     aszAnim[ bd->animate_computer_moves ], bd->animate_speed,
	     rAzimuth, rElevation, (int) ( bd->aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarColour[ 0 ][ 2 ] * 0xFF ), bd->aarColour[ 0 ][ 3 ],
	     bd->arRefraction[ 0 ], bd->arCoefficient[ 0 ],
	     bd->arExponent[ 0 ],
	     (int) ( bd->aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarColour[ 1 ][ 2 ] * 0xFF ), bd->aarColour[ 1 ][ 3 ],
	     bd->arRefraction[ 1 ], bd->arCoefficient[ 1 ],
	     bd->arExponent[ 1 ],
	     bd->aanBoardColour[ 2 ][ 0 ], bd->aanBoardColour[ 2 ][ 1 ], 
	     bd->aanBoardColour[ 2 ][ 2 ], bd->aSpeckle[ 2 ] / 128.0f,
	     bd->aanBoardColour[ 3 ][ 0 ], bd->aanBoardColour[ 3 ][ 1 ], 
	     bd->aanBoardColour[ 3 ][ 2 ], bd->aSpeckle[ 3 ] / 128.0f );

    return sz;
}
