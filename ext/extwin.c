/*
 * extwin.c
 *
 * by Gary Wong, 1997
 *
 */

#include "config.h"

#define XK_MISCELLANY
#define XK_LATIN1

#include <stdlib.h>
#include <string.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmu/Drawing.h>

#include <ext.h>
#include <extwin.h>

#define AllButtonMask ( Button1Mask | Button2Mask | Button3Mask | Button4Mask \
			| Button5Mask )
#define BUTTONTOMASK(b) ( 1 << ( (b) + 7 ) )

#define MAKE_QUARK( q ) static extquark eq_##q = { #q, 0 };

MAKE_QUARK( body );
MAKE_QUARK( cursor );
MAKE_QUARK( cursorFont );
MAKE_QUARK( dark );
MAKE_QUARK( dark2 );
MAKE_QUARK( font );
MAKE_QUARK( Font );
MAKE_QUARK( highlight );
MAKE_QUARK( justification );
MAKE_QUARK( Justification );
MAKE_QUARK( light );
MAKE_QUARK( light2 );
MAKE_QUARK( outline );
MAKE_QUARK( selectionBackground );
MAKE_QUARK( selectionForeground );
MAKE_QUARK( tick );

#undef MAKE_QUARK

/* FIXME FIXME FIXME!!!  add ExtDestroyNotify handlers to free() all window
   data */

typedef enum _justificationtype {
    left, right, centre
} justificationtype;

typedef struct _exttextdata {
    GC gc;
    XFontStruct *pxfs;
    char *pch;
    extcolour *pecol;
    justificationtype j;
} exttextdata;

static int ExtTextRedraw( extwindow *pewnd, exttextdata *petd, int fClear ) {

    int x;
    
    if( fClear )
	XClearWindow( pewnd->pdsp, pewnd->wnd );

    if( !petd->pch )
	return 0;
    
    switch( petd->j ) {
    case right:
	x = pewnd->cx - XTextWidth( petd->pxfs, petd->pch,
				    strlen( petd->pch ) );
	break;
	
    case centre:
	x = ( pewnd->cx - XTextWidth( petd->pxfs, petd->pch,
				    strlen( petd->pch ) ) ) / 2;
	break;
	
    default:
	x = 0;
    }
    
    XDrawString( pewnd->pdsp, pewnd->wnd, petd->gc, x, pewnd->cy -
		 petd->pxfs->descent, petd->pch, strlen( petd->pch ) );

    return 0;
}

static int ExtTextPropertyNotify( extwindow *pewnd, exttextdata *petd,
				  extpropertyevent *pepev ) {
    if( pepev->id != TP_TEXT )
	return -1;

    if( pepev->pv && petd->pch && !strcmp( pepev->pv, petd->pch ) )
	return 0;
    
    XFree( petd->pch );

    petd->pch = pepev->pv ? strdup( pepev->pv ) : NULL;

    ExtTextRedraw( pewnd, petd, True );

    return 0;
}

static int ExtTextPreCreate( extwindow *pewnd ) {

    exttextdata *petd = malloc( sizeof( *petd ) );

    petd->pch = pewnd->pv ? strdup( pewnd->pv ) : NULL;
    pewnd->pv = petd;
    
    petd->gc = None;
    
    return 0;
}

static int ExtTextCreate( extwindow *pewnd, exttextdata *petd ) {

    XGCValues xv;
    XColor xcol;
    char *pch;
    XrmValue xrmv;
    
    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_foreground, &eq_Foreground, &xcol,
			&petd->pecol );
    xv.foreground = xcol.pixel;
    xv.font = ( petd->pxfs = ExtWndAttachFont( pewnd, &eq_font, &eq_Font,
					       "fixed" ) ) ?
	petd->pxfs->fid : None;

    petd->gc = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );

    ExtResLookup( &pewnd->eres, &eq_justification, &eq_Justification,
		  NULL, &xrmv );
    
    pch = xrmv.addr;
    if( pch && *pch == 'r' )
	petd->j = right;
    else if( pch && *pch == 'c' )
	petd->j = centre;
    else
	petd->j = left;
    
    return 0;
}

static int ExtTextDestroyNotify( extwindow *pewnd, exttextdata *petd ) {

    ExtWndDetachColour( pewnd, petd->pecol );
    ExtWndDetachFont( pewnd, petd->pxfs->fid );
    ExtWndDetachGC( pewnd, petd->gc );

    XFree( petd->pch );
    
    return 0;
}

static int ExtTextHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    ExtTextRedraw( pewnd, pewnd->pv, False );

	break;
/*
  FIXME this is probably unnecessary?  (handled by gravity/expose)
    case ConfigureNotify:
	if( ( pewnd->cx != pxev->xconfigure.width ) ||
	    ( pewnd->cy != pxev->xconfigure.height ) ) {
	    pewnd->cx = pxev->xconfigure.width;
	    pewnd->cy = pxev->xconfigure.height;

	    ExtTextRedraw( pewnd, pewnd->pv, True );
	}
	
	break;
	*/
    case DestroyNotify:
	ExtTextDestroyNotify( pewnd, pewnd->pv );
	break;
	
    case ExtPropertyNotify:
	ExtTextPropertyNotify( pewnd, pewnd->pv, (extpropertyevent *) pxev );
	break;
	
    case ExtPreCreateNotify:
	ExtTextPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	ExtTextCreate( pewnd, pewnd->pv );
	break;
    }

    return ExtHandler( pewnd, pxev );
}

static extdefault aedText[] = {
    { &eq_font, "-B&H-Lucida-Medium-R-Normal-Sans-12-*-*-*-*-*-*" },
    { NULL, NULL }
};

extwindowclass ewcText = {
    ExposureMask | StructureNotifyMask,
    1, 1, 0, 0,
    ExtTextHandler,
    "Text",
    aedText
};

typedef enum _buttontype {
    btPush, btCheck, btRadio
} buttontype;

typedef struct _extbuttondata {
    GC gcBody, gcOutline, gcDark, gcLight, gcLabel, gcMark;
    Font font;
    extcolour *pecolBody, *pecolOutline, *pecolDark, *pecolLight,
	*pecolLabel, *pecolMark;
    XFontStruct *pxfs;
    int cxText;
    char *pch;
    buttontype bt;
    Bool fActive, fState;
    Pixmap pm; /* FIXME share this! */
} extbuttondata;

static int ExtButtonHandler( extwindow *pewnd, XEvent *pxev );

extwindowclass ewcPushButton = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask,
    1, 1, 0, 0,
    ExtButtonHandler,
    "PushButton"
};

extwindowclass ewcCheckButton = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask,
    1, 1, 0, 0,
    ExtButtonHandler,
    "CheckButton"
};

extwindowclass ewcRadioButton = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask,
    1, 1, 0, 0,
    ExtButtonHandler,
    "RadioButton"
};

