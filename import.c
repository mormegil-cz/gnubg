/*
 * import.c
 *
 * by Øystein Johansen, 2000, 2001, 2002
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
 */

#include "config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backgammon.h"
#include "drawboard.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "import.h"
#include "positionid.h"

static int ReadInt16( FILE *pf ) {

    /* Read a little-endian, signed (2's complement) 16-bit integer.
       This is inefficient on hardware which is already little-endian
       or 2's complement, but at least it's portable. */

    /* FIXME what about error handling? */
    
    unsigned char auch[ 2 ];
    int n;
    
    fread( auch, 2, 1, pf );
    n = auch[ 0 ] | ( auch[ 1 ] << 8 );

    if( n >= 0x8000 )
	n -= 0x10000;

    return n;
}

extern void
ImportJF( FILE * fp, char *szFileName) {

  int nVersion, nCubeOwner, nOnRoll, nMovesLeft, nMovesRight;
  int nGameOrMatch, nOpponent, nLevel, nScore1, nScore2;
  int nDie1, nDie2; 
  int fCaution, fCubeUse, fJacoby, fBeavers, fSwapDice, fJFplayedLast;
  int i, idx, anNew[26], anOld[26];
  char szPlayer1[40];
  char szPlayer2[40];
  char szLastMove[25];
  unsigned char c;
  moverecord *pmr;
  
  nVersion = ReadInt16( fp );

  if ( nVersion < 124 || nVersion > 126 ){
    outputl("File not recognised as Jellyfish file.");
    return;
  }
  if ( nVersion == 126 ){
    /* 3.0 */
      fCaution = ReadInt16( fp );
      ReadInt16( fp ); /* Not in use */
  }

  if ( nVersion == 125 || nVersion == 126 ){
    /* 1.6 or newer */
    /* 3 variables not used by older version */
      fCubeUse = ReadInt16( fp );
      fJacoby = ReadInt16( fp );
      fBeavers = ReadInt16( fp );

      if ( nVersion == 125 ){
	  /* If reading, caution can be set here
	     use caution = */
      }
  }

  if ( nVersion == 124 ){
    /* 1.0, 1.1 or 1.2 
       If reading, the other variables can be set here */
    /*      use cube =
            jacoby =
            beaver =
            use caution =
    */
  }

  ms.nCube = ReadInt16( fp );
  nCubeOwner = ReadInt16( fp );
  /* Owner: 1 or 2 is player 1 or 2, 
     respectively, 0 means cube in the middle */

  ms.fCubeOwner = nCubeOwner - 1;

  nOnRoll = ReadInt16( fp );
  /* 0 means starting position. 
     If you add 2 to the player (to get 3 or 4)   Sure?
     it means that the player is on roll
     but the dice have not been rolled yet. */

  nMovesLeft = ReadInt16( fp );
  nMovesRight = ReadInt16( fp );
  /* These two variables are used when you use movement #1,
     (two buttons) and tells how many moves you have left
     to play with the left and the right die, respectively.
     Initialized to 1 (if you roll a double, left = 4 and
     right = 0). If movement #2 (one button), only the first
     one (left) is used to store both dice.  */

  ReadInt16( fp ); /* Not in use */

  nGameOrMatch = ReadInt16( fp );
  /* 1 = match, 3 = game */

  nOpponent = ReadInt16( fp );
  /* 1 = 2 players, 2 = JF plays one side */

  nLevel = ReadInt16( fp );

  ms.nMatchTo = ReadInt16( fp );
  /* 0 if single game  */

  if(nGameOrMatch == 3)
      ms.nMatchTo = 0;

  nScore1 = ReadInt16( fp );
  nScore2 = ReadInt16( fp );
  /* Can be whatever if match length = 0  */

  ms.anScore[0] = nScore1;
  ms.anScore[1] = nScore2;

  fread(&c, 1, 1 , fp);
  for (i = 0 ; i < c; i++) fread(&szPlayer1[i], 1, 1, fp);
  szPlayer1[c]='\0';
 
  if (nOpponent == 2) strcpy(szPlayer1, "Jellyfish");

  fread(&c, 1, 1 , fp);
  for (i = 0 ; i < c; i++) fread(&szPlayer2[i], 1, 1, fp);
  szPlayer2[c]='\0';

  fSwapDice = ReadInt16( fp );
  /* TRUE if lower die is to be drawn to the left  */

  switch( ReadInt16( fp ) ) {
  case 2: /* Crawford */
      ms.fPostCrawford = FALSE;
      ms.fCrawford = TRUE;
      break;
      
  case 3: /* post-Crawford  */
      ms.fPostCrawford = TRUE;
      ms.fCrawford = FALSE;
      break;

  default:
      ms.fCrawford = ms.fPostCrawford = FALSE;
      break;
  }
  
  if(nGameOrMatch == 3) ms.fCrawford = ms.fPostCrawford = FALSE;

  fJFplayedLast = ReadInt16( fp );

  fread(&c, 1, 1 , fp);
  for (i = 0 ; i < c; i++) fread(&szLastMove[i], 1, 1, fp);
  szLastMove[c]='\0';
  /* Stores whether the last move was played by JF
     If so, the move is stored in a string to be 
     displayed in the 'Show last' dialog box  */

  nDie1 = abs( ReadInt16( fp ) );
  nDie2 = ReadInt16( fp );  
  /*  if ( nDie1 < 0 ) { nDie1=65536; } */ /*  What?? */


  /* In the end the position itself is stored, 
     as well as the old position to be able to undo. 
     The two position arrays can be read like this:   */

  for ( i=0; i < 26; i++ ) {
      anNew[ i ] = ReadInt16( fp ) - 20;
      anOld[ i ] = ReadInt16( fp ) - 20;
      /* 20 has been added to each number when storing */
  }
  /* Player 1's checkers are represented with negative numbers, 
     player 2's with positive. The arrays are representing the 
     26 different points on the board, starting with anNew[0]
     which is the upper bar and ending with anNew[25] which is 
     the bottom bar. The remaining numbers are in the opposite 
     direction of the numbers you see if you choose 'Numbers' 
     from the 'View' menu, so anNew[1] is marked number 24
     on the screen. */


  if (nOnRoll == 1 || nOnRoll == 3)
    idx = 0;
  else
    idx = 1;
  
  ms.fTurn = idx;
 
  if (nOnRoll == 0) ms.fTurn = -1; 

  if (nMovesLeft + nMovesRight > 1) {
    ms.anDice[0] = nDie1;
    ms.anDice[1] = nDie2;
  } else {
    ms.anDice[0] = 0;
    ms.anDice[1] = 0;
  } 

  for( i = 0; i < 25; i++ ) {
    ms.anBoard[ idx ][ i ] = ( anNew[ i + 1 ]  < 0 ) ?  -anNew[ i + 1 ]  : 0;
    ms.anBoard[ ! idx ][ i ] = ( anNew[ 24 - i ]  > 0 ) ?
	anNew[ 24 - i ]  : 0;
  }

  ms.anBoard[ ! idx ][ 24 ] =  anNew[0];
  ms.anBoard[ idx ][ 24 ] =  -anNew[25];

  if( ms.anDice[ 0 ] ) {
      pmr = malloc( sizeof( pmr->sd ) );
      pmr->sd.mt = MOVE_SETDICE;
      pmr->sd.sz = NULL;
      pmr->sd.fPlayer = ms.fTurn;
      pmr->sd.anDice[ 0 ] = ms.anDice[ 0 ];
      pmr->sd.anDice[ 1 ] = ms.anDice[ 1 ];
      pmr->sd.lt = LUCK_NONE;
      pmr->sd.rLuck = ERR_VAL;
      AddMoveRecord( pmr );
  }

  pmr = malloc( sizeof( pmr->sb ) );
  pmr->sb.mt = MOVE_SETBOARD;
  pmr->sb.sz = NULL;
  if( ms.fTurn )
      SwapSides( ms.anBoard );
  PositionKey( ms.anBoard, pmr->sb.auchKey );
  if( ms.fTurn )
      SwapSides( ms.anBoard );
  AddMoveRecord( pmr );

  pmr = malloc( sizeof( pmr->scv ) );
  pmr->scv.mt = MOVE_SETCUBEVAL;
  pmr->scv.sz = NULL;
  pmr->scv.nCube = ms.nCube;
  AddMoveRecord( pmr );

  pmr = malloc( sizeof( pmr->scp ) );
  pmr->scp.mt = MOVE_SETCUBEPOS;
  pmr->scp.sz = NULL;
  pmr->scp.fCubeOwner = ms.fCubeOwner;
  AddMoveRecord( pmr );
  
  return;
}  

