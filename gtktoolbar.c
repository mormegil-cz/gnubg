/*
 * gtkbearoff.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
 *
 * Based on Sho Sengoku's Equity Temperature Map
 * http://www46.pair.com/sengoku/TempMap/English/TempMap.html
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

#define GTK_ENABLE_BROKEN /* for GtkText */
#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "backgammon.h"
#include "gtktoolbar.h"
#include "gtkboard.h"
#include "gtk-multiview.h"
#include "i18n.h"
#include "drawboard.h"


typedef struct _toolbarwidget {

  GtkWidget *pwRoll;       /* button for "Roll" */
  GtkWidget *pwDouble;     /* button for "Double" */
  GtkWidget *pwRedouble;   /* button for "Redouble" */
  GtkWidget *pwTake;       /* button for "Take" */
  GtkWidget *pwDrop;       /* button for "Drop" */
  GtkWidget *pwPlay;       /* button for "Play" */
  GtkWidget *pwReset;      /* button for "Reset" */
  GtkWidget *pwStop;       /* button for "Stop" */
  GtkWidget *pwStopParent; /* parent for pwStop */
  GtkWidget *pwEdit;       /* button for "Edit" */
  GtkWidget *pwButtonClockwise; /* button for clockwise */

} toolbarwidget;


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
  

static GtkWidget *
toggle_button_from_images( GtkWidget *pwImageOff,
                           GtkWidget *pwImageOn ) {

  GtkWidget **aapw;
  GtkWidget *pwm = gtk_multiview_new();
  GtkWidget *pw = gtk_toggle_button_new();

  aapw = (GtkWidget **) g_malloc( 3 * sizeof ( GtkWidget *) );

  aapw[ 0 ] = pwImageOff;
  aapw[ 1 ] = pwImageOn;
  aapw[ 2 ] = pwm;

  gtk_container_add( GTK_CONTAINER( pw ), pwm );

  gtk_container_add( GTK_CONTAINER( pwm ), pwImageOff );
  gtk_container_add( GTK_CONTAINER( pwm ), pwImageOn );
  
  gtk_object_set_data_full( GTK_OBJECT( pw ), "user_data", aapw, g_free );

  return pw;
  
}

extern void
ToolbarSetPlaying( GtkWidget *pwToolbar, const int f ) {

  toolbarwidget *ptw = gtk_object_get_user_data ( GTK_OBJECT ( pwToolbar ) );
  
  gtk_widget_set_sensitive( ptw->pwReset, f );

}


static void
ToolbarToggleClockwise( GtkWidget *pw, toolbarwidget *ptw ) {

  GtkWidget **aapw = 
    (GtkWidget **) gtk_object_get_user_data( GTK_OBJECT( pw ) );
  int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
  
  gtk_multiview_set_current( GTK_MULTIVIEW( aapw[ 2 ] ), aapw[ f ] );

  if ( fClockwise != f ) {
    gchar *sz = g_strdup_printf( "set clockwise %s", f ? "on" : "off" );
    UserCommand( sz );
    g_free( sz );
  }

}


static void
ToolbarToggleEdit( GtkWidget *pw, toolbarwidget *ptw ) {

  BoardData *pbd = BOARD( pwBoard )->board_data;

  board_edit( pbd );

}


static void 
ToolbarStop( GtkWidget *pw, gpointer unused ) {

    fInterrupt = TRUE;
}


extern GtkWidget *
ToolbarGetStopParent ( GtkWidget *pwToolbar ) {

  toolbarwidget *ptw = gtk_object_get_user_data ( GTK_OBJECT ( pwToolbar ) );

  assert ( ptw );

  return ptw->pwStopParent;

}

extern int
ToolbarIsEditing( GtkWidget *pwToolbar ) {

  toolbarwidget *ptw = gtk_object_get_user_data ( GTK_OBJECT ( pwToolbar ) );

  return gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ptw->pwEdit ) );

}


