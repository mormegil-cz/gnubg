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
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#include "analysis.h"
#include "backgammon.h"
#include "drawboard.h"
#include "export.h"
#include "eval.h"
#include "positionid.h"
#include "matchid.h"

#include "i18n.h"

#define STYLESHEET \
"<style type=\"text/css\">\n" \
".movetable { background-color: #c7c7c7 } \n" \
".moveheader { background-color: #787878 } \n" \
".movenumber { width: 2em; text-align: center } \n" \
".movenumber { text-align: right } \n" \
".moveply { width: 5em; text-align: center } \n" \
".movemove { width: 20em; text-align: left }\n" \
".moveequity { width: 10em; text-align: left } \n" \
".movethemove { background-color: #ffffcc } \n" \
".blunder { background-color: red; color: yellow } \n" \
".joker { background-color: red; color: yellow } \n" \
".stattable { text-align: left; width: 40em; background-color: #c7c7c7; border: 0px; padding: 0px } \n" \
".stattableheader { background-color: #787878 } \n" \
".result { background-color: yellow; font-weight: bold; text-align: center; color: black; width: 40em; padding: 0.2em } \n" \
".tiny { font-size: 25%% } \n" \
".cubedecision { background-color: #ddddee; text-align: left } \n" \
".cubedecision th { background-color: #89d0e2; text-align: center} \n" \
".comment { background-color: #449911; width: 39.5em; padding: 0.5em } \n" \
".commentheader { background-color: #557711; font-weight: bold; text-align: center; width: 40em; padding: 0.25em } \n" \
"td img {display: block;}\n" \
"</style>\n" 

#define FORMATHTMLPROB(f) \
( ( f ) < 1.0f ) ? "&nbsp;" : "", \
( ( f ) < 0.1f ) ? "&nbsp;" : "", \
( f ) * 100.0f
   



/* text for links on html page */

static char *aszLinkText[] = {
  N_ ("[First Game]"), 
  N_ ("[Previous Game]"), 
  N_ ("[Next Game]"), 
  N_ ("[Last Game]") };

/* Color of chequers */

static char *aszColorNameBBS[] = { N_ ("white"), N_ ("blue") };
static char *aszColorNameF2H[] = { N_ ("white"), N_ ("red") };
static char *aszColorNameGNU[] = { N_ ("red"), N_ ("black") };


static void
printStatTableHeader ( FILE *pf, const char *format, ... ) {

  va_list val;

  va_start( val, format );

  fputs ( "<tr class=\"stattableheader\">\n" 
          "<th colspan=\"3\" style=\"text-align: center\">", pf );
  vfprintf ( pf, format, val );
  fputs ( "</th>\n</tr>\n", pf );

  va_end( val );

}


static void
printStatTableRow ( FILE *pf, const char *format1, const char *format2,
                    ... ) {

  va_list val;
  char *sz;
  int l = 100 + strlen ( format1 ) + 2 * strlen ( format2 );

  va_start( val, format2 );

  sprintf ( sz = (char *) malloc ( l ),
             "<tr>\n" 
             "<td>%s</td>\n" 
             "<td>%s</td>\n" 
             "<td>%s</td>\n" 
             "</tr>\n", format1, format2, format2 );

  vfprintf ( pf, sz, val );

  free ( sz );

  va_end( val );

}


static void
printStatTableRow2 ( FILE *pf, const char *format1, const char *format2,
                     const char *format3, ... ) {

  va_list val;
  char *sz;
  int l = 100 + strlen ( format1 ) 
    + 2 * strlen ( format2 ) + 2 * strlen ( format3 );

  va_start( val, format3 );

  sprintf ( sz = (char *) malloc ( l ),
             "<tr>\n" 
             "<td>%s</td>\n" 
             "<td>%s (%s) </td>\n" 
             "<td>%s (%s) </td>\n" 
             "</tr>\n",
             format1, format2, format3, format2, format3 );
  
  vfprintf ( pf, sz, val );

  free ( sz );

  va_end( val );

}