static int fWarned, fPostCrawford;

static void ParseMatMove( char *sz, int iPlayer ) {

    char *pch;
    moverecord *pmr;
    int i, c;
    
    sz += strspn( sz, " \t" );

    if( !*sz )
	return;

    for( pch = strchr( sz, 0 ) - 1;
	 pch >= sz && ( *pch == ' ' || *pch == '\t' ); pch-- )
	*pch = 0;
    
    if( sz[ 0 ] >= '1' && sz[ 0 ] <= '6' && sz[ 1 ] >= '1' && sz[ 1 ] <= '6' &&
	sz[ 2 ] == ':' ) {
        pmr = malloc( sizeof( pmr->n ) );
        pmr->n.mt = MOVE_NORMAL;
	pmr->n.sz = NULL;
        pmr->n.anRoll[ 0 ] = sz[ 0 ] - '0';
        pmr->n.anRoll[ 1 ] = sz[ 1 ] - '0';
        pmr->n.fPlayer = iPlayer;
	pmr->n.ml.cMoves = 0;
	pmr->n.ml.amMoves = NULL;
        pmr->n.esDouble.et = EVAL_NONE;
        pmr->n.esChequer.et = EVAL_NONE;
	pmr->n.lt = LUCK_NONE;
	pmr->n.rLuck = ERR_VAL;
	pmr->n.st = SKILL_NONE;
	
	if( ( c = ParseMove( sz + 3, pmr->n.anMove ) ) >= 0 ) {
	    for( i = 0; i < ( c << 1 ); i++ )
		pmr->n.anMove[ i ]--;
	    if( c < 4 )
		pmr->n.anMove[ c << 1 ] = pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;
	    
	    AddMoveRecord( pmr );
	} else
	    free( pmr );
    } else if( !strncasecmp( sz, "double", 6 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_DOUBLE;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
    } else if( !strncasecmp( sz, "take", 4 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_TAKE;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
    } else if( !strncasecmp( sz, "drop", 4 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_DROP;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
    } else if( !strncasecmp( sz, "win", 3 ) ) {
	if( ms.gs == GAME_PLAYING ) {
	    /* Neither a drop nor a bearoff to win, so we presume the loser
	       resigned. */
	    pmr = malloc( sizeof( pmr->r ) );
	    pmr->r.mt = MOVE_RESIGN;
	    pmr->r.sz = NULL;
            pmr->r.esResign.et = EVAL_NONE;
	    pmr->r.fPlayer = !iPlayer;
	    if( ( pmr->r.nResigned = atoi( sz + 4 ) / ms.nCube ) < 1 )
		pmr->r.nResigned = 1;
	    else if( pmr->r.nResigned > 3 )
		pmr->r.nResigned = 3;
	    AddMoveRecord( pmr );
	}
    } else if( !fWarned ) {
	outputf( "Unrecognised move \"%s\" in .mat file.\n", sz );
	fWarned = TRUE;
    }
}

static void ImportGame( FILE *pf, int iGame, int nLength ) {

    char sz[ 128 ], sz0[ 32 ], sz1[ 32 ], *pch, *pchLeft, *pchRight = NULL;
    int n0, n1;
    moverecord *pmr;
    
    if( fscanf( pf, " %31[^:]:%d %31[^:]:%d%*[ ]", sz0, &n0, sz1, &n1 ) < 4 )
	return;

    if( nLength && ( n0 >= nLength || n1 >= nLength ) )
	/* match is already over -- ignore extra games */
	return;
    
    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    for( pch = sz0; *pch; pch++ )
	;
    for( pch--; pch >= sz0 && isspace( *pch ); )
	*pch-- = 0;
    for( ; pch >= sz0; pch-- )
	if( isspace( *pch ) )
	    *pch = '_';
    strcpy( ap[ 0 ].szName, sz0 );
    
    for( pch = sz1; *pch; pch++ )
	;
    for( pch--; pch >= sz1 && isspace( *pch ); )
	*pch-- = 0;
    for( ; pch >= sz1; pch-- )
	if( isspace( *pch ) )
	    *pch = '_';
    strcpy( ap[ 1 ].szName, sz1 );
    
    pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.sz = NULL;
    pmr->g.i = iGame;
    pmr->g.nMatch = nLength;
    pmr->g.anScore[ 0 ] = n0;
    pmr->g.anScore[ 1 ] = n1;
    pmr->g.fCrawford = TRUE; /* assume JF always uses Crawford rule */
    if( ( pmr->g.fCrawfordGame = !fPostCrawford &&
	  ( n0 == nLength - 1 ) ^ ( n1 == nLength - 1 ) ) )
	fPostCrawford = TRUE;
    pmr->g.fJacoby = FALSE; /* assume JF never uses Jacoby rule */
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
    
    do
	if( !fgets( sz, 128, pf ) ) {
	    sz[ 0 ] = 0;
	    break;
	}
    while( strspn( sz, " \n\r\t" ) == strlen( sz ) );

    do {
	if( ( pch = strpbrk( sz, "\n\r" ) ) )
	    *pch = 0;

	if( ( pchLeft = strchr( sz, ':' ) ) &&
		 ( pchRight = strchr( pchLeft + 1, ':' ) ) &&
		 pchRight > sz + 3 )
	    *( ( pchRight -= 2 ) - 1 ) = 0;
	else if( strlen( sz ) > 15 && ( pchRight = strstr( sz + 15, "  " ) ) )
	    *pchRight++ = 0;
	
	if( ( pchLeft = strchr( sz, ')' ) ) )
	    pchLeft++;
	else
	    pchLeft = sz;

	ParseMatMove( pchLeft, 0 );

	if( pchRight )
	    ParseMatMove( pchRight, 1 );
	
	if( !fgets( sz, 128, pf ) )
	    break;
    } while( strspn( sz, " \n\r\t" ) != strlen( sz ) );

    AddGame( pmr );
}

extern void ImportMat( FILE *pf, char *szFilename ) {

    int n, nLength, i;
    char ch;

    fWarned = fPostCrawford = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "%d %*1[Pp]oint %*1[Mm]atch%c", &nLength,
			  &ch ) ) == EOF ) {
	    fprintf( stderr, "%s: not a valid .mat file\n", szFilename );
	    return;
	} else if( n > 1 )
	    break;

	/* discard line */
	while( ( n = getc( pf ) ) != '\n' && n != EOF )
	    ;
    }
    
    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( "Are you sure you want to import a saved match, "
			 "and discard the game in progress? " ) )
	    return;
    }

