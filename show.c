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

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "dice.h"

#if USE_GTK
#include "gtkboard.h"
#elif USE_EXT
#include "xgame.h"
#endif

extern char *aszCopying[], *aszWarranty[]; /* from copying.c */

static void ShowEvaluation( evalcontext *pec ) {
    
    printf( "    %d-ply evaluation.\n"
	    "    %d move search candidate%s.\n"
	    "    %0.3g cubeless search tolerance.\n"
	    "    %.0f%% speed.\n"
	    "    %s.\n\n",
	    pec->nPlies, pec->nSearchCandidates, pec->nSearchCandidates == 1 ?
	    "" : "s", pec->rSearchTolerance,
	    (pec->nReduced) ? 100. * pec->nReduced / 21.0 : 100.,
	    pec->fRelativeAccuracy ? "Consistent evaluations" :
	    "Variable evaluations" );
}

static void ShowPaged( char **ppch ) {

    int i, nRows = 0;
    char *pchLines;
#if TIOCGWINSZ
    struct winsize ws;
#endif

#if HAVE_ISATTY
    if( isatty( STDIN_FILENO ) ) {
#endif
#if TIOCGWINSZ
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
	    puts( *ppch++ );
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
	    puts( *ppch++ );
}

extern void CommandShowAutomatic( char *sz ) {

    static char *szOn = "On", *szOff = "Off";
    
    printf( "bearoff \t(Play certain non-contact bearoff moves):      \t%s\n"
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
	    puts( "No position specified and no game in progress." );
	else
	    ShowBoard();
	
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	puts( "Illegal position." );

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
	puts( DrawBoard( szOut, an, TRUE, ap ) );
}

extern void CommandShowDelay( char *sz ) {
#if USE_GUI
    if( nDelay )
	printf( "The delay is set to %d ms.\n",nDelay);
    else
	puts( "No delay is being used." );
#else
    puts( "The `show delay' command applies only when using a window "
	  "system." );
#endif
}

extern void CommandShowCache( char *sz ) {

    int c, cLookup, cHit;

    EvalCacheStats( &c, &cLookup, &cHit );

    printf( "%d cache entries have been used.  %d lookups, %d hits",
	    c, cLookup, cHit );

    if( cLookup )
	printf( " (%d%%).", ( cHit * 100 + cLookup / 2 ) / cLookup );
    else
	putchar( '.' );

    putchar( '\n' );
}

extern void CommandShowConfirm( char *sz ) {

    if( fConfirm )
	puts( "GNU Backgammon will ask for confirmation before "
	       "aborting games in progress." );
    else
	puts( "GNU Backgammon will not ask for confirmation "
	       "before aborting games in progress." );
}

extern void CommandShowCopying( char *sz ) {

    ShowPaged( aszCopying );
}

extern void CommandShowCrawford( char *sz ) {

  if( nMatchTo > 0 ) 
    puts( fCrawford ?
	  "This game is the Crawford game." :
	  "This game is not the Crawford game" );
  else if ( ! nMatchTo )
    puts( "Crawford rule is not used in money sessions." );
  else
    puts( "No match is being played." );

}

extern void CommandShowCube( char *sz ) {

    if( fTurn < 0 ) {
	puts( "There is no game in progress." );
	return;
    }

    if( fCrawford ) {
	puts( "The cube is disabled during the Crawford game." );
	return;
    }
    
    if( !fCubeUse ) {
	puts( "The doubling cube is disabled." );
	return;
    }
	
    printf( "The cube is at %d, ", nCube );

    if( fCubeOwner == -1 )
	puts( "and is centred." );
    else
	printf( "and is owned by %s.", ap[ fCubeOwner ].szName );
}

extern void CommandShowDice( char *sz ) {

    if( fTurn < 0 ) {
	puts( "The dice will not be rolled until a game is started." );

	return;
    }

    if( anDice[ 0 ] < 1 )
	printf( "%s has not yet rolled the dice.\n", ap[ fMove ].szName );
    else
	printf( "%s has rolled %d and %d.\n", ap[ fMove ].szName, anDice[ 0 ],
		anDice[ 1 ] );
}

extern void CommandShowDisplay( char *sz ) {

    if( fDisplay )
	puts( "GNU Backgammon will display boards for computer moves." );
    else
	puts( "GNU Backgammon will not display boards for computer moves." );
}

extern void CommandShowEvaluation( char *sz ) {

    puts( "`eval' and `hint' will use:" );
    ShowEvaluation( &ecEval );
}

extern void CommandShowJacoby( char *sz ) {

    if ( fJacoby ) 
      puts( "Money sessions is played with the Jacoby rule." );
    else
      puts( "Money sessions is played without the Jacoby rule." );

}

extern void CommandShowNackgammon( char *sz ) {

    if( fNackgammon )
	puts( "New games will use the Nackgammon starting position." );
    else
	puts( "New games will use the standard backgammon starting "
	      "position." );
}

extern void CommandShowPipCount( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];

    if( !*sz && fTurn == -1 ) {
	puts( "No position specified and no game in progress." );
	return;
    }
    
    if( ParsePosition( an, sz ) ) {
	puts( "Illegal position." );

	return;
    }
    
    PipCount( an, anPips );
    
    printf( "The pip counts are: %s %d, %s %d.\n", ap[ fMove ].szName,
	    anPips[ 1 ], ap[ !fMove ].szName, anPips[ 0 ] );
}

extern void CommandShowPlayer( char *sz ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	printf( "Player %d:\n"
		"  Name: %s\n"
		"  Type: ", i, ap[ i ].szName );

	switch( ap[ i ].pt ) {
	case PLAYER_GNU:
	    printf( "gnubg:\n" );
	    ShowEvaluation( &ap[ i ].ec );
	    break;
	case PLAYER_PUBEVAL:
	    puts( "pubeval\n" );
	    break;
	case PLAYER_HUMAN:
	    puts( "human\n" );
	    break;
	}
    }
}

