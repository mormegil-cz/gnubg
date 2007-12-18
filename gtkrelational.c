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
static GtkWidget *aliases;
static GtkWidget *pwAliasList;
static char envString[100], curEnvString[100];
static GtkWidget *envP1;
static GtkWidget *envP2;
static RowSet *pNick1Envs;
static RowSet *pNick2Envs;
static RowSet *pEnvRows;
static char envString[100];
static char curEnvString[100];
static char linkPlayer[100];
static int currentLinkRow;

static int curEnvRow;
static int numEnvRows;
static GtkWidget *pwRemoveEnv;
static GtkWidget *pwRenameEnv;


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
	int i, j;
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
	if (!RunQuery(&r, "nick.name,"
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
		      "FROM matchstat NATURAL JOIN nick group by nick.name"))
		return 0;
	for (j = 1; j < r.rows; ++j) {
		for (i = 1; i < 4; ++i) {
			moves[i - 1] = strtol(r.data[j][i], NULL, 0);
		}
		for (i = 4; i < 13; ++i) {
			stats[i - 4] =
			    (float) g_strtod(r.data[j][i], NULL);
		}
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

	treeview = gtk_tree_view_new_with_model(model);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(treeview), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview),
					COLUMN_NICK);

	g_object_unref(model);

	add_columns(GTK_TREE_VIEW(treeview));

	return treeview;
}


static void
view_onRowActivated(GtkTreeView * treeview,
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
	g_signal_connect(treeview1, "row-activated",
			 (GCallback) view_onRowActivated, pwDialog);
	gtk_container_add(GTK_CONTAINER(scrolledwindow1), treeview1);

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
			  scrolledwindow1);
	gtk_window_set_default_size(GTK_WINDOW(pwDialog), 1, 400);
	gtk_widget_show_all(pwDialog);
	gtk_main();
}

static void SelectEnvOK(GtkWidget * pw, GtkWidget * pwCombo)
{
	char *current =
	    (char *)
	    gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pwCombo)->entry));
	strcpy(envString, current);
	gtk_widget_destroy(gtk_widget_get_toplevel(pw));
}

static void SelEnvChange(GtkList * list, GtkWidget * pwCombo)
{
	char *current =
	    (char *)
	    gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pwCombo)->entry));
	if (current && *current
	    && (!*curEnvString || strcmp(current, curEnvString))) {
		int i, id;
		strcpy(curEnvString, current);

		/* Get id from selected string */
		i = 1;
		while (strcmp(pEnvRows->data[i][1], curEnvString))
			i++;
		id = atoi(pEnvRows->data[i][0]);

		/* Set player names for env (if available) */
		for (i = 1; i < pNick1Envs->rows; i++) {
			if (atoi(pNick1Envs->data[i][0]) == id)
				break;
		}
		if (i < pNick1Envs->rows)
			gtk_label_set_text(GTK_LABEL(envP1),
					   pNick1Envs->data[i][1]);
		else
			gtk_label_set_text(GTK_LABEL(envP1),
					   "[new to env]");

		for (i = 1; i < pNick2Envs->rows; i++) {
			if (atoi(pNick2Envs->data[i][0]) == id)
				break;
		}
		if (i < pNick2Envs->rows)
			gtk_label_set_text(GTK_LABEL(envP2),
					   pNick2Envs->data[i][1]);
		else
			gtk_label_set_text(GTK_LABEL(envP2),
					   "[new to env]");
	}
}

