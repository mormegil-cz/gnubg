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

#include <config.h>
#include "backgammon.h"

#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gtkfile.h"
#include "gtkgame.h"
#include "gtkwindows.h"

typedef struct _FileFormat FileFormat;
struct _FileFormat {
    char *extension;
    char *description;
    char *clname;
    int canimport;
    int canexport;
    int exports[3];
};

/* char *extension; char *description; char *clname;
 * gboolean canimport; gboolean canexport; gboolean exports[3]; */
FileFormat file_format[] = {
  {".sgf", N_("Gnu Backgammon File"), "sgf", TRUE, TRUE, {TRUE, TRUE, TRUE}}, /*must be the first element*/
  {".eps", N_("Encapsulated Postscript"), "eps", FALSE, TRUE, {FALSE, FALSE, TRUE}},
  {".fibs", N_("Fibs Oldmoves"), "oldmoves", FALSE, FALSE, {FALSE, FALSE, FALSE}},
  {".sgg", N_("Gamesgrid Save Game"), "sgg", TRUE, FALSE, {FALSE, FALSE, FALSE}},
  {".bkg", N_("Hans Berliner's BKG Format"), "bkg", TRUE, FALSE, {FALSE, FALSE, FALSE}},
  {".html", N_("HTML"), "html", FALSE, TRUE, {TRUE, TRUE, TRUE}},
  {".gam", N_("Jellyfish Game"), "gam", FALSE, TRUE, {FALSE, TRUE, FALSE}},
  {".mat", N_("Jellyfish Match"), "mat", TRUE, TRUE, {TRUE, FALSE, FALSE}},
  {".pos", N_("Jellyfish Position"), "pos", TRUE, TRUE, {FALSE, FALSE, TRUE}},
  {".tex", N_("LaTeX"), "latex", FALSE, TRUE, {TRUE, TRUE, FALSE}},
  {".pdf", N_("PDF"), "pdf", FALSE, TRUE, {TRUE, TRUE, FALSE}},
  {".txt", N_("Plain Text"), "text", FALSE, TRUE, {TRUE, TRUE, TRUE}},
  {".png", N_("Portable Network Graphics"), "pdf", FALSE, TRUE, {FALSE, FALSE, TRUE}},
  {".ps", N_("PostScript"), "postscript", FALSE, TRUE, {TRUE, TRUE, FALSE}},
  {".txt", N_("Snowie Text"), "snowietxt", TRUE, TRUE, {FALSE, FALSE, TRUE}},
  {".tmg", N_("True Moneygames"), "tmg", TRUE, FALSE, {FALSE, FALSE, FALSE}},
  {".gam", N_("GammonEmpire Game"), "empire", TRUE, FALSE, {FALSE, FALSE, FALSE}},
  {".gam", N_("PartyGammon Game"), "party", TRUE, FALSE, {FALSE, FALSE, FALSE}}
};

gint n_file_formats = G_N_ELEMENTS(file_format);

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
#if WIN32
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

#if WIN32
  programdir = g_strdup(szDataDirectory);
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
      if ((fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (pso->fc))) != NULL)
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
  static guint last_export_format = 0;
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

/* Data structures and functions for getting file type data */

typedef struct _FilePreviewData
{
	FileFormat *format;
} FilePreviewData;

typedef struct _FileHelper
{
	FILE *fp;
	int dataRead;
	int dataPos;
	char* data;
} FileHelper;

FileHelper *OpenFileHelper(char *filename)
{
	FileHelper *fh;
	if (access(filename, R_OK))
		return NULL;	/* File not found */

	fh = malloc(sizeof(FileHelper));
	fh->fp = fopen(filename, "r");
	if (!fh->fp)
	{	/* Failed to open file */
		free(fh);
		return NULL;
	}
	fh->dataRead = 0;
	fh->dataPos = 0;
	fh->data = NULL;
	return fh;
}

void CloseFileHelper(FileHelper *fh)
{
	fclose(fh->fp);
	free(fh->data);
}

void fhReset(FileHelper *fh)
{	/* Reset data pointer to start of file */
	fh->dataPos = 0;
}

void fhDataGetChar(FileHelper *fh)
{
	int read;
	if (fh->dataPos < fh->dataRead)
		return;

#define BLOCK_SIZE 1024
#define MAX_READ_SIZE 5000
	fh->data = realloc(fh->data, fh->dataRead + BLOCK_SIZE);
	if (fh->dataRead > MAX_READ_SIZE)
		read = 0;	/* Too big - should have worked things out by now! */
	else
		read = fread(fh->data + fh->dataRead, 1, BLOCK_SIZE, fh->fp);
	if (read < BLOCK_SIZE)
	{
		(fh->data + fh->dataRead)[read] = '\0';
		read++;
	}
	fh->dataRead += read;
}

char fhPeekNextChar(FileHelper *fh)
{
	fhDataGetChar(fh);
	return fh->data[fh->dataPos];
}

