/*
 * gtkmovelist.c
 * by Jon Kinsey, 2005
 *
 * Analysis move list
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

#include <config.h>

#include <gtk/gtk.h>
#include "eval.h"
#include "gtkchequer.h"
#include "i18n.h"
#include "backgammon.h"
#include "format.h"
#include "assert.h"
#include "gtkmovelistctrl.h"
#include "gtkgame.h"
#include "drawboard.h"

#if USE_GTK2
extern void HintDoubleClick(GtkTreeView        *treeview,
                       GtkTreePath        *path,
                       GtkTreeViewColumn  *col,
                       hintdata *phd);
extern void HintSelect(GtkTreeSelection *selection, hintdata *phd);
#endif

extern GdkColor wlCol;
void ShowMove ( hintdata *phd, const int f );

#define DETAIL_COLUMN_COUNT 11
#define MIN_COLUMN_COUNT 5

#if USE_GTK2
enum
{
  ML_COL_RANK = 0,
  ML_COL_TYPE,
  ML_COL_WIN,
  ML_COL_GWIN,
  ML_COL_BGWIN,
  ML_COL_LOSS,
  ML_COL_GLOSS,
  ML_COL_BGLOSS,
  ML_COL_EQUITY,
  ML_COL_DIFF,
  ML_COL_MOVE,
  ML_COL_FGCOL,
  ML_COL_DATA
} ;
#endif

void MoveListUpdate ( const hintdata *phd );

void MoveListCreate(hintdata *phd)
{
    static char *aszTitleDetails[] = {
	N_("Rank"), 
        N_("Type"), 
        N_("Win"), 
        N_("W g"), 
        N_("W bg"), 
        N_("Lose"), 
        N_("L g"), 
        N_("L bg"),
       NULL, 
        N_("Diff."), 
        N_("Move")
    };
	int i;
	int showWLTree = showMoveListDetail && !phd->fDetails;

/* Create list widget */
#if USE_GTK2
	GtkListStore *store;
	GtkTreeIter iter;
	GtkTreeSelection* sel;
	GtkWidget *view = gtk_tree_view_new();
	int offset = (phd->fDetails) ? 0 : MIN_COLUMN_COUNT - DETAIL_COLUMN_COUNT;

	if (showWLTree)
	{
		GtkStyle *psDefault = gtk_widget_get_style(view);

		GtkCellRenderer *renderer = custom_cell_renderer_movelist_new();
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_RANK], renderer, "movelist", 0, "rank", 1, NULL);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
		g_object_set(renderer, "cell-background-gdk", &psDefault->bg[GTK_STATE_NORMAL],
               "cell-background-set", TRUE, NULL);

		g_object_set_data(G_OBJECT(view), "hintdata", phd);
	}
	else
	{
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_RANK], gtk_cell_renderer_text_new(), "text", ML_COL_RANK, "foreground", ML_COL_FGCOL + offset, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_TYPE], gtk_cell_renderer_text_new(), "text", ML_COL_TYPE, "foreground", ML_COL_FGCOL + offset, NULL);

		if (phd->fDetails)
		{
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_WIN], gtk_cell_renderer_text_new(), "text", ML_COL_WIN, "foreground", ML_COL_FGCOL + offset, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_GWIN], gtk_cell_renderer_text_new(), "text", ML_COL_GWIN, "foreground", ML_COL_FGCOL + offset, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_BGWIN], gtk_cell_renderer_text_new(), "text", ML_COL_BGWIN, "foreground", ML_COL_FGCOL + offset, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_LOSS], gtk_cell_renderer_text_new(), "text", ML_COL_LOSS, "foreground", ML_COL_FGCOL + offset, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_GLOSS], gtk_cell_renderer_text_new(), "text", ML_COL_GLOSS, "foreground", ML_COL_FGCOL + offset, NULL);
			gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_BGLOSS], gtk_cell_renderer_text_new(), "text", ML_COL_BGLOSS, "foreground", ML_COL_FGCOL + offset, NULL);
		}

		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_EQUITY], gtk_cell_renderer_text_new(), "text", ML_COL_EQUITY + offset, "foreground", ML_COL_FGCOL + offset, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_DIFF], gtk_cell_renderer_text_new(), "text", ML_COL_DIFF + offset, "foreground", ML_COL_FGCOL + offset, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, aszTitleDetails[ML_COL_MOVE], gtk_cell_renderer_text_new(), "text", ML_COL_MOVE + offset, "foreground", ML_COL_FGCOL + offset, NULL);
	}

	phd->pwMoves = view;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	g_signal_connect(view, "row-activated", G_CALLBACK(HintDoubleClick), phd);
	g_signal_connect(sel, "changed", G_CALLBACK(HintSelect), phd);

