/*
 * sgf.y
 *
 * Generic SGF file parser.
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
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sgf.h"

static list *plCollection;    
    
extern int sgflex( void );

#define YYERROR_VERBOSE

void ( *SGFErrorHandler )( char *, int ) = NULL;
 
extern int sgferror( char *s ) {

    if( SGFErrorHandler )
	SGFErrorHandler( s, 1 );
    else
	fprintf( stderr, "%s\n", s );
    
    return 0;
}

list *NewList( void ) {
    list *pl = malloc( sizeof( *pl ) );
    ListCreate( pl );
    return pl;
}

static char *Concatenate( list *pl ) {

    int cch = 0;
    char *sz, *pchDest, *pchSrc;
    
    for( pl = pl->plNext; pl->p; pl = pl->plNext )
	cch += strlen( pl->p );

    pchDest = sz = malloc( cch + 1 );
    
    while( pl->plNext != pl ) {
	for( pchSrc = pl->plNext->p; ( *pchDest++ = *pchSrc++ ); )
	    ;

	pchDest--;
	
	free( pl->plNext->p );
	ListDelete( pl->plNext );
    }

    free( pl );
    
    return sz;
}
 
%}

/* There are 2 shift/reduce conflicts caused by ambiguities at which level
   error handling should be performed (GameTreeSeq, Sequence, and
   PropertySeq); the default shifting produces correct behaviour. */
%expect 2

%union {
    char ach[ 2 ]; /* property identifier */
    char *pch; /* property value */
    property *pp; /* complete property */
    list *pl; /* nodes, sequences, gametrees */
}

%token <ach> PROPERTY <pch> VALUETEXT

%type <pp> Property
%type <pch> Value
%type <pl> Collection GameTreeSeq GameTree Sequence Node PropertySeq ValueSeq
%type <pl> ValueCharSeq

%%
		/* The specification says empty collections are illegal, but
		   we'll try to be accomodating. */
Collection:	GameTreeSeq
		{ $$ = plCollection = $1; }

GameTreeSeq:	/* empty */
		{ $$ = NewList(); }
	|	GameTreeSeq GameTree
		{ ListInsert( $1, $2 ); $$ = $1; }
	|	GameTreeSeq error
	;

GameTree:	'(' Sequence GameTreeSeq ')'
		{ ListInsert( $3->plNext, $2 ); $$ = $3; }
	;

Sequence:	Node
		{ $$ = NewList(); ListInsert( $$, $1 ); }
	|	Sequence Node
		{ ListInsert( $1, $2 ); $$ = $1; }
	|	Sequence error
	;

Node:		';' PropertySeq
		{ $$ = $2; }
	;

PropertySeq:	/* empty */
		{ $$ = NewList(); }
	|	PropertySeq Property
		{ ListInsert( $1, $2 ); $$ = $1; }
		/* FIXME if property already exists in node, augment existing
		   list */
	|	PropertySeq error
	;

Property:	PROPERTY ValueSeq Value
		{ 
		    ListInsert( $2, $3 );
		    $$ = malloc( sizeof(property) ); $$->pl = $2;
		    $$->ach[ 0 ] = $1[ 0 ]; $$->ach[ 1 ] = $1[ 1 ];
		}
	;

ValueSeq:	/* empty */
		{ $$ = NewList(); }
	|	ValueSeq Value
		{ ListInsert( $1, $2 ); $$ = $1; }
	;

Value:		'[' ValueCharSeq ']'
		{ $$ = Concatenate( $2 ); }
	;

ValueCharSeq:	/* empty */
		{ $$ = NewList(); }
	|	ValueCharSeq VALUETEXT
		{ ListInsert( $1, $2 ); $$ = $1; }
	;

%%

extern list *SGFParse( FILE *pf ) {

    extern FILE *sgfin;
    
    sgfin = pf;
    plCollection = NULL;
    
    sgfparse();

    return plCollection;
}
	
#if SGFTEST

#include <mcheck.h>
	
static void Indent( int n ) {
    while( n-- )
	putchar( ' ' );
}

static void PrintProperty( property *pp, int n ) {

    list *pl;
    
    Indent( n );
    putchar( pp->ach[ 0 ] );
    putchar( pp->ach[ 1 ] );

    for( pl = pp->pl->plNext; pl->p; pl = pl->plNext )
	printf( "[%s]", (char *) pl->p );
    
    putchar( '\n' );
}

static void PrintNode( list *pl, int n ) {

    for( pl = pl->plNext; pl->p; pl = pl->plNext )
	PrintProperty( pl->p, n );
    
    Indent( n ); puts( "-" );
}

static void PrintSequence( list *pl, int n ) {

    for( pl = pl->plNext; pl->p; pl = pl->plNext )
	PrintNode( pl->p, n );
}

static void PrintGameTreeSeq( list *pl, int n );

static void PrintGameTree( list *pl, int n ) {

    pl = pl->plNext;

    Indent( n ); puts( "<<<<<" );
    PrintSequence( pl->p, n );
    PrintGameTreeSeq( pl, n + 1 );
    Indent( n ); puts( ">>>>>" );
}

static void PrintGameTreeSeq( list *pl, int n ) {

    for( pl = pl->plNext; pl->p; pl = pl->plNext )
	PrintGameTree( pl->p, n );
}

void Error( char *s, int f ) {

    fprintf( stderr, "sgf error: %s\n", s );
}

int main( int argc, char *argv[] ) {

    FILE *pf = NULL;
    list *pl;
    
    mtrace();

    SGFErrorHandler = Error;

    if( argc > 1 )
	if( !( pf = fopen( argv[ 1 ], "r" ) ) ) {
	    perror( argv[ 1 ] );
	    return 1;
	}
    
    if( ( pl = SGFParse( pf ? pf : stdin ) ) )
	PrintGameTreeSeq( pl, 0 );
    else {
	puts( "Fatal error; can't print collection." );
	return 2;
    }
    
    return 0;
}
#endif
