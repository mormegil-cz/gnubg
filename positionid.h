/*
 * positionid.h
 *
 * by Gary Wong, 1998, 1999, 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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

#include "gnubg-types.h"
extern void PositionKey( TanBoard anBoard, unsigned char auchKey[ 10 ] );
extern char *PositionID( TanBoard anBoard );
extern char *PositionIDFromKey( const unsigned char auchKey[ 10 ] );

extern 
unsigned int PositionBearoff( const int anBoard[],
                              int nPoints,
                              int nChequers );

extern void PositionFromKey(TanBoard anBoard, const unsigned char* puch);

/* Return 1 for success, 0 for invalid id */
extern int PositionFromID( TanBoard anBoard, const char* szID );

extern void 
PositionFromBearoff(int anBoard[], unsigned int usID,
		    int nPoints, int nChequers );

extern unsigned short PositionIndex(int g, const int anBoard[6]);

extern int EqualKeys( const unsigned char auch0[ 10 ], const unsigned char auch1[ 10 ] );
extern int EqualBoards( TanBoard anBoard0, TanBoard anBoard1 );

/* Return 1 for valid position, 0 for not */
extern int CheckPosition( TanBoard anBoard );

extern void ClosestLegalPosition( TanBoard anBoard );

extern unsigned int Combination ( const int n, const int r );

extern unsigned char Base64( const unsigned char ch );

#endif

