/*
 * dynarray.c
 *
 * by Gary Wong, 1996
 */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include "config.h"
#include "dynarray.h"

int DynArrayCreate( dynarray *pda, int c, int fCompact ) {

    if( !( pda->ap = calloc( pda->cp = c, sizeof( void * ) ) ) )
	return -1;
    
    pda->c = 0;
    pda->fCompact = fCompact;
    pda->iFinish = 0;
    
    return 0;
}

int DynArrayDestroy( dynarray *pda ) {

    free( pda->ap );

    return 0;
}

int DynArrayAdd( dynarray *pda, void *p ) {

    int i;

    if( pda->fCompact )
	i = pda->c;
    else for( i = 0; i < pda->iFinish; i++ )
	if( !pda->ap[ i ] )
	    break;

    if( ( i >= pda->cp ) &&
	!( pda->ap = realloc( pda->ap, ( pda->cp <<= 1 ) *
			      sizeof( void * ) ) ) )
	return -1;

    pda->c++;
    
    pda->ap[ i ] = p;

    if( pda->iFinish <= i )
	pda->iFinish = i + 1;

    return i;
}

int DynArrayDelete( dynarray *pda, int i ) {

    assert( pda->ap[ i ] );
    
    pda->c--;

    if( pda->fCompact ) {
	pda->iFinish = pda->c;
	if( i != pda->c )
	    pda->ap[ i ] = pda->ap[ pda->c ];
    } else {
	pda->ap[ i ] = NULL;
	while( !pda->ap[ pda->iFinish ] )
	    pda->iFinish--;
    }
    
    if( ( ( pda->cp >> 2 ) >= pda->iFinish ) &&
	!( pda->ap = realloc( pda->ap, ( pda->cp >>= 1 ) *
			      sizeof( void * ) ) ) )
	return -1;
    
    return 0;
}

extern int DynArrayRemove( dynarray *pda, void *p ) {

    int i;

    if( !p ) {
	errno = EINVAL;
	return -1;
    }
    
    for( i = 0; i < pda->iFinish; i++ )
	if( pda->ap[ i ] == p )
	    return DynArrayDelete( pda, i );

    errno = ENOENT;
    return -1;
}

extern int DynArraySet( dynarray *pda, int i, void *p ) {

    if( pda->fCompact && i > pda->iFinish ) {
	errno = EINVAL;
	return -1;
    }

    if( i < pda->iFinish ) {
	if( !pda->ap[ i ] )
	    pda->c++;

	pda->ap[ i ] = p;

	return 0;
    }
    
    if( ( i >= pda->cp ) && !( pda->ap = realloc( pda->ap, ( pda->cp <<= 1 ) *
						  sizeof( void * ) ) ) )
	return -1;

    pda->c++;
    
    pda->ap[ i ] = p;

    pda->iFinish = i + 1;

    return 0;
}
