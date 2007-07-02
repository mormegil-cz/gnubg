/*
 * file.c
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
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
//#include <glib/gstdio.h>
#include "file.h"

/* char *extension; char *description; char *clname;
 * gboolean canimport; gboolean canexport; gboolean exports[3]; */
FileFormat file_format[] = {
	{".sgf", N_("Gnu Backgammon File"), "sgf", TRUE, TRUE, {TRUE, TRUE, TRUE}},	/*must be the first element */
	{".eps", N_("Encapsulated Postscript"), "eps", FALSE, TRUE,
	 {FALSE, FALSE, TRUE}},
	{".fibs", N_("Fibs Oldmoves"), "oldmoves", FALSE, FALSE,
	 {FALSE, FALSE, FALSE}},
	{".sgg", N_("Gamesgrid Save Game"), "sgg", TRUE, FALSE,
	 {FALSE, FALSE, FALSE}},
	{".bkg", N_("Hans Berliner's BKG Format"), "bkg", TRUE, FALSE,
	 {FALSE, FALSE, FALSE}},
	{".html", N_("HTML"), "html", FALSE, TRUE, {TRUE, TRUE, TRUE}},
	{".gam", N_("Jellyfish Game"), "gam", FALSE, TRUE,
	 {FALSE, TRUE, FALSE}},
	{".mat", N_("Jellyfish Match"), "mat", TRUE, TRUE,
	 {TRUE, FALSE, FALSE}},
	{".pos", N_("Jellyfish Position"), "pos", TRUE, TRUE,
	 {FALSE, FALSE, TRUE}},
	{".tex", N_("LaTeX"), "latex", FALSE, TRUE, {TRUE, TRUE, FALSE}},
	{".pdf", N_("PDF"), "pdf", FALSE, TRUE, {TRUE, TRUE, FALSE}},
	{".txt", N_("Plain Text"), "text", FALSE, TRUE,
	 {TRUE, TRUE, TRUE}},
	{".png", N_("Portable Network Graphics"), "png", FALSE, TRUE,
	 {FALSE, FALSE, TRUE}},
	{".ps", N_("PostScript"), "postscript", FALSE, TRUE,
	 {TRUE, TRUE, FALSE}},
	{".txt", N_("Snowie Text"), "snowietxt", TRUE, TRUE,
	 {FALSE, FALSE, TRUE}},
	{".tmg", N_("True Moneygames"), "tmg", TRUE, FALSE,
	 {FALSE, FALSE, FALSE}},
	{".gam", N_("GammonEmpire Game"), "empire", TRUE, FALSE,
	 {FALSE, FALSE, FALSE}},
	{".gam", N_("PartyGammon Game"), "party", TRUE, FALSE,
	 {FALSE, FALSE, FALSE}}
};

gint n_file_formats = G_N_ELEMENTS(file_format);

typedef struct _FileHelper {
	FILE *fp;
	int dataRead;
	int dataPos;
	char *data;
} FileHelper;

extern int FormatFromDescription(const gchar * text)
{
	gint i;
	if (!text)
		return -1;
	for (i = 0; i < n_file_formats; i++) {
		if (!strcasecmp(text, file_format[i].description))
			break;
	}
	return i;
}

/* Data structures and functions for getting file type data */

static FileHelper *OpenFileHelper(char *filename)
{
	FileHelper *fh;
	if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		return NULL;	/* File not found */

	fh = g_new(FileHelper, 1);
	fh->fp = fopen(filename, "r");
	if (!fh->fp) {		/* Failed to open file */
		g_free(fh);
		return NULL;
	}
	fh->dataRead = 0;
	fh->dataPos = 0;
	fh->data = NULL;
	return fh;
}

static void CloseFileHelper(FileHelper * fh)
{
	fclose(fh->fp);
	g_free(fh->data);
}

static void fhReset(FileHelper * fh)
{				/* Reset data pointer to start of file */
	fh->dataPos = 0;
}

static void fhDataGetChar(FileHelper * fh)
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
		read =
		    fread(fh->data + fh->dataRead, 1, BLOCK_SIZE, fh->fp);
	if (read < BLOCK_SIZE) {
		(fh->data + fh->dataRead)[read] = '\0';
		read++;
	}
	fh->dataRead += read;
}

static char fhPeekNextChar(FileHelper * fh)
{
	fhDataGetChar(fh);
	return fh->data[fh->dataPos];
}

static char fhReadNextChar(FileHelper * fh)
{
	fhDataGetChar(fh);
	return fh->data[fh->dataPos++];
}

static int fhPeekNextIsWS(FileHelper * fh)
{
	char c = fhPeekNextChar(fh);
	return (c == ' ' || c == '\t' || c == '\n');
}

static void fhSkipWS(FileHelper * fh)
{
	while (fhPeekNextIsWS(fh))
		fhReadNextChar(fh);
}

static int fhSkipToEOL(FileHelper * fh)
{
	char c;
	do {
		c = fhReadNextChar(fh);
		if (c == '\n')
			return TRUE;
	} while (c != '\0');

	return FALSE;
}

static int fhReadString(FileHelper * fh, char *str)
{				/* Check file has str next */
	while (*str) {
		if (fhReadNextChar(fh) != *str)
			return FALSE;
		str++;
	}
	return TRUE;
}

