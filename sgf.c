/*
 * sgf.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001.
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "positionid.h"
#include "sgf.h"

#ifndef HUGE_VALF
#define HUGE_VALF (-1e38)
#endif

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

static void SetScore( movegameinfo *pmgi, int fBlack, int n ) {

    if( n >= 0 && ( !pmgi->nMatch || n < pmgi->nMatch ) )
	pmgi->anScore[ fBlack ] = n;
}

static void RestoreMI( list *pl, movegameinfo *pmgi ) {

    char *pch;

    for( pl = pl->plNext; ( pch = pl->p ); pl = pl->plNext )
	if( !strncmp( pch, "length:", 7 ) ) {
	    pmgi->nMatch = atoi( pch + 7 );

	    if( pmgi->nMatch < 0 )
		pmgi->nMatch = 0;
	} else if( !strncmp( pch, "game:", 5 ) ) {
	    pmgi->i = atoi( pch + 5 );

	    if( pmgi->i < 0 )
		pmgi->i = 0;
	} else if( !strncmp( pch, "ws:", 3 ) || !strncmp( pch, "bs:", 3 ) )
	    SetScore( pmgi, *pch == 'b', atoi( pch + 3 ) );
}

static void RestoreRootNode( list *pl ) {

    property *pp;
    movegameinfo *pmgi = malloc( sizeof( *pmgi ) );
    char *pch;
    int i;
    
    pmgi->mt = MOVE_GAMEINFO;
    pmgi->sz = NULL;
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
	    if( pmgi->nPoints < 1 )
		pmgi->nPoints = 1;
	    
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
	} else if( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'V' ) {
	    /* CV - Cube value (i.e. automatic doubles) */
	    for( i = 0; ( 1 << i ) <= MAX_CUBE; i++ )
		if( atoi( pp->pl->plNext->p ) == 1 << i ) {
		    pmgi->nAutoDoubles = i;
		    break;
		}
	}

    AddMoveRecord( pmgi );
}

static int Point( char ch, int f ) {

    if( ch == 'y' )
	return 24; /* bar */
    else if( ch <= 'x' && ch >= 'a' )
	return f ? 'x' - ch : ch - 'a';
    else
	return -1; /* off */
}

static void RestoreDoubleAnalysis( property *pp, evaltype *pet,
				   float ar[], evalsetup *pes ) {
    char *pch = pp->pl->plNext->p, ch;

    switch( *pch ) {
    case 'E':
	/* EVAL_EVAL */
	*pet = EVAL_EVAL;
	pes->ec.nSearchCandidates = 0;
	pes->ec.rSearchTolerance = 0;
	pes->ec.nReduced = 0;
	
	sscanf( pch + 1, "%f %f %f %f %d%c", &ar[ 0 ], &ar[ 1 ], &ar[ 2 ],
		&ar[ 3 ], &pes->ec.nPlies, &ch );

	pes->ec.fCubeful = ch == 'C';
	break;

    default:
	/* FIXME */
	break;
    }
}

static void RestoreMoveAnalysis( property *pp, int fPlayer,
				 movelist *pml, int *piMove ) {
    list *pl = pp->pl->plNext;
    char *pch, ch;
    move *pm;
    int i;
    
    *piMove = atoi( pl->p );

    for( pml->cMoves = 0, pl = pl->plNext; pl->p; pl = pl->plNext )
	pml->cMoves++;

    /* FIXME we could work these out, but it hardly seems worth it */
    pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
    pml->rBestScore = 0;
    
    pm = pml->amMoves = malloc( pml->cMoves * sizeof( move ) );

    for( pl = pp->pl->plNext->plNext; pl->p; pl = pl->plNext, pm++ ) {
	pch = pl->p;

	/* FIXME we could work these out, but it hardly seems worth it */
	pm->cMoves = pm->cPips = 0;
	pm->rScore2 = 0;
	
	for( i = 0; i < 4 && pch[ 0 ] && pch[ 1 ] && pch[ 0 ] != ' ';
	     pch += 2, i++ ) {
	    pm->anMove[ i << 1 ] = Point( pch[ 0 ], fPlayer );
	    pm->anMove[ ( i << 1 ) | 1 ] = Point( pch[ 1 ], fPlayer );
	}
	
	if( i < 4 )
	    pm->anMove[ i << 1 ] = -1;

	sscanf( pch, " %c", &ch );
	
	switch( ch ) {
	case 'E':
	    /* EVAL_EVAL */
	    pm->etMove = EVAL_EVAL;
	    pm->esMove.ec.nSearchCandidates = 0;
	    pm->esMove.ec.rSearchTolerance = 0;
	    pm->esMove.ec.nReduced = 0;

	    sscanf( pch, " %*c %f %f %f %f %f %f %d%c",
		    &pm->arEvalMove[ 0 ], &pm->arEvalMove[ 1 ],
		    &pm->arEvalMove[ 2 ], &pm->arEvalMove[ 3 ],
		    &pm->arEvalMove[ 4 ], &pm->rScore,
		    &pm->esMove.ec.nPlies, &ch );
	    pm->esMove.ec.fCubeful = ch == 'C';
	    break;
	    
	default:
	    /* FIXME */
	    break;
	}
    }
}

