/*
 * gtktexi.c
 *
 * by Gary Wong <gtw@gnu.org>, 2002
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

#include <assert.h>
#include <ctype.h>
#include <hash.h>
#include <list.h>
#include <libxml/parser.h>
#include <stdarg.h>
#include <string.h>

#include "gtktexi.h"

#define BLOCK_SIZE 1024
#define MAX_ITEM_DEPTH 8

typedef struct _nodeinfo {
    char *sz; /* node/anchor tag; malloc()ed */
    int fNode; /* TRUE for nodes, FALSE for anchors */
    int ib; /* approximate byte offset in file (ib <= true offset) */
} nodeinfo;

typedef struct _refinfo {
    char *sz; /* target; malloc()ed */
    int iStart, iEnd; /* offsets in buffer */
} refinfo;

struct _texinfocontext {
    /* Always valid: */
    GtkTextBuffer *ptb;
    FILE *pf;
    char *szFile; /* malloc()ed */
    list lNode; /* list of all node (and anchor) offsets in file */
    list lRef; /* list of all cross reference links in buffer */
    
    /* gtktexi_load() state: */
    int ib, ibNode; /* byte offset into file */
    
    /* RenderNode() state: */
    xmlParserCtxt *pxpc;
    list l; /* active XML elements */
    refinfo *pri;
    int cIgnore, cPreFormat, cPara, cList, fElemStart, fWhitespace,
	fEmptyParagraph, fSingleNode, fFinished;
    char szParameter[ 1024 ], *pch, *szTitleTag, *szSkipTo;
    char achList[ MAX_ITEM_DEPTH ];
    enum { LIST_CONST, LIST_NUMERIC, LIST_ALPHA } aListType[ MAX_ITEM_DEPTH ];
};

static GtkTextTagTable *pttt;
static GtkTextTag *apttItem[ MAX_ITEM_DEPTH ], *pttDefault;
static hash hIgnore, hPreFormat;

static gboolean TagEvent( GtkTextTag *ptt, GtkWidget *pwView, GdkEvent *pev,
			  GtkTextIter *pti, void *pv ) {

    static GtkTextTag *pttButton;
    
    if( pev->type == GDK_BUTTON_PRESS )
	pttButton = ptt;
    else if( pev->type == GDK_BUTTON_RELEASE ) {
	if( ptt == pttButton ) {
	    /* cross reference */
	    GtkTextBuffer *ptb = gtk_text_view_get_buffer(
		GTK_TEXT_VIEW( pwView ) );
	    texinfocontext *ptic = g_object_get_data( G_OBJECT( ptb ),
						      "texinfocontext" );
	    list *pl;
	    refinfo *pri;
	    int i = gtk_text_iter_get_offset( pti );

	    for( pl = ptic->lRef.plNext; ( pri = pl->p ); pl = pl->plNext )
		if( i < pri->iStart ) {
		    pri = NULL;
		    break;
		} else if( i < pri->iEnd )
		    break;

	    if( pri && pri->sz ) {
		/* NB: it is not safe to pass pri->sz, because it is in
		   storage that will be freed by gtktexi_render_node() */
#if __GNUC__
		char sz[ strlen( pri->sz ) + 1 ];
#elif HAVE_ALLOCA
		char *sz = alloca( strlen( pri->sz ) + 1 );
#else
		char sz[ 4096 ];
#endif
		strcpy( sz, pri->sz );
		gtktexi_render_node( ptic, sz );
	    }
	}
	
	pttButton = NULL;
    }
    
    return TRUE; /* not ideal, but GTK gets into all sorts of trouble if
		    we drastically modify the buffer and then allow other
		    handlers to run */
}

