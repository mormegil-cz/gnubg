/*
 * ext.c
 *
 * by Gary Wong, 1997-2000
 *
 */

#include "config.h"

#include <assert.h>
#include <event.h>
#include <hash.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xmu/StdCmap.h>

#if EXT_DEBUG
#include <stdio.h>
#endif

#include <ext.h>

static XContext xc;

static hash hFont, hColourMap, hColourName, hColour, hGC, hdsp;

static int ( *fnErrorHandlerOld )() = NULL;

#define MAKE_QUARK( q ) extquark eq_##q = { #q, 0 };

MAKE_QUARK( background );
MAKE_QUARK( Background );
MAKE_QUARK( borderColor );
MAKE_QUARK( BorderColor );
MAKE_QUARK( borderWidth );
MAKE_QUARK( BorderWidth );
MAKE_QUARK( font );
MAKE_QUARK( Font );
MAKE_QUARK( foreground );
MAKE_QUARK( Foreground );
MAKE_QUARK( geometry );
MAKE_QUARK( Geometry );

#undef MAKE_QUARK

/* maps sz->pxfs, for fixed display.  Cache only; never freed. */
typedef struct _fonthashdata {
    char *sz;
    int idDsp;
    XFontStruct *pxfs;
    int cRef; /* font is unloaded when unreferenced, but pxfs is retained */
} fonthashdata;

/* maps vid->pxscm, for fixed screen.  Cache only; never freed. */

typedef struct _colourmaphashdata {
    int idDsp;
    Window wndRoot;
    VisualID vid;
    XStandardColormap xscm;
} colourmaphashdata;

/* maps sz->xcol, for fixed screen.  Cache only; never freed. */
typedef struct _colournamehashdata {
    char *sz;
    int idDsp;
    Window wndRoot;
    XColor xcol;
} colournamehashdata;

/* (extcolour) maps RGB->pix, for fixed colourmap */
typedef extcolour colourhashdata;

typedef struct _gchashdata {
    XGCValues xgcv;
    unsigned long fl;
    int idDsp;
    Window wndRoot;
    int nDepth;
    GC gc;
    int cRef;
} gchashdata;

long FontHash( fonthashdata *pfhd ) {

    return StringHash( pfhd->sz ) ^ pfhd->idDsp;
}

long ColourMapHash( colourmaphashdata *pcmhd ) {

    return ( pcmhd->idDsp << 16 ) ^ ( pcmhd->wndRoot << 8 ) ^ pcmhd->vid;
}

long ColourNameHash( colournamehashdata *pcnhd ) {

    return StringHash( pcnhd->sz ) ^ ( pcnhd->idDsp << 8 ) ^ pcnhd->wndRoot;
}

long ColourHash( extcolour *pecol ) {

    return ( pecol->xcol.red << 16 ) ^ ( pecol->xcol.green << 12 ) ^
	( pecol->xcol.blue << 8 ) ^ ( pecol->idDsp << 4 ) ^
	pecol->cmap;
}

long GCHash( gchashdata *pgchd ) {

    long l = ( pgchd->idDsp << 16 ) ^ ( pgchd->wndRoot << 8 ) ^ pgchd->nDepth;

#define UPDATE( f, m, d ) \
    l = ( ( l << 8 ) % 1073741789 ) ^ ( pgchd->fl & (f) ? pgchd->xgcv.m : (d) )

    UPDATE( GCFunction, function, GXcopy );
    UPDATE( GCPlaneMask, plane_mask, AllPlanes );
    UPDATE( GCForeground, foreground, 0 );
    UPDATE( GCBackground, background, 1 );
    UPDATE( GCLineWidth, line_width, 0 );
    UPDATE( GCLineStyle, line_style, LineSolid );
    UPDATE( GCCapStyle, cap_style, CapButt );
    UPDATE( GCJoinStyle, join_style, JoinMiter );
    UPDATE( GCFillStyle, fill_style, FillSolid );
    UPDATE( GCFillRule, fill_rule, EvenOddRule );
    UPDATE( GCTile, tile, ~0L );
    UPDATE( GCStipple, stipple, ~0L );
    UPDATE( GCTileStipXOrigin, ts_x_origin, 0 );
    UPDATE( GCTileStipYOrigin, ts_y_origin, 0 );
    UPDATE( GCFont, font, ~0L );
    UPDATE( GCSubwindowMode, subwindow_mode, ClipByChildren );
    UPDATE( GCGraphicsExposures, graphics_exposures, True );
    UPDATE( GCClipXOrigin, clip_x_origin, 0 );
    UPDATE( GCClipYOrigin, clip_y_origin, 0 );
    UPDATE( GCClipMask, clip_mask, None );
    UPDATE( GCDashOffset, dash_offset, 0 );
    UPDATE( GCDashList, dashes, 4 );
    UPDATE( GCArcMode, arc_mode, ArcPieSlice );

#undef UPDATE

    return l;
}

int FontCompare( fonthashdata *pfhd0, fonthashdata *pfhd1 ) {

    return ( pfhd0->idDsp != pfhd1->idDsp ) ||
	strcmp( pfhd0->sz, pfhd1->sz );
}

int ColourMapCompare( colourmaphashdata *pcmhd0,
		      colourmaphashdata *pcmhd1 ) {

    return ( pcmhd0->idDsp != pcmhd1->idDsp ) ||
	( pcmhd0->wndRoot != pcmhd1->wndRoot ) ||
	( pcmhd0->vid != pcmhd1->vid );
}

int ColourNameCompare( colournamehashdata *pcnhd0,
		       colournamehashdata *pcnhd1 ) {

    return ( pcnhd0->idDsp != pcnhd1->idDsp ) ||
	( pcnhd0->wndRoot != pcnhd1->wndRoot ) ||
	strcmp( pcnhd0->sz, pcnhd1->sz );
}

int ColourCompare( extcolour *pecol0, extcolour *pecol1 ) {

    return ( pecol0->cmap != pecol1->cmap ) ||
	( pecol0->idDsp != pecol1->idDsp ) ||
	( pecol0->xcol.red != pecol1->xcol.red ) ||
	( pecol0->xcol.green != pecol1->xcol.green ) ||
	( pecol0->xcol.blue != pecol1->xcol.blue );
}

