
/*
 * osr.c
 *
 * by Jørn Thyssen <jthyssen@dk.ibm.com>, 2002.
 * (after inspiration from osr.cc from fibs2html <fibs2html.sourceforge.net>
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

#include <stdio.h>
#include <assert.h>
#include <string.h>


#include "config.h"

#include "eval.h"
#include "positionid.h"
#include "osr.h"
#include "mt19937int.h"

#define MAX_PROBS        32
#define MAX_GAMMON_PROBS 15
#define min(x,y)   (((x) > (y)) ? (y) : (x))

static int 
OSRQuasiRandomDice( const int iTurn, const int iGame, const int cGames,
                    int anDice[ 2 ] ) {

  if( !iTurn && !( cGames % 36 ) ) {
    anDice[ 0 ] = ( iGame % 6 ) + 1;
    anDice[ 1 ] = ( ( iGame / 6 ) % 6 ) + 1;
    return 0;
  } else if( iTurn == 1 && !( cGames % 1296 ) ) {
    anDice[ 0 ] = ( ( iGame / 36 ) % 6 ) + 1;
    anDice[ 1 ] = ( ( iGame / 216 ) % 6 ) + 1;
    return 0;
  } else {
    anDice[ 0 ] = ( genrand() % 6 ) + 1;
    anDice[ 1 ] = ( genrand() % 6 ) + 1;
    return ( anDice[ 0 ] > 0 && anDice[ 1 ] > 0 );
  }
}

/* Fill aaProb with one sided bearoff probabilities for position with */
/* bearoff id n.                                                      */

static 
#if defined( __GNUC__ )
inline
#endif
void
getBearoffProbs(const unsigned int n, unsigned int aaProb[32])
{
  int i;

  assert( pBearoff1 );
  
  for( i = 0; i < 32; i++ )
    aaProb[ i ] = pBearoff1[ ( n << 6 ) | ( i << 1 ) ] +
      ( pBearoff1[ ( n << 6 ) | ( i << 1 ) | 1 ] << 8 );
}


static int
isCrossOver ( const int from, const int to ) {
  return ( from / 6 ) != ( to / 6 );
}


/*
 * Find (and move) best move in one side rollout.
 *
 * Input
 *    anBoard: the board (reversed compared to normal convention)
 *    anDice: the roll (dice 1 <> dice 2)
 *    pnOut: current number of chequers outside home quadrant
 *
 * Output:
 *    anBoard: the updated board after the move
 *    pnOut: the number of chequers outside home quadrant after move
 *
 */

static int
chequersout ( int anBoard[ 24 ] ) {

  int i;
  int n=0;

  for ( i = 0; i < 18; i++ )
    n += anBoard[i];

  return n;

}

static int
checkboard ( int anBoard[ 24 ] ) {

  int i;

  for ( i = 0; i < 24; ++i )
    if ( anBoard[ i ] < 0 )
      return 0;

  return 1;

}

