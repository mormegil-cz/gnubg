/*
 * bearoff.c
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <unistd.h>
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif


#include "positionid.h"
#include "eval.h"
#include "bearoff.h"
#include "i18n.h"
#include "bearoffgammon.h"
#include "path.h"

#if WIN32
#define BINARY O_BINARY
#else
#define BINARY 0
#endif


char *aszBearoffGenerator[ NUM_BEAROFFS ] = {
  N_("GNU Backgammon"),
  N_("ExactBearoff"),
  N_("Unknown program")
};


static void
setGammonProb(int anBoard[2][25], int bp0, int bp1, float* g0, float* g1)
{
  int i;
  unsigned short int prob[32];

  int tot0 = 0;
  int tot1 = 0;
  
  for(i = 5; i >= 0; --i) {
    tot0 += anBoard[0][i];
    tot1 += anBoard[1][i];
  }

  {                                       assert( tot0 == 15 || tot1 == 15 ); }

  *g0 = 0.0;
  *g1 = 0.0;

  if( tot0 == 15 ) {
    struct GammonProbs* gp = getBearoffGammonProbs(anBoard[0]);
    double make[3];

    BearoffDist ( pbc1, bp1, NULL, NULL, NULL, prob, NULL );
    
    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));
    
    *g1 = ((prob[1] / 65535.0) +
	   (1 - make[0]) * (prob[2] / 65535.0) +
	   (1 - make[1]) * (prob[3] / 65535.0) +
	   (1 - make[2]) * (prob[4] / 65535.0));
  }

  if( tot1 == 15 ) {
    struct GammonProbs* gp = getBearoffGammonProbs(anBoard[1]);
    double make[3];

    BearoffDist ( pbc1, bp0, NULL, NULL, NULL, prob, NULL );

    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));

    *g0 = ((prob[1] / 65535.0) * (1-make[0]) + (prob[2]/65535.0) * (1-make[1])
	   + (prob[3]/65535.0) * (1-make[2]));
  }
}  



static void 
AverageRolls ( const float arProb[ 32 ], float *ar ) {

  float sx;
  float sx2;
  int i;

  sx = sx2 = 0.0f;

  for ( i = 1; i < 32; ++i ) {
    sx += i * arProb[ i ];
    sx2 += i * i * arProb[ i ];
  }


  ar[ 0 ] = sx;
  ar[ 1 ] = sqrt ( sx2 - sx *sx );

}

/* Make a plausible bearoff move (used to create approximate bearoff
   database). */
