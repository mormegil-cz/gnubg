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
 * $Id$
 */

#include "config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "backgammon.h"
#include "drawboard.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "import.h"
#include "positionid.h"
#include "i18n.h"

static int
IsValidMove ( int anBoard[ 2 ][ 25 ], const int anMove[ 8 ] ) {

  int anBoardTemp[ 2 ][ 25 ];
  int anMoveTemp[ 8 ];

  memcpy ( anBoardTemp, anBoard, 2 * 25 * sizeof ( int ) );
  memcpy ( anMoveTemp, anMove, 8 * sizeof ( int ) );

  if ( ! ApplyMove ( anBoardTemp, anMoveTemp, TRUE ) )
    return 1;
  else
    return 0;

}


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
    outputl(_("File not recognised as Jellyfish file."));
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


static void
ExpandMatMove ( int anBoard[ 2 ][ 25 ], int anMove[ 8 ], int *pc,
             const int anDice[ 2 ] ) {

  int i, j, k;
  int c = *pc;

  if ( anDice[ 0 ] != anDice[ 1 ] ) {
    
    if ( ( anMove[ 0 ] - anMove[ 1 ] ) == ( anDice[ 0 ] + anDice[ 1 ] ) ) {

      int an[ 8 ];

      /* consolidated move */

      for ( i = 0; i < 2; ++i ) {

        an[ 0 ] = anMove[ 0 ];
        an[ 1 ] = an[ 0 ] - anDice[ i ];
        
        an[ 2 ] = an[ 1 ];
        an[ 3 ] = an[ 2 ] - anDice[ !i ];

        an[ 4 ] = -1;
        an[ 5 ] = -1;

        if ( IsValidMove ( anBoard, an ) ) {
          memcpy ( anMove, an, sizeof ( an ) );
          ++*pc;
          break;
        }

      }

    }

  }
  else {
  
    for ( i = 0; i < c; ++i ) {

      /* if this is a consolidated move then "expand" it */

      j = ( anMove[ 2 * i ] - anMove[ 2 * i + 1 ] ) / anDice[ 0 ];

      for ( k = 1; k < j; ++k ) {

        /* new move */

        anMove[ 2 * *pc ] = anMove[ 2 * *pc +1 ] = anMove[ 2 * i ];
        anMove[ 2 * *pc + 1 ] -= anDice[ 0 ];

        /* fix old move */

        anMove[ 2 * i ] -= anDice[ 0 ];

        ++*pc;

        assert ( *pc <= 4 );

        /* terminator */

        if ( *pc < 4 ) {
          anMove [ 2 * *pc ] = -1;
          anMove [ 2 * *pc + 1 ] = -1;
        }

      }

    }

  }

  if ( c != *pc )
    /* reorder moves */
    CanonicalMoveOrder ( anMove );

}

static void ParseMatMove( char *sz, int iPlayer ) {

    char *pch;
    moverecord *pmr;
    int i, c;
    static int fBeaver = FALSE;
    
    sz += strspn( sz, " \t" );

    if( !*sz )
	return;

    for( pch = strchr( sz, 0 ) - 1;
	 pch >= sz && ( *pch == ' ' || *pch == '\t' ); pch-- )
	*pch = 0;
    
    if( sz[ 0 ] >= '1' && sz[ 0 ] <= '6' && sz[ 1 ] >= '1' && sz[ 1 ] <= '6' &&
	sz[ 2 ] == ':' ) {

        if ( fBeaver ) {
          /* look likes the previous beaver was taken */
          pmr = malloc( sizeof( pmr->d ) );
          pmr->d.mt = MOVE_TAKE;
          pmr->d.sz = NULL;
          pmr->d.fPlayer = iPlayer;
          pmr->d.esDouble.et = EVAL_NONE;
          pmr->d.st = SKILL_NONE;
          AddMoveRecord( pmr );
        }
        fBeaver = FALSE;

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
	pmr->n.stMove = SKILL_NONE;
	pmr->n.stCube = SKILL_NONE;
	
	if( ( c = ParseMove( sz + 3, pmr->n.anMove ) ) >= 0 ) {
	    for( i = 0; i < ( c << 1 ); i++ )
		pmr->n.anMove[ i ]--;
	    if( c < 4 )
		pmr->n.anMove[ c << 1 ] = pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;

            /* remove consolidation */

            ExpandMatMove ( ms.anBoard, pmr->n.anMove, &c, 
                         pmr->n.anRoll );

            /* check if move is valid */

            if ( ! IsValidMove ( ms.anBoard, pmr->n.anMove ) )
              outputf ( _("WARNING: Invalid move: \"%s\" encountered\n"),
                        sz + 3 );
	    
	    AddMoveRecord( pmr );

	    return;
	} else {
	    /* roll but no move found; check for a resignation */
	    int anDice[ 2 ] = { pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] };
	    
	    free( pmr );
	    sz += 3;
	    sz += strspn( sz, " \t" );
	    
	    if( !strncasecmp( sz, "win", 3 ) ) {
		/* the player not on roll resigned -- record the roll */
		pmr = malloc( sizeof( pmr->sd ) );
		pmr->sd.mt = MOVE_SETDICE;
		pmr->sd.sz = NULL;
		pmr->sd.fPlayer = iPlayer;
		pmr->sd.anDice[ 0 ] = anDice[ 0 ];
		pmr->sd.anDice[ 1 ] = anDice[ 1 ];
		pmr->sd.lt = LUCK_NONE;
		pmr->sd.rLuck = ERR_VAL;
		AddMoveRecord( pmr );
		/* and now fall through and handle the resignation below */
	    } else
		return;
	}
    }
	
    if( !strncasecmp( sz, "double", 6 ) || 
        !strncasecmp( sz, "beavers", 7 ) ||
        !strncasecmp( sz, "raccoons", 7 ) ) {
	pmr = malloc( sizeof( pmr->d ) );
	pmr->d.mt = MOVE_DOUBLE;
	pmr->d.sz = NULL;
	pmr->d.fPlayer = iPlayer;
	pmr->d.esDouble.et = EVAL_NONE;
	pmr->d.st = SKILL_NONE;
	AddMoveRecord( pmr );
        fBeaver = !strncasecmp( sz, "beavers", 6 );
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
	outputf( _("Unrecognised move \"%s\" in .mat file.\n"), sz );
	fWarned = TRUE;
    }
}