int fhEOF(FileHelper *fh)
{
	return fhPeekNextChar(fh) == '\0';
}

char fhReadNextChar(FileHelper *fh)
{
	fhDataGetChar(fh);
	return fh->data[fh->dataPos++];
}

int fhPeekNextIsWS(FileHelper *fh)
{
	char c = fhPeekNextChar(fh);
	return (c == ' ' || c == '\t' || c == '\n');
}

void fhSkipWS(FileHelper *fh)
{
	while(fhPeekNextIsWS(fh))
		fhReadNextChar(fh);
}

int fhSkipToEOL(FileHelper *fh)
{
	char c;
	do
	{
		c = fhReadNextChar(fh);
		if (c == '\n')
			return TRUE;
	} while (c != '\0');

	return FALSE;
}

int fhReadString(FileHelper *fh, char *str)
{	/* Check file has str next */
	while (*str)
	{
		if (fhReadNextChar(fh) != *str)
			return FALSE;
		str++;
	}
	return TRUE;
}

int fhReadStringNC(FileHelper *fh, char *str)
{	/* Check file has str next (ignoring case) */
	while (*str)
	{
		char c = fhReadNextChar(fh);
		if (tolower(c) != *str)
			return FALSE;
		str++;
	}
	return TRUE;
}

int fhPeekStringNC(FileHelper *fh, char *str)
{	/* Check file has str next (ignoring case) but don't move */
	int pos = fh->dataPos;
	int ret = TRUE;
	while (*str)
	{
		char c = fhReadNextChar(fh);
		if (tolower(c) != *str)
		{
			ret = FALSE;
			break;
		}
		str++;
	}
	fh->dataPos = pos;
	return ret;
}

int fhReadNumber(FileHelper *fh)
{	/* Check file has str next */
	int anyNumbers = FALSE;
	do
	{
		char c = fhPeekNextChar(fh);
		if (!isdigit(c))
			return anyNumbers;
		anyNumbers = TRUE;
	} while (fhReadNextChar(fh) != '\0');

	return TRUE;
}

int fhReadAnyAlphNumString(FileHelper *fh)
{
	char c = fhPeekNextChar(fh);
	if (!isalnum(c))
		return FALSE;
	do
	{
		char c = fhPeekNextChar(fh);
		if (!isalnum(c) && c != '_')
			return fhPeekNextIsWS(fh);
	} while (fhReadNextChar(fh) != '\0');
	return TRUE;
}

int IsSGFFile(FileHelper *fh)
{
	char *elements[] = {"(", ";", "FF", "[", "4", "]", "GM", "[", "6", "]", ""};
	char **test = elements;

	fhReset(fh);
	while (**test)
	{
		fhSkipWS(fh);
		if (!fhReadString(fh, *test))
			return FALSE;
		test++;
	}
	return TRUE;
}

int IsSGGFile(FileHelper *fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadAnyAlphNumString(fh))
	{
		fhSkipWS(fh);
		if (!fhReadString(fh, "vs."))
			return FALSE;
		fhSkipWS(fh);
		if (fhReadAnyAlphNumString(fh))
			return TRUE;
	}
	return FALSE;
}

int IsMATFile(FileHelper *fh)
{
	fhReset(fh);
	do
	{
		char c;
		fhSkipWS(fh);
		c = fhPeekNextChar(fh);
		if (isdigit(c))
		{
			if (fhReadNumber(fh))
			{
				fhSkipWS(fh);
				if (fhReadStringNC(fh, "point"))
				{
					fhSkipWS(fh);
					if (fhReadStringNC(fh, "match"))
						return TRUE;
				}
			}
			return FALSE;
		}
	} while (fhSkipToEOL(fh));
	return FALSE;
}

int IsTMGFile(FileHelper *fh)
{
	fhReset(fh);
	do
	{
		fhSkipWS(fh);
		if (fhPeekStringNC(fh, "game"))
		{
			fhReadStringNC(fh, "game");
			fhSkipWS(fh);
			return fhReadNumber(fh) && (fhPeekNextChar(fh) == ':');
		}
		fhReadAnyAlphNumString(fh);
		if (fhPeekNextChar(fh) != ':')
			return FALSE;
	} while (fhSkipToEOL(fh));

	return FALSE;
}

int IsTXTFile(FileHelper *fh)
{
	fhReset(fh);
	fhSkipWS(fh);
	if (fhReadNumber(fh) && fhReadNextChar(fh) == ';')
	{
		fhSkipWS(fh);
		if (fhReadNumber(fh) && fhReadNextChar(fh) == ';')
			return TRUE;
	}
	return FALSE;
}

int IsJFPFile(FileHelper *fh)
{
	fhReset(fh);
	if ((fhReadNextChar(fh) == 126) && (fhReadNextChar(fh) == '\0') &&
		(fhReadNextChar(fh) == '\0') && (fhReadNextChar(fh) == '\0'))
		return TRUE;

	return FALSE;
}