static void
FindBestMoveOSR2 ( int anBoard[ 24 ], const int anDice[ 2 ], int *pnOut ) {

  int ifar, inear, iboth;
  int iused = 0;
  int i, j, lc;

  ifar = 18 - anDice[ 0 ];
  inear = 18 - anDice[ 1 ];

  if ( anBoard[ ifar ] && anBoard[ inear ] ) {

    /* two chequers move exactly into the home quadrant */

    /* move chequers */

    --anBoard[ ifar ];
    --anBoard[ inear ];
    assert ( checkboard ( anBoard ) );
    anBoard[ 18 ] += 2;

    *pnOut -= 2;
    assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );

    return;

  }


  iboth = 18 - ( anDice[ 0 ] + anDice[ 1 ] );

  if ( anBoard[ iboth ] ) {

    /* one chequer move exactly into the home quadrant */

    /* move chequer */

    --anBoard[ iboth ];
    ++anBoard[ 18 ];
    assert ( checkboard ( anBoard ) );

    --*pnOut;
    assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );

    return;

  }


  /* loop through dice */

  for ( i = 0; i < 2 && *pnOut; ++i ) {

    /* check for exact cross over */

    if ( anBoard[ 18 - anDice[ i ] ] ) {

      /* move chequer */

      --anBoard[ 18 - anDice[ i ] ];
      ++anBoard[ 18 ];
    assert ( checkboard ( anBoard ) );

      --*pnOut;
      assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );

      ++iused;

      /* next die */

      continue; 

    }

    /* find chequer furthest away */

    lc = 0;
    while ( lc < 18 && ! anBoard[ lc ] )
      ++lc;

    /* try to make cross over from the back */

    for ( j = lc; j + anDice[ i ] < 18; ++j ) {

      if ( anBoard[ j ] && isCrossOver ( j, j + anDice[ i ] ) ) {

        /* move chequer */

        --anBoard[ j ];
        ++anBoard[ j + anDice[ i ] ];
        assert ( checkboard ( anBoard ) );
        ++iused;
        /* FIXME: increment lc if needed */

        break;

      }

    }


    if ( j + anDice[ i ] >= 18 ) {

      /* no move with cross-over was found */

      /* move chequer from the rear */

      for ( j = lc; j < 18; ++j ) {

        if ( anBoard[ j ] ) {

          --anBoard[ j ];
          ++anBoard[ j + anDice[ i ] ];
    assert ( checkboard ( anBoard ) );
          ++iused;

          if ( j + anDice[ i ] >= 18 )
            /* we've moved inside home quadrant */
            --*pnOut;
          assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );

          break;

        }
        
      }
      
      
    }
    
  }

  if ( ! *pnOut && iused < 2 ) {

    /* die 2 still left, and all chequers inside home quadrant */

    if ( anBoard[ 24 - anDice[ 1 ] ] ) {
      /* bear-off */
      --anBoard[ 24 - anDice[ 1 ] ];
    assert ( checkboard ( anBoard ) );
      return;
    }

    /* try filling rearest empty space */
    
    for ( j = 18; j + anDice[ 1 ] < 24; ++j ) {
      
      if ( anBoard[ j ] && ! anBoard[ j + anDice[ 1 ] ] ) {
        /* empty space found */
        
        --anBoard[ j ];
        ++anBoard[ j + anDice[ 1 ] ];
    assert ( checkboard ( anBoard ) );
        return;
        
      }
    }

    /* move chequer from the rear */

    for ( j = 18; j < 24; ++j ) {

      if ( anBoard[ j ] ) {

        /* move chequer from the rear */

        --anBoard[ j ];

        if ( j + anDice[ 1 ] < 24 )
          /* add chequer to point */
          ++anBoard[ j + anDice[ 1 ] ];
        /* else */
        /*   bearoff */

    assert ( checkboard ( anBoard ) );
        return;

      }

    }

  }

  assert ( iused == 2 );
  return;

}

/*
 * Find (and move) best move in one side rollout.
 *
 * Input
 *    anBoard: the board (reversed compared to normal convention)
 *    anDice: the roll (dice 1 = dice 2)
 *    pnOut: current number of chequers outside home quadrant
 *
 * Output:
 *    anBoard: the updated board after the move
 *    pnOut: the number of chequers outside home quadrant after move
 *
 */

