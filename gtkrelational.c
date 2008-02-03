/*
 * gtkrelational.c
 *
 * by Christian Anthon <anthon@kiku.dk>, 2006.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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
#if USE_PYTHON
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "gtkgame.h"
#include "gtkrelational.h"
#include "relational.h"
#include "gtkwindows.h"
#include <glib/gi18n.h>

enum {
	COLUMN_NICK,
	COLUMN_GNUE,
	COLUMN_GCHE,
	COLUMN_GCUE,
	COLUMN_SNWE,
	COLUMN_SCHE,
	COLUMN_SCUE,
	COLUMN_WRPA,
	COLUMN_WRTA,
	COLUMN_WDTG,
	COLUMN_WDBD,
	COLUMN_MDAC,
	COLUMN_MDBC,
	COLUMN_LUCK,
	NUM_COLUMNS
};

static gchar *titles[] = {
	N_("Nick"),
	N_("Gnu\nErr"),
	N_("Gnu\nMove"),
	N_("Gnu\nCube"),
	N_("Snw\nErr"),
	N_("Snw\nMove"),
	N_("Snw\nCube"),
	N_("Pass"),
	N_("Take"),
	N_("WDb\nPass"),
	N_("WDb\nTake"),
	N_("MDb\nPass"),
	N_("MDb\nTake"),
	N_("Luck")
};

static int curPlayerId;
static int curRow;
static GtkWidget *pwPlayerName;
static GtkWidget *pwPlayerNotes;
static GtkWidget *pwQueryText;
static GtkWidget *pwQueryResult;
static GtkWidget *pwQueryBox;

#define PACK_OFFSET 4
#define OUTSIDE_FRAME_GAP PACK_OFFSET
#define INSIDE_FRAME_GAP PACK_OFFSET
#define NAME_NOTES_VGAP PACK_OFFSET
#define BUTTON_GAP PACK_OFFSET
#define QUERY_BORDER 1

static GtkTreeModel *create_model(void)
{
	GtkListStore *store;
	GtkTreeIter iter;
	RowSet r;

	long moves[3];
	unsigned int i, j;
	gfloat stats[13];

	/* create list store */
	store = gtk_list_store_new(NUM_COLUMNS,
				   G_TYPE_STRING,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT,
				   G_TYPE_FLOAT, G_TYPE_FLOAT,
				   G_TYPE_FLOAT);

	/* prepare the sql query */
	if (!RunQuery(&r, "name,"
		      "SUM(total_moves),"
		      "SUM(unforced_moves),"
		      "SUM(close_cube_decisions),"
		      "SUM(error_missed_doubles_below_cp_normalised),"
		      "SUM(error_missed_doubles_above_cp_normalised),"
		      "SUM(error_wrong_doubles_below_dp_normalised),"
		      "SUM(error_wrong_doubles_above_tg_normalised),"
		      "SUM(error_wrong_takes_normalised),"
		      "SUM(error_wrong_passes_normalised),"
		      "SUM(cube_error_total_normalised),"
		      "SUM(chequer_error_total_normalised),"
		      "SUM(luck_total_normalised) "
		      "FROM matchstat NATURAL JOIN player group by name"))
		return 0;

	if (r.rows < 2) {
		GTKMessage(_("No data in database"), DT_INFO);
		return 0;
	}

	for (j = 1; j < r.rows; ++j)
	{
		for (i = 1; i < 4; ++i)
			moves[i - 1] = strtol(r.data[j][i], NULL, 0);

		for (i = 4; i < 13; ++i)
			stats[i - 4] = (float)g_strtod(r.data[j][i], NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_NICK, r.data[j][0],
				   COLUMN_GNUE,
				   (stats[6] + stats[7]) / (moves[1] +
							    moves[2]) *
				   1000.0f, COLUMN_GCHE,
				   stats[7] / moves[1] * 1000.0f,
				   COLUMN_GCUE,
				   stats[6] / moves[2] * 1000.0f,
				   COLUMN_SNWE,
				   (stats[6] +
				    stats[7]) / moves[0] * 500.0f,
				   COLUMN_SCHE,
				   stats[7] / moves[0] * 500.0f,
				   COLUMN_SCUE,
				   stats[6] / moves[0] * 500.0f,
				   COLUMN_WRPA,
				   stats[5] / moves[0] * 500.0f,
				   COLUMN_WRTA,
				   stats[4] / moves[0] * 500.0f,
				   COLUMN_WDTG,
				   stats[3] / moves[0] * 500.0f,
				   COLUMN_WDBD,
				   stats[2] / moves[0] * 500.0f,
				   COLUMN_MDAC,
				   stats[1] / moves[0] * 500.0f,
				   COLUMN_MDBC,
				   stats[0] / moves[0] * 500.0f,
				   COLUMN_LUCK,
				   stats[8] / moves[0] * 1000.0f, -1);
	}
	FreeRowset(&r);
	return GTK_TREE_MODEL(store);
}

