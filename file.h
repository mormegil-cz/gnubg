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