static int GtkGetEnv(char *env)
{				/* Get an environment for the match to be added */
	int envID;
	/* See if more than one env (if not no choice) */
	RowSet envRows;
	char *query = "env_id, place FROM env ORDER BY env_id";

	if (!RunQuery(&envRows, query)) {
		GTKMessage(_("Database error getting environment"),
			   DT_ERROR);
		return -1;
	}

	if (envRows.rows == 2) {	/* Just one env */
		strcpy(env, envRows.data[1][1]);
	} else {
		/* find out what envs players nicks are in */
		int found, cur1, cur2, i, NextEnv;
		RowSet nick1, nick2;
		char query[1024];
		char *queryText = "env_id, person.name"
		    " FROM nick INNER JOIN person"
		    " ON nick.person_id = person.person_id"
		    " WHERE nick.name = '%s' ORDER BY env_id";

		pNick1Envs = &nick1;
		pNick2Envs = &nick2;
		pEnvRows = &envRows;

		sprintf(query, queryText, ap[0].szName);
		if (!RunQuery(&nick1, query)) {
			GTKMessage(_("Database error getting environment"),
				   DT_ERROR);
			return -1;
		}

		sprintf(query, queryText, ap[1].szName);
		if (!RunQuery(&nick2, query)) {
			GTKMessage(_("Database error getting environment"),
				   DT_ERROR);
			return -1;
		}

		/* pick best (first with both players or first with either player) */
		found = 0;
		cur1 = cur2 = 1;
		envID = atoi(envRows.data[1][0]);

		for (i = 1; i < envRows.rows; i++) {
			int oneFound = FALSE;
			int curID = atoi(envRows.data[i][0]);
			if (nick1.rows > cur1) {
				NextEnv = atoi(nick1.data[cur1][0]);
				if (NextEnv == curID) {
					if (found == 0) {	/* Found at least one */
						found = 1;
						envID = curID;
					}
					oneFound = TRUE;
					cur1++;
				}
			}
			if (nick2.rows > cur2) {
				NextEnv = atoi(nick2.data[cur2][0]);
				if (NextEnv == curID) {
					if (found == 0) {	/* Found at least one */
						found = 1;
						envID = curID;
					}
					if (oneFound == 1) {	/* Found env with both nicks */
						envID = curID;
						break;
					}
				}
			}
		}
		/* show dialog to select env */
		{
			GList *glist;
			GtkWidget *pwEnvCombo, *pwDialog, *pwHbox, *pwVbox;
			pwEnvCombo = gtk_combo_new();

			pwDialog =
			    GTKCreateDialog(_
					    ("GNU Backgammon - Select Environment"),
					    DT_QUESTION, NULL,
					    DIALOG_FLAG_MODAL,
					    G_CALLBACK(SelectEnvOK),
					    pwEnvCombo);

			gtk_container_add(GTK_CONTAINER
					  (DialogArea(pwDialog, DA_MAIN)),
					  pwHbox = gtk_hbox_new(FALSE, 0));

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
					   gtk_vbox_new(FALSE, 0), FALSE,
					   FALSE, 0);
			gtk_box_pack_start(GTK_BOX(pwVbox),
					   gtk_label_new(_("Environment")),
					   FALSE, FALSE, 0);

			gtk_combo_set_value_in_list(GTK_COMBO(pwEnvCombo),
						    TRUE, FALSE);
			glist = NULL;
			for (i = 1; i < envRows.rows; i++)
				glist =
				    g_list_append(glist,
						  envRows.data[i][1]);
			gtk_combo_set_popdown_strings(GTK_COMBO
						      (pwEnvCombo), glist);
			g_list_free(glist);
			gtk_box_pack_start(GTK_BOX(pwVbox), pwEnvCombo,
					   FALSE, FALSE, 0);

			/* Select best env */
			i = 1;
			while (atoi(envRows.data[i][0]) != envID)
				i++;

			*curEnvString = '\0';
			gtk_widget_set_sensitive(GTK_WIDGET
						 (GTK_COMBO(pwEnvCombo)->
						  entry), FALSE);
			g_signal_connect(G_OBJECT
					 (GTK_COMBO(pwEnvCombo)->list),
					 "selection-changed",
					 G_CALLBACK(SelEnvChange),
					 pwEnvCombo);

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
					   gtk_vbox_new(FALSE, 0), FALSE,
					   FALSE, 10);
			gtk_box_pack_start(GTK_BOX(pwVbox),
					   gtk_label_new(ap[0].szName),
					   FALSE, FALSE, 0);
			envP1 = gtk_label_new(NULL);
			gtk_box_pack_start(GTK_BOX(pwVbox), envP1, FALSE,
					   FALSE, 0);

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
					   gtk_vbox_new(FALSE, 0), FALSE,
					   FALSE, 10);
			gtk_box_pack_start(GTK_BOX(pwVbox),
					   gtk_label_new(ap[1].szName),
					   FALSE, FALSE, 0);
			envP2 = gtk_label_new(NULL);
			gtk_box_pack_start(GTK_BOX(pwVbox), envP2, FALSE,
					   FALSE, 0);

			gtk_entry_set_text(GTK_ENTRY
					   (GTK_COMBO(pwEnvCombo)->entry),
					   envRows.data[i][1]);

			gtk_widget_show_all(pwDialog);

			*envString = '\0';

			GTKDisallowStdin();
			gtk_main();
			GTKAllowStdin();

			if (*envString) {
				strcpy(env, envString);
			} else
				*env = '\0';
		}

		FreeRowset(&nick1);
		FreeRowset(&nick2);
	}

	FreeRowset(&envRows);
	return 0;
}