#else
    static char *aszEmpty[] = { NULL, NULL, NULL, NULL, 
                                NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    char *aszTemp[DETAIL_COLUMN_COUNT];
	int numCols;

    /* set titles */

	if (phd->fDetails)
	{
		numCols = DETAIL_COLUMN_COUNT;
      for ( i = 0; i < numCols; i++ )
        aszTemp[ i ] = aszTitleDetails[ i ] ? gettext ( aszTitleDetails[ i ] ) : NULL;
    }
    else
	{
		numCols = MIN_COLUMN_COUNT;
      for ( i = 0; i < numCols; i++ )
        aszTemp[ i ] = aszTitle[ i ] ? gettext ( aszTitle[ i ] ) : NULL;
    }

	phd->pwMoves = gtk_clist_new_with_titles(numCols, aszTemp);

    for (i = 0; i < numCols; i++)
	{
	gtk_clist_set_column_auto_resize( GTK_CLIST( phd->pwMoves ), i, TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( phd->pwMoves ), i,
					    (i == 1 || i == (numCols - 1)) ? GTK_JUSTIFY_LEFT : GTK_JUSTIFY_RIGHT );
    }
    gtk_clist_column_titles_passive( GTK_CLIST( phd->pwMoves ) );
    gtk_clist_set_selection_mode( GTK_CLIST( phd->pwMoves ),
				  GTK_SELECTION_MULTIPLE );

    gtk_selection_add_target( phd->pwMoves, GDK_SELECTION_PRIMARY,
			      GDK_SELECTION_TYPE_STRING, 0 );

    gtk_signal_connect( GTK_OBJECT( phd->pwMoves ), "select-row",
			GTK_SIGNAL_FUNC( HintSelect ), phd );
    gtk_signal_connect( GTK_OBJECT( phd->pwMoves ), "unselect-row",
			GTK_SIGNAL_FUNC( HintSelect ), phd );
    gtk_signal_connect( GTK_OBJECT( phd->pwMoves ), "selection_clear_event",
			GTK_SIGNAL_FUNC( MoveListClearSelection ), phd );
    gtk_signal_connect( GTK_OBJECT( phd->pwMoves ), "selection_get",
			GTK_SIGNAL_FUNC( HintGetSelection ), phd );
#endif

/* Add empty rows */
#if USE_GTK2
	if (phd->fDetails)
		store = gtk_list_store_new(DETAIL_COLUMN_COUNT + 2, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	else
	{
		if (showWLTree)
			store = gtk_list_store_new(2, G_TYPE_POINTER, G_TYPE_INT);
		else
			store = gtk_list_store_new(MIN_COLUMN_COUNT + 2, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	}

	for (i = 0; i < phd->pml->cMoves; i++)
		gtk_list_store_append(store, &iter);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
#else
	for (i = 0; i < phd->pml->cMoves; i++)
		gtk_clist_append( GTK_CLIST( phd->pwMoves ), aszEmpty );
#endif

    MoveListUpdate(phd);
}

float rBest;
int aanColumns[][ 2 ] = {
    { 2, OUTPUT_WIN },
    { 3, OUTPUT_WINGAMMON },
    { 4, OUTPUT_WINBACKGAMMON },
    { 6, OUTPUT_LOSEGAMMON },
    { 7, OUTPUT_LOSEBACKGAMMON }
  };

GtkStyle *psHighlight = NULL;

extern GtkWidget *pwMoveAnalysis;

void MoveListRefreshSize()
{
	custom_cell_renderer_invalidate_size();
	if (pwMoveAnalysis)
	{
		hintdata *phd = gtk_object_get_user_data(GTK_OBJECT(pwMoveAnalysis));
		MoveListUpdate(phd);
	}
}

/*
 * Call UpdateMostList to update the movelist in the GTK hint window.
 * For example, after new evaluations, rollouts or toggle of MWC/Equity.
 *
 */
void MoveListUpdate ( const hintdata *phd )
{
  int i, j;
  char sz[ 32 ];
  cubeinfo ci;
  movelist *pml = phd->pml;
  int col = phd->fDetails ? 8 : 2;
  int showWLTree = showMoveListDetail && !phd->fDetails;

#if USE_GTK2
	int offset = (phd->fDetails) ? 0 : MIN_COLUMN_COUNT - DETAIL_COLUMN_COUNT;
	GtkTreeIter iter;
	GtkListStore *store;
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(phd->pwMoves)));
	gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
#endif

if (!psHighlight)
{	/* Get highlight style first time in */
	GtkStyle *psTemp;
    GtkStyle *psMoves = gtk_widget_get_style(phd->pwMoves);
    GetStyleFromRCFile(&psHighlight, "move-done", psMoves);
    /* Use correct background colour when selected */
    memcpy(&psHighlight->bg[GTK_STATE_SELECTED], &psMoves->bg[GTK_STATE_SELECTED], sizeof(GdkColor));

	/* Also get colour to use for w/l stats in detail view */
    GetStyleFromRCFile(&psTemp, "move-winlossfg", psMoves);
    memcpy(&wlCol, &psTemp->fg[GTK_STATE_NORMAL], sizeof(GdkColor));
}

  /* This function should only be called when the game state matches
     the move list. */
  assert( ms.fMove == 0 || ms.fMove == 1 );
    
  GetMatchStateCubeInfo( &ci, &ms );
  rBest = pml->amMoves[ 0 ].rScore;

#if USE_GTK2
	if (!showWLTree)
		gtk_tree_view_column_set_title(gtk_tree_view_get_column(GTK_TREE_VIEW(phd->pwMoves), col),
			(fOutputMWC && ms.nMatchTo) ? _("MWC") : _("Equity"));
#else
	gtk_clist_set_column_title(GTK_CLIST(phd->pwMoves), col,
		(fOutputMWC && ms.nMatchTo) ? _("MWC") : _("Equity"));
#endif

  for( i = 0; i < pml->cMoves; i++ )
  {
    float *ar = pml->amMoves[ i ].arEvalMove;
	int rankKnown;

#if USE_GTK2
	if (showWLTree)
		gtk_list_store_set(store, &iter, 0, pml->amMoves + i, -1);
	else
		gtk_list_store_set(store, &iter, ML_COL_DATA + offset, pml->amMoves + i, -1);
#else
    gtk_clist_set_row_data( GTK_CLIST(phd->pwMoves), i, pml->amMoves + i );
#endif

	rankKnown = 1;
    if( i && i == pml->cMoves - 1 && phd->piHighlight && i == *phd->piHighlight )
      /* The move made is the last on the list.  Some moves might
         have been deleted to fit this one in */
	{
		/* Lets count how many moves are possible to see if this is the last move */
		movelist ml;
		if (!ms.anDice[0])
		{	/* If the dice have got lost, try to find them */
			moverecord* pmr = (moverecord*)plLastMove->p;
			if (pmr)
			{
				ms.anDice[0] = pmr->anDice[0];
				ms.anDice[1] = pmr->anDice[1];
			}
		}
		GenerateMoves(&ml, ms.anBoard, ms.anDice[0], ms.anDice[1], FALSE);
		if (i < ml.cMoves - 1)
			rankKnown = 0;
	}

	if (rankKnown)
      sprintf( sz, "%d", i + 1 );
    else
      strcpy( sz, "??" );

#if USE_GTK2
	if (showWLTree)
	{
		gtk_list_store_set(store, &iter, 1, rankKnown ? i + 1 : -1, -1);
		goto skipoldcode;
	}
	else
		gtk_list_store_set(store, &iter, ML_COL_RANK, sz, -1);
#else
    gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, 0, sz );
#endif

    FormatEval( sz, &pml->amMoves[ i ].esMove );
#if USE_GTK2
	gtk_list_store_set(store, &iter, ML_COL_TYPE, sz, -1);
#else
    gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, 1, sz );
