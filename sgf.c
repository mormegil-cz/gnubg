/*
 * sgf.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998, 1999, 2000.
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "backgammon.h"
#include "sgf.h"

static char *szFile;
static int fError;

static void ErrorHandler( char *sz, int fParseError ) {

    if( !fError ) {
	fError = TRUE;
	fprintf( stderr, "%s: %s\n", szFile, sz );
    }
}

static void FreeCollection( list *pl ) {

    /* FIXME what a mess to clean up... */
}

static list *LoadCollection( char *sz ) {

    list *plCollection, *pl, *plRoot, *plProp;
    FILE *pf;
    int c;
    property *pp;
    
    fError = FALSE;
    SGFErrorHandler = ErrorHandler;
    
    if( strcmp( sz, "-" ) ) {
	if( !( pf = fopen( sz, "r" ) ) ) {
	    perror( sz );
	    return NULL;
	}
	szFile = sz;
    } else {
	/* FIXME does it really make sense to try to load from stdin? */
	pf = stdin;
	szFile = "(stdin)";
    }
    
    plCollection = SGFParse( pf );

    if( pf != stdin )
	fclose( pf );

    /* Traverse collection, looking for backgammon games. */
    if( plCollection ) {
	c = 0;

	/* FIXME aaaargh!!! */
	for( pl = plCollection->plNext; pl != plCollection; pl = pl->plNext )
	    if( ( plRoot = ( (list *) pl->p )->plNext->p ) ) {
		for( plProp = plRoot->plNext; plProp != plRoot;
		     plProp = plProp->plNext ) {
		    pp = plProp->p;

putchar( pp->ach[ 0 ] );
putchar( pp->ach[ 1 ] );
putchar( '\n' );
		    
		    if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'M' &&
			pp->pl->plNext->p && atoi( (char *)
						   pp->pl->plNext->p ) == 6 ) {
			c++;
			break;
		    }
		}
	}
	    
	if( !c ) {
	    ErrorHandler( "warning: no backgammon games in SGF file", TRUE );
	    FreeCollection( plCollection );
	    return NULL;
	}
    }
    
    return pl;
}

extern void CommandLoadGame( char *sz ) {

    list *pl;

    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "game')." );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	/* FIXME ask if they want to abort current match */
	/* FIXME free match */

	/* FIXME parse file */

	FreeCollection( pl );
    }
}

extern void CommandLoadMatch( char *sz ) {

    list *pl;

    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "match')." );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	/* FIXME ask if they want to abort current match */
	/* FIXME free match */

	/* FIXME parse file */

	FreeCollection( pl );
    }
}

static void WriteEscapedString( FILE *pf, char *pch ) {

    for( ; *pch; pch++ )
	switch( *pch ) {
	case '\\':
	    putc( '\\', pf );
	    putc( '\\', pf );
	    break;
	case ':':
	    putc( '\\', pf );
	    putc( ':', pf );
	    break;
	case ']':
	    putc( '\\', pf );
	    putc( ']', pf );
	    break;
	default:
	    putc( *pch, pf );
	}
}

static void WriteMove( FILE *pf, movenormal *pmn ) {

    int i;

    for( i = 0; i < 8; i++ )
	switch( pmn->anMove[ i ] ) {
	case 24: /* bar */
	    putc( 'y', pf );
	    break;
	case -1: /* off */
	    if( !( i & 1 ) )
		return;
	    putc( 'z', pf );
	    break;
	default:
	    putc( pmn->fPlayer ? 'x' - pmn->anMove[ i ] :
		  'a' + pmn->anMove[ i ], pf );
	}
}

static void SaveGame( FILE *pf, list *plGame ) {

    list *pl;
    moverecord *pmr;
    int nResult = 0, nFileCube = 1, fResigned = FALSE, anBoard[ 2 ][ 25 ];
    char ch;

    pl = plGame->plNext;
    pmr = pl->p;
    assert( pmr->mt == MOVE_GAMEINFO );
    
    /* Fixed header */
    fputs( "(;FF[4]GM[6]AP[GNU Backgammon:" VERSION "]", pf );

    /* Match length, if appropriate */
    if( pmr->g.nMatch )
	fprintf( pf, "MI[length:%d][game:%d][ws:%d][bs:%d]", pmr->g.nMatch,
		 pmr->g.i, pmr->g.anScore[ 0 ], pmr->g.anScore[ 1 ] );
    
    /* Names */
    fputs( "PW[", pf );
    WriteEscapedString( pf, ap[ 0 ].szName );
    fputs( "]PB[", pf );
    WriteEscapedString( pf, ap[ 1 ].szName );
    putc( ']', pf );

    if( pmr->g.fCrawford ) {
	fputs( "RU[Crawford]", pf );
	if( pmr->g.fCrawfordGame )
	    fputs( "[CrawfordGame]", pf );
    } else if( pmr->g.fJacoby )
	fputs( "RU[Jacoby]", pf );

    if( pmr->g.fWinner >= 0 )
	fprintf( pf, "RE[%c+%d%s]", pmr->g.fWinner ? 'B' : 'W',
		 pmr->g.nPoints, pmr->g.fResigned ? "R" : "" );

    putc( '\n', pf );

    InitBoard( anBoard );
    
    for( pl = pl->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    ch = pmr->n.fPlayer ? 'B' : 'W'; /* initialise ch -- the first
						move should be MOVE_NORMAL */
	    fprintf( pf, ";%c[%d%d", ch, pmr->n.anRoll[ 0 ],
		     pmr->n.anRoll[ 1 ] );
	    WriteMove( pf, &pmr->n );
	    putc( ']', pf );
	    ApplyMove( anBoard, pmr->n.anMove );
	    
	    nResult = GameStatus( anBoard );

	    SwapSides( anBoard );
	    break;
	    
	case MOVE_DOUBLE:
	    fprintf( pf, ";%c[double]", ch );
	    break;
	    
	case MOVE_TAKE:
	    fprintf( pf, ";%c[take]", ch );
	    nFileCube <<= 1;
	    break;
	    
	case MOVE_DROP:
	    fprintf( pf, ";%c[drop]", ch );
	    nResult = 1;
	    ch ^= 'B' ^ 'W'; /* when dropping, the other player wins */
	    break;
	    
	case MOVE_RESIGN:
	    fResigned = TRUE;
	    nResult = pmr->r.nResigned;
	    ch ^= 'B' ^ 'W'; /* when resigning, the other player wins */
	    break;
	default:
	    assert( FALSE );
	}

	if( nResult )
	    break;
	
	ch ^= 'B' ^ 'W'; /* switch B/W each turn */
    }

    anScore[ ch == 'B' ] += nFileCube * nResult;
    
    fputs( ")\n", pf );
}

extern void CommandSaveGame( char *sz ) {

    FILE *pf;
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to save to (see `help save"
		 "game')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    SaveGame( pf, plGame );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandSaveMatch( char *sz ) {

    FILE *pf;
    list *pl;

    /* FIXME what should be done if nMatchTo == 0? */
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to save to (see `help save "
		 "match')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
	SaveGame( pf, pl->p );
    
    if( pf != stdout )
	fclose( pf );
}
