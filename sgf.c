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
#include <string.h>

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

static void FreeList( list *pl, int nLevel ) {
    /* Levels are:
     *  0 - GameTreeSeq
     *  1 - GameTree
     *  2 - Sequence
     *  3 - Node
     *  4 - Property
     */

    if( nLevel == 1 ) {
	FreeList( pl->plNext->p, 2 ); /* initial sequence */
	ListDelete( pl->plNext );
	nLevel = 0; /* remainder of list is more GameTreeSeqs */
    }
    
    while( pl->plNext != pl ) {
	if( nLevel == 3 ) {
	    FreeList( ( (property *) pl->plNext->p )->pl, 4 );
	    free( pl->plNext->p );
	} else if( nLevel == 4 )
	    free( pl->plNext->p );
	else
	    FreeList( pl->plNext->p, nLevel + 1 );
	
	ListDelete( pl->plNext );
    }

    if( nLevel != 4 )
	free( pl );
}

static void FreeGameTreeSeq( list *pl ) {

    FreeList( pl, 0 );
}

static list *LoadCollection( char *sz ) {

    list *plCollection, *pl, *plRoot, *plProp;
    FILE *pf;
    int fBackgammon;
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
	pl = plCollection->plNext;
	while( pl != plCollection ) {
	    plRoot = ( (list *) ( (list *) pl->p )->plNext->p )->plNext->p;
	    fBackgammon = FALSE;
	    for( plProp = plRoot->plNext; plProp != plRoot;
		 plProp = plProp->plNext ) {
		pp = plProp->p;
		
		if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'M' &&
		    pp->pl->plNext->p && atoi( (char *)
					       pp->pl->plNext->p ) == 6 ) {
		    fBackgammon = TRUE;
		    break;
		}
	    }

	    pl = pl->plNext;
		
	    if( !fBackgammon ) {
		FreeList( pl->plPrev->p, 1 );
		ListDelete( pl->plPrev );
	    }
	}
	    
	if( ListEmpty( plCollection ) ) {
	    fError = FALSE; /* we always want to see this one */
	    ErrorHandler( "warning: no backgammon games in SGF file", TRUE );
	    free( plCollection );
	    return NULL;
	}
    }
    
    return plCollection;
}

static void CopyName( int i, char *sz ) {

    /* FIXME sanity check the name as in CommandSetPlayerName */
    
    if( strlen( sz ) > 31 )
	sz[ 31 ] = 0;
    
    strcpy( ap[ i ].szName, sz );
}

static void SetScore( int fBlack, int n ) {

    if( n >= 0 && ( !nMatchTo || n < nMatchTo ) )
	anScore[ fBlack ] = n;
}

static void RestoreMI( list *pl, movegameinfo *pmgi ) {

    char *pch;

    for( pl = pl->plNext; ( pch = pl->p ); pl = pl->plNext )
	if( !strncmp( pch, "length:", 7 ) ) {
	    nMatchTo = atoi( pch + 7 );

	    if( nMatchTo < 0 )
		nMatchTo = 0;
	} else if( !strncmp( pch, "game:", 5 ) ) {
	    cGames = atoi( pch + 5 );

	    if( cGames < 0 )
		cGames = 0;
	} else if( !strncmp( pch, "ws:", 3 ) || !strncmp( pch, "bs:", 3 ) )
	    SetScore( *pch == 'b', atoi( pch + 3 ) );
}

