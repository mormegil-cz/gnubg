/*
 * set.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999-2000.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"

static command acSetEvaluation[] = {
    { "candidates", CommandSetEvalCandidates, "Limit the number of moves "
      "for deep evaluation", NULL },
    { "plies", CommandSetEvalPlies, "Choose how many plies the `eval' and "
      "`hint' commands look ahead", NULL },
    { "tolerance", CommandSetEvalTolerance, "Control the equity range "
      "of moves for deep evaluation", NULL },
    { NULL, NULL, NULL, NULL }
}, acSetPlayer[] = {
    { "evaluation", CommandSetPlayerEvaluation, "Control evaluation "
      "parameters when gnubg plays", NULL },
    { "gnubg", CommandSetPlayerGNU, "Have gnubg make all moves for a player",
      NULL },
    { "human", CommandSetPlayerHuman, "Have a human make all moves for a "
      "player", NULL },
    { "name", CommandSetPlayerName, "Change a player's name", NULL },
    { "pubeval", CommandSetPlayerPubeval, "Have pubeval make all moves for a "
      "player", NULL },
    { NULL, NULL, NULL, NULL }
};

static void SetRNG( rng rngNew, char *szSeed ) {

    static char *aszRNG[] = {
	"ANSI", "BSD", "ISAAC", "manual", "Mersenne Twister",
	"user supplied"
    };
    
    /* FIXME: read name of file with user RNG */

    if( rngCurrent == rngNew ) {
	printf( "You are already using the %s generator.\n",
		aszRNG[ rngNew ] );
	if( szSeed )
	    CommandSetSeed( szSeed );
    } else {
	printf( "GNU Backgammon will now use the %s generator.\n",
		aszRNG[ rngNew ] );

        /* Dispose dynamically linked user module if necesary */

        if ( rngCurrent == RNG_USER )
	  UserRNGClose();

	/* Load dynamic library with user RNG */

        if ( rngNew == RNG_USER ) {
	  
	  if ( !UserRNGOpen() ) {

	    printf ( "Error loading shared library.\n" );
	    printf ( "You are still using the %s generator.\n",
		     aszRNG[ rngCurrent ] );
            return;
	  }

	}
	    
	if( ( rngCurrent = rngNew ) != RNG_MANUAL )
	    CommandSetSeed( szSeed );
    }
}

extern void CommandSetAutoBearoff( char *sz ) {

    SetToggle( "automatic bearoff", &fAutoBearoff, sz, "Will automatically "
	       "bear off as many chequers as possible.", "Will not "
	       "automatically bear off chequers." );
}

extern void CommandSetAutoCrawford( char *sz ) {

    SetToggle( "automatic crawford", &fAutoCrawford, sz, "Will enable the "
	       "Crawford game according to match score.", "Will not "
	       "enable the Crawford game according to match score." );
}

extern void CommandSetAutoDoubles( char *sz ) {

    int n;
    
    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	puts( "You must specify how many automatic doubles to use "
	      "(try `help set automatic double')." );
	return;
    }

    if( n > 16 ) {
	puts( "Please specify a smaller limit (up to 16 automatic "
	      "doubles." );
	return;
    }
	
    if( ( cAutoDoubles = n ) > 1 )
	printf( "Automatic doubles will be used (up to a limit of %d).\n", n );
    else
	puts( "A single automatic double will be permitted." );

    if( cAutoDoubles ) {
	if( nMatchTo > 0 )
	    puts( "(Note that automatic doubles will have no effect until you "
		  "start session play.)" );
	else if( !fCubeUse )
	    puts( "(Note that automatic doubles will have no effect until you "
		  "enable cube use --\nsee `help set cube use'.)" );
    }
}

extern void CommandSetAutoGame( char *sz ) {

    SetToggle( "automatic game", &fAutoGame, sz, "Will automatically start "
	       "games after wins.", "Will not automatically start games." );
}