int GCCompare( gchashdata *pgchd0, gchashdata *pgchd1 ) {

#define DIFF( f, m, d ) ( ( pgchd0->fl & (f) ? pgchd0->xgcv.m : (d) ) != \
    ( pgchd1->fl & (f) ? pgchd1->xgcv.m : (d) ) )

    return ( pgchd0->idDsp != pgchd1->idDsp ) ||
	( pgchd0->wndRoot != pgchd1->wndRoot ) ||
	( pgchd0->nDepth != pgchd1->nDepth ) ||
	DIFF( GCPlaneMask, plane_mask, AllPlanes ) ||
	DIFF( GCForeground, foreground, 0 ) ||
	DIFF( GCBackground, background, 1 ) ||
	DIFF( GCLineWidth, line_width, 0 ) ||
	DIFF( GCLineStyle, line_style, LineSolid ) ||
	DIFF( GCCapStyle, cap_style, CapButt ) ||
	DIFF( GCJoinStyle, join_style, JoinMiter ) ||
	DIFF( GCFillStyle, fill_style, FillSolid ) ||
	DIFF( GCFillRule, fill_rule, EvenOddRule ) ||
	DIFF( GCTile, tile, ~0L ) ||
	DIFF( GCStipple, stipple, ~0L ) ||
	DIFF( GCTileStipXOrigin, ts_x_origin, 0 ) ||
	DIFF( GCTileStipYOrigin, ts_y_origin, 0 ) ||
	DIFF( GCFont, font, ~0L ) ||
	DIFF( GCSubwindowMode, subwindow_mode, ClipByChildren ) ||
	DIFF( GCGraphicsExposures, graphics_exposures, True ) ||
	DIFF( GCClipXOrigin, clip_x_origin, 0 ) ||
	DIFF( GCClipYOrigin, clip_y_origin, 0 ) ||
	DIFF( GCClipMask, clip_mask, None ) ||
	DIFF( GCDashOffset, dash_offset, 0 ) ||
	DIFF( GCDashList, dashes, 4 ) ||
	DIFF( GCArcMode, arc_mode, ArcPieSlice );

#undef DIFF    
}

typedef struct _replyevent {
    event *pev;
    unsigned long n;
} replyevent;

typedef struct _requesterrorhandler {
    int ( *fn )( extdisplay *pedsp, XErrorEvent *pxeev );
    unsigned long n;
} requesterrorhandler;

static int ExtDspErrorHandler( Display *pdsp, XErrorEvent *pxeev ) {

    extdisplay *pedsp;
    list *pl;
    requesterrorhandler *preh;
    
    if( !( pedsp = ExtDspFind( pdsp ) ) )
	return -1;

    for( pl = pedsp->levRequestErrorHandler.plNext;
	 pl != &pedsp->levRequestErrorHandler; pl = pl->plNext ) {
	preh = pl->p;

	if( pxeev->serial == preh->n ) {
	    int ( *fn )( extdisplay *pedsp, XErrorEvent *pxeev ) = preh->fn;

	    ListDelete( pl );
	    free( preh );

	    return fn ? fn( pedsp, pxeev ) : 0;
	} else if( pxeev->serial < preh->n )
	    break;
    }
    
    /* Daisy chain to the old handler */
    return fnErrorHandlerOld( pdsp, pxeev );
}

static int ExtDspNotify( event *pev, extdisplay *pedsp ) {
    
    XEvent xev;
    extwindow *pewnd;
    list *pl;
    replyevent *prev;
    requesterrorhandler *preh;
    
#if EXT_DEBUG    
    puts( "reading" );    
#endif
    
    XEventsQueued( pedsp->pdsp, QueuedAfterReading );

    pedsp->nLast = LastKnownRequestProcessed( pedsp->pdsp );

    /* Search for saved reply events; handle and free any that have
       occurred. */
    while( pedsp->levReply.plNext != &pedsp->levReply ) {
	pl = pedsp->levReply.plNext;
	prev = pl->p;

	if( pedsp->nLast >= prev->n ) {
	    event *pev = prev->pev;
	    
	    ListDelete( pl );
	    free( prev );
	    
	    EventPending( pev, TRUE );
	    EventProcess( pev );
	} else
	    break;
    }

    /* Search for errors from marked requests; if a request has been
       processed then it must have completed without error, so delete
       the list entry. */
    /* FIXME is this really right?  The Xlib documentation doesn't
       seem to be precise about whether XEventsQueued will call any
       pending error handlers (XSync does, but it doesn't mention
       XEventsQueued).  We presume it does... */
    while( pedsp->levRequestErrorHandler.plNext !=
	   &pedsp->levRequestErrorHandler ) {
	pl = pedsp->levRequestErrorHandler.plNext;
	preh = pl->p;

	if( pedsp->nLast >= preh->n ) {
	    ListDelete( pl );
	    free( preh );
	} else
	    break;
    }
	
    while( XEventsQueued( pedsp->pdsp, QueuedAlready ) ) {
	XNextEvent( pedsp->pdsp, &xev );

	/* FIXME KeymapNotify? */
	if( xev.type == MappingNotify )
	    XRefreshKeyboardMapping( &xev.xmapping );
	else if( !XFindContext( pedsp->pdsp, xev.xany.window, xc,
				(XPointer *) &pewnd ) )
	    pewnd->pewc->pxevh( pewnd, &xev );
	/* FIXME client message translation? */
	/* FIXME change PropertyNotify of Ext types to ExtPropertyNotify */
    }
    
    EventPending( pev, FALSE );

    return 0;
}

static int ExtDspCommit( event *pev, extdisplay *pedsp ) {

#if EXT_DEBUG    
    puts( "committing" );    
#endif
    
    XFlush( pedsp->pdsp );

    if( ( LastKnownRequestProcessed( pedsp->pdsp ) > pedsp->nLast ) ||
	( XEventsQueued( pedsp->pdsp, QueuedAlready ) ) )
	EventPending( &pedsp->ev, TRUE );
    
    return 0;
}

