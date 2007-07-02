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

typedef struct _FileFormat FileFormat;
struct _FileFormat {
	char *extension;
	char *description;
	char *clname;
	int canimport;
	int canexport;
	int exports[3];
};

typedef struct _FilePreviewData {
	FileFormat *format;
} FilePreviewData;

extern FileFormat file_format[];
extern gint n_file_formats;

extern int FormatFromDescription(const gchar * text);
extern char *GetFilename(int CheckForCurrent, int format);
extern FilePreviewData *ReadFilePreview(char *filename);