extern texinfocontext *gtktexi_create( void ) {

    texinfocontext *ptic = malloc( sizeof( *ptic ) );
    static int fInitialised;
    
    if( !fInitialised ) {
	static char *aszIgnore[] = {
	    "columnfraction",
	    "dircategory",
	    "indexterm",
	    "nodename",
	    "nodenext",
	    "nodeprev",
	    "nodeup",
	    "printindex",
	    "setfilename",
	    "settitle",
	    "urefurl",
	    NULL
	}, *aszPreFormat[] = {
	    "display",
	    "example",
	    "format",
	    "lisp",
	    "smalldisplay",
	    "smallexample",
	    "smallformat",
	    "smalllisp",
	    NULL
	};
	GtkTextTag *ptt;
	int i;
	char **ppch;
    
	pttt = gtk_text_tag_table_new();

	pttDefault = gtk_text_tag_new( NULL );
	g_object_set( G_OBJECT( pttDefault ),
		      "left-margin", 4,
		      "right-margin", 4,
		      "editable", FALSE,
		      "wrap-mode", GTK_WRAP_WORD,
		      NULL );
	gtk_text_tag_table_add( pttt, pttDefault );
    
	ptt = gtk_text_tag_new( "acronym" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_SMALL, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "b" );
	g_object_set( G_OBJECT( ptt ),
		      "weight", PANGO_WEIGHT_BOLD, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "center" );
	g_object_set( G_OBJECT( ptt ),
		      "justification", GTK_JUSTIFY_CENTER, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "cite" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_ITALIC, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "code" );
	g_object_set( G_OBJECT( ptt ),
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "dfn" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_ITALIC, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "display" );
	g_object_set( G_OBJECT( ptt ),
		      "wrap_mode", GTK_WRAP_NONE,
		      "left-margin", 40,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "emph" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_ITALIC, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "example" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
	
	ptt = gtk_text_tag_new( "format" );
	g_object_set( G_OBJECT( ptt ),
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "i" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_ITALIC, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "inforef" );
	g_object_set( G_OBJECT( ptt ),
		      "foreground", "blue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
	g_signal_connect( G_OBJECT( ptt ), "event",
			  G_CALLBACK( TagEvent ), NULL );
    
	for( i = 0; i < MAX_ITEM_DEPTH; i++ ) {
	    apttItem[ i ] = gtk_text_tag_new( NULL );
	    g_object_set( G_OBJECT( apttItem[ i ] ),
			  "left-margin", ( i + 1 ) * 24 + 4, NULL );
	    gtk_text_tag_table_add( pttt, apttItem[ i ] );
	}
    
	ptt = gtk_text_tag_new( "menutitle" );
	g_object_set( G_OBJECT( ptt ),
		      "foreground", "blue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
	g_signal_connect( G_OBJECT( ptt ), "event",
			  G_CALLBACK( TagEvent ), NULL );
    
	ptt = gtk_text_tag_new( "kbd" );
	g_object_set( G_OBJECT( ptt ),
		      "family", "monospace",
		      "style", PANGO_STYLE_OBLIQUE,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "lisp" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "menucomment" );
	g_object_set( G_OBJECT( ptt ),
		      "wrap_mode", GTK_WRAP_NONE, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "multitable" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "right-margin", 40,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "quotation" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "right-margin", 40,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "r" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_NORMAL, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "sc" );
	g_object_set( G_OBJECT( ptt ),
		      "variant", PANGO_VARIANT_SMALL_CAPS, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "smalldisplay" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_SMALL,
		      "wrap_mode", GTK_WRAP_NONE,
		      "left-margin", 40,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "smallexample" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "scale", PANGO_SCALE_SMALL,
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "smallformat" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_SMALL,
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "smalllisp" );
	g_object_set( G_OBJECT( ptt ),
		      "left-margin", 40,
		      "scale", PANGO_SCALE_SMALL,
		      "wrap_mode", GTK_WRAP_NONE,
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "strong" );
	g_object_set( G_OBJECT( ptt ),
		      "weight", PANGO_WEIGHT_BOLD, NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "titlechap" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_XX_LARGE,
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "titlesec" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_X_LARGE,
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "titlesubsec" );
	g_object_set( G_OBJECT( ptt ),
		      "scale", PANGO_SCALE_LARGE,
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "titlesubsubsec" );
	g_object_set( G_OBJECT( ptt ),
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "tt" );
	g_object_set( G_OBJECT( ptt ),
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "uref" );
	g_object_set( G_OBJECT( ptt ),
		      "foreground", "blue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
	g_signal_connect( G_OBJECT( ptt ), "event",
			  G_CALLBACK( TagEvent ), NULL );
    
	ptt = gtk_text_tag_new( "url" );
	g_object_set( G_OBJECT( ptt ),
		      "family", "monospace", NULL );
	gtk_text_tag_table_add( pttt, ptt );
    
	ptt = gtk_text_tag_new( "var" );
	g_object_set( G_OBJECT( ptt ),
		      "style", PANGO_STYLE_OBLIQUE, NULL );
	gtk_text_tag_table_add( pttt, ptt );
	
	ptt = gtk_text_tag_new( "xref" );
	g_object_set( G_OBJECT( ptt ),
		      "foreground", "blue",
		      "underline", PANGO_UNDERLINE_SINGLE,
		      NULL );
	gtk_text_tag_table_add( pttt, ptt );
	g_signal_connect( G_OBJECT( ptt ), "event",
			  G_CALLBACK( TagEvent ), NULL );
    
	HashCreate( &hIgnore, 16, (hashcomparefunc) strcmp );
	for( ppch = aszIgnore; *ppch; ppch++ )
	    HashAdd( &hIgnore, StringHash( *ppch ), *ppch );
	
	HashCreate( &hPreFormat, 16, (hashcomparefunc) strcmp );
	for( ppch = aszPreFormat; *ppch; ppch++ )
	    HashAdd( &hPreFormat, StringHash( *ppch ), *ppch );

	fInitialised = TRUE;
    }

    ptic->ptb = gtk_text_buffer_new( pttt );
    g_object_set_data( G_OBJECT( ptic->ptb ), "texinfocontext", ptic );

    ptic->pf = NULL;
    ptic->szFile = NULL;
    ListCreate( &ptic->lNode );
    ListCreate( &ptic->lRef );
    
    return ptic;
}

extern GtkTextBuffer *gtktexi_get_buffer( texinfocontext *ptic ) {

    return ptic->ptb;
}

static void Unload( texinfocontext *ptic ) {

    list *pl;
    nodeinfo *pni;
    
    if( ptic->pf ) {
	fclose( ptic->pf );
	ptic->pf = NULL;
    }

    if( ptic->szFile ) {
	free( ptic->szFile );
	ptic->szFile = NULL;
    }
    
    while( ( pl = ptic->lNode.plNext ) != &ptic->lNode ) {
	pni = pl->p;
	free( pni->sz );
	free( pni );
	ListDelete( pl );
    }
    
    /* FIXME clear text buffer? */
}

extern void gtktexi_destroy( texinfocontext *ptic ) {

    Unload( ptic );

    /* FIXME unref tag table and buffer? */
}

static void AddNode( texinfocontext *ptic, char *sz, int fNode, int ib ) {
    
    nodeinfo *pni = malloc( sizeof( *pni ) );

    pni->sz = malloc( strlen( sz ) + 1 );
    strcpy( pni->sz, sz );
    pni->fNode = fNode;
    pni->ib = ib;
    
    ListInsert( &ptic->lNode, pni );
}

static void NewParameter( texinfocontext *ptic ) {

    assert( !ptic->pch );
    
    *( ptic->pch = ptic->szParameter ) = 0;
}

static char *ReadParameter( texinfocontext *ptic ) {

    assert( ptic->pch );
    
    ptic->pch = NULL;
    return ptic->szParameter;
}

static void ScanStartElement( void *pv, const xmlChar *pchName,
			      const xmlChar **ppchAttrs ) {

    texinfocontext *ptic = pv;

    if( !strcmp( pchName, "anchor" ) )
	AddNode( ptic, "FIXME handle anchor name attribute", FALSE, ptic->ib );
    else if( !strcmp( pchName, "node" ) )
	ptic->ibNode = ptic->ib;
    else if( !strcmp( pchName, "nodename" ) )
	NewParameter( ptic );
}

static void ScanEndElement( void *pv, const xmlChar *pchName ) {

    texinfocontext *ptic = pv;
    
    if( !strcmp( pchName, "nodename" ) ) {
	AddNode( ptic, ReadParameter( ptic ), TRUE, ptic->ibNode );
	ptic->pch = NULL;
    }
}

static void ScanCharacters( void *pv, const xmlChar *pchIn, int cch ) {

    texinfocontext *ptic = pv;

    if( ptic->pch ) {
	/* reading a parameter */
	memcpy( ptic->pch, pchIn, cch );
	*( ptic->pch += cch ) = 0;
    }
}

static void Err( void *pv, const char *msg, ... ) {

#if 0
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);
#endif
}

xmlSAXHandler xsaxScan = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    ScanStartElement, /* startElement */
    ScanEndElement, /* endElement */
    NULL, /* reference */
    ScanCharacters, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    Err, /* xmlParserWarning */
    Err, /* xmlParserError */
    Err, /* fatal error */
    NULL, /* getParameterEntity */
    NULL, /* cdataBlock; */
    NULL,  /* externalSubset; */
    TRUE
};

extern int gtktexi_load( texinfocontext *ptic, char *szFile ) {

    xmlParserCtxt *pxpc;
    char ach[ BLOCK_SIZE ];
    int cb;

    Unload( ptic );
    
    if( !( ptic->pf = fopen( szFile, "r" ) ) )
	return -1;

    ptic->szFile = malloc( strlen( szFile ) + 1 );
    strcpy( ptic->szFile, szFile );
    
    ptic->ib = 0;
    ptic->pch = NULL;
    
    cb = fread( ach, sizeof( char ), BLOCK_SIZE, ptic->pf );
    
    pxpc = xmlCreatePushParserCtxt( &xsaxScan, ptic, ach, cb, szFile );
    while( ( cb = fread( ach, sizeof( char ), BLOCK_SIZE, ptic->pf ) ) ) {
	xmlParseChunk( pxpc, ach, cb, FALSE );
	ptic->ib += cb;
	
	while( gtk_events_pending() )
	    gtk_main_iteration();	
    }
    xmlParseChunk( pxpc, NULL, 0, TRUE );
    xmlFreeParserCtxt( pxpc );

    return 0;
}

static void BufferAppend( texinfocontext *ptic, const char *pch, int cch ) {
    
    GtkTextIter tiStart, tiEnd;
    int iStart;
    list *pl;

    gtk_text_buffer_get_end_iter( ptic->ptb, &tiEnd );
    iStart = gtk_text_iter_get_offset( &tiEnd );

    gtk_text_buffer_insert( ptic->ptb, &tiEnd, pch, cch );
        
    gtk_text_buffer_get_iter_at_offset( ptic->ptb, &tiStart, iStart );

    gtk_text_buffer_apply_tag( ptic->ptb, pttDefault, &tiStart, &tiEnd );
    for( pl = ptic->l.plNext; pl != &ptic->l; pl = pl->plNext )
	if( pl->p )
	    gtk_text_buffer_apply_tag( ptic->ptb, pl->p, &tiStart, &tiEnd );
}

static void BufferAppendPixbuf( texinfocontext *ptic, GdkPixbuf *ppb ) {
    
    GtkTextIter tiStart, tiEnd;
    int iStart;
    list *pl;

    gtk_text_buffer_get_end_iter( ptic->ptb, &tiEnd );
    iStart = gtk_text_iter_get_offset( &tiEnd );

    gtk_text_buffer_insert_pixbuf( ptic->ptb, &tiEnd, ppb );
        
    gtk_text_buffer_get_iter_at_offset( ptic->ptb, &tiStart, iStart );

    gtk_text_buffer_apply_tag( ptic->ptb, pttDefault, &tiStart, &tiEnd );
    for( pl = ptic->l.plNext; pl != &ptic->l; pl = pl->plNext )
	if( pl->p )
	    gtk_text_buffer_apply_tag( ptic->ptb, pl->p, &tiStart, &tiEnd );
}

static refinfo *NewRef( texinfocontext *ptic ) {

    refinfo *pri = malloc( sizeof( *pri ) );

    pri->sz = NULL;
    pri->iStart = gtk_text_buffer_get_char_count( ptic->ptb );
    pri->iEnd = -1;

    ListInsert( &ptic->lRef, pri );
    
    return pri;
}

static void StartElement( void *pv, const xmlChar *pchName,
			  const xmlChar **ppchAttrs ) {

    texinfocontext *ptic = pv;
    long l = StringHash( pchName );
    GtkTextTag *ptt;

#if 0
    printf( "StartElement(%s)\n", pchName );
#endif
    
    if( ptic->szSkipTo ) {
	/* ignore everything except the <nodename> we're looking for */
	if( !strcmp( pchName, "nodename" ) )
	    NewParameter( ptic );

	return;
    }
    
    if( HashLookup( &hIgnore, l, pchName ) ) {
	ptic->cIgnore++;
	return;
    }

    ptic->fElemStart = TRUE;
    
    if( HashLookup( &hPreFormat, l, pchName ) )
	ptic->cPreFormat++;

    if( !strcmp( pchName, "para" ) || !strcmp( pchName, "group" ) )
	ptic->cPara++;

    if( !strcmp( pchName, "image" ) ||
	!strcmp( pchName, "xrefnodename" ) ||
	!strcmp( pchName, "menunode" ) )
	/* FIXME handle other types of references too */
	NewParameter( ptic );

    if( !strcmp( pchName, "menucomment" ) )
	BufferAppend( ptic, "\t", 1 );
    
    if( !strcmp( pchName, "chapter" ) || !strcmp( pchName, "unnumbered" ) ||
	!strcmp( pchName, "appendix" ) || !strcmp( pchName, "chapheading" ) )
	ptic->szTitleTag = "titlechap";
    else if( !strcmp( pchName, "section" ) ||
	     !strcmp( pchName, "unnumberedsec" ) ||
	     !strcmp( pchName, "appendixsec" ) ||
	     !strcmp( pchName, "heading" ) )
	ptic->szTitleTag = "titlesec";
    else if( !strcmp( pchName, "section" ) ||
	     !strcmp( pchName, "unnumberedsec" ) ||
	     !strcmp( pchName, "appendixsec" ) ||
	     !strcmp( pchName, "heading" ) )
	ptic->szTitleTag = "titlesec";
    else if( !strcmp( pchName, "subsection" ) ||
	     !strcmp( pchName, "unnumberedsubsec" ) ||
	     !strcmp( pchName, "appendixsubsec" ) ||
	     !strcmp( pchName, "subheading" ) )
	ptic->szTitleTag = "titlesubsec";
    else if( !strcmp( pchName, "subsubsection" ) ||
	     !strcmp( pchName, "unnumberedsubsubsec" ) ||
	     !strcmp( pchName, "appendixsubsubsec" ) ||
	     !strcmp( pchName, "subsubheading" ) )
	ptic->szTitleTag = "titlesubsubsec";

    if( !strcmp( pchName, "enumerate" ) ) {
	ptic->cList = MIN( ptic->cList + 1, 7 );
	ptic->achList[ ptic->cList ] = 1; /* FIXME */
	ptic->aListType[ ptic->cList ] = LIST_NUMERIC; /* FIXME */
    } else if( !strcmp( pchName, "itemize" ) ) {
	ptic->cList = MIN( ptic->cList + 1, 7 );
	ptic->achList[ ptic->cList ] = '*'; /* FIXME */
	ptic->aListType[ ptic->cList ] = LIST_CONST;
    } else if( !strcmp( pchName, "table" ) ) {
	ptic->cList = MIN( ptic->cList + 1, 7 );
	ptic->achList[ ptic->cList ] = 0;
	ptic->aListType[ ptic->cList ] = LIST_CONST;
    }

    /* FIXME handle "key" tag here */

    if( !strcmp( pchName, "inforef" ) || !strcmp( pchName, "menuentry" ) ||
	!strcmp( pchName, "uref" ) || !strcmp( pchName, "xref" ) )
	ptic->pri = NewRef( ptic );
    else if( !strcmp( pchName, "menutitle" ) )
	ptic->pri->iStart = gtk_text_buffer_get_char_count( ptic->ptb );
    
    if( !strcmp( pchName, "item" ) ) 
	ptt = apttItem[ CLAMP( ptic->cList, 0, 7 ) ];
    else
	ptt = gtk_text_tag_table_lookup( pttt, strcmp( pchName, "title" ) ?
					 (char *) pchName : ptic->szTitleTag );
    
    ListInsert( &ptic->l, ptt );

    if( !strcmp( pchName, "item" ) ) {
	char szLabel[ 32 ];
	
	switch( ptic->aListType[ ptic->cList ] ) {
	case LIST_CONST:
	    szLabel[ 0 ] = ptic->achList[ ptic->cList ];
	    szLabel[ 1 ] = ' ';
	    szLabel[ 2 ] = 0;
	    break;
	    
	case LIST_NUMERIC:
	    sprintf( szLabel, "%d. ", (int) ptic->achList[ ptic->cList ]++ );
	    break;
	    
	case LIST_ALPHA:
	    szLabel[ 0 ] = ptic->achList[ ptic->cList ]++;
	    szLabel[ 1 ] = '.';
	    szLabel[ 2 ] = ' ';
	    szLabel[ 3 ] = 0;
	    break;
	}

	BufferAppend( ptic, szLabel, -1 );
    }
}

static void EndElement( void *pv, const xmlChar *pchName ) {

    texinfocontext *ptic = pv;
    long l = StringHash( pchName );

#if 0
    printf( "EndElement(%s)\n", pchName );
#endif
    
    if( ptic->szSkipTo ) {
	if( !strcmp( pchName, "nodename" ) &&
	    !strcmp( ReadParameter( ptic ), ptic->szSkipTo ) )
	    ptic->szSkipTo = NULL;
	
	return;
    }
    
    if( HashLookup( &hIgnore, l, pchName ) ) {
	ptic->cIgnore--;
	return;
    }

    if( HashLookup( &hPreFormat, l, pchName ) )
	ptic->cPreFormat--;

    if( !strcmp( pchName, "node" ) && ptic->fSingleNode ) {
	ptic->fFinished = TRUE;
	xmlStopParser( ptic->pxpc );
	return;
    }
    
    if( !strcmp( pchName, "para" ) || !strcmp( pchName, "group" ) )
	ptic->cPara--;

    if( !strcmp( pchName, "image" ) ) {
	GdkPixbuf *ppb;

	/* FIXME look for image in same directory as original file */
	strcpy( ptic->pch, ".png" );
	if( !( ppb = gdk_pixbuf_new_from_file( ptic->szParameter, NULL ) ) ) {
	    strcpy( ptic->pch, ".jpg" );
	    ppb = gdk_pixbuf_new_from_file( ptic->szParameter, NULL );
	}

	if( ppb ) {
	    BufferAppendPixbuf( ptic, ppb );
	    ptic->fEmptyParagraph = FALSE;
	} else
	    /* FIXME draw default "missing image" */
	    ;

	/* FIXME do we need to handle refcounts on the image? */

	ReadParameter( ptic );
    }

    if( !strcmp( pchName, "xrefnodename" ) ) {
	BufferAppend( ptic, ReadParameter( ptic ), -1 );
	ptic->pri->iEnd = gtk_text_buffer_get_char_count( ptic->ptb );
	ptic->pri->sz = strdup( ptic->szParameter );
    } else if( !strcmp( pchName, "menunode" ) ) {
	ReadParameter( ptic );
	ptic->pri->sz = strdup( ptic->szParameter );
    } else if( !strcmp( pchName, "menutitle" ) )
	ptic->pri->iEnd = gtk_text_buffer_get_char_count( ptic->ptb );
    /* FIXME handle other types of references too */
    
    if( !ptic->fEmptyParagraph ) {
	if( !strcmp( pchName, "tableterm" ) ) {
	    BufferAppend( ptic, "\n", 1 );
	    ptic->fEmptyParagraph = TRUE;
	} else if( !strcmp( pchName, "para" ) ||
		   !strcmp( pchName, "entry" ) ||
		   !strcmp( pchName, "row" ) ||
		   !strcmp( pchName, "title" ) ) {
	    BufferAppend( ptic, "\n\n", 2 );
	    ptic->fEmptyParagraph = TRUE;
	}
    }
    
    if( !strcmp( pchName, "enumerate" ) || !strcmp( pchName, "itemize" ) ||
	!strcmp( pchName, "table" ) )
	ptic->cList = MAX( ptic->cList - 1, -1 );

    /* FIXME handle "key" tag here */

    if( !ListEmpty( &ptic->l ) )
	ListDelete( ptic->l.plPrev );
}

static void Characters( void *pv, const xmlChar *pchIn, int cch ) {

    texinfocontext *ptic = pv;
#if __GNUC__
    char sz[ cch ];
#elif HAVE_ALLOCA
    char *sz = alloca( cch );
#else
    char sz[ 4096 ];
#endif
    char *pch;

    if( ptic->pch ) {
	/* reading a parameter */
	assert( ptic->pch + cch - ptic->szParameter <
		sizeof( ptic->szParameter ) );
	
	memcpy( ptic->pch, pchIn, cch );
	*( ptic->pch += cch ) = 0;

	return;
    }
    
    if( ptic->cIgnore || ptic->szSkipTo )
	return;

    if( !( ptic->cPara && ptic->cPreFormat ) ) {
	/* Ignore all whitespace at start of elements. */
	if( !ptic->cPara || ptic->fElemStart ) {
	    while( cch && isspace( *pchIn ) ) {
		cch--;
		pchIn++;
	    }

	    if( !cch )
		return;

	    ptic->fElemStart = FALSE;
	}
    
	/* FIXME preserve two spaces after a full stop */
	
	for( pch = sz; cch--; pchIn++ )
	    if( isspace( *pchIn ) ) {
		if( !ptic->fWhitespace )
		    *pch++ = ' ';
		
		ptic->fWhitespace = TRUE;
	    } else {
		*pch++ = *pchIn;
		ptic->fWhitespace = FALSE;
		ptic->fEmptyParagraph = FALSE;
	    }

	*pch = 0;

	BufferAppend( ptic, sz, -1 );
    } else {
	BufferAppend( ptic, pchIn, cch );
	ptic->fEmptyParagraph = FALSE;
    }
}

xmlSAXHandler xsax = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    StartElement, /* startElement */
    EndElement, /* endElement */
    NULL, /* reference */
    Characters, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    Err, /* xmlParserWarning */
    Err, /* xmlParserError */
    Err, /* fatal error */
    NULL, /* getParameterEntity */
    NULL, /* cdataBlock; */
    NULL,  /* externalSubset; */
    TRUE
};

/* Scan for string SZ in stream PF, using buffer ACH (size *PCCH).  If
   found, return the offset of the first byte in the string, and set
   *PCCH to the number of following characters in the buffer.  If not
   found, return -1. */
static int ScanFor( FILE *pf, char *sz, char ach[], int *pcch ) {

    int cLeftover = 0, cch, cchTarget = strlen( sz ), i;

    if( cchTarget > *pcch )
	/* can't be done */
	return -1;
    
    while( ( cch = fread( ach + cLeftover, sizeof( char ),
			  *pcch - cLeftover, pf ) ) ) {
	cch += cLeftover;
	for( i = 0; i <= cch - cchTarget; i++ )
	    if( !memcmp( ach + i, sz, cchTarget ) ) {
		/* found match */
		*pcch = cch - i;
		return i;
	    }

	for( i = cchTarget - 1; i; i-- )
	    if( !memcmp( ach + cch - i, sz, i ) ) {
		/* found partial match; copy to beginning of buffer for
		   next iteration */
		memcpy( ach, sz, cLeftover = i );
		break;
	    }
    }

    return -1;
}

static void ClearBuffer( texinfocontext *ptic ) {

    list *pl;
    refinfo *pri;
    
    gtk_text_buffer_set_text( ptic->ptb, "", 0 );

    while( ( pl = ptic->lRef.plNext ) != &ptic->lRef ) {
	pri = pl->p;
	free( pri->sz );
	free( pri );
	ListDelete( pl );
    }
}

/* Render node SZTAG, or the entire file is SZTAG is NULL. */
extern int gtktexi_render_node( texinfocontext *ptic, char *szTag ) {
    
    nodeinfo *pni;
    list *pl;
    char ach[ BLOCK_SIZE ];
    int cch, i;

#if 0
    printf( "gtktexi_render_node(%s)\n", szTag );
#endif
    
    /* FIXME handle references to tags in other files */

    ClearBuffer( ptic );
    
    ptic->cIgnore = ptic->cPreFormat = ptic->cPara = 0;
    ptic->cList = -1;
    ptic->fElemStart = ptic->fWhitespace = ptic->fFinished = FALSE;
    ptic->fEmptyParagraph = TRUE;
    ptic->pch = ptic->szTitleTag = NULL;
    ListCreate( &ptic->l );

    if( szTag ) {
	for( pl = ptic->lNode.plNext; pl != &ptic->lNode; pl = pl->plNext ) {
	    pni = pl->p;
#if 0
	    printf( "Looking for %s, got %s\n", szTag, pni->sz );
#endif
	    if( !strcmp( szTag, pni->sz ) )
		break;
	}
	
	if( pl == &ptic->lNode )
	    /* couldn't find node */
	    return -1;

	fseek( ptic->pf, pni->ib, SEEK_SET );
	/* step over the first 9 bytes of the buffer, so we have room to add
	   a "<texinfo>" tag */
	cch = BLOCK_SIZE - 9;
	if( ( i = ScanFor( ptic->pf, "<node>", ach + 9, &cch ) ) < 0 )
	    return -1;
	memcpy( ach + i, "<texinfo>", 9 );
	cch += 9;
	ptic->szSkipTo = pni->sz;
	ptic->fSingleNode = TRUE;
    } else {
	fseek( ptic->pf, 0, SEEK_SET );
	cch = fread( ach, sizeof( char ), BLOCK_SIZE, ptic->pf );
	i = 0;
	ptic->szSkipTo = NULL;
	ptic->fSingleNode = FALSE;
    }
    
#if 0
    printf( "skipping to %s\n", ptic->szSkipTo );
#endif
    
    ptic->pxpc = xmlCreatePushParserCtxt( &xsax, ptic, ach + i, cch,
					  ptic->szFile );
    while( !ptic->fFinished &&
	   ( cch = fread( ach, sizeof( char ), BLOCK_SIZE, ptic->pf ) ) )
	xmlParseChunk( ptic->pxpc, ach, cch, FALSE );
    
    xmlParseChunk( ptic->pxpc, NULL, 0, TRUE );
    xmlFreeParserCtxt( ptic->pxpc );

    while( ( pl = ptic->l.plNext ) != &ptic->l )
	ListDelete( pl );
    
    return 0;
}

/* Things to finish:

   - entities, e.g. &dots;
   - handle multitable properly
   - footnotes
   
   - label items shouldn't be indented quite as far (the start of the
     item text should be aligned with other lines of the item)
   - fix xref and uref display -- make the most of whatever tags are
     present.  Seems impossible to fix completely without patching makeinfo.
   - indentation (will be buggy until makeinfo handles @noindent)
*/