static eventhandler ExtDspHandler = {
    (eventhandlerfunc) ExtDspNotify,
    (eventhandlerfunc) ExtDspCommit
};

extern int ExtDspCreate( extdisplay *pedsp, Display *pdsp ) {

    if( EventCreate( &pedsp->ev, &ExtDspHandler, pedsp ) < 0 )
	return -1;

#if EXT_DEBUG    
    printf( "ext event: %p\n", &pedsp->ev );    
#endif
    
    pedsp->ev.h = ConnectionNumber( pdsp );
    pedsp->ev.fWrite = FALSE;

    pedsp->pdsp = pdsp;
    
    ListCreate( &pedsp->levReply );
    ListCreate( &pedsp->levRequestErrorHandler );

    if( !fnErrorHandlerOld )
	fnErrorHandlerOld = XSetErrorHandler( ExtDspErrorHandler );
    
    EventHandlerReady( &pedsp->ev, TRUE, 0 );

    return HashAdd( &hdsp, ConnectionNumber( pdsp ), pedsp );
}

extern int ExtDspDestroy( extdisplay *pedsp ) {

    while( pedsp->levReply.plNext != &pedsp->levReply )
	ListDelete( pedsp->levReply.plNext );
    
    while( pedsp->levRequestErrorHandler.plNext !=
	   &pedsp->levRequestErrorHandler )
	ListDelete( pedsp->levRequestErrorHandler.plNext );
    
    EventDestroy( &pedsp->ev );

    return HashDelete( &hdsp, ConnectionNumber( pedsp->pdsp ), NULL );
}

extern extdisplay *ExtDspFind( Display *pdsp ) {

    return HashLookup( &hdsp, ConnectionNumber( pdsp ), NULL );
}

extern int ExtDspReplyEvent( extdisplay *pedsp, unsigned long n, event *pev ) {

    list *pl;
    replyevent *prev, *prevNew = malloc( sizeof( replyevent ) );
    
    for( pl = pedsp->levReply.plNext; pl != &pedsp->levReply;
	 pl = pl->plNext ) {
	prev = pl->p;
	
	if( n < prev->n )
	    break;
    }

    prevNew->pev = pev;
    prevNew->n = n;
    
    return !ListInsert( pl, prevNew );
}

extern int ExtDspHandleNextError( extdisplay *pedsp,
				  int ( *fn )( extdisplay *pedsp,
					       XErrorEvent *pxeev ) ) {
    list *pl;
    requesterrorhandler *preh, *prehNew = malloc( sizeof( *prehNew ) );
    int n = NextRequest( pedsp->pdsp );
    
    for( pl = pedsp->levRequestErrorHandler.plNext;
	 pl != &pedsp->levRequestErrorHandler; pl = pl->plNext ) {
	preh = pl->p;

	if( n < preh->n )
	    break;
    }

    prehNew->fn = fn;
    prehNew->n = n;
    
    return !ListInsert( pl, prehNew );
}

extern int ExtQuarkCreate( extquark *peq ) {

    if( !peq->q )
	peq->q = XrmStringToQuark( peq->sz );

    return 0;
}

extern int ExtResCreateQ( extresource *peres, extresource *peresParent,
			  XrmName qName, XrmClass qClass, extdefault *paed,
			  XrmDatabase rdb ) {

    int i = 0;

    if( peresParent )
	for( ; peresParent->aqName[ i ]; i++ ) {
	    peres->aqName[ i ] = peresParent->aqName[ i ];
	    peres->aqClass[ i ] = peresParent->aqClass[ i ];
	}

    peres->aqName[ i ] = qName;
    peres->aqClass[ i ] = qClass;

    i++;

    peres->aqName[ i ] = 0;
    peres->aqClass[ i ] = 0;

    if( rdb )
	peres->rdb = rdb;
    else if( peresParent )
	peres->rdb = peresParent->rdb;
    else
	assert( 0 );

    peres->paed = paed;
    
    assert( i < EXTMAXRESOURCELEN );

    return 0;
}

extern int ExtResCreateS( extresource *peres, extresource *peresParent,
			  char *szName, char *szClass, extdefault *paed,
			  XrmDatabase rdb ) {

    return ExtResCreateQ( peres, peresParent, XrmStringToQuark( szName ),
			  XrmStringToQuark( szClass ), paed, rdb );
}

extern int ExtResCreate( extresource *peres, extresource *peresParent,
			 extquark *peqName, extquark *peqClass,
			 extdefault *paed, XrmDatabase rdb ) {
    if( !peqName->q )
	ExtQuarkCreate( peqName );

    if( !peqClass->q )
	ExtQuarkCreate( peqClass );

    return ExtResCreateQ( peres, peresParent, peqName->q, peqClass->q, paed,
			  rdb );
}

extern int ExtResLookupQ( extresource *peres, XrmNameList aqNameTail,
			  XrmClassList aqClassTail,
			  XrmRepresentation *pqRep, XrmValue *pval ) {
    int i, iNew;
    Bool fReturn;
    XrmRepresentation qRep;

    pval->addr = NULL;
    
    for( i = 0; peres->aqName[ i ]; i++ )
	;

    for( iNew = 0; aqNameTail[ iNew ]; iNew++ ) {
	peres->aqName[ i + iNew ] = aqNameTail[ iNew ];
	peres->aqClass[ i + iNew ] = aqClassTail[ iNew ];
    }

    assert( i + iNew < EXTMAXRESOURCELEN );
    
    peres->aqName[ i + iNew ] = 0;
    peres->aqClass[ i + iNew ] = 0;

    fReturn = XrmQGetResource( peres->rdb, peres->aqName, peres->aqClass,
			       pqRep ? pqRep : &qRep, pval );

    peres->aqName[ i ] = 0;
    peres->aqClass[ i ] = 0;

    if( fReturn )
	return 0;
    
    if( !aqNameTail[ 1 ] && !aqClassTail[ 1 ] && peres->paed ) {
	extdefault *ped;

	for( ped = peres->paed; ped->peqName; ped++ ) {
	    if( !ped->peqName->q )
		ExtQuarkCreate( ped->peqName );

	    if( ped->peqName->q == aqNameTail[ 0 ] ) {
		pval->addr = ped->szDefault;
		pval->size = strlen( ped->szDefault ) + 1;
		/* FIXME save representation = string */
		return 0;
	    }
	}
    }
    
    return -1;
}