static void PointList( list *pl, int an[] ) {

    int i;
    char *pch, ch0, ch1;
    
    for( i = 0; i < 25; i++ )
	an[ i ] = 0;

    for( ; pl->p; pl = pl->plNext ) {
	pch = pl->p;

	if( strchr( pch, ':' ) ) {
	    ch0 = ch1 = 0;
	    sscanf( pch, "%c:%c", &ch0, &ch1 );
	    if( ch0 >= 'a' && ch1 <= 'y' && ch0 < ch1 )
		for( i = ch0 - 'a'; i < ch1 - 'a'; i++ )
		    an[ i ]++;
	} else if( *pch >= 'a' && *pch <= 'y' )
	    an[ *pch - 'a' ]++;
    }
}

static char *CopyEscapedString( char *pchOrig ) {

    char *sz, *pch;

    for( pch = sz = malloc( strlen( pchOrig ) + 1 ); *pchOrig; pchOrig++ ) {
	if( *pchOrig == '\\' ) {
	    if( pchOrig[ 1 ] == '\\' ) {
		*pch++ = '\\';
		pchOrig++;
	    }
	    continue;
	}

	if( isspace( *pchOrig ) && *pchOrig != '\n' ) {
	    *pch++ = ' ';
	    continue;
	}

	*pch++ = *pchOrig;
    }

    *pch = 0;
    
    return sz;
}

