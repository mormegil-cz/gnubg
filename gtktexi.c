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

#if HAVE_CONFIG_H
#include <config.h>
#else
/* Compiling standalone; assume all dependencies are satisfied. */
#define HAVE_LIBXML2 1
#define USE_GTK2 1
#endif

#if HAVE_LIBXML2 && USE_GTK2

#include <assert.h>
#include <ctype.h>
#include <hash.h>
#include <list.h>
#include <libxml/parser.h>
#include <stdarg.h>
#include <string.h>

#include "gtktexi.h"
#include "i18n.h"

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

struct _gtktexicontext {
    /* Always valid: */
    FILE *pf;
    char *szFile; /* malloc()ed */
    list lNode; /* list of all node (and anchor) offsets in file */
    list lRef; /* list of all cross reference links in buffer */
    list lHistory; /* list (char **) of file/tag pairs */
    list *plCurrent; /* pointer into history list */
    char *aszNavTarget[ 3 ]; /* malloc()ed */
    
    /* gtk_texi_load() state: */
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
static char *aszNavLabel[ 3 ] = { N_("Next:"), N_("Prev:"), N_("Up:") };
static GtkWindowClass *pcParent;

static gboolean TagEvent( GtkTextTag *ptt, GtkWidget *pwView, GdkEvent *pev,
			  GtkTextIter *pti, void *pv ) {

    static GtkTextTag *pttButton;
    
    if( pev->type == GDK_BUTTON_PRESS )
	pttButton = ptt;
    else if( pev->type == GDK_BUTTON_RELEASE ) {
	if( ptt == pttButton ) {
	    /* cross reference */
	    GtkTexi *pw = GTK_TEXI( gtk_widget_get_toplevel( pwView ) );
	    list *pl;
	    refinfo *pri;
	    int i = gtk_text_iter_get_offset( pti );

	    for( pl = pw->ptic->lRef.plNext; ( pri = pl->p ); pl = pl->plNext )
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
		gtk_texi_render_node( pw, sz );
	    }
	}
	
	pttButton = NULL;
    }
    
    return TRUE; /* not ideal, but GTK gets into all sorts of trouble if
		    we drastically modify the buffer and then allow other
		    handlers to run */
}