extern void CommandSetAutoMove( char *sz ) {

    SetToggle( "automatic move", &fAutoMove, sz, "Forced moves will be made "
	       "automatically.", "Forced moves will not be made "
	       "automatically." );
}

extern void CommandSetAutoRoll( char *sz ) {

    SetToggle( "automatic roll", &fAutoRoll, sz, "Will automatically roll the "
	       "dice when no cube action is possible.", "Will not "
	       "automatically roll the dice." );
}

extern void CommandSetBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    
    if( fTurn < 0 ) {
	puts( "There must be a game in progress to set the board." );

	return;
    }

    if( !*sz ) {
	puts( "You must specify a position -- see `help set board'." );

	return;
    }

    if( ParsePosition( an, sz ) ) {
	puts( "Illegal position." );
	return;
    }

    memcpy( anBoard, an, sizeof( an ) );

    ShowBoard();
}

static int CheckCubeAllowed( void ) {
    
    if( fTurn < 0 ) {
	puts( "There must be a game in progress to set the cube." );
	return -1;
    }

    if( fCrawford ) {
	puts( "The cube is disabled during the Crawford game." );
	return -1;
    }
    
    if( !fCubeUse ) {
	puts( "The cube is disabled (see `help set cube use')." );
	return -1;
    }

    return 0;
}

extern void CommandSetCache( char *sz ) {

    int n;

    if( ( n = ParseNumber( &sz ) ) < 0 ) {
	puts( "You must specify the number of cache entries to use." );

	return;
    }

    if( EvalCacheResize( n ) )
	perror( "EvalCacheResize" );
    else
	printf( "The position cache has been sized to %d entr%s.\n", n,
		n == 1 ? "y" : "ies" );
}

extern void CommandSetCubeCentre( char *sz ) {

    if( CheckCubeAllowed() )
	return;
    
    fCubeOwner = -1;

    puts( "The cube has been centred (either player may double)." );
    
#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif

    if( fDoubled ) {
	printf( "(%s's double has been cancelled.)\n", ap[ fMove ].szName );
	fDoubled = FALSE;
	fNextTurn = TRUE;
    }
}

extern void CommandSetCubeOwner( char *sz ) {

    int i;
    
    if( CheckCubeAllowed() )
	return;

    switch( i = ParsePlayer( sz ) ) {
    case 0:
    case 1:
	fCubeOwner = i;
	break;
	
    case 2:
	/* "set cube owner both" is the same as "set cube centre" */
	CommandSetCubeCentre( NULL );
	return;

    default:
	puts( "You must specify which player owns the cube (see `help set "
	      "cube owner')." );
	return;
    }

    printf( "%s now owns the cube.\n", ap[ fCubeOwner ].szName );

#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif    
    
    if( fDoubled ) {
	printf( "(%s's double has been cancelled.)\n", ap[ fMove ].szName );
	fDoubled = FALSE;
	fNextTurn = TRUE;
    }
}

extern void CommandSetCubeUse( char *sz ) {

    if( SetToggle( "cube use", &fCubeUse, sz, "Use of the doubling cube is "
		   "permitted.",
		   "Use of the doubling cube is disabled." ) < 0 )
	return;

    if( !nMatchTo && fJacoby && !fCubeUse )
	puts( "(Note that you'll have to disable the Jacoby rule if you want "
	      "gammons and\nbackgammons to be scored -- see `help set "
	      "jacoby')." );
    
    if( fCrawford && fCubeUse )
	puts( "(But the Crawford rule is in effect, so you won't be able to "
	      "use it during\nthis game.)" );
    else if( fTurn >= 0 && !fCubeUse ) {
	/* The cube was being used and now it isn't; reset it to 1,
	   centred. */
	nCube = 1;
	fCubeOwner = -1;
	
#if !X_DISPLAY_MISSING
	if( fX )
	    ShowBoard();
#endif

	if( fDoubled ) {
	    printf( "(%s's double has been cancelled.)\n",
		    ap[ fMove ].szName );
	    fDoubled = FALSE;
	    fNextTurn = TRUE;
	}
    }
}

