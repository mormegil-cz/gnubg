/*
 * heap.c
 *
 * by Gary Wong, 1997
 */

#include <stddef.h>
#include <stdlib.h>

#include "heap.h"

extern int HeapCreate( heap *ph, int c, heapcomparefunc phcf ) {

    if( !( ph->ap = malloc( c * sizeof( void * ) ) ) )
	return -1;
    
    ph->cp = 0;
    ph->cpAlloc = c;
    ph->phcf = phcf;
    
    return 0;
}

extern int HeapDestroy( heap *ph ) {

    free( ph->ap );
    
    return 0;
}

extern int HeapInsert( heap *ph, void *p ) {

    int i;
    void *pTemp;
    
    if( ph->cp == ph->cpAlloc ) {
	if( !( ph->ap = realloc( ph->ap, ( ph->cpAlloc << 1 ) *
				 sizeof( void * ) ) ) )
	    return -1;

	ph->cpAlloc <<= 1;
    }

    ph->ap[ ph->cp ] = p;

    for( i = ph->cp++; i && ph->phcf( ph->ap[ i ],
				      ph->ap[ ( i - 1 ) >> 1 ] ) < 0;
	 i = ( i - 1 ) >> 1 ) {
	pTemp = ph->ap[ i ];
	ph->ap[ i ] = ph->ap[ ( i - 1 ) >> 1 ];
	ph->ap[ ( i - 1 ) >> 1 ] = pTemp;
    }
    
    return 0;
}

static int DeleteItem( heap *ph, int i ) {

    void *pTemp;
    int iSwap;
    
    ph->ap[ i ] = ph->ap[ --ph->cp ];

    while( ( i << 1 ) + 1 < ph->cp ) {
	iSwap = i;
	
	if( ph->phcf( ph->ap[ iSwap ], ph->ap[ ( i << 1 ) + 1 ] ) > 0 )
	    iSwap = ( i << 1 ) + 1;

	if( ( i << 1 ) + 2 < ph->cp &&
	    ph->phcf( ph->ap[ iSwap ], ph->ap[ ( i << 1 ) + 2 ] ) > 0 )
	    iSwap = ( i << 1 ) + 2;

	if( iSwap == i )
	    break;

	pTemp = ph->ap[ i ];
	ph->ap[ i ] = ph->ap[ iSwap ];
	ph->ap[ iSwap ] = pTemp;
	
	i = iSwap;
    }

    if( ph->cpAlloc > 1 && ( ph->cp << 1 < ph->cpAlloc ) )
	return ( ph->ap = realloc( ph->ap, ( ph->cpAlloc >>= 1 ) *
				   sizeof( void * ) ) ) ? 0 : -1;

    return 0;
}

extern int HeapDeleteItem( heap *ph, void *p ) {

    int i;

    for( i = 0; i < ph->cp; i++ )
	if( ph->ap[ i ] == p )
	    return DeleteItem( ph, i );

    return -1;
}

extern void *HeapDelete( heap *ph ) {

    void *p = HeapLookup( ph );

    if( !ph->cp )
	return NULL;

    return DeleteItem( ph, 0 ) ? NULL : p;
}

extern void *HeapLookup( heap *ph ) {

    return ph->cp ? ph->ap[ 0 ] : NULL;
}
