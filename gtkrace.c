/*
 * gtkrace.c
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
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "backgammon.h"
#include "eval.h"
#include "gtkrace.h"
#include "gtkgame.h"
#include "i18n.h"
#include "onechequer.h"
#include "osr.h"
#include "format.h"

#if !HAVE_ERF
extern double erf( double x );
#endif


typedef struct _epcwidget {
  GtkWidget *apwEPC[ 2 ];
  GtkWidget *apwWastage[ 2 ];
} epcwidget;

typedef struct _racewidget {
  GtkAdjustment *padjTrials;
  GtkWidget *pwRollout, *pwOutput;
  int anBoard[ 2 ][ 25 ];
  epcwidget epcwOSR;
} racewidget;


static GtkWidget *
KleinmanPage ( int anBoard[ 2 ][ 25 ] ) {

  GtkWidget *pwvbox = gtk_vbox_new( FALSE, 4 );
  GtkWidget *pwTable = gtk_table_new ( 4, 2, FALSE );
  GtkWidget *pw;
  GtkWidget *pwp = gtk_alignment_new( 0, 0, 0, 0 );
  int i, nDiff, nSum;
  int anPips[ 2 ];
  char *sz;
  
  gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
  gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

  /* 
   * pip counts, diffs and sum 
   */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwTable, FALSE, FALSE, 4 );

  /* pip counts */

  PipCount ( anBoard, anPips );

  for ( i = 0; i < 2; ++i ) {

    sz = g_strdup_printf ( _("Player %s"), ap[  ms.fMove ? i : !i ].szName );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       0, 1, i, i + 1, 
                       0, 0, 4, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

    sz = g_strdup_printf ( _("%d pips"), anPips[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       gtk_label_new ( sz ),
                       1, 2, i, i + 1, 
                       0, 0, 4, 4 );
    g_free ( sz );

  }

  /* difference */

  nDiff = anPips[ 0 ] - anPips[ 1 ];

  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Difference:") ),
                     0, 1, 2, 3,
                     0, 0, 4, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  sz = g_strdup_printf ( _("%d"), nDiff );
  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     gtk_label_new ( sz ),
                     1, 2, 2, 3,
                     0, 0, 4, 4 );
  g_free ( sz );

  /* sum */

  nSum = anPips[ 0 ] + anPips[ 1 ];

  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Sum:") ),
                     0, 1, 3, 4,
                     0, 0, 4, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  sz = g_strdup_printf ( _("%d"), nSum );
  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     gtk_label_new ( sz ),
                     1, 2, 3, 4,
                     0, 0, 4, 4 );
  g_free ( sz );

  /* separator */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       gtk_hseparator_new() , FALSE, FALSE, 4 );
  
  /* kleinman count */

  if ( nDiff < -4 ) {

    /* position not eligible for Kleinman count */

    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                         pw = gtk_label_new ( _("Position not eligible for "
                                                "Kleinman count as "
                                                "diff < -4" ) ), 
                         FALSE, FALSE, 4 );

  }
  else {

    double rK = (double) ( nDiff + 4 ) / ( 2 * sqrt ( nSum -4 ) );

    sz = g_strdup_printf ( "K = (diff+4)/(2 sqrt(sum-4)) = %8.4g", rK );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                         pw = gtk_label_new ( sz ),
                         FALSE, FALSE, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

    sz = g_strdup_printf ( "Cubeless winning chance = "
                           "0.5 (1+erf(K)) = %8.4g%%", 
                           100.0 * ( 0.5 * ( 1.0 + erf ( rK ) ) ) );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                         pw = gtk_label_new ( sz ),
                         FALSE, FALSE, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

  }

  return pwp;

}