static void
printStatTableRow3 ( FILE *pf, const char *format1, const char *format2,
                     const char *format3, const char *format4, ... ) {

  va_list val;
  char *sz;
  int l = 100 + strlen ( format1 ) 
    + 2 * strlen ( format2 ) + 2 * strlen ( format3 ) + 2 * strlen (format4 );

  va_start( val, format4 );

  sprintf ( sz = (char *) malloc ( l ),
             "<tr>\n" 
             "<td>%s</td>\n" 
             "<td>%s (%s (%s)) </td>\n" 
             "<td>%s (%s (%s)) </td>\n" 
             "</tr>\n",
             format1, 
             format2, format3, format4, 
             format2, format3, format4 );
  
  vfprintf ( pf, sz, val );

  free ( sz );

  va_end( val );

}


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
             const char *szExtension, const char *szAlt ) {

  fprintf ( pf, "<img src=\"%s%s%s.%s\" alt=\"%s\" />",
            ( szImageDir ) ? szImageDir : "",
            ( ! szImageDir || szImageDir[ strlen ( szImageDir ) - 1 ] == '/' ) ? "" : "/",
            szImage, szExtension, 
            ( szAlt ) ? szAlt : "" );

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
printPointBBS ( FILE *pf, const char *szImageDir, const char *szExtension,
                int iPoint0, int iPoint1, 
                const int fColor, const int fUp ) {

  char sz[ 100 ];
  char szAlt[ 100 ];
  char *aasz[ 2 ][ 2 ] = { { "dd", "dn" },
                           { "ud", "up" } };

  if ( iPoint0 ) {

    /* player 0 owns the point */

    sprintf ( sz, "p_%s_w_%d", aasz[ fUp ][ fColor ], iPoint0 );
    sprintf ( szAlt, "%1xX", iPoint0 );
    
  }
  else if ( iPoint1 ) {

    /* player 1 owns the point */

    sprintf ( sz, "p_%s_b_%d", aasz[ fUp ][ fColor ], iPoint1 );
    sprintf ( szAlt, "%1xO", iPoint1 );

  }
  else {
    /* empty point */
    sprintf ( sz, "p_%s_0", aasz[ fUp ][ fColor ] );
    sprintf ( szAlt, "&nbsp;'" );
  }

  printImage ( pf, szImageDir, sz, szExtension, szAlt );

}


static void
printHTMLBoardBBS ( FILE *pf, matchstate *pms, int fTurn,
                    const char *szImageDir, const char *szExtension,
                    const char *szType ) {

  int anBoard[ 2 ][ 25 ];
  int anPips[ 2 ];
  int acOff[ 2 ];
  int i, j;
  char sz[ 1024 ];

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  if ( pms->fMove ) SwapSides ( anBoard );
  PipCount ( anBoard, anPips );

  for( i = 0; i < 2; i++ ) {
    acOff[ i ] = 15;
    for ( j = 0; j < 25; j++ )
      acOff[ i ] -= anBoard[ i ][ j ];
  }

    
  /* 
   * Top row
   */

  printImage ( pf, szImageDir, fTurn ? "n_high" : "n_low", szExtension, NULL );
  fputs ( "<br />\n", pf );

  /* chequers off */

  sprintf ( sz, "o_w_%d", acOff[ 0 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 0's inner board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ i ],
                    anBoard[ 0 ][ 23 - i ],
                    ! ( i % 2 ), TRUE );

  /* player 1's chequers on the bar */

  sprintf ( sz, "b_up_%d", anBoard[ 0 ][ 24 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 0's outer board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ i + 6 ],
                    anBoard[ 0 ][ 17 - i ],
                    ! ( i % 2 ), TRUE );

  /* player 0 owning cube */

  if ( ! pms->fCubeOwner ) {
    sprintf ( sz, "c_up_%d", pms->nCube );
    printImage ( pf, szImageDir, sz, szExtension, NULL );
  }    
  else
    printImage ( pf, szImageDir, "c_up_0", szExtension, NULL );

  fputs ( "<br />\n", pf );

  /* end of first row */

  /*
   * center row (dice)
   */

  if ( pms->anDice[ 0 ] ) {

    /* dice rolled */

    sprintf ( sz, "b_center%d%d%s", 
              ( pms->anDice[ 0 ] < pms->anDice[ 1 ] ) ? 
              pms->anDice[ 0 ] : pms->anDice[ 1 ], 
              ( pms->anDice[ 0 ] < pms->anDice[ 1 ] ) ? 
              pms->anDice[ 1 ] : pms->anDice[ 0 ], 
              pms->fMove ? "right" : "left" );
    printImage ( pf, szImageDir, sz, szExtension, NULL );

  }
  else 
    /* no dice rolled */
    printImage ( pf, szImageDir, "b_center", szExtension, NULL );

  /* center cube */

  if ( pms->fCubeOwner == -1 ) {
    sprintf ( sz, "c_ce_%d", pms->nCube );
    printImage ( pf, szImageDir, sz, szExtension, NULL );
  }    
  else
    printImage ( pf, szImageDir, "c_ce_0", szExtension, NULL );

  fputs ( "<br />\n", pf );

  /* end of center row */

  /*
   * Bottom row
   */

  /* player 1's chequers off */

  sprintf ( sz, "o_b_%d", acOff[ 0 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 1's inner board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 23 - i ],
                    anBoard[ 0 ][ i ],
                    ! ( i % 2 ), FALSE );

  /* player 0's chequers on the bar */

  sprintf ( sz, "b_dn_%d", anBoard[ 0 ][ 24 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 1's outer board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 17 - i ],
                    anBoard[ 0 ][ i + 6 ],
                    ! ( i % 2 ), FALSE );

  /* player 1 owning cube */

  if ( pms->fCubeOwner == 1 ) {
    sprintf ( sz, "c_dn_%d", pms->nCube );
    printImage ( pf, szImageDir, sz, szExtension, NULL );
  }    
  else
    printImage ( pf, szImageDir, "c_dn_0", szExtension, NULL );

  fputs ( "<br />\n", pf );

  /* point numbers */

  printImage ( pf, szImageDir, fTurn ? "n_low" : "n_high", szExtension, NULL );

  fputs ( "<br />\n", pf );

  /* end of bottom row */

  /* position ID */

  fprintf ( pf, _("Position ID: <tt>%s</tt> Match ID: <tt>%s</tt><br />\n"),
            PositionID ( pms->anBoard ),
            MatchIDFromMatchState ( pms ) );

  /* pip counts */

  PipCount ( anBoard, anPips );
  fprintf ( pf, _("Pip counts: Blue %d, White %d<br />\n"),
            anPips[ 0 ], anPips[ 1 ] );
  

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
printPointF2H ( FILE *pf, const char *szImageDir, const char *szExtension,
                int iPoint0, int iPoint1, 
                const int fColor, const int fUp ) {

  char sz[ 100 ];
  char szAlt[ 100 ];

  if ( iPoint0 ) {

    /* player 0 owns the point */

    sprintf ( sz, "b-%c%c-o%d", fColor ? 'g' : 'y', fUp ? 'd' : 'u',
              ( iPoint0 >= 11 ) ? 11 : iPoint0 );
    sprintf ( szAlt, "%1xX", iPoint0 );
    
  }
  else if ( iPoint1 ) {

    /* player 1 owns the point */

    sprintf ( sz, "b-%c%c-x%d", fColor ? 'g' : 'y', fUp ? 'd' : 'u',
              ( iPoint1 >= 11 ) ?
              11 : iPoint1 );
    sprintf ( szAlt, "%1xO", iPoint1 );

  }
  else {
    /* empty point */
    sprintf ( sz, "b-%c%c", fColor ? 'g' : 'y', fUp ? 'd' : 'u' );
    sprintf ( szAlt, "&nbsp;'" );
  }

  printImage ( pf, szImageDir, sz, szExtension, szAlt );

}


static void
printHTMLBoardF2H ( FILE *pf, matchstate *pms, int fTurn,
                    const char *szImageDir, const char *szExtension,
                    const char *szType ) {

  char sz[ 100 ];
  char szAlt[ 100 ];
  int fColor;
  int i;

  char *aszDieO[] = { "b-odie1", "b-odie2", "b-odie3",
                     "b-odie4", "b-odie5", "b-odie6" };
  char *aszDieX[] = { "b-xdie1", "b-xdie2", "b-xdie3",
                     "b-xdie4", "b-xdie5", "b-xdie6" };

  int anBoard[ 2 ][ 25 ];
  int anPips[ 2 ];

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  if ( pms->fMove ) SwapSides ( anBoard );

  /* top line with board numbers */

  fprintf ( pf, "<p>\n" );
  printImage ( pf, szImageDir, "b-indent", szExtension, "" );
  printImage ( pf, szImageDir, fTurn ? "b-hitop" : "b-lotop", szExtension,
               fTurn ? "+-13-14-15-16-17-18-+---+-19-20-21-22-23-24-+" :
               "+-12-11-10--9--8--7-+---+--6--5--4--3--2--1-+" );
  fprintf ( pf, "<br />\n" );

  /* cube image */

  if ( pms->fDoubled ) {

    /* questionmark with cube (if player 0 is doubled) */

    if ( ! pms->fTurn ) {
      sprintf ( sz, "b-dup%d", 2 * pms->nCube );
      printImage ( pf, szImageDir, sz, szExtension, "" );

    }
    else 
      printImage (pf, szImageDir, "b-indent", szExtension, "" );

  }
  else {
    
    /* display arrow */

    if ( pms->fCubeOwner == -1 || pms->fCubeOwner )
      printImage (pf, szImageDir, fTurn ? "b-topdn" : "b-topup",
                  szExtension, "" );
    else {
      sprintf ( sz, "%s%d", fTurn ? "b-tdn" : "b-tup", pms->nCube );
      printImage (pf, szImageDir, sz, szExtension, "" );
    }

  }

  /* display left border */

  printImage ( pf, szImageDir, "b-left", szExtension, "|" );

  /* display player 0's outer quadrant */

  fColor = 1;

  for ( i = 0; i < 6; i++ ) {

    printPointF2H ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 11 - i ],
                    anBoard[ 0 ][ 12 + i],
                    fColor, TRUE );

    fColor = ! fColor;

  }

  /* display bar */

  if ( anBoard[ 0 ][ 24 ] ) {

    sprintf ( sz, "b-bar-x%d", 
              ( anBoard[ 0 ][ 24 ] > 4 ) ?
              4 : anBoard[ 0 ][ 24 ] );
    sprintf ( szAlt, "|%1X&nbsp;|", anBoard[ 0 ][ 24 ] );
    printImage ( pf, szImageDir, sz, szExtension, szAlt );

  }
  else 
    printImage ( pf, szImageDir, "b-bar", szExtension, "|&nbsp;&nbsp;&nbsp;|" );

  /* display player 0's home quadrant */

  fColor = 1;

  for ( i = 0; i < 6; i++ ) {

    printPointF2H ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 5 - i ],
                    anBoard[ 0 ][ 18 + i],
                    fColor, TRUE );

    fColor = ! fColor;

  }

  /* right border */

  printImage ( pf, szImageDir, "b-right", szExtension, "|" );

  fprintf ( pf, "<br />\n" );

  /* center of board */

  if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] ) {

    printImage ( pf, szImageDir, "b-midin", szExtension, "" );
    printImage ( pf, szImageDir, "b-midl", szExtension, "|" );
  
    printImage ( pf, szImageDir,
                 fTurn ? "b-midg" : "b-midg2", szExtension,
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    printImage ( pf, szImageDir,
                 fTurn ? "b-midc" : aszDieO [ pms->anDice[ 0 ] - 1 ],
                 szExtension,
                 "|&nbsp;&nbsp;&nbsp;|" );
    printImage ( pf, szImageDir,
                 fTurn ? "b-midg2" : aszDieO [ pms->anDice[ 1 ] - 1 ],
                 szExtension, 
                 "|&nbsp;&nbsp;&nbsp;|" );
    printImage ( pf, szImageDir,
                 fTurn ? aszDieX [ pms->anDice[ 0 ] - 1 ] : "b-midg2",
                 szExtension,
                 "|&nbsp;&nbsp;&nbsp;|" );
    printImage ( pf, szImageDir,
                 fTurn ? aszDieX [ pms->anDice[ 1 ] - 1 ] : "b-midc",
                 szExtension,
                 "|&nbsp;&nbsp;&nbsp;|" );
    printImage ( pf, szImageDir, fTurn ? "b-midg2" : "b-midg", szExtension,
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    printImage ( pf, szImageDir, "b-midr", szExtension, "|" );

    fprintf ( pf, "<br />\n" );

  }
  else {

    if ( pms->fDoubled )
      printImage ( pf, szImageDir, "b-indent", szExtension, "" );
    else
      printImage ( pf, szImageDir, "b-midin", szExtension, "" );

    printImage ( pf, szImageDir, "b-midl", szExtension, "|" );
    printImage ( pf, szImageDir, "b-midg", szExtension,
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    printImage ( pf, szImageDir, "b-midc", szExtension,
                 "|&nbsp;&nbsp;&nbsp;|" );
    printImage ( pf, szImageDir, "b-midg", szExtension,
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    printImage ( pf, szImageDir, "b-midr", szExtension, "|" );

    fprintf ( pf, "<br />\n" );

  }

  /* cube image */

  if ( pms->fDoubled ) {

    /* questionmark with cube (if player 0 is doubled) */

    if ( pms->fTurn ) {
      sprintf ( sz, "b-ddn%d", 2 * pms->nCube );
      printImage ( pf, szImageDir, sz, szExtension, "" );

    }
    else 
      printImage (pf, szImageDir, "b-indent", szExtension, "" );

  }
  else {
    
    /* display cube */

    if ( pms->fCubeOwner == -1 || ! pms->fCubeOwner )
      printImage (pf, szImageDir, fTurn ? "b-botdn" : "b-botup",
                  szExtension, "" );
    else {
      sprintf ( sz, "%s%d", fTurn ? "b-bdn" : "b-bup", pms->nCube );
      printImage (pf, szImageDir, sz, szExtension, "" );
    }

  }

  printImage ( pf, szImageDir, "b-left", szExtension, "|" );

  /* display player 1's outer quadrant */

  fColor = 0;

  for ( i = 0; i < 6; i++ ) {

    printPointF2H ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 12 + i ],
                    anBoard[ 0 ][ 11 - i],
                    fColor, FALSE );

    fColor = ! fColor;

  }

  /* display bar */

  if ( anBoard[ 1 ][ 24 ] ) {

    sprintf ( sz, "b-bar-o%d", 
              ( anBoard[ 1 ][ 24 ] > 4 ) ?
              4 : anBoard[ 1 ][ 24 ] );
    sprintf ( szAlt, "|&nbsp;%1dO|", anBoard[ 1 ][ 24 ] );
    printImage ( pf, szImageDir, sz, szExtension, szAlt );

  }
  else 
    printImage ( pf, szImageDir, "b-bar", szExtension, "|&nbsp;&nbsp;&nbsp;|" );

  /* display player 1's outer quadrant */

  fColor = 0;

  for ( i = 0; i < 6; i++ ) {

    printPointF2H ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 18 + i ],
                    anBoard[ 0 ][ 5 - i],
                    fColor, FALSE );

    fColor = ! fColor;

  }

  /* right border */

  printImage ( pf, szImageDir, "b-right", szExtension, "|" );
  fprintf ( pf, "<br />\n" );

  /* bottom */

  printImage ( pf, szImageDir, "b-indent", szExtension, "" );
  printImage ( pf, szImageDir, fTurn ? "b-lobot" : "b-hibot", szExtension,
               fTurn ?
               "+-12-11-10--9--8--7-+---+--6--5--4--3--2--1-+" :
               "+-13-14-15-16-17-18-+---+-19-20-21-22-23-24-+" );
  fprintf ( pf, "<br />\n" );

  /* position ID */

  printImage ( pf, szImageDir, "b-indent", szExtension, "" );
  fprintf ( pf, "Position ID: <tt>%s</tt> Match ID: <tt>%s</tt><br />\n",
            PositionID ( pms->anBoard ),
            MatchIDFromMatchState ( pms ) );

  /* pip counts */

  printImage ( pf, szImageDir, "b-indent", szExtension, "" );

  PipCount ( anBoard, anPips );
  fprintf ( pf, _("Pip counts: Red %d, White %d<br />\n"),
            anPips[ 0 ], anPips[ 1 ] );
  
  fprintf ( pf, "</p>\n" );

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
printPointGNU ( FILE *pf, const char *szImageDir, const char *szExtension,
                int iPoint0, int iPoint1, 
                const int fColor, const int fUp ) {

  char sz[ 100 ];
  char szAlt[ 100 ];

  if ( iPoint0 ) {

    /* player 0 owns the point */

    sprintf ( sz, "b-%c%c-x%d", fColor ? 'g' : 'r', fUp ? 'd' : 'u',
              ( iPoint0 >= 11 ) ? 11 : iPoint0 );
    sprintf ( szAlt, "%1xX", iPoint0 );
    
  }
  else if ( iPoint1 ) {

    /* player 1 owns the point */

    sprintf ( sz, "b-%c%c-o%d", fColor ? 'g' : 'r', fUp ? 'd' : 'u',
              ( iPoint1 >= 11 ) ?
              11 : iPoint1 );
    sprintf ( szAlt, "%1xO", iPoint1 );

  }
  else {
    /* empty point */
    sprintf ( sz, "b-%c%c", fColor ? 'g' : 'r', fUp ? 'd' : 'u' );
    sprintf ( szAlt, "&nbsp;'" );
  }

  printImage ( pf, szImageDir, sz, szExtension, szAlt );

}




static void
printHTMLBoardGNU ( FILE *pf, matchstate *pms, int fTurn,
                    const char *szImageDir, const char *szExtension,
                    const char *szType ) {

  char sz[ 100 ];
  char szAlt[ 100 ];
  int i, j;

  int anBoard[ 2 ][ 25 ];
  int anPips[ 2 ];
  int acOff[ 2 ];

  memcpy ( anBoard, pms->anBoard, sizeof ( anBoard ) );

  if ( pms->fMove ) SwapSides ( anBoard );

  for( i = 0; i < 2; i++ ) {
    acOff[ i ] = 15;
    for ( j = 0; j < 25; j++ )
      acOff[ i ] -= anBoard[ i ][ j ];
  }

  /* top line with board numbers */

  fputs ( "<table cellpadding=\"0\" border=\"0\" cellspacing=\"0\""
          " style=\"margin: 0; padding: 0; border: 0\">\n", pf );
  fputs ( "<tr>", pf );

  fputs ( "<td colspan=\"15\">", pf );
  printImage ( pf, szImageDir, fTurn ? "b-hitop" : "b-lotop", szExtension,
               fTurn ? "+-13-14-15-16-17-18-+---+-19-20-21-22-23-24-+" :
               "+-12-11-10--9--8--7-+---+--6--5--4--3--2--1-+" );
  fputs ( "</td></tr>\n", pf );

  /* display left bearoff tray */

  fputs ( "<tr>", pf );

  fputs ( "<td rowspan=\"2\">", pf );
  printImage ( pf, szImageDir, "b-roff", szExtension, "|" );
  fputs ( "</td>", pf );

  /* display player 0's outer quadrant */

  for ( i = 0; i < 6; i++ ) {
    fputs ( "<td rowspan=\"2\">", pf );
    printPointGNU ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 11 - i ],
                    anBoard[ 0 ][ 12 + i],
                    ! (i % 2), TRUE );
    fputs ( "</td>", pf );
  }


  /* display cube */

  fputs ( "<td>", pf );

  if ( ! pms->fCubeOwner )
    sprintf ( sz, "b-ct-%d", pms->nCube );
  else
    strcpy ( sz, "b-ct" );
  printImage ( pf, szImageDir, sz, szExtension, "" );

  fputs ( "</td>", pf );


  /* display player 0's home quadrant */

  for ( i = 0; i < 6; i++ ) {
    fputs ( "<td rowspan=\"2\">", pf );
    printPointGNU ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 5 - i ],
                    anBoard[ 0 ][ 18 + i],
                    ! ( i % 2 ), TRUE );
    fputs ( "</td>", pf );
  }


  /* right bearoff tray */

  fputs ( "<td rowspan=\"2\">", pf );
  if ( acOff[ 0 ] ) 
    sprintf ( sz, "b-roff-x%d", acOff[ 0 ] );
  else
    strcpy ( sz, "b-roff" );
  printImage ( pf, szImageDir, sz, szExtension, "|" );
  fputs ( "</td>", pf );

  fputs ( "</tr>\n", pf );


  /* display bar */

  fputs ( "<tr>", pf );
  fputs ( "<td>", pf );

  sprintf ( sz, "b-bar-o%d", anBoard[ 1 ][ 24 ] );
  if ( anBoard[ 1 ][ 24 ] )
    sprintf ( szAlt, "|%1X&nbsp;|", anBoard[ 1 ][ 24 ] );
  else
    strcpy ( szAlt, "|&nbsp;&nbsp;&nbsp;|" );
  printImage ( pf, szImageDir, sz, szExtension, szAlt );

  fputs ( "</td>", pf );
  fputs ( "</tr>\n", pf );

  
  /* center of board */

  fputs ( "<tr>", pf );

  /* left part of bar */

  fputs ( "<td>", pf );
  printImage ( pf, szImageDir, "b-midlb", szExtension, "|" );
  fputs ( "</td>", pf );

  /* center of board */

  fputs ( "<td colspan=\"6\">", pf );

  if ( ! pms->fMove && pms->anDice[ 0 ] && pms->anDice[ 1 ] ) {

    /* player 0 has rolled the dice */
    
    sprintf ( sz, "b-midl-x%d%d", pms->anDice[ 0 ], pms->anDice[ 1 ] );
    sprintf ( szAlt, "&nbsp;&nbsp;%d%d&nbsp;&nbsp;", 
              pms->anDice[ 0 ], pms->anDice[ 1 ] );
    
    printImage ( pf, szImageDir, sz, szExtension, szAlt );
    
  }
  else if ( ! pms->fMove && pms->fDoubled ) {
    
    /* player 0 has doubled */
    
    sprintf ( sz, "b-midl-c%d", 2 * pms->nCube );
    sprintf ( szAlt, "&nbsp;[%d]&nbsp;&nbsp;", 2 * pms->nCube );
    
    printImage ( pf, szImageDir, sz, szExtension, szAlt );
    
  }
  else {
    
    /* player 0 on roll */
    
    printImage ( pf, szImageDir, "b-midl", szExtension, 
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    
  }

  fputs ( "</td>", pf );

  /* centered cube */

  if ( pms->fCubeOwner == -1 && ! pms->fDoubled )
    sprintf ( sz, "b-midc-%d", pms->nCube );
  else
    strcpy ( sz, "b-midc" );

  fputs ( "<td>", pf );
  printImage ( pf, szImageDir, sz, szExtension, "|" );
  fputs ( "</td>", pf );

  /* player 1 */

  fputs ( "<td colspan=\"6\">", pf );

  if ( pms->fMove == 1 && pms->anDice[ 0 ] && pms->anDice[ 1 ] ) {

    /* player 1 has rolled the dice */
    
    sprintf ( sz, "b-midr-o%d%d", pms->anDice[ 0 ], pms->anDice[ 1 ] );
    sprintf ( szAlt, "&nbsp;&nbsp;%d%d&nbsp;&nbsp;", 
              pms->anDice[ 0 ], pms->anDice[ 1 ] );
    
    printImage ( pf, szImageDir, sz, szExtension, szAlt );
    
  }
  else if ( pms->fMove == 1 && pms->fDoubled ) {
    
    /* player 1 has doubled */
    
    sprintf ( sz, "b-midr-c%d", 2 * pms->nCube );
    sprintf ( szAlt, "&nbsp;[%d]&nbsp;&nbsp;", 2 * pms->nCube );
    
    printImage ( pf, szImageDir, sz, szExtension, szAlt );
    
  }
  else {
    
    /* player 1 on roll */
    
    printImage ( pf, szImageDir, "b-midr", szExtension, 
                 "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;" );
    
  }

  fputs ( "</td>", pf );

  /* right part of bar */

  fputs ( "<td>", pf );
  printImage ( pf, szImageDir, "b-midlb", szExtension, "|" );
  fputs ( "</td>", pf );

  fputs ( "</tr>\n", pf );


  /* display left bearoff tray */

  fputs ( "<tr>", pf );

  fputs ( "<td rowspan=\"2\">", pf );
  printImage ( pf, szImageDir, "b-roff", szExtension, "|" );
  fputs ( "</td>", pf );


  /* display player 1's outer quadrant */

  for ( i = 0; i < 6; i++ ) {
    fputs ( "<td rowspan=\"2\">", pf );
    printPointGNU ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 12 + i ],
                    anBoard[ 0 ][ 11 - i],
                    i % 2, FALSE );
    fputs ( "</td>", pf );
  }


  /* display bar */

  fputs ( "<td>", pf );

  sprintf ( sz, "b-bar-x%d", anBoard[ 0 ][ 24 ] );
  if ( anBoard[ 0 ][ 24 ] )
    sprintf ( szAlt, "|%1X&nbsp;|", anBoard[ 0 ][ 24 ] );
  else
    strcpy ( szAlt, "|&nbsp;&nbsp;&nbsp;|" );
  printImage ( pf, szImageDir, sz, szExtension, szAlt );

  fputs ( "</td>", pf );


  /* display player 1's outer quadrant */

  for ( i = 0; i < 6; i++ ) {
    fputs ( "<td rowspan=\"2\">", pf );
    printPointGNU ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 18 + i ],
                    anBoard[ 0 ][ 5 - i],
                    i % 2, FALSE );
    fputs ( "</td>", pf );
  }


  /* right bearoff tray */

  fputs ( "<td rowspan=\"2\">", pf );
  if ( acOff[ 0 ] ) 
    sprintf ( sz, "b-roff-o%d", acOff[ 0 ] );
  else
    strcpy ( sz, "b-roff" );
  printImage ( pf, szImageDir, sz, szExtension, "|" );
  fputs ( "</td>", pf );

  fputs ( "</tr>\n", pf );


  /* display cube */
  
  fputs ( "<tr>", pf );
  fputs ( "<td>", pf );

  if ( pms->fCubeOwner == 1 )
    sprintf ( sz, "b-cb-%d", pms->nCube );
  else
    strcpy ( sz, "b-cb" );
  printImage ( pf, szImageDir, sz, szExtension, "" );

  fputs ( "</td>", pf );
  fputs ( "</tr>\n", pf );


  /* bottom */

  fputs ( "<tr>", pf );
  fputs ( "<td colspan=\"15\">", pf );
  printImage ( pf, szImageDir, fTurn ? "b-lobot" : "b-hibot", szExtension,
               fTurn ?
               "+-12-11-10--9--8--7-+---+--6--5--4--3--2--1-+" :
               "+-13-14-15-16-17-18-+---+-19-20-21-22-23-24-+" );
  fputs ( "</td>", pf );
  fputs ( "</tr>", pf );

  /* position ID */

  fputs ( "<tr>", pf );
  fputs ( "<td colspan=\"15\">", pf );
  fprintf ( pf, _("Position ID: <tt>%s</tt> Match ID: <tt>%s</tt><br />\n"),
            PositionID ( pms->anBoard ),
            MatchIDFromMatchState ( pms ) );
  fputs ( "</td>", pf );
  fputs ( "</tr>\n", pf );

  /* pip counts */

  PipCount ( anBoard, anPips );
  fputs ( "<tr>", pf );
  fputs ( "<td colspan=\"15\">", pf );
  fprintf ( pf, _("Pip counts: Black %d, Red %d<br />\n"),
            anPips[ 0 ], anPips[ 1 ] );
  fputs ( "</td>", pf );
  fputs ( "</tr>\n", pf );

  fputs ( "</table>\n\n", pf );

}


