/*
 * gtkchequer.c
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
#include <assert.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "positionid.h"
#include "rollout.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkchequer.h"
#include "i18n.h"
#include "gtktempmap.h"




/*
 * Call UpdateMostList to update the movelist in the GTK hint window.
 * For example, after new evaluations, rollouts or toggle of MWC/Equity.
 *
 */

static void
UpdateMoveList ( const hintdata *phd ) {

  static int aanColumns[][ 2 ] = {
    { 2, OUTPUT_WIN },
    { 3, OUTPUT_WINGAMMON },
    { 4, OUTPUT_WINBACKGAMMON },
    { 6, OUTPUT_LOSEGAMMON },
    { 7, OUTPUT_LOSEBACKGAMMON }
  };

  GtkWidget *pwMoves = phd->pwMoves;
  int i, j;
  char sz[ 32 ];
  float rBest;
  cubeinfo ci;
  movelist *pml = phd->pml;
  int *piHighlight = phd->piHighlight;

  /* This function should only be called when the game state matches
     the move list. */

  assert( ms.fMove == 0 || ms.fMove == 1 );
    
  GetMatchStateCubeInfo( &ci, &ms );
    
  if( fOutputMWC && ms.nMatchTo ) {
    gtk_clist_set_column_title( GTK_CLIST( pwMoves ), 8, _("MWC") );
    rBest = 100.0f * eq2mwc ( pml->amMoves[ 0 ].rScore, &ci );
  } else {
    gtk_clist_set_column_title( GTK_CLIST( pwMoves ), 8, _("Equity") );
    rBest = pml->amMoves[ 0 ].rScore;
  }
    
  for( i = 0; i < pml->cMoves; i++ ) {
    float *ar = pml->amMoves[ i ].arEvalMove;

    gtk_clist_set_row_data( GTK_CLIST( pwMoves ), i, pml->amMoves + i );

    if( i && i == pml->cMoves - 1 && piHighlight && i == *piHighlight )
      /* The move made is the last on the list.  Some moves might
         have been deleted to fit this one in, so its rank isn't
         known. */
      strcpy( sz, "??" );
    else
      sprintf( sz, "%d", i + 1 );
    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 0, sz );

    FormatEval( sz, &pml->amMoves[ i ].esMove );
    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 1, sz );

    for( j = 0; j < 5; j++ ) {
      if( fOutputWinPC )
        sprintf( sz, "%5.1f%%", ar[ aanColumns[ j ][ 1 ] ] * 100.0f );
      else
        sprintf( sz, "%5.3f", ar[ aanColumns[ j ][ 1 ] ] );
	    
      gtk_clist_set_text( GTK_CLIST( pwMoves ), i, aanColumns[ j ][ 0 ],
                          sz );
    }

    if( fOutputWinPC )
      sprintf( sz, "%5.1f%%", ( 1.0f - ar[ OUTPUT_WIN ] ) * 100.0f );
    else
      sprintf( sz, "%5.3f", 1.0f - ar[ OUTPUT_WIN ] );
	    
    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 5, sz );

    if( fOutputMWC && ms.nMatchTo )
      sprintf( sz, "%7.3f%%", 100.0f * eq2mwc( pml->amMoves[ i ].rScore,
                                               &ci ) );
    else
      sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore );
    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 8, sz );

    if( i ) {
      if( fOutputMWC && ms.nMatchTo )
        sprintf( sz, "%7.3f%%", eq2mwc( pml->amMoves[ i ].rScore, &ci )
                 * 100.0f - rBest );
      else
        sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore - rBest );
      gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 9, sz );
    }
	
    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 10,
                        FormatMove( sz, ms.anBoard,
                                    pml->amMoves[ i ].anMove ) );
  }

  /* highlight row */

  if( piHighlight && *piHighlight >= 0 ) {
    GtkStyle *ps;

    ps = gtk_style_copy( gtk_rc_get_style( pwMoves ) );
    
    ps->fg[ GTK_STATE_NORMAL ].red = ps->fg[ GTK_STATE_ACTIVE ].red =
      ps->fg[ GTK_STATE_SELECTED ].red = Highlightrgb[0];
    ps->fg[ GTK_STATE_NORMAL ].green = ps->fg[ GTK_STATE_ACTIVE ].green =
      ps->fg[ GTK_STATE_SELECTED ].green = Highlightrgb[1];
    ps->fg[ GTK_STATE_NORMAL ].blue = ps->fg[ GTK_STATE_ACTIVE ].blue =
      ps->fg[ GTK_STATE_SELECTED ].blue = Highlightrgb[2];

    for ( i = 0; i < pml->cMoves; i++ )
	gtk_clist_set_row_style( GTK_CLIST( pwMoves ), i, i == *piHighlight ?
				 ps : NULL );
    
    gtk_style_unref( ps );
  }
    
  /* update storedmoves global struct */

  UpdateStoredMoves ( pml, &ms );

}