#endif

    /* gwc */
    if ( phd->fDetails )
	{
		for( j = 0; j < 5; j++ ) 
		{
#if USE_GTK2
			gtk_list_store_set(store, &iter, ML_COL_WIN + j, OutputPercent( ar[ aanColumns[ j ][ 1 ] ] ), -1);
#else
		    gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, aanColumns[ j ][ 0 ],
                            OutputPercent( ar[ aanColumns[ j ][ 1 ] ] ) );
#endif
		  }
#if USE_GTK2
		gtk_list_store_set(store, &iter, ML_COL_WIN + j, OutputPercent(1.0f - ar[ aanColumns[ OUTPUT_WIN ][ 1 ] ] ), -1);
#else
		gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, 5, 
                          OutputPercent( 1.0f - ar[ OUTPUT_WIN ] ) );
#endif
	}

    /* cubeless equity */
#if USE_GTK2
	gtk_list_store_set(store, &iter, ML_COL_EQUITY + offset,
                        OutputEquity( pml->amMoves[ i ].rScore, &ci, TRUE ), -1);
#else
    gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, col, 
                        OutputEquity( pml->amMoves[ i ].rScore, &ci, TRUE ) );
#endif

	if (i != 0)
	{
#if USE_GTK2
		gtk_list_store_set(store, &iter, ML_COL_DIFF + offset,
                          OutputEquityDiff( pml->amMoves[ i ].rScore, 
                                            rBest, &ci ), -1);
#else
		gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, col + 1, 
                          OutputEquityDiff( pml->amMoves[ i ].rScore, 
                                            rBest, &ci ) );
