/*
 * drawboard.c
 *
 * by Gary Wong, 1999
 *
 */

#include "config.h"

#include <assert.h>
#include <string.h>

#include "drawboard.h"
#include "positionid.h"

/*
 *  GNU Backgammon  Position ID: 0123456789ABCD
 *
 *  +13-14-15-16-17-18------19-20-21-22-23-24-+     O: gnubg (0 points)
 *  |                  |   | O  O  O  O     O | OO  Cube: 2
 *  |                  |   | O     O          | OO  On roll
 *  |                  |   |       O          | O
 *  |                  |   |                  | O
 *  |                  |   |                  | O   
 * v|                  |BAR|                  |     Cube: 1 (7 point match)
 *  |                  |   |                  | X    
 *  |                  |   |                  | X
 *  |                  |   |                  | X
 *  |                  |   |       X  X  X  X | X   Rolled 11
 *  |                  |   |    X  X  X  X  X | XX  Cube: 2
 *  +12-11-10--9--8--7-------6--5--4--3--2--1-+     X: Gary (0 points)
 *
 */

extern char *DrawBoard( char *sz, int anBoard[ 2 ][ 25 ], int fRoll,
			char *asz[] ) {

    char *pch = sz, *pchIn;
    int x, y, an[ 2 ][ 25 ], cOffO = 15, cOffX = 15;
    static char achX[ 16 ] = "     X6789ABCDEF",
	achO[ 16 ] = "     O6789ABCDEF";

    for( x = 0; x < 25; x++ ) {
	cOffO -= anBoard[ 0 ][ x ];
	cOffX -= anBoard[ 1 ][ x ];
    }
    
    strcpy( pch, " GNU Backgammon  Position ID: " );

    pch += 30;

    if( fRoll )
	strcpy( pch, PositionID( anBoard ) );
    else {
	for( x = 0; x < 25; x++ ) {
	    an[ 0 ][ x ] = anBoard[ 1 ][ x ];
	    an[ 1 ][ x ] = anBoard[ 0 ][ x ];
	}
	
	strcpy( pch, PositionID( an ) );
    }
    
    pch += 14;
    *pch++ = '\n';
	    
    strcpy( pch, fRoll ? " +13-14-15-16-17-18------19-20-21-22-23-24-+     " :
	    " +12-11-10--9--8--7-------6--5--4--3--2--1-+     " );
    pch += 49;

    if( asz[ 0 ] )
	for( pchIn = asz[ 0 ]; *pchIn; pchIn++ )
	    *pch++ = *pchIn;

    *pch++ = '\n';
    
    for( y = 0; y < 4; y++ ) {
	*pch++ = ' ';
	*pch++ = '|';

        for( x = 12; x < 18; x++ ) {
	    *pch++ = ' ';
	    *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
		anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
	    *pch++ = ' ';
	}

	*pch++ = '|';
	*pch++ = ' ';
	*pch++ = anBoard[ 0 ][ 24 ] > y ? 'O' : ' ';
	*pch++ = ' ';
	*pch++ = '|';
	
        for( ; x < 24; x++ ) {
	    *pch++ = ' ';
	    *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
		anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
	    *pch++ = ' ';
	}

	*pch++ = '|';
	*pch++ = ' ';

	for( x = 0; x < 3; x++ )
	    *pch++ = ( cOffO > 5 * x + y ) ? 'O' : ' ';

	*pch++ = ' ';
	
	if( y < 2 && asz[ y + 1 ] )
	    for( pchIn = asz[ y + 1 ]; *pchIn; pchIn++ )
		*pch++ = *pchIn;

	*pch++ = '\n';
    }

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 12; x < 18; x++ ) {
	*pch++ = ' ';
	*pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
		achO[ anBoard[ 0 ][ 23 - x ] ];
	*pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achO[ anBoard[ 0 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
	
    for( ; x < 24; x++ ) {
	*pch++ = ' ';
	*pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
		achO[ anBoard[ 0 ][ 23 - x ] ];
	*pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
	
    for( x = 0; x < 3; x++ )
	*pch++ = ( cOffO > 5 * x + 4 ) ? 'O' : ' ';

    *pch++ = '\n';
    
    *pch++ = fRoll ? 'v' : '^';
    strcpy( pch, "|                  |BAR|                  |     " );
    pch += 48;
    
    if( asz[ 3 ] )
	for( pchIn = asz[ 3 ]; *pchIn; pchIn++ )
	    *pch++ = *pchIn;

    *pch++ = '\n';

    *pch++ = ' ';
    *pch++ = '|';

    for( x = 11; x > 5; x-- ) {
	*pch++ = ' ';
	*pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
		achO[ anBoard[ 0 ][ 23 - x ] ];
	*pch++ = ' ';
    }

    *pch++ = '|';
    *pch++ = ' ';
    *pch++ = achX[ anBoard[ 1 ][ 24 ] ];
    *pch++ = ' ';
    *pch++ = '|';
	
    for( ; x >= 0; x-- ) {
	*pch++ = ' ';
	*pch++ = anBoard[ 1 ][ x ] ? achX[ anBoard[ 1 ][ x ] ] :
		achO[ anBoard[ 0 ][ 23 - x ] ];
	*pch++ = ' ';
    }

    *pch++ = '|';

    *pch++ = ' ';
	
    for( x = 0; x < 3; x++ )
	*pch++ = ( cOffX > 5 * x + 4 ) ? 'X' : ' ';

    *pch++ = '\n';
    
    for( y = 3; y >= 0; y-- ) {
	*pch++ = ' ';
	*pch++ = '|';

        for( x = 11; x > 5; x-- ) {
	    *pch++ = ' ';
	    *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
		anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
	    *pch++ = ' ';
	}

	*pch++ = '|';
	*pch++ = ' ';
	*pch++ = anBoard[ 1 ][ 24 ] > y ? 'X' : ' ';
	*pch++ = ' ';
	*pch++ = '|';
	
        for( ; x >= 0; x-- ) {
	    *pch++ = ' ';
	    *pch++ = anBoard[ 1 ][ x ] > y ? 'X' :
		anBoard[ 0 ][ 23 - x ] > y ? 'O' : ' ';
	    *pch++ = ' ';
	}
	
	*pch++ = '|';
	*pch++ = ' ';

	for( x = 0; x < 3; x++ )
	    *pch++ = ( cOffX > 5 * x + y ) ? 'X' : ' ';

	*pch++ = ' ';
	
	if( y < 2 && asz[ 5 - y ] )
	    for( pchIn = asz[ 5 - y ]; *pchIn; pchIn++ )
		*pch++ = *pchIn;
	
	*pch++ = '\n';
    }

    strcpy( pch, fRoll ? " +12-11-10--9--8--7-------6--5--4--3--2--1-+     " :
	    " +13-14-15-16-17-18------19-20-21-22-23-24-+     " );
    pch += 49;

    if( asz[ 6 ] )
	for( pchIn = asz[ 6 ]; *pchIn; pchIn++ )
	    *pch++ = *pchIn;
    
    *pch++ = '\n';
    *pch = 0;

    return sz;
}

static char *FormatPoint( char *pch, int n ) {

    assert( n >= 0 );
    
    if( !n ) {
	strcpy( pch, "off" );
	return pch + 3;
    } else if( n == 25 ) {
	strcpy( pch, "bar" );
	return pch + 3;
    } else if( n > 9 )
	*pch++ = n / 10 + '0';

    *pch++ = ( n % 10 ) + '0';

    return pch;
}

extern char *FormatMove( char *sz, int anBoard[ 2 ][ 25 ], int anMove[ 8 ] ) {

    char *pch = sz;
    int i, j;
    
    for( i = 0; i < 8 && anMove[ i ] >= 0; i += 2 ) {
	pch = FormatPoint( pch, anMove[ i ] + 1 );
	*pch++ = '/';
	pch = FormatPoint( pch, anMove[ i + 1 ] + 1 );

	if( anBoard && anMove[ i + 1 ] >= 0 &&
	    anBoard[ 0 ][ 23 - anMove[ i + 1 ] ] ) {
	    for( j = 1; ; j += 2 )
		if( j > i ) {
		    *pch++ = '*';
		    break;
		} else if( anMove[ i + 1 ] == anMove[ j ] )
		    break;
	}
	
	if( i < 6 )
	    *pch++ = ' ';
    }

    *pch = 0;
    
    return sz;
}
