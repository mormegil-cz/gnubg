/*
 * analysis.h
 *
 * by Joern Thyssen <joern@thyssen.nu>, 2000
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

#ifndef _ANALYSIS_H_
#define _ANALYSIS_H_

typedef enum _lucktype {
    LUCK_VERYBAD, LUCK_BAD, LUCK_NONE, LUCK_GOOD, LUCK_VERYGOOD
} lucktype;

typedef enum _skilltype {
    SKILL_VERYBAD, SKILL_BAD, SKILL_DOUBTFUL, SKILL_NONE,
    SKILL_INTERESTING, SKILL_GOOD, SKILL_VERYGOOD
} skilltype;

typedef struct _statcontext {
  int fMoves, fCube, fDice; /* which statistics have been computed? */
    
  int anUnforcedMoves[ 2 ];
  int anTotalMoves[ 2 ];

  int anTotalCube[ 2 ];
  int anDouble[ 2 ];
  int anTake[ 2 ];
  int anPass[ 2 ];

  int anMoves[ 2 ][ SKILL_VERYGOOD + 1 ];

  int anLuck[ 2 ][ LUCK_VERYGOOD + 1 ];

  int anCubeMissedDoubleDP[ 2 ];
  int anCubeMissedDoubleTG[ 2 ];
  int anCubeWrongDoubleDP [ 2 ];
  int anCubeWrongDoubleTG [ 2 ];
  int anCubeWrongTake [ 2 ];
  int anCubeWrongPass [ 2 ];

  /* all accumulated errors have dimension 2x2 
   *  - first dimension is player
   *  - second dimension is error rate in:
   *    - EMG and MWC for match play
   *    - Normalized and unnormalized equity for money games
   */

  float arErrorCheckerplay[ 2 ][ 2 ];
  float arErrorMissedDoubleDP [ 2 ][ 2 ];
  float arErrorMissedDoubleTG [ 2 ][ 2 ];
  float arErrorWrongDoubleDP [ 2 ][ 2 ];
  float arErrorWrongDoubleTG [ 2 ][ 2 ];
  float arErrorWrongTake [ 2 ][ 2 ];
  float arErrorWrongPass [ 2 ][ 2 ];
  float arLuck[ 2 ][ 2 ];
  
} statcontext;

typedef enum _ratingtype {
  RAT_BEGINNER, RAT_NOVICE, RAT_INTERMEDIATE, RAT_ADVANCED,
  RAT_EXPERT, RAT_WORLD_CLASS, RAT_EXTRA_TERRESTRIAL 
} ratingtype;

const char *aszRating [ RAT_EXTRA_TERRESTRIAL + 1 ];

extern ratingtype GetRating ( const float rError );
extern void IniStatcontext ( statcontext *psc );
extern void AddStatcontext ( statcontext *pscA, statcontext *pscB );

#endif
