/*
 * show.c
 *
 * by Gary Wong, 1999
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

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "dice.h"
#include "matchequity.h"

#if USE_GTK
#include "gtkboard.h"
#include "gtkgame.h"
#elif USE_EXT
#include "xgame.h"
#endif

extern char *aszCopying[], *aszWarranty[]; /* from copying.c */

static void ShowEvaluation( evalcontext *pec ) {
    
    outputf( "    %d-ply evaluation.\n"
             "    %d move search candidate%s.\n"
             "    %0.3g cubeless search tolerance.\n"
             "    %.0f%% speed.\n"
             "    %s evaluations.\n\n",
             pec->nPlies, pec->nSearchCandidates, pec->nSearchCandidates == 1 ?
             "" : "s", pec->rSearchTolerance,
             (pec->nReduced) ? 100. * pec->nReduced / 21.0 : 100.,
             pec->fCubeful ? "Cubeful" : "Cubeless" );
}

static void ShowPaged( char **ppch ) {

    int i, nRows = 0;
    char *pchLines;
#ifdef TIOCGWINSZ
    struct winsize ws;
#endif

#if HAVE_ISATTY
    if( isatty( STDIN_FILENO ) ) {
#endif
#ifdef TIOCGWINSZ
	if( !( ioctl( STDIN_FILENO, TIOCGWINSZ, &ws ) ) )
	    nRows = ws.ws_row;
#endif
	if( !nRows && ( pchLines = getenv( "LINES" ) ) )
	    nRows = atoi( pchLines );

	/* FIXME we could try termcap-style tgetnum( "li" ) here, but it
	   hardly seems worth it */
	
	if( !nRows )
	    nRows = 24;

	i = 0;

	while( *ppch ) {
	    outputl( *ppch++ );
	    if( ++i >= nRows - 1 ) {
		GetInput( "-- Press <return> to continue --" );
		
		if( fInterrupt )
		    return;
		
		i = 0;
	    }
	}
#if HAVE_ISATTY
    } else
#endif
	while( *ppch )
	    outputl( *ppch++ );
}

extern void CommandShowAutomatic( char *sz ) {

    static char *szOn = "On", *szOff = "Off";
    
    outputf( "bearoff \t(Play certain non-contact bearoff moves):      \t%s\n"
	    "crawford\t(Enable the Crawford rule as appropriate):     \t%s\n"
	    "doubles \t(Turn the cube when opening roll is a double): \t%d\n"
	    "game    \t(Start a new game after each one is completed):\t%s\n"
	    "move    \t(Play the forced move when there is no choice):\t%s\n"
	    "roll    \t(Roll the dice if no double is possible):      \t%s\n",
	    fAutoBearoff ? szOn : szOff,
	    fAutoCrawford ? szOn : szOff,
	    cAutoDoubles,
	    fAutoGame ? szOn : szOff,
	    fAutoMove ? szOn : szOff,
	    fAutoRoll ? szOn : szOff );
}

