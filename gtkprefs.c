
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
#include <unistd.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#endif

#include "backgammon.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#include "i18n.h"
#include "path.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_color_selection_set_has_opacity_control \
    gtk_color_selection_set_opacity
#endif

static GtkAdjustment *apadj[ 2 ], *paAzimuth, *paElevation,
    *apadjCoefficient[ 2 ], *apadjExponent[ 2 ], *apadjPoint[ 2 ],
    *apadjBoard[ 2 ], *padjSpeed, *padjRound;
static GtkAdjustment *apadjDiceExponent[ 2 ], *apadjDiceCoefficient[ 2 ];
static GtkWidget *apwColour[ 2 ], *apwPoint[ 2 ], *apwBoard[ 2 ],
    *pwTranslucent, *pwLabels, *pwUseDiceIcon, *pwPermitIllegal,
    *pwBeepIllegal, *pwHigherDieFirst, *pwAnimateNone, *pwAnimateBlink,
    *pwAnimateSlide, *pwSpeed, *pwWood, *pwWoodType, *pwWoodMenu, *pwHinges;

#if HAVE_LIBXML2
static GtkWidget *pwDesignTitle;
static GtkWidget *pwDesignAuthor;
static GtkWidget *pwDesignPixmap;
static GtkWidget *pwDesignList;
static GtkWidget *pwDesignUse;
/* static GtkWidget *pwDesignRemove; */
#endif /* HAVE_LIBXML2 */

static GtkWidget *pwShowIDs;
static GtkWidget *pwShowPips;
static GtkWidget *apwDiceColour[ 2 ];
static GtkWidget *pwCubeColour;
static GtkWidget *apwDiceDotColour[ 2 ];
static GtkWidget *apwDieColor[ 2 ];
static GtkWidget *apwDiceColorBox[ 2 ];
static int fTranslucent, fLabels, fUseDiceIcon, fPermitIllegal, fBeepIllegal,
    fHigherDieFirst, fWood, fHinges;
static int fShowIDs;
static int fShowPips;
static animation anim;

#if HAVE_LIBXML2

static GList *
ParseBoardDesigns ( const char *szFile );

static void BoardPrefsDo( GtkWidget *pw, BoardData *bd, int fOK );


typedef struct _boarddesign {

  gchar *szTitle;        /* Title of board design */
  gchar *szAuthor;       /* Name of author */
  gchar *szFilePreview;  /* preview picture */
  gchar *szBoardDesign;  /* Command for setting board */

} boarddesign;


static boarddesign *pbdeSelected = NULL;

extern GList *
read_board_designs ( void ) {

  return ParseBoardDesigns ( "boards/boards.xml" );
}

static void
free_board_design ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;

  if ( ! pbde )
    return;

  g_free ( pbde->szTitle );
  g_free ( pbde->szAuthor );
  g_free ( pbde->szFilePreview );
  g_free ( pbde->szBoardDesign );

  g_free ( data );

}

extern void
free_board_designs ( GList *pl ) {

  g_list_foreach ( pl, free_board_design, NULL );
  g_list_free ( pl );

}


#endif /* HAVE_LIBXML2 */



static GtkWidget *ChequerPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

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
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );
    
    return pwx;
}


static void
DieColorChanged ( GtkWidget *pw, GtkWidget *pwBox ) {

  int f = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( pw ) );
  gtk_widget_set_sensitive ( GTK_WIDGET ( pwBox ), ! f );

}

static GtkWidget *DicePrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    GtkWidget *pwvbox;
    GtkWidget *pwFrame;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    /* frame with color selections for the dice */

    pwFrame = gtk_frame_new ( _("Dice color") );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwFrame, 
                         FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    apwDieColor[ f ] = 
      gtk_check_button_new_with_label ( _("Die color same "
                                          "as chequer color" ) );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDieColor[ f ], FALSE, FALSE, 0 );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColor[ f ] ),
                                   bd->afDieColor[ f ] );


    apwDiceColorBox[ f ] = gtk_vbox_new ( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDiceColorBox[ f ], FALSE, FALSE, 0 );

    /*
      refractive index not used for dice
    apadjDice[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new( bd->arDiceRefraction[ f ],
						     1.0, 3.5, 0.1, 1.0,
						     0.0 ) );
    */
    apadjDiceCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->arDiceCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjDiceExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->arDiceExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColorBox[ f ] ),
			apwDiceColour[ f ] = gtk_color_selection_new(),
			FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
	GTK_COLOR_SELECTION( apwDiceColour[ f ] ), FALSE );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwDiceColour[ f ] ),
				   bd->aarDiceColour[ f ] );
    gtk_box_pack_start( GTK_BOX( apwDiceColorBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    /*
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
    */

    gtk_box_pack_start( GTK_BOX( apwDiceColorBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColorBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), 
                        gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), 
                        gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );

    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColorBox[ f ] ),
                               ! bd->afDieColor[ f ] );

    /* color of dot on dice */
    
    pwFrame = gtk_frame_new ( _("Color of dice dot") );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwFrame, FALSE, FALSE, 0 );

    gtk_container_add ( GTK_CONTAINER ( pwFrame ), 
			apwDiceDotColour[ f ] = gtk_color_selection_new() );
    gtk_color_selection_set_has_opacity_control(
	GTK_COLOR_SELECTION( apwDiceDotColour[ f ] ), FALSE );
    gtk_color_selection_set_color( 
        GTK_COLOR_SELECTION( apwDiceDotColour[ f ] ),
        bd->aarDiceDotColour[ f ] );

    /* signals */

    gtk_signal_connect ( GTK_OBJECT( apwDieColor[ f ] ), "toggled",
                         GTK_SIGNAL_FUNC( DieColorChanged ), 
                         apwDiceColorBox[ f ] );



    return pwx;
}

static GtkWidget *CubePrefs( BoardData *bd ) {


    GtkWidget *pw;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 4 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    /* frame with cube color */

    gtk_box_pack_start ( GTK_BOX ( pw ), 
                         pwCubeColour = gtk_color_selection_new(),
                         FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
	GTK_COLOR_SELECTION( pwCubeColour ), FALSE );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( pwCubeColour ),
				   bd->arCubeColour );

    return pwx;

}

static GtkWidget *PointPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    apadjPoint[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->aSpeckle[ 2 + f ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ 2 + f ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwPoint[ f ] =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
                        GTK_COLOR_SELECTION( apwPoint[ f ] ), FALSE );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwPoint[ f ] ),
				   ar );

    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Smooth") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjPoint[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Speckled") ),
			FALSE, FALSE, 4 );

    return pwx;
}