static int HeuristicBearoff( int anBoard[ 6 ], int anRoll[ 2 ] ) {

    int i, /* current die being played */
	c, /* number of dice to play */
	nMax, /* highest occupied point */
	anDice[ 4 ],
	n, /* point to play from */
	j, iSearch, nTotal;

    if( anRoll[ 0 ] == anRoll[ 1 ] ) {
	/* doubles */
	anDice[ 0 ] = anDice[ 1 ] = anDice[ 2 ] = anDice[ 3 ] = anRoll[ 0 ];
	c = 4;
    } else {
	/* non-doubles */
	assert( anRoll[ 0 ] > anRoll[ 1 ] );
	
	anDice[ 0 ] = anRoll[ 0 ];
	anDice[ 1 ] = anRoll[ 1 ];
	c = 2;
    }

    for( i = 0; i < c; i++ ) {
	for( nMax = 5; nMax >= 0 && !anBoard[ nMax ]; nMax-- )
	    ;

	if( nMax < 0 )
	    /* finished bearoff */
	    break;
    
	if( anBoard[ anDice[ i ] - 1 ] ) {
	    /* bear off exactly */
	    n = anDice[ i ] - 1;
	    goto move;
	}

	if( anDice[ i ] - 1 > nMax ) {
	    /* bear off highest chequer */
	    n = nMax;
	    goto move;
	}
	
	nTotal = anDice[ i ] - 1;
	for( j = i + 1; j < c; j++ ) {
	    nTotal += anDice[ j ];
	    if( nTotal < 6 && anBoard[ nTotal ] ) {
		/* there's a chequer we can bear off with subsequent dice;
		   do it */
		n = nTotal;
		goto move;
	    }
	}

	for( n = -1, iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
	    if( anBoard[ iSearch ] >= 2 && /* at least 2 on source point */
		!anBoard[ iSearch - anDice[ i ] ] && /* dest empty */
		( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ) )
		n = iSearch;
	if( n >= 0 )
	    goto move;

	/* find the point with the most on it (or least on dest) */
	for( iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
	    if( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ||
		( anBoard[ iSearch ] == anBoard[ n ] &&
		  anBoard[ iSearch - anDice[ i ] ] <
		  anBoard[ n - anDice[ i ] ] ) )
		n = iSearch;

    move:
	assert( n >= 0 );
	assert( anBoard[ n ] );
	anBoard[ n ]--;

	if( n >= anDice[ i ] )
	    anBoard[ n - anDice[ i ] ]++;
    }

    return PositionBearoff( anBoard, 6 );
}

static void GenerateBearoff( unsigned char *p, int nId ) {

    int i, iBest, anRoll[ 2 ], anBoard[ 6 ], aProb[ 32 ];
    unsigned short us;

    for( i = 0; i < 32; i++ )
	aProb[ i ] = 0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    PositionFromBearoff( anBoard, nId, 6 );
	    iBest = HeuristicBearoff( anBoard, anRoll );

	    assert( iBest >= 0 );
	    assert( iBest < nId );
	    
	    if( anRoll[ 0 ] == anRoll[ 1 ] )
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += p[ ( iBest << 6 ) | ( i << 1 ) ] +
			( p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 );
	    else
		for( i = 0; i < 31; i++ )
		    aProb[ i + 1 ] += ( p[ ( iBest << 6 ) | ( i << 1 ) ] +
			( p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 ) ) << 1;
	}

    for( i = 0; i < 32; i++ ) {
	us = ( aProb[ i ] + 18 ) / 36;
	p[ ( nId << 6 ) | ( i << 1 ) ] = us & 0xFF;
	p[ ( nId << 6 ) | ( i << 1 ) | 1 ] = us >> 8;
    }
}

static unsigned char *HeuristicDatabase( void (*pfProgress)( int ) ) {

    unsigned char *p = malloc( 54264 * 64 );
    int i;
    
    if( !p )
	return NULL;

    p[ 0 ] = p[ 1 ] = 0xFF;
    for( i = 2; i < 64; i++ )
	p[ i ] = 0;

    for( i = 1; i < 54264; i++ ) {
	GenerateBearoff( p, i );
	if( pfProgress && !( i % 1000 ) )
	    pfProgress( i );
    }

    return p;
}


/*
 * BEAROFF_GNUBG: read two sided bearoff database
 *
 */

static int
ReadTwoSidedBearoff ( bearoffcontext *pbc,
                      const unsigned int iPos,
                      float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  int k = ( pbc->fCubeful ) ? 4 : 1;
  int i;
  unsigned char ac[ 8 ];
  unsigned char *pc;
  unsigned short int us;

  if ( pbc->fInMemory )
    pc = ((char *) pbc->p)+ 40 + 2 * iPos * k;
  else {
    lseek ( pbc->h, 40 + 2 * iPos * k, SEEK_SET );
    read ( pbc->h, ac, k * 2 );
    pc = ac;
  }

  for ( i = 0; i < k; ++i ) {
    us = pc[ 2 * i ] | ( pc[ 2 * i + 1 ] ) << 8;
    if ( aus )
      aus[ i ] = us;
    if ( ar )
      ar[ i ] = us / 32767.5f - 1.0f;
  }      

  ++pbc->nReads;

  return 0;

}


static int
countBoard ( const int nDistance, const int nChequers ) {

  int i;
  int id = 0;

  if ( nDistance == 1 || ! nChequers )
    return 1;
  else {
    for ( i = 0; i <= nChequers; ++i )
      id += countBoard ( nDistance - 1, nChequers - i );
    return id;
  }

  return 0;

}


static unsigned int
boardNr ( const int anBoard[], const int nDistance, 
          const int nPoints, const int nChequers ) {

  int i;
  unsigned int id;

  if ( nDistance == 1 || ! nChequers )
    return 0;

  id = boardNr ( anBoard, nDistance - 1, nPoints, 
                 nChequers - anBoard [ nDistance - 2 ] );
  for ( i = 0; i < anBoard [ nDistance - 2 ]; ++i )
    id += countBoard ( nDistance - 1, nChequers - i );

  return id;

}


/*
 * BEAROFF_EXACT_BEAROFF: read equities from databases generated by
 * ExactBearoff <URL:http://www.xs4all.nl/~mdgsoft/bearoff/>
 *
 * 12 bytes is stored for each position
 *
 */

static int
ReadExactBearoff ( bearoffcontext *pbc, 
                   const unsigned int iPos,
                   float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  unsigned char ac[ 12 ];
  long offset;
  unsigned long ul;
  const float storefactor = 4194000.0f;
  int i;
  unsigned int nUs, nThem;
  int n = Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints );
  int anBoard[ 2 ][ 25 ];

  /* convert gnu backgammon position to ExactBearoff position */

  nUs = iPos / n;
  nThem = iPos % n;


#if 0
  for ( i = 0; i < 925; ++i ) {
    PositionFromBearoff ( anBoard[ 0 ], i, pbc->nChequers );
    nUs = boardNr ( anBoard[ 0 ], pbc->nPoints, pbc->nPoints,
                    pbc->nChequers );
    if ( nUs == 3 ) {
      printf ( "position match; %d (%d %d %d %d %d %d)\n", i,
               anBoard[ 0 ][ 0 ],
               anBoard[ 0 ][ 1 ],
               anBoard[ 0 ][ 2 ],
               anBoard[ 0 ][ 3 ],
               anBoard[ 0 ][ 4 ],
               anBoard[ 0 ][ 5 ] );
      exit(-1);
    }
  }
#endif
      


  PositionFromBearoff ( anBoard[ 0 ], nThem, pbc->nChequers );
  PositionFromBearoff ( anBoard[ 1 ], nUs, pbc->nChequers );

  /* printf ( "gnu position %u %u\n", nUs, nThem ); */

  nThem = boardNr ( anBoard[ 0 ], pbc->nPoints, pbc->nPoints, pbc->nChequers );
  nUs =   boardNr ( anBoard[ 1 ], pbc->nPoints, pbc->nPoints, pbc->nChequers );

  /* printf ( "eb position  %u %u\n", nUs, nThem ); */

  offset = nUs * n + nThem + 16;

  /* read from database */

  if ( lseek ( pbc->h, offset, SEEK_SET ) < 0 ) 
    return -1;

  if ( read ( pbc->h, ac, 12 ) < 12 )
    return -1;

  for ( i = 0; i < 4; ++i ) {
    ul = ac[ 3 * i ] | ac[ 3 * i + 1 ] << 8 | ac[ 3 * i + 2 ] << 16;

    if ( ar )
      ar[ i ] = ul / storefactor - 2.0f;
    if ( aus )
      aus[ i ] = ul >> 8;

  }
  
  ++pbc->nReads;

  return 0;

}