extern void GtkRelationalAddMatch(gpointer p, guint n, GtkWidget * pw)
{
	char env[100];
	char buf[200];
	int exists = RelationalMatchExists();
	if (exists == -1 ||
	    (exists == 1 && !GetInputYN(_("Match exists, overwrite?"))))
		return;

	if (GtkGetEnv(env) < 0)
		return;

	/* Pass in env id and force addition */
	sprintf(buf, "\"%s\" Yes", env);
	CommandRelationalAddMatch(buf);

	outputx();
}

static GtkWidget *GetRelList(RowSet * pRow)
{
	int i;
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
	char *pName, *pEnv;
	RowSet r, r2;
	char query[1024];
	int i;

	gtk_clist_get_text(GTK_CLIST(pw), y, 0, &pName);
	gtk_clist_get_text(GTK_CLIST(pw), y, 1, &pEnv);

	sprintf(query, "person.person_id, person.name, person.notes"
		" FROM nick INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE nick.name = '%s' AND env.place = '%s'",
		pName, pEnv);

	ClearText(GTK_TEXT_VIEW(pwPlayerNotes));
	if (pwAliasList) {
		ClearText(GTK_TEXT_VIEW(pwAliasList));
	}
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

	sprintf(query, _("%s is:"), r.data[1][1]);
	if (aliases)
		gtk_label_set_text(GTK_LABEL(aliases), query);

	sprintf(query, "nick.name, env.place"
		" FROM nick INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE person.person_id = %d", curPlayerId);

	if (!RunQuery(&r2, query)) {
		return;
	}

	if (pwAliasList) {
		GtkTextBuffer *buffer =
		    gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwAliasList));
		GtkTextIter it;
		gtk_text_buffer_get_end_iter(buffer, &it);
		for (i = 1; i < r2.rows; i++) {
			char line[100];
			sprintf(line, _("%s on %s\n"), r2.data[i][0],
				r2.data[i][1]);
			gtk_text_buffer_insert(buffer, &it, line, -1);
		}
	}
	FreeRowset(&r);
	FreeRowset(&r2);

}

static void RelationalOpen(GtkWidget * pw, GtkWidget * pwList)
{
	char *player, *env;
	char buf[200];

	if (curPlayerId == -1)
		return;

	/* Get player name and env from list */
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 1, &env);

	sprintf(buf, "Open (%s, %s)", player, env);
	output(buf);
	outputx();
}

static void RelationalErase(GtkWidget * pw, GtkWidget * pwList)
{
	char *player, *env;
	char buf[200];

	if (curPlayerId == -1)
		return;

	/* Get player name and env from list */
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 1, &env);

	sprintf(buf, _("Remove all data for %s?"), player);
	if (!GetInputYN(buf))
		return;

	sprintf(buf, "\"%s\" \"%s\"", player, env);
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

static void UpdatePlayerDetails(GtkWidget * pw, int *dummy)
{
	char *pch;
	if (curPlayerId == -1)
		return;

	pch = GetText(GTK_TEXT_VIEW(pwPlayerNotes));
	RelationalUpdatePlayerDetails(curPlayerId,
				      gtk_entry_get_text(GTK_ENTRY
							 (pwPlayerName)),
				      pch);
	g_free(pch);
}

static void SelectLinkOK(GtkWidget * pw, GtkWidget * pwList)
{
	char *current;
	if (currentLinkRow != -1) {
		gtk_clist_get_text(GTK_CLIST(pwList), currentLinkRow, 0,
				   &current);
		strcpy(linkPlayer, current);
		gtk_widget_destroy(gtk_widget_get_toplevel(pw));
	}
}