static void Initialise( void ) {
    
    static char *aszIgnore[] = {
	"columnfraction",
	"dircategory",
	"indexterm",
	"nodename",
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
}

static void ClearBuffer( GtkTexi *pw ) {

    list *pl;
    refinfo *pri;
    int i;
    
    gtk_text_buffer_set_text( pw->ptb, "", 0 );

    while( ( pl = pw->ptic->lRef.plNext ) != &pw->ptic->lRef ) {
	pri = pl->p;
	free( pri->sz );
	free( pri );
	ListDelete( pl );
    }
    
    for( i = 0; i < 3; i++ ) {
	if( pw->ptic->aszNavTarget[ i ] ) {
	    free( pw->ptic->aszNavTarget[ i ] );
	    pw->ptic->aszNavTarget[ i ] = NULL;
	}
	gtk_label_set_text( GTK_LABEL( pw->apwLabel[ i ] ),
			    gettext ( aszNavLabel[ i ] ) );
	gtk_widget_set_sensitive( gtk_widget_get_parent( pw->apwLabel[ i ] ),
				  FALSE );
	gtk_widget_set_sensitive( pw->apwNavMenu[ i ], FALSE );
    }
}

static void Unload( GtkTexi *pw ) {

    list *pl;
    nodeinfo *pni;
    gtktexicontext *ptic = pw->ptic;
    
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

    ClearBuffer( pw );
}

static void AddNode( GtkTexi *pw, char *sz, int fNode, int ib ) {
    
    nodeinfo *pni = malloc( sizeof( *pni ) );

    pni->sz = malloc( strlen( sz ) + 1 );
    strcpy( pni->sz, sz );
    pni->fNode = fNode;
    pni->ib = ib;
    
    ListInsert( &pw->ptic->lNode, pni );
}

static void NewParameter( GtkTexi *pw ) {

    assert( !pw->ptic->pch );
    
    *( pw->ptic->pch = pw->ptic->szParameter ) = 0;
}

static char *ReadParameter( GtkTexi *pw ) {

    assert( pw->ptic->pch );
    
    pw->ptic->pch = NULL;
    return pw->ptic->szParameter;
}

static void ScanStartElement( void *pv, const xmlChar *pchName,
			      const xmlChar **ppchAttrs ) {

    GtkTexi *pw = pv;
    
    if( !strcmp( pchName, "anchor" ) )
	AddNode( pw, "FIXME handle anchor name attribute", FALSE,
		 pw->ptic->ib );
    else if( !strcmp( pchName, "node" ) )
	pw->ptic->ibNode = pw->ptic->ib;
    else if( !strcmp( pchName, "nodename" ) )
	NewParameter( pw );
}

static void ScanEndElement( void *pv, const xmlChar *pchName ) {

    GtkTexi *pw = pv;
    
    if( !strcmp( pchName, "nodename" ) ) {
	AddNode( pw, ReadParameter( pw ), TRUE, pw->ptic->ibNode );
	pw->ptic->pch = NULL;
    }
}

static void ScanCharacters( void *pv, const xmlChar *pchIn, int cch ) {

    GtkTexi *pw = pv;

    if( pw->ptic->pch ) {
	/* reading a parameter */
	memcpy( pw->ptic->pch, pchIn, cch );
	*( pw->ptic->pch += cch ) = 0;
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

extern int gtk_texi_load( GtkTexi *pw, char *szFile ) {

    xmlParserCtxt *pxpc;
    char ach[ BLOCK_SIZE ];
    int cb;

    Unload( pw );
    
    if( !( pw->ptic->pf = fopen( szFile, "r" ) ) )
	return -1;

    pw->ptic->szFile = malloc( strlen( szFile ) + 1 );
    strcpy( pw->ptic->szFile, szFile );
    
    pw->ptic->ib = 0;
    pw->ptic->pch = NULL;
    
    cb = fread( ach, sizeof( char ), BLOCK_SIZE, pw->ptic->pf );
    
    pxpc = xmlCreatePushParserCtxt( &xsaxScan, pw, ach, cb, szFile );
    while( ( cb = fread( ach, sizeof( char ), BLOCK_SIZE, pw->ptic->pf ) ) ) {
	xmlParseChunk( pxpc, ach, cb, FALSE );
	pw->ptic->ib += cb;
	
	while( gtk_events_pending() )
	    gtk_main_iteration();	
    }
    xmlParseChunk( pxpc, NULL, 0, TRUE );
    xmlFreeParserCtxt( pxpc );

    return 0;
}

static void BufferAppend( GtkTexi *pw, const char *pch, int cch ) {
    
    GtkTextIter tiStart, tiEnd;
    int iStart;
    list *pl;

    gtk_text_buffer_get_end_iter( pw->ptb, &tiEnd );
    iStart = gtk_text_iter_get_offset( &tiEnd );

    gtk_text_buffer_insert( pw->ptb, &tiEnd, pch, cch );
        
    gtk_text_buffer_get_iter_at_offset( pw->ptb, &tiStart, iStart );

    gtk_text_buffer_apply_tag( pw->ptb, pttDefault, &tiStart, &tiEnd );
    for( pl = pw->ptic->l.plNext; pl != &pw->ptic->l; pl = pl->plNext )
	if( pl->p )
	    gtk_text_buffer_apply_tag( pw->ptb, pl->p, &tiStart, &tiEnd );
}

static void BufferAppendPixbuf( GtkTexi *pw, GdkPixbuf *ppb ) {
    
    GtkTextIter tiStart, tiEnd;
    int iStart;
    list *pl;

    gtk_text_buffer_get_end_iter( pw->ptb, &tiEnd );
    iStart = gtk_text_iter_get_offset( &tiEnd );

    gtk_text_buffer_insert_pixbuf( pw->ptb, &tiEnd, ppb );
        
    gtk_text_buffer_get_iter_at_offset( pw->ptb, &tiStart, iStart );

    gtk_text_buffer_apply_tag( pw->ptb, pttDefault, &tiStart, &tiEnd );
    for( pl = pw->ptic->l.plNext; pl != &pw->ptic->l; pl = pl->plNext )
	if( pl->p )
	    gtk_text_buffer_apply_tag( pw->ptb, pl->p, &tiStart, &tiEnd );
}

static refinfo *NewRef( GtkTexi *pw ) {

    refinfo *pri = malloc( sizeof( *pri ) );

    pri->sz = NULL;
    pri->iStart = gtk_text_buffer_get_char_count( pw->ptb );
    pri->iEnd = -1;

    ListInsert( &pw->ptic->lRef, pri );
    
    return pri;
}

static void StartElement( void *pv, const xmlChar *pchName,
			  const xmlChar **ppchAttrs ) {

    GtkTexi *pw = pv;
    long l = StringHash( (char *) pchName );
    GtkTextTag *ptt;

#if 0
    printf( "StartElement(%s)\n", pchName );
#endif
    
    if( pw->ptic->szSkipTo ) {
	/* ignore everything except the <nodename> we're looking for */
	if( !strcmp( pchName, "nodename" ) )
	    NewParameter( pw );

	return;
    }
    
    if( HashLookup( &hIgnore, l, (char *) pchName ) ) {
	pw->ptic->cIgnore++;
	return;
    }

    pw->ptic->fElemStart = TRUE;
    
    if( HashLookup( &hPreFormat, l, (char *) pchName ) )
	pw->ptic->cPreFormat++;

    if( !strcmp( pchName, "para" ) || !strcmp( pchName, "group" ) )
	pw->ptic->cPara++;

    if( !strcmp( pchName, "image" ) ||
	!strcmp( pchName, "xrefnodename" ) ||
	!strcmp( pchName, "menunode" ) ||
	/* FIXME handle other types of references too */
	!strcmp( pchName, "nodenext" ) ||
	!strcmp( pchName, "nodeprev" ) ||
	!strcmp( pchName, "nodeup" ) )
	NewParameter( pw );

    if( !strcmp( pchName, "menucomment" ) )
	BufferAppend( pw, "\t", 1 );
    
    if( !strcmp( pchName, "chapter" ) || !strcmp( pchName, "unnumbered" ) ||
	!strcmp( pchName, "appendix" ) || !strcmp( pchName, "chapheading" ) )
	pw->ptic->szTitleTag = "titlechap";
    else if( !strcmp( pchName, "section" ) ||
	     !strcmp( pchName, "unnumberedsec" ) ||
	     !strcmp( pchName, "appendixsec" ) ||
	     !strcmp( pchName, "heading" ) )
	pw->ptic->szTitleTag = "titlesec";
    else if( !strcmp( pchName, "section" ) ||
	     !strcmp( pchName, "unnumberedsec" ) ||
	     !strcmp( pchName, "appendixsec" ) ||
	     !strcmp( pchName, "heading" ) )
	pw->ptic->szTitleTag = "titlesec";
    else if( !strcmp( pchName, "subsection" ) ||
	     !strcmp( pchName, "unnumberedsubsec" ) ||
	     !strcmp( pchName, "appendixsubsec" ) ||
	     !strcmp( pchName, "subheading" ) )
	pw->ptic->szTitleTag = "titlesubsec";
    else if( !strcmp( pchName, "subsubsection" ) ||
	     !strcmp( pchName, "unnumberedsubsubsec" ) ||
	     !strcmp( pchName, "appendixsubsubsec" ) ||
	     !strcmp( pchName, "subsubheading" ) )
	pw->ptic->szTitleTag = "titlesubsubsec";

    if( !strcmp( pchName, "enumerate" ) ) {
	pw->ptic->cList = MIN( pw->ptic->cList + 1, 7 );
	pw->ptic->achList[ pw->ptic->cList ] = 1; /* FIXME */
	pw->ptic->aListType[ pw->ptic->cList ] = LIST_NUMERIC; /* FIXME */
    } else if( !strcmp( pchName, "itemize" ) ) {
	pw->ptic->cList = MIN( pw->ptic->cList + 1, 7 );
	pw->ptic->achList[ pw->ptic->cList ] = '*'; /* FIXME */
	pw->ptic->aListType[ pw->ptic->cList ] = LIST_CONST;
    } else if( !strcmp( pchName, "table" ) ) {
	pw->ptic->cList = MIN( pw->ptic->cList + 1, 7 );
	pw->ptic->achList[ pw->ptic->cList ] = 0;
	pw->ptic->aListType[ pw->ptic->cList ] = LIST_CONST;
    }

    /* FIXME handle "key" tag here */

    if( !strcmp( pchName, "inforef" ) || !strcmp( pchName, "menuentry" ) ||
	!strcmp( pchName, "uref" ) || !strcmp( pchName, "xref" ) )
	pw->ptic->pri = NewRef( pw );
    else if( !strcmp( pchName, "menutitle" ) )
	pw->ptic->pri->iStart = gtk_text_buffer_get_char_count( pw->ptb );
    
    if( !strcmp( pchName, "item" ) ) 
	ptt = apttItem[ CLAMP( pw->ptic->cList, 0, 7 ) ];
    else
	ptt = gtk_text_tag_table_lookup( pttt, strcmp( pchName, "title" ) ?
					 (char *) pchName :
					 pw->ptic->szTitleTag );
    
    ListInsert( &pw->ptic->l, ptt );

    if( !strcmp( pchName, "item" ) ) {
	char szLabel[ 32 ];
	
	switch( pw->ptic->aListType[ pw->ptic->cList ] ) {
	case LIST_CONST:
	    szLabel[ 0 ] = pw->ptic->achList[ pw->ptic->cList ];
	    szLabel[ 1 ] = ' ';
	    szLabel[ 2 ] = 0;
	    break;
	    
	case LIST_NUMERIC:
	    sprintf( szLabel, "%d. ",
		     (int) pw->ptic->achList[ pw->ptic->cList ]++ );
	    break;
	    
	case LIST_ALPHA:
	    szLabel[ 0 ] = pw->ptic->achList[ pw->ptic->cList ]++;
	    szLabel[ 1 ] = '.';
	    szLabel[ 2 ] = ' ';
	    szLabel[ 3 ] = 0;
	    break;
	}

	BufferAppend( pw, szLabel, -1 );
    }
}

static void EndElement( void *pv, const xmlChar *pchName ) {

    GtkTexi *pw = pv;
    long l = StringHash( (char *) pchName );
    int iNavLabel;
    
#if 0
    printf( "EndElement(%s)\n", pchName );
#endif
    
    if( pw->ptic->szSkipTo ) {
	if( !strcmp( pchName, "nodename" ) &&
	    !strcmp( ReadParameter( pw ), pw->ptic->szSkipTo ) )
	    pw->ptic->szSkipTo = NULL;
	
	return;
    }
    
    if( HashLookup( &hIgnore, l, (char *) pchName ) ) {
	pw->ptic->cIgnore--;
	return;
    }

    if( HashLookup( &hPreFormat, l, (char *) pchName ) )
	pw->ptic->cPreFormat--;

    if( !strcmp( pchName, "node" ) && pw->ptic->fSingleNode ) {
	pw->ptic->fFinished = TRUE;
	xmlStopParser( pw->ptic->pxpc );
	return;
    }
    
    if( !strcmp( pchName, "para" ) || !strcmp( pchName, "group" ) )
	pw->ptic->cPara--;

    if( !strcmp( pchName, "image" ) ) {
	GdkPixbuf *ppb;

	/* FIXME look for image in same directory as original file */
	strcpy( pw->ptic->pch, ".png" );
	if( !( ppb = gdk_pixbuf_new_from_file( pw->ptic->szParameter,
					       NULL ) ) ) {
	    strcpy( pw->ptic->pch, ".jpg" );
	    ppb = gdk_pixbuf_new_from_file( pw->ptic->szParameter, NULL );
	}

	if( ppb ) {
	    BufferAppendPixbuf( pw, ppb );
	    pw->ptic->fEmptyParagraph = FALSE;
	} else
	    /* FIXME draw default "missing image" */
	    ;

	/* FIXME do we need to handle refcounts on the image? */

	ReadParameter( pw );
    }

    if( !strcmp( pchName, "xrefnodename" ) ) {
	BufferAppend( pw, ReadParameter( pw ), -1 );
	pw->ptic->pri->iEnd = gtk_text_buffer_get_char_count( pw->ptb );
	pw->ptic->pri->sz = strdup( pw->ptic->szParameter );
    } else if( !strcmp( pchName, "menunode" ) ) {
	ReadParameter( pw );
	pw->ptic->pri->sz = strdup( pw->ptic->szParameter );
    } else if( !strcmp( pchName, "menutitle" ) )
	pw->ptic->pri->iEnd = gtk_text_buffer_get_char_count( pw->ptb );
    /* FIXME handle other types of references too */

    iNavLabel = -1;
    if( !strcmp( pchName, "nodenext" ) )
	iNavLabel = 0;
    else if( !strcmp( pchName, "nodeprev" ) )
	iNavLabel = 1;
    else if( !strcmp( pchName, "nodeup" ) )
	iNavLabel = 2;
    if( iNavLabel >= 0 ) {
	char *pchLabel, *pch = ReadParameter( pw );

	if( pw->ptic->aszNavTarget[ iNavLabel ] )
	    free( pw->ptic->aszNavTarget[ iNavLabel ] );

	strcpy( pw->ptic->aszNavTarget[ iNavLabel ] =
		malloc( strlen( pch ) + 1 ),
		pch );
	
	pchLabel = g_strdup_printf( "%s %s", 
                                    gettext ( aszNavLabel[ iNavLabel ] ), pch );

	gtk_label_set_text( GTK_LABEL( pw->apwLabel[ iNavLabel ] ), pchLabel );
	gtk_widget_set_sensitive( gtk_widget_get_parent(
	    pw->apwLabel[ iNavLabel ] ), TRUE );
	gtk_widget_set_sensitive( pw->apwNavMenu[ iNavLabel ], TRUE );
	g_free( pchLabel );
    }
    
    if( !pw->ptic->fEmptyParagraph ) {
	if( !strcmp( pchName, "tableterm" ) ) {
	    BufferAppend( pw, "\n", 1 );
	    pw->ptic->fEmptyParagraph = TRUE;
	} else if( !strcmp( pchName, "para" ) ||
		   !strcmp( pchName, "entry" ) ||
		   !strcmp( pchName, "row" ) ||
		   !strcmp( pchName, "title" ) ) {
	    BufferAppend( pw, "\n\n", 2 );
	    pw->ptic->fEmptyParagraph = TRUE;
	}
    }
    
    if( !strcmp( pchName, "enumerate" ) || !strcmp( pchName, "itemize" ) ||
	!strcmp( pchName, "table" ) )
	pw->ptic->cList = MAX( pw->ptic->cList - 1, -1 );

    /* FIXME handle "key" tag here */

    if( !ListEmpty( &pw->ptic->l ) )
	ListDelete( pw->ptic->l.plPrev );
}

static void Characters( void *pv, const xmlChar *pchIn, int cch ) {

    GtkTexi *pw = pv;
#if __GNUC__
    char sz[ cch ];
#elif HAVE_ALLOCA
    char *sz = alloca( cch );
#else
    char sz[ 4096 ];
#endif
    char *pch;

    if( pw->ptic->pch ) {
	/* reading a parameter */
	assert( pw->ptic->pch + cch - pw->ptic->szParameter <
		sizeof( pw->ptic->szParameter ) );
	
	memcpy( pw->ptic->pch, pchIn, cch );
	*( pw->ptic->pch += cch ) = 0;

	return;
    }
    
    if( pw->ptic->cIgnore || pw->ptic->szSkipTo )
	return;

    if( !( pw->ptic->cPara && pw->ptic->cPreFormat ) ) {
	/* Ignore all whitespace at start of elements. */
	if( !pw->ptic->cPara || pw->ptic->fElemStart ) {
	    while( cch && isspace( *pchIn ) ) {
		cch--;
		pchIn++;
	    }

	    if( !cch )
		return;

	    pw->ptic->fElemStart = FALSE;
	}
    
	/* FIXME preserve two spaces after a full stop */
	
	for( pch = sz; cch--; pchIn++ )
	    if( isspace( *pchIn ) ) {
		if( !pw->ptic->fWhitespace )
		    *pch++ = ' ';
		
		pw->ptic->fWhitespace = TRUE;
	    } else {
		*pch++ = *pchIn;
		pw->ptic->fWhitespace = FALSE;
		pw->ptic->fEmptyParagraph = FALSE;
	    }

	*pch = 0;

	BufferAppend( pw, sz, -1 );
    } else {
	BufferAppend( pw, pchIn, cch );
	pw->ptic->fEmptyParagraph = FALSE;
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

static void HistoryAdd( GtkTexi *pw, char *szFile, char *szTag ) {

    char **pp;

    while( ( pp = pw->ptic->plCurrent->plNext->p ) ) {
	free( pp[ 0 ] );
	free( pp[ 1 ] );
	free( pp );
	ListDelete( pw->ptic->plCurrent->plNext );
    }

    pp = malloc( 2 * sizeof( *pp ) );
    strcpy( pp[ 0 ] = malloc( strlen( szFile ) + 1 ), szFile );
    strcpy( pp[ 1 ] = malloc( strlen( szTag ) + 1 ), szTag );
    
    pw->ptic->plCurrent = ListInsert( &pw->ptic->lHistory, pp );
}

static int RenderNode( GtkTexi *pw, char *szTag ) {

    nodeinfo *pni;
    list *pl;
    char ach[ BLOCK_SIZE ];
    int cch, i;
    
    /* FIXME handle references to tags in other files */

    ClearBuffer( pw );
    
    pw->ptic->cIgnore = pw->ptic->cPreFormat = pw->ptic->cPara = 0;
    pw->ptic->cList = -1;
    pw->ptic->fElemStart = pw->ptic->fWhitespace = pw->ptic->fFinished = FALSE;
    pw->ptic->fEmptyParagraph = TRUE;
    pw->ptic->pch = pw->ptic->szTitleTag = NULL;
    ListCreate( &pw->ptic->l );

    gtk_widget_set_sensitive( pw->apwGoMenu[ 0 ],
			      pw->ptic->plCurrent->plPrev->p != NULL );
    gtk_widget_set_sensitive( pw->apwButton[ 0 ],
			      pw->ptic->plCurrent->plPrev->p != NULL );
    gtk_widget_set_sensitive( pw->apwGoMenu[ 1 ],
			      pw->ptic->plCurrent->plNext->p != NULL );
    gtk_widget_set_sensitive( pw->apwButton[ 1 ],
			      pw->ptic->plCurrent->plNext->p != NULL );
    
    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pw->pif, "/Go/Top" ), strcmp( szTag, "Top" ) );
	
    if( strcmp( szTag, "*" ) ) {
	for( pl = pw->ptic->lNode.plNext; pl != &pw->ptic->lNode;
	     pl = pl->plNext ) {
	    pni = pl->p;
#if 0
	    printf( "Looking for %s, got %s\n", szTag, pni->sz );
#endif
	    if( !strcmp( szTag, pni->sz ) )
		break;
	}
	
	if( pl == &pw->ptic->lNode )
	    /* couldn't find node */
	    return -1;

	fseek( pw->ptic->pf, pni->ib, SEEK_SET );
	/* step over the first 9 bytes of the buffer, so we have room to add
	   a "<texinfo>" tag */
	cch = BLOCK_SIZE - 9;
	if( ( i = ScanFor( pw->ptic->pf, "<node>", ach + 9, &cch ) ) < 0 )
	    return -1;
	memcpy( ach + i, "<texinfo>", 9 );
	cch += 9;
	pw->ptic->szSkipTo = pni->sz;
	pw->ptic->fSingleNode = TRUE;
    } else {
	fseek( pw->ptic->pf, 0, SEEK_SET );
	cch = fread( ach, sizeof( char ), BLOCK_SIZE, pw->ptic->pf );
	i = 0;
	pw->ptic->szSkipTo = NULL;
	pw->ptic->fSingleNode = FALSE;
    }
    
#if 0
    printf( "skipping to %s\n", pw->ptic->szSkipTo );
#endif
    
    pw->ptic->pxpc = xmlCreatePushParserCtxt( &xsax, pw, ach + i, cch,
					      pw->ptic->szFile );
    while( !pw->ptic->fFinished &&
	   ( cch = fread( ach, sizeof( char ), BLOCK_SIZE, pw->ptic->pf ) ) )
	xmlParseChunk( pw->ptic->pxpc, ach, cch, FALSE );
    
    xmlParseChunk( pw->ptic->pxpc, NULL, 0, TRUE );
    xmlFreeParserCtxt( pw->ptic->pxpc );

    while( ( pl = pw->ptic->l.plNext ) != &pw->ptic->l )
	ListDelete( pl );
    
    return 0;
}

extern int gtk_texi_render_node( GtkTexi *pw, char *szTag ) {
    
#if 0
    printf( "gtktexi_render_node(%s)\n", szTag );
#endif

    HistoryAdd( pw, pw->ptic->szFile, szTag );

    return RenderNode( pw, szTag );
}

static void Go( GtkTexi *pw, list *pl ) {

    char **pp;
    
    if( !( pp = pl->p ) )
	return;

    pw->ptic->plCurrent = pl;

    if( !strcmp( pp[ 0 ], pw->ptic->szFile ) )
	gtk_texi_load( pw, pp[ 0 ] );

    /* FIXME if this is the current node, just jump to the tag */
    RenderNode( pw, pp[ 1 ] );
}

static void GoBack( GtkWidget *pwButton, GtkTexi *pw ) {

    Go( pw, pw->ptic->plCurrent->plPrev );
}

static void GoForward( GtkWidget *pwButton, GtkTexi *pw ) {

    Go( pw, pw->ptic->plCurrent->plNext );
}

static void GoNavTarget( GtkTexi *pw, int i ) {

    /* NB: it is not safe to pass ptic->aszNavTarget[ i ], because it is in
       storage that will be freed by gtktexi_render_node() */
    if( pw->ptic->aszNavTarget[ i ] ) {
#if __GNUC__
	char sz[ strlen( pw->ptic->aszNavTarget[ i ] ) + 1 ];
#elif HAVE_ALLOCA
	char *sz = alloca( strlen( pw->ptic->aszNavTarget[ i ] ) + 1 );
#else
	char sz[ 4096 ];
#endif
	
	strcpy( sz, pw->ptic->aszNavTarget[ i ] );
	gtk_texi_render_node( pw, sz );
    }
}

static void GoNext( GtkWidget *pwButton, GtkTexi *pw ) {

    GoNavTarget( pw, 0 );
}

static void GoPrev( GtkWidget *pwButton, GtkTexi *pw ) {

    GoNavTarget( pw, 1 );
}

static void GoUp( GtkWidget *pwButton, GtkTexi *pw ) {

    GoNavTarget( pw, 2 );
}

static void MenuClose( gpointer pv, guint n, GtkWidget *pwMenu ) {

    gtk_widget_destroy( pv );
}

static void MenuGoBack( gpointer pv, guint n, GtkWidget *pw ) {

    GoBack( pw, pv );
}

static void MenuGoForward( gpointer pv, guint n, GtkWidget *pw ) {

    GoForward( pw, pv );
}

static void MenuGoUp( gpointer pv, guint n, GtkWidget *pw ) {

    GoNavTarget( pv, 2 );
}		  
      		  
static void MenuGoNext( gpointer pv, guint n, GtkWidget *pw ) {

    GoNavTarget( pv, 0 );
}		  
      		  
static void MenuGoPrev( gpointer pv, guint n, GtkWidget *pw ) {

    GoNavTarget( pv, 1 );
}		  
      		  
static void MenuGoTop( gpointer pv, guint n, GtkWidget *pw ) {

    gtk_texi_render_node( pv, "Top" );
}		  
      		  
static void MenuGoDir( gpointer pv, guint n, GtkWidget *pw ) {

    /* FIXME */
}

static void gtk_texi_init( GtkTexi *pw ) {

    static int fInitialised;
    PangoTabArray *pta;
    GtkTextAttributes *ptaDefault;
    GtkAccelGroup *pag;
    GtkWidget *pwHbox, *pwVbox, *pwToolbar, *pwTable, *pwButton;
    static GtkItemFactoryEntry aife[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/File/Close", "<control>W", MenuClose, 0, "<StockItem>",
	  GTK_STOCK_CLOSE },
	{ "/_Edit", NULL, NULL, 0, "<Branch>" },
	{ "/Edit/_Copy", "<control>C", NULL, 0, "<StockItem>",
	  GTK_STOCK_COPY },
	{ "/Edit/-", NULL, NULL, 0, "<Separator>" },
	{ "/Edit/_Find...", "<control>F", NULL, 0, "<StockItem>",
	  GTK_STOCK_FIND },
	{ "/_View", NULL, NULL, 0, "<Branch>" },
	{ "/View/_Node", NULL, NULL, 0, "<RadioItem>" },
	{ "/View/_Chapter", NULL, NULL, 0, "/View/Node" },
	{ "/View/_File", NULL, NULL, 0, "/View/Node" },
	{ "/_Go", NULL, NULL, 0, "<Branch>" },
	{ "/Go/_Back", "L", MenuGoBack, 0, "<StockItem>", GTK_STOCK_GO_BACK },
	{ "/Go/_Forward", NULL, MenuGoForward, 0, "<StockItem>",
	  GTK_STOCK_GO_FORWARD },
	{ "/Go/-", NULL, NULL, 0, "<Separator>" },
	{ "/Go/_Up", "U", MenuGoUp, 0, "<StockItem>", GTK_STOCK_GO_UP },
	{ "/Go/_Next", "N", MenuGoNext, 0 },
	{ "/Go/_Previous", "P", MenuGoPrev, 0 },
	{ "/Go/_Top", "T", MenuGoTop, 0, "<StockItem>", GTK_STOCK_GOTO_TOP },
	{ "/Go/_Dir", "D", MenuGoDir, 0 }
	/* FIXME add a help menu */
    };
    static void (*apfNav[ 3 ] )() = { GoNext, GoPrev, GoUp };
    int i;
    
    if( !fInitialised ) {
	Initialise();
	fInitialised = TRUE;
    }

    pw->ptic = malloc( sizeof( *pw->ptic ) );
    
    gtk_container_add( GTK_CONTAINER( pw ),
		       pwVbox = gtk_vbox_new( FALSE, 0 ) );

    pag = gtk_accel_group_new();
    gtk_window_add_accel_group( GTK_WINDOW( pw ), pag );
    pw->pif = gtk_item_factory_new( GTK_TYPE_MENU_BAR, "<gtktexi-main>",
				      pag );
    g_object_unref( G_OBJECT( pag ) );
    
    gtk_item_factory_create_items( pw->pif, sizeof( aife ) /
				   sizeof( aife[ 0 ] ), aife, pw );
    gtk_box_pack_start( GTK_BOX( pwVbox ),
			gtk_item_factory_get_widget( pw->pif,
						     "<gtktexi-main>" ),
			FALSE, FALSE, 0 );

    pwHbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( pwVbox ), pwHbox, FALSE, FALSE, 0 );
    
    pwToolbar = gtk_toolbar_new();
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwToolbar, FALSE, FALSE, 0 );
    pw->apwButton[ 0 ] = gtk_toolbar_insert_stock(
	GTK_TOOLBAR( pwToolbar ), GTK_STOCK_GO_BACK, "tooltip", "private",
	G_CALLBACK( GoBack ), pw, -1 );
    gtk_widget_set_sensitive( pw->apwButton[ 0 ], FALSE );
    pw->apwButton[ 1 ] = gtk_toolbar_insert_stock(
	GTK_TOOLBAR( pwToolbar ), GTK_STOCK_GO_FORWARD, "tooltip", "private",
	G_CALLBACK( GoForward ), pw, -1 );
    gtk_widget_set_sensitive( pw->apwButton[ 1 ], FALSE );

    gtk_widget_set_sensitive( pw->apwGoMenu[ 0 ] = gtk_item_factory_get_item(
	pw->pif, "/Go/Back" ), FALSE );
    gtk_widget_set_sensitive( pw->apwGoMenu[ 1 ] = gtk_item_factory_get_item(
	pw->pif, "/Go/Forward" ), FALSE );
    
    pwTable = gtk_table_new( 1, 3, TRUE );
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwTable, TRUE, TRUE, 0 );

    for( i = 0; i < 3; i++ ) {
	pw->ptic->aszNavTarget[ i ] = NULL;
	pw->apwLabel[ i ] = gtk_label_new( gettext ( aszNavLabel[ i ] ) );
	gtk_misc_set_alignment( GTK_MISC( pw->apwLabel[ i ] ), 0.0, 0.5 );

	pwButton = gtk_button_new();
	gtk_widget_set_sensitive( pwButton, FALSE );
	g_signal_connect( G_OBJECT( pwButton ), "clicked",
			  G_CALLBACK( apfNav[ i ] ), pw );
	gtk_widget_set_size_request( pwButton, 0, -1 );
	gtk_container_add( GTK_CONTAINER( pwButton ), pw->apwLabel[ i ] );
	gtk_table_attach( GTK_TABLE( pwTable ), pwButton, i, i + 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, 0, 0 );
    }
    
    gtk_widget_set_sensitive( pw->apwNavMenu[ 0 ] =
			      gtk_item_factory_get_item(
				  pw->pif, "/Go/Next" ), FALSE );
    gtk_widget_set_sensitive( pw->apwNavMenu[ 1 ] =
			      gtk_item_factory_get_item(
				  pw->pif, "/Go/Previous" ), FALSE );
    gtk_widget_set_sensitive( pw->apwNavMenu[ 2 ] =
			      gtk_item_factory_get_item(
				  pw->pif, "/Go/Up" ), FALSE );
    
    pw->pwScrolled = gtk_scrolled_window_new( NULL, NULL );
    gtk_scrolled_window_set_shadow_type(
	GTK_SCROLLED_WINDOW( pw->pwScrolled ), GTK_SHADOW_IN );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pw->pwScrolled ),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC );
    gtk_box_pack_start( GTK_BOX( pwVbox ), pw->pwScrolled, TRUE, TRUE, 0 );
    
    pw->ptb = gtk_text_buffer_new( pttt );

    pw->ptic->pf = NULL;
    pw->ptic->szFile = NULL;
    ListCreate( &pw->ptic->lNode );
    ListCreate( &pw->ptic->lRef );
    ListCreate( pw->ptic->plCurrent = &pw->ptic->lHistory );

    pw->pwText = gtk_text_view_new_with_buffer( pw->ptb );
    gtk_text_view_set_editable( GTK_TEXT_VIEW( pw->pwText ), FALSE );
    gtk_text_view_set_cursor_visible( GTK_TEXT_VIEW( pw->pwText ), FALSE );
    ptaDefault = gtk_text_view_get_default_attributes(
	GTK_TEXT_VIEW( pw->pwText ) );
    pta = pango_tab_array_new_with_positions(
	1, FALSE, PANGO_TAB_LEFT,
	pango_font_description_get_size( ptaDefault->font ) * 20 );
    gtk_text_attributes_unref( ptaDefault );
    gtk_text_view_set_tabs( GTK_TEXT_VIEW( pw->pwText ), pta );
    pango_tab_array_free( pta );
    gtk_container_add( GTK_CONTAINER( pw->pwScrolled ), pw->pwText );
}