static void 
MoveListRollout( GtkWidget *pw, hintdata *phd ) {


  GList *pl;
  GtkWidget *pwMoves = phd->pwMoves;
  cubeinfo ci;
#if HAVE_ALLOCA
  char ( *asz )[ 40 ];
#else
  char asz[ 10 ][ 40 ];
#endif
  int c;
  int i;

  int *ai;

  if ( !  GTK_CLIST( pwMoves )->selection )
    return;

  GetMatchStateCubeInfo( &ci, &ms );
  
  for(  c = 0, pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next )
    c++;

  /* setup rollout dialog */

#if HAVE_ALLOCA
    asz = alloca( 40 * c );
#else
    if( c > 10 )
	c = 10;
#endif


  for( i = 0, pl = GTK_CLIST( pwMoves )->selection; i < c; 
       pl = pl->next, i++ ) {

    move *m = &phd->pml->amMoves[ GPOINTER_TO_INT ( pl->data ) ];

    FormatMove ( asz[ i ], ms.anBoard, m->anMove );

  }

#if USE_GTK
  if( fX )
    GTKRollout( c, asz, rcRollout.nTrials, NULL ); 
#endif

  ProgressStartValue( _("Rolling out positions; position:"), c );

  for( i = 0, pl = GTK_CLIST( pwMoves )->selection; i < c; 
       pl = pl->next, i++ ) {

    move *m = &phd->pml->amMoves[ GPOINTER_TO_INT ( pl->data ) ];

    GTKRolloutRow ( i );

    if ( ScoreMoveRollout ( m, &ci, &rcRollout ) < 0 ) {
      GTKRolloutDone ();
      ProgressEnd ();
      return;
    }
    
    ProgressValueAdd ( 1 );

    /* Calling RefreshMoveList here requires some extra work, as
       it may reorder moves */

    UpdateMoveList ( phd );

  }

  GTKRolloutDone ();

  gtk_clist_unselect_all ( GTK_CLIST ( pwMoves ) );

  ai = (int *) malloc ( phd->pml->cMoves * sizeof ( int ) );
  RefreshMoveList ( phd->pml, ai );

  if ( phd->piHighlight && phd->pml->cMoves ) 
    *phd->piHighlight = ai [ *phd->piHighlight ];

  free ( ai );


  UpdateMoveList ( phd );

  ProgressEnd ();

}


static void
ShowMove ( hintdata *phd, const int f ) {

  move *pm;
  char *sz;
  int i;
  GtkWidget *pwMoves = phd->pwMoves;
  int anBoard[ 2 ][ 25 ];
  if ( f ) {

    assert( GTK_CLIST( pwMoves )->selection );

    /* the button is toggled */
  
    i = GPOINTER_TO_INT( GTK_CLIST( pwMoves )->selection->data );
    pm = (move * ) gtk_clist_get_row_data( GTK_CLIST( pwMoves ), i );

    memcpy ( anBoard, ms.anBoard, sizeof ( anBoard ) );
    ApplyMove ( anBoard, pm->anMove, FALSE );

    if ( ! ms.fMove )
      SwapSides ( anBoard );

    UpdateMove( ( BOARD( pwBoard ) )->board_data, anBoard );

  }
  else {

    sz = g_strdup ( "show board" );
    UserCommand( sz );
    g_free ( sz );

  }

}