static int fhReadStringNC(FileHelper * fh, char *str)
{				/* Check file has str next (ignoring case) */
	while (*str) {
		char c = fhReadNextChar(fh);
		if (g_ascii_tolower(c) != *str)
			return FALSE;
		str++;
	}
	return TRUE;
}

static int fhPeekStringNC(FileHelper * fh, char *str)
{				/* Check file has str next (ignoring case) but don't move */
	int pos = fh->dataPos;
	int ret = TRUE;
	while (*str) {
		char c = fhReadNextChar(fh);
		if (g_ascii_tolower(c) != *str) {
			ret = FALSE;
			break;
		}
		str++;
	}
	fh->dataPos = pos;
	return ret;
}

static int fhReadNumber(FileHelper * fh)
{				/* Check file has str next */
	int anyNumbers = FALSE;
	do {
		char c = fhPeekNextChar(fh);
		if (!g_ascii_isdigit(c))
			return anyNumbers;
		anyNumbers = TRUE;
	} while (fhReadNextChar(fh) != '\0');

	return TRUE;
}

static int fhReadAnyAlphNumString(FileHelper * fh)
{
	char c = fhPeekNextChar(fh);
	if (!g_ascii_isalnum(c))
		return FALSE;
	do {
		char c = fhPeekNextChar(fh);
		if (!g_ascii_isalnum(c) && c != '_')
			return fhPeekNextIsWS(fh);
	} while (fhReadNextChar(fh) != '\0');
	return TRUE;
}

static int IsSGFFile(FileHelper * fh)
{
	char *elements[] =
	    { "(", ";", "FF", "[", "4", "]", "GM", "[", "6", "]", "" };
	char **test = elements;

	fhReset(fh);
	while (**test) {
		fhSkipWS(fh);
		if (!fhReadString(fh, *test))
			return FALSE;
		test++;
	}
	return TRUE;
}

static int IsSGGFile(FileHelper * fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadAnyAlphNumString(fh)) {
		fhSkipWS(fh);
		if (!fhReadString(fh, "vs."))
			return FALSE;
		fhSkipWS(fh);
		if (fhReadAnyAlphNumString(fh))
			return TRUE;
	}
	return FALSE;
}

static int IsMATFile(FileHelper * fh)
{
	fhReset(fh);
	do {
		char c;
		fhSkipWS(fh);
		c = fhPeekNextChar(fh);
		if (g_ascii_isdigit(c)) {
			if (fhReadNumber(fh)) {
				fhSkipWS(fh);
				if (fhReadStringNC(fh, "point")) {
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

static int IsTMGFile(FileHelper * fh)
{
	fhReset(fh);
	do {
		fhSkipWS(fh);
		if (fhPeekStringNC(fh, "game")) {
			fhReadStringNC(fh, "game");
			fhSkipWS(fh);
			return fhReadNumber(fh)
			    && (fhPeekNextChar(fh) == ':');
		}
		fhReadAnyAlphNumString(fh);
		if (fhPeekNextChar(fh) != ':')
			return FALSE;
	} while (fhSkipToEOL(fh));

	return FALSE;
}

static int IsTXTFile(FileHelper * fh)
{
	fhReset(fh);
	fhSkipWS(fh);
	if (fhReadNumber(fh) && fhReadNextChar(fh) == ';') {
		fhSkipWS(fh);
		if (fhReadNumber(fh) && fhReadNextChar(fh) == ';')
			return TRUE;
	}
	return FALSE;
}

static int IsJFPFile(FileHelper * fh)
{
	fhReset(fh);
	if ((fhReadNextChar(fh) == 126) && (fhReadNextChar(fh) == '\0') &&
	    (fhReadNextChar(fh) == '\0') && (fhReadNextChar(fh) == '\0'))
		return TRUE;

	return FALSE;
}

static int IsBKGFile(FileHelper * fh)
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

static int IsGAMFile(FileHelper * fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadAnyAlphNumString(fh)) {
		fhSkipWS(fh);
		if (fhReadAnyAlphNumString(fh)) {
			fhSkipWS(fh);
			return fhReadString(fh, "1)");
		}
	}

	return FALSE;
}

static int IsPARFile(FileHelper * fh)
{
	fhReset(fh);
	fhSkipWS(fh);

	if (fhReadStringNC(fh, "boardid=")) {
		fhSkipToEOL(fh);
		if (fhReadStringNC(fh, "creator="))
			return TRUE;
	}

	return FALSE;
}

extern FilePreviewData *ReadFilePreview(char *filename)
{
	FilePreviewData *fpd;
	FileHelper *fh = OpenFileHelper(filename);
	if (!fh)
		return 0;

	fpd = g_new0(FilePreviewData, 1);

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
extern char *GetFilename(int CheckForCurrent, int format)
{
	char *sz, tstr[15];
	time_t t;

	if (CheckForCurrent && szCurrentFileName && *szCurrentFileName)
		sz = g_strdup_printf("%s%s", szCurrentFileName,
				     file_format[format].extension);
	else {
		if (mi.nYear)
			sprintf(tstr, "%04d-%02d-%02d", mi.nYear,
				mi.nMonth, mi.nDay);
		else {
			time(&t);
			strftime(tstr, 14, _("%Y-%m-%d-%H%M"),
				 localtime(&t));
		}
		sz = g_strdup_printf("%s-%s_%dp_%s.sgf", ap[0].szName,
				     ap[1].szName, ms.nMatchTo, tstr);
	}
	return sz;
}