#if USE_GTK
    if( fX )
	GTKFreeze();
#endif
    
    FreeMatch();
    ClearMatch();

    while( 1 ) {
	if( ( n = fscanf( pf, " Game %d\n", &i ) ) == 1 )
	    ImportGame( pf, i - 1, nLength );
	else if( n == EOF )
	    break;
	else
	    /* discard line */
	    while( ( n = getc( pf ) ) != '\n' && n != EOF )
		;
    }

    UpdateSettings();
    
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}

static void ParseOldmove( char *sz, int fInvert ) {

    int iPlayer, i, c;
    moverecord *pmr;
    char *pch;
    int anMoveLocal[8];
    
    switch( *sz ) {
    case 'X':
	iPlayer = fInvert;
	break;
    case 'O':
	iPlayer = !fInvert;
	break;
    default:
	goto error;
    }
    
    if( sz[ 3 ] == '(' && strlen( sz ) >= 8 ) {
        pmr = malloc( sizeof( pmr->n ) );
        pmr->n.mt = MOVE_NORMAL;
	pmr->n.sz = NULL;
        pmr->n.anRoll[ 0 ] = sz[ 4 ] - '0';
        pmr->n.anRoll[ 1 ] = sz[ 6 ] - '0';
        pmr->n.fPlayer = iPlayer;
	pmr->n.ml.cMoves = 0;
	pmr->n.ml.amMoves = NULL;
        pmr->n.esDouble.et = EVAL_NONE;
        pmr->n.esChequer.et = EVAL_NONE;
	pmr->n.lt = LUCK_NONE;
	pmr->n.rLuck = ERR_VAL;
	pmr->n.st = SKILL_NONE;

	if( !strncasecmp( sz + 9, "can't move", 10 ) )
	    c = 0;
	else {
	    for( pch = sz + 9; *pch; pch++ )
		if( *pch == '-' )
		    *pch = '/';
	    
	    c = ParseMove( sz + 9, pmr->n.anMove );
	}
	
	if( c >= 0 ) {
	    for( i = 0; i < ( c << 1 ); i++ ) {
		pmr->n.anMove[ i ]--;

		if( iPlayer == fInvert && pmr->n.anMove[ i ] >= 0 &&
		    pmr->n.anMove[ i ] <= 23 )
		    pmr->n.anMove[ i ] = 23 - pmr->n.anMove[ i ];
	    }
 
	    if( c < 4 )
		pmr->n.anMove[ c << 1 ] = pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;

            /* We have to order the moves before calling AddMoveRecord
               Think about the opening roll of 6 5, and the legal move
               18/13 24/18 is entered. AddMoveRecord will then silently
               fail in the sub-call to PlayMove(). This problem should
               maybe be fixed in PlayMove (drawboard.c). Opinions? */

            for( i = 0; i < 8 ; i++ )
                anMoveLocal[i] = pmr->n.anMove[i] + 1 ;

            CanonicalMoveOrder( anMoveLocal );

	    for( i = 0; i < 8 ; i++ )
                pmr->n.anMove[i] = anMoveLocal[i] - 1 ;

            /* Now we're ready */

	    AddMoveRecord( pmr );
	} else
	    free( pmr );
	return;
    } else if( !strncasecmp( sz + 3, "doubles", 7 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_DOUBLE;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
	return;
    } else if( !strncasecmp( sz + 3, "accepts", 7 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_TAKE;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
	return;
    } else if( !strncasecmp( sz + 3, "rejects", 7 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_DROP;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
	return;
    } else if( !strncasecmp( sz + 3, "wants to resign", 15 ) ) {
	/* ignore */
	return;
    } else if( !strncasecmp( sz + 3, "wins", 4 ) ) {
	if( ms.gs == GAME_PLAYING ) {
	    /* Neither a drop nor a bearoff to win, so we presume the loser
	       resigned. */
	    pmr = malloc( sizeof( pmr->r ) );
	    pmr->r.mt = MOVE_RESIGN;
            pmr->r.esResign.et = EVAL_NONE;
	    pmr->r.sz = NULL;
	    pmr->r.fPlayer = !iPlayer;
	    pmr->r.nResigned = 1; /* FIXME determine from score */
	    AddMoveRecord( pmr );
	}
	return;
    }

 error:
    if( !fWarned ) {
	outputf( "Unrecognised move \"%s\" in oldmoves file.\n", sz );
	fWarned = TRUE;
    }
}

static void ImportOldmovesGame( FILE *pf, int iGame, int nLength, int n0,
				int n1 ) {

    char sz[ 80 ], sz0[ 32 ], sz1[ 32 ], *pch;
    moverecord *pmr;
    int fInvert;
    static int anExpectedScore[ 2 ];
    
    if( fscanf( pf, " %31s is X - %31s is O\n", sz0, sz1 ) < 2 )
	return;

    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    if( !iGame ) {
	strcpy( ap[ 0 ].szName, sz0 );
	strcpy( ap[ 1 ].szName, sz1 );
	fInvert = FALSE;
    } else
	fInvert = strcmp( ap[ 0 ].szName, sz0 ) != 0;
    
    pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.sz = NULL;
    pmr->g.i = iGame;
    pmr->g.nMatch = nLength;
    pmr->g.anScore[ fInvert ] = n0;
    pmr->g.anScore[ ! fInvert ] = n1;

    /* check if score is swapped (due to fibs oldmoves bug) */

    if ( iGame && pmr->g.anScore[ 0 ] == anExpectedScore[ 1 ] &&
         pmr->g.anScore[ 1 ] == anExpectedScore[ 0 ] ) {

      pmr->g.anScore[ 0 ] = anExpectedScore[ 0 ];
      pmr->g.anScore[ 1 ] = anExpectedScore[ 1 ];

    }
    

    pmr->g.fCrawford = nLength != 0; /* assume matches use Crawford rule */
    if( ( pmr->g.fCrawfordGame = !fPostCrawford &&
	  ( n0 == nLength - 1 ) ^ ( n1 == nLength - 1 ) ) )
	fPostCrawford = TRUE;
    pmr->g.fJacoby = !nLength; /* assume matches never use Jacoby rule */
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
    
    do
	if( !fgets( sz, 80, pf ) ) {
	    sz[ 0 ] = 0;
	    break;
	}
    while( strspn( sz, " \n\r\t" ) == strlen( sz ) );

    do {
        /* FIXME This requires that the games are separated with a
                 line shift */
	if( ( pch = strpbrk( sz, "\n\r" ) ) )
	    *pch = 0;

	ParseOldmove( sz, fInvert );
	
	if( !fgets( sz, 80, pf ) )
	    break;
    } while( strspn( sz, " \n\r\t" ) != strlen( sz ) );


    anExpectedScore[ 0 ] = pmr->g.anScore[ 0 ];
    anExpectedScore[ 1 ] = pmr->g.anScore[ 1 ];
    anExpectedScore[ pmr->g.fWinner ] += pmr->g.nPoints;

    AddGame( pmr );
}

extern void ImportOldmoves( FILE *pf, char *szFilename ) {

    int n, n0, n1, nLength, i;

    fWarned = fPostCrawford = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "Score is %d-%d in a %d", &n0, &n1,
			  &nLength ) ) == EOF ) {
	    fprintf( stderr, "%s: not a valid oldmoves file\n", szFilename );
	    return;
	} else if( n == 2 ) {
	    /* assume a money game */
	    nLength = 0;
	    break;
	} else if( n == 3 )
	    break;
	
	/* discard line */
	while( ( n = getc( pf ) ) != '\n' && n != EOF )
	    ;
    }
    
    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	
	if( !GetInputYN( "Are you sure you want to import a saved match, "
			 "and discard the game in progress? " ) )
	    return;
    }

