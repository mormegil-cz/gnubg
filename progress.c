/*
 * progress.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#if USE_GTK
#include <gtk/gtk.h>
#endif /* USE_GTK */


#include "eval.h"
#include "rollout.h"
#include "progress.h"
#include "backgammon.h"
#include "i18n.h"
#include "format.h"

#if USE_GTK
#include "gtkgame.h"
#endif /* USE_GTK */


typedef struct _rolloutprogress {

  void *p;
  int n;
  char **ppch;
  int iNextAlternative;
  int iNextGame;

#if USE_GTK
  rolloutstat *prs;
  GtkWidget *pwRolloutDialog;
  GtkWidget *pwRolloutResult;
  GtkWidget *pwRolloutProgress;
  GtkWidget *pwRolloutOK;
  GtkWidget *pwRolloutStop;
  GtkWidget *pwRolloutViewStat;
  guint nRolloutSignal;
#endif

} rolloutprogress;


#if USE_GTK




/*
 *
 */

/*
 * Make pages with statistics.
 */

static GtkWidget *
GTKStatPageWin ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 7 ];

  static char *aszRow[ 7 ];
  int i;
  int anTotal[ 6 ];

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( _("Win statistics") );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* table with results */

  aszColumnTitle[ 0 ] = strdup ( _("Cube") );
  for ( i = 0; i < 2; i++ ) {
    aszColumnTitle[ 3 * i + 1 ] = 
      g_strdup_printf ( _("Win Single\n%s"), ap[ i ].szName );
    aszColumnTitle[ 3 * i + 2 ] = 
      g_strdup_printf ( _("Win Gammon\n%s"), ap[ i ].szName );
    aszColumnTitle[ 3 * i + 3 ] = 
      g_strdup_printf ( _("Win BG\n%s"), ap[ i ].szName );
  }

  pwStat = gtk_clist_new_with_titles( 7, aszColumnTitle );

  for ( i = 0; i < 7; i++ )
    g_free ( aszColumnTitle[ i ] );

  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 7; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  memset ( anTotal, 0, sizeof ( anTotal ) );
  
  for ( i = 0; i < STAT_MAXCUBE; i++ ) {

    sprintf ( aszRow[ 0 ], ( i < STAT_MAXCUBE - 1 ) ?
	      _("%d-cube") : _(">= %d-cube"), 1 << i);
    sprintf ( aszRow[ 1 ], "%d", prs->acWin[ i ] );
    sprintf ( aszRow[ 2 ], "%d", prs->acWinGammon[ i ] );
    sprintf ( aszRow[ 3 ], "%d", prs->acWinBackgammon[ i ] );

    sprintf ( aszRow[ 4 ], "%d", (prs+1)->acWin[ i ] );
    sprintf ( aszRow[ 5 ], "%d", (prs+1)->acWinGammon[ i ] );
    sprintf ( aszRow[ 6 ], "%d", (prs+1)->acWinBackgammon[ i ] );

    anTotal[ 0 ] += prs->acWin[ i ];
    anTotal[ 1 ] += prs->acWinGammon[ i ];
    anTotal[ 2 ] += prs->acWinBackgammon[ i ];
    anTotal[ 3 ] += (prs+1)->acWin[ i ];
    anTotal[ 4 ] += (prs+1)->acWinGammon[ i ];
    anTotal[ 5 ] += (prs+1)->acWinBackgammon[ i ];

    gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  }


  sprintf ( aszRow[ 0 ], _("Total") );
  for ( i = 0; i < 6; i++ )
    sprintf ( aszRow[ i + 1 ], "%d", anTotal[ i ] );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  sprintf ( aszRow[ 0 ], "%%" );
  for ( i = 0; i < 6; i++ )
    sprintf ( aszRow[ i + 1 ], "%6.2f%%",
	      100.0 * (float) anTotal[ i ] / (float) cGames );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  for ( i = 0; i < 7; i++ ) free ( aszRow[ i ] );


  return pw;

}