static GtkWidget *
ThorpPage ( int anBoard[ 2 ][ 25 ] ) {

  GtkWidget *pwvbox = gtk_vbox_new( FALSE, 4 );
  GtkWidget *pwTable = gtk_table_new ( 2, 2, FALSE );
  GtkWidget *pw;
  GtkWidget *pwp = gtk_alignment_new( 0, 0, 0, 0 );
  int i, nLeader, nTrailer, nDiff;
  int anPips[ 2 ];
  char *sz;
  
  gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
  gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

  /* 
   * pip counts, diffs and sum 
   */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwTable, FALSE, FALSE, 4 );

  /* pip counts */

  PipCount ( anBoard, anPips );

  for ( i = 0; i < 2; ++i ) {

    sz = g_strdup_printf ( _("Player %s"), ap[  ms.fMove ? i : !i ].szName );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       0, 1, i, i + 1, 
                       0, 0, 4, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

    sz = g_strdup_printf ( _("%d pips"), anPips[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       gtk_label_new ( sz ),
                       1, 2, i, i + 1, 
                       0, 0, 4, 4 );
    g_free ( sz );

  }

  /* separator */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       gtk_hseparator_new() , FALSE, FALSE, 4 );

  /* Thorp count */

  ThorpCount ( anBoard, &nLeader, &nTrailer );

  sz = g_strdup_printf ( "L = %d  T = %d -> %s, %s", 
                         nLeader, nTrailer,
                         ( nTrailer >= ( nLeader -1 ) ? _("Redouble") :
                           ( ( nTrailer >= ( nLeader -2 ) ) ? _("Double") : 
                             _("No double") ) ),
                         ( nTrailer >= ( nLeader + 2 ) ) ? 
                         _("drop") : _("take") );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( sz ),
                       FALSE, FALSE, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
  g_free ( sz );

  /* Bower's interpolation */

  if( ( nDiff = nTrailer - nLeader ) > 13 )
    nDiff = 13;
  else if( nDiff < -37 )
    nDiff = -37;
  sz = g_strdup_printf ( "Bower's interpolation: %d%% cubeless gwc", 
                         74 + 2 * nDiff );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( sz ),
                       FALSE, FALSE, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
  g_free ( sz );

  return pwp;

}

static GtkWidget *
EffectivePipCount( const float arPips[ 2 ], const float arWastage[ 2 ],
                   epcwidget *pepcw ) {

  GtkWidget *pwTable = gtk_table_new( 3, 4, FALSE );
  GtkWidget *pwvbox = gtk_vbox_new( FALSE, 0 );
  GtkWidget *pw;
  GtkWidget *pwFrame;
  gchar *sz;
  int i;

  pwFrame = gtk_frame_new( _( "Effective pip count" ) );

  gtk_container_add( GTK_CONTAINER( pwFrame ), pwvbox );
  gtk_container_set_border_width( GTK_CONTAINER( pwvbox ), 4 );

  /* table */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwTable, FALSE, FALSE, 4 );

  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("EPC") ),
                     1, 2, 0, 1,
                     0, 0, 4, 4 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Wastage") ),
                     2, 3, 0, 1,
                     0, 0, 4, 4 );

  for ( i = 0; i < 2; ++i ) {

    sz = g_strdup_printf ( _("Player %s"), ap[ i ].szName );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       0, 1, i + 1, i + 2, 
                       0, 0, 4, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

    sz = g_strdup_printf ( _("%7.3f"), arPips[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       1, 2, i + 1, i + 2, 
                       0, 0, 4, 4 );
    g_free ( sz );
    if ( pepcw )
      pepcw->apwEPC[ i ] = pw;

    sz = g_strdup_printf ( "%7.3f", arWastage[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       2, 3, i + 1, i + 2, 
                       0, 0, 4, 4 );
    g_free ( sz );
    if ( pepcw )
      pepcw->apwWastage[ i ] = pw;

  }

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( "EPC = Effective pip count = "
                                            "Avg. rolls * 8.167" ),
                       FALSE, FALSE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( "Wastage = EPC - Pips" ),
                       FALSE, FALSE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  return pwFrame;

}

static GtkWidget *
OneChequerPage ( int anBoard[ 2 ][ 25 ] ) {

  GtkWidget *pwvbox = gtk_vbox_new( FALSE, 4 );
  GtkWidget *pwTable = gtk_table_new ( 3, 4, FALSE );
  GtkWidget *pw;
  GtkWidget *pwp = gtk_alignment_new( 0, 0, 0, 0 );

  int anPips[ 2 ];
  float arMu[ 2 ];
  float arSigma[ 2 ];
  int i, j;
  char *sz;
  float r;
  float aarProb[ 2 ][ 100 ];
  int an[ 2 ][ 25 ];
  float arEPC[ 2 ];
  float arWastage[ 2 ];
  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
              5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
              1 * 16 + 1 * 20 + 1 * 24 ) / 36.0;

  gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
  gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

  /* calculate one chequer bearoff */

  memcpy( an, anBoard, 2 * 25 *sizeof ( int ) );

  if ( ms.fMove )
    SwapSides( an );

  PipCount ( an, anPips );

  for ( i = 0; i < 2; ++i )
    OneChequer ( anPips[ i ], &arMu[ i ], &arSigma[ i ] );

  for ( j = 0; j < 2; ++j )
    for ( i = 0; i < 100; ++i ) 
      aarProb[ j ][ i ] = fnd ( 1.0f * i, arMu[ j ], arSigma[ j ] );
  
  r = 0;
  for ( i = 0; i < 100; ++i )
    for ( j = i; j < 100; ++j )
      r += aarProb[ 1 ][ i ] * aarProb[ 0 ][ j ];

  /* build widget */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( "Number of rolls to bear off, "
                                            "assuming each player has one "
                                            "chequer only." ),
                       FALSE, FALSE, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  /* table */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwTable, FALSE, FALSE, 4 );

  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Pips") ),
                     1, 2, 0, 1,
                     0, 0, 4, 4 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Avg. rolls") ),
                     2, 3, 0, 1,
                     0, 0, 4, 4 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), 
                     pw = gtk_label_new ( _("Std.dev") ),
                     3, 4, 0, 1,
                     0, 0, 4, 4 );

  for ( i = 0; i < 2; ++i ) {

    sz = g_strdup_printf ( _("Player %s"), ap[ i ].szName );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       pw = gtk_label_new ( sz ),
                       0, 1, i + 1, i + 2, 
                       0, 0, 4, 4 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    g_free ( sz );

    sz = g_strdup_printf ( _("%d pips"), anPips[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       gtk_label_new ( sz ),
                       1, 2, i + 1, i + 2, 
                       0, 0, 4, 4 );
    g_free ( sz );

    sz = g_strdup_printf ( "%7.3f", arMu[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       gtk_label_new ( sz ),
                       2, 3, i + 1, i + 2, 
                       0, 0, 4, 4 );
    g_free ( sz );

    sz = g_strdup_printf ( "%7.3f", arSigma[ i ] );
    gtk_table_attach ( GTK_TABLE ( pwTable ), 
                       gtk_label_new ( sz ),
                       3, 4, i + 1, i + 2, 
                       0, 0, 4, 4 );
    g_free ( sz );

  }

  /* cubeless gwc */

  sz = g_strdup_printf ( _("Estimated cubeless gwc: %8.4f%%"),
                         r * 100.0f );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       pw = gtk_label_new ( sz ),
                       FALSE, FALSE, 4 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
  g_free ( sz );

  /* 
   * effective pip count 
   */

  for ( i = 0; i < 2; ++i ) {
    arEPC[ i ] = arMu[ i ] * x;
    arWastage[ i ] = arEPC[ i ] - anPips[ i ];
  }

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       EffectivePipCount( arEPC, arWastage, NULL ),
                       FALSE, FALSE, 4 );

  return pwp;

}