int IsBKGFile(FileHelper *fh)
{
	fhReset(fh);
	fhSkipWS(fh);
	if (fhReadString(fh, "Black"))
		return TRUE;
	fhReset(fh);
	fhSkipWS(fh);
	if (fhReadString(fh, "White"))
		return TRUE;

	return FALSE;
}

int IsGAMFile(FileHelper *fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadAnyAlphNumString(fh))
	{
		fhSkipWS(fh);
		if (fhReadAnyAlphNumString(fh))
		{
			fhSkipWS(fh);
			return fhReadString(fh, "1)");
		}
	}

	return FALSE;
}

int IsPARFile(FileHelper *fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadStringNC(fh, "boardid="))
	{
		fhSkipToEOL(fh);
		if (fhReadStringNC(fh, "creator="))
			return TRUE;
	}

	return FALSE;
}

FilePreviewData *ReadFilePreview(char *filename)
{
	FilePreviewData *fpd;
	FileHelper *fh = OpenFileHelper(filename);
	if (!fh)
		return 0;

	fpd = malloc(sizeof(FilePreviewData));
	fpd->format = NULL;

	if (IsSGFFile(fh))
		fpd->format = &file_format[0];
	else if (IsSGGFile(fh))
		fpd->format = &file_format[3];
	else if (IsTXTFile(fh))
		fpd->format = &file_format[14];
	else if (IsTMGFile(fh))
		fpd->format = &file_format[15];
	else if (IsMATFile(fh))
		fpd->format = &file_format[7];
	else if (IsJFPFile(fh))
		fpd->format = &file_format[8];
	else if (IsBKGFile(fh))
		fpd->format = &file_format[4];
	else if (IsGAMFile(fh))
		fpd->format = &file_format[16];
	else if (IsPARFile(fh))
		fpd->format = &file_format[17];

	CloseFileHelper(fh);
	return fpd;
}

static void
update_preview_cb (GtkFileChooser *file_chooser, gpointer data)
{
	GtkWidget *preview = GTK_WIDGET (data);
	char *filename = gtk_file_chooser_get_preview_filename (file_chooser);
	FilePreviewData *fdp = ReadFilePreview(filename);
	char *label = "";

	gtk_dialog_set_response_sensitive(GTK_DIALOG(file_chooser), GTK_RESPONSE_ACCEPT, FALSE);
	if (fdp)
	{
		if (!fdp->format)
			label = _("not a backgammon file");
		else
		{
			label = gettext(fdp->format->description);
			gtk_dialog_set_response_sensitive(GTK_DIALOG(file_chooser), GTK_RESPONSE_ACCEPT, TRUE);
		}

		free(fdp);
	}
	gtk_label_set_text(GTK_LABEL(preview), label);
}

extern void GTKOpen (gpointer * p, guint n, GtkWidget * pw)
{
  gchar *fn, *sg, *cmd = NULL;
  int i;
  GtkFileFilter *aff;
  GtkWidget *fc, *preview;
  static gchar *last_import_folder = NULL;
  gchar *folder = NULL;

  folder = last_import_folder ? last_import_folder : default_import_folder;

  fc = GnuBGFileDialog (_("Open backgammon file"), folder, NULL, GTK_FILE_CHOOSER_ACTION_OPEN);

  preview = gtk_label_new("");
  gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(fc), preview);
  g_signal_connect (GTK_FILE_CHOOSER(fc), "update-preview", G_CALLBACK (update_preview_cb), preview);

  aff = gtk_file_filter_new ();
  gtk_file_filter_set_name (aff, _("Supported files"));
  for (i = 0; i < n_file_formats; ++i)
    {
      if (!file_format[i].canimport)
	continue;
      sg = g_strdup_printf ("*%s", file_format[i].extension);
      gtk_file_filter_add_pattern (aff, sg);
      g_free (sg);
    }
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fc), aff);
  FilterAdd (_("All Files"), "*", GTK_FILE_CHOOSER (fc));
  for (i = 0; i < n_file_formats; ++i)
    {
      if (!file_format[i].canimport)
	continue;
      sg = g_strdup_printf ("*%s", file_format[i].extension);
      FilterAdd (file_format[i].description, sg, GTK_FILE_CHOOSER (fc));
      g_free (sg);
    }

	if (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_ACCEPT)
	{
		fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (fc));
		if (fn)
		{
			char *tempFile = NULL;
			FilePreviewData *fdp = ReadFilePreview(fn);

			if (fdp && fdp->format)
			{
				if (fdp->format == &file_format[0])
				{
					cmd = g_strdup_printf ("load match \"%s\"", fn);
					g_free (last_import_folder);
					last_import_folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (fc));
				}
				else
				{
						cmd = g_strdup_printf ("import %s \"%s\"", fdp->format->clname, fn);
						g_free (last_import_folder);
						last_import_folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (fc));
				}
			}
			free(fdp);

			if (cmd)
			{
				UserCommand (cmd);
				g_free (cmd);
				if (tempFile)
					unlink(tempFile);	/* Remove temporary PartyGammon mat file */
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