static GtkWidget *
GTKStatPageCube ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 5 ];

  static char *aszRow[ 5 ];
  int i;
  char sz[ 100 ];
  int anTotal[ 4 ];

  aszColumnTitle[ 0 ] = strdup ( _("Cube") );
  for ( i = 0; i < 2; i++ ) {
    aszColumnTitle[ 2 * i + 1 ] = 
      g_strdup_printf ( _("#Double, take\n%s"), ap[ i ].szName );
    aszColumnTitle[ 2 * i + 2 ] = 
      g_strdup_printf ( _("#Double, pass\n%s"), ap[ i ].szName );
  }

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( _("Cube statistics") );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 5, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  for ( i = 0; i < 5; i++ )
    g_free ( aszColumnTitle[ i ] );
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 5; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  memset ( anTotal, 0, sizeof ( anTotal ) );
  
  for ( i = 0; i < STAT_MAXCUBE; i++ ) {

    sprintf ( aszRow[ 0 ], ( i < STAT_MAXCUBE - 1 ) ?
	      _("%d-cube") : _(">= %d-cube"), 2 << i );
    sprintf ( aszRow[ 1 ], "%d", prs->acDoubleTake[ i ] );
    sprintf ( aszRow[ 2 ], "%d", prs->acDoubleDrop[ i ] );

    sprintf ( aszRow[ 3 ], "%d", (prs+1)->acDoubleTake[ i ] );
    sprintf ( aszRow[ 4 ], "%d", (prs+1)->acDoubleDrop[ i ] );

    anTotal[ 0 ] += prs->acDoubleTake[ i ];
    anTotal[ 1 ] += prs->acDoubleDrop[ i ];
    anTotal[ 2 ] += (prs+1)->acDoubleTake[ i ];
    anTotal[ 3 ] += (prs+1)->acDoubleDrop[ i ];

    gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  }


  sprintf ( aszRow[ 0 ], _("Total") );
  for ( i = 0; i < 4; i++ )
    sprintf ( aszRow[ i + 1 ], "%d", anTotal[ i ] );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  sprintf ( aszRow[ 0 ], "%%" );
  for ( i = 0; i < 4; i++ )
    sprintf ( aszRow[ i + 1 ], "%6.2f%%",
	      100.0 * (float) anTotal[ i ] / (float) cGames );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  for ( i = 0; i < 5; i++ ) free ( aszRow[ i ] );

  if ( anTotal[ 1 ] + anTotal[ 0 ] ) {
    sprintf ( sz, _("Cube efficiency for %s: %7.4f"),
              ap[ 0 ].szName,
	      (float) anTotal[ 0 ] / ( anTotal[ 1 ] + anTotal[ 0 ] ) );
    pwLabel = gtk_label_new ( sz );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );
  }

  if ( anTotal[ 2 ] + anTotal[ 3 ] ) {
    sprintf ( sz, _("Cube efficiency for %s: %7.4f"),
              ap[ 1 ].szName,
	      (float) anTotal[ 2 ] / ( anTotal[ 3 ] + anTotal[ 2 ] ) );
    pwLabel = gtk_label_new ( sz );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );
  }

  return pw;

}

static GtkWidget *
GTKStatPageBearoff ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( _("Bearoff statistics") );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );
    

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  
  sprintf ( aszRow[ 0 ], _("Moves with bearoff") );
  sprintf ( aszRow[ 1 ], "%d", prs->nBearoffMoves );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nBearoffMoves );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], _("Pips lost") );
  sprintf ( aszRow[ 1 ], "%d", prs->nBearoffPipsLost );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nBearoffPipsLost );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], _("Average Pips lost") );

  if ( prs->nBearoffMoves )
    sprintf ( aszRow[ 1 ], "%7.2f",
  	    (float) prs->nBearoffPipsLost / prs->nBearoffMoves );
  else
    sprintf ( aszRow[ 1 ], "n/a" );

  if ( (prs+1)->nBearoffMoves )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      (float) (prs+1)->nBearoffPipsLost / (prs+1)->nBearoffMoves );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}

static GtkWidget *
GTKStatPageClosedOut ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( _("Closed out statistics") );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  sprintf ( aszRow[ 0 ], _("Number of close-outs") );
  sprintf ( aszRow[ 1 ], "%d", prs->nOpponentClosedOut );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nOpponentClosedOut );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], _("Average move number for close out") );
  if ( prs->nOpponentClosedOut )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentClosedOutMove / prs->nOpponentClosedOut );
  else
    sprintf ( aszRow[ 1 ], "n/a" );


  if ( (prs+1)->nOpponentClosedOut )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      1.0f + (prs+1)->rOpponentClosedOutMove /
	      (prs+1)->nOpponentClosedOut );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}