extern void CommandShowBoard( char *sz ) {

    int an[ 2 ][ 25 ];
    char szOut[ 2048 ];
    char *ap[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    
    if( !*sz ) {
	if( fTurn < 0 )
	    outputl( "No position specified and no game in progress." );
	else
	    ShowBoard();
	
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	outputl( "Illegal position." );

	return;
    }

#if USE_GUI
    if( fX )
#if USE_GTK
	game_set( BOARD( pwBoard ), an, TRUE, "", "", 0, 0, 0, -1, -1 );
#else
        GameSet( &ewnd, an, TRUE, "", "", 0, 0, 0, -1, -1 );    
#endif
    else
#endif
	outputl( DrawBoard( szOut, an, TRUE, ap ) );
}

extern void CommandShowDelay( char *sz ) {
#if USE_GUI
    if( nDelay )
	outputf( "The delay is set to %d ms.\n",nDelay);
    else
	outputl( "No delay is being used." );
#else
    outputl( "The `show delay' command applies only when using a window "
	  "system." );
#endif
}

extern void CommandShowCache( char *sz ) {

    int c, cLookup, cHit;

    EvalCacheStats( &c, &cLookup, &cHit );

    outputf( "%d cache entries have been used.  %d lookups, %d hits",
	    c, cLookup, cHit );

    if( cLookup )
	outputf( " (%d%%).", ( cHit * 100 + cLookup / 2 ) / cLookup );
    else
	outputc( '.' );

    outputc( '\n' );
}

extern void CommandShowConfirm( char *sz ) {

    if( fConfirm )
	outputl( "GNU Backgammon will ask for confirmation before "
	       "aborting games in progress." );
    else
	outputl( "GNU Backgammon will not ask for confirmation "
	       "before aborting games in progress." );
}

extern void CommandShowCopying( char *sz ) {

#if USE_GTK
    if( fX )
	ShowList( aszCopying, "Copying" );
    else
#endif
	ShowPaged( aszCopying );
}

extern void CommandShowCrawford( char *sz ) {

  if( nMatchTo > 0 ) 
    outputl( fCrawford ?
	  "This game is the Crawford game." :
	  "This game is not the Crawford game" );
  else if ( ! nMatchTo )
    outputl( "Crawford rule is not used in money sessions." );
  else
    outputl( "No match is being played." );

}

extern void CommandShowCube( char *sz ) {

    if( fTurn < 0 ) {
	outputl( "There is no game in progress." );
	return;
    }

    if( fCrawford ) {
	outputl( "The cube is disabled during the Crawford game." );
	return;
    }
    
    if( !fCubeUse ) {
	outputl( "The doubling cube is disabled." );
	return;
    }
	
    outputf( "The cube is at %d, ", nCube );

    if( fCubeOwner == -1 )
	outputl( "and is centred." );
    else
	outputf( "and is owned by %s.", ap[ fCubeOwner ].szName );
}

extern void CommandShowDice( char *sz ) {

    if( fTurn < 0 ) {
	outputl( "The dice will not be rolled until a game is started." );

	return;
    }

    if( anDice[ 0 ] < 1 )
	outputf( "%s has not yet rolled the dice.\n", ap[ fMove ].szName );
    else
	outputf( "%s has rolled %d and %d.\n", ap[ fMove ].szName, anDice[ 0 ],
		anDice[ 1 ] );
}

extern void CommandShowDisplay( char *sz ) {

    if( fDisplay )
	outputl( "GNU Backgammon will display boards for computer moves." );
    else
	outputl( "GNU Backgammon will not display boards for computer moves." );
}

extern void CommandShowEvaluation( char *sz ) {

    outputl( "`eval' and `hint' will use:" );
    ShowEvaluation( &ecEval );
}

extern void CommandShowJacoby( char *sz ) {

    if ( fJacoby ) 
      outputl( "Money sessions is played with the Jacoby rule." );
    else
      outputl( "Money sessions is played without the Jacoby rule." );

}

extern void CommandShowNackgammon( char *sz ) {

    if( fNackgammon )
	outputl( "New games will use the Nackgammon starting position." );
    else
	outputl( "New games will use the standard backgammon starting "
	      "position." );
}

extern void CommandShowPipCount( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];

    if( !*sz && fTurn == -1 ) {
	outputl( "No position specified and no game in progress." );
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	outputl( "Illegal position." );

	return;
    }
    
    PipCount( an, anPips );
    
    outputf( "The pip counts are: %s %d, %s %d.\n", ap[ fMove ].szName,
	    anPips[ 1 ], ap[ !fMove ].szName, anPips[ 0 ] );
}

extern void CommandShowPlayer( char *sz ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	outputf( "Player %d:\n"
		"  Name: %s\n"
		"  Type: ", i, ap[ i ].szName );

	switch( ap[ i ].pt ) {
	case PLAYER_GNU:
	    outputf( "gnubg:\n" );
	    ShowEvaluation( &ap[ i ].ec );
	    break;
	case PLAYER_PUBEVAL:
	    outputl( "pubeval\n" );
	    break;
	case PLAYER_HUMAN:
	    outputl( "human\n" );
	    break;
	}
    }
}

