
/*
 * path.c
 * routines extracted from eval.c
 * by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002.
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

#include "config.h"

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <cache.h>
#include <errno.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <isaac.h>
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#include <md5.h>
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "path.h"

#if WIN32
#define BINARY O_BINARY
#else
#define BINARY 0
#endif


/* Search for a readable file in szDir, ., and PKGDATADIR.  The
   return string is malloc()ed. */
extern char *PathSearch( const char *szFile, const char *szDir ) {

    char *pch;
    size_t cch;
    
    if( !szFile )
	return NULL;

    if( *szFile == '/' )
	/* Absolute file name specified; don't bother searching. */
	return strdup( szFile );

    cch = szDir ? strlen( szDir ) : 0;
    if( cch < strlen( PKGDATADIR ) )
	cch = strlen( PKGDATADIR );

    cch += strlen( szFile ) + 2;

    if( !( pch = malloc( cch ) ) )
	return NULL;

    sprintf( pch, "%s/%s", szDir, szFile );
    if( !access( pch, R_OK ) )
	return realloc( pch, strlen( pch ) + 1 );

    strcpy( pch, szFile );
    if( !access( pch, R_OK ) )
	return realloc( pch, strlen( pch ) + 1 );
    
    sprintf( pch, PKGDATADIR "/%s", szFile );
    if( !access( pch, R_OK ) )
	return realloc( pch, strlen( pch ) + 1 );

    /* Return sz, so that a sensible error message can be given. */
    strcpy( pch, szFile );
    return realloc( pch, strlen( pch ) + 1 );
}

/* Open a file for reading with the search path "(szDir):.:(PKGDATADIR)". */
extern int 
PathOpen( const char *szFile, const char *szDir, const int f ) {

    int h, idFirstError = 0;
#if __GNUC__
    char szPath[ strlen( PKGDATADIR ) + ( szDir ? strlen( szDir ) : 0 ) +
	       strlen( szFile ) + 2 ];
#elif HAVE_ALLOCA
    char *szPath = alloca( strlen( PKGDATADIR ) +
			   ( szDir ? strlen( szDir ) : 0 ) +
			   strlen( szFile ) + 2 );
#else
    char szPath[ 4096 ];
#endif
    
    if( szDir ) {
	sprintf( szPath, "%s/%s", szDir, szFile );
	if( ( h = open( szPath, O_RDONLY | f ) ) >= 0 )
	    return h;

	/* Try to report the more serious error (ENOENT is less
           important than, say, EACCESS). */
	if( errno != ENOENT )
	    idFirstError = errno;
    }

    if( ( h = open( szFile, O_RDONLY | f ) ) >= 0 )
	return h;

    if( !idFirstError && errno != ENOENT )
	idFirstError = errno;

    sprintf( szPath, PKGDATADIR "/%s", szFile );
    if( ( h = open( szPath, O_RDONLY | f ) ) >= 0 )
	return h;

    if( idFirstError )
	errno = idFirstError;

    return -1;
}