static GtkWidget *
GTKStatPageHit ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( _("Hit statistics") );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  sprintf ( aszRow[ 0 ], _("Number of games with hit(s)") );
  sprintf ( aszRow[ 1 ], "%d", prs->nOpponentHit );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nOpponentHit );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], _("Percent games with hits") );
  sprintf ( aszRow[ 1 ], "%7.2f%%", 
            100.0 * (prs)->nOpponentHit / ( 1.0 * cGames ) );
  sprintf ( aszRow[ 2 ], "%7.2f%%", 
            100.0 * (prs+1)->nOpponentHit / (1.0 * cGames ) );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], _("Average move number for first hit") );
  if ( prs->nOpponentHit )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentHitMove / prs->nOpponentHit );
  else
    sprintf ( aszRow[ 1 ], "n/a" );


  sprintf ( aszRow[ 0 ], _("Average move number for first hit") );
  if ( prs->nOpponentHit )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentHitMove / (1.0 * prs->nOpponentHit ) );
  else
    sprintf ( aszRow[ 1 ], "n/a" );

  if ( (prs+1)->nOpponentHit )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      1.0f + (prs+1)->rOpponentHitMove /
	      ( 1.0 * (prs+1)->nOpponentHit ) );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}

static GtkWidget *
GTKRolloutStatPage ( const rolloutstat *prs,
                     const int cGames ) {


  /* GTK Widgets */

  GtkWidget *pw;
  GtkWidget *pwWin, *pwCube, *pwHit, *pwBearoff, *pwClosedOut;

  GtkWidget *psw;

  /* Create notebook pages */

  pw = gtk_vbox_new ( FALSE, 0 );

  pwWin = GTKStatPageWin ( prs, cGames );
  pwCube = GTKStatPageCube ( prs, cGames );
  pwBearoff = GTKStatPageBearoff ( prs, cGames );
  pwHit =  GTKStatPageHit ( prs, cGames );
  pwClosedOut =  GTKStatPageClosedOut ( prs, cGames );

  /*
  pwNotebook = gtk_notebook_new ();

  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwWin,
                             gtk_label_new ( "Win" ) );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwCube,
                             gtk_label_new ( "Cube" ) );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwBearoff,
                             gtk_label_new ( "Bearoff" ) );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                             pwHit,
                             gtk_label_new ( "Hit" ) );

  gtk_box_pack_start( GTK_BOX( pw ), pwNotebook, FALSE, FALSE, 0 );
  */

  psw = gtk_scrolled_window_new ( NULL, NULL );

  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( psw ),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwWin, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwCube, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwBearoff, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwClosedOut, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwHit, FALSE, FALSE, 0 );

  gtk_scrolled_window_add_with_viewport (
      GTK_SCROLLED_WINDOW ( psw), pw);


  return psw;

}


static void
GTKViewRolloutStatistics(GtkWidget *widget, gpointer data){

  /* Rollout statistics information */
  
  rolloutprogress *prp = (rolloutprogress *) data;
  rolloutstat *prs = prp->prs;
  int cGames = gtk_progress_get_value ( GTK_PROGRESS( prp->pwRolloutProgress ) );
  int nRollouts = GTK_CLIST( prp->pwRolloutResult )->rows / 2;
  int i;

  /* GTK Widgets */

  GtkWidget *pwDialog;
  GtkWidget *pwNotebook;

  /* Temporary variables */

  char *sz;

  /* Create dialog */

  pwDialog = GTKCreateDialog ( _("Rollout statistics"), DT_INFO, NULL, NULL );

  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 0, 400 );

  /* Create notebook pages */

  pwNotebook = gtk_notebook_new ();

  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     pwNotebook );

  for ( i = 0; i < nRollouts; i++ ) {

    gtk_clist_get_text ( GTK_CLIST ( prp->pwRolloutResult ),
                              i * 2, 0, &sz );

    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                               GTKRolloutStatPage ( &prs[ i * 2 ],
                                                    cGames ),
                               gtk_label_new ( sz ) );

  }


  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  gtk_main();


}

static void RolloutCancel( GtkObject *po, rolloutprogress *prp ) {

    pwGrab = pwOldGrab;
    prp->pwRolloutDialog = NULL;
    prp->pwRolloutResult = NULL;
    prp->pwRolloutProgress = NULL;
    fInterrupt = TRUE;
    /* FIXME: we have a small problem here:
     *
     * rollout.c: calls fnAction, which calls
     *            GTKSuspendInput
     *            then the rollout dialog is canceled, which leads
     *            to pwGrab being reset to pwOldGrab
     *            GTKResumeInput, which tries to remove signal from
     *            pwGrab, but pwGrab has just be replaced...
     */


}