static void
printHTMLBoard ( FILE *pf, matchstate *pms, int fTurn,
                 const char *szImageDir, const char *szExtension,
                 const char *szType ) {


  if ( ! strcmp ( szType, "fibs2html" ) )
    printHTMLBoardF2H ( pf, pms, fTurn, szImageDir, szExtension, szType );
  else if ( ! strcmp ( szType, "bbs" ) )
    printHTMLBoardBBS ( pf, pms, fTurn, szImageDir, szExtension, szType );
  else if ( ! strcmp ( szType, "gnu" ) )
    printHTMLBoardGNU ( pf, pms, fTurn, szImageDir, szExtension, szType );
  else
    printf ( _("unknown board type\n") );


}


/*
 * Print html header for board: move or cube decision 
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *   iMove: move no.
 *
 */

static void 
HTMLBoardHeader ( FILE *pf, const matchstate *pms, 
                  char *aszColorName[],
                  const int iGame, const int iMove ) {

  fprintf ( pf,
            "<hr />\n"
            "<p><b>"
            "<a name=\"game%d.move%d\">",
            iGame + 1, iMove + 1 );
  fprintf ( pf, _("Move number %d:"), iMove + 1 );
  fputs ( "</a></b>", pf );

  if ( pms->fResigned ) 
    
    /* resignation */

    fprintf ( pf,
              _(" %s resigns %d points"), 
              gettext ( aszColorName[ pms->fTurn ] ),
              pms->fResigned * pms->nCube
            );
  
  else if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] )

    /* chequer play decision */

    fprintf ( pf,
              _(" %s to play %d%d"),
              gettext ( aszColorName[ pms->fTurn ] ),
              pms->anDice[ 0 ], pms->anDice[ 1 ] 
            );

  else if ( pms->fDoubled )

    /* take decision */

    fprintf ( pf,
              _(" %s doubles to %d"),
              gettext ( aszColorName[ ! pms->fMove ] ),
              pms->nCube * 2
            );

  else

    /* cube decision */

    fprintf ( pf,
              _(" %s onroll, cube decision?"),
              gettext ( aszColorName[ pms->fTurn ] )
            );


  fputs ( "</p>\n", pf );

}


