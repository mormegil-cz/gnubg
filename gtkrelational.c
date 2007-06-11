/*
 * gtkrelational.c
 *
 * by Christian Anthon <anthon@kiku.dk>, 2006.
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


enum
{
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


static GtkTreeModel *
create_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  RowSet r;

  long moves[3];
  int i, j;
  gfloat stats[13];



  /* create list store */
  store = gtk_list_store_new (NUM_COLUMNS,
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
			      G_TYPE_FLOAT, G_TYPE_FLOAT, G_TYPE_FLOAT);

  /* prepare the sql query */
  if (!RunQuery (&r, "nick.name,"
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
  for (j = 1; j < r.rows; ++j)
    {
      for (i = 1; i < 4; ++i)
	{
	  moves[i - 1] = strtol (r.data[j][i], NULL, 0);
	}
      for (i = 4; i < 13; ++i)
	{
	  stats[i - 4] = (float)g_strtod (r.data[j][i], NULL);
	}
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
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
			  COLUMN_LUCK, stats[8] / moves[0] * 1000.0f, -1);
    }
  FreeRowset (&r);
  return GTK_TREE_MODEL (store);
}

void
cell_data_func (GtkTreeViewColumn * col,
		GtkCellRenderer * renderer,
		GtkTreeModel * model, GtkTreeIter * iter, gpointer column)
{
  gfloat data;
  gchar buf[20];

  /* get data pointed to by column */
  gtk_tree_model_get (model, iter, GPOINTER_TO_INT (column), &data, -1);

  /* format the data to two digits */
  g_snprintf (buf, sizeof (buf), "%.2f", data);

  /* render the string right aligned */
  g_object_set (renderer, "text", buf, NULL);
  g_object_set (renderer, "xalign", 1.0, NULL);
}

static void
add_columns (GtkTreeView * treeview)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  gint i;

  renderer = gtk_cell_renderer_text_new ();

  column =
    gtk_tree_view_column_new_with_attributes ("Nick", renderer,
					      "text", COLUMN_NICK, NULL);
  gtk_tree_view_column_set_sort_column_id (column, 0);
  gtk_tree_view_append_column (treeview, column);

  for (i = 1; i < NUM_COLUMNS; i++)
    {
      column = gtk_tree_view_column_new ();
      renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_column_pack_end (column, renderer, TRUE);
      gtk_tree_view_column_set_cell_data_func (column, renderer,
					       cell_data_func,
					       GINT_TO_POINTER (i), NULL);
      gtk_tree_view_column_set_sort_column_id (column, i);
      gtk_tree_view_column_set_title (column, gettext (titles[i]));
      gtk_tree_view_append_column (treeview, column);
    }
}

GtkWidget *
do_list_store (void)
{
  GtkTreeModel *model;
  GtkWidget *treeview;

  model = create_model ();

  treeview = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (treeview), COLUMN_NICK);

  g_object_unref (model);

  add_columns (GTK_TREE_VIEW (treeview));

  return treeview;
}


void
view_onRowActivated (GtkTreeView * treeview,
		     GtkTreePath * path,
		     GtkTreeViewColumn * col, gpointer userdata)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (treeview);

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gchar *name;
      gtk_tree_model_get (model, &iter, COLUMN_NICK, &name, -1);
	  GTKSetCurrentParent(GTK_WIDGET(userdata));
      CommandRelationalShowDetails (name);
      g_free (name);
    }
}

extern void
GtkRelationalShowStats (gpointer p, guint n, GtkWidget * pw)
{
  GtkWidget *pwDialog;
  GtkWidget *scrolledwindow1;
  GtkWidget *treeview1;

  pwDialog = GTKCreateDialog (_("GNU Backgammon - Player Stats"),
	  DT_INFO, NULL, DIALOG_FLAG_MODAL, NULL, NULL);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
				       (scrolledwindow1), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW
				  (scrolledwindow1), GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
				       (scrolledwindow1), GTK_SHADOW_IN);

  treeview1 = do_list_store ();
  g_signal_connect (treeview1, "row-activated",
		    (GCallback) view_onRowActivated, pwDialog);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), treeview1);

  gtk_container_add (GTK_CONTAINER (DialogArea (pwDialog, DA_MAIN)),
		     scrolledwindow1);
  gtk_window_set_default_size (GTK_WINDOW (pwDialog), 1, 400);
  gtk_widget_show_all (pwDialog);
  gtk_main ();
}
#endif
