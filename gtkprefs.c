/*
 * gtkprefs.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001, 2002.
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
#include <time.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "gtkcolour.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#include "i18n.h"
#include "path.h"
#include "render.h"

#define COLOUR_SEL_DIA( pcp ) GTK_COLOR_SELECTION_DIALOG( GTK_COLOUR_PICKER( \
	pcp )->pwColourSel )
#define COLOUR_SEL( pcp ) GTK_COLOR_SELECTION( COLOUR_SEL_DIA(pcp)->colorsel )

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum _pixmapindex {
    PI_DESIGN, PI_CHEQUERS0, PI_CHEQUERS1, PI_BOARD, PI_BORDER, PI_DICE0,
    PI_DICE1, PI_CUBE, NUM_PIXMAPS
} pixmapindex;

static GdkPixmap *ppm; /* preview pixmap */
static GtkAdjustment *apadj[ 2 ], *paAzimuth, *paElevation,
    *apadjCoefficient[ 2 ], *apadjExponent[ 2 ],
    *apadjBoard[ 4 ], *padjSpeed, *padjRound;
static GtkAdjustment *apadjDiceExponent[ 2 ], *apadjDiceCoefficient[ 2 ];
static GtkWidget *apwColour[ 2 ], *apwBoard[ 4 ],
    *pwLabels, *pwUseDiceIcon, *pwPermitIllegal,
    *pwBeepIllegal, *pwHigherDieFirst, *pwAnimateNone, *pwAnimateBlink,
    *pwAnimateSlide, *pwSpeed, *pwWood, *pwWoodType, *pwWoodMenu, *pwHinges,
    *pwWoodF, *pwPreview[ NUM_PIXMAPS ];

#if HAVE_LIBXML2
static GtkWidget *pwDesignTitle;
static GtkWidget *pwDesignAuthor;
static GtkWidget *pwDesignDeletable;
static GtkWidget *pwDesignList;
static GtkWidget *pwDesignRemove;
static GtkWidget *pwDesignAddAuthor;
static GtkWidget *pwDesignAddTitle;
#endif /* HAVE_LIBXML2 */

static GtkWidget *pwShowIDs;
static GtkWidget *pwShowPips;
static GtkWidget *apwDiceColour[ 2 ];
static GtkWidget *pwCubeColour;
static GtkWidget *apwDiceDotColour[ 2 ];
static GtkWidget *apwDieColour[ 2 ];
static GtkWidget *apwDiceColourBox[ 2 ];
static int fLabels, fUseDiceIcon, fPermitIllegal, fBeepIllegal,
    fHigherDieFirst, fWood, fHinges;
static int fShowIDs;
static int fShowPips;
static animation anim;
static int fUpdate;

static char *aszWoodName[] = {
    "alder", "ash", "basswood", "beech", "cedar", "ebony", "fir", "maple",
    "oak", "pine", "redwood", "walnut", "willow", "paint"
};


static void GetPrefs ( BoardData *bd );

static void
AddDesignRow ( gpointer data, gpointer user_data );


#if HAVE_LIBXML2

static GList *
ParseBoardDesigns ( const char *szFile, const int fDeletable );

typedef struct _boarddesign {

  gchar *szTitle;        /* Title of board design */
  gchar *szAuthor;       /* Name of author */
  gchar *szBoardDesign;  /* Command for setting board */

  int fDeletable;       /* is the board design deletable */

} boarddesign;


static boarddesign *pbdeSelected = NULL;

extern GList *
read_board_designs ( void ) {

  GList *plUser, *plSystem, *plFinal;
  gchar *sz;

  plSystem = ParseBoardDesigns ( "boards.xml", FALSE );

  sz = g_strdup_printf ( "%s/.gnubg/boards.xml", szHomeDirectory );
  plUser = ParseBoardDesigns ( sz, TRUE );
  g_free ( sz );

  plFinal = g_list_concat ( plSystem, plUser );

  return plFinal;

}

static void
free_board_design ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;

  if ( ! pbde )
    return;

  g_free ( pbde->szTitle );
  g_free ( pbde->szAuthor );
  g_free ( pbde->szBoardDesign );

  g_free ( data );

}

extern void
free_board_designs ( GList *pl ) {

  g_list_foreach ( pl, free_board_design, NULL );
  g_list_free ( pl );

}


#endif /* HAVE_LIBXML2 */

static void Preview( renderdata *prd ) {
    
    GdkGC *gc;
    renderdata rd;
    renderimages ri;
    unsigned char auch[ 108 * 3 * 72 * 3 * 3 ];
    int i, anBoard[ 2 ][ 25 ];
    int anDice[ 2 ] = { 4, 3 };
    int anDicePosition[ 2 ][ 2 ] = { { 70, 30 }, { 80, 32 } };
    int anCubePosition[ 2 ] = { 50, 32 };

    if( !fUpdate )
	return;
    
    memcpy( &rd, prd, sizeof rd );
    rd.nSize = 3;
    
    RenderImages( &rd, &ri );
  
    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ] = 0;
    
    anBoard[ 0 ][ 5 ] = anBoard[ 1 ][ 5 ] = 5;
    anBoard[ 0 ][ 7 ] = anBoard[ 1 ][ 7 ] = 3;
    anBoard[ 0 ][ 12 ] = anBoard[ 1 ][ 12 ] = 5;
    anBoard[ 0 ][ 23 ] = anBoard[ 1 ][ 23 ] = 2;
    
    CalculateArea( &rd, auch, 108 * 3 * 3, &ri, anBoard, NULL, anDice,
		   anDicePosition, 1, anCubePosition, 0, 0, 0, 0, 108 * 3,
		   72 * 3 );
    FreeImages( &ri );
  
    gc = gdk_gc_new( ppm );

    gdk_draw_rgb_image( ppm, gc, 0, 0, 108 * 3, 72 * 3, GDK_RGB_DITHER_MAX,
			auch, 108 * 3 * 3 );

    gdk_gc_unref( gc );
}

static void UpdatePreview( GtkWidget **ppw ) {

    BoardData bd;
  
    GetPrefs( &bd );
    Preview( &bd.rd );
    
    if( ppw )
	gtk_widget_queue_draw( *ppw );
}

static GtkWidget *ChequerPrefs( BoardData *bd, int f ) {

    GtkWidget *pw, *pwhbox;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    apadj[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new( bd->rd.arRefraction[ f ],
						     1.0, 3.5, 0.1, 1.0,
						     0.0 ) );
    apadjCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd.arCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd.arExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Colour:") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwColour[ f ] = gtk_colour_picker_new(), TRUE,
			TRUE, 4 );
    
    gtk_colour_picker_set_has_opacity_control(
	GTK_COLOUR_PICKER( apwColour[ f ] ), TRUE );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwColour[ f ] ),
				   bd->rd.aarColour[ f ] );
    gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( apwColour[ f ] ) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_CHEQUERS0 + f ) );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
    gtk_signal_connect_object( GTK_OBJECT( apadj[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_CHEQUERS0 + f ) );

    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
    gtk_signal_connect_object( GTK_OBJECT( apadjCoefficient[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_CHEQUERS0 + f ) );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Diffuse") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjExponent[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Specular") ), FALSE,
			FALSE, 4 );
    gtk_signal_connect_object( GTK_OBJECT( apadjExponent[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_CHEQUERS0 + f ) );
    
    return pwx;
}


