/*
 * database.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999, 2000, 2001.
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

#if HAVE_LIBGDBM
#include <gdbm.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#include "database.h"
#include "positionid.h"
#include "rollout.h"
#include "i18n.h"

#if HAVE_LIBGDBM

#if !HAVE_GDBM_ERRNO && HAVE_IMP_GDBM_ERRNO
/* Kludge to access gdbm_errno in Windows DLLs that won't let us use
   gdbm_errno directly. */
extern gdbm_error *_imp__gdbm_errno;
#define gdbm_errno (*_imp__gdbm_errno)
#endif

static char *szDatabase = "gnubg.gdbm";

extern void CommandDatabaseDump( char *sz ) {
    
    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation *pev;
    int c = 0;
    void *p;
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_READER, 0, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }
    
    dKey = gdbm_firstkey( pdb );
    
    while( dKey.dptr ) {
	if( !( dValue = gdbm_fetch( pdb, dKey ) ).dptr ) {
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );
	    
	    gdbm_close( pdb );

	    return;
	}

	pev = (dbevaluation *) dValue.dptr;
	
	if( !c )
	    outputl( _("Position       Win   W(g)  W(bg) L(g)  L(bg) Trials "
		     "Date") );
	
	outputf( "%14s %5.3f %5.3f %5.3f %5.3f %5.3f %6d %s",
		 PositionIDFromKey( (unsigned char *) dKey.dptr ),
		 (float) pev->asEq[ OUTPUT_WIN ] / 0xFFFF,
		 (float) pev->asEq[ OUTPUT_WINGAMMON ] / 0xFFFF,
		 (float) pev->asEq[ OUTPUT_WINBACKGAMMON ] / 0xFFFF,
		 (float) pev->asEq[ OUTPUT_LOSEGAMMON ] / 0xFFFF,
		 (float) pev->asEq[ OUTPUT_LOSEBACKGAMMON ] / 0xFFFF,
		 pev->c, pev->t ? ctime( &pev->t ) : _(" (no rollout data)\n") );
	
	c++;
	
	free( pev );
	
	p = dKey.dptr;

	if( fAction )
	    fnAction();
	
	if( fInterrupt ) {
	    free( p );
	    break;
	}
	
	dKey = gdbm_nextkey( pdb, dKey );
	
	free( p );
    }
    
    if( !c )
	outputl( _("The database is empty.") );
    
    gdbm_close( pdb );
}

