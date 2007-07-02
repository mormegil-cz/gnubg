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
 * hash.c
 *
 * by Gary Wong, 1997-2000
 * $Id$
 */

#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "hash.h"

static unsigned int ac[] = { 2, 3, 7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093,
		    8191, 16381, 32749, 65521, 131071, 262139, 524287,
		    1048573, 2097143, 4194301, 8388593, 16777213, 33554393,
		    67108859, 134217689, 268435399, 536870909, 1073741789, 0 };

extern int HashCreate( hash *ph, unsigned int c, hashcomparefunc phcf ) {

    int i;
    
    for( i = 0; ac[ i + 1 ] && c > ac[ i ]; i++ )
		;
    
    if( ( ph->aphn = (hashnode**)calloc( ac[ i ], sizeof( hashnode * ) ) ) == NULL )
		return -1;
    
    ph->c = 0;
    ph->icp = i;
    ph->phcf = phcf;
    ph->cSize = ac[ i ];
    ph->cHits = ph->cMisses = ph->cLookups = 0;

    return 0;
}

extern int HashDestroy( const hash *ph ) {

	hashnode *phn, *phnNext;
	unsigned int i;

	for( i = 0; i < ac[ ph->icp ]; i++ ) {
		phn = ph->aphn[ i ];

		while( phn ) {
			phnNext = phn->phnNext;

			free( phn );

			phn = phnNext;
		}
	}

	free( ph->aphn );

	return 0;
}

extern int HashAdd( hash *ph, unsigned long l, void *p ) {

    /* FIXME check for rehash */

	hashnode **pphn = ph->aphn + l % ac[ ph->icp ];
    hashnode *phn = (hashnode *) malloc( sizeof( hashnode ) );
	if (!phn)
		return -1;
    
    phn->phnNext = *pphn;
    phn->l = l;
    phn->p = p;
    
    *pphn = phn;
    ph->c++;

    return 0;
}

extern int HashDelete( hash *ph, unsigned long l, void *p ) {

    hashnode **pphn, *phnNext;

    for( pphn = ph->aphn + l % ac[ ph->icp ]; *pphn;
	 pphn = &( ( *pphn )->phnNext ) )
	if( ( ( *pphn )->l == l ) && ( !ph->phcf ||
	    !( ph->phcf )( p, ( *pphn )->p ) ) ) {
	    phnNext = ( *pphn )->phnNext;
	    free( *pphn );
	    *pphn = phnNext;

	    ph->c--;
	    
	    /* FIXME check for rehash */

	    return 0;
	}

    return -1;
}

extern void *HashLookup( hash *ph, unsigned long l, void *p ) {

    hashnode **pphn;

    ++ph->cLookups;

    for( pphn = ph->aphn + l % ac[ ph->icp ]; *pphn;
	 pphn = &( ( *pphn )->phnNext ) )
	if( ( ( *pphn )->l == l ) && ( !ph->phcf ||
            !( ph->phcf )( p, ( *pphn )->p ) ) ) {
            ++ph->cHits;
	    return ( *pphn )->p;
        }
    
    ++ph->cMisses;
    return NULL;
}

extern long StringHash( char *sz ) {

    long l = 0;
    char *pch;

    for( pch = sz; *pch; pch++ )
		l = /*lint --e(703) */( ( l << 8 ) % 8388593 ) ^ *pch;

    return l;
}
