/*
 * movefilter.c
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#define GTK_ENABLE_BROKEN /* for GtkText */
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "eval.h"
#include "gtkgame.h"
#include "i18n.h"
#include "matchequity.h"
#include "gtkmovefilter.h"

typedef struct _movefilterwidget {


  movefilter *pmf;

  /* the menu with default settings */
  
  GtkWidget *pwOptionMenu;

} movefilterwidget;


typedef struct _movefiltersetupwidget {

  int *pfOK;
  movefilter *pmf;

  GtkAdjustment *aapadjAccept[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  GtkAdjustment *aapadjExtra[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  GtkAdjustment *aapadjThreshold[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  GtkWidget *aapwET[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  GtkWidget *aapwT[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  GtkWidget *pwOptionMenu;

} movefiltersetupwidget;


static void
MoveFilterSetupSetValues ( movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
                           movefiltersetupwidget *pmfsw ) {

  int i, j;

  for ( i = 0; i < MAX_FILTER_PLIES; ++i ) 
    for ( j = 0; j <=i; ++j ) {

      gtk_adjustment_set_value ( pmfsw->aapadjAccept[ i ][ j ], 
                                 aamf[ i ][ j ].Accept );
      gtk_adjustment_set_value ( pmfsw->aapadjExtra[ i ][ j ], 
                                 aamf[ i ][ j ].Accept ?
                                 aamf[ i ][ j ].Extra : 0 );
      gtk_adjustment_set_value ( pmfsw->aapadjThreshold[ i ][ j ], 
                                 aamf[ i ][ j ].Extra ?
                                 aamf[ i ][ j ].Threshold : 0 );

    }

}



static void
MoveFilterSetupGetValues ( movefilter *pmf, movefiltersetupwidget *pmfsw ) {

  int i, j;
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];

  memset ( aamf, 0, sizeof ( aamf ) );

  for ( i = 0; i < MAX_FILTER_PLIES; ++i ) 
    for ( j = 0; j <=i; ++j ) 
      if ( ( aamf[ i ][ j ].Accept = pmfsw->aapadjAccept[ i ][ j ]->value ) )
        if ( ( aamf[ i ][ j ].Extra = pmfsw->aapadjExtra[ i ][ j ]->value ) )
          aamf[ i ][ j ].Threshold = pmfsw->aapadjThreshold[ i ][ j ]->value;

  memcpy ( pmf, aamf, sizeof ( aamf ) );

}

static int
equal_movefilter ( const int i, 
                   movefilter amf1[ MAX_FILTER_PLIES ],
                   movefilter amf2[ MAX_FILTER_PLIES ] ) {

  int j;

  for ( j = 0; j <= i; ++j ) {
    if ( amf1[ j ].Accept != amf2[ j ].Accept )
      return 0;
    if ( ! amf1[ j ].Accept )
      continue;
    if ( amf1[ j ].Extra != amf2[ j ].Extra )
      return 0;
    if ( ! amf1[ j ].Extra )
      continue;
    if ( amf1[ j ].Threshold != amf2[ j ].Threshold )
      return 0;
    
  }

  return 1;

}


static int
equal_movefilters ( movefilter aamf1[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
                    movefilter aamf2[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int i, j;

  for ( i = 0; i < MAX_FILTER_PLIES; ++i )
    if ( ! equal_movefilter ( i, aamf1[ i ], aamf2[ i ] ) )
      return 0;
      
  return 1;

}


static void
AcceptChanged ( GtkWidget *pw, movefiltersetupwidget *pmfsw ) {

  int fFound;
  int i, j;
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];

  for ( i = 0; i < MAX_FILTER_PLIES; ++i ) 
    for ( j = 0; j <= i; ++j ) {
      gtk_widget_set_sensitive ( GTK_WIDGET ( pmfsw->aapwET[ i ][ j ] ),
                                 pmfsw->aapadjAccept[ i ][ j ]->value );
      gtk_widget_set_sensitive ( GTK_WIDGET ( pmfsw->aapwT[ i ][ j ] ),
                                 pmfsw->aapadjExtra[ i ][ j ]->value );
  }

  /* see if current settings match a predefined one */

  fFound = FALSE;

  
  MoveFilterSetupGetValues ( (movefilter *) aamf, pmfsw );

  for ( i = 0; i < NUM_MOVEFILTER_SETTINGS; ++i ) 
    if ( equal_movefilters ( aamf, 
                             aaamfMoveFilterSettings[ i ] ) ) {
      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pmfsw->pwOptionMenu ),
                                    i );
      fFound = TRUE;
      break;
    }

  if ( ! fFound )
      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pmfsw->pwOptionMenu ),
                                    NUM_MOVEFILTER_SETTINGS );

}


static void
SetupSettingsMenuActivate ( GtkWidget *pwItem,
                            movefiltersetupwidget *pfmsw ) {

  int *piSelected;
  
  piSelected = gtk_object_get_data ( GTK_OBJECT ( pwItem ), "user_data" );

  if ( *piSelected == NUM_MOVEFILTER_SETTINGS )
    return; /* user defined */

  MoveFilterSetupSetValues ( aaamfMoveFilterSettings[ *piSelected ],
                             pfmsw );

}




static GtkWidget *
MoveFilterPage ( const int i, const int j,
                 movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
                 movefiltersetupwidget *pmfsw ) {

  GtkWidget *pwPage;
  GtkWidget *pwhbox;
  GtkWidget *pw;

  pwPage = gtk_vbox_new ( FALSE, 0 );

  /* accept */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwPage ), pwhbox, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Always accept: ") ),
                       FALSE, FALSE, 0 );

  pmfsw->aapadjAccept[ i ][ j ] = 
    GTK_ADJUSTMENT ( gtk_adjustment_new ( 0, 0, 1000,
                                          1, 5, 5 ) );

  pw = gtk_spin_button_new ( GTK_ADJUSTMENT ( pmfsw->aapadjAccept[ i ][ j ] ), 
                             1, 0 );

  gtk_spin_button_set_numeric ( GTK_SPIN_BUTTON ( pw ), TRUE );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("moves.") ),
                       FALSE, FALSE, 0 );

  gtk_signal_connect( GTK_OBJECT( pmfsw->aapadjAccept[ i ][ j ] ), 
                      "value-changed",
                      GTK_SIGNAL_FUNC( AcceptChanged ), pmfsw );

  /* extra */

  pwhbox = gtk_hbox_new ( FALSE, 0 );
  pmfsw->aapwET[ i ][ j ] = pwhbox;
  gtk_box_pack_start ( GTK_BOX ( pwPage ), pwhbox, FALSE, FALSE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("Add extra: ") ),
                       FALSE, FALSE, 0 );

  pmfsw->aapadjExtra[ i ][ j ] = 
    GTK_ADJUSTMENT ( gtk_adjustment_new ( 0, 0, 1000,
                                          1, 5, 5 ) );

  pw = gtk_spin_button_new ( GTK_ADJUSTMENT ( pmfsw->aapadjExtra[ i ][ j ] ), 
                             1, 0 );

  gtk_spin_button_set_numeric ( GTK_SPIN_BUTTON ( pw ), TRUE );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, FALSE, FALSE, 0 );

  gtk_signal_connect( GTK_OBJECT( pmfsw->aapadjExtra[ i ][ j ] ), 
                      "value-changed",
                      GTK_SIGNAL_FUNC( AcceptChanged ), pmfsw );

  /* threshold */

  gtk_box_pack_start ( GTK_BOX ( pwhbox ),
                       gtk_label_new ( _("moves within") ),
                       FALSE, FALSE, 0 );

  pmfsw->aapadjThreshold[ i ][ j ] = 
    GTK_ADJUSTMENT ( gtk_adjustment_new ( 0, 0, 10,
                                          0.001, 0.1, 0.1 ) );

  pw = 
    gtk_spin_button_new ( GTK_ADJUSTMENT ( pmfsw->aapadjThreshold[ i ][ j ] ), 
                          1, 3 );
  pmfsw->aapwT[ i ][ j ] = pw;

  gtk_spin_button_set_numeric ( GTK_SPIN_BUTTON ( pw ), TRUE );

  gtk_box_pack_start ( GTK_BOX ( pwhbox ), pw, TRUE, TRUE, 0 );

  gtk_signal_connect( GTK_OBJECT( pmfsw->aapadjThreshold[ i ][ j ] ), 
                      "value-changed",
                      GTK_SIGNAL_FUNC( AcceptChanged ), pmfsw );

  

  return pwPage;

}