static void
DieColourChanged ( GtkWidget *pw, GtkWidget *pwBox ) {

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
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    /* frame with colour selections for the dice */

    pwFrame = gtk_frame_new ( _("Die colour") );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwFrame, 
                         FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );

    apwDieColour[ f ] = 
      gtk_check_button_new_with_label ( _("Die colour same "
                                          "as chequer colour" ) );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDieColour[ f ], FALSE, FALSE, 0 );
    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColour[ f ] ),
                                   bd->rd.afDieColour[ f ] );


    apwDiceColourBox[ f ] = gtk_vbox_new ( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwvbox ),
			apwDiceColourBox[ f ], FALSE, FALSE, 0 );

    apadjDiceCoefficient[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd.arDiceCoefficient[ f ], 0.0, 1.0, 0.1, 0.1, 0.0 ) );
    apadjDiceExponent[ f ] = GTK_ADJUSTMENT( gtk_adjustment_new(
	bd->rd.arDiceExponent[ f ], 1.0, 100.0, 1.0, 10.0, 0.0 ) );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ),
			pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Die colour:") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwDiceColour[ f ] = gtk_colour_picker_new(), TRUE,
			TRUE, 4 );

    gtk_colour_picker_set_has_opacity_control(
	GTK_COLOUR_PICKER( apwDiceColour[ f ] ), FALSE );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwDiceColour[ f ] ),
				   bd->rd.aarDiceColour[ f ] );
    gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( apwDiceColour[ f ] ) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_DICE0 + f ) );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    /*
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			gtk_label_new( _("Refractive Index:") ), FALSE, FALSE,
			4 );
    gtk_box_pack_end( GTK_BOX( pwhbox ), gtk_hscale_new( apadj[ f ] ), TRUE,
		      TRUE, 4 );
    */

    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
                        pwhbox = gtk_hbox_new( FALSE, 0 ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Dull") ), FALSE,
			FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
	apadjDiceCoefficient[ f ] ), TRUE, TRUE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Shiny") ), FALSE,
			FALSE, 4 );
    gtk_signal_connect_object( GTK_OBJECT( apadjDiceCoefficient[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_DICE0 + f ) );
    
    gtk_box_pack_start( GTK_BOX( apwDiceColourBox[ f ] ), 
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
    gtk_signal_connect_object( GTK_OBJECT( apadjDiceExponent[ f ] ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_DICE0 + f ) );

    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ f ] ),
                               ! bd->rd.afDieColour[ f ] );

    /* colour of dot on dice */
    
    gtk_box_pack_start ( GTK_BOX( pw ),
			pwhbox = gtk_hbox_new( FALSE, 0 ), FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Pip colour:") ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwhbox ),
			apwDiceDotColour[ f ] = gtk_colour_picker_new(), TRUE,
			TRUE, 4 );
    
    gtk_colour_picker_set_has_opacity_control(
	GTK_COLOUR_PICKER( apwDiceDotColour[ f ] ), FALSE );
    gtk_colour_picker_set_colour( 
        GTK_COLOUR_PICKER( apwDiceDotColour[ f ] ),
        bd->rd.aarDiceDotColour[ f ] );
    gtk_signal_connect_object( GTK_OBJECT(
				   COLOUR_SEL( apwDiceDotColour[ f ] ) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_DICE0 + f ) );

    /* signals */

    gtk_signal_connect ( GTK_OBJECT( apwDieColour[ f ] ), "toggled",
                         GTK_SIGNAL_FUNC( DieColourChanged ), 
                         apwDiceColourBox[ f ] );
    gtk_signal_connect_object( GTK_OBJECT( apwDieColour[ f ] ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_DICE0 + f ) );

    return pwx;
}

static GtkWidget *CubePrefs( BoardData *bd ) {


    GtkWidget *pw;
    GtkWidget *pwx;

    pw = gtk_vbox_new( FALSE, 4 );
    
    pwx = gtk_hbox_new ( FALSE, 4 );
    
    gtk_box_pack_start ( GTK_BOX ( pw ), pwx, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX( pwx ), gtk_label_new( _("Cube colour:") ),
			FALSE, FALSE, 0 );
    
    gtk_box_pack_start ( GTK_BOX ( pwx ), 
                         pwCubeColour = gtk_colour_picker_new(),
			 TRUE, TRUE, 0 );
    gtk_colour_picker_set_has_opacity_control(
	GTK_COLOUR_PICKER( pwCubeColour ), FALSE );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( pwCubeColour ),
				   bd->rd.arCubeColour );
    gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( pwCubeColour ) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_CUBE ) );

    /* FIXME add cube text colour settings */
    
    return pw;

}

static GtkWidget *BoardPage( BoardData *bd ) {

    GtkWidget *pw, *pwhbox;
    gdouble ar[ 4 ];
    int i, j;
    GtkWidget *pwx;
    static char *asz[ 4 ] = {
	N_("Background colour:"),
	NULL,
	N_("First point colour:"),
	N_("Second point colour:"),
    };
    
    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

    for( j = 0; j < 4; j++ ) {
	if( j == 1 )
	    continue; /* colour 1 unused */
	apadjBoard[ j ] = GTK_ADJUSTMENT( gtk_adjustment_new(
            bd->rd.aSpeckle[ j ] / 128.0, 0, 1, 0.1, 0.1, 0 ) );

	for( i = 0; i < 3; i++ )
	    ar[ i ] = bd->rd.aanBoardColour[ j ][ i ] / 255.0;
    
	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 0 );
	gtk_box_pack_start( GTK_BOX( pwhbox ),
			    gtk_label_new( gettext( asz[ j ] ) ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ),
			    apwBoard[ j ] = gtk_colour_picker_new(), TRUE,
			    TRUE, 4 );
	gtk_colour_picker_set_has_opacity_control(
	    GTK_COLOUR_PICKER( apwBoard[ j ] ), FALSE );
	gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwBoard[ j ] ),
				      ar );
	gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( apwBoard[ j ] ) ),
				   "color-changed",
				   GTK_SIGNAL_FUNC( UpdatePreview ),
				   (GtkObject*) ( pwPreview + PI_BOARD ) );

	gtk_box_pack_start( GTK_BOX( pw ), pwhbox = gtk_hbox_new( FALSE, 0 ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Smooth") ),
			    FALSE, FALSE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_hscale_new(
				apadjBoard[ j ] ), TRUE, TRUE, 4 );
	gtk_box_pack_start( GTK_BOX( pwhbox ), gtk_label_new( _("Speckled") ),
			    FALSE, FALSE, 4 );
	gtk_signal_connect_object( GTK_OBJECT( apadjBoard[ j ] ),
				   "value-changed",
				   GTK_SIGNAL_FUNC( UpdatePreview ),
				   (GtkObject*) ( pwPreview + PI_BOARD ) );
    }
    
    return pwx;
}

