/*
 * show.c
 *
 * by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001.
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
#include <string.h>
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
    
    outputf( "        %d-ply evaluation.\n"
             "        %d move search candidate%s.\n"
             "        %0.3g cubeless search tolerance.\n"
             "        %d%% speed.\n"
             "        %s evaluations.\n",
             pec->nPlies, pec->nSearchCandidates, pec->nSearchCandidates == 1 ?
             "" : "s", pec->rSearchTolerance,
             (pec->nReduced) ? 100 / pec->nReduced : 100,
             pec->fCubeful ? "Cubeful" : "Cubeless" );

    if( pec->rNoise )
	outputf( "    Noise standard deviation %5.3f", pec->rNoise );
    else
	output( "    Noiseless evaluations" );

    outputl( pec->fDeterministic ? " (deterministic noise).\n" :
	     " (pseudo-random noise).\n" );
}


extern void
ShowRollout ( rolloutcontext *prc ) {

  static char *aszRNG[] = {
    "ANSI", "BSD", "ISAAC", "manual", "MD5", "Mersenne Twister",
    "user supplied"
  };

  int i;

  outputf( "%d game%s will be played per rollout.\n"
           "Truncation after %d pl%s.\n"
           "Lookahead variance reduction is %sabled.\n"
           "Cube%s rollout.\n"
           "%s dice generator with seed %u.\n",
           prc->nTrials, prc->nTrials == 1 ? "" : "s",
           prc->nTruncate, prc->nTruncate == 1 ? "y" : "ies",
           prc->fVarRedn ? "en" : "dis",
           prc->fCubeful ? "ful" : "less",
           aszRNG[ prc->rngRollout ], prc->nSeed );

  /* FIXME: more compact notation when aecCube = aecChequer etc. */

  outputl ("Chequer play parameters:");

  for ( i = 0; i < 2; i++ ) {
    outputf ( "  Player %d:\n", i );
    ShowEvaluation ( &prc->aecChequer[ i ] );
  }

  outputl ("Cube decision parameters:");

  for ( i = 0; i < 2; i++ ) {
    outputf ( "  Player %d:\n", i );
    ShowEvaluation ( &prc->aecCube[ i ] );
  }

}


extern void
ShowEvalSetup ( evalsetup *pes ) {

  switch ( pes->et ) {

  case EVAL_NONE:
    outputl ( "      No evaluation." );
    break;
  case EVAL_EVAL:
    outputl ( "      Neural net evaluation:" );
    ShowEvaluation ( &pes->ec );
    break;
  case EVAL_ROLLOUT:
    outputl ( "      Rollout:" );
    ShowRollout ( &pes->rc );
    break;
  default:
    assert ( FALSE );

  }

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

extern void CommandShowAnalysis( char *sz ) {

    outputl( fAnalyseCube ? "Cube action will be analysed." :
	     "Cube action will not be analysed." );

    outputl( fAnalyseDice ? "Dice rolls will be analysed." :
	     "Dice rolls will not be analysed." );

    if( fAnalyseMove ) {
	outputl( "Chequer play will be analysed." );
	if( cAnalysisMoves < 0 )
	    outputl( "Every legal move will be analysed." );
	else
	    outputf( "Up to %d moves will be analysed.\n", cAnalysisMoves );
    } else
	outputl( "Chequer play will not be analysed." );

    outputf( "\nAnalysis thresholds:\n"
	     "  +%.3f very good\n"
	     "  +%.3f good\n"
	     "  -%.3f doubtful\n"
	     "  -%.3f bad\n"
	     "  -%.3f very bad\n"
	     "\n"
	     "  +%.3f very lucky\n"
	     "  +%.3f lucky\n"
	     "  -%.3f unlucky\n"
	     "  -%.3f very unlucky\n",
	     arSkillLevel[ SKILL_VERYGOOD ], arSkillLevel[ SKILL_GOOD ],
	     arSkillLevel[ SKILL_DOUBTFUL ], 
	     arSkillLevel[ SKILL_BAD ], arSkillLevel[ SKILL_VERYBAD ],
	     arLuckLevel[ LUCK_VERYGOOD ], arLuckLevel[ LUCK_GOOD ],
	     arLuckLevel[ LUCK_BAD ], arLuckLevel[ LUCK_VERYBAD ] );

    outputl( "\n"
               "Analysis will be performed with the "
             "following evaluation parameters:" );
  outputl( "    Chequer play:" );
    ShowEvalSetup ( &esAnalysisChequer );
    outputl( "    Cube decisions:" );
    ShowEvalSetup ( &esAnalysisCube );

    

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
	if( ms.gs == GAME_NONE )
	    outputl( "No position specified and no game in progress." );
	else
	    ShowBoard();
	
	return;
    }

    /* FIXME handle =n notation */
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

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

    EvalCacheStats( &c, NULL, &cLookup, &cHit );

    outputf( "%d cache entries have been used.  %d lookups, %d hits",
	    c, cLookup, cHit );

    if( cLookup > 0x01000000 ) /* calculate carefully to avoid overflow */
	outputf( " (%d%%).", ( cHit + ( cLookup / 200 ) ) /
		 ( cLookup / 100 ) );
    else if( cLookup )
	outputf( " (%d%%).", ( cHit * 100 + cLookup / 2 ) / cLookup );
    else
	outputc( '.' );

    outputc( '\n' );
}