static void
FindBestMoveOSR4 ( int anBoard[ 24 ], const int nDice, int *pnOut ) {

  int nd = 4;
  int i, n;
  int first, any;
  int lc;

  /* check for exact bear-ins */

  while ( nd > 0 && *pnOut > 0 && anBoard[ 18 - nDice ] ) {

    --anBoard[ 18 - nDice ];
    ++anBoard[ 18 ];
    assert ( checkboard ( anBoard ) );

    --nd;
    --*pnOut;
    assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );

  }

  if ( *pnOut > 0 ) {

    /* check for 4, 3, or 2 chequers move exactly into home quadrant */
    
    for ( n = nd; n > 1; --n ) {
      
      i = 18 - n * nDice;
      
      if ( i >= 0 && anBoard[ i ] ) {
        
        --anBoard[ i ];
        ++anBoard[ 18 ];
    assert ( checkboard ( anBoard ) );
        
        nd -= n;
        --*pnOut;
    assert ( *pnOut >= 0 );
    assert ( *pnOut == chequersout ( anBoard ) );
        
        n =nd; /* restart loop */
        
      }
      
    }

  }

  if ( *pnOut > 0 && nd > 0 ) {

    first = TRUE;

    /* find rearest chequer */

    lc = 0;
    while ( lc < 18 && ! anBoard[ lc ] )
      lc++;

    /* try to make cross over from the back */

    for ( i = lc; i < 18; ++i ) {

      if ( anBoard[ i ] ) {

        if ( isCrossOver ( i, i + nDice ) && ( first || i + nDice < 18 ) ) {

          /* move chequers */
          
          while ( anBoard[ i ] && nd && *pnOut ) {
            
            --anBoard[ i ];
            ++anBoard[ i + nDice ];
            assert ( checkboard ( anBoard ) );
            
            if ( i + nDice >= 18 )
              --*pnOut;
            assert ( *pnOut >= 0 );
            assert ( *pnOut == chequersout ( anBoard ) );
            
            --nd;
            
          }

          if ( ! *pnOut || ! nd )
            break;
          
          /* did we move all chequers from that point */

          first = ! anBoard[ i ];

        }

      }

    }

    /* move chequers from the rear */

    while ( *pnOut && nd ) {

      for ( i = lc; i < 18; ++i ) {

        if( anBoard[ i ] ) {

          while ( anBoard[ i ] && nd && *pnOut ) {

            --anBoard[ i ];
            ++anBoard[ i + nDice ];
            assert ( checkboard ( anBoard ) );

            if ( i + nDice >= 18 )
              --*pnOut;
            assert ( *pnOut >= 0 );
            assert ( *pnOut == chequersout ( anBoard ) );

            --nd;

          }

          if ( ! n || ! *pnOut )
            break;

        }
      }
    }
  }

  
  if ( ! *pnOut ) {

    /* all chequers inside home quadrant */

    while ( nd ) {

      if ( anBoard[ 24 - nDice ] ) {
        /* perfect bear-off */
        --anBoard[ 24 - nDice ];
        --nd;
        assert ( checkboard ( anBoard ) );
        continue;
      }

      if ( nd >= 2 && nDice <= 3 && anBoard[ 24 - 2 *nDice ] > 0 ) {
        /* bear  double 1s, 2s, and 3s off, e.g., 4/2/0 */
        --anBoard[ 24 - 2 * nDice ];
        nd -= 2;
        assert ( checkboard ( anBoard ) );
        continue;
      }

      if ( nd >= 3 && nDice <= 2 && anBoard[ 24 - 3 * nDice ] > 0 ) {
        /* bear double 1s off from 3 point (3/2/1/0) or
           double 2s off from 6 point (6/4/2/0) */
        --anBoard[ 24 - 3 * nDice ];
        nd -= 3;
        assert ( checkboard ( anBoard ) );
        continue;
      }

      if ( nd >= 4 && nDice <= 1 && anBoard[ 24 - 4 * nDice ] > 0 ) {
        /* hmmm, this should not be possible... */
        /* bear off double 1s: 4/3/2/1/0 */
        --anBoard[ 24 - 4 * nDice ];
        assert ( checkboard ( anBoard ) );
        nd -= 4;
      }

      any = FALSE;

      /* move chequers from rear */

      /* FIXME: fill gaps? */

      for ( i = 18; nd && i < 24; ++i ) {

        while ( anBoard[ i ] && nd ) {

          any = TRUE;

          --anBoard[ i ];
          --nd;

          if ( i + nDice < 24 )
            ++anBoard[ i + nDice ];
          assert ( checkboard ( anBoard ) );

        }

      }

      if ( ! any )
        /* no more chequers left */
        nd = 0;

    }

  }

}


/*
 * Find (and move) best move in one sided rollout.
 *
 * Input
 *    anBoard: the board (reversed compared to normal convention)
 *    anDice: the roll (dice 1 is assumed to be lower than dice 2)
 *    pnOut: current number of chequers outside home quadrant
 *
 * Output:
 *    anBoard: the updated board after the move
 *    pnOut: the number of chequers outside home quadrant after move
 *
 */

static void
FindBestMoveOSR ( int anBoard[ 24 ], const int anDice[ 2 ], int *pnOut ) {

  if ( anDice[ 0 ] != anDice[ 1 ] )
    FindBestMoveOSR2 ( anBoard, anDice, pnOut );
  else
    FindBestMoveOSR4 ( anBoard, anDice[ 0 ], pnOut );

}


/*
 * osr: one sided rollout.
 *
 * Input:
 *   anBoard: the board (reversed compared to normal convention)
 *   iGame: game#
 *   nGames: # of games.
 *   nOut: current number of chequers outside home quadrant.
 *
 * Returns: number of rolls used to get all chequers inside home
 *          quadrant.
 */

static int
osr ( int anBoard[ 24 ], const int iGame, const int nGames, const int nOut ) {

  int iTurn = 0;
  int anDice[ 2 ];
  int n = nOut;

  /* loop until all chequers are in home quadrant */

  while ( n ) {

    /* roll dice */

    if ( OSRQuasiRandomDice ( iTurn, iGame, nGames, anDice ) < 0 )
      return -1;

    if ( anDice[ 0 ] < anDice[ 1 ] )
      swap ( anDice, anDice + 1 );

    /* find and move best move */

    FindBestMoveOSR ( anBoard, anDice, &n );

    iTurn++;

  }


  return iTurn;

}


