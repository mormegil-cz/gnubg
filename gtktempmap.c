/*
 * gtktempmap.c
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

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "backgammon.h"
#include "eval.h"
#include "gtktempmap.h"
#include "gtkgame.h"
#include "drawboard.h"
#include "i18n.h"
#include "export.h"
#include "render.h"
#include "renderprefs.h"
#include "gtkboard.h"
#include "gtkgame.h"

#define SIZE_DIE 5


typedef struct _tempmapwidget {

  matchstate *pms;
  float aarEquity[ 6 ][ 6 ];
  float rAverage, rMin, rMax;

  GtkWidget *aapwDA[ 6 ][ 6 ];
  GtkWidget *aapwe[ 6 ][ 6 ];
  GtkWidget *pwAverage;

  unsigned char *achDice[ 2 ];
  unsigned char *achPips[ 2 ];

  GtkWidget *apwGauge[ 2 ];
  GtkWidget *pweAverage;

} tempmapwidget;


static int
CalcTempMapEquities( evalcontext *pec, matchstate *pms,
                     float aarEquity[ 6 ][ 6 ] ) {


  int i, j;
  float arOutput[ NUM_ROLLOUT_OUTPUTS ];
  int anBoard[ 2 ][ 25 ];
  int anMove[ 8 ];
  cubeinfo ci;
  float aar[ 6 ][ 6 ];

  /* calculate equities */

  ProgressStartValue ( _("Calculating equities" ), 21 );

  GetMatchStateCubeInfo( &ci, pms );

  for ( i = 0; i < 6; ++i )
    for ( j = 0; j <= i; ++j ) {

      /* find best move */

      memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

      if ( FindBestMove ( anMove, i + 1, j + 1, anBoard, &ci, pec,
                          defaultFilters ) < 0 ) {
        ProgressEnd();
        return -1;
      }

      /* evaluate resulting position */

      SwapSides ( anBoard );

      if ( GeneralEvaluationE ( arOutput, anBoard, &ci, pec ) < 0 ) {
        ProgressEnd();
        return -1;
      }

      
      InvertEvaluationR( arOutput, &ci );

      aar[ i ][ j ] = arOutput[ OUTPUT_CUBEFUL_EQUITY ];
      aar[ j ][ i ] = arOutput[ OUTPUT_CUBEFUL_EQUITY ];
      
      ProgressValueAdd ( 1 );

    }



  ProgressEnd();

  memcpy( aarEquity, aar, sizeof aar );

  return 0;

}


static void
UpdateStyle( GtkWidget *pw, const float r ) {

  GtkStyle *ps = gtk_style_copy( pw->style );

  ps->fg[ GTK_STATE_NORMAL ].red = 0xFFFF;
  ps->fg[ GTK_STATE_NORMAL ].blue = 
    ps->fg[ GTK_STATE_NORMAL ].green = ( 1.0 - r ) * 0xFFFF;

  ps->bg[ GTK_STATE_NORMAL ].red = 0xFFFF;
  ps->bg[ GTK_STATE_NORMAL ].blue = 
    ps->bg[ GTK_STATE_NORMAL ].green = ( 1.0 - r ) * 0xFFFF;

  gtk_widget_set_style( pw, ps ); 

}


static void
UpdateTempMapEquities( tempmapwidget *ptmw ) {

  int i, j;
  float rMax, rMin, rAverage, r;
  cubeinfo ci;

  /* calc. min, max and average */

  rMax = -10000;
  rMin = +10000;
  rAverage = 0.0f;
  for ( i = 0; i < 6; ++i )
    for ( j = 0; j < 6; ++j ) {
      r = ptmw->aarEquity[ i ][ j ];
      rAverage += r;
      if ( r > rMax )
        rMax = r;
      if ( r < rMin ) 
        rMin = r;
    }
  rAverage /= 36.0;


  ptmw->rMax = rMax;
  ptmw->rMin = rMin;
  ptmw->rAverage = rAverage;

  /* update styles */

  GetMatchStateCubeInfo( &ci, ptmw->pms );

  for ( i = 0; i < 6; ++i )
    for ( j = 0; j < 6; ++j ) {
      r = ( ptmw->aarEquity[ i ][ j ] - rMin ) / ( rMax - rMin );
      UpdateStyle( ptmw->aapwDA[ i ][ j ], r );
      gtk_tooltips_set_tip( ptt, ptmw->aapwe[ i ][ j ], 
                            OutputMWC( ptmw->aarEquity[ i ][ j ], 
                                       &ci, TRUE ), "" );
      gtk_widget_queue_draw( ptmw->aapwDA[ i ][ j ] );
    }


  r = ( ptmw->rAverage - rMin ) / ( rMax - rMin );
  gtk_tooltips_set_tip( ptt, ptmw->pweAverage,
                        OutputMWC( ptmw->rAverage, &ci, TRUE ), "" );
  UpdateStyle( ptmw->pwAverage, r );
  gtk_widget_queue_draw( ptmw->pwAverage );


  /* update labels on gauge */

  gtk_label_set_text( GTK_LABEL( ptmw->apwGauge[ 0 ] ),
                      OutputMWC( rMin, &ci, TRUE ) );
  gtk_label_set_text( GTK_LABEL( ptmw->apwGauge[ 1 ] ),
                      OutputMWC( rMax, &ci, TRUE ) );

} 

