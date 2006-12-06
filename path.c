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

#include <glib.h>
#include <cache.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "common.h"
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

    if( *szFile == DIR_SEPARATOR )
	/* Absolute file name specified; don't bother searching. */
	return strdup( szFile );

    cch = szDir ? strlen( szDir ) : 0;
    if( cch < strlen( PKGDATADIR ) )
	cch = strlen( PKGDATADIR );

    cch += strlen( szFile ) + 2;

    if( ( pch = malloc( cch ) ) == NULL )
	return NULL;

    if( szDir ) {
	sprintf( pch, "%s%c%s", szDir, DIR_SEPARATOR, szFile );
	if( !access( pch, R_OK ) )
	    return realloc( pch, strlen( pch ) + 1 );
    }
    
    strcpy( pch, szFile );
    if( !access( pch, R_OK ) )
	return realloc( pch, strlen( pch ) + 1 );
    
    sprintf( pch, PKGDATADIR DIR_SEPARATOR_S "%s", szFile );
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
    char *szPath = (char *) g_alloca (strlen( PKGDATADIR ) + ( szDir ? strlen( szDir ) : 0 ) + strlen( szFile ) + 2);
    
    if( szDir ) {
	sprintf( szPath, "%s%c%s", szDir, DIR_SEPARATOR, szFile );
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

    sprintf( szPath, PKGDATADIR DIR_SEPARATOR_S "%s", szFile );
    if( ( h = open( szPath, O_RDONLY | f ) ) >= 0 )
	return h;

    if( idFirstError )
	errno = idFirstError;

    return -1;
}


/*
 * Backup file by renaming it to sz~
 *
 */

extern int
BackupFile ( const char *sz ) {

  char *szNew;
  int rc;

  if ( ! sz || !*sz )
    return 0;

  if ( access ( sz, R_OK ) )
    return 0;

  if ( ( szNew = (char *) malloc ( strlen ( sz ) + 2 ) ) == NULL ) 
    return -1;

  strcpy ( szNew, sz );
  strcat ( szNew, "~" );

#ifdef WIN32
  /* windows can not rename to an existing file */
  if ( unlink ( szNew ) && errno != ENOENT ) {
    /* do not complain if file is not found */
    perror ( szNew );
    free ( szNew );
    return -1;
  }
#endif


  rc = rename ( sz, szNew );
  free ( szNew );

  return rc;

}
