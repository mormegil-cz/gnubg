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
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "analysis.h"
#include "positionid.h"
#include "sgf.h"
#include "i18n.h"

static char *szFile;
static int fError;

static void ErrorHandler( char *sz, int fParseError ) {

    if( !fError ) {
	fError = TRUE;
	outputerrf( "%s: %s", szFile, sz );
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

    PushLocale ( "C" );
    
    if( strcmp( sz, "-" ) ) {
	if( !( pf = fopen( sz, "r" ) ) ) {
	    outputerr( sz );
            PopLocale ();
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
	    ErrorHandler( _("warning: no backgammon games in SGF file"), TRUE );
	    free( plCollection );
            PopLocale ();
	    return NULL;
	}
    }
    
    PopLocale ();
    return plCollection;
}

static void CopyName( int i, char *sz, char *szCharset ) {

    char *pc;

    /* FIXME sanity check the name as in CommandSetPlayerName */
    
    pc = Convert ( sz, GNUBG_CHARSET, szCharset );
    
    if( strlen( pc ) > 31 )
      pc[ 31 ] = 0;

    strcpy( ap[ i ].szName, pc );

    free ( pc );

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
#if USE_TIMECONTROL
	else if( !strncmp( pch, "wtime:", 6 ) ) {
	    pmgi->tl[0].tv_sec= atoi( pch + 6 );
	} else if( !strncmp( pch, "btime:", 6 ) ) {
	    pmgi->tl[1].tv_sec= atoi( pch + 6 );
	} else if( !strncmp( pch, "wtimeouts:", 10 ) ) {
	    pmgi->nTimeouts[0] = atoi( pch + 10 );
	} else if( !strncmp( pch, "btimeouts:", 10 ) ) {
	    pmgi->nTimeouts[1] = atoi( pch + 10 );
	}	
#endif
	
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

	    for( st = SKILL_VERYBAD; st < N_SKILLS; st++ ) {
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
          break;
	}

    AddStatcontext( psc, &scMatch );
}

static char *CopyEscapedString( char *pchOrig, const char *szCharset ) {

    char *sz, *pch;
    char *pc;

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

    pc = Convert ( sz, GNUBG_CHARSET, szCharset );
    free ( sz );

    return pc;

}

static void RestoreText( char *sz, char **ppch, const char *szCharset ) {

    if( !sz || !*sz )
	return;
    
    if( *ppch )
	free( *ppch );
    
    *ppch = CopyEscapedString( sz, szCharset );
}

static void
RestoreRules( movegameinfo *pmgi, const char *sz ) {

  char *pch;
  char *pchx;

  /* split string at colons */

  pch = strdup( sz );

  pchx = strtok( pch, ":" );
  while ( pchx ) {

    if( !strcmp( pchx, "Crawford" ) )
      pmgi->fCrawford = TRUE;
    else if( !strcmp( pchx, "CrawfordGame" ) )
      pmgi->fCrawfordGame = TRUE;
    else if( !strcmp( pchx, "Jacoby" ) )
      pmgi->fJacoby = TRUE;
    else if( !strcmp( pchx, "Nackgammon" ) )
      pmgi->bgv = VARIATION_NACKGAMMON;
    else if( !strcmp( pchx, "Hypergammon1" ) )
      pmgi->bgv = VARIATION_HYPERGAMMON_1;
    else if( !strcmp( pchx, "Hypergammon2" ) )
      pmgi->bgv = VARIATION_HYPERGAMMON_2;
    else if( !strcmp( pchx, "Hypergammon3" ) )
      pmgi->bgv = VARIATION_HYPERGAMMON_3;
    else if( !strcmp( pchx, "NoCube" ) )
      pmgi->fCubeUse = FALSE;

    pchx = strtok( NULL, ":" );

  }

  free( pch );

}



static void RestoreRootNode( list *pl, char *szCharset ) {

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
    pmgi->bgv = VARIATION_STANDARD;
    pmgi->fCubeUse = TRUE;
#if USE_TIMECONTROL
    pmgi->tl[0].tv_sec = pmgi->tl[1].tv_sec = 0;
    pmgi->tl[0].tv_usec = pmgi->tl[1].tv_usec = 0;
    pmgi->nTimeouts[0] = pmgi->nTimeouts[1] = 0; 
#endif
    IniStatcontext( &pmgi->sc );
    
    for( pl = pl->plNext; ( pp = pl->p ); pl = pl->plNext ) 
	if( pp->ach[ 0 ] == 'M' && pp->ach[ 1 ] == 'I' )
	    /* MI - Match info property */
	    RestoreMI( pp->pl, pmgi );
        else if ( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'A' )
          /* CA - charset property */
          strcpy ( szCharset, (const char *) pp->pl->plNext->p );
	else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'B' )
	    /* PB - Black player property */
	    CopyName( 1, pp->pl->plNext->p, szCharset );
	else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'W' )
	    /* PW - White player property */
	    CopyName( 0, pp->pl->plNext->p, szCharset );
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
            RestoreRules( pmgi, (const char *) pp->pl->plNext->p );
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
	else if( pp->ach[ 0 ] == 'W' && pp->ach[ 1 ] == 'R' )
	    /* WR - White rank */
	    RestoreText( pp->pl->plNext->p, &mi.pchRating[ 0 ], szCharset );
	else if( pp->ach[ 0 ] == 'B' && pp->ach[ 1 ] == 'R' )
	    /* BR - Black rank */
	    RestoreText( pp->pl->plNext->p, &mi.pchRating[ 1 ], szCharset );
	else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'T' ) {
	    /* DT - Date */
	    int nYear, nMonth, nDay;
	    
	    if( pp->pl->plNext->p &&
		sscanf( pp->pl->plNext->p, "%d-%d-%d", &nYear, &nMonth,
			&nDay ) == 3 ) {
		mi.nYear = nYear;
		mi.nMonth = nMonth;
		mi.nDay = nDay;
	    }
	} else if( pp->ach[ 0 ] == 'E' && pp->ach[ 1 ] == 'V' )
	    /* EV - Event */
	    RestoreText( pp->pl->plNext->p, &mi.pchEvent, szCharset );
	else if( pp->ach[ 0 ] == 'R' && pp->ach[ 1 ] == 'O' )
	    /* RO - Round */
	    RestoreText( pp->pl->plNext->p, &mi.pchRound, szCharset );
	else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'C' )
	    /* PC - Place */
	    RestoreText( pp->pl->plNext->p, &mi.pchPlace, szCharset );
	else if( pp->ach[ 0 ] == 'A' && pp->ach[ 1 ] == 'N' )
	    /* AN - Annotator */
	    RestoreText( pp->pl->plNext->p, &mi.pchAnnotator, szCharset );
	else if( pp->ach[ 0 ] == 'G' && pp->ach[ 1 ] == 'C' )
	    /* GC - Game comment */
	    RestoreText( pp->pl->plNext->p, &mi.pchComment, szCharset );
    
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

static void
RestoreRolloutScore ( move *pm, const char *sz ) {

  char *pc = strstr ( sz, "Score" );

  pm->rScore = -99999;
  pm->rScore2 = -99999;

  if ( ! pc )
    return;

  sscanf ( pc, "Score %f %f",
           &pm->rScore, &pm->rScore2 );

}