static void RolloutStop( GtkObject *po, gpointer p ) {

    fInterrupt = TRUE;
}

static void 
GTKRolloutProgressStart( const cubeinfo *pci, const int n,
                         rolloutstat aars[][ 2 ],
                         rolloutcontext *prc,
                         char asz[][ 40 ], void **pp ) {
    
  static char *aszTitle[] = {
    NULL,
    N_("Win"), 
    N_("Win (g)"), 
    N_("Win (bg)"), 
    N_("Lose (g)"), 
    N_("Lose (bg)"),
    N_("Cubeless"), 
    N_("Cubeful"),
	N_("Rank/no. JSDs")
  }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
  char *aszTemp[ 9 ];
  int i;
  GtkWidget *pwVbox;
  GtkWidget *pwButtons;
  rolloutprogress *prp = 
    (rolloutprogress *) g_malloc( sizeof ( rolloutprogress ) );
  int nColumns = sizeof (aszTitle) / sizeof (aszTitle[0]);
  *pp = prp;
  prp->prs = (rolloutstat *) aars;
    
  prp->pwRolloutDialog = 
    GTKCreateDialog( _("GNU Backgammon - Rollout"), DT_INFO,
                     NULL, NULL );
  prp->pwRolloutViewStat = 
    gtk_button_new_with_label ( _("View statistics") );
  prp->pwRolloutStop = gtk_button_new_with_label( _("Stop") );
    
  pwOldGrab = pwGrab;
  pwGrab = prp->pwRolloutDialog;
    
  prp->nRolloutSignal = 
    gtk_signal_connect( GTK_OBJECT( prp->pwRolloutDialog ),
                        "destroy", GTK_SIGNAL_FUNC( RolloutCancel ), prp );

  /* Buttons */

  pwButtons = DialogArea( prp->pwRolloutDialog, DA_BUTTONS );
  prp->pwRolloutOK = DialogArea( prp->pwRolloutDialog, DA_OK );

  gtk_container_add( GTK_CONTAINER( pwButtons ), prp->pwRolloutStop );
    
  if ( aars )
    gtk_container_add( GTK_CONTAINER( pwButtons ), prp->pwRolloutViewStat );

  gtk_widget_set_sensitive( prp->pwRolloutOK, FALSE );
  gtk_widget_set_sensitive( prp->pwRolloutViewStat, FALSE );
    
  /* Setup signal */

  gtk_signal_connect( GTK_OBJECT( prp->pwRolloutStop ), "clicked",
                      GTK_SIGNAL_FUNC( RolloutStop ), prp );
    
  gtk_signal_connect( GTK_OBJECT( prp->pwRolloutViewStat ), "clicked",
                      GTK_SIGNAL_FUNC( GTKViewRolloutStatistics ), prp );

  pwVbox = gtk_vbox_new( FALSE, 4 );
	
  for ( i = 0; i < nColumns; i++ )
    aszTemp[ i ] = aszTitle[ i ] ? gettext ( aszTitle[ i ] ) : "";

  prp->pwRolloutResult = gtk_clist_new_with_titles( nColumns, aszTemp );
  gtk_clist_column_titles_passive( GTK_CLIST( prp->pwRolloutResult ) );
    
  prp->pwRolloutProgress = gtk_progress_bar_new();
  gtk_progress_set_show_text ( GTK_PROGRESS ( prp->pwRolloutProgress ), TRUE );

  gtk_box_pack_start( GTK_BOX( pwVbox ), prp->pwRolloutResult, TRUE, TRUE, 0 );
  gtk_box_pack_start( GTK_BOX( pwVbox ), prp->pwRolloutProgress, FALSE, FALSE,
                      0 );
    
  for( i = 0; i < nColumns; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( prp->pwRolloutResult ), i,
                                      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( prp->pwRolloutResult ), i,
                                        GTK_JUSTIFY_RIGHT );
  }

  for( i = 0; i < n; i++ ) {
    gtk_clist_append( GTK_CLIST( prp->pwRolloutResult ), aszEmpty );
    gtk_clist_append( GTK_CLIST( prp->pwRolloutResult ), aszEmpty );

    gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ), i << 1, 0,
                        asz[ i ] );
    gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ), ( i << 1 ) | 1, 0,
                        _("Standard error") );
  }

  gtk_progress_configure( GTK_PROGRESS( prp->pwRolloutProgress ), 0, 0, 
                          prc->nTrials );
  gtk_progress_set_format_string( GTK_PROGRESS( prp->pwRolloutProgress ),
                                  "%v/%u (%p%%)" );
    
  gtk_container_add( GTK_CONTAINER( DialogArea( prp->pwRolloutDialog, DA_MAIN ) ),
                     pwVbox );

  gtk_window_set_modal( GTK_WINDOW( prp->pwRolloutDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( prp->pwRolloutDialog ),
                                GTK_WINDOW( pwMain ) );
    
  gtk_widget_show_all( prp->pwRolloutDialog );

#if 0
  GTKDisallowStdin();
  while( gtk_events_pending() )
    gtk_main_iteration();
  GTKAllowStdin();
#endif

}



