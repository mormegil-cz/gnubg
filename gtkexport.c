/*
 * gtkexport.c
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
#include "export.h"
#include "gtkexport.h"

typedef struct _exportwidget {

  /* export settings */

  exportsetup *pexs;

  /* include */

  GtkWidget *apwInclude[ 4 ];

  /* board */

  GtkAdjustment *padjDisplayBoard;
  GtkWidget *apwSide[ 2 ];

  /* moves */

  GtkAdjustment *padjMoves;
  GtkWidget *pwMovesDetailProb;
  GtkWidget *apwMovesParameters[ 2 ];
  GtkWidget *apwMovesDisplay[ 7 ];

  /* cube */

  GtkWidget *pwCubeDetailProb;
  GtkWidget *apwCubeParameters[ 2 ];
  GtkWidget *apwCubeDisplay[ 10 ];

  /* other stuff */

  GtkWidget *pwHTMLPictureURL;

} exportwidget;




static void
ExportOK ( GtkWidget *pw, exportwidget *pew ) {

  exportsetup *pexs = pew->pexs;
  int i;

  /* include */
  
  pexs->fIncludeAnnotation = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 0 ] ) );

  pexs->fIncludeAnalysis = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 1 ] ) );

  pexs->fIncludeStatistics = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 2 ] ) );

  pexs->fIncludeLegend = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 3 ] ) );


  /* board */

  pexs->fDisplayBoard = pew->padjDisplayBoard->value;

  pexs->fSide = 0;
  for ( i = 0; i < 2; i++ ) 
    pexs->fSide = pexs->fSide |
      ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pew->apwSide[ i ] ) )  >> i );

  /* moves */

  pexs->nMoves = pew->padjMoves->value;

  pexs->fMovesDetailProb = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                     pew->pwMovesDetailProb ) );

  for ( i = 0; i < 2; i++ )
    pexs->afMovesParameters[ i ] =
      gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                       pew->apwMovesParameters[ i ] ) );

  for ( i = 0; i < 7; i++ )
    pexs->afMovesDisplay[ i ] =
      gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                       pew->apwMovesDisplay[ i ] ) );

  /* cube */
  
  pexs->fCubeDetailProb = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                     pew->pwCubeDetailProb ) );

  for ( i = 0; i < 2; i++ )
    pexs->afCubeParameters[ i ] =
      gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                       pew->apwCubeParameters[ i ] ) );

  for ( i = 0; i < 10; i++ )
    pexs->afCubeDisplay[ i ] =
      gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                       pew->apwCubeDisplay[ i ] ) );

  /* html */

  if ( pexs->szHTMLPictureURL )
    free ( pexs->szHTMLPictureURL );

  pexs->szHTMLPictureURL = 
    strdup ( gtk_entry_get_text( GTK_ENTRY( pew->pwHTMLPictureURL ) ) );

  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );


}


static void
ExportSet ( exportwidget *pew ) {

  exportsetup *pexs = pew->pexs;
  int i;

  /* include */

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 0 ] ),
                                pexs->fIncludeAnnotation );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 1 ] ),
                                pexs->fIncludeAnalysis );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 2 ] ),
                                pexs->fIncludeStatistics );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwInclude[ 3 ] ),
                                pexs->fIncludeLegend );

  /* board */

  gtk_adjustment_set_value ( pew->padjDisplayBoard, pexs->fDisplayBoard );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwSide[ 0 ] ),
                                pexs->fSide & 1 );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->apwSide[ 1 ] ),
                                pexs->fSide & 2 );

  /* moves */

  gtk_adjustment_set_value ( pew->padjMoves, pexs->nMoves );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwMovesDetailProb ),
                                pexs->fMovesDetailProb );
  for ( i = 0; i < 2; i++ )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                     pew->apwMovesParameters[ i ] ),
                                  pexs->afMovesParameters[ i ] );

  for ( i = 0; i < 7; i++ )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                     pew->apwMovesDisplay[ i ] ),
                                  pexs->afMovesDisplay[ i ] );

  /* cube */
  
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwCubeDetailProb ),
                                pexs->fMovesDetailProb );

  for ( i = 0; i < 2; i++ )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                     pew->apwCubeParameters[ i ] ),
                                  pexs->afCubeParameters[ i ] );

  for ( i = 0; i < 10; i++ )
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( 
                                     pew->apwCubeDisplay[ i ] ),
                                  pexs->afCubeDisplay[ i ] );

  /* html */

  if ( pexs->szHTMLPictureURL )
    gtk_entry_set_text( GTK_ENTRY( pew->pwHTMLPictureURL ), 
                        pexs->szHTMLPictureURL );


}


