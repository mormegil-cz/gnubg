/*
 * i18n.h
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

#ifndef _I18N_H_
#define _I18N_H_

#include <stdio.h>

#include "config.h"

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
# define gettext(Text) Text
#define textdomain(Domain)
#define bindtextdomain(Package, Directory)
#define bind_textdomain_codeset(d,c)
#endif
#ifdef gettext_noop
# define N_(String) gettext_noop (String)
#else
# define N_(String) (String)
#endif

/* utility functions */

void PushLocale ( const char *locale );
void PopLocale ( void );

/* locale "independent" functions (meaning they run in the "C" locale) */

#if !defined (__GNUC__) && !defined (__attribute__)
#define __attribute__(X)
#endif

extern int 
liscanf ( const char *format, ... )
    __attribute__((format(scanf,1,2)));
extern int 
lifscanf ( FILE *stream, const char *format, ... )
    __attribute__((format(scanf,2,3)));
extern int
lisscanf( const char *str, const char *format, ...)
    __attribute__((format(scanf,2,3)));

extern int 
liprintf(const char *format, ...)
    __attribute__((format(printf,1,2)));
extern int 
lifprintf(FILE *stream, const char *format, ...)
    __attribute__((format(printf,2,3)));
extern int 
lisprintf(char *str, const char *format, ...)
    __attribute__((format(printf,2,3)));
extern int 
lisnprintf(char *str, size_t size, const  char  *format, ...)
    __attribute__((format(printf,3,4)));


#endif