extern void CommandShowClockwise( char *sz ) {

    if( fClockwise )
	outputl( "Player 1 moves clockwise (and player 0 moves "
		 "anticlockwise)." );
    else
	outputl( "Player 1 moves anticlockwise (and player 0 moves "
		 "clockwise)." );
}

static void ShowCommands( command *pc, char *szPrefix ) {

    char sz[ 64 ], *pch;

    strcpy( sz, szPrefix );
    pch = strchr( sz, 0 );

    for( ; pc->sz; pc++ ) {
	if( !pc->szHelp )
	    continue;

	strcpy( pch, pc->sz );

	if( pc->pc ) {
	    strcat( sz, " " );
	    ShowCommands( pc->pc, sz );
	} else
	    outputl( sz );
    }
}

extern void CommandShowCommands( char *sz ) {

    ShowCommands( acTop, "" );
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

  if( ms.nMatchTo > 0 ) 
    outputl( ms.fCrawford ?
	  "This game is the Crawford game." :
	  "This game is not the Crawford game" );
  else if ( !ms.nMatchTo )
    outputl( "Crawford rule is not used in money sessions." );
  else
    outputl( "No match is being played." );

}

extern void CommandShowCube( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( "There is no game in progress." );
	return;
    }

    if( ms.fCrawford ) {
	outputl( "The cube is disabled during the Crawford game." );
	return;
    }
    
    if( !fCubeUse ) {
	outputl( "The doubling cube is disabled." );
	return;
    }
	
    outputf( "The cube is at %d, ", ms.nCube );

    if( ms.fCubeOwner == -1 )
	outputl( "and is centred." );
    else
	outputf( "and is owned by %s.", ap[ ms.fCubeOwner ].szName );
}

extern void CommandShowDice( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( "The dice will not be rolled until a game is started." );

	return;
    }

    if( ms.anDice[ 0 ] < 1 )
	outputf( "%s has not yet rolled the dice.\n", ap[ ms.fMove ].szName );
    else
	outputf( "%s has rolled %d and %d.\n", ap[ ms.fMove ].szName,
		 ms.anDice[ 0 ], ms.anDice[ 1 ] );
}

extern void CommandShowDisplay( char *sz ) {

    if( fDisplay )
	outputl( "GNU Backgammon will display boards for computer moves." );
    else
	outputl( "GNU Backgammon will not display boards for computer moves." );
}

extern void CommandShowEngine( char *sz ) {

    char szBuffer[ 4096 ];
    
    EvalStatus( szBuffer );

    output( szBuffer );
}

extern void CommandShowEvaluation( char *sz ) {

    outputl( "`eval' and `hint' will use:" );
    outputl( "    Chequer play:" );
    ShowEvalSetup ( &esEvalChequer );
    outputl( "    Cube decisions:" );
    ShowEvalSetup ( &esEvalCube );

}

extern void CommandShowEgyptian( char *sz ) {

    if ( fEgyptian )
      outputl( "Sessions are played with the Egyptian rule." );
    else
      outputl( "Sessions are played without the Egyptian rule." );

}

