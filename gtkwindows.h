
#ifndef _GTKWINDOWS_H_
#define _GTKWINDOWS_H_

#define GTK_STOCK_DIALOG_GNU "gtk-dialog-gnu" /* stock gnu head icon */
#define GTK_STOCK_DIALOG_GNU_QUESTION "gtk-dialog-gnuquestion" /* stock gnu head icon with question mark */
#define GTK_STOCK_DIALOG_GNU_BIG "gtk-dialog-gnubig" /* large gnu icon */

enum {	/* Dialog flags */
	DIALOG_FLAG_NONE = 0,
	DIALOG_FLAG_MODAL = 1,
	DIALOG_FLAG_CUSTOM_PICKMAP = 2,
	DIALOG_FLAG_NOOK = 4,
	DIALOG_FLAG_CLOSEBUTTON = 8,
	DIALOG_FLAG_NOTIDY = 16,
	DIALOG_FLAG_MINMAXBUTTONS = 32
};

typedef enum _dialogarea {
    DA_MAIN,
    DA_BUTTONS,
    DA_OK
} dialogarea;

typedef enum _dialogtype {
    DT_INFO,
    DT_QUESTION,
    DT_AREYOUSURE,
    DT_WARNING,
    DT_ERROR,
    DT_GNU,
	DT_GNUQUESTION,
    NUM_DIALOG_TYPES
} dialogtype;

extern GtkWidget *GTKCreateDialog( const char *szTitle, const dialogtype dt, 
                                   GtkWidget *parent, int flags, GtkSignalFunc pf, void *p );
extern GtkWidget *DialogArea( GtkWidget *pw, dialogarea da );
    
extern int GTKGetInputYN( char *szPrompt );
extern char* GTKGetInput(char* title, char* prompt);

extern int GTKMessage( char *sz, dialogtype dt );
extern void GTKSetCurrentParent(GtkWidget *parent);
extern GtkWidget *GTKGetCurrentParent();

typedef enum _warnings { 
  WARN_FULLSCREEN_EXIT=0, WARN_QUICKDRAW_MODE, WARN_SET_SHADOWS, 
	  WARN_UNACCELERATED, WARN_NUM_WARNINGS
} warnings;

extern void GTKShowWarning(warnings warning, GtkWidget *pwParent);
extern char* warningStrings[WARN_NUM_WARNINGS];
extern char* warningNames[WARN_NUM_WARNINGS];
extern int warningEnabled[WARN_NUM_WARNINGS];

#endif