static void RestoreRootNode( list *pl ) {

    property *pp;
    movegameinfo *pmgi = malloc( sizeof( *pmgi ) );
    char *pch;
    
    pmgi->mt = MOVE_GAMEINFO;
    pmgi->i = 0;
    pmgi->nMatch = 0;
    pmgi->anScore[ 0 ] = 0;
    pmgi->anScore[ 1 ] = 0;
    pmgi->fCrawford = FALSE;
    pmgi->fCrawfordGame = FALSE;
    pmgi->fJacoby = FALSE;
    pmgi->fWinner = -1;
    pmgi->nPoints = 0;
    pmgi->fResigned = FALSE;
    pmgi->nAutoDoubles = 0;
    ListInsert( plGame, pmgi );

    for( pl = pl->plNext; ( pp = pl->p ); pl = pl->plNext )
	if( pp->ach[ 0 ] == 'M' && pp->ach[ 1 ] == 'I' )
	    /* MI - Match info property */
	    RestoreMI( pp->pl, pmgi );
	else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'B' )
	    /* PB - Black player property */
	    CopyName( 1, pp->pl->plNext->p );
	else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'W' )
	    /* PW - White player property */
	    CopyName( 0, pp->pl->plNext->p );
	else if( pp->ach[ 0 ] == 'R' && pp->ach[ 1 ] == 'E' ) {
	    /* RE - Result property */
	    pch = pp->pl->plNext->p;

	    pmgi->fWinner = -1;
	    
	    if( toupper( *pch ) == 'B' )
		pmgi->fWinner = 1;
	    else if( toupper( *pch ) == 'W' )
		pmgi->fWinner = 0;

	    if( pmgi->fWinner == -1 )
		continue;
	    
	    if( *++pch != '+' )
		continue;

	    pmgi->nPoints = strtol( pch, &pch, 10 );

	    pmgi->fResigned = toupper( *pch ) == 'R';
	} else if( pp->ach[ 0 ] == 'R' && pp->ach[ 1 ] == 'U' ) { 
	    /* RU - Rules property */

	    pch = pp->pl->plNext->p;

	    if( !strcmp( pch, "Crawford" ) )
		pmgi->fCrawford = TRUE;
	    else if( !strcmp( pch, "Crawford:CrawfordGame" ) )
		pmgi->fCrawford = pmgi->fCrawfordGame = TRUE;
	    else if( !strcmp( pch, "Jacoby" ) )
		pmgi->fJacoby = TRUE;
	}
}

static int Point( char ch, int f ) {

    if( ch == 'y' )
	return 25; /* bar */
    else if( ch <= 'x' && ch >= 'a' )
	return f ? 'x' - ch + 1 : ch - 'a' + 1;
    else
	return 0; /* off */
}

static void PlayMove( int anMove[ 8 ], int fPlayer ) {

    int i, nSrc, nDest;
    
    if( fPlayer )
	SwapSides( anBoard );
    
    for( i = 0; i < 8; i += 2 ) {
	nSrc = anMove[ i ] - 1;
	nDest = anMove[ i | 1 ] - 1;

	if( nSrc < 0 )
	    /* move is finished */
	    break;
	
	if( !anBoard[ 1 ][ nSrc ] )
	    /* source point is empty; ignore */
	    continue;

	anBoard[ 1 ][ nSrc ]--;
	if( nDest >= 0 )
	    anBoard[ 1 ][ nDest ]++;

	if( nSrc >= 0 && nSrc < 24 ) {
	    anBoard[ 0 ][ 24 ] += anBoard[ 0 ][ 23 - nDest ];
	    anBoard[ 0 ][ 23 - nDest ] = 0;
	}
    }

    if( fPlayer )
	SwapSides( anBoard );    
}