static void
PerformOSR ( GtkWidget *pw, racewidget *prw ) {

  GtkWidget *pwOutput = prw->pwOutput;
  int nTrials = prw->padjTrials->value;
  float ar[ 5 ];
  int i;
  char sz[ 16 ];
  int anPips[ 2 ];
  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
              5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
              1 * 16 + 1 * 20 + 1 * 24 ) / 36.0;
  float arMu[ 2 ];
  gchar *pch;

  raceProbs ( prw->anBoard, nTrials, ar, arMu );

  PipCount( prw->anBoard, anPips );
  
  for ( i = 0; i < 5; ++i ) {
    if( fOutputWinPC )
      sprintf( sz, "%5.1f%%", ar[ i ] * 100.0f );
    else
      sprintf( sz, "%5.3f", ar[ i ] );
    
      gtk_clist_set_text( GTK_CLIST( pwOutput ), 0, i + 1, sz );
  }

  for ( i = 0; i < 5; ++i )
    gtk_clist_set_text ( GTK_CLIST ( pwOutput ), 1, i + 1, _( "n/a" ) );

  /* effective pip count */

  for ( i = 0; i < 2; ++i ) {

    pch = g_strdup_printf ( _("%7.3f"), arMu[ i ] * x );
    gtk_label_set_text( GTK_LABEL( prw->epcwOSR.apwEPC[ i ] ), pch );
    g_free ( pch );

    pch = g_strdup_printf ( "%7.3f", arMu[ i ] * x - anPips[ i ] );
    gtk_label_set_text( GTK_LABEL( prw->epcwOSR.apwWastage[ i ] ), pch );
    g_free ( pch );

  }

}