static void
MoveListTempMapClicked( GtkWidget *pw, hintdata *phd ) {

  GList *pl;
  GtkWidget *pwMoves = phd->pwMoves;
  char szMove[ 100 ];
  matchstate *ams;
  int i, c;
  gchar **asz;

  if ( !  GTK_CLIST( pwMoves )->selection )
    return;

  for( c = 0, pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next, ++c ) 
    ;

  ams = (matchstate *) g_malloc( c * sizeof ( matchstate ) );
  asz = (char **) g_malloc ( c * sizeof ( char * ) );

  for( i = 0, pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next, ++i ) {
  
    move *m = &phd->pml->amMoves[ GPOINTER_TO_INT ( pl->data ) ];

    /* Apply move to get board */

    memcpy( &ams[ i ], &ms, sizeof ( matchstate ) );

    FormatMove( szMove, ams[ i ].anBoard, m->anMove );
    ApplyMove( ams[ i ].anBoard, m->anMove, FALSE );

    /* Swap sides */

    SwapSides( ams[ i ].anBoard );
    ams[ i ].fMove = ! ams[ i ].fMove;
    ams[ i ].fTurn = ! ams[ i ].fTurn;

    /* Show temp map dialog */

    asz[ i ] = g_strdup( szMove );

  }

  GTKShowTempMap( ams, c, ( const gchar** ) asz, TRUE );

  g_free( ams );
  for ( i = 0; i < c; ++i )
    g_free( asz[ i ] );
  g_free( asz );

  
}



static void
MoveListShowToggled ( GtkWidget *pw, hintdata *phd ) {

  int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( phd->pwShow ) );
  int c = CheckHintButtons ( phd );
  GtkWidget *pwMoves = phd->pwMoves;


  /* allow only one move to be selected when "Show" is active */

  if ( f )
    gtk_clist_set_selection_mode( GTK_CLIST( pwMoves ),
				  GTK_SELECTION_SINGLE );
  else
    gtk_clist_set_selection_mode( GTK_CLIST( pwMoves ),
				  GTK_SELECTION_MULTIPLE );


  c = CheckHintButtons( phd );

  if ( f && c == 1 )
    ShowMove ( phd, TRUE );
  else
    ShowMove ( phd, FALSE );
  
}


static void
MoveListMWC ( GtkWidget *pw, hintdata *phd ) {

  char sz[ 80 ];
  int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );

  if ( f != fOutputMWC ) {
    sprintf ( sz, "set output mwc %s", fOutputMWC ? "off" : "on" );
    
    UserCommand ( sz );
  }

  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pw ), fOutputMWC );

  UpdateMoveList ( phd );

}




static void
EvalMoves ( hintdata *phd, evalcontext *pec ) {

  GList *pl;
  GtkWidget *pwMoves = phd->pwMoves;
  cubeinfo ci;
  int *ai;

  if ( !  GTK_CLIST( pwMoves )->selection )
    return;

  GetMatchStateCubeInfo( &ci, &ms );
  
  ProgressStart( _("Evaluating positions...") );

  for(  pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next ) {

    if ( ScoreMove ( &phd->pml->amMoves[ GPOINTER_TO_INT ( pl->data ) ],
                     &ci, pec, pec->nPlies ) < 0 ) {
      ProgressEnd ();
      return;
    }

    /* Calling RefreshMoveList here requires some extra work, as
       it may reorder moves */

    UpdateMoveList ( phd );

  }

  gtk_clist_unselect_all ( GTK_CLIST ( pwMoves ) );

  ai = (int *) malloc ( phd->pml->cMoves * sizeof ( int ) );
  RefreshMoveList ( phd->pml, ai );

  if ( phd->piHighlight && phd->pml->cMoves ) 
    *phd->piHighlight = ai [ *phd->piHighlight ];

  free ( ai );

  UpdateMoveList ( phd );

  ProgressEnd ();

}

static void
MoveListEval ( GtkWidget *pw, hintdata *phd ) {

  EvalMoves ( phd, &esEvalChequer.ec );

}


static void
MoveListEvalPly ( GtkWidget *pw, hintdata *phd ) {

  char *szPly = gtk_object_get_data ( GTK_OBJECT ( pw ), "user_data" );
  evalcontext ec = { TRUE, 0, 0, TRUE, 0.0 };

  ec.nPlies = atoi ( szPly );

  EvalMoves ( phd, &ec );

}


static void
MoveListEvalSettings ( GtkWidget *pw, void *unused ) {

  SetEvaluation ( NULL, 0, NULL );

#if GTK_CHECK_VERSION(2,0,0)
  /* bring the dialog holding this button to the top */
  gtk_window_present ( GTK_WINDOW ( gtk_widget_get_toplevel( pw ) ) );
#endif

}

static void
MoveListRolloutSettings ( GtkWidget *pw, void *unused ) {

  SetRollouts ( NULL, 0, NULL );

#if GTK_CHECK_VERSION(2,0,0)
  /* bring the dialog holding this button to the top */
  gtk_window_present ( GTK_WINDOW ( gtk_widget_get_toplevel( pw ) ) );
#endif

}


typedef int ( *cfunc )( const void *, const void * );