static GtkWidget *BoardPage( BoardData *bd ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    apadjBoard[ 0 ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->aSpeckle[ 0 ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ 0 ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ 0 ] =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
                        GTK_COLOR_SELECTION( apwBoard[ 0 ] ), FALSE );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwBoard[ 0 ] ),
				   ar );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Smooth") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjBoard[ 0 ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Speckled") ),
			FALSE, FALSE, 4 );
    
    return pwx;
}

static void ToggleWood( GtkWidget *pw, BoardData *bd ) {

    int fWood;
    
    fWood = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
    
    gtk_widget_set_sensitive( pwWoodType, fWood );
    gtk_widget_set_sensitive( apwBoard[ 1 ], !fWood );
}

static GtkWidget *BorderPage( BoardData *bd ) {

    GtkWidget *pw, *pwWoodF;
    gdouble ar[ 4 ];
    int i;
    static char *aszWood[] = {
	N_ ("Alder"), 
        N_ ("Ash"), 
        N_ ("Basswood"), 
        N_ ("Beech"), 
        N_ ("Cedar"), 
        N_ ("Ebony"), 
        N_ ("Fir"), 
        N_ ("Maple"),
	N_ ("Oak"), 
        N_ ("Pine"), 
        N_ ("Redwood"), 
        N_ ("Walnut"), 
        N_ ("Willow")
    };
    BoardWood bw;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ),
			pwWood = gtk_radio_button_new_with_label( NULL,
								  _("Wooden") ),
			FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ), pwWoodType = gtk_option_menu_new(),
			FALSE, FALSE, 4 );

    pwWoodMenu = gtk_menu_new();
    for( bw = 0; bw < WOOD_PAINT; bw++ )
	gtk_menu_shell_append( GTK_MENU_SHELL( pwWoodMenu ),
			       gtk_menu_item_new_with_label( 
                                  gettext ( aszWood[ bw ] ) ) );
	
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pwWoodType ), pwWoodMenu );
    if( bd->wood != WOOD_PAINT )
	gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ), bd->wood );
    
    gtk_box_pack_start( GTK_BOX( pw ),
			pwWoodF = gtk_radio_button_new_with_label_from_widget(
			    GTK_RADIO_BUTTON( pwWood ), _("Painted") ),
			FALSE, FALSE, 0 );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->wood != WOOD_PAINT ?
						     pwWood : pwWoodF ),
				  TRUE );
    
    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->aanBoardColour[ 1 ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ 1 ] =
			gtk_color_selection_new(), FALSE, FALSE, 0 );
    gtk_color_selection_set_has_opacity_control(
                        GTK_COLOR_SELECTION( apwBoard[ 1 ] ), FALSE );
    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwBoard[ 1 ] ),
				   ar );


    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), fHinges );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );

    gtk_signal_connect( GTK_OBJECT( pwWood ), "toggled",
			GTK_SIGNAL_FUNC( ToggleWood ), bd );
    
    gtk_widget_set_sensitive( pwWoodType, bd->wood != WOOD_PAINT );
    gtk_widget_set_sensitive( apwBoard[ 1 ], bd->wood == WOOD_PAINT);
    
    return pwx;
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
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, FALSE, FALSE, 0 );

    pwTranslucent = gtk_check_button_new_with_label( _("Translucent chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwTranslucent ),
				  fTranslucent );
    gtk_signal_connect( GTK_OBJECT( pwTranslucent ), "toggled",
			GTK_SIGNAL_FUNC( ToggleTranslucent ), bd );
    gtk_box_pack_start( GTK_BOX( pw ), pwTranslucent, FALSE, FALSE, 4 );

    pwLabels = gtk_check_button_new_with_label( _("Numbered point labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwLabels ), fLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwLabels, FALSE, FALSE, 0 );
    
    pwUseDiceIcon = 
      gtk_check_button_new_with_label( _("Show dice below board when human "
                                         "player on roll") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwUseDiceIcon ),
				  fUseDiceIcon );
    gtk_box_pack_start( GTK_BOX( pw ), pwUseDiceIcon, FALSE, FALSE, 4 );
    
    pwPermitIllegal = gtk_check_button_new_with_label(
	_("Allow dragging to illegal points") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwPermitIllegal ),
				  fPermitIllegal );
    gtk_box_pack_start( GTK_BOX( pw ), pwPermitIllegal, FALSE, FALSE, 0 );

    pwBeepIllegal = gtk_check_button_new_with_label(
	_("Beep on illegal input") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwBeepIllegal ),
				  fBeepIllegal );
    gtk_box_pack_start( GTK_BOX( pw ), pwBeepIllegal, FALSE, FALSE, 0 );

    pwHigherDieFirst = gtk_check_button_new_with_label(
	_("Show higher die on left") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHigherDieFirst ),
				  fHigherDieFirst );
    gtk_box_pack_start( GTK_BOX( pw ), pwHigherDieFirst, FALSE, FALSE, 0 );

    pwShowIDs = gtk_check_button_new_with_label(
	_("Show Position ID and Match ID") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwShowIDs ),
				  fShowIDs );
    gtk_box_pack_start( GTK_BOX( pw ), pwShowIDs, FALSE, FALSE, 0 );

    pwShowPips = gtk_check_button_new_with_label(
	_("Show pip count permanently") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwShowPips ),
				  fShowPips );
    gtk_box_pack_start( GTK_BOX( pw ), pwShowPips, FALSE, FALSE, 0 );

    pwAnimBox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pw ), pwAnimBox, FALSE, FALSE, 0 );
    
    pwFrame = gtk_frame_new( _("Animation") );
    gtk_box_pack_start( GTK_BOX( pwAnimBox ), pwFrame, FALSE, FALSE, 4 );

    pwBox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame ), pwBox );

    pwAnimateNone = gtk_radio_button_new_with_label( NULL, _("None") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateNone ),
				  anim == ANIMATE_NONE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateNone, FALSE, FALSE, 0 );
    
    pwAnimateBlink = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pwAnimateNone ), _("Blink moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateBlink ),
				  anim == ANIMATE_BLINK );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateBlink, FALSE, FALSE, 0 );
    
    pwAnimateSlide = gtk_radio_button_new_with_label_from_widget(
	GTK_RADIO_BUTTON( pwAnimateNone ), _("Slide moving chequers") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwAnimateSlide ),
				  anim == ANIMATE_SLIDE );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwAnimateSlide, FALSE, FALSE, 0 );

    pwSpeed = gtk_hbox_new( FALSE, 0 );
    
    padjSpeed = GTK_ADJUSTMENT( gtk_adjustment_new( bd->animate_speed, 0, 7,
						    1, 1, 0 ) );
    pwScale = gtk_hscale_new( padjSpeed );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_widget_set_size_request( pwScale, 100, -1 );