static void RestoreNode( list *pl ) {

    property *pp, *ppDA = NULL, *ppA = NULL, *ppC = NULL;
    moverecord *pmr = NULL;
    char *pch;
    int i, fPlayer, fSetBoard = FALSE, an[ 25 ];
    skilltype st = SKILL_NONE;
    lucktype lt = LUCK_NONE;
    float rLuck = -HUGE_VALF;
    
    for( pl = pl->plNext; ( pp = pl->p ); pl = pl->plNext ) {
	if( pp->ach[ 1 ] == 0 &&
	    ( pp->ach[ 0 ] == 'B' || pp->ach[ 0 ] == 'W' ) ) {
	    /* B or W - Move property. */
	    
	    if( pmr )
		/* Duplicate move -- ignore. */
		continue;
	    
	    pch = pp->pl->plNext->p;
	    fPlayer = pp->ach[ 0 ] == 'B';
	    
	    if( !strcmp( pch, "double" ) ) {
		pmr = malloc( sizeof( pmr->d ) );
		pmr->mt = MOVE_DOUBLE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.etDouble = EVAL_NONE;
	    } else if( !strcmp( pch, "take" ) ) {
		pmr = malloc( sizeof( pmr->t ) );
		pmr->mt = MOVE_TAKE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
	    } else if( !strcmp( pch, "drop" ) ) {
		pmr = malloc( sizeof( pmr->t ) );
		pmr->mt = MOVE_DROP;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
	    } else {
		pmr = malloc( sizeof( pmr->n ) );
		pmr->mt = MOVE_NORMAL;
		pmr->n.sz = NULL;
		pmr->n.fPlayer = fPlayer;
		pmr->n.ml.cMoves = 0;
		pmr->n.ml.amMoves = NULL;
		pmr->n.etDouble = EVAL_NONE;
		
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
			
		if( pmr->n.anRoll[ 0 ] < 1 || pmr->n.anRoll[ 0 ] > 6 ||
		    pmr->n.anRoll[ 1 ] < 1 || pmr->n.anRoll[ 1 ] > 6 ) {
		    /* illegal roll -- ignore */
		    free( pmr );
		    pmr = NULL;
		}
	    }
	} else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'E' ) {
	    fSetBoard = TRUE;
	    PointList( pp->pl->plNext, an );
	    for( i = 0; i < 25; i++ )
		if( an[ i ] ) {
		    anBoard[ 0 ][ i == 24 ? i : 23 - i ] = 0;
		    anBoard[ 1 ][ i ] = 0;
		}
	} else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'B' ) {
	    fSetBoard = TRUE;
	    PointList( pp->pl->plNext, an );
	    for( i = 0; i < 25; i++ )
		anBoard[ 0 ][ i == 24 ? i : 23 - i ] += an[ i ];
	} else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'W' ) {
	    fSetBoard = TRUE;
	    PointList( pp->pl->plNext, an );
	    for( i = 0; i < 25; i++ )
		anBoard[ 1 ][ i ] += an[ i ];
	} else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'L' )
	    fTurn = fMove = *( (char *) pp->pl->plNext->p ) == 'B';
	else if( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'V' ) {
	    for( i = 1; i <= MAX_CUBE; i <<= 1 )
		if( atoi( pp->pl->plNext->p ) == i ) {
		    pmr = malloc( sizeof( pmr->scv ) );
		    pmr->mt = MOVE_SETCUBEVAL;
		    pmr->scv.sz = NULL;
		    pmr->scv.nCube = i;
		    AddMoveRecord( pmr );
		    pmr = NULL;
		    break;
		}
	} else if( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'P' ) {
	    pmr = malloc( sizeof( pmr->scp ) );
	    pmr->mt = MOVE_SETCUBEPOS;
	    pmr->scp.sz = NULL;
	    switch( *( (char *) pp->pl->plNext->p ) ) {
	    case 'c':
		pmr->scp.fCubeOwner = -1;
		break;
	    case 'b':
		pmr->scp.fCubeOwner = 1;
		break;
	    case 'w':
		pmr->scp.fCubeOwner = 0;
		break;
	    default:
		free( pmr );
		pmr = NULL;
	    }

	    if( pmr ) {
		AddMoveRecord( pmr );
		pmr = NULL;
	    }
	} else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'I' ) {
	    char ach[ 2 ];
	    
	    sscanf( pp->pl->plNext->p, "%2c", ach );

	    if( ach[ 0 ] >= '1' && ach[ 0 ] <= '6' && ach[ 1 ] >= '1' &&
		ach[ 1 ] <= '6' ) {
		pmr = malloc( sizeof( pmr->sd ) );
		pmr->mt = MOVE_SETDICE;
		pmr->sd.sz = NULL;
		pmr->sd.fPlayer = fMove;
		pmr->sd.anDice[ 0 ] = ach[ 0 ] - '0';
		pmr->sd.anDice[ 1 ] = ach[ 1 ] - '0';
	    }
	} else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'A' )
	    /* double analysis */
	    ppDA = pp;
	else if( pp->ach[ 0 ] == 'A' && !pp->ach[ 1 ] )
	    /* move analysis */
	    ppA = pp;
	else if( pp->ach[ 0 ] == 'C' && !pp->ach[ 1 ] )
	    /* comment */
	    ppC = pp;
	else if( pp->ach[ 0 ] == 'B' && pp->ach[ 1 ] == 'M' )
	    st = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYBAD :
	    SKILL_BAD;
	else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'O' )
	    st = SKILL_DOUBTFUL;
	else if( pp->ach[ 0 ] == 'I' && pp->ach[ 1 ] == 'T' )
	    st = SKILL_INTERESTING;
	else if( pp->ach[ 0 ] == 'T' && pp->ach[ 1 ] == 'E' )
	    st = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYGOOD :
	    SKILL_GOOD;
	else if( pp->ach[ 0 ] == 'L' && pp->ach[ 1 ] == 'U' )
	    rLuck = atof( pp->pl->plNext->p );
	/* FIXME handle GB and GW */
    }
    
    if( fSetBoard && !pmr ) {
	pmr = malloc( sizeof( pmr->sb ) );
	pmr->mt = MOVE_SETBOARD;
	pmr->sb.sz = NULL;
	PositionKey( anBoard, pmr->sb.auchKey );
    }

    if( pmr && ppC )
	pmr->a.sz = CopyEscapedString( ppC->pl->plNext->p );
    
    if( pmr ) {
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, &pmr->n.etDouble,
				       pmr->n.arDouble, &pmr->n.esDouble );
	    if( ppA )
		RestoreMoveAnalysis( ppA, pmr->n.fPlayer, &pmr->n.ml,
				     &pmr->n.iMove );
	    pmr->n.st = st;
	    pmr->n.lt = lt;
	    pmr->n.rLuck = rLuck;
	    break;
	    
	case MOVE_DOUBLE:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, &pmr->d.etDouble,
				       pmr->d.arDouble, &pmr->d.esDouble );
	    pmr->d.st = st;
	    break;

	case MOVE_TAKE:
	case MOVE_DROP:
	    pmr->t.st = st;
	    break;
	    
	case MOVE_SETDICE:
	    pmr->sd.lt = lt;
	    pmr->sd.rLuck = rLuck;
	    break;
	    
	default:
	    /* FIXME allow comments for all movetypes */
	    break;
	}

	AddMoveRecord( pmr );
    }
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

    moverecord *pmr, *pmrResign;
    
    InitBoard( anBoard );

    /* FIXME should anything be done with the current game? */
    
    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    anDice[ 0 ] = anDice[ 1 ] = 0;
    fResigned = fDoubled = FALSE;
    nCube = 1;
    fTurn = fMove = fCubeOwner = -1;
    gs = GAME_NONE;
    
    RestoreTree( pl, TRUE );

    pmr = plGame->plNext->p;
    assert( pmr->mt == MOVE_GAMEINFO );

    AddGame( pmr );
    
    if( pmr->g.fResigned ) {
	fTurn = fMove = -1;
	
	pmrResign = malloc( sizeof( pmrResign ->r ) );
	pmrResign->mt = MOVE_RESIGN;
	pmrResign->r.sz = NULL;
	pmrResign->r.fPlayer = !pmr->g.fWinner;
	pmrResign->r.nResigned = pmr->g.nPoints / nCube;

	if( pmrResign->r.nResigned < 1 )
	    pmrResign->r.nResigned = 1;
	else if( pmrResign->r.nResigned > 3 )
	    pmrResign->r.nResigned = 3;

	AddMoveRecord( pmrResign );
    }
}

