/*
 * postscript.c
 *
 * by Gary Wong <gtw@gnu.org>, 2001, 2002
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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <dynarray.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "analysis.h"
#include "backgammon.h"
#include "drawboard.h"
#include "positionid.h"
#include "i18n.h"
#include "export.h"
#include "matchid.h"

typedef enum _font { 
  FONT_NONE, 
  /* Times-Roman */
  FONT_RM,               
  FONT_RM_BOLD,          
  FONT_RM_ITALIC,       
  FONT_RM_BOLD_ITALIC,  
  /* Helvetica (Sans serif) */
  FONT_SF,  
  FONT_SF_BOLD,  
  FONT_SF_OBLIQUE,  
  FONT_SF_BOLD_OBLIQUE,  
  /* Courier (type writer) */
  FONT_TT,  
  FONT_TT_BOLD,
  FONT_TT_OBLIQUE,
  FONT_TT_BOLD_OBLIQUE,
  NUM_FONTS,
} font;
static char *aszFont[ NUM_FONTS ] = { 
  NULL, 
  "rm", "rmb", "rmi", "rmbi",
  "sf", "sfb", "sfo", "sfbo",
  "tt", "ttb", "tto", "ttbo",
};

static char *aszFontName[ NUM_FONTS ] = {
  NULL,
  "Times-Roman",
  "Times-Bold",
  "Times-Italic",
  "Times-BoldItalic",
  "Helvetica",
  "Helvetica-Bold",
  "Helvetica-Oblique",
  "Helvetica-Bold-Oblique",
  "Courier",
  "Courier-Bold",
  "Courier-Oblique",
  "Courier-Bold-Oblique" 
};

/* Output settings.  FIXME there should be commands to modify these! */
static int cxPage = 595, cyPage = 842; /* A4 -- min (451,648) */
static int nMag = 100; /* Board scale (percentage) -- min 1, max 127 */

/* Current document state. */
static int iPage, y, nFont, fPDF, idPages, idResources, idLength,
    aidFont[ NUM_FONTS ];
static long lStreamStart;
static font fn;
static dynarray daXRef, daPages;

static int AllocateObject( void ) {

    assert( fPDF );
    
    return DynArrayAdd( &daXRef, (void *) -1 );
}

static void StartObject( FILE *pf, int i ) {

    assert( fPDF );

    DynArraySet( &daXRef, i, (void *) ftell( pf ) );

    fprintf( pf, "%d 0 obj\n", i );
}

static void EndObject( FILE *pf ) {

    assert( fPDF );

    fputs( "endobj\n", pf );
}

static void PostScriptEscape( FILE *pf, unsigned char *pchIn ) {

    unsigned char *pch, *sz;

    pch = sz = Convert( pchIn, "ISO-8859-1", GNUBG_CHARSET );
    
    while( *pch ) {
	switch( *pch ) {
	case '\\':
	    fputs( "\\\\", pf );
	    break;
	case '(':
	    fputs( "\\(", pf );
	    break;
	case ')':
	    fputs( "\\)", pf );
	    break;
	default:
	    if( (unsigned char) *pch >= 0x80 )
		fprintf( pf, "\\%03o", *pch );
	    else
		putc( *pch, pf );
	    break;
	}
	pch++;
    }

    free( sz );
}

static void PSStartPage( FILE *pf ) {

    iPage++;
    fn = FONT_NONE;
    y = 648;

    if( fPDF ) {
	int id, idContents;
	
	StartObject( pf, id = AllocateObject() );
	DynArrayAdd( &daPages, (void *) id );

	idContents = AllocateObject();
	fprintf( pf, "<<\n"
		 "/Type /Page\n"
		 "/Parent %d 0 R\n"
		 "/Contents %d 0 R\n"
		 "/Resources %d 0 R\n"
		 ">>\n", idPages, idContents, idResources );
	EndObject( pf );
	
	StartObject( pf, idContents );

	fprintf( pf, "<< /Length %d 0 R >>\n"
		 "stream\n", idLength = AllocateObject() );
	lStreamStart = ftell( pf );
	fprintf( pf, "1 0 0 1 %d %d cm\n", ( cxPage - 451 ) / 2,
		 ( cyPage - 648 ) / 2 );
    } else
	fprintf( pf, "%%%%Page: %d %d\n"
		 "%%%%BeginPageSetup\n"
		 "/pageobj save def %d %d translate\n"
		 "%%%%EndPageSetup\n", iPage, iPage,
		 ( cxPage - 451 ) / 2, ( cyPage - 648 ) / 2 );
}

static void PSEndPage( FILE *pf ) {

    if( fPDF ) {
	long cb;

	if( fn != FONT_NONE )
	    fputs( "ET\n", pf );
	
	cb = ftell( pf ) - lStreamStart;
	fputs( "endstream\n", pf );
	EndObject( pf );

	StartObject( pf, idLength );
	fprintf( pf, "%ld\n", cb );
	EndObject( pf );
    } else
	fputs( "pageobj restore showpage\n", pf );
}

static void RequestFont( FILE *pf, font fnNew, int nFontNew ) {

    if( fnNew != fn || nFontNew != nFont ) {
	if( fPDF && fn != FONT_NONE )
	    fputs( "ET\n", pf );
	
	fn = fnNew;
	nFont = nFontNew;

	if( fPDF )
	    fprintf( pf, "BT\n/%s %d Tf\n", aszFont[ fn ], nFont );
	else
	    fprintf( pf, "%d %s\n", nFont, aszFont[ fn ] );
    }
}

static void ReleaseFont( FILE *pf ) {

    if( fPDF && fn != FONT_NONE ) {
	fputs( "ET\n", pf );
	fn = FONT_NONE;
    }
}

static void Ensure( FILE *pf, int cy ) {

    assert( cy <= 648 );
    
    if( y < cy ) {
	PSEndPage( pf );
	PSStartPage( pf );
    }
}

static void Consume( FILE *pf, int cy ) {

    y -= cy;

    assert( y >= 0 );
}

static void Advance( FILE *pf, int cy ) {

    Ensure( pf, cy );
    Consume( pf, cy );
}

