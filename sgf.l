/*
 * sgf.l
 *
 * by Gary Wong <gtw@gnu.org>, 2000.
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

%{
#include <ctype.h>
#include <list.h>
#include <string.h>

#include "sgf.h"
#include "sgfp.h"

extern int _SGFWarning( char * );

static int error( char *s ) {

    /* refer to yy_fatal_error, to shut up the compiler */
    (void) yy_fatal_error;

    if( SGFErrorHandler )
	SGFErrorHandler( s, 0 );
    else
	fprintf( stderr, "%s\n", s );
    
    return 0;
}

#define YY_FATAL_ERROR(m) error(m)
%}

%option nodefault
%option ecs
%option fast
%option noread
%option noyywrap

%option noinput
%option nounput
%option noyy_scan_buffer
%option noyy_scan_bytes
%option noyy_scan_string

%x value

%%

[[:space:]]+		/* ignore */
"("|")"|";"		return *yytext;
[[:lower:]]*[[:upper:]][[:lower:]]*[[:upper:]]?[[:lower:]]* {
			    char *pch;

			    sgflval.ach[ 1 ] = 0;

			    for( pch = yytext; *pch; pch++ )
			        if( isupper( *pch ) ) {
				    sgflval.ach[ 0 ] = *pch;    
				    break;
				}

			    for( pch++; *pch; pch++ )
			        if( isupper( *pch ) ) {
				    sgflval.ach[ 1 ] = *pch;
				    break;
				}

			    return PROPERTY;
			}
[[:lower:]]+		/* ignore -- this rule avoids making flex back up */
"["			BEGIN(value); return '[';
.			{ error( "illegal character in SGF file" ); }

<value>"\\]"		{ sgflval.pch = strdup( "]" ); return VALUETEXT; }
<value>"\\\n"		/* ignore */
<value>"]"		BEGIN(INITIAL); return ']';
<value>([^]\\])+	{ sgflval.pch = strdup( yytext ); return VALUETEXT; }
<value>\\.?		{ sgflval.pch = strdup( yytext ); return VALUETEXT; }

%%
