/*
 * gtkmet.c
 *
 * by Joern Thyssen <jth@freeshell.org>, 2002
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
#include <gtk/gtk.h>
#if USE_GTKEXTRA
#include <gtkextra/gtksheet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "eval.h"
#include "gtkgame.h"
#include "i18n.h"
#include "matchequity.h"
#include "gtkmet.h"

typedef struct _mettable {
  GtkWidget *pwTable;
  GtkWidget *pwName;
  GtkWidget *pwFileName;
  GtkWidget *pwDescription;
  GtkWidget *aapwLabel[ MAXSCORE ][ MAXSCORE ];
} mettable;

typedef struct _metwidget {
  GtkWidget *apwPostCrawford[ 2 ];
  GtkWidget *pwPreCrawford;
  int nMatchTo;
  int anAway[ 2 ];
} metwidget;


static void
UpdateTable ( mettable *pmt, 
              float aafMET[ MAXSCORE ][ MAXSCORE ],
              metinfo *pmi,
              const int nRows, const int nCols, const int fInvert ) {

  int i, j;
  char sz[ 16 ];

  /* set labels */

  gtk_label_set_text ( GTK_LABEL ( pmt->pwName ), pmi->szName );
  gtk_label_set_text ( GTK_LABEL ( pmt->pwFileName ), pmi->szFileName );
  gtk_label_set_text ( GTK_LABEL ( pmt->pwDescription ), pmi->szDescription );

  /* fill out table */
    
  for( i = 0; i < nRows; i++ )
    for( j = 0; j < nCols; j++ ) {

      if ( fInvert )
        sprintf( sz, "%8.4f", GET_MET( j, i, aafMET ) * 100.0f );
      else
        sprintf( sz, "%8.4f", GET_MET( i, j, aafMET ) * 100.0f );

      gtk_label_set_text ( GTK_LABEL ( pmt->aapwLabel[ i ][ j ] ), sz );

    }

}


static void
UpdateAllTables ( metwidget *pmw ) {

  mettable *pmt;
  int i;

  pmt = gtk_object_get_user_data ( GTK_OBJECT ( pmw->pwPreCrawford ) );
  UpdateTable ( pmt, aafMET, &miCurrent, pmw->nMatchTo, pmw->nMatchTo, FALSE );

  for ( i = 0; i < 2; ++i ) {
    pmt = 
      gtk_object_get_user_data ( GTK_OBJECT ( pmw->apwPostCrawford[ i ] ) );
    UpdateTable ( pmt, (float (*)[ MAXSCORE ]) aafMETPostCrawford[ i ], 
                  &miCurrent, pmw->nMatchTo, 1, TRUE );
  }


}


static GtkWidget 
*GTKWriteMET ( const int nRows, const int nCols,
               const int nAway0, const int nAway1 ) {

  int i, j;
  char sz[ 16 ];
  GtkWidget *pwScrolledWindow = gtk_scrolled_window_new( NULL, NULL );
#if USE_GTKEXTRA
  GtkWidget *pwTable = gtk_sheet_new_browser( nRows, nCols, "" );
#else
  GtkWidget *pwTable = gtk_table_new( nRows + 1, nCols + 1, TRUE );
  GtkWidget *pw;
#endif
  GtkWidget *pwBox = gtk_vbox_new( FALSE, 0 );
  mettable *pmt;

  pmt = (mettable *) g_malloc ( sizeof ( mettable ) );
  pmt->pwTable = pwTable;

  gtk_box_pack_start( GTK_BOX( pwBox ), 
                      pmt->pwName = gtk_label_new( miCurrent.szName ),
                      FALSE, FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pwBox ),
                      pmt->pwFileName = gtk_label_new( miCurrent.szFileName ),
                      FALSE, FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pwBox ),
                      pmt->pwDescription = gtk_label_new( miCurrent.szDescription ),
                      FALSE, FALSE, 4 );

  gtk_box_pack_start( GTK_BOX( pwBox ), pwScrolledWindow, TRUE, TRUE, 0 );

#if USE_GTKEXTRA
  gtk_container_add( GTK_CONTAINER( pwScrolledWindow ), pwTable );
#else
  gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(
                                                             pwScrolledWindow ), pwTable );
