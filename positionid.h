/*
 * positionid.h
 *
 * by Gary Wong, 1998, 1999, 2002
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

#ifndef _POSITIONID_H_
#define _POSITIONID_H_

extern void PositionKey( int anBoard[ 2 ][ 25 ], unsigned char auchKey[ 10 ] );
extern char *PositionID( int anBoard[ 2 ][ 25 ] );
extern char *PositionIDFromKey( unsigned char auchKey[ 10 ] );

extern 
unsigned int PositionBearoff( const int anBoard[],
                              const int nPoints,
                              const int nChequers );

extern void PositionFromKey( int anBoard[ 2 ][ 25 ],
                             unsigned char *puch );
extern int PositionFromID( int anBoard[ 2 ][ 25 ], const char *szID );

extern void 
PositionFromBearoff( int anBoard[], const unsigned int usID,
                                 const int nPoints, const int nChequers );

extern unsigned short PositionIndex(int g, int anBoard[6]);

extern int 
EqualKeys( const unsigned char auch0[ 10 ], const unsigned char auch1[ 10 ] );
extern int EqualBoards( int anBoard0[ 2 ][ 25 ], int anBoard1[ 2 ][ 25 ] );

extern int 
CheckPosition( int anBoard[ 2 ][ 25 ] );
extern void ClosestLegalPosition( int anBoard[ 2 ][ 25 ] );

extern int
Combination ( const int n, const int r );

extern int
Base64( const char ch );

#endif

