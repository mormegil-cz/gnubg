 * gfibs.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-1999.
 */

#include "config.h"

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "event.h"
#include "neuralnet.h"
#include "matchequity.h"
#include "buffer.h"
#include "eval.h"

typedef struct _fibs {
    buffer b;
    int h;
    int fMove;
    event evRead, evWrite;
} fibs;

typedef struct _gamedata {
    char szName[ 32 ], szNameOpponent[ 32 ];
    int nMatchTo, nScore, nScoreOpponent;
    int anBoard[ 28 ]; /* 0 and 25 are the bars */
    int fTurn; /* -1 is X, 1 is O, 0 if game over */
    int anDice[ 2 ], anDiceOpponent[ 2 ]; /* 0, 0 if not rolled */
    int nCube;
    int fDouble, fDoubleOpponent; /* allowed to double */
    int fDoubled; /* opponent just doubled */
    int fColour; /* -1 for player X, 1 for player O */
    int fDirection; /* -1 playing from 24 to 1, 1 playing from 1 to 24 */
    int nHome, nBar; /* 0 or 25 depending on fDirection */
    int nOff, nOffOpponent; /* number of men borne off */
    int nOnBar, nOnBarOpponent; /* number of men on bar */
    int nToMove; /* 0 to 4 -- number of pieces to move */
    int nForced, nCrawford; /* unused */
    int nRedoubles; /* number of instant redoubles allowed */
} gamedata;

char szBot[ 32 ], szOpponent[ 32 ], szShoutLog[ 32 ], szKibitzLog[ 32 ];
int fWon = -1, cIntro = -1, fQuit = FALSE, fJoining = TRUE;
int fResigned = 0, fIDoubled = 0;
FILE *pfLog, *pfShouts, *pfKibitz;
float arDouble[ 4 ];

static int FibsParse( fibs *pf, char *sz );
static int IsDropper ( char *szName );

static int FibsReadNotify( event *pev, fibs *pf ) {

    char sz[ 1024 ], *pch;
    int cch;

    /* FIXME write whole function properly */
    
    while( 1 ) {
        if( ( cch = BufferCopyFrom( &pf->b, sz, sizeof( sz ) - 1 ) ) < 0 ) {
            /* FIXME error... signal close and destroy */
	    StopEvents();
	    
            return -1;
        }

	sz[ cch ] = 0;

	/* Check for responses to strings with no linefeeds.  A bit grotty,
	   but it's too hard to do elsewhere. */

	if( !strcmp( sz, "login: " ) ) {
	    BufferWritef( &pf->b, "connect %s chinchilla\r\n", szBot );
	    BufferConsume( &pf->b, 7 );
	    
	    return 0;
	}
	
	if( !( pch = strchr( sz, '\n' ) ) ) {
	    EventPending( pev, FALSE );

	    return 0;
	}

	*pch = 0;

	BufferConsume( &pf->b, pch - sz + 1 );

        if( *--pch == '\r' )
            *pch = 0;

	FibsParse( pf, sz );

	fflush ( stdout );

        /* FIXME if( message handler destroys this connection ) return -1; */
        
        /* FIXME have some escape in here, so that if the message handler
           couldn't complete (or may not be able to complete the next
           request?), mark this handler as no longer being ready until
           some output buffers empty */	
    }
}

static eventhandler FibsReadHandler = {
    (eventhandlerfunc) FibsReadNotify, NULL
};

static eventhandler FibsWriteHandler = { NULL, NULL };

