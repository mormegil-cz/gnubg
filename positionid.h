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

#define L_POSITIONID 14

extern void PositionKey( const TanBoard anBoard, unsigned char auchKey[ 10 ] );
extern char *PositionID( const TanBoard anBoard );
extern char *PositionIDFromKey( const unsigned char auchKey[ 10 ] );

extern 
unsigned int PositionBearoff( const unsigned int anBoard[],
                              unsigned int nPoints,
                              unsigned int nChequers );

extern void PositionFromKey(TanBoard anBoard, const unsigned char* puch);

/* Return 1 for success, 0 for invalid id */
extern int PositionFromID( TanBoard anBoard, const char* szID );

extern void PositionFromBearoff(unsigned int anBoard[], unsigned int usID,
		    unsigned int nPoints, unsigned int nChequers );

extern unsigned short PositionIndex(unsigned int g, const unsigned int anBoard[6]);

#define EqualKeys(k1, k2) (!memcmp(k1, k2, sizeof(char[10])))
extern int EqualBoards( const TanBoard anBoard0, const TanBoard anBoard1 );

/* Return 1 for valid position, 0 for not */
extern int CheckPosition( const TanBoard anBoard );

extern void ClosestLegalPosition( TanBoard anBoard );

extern unsigned int Combination ( const unsigned int n, const unsigned int r );

extern unsigned char Base64( const unsigned char ch );

#endif

