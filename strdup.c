/*
 * strdup.c
 *
 * by Gary Wong <gtw@gnu.org>, 2002
 *
 * $Id$
 */

#include <stdlib.h>
#include <string.h>

extern char *strdup( const char *sz ) {

    char *pch;
    size_t cch;
    
    if( !sz )
	return NULL;

    cch = strlen( sz );

    if( !( pch = malloc( cch + 1 ) ) )
	return NULL;

    memcpy( pch, sz, cch + 1 );

    return pch;
}
