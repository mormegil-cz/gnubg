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

typedef enum _met {
  MET_ZADEH, MET_SNOWIE, MET_WOOLSEY
} met;


extern float aafA1 [ MAXSCORE ][ MAXSCORE ];
extern float aafA2 [ MAXSCORE ][ MAXSCORE ];

extern float afBtilde [ MAXSCORE ];

extern met metCurrent;
extern int nMaxScore;

void
InitMatchEquity ( met metInit );

extern int
GetPoints ( float arOutput [ 5 ], int   anScore[ 2 ], int nMatchTo,
	    cubeinfo *pci, float arCP[ 2 ] );

extern float
GetDoublePointDeadCube ( float arOutput [ 5 ],
                         int   anScore[ 2 ], int nMatchTo,
                         cubeinfo *pci );

#endif
