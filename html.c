/*
 * html.c
 *
 * by Joern Thyssen  <jthyssen@dk.ibm.com>, 2002
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

#include <stdio.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "export.h"


/*
 * Print img tag.
 *
 * Input:
 *    pf : write to file
 *    szImageDir: path (URI) to images
 *    szImage: the image to print
 *    szExtension: extension of the image (e.g. gif or png)
 *
 */

static void
printImage ( FILE *pf, const char *szImageDir, const char *szImage,
             const char *szExtension ) {

  fprintf ( pf, "<img src=\"%s%s%s.%s\">",
            ( szImageDir ) ? szImageDir : "",
            ( ! szImageDir || szImageDir[ strlen ( szImageDir ) - 1 ] == '/' ) ? "" : "/",
            szImage, szExtension );

}

/*
 * print image for a point
 *
 * Input:
 *    pf : write to file
 *    szImageDir: path (URI) to images
 *    szImage: the image to print
 *    szExtension: extension of the image (e.g. gif or png)
 *    anBoard: the board
 *    iPoint: the point to draw
 *    fColor: color
 *    fUp: upper half or lower half of board
 *
 */

static void
printPoint ( FILE *pf, const char *szImageDir, const char *szExtension,
             int iPoint0, int iPoint1, 
             const int fColor, const int fUp ) {

  char sz[ 100 ];

  if ( iPoint0 )

    /* player 0 owns the point */

    sprintf ( sz, "b-%c%c-o%d", fColor ? 'g' : 'y', fUp ? 'd' : 'u',
              ( iPoint0 >= 11 ) ? 11 : iPoint0 );
    
  else if ( iPoint1 )

    /* player 1 owns the point */

    sprintf ( sz, "b-%c%c-x%d", fColor ? 'g' : 'y', fUp ? 'd' : 'u',
              ( iPoint1 >= 11 ) ?
              11 : iPoint1 );

  else
    /* empty point */
    sprintf ( sz, "b-%c%c", fColor ? 'g' : 'y', fUp ? 'd' : 'u' );

  printImage ( pf, szImageDir, sz, szExtension );

  

}


