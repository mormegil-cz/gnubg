/*
 * gtkrolls.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2002
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
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>

#include "backgammon.h"
#include "eval.h"
#include "gtkrolls.h"
#include "gtkgame.h"
#include "drawboard.h"
#include "i18n.h"
#include "format.h"

#if USE_GTK2

typedef struct _rollswidget {

  GtkWidget *psw;   /* the scrolled window */
  GtkWidget *ptv;   /* the tree widget */
  GtkWidget *pDialog, *pCancel, *pScale;
    
  int closing;
  evalcontext *pec;
  matchstate *pms;
  int nDepth; /* current depth */
} rollswidget;


static void
add_level ( GtkTreeStore *model, GtkTreeIter *iter,
            const int n, int anBoard[ 2 ][ 25 ],
            evalcontext *pec, cubeinfo *pci,
            const int fInvert,
            float arOutput[ NUM_ROLLOUT_OUTPUTS ] ) {

  int n0, n1;
  GtkTreeIter child_iter;
  cubeinfo ci;
  int an[ 2 ][ 25 ];
  float ar[ NUM_ROLLOUT_OUTPUTS ];
  int anMove[ 8 ];
  int i;

  char szRoll[ 3 ], szMove[ 100 ], *szEquity;

  /* cubeinfo for opponent on roll */

  memcpy ( &ci, pci, sizeof ( cubeinfo ) );
  ci.fMove = ! pci->fMove;

  for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; ++i )
    arOutput[ i ] = 0.0f;

  for ( n0 = 0; n0 < 6; ++n0 ) {
    for ( n1 = 0; n1 <= n0; ++n1 ) {

      memcpy ( an, anBoard, sizeof ( an ) );

      if ( FindBestMove ( anMove, n0 + 1, n1 + 1, an, pci, pec,
                          defaultFilters ) < 0 )
        return;

      SwapSides ( an );

      gtk_tree_store_append ( model, &child_iter, iter );

      if ( n ) {

        add_level ( model, &child_iter, n - 1, an, pec, &ci, ! fInvert, ar );
		if (fInterrupt)
			return;

      }
      else {

        /* evaluate resulting position */

        ProgressValueAdd ( 1 );

        if ( GeneralEvaluationE ( ar, an, &ci, pec ) < 0 )
          return;

      }

      if ( fInvert )
        InvertEvaluationR ( ar, &ci );

      sprintf ( szRoll, _("%d%d"), n0 + 1, n1 + 1 );
      FormatMove ( szMove, anBoard, anMove );

      szEquity = OutputMWC ( ar[ OUTPUT_CUBEFUL_EQUITY ], 
                             fInvert ? pci : &ci, TRUE );

      gtk_tree_store_set ( model, &child_iter,
                           0, szRoll,
                           1, szMove,
                           2, szEquity,
                           -1 ); 

      for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; ++i )
        arOutput[ i ] += ( n0 == n1 ) ? ar[ i ] : 2.0f * ar[ i ];

    }

  }

  for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; ++i )
    arOutput[ i ] /= 36.0f;

  /* add average equity */

  szEquity = OutputMWC ( arOutput[ OUTPUT_CUBEFUL_EQUITY ],  
                         ! fInvert ? pci : &ci, TRUE );

  gtk_tree_store_append ( model, &child_iter, iter );

  gtk_tree_store_set ( model, &child_iter,
                       0, _("Average equity"),
                       1, "",
                       2, szEquity,
                       -1 );
  
  if ( ! fInvert )
    InvertEvaluationR ( arOutput, pci );

}


static gint
sort_func ( GtkTreeModel *model,
            GtkTreeIter *a,
            GtkTreeIter *b,
            gpointer user_data) {

  char *sz0, *sz1;
  float r0, r1;

  gtk_tree_model_get ( model, a, 2, &sz0, -1 );
  gtk_tree_model_get ( model, b, 2, &sz1, -1 );

  r0 = atof ( sz0 );
  r1 = atof ( sz1 );

  if ( r0 < r1 )
    return -1;
  else if( r0 > r1 )
    return +1;
  else 
    return 0;

}