extern int
BearoffCubeful ( bearoffcontext *pbc,
                 const unsigned int iPos,
                 float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:

    if ( ! pbc->fCubeful )
      return -1;
    else {
      return ReadTwoSidedBearoff ( pbc, iPos, ar, aus );
    }

    break;

  case BEAROFF_EXACT_BEAROFF:

    return ReadExactBearoff ( pbc, iPos, ar, aus );
    break;

  default:

    assert ( FALSE );
    break;

  }

  return 0;

}


static int
BearoffEvalTwoSided ( bearoffcontext *pbc, 
                      int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  
  ReadTwoSidedBearoff ( pbc, iPos, ar, NULL );

  memset ( arOutput, 0, 5 * sizeof ( float ) );
  arOutput[ OUTPUT_WIN ] = ar[ 0 ] / 2.0f + 0.5;
  
  return ar [ 0 ] * 65535.0;

}

static int
BearoffEvalOneSided ( bearoffcontext *pbc, 
                      int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  int i, j;
  float aarProb[ 2 ][ 32 ];
  float aarGammonProb[ 2 ][ 32 ];
  float r;
  int anOn[ 2 ];
  int an[ 2 ];
  float ar[ 2 ][ 4 ];

  /* get bearoff probabilities */

  for ( i = 0; i < 2; ++i ) {

    an[ i ] = PositionBearoff ( anBoard[ i ], pbc->nPoints );
    BearoffDist ( pbc, an[ i ], aarProb[ i ], aarGammonProb[ i ], ar [ i ],
                  NULL, NULL );

  }

  /* calculate winning chance */

  r = 0.0;
  for ( i = 0; i < 32; ++i )
    for ( j = i; j < 32; ++j )
      r += aarProb[ 1 ][ i ] * aarProb[ 0 ][ j ];

  arOutput[ OUTPUT_WIN ] = r;

  /* calculate gammon chances */

  for ( i = 0; i < 2; ++i )
    for ( j = 0, anOn[ i ] = 0; j < 25; ++j )
      anOn[ i ] += anBoard[ i ][ j ];

  if ( anOn[ 0 ] == 15 || anOn[ 1 ] == 15 ) {

    if ( pbc->fGammon ) {

      /* my gammon chance: I'm out in i rolls and my opponent isn't inside
         home quadrant in less than i rolls */
      
      r = 0;
      for( i = 0; i < 32; i++ )
        for( j = i; j < 32; j++ )
          r += aarProb[ 1 ][ i ] * aarGammonProb[ 0 ][ j ];
      
      arOutput[ OUTPUT_WINGAMMON ] = r;
      
      /* opp gammon chance */
      
      r = 0;
      for( i = 0; i < 32; i++ )
        for( j = i + 1; j < 32; j++ )
          r += aarProb[ 0 ][ i ] * aarGammonProb[ 1 ][ j ];
      
      arOutput[ OUTPUT_LOSEGAMMON ] = r;
      
    }
    else {
      
      setGammonProb(anBoard, an[ 0 ], an[ 1 ],
                    &arOutput[ OUTPUT_LOSEGAMMON ],
                    &arOutput[ OUTPUT_WINGAMMON ] );
      
    }
  }
  else {
    /* no gammons possible */
    arOutput[ OUTPUT_WINGAMMON ] = 0.0f;
    arOutput[ OUTPUT_LOSEGAMMON ] = 0.0f;
  }
      
    
  /* no backgammons possible */
    
  arOutput[ OUTPUT_LOSEBACKGAMMON ] = 0.0f;
  arOutput[ OUTPUT_WINBACKGAMMON ] = 0.0f;

  return ar [ 0 ][ 0 ] * 65535.0;

}



extern int
BearoffEval ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:

    if ( pbc->fTwoSided ) 
      BearoffEvalTwoSided ( pbc, anBoard, arOutput );
    else 
      BearoffEvalOneSided ( pbc, anBoard, arOutput );

    break;


  case BEAROFF_EXACT_BEAROFF:

    assert ( pbc->fTwoSided );
    BearoffEvalTwoSided ( pbc, anBoard, arOutput );
    break;

  default:

    assert ( FALSE );
    break;

  }

  return 0;
}

extern void
BearoffStatus ( bearoffcontext *pbc, char *sz ) {

  if ( ! pbc )
    return;

  if ( pbc->fTwoSided ) {

    sprintf ( sz, _(" * %s:\n"
                    "   - generated by %s\n"
                    "   - up to %d chequers on %d points (%d positions)"
                    " per player\n"
                    "   - %s\n"
                    "   - number of reads: %lu\n"),
              pbc->fInMemory ?
              _("In memory 2-sided bearoff database evaluator") :
              _("On disk 2-sided bearoff database evaluator"),
              gettext ( aszBearoffGenerator [ pbc->bt ] ),
              pbc->nChequers, pbc->nPoints, 
              Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints ),
              pbc->fCubeful ?
              _("database includes both cubeful and cubeless equities") :
              _("cubeless database"),
              pbc->nReads );

  }
  else {

    sprintf ( sz, _(" * %s:\n"
                    "   - generated by %s\n"
                    "   - up to %d chequers on %d points (%d positions)"
                    " per player\n"
                    "%s"
                    "%s"
                    "   - %s\n"
                    "   - number of reads: %lu\n"),
              pbc->fInMemory ?
              _("In memory 1-sided bearoff database evaluator") :
              _("On disk 1-sided bearoff database evaluator"),
              gettext ( aszBearoffGenerator [ pbc->bt ] ),
              pbc->nChequers, pbc->nPoints, 
              Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints ),
              pbc->fND ?
              _("   - distributions are approximated with a "
                "normal distribution\n") : "",
              pbc->fHeuristic ?
              _("   - with heuristic moves ") : "",
              pbc->fGammon ?
              _("database includes gammon distributions") :
              _("database does not include gammon distributions"),
              pbc->nReads );
    
  }
  
}