static void
cell_data_func(GtkTreeViewColumn * col,
	       GtkCellRenderer * renderer,
	       GtkTreeModel * model, GtkTreeIter * iter, gpointer column)
{
	gfloat data;
	gchar buf[20];

	/* get data pointed to by column */
	gtk_tree_model_get(model, iter, GPOINTER_TO_INT(column), &data,
			   -1);

	/* format the data to two digits */
	g_snprintf(buf, sizeof(buf), "%.2f", data);

	/* render the string right aligned */
	g_object_set(renderer, "text", buf, NULL);
	g_object_set(renderer, "xalign", 1.0, NULL);
}

static void add_columns(GtkTreeView * treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	gint i;

	renderer = gtk_cell_renderer_text_new();

	column =
	    gtk_tree_view_column_new_with_attributes("Nick", renderer,
						     "text", COLUMN_NICK,
						     NULL);
	gtk_tree_view_column_set_sort_column_id(column, 0);
	gtk_tree_view_append_column(treeview, column);

	for (i = 1; i < NUM_COLUMNS; i++) {
		column = gtk_tree_view_column_new();
		renderer = gtk_cell_renderer_text_new();
		gtk_tree_view_column_pack_end(column, renderer, TRUE);
		gtk_tree_view_column_set_cell_data_func(column, renderer,
							cell_data_func,
							GINT_TO_POINTER(i),
							NULL);
		gtk_tree_view_column_set_sort_column_id(column, i);
		gtk_tree_view_column_set_title(column, gettext(titles[i]));
		gtk_tree_view_append_column(treeview, column);
	}
}

static GtkWidget *do_list_store(void)
{
	GtkTreeModel *model;
	GtkWidget *treeview;

	model = create_model();
	if (!model)
		return NULL;

	treeview = gtk_tree_view_new_with_model(model);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					COLUMN_NICK);

	g_object_unref(model);

	add_columns(GTK_TREE_VIEW(treeview));

	return treeview;
}

static void view_onRowActivated(GtkTreeView * treeview,
		    GtkTreePath * path,
		    GtkTreeViewColumn * col, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(treeview);

	if (gtk_tree_model_get_iter(model, &iter, path)) {
		gchar *name;
		gtk_tree_model_get(model, &iter, COLUMN_NICK, &name, -1);
		GTKSetCurrentParent(GTK_WIDGET(userdata));
		CommandRelationalShowDetails(name);
		g_free(name);
	}
}

extern void GtkRelationalShowStats(gpointer p, guint n, GtkWidget * pw)
{
	GtkWidget *pwDialog;
	GtkWidget *scrolledwindow1;
	GtkWidget *treeview1;

	pwDialog = GTKCreateDialog(_("GNU Backgammon - Player Stats"),
				   DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL,
				   NULL);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
					    (scrolledwindow1),
					    GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW
				       (scrolledwindow1), GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
					    (scrolledwindow1),
					    GTK_SHADOW_IN);

	treeview1 = do_list_store();
	if (treeview1)
	{
		g_signal_connect(treeview1, "row-activated",
				(GCallback) view_onRowActivated, pwDialog);
		gtk_container_add(GTK_CONTAINER(scrolledwindow1), treeview1);

		gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
				scrolledwindow1);
		gtk_window_set_default_size(GTK_WINDOW(pwDialog), 1, 400);
		gtk_widget_show_all(pwDialog);
		gtk_main();
	}
}

extern void GtkRelationalAddMatch(gpointer p, guint n, GtkWidget * pw)
{
	int exists = RelationalMatchExists();
	if (exists == -1 ||
	    (exists == 1 && !GetInputYN(_("Match exists, overwrite?"))))
		return;

	CommandRelationalAddMatch(NULL);

	outputx();
}

static GtkWidget *GetRelList(RowSet * pRow)
{
	unsigned int i;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	GtkWidget *pwList = gtk_clist_new(pRow->cols);
	gtk_clist_column_titles_show(GTK_CLIST(pwList));
	gtk_clist_column_titles_passive(GTK_CLIST(pwList));
	for (i = 0; i < pRow->cols; i++) {
		char *widthStr = malloc(pRow->widths[i] + 1);
		int width;
		memset(widthStr, 'a', pRow->widths[i]);
		widthStr[pRow->widths[i]] = '\0';

		layout = gtk_widget_create_pango_layout(pwList, widthStr);
		pango_layout_get_pixel_extents(layout, NULL,
					       &logical_rect);
		g_object_unref(layout);
		width = logical_rect.width;
		gtk_clist_set_column_title(GTK_CLIST(pwList), i,
					   pRow->data[0][i]);

		gtk_clist_set_column_width(GTK_CLIST(pwList), i, width);
		gtk_clist_set_column_resizeable(GTK_CLIST(pwList), i,
						FALSE);
	}
	GTK_WIDGET_UNSET_FLAGS(pwList, GTK_CAN_FOCUS);

	for (i = 1; i < pRow->rows; i++) {
		gtk_clist_append(GTK_CLIST(pwList), pRow->data[i]);
	}
	return pwList;
}

