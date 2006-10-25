/*
 * gtkfile.c
 *
 * by Ingo Macherius 2005
 *
 * File dialogs
 *
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
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "backgammon.h"
#include "gtkfile.h"
#include "gtkgame.h"

static char *
GetFilename (void)
{
  char *sz, tstr[15];
  time_t t;

  if (szCurrentFileName && *szCurrentFileName)
    sz = g_strdup_printf ("%s.sgf", szCurrentFileName);
  else
    {
      if (mi.nYear)
	sprintf (tstr, "%04d-%02d-%02d", mi.nYear, mi.nMonth, mi.nDay);
      else
	{
	  time (&t);
	  strftime (tstr, 14, _("%Y-%m-%d-%H%M"), localtime (&t));
	}
      sz =
	g_strdup_printf ("%s-%s_%dp_%s.sgf", ap[0].szName, ap[1].szName,
			 ms.nMatchTo, tstr);
    }
  return sz;
}


static int
FormatFromDescription (const gchar * text)
{
  gint i;
  if (!text)
    return -1;
  for (i = 0; i < n_file_formats; i++)
    {
      if (!strcasecmp (text, file_format[i].description))
	break;
    }
  return i;
}

static void
FilterAdd (char *fn, char *pt, GtkFileChooser * fc)
{
  GtkFileFilter *aff = gtk_file_filter_new ();
  gtk_file_filter_set_name (aff, fn);
  gtk_file_filter_add_pattern (aff, pt);
  gtk_file_chooser_add_filter (fc, aff);
}

static GtkWidget *
GnuBGFileDialog (gchar * prompt, gchar * folder, gchar * name,
		 GtkFileChooserAction action)
{
  GtkWidget *fc;
  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
      fc = gtk_file_chooser_dialog_new (prompt, NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN,
					GTK_RESPONSE_ACCEPT, NULL);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      fc = gtk_file_chooser_dialog_new (prompt, NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE,
					GTK_RESPONSE_ACCEPT, NULL);
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

#if WIN32
  if ((pc = getenv ("ProgramFiles")))
    {
      tmp = g_strdup_printf ("%s\\TMG\\SavedGames", pc);
      gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
      g_free (tmp);
      tmp = g_strdup_printf ("%s\\GamesGrid\\SaveGame\\", pc);
      gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (fc), tmp, NULL);
      g_free (tmp);
    }
#endif
  return fc;
}

extern char *
GTKFileSelect (gchar * prompt, gchar * extension, gchar * folder,
	       gchar * name, GtkFileChooserAction action)
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
  GtkWidget *fc, *description, *type, *upext;
} SaveOptions;

static void
SaveOptionsCallBack (GtkWidget * pw, SaveOptions * pso)
{
  gchar *description, *fn, *fnn, *fnd;
  gint format, type;

  description =
    gtk_combo_box_get_active_text (GTK_COMBO_BOX (pso->description));
  format = FormatFromDescription (description);
  type = gtk_combo_box_get_active (GTK_COMBO_BOX (pso->type));
  gtk_dialog_set_response_sensitive (GTK_DIALOG (pso->fc),
				     GTK_RESPONSE_ACCEPT,
				     file_format[format].exports[type]);
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pso->upext)))
    {
      if ((fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pso->fc))))
	{
	  DisectPath (fn, file_format[format].extension, &fnn, &fnd);
	  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (pso->fc),
					       fnd);
	  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (pso->fc), fnn);
	  g_free (fn);
	  g_free (fnn);
	  g_free (fnd);
	}
    }
}


static void
SaveCommon (guint f, gchar * prompt)
{

  GtkWidget *hbox;
  guint i, j, format;
  SaveOptions so;
  static gint last_export_format = 0;
  static gint last_export_type = 0;
  static gchar *last_save_folder = NULL;
  static gchar *last_export_folder = NULL;
  gchar *fn = GetFilename ();
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
      if (!file_format[i].canexport)
	continue;
      gtk_combo_box_append_text (GTK_COMBO_BOX (so.description),
				 file_format[i].description);
      if (i == last_export_format)
	gtk_combo_box_set_active (GTK_COMBO_BOX (so.description), j);
      j++;
    }
  if (f == 1)
    gtk_combo_box_set_active (GTK_COMBO_BOX (so.description), 0);

  so.type = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.type), _("match"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.type), _("game"));
  gtk_combo_box_append_text (GTK_COMBO_BOX (so.type), _("position"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (so.type), last_export_type);

  so.upext = gtk_check_button_new_with_label (_("Update extension"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (so.upext), TRUE);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.type);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.description);
  gtk_box_pack_start_defaults (GTK_BOX (hbox), so.upext);
  gtk_widget_show_all (hbox);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (so.fc), hbox);

  g_signal_connect (G_OBJECT (so.description), "changed",
		    G_CALLBACK (SaveOptionsCallBack), &so);
  g_signal_connect (G_OBJECT (so.type), "changed",
		    G_CALLBACK (SaveOptionsCallBack), &so);

  SaveOptionsCallBack (so.fc, &so);

  if (gtk_dialog_run (GTK_DIALOG (so.fc)) == GTK_RESPONSE_ACCEPT)
    {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (so.fc));
      if (fn)
	{
	  gchar *ft =
	    gtk_combo_box_get_active_text (GTK_COMBO_BOX (so.description));
	  gchar *et = gtk_combo_box_get_active_text (GTK_COMBO_BOX (so.type));
	  gchar *cmd = NULL;
	  format = FormatFromDescription (ft);
	  if (format)
	    {
	      cmd =
		g_strdup_printf ("export %s %s \"%s\"", et,
				 file_format[format].clname, fn);
	      last_export_format = format;
	      last_export_type =
		gtk_combo_box_get_active (GTK_COMBO_BOX (so.type));
	      g_free (last_export_folder);
	      last_export_folder =
		gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER
						     (so.fc));
	    }
	  else
	    {
	      cmd = g_strdup_printf ("save %s \"%s\"", et, fn);
	      last_export_type =
		gtk_combo_box_get_active (GTK_COMBO_BOX (so.type));
	      g_free (last_save_folder);
	      last_save_folder =
		gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER
						     (so.fc));
	    }
	  if (cmd)
	    {
	      UserCommand (cmd);
	      g_free (cmd);
	    }
	  g_free (fn);
	}
    }
  gtk_widget_destroy (so.fc);
}

static void
OpenCommon (guint f, gchar * prompt)
{

  gchar *fn, *sg, *cmd = NULL;
  guint format, i, j;
  GtkFileFilter *aff;
  GtkWidget *desc, *fc;
  static gint last_import_format = 0;
  static gchar *last_load_folder = NULL;
  static gchar *last_import_folder = NULL;
  gchar *folder = NULL;

  if (f == 1)
    folder = last_load_folder ? last_load_folder : default_sgf_folder;
  else
    folder = last_import_folder ? last_import_folder : default_import_folder;

  fc = GnuBGFileDialog (prompt, folder, NULL, GTK_FILE_CHOOSER_ACTION_OPEN);

  aff = gtk_file_filter_new ();
  gtk_file_filter_set_name (aff, _("Supported files"));
  for (i = 0; i < f; ++i)
    {
      if (!file_format[i].canimport)
	continue;
      sg = g_strdup_printf ("*%s", file_format[i].extension);
      gtk_file_filter_add_pattern (aff, sg);
      g_free (sg);
    }
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fc), aff);
  FilterAdd (_("All Files"), "*", GTK_FILE_CHOOSER (fc));
  for (i = 0; i < f; ++i)
    {
      if (!file_format[i].canimport)
	continue;
      sg = g_strdup_printf ("*%s", file_format[i].extension);
      FilterAdd (file_format[i].description, sg, GTK_FILE_CHOOSER (fc));
      g_free (sg);
    }

  desc = gtk_combo_box_new_text ();
  for (j = i = 0; i < f; ++i)
    {
      if (!file_format[i].canimport)
	continue;
      gtk_combo_box_append_text (GTK_COMBO_BOX
				 (desc), file_format[i].description);
      if (i == last_import_format)
	gtk_combo_box_set_active (GTK_COMBO_BOX (desc), j);
      j++;
    }
  if (f == 1)
    gtk_combo_box_set_active (GTK_COMBO_BOX (desc), 0);
  gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (fc), desc);
  if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
    {
      fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
      if (fn)
	{
	  format =
	    FormatFromDescription
	    (gtk_combo_box_get_active_text (GTK_COMBO_BOX (desc)));
	  if (format)
	    {
	      cmd =
		g_strdup_printf ("import %s \"%s\"",
				 file_format[format].clname, fn);
	      last_import_format = format;
	      g_free (last_import_folder);
	      last_import_folder =
		gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (fc));
	    }
	  else
	    {
	      cmd = g_strdup_printf ("load match \"%s\"", fn);
	      g_free (last_load_folder);
	      last_load_folder =
		gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (fc));
	    }
	  if (cmd)
	    {
	      UserCommand (cmd);
	      g_free (cmd);
	    }
	  g_free (fn);
	}
    }
  gtk_widget_destroy (fc);
}

extern void
GTKExport (gpointer * p, guint n, GtkWidget * pw)
{
  SaveCommon (n_file_formats, _("Export to foreign formats"));
}

extern void
GTKSave (gpointer * p, guint n, GtkWidget * pw)
{
  SaveCommon (1, _("Save in native gnubg .sgf format"));
}

extern void
GTKImport (gpointer * p, guint n, GtkWidget * pw)
{
  OpenCommon (n_file_formats, _("Import from foreign formats"));
}

extern void
GTKOpen (gpointer * p, guint n, GtkWidget * pw)
{
  OpenCommon (1, _("Open gnubg .sgf formated file"));
}