static void
BearoffDumpTwoSided ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  int i;
  static char *aszEquity[] = {
    N_("Cubeless equity"),
    N_("Owned cube"),
    N_("Centered cube"),
    N_("Opponent owns cube")
  };

  sprintf ( strchr ( sz, 0 ),
            "             Player       Opponent\n"
            "Position %12d  %12d\n\n", 
            nUs, nThem );

  ReadTwoSidedBearoff ( pbc, iPos, ar, NULL );

  if ( pbc->fCubeful )
    for ( i = 0; i < 4 ; ++i )
      sprintf ( strchr ( sz, 0 ),
                "%-30.30s: %+7.4f\n", 
                gettext ( aszEquity[ i ] ), ar[ i ] );
  else
    sprintf ( strchr ( sz, 0 ),
              "%-30.30s: %+7.4f\n",
              gettext ( aszEquity[ 0 ] ), 2.0 * ar[ 0 ] - 1.0f );

  strcat ( sz, "\n" );

}


static void
BearoffDumpOneSided ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints );
  float ar[ 2 ][ 4 ];
  int i;
  float aarProb[ 2 ][ 32 ], aarGammonProb[ 2 ][ 32 ];
  int f0, f1, f2, f3;

  BearoffDist ( pbc, nUs, aarProb[ 0 ], aarGammonProb[ 0 ], ar[ 0 ], 
                NULL, NULL );
  BearoffDist ( pbc, nThem, aarProb[ 1 ], aarGammonProb[ 1 ], ar[ 1 ],
                NULL, NULL );

  sprintf ( strchr ( sz, 0 ),
            "             Player       Opponent\n"
            "Position %12d  %12d\n\n", 
            nUs, nThem );

  strcat( sz, _("Bearing off" "\t\t\t\t"
                "Bearing at least one chequer off\n") );
  strcat( sz, _("Rolls\tPlayer\tOpponent" "\t" 
                "Player\tOpponent\n") );
    
  f0 = f1 = f2 = f3 = FALSE;

  for( i = 0; i < 32; i++ ) {

    if( aarProb[ 0 ][ i ] > 0.0f )
      f0 = TRUE;
    
    if( aarProb[ 1 ][ i ] > 0.0f )
      f1 = TRUE;
    
    if( aarGammonProb[ 0 ][ i ] > 0.0f )
      f2 = TRUE;
    
    if( aarGammonProb[ 1 ][ i ] > 0.0f )
      f3 = TRUE;
    
    if( f0 || f1 || f2 || f3 ) {
      if( f0 && f1 && ( ( f2 && f3 ) || ! pbc->fGammon ) &&
          aarProb[ 0 ][ i ] == 0.0f && aarProb[ 1 ][ i ] == 0.0f &&
          ( ( aarGammonProb[ 0 ][ i ] == 0.0f && 
            aarGammonProb[ 1 ][ i ] == 0.0f ) || ! pbc->fGammon ) )
        break;
      
      sprintf( sz = strchr ( sz, 0 ),
               "%5d\t%7.3f\t%7.3f" "\t\t",
               i, 
               aarProb[ 0 ][ i ] * 100.0f,
               aarProb[ 1 ][ i ] * 100.0f );

      if ( pbc->fGammon )
        sprintf ( sz = strchr ( sz, 0 ),
                  "%7.3f\t%7.3f\n", 
                  aarGammonProb[ 0 ][ i ] * 100.0f,
                  aarGammonProb[ 1 ][ i ] * 100.0f );
      else
        sprintf ( sz = strchr ( sz, 0 ),
                  "%-7.7s\t%-7.7s\n",
                  _("n/a"), _("n/a" ) );
                  
      
    }
  }

  strcat( sz, _("\nAverage rolls\n") );
  strcat( sz, _("Bearing off" "\t\t\t\t"
                "Saving gammon\n") );
  strcat( sz, _("\tPlayer\tOpponent" "\t" 
                "Player\tOpponent\n") );

  /* mean rolls */

  sprintf ( sz = strchr ( sz, 0 ),
            "Mean\t%7.3f\t%7.3f\t\t",
            ar[ 0 ][ 0 ], ar[ 1 ][ 0 ] );

  if ( pbc->fGammon )
    sprintf ( sz = strchr ( sz, 0 ),
              "%7.3f\t%7.3f\n",
              ar[ 0 ][ 2 ], ar[ 1 ][ 2 ] );
  else
    sprintf ( sz = strchr ( sz, 0 ),
              "%-7.7s\t%-7.7s\n",
              _("n/a"), _("n/a" ) );

  /* std. dev */

  sprintf ( sz = strchr ( sz, 0 ),
            "Std dev\t%7.3f\t%7.3f\t\t",
            ar[ 0 ][ 1 ], ar[ 1 ][ 1 ] );

  if ( pbc->fGammon )
    sprintf ( sz = strchr ( sz, 0 ),
              "%7.3f\t%7.3f\n",
              ar[ 0 ][ 3 ], ar[ 1 ][ 3 ] );
  else
    sprintf ( sz = strchr ( sz, 0 ),
              "%-7.7s\t%-7.7s\n",
              _("n/a"), _("n/a" ) );


}


