/*
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
 * hashtest.c
 *
 * by Gary Wong, 1997
 * $Id$
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "hash.h"

int main( void ) {

    hash h;
    
    HashCreate( &h, 16, (hashcomparefunc) strcmp );

    HashAdd( &h, 0, "Hello, " );
    HashAdd( &h, 0, "world." );
    HashAdd( &h, 0, "From" );
    HashAdd( &h, 0, "Hashtest." );

    if( strcmp( HashLookup( &h, 0, "Hello, " ), "Hello, " ) )
	return 1;
    if( strcmp( HashLookup( &h, 0, "world." ), "world." ) )
	return 1;
    if( strcmp( HashLookup( &h, 0, "From" ), "From" ) )
	return 1;
    if( strcmp( HashLookup( &h, 0, "Hashtest." ), "Hashtest." ) )
	return 1;

    if( HashLookup( &h, 0, "not there" ) )
	return 1;

    return 0;
}
