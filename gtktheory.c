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

  GtkWidget *aaapwMW[ 2 ][ 8 ][ 3 ];

  /* gammon prices */

  GtkWidget *apwGammonPrice[ 2 ];

  GtkWidget *aapwMatchGP[ 2 ][ 2 ];
  GtkWidget *aaapwMoneyGP[ 2 ][ 2 ][ 2 ];

} theorywidget;



static void
ResetTheory ( GtkWidget *pw, theorywidget *ptw ) {

  float aarRates[ 2 ][ 2 ];
  evalcontext ec = { 0, FALSE, 0, 0, TRUE, 0.0, 0.0 };
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

  char sz[ 255 ];
  int i, j, k;
  int afAutoRedouble[ 2 ];
  int afDead[ 2 ];

  const char *aszMoneyPointLabel[] = {
    "Take Point (TP)",
    "Beaver Point (BP)",
    "Raccoon Point (RP)",
    "Initial Double Point (IDP)",
    "Redouble Point (RDP)",
    "Cash Point (CP)",
    "Too good Point (TP)" };

  const char *aszMatchPlayLabel[] = {
    "Take Point (TP)",
    "Double point (DP)",
    "Cash Point (CP)",
    "Too good Point (TP)" };

  /* get values */

  TheoryGetValues ( ptw, &ci, aarRates );

  SetCubeInfo ( &ci, ci.nCube, 0, 0, ci.nMatchTo,
                ci.anScore, ci.fCrawford, ci.fJacoby, ci.fBeavers );

  /* hide show widgets */

  gtk_widget_show ( ptw->apwFrame[ ! ci.nMatchTo ] );
  gtk_widget_hide ( ptw->apwFrame[ ci.nMatchTo != 0 ] );

  gtk_widget_show ( ptw->apwGammonPrice[ ! ci.nMatchTo ] );
  gtk_widget_hide ( ptw->apwGammonPrice[ ci.nMatchTo != 0 ] );

  sprintf ( sz, "Gammon price at %d-away, %d-away (%d cube)",
            ci.nMatchTo - ci.anScore[ 0 ],
            ci.nMatchTo - ci.anScore[ 1 ],
            ci.nCube );
  gtk_frame_set_label ( GTK_FRAME ( ptw->apwGammonPrice[ 0 ] ),
                        sz );

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

    /* match play */

    getMatchPoints ( aaarPointsMatch, afAutoRedouble, afDead, &ci, aarRates );

    for ( i = 0; i < 2; i++ ) {

      /* column headers */

      gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 1 ] ),
                           "Dead cube" );

      if ( afAutoRedouble[ i ] )
        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 2 ] ),
                             "Opp. redoubles" );
      else if ( ! afDead[ i ] )
        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 2 ] ),
                             "Live cube" );
      else
        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 2 ] ),
                             "" );
        

      for ( j = 0; j < 4; j++ ) {

        /* row header */

        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j + 1 ][ 0 ] ),
                             aszMatchPlayLabel[ j ] );

        for ( k = 0; k < 2; k++ ) {
          
          int f = ( ( ! k ) || ( ! afDead[ i ] ) ) &&
            ! ( k && afAutoRedouble[ i ] && ! j ) &&
            ! ( k && afAutoRedouble[ i ] && j == 3 );

          if ( f ) {
            sprintf ( sz, "%7.3f%%", 100.0f * aaarPointsMatch[ i ][ j ][ k ] );
            gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j + 1 ][ k + 1 ] ), 
                                 sz );
          }
          else
            gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j + 1 ][ k + 1 ] ), 
                                 "" );

        }
      }

      for ( j = 5; j < 8; j++ ) 
        for ( k = 0; k < 3; k++ )
          gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j ][ k ] ), 
                               "" );


    }

  }
  else {

    /* money play */

    getMoneyPoints ( aaarPoints, ci.fJacoby, ci.fBeavers, aarRates );

    for ( i = 0; i < 2; i++ ) {

      /* column headers */

      gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 1 ] ),
                           "Dead cube" );
      gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ 0 ][ 2 ] ),
                           "Live cube" );

      for ( j = 0; j < 7; j++ ) {

        /* row header */

        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j + 1 ][ 0 ] ),
                             aszMoneyPointLabel[ j ] );

        for ( k = 0; k < 2; k++ ) {

          sprintf ( sz, "%7.3f%%", 100.0f * aaarPoints[ i ][ j ][ k ] );
          gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMW[ i ][ j + 1 ][ k + 1 ] ), 
                               sz );

        }
      }
    }

  }

  /*
   * Update gammon price widgets
   */

  if ( ci.nMatchTo ) {

    /* match play */

    for ( i = 0; i < 2; i++ ) {

      sprintf ( sz, "%6.4f", ci.arGammonPrice[ i ] );
      gtk_label_set_text ( GTK_LABEL ( ptw->aapwMatchGP[ i ][ 0 ] ),
                            sz );

      sprintf ( sz, "%6.4f", ci.arGammonPrice[ 2 + i ] );
      gtk_label_set_text ( GTK_LABEL ( ptw->aapwMatchGP[ i ][ 1 ] ),
                            sz );

    }

  }
  else {

    /* money play */

    for ( j = 0; j < 2; j++ ) {

      SetCubeInfo ( &ci, 1, j ? 1 : -1, 0, ci.nMatchTo,
                    ci.anScore, ci.fCrawford, ci.fJacoby, ci.fBeavers );

      for ( i = 0; i < 2; i++ ) {
        
        sprintf ( sz, "%6.4f", ci.arGammonPrice[ i ] );
        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMoneyGP[ j ][ i ][ 0 ] ),
                             sz );
        
        sprintf ( sz, "%6.4f", ci.arGammonPrice[ 2 + i ] );
        gtk_label_set_text ( GTK_LABEL ( ptw->aaapwMoneyGP[ j ][ i ][ 1 ] ),
                             sz );
        
      }

    }

  }

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

  GtkWidget *pwDialog;
  GtkWidget *pwNotebook;

  GtkWidget *pwVBox;
  GtkWidget *pwHBox;
  GtkWidget *pwFrame;
  GtkWidget *pwRadio;
  GtkWidget *pwTable;

  GtkWidget *pw, *pwx;

  int i, j, k;
  char sz[ 256 ];

  theorywidget *ptw;

  ptw = malloc ( sizeof ( theorywidget ) );

  /* create dialog */

  pwDialog = CreateDialog ( "GNU Backgammon - Theory", FALSE, 
                            NULL, NULL );

  pwVBox = gtk_vbox_new ( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pwVBox ), 8 );


  gtk_container_add ( GTK_CONTAINER ( DialogArea ( pwDialog, DA_MAIN ) ),
                      pwVBox );

  /* match/money play */

  pwHBox = gtk_hbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), pwHBox );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ), 
                      ptw->apwRadio[ 0 ] = 
                      gtk_radio_button_new_with_label ( NULL, "Match play" ) );
  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->apwRadio[ 1 ] = 
                      gtk_radio_button_new_with_label_from_widget (
                      GTK_RADIO_BUTTON ( ptw->apwRadio[ 0 ] ), 
                      "Money game" ) );

  gtk_signal_connect( GTK_OBJECT( ptw->apwRadio[ 0 ] ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );
  gtk_signal_connect( GTK_OBJECT( ptw->apwRadio[ 1 ] ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );


  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwReset = gtk_button_new_with_label ( "Reset" ) );
  gtk_signal_connect( GTK_OBJECT( ptw->pwReset ), "clicked",
                      GTK_SIGNAL_FUNC( ResetTheory ), ptw );

  /* match score widget */

  ptw->apwFrame[ 0 ] = gtk_frame_new ( "Match score" );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), ptw->apwFrame[ 0 ] );

  pw = gtk_vbox_new ( 0, FALSE );
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
                       pwx = gtk_label_new ( "-away" ),
                       2, 3, 0 + i, 1 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

    gtk_signal_connect( GTK_OBJECT( ptw->apwScoreAway[ i ] ), "value-changed",
			GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  }

  pwHBox = gtk_hbox_new ( 0, FALSE );
  gtk_container_add ( GTK_CONTAINER ( pw ), pwHBox );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwCrawford = gtk_check_button_new_with_label (
                          "Crawford game" ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwCrawford ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  ptw->pwCubeFrame = gtk_frame_new ( "Cube" );
  gtk_container_add ( GTK_CONTAINER ( pw ), ptw->pwCubeFrame );

  pwHBox = gtk_hbox_new ( 0, FALSE );
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
  

  ptw->apwFrame[ 1 ] = gtk_frame_new ( "Money play" );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), ptw->apwFrame[ 1 ] );

  pwHBox = gtk_hbox_new ( 0, FALSE );
  gtk_container_add ( GTK_CONTAINER ( ptw->apwFrame[ 1 ] ), pwHBox );

  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwJacoby = gtk_check_button_new_with_label (
                          "Jacoby rule" ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwJacoby ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );


  gtk_container_add ( GTK_CONTAINER ( pwHBox ),
                      ptw->pwBeavers = gtk_check_button_new_with_label (
                          "Beavers allowed" ) );

  gtk_signal_connect( GTK_OBJECT( ptw->pwBeavers ), "toggled",
                      GTK_SIGNAL_FUNC( TheoryUpdated ), ptw );

  /* gammon and backgammon percentages */

  pwFrame = gtk_frame_new ( "Gammon and backgammon percentages" );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), pwFrame );

  pwTable = gtk_table_new ( 3, 3, TRUE );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwTable );

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
                       pw = gtk_label_new ( i ? "bg rate(%)" : 
                                            "gammon rate(%)" ),
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
  

  /* add notebook pages */

  pwNotebook = gtk_notebook_new ();
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), pwNotebook );

  gtk_container_set_border_width ( GTK_CONTAINER ( pwNotebook ), 4 );

  /* market window */

  pwVBox = gtk_vbox_new ( 0, FALSE );
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwVBox,
                             gtk_label_new ( "Market window" ) );

  for ( i = 0; i < 2; i++ ) {

    sprintf ( sz, "Market window for player %s", ap[ i ].szName );
    pwFrame = gtk_frame_new ( sz );
    gtk_container_add ( GTK_CONTAINER ( pwVBox ), pwFrame );

    pwTable = gtk_table_new ( 8, 3, FALSE );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwTable );

    for ( j = 0; j < 3; j++ ) 
      for ( k = 0; k < 8; k++ )

        if ( j || k ) {

          gtk_table_attach ( GTK_TABLE ( pwTable ),
                             ptw->aaapwMW[ i ][ k ][ j ] = 
                             gtk_label_new ( "dummy" ),
                             j, j + 1, k, k + 1,
                             GTK_EXPAND | GTK_FILL,
                             GTK_EXPAND | GTK_FILL,
                             4, 0 );
          gtk_misc_set_alignment( GTK_MISC( ptw->aaapwMW[ i ][ k ][ j ] ), 
                                  0, 0.5 );

        }
        else 
          ptw->aaapwMW[ i ][ j ][ k ] = NULL;



  }

  /* gammon prices */

  pwVBox = gtk_vbox_new ( 0, FALSE );

  ptw->apwGammonPrice[ 0 ] = 
    gtk_frame_new ( "Gammon price at 0-away, 0-away for 0-cube" );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), ptw->apwGammonPrice[ 0 ] );

  pwTable = gtk_table_new ( 3, 3, TRUE );
  gtk_container_add ( GTK_CONTAINER ( ptw->apwGammonPrice[ 0 ] ), pwTable );

  for ( i = 0; i < 2; i++ ) {

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pwx = gtk_label_new ( ap[ i ].szName ),
                       0, 1, 1 + i, 2 + i, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

    gtk_table_attach ( GTK_TABLE ( pwTable ),
                       pwx = gtk_label_new ( i ? "backgammon price" : 
                                             "gammon price" ), 
                       1+ i, 2 + i, 0, 1, 
                       GTK_EXPAND | GTK_FILL,
                       GTK_EXPAND | GTK_FILL,
                       4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

    for ( j = 0; j < 2; j++ ) {

      gtk_table_attach ( GTK_TABLE ( pwTable ),
                         ptw->aapwMatchGP[ i ][ j ] = 
                         gtk_label_new ( "dummy" ),
                         1 + j, 2 + j, 1 +i, 2 + i,
                         GTK_EXPAND | GTK_FILL,
                         GTK_EXPAND | GTK_FILL,
                         4, 0 );
      gtk_misc_set_alignment( GTK_MISC( ptw->aapwMatchGP[ i ][ j ] ), 0, 0.5 );

    }

  }

  ptw->apwGammonPrice[ 1 ] = 
    gtk_frame_new ( "Gammon prices for money game" );
  gtk_container_add ( GTK_CONTAINER ( pwVBox ), ptw->apwGammonPrice[ 1 ] );

  pw = gtk_vbox_new ( 0, FALSE );
  gtk_container_add ( GTK_CONTAINER ( ptw->apwGammonPrice[ 1 ] ), pw );

  for ( k = 0; k < 2; k++ ) {

    gtk_container_add ( GTK_CONTAINER ( pw ), 
                        gtk_label_new ( k ?
                                        "Doubled cube" :
                                        "Centered cube" ) );
    
    pwTable = gtk_table_new ( 3, 3, TRUE );
    gtk_container_add ( GTK_CONTAINER ( pw ), pwTable );

    for ( i = 0; i < 2; i++ ) {

      gtk_table_attach ( GTK_TABLE ( pwTable ),
                         pwx = gtk_label_new ( ap[ i ].szName ),
                         0, 1, 1 + i, 2 + i, 
                         GTK_EXPAND | GTK_FILL,
                         GTK_EXPAND | GTK_FILL,
                         4, 0 );
      gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

      gtk_table_attach ( GTK_TABLE ( pwTable ),
                         pwx = gtk_label_new ( i ? "backgammon price" : 
                                               "gammon price" ), 
                         1+ i, 2 + i, 0, 1, 
                         GTK_EXPAND | GTK_FILL,
                         GTK_EXPAND | GTK_FILL,
                         4, 0 );
      gtk_misc_set_alignment( GTK_MISC( pwx ), 0, 0.5 );

      for ( j = 0; j < 2; j++ ) {
        
        gtk_table_attach ( GTK_TABLE ( pwTable ),
                           ptw->aaapwMoneyGP[ k ][ i ][ j ] = 
                           gtk_label_new ( "dummy" ),
                           1 + j, 2 + j, 1 +i, 2 + i,
                           GTK_EXPAND | GTK_FILL,
                           GTK_EXPAND | GTK_FILL,
                           4, 0 );
        gtk_misc_set_alignment( GTK_MISC( ptw->aaapwMoneyGP[ k ][ i ][ j ] ), 
                                0, 0.5 );

      }
      
    }

  }

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwVBox,
                             gtk_label_new ( "Gammon prices" ) );

  /* show dialog */

  gtk_notebook_set_page ( GTK_NOTEBOOK ( pwNotebook ),
                          fActivePage );

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  ResetTheory ( NULL, ptw );
  
  TheoryUpdated ( NULL, ptw );
  
  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}

