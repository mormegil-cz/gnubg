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

#define EXPORT_SIDE_PLAYER0  1
#define EXPORT_SIDE_PLAYER1  2

#define EXPORT_MOVES_NONE    0
#define EXPORT_MOVES_ROLLOUT 1
#define EXPORT_MOVES_EVAL    2

#define EXPORT_MOVES_ERROR   1
#define EXPORT_MOVES_BLUNDER 2
#define EXPORT_MOVES_ALL     4

#define EXPORT_CUBE_NONE     0
#define EXPORT_CUBE_ROLLOUT  1
#define EXPORT_CUBE_EVAL     2

#define EXPORT_CUBE_ACTUAL   1
#define EXPORT_CUBE_MISSED   2
#define EXPORT_CUBE_CLOSE    4
#define EXPORT_CUBE_ALL      8

typedef struct _exportsetup {

  int fIncludeAnnotation;
  int fIncludeAnalysis;
  int fIncludeStatistics;
  int fIncludeLegend;

  /* display board: 0 (never), 1 (every move), 2 (every second move) etc */

  int fDisplayBoard;

  int fSide; /* bits: 1 (player 0), 2 (player 1) */

  /* moves */

  int nMoves; /* show at most nMoves */
  int fMovesDetailProb; /* show detailed probabilities */
  int fMovesParameters; /* bits: 1 (rollout), 2 (eval) */
  int fMovesDisplay;    /* bits: 1 (error), 2 (blunder), 3 (all) */

  /* cube */
  
  int fCubeDetailProb; /* show detailed probabilities */
  int fCubeParameters; /* bits: 1 (rollout), 2 (eval) */
  int fCubeDisplay;    /* bits: 1 (actual), 2 (close), 3 (all) */

  /* FIXME: add format specific options */


} exportsetup;

extern exportsetup ecsExport;

#endif