extern void CommandSetCubeValue( char *sz ) {

    int i, n;

    if( CheckCubeAllowed() )
	return;
    
    n = ParseNumber( &sz );

    for( i = fDoubled ? MAX_CUBE >> 1 : MAX_CUBE; i; i >>= 1 )
	if( n == i ) {
	    printf( "The cube has been set to %d.\n", nCube = n );
	    
#if !X_DISPLAY_MISSING
	    if( fX )
		ShowBoard();
#endif
	    return;
	}

    puts( "You must specify a legal cube value (see `help set cube value')." );
}

extern void CommandSetDelay( char *sz ) {
#if !X_DISPLAY_MISSING
    int n;

    if( fX ) {
	if( *sz && !strncasecmp( sz, "none", strlen( sz ) ) )
	    n = 0;
	else if( ( n = ParseNumber( &sz ) ) < 0 || n > 10000 ) {
	    puts( "You must specify a legal move delay (see `help set "
		  "delay')." );
	    return;
	}

	if( n )
	    printf( "All moves will be shown for at least %d millisecond%s.\n",
		    n, n > 1 ? "s" : "" );
	else
	    puts( "Moves will not be delayed." );
	
	nDelay = n;
    } else
#endif
	puts( "The `set delay' command applies only when using the X Window "
	      "System." );
}

extern void CommandSetDice( char *sz ) {

    int n0, n1;
    
    if( fTurn < 0 ) {
	puts( "There must be a game in progress to set the dice." );

	return;
    }

    if( ( n0 = ParseNumber( &sz ) ) < 1 || n0 > 6 ||
	( n1 = ParseNumber( &sz ) ) < 1 || n1 > 6 ) {
	puts( "You must specify two numbers from 1 to 6 for the dice." );

	return;
    }

    printf( "The dice have been set to %d and %d.\n", anDice[ 0 ] = n0,
	    anDice[ 1 ] = n1 );

#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif
}

extern void CommandSetDisplay( char *sz ) {

    SetToggle( "display", &fDisplay, sz, "Will display boards for computer "
	       "moves.", "Will not display boards for computer moves." );
}

static evalcontext *pecSet;
static char *szSet, *szSetCommand;

extern void CommandSetEvalCandidates( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 || n > MAX_SEARCH_CANDIDATES ) {
	printf( "You must specify a valid number of moves to consider -- try "
	      "`help set %sevaluation candidates'.\n", szSetCommand );

	return;
    }

    pecSet->nSearchCandidates = n;

    printf( "%s will consider up to %d move%s for evaluation at deeper "
	    "plies.\n", szSet, pecSet->nSearchCandidates,
	    pecSet->nSearchCandidates == 1 ? "" : "s" );
}

extern void CommandSetEvalPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	printf( "You must specify a valid number of plies to look ahead -- "
		"try `help set %sevaluation plies'.\n", szSetCommand );

	return;
    }

    pecSet->nPlies = n;

    printf( "%s will use %d ply evaluation.\n", szSet, pecSet->nPlies );
}

extern void CommandSetEvalTolerance( char *sz ) {

    double r = ParseReal( &sz );

    if( r < 0.0 ) {
	printf( "You must specify a valid cubeless equity tolerance to use -- "
		"try `help set\n%sevaluation tolerance'.", szSetCommand );

	return;
    }

    pecSet->rSearchTolerance = r;

    printf( "%s will select moves within %0.3g cubeless equity for\n"
	    "evaluation at deeper plies.\n", szSet, pecSet->rSearchTolerance );
}

extern void CommandSetEvaluation( char *sz ) {

    szSet = "`eval' and `hint'";
    szSetCommand = "";
    pecSet = &ecEval;
    HandleCommand( sz, acSetEvaluation );
}

