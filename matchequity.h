/*
 * FIXME: insert description of the constants below
 * MAXSCORE    : The maximum match score
 * MAXCUBELEVEL: 1 + ceil(ln(MAXSCORE)/ln(2)), 
 *               ie the maximum level the cube goes to.
 */

#ifndef _MATCHEQUITY_H_
#define _MATCHEQUITY_H_

#include "eval.h"

#define MAXSCORE      64
#define MAXCUBELEVEL  7
#define DELTA         0.08
#define DELTABAR      0.06
#define G1            0.25
#define G2            0.15
#define GAMMONRATE    0.25

#define GET_A1(i,j,aafA1) ( ( (i) < 0 ) ? 1.0 : ( ( (j) < 0 ) ? 0.0 : \
						 (aafA1 [ i ][ j ]) ) )
#define GET_A2(i,j,aafA2) ( ( (i) < 0 ) ? 0.0 : ( ( (j) < 0 ) ? 1.0 : \
						 (aafA2 [ i ][ j ]) ) )
#define GET_Btilde(i,afBtilde) ( (i) < 0 ? 1.0 : afBtilde [ i ] )

/*
 * A1 (A2) is the match equity of player 1 (2)
 * Btilde is the post-crawford match equities.
 */

extern float aafA1 [ MAXSCORE ][ MAXSCORE ];
extern float aafA2 [ MAXSCORE ][ MAXSCORE ];

extern float afBtilde [ MAXSCORE ];

/*
 * D1bar (D2bar) is the equity of player 1 (2) at the drop point
 * of player 2 (1) assuming perfectly efficient recubes.
 * D1 (D2) is the equity of player 1 (2) at the drop point
 * of player 2 (1) assuming semiefficient recubes.
 */

extern float aaafD1 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
extern float aaafD2 [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
extern float aaafD1bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];
extern float aaafD2bar [ MAXSCORE ][ MAXSCORE ][ MAXCUBELEVEL ];


void
CalcMatchEq ();


void
GetTakePoint ( float arOutput [ 5 ],
	       int   nScore[ 2 ], int nMatchTo,
	       int   nCube,
	       float arTakePoint[ 2 ] );

void
GetDoublePointDeadCube ( float arOutput [ 5 ],
			 int   anScore[ 2 ], int nMatchTo,
			 cubeinfo *pci, float *rDP );

#endif
