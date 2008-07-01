/*
 * gtkfile.c
 *
 * by Ingo Macherius 2005
 *
 * File dialogs
 *
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
#include "backgammon.h"

#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "gtkfile.h"
#include "gtkgame.h"
#include "gtkwindows.h"
#include "file.h"
#include "util.h"

static void FilterAdd (const char *fn, const char *pt, GtkFileChooser * fc)
{
  GtkFileFilter *aff = gtk_file_filter_new ();
  gtk_file_filter_set_name (aff, fn);
  gtk_file_filter_add_pattern (aff, pt);
  gtk_file_chooser_add_filter (fc, aff);
}

static GtkWidget * GnuBGFileDialog (const gchar * prompt, const gchar * folder, const gchar * name,
		 GtkFileChooserAction action)
{
#ifdef WIN32
char *programdir, *pc, *tmp;
#endif
  GtkWidget *fc;
  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      fc = gtk_file_chooser_dialog_new (prompt, NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_OPEN,
					GTK_RESPONSE_ACCEPT,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					NULL);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      fc = gtk_file_chooser_dialog_new (prompt, NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_SAVE,
					GTK_RESPONSE_ACCEPT,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					NULL);
      break;
    default:
      return NULL;
      break;
    }
  gtk_window_set_modal (GTK_WINDOW (fc), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (fc), GTK_WINDOW (pwMain));

  if (folder && *folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), folder);
  if (name && *name)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), name);

#ifdef WIN32
  programdir = g_strdup(getInstallDir());
  if ((pc = strrchr(programdir, G_DIR_SEPARATOR)) != NULL) {
          *pc = '\0';

          tmp = g_build_filename(programdir, "GamesGrid", "SaveGame", NULL);
          gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
          g_free(tmp);

          tmp = g_build_filename(programdir, "GammonEmpire", "savedgames", NULL);
          gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
          g_free(tmp);

          tmp = g_build_filename(programdir, "PartyGaming", "PartyGammon", "SavedGames", NULL);
          gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
          g_free(tmp);

          tmp = g_build_filename(programdir, "Play65", "savedgames", NULL);
          gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
          g_free(tmp);

          tmp = g_build_filename(programdir, "TrueMoneyGames", "SavedGames", NULL);
          gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
          g_free(tmp);
  }
  g_free(programdir);
#endif
  return fc;
}

extern char * GTKFileSelect (const gchar * prompt, const gchar * extension, const gchar * folder,
	       const gchar * name, GtkFileChooserAction action)
{
  gchar *sz, *filename=NULL;
  GtkWidget *fc = GnuBGFileDialog (prompt, folder, name, action);
  if (extension && *extension)
    {
      sz = g_strdup_printf (_("Supported files (%s)"), extension);
      FilterAdd (sz, extension, GTK_FILE_CHOOSER (fc));
      FilterAdd (_("All Files"), "*", GTK_FILE_CHOOSER (fc));
      g_free(sz);
    }

  if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
  gtk_widget_destroy (fc);
  return filename;
}

typedef struct _SaveOptions
{
  GtkWidget *fc, *description, *mgp, *upext;
} SaveOptions;

static void SaveOptionsCallBack (GtkWidget * pw, SaveOptions * pso)
{
  gchar *fn, *fnn, *fnd;
  gint type, mgp;

  type = gtk_combo_box_get_active(GTK_COMBO_BOX (pso->description));
  mgp = gtk_combo_box_get_active (GTK_COMBO_BOX (pso->mgp));
  gtk_dialog_set_response_sensitive (GTK_DIALOG (pso->fc),
				     GTK_RESPONSE_ACCEPT,
				     export_format[type].exports[mgp]);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pso->upext)))
    {
      if ((fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pso->fc))) != NULL)
	{
	  DisectPath (fn, export_format[type].extension, &fnn, &fnd);
	  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pso->fc),
					       fnd);
	  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (pso->fc), fnn);
	  g_free (fn);
	  g_free (fnn);
	  g_free (fnd);
	}
    }
}

static void SaveCommon (guint f, gchar * prompt)
{

  GtkWidget *hbox;
  guint i, j, type;
  SaveOptions so;
  static guint last_export_type = 0;
  static guint last_export_mgp = 0;
  static gchar *last_save_folder = NULL;
  static gchar *last_export_folder = NULL;
  gchar *fn = GetFilename (TRUE, (f == 1) ? 0 : last_export_type);
  gchar *folder = NULL;

  if (f == 1)
    folder = last_save_folder ? last_save_folder : default_sgf_folder;
  else
    folder = last_export_folder ? last_export_folder : default_export_folder;

  so.fc = GnuBGFileDialog (prompt, folder, fn, GTK_FILE_CHOOSER_ACTION_SAVE);
  g_free (fn);

  so.description = gtk_combo_box_new_text ();
  for (j = i = 0; i < f; ++i)
    {
      gtk_combo_box_append_text (GTK_COMBO_BOX (so.description),
				 export_format[i].description);
      if (i == last_export_type)
	gtk_combo_box_set_active (GTK_COMBO_BOX (so.description), j);
      j++;
    }
  if (f == 1)
    gtk_combo_box_set_active (GTK_COMBO_BOX (so.description), 0);

  so.mgp = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.mgp), _("match"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.mgp), _("game"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.mgp), _("position"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (so.mgp), last_export_mgp);

  so.upext = gtk_check_button_new_with_label (_("Update extension"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (so.upext), TRUE);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.mgp);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.description);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.upext);
  gtk_widget_show_all (hbox);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (so.fc), hbox);

  g_signal_connect (G_OBJECT (so.description), "changed",
		    G_CALLBACK (SaveOptionsCallBack), &so);
  g_signal_connect (G_OBJECT (so.mgp), "changed",
		    G_CALLBACK (SaveOptionsCallBack), &so);

  SaveOptionsCallBack (so.fc, &so);

  if (gtk_dialog_run (GTK_DIALOG (so.fc)) == GTK_RESPONSE_ACCEPT)
    {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (so.fc));
      if (fn)
	{
	  gchar *et = gtk_combo_box_get_active_text (GTK_COMBO_BOX (so.mgp));
	  gchar *cmd = NULL;
	  type = gtk_combo_box_get_active (GTK_COMBO_BOX (so.description));
	  if (type == EXPORT_SGF)
	      cmd = g_strdup_printf ("save %s \"%s\"", et, fn);
	  else
	      cmd = g_strdup_printf ("export %s %s \"%s\"", et,
				 export_format[type].clname, fn);
	  last_export_type = type;
	  last_export_mgp = gtk_combo_box_get_active (GTK_COMBO_BOX (so.mgp));
	  g_free (last_export_folder);
	  last_export_folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (so.fc));
	  UserCommand (cmd);
	  g_free (cmd);
	}
      g_free (fn);
    }
  gtk_widget_destroy (so.fc);
}

ImportType lastOpenType;
GtkWidget *openButton, *selFileType;
int autoOpen;

static void update_preview_cb (GtkFileChooser *file_chooser, void *notused)
{
	const char *label;
	char *buf;
	char *filename = gtk_file_chooser_get_preview_filename (file_chooser);
	FilePreviewData *fpd = ReadFilePreview(filename);
	int openable = FALSE;
	if (!fpd)
	{
		lastOpenType = N_IMPORT_TYPES;
		label = "";
	}
	else
	{
		lastOpenType = fpd->type;
		label = gettext((import_format[lastOpenType]).description);
		g_free(fpd);
		if (lastOpenType != N_IMPORT_TYPES || !autoOpen)
			openable = TRUE;
	}
	buf = g_strdup_printf("<b>%s</b>", label);
	gtk_label_set_markup (GTK_LABEL(selFileType), buf);
	g_free(buf);

	gtk_widget_set_sensitive(openButton, openable);
}

static void add_import_filters (GtkFileChooser *fc)
{
	GtkFileFilter *aff = gtk_file_filter_new ();
	gint i;
	gchar *sg;

	gtk_file_filter_set_name (aff, _("Supported files"));
	for (i = 0; i < N_IMPORT_TYPES; ++i)
	{
		sg = g_strdup_printf ("*%s", import_format[i].extension);
		gtk_file_filter_add_pattern (aff, sg);
		g_free (sg);
	}
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fc), aff);

	FilterAdd (_("All Files"), "*", GTK_FILE_CHOOSER (fc));

	for (i = 0; i < N_IMPORT_TYPES; ++i)
	{
		sg = g_strdup_printf ("*%s", import_format[i].extension);
		FilterAdd (import_format[i].description, sg, GTK_FILE_CHOOSER (fc));
		g_free (sg);
	}
}

static void OpenTypeChanged(GtkComboBox *widget, gpointer fc)
{
	autoOpen = (gtk_combo_box_get_active(widget) == 0);
	update_preview_cb(fc, NULL);
}

static GtkWidget* import_types_combo(void)
{
	gint i;
	GtkWidget *type_combo = gtk_combo_box_new_text();

	/* Default option 'automatic' */
	gtk_combo_box_append_text(GTK_COMBO_BOX(type_combo), _("Automatic"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), 0);

	for (i = 0; i < N_IMPORT_TYPES; ++i)
		gtk_combo_box_append_text(GTK_COMBO_BOX(type_combo), import_format[i].description);

	/* Extra option 'command file' */
	gtk_combo_box_append_text(GTK_COMBO_BOX(type_combo), _("Gnubg Command file"));

	return type_combo;
}