extern void CommandShowPostCrawford( char *sz ) {

  if( nMatchTo > 0 ) 
    puts( fPostCrawford ?
	  "This is post-Crawford play." :
	  "This is not post-Crawford play." );
  else if ( ! nMatchTo )
    puts( "Crawford rule is not used in money sessions." );
  else
    puts( "No match is being played." );

}

extern void CommandShowPrompt( char *sz ) {

    printf( "The prompt is set to `%s'.\n", szPrompt );
}

extern void CommandShowRNG( char *sz ) {

  static char *aszRNG[] = {
    "ANSI", "BSD", "ISAAC", "manual", "Mersenne Twister",
    "user supplied"
  };

  printf( "You are using the %s generator.\n",
	  aszRNG[ rngCurrent ] );
    
}

extern void CommandShowRollout( char *sz ) {

    puts( "Rollouts will use:" );
    ShowEvaluation( &ecRollout );

    printf( "%d game%s will be played per rollout, truncating after %d "
	    "pl%s.\nLookahead variance reduction is %sabled.\n",
	    nRollouts, nRollouts == 1 ? "" : "s", nRolloutTruncate,
	    nRolloutTruncate == 1 ? "y" : "ies", fVarRedn ? "en" : "dis" );
}

extern void CommandShowScore( char *sz ) {

    printf( "The score (after %d game%s) is: %s %d, %s %d",
	    cGames, cGames == 1 ? "" : "s",
	    ap[ 0 ].szName, anScore[ 0 ],
	    ap[ 1 ].szName, anScore[ 1 ] );

    if ( nMatchTo > 0 ) {
        printf ( nMatchTo == 1 ? 
	         " (match to %d point%s)\n" :
	         " (match to %d points%s)\n",
                 nMatchTo,
		 fCrawford ? 
		 ", (Crawford game)" : ( fPostCrawford ?
					 ", (post-Crawford play)" : ""));
    } 
    else {
        if ( fJacoby )
	    puts ( " (money session (with Jacoby rule))\n" );
        else
	    puts ( " (money session (without Jacoby rule))\n" );
    }

}

extern void CommandShowTurn( char *sz ) {

    if( fTurn < 0 ) {
	puts( "No game is being played." );

	return;
    }
    
    printf( "%s is on %s.\n", ap[ fMove ].szName,
	    anDice[ 0 ] ? "move" : "roll" );

    if( fResigned )
	printf( "%s has offered to resign a %s.\n", ap[ fMove ].szName,
		aszGameResult[ fResigned - 1 ] );
}

extern void CommandShowWarranty( char *sz ) {

    ShowPaged( aszWarranty );
}

extern void CommandShowKleinman( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];
    float fKC;

    if( !*sz && fTurn == -1 ) {
        puts( "No position specified and no game in progress." );
        return;
    }
 
    if( ParsePosition( an, sz ) ) {
        puts( "Illegal position." );

        return;
    }
     
    PipCount( an, anPips );
 
    fKC = KleinmanCount (anPips[1], anPips[0]);
    if (fKC == -1.0)
        printf ("Pipcount unsuitable for Kleinman Count.\n");
    else
        printf ("Cubeless Winning Chance: %.4f\n", fKC);
 }

extern void CommandShowThorp( char *sz ) {

    int anPips[ 2 ], an[ 2 ][ 25 ];
    int nLeader, nTrailer, anCovered[2], anMenLeft[2];
    int x;

    if( !*sz && fTurn == -1 ) {
        puts( "No position specified and no game in progress." );
        return;
    }

    if( ParsePosition( an, sz ) ) {
        puts( "Illegal position." );

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

        printf("L = %d  T = %d  -> ", nLeader, nTrailer);
        if (nTrailer >= (nLeader - 1))
          printf("re");
        if (nTrailer >= (nLeader - 2))
          printf("double/");
        if (nTrailer < (nLeader - 2))
          printf("no double/");
        if (nTrailer >= (nLeader + 2))
          printf("drop\n");
        else
          printf("take\n");

        printf("Bower's interpolation: %d%% cubeless winning "
                "chance\n", 74+2*(nTrailer-nLeader));
}