#if USE_GTK
    if( fX )
	GTKFreeze();
#endif
    
    FreeMatch();
    ClearMatch();

    i = 0;
    
    while( 1 ) {
	/* discard line */
	while( ( n = getc( pf ) ) != '\n' && n != EOF )
	    ;
	
	ImportOldmovesGame( pf, i++, nLength, n0, n1 );

	do {
	    if( ( n = fscanf( pf, "Score is %d-%d in a %d", &n0, &n1,
			      &nLength ) ) >= 2 )
		break;

	    /* discard line */
	    while( ( n = getc( pf ) ) != '\n' && n != EOF )
		;
	} while( n != EOF );

	if( n == EOF )
	    break;
    }

    UpdateSettings();
    
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}

static void ImportSGGGame( FILE *pf, int i, int nLength, int n0, int n1,
			   int fCrawford ) {

    char sz[ 1024 ];
    char *pch;
    int c, fPlayer = 0, anRoll[ 2 ];
    moverecord *pmgi, *pmr;
    
    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    pmgi = malloc( sizeof( movegameinfo ) );
    pmgi->g.mt = MOVE_GAMEINFO;
    pmgi->g.sz = NULL;
    pmgi->g.i = i - 1;
    pmgi->g.nMatch = nLength;
    pmgi->g.anScore[ 0 ] = n0;
    pmgi->g.anScore[ 1 ] = n1;
    pmgi->g.fCrawford = nLength != 0; /* FIXME */
    pmgi->g.fCrawfordGame = fCrawford;
    pmgi->g.fJacoby = !nLength; /* FIXME */
    pmgi->g.fWinner = -1;
    pmgi->g.nPoints = 0;
    pmgi->g.fResigned = FALSE;
    pmgi->g.nAutoDoubles = 0; /* FIXME */
    IniStatcontext( &pmgi->g.sc );
    AddMoveRecord( pmgi );

    anRoll[ 0 ] = 0;
    
    while( fgets( sz, 1024, pf ) ) {
	/* check for game over */
	for( pch = sz; *pch; pch++ )
	    if( !strncmp( pch, "  wins ", 7 ) )
		goto finished;
	    else if( *pch == ':' || *pch == ' ' )
		break;

	if( isdigit( *sz ) ) {
	    for( pch = sz; isdigit( *pch ); pch++ )
		;

	    if( *pch++ == '\t' ) {
		/* we have a move! */
		if( anRoll[ 0 ] ) {
		    pmr = malloc( sizeof( pmr->n ) );
		    pmr->n.mt = MOVE_NORMAL;
		    pmr->n.sz = NULL;
		    pmr->n.anRoll[ 0 ] = anRoll[ 0 ];
		    pmr->n.anRoll[ 1 ] = anRoll[ 1 ];
		    pmr->n.fPlayer = fPlayer;
		    pmr->n.ml.cMoves = 0;
		    pmr->n.ml.amMoves = NULL;
		    pmr->n.esDouble.et = EVAL_NONE;
		    pmr->n.esChequer.et = EVAL_NONE;
		    pmr->n.lt = LUCK_NONE;
		    pmr->n.rLuck = ERR_VAL;
		    pmr->n.st = SKILL_NONE;
		    
		    if( ( c = ParseMove( pch + 4, pmr->n.anMove ) ) >= 0 ) {
			for( i = 0; i < ( c << 1 ); i++ )
			    pmr->n.anMove[ i ]--;
			if( c < 4 )
			    pmr->n.anMove[ c << 1 ] =
				pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;
			
			AddMoveRecord( pmr );
		    } else
			free( pmr );
		    
		    anRoll[ 0 ] = 0;
		} else {
		    if( ( fPlayer = *pch == '\t' ) )
			pch++;

		    if( *pch >= '1' && *pch <= '6' && pch[ 1 ] >= '1' &&
			pch[ 1 ] <= '6' ) {
			/* dice roll */
			anRoll[ 0 ] = *pch - '0';
			anRoll[ 1 ] = pch[ 1 ] - '0';

			if( strstr( pch, "O-O" ) ) {
			    pmr = malloc( sizeof( pmr->n ) );
			    pmr->n.mt = MOVE_NORMAL;
			    pmr->n.sz = NULL;
			    pmr->n.anRoll[ 0 ] = anRoll[ 0 ];
			    pmr->n.anRoll[ 1 ] = anRoll[ 1 ];
			    pmr->n.fPlayer = fPlayer;
			    pmr->n.ml.cMoves = 0;
			    pmr->n.ml.amMoves = NULL;
			    pmr->n.anMove[ 0 ] = -1;
			    pmr->n.esDouble.et = EVAL_NONE;
			    pmr->n.esChequer.et = EVAL_NONE;
			    pmr->n.lt = LUCK_NONE;
			    pmr->n.rLuck = ERR_VAL;
			    pmr->n.st = SKILL_NONE;
			    AddMoveRecord( pmr );
			    anRoll[ 0 ] = 0;
			} else {
			    for( pch += 3; *pch; pch++ )
				if( !isspace( *pch ) )
				    break;
			    
			    if( *pch ) {
				/* Apparently SGG files can contain spurious
				   duplicate moves -- the only thing we can
				   do is ignore them. */
				anRoll[ 0 ] = 0;
				continue;
			    }
			
			    pmr = malloc( sizeof( pmr->sd ) );
			    pmr->sd.mt = MOVE_SETDICE;
			    pmr->sd.sz = NULL;
			    pmr->sd.fPlayer = fPlayer;
			    pmr->sd.anDice[ 0 ] = anRoll[ 0 ];
			    pmr->sd.anDice[ 1 ] = anRoll[ 1 ];
			    pmr->sd.lt = LUCK_NONE;
			    pmr->sd.rLuck = ERR_VAL;
			    AddMoveRecord( pmr );
			}
		    } else {
			if( !strncasecmp( pch, "double", 6 ) ) {
			    if( ms.fDoubled && ms.fTurn != fPlayer )
				/* Presumably a duplicated move in the
				   SGG file -- ignore */
				continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DOUBLE;
			    pmr->d.sz = NULL;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;
			    AddMoveRecord( pmr );
			} else if( !strncasecmp( pch, "accept", 6 ) ) {
			    if( !ms.fDoubled )
				continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_TAKE;
			    pmr->d.sz = NULL;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;
			    AddMoveRecord( pmr );
			} else if( !strncasecmp( pch, "pass", 4 ) ) {
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DROP;
			    pmr->d.sz = NULL;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;
			    AddMoveRecord( pmr );
			}
		    }
		}
	    }
	}
    }

 finished:
    AddGame( pmgi );
}