static void ToggleWood( GtkWidget *pw, BoardData *bd ) {

    int fWood;
    
    fWood = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
    
    gtk_widget_set_sensitive( pwWoodType, fWood );
    gtk_widget_set_sensitive( apwBoard[ 1 ], !fWood );
}

static GtkWidget *BorderPage( BoardData *bd ) {

    GtkWidget *pw;
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
    woodtype bw;
    GtkWidget *pwx;

    pwx = gtk_hbox_new ( FALSE, 0 );

    pw = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start ( GTK_BOX ( pwx ), pw, TRUE, TRUE, 0 );

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
    if( bd->rd.wt != WOOD_PAINT )
	gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ),
				     bd->rd.wt );

#if GTK_CHECK_VERSION(2,0,0)
    gtk_signal_connect_object( GTK_OBJECT( pwWoodType ), "changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_BORDER ) );
#else
    gtk_signal_connect_object( GTK_OBJECT( pwWoodMenu ), "selection-done",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_BORDER ) );
#endif
    
    gtk_box_pack_start( GTK_BOX( pw ),
			pwWoodF = gtk_radio_button_new_with_label_from_widget(
			    GTK_RADIO_BUTTON( pwWood ), _("Painted") ),
			FALSE, FALSE, 0 );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd->rd.wt != WOOD_PAINT ?
						     pwWood : pwWoodF ),
				  TRUE );
    
    for( i = 0; i < 3; i++ )
	ar[ i ] = bd->rd.aanBoardColour[ 1 ][ i ] / 255.0;
    
    gtk_box_pack_start( GTK_BOX( pw ), apwBoard[ 1 ] =
			gtk_colour_picker_new(), FALSE, FALSE, 0 );
    gtk_colour_picker_set_has_opacity_control(
                        GTK_COLOUR_PICKER( apwBoard[ 1 ] ), FALSE );
    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwBoard[ 1 ] ),
				   ar );
    gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( apwBoard[ 1 ] ) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_BORDER ) );


    pwHinges = gtk_check_button_new_with_label( _("Show hinges") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwHinges ), fHinges );
    gtk_box_pack_start( GTK_BOX( pw ), pwHinges, FALSE, FALSE, 0 );
    gtk_signal_connect_object( GTK_OBJECT( pwHinges ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_BORDER ) );

    gtk_signal_connect( GTK_OBJECT( pwWood ), "toggled",
			GTK_SIGNAL_FUNC( ToggleWood ), bd );
    gtk_signal_connect_object( GTK_OBJECT( pwWood ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ),
			       (GtkObject*) ( pwPreview + PI_BORDER ) );
    
    gtk_widget_set_sensitive( pwWoodType, bd->rd.wt != WOOD_PAINT );
    gtk_widget_set_sensitive( apwBoard[ 1 ], bd->rd.wt == WOOD_PAINT);
    
    return pwx;
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

    pwLabels = gtk_check_button_new_with_label( _("Numbered point labels") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwLabels ), fLabels );
    gtk_box_pack_start( GTK_BOX( pw ), pwLabels, FALSE, FALSE, 0 );
    gtk_signal_connect_object( GTK_OBJECT( pwLabels ), "toggled",
			       GTK_SIGNAL_FUNC( UpdatePreview ), NULL );
    
    pwUseDiceIcon = 
      gtk_check_button_new_with_label( _("Show dice below board when human "
                                         "player on roll") );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwUseDiceIcon ),
				  fUseDiceIcon );
    gtk_box_pack_start( GTK_BOX( pw ), pwUseDiceIcon, FALSE, FALSE, 0 );
    
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

    rElevation = asinf( bd->rd.arLight[ 2 ] ) * 180 / M_PI;
    if ( fabs ( bd->rd.arLight[ 2 ] - 1.0f ) < 1e-5 ) 
      rAzimuth = 0.0;
    else
      rAzimuth = 
        acosf( bd->rd.arLight[ 0 ] / sqrt( 1.0 - bd->rd.arLight[ 2 ] *
                                        bd->rd.arLight[ 2 ] ) ) * 180 / M_PI;
    if( bd->rd.arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;
    
    paAzimuth = GTK_ADJUSTMENT( gtk_adjustment_new( rAzimuth, 0.0, 360.0, 1.0,
						    30.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_hscale_new( paAzimuth ),
		      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
    gtk_signal_connect_object( GTK_OBJECT( paAzimuth ), "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ), NULL );

    paElevation = GTK_ADJUSTMENT( gtk_adjustment_new( rElevation, 0.0, 90.0,
						      1.0, 10.0, 0.0 ) );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_hscale_new( paElevation ),
		      1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 4, 2 );
    gtk_signal_connect_object( GTK_OBJECT( paElevation ), "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ), NULL );
    
    pwBox = gtk_hbox_new( FALSE, 0 );
    
    padjRound = GTK_ADJUSTMENT( gtk_adjustment_new( 1.0 - bd->rd.rRound, 0, 1,
						    0.1, 0.1, 0 ) );
    gtk_signal_connect_object( GTK_OBJECT( padjRound ), "value-changed",
			       GTK_SIGNAL_FUNC( UpdatePreview ), NULL );
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
UseDesign ( void ) {

  BoardData bd;
  int i, j;
  gdouble ar[ 4 ];
  gfloat rAzimuth, rElevation;
  char *apch[ 2 ];
  gchar *sz, *pch;

  GetPrefs( &bd );

  pch = sz = g_strdup ( pbdeSelected->szBoardDesign );
  while( ParseKeyValue( &sz, apch ) ) 
    BoardPreferencesParam( &bd, apch[ 0 ], apch[ 1 ] );
  g_free ( pch );

  fUpdate = FALSE;

  /* chequers */

  for ( i = 0; i < 2; ++i ) {
    gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwColour[ i ] ),
                                    bd.rd.aarColour[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadj[ i ] ),
                               bd.rd.arRefraction[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjCoefficient[ i ] ),
                               bd.rd.arCoefficient[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjExponent[ i ] ),
                               bd.rd.arExponent[ i ] );


  }

  /* board, border, and points */

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pwHinges ), 
                                 bd.rd.fHinges );
  gtk_option_menu_set_history( GTK_OPTION_MENU( pwWoodType ), bd.rd.wt );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bd.rd.wt != WOOD_PAINT ?
                                                   pwWood : pwWoodF ),
                                TRUE );

  gtk_widget_set_sensitive( pwWoodType, bd.rd.wt != WOOD_PAINT );
  gtk_widget_set_sensitive( apwBoard[ 1 ], bd.rd.wt == WOOD_PAINT);

  /* board + points */
    
  for ( i = 0; i < 4; ++i ) {

    if ( i != 1 ) 
      gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjBoard[ i ] ),
                                 bd.rd.aSpeckle[ i ] / 128.0 );

    for ( j = 0; j < 3; j++ )
      ar[ j ] = bd.rd.aanBoardColour[ i ][ j ] / 255.0;

    gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwBoard[ i ]),
                                    ar );

  }


  /* dice + dice dot */

  for ( i = 0; i < 2; ++i ) {

    gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( apwDieColour[ i ] ),
                                   bd.rd.afDieColour[ i ] );
    gtk_widget_set_sensitive ( GTK_WIDGET ( apwDiceColourBox[ i ] ),
                               ! bd.rd.afDieColour[ i ] );

    gtk_colour_picker_set_colour( GTK_COLOUR_PICKER( apwDiceColour[ i ] ),
                                   bd.rd.afDieColour[ i ] ? 
                                   bd.rd.aarColour[ i ] :
                                   bd.rd.aarDiceColour[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceExponent[ i ] ),
                               bd.rd.afDieColour[ i ] ? 
                               bd.rd.arExponent[ i ] :
                               bd.rd.arDiceExponent[ i ] );

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( apadjDiceCoefficient[ i ] ),
                               bd.rd.afDieColour[ i ] ?
                               bd.rd.arCoefficient[ i ] :
                               bd.rd.arDiceCoefficient[ i ] );


    /* die dot */

    gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( apwDiceDotColour[ i ] ), 
                                    bd.rd.aarDiceDotColour[ i ] );

  }

  /* cube colour */
  
  gtk_colour_picker_set_colour ( GTK_COLOUR_PICKER ( pwCubeColour ), 
                                  bd.rd.arCubeColour );

  /* light */

  rElevation = asinf( bd.rd.arLight[ 2 ] ) * 180 / M_PI;
    if ( fabs ( bd.rd.arLight[ 2 ] - 1.0f ) < 1e-5 ) 
      rAzimuth = 0.0;
    else
      rAzimuth = 
        acosf( bd.rd.arLight[ 0 ] / sqrt( 1.0 - bd.rd.arLight[ 2 ] *
                                        bd.rd.arLight[ 2 ] ) ) * 180 / M_PI;
  if( bd.rd.arLight[ 1 ] < 0 )
    rAzimuth = 360 - rAzimuth;
    
  gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paAzimuth ),
                             rAzimuth );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT ( paElevation ),
                             rElevation );

  /* round */

  gtk_adjustment_set_value ( GTK_ADJUSTMENT ( padjRound ),
                             1.0f - bd.rd.rRound );

  fUpdate = TRUE;
}