extern void
BearoffDump ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:

    if ( pbc->fTwoSided ) 
      BearoffDumpTwoSided ( pbc, anBoard, sz );
    else
      BearoffDumpOneSided ( pbc, anBoard, sz );

    break;

  case BEAROFF_EXACT_BEAROFF:

    BearoffDumpTwoSided ( pbc, anBoard, sz );
    break;

  default:

    assert ( FALSE );
    break;

  }

}

extern void
BearoffClose ( bearoffcontext *pbc ) {

  if ( ! pbc )
    return;

  if ( ! pbc->fInMemory )
    close ( pbc->h );
  else if ( pbc->p && pbc->fMalloc )
    free ( pbc->p );
  
}


static int
ReadIntoMemory ( bearoffcontext *pbc, const int iOffset, const int nSize ) {

  pbc->fMalloc = TRUE;

#if HAVE_MMAP
  if ( ( pbc->p = mmap ( NULL, nSize, PROT_READ, 
                           MAP_SHARED, pbc->h, iOffset ) ) == (void *) -1 ) {
    /* perror ( "mmap" ); */
    /* mmap failed */
#endif /* HAVE_MMAP */

    /* allocate memory for database */

    if ( ! ( pbc->p = malloc ( nSize ) ) ) {
      perror ( "pbc->p" );
      return -1;
    }

    if ( lseek( pbc->h, iOffset, SEEK_SET ) == (off_t) -1 ) {
      perror ( "lseek" );
      return -1;
    }

    if ( read ( pbc->h, pbc->p, nSize ) < nSize ) {
      if ( errno )
        perror ( "read failed" );
      else
        fprintf ( stderr, _("incomplete bearoff database\n"
                            "(expected size: %d)\n"),
                  nSize );
      free ( pbc->p );
      pbc->p = NULL;
      return -1;
    }

#if HAVE_MMAP
  }
  else
    pbc->fMalloc = FALSE;
#endif /* HAVE_MMAP */

  return 0;

}

/*
 * Check whether this is a exact bearoff file 
 *
 * The first long must be 73457356
 * The second long must be 100
 *
 */

static int
isExactBearoff ( const char ac[ 8 ] ) {

  long id = ( ac[ 0 ] | ac[ 1 ] << 8 | ac[ 2 ] << 16 | ac[ 3 ] << 24 );
  long ver = ( ac[ 4 ] | ac[ 5 ] << 8 | ac[ 6 ] << 16 | ac[ 7 ] << 24 );

  return id == 73457356 && ver == 100;

}


/*
 * Initialise bearoff database
 *
 * Input:
 *   szFilename: the filename of the database to open
 *
 * Returns:
 *   pointer to bearoff context on succes; NULL on error
 *
 * Garbage collect:
 *   caller must free returned pointer if not NULL.
 *
 *
 */