extern void CommandShowJacoby( char *sz ) {

    if ( fJacoby ) 
      outputl( "Money sessions are played with the Jacoby rule." );
    else
      outputl( "Money sessions are played without the Jacoby rule." );

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

    if( !*sz && ms.gs == GAME_NONE ) {
	outputl( "No position specified and no game in progress." );
	return;
    }
    
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;
    
    PipCount( an, anPips );
    
    outputf( "The pip counts are: %s %d, %s %d.\n", ap[ ms.fMove ].szName,
	    anPips[ 1 ], ap[ !ms.fMove ].szName, anPips[ 0 ] );
}

extern void CommandShowPlayer( char *sz ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	outputf( "Player %d:\n"
		"  Name: %s\n"
		"  Type: ", i, ap[ i ].szName );

	switch( ap[ i ].pt ) {
	case PLAYER_EXTERNAL:
	    outputl( "external\n" );
	    break;
	case PLAYER_GNU:
	    outputf( "gnubg:\n" );
            outputl( "    Checker play:" );
            ShowEvalSetup ( &ap[ i ].esChequer );
            outputl( "    Cube decisions:" );
            ShowEvalSetup ( &ap[ i ].esCube );
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

  if( ms.nMatchTo > 0 ) 
    outputl( ms.fPostCrawford ?
	  "This is post-Crawford play." :
	  "This is not post-Crawford play." );
  else if ( !ms.nMatchTo )
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

  outputl( "`rollout' will use:" );
  ShowRollout ( &rcRollout );

}

extern void CommandShowScore( char *sz ) {

    /* FIXME this display will be wrong if the current game is not the
       last one */
    outputf( "The score (after %d game%s) is: %s %d, %s %d",
	    ms.cGames, ms.cGames == 1 ? "" : "s",
	    ap[ 0 ].szName, ms.anScore[ 0 ],
	    ap[ 1 ].szName, ms.anScore[ 1 ] );

    if ( ms.nMatchTo > 0 ) {
        outputf ( ms.nMatchTo == 1 ? 
	         " (match to %d point%s).\n" :
	         " (match to %d points%s).\n",
                 ms.nMatchTo,
		 ms.fCrawford ? 
		 ", Crawford game" : ( ms.fPostCrawford ?
					 ", post-Crawford play" : ""));
    } 
    else {
        if ( fJacoby )
	    outputl ( " (money session,\nwith Jacoby rule).\n" );
        else
	    outputl ( " (money session,\nwithout Jacoby rule).\n" );
    }

}

extern void CommandShowSeed( char *sz ) {

    PrintRNGSeed();
}

extern void CommandShowTurn( char *sz ) {

    if( ms.gs != GAME_PLAYING ) {
	outputl( "No game is being played." );

	return;
    }
    
    outputf( "%s is on %s.\n", ap[ ms.fMove ].szName,
	    ms.anDice[ 0 ] ? "move" : "roll" );

    if( ms.fResigned )
	outputf( "%s has offered to resign a %s.\n", ap[ ms.fMove ].szName,
		aszGameResult[ ms.fResigned - 1 ] );
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

    if( !*sz && ms.gs == GAME_NONE ) {
        outputl( "No position specified and no game in progress." );
        return;
    }
 
    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;
     
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

    if( !*sz && ms.gs == GAME_NONE ) {
        outputl( "No position specified and no game in progress." );
        return;
    }

    if( ParsePosition( an, &sz, NULL ) < 0 )
	return;

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
	outputl( "Beavers, racoons, and other critters are allowed in"
		 " money sessions." );
    else
	outputl( "Beavers, racoons, and other critters are not allowed in"
		 " money sessions." );
}

extern void CommandShowGammonPrice ( char *sz ) {

  cubeinfo ci;
  int i;

  if( ms.gs != GAME_PLAYING ) {
    outputl( "No game in progress (type `new game' to start one)." );

    return;
  }

  GetMatchStateCubeInfo( &ci, &ms );

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

  if ( ( n <= 0 ) || ( n > MAXSCORE ) ) {
    if ( ms.nMatchTo )
      n = ms.nMatchTo;
    else
      n = MAXSCORE;
  }

#if USE_GTK
  if( fX ) {
      GTKShowMatchEquityTable( n );
      return;
  }
#endif

  output ( "Match equity table: " );
  outputl( szMET[ metCurrent ] );
  outputl( "" );
  
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

extern void CommandShowOutput( char *sz ) {

    outputf( "Match winning chances will be shown as %s.\n", fOutputMatchPC ?
	     "percentages" : "probabilities" );

    if ( fOutputMWC )
	outputl( "Match equities shown in MWC (match winning chance) "
	      "(match play only)." ); 
    else
	outputl( "Match equities shown in EMG (normalized money game equity) "
	      "(match play only)." ); 

    outputf( "Game winning chances will be shown as %s.\n", fOutputWinPC ?
	     "percentages" : "probabilities" );

#if USE_GUI
    if( !fX )
#endif
	outputf( "Boards will be shown in %s.\n", fOutputRawboard ?
		 "raw format" : "ASCII" );
}

extern void CommandShowTraining( char *sz ) {

    outputf( "Learning rate (alpha) %f,\n", rAlpha );

    if( rAnneal )
	outputf( "Annealing rate %f,\n", rAnneal );
    else
	outputl( "Annealing disabled," );

    if( rThreshold )
	outputf( "Error threshold %f.\n", rThreshold );
    else
	outputl( "Error threshold disabled." );
}

extern void CommandShowVersion( char *sz ) {

    char **ppch = aszVersion;
    extern char *aszCredits[];
    int i = 0, cch, cCol = 0;
    
#if USE_GTK
    if( fX ) {
	GTKShowVersion();
	return;
    }
#endif

    while( *ppch )
	outputl( *ppch++ );

    outputc( '\n' );

    outputl( "GNU Backgammon was written by Joseph Heled, Øystein Johansen, "
	     "David Montgomery,\nJørn Thyssen and Gary Wong.\n\n"
	     "Special thanks to:" );

    cCol = 80;

    for( ppch = aszCredits; *ppch; ppch++ ) {
	i += ( cch = strlen( *ppch ) + 2 );
	if( i >= cCol ) {
	    outputc( '\n' );
	    i = cch;
	}
	
	outputf( "%s%c ", *ppch, *( ppch + 1 ) ? ',' : '.' );
    }

    outputc( '\n' );
}

extern void CommandShowMarketWindow ( char * sz ) {

  cubeinfo ci;

  float arOutput[ NUM_OUTPUTS ];
  float rG[ 2 ], rBG[ 2 ];
  float arDP1[ 2 ], arDP2[ 2 ],arCP1[ 2 ], arCP2[ 2 ];
  float rDTW, rDTL, rNDW, rNDL, rDP, rRisk, rGain, r;

  int i, fAutoRedouble[ 2 ], afDead[ 2 ], anNormScore[ 2 ];

  if( ms.gs != GAME_PLAYING ) {
    outputl( "No game in progress (type `new game' to start one)." );

    return;
  }
      
  /* Show market window */

  /* First, get gammon and backgammon percentages */
  GetMatchStateCubeInfo( &ci, &ms );

  /* see if ratios are given on command line */

  rG[ 0 ] = ParseReal ( &sz );

  if ( rG[ 0 ] >= 0 ) {

    /* read the others */

    rG[ 1 ]  = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;
    rBG[ 0 ] = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;
    rBG[ 1 ] = ( (r = ParseReal ( &sz ) ) > 0.0) ? r : 0.0;

    /* If one of the ratios are larger than 1 we assume the user
       has entered 25.1 instead of 0.251 */

    if ( rG[ 0 ] > 1.0 || rG[ 1 ] > 1.0 ||
         rBG[ 1 ] > 1.0 || rBG[ 1 ] > 1.0 ) {
      rG[ 0 ]  /= 100.0;
      rG[ 1 ]  /= 100.0;
      rBG[ 0 ] /= 100.0;
      rBG[ 1 ] /= 100.0;
    }

    /* Check that ratios are 0 <= ratio <= 1 */

    for ( i = 0; i < 2; i++ ) {
      if ( rG[ i ] > 1.0 ) {
        outputf ( "illegal gammon ratio for player %i: %f\n",
                  i, rG[ i ] );
        return ;
      }
      if ( rBG[ i ] > 1.0 ) {
        outputf ( "illegal backgammon ratio for player %i: %f\n",
                  i, rBG[ i ] );
        return ;
      }
    }

    /* Transfer rations to arOutput
       (used in call to GetPoints below)*/ 

    arOutput[ OUTPUT_WIN ] = 0.5;
    arOutput[ OUTPUT_WINGAMMON ] =
      ( rG[ ms.fMove ] + rBG[ ms.fMove ] ) * 0.5;
    arOutput[ OUTPUT_LOSEGAMMON ] =
      ( rG[ !ms.fMove ] + rBG[ !ms.fMove ] ) * 0.5;
    arOutput[ OUTPUT_WINBACKGAMMON ] = rBG[ ms.fMove ] * 0.5;
    arOutput[ OUTPUT_LOSEBACKGAMMON ] = rBG[ !ms.fMove ] * 0.5;

  } else {

    /* calculate them based on current position */

    if( EvaluatePosition( ms.anBoard, arOutput, &ci, &esEvalCube.ec ) < 0 )
      return;

    if ( arOutput[ OUTPUT_WIN ] > 0.0 ) {
      rG[ ms.fMove ] = ( arOutput[ OUTPUT_WINGAMMON ] -
                      arOutput[ OUTPUT_WINBACKGAMMON ] ) /
        arOutput[ OUTPUT_WIN ];
      rBG[ ms.fMove ] = arOutput[ OUTPUT_WINBACKGAMMON ] /
        arOutput[ OUTPUT_WIN ];
    }
    else {
      rG[ ms.fMove ] = 0.0;
      rBG[ ms.fMove ] = 0.0;
    }

    if ( arOutput[ OUTPUT_WIN ] < 1.0 ) {
      rG[ !ms.fMove ] = ( arOutput[ OUTPUT_LOSEGAMMON ] -
                        arOutput[ OUTPUT_LOSEBACKGAMMON ] ) /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
      rBG[ !ms.fMove ] = arOutput[ OUTPUT_LOSEBACKGAMMON ] /
        ( 1.0 - arOutput[ OUTPUT_WIN ] );
    }
    else {
      rG[ !ms.fMove ] = 0.0;
      rBG[ !ms.fMove ] = 0.0;
    }

  }

  for ( i = 0; i < 2; i++ ) 
    outputf ( "Player %-25s: gammon rate %6.2f%%, bg rate %6.2f%%\n",
              ap[ i ].szName, rG[ i ] * 100.0, rBG[ i ] * 100.0);


  if ( ms.nMatchTo ) {

    for ( i = 0; i < 2; i++ )
      anNormScore[ i ] = ms.nMatchTo - ms.anScore[ i ];

    GetPoints ( arOutput, &ci, arCP2 );

    for ( i = 0; i < 2; i++ ) {

      fAutoRedouble [ i ] =
        ( anNormScore[ i ] - 2 * ms.nCube <= 0 ) &&
        ( anNormScore[ ! i ] - 2 * ms.nCube > 0 );

      afDead[ i ] =
        ( anNormScore[ ! i ] - 2 * ms.nCube <=0 );

      /* MWC for "double, take; win" */

      rDTW =
        (1.0 - rG[ i ] - rBG[ i ]) *
        GET_MET ( anNormScore[ i ] - 2 * ms.nCube - 1,
                  anNormScore[ !i ] - 1, aafMET )
        + rG[ i ] * GET_MET ( anNormScore[ i ] - 4 * ms.nCube - 1,
                              anNormScore[ ! i ] - 1, aafMET )
        + rBG[ i ] * GET_MET ( anNormScore[ i ] - 6 * ms.nCube - 1,
                               anNormScore[ ! i ] - 1, aafMET );

      /* MWC for "no double, take; win" */

      rNDW =
        (1.0 - rG[ i ] - rBG[ i ]) *
        GET_MET ( anNormScore[ i ] - ms.nCube - 1,
                  anNormScore[ !i ] - 1, aafMET )
        + rG[ i ] * GET_MET ( anNormScore[ i ] - 2 * ms.nCube - 1,
                              anNormScore[ ! i ] - 1, aafMET )
        + rBG[ i ] * GET_MET ( anNormScore[ i ] - 3 * ms.nCube - 1,
                               anNormScore[ ! i ] - 1, aafMET );

      /* MWC for "Double, take; lose" */

      rDTL =
        (1.0 - rG[ ! i ] - rBG[ ! i ]) *
        GET_MET ( anNormScore[ i ] - 1,
                  anNormScore[ !i ] - 2 * ms.nCube - 1, aafMET )
        + rG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                              anNormScore[ ! i ] - 4 * ms.nCube - 1, aafMET )
        + rBG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                               anNormScore[ ! i ] - 6 * ms.nCube - 1, aafMET );

      /* MWC for "No double; lose" */

      rNDL =
        (1.0 - rG[ ! i ] - rBG[ ! i ]) *
        GET_MET ( anNormScore[ i ] - 1,
                  anNormScore[ !i ] - 1 * ms.nCube - 1, aafMET )
        + rG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                              anNormScore[ ! i ] - 2 * ms.nCube - 1, aafMET )
        + rBG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                               anNormScore[ ! i ] - 3 * ms.nCube - 1, aafMET );

      /* MWC for "Double, pass" */

      rDP = GET_MET( anNormScore[ i ] - ms.nCube - 1,
                     anNormScore[ ! i ] - 1, aafMET );

      /* Double point */

      rRisk = rNDL - rDTL;
      rGain = rDTW - rNDW;

      arDP1 [ i ] = rRisk / ( rRisk + rGain );
      arDP2 [ i ] = arDP1 [ i ];

      /* Dead cube take point without redouble */

      rRisk = rDTW - rDP;
      rGain = rDP - rDTL;

      arCP1 [ i ] = 1.0 - rRisk / ( rRisk + rGain );

      if ( fAutoRedouble[ i ] ) {

        /* With redouble */

        rDTW =
          (1.0 - rG[ i ] - rBG[ i ]) *
          GET_MET ( anNormScore[ i ] - 4 * ms.nCube - 1,
                    anNormScore[ !i ] - 1, aafMET )
          + rG[ i ] * GET_MET ( anNormScore[ i ] - 8 * ms.nCube - 1,
                                anNormScore[ ! i ] - 1, aafMET )
          + rBG[ i ] * GET_MET ( anNormScore[ i ] - 12 * ms.nCube - 1,
                                 anNormScore[ ! i ] - 1, aafMET );

        rDTL =
          (1.0 - rG[ ! i ] - rBG[ ! i ]) *
          GET_MET ( anNormScore[ i ] - 1,
                    anNormScore[ !i ] - 4 * ms.nCube - 1, aafMET )
          + rG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                                  anNormScore[ ! i ] - 8 * ms.nCube - 1,
				  aafMET )
          + rBG[ ! i ] * GET_MET ( anNormScore[ i ] - 1,
                                   anNormScore[ ! i ] - 12 * ms.nCube - 1,
				   aafMET );

        rRisk = rDTW - rDP;
        rGain = rDP - rDTL;

        arCP2 [ i ] = 1.0 - rRisk / ( rRisk + rGain );

        /* Double point */

        rRisk = rNDL - rDTL;
        rGain = rDTW - rNDW;
      
        arDP2 [ i ] = rRisk / ( rRisk + rGain );

      }
    }

    /* output */

    output ( "\n\n" );

    for ( i = 0; i < 2; i++ ) {

      outputf ("Player %s market window:\n\n", ap[ i ].szName );

      if ( fAutoRedouble[ i ] )
        output ("Dead cube (opponent doesn't redouble): ");
      else
        output ("Dead cube: ");

      outputf ("%6.2f%% - %6.2f%%\n", 100. * arDP1[ i ], 100. * arCP1[
                                                                      i ] );

      if ( fAutoRedouble[ i ] ) 
        outputf ("Dead cube (opponent redoubles):"
                 "%6.2f%% - %6.2f%%\n\n",
                 100. * arDP2[ i ], 100. * arCP2[ i ] );
      else if ( ! afDead[ i ] )
        outputf ("Live cube:"
                 "%6.2f%% - %6.2f%%\n\n",
                 100. * arDP2[ i ], 100. * arCP2[ i ] );

    }

  } /* ms.nMatchTo */
  else {

    /* money play: use Janowski's formulae */

    /* 
     * FIXME's: (1) make GTK version
     *          (2) make output for "current" value of X
     *          (3) improve layout?
     */

    float arTPDead[ 2 ], arTPLive[ 2 ]; /* take points */
    float arBPDead[ 2 ], arBPLive[ 2 ]; /* beaver points */
    float arRPDead[ 2 ], arRPLive[ 2 ]; /* raccon points */
    float arDPDead[ 2 ], arDPLive[ 2 ]; /* double points */
    float arCPDead[ 2 ], arCPLive[ 2 ]; /* cash points */
    float arTGDead[ 2 ], arTGLive[ 2 ]; /* too good point */
    float arCLV[ 2 ];/* average cubeless value of games won */
    float rW, rL;
    int i;

    for ( i = 0; i < 2; i++ )
      arCLV[ i ] = 1.0 + rG[ i ] + rBG[ i ];

    GetMatchStateCubeInfo( &ci, &ms );
    
    for ( i = 0; i < 2; i++ ) {

      /* Determine rW and rL from Rick's formulae */

      rW = arCLV[ i ];
      rL = arCLV[ ! i ];

      /* Determine points */

      arTPDead[ i ] = ( rL - 0.5 ) / ( rW + rL );
      arBPDead[ i ] = rL / ( rW + rL );
      arRPDead[ i ] = rL / ( rW + rL );
      
      arTPLive[ i ] = ( rL - 0.5 ) / ( rW + rL + 0.5 );
      arBPLive[ i ] = rL / ( rW + rL + 0.5 );
      arRPLive[ i ] = ( rL + 0.5 ) / ( rW + rL + 0.5 );
      
      if ( ci.fCubeOwner == -1 ) {
        /* initial double point */
        if ( ! ci.fJacoby ) {
          /* without Jacoby */
          arDPDead[ i ] = rL / ( rW + rL );
        }
        else {
          /* with Jacoby */
          
          if ( ci.fBeavers )
            /* with beavers */
            arDPDead[ i ] = ( rL - 0.25 ) / ( rL + rW - 0.5 );
          else
            /* without beavers */
            arDPDead[ i ] = ( rL - 0.5 ) / ( rL + rW - 1.0 );

        }

      }
      else {
        /* redouble point */
        arDPDead[ i ] = rL / ( rW + rL );

      }

      arDPLive[ i ] = ( rL + 1.0 ) / ( rL + rW + 0.5 );

      arCPDead[ i ] = ( rL + 0.5 ) / ( rW + rL );
      arCPLive[ i ] = ( rL + 1.0 ) / ( rW + rL + 0.5 );
      
      arTGDead[ i ] = ( rL + 1.0 ) / ( rW + rL );
      arTGLive[ i ] = ( rL + 1.0 ) / ( rW + rL + 0.5 );
      
      outputf ("\nPlayer %s cube parameters:\n\n", ap[ i ].szName );
      outputl ( "Cube parameter     Dead Cube   Live Cube\n" );

      outputf ( "Take point, TP     %6.2f%%     %6.2f%%\n",
                arTPDead[ i ] * 100.0, arTPLive[ i ] * 100.0 );
      outputf ( "Beaver point, BP   %6.2f%%     %6.2f%%\n",
                arBPDead[ i ] * 100.0, arBPLive[ i ] * 100.0 );
      outputf ( "Raccoon point, RP  %6.2f%%     %6.2f%%\n",
                arRPDead[ i ] * 100.0, arRPLive[ i ] * 100.0 );
      outputf ( "Double point, DP   %6.2f%%     %6.2f%%\n",
                arDPDead[ i ] * 100.0, arDPLive[ i ] * 100.0 );
      outputf ( "Cash point, CP     %6.2f%%     %6.2f%%\n",
                arCPDead[ i ] * 100.0, arCPLive[ i ] * 100.0 );
      outputf ( "Too good point, CP %6.2f%%     %6.2f%%\n",
                arTGDead[ i ] * 100.0, arTGLive[ i ] * 100.0 );

    }

  }

}