static void
DesignActivate ( boarddesign *pbde ) {

  gchar *sz;
  BoardData bd;
  char *apch[ 2 ];
  gchar *pch;
  
  /* set title */
  gtk_label_set_text ( GTK_LABEL ( pwDesignTitle ),
                       pbde->szTitle );

  /* set author */
  sz = g_strdup_printf ( _("by %s" ), pbde->szAuthor );
  gtk_label_set_text ( GTK_LABEL ( pwDesignAuthor ), sz );
  g_free ( sz );

  /* set txt */

  gtk_label_set_text ( GTK_LABEL ( pwDesignDeletable ),
                       pbde->fDeletable ?
                       _("(user defined board design)" ) :
                       _("(predefined board design)" ) );
  
  /* set preview */
  GetPrefs( &bd );
  
  sz = pch = g_strdup ( pbde->szBoardDesign );
  while( ParseKeyValue( &sz, apch ) ) 
    BoardPreferencesParam( &bd, apch[ 0 ], apch[ 1 ] );
  g_free ( pch );

  Preview( &bd.rd );
  gtk_widget_queue_draw( pwPreview[ PI_DESIGN ] );

  UseDesign();
}


static void
WriteDesign ( gpointer data, gpointer user_data ) {

  boarddesign *pbde = data;
  FILE *pf = user_data;

  if ( ! pbde )
    return;

  if ( ! pbde->fDeletable )
    /* predefined board design */
    return;

  fputs ( "   <board-design>\n\n", pf );

  /* <about> */

  fprintf ( pf,
            "      <about>\n"
            "         <title>%s</title>\n"
            "         <author>%s</author>\n"
            "      </about>\n\n"
            "      <design>%s   </design>\n\n",
            pbde->szTitle ? pbde->szTitle : _("unknown title"),
            pbde->szAuthor ? pbde->szAuthor : _("unknown author"),
            pbde->szBoardDesign ? pbde->szBoardDesign : "" );

  fputs ( "   </board-design>\n\n\n", pf );

}



static void
DesignSave ( GtkWidget *pw, gpointer data ) {

  gchar *szFile;
  FILE *pf;
  GList **pplBoardDesigns = (GList **) data;
  time_t t;

  szFile = g_strdup_printf ( "%s/.gnubg/boards.xml", szHomeDirectory );
  BackupFile ( szFile );

  if ( ! ( pf = fopen ( szFile, "w+" ) ) ) {
    outputerr ( szFile );
    g_free ( szFile );
    return;
  }

  fputs ( "<?xml version=\"1.0\" encoding=\"" GNUBG_CHARSET "\"?>\n"
          "\n"
          "<!--\n"
          "\n"
          "    $HOME/.gnubg/boards.xml\n"
          "       generated by GNU Backgammon " VERSION "\n", pf );
  fputs ( "       ", pf );
  time ( &t );
  fputs ( ctime ( &t ), pf );
  fputs ( "\n"
          "    $Id$\n"
          "\n"
          " -->\n"
          "\n"
          "\n"
          "<board-designs>\n"
          "\n", pf );

  g_list_foreach ( *pplBoardDesigns, WriteDesign, pf );

  fputs ( "</board-designs>\n", pf );

  fclose ( pf );
  
}

static void DesignAddOK( GtkWidget *pw, boarddesign *pbde ) {

  pbde->szAuthor = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddAuthor ), 
                                           0, -1 );
  pbde->szTitle = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddTitle ), 
                                           0, -1 );
  
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}

static void
DesignAddChanged ( GtkWidget *pw, GtkWidget *pwDialog ) {

  char *szAuthor = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddAuthor ), 
                                           0, -1 );
  char *szTitle = gtk_editable_get_chars( GTK_EDITABLE( pwDesignAddTitle ), 
                                          0, -1 );

  GtkWidget *pwOK = DialogArea ( GTK_WIDGET ( pwDialog ), DA_OK );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwOK ), 
                             szAuthor && szTitle &&
                             *szAuthor && *szTitle );

}