static void do_import_file(gint import_type, gchar * fn)
{
	gchar *cmd = NULL;

	if (!fn)
		return;
	if (import_type == N_IMPORT_TYPES)
		outputerrf(_("Unable to import. Unrecognized file type"));
	else if (import_type == IMPORT_SGF) {
		cmd = g_strdup_printf("load match \"%s\"", fn);
	} else {
		cmd =
		    g_strdup_printf("import %s \"%s\"",
				    import_format[import_type].clname, fn);
	}
	if (cmd)
		UserCommand(cmd);
	g_free(cmd);

}

extern void GTKOpen(gpointer p, guint n, GtkWidget * pw)
{
	GtkWidget *fc;
	GtkWidget *type_combo, *box, *box2;
	gchar *fn;
	gchar *folder = NULL;
	gint import_type;
	static gchar *last_import_folder = NULL;

	folder = last_import_folder ? last_import_folder : default_import_folder;

	fc = GnuBGFileDialog(_("Open backgammon file"), folder, NULL, GTK_FILE_CHOOSER_ACTION_OPEN);

	type_combo = import_types_combo();
	g_signal_connect(type_combo, "changed", G_CALLBACK( OpenTypeChanged ), fc );

	box = gtk_hbox_new(FALSE, 0);
	box2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), gtk_label_new(_("Open as:")), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box2), type_combo, FALSE, FALSE, 0);

	box2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 10);
	gtk_box_pack_start(GTK_BOX(box2), gtk_label_new(_("Selected file type: ")), FALSE, FALSE, 0);
	selFileType = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(box2), selFileType, FALSE, FALSE, 0);
	gtk_widget_show_all(box);

	g_signal_connect(GTK_FILE_CHOOSER(fc), "update-preview", G_CALLBACK(update_preview_cb), NULL);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(fc), box);

	add_import_filters(GTK_FILE_CHOOSER(fc));
	openButton = DialogArea(fc, DA_OK);
	autoOpen = TRUE;

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT)
	{
		fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		import_type = gtk_combo_box_get_active(GTK_COMBO_BOX(type_combo));
		if (import_type == 0)
		{	/* Type automatically based on file */
			do_import_file(lastOpenType, fn);
		}
		else
		{
			import_type--;	/* Ignore auto option */
			if (import_type == N_IMPORT_TYPES)
			{	/* Load command file */
				CommandLoadCommands(fn);
			}
			else
			{	/* Import as specific type */
				do_import_file(import_type, fn);
			}
		}

		g_free(last_import_folder);
		last_import_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(fc));
		g_free(fn);
	}
	gtk_widget_destroy(fc);
}