static int ExtButtonRedraw( extwindow *pewnd, extbuttondata *pebd,
			    int fClear ) {
    int yButton;
    
    if( !pewnd->pdsp )
	return 0;
    
    if( fClear )
	XClearWindow( pewnd->pdsp, pewnd->wnd );

    if( pebd->cxText < 0 )
	pebd->cxText = pebd->pxfs ? XTextWidth( pebd->pxfs, pebd->pch,
						strlen( pebd->pch ) ) : -1;
    
    switch( pebd->bt ) {
    case btPush:
	XmuFillRoundedRectangle( pewnd->pdsp, pewnd->wnd, pebd->gcBody, 1, 1,
				 pewnd->cx - 3, pewnd->cy - 3, 3, 3 );

	XDrawString( pewnd->pdsp, pewnd->wnd, pebd->gcLabel,
		     ( ( pewnd->cx - pebd->cxText ) >> 1 ) +
		     ( pebd->fActive ? 1 : 0 ),
		     ( ( pewnd->cy + pebd->pxfs->ascent ) >> 1 ) +
		     ( pebd->fActive ? 1 : 0 ), pebd->pch,
		     strlen( pebd->pch ) );

	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcDark :
		   pebd->gcLight, 2, 1, pewnd->cx - 3, 1 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcDark :
		   pebd->gcLight, 1, 2, 1, pewnd->cy - 3 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcLight :
		   pebd->gcDark, 2, pewnd->cy - 2, pewnd->cx - 3,
		   pewnd->cy - 2 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcLight :
		   pebd->gcDark, pewnd->cx - 2, 2, pewnd->cx - 2,
		   pewnd->cy - 3 );
    
	XmuDrawRoundedRectangle( pewnd->pdsp, pewnd->wnd, pebd->gcOutline,
				 0, 0, pewnd->cx - 1, pewnd->cy - 1, 3, 3 );
	break;

    case btCheck:
	yButton = ( pewnd->cy - 16 ) >> 1;
	
	XFillRectangle( pewnd->pdsp, pewnd->wnd, pebd->gcBody, 1, yButton + 1,
			14, 14 );

	if( pebd->fState )
	    XCopyPlane( pewnd->pdsp, pebd->pm, pewnd->wnd, pebd->gcMark, 0, 0,
			16, 16, pebd->fActive ? 1 : 0, pebd->fActive ? 1 : 0,
			1 );
	
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcDark :
		   pebd->gcLight, 1, yButton + 1, 13, yButton + 1 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcDark :
		   pebd->gcLight, 1, yButton + 2, 1, yButton + 13 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcLight :
		   pebd->gcDark, 2, yButton + 14, 14, yButton + 14 );
	XDrawLine( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcLight :
		   pebd->gcDark, 14, yButton + 2, 14, yButton + 13 );

	XDrawRectangle( pewnd->pdsp, pewnd->wnd, pebd->gcOutline, 0, yButton,
			15, 15 );

	XDrawString( pewnd->pdsp, pewnd->wnd, pebd->gcLabel, 19,
		     ( pewnd->cy + pebd->pxfs->ascent ) >> 1,
		     pebd->pch, strlen( pebd->pch ) );

	break;
	
    case btRadio:
	yButton = ( pewnd->cy - 16 ) >> 1;
	
	XFillArc( pewnd->pdsp, pewnd->wnd, pebd->gcBody, 0, yButton + 0,
		  15, 15, 0, 360 * 64 );
	     
	if( pebd->fState )
	    XFillArc( pewnd->pdsp, pewnd->wnd, pebd->gcMark,
		      pebd->fActive ? 5 : 4, yButton +
		      ( pebd->fActive ? 5 : 4 ), 7, 7, 0, 360 * 64 );

	XDrawArc( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcDark :
		  pebd->gcLight, 1, yButton + 1, 13, 13, 55 * 64, 160 * 64 );
	XDrawArc( pewnd->pdsp, pewnd->wnd, pebd->fActive ? pebd->gcLight :
		  pebd->gcDark, 1, yButton + 1, 13, 13, 235 * 64, 160 * 64 );

	XDrawArc( pewnd->pdsp, pewnd->wnd, pebd->gcOutline, 0, yButton, 15, 15,
		  0, 360 * 64 );

	XDrawString( pewnd->pdsp, pewnd->wnd, pebd->gcLabel, 19,
		     ( pewnd->cy + pebd->pxfs->ascent ) >> 1,
		     pebd->pch, strlen( pebd->pch ) );
	
	break;
    }
    
    return 0;
}

static int ExtButtonPreCreate( extwindow *pewnd ) {

    extbuttondata *pebd = malloc( sizeof( *pebd ) );

    pewnd->pv = pebd;
    
    pebd->gcBody = pebd->gcOutline = pebd->gcDark = pebd->gcLight =
	pebd->gcLabel = pebd->gcMark = None;
    pebd->pxfs = NULL;
    pebd->pch = "";
    pebd->cxText = -1;
    pebd->fActive = pebd->fState = 0;

    if( pewnd->pewc == &ewcPushButton )
	pebd->bt = btPush;
    else if( pewnd->pewc == &ewcCheckButton )
	pebd->bt = btCheck;
    else if( pewnd->pewc == &ewcRadioButton )
	pebd->bt = btRadio;
    
    return 0;
}

static int ExtButtonCreate( extwindow *pewnd, extbuttondata *pebd ) {

#include "tick.xbm"
    
    XGCValues xv;
    XColor xcol, xcolNew;
    unsigned long pixBody;

    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_foreground, &eq_Foreground, &xcol,
			&pebd->pecolLabel );
    xv.foreground = xcol.pixel;
    xv.font = pebd->font =
	( pebd->pxfs = ExtWndAttachFont( pewnd, &eq_font, &eq_Font,
					 "fixed" ) ) ?
	pebd->pxfs->fid : None;
    
    pebd->gcLabel = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );

    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_outline, &eq_Foreground, &xcol,
			&pebd->pecolOutline);
    xv.foreground = xcol.pixel;
    pebd->gcOutline = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol.red = xcol.green = xcol.blue = 0xFFFF;
    ExtWndAttachColour( pewnd, &eq_body, &eq_Background, &xcol,
			&pebd->pecolBody );
    pixBody = xv.foreground = xcol.pixel;
    pebd->gcBody = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = ( xcol.red + 0xFFFF ) >> 1;
    xcolNew.green = ( xcol.green + 0xFFFF ) >> 1;
    xcolNew.blue = ( xcol.blue + 0xFFFF ) >> 1;
    ExtWndAttachColour( pewnd, &eq_light, &eq_Background, &xcolNew,
			&pebd->pecolLight );
    xv.foreground = xcolNew.pixel;
    pebd->gcLight = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = xcol.red >> 1;
    xcolNew.green = xcol.green >> 1;
    xcolNew.blue = xcol.blue >> 1;
    ExtWndAttachColour( pewnd, &eq_dark, &eq_Background, &xcolNew,
			&pebd->pecolDark );
    xv.foreground = xcolNew.pixel;
    pebd->gcDark = ExtWndAttachGC( pewnd, GCForeground, &xv );

    switch( pebd->bt ) {
    case btPush:
	break;

    case btCheck:
	xcol.red = xcol.green = xcol.blue = 0;
	ExtWndAttachColour( pewnd, &eq_tick, &eq_Foreground, &xcol,
			    &pebd->pecolMark );
	xv.foreground = xcol.pixel;
	xv.background = pixBody;
	pebd->gcMark = ExtWndAttachGC( pewnd, GCForeground | GCBackground, &xv );
	
	/* FIXME share this! */
	pebd->pm = XCreateBitmapFromData( pewnd->pdsp, pewnd->wnd, tick_bits,
					  tick_width, tick_height );
	break;

    case btRadio:
	xcol.red = xcol.green = xcol.blue = 0;
	ExtWndAttachColour( pewnd, &eq_highlight, &eq_Foreground, &xcol,
			    &pebd->pecolMark );
	xv.foreground = xcol.pixel;
	xv.background = pixBody;
	pebd->gcMark = ExtWndAttachGC( pewnd, GCForeground | GCBackground, &xv );
	break;
    }
    
    return 0;
}

static int ExtButtonDestroyNotify( extwindow *pewnd, extbuttondata *pebd ) {

    ExtWndDetachFont( pewnd, pebd->font );
    
    ExtWndDetachColour( pewnd, pebd->pecolLabel );
    ExtWndDetachColour( pewnd, pebd->pecolOutline );
    ExtWndDetachColour( pewnd, pebd->pecolBody );
    ExtWndDetachColour( pewnd, pebd->pecolLight );
    ExtWndDetachColour( pewnd, pebd->pecolDark );

    ExtWndDetachGC( pewnd, pebd->gcLabel );
    ExtWndDetachGC( pewnd, pebd->gcOutline );
    ExtWndDetachGC( pewnd, pebd->gcBody );
    ExtWndDetachGC( pewnd, pebd->gcLight );
    ExtWndDetachGC( pewnd, pebd->gcDark );
    
    if( pebd->bt != btPush ) {
	ExtWndDetachColour( pewnd, pebd->pecolMark );
	ExtWndDetachGC( pewnd, pebd->gcMark );
    }

    return 0;
}