extern void CommandDatabaseExport( char *sz ) {
    
    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation *pev;
    void *p;
    FILE *pf;
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_READER, 0, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to export to (see `help database "
		 "export').") );
	gdbm_close( pdb );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	outputerr( sz );
	gdbm_close( pdb );
	return;
    }

    dKey = gdbm_firstkey( pdb );

    while( dKey.dptr ) {
	if( !( dValue = gdbm_fetch( pdb, dKey ) ).dptr ) {
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );
	    
	    gdbm_close( pdb );

	    if( pf != stdout )
		fclose( pf );
	    
	    return;
	}
	
	pev = (dbevaluation *) dValue.dptr;

	fprintf( pf, "%14s %d %d %d %d %d %d %ld\n",
		 PositionIDFromKey( (unsigned char *) dKey.dptr ),
		 pev->asEq[ OUTPUT_WIN ],
		 pev->asEq[ OUTPUT_WINGAMMON ],
		 pev->asEq[ OUTPUT_WINBACKGAMMON ],
		 pev->asEq[ OUTPUT_LOSEGAMMON ],
		 pev->asEq[ OUTPUT_LOSEBACKGAMMON ],
		 pev->c, (long) pev->t );
	
	free( pev );

	p = dKey.dptr;
	
	if( fAction )
	    fnAction();
	
	if( fInterrupt ) {
	    free( p );
	    break;
	}
    
	dKey = gdbm_nextkey( pdb, dKey );
	
	free( p );
    }

    if( pf != stdout )
	fclose( pf );
    
    gdbm_close( pdb );
}

extern void CommandDatabaseImport( char *sz ) {

    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation ev, *pev;
    FILE *pf;
    char szID[ 15 ];
    long lTime;
    int i, anBoard[ 2 ][ 25 ];
    unsigned char auchKey[ 10 ];
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_WRCREAT, 0666, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to import from (see `help database "
		 "import').") );
	gdbm_close( pdb );
	return;
    }

    if( !( pf = fopen( sz, "r" ) ) ) {
	outputerr( sz );
	gdbm_close( pdb );
	return;
    }

    while( 1 ) {
	if( fscanf( pf, "%14s %hd %hd %hd %hd %hd %hd %ld\n",
		    szID, ev.asEq + OUTPUT_WIN,
		    ev.asEq + OUTPUT_WINGAMMON,
		    ev.asEq + OUTPUT_WINBACKGAMMON,
		    ev.asEq + OUTPUT_LOSEGAMMON,
		    ev.asEq + OUTPUT_LOSEBACKGAMMON, &ev.c, &lTime ) < 8 )
	    break;

	PositionFromID( anBoard, szID );
	PositionKey( anBoard, auchKey );

	dKey.dsize = 10;
	dKey.dptr = auchKey;

	dValue = gdbm_fetch( pdb, dKey );
	pev = (dbevaluation *) dValue.dptr;

	if( pev && pev->t && pev->t < (time_t) lTime )
	    ev.t = pev->t;
	else
	    ev.t = (time_t) lTime;

	if( pev ) {
	    ev.c += pev->c;
	    if( pev->c + ev.c )
		for( i = OUTPUT_WIN; i <= OUTPUT_LOSEBACKGAMMON; i++ )
		    ev.asEq[ i ] = ( ev.c * ev.asEq[ i ] + pev->c *
				     pev->asEq[ i ] ) / ( ev.c + pev->c );
	}

	dValue.dptr = (char *) &ev;
	dValue.dsize = sizeof ev;
	if( gdbm_store( pdb, dKey, dValue, GDBM_REPLACE ) < 0 ) {
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );

	    fclose( pf );
	    gdbm_close( pdb );

	    return;	    
	}

	if( pev )
	    free( pev );
    }

    if( ferror( pf ) )
	outputerr( sz );
    else if( !feof( pf ) )
	fprintf( stderr, _("%s: malformed position data\n"), sz );
    
    fclose( pf );
    gdbm_close( pdb );
}

extern void CommandDatabaseRollout( char *sz ) {

    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation *pev;
    int i, c = 0, anBoardEval[ 2 ][ 25 ];
    float arOutput[ NUM_ROLLOUT_OUTPUTS ];
    void *p;
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_WRITER, 0, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }
    
    dKey = gdbm_firstkey( pdb );
    
    while( dKey.dptr ) {
	if( !( dValue = gdbm_fetch( pdb, dKey ) ).dptr ) {
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );
	    
	    gdbm_close( pdb );

	    return;
	}
	
	pev = (dbevaluation *) dValue.dptr;
	
	if( pev->c < rcRollout.nTrials /* FIXME */ ) {
	    c++;
	    
	    PositionFromKey( anBoardEval, (unsigned char *) dKey.dptr );
	
	    /* FIXME if position has some existing rollouts, merge them */

	    if( !GeneralEvaluationR( PositionIDFromKey(
		(unsigned char *) dKey.dptr ), arOutput, NULL, NULL, anBoardEval,
				     &ciCubeless, &rcRollout ) ) {
		pev->c = rcRollout.nTrials;
		pev->t = time( NULL );

		for( i = 0; i < NUM_OUTPUTS; i++ )
		    pev->asEq[ i ] = arOutput[ i ] * 0xFFFF;
	    } else {
		for( i = 0; i < NUM_OUTPUTS; i++ )
		    pev->asEq[ i ] = 0;
		
		pev->c = 0;
		pev->t = 0;
	    }
	    
	    if( gdbm_store( pdb, dKey, dValue, GDBM_REPLACE ) < 0 ) {
		fprintf( stderr, "%s: %s\n", szDatabase,
			 gdbm_strerror( gdbm_errno ) );
		
		gdbm_close( pdb );
		
		return;	    
	    }
	}
	
	free( pev );
	
	p = dKey.dptr;
	
	if( fAction )
	    fnAction();
	
	if( fInterrupt ) {
	    free( p );
	    break;
	}
	
	dKey = gdbm_nextkey( pdb, dKey );
	
	free( p );
    }
    
    if( !fInterrupt && !c )
	outputl( _("There are no unevaluated positions in the database to roll "
		 "out.") );
    
    gdbm_close( pdb );
}

