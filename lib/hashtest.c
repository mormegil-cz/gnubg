/*
 * hashtest.c
 *
 * by Gary Wong, 1997
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
