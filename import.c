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

#include <stdio.h>
#include <math.h>
#include "backgammon.h"
#include "import.h"

extern void
ImportJF( FILE * fp, char *szFileName) {

  int nVersion, nCubeOwner, nOnRoll, nMovesLeft, nMovesRight;
  int nGameOrMatch, nOpponent, nLevel, nScore1, nScore2;
  int nCrawford, nDie1, nDie2; 
  int fCaution, fCubeUse, fJacoby, fBeavers, fSwapDice, fJFplayedLast;

  int i, idx, anNew[26], anOld[26];

  char szPlayer1[40];
  char szPlayer2[40];
  char szLastMove[25];

  short int w;
  unsigned char c;

  fread(&w, 2, 1, fp);   nVersion = (int) w; 

  if ( nVersion < 124 || nVersion > 126 ){
    outputl("File not recognised as Jellyfish file.");
    return;
  }
  if ( nVersion == 126 ){
    /* 3.0 */
    fread(&w, 2, 1, fp);   fCaution = (int) w; 
    fread(&w, 2, 1, fp);        /* Not in use */
  }

  if ( nVersion == 125 || nVersion == 126 ){
    /* 1.6 or newer */
    /* 3 variables not used by older version */
    fread(&w, 2, 1, fp);  fCubeUse = (int) w;
    fread(&w, 2, 1, fp);  fJacoby = (int) w;
    fread(&w, 2, 1, fp);  fBeavers = (int) w;

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

  fread(&w, 2, 1, fp);  nCube = (int) w;
  fread(&w, 2, 1, fp);  nCubeOwner = (int) w;
  /* Owner: 1 or 2 is player 1 or 2, 
     respectively, 0 means cube in the middle */

  fCubeOwner = nCubeOwner - 1;  /* Gary, is this right? */
  
  fread(&w, 2, 1, fp);  nOnRoll = (int) w;
  /* 0 means starting position. 
     If you add 2 to the player (to get 3 or 4)   Sure?
     it means that the player is on roll
     but the dice have not been rolled yet. */
 
  fread(&w, 2, 1, fp);  nMovesLeft = (int) w;
  fread(&w, 2, 1, fp);  nMovesRight = (int) w;
  /* These two variables are used when you use movement #1,
     (two buttons) and tells how many moves you have left
     to play with the left and the right die, respectively.
     Initialized to 1 (if you roll a double, left = 4 and
     right = 0). If movement #2 (one button), only the first
     one (left) is used to store both dice.  */
 
  fread(&w, 2, 1, fp);   /* Not in use */
 
  fread(&w, 2, 1, fp);  nGameOrMatch = (int) w;
  /* 1 = match, 3 = game */
 
  fread(&w, 2, 1, fp);  nOpponent = (int) w;
  /* 1 = 2 players, 2 = JF plays one side */
 
  fread(&w, 2, 1, fp);  nLevel = (int) w;

  fread(&w, 2, 1, fp);  nMatchTo = (int) w;
  /* 0 if single game  */

  if(nGameOrMatch == 3) nMatchTo = 0;

  fread(&w, 2, 1, fp);  nScore1 = (int) w;
  fread(&w, 2, 1, fp);  nScore2 = (int) w;
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

  fread(&w, 2, 1, fp);  fSwapDice = (int) w;
  /* TRUE if lower die is to be drawn to the left  */

  fread(&w, 2, 1, fp);  nCrawford = (int) w;
  /* 1 = pre-Crawford, 2 = Crawford, 3 = post-Crawford  */

  if(nCrawford == 2)  fCrawford = TRUE;
  if(nCrawford == 3)  fPostCrawford = TRUE;
  if(nGameOrMatch == 3) fCrawford = FALSE;

  fread(&w, 2, 1, fp);  fJFplayedLast = (int) w;

  fread(&c, 1, 1 , fp);
  for (i = 0 ; i < c; i++) fread(&szLastMove[i], 1, 1, fp);
  szLastMove[c]='\0';
  /* Stores whether the last move was played by JF
     If so, the move is stored in a string to be 
     displayed in the 'Show last' dialog box  */

  fread(&w, 2, 1, fp);    nDie1 = abs( (int) w );
  fread(&w, 2, 1, fp);    nDie2 = (int) w;
  /*  if ( nDie1 < 0 ) { nDie1=65536; }  /*  What?? */


  /* In the end the position itself is stored, 
     as well as the old position to be able to undo. 
     The two position arrays can be read like this:   */

  for ( i=0; i < 26; i++ ) {
    fread(&w, 2, 1, fp);
    anNew[ i ]=( (int) w ) - 20;
    fread(&w, 2, 1, fp);
    anOld[ i ]=( (int) w ) - 20;
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
    idx = 1;
  else
    idx = 0;
  
  if (nMovesLeft + nMovesRight > 1) {
    anDice[0] = nDie1;
    anDice[1] = nDie2;
  } else {
    anDice[0] = 0;
    anDice[1] = 0;
  } 

  for( i = 0; i < 25; i++ ) {
    anBoard[ idx ][ i ] = ( anNew[ i + 1 ]  < 0 ) ?  -anNew[ i + 1 ]  : 0;
    anBoard[ 1-idx ][ i ] = ( anNew[ 24 - i ]  > 0 ) ?  anNew[ 24 - i ]  : 0;
  }

  anBoard[ 1-idx ][ 24 ] =  anNew[0];
  anBoard[ idx ][ 24 ] =  -anNew[25];
 
  return;

}  
