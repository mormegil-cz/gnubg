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

#if defined (HAVE_BASENAME) && defined (HAVE_LIBGEN_H)
#include "libgen.h"
#endif

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
".moveodd { background-color: #d0d0d0 } \n" \
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
".number { text-align: center; font-weight: bold; font-size: 60%%; " \
"font-family: sans-serif } \n" \
".percent { text-align: right } \n" \
"</style>\n" 

#define FORMATHTMLPROB(f) \
( ( f ) < 1.0f ) ? "&nbsp;" : "", \
( ( f ) < 0.1f ) ? "&nbsp;" : "", \
( f ) * 100.0f
   

char *aszHTMLExportType[ NUM_HTML_EXPORT_TYPES ] = {
  "gnu",
  "bbs",
  "fibs2html" };



/* text for links on html page */

static char *aszLinkText[] = {
  N_ ("[First Game]"), 
  N_ ("[Previous Game]"), 
  N_ ("[Next Game]"), 
  N_ ("[Last Game]") };

/* Color of chequers */

static char *aaszColorName[ NUM_HTML_EXPORT_TYPES ][ 2 ] = {
  { N_("red"), N_("black") },
  { N_("white"), N_("blue") },
  { N_("white"), N_("red") }
};


static void
printRolloutTable ( FILE *pf, 
                    char asz[][ 1024 ],
                    float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                    float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                    const cubeinfo aci[],
                    const int cci,
                    const int fCubeful,
                    const int fHeader ) {

  int ici;

  fputs ( "<table>\n", pf );

  if ( fHeader ) {

    fputs ( "<tr>", pf );

    if ( asz )
      fputs ( "<td>&nbsp;</td>", pf );

    fprintf ( pf,
              "<td>%s</td>"
              "<td>%s</td>"
              "<td>%s</td>"
              "<td>&nbsp;</td>"
              "<td>%s</td>"
              "<td>%s</td>"
              "<td>%s</td>"
              "<td>%s</td>",
              _("Win"),
              _("W g"),
              _("W bg"),
              _("Lose"),
              _("L g"),
              _("L bg"),
              _("Cubeless") );

    if ( fCubeful )
      fprintf ( pf, "<td>%s</td>", _("Cubeful") );

    fputs ( "</tr>\n", pf );

  }

  for ( ici = 0; ici < cci; ici++ ) {

    fputs ( "<tr>", pf );

    /* output */

    if ( asz )
      fprintf ( pf, "<td>%s</td>", asz[ ici ] );

    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarOutput[ ici ][ OUTPUT_WIN ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarOutput[ ici ][ OUTPUT_WINGAMMON ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarOutput[ ici ][ OUTPUT_WINBACKGAMMON ] ) );
    
    fputs ( "<td>-</td>", pf );
    
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( 1.0f - aarOutput[ ici ][ OUTPUT_WIN ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarOutput[ ici ][ OUTPUT_LOSEGAMMON ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarOutput[ ici ][ OUTPUT_LOSEBACKGAMMON ] ) );
    
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputEquityScale ( aarOutput[ ici ][ OUTPUT_EQUITY ], 
                             &aci[ ici ], &aci[ 0 ], TRUE ) );

    if ( fCubeful )
      fprintf ( pf, "<td class=\"percent\">%s</td>", 
                OutputMWC ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], 
                            &aci[ 0 ], TRUE ) );

    fputs ( "</tr>\n", pf );

    /* stddev */

    fputs ( "<tr>", pf );

    if ( asz )
      fprintf ( pf, "<td>%s</td>", _("Standard error") );

    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_WIN ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_WINGAMMON ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_WINBACKGAMMON ] ) );
    
    fputs ( "<td>-</td>", pf );
    
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_WIN ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_LOSEGAMMON ] ) );
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputPercent ( aarStdDev[ ici ][ OUTPUT_LOSEBACKGAMMON ] ) );
    
    fprintf ( pf, "<td class=\"percent\">%s</td>", 
              OutputEquityScale ( aarStdDev[ ici ][ OUTPUT_EQUITY ], 
                             &aci[ ici ], &aci[ 0 ], FALSE ) );
    
    if ( fCubeful )
      fprintf ( pf, "<td class=\"percent\">%s</td>", 
                OutputMWC ( aarStdDev[ ici ][ OUTPUT_CUBEFUL_EQUITY ], 
                            &aci[ 0 ], FALSE ) );

    fputs ( "</tr>\n", pf );

  }

  fputs ( "</table>\n", pf );

}


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