static void
RestoreRolloutTrials ( int *piTrials, const char *sz ) {

  char *pc = strstr ( sz, "Trials" );

  *piTrials = 0;

  if ( ! pc )
    return;

  sscanf ( pc, "Trials %d", piTrials );

}

static void
RestoreRolloutOutput ( float ar[ NUM_ROLLOUT_OUTPUTS ], 
                       const char *sz, const char *szKeyword ) {

  char *pc = strstr ( sz, szKeyword );
  int i;
  
  for ( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
    ar[ i ] = 0.0;

  if ( ! pc )
    return;

  sscanf ( pc, "%*s %f %f %f %f %f %f %f",
           &ar[ 0 ], &ar[ 1 ], &ar[ 2 ], &ar[ 3 ], &ar[ 4 ],
           &ar[ 5 ], &ar[ 6 ] );

}

static void
InitEvalContext ( evalcontext *pec ) {

  pec->nPlies = 0;
  pec->fCubeful = FALSE;
  pec->nReduced = 0;
  pec->fDeterministic = FALSE;
  pec->rNoise = 0.0;

}


static void
RestoreEvalContext ( evalcontext *pec, const char *sz ) {

  int nPlies, nReduced, fDeterministic;
  char ch;

  InitEvalContext ( pec );

  sscanf ( sz, "%d%c %d %d %f",
           &nPlies, &ch, &nReduced, &fDeterministic, &pec->rNoise );

  pec->nPlies = nPlies;
  pec->fCubeful = ch == 'C';
  pec->nReduced = nReduced;
  pec->fDeterministic = fDeterministic;

}

static void
RestoreRolloutContextEvalContext ( evalcontext *pec, const char *sz,
                                   const char *szKeyword ) {

  char *pc = strstr ( sz, szKeyword );

  InitEvalContext ( pec );

  if ( ! pc )
    return;

  pc = strchr ( pc, ' ' );

  if ( ! pc )
    return;

  RestoreEvalContext ( pec, pc );

}


static void
RestoreRolloutRolloutContext ( rolloutcontext *prc, const char *sz ) {

  char *pc = strstr ( sz, "RC" );
  char szTemp[ 1024 ];
  int fCubeful, fVarRedn, fInitial, fRotate, fTruncBearoff2, fTruncBearoffOS;

  fCubeful = FALSE;
  fVarRedn = FALSE;
  fInitial = FALSE;
  prc->nTruncate = 0;
  prc->nTrials = 0;
  prc->rngRollout = RNG_MERSENNE;
  prc->nSeed = 0;
  fRotate = TRUE;
  fTruncBearoff2 = FALSE;
  fTruncBearoffOS = FALSE;
  /* set usable, but ignored values for everything else */
  prc->fLateEvals = 0;
  prc->fStopOnSTD = 0;
  prc->nLate = 0;
  prc->nMinimumGames = 144;
  prc->rStdLimit = 0.1;

  if ( ! pc )
    return;

  sscanf ( pc, "RC %d %d %d %hu %u \"%[^\"]\" %d %d %d %d",
           &fCubeful,
           &fVarRedn,
           &fInitial,
           &prc->nTruncate,
           &prc->nTrials,
           szTemp,
           &prc->nSeed,
           &fRotate,
           &fTruncBearoff2, 
           &fTruncBearoffOS );

  prc->fCubeful = fCubeful;
  prc->fVarRedn = fVarRedn;
  prc->fRotate = fRotate;
  prc->fInitial = fInitial;
  prc->fTruncBearoff2 = fTruncBearoff2;
  prc->fTruncBearoffOS = fTruncBearoffOS;

  RestoreRolloutContextEvalContext ( &prc->aecCube[ 0 ],
                                     sz, "cube0" );
  RestoreRolloutContextEvalContext ( &prc->aecCube[ 1 ],
                                     sz, "cube1" );
  RestoreRolloutContextEvalContext ( &prc->aecChequer[ 0 ],
                                     sz, "cheq0" );
  RestoreRolloutContextEvalContext ( &prc->aecChequer[ 1 ],
                                     sz, "cheq1" );

}


static void
RestoreRollout ( move *pm, const char *sz ) {
  
  int n;

  pm->esMove.et = EVAL_ROLLOUT;
  RestoreRolloutScore ( pm, sz );
  RestoreRolloutTrials ( &n, sz ); 
  RestoreRolloutOutput ( pm->arEvalMove, sz, "Output" ); 
  RestoreRolloutOutput ( pm->arEvalStdDev, sz, "StdDev" ); 
  RestoreRolloutRolloutContext ( &pm->esMove.rc, sz ); 

}


static void
RestoreCubeRolloutOutput ( float arOutput[], float arStdDev[],
                           const char *sz, const char *szKeyword ) {

  char *pc = strstr ( sz, szKeyword );

  memset ( arOutput, 0, NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  memset ( arStdDev, 0, NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

  if ( ! pc )
    return;

  RestoreRolloutOutput ( arOutput, pc, "Output" );
  RestoreRolloutOutput ( arStdDev, pc, "StdDev" );

}


static void
RestoreCubeRollout ( const char *sz,
                     float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                     float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                     evalsetup *pes ) {

  RestoreRolloutTrials ( &pes->rc.nGamesDone, sz );
  RestoreCubeRolloutOutput ( aarOutput[ 0 ], aarStdDev[ 0 ], sz, "NoDouble" );
  RestoreCubeRolloutOutput ( aarOutput[ 1 ], aarStdDev[ 1 ], 
                             sz, "DoubleTake" );
  RestoreRolloutRolloutContext ( &pes->rc, sz );

}



static void
RestoreRolloutInternals ( evalsetup *pes, const char *sz) {

  char *pc;
  if ((pc = strstr (sz, "SK")) != 0)
    sscanf (pc, "SK %d", &pes->rc.nSkip);
}

static int 
CheckExtendedRolloutVersion ( const char *sz ) {

  int	n;
  char  *pch = strstr (sz, "ver");
  if ((pch == 0) || (sscanf (pch, "ver %d", &n) != 1))
    return 0;

  return (n == SGF_ROLLOUT_VER);
}

static void
RestoreRolloutMoveFilter (const char *sz, char *name, 
		movefilter mf[MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
			  int nPlies) {

  char *pc = strstr (sz, name);
  int  i;
  int  consumed;

  if (pc == 0)
    return;

  pc += strlen (name);
  for (i = 0; i < nPlies; ++i) {
    if (sscanf (pc, "%d %d %f%n", &mf[ nPlies - 1 ][ i ].Accept,
		&mf[ nPlies - 1 ][ i ].Extra,
		&mf[ nPlies - 1][ i ].Threshold,
		&consumed) != 3)
      return;
    pc += consumed;
  }
}

static void 
RestoreExtendedRolloutContext (rolloutcontext *prc, const char *sz ) {

  char *pc = strstr ( sz, "RC" );
  char szTemp[ 1024 ];
  int fCubeful, fVarRedn, fInitial, fRotate, fTruncBearoff2, fTruncBearoffOS;
  int fLateEvals, fDoTruncate;
  int i;


  if ( ! pc )
    return;

  if (sscanf ( pc, "RC %d %d %d %d %d %d %hu %d %d %hu \"%[^\"]\" %d",
           &fCubeful,
           &fVarRedn,
           &fInitial,
	   &fRotate,
	   &fLateEvals,
	   &fDoTruncate,
           &prc->nTruncate,
	   &fTruncBearoff2,
	   &fTruncBearoffOS,
	   &prc->nLate,
           szTemp,
           &prc->nSeed) != 12)
    return;

  prc->fCubeful = fCubeful;
  prc->fVarRedn = fVarRedn;
  prc->fInitial = fInitial;
  prc->fRotate = fRotate;
  prc->fLateEvals = fLateEvals;
  prc->fDoTruncate = fDoTruncate;
  prc->fTruncBearoff2 = fTruncBearoff2;
  prc->fTruncBearoffOS = fTruncBearoffOS;
  prc->rngRollout = RNG_MERSENNE;
  prc->nMinimumGames = 144;
  prc->rStdLimit = 0.1;

  for (i = 0; i < 2; ++i) {
    sprintf (szTemp, "latecube%d ", i);
    RestoreRolloutContextEvalContext ( &prc->aecCube[ i ], sz, szTemp + 4 );
    RestoreRolloutContextEvalContext ( &prc->aecCubeLate[ i ], sz, szTemp );
    sprintf (szTemp, "latecheq%d", i);
    RestoreRolloutContextEvalContext ( &prc->aecChequer[ i ], sz, szTemp + 4 );
    RestoreRolloutContextEvalContext ( &prc->aecChequerLate[ i ], sz, szTemp );
    sprintf (szTemp, "latefilt%d ", i );
    if (prc->aecChequer[ i ].nPlies) {
      RestoreRolloutMoveFilter ( sz, szTemp + 4, prc->aaamfChequer[ i ],
				 prc->aecChequer[ i ].nPlies);
    }
    if (prc->aecChequerLate[ i ].nPlies) {
      RestoreRolloutMoveFilter ( sz, szTemp, prc->aaamfLate[ i ],
				 prc->aecChequerLate[ i ].nPlies);
    }
  }
  RestoreRolloutContextEvalContext ( &prc->aecCubeTrunc, sz, "cubetrunc");
  RestoreRolloutContextEvalContext ( &prc->aecCubeTrunc, sz, "cheqtrunc");


}
  
static void
RestoreExtendedCubeRollout ( const char *sz,
                     float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                     float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                     evalsetup *pes ) {
 
  int is_current;
  /* we assume new versions will still begin with Eq 4 floats
     Trials int 
     */
  is_current = CheckExtendedRolloutVersion (sz);

  RestoreRolloutTrials ( &pes->rc.nGamesDone, sz );
  RestoreCubeRolloutOutput ( aarOutput[ 0 ], aarStdDev[ 0 ], sz, "NoDouble" );
  RestoreCubeRolloutOutput ( aarOutput[ 1 ], aarStdDev[ 1 ],
                             sz, "DoubleTake" );
  if (is_current) {
    RestoreRolloutInternals ( pes, sz );
    RestoreExtendedRolloutContext ( &pes->rc, sz );
  }
}

static void
RestoreExtendedRollout ( move *pm, const char *sz) {

  evalsetup *pes = &pm->esMove;
  int is_current;

  /* we assume new versions will still begin with Score 2 floats
     Trials int 
     */
  is_current = CheckExtendedRolloutVersion (sz);
  pes->et = EVAL_ROLLOUT;
  RestoreRolloutScore ( pm, sz );
  RestoreRolloutTrials ( &pes->rc.nGamesDone, sz );
  RestoreRolloutOutput ( pm->arEvalMove, sz, "Output" ); 
  RestoreRolloutOutput ( pm->arEvalStdDev, sz, "StdDev" ); 
  if (is_current) {
    RestoreRolloutInternals ( pes, sz );
    RestoreExtendedRolloutContext ( &pes->rc, sz );
  }
}

static void RestoreDoubleAnalysis( property *pp,
                                   float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                                   float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                                   evalsetup *pes ) {
    
    char *pch = pp->pl->plNext->p, ch;
    int nPlies, nReduced, fDeterministic;
    /* FIXME: removing arUnued will break existing SGF files */
    float arUnused[ 4 ];
    
    switch( *pch ) {
    case 'E':
	/* EVAL_EVAL */
	pes->et = EVAL_EVAL;
	nReduced = 0;
        pes->ec.rNoise = 0.0f;
        fDeterministic = TRUE;
        memset ( aarOutput[ 0 ], 0, NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
        memset ( aarOutput[ 1 ], 0, NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
        aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] = -20000.0;
        aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] = -20000.0;
	
	sscanf( pch + 1, "%f %f %f %f %d%c %d %d %f"
                "%f %f %f %f %f %f %f" 
                "%f %f %f %f %f %f %f", 
                &arUnused[ 0 ], &arUnused[ 1 ], 
                &arUnused[ 2 ], &arUnused[ 3 ], 
                &nPlies, &ch,
                &nReduced, 
                &fDeterministic,
                &pes->ec.rNoise,
                &aarOutput[ 0 ][ 0 ], &aarOutput[ 0 ][ 1 ], 
                &aarOutput[ 0 ][ 2 ], &aarOutput[ 0 ][ 3 ], 
                &aarOutput[ 0 ][ 4 ], &aarOutput[ 0 ][ 5 ], 
                &aarOutput[ 0 ][ 6 ],
                &aarOutput[ 1 ][ 0 ], &aarOutput[ 1 ][ 1 ], 
                &aarOutput[ 1 ][ 2 ], &aarOutput[ 1 ][ 3 ], 
                &aarOutput[ 1 ][ 4 ], &aarOutput[ 1 ][ 5 ], 
                &aarOutput[ 1 ][ 6 ] );

	pes->ec.nPlies = nPlies;
        pes->ec.nReduced = nReduced;
        pes->ec.fDeterministic = fDeterministic;
	pes->ec.fCubeful = ch == 'C';

	break;

    case 'R':

      pes->et = EVAL_ROLLOUT;
      RestoreCubeRollout ( pch + 1, aarOutput, aarStdDev, pes );
      break;

    case 'X':

      pes->et = EVAL_ROLLOUT;
      RestoreExtendedCubeRollout (pch + 1, aarOutput, aarStdDev, pes );
      break;

    default:
	/* FIXME */
	break;
    }
}


static void RestoreMoveAnalysis( property *pp, int fPlayer,
				 movelist *pml, int *piMove, 
                                 evalsetup *pesChequer,
                                 const matchstate *pms ) {
    list *pl = pp->pl->plNext;
    char *pch, ch;
    move *pm;
    int i, nPlies, fDeterministic, nReduced;
    int anBoardMove[ 2 ][ 25 ];
    
    *piMove = atoi( pl->p );

    for( pml->cMoves = 0, pl = pl->plNext; pl->p; pl = pl->plNext )
	pml->cMoves++;

    /* FIXME we could work these out, but it hardly seems worth it */
    pml->cMaxMoves = pml->cMaxPips = pml->iMoveBest = 0;
    pml->rBestScore = 0;
    
    pm = pml->amMoves = calloc( pml->cMoves, sizeof( move ) );

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

        /* restore auch */

        memcpy ( anBoardMove, pms->anBoard, sizeof ( anBoardMove ) );
        ApplyMove ( anBoardMove, pm->anMove, FALSE );
        PositionKey ( anBoardMove, pm->auch );

	sscanf( pch, " %c", &ch );
	
	switch( ch ) {
	case 'E':
	    /* EVAL_EVAL */
	    pm->esMove.et = EVAL_EVAL;
	    nReduced = 0;
            fDeterministic = 0;

	    sscanf( pch, " %*c %f %f %f %f %f %f %d%c %d %d %f",
		    &pm->arEvalMove[ 0 ], &pm->arEvalMove[ 1 ],
		    &pm->arEvalMove[ 2 ], &pm->arEvalMove[ 3 ],
		    &pm->arEvalMove[ 4 ], &pm->rScore,
		    &nPlies, 
                    &ch, 
                    &nReduced, 
                    &fDeterministic, 
                    &pm->esMove.ec.rNoise );


	    pm->esMove.ec.fCubeful = ch == 'C';
	    pm->esMove.ec.nPlies = nPlies;
	    pm->esMove.ec.nReduced = nReduced;
	    pm->esMove.ec.fDeterministic = fDeterministic;
	    break;

        case 'R':

          RestoreRollout ( pm, pch );
          break;
	    
	case 'X':
	  RestoreExtendedRollout ( pm, pch );
	  break;

	default:
	    /* FIXME */
	    break;
	}

	/* save "largest" evalsetup */

    if ( cmp_evalsetup ( pesChequer, &pm->esMove ) < 0 )
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

static void RestoreNode( list *pl, char *szCharset ) {

    property *pp, *ppDA = NULL, *ppA = NULL, *ppC = NULL;
    moverecord *pmr = NULL;
    char *pch;
    int i, fPlayer = 0, fSetBoard = FALSE, an[ 25 ];
    skilltype ast[ 2 ] = { SKILL_NONE, SKILL_NONE };
    lucktype lt = LUCK_NONE;
    float rLuck = ERR_VAL;

#if USE_TIMECONTROL
	int fTimeset=0;
#endif
    
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

		pmr = calloc( 1, sizeof( pmr->d ) );
		pmr->mt = MOVE_DOUBLE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		if( ! LinkToDouble( pmr ) ) {
		  pmr->d.nAnimals = 0;
		pmr->d.CubeDecPtr = &pmr->d.CubeDec;
		pmr->d.CubeDecPtr->esDouble.et = EVAL_NONE;
		}
	    } else if( !strcmp( pch, "take" ) ) {
		pmr = calloc( 1, sizeof( pmr->d ) );
		pmr->mt = MOVE_TAKE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.st = SKILL_NONE;
		if (!LinkToDouble( pmr ) ) {
		  free (pmr);
		  continue;
		}
	    } else if( !strcmp( pch, "drop" ) ) {
		pmr = calloc( 1, sizeof( pmr->d ) );
		pmr->mt = MOVE_DROP;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.st = SKILL_NONE;
		if (!LinkToDouble( pmr ) ) {
		  free (pmr);
		  continue;
		}
	    } else {
		pmr = calloc( 1, sizeof( pmr->n ) );
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
	} else if( pp->ach[ 0 ] == 'P' && pp->ach[ 1 ] == 'L' ) {
	    int fTurnNew = *( (char *) pp->pl->plNext->p ) == 'B';
	    if( ms.fMove != fTurnNew )
		SwapSides( ms.anBoard );
	    ms.fTurn = ms.fMove = fTurnNew;
	} else if( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'V' ) {
	    for( i = 1; i <= MAX_CUBE; i <<= 1 )
		if( atoi( pp->pl->plNext->p ) == i ) {
		    pmr = calloc( 1, sizeof( pmr->scv ) );
		    pmr->mt = MOVE_SETCUBEVAL;
		    pmr->scv.sz = NULL;
		    pmr->scv.nCube = i;
		    AddMoveRecord( pmr );
		    pmr = NULL;
		    break;
		}
	} else if( pp->ach[ 0 ] == 'C' && pp->ach[ 1 ] == 'P' ) {
	    pmr = calloc( 1, sizeof( pmr->scp ) );
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
		pmr = calloc( 1, sizeof( pmr->sd ) );
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
	    ast[ 0 ] 
              = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYBAD :
	    SKILL_BAD;
	else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'O' )
	    ast[ 0 ] = SKILL_DOUBTFUL;
/* 	else if( pp->ach[ 0 ] == 'I' && pp->ach[ 1 ] == 'T' ) */
/* 	    ast[ 0 ] = SKILL_INTERESTING; */
/* 	else if( pp->ach[ 0 ] == 'T' && pp->ach[ 1 ] == 'E' ) */
/* 	    ast[ 0 ]  */
/*               = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYGOOD : */
/* 	    SKILL_GOOD; */
	else if( pp->ach[ 0 ] == 'B' && pp->ach[ 1 ] == 'C' )
	    ast[ 1 ] 
              = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYBAD :
	    SKILL_BAD;
	else if( pp->ach[ 0 ] == 'D' && pp->ach[ 1 ] == 'C' )
	    ast[ 1 ] = SKILL_DOUBTFUL;
/* 	else if( pp->ach[ 0 ] == 'I' && pp->ach[ 1 ] == 'C' ) */
/* 	    ast[ 1 ] = SKILL_INTERESTING; */
/* 	else if( pp->ach[ 0 ] == 'T' && pp->ach[ 1 ] == 'C' ) */
/* 	    ast[ 1 ]  */
/*               = *( (char *) pp->pl->plNext->p ) == '2' ? SKILL_VERYGOOD : */
/* 	    SKILL_GOOD; */
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
#if USE_TIMECONTROL
	else if( pp->ach[ 0 ] == 'W' && pp->ach[ 1 ] == 'L' ) {
	    pmr->a.tl[0].tv_sec = atoi ( (char *) pp->pl->plNext->p );
	    fTimeset |= 1;
	}
	else if( pp->ach[ 0 ] == 'B' && pp->ach[ 1 ] == 'L' ) {
	    pmr->a.tl[1].tv_sec = atoi ( (char *) pp->pl->plNext->p );
	    fTimeset |= 2;
	}
	else if( ( pp->ach[ 0 ] == 'W' || pp->ach[ 0 ] == 'B') && pp->ach[ 1 ] == 'X' ) {
	    pmr = malloc( sizeof( pmr->t ) );
	    pmr->t.mt = MOVE_TIME;
	    pmr->t.sz = 0;
	    pmr->t.nPoints = atoi ( (char *) pp->pl->plNext->p );
	    pmr->t.fPlayer = (pp->ach[ 0 ] == 'W');
	}
#endif
    }
    
    if( fSetBoard && !pmr ) {
	pmr = calloc( 1, sizeof( pmr->sb ) );
	pmr->mt = MOVE_SETBOARD;
	pmr->sb.sz = NULL;
	ClosestLegalPosition( ms.anBoard );
	PositionKey( ms.anBoard, pmr->sb.auchKey );
    }

#if USE_TIMECONTROL
    /* Compensate for mising timestamps */
    if (pmr && !(fTimeset&1))
	pmr->a.tl[0]= ((moverecord *) (plLastMove->p))->a.tl[0];

    if (pmr && !(fTimeset&2))
	pmr->a.tl[1]= ((moverecord *) (plLastMove->p))->a.tl[1];
#endif

    if( pmr && ppC )
	pmr->a.sz = CopyEscapedString( ppC->pl->plNext->p, szCharset );

    if( pmr ) {
      
        FixMatchState( &ms, pmr );

	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, 
                                       pmr->n.aarOutput,
                                       pmr->n.aarStdDev,
                                       &pmr->n.esDouble );
	    if( ppA )
		RestoreMoveAnalysis( ppA, pmr->n.fPlayer, &pmr->n.ml,
				     &pmr->n.iMove, &pmr->n.esChequer,
                                     &ms );
            /* FIXME: separate st's */
	    pmr->n.stMove = ast[ 0 ];
	    pmr->n.stCube = ast[ 1 ];
	    pmr->n.lt = fPlayer ? lt : LUCK_VERYGOOD - lt;
	    pmr->n.rLuck = rLuck;
	    break;
	    
	case MOVE_DOUBLE:
	case MOVE_TAKE:
	case MOVE_DROP:
	    if( ppDA )
		RestoreDoubleAnalysis( ppDA, 
							   pmr->d.CubeDecPtr->aarOutput,
							   pmr->d.CubeDecPtr->aarStdDev,
							   &pmr->d.CubeDecPtr->esDouble );
	    pmr->d.st = ast[ 0 ];
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

static void RestoreSequence( list *pl, int fRoot, char *szCharset ) {

    pl = pl->plNext;
    if( fRoot )
	RestoreRootNode( pl->p, szCharset);
    else
	RestoreNode( pl->p, szCharset );

    while( pl = pl->plNext, pl->p )
	RestoreNode( pl->p, szCharset );
}

static void RestoreTree( list *pl, int fRoot, char *szCharset ) {
    
    pl = pl->plNext;
    RestoreSequence( pl->p, fRoot, szCharset );

    pl = pl->plNext;
    if( pl->p )
	RestoreTree( pl->p, FALSE, szCharset );

    /* FIXME restore other variations, once we can handle them */
}

static void RestoreGame( list *pl, char *szCharset ) {

    moverecord *pmr, *pmrResign;

    PushLocale ( "C" );
    
    InitBoard( ms.anBoard, ms.bgv );

    /* FIXME should anything be done with the current game? */
    
    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    ms.anDice[ 0 ] = ms.anDice[ 1 ] = 0;
    ms.fResigned = ms.fDoubled = FALSE;
    ms.nCube = 1;
    ms.fTurn = ms.fMove = ms.fCubeOwner = -1;
    ms.gs = GAME_NONE;
#if USE_TIMECONTROL
    ms.nTimeouts[0] = ms.nTimeouts[1] = 0;
    ms.gc.pc[0].tc.timing = ms.gc.pc[1].tc.timing =  TC_NONE;
#endif
    
    RestoreTree( pl, TRUE, szCharset );

    pmr = plGame->plNext->p;
    assert( pmr->mt == MOVE_GAMEINFO );

    AddGame( pmr );
    
#if USE_TIMECONTROL
    if( pmr->g.tl[0].tv_sec && pmr->g.tl[1].tv_sec) 
	ms.gc.pc[0].tc.timing = ms.gc.pc[1].tc.timing =  TC_UNKNOWN;
#endif
    if( pmr->g.fResigned ) {
	ms.fTurn = ms.fMove = -1;
	
	pmrResign = calloc( 1, sizeof( pmrResign ->r ) );
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

    PopLocale ();

}

extern void CommandLoadGame( char *sz ) {

    list *pl;
    char szCharset[ 80 ] = "ISO-8859-1";

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to load from (see `help load "
		 "game').") );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	if( ms.gs == GAME_PLAYING && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( _("Are you sure you want to load a saved game, "
			     "and discard the one in progress? ") ) )
		return;
	}

#if USE_GTK
	if( fX )
	    GTKFreeze();
#endif
	
	FreeMatch();
	ClearMatch();
	
	/* FIXME if pl contains multiple games, ask which one to load */

	RestoreGame( pl->plNext->p, szCharset );
	
	FreeGameTreeSeq( pl );

	UpdateSettings();
	
#if USE_GTK
	if( fX ){
	    GTKThaw();
	    GTKSet(ap);
        }

        setDefaultFileName ( sz, PATH_SGF );

#endif

        if ( fGotoFirstGame )
          CommandFirstGame( NULL );

    }
}


extern void 
CommandLoadPosition( char *sz ) {

    list *pl;
    char szCharset[ 80 ] = "ISO-8859-1";

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to load from (see `help load "
		 "position').") );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	if( ms.gs == GAME_PLAYING && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( _("Are you sure you want to load a saved position, "
			     "and discard the match in progress? ") ) )
		return;
	}

#if USE_GTK
	if( fX )
	    GTKFreeze();
#endif
	
	FreeMatch();
	ClearMatch();
	
	/* FIXME if pl contains multiple games, ask which one to load */

	RestoreGame( pl->plNext->p, szCharset );
	
	FreeGameTreeSeq( pl );

	UpdateSettings();
	
#if USE_GTK
	if( fX ){
	    GTKThaw();
	    GTKSet(ap);
        }

        setDefaultFileName ( sz, PATH_SGF );

#endif

    }
}


extern void CommandLoadMatch( char *sz ) {

    list *pl;
    char szCharset[ 80 ];

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to load from (see `help load "
		 "match').") );
	return;
    }

    if( ( pl = LoadCollection( sz ) ) ) {
	/* FIXME make sure the root nodes have MI properties; if not,
	   we're loading a session. */
	if( ms.gs == GAME_PLAYING && fConfirm ) {
	    if( fInterrupt )
		return;
	    
	    if( !GetInputYN( _("Are you sure you want to load a saved match, "
			     "and discard the game in progress? ") ) )
		return;
	}

#if USE_GTK
	if( fX )
	    GTKFreeze();
#endif
	
	FreeMatch();
	ClearMatch();
	
	for( pl = pl->plNext; pl->p; pl = pl->plNext ) {
          strcpy ( szCharset, "ISO-8859-1" );
          RestoreGame( pl->p, szCharset );
        }

	FreeGameTreeSeq( pl );

	UpdateSettings();
	
#if USE_GTK
	if( fX ){
	    GTKThaw();
	    GTKSet(ap);
        }
#endif

        setDefaultFileName ( sz, PATH_SGF );

        if ( fGotoFirstGame )
          CommandFirstGame( NULL );

    }
}

static void WriteEscapedString( FILE *pf, char *pch, int fEscapeColons ) {

  char *sz, *pc;

  sz = (char *) malloc ( 2 * strlen ( pch ) + 1 );

  for( pc = sz; *pch; pch++ )
    switch( *pch ) {
    case '\\':
      *pc++ = '\\';
      *pc++ = '\\';
      break;
    case ':':
      if ( fEscapeColons )
        *pc++ = '\\';
      *pc++ = ':';
      break;
    case ']':
      *pc++ = '\\';
      *pc++ = ']';
      break;
    default:
      *pc++ = *pch;
      break;
    }

  *pc++ = 0;


  /* convert string */

  pc = Convert ( sz, "UTF-8", GNUBG_CHARSET );

  if ( pc ) {
    fputs ( pc, pf );
    free ( pc );
  }

  free ( sz );

}

static void
WriteEvalContext ( FILE *pf, const evalcontext *pec ) {

  fprintf( pf, "%d%s %d %d %.4f",
           pec->nPlies,
           pec->fCubeful ? "C" : "",
           pec->nReduced, 
           pec->fDeterministic, 
           pec->rNoise );

}

static void
WriteMoveFilters (FILE *pf, 
		  const movefilter mf[MAX_FILTER_PLIES][MAX_FILTER_PLIES], 
		  int nPlies ) {
  int   i;

  for (i = 0; i < nPlies; ++i) {
    fprintf (pf, "%d %d %.5f ", mf[ nPlies - 1 ][ i ].Accept,
	     mf[ nPlies - 1 ][ i ].Extra,
	     mf[ nPlies - 1 ][ i ].Threshold);
  }
}

static void
WriteRolloutContext ( FILE *pf, const rolloutcontext *prc ) {
  
  int i;

  fprintf ( pf, "RC %d %d %d %d %d %d %d %d %d %d \"%s\" %d ",
            prc->fCubeful,
            prc->fVarRedn,
            prc->fInitial,
            prc->fRotate,
            prc->fLateEvals,
	    prc->fDoTruncate,
            prc->nTruncate,
            prc->fTruncBearoff2, 
            prc->fTruncBearoffOS,
	    prc->nLate,
            aszRNG[ prc->rngRollout ],
            prc->nSeed);

    for ( i = 0; i < 2; i++ ) {
      fprintf ( pf, " cube%d ", i );
      WriteEvalContext ( pf, &prc->aecCube[ i ] );
      fputc ( ' ', pf );

      fprintf ( pf, " cheq%d ", i );
      WriteEvalContext ( pf, &prc->aecChequer[ i ] );
      if (prc->aecChequer[ i ].nPlies) {
	fprintf (pf, " filt%d ", i);
	WriteMoveFilters (pf, prc->aaamfChequer[ i ], 
			prc->aecChequer[ i ].nPlies );
      }
  }

  for ( i = 0; i < 2; i++ ) {
    fprintf ( pf, " latecube%d ", i );
    WriteEvalContext ( pf, &prc->aecCubeLate[ i ] );
    fputc ( ' ', pf );

    fprintf ( pf, " latecheq%d ", i );
    WriteEvalContext ( pf, &prc->aecChequerLate[ i ] );
    if (prc->aecChequerLate[ i ].nPlies) {
      fprintf (pf, " latefilt%d ", i);
      WriteMoveFilters (pf, prc->aaamfLate[i], 
		      prc->aecChequerLate[ i ].nPlies );
    }
  }

  fprintf ( pf, " cubetrunc ");
  WriteEvalContext ( pf, &prc->aecCubeTrunc);
  fprintf ( pf, " cheqtrunc ");
  WriteEvalContext ( pf, &prc->aecChequerTrunc);

}

static void WriteRolloutAnalysis( FILE *pf, int fIsMove, 
				  float rScore, float rScore2,
				  float aarOutput0[ NUM_ROLLOUT_OUTPUTS ],
				  float aarOutput1[ NUM_ROLLOUT_OUTPUTS ],
				  float aarStdDev0[ NUM_ROLLOUT_OUTPUTS ],
				  float aarStdDev1[ NUM_ROLLOUT_OUTPUTS ],
				 evalsetup *pes ) {

  int i;

  /* identify what version we're writing to avoid future problems 
     the version is defined in eval.h
     */
  if (fIsMove) {
    fprintf ( pf, "X ver %d Score %.10g %.10g ", SGF_ROLLOUT_VER,
	      rScore, rScore2);
  } else {
    fprintf ( pf, "X ver %d Eq ", SGF_ROLLOUT_VER);
  }

  fprintf ( pf, "Trials %d ", pes->rc.nGamesDone);
  if (!fIsMove) 
    fprintf (pf, "NoDouble ");

  fprintf (pf, "Output ");
  for (i = 0; i < 7; ++i) 
    fprintf ( pf, "%.10g ", aarOutput0[ i] );

  fprintf ( pf, "StdDev ");
  for (i = 0; i < 7; ++i)
    fprintf ( pf, "%.10g ", aarStdDev0[ i] );

  if (!fIsMove) {
    fprintf ( pf, "DoubleTake Output ");
    for (i = 0; i < 7; ++i) 
      fprintf ( pf, "%.10g ", aarOutput1[ i] );

    fprintf ( pf, "StdDev ");
    for (i = 0; i < 7; ++i)
      fprintf ( pf, "%.10f ", aarStdDev1[ i] );
  }

  fprintf (pf, "SK %d ", pes->rc.nSkip);
  WriteRolloutContext ( pf, &pes->rc );
}

static void WriteDoubleAnalysis( FILE *pf, 
                                 float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                                 float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
				 evalsetup *pes ) {

  /* FIXME: removing write of 0 0 0 0 will break existing SGF files */

  fputs ( "DA[", pf );

  switch( pes->et ) {
  case EVAL_EVAL:
    fprintf( pf, "E %.4f %.4f %.4f %.4f %d%s %d %d %.4f "
             "%.4f %.4f %.4f %.4f %.4f %.4f %.4f "
             "%.4f %.4f %.4f %.4f %.4f %.4f %.4f", 
             0.0f, 0.0f, 0.0f, 0.0f,
             pes->ec.nPlies,
             pes->ec.fCubeful ? "C" : "",
             pes->ec.nReduced,
             pes->ec.fDeterministic,
             pes->ec.rNoise,
             aarOutput[ 0 ][ 0 ], aarOutput[ 0 ][ 1 ], 
             aarOutput[ 0 ][ 2 ], aarOutput[ 0 ][ 3 ], 
             aarOutput[ 0 ][ 4 ], aarOutput[ 0 ][ 5 ], 
             aarOutput[ 0 ][ 6 ],
             aarOutput[ 1 ][ 0 ], aarOutput[ 1 ][ 1 ], 
             aarOutput[ 1 ][ 2 ], aarOutput[ 1 ][ 3 ], 
             aarOutput[ 1 ][ 4 ], aarOutput[ 1 ][ 5 ], 
             aarOutput[ 1 ][ 6 ] );
    break;
    
  case EVAL_ROLLOUT:

    WriteRolloutAnalysis (pf, 0, 0.0f, 0.0f, aarOutput[0], 
			  aarOutput[1], aarStdDev[0], aarStdDev[1], pes);
    break;
    
  default:
    assert( FALSE );
  }
  
  fputc ( ']', pf );

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
	    fprintf( pf, "E %.4f %.4f %.4f %.4f %.4f %.4f %d%s "
                     "%d %d %.4f",
		     pml->amMoves[ i ].arEvalMove[ 0 ],
		     pml->amMoves[ i ].arEvalMove[ 1 ],
		     pml->amMoves[ i ].arEvalMove[ 2 ],
		     pml->amMoves[ i ].arEvalMove[ 3 ],
		     pml->amMoves[ i ].arEvalMove[ 4 ],
		     pml->amMoves[ i ].rScore,
		     pml->amMoves[ i ].esMove.ec.nPlies,
		     pml->amMoves[ i ].esMove.ec.fCubeful ? "C" : "",
                     pml->amMoves[ i ].esMove.ec.nReduced, 
                     pml->amMoves[ i ].esMove.ec.fDeterministic, 
                     pml->amMoves[ i ].esMove.ec.rNoise );
	    break;
	    
	case EVAL_ROLLOUT:
              WriteRolloutAnalysis (pf, 1, pml->amMoves[ i ].rScore, 
				    pml->amMoves[ i ].rScore2, 
				    pml->amMoves[ i ].arEvalMove, 0, 
				    pml->amMoves[ i ].arEvalStdDev, 0,
				    &pml->amMoves[ i ].esMove);
	      break;

	  

	default:
	    assert( FALSE );
	}

	fputc( ']', pf );
    }
}

static void WriteLuck( FILE *pf, int fPlayer, float rLuck, lucktype lt ) {

    if( rLuck != ERR_VAL )
	fprintf( pf, "LU[%+.5f]", rLuck );
    
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

static void
WriteSkill ( FILE *pf, const skilltype st ) {

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

    case SKILL_GOOD: 
      fputs( "TE[1]", pf ); 
      break; 
	
/*     case SKILL_INTERESTING: */
/* 	fputs( "IT[]", pf ); */
/* 	break; */
	
/*     case SKILL_GOOD: */
/* 	fputs( "TE[1]", pf ); */
/* 	break; */
	
/*     case SKILL_VERYGOOD: */
/* 	fputs( "TE[2]", pf ); */
/* 	break; */
    }
}

static void
WriteSkillCube ( FILE *pf, const skilltype st ) {

    switch( st ) {
    case SKILL_VERYBAD:
	fputs( "BC[2]", pf );
	break;
	
    case SKILL_BAD:
	fputs( "BC[1]", pf );
	break;
	
    case SKILL_DOUBTFUL:
	fputs( "DC[]", pf );
	break;
	
    case SKILL_NONE:
	break;
	
/*     case SKILL_INTERESTING: */
/* 	fputs( "IC[]", pf ); */
/* 	break; */
	
    case SKILL_GOOD:
	fputs( "TC[1]", pf );
	break;
	
/*     case SKILL_VERYGOOD: */
/* 	fputs( "TC[2]", pf ); */
/* 	break; */
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
	for( st = SKILL_VERYBAD; st < N_SKILLS; st++ )
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
	fprintf( pf, "%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f "
		     "%.6f %.6f %.6f %.6f ",
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
	fprintf( pf, "%.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f "
		     "%.6f %.6f %.6f %.6f]",
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

static void WriteProperty( FILE *pf, char *szName, char *szValue ) {

    if( !szValue || !*szValue )
	return;
    
    fputs( szName, pf );
    putc( '[', pf );
    WriteEscapedString( pf, szValue, FALSE );
    putc( ']', pf );
}

static void
AddRule( FILE *pf, const char *sz, int *pfFirst ) {

  fprintf( pf, "%s%s", ( *pfFirst ) ? "" : ":", sz );
  *pfFirst = FALSE;

}

static void SaveGame( FILE *pf, list *plGame ) {

    list *pl;
    moverecord *pmr;
    int i, j, anBoard[ 2 ][ 25 ];

    updateStatisticsGame ( plGame );
    PushLocale ( "C" );
    
    pl = plGame->plNext;
    pmr = pl->p;
    assert( pmr->mt == MOVE_GAMEINFO );
    
    /* Fixed header */
    fputs( "(;FF[4]GM[6]CA[UTF-8]AP[GNU Backgammon:" VERSION "]", pf );

    /* Match length, if appropriate */
    /* FIXME: isn't it always appropriate to write this? */
    /* If not, money games will be loaded without score and game number */
    /* if( pmr->g.nMatch ) */
#if USE_TIMECONTROL
    fprintf( pf, "MI[length:%d][game:%d][ws:%d][bs:%d][wtime:%d][btime:%d][wtimeouts:%d][btimeouts:%d]", pmr->g.nMatch,
             pmr->g.i, pmr->g.anScore[ 0 ], pmr->g.anScore[ 1 ],
	     (int)pmr->g.tl[0].tv_sec, (int)pmr->g.tl[1].tv_sec, pmr->g.nTimeouts[0], pmr->g.nTimeouts[1]);
#else
    fprintf( pf, "MI[length:%d][game:%d][ws:%d][bs:%d]", pmr->g.nMatch,
             pmr->g.i, pmr->g.anScore[ 0 ], pmr->g.anScore[ 1 ] );
#endif
    
    /* Names */
    fputs( "PW[", pf );
    WriteEscapedString( pf, ap[ 0 ].szName, FALSE );
    fputs( "]PB[", pf );
    WriteEscapedString( pf, ap[ 1 ].szName, FALSE );
    putc( ']', pf );

    if( !pmr->g.i ) {
	char szDate[ 32 ];
	
	WriteProperty( pf, "WR", mi.pchRating[ 0 ] );
	WriteProperty( pf, "BR", mi.pchRating[ 1 ] );
	if( mi.nYear ) {
	    sprintf( szDate, "%04d-%02d-%02d", mi.nYear, mi.nMonth, mi.nDay );
	    WriteProperty( pf, "DT", szDate );
	}
	WriteProperty( pf, "EV", mi.pchEvent );
	WriteProperty( pf, "RO", mi.pchRound );
	WriteProperty( pf, "PC", mi.pchPlace );
	WriteProperty( pf, "AN", mi.pchAnnotator );
	WriteProperty( pf, "GC", mi.pchComment );
    }

    if ( pmr->g.fCrawford || pmr->g.fJacoby || 
         pmr->g.bgv != VARIATION_STANDARD || ! pmr->g.fCubeUse ) {

      static char *aszSGFVariation[ NUM_VARIATIONS ] =
        { NULL, "Nackgammon", "Hypergammon1", "Hypergammon2", "Hypergammon3" };
      int fFirst = TRUE;

      fputs( "RU[", pf );
      
      if( !pmr->g.fCubeUse ) 
        AddRule( pf, "NoCube", &fFirst );
      if( pmr->g.fCrawford )
        AddRule( pf, "Crawford", &fFirst );
      if( pmr->g.fCrawfordGame )
        AddRule( pf, "CrawfordGame", &fFirst );
      if( pmr->g.fJacoby ) 
        AddRule( pf, "Jacoby", &fFirst );
      if( pmr->g.bgv != VARIATION_STANDARD )
        AddRule( pf, aszSGFVariation[ pmr->g.bgv ], &fFirst );

      fputs( "]", pf );
    }

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

#if USE_TIMECONTROL
	    fprintf( pf, "WL[%d]BL[%d]", (int)pmr->n.tl[0].tv_sec, (int)pmr->n.tl[1].tv_sec);
#endif
	    if( pmr->n.esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, 
                                     pmr->n.aarOutput, pmr->n.aarStdDev,
				     &pmr->n.esDouble );

	    if( pmr->n.ml.cMoves )
		WriteMoveAnalysis( pf, pmr->n.fPlayer, &pmr->n.ml,
				   pmr->n.iMove );

	    WriteLuck( pf, pmr->n.fPlayer, pmr->n.rLuck, pmr->n.lt );
            /* FIXME: separate skill for cube and move */
	    WriteSkill( pf, pmr->n.stMove );
	    WriteSkillCube( pf, pmr->n.stCube );
	    
	    break;
	    
	case MOVE_DOUBLE:
	    fprintf( pf, "\n;%c[double]", pmr->d.fPlayer ? 'B' : 'W' );

#if USE_TIMECONTROL
	    fprintf( pf, "WL[%d]BL[%d]", (int)pmr->d.tl[0].tv_sec, (int)pmr->d.tl[1].tv_sec);
#endif
	    if( pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, 
				     pmr->d.CubeDecPtr->aarOutput, 
				     pmr->d.CubeDecPtr->aarStdDev,
				     &pmr->d.CubeDecPtr->esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    
	case MOVE_TAKE:
	    fprintf( pf, "\n;%c[take]", pmr->d.fPlayer ? 'B' : 'W' );

#if USE_TIMECONTROL
	    fprintf( pf, "WL[%d]BL[%d]", (int)pmr->d.tl[0].tv_sec, (int)pmr->d.tl[1].tv_sec);
#endif
	    if( pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, 
				     pmr->d.CubeDecPtr->aarOutput, 
				     pmr->d.CubeDecPtr->aarStdDev,
				     &pmr->d.CubeDecPtr->esDouble );
	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    
	case MOVE_DROP:
	    fprintf( pf, "\n;%c[drop]", pmr->d.fPlayer ? 'B' : 'W' );

#if USE_TIMECONTROL
	    fprintf( pf, "WL[%d]BL[%d]", (int)pmr->d.tl[0].tv_sec, (int)pmr->d.tl[1].tv_sec);
#endif
	    if( pmr->d.CubeDecPtr->esDouble.et != EVAL_NONE )
		WriteDoubleAnalysis( pf, 
				     pmr->d.CubeDecPtr->aarOutput,
				     pmr->d.CubeDecPtr->aarStdDev,
				     &pmr->d.CubeDecPtr->esDouble );

	    
	    WriteSkill( pf, pmr->d.st );
	    
	    break;
	    	    
	case MOVE_RESIGN:
	    break;

	case MOVE_SETBOARD:
	    PositionFromKey( anBoard, pmr->sb.auchKey );

	    fputs( "\n;AE[a:y]", pf );

            for ( i = 0, j = 0; i < 25; ++i )
              j += anBoard[ 1 ][ i ];

            if ( j ) {
              fputs( "AW", pf );
              for( i = 0; i < 25; i++ )
		for( j = 0; j < anBoard[ 1 ][ i ]; j++ )
                  fprintf( pf, "[%c]", 'a' + i );
            }

            for ( i = 0, j = 0; i < 25; ++i )
              j += anBoard[ 0 ][ i ];

            if ( j ) {
              fputs( "AB", pf );
              for( i = 0; i < 25; i++ )
		for( j = 0; j < anBoard[ 0 ][ i ]; j++ )
                  fprintf( pf, "[%c]", i == 24 ? 'y' : 'x' - i );
            }

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
	    
#if USE_TIMECONTROL
	case MOVE_TIME:
	    fprintf( pf, "\n;%cX[%d]",  ! pmr->sd.fPlayer ? 'B' : 'W',
		pmr->t.nPoints);
	    fprintf( pf, "WL[%d]BL[%d]", (int)pmr->t.tl[0].tv_sec, (int)pmr->t.tl[1].tv_sec);
	    break;
#endif

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

    PopLocale ();
}

extern void CommandSaveGame( char *sz ) {

    FILE *pf;

    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to save to (see `help save "
		 "game').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	outputerr( sz );
	return;
    }

    SaveGame( pf, plGame );
    
    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_SGF );

}

extern void CommandSaveMatch( char *sz ) {

    FILE *pf;
    list *pl;

    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    /* FIXME what should be done if nMatchTo == 0? */
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to save to (see `help save "
		 "match').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	outputerr( sz );
	return;
    }

    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
	SaveGame( pf, pl->p );
    
    if( pf != stdout )
	fclose( pf );

    setDefaultFileName ( sz, PATH_SGF );

}

extern void CommandSavePosition( char *sz ) {

    FILE *pf;
    list l;
    movegameinfo *pmgi;
    movesetboard *pmsb;
    movesetdice *pmsd;
    movesetcubeval *pmscv;
    movesetcubepos *pmscp;
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( _("No game in progress (type `new game' to start one).") );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to save to (see `help save "
		 "position').") );
	return;
    }

    if ( ! confirmOverwrite ( sz, fConfirmSave ) )
      return;

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	outputerr( sz );
	return;
    }

    ListCreate( &l );
    
    pmgi = calloc( 1, sizeof( movegameinfo ) );
    pmgi->mt = MOVE_GAMEINFO;
    pmgi->sz = NULL;
    pmgi->i = 0;
    pmgi->nMatch = ms.nMatchTo;
    pmgi->anScore[ 0 ] = ms.anScore[ 0 ];
    pmgi->anScore[ 1 ] = ms.anScore[ 1 ];
    pmgi->fCrawford = fAutoCrawford && ms.nMatchTo > 1;
    pmgi->fCrawfordGame = ms.fCrawford;
    pmgi->fJacoby = ms.fJacoby && !ms.nMatchTo;
    pmgi->fWinner = -1;
    pmgi->nPoints = 0;
    pmgi->fResigned = FALSE;
    pmgi->nAutoDoubles = 0;
    pmgi->bgv = ms.bgv;
    pmgi->fCubeUse = ms.fCubeUse;
    IniStatcontext( &pmgi->sc );
    ListInsert( &l, pmgi );

    pmsb = calloc( 1, sizeof( movesetboard ) );
    pmsb->mt = MOVE_SETBOARD;
    pmsb->sz = NULL;
    if( ms.fMove )
	SwapSides( ms.anBoard );
    PositionKey( ms.anBoard, pmsb->auchKey );
    if( ms.fMove )
	SwapSides( ms.anBoard );
    ListInsert( &l, pmsb );

    pmscv = calloc( 1, sizeof( movesetcubeval ) );
    pmscv->mt = MOVE_SETCUBEVAL;
    pmscv->sz = NULL;
    pmscv->nCube = ms.nCube;
    ListInsert( &l, pmscv );
    
    pmscp = calloc( 1, sizeof( movesetcubepos ) );
    pmscp->mt = MOVE_SETCUBEPOS;
    pmscp->sz = NULL;
    pmscp->fCubeOwner = ms.fCubeOwner;
    ListInsert( &l, pmscp );
    
    /* FIXME if the dice are not rolled, this should be done with a PL
       property (which is SaveGame()'s job) */
    pmsd = calloc( 1, sizeof( movesetdice ) );
    pmsd->mt = MOVE_SETDICE;
    pmsd->sz = NULL;
    pmsd->fPlayer = ms.fMove;
    pmsd->anDice[ 0 ] = ms.anDice[ 0 ];
    pmsd->anDice[ 1 ] = ms.anDice[ 1 ];
    pmsd->lt = LUCK_NONE;
    pmsd->rLuck = ERR_VAL;
    ListInsert( &l, pmsd );

    /* FIXME add MOVE_DOUBLE record(s) as appropriate */
    
    SaveGame( pf, &l );

    if( pf != stdout )
	fclose( pf );

    while( l.plNext->p )
	ListDelete( l.plNext );
    
    free( pmgi );
    free( pmsb );
    free( pmsd );
    free( pmscv );
    free( pmscp );
    
    setDefaultFileName ( sz, PATH_SGF );
}
