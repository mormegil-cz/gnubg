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

#include <list.h>

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
  int anCloseCube[ 2 ];
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
  RAT_AWFUL,
  RAT_BEGINNER, RAT_CASUAL_PLAYER, RAT_INTERMEDIATE, RAT_ADVANCED,
  RAT_EXPERT, RAT_WORLD_CLASS, RAT_SUPERNATURAL, RAT_UNDEFINED
} ratingtype;

extern const char *aszRating [ RAT_UNDEFINED + 1 ];
extern const char *aszLuckRating[ 7 ];

extern ratingtype GetRating ( const float rError );
extern void IniStatcontext ( statcontext *psc );
extern void AddStatcontext ( statcontext *pscA, statcontext *pscB );

extern void
DumpStatcontext ( char *szOutput, const statcontext *psc, const char * sz );

extern void
updateStatisticsGame ( list *plGame );

extern void
updateStatisticsMatch ( list *plMatch );

extern int getLuckRating ( const float rLuck );

extern float
relativeFibsRating ( const float r, const int n );

extern float
absoluteFibsRating ( const float r, const int n );

#define CHEQUERPLAY  0
#define CUBEDECISION 1
#define COMBINED     2

#define TOTAL        0
#define PERMOVE      1

#define PLAYER_0     0
#define PLAYER_1     1

#define NORMALISED   0
#define UNNORMALISED 1

extern float
getMWCFromError ( const statcontext *psc, float aaaar[ 3 ][ 2 ][ 2 ][ 2 ] );

extern skilltype
Skill( float r );

#endif
