/*
 * ext.h
 *
 * by Gary Wong, 1997-2000
 *
 */

#ifndef _EXT_H_
#define _EXT_H_

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#include <event.h>
#include <list.h>

#define EXTMAXRESOURCELEN 16

typedef struct _extdisplay {
    Display *pdsp;
    list levReply, levRequestErrorHandler;
    event ev;    
    unsigned long nLast;
} extdisplay;

extern int ExtDspCreate( extdisplay *pedsp, Display *pdsp );
extern int ExtDspDestroy( extdisplay *pedsp );
extern int ExtDspReplyEvent( extdisplay *pedsp, unsigned long n, event *pev );
extern int ExtDspHandleNextError( extdisplay *pedsp,
				  int ( *fn )( extdisplay *pedsp,
					       XErrorEvent *pxeev ) );
extern extdisplay *ExtDspFind( Display *pdsp );

extern int InitExt( void );

typedef struct _extquark {
    char *sz;
    XrmQuark q;
} extquark;

extern int ExtQuarkCreate( extquark *peq );

typedef struct _extdefault {
    extquark *peqName;
    char *szDefault;
} extdefault;

typedef struct _extresource {
    XrmName aqName[ EXTMAXRESOURCELEN ];
    XrmClass aqClass[ EXTMAXRESOURCELEN ];
    extdefault *paed;
    XrmDatabase rdb;
} extresource;

extern int ExtResCreateS( extresource *peres, extresource *peresParent,
			  char *szName, char *szClass,
			  extdefault *paed, XrmDatabase rdb );
extern int ExtResCreate( extresource *peres, extresource *peresParent,
			 extquark *peqName, extquark *peqClass,
			 extdefault *paed, XrmDatabase rdb );
extern int ExtResCreateQ( extresource *peres, extresource *peresParent,
			  XrmName qName, XrmClass qClass,
			  extdefault *paed, XrmDatabase rdb );
extern int ExtResLookupS( extresource *peres, char *szName, char *szClass,
			  char **ppszType, XrmValue *pval );
extern int ExtResLookup( extresource *peres, extquark *peqName,
			 extquark *peqClass, XrmRepresentation *pqRep,
			 XrmValue *pval );
extern int ExtResLookupQ( extresource *peres, XrmNameList aqNameTail,
			  XrmClassList aqClassTail,
			  XrmRepresentation *pqRep, XrmValue *pval );

struct _extwindow;

typedef int ( *xeventhandler )( struct _extwindow *pewnd, XEvent *pxev );

typedef struct _extwindowclass {
    long flEventMask;
    int cxInc, cyInc, cxDelta, cyDelta; /* FIXME this isn't fixed (depends on
					   font -- make it a function?) */
    /* FIXME other window manager hint things */
    xeventhandler pxevh;
    char *szClass;
    extdefault *paed;
} extwindowclass;

typedef struct _extcolour {
    XColor xcol;
    int idDsp;
    Colormap cmap;
    int cRef;
} extcolour;

typedef struct _extwindow {
    Display *pdsp;
    Window wnd;
    unsigned int cx, cy;
    Window wndOwner; /* wndParent? */
    unsigned int id;
    Colormap cm;
    VisualID vid;
    Window wndRoot; /* these 2 for GC sharing */
    int nDepth;
    extcolour *pecolBorder, *pecolBackground;
    extresource eres;
    extwindowclass *pewc;
    void *pv;
} extwindow;

typedef struct _extwindowspec {
    char *szName;
    extwindowclass *pewc;
    extdefault *paed;
    void *pv;
    unsigned int id;
} extwindowspec;

extern int ExtWndCreate( extwindow *pewnd, extresource *peresParent,
			 char *szName, extwindowclass *pewc,
			 XrmDatabase rdb, extdefault *paed, void *pv );
extern int ExtWndDestroy( extwindow *pewnd );
extern int ExtWndRealise( extwindow *pewnd, Display *pdsp,
			  Window wndParent,
			  char *pszGeometry /* FIXME no need */,
			  Window wndOwner, unsigned int id );
extern int ExtWndAttach( extwindow *pewnd, Display *pdsp, Window wnd );
extern extwindow *ExtWndCreateWindows( extwindow *pewndParent,
				       extwindowspec *paews, int c );
    
extern XColor *ExtWndLookupColour( extwindow *pewnd, char *szName );
extern int ExtWndAttachColour( extwindow *pewnd, extquark *peqName,
			       extquark *peqClass, XColor *pxcol,
			       extcolour **ppecol );
extern XFontStruct *ExtWndAttachFont( extwindow *pewnd, extquark *peqName,
				      extquark *peqClass, char *szDefault );
extern GC ExtWndAttachGC( extwindow *pewnd, unsigned long flValues,
			  XGCValues *pxgcv );

extern int ExtWndDetachColour( extwindow *pewnd, extcolour *pecol );
extern int ExtWndDetachFont( extwindow *pewnd, Font font );
extern int ExtWndDetachGC( extwindow *pewnd, GC gc );

extern int ExtHandler( extwindow *pewnd, XEvent *pxev );

#define ExtPreCreateNotify 0x7F
#define ExtCreateNotify 0x7E
#define ExtDestroyNotify 0x7D
#define ExtPropertyNotify 0x7C
#define ExtControlNotify 0x7B
#define ExtTimerNotify 0x7A

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    char id;
    int format;
    void *pv;
    int c;
} extpropertyevent;

extern Screen *ExtScreenOfRoot( Display *pdsp, Window wnd );
extern XStandardColormap *ExtGetColourmap( Display *pdsp, Window wndRoot,
					   VisualID vid );

extern int ExtSendEvent( Display *pdsp, Window w, XEvent *pxev );
extern int ExtSendEventHandler( extwindow *pewnd, XEvent *pxev );

extern int ExtChangeProperty( Display *pdsp, Window wnd, char id, int format,
			      void *pv, int c );
extern int ExtChangePropertyHandler( extwindow *pewnd, char id,
				     int format, void *pv, int c );

extern eventhandler ExtTimerHandler;

extern extquark eq_background, eq_Background, eq_borderColor, eq_BorderColor,
    eq_borderWidth, eq_BorderWidth, eq_font, eq_Font,
    eq_foreground, eq_Foreground, eq_geometry, eq_Geometry;

#endif