static GtkWidget *
MoveFilterSetup ( movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
                  int *pfOK ) {

  GtkWidget *pwSetup;
  GtkWidget *pwFrame;
  int i, j;
  movefiltersetupwidget *pmfsw;
  GtkWidget *pwNotebook;
  GtkWidget *pwvbox;
  GtkWidget *pwMenu;
  GtkWidget *pwItem;
  GtkWidget *pw;
  int *pi;

  pwSetup = gtk_vbox_new ( FALSE, 4 );

  pmfsw = 
    (movefiltersetupwidget *) malloc ( sizeof ( movefiltersetupwidget ) );

  /* predefined settings */

  /* output widget (with "User defined", or "Large" etc */

  pwFrame = gtk_frame_new ( _("Predefined move filters:" ) );
  gtk_box_pack_start ( GTK_BOX ( pwSetup ), pwFrame, TRUE, TRUE, 0 );

  pwMenu = gtk_menu_new ();

  for ( i = 0; i <= NUM_MOVEFILTER_SETTINGS; i++ ) {

    if ( i < NUM_MOVEFILTER_SETTINGS )
      gtk_menu_append ( GTK_MENU ( pwMenu ),
                        pwItem = gtk_menu_item_new_with_label ( 
                                    gettext ( aszMoveFilterSettings[ i ] ) ) );
    else
      gtk_menu_append ( GTK_MENU ( pwMenu ),
                        pwItem = gtk_menu_item_new_with_label (
                                    _("user defined") ) );

    pi = g_malloc ( sizeof ( int ) );
    *pi = i;
    gtk_object_set_data_full( GTK_OBJECT( pwItem ), "user_data", 
                              pi, g_free );

    gtk_signal_connect ( GTK_OBJECT ( pwItem ), "activate",
                         GTK_SIGNAL_FUNC ( SetupSettingsMenuActivate ),
                         (void *) pmfsw );

    }

  pmfsw->pwOptionMenu = gtk_option_menu_new ();
  gtk_option_menu_set_menu ( GTK_OPTION_MENU ( pmfsw->pwOptionMenu ), pwMenu );

  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pmfsw->pwOptionMenu );

  /* notebook with pages for each ply */

  pwNotebook = gtk_notebook_new ();
  gtk_box_pack_start ( GTK_BOX ( pwSetup ), pwNotebook, FALSE, FALSE, 0 );

  for ( i = 0; i < MAX_FILTER_PLIES; ++i ) {

    char *sz = g_strdup_printf ( _("%d-ply"), i + 1 );

    pwvbox = gtk_vbox_new ( FALSE, 4 );
    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                               pwvbox,
                               gtk_label_new ( sz ) );
    g_free ( sz );

    for ( j = 0; j <= i; ++j ) {

      sz = g_strdup_printf ( _("%d-ply"), j );
      pwFrame = gtk_frame_new ( sz );
      g_free ( sz );

      gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwFrame, FALSE, FALSE, 4 );

      gtk_container_add ( GTK_CONTAINER ( pwFrame ),
                          MoveFilterPage ( i, j, aamf, pmfsw ) );

    }

  }

  gtk_object_set_data_full( GTK_OBJECT( pwSetup ), "user_data", 
                            pmfsw, g_free );

  pmfsw->pfOK = pfOK;
  pmfsw->pmf = (movefilter *) aamf;

  MoveFilterSetupSetValues ( aamf, pmfsw );

  return pwSetup;


}

