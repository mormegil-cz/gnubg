/*
 * export.h
 *
 * by Jørn Thyssen  <jthyssen@dk.ibm.com>, 2002.
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

#ifndef _EXPORT_H_
#define _EXPORT_H_

#define EXPORT_CUBE_ACTUAL   7
#define EXPORT_CUBE_MISSED   8
#define EXPORT_CUBE_CLOSE    9


typedef enum _htmlexporttype {
  HTML_EXPORT_TYPE_GNU,
  HTML_EXPORT_TYPE_BBS,
  HTML_EXPORT_TYPE_FIBS2HTML,
  NUM_HTML_EXPORT_TYPES
} htmlexporttype;

typedef enum _htmlexportcss {
  HTML_EXPORT_CSS_HEAD,
  HTML_EXPORT_CSS_INLINE,
  HTML_EXPORT_CSS_EXTERNAL,
  NUM_HTML_EXPORT_CSS 
} htmlexportcss;

extern char *aszHTMLExportType[];
extern char *aszHTMLExportCSS[];
extern char *aszHTMLExportCSSCommand[];

typedef struct _exportsetup {

  int fIncludeAnnotation;
  int fIncludeAnalysis;
  int fIncludeStatistics;
  int fIncludeLegend;
  int fIncludeMatchInfo;

  /* display board: 0 (never), 1 (every move), 2 (every second move) etc */

  int fDisplayBoard;

  int fSide; /* 0, 1, or -1 for both players */

  /* moves */

  int nMoves; /* show at most nMoves */
  int fMovesDetailProb; /* show detailed probabilities */
  int afMovesParameters[ 2 ]; /* detailed parameters */
  int afMovesDisplay[ 7 ];    /* display moves */

  /* cube */
  
  int fCubeDetailProb; /* show detailed probabilities */
  int afCubeParameters[ 2 ]; /* detailed parameters */
  int afCubeDisplay[ 10 ];    /* display moves */

  /* FIXME: add format specific options */

  /* For example, frames/non frames for HTML. */

  char *szHTMLPictureURL;
  htmlexporttype het;
  char *szHTMLExtension;
  htmlexportcss hecss;

  /* png */

  int nPNGSize;

} exportsetup;

extern exportsetup exsExport;

extern int
getGameNumber ( const list *plGame );

extern int
getMoveNumber ( const list *plGame, const void *p );

extern char *
HTMLFilename ( const char *szBase, const int iGame );

extern int WritePNG( const char *sz, unsigned char *puch, int nStride,
		     const int nSizeX, const int nSizeY );

/* misc. output routines used by text and HTML export */

extern char *
OutputPercents ( const float ar[], const int f );

extern char *
OutputPercent ( const float r );

extern char *
OutputMWC ( const float r, const cubeinfo *pci, const int f );

extern char *
OutputEquity ( const float r, const cubeinfo *pci, const int f );

extern char *
OutputRolloutContext ( const char *szIndent, const rolloutcontext *prc );

extern char *
OutputEvalContext ( const evalcontext *pec, const int fChequer );

extern char *
OutputEquityDiff ( const float r1, const float r2, const cubeinfo *pci );

extern char *
OutputEquityScale ( const float r, const cubeinfo *pci, 
                    const cubeinfo *pciBase, const int f );

extern char *
OutputRolloutResult ( const char *szIndent,
                      char asz[][ 1024 ],
                      float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                      float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                      const cubeinfo aci[],
                      const int cci,
                      const int fCubeful );

extern char *
OutputCubeAnalysis ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                     float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                     evalsetup *pes, cubeinfo *pci,
                     int fDouble, int fTake,
                     skilltype stDouble,
                     skilltype stTake,
                     const int fPrintAlerts );

extern char *
OutputMoneyEquity ( const float ar[], const int f );


#endif