extern void GTKSave (gpointer p, guint n, GtkWidget * pw)
{
  SaveCommon (N_EXPORT_TYPES, _("Save or export to file"));
}

enum {
	COL_RESULT = 0,
	COL_DESC,
	COL_FILE,
	COL_PATH,
	NUM_COLS
};

static char *batch_analyse(gchar *filename)
{
	gchar *file;
	gchar *folder;
	gchar *dir;
	gchar *save;
	gchar *cmd;

	DisectPath(filename, NULL, &file, &folder);
	dir = g_build_filename(folder, "analysed", NULL);
	g_free(folder);

	if (!g_file_test(dir, G_FILE_TEST_EXISTS))
		g_mkdir(dir, 0700);

	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
	{
		g_free(dir);
		return (_("Failed to make directory"));
	}

	save = g_strconcat(dir, G_DIR_SEPARATOR_S, file, ".sgf", NULL);
	g_free(file);
	g_free(dir);
	if (g_file_test((save), G_FILE_TEST_EXISTS))
		return(_("Previous"));

	g_free(szCurrentFileName);
	szCurrentFileName = NULL;
	cmd = g_strdup_printf("import auto \"%s\"", filename);
	UserCommand(cmd);
	g_free(cmd);
	if (!szCurrentFileName)
		return _("Failed import");

	UserCommand("analysis clear match");
	UserCommand("analyse match");

	if (!MatchAnalysed())
		return(_("Cancelled"));

	cmd = g_strdup_printf("save match \"%s\"", save);
	UserCommand(cmd);
	g_free(cmd);
	return (_("Done"));
}

static void batch_do_all(gpointer batch_model)
{
	gchar *result;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail(batch_model != NULL);

	valid = gtk_tree_model_get_iter_first(batch_model, &iter);

	while (valid) {
		gchar *filename;
		gint cancelled;
		gtk_tree_model_get(batch_model, &iter, COL_PATH, &filename,
				   -1);

		result = batch_analyse(filename);
		gtk_list_store_set(batch_model, &iter, COL_RESULT, result,
				   -1);

		cancelled = GPOINTER_TO_INT(g_object_get_data(batch_model,
					"cancelled"));
		if (cancelled)
			break;
		valid = gtk_tree_model_iter_next(batch_model, &iter);
	}
}