extern bearoffcontext *
BearoffInit ( const char *szFilename, const char *szDir,
              const int bo, void (*pfProgress) ( int ) ) {

  bearoffcontext *pbc;
  char sz[ 41 ];
  int nSize = -1;
  int iOffset = 0;

  if ( bo & BO_HEURISTIC ) {
    
    if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {
      /* malloc failed */
      perror ( "bearoffcontext" );
      return NULL;
    }

    pbc->bt = BEAROFF_GNUBG;
    pbc->fInMemory = TRUE;
    pbc->h = -1;
    pbc->nPoints = 6;
    pbc->nChequers = 6;
    pbc->fTwoSided = FALSE;
    pbc->fCompressed = FALSE;
    pbc->fGammon = FALSE;
    pbc->fND = FALSE;
    pbc->fCubeful = FALSE;
    pbc->nReads = 0;
    pbc->fHeuristic = TRUE;
    pbc->fMalloc = TRUE;
    pbc->p = HeuristicDatabase ( pfProgress );

    return pbc;
    
  }

  

  if ( ! szFilename || ! *szFilename )
    return NULL;

  /*
   * Allocate memory for bearoff context
   */

  if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {
    /* malloc failed */
    perror ( "bearoffcontext" );
    return NULL;
  }

  pbc->fInMemory = bo & BO_IN_MEMORY;

  /*
   * Open bearoff file
   */

  if ( ( pbc->h = PathOpen ( szFilename, szDir, BINARY ) ) < 0 ) {
    /* open failed */
    free ( pbc );
    return NULL;
  }


  /*
   * Read header bearoff file
   */

  /* read header */

  if ( read ( pbc->h, sz, 40 ) < 40 ) {
    if ( errno )
      perror ( szFilename );
    else {
      fprintf ( stderr, _("%s: incomplete bearoff database\n"), szFilename );
      close ( pbc->h );
    }
    free ( pbc );
    return NULL;
  }

  /* detect bearoff program */

  if ( ! strncmp ( sz, "gnubg", 5 ) )
    pbc->bt = BEAROFF_GNUBG;
  else if ( isExactBearoff ( sz ) )
    pbc->bt = BEAROFF_EXACT_BEAROFF;
  else
    pbc->bt = BEAROFF_UNKNOWN;

  switch ( pbc->bt ) {

  case BEAROFF_GNUBG:

    /* one sided or two sided? */

    if ( ! strncmp ( sz + 6, "TS", 2 ) ) 
      pbc->fTwoSided = TRUE;
    else if ( ! strncmp ( sz + 6, "OS", 2 ) )
      pbc->fTwoSided = FALSE;
    else {
      fprintf ( stderr,
                _("%s: incomplete bearoff database\n"
                  "(type is illegal: '%2s')\n"),
                szFilename, sz + 6 );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    /* number of points */

    pbc->nPoints = atoi ( sz + 9 );
    if ( pbc->nPoints < 1 || pbc->nPoints >= 24 ) {
      fprintf ( stderr, 
                _("%s: incomplete bearoff database\n"
                  "(illegal number of points is %d)"), 
                szFilename, pbc->nPoints );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    /* number of chequers */

    pbc->nChequers = atoi ( sz + 12 );
    if ( pbc->nPoints < 1 || pbc->nPoints > 15 ) {
      fprintf ( stderr, 
                _("%s: incomplete bearoff database\n"
                  "(illegal number of chequers is %d)"), 
                szFilename, pbc->nChequers );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    pbc->fCompressed = FALSE;
    pbc->fGammon = FALSE;
    pbc->fCubeful = FALSE;
    pbc->fND = FALSE;
    pbc->fHeuristic = FALSE;

    if ( pbc->fTwoSided ) {
      /* options for two-sided dbs */
      pbc->fCubeful = atoi ( sz + 15 );
    }
    else {
      /* options for one-sided dbs */
      pbc->fGammon = atoi ( sz + 15 );
      pbc->fCompressed = atoi ( sz + 17 );
      pbc->fND = atoi ( sz + 19 );
    }

    iOffset = 0;
    nSize = -1;

    /*
    fprintf ( stderr, "Database:\n"
              "two-sided: %d\n"
              "points   : %d\n"
              "chequers : %d\n"
              "compress : %d\n"
              "gammon   : %d\n"
              "cubeful  : %d\n"
              "normaldis: %d\n"
              "in memory: %d\n",
              pbc->fTwoSided,
              pbc->nPoints, pbc->nChequers,
              pbc->fCompressed, pbc->fGammon, pbc->fCubeful, pbc->fND,
              pbc->fInMemory );*/
    
    break;

  case BEAROFF_EXACT_BEAROFF: 
    {
      long l, m;
      
      l = ( sz[ 8 ] | sz[ 9 ] << 8 | sz[ 10 ] << 16 | sz[ 11 ] << 24 );
      m = ( sz[ 12 ] | sz[ 13 ] << 8 | sz[ 14 ] << 16 | sz[ 15 ] << 24 );
      printf ( "nBottom %ld\n", l );
      printf ( "nTop %ld\n", m );

      if ( m != l ) {
        fprintf ( stderr, _("GNU Backgammon can only read ExactBearoff "
                            "databases with an\n"
                            "equal number of chequers on both sides\n"
                            "(read: botton %ld, top %ld)\n"),
                  l, m );
      }

      if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {
        /* malloc failed */
        perror ( "bearoffcontext" );
        return NULL;
      }

      pbc->bt = BEAROFF_EXACT_BEAROFF;
      pbc->fInMemory = TRUE;
      pbc->nPoints = 6;
      pbc->nChequers = l;
      pbc->fTwoSided = TRUE;
      pbc->fCompressed = FALSE;
      pbc->fGammon = FALSE;
      pbc->fND = FALSE;
      pbc->fCubeful = TRUE;
      pbc->nReads = 0;
      pbc->fHeuristic = FALSE;
      pbc->fMalloc = FALSE;
      pbc->p = NULL;

      iOffset = 0;
      nSize = -1;
    
    }
    break;

  default:

    fprintf ( stderr,
              _("Unknown bearoff database\n" ) );

    close ( pbc->h );
    free ( pbc );
    return NULL;

  }

  /* 
   * read database into memory if requested 
   */

  if ( pbc->fInMemory ) {

    if ( nSize < 0 ) {
      struct stat st;
      if ( fstat ( pbc->h, &st ) ) {
        perror ( szFilename );
        close ( pbc->h );
        free ( pbc );
        return NULL;
      }
      nSize = st.st_size;
    }
    
    if ( ReadIntoMemory ( pbc, iOffset, nSize ) ) {
      
      close ( pbc->h );
      free ( pbc );
      return NULL;

    }
    
    close ( pbc->h );
    pbc->h = -1;
    
  }
  
  pbc->nReads = 0;
  
  return pbc;

}


extern float
fnd ( const float x, const float mu, const float sigma  ) {

   const float epsilon = 1.0e-7;
   const float PI = 3.14159265358979323846;

   if ( sigma <= epsilon )
      /* dirac delta function */
      return ( fabs ( mu - x ) < epsilon ) ? 1.0 : 0.0;
   else {

     float xm = ( x - mu ) / sigma;

     return 1.0 / ( sigma * sqrt ( 2.0 * PI ) ) * exp ( - xm * xm / 2.0 );

   }

}



static void
ReadBearoffOneSidedND ( bearoffcontext *pbc, 
                        const unsigned int nPosID,
                        float arProb[ 32 ], float arGammonProb[ 32 ],
                        float *ar,
                        unsigned short int ausProb[ 32 ], 
                        unsigned short int ausGammonProb[ 32 ] ) {
  
  unsigned char ac[ 16 ];
  float arx[ 4 ];
  int i;
  float r;

  if ( lseek ( pbc->h, 40 + nPosID * 16, SEEK_SET ) < 0 ) {
    perror ( "OS bearoff database" );
    return;
  }

  if ( read ( pbc->h, ac, 16 ) < 16 ) {
    if ( errno )
      perror ( "OS bearoff database" );
    else
      fprintf ( stderr, "error reading OS bearoff database" );
    return;
  }

  memcpy ( arx, ac, 16 );

  if ( arProb || ausProb )
     for ( i = 0; i < 32; ++i ) {
       r = fnd ( 1.0f * i, arx[ 0 ], arx[ 1 ] );
       if ( arProb )
         arProb[ i ] = r;
       if ( ausProb )
         ausProb[ i ] = r * 65535.0f;
     }
     


  if ( arGammonProb || ausGammonProb )
    for ( i = 0; i < 32; ++i ) {
      r = fnd ( 1.0f * i, arx[ 2 ], arx[ 3 ] );
      if ( arGammonProb )
        arGammonProb[ i ] = r;
      if ( ausGammonProb )
        ausGammonProb[ i ] = r * 65535.0f;
    }
  
  if ( ar )
    memcpy ( ar, arx, 16 );

  ++pbc->nReads;

}


static void
ReadBearoffOneSidedExact ( bearoffcontext *pbc, const unsigned int nPosID,
                           float arProb[ 32 ], float arGammonProb[ 32 ],
                           float ar[ 4 ],
                           unsigned short int ausProb[ 32 ], 
                           unsigned short int ausGammonProb[ 32 ] ) {

  if ( pbc->fInMemory ) {

    if ( ! pbc->fCompressed ) {

      int i, j;
      float arP[ 32 ], arGP[ 32 ];
      unsigned short int us;
      unsigned char *ac = (unsigned char *) pbc->p;
 
      i = 64 * nPosID * ( pbc->fGammon  ? 2 : 1 );
      for ( j = 0; j < 32; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i + 1 ] << 8;
        arP[ j ] = us / 65535.0f;
        if ( ausProb )
          ausProb[ j ] = us;
      }

      memset ( arGP, 0, 32 * sizeof ( float ) );
      if ( pbc->fGammon ) {
        for ( j = 0; j < 32; ++j, i += 2 ) {
          us = ac[ i ] | ac[ i+1 ] << 8;
          arGP[ j ] = us / 65535.0f;
          if ( ausGammonProb )
            ausGammonProb[ j ] = us;
        }
      }
      else if ( ausGammonProb )
        memset ( ausGammonProb, 0, 64 );
        

      if ( arProb )
        memcpy ( arProb, arP, sizeof ( float ) * 32 );

      if ( arGammonProb )
        memcpy ( arGammonProb, arGP, sizeof ( float ) * 32 );

      if ( ar ) {
        AverageRolls ( arP, ar );
        AverageRolls ( arGP, ar + 2 );
      }


    }
    else {
      unsigned char *puch;
      int i, j;
      off_t iOffset;
      int nBytes;
      int nPos = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
      float arP[ 32 ], arGP[ 32 ];
      
      unsigned int ioff, nz, ioffg, nzg;

      unsigned short int us;

      /* find offsets and no. of non-zero elements */

      puch = ( (unsigned char *) pbc->p ) + 40 + nPosID * 8;

      iOffset = 
        puch[ 0 ] | 
        puch[ 1 ] << 8 |
        puch[ 2 ] << 16 |
        puch[ 3 ] << 24;
    
      nz = puch[ 4 ];
      ioff = puch[ 5 ];
      nzg = puch[ 6 ];
      ioffg = puch[ 7 ];
    
      /* read prob + gammon probs */
    
      iOffset = 40     /* the header */
        + nPos * 8     /* the offset data */
        + 2 * iOffset; /* offset to current position */
    
      /* read values */
    
      nBytes = 2 * ( nz + nzg );

      puch = ( ( unsigned char *) pbc->p ) + iOffset;
    
      i = 0;
      memset ( arP, 0, 32 * sizeof ( float ) );
      if ( ausProb )
        memset ( ausProb, 0, 64 );
      for ( j = 0; j < nz; ++j, i += 2 ) {
        us = puch[ i ] | puch[ i + 1 ] << 8;
        arP[ ioff + j ] = us / 65535.0f;
        if ( ausProb )
          ausProb[ ioff + j ] = us;
      }


      memset ( arGP, 0, 32 * sizeof ( float ) );
      if ( ausGammonProb )
        memset ( ausGammonProb, 0, 64 );
      for ( j = 0; j < nzg; ++j, i += 2 ) {
        us = puch[ i ] | puch[ i+1 ] << 8;
        arGP[ ioffg + j ] = us / 65535.0f;
        if ( ausGammonProb )
          ausGammonProb[ ioffg + j ] = us;
      }
    
      if ( arProb )
        memcpy ( arProb, arP, sizeof ( float ) * 32 );

      if ( arGammonProb )
        memcpy ( arGammonProb, arGP, sizeof ( float ) * 32 );

      if ( ar ) {
        AverageRolls ( arP, ar );
        AverageRolls ( arGP, ar + 2 );
      }

    }

  }
  else {

    /* read from disk */

    if ( pbc->fCompressed ) {

      unsigned char ac[ 128 ];
      int i, j;
      off_t iOffset;
      int nBytes;
      int nPos = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
      float arP[ 32 ], arGP[ 32 ];
      
      unsigned int ioff, nz, ioffg, nzg;

      unsigned short int us;

      /* find offsets and no. of non-zero elements */

      if ( lseek ( pbc->h, 40 + nPosID * 8, SEEK_SET ) < 0 ) {
        perror ( "OS bearoff database" );
        return;
      }

      if ( read ( pbc->h, ac, 8 ) < 8 ) {
        if ( errno )
          perror ( "OS bearoff database" );
        else
          fprintf ( stderr, "error reading OS bearoff database" );
        return;
      }
    
      iOffset = 
        ac[ 0 ] | 
        ac[ 1 ] << 8 |
        ac[ 2 ] << 16 |
        ac[ 3 ] << 24;
    
      nz = ac[ 4 ];
      ioff = ac[ 5 ];
      nzg = ac[ 6 ];
      ioffg = ac[ 7 ];
    
      /* read prob + gammon probs */
    
      iOffset = 40     /* the header */
        + nPos * 8     /* the offset data */
        + 2 * iOffset; /* offset to current position */
    
      /* read values */
    
      nBytes = 2 * ( nz + nzg );
    
      if ( lseek ( pbc->h, iOffset, SEEK_SET ) < 0 ) {
        perror ( "OS bearoff database" );
        return;
      }
    
      if ( read ( pbc->h, ac, nBytes ) < nBytes ) {
        if ( errno )
          perror ( "OS bearoff database" );
        else
          fprintf ( stderr, "error reading OS bearoff database" );
        return;
      }
    
      i = 0;
      memset ( arP, 0, 32 * sizeof ( float ) );
      if ( ausProb )
        memset ( ausProb, 0, 64 );
      for ( j = 0; j < nz; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i + 1 ] << 8;
        arP[ ioff + j ] = us / 65535.0f;
        if ( ausProb )
          ausProb[ ioff + j ] = us;
      }


      memset ( arGP, 0, 32 * sizeof ( float ) );
      if ( ausGammonProb )
        memset ( ausGammonProb, 0, 64 );
      for ( j = 0; j < nzg; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i+1 ] << 8;
        arGP[ ioffg + j ] = us / 65535.0f;
        if ( ausGammonProb )
          ausGammonProb[ ioffg + j ] = us;
      }
    
      if ( arProb )
        memcpy ( arProb, arP, sizeof ( float ) * 32 );

      if ( arGammonProb )
        memcpy ( arGammonProb, arGP, sizeof ( float ) * 32 );

      if ( ar ) {
        AverageRolls ( arP, ar );
        AverageRolls ( arGP, ar + 2 );
      }

    }
    else {

      unsigned char ac[ 128 ];
      int i, j;
      float arP[ 32 ], arGP[ 32 ];
      unsigned int us;

      lseek ( pbc->h, 40 + nPosID * ( pbc->fGammon ? 128 : 64 ), SEEK_SET );

      read ( pbc->h, ac, pbc->fGammon ? 128 : 64 );

      i = 0;
      memset ( arP, 0, 32 * sizeof ( float ) );
      if ( ausProb )
        memset ( ausProb, 0, 64 );
      for ( j = 0; j < 32; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i + 1 ] << 8;
        arP[ j ] = us / 65535.0f;
        if ( ausProb )
          ausProb[ j ] = us;
      }


      if ( ausGammonProb )
        memset ( ausGammonProb, 0, 64 );
      for ( j = 0; j < 32; ++j, i += 2 ) {
        us = ac[ i ] | ac[ i+1 ] << 8;
        arGP[ j ] = us / 65535.0f;
        if ( ausGammonProb )
          ausGammonProb[ j ] = us;
      }

      if ( arProb )
        memcpy ( arProb, arP, sizeof ( float ) * 32 );

      if ( arGammonProb )
        memcpy ( arGammonProb, arGP, sizeof ( float ) * 32 );

      if ( ar ) {
        AverageRolls ( arP, ar );
        AverageRolls ( arGP, ar + 2 );
      }

    }
      
  }

  ++pbc->nReads;

}