static void ClearMatch( void ) {

    nMatchTo = 0;

    cGames = anScore[ 0 ] = anScore[ 1 ] = 0;
    fMove = fTurn = -1;
    fCrawford = FALSE;
    fPostCrawford = FALSE;
    gs = GAME_NONE;
}

static void UpdateSettings( void ) {

    UpdateSetting( &nCube );
    UpdateSetting( &fCubeOwner );
    UpdateSetting( &fTurn );
    UpdateSetting( &nMatchTo );
    UpdateSetting( &fCrawford );

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
	if( gs == GAME_PLAYING && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to load a saved game, "
			     "and discard the one in progress? " ) )
		return;
	}

#if USE_GTK
	if( fX )
	    GTKFreeze();
#endif
	
	FreeMatch();
	ClearMatch();
	
	/* FIXME if pl contains multiple games, ask which one to load */

	RestoreGame( pl->plNext->p );
	
	FreeGameTreeSeq( pl );

	UpdateSettings();
	
#if USE_GTK
	if( fX )
	    GTKThaw();
#endif
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
	/* FIXME make sure the root nodes have MI properties; if not,
	   we're loading a session. */
	if( gs == GAME_PLAYING && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( "Are you sure you want to load a saved match, "
			     "and discard the game in progress? " ) )
		return;
	}

#if USE_GTK
	if( fX )
	    GTKFreeze();
#endif
	
	FreeMatch();
	ClearMatch();
	
	for( pl = pl->plNext; pl->p; pl = pl->plNext )
	    RestoreGame( pl->p );

	FreeGameTreeSeq( pl );

	UpdateSettings();
	
#if USE_GTK
	if( fX )
	    GTKThaw();
#endif
    }
}