extern int ExtResLookupS( extresource *peres, char *szName, char *szClass,
			 char **ppszType, XrmValue *pval ) {

    XrmName aqName[ EXTMAXRESOURCELEN ];
    XrmClass aqClass[ EXTMAXRESOURCELEN ];
    XrmRepresentation rep;

    XrmStringToQuarkList( szName, aqName );
    XrmStringToQuarkList( szClass, aqClass );

    if( ExtResLookupQ( peres, aqName, aqClass, &rep, pval ) < 0 )
	return -1;

    *ppszType = XrmRepresentationToString( rep );

    return 0;
}

extern int ExtResLookup( extresource *peres, extquark *peqName,
			 extquark *peqClass, XrmRepresentation *pqRep,
			 XrmValue *pval ) {
    XrmName aqName[ 2 ];
    XrmClass aqClass[ 2 ];
    
    if( !peqName->q )
	ExtQuarkCreate( peqName );

    if( !peqClass->q )
	ExtQuarkCreate( peqClass );

    aqName[ 0 ] = peqName->q;
    aqName[ 1 ] = 0;
    
    aqClass[ 0 ] = peqClass->q;
    aqClass[ 1 ] = 0;
    
    return ExtResLookupQ( peres, aqName, aqClass, pqRep, pval );
}

extern int ExtWndCreate( extwindow *pewnd, extresource *peresParent,
			 char *szName, extwindowclass *pewc,
			 XrmDatabase rdb, extdefault *paed, void *pv ) {
    XEvent xev;

    pewnd->pdsp = NULL;
    pewnd->wnd = None;
    pewnd->pewc = pewc;
    pewnd->pv = pv;
    
    if( ExtResCreateS( &pewnd->eres, peresParent, szName, pewc->szClass,
		       paed ? paed : pewc->paed, rdb ) )
	return -1;

    xev.type = ExtPreCreateNotify;
    xev.xclient.format = 8;
    
    return ExtSendEventHandler( pewnd, &xev );
}

extern int ExtWndDestroy( extwindow *pewnd ) {

    XEvent xev;
    
    xev.type = ExtDestroyNotify;
    xev.xclient.format = 8;

    return ExtSendEventHandler( pewnd, &xev );
}

extern int ExtWndRealise( extwindow *pewnd, Display *pdsp,
			  Window wndParent, char *pszGeometry,
			  Window wndOwner, unsigned int id ) {

    int x, y, xDef, yDef, fl, flDef, cBorder;
    unsigned int cxDef, cyDef;
    XWindowAttributes xwa;
    XrmValue xvGeometry, xvBorderWidth;
    char *pszResource;
    XColor xcol, xcolBorder;
    XEvent xev;
    XSetWindowAttributes xswa;
    
    pewnd->pdsp = pdsp;
    pewnd->wndOwner = wndOwner;
    pewnd->id = id;
    
    /* FIXME don't need to query server for window attributes if
       window is local */
    if( !XGetWindowAttributes( pdsp, wndParent, &xwa ) )
	return -1;
    
    pewnd->cm = xwa.colormap;
    pewnd->wndRoot = xwa.root;
    pewnd->nDepth = xwa.depth;
    pewnd->vid = XVisualIDFromVisual( xwa.visual );
    
    /* FIXME do all windows need backgrounds? */
    xcol.red = xcol.green = xcol.blue = 0xFFFF; /* white by default */
    ExtWndAttachColour( pewnd, &eq_background, &eq_Background, &xcol,
			&pewnd->pecolBackground );

    if( ( cBorder =  ExtResLookup( &pewnd->eres, &eq_borderWidth,
				   &eq_BorderWidth,
				   NULL, &xvBorderWidth ) ? 0 :
	  atoi( (char *) xvBorderWidth.addr ) ) ) {
	xcolBorder.red = xcolBorder.green = xcolBorder.blue = 0;
	ExtWndAttachColour( pewnd, &eq_borderColor, &eq_BorderColor,
			    &xcolBorder, &pewnd->pecolBorder );
    } else {
	xcolBorder.pixel = None;
	pewnd->pecolBorder = NULL;
    }

    /* FIXME get quarks for geometry */
    pszResource = (char *) ( !ExtResLookup( &pewnd->eres, &eq_geometry,
					   &eq_Geometry, NULL, &xvGeometry ) ?
			     xvGeometry.addr : NULL );

    fl = XParseGeometry( pszResource, &x, &y, &pewnd->cx, &pewnd->cy );
    flDef = XParseGeometry( pszGeometry, &xDef, &yDef, &cxDef, &cyDef );

    if( !( fl & WidthValue ) )
        pewnd->cx = flDef & WidthValue ? cxDef : 1;

    if( !( fl & HeightValue ) )
        pewnd->cy = flDef & HeightValue ? cyDef : 1;

    if( !( fl & XValue ) ) {
        x = xDef;
        fl &= ~XNegative;
        fl |= flDef & XNegative;
    }

    if( !( fl & YValue ) ) {
        y = yDef;
        fl &= ~YNegative;
        fl |= flDef & YNegative;
    }

    if( fl & XNegative )
        x += xwa.width - pewnd->cx * pewnd->pewc->cxInc -
	    pewnd->pewc->cxDelta - ( cBorder << 1 );

    if( fl & YNegative )
        y += xwa.height - pewnd->cy * pewnd->pewc->cyInc -
	    pewnd->pewc->cyDelta - ( cBorder << 1 );

    /* FIXME could initialise more of xswa from class */
    xswa.background_pixel = xcol.pixel;
    xswa.border_pixel = xcolBorder.pixel;
    xswa.event_mask = pewnd->pewc->flEventMask | StructureNotifyMask;
    
    pewnd->wnd = XCreateWindow( pdsp, wndParent, x, y, pewnd->cx, pewnd->cy,
				cBorder, CopyFromParent, CopyFromParent,
				CopyFromParent, CWBackPixel | CWEventMask |
				( cBorder ? CWBorderPixel : 0 ), &xswa );

    XSaveContext( pdsp, pewnd->wnd, xc, (XPointer) pewnd );

    if( pewnd->wndRoot == wndParent ) {
	int i;
	XClassHint xch;
	XSizeHints xsh;
	
	for( i = 0; pewnd->eres.aqName[ i ]; i++ )
	    ;

	xch.res_name = XrmQuarkToString( pewnd->eres.aqName[ i - 1 ] );
	xch.res_class = pewnd->pewc->szClass;

	xsh.flags = 0;
	/* FIXME fill xsh */
	
	XmbSetWMProperties( pdsp, pewnd->wnd, "FIXME name",
			    "FIXME icon name", NULL, 0
			    /* FIXME argv argc */, &xsh,
			    NULL /* FIXME WM */, &xch );
#if 0	
	XSetTransientForHint();
	XSetWMProtocols();
	XSetWMColormapWindows();
	XSetIconSizes();
#endif	
    }
    
    xev.type = ExtCreateNotify;
    xev.xclient.format = 8;
    xev.xclient.data.b[ 0 ] = 1;
    
    return ExtSendEventHandler( pewnd, &xev );
}

