/*
 * gtkgame.c
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
#include "gtktheory.h"
#include "i18n.h"
#include "matchequity.h"

#define MAXPLY 4

typedef struct _theorywidget {

  cubeinfo ci;

  /* radio buttons */

  GtkWidget *apwRadio[ 2 ];

  /* frames ("match score" and "moeny game") */

  GtkWidget *apwFrame[ 2 ];

  /* gammon/backgammon rates widgets */

  GtkAdjustment *aapwRates[ 2 ][ 2 ];

  /* score */

  GtkAdjustment *apwScoreAway[ 2 ];

  /* cube */

  GtkWidget *apwCube[ 7 ];
  GtkWidget *pwCubeFrame;

  /* crawford, jacoby and more */

  GtkWidget *pwCrawford, *pwJacoby, *pwBeavers;

  /* reset */

  GtkWidget *pwReset;

  /* market window */

  GtkWidget *apwMW[ 2 ];

  /* window graphs */
  GtkWidget *apwGraph[ 2 ];
    
  /* gammon prices */

  GtkWidget *pwGammonPrice;

  /* dead double, live cash, and dead too good points; for graph drawing */
  float aar[ 2 ][ 3 ];


  /* radio buttons for plies */

  GtkWidget *apwPly[ MAXPLY ];

} theorywidget;



static void
MWGetSelection( GtkWidget *pw, GtkSelectionData *psd,
                guint n, guint t, theorywidget *ptw ) {


  printf( "sorry not implemented\n" );

}

static void
ResetTheory ( GtkWidget *pw, theorywidget *ptw ) {

  float aarRates[ 2 ][ 2 ];
  evalcontext ec = { FALSE, 0, 0, TRUE, 0.0 };
  float arOutput[ NUM_OUTPUTS ];

  int i,j;

  /* get current gammon rates */

  GetMatchStateCubeInfo ( &ptw->ci, &ms );

  getCurrentGammonRates ( aarRates, arOutput, ms.anBoard, &ptw->ci, &ec );

  /* cube */

  j = 1;
  for ( i = 0; i < 7; i++ ) {

    if ( j == ptw->ci.nCube ) {

      gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( ptw->apwCube[ i ] ),
                                     TRUE );
      break;

    }

    j *= 2;

  }

  /* set match play/money play radio button */

  gtk_toggle_button_set_active ( 
      GTK_TOGGLE_BUTTON ( ptw->apwRadio[ ptw->ci.nMatchTo == 0 ] ), TRUE );

  /* crawford, jacoby, beavers */

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( ptw->pwCrawford ),
                                 ptw->ci.fCrawford );

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( ptw->pwJacoby ),
                                 ptw->ci.fJacoby );

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( ptw->pwBeavers ),
                                 ptw->ci.fBeavers );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ptw->apwPly[ ec.nPlies ] ),
                                TRUE );


  for ( i = 0; i < 2; i++ ) {

    /* set score for player i */

    gtk_adjustment_set_value ( GTK_ADJUSTMENT ( ptw->apwScoreAway[ i ] ),
                               ( ptw->ci.nMatchTo ) ? 
                               ( ptw->ci.nMatchTo - ptw->ci.anScore[ i ] ) : 
                               1 );

    for ( j = 0; j < 2; j++ ) 
      /* gammon/backgammon rates */
      gtk_adjustment_set_value ( GTK_ADJUSTMENT ( ptw->aapwRates[ i ][ j ] ),
                                 aarRates[ i ][ j ] * 100.0f );

  }

  
}


