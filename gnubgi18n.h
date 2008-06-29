
extern int CheckGameExists();

extern char *gnubg_gettext(const char *msgid);
extern void InitTranslationLists();
extern void FreeTranslationLists();

extern char *ngettext (const char *__msgid1, const char *__msgid2, unsigned long int __n);

#define gettext(s) gnubg_gettext(s)
#define  _(String) gnubg_gettext(String)
#define N_(String) (String)