extern extwindow *ExtWndCreateWindows( extwindow *pewndParent,
				       extwindowspec *paews, int c ) {
    extwindow *paewnd = malloc( c * sizeof( *paewnd ) ), *pewnd;
    extwindowspec *pews;
    int i;
    
    if( !paewnd )
	return NULL;

    for( i = 0, pewnd = paewnd, pews = paews; i < c; i++, pewnd++, pews++ ) {
	ExtWndCreate( pewnd, &pewndParent->eres, pews->szName, pews->pewc,
		      pewndParent->eres.rdb, pews->paed, pews->pv );
	ExtWndRealise( pewnd, pewndParent->pdsp, pewndParent->wnd,
		       NULL, pewndParent->wnd, pews->id );
    }

    return paewnd;
}

extern int ExtWndAttach( extwindow *pewnd, Display *pdsp, Window wnd ) {

    XEvent xev;
    
    pewnd->pdsp = pdsp;
    pewnd->wnd = wnd;

    /* FIXME query geometry, colormap, screen, depth */
    
    XSaveContext( pdsp, wnd, xc, (XPointer) pewnd );

    xev.type = ExtCreateNotify;
    xev.xclient.format = 8;
    xev.xclient.data.b[ 0 ] = 0;
    
    return ExtSendEventHandler( pewnd, &xev );
}

#if EXT_DEBUG
static int acColour[ 2 ], acFont[ 2 ], acGC[ 2 ];
#endif

extern XColor *ExtWndLookupColour( extwindow *pewnd, char *szName ) {
    colournamehashdata cnhd, *pcnhd;

    cnhd.sz = szName;
    cnhd.idDsp = ConnectionNumber( pewnd->pdsp );
    cnhd.wndRoot = pewnd->wndRoot;
    
    if( !( pcnhd = HashLookup( &hColourName, ColourNameHash( &cnhd ),
			       &cnhd ) ) ) {
	pcnhd = (colournamehashdata *) malloc( sizeof( colournamehashdata ) );
	pcnhd->sz = cnhd.sz;
	pcnhd->idDsp = cnhd.idDsp;
	pcnhd->wndRoot = cnhd.wndRoot;
	XParseColor( pewnd->pdsp, pewnd->cm, pcnhd->sz, &pcnhd->xcol );
	HashAdd( &hColourName, ColourNameHash( pcnhd ), pcnhd );
    }

    return &pcnhd->xcol;
}

extern int ExtWndAttachColour( extwindow *pewnd, extquark *peqName,
			       extquark *peqClass, XColor *pxcol,
			       extcolour **ppecol ) {
    XrmValue xv;
    colourhashdata chd, *pchd;
    
    if( !ExtResLookup( &pewnd->eres, peqName, peqClass, NULL, &xv ) )
	*pxcol = *ExtWndLookupColour( pewnd, (char *) xv.addr );

    chd.xcol = *pxcol;
    chd.idDsp = ConnectionNumber( pewnd->pdsp );
    chd.cmap = pewnd->cm;

    if( ( pchd = HashLookup( &hColour, ColourHash( &chd ), &chd ) ) ) {
	*pxcol = pchd->xcol;
	pchd->cRef++;
    } else {
	XAllocColor( pewnd->pdsp, pewnd->cm, pxcol );
	/* FIXME if that failed, look up in standard colourmap */
	pchd = (colourhashdata *) malloc( sizeof( colourhashdata ) );
	*pchd = chd;
	pchd->xcol.pixel = pxcol->pixel;
	pchd->cRef = 1;
	HashAdd( &hColour, ColourHash( pchd ), pchd );
#if EXT_DEBUG
	acColour[ 1 ]++;
#endif
    }

#if EXT_DEBUG    
    printf( "%d colours of %d (%s)\n", ++acColour[ 0 ], acColour[ 1 ], szName );
#endif
    
    *ppecol = pchd;
    
    return 0;
}

extern XFontStruct *ExtWndAttachFont( extwindow *pewnd, extquark *peqName,
				      extquark *peqClass, char *szDefault ) {
    XrmValue xv;
    fonthashdata fhd, *pfhd;
    
    fhd.sz = ExtResLookup( &pewnd->eres, peqName, peqClass, NULL, &xv ) ?
	szDefault : xv.addr;
    fhd.idDsp = ConnectionNumber( pewnd->pdsp );

    if( ( pfhd = HashLookup( &hFont, FontHash( &fhd ), &fhd ) ) ) {
	if( !pfhd->cRef++ )
	    XSaveContext( pewnd->pdsp, pfhd->pxfs->fid =
			  XLoadFont( pewnd->pdsp, fhd.sz ),
			  xc, (XPointer) pfhd );
#if EXT_DEBUG    
	printf( "%d fonts of %d\n", ++acFont[ 0 ], acFont[ 1 ] );
#endif	
	return pfhd->pxfs;
    } else {
	if( ( fhd.pxfs = XLoadQueryFont( pewnd->pdsp, fhd.sz ) ) ) {
	    pfhd = (fonthashdata *) malloc( sizeof( fonthashdata ) );
	    pfhd->idDsp = fhd.idDsp;
	    pfhd->sz = strdup( fhd.sz );
	    pfhd->pxfs = fhd.pxfs;
	    pfhd->cRef = 1;
	    XSaveContext( pewnd->pdsp, pfhd->pxfs->fid, xc, (XPointer) pfhd );
	    HashAdd( &hFont, FontHash( pfhd ), pfhd );
#if EXT_DEBUG    
	    printf( "%d fonts of %d\n", ++acFont[ 0 ], ++acFont[ 1 ] );
#endif	    
	}
	return fhd.pxfs;
    }
}