static void
TheoryGetValues ( theorywidget *ptw, cubeinfo *pci, 
                  float aarRates[ 2 ][ 2 ] ) {

  int i, j;

  /* get rates */

  for ( i = 0; i < 2; i++ )
    for ( j = 0; j < 2; j++ )
      aarRates[ i ][ j ] = 0.01f * ptw->aapwRates[ i ][ j ]->value;


  /* money game or match play */

  if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( ptw->apwRadio[ 0 ] ) ) ) {

    /* match play */

    int n0 = ptw->apwScoreAway[ 0 ]->value;
    int n1 = ptw->apwScoreAway[ 1 ]->value;

    pci->nMatchTo = ( n1 > n0 ) ? n1 : n0;

    pci->anScore[ 0 ] = pci->nMatchTo - n0;
    pci->anScore[ 1 ] = pci->nMatchTo - n1;

    pci->fCrawford = gtk_toggle_button_get_active (
                        GTK_TOGGLE_BUTTON ( ptw->pwCrawford ) );

    j = 1;
    for ( i = 0; i < 7; i++ ) {

      if ( gtk_toggle_button_get_active ( 
              GTK_TOGGLE_BUTTON ( ptw->apwCube[ i ] ) ) ) {

        pci->nCube = j;
        break;

      }

      j *= 2;

    }


  }
  else {

    /* money play */

    pci->nMatchTo = 0;

    pci->anScore[ 0 ] = pci->anScore[ 1 ] = 0;

    pci->fJacoby = gtk_toggle_button_get_active (
                        GTK_TOGGLE_BUTTON ( ptw->pwJacoby ) );
    pci->fBeavers = gtk_toggle_button_get_active (
                        GTK_TOGGLE_BUTTON ( ptw->pwBeavers ) );

  }

}





