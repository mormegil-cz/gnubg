/*
 * show.c
 *
 * by Gary Wong, 1999
 *
 * $Id$
 */

#include "config.h"

#include <stdio.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "dice.h"

#if !X_DISPLAY_MISSING
#include "xgame.h"
#endif

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

#if !X_DISPLAY_MISSING
    if( fX )
	GameSetBoard( &ewnd, an, TRUE, "", "", 0, 0, 0, 0, 0 );
    else
#endif
	puts( DrawBoard( szOut, an, TRUE, ap ) );

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

extern void CommandShowPipCount( char *sz ) {

    int an[ 2 ];

    /* FIXME take a board argument */
    
    if( fTurn < 0 ) {
	puts( "There must be a game in progress to see the pip count." );

	return;
    }

    PipCount( anBoard, an );
    
    printf( "The pip counts are: %s %d, %s %d.\n", ap[ fMove ].szName,
	    an[ 1 ], ap[ !fMove ].szName, an[ 0 ] );
}

extern void CommandShowPlayer( char *sz ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	printf( "Player %d:\n"
		"  Name: %s\n"
		"  Type: ", i, ap[ i ].szName );

	switch( ap[ i ].pt ) {
	case PLAYER_GNU:
	    printf( "gnubg (%d ply)\n\n", ap[ i ].nPlies );
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

extern void CommandShowRNG( char *sz ) {

  static char *aszRNG[] = {
    "ANSI", "BSD", "ISAAC", "manual", "Mersenne Twister",
    "user supplied"
  };

  printf( "You are using the %s generator.\n",
	  aszRNG[ rngCurrent ] );
    
}


extern void CommandShowJacoby( char *sz ) {

    if ( fJacoby ) 
      puts( "Money sessions is played with the Jacoby rule." );
    else
      puts( "Money sessions is played without the Jacoby rule." );

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