extern toolbarcontrol
ToolbarUpdate ( GtkWidget *pwToolbar,
                const matchstate *pms,
                const int anDice[ 2 ],
                const int fComputerTurn,
                const int fPlaying ) {

  toolbarwidget *ptw = gtk_object_get_user_data ( GTK_OBJECT ( pwToolbar ) );
  toolbarcontrol c;
  

  assert ( ptw );

  c = C_NONE;

  if( ! anDice[ 0 ] )
    c = C_ROLLDOUBLE;

  if( pms->fDoubled )
    c = C_TAKEDROP;

  if( pms->fResigned )
    c = C_AGREEDECLINE;

  if( fComputerTurn )
    c = C_PLAY;

  if( ToolbarIsEditing( pwToolbar ) || ! fPlaying )
    c = C_NONE;

  gtk_widget_set_sensitive ( ptw->pwRoll, c == C_ROLLDOUBLE );
  gtk_widget_set_sensitive ( ptw->pwDouble, c == C_ROLLDOUBLE );

  gtk_widget_set_sensitive ( ptw->pwPlay, c == C_PLAY );

  gtk_widget_set_sensitive ( ptw->pwTake,
                             c == C_TAKEDROP || c == C_AGREEDECLINE );
  gtk_widget_set_sensitive ( ptw->pwDrop,
                             c == C_TAKEDROP || c == C_AGREEDECLINE );
  /* FIXME: max beavers etc */
  gtk_widget_set_sensitive ( ptw->pwRedouble,
                             c == C_TAKEDROP && ! pms->nMatchTo );

  return c;


}





extern GtkWidget *
ToolbarNew ( void ) {

  GtkWidget *vbox_toolbar;
  GtkWidget *pwToolbar;

  toolbarwidget *ptw;

#include "xpm/tb_roll.xpm"
#include "xpm/tb_double.xpm"
#include "xpm/tb_redouble.xpm"
#include "xpm/tb_edit.xpm"
#include "xpm/tb_anticlockwise.xpm"
#include "xpm/tb_clockwise.xpm"
#ifndef USE_GTK2
#include "xpm/tb_no.xpm"
#include "xpm/tb_yes.xpm"
#include "xpm/tb_stop.xpm"
#include "xpm/tb_undo.xpm"
#endif
    
  /* 
   * Create tool bar 
   */
  
  ptw = (toolbarwidget *) g_malloc ( sizeof ( toolbarwidget ) );
  
  vbox_toolbar = gtk_vbox_new ( FALSE, 0 );
  
  gtk_object_set_data_full ( GTK_OBJECT ( vbox_toolbar ), "user_data",
                             ptw, g_free );
  
#if USE_GTK2
  pwToolbar = gtk_toolbar_new ();
  gtk_toolbar_set_orientation ( GTK_TOOLBAR ( pwToolbar ),
                                GTK_ORIENTATION_HORIZONTAL );
  gtk_toolbar_set_style ( GTK_TOOLBAR ( pwToolbar ),
                          GTK_TOOLBAR_ICONS );
#else
  pwToolbar = gtk_toolbar_new ( GTK_ORIENTATION_HORIZONTAL,
                                GTK_TOOLBAR_ICONS );
#endif /* ! USE_GTK2 */


  gtk_box_pack_start( GTK_BOX( vbox_toolbar ), pwToolbar, 
                      FALSE, FALSE, 0 );

  gtk_toolbar_set_tooltips ( GTK_TOOLBAR ( pwToolbar ), TRUE );

  /* roll button */

  ptw->pwRoll = button_from_image( image_from_xpm_d ( tb_roll_xpm,
                                                      pwToolbar ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwRoll ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClicked ), "roll" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwRoll ), FALSE );
  
  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwRoll,
                              _("Roll dice"), NULL );

    /* double button */

  ptw->pwDouble = button_from_image( image_from_xpm_d ( tb_double_xpm,
                                                        pwToolbar ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwDouble ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClicked ), "double" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwDouble ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwDouble, _("Double"), NULL );

  /* take button */

  gtk_toolbar_append_space ( GTK_TOOLBAR ( pwToolbar ) );

#if USE_GTK2
  ptw->pwTake =
    button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_YES, 
                                                   GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
  ptw->pwTake =
    button_from_image ( image_from_xpm_d ( tb_yes_xpm,
                                           pwToolbar ) );
#endif

  gtk_signal_connect( GTK_OBJECT( ptw->pwTake ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClickedYesNo ), "yes" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwTake ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwTake,
                              _("Take the offered cube or "
                                "accept the offered resignation"), NULL );

  /* drop button */