static int CompareInts( int *p0, int *p1 ) {

    return *p0 - *p1;
}


static char *
MoveListCopyData ( hintdata *phd ) {

  GtkWidget *pw = phd->pwMoves;
  int c, i;
  GList *pl;
  char *sz, *pch;
  int *an;

  for( c = 0, pl = GTK_CLIST( pw )->selection; pl; pl = pl->next )
    c++;
  
#if HAVE_ALLOCA
  an = alloca( c * sizeof( an[ 0 ] ) );
#else
  an = malloc( c * sizeof( an[ 0 ] ) );
#endif
  sz = malloc( c * 9 * 80 );
    
  *sz = 0;
    
  for( i = 0, pl = GTK_CLIST( pw )->selection; pl;
       pl = pl->next, i++ )
    an[ i ] = GPOINTER_TO_INT( pl->data );
  
  qsort( an, c, sizeof( an[ 0 ] ), (cfunc) CompareInts );
  
  for( i = 0, pch = sz; i < c; i++, pch = strchr( pch, 0 ) )
    FormatMoveHint( pch, &ms, phd->pml, an[ i ], TRUE, TRUE, TRUE );
  
#if !HAVE_ALLOCA
  free( an );
#endif        

  return sz;

}


static void
MoveListMove ( GtkWidget *pw, hintdata *phd ) {

  move m;
  char szMove[ 40 ];
  int i;
  GtkWidget *pwMoves = phd->pwMoves;
  int anBoard[ 2 ][ 25 ];
  
  assert( GTK_CLIST( pwMoves )->selection );
  
  i = GPOINTER_TO_INT( GTK_CLIST( pwMoves )->selection->data );
  memcpy ( &m, (move * ) gtk_clist_get_row_data( GTK_CLIST( pwMoves ), i ),
           sizeof ( move ) );

  memcpy ( anBoard, ms.anBoard, sizeof ( anBoard ) );
  ApplyMove ( anBoard, m.anMove, FALSE );

  if ( ! ms.fMove )
    SwapSides ( anBoard );

  sprintf ( szMove, "show fullboard %s", PositionID ( anBoard ) );
  UserCommand ( szMove );
  
  if ( phd->fDestroyOnMove )
    /* Destroy widget on exit */
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

  FormatMove( szMove, ms.anBoard, m.anMove );
  UserCommand( szMove );

}


static void
MoveListCopy ( GtkWidget *pw, hintdata *phd ) {

  UserCommand ( "xcopy" );

}