static int ExtButtonPointer( extwindow *pewnd, extbuttondata *pebd,
			     XEvent *pxev ) {
    XEvent xevNotify;
    
    switch( pxev->type ) {
    case ButtonPress:
	if( pxev->xbutton.state & AllButtonMask )
	    return 0;

	XChangeActivePointerGrab( pewnd->pdsp, ButtonPressMask |
				  ButtonReleaseMask | EnterWindowMask |
				  LeaveWindowMask, None, pxev->xbutton.time );
	/* fall through */
	
    case EnterNotify:
	pebd->fActive = True;
	break;

    case LeaveNotify:
	pebd->fActive = False;
	break;
	
    case ButtonRelease:
	if( ( !pebd->fActive ) || ( ( pxev->xbutton.state & AllButtonMask ) &
	       ~BUTTONTOMASK( pxev->xbutton.button ) ) )
	    return 0;

	pebd->fActive = False;

	switch( pebd->bt ) {
	case btPush:
	    break;

	case btCheck:
	    pebd->fState = !pebd->fState;
	    break;

	case btRadio:
	    pebd->fState = True;
	    /* FIXME disable other radio buttons in group */
	    break;
	}

	xevNotify.type = ExtControlNotify;
	xevNotify.xclient.format = 16;
	xevNotify.xclient.data.s[ 0 ] = pewnd->id;
	xevNotify.xclient.data.s[ 1 ] = pewnd->wnd;
	xevNotify.xclient.data.s[ 2 ] = BN_CLICK;
	xevNotify.xclient.data.s[ 3 ] = pebd->fState;
	ExtSendEvent( pewnd->pdsp, pewnd->wndOwner, &xevNotify );

	break;
	
    default:
	return 0;
    }
    
    return ExtButtonRedraw( pewnd, pebd, True );
}

static int ExtButtonPropertyNotify( extwindow *pewnd, extbuttondata *pebd,
				    extpropertyevent *epev ) {
    if( epev->id != BP_LABEL )
	return 0;

    pebd->pch = epev->pv; /* FIXME alloc mem for it! */
    pebd->cxText = pebd->pxfs ? XTextWidth( pebd->pxfs, pebd->pch,
					    strlen( pebd->pch ) ) : -1;
    
    ExtButtonRedraw( pewnd, pebd, True );
    
    return 0;
}

static int ExtButtonHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    ExtButtonRedraw( pewnd, pewnd->pv, False );

	break;

    case ConfigureNotify:
	if( ( pewnd->cx != pxev->xconfigure.width ) ||
	    ( pewnd->cy != pxev->xconfigure.height ) ) {
	    pewnd->cx = pxev->xconfigure.width;
	    pewnd->cy = pxev->xconfigure.height;

	    ExtButtonRedraw( pewnd, pewnd->pv, True );
	}
	
	break;

    case ButtonPress:
    case ButtonRelease:
    case EnterNotify:
    case LeaveNotify:
	ExtButtonPointer( pewnd, pewnd->pv, pxev );
	break;

    case DestroyNotify:
	ExtButtonDestroyNotify( pewnd, pewnd->pv );
	break;
	
    case ExtPropertyNotify:
	ExtButtonPropertyNotify( pewnd, pewnd->pv, (extpropertyevent *) pxev );
	break;
	
    case ExtPreCreateNotify:
	ExtButtonPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	ExtButtonCreate( pewnd, pewnd->pv );
	break;
    }

    return ExtHandler( pewnd, pxev );
}

typedef struct _extscrollbardata {
    GC gcBody, gcOutline, gcDark, gcLight, gcDark2, gcLight2;
    extcolour *pecolBody, *pecolOutline, *pecolDark, *pecolLight, *pecolDark2,
	*pecolLight2;
    int nPos, nSize, nMax;
    Bool fHoriz, fActive;
} extscrollbardata;

static int ExtScrollBarRedraw( extwindow *pewnd, extscrollbardata *pesbd,
			    int fClear ) {
    int x, y, cx, cy;
    
    if( !pewnd->pdsp )
	return 0;
    
    if( fClear )
	XClearWindow( pewnd->pdsp, pewnd->wnd );

    if( pesbd->fHoriz ) {
	/* FIXME */
    } else {
	x = 0;
	cx = pewnd->cx;
	cy = pesbd->nMax ? pesbd->nSize * pewnd->cy / pesbd->nMax : pewnd->cy;
	if( cy < 15 )
	    cy = 15;
	if( cy > pewnd->cy )
	    cy = pewnd->cy;
	y = ( pesbd->nMax - pesbd->nSize ) ? ( pewnd->cy - cy ) * pesbd->nPos /
	    ( pesbd->nMax - pesbd->nSize ) : 0;
    }
    
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->gcDark2, 0, 0, pewnd->cx - 2,
	       0 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->gcDark2, 0, 1, 0, pewnd->cy -
	       2 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->gcLight2, 1, pewnd->cy - 1,
	       pewnd->cx - 2, pewnd->cy - 1 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->gcLight2, pewnd->cx - 1, 1,
	       pewnd->cx - 1, pewnd->cy - 1 );

    XFillRectangle( pewnd->pdsp, pewnd->wnd, pesbd->gcBody, x + 1, y + 1,
		    cx - 2, cy - 2 );
    
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->fActive ? pesbd->gcDark :
	       pesbd->gcLight, x + 1, y + 1, x + cx - 2, y + 1 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->fActive ? pesbd->gcDark :
	       pesbd->gcLight, x + 1, y + 1, x + 1, y + cy - 2 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->fActive ? pesbd->gcLight :
	       pesbd->gcDark, x + 2, y + cy - 1, x + cx - 1, y + cy - 1 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pesbd->fActive ? pesbd->gcLight :
	       pesbd->gcDark, x + cx - 1, y + 2, x + cx - 1, y + cy - 1 );
    
    XDrawRectangle( pewnd->pdsp, pewnd->wnd, pesbd->gcOutline,
		    x, y, cx, cy );

    return 0;
}

static int ExtScrollBarPreCreate( extwindow *pewnd ) {

    extscrollbardata *pesbd = malloc( sizeof( *pesbd ) );

    pewnd->pv = pesbd;
    
    pesbd->gcBody = pesbd->gcOutline = pesbd->gcDark = pesbd->gcLight =
	pesbd->gcDark2 = pesbd->gcLight2 = None;
    pesbd->nPos = pesbd->nSize = 0;
    pesbd->nMax = 0x100;
    pesbd->fHoriz = pesbd->fActive = False;
    
    return 0;
}