static void Skip( FILE *pf, int cy ) {

    if( y < cy ) {
	PSEndPage( pf );
	PSStartPage( pf );
    } else if( y != 648 )
	Consume( pf, cy );
}

static void PostScriptPrologue( FILE *pf, int fEPS, char *szTitle ) {

    /* FIXME change the font-setting procedures to use ISO 8859-1
       encoding */
   
    int i;
    
    static char *aszPrologue[] = {
	"%%%%Creator: (GNU Backgammon " VERSION ")\n"
	"%%%%DocumentData: Clean7Bit\n"
	"%%%%DocumentNeededResources: font Courier Helvetica Times-Roman\n"
	"%%%%DocumentSuppliedResources: procset GNU-Backgammon-Prolog 1.0 0\n"
	"%%%%LanguageLevel: 1\n"
	"%%%%Orientation: Portrait\n"
	"%%%%Pages: (atend)\n"
	"%%%%PageOrder: Ascend\n"
	"%%%%EndComments\n"
	"%%%%BeginDefaults\n"
        "%%%%PageResources: font Courier Helvetica Times-Roman\n"
	"%%%%EndDefaults\n"
	"%%%%BeginProlog\n"
	"%%%%BeginResource: procset GNU-Backgammon-Prolog 1.0 0\n"
	"\n",
	"\n"
	"/cshow { dup stringwidth pop 2 div neg 0 rmoveto show } bind def\n"
	"\n"
	"/boardsection {\n"
	"  currentpoint\n"
	"  0.5 setgray\n"
	"  3 {\n"
	"    currentpoint\n"
	"    10 100 rlineto 10 -100 rlineto closepath fill\n"
	"    moveto 40 0 rmoveto\n"
	"  } repeat\n"
	"  moveto 0 setgray\n"
	"  6 { 10 100 rlineto 10 -100 rlineto } repeat\n"
	"  closepath stroke\n"
	"} bind def\n"
	"\n"	
	"/pointlabels {\n"
	"  0 1 11 {\n"
	"    3 copy 4 3 roll dup\n"
	"    5 gt { 1 add } if %d mul %d add\n"
	"    3 1 roll\n"
	"    1 index 13 lt { 11 exch sub } if add 2 string cvs\n"
	"    3 1 roll exch\n"
	"    moveto cshow\n"
	"  } for\n"
	"  pop pop\n"
	"} bind def\n"
	"\n"
	"/board {\n"
	"  gsave\n"
	"  currentpoint translate\n"
	"  60 10 moveto 340 10 lineto 340 250 lineto 60 250 lineto closepath "
	"stroke\n"
	"  70 20 moveto boardsection 210 20 moveto boardsection\n"
	"  180 rotate\n"
	"  -190 -240 moveto boardsection -330 -240 moveto boardsection\n"
	"  -180 rotate\n"
	"  70 20 moveto 190 20 lineto 190 240 lineto 70 240 lineto closepath "
	"stroke\n"
	"  210 20 moveto 330 20 lineto 330 240 lineto 210 240 lineto "
	"closepath stroke\n"
	"  8 sf { 242 12 } { 12 242 } ifelse 1 pointlabels 13 pointlabels\n"
	"  grestore\n"
	"} bind def\n"
	"\n"
	"/wcheq {\n"
	"  newpath 2 copy 10 0 360 arc 1 setgray fill 10 0 360 arc 0 setgray "
	"stroke\n"
	"} bind def\n"
	"\n"
	"/bcheq {\n"
	"  newpath 10 0 360 arc fill\n"
	"} bind def\n"
	"\n"
	"/label {\n"
	"  gsave\n"
	"  3 1 roll newpath moveto currentpoint\n"
	"  -5 -5 rmoveto currentpoint\n"
	"  10 0 rlineto 0 10 rlineto -10 0 rlineto closepath\n"
	"  1 setgray fill\n"
	"  newpath moveto\n"
	"  10 0 rlineto 0 10 rlineto -10 0 rlineto closepath\n"
	"  0 setgray stroke\n"
	"  8 sf moveto 0 -3 rmoveto 2 string cvs cshow\n"
	"  grestore\n"
	"} bind def\n"
	"\n"
	"/cube {\n"
	"  gsave\n"
	"  exch newpath %d exch moveto currentpoint\n"
	"  -12 -12 rmoveto 24 0 rlineto 0 24 rlineto -24 0 rlineto closepath "
	"stroke\n"
	"  18 sf translate rotate 0 -6 moveto 4 string cvs cshow\n"
	"  grestore\n"
	"} bind def\n"
	"\n"
	"%%%%EndResource\n"
	"%%%%EndProlog\n"
	"%%%%BeginSetup\n"
	"%%%%IncludeResource: font Courier\n"
	"%%%%IncludeResource: font Helvetica\n"
	"%%%%IncludeResource: font Times-Roman\n"
	"%%%%EndSetup\n" };
    time_t t;
    char *sz, *pch;

    if( fPDF ) {
	fputs( "%PDF-1.3\n"
	       /* binary rubbish */
	       "%\307\361\374\r\n", pf );
	
	DynArrayCreate( &daXRef, 64, TRUE );
	DynArrayCreate( &daPages, 32, TRUE );

	AllocateObject(); /* object 0 is always free */
	
	StartObject( pf, AllocateObject() ); /* object 1 -- the root */
	fprintf( pf, "<<\n"
		 "/Type /Catalog\n"
		 "/Pages %d 0 R\n"
		 ">>\n",
		 /* FIXME add outline object */
		 idPages = AllocateObject() );
	EndObject( pf );

        /* fonts */

        for ( i = 1; i < NUM_FONTS; ++i ) {

          StartObject ( pf, aidFont[ i ] = AllocateObject() );

          fprintf ( pf,
                    "<<\n"
                    "/Type /Font\n"
                    "/Subtype /Type1\n"
                    "/Name /%s\n"
                    "/BaseFont /%s\n"
                    "/Encoding /WinAnsiEncoding\n"
                    ">>\n", aszFont[ i ], aszFontName[ i ] );
          EndObject( pf );

        }
	
	StartObject( pf, idResources = AllocateObject() );
	fprintf( pf, "<<\n"
		 "/ProcSet [/PDF /Text]\n"
		 "/Font << " );

        for ( i = 1; i < NUM_FONTS; ++i ) 
          fprintf ( pf, "/%s %d 0 R ", aszFont[ i ], aidFont[ i ] );

        fprintf ( pf, ">>\n" ">>\n" );

	/* FIXME list xobject resources */
	EndObject( pf );
    } else {
	fputs( "%!PS-Adobe-3.0\n", pf );

	if( fEPS )
	    fprintf( pf, "%%%%BoundingBox: %d %d %d %d\n",
		     ( cxPage - 451 ) / 2 + 225 - 200 * nMag / 100,
		     ( cyPage + 648 ) / 2 - 260 * nMag / 100,
		     ( cxPage - 451 ) / 2 + 225 + 200 * nMag / 100,
		     ( cyPage + 648 ) / 2 );
	else
	    fprintf( pf, "%%%%BoundingBox: %d %d %d %d\n",
		     ( cxPage - 451 ) / 2, ( cyPage - 648 ) / 2,
		     ( cxPage + 451 ) / 2, ( cyPage + 648 ) / 2 );
	
	time( &t );
	sz = ctime( &t );
	if( ( pch = strchr( sz, '\n' ) ) )
	    *pch = 0;
	fprintf( pf, "%%%%CreationDate: (%s)\n", sz );
	
	fputs( "%%Title: (", pf );
	PostScriptEscape( pf, szTitle );
	fputs( ")\n", pf );

        /* Prologue */

        fputs ( aszPrologue[ 0 ], pf );

        /* fonts */

        for ( i = 1; i < NUM_FONTS; ++i ) 
          fprintf ( pf, "/%s { /%s findfont exch scalefont setfont } "
                    "bind def\n", aszFont[ i ], aszFontName[ i ] );
	
	fprintf( pf, aszPrologue[ 1 ], fClockwise ? -20 : 20,
		 fClockwise ? 320 : 80, fClockwise ? 365 : 35 );
    }
    
    iPage = 0;
    
    PSStartPage( pf );
}