static void ClearText(GtkTextView * pwText)
{
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(pwText), "", -1);
}

static void ShowRelationalSelect(GtkWidget * pw, int y, int x,
				 GdkEventButton * peb, GtkWidget * pwCopy)
{
	char *pName;
	RowSet r;
	char query[1024];

	gtk_clist_get_text(GTK_CLIST(pw), y, 0, &pName);

	sprintf(query, "player_id, name, notes"
		" FROM player WHERE player.name = '%s'", pName);

	ClearText(GTK_TEXT_VIEW(pwPlayerNotes));
	if (!RunQuery(&r, query)) {
		gtk_entry_set_text(GTK_ENTRY(pwPlayerName), "");
		return;
	}

	g_assert(r.rows == 2);	/* Should be exactly one entry */

	curRow = y;
	curPlayerId = atoi(r.data[1][0]);
	gtk_entry_set_text(GTK_ENTRY(pwPlayerName), r.data[1][1]);
	gtk_text_buffer_set_text(gtk_text_view_get_buffer
				 (GTK_TEXT_VIEW(pwPlayerNotes)),
				 r.data[1][2], -1);

	FreeRowset(&r);
}

static void RelationalOpen(GtkWidget * pw, GtkWidget * pwList)
{
	char *player;
	char buf[200];

	if (curPlayerId == -1)
		return;

	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);
	sprintf(buf, "Open (%s)", player);
	output(buf);
	outputx();
}

static void RelationalErase(GtkWidget * pw, GtkWidget * pwList)
{
	char *player;
	char buf[200];

	if (curPlayerId == -1)
		return;

	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);

	sprintf(buf, _("Remove all data for %s?"), player);
	if (!GetInputYN(buf))
		return;

	sprintf(buf, "\"%s\"", player);
	CommandRelationalErase(buf);

	gtk_clist_remove(GTK_CLIST(pwList), curRow);
}

static char *GetText(GtkTextView * pwText)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(pwText);
	char *pch;

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	pch = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	return pch;
}

static void UpdatePlayerDetails(GtkWidget *pw, GtkWidget *pwList)
{
	char *notes;
	const char *name;
	if (curPlayerId == -1)
		return;

	notes = GetText(GTK_TEXT_VIEW(pwPlayerNotes));
	name = gtk_entry_get_text(GTK_ENTRY(pwPlayerName));
	if (RelationalUpdatePlayerDetails(curPlayerId, name, notes) != 0)
		gtk_clist_set_text(GTK_CLIST(pwList), curRow, 0, name);

	g_free(notes);
}

static void RelationalQuery(GtkWidget * pw, GtkWidget * pwVbox)
{
	RowSet r;
	char *pch, *query;

	pch = GetText(GTK_TEXT_VIEW(pwQueryText));

	if (!StrNCaseCmp("select ", pch, strlen("select ")))
		query = pch + strlen("select ");
	else
		query = pch;

	if (RunQuery(&r, query)) {
		gtk_widget_destroy(pwQueryResult);
		pwQueryResult = GetRelList(&r);
		gtk_box_pack_start(GTK_BOX(pwQueryBox), pwQueryResult,
				   TRUE, TRUE, 0);
		gtk_widget_show(pwQueryResult);
		FreeRowset(&r);
	}

	g_free(pch);
}

extern void GtkShowRelational(gpointer p, guint n, GtkWidget * pw)
{
	RowSet r;
	GtkWidget *pwRun, *pwList, *pwDialog, *pwHbox2, *pwVbox2,
	    *pwPlayerFrame, *pwUpdate, *pwHbox, *pwVbox, *pwErase, *pwOpen,
	    *pwn, *pwLabel, *pwScrolled;

	curPlayerId = -1;

	pwDialog = GTKCreateDialog(_("GNU Backgammon - Relational Database"),
			    DT_INFO, NULL, DIALOG_FLAG_MODAL | DIALOG_FLAG_MINMAXBUTTONS, NULL, NULL);

	pwn = gtk_notebook_new();
	gtk_container_set_border_width(GTK_CONTAINER(pwn), 0);
	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), pwn);