extern void CommandDatabaseGenerate( char *sz ) {

  GDBM_FILE pdb;
  datum dKey, dValue;
  dbevaluation ev;
  int i, c = 0, n, anBoardGenerate[ 2 ][ 25 ], anDiceGenerate[ 2 ];
  unsigned char auchKey[ 10 ];

  if( sz && *sz ) {
      if( ( n = ParseNumber( &sz ) ) < 1 ) {
	  outputl( _("If you specify a parameter to `database generate', it\n"
		   "must be a number of positions to create.") );
	  return;
      }
  } else
      n = 0;
      
  if( !( pdb = gdbm_open( szDatabase, 0, GDBM_WRCREAT, 0666, NULL ) ) ) {
    fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
    return;
  }

  ProgressStart( _("Generating database...") );
  
  while( ( !n || c <= n ) && !fInterrupt ) {
    InitBoard( anBoardGenerate );
	
    do {    
	if( !( ++c % 10 ) )
	    Progress();
	    
	RollDice( anDiceGenerate, rngCurrent );

	if( fInterrupt )
	    break;
	    
	FindBestMove( NULL, anDiceGenerate[ 0 ], anDiceGenerate[ 1 ],
		      anBoardGenerate, &ciCubeless, NULL );

	if( fAction )
	    fnAction();
	
	if( fInterrupt )
	    break;
	    
	SwapSides( anBoardGenerate );

	if( rThreshold ) {
	    float ar[ NUM_OUTPUTS ], r0, r1;
	    static evalcontext ec0 = { 0, FALSE, 0, 0, TRUE, FALSE, 0.16, 0.0 },
		ec1 = { 8, FALSE, 1, 0, TRUE, FALSE, 0.16, 0.0 };
		
	    if( EvaluatePosition( anBoardGenerate, ar, &ciCubeless, &ec0 )
		< 0 )
		break;
	    r0 = Utility( ar, &ciCubeless );
	    
	    if( EvaluatePosition( anBoardGenerate, ar, &ciCubeless, &ec1 )
		< 0 )
		break;
	    r1 = Utility( ar, &ciCubeless );
	    
	    if( fabs( r0 - r1 ) < rThreshold )
		/* error too small; keep looking */
		continue;
	}
	
	PositionKey( anBoardGenerate, auchKey );
	
	dKey.dptr = auchKey;
	dKey.dsize = 10;
	
	ev.t = ev.c = 0;
	for( i = 0; i < NUM_OUTPUTS; i++ )
	    ev.asEq[ i ] = 0;
	
	dValue.dptr = (char *) &ev;
	dValue.dsize = sizeof ev;
	
	if( gdbm_store( pdb, dKey, dValue, GDBM_INSERT ) < 0 ) {
	    ProgressEnd();
	    
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );
	    
	    gdbm_close( pdb );
	    
	    return;	    
	}
    } while( ( !n || c <= n ) && !fInterrupt &&
	     ClassifyPosition( anBoardGenerate ) > CLASS_PERFECT );
  }

  ProgressEnd();
  
  gdbm_close( pdb );
}

extern void CommandDatabaseTrain( char *sz ) {

    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation *pev;
    int c = 0, n, i, anBoardTrain[ 2 ][ 25 ];
    float arDesired[ NUM_OUTPUTS ];
    void *p;
    
    if( sz && *sz ) {
	if( ( n = ParseNumber( &sz ) ) < 1 ) {
	    outputl( _("If you specify a parameter to `database train', it\n"
		     "must be a number of positions to train on.") );
	    return;
	}
    } else
	n = 0;
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_READER, 0, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }

    ProgressStart( _("Training from database...") );
    
    while( ( !n || c <= n ) && !fInterrupt ) {
	dKey = gdbm_firstkey( pdb );
	
	while( dKey.dptr ) {
	    if( !( dValue = gdbm_fetch( pdb, dKey ) ).dptr ) {
		ProgressEnd();
		
		fprintf( stderr, "%s: %s\n", szDatabase,
			 gdbm_strerror( gdbm_errno ) );
		
		gdbm_close( pdb );
		
		return;
	    }
	    
	    pev = (dbevaluation *) dValue.dptr;

	    if( pev->c >= 36 /* FIXME */ ) {
		if( !( ++c % 1000 ) )
		    Progress();
		
		for( i = 0; i < NUM_OUTPUTS; i++ )
		    arDesired[ i ] = (float) pev->asEq[ i ] / 0xFFFF;
		
		PositionFromKey( anBoardTrain, (unsigned char *) dKey.dptr );
		
		TrainPosition( anBoardTrain, arDesired, rAlpha, rAnneal );
	    }
	    
	    free( pev );
	    
	    p = dKey.dptr;
	    
	    if( fAction )
		fnAction();
	
	    if( ( n && c > n ) || fInterrupt ) {
		free( p );
		break;
	    }
	    
	    dKey = gdbm_nextkey( pdb, dKey );
	    
	    free( p );
	}
	
	if( !c ) {
	    outputl( _("There are no target evaluations in the database to "
		     "train from.") );
	    break;
	}
    }

    ProgressEnd();
    
    gdbm_close( pdb );
}

