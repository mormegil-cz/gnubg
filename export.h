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

typedef struct _exportsetup {

  int fIncludeAnnotation;
  int fIncludeAnalysis;
  int fIncludeStatistics;
  int fIncludeLegend;

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


} exportsetup;

extern exportsetup exsExport;

#endif