static int 
ImportGame( FILE *pf, int iGame, int nLength ) {

    char sz[ 128 ], sz0[ 32 ], sz1[ 32 ], *pch, *pchLeft, *pchRight = NULL;
    int n0, n1;
    moverecord *pmr;
    
    if( fscanf( pf, " %31[^:]:%d %31[^:]:%d%*[ ]", sz0, &n0, sz1, &n1 ) < 4 )
        return 0;

    if( nLength && ( n0 >= nLength || n1 >= nLength ) )
	/* match is already over -- ignore extra games */
	return 1;
    
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

        pchRight = pchLeft = NULL;

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

    return ( ms.nMatchTo && 
             ( ms.anScore[ 0 ] >= ms.nMatchTo || 
               ms.anScore[ 1 ] >= ms.nMatchTo ) );

}

extern void ImportMat( FILE *pf, char *szFilename ) {

    int n, nLength, i;
    char ch;

    fWarned = fPostCrawford = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "%d %*1[Pp]oint %*1[Mm]atch%c", &nLength,
			  &ch ) ) == EOF ) {
	    outputerrf( _("%s: not a valid .mat file"), szFilename );
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
	    
	if( !GetInputYN( _("Are you sure you want to import a saved match, "
			 "and discard the game in progress? ") ) )
	    return;
    }

#if USE_GTK
    if( fX )
	GTKFreeze();
#endif
    
    FreeMatch();
    ClearMatch();

    while( 1 ) {
        if( ( n = fscanf( pf, " Game %d\n", &i ) ) == 1 ) {
          if ( ImportGame( pf, i - 1, nLength ) ) 
            /* match is over; avoid multiple matches in same .mat file */
            break;
        }
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
	pmr->n.stCube = SKILL_NONE;
	pmr->n.stMove = SKILL_NONE;

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
	outputf( _("Unrecognised move \"%s\" in oldmoves file.\n"), sz );
	fWarned = TRUE;
    }
}

#if 0

static int
IncreasingScore ( const int anScore[ 2 ], const int n0, const int n1 ) {

  /* check for decreasing score */
  
  if ( n0 == anScore[ 0 ] ) {
    
    /* n0 is unchanged, n1 must be increasing */

    if ( n1 <= anScore[ 1 ] )
      /* score is not increasing */
      return 0;
    
  }
  else if ( n1 == anScore[ 0 ] ) {
    
    if ( n0 <= anScore[ 1 ] )
      /* score is not increasing */
      return 0;
    
  }
  else if ( n0 == anScore[ 1 ] ) {
    
    if ( n1 <= anScore[ 0 ] )
      /* score is not increasing */
      return 0;
    
  } 
  else if ( n1 == anScore[ 1 ] ) {
    
    if ( n0 <= anScore[ 0 ] )
      /* score is not increasing */
      return 0;
    
  }
  else {
    /* most likely a new game */
    return 0;
  }
  
  return 1;

}

#endif

static int
NewPlayers ( const char* szOld0, const char* szOld1,
             const char *szNew0, const char* szNew1 ) {


  if ( ! strcmp ( szOld0, szNew0 ) )
    /* player 0 have not changed */
    return strcmp ( szOld1, szNew1 );
  else if ( ! strcmp ( szOld0, szNew1 ) )
    /* swap of player1 and player 0 */
    return strcmp ( szOld1, szNew0 );
  else 
    /* definitely a new player */
    return 1;

}


static int
ImportOldmovesGame( FILE *pf, int iGame, int nLength, int n0,
                    int n1 ) {

    char sz[ 80 ], sz0[ 32 ], sz1[ 32 ], *pch;
    moverecord *pmr;
    int fInvert;
    static int anExpectedScore[ 2 ];

    /* FIXME's:
     *
     * what about multiple matches?
     * pmr->r.nResigned is not correct in ParseOldMove
     *
     * Possible solution: read through file one time and collect match scores.
     * If the score is decreasing or the players's names changes stop.
     *
     * Since we now know the score after every game it's easy to 
     * have pmr->r.nResigned correct. 
     *
     */
    
    if( fscanf( pf, " %31s is X - %31s is O\n", sz0, sz1 ) < 2 )
	return 0;

    /* consistency checks */

    if ( iGame ) {

      if ( NewPlayers ( ap[ 0 ].szName, ap[ 1 ].szName, sz0, sz1 ) ) 
        return 1;

      if ( ms.nMatchTo != nLength )
        /* match length have changed */
        return 1;

#if 0
      if ( ! IncreasingScore ( ms.anScore, n0, n1 ) ) {
        printf ( "not increasing score %d %d %d %d\n",
                 ms.anScore[ 0 ], ms.anScore[ 1 ], n0, n1 );
        return 1;
      }
#endif

    }

    /* initialise */

    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    if( !iGame ) {
	strcpy( ap[ 0 ].szName, sz0 );
	strcpy( ap[ 1 ].szName, sz1 );
	fInvert = FALSE;
    } else
      fInvert = strcmp( ap[ 0 ].szName, sz0 ) != 0;

    /* add game info */


    pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.sz = NULL;
    pmr->g.i = iGame;
    pmr->g.nMatch = nLength;
    pmr->g.anScore[ fInvert ] = n0;
    pmr->g.anScore[ ! fInvert ] = n1;

    /* check if score is swapped (due to fibs oldmoves bug) */

    if ( iGame && 
         ( pmr->g.anScore[ 0 ] == anExpectedScore[ 1 ] ||
           pmr->g.anScore[ 1 ] == anExpectedScore[ 0 ] ) ) {

      int n = pmr->g.anScore[ 0 ];
      pmr->g.anScore[ 0 ] = pmr->g.anScore[ 1 ];
      pmr->g.anScore[ 1 ] = n;

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

	if( ( pch = strpbrk( sz, "\n\r" ) ) )
	    *pch = 0;

	ParseOldmove( sz, fInvert );

        if ( ms.gs != GAME_PLAYING )
          break;
	
	if( !fgets( sz, 80, pf ) )
	    break;
    } while( strspn( sz, " \n\r\t" ) != strlen( sz ) );


    anExpectedScore[ 0 ] = pmr->g.anScore[ 0 ];
    anExpectedScore[ 1 ] = pmr->g.anScore[ 1 ];
    anExpectedScore[ pmr->g.fWinner ] += pmr->g.nPoints;

    AddGame( pmr );

    return 0;

}