static void
GTKRolloutProgress( float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                    float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                    const rolloutcontext *prc,
                    const cubeinfo aci[],
                    const int iGame,
                    const int iAlternative,
					const int nRank,	
					const float rJsd,
					const int fStopped,
					const int fShowRanks,
                    rolloutprogress *prp ) {

    char sz[ 32 ];
    int i;

    if( !prp ||  !prp->pwRolloutResult )
      return;

    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {

      /* result */

      if ( i < OUTPUT_EQUITY ) 
        strcpy( sz, OutputPercent( aarOutput[ iAlternative ][ i ] ) );
      else if ( i == OUTPUT_EQUITY ) 
        strcpy( sz, OutputEquityScale( aarOutput[ iAlternative ][ i ],
                                       &aci[ iAlternative ], &aci[ 0 ], 
                                       TRUE ) );
      else 
        strcpy( sz, 
                prc->fCubeful ? OutputMWC( aarOutput[ iAlternative ][ i ],
                                           &aci[ 0 ], TRUE ) : "n/a" );

      gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ), 
                          iAlternative * 2, i + 1, sz );

      /* standard errors */

      if ( i < OUTPUT_EQUITY )
        strcpy( sz, OutputPercent( aarStdDev[ iAlternative ][ i ] ) );
      else if ( i == OUTPUT_EQUITY ) 
        strcpy( sz, OutputEquityScale( aarStdDev[ iAlternative ][ i ],
                                       &aci[ iAlternative ], &aci[ 0 ], 
                                       FALSE ) );
      else
        strcpy( sz, 
                prc->fCubeful ? OutputMWC( aarStdDev[ iAlternative ][ i ],
                                           &aci[ 0 ], FALSE ) : "n/a" );

      gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ), 
                          iAlternative * 2 + 1, i + 1, sz );

    }

    if (fShowRanks && iGame > 1) {
	  sprintf (sz, "%d %s", nRank, fStopped ? "s" : "r");
	  gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ),
			      iAlternative * 2, i + 1, sz);
	  if (nRank != 1)
	    sprintf( sz,  "%5.3f", rJsd);
	  else
	    strcpy (sz, " ");

	  gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ),
			      iAlternative * 2 + 1, i + 1, sz);
	} else {
	  gtk_clist_set_text( GTK_CLIST( prp->pwRolloutResult ),
						  iAlternative * 2, i + 1, "n/a");
	}
	  
    gtk_progress_configure( GTK_PROGRESS( prp->pwRolloutProgress ),
                            iGame + 1, 0, prc->nTrials );
#if 0
    GTKDisallowStdin();
    while( gtk_events_pending() )
      gtk_main_iteration();
    GTKAllowStdin();
#endif
    return;
}