static void DrawPostScriptPoint( FILE *pf, int i, int fPlayer, int c ) {

    int j, x, y = 0;

    if( i < 6 )
	x = 320 - 20 * i;
    else if( i < 12 )
	x = 300 - 20 * i;
    else if( i < 18 )
	x = 20 * i - 160;
    else if( i < 24 )
	x = 20 * i - 140;
    else if( i == 24 ) /* bar */
	x = 200;
    else /* off */
	x = 365;

    if( fClockwise )
	x = 400 - x;
    
    for( j = 0; j < c; j++ ) {
	if( j == 5 || ( i == 24 && j == 3 ) ) {
	    if( fPDF ) {
		fprintf( pf, "1 g %d %d 10 10 re b 0 g\n", x - 5, y - 5 );
		fprintf( pf, "BT /sf 8 Tf 1 0 0 1 %d %d Tm (%d) Tj ET\n",
			 x - ( c > 9 ? 5 : 2 ), y - 3, c );
	    } else
		fprintf( pf, "%d %d %d label\n", x, y, c );
	    
	    break;
	}
	
	if( i == 24 )
	    /* bar */
	    y = fPlayer ? 60 + 20 * j : 200 - 20 * j;
	else if( fPlayer )
	    y = i < 12 || i == 25 ? 30 + 20 * j : 230 - 20 * j;
	else
	    y = i >= 12 && i != 25 ? 30 + 20 * j : 230 - 20 * j;

	if( fPDF ) {
	    fprintf( pf, "%d g ", fPlayer ? 0 : 1 );
	    fprintf( pf, "%d %d m %d %d %d %d %d %d c\n",
		     x - 10, y, x - 10, y + 5, x - 5, y + 10, x, y + 10 );
	    fprintf( pf, "%d %d %d %d %d %d c\n",
		     x + 5, y + 10, x + 10, y + 5, x + 10, y );
	    fprintf( pf, "%d %d %d %d %d %d c\n",
		     x + 10, y - 5, x + 5, y - 10, x, y - 10 );
	    fprintf( pf, "%d %d %d %d %d %d c %c\n",
		     x - 5, y - 10, x - 10, y - 5, x - 10, y,
		     fPlayer ? 'f' : 'b' );
	} else
	    fprintf( pf, "%d %d %ccheq\n", x, y, fPlayer ? 'b' : 'w' );
    }
}