extern void ImportOldmoves( FILE *pf, char *szFilename ) {

    int n, n0, n1, nLength, i;

    fWarned = fPostCrawford = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "Score is %d-%d in a %d", &n0, &n1,
			  &nLength ) ) == EOF ) {
	    outputerrf( _("%s: not a valid oldmoves file"), szFilename );
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
	
	if( !GetInputYN( _("Are you sure you want to import a saved match, "
			 "and discard the game in progress? ") ) )
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
	
        if ( ImportOldmovesGame( pf, i++, nLength, n0, n1 ) )
          /* new match */
          break;

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
			   int fCrawford,
                           int fCrawfordRule, int fAutoDoubles, 
                           int fJacobyRule ) {


    char sz[ 1024 ];
    char *pch;
    int c, fPlayer = 0, anRoll[ 2 ];
    moverecord *pmgi, *pmr;
    char *szComment = NULL;
    int fBeaver = FALSE;

    int fPlayerOld, nMoveOld; 
    int nMove = -1;
    
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
    pmgi->g.fCrawford = fCrawfordRule && nLength;
    pmgi->g.fCrawfordGame = fCrawford;
    pmgi->g.fJacoby = fJacobyRule && ! nLength;
    pmgi->g.fWinner = -1;
    pmgi->g.nPoints = 0;
    pmgi->g.fResigned = FALSE;
    pmgi->g.nAutoDoubles = 0; /* we check for automatic doubles later */
    IniStatcontext( &pmgi->g.sc );
    AddMoveRecord( pmgi );

    anRoll[ 0 ] = 0;
    
    while( fgets( sz, 1024, pf ) ) {

        nMoveOld = nMove;
        fPlayerOld = fPlayer;

	/* check for game over */
	for( pch = sz; *pch; pch++ )
	    if( !strncmp( pch, "  wins ", 7 ) )
		goto finished;
	    else if( *pch == ':' || *pch == ' ' )
		break;

	if( isdigit( *sz ) ) {
            nMove = atoi ( sz );
	    for( pch = sz; isdigit( *pch ); pch++ )
		;

	    if( *pch++ == '\t' ) {
		/* we have a move! */
		if( anRoll[ 0 ] ) {

                    /* check for
                     * 1	43:  
                     * 1	43:             <---- check for this line
                     * 1	43: 13/9, 13/10 
                     */

                    if ( *(pch+4) == '\n' || !*(pch+4) )
                       continue;

                    /* when beavered there is no explicit "take" */
                    if ( fBeaver ) {
                      pmr = malloc( sizeof( pmr->d ) );
                      pmr->d.mt = MOVE_TAKE;
                      pmr->d.sz = szComment;
                      pmr->d.fPlayer = fPlayer;
                      pmr->d.esDouble.et = EVAL_NONE;
                      pmr->d.st = SKILL_NONE;
                      
                      AddMoveRecord( pmr );
                      szComment = NULL;
                      fBeaver = FALSE;
                    }
                
		    pmr = malloc( sizeof( pmr->n ) );
		    pmr->n.mt = MOVE_NORMAL;
		    pmr->n.sz = szComment;
		    pmr->n.anRoll[ 0 ] = anRoll[ 0 ];
		    pmr->n.anRoll[ 1 ] = anRoll[ 1 ];
		    pmr->n.fPlayer = fPlayer;
		    pmr->n.ml.cMoves = 0;
		    pmr->n.ml.amMoves = NULL;
		    pmr->n.esDouble.et = EVAL_NONE;
		    pmr->n.esChequer.et = EVAL_NONE;
		    pmr->n.lt = LUCK_NONE;
		    pmr->n.rLuck = ERR_VAL;
		    pmr->n.stCube = SKILL_NONE;
		    pmr->n.stMove = SKILL_NONE;
		    
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
                    szComment = NULL;

		} else {
		    if( ( fPlayer = *pch == '\t' ) )
			pch++;

		    if( *pch >= '1' && *pch <= '6' && pch[ 1 ] >= '1' &&
			pch[ 1 ] <= '6' ) {
			/* dice roll */
			anRoll[ 0 ] = *pch - '0';
			anRoll[ 1 ] = pch[ 1 ] - '0';

			if( strstr( pch, "O-O" ) ) {

                            if ( nMove == nMoveOld && fPlayerOld == fPlayer ) {
                              /* handle duplicate dances:
                                 24	62: O-O
                                 24	62: O-O
                              */
                              anRoll[ 0 ] = 0;
                              continue;
                            }



                            /* when beavered there is no explicit "take" */
                            if ( fBeaver ) {
                              pmr = malloc( sizeof( pmr->d ) );
                              pmr->d.mt = MOVE_TAKE;
                              pmr->d.sz = szComment;
                              pmr->d.fPlayer = fPlayer;
                              pmr->d.esDouble.et = EVAL_NONE;
                              pmr->d.st = SKILL_NONE;
                              
                              AddMoveRecord( pmr );
                              szComment = NULL;
                              fBeaver = FALSE;
                            }
                            
                            pmr = malloc( sizeof( pmr->n ) );
                            pmr->n.mt = MOVE_NORMAL;
                            pmr->n.sz = szComment;
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
			    pmr->n.stMove = SKILL_NONE;
			    pmr->n.stCube = SKILL_NONE;
			    AddMoveRecord( pmr );

			    anRoll[ 0 ] = 0;
                            szComment = NULL;
                            fBeaver = FALSE;

			} else {
			    for( pch += 3; *pch; pch++ )
				if( !isspace( *pch ) )
				    break;
			    
			    if( *pch ) {
				/* Apparently SGG files can contain spurious
				   duplicate moves -- the only thing we can
				   do is ignore them. */

                                /* this catches:
                                   1	43: 
                                   1	43: 13/9, 13/10
                                   1	43: 13/9, 13/10 */

				anRoll[ 0 ] = 0;
				continue;
			    }

                            /* when beavered there is no explicit "take" */
                            if ( fBeaver ) {
                              pmr = malloc( sizeof( pmr->d ) );
                              pmr->d.mt = MOVE_TAKE;
                              pmr->d.sz = szComment;
                              pmr->d.fPlayer = fPlayer;
                              pmr->d.esDouble.et = EVAL_NONE;
                              pmr->d.st = SKILL_NONE;
                              
                              AddMoveRecord( pmr );
                              szComment = NULL;
                              fBeaver = FALSE;
                            }

			    pmr = malloc( sizeof( pmr->sd ) );
			    pmr->sd.mt = MOVE_SETDICE;
                            /* we do not want comments on MOVE_SETDICE */
			    pmr->sd.sz = NULL;
			    pmr->sd.fPlayer = fPlayer;
			    pmr->sd.anDice[ 0 ] = anRoll[ 0 ];
			    pmr->sd.anDice[ 1 ] = anRoll[ 1 ];
			    pmr->sd.lt = LUCK_NONE;
			    pmr->sd.rLuck = ERR_VAL;

			    AddMoveRecord( pmr );
                            fBeaver = FALSE;

			}
		    } else {
			if( !strncasecmp( pch, "double", 6 ) ) {
			    if( ms.fDoubled && ms.fTurn != fPlayer )
				/* Presumably a duplicated move in the
				   SGG file -- ignore */
				continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DOUBLE;
			    pmr->d.sz = szComment;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;

			    AddMoveRecord( pmr );
                            fBeaver = FALSE;
                            szComment = NULL;
                        } else if( !strncasecmp (pch, "beaver", 6 ) ) {
                          /* Beaver to 4 */
                          if ( ms.fDoubled && ms.fTurn != fPlayer )
				/* Presumably a duplicated move in the
				   SGG file -- ignore */
				continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DOUBLE;
			    pmr->d.sz = szComment;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;

			    AddMoveRecord( pmr );
                            szComment = NULL;
                            fBeaver = TRUE;
                        } else if( !strncasecmp (pch, "raccoon", 7 ) ) {
                            /* Raccoon to 8 */
                            if ( ms.fDoubled && ms.fTurn != fPlayer )
                              /* Presumably a duplicated move in the
                                 SGG file -- ignore */
                              continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DOUBLE;
			    pmr->d.sz = szComment;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;

			    AddMoveRecord( pmr );
                            szComment = NULL;
                            fBeaver = TRUE;
			} else if( !strncasecmp( pch, "accept", 6 ) ) {
			    if( !ms.fDoubled )
				continue;
			    
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_TAKE;
			    pmr->d.sz = szComment;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;

			    AddMoveRecord( pmr );
                            szComment = NULL;
                            fBeaver = FALSE;
			} else if( !strncasecmp( pch, "pass", 4 ) ) {
			    pmr = malloc( sizeof( pmr->d ) );
			    pmr->d.mt = MOVE_DROP;
			    pmr->d.sz = szComment;
			    pmr->d.fPlayer = fPlayer;
			    pmr->d.esDouble.et = EVAL_NONE;
			    pmr->d.st = SKILL_NONE;

			    AddMoveRecord( pmr );
                            szComment = NULL;
                            fBeaver = FALSE;
			}
		    }
		}
	    } /* *pch++ == '\t' */
            else {

              /* check for automatic doubles:

                 11: Automatic double
              
              */

              if ( strstr ( pch, "Automatic double" ) ) {
                ++pmgi->g.nAutoDoubles;
                /* we've already called AddMoveRecord for pmgi, 
                   so we manually update the cube value */
                ms.nCube *= 2;
              }


            }
            

	} /* isdigit */
        else {

          /* the text is most likely a comment */

          if ( *sz != '\n' ) {
            /* non-empty line */
            if ( ! szComment )
              szComment = strdup ( sz );
            else {
              szComment = 
                (char *) realloc ( szComment,
                                   strlen ( sz ) + strlen ( szComment ) + 1 );
              strcat ( szComment, sz );
            }

          }

        }
          
    }

 finished:
    AddGame( pmgi );

    /* garbage collect */

    if ( szComment )
      free ( szComment );

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


static void
ParseSGGDate ( const char *sz, int *pnDay, int *pnMonth, int *pnYear ) {

  static char *aszMonths[] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December" };
  int i;
  char szMonth[ 80 ];
  int nDay, nYear;
  int n;
  

  *pnDay = 1;
  *pnMonth = 1;
  *pnYear = 1900;

  if ( ( n = sscanf ( sz, "%*s %s %d, %d", szMonth, &nDay, &nYear ) ) != 3 ) 
    return;

  *pnDay = nDay;
  *pnYear = nYear;

  for ( i = 0; i < 12; ++i ) {
    if ( ! strcmp ( szMonth, aszMonths[ i ] ) ) {
      *pnMonth = i + 1;
      break;
    }
  }

}


static void
ParseSGGOptions ( const char *sz, matchinfo *pmi, int *pfCrawfordRule,
                  int *pfAutoDoubles, int *pfJacobyRule ) {

  char szTemp[ 80 ];
  int i;
  static char *aszOptions[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
    "Saturday", "Sunday",
    "ELO:", "Jacoby:", "Automatic Doubles:", "Crawford:", "Beavers:",
    "Raccoons:", "Doubling Cube:", "Rated:", "Match Length:",
    "Variant:", NULL };
  double arRating[ 2 ];
  int anExp[ 2 ];
  char *pc;
  
  for ( i = 0, pc = aszOptions[ i ]; pc; ++i, pc = aszOptions[ i ] ) 
    if ( ! strncmp ( sz, pc, strlen ( pc ) ) )
      break;
  
  switch ( i ) {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    
    /* date */
    ParseSGGDate ( sz, &pmi->nDay, &pmi->nMonth, &pmi->nYear );
    break;
  case 7:

    /* ratings */
    if ( ( sscanf ( sz, "%*s %lf %d %lf %d", &arRating[ 1 ], &anExp[ 1 ], 
                    &arRating[ 0 ], &anExp[ 0 ] ) ) != 4 )
      break;

    for ( i = 0; i < 2; ++i ) {
      if ( pmi->pchRating[ i ] )
        free ( pmi->pchRating[ i ] );
      sprintf ( szTemp, "%.6g (Exp %d)", arRating[ i ], anExp[ i ] );
      pmi->pchRating[ i ] = strdup ( szTemp );
    }
    break;

  case 8:

    /* Jacoby rule */

    if ( ( sscanf ( sz, "%*s %s", szTemp ) ) != 1 )
      break;
    
    *pfJacobyRule = ! strcmp ( szTemp, "Yes" );
    break;

  case 9:

    /* automatic doubles */

    if ( ( sscanf ( sz, "%*s %s", szTemp ) ) != 1 )
      break;
    
    *pfAutoDoubles = ! strcmp ( szTemp, "Yes" );
    break;

  case 10:

    /* crawford rule */

    if ( ( sscanf ( sz, "%*s %s", szTemp ) ) != 1 )
      break;

    *pfCrawfordRule = ! strcmp ( szTemp, "Yes" );
    break;

  case 11:
  case 12:

    /* beavers & raccons*/

    /* ignored as they are not used internally in gnubg */
    break;

  case 13:

    /* doubling cube */
    break;

  case 14:

    /* rated */
    break;

  case 15:

    /* match length */
    break;

  case 16:
    
    /* variant */
    break;

  default:

    /* most likely "place" */
    if ( pmi->pchPlace )
      free ( pmi->pchPlace );

    pmi->pchPlace = strdup ( sz );
    if ( ( pc = strchr ( sz, '\n' ) ) )
      *pc = 0;

  }

}


extern void ImportSGG( FILE *pf, char *szFilename ) {

    char sz[ 80 ], sz0[ 32 ], sz1[ 32 ];
    int n, n0, n1, nLength, i, fCrawford;
    int fCrawfordRule = TRUE;
    int fJacobyRule = TRUE;
    int fAutoDoubles = 0;

    fWarned = FALSE;
    
    while( 1 ) {
	if( ( n = fscanf( pf, "%32s vs. %32s\n", sz0, sz1 ) ) == EOF ) {
	    outputerrf( _("%s: not a valid SGG file"), szFilename );
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
	
	if( !GetInputYN( _("Are you sure you want to import a saved match, "
			 "and discard the game in progress? ") ) )
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

    while ( fgets ( sz, 80, pf ) ) {
      if( !ParseSGGGame( sz, &i, &n0, &n1, &fCrawford, &nLength ) )
        break;
      
      ParseSGGOptions ( sz, &mi, &fCrawfordRule, 
                        &fAutoDoubles, &fJacobyRule );

    }
    
    while( !feof( pf ) ) {
	ImportSGGGame( pf, i, nLength, n0, n1, fCrawford,
                       fCrawfordRule, fAutoDoubles, fJacobyRule );
	
	while( fgets( sz, 80, pf ) )
	    if( !ParseSGGGame( sz, &i, &n0, &n1, &fCrawford, &nLength ) )
		break;
    }
	
    UpdateSettings();

    /* swap players */

    CommandSwapPlayers ( NULL );
    
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}


/*
 * Parse TMG files 
 *
 */

static int
ParseTMGOptions ( const char *sz, matchinfo *pmi, int *pfCrawfordRule,
                  int *pfAutoDoubles, int *pfJacobyRule,
                  int *pnLength ) {

  char szTemp[ 80 ];
  int i, j;
  static char *aszOptions[] = {
    "MatchID:", "Player1:", "Player2:", "Stake:", "RakePct:",
    "MaxRakeAbs:", "Startdate:", "Jacoby:", "AutoDistrib:", "Beavers:", 
    "Raccoons:", "Crawford:", "Cube:", "MaxCube:", "Length:", "MaxGames:",
    "Variant:", "PlayMoney:", NULL };
  char *pc;
  char szName[ 80 ];

  for ( i = 0, pc = aszOptions[ i ]; pc; ++i, pc = aszOptions[ i ] ) 
    if ( ! strncmp ( sz, pc, strlen ( pc ) ) )
      break;
  
  switch ( i ) {
  case 0: /* MatchID */
    pc = strchr ( sz, ':' ) + 2;
    sprintf( szTemp, _("TMG MatchID: %s"), pc );
    if ( ( pc = strchr ( szTemp, '\n' ) ) )
      *pc = 0;
    pmi->pchComment = strdup ( szTemp );
    return 0;
    break;

  case 1: /* Player1: */
  case 2: /* Player2: */
    if ( sscanf ( sz, "Player%d: %*d %s %*f", &j, szName ) == 2 ) 
      if ( ( j == 1 || j == 2 ) && *szName )
        strcpy ( ap[ j - 1 ].szName, szName );
    return 0;
    break;

  case 3: /* Stake: */
    /* ignore, however, we could read into pmi->szComment, but I guess 
       people do not want to see their stakes in, say, html export */
    break;

  case 4: /* RakePct: */
  case 5: /* MaxRakeAbs: */
  case 8: /* AutoDistrib */
  case 13: /* MaxCube */
  case 15: /* MaxGames */
  case 16: /* Variant */
    /* ignore */
    return 0;
    break;

  case 6: /* Startdate */

    if ( ( sscanf ( sz, "Startdate: %d-%d-%d", 
                    &pmi->nYear, &pmi->nMonth, &pmi->nDay ) ) != 3 ) 
      pmi->nYear = pmi->nMonth = pmi->nDay = 0;
    return 0;
    break;

  case 7: /* Jacoby */
    if ( ! ( pc = strchr( sz, ':' ) ) )
      return 0;
    *pfJacobyRule = atoi ( pc + 2 );
    return 0;
    break;

  case 9: /* Beavers */
    break;

  case 10: /* Raccoons */
    break;

  case 11: /* Crawford */
    /* ignore. Possible FIXME: use it? */
    break;

  case 12: /* Cube */
    break;

  case 14: /* Length */
    if ( ! ( pc = strchr( sz, ':' ) ) )
      return 0;
    *pnLength = atoi ( pc + 2 );
    return 0;
    break;

  case 17: /* PlayMoney */
    break;

  }

  /* code not reachable */

  return -1;

}


static int
ParseTMGGame ( const char *sz,
               int *piGame, int *pn0, int *pn1, int *pfCrawford,
               const int nLength ) {

  int i = sscanf ( sz, "Game %d: %d-%d", piGame, pn0, pn1 ) == 3;

  if ( !i )
    return FALSE;

  if ( nLength ) {
    if ( ! *pfCrawford ) 
      *pfCrawford = ( *pn0 == ( nLength - 1 ) ) || ( *pn1 == ( nLength - 1 ) );
    else
      *pfCrawford = FALSE;
  }

  return TRUE;

}
               
static void ImportTMGGame( FILE *pf, int i, int nLength, int n0, int n1,
			   int fCrawford,
                           int fCrawfordRule, int fAutoDoubles, 
                           int fJacobyRule ) {


    char sz[ 1024 ];
    char *pch;
    int c, fPlayer = 0, anRoll[ 2 ];
    moverecord *pmgi, *pmr;
    int iMove;
    int j;

    typedef enum _tmgrecordtype {
      TMG_ROLL = 1,
      TMG_AUTO_DOUBLE = 2,
      TMG_MOVE = 4,
      TMG_DOUBLE = 5,
      TMG_TAKE = 7,
      TMG_PASS = 10,
      TMG_WIN_SINGLE = 14,
      TMG_WIN_GAMMON = 15,
      TMG_WIN_BACKGAMMON = 16,
      TMG_TABLE_STAKE = 19 } tmgrecordtype;
    tmgrecordtype trt;
    
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
    pmgi->g.fCrawford = fCrawfordRule && nLength;
    pmgi->g.fCrawfordGame = fCrawford;
    pmgi->g.fJacoby = fJacobyRule && ! nLength;
    pmgi->g.fWinner = -1;
    pmgi->g.nPoints = 0;
    pmgi->g.fResigned = FALSE;
    pmgi->g.nAutoDoubles = 0; /* we check for automatic doubles later */
    IniStatcontext( &pmgi->g.sc );
    AddMoveRecord( pmgi );

    anRoll[ 0 ] = 0;
    
    while( fgets( sz, 1024, pf ) ) {

      /* skip white space */

      for ( pch = sz; isspace ( *pch ); ++pch )
        ;

      /* 
       * is it:
       *   -1 4 13/11 24/23
       * or
       * Beavers: 0
       */

      if ( isdigit ( *pch ) || *pch == '-' ) {

        iMove = abs ( atoi ( pch ) ); /* move number */
        fPlayer = ( *pch == '-' );    /* player: +n for player 0,
                                                 -n for player 1 */
        
        /* step over move number */

        for ( ; ! isspace ( *pch ); ++pch )
          ;
        for ( ; isspace ( *pch ); ++pch )
          ;

        /* determine record type */

        trt = (tmgrecordtype) atoi ( pch );

        /* step over record type */

        for ( ; ! isspace ( *pch ); ++pch )
          ;
        for ( ; isspace ( *pch ); ++pch )
          ;

        switch ( trt ) {
        case TMG_ROLL: /* roll:    1 1 21: */
          anRoll[ 0 ] = *pch - '0';
          anRoll[ 1 ] = *(pch+1) - '0';

          pmr = malloc( sizeof( pmr->sd ) );
          pmr->sd.mt = MOVE_SETDICE;
          pmr->sd.sz = NULL;
          pmr->sd.fPlayer = fPlayer;
          pmr->sd.anDice[ 0 ] = anRoll[ 0 ];
          pmr->sd.anDice[ 1 ] = anRoll[ 1 ];
          pmr->sd.lt = LUCK_NONE;
          pmr->sd.rLuck = ERR_VAL;

          AddMoveRecord( pmr );

          break;

        case TMG_AUTO_DOUBLE: /* automatic double:  0 2 Automatic to 2 */

          ++pmgi->g.nAutoDoubles;
          /* we've already called AddMoveRecord for pmgi, 
             so we manually update the cube value */
          ms.nCube *= 2;

          break;

        case TMG_MOVE: /* move:    1 4 24/18 13/10 */

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
          pmr->n.stMove = SKILL_NONE;
          pmr->n.stCube = SKILL_NONE;

          if ( ! strncmp ( pch, "0/0", 3 ) ) {
            /* fans */
            AddMoveRecord ( pmr );
          }
          else if( ( c = ParseMove( pch, pmr->n.anMove ) ) >= 0 ) {
            for( i = 0; i < ( c << 1 ); i++ )
              pmr->n.anMove[ i ]--;
            if( c < 4 )
              pmr->n.anMove[ c << 1 ] =
                pmr->n.anMove[ ( c << 1 ) | 1 ] = -1;
            
            AddMoveRecord( pmr );
          } else
            free( pmr );
		    
          break;

        case TMG_DOUBLE: /* double:   -7 5 Double to 2 */

          pmr = malloc( sizeof( pmr->d ) );
          pmr->d.mt = MOVE_DOUBLE;
          pmr->d.sz = NULL;
          pmr->d.fPlayer = fPlayer;
          pmr->d.esDouble.et = EVAL_NONE;
          pmr->d.st = SKILL_NONE;
          
          AddMoveRecord( pmr );

          break;

        case TMG_TAKE: /* take:   -6 7 Take */

          pmr = malloc( sizeof( pmr->d ) );
          pmr->d.mt = MOVE_TAKE;
          pmr->d.sz = NULL;
          pmr->d.fPlayer = fPlayer;
          pmr->d.esDouble.et = EVAL_NONE;
          pmr->d.st = SKILL_NONE;
          
          AddMoveRecord( pmr );

          break;
                            
        case TMG_PASS: /* pass:    5 10 Pass */

          pmr = malloc( sizeof( pmr->d ) );
          pmr->d.mt = MOVE_DROP;
          pmr->d.sz = NULL;
          pmr->d.fPlayer = fPlayer;
          pmr->d.esDouble.et = EVAL_NONE;
          pmr->d.st = SKILL_NONE;
          
          AddMoveRecord( pmr );

          break;
                            
        case TMG_WIN_SINGLE: /* win single:    4 14 1 thj wins 1 point */
        case TMG_WIN_GAMMON: /* resign:  -30 15 2 Saltyzoo7 wins 2 points */
        case TMG_WIN_BACKGAMMON: /* win backgammon: ??? */

          if ( ! GameStatus ( ms.anBoard ) ) {
            /* game is in progress: the opponent resigned */
            pmr = malloc( sizeof( *pmr ) );
            pmr->r.mt = MOVE_RESIGN;
            pmr->r.sz = NULL;
            pmr->r.fPlayer = ! fPlayer;
            pmr->r.nResigned = atoi ( pch ) / ms.nCube;
            pmr->r.esResign.et = EVAL_NONE;
            
            AddMoveRecord( pmr );

          }

          goto finished;

          break;

        case TMG_TABLE_STAKE: 
          /* Table stake:   0 19 57.68 0.00 43.57 0.00 Settlement */

          /* ignore: FIXME: this should be stored in order to analyse
             tables stakes correctly */

          break;

        default:

          outputf ( "Please send the TMG file to bug-gnubg@gnubg.org!\n" );
          assert ( FALSE );

          break;

        } 


      }
      else {

        /* it's most likely an option: the players may enable or
           disable beavers and auto doubles during play */

        ParseTMGOptions ( sz, &mi, &pmgi->g.fCrawford,
                           &j, &pmgi->g.fJacoby, &j );

      }

    }

 finished:
    AddGame( pmgi );



}

extern void
ImportTMG ( FILE *pf, const char *szFilename ) {

  int fCrawfordRule = TRUE;
  int fJacobyRule = TRUE;
  int fAutoDoubles = 0;
  int nLength = 0;
  int fCrawford = FALSE;
  int n0, n1;
  int i, j;
  char sz[ 80 ];

  FreeMatch();
  ClearMatch();

  /* clear matchinfo */

  for ( j = 0; j < 2; ++j )
    SetMatchInfo ( &mi.pchRating[ j ], NULL, NULL );
  SetMatchInfo ( &mi.pchPlace, 
                 _("TrueMoneyGames (http://www.truemoneygames.com)"), NULL );
  SetMatchInfo ( &mi.pchEvent, NULL, NULL );
  SetMatchInfo ( &mi.pchRound, NULL, NULL );
  SetMatchInfo ( &mi.pchAnnotator, NULL, NULL );
  SetMatchInfo ( &mi.pchComment, NULL, NULL );
  mi.nYear = mi.nMonth = mi.nDay = 0;

  /* search for options (until first game is found) */

  while ( fgets ( sz, 80, pf ) ) {
    if( ParseTMGGame( sz, &i, &n0, &n1, &fCrawford, nLength ) )
      break;
      
    ParseTMGOptions ( sz, &mi, &fCrawfordRule, 
                      &fAutoDoubles, &fJacobyRule, &nLength );

  }

  /* set remainder of match info */

  while( !feof( pf ) ) {

    ImportTMGGame( pf, i, nLength, n0, n1, fCrawford,
                   fCrawfordRule, fAutoDoubles, fJacobyRule );
	
    while( fgets( sz, 80, pf ) )
      if( ParseTMGGame( sz, &i, &n0, &n1, &fCrawford, nLength ) )
        break;

  }
  
  UpdateSettings();
  
  /* swap players */
  
  CommandSwapPlayers ( NULL );
  
#if USE_GTK
  if( fX ){
    GTKThaw();
    GTKSet(ap);
  }
#endif
}

static void ImportBKGGame( FILE *pf, int *pi ) {

    char sz[ 80 ], *pch;
    moverecord *pmr, *pmrGame;
    int i, fPlayer;
    
    /* skip to first game */
    do {
	fgets( sz, 80, pf );
	if( feof( pf ) )
	    return;
    } while( strncmp( sz, "Black", 5 ) && strncmp( sz, "White", 5 ) );

    InitBoard( ms.anBoard );

    ClearMoveRecord();

    ListInsert( &lMatch, plGame );

    pmrGame = pmr = malloc( sizeof( movegameinfo ) );
    pmr->g.mt = MOVE_GAMEINFO;
    pmr->g.sz = NULL;
    pmr->g.i = ( *pi )++;
    pmr->g.nMatch = 0; /* not stored in BKG files -- assume money sessions */
    pmr->g.anScore[ 0 ] = ms.anScore[ 0 ];
    pmr->g.anScore[ 1 ] = ms.anScore[ 1 ];
    pmr->g.fCrawford = FALSE;
    pmr->g.fCrawfordGame = FALSE;
    pmr->g.fJacoby = fJacoby;
    pmr->g.fWinner = -1;
    pmr->g.nPoints = 0;
    pmr->g.fResigned = FALSE;
    pmr->g.nAutoDoubles = 0;
    IniStatcontext( &pmr->g.sc );
    AddMoveRecord( pmr );
    
    while( 1 ) {
	if( !strncmp( sz, "Black", 5 ) || !strncmp( sz, "White", 5 ) ) {
	    fPlayer = !strncmp( sz, "White", 5 );
	    
	    if( strlen( sz ) > 6 && !strncmp( sz + 6, "wins", 4 ) ) {
		if( ms.gs == GAME_PLAYING ) {
		    /* Neither a drop nor a bearoff to win, so we
		       presume the loser resigned. */
		    pmr = malloc( sizeof( pmr->r ) );
		    pmr->r.mt = MOVE_RESIGN;
		    pmr->r.sz = NULL;
		    pmr->r.esResign.et = EVAL_NONE;
		    pmr->r.fPlayer = !fPlayer;
		    if( strlen( sz ) > 14 && !strncmp( sz + 14, "gammon", 6 ) )
			pmr->r.nResigned = 2;
		    else if( strlen( sz ) > 14 &&
			     !strncmp( sz + 14, "backgammon", 10 ) )
			pmr->r.nResigned = 3;
		    else
			pmr->r.nResigned = 1;
		    AddMoveRecord( pmr );
		}

		AddGame( pmrGame );
		
		return;
	    } else if( strlen( sz ) > 6 && !strncmp( sz + 6, "Doubles", 7 ) ) {
		pmr = malloc( sizeof( pmr->d ) );
		pmr->d.mt = MOVE_DOUBLE;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = fPlayer;
		pmr->d.esDouble.et = EVAL_NONE;
		pmr->d.st = SKILL_NONE;
		AddMoveRecord( pmr );

		pmr = malloc( sizeof( pmr->d ) );
		pmr->d.mt = strlen( sz ) > 22 &&
		    !strncmp( sz + 22, "Accepted", 8 ) ? MOVE_TAKE : MOVE_DROP;
		pmr->d.sz = NULL;
		pmr->d.fPlayer = !fPlayer;
		pmr->d.esDouble.et = EVAL_NONE;
		pmr->d.st = SKILL_NONE;
		AddMoveRecord( pmr );
	    } else if( strlen( sz ) > 9 && isdigit( sz[ 7 ] ) &&
		       isdigit( sz[ 9 ] ) ) {
		pmr = malloc( sizeof( pmr->n ) );
		pmr->n.mt = MOVE_NORMAL;
		pmr->n.sz = NULL;
		pmr->n.anRoll[ 0 ] = sz[ 7 ] - '0';
		pmr->n.anRoll[ 1 ] = sz[ 9 ] - '0';
		pmr->n.fPlayer = fPlayer;
		pmr->n.ml.cMoves = 0;
		pmr->n.ml.amMoves = NULL;
		pmr->n.esDouble.et = EVAL_NONE;
		pmr->n.esChequer.et = EVAL_NONE;
		pmr->n.lt = LUCK_NONE;
		pmr->n.rLuck = ERR_VAL;
		pmr->n.stMove = SKILL_NONE;
		pmr->n.stCube = SKILL_NONE;

		if( strlen( sz ) > 13 ) {
		    for( i = 0, pch = sz + 13; i < 8 && *pch; i++ ) {
			while( *pch && !isdigit( *pch ) )
			    pch++;

			if( !*pch )
			    break;

			pmr->n.anMove[ i ] = fPlayer ? 24 - atoi( pch ) :
			    atoi( pch ) - 1;

			while( isdigit( *pch ) )
			    pch++;
		    }

		    if( i < 8 )
			pmr->n.anMove[ i ] = -1;
		} else
		    pmr->n.anMove[ 0 ] = -1;

		AddMoveRecord( pmr );
	    }
	}

	fgets( sz, 80, pf );
	if( feof( pf ) )
	    return;
    }
}

extern void ImportBKG( FILE *pf, const char *szFilename ) {

    int i;

    if( ms.gs == GAME_PLAYING && fConfirm ) {
	if( fInterrupt )
	    return;
	    
	if( !GetInputYN( _("Are you sure you want to import a saved match, "
			 "and discard the game in progress? ") ) )
	    return;
    }

#if USE_GTK
    if( fX )
	GTKFreeze();
#endif
    
    FreeMatch();
    ClearMatch();

    /* clear matchinfo */
    SetMatchInfo( &mi.pchRating[ 0 ], NULL, NULL );
    SetMatchInfo( &mi.pchRating[ 1 ], NULL, NULL );
    SetMatchInfo( &mi.pchPlace, NULL, NULL );
    SetMatchInfo( &mi.pchEvent, NULL, NULL );
    SetMatchInfo( &mi.pchRound, NULL, NULL );
    SetMatchInfo( &mi.pchAnnotator, NULL, NULL );
    SetMatchInfo( &mi.pchComment, NULL, NULL );
    mi.nYear = mi.nMonth = mi.nDay = 0;

    i = 0;
    
    while( !feof( pf ) )
	ImportBKGGame( pf, &i );
    
    UpdateSettings();
  
#if USE_GTK
    if( fX ){
	GTKThaw();
	GTKSet(ap);
    }
#endif
}