#endif

  /* header for rows */

  for( i = 0; i < nCols; i++ ) {
    sprintf( sz, _("%d-away"), i + 1 );
#if USE_GTKEXTRA
    gtk_sheet_row_button_add_label( GTK_SHEET( pwTable ), i, sz );
#else
    gtk_table_attach_defaults( GTK_TABLE( pwTable ),
                               pw = gtk_label_new( sz ),
                               i + 1, i + 2, 0, 1 );
    if ( i == nAway1 ) {
      gtk_widget_set_name ( GTK_WIDGET ( pw ),
                            "gnubg-met-matching-score" );
    }
#endif
  }

  /* header for columns */

  for( i = 0; i < nRows; i++ ) {
    sprintf( sz, _("%d-away"), i + 1 );
#if USE_GTKEXTRA
    gtk_sheet_column_button_add_label( GTK_SHEET( pwTable ), i, sz );
#else
    gtk_table_attach_defaults( GTK_TABLE( pwTable ),
                               pw = gtk_label_new( sz ),
                               0, 1, i + 1, i + 2 );
    if ( i == nAway0 ) {
      gtk_widget_set_name ( GTK_WIDGET ( pw ),
                            "gnubg-met-matching-score" );
    }
#endif

  }


  /* fill out table */
    
  for( i = 0; i < nRows; i++ )
    for( j = 0; j < nCols; j++ ) {

      pmt->aapwLabel[ i ][ j ] = gtk_label_new( NULL );

#if USE_GTKEXTRA
      gtk_sheet_attach_default ( GTK_SHEET ( pmt->pwTable ),
                                 pmt->aapwLabel[ i ][ j ], i, j );
#else
      gtk_table_attach_defaults( GTK_TABLE( pmt->pwTable ),
                                 pmt->aapwLabel[ i ][ j ],
                                 j + 1, j + 2, i + 1, i + 2 );
#endif /* USE_GTKEXTRA */

      if ( i == nAway0 && j == nAway1 ) {
        gtk_widget_set_name ( GTK_WIDGET ( pmt->aapwLabel[ i ][ j ] ),
                              "gnubg-met-the-score" );
      }
      else if ( i == nAway0 || j == nAway1 ) {
        gtk_widget_set_name ( GTK_WIDGET ( pmt->aapwLabel[ i ][ j ] ),
                              "gnubg-met-matching-score" );
      }
    }

#if !USE_GTKEXTRA
  gtk_table_set_col_spacings( GTK_TABLE( pwTable ), 4 );
#endif

  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolledWindow ),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC );

  gtk_object_set_data_full ( GTK_OBJECT ( pwBox ), "user_data", pmt, g_free );

  return pwBox;

}

static void invertMETlocal( GtkWidget *pw, metwidget *pmw ){

  if(fInvertMET)
    UserCommand( "set invert met off" );
  else
    UserCommand( "set invert met on" );

  UpdateAllTables ( pmw );

}


static void loadMET ( GtkWidget *pw, metwidget *pmw ) {

  SetMET ( NULL, NULL );

  UpdateAllTables ( pmw );

}


extern void GTKShowMatchEquityTable( const int nMatchTo,
                                     const int anScore[ 2 ] ) {

  /* FIXME: Widget should update after 'Invert' or 'Load ...' */  
  int i;
  char sz[ 50 ];
  GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Match equity table"),
                                      DT_INFO, NULL, NULL );
  GtkWidget *pwNotebook = gtk_notebook_new ();
  GtkWidget *pwLoad = gtk_button_new_with_label(_("Load table..."));
    
  GtkWidget *pwInvertButton = 
    gtk_toggle_button_new_with_label(_("Invert table")); 
  metwidget mw;

  mw.nMatchTo = nMatchTo;
  mw.anAway[ 0 ] = nMatchTo - anScore[ 0 ] - 1;
  mw.anAway[ 1 ] = nMatchTo - anScore[ 1 ] - 1;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwInvertButton),
                               fInvertMET); 
    
  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     pwNotebook );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
                     pwInvertButton );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
                     pwLoad );

  mw.pwPreCrawford = GTKWriteMET ( mw.nMatchTo, mw.nMatchTo,
                                   mw.anAway[ 0 ], mw.anAway[ 1 ] );
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             mw.pwPreCrawford, 
                             gtk_label_new ( _("Pre-Crawford") ) );

  for ( i = 0; i < 2; i++ ) {
      
    sprintf ( sz, _("Post-Crawford for player %s"), ap[ i ].szName );

    mw.apwPostCrawford[ i ] = GTKWriteMET ( nMatchTo , 1, 
                                            mw.anAway[ i ], mw.anAway[ !i ] );
    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                               mw.apwPostCrawford[ i ],
                               gtk_label_new ( sz ) );
  }

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 500, 300 );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwInvertButton ), "toggled",
                      GTK_SIGNAL_FUNC( invertMETlocal ), &mw );
  gtk_signal_connect( GTK_OBJECT( pwLoad ), "clicked",
                      GTK_SIGNAL_FUNC ( loadMET ), &mw );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  UpdateAllTables ( &mw );
    
  gtk_widget_show_all( pwDialog );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}