/*
 * RollOSR: perform onesided rollout
 *
 * Input:
 *   nGames: number of simulations
 *   anBoard: the board (reversed compared to normal convention)
 *   nOut: number of chequers outside home quadrant
 *
 * Output:
 *   arProbs[ nMaxProbs ]: probabilities
 *   arGammonProbs[ nMaxGammonProbs ]: gammon probabilities
 *
 */

static void
rollOSR ( const int nGames, const int anBoard[ 24 ], const int nOut,
          float arProbs[], const int nMaxProbs,
          float arGammonProbs[], const int nMaxGammonProbs ) {

  int anCounts [ nMaxGammonProbs ];
  int an[ 24 ];
  int antmp[ 6 ];
  unsigned int anProb[ 32 ];
  int i, n, m;
  int iGame;
  
  memset ( anCounts, 0, sizeof ( anCounts ) );

  for ( i = 0; i < nMaxProbs; ++i ) 
    arProbs[ i ] = 0.0f;

  /* perform rollouts */

  for ( iGame = 0; iGame < nGames; ++iGame ) {

    memcpy ( an, anBoard, sizeof ( an ) );

    /* do actual rollout */

    n = osr ( an, iGame, nGames, nOut );

    /* number of chequers in home quadrant */

    m = 0;
    for ( i = 18; i < 24; ++i )
      m += an[ i ];

    /* update counts */

    ++anCounts[ min ( m == 15 ? n + 1 : n, nMaxGammonProbs - 1 ) ];

    /* get prob. from bearoff1 */

    for ( i = 0; i < 6; ++i )
      antmp[ i ] = an[ 23 - i ];

    getBearoffProbs ( PositionBearoff ( antmp, 6 ), anProb );

    for ( i = 0; i < 32; ++i )
      arProbs[ min ( n + i, nMaxProbs - 1 ) ] += anProb[ i ] / 65535.0f;

  }

  /* scale resulting probabilities */

  for ( i = 0; i < nMaxProbs; ++i ) {
    arProbs[ i ] /= nGames;
    printf ( "arProbs[%d]=%f\n", i, arProbs[ i ] );
  }

  /* calculate gammon probs. 
     (prob. of getting inside home quadrant in i rolls */

  for ( i = 0; i < nMaxGammonProbs; ++i ) {
    arGammonProbs[ i ] = 1.0f * anCounts[ i ] / nGames;
    printf ( "arGammonProbs[%d]=%f\n", i, arGammonProbs[ i ] );
  }

}



/*
 * OSP: one sided probabilities
 *
 * Input:
 *   anBoard: one side of the board
 *   nGames: number of simulations
 *   
 * Output:
 *   an: ???
 *   arProb: array of probabilities (ar[ i ] is the prob. that player 
 *           bears off in i moves)
 *   arGammonProb: gammon probabilities
 *
 */

static int
osp ( const int anBoard[ 25 ], const int nGames,
      int an[ 25 ], float arProbs[ MAX_PROBS ], 
      float arGammonProbs[ MAX_GAMMON_PROBS ] ) {

  int i, n;
  int nTotal, nOut;
  unsigned int anProb[ 32 ];

  /* copy board into an, and find total number of chequers left,
     and number of chequers outside home */

  nTotal = nOut = 0;

  for ( i = 0; i < 24; ++i ) {

    n = anBoard[ 23 - i ];

    /* copy board into an */

    an[ i ] = n;

    if ( n ) {

      /* total number of chequers left */
      nTotal += n;

      if ( i < 18 )
        /* number of chequers outside home */
        nOut += n;

    }

  }


  if ( nOut > 0 ) 
    /* chequers outside home: do one sided rollout */
    rollOSR ( nGames, an, nOut, 
              arProbs, MAX_PROBS, 
              arGammonProbs, MAX_GAMMON_PROBS );
  else {
    /* chequers inde home: use BEAROFF2 */

    /* no gammon possible */
    for ( i = 0; i < MAX_GAMMON_PROBS; ++i )
      arGammonProbs[ i ] = 0.0f;

    if ( nTotal == 15 )
      arGammonProbs[ 1 ] = 1.0f;
    else
      arGammonProbs[ 0 ] = 1.0f;

    /* get probs from BEAROFF2 */

    for ( i = 0; i < MAX_PROBS; ++i )
      arProbs[ i ] = 0.0f;

    getBearoffProbs ( PositionBearoff ( anBoard, 6 ), anProb );

    for ( i = 0; i < 32; ++i ) {
      n = min ( i, MAX_PROBS - 1 );
      arProbs[ n ] += anProb[ i ] / 65535.0f;
      printf ( "arProbs[%d]=%f\n", n, arProbs[n] );
    }

  }

  return nTotal;

}