static GtkTreeModel *
create_model ( const int n, evalcontext *pec, matchstate *pms ) {

  GtkTreeStore *model;
  int anBoard[ 2 ][ 25 ];
  cubeinfo ci;
  float arOutput[ NUM_ROLLOUT_OUTPUTS ];
  int i, j;

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  /* create tree store */
  model = gtk_tree_store_new ( 3,
			       G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING );

  GetMatchStateCubeInfo ( &ci, pms );

  for ( i = 0, j = 1; i < n; ++i, j *= 21 )
    ;

  ProgressStartValue ( _("Calculating equities" ), j );

  add_level ( model, NULL, n - 1, anBoard, pec, &ci, TRUE, arOutput );

  ProgressEnd ();

  if (!fInterrupt)
  {
	gtk_tree_sortable_set_sort_func ( GTK_TREE_SORTABLE ( model ),
										2, sort_func, NULL, NULL );
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 
											2, GTK_SORT_DESCENDING);
	return GTK_TREE_MODEL ( model );
  }
  else
	  return NULL;
}


static GtkWidget *
RollsTree ( const int n, evalcontext *pec, matchstate *pms ) {

  GtkTreeModel *pm;
  GtkWidget *ptv;
  int i;
  GtkCellRenderer *renderer;
  static char *aszColumn[] = {
    N_("Roll"),
    N_("Move"),
    N_("Equity")
  };

  pm = create_model( n, pec, pms );
  if( fInterrupt ) {
	  return NULL;
  }
  ptv = gtk_tree_view_new_with_model ( pm );
  g_object_unref ( G_OBJECT ( pm ) ); 

  /* add columns */

  for ( i = 0; i < 3; ++i ) {

    renderer = gtk_cell_renderer_text_new ();
    g_object_set ( G_OBJECT ( renderer ), "xalign", 0.0, NULL );

    gtk_tree_view_insert_column_with_attributes ( GTK_TREE_VIEW ( ptv ), 
                                                  -1, 
                                                  gettext ( aszColumn[ i ] ),
                                                  renderer, "text",
                                                  i, NULL );
  }  

  if( fInterrupt ) {
      gtk_widget_destroy( ptv );
      return NULL;
  }
  
  return ptv;
}

static void DepthChanged ( GtkRange *pr, rollswidget *prw )
{
	GtkWidget *pNewRolls;

	if (fInterrupt)
	{	/* Stop recursion on cancel */
		fInterrupt = FALSE;
		return;
	}

	int n = gtk_range_get_value(pr);
	if (n == prw->nDepth)
		return;

	pwOldGrab = pwGrab;
	pwGrab = prw->pDialog;

	gtk_widget_set_sensitive(DialogArea(prw->pDialog, DA_BUTTONS), FALSE);
	gtk_widget_set_sensitive(prw->pScale, FALSE);
	gtk_widget_set_sensitive(prw->pCancel, TRUE);

	pNewRolls = RollsTree ( n, prw->pec, prw->pms );

	if (pNewRolls)
	{
		if ( prw->ptv )
		{
			gtk_widget_destroy ( GTK_WIDGET ( prw->ptv ) );
			prw->ptv = NULL;
		}
		prw->ptv = pNewRolls;

		gtk_container_add ( GTK_CONTAINER ( prw->psw ), prw->ptv );
		prw->nDepth = n;
	}
	else
	{
		if (!prw->closing)
			gtk_range_set_value(GTK_RANGE(prw->pScale), prw->nDepth);
	}

	pwGrab = pwOldGrab;
	if (!prw->closing)
	{
		gtk_widget_set_sensitive(DialogArea(prw->pDialog, DA_BUTTONS), TRUE);
		gtk_widget_set_sensitive(prw->pScale, TRUE);
		gtk_widget_set_sensitive(prw->pCancel, FALSE);

		gtk_widget_show_all ( GTK_WIDGET ( prw->psw ) );
	}
	else
	{	/* dialog is waiting to close */
		gtk_widget_destroy(prw->pDialog);
	}
}

#endif /* USE_GTK2 */

void CancelRolls(GtkWidget* pButton)
{
	fInterrupt = TRUE;
	gtk_widget_set_sensitive(pButton, FALSE);
}