/*
 * Print html header: dtd, head etc.
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *
 */

static void 
HTMLPrologue ( FILE *pf, const matchstate *pms,
               const int iGame,
               char *aszLinks[ 4 ] ) {

  char szTitle[ 100 ];
  char szHeader[ 100 ];

  int i;
  int fFirst;

  /* DTD */

  if ( pms->nMatchTo )
    sprintf ( szTitle,
              _("%s versus %s, score is %d-%d in %d points match (game %d)"),
              ap [ 1 ].szName, ap[ 0 ].szName,
              pms->anScore[ 1 ], pms->anScore[ 0 ], pms->nMatchTo,
              iGame + 1 );
  else
    sprintf ( szTitle,
              _("%s versus %s, score is %d-%d in money game (game %d)"),
              ap [ 1 ].szName, ap[ 0 ].szName,
              pms->anScore[ 1 ], pms->anScore[ 0 ], 
              iGame + 1 );

  if ( pms->nMatchTo )
    sprintf ( szHeader,
              _("%s (red, %d pts) vs. %s (black, %d pts) (Match to %d)"),
              ap [ 0 ].szName, pms->anScore[ 0 ],
              ap [ 1 ].szName, pms->anScore[ 1 ],
              pms->nMatchTo );
  else
    sprintf ( szHeader,
              _("%s (red, %d pts) vs. %s (black, %d pts) (money game)"),
              ap [ 0 ].szName, pms->anScore[ 0 ],
              ap [ 1 ].szName, pms->anScore[ 1 ] );


  fprintf ( pf,
            "<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Strict//EN' "
            "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" "
            "lang=\"en\">\n"
            "<head>\n"
            "<meta name=\"generator\" content=\"GNU Backgammon %s\" />\n"
            "<meta http-equiv=\"Content-Type\" "
            "content=\"text/html; charset=ISO-8859-1\" />\n" 
            "<meta name=\"keywords\" content=\"%s, %s, %s\" />\n"
            "<meta name=\"description\" "
            "content=\"",
            VERSION,
            ap[ 0 ].szName, ap[ 1 ].szName,
            ( pms->nMatchTo ) ? _("match play") : _("money game") );

  fprintf ( pf, 
            _("%s (analysed by GNU Backgammon %s)"),
            szTitle, VERSION );

  fprintf ( pf,
            "\" />\n"
            "<title>%s</title>\n"
            STYLESHEET
            "</head>\n"
            "\n"
            "<body>\n"
            "<h1>",
            szTitle );

  fprintf ( pf,
            _("Game number %d"),
            iGame + 1 );

  fprintf ( pf, 
            "</h1>\n"
            "<h2>%s</h2>\n"
            ,
            szHeader );

  /* add links to other games */

  fFirst = TRUE;

  for ( i = 0; i < 4; i++ )
    if ( aszLinks && aszLinks[ i ] ) {
      if ( fFirst ) {
        fprintf ( pf, "<hr />\n" );
        fputs ( "<p>\n", pf );
        fFirst = FALSE;
      }
      fprintf ( pf, "<a href=\"%s\">%s</a>\n",
                aszLinks[ i ], gettext ( aszLinkText[ i ] ) );
    }

  if ( ! fFirst )
    fputs ( "</p>\n", pf );

}


/*
 * Print html header: dtd, head etc.
 *
 * Input:
 *   pf: output file
 *   ms: current match state
 *
 */

static void 
HTMLEpilogue ( FILE *pf, const matchstate *pms, char *aszLinks[ 4 ] ) {

  time_t t;
  int fFirst;
  int i;

  const char szVersion[] = "$Revision$";
  int iMajor, iMinor;

  iMajor = atoi ( strchr ( szVersion, ' ' ) );
  iMinor = atoi ( strchr ( szVersion, '.' ) + 1 );

  /* add links to other games */

  fFirst = TRUE;

  for ( i = 0; i < 4; i++ )
    if ( aszLinks && aszLinks[ i ] ) {
      if ( fFirst ) {
        fprintf ( pf, "<hr />\n" );
        fputs ( "<p>\n", pf );
        fFirst = FALSE;
      }
      fprintf ( pf, "<a href=\"%s\">%s</a>\n",
                aszLinks[ i ], aszLinkText[ i ] );
    }

  if ( ! fFirst )
    fputs ( "</p>\n", pf );

  time ( &t );

  fputs ( "<hr />\n"
          "<address>", pf );

  fprintf ( pf, 
            _("Output generated %s by "
              "<a href=\"http://www.gnu.org/software/gnubg/\">GNU Backgammon " 
              "%s</a>") ,
            ctime ( &t ), VERSION );
            
  fprintf ( pf,
            _("(HTML Export version %d.%d)"),
            iMajor, iMinor );

  fprintf ( pf,
            "</address>\n"
            "<p>\n"
            "<a href=\"http://validator.w3.org/check/referer\">"
            "<img style=\"border:0;width:88px;height:31px\" "
            "src=\"http://www.w3.org/Icons/valid-xhtml10\" "
            "alt=\"%s\" /></a>\n"
            "<a href=\"http://jigsaw.w3.org/css-validator/\">"
            "<img style=\"border:0;width:88px;height:31px\" "
            "src=\"http://jigsaw.w3.org/css-validator/images/vcss\" "
            "alt=\"%s\" />"
            "</a>\n"
            "</p>\n"
            "</body>\n"
            "</html>\n",
            _("Valid XHTML 1.0!"),
            _("Valid CSS!") );
         

}

/*
 * is this a close cubedecision?
 *
 * Input:
 *   arDouble: equities for cube decisions
 *
 */

static int
isCloseCubedecision ( const float arDouble[] ) {
  
  const float rThr = 0.05;

  /* too good positions */

  if ( arDouble[ OUTPUT_NODOUBLE ] > 1.0 ) return 1;

  /* almost a double */

  if ( fabs ( arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ] ) < rThr )
    return 1;

  /* almost a pass */

  if ( fabs ( arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ] ) < rThr )
    return 1;

  return 0;

}


/*
 * is this a missed double?
 *
 * Input:
 *   arDouble: equities for cube decisions
 *   fDouble: did the player double
 *   pci: cubeinfo
 *
 */

static int
isMissedDouble ( float arDouble[], int fDouble, cubeinfo *pci ) {

  cubedecision cd = FindBestCubeDecision ( arDouble, pci );

  switch ( cd ) {
    
  case DOUBLE_TAKE:
  case DOUBLE_PASS:
  case DOUBLE_BEAVER:
  case REDOUBLE_TAKE:
  case REDOUBLE_PASS:

    return ! fDouble;
    break;

  default:

    return 0;
    break;


  }

}
                 


/*
 * Print cube analysis
 *
 * Input:
 *  pf: output file
 *  arDouble: equitites for cube decisions
 *  fPlayer: player who doubled
 *  esDouble: eval setup
 *  pci: cubeinfo
 *  fDouble: double/no double
 *  fTake: take/drop
 *
 */