static void LinkPlayerSelect(GtkWidget * pw, int y, int x,
			     GdkEventButton * peb, void *null)
{
	currentLinkRow = y;
}


static void RelationalLinkPlayers(GtkWidget * pw, GtkWidget * pwRelList)
{
	char query[1024];
	RowSet r;
	GtkWidget *pwDialog, *pwList;
	char *linkNick, *linkEnv;
	if (curPlayerId == -1)
		return;

	gtk_clist_get_text(GTK_CLIST(pwRelList), curRow, 0, &linkNick);
	gtk_clist_get_text(GTK_CLIST(pwRelList), curRow, 1, &linkEnv);

	/* Pick suitable linking players with this query... */
	sprintf(query,
		"person.name AS Player FROM person WHERE person_id NOT IN"
		" (SELECT person.person_id FROM nick"
		" INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person ON nick.person_id = person.person_id"
		" WHERE env.env_id IN"
		" (SELECT env_id FROM nick INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE person.name = '%s'))",
		gtk_entry_get_text(GTK_ENTRY(pwPlayerName)));
	if (!RunQuery(&r, query))
		return;

	if (r.rows < 2) {
		GTKMessage("No players to link to", DT_ERROR);
		FreeRowset(&r);
		return;
	}

	pwList = GetRelList(&r);
	FreeRowset(&r);

	pwDialog =
	    GTKCreateDialog(_("GNU Backgammon - Select Linked Player"),
			    DT_QUESTION, pw, DIALOG_FLAG_MODAL,
			    G_CALLBACK(SelectLinkOK), pwList);

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
			  pwList);
	g_signal_connect(G_OBJECT(pwList), "select-row",
			 G_CALLBACK(LinkPlayerSelect), pwList);

	gtk_widget_show_all(pwDialog);

	*linkPlayer = '\0';
	currentLinkRow = -1;

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();

	if (*linkPlayer) {
		RelationalLinkNick(linkNick, linkEnv, linkPlayer);
		ShowRelationalSelect(pwRelList, curRow, 0, 0, 0);
		/* update new details list...
		   >> change query so person not picked if nick in selected env!
		 */
	}
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
	    *pwn, *pwLabel, *pwLink, *pwScrolled;
	int multipleEnv;


	pwAliasList = 0;
	aliases = 0;

	/* See if there's more than one environment */
	if (!RunQuery(&r, "env_id FROM env")) {
		GTKMessage(_("Database error"), DT_ERROR);
		return;
	}
	multipleEnv = (r.rows > 2);
	FreeRowset(&r);

	curPlayerId = -1;

	pwDialog =
	    GTKCreateDialog(_("GNU Backgammon - Relational Database"),
			    DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL, NULL);

	pwn = gtk_notebook_new();
	gtk_container_set_border_width(GTK_CONTAINER(pwn), 0);
	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
			  pwn);

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

	if (!RunQuery
	    (&r,
	     "name AS Nickname, place AS env FROM nick INNER JOIN env"
	     " ON nick.env_id = env.env_id ORDER BY name"))
		return;

	if (r.rows < 2) {
		GTKMessage(_("No data in database"), DT_ERROR);
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
			 G_CALLBACK(UpdatePlayerDetails), NULL);

/*******************************************************
** End of right hand side of player screen...
*******************************************************/

	/* If more than one environment, show linked nicknames */
	if (multipleEnv) {	/* Multiple environments */
		gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
				   gtk_vbox_new(FALSE, 0), FALSE, FALSE,
				   0);
		aliases = gtk_label_new(NULL);
		gtk_misc_set_alignment(GTK_MISC(aliases), 0, 0);
		gtk_box_pack_start(GTK_BOX(pwVbox), aliases, FALSE, FALSE,
				   0);

		pwAliasList = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(pwAliasList),
					   FALSE);
		gtk_box_pack_start(GTK_BOX(pwVbox), pwAliasList, TRUE,
				   TRUE, 0);

		pwLink = gtk_button_new_with_label(_("Link players"));
		g_signal_connect(G_OBJECT(pwLink), "clicked",
				 G_CALLBACK(RelationalLinkPlayers),
				 pwList);
		gtk_box_pack_start(GTK_BOX(pwVbox), pwLink, FALSE, TRUE,
				   0);
	}

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
	gtk_widget_set_usize(pwQueryText, 250, 80);

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

