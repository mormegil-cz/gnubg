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

static void RestoreGS( list *pl, statcontext *psc ) {

    char *pch;
    lucktype lt;
    skilltype st;
    
    for( pl = pl->plNext; ( pch = pl->p ); pl = pl->plNext )
	switch( *pch ) {
	case 'M': /* moves */
	    psc->fMoves = TRUE;
	    
	    psc->anUnforcedMoves[ 0 ] = strtol( pch + 2, &pch, 10 );
	    psc->anUnforcedMoves[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anTotalMoves[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anTotalMoves[ 1 ] = strtol( pch, &pch, 10 );

	    for( st = SKILL_VERYBAD; st <= SKILL_VERYGOOD; st++ ) {
		psc->anMoves[ 0 ][ st ] = strtol( pch, &pch, 10 );
		psc->anMoves[ 1 ][ st ] = strtol( pch, &pch, 10 );
	    }

	    psc->arErrorCheckerplay[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorCheckerplay[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorCheckerplay[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorCheckerplay[ 1 ][ 1 ] = strtod( pch, &pch );
	    
	    break;
	    
	case 'C': /* cube */
	    psc->fCube = TRUE;
	    
	    psc->anTotalCube[ 0 ] = strtol( pch + 2, &pch, 10 );
	    psc->anTotalCube[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anDouble[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anDouble[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anTake[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anTake[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anPass[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anPass[ 1 ] = strtol( pch, &pch, 10 );

	    psc->anCubeMissedDoubleDP[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeMissedDoubleDP[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anCubeMissedDoubleTG[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeMissedDoubleTG[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongDoubleDP[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongDoubleDP[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongDoubleTG[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongDoubleTG[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongTake[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongTake[ 1 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongPass[ 0 ] = strtol( pch, &pch, 10 );
	    psc->anCubeWrongPass[ 1 ] = strtol( pch, &pch, 10 );
	    
	    psc->arErrorMissedDoubleDP[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleDP[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleTG[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleTG[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleDP[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleDP[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleTG[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleTG[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongTake[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongTake[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongPass[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongPass[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleDP[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleDP[ 1 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleTG[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorMissedDoubleTG[ 1 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleDP[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleDP[ 1 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleTG[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongDoubleTG[ 1 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongTake[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongTake[ 1 ][ 1 ] = strtod( pch, &pch );
	    psc->arErrorWrongPass[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arErrorWrongPass[ 1 ][ 1 ] = strtod( pch, &pch );
	    
	    break;
	    
	case 'D': /* dice */
	    psc->fDice = TRUE;
	    
	    pch += 2;
	    
	    for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ ) {
		psc->anLuck[ 0 ][ lt ] = strtol( pch, &pch, 10 );
		psc->anLuck[ 1 ][ lt ] = strtol( pch, &pch, 10 );
	    }

	    psc->arLuck[ 0 ][ 0 ] = strtod( pch, &pch );
	    psc->arLuck[ 0 ][ 1 ] = strtod( pch, &pch );
	    psc->arLuck[ 1 ][ 0 ] = strtod( pch, &pch );
	    psc->arLuck[ 1 ][ 1 ] = strtod( pch, &pch );
	    
	    break;
	    
	default:
	    /* ignore */
	}

    AddStatcontext( psc, &scMatch );
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
    IniStatcontext( &pmgi->sc );
    
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
	} else if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'S' )
	    /* GS - Game statistics */
	    RestoreGS( pp->pl, &pmgi->sc );

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

static void RestoreDoubleAnalysis( property *pp,
				   float ar[], evalsetup *pes ) {
    
    char *pch = pp->pl->plNext->p, ch;
    int nPlies;
    
    switch( *pch ) {
    case 'E':
	/* EVAL_EVAL */
	pes->et = EVAL_EVAL;
	pes->ec.nSearchCandidates = 0;
	pes->ec.rSearchTolerance = 0;
	pes->ec.nReduced = 0;
	
	sscanf( pch + 1, "%f %f %f %f %d%c", &ar[ 0 ], &ar[ 1 ], &ar[ 2 ],
		&ar[ 3 ], &nPlies, &ch );

	pes->ec.nPlies = nPlies;

	pes->ec.fCubeful = ch == 'C';
	break;

    default:
	/* FIXME */
	break;
    }
}

static void RestoreMoveAnalysis( property *pp, int fPlayer,
				 movelist *pml, int *piMove, evalsetup *pesChequer ) {
    list *pl = pp->pl->plNext;
    char *pch, ch;
    move *pm;
    int i, nPlies;
    
    *piMove = atoi( pl->p );

    for( pml->cMoves = 0, pl = pl->plNext; pl->p; pl = pl->plNext )
	pml->cMoves++;

    /* FIXME we could work these out, but it hardly seems worth it */
    pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
    pml->rBestScore = 0;
    
    pm = pml->amMoves = malloc( pml->cMoves * sizeof( move ) );

	pesChequer->et = EVAL_NONE;

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
	    pm->esMove.et = EVAL_EVAL;
	    pm->esMove.ec.nSearchCandidates = 0;
	    pm->esMove.ec.rSearchTolerance = 0;
	    pm->esMove.ec.nReduced = 0;

	    sscanf( pch, " %*c %f %f %f %f %f %f %d%c",
		    &pm->arEvalMove[ 0 ], &pm->arEvalMove[ 1 ],
		    &pm->arEvalMove[ 2 ], &pm->arEvalMove[ 3 ],
		    &pm->arEvalMove[ 4 ], &pm->rScore,
		    &nPlies, &ch );
	    pm->esMove.ec.fCubeful = ch == 'C';
	    pm->esMove.ec.nPlies = nPlies;
	    
	    break;
	    
	default:
	    /* FIXME */
	    break;
	}

	/* save "largest" evalsetup */

    if ( cmp_evalsetup ( pesChequer, &pm->esMove ) > 0 )
	   memcpy ( pesChequer, &pm->esMove, sizeof ( evalsetup ) );


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
    int i, fPlayer = 0, fSetBoard = FALSE, an[ 25 ];
    skilltype st = SKILL_NONE;
    lucktype lt = LUCK_NONE;
    float rLuck = ERR_VAL;
    
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
		pmr->d.esDouble.et = EVAL_NONE;
	    } else if( !strcmp( pch, "take" ) ) {
		pmr = malloc( sizeof( pmr->d ) );
		pmr->mt = MOVE_TAKE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.esDouble.et = EVAL_NONE;
	    } else if( !strcmp( pch, "drop" ) ) {
		pmr = malloc( sizeof( pmr->d ) );
		pmr->mt = MOVE_DROP;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.esDouble.et = EVAL_NONE;
	    } else {
		pmr = malloc( sizeof( pmr->n ) );
		pmr->mt = MOVE_NORMAL;
		pmr->n.sz = NULL;
		pmr->n.fPlayer = fPlayer;
		pmr->n.ml.cMoves = 0;
		pmr->n.ml.amMoves = NULL;
                pmr->n.esChequer.et = EVAL_NONE;
		pmr->n.esDouble.et = EVAL_NONE;
		
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
		    ms.anBoard[ 0 ][ i == 24 ? i : 23 - i ] = 0;
		    ms.anBoard[ 1 ][ i ] = 0;
		}
	} else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'B' ) {
	    fSetBoard = TRUE;
	    PointList( pp->pl->plNext, an );
	    for( i = 0; i < 25; i++ )
		ms.anBoard[ 0 ][ i == 24 ? i : 23 - i ] += an[ i ];
	} else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'W' ) {
	    fSetBoard = TRUE;
	    PointList( pp->pl->plNext, an );
	    for( i = 0; i < 25; i++ )
		ms.anBoard[ 1 ][ i ] += an[ i ];
	} else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'L' )
	    ms.fTurn = ms.fMove = *( (char *) pp->pl->plNext->p ) == 'B';
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
		pmr->sd.fPlayer = ms.fMove;
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
	else if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'B' )
	    /* good for black */
	    lt = *( (char *) pp->pl->plNext->p ) == '2' ? LUCK_VERYGOOD :
	    LUCK_GOOD;
	else if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'W' )
	    /* good for white */
	    lt = *( (char *) pp->pl->plNext->p ) == '2' ? LUCK_VERYBAD :
	    LUCK_BAD;
    }
    
    if( fSetBoard && !pmr ) {
	pmr = malloc( sizeof( pmr->sb ) );
	pmr->mt = MOVE_SETBOARD;
	pmr->sb.sz = NULL;
	PositionKey( ms.anBoard, pmr->sb.auchKey );
    }

    if( pmr && ppC )
	pmr->a.sz = CopyEscapedString( ppC->pl->plNext->p );
    
    if( pmr ) {
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, 
				       pmr->n.arDouble, &pmr->n.esDouble );
	    if( ppA )
		RestoreMoveAnalysis( ppA, pmr->n.fPlayer, &pmr->n.ml,
				     &pmr->n.iMove, &pmr->n.esChequer );
            /* FIXME: separate st's */
	    pmr->n.stMove = st;
	    pmr->n.stCube = st;
	    pmr->n.lt = fPlayer ? lt : LUCK_VERYGOOD - lt;
	    pmr->n.rLuck = rLuck;
	    break;
	    
	case MOVE_DOUBLE:
	case MOVE_TAKE:
	case MOVE_DROP:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, 
				       pmr->d.arDouble, &pmr->d.esDouble );
	    pmr->d.st = st;
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
    
    InitBoard( ms.anBoard );

    /* FIXME should anything be done with the current game? */
    
    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    ms.anDice[ 0 ] = ms.anDice[ 1 ] = 0;
    ms.fResigned = ms.fDoubled = FALSE;
    ms.nCube = 1;
    ms.fTurn = ms.fMove = ms.fCubeOwner = -1;
    ms.gs = GAME_NONE;
    
    RestoreTree( pl, TRUE );

    pmr = plGame->plNext->p;
    assert( pmr->mt == MOVE_GAMEINFO );

    AddGame( pmr );
    
    if( pmr->g.fResigned ) {
	ms.fTurn = ms.fMove = -1;
	
	pmrResign = malloc( sizeof( pmrResign ->r ) );
	pmrResign->mt = MOVE_RESIGN;
	pmrResign->r.sz = NULL;
        pmrResign->r.esResign.et = EVAL_NONE;
	pmrResign->r.fPlayer = !pmr->g.fWinner;
	pmrResign->r.nResigned = pmr->g.nPoints / ms.nCube;

	if( pmrResign->r.nResigned < 1 )
	    pmrResign->r.nResigned = 1;
	else if( pmrResign->r.nResigned > 3 )
	    pmrResign->r.nResigned = 3;

	AddMoveRecord( pmrResign );
    }
}

extern void CommandLoadGame( char *sz ) {

    list *pl;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "game')." );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	if( ms.gs == GAME_PLAYING && fConfirm ) {
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
	if( fX ){
	    GTKThaw();
	    GTKSet(ap);
        }
#endif
    }
}

extern void CommandLoadMatch( char *sz ) {

    list *pl;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "match')." );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	/* FIXME make sure the root nodes have MI properties; if not,
	   we're loading a session. */
	if( ms.gs == GAME_PLAYING && fConfirm ) {
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
	if( fX ){
	    GTKThaw();
	    GTKSet(ap);
        }
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

static void WriteDoubleAnalysis( FILE *pf, float ar[],
				 evalsetup *pes ) {
    switch( pes->et ) {
    case EVAL_EVAL:
	fprintf( pf, "DA[E %.4f %.4f %.4f %.4f %d%s]", ar[ 0 ], ar[ 1 ],
		 ar[ 2 ], ar[ 3 ], pes->ec.nPlies,
		 pes->ec.fCubeful ? "C" : "" );
	break;

    case EVAL_ROLLOUT:
	/* FIXME save rollout analysis */

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
	
	switch( pml->amMoves[ i ].esMove.et ) {
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

    if( rLuck != ERR_VAL )
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

static void WriteStatContext( FILE *pf, statcontext *psc ) {

    lucktype lt;
    skilltype st;
    
    fputs( "GS", pf );
    
    if( psc->fMoves ) {
	fprintf( pf, "[M:%d %d %d %d ", psc->anUnforcedMoves[ 0 ],
		 psc->anUnforcedMoves[ 1 ], psc->anTotalMoves[ 0 ],
		 psc->anTotalMoves[ 1 ] );
	for( st = SKILL_VERYBAD; st <= SKILL_VERYGOOD; st++ )
	    fprintf( pf, "%d %d ", psc->anMoves[ 0 ][ st ],
		     psc->anMoves[ 1 ][ st ] );
	fprintf( pf, "%.6f %.6f %.6f %.6f]", psc->arErrorCheckerplay[ 0 ][ 0 ],
		 psc->arErrorCheckerplay[ 0 ][ 1 ],
		 psc->arErrorCheckerplay[ 1 ][ 0 ],
		 psc->arErrorCheckerplay[ 1 ][ 1 ] );
    }

    if( psc->fCube ) {
	fprintf( pf, "[C:%d %d %d %d %d %d %d %d ", psc->anTotalCube[ 0 ],
		 psc->anTotalCube[ 1 ], psc->anDouble[ 0 ], psc->anDouble[ 1 ],
		 psc->anTake[ 0 ], psc->anTake[ 1 ], psc->anPass[ 0 ],
		 psc->anPass[ 1 ] );
	fprintf( pf, "%d %d %d %d %d %d %d %d %d %d %d %d ",
		 psc->anCubeMissedDoubleDP[ 0 ],
		 psc->anCubeMissedDoubleDP[ 1 ],
		 psc->anCubeMissedDoubleTG[ 0 ],
		 psc->anCubeMissedDoubleTG[ 1 ],
		 psc->anCubeWrongDoubleDP[ 0 ], psc->anCubeWrongDoubleDP[ 1 ],
		 psc->anCubeWrongDoubleTG[ 0 ], psc->anCubeWrongDoubleTG[ 1 ],
		 psc->anCubeWrongTake[ 0 ], psc->anCubeWrongTake[ 1 ],
		 psc->anCubeWrongPass[ 0 ], psc->anCubeWrongPass[ 1 ] );
	fprintf( pf, "%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f "
		 "%.6f ",
		 psc->arErrorMissedDoubleDP[ 0 ][ 0 ],
		 psc->arErrorMissedDoubleDP[ 0 ][ 1 ],
		 psc->arErrorMissedDoubleTG[ 0 ][ 0 ],
		 psc->arErrorMissedDoubleTG[ 0 ][ 1 ],
		 psc->arErrorWrongDoubleDP[ 0 ][ 0 ],
		 psc->arErrorWrongDoubleDP[ 0 ][ 1 ],
		 psc->arErrorWrongDoubleTG[ 0 ][ 0 ],
		 psc->arErrorWrongDoubleTG[ 0 ][ 1 ],
		 psc->arErrorWrongTake[ 0 ][ 0 ],
		 psc->arErrorWrongTake[ 0 ][ 1 ],
		 psc->arErrorWrongPass[ 0 ][ 0 ],
		 psc->arErrorWrongPass[ 0 ][ 1 ] );
	fprintf( pf, "%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f "
		 "%.6f]",
		 psc->arErrorMissedDoubleDP[ 1 ][ 0 ],
		 psc->arErrorMissedDoubleDP[ 1 ][ 1 ],
		 psc->arErrorMissedDoubleTG[ 1 ][ 0 ],
		 psc->arErrorMissedDoubleTG[ 1 ][ 1 ],
		 psc->arErrorWrongDoubleDP[ 1 ][ 0 ],
		 psc->arErrorWrongDoubleDP[ 1 ][ 1 ],
		 psc->arErrorWrongDoubleTG[ 1 ][ 0 ],
		 psc->arErrorWrongDoubleTG[ 1 ][ 1 ],
		 psc->arErrorWrongTake[ 1 ][ 0 ],
		 psc->arErrorWrongTake[ 1 ][ 1 ],
		 psc->arErrorWrongPass[ 1 ][ 0 ],
		 psc->arErrorWrongPass[ 1 ][ 1 ] );
    }
    
    if( psc->fDice ) {
	fputs( "[D:", pf );
	for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ )
	    fprintf( pf, "%d %d ", psc->anLuck[ 0 ][ lt ],
		     psc->anLuck[ 1 ][ lt ] );
	fprintf( pf, "%.6f %.6f %.6f %.6f]", psc->arLuck[ 0 ][ 0 ],
		 psc->arLuck[ 0 ][ 1 ], psc->arLuck[ 1 ][ 0 ],
		 psc->arLuck[ 1 ][ 1 ] );
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

    if( pmr->g.sc.fMoves || pmr->g.sc.fCube || pmr->g.sc.fDice )
	WriteStatContext( pf, &pmr->g.sc );

    for( pl = pl->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    fprintf( pf, "\n;%c[%d%d", pmr->n.fPlayer ? 'B' : 'W',
		     pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );
	    WriteMove( pf, pmr->n.fPlayer, pmr->n.anMove );
	    putc( ']', pf );

	    if( pmr->n.esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->n.arDouble,
				     &pmr->n.esDouble );

	    if( pmr->n.ml.cMoves )
		WriteMoveAnalysis( pf, pmr->n.fPlayer, &pmr->n.ml,
				   pmr->n.iMove );

	    WriteLuck( pf, pmr->n.fPlayer, pmr->n.rLuck, pmr->n.lt );
            /* FIXME: separate skill for cube and move */
	    WriteSkill( pf, pmr->n.stMove );
	    
	    break;
	    
	case MOVE_DOUBLE:
	    fprintf( pf, "\n;%c[double]", pmr->d.fPlayer ? 'B' : 'W' );

	    if( pmr->d.esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->d.arDouble,
				     &pmr->d.esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    
	case MOVE_TAKE:
	    fprintf( pf, "\n;%c[take]", pmr->d.fPlayer ? 'B' : 'W' );

	    if( pmr->d.esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->d.arDouble,
				     &pmr->d.esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    
	case MOVE_DROP:
	    fprintf( pf, "\n;%c[drop]", pmr->d.fPlayer ? 'B' : 'W' );

	    if( pmr->d.esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, pmr->d.arDouble,
				     &pmr->d.esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
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

    sz = NextToken( &sz );
    
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

    sz = NextToken( &sz );
    
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