static void
printHTMLBoard ( FILE *pf, matchstate *pms, int fTurn,
                 const char *szImageDir, const char *szExtension ) {

  char sz[ 100 ];
  int fColor;
  int i;

  char *aszDieO[] = { "b-odie1", "b-odie2", "b-oide3",
                     "b-odie4", "b-odie5", "b-oide6" };
  char *aszDieX[] = { "b-xdie1", "b-xdie2", "b-xide3",
                     "b-xdie4", "b-xdie5", "b-xide6" };

  int anBoard[ 2 ][ 25 ];
  int anPips[ 2 ];

  memcpy ( anBoard, ms.anBoard, sizeof ( anBoard ) );

  if ( pms->fMove ) SwapSides ( anBoard );

  /* top line with board numbers */

  fprintf ( pf, "<p>\n" );
  printImage ( pf, szImageDir, "b-indent", szExtension );
  printImage ( pf, szImageDir, fTurn ? "b-hitop" : "b-lotop", szExtension );
  fprintf ( pf, "<br>\n" );

  /* cube image */

  if ( pms->fDoubled ) {

    /* questionmark with cube (if player 0 is doubled) */

    if ( ! pms->fTurn ) {
      sprintf ( sz, "b-dup%d", 2 * pms->nCube );
      printImage ( pf, szImageDir, sz, szExtension );

    }
    else 
      printImage (pf, szImageDir, "b-indent", szExtension );

  }
  else {
    
    /* display arrow */

    if ( pms->fCubeOwner == -1 || pms->fCubeOwner )
      printImage (pf, szImageDir, fTurn ? "b-topdn" : "b-topup",
                  szExtension );
    else {
      sprintf ( sz, "%s%d", fTurn ? "b-tdn" : "b-tup", pms->nCube );
      printImage (pf, szImageDir, sz, szExtension );
    }

  }

  /* display left border */

  printImage ( pf, szImageDir, "b-left", szExtension );

  /* display player 0's outer quadrant */

  fColor = 1;

  for ( i = 0; i < 6; i++ ) {

      printPoint ( pf, szImageDir, szExtension, 
                   anBoard[ 1 ][ 11 - i ],
                   anBoard[ 0 ][ 12 + i],
                   fColor, TRUE );

    fColor = ! fColor;

  }

  /* display bar */

  if ( anBoard[ 1 ][ 24 ] ) {

    sprintf ( sz, "b-bar-x%d", 
              ( anBoard[ 1 ][ 24 ] > 4 ) ?
              4 : anBoard[ 1 ][ 24 ] );
    printImage ( pf, szImageDir, sz, szExtension );

  }
  else 
    printImage ( pf, szImageDir, "b-bar", szExtension );

  /* display player 0's home quadrant */

  fColor = 1;

  for ( i = 0; i < 6; i++ ) {

      printPoint ( pf, szImageDir, szExtension, 
                   anBoard[ 1 ][ 5 - i ],
                   anBoard[ 0 ][ 18 + i],
                   fColor, TRUE );

    fColor = ! fColor;

  }

  /* right border */

  printImage ( pf, szImageDir, "b-right", szExtension );

  fprintf ( pf, "<br>\n" );

  /* center of board */

  if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] ) {

    printImage ( pf, szImageDir, "b-midin", szExtension );
    printImage ( pf, szImageDir, "b-midl", szExtension );
  
    printImage ( pf, szImageDir, fTurn ? "b-midg" : "b-midg2", szExtension );
    printImage ( pf, szImageDir,
                 fTurn ? "b-midc" : aszDieO [ ms.anDice[ 0 ] - 1 ],
                 szExtension );
    printImage ( pf, szImageDir,
                 fTurn ? "b-midg2" : aszDieO [ ms.anDice[ 1 ] - 1 ],
                 szExtension );
    printImage ( pf, szImageDir,
                 fTurn ? aszDieX [ ms.anDice[ 0 ] - 1 ] : "b-midg2",
                 szExtension );
    printImage ( pf, szImageDir,
                 fTurn ? aszDieX [ ms.anDice[ 1 ] - 1 ] : "b-midc",
                 szExtension );
    printImage ( pf, szImageDir, fTurn ? "b-midg2" : "b-midg", szExtension );
    printImage ( pf, szImageDir, "b-midr", szExtension );

    fprintf ( pf, "<br>\n" );

  }
  else {

    if ( pms->fDoubled )
      printImage ( pf, szImageDir, "b-indent", szExtension );
    else
      printImage ( pf, szImageDir, "b-midin", szExtension );

    printImage ( pf, szImageDir, "b-midl", szExtension );
    printImage ( pf, szImageDir, "b-midg", szExtension );
    printImage ( pf, szImageDir, "b-midc", szExtension );
    printImage ( pf, szImageDir, "b-midg", szExtension );
    printImage ( pf, szImageDir, "b-midr", szExtension );

    fprintf ( pf, "<br>\n" );

  }

  /* cube image */

  if ( pms->fDoubled ) {

    /* questionmark with cube (if player 0 is doubled) */

    if ( pms->fTurn ) {
      sprintf ( sz, "b-ddn%d", 2 * pms->nCube );
      printImage ( pf, szImageDir, sz, szExtension );

    }
    else 
      printImage (pf, szImageDir, "b-indent", szExtension );

  }
  else {
    
    /* display cube */

    if ( pms->fCubeOwner == -1 || ! pms->fCubeOwner )
      printImage (pf, szImageDir, fTurn ? "b-botdn" : "b-botup",
                  szExtension );
    else {
      sprintf ( sz, "%s%d", fTurn ? "b-bdn" : "b-bup", pms->nCube );
      printImage (pf, szImageDir, sz, szExtension );
    }

  }

  printImage ( pf, szImageDir, "b-left", szExtension );

  /* display player 1's outer quadrant */

  fColor = 0;

  for ( i = 0; i < 6; i++ ) {

      printPoint ( pf, szImageDir, szExtension, 
                   anBoard[ 1 ][ 12 + i ],
                   anBoard[ 0 ][ 11 - i],
                   fColor, FALSE );

    fColor = ! fColor;

  }

  /* display bar */

  if ( anBoard[ 0 ][ 24 ] ) {

    sprintf ( sz, "b-bar-o%d", 
              ( anBoard[ 0 ][ 24 ] > 4 ) ?
              4 : anBoard[ 0 ][ 24 ] );
    printImage ( pf, szImageDir, sz, szExtension );

  }
  else 
    printImage ( pf, szImageDir, "b-bar", szExtension );

  /* display player 1's outer quadrant */

  fColor = 0;

  for ( i = 0; i < 6; i++ ) {

      printPoint ( pf, szImageDir, szExtension, 
                   anBoard[ 1 ][ 18 + i ],
                   anBoard[ 0 ][ 5 - i],
                   fColor, FALSE );

    fColor = ! fColor;

  }

  /* right border */

  printImage ( pf, szImageDir, "b-right", szExtension );
  fprintf ( pf, "<br>\n" );

  /* bottom */

  printImage ( pf, szImageDir, "b-indent", szExtension );
  printImage ( pf, szImageDir, fTurn ? "b-lobot" : "b-hibot", szExtension );
  fprintf ( pf, "<br>\n" );

  /* position ID */

  printImage ( pf, szImageDir, "b-indent", szExtension );
  fprintf ( pf, "PositionID: <tt>%s</tt><br>\n", PositionID ( ms.anBoard ) );

  /* pip counts */

  printImage ( pf, szImageDir, "b-indent", szExtension );

  PipCount ( anBoard, anPips );
  fprintf ( pf, "Pip counts: Red %d, White %d<br>\n",
            anPips[ 0 ], anPips[ 1 ] );
  

}



extern void CommandExportGameHtml( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export"
		 "game html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /*
    LaTeXPrologue( pf );
    
    ExportGameLaTeX( pf, plGame );

    LaTeXEpilogue( pf );
    */
    
    if( pf != stdout )
	fclose( pf );

}

extern void CommandExportMatchHtml( char *sz ) {
    
    FILE *pf;
    list *pl;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "match html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /* LaTeXPrologue( pf ); */

    /* FIXME write match introduction? */
    
    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
      /* ExportGameLaTeX( pf, pl->p ) */; 
    
    /* LaTeXEpilogue( pf ); */
    
    if( pf != stdout )
	fclose( pf );
}


extern void CommandExportPositionHtml( char *sz ) {

    FILE *pf;
    char szTitle[ 32 ];
	
    sz = NextToken( &sz );
    
    if( ms.gs == GAME_NONE ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "position html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    sprintf( szTitle, "Position %s", PositionID( ms.anBoard ) );

    printHTMLBoard( pf, &ms, ms.fTurn,
                    "http://fibs2html.sourceforge.net/images/", "gif" );
    
    if( pf != stdout )
	fclose( pf );
}