extern void CommandSetNackgammon( char *sz ) {
    
    SetToggle( "nackgammon", &fNackgammon, sz, "New games will use the "
	       "Nackgammon starting position.", "New games will use the "
	       "standards backgammon starting position." );

#if !X_DISPLAY_MISSING
    if( fX && fTurn == -1 )
	ShowBoard();
#endif
}

static int iPlayerSet;

extern void CommandSetPlayerEvaluation( char *sz ) {

    szSet = ap[ iPlayerSet ].szName;
    szSetCommand = "player ";
    pecSet = &ap[ iPlayerSet ].ec;

    HandleCommand( sz, acSetEvaluation );

    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	printf( "(Note that this setting will have no effect until you "
		"`set player %s gnu'.)\n", ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerGNU( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_GNU;

    printf( "Moves for %s will now be played by GNU Backgammon.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerHuman( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_HUMAN;

    printf( "Moves for %s must now be entered manually.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayerName( char *sz ) {

    char *pch = NextToken( &sz );
    
    if( !pch ) {
	puts( "You must specify a name to use." );

	return;
    }

    if( ( *pch == '0' || *pch == '1' ) && !pch[ 1 ] ) {
	printf( "`%c' is not a legal name.\n", *pch );

	return;
    }

    if( !strncasecmp( pch, "both", strlen( pch ) ) ) {
	puts( "`both' is a reserved word; you can't call a player that.\n" );

	return;
    }

    if( !strncasecmp( pch, ap[ !iPlayerSet ].szName,
		      sizeof( ap[ 0 ].szName ) ) ) {
	puts( "That name is already in use by the other player." );

	return;
    }

    if( strlen( pch ) > 31 )
	pch[ 31 ] = 0;
    
    strcpy( ap[ iPlayerSet ].szName, pch );

    printf( "Player %d is now knwon as `%s'.\n", iPlayerSet, pch );
}

extern void CommandSetPlayerPlies( char *sz ) {

    int n = ParseNumber( &sz );

    if( n < 0 ) {
	puts( "You must specify a vaid ply depth to look ahead -- try `help "
	      "set player plies'." );

	return;
    }

    ap[ iPlayerSet ].ec.nPlies = n;
    
    if( ap[ iPlayerSet ].pt != PLAYER_GNU )
	printf( "Moves for %s will be played with %d ply lookahead (note that "
		"this will\nhave no effect yet, because gnubg is not moving "
		"for this player).\n", ap[ iPlayerSet ].szName, n );
    else
	printf( "Moves for %s will be played with %d ply lookahead.\n",
		ap[ iPlayerSet ].szName, n );
}

extern void CommandSetPlayerPubeval( char *sz ) {

    ap[ iPlayerSet ].pt = PLAYER_PUBEVAL;

    printf( "Moves for %s will now be played by pubeval.\n",
	    ap[ iPlayerSet ].szName );
}

extern void CommandSetPlayer( char *sz ) {

    char *pch = NextToken( &sz ), *pchCopy;
    int i;

    if( !pch ) {
	puts( "You must specify a player -- try `help set player'." );
	
	return;
    }

    if( !( i = ParsePlayer( pch ) ) || i == 1 ) {
	iPlayerSet = i;

	HandleCommand( sz, acSetPlayer );
	
	return;
    }

    if( i == 2 ) {
	if( !( pchCopy = malloc( strlen( sz ) + 1 ) ) ) {
	    puts( "Insufficient memory." );
		
	    return;
	}

	strcpy( pchCopy, sz );

	iPlayerSet = 0;
	HandleCommand( sz, acSetPlayer );

	iPlayerSet = 1;
	HandleCommand( pchCopy, acSetPlayer );

	free( pchCopy );
	
	return;
    }
    
    printf( "Unknown player `%s' -- try `help set player'.\n", pch );
}

extern void CommandSetPrompt( char *szParam ) {

    static char sz[ 128 ]; /* FIXME check overflow */

    szPrompt = ( szParam && *szParam ) ? strcpy( sz, szParam ) :
	szDefaultPrompt;
    
    printf( "The prompt has been set to `%s'.\n", szPrompt );
}

extern void CommandSetRNGAnsi( char *sz ) {

    SetRNG( RNG_ANSI, sz );
}

extern void CommandSetRNGBsd( char *sz ) {
#if HAVE_RANDOM
    SetRNG( RNG_BSD, sz );
#else
    puts( "This installation of GNU Backgammon was compiled without the" );
    puts( "BSD generator." );
#endif
}

extern void CommandSetRNGIsaac( char *sz ) {

    SetRNG( RNG_ISAAC, sz );
}

extern void CommandSetRNGManual( char *sz ) {

    SetRNG ( RNG_MANUAL, sz );
}

extern void CommandSetRNGMersenne( char *sz ) {

    SetRNG( RNG_MERSENNE, sz );
}

extern void CommandSetRNGUser( char *sz ) {

#if HAVE_LIBDL
    SetRNG( RNG_USER, sz );
#else
    puts( "This installation of GNU Backgammon was compiled without the" );
    puts( "dynamic linking library needed for user RNG's." );
#endif

}

extern void CommandSetRolloutEvaluation( char *sz ) {

    szSet = "Rollouts";
    szSetCommand = "rollout ";
    pecSet = &ecRollout;
    HandleCommand( sz, acSetEvaluation );
}

extern void CommandSetRolloutTrials( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	puts( "You must specify a valid number of trials to make -- "
		"try `help set rollout trials'." );

	return;
    }

    nRollouts = n;

    printf( "%d game%s will be played per rollout.\n", nRollouts,
	    nRollouts == 1 ? "" : "s" );
}

extern void CommandSetRolloutTruncation( char *sz ) {
    
    int n = ParseNumber( &sz );

    if( n < 1 ) {
	puts( "You must specify a valid ply at which to truncate  -- "
		"try `help set rollout truncation'." );

	return;
    }

    nRolloutTruncate = n;

    printf( "Rollouts will be truncated after %d pl%s.\n", nRolloutTruncate,
	    nRolloutTruncate == 1 ? "y" : "ies" );
}

extern void CommandSetRolloutVarRedn( char *sz ) {
    
    SetToggle( "rollout varredn", &fVarRedn, sz, "Will lookahead during "
	       "rollouts to reduce variance.", "Will not use lookahead "
	       "variance reduction during rollouts." );
}
    
extern void CommandSetScore( char *sz ) {

    int n0, n1;

    /* FIXME allow setting the score in `n-away' notation, e.g.
     "set score -3 -4".  Also allow specifying Crawford here, e.g.
    "set score crawford -3". */
    
    if( ( n0 = ParseNumber( &sz ) ) < 0 ||
	( n1 = ParseNumber( &sz ) ) < 0 ) {
	puts( "You must specify the score for both players -- try `help set "
	      "score'." );
	return;
    }

    if( nMatchTo && ( n0 >= nMatchTo || n1 >= nMatchTo ) ) {
	printf( "Each player's score must be below %d point%s.\n", nMatchTo,
		nMatchTo == 1 ? "" : "s" );
	return;
    }

    anScore[ 0 ] = n0;
    anScore[ 1 ] = n1;

    fCrawford = ( n0 == nMatchTo - 1 ) || ( n1 == nMatchTo - 1 );
    fPostCrawford = FALSE;

    CommandShowScore( NULL );
    
}

extern void CommandSetSeed( char *sz ) {

    int n;
    
    if( rngCurrent == RNG_MANUAL ) {
	puts( "You can't set a seed if you're using manual dice generation." );
	return;
    }

    if( *sz ) {
	n = ParseNumber( &sz );

	if( n < 0 ) {
	    puts( "You must specify a vaid seed -- try `help set seed'." );

	    return;
	}

	InitRNGSeed( n );
	printf( "Seed set to %d.\n", n );
    } else
	puts( InitRNG() ? "Seed initialised from system random data." :
	      "Seed initialised by system clock." );
}

extern void CommandSetTurn( char *sz ) {

    char *pch = NextToken( &sz );
    int i;

    if( fResigned ) {
	puts( "Please resolve the resignation first." );

	return;
    }
    
    if( !pch ) {
	puts( "Which player do you want to set on roll?" );

	return;
    }

    if( ( i = ParsePlayer( pch ) ) < 0 ) {
	printf( "Unknown player `%s' -- try `help set turn'.\n", pch );

	return;
    }

    if( i == 2 ) {
	puts( "You can't set both players on roll." );

	return;
    }

    if( fTurn != i )
	SwapSides( anBoard );
    
    fTurn = fMove = i;
    fDoubled = 0;

    anDice[ 0 ] = anDice[ 1 ] = 0;

#if !X_DISPLAY_MISSING
    if( fX )
	ShowBoard();
#endif
    
    printf( "`%s' is now on roll.\n", ap[ i ].szName );
}


extern void CommandSetJacoby( char *sz ) {

  SetToggle( "jacoby", &fJacoby, sz, 
	     "Will use the Jacoby rule for money sessions.",
             "Will not use the Jacoby rule for money sessions." );

  if( fJacoby && !fCubeUse )
    puts( "(Note that you'll have to enable the cube if you want gammons "
          "and backgammons\nto be scored -- see `help set cube use'.)" );

}


extern void CommandSetCrawford( char *sz ) {

  if ( nMatchTo > 0 ) {
    if ( ( nMatchTo - anScore[ 0 ] == 1 ) || 
	 ( nMatchTo - anScore[ 1 ] == 1 ) ) {

      if( SetToggle( "crawford", &fCrawford, sz, 
		 "This game is the Crawford game (no doubling allowed).",
		 "This game is not the Crawford game." ) < 0 )
	  return;

      /* sanity check */

      if ( fCrawford && fPostCrawford )
	CommandSetPostCrawford ( "off" );

      if ( !fCrawford && !fPostCrawford )
	CommandSetPostCrawford ( "on" );

      if( fCrawford && fDoubled ) {
	  printf( "(%s's double has been cancelled.)\n", ap[ fMove ].szName );
	  fDoubled = FALSE;
	  fNextTurn = TRUE;
      }
    }
    else {
      puts( "Cannot set whether this is the Crawford game\n"
	    "as none of the players are 1-away from winning." );
    }
  }
  else if ( ! nMatchTo ) 
    puts ( "Cannot set Crawford play for money sessions." );
  else
    puts ( "No match in progress (type `new match n' to start one)." );
}

extern void CommandSetPostCrawford( char *sz ) {

  if ( nMatchTo > 0 ) {
    if ( ( nMatchTo - anScore[ 0 ] == 1 ) || 
	 ( nMatchTo - anScore[ 1 ] == 1 ) ) {

      SetToggle( "postcrawford", &fPostCrawford, sz, 
		 "This is post-Crawford play (doubling allowed).",
		 "This is not post-Crawford play." );

      /* sanity check */

      if ( fPostCrawford && fCrawford )
	CommandSetCrawford ( "off" );

      if ( !fPostCrawford && !fCrawford )
	CommandSetCrawford ( "on" );

    }
    else {
      puts( "Cannot set whether this is post-Crawford play\n"
	    "as none of the players are 1-away from winning." );
    }
  }
  else if ( ! nMatchTo ) 
    puts ( "Cannot set post-Crawford play for money sessions." );
  else
    puts ( "No match in progress (type `new match n' to start one)." );

}