#else
    gtk_widget_set_usize ( GTK_WIDGET ( pwScale ), 100, -1 );
#endif
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 0 );

    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Speed:") ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Slow") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwSpeed ), gtk_label_new( _("Fast") ),
			FALSE, FALSE, 4 );
    
    gtk_signal_connect( GTK_OBJECT( pwAnimateNone ), "toggled",
			GTK_SIGNAL_FUNC( ToggleAnimation ), bd );
    ToggleAnimation( pwAnimateNone, NULL );

    gtk_container_add( GTK_CONTAINER( pwAnimBox ), pwSpeed );
    
    pwTable = gtk_table_new( 2, 2, FALSE );
    gtk_box_pack_start( GTK_BOX( pw ), pwTable, FALSE, FALSE, 4 );
    
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( _("Light azimuth") ),
		      0, 1, 0, 1, 0, 0, 4, 2 );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( _("Light elevation") ),
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
    
    pwBox = gtk_hbox_new( FALSE, 0 );
    
    padjRound = GTK_ADJUSTMENT( gtk_adjustment_new( 1.0 - bd->round, 0, 1,
						    0.1, 0.1, 0 ) );
    pwScale = gtk_hscale_new( padjRound );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_widget_set_size_request( pwScale, 100, -1 );
#else
    gtk_widget_set_usize ( GTK_WIDGET ( pwScale ), 100, -1 );
#endif
    gtk_scale_set_draw_value( GTK_SCALE( pwScale ), FALSE );
    gtk_scale_set_digits( GTK_SCALE( pwScale ), 2 );

    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( _("Chequer shape:") ),
			FALSE, FALSE, 8 );
    gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( _("Flat") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwBox ), pwScale, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( _("Round") ),
			FALSE, FALSE, 4 );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwBox, FALSE, FALSE, 4 );

    return pwx;
}

extern void BoardPreferencesStart( GtkWidget *pwBoard ) {

    BoardData *bd = BOARD( pwBoard )->board_data;

    if( GTK_WIDGET_REALIZED( pwBoard ) )
	board_free_pixmaps( bd );
}

#if HAVE_LIBXML2

/* functions for board design */

static void
DesignActivate ( GtkWidget *pw, boarddesign *pbde ) {

  gchar *sz;
  GdkPixmap *ppm;
  GdkBitmap *mask;
  char *szPath = NULL;

#include "xpm/no_picture.xpm"

  /* set title */

  gtk_label_set_text ( GTK_LABEL ( pwDesignTitle ),
                       pbde->szTitle );

  /* set author */

  sz = g_strdup_printf ( _("by %s" ), pbde->szAuthor );
  gtk_label_set_text ( GTK_LABEL ( pwDesignAuthor ), sz );
  g_free ( sz );

  /* set picture */

  if ( pbde->szFilePreview ) {

    szPath = PathSearch ( pbde->szFilePreview, szDataDirectory );
    if ( szPath && access ( szPath, R_OK ) ) {
      g_free ( szPath );
      szPath = NULL;
    }

  }

  if ( pbde->szFilePreview && szPath ) {

    ppm = gdk_pixmap_colormap_create_from_xpm( NULL,
                                               gtk_widget_get_colormap( pw ), 
                                               &mask, NULL,
                                               szPath );

    g_free ( szPath );

    if ( ! ppm )
      return;

  }
  else {

    /* no picture available */

    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
                                                 gtk_widget_get_colormap( pw ), 
                                                 &mask, NULL,
                                                 no_picture_xpm );
    if ( ! ppm )
      return;

  }

  gtk_pixmap_set ( GTK_PIXMAP ( pwDesignPixmap ),
                   ppm, mask );

}


#if 0

static void
SaveDesigns ( GtkWidget *pw, gpointer unused ) {

  /* IMPLEMENT ME */
  
}


static void
AddDesign ( GtkWidget *pw, gpointer unused ) {

  /* IMPLEMENT ME */

}



static void
RemoveDesign ( GtkWidget *pw, gpointer data ) {

  GList *plBoardDesigns = data;
  int i = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ),
                                         pbdeSelected );
  gtk_clist_remove ( GTK_CLIST ( pwDesignList ), i );

  plBoardDesigns = g_list_remove ( plBoardDesigns, pbdeSelected );

}


