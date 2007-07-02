/* This program is free software; you can redistribute it and/or modify
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
 *
 * dynarray.c
 *
 * by Gary Wong, 1996
 * $Id$
 */

#include "config.h"
#include <stdlib.h>
#include <glib.h>
#include <errno.h>
#include <stddef.h>

#include "dynarray.h"

int DynArrayCreate( dynarray *pda, unsigned int c, int fCompact )
{
	if( ( pda->ap = (void**)calloc( pda->cp = c, sizeof( void * ) ) ) == NULL )
		return FALSE;

	pda->c = 0;
	pda->fCompact = fCompact;
	pda->iFinish = 0;

	return TRUE;
}

void DynArrayDestroy( const dynarray *pda ) {

    free( pda->ap );
}

unsigned int DynArrayAdd( dynarray *pda, void *p ) {

	unsigned int i;

	if( pda->fCompact )
		i = pda->c;
	else
		for( i = 0; i < pda->iFinish; i++ )
			if( !pda->ap[ i ] )
				break;

	if ( i >= pda->cp )
	{
		pda->ap = (void**)realloc( pda->ap, ( pda->cp <<= 1 ) *
			sizeof( void * ) );
		if (!pda->ap)
			return 0;
	}

	pda->c++;

	pda->ap[ i ] = p;

	if( pda->iFinish <= i )
		pda->iFinish = i + 1;

	return i;
}

int DynArrayDelete( dynarray *pda, unsigned int i ) {

	/* assert( pda->ap[ i ] );	Who knows what this was supposed to mean?... */

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
		(( pda->ap = (void**)realloc( pda->ap, ( pda->cp >>= 1 ) *
			sizeof( void * ) ) ) == NULL) )
		return -1;

	return 0;
}

extern int DynArrayRemove( dynarray *pda, const void *p ) {

	unsigned int i;

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

extern int DynArraySet( dynarray *pda, unsigned int i, void *p ) {

	if( pda->fCompact && i > pda->iFinish ) {
		errno = EINVAL;
		return FALSE;
	}

	if( i < pda->iFinish ) {
		if( !pda->ap[ i ] )
			pda->c++;

		pda->ap[ i ] = p;

		return TRUE;
	}

	if( ( i >= pda->cp ) && (( pda->ap = (void**)realloc( pda->ap, ( pda->cp <<= 1 ) *
			sizeof( void * ) ) ) == NULL ) )
		return FALSE;

	pda->c++;

	pda->ap[ i ] = p;

	pda->iFinish = i + 1;

	return TRUE;
}