static GtkWidget *
OSRPage ( int anBoard[ 2 ][ 25 ], racewidget *prw ) {

  GtkWidget *pwvbox = gtk_vbox_new( FALSE, 4 );
  GtkWidget *pw;
  GtkWidget *pwp = gtk_alignment_new( 0, 0, 0, 0 );
  int i;
  char *asz[ 6 ];
  float ar0[ 2 ] = { 0, 0 };
  const char *aszTitle[] = {
    "",
    N_("Win"),
    N_("W g"),
    N_("W bg"),
    N_("L g"),
    N_("L bg") 
  };

  gtk_container_set_border_width( GTK_CONTAINER( pwp ), 4 );
  gtk_container_add( GTK_CONTAINER( pwp ), pwvbox );

  prw->padjTrials = GTK_ADJUSTMENT(gtk_adjustment_new(576, 1, 
                                                      1296 * 1296, 36, 36, 0 ));
  pw = gtk_hbox_new( FALSE, 0 );
  gtk_box_pack_start( GTK_BOX( pwvbox), pw, FALSE, FALSE, 4 );

  gtk_box_pack_start( GTK_BOX( pw ),
                      gtk_label_new( _("Trials:") ), FALSE, FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pw ),
                      gtk_spin_button_new( prw->padjTrials, 36, 0 ),
                      TRUE, TRUE, 4 );
  gtk_box_pack_start( GTK_BOX( pw ),
                      prw->pwRollout = 
				gtk_button_new_with_label( _( "Roll out" ) ),
                      TRUE, TRUE, 4 );

  gtk_signal_connect( GTK_OBJECT( prw->pwRollout ), "clicked", 
                      GTK_SIGNAL_FUNC( PerformOSR ), prw );

  /* separator */

  gtk_box_pack_start( GTK_BOX( pwvbox ),
                      gtk_hseparator_new() , FALSE, FALSE, 4 );

  /* result */

  for ( i = 0; i < 6; ++i )
    asz[ i ] = gettext ( aszTitle[ i ] );

  prw->pwOutput = gtk_clist_new_with_titles( 6, asz );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       prw->pwOutput, FALSE, FALSE, 4 );
  gtk_clist_set_selection_mode( GTK_CLIST( prw->pwOutput ),
                                GTK_SELECTION_BROWSE );
  gtk_clist_column_titles_passive( GTK_CLIST( prw->pwOutput ) );

  for( i = 0; i < 6; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( prw->pwOutput ), i, TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( prw->pwOutput ), i,
                                        i ? GTK_JUSTIFY_RIGHT :
                                        GTK_JUSTIFY_LEFT );
    }

  for ( i = 0; i < 6; ++i )
    asz[ i ] = NULL;

  gtk_clist_append ( GTK_CLIST ( prw->pwOutput ), asz );
  gtk_clist_append ( GTK_CLIST ( prw->pwOutput ), asz );
  gtk_clist_set_text ( GTK_CLIST ( prw->pwOutput ), 0, 0, _( "Rollout" ) );
  gtk_clist_set_text ( GTK_CLIST ( prw->pwOutput ), 1, 0, _( "Std.dev." ) );

  /* effective pip count */

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       gtk_hseparator_new() , FALSE, FALSE, 4 );

  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       EffectivePipCount( ar0, ar0, &prw->epcwOSR ),
                       FALSE, FALSE, 4 );

  return pwp;

}

/*
 * Display widget with misc. race stuff:
 * - kleinman
 * - thorp
 * - one chequer race
 * - one sided rollout
 *
 * Input:
 *   fActivePage: with notebook page should recieve focus.
 *
 */

extern void
GTKShowRace ( const int fActivePage, int anBoard[ 2 ][ 25 ] ) {

  GtkWidget *pwDialog;
  GtkWidget *pwNotebook;

  racewidget *prw;

  prw = malloc ( sizeof ( racewidget ) );
  memcpy ( prw->anBoard, anBoard, 2 * 25 * sizeof ( int ) );

  /* create dialog */

  pwDialog = GTKCreateDialog ( _("GNU Backgammon - Race Theory"), DT_INFO,
                            NULL, NULL );

  /* add notebook pages */

  pwNotebook = gtk_notebook_new ();
  gtk_container_add ( GTK_CONTAINER ( DialogArea ( pwDialog, DA_MAIN ) ),
                      pwNotebook );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwNotebook ), 4 );

  /* Kleinman */

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             KleinmanPage ( anBoard ),
                             gtk_label_new ( _("Kleinman Count") ) );

  /* one chequer */

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             OneChequerPage ( anBoard ),
                             gtk_label_new ( _("One chequer bearoff") ) );

  /* Thorp */

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             ThorpPage ( anBoard ),
                             gtk_label_new ( _("Thorp Count") ) );

  /* One sided rollout */

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             OSRPage ( anBoard, prw ),
                             gtk_label_new ( _("One sided rollout") ) );

  /* show dialog */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  PerformOSR ( NULL, prw );
  
  gtk_notebook_set_page ( GTK_NOTEBOOK ( pwNotebook ),
                          fActivePage );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}