static void RestoreNode( list *pl ) {

    property *pp;
    moverecord *pmr;
    char *pch;
    int i;
    
    for( pl = pl->plNext; ( pp = pl->p ); pl = pl->plNext )
	if( pp->ach[ 0 ] == 'B' || pp->ach[ 0 ] == 'W' ) {
	    /* B or W - Move property */
	    pch = pp->pl->plNext->p;
	    pmr = NULL;
	    
	    if( !strcmp( pch, "double" ) ) {
		pmr = malloc( sizeof( pmr->mt ) );
		pmr->mt = MOVE_DOUBLE;
		/* FIXME handle cube action */
	    } else if( !strcmp( pch, "take" ) ) {
		pmr = malloc( sizeof( pmr->mt ) );
		pmr->mt = MOVE_TAKE;
		/* FIXME handle cube action */
	    } else if( !strcmp( pch, "drop" ) ) {
		pmr = malloc( sizeof( pmr->mt ) );
		pmr->mt = MOVE_DROP;
		/* FIXME handle cube action */
	    } else {
		pmr = malloc( sizeof( pmr->n ) );
		pmr->n.mt = MOVE_NORMAL;
		pmr->n.fPlayer = pp->ach[ 0 ] == 'B';
		
		pmr->n.anRoll[ 1 ] = 0;
		
		for( i = 0; i < 2 && *pch; pch++, i++ )
		    pmr->n.anRoll[ i ] = *pch - '0';

		for( i = 0; i < 4 && pch[ 0 ] && pch[ 1 ]; pch += 2, i++ ) {
		    pmr->n.anMove[ i << 1 ] = Point( pch[ 0 ],
						     pmr->n.fPlayer );
		    pmr->n.anMove[ ( i << 1 ) | 1 ] = Point( pch[ 1 ],
							     pmr->n.fPlayer );
		}

		if( i < 4 )
		    pmr->n.anMove[ i << 1 ] = -1;
			
		if( pmr->n.anRoll[ 0 ] >= 1 && pmr->n.anRoll[ 0 ] <= 6 &&
		    pmr->n.anRoll[ 1 ] >= 1 && pmr->n.anRoll[ 1 ] <= 6 ) {
		    PlayMove( pmr->n.anMove, pmr->n.fPlayer );
		    ListInsert( plGame, pmr );
		} else
		    /* illegal move -- ignore */
		    free( pmr );
	    }

	    fMove = fTurn = !pmr->n.fPlayer;
	} else
	    /* FIXME handle setup properties */
	    ;
    
}

static void RestoreSequence( list *pl, int fRoot ) {

    pl = pl->plNext;
    if( fRoot )
	RestoreRootNode( pl->p );
    else
	RestoreNode( pl->p );

    while( pl = pl->plNext, pl->p )
	RestoreNode( pl->p );
}

static void RestoreTree( list *pl, int fRoot ) {
    
    pl = pl->plNext;
    RestoreSequence( pl->p, fRoot );

    pl = pl->plNext;
    if( pl->p )
	RestoreTree( pl->p, FALSE );

    /* FIXME restore other variations, once we can handle them */
}

static void RestoreGame( list *pl ) {

    InitBoard( anBoard );
    
    plGame = malloc( sizeof( *plGame ) );
    ListCreate( plGame );

    ListInsert( &lMatch, plGame );

    fResigned = fDoubled = FALSE;
    nCube = 1;
    fCubeOwner = -1;

    RestoreTree( pl, TRUE );

    /* FIXME add "resign" node if necessary */
}

static void ClearMatch( void ) {

    nMatchTo = 0;

    cGames = anScore[ 0 ] = anScore[ 1 ] = 0;
    fMove = fTurn = -1;
    fCrawford = FALSE;
    fPostCrawford = FALSE;
    
}

static void UpdateSettings( void ) {

    UpdateSetting( &nCube );
    UpdateSetting( &fCubeOwner );
    UpdateSetting( &fTurn );
    UpdateSetting( &nMatchTo );
    UpdateSetting( &fCrawford );

    if( fTurn )
	SwapSides( anBoard );
    
    ShowBoard();
}

extern void CommandLoadGame( char *sz ) {

    list *pl;

    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "game')." );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	if( fTurn != -1 && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to load a saved game, "
			     "and discard the one in progress? " ) )
		return;
	}

	FreeMatch();
	ClearMatch();
	
	/* FIXME if pl contains multiple games, ask which one to load */

	RestoreGame( pl->plNext->p );
	
	FreeGameTreeSeq( pl );

	UpdateSettings();
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
	/* FIXME make sure the root nodes have MI properties */
	if( fTurn != -1 && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to load a saved match, "
			     "and discard the game in progress? " ) )
		return;
	}

	FreeMatch();
	ClearMatch();
	
	for( pl = pl->plNext; pl->p; pl = pl->plNext )
	    RestoreGame( pl->p );

	FreeGameTreeSeq( pl );

	UpdateSettings();
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
	fputs( "RU[Crawford", pf );
	if( pmr->g.fCrawfordGame )
	    fputs( ":CrawfordGame", pf );
	putc( ']', pf );
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