/*******************************************************
** Start of (left hand side) of player screen...
*******************************************************/

	pwHbox = gtk_hbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(pwn), pwHbox,
				 gtk_label_new(_("Players")));

	pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox),
				       INSIDE_FRAME_GAP);

	if (!RunQuery(&r, "name FROM player ORDER BY name"))
		return;

	if (r.rows < 2) {
		GTKMessage(_("No data in database"), DT_INFO);
		return;
	}

	pwList = GetRelList(&r);
	g_signal_connect(G_OBJECT(pwList), "select-row",
			 G_CALLBACK(ShowRelationalSelect), pwList);
	FreeRowset(&r);

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				       GTK_POLICY_NEVER,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(pwScrolled), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwScrolled, TRUE, TRUE, 0);

	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);

	pwOpen = gtk_button_new_with_label("Open");
	g_signal_connect(G_OBJECT(pwOpen), "clicked",
			 G_CALLBACK(RelationalOpen), pwList);
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwOpen, FALSE, FALSE, 0);

	pwErase = gtk_button_new_with_label("Erase");
	g_signal_connect(G_OBJECT(pwErase), "clicked",
			 G_CALLBACK(RelationalErase), pwList);
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwErase, FALSE, FALSE,
			   BUTTON_GAP);

/*******************************************************
** Start of right hand side of player screen...
*******************************************************/

	pwPlayerFrame = gtk_frame_new("Player");
	gtk_container_set_border_width(GTK_CONTAINER(pwPlayerFrame),
				       OUTSIDE_FRAME_GAP);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwPlayerFrame, TRUE, TRUE, 0);

	pwVbox = gtk_vbox_new(FALSE, NAME_NOTES_VGAP);
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox),
				       INSIDE_FRAME_GAP);
	gtk_container_add(GTK_CONTAINER(pwPlayerFrame), pwVbox);

	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);

	pwLabel = gtk_label_new("Name");
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwLabel, FALSE, FALSE, 0);
	pwPlayerName = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwPlayerName, TRUE, TRUE, 0);

	pwVbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwVbox2, TRUE, TRUE, 0);

	pwLabel = gtk_label_new("Notes");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwVbox2), pwLabel, FALSE, FALSE, 0);

	pwPlayerNotes = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(pwPlayerNotes), TRUE);

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(pwScrolled), pwPlayerNotes);
	gtk_box_pack_start(GTK_BOX(pwVbox2), pwScrolled, TRUE, TRUE, 0);

	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);

	pwUpdate = gtk_button_new_with_label("Update Details");
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwUpdate, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(pwUpdate), "clicked",
			 G_CALLBACK(UpdatePlayerDetails), pwList);

/*******************************************************
** End of right hand side of player screen...
*******************************************************/

	/* Query sheet */
	pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(pwn), pwVbox,
				 gtk_label_new(_("Query")));
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox),
				       INSIDE_FRAME_GAP);

	pwLabel = gtk_label_new("Query text");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwLabel, FALSE, FALSE, 0);

	pwQueryText = gtk_text_view_new();
	gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText),
					     GTK_TEXT_WINDOW_TOP,
					     QUERY_BORDER);
	gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText),
					     GTK_TEXT_WINDOW_RIGHT,
					     QUERY_BORDER);
	gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText),
					     GTK_TEXT_WINDOW_BOTTOM,
					     QUERY_BORDER);
	gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText),
					     GTK_TEXT_WINDOW_LEFT,
					     QUERY_BORDER);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(pwQueryText), TRUE);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwQueryText, FALSE, FALSE, 0);
	gtk_widget_set_size_request(pwQueryText, 250, 80);

	pwHbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox, FALSE, FALSE, 0);
	pwLabel = gtk_label_new("Result");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwLabel, TRUE, TRUE, 0);

	pwRun = gtk_button_new_with_label("Run Query");
	g_signal_connect(G_OBJECT(pwRun), "clicked",
			 G_CALLBACK(RelationalQuery), pwVbox);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwRun, FALSE, FALSE, 0);

	pwQueryResult = gtk_clist_new(1);
	gtk_clist_set_column_title(GTK_CLIST(pwQueryResult), 0, " ");
	gtk_clist_column_titles_show(GTK_CLIST(pwQueryResult));
	gtk_clist_column_titles_passive(GTK_CLIST(pwQueryResult));

	pwQueryBox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwQueryBox), pwQueryResult, TRUE, TRUE,
			   0);

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
					      (pwScrolled), pwQueryBox);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwScrolled, TRUE, TRUE, 0);

	gtk_widget_show_all(pwDialog);

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
}

extern void GtkShowQuery(RowSet * pRow)
{
	GtkWidget *pwDialog;

	pwDialog =
	    GTKCreateDialog(_("GNU Backgammon - Database Result"), DT_INFO,
			    NULL, DIALOG_FLAG_MODAL, NULL, NULL);

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
			  GetRelList(pRow));

	gtk_widget_show_all(pwDialog);

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
}

#else
/* Avoid no code warning */
extern int dummy;
#endif