static int ExtScrollBarCreate( extwindow *pewnd, extscrollbardata *pesbd ) {

    XGCValues xv;
    XColor xcol, xcolNew;
    unsigned long pixBody;

    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_outline, &eq_Foreground, &xcol,
			&pesbd->pecolOutline );
    xv.foreground = xcol.pixel;
    pesbd->gcOutline = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol.red = xcol.green = xcol.blue = 0xFFFF;
    ExtWndAttachColour( pewnd, &eq_body, &eq_Foreground, &xcol,
			&pesbd->pecolBody );
    pixBody = xv.foreground = xcol.pixel;
    pesbd->gcBody = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = ( xcol.red + 0xFFFF ) >> 1;
    xcolNew.green = ( xcol.green + 0xFFFF ) >> 1;
    xcolNew.blue = ( xcol.blue + 0xFFFF ) >> 1;
    ExtWndAttachColour( pewnd, &eq_light, &eq_Background, &xcolNew,
			&pesbd->pecolLight );
    xv.foreground = xcolNew.pixel;
    pesbd->gcLight = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = xcol.red >> 1;
    xcolNew.green = xcol.green >> 1;
    xcolNew.blue = xcol.blue >> 1;
    ExtWndAttachColour( pewnd, &eq_dark, &eq_Background, &xcolNew,
			&pesbd->pecolDark );
    xv.foreground = xcolNew.pixel;
    pesbd->gcDark = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol = pewnd->pecolBackground->xcol;

    xcolNew.red = ( xcol.red + 0xC000 ) >> 1;
    xcolNew.green = ( xcol.green + 0xC000 ) >> 1;
    xcolNew.blue = ( xcol.blue + 0xC000 ) >> 1;
    ExtWndAttachColour( pewnd, &eq_light2, &eq_Background, &xcolNew,
			&pesbd->pecolLight2 );
    xv.foreground = xcolNew.pixel;
    pesbd->gcLight2 = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = xcol.red >> 1;
    xcolNew.green = xcol.green >> 1;
    xcolNew.blue = xcol.blue >> 1;
    ExtWndAttachColour( pewnd, &eq_dark2, &eq_Background, &xcolNew,
			&pesbd->pecolDark2 );
    xv.foreground = xcolNew.pixel;
    pesbd->gcDark2 = ExtWndAttachGC( pewnd, GCForeground, &xv );
    
    return 0;
}

static int ExtScrollBarDestroyNotify( extwindow *pewnd,
				      extscrollbardata *pesbd ) {

    ExtWndDetachColour( pewnd, pesbd->pecolBody );
    ExtWndDetachColour( pewnd, pesbd->pecolOutline );
    ExtWndDetachColour( pewnd, pesbd->pecolDark );
    ExtWndDetachColour( pewnd, pesbd->pecolLight );
    ExtWndDetachColour( pewnd, pesbd->pecolDark2 );
    ExtWndDetachColour( pewnd, pesbd->pecolLight2 );

    ExtWndDetachGC( pewnd, pesbd->gcBody );
    ExtWndDetachGC( pewnd, pesbd->gcOutline );
    ExtWndDetachGC( pewnd, pesbd->gcDark );
    ExtWndDetachGC( pewnd, pesbd->gcLight );
    ExtWndDetachGC( pewnd, pesbd->gcDark2 );
    ExtWndDetachGC( pewnd, pesbd->gcLight2 );

    return 0;
}

static int ExtScrollBarPointer( extwindow *pewnd, extscrollbardata *pesbd,
				XEvent *pxev ) {
    XEvent xevNotify;
    int x, y;
    int nPosOld = pesbd->nPos;
    Bool fActiveOld = pesbd->fActive;
    
    switch( pxev->type ) {
    case ButtonPress:
	switch( pxev->xbutton.button ) {
	case Button1:
	case Button3:
	    if( pesbd->fActive )
		return 0;
	    
	    xevNotify.xclient.data.s[ 2 ] = SBN_POSITION;
	    if( pesbd->fHoriz )
		pesbd->nPos += ( pxev->xbutton.x * pesbd->nSize /
				 (int) pewnd->cx ) *
		    ( ( pxev->xbutton.button == Button1 ) ? 1 : -1 );
	    else
		pesbd->nPos += ( pxev->xbutton.y * pesbd->nSize /
				 (int) pewnd->cy ) *
		    ( ( pxev->xbutton.button == Button1 ) ? 1 : -1 );
	    break;
	    
	case Button2:
	    pesbd->fActive = True;
	    xevNotify.xclient.data.s[ 2 ] = SBN_TRACK;
	    x = pxev->xbutton.x;
	    y = pxev->xbutton.y;
	    break;
	    
	default:
	    return 0;
	}
	break;

    case MotionNotify:
	if( !( pxev->xbutton.state & Button2Mask ) || ( !pesbd->fActive ) )
	    return 0;
	
	xevNotify.xclient.data.s[ 2 ] = SBN_TRACK;
	x = pxev->xmotion.x;
	y = pxev->xmotion.y;
	break;
	
    case ButtonRelease:
	if( ( pxev->xbutton.button != Button2 ) || ( !pesbd->fActive ) )
	    return 0;

	pesbd->fActive = False;
	xevNotify.xclient.data.s[ 2 ] = SBN_POSITION;
	
	break;
	
    default:
	return 0;
    }

    if( xevNotify.xclient.data.s[ 2 ] == SBN_TRACK )
	if( pesbd->fHoriz )
	    pesbd->nPos = x * pesbd->nMax / (int) pewnd->cx;
	else
	    pesbd->nPos = y * pesbd->nMax / (int) pewnd->cy;
    
    if( pesbd->nPos > pesbd->nMax - pesbd->nSize )
	pesbd->nPos = pesbd->nMax - pesbd->nSize;

    if( pesbd->nPos < 0 )
	pesbd->nPos = 0;

    if( ( pesbd->fActive != fActiveOld ) || ( pesbd->nPos != nPosOld ) ) {
	xevNotify.type = ExtControlNotify;
	xevNotify.xclient.format = 16;
	xevNotify.xclient.data.s[ 0 ] = pewnd->id;
	xevNotify.xclient.data.s[ 1 ] = pewnd->wnd;
	/* xevNotify.xclient.data.s[ 2 ] set above */
	xevNotify.xclient.data.s[ 3 ] = pesbd->nPos;
    
	ExtSendEvent( pewnd->pdsp, pewnd->wndOwner, &xevNotify );
    
	ExtScrollBarRedraw( pewnd, pesbd, True );
    }

    return 0;
}

static int ExtScrollBarPropertyNotify( extwindow *pewnd,
				       extscrollbardata *pesbd,
				       extpropertyevent *epev ) {
    short *an = epev->pv;
    
    if( epev->id != SBP_CONFIG )
	return 0;

    if( epev->c >= 1 )
	pesbd->nPos = an[ 0 ];

    if( epev->c >= 2 )
	pesbd->nSize = an[ 1 ];

    if( epev->c >= 3 )
	pesbd->nMax = an[ 2 ];

    if( epev->c >= 4 )
	pesbd->fHoriz = an[ 3 ];

    if( pesbd->nPos < 0 )
	pesbd->nPos = 0;

    if( pesbd->nPos > pesbd->nMax - pesbd->nSize )
	pesbd->nPos = pesbd->nMax - pesbd->nSize;
    
    return ExtScrollBarRedraw( pewnd, pesbd, True );
}

static int ExtScrollBarHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    ExtScrollBarRedraw( pewnd, pewnd->pv, False );

	break;

    case ConfigureNotify:
	if( ( pewnd->cx != pxev->xconfigure.width ) ||
	    ( pewnd->cy != pxev->xconfigure.height ) ) {
	    pewnd->cx = pxev->xconfigure.width;
	    pewnd->cy = pxev->xconfigure.height;

	    ExtScrollBarRedraw( pewnd, pewnd->pv, True );
	}
	
	break;

    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	ExtScrollBarPointer( pewnd, pewnd->pv, pxev );
	break;

    case DestroyNotify:
	ExtScrollBarDestroyNotify( pewnd, pewnd->pv );
	break;
	
    case ExtPropertyNotify:
	ExtScrollBarPropertyNotify( pewnd, pewnd->pv,
				    (extpropertyevent *) pxev );
	break;
	
    case ExtPreCreateNotify:
	ExtScrollBarPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	ExtScrollBarCreate( pewnd, pewnd->pv );
	break;
    }
    
    return ExtHandler( pewnd, pxev );
}

#define EXTLISTBOX_SCROLLBARWIDTH 16