static GtkWidget *
CreateMoveListTools ( hintdata *phd ) {

  GtkWidget *pwTools;
  GtkWidget *pwEval = gtk_button_new_with_label ( _("Eval") );
  GtkWidget *pwEvalSettings = gtk_button_new_with_label ( _("...") );
  GtkWidget *pwRollout = gtk_button_new_with_label( _("Rollout") );
  GtkWidget *pwRolloutSettings = gtk_button_new_with_label ( _("...") );
  GtkWidget *pwMWC = gtk_toggle_button_new_with_label( _("MWC") );
  GtkWidget *pwMove = gtk_button_new_with_label ( _("Move") );
  GtkWidget *pwShow = gtk_toggle_button_new_with_label ( _("Show") );
  GtkWidget *pwCopy = gtk_button_new_with_label ( _("Copy") );
  GtkWidget *pwTempMap = gtk_button_new_with_label( _("Temp. Map") );
  GtkWidget *pwply;
  int i;
  char *sz;

  GtkTooltips *pt = gtk_tooltips_new ();

  phd->pwRollout = pwRollout;
  phd->pwRolloutSettings = pwRolloutSettings;
  phd->pwEval = pwEval;
  phd->pwEvalSettings = pwEvalSettings;
  phd->pwMove = pwMove;
  phd->pwShow = pwShow;
  phd->pwCopy = pwCopy;
  phd->pwTempMap = pwTempMap;

  /* toolbox on the left with buttons for eval, rollout and more */
  
  pwTools = gtk_table_new (8, 2, FALSE);
  
  gtk_table_attach (GTK_TABLE (pwTools), pwEval, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_table_attach (GTK_TABLE (pwTools), pwEvalSettings, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  phd->pwEvalPly = gtk_hbox_new ( FALSE, 0 );
  gtk_table_attach (GTK_TABLE (pwTools), phd->pwEvalPly, 0, 2, 1, 2, 
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  for ( i = 0; i < 5; ++i ) {

    sz = g_strdup_printf ( "%d", i ); /* string is freed by set_data_full */
    pwply = gtk_button_new_with_label ( sz );

    gtk_box_pack_start ( GTK_BOX ( phd->pwEvalPly ), pwply, TRUE, TRUE, 0 );

    gtk_signal_connect( GTK_OBJECT( pwply ), "clicked",
                        GTK_SIGNAL_FUNC( MoveListEvalPly ), phd );

    gtk_object_set_data_full ( GTK_OBJECT ( pwply ), "user_data", sz, free );

    sz = g_strdup_printf ( _("Evaluate play on cubeful %d-ply"), i );
    gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwply, sz, sz );
    g_free ( sz );

  }

  gtk_table_attach (GTK_TABLE (pwTools), pwRollout, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_table_attach (GTK_TABLE (pwTools), pwRolloutSettings, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  gtk_table_attach (GTK_TABLE (pwTools), pwMWC, 0, 2, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_table_attach (GTK_TABLE (pwTools), pwCopy, 0, 2, 4, 5,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  gtk_table_attach (GTK_TABLE (pwTools), pwShow, 0, 2, 5, 6,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  gtk_table_attach (GTK_TABLE (pwTools), pwMove, 0, 2, 6, 7,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  gtk_table_attach (GTK_TABLE (pwTools), pwTempMap, 0, 2, 7, 8,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  
  gtk_widget_set_sensitive( pwMWC, ms.nMatchTo );
  gtk_widget_set_sensitive( pwMove, FALSE );
  gtk_widget_set_sensitive( pwCopy, FALSE );
  gtk_widget_set_sensitive( pwTempMap, FALSE );
  
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( pwMWC ),
                                 fOutputMWC );

  /* signals */

  gtk_signal_connect( GTK_OBJECT( pwRollout ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListRollout ), phd );
  gtk_signal_connect( GTK_OBJECT( pwEval ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListEval ), phd );
  gtk_signal_connect( GTK_OBJECT( pwEvalSettings ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListEvalSettings ), NULL );
  gtk_signal_connect( GTK_OBJECT( pwRolloutSettings ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListRolloutSettings ), NULL );
  gtk_signal_connect( GTK_OBJECT( pwMWC ), "toggled",
                      GTK_SIGNAL_FUNC( MoveListMWC ), phd );
  gtk_signal_connect( GTK_OBJECT( pwMove ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListMove ), phd );
  gtk_signal_connect( GTK_OBJECT( pwShow ), "toggled",
                      GTK_SIGNAL_FUNC( MoveListShowToggled ), phd );
  gtk_signal_connect( GTK_OBJECT( pwCopy ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListCopy ), phd );
  gtk_signal_connect( GTK_OBJECT( pwTempMap ), "clicked",
                      GTK_SIGNAL_FUNC( MoveListTempMapClicked ), phd );

  /* tool tips */

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwRollout,
                         _("Rollout chequer play with current settings"),
                         _("Rollout chequer play with current settings") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwEval,
                         _("Evaluate chequer play with current settings"),
                         _("Evaluate chequer play with current settings") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwRolloutSettings,
                         _("Modify rollout settings"),
                         _("Modify rollout settings") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwEvalSettings,
                         _("Modify evaluation settings"),
                         _("Modify evaluation settings") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwMWC,
                         _("Toggle output as MWC or equity"),
                         _("Toggle output as MWC or equity") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwCopy,
                         _("Copy selected moves to clipboard"),
                         _("Copy selected moves to clipboard") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwMove,
                         _("Move the selected move"),
                         _("Move the selected move") );

  gtk_tooltips_set_tip ( GTK_TOOLTIPS ( pt ), pwTempMap,
                         _("Show Sho Sengoku Temperature Map of position "
                           "after selected move"),
                         "" );


  return pwTools;
  
}


static void HintSelect( GtkWidget *pw, int y, int x, GdkEventButton *peb,
			hintdata *phd ) {
    
    int c = CheckHintButtons( phd );

    if( c && peb )
	gtk_selection_owner_set( pw, GDK_SELECTION_PRIMARY, peb->time );

    /* Double clicking a row makes that move. */
    if( c == 1 && peb && peb->type == GDK_2BUTTON_PRESS && 
        phd->fButtonsValid ) {
      gtk_button_clicked( GTK_BUTTON( phd->pwMove ) );
      return;
    }

    /* show moves */

    if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( phd->pwShow ) ) ) {
      switch ( c ) {
      case 0:
      case 1:
        ShowMove ( phd, c );
        break;

      default:
        ShowMove ( phd, FALSE );
        break;

      }

    }
    
}

static gint HintClearSelection( GtkWidget *pw, GdkEventSelection *pes,
				hintdata *phd ) {
    
    gtk_clist_unselect_all( GTK_CLIST( pw ) );

    return TRUE;
}


static void HintGetSelection( GtkWidget *pw, GtkSelectionData *psd,
			      guint n, guint t, hintdata *phd ) {
  char *pc = MoveListCopyData ( phd );
    
  gtk_selection_data_set( psd, GDK_SELECTION_TYPE_STRING, 8,
                          pc, strlen( pc ) );
  
  free ( pc );

}


extern int 
CheckHintButtons( hintdata *phd ) {

    int c;
    GList *pl;
    GtkWidget *pw = phd->pwMoves;

    for( c = 0, pl = GTK_CLIST( pw )->selection; c < 2 && pl; pl = pl->next )
	c++;

    gtk_widget_set_sensitive( phd->pwMove, c == 1 && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwCopy, c && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwTempMap, c && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwRollout, c && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwEval, c && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwEvalPly, c && phd->fButtonsValid );

    return c;
}



extern GtkWidget *
CreateMoveList( movelist *pml, int *piHighlight, const int fButtonsValid,
                const int fDestroyOnMove ) {

    static char *aszTitle[] = {
	N_("Rank"), 
        N_("Type"), 
        N_("Win"), 
        N_("W g"), 
        N_("W bg"), 
        N_("Lose"), 
        N_("L g"), 
        N_("L bg"),
	"", 
        N_("Diff."), 
        N_("Move")
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		       NULL, NULL };
    char *aszTemp[ 11 ];
    GtkWidget *pwMoves;
    GtkWidget *pw;
    GtkWidget *pwHBox;
    int i;

    hintdata *phd = (hintdata *) malloc ( sizeof ( hintdata ) );

    /* set titles */

    phd->piHighlight = piHighlight;
    phd->pml = pml;
    phd->fButtonsValid = fButtonsValid;
    phd->fDestroyOnMove = fDestroyOnMove;
    phd->pwMove = NULL;

    for ( i = 0; i < 11; i++ )
     aszTemp[ i ] = gettext ( aszTitle[ i ] );

    pwMoves = gtk_clist_new_with_titles( 11, aszTemp );

    /* This function should only be called when the game state matches
       the move list. */
    assert( ms.fMove == 0 || ms.fMove == 1 );
    
    for( i = 0; i < 11; i++ ) {
	gtk_clist_set_column_auto_resize( GTK_CLIST( pwMoves ), i, TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( pwMoves ), i,
					    i == 1 || i == 10 ?
					    GTK_JUSTIFY_LEFT :
					    GTK_JUSTIFY_RIGHT );
    }
    gtk_clist_column_titles_passive( GTK_CLIST( pwMoves ) );
    gtk_clist_set_selection_mode( GTK_CLIST( pwMoves ),
				  GTK_SELECTION_MULTIPLE );

    for( i = 0; i < pml->cMoves; i++ )
      gtk_clist_append( GTK_CLIST( pwMoves ), aszEmpty );


    phd->pwMoves = pwMoves;
    UpdateMoveList ( phd );

    pw = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pw ),
                                    GTK_POLICY_NEVER, 
                                    GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( pw ),
                       pwMoves );

    pwHBox = gtk_hbox_new ( FALSE, 0 );

    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, TRUE, TRUE, 0 );
    gtk_box_pack_end ( GTK_BOX ( pwHBox ),
                       CreateMoveListTools( phd ),
                       FALSE, FALSE, 0 );
    
    gtk_selection_add_target( pwMoves, GDK_SELECTION_PRIMARY,
			      GDK_SELECTION_TYPE_STRING, 0 );
    
    gtk_object_set_data_full( GTK_OBJECT( pwHBox ), "user_data", 
                              phd, free );

    HintSelect( pwMoves, 0, 0, NULL, phd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "select-row",
			GTK_SIGNAL_FUNC( HintSelect ), phd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "unselect-row",
			GTK_SIGNAL_FUNC( HintSelect ), phd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "selection_clear_event",
			GTK_SIGNAL_FUNC( HintClearSelection ), phd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "selection_get",
			GTK_SIGNAL_FUNC( HintGetSelection ), phd );
    
    return pwHBox;

}