static void
DesignAddTitle ( boarddesign *pbde ) {

  GtkWidget *pwDialog;
  GtkWidget *pwvbox;
  GtkWidget *pwhbox;

  pwDialog = CreateDialog( _("GNU Backgammon - Add current board design"), 
                           DT_QUESTION,
                           GTK_SIGNAL_FUNC( DesignAddOK ), pbde );

  pwvbox = gtk_vbox_new ( FALSE, 4 );
  gtk_container_add ( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), 
                      pwvbox );

  /* title */

  pwhbox = gtk_hbox_new ( FALSE, 4 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Title of new design:") ),
                       FALSE, FALSE, 4 );

  pwDesignAddTitle = gtk_entry_new ();
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddTitle, 
                       FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignAddTitle ), "changed",
                       GTK_SIGNAL_FUNC ( DesignAddChanged ), pwDialog );

  /* author */

  pwhbox = gtk_hbox_new ( FALSE, 4 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Author of new design:") ),
                       FALSE, FALSE, 4 );

  pwDesignAddAuthor = gtk_entry_new ();
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignAddAuthor, 
                       FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignAddAuthor ), "changed",
                       GTK_SIGNAL_FUNC ( DesignAddChanged ), pwDialog );

  /* show dialog */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  /*
  gtk_signal_connect_after( GTK_OBJECT( pwEntry ), "activate",
  GTK_SIGNAL_FUNC( StringOK ), &sz );*/
  
  gtk_widget_grab_focus( pwDesignAddTitle );
  gtk_widget_show_all( pwDialog );

  DesignAddChanged ( NULL, pwDialog );
  
  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}


static void
DesignAdd ( GtkWidget *pw, gpointer data ) {

  BoardData bd;
  float rElevation;
  float rAzimuth;
  boarddesign *pbde;
  GList **pplBoardDesigns = data;

  if ( ! ( pbde = (boarddesign *) malloc ( sizeof ( boarddesign ) ) ) ) {
    outputerr ( "allocate boarddesign" );
    return;
  }

  /* name and author of board */

  pbde->szTitle = pbde->szAuthor = pbde->szBoardDesign = NULL;

  DesignAddTitle ( pbde );
  if ( ! pbde->szTitle || ! pbde->szAuthor ) {
    free ( pbde );
    return;
  }

  /* get board design */

  GetPrefs ( &bd );

  rElevation = asinf( bd.rd.arLight[ 2 ] ) * 180 / M_PI;
  rAzimuth = ( fabs ( bd.rd.arLight[ 2 ] - 1.0f ) < 1e-5 ) ? 0.0f : 
    acosf( bd.rd.arLight[ 0 ] / sqrt( 1.0 - bd.rd.arLight[ 2 ] *
                                    bd.rd.arLight[ 2 ] ) ) * 180 / M_PI;
  if( bd.rd.arLight[ 1 ] < 0 )
    rAzimuth = 360 - rAzimuth;

  PushLocale( "C" );

  pbde->szBoardDesign = g_strdup_printf (
            "\n"
            "         board=#%02X%02X%02X;%0.2f\n"
            "         border=#%02X%02X%02X\n"
            "         wood=%s\n"
            "         hinges=%c\n"
            "         light=%0.0f;%0.0f\n"
            "         shape=%0.1f\n"
            "         chequers0=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f\n"
            "         chequers1=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f\n"
            "         dice0=#%02X%02X%02X;%0.2f;%0.2f;%c\n"
            "         dice1=#%02X%02X%02X;%0.2f;%0.2f;%c\n"
            "         dot0=#%02X%02X%02X\n"
            "         dot1=#%02X%02X%02X\n"
            "         cube=#%02X%02X%02X\n"
            "         points0=#%02X%02X%02X;%0.2f\n"
            "         points1=#%02X%02X%02X;%0.2f\n"
            "\n",
            /* board */
            bd.rd.aanBoardColour[ 0 ][ 0 ], bd.rd.aanBoardColour[ 0 ][ 1 ], 
            bd.rd.aanBoardColour[ 0 ][ 2 ], bd.rd.aSpeckle[ 0 ] / 128.0f,
            /* border */
            bd.rd.aanBoardColour[ 1 ][ 0 ], bd.rd.aanBoardColour[ 1 ][ 1 ], 
            bd.rd.aanBoardColour[ 1 ][ 2 ],
            /* wood ... */
            aszWoodName[ bd.rd.wt ],
            bd.rd.fHinges ? 'y' : 'n',
            rAzimuth, rElevation, 1.0 - bd.rd.rRound,
             /* chequers0 */
             (int) ( bd.rd.aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarColour[ 0 ][ 2 ] * 0xFF ), 
             bd.rd.aarColour[ 0 ][ 3 ], bd.rd.arRefraction[ 0 ], 
             bd.rd.arCoefficient[ 0 ], bd.rd.arExponent[ 0 ],
             /* chequers1 */
	     (int) ( bd.rd.aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarColour[ 1 ][ 2 ] * 0xFF ), 
             bd.rd.aarColour[ 1 ][ 3 ], bd.rd.arRefraction[ 1 ], 
             bd.rd.arCoefficient[ 1 ], bd.rd.arExponent[ 1 ],
             /* dice0 */
             (int) ( bd.rd.aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             bd.rd.arDiceCoefficient[ 0 ], bd.rd.arDiceExponent[ 0 ],
             bd.rd.afDieColour[ 0 ] ? 'y' : 'n',
             /* dice1 */
	     (int) ( bd.rd.aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             bd.rd.arDiceCoefficient[ 1 ], bd.rd.arDiceExponent[ 1 ],
             bd.rd.afDieColour[ 1 ] ? 'y' : 'n',
             /* dot0 */
	     (int) ( bd.rd.aarDiceDotColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarDiceDotColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarDiceDotColour[ 0 ][ 2 ] * 0xFF ), 
             /* dot1 */
	     (int) ( bd.rd.aarDiceDotColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd.rd.aarDiceDotColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd.rd.aarDiceDotColour[ 1 ][ 2 ] * 0xFF ), 
             /* cube */
	     (int) ( bd.rd.arCubeColour[ 0 ] * 0xFF ),
	     (int) ( bd.rd.arCubeColour[ 1 ] * 0xFF ), 
	     (int) ( bd.rd.arCubeColour[ 2 ] * 0xFF ), 
             /* points0 */
	     bd.rd.aanBoardColour[ 2 ][ 0 ], bd.rd.aanBoardColour[ 2 ][ 1 ], 
	     bd.rd.aanBoardColour[ 2 ][ 2 ], bd.rd.aSpeckle[ 2 ] / 128.0f,
             /* points1 */
	     bd.rd.aanBoardColour[ 3 ][ 0 ], bd.rd.aanBoardColour[ 3 ][ 1 ], 
	     bd.rd.aanBoardColour[ 3 ][ 2 ], bd.rd.aSpeckle[ 3 ] / 128.0f );

  PopLocale();

  pbde->fDeletable = TRUE;

  *pplBoardDesigns = g_list_append ( *pplBoardDesigns, (gpointer) pbde );

  AddDesignRow ( pbde, pwDesignList );
           

}


static void
RemoveDesign ( GtkWidget *pw, gpointer data ) {

  GList **pplBoardDesigns = (GList **) data;
  int i = gtk_clist_find_row_from_data ( GTK_CLIST ( pwDesignList ),
                                         pbdeSelected );

  *pplBoardDesigns = g_list_remove ( *pplBoardDesigns, pbdeSelected );

  gtk_clist_remove ( GTK_CLIST ( pwDesignList ), i );

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

  pbdeSelected = gtk_clist_get_row_data ( GTK_CLIST ( pwDesignList ),
                                          nRow );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), 
                             pbdeSelected->fDeletable );

  DesignActivate ( pbdeSelected );

}