extern GC ExtWndAttachGC( extwindow *pewnd, unsigned long flValues,
			  XGCValues *pxgcv ) {
    
    gchashdata gchd, *pgchd;
    long ngchdHash;
    
    gchd.idDsp = ConnectionNumber( pewnd->pdsp );
    gchd.wndRoot = pewnd->wndRoot;
    gchd.nDepth = pewnd->nDepth;
    if( ( gchd.fl = flValues ) )
	gchd.xgcv = *pxgcv;

    ngchdHash = GCHash( &gchd );
    
    if( ( pgchd = HashLookup( &hGC, ngchdHash, &gchd ) ) ) {
	pgchd->cRef++;
#if EXT_DEBUG    
	printf( "%d GCs of %d\n", ++acGC[ 0 ], acGC[ 1 ] );
#endif	
	return pgchd->gc;
    } else {
	pgchd = (gchashdata *) malloc( sizeof( gchashdata ) );
	*pgchd = gchd;
	pgchd->gc = XCreateGC( pewnd->pdsp, pewnd->wnd, flValues, pxgcv );
	pgchd->cRef = 1;
	XSaveContext( pewnd->pdsp, XGContextFromGC( pgchd->gc ), xc,
		      (XPointer) pgchd );
	HashAdd( &hGC, ngchdHash, pgchd );
#if EXT_DEBUG    
	printf( "%d GCs of %d\n", ++acGC[ 0 ], ++acGC[ 1 ] );
#endif	
	return pgchd->gc;
    }
}

extern int ExtWndDetachColour( extwindow *pewnd, extcolour *pecol ) {

    if( !--pecol->cRef ) {
	HashDelete( &hColour, ColourHash( pecol ), pecol );
	XFreeColors( pewnd->pdsp, pewnd->cm, &pecol->xcol.pixel, 1, 0 );
	free( pecol );
#if EXT_DEBUG
	acColour[ 1 ]--;
#endif
    }
    
#if EXT_DEBUG    
    printf( "%d colours of %d\n", --acColour[ 0 ], acColour[ 1 ] );
#endif
    
    return 0;
}

extern int ExtWndDetachFont( extwindow *pewnd, Font font ) {

    fonthashdata *pfhd;
    
    if( XFindContext( pewnd->pdsp, font, xc, (XPointer *) &pfhd ) )
	return -1;

    if( !--pfhd->cRef )
	XUnloadFont( pewnd->pdsp, pfhd->pxfs->fid );
    
#if EXT_DEBUG    
    printf( "%d fonts of %d\n", --acFont[ 0 ], acFont[ 1 ] );
#endif    
        
    return 0;
}

extern int ExtWndDetachGC( extwindow *pewnd, GC gc ) {

    gchashdata *pgchd;

    if( XFindContext( pewnd->pdsp, XGContextFromGC( gc ), xc,
		      (XPointer *) &pgchd ) )
	return -1;

    if( !--pgchd->cRef ) {
	HashDelete( &hGC, GCHash( pgchd ), pgchd );
	XFreeGC( pewnd->pdsp, pgchd->gc );
	free( pgchd );
#if EXT_DEBUG
	acGC[ 1 ]--;
#endif
    }
    
#if EXT_DEBUG    
    printf( "%d GCs of %d\n", --acGC[ 0 ], acGC[ 1 ] );
#endif    
        
    return 0;
}

extern Screen *ExtScreenOfRoot( Display *pdsp, Window wnd ) {

    int i;
    
    for( i = 0; i < ScreenCount( pdsp ); i++ )
	if( wnd == RootWindow( pdsp, i ) )
	    return ScreenOfDisplay( pdsp, i );

    return NULL;
}