static void
PostScriptPositionID ( FILE *pf, matchstate *pms ) {

  int anBoard[ 2 ][ 25 ];

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  if ( ! pms->fMove )
    SwapSides ( anBoard );

  RequestFont ( pf, FONT_RM, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (" : "%d %d moveto (", 
           285 - 200 * nMag / 100, y );
  fputs ( _("Position ID:"), pf );
  fputc ( ' ', pf );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

  RequestFont ( pf, FONT_TT, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (" : "%d %d moveto (", 
           285 - 200 * nMag / 100 + 55, y );
  fputs ( PositionID ( anBoard ), pf );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

  RequestFont ( pf, FONT_RM, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (" : "%d %d moveto (", 
           285 - 200 * nMag / 100 + 160, y );
  fputs ( _("Match ID:"), pf );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

  RequestFont ( pf, FONT_TT, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (" : "%d %d moveto (", 
           285 - 200 * nMag / 100 + 205, y );
  fputs ( MatchIDFromMatchState ( pms ), pf );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

}

static void
PostScriptPipCounts ( FILE *pf, int anBoard[ 2 ][ 25 ], int fMove ) {

  int an[ 2 ][ 25 ];
  int anPips[ 2 ];

  memcpy ( an, anBoard, sizeof ( an ) );

  if ( ! fMove )
    SwapSides ( an );

  PipCount ( an, anPips );

  Advance ( pf, 10 );
  RequestFont ( pf, FONT_RM, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (" : "%d %d moveto (", 
           285 - 200 * nMag / 100, y );
  fprintf ( pf, _("Pip counts: %s %d, %s %d"), 
            ap[ 0 ].szName, anPips[ 0 ], ap[ 1 ].szName, anPips[ 1 ] );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

}

static void PrintPostScriptBoard( FILE *pf, matchstate *pms, int fPlayer ) {

    int yCube, theta, cos, sin, anOff[ 2 ] = { 15, 15 }, i;

    if( pms->fCubeOwner < 0 ) {
	yCube = 130;
	theta = fClockwise ? 270 : 90;
	sin = fClockwise ? -1 : 1;
	cos = 0;
    } else if( pms->fCubeOwner ) {
	yCube = 30;
	theta = 0;
	sin = 0;
	cos = 1;
    } else {
	yCube = 230;
	theta = 180;
	sin = 0;
	cos = -1;
    }

    Advance( pf, 260 * nMag / 100 );
    ReleaseFont( pf );
    
    if( fPDF ) {
	/* FIXME most of the following could be encapsulated into a PDF
	   XObject to optimise the output file */
	lifprintf( pf, "q 1 0 0 1 %d %d cm %.2f 0 0 %.2f 0 0 cm 0.5 g\n",
		 225 - 200 * nMag / 100, y, nMag / 100.0, nMag / 100.0 );
	fputs( "60 10 280 240 re S\n", pf );
	for( i = 0; i < 6; i++ ) {
	    fprintf( pf, "%d %d m %d %d l %d %d l %c\n",
		     70 + i * 20, 20, 80 + i * 20, 120, 90 + i * 20, 20,
		     i & 1 ? 's' : 'b' );
	    fprintf( pf, "%d %d m %d %d l %d %d l %c\n",
		     210 + i * 20, 20, 220 + i * 20, 120, 230 + i * 20, 20,
		     i & 1 ? 's' : 'b' );
	    fprintf( pf, "%d %d m %d %d l %d %d l %c\n",
		     70 + i * 20, 240, 80 + i * 20, 140, 90 + i * 20, 240,
		     i & 1 ? 'b' : 's' );
	    fprintf( pf, "%d %d m %d %d l %d %d l %c\n",
		     210 + i * 20, 240, 220 + i * 20, 140, 230 + i * 20, 240,
		     i & 1 ? 'b' : 's' );
	}
	fputs( "70 20 120 220 re 210 20 120 220 re S 0 g\n", pf );
	
	for( i = 1; i <= 12; i++ )
	    fprintf( pf, "BT /sf 8 Tf 1 0 0 1 %d %d Tm (%d) Tj ET\n",
		     340 - i * 20 - ( i > 6 ) * 20 - ( i > 9 ? 5 : 2 ),
		     fPlayer ? 12 : 242, fClockwise ? 13 - i : i );

	for( i = 13; i <= 24; i++ )
	    fprintf( pf, "BT /sf 8 Tf 1 0 0 1 %d %d Tm (%d) Tj ET\n",
		     i * 20 - 185 + ( i > 18 ) * 20, fPlayer ? 242 : 12,
		     fClockwise ? 37 - i : i );

	fprintf( pf, "q %d %d %d %d %d %d cm -12 -12 24 24 re S\n"
		 "BT /sf 18 Tf 1 0 0 1 %d -6 Tm (%d) Tj ET Q\n",
		 cos, sin, -sin, cos,
		 fClockwise ? 365 : 35, yCube,
		 pms->nCube == 1 || pms->nCube > 8 ? -10 : -5,
		 pms->nCube == 1 ? 64 : pms->nCube );
    } else
	lifprintf( pf, "gsave\n"
		   "%d %d translate %.2f %.2f scale\n"
		   "0 0 moveto %s board\n"
		   "%d %d %d cube\n", 225 - 200 * nMag / 100, y,
		   nMag / 100.0, nMag / 100.0, fPlayer ? "true" : "false",
		   pms->nCube == 1 ? 64 : pms->nCube, yCube, theta );

    for( i = 0; i < 25; i++ ) {
	anOff[ 0 ] -= pms->anBoard[ 0 ][ i ];
	anOff[ 1 ] -= pms->anBoard[ 1 ][ i ];

	DrawPostScriptPoint( pf, i, 0, pms->anBoard[ !fPlayer ][ i ] );
	DrawPostScriptPoint( pf, i, 1, pms->anBoard[ fPlayer ][ i ] );
    }

    DrawPostScriptPoint( pf, 25, 0, anOff[ !fPlayer ] );
    DrawPostScriptPoint( pf, 25, 1, anOff[ fPlayer ] );    

    if( fPDF )
	fputs( "Q\n", pf );
    else
	fputs( "grestore\n", pf );

    /* FIXME: write position ID, match ID, and pip counts */

    PostScriptPositionID ( pf, pms );
    PostScriptPipCounts ( pf, pms->anBoard, pms->fMove );

}

static short acxTimesRoman[ 256 ] = {
    /* Width of Times-Roman font in ISO 8859-1 encoding. */
    250, 250, 250, 250, 250, 250, 250, 250, /*   0 to   7 */
    250, 250, 250, 250, 250, 250, 250, 250, /*   8 to  15 */
    250, 250, 250, 250, 250, 250, 250, 250, /*  16 to  23 */
    250, 250, 250, 250, 250, 250, 250, 250, /*  24 to  31 */
    250, 333, 408, 500, 500, 833, 778, 333, /*  32 to  39 */
    333, 333, 500, 564, 250, 333, 250, 278, /*  40 to  47 */
    500, 500, 500, 500, 500, 500, 500, 500, /*  48 to  55 */
    500, 500, 278, 278, 564, 564, 564, 444, /*  56 to  63 */
    921, 722, 667, 667, 722, 611, 556, 722, /*  64 to  71 */
    722, 333, 389, 722, 611, 889, 722, 722, /*  72 to  79 */
    556, 722, 667, 556, 611, 722, 722, 944, /*  80 to  87 */
    722, 722, 611, 333, 278, 333, 469, 500, /*  88 to  95 */
    333, 444, 500, 444, 500, 444, 333, 500, /*  96 to 103 */
    500, 278, 278, 500, 278, 778, 500, 500, /* 104 to 111 */
    500, 500, 333, 389, 278, 500, 500, 722, /* 112 to 119 */
    500, 500, 444, 480, 200, 480, 541, 250, /* 120 to 127 */
    250, 250, 250, 250, 250, 250, 250, 250, /* 128 to 135 */
    250, 250, 250, 250, 250, 250, 250, 250, /* 136 to 143 */
    250, 250, 250, 250, 250, 250, 250, 250, /* 144 to 151 */
    250, 250, 250, 250, 250, 250, 250, 250, /* 152 to 159 */
    250, 333, 500, 500, 500, 500, 200, 500, /* 160 to 167 */
    333, 760, 276, 500, 564, 333, 760, 333, /* 168 to 175 */
    400, 564, 300, 300, 333, 500, 453, 250, /* 176 to 183 */
    333, 300, 310, 500, 750, 750, 750, 444, /* 184 to 191 */
    722, 722, 722, 722, 722, 722, 889, 667, /* 192 to 199 */
    611, 611, 611, 611, 333, 333, 333, 333, /* 200 to 207 */
    722, 722, 722, 722, 722, 722, 722, 564, /* 208 to 215 */
    722, 722, 722, 722, 722, 722, 556, 500, /* 216 to 223 */
    444, 444, 444, 444, 444, 444, 667, 444, /* 224 to 231 */
    444, 444, 444, 444, 278, 278, 278, 278, /* 232 to 239 */
    500, 500, 500, 500, 500, 500, 500, 564, /* 240 to 247 */
    500, 500, 500, 500, 500, 500, 500, 500  /* 248 to 255 */
};

static int StringWidth( unsigned char *sz ) {

    unsigned char *pch;
    int c;
    
    switch( fn ) {
    case FONT_RM:
	for( c = 0, pch = sz; *pch; pch++ )
	    if( isprint( *pch ) )
		c += acxTimesRoman[ *pch ];
	break;
	
    case FONT_TT:
	for( c = 0, pch = sz; *pch; pch++ )
	    if( isprint( *pch ) )
		c += 600; /* fixed Courier character width */
	break;
	
    default:
	abort();
    }

    return c * nFont / 1000;
}

/* Typeset a line of text.  We're not TeX, so we don't do any kerning,
   hyphenation or justification.  Word wrapping will have to do. */

static void PrintPostScriptLineWithSkip( FILE *pf, unsigned char *pch, 
                                         const int nSkip,
                                         font fn, int nSize ) {

    int x;
    unsigned char *sz, *pchStart, *pchBreak;
    
    if( !pch || !*pch )
	return;

    pch = sz = Convert( pch, "ISO-8859-1", GNUBG_CHARSET );
    
    if ( nSkip )
      Skip( pf, nSkip );

    while( *pch ) {
	Advance( pf, 10 );
	
	pchBreak = NULL;
	pchStart = pch;
	
	for( x = 0; x < 451 * 1000 / nSize; x += acxTimesRoman[ *pch++ ] )
	    if( !*pch ) {
		/* finished; break here */
		pchBreak = pch;
		break;
	    } else if( *pch == '\n' ) {
		/* new line; break immediately */
		pchBreak = pch++;
		break;
	    } else if( *pch == ' ' )
		/* space; note candidate for breaking */
		pchBreak = pch;

	if( !pchBreak )
	    pchBreak = ( pch == pchStart ) ? pch : pch - 1;

	RequestFont( pf, fn, nSize );

	fprintf( pf, fPDF ? "1 0 0 1 0 %d Tm (" : "0 %d moveto (", y );
	
	for( ; pchStart <= pchBreak; pchStart++ )
	    switch( *pchStart ) {
	    case 0:
	    case '\n':
		break;
	    case '\\':
	    case '(':
	    case ')':
		putc( '\\', pf );
		/* fall through */
	    default:
		if( (unsigned char) *pchStart >= 0x80 )
		    fprintf( pf, "\\%03o", *pchStart );
		else
		    putc( *pchStart, pf );
	    }
	fputs( fPDF ? ") Tj\n" : ") show\n", pf );

	if( !*pch )
	    break;
	
	pch = pchBreak + 1;
    }

    free( sz );
}

static void
PrintPostScriptLine ( FILE *pf, unsigned char *pch ) {

  PrintPostScriptLineWithSkip ( pf, pch, 0, FONT_RM, 10 );

}

static void
PrintPostScriptLineFont ( FILE *pf, unsigned char *pch, 
                          font fn, int nSize ) {

  PrintPostScriptLineWithSkip ( pf, pch, 0, fn, nSize );

}



static void
PrintPostScriptComment ( FILE *pf, unsigned char *pch ) {

  PrintPostScriptLineWithSkip ( pf, pch, 6, FONT_RM, 10 );

}



static void 
PrintPostScriptCubeAnalysis( FILE *pf, matchstate *pms,
                             int fPlayer, float arDouble[ 4 ],
                             float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                             evalsetup *pes ) { 
    cubeinfo ci;
    char sz[ 1024 ], *pch, *pchNext;

    if( pes->et == EVAL_NONE )
	return;
    
    SetCubeInfo( &ci, pms->nCube, pms->fCubeOwner, fPlayer, pms->nMatchTo,
		 pms->anScore, pms->fCrawford, pms->fJacoby, nBeavers,
                 pms->bgv );
    
    if( !GetDPEq( NULL, NULL, &ci ) )
	/* No cube action possible */
	return;
    
    GetCubeActionSz( arDouble, aarOutput, sz, &ci, fOutputMWC, FALSE );

    Skip( pf, 4 );
    for( pch = sz; pch && *pch; pch = pchNext ) {
	Advance( pf, 8 );
	RequestFont( pf, FONT_TT, 8 );
	fprintf( pf, fPDF ? "1 0 0 1 144 %d Tm (" : "144 %d moveto (", y );
	if( ( pchNext = strchr( pch, '\n' ) ) )
	    *pchNext++ = 0;
	fputs( pch, pf );
	fputs( fPDF ? ") Tj\n" : ") show\n", pf );
    }
    Skip( pf, 6 );
}

static void PlayerSymbol( FILE *pf, int x, int fPlayer ) {

    if( fPDF ) {
	ReleaseFont( pf );
	
	fprintf( pf, "%d %d m %d %d %d %d %d %d c\n",
		 x - 4, y + 4, x - 4, y + 6, x - 2, y + 8, x, y + 8 );
	fprintf( pf, "%d %d %d %d %d %d c\n",
		 x + 2, y + 8, x + 4, y + 6, x + 4, y + 4 );
	fprintf( pf, "%d %d %d %d %d %d c\n",
		 x + 4, y + 2, x + 2, y, x, y );
	fprintf( pf, "%d %d %d %d %d %d c %c\n",
		 x - 2, y, x - 4, y + 2, x - 4, y + 4,
		 fPlayer ? 'f' : 's' );
    } else
	fprintf( pf, "newpath %d %d 4 0 360 arc %s\n", x, y + 4,
		 fPlayer ? "fill" : "stroke" );
}


static void
PostScriptMatchInfo ( FILE *pf, matchinfo *pmi ) {

  char sz[ 1000 ];
  char szx[ 1000 ];
  int i;
  struct tm tmx;
  
  Ensure ( pf, 100 );
  Advance ( pf, 14 );
  PrintPostScriptLineFont ( pf, _("Match Information" ), FONT_RM_BOLD, 14 );
  Advance ( pf, 6 );

  /* ratings */

  RequestFont ( pf, FONT_RM, 10 );
  for ( i = 0; i < 2; ++i ) {
    sprintf ( sz, _("%s's rating: %s"),
              ap[ i ].szName,
              pmi->pchRating[ i ] ? pmi->pchRating[ i ] : _("n/a") );
    PrintPostScriptLine ( pf, sz );
  }

  /* date */

  if ( pmi->nYear ) {

    tmx.tm_year = pmi->nYear - 1900;
    tmx.tm_mon = pmi->nMonth - 1;
    tmx.tm_mday = pmi->nDay;
    strftime ( szx, sizeof ( szx ), _("%B %d, %Y"), &tmx );
    sprintf ( sz, _("Date: %s"), szx ); 
    PrintPostScriptLine ( pf, sz );
  }
  else 
    PrintPostScriptLine ( pf, _("Date: n/a") );

  /* event, round, place and annotator */

  sprintf( sz, _("Event: %s"),
           pmi->pchEvent ? pmi->pchEvent : _("n/a") );
  PrintPostScriptLine ( pf, sz );

  sprintf( sz, _("Round: %s"),
           pmi->pchRound ? pmi->pchRound : _("n/a") );
  PrintPostScriptLine ( pf, sz );

  sprintf( sz, _("Place: %s"),
           pmi->pchPlace ? pmi->pchPlace : _("n/a") );
  PrintPostScriptLine ( pf, sz );

  sprintf( sz, _("Annotator: %s"),
           pmi->pchAnnotator ? pmi->pchAnnotator : _("n/a") );
  PrintPostScriptLine ( pf, sz );

  sprintf( sz, _("Comments: %s"),
           pmi->pchComment ? pmi->pchComment : _("n/a") );
  PrintPostScriptLine ( pf, sz );
    
  ReleaseFont ( pf );

}

static void
PostScriptBoardHeader ( FILE *pf, matchstate *pms, const int iMove ) {

  /* move number */

  Advance ( pf, 20 );
  RequestFont ( pf, FONT_RM_BOLD, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 0 %d Tm (" : "0 %d moveto (", y );
  fprintf ( pf,_("Move number %d:"), iMove + 1 );
  fputs( fPDF ? ") Tj\n" : ") show\n", pf );

  RequestFont ( pf, FONT_RM, 10 );
  fprintf( pf, fPDF ? "1 0 0 1 100 %d Tm (" : "100 %d moveto (", y );

  if ( pms->fResigned ) 
    
    /* resignation */

    fprintf ( pf,
              _("%s resigns %d points"), 
              ap[ pms->fTurn ].szName,
              pms->fResigned * pms->nCube
            );
  
  else if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] )

    /* chequer play decision */

    fprintf ( pf,
              _("%s to play %d%d"),
              ap[ pms->fTurn ].szName,
              pms->anDice[ 0 ], pms->anDice[ 1 ] 
            );

  else if ( pms->fDoubled )

    /* take decision */

    fprintf ( pf,
              _("%s doubles to %d"),
              ap[ pms->fMove ].szName,
              pms->nCube * 2
            );

  else

    /* cube decision */

    fprintf ( pf,
              _("%s on roll, cube decision?"),
              ap[ pms->fTurn ].szName
            );

  fputs( fPDF ? ") Tj\n" : ") show\n", pf );
  

}

static void ExportGamePostScript( FILE *pf, list *plGame ) {

    list *pl;
    moverecord *pmr;
    matchstate msExport;
    int fTook = FALSE, i, cx;
    char sz[ 1024 ], *pch;
    int iMove = 0;

    updateStatisticsGame ( plGame );

    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
        FixMatchState ( &msExport, pmr );
	switch( pmr->mt ) {
	case MOVE_GAMEINFO:

            ApplyMoveRecord ( &msExport, plGame, pmr );
          
	    Ensure( pf, 26 );
	    Consume( pf, 14 );
	    RequestFont( pf, FONT_RM, 14 );
	    if( pmr->g.nMatch )
		sprintf( sz, _("%d point match (game %d)"), pmr->g.nMatch,
			 pmr->g.i + 1 );
	    else
		sprintf( sz, _("Money session (game %d)"), pmr->g.i + 1 );
	    
	    fprintf( pf, fPDF ? "1 0 0 1 0 %d Tm (%s) Tj\n" :
		     "0 %d moveto (%s) show\n", y, sz );

	    Consume( pf, 12 );
	    PlayerSymbol( pf, 8, 0 );
	    RequestFont( pf, FONT_RM, 12 );
	    fprintf( pf, fPDF ? "1 0 0 1 16 %d Tm (" : "16 %d moveto (",
		     y );
	    PostScriptEscape( pf, ap[ 0 ].szName );
	    fprintf( pf, _(" (%d points)"), pmr->g.anScore[ 0 ] );
	    fputs( fPDF ? ") Tj\n" : ") show\n", pf );
	    
	    PlayerSymbol( pf, 225, 1 );
	    RequestFont( pf, FONT_RM, 12 );
	    fprintf( pf, fPDF ? "1 0 0 1 233 %d Tm (" : "233 %d moveto (",
		     y );
	    PostScriptEscape( pf, ap[ 1 ].szName );
	    fprintf( pf, _(" (%d points)"), pmr->g.anScore[ 1 ] );
	    fputs( fPDF ? ") Tj\n" : ") show\n", pf );

            /* match information */

            if ( exsExport.fIncludeMatchInfo )
              PostScriptMatchInfo ( pf, &mi );
	    
	    break;
	    
	case MOVE_NORMAL:

	    msExport.fTurn = msExport.fMove = pmr->n.fPlayer;
            msExport.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
            msExport.anDice[ 1 ] = pmr->n.anRoll[ 1 ];

	    if( fTook )
		/* no need to print board following a double/take */
		fTook = FALSE;
	    else {
		Ensure( pf, 260 * nMag / 100 + 30 );
		PostScriptBoardHeader ( pf, &msExport, iMove );
		PrintPostScriptBoard( pf, &msExport, pmr->n.fPlayer );
	    }
	    
	    PrintPostScriptCubeAnalysis( pf, &msExport, pmr->n.fPlayer,
					 pmr->n.arDouble, 
                                         pmr->n.aarOutput, 
                                         &pmr->n.esDouble );
	    
	    Advance( pf, 10 );
	    FormatMove( sz, msExport.anBoard, pmr->n.anMove );
	    RequestFont( pf, FONT_RM, 10 );
	    cx = 15 /* 2 digits + colon + space */ +
		StringWidth( aszLuckTypeAbbr[ pmr->n.lt ] ) +
		StringWidth( aszSkillTypeAbbr[ pmr->n.stMove ] ) +
		StringWidth( sz );
	    fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (%d%d%s: %s%s) Tj\n" :
		     "%d %d moveto (%d%d%s: %s%s) show\n",
		     225 - cx / 2 + 6, y,
		     pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ],
		     aszLuckTypeAbbr[ pmr->n.lt ], sz,
		     aszSkillTypeAbbr[ pmr->n.stMove ] );
	    PlayerSymbol( pf, 225 - cx / 2 - 2, pmr->n.fPlayer );
            /* FIXME: output cube skill as well */

	    if( pmr->n.ml.cMoves ) {
		Skip( pf, 4 );
		for( i = 0; i < pmr->n.ml.cMoves && i <= pmr->n.iMove; i++ ) {
		    if( i >= 5 /* FIXME allow user to choose limit */ &&
			i != pmr->n.iMove )
			continue;

		    Ensure( pf, 16 );
		    Consume( pf, 8 );
		    RequestFont( pf, FONT_TT, 8 );
		    fprintf( pf, fPDF ? "1 0 0 1 46 %d Tm (" :
			     "46 %d moveto (", y );
		    putc( i == pmr->n.iMove ? '*' : ' ', pf );
		    FormatMoveHint( sz, &msExport, &pmr->n.ml, i,
				    i != pmr->n.iMove ||
				    i != pmr->n.ml.cMoves - 1, TRUE, TRUE );
		    pch = strchr( sz, '\n' );
		    *pch++ = 0;
		    fputs( sz, pf );
		    fputs( fPDF ? ") Tj\n" : ") show\n", pf );
		    
		    Consume( pf, 8 );
		    fprintf( pf, fPDF ? "1 0 0 1 46 %d Tm (" :
			     "46 %d moveto (", y );
		    putc( ' ', pf );
		    *strchr( pch, '\n' ) = 0;
		    fputs( pch, pf );
		    fputs( fPDF ? ") Tj\n" : ") show\n", pf );
		}
	    }

	    PrintPostScriptComment( pf, pmr->a.sz );

	    Skip( pf, 6 );

            ++iMove;
	    
	    break;
	    
	case MOVE_DOUBLE:	    
	    Ensure( pf, 260 * nMag / 100 );
	    PrintPostScriptBoard( pf, &msExport, pmr->d.fPlayer );

	    PrintPostScriptCubeAnalysis( pf, &msExport, pmr->d.fPlayer,
					 pmr->d.arDouble, 
                                         pmr->d.aarOutput, &pmr->d.esDouble );

	    Advance( pf, 10 );
	    RequestFont( pf, FONT_RM, 10 );
	    /* (Double) stringwidth = 29.439 */
	    cx = StringWidth( aszSkillTypeAbbr[ pmr->d.st ] ) + 29;
	    fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (%s%s) Tj\n" :
		     "%d %d moveto (%s%s) show\n", 225 - cx / 2 + 6, y,
                     _("Double"),
		     aszSkillTypeAbbr[ pmr->d.st ] );
	    
	    PlayerSymbol( pf, 225 - cx / 2 - 2, pmr->n.fPlayer );

	    PrintPostScriptComment( pf, pmr->a.sz );

	    Skip( pf, 6 );

            ++iMove;
	    
	    break;
	    
	case MOVE_TAKE:
	    fTook = TRUE;

	    Advance( pf, 10 );
	    RequestFont( pf, FONT_RM, 10 );
	    /* (Take) stringwidth = 19.9892 */
	    cx = StringWidth( aszSkillTypeAbbr[ pmr->d.st ] ) + 20;
	    fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (%s%s) Tj\n" :
		     "%d %d moveto (%s%s) show\n", 225 - cx / 2 + 6, y,
                     _("Take"),
		     aszSkillTypeAbbr[ pmr->d.st ] );
	    
	    PlayerSymbol( pf, 225 - cx / 2 - 2, pmr->n.fPlayer );

	    PrintPostScriptComment( pf, pmr->a.sz );

	    Skip( pf, 6 );

            ++iMove;
	    
	    break;
	    
	case MOVE_DROP:
	    Advance( pf, 10 );
	    RequestFont( pf, FONT_RM, 10 );
	    /* (Drop) stringwidth = 20.5494 */
	    cx = StringWidth( aszSkillTypeAbbr[ pmr->d.st ] ) + 20;
	    fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (%s%s) Tj\n" :
		     "%d %d moveto (%s%s) show\n", 225 - cx / 2 + 6, y,
                     _("Drop"),
		     aszSkillTypeAbbr[ pmr->d.st ] );
	    
	    PlayerSymbol( pf, 225 - cx / 2 - 2, pmr->n.fPlayer );
	    
	    PrintPostScriptComment( pf, pmr->a.sz );

	    Skip( pf, 6 );

            ++iMove;

	    break;
	    
	case MOVE_RESIGN:
	    Advance( pf, 10 );
	    RequestFont( pf, FONT_RM, 10 );
	    /* (Resigns ) stringwidth = 34.1685 */
	    cx = StringWidth( gettext ( aszGameResult[ pmr->r.nResigned - 1 ]  ) ) + 34;
	    fprintf( pf, fPDF ? "1 0 0 1 %d %d Tm (%s %s) Tj\n" :
		     "%d %d moveto (%s %s) show\n", 225 - cx / 2 + 6, y,
                     _("Resigns"),
		     gettext ( aszGameResult[ pmr->r.nResigned - 1 ] ) );
	    
	    PlayerSymbol( pf, 225 - cx / 2 - 2, pmr->n.fPlayer );
	    
	    /* FIXME print resignation analysis, if available */
	    
	    PrintPostScriptComment( pf, pmr->a.sz );

	    Skip( pf, 6 );

            ++iMove;

	    break;
	    	    
	case MOVE_SETDICE:
	    /* ignore */
	    break;
	    
	case MOVE_SETBOARD:
	case MOVE_SETCUBEVAL:
	case MOVE_SETCUBEPOS:
	    /* FIXME print something? */
	    break;
	}

	ApplyMoveRecord( &msExport, plGame, pmr );
    }
    
    if( ( GameStatus( msExport.anBoard, msExport.bgv ) ) )
	/* FIXME print game result and statistics */
	;
    
}

static void PostScriptEpilogue( FILE *pf ) {

    int i;
    long lXRef;
    
    PSEndPage( pf );

    if( fPDF ) {
	StartObject( pf, idPages );
	fputs( "<<\n"
	       "/Type /Pages\n"
	       "/Kids [\n", pf );
	for( i = 0; i < daPages.c; i++ )
	    fprintf( pf, " %d 0 R\n", (int) daPages.ap[ i ] );
	fprintf( pf, "]\n"
		 "/Count %d\n"
		 "/MediaBox [0 0 %d %d]\n"
		 ">>\n", daPages.c, cxPage, cyPage );
	EndObject( pf );
	
	lXRef = ftell( pf );
	
	fprintf( pf, "xref\n"
		 "0 %d\n", daXRef.iFinish );
	
	fputs( "0000000000 65535 f \n", pf );
	
	for( i = 1; i < daXRef.iFinish; i++ ) {
	    assert( daXRef.ap[ i ] );
	    
	    fprintf( pf, "%010ld 00000 n \n", (long) daXRef.ap[ i ] );
	}

	fprintf( pf, "trailer\n"
		 "<< /Size %d /Root 1 0 R >>\n"
		 "startxref\n"
		 "%ld\n"
		 "%%%%EOF\n", daXRef.iFinish, lXRef );
    } else
	fprintf( pf, "%%%%Trailer\n"
		 "%%%%Pages: %d\n"
		 "%%%%EOF\n", iPage );
}

static void ExportGameGeneral( int f, char *sz ) {

    FILE *pf;
    char szTitle[ 128 ];

    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputf( _("You must specify a file to export to (see `help export "
		 "game %s').\n"), f ? "pdf" : "postscript" );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) ) {
	if( f ) {
	    outputl( _("PDF files may not be written to standard output ("
		     "see `help export game pdf').") );
	    return;
	}
	
	pf = stdout;
    } else if( !( pf = fopen( sz, f ? "wb" : "w" ) ) ) {
	outputerr( sz );
	return;
    }

    fPDF = f;
    sprintf( szTitle, "%s vs. %s", ap[ 0 ].szName, ap[ 1 ].szName );
    PostScriptPrologue( pf, FALSE, szTitle );
    
    ExportGamePostScript( pf, plGame );

    PostScriptEpilogue( pf );
    
    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, f ? PATH_PDF : PATH_POSTSCRIPT );

}

extern void CommandExportGamePDF( char *sz ) {

    ExportGameGeneral( TRUE, sz );
}

extern void CommandExportGamePostScript( char *sz ) {

    ExportGameGeneral( FALSE, sz );
}

static void ExportMatchGeneral( int f, char *sz ) {
    
    FILE *pf;
    list *pl;
    char szTitle[ 128 ];
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputf( _("You must specify a file to export to (see `help export "
		 "match %s').\n"), f ? "pdf" : "postscript" );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) ) {
	if( f ) {
	    outputl( _("PDF files may not be written to standard output ("
		     "see `help export match pdf').") );
	    return;
	}
	
	pf = stdout;
    } else if( !( pf = fopen( sz, f ? "wb" : "w" ) ) ) {
	outputerr( sz );
	return;
    }

    fPDF = f;
    sprintf( szTitle, "%s vs. %s", ap[ 0 ].szName, ap[ 1 ].szName );
    PostScriptPrologue( pf, FALSE, szTitle );

    /* FIXME write match introduction? */
    
    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
	ExportGamePostScript( pf, pl->p );
    
    PostScriptEpilogue( pf );
    
    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, f ? PATH_PDF : PATH_POSTSCRIPT );

}

extern void CommandExportMatchPDF( char *sz ) {

    ExportMatchGeneral( TRUE, sz );
}

extern void CommandExportMatchPostScript( char *sz ) {

    ExportMatchGeneral( FALSE, sz );
}

extern void CommandExportPositionEPS( char *sz ) {

    FILE *pf;
    char szTitle[ 32 ];
	
    sz = NextToken( &sz );
    
    if( ms.gs == GAME_NONE ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export "
		 "position eps').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	outputerr( sz );
	return;
    }

    fPDF = FALSE;
    sprintf( szTitle, _("Position %s"), PositionID( ms.anBoard ) );
    PostScriptPrologue( pf, TRUE, szTitle );

    PrintPostScriptBoard( pf, &ms, ms.fTurn );
    
    PostScriptEpilogue( pf );

    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_EPS );

}