static void DesignUnselect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, gpointer unused ) {

#if 0
  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );
#endif

  pbdeSelected = NULL;

}


static GtkWidget *
DesignPage ( GList **pplBoardDesigns, BoardData *bd ) {

  GtkWidget *pwvbox;
  GtkWidget *pwhbox;
  GtkWidget *pwFrame;
  GtkWidget *pwv;
  GtkWidget *pwScrolled;
  GtkWidget *pwPage;
  GtkWidget *pw;
  
  pwPage = gtk_hbox_new ( FALSE, 4 );

  /* CList with board designs */

  pwScrolled = gtk_scrolled_window_new( NULL, NULL );
  gtk_container_add ( GTK_CONTAINER ( pwPage ), pwScrolled );

  pwDesignList = gtk_clist_new( 1 );
  gtk_clist_set_column_auto_resize( GTK_CLIST( pwDesignList ), 0, TRUE );
  
  g_list_foreach ( *pplBoardDesigns, AddDesignRow, pwDesignList );
  gtk_container_add ( GTK_CONTAINER ( pwScrolled ), pwDesignList );

  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "select-row",
                      GTK_SIGNAL_FUNC( DesignSelect ), NULL );
  gtk_signal_connect( GTK_OBJECT( pwDesignList ), "unselect-row",
                      GTK_SIGNAL_FUNC( DesignUnselect ), NULL );


  /* preview page */

  pwvbox = gtk_vbox_new ( FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pwPage ), pwvbox, FALSE, FALSE, 0 );

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

  /* design author */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwv ), pwhbox, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       pwDesignDeletable = gtk_label_new ( NULL ),
                       FALSE, FALSE, 0 );

  /* design preview */
  gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                       pwPreview[ PI_DESIGN ] = gtk_pixmap_new( ppm, NULL ),
                       FALSE, FALSE, 0 );

  /* horisontal separator */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ),
                       gtk_hseparator_new (), FALSE, FALSE, 4 );

  /* button: use design */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwhbox, FALSE, FALSE, 4 );

  /* 
   * buttons 
   */

  /* remove design */

  pwDesignRemove = gtk_button_new_with_label ( _("Remove design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwDesignRemove, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pwDesignRemove ), "clicked",
                       GTK_SIGNAL_FUNC ( RemoveDesign ), pplBoardDesigns );

  /* add current design */

  pw = gtk_button_new_with_label ( _("Add current design") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                       GTK_SIGNAL_FUNC ( DesignAdd ), pplBoardDesigns );

  /* save designs */

  pw = gtk_button_new_with_label ( _("Save designs") );
  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                       GTK_SIGNAL_FUNC ( DesignSave ), pplBoardDesigns );


  gtk_widget_set_sensitive ( GTK_WIDGET ( pwDesignRemove ), FALSE );

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

static void GetPrefs ( BoardData *bd ) {

    int i, j;
    gdouble ar[ 4 ];
    
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
    bd->rd.rRound = 1.0 - padjRound->value;
    
    for( i = 0; i < 2; i++ ) {
	bd->rd.arRefraction[ i ] = apadj[ i ]->value;
	bd->rd.arCoefficient[ i ] = apadjCoefficient[ i ]->value;
	bd->rd.arExponent[ i ] = apadjExponent[ i ]->value;

        bd->rd.arDiceCoefficient[ i ] = apadjDiceCoefficient[ i ]->value;
	bd->rd.arDiceExponent[ i ] = apadjDiceExponent[ i ]->value;
    }
    
    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwColour[ 0 ] ), ar );
    for( i = 0; i < 4; i++ )
	bd->rd.aarColour[ 0 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwColour[ 1 ] ), ar );
    for( i = 0; i < 4; i++ )
	bd->rd.aarColour[ 1 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwDiceColour[ 0 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->rd.aarDiceColour[ 0 ][ i ] = ar[ i ];

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwDiceColour[ 1 ] ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->rd.aarDiceColour[ 1 ][ i ] = ar[ i ];

    for ( j = 0; j < 2; ++j ) {

      gtk_colour_picker_get_colour( 
           GTK_COLOUR_PICKER( apwDiceDotColour[ j ] ), 
           ar );
      for( i = 0; i < 3; i++ )
	bd->rd.aarDiceDotColour[ j ][ i ] = ar[ i ];
    }

    /* dice colour same as chequer colour */

    for ( i = 0; i < 2; ++i )
      bd->rd.afDieColour[ i ] = 
        gtk_toggle_button_get_active ( 
           GTK_TOGGLE_BUTTON ( apwDieColour[ i ] ) );

    /* cube colour */

    gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( pwCubeColour ), 
                                   ar );
    for( i = 0; i < 3; i++ )
	bd->rd.arCubeColour[ i ] = ar[ i ];

    /* board colour */

    for( j = 0; j < 4; j++ ) {
	gtk_colour_picker_get_colour( GTK_COLOUR_PICKER( apwBoard[ j ] ),
				      ar );
	for( i = 0; i < 3; i++ )
	    bd->rd.aanBoardColour[ j ][ i ] = ar[ i ] * 0xFF;
    }
    
    bd->rd.aSpeckle[ 0 ] = apadjBoard[ 0 ]->value * 0x80;
/*    bd->rd.aSpeckle[ 1 ] = apadjBoard[ 1 ]->value * 0x80; */
    bd->rd.aSpeckle[ 2 ] = apadjBoard[ 2 ]->value * 0x80;
    bd->rd.aSpeckle[ 3 ] = apadjBoard[ 3 ]->value * 0x80;
    
    bd->rd.arLight[ 2 ] = sinf( paElevation->value / 180 * M_PI );
    bd->rd.arLight[ 0 ] = cosf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->rd.arLight[ 2 ] * bd->rd.arLight[ 2 ] );
    bd->rd.arLight[ 1 ] = sinf( paAzimuth->value / 180 * M_PI ) *
	sqrt( 1.0 - bd->rd.arLight[ 2 ] * bd->rd.arLight[ 2 ] );

    bd->rd.fLabels = fLabels;
    bd->usedicearea = fUseDiceIcon;
    bd->show_ids = fShowIDs;
    bd->show_pips = fShowPips;
    bd->permit_illegal = fPermitIllegal;
    bd->beep_illegal = fBeepIllegal;
    bd->higher_die_first = fHigherDieFirst;
    bd->rd.wt = fWood ? gtk_option_menu_get_history( GTK_OPTION_MENU(
	pwWoodType ) ) : WOOD_PAINT;
    bd->rd.fHinges = fHinges;
    bd->animate_computer_moves = anim;
    bd->rd.fClockwise = fClockwise; /* yuck */
}