#endif
static void
UseDesign ( GtkWidget *pw, BoardData *bdBoard ) {

  BoardData bd;
  int i, j;
  gdouble ar[ 4 ];
  gfloat rAzimuth, rElevation;
  char *apch[ 2 ];
  gchar *sz, *pch;

  memset ( &bd, 0, sizeof ( BoardData ) );

  pch = sz = g_strdup ( pbdeSelected->szBoardDesign );
  while( ParseKeyValue( &sz, apch ) ) 
    BoardPreferencesParam( &bd, apch[ 0 ], apch[ 1 ] );
  g_free ( pch );
  

  /* chequers */

  fTranslucent = bd.translucent;
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwTranslucent ), 
                                fTranslucent );

  for ( i = 0; i < 2; ++i ) {
    gtk_color_selection_set_color ( GTK_COLOR_SELECTION ( apwColour[ i ] ),
                                    bd.aarColour[ i ] );

    gtk_color_selection_set_has_opacity_control(
	GTK_COLOR_SELECTION( apwColour[ i ] ), fTranslucent );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadj[ i ] ),
                               bd.arRefraction[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjCoefficient[ i ] ),
                               bd.arCoefficient[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjExponent[ i ] ),
                               bd.arExponent[ i ] );


  }

  /* board, border, and points */

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pwHinges ), 
                                 bd.hinges );
  gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ), bd.wood );

  gtk_widget_set_sensitive( pwWoodType, bd.wood != WOOD_PAINT );
  gtk_widget_set_sensitive( apwBoard[ 1 ], bd.wood == WOOD_PAINT);

  /* board + border */
    
  for ( i = 0; i < 2; ++i ) {

    if ( !i ) 
      gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjBoard[ i ] ),
                                 bd.aSpeckle[ i ] / 128.0 );

    for ( j = 0; j < 3; j++ )
      ar[ j ] = bd.aanBoardColour[ i ][ j ] / 255.0;

    gtk_color_selection_set_color ( GTK_COLOR_SELECTION ( apwBoard[ i ]),
                                    ar );

  }

  /* points */

  for ( i = 0; i < 2; ++i ) {

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjPoint[ i ] ),
                               bd.aSpeckle[ i + 2 ] / 128.0 );

    for ( j = 0; j < 3; j++ )
      ar[ j ] = bd.aanBoardColour[ i + 2 ][ j ] / 255.0;

    gtk_color_selection_set_color ( GTK_COLOR_SELECTION ( apwPoint[ i ]),
                                    ar );

  }

  /* dice + dice dot */

  for ( i = 0; i < 2; ++i ) {

    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColor[ i ] ),
                                   bd.afDieColor[ i ] );
    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColorBox[ i ] ),
                               ! bd.afDieColor[ i ] );

    gtk_color_selection_set_color( GTK_COLOR_SELECTION( apwDiceColour[ i ] ),
                                   bd.afDieColor[ i ] ? 
                                   bd.aarColour[ i ] :
                                   bd.aarDiceColour[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceExponent[ i ] ),
                               bd.afDieColor[ i ] ? 
                               bd.arExponent[ i ] :
                               bd.arDiceExponent[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceCoefficient[ i ] ),
                               bd.afDieColor[ i ] ?
                               bd.arCoefficient[ i ] :
                               bd.arDiceCoefficient[ i ] );


    /* die dot */

    gtk_color_selection_set_color ( GTK_COLOR_SELECTION ( apwDiceDotColour[ i ] ), 
                                    bd.aarDiceDotColour[ i ] );

  }

  /* cube color */
  
  gtk_color_selection_set_color ( GTK_COLOR_SELECTION ( pwCubeColour ), 
                                  bd.arCubeColour );

  /* light */

  rElevation = asinf( bd.arLight[ 2 ] ) * 180 / M_PI;
  rAzimuth = acosf( bd.arLight[ 0 ] / sqrt( 1.0 - bd.arLight[ 2 ] *
                                             bd.arLight[ 2 ] ) ) * 180 / M_PI;
  if( bd.arLight[ 1 ] < 0 )
    rAzimuth = 360 - rAzimuth;
    
  gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paAzimuth ),
                             rAzimuth );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paElevation ),
                             rElevation );


  BoardPrefsDo ( pw, bdBoard, FALSE );

}


static void
AddDesignRow ( gpointer data, gpointer user_data ) {

  GtkWidget *pwList = user_data;
  boarddesign *pbde = data;
  char *asz[ 1 ];
  gint i;

  if ( ! pbde )
    return;

  asz[ 0 ] = pbde->szTitle;

  i = gtk_clist_append ( GTK_CLIST ( pwList ), asz );
  gtk_clist_set_row_data ( GTK_CLIST ( pwList ), i, pbde );

}

static void DesignSelect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused ) {

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUse ), TRUE );
#if 0
  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), TRUE );
#endif

  pbdeSelected = gtk_clist_get_row_data ( GTK_CLIST ( pwDesignList ),
                                          nRow );

  DesignActivate ( pwDesignUse, pbdeSelected );

}

static void DesignUnselect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused ) {

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUse ), FALSE );
#if 0
  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );
#endif

  pbdeSelected = NULL;

}


static GtkWidget *
DesignPage ( GList *plBoardDesigns, BoardData *bd ) {

  GtkWidget *pwvbox;
  GtkWidget *pwhbox;
  GtkWidget *pwFrame;
  GtkWidget *pwv;
  GtkWidget *pwScrolled;
  GtkWidget *pwPage;

#include "xpm/no_picture.xpm"

  pwPage = gtk_hbox_new ( FALSE, 4 );

  /* CList with board designs */

  pwScrolled = gtk_scrolled_window_new( NULL, NULL );
  gtk_container_add ( GTK_CONTAINER ( pwPage ), pwScrolled );

  pwDesignList = gtk_clist_new( 1 );
  gtk_clist_set_column_auto_resize( GTK_CLIST( pwDesignList ), 0, TRUE );
  
  g_list_foreach ( plBoardDesigns, AddDesignRow, pwDesignList );
  gtk_container_add ( GTK_CONTAINER ( pwScrolled ), pwDesignList );

  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "select-row",
                      GTK_SIGNAL_FUNC( DesignSelect ), NULL );
  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "unselect-row",
                      GTK_SIGNAL_FUNC( DesignUnselect ), NULL );


  /* preview page */

  pwvbox = gtk_vbox_new ( FALSE, 4 );
  gtk_container_add ( GTK_CONTAINER ( pwPage ), pwvbox );

  pwFrame = gtk_frame_new ( _("Selected board design" ) );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pwFrame, FALSE, FALSE, 4 );

  pwv = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwv );

  /* design title */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwv ), pwhbox, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       pwDesignTitle = gtk_label_new ( NULL ),
                       FALSE, FALSE, 0 );

  /* design author */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwv ), pwhbox, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       pwDesignAuthor = gtk_label_new ( NULL ),
                       FALSE, FALSE, 0 );

  /* design preview */

  pwDesignPixmap = image_from_xpm_d ( no_picture_xpm, 
                                       pwvbox );

  gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                       pwDesignPixmap,
                       FALSE, FALSE, 0 );


  /* horisontal separator */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                       gtk_hseparator_new (), FALSE, FALSE, 4 );

  /* button: use design */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );

  /* buttons */

  pwDesignUse = gtk_button_new_with_label ( _("Apply design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignUse, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignUse ), "clicked",
                       GTK_SIGNAL_FUNC ( UseDesign ), bd );

#if 0

  pwDesignRemove = gtk_button_new_with_label ( _("Remove design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignRemove, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignRemove ), "clicked",
                       GTK_SIGNAL_FUNC ( RemoveDesign ), plBoardDesigns );

  pw = gtk_button_new_with_label ( _("Add current design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                       GTK_SIGNAL_FUNC ( AddDesign ), NULL );
  gtk_widget_set_sensitive ( GTK_WIDGET ( pw ), FALSE );

  pw = gtk_button_new_with_label ( _("Save designs") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                       GTK_SIGNAL_FUNC ( SaveDesigns ), NULL );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );

#endif

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignUse ), FALSE );

  return pwPage;

}

#endif /* HAVE_LIBXML2 */


