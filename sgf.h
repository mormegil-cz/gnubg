/*
 * sgf.h
 *
 * by Gary Wong <gtw@gnu.org>, 2000
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

#ifndef _SGF_H_
#define _SGF_H_

#include <list.h>
#include <stdio.h>

typedef struct _property {
    char ach[ 2 ]; /* 2 character tag; ach[ 1 ] = 0 for 1 character tags */
    list *pl; /* Values */
} property;

extern void ( *SGFErrorHandler )( char *szMessage, int fParseError );

extern list *SGFParse( FILE *pf );
/* Parse an SGF file, and return a syntax tree.  The tree is saved as a list
   of game trees; each game tree is a list where the first element is the
   initial sequence of SGF nodes and any other elements are alternate
   variations (each variation is itself a game tree).  Sequences of SGF
   nodes are also stored as lists; each element is a single SGF node.
   Nodes consist of yet MORE lists; each element is a "property" struct
   as defined above.

   If there are any errors in the file, SGFParse calls SGFErrorHandler
   (if set), or complains to stderr (otherwise). */

#endif