static void BoardPrefsOK( GtkWidget *pw, BoardData *bd ) {

    char sz[ 512 ];

    GetPrefs ( bd );

    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

    BoardPreferencesCommand( bd->widget, sz );

    UserCommand( sz );
}

static void
BoardPrefsDestroy ( GtkWidget *pw, GList **plBoardDesigns ) {

#if HAVE_LIBXML2
  free_board_designs ( *plBoardDesigns );
#endif /* HAVE_LIBXML2 */

}

static void append_preview_page( GtkWidget *pwNotebook, GtkWidget *pwPage,
				 char *szLabel, pixmapindex pi ) {

    GtkWidget *pw;

    pw = gtk_hbox_new( FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pw ), pwPage, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pw ),
			pwPreview[ pi ] = gtk_pixmap_new( ppm, NULL ), FALSE,
			FALSE, 0 );

    gtk_widget_show_all( pw );
    
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), pw,
			      gtk_label_new( szLabel ) );
}

extern void BoardPreferences( GtkWidget *pwBoard ) {

    int i;
    GtkWidget *pwDialog, *pwNotebook;
    BoardData *bd = BOARD( pwBoard )->board_data;
    GList *plBoardDesigns;
    
    fLabels = bd->rd.fLabels;
    fUseDiceIcon = bd->usedicearea;
    fShowIDs = bd->show_ids;
    fShowPips = bd->show_pips;
    fPermitIllegal = bd->permit_illegal;
    fBeepIllegal = bd->beep_illegal;
    fHigherDieFirst = bd->higher_die_first;
    fWood = bd->rd.wt;
    fHinges = bd->rd.fHinges;
    anim = bd->animate_computer_moves;
    
    pwDialog = CreateDialog( _("GNU Backgammon - Appearance"), DT_QUESTION,
			     GTK_SIGNAL_FUNC( BoardPrefsOK ), bd );

    ppm = gdk_pixmap_new( bd->drawing_area->window, 108 * 3, 72 * 3, -1 );
    fUpdate = TRUE;
    Preview( &bd->rd );
    fUpdate = FALSE;
    
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
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      DesignPage( &plBoardDesigns, bd ),
			      gtk_label_new(_("Designs") ) );
#endif /* HAVE_LIBXML2 */
    append_preview_page( pwNotebook, ChequerPrefs( bd, 0 ),
			 _("Chequers (0)"), PI_CHEQUERS0 );
    append_preview_page( pwNotebook, ChequerPrefs( bd, 1 ),
			 _("Chequers (1)"), PI_CHEQUERS1 );
    append_preview_page( pwNotebook, BoardPage( bd ), _("Board"),
			 PI_BOARD );
    append_preview_page( pwNotebook, BorderPage( bd ), _("Border"),
			 PI_BORDER );
    append_preview_page( pwNotebook, DicePrefs( bd, 0 ), _("Dice (0)"),
			 PI_DICE0 );
    append_preview_page( pwNotebook, DicePrefs( bd, 1 ), _("Dice (1)"),
			 PI_DICE1 );
    append_preview_page( pwNotebook, CubePrefs( bd ), _("Cube"), PI_CUBE );

    /* FIXME add settings for ambient light */
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
#if HAVE_LIBXML2
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( BoardPrefsDestroy ), 
                        &plBoardDesigns );
#endif /* HAVE_LIBXML2 */
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    gtk_widget_show_all( pwDialog );

    /* hack the set_opacity function does not work until widget has been
       drawn */

    for ( i = 0; i < 2; i++ ) {
      gtk_colour_picker_set_has_opacity_control(
         GTK_COLOUR_PICKER( apwBoard[ i ] ), FALSE );
      gtk_colour_picker_set_has_opacity_control(
         GTK_COLOUR_PICKER( apwBoard[ i + 2 ] ), FALSE );
      gtk_colour_picker_set_has_opacity_control(
         GTK_COLOUR_PICKER( apwDiceColour[ i ] ), FALSE );
      gtk_colour_picker_set_has_opacity_control(
         GTK_COLOUR_PICKER( apwDiceDotColour[ i ] ), FALSE );
    }
    gtk_colour_picker_set_has_opacity_control(
         GTK_COLOUR_PICKER( pwCubeColour ), FALSE );

    gdk_pixmap_unref( ppm );
    
    fUpdate = TRUE;

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
                         int afDieColour[ 2 ],
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
            /* die colour same as chequer colour */
            afDieColour[ i ] = ( toupper ( *pch ) == 'Y' );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
            afDieColour[ i ] = TRUE;

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