extern void
GTKShowExport ( exportsetup *pexs ) {

  GtkWidget *pwDialog;
  GtkWidget *pwNotebook;

  GtkWidget *pwVBox;
  GtkWidget *pwHBox;
  GtkWidget *pwFrame;
  GtkWidget *pwTable;
  GtkWidget *pwTableX;
  GtkWidget *pwAlign;
  
  GtkWidget *pw, *pwx;

  char *aszInclude[] = {
    "Annotations", "Analysis", "Statistics", "Legend" };

  char *aszMovesDisplay[] = {
    "Show moves marked 'verybad'",
    "Show moves marked 'bad'",
    "Show moves marked 'doubtful'",
    "Show unmarked moves",
    "Show moves marked 'interesting'",
    "Show moves marked 'good'",
    "Show moves marked 'verygood'" };

  char *aszCubeDisplay[] = {
    "Show cube decisions marked 'verybad'",
    "Show cube decisions marked 'bad'",
    "Show cube decisions marked 'doubtful'",
    "Show unmarked cube decisions",
    "Show cube decisions marked 'interesting'",
    "Show cube decisions marked 'good'",
    "Show cube decisions marked 'verygood'",
    "Show actual cube decisions",
    "Show missed doubles",
    "Show close cube decisions" };

  int i, j, k;
  char sz[ 256 ];

  exportwidget *pew;

  pew = malloc ( sizeof ( exportwidget ) );

  pew->pexs = pexs;

  /* create dialog */

  pwDialog = CreateDialog ( "GNU Backgammon - Export Settings", TRUE, 
                            GTK_SIGNAL_FUNC ( ExportOK ), pew );

  pwTable = gtk_table_new ( 3, 2, FALSE );
  gtk_container_add ( GTK_CONTAINER ( DialogArea ( pwDialog, DA_MAIN ) ),
                      pwTable );

  /* include stuff */

  pwFrame = gtk_frame_new ( "Include" );
  gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), pwFrame,
                     0, 1, 0, 1,
                     GTK_FILL, 
                     GTK_FILL, 
                     8, 0 );


  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwVBox );

  for ( i = 0; i < 4; i++ ) {

    gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                         pew->apwInclude[ i ] =
                         gtk_check_button_new_with_label ( aszInclude[ i ] ),
                         TRUE, TRUE, 0 );
  }

  /* show stuff */

  pwFrame = gtk_frame_new ( "Board" );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), pwFrame,
                     1, 2, 0, 1,
                     GTK_FILL, 
                     GTK_FILL, 
                     2, 2 );



  pwTableX = gtk_table_new ( 2, 3, FALSE );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwTableX );

  gtk_table_attach ( GTK_TABLE ( pwTableX ), 
                     pw = gtk_label_new ( "Board" ),
                     0, 1, 0, 1,
                     GTK_FILL, 
                     GTK_FILL, 
                     4, 0 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  pw = gtk_hbox_new ( FALSE, 0 );
  
  pew->padjDisplayBoard =  
    GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 1000,
                                        1, 1, 0 ) );

  gtk_box_pack_start ( GTK_BOX ( pw ), 
                       gtk_spin_button_new( pew->padjDisplayBoard, 1, 0 ),
                       TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pw ), 
                       gtk_label_new ( "move(s) between board shown" ),
                       TRUE, TRUE, 0 );

  gtk_table_attach ( GTK_TABLE ( pwTableX ), 
                     pw, 
                     1, 2, 0, 1,
                     GTK_FILL, 
                     GTK_FILL, 
                     4, 0 );


  gtk_table_attach ( GTK_TABLE ( pwTableX ), 
                     pw = gtk_label_new ( "Players" ),
                     0, 1, 1, 2,
                     GTK_FILL, 
                     GTK_FILL, 
                     4, 0 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  gtk_table_attach ( GTK_TABLE ( pwTableX ),
                     pew->apwSide[ 0 ] =
                     gtk_check_button_new_with_label ( ap[ 0 ].szName ),
                     1, 2, 1, 2,
                     GTK_FILL, 
                     GTK_FILL, 
                     4, 0 );

  gtk_table_attach ( GTK_TABLE ( pwTableX ),
                     pew->apwSide[ 1 ] =
                     gtk_check_button_new_with_label ( ap[ 1 ].szName ),
                     1, 2, 2, 3,
                     GTK_FILL, 
                     GTK_FILL, 
                     4, 0 );

  /* moves */

  pwFrame = gtk_frame_new ( "Output moves" );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), pwFrame,
                     0, 1, 1, 2,
                     GTK_FILL, 
                     GTK_FILL, 
                     2, 2 );

  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwVBox );
  

  pw = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pwVBox ), pw, TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pw ), gtk_label_new ( "Show at most" ),
                       TRUE, TRUE, 4 );

  pew->padjMoves =  
    GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 1000,
                                        1, 1, 0 ) );

  gtk_box_pack_start ( GTK_BOX ( pw ), 
                       gtk_spin_button_new( pew->padjMoves, 1, 0 ),
                       TRUE, TRUE, 4 );

  gtk_box_pack_start ( GTK_BOX ( pw ), 
                       gtk_label_new ( "move(s)" ),
                       TRUE, TRUE, 4 );


  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->pwMovesDetailProb =
                       gtk_check_button_new_with_label ( "Show detailed "
                                                         "probabilities" ), 
                       TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->apwMovesParameters[ 0 ] =
                       gtk_check_button_new_with_label ( "Show evaluation parameters" ), 
                       TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->apwMovesParameters[ 1 ] =
                       gtk_check_button_new_with_label ( "Show rollout parameters" ), 
                       TRUE, TRUE, 0 );


  for ( i = 0; i < 7; i++ )

    gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                         pew->apwMovesDisplay[ i ] =
                         gtk_check_button_new_with_label ( aszMovesDisplay[ i ] ), 
                         TRUE, TRUE, 0 );
                         

  /* cube */

  pwFrame = gtk_frame_new ( "Output cube decisions" );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), pwFrame,
                     1, 2, 1, 2,
                     GTK_FILL, 
                     GTK_FILL, 
                     2, 2 );

  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwVBox );
  

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->pwCubeDetailProb =
                       gtk_check_button_new_with_label ( "Show detailed " 
                                                         "probabilities" ), 
                       TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->apwCubeParameters[ 0 ] =
                       gtk_check_button_new_with_label ( "Show evaluation "
                                                         "parameters" ), 
                       TRUE, TRUE, 0 );

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->apwCubeParameters[ 1 ] =
                       gtk_check_button_new_with_label ( "Show rollout "
                                                         "parameters" ), 
                       TRUE, TRUE, 0 );


  for ( i = 0; i < 10; i++ )

    gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                         pew->apwCubeDisplay[ i ] =
                         gtk_check_button_new_with_label ( aszCubeDisplay[ i ] ), 
                         TRUE, TRUE, 0 );
                    

  /* html */

  pwFrame = gtk_frame_new ( "HTML export options" );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );
  gtk_table_attach ( GTK_TABLE ( pwTable ), pwFrame,
                     0, 1, 2, 3,
                     GTK_FILL, 
                     GTK_FILL, 
                     2, 2 );

  pwVBox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwVBox );
  gtk_container_set_border_width ( GTK_CONTAINER ( pwVBox ), 4 );
  
  
  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pw = gtk_label_new ( "URL to pictures" ),
                       TRUE, TRUE, 0 );
  gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

  gtk_box_pack_start ( GTK_BOX ( pwVBox ),
                       pew->pwHTMLPictureURL =
                       gtk_entry_new (),
                       TRUE, TRUE, 0 );


  /* show dialog */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  ExportSet ( pew );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}