extern XStandardColormap *ExtGetColourmap( Display *pdsp, Window wndRoot,
					   VisualID vid ) {
    /* Finds a suitable default colourmap to use:
     *
     * 1) if a colourmap for this display, screen and visual is already
     *    hashed, use that.
     * 2) if the visual is True Colour, Static Colour or Static Grey, the
     *    best colourmap is hard-wired; use it.
     * 3) if an RGB_DEFAULT_MAP for the appropriate visual already exists,
     *    use it.
     * 4) if at least 8 cells can be allocated from the default colourmap,
     *    then create the best RGB_DEFAULT_MAP possible in as many cells
     *    were allocated, and use that.
     * 5) if an RGB_BEST_MAP for the appropriate visual already exists, use
     *    it.
     * 6) if a private colourmap of at least 8 cells can be allocated, then
     *    create an RGB_BEST_MAP into it, and use that.
     * 7) if all else fails, return the plain monochrome colourmap with
     *    BlackPixel and WhitePixel. */

    /* FIXME let them specify a preference for which method is used -- query
       a resource, start at that method and continue down in order if they
       fail */
    
    colourmaphashdata cmhd, *pcmhd;
    long ncmhdHash;
    XStandardColormap *pxscm;
    unsigned long ul;
    int n, cxscm, ixscm, fSuccess, i, iR, iG, iB;
    XVisualInfo *pxvi, xvi;
    Screen *pscr;
    Colormap cm;
    Display *pdspAlt;
    unsigned long *ppix;
    XColor *pxc = NULL;
    
    cmhd.idDsp = ConnectionNumber( pdsp );
    cmhd.wndRoot = wndRoot;
    cmhd.vid = vid;

    ncmhdHash = ColourMapHash( &cmhd );

    /* case 1) */
    if( ( pcmhd = HashLookup( &hColourMap, ncmhdHash, &cmhd ) ) )
	return &pcmhd->xscm;

    pcmhd = (colourmaphashdata *) malloc( sizeof( *pcmhd ) );
    pcmhd->xscm.visualid = vid;

    xvi.visualid = vid;
    pxvi = XGetVisualInfo( pdsp, VisualIDMask, &xvi, &n );

    /* case 2) */
    switch( pxvi->class ) {
    case StaticGray:
    case StaticColor:
    case TrueColor:
	pcmhd->xscm.killid = ReleaseByFreeingColormap;
	pcmhd->xscm.colormap = XCreateColormap( pdsp, wndRoot, pxvi->visual,
						AllocNone );
	if( pxvi->red_mask ) {
	    for( n = 0, ul = pxvi->red_mask; !( ul & 1 ); ul >>= 1, n++ )
		;

	    pcmhd->xscm.red_max = ul;
	    pcmhd->xscm.red_mult = 1 << n;
	} else {
	    pcmhd->xscm.red_max = 0;
	    pcmhd->xscm.red_mult = 0;
	}
	
	if( pxvi->green_mask ) {
	    for( n = 0, ul = pxvi->green_mask; !( ul & 1 ); ul >>= 1, n++ )
		;

	    pcmhd->xscm.green_max = ul;
	    pcmhd->xscm.green_mult = 1 << n;
	} else {
	    pcmhd->xscm.green_max = 0;
	    pcmhd->xscm.green_mult = 0;
	}

	if( pxvi->blue_mask ) {
	    for( n = 0, ul = pxvi->blue_mask; !( ul & 1 ); ul >>= 1, n++ )
		;

	    pcmhd->xscm.blue_max = ul;
	    pcmhd->xscm.blue_mult = 1 << n;
	} else {
	    pcmhd->xscm.blue_max = 0;
	    pcmhd->xscm.blue_mult = 0;
	}
	
	HashAdd( &hColourMap, ncmhdHash, pcmhd );
	return &pcmhd->xscm;
    }

    /* case 3) */
    if( XGetRGBColormaps( pdsp, wndRoot, &pxscm, &cxscm,
			  XA_RGB_DEFAULT_MAP ) ) {
	for( ixscm = 0; ixscm < cxscm; ixscm++ )
	    if( pxscm->visualid == vid ) {
		pcmhd->xscm = pxscm[ ixscm ];
		HashAdd( &hColourMap, ncmhdHash, pcmhd );
		XFree( pxscm );
		return &pcmhd->xscm;
	    }
	
	XFree( pxscm );
    }

    /* case 4) */
    pscr = ExtScreenOfRoot( pdsp, wndRoot );
    cm = DefaultColormapOfScreen( pscr );
    fSuccess = FALSE;
    if( ( XVisualIDFromVisual( DefaultVisualOfScreen( pscr ) ) == vid )
	/* FIXME and cm is big enough */
	&& ( pdspAlt = XOpenDisplay( XDisplayString( pdsp ) ) ) ) {
	ppix = malloc( pxvi->colormap_size * sizeof( long ) );
	
	pcmhd->xscm.colormap = cm;
        pcmhd->xscm.killid = (XID) XCreatePixmap( pdspAlt, wndRoot, 1, 1, 1 );
	
	switch( pxvi->class ) {
	case GrayScale:
	    for( n = pxvi->colormap_size; n >= 4; n >>= 1 )
		if( XAllocColorCells( pdspAlt, cm, 1, NULL, 0, ppix, n ) ) {
		    pcmhd->xscm.red_max = n - 1;
		    pcmhd->xscm.green_max = pcmhd->xscm.blue_max = 0;
		    pcmhd->xscm.red_mult = 1;
		    pcmhd->xscm.green_mult =  pcmhd->xscm.blue_mult = 0;
		    pcmhd->xscm.base_pixel = *ppix;

		    pxc = malloc( n * sizeof( XColor ) );

		    for( i = 0; i < n; i++ ) {
			pxc[ i ].pixel = pcmhd->xscm.base_pixel + i;
			pxc[ i ].flags = DoRed | DoGreen | DoBlue;
			pxc[ i ].red = pxc[ i ].green = pxc[ i ].blue =
			    i * 0xFFFF / ( n - 1 );
		    }

		    XStoreColors( pdspAlt, cm, pxc, n );

		    fSuccess = TRUE;
		    break;
		}
	    break;

	case PseudoColor:
	    for( n = 1; n * n * n <= pxvi->colormap_size; n++ )
		;

	    for( n--; n >= 2; n-- )
		if( XAllocColorCells( pdspAlt, cm, 1, NULL, 0, ppix,
				      n * n * n ) ) {
		    pcmhd->xscm.red_max = pcmhd->xscm.green_max = 
			pcmhd->xscm.blue_max = n - 1;
		    pcmhd->xscm.red_mult = n * n;
		    pcmhd->xscm.green_mult = n;
		    pcmhd->xscm.blue_mult = 1;
		    pcmhd->xscm.base_pixel = *ppix;

		    pxc = malloc( n * n * n * sizeof( XColor ) );

		    i = 0;
		    for( iR = 0; iR < n; iR++ )
			for( iG = 0; iG < n; iG++ )
			    for( iB = 0; iB < n; iB++, i++ ) {
				pxc[ i ].pixel = pcmhd->xscm.base_pixel + i;
				pxc[ i ].flags = DoRed | DoGreen | DoBlue;
				pxc[ i ].red = iR * 0xFFFF /
				    pcmhd->xscm.red_max;
				pxc[ i ].green = iG * 0xFFFF /
				    pcmhd->xscm.green_max;
				pxc[ i ].blue = iB * 0xFFFF /
				    pcmhd->xscm.blue_max;
			    }

		    XStoreColors( pdspAlt, cm, pxc, n * n * n );

		    fSuccess = TRUE;
		    break;
		}
	    break;

	case DirectColor:
	    for( n = pxvi->colormap_size; n >= 2; n >>= 1 )
		if( XAllocColorCells( pdspAlt, cm, 1, NULL, 0, ppix, n ) ) {
		    /* store colours */
		    /* set up xcsm */
		    fSuccess = TRUE;
		    break;
		}
	    break;
	}

	free( ppix );
	if( pxc )
	    free( pxc );
	
	if( fSuccess ) {
	    XGrabServer( pdspAlt );
	    /* FIXME check again nobody else has allocated in meantime */
	    XSetRGBColormaps( pdspAlt, wndRoot, &pcmhd->xscm, 1, XA_RGB_DEFAULT_MAP );
	    XSetCloseDownMode( pdspAlt, RetainPermanent );
	    XUngrabServer( pdspAlt );
	    XCloseDisplay( pdspAlt );
	    HashAdd( &hColourMap, ncmhdHash, pcmhd );
	    return &pcmhd->xscm;
	}

	XCloseDisplay( pdspAlt );
    }
    
    /* case 5) */
    /* FIXME */
    
    /* case 6) */
    /* FIXME */
    
    /* case 7) */
    pcmhd->xscm.colormap = cm;
    pcmhd->xscm.killid = ReleaseByFreeingColormap;
    pcmhd->xscm.base_pixel = BlackPixelOfScreen( pscr );
    pcmhd->xscm.red_max = 1;
    pcmhd->xscm.green_max = 0;
    pcmhd->xscm.blue_max = 0;
    pcmhd->xscm.red_mult = WhitePixelOfScreen( pscr )
	- BlackPixelOfScreen( pscr );
    pcmhd->xscm.green_mult = 0;
    pcmhd->xscm.blue_mult = 0;

    HashAdd( &hColourMap, ncmhdHash, pcmhd );
    return &pcmhd->xscm;
}

