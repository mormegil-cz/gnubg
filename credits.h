/* Do not modify this file!  It is created automatically by credits.sh.
   Modify credits.sh instead. */

#include <glib/gi18n.h>

typedef struct _credEntry {
	char* Name;
	char* Type;
} credEntry;

typedef struct _credits {
	char* Title;
	credEntry *Entry;
} credits;

extern credEntry ceAuthors[];
extern credEntry ceContrib[];
extern credEntry ceTranslations[];
extern credEntry ceSupport[];
extern credEntry ceCredits[];

extern credits creditList[];

extern char aszAUTHORS[];

extern char aszCOPYRIGHT[];