static void
TheoryUpdated ( GtkWidget *pw, theorywidget *ptw ) {

  cubeinfo ci;
  float aarRates[ 2 ][ 2 ];
  float aaarPoints[ 2 ][ 7 ][ 2 ];
  float aaarPointsMatch[ 2 ][ 4 ][ 2 ];

  int i, j, k;
  int afAutoRedouble[ 2 ];
  int afDead[ 2 ];
  GdkFont *pf;
  gchar *pch;

  const char *aszMoneyPointLabel[] = {
    N_ ("Take Point (TP)"),
    N_ ("Beaver Point (BP)"),
    N_ ("Raccoon Point (RP)"),
    N_ ("Initial Double Point (IDP)"),
    N_ ("Redouble Point (RDP)"),
    N_ ("Cash Point (CP)"),
    N_ ("Too good Point (TP)") };

  const char *aszMatchPlayLabel[] = {
    N_ ("Take Point (TP)"),
    N_ ("Double point (DP)"),
    N_ ("Cash Point (CP)"),
    N_ ("Too good Point (TP)") };

  /* get values */

  TheoryGetValues ( ptw, &ci, aarRates );

  SetCubeInfo ( &ci, ci.nCube, 0, 0, ci.nMatchTo,
                ci.anScore, ci.fCrawford, ci.fJacoby, ci.fBeavers, ci.bgv );

  /* hide show widgets */

  gtk_widget_show ( ptw->apwFrame[ ! ci.nMatchTo ] );
  gtk_widget_hide ( ptw->apwFrame[ ci.nMatchTo != 0 ] );

  /* update match play widget */

  gtk_widget_set_sensitive ( ptw->pwCrawford, 
                             ( ( ci.anScore[ 0 ] == ci.nMatchTo - 1 ) ^
                               ( ci.anScore[ 1 ] == ci.nMatchTo - 1 ) ) );


  if ( ci.nMatchTo ) {

    gtk_widget_set_sensitive ( ptw->pwCubeFrame, ci.nMatchTo != 1 );
                             
    j = 1;
    for ( i = 0; i < 7; i++ ) {
      
      gtk_widget_set_sensitive ( ptw->apwCube[ i ],
                                 j < 2 * ci.nMatchTo );

      j *= 2;

    }

  }


  /* update money play widget */


  /* 
   * update market window widgets 
   */

  if( ci.nMatchTo ) {

    gchar *asz[ 4 ];

    /* match play */

    getMatchPoints ( aaarPointsMatch, afAutoRedouble, afDead, &ci, aarRates );

    for ( i = 0; i < 2; ++i ) {
    
      gtk_clist_freeze( GTK_CLIST( ptw->apwMW[ i ] ) );

      gtk_clist_clear( GTK_CLIST( ptw->apwMW[ i ] ) );

      gtk_clist_set_column_title( GTK_CLIST( ptw->apwMW[ i ] ), 1, 
                                  _("Dead cube") );
      if ( afAutoRedouble[ i ] )
        gtk_clist_set_column_title( GTK_CLIST( ptw->apwMW[ i ] ), 3, 
                                    _("Opp. redoubles") );
      else
        gtk_clist_set_column_title( GTK_CLIST( ptw->apwMW[ i ] ), 3, 
                                    _("Fully live") );

      for ( j = 0; j < 4; j++ ) {

        asz[ 0 ] = g_strdup( gettext( aszMatchPlayLabel[ j ] ) );


        for ( k = 0; k < 2; k++ ) {
          
          int f = ( ( ! k ) || ( ! afDead[ i ] ) ) &&
            ! ( k && afAutoRedouble[ i ] && ! j ) &&
            ! ( k && afAutoRedouble[ i ] && j == 3 );

          f = f || ( k && afAutoRedouble[ !i ] && ! j );

          f = f && 
            ( ! ci.nMatchTo || ( ci.anScore[ i ] + ci.nCube < ci.nMatchTo ) );

          if ( f ) 
            asz[ 1 + 2 * k ] = 
              g_strdup_printf( "%7.3f%%", 
                               100.0f * aaarPointsMatch[ i ][ j ][ k ] );
          else
            asz[ 1 + 2 * k ] = g_strdup( "" );

        }

        asz[ 2 ] = g_strdup( "" );

        gtk_clist_append( GTK_CLIST( ptw->apwMW[ i ] ), asz );

        for ( k = 0; k < 4; ++k )
          g_free( asz[ k ] );

      }

      gtk_clist_thaw( GTK_CLIST( ptw->apwMW[ i ] ) );

      for ( j = 0; j < 4; ++j )
        gtk_clist_set_column_width( GTK_CLIST( ptw->apwMW[ i ] ),
                                    j,
                                    gtk_clist_optimal_column_width( GTK_CLIST( ptw->apwMW[ i ] ), j  ) );

      ptw->aar[ i ][ 0 ] = aaarPointsMatch[ i ][ 1 ][ 0 ];
      ptw->aar[ i ][ 1 ] = aaarPointsMatch[ i ][ 2 ][ 1 ];
      ptw->aar[ i ][ 2 ] = aaarPointsMatch[ i ][ 3 ][ 0 ];
      
      gtk_widget_queue_draw( ptw->apwGraph[ i ] );

    }

  }
  else {

    gchar *asz[ 4 ];

    /* money play */

    getMoneyPoints ( aaarPoints, ci.fJacoby, ci.fBeavers, aarRates );

    for ( i = 0; i < 2; ++i ) {

      gtk_clist_freeze( GTK_CLIST( ptw->apwMW[ i ] ) );
      
      gtk_clist_clear( GTK_CLIST( ptw->apwMW[ i ] ) );
    
      gtk_clist_set_column_title( GTK_CLIST( ptw->apwMW[ i ] ), 1, 
                                _("Dead cube") );
      gtk_clist_set_column_title( GTK_CLIST( ptw->apwMW[ i ] ), 3, 
                                  _("Fully live") );

      for ( j = 0; j < 7; j++ ) {

        asz[ 0 ] = g_strdup( gettext( aszMoneyPointLabel[ j ] ) );


        for ( k = 0; k < 2; k++ )
          asz[ 2 * k + 1 ] = 
            g_strdup_printf( "%7.3f%%", 
                             100.0f * aaarPoints[ i ][ j ][ k ] );

        asz[ 2 ] = g_strdup( "" );

        gtk_clist_append( GTK_CLIST( ptw->apwMW[ i ] ), asz );

        for ( k = 0; k < 4; ++k )
          g_free( asz[ k ] );

      }

      gtk_clist_thaw( GTK_CLIST( ptw->apwMW[ i ] ) );

      for ( j = 0; j < 4; ++j )
        gtk_clist_set_column_width( GTK_CLIST( ptw->apwMW[ i ] ),
                                    j,
                                    gtk_clist_optimal_column_width( GTK_CLIST( ptw->apwMW[ i ] ), j ) );

      ptw->aar[ i ][ 0 ] = aaarPoints[ i ][ 3 ][ 0 ];
      ptw->aar[ i ][ 1 ] = aaarPoints[ i ][ 5 ][ 1 ];
      ptw->aar[ i ][ 2 ] = aaarPoints[ i ][ 6 ][ 0 ];

      gtk_widget_queue_draw( ptw->apwGraph[ i ] );
    }

  }

  /*
   * Update gammon price widgets
   */

#if WIN32
  /* Windows fonts come out smaller than you ask for, for some reason... */
  pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-12-"
                      "*-*-*-m-*-iso8859-1" );