static void
ExposeQuadrant( GtkWidget *pw, GdkEventExpose *pev, gpointer *unused ) {
  
  gtk_draw_box( pw->style, pw->window, GTK_STATE_NORMAL, GTK_SHADOW_IN,
                0, 0, pw->allocation.width, pw->allocation.height );


}

static void
ExposeDie( GtkWidget *pw, GdkEventExpose *pev,
           tempmapwidget *ptmw ) {

  int *pi = (int *) gtk_object_get_user_data( GTK_OBJECT( pw ) );
  GdkGC *gc = ( ( BoardData *) ( BOARD( pwBoard ) )->board_data )->gc_copy;
  int x = ( pw->allocation.width - SIZE_DIE * 7 ) / 2;
  int y = ( pw->allocation.height - SIZE_DIE * 7 ) / 2;

  DrawDie( pw->window, ptmw->achDice, ptmw->achPips, SIZE_DIE,
           gc, x, y, ptmw->pms->fMove, *pi + 1 );

}


static void
TempMapPlyClicked( GtkWidget *pw, tempmapwidget *ptmw ) {

  int *pi = (int *) gtk_object_get_user_data( GTK_OBJECT( pw ) );
  evalcontext ec = { TRUE, 0, 0, TRUE, 0.0 };

  ec.nPlies = *pi;
  if ( CalcTempMapEquities( &ec, ptmw->pms, ptmw->aarEquity ) )
    return;

  UpdateTempMapEquities( ptmw );


}