static gint RollsClose(GtkWidget *widget, GdkEvent *event, rollswidget *prw)
{
	if (pwOldGrab != pwGrab)
	{	/* Mid-depth change - wait for it to cancel */
		gtk_widget_set_sensitive(prw->pCancel, FALSE);
		prw->closing = TRUE;
	    fInterrupt = TRUE;
		return TRUE;
	}
	else
		return FALSE;
}

extern void
GTKShowRolls ( const gint nDepth, evalcontext *pec, matchstate *pms ) {

#if USE_GTK2

  GtkWidget *vbox, *hbox;
  GtkAdjustment *padj;

  rollswidget *prw = g_malloc ( sizeof ( rollswidget ) );

  prw->closing = FALSE;
  prw->pDialog = GTKCreateDialog( _("Distribution of rolls"), DT_INFO, NULL, NULL );

  int n = ( nDepth < 1 ) ? 1 : nDepth;

  prw->pms = pms;
  prw->pec = pec;
  prw->nDepth = -1; /* not yet calculated */
  
  /* vbox to hold tree widget and buttons */

  vbox = gtk_vbox_new ( FALSE, 8 );
  gtk_container_set_border_width ( GTK_CONTAINER ( vbox ), 8);
  gtk_container_add ( GTK_CONTAINER (DialogArea( prw->pDialog, DA_MAIN ) ), vbox );
  gtk_object_set_data_full ( GTK_OBJECT ( vbox ), "user_data",
                             prw, g_free );

  /* scrolled window to hold tree widget */

  prw->psw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type ( GTK_SCROLLED_WINDOW ( prw->psw ),
                                        GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( prw->psw ),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC );
  gtk_box_pack_start (GTK_BOX ( vbox ),prw->psw, TRUE, TRUE, 0);

  /* buttons */

  gtk_box_pack_start ( GTK_BOX ( vbox ), gtk_hseparator_new (),
                       FALSE, FALSE, 0 );

  hbox = gtk_hbox_new ( FALSE, 0 );
  gtk_box_pack_start (GTK_BOX ( vbox ),hbox, FALSE, FALSE, 4 );

  gtk_box_pack_start ( GTK_BOX ( hbox ), gtk_label_new ( _("Depth") ), 
                       FALSE, FALSE, 4 );

  /* Set page size to 1 */
  padj = GTK_ADJUSTMENT(gtk_adjustment_new(1, 1, 5, 1, 1, 0));
  prw->pScale = gtk_hscale_new( padj );
  gtk_widget_set_size_request( prw->pScale, 100, -1 );
  gtk_box_pack_start ( GTK_BOX ( hbox ), prw->pScale, FALSE, FALSE, 4 );
  gtk_scale_set_digits( GTK_SCALE( prw->pScale ), 0 );
  gtk_scale_set_draw_value( GTK_SCALE( prw->pScale ), TRUE );
  gtk_range_set_update_policy( GTK_RANGE( prw->pScale ), GTK_UPDATE_DISCONTINUOUS );

  prw->pCancel = gtk_button_new_with_label( _("Cancel") );
  gtk_signal_connect( GTK_OBJECT( prw->pCancel ), "clicked", 
			GTK_SIGNAL_FUNC( CancelRolls ), NULL );
  gtk_box_pack_start ( GTK_BOX ( hbox ), prw->pCancel, FALSE, FALSE, 4 );

  gtk_signal_connect ( GTK_OBJECT ( prw->pScale ), "value-changed",
                       GTK_SIGNAL_FUNC ( DepthChanged ), prw );

  /* tree  */

  if( ( prw->ptv = RollsTree ( n, pec, pms ) ) ) {
      gtk_container_add ( GTK_CONTAINER ( prw->psw ), prw->ptv );
      prw->nDepth = n;
  }
  
  /* modality */

  gtk_window_set_default_size( GTK_WINDOW( prw->pDialog ), 560, 400 ); 
  gtk_window_set_modal( GTK_WINDOW( prw->pDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( prw->pDialog ),
                                GTK_WINDOW( pwMain ) );

  gtk_widget_show_all( prw->pDialog );

  gtk_signal_connect( GTK_OBJECT( prw->pDialog ), "delete_event",
                      GTK_SIGNAL_FUNC(RollsClose), prw );
  gtk_signal_connect( GTK_OBJECT( prw->pDialog ), "destroy",
                      GTK_SIGNAL_FUNC(gtk_main_quit), NULL );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();
  
#endif

}
