/*
 * drawboard.h
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
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
 */

#ifndef _DRAWBOARD_H_
#define _DRAWBOARD_H_

extern char *DrawBoard( char *pch, int anBoard[ 2 ][ 25 ], int fRoll,
			char *asz[] );
extern char *FormatMove( char *pch, int anBoard[ 2 ][ 25 ], int anMove[ 8 ] );

#endif