#if USE_GTK2
  ptw->pwDrop =
    button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_NO, 
                                                   GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
  ptw->pwDrop =
    button_from_image ( image_from_xpm_d ( tb_no_xpm,
                                           pwToolbar ) );
#endif

  gtk_signal_connect( GTK_OBJECT( ptw->pwDrop ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClickedYesNo ), "no" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwDrop ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwDrop,
                              _("Drop the offered cube or "
                                "decline offered resignation"), NULL );

  /* ptw->pwRedouble button */

  ptw->pwRedouble = button_from_image( image_from_xpm_d ( tb_redouble_xpm,
                                                          pwToolbar ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwRedouble ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClicked ), "redouble" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwRedouble ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwRedouble,
                              _("Redouble immediately (beaver)"), NULL );

  /* play button */

  gtk_toolbar_append_space ( GTK_TOOLBAR ( pwToolbar ) );

#if USE_GTK2
  ptw->pwPlay = 
    button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_EXECUTE, 
                                                   GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
  ptw->pwPlay = gtk_button_new_with_label ( _("Play" ) );
#endif
  gtk_signal_connect( GTK_OBJECT( ptw->pwPlay ), "clicked",
                      GTK_SIGNAL_FUNC( ButtonClicked ), "play" );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwPlay ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwPlay,
                              _("Force computer to play"), NULL );

  /* reset button */

  gtk_toolbar_append_space ( GTK_TOOLBAR ( pwToolbar ) );

#if USE_GTK2
  ptw->pwReset = 
    button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_UNDO, 
                                                   GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
  ptw->pwReset = 
    button_from_image ( image_from_xpm_d ( tb_undo_xpm,
                                           pwToolbar ) );
#endif
  gtk_signal_connect( GTK_OBJECT( ptw->pwReset ), "clicked",
                      GTK_SIGNAL_FUNC( ShowBoard ), NULL );
  gtk_widget_set_sensitive ( GTK_WIDGET ( ptw->pwReset ), FALSE );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwReset,
                              _("Undo moves"), NULL );

  /* edit button */

  gtk_toolbar_append_space ( GTK_TOOLBAR ( pwToolbar ) );

  ptw->pwEdit =
    toggle_button_from_image ( image_from_xpm_d ( tb_edit_xpm,
                                                  pwToolbar ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwEdit ), "toggled",
                      GTK_SIGNAL_FUNC( ToolbarToggleEdit ), ptw );
    
  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwEdit,
                              _("Edit position"), NULL );

  /* direction of play */

  ptw->pwButtonClockwise =
    toggle_button_from_images( image_from_xpm_d( tb_anticlockwise_xpm,
                                                 pwToolbar ),
                               image_from_xpm_d( tb_clockwise_xpm,
                                                 pwToolbar ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwButtonClockwise ), "toggled",
                      GTK_SIGNAL_FUNC( ToolbarToggleClockwise ), ptw );

  gtk_toolbar_append_widget( GTK_TOOLBAR( pwToolbar ),
                             ptw->pwButtonClockwise,
                             _("Reverse direction of play"),
                             NULL );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ptw->pwButtonClockwise ),
                                fClockwise );
                                 

  /* stop button */

  gtk_toolbar_append_space ( GTK_TOOLBAR ( pwToolbar ) );

  ptw->pwStopParent = gtk_event_box_new();
#if USE_GTK2
  ptw->pwStop =
    button_from_image ( gtk_image_new_from_stock ( GTK_STOCK_STOP, 
                                                   GTK_ICON_SIZE_SMALL_TOOLBAR ) );
#else
  ptw->pwStop =
    button_from_image ( image_from_xpm_d ( tb_stop_xpm,
                                           pwToolbar ) );
#endif
  gtk_container_add( GTK_CONTAINER( ptw->pwStopParent ), ptw->pwStop );

  gtk_signal_connect( GTK_OBJECT( ptw->pwStop ), "clicked",
                      GTK_SIGNAL_FUNC( ToolbarStop ), NULL );

  gtk_toolbar_append_widget ( GTK_TOOLBAR ( pwToolbar ),
                              ptw->pwStopParent,
                              _("Stop the current operation"), NULL );


  return vbox_toolbar;

}