static int ParseSGGGame( char *pch, int *pi, int *pn0, int *pn1,
			 int *pfCrawford, int *pnLength ) {

    *pfCrawford = FALSE;
    
    if( strncmp( pch, "Game ", 5 ) )
	return -1;

    pch += 5;

    *pi = strtol( pch, &pch, 10 );

    if( *pch != '.' )
	return -1;

    pch++;

    *pn0 = strtol( pch, &pch, 10 );
    
    if( *pch == '*' ) {
	pch++;
	*pfCrawford = TRUE;
    }

    if( *pch != '-' )
	return -1;

    pch++;
    
    *pn1 = strtol( pch, &pch, 10 );
    
    if( *pch == '*' ) {
	pch++;
	*pfCrawford = TRUE;
    }

    if( *pch != '/' ) {
	/* assume money session */
	*pnLength = 0;
	return 0;
    }
    
    pch++;
    
    *pnLength = strtol( pch, &pch, 10 );

    return 0;
}

extern void ImportSGG( FILE *pf, char *szFilename ) {

    char sz[ 80 ], sz0[ 32 ], sz1[ 32 ];
    int n, n0, n1, nLength, i, fCrawford;

    fWarned = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "%32s vs. %32s\n", sz0, sz1 ) ) == EOF ) {
	    fprintf( stderr, "%s: not a valid SGG file\n", szFilename );
	    return;
	} else if( n == 2 )
	    break;
	
	/* discard line */
	while( ( n = getc( pf ) ) != '\n' && n != EOF )
	    ;
    }
    
    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	
	if( !GetInputYN( "Are you sure you want to import a saved match, "
			 "and discard the game in progress? " ) )
	    return;
    }

#if USE_GTK
    if( fX )
	GTKFreeze();
#endif
    
    FreeMatch();
    ClearMatch();

    strcpy( ap[ 0 ].szName, sz0 );
    strcpy( ap[ 1 ].szName, sz1 );
    
    while( fgets( sz, 80, pf ) ) {
	if( !ParseSGGGame( sz, &i, &n0, &n1, &fCrawford, &nLength ) )
	    break;

	/* FIXME check for options -- Jacoby, Crawford, etc. */
    }
    
    while( !feof( pf ) ) {
	ImportSGGGame( pf, i, nLength, n0, n1, fCrawford );
	
	while( fgets( sz, 80, pf ) )
	    if( !ParseSGGGame( sz, &i, &n0, &n1, &fCrawford, &nLength ) )
		break;
    }
	
    UpdateSettings();
    
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}