#else
  pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-10-"
                      "*-*-*-m-*-iso8859-1" );
#endif

  gtk_text_freeze( GTK_TEXT( ptw->pwGammonPrice ) );

  gtk_editable_delete_text( GTK_EDITABLE( ptw->pwGammonPrice ), 0, -1 );

  if ( ci.nMatchTo ) {

    pch = g_strdup_printf( ("Gammon values at %d-away, %d-away (%d cube)\n\n"),
                           ci.nMatchTo - ci.anScore[ 0 ],
                           ci.nMatchTo - ci.anScore[ 1 ],
                           ci.nCube );

    gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                     pch, -1 );
    
    g_free( pch );

    gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                     "Player                          Gammon   BG\n", -1 );
    

    for ( j = 0; j < 2; ++j ) {
        
      pch = g_strdup_printf( "%-31.31s %6.4f   %6.4f\n",
                             ap[ j ].szName,
                             0.5f * ci.arGammonPrice[ j ],
                             0.5f * ( ci.arGammonPrice[ 2 + j ] + 
                                      ci.arGammonPrice[ j ] ) );
      
      gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                       pch, -1 );
      
      g_free( pch );
      
    }

  }
  else {

    gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                     _("Gammon values for money game:\n\n"), -1 );

    for ( i = 0; i < 2; ++i ) {

      pch = g_strdup_printf( _("%s:\n\n"
                               "Player                          "
                               "Gammon   BG\n"),
                             i ? _("Doubled cube") : _("Centered cube") );

      gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                       pch, -1 );
    
      g_free( pch );


      SetCubeInfo ( &ci, 1, i ? 1 : -1, 0, ci.nMatchTo, ci.anScore, 
                    ci.fCrawford, ci.fJacoby, ci.fBeavers, ci.bgv );

      for ( j = 0; j < 2; ++j ) {
        
        pch = g_strdup_printf( "%-31.31s %6.4f   %6.4f\n",
                               ap[ j ].szName,
                               0.5f * ci.arGammonPrice[ j ],
                               0.5f * ( ci.arGammonPrice[ 2 + j ] + 
                                        ci.arGammonPrice[ j ] ) );

        gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                         pch, -1 );
    
        g_free( pch );

      }

      gtk_text_insert( GTK_TEXT( ptw->pwGammonPrice ), pf, NULL, NULL,
                       "\n", -1 );

    }

  }

  gtk_text_thaw( GTK_TEXT( ptw->pwGammonPrice ) );




}