static void WriteEscapedString( FILE *pf, char *pch, int fEscapeColons ) {

    for( ; *pch; pch++ )
	switch( *pch ) {
	case '\\':
	    putc( '\\', pf );
	    putc( '\\', pf );
	    break;
	case ':':
	    if( fEscapeColons )
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

static void WriteDoubleAnalysis( FILE *pf, evaltype et, float ar[],
				 evalsetup *pes ) {

    switch( et ) {
    case EVAL_EVAL:
	fprintf( pf, "DA[E %.4f %.4f %.4f %.4f %d%s]", ar[ 0 ], ar[ 1 ],
		 ar[ 2 ], ar[ 3 ], pes->ec.nPlies,
		 pes->ec.fCubeful ? "C" : "" );
	break;

    case EVAL_ROLLOUT:
	/* FIXME */

    default:
	assert( FALSE );
    }
}

static void WriteMove( FILE *pf, int fPlayer, int anMove[] ) {

    int i;

    for( i = 0; i < 8; i++ )
	switch( anMove[ i ] ) {
	case 24: /* bar */
	    putc( 'y', pf );
	    break;
	case -1: /* off */
	    if( !( i & 1 ) )
		return;
	    putc( 'z', pf );
	    break;
	default:
	    putc( fPlayer ? 'x' - anMove[ i ] : 'a' + anMove[ i ], pf );
	}
}

static void WriteMoveAnalysis( FILE *pf, int fPlayer, movelist *pml,
			       int iMove ) {

    int i;

    fprintf( pf, "A[%d]", iMove );

    for( i = 0; i < pml->cMoves; i++ ) {
	fputc( '[', pf );
	WriteMove( pf, fPlayer, pml->amMoves[ i ].anMove );
	fputc( ' ', pf );
	
	switch( pml->amMoves[ i ].etMove ) {
	case EVAL_NONE:
	    break;
	    
	case EVAL_EVAL:
	    fprintf( pf, "E %.4f %.4f %.4f %.4f %.4f %.4f %d%s",
		     pml->amMoves[ i ].arEvalMove[ 0 ],
		     pml->amMoves[ i ].arEvalMove[ 1 ],
		     pml->amMoves[ i ].arEvalMove[ 2 ],
		     pml->amMoves[ i ].arEvalMove[ 3 ],
		     pml->amMoves[ i ].arEvalMove[ 4 ],
		     pml->amMoves[ i ].rScore,
		     pml->amMoves[ i ].esMove.ec.nPlies,
		     pml->amMoves[ i ].esMove.ec.fCubeful ? "C" : "" );
	    break;
	    
	case EVAL_ROLLOUT:
	    /* FIXME */

	default:
	    assert( FALSE );
	}

	fputc( ']', pf );
    }
}

static void WriteLuck( FILE *pf, int fPlayer, float rLuck, lucktype lt ) {

    if( rLuck != -HUGE_VALF )
	fprintf( pf, "LU[%+.3f]", rLuck );
    
    switch( lt ) {
    case LUCK_VERYBAD:
	fprintf( pf, "G%c[2]", fPlayer ? 'W' : 'B' );
	break;
	
    case LUCK_BAD:
	fprintf( pf, "G%c[1]", fPlayer ? 'W' : 'B' );
	break;
	
    case LUCK_NONE:
	break;
	
    case LUCK_GOOD:
	fprintf( pf, "G%c[1]", fPlayer ? 'B' : 'W' );
	break;
	
    case LUCK_VERYGOOD:
	fprintf( pf, "G%c[2]", fPlayer ? 'B' : 'W' );
	break;	
    }
}

static void WriteSkill( FILE *pf, skilltype st ) {

    switch( st ) {
    case SKILL_VERYBAD:
	fputs( "BM[2]", pf );
	break;
	
    case SKILL_BAD:
	fputs( "BM[1]", pf );
	break;
	
    case SKILL_DOUBTFUL:
	fputs( "DO[]", pf );
	break;
	
    case SKILL_NONE:
	break;
	
    case SKILL_INTERESTING:
	fputs( "IT[]", pf );
	break;
	
    case SKILL_GOOD:
	fputs( "TE[1]", pf );
	break;
	
    case SKILL_VERYGOOD:
	fputs( "TE[2]", pf );
	break;
    }
}

static void SaveGame( FILE *pf, list *plGame ) {

    list *pl;
    moverecord *pmr;
    int i, j, anBoard[ 2 ][ 25 ];
    
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
    WriteEscapedString( pf, ap[ 0 ].szName, FALSE );
    fputs( "]PB[", pf );
    WriteEscapedString( pf, ap[ 1 ].szName, FALSE );
    putc( ']', pf );

    if( pmr->g.fCrawford ) {
	fputs( "RU[Crawford", pf );
	if( pmr->g.fCrawfordGame )
	    fputs( ":CrawfordGame", pf );
	putc( ']', pf );
    } else if( pmr->g.fJacoby )
	fputs( "RU[Jacoby]", pf );

    if( pmr->g.nAutoDoubles )
	fprintf( pf, "CV[%d]", 1 << pmr->g.nAutoDoubles );
    
    if( pmr->g.fWinner >= 0 )
	fprintf( pf, "RE[%c+%d%s]", pmr->g.fWinner ? 'B' : 'W',
		 pmr->g.nPoints, pmr->g.fResigned ? "R" : "" );

    for( pl = pl->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    fprintf( pf, "\n;%c[%d%d", pmr->n.fPlayer ? 'B' : 'W',
		     pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );
	    WriteMove( pf, pmr->n.fPlayer, pmr->n.anMove );
	    putc( ']', pf );

	    if( pmr->n.etDouble != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->n.etDouble, pmr->n.arDouble,
				     &pmr->n.esDouble );

	    if( pmr->n.ml.cMoves )
		WriteMoveAnalysis( pf, pmr->n.fPlayer, &pmr->n.ml,
				   pmr->n.iMove );

	    WriteLuck( pf, pmr->n.fPlayer, pmr->n.rLuck, pmr->n.lt );
	    WriteSkill( pf, pmr->n.st );
	    
	    break;
	    
	case MOVE_DOUBLE:
	    fprintf( pf, "\n;%c[double]", pmr->d.fPlayer ? 'B' : 'W' );

	    if( pmr->d.etDouble != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->d.etDouble, pmr->d.arDouble,
				     &pmr->d.esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    
	case MOVE_TAKE:
	    fprintf( pf, "\n;%c[take]", pmr->t.fPlayer ? 'B' : 'W' );

	    WriteSkill( pf, pmr->t.st );

	    break;
	    
	case MOVE_DROP:
	    fprintf( pf, "\n;%c[drop]", pmr->t.fPlayer ? 'B' : 'W' );

	    WriteSkill( pf, pmr->t.st );

	    break;
	    
	case MOVE_RESIGN:
	    break;

	case MOVE_SETBOARD:
	    PositionFromKey( anBoard, pmr->sb.auchKey );

	    fputs( "\n;AE[a:y]AW", pf );
	    for( i = 0; i < 25; i++ )
		for( j = 0; j < anBoard[ 1 ][ i ]; j++ )
		    fprintf( pf, "[%c]", 'a' + i );

	    fputs( "AB", pf );
	    for( i = 0; i < 25; i++ )
		for( j = 0; j < anBoard[ 0 ][ i ]; j++ )
		    fprintf( pf, "[%c]", i == 24 ? 'y' : 'x' - i );

	    break;
	    
	case MOVE_SETDICE:
	    fprintf( pf, "\n;PL[%c]DI[%d%d]", pmr->sd.fPlayer ? 'B' : 'W',
		     pmr->sd.anDice[ 0 ], pmr->sd.anDice[ 1 ] );
	    
	    WriteLuck( pf, pmr->sd.fPlayer, pmr->sd.rLuck, pmr->sd.lt );
	    
	    break;
	    
	case MOVE_SETCUBEVAL:
	    fprintf( pf, "\n;CV[%d]", pmr->scv.nCube );
	    break;
	    
	case MOVE_SETCUBEPOS:
	    fprintf( pf, "\n;CP[%c]", "cwb"[ pmr->scp.fCubeOwner + 1 ] );
	    break;
	    
	default:
	    assert( FALSE );
	}

	if( pmr->a.sz ) {
	    fputs( "C[", pf );
	    WriteEscapedString( pf, pmr->a.sz, FALSE );
	    putc( ']', pf );
	}
    }

    /* FIXME if the game is not over and the player on roll is the last
       player to move, add a PL property */

    fputs( ")\n", pf );
}

extern void CommandSaveGame( char *sz ) {

    FILE *pf;

    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to save to (see `help save "
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

    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
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
