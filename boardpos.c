/*
 * boardpos.c
 *
 * by Jørn Thyssen <jth@gnubg.org>, 2003.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>

#include "boarddim.h"
#include "boardpos.h"
#include <glib/gi18n.h>

extern void
ChequerPosition( const int clockwise, 
                 const int point, const int chequer,
                 int *px, int *py ) {

    int c_chequer;

    c_chequer = ( !point || point == 25 ) ? 3 : 5;

    if ( c_chequer > chequer )
      c_chequer = chequer;

    *px = positions[ clockwise ][ point ][ 0 ];
    *py = positions[ clockwise ][ point ][ 1 ] - ( c_chequer - 1 ) *
	positions[ clockwise ][ point ][ 2 ];

}


extern void
PointArea( const int fClockwise, const int nSize,
           const int n,
           int *px, int *py, int *pcx, int *pcy ) {

    /* max chequer in column */
    int c_chequer = ( !n || n == 25 ) ? 3 : 5;
    
    *px = positions[ fClockwise ][ n ][ 0 ] * nSize;
    *py = positions[ fClockwise ][ n ][ 1 ] * nSize;
    *pcx = CHEQUER_WIDTH * nSize;
    *pcy = positions[ fClockwise ][ n ][ 2 ] * nSize;
    
    if( *pcy > 0 ) {
	*pcy = *pcy * ( c_chequer - 1 ) + 
	  (CHEQUER_HEIGHT + DISPLAY_POINT_EXTRA) * nSize;
	*py += CHEQUER_HEIGHT * nSize - *pcy;
    } else
	*pcy = -*pcy * ( c_chequer - 1 ) + 
	  (CHEQUER_HEIGHT + DISPLAY_POINT_EXTRA) * nSize;




}


extern void
CubePosition( const int crawford_game, const int cube_use,
              const int doubled, const int cube_owner,
              int *px, int *py, int *porient ) {

    if( crawford_game || !cube_use ) {
	/* no cube */
	if( px ) *px = NO_CUBE;
	if( py ) *py = NO_CUBE;
	if( porient ) *porient = -1;
    } else if( doubled ) {
      if( px ) *px = (doubled > 0) ? CUBE_RIGHT_X: CUBE_LEFT_X;
	if( py ) *py = CUBE_CENTRE_Y;
	if( porient ) *porient = doubled;
    } else {
	if( px ) *px = CUBE_CENTRE_X;
	if( py ) *py = (cube_owner < 0)  ? CUBE_OWN_1_Y :
	               (cube_owner == 0) ? CUBE_CENTRE_Y :  CUBE_OWN_0_Y;
	if( porient ) *porient = cube_owner;
    }

}


extern void
ResignPosition( const int resigned, int *px, int *py, int *porient ) {

  if( resigned ) {
    if ( px ) *px = (resigned / abs ( resigned ) < 0) ? 
		CUBE_RESIGN_LEFT_X : CUBE_RESIGN_RIGHT_X;
    if ( py ) *py = CUBE_CENTRE_Y;
    if( porient ) *porient = - resigned / abs ( resigned );
  }
  else {
    /* no resignation */
    if( px ) *px = NO_CUBE;
    if( py ) *py = NO_CUBE;
    if( porient ) *porient = -1;
  }

}

extern void
ArrowPosition( const int clockwise, const int nSize, int *px, int *py ) {

  /* calculate the position of the arrow to indicate
     player on turn and direction of play;  *px and *py
     return the position of the upper left corner in pixels,
     NOT board units */

    int Point28_x, Point28_y, Point28_dx, Point28_dy;
    int Point29_x, Point29_y, Point29_dx, Point29_dy;

    PointArea( clockwise, nSize, POINT_UNUSED0, 
               &Point28_x, &Point28_y, &Point28_dx, &Point28_dy );
    PointArea( clockwise, nSize, POINT_UNUSED1, 
               &Point29_x, &Point29_y, &Point29_dx, &Point29_dy );

    assert( Point28_x == Point29_x );
    assert( Point28_dx == Point29_dx );
    assert( Point28_dy == Point29_dy );

    if ( px ) *px = Point29_x + Point29_dx / 2
			- nSize * ARROW_SIZE / 2;
    if ( py ) *py = (nSize * (BOARD_HEIGHT - ARROW_HEIGHT) / 2);


}
