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
#include "gtk/gtk.h"
#include "backgammon.h"
#include "gtkgame.h"

/* --------------------------------------------------------------------------------------------- */

#if GTK_CHECK_VERSION(2,4,0)

/* This is a temporary hack and will move and mutate. Too many places where things
   are hardcoded or omplicated to access -- this clumsy switch is my dynamic copy.
   I'm not convinced a giant switch statement and pseudo-object orientation are the final
   solution, but for the time being it's easy to maintain.
   Disclaimer: This is not optimized or even written with performance as #1 priority (yet),
   and BETA anyway ...
*/

typedef struct {
	char		*description_long,		/* Description of the media type for use in e.g. Bubble Help */
				*description_short,		/* Description of the media type for use in e.g. a label */
				*path_current,			/* Path where the last file operation on this media type took place */
										/* or NULL if none happened during the lifetime of this gnubg */
				*path_default,			/* Path where operations on this media type should be started if */
										/* none happened during the lifetime of this gnubg so far */
				*file_suffix,			/* the file extension used this type */
				*command;				/* corresponding no-gui command FIXME: immature */
	pathformat	path_format;			/* LEGACY: existing enumeration, De-facto ID. */
	int			capabilities;			/* What degree of import / export this format offers? */


} External_IO_Format;

#define External_IO_none		(1<<0)
#define External_IO_position	(1<<1)
#define External_IO_game		(1<<2)
#define External_IO_match		(1<<3)
#define External_IO_session		(1<<4)
#define External_IO_import		(1<<29)
#define External_IO_export		(1<<30)
#define External_IO_all_game_types (External_IO_position | External_IO_game | External_IO_match | External_IO_session)