extern void BoardPreferencesDone( GtkWidget *pwBoard ) {
    
    BoardData *bd = BOARD( pwBoard )->board_data;
    
    if( GTK_WIDGET_REALIZED( pwBoard ) ) {
	board_create_pixmaps( pwBoard, bd );

        /* dice area */

	if( GTK_WIDGET_VISIBLE( bd->dice_area ) && !bd->usedicearea ) {
	    gtk_widget_hide( bd->dice_area );
	}

	if( ! GTK_WIDGET_VISIBLE( bd->dice_area ) && bd->usedicearea ) {
	    gtk_widget_show_all( bd->dice_area );
	}

        /* Position ID and Match ID */
        
        if ( GTK_WIDGET_VISIBLE ( bd->vbox_ids ) != bd->show_ids ) {
          if ( bd->show_ids )
            gtk_widget_show_all ( bd->vbox_ids );
          else
            gtk_widget_hide ( bd->vbox_ids );
        }

	gtk_widget_queue_draw( bd->drawing_area );
	gtk_widget_queue_draw( bd->dice_area );
	gtk_widget_queue_draw( bd->table );
    }
}

/* "OK" if fOK is TRUE; "Apply" if fOK is FALSE. */
static void BoardPrefsDo( GtkWidget *pw, BoardData *bd, int fOK ) {

    int i, j, fTranslucentSaved;
    gdouble ar[ 4 ];
    char sz[ 512 ];
    
    fLabels = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwLabels ) );
    fUseDiceIcon = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwUseDiceIcon ) );
    fPermitIllegal = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwPermitIllegal ) );
    fBeepIllegal = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwBeepIllegal ) );
    fHigherDieFirst = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwHigherDieFirst ) );
    fHinges = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwHinges ) );
    fWood = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwWood ) );
    fShowIDs = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwShowIDs ) );
    fShowPips = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pwShowPips ) );
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwAnimateBlink ) ) )
	anim = ANIMATE_BLINK;
    else if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
	pwAnimateSlide ) ) )
	anim = ANIMATE_SLIDE;
    else
	anim = ANIMATE_NONE;

    bd->animate_speed = (int) ( padjSpeed->value + 0.5 );
    bd->round = 1.0 - padjRound->value;
    
    for( i = 0; i < 2; i++ ) {
	bd->arRefraction[ i ] = apadj[ i ]->value;
	bd->arCoefficient[ i ] = apadjCoefficient[ i ]->value;
	bd->arExponent[ i ] = apadjExponent[ i ]->value;

        bd->arDiceCoefficient[ i ] = apadjDiceCoefficient[ i ]->value;
	bd->arDiceExponent[ i ] = apadjDiceExponent[ i ]->value;
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

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwDiceColour[ 0 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->aarDiceColour[ 0 ][ i ] = ar[ i ];
    if( fTranslucent )
	bd->aarDiceColour[ 0 ][ 3 ] = ar[ 3 ];

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( apwDiceColour[ 1 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->aarDiceColour[ 1 ][ i ] = ar[ i ];
    if( fTranslucent )
	bd->aarDiceColour[ 1 ][ 3 ] = ar[ 3 ];

    for ( j = 0; j < 2; ++j ) {

      gtk_color_selection_get_color( 
           GTK_COLOR_SELECTION( apwDiceDotColour[ j ] ), 
           ar );
      for( i = 0; i < 3; i++ )
	bd->aarDiceDotColour[ j ][ i ] = ar[ i ];
      if( fTranslucent )
	bd->aarDiceDotColour[ j ][ 3 ] = ar[ 3 ];
    }

    /* dice color same as chequer color */

    for ( i = 0; i < 2; ++i )
      bd->afDieColor[ i ] = 
        gtk_toggle_button_get_active ( 
           GTK_TOGGLE_BUTTON ( apwDieColor[ i ] ) );

    /* cube color */

    gtk_color_selection_get_color( GTK_COLOR_SELECTION( pwCubeColour ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->arCubeColour[ i ] = ar[ i ];
    if( fTranslucent )
	bd->arCubeColour[ 3 ] = ar[ 3 ];

    /* board color */

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
/*    bd->aSpeckle[ 1 ] = apadjBoard[ 1 ]->value * 0x80; */
    bd->aSpeckle[ 2 ] = apadjPoint[ 0 ]->value * 0x80;
    bd->aSpeckle[ 3 ] = apadjPoint[ 1 ]->value * 0x80;
    
    bd->arLight[ 2 ] = sinf( paElevation->value / 180 * M_PI );
    bd->arLight[ 0 ] = cosf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );
    bd->arLight[ 1 ] = sinf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->arLight[ 2 ] * bd->arLight[ 2 ] );

    bd->labels = fLabels;
    bd->usedicearea = fUseDiceIcon;
    bd->show_ids = fShowIDs;
    bd->show_pips = fShowPips;
    bd->permit_illegal = fPermitIllegal;
    bd->beep_illegal = fBeepIllegal;
    bd->higher_die_first = fHigherDieFirst;
    bd->wood = fWood ? gtk_option_menu_get_history( GTK_OPTION_MENU(
	pwWoodType ) ) : WOOD_PAINT;
    bd->hinges = fHinges;
    bd->animate_computer_moves = anim;
    
    /* This is a horrible hack, but we need translucency set to the new
       value to call BoardPreferencesCommand(), so we get the correct
       output; but then we reset it to the _old_ value before we change,
       so the old pixmaps can be deallocated. */
    fTranslucentSaved = bd->translucent;
    
    bd->translucent = fTranslucent;

    if( fOK )
	gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

    BoardPreferencesCommand( bd->widget, sz );

    bd->translucent = fTranslucentSaved;
    
    UserCommand( sz );
}

static void BoardPrefsApply( GtkWidget *pw, BoardData *bd ) {

    BoardPrefsDo( pw, bd, FALSE );
}

static void BoardPrefsOK( GtkWidget *pw, BoardData *bd ) {

    BoardPrefsDo( pw, bd, TRUE );
}

static void
BoardPrefsDestroy ( GtkWidget *pw, GList *plBoardDesigns ) {

  free_board_designs ( plBoardDesigns );

}


extern void BoardPreferences( GtkWidget *pwBoard ) {

    int i;
    GtkWidget *pwDialog, *pwNotebook,
        *pwApply = gtk_button_new_with_label( _("Apply") );
    BoardData *bd = BOARD( pwBoard )->board_data;
    GList *plBoardDesigns;
    
    fTranslucent = bd->translucent;
    fLabels = bd->labels;
    fUseDiceIcon = bd->usedicearea;
    fShowIDs = bd->show_ids;
    fShowPips = bd->show_pips;
    fPermitIllegal = bd->permit_illegal;
    fBeepIllegal = bd->beep_illegal;
    fHigherDieFirst = bd->higher_die_first;
    fWood = bd->wood;
    fHinges = bd->hinges;
    anim = bd->animate_computer_moves;
    
    pwDialog = CreateDialog( _("GNU Backgammon - Appearance"), DT_QUESTION,
			     GTK_SIGNAL_FUNC( BoardPrefsOK ), bd );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
                       pwApply );
    gtk_signal_connect( GTK_OBJECT( pwApply ), "clicked",
			GTK_SIGNAL_FUNC( BoardPrefsApply ), bd );

    pwNotebook = gtk_notebook_new();
    gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
    gtk_notebook_popup_enable( GTK_NOTEBOOK( pwNotebook ) );
    
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwNotebook );

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      GeneralPage( bd ),
			      gtk_label_new( _("General") ) );
#if HAVE_LIBXML2
    plBoardDesigns = read_board_designs ();
    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                               DesignPage ( plBoardDesigns, bd ),
                               gtk_label_new ( "Designs" ) );