static int ExtHandleDestroyNotify( extwindow *pewnd ) {

    if( pewnd->pecolBackground )
	ExtWndDetachColour( pewnd, pewnd->pecolBackground );
    
    if( pewnd->pecolBorder )
	ExtWndDetachColour( pewnd, pewnd->pecolBorder );
    
    return XDeleteContext( pewnd->pdsp, pewnd->wnd, xc ) ? -1 : 0;
}

extern int ExtHandler( extwindow *pewnd, XEvent *pxev ) {

    switch( pxev->type ) {
    case ConfigureNotify:
	pewnd->cx = pxev->xconfigure.width;
	pewnd->cy = pxev->xconfigure.height;
	break;

    case DestroyNotify:
	if( pxev->xdestroywindow.window == pewnd->wnd )
	    ExtHandleDestroyNotify( pewnd );
	break;
	
    case ClientMessage:
	/* FIXME anything here? */
    }

    return 0;
}

extern int ExtSendEvent( Display *pdsp, Window wnd, XEvent *pxev ) {

    extwindow *pewnd;

    pxev->xany.display = pdsp;
    pxev->xany.window = wnd;
    
    if( !XFindContext( pdsp, wnd, xc, (XPointer *) &pewnd ) ) {
	pxev->xany.serial = NextRequest( pewnd->pdsp ) - 1;
	pxev->xany.send_event = -1;

	pewnd->pewc->pxevh( pewnd, pxev );

	return 0;
    }

    /* FIXME translate event and send to server */

    return 0;
}

extern int ExtSendEventHandler( extwindow *pewnd, XEvent *pxev ) {

    pxev->xany.display = pewnd->pdsp;
    pxev->xany.window = pewnd->wnd;
    pxev->xany.serial = pewnd->pdsp ? NextRequest( pewnd->pdsp ) - 1 : -1;
    pxev->xany.send_event = -1;

    pewnd->pewc->pxevh( pewnd, pxev );

    return 0;
}

extern int ExtChangeProperty( Display *pdsp, Window wnd, char id, int format,
			      void *pv, int c ) {
    extwindow *pewnd;
    
    if( !XFindContext( pdsp, wnd, xc, (XPointer *) &pewnd ) ) {
	extpropertyevent epev;

	epev.type = ExtPropertyNotify;
	epev.serial = NextRequest( pewnd->pdsp ) - 1;
	epev.send_event = -1;
	epev.display = pdsp;
	epev.window = wnd;
	epev.id = id;
	epev.format = format;
	epev.pv = pv;
	epev.c = c;

	pewnd->pewc->pxevh( pewnd, (XEvent *) &epev );

	return 0;
    }
    
    /* FIXME translate event and send to server */

    return 0;
}

extern int ExtChangePropertyHandler( extwindow *pewnd, char id,
				     int format, void *pv, int c ) {
    extpropertyevent epev;

    epev.type = ExtPropertyNotify;
    epev.serial = pewnd->pdsp ? NextRequest( pewnd->pdsp ) - 1 : -1;
    epev.send_event = -1;
    epev.display = pewnd->pdsp;
    epev.window = pewnd->wnd;
    epev.id = id;
    epev.format = format;
    epev.pv = pv;
    epev.c = c;
    
    pewnd->pewc->pxevh( pewnd, (XEvent *) &epev );

    return 0;
}

static int ExtTimer( event *pev, extwindow *pewnd ) {

    XEvent xev;

    EventHandlerReady( pev, FALSE, -1 );
    
    xev.type = ExtTimerNotify;
    xev.xclient.format = 16;
    xev.xclient.data.s[ 0 ] = pewnd->id;
    xev.xclient.data.s[ 1 ] = pewnd->wnd;
    
    ExtSendEventHandler( pewnd, &xev );
    
    return 0;
}

eventhandler ExtTimerHandler = { NULL, (eventhandlerfunc) ExtTimer };

extern int InitExt( void ) {
    
    xc = XUniqueContext();

    HashCreate( &hFont, 20, (hashcomparefunc) FontCompare );
    HashCreate( &hColourMap, 20, (hashcomparefunc) ColourMapCompare );
    HashCreate( &hColourName, 20, (hashcomparefunc) ColourNameCompare );
    HashCreate( &hColour, 20, (hashcomparefunc) ColourCompare );
    HashCreate( &hGC, 20, (hashcomparefunc) GCCompare );
    
    HashCreate( &hdsp, 4, NULL );
    
    return 0;
}