extern void CommandShowPostCrawford( char *sz ) {

  if( nMatchTo > 0 ) 
    outputl( fPostCrawford ?
	  "This is post-Crawford play." :
	  "This is not post-Crawford play." );
  else if ( ! nMatchTo )
    outputl( "Crawford rule is not used in money sessions." );
  else
    outputl( "No match is being played." );

}

extern void CommandShowPrompt( char *sz ) {

    outputf( "The prompt is set to `%s'.\n", szPrompt );
}

extern void CommandShowRNG( char *sz ) {

  static char *aszRNG[] = {
    "ANSI", "BSD", "ISAAC", "manual", "MD5", "Mersenne Twister",
    "user supplied"
  };

  outputf( "You are using the %s generator.\n",
	  aszRNG[ rngCurrent ] );
    
}

extern void CommandShowRollout( char *sz ) {

    outputl( "Rollouts will use:" );
    ShowEvaluation( &ecRollout );

    outputf( "%d game%s will be played per rollout, truncating after %d "
	    "pl%s.\nLookahead variance reduction is %sabled.\n",
	    nRollouts, nRollouts == 1 ? "" : "s", nRolloutTruncate,
	    nRolloutTruncate == 1 ? "y" : "ies", fVarRedn ? "en" : "dis" );
}

extern void CommandShowScore( char *sz ) {

    outputf( "The score (after %d game%s) is: %s %d, %s %d",
	    cGames, cGames == 1 ? "" : "s",
	    ap[ 0 ].szName, anScore[ 0 ],
	    ap[ 1 ].szName, anScore[ 1 ] );

    if ( nMatchTo > 0 ) {
        outputf ( nMatchTo == 1 ? 
	         " (match to %d point%s)\n" :
	         " (match to %d points%s)\n",
                 nMatchTo,
		 fCrawford ? 
		 ", Crawford game" : ( fPostCrawford ?
					 ", post-Crawford play" : ""));
    } 
    else {
        if ( fJacoby )
	    outputl ( " (money session, with Jacoby rule)\n" );
        else
	    outputl ( " (money session, without Jacoby rule)\n" );
    }

}

extern void CommandShowSeed( char *sz ) {

    PrintRNGSeed();
}

extern void CommandShowTurn( char *sz ) {

    if( fTurn < 0 ) {
	outputl( "No game is being played." );

	return;
    }
    
    outputf( "%s is on %s.\n", ap[ fMove ].szName,
	    anDice[ 0 ] ? "move" : "roll" );

    if( fResigned )
	outputf( "%s has offered to resign a %s.\n", ap[ fMove ].szName,
		aszGameResult[ fResigned - 1 ] );
}

extern void CommandShowWarranty( char *sz ) {

#if USE_GTK
    if( fX )
	ShowList( aszWarranty, "Warranty" );
    else
#endif
	ShowPaged( aszWarranty );
}

extern void CommandShowKleinman( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];
    float fKC;

    if( !*sz && fTurn == -1 ) {
        outputl( "No position specified and no game in progress." );
        return;
    }
 
    if( ParsePosition( an, sz ) ) {
        outputl( "Illegal position." );

        return;
    }
     
    PipCount( an, anPips );
 
    fKC = KleinmanCount (anPips[1], anPips[0]);
    if (fKC == -1.0)
        outputf ("Pipcount unsuitable for Kleinman Count.\n");
    else
        outputf ("Cubeless Winning Chance: %.4f\n", fKC);
 }