extern void
BearoffDist ( bearoffcontext *pbc, const unsigned int nPosID,
              float arProb[ 32 ], float arGammonProb[ 32 ],
              float ar[ 4 ],
              unsigned short int ausProb[ 32 ], 
              unsigned short int ausGammonProb[ 32 ] ) {

  switch ( pbc->bt ) {
  case BEAROFF_GNUBG:

    if ( pbc->fTwoSided ) {
      assert ( FALSE );
    }
    else {
      /* one-sided db */
      if ( pbc->fND ) 
        ReadBearoffOneSidedND ( pbc, nPosID, arProb, arGammonProb, ar,
                                ausProb, ausGammonProb );
      else
        ReadBearoffOneSidedExact ( pbc, nPosID, arProb, arGammonProb, ar,
                                   ausProb, ausGammonProb );
    }
    break;

  default:
    assert ( FALSE );
    break;
  }

}


extern int
isBearoff ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ] ) {

  int nOppBack = -1, nBack = -1;
  int n = 0, nOpp = 0;
  int i;

  if ( ! pbc )
    return FALSE;

  for( nOppBack = 24; nOppBack >= 0; --nOppBack)
    if( anBoard[0][nOppBack] )
      break;

  for(nBack = 24; nBack >= 0; --nBack)
    if( anBoard[1][nBack] )
      break;

  if ( nBack < 0 || nOppBack < 0 )
    /* the game is over */
    return FALSE;

  if ( nBack + nOppBack > 22 )
    /* contact position */
    return FALSE;

  for ( i = 0; i <= nOppBack; ++i )
    nOpp += anBoard[ 0 ][ i ];

  for ( i = 0; i <= nBack; ++i )
    n += anBoard[ 1 ][ i ];

  if ( n <= pbc->nChequers && nOpp <= pbc->nChequers &&
       nBack < pbc->nPoints && nOppBack < pbc->nPoints )
    return TRUE;
  else
    return FALSE;

}