static void ManageEnvSelect(GtkWidget * pw, int y, int x,
			    GdkEventButton * peb, GtkWidget * pwCopy)
{
	gtk_widget_set_sensitive(pwRemoveEnv, TRUE);
	gtk_widget_set_sensitive(pwRenameEnv, TRUE);

	curEnvRow = y;
}

static void RelationalNewEnv(GtkWidget * pw, GtkWidget * pwList)
{
	char *newName;

	if ((newName = GTKGetInput(_("New Environment"), _("Enter name")))) {
		char *aszRow[1];
		CommandRelationalAddEnvironment(newName);

		if (env_added) {
			aszRow[0] = newName;
			gtk_clist_append(GTK_CLIST(pwList), aszRow);
			free(newName);

			gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
			gtk_widget_set_sensitive(pwRenameEnv, FALSE);

			numEnvRows++;
		}
	}
}


static void RelationalRemoveEnv(GtkWidget * pw, GtkWidget * pwList)
{
	char *pch;

	if (numEnvRows == 1) {
		GTKMessage(_("You must have at least one environment"),
			   DT_WARNING);
		return;
	}

	gtk_clist_get_text(GTK_CLIST(pwList), curEnvRow, 0, &pch);

	CommandRelationalEraseEnv(pch);

	if (env_deleted) {
		gtk_clist_remove(GTK_CLIST(pwList), curEnvRow);
		gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
		gtk_widget_set_sensitive(pwRenameEnv, FALSE);

		numEnvRows--;
	}
}

static void RelationalRenameEnv(GtkWidget * pw, GtkWidget * pwList)
{
	char *pch, *newName;
	gtk_clist_get_text(GTK_CLIST(pwList), curEnvRow, 0, &pch);

	if ((newName =
	     GTKGetInput(_("Rename Environment"), _("Enter new name")))) {
		char str[1024];
		sprintf(str, "\"%s\" \"%s\"", pch, newName);
		CommandRelationalRenameEnv(str);
		gtk_clist_set_text(GTK_CLIST(pwList), curEnvRow, 0,
				   newName);
		free(newName);
	}
}

extern void GtkManageRelationalEnvs(gpointer p, guint n, GtkWidget * pw)
{
	RowSet r;
	GtkWidget *pwList, *pwDialog, *pwHbox, *pwVbox, *pwBut;

	pwDialog =
	    GTKCreateDialog(_("GNU Backgammon - Manage Environments"),
			    DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL, NULL);

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)),
			  pwHbox = gtk_hbox_new(FALSE, 0));

	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
			   gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);

	if (!RunQuery(&r, "place as environment FROM env"))
		return;

	numEnvRows = r.rows - 1;	/* -1 as header row counted */

	pwList = GetRelList(&r);
	g_signal_connect(G_OBJECT(pwList), "select-row",
			 G_CALLBACK(ManageEnvSelect), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwList, TRUE, TRUE, 0);
	FreeRowset(&r);

	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox =
			   gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);

	pwBut = gtk_button_new_with_label(_("New Env"));
	g_signal_connect(G_OBJECT(pwBut), "clicked",
			 G_CALLBACK(RelationalNewEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwBut, FALSE, FALSE, 4);

	pwRemoveEnv = gtk_button_new_with_label(_("Remove Env"));
	g_signal_connect(G_OBJECT(pwRemoveEnv), "clicked",
			 G_CALLBACK(RelationalRemoveEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwRemoveEnv, FALSE, FALSE, 4);

	pwRenameEnv = gtk_button_new_with_label(_("Rename Env"));
	g_signal_connect(G_OBJECT(pwRenameEnv), "clicked",
			 G_CALLBACK(RelationalRenameEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwRenameEnv, FALSE, FALSE, 4);

	gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
	gtk_widget_set_sensitive(pwRenameEnv, FALSE);

	gtk_widget_show_all(pwDialog);

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
}
#else
/* Avoid no code warning */
extern int dummy;
#endif