extern void CommandDatabaseVerify( char *sz ) {
    
    GDBM_FILE pdb;
    datum dKey, dValue;
    dbevaluation *pev;
    int i, c = 0, anBoardEval[ 2 ][ 25 ];
    float arDesired[ NUM_ROLLOUT_OUTPUTS ], arOutput[ NUM_ROLLOUT_OUTPUTS ];
    double arError[ NUM_ROLLOUT_OUTPUTS ];
    void *p;
    
    if( !( pdb = gdbm_open( szDatabase, 0, GDBM_READER, 0, NULL ) ) ) {
	fprintf( stderr, "%s: %s\n", szDatabase, gdbm_strerror( gdbm_errno ) );
        
	return;
    }

    for( i = 0; i < NUM_OUTPUTS; i++ )
	arError[ i ] = 0.0;
    
    dKey = gdbm_firstkey( pdb );
    
    while( dKey.dptr ) {
	if( !( dValue = gdbm_fetch( pdb, dKey ) ).dptr ) {
	    fprintf( stderr, "%s: %s\n", szDatabase,
		     gdbm_strerror( gdbm_errno ) );
	    
	    gdbm_close( pdb );

	    return;
	}
	
	pev = (dbevaluation *) dValue.dptr;
	
	if( pev->c >= 36 ) {
	    c++;
	    
	    for( i = 0; i < NUM_OUTPUTS; i++ )
		arDesired[ i ] = (float) pev->asEq[ i ] / 0xFFFF;

	    arDesired[ OUTPUT_EQUITY ] = Utility( arDesired, &ciCubeless );
	    
	    PositionFromKey( anBoardEval, (unsigned char *) dKey.dptr );
	    EvaluatePosition( anBoardEval, arOutput, &ciCubeless, NULL );

	    arOutput[ OUTPUT_EQUITY ] = Utility( arOutput, &ciCubeless );
	    
	    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
		arError[ i ] += ( arDesired[ i ] - arOutput[ i ] ) *
		    ( arDesired[ i ] - arOutput[ i ] );
	}
		
	free( pev );
	
	p = dKey.dptr;
	
	if( fAction )
	    fnAction();
	
	if( fInterrupt ) {
	    free( p );
	    break;
	}
	
	dKey = gdbm_nextkey( pdb, dKey );
	
	free( p );
    }
    
    if( c ) {
	for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	    arError[ i ] = sqrt( arError[ i ] / c );

	outputf( _("Error in: p(W) %5.3f p(WG) %5.3f p(WBG) %5.3f "
		 "p(LG) %5.3f p(LBG) %5.3f\n"), arError[ OUTPUT_WIN ],
		 arError[ OUTPUT_WINGAMMON ], arError[ OUTPUT_WINBACKGAMMON ],
		 arError[ OUTPUT_LOSEGAMMON ],
		 arError[ OUTPUT_LOSEBACKGAMMON ] );
	outputf( _("Equity error %5.3f\n"), arError[ OUTPUT_EQUITY ] );
    } else
	outputl( _("There are no target evaluations in the database to "
		 "verify against.") );
    
    gdbm_close( pdb );
}
#else
static void NoGDBM( void ) {

  outputl( _("This installation of GNU Backgammon was compiled without GDBM\n"
           "support, and does not implement position database operations.") );
}

extern void CommandDatabaseDump( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseExport( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseGenerate( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseImport( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseRollout( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseTrain( char *sz ) {
  NoGDBM();
}

extern void CommandDatabaseVerify( char *sz ) {
  NoGDBM();
}
#endif