External_IO_Format *External_IO_Format_new (pathformat id) {

	External_IO_Format eiof;
	External_IO_Format *result;

	switch (id) {
		case PATH_EPS: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Encapsulated Postscript";
			eiof.path_current			= aaszPaths[ PATH_EPS ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_EPS ][ 1 ];
			eiof.path_format			= PATH_EPS;
			eiof.file_suffix			= aszExtensions [PATH_EPS];
			eiof.capabilities			= External_IO_position | External_IO_export;
			break;
					   }
		case PATH_GAM: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Jellyfish Game";
			eiof.path_current			= aaszPaths[ PATH_GAM ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_GAM ][ 1 ];
			eiof.path_format			= PATH_GAM;
			eiof.file_suffix			= aszExtensions [PATH_GAM];
			eiof.capabilities			= External_IO_game | External_IO_import;
			break;
					   }
		case PATH_HTML:{
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Hypertext Markup Language";
			eiof.path_current			= aaszPaths[ PATH_HTML ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_HTML ][ 1 ];
			eiof.path_format			= PATH_HTML;
			eiof.file_suffix			= aszExtensions [PATH_HTML];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
					   }
		case PATH_LATEX: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "LaTeX";
			eiof.path_current			= aaszPaths[ PATH_LATEX ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_LATEX ][ 1 ];
			eiof.path_format			= PATH_LATEX;
			eiof.file_suffix			= aszExtensions [PATH_LATEX];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
						 }
		case PATH_MAT: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Jellyfish Match";
			eiof.path_current			= aaszPaths[ PATH_MAT ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_MAT ][ 1 ];
			eiof.path_format			= PATH_MAT;
			eiof.file_suffix			= aszExtensions [PATH_MAT];
			eiof.capabilities			= External_IO_match | External_IO_import;
			break;
					   }
		case PATH_OLDMOVES: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Fibs Oldmoves";
			eiof.path_current			= aaszPaths[ PATH_OLDMOVES ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_OLDMOVES ][ 1 ];
			eiof.path_format			= PATH_OLDMOVES;
			eiof.file_suffix			= aszExtensions [PATH_OLDMOVES];
			eiof.capabilities			= External_IO_match | External_IO_import; /*? */
			break;
							}
		case PATH_PDF: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Portable Document Format";
			eiof.path_current			= aaszPaths[ PATH_PDF ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_PDF ][ 1 ];
			eiof.path_format			= PATH_PDF;
			eiof.file_suffix			= aszExtensions [PATH_PDF];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
					   }
		case PATH_PNG: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Portable Network Graphics";
			eiof.path_current			= aaszPaths[ PATH_PNG ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_PNG ][ 1 ];
			eiof.path_format			= PATH_PNG;
			eiof.file_suffix			= aszExtensions [PATH_PNG];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
					   } 
		case PATH_POS: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Jellyfish Position";
			eiof.path_current			= aaszPaths[ PATH_POS ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_POS ][ 1 ];
			eiof.path_format			= PATH_POS;
			eiof.file_suffix			= aszExtensions [PATH_POS];
			eiof.capabilities			= External_IO_position | External_IO_import;
			break;
					   }
		case PATH_POSTSCRIPT: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "PostScript";
			eiof.path_current			= aaszPaths[ PATH_POSTSCRIPT ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_POSTSCRIPT ][ 1 ];
			eiof.path_format			= PATH_POSTSCRIPT;
			eiof.file_suffix			= aszExtensions [PATH_POSTSCRIPT];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
							  }
		case PATH_SGF: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Gnu Backgammon Savegame Format";
			eiof.path_current			= aaszPaths[ PATH_SGF ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_SGF ][ 1 ];
			eiof.path_format			= PATH_SGF;
			eiof.file_suffix			= aszExtensions [PATH_SGF];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export | External_IO_import;
			break;
					   }
		case PATH_SGG: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Gamesgrind Save Game";
			eiof.path_current			= aaszPaths[ PATH_SGG ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_SGG ][ 1 ];
			eiof.path_format			= PATH_SGG;
			eiof.file_suffix			= aszExtensions [PATH_SGG];
			eiof.capabilities			= External_IO_game | External_IO_import;
			break;
					   }
		case PATH_TEXT: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Plain Text";
			eiof.path_current			= aaszPaths[ PATH_TEXT ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_TEXT ][ 1 ];
			eiof.path_format			= PATH_TEXT;
			eiof.file_suffix			= aszExtensions [PATH_TEXT];
			eiof.capabilities			= External_IO_all_game_types | External_IO_export;
			break;
						}
		case PATH_MET: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Match Equity Table";
			eiof.path_current			= aaszPaths[ PATH_MET ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_MET ][ 1 ];
			eiof.path_format			= PATH_MET;
			eiof.file_suffix			= aszExtensions [PATH_MET];
			eiof.capabilities			= External_IO_none | External_IO_import;
			break;
					   }
		case PATH_TMG: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "True Moneygames";
			eiof.path_current			= aaszPaths[ PATH_TMG ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_TMG ][ 1 ];
			eiof.path_format			= PATH_TMG;
			eiof.file_suffix			= aszExtensions [PATH_TMG];
			eiof.capabilities			= External_IO_match | External_IO_import; /* ? */
			break;
					   }
		case PATH_BKG: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Hans Berliner's BKG Format";
			eiof.path_current			= aaszPaths[ PATH_BKG ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_BKG ][ 1 ];
			eiof.path_format			= PATH_BKG;
			eiof.file_suffix			= aszExtensions [PATH_BKG];
			eiof.capabilities			= External_IO_match | External_IO_import; /* ? */
			break;
		  }
		case PATH_SNOWIE_TXT: {
			eiof.description_long		= "FIXME";
			eiof.description_short		= "Snowie Text";
			eiof.path_current			= aaszPaths[ PATH_SNOWIE_TXT ][ 0 ];
			eiof.path_default			= aaszPaths[ PATH_SNOWIE_TXT ][ 1 ];
			eiof.path_format			= PATH_SNOWIE_TXT;
			eiof.file_suffix			= aszExtensions [PATH_SNOWIE_TXT ];
			eiof.capabilities			= External_IO_match | External_IO_import; /* ? */
			break;
							  } 
		case NUM_PATHS:
		case PATH_NULL:
		default: {
			eiof.description_long		= "NUM_PATHS is an error and PATH_NULL a hack";
			eiof.description_short		= "error";
			eiof.path_current			= "";
			eiof.path_default			= "";
			eiof.path_format			= PATH_NULL;
			eiof.file_suffix			= "";
			eiof.capabilities			= External_IO_none;
			break;
			}
		}

		result = malloc(sizeof(External_IO_Format));
		memset(result, 0, sizeof(External_IO_Format));

		/*
		result->description_long = malloc(strlen(eiof.description_long));
		result->description_short = malloc(strlen(eiof.description_short));
		result->path_current = malloc(strlen(eiof.path_current));
		result->path_default = malloc(strlen(eiof.path_default));
		result->path_format = eiof.path_format;
		result->file_suffix = malloc(strlen(eiof.file_suffix));
		result->capabilities = eiof.capabilities;

		strcpy(result->description_long, eiof.description_long);
		strcpy(result->description_short, eiof.description_short);
		strcpy(result->path_current, eiof.path_current);
		strcpy(result->path_default, eiof.path_default);
		strcpy(result->file_suffix, eiof.file_suffix);
		*/

		/* no need to copy a lot, everything is static already ... */

		result->description_long = eiof.description_long;
		result->description_short = eiof.description_short;
		result->path_current = eiof.path_current;
		result->path_default = eiof.path_default;
		result->path_format = eiof.path_format;
		result->file_suffix = eiof.file_suffix;
		result->capabilities = eiof.capabilities;


		return result;
}