static float
bgProb ( const int anBoard[ 25 ],
         const int fOnRoll,
         const int nTotal,
         const float arProbs[],
         const int nMaxProbs ) {

  int nTotPipsHome = 0;
  int i, j;
  float r, s;
  int antmp[ 6 ];
  int anProb[ 32 ];

  /* total pips before out of opponent's home quadrant */

  for ( i = 1; i < 6; ++i )
    nTotPipsHome += anBoard[ i ] * ( 6 - i );

  r = 0.0f;

  if ( nTotPipsHome ) {

    /* ( nTotal + 3 ) / 4 - 1: number of rolls before opponent is off. */
    /* (nTotPipsHome + 2) / 3: numbers of rolls before I'm out of
       opponent's home quadrant (with consequtive 2-1's) */

    if ( ( nTotal + 3 ) / 4 - 1  <= ( nTotPipsHome + 2 ) / 3 ) {

      /* backgammon is possible */

      /* get "bear-off" prob (for getting out of opp.'s home quadr.) */

      for ( i = 0; i < 6; ++i )
        antmp[ i ] = anBoard[ 5 - i ];
      
      getBearoffProbs ( PositionBearoff ( antmp, 6 ), anProb );

      for ( i = 0; i < nMaxProbs; ++i ) {

        if ( arProbs[ i ] > 0.0f ) {

          s = 0.0f;

          for ( j = i + ! fOnRoll; j < 32; ++j ) 
            s += anProb[ j ] / 65535.0f;

          r += s * arProbs[ i ];

        }

      }

    }

  }

  return r;

}


/*
 * Calculate race probabilities using one sided rollouts.
 *
 * Input:
 *   anBoard: the current board 
 *            (assumed to be a race position without contact)
 *   nGames:  the number of simulations to perform
 *
 * Output:
 *   arOutput: probabilities.
 *
 */

extern void
raceProbs ( int anBoard[ 2 ][ 25 ], const int nGames,
            float arOutput[ NUM_OUTPUTS ] ) {
  
  int an[ 2 ][ 25 ];
  float aarProbs[ 2 ][ MAX_PROBS ];
  float aarGammonProbs[ 2 ][ MAX_PROBS ];
  float arG[ 2 ], arBG[ 2 ];

  int anTotal[ 2 ];

  int i, j, k;

  float w, s;

  sgenrand ( 0 );

  for ( i = 0; i < 5; ++i )
    arOutput[ i ] = 0.0f;

  for ( i = 0; i < 2; ++i )
    anTotal[ i ] = 
      osp ( anBoard[ i ], nGames, an[ i ], 
            aarProbs[ i ], aarGammonProbs[ i ] );

  /* calculate OUTPUT_WIN */

  w = 0;
 
  for ( i = 0; i < MAX_PROBS; ++i ) {
    
    /* calculate the prob. of the opponent using more than i rolls
       to bear off */
    
    s = 0.0f;
    for ( j = i; j < MAX_PROBS; ++j )
      s += aarProbs[ 0 ][ j ];
    
    /* winning chance is: prob. of me bearing off in i rolls times
       prob. the opponent doesn't bear off in i rolls */
    
    w += aarProbs[ 1 ][ i ] * s;
    
  }

  arOutput[ OUTPUT_WIN ] = min ( w, 1.0f );

  /* calculate gammon and backgammon probs */

  for ( i = 0; i < 2; ++i ) {

    arG[ i ] = 0.0f;
    arBG[ i ] = 0.0f;

    if ( anTotal[ ! i ] == 15 ) {

      /* gammon and backgammon possible */

      for ( j = 0; j < MAX_GAMMON_PROBS; ++j ) {

        /* chance of opponent having borne all chequers of
           within j rolls */

        s = 0.0f;
        for ( k = 0; k < j + i; ++k )
          s += aarProbs[ i ][ k ];

        /* gammon chance */

        arG[ i ] += aarGammonProbs[ ! i ][ j ] * s;

      }

      if ( arG[ i ] > 0.0f )
        /* calculate backgammon probs */
        arBG[ i ] = bgProb ( an[ ! i ], i, anTotal[ i ], 
                             aarProbs[ i ], MAX_PROBS );

    }

  }

  arOutput[ OUTPUT_WINGAMMON ] = min ( arG[ 1 ], 1.0f );
  arOutput[ OUTPUT_LOSEGAMMON ] = min ( arG[ 0 ], 1.0f );
  arOutput[ OUTPUT_WINBACKGAMMON ] = min ( arBG[ 1 ], 1.0f );
  arOutput[ OUTPUT_LOSEBACKGAMMON ] = min ( arBG[ 0 ], 1.0f );

}