static extdefault aedScrollBar[] = {
    { &eq_background, "LightSteelBlue4" },
    { &eq_body, "LightSteelBlue3" },
    { &eq_borderColor, "#000" },
    { &eq_borderWidth, "1" },
    { &eq_outline, "#000" },
    { NULL, NULL }
};

extwindowclass ewcScrollBar = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
    Button2MotionMask,
    1, 1, 0, 0,
    ExtScrollBarHandler,
    "ScrollBar",
    aedScrollBar
};

typedef struct _extlistboxdata {
    GC gcDark, gcLight, gcText, gcSelect, gcSelectText;
	/* NB body and outline are background and border */
    Font font;
    extcolour *pecolDark, *pecolLight, *pecolText, *pecolSelect,
	*pecolSelectText;
    char **apsz;
    int c, cp, i, fUp;
    XFontStruct *pxfs;
    extwindow ewndScroll;
    event evScroll;
    short anScroll[ 4 ];
    Time time;
} extlistboxdata;

#define LISTBOXROW( p ) ( ( p - 3 ) / ( pelbd->pxfs->ascent + \
					pelbd->pxfs->descent + 1 ) + \
			  pelbd->anScroll[ 0 ] )

static int ExtListBoxResizeScroll( extwindow *pewnd, extlistboxdata *pelbd ) {

    int n;

    if( !pelbd->pxfs )
	return 0;
    
    n = ( pewnd->cy - 6 ) / ( pelbd->pxfs->ascent + pelbd->pxfs->descent +
				  1 );
    
    pelbd->anScroll[ 1 ] = n;
    pelbd->anScroll[ 2 ] = pelbd->c;
    /* FIXME don't update if anScroll hasn't changed */
    return ExtChangePropertyHandler( &pelbd->ewndScroll, SBP_CONFIG, 16,
				     pelbd->anScroll, 3 );
}

static int ExtListBoxDrawLine( extwindow *pewnd, extlistboxdata *pelbd,
			       int i, Bool fClear ) {
    int iTop = pelbd->anScroll[ 0 ];
    int iBot = iTop + pelbd->anScroll[ 1 ] - 1;
    int y;
    
    if( !pewnd->pdsp )
	return 0;

    y = 3 + ( pelbd->pxfs->ascent + pelbd->pxfs->descent + 1 ) * ( i - iTop );
    
    if( ( i < iTop ) || ( i > iBot ) || ( i >= pelbd->c ) )
	return 0;

    if( i == pelbd->i )
	XFillRectangle( pewnd->pdsp, pewnd->wnd, pelbd->gcSelect, 1, y,
			pewnd->cx - 2 - EXTLISTBOX_SCROLLBARWIDTH,
			pelbd->pxfs->ascent + pelbd->pxfs->descent + 1 );
    else if( fClear )
	XClearArea( pewnd->pdsp, pewnd->wnd, 1, y,
		    pewnd->cx - 2 - EXTLISTBOX_SCROLLBARWIDTH,
		    pelbd->pxfs->ascent + pelbd->pxfs->descent + 1, False );
    
    XDrawString( pewnd->pdsp, pewnd->wnd, i == pelbd->i ? pelbd->gcSelectText
		 : pelbd->gcText, 3, y + pelbd->pxfs->ascent, pelbd->apsz[ i ],
		 strlen( pelbd->apsz[ i ] ) );

    return 0;
}

static int ExtListBoxDrawBorder( extwindow *pewnd, extlistboxdata *pelbd ) {

    XDrawLine( pewnd->pdsp, pewnd->wnd, pelbd->gcDark, 0, 0, pewnd->cx - 3
	       - EXTLISTBOX_SCROLLBARWIDTH, 0 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pelbd->gcDark, 0, 1, 0, pewnd->cy -
	       2 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pelbd->gcLight, 1, pewnd->cy - 1,
	       pewnd->cx - 3 - EXTLISTBOX_SCROLLBARWIDTH, pewnd->cy - 1 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, pelbd->gcLight, pewnd->cx -
	       EXTLISTBOX_SCROLLBARWIDTH - 2, 1,
	       pewnd->cx - EXTLISTBOX_SCROLLBARWIDTH - 2, pewnd->cy - 1 );

    return 0;
}
    
static int ExtListBoxRedraw( extwindow *pewnd, extlistboxdata *pelbd,
			     XExposeEvent *pxeev ) {
    int i, iFirst, iLast;
    
    iFirst = LISTBOXROW( pxeev->y );
    iLast = LISTBOXROW( pxeev->y + pxeev->height );

    for( i = iFirst; i <= iLast; i++ )
	ExtListBoxDrawLine( pewnd, pelbd, i, False );

    if( !pxeev->count )
	ExtListBoxDrawBorder( pewnd, pelbd );
    
    return 0;
}

static int ExtListBoxScroll( extwindow *pewnd, extlistboxdata *pelbd,
			     int n ) {

    /* FIXME scroll not redraw if poss. */

    int i;

    if( pelbd->anScroll[ 0 ] == n )
	return 0;
    
    XClearArea( pewnd->pdsp, pewnd->wnd, 1, 1, pewnd->cx -
		EXTLISTBOX_SCROLLBARWIDTH - 2, pewnd->cy, False );
    
    pelbd->anScroll[ 0 ] = n;
    
    for( i = pelbd->anScroll[ 0 ]; i < ( pelbd->anScroll[ 0 ] +
	     pelbd->anScroll[ 1 ] ); i++ )
	ExtListBoxDrawLine( pewnd, pelbd, i, False );

    ExtListBoxDrawBorder( pewnd, pelbd );

    return 0;
}

static int ExtListBoxTimer( extwindow *pewnd, extlistboxdata *pelbd ) {

    int n;
    
    if( pelbd->fUp ) {
	if( pelbd->anScroll[ 0 ] )
	    pelbd->i = n = pelbd->anScroll[ 0 ] - 1;
	else
	    return 0;
    } else
	if( pelbd->anScroll[ 0 ] + pelbd->anScroll[ 1 ] <
	    pelbd->anScroll[ 2 ] ) {
	    n = pelbd->anScroll[ 0 ] + 1;
	    pelbd->i = n + pelbd->anScroll[ 1 ] - 1;
	} else
	    return 0;
    
    ExtListBoxScroll( pewnd, pelbd, n );
    
    ExtChangePropertyHandler( &pelbd->ewndScroll, SBP_CONFIG, 16,
			      pelbd->anScroll, 1 );

    EventHandlerReady( &pelbd->evScroll, TRUE, 50 );
    
    return 0;
}

static int ExtListBoxSelect( extwindow *pewnd, extlistboxdata *pelbd,
			     int i ) {
    int iOld = pelbd->i;

    if( i == iOld )
	return 0;

    pelbd->i = i;
    
    ExtListBoxDrawLine( pewnd, pelbd, iOld, True );
    ExtListBoxDrawLine( pewnd, pelbd, pelbd->i, False );
    
    /* FIXME send notify */

    return 0;
}

static int ExtListBoxPointer( extwindow *pewnd, extlistboxdata *pelbd,
			      XEvent *pxev ) {
    switch( pxev->type ) {
    case ButtonPress:
	XSetSelectionOwner( pewnd->pdsp, XA_PRIMARY, pewnd->wnd,
			    pxev->xbutton.time );
    
	/* FIXME set time, check for double click */
	ExtListBoxSelect( pewnd, pelbd, LISTBOXROW( pxev->xbutton.y ) );

	return 0;

    case MotionNotify:
	if( ( pxev->xmotion.y >= 0 ) && ( pxev->xmotion.y < pewnd->cy ) ) {
	    ExtListBoxSelect( pewnd, pelbd, LISTBOXROW( pxev->xmotion.y ) );
	    EventHandlerReady( &pelbd->evScroll, FALSE, -1 );
	} else {
	    pelbd->fUp = ( pxev->xmotion.y < 0 );
	    if( !pelbd->evScroll.fHandlerReady ) {
		ExtListBoxSelect( pewnd, pelbd, pelbd->fUp ?
				  pelbd->anScroll[ 0 ] :
				  pelbd->anScroll[ 0 ] + pelbd->anScroll[ 1 ] -
				  1 );
		EventHandlerReady( &pelbd->evScroll, TRUE, 50 );
		/* FIXME anything else? */
	    }
	}

	return 0;
	
    case ButtonRelease:
	if( ( pxev->xbutton.state & AllButtonMask ) &
	    ~BUTTONTOMASK( pxev->xbutton.button ) )
	    return 0;

	if( pelbd->evScroll.fHandlerReady )
	    EventHandlerReady( &pelbd->evScroll, FALSE, -1 );
	
	return 0;
    }

    return 0;
}