static void batch_cancel (GtkWidget *pw, gpointer model)
{
    pwGrab = pwOldGrab;
    fInterrupt = TRUE;
}


static void batch_stop(GtkWidget * pw, gpointer p) 
{
	fInterrupt = TRUE;
	g_object_set_data(G_OBJECT(p), "cancelled", GINT_TO_POINTER(1));
}

static void batch_skip_file(GtkWidget * pw, gpointer p) 
{
	fInterrupt = TRUE;
}

static GtkTreeModel *batch_create_model(GSList * filenames)
{
	GtkListStore *store;
	GtkTreeIter tree_iter;
	GSList *iter;
	FilePreviewData *fpd;

	store =
	    gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING,
			       G_TYPE_STRING, G_TYPE_STRING);

	for (iter = filenames; iter != NULL; iter = iter->next) {
		char *desc;
		char *folder;
		char *file;
		char *filename = (char *) iter->data;

		gtk_list_store_append(store, &tree_iter);

		fpd = ReadFilePreview(filename);
		if (!fpd)
			continue;

		desc = g_strdup(import_format[fpd->type].description);
		g_free(fpd);
		gtk_list_store_set(store, &tree_iter, COL_DESC, desc, -1);

		DisectPath(filename, NULL, &file, &folder);
		gtk_list_store_set(store, &tree_iter, COL_FILE, file, -1);
		g_free(file);
		g_free(folder);

		gtk_list_store_set(store, &tree_iter, COL_PATH, filename,
				   -1);

	}
	return GTK_TREE_MODEL(store);
}

static GtkWidget *batch_create_view(GSList * filenames)
{
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *view;

	view = gtk_tree_view_new();

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    -1, _("Result"),
						    renderer, "text",
						    COL_RESULT, NULL);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    -1, _("Type"),
						    renderer, "text",
						    COL_DESC, NULL);

	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    -1, ("File"), renderer,
						    "text", COL_FILE,
						    NULL);
	model = batch_create_model(filenames);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

	g_object_unref(model);
	return view;
}

static void batch_create_dialog_and_run(GSList * filenames)
{
	GtkWidget *dialog;
	GtkWidget *buttons;
	GtkWidget *view;
	GtkWidget *ok_button;
	GtkWidget *skip_button;
	GtkWidget *stop_button;
	GtkTreeModel *model;
	GtkWidget *sw;

	view = batch_create_view(filenames);
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
	g_object_set_data(G_OBJECT(model), "cancelled",
			  GINT_TO_POINTER(0));

	dialog = GTKCreateDialog(_("Batch analyse files"), DT_INFO, NULL,
			DIALOG_FLAG_MODAL | DIALOG_FLAG_MINMAXBUTTONS |
			DIALOG_FLAG_NOTIDY, NULL, NULL);

	gtk_window_set_default_size( GTK_WINDOW( dialog ), -1, 400);

	pwOldGrab = pwGrab;
	pwGrab = dialog;

	g_signal_connect(G_OBJECT(dialog), "destroy",
			 G_CALLBACK(batch_cancel), model);


	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), view);
	gtk_container_add (GTK_CONTAINER (DialogArea (dialog, DA_MAIN)), sw);

	buttons = DialogArea(dialog, DA_BUTTONS);

	ok_button = DialogArea(dialog, DA_OK);
	gtk_widget_set_sensitive(ok_button, FALSE);

	skip_button = gtk_button_new_with_label(_("Skip"));
	stop_button = gtk_button_new_with_label(_("Stop"));

	gtk_container_add(GTK_CONTAINER(buttons), skip_button);
	gtk_container_add(GTK_CONTAINER(buttons), stop_button);

	g_signal_connect(G_OBJECT(skip_button), "clicked",
			 G_CALLBACK(batch_skip_file), model);
	g_signal_connect(G_OBJECT(stop_button), "clicked",
			 G_CALLBACK(batch_stop), model);
	gtk_widget_show_all(dialog);
	batch_do_all(model);
	gtk_widget_set_sensitive(ok_button, TRUE);
}

extern void GTKBatchAnalyse(gpointer p, guint n, GtkWidget * pw)
{
	gchar *folder = NULL;
	GSList *filenames = NULL;
	GtkWidget *fc;
	static gchar *last_folder = NULL;

	folder = last_folder ? last_folder : default_import_folder;

	fc = GnuBGFileDialog(_("Select files to analyse"), folder, NULL,
			     GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(fc), TRUE);
	add_import_filters(GTK_FILE_CHOOSER(fc));

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		filenames =
		    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(fc));
	}
	if (filenames) {
		g_free(last_folder);
		last_folder =
		    gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER
							(fc));
		gtk_widget_destroy(fc);
		batch_create_dialog_and_run(filenames);
	} else
		gtk_widget_destroy(fc);
}