static void GTKRolloutProgressEnd( void **pp ) {

    rolloutprogress *prp = *pp;

    fInterrupt = FALSE;

    pwGrab = pwOldGrab;

    /* if they cancelled the rollout early, 
       prp->pwRolloutDialog has already been destroyed */
    if( !prp->pwRolloutDialog ) {
        g_free( *pp );
        return;
    }

    gtk_widget_set_sensitive( prp->pwRolloutOK, TRUE );
    gtk_widget_set_sensitive( prp->pwRolloutStop, FALSE );
    gtk_widget_set_sensitive( prp->pwRolloutViewStat, TRUE );

    gtk_progress_set_format_string( GTK_PROGRESS( prp->pwRolloutProgress ),
                                    _("Finished (%v trials)") );

    gtk_signal_disconnect( GTK_OBJECT( prp->pwRolloutDialog ), 
                           prp->nRolloutSignal );
    gtk_signal_connect( GTK_OBJECT( prp->pwRolloutDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    prp->pwRolloutProgress = NULL;

    g_free( *pp );
}


#endif /* USE_GTK */

static void
TextRolloutProgressStart( const cubeinfo *pci, const int n,
                          rolloutstat aars[ 2 ][ 2 ],
                          rolloutcontext *prc, char asz[][ 40 ], void **pp ) {

  int i;

  rolloutprogress *prp = 
    (rolloutprogress *) g_malloc( sizeof ( rolloutprogress ) );

  *pp = prp;

  prp->ppch = (char **) g_malloc( n * sizeof (char *) );
  for ( i = 0; i < n; ++i )
    prp->ppch[ i ] = (char *) asz[ i ];
  prp->n = n;
  prp->iNextAlternative = 0;
  prp->iNextGame = prc->nTrials/10;

}

static void
TextRolloutProgressEnd( void **pp ) {

  rolloutprogress *prp = *pp;

  g_free( prp->ppch );
  g_free( prp );

  output( "\r\n" );
  fflush( stdout );

}


static void
TextRolloutProgress( float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                     float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                     const rolloutcontext *prc,
                     const cubeinfo aci[],
                     const int iGame,
                     const int iAlternative,
                     const int nRank,
                     const float rJsd,
                     const int fStopped,
                     const int fShowRanks,
                     rolloutprogress *prp ) {

  char *pch, *pc;

  /* write progress 1/10th trial */

  if ( iGame == prp->iNextGame && iAlternative == prp->iNextAlternative ) {

    if ( ! iAlternative )
      outputl( "" );

    pch = OutputRolloutResult( NULL, 
                               (char(*) [1024]) prp->ppch[ iAlternative ], 
                               GCCCONSTAHACK aarOutput[ iAlternative ],
                               GCCCONSTAHACK aarStdDev[ iAlternative ],
                               &aci[ iAlternative ], 1, prc->fCubeful );

    if ( fShowRanks && iGame > 1 ) {

      pc = strrchr( pch, '\n' );
      *pc = 0;

      sprintf( pc, " %d%c", nRank, fStopped ? 's' : 'r' );

      if ( nRank != 1 )
        sprintf( strchr( pc, 0 ), " %5.3f\n", rJsd );
      else
        strcat( pc, "\n" );

      
    }

    prp->iNextAlternative = ( ++prp->iNextAlternative ) % prp->n;
    if ( iAlternative == ( prp->n - 1 ) )
      prp->iNextGame += prc->nTrials/10;

    output( pch );

  }
  else {

    /* To avoid *.po include \r. */
    outputf( _("Rollout: %d/%d trials"), iGame, prc->nTrials );
    output( "      \r" );
    fflush( stdout );

  }

}


extern void
RolloutProgressStart( const cubeinfo *pci, const int n,
                      rolloutstat aars[ 2 ][ 2 ],
                      rolloutcontext *prc, char asz[][ 40 ], void **pp ) {

  if ( ! fShowProgress )
    return;

#if USE_GTK
  if ( fX ) {
    GTKRolloutProgressStart( pci, n, aars, prc, asz, pp );
    return;
  }
#endif /* USE_GTK */

  TextRolloutProgressStart( pci, n, aars, prc, asz, pp );

}


extern void
RolloutProgress( float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                 float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                 const rolloutcontext *prc,
                 const cubeinfo aci[],
                 const int iGame,
                 const int iAlternative,
                 const int nRank,
                 const float rJsd,
                 const int fStopped,
                 const int fShowRanks,
                 void *pUserData ) {

  if ( ! fShowProgress )
    return;

#if USE_GTK
  if ( fX ) {
    GTKRolloutProgress( aarOutput, aarStdDev, prc, aci, iGame, iAlternative, 
                        nRank, rJsd, fStopped, fShowRanks, pUserData );
    return;
  }
#endif /* USE_GTK */

  TextRolloutProgress( aarOutput, aarStdDev, prc, aci, iGame, iAlternative, 
                       nRank, rJsd, fStopped, fShowRanks, pUserData );

}

extern void
RolloutProgressEnd( void **pp ) {

  if ( ! fShowProgress )
    return;

#if USE_GTK
  if ( fX ) {
    GTKRolloutProgressEnd( pp );
    return;
  }
#endif /* USE_GTK */

  TextRolloutProgressEnd( pp );

}