static void GraphExpose( GtkWidget *pwGraph, GdkEventExpose *pev,
			 theorywidget *ptw ) {
    
    int i, x = 8, y = 12, cx = pwGraph->allocation.width - 16 - 1,
	cy = pwGraph->allocation.height - 12, iPlayer, ax[ 3 ];
    char sz[ 4 ];
    
    /* FIXME: The co-ordinates used in this function should be determined
       from the text size, not hard-coded.  But GDK's text handling will
       undergo an overhaul with Pango once GTK+ 2.0 comes out, so let's
       cheat for now and then get it right once 2.0 is here. */

    iPlayer = pwGraph == ptw->apwGraph[ 1 ];
    
    for( i = 0; i <= 20; i++ ) {
	gtk_paint_vline( pwGraph->style, pwGraph->window, GTK_STATE_NORMAL,
			 NULL, pwGraph, "tick", y - 1, i & 3 ? y - 3 : y - 5,
			 x + cx * i / 20 );

	if( !( i & 3 ) ) {
	    sprintf( sz, "%d", i * 5 );
	    gtk_paint_string( pwGraph->style, pwGraph->window,
			      GTK_STATE_NORMAL, NULL, pwGraph, "label",
			      x + cx * i / 20 - 8 /* FIXME */, y - 3, sz );
	}
    }

    for( i = 0; i < 3; i++ )
	ax[ i ] = x + cx * ptw->aar[ iPlayer ][ i ];

    gtk_paint_box( pwGraph->style, pwGraph->window, GTK_STATE_NORMAL,
		   GTK_SHADOW_IN, NULL, pwGraph, "doubling-window",
		   x, 12, cx, cy );

    /* FIXME it's horrible to abuse the "state" parameters like this */
    if( ptw->aar[ iPlayer ][ 1 ] > ptw->aar[ iPlayer ][ 0 ] )
	gtk_paint_box( pwGraph->style, pwGraph->window, GTK_STATE_ACTIVE,
		       GTK_SHADOW_OUT, NULL, pwGraph, "take",
		       ax[ 0 ], 13, ax[ 1 ] - ax[ 0 ], cy - 2 );
    
    if( ptw->aar[ iPlayer ][ 2 ] > ptw->aar[ iPlayer ][ 1 ] )
	gtk_paint_box( pwGraph->style, pwGraph->window, GTK_STATE_PRELIGHT,
		       GTK_SHADOW_OUT, NULL, pwGraph, "drop",
		       ax[ 1 ], 13, ax[ 2 ] - ax[ 1 ], cy - 2 );
    
    if( ptw->aar[ iPlayer ][ 2 ] < 1.0 )
	gtk_paint_box( pwGraph->style, pwGraph->window, GTK_STATE_SELECTED,
		       GTK_SHADOW_OUT, NULL, pwGraph, "too-good",
		       ax[ 2 ], 13, x + cx - ax[ 2 ], cy - 2 );
}


static void
PlyClicked( GtkWidget *pw, theorywidget *ptw ) {

  int *pi = (int *) gtk_object_get_user_data( GTK_OBJECT( pw ) );
  int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
  cubeinfo ci;
  float aarRates[ 2 ][ 2 ];
  evalcontext ec = { FALSE, 0, 0, TRUE, 0.0 };
  float arOutput[ NUM_OUTPUTS ];
  int i, j;

  if ( !f )
    return;

  GetMatchStateCubeInfo ( &ci, &ms );
  TheoryGetValues( ptw, &ci, aarRates );
  
  ec.nPlies = *pi;
  ProgressStart( _("Evaluating gammon percentages" ) );
  if ( getCurrentGammonRates ( aarRates, arOutput, ms.anBoard, 
                               &ci, &ec ) < 0 ) {
    ProgressEnd();
    fInterrupt = FALSE;
    return;
  }
  ProgressEnd();

  for ( i = 0; i < 2; ++i )
    for ( j = 0; j < 2; ++j )
      gtk_adjustment_set_value ( GTK_ADJUSTMENT ( ptw->aapwRates[ i ][ j ] ),
                                 aarRates[ i ][ j ] * 100.0f );

  TheoryUpdated( NULL, ptw );

}


/*
 * Display widget with misc. theory:
 * - market windows
 * - gammon price
 *
 * Input:
 *   fActivePage: with notebook page should recieve focus.
 *
 */