static void
printStatTableRow4 ( FILE *pf, const char *format1, const char *format2,
                     const char *format3, 
                     const int f0, const float r00, const float r01,
                     const int f1, const float r10, const float r11 ) {
                     
  fprintf ( pf, 
            "<tr>\n"
            "<td>%s</td>\n",
            format1 );

  fputs ( "<td>", pf );

  if ( f0 ) {
    fprintf ( pf, format2, r00 );
    fputs ( " (", pf );
    fprintf ( pf, format3, r01 );
    fputs ( " )", pf );
  }
  else
    fputs ( _("n/a"), pf );

  fputs ( "</td>\n<td>", pf );

  if ( f1 ) {
    fprintf ( pf, format2, r10 );
    fputs ( " (", pf );
    fprintf ( pf, format3, r11 );
    fputs ( " )", pf );
  }
  else
    fputs ( _("n/a"), pf );

  fputs ( "</td>\n</tr>\n", pf );

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
                    const char *szImageDir, const char *szExtension ) {

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

  sprintf ( sz, "o_w_%d", acOff[ 1 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 0's inner board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ i ],
                    anBoard[ 0 ][ 23 - i ],
                    ! ( i % 2 ), TRUE );

  /* player 0's chequers on the bar */

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

  if ( pms->fCubeOwner == -1 )
    printImage ( pf, szImageDir, "c_center", szExtension, NULL );
  else
    printImage ( pf, szImageDir, "c_blank", szExtension, NULL );

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
                    ( i % 2 ), FALSE );

  /* player 1's chequers on the bar */

  sprintf ( sz, "b_dn_%d", anBoard[ 1 ][ 24 ] );
  printImage ( pf, szImageDir, sz, szExtension, NULL );

  /* player 1's outer board */

  for ( i = 0; i < 6; i++ )
    printPointBBS ( pf, szImageDir, szExtension, 
                    anBoard[ 1 ][ 17 - i ],
                    anBoard[ 0 ][ i + 6 ],
                    ( i % 2 ), FALSE );

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
                    const char *szImageDir, const char *szExtension ) {

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
              iPoint0 );
    sprintf ( szAlt, "%1xX", iPoint0 );
    
  }
  else if ( iPoint1 ) {

    /* player 1 owns the point */

    sprintf ( sz, "b-%c%c-o%d", fColor ? 'g' : 'r', fUp ? 'd' : 'u',
              iPoint1 );
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
printNumbers ( FILE *pf, const int fTop ) {

  int i;

  if ( fTop ) {

    fputs ( "<tr><td>&nbsp;</td>", pf );
    for ( i = 13; i <= 18; i++ )
      fprintf ( pf, "<td class=\"number\">%d</td>", i );
    fputs ( "<td>&nbsp;</td>", pf );
    for ( i = 19; i <= 24; i++ )
      fprintf ( pf, "<td class=\"number\">%d</td>", i );
    fputs ( "</tr>\n", pf );

  }
  else {

    fputs ( "<tr><td>&nbsp;</td>", pf );
    for ( i = 12; i >= 7; i-- )
      fprintf ( pf, "<td class=\"number\">%d</td>", i );
    fputs ( "<td>&nbsp;</td>", pf );
    for ( i = 6; i >= 1; i-- )
      fprintf ( pf, "<td class=\"number\">%d</td>", i );
    fputs ( "</tr>\n", pf );

  }

}



static void
printHTMLBoardGNU ( FILE *pf, matchstate *pms, int fTurn,
                    const char *szImageDir, const char *szExtension ) {

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
  printNumbers ( pf, fTurn );

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
  if ( acOff[ 1 ] ) 
    sprintf ( sz, "b-roff-x%d", acOff[ 1 ] );
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

  printNumbers ( pf, ! fTurn );

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
                 const htmlexporttype het ) {

  switch ( het ) {
  case HTML_EXPORT_TYPE_FIBS2HTML:
    printHTMLBoardF2H ( pf, pms, fTurn, szImageDir, szExtension );
    break;
  case HTML_EXPORT_TYPE_BBS:
    printHTMLBoardBBS ( pf, pms, fTurn, szImageDir, szExtension );
    break;
  case HTML_EXPORT_TYPE_GNU:
    printHTMLBoardGNU ( pf, pms, fTurn, szImageDir, szExtension );
    break;
  default:
    printf ( _("unknown board type\n") );
    break;
  }


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
                  const htmlexporttype het,
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
              gettext ( aaszColorName[ het ][ pms->fTurn ] ),
              pms->fResigned * pms->nCube
            );
  
  else if ( pms->anDice[ 0 ] && pms->anDice[ 1 ] )

    /* chequer play decision */

    fprintf ( pf,
              _(" %s to play %d%d"),
              gettext ( aaszColorName[ het ][ pms->fMove ] ),
              pms->anDice[ 0 ], pms->anDice[ 1 ] 
            );

  else if ( pms->fDoubled )

    /* take decision */

    fprintf ( pf,
              _(" %s doubles to %d"),
              gettext ( aaszColorName[ het ][ pms->fMove ] ),
              pms->nCube * 2
            );

  else

    /* cube decision */

    fprintf ( pf,
              _(" %s onroll, cube decision?"),
              gettext ( aaszColorName[ het ][ pms->fMove ] ) );


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
               char *aszLinks[ 4 ],
               const htmlexporttype het ) {

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
              _("%s (%s, %d pts) vs. %s (%s, %d pts) (Match to %d)"),
              aaszColorName[ het ][ 0 ],
              ap [ 0 ].szName, pms->anScore[ 0 ],
              aaszColorName[ het ][ 1 ],
              ap [ 1 ].szName, pms->anScore[ 1 ],
              pms->nMatchTo );
  else
    sprintf ( szHeader,
              _("%s (%s, %d pts) vs. %s (%s, %d pts) (money game)"),
              aaszColorName[ het ][ 0 ],
              ap [ 0 ].szName, pms->anScore[ 0 ],
              aaszColorName[ het ][ 1 ],
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

  fputs ( " ", pf );
            
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
            "<a href=\"http://jigsaw.w3.org/css-validator/check/referer\">"
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

  cubedecision cd;

  /* check if cube analysis should be printed */

  if ( pes->et == EVAL_NONE ) return; /* no evaluation */
  if ( ! GetDPEq ( NULL, NULL, pci ) ) return; /* cube not available */

  fActual = fDouble;
  fClose = isCloseCubedecision ( arDouble ); 
  fMissed = isMissedDouble ( arDouble, aarOutput, fDouble, pci );

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

    fprintf ( pf, "<p><span class=\"blunder\">%s (%s)!",
              _("Alert: missed double"),
              OutputEquityDiff ( arDouble[ OUTPUT_NODOUBLE ], 
                                ( arDouble[ OUTPUT_TAKE ] > 
                                   arDouble[ OUTPUT_DROP ] ) ? 
                                 arDouble[ OUTPUT_DROP ] : 
                                 arDouble[ OUTPUT_TAKE ],
                                 pci ) );
    
    if ( stDouble != SKILL_NONE )
      fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );
    
    fprintf ( pf, "</span></p>\n" );

  }

  r = arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_DROP ];

  if ( fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong take */

    fprintf ( pf, "<p><span class=\"blunder\">%s (%s)!",
              _("Alert: wrong take"),
              OutputEquityDiff ( arDouble[ OUTPUT_DROP ],
                                 arDouble[ OUTPUT_TAKE ],
                                 pci ) );

    if ( stTake != SKILL_NONE )
      fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );
    
    fprintf ( pf, "</span></p>\n" );

  }

  r = arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && ! fTake && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong pass */

    fprintf ( pf, "<p><span class=\"blunder\">%s (%s)!",
              _("Alert: wrong pass"),
              OutputEquityDiff ( arDouble[ OUTPUT_TAKE ],
                                 arDouble[ OUTPUT_DROP ],
                                 pci ) );

    if ( stTake != SKILL_NONE )
      fprintf ( pf, " [%s]", gettext ( aszSkillType[ stTake ] ) );
    
    fprintf ( pf, "</span></p>\n" );

  }


  if ( arDouble[ OUTPUT_TAKE ] > arDouble[ OUTPUT_DROP ] )
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_DROP ];
  else
    r = arDouble[ OUTPUT_NODOUBLE ] - arDouble[ OUTPUT_TAKE ];

  if ( fDouble && r > 0.0f ) {

    fAnno = TRUE;

    /* wrong double */

    fprintf ( pf, "<p><span class=\"blunder\">%s (%s)!",
              _("Alert: wrong double"),
              OutputEquityDiff ( ( arDouble[ OUTPUT_TAKE ] > 
                                   arDouble[ OUTPUT_DROP ] ) ? 
                                 arDouble[ OUTPUT_DROP ] : 
                                 arDouble[ OUTPUT_TAKE ],
                                 arDouble[ OUTPUT_NODOUBLE ], 
                                 pci ) );

    if ( stDouble != SKILL_NONE )
      fprintf ( pf, " [%s]", gettext ( aszSkillType[ stDouble ] ) );

    fprintf ( pf, "</span></p>\n" );

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

  fputs ( "<td colspan=\"2\">", pf );
  
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

  /* cubeless equity */

  if ( pci->nMatchTo ) 
    fprintf ( pf, " %s</td><td>%s</td><td>(%s: %s)</td>\n",
              ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) ?
              _("cubeless equity") : _("cubeless MWC"),
              OutputEquity ( aarOutput[ 0 ][ OUTPUT_EQUITY ], pci, TRUE ),
              _("Money"), 
              OutputMoneyEquity ( aarOutput[ 0 ], TRUE ) );
  else
    fprintf ( pf, " %s</td><td>%s</td><td>&nbsp;</td>\n",
              ( !pci->nMatchTo || ( pci->nMatchTo && ! fOutputMWC ) ) ?
              _("cubeless equity") : _("cubeless MWC"),
              OutputMoneyEquity ( aarOutput[ 0 ], TRUE ) );


  fprintf ( pf, "</tr>\n" );

  /* percentages */

  if ( exsExport.fCubeDetailProb && pes->et == EVAL_EVAL ) {

    fputs ( "<tr><td>&nbsp;</td>"
            "<td colspan=\"3\">", pf );

    fputs ( OutputPercents ( aarOutput[ 0 ], TRUE ), pf );

    fputs ( "</td></tr>\n", pf );

  }

  /* equities */

  getCubeDecisionOrdering ( ai, arDouble, aarOutput, pci );

  for ( i = 0; i < 3; i++ ) {

    fprintf ( pf,
              "<tr><td>%d.</td><td>%s</td>", i + 1, 
              gettext ( aszCube[ ai[ i ] ] ) );

    fprintf ( pf,
              "<td>%s</td>",
              OutputEquity ( arDouble[ ai [ i ] ], pci, TRUE ) );

    if ( i )
      fprintf ( pf,
                "<td>%s</td>",
                OutputEquityDiff ( arDouble[ ai [ i ] ], 
                                   arDouble[ OUTPUT_OPTIMAL ], pci ) );
    else
      fputs ( "<td>&nbsp;</td>", pf );

    fputs ( "</tr>\n", pf );
      
  }

  /* cube decision */

  cd = FindBestCubeDecision ( arDouble, aarOutput, pci );

  fprintf ( pf,
            "<tr><td colspan=\"2\">%s</td>"
            "<td colspan=\"2\">%s",
            _("Proper cube action:"),
            GetCubeRecommendation ( cd ) );

  if ( ( r = getPercent ( cd, arDouble ) ) >= 0.0 )
    fprintf ( pf, " (%.1f%%)", 100.0f * r );



  fprintf ( pf, "</td></tr>\n" );

  /* rollout details */

  if ( pes->et == EVAL_ROLLOUT && 
       ( exsExport.fCubeDetailProb || exsExport.afCubeParameters [ 1 ] ) ) {

    fprintf ( pf, 
              "<tr><th colspan=\"4\">%s</th></tr>\n",
              _("Rollout details") );

  }

  if ( pes->et == EVAL_ROLLOUT && exsExport.fCubeDetailProb ) {

    char asz[ 2 ][ 1024 ];
    cubeinfo aci[ 2 ];

    for ( i = 0; i < 2; i++ ) {

      memcpy ( &aci[ i ], pci, sizeof ( cubeinfo ) );

      if ( i ) {
        aci[ i ].fCubeOwner = ! pci->fMove;
        aci[ i ].nCube *= 2;
      }

      FormatCubePosition ( asz[ i ], &aci[ i ] );

    }

    fputs ( "<tr>"
            "<td colspan=\"4\">", pf );
    
    printRolloutTable ( pf, asz, aarOutput, aarStdDev, aci, 2, 
                        pes->rc.fCubeful, TRUE );

    fputs ( "</td></tr>\n", pf );

  }

  if ( pes->et == EVAL_ROLLOUT && exsExport.afCubeParameters[ 1 ] ) {

    char *sz = strdup ( OutputRolloutContext ( NULL, &pes->rc ) );
    char *pcS = sz, *pcE;

    while ( ( pcE = strstr ( pcS, "\n" ) ) ) {

      *pcE = 0;
      fprintf ( pf, 
                "<tr><td colspan=\"4\">%s</td></tr>\n",
                pcS );
      pcS = pcE + 1;

    }

    free ( sz );


  }

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
                        const htmlexporttype het ) {

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
                        const htmlexporttype het ) {

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

      if ( i == pmr->n.iMove )
        fputs ( "<tr class=\"movethemove\">\n", pf );
      else if ( i % 2 )
        fputs ( "<tr class=\"moveodd\">\n", pf );
      else
        fputs ( "<tr>\n", pf );

      /* selected move or not */
      
      fprintf ( pf, 
                "<td class=\"movenumberdata\">%s</td>\n",
                ( i == pmr->n.iMove ) ? "*" : "&nbsp;" );

      /* move no */

      if ( i != pmr->n.iMove || i != pmr->n.ml.cMoves - 1 || 
           pmr->n.ml.cMoves == 1 ) 
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

      if ( i ) 
        fprintf ( pf,
                  "<td>%s (%s)</td>\n", 
                  OutputEquity ( rEq, &ci, TRUE ), 
                  OutputEquityDiff ( rEq, rEqTop, &ci ) );
      else
        fprintf ( pf,
                  "<td>%s</td>\n", 
                  OutputEquity ( rEq, &ci, TRUE ) );

      /* end row */

      fprintf ( pf, "</tr>\n" );

      /*
       * print row with detailed probabilities 
       */

      if ( exsExport.fMovesDetailProb ) {

        float *ar = pmr->n.ml.amMoves[ i ].arEvalMove;

        /* percentages */

        if ( i == pmr->n.iMove )
          fputs ( "<tr class=\"movethemove\">\n", pf );
        else if ( i % 2 )
          fputs ( "<tr class=\"moveodd\">\n", pf );
        else
          fputs ( "<tr>\n", pf );

        fputs ( "<td colspan=\"3\">&nbsp;</td>\n", pf );


        fputs ( "<td colspan=\"2\">", pf );


        switch ( pmr->n.ml.amMoves[ i ].esMove.et ) {
        case EVAL_EVAL:
          fputs ( OutputPercents ( ar, TRUE ), pf );
          break;
        case EVAL_ROLLOUT:
          printRolloutTable ( pf, NULL,
                              ( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) 
                              pmr->n.ml.amMoves[ i ].arEvalMove,
                              ( float (*)[ NUM_ROLLOUT_OUTPUTS ] ) 
                              pmr->n.ml.amMoves[ i ].arEvalStdDev,
                              &ci,
                              1,
                              pmr->n.ml.amMoves[ i ].esMove.rc.fCubeful,
                              FALSE );
          break;
        default:
          break;
        }

        fputs ( "</td>\n", pf );

        fputs ( "</tr>\n", pf );
      }

      /*
       * Write row with move parameters 
       */
  
      if ( exsExport.afMovesParameters 
              [ pmr->n.ml.amMoves[ i ].esMove.et - 1 ] ) {

        evalsetup *pes = &pmr->n.ml.amMoves[ i ].esMove;

        switch ( pes->et ) {
        case EVAL_EVAL: 

          if ( i == pmr->n.iMove )
            fputs ( "<tr class=\"movethemove\">\n", pf );
          else if ( i % 2 )
            fputs ( "<tr class=\"moveodd\">\n", pf );
          else
            fputs ( "<tr>\n", pf );
          
          fprintf ( pf, "<td colspan=\"3\">&nbsp;</td>\n" );

          fputs ( "<td colspan=\"2\">", pf );
          fputs ( OutputEvalContext ( &pes->ec, TRUE ), pf );
          fputs ( "</td>\n", pf );

          fputs ( "</tr>\n", pf );

          break;

        case EVAL_ROLLOUT: {

          char *sz = strdup ( OutputRolloutContext ( NULL, &pes->rc ) );
          char *pcS = sz, *pcE;
          
          while ( ( pcE = strstr ( pcS, "\n" ) ) ) {
            
            *pcE = 0;

            if ( i == pmr->n.iMove )
              fputs ( "<tr class=\"movethemove\">\n", pf );
            else if ( i % 2 )
              fputs ( "<tr class=\"moveodd\">\n", pf );
            else
              fputs ( "<tr>\n", pf );
            
            fprintf ( pf, "<td colspan=\"3\">&nbsp;</td>\n" );
            
            fputs ( "<td colspan=\"2\">", pf );
            fputs ( pcS, pf );
            fputs ( "</td>\n", pf );
            
            fputs ( "</tr>\n", pf );
            
            pcS = pcE + 1;
            
          }

          free ( sz );

          break;

        }
        default:

          break;

        }

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
               const htmlexporttype het ) {

  char sz[ 1024 ];

  switch ( pmr->mt ) {

  case MOVE_NORMAL:

    fprintf ( pf, "<p>" );

    if ( het == HTML_EXPORT_TYPE_FIBS2HTML )
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

    HTMLPrintCubeAnalysis ( pf, pms, pmr, szImageDir, szExtension, het );

    HTMLPrintMoveAnalysis ( pf, pms, pmr, szImageDir, szExtension, het );

    break;

  case MOVE_TAKE:
  case MOVE_DROP:
  case MOVE_DOUBLE:

    fprintf ( pf, "<p>" );

    if ( het == HTML_EXPORT_TYPE_FIBS2HTML )
      printImage ( pf, szImageDir, "b-indent", szExtension, "" );

    if ( pmr->mt == MOVE_DOUBLE )
      fprintf ( pf,
                "*%s doubles</p>\n",
                ap[ pmr->d.fPlayer ].szName );
    else
      fprintf ( pf,
                "*%s %s</p>\n",
                ap[ pmr->d.fPlayer ].szName,
                ( pmr->mt == MOVE_TAKE ) ? _("accepts") : _("rejects") );

    HTMLPrintCubeAnalysis ( pf, pms, pmr, szImageDir, szExtension, het );

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
  int ai[ 2 ];
  float aaaar[ 3 ][ 2 ][ 2 ][ 2 ];
  float r = getMWCFromError ( psc, aaaar );

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
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

      printStatTableRow4 ( pf,
                           _("Error rate (pr. move)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anUnforcedMoves[ 0 ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           psc->anUnforcedMoves[ 1 ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

    }
    else {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] );

      printStatTableRow4 ( pf,
                           _("Error rate (pr. move)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anUnforcedMoves[ 0 ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ],
                           psc->anUnforcedMoves[ 1 ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CHEQUERPLAY ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] );

    }

    for ( i = 0 ; i < 2; i++ )
      rt[ i ] = GetRating ( aaaar[ CHEQUERPLAY ][ PERMOVE ][ i ][ NORMALISED ] );
    
    printStatTableRow ( pf, 
                        _( "Chequer play rating"), "%s",
                        psc->anUnforcedMoves[ 0 ] ? 
                        gettext ( aszRating[ rt[ 0 ] ] ) : _("n/a"), 
                        psc->anUnforcedMoves[ 1 ] ? 
                        gettext ( aszRating[ rt[ 1 ] ] ) : _("n/a" ) );

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
      printStatTableRow4 ( pf, 
                           _( "Luck rate (pr. move)"), 
                           "%+6.3f", "%+7.3f",
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 0 ][ 0 ] /
                           psc->anTotalMoves[ 0 ],
                           psc->arLuck[ 0 ][ 1 ] /
                           psc->anTotalMoves[ 0 ],
                           psc->anTotalMoves[ 1 ],
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
                        psc->anTotalMoves[ 0 ] ?
                        gettext ( aszLuckRating[ ai[ 0 ] ] ) : _("n/a"), 
                        psc->anTotalMoves[ 1 ] ?
                        gettext ( aszLuckRating[ ai[ 1 ] ] ) : _("n/a") );

  }

  /* cube statistics */

  if( psc->fCube ) {

    printStatTableHeader ( pf,
                           _( "Cube statistics" ) );
    printStatTableRow ( pf, _ ( "Total cube decisions"), "%d",
                        psc->anTotalCube[ 0 ],
                        psc->anTotalCube[ 1 ] );
    printStatTableRow ( pf, _ ( "Close or actual cube decisions"), "%d",
                        psc->anCloseCube[ 0 ],
                        psc->anCloseCube[ 1 ] );
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
                           -psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
                           -psc->arErrorMissedDoubleDP[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeMissedDoubleDP[ 1 ],
                           -psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
                           -psc->arErrorMissedDoubleDP[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Missed doubles around TG" ),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeMissedDoubleTG[ 0 ],
                           -psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
                           -psc->arErrorMissedDoubleTG[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeMissedDoubleTG[ 1 ],
                           -psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
                           -psc->arErrorMissedDoubleTG[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongDoubleDP[ 0 ],
                           -psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
                           -psc->arErrorWrongDoubleDP[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongDoubleDP[ 1 ],
                           -psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
                           -psc->arErrorWrongDoubleDP[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongDoubleTG[ 0 ],
                           -psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
                           -psc->arErrorWrongDoubleTG[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongDoubleTG[ 1 ],
                           -psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
                           -psc->arErrorWrongDoubleTG[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong takes"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongTake[ 0 ],
                           -psc->arErrorWrongTake[ 0 ][ 0 ],
                           -psc->arErrorWrongTake[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongTake[ 1 ],
                           -psc->arErrorWrongTake[ 1 ][ 0 ],
                           -psc->arErrorWrongTake[ 1 ][ 1 ] * 100.0f );
      printStatTableRow3 ( pf, _ ( "Wrong passes"),
                           "%d", "%+6.3f", "%+7.3f%%",
                           psc->anCubeWrongPass[ 0 ],
                           -psc->arErrorWrongPass[ 0 ][ 0 ],
                           -psc->arErrorWrongPass[ 0 ][ 1 ] * 100.0f,
                           psc->anCubeWrongPass[ 1 ],
                           -psc->arErrorWrongPass[ 1 ][ 0 ],
                           -psc->arErrorWrongPass[ 1 ][ 1 ] * 100.0f );
    }
    else {
      printStatTableRow3 ( pf, _ ( "Missed doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeMissedDoubleDP[ 0 ],
                           -psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
                           -psc->arErrorMissedDoubleDP[ 0 ][ 1 ],
                           psc->anCubeMissedDoubleDP[ 1 ],
                           -psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
                           -psc->arErrorMissedDoubleDP[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Missed doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeMissedDoubleTG[ 0 ],
                           -psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
                           -psc->arErrorMissedDoubleTG[ 0 ][ 1 ],
                           psc->anCubeMissedDoubleTG[ 1 ],
                           -psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
                           -psc->arErrorMissedDoubleTG[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around DP"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongDoubleDP[ 0 ],
                           -psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
                           -psc->arErrorWrongDoubleDP[ 0 ][ 1 ],
                           psc->anCubeWrongDoubleDP[ 1 ],
                           -psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
                           -psc->arErrorWrongDoubleDP[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong doubles around TG"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongDoubleTG[ 0 ],
                           -psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
                           -psc->arErrorWrongDoubleTG[ 0 ][ 1 ],
                           psc->anCubeWrongDoubleTG[ 1 ],
                           -psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
                           -psc->arErrorWrongDoubleTG[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong takes"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongTake[ 0 ],
                           -psc->arErrorWrongTake[ 0 ][ 0 ],
                           -psc->arErrorWrongTake[ 0 ][ 1 ],
                           psc->anCubeWrongTake[ 1 ],
                           -psc->arErrorWrongTake[ 1 ][ 0 ],
                           -psc->arErrorWrongTake[ 1 ][ 1 ] );
      printStatTableRow3 ( pf, _ ( "Wrong passes"),
                           "%d", "%+6.3f", "%+7.3f",
                           psc->anCubeWrongPass[ 0 ],
                           -psc->arErrorWrongPass[ 0 ][ 0 ],
                           -psc->arErrorWrongPass[ 0 ][ 1 ],
                           psc->anCubeWrongPass[ 1 ],
                           -psc->arErrorWrongPass[ 1 ][ 0 ],
                           -psc->arErrorWrongPass[ 1 ][ 1 ] );
    }


    if ( pms->nMatchTo ) {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

      printStatTableRow4 ( pf,
                           _("Error rate (per cube decision)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anCloseCube[ 0 ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           psc->anCloseCube[ 1 ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

    }
    else {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ],
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] );

      printStatTableRow4 ( pf,
                           _("Error rate (per cube decision)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anCloseCube[ 0 ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ],
                           psc->anCloseCube[ 1 ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ CUBEDECISION ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] );

    }

    for ( i = 0 ; i < 2; i++ )
      rt[ i ] = GetRating ( aaaar[ CUBEDECISION ][ PERMOVE ][ i ][ NORMALISED ] );

    printStatTableRow ( pf, 
                        _( "Cube decision rating"), "%s",
                        psc->anCloseCube[ 0 ] ? 
                        gettext ( aszRating[ rt [ 0 ] ] ) : _("n/a"), 
                        psc->anCloseCube[ 1 ] ? 
                        gettext ( aszRating[ rt [ 1 ] ] ) : _("n/a") );
  }
    
  /* overall rating */
    
  if( psc->fMoves && psc->fCube ) {

    cubeinfo ci;
    

    printStatTableHeader ( pf,
                           _("Overall rating" ) );
                            
    if ( pms->nMatchTo ) {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

      printStatTableRow4 ( pf,
                           _("Error rate (per decision)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anUnforcedMoves[ 0 ] + psc->anCloseCube[ 0 ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ] 
                           * 100.0f,
                           psc->anUnforcedMoves[ 1 ] + psc->anCloseCube[ 1 ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] 
                           * 100.0f );

    }
    else {

      printStatTableRow2 ( pf,
                           _("Error rate (total)"), 
                           "%+6.3f", "%+7.3f%",
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_0 ][ UNNORMALISED ],
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ TOTAL ][ PLAYER_1 ][ UNNORMALISED ] );

      printStatTableRow4 ( pf,
                           _("Error rate (per decision)"), 
                           "%+6.3f", "%+7.3f%",
                           psc->anUnforcedMoves[ 0 ] + psc->anCloseCube[ 0 ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_0 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_0 ][ UNNORMALISED ],
                           psc->anUnforcedMoves[ 1 ] + psc->anCloseCube[ 1 ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_1 ][ NORMALISED ],
                           -aaaar[ COMBINED ][ PERMOVE ][ PLAYER_1 ][ UNNORMALISED ] );

    }

    for ( i = 0 ; i < 2; i++ ) 
      rt[ i ] = GetRating ( aaaar[ COMBINED ][ PERMOVE ][ i ][ NORMALISED ] );
      
    printStatTableRow ( pf, 
                        _("Overall rating"), "%s",
                        ( psc->anCloseCube[ 0 ] + psc->anUnforcedMoves[ 0 ] )? 
                        gettext ( aszRating[ rt [ 0 ] ] ) : _("n/a"), 
                        ( psc->anCloseCube[ 1 ] + psc->anUnforcedMoves[ 1 ] )? 
                        gettext ( aszRating[ rt [ 1 ] ] ) : _("n/a" ) );

    SetCubeInfo( &ci, pms->nCube, pms->fCubeOwner, 0,
                 pms->nMatchTo, pms->anScore, pms->fCrawford,
                 fJacoby, nBeavers );

    if ( pms->nMatchTo ) {

      /* skill */

      printStatTableRow ( pf,
                          _( "MWC against current opponent"),
                          "%6.2f%%",
                          100.0 * r, 
                          100.0 * ( 1.0 - r ) );

      printStatTableRow ( pf,
                          _( "Guestimated abs. rating"),
                          "%6.2f",
                          absoluteFibsRating ( aaaar[ COMBINED ][ PERMOVE ]
                                                    [ PLAYER_0 ][ NORMALISED ],
                                               pms->nMatchTo ),
                          absoluteFibsRating ( aaaar[ COMBINED ][ PERMOVE ]
                                                    [ PLAYER_1 ][ NORMALISED ],
                                               pms->nMatchTo ) );

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


static void
HTMLMatchInfo ( FILE *pf, const matchinfo *pmi ) {

  int i;
  char sz[ 80 ];
  struct tm tmx;

  fputs ( "<hr />", pf );

  fprintf ( pf, "<h2>%s</h2>\n", _("Match Information") );

  /* ratings */

  for ( i = 0; i < 2; ++i )
      fprintf ( pf, _("%s's rating: %s<br />\n"), 
                ap[ i ].szName, 
                pmi->pchRating[ i ] ? pmi->pchRating[ i ] : _("n/a") );

  /* date */

  if ( pmi->nYear ) {

    tmx.tm_year = pmi->nYear - 1900;
    tmx.tm_mon = pmi->nMonth - 1;
    tmx.tm_mday = pmi->nDay;
    strftime ( sz, sizeof ( sz ), _("%B %d, %Y"), &tmx );
    fprintf ( pf, _("Date: %s<br />\n"), sz ); 

  }
  else
    fputs ( _("Date: n/a<br />\n"), pf );

  /* event, round, place and annotator */

  fprintf ( pf, _("Event: %s<br />\n"),
            pmi->pchEvent ? pmi->pchEvent : _("n/a") );
  fprintf ( pf, _("Round: %s<br />\n"),
            pmi->pchRound ? pmi->pchRound : _("n/a") );
  fprintf ( pf, _("Place: %s<br />\n"),
            pmi->pchPlace ? pmi->pchPlace : _("n/a") );
  fprintf ( pf, _("Annotator: %s<br />\n"),
            pmi->pchAnnotator ? pmi->pchAnnotator : _("n/a") );
  fprintf ( pf, _("Comments: %s<br />\n"),
            pmi->pchComment ? pmi->pchComment : _("n/a") );

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
                             const char *szExtension, 
                             const htmlexporttype het,
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

      FixMatchState ( &msExport, pmr );

      switch( pmr->mt ) {

      case MOVE_GAMEINFO:

        ApplyMoveRecord ( &msExport, pmr );

        HTMLPrologue( pf, &msExport, iGame, aszLinks, het );

        if ( exsExport.fIncludeMatchInfo )
          HTMLMatchInfo ( pf, &mi );

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

        HTMLBoardHeader ( pf, &msExport, het, iGame, iMove );

        printHTMLBoard( pf, &msExport, msExport.fTurn, 
                        szImageDir, szExtension, het );
        HTMLAnalysis ( pf, &msExport, pmr, szImageDir, szExtension, het );
        
        iMove++;

        break;

      case MOVE_TAKE:
      case MOVE_DROP:

        HTMLBoardHeader ( pf,&msExport, het, iGame, iMove );

        printHTMLBoard( pf, &msExport, msExport.fTurn, 
                        szImageDir, szExtension, het );
        
        HTMLAnalysis ( pf, &msExport, pmr, szImageDir, szExtension, het );
        
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


    HTMLEpilogue( pf, &msExport, aszLinks );
    
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
	outputerr( sz );
	return;
    }

    ExportGameHTML( pf, plGame,
                    exsExport.szHTMLPictureURL, exsExport.szHTMLExtension, 
                    exsExport.het,
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

      sprintf ( sz, "%s_%03d", szBase, iGame + 1 );
      return sz;

    }
    else {

      strcpy ( sz, szBase );
      pc = strrchr ( sz, '.' );
      
      sprintf ( pc, "_%03d%s", iGame + 1, szExtension );

      return sz;

    }

  }

}



extern void CommandExportMatchHtml( char *sz ) {
    
    FILE *pf;
    list *pl;
    int nGames;
    char *aszLinks[ 4 ], *filenames[4];
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
	  filenames[0] = HTMLFilename ( sz, 0 );
      aszLinks[ 0 ] = basename ( filenames[ 0 ] );
	  filenames[ 1 ] = aszLinks[ 1 ] = NULL;
	  if (i > 0) {
		filenames[ 1 ] = HTMLFilename ( sz, i - 1 );
		aszLinks[ 1 ]  = basename ( filenames[ 1 ] );
	  }
		
	  filenames[ 2 ] = aszLinks[ 2 ] = NULL;
	  if (i < nGames - 1) {
		filenames[ 2 ] = HTMLFilename ( sz, i + 1 );
		aszLinks[ 2 ]  = basename ( filenames[ 2 ] );
	  }


	  
	  filenames[ 3 ] = HTMLFilename ( sz, nGames - 1 );
	  aszLinks[ 3 ] = basename ( filenames[ 3 ] );
      if ( !i ) {

        if ( ! confirmOverwrite ( sz, fConfirmSave ) ) {
		  for ( j = 0; j < 4; j++ )
			free (filenames [ j ] );

		  free ( szCurrent );
          return;
		}

        setDefaultFileName ( sz, PATH_HTML );

      }


      if( !strcmp( szCurrent, "-" ) )
	pf = stdout;
      else if( !( pf = fopen( szCurrent, "w" ) ) ) {
	outputerr( szCurrent );
		for ( j = 0; j < 4; j++ )
		  free (filenames [ j ] );

		free ( szCurrent );
	return;
      }

      ExportGameHTML ( pf, pl->p, 
                       exsExport.szHTMLPictureURL, exsExport.szHTMLExtension,
                       exsExport.het, 
                       i, i == nGames - 1,
                       aszLinks );

      for ( j = 0; j < 4; j++ )
        free (filenames [ j ] );

      free ( szCurrent );

      if( pf != stdout )
        fclose( pf );

    }
    
}


extern void CommandExportPositionHtml( char *sz ) {

    FILE *pf;
    moverecord *pmr = getCurrentMoveRecord ();
	
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
	outputerr( sz );
	return;
    }

    HTMLPrologue ( pf, &ms, getGameNumber ( plGame ), NULL, exsExport.het );

    if ( exsExport.fIncludeMatchInfo )
      HTMLMatchInfo ( pf, &mi );

    HTMLBoardHeader ( pf, &ms, exsExport.het,
                      getGameNumber ( plGame ),
                      getMoveNumber ( plGame, pmr ) - 1 );

    printHTMLBoard( pf, &ms, ms.fTurn,
                    exsExport.szHTMLPictureURL, exsExport.szHTMLExtension,
                    exsExport.het );

    if( pmr )
      HTMLAnalysis ( pf, &ms, pmr,
                     exsExport.szHTMLPictureURL, exsExport.szHTMLExtension,
                     exsExport.het );
    
    HTMLEpilogue ( pf, &ms, NULL );

    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_HTML );

}