#endif
    }
	
#if USE_GTK2
	gtk_list_store_set(store, &iter, ML_COL_MOVE + offset,
                        FormatMove( sz, ms.anBoard,
                                    pml->amMoves[ i ].anMove ), -1);

	/* highlight row */
	if (phd->piHighlight && *phd->piHighlight == i)
	{
		char buf[20];
		sprintf(buf, "#%02x%02x%02x", psHighlight->fg[GTK_STATE_SELECTED].red / 256, psHighlight->fg[GTK_STATE_SELECTED].green / 256, psHighlight->fg[GTK_STATE_SELECTED].blue / 256);
		gtk_list_store_set(store, &iter, ML_COL_FGCOL + offset, buf, -1);
	}
#else
    gtk_clist_set_text( GTK_CLIST(phd->pwMoves), i, col + 2,
                        FormatMove( sz, ms.anBoard,
                                    pml->amMoves[ i ].anMove ) );
#endif
#if USE_GTK2
skipoldcode:	/* Messy as 3 copies of code at moment... */
	gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
#endif
  }

#if !USE_GTK2
  /* highlight row */
  if (phd->piHighlight && *phd->piHighlight >= 0)
  {
    for ( i = 0; i < pml->cMoves; i++ )
      gtk_clist_set_row_style( GTK_CLIST(phd->pwMoves), i, i == *phd->piHighlight ?
				 psHighlight : NULL );
  }
#endif

  /* update storedmoves global struct */
  UpdateStoredMoves ( pml, &ms );
}

int MoveListGetSelectionCount(const hintdata *phd)
{
#if USE_GTK2
	return gtk_tree_selection_count_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(phd->pwMoves)));
#else
	return g_list_length(GTK_CLIST( phd->pwMoves )->selection);
#endif
}

GList *MoveListGetSelectionList(const hintdata *phd)
{
#if USE_GTK2
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(phd->pwMoves));
	GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(phd->pwMoves));
	return gtk_tree_selection_get_selected_rows(sel, &model);
#else
	return GTK_CLIST( phd->pwMoves )->selection;
#endif
}

void MoveListFreeSelectionList(GList *pl)
{
#if USE_GTK2
	g_list_foreach (pl, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(pl);
#endif
}

move *MoveListGetMove(const hintdata *phd, GList *pl)
{
#if USE_GTK2
	move *m;
	int showWLTree = showMoveListDetail && !phd->fDetails;
	int col, offset = (phd->fDetails) ? 0 : MIN_COLUMN_COUNT - DETAIL_COLUMN_COUNT;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(phd->pwMoves));

	gboolean check = gtk_tree_model_get_iter(model, &iter, (GtkTreePath*)(pl->data));
	assert(check);

	if (showWLTree)
		col = 0;
	else
		col = ML_COL_DATA + offset;
	gtk_tree_model_get(model, &iter, col, &m, -1);
	return m;
#else
	return &phd->pml->amMoves[ GPOINTER_TO_INT ( pl->data ) ];
#endif
}

void MoveListShowToggledClicked(GtkWidget *pw, hintdata *phd)
{
	int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( phd->pwShow ) );
#if USE_GTK2
	if (f)
		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(phd->pwMoves)), GTK_SELECTION_SINGLE);
	else
		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(phd->pwMoves)), GTK_SELECTION_MULTIPLE);

	ShowMove(phd, f);
#else
	int selrow = -1;
	GList *plSelList = MoveListGetSelectionList(phd);

	if ( f )
	{
		if (g_list_length(plSelList) == 1)
			selrow = GPOINTER_TO_INT(plSelList->data);

		/* allow only one move to be selected when "Show" is active */
		gtk_clist_set_selection_mode( GTK_CLIST( phd->pwMoves ),
				  GTK_SELECTION_SINGLE );
	}
  else
    gtk_clist_set_selection_mode( GTK_CLIST( phd->pwMoves ),
				  GTK_SELECTION_MULTIPLE );

	if (selrow != -1)
	{	/* Show single selcted move when show clicked */
		gtk_clist_select_row( GTK_CLIST( phd->pwMoves ), selrow, 0 );
		ShowMove ( phd, TRUE );
	}
	else
		ShowMove ( phd, FALSE );

	MoveListFreeSelectionList(plSelList);
#endif
}

gint MoveListClearSelection( GtkWidget *pw, GdkEventSelection *pes, hintdata *phd )
{
#if USE_GTK2
	gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(phd->pwMoves)));
#else
	gtk_clist_unselect_all( GTK_CLIST( phd->pwMoves ) );
#endif

    return TRUE;
}
