/*
 * import.c
 *
 * by Øystein Johansen, 2000
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
#include <string.h>
#include "backgammon.h"
#include "drawboard.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "import.h"

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
 
  return;

}  

static int fWarned;

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
	pmr->n.lt = LUCK_NONE;
	pmr->n.rLuck = -HUGE_VALF;
	pmr->n.st = SKILL_NONE;
	
	if( ( c = ParseMove( sz + 3, pmr->n.anMove ) ) >= 0 ) {
	    for( i = 0; i < ( c << 1 ); i++ )
		pmr->n.anMove[ i ]--;
	    if( c < 4 )
		pmr->n.anMove[ c << 1 ] = pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;
	    
	    AddMoveRecord( pmr );
	}
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
	/* FIXME game over -- update the gameinfo record */
	/* if there was no double/drop and no bear off to win, presume
	   a resignation */
    } else if( !fWarned ) {
	outputf( "Unrecognised move \"%s\" in .mat file.\n", sz );
	fWarned = TRUE;
    }
}

static void ImportGame( FILE *pf, int iGame, int nLength ) {

    char sz[ 80 ], sz0[ 32 ], sz1[ 32 ], *pch, *pchLeft, *pchRight;
    int n0, n1;
    moverecord *pmr;
    
    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    fscanf( pf, " %31[^:]:%d %31[^:]:%d%*[ ]", sz0, &n0, sz1, &n1 );

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
    pmr->g.fCrawford = TRUE; /* FIXME assume JF always uses Crawford rule? */
    pmr->g.fCrawfordGame = FALSE; /* FIXME how does JF mark this? */
    pmr->g.fJacoby = FALSE; /* FIXME assume JF never uses Jacoby rule? */
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
    
    do
	fgets( sz, 80, pf );
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
	
	if( !fgets( sz, 80, pf ) )
	    break;
    } while( strspn( sz, " \n\r\t" ) != strlen( sz ) );

    AddGame( pmr );
}

extern void ImportMat( FILE *pf, char *szFilename ) {

    int n, nLength, i;
    char ch;

    fWarned = FALSE;
    
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
    
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}