static void
HTMLPrintCubeAnalysisTable ( FILE *pf, float arDouble[],
                             float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                             float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                             int fPlayer,
                             evalsetup *pes, cubeinfo *pci,
                             int fDouble, int fTake,
                             skilltype stDouble,
                             skilltype stTake ) {
  const char *aszCube[] = {
    NULL, 
    N_("No double"), 
    N_("Double, take"), 
    N_("Double, pass") 
  };

  int i;
  int ai[ 3 ];
  float r;

  int fActual, fClose, fMissed;
  int fDisplay;
  int fAnno = FALSE;

  /* check if cube analysis should be printed */

  if ( pes->et == EVAL_NONE ) return; /* no evaluation */
  if ( ! GetDPEq ( NULL, NULL, pci ) ) return; /* cube not available */

  fActual = fDouble;
  fClose = isCloseCubedecision ( arDouble ); 
  fMissed = isMissedDouble ( arDouble, fDouble, pci );

  fDisplay = 
    ( fActual && exsExport.afCubeDisplay[ EXPORT_CUBE_ACTUAL ] ) ||
    ( fClose && exsExport.afCubeDisplay[ EXPORT_CUBE_CLOSE ] ) ||
    ( fMissed && exsExport.afCubeDisplay[ EXPORT_CUBE_MISSED ] ) ||
    ( exsExport.afCubeDisplay[ stDouble ] ) ||
    ( exsExport.afCubeDisplay[ stTake ] );

  if ( ! fDisplay )
    return;

  /* print alerts */

  if ( fMissed ) {

    fAnno = TRUE;

    /* missed double */

    fprintf ( pf, "<p><span class=\"blunder\">%s ",
              _("Alert: missed double") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fprintf ( pf, "</span></p>\n" );

    }
    else {


      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc( arDouble[ OUTPUT_DROP ], pci ) -
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc( arDouble[ OUTPUT_TAKE ], pci ) -
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fprintf ( pf, "</span></p>\n" );

    }

  }

  r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_DROP ];

  if ( fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong take */

    fprintf ( pf, "<p><span class=\"blunder\">%s ",
              _("Alert: wrong take") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fprintf ( pf, "</span></p>\n" );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fprintf ( pf, "</span></p>\n" );

    }

  }

  r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && ! fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong pass */

    fprintf ( pf, "<p><span class=\"blunder\">%s ",
              _("Alert: wrong pass") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fprintf ( pf, "</span></p>\n" );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stTake != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );

      fprintf ( pf, "</span></p>\n" );

    }

  }


  if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ];
  else
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong double */

    fprintf ( pf, "<p><span class=\"blunder\">%s ",
              _("Alert: wrong double") );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_NODOUBLE ];
      else
        r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_NODOUBLE ];    

      fprintf ( pf, " (%+7.3f)!", r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fprintf ( pf, "</span></p>\n" );

    }
    else {

      if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
        r = eq2mwc ( arDouble[ OUTPUT_DROP ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );
      else
        r = eq2mwc ( arDouble[ OUTPUT_TAKE ], pci ) - 
          eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], pci );

      fprintf ( pf, " (%+6.3f%%)!", 100.0f * r );
      
      if ( stDouble != SKILL_NONE )
        fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

      fprintf ( pf, "</span></p>\n" );

    }

  }

  if ( ( stDouble != SKILL_NONE || stTake != SKILL_NONE ) && ! fAnno ) {
    
    if ( stDouble != SKILL_NONE ) {
      fputs ( "<p><span class=\"blunder\">", pf );
      fprintf ( pf, _("Alert: double decision marked %s"),
                gettext ( aszSkillType[ stDouble ] ) );
      fputs ( "</span></p>\n", pf );
    }

    if ( stTake != SKILL_NONE ) {
      fputs ( "<p><span class=\"blunder\">", pf );
      fprintf ( pf, _("Alert: take decision marked %s"),
                gettext ( aszSkillType[ stTake ] ) );
      fputs ( "</span></p>\n", pf );
    }

  }

  /* print table */

  fprintf ( pf,
            "<table class=\"cubedecision\">\n" );

  /* header */

  fprintf ( pf, 
            "<tr><th colspan=\"4\">%s</th></tr>\n",
            _("Cube decision") );

  /* ply & cubeless equity */

  /* FIXME: about parameters if exsExport.afCubeParameters */

  fprintf ( pf, "<tr>" );

  /* ply */

  fputs ( "<td>", pf );
  
  switch ( pes->et ) {
  case EVAL_NONE:
    fputs ( _("n/a"), pf );
    break;
  case EVAL_EVAL:
    fprintf ( pf, 
              _("%d-ply"), 
              pes->ec.nPlies );
    break;
  case EVAL_ROLLOUT:
    fputs ( _("Rollout"), pf );
    break;
  }

  fputs ( "</td>\n", pf );

  /* cubeless equity */
  
  if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) )
    fprintf ( pf, "<td>%s</td><td>%+7.3f</td><td>&nbsp;</td>\n",
              _("Cubeless equity"),
              aarOutput[ 0 ][ OUTPUT_EQUITY ] );
  else
    fprintf ( pf, "<td>%s</td><td>%+7.3f</td><td>&nbsp;</td>\n",
              _("Cubeless equity"),
              eq2mwc ( aarOutput[ 0 ][ OUTPUT_EQUITY ], pci ) );
              

  fprintf ( pf, "</tr>\n" );

  /* percentages */

  if ( exsExport.fCubeDetailProb ) {

    /* FIXME: include in movenormal and movedouble */

    float *ar = aarOutput[ 0 ];

    fprintf ( pf,
              "<tr><td>&nbsp;</td>"
              "<td colspan=\"3\">"
              "%s%s%6.2f%% "
              "%s%s%6.2f%% "
              "%s%s%6.2f%% "
              "%s%s%6.2f%% "
              "%s%s%6.2f%% "
              "%s%s%6.2f%% "
              "</td></tr>\n",
              FORMATHTMLPROB ( ar[ OUTPUT_WINBACKGAMMON ] ),
              FORMATHTMLPROB ( ar[ OUTPUT_WINGAMMON ] ),
              FORMATHTMLPROB ( ar[ OUTPUT_WIN ] ),
              FORMATHTMLPROB ( 1.0 - ar[ OUTPUT_WIN ] ),
              FORMATHTMLPROB ( ar [ OUTPUT_LOSEGAMMON ] ),
              FORMATHTMLPROB ( ar [ OUTPUT_LOSEBACKGAMMON ]) );

  }

  /* equities */

  getCubeDecisionOrdering ( ai, arDouble, pci );

  for ( i = 0; i < 3; i++ ) {

    fprintf ( pf,
              "<tr><td>%d.</td><td>%s</td>", i + 1, 
              gettext ( aszCube[ ai[ i ] ] ) );

    if ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) {
      if ( i ) 
        fprintf ( pf,
                  "<td>%+7.3f</td><td>%+7.3f</td></tr>\n",
                  arDouble[ ai[ i ] ],
                  arDouble[ ai[ i ] ] - arDouble[ OUTPUT_OPTIMAL ] );
      else
        fprintf ( pf,
                  "<td>%+7.3f</td><td>&nbsp;</td></tr>\n",
                  arDouble [ ai[ i ] ] );

    }
    else {
      if ( i )
        fprintf ( pf,
                  "<td>%7.3f%%</td><td>%+7.3f%%</td></tr>\n",
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ), 
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ) -
                  100.0f * eq2mwc( arDouble[ OUTPUT_OPTIMAL ], pci ) );
      else
        fprintf ( pf,
                  "<td>%7.3f%%</td><td>&nbsp;</td></tr>\n",
                  100.0f * eq2mwc( arDouble[ ai[ i ] ], pci ) );
    }

  }

  /* cube decision */

  fprintf ( pf,
            "<tr><td colspan=\"2\">%s</td>"
            "<td colspan=\"2\">%s",
            _("Proper cube action:"),
            GetCubeRecommendation ( FindBestCubeDecision ( arDouble, pci ) ) );

  if ( ( r = getPercent ( FindBestCubeDecision ( arDouble, pci ), 
                          arDouble ) ) >= 0.0 )
    fprintf ( pf, " (%.1f%%)", 100.0f * r );



  fprintf ( pf, "</td></tr>\n" );

  fprintf ( pf,
            "</table>\n" );

}



/*
 * Wrapper for print cube analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *  szImageDir: URI to images
 *  szExtension: extension of images
 *  fTake: take/drop
 *
 */

static void
HTMLPrintCubeAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr,
                        const char *szImageDir, const char *szExtension,
                        const char *szType ) {

  cubeinfo ci;

  GetMatchStateCubeInfo ( &ci, pms );

  switch ( pmr->mt ) {

  case MOVE_NORMAL:

    /* cube analysis from move */

    HTMLPrintCubeAnalysisTable ( pf, pmr->n.arDouble, 
                                 pmr->n.aarOutput, pmr->n.aarStdDev,
                                 pmr->n.fPlayer,
                                 &pmr->n.esDouble, &ci, FALSE, FALSE,
                                 pmr->n.stCube, SKILL_NONE );

    break;

  case MOVE_DOUBLE:

    HTMLPrintCubeAnalysisTable ( pf, pmr->d.arDouble, 
                                 pmr->d.aarOutput, pmr->d.aarStdDev,
                                 pmr->d.fPlayer,
                                 &pmr->d.esDouble, &ci, TRUE, FALSE,
                                 pmr->d.st, SKILL_NONE );

    break;

  case MOVE_TAKE:
  case MOVE_DROP:

    /* cube analysis from double, {take, drop, beaver} */

    HTMLPrintCubeAnalysisTable ( pf, pmr->d.arDouble, 
                                 pmr->d.aarOutput, pmr->d.aarStdDev,
                                 pmr->d.fPlayer,
                                 &pmr->d.esDouble, &ci, TRUE, 
                                 pmr->mt == MOVE_TAKE,
                                 SKILL_NONE, /* FIXME: skill from prev. cube */
                                 pmr->d.st );

    break;

  default:

    assert ( FALSE );


  }

  return;

}


/*
 * Print move analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *  szImageDir: URI to images
 *  szExtension: extension of images
 *
 */

static void
HTMLPrintMoveAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr,
                        const char *szImageDir, const char *szExtension,
                        const char *szType ) {

  char sz[ 64 ];
  int i;
  float rEq, rEqTop;

  cubeinfo ci;

  GetMatchStateCubeInfo( &ci, pms );

  /* check if move should be printed */

  if ( ! exsExport.afMovesDisplay [ pmr->n.stMove ] )
    return;

  /* print alerts */

  if ( pmr->n.stMove <= SKILL_BAD || pmr->n.stMove > SKILL_NONE ) {

    /* blunder or error */

    fputs ( "<p><span class=\"blunder\">", pf );
    fprintf ( pf, 
              _("Alert: %s move"),
              gettext ( aszSkillType[ pmr->n.stMove ] ) );
    
    if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) )
      fprintf ( pf, " (%+7.3f)</span></p>\n",
                pmr->n.ml.amMoves[ pmr->n.iMove ].rScore -
                pmr->n.ml.amMoves[ 0 ].rScore  );
    else
      fprintf ( pf, " (%+6.3f%%)</span></p>\n",
                100.0f *
                eq2mwc ( pmr->n.ml.amMoves[ pmr->n.iMove ].rScore, &ci ) - 
                100.0f *
                eq2mwc ( pmr->n.ml.amMoves[ 0 ].rScore, &ci ) );

  }

  if ( pmr->n.lt != LUCK_NONE ) {

    /* joker */

    fputs ( "<p><span class=\"joker\">", pf );
    fprintf ( pf, 
              _("Alert: %s roll!"),
              gettext ( aszLuckType[ pmr->n.lt ] ) );
    
    if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) )
      fprintf ( pf, " (%+7.3f)</span></p>\n", pmr->n.rLuck );
    else
      fprintf ( pf, " (%+6.3f%%)</span></p>\n",
                100.0f * eq2mwc ( pmr->n.rLuck, &ci ) - 
                100.0f * eq2mwc ( 0.0f, &ci ) );

  }


  /* table header */

  fprintf ( pf,
            "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\""
            " class=\"movetable\">\n"
            "<tr class=\"moveheader\">\n"
            "<th class=\"movenumber\" colspan=\"2\">%s</th>\n"
            "<th class=\"moveply\">%s</th>\n"
            "<th class=\"movemove\">%s</th>\n"
            "<th class=\"moveequity\">%s</th>\n"
            "</tr>\n",
            _("#"),
            _("Ply"),
            _("Move"),
            ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) ) ?
            _("Equity") : _("MWC") );
            

  if ( pmr->n.ml.cMoves ) {

    for ( i = 0; i < pmr->n.ml.cMoves; i++ ) {

      if ( i >= exsExport.nMoves && i != pmr->n.iMove ) 
        continue;

      fprintf ( pf, "<tr %s>\n",
                ( i == pmr->n.iMove ) ? "class=\"movethemove\"" : "" );

      /* selected move or not */
      
      fprintf ( pf, 
                "<td class=\"movenumberdata\">%s</td>\n",
                ( i == pmr->n.iMove ) ? "*" : "&nbsp;" );

      /* move no */

      if ( i != pmr->n.iMove || i != pmr->n.ml.cMoves - 1 ) 
        fprintf ( pf, 
                  "<td>%d</td>\n", i + 1 );
      else
        fprintf ( pf, "<td>\?\?</td>\n" );

      /* ply */

      switch ( pmr->n.ml.amMoves[ i ].esMove.et ) {
      case EVAL_NONE:
        fprintf ( pf,
                  "<td class=\"moveply\">n/a</td>\n" );
        break;
      case EVAL_EVAL:
        fprintf ( pf,
                  "<td class=\"moveply\">%d</td>\n",
                  pmr->n.ml.amMoves[ i ].esMove.ec.nPlies );
        break;
      case EVAL_ROLLOUT:
        fprintf ( pf,
                  "<td class=\"moveply\">R</td>\n" );
        break;
      }

      /* move */

      fprintf ( pf,
                "<td class=\"movemove\">%s</td>\n",
                FormatMove ( sz, pms->anBoard,
                             pmr->n.ml.amMoves[ i ].anMove ) );

      /* equity */

      rEq = pmr->n.ml.amMoves[ i ].rScore;
      rEqTop = pmr->n.ml.amMoves[ 0 ].rScore;

      if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) ) {

        /* output in equity */

        if ( i ) 
          fprintf ( pf,
                    "<td>%+7.3f (%7.3f)</td>\n", rEq, rEq - rEqTop );
        else
          fprintf ( pf,
                    "<td>%+7.3f</td>\n", rEq );

      }
      else {

        /* output in mwc */

        rEq = 100.0f * eq2mwc ( rEq, &ci );
        rEqTop = 100.0f * eq2mwc ( rEqTop, &ci );

        if ( i ) 
          fprintf ( pf,
                    "<td>%7.3f%% (%7.3f%%)</td>\n", rEq, rEq - rEqTop );
        else
          fprintf ( pf,
                    "<td>%7.3f%%</td>\n", rEq );

      }

      /* end row */

      fprintf ( pf, "</tr>\n" );

      /*
       * print row with detailed probabilities 
       */

      if ( exsExport.fMovesDetailProb ) {

        float *ar = pmr->n.ml.amMoves[ i ].arEvalMove;

        fprintf ( pf, "<tr %s>\n",
                  ( i == pmr->n.iMove ) ? "class=\"movethemove\"" : "" );

        fprintf ( pf, "<td colspan=\"3\">&nbsp;</td>\n" );

        fprintf ( pf,
                  "<td colspan=\"2\">"
                  "%s%s%6.2f%% "
                  "%s%s%6.2f%% "
                  "%s%s%6.2f%% "
                  "%s%s%6.2f%% "
                  "%s%s%6.2f%% "
                  "%s%s%6.2f%% "
                  "</td>\n",
                  FORMATHTMLPROB ( ar[ OUTPUT_WINBACKGAMMON ] ),
                  FORMATHTMLPROB ( ar[ OUTPUT_WINGAMMON ] ),
                  FORMATHTMLPROB ( ar[ OUTPUT_WIN ] ),
                  FORMATHTMLPROB ( 1.0 - ar[ OUTPUT_WIN ] ),
                  FORMATHTMLPROB ( ar [ OUTPUT_LOSEGAMMON ] ),
                  FORMATHTMLPROB ( ar [ OUTPUT_LOSEBACKGAMMON ]) );

        fprintf ( pf, "</tr>\n" );

      }

      /*
       * Write row with move parameters 
       */
  
      if ( exsExport.afMovesParameters 
              [ pmr->n.ml.amMoves[ i ].esMove.et - 1 ] ) {

        evalsetup *pes = &pmr->n.ml.amMoves[ i ].esMove;

        fprintf ( pf, "<tr %s>\n",
                  ( i == pmr->n.iMove ) ? "class=\"movethemove\"" : "" );

        fprintf ( pf, "<td colspan=\"3\">&nbsp;</td>\n" );


        switch ( pes->et ) {
        case EVAL_EVAL: 

          fputs ( "<td colspan=\"2\">", pf );
          fprintf ( pf, 
                    _("%d-ply %s "
                    "(%d%% speed, %d cand., %0.3g tol., noise %0.3g )"),
                    pes->ec.nPlies,
                    pes->ec.fCubeful ? _("cubeful") : _("cubeless"),
                    (pes->ec.nReduced) ? 100 / pes->ec.nReduced : 100,
                    pes->ec.nSearchCandidates,
                    pes->ec.rSearchTolerance,
                    pes->ec.rNoise );
          fputs ( "</td>\n", pf );
          break;

        case EVAL_ROLLOUT:

          fprintf ( pf, "<td colspan=\"2\">write me</td>\n" );
          break;

        default:

          break;

        }


        fprintf ( pf, "</tr>\n" );

      }


    }

  }
  else {

    if ( pmr->n.anMove[ 0 ] >= 0 )
      /* no movelist saved */
      fprintf ( pf,
                "<tr class=\"movethemove\"><td>&nbsp;</td><td>&nbsp;</td>"
                "<td>&nbsp;</td><td>%s</td><td>&nbsp;</td></tr>\n",
                FormatMove ( sz, pms->anBoard, pmr->n.anMove ) );
    else 
      /* no legal moves */
      /* FIXME: output equity?? */
      fprintf ( pf,
                "<tr class=\"movethemove\"><td>&nbsp;</td><td>&nbsp;</td>"
                "<td>&nbsp;</td><td>%s</td><td>&nbsp;</td></tr>\n",
                _("Cannot move") );

  }


  fprintf ( pf,
            "</table>\n" );

  return;

}