static void
MoveFilterSetupOK ( GtkWidget *pw, GtkWidget *pwMoveFilterSetup ) {

  movefiltersetupwidget *pmfsw = 
    gtk_object_get_user_data ( GTK_OBJECT ( pwMoveFilterSetup ) );

                               
  if ( pmfsw->pfOK )
    *pmfsw->pfOK = TRUE;

  MoveFilterSetupGetValues ( pmfsw->pmf, pmfsw );

  if ( pmfsw->pfOK )
    gtk_widget_destroy ( gtk_widget_get_toplevel ( pw ) );

}

static void
MoveFilterChanged ( movefilterwidget *pmfw ) {

  int i;
  int fFound = FALSE;
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  

  memcpy ( aamf, pmfw->pmf, sizeof ( aamf ) );

  for ( i = 0; i < NUM_MOVEFILTER_SETTINGS; ++i ) 
    if ( equal_movefilters ( aamf, 
                             aaamfMoveFilterSettings[ i ] ) ) {
      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pmfw->pwOptionMenu ),
                                    i );
      fFound = TRUE;
      break;
    }

  if ( ! fFound )
      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pmfw->pwOptionMenu ),
                                    NUM_MOVEFILTER_SETTINGS );



}


static void
SettingsMenuActivate ( GtkWidget *pwItem,
                       movefilterwidget *pmfw ) {

  int *piSelected;
  
  piSelected = gtk_object_get_data ( GTK_OBJECT ( pwItem ), "user_data" );

  if ( *piSelected == NUM_MOVEFILTER_SETTINGS )
    return; /* user defined */

  memcpy ( pmfw->pmf, aaamfMoveFilterSettings[ *piSelected ],
           MAX_FILTER_PLIES * MAX_FILTER_PLIES * sizeof ( movefilter ) );

  MoveFilterChanged ( pmfw );

}