static int SetWood( char *sz, woodtype *pbw ) {

    woodtype bw;
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
	fValueError = SetColourSpeckle( szValue, bd->rd.aanBoardColour[ 0 ],
					&bd->rd.aSpeckle[ 0 ] );
    else if( !g_strncasecmp( szParam, "border", c ) )
	/* border=colour */
	fValueError = SetColour( szValue, bd->rd.aanBoardColour[ 1 ] );
    else if( !g_strncasecmp( szParam, "cube", c ) )
	/* cube=colour */
        fValueError = SetColourX( bd->rd.arCubeColour, szValue );
    else if( !g_strncasecmp( szParam, "translucent", c ) )
	/* deprecated option "translucent"; ignore */
	;
    else if( !g_strncasecmp( szParam, "labels", c ) )
	/* labels=bool */
	bd->rd.fLabels = toupper( *szValue ) == 'Y';
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
	fValueError = SetWood( szValue, &bd->rd.wt );
    else if( !g_strncasecmp( szParam, "hinges", c ) )
	bd->rd.fHinges = toupper( *szValue ) == 'Y';
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
	    
	    bd->rd.arLight[ 2 ] = sinf( rElevation / 180 * M_PI );
	    bd->rd.arLight[ 0 ] = cosf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - bd->rd.arLight[ 2 ] * bd->rd.arLight[ 2 ] );
	    bd->rd.arLight[ 1 ] = sinf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - bd->rd.arLight[ 2 ] * bd->rd.arLight[ 2 ] );
	}
        PopLocale ();
    } else if( !g_strncasecmp( szParam, "shape", c ) ) {
	float rRound;

	PushLocale ( "C" );
	if( sscanf( szValue, "%f", &rRound ) < 1 )
	    fValueError = TRUE;
	else
	    bd->rd.rRound = 1.0 - rRound;
        PopLocale ();
    } else if( c > 1 &&
	       ( !g_strncasecmp( szParam, "chequers", c - 1 ) ||
		 !g_strncasecmp( szParam, "checkers", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* chequers=colour;alpha;refrac;shine;spec */
	fValueError = SetColourARSS( bd->rd.aarColour, 
                                     bd->rd.arRefraction,
                                     bd->rd.arCoefficient,
                                     bd->rd.arExponent,
                                     szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !g_strncasecmp( szParam, "dice", c - 1 ) ||
               !g_strncasecmp( szParam, "dice", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* dice=colour;shine;spec;flag */
        fValueError = SetColourSSF( bd->rd.aarDiceColour, 
                                    bd->rd.arDiceCoefficient,
                                    bd->rd.arDiceExponent,
                                    bd->rd.afDieColour,
                                    szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !g_strncasecmp( szParam, "dot", c - 1 ) ||
               !g_strncasecmp( szParam, "dot", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
        /* dot=colour */
	fValueError = 
          SetColourX( bd->rd.aarDiceDotColour [ szParam[ c - 1 ] - '0' ],
                      szValue );
    else if( c > 1 && !g_strncasecmp( szParam, "points", c - 1 ) &&
	     ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* pointsn=colour;speckle */
	fValueError = SetColourSpeckle( szValue,
					bd->rd.aanBoardColour[
					    szParam[ c - 1 ] - '0' + 2 ],
					&bd->rd.aSpeckle[
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
    rElevation = asinf( bd->rd.arLight[ 2 ] ) * 180 / M_PI;
    rAzimuth = ( fabs ( bd->rd.arLight[ 2 ] - 1.0f ) < 1e-5 ) ? 0.0f : 
      acosf( bd->rd.arLight[ 0 ] / sqrt( 1.0 - bd->rd.arLight[ 2 ] *
                                      bd->rd.arLight[ 2 ] ) ) * 180 / M_PI;

    if( bd->rd.arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;

    PushLocale ( "C" );
    
    sprintf( sz, 
             "set appearance board=#%02X%02X%02X;%0.2f "
	     "border=#%02X%02X%02X "
	     "labels=%c diceicon=%c illegal=%c "
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
	     bd->rd.aanBoardColour[ 0 ][ 0 ], bd->rd.aanBoardColour[ 0 ][ 1 ], 
	     bd->rd.aanBoardColour[ 0 ][ 2 ], bd->rd.aSpeckle[ 0 ] / 128.0f,
             /* border */
	     bd->rd.aanBoardColour[ 1 ][ 0 ], bd->rd.aanBoardColour[ 1 ][ 1 ], 
	     bd->rd.aanBoardColour[ 1 ][ 2 ],
             /* labels ... */
             bd->rd.fLabels ? 'y' : 'n',
	     bd->usedicearea ? 'y' : 'n', bd->permit_illegal ? 'y' : 'n',
             /* beep, highdie, .... */
	     bd->beep_illegal ? 'y' : 'n', bd->higher_die_first ? 'y' : 'n',
	     aszWoodName[ bd->rd.wt ],
	     bd->rd.fHinges ? 'y' : 'n',
	     bd->show_ids ? 'y' : 'n',
             bd->show_pips ? 'y' : 'n',
             /* animate, speed, ... */
	     aszAnim[ bd->animate_computer_moves ], bd->animate_speed,
	     rAzimuth, rElevation, 1.0 - bd->rd.rRound,
             /* chequers0 */
             (int) ( bd->rd.aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarColour[ 0 ][ 2 ] * 0xFF ), 
             bd->rd.aarColour[ 0 ][ 3 ], bd->rd.arRefraction[ 0 ], 
             bd->rd.arCoefficient[ 0 ], bd->rd.arExponent[ 0 ],
             /* chequers1 */
	     (int) ( bd->rd.aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarColour[ 1 ][ 2 ] * 0xFF ), 
             bd->rd.aarColour[ 1 ][ 3 ], bd->rd.arRefraction[ 1 ], 
             bd->rd.arCoefficient[ 1 ], bd->rd.arExponent[ 1 ],
             /* dice0 */
             (int) ( bd->rd.aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             bd->rd.arDiceCoefficient[ 0 ], bd->rd.arDiceExponent[ 0 ],
             bd->rd.afDieColour[ 0 ] ? 'y' : 'n',
             /* dice1 */
	     (int) ( bd->rd.aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             bd->rd.arDiceCoefficient[ 1 ], bd->rd.arDiceExponent[ 1 ],
             bd->rd.afDieColour[ 1 ] ? 'y' : 'n',
             /* dot0 */
	     (int) ( bd->rd.aarDiceDotColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarDiceDotColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarDiceDotColour[ 0 ][ 2 ] * 0xFF ), 
             /* dot1 */
	     (int) ( bd->rd.aarDiceDotColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( bd->rd.aarDiceDotColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( bd->rd.aarDiceDotColour[ 1 ][ 2 ] * 0xFF ), 
             /* cube */
	     (int) ( bd->rd.arCubeColour[ 0 ] * 0xFF ),
	     (int) ( bd->rd.arCubeColour[ 1 ] * 0xFF ), 
	     (int) ( bd->rd.arCubeColour[ 2 ] * 0xFF ), 
             /* points0 */
	     bd->rd.aanBoardColour[ 2 ][ 0 ], bd->rd.aanBoardColour[ 2 ][ 1 ], 
	     bd->rd.aanBoardColour[ 2 ][ 2 ], bd->rd.aSpeckle[ 2 ] / 128.0f,
             /* points1 */
	     bd->rd.aanBoardColour[ 3 ][ 0 ], bd->rd.aanBoardColour[ 3 ][ 1 ], 
	     bd->rd.aanBoardColour[ 3 ][ 2 ], bd->rd.aSpeckle[ 3 ] / 128.0f );

    PopLocale ();

    return sz;
}

#if HAVE_LIBXML2

typedef enum _parsestate {
  STATE_NONE,
  STATE_BOARD_DESIGNS,
  STATE_BOARD_DESIGN,
  STATE_ABOUT,
  STATE_TITLE, STATE_AUTHOR,
  STATE_DESIGN, 
} parsestate;

typedef struct _parsecontext {

  GList *pl;  /* list of board designs */

  parsestate aps[ 10 ];      /* the current tag */
  int ips;                   /* depth of stack */

  boarddesign *pbde;
  int i, j;      /* counters */
  int err;     /* return code */
  int fDeletable; /* is the board design deletable */

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
    ppc->pbde->fDeletable = ppc->fDeletable;
    ppc->pl = g_list_append ( ppc->pl, (gpointer) ppc->pbde );

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
ParseBoardDesigns ( const char *szFile, const int fDeletable ) {

  xmlParserCtxt *pxpc;
  parsecontext pc;
  char *pch;

  pc.ips = -1;
  pc.pl = NULL;
  pc.err = FALSE;
  pc.fDeletable = fDeletable;

  /* create parser context */

  if ( ! ( pch = PathSearch ( szFile, szDataDirectory ) ) )
    return NULL;
  if( access( pch, R_OK ) )
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