static int FibsCreate( fibs *pf, char *szHost, int nPort ) {

    struct sockaddr_in sin;
    
    if( ( pf->h = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
	return -1;

    sin.sin_family = AF_INET;
    /* FIXME check if these fail */
    bcopy( gethostbyname( szHost )->h_addr,
           (char *) &sin.sin_addr, sizeof( sin.sin_addr ) );
    sin.sin_port = htons( nPort );
    
    if( connect( pf->h, (struct sockaddr *) &sin, sizeof( sin ) ) < 0 ) {
	close( pf->h );
	
        /* FIXME something better than this! */
        
        return -1;
    }

    EventCreate( &pf->evRead, &FibsReadHandler, pf );
    EventCreate( &pf->evWrite, &FibsWriteHandler, pf );

    EventHandlerReady( &pf->evRead, TRUE, -1 );
    EventHandlerReady( &pf->evWrite, TRUE, -1 );

    BufferCreate( &pf->b, pf->h, pf->h, 0x10000, 0x1000, -1, -1,
		  &pf->evRead, &pf->evWrite );
    
    return 0;
}

static int FibsCommand( fibs *pf, char *szCommand ) {

    BufferWritef( &pf->b, "%s\r\n", szCommand );

    return 0;
}

static int FibsDestroy( fibs *pf ) {

    EventDestroy( &pf->evRead );
    EventDestroy( &pf->evWrite );

    return BufferDestroy( &pf->b );
}

typedef struct _input {
    fibs *pf;
    event evRead;
} input;

static int InputReadNotify( event *pev, input *pin ) {

    char sz[ 512 ];
    int cch;
    
    if( ( cch = read( STDIN_FILENO, sz, sizeof( sz ) - 1 ) ) <= 0 ) {
	fQuit = TRUE;
	    
	StopEvents();
	
	return 0;
    }

    EventPending( pev, FALSE );
    
    sz[ cch ] = 0;

    while( cch && ( sz[ cch - 1 ] == '\r' || sz[ cch - 1 ] == '\n' ) )
	sz[ --cch ] = 0;

    if( !strcmp( sz, ".join" ) ) {
	fJoining = TRUE;

	puts( "Joining enabled." );
	
	return 0;
    } else if( !strcmp( sz, ".nojoin" ) ) {
	fJoining = FALSE;

	puts( "Joining disabled." );

	return 0;
    } else
	return FibsCommand( pin->pf, sz );
}

static eventhandler InputReadHandler = {
    (eventhandlerfunc) InputReadNotify, NULL
};

static int InputCreate( input *pin, fibs *pf ) {

    pin->pf = pf;
    
    EventCreate( &pin->evRead, &InputReadHandler, pin );

    pin->evRead.h = STDIN_FILENO;
    pin->evRead.fWrite = FALSE;
    
    return EventHandlerReady( &pin->evRead, TRUE, -1 );
}

static int InputDestroy( input *pin ) {

    return EventDestroy( &pin->evRead );
}

static int BoardSet( gamedata *pgd, char *pch ) {

    char *pchDest;
    int i, *pn, **ppn;
    int *apnMatch[] = { &pgd->nMatchTo, &pgd->nScore, &pgd->nScoreOpponent };
    int *apnGame[] = { &pgd->fTurn, pgd->anDice, pgd->anDice + 1,
		       pgd->anDiceOpponent, pgd->anDiceOpponent + 1,
		       &pgd->nCube, &pgd->fDouble, &pgd->fDoubleOpponent,
		       &pgd->fDoubled, &pgd->fColour, &pgd->fDirection,
		       &pgd->nHome, &pgd->nBar, &pgd->nOff, &pgd->nOffOpponent,
		       &pgd->nOnBar, &pgd->nOnBarOpponent, &pgd->nToMove,
		       &pgd->nForced, &pgd->nCrawford, &pgd->nRedoubles };
	    
    if( strncmp( pch, "board:", 6 ) )
	return -1;
    
    pch += 6;

    for( pchDest = pgd->szName, i = 31; i && *pch && *pch != ':'; i-- )
	*pchDest++ = *pch++;

    *pchDest = 0;

    if( !pch )
	return -1;

    pch++;
    
    for( pchDest = pgd->szNameOpponent, i = 31; i && *pch && *pch != ':'; i-- )
	*pchDest++ = *pch++;

    *pchDest = 0;

    if( !pch )
	return -1;

    for( i = 3, ppn = apnMatch; i--; ) {
	if( *pch++ != ':' ) /* FIXME should really check end of string */
	    return -1;

	**ppn++ = strtol( pch, &pch, 10 );
    }

    for( i = 0, pn = pgd->anBoard; i < 26; i++ ) {
	if( *pch++ != ':' )
	    return -1;

	*pn++ = strtol( pch, &pch, 10 );
    }

    for( i = 21, ppn = apnGame; i--; ) {
	if( *pch++ != ':' )
	    return -1;

	**ppn++ = strtol( pch, &pch, 10 );
    }

    if( pgd->fColour < 0 )
	pgd->nOff = -pgd->nOff;
    else
	pgd->nOffOpponent = -pgd->nOffOpponent;
    
    if( pgd->fDirection < 0 ) {
	pgd->anBoard[ 26 ] = pgd->nOff;
	pgd->anBoard[ 27 ] = pgd->nOffOpponent;
    } else {
	pgd->anBoard[ 26 ] = pgd->nOffOpponent;
	pgd->anBoard[ 27 ] = pgd->nOff;
    }
    
    return 0;
}

static void FixName( char *pch ) {

    while( ( *pch >= 'a' && *pch <= 'z' ) ||
	   ( *pch >= 'A' && *pch <= 'Z' ) ||
	   *pch == '_' )
	pch++;
    
    *pch = 0;
}

static int FibsParse( fibs *pf, char *sz ) {

    char szName[ 1024 ], szTemp[ 1024 ];
    int n;
    gamedata gd;
    evalcontext ecMoves = { 1, 8, 0.16 };
    evalcontext ecDouble = { 2, 0, 0 };
    
    puts( sz );

    if( !strcmp( sz, "dirac says: quit" ) ) {
	FibsCommand( pf, "bye" );
	
	fQuit = TRUE;

	return 0;
    }
    
    if( !strncmp( sz, "dirac says: do ", 15 ) ) {
	FibsCommand( pf, sz + 15 );

	return 0;
    }

    if( sscanf( sz, "%s shouts: %s", szTemp, szTemp ) == 2 ) {
	
      if ( strstr ( sz, "mpgnu" ) || strstr ( sz, "bot" ) ) {

	/* this might be an interesting shout */

	fprintf ( pfShouts, "%s\n", sz );

      }

      return 0;

    }

    if ( sscanf( sz, "Starting a new game with %s", szName ) == 1 ) {

	printf ("starting a new game");
	FixName( szName );
	strcpy( szOpponent, szName );

    }

    
    if( ( sscanf( sz, "Player %s has joined you for a", szName ) == 1 ) ||
	( sscanf( sz, "** You are now playing a %i point match with %s",
		  &n, szName ) == 2 ) ) {

	printf ("starting a new match");
	FixName( szName );

	strcpy( szOpponent, szName );

	cIntro = 0;

	return 0;
    }

    if( sscanf( sz, "%s has joined you. Your running match was %s",
		szName, szTemp ) == 2 ) {
	FixName( szName );

	strcpy( szOpponent, szName );

	FibsCommand( pf, "board" );

	cIntro = 0;
	
	return 0;
    }

    if( ( ( sscanf( sz, "%s kibitzes: rol%c", szName, szTemp ) == 2 &&
	*szTemp == 'l' ) ) 
	|| ! strncmp ( sz, "It's your turn. Please roll or double", 37
		       )
	|| ! strncmp ( sz, "It's your turn to roll or double", 32 ) 
	|| ! strncmp ( sz, "Please move", 11 ) 
	|| ! strncmp ( sz, "** You did already roll the dice.", 33 ) )

      {
                            

      printf ( "board...\n" );

      /* the board command should make me roll */
      
      FibsCommand( pf, "board" ); 

      sleep ( 2 );

      printf ( "thx for the board\n" );

	return 0;
    }

    if( sscanf( sz, "%s kibitzes: joi%c", szName, szTemp ) == 2 &&
	*szTemp == 'n' ) {

	FibsCommand( pf, "join" );
	
	return 0;
    }

    if( sscanf( sz, "%s kibitzes: mov%c", szName, szTemp ) == 2 &&
	*szTemp == 'e' ) {
	FibsCommand( pf, "board" );
	
	return 0;
    }

    if( sscanf( sz, "%s wants to resign. You will win %i point", 
		szName, &fResigned ) == 2 ) {

      /* FIXME: check for: **blabla wanted to resign */

      printf ( "Opponent wants to resign %i points.\n", fResigned );

      FibsCommand ( pf, "board" );

      return 0;

    }
    
    if( sscanf( sz, "%s wants to play a %d point match with %s", szName, &n,
                szTemp ) == 3 ) {
	if( ( n>=3 ) && ( n <= 17 ) ) {
	    if( fJoining )
	      // if( fJoining && ( strcmp( "dirac", szName ) == 0 ) )
	      BufferWritef( &pf->b, "invite %s\r\n", szName );

	    return 0;
	} else{
	    BufferWritef( &pf->b, 
			  "tell %s Sorry, I'm a bot, and I "
			  "only play 3-17 point matches\r\n",
			  szName );
	    
	    return 0;
	}
    }

    if( ( sscanf( sz, "%s wants to resume %s", szName, szTemp ) == 2 ) &&
	fJoining ) {

      sleep ( 2 );
      BufferWritef( &pf->b, "invite %s\r\n", szName );

      return 0;
    }

    if ( ! strncmp ( sz, "Type 'join' if you want to pla", 30 ) ) {

      // FibsCommand ( pf, "join" );

      printf ( "trying to join...\n" );

      strcpy ( szTemp, "join\r\n\n\n" );
      write ( pf->h, szTemp, strlen ( szTemp ) );

      return 0;

    }

    
    if( sscanf( sz, "** There's no saved match with %s", szName ) == 1 ) {
	FixName( szName );

	if ( ! IsDropper ( szName ) ) 
	  BufferWritef( &pf->b, "join %s\r\n", szName );
	else
	  BufferWritef( &pf->b, "tell %s you\'re blacklisted...\r\n" );

	return 0;	
    }

    if( sscanf( sz, "%s win the %i point match %i-%i", szName, &n, &n, &n )
	== 4 ||
	sscanf( sz, "%s wins the %i point match", szName, &n ) == 2 ) {

      int n1, n2, n3;
      time_t t;
      
      printf ( "The match is over...\n" );
      printf ( "sz=%s\nszName=%s\n", sz , szName );

      if ( ! strcmp ( szName, "You" ) ) {
	sscanf ( sz, "You win the %i point match %i-%i", 
		 &n1, &n2, &n3 ); 
	fWon = TRUE;
	printf ( "n1,n2,n3=%i,%i,%i\n", n1,n2,n3);
      }
      else {
	sscanf ( sz, "%s wins the %i point match %i-%i",
		 szTemp, &n1, &n2, &n3 );
	fWon = FALSE;
	printf ( "n1,n2,n3=%i,%i,%i\n", n1,n2,n3);
      }
      
      BufferWritef( &pf->b, "rawwho %s\r\n", szOpponent );

      time ( &t );

      fprintf ( pfLog, "%s: %2i-%2i (%2i point match), ",
		ctime ( &t ), n2, n3, n1 );
      
      return 0;
    }
 
    if( fWon >= 0 && !strncmp( sz, "who:", 4 ) ) {

      int nRating, nRatingCents, nExperience;
	
      sscanf( sz, "who: %*d %*c%*c%*c %*s %d.%d %d", &nRating, &nRatingCents,
	      &nExperience );

      fprintf( pfLog, "%5d %4d.%02d %c %s\n", 
	       nExperience, nRating,
	       nRatingCents, fWon ? 'w' : 'l', szOpponent );

      fflush( pfLog );

      BufferWritef( &pf->b, "tell %s thanks for the match\r\n",
		    szOpponent);

      fWon = -1;
    }

    if( sscanf( sz, "%s doubles. Type %s", szName, szTemp ) == 2 ) {

      printf ( "I'm doubled...\n" );
      
      FibsCommand( pf, "board" );

      return 0;

    }

    if(  sscanf( sz, "%s kibitzes: %s", szName, szTemp ) == 2 ) {

      if ( strcmp ( szName, "mpgnu" ) )

	fprintf ( pfKibitz, "%s: %s\n", szName, szTemp );

      return 0;

    }


    if( !strncmp( sz, "board:", 6 ) ) {
	int anBoard[ 2 ][ 25 ], anMove[ 8 ], i, c;
	cubeinfo ci;
	
	BoardSet( &gd, sz );

	if( gd.nMatchTo > MAXSCORE && gd.nMatchTo != 39 ) {

	    FibsCommand( pf, "leave" );

	    return 0;
	}

	if( gd.anDiceOpponent[ 0 ] && ! ( fResigned ) ) { 
	  /* not our move */

	  if ( fIDoubled ) {

	    /* we are not about to display the kibitzes below */

	    fIDoubled = 0;

	    sprintf ( szTemp, "kibitz your mwc for "
		      "Double, take: %7.4f and Double, pass: %7.4f",
		      1.0 - arDouble[ 2 ], 1.0 - arDouble[ 3 ] );

	    FibsCommand ( pf, szTemp );

	    return 0;

	  }
	    switch( cIntro ) {
	    case 0:
		FibsCommand( pf, "k Hi!  I'm a computer player.  Please send "
			     "comments to <joern@thyssen.nu>.  If I "
			     "forget to move, type `kibitz move'," 
			     "`kibitz roll', or `kibitz join'."
			     "Technical details: checkerplay 1-ply"
			     "(2-3 seconds pr move, "
			     "equivalent to Snowie 2-ply) and "
			     "doubling decisions on 2-ply"
			     " (10-15 seconds to decide)");
		cIntro = 1;
		break;
		
	    case 1:
		FibsCommand( pf, "k GNU Backgammon is free software, and you "
			     "are welcome to obtain, modify and distribute "
			     "copies of it under certain conditions.  Please "
			     "see http://www.gnu.org/software/gnubg/ for "
			     "more information." );
		cIntro = -1;
		break;
	    }
	    
	    return 0;
	}

	if( gd.fDirection > 0 )
	    for( i = 0; i < 25; i++ ) {
		anBoard[ 0 ][ i ] = ( gd.anBoard[ i + 1 ] * gd.fColour < 0 ) ?
		    gd.anBoard[ i + 1 ] * -gd.fColour : 0;
		anBoard[ 1 ][ i ] = ( gd.anBoard[ 24 - i ] * gd.fColour > 0 ) ?
		    gd.anBoard[ 24 - i ] * gd.fColour : 0;
	    }
	else
	    for( i = 0; i < 25; i++ ) {
		anBoard[ 0 ][ i ] = ( gd.anBoard[ 24 - i ] * gd.fColour < 0 ) ?
		    gd.anBoard[ 24 - i ] * -gd.fColour : 0;
		anBoard[ 1 ][ i ] = ( gd.anBoard[ i + 1 ] * gd.fColour > 0 ) ?
		    gd.anBoard[ i + 1 ] * gd.fColour : 0;
	    }

	anBoard[ 0 ][ 24 ] = abs( gd.anBoard[ gd.fDirection < 0 ? 0 : 25 ] );
	anBoard[ 1 ][ 24 ] = abs( gd.anBoard[ gd.fDirection > 0 ? 0 :
					    25 ] );

	/* set variables from eval.c */

	nMatchTo     = gd.nMatchTo;
	anScore[ 0 ] = gd.nScore;
	anScore[ 1 ] = gd.nScoreOpponent;
	fCrawford    = 0;
	fPostCrawford = 0; /* FIXME: use the gd.nCrawford instead */

	/* the fibs nCrawford flag is actually a Post-Crawford flag */

	if ( ( nMatchTo - anScore[ 0 ] == 1 || nMatchTo - anScore[ 1 ]
	       == 1 ) ) {

	  if ( gd.nCrawford )
	    fPostCrawford = 1;
	  else
	    fCrawford = 1;
	}

	if ( gd.fDouble && gd.fDoubleOpponent )
	  fCubeOwner = -1;
	else if ( gd.fDouble )
	  fCubeOwner = 0;
	else
	  fCubeOwner = 1;

	SetCubeInfo ( &ci, gd.nCube, fCubeOwner, 0 );

	printf ( "nCube, owner %1i %+1i\n", gd.nCube, fCubeOwner );
	printf ( "gammon price %7.4f %7.4f %7.4f %7.4f\n",
		 ci.arGammonPrice[ 0 ], ci.arGammonPrice[ 1 ],
		 ci.arGammonPrice[ 2 ], ci.arGammonPrice[ 3 ] );


	if ( fResigned ) {

	  float rEq, rMwc;
	  int nR;

	  EvaluatePositionCubeful ( anBoard, arDouble, &ci,
				    &ecDouble, ecDouble.nPlies );

	  nR = fResigned / ci.nCube;

	  if ( nR == 1 )
	    rEq = 1.0;
	  else if ( nR == 2 )
	    rEq = ci.arGammonPrice[ 0 ] + 1.0;
	  else 
	    rEq = ci.arGammonPrice[ 2 ] + ci.arGammonPrice[ 0 ] + 1.0;

	  rMwc = eq2mwc ( rEq, &ci );

	  printf ( "Mwc for resignation:\n"
		   "mwc current position : %7.4f\n"
		   "mwc after resignation: %7.4f (%+7.4f)\n",
		   arDouble[ 0 ], rMwc, rEq );

	  if ( rMwc >= arDouble[ 0 ] )
	    FibsCommand ( pf, "accept" );
	  else
	    FibsCommand ( pf, "reject" );

	  fResigned = 0;

	  return 0;
	  

	}
	else if ( gd.fDoubled ) {

	  /* I'm doubled, consider the take */

	  ci.fMove = 1; /* other player on roll */
	  SwapSides ( anBoard );
	  
	  EvaluatePositionCubeful( anBoard, arDouble, &ci,
				   &ecDouble, ecDouble.nPlies );

	  sprintf ( szTemp, 
		    "kibitz your mwc"
		    "%7.4f (no double)"
		    "%7.4f (double, take)"
		    "%7.4f (double, pass)\r\n",
		    arDouble[ 1 ], arDouble[ 2 ], arDouble[ 3 ] );
	  write ( pf->h, szTemp, strlen ( szTemp ) );

	  if ( arDouble[ 3 ] < arDouble[ 2 ] )
	    strcpy ( szTemp, "reject\r\n" );
	  else
	    strcpy ( szTemp, "accept\r\n" );

	  printf ( "cube action: %s", szTemp );

	  write ( pf->h, szTemp, strlen ( szTemp ) );

	  return 0;

	}
	
	else if ( ! gd.anDice[ 0 ] && ( gd.fTurn == gd.fColour ) ) {

	  /* consider doubling */

	  int fUseCube;

	  fUseCube = gd.fDouble &&   /* I can double */
	    ( ! fCrawford ) &&       /* it's not the crawford game */
	    ( gd.nScore + gd.nCube < nMatchTo ); /* cube is alive */

	  if ( fUseCube ) {

	    /* I'm allowed to double */

	    EvaluatePositionCubeful( anBoard, arDouble, &ci,
				     &ecDouble, ecDouble.nPlies );

	    printf ("no double:    mwc %6.3f\n", arDouble[ 1 ] );
	    printf ("double, take: mwc %6.3f\n", arDouble[ 2 ] );
	    printf ("double, pass: mwc %6.3f\n", arDouble[ 3 ] );

	    if ( ( arDouble[ 2 ] >= arDouble[ 1 ] ) &&
		 ( arDouble[ 3 ] >= arDouble[ 1 ] ) ) {
	      FibsCommand ( pf, "double" );
	      fIDoubled = 1;
	    }
	    else
	      FibsCommand ( pf, "roll" );



	    return 0;
	    

	  }
	  else if ( ! gd.anDice[ 0 ] ) {
	    
	    FibsCommand ( pf, "roll" );

	    return 0;

	  }


	} else { 

	  if ( gd.anDice[ 0 ] ) {

	  float arOutput[ NUM_OUTPUTS ];

	  c = FindBestMove( anMove, gd.anDice[ 0 ], gd.anDice[ 1 ],
			    anBoard, &ci, &ecMoves );

	  EvaluatePosition ( anBoard, arOutput, &ci, &ecMoves );

	  printf("\tWin\tW(g)\tW(bg)\tL(g)\tL(bg)\tEquity\tmwc\n");
	  printf("\t%6.4f\t%6.4f\t%6.4f\t%6.4f\t%6.4f\t%+7.4f\t%6.4f\n",
		 arOutput[ 0 ], arOutput[ 1 ], arOutput[ 2 ],
		 arOutput[ 3 ], arOutput[ 4 ], 
		 Utility ( arOutput, &ci ),
		 UtilityMwc ( arOutput, &ci ) );

#if 1	
	for( i = 0; i < c >> 1; i++ ) {
	    if( anMove[ 2 * i ] == 24 )
		printf( "bar " );
	    else
		printf( "%d ", gd.fDirection < 0 ?
			      anMove[ 2 * i ] + 1 : 24 - anMove[ 2 * i ] );

	    if( anMove[ 2 * i + 1 ] < 0 )
		printf( "off " );
	    else
		printf( "%d ", gd.fDirection < 0 ?
			      anMove[ 2 * i + 1 ] + 1 :
			      24 - anMove[ 2 * i + 1 ] );
	}
	putchar( '\n' );

#endif

	sleep ( 2 );

	  for( i = 0; i < c >> 1; i++ ) {
	    if( anMove[ 2 * i ] == 24 )
	      BufferWritef( &pf->b, "bar " );
	    else
	      BufferWritef( &pf->b, "%d ", gd.fDirection < 0 ?
			    anMove[ 2 * i ] + 1 : 24 - anMove[ 2 * i ] );
	    
	    if( anMove[ 2 * i + 1 ] < 0 )
	      BufferWritef( &pf->b, "off " );
	    else
	      BufferWritef( &pf->b, "%d ", gd.fDirection < 0 ?
			    anMove[ 2 * i + 1 ] + 1 :
			    24 - anMove[ 2 * i + 1 ] );
	  }
	  printf ("sending move command...\n");
	
	  FibsCommand( pf, "" );
	
	  return 0;

	}
	  else {
	    printf ("board, but no dice\n" );
	}
    }
    
    return 0;
}

int main( int argc, char *argv[] ) {

    fibs f;
    input in;
    int nDelay;
    time_t tStart;
    
    if( argc < 2 ) {
	fprintf( stderr, "usage:\n\n %s botname\n", argv[ 0 ] );
	
	return 1;
    }
    
    strcpy( szBot, argv[ 1 ] );

    pfLog = fopen( szBot, "a" );

    strcpy ( szShoutLog, szBot );
    pfShouts = fopen ( strcat ( szShoutLog, ".shouts" ) , "a" );

    strcpy ( szKibitzLog, szBot );
    pfKibitz = fopen ( strcat ( szKibitzLog, ".kibitz" ) , "a" );
    
    EvalInitialise( GNUBG_WEIGHTS, GNUBG_WEIGHTS_BINARY, GNUBG_BEAROFF
		    );

    CalcMatchEq ();

    // SetGammonPrice( 0, 0, 0, 0 );
    InitEvents();
    
    nDelay = 10;
    
    while( 1 ) {
	if( !FibsCreate( &f, "fibs.com", 4321 ) ) {
	    tStart = time( NULL );
	    
	    InputCreate( &in, &f );

	    HandleEvents();

	    InputDestroy( &in );

	    FibsDestroy( &f );

	    if( fQuit )
		return 0;
	    
	    if( time( NULL ) - tStart > 60 )
		nDelay = 10;
	    else
		nDelay <<= 1;
	} else
	    nDelay <<= 1;

	if( nDelay > 60 * 15 )
	    nDelay = 60 * 15;
	
	sleep( nDelay );
    }
}

static int IsDropper ( char *szName ) {

  /* check with known list of droppers */

  static char *aszDroppers[] = { "tiggie", "phs", NULL };
  int i;

  for ( i = 0; aszDroppers[ i ] != NULL ; i++ ) {

    if ( ! strcmp ( szName, aszDroppers[ i ] ) )
      return 1;

  }

  return 0;

}