extern void
GTKShowTempMap( matchstate *pms ) {

  evalcontext ec = { TRUE, 0, 0, TRUE, 0.0 };
  tempmapwidget tmw;
  int *pi;
  int i, j;
  renderdata rd;

  GtkWidget *pwDialog = GTKCreateDialog( _("Sho Sengoku Temperature Map - "
                                        "Distribution of rolls"),
                                      DT_INFO, NULL, NULL );


  GtkWidget *pwTable = gtk_table_new( 7, 7, TRUE );
  GtkWidget *pwv;
  GtkWidget *pw;
  GtkWidget *pwh;

  /* vbox to hold tree widget and buttons */

  pwv = gtk_vbox_new ( FALSE, 8 );
  gtk_container_set_border_width ( GTK_CONTAINER ( pwv ), 8);
  gtk_container_add ( GTK_CONTAINER (DialogArea( pwDialog, DA_MAIN ) ), pwv );

  gtk_box_pack_start( GTK_BOX( pwv ), pwTable, FALSE, FALSE, 0 );

  /* render die */

  memcpy( &rd, &rdAppearance, sizeof rd );
  rd.nSize = SIZE_DIE;

  tmw.achDice[ 0 ] = malloc ( SIZE_DIE * SIZE_DIE * 7 * 7 * 4 );
  tmw.achDice[ 1 ] = malloc ( SIZE_DIE * SIZE_DIE * 7 * 7 * 4 );
  tmw.achPips[ 0 ] = malloc ( SIZE_DIE * SIZE_DIE * 3 );
  tmw.achPips[ 1 ] = malloc ( SIZE_DIE * SIZE_DIE * 3 );

  RenderDice( &rd, tmw.achDice[ 0 ], tmw.achDice[ 1 ], SIZE_DIE * 7 * 4 );
  RenderPips( &rd, tmw.achPips[ 0 ], tmw.achPips[ 1 ], SIZE_DIE * 3 );

  /* drawing areas */

  for ( i = 0; i < 6; ++i ) {
    for ( j = 0; j < 6; ++j ) {

      tmw.aapwDA[ i ][ j ] = gtk_drawing_area_new();
      tmw.aapwe[ i ][ j ] = gtk_event_box_new();

      gtk_container_add( GTK_CONTAINER( tmw.aapwe[ i ][ j ] ),
                         tmw.aapwDA[ i ][ j ] );

      gtk_drawing_area_size( GTK_DRAWING_AREA( tmw.aapwDA[ i ][ j ] ),
                             50, 50 );

      gtk_table_attach_defaults( GTK_TABLE( pwTable ), 
                                 tmw.aapwe[ i ][ j ],
                                 i + 1, i + 2,
                                 j + 1, j + 2 );

      gtk_signal_connect( GTK_OBJECT( tmw.aapwDA[ i ][ j ] ),
                          "expose_event",
                          GTK_SIGNAL_FUNC( ExposeQuadrant ), NULL );

    }

    /* die */

    pw = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( pw ), 50, 50 );

    gtk_table_attach_defaults( GTK_TABLE( pwTable ),
                               pw, 0, 1, i + 1, i + 2 );

    pi = (int *) g_malloc( sizeof ( int ) );
    *pi = i;
    
    gtk_object_set_data_full( GTK_OBJECT( pw ),
                              "user_data", pi, g_free );
    
    gtk_signal_connect( GTK_OBJECT( pw ),
                          "expose_event",
                        GTK_SIGNAL_FUNC( ExposeDie ),
                        &tmw );
    


    /* die */
    
    pw = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( pw ), 50, 50 );
    
    gtk_table_attach_defaults( GTK_TABLE( pwTable ),
                               pw, i + 1, i + 2, 0, 1 );

    pi = (int *) g_malloc( sizeof ( int ) );
    *pi = i;
    
    gtk_object_set_data_full( GTK_OBJECT( pw ),
                              "user_data", pi, g_free );
    
    gtk_signal_connect( GTK_OBJECT( pw ),
                          "expose_event",
                        GTK_SIGNAL_FUNC( ExposeDie ),
                        &tmw );
    


  }

  /* drawing area for average */

  tmw.pwAverage = gtk_drawing_area_new();
  tmw.pweAverage = gtk_event_box_new();

  gtk_container_add( GTK_CONTAINER( tmw.pweAverage ), tmw.pwAverage );

  gtk_drawing_area_size( GTK_DRAWING_AREA( tmw.pwAverage ),
                         50, 50 );
  
  gtk_table_attach_defaults( GTK_TABLE( pwTable ), 
                             tmw.pweAverage,
                             0, 1, 0, 1 );
  
  pi = (int *) g_malloc( sizeof ( int ) );
  *pi = -1;
  
  gtk_object_set_data_full( GTK_OBJECT( tmw.pwAverage ),
                            "user_data", pi, g_free );
  
  gtk_signal_connect( GTK_OBJECT( tmw.pwAverage ),
                      "expose_event",
                      GTK_SIGNAL_FUNC( ExposeQuadrant ),
                      &tmw );


  /* separator */

  gtk_box_pack_start( GTK_BOX( pwv ), gtk_hseparator_new(),
                      FALSE, FALSE, 0 );

  /* gauge */

  pwTable = gtk_table_new( 2, 16, TRUE );
  gtk_box_pack_start( GTK_BOX( pwv ), pwTable, FALSE, FALSE, 0 );

  for ( i = 0; i < 16; ++i ) {

    pw = gtk_drawing_area_new();
    gtk_drawing_area_size( GTK_DRAWING_AREA( pw ), 20, 20 );

      gtk_table_attach_defaults( GTK_TABLE( pwTable ), 
                                 pw,
                                 i, i + 1,
                                 1, 2 );

      gtk_signal_connect( GTK_OBJECT( pw ),
                          "expose_event",
                          GTK_SIGNAL_FUNC( ExposeQuadrant ), NULL );

      UpdateStyle( pw, 1.0 * i / 15.0 );


  }

  for ( i = 0; i < 2; ++i ) {
    tmw.apwGauge[ i ] = gtk_label_new( "" );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), tmw.apwGauge[ i ],
                               15 * i, 15 * i + 1,
                               0, 1 );
  }

  /* separator */

  gtk_box_pack_start( GTK_BOX( pwv ), gtk_hseparator_new(), FALSE, FALSE, 0 );


  /* buttons */

  pwh = gtk_hbox_new( FALSE, 4 );
  gtk_box_pack_start( GTK_BOX( pwv ), pwh, FALSE, FALSE, 0 );
  
  for ( i = 0; i < 4; ++i ) {

    gchar *sz = g_strdup_printf( _("%d ply"), i );
    pw = gtk_button_new_with_label( sz );
    gtk_box_pack_start( GTK_BOX( pwh ), pw, FALSE, FALSE, 0 );

    pi = (int *) g_malloc( sizeof ( int ) );
    *pi = i;
    
    gtk_object_set_data_full( GTK_OBJECT( pw ),
                              "user_data", pi, g_free );
    
    gtk_signal_connect( GTK_OBJECT( pw ), "clicked", 
                        GTK_SIGNAL_FUNC( TempMapPlyClicked ), &tmw );
                        
  }


  /* update */

  tmw.pms = pms;
  CalcTempMapEquities( &ec, pms, tmw.aarEquity );
  UpdateTempMapEquities( &tmw );

  /* modality */

  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 560, 500 ); 
  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );

  gtk_widget_show_all( pwDialog );

  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

  /* garbage collect */

  free( tmw.achDice[ 0 ] );
  free( tmw.achDice[ 1 ] );
  free( tmw.achPips[ 0 ] );
  free( tmw.achPips[ 1 ] );

}