extern void CommandShowThorp( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];
    int nLeader, nTrailer, nDiff, anCovered[2], anMenLeft[2];
    int x;

    if( !*sz && fTurn == -1 ) {
        outputl( "No position specified and no game in progress." );
        return;
    }

    if( ParsePosition( an, sz ) ) {
        outputl( "Illegal position." );

        return;
    }

    PipCount( an, anPips );

  anMenLeft[0] = 0;
  anMenLeft[1] = 0;
  for (x = 0; x < 25; x++)
    {
      anMenLeft[0] += an[0][x];
      anMenLeft[1] += an[1][x];
    }

  anCovered[0] = 0;
  anCovered[1] = 0;
  for (x = 0; x < 6; x++)
    {
      if (an[0][x])
        anCovered[0]++;
      if (an[1][x])
        anCovered[1]++;
    }

        nLeader = anPips[1];
        nLeader += 2*anMenLeft[1];
        nLeader += an[1][0];
        nLeader -= anCovered[1];

        if (nLeader > 30) {
         if ((nLeader % 10) > 5)
        {
           nLeader *= 1.1;
           nLeader += 1;
        }
         else
          nLeader *= 1.1;
        }
        nTrailer = anPips[0];
        nTrailer += 2*anMenLeft[0];
        nTrailer += an[0][0];
        nTrailer -= anCovered[0];

        outputf("L = %d  T = %d  -> ", nLeader, nTrailer);
        if (nTrailer >= (nLeader - 1))
          output("re");
        if (nTrailer >= (nLeader - 2))
          output("double/");
        if (nTrailer < (nLeader - 2))
          output("no double/");
        if (nTrailer >= (nLeader + 2))
          outputl("drop");
        else
          outputl("take");

	if( ( nDiff = nTrailer - nLeader ) > 13 )
	    nDiff = 13;
	else if( nDiff < -37 )
	    nDiff = -37;
	
        outputf("Bower's interpolation: %d%% cubeless winning "
                "chance\n", 74 + 2 * nDiff );
}


extern void CommandShowBeavers( char *sz ) {

  if ( fBeavers )
    puts( "Beavers, racoons, and other critters are allowed in"
          " money sessions.." );
  else
    puts( "Beavers, racoons, and other critters are not allowed in"
          " money sessions.." );

}


extern void CommandShowGammonPrice ( char *sz ) {

  cubeinfo ci;
  int i;

  SetCubeInfo ( &ci, nCube, fCubeOwner, fMove );

  output ( "Player        Gammon price    Backgammon price\n" );

  for ( i = 0; i < 2; i++ ) {

    outputf ("%-12s     %7.5f         %7.5f\n",
	     ap[ i ].szName,
	     ci.arGammonPrice[ i ], ci.arGammonPrice[ 2 + i ] );
  }

}


extern void CommandShowMatchEquityTable ( char *sz ) {

  /* Read a number n. */

  int n = ParseNumber ( &sz );
  int i, j;

  /* If n > 0 write n x n match equity table,
     else if match write nMatchTo x nMatchTo table,
     else write full table (may be HUGE!) */

  if ( ( n <= 0 ) || ( n > nMaxScore ) ) {
    if ( nMatchTo )
      n = nMatchTo;
    else
      n = nMaxScore;
  }

  /* FIXME: for GTK write out to table */

  output ( "Match equity table: " );

  switch ( metCurrent ) {
  case MET_ZADEH:
    output ( "N. Zadeh, Management Science 23, 986 (1977)\n\n" );
    break;
  case MET_SNOWIE:
    output ( "Snowie 2.1, Oasya, 1999\n\n" );
    break;
  case MET_WOOLSEY:
    output ( "K. Woolsey, How to Play Tournament Backgammon "
             "(1993)\n\n" );
    break;
  case MET_JACOBS:
    output ( "J. Jacobs & W. Trice, Can a Fish Taste Twice as Good. "
             "(1996)\n\n" );
    break;
  default:
    assert ( FALSE );
  }

  /* Write column headers */

  output ( "          " );
  for ( j = 0; j < n; j++ )
    outputf ( " %3i-away ", j + 1 );
  output ( "\n" );

  for ( i = 0; i < n; i++ ) {
    
    outputf ( " %3i-away ", i + 1 );
    
    for ( j = 0; j < n; j++ )
      outputf ( " %8.4f ", GET_MET ( i, j, aafMET ) * 100.0 );
    output ( "\n" );
  }
  output ( "\n" );

}


extern void CommandShowOutputMWC( char *sz ) {

  if ( fOutputMWC )
    puts( "Output shown in MWC (match winning chance) "
	  "(match play only)." ); 
  else
    puts( "Output shown in EMG (normalized money game equity) "
	  "(match play only)." ); 

}