#endif /* HAVE_LIBXML2 */
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      ChequerPrefs( bd, 0 ),
			      gtk_label_new( _("Chequers (0)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      ChequerPrefs( bd, 1 ),
			      gtk_label_new( _("Chequers (1)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      BoardPage( bd ), gtk_label_new( _("Board") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), BorderPage( bd ),
			      gtk_label_new( _("Border") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PointPrefs( bd, 0 ),
			      gtk_label_new( _("Points (0)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PointPrefs( bd, 1 ),
			      gtk_label_new( _("Points (1)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      DicePrefs( bd, 0 ),
			      gtk_label_new( _("Dice (0)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      DicePrefs( bd, 1 ),
			      gtk_label_new( _("Dice (1)") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      CubePrefs( bd ),
			      gtk_label_new( _("Cube") ) );

    /* FIXME add settings for ambient light, and dice pip and cube colours */
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
#if HAVE_LIBXML2
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( BoardPrefsDestroy ), plBoardDesigns );
#endif /* HAVE_LIBXML2 */
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    /* hack the set_opacity function does not work until widget has been
       drawn */

    for ( i = 0; i < 2; i++ ) {
      gtk_color_selection_set_has_opacity_control(
         GTK_COLOR_SELECTION( apwPoint[ i ] ), FALSE );
      gtk_color_selection_set_has_opacity_control(
         GTK_COLOR_SELECTION( apwBoard[ i ] ), FALSE );
      gtk_color_selection_set_has_opacity_control(
         GTK_COLOR_SELECTION( apwDiceColour[ i ] ), FALSE );
      gtk_color_selection_set_has_opacity_control(
         GTK_COLOR_SELECTION( apwDiceDotColour[ i ] ), FALSE );
    }
    gtk_color_selection_set_has_opacity_control(
         GTK_COLOR_SELECTION( pwCubeColour ), FALSE );



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
            PushLocale ( "C" );
	    *pnSpeckle = atof( pch ) * 128;
            PopLocale ();
	    
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
static int SetColourARSS( gdouble aarColour[ 2 ][ 4 ], 
                          gfloat arRefraction[ 2 ],
                          gfloat arCoefficient[ 2 ],
                          gfloat arExponent[ 2 ],
                          char *sz, int i ) {

    char *pch;
    GdkColor col;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( gdk_color_parse( sz, &col ) ) {
	aarColour[ i ][ 0 ] = col.red / 65535.0f;
	aarColour[ i ][ 1 ] = col.green / 65535.0f;
	aarColour[ i ][ 2 ] = col.blue / 65535.0f;

        PushLocale ( "C" );

	if( pch ) {
	    /* alpha */
	    aarColour[ i ][ 3 ] = atof( pch );

	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    aarColour[ i ][ 3 ] = 1.0f; /* opaque */

	if( pch ) {
	    /* refraction */
	    arRefraction[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arRefraction[ i ] = 1.5f;

	if( pch ) {
	    /* shine */
	    arCoefficient[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arCoefficient[ i ] = 0.5f;

	if( pch ) {
	    /* specular */
	    arExponent[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arExponent[ i ] = 10.0f;	

        PopLocale ();

	return 0;
    }

    return -1;
}

/* Set colour, shine, specular, flag. */
static int SetColourSSF( gdouble aarColour[ 2 ][ 4 ], 
                         gfloat arCoefficient[ 2 ],
                         gfloat arExponent[ 2 ],
                         int afDieColor[ 2 ],
                         char *sz, int i ) {

    char *pch;
    GdkColor col;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( gdk_color_parse( sz, &col ) ) {
	aarColour[ i ][ 0 ] = col.red / 65535.0f;
	aarColour[ i ][ 1 ] = col.green / 65535.0f;
	aarColour[ i ][ 2 ] = col.blue / 65535.0f;

        PushLocale ( "C" );

	if( pch ) {
	    /* shine */
	    arCoefficient[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arCoefficient[ i ] = 0.5f;

	if( pch ) {
	    /* specular */
	    arExponent[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arExponent[ i ] = 10.0f;	

	if( pch ) {
            /* die color same as chequer color */
            afDieColor[ i ] = ( toupper ( *pch ) == 'Y' );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
            afDieColor[ i ] = TRUE;

        PopLocale ();

	return 0;
    }

    return -1;
}

/* Set colour (with floats) */
static int SetColourX( gdouble arColour[ 4 ], char *sz ) {

    char *pch;
    GdkColor col;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( gdk_color_parse( sz, &col ) ) {
	arColour[ 0 ] = col.red / 65535.0f;
	arColour[ 1 ] = col.green / 65535.0f;
	arColour[ 2 ] = col.blue / 65535.0f;
	return 0;
    }

    return -1;
}

static char *aszWoodName[] = {
    "alder", "ash", "basswood", "beech", "cedar", "ebony", "fir", "maple",
    "oak", "pine", "redwood", "walnut", "willow", "paint"
};

static int SetWood( char *sz, BoardWood *pbw ) {

    BoardWood bw;
    int cch = strlen( sz );
    
    for( bw = 0; bw <= WOOD_PAINT; bw++ )
	if( !strncasecmp( sz, aszWoodName[ bw ], cch ) ) {
	    *pbw = bw;
	    return 0;
	}

    return -1;
}

extern void BoardPreferencesParam( BoardData *bd, char *szParam,
				   char *szValue ) {

    int c, fValueError = FALSE;
    
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
    else if( !g_strncasecmp( szParam, "cube", c ) )
	/* cube=colour */
        fValueError = SetColourX( bd->arCubeColour, szValue );
    else if( !g_strncasecmp( szParam, "translucent", c ) )
	/* translucent=bool */
	bd->translucent = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "labels", c ) )
	/* labels=bool */
	bd->labels = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "diceicon", c ) )
	bd->usedicearea = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "show_ids", c ) )
	bd->show_ids = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "show_pips", c ) )
	bd->show_pips = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "illegal", c ) )
	bd->permit_illegal = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "beep", c ) )
	bd->beep_illegal = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "highdie", c ) )
	bd->higher_die_first = toupper( *szValue ) == 'Y';
    else if( !g_strncasecmp( szParam, "wood", c ) )
	fValueError = SetWood( szValue, &bd->wood );
    else if( !g_strncasecmp( szParam, "hinges", c ) )
	bd->hinges = toupper( *szValue ) == 'Y';
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

	PushLocale ( "C" );
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
        PopLocale ();
    } else if( !g_strncasecmp( szParam, "shape", c ) ) {
	float rRound;

	PushLocale ( "C" );
	if( sscanf( szValue, "%f", &rRound ) < 1 )
	    fValueError = TRUE;
	else
	    bd->round = 1.0 - rRound;
        PopLocale ();
    } else if( c > 1 &&
	       ( !g_strncasecmp( szParam, "chequers", c - 1 ) ||
		 !g_strncasecmp( szParam, "checkers", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* chequers=colour;alpha;refrac;shine;spec */
	fValueError = SetColourARSS( bd->aarColour, 
                                     bd->arRefraction,
                                     bd->arCoefficient,
                                     bd->arExponent,
                                     szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !g_strncasecmp( szParam, "dice", c - 1 ) ||
               !g_strncasecmp( szParam, "dice", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* dice=colour;shine;spec;flag */
        fValueError = SetColourSSF( bd->aarDiceColour, 
                                    bd->arDiceCoefficient,
                                    bd->arDiceExponent,
                                    bd->afDieColor,
                                    szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !g_strncasecmp( szParam, "dot", c - 1 ) ||
               !g_strncasecmp( szParam, "dot", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
        /* dot=colour */
	fValueError = 
          SetColourX( bd->aarDiceDotColour [ szParam[ c - 1 ] - '0' ],
                      szValue );
    else if( c > 1 && !g_strncasecmp( szParam, "points", c - 1 ) &&
	     ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* pointsn=colour;speckle */
	fValueError = SetColourSpeckle( szValue,
					bd->aanBoardColour[
					    szParam[ c - 1 ] - '0' + 2 ],
					&bd->aSpeckle[
					    szParam[ c - 1 ] - '0' + 2 ] );
    else
	outputf( _("Unknown setting `%s'.\n"), szParam );

    if( fValueError )
	outputf( _("`%s' is not a legal value for parameter `%s'.\n"), szValue,
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

    PushLocale ( "C" );
    
    sprintf( sz, 
             "set appearance board=#%02X%02X%02X;%0.2f "
	     "border=#%02X%02X%02X "
	     "translucent=%c labels=%c diceicon=%c illegal=%c "
	     "beep=%c highdie=%c wood=%s hinges=%c "
             "show_ids=%c show_pips=%c "
	     "animate=%s speed=%d light=%0.0f;%0.0f shape=%0.1f " 
	     "chequers0=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "chequers1=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "dice0=#%02X%02X%02X;%0.2f;%0.2f;%c "
	     "dice1=#%02X%02X%02X;%0.2f;%0.2f;%c "
	     "dot0=#%02X%02X%02X "
	     "dot1=#%02X%02X%02X "
	     "cube=#%02X%02X%02X "
	     "points0=#%02X%02X%02X;%0.2f "
	     "points1=#%02X%02X%02X;%0.2f",
             /* board */
	     bd->aanBoardColour[ 0 ][ 0 ], bd->aanBoardColour[ 0 ][ 1 ], 
	     bd->aanBoardColour[ 0 ][ 2 ], bd->aSpeckle[ 0 ] / 128.0f,
             /* border */
	     bd->aanBoardColour[ 1 ][ 0 ], bd->aanBoardColour[ 1 ][ 1 ], 
	     bd->aanBoardColour[ 1 ][ 2 ],
             /* translucent, labels ... */
	     bd->translucent ? 'y' : 'n', 
             bd->labels ? 'y' : 'n',
	     bd->usedicearea ? 'y' : 'n', bd->permit_illegal ? 'y' : 'n',
             /* beep, highdie, .... */
	     bd->beep_illegal ? 'y' : 'n', bd->higher_die_first ? 'y' : 'n',
	     aszWoodName[ bd->wood ],
	     bd->hinges ? 'y' : 'n',
	     bd->show_ids ? 'y' : 'n',
             bd->show_pips ? 'y' : 'n',
             /* animate, speed, ... */
	     aszAnim[ bd->animate_computer_moves ], bd->animate_speed,
	     rAzimuth, rElevation, 1.0 - bd->round,
             /* chequers0 */
             (int) ( bd->aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarColour[ 0 ][ 2 ] * 0xFF ), 
             bd->aarColour[ 0 ][ 3 ], bd->arRefraction[ 0 ], 
             bd->arCoefficient[ 0 ], bd->arExponent[ 0 ],
             /* chequers1 */
	     (int) ( bd->aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarColour[ 1 ][ 2 ] * 0xFF ), 
             bd->aarColour[ 1 ][ 3 ], bd->arRefraction[ 1 ], 
             bd->arCoefficient[ 1 ], bd->arExponent[ 1 ],
             /* dice0 */
             (int) ( bd->aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             bd->arDiceCoefficient[ 0 ], bd->arDiceExponent[ 0 ],
             bd->afDieColor[ 0 ] ? 'y' : 'n',
             /* dice1 */
	     (int) ( bd->aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             bd->arDiceCoefficient[ 1 ], bd->arDiceExponent[ 1 ],
             bd->afDieColor[ 1 ] ? 'y' : 'n',
             /* dot0 */
	     (int) ( bd->aarDiceDotColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarDiceDotColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarDiceDotColour[ 0 ][ 2 ] * 0xFF ), 
             /* dot1 */
	     (int) ( bd->aarDiceDotColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->aarDiceDotColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->aarDiceDotColour[ 1 ][ 2 ] * 0xFF ), 
             /* cube */
	     (int) ( bd->arCubeColour[ 0 ] * 0xFF ),
	     (int) ( bd->arCubeColour[ 1 ] * 0xFF ), 
	     (int) ( bd->arCubeColour[ 2 ] * 0xFF ), 
             /* points0 */
	     bd->aanBoardColour[ 2 ][ 0 ], bd->aanBoardColour[ 2 ][ 1 ], 
	     bd->aanBoardColour[ 2 ][ 2 ], bd->aSpeckle[ 2 ] / 128.0f,
             /* points1 */
	     bd->aanBoardColour[ 3 ][ 0 ], bd->aanBoardColour[ 3 ][ 1 ], 
	     bd->aanBoardColour[ 3 ][ 2 ], bd->aSpeckle[ 3 ] / 128.0f );

    PopLocale ();

    return sz;
}

#if HAVE_LIBXML2

typedef enum _parsestate {
  STATE_NONE,
  STATE_BOARD_DESIGNS,
  STATE_BOARD_DESIGN,
  STATE_ABOUT,
  STATE_TITLE, STATE_AUTHOR, STATE_PREVIEW_FILE,
  STATE_DESIGN, 
} parsestate;

typedef struct _parsecontext {

  GList *pl;  /* list of board designs */

  parsestate aps[ 10 ];      /* the current tag */
  int ips;                   /* depth of stack */

  boarddesign *pbde;
  int i, j;      /* counters */
  int err;     /* return code */

} parsecontext;


static void
PushState ( parsecontext *ppc, const parsestate ps ) {

  if ( ppc->ips > 9 ) {
    ppc->err = TRUE;
    return;
  }

  ++(ppc->ips);
  ppc->aps[ ppc->ips ] = ps;

}

static void
PopState ( parsecontext *ppc ) {

 (ppc->ips)--;

}


static void Err( void *pv, const char *msg, ... ) {

  parsecontext *ppc = pv;
  va_list args;
  
  ppc->err = TRUE;
  
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  
}

static void ScanEndElement( void *pv, const xmlChar *pchName ) {
  
  parsecontext *ppc = pv;

  /* pop element */

  PopState ( ppc );

}



static void ScanCharacters( void *pv, const xmlChar *pchIn, int cch ) {

  parsecontext *ppc = pv;
  char *sz = g_strndup ( pchIn, cch );
  char *pc;

  switch ( ppc->aps[ ppc->ips ] ) {
  case STATE_BOARD_DESIGNS:
  case STATE_BOARD_DESIGN:
    /* no chars in these elements */
    break;

  case STATE_TITLE:

    pc = ppc->pbde->szTitle;
    ppc->pbde->szTitle = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  case STATE_AUTHOR:

    pc = ppc->pbde->szAuthor;
    ppc->pbde->szAuthor = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  case STATE_PREVIEW_FILE:

    pc = ppc->pbde->szFilePreview;
    ppc->pbde->szFilePreview = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  case STATE_DESIGN:
    pc = ppc->pbde->szBoardDesign;
    ppc->pbde->szBoardDesign = g_strconcat( pc, sz, NULL );
    g_free ( pc );
    break;

  default:
    break;
  }

  g_free ( sz );

}

static void ScanStartElement( void *pv, const xmlChar *pchName,
                              const xmlChar **ppchAttrs ) {

  parsecontext *ppc = pv;

  if ( ! strcmp ( pchName, "board-designs" ) && ppc->ips == -1 ) {

    PushState ( ppc, STATE_BOARD_DESIGNS );

  }
  else if ( ! strcmp ( pchName, "board-design" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGNS ) {

    /* allocate list if empty */
    if ( ! ppc->pl )
      ppc->pl = g_list_alloc ();

    ppc->pbde = (boarddesign *) malloc ( sizeof ( boarddesign ) );
    memset ( ppc->pbde, 0, sizeof ( boarddesign ) );
    g_list_append ( ppc->pl, (gpointer) ppc->pbde );

    PushState ( ppc, STATE_BOARD_DESIGN );

  }
  else if ( ! strcmp ( pchName, "about" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGN ) {

    PushState ( ppc, STATE_ABOUT );

  }
  else if ( ! strcmp ( pchName, "title" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_TITLE );
    ppc->pbde->szTitle = g_strdup ( "" );

  }
  else if ( ! strcmp ( pchName, "author" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_AUTHOR );
    ppc->pbde->szAuthor = g_strdup ( "" );

  }
  else if ( ! strcmp ( pchName, "preview-file" ) &&
            ppc->aps[ ppc->ips ] == STATE_ABOUT ) {

    PushState ( ppc, STATE_PREVIEW_FILE );
    ppc->pbde->szFilePreview = g_strdup ( "" );

  }
  else if ( ! strcmp ( pchName, "design" ) &&
            ppc->aps[ ppc->ips ] == STATE_BOARD_DESIGN ) {

    PushState ( ppc, STATE_DESIGN );
    ppc->pbde->szBoardDesign = g_strdup ( "" );

  }
  else
    printf ( "ignoring start tag \"%s\"\n", pchName );


}


static xmlSAXHandler xsaxScan = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    ScanStartElement, /* startElement */
    ScanEndElement, /* endElement */
    NULL, /* reference */
    ScanCharacters, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    Err, /* xmlParserWarning */
    Err, /* xmlParserError */
    Err, /* fatal error */
    NULL, /* getParameterEntity */
    NULL, /* cdataBlock; */
    NULL,  /* externalSubset; */
    TRUE
};

static GList *
ParseBoardDesigns ( const char *szFile ) {

  xmlParserCtxt *pxpc;
  parsecontext pc;
  char *pch;

  pc.ips = -1;
  pc.pl = NULL;
  pc.err = FALSE;

  /* create parser context */

  if ( ! ( pch = PathSearch ( szFile, szDataDirectory ) ) )
    return NULL;
  pxpc = xmlCreateFileParserCtxt ( pch );
  free ( pch );
  if ( ! pxpc )
    return NULL;

  pxpc->sax = &xsaxScan;
  pxpc->userData = &pc;

  /* parse document */

  xmlParseDocument ( pxpc );

  if ( pc.err )
    free_board_designs ( pc.pl );

  return pc.err ? NULL : pc.pl;

}

#endif /* HAVE_LIBXML2 */