static int ExtListBoxPreCreate( extwindow *pewnd ) {

    extlistboxdata *pelbd = malloc( sizeof( *pelbd ) );

    static char *apsz[] = { "Test Q^j", "Line 2 Q^j", "Three", "Four", "Five",
			    "Six", "Seven", "Eight", "Nine", "Ten", "Eleven",
			    "Twelve", "Thirteen", "Fourteen", "Fifteen",
			    "Sixteen", "Seventeen", "Eighteen", "Nineteen",
			    "Twenty", "Twenty one", "Twenty two",
			    "Twenty three", "Twenty four", "Twenty five",
			    "Twenty six", "Twenty seven", "Twenty eight",
			    "Twenty nine", "Thirty" };
    
    pewnd->pv = pelbd;
    
    pelbd->gcDark = pelbd->gcLight = pelbd->gcText = pelbd->gcSelect =
	pelbd->gcSelectText = None;
    pelbd->apsz = apsz;
    pelbd->pxfs = NULL;
    pelbd->c = pelbd->cp = 30; /* FIXME */
    pelbd->i = pelbd->time = 0;
    
    EventCreate( &pelbd->evScroll, &ExtTimerHandler, pewnd );
    
    ExtWndCreate( &pelbd->ewndScroll, &pewnd->eres, "scrollBar",
		  &ewcScrollBar, pewnd->eres.rdb, NULL, NULL );
    pelbd->anScroll[ 0 ] = 0;
    pelbd->anScroll[ 1 ] = 0;
    pelbd->anScroll[ 2 ] = 0;
    pelbd->anScroll[ 3 ] = 0;
    ExtChangePropertyHandler( &pelbd->ewndScroll, SBP_CONFIG, 16,
			      pelbd->anScroll, 4 );
    
    return 0;
}

static int ExtListBoxCreate( extwindow *pewnd, extlistboxdata *pelbd ) {

    XGCValues xv;
    XColor xcol, xcolNew;
    char szGeometry[ 32 ];

    xcol.red = pewnd->pecolBackground->xcol.red;
    xcol.green = pewnd->pecolBackground->xcol.green;
    xcol.blue = pewnd->pecolBackground->xcol.blue;

    xcolNew.red = ( xcol.red + 0xFFFF ) >> 1;
    xcolNew.green = ( xcol.green + 0xFFFF ) >> 1;
    xcolNew.blue = ( xcol.blue + 0xFFFF ) >> 1;
    ExtWndAttachColour( pewnd, &eq_light, &eq_Background, &xcolNew,
			&pelbd->pecolLight );
    xv.foreground = xcolNew.pixel;
    pelbd->gcLight = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = xcol.red >> 1;
    xcolNew.green = xcol.green >> 1;
    xcolNew.blue = xcol.blue >> 1;
    ExtWndAttachColour( pewnd, &eq_dark, &eq_Background, &xcolNew,
			&pelbd->pecolDark );
    xv.foreground = xcolNew.pixel;
    pelbd->gcDark = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_foreground, &eq_Foreground, &xcol,
			&pelbd->pecolText );
    xv.foreground = xcol.pixel;
    xv.font = pelbd->font =
	( pelbd->pxfs = ExtWndAttachFont( pewnd, &eq_font, &eq_Font,
					  "fixed" ) ) ?
	pelbd->pxfs->fid : None;
    pelbd->gcText = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );

    xcol.red = xcol.green = xcol.blue = 0xFFFF;
    ExtWndAttachColour( pewnd, &eq_selectionForeground, &eq_Foreground, &xcol,
			&pelbd->pecolSelectText );
    xv.foreground = xcol.pixel;
    pelbd->gcSelectText = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );
    
    xcol.red = xcol.green = xcol.blue = 0x0;
    ExtWndAttachColour( pewnd, &eq_selectionBackground, &eq_Background, &xcol,
			&pelbd->pecolSelect );
    xv.foreground = xcol.pixel;
    pelbd->gcSelect = ExtWndAttachGC( pewnd, GCForeground, &xv );
    
    sprintf( szGeometry, "16x%d--1+-1", pewnd->cy );
    ExtWndRealise( &pelbd->ewndScroll, pewnd->pdsp, pewnd->wnd, szGeometry,
		   pewnd->wnd, 0 );
    XMapWindow( pewnd->pdsp, pelbd->ewndScroll.wnd );
    
    ExtListBoxResizeScroll( pewnd, pelbd );
    
    return 0;
}

static int ExtListBoxDestroyNotify( extwindow *pewnd, extlistboxdata *pelbd ) {

    /* FIXME this should be done on ExtDestroyNotify */
    EventDestroy( &pelbd->evScroll );
    
    ExtWndDetachFont( pewnd, pelbd->font );
    
    ExtWndDetachColour( pewnd, pelbd->pecolLight );
    ExtWndDetachColour( pewnd, pelbd->pecolDark );
    ExtWndDetachColour( pewnd, pelbd->pecolText );
    ExtWndDetachColour( pewnd, pelbd->pecolSelect );
    ExtWndDetachColour( pewnd, pelbd->pecolSelectText );

    ExtWndDetachGC( pewnd, pelbd->gcLight );
    ExtWndDetachGC( pewnd, pelbd->gcDark );
    ExtWndDetachGC( pewnd, pelbd->gcText );
    ExtWndDetachGC( pewnd, pelbd->gcSelect );
    ExtWndDetachGC( pewnd, pelbd->gcSelectText );
    
    return 0;
}

static int ExtListBoxConfigure( extwindow *pewnd, extlistboxdata *pelbd,
				XConfigureEvent *pxcev ) {
    
    if( ( pewnd->cx != pxcev->width ) || ( pewnd->cy != pxcev->height ) ) {
	pewnd->cx = pxcev->width;
	pewnd->cy = pxcev->height;

	XMoveResizeWindow( pewnd->pdsp, pelbd->ewndScroll.wnd, pewnd->cx -
			   15, -1, 16, pewnd->cy );

	/* FIXME redraw properly! */
    }

    return 0;
}

static int ExtListBoxHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	ExtListBoxRedraw( pewnd, pewnd->pv, &pxev->xexpose );
	break;

    case ConfigureNotify:
	ExtListBoxConfigure( pewnd, pewnd->pv, &pxev->xconfigure );
	break;

    case DestroyNotify:
	ExtListBoxDestroyNotify( pewnd, pewnd->pv );
	break;

    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	ExtListBoxPointer( pewnd, pewnd->pv, pxev );
	break;

	/* FIXME handle selection events */
	
    case ExtControlNotify:
	/* FIXME check parameters */
	ExtListBoxScroll( pewnd, pewnd->pv, pxev->xclient.data.s[ 3 ] );
	break;

    case ExtTimerNotify:
	ExtListBoxTimer( pewnd, pewnd->pv );
	break;
	
    case ExtPreCreateNotify:
	ExtListBoxPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	ExtListBoxCreate( pewnd, pewnd->pv );
	break;
    }
    
    return ExtHandler( pewnd, pxev );
}