static void
ClickButton ( GtkWidget *pw, movefilterwidget *pmfw ) {

  int fOK;
  GtkWidget *pwDialog, *pwEval;
  GtkWidget *pwMoveFilterSetup;
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];


  memcpy ( aamf, pmfw->pmf, sizeof ( aamf ) );
  pwMoveFilterSetup = MoveFilterSetup( aamf, &fOK );

  pwDialog = CreateDialog( _("GNU Backgammon - Move filter setup"), 
                           DT_QUESTION,
                           GTK_SIGNAL_FUNC( MoveFilterSetupOK ), 
                           pwMoveFilterSetup );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     pwMoveFilterSetup );
  
  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
  
  gtk_widget_show_all( pwDialog );
  
  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

  if( fOK ) {
    memcpy ( pmfw->pmf, aamf, sizeof ( aamf ) );
    MoveFilterChanged ( pmfw );
  }

}


extern GtkWidget *
MoveFilterWidget ( movefilter *pmf, int *pfOK ) {

  GtkWidget *pwFrame;
  movefilterwidget *pmfw;
  GtkWidget *pw;
  GtkWidget *pwButton;
  GtkWidget *pwMenu;
  GtkWidget *pwItem;
  int i;
  int *pi;
  
  pwFrame = gtk_frame_new ( _("Move filter") );
  pmfw = (movefilterwidget *) malloc ( sizeof ( movefilterwidget ) );
  pmfw->pmf = pmf;

  /* output widget (with "User defined", or "Large" etc */

  pw = gtk_hbox_new ( FALSE, 4 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw );

  pwMenu = gtk_menu_new ();

  for ( i = 0; i <= NUM_MOVEFILTER_SETTINGS; i++ ) {

    if ( i < NUM_MOVEFILTER_SETTINGS )
      gtk_menu_append ( GTK_MENU ( pwMenu ),
                        pwItem = gtk_menu_item_new_with_label ( 
                                    gettext ( aszMoveFilterSettings[ i ] ) ) );
    else
      gtk_menu_append ( GTK_MENU ( pwMenu ),
                        pwItem = gtk_menu_item_new_with_label (
                                    _("user defined") ) );

    pi = g_malloc ( sizeof ( int ) );
    *pi = i;
    gtk_object_set_data_full( GTK_OBJECT( pwItem ), "user_data", 
                              pi, g_free );

    gtk_signal_connect ( GTK_OBJECT ( pwItem ), "activate",
                         GTK_SIGNAL_FUNC ( SettingsMenuActivate ),
                         (void *) pmfw );

    }

  pmfw->pwOptionMenu = gtk_option_menu_new ();
  gtk_option_menu_set_menu ( GTK_OPTION_MENU ( pmfw->pwOptionMenu ), pwMenu );

  gtk_box_pack_start ( GTK_BOX ( pw ), pmfw->pwOptionMenu, TRUE, TRUE, 0 );

  /* Button */

  pwButton = gtk_button_new_with_label ( _("Modify...") );
  gtk_box_pack_end ( GTK_BOX ( pw ), pwButton, FALSE, FALSE, 0 );

  
  gtk_signal_connect ( GTK_OBJECT ( pwButton ), "clicked",
                       GTK_SIGNAL_FUNC ( ClickButton ), pmfw );

  /* save movefilterwidget */

  gtk_object_set_data_full( GTK_OBJECT( pwFrame ), "user_data", pmfw, g_free );

  MoveFilterChanged ( pmfw );

  return pwFrame;


}

extern void
SetMovefilterCommands ( const char *sz,
                 movefilter aamfNew[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ], 
                 movefilter aamfOld[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

  int i, j;

  for ( i = 0; i < MAX_FILTER_PLIES; ++i )
    for ( j = 0; j <= i; ++j ) {
      if ( aamfNew[ i ][ j ].Accept != aamfOld[ i ][ j ].Accept ||
           aamfNew[ i ][ j ].Extra != aamfOld[ i ][ j ].Extra ||
           aamfNew[ i ][ j ].Threshold != aamfOld[ i ][ j ].Threshold ) {
      char *szCmd = g_strdup_printf ( "%s %d %d %d %d %f",
                                   sz, i + 1, j,
                                   aamfNew[ i ][ j ].Accept,
                                   aamfNew[ i ][ j ].Extra,
                                   aamfNew[ i ][ j ].Threshold );
      UserCommand ( szCmd );
      g_free ( szCmd );
      }

    }

}

extern void
MoveFilterOK ( GtkWidget *pw, GtkWidget *pwMoveFilter ) {



}