/*
 * Print cube analysis
 *
 * Input:
 *  pf: output file
 *  pms: match state
 *  pmr: current move record
 *  szImageDir: URI to images
 *  szExtension: extension of images
 *
 */

static void
HTMLAnalysis ( FILE *pf, matchstate *pms, moverecord *pmr,
               const char *szImageDir, const char *szExtension,
               const char *szType ) {

  char sz[ 1024 ];

  switch ( pmr->mt ) {

  case MOVE_NORMAL:

    fprintf ( pf, "<p>" );

    if ( ! strcmp ( szType, "fibs2html" ) )
         printImage ( pf, szImageDir, "b-indent", szExtension, "" );

    if ( pmr->n.anMove[ 0 ] >= 0 )
      fprintf ( pf,
                _("*%s moves %s"),
                ap[ pmr->n.fPlayer ].szName,
                FormatMove ( sz, pms->anBoard, pmr->n.anMove ) );
    else
      fprintf ( pf,
                _("*%s cannot move"),
                ap[ pmr->n.fPlayer ].szName );

    fputs ( "</p>\n", pf );

    // HTMLRollAlert ( pf, pms, pmr, szImageDir, szExtension );

    HTMLPrintCubeAnalysis ( pf, pms, pmr, szImageDir, szExtension, szType );

    HTMLPrintMoveAnalysis ( pf, pms, pmr, szImageDir, szExtension, szType );

    break;

  case MOVE_DOUBLE:

    /* no-op: we assume the following move is MOVE_TAKE or MOVE_DROP */

    break;

  case MOVE_TAKE:
  case MOVE_DROP:

    fprintf ( pf, "<p>" );

    if ( ! strcmp ( szType, "fibs2html" ) )
      printImage ( pf, szImageDir, "b-indent", szExtension, "" );

    fprintf ( pf,
              "*%s %s</p>\n",
              ap[ pmr->d.fPlayer ].szName,
              ( pmr->mt == MOVE_TAKE ) ? _("accepts") : _("rejects") );

    HTMLPrintCubeAnalysis ( pf, pms, pmr, szImageDir, szExtension, szType );

    break;

  default:
    break;

  }

}


/*
 * Dump statcontext
 *
 * Input:
 *   pf: output file
 *   statcontext to dump
 *
 */