extern void
GTKShowTheory ( const int fActivePage ) {

  GtkWidget *pwDialog, *pwNotebook;

  GtkWidget *pwOuterHBox, *pwVBox, *pwHBox;
  GtkWidget *pwFrame, *pwTable, *pwAlign;
  
  GtkWidget *pw, *pwx, *pwz;

  int i, j;
  char sz[ 256 ];
  int *pi;

  static char *aszTitles[] = {
    "",
    N_("Dead cube"),
    "" /* N_("Live cube") */,
    N_("Fully live")
  };
  gchar *asz[ 4 ];
    

  theorywidget *ptw;

  ptw = malloc ( sizeof ( theorywidget ) );

  /* create dialog */

  pwDialog = GTKCreateDialog ( _("GNU Backgammon - Theory"), DT_INFO,
                            NULL, NULL );

  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 660, 300 );

  pwOuterHBox = gtk_hbox_new ( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pwOuterHBox ), 8 );

  gtk_container_add ( GTK_CONTAINER ( DialogArea ( pwDialog, DA_MAIN ) ),
                      pwOuterHBox );

  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( pwOuterHBox ), pwVBox, FALSE, FALSE, 0 );

  /* match/money play */

  pwHBox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( pwVBox ), pwHBox, FALSE, FALSE, 0 );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ), 
                      ptw->apwRadio[ 0 ] = 
                      gtk_radio_button_new_with_label ( NULL, _("Match play") ) );
  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->apwRadio[ 1 ] = 
                      gtk_radio_button_new_with_label_from_widget (
                      GTK_RADIO_BUTTON ( ptw->apwRadio[ 0 ] ), 
                      _("Money game") ) );

  gtk_signal_connect( GTK_OBJECT( ptw->apwRadio[ 0 ] ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );
  gtk_signal_connect( GTK_OBJECT( ptw->apwRadio[ 1 ] ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );


  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwReset = gtk_button_new_with_label ( _("Reset") ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwReset ), "clicked",
                      GTK_SIGNAL_FUNC( ResetTheory ), ptw );

  /* match score widget */

  ptw->apwFrame[ 0 ] = gtk_frame_new ( _("Match score") );
  gtk_box_pack_start( GTK_BOX( pwVBox ), ptw->apwFrame[ 0 ], FALSE, FALSE, 0 );

  pw = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( ptw->apwFrame[ 0 ] ), pw );

  pwTable = gtk_table_new ( 2, 3, FALSE );
  gtk_container_add ( GTK_CONTAINER ( pw ), pwTable );

  for ( i = 0; i < 2; i++ ) {

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pwx = gtk_label_new ( ap[ i ].szName ),
                       0, 1, 0 + i, 1 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );


    ptw->apwScoreAway[ i ] = GTK_ADJUSTMENT (
       gtk_adjustment_new ( 
                           1, 
                           1, 64,
                           1, 10, 10 ) );

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       gtk_spin_button_new ( ptw->apwScoreAway[ i ],
                                             1, 0 ) ,
                       1, 2, 0 + i, 1 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pwx = gtk_label_new ( _("-away") ),
                       2, 3, 0 + i, 1 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

    gtk_signal_connect( GTK_OBJECT( ptw->apwScoreAway[ i ] ), "value-changed",
			GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  }

  pwHBox = gtk_hbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pw ), pwHBox );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwCrawford = gtk_check_button_new_with_label (
                          _("Crawford game") ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwCrawford ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  ptw->pwCubeFrame = gtk_frame_new ( _("Cube") );
  gtk_container_add ( GTK_CONTAINER ( pw ), ptw->pwCubeFrame );

  pwHBox = gtk_hbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( ptw->pwCubeFrame ), pwHBox );

  j = 1;
  for ( i = 0; i < 7; i++ ) {

    sprintf ( sz, "%d", j );

    if ( ! i )
      gtk_container_add ( GTK_CONTAINER ( pwHBox ), 
                          ptw->apwCube[ i ] = 
                          gtk_radio_button_new_with_label ( NULL, sz ) );
    else
      gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                          ptw->apwCube[ i ] = 
                          gtk_radio_button_new_with_label_from_widget (
                          GTK_RADIO_BUTTON ( ptw->apwCube[ 0 ] ), 
                          sz ) );

    gtk_signal_connect( GTK_OBJECT( ptw->apwCube[ i ] ), "toggled",
                        GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

    j *= 2;

  }

  /* match equity table */

  pwFrame = gtk_frame_new ( _("Match equity table" ) );
  gtk_container_add ( GTK_CONTAINER ( pw ), pwFrame );

  pwx = gtk_vbox_new ( FALSE, 4 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwx );

  gtk_box_pack_start( GTK_BOX( pwx ), pwz = gtk_label_new( miCurrent.szName ),
                      FALSE, FALSE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pwz ), 0, 0.5 );
  gtk_box_pack_start( GTK_BOX( pwx ),
                      pwz = gtk_label_new( miCurrent.szFileName ),
                      FALSE, FALSE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pwz ), 0, 0.5 );
  gtk_box_pack_start( GTK_BOX( pwx ),
                      pwz = gtk_label_new( miCurrent.szDescription ),
                      FALSE, FALSE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pwz ), 0, 0.5 );
  

  /* money play widget */

  ptw->apwFrame[ 1 ] = gtk_frame_new ( _("Money play") );
  gtk_box_pack_start( GTK_BOX( pwVBox ), ptw->apwFrame[ 1 ], FALSE, FALSE, 0 );

  pwHBox = gtk_hbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( ptw->apwFrame[ 1 ] ), pwHBox );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwJacoby = gtk_check_button_new_with_label (
                          _("Jacoby rule") ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwJacoby ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );


  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwBeavers = gtk_check_button_new_with_label (
                          _("Beavers allowed") ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwBeavers ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  /* gammon and backgammon percentages */

  pwFrame = gtk_frame_new ( _("Gammon and backgammon percentages") );
  gtk_box_pack_start( GTK_BOX( pwVBox ), pwFrame, FALSE, FALSE, 0 );

  pwx = gtk_vbox_new( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwx );


  pwTable = gtk_table_new ( 3, 3, TRUE );
  gtk_box_pack_start( GTK_BOX( pwx ), pwTable, FALSE, FALSE, 4 );

  for ( i = 0; i < 2; i++ ) {

    /* player name */

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pw = gtk_label_new ( ap[ i ].szName ),
                       0, 1, 1 + i, 2 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

    /* column title */

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pw = gtk_label_new ( i ? _("bg rate(%)") : 
                                            _("gammon rate(%)") ),
                       1 + i, 2 + i, 0, 1, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

    for ( j = 0; j < 2; j++ ) {

      ptw->aapwRates[ i ][ j ] = GTK_ADJUSTMENT (
                             gtk_adjustment_new ( 0.0, 0.0, 100.0,
                             0.01, 1.0, 1.0 ) );

      gtk_table_attach ( GTK_TABLE ( pwTable ),
                         gtk_spin_button_new ( ptw->aapwRates[ i ][ j ], 
                                               0.01, 2 ),
                         j + 1, j + 2, i + 1, i + 2,
                         GTK_EXPAND | GTK_FILL,
                         GTK_EXPAND | GTK_FILL,
                         4, 0 );

      gtk_signal_connect( GTK_OBJECT( ptw->aapwRates[ i ][ j ] ), 
                          "value-changed", 
                          GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

    }

  }

  /* radio buttons with plies */

  pwz = gtk_hbox_new( FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pwx ), pwz, FALSE, FALSE, 4 );

  for ( i = 0; i < MAXPLY; ++i ) {
    
    gchar *sz = g_strdup_printf( _("%d ply"), i );
    if ( !i )
      ptw->apwPly[ i ] = gtk_radio_button_new_with_label( NULL, sz );
    else
      ptw->apwPly[ i ] =
        gtk_radio_button_new_with_label_from_widget( 
                   GTK_RADIO_BUTTON ( ptw->apwPly[ 0 ] ), sz );
    g_free( sz );
  
    pi = (int *) g_malloc( sizeof ( int ) );
    *pi = i;
    
    gtk_object_set_data_full( GTK_OBJECT( ptw->apwPly[ i ] ),
                              "user_data", pi, g_free );

    gtk_box_pack_start( GTK_BOX( pwz ), ptw->apwPly[ i ], FALSE, FALSE, 4 );

    gtk_signal_connect( GTK_OBJECT( ptw->apwPly[ i ] ), "toggled", 
                        GTK_SIGNAL_FUNC( PlyClicked ), ptw );

  }

  /* add notebook pages */

  pwNotebook = gtk_notebook_new ();
  gtk_container_set_border_width ( GTK_CONTAINER ( pwNotebook ), 0 );

  gtk_box_pack_start( GTK_BOX( pwOuterHBox ), pwNotebook, TRUE, TRUE, 0 );

  /* market window */

  pwVBox = gtk_vbox_new ( FALSE, 10 );
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwVBox,
                             gtk_label_new ( _("Market window") ) );

  for ( i = 0; i < 4; ++i )
    asz[ i ] = gettext( aszTitles[ i ] );

  for ( i = 0; i < 2; ++i ) {

    sprintf ( sz, _("Market window for player %s"), ap[ i ].szName );
    pwFrame = gtk_frame_new ( sz );
    gtk_box_pack_start( GTK_BOX( pwVBox ), pwFrame, FALSE, FALSE, 0 );

    ptw->apwMW[ i ] = gtk_clist_new_with_titles( 4, asz );

    gtk_clist_set_selection_mode( GTK_CLIST( ptw->apwMW[ i ] ),
                              GTK_SELECTION_MULTIPLE );

    gtk_clist_column_titles_passive ( GTK_CLIST( ptw->apwMW[ i ] ) );

    /*
    for ( j = 0; j < 4; ++j )
      gtk_clist_set_column_auto_resize ( GTK_CLIST( ptw->apwMW[ i ] ),
      j, TRUE );*/

    gtk_selection_add_target( ptw->apwMW[ i ], GDK_SELECTION_PRIMARY,
                              GDK_SELECTION_TYPE_STRING, 0 );

    gtk_signal_connect( GTK_OBJECT( ptw->apwMW[ i ] ), "selection_get",
                        GTK_SIGNAL_FUNC( MWGetSelection ), ptw );

    gtk_container_add ( GTK_CONTAINER( pwFrame ), ptw->apwMW[ i ] );

  }

  /* window graph */
  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwVBox,
                             gtk_label_new ( _("Window graph") ) );

  for ( i = 0; i < 2; i++ ) {
      sprintf( sz, _("Window graph for player %s"), ap[ i ].szName );
      pwFrame = gtk_frame_new( sz );
      gtk_box_pack_start( GTK_BOX ( pwVBox ), pwFrame, FALSE, FALSE, 4 );

      gtk_container_add( GTK_CONTAINER( pwFrame ), pwAlign =
			 gtk_alignment_new( 0.5, 0.5, 1, 0 ) );
      gtk_container_add( GTK_CONTAINER( pwAlign ), ptw->apwGraph[ i ] =
			 gtk_drawing_area_new() );
      gtk_widget_set_name( ptw->apwGraph[ i ], "gnubg-doubling-window-graph" );
      gtk_container_set_border_width( GTK_CONTAINER( pwAlign ), 4 );
      gtk_widget_set_usize( ptw->apwGraph[ i ], -1, 48 );
      gtk_signal_connect( GTK_OBJECT( ptw->apwGraph[ i ] ), "expose_event",
			  GTK_SIGNAL_FUNC( GraphExpose ), ptw );
  }

  /* gammon prices */

  pwVBox = gtk_vbox_new ( FALSE, 0 );

  ptw->pwGammonPrice = gtk_text_new( NULL, NULL );
  gtk_text_set_line_wrap ( GTK_TEXT( ptw->pwGammonPrice ), FALSE );

  gtk_container_add ( GTK_CONTAINER( pwVBox ), ptw->pwGammonPrice );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwVBox,
                             gtk_label_new ( _("Gammon values") ) );

  /* show dialog */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  ResetTheory ( NULL, ptw );
  
  TheoryUpdated ( NULL, ptw );
  
  gtk_notebook_set_page ( GTK_NOTEBOOK ( pwNotebook ),
                          fActivePage ? 2 /* prices */ : 0 /* market */ );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}

