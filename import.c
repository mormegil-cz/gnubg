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

#include <stdio.h>
#include <math.h>
#include "backgammon.h"
#include "import.h"

static int ReadInt16( FILE *pf ) {

    /* Read a little-endian, signed (2's complement) 16-bit integer.
       This is inefficient on hardware which is already little-endian
       or 2's complement, but at least it's portable. */
    
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

  nCube = ReadInt16( fp );
  nCubeOwner = ReadInt16( fp );
  /* Owner: 1 or 2 is player 1 or 2, 
     respectively, 0 means cube in the middle */

  fCubeOwner = nCubeOwner - 1;

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

  nMatchTo = ReadInt16( fp );
  /* 0 if single game  */

  if(nGameOrMatch == 3)
      nMatchTo = 0;

  nScore1 = ReadInt16( fp );
  nScore2 = ReadInt16( fp );
  /* Can be whatever if match length = 0  */

  anScore[0] = nScore1;
  anScore[1] = nScore2;

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
      fPostCrawford = FALSE;
      fCrawford = TRUE;
      break;
      
  case 3: /* post-Crawford  */
      fPostCrawford = TRUE;
      fCrawford = FALSE;
      break;

  default:
      fCrawford = fPostCrawford = FALSE;
      break;
  }
  
  if(nGameOrMatch == 3) fCrawford = fPostCrawford = FALSE;

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
  
  fTurn = idx;
 
  if (nOnRoll == 0) fTurn = -1; 

  if (nMovesLeft + nMovesRight > 1) {
    anDice[0] = nDie1;
    anDice[1] = nDie2;
  } else {
    anDice[0] = 0;
    anDice[1] = 0;
  } 

  for( i = 0; i < 25; i++ ) {
    anBoard[ idx ][ i ] = ( anNew[ i + 1 ]  < 0 ) ?  -anNew[ i + 1 ]  : 0;
    anBoard[ ! idx ][ i ] = ( anNew[ 24 - i ]  > 0 ) ?  anNew[ 24 - i ]  : 0;
  }

  anBoard[ ! idx ][ 24 ] =  anNew[0];
  anBoard[ idx ][ 24 ] =  -anNew[25];
 
  return;

}  