void External_IO_Format_destroy (External_IO_Format *eiof) {

	if (eiof) {
		/*
		if (eiof->description_long) free(eiof->description_long);
		if (eiof->description_short) free(eiof->description_short);
		if (eiof->path_current) free(eiof->path_current);
		if (eiof->path_default) free(eiof->path_default);
		if (eiof->file_suffix) free(eiof->file_suffix);
		*/
		free(eiof);
	}
}

extern void GTKFileCommand24( char *szPrompt, char *szDefault, char *szCommand,
                              char *szPath, filedialogtype fdt, pathformat pathId) {

  	char *filename;				/* the result */
	External_IO_Format *eiof;	/* a bunch of data on a file format */

	GtkWidget *filechooser;

	char *dialogTitle;

	/* Determine dialog title */
	if (szPrompt) {								/* FIXME: When the eiof type ist is complete, all */
		dialogTitle = szPrompt;					/* titles should be generated.  */
	} else if (fdt == FDT_NONE_SAVE) {
		dialogTitle = "GNU Backgammon - Save File";
	} else {
		dialogTitle = "GNU Backgammon - Open File";
	}

	/* Creaze dialog */
	filechooser= gtk_file_chooser_dialog_new (
					  dialogTitle,										/* Window Text */
				      NULL,												/* Parent Window */
					  (fdt == FDT_NONE_OPEN)
							? GTK_FILE_CHOOSER_ACTION_OPEN				/* Action */
							: GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,			/* List of Buttons */
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);
	gtk_window_set_modal( GTK_WINDOW( filechooser ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( filechooser ), GTK_WINDOW( pwMain ) );
	/* Our DAO */

	eiof = External_IO_Format_new(pathId);

	/* Set default path on dialog */
	gtk_file_chooser_set_current_folder((GtkFileChooser *) filechooser, eiof->path_current);

	
	if (fdt == FDT_NONE_SAVE) { /* Case: we want to save */
		/* Set default filename */
		gtk_file_chooser_set_current_name((GtkFileChooser *) filechooser, getDefaultFileName(pathId));

	} else { /* FTD_NONE_OPEN, we want to load */

		/* Create 2 File Filters: *.* and *.suffix */

		char *suffixGlobber;
		char *filterDescription; 

		GtkFileFilter *anyFileFilter, *suffixFileFilter;

		filterDescription = g_strconcat ("Show ", eiof->description_short, " files (*.", eiof->file_suffix, ") only", NULL);
		suffixGlobber = g_strconcat("*.", eiof->file_suffix, NULL );

		suffixFileFilter = gtk_file_filter_new();
		anyFileFilter =  gtk_file_filter_new();
		gtk_file_filter_add_pattern (suffixFileFilter, suffixGlobber);
		gtk_file_filter_add_pattern (anyFileFilter, "*");
		gtk_file_filter_set_name    (anyFileFilter, "Show all Files (*)");
		gtk_file_filter_set_name    (suffixFileFilter, filterDescription);
		/* Only show *.suffix filter if suffix is not empty */
		if (*eiof->file_suffix)
			gtk_file_chooser_add_filter ((GtkFileChooser *) filechooser, suffixFileFilter);
		gtk_file_chooser_add_filter ((GtkFileChooser *) filechooser, anyFileFilter);

		if (filterDescription) g_free(filterDescription);
		if (suffixGlobber) g_free(suffixGlobber);

	}
	
	/* launch dialog */
	if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT) {
	    filename = gtk_file_chooser_get_filename ((GtkFileChooser *)  filechooser);

		if (filename) {
#if __GNUC__
			char sz[ strlen( filename ) + strlen( szCommand ) + 4 ];
#elif HAVE_ALLOCA
			char *sz = alloca( strlen( filename ) + strlen( szCommand ) + 4 );
#elif GLIB_CHECK_VERSION(1,1,12)
			char *sz = g_alloca(strlen( filename ) + strlen( szCommand ) + 4);
#else
			char sz[ 1024 ];
#endif
			/* and actually do the requested IO */
			sprintf( sz, "%s \"%s\"", szCommand, filename );
			UserCommand( sz );
			g_free (filename);
		}
	}
	/* Cleanup */

	gtk_widget_destroy (filechooser);
	External_IO_Format_destroy(eiof);
}
#endif

/* ------------------------------------------------------------------------------------------ */
