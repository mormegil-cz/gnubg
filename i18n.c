/*
 * i18n.c
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "i18n.h"

#define MAX_STACK 30

static char *aszLocaleStack [ MAX_STACK ];
static int iLocale = -1;

void PushLocale ( const char *locale ) {
  
#if ENABLE_NLS

  char *pc;

  if ( iLocale > MAX_STACK - 1 ) {
     printf ( "stack out of bounds in PushLocale\n" );
     assert ( FALSE );
  }

  ++iLocale;

  pc = setlocale ( LC_ALL, NULL );
  
  aszLocaleStack [ iLocale ] = strdup ( pc );

  setlocale ( LC_ALL, locale );

#endif

}

void PopLocale ( void ) {

#if ENABLE_NLS

  if ( iLocale < 0 )
    return;

  setlocale ( LC_ALL, aszLocaleStack[ iLocale ] );

  free ( aszLocaleStack[ iLocale ] );
  iLocale--;

#endif

}

#if 0

extern int 
liscanf ( const char *format, ... ) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vscanf ( format, val );
    va_end( val );

    PopLocale();

    return rc;

}

extern int 
lifscanf ( FILE *stream, const char *format, ... ) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vfscanf ( stream, format, val );
    va_end( val );

    PopLocale();

    return rc;

}


extern int
lisscanf( const char *str, const char *format, ...) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vsscanf ( str, format, val );
    va_end( val );

    PopLocale();

    return rc;

}

#endif


extern int 
liprintf(const char *format, ...) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vprintf ( format, val );
    va_end( val );

    PopLocale ();

    return rc;

}

extern int 
lifprintf(FILE *stream, const char *format, ...) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vfprintf ( stream, format, val );
    va_end( val );

    PopLocale ();

    return rc;

}

extern int 
lisprintf(char *str, const char *format, ...) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vsprintf ( str, format, val );
    va_end( val );

    PopLocale ();

    return rc;

}

#if 0

vsnprintf is not posix...

extern int 
lisnprintf(char *str, size_t size, const  char  *format, ...) {

    va_list val;
    int rc;

    PushLocale ( "C" );

    va_start( val, format );
    rc = vsnprintf ( str, size, format, val );
    va_end( val );

    PopLocale ();

    return rc;

}
#endif