#undef LISTBOXROW

extwindowclass ewcListBox = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
    ButtonMotionMask,
    1, 1, 0, 0,
    ExtListBoxHandler,
    "ListBox"
};

#define FOCUS_POINTER 0x01
#define FOCUS_SET 0x02

typedef struct _extentryfielddata {
    GC gcDark, gcLight, gcText, gcSelect, gcSelectText, gcCursor;
	/* NB body and outline are background and border */
    Font font;
    extcolour *pecolDark, *pecolLight, *pecolText, *pecolSelect,
	*pecolSelectText, *pecolCursor;
    char *psz;
    int cch, cchAlloc, ich, cchSelect, fLeft, flFocus, xOffset;
    XFontStruct *pxfs, *pxfsCursor;
    event evScroll;
    Time time;
} extentryfielddata;

static int ExtEntryFieldIndex( extentryfielddata *peefd, int x ) {

    int i;

    /* FIXME better to use binary search */
    /* FIXME x + offset - 5 is a bit of a hack, should be - 3 + half width
       of char at posn */
    for( i = 0; ( i < peefd->cch ) && ( x + peefd->xOffset - 5 ) >
	     XTextWidth( peefd->pxfs, peefd->psz, i ); i++ )
	;

    return i;
}

static int ExtEntryFieldRedraw( extwindow *pewnd, extentryfielddata *peefd,
				Bool fClear ) {
    
    static char chCursor = XC_xterm;
    
    if( fClear )
	XClearArea( pewnd->pdsp, pewnd->wnd, 1, 1, pewnd->cx - 2,
		    pewnd->cy - 2, False );

    if( peefd->cchSelect ) {
	/* FIXME */
    } else
	XDrawString( pewnd->pdsp, pewnd->wnd, peefd->gcText,
		     3 - peefd->xOffset, pewnd->cy - 3 - peefd->pxfs->descent,
		     peefd->psz, peefd->cch );

    if( peefd->flFocus )
	XDrawString( pewnd->pdsp, pewnd->wnd, peefd->gcCursor,
		     XTextWidth( peefd->pxfs, peefd->psz, peefd->ich +
			 peefd->cchSelect ) + 3,
		     pewnd->cy - 3 - peefd->pxfs->descent -
		     ( peefd->pxfs->ascent >> 1 ), &chCursor, 1 );
    
    XDrawLine( pewnd->pdsp, pewnd->wnd, peefd->gcDark, 0, 0, pewnd->cx - 2,
	       0 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, peefd->gcDark, 0, 1, 0, pewnd->cy -
	       2 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, peefd->gcLight, 1, pewnd->cy - 1,
	       pewnd->cx - 2, pewnd->cy - 1 );
    XDrawLine( pewnd->pdsp, pewnd->wnd, peefd->gcLight, pewnd->cx - 1, 1,
	       pewnd->cx - 1, pewnd->cy - 1 );

    return 0;
}

static int ExtEntryFieldKeyPress( extwindow *pewnd, extentryfielddata *peefd,
				  XKeyEvent *pxkev ) {
    char ch;
    KeySym ks;

    if( pxkev->state & ( Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |
			 Mod5Mask ) ) {
	ExtSendEvent( pewnd->pdsp, pewnd->wndOwner, (XEvent *) pxkev );
	return 0;
    }
    
    ks = XLookupKeysym( pxkev, 0 );

    if( ( ks >> 8 ) == 0xFF )
	switch( ks ) {
	case XK_BackSpace:
	case XK_Delete:
#ifdef XK_KP_Delete
	case XK_KP_Delete:
#endif	    
	    if( peefd->cchSelect ) {
		strcpy( peefd->psz + peefd->ich, peefd->psz + peefd->ich +
		    peefd->cchSelect );
		peefd->cch -= peefd->cchSelect;
	    } else if( peefd->ich ) {
		strcpy( peefd->psz + peefd->ich - 1, peefd->psz + peefd->ich );
		peefd->ich--;
		peefd->cch--;
	    } else
		return 0;
	    peefd->cchSelect = 0;
	    break;

#ifdef XK_KP_Left
	case XK_KP_Left:
#endif	    
	case XK_Left:
	    if( peefd->ich )
		peefd->ich--;
	    else
		return 0;
	    break;

#ifdef XK_KP_Right
	case XK_KP_Right:
#endif	    
	case XK_Right:
	    if( peefd->ich < peefd->cch )
		peefd->ich++;
	    else
		return 0;
	    break;

#ifdef XK_KP_Home
	case XK_KP_Home:
	case XK_KP_Begin:
#endif	    
	case XK_Home:
	case XK_Begin:
	    if( !peefd->ich )
		return 0;

	    peefd->ich = 0;
	    break;

#ifdef XK_KP_End
	case XK_KP_End:
#endif	    
	case XK_End:
	    if( peefd->ich == peefd->cch )
		return 0;

	    peefd->ich = peefd->cch;
	    break;
	    
	default:
	    ExtSendEvent( pewnd->pdsp, pewnd->wndOwner, (XEvent *) pxkev );
	    return 0;
	}
    else
	/* FIXME ctrl characters? */
	/* FIXME keep compose status */
	if( XLookupString( pxkev, &ch, 1, NULL, NULL ) ) {
	    if( ( peefd->cch >= peefd->cchAlloc - 1 ) &&
		!peefd->cchSelect )
		/* FIXME beep or something */
		return 0;
	    /* FIXME this probably isn't optimal */
	    if( peefd->cchSelect != 1 )
		memmove( peefd->psz + peefd->ich + 1, peefd->psz + peefd->ich +
			peefd->cchSelect, strlen( peefd->psz + peefd->ich +
			    peefd->cchSelect ) + 1 );
	    peefd->psz[ peefd->ich++ ] = ch;
	    peefd->cch = strlen( peefd->psz );
	    peefd->cchSelect = 0;
	} else
	    return 0;

    ExtEntryFieldRedraw( pewnd, peefd, True );
    
    return 0;
}

static int ExtEntryFieldFocus( extwindow *pewnd, extentryfielddata *peefd,
			       XFocusChangeEvent *pxfcev ) {
    
    int flFocusOld = peefd->flFocus;

    if( pxfcev->type == FocusIn )
	peefd->flFocus |= ( pxfcev->detail == NotifyPointer ) ?
	    FOCUS_POINTER : FOCUS_SET;
    else
	peefd->flFocus &= ( pxfcev->detail == NotifyPointer ) ?
	    ~FOCUS_POINTER : ~FOCUS_SET;

    if( ( peefd->flFocus != 0 ) != ( flFocusOld != 0 ) )
	ExtEntryFieldRedraw( pewnd, peefd, True );

    return 0;
}

static int ExtEntryFieldPropertyNotify( extwindow *pewnd,
					extentryfielddata *peefd,
					extpropertyevent *epev ) {
    switch( epev->id ) {
    case EFP_SIZE:
	return 0;

    case EFP_TEXT:
	/* FIXME check buffer overflow.  strncpy isn't much use */
	strcpy( peefd->psz, epev->pv );
	peefd->cch = strlen( peefd->psz );
	peefd->ich = peefd->cchSelect = 0;
	ExtEntryFieldRedraw( pewnd, peefd, True );
	return 0;
    }

    return 0;
}

static int ExtEntryFieldPointer( extwindow *pewnd, extentryfielddata *peefd,
				 XEvent *pxev ) {
    switch( pxev->type ) {
    case ButtonPress:
	peefd->ich = ExtEntryFieldIndex( peefd, pxev->xbutton.x );
	ExtEntryFieldRedraw( pewnd, peefd, True );
	break;
    }

    return 0;
}