static void gtk_texi_destroy( GtkObject *p ) {

    GtkTexi *pw = GTK_TEXI( p );
    char **pp;

    if( pw->ptb ) {
	Unload( pw );

	g_object_unref( G_OBJECT( pw->ptb ) );
	pw->ptb = NULL;
    }
    
    if( pw->pif ) {
	g_object_unref( G_OBJECT( pw->pif ) );
	pw->pif = NULL;
    }
    
    if( pw->ptic ) {
	while( ( pp = pw->ptic->lHistory.plNext->p ) ) {
	    free( pp[ 0 ] );
	    free( pp[ 1 ] );
	    free( pp );
	    ListDelete( pw->ptic->lHistory.plNext );
	}

	free( pw->ptic );
	pw->ptic = NULL;
    }
    
    GTK_OBJECT_CLASS( pcParent )->destroy( p );
}

static void gtk_texi_class_init( GtkTexiClass *pc ) {

    pcParent = gtk_type_class( GTK_TYPE_WINDOW );

    GTK_OBJECT_CLASS( pc )->destroy = gtk_texi_destroy;
}

extern GtkType gtk_texi_get_type( void ) {

    static GtkType texi_type;

    if( !texi_type ) {
	static const GtkTypeInfo texi_info = {
	    "GtkTexi",
	    sizeof( GtkTexi ),
	    sizeof( GtkTexiClass ),
	    (GtkClassInitFunc) gtk_texi_class_init,
	    (GtkObjectInitFunc) gtk_texi_init,
	    NULL, NULL, NULL
	};

	texi_type = gtk_type_unique( GTK_TYPE_WINDOW, &texi_info );
    }

    return texi_type;
}

extern GtkWidget *gtk_texi_new( void ) {

    return GTK_WIDGET( gtk_type_new( gtk_texi_get_type() ) );
}
 
/* Things to finish:

   - entities, e.g. &dots;
   - handle multitable properly
   - footnotes
   
   - label items shouldn't be indented quite as far (the start of the
     item text should be aligned with other lines of the item)
   - fix xref and uref display -- make the most of whatever tags are
     present.  Seems impossible to fix completely without patching makeinfo.
   - implement indentation (will be buggy until makeinfo handles @noindent)
   - handle anchors and references to other files
   - whitespace ignoring is sometimes performed incorrectly
*/

#endif