extern void HTMLDumpStatcontext ( FILE *pf, const statcontext *psc,
                                  matchstate *pms, const int iGame ) {

  int i;
  ratingtype rt[ 2 ];
  float ar[ 2 ];
  int ai[ 2 ];
  float aar[ 2 ][ 2 ];

  const char *aszLuckRating[] = {
    N_("&quot;Haaa-haaa&quot;"),
    N_("Go to bed"), 
    N_("Better luck next time"),
    N_("None"),
    N_("Good dice, man!"),
    N_("Go to Las Vegas immediately"),
    N_("Cheater :-)")
  };

  if ( iGame >= 0 ) {
    fprintf ( pf,
              "<table class=\"stattable\">\n" );
    printStatTableHeader ( pf,
                           _("Game statistics for game %d"),
                           iGame );
    fprintf ( pf,
              "<tr class=\"stattableheader\">\n"
              "<th>%s</th><th>%s</th><th>%s</th>\n"
              "</tr>\n",
              _("Player"), 
              ap[ 0 ].szName, ap[ 1 ].szName );
  }
  else {
    
    fputs ( "<table class=\"stattable\">\n", pf );

    if ( pms->nMatchTo )
      printStatTableHeader ( pf,
                             _( "Match statistics" ) );
    else
      printStatTableHeader ( pf,
                             _( "Session statistics" ) );

    fprintf ( pf,
              "<tr class=\"stattableheader\">\n"
              "<th>%s</th><th>%s</th><th>%s</th>\n"
              "</tr>\n",
              _("Player"),
              ap[ 0 ].szName, ap[ 1 ].szName );
  }

  /* checker play */

  if( psc->fMoves ) {

    printStatTableHeader ( pf, _("Checker play statistics") );

    printStatTableRow ( pf, 
                        _("Total moves"), "%d",
                        psc->anTotalMoves[ 0 ],
                        psc->anTotalMoves[ 1 ] );
    printStatTableRow ( pf, 
                        _("Unforced moves"), "%d",
                        psc->anUnforcedMoves[ 0 ], 
                        psc->anUnforcedMoves[ 1 ] );
    printStatTableRow ( pf, 
                        _("Moves marked very good"), "%d",
                        psc->anMoves[ 0 ][ SKILL_VERYGOOD ],
                        psc->anMoves[ 1 ][ SKILL_VERYGOOD ] );
    printStatTableRow ( pf, 
                        _("Moves marked good"), "%d",
                        psc->anMoves[ 0 ][ SKILL_GOOD ],
                        psc->anMoves[ 1 ][ SKILL_GOOD ] );
    printStatTableRow ( pf, 
                        _("Moves marked interesting"), "%d",
                        psc->anMoves[ 0 ][ SKILL_INTERESTING ],
                        psc->anMoves[ 1 ][ SKILL_INTERESTING ] );
    printStatTableRow ( pf, 
                        _("Moves unmarked"), "%d",
                        psc->anMoves[ 0 ][ SKILL_NONE ],
                        psc->anMoves[ 1 ][ SKILL_NONE ] );
    printStatTableRow ( pf, 
                        _("Moves marked doubtful"), "%d",
                        psc->anMoves[ 0 ][ SKILL_DOUBTFUL ],
                        psc->anMoves[ 1 ][ SKILL_DOUBTFUL ] );
    printStatTableRow ( pf, 
                        _("Moves marked bad"), "%d",
                        psc->anMoves[ 0 ][ SKILL_BAD ],
                        psc->anMoves[ 1 ][ SKILL_BAD ] );
    printStatTableRow ( pf, 
                        _("Moves marked very bad"), "%d",
                        psc->anMoves[ 0 ][ SKILL_VERYBAD ],
                        psc->anMoves[ 1 ][ SKILL_VERYBAD ] );

    if ( pms->nMatchTo ) {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%%",
                           psc->arErrorCheckerplay[ 0 ][ 0 ],
                           psc->arErrorCheckerplay[ 0 ][ 1 ] * 100.0f,
                           psc->arErrorCheckerplay[ 1 ][ 0 ],
                           psc->arErrorCheckerplay[ 1 ][ 1 ] * 100.0f );
      printStatTableRow2 ( pf,
                           _("Error rate (pr. move)"), 
                           "%+6.3f", "%+7.3f%%",
                           psc->arErrorCheckerplay[ 0 ][ 0 ] /
                           psc->anUnforcedMoves[ 0 ],
                           psc->arErrorCheckerplay[ 0 ][ 1 ] * 100.0f /
                           psc->anUnforcedMoves[ 0 ],
                           psc->arErrorCheckerplay[ 1 ][ 0 ] /
                           psc->anUnforcedMoves[ 1 ],
                           psc->arErrorCheckerplay[ 1 ][ 1 ] * 100.0f /
                           psc->anUnforcedMoves[ 1 ] );
    }
    else {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->arErrorCheckerplay[ 0 ][ 0 ],
                           psc->arErrorCheckerplay[ 0 ][ 1 ],
                           psc->arErrorCheckerplay[ 1 ][ 0 ],
                           psc->arErrorCheckerplay[ 1 ][ 1 ] );

      printStatTableRow2 ( pf,
                           _("Error rate (pr. move)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->arErrorCheckerplay[ 0 ][ 0 ] /
                           psc->anUnforcedMoves[ 0 ],
                           psc->arErrorCheckerplay[ 0 ][ 1 ] /
                           psc->anUnforcedMoves[ 0 ],
                           psc->arErrorCheckerplay[ 1 ][ 0 ] /
                           psc->anUnforcedMoves[ 1 ],
                           psc->arErrorCheckerplay[ 1 ][ 1 ] /
                           psc->anUnforcedMoves[ 1 ] );

    }

    for ( i = 0 ; i < 2; i++ )
      rt[ i ] = GetRating ( psc->arErrorCheckerplay[ i ][ 0 ] /
                            psc->anUnforcedMoves[ i ] );
    
    printStatTableRow ( pf, 
                        _( "Chequer play rating"), "%s",
                        gettext ( aszLuckRating[ rt[ 0 ] ] ), 
                        gettext ( aszLuckRating[ rt[ 1 ] ] ) );

  }

  /* dice stat */

  if( psc->fDice ) {

    printStatTableHeader ( pf, 
                           _( "Luck statistics" ) );

    printStatTableRow ( pf,
                        _("Rolls marked very lucky"), "%d",
                        psc->anLuck[ 0 ][ LUCK_VERYGOOD ],
                        psc->anLuck[ 1 ][ LUCK_VERYGOOD ] );
    printStatTableRow ( pf, _( "Rolls marked lucky" ), "%d",
                        psc->anLuck[ 0 ][ LUCK_GOOD ],
                        psc->anLuck[ 1 ][ LUCK_GOOD ] );
    printStatTableRow ( pf, _( "Rolls unmarked" ), "%d",
                        psc->anLuck[ 0 ][ LUCK_NONE ],
                        psc->anLuck[ 1 ][ LUCK_NONE ] );
    printStatTableRow ( pf, _( "Rolls marked unlucky" ), "%d",
                        psc->anLuck[ 0 ][ LUCK_BAD ],
                        psc->anLuck[ 1 ][ LUCK_BAD ] );
    printStatTableRow ( pf, _( "Rolls marked very unlucky" ), "%d",
                        psc->anLuck[ 0 ][ LUCK_VERYBAD ],
                        psc->anLuck[ 1 ][ LUCK_VERYBAD ] );
       
    if ( pms->nMatchTo ) {

      printStatTableRow2 ( pf, 
                           _( "Luck rate (total)"), 
                           "%+6.3f", "%+7.3f%%",
                           psc->arLuck[ 0 ][ 0 ],
                           psc->arLuck[ 0 ][ 1 ] * 100.0f,
                           psc->arLuck[ 1 ][ 0 ],
                           psc->arLuck[ 1 ][ 1 ] * 100.0f );
      printStatTableRow2 ( pf, 
                           _( "Luck rate (pr. move)"), 
                           "%+6.3f", "%+7.3f%%",
                           psc->arLuck[ 0 ][ 0 ] /
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 0 ][ 1 ] * 100.0f /
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 1 ][ 0 ] /
                           psc->anTotalMoves[ 1 ],
                           psc->arLuck[ 1 ][ 1 ] * 100.0f /
                           psc->anTotalMoves[ 1 ] );
    }
    else {
      printStatTableRow2 ( pf, 
                           _( "Luck rate (total)"), 
                           "%+6.3f", "%+7.3f",
                           psc->arLuck[ 0 ][ 0 ],
                           psc->arLuck[ 0 ][ 1 ],
                           psc->arLuck[ 1 ][ 0 ],
                           psc->arLuck[ 1 ][ 1 ] );
      printStatTableRow2 ( pf, 
                           _( "Luck rate (pr. move)"), 
                           "%+6.3f", "%+7.3f",
                           psc->arLuck[ 0 ][ 0 ] /
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 0 ][ 1 ] /
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 1 ][ 0 ] /
                           psc->anTotalMoves[ 1 ],
                           psc->arLuck[ 1 ][ 1 ] /
                           psc->anTotalMoves[ 1 ] );
    }

    for ( i = 0; i < 2; i++ ) 
      ai[ i ] = getLuckRating ( psc->arLuck[ i ][ 0 ] /
                                psc->anTotalMoves[ i ] );

    printStatTableRow ( pf, 
                        _( "Luck rating"), "%s",
                        gettext ( aszLuckRating[ ai[ 0 ] ] ), 
                        gettext ( aszLuckRating[ ai[ 1 ] ] ) );

  }

  /* cube statistics */

  if( psc->fCube ) {

    printStatTableHeader ( pf,
                           _( "Cube statistics" ) );
    printStatTableRow ( pf, _ ( "Total cube decisions"), "%d",
                        psc->anTotalCube[ 0 ],
                        psc->anTotalCube[ 1 ] );
    printStatTableRow ( pf, _ ( "Doubles"), "%d",
                        psc->anDouble[ 0 ], 
                        psc->anDouble[ 1 ] );
    printStatTableRow ( pf, _( "Takes"), "%d",
                        psc->anTake[ 0 ],
                        psc->anTake[ 1 ] );
    printStatTableRow ( pf, _( "Passes"), "%d",
                        psc->anPass[ 0 ], 
                        psc->anPass[ 1 ] );
      
    if ( pms->nMatchTo ) {

      printStatTableRow3 ( pf, _ ( "Missed doubles around DP" ),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeMissedDoubleDP[ 0 ],
                           psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
                           psc->arErrorMissedDoubleDP[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeMissedDoubleDP[ 1 ],
                           psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
                           psc->arErrorMissedDoubleDP[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Missed doubles around TG" ),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeMissedDoubleTG[ 0 ],
                           psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
                           psc->arErrorMissedDoubleTG[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeMissedDoubleTG[ 1 ],
                           psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
                           psc->arErrorMissedDoubleTG[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongDoubleDP[ 0 ],
                           psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
                           psc->arErrorWrongDoubleDP[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongDoubleDP[ 1 ],
                           psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
                           psc->arErrorWrongDoubleDP[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongDoubleTG[ 0 ],
                           psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
                           psc->arErrorWrongDoubleTG[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongDoubleTG[ 1 ],
                           psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
                           psc->arErrorWrongDoubleTG[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong takes"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongTake[ 0 ],
                           psc->arErrorWrongTake[ 0 ][ 0 ],
                           psc->arErrorWrongTake[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongTake[ 1 ],
                           psc->arErrorWrongTake[ 1 ][ 0 ],
                           psc->arErrorWrongTake[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong passes"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongPass[ 0 ],
                           psc->arErrorWrongPass[ 0 ][ 0 ],
                           psc->arErrorWrongPass[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongPass[ 1 ],
                           psc->arErrorWrongPass[ 1 ][ 0 ],
                           psc->arErrorWrongPass[ 1 ][ 1 ] * 100.0f );
    }
    else {
      printStatTableRow3 ( pf, _ ( "Missed doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeMissedDoubleDP[ 0 ],
                           psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
                           psc->arErrorMissedDoubleDP[ 0 ][ 1 ],
                           psc->anCubeMissedDoubleDP[ 1 ],
                           psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
                           psc->arErrorMissedDoubleDP[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Missed doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeMissedDoubleTG[ 0 ],
                           psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
                           psc->arErrorMissedDoubleTG[ 0 ][ 1 ],
                           psc->anCubeMissedDoubleTG[ 1 ],
                           psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
                           psc->arErrorMissedDoubleTG[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongDoubleDP[ 0 ],
                           psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
                           psc->arErrorWrongDoubleDP[ 0 ][ 1 ],
                           psc->anCubeWrongDoubleDP[ 1 ],
                           psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
                           psc->arErrorWrongDoubleDP[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongDoubleTG[ 0 ],
                           psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
                           psc->arErrorWrongDoubleTG[ 0 ][ 1 ],
                           psc->anCubeWrongDoubleTG[ 1 ],
                           psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
                           psc->arErrorWrongDoubleTG[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong takes"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongTake[ 0 ],
                           psc->arErrorWrongTake[ 0 ][ 0 ],
                           psc->arErrorWrongTake[ 0 ][ 1 ],
                           psc->anCubeWrongTake[ 1 ],
                           psc->arErrorWrongTake[ 1 ][ 0 ],
                           psc->arErrorWrongTake[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong passes"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongPass[ 0 ],
                           psc->arErrorWrongPass[ 0 ][ 0 ],
                           psc->arErrorWrongPass[ 0 ][ 1 ],
                           psc->anCubeWrongPass[ 1 ],
                           psc->arErrorWrongPass[ 1 ][ 0 ],
                           psc->arErrorWrongPass[ 1 ][ 1 ] );
    }

    for ( i = 0 ; i < 2; i++ )
      rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                              + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                              + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                              + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                              + psc->arErrorWrongTake[ i ][ 0 ]
                              + psc->arErrorWrongPass[ i ][ 0 ] ) /
                            psc->anTotalCube[ i ] );
      
    printStatTableRow ( pf, 
                        _( "Cube decision rating"), "%s",
                        gettext ( aszRating[ rt [ 0 ] ] ), 
                        gettext ( aszRating[ rt [ 1 ] ] ) );
  }
    
  /* overall rating */
    
  if( psc->fMoves && psc->fCube ) {

    cubeinfo ci;
    
    for ( i = 0 ; i < 2; i++ ) {
      rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                              + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                              + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                              + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                              + psc->arErrorWrongTake[ i ][ 0 ]
                              + psc->arErrorWrongPass[ i ][ 0 ]
                              + psc->arErrorCheckerplay[ i ][ 0 ] ) /
                            ( psc->anTotalCube[ i ] +
                              psc->anUnforcedMoves[ i ] ) );
      
      ar[ i ] = psc->arErrorMissedDoubleDP[ i ][ 1 ]
        + psc->arErrorMissedDoubleTG[ i ][ 1 ]
        + psc->arErrorWrongDoubleDP[ i ][ 1 ]
        + psc->arErrorWrongDoubleTG[ i ][ 1 ]
        + psc->arErrorWrongTake[ i ][ 1 ]
        + psc->arErrorWrongPass[ i ][ 1 ]
        + psc->arErrorCheckerplay[ i ][ 1 ];

    }
     
    printStatTableHeader ( pf,
                           _("Overall rating" ) );
                            
    printStatTableRow ( pf, 
                        _("Overall rating"), "%s",
                        gettext ( aszRating[ rt [ 0 ] ] ), 
                        gettext ( aszRating[ rt [ 1 ] ] ) );

    SetCubeInfo( &ci, pms->nCube, pms->fCubeOwner, 0,
                 pms->nMatchTo, pms->anScore, pms->fCrawford,
                 fJacoby, nBeavers );

    if ( pms->nMatchTo ) {

      /* skill */

      getMWCFromError ( psc, aar );

      printStatTableRow ( pf,
                          _( "MWC against current opponent"),
                          "%6.2f%%",
                          100.0 * aar[ 0 ][ 0 ], 
                          100.0 * aar[ 1 ][ 0 ] );

      printStatTableRow ( pf,
                          _( "Relative rating"),
                          "%6.2f",
                          relativeFibsRating ( aar[ 0 ][ 0 ], pms->nMatchTo ),
                          relativeFibsRating ( aar[ 1 ][ 0 ], pms->nMatchTo ) );

      printStatTableRow ( pf,
                          _( "MWC against perfect opponent"),
                          "%6.2f%%",
                          100.0 * aar[ 0 ][ 1 ], 
                          100.0 * aar[ 1 ][ 1 ] );

      printStatTableRow ( pf,
                          _( "Relative rating"),
                          "%6.2f",
                          relativeFibsRating ( aar[ 0 ][ 1 ], pms->nMatchTo ),
                          relativeFibsRating ( aar[ 1 ][ 1 ], pms->nMatchTo ) );


    }
  
  }

  fprintf ( pf, "</table>\n" );



}


static void
HTMLPrintComment ( FILE *pf, const moverecord *pmr ) {

  char *sz = NULL;

  switch ( pmr->mt ) {

  case MOVE_GAMEINFO:
    sz = pmr->g.sz;
    break;
  case MOVE_DOUBLE:
  case MOVE_TAKE:
  case MOVE_DROP:
    sz = pmr->d.sz;
    break;
  case MOVE_NORMAL:
    sz = pmr->n.sz;
    break;
  case MOVE_RESIGN:
    sz = pmr->r.sz;
    break;
  case MOVE_SETBOARD:
    sz = pmr->sb.sz;
    break;
  case MOVE_SETDICE:
    sz = pmr->sd.sz;
    break;
  case MOVE_SETCUBEVAL:
    sz = pmr->scv.sz;
    break;
  case MOVE_SETCUBEPOS:
    sz = pmr->scp.sz;
    break;

  }

  if ( sz ) {

    fputs ( "<br />\n"
            "<div class=\"commentheader\">", pf );
    fputs ( _("Annotation"), pf );
    fputs ( "</div>\n", pf );

    fputs ( "<div class=\"comment\">", pf );

    while ( *sz ) {

      if ( *sz == '\n' )
        fputs ( "<br />\n", pf );
      else
        fputc ( *sz, pf );

      sz++;

    }

    fputs ( "</div>\n", pf );

    

  }


}


/*
 * Export a game in HTML
 *
 * Input:
 *   pf: output file
 *   plGame: list of moverecords for the current game
 *
 */

static void ExportGameHTML ( FILE *pf, list *plGame, const char *szImageDir,
                             const char *szExtension, const char *szType,
                             char *aszColorName[], 
                             const int iGame, const int fLastGame,
                             char *aszLinks[ 4 ] ) {

    list *pl;
    moverecord *pmr;
    matchstate msExport;
    matchstate msOrig;
    int iMove = 0;
    statcontext *psc = NULL;
    static statcontext scTotal;
    movegameinfo *pmgi = NULL;

    if ( ! iGame )
      IniStatcontext ( &scTotal );

    updateStatisticsGame ( plGame );

    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {

      pmr = pl->p;
      
      switch( pmr->mt ) {

      case MOVE_GAMEINFO:

        ApplyMoveRecord ( &msExport, pmr );

        HTMLPrologue( pf, &msExport, iGame, aszLinks );

        msOrig = msExport;
        pmgi = &pmr->g;

        psc = &pmr->g.sc;

        AddStatcontext ( psc, &scTotal );
    
        /* FIXME: game introduction */
        break;

      case MOVE_NORMAL:

	if( pmr->n.fPlayer != msExport.fMove ) {
	    SwapSides( msExport.anBoard );
	    msExport.fMove = pmr->n.fPlayer;
	}
      
        msExport.fTurn = msExport.fMove = pmr->n.fPlayer;
        msExport.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
        msExport.anDice[ 1 ] = pmr->n.anRoll[ 1 ];

        HTMLBoardHeader ( pf, &msExport, aszColorName, iGame, iMove );

        printHTMLBoard( pf, &msExport, msExport.fTurn, 
                        szImageDir, szExtension, szType );
        HTMLAnalysis ( pf, &msExport, pmr, szImageDir, szExtension, szType );
        
        iMove++;

        break;

      case MOVE_TAKE:
      case MOVE_DROP:

        HTMLBoardHeader ( pf,&msExport, aszColorName, iGame, iMove );

        printHTMLBoard( pf, &msExport, msExport.fTurn, 
                        szImageDir, szExtension, szType );
        
        HTMLAnalysis ( pf, &msExport, pmr, szImageDir, szExtension, szType );
        
        iMove++;

        break;


      default:

        break;
        
      }

      HTMLPrintComment ( pf, pmr );

      ApplyMoveRecord ( &msExport, pmr );

    }

    if( pmgi && pmgi->fWinner != -1 ) {

      /* print game result */

      if ( pmgi->nPoints > 1 )
        fprintf ( pf, 
                  _("%s%s wins %d points%s"),
                  "<p class=\"result\">",
                  ap[ pmgi->fWinner ].szName, 
                  pmgi->nPoints,
                  "</p>\n");
      else
        fprintf ( pf, 
                  _("%s%s wins %d point%s"),
                  "<p class=\"result\">",
                  ap[ pmgi->fWinner ].szName, 
                  pmgi->nPoints,
                  "</p>\n");

    }

    if ( psc )
      HTMLDumpStatcontext ( pf, psc, &msOrig, iGame );


    if ( fLastGame ) {

      fprintf ( pf, "<hr />\n" );
      HTMLDumpStatcontext ( pf, &scTotal, &msOrig, -1 );

    }


    HTMLEpilogue( pf, &ms, aszLinks );
    
}

/*
 * get name number
 *
 * Input
 *    plGame: the game for which the game number is requested
 *
 * Returns:
 *    the game number
 *
 */

extern int
getGameNumber ( const list *plGame ) {

  list *pl;
  int iGame;

  for( pl = lMatch.plNext, iGame = 0; pl != &lMatch; 
       pl = pl->plNext, iGame++ )
    if ( pl->p == plGame ) return iGame;

  return -1;

}


/*
 * get move number
 *
 * Input
 *    plGame: the game 
 *    p: the move
 *
 * Returns:
 *    the move number
 *
 */

extern int
getMoveNumber ( const list *plGame, const void *p ) {

  list *pl;
  int iMove;

  for( pl = plGame->plNext, iMove = 0; pl != plGame; 
       pl = pl->plNext, iMove++ )
    if ( p == pl->p ) return iMove;

  return -1;

}


extern void CommandExportGameHtml( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export"
		 "game html').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /*
    ExportGameHTML( pf, plGame,
                    "http://fibs2html.sourceforge.net/images/", "gif", 
                    "fibs2html", 
                    aszColorNameF2H,
                    getGameNumber ( plGame ), FALSE, 
                    NULL );
    ExportGameHTML( pf, plGame,
                    "http://www.dbgf.dk/bbs/images/", "png", 
                    "bbs", 
                    aszColorNameBBS,
                    getGameNumber ( plGame ), FALSE, 
                    NULL );
    */
    ExportGameHTML( pf, plGame,
                     exsExport.szHTMLPictureURL, "png", 
                    "gnu", 
                    aszColorNameBBS,
                    getGameNumber ( plGame ), FALSE, 
                    NULL );
    

    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_HTML );
    
}

/*
 * Get filename for html file:
 *
 * Number 0 gets the value of szBase.
 * Assume szBase is "blah.html", then number 1 will be
 * "blah_001.html", and so forth.
 *
 * Input:
 *   szBase: base filename
 *   iGame: game number
 *
 * Garbage collect:
 *   Caller must free returned pointer if not NULL
 * 
 */

extern char *
HTMLFilename ( const char *szBase, const int iGame ) {

  if ( ! iGame )
    return strdup ( szBase );
  else {

    char *sz = malloc ( strlen ( szBase ) + 5 );
    char *szExtension = strrchr ( szBase, '.' );
    char *pc;

    if ( ! szExtension ) {

      sprintf ( sz, "%s_%03d", szBase, iGame );
      return sz;

    }
    else {

      strcpy ( sz, szBase );
      pc = strrchr ( sz, '.' );
      
      sprintf ( pc, "_%03d%s", iGame, szExtension );

      return sz;

    }

  }

}



extern void CommandExportMatchHtml( char *sz ) {
    
    FILE *pf;
    list *pl;
    int nGames;
    char *aszLinks[ 4 ];
    char *szCurrent;
    int i, j;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export "
		 "match html').") );
	return;
    }

    /* Find number of games in match */

    for( pl = lMatch.plNext, nGames = 0; pl != &lMatch; 
         pl = pl->plNext, nGames++ )
      ;

    for( pl = lMatch.plNext, i = 0; pl != &lMatch; pl = pl->plNext, i++ ) {

      szCurrent = HTMLFilename ( sz, i );
      aszLinks[ 0 ] = HTMLFilename ( sz, 0 );
      aszLinks[ 1 ] = ( i > 0 ) ? HTMLFilename ( sz, i - 1 ) : NULL;
      aszLinks[ 2 ] = ( i < nGames - 1 ) ? HTMLFilename ( sz, i + 1 ) : NULL;
      aszLinks[ 3 ] = HTMLFilename ( sz, nGames - 1 );

      if ( !i ) {

        if ( ! confirmOverwrite ( sz, fConfirmSave ) )
          return;

        setDefaultFileName ( sz, PATH_HTML );

      }


      if( !strcmp( szCurrent, "-" ) )
	pf = stdout;
      else if( !( pf = fopen( szCurrent, "w" ) ) ) {
	perror( szCurrent );
	return;
      }

#ifdef UNDEF
      ExportGameHTML ( pf, pl->p, 
                       "http://fibs2html.sourceforge.net/images/", "gif",
                       "fibs2html", 
                       aszColorNameF2H,
                       i, i == nGames - 1,
                       aszLinks );
#endif

      ExportGameHTML ( pf, pl->p, 
                       exsExport.szHTMLPictureURL, "png",
                       "gnu", 
                       aszColorNameGNU,
                       i, i == nGames - 1,
                       aszLinks );

      for ( j = 0; j < 4; j++ )
        if ( aszLinks[ j ] )
          free ( aszLinks[ j ] );
      free ( szCurrent );

      if( pf != stdout )
        fclose( pf );

    }
    
}


extern void CommandExportPositionHtml( char *sz ) {

    FILE *pf;
	
    sz = NextToken( &sz );
    
    if( ms.gs == GAME_NONE ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help export "
		 "position html').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    HTMLPrologue ( pf, &ms, getGameNumber ( plGame ), NULL );

    HTMLBoardHeader ( pf, &ms, aszColorNameF2H,
                      getGameNumber ( plGame ),
                      getMoveNumber ( plGame, plLastMove->plNext->p ) - 1 );

    printHTMLBoard( pf, &ms, ms.fTurn,
                     exsExport.szHTMLPictureURL, "png",
                    "gnu" );

    if( plLastMove->plNext->p != NULL)
        HTMLAnalysis ( pf, &ms, plLastMove->plNext->p,
                       exsExport.szHTMLPictureURL, "png",
                       "gnu" );
    
    HTMLEpilogue ( pf, &ms, NULL );

    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_HTML );

}