static int ExtEntryFieldEnterLeave( extwindow *pewnd, extentryfielddata *peefd,
				    XCrossingEvent *pxcev ) {
    
    int flFocusOld = peefd->flFocus;
    
    if( pxcev->type == LeaveNotify )
	peefd->flFocus &= ~FOCUS_POINTER;
    else if( pxcev->focus )
	peefd->flFocus |= FOCUS_POINTER;

    if( ( peefd->flFocus != 0 ) != ( flFocusOld != 0 ) )
	ExtEntryFieldRedraw( pewnd, peefd, True );

    return 0;
}

static int ExtEntryFieldPreCreate( extwindow *pewnd ) {

    extentryfielddata *peefd = malloc( sizeof( *peefd ) );

    pewnd->pv = peefd;
    
    peefd->gcDark = peefd->gcLight = peefd->gcText = peefd->gcSelect =
	peefd->gcSelectText = peefd->gcCursor = None;
    peefd->psz = malloc( 256 );
    peefd->psz[ 0 ] = 0;
    peefd->pxfs = peefd->pxfsCursor = NULL;
    peefd->cch = 0;
    peefd->cchAlloc = 256;
    peefd->ich = peefd->cchSelect = peefd->xOffset = peefd->time = 0;
    peefd->flFocus = 0;
    
    EventCreate( &peefd->evScroll, &ExtTimerHandler, pewnd );
    
    return 0;
}

static int ExtEntryFieldCreate( extwindow *pewnd, extentryfielddata *peefd ) {

    XGCValues xv;
    XColor xcol, xcolNew;

    xcol.red = pewnd->pecolBackground->xcol.red;
    xcol.green = pewnd->pecolBackground->xcol.green;
    xcol.blue = pewnd->pecolBackground->xcol.blue;

    xcolNew.red = ( xcol.red + 0xFFFF ) >> 1;
    xcolNew.green = ( xcol.green + 0xFFFF ) >> 1;
    xcolNew.blue = ( xcol.blue + 0xFFFF ) >> 1;
    ExtWndAttachColour( pewnd, &eq_light, &eq_Background, &xcolNew,
			&peefd->pecolLight );
    xv.foreground = xcolNew.pixel;
    peefd->gcLight = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcolNew.red = xcol.red >> 1;
    xcolNew.green = xcol.green >> 1;
    xcolNew.blue = xcol.blue >> 1;
    ExtWndAttachColour( pewnd, &eq_dark, &eq_Background, &xcolNew,
			&peefd->pecolDark );
    xv.foreground = xcolNew.pixel;
    peefd->gcDark = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol.red = xcol.green = xcol.blue = 0;
    ExtWndAttachColour( pewnd, &eq_foreground, &eq_Foreground, &xcol,
			&peefd->pecolText );
    xv.foreground = xcol.pixel;
    xv.font = peefd->font =
	( peefd->pxfs = ExtWndAttachFont( pewnd, &eq_font, &eq_Font,
					  "fixed" ) ) ?
	peefd->pxfs->fid : None;
    peefd->gcText = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );

    xcol.red = xcol.green = xcol.blue = 0xFFFF;
    ExtWndAttachColour( pewnd, &eq_selectionForeground, &eq_Foreground, &xcol,
			&peefd->pecolSelectText );
    xv.foreground = xcol.pixel;
    peefd->gcSelectText = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );
    
    xcol.red = xcol.green = xcol.blue = 0x0;
    ExtWndAttachColour( pewnd, &eq_selectionBackground, &eq_Background, &xcol,
			&peefd->pecolSelect );
    xv.foreground = xcol.pixel;
    peefd->gcSelect = ExtWndAttachGC( pewnd, GCForeground, &xv );

    xcol.red = xcol.green = xcol.blue = 0x0;
    ExtWndAttachColour( pewnd, &eq_cursor, &eq_Foreground, &xcol,
			&peefd->pecolCursor );
    xv.foreground = xcol.pixel;
    xv.font = ( peefd->pxfsCursor = ExtWndAttachFont( pewnd, &eq_cursorFont,
						      &eq_Font, "cursor" ) ) ?
	peefd->pxfsCursor->fid : None;
    peefd->gcCursor = ExtWndAttachGC( pewnd, GCForeground | GCFont, &xv );
    
    return 0;
}

static int ExtEntryFieldDestroyNotify( extwindow *pewnd,
				       extentryfielddata *peefd ) {

    /* FIXME this should be done on ExtDestroyNotify */
    EventDestroy( &peefd->evScroll );
    
    ExtWndDetachFont( pewnd, peefd->font );
    ExtWndDetachFont( pewnd, peefd->pxfsCursor->fid );
    
    ExtWndDetachColour( pewnd, peefd->pecolLight );
    ExtWndDetachColour( pewnd, peefd->pecolDark );
    ExtWndDetachColour( pewnd, peefd->pecolText );
    ExtWndDetachColour( pewnd, peefd->pecolSelect );
    ExtWndDetachColour( pewnd, peefd->pecolSelectText );
    ExtWndDetachColour( pewnd, peefd->pecolCursor );

    ExtWndDetachGC( pewnd, peefd->gcLight );
    ExtWndDetachGC( pewnd, peefd->gcDark );
    ExtWndDetachGC( pewnd, peefd->gcText );
    ExtWndDetachGC( pewnd, peefd->gcSelect );
    ExtWndDetachGC( pewnd, peefd->gcSelectText );
    ExtWndDetachGC( pewnd, peefd->gcCursor );
    
    return 0;
}

extern char *ExtEntryFieldString( extwindow *pewnd ) {

    extentryfielddata *peefd = pewnd->pv;
    
    return peefd->psz;
}

static int ExtEntryFieldHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case Expose:
	if( !pxev->xexpose.count )
	    ExtEntryFieldRedraw( pewnd, pewnd->pv, False );

	break;

    case DestroyNotify:
	ExtEntryFieldDestroyNotify( pewnd, pewnd->pv );
	break;

    case FocusIn:
    case FocusOut:
	ExtEntryFieldFocus( pewnd, pewnd->pv, (XFocusChangeEvent *) pxev );
	break;

    case EnterNotify:
    case LeaveNotify:
	ExtEntryFieldEnterLeave( pewnd, pewnd->pv, (XCrossingEvent *) pxev );
	break;

    case KeyPress:
	ExtEntryFieldKeyPress( pewnd, pewnd->pv, (XKeyEvent *) pxev );
	break;

    case ButtonPress:
    case ButtonRelease:
    case MotionNotify:
	ExtEntryFieldPointer( pewnd, pewnd->pv, pxev );
	break;
/*	
    case ExtTimerNotify:
	ExtEntryFieldTimer( pewnd, pewnd->pv );
	break;
	*/
	
    case ExtPropertyNotify:
	ExtEntryFieldPropertyNotify( pewnd, pewnd->pv,
				     (extpropertyevent *) pxev );
	break;
	
    case ExtPreCreateNotify:
	ExtEntryFieldPreCreate( pewnd );
	break;

    case ExtCreateNotify:
	ExtEntryFieldCreate( pewnd, pewnd->pv );
	break;
    }
    
    return ExtHandler( pewnd, pxev );
}

#undef FOCUS_POINTER
#undef FOCUS_SET

static extdefault aedEntryField[] = {
    { &eq_borderWidth, "1" },
    { &eq_borderColor, "#000" },
    { &eq_background, "Khaki" },
    { &eq_font, "-B&H-Lucida-Medium-R-Normal-Sans-12-*-*-*-*-*-*" },
    { &eq_selectionBackground, "LightSteelBlue3" },
    { &eq_selectionForeground, "darkblue" },
    { NULL, NULL }
};

extwindowclass ewcEntryField = {
    ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
    ButtonMotionMask | FocusChangeMask | KeyPressMask | EnterWindowMask |
    LeaveWindowMask,
    1, 1, 0, 0,
    ExtEntryFieldHandler,
    "EntryField",
    aedEntryField
};
