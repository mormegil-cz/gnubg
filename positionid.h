/*
 * positionid.h
 *
 * by Gary Wong, 1998-1999
 *
 */

#ifndef _POSITIONID_H_
#define _POSITIONID_H_

extern void PositionKey( int anBoard[ 2 ][ 25 ], unsigned char auchKey[ 10 ] );
extern char *PositionID( int anBoard[ 2 ][ 25 ] );
extern char *PositionIDFromKey( unsigned char auchKey[ 10 ] );
extern unsigned short PositionBearoff( int anBoard[ 6 ] );
extern void PositionFromKey( int anBoard[ 2 ][ 25 ],
			     unsigned char *puch );
extern void PositionFromID( int anBoard[ 2 ][ 25 ], char *szID );
extern void PositionFromBearoff( int anBoard[ 6 ], unsigned short usID );
extern int EqualKeys( unsigned char auch0[ 10 ], unsigned char auch1[ 10 ] );

#endif
