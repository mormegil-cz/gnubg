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

#include <glib/gutils.h>

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
#if HAVE_LIBGEN_H
#include <libgen.h>
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


typedef struct _hashentryonesided {
  unsigned int nPosID;
  unsigned short int aus[ 64 ];
} hashentryonesided;

typedef struct _hashentrytwosided {
  unsigned int nPosID;
  unsigned char auch[ 8 ];
} hashentrytwosided;

typedef struct _hashentryhypergammon {
  unsigned int nPosID;
  unsigned char auch[ 18 ];
} hashentryhypergammon;


typedef int ( *hcmpfunc ) ( void *p1, void *p2 );

static int anCacheSize[] = { 100000, 10000, 5000 };


char *aszBearoffGenerator[ NUM_BEAROFFS ] = {
  N_("GNU Backgammon"),
  N_("ExactBearoff"),
  N_("Hugh Sconyers"),
  N_("Unknown program")
};


static int 
ReadSconyers15x15( bearoffcontext *pbc,
                   const unsigned int iPos,
                   float arOutput[], float arEquity[] );


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


static int
hcmpOneSided( void *p1, void *p2 ) {

  hashentryonesided *ph1 = p1;
  hashentryonesided *ph2 = p2;

  if ( ph1->nPosID < ph2->nPosID )
    return -1;
  else if ( ph1->nPosID == ph2->nPosID )
    return 0;
  else
    return 1;

}

static int
hcmpTwoSided( void *p1, void *p2 ) {


  hashentrytwosided *ph1 = p1;
  hashentrytwosided *ph2 = p2;

  if ( ph1->nPosID < ph2->nPosID )
    return -1;
  else if ( ph1->nPosID == ph2->nPosID )
    return 0;
  else
    return 1;

}

static int
hcmpHypergammon ( void *p1, void *p2 ) {

  hashentryhypergammon *ph1 = p1;
  hashentryhypergammon *ph2 = p2;

  if ( ph1->nPosID < ph2->nPosID )
    return -1;
  else if ( ph1->nPosID == ph2->nPosID )
    return 0;
  else
    return 1;

}

static hcmpfunc ahcmp[] = { hcmpOneSided, hcmpTwoSided, hcmpHypergammon };



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

    return PositionBearoff( anBoard, 6, 15 );
}

static void GenerateBearoff( unsigned char *p, int nId ) {

    int i, iBest, anRoll[ 2 ], anBoard[ 6 ], aProb[ 32 ];
    unsigned short us;

    for( i = 0; i < 32; i++ )
	aProb[ i ] = 0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ ) {
	    PositionFromBearoff( anBoard, nId, 6, 15 );
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


static unsigned long
HashTwoSided ( const unsigned int nPosID ) {
  return nPosID;
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
  unsigned char *pc = NULL;
  unsigned short int us;

  /* look up in cache */

  if ( ! pbc->fInMemory && pbc->ph ) {

    hashentrytwosided *phe;
    hashentrytwosided he;
    
    he.nPosID = iPos;
    if ( ( phe = HashLookup ( pbc->ph, HashTwoSided ( iPos ), &he ) ) ) 
      pc = phe->auch;
      
  }

  if ( ! pc ) {

    if ( pbc->fInMemory )
      pc = ((char *) pbc->p)+ 40 + 2 * iPos * k;
    else {
      lseek ( pbc->h, 40 + 2 * iPos * k, SEEK_SET );
      read ( pbc->h, ac, k * 2 );
      pc = ac;
    }

    /* add to cache */

    if ( ! pbc->fInMemory && pbc->ph ) {
      /* add to cache */
      hashentrytwosided *phe = 
        (hashentrytwosided *) malloc ( sizeof ( hashentrytwosided ) );
      if ( phe ) {
        phe->nPosID = iPos;
        memcpy ( phe->auch, pc, sizeof ( phe->auch ) );
        HashAdd ( pbc->ph, HashTwoSided ( iPos ), phe );
      }
      
    }
    
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
    PositionFromBearoff ( anBoard[ 0 ], i, pbc->nPoints, pbc->nChequers );
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
      


  PositionFromBearoff ( anBoard[ 0 ], nThem, pbc->nPoints, pbc->nChequers );
  PositionFromBearoff ( anBoard[ 1 ], nUs, pbc->nPoints, pbc->nChequers  );

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

  switch ( pbc->bc ) {
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

  case BEAROFF_SCONYERS:

    assert( aus == NULL );

    switch( pbc->hsdb ) {
    case HS_15x15_ON_DISK:
    case HS_15x15_ON_DVDS:
      return ReadSconyers15x15( pbc, iPos, NULL, ar );
      break;
    default:
      assert( FALSE );
      break;
    }
    break;

  default:

    assert ( FALSE );
    break;

  }

  /* code not reachable */
  return 0;

}


static int
BearoffEvalTwoSided ( bearoffcontext *pbc, 
                      int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  
  if ( ReadTwoSidedBearoff ( pbc, iPos, ar, NULL ) )
    return -1;

  memset ( arOutput, 0, 5 * sizeof ( float ) );
  arOutput[ OUTPUT_WIN ] = ar[ 0 ] / 2.0f + 0.5;
  
  return 0;

}


static int
ReadHypergammon( bearoffcontext *pbc,
                 const unsigned int iPos,
                 float arOutput[ NUM_OUTPUTS ],
                 float arEquity[ 4 ] ) {

  unsigned char ac[ 28 ];
  unsigned char *pc = NULL;
  unsigned int us;
  int i;
  const int x = 28;

  if ( pbc->fInMemory )
    pc = ((char *) pbc->p)+ 40 + x * iPos;
  else {
    lseek ( pbc->h, 40 + x * iPos, SEEK_SET );
    read ( pbc->h, ac, x );
    pc = ac;
  }

  if ( arOutput )
    for ( i = 0; i < NUM_OUTPUTS; ++i ) {
      us = pc[ 3 * i ] | ( pc[ 3 * i + 1 ] ) << 8 | ( pc[ 3 * i + 2 ] ) << 16;
      arOutput[ i ] = us / 16777215.0;
    }

  if ( arEquity )
    for ( i = 0; i < 4; ++i ) {
      us = pc[ 15 + 3 * i ] | 
         ( pc[ 15 + 3 * i + 1 ] ) << 8 | 
         ( pc[ 15 + 3 * i + 2 ] ) << 16;
      arEquity[ i ] = ( us / 16777215.0f - 0.5f ) * 6.0f;
    }

  ++pbc->nReads;

  return 0;

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

    an[ i ] = PositionBearoff ( anBoard[ i ], pbc->nPoints, pbc->nChequers );
    if ( BearoffDist ( pbc, an[ i ], aarProb[ i ], 
                       aarGammonProb[ i ], ar [ i ], NULL, NULL ) )
      return -1;

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

  return 0;

}


extern int
BearoffHyper( bearoffcontext *pbc,
              const unsigned int iPos,
              float arOutput[], float arEquity[] ) {

  return ReadHypergammon( pbc, iPos, arOutput, arEquity );

}


static int
BearoffEvalHypergammon ( bearoffcontext *pbc, 
                         int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;

  return ReadHypergammon ( pbc, iPos, arOutput, NULL );

}


static int 
ReadSconyers15x15( bearoffcontext *pbc,
                   const unsigned int iPos,
                   float arOutput[], float arEquity[] ) {

  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int nFile = ( iPos / n ) / 1250;
  char sz[ 15 ];
  char *pch, *pch2;
  long lPos;
  unsigned char ac[ 16 ];
  float ar[ 4 ];
  float *pr;
  long l;
  int i;
  int h = -1;

  switch( pbc->hsdb ) {
  case HS_15x15_ON_DVDS:

    if ( pbc->nCurrentFile != nFile ) {

      if ( pbc->nCurrentFile != -1 ) 
        close( pbc->h );
      
      sprintf( sz, "bg15_%d.dat", nFile + 1 );
      if ( ( pbc->h = PathOpen( sz, pbc->szDir, BINARY ) ) < 0 ) {
        /* could not open file... */
        do {
          
          pch = (*pbc->pfDJ)( _("Please insert disk with file %s"), 
                              TRUE, sz );

          if ( fInterrupt ) {
            pbc->nCurrentFile = -1;
            return -1;
          }

          if ( pch ) {
            /* new path */
          
            /* FIXME: redo this... */
          
            pch2 = strdup( pch );
          
            if( pbc->szDir )
              free( pbc->szDir );

#if ! defined(HAVE_DIRNAME) && ! defined(HAVE_LIBGEN_H)
            pbc->szDir = (char *) g_path_get_dirname( (const gchar *) pch );
#else
            pbc->szDir = dirname( pch );
#endif

            free( pch );
            free( pch2 );

          }
          
          printf( "attempt to open '%s' '%s'\n",
                  sz, pbc->szDir );
          if ( ( pbc->h = PathOpen( sz, pbc->szDir, BINARY ) ) >= 0 )
            break;
          
        } while( 1 );

      }
      
    }

    pbc->nCurrentFile = nFile;
    h = pbc->h;

    break;

  case HS_15x15_ON_DISK:

    if ( ( h = pbc->ah[ nFile ] ) < 0 ) {

      /* file not open: open it */
      
      sprintf( sz, "bg15_%d.dat", nFile + 1 );
      if ( ( h = pbc->ah[ nFile ] = PathOpen( sz, pbc->szDir, 
                                              BINARY ) ) < 0 ) {
        perror( sz );
      return -1;
      }

    }

    break;


  default:

    assert( FALSE );

  }

  /* Position number */

  lPos = iPos;

  /* printf( "lPos %ld, disk no. %d\n", lPos, nFile ); */

  /* 
   * Hugh does not score positions will all chequers off:
   * gnubg calculate position# as 54264 * nUs + nThem.
   * Hugh uses                    54263 * ( nUs -1 ) + ( nThem - 1 )
   * The difference is:           nUs + 54263 + 1
   */

  lPos -= ( ( iPos / 54264 ) + ( n - 1 ) + 1 );

  /*
   * Each disk contains 1250 * 54263 positions.
   * Subtract disk number minus 1 times the number of positions per disk.
   */

  lPos -= nFile * 1250 * ( n - 1 );

  /* Each position uses 16 bytes (4 floats of 4 bytes each) */

  printf( "modified lPos %ld\n", lPos ); 

  lPos *= 16L;

  /* Seek to calculated offset */

  if ( ( l = lseek( h, lPos, SEEK_SET ) ) < 0 ) {
    perror( "lseek (15x15)" );
    return -1;
  }

  /* printf( "new offset %ld\n", l );  */

  /* Read the 16 bytes for this position */

  if ( ( l = read( h, ac, 16 ) ) < 16 ) {
    printf( "bytes read: %ld, expected %d\n", l, 16 );
    if ( errno )
      perror( "read (15x15)" );
    return -1;
  }


  for ( i = 0; i < 4; ++i ) {
    /* FIXME: handle little endian/big endian */
    l = ac[ 4 * i + 0 ] | 
      ( ac[ 4 * i + 1 ] << 8 ) |
      ( ac[ 4 * i + 2 ] << 16 ) |
      ( ac[ 4 * i + 3 ] << 24 );
    pr = (float *) &l;
    ar[ i ] = *pr;
  }

  /* Save equities */

  if ( arEquity )
    memcpy( arEquity, ar, 4 * sizeof ( float ) );

  if ( arOutput ) {
    memset( arOutput, 0, 5 * sizeof ( float ) );
    arOutput[ 0 ] = ar[ 0 ] / 2.0f + 0.5f;
    /* FIXME: what do we do when gammons are possible??? */
  }

  ++pbc->nReads;

  return 0;

}


static int
BearoffEvalSconyers( bearoffcontext *pbc, 
                     int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  unsigned int nUs = 
    PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = 
    PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  unsigned int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;

  switch( pbc->hsdb ) {
  case HS_15x15_ON_DISK:
  case HS_15x15_ON_DVDS:
    return ReadSconyers15x15( pbc, iPos, arOutput, NULL );
    break;
  default:
    assert( FALSE );
    break;
  }

  /* code not reachable */
  return 0;

}


extern int
BearoffEval ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], float arOutput[] ) {

  switch ( pbc->bc ) {
  case BEAROFF_GNUBG:

    switch ( pbc->bt ) {
    case BEAROFF_TWOSIDED:
      return BearoffEvalTwoSided ( pbc, anBoard, arOutput );
      break;
    case BEAROFF_ONESIDED:
      return BearoffEvalOneSided ( pbc, anBoard, arOutput );
      break;
    case BEAROFF_HYPERGAMMON:
      return BearoffEvalHypergammon ( pbc, anBoard, arOutput );
      break;
    }

    break;

  case BEAROFF_SCONYERS:

    return BearoffEvalSconyers( pbc, anBoard, arOutput );
    break;

  case BEAROFF_EXACT_BEAROFF:

    assert ( pbc->bt == BEAROFF_TWOSIDED );
    return BearoffEvalTwoSided ( pbc, anBoard, arOutput );
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

  switch ( pbc->bt ) {
  case BEAROFF_TWOSIDED:

    sprintf ( sz, _(" * %s:\n"
                    "   - generated by %s\n"
                    "   - up to %d chequers on %d points (%d positions)"
                    " per player\n"
                    "   - %s\n"
                    "   - number of reads: %lu\n"),
              pbc->fInMemory ?
              _("In memory 2-sided bearoff database evaluator") :
              _("On disk 2-sided bearoff database evaluator"),
              gettext ( aszBearoffGenerator [ pbc->bc ] ),
              pbc->nChequers, pbc->nPoints, 
              Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints ),
              pbc->fCubeful ?
              _("database includes both cubeful and cubeless equities") :
              _("cubeless database"),
              pbc->nReads );
    break;

  case BEAROFF_ONESIDED:

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
              gettext ( aszBearoffGenerator [ pbc->bc ] ),
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
    
    break;

  case BEAROFF_HYPERGAMMON:

    if ( pbc->fInMemory )
      sprintf( sz, _(" * In memory 2-sided exact %d-chequer Hypergammon "
                     "database evaluator\n"), pbc->nChequers );
    else
      sprintf( sz, _(" * On disk 2-sided exact %d-chequer Hypergammon "
                     "database evaluator\n"), pbc->nChequers );

    sprintf( strchr( sz, 0 ),
             _("   - generated by %s\n"
               "   - up to %d chequers on %d points (%d positions)"
               " per player\n"
               "   - number of reads: %lu\n"),
             gettext ( aszBearoffGenerator [ pbc->bc ] ),
             pbc->nChequers, pbc->nPoints, 
             Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints ),
             pbc->nReads );
    
    break;

  }

  if ( pbc->ph )
    sprintf ( strchr ( sz, 0 ),
              _("   - cache: size %lu entries, %lu look-ups "
                "(%lu hits, and %lu misses)\n"),
              pbc->ph->cSize, pbc->ph->cLookups, 
              pbc->ph->cHits, pbc->ph->cMisses );

}


static int
BearoffDumpTwoSided ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
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

  if( ReadTwoSidedBearoff ( pbc, iPos, ar, NULL ) )
    return -1;

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

  return 0;

}


static int
BearoffDumpOneSided ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  float ar[ 2 ][ 4 ];
  int i;
  float aarProb[ 2 ][ 32 ], aarGammonProb[ 2 ][ 32 ];
  int f0, f1, f2, f3;
  int anPips[ 2 ];
  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
              5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
              1 * 16 + 1 * 20 + 1 * 24 ) / 36.0;


  if ( BearoffDist ( pbc, nUs, aarProb[ 0 ], aarGammonProb[ 0 ], ar[ 0 ], 
                     NULL, NULL ) )
    return -1;
  if ( BearoffDist ( pbc, nThem, aarProb[ 1 ], aarGammonProb[ 1 ], ar[ 1 ],
                     NULL, NULL ) )
    return -1;

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


  /* effective pip count */

  PipCount( anBoard, anPips );

  strcat( strchr( sz, 0 ), _("\nEffective pip count:\n" ) );
  strcat( strchr( sz, 0 ), _("\tPlayer\tOpponent\n" ) );
  sprintf( strchr( sz, 0 ), _("EPC\t%7.3f\t%7.3f\n"
                              "Wastage\t%7.3f\t%7.3f\n\n" ),
           ar[ 0 ][ 0 ] * x, ar[ 1 ][ 0 ] * x,
           ar[ 0 ][ 0 ] * x - anPips[ 1 ],
           ar[ 1 ][ 0 ] * x - anPips[ 0 ] );

  sprintf( strchr( sz, 0 ),
          _("EPC = %5.3f * Average rolls\n"
            "Wastage = EPC - pips\n\n" ), x );

  return 0;

}


static int
BearoffDumpHyper( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  int i;
  static char *aszEquity[] = {
    N_("Owned cube"),
    N_("Centered cube"),
    N_("Centered cube (Jacoby rule)"),
    N_("Opponent owns cube")
  };

  if ( BearoffHyper ( pbc, iPos, NULL, ar ) )
    return -1;

  sprintf ( strchr ( sz, 0 ),
            "             Player       Opponent\n"
            "Position %12d  %12d\n\n", 
            nUs, nThem );

  for ( i = 0; i < 4 ; ++i )
    sprintf ( strchr ( sz, 0 ),
              "%-30.30s: %+7.4f\n", 
              gettext ( aszEquity[ i ] ), ar[ i ] );

  return 0;

}


extern int
BearoffDumpSconyers15x15( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ],
                          char *sz ) {

  int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  int iPos = nUs * n + nThem;
  float ar[ 4 ];
  int i;
  static char *aszEquity[] = {
    N_("Cubeless"),
    N_("Owned cube"),
    N_("Centered cube"),
    N_("Opponent owns cube")
  };


  if ( ReadSconyers15x15( pbc, iPos, NULL, ar ) )
    return -1;

  sprintf ( strchr ( sz, 0 ),
            "             Player       Opponent\n"
            "Position %12d  %12d\n\n", 
            nUs, nThem );

  for ( i = 0; i < 4 ; ++i )
    sprintf ( strchr ( sz, 0 ),
              "%-30.30s: %+7.4f\n", 
              gettext ( aszEquity[ i ] ), ar[ i ] );

  return 0;

}


extern int
BearoffDump ( bearoffcontext *pbc, int anBoard[ 2 ][ 25 ], char *sz ) {

  switch ( pbc->bc ) {
  case BEAROFF_GNUBG:

    switch ( pbc->bt ) {
    case BEAROFF_TWOSIDED:
      return BearoffDumpTwoSided ( pbc, anBoard, sz );
      break;
    case BEAROFF_ONESIDED:
      return BearoffDumpOneSided ( pbc, anBoard, sz );
      break;
    case BEAROFF_HYPERGAMMON:
      return BearoffDumpHyper ( pbc, anBoard, sz );
      break;
    }

    break;

  case BEAROFF_EXACT_BEAROFF:

    return BearoffDumpTwoSided ( pbc, anBoard, sz );
    break;

  case BEAROFF_SCONYERS:

    switch( pbc->hsdb ) {
    case HS_15x15_ON_DISK:
    case HS_15x15_ON_DVDS:
      return BearoffDumpSconyers15x15( pbc, anBoard, sz );
      break;
    default:
      assert( FALSE );
    }
    break;

  default:

    assert ( FALSE );
    break;

  }

  /* code not reachable */
  return 0;

}

extern void
BearoffClose ( bearoffcontext **ppbc ) {

  if ( ! ppbc || ! *ppbc )
    return;

  if ( ! ((*ppbc))->fInMemory )
    close ( (*ppbc)->h );
  else if ( (*ppbc)->p && (*ppbc)->fMalloc )
    free ( (*ppbc)->p );

  if ( (*ppbc)->szDir )
    free( (*ppbc)->szDir );

  if ( (*ppbc)->szFilename )
    free( (*ppbc)->szFilename );

  if ( (*ppbc)->ah ) {
    int i;
    for ( i = 0; i < (*ppbc)->nFiles; ++i )
      close( (*ppbc)->ah[ i ] );
    free( (*ppbc)->ah );
  }

  free( (*ppbc) );
  
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


bearoffcontext *
BearoffAlloc( void ) {

  bearoffcontext *pbc;

  if( ! ( pbc = ( bearoffcontext *) malloc( sizeof ( bearoffcontext ) ) ) )
    return NULL;
  
  pbc->h = -1;
  pbc->ah = NULL;
  pbc->nFiles = 0;
  pbc->bt = -1;
  pbc->bc = -1;
  pbc->nPoints = -1;
  pbc->nChequers = -1;
  pbc->fInMemory = FALSE;
  pbc->fMalloc = FALSE;
  pbc->szDir = NULL;
  pbc->szFilename = NULL;
  pbc->fCompressed = TRUE;
  pbc->fGammon = TRUE;
  pbc->fND = FALSE;
  pbc->fHeuristic = FALSE;
  pbc->nOffsetBuffer = -1;
  pbc->puchBuffer = NULL;
  pbc->nOffsetA = -1;
  pbc->puchA = NULL;
  pbc->fCubeful = TRUE;
  pbc->hsdb = -1;
  pbc->pfDJ = NULL;
  pbc->nCurrentFile = -1;
  pbc->p = NULL;
  pbc->ph = NULL;

  return pbc;

}



/*
 * Setup a bearoff context for Hugh Sconyers bearoff databases.
 * So far only the huuuuge 15x15 database is supported.
 * The pointer p is a function to be called when a new disk should
 * be inserted.
 *
 */

static bearoffcontext *
BearoffInitSconyers( const char *szFilename, const char *szDir,
                     const int bo, void *p ) {

  bearoffcontext *pbc;
  int i;
  char  sz[ 15 ];

  if ( ! ( pbc = BearoffAlloc() ) )
    return NULL;

  if ( ( bo & BO_SCONYERS_15x15 ) && ( bo & BO_ON_DISK ) ) {

    pbc->bt = BEAROFF_TWOSIDED;
    pbc->bc = BEAROFF_SCONYERS;
    pbc->nPoints = 6;
    pbc->nChequers = 15;
    pbc->fInMemory = FALSE; /* are you nuts? 47 Gigs :-) */
    pbc->fCubeful = TRUE; /* yup, it has cubeful equities as well */
    pbc->hsdb = HS_15x15_ON_DISK;
    pbc->nCurrentFile = -1;
    pbc->pfDJ = NULL;
    pbc->szDir = szDir ? strdup( szDir ) : NULL;
    pbc->szFilename = NULL;
    pbc->ph = NULL;
    pbc->ah = (int *) malloc( 44 * sizeof( int ) );
    /* open all 44 files... */
    /* FIXME: considering just checking that all files are there */
    for ( i = 0; i < 44; ++i ) {
      sprintf( sz, "bg15_%d.dat", i + 1 );
      if ( ( pbc->ah[ i ] = PathOpen( sz, pbc->szDir, BINARY ) ) < 0 ) {
        /* could not open file */
        perror( sz );
        BearoffClose( &pbc );
        return NULL;
      }
    }


    return pbc;

  }
  else if ( ( bo & BO_SCONYERS_15x15 ) && ( bo & BO_ON_DVDS ) ) {

    pbc->bt = BEAROFF_TWOSIDED;
    pbc->bc = BEAROFF_SCONYERS;
    pbc->nPoints = 6;
    pbc->nChequers = 15;
    pbc->fInMemory = FALSE; /* are you nuts? 47 Gigs :-) */
    pbc->fCubeful = TRUE; /* yup, it has cubeful equities as well */
    pbc->hsdb = HS_15x15_ON_DVDS;
    pbc->nCurrentFile = -1;
    pbc->pfDJ = p; /* pointer to the disk jockey function */
    pbc->szDir = szDir ? strdup( szDir ) : NULL;
    pbc->szFilename = NULL;
    pbc->ph = NULL;
    pbc->ah = NULL;

    return pbc;

  }
  else {

    assert ( FALSE );

  }

  return NULL;

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
              const int bo, void *p ) {

  bearoffcontext *pbc;
  char sz[ 41 ];
  int nSize = -1;
  int iOffset = 0;

  if ( bo & BO_SCONYERS_15x15 ) 
    return BearoffInitSconyers( szFilename, szDir, bo, p );

  if ( bo & BO_HEURISTIC ) {
    
    if ( ! ( pbc = BearoffAlloc() ) ) {
      /* malloc failed */
      perror ( "bearoffcontext" );
      return NULL;
    }

    pbc->bc = BEAROFF_GNUBG;
    pbc->bt = BEAROFF_ONESIDED;
    pbc->fInMemory = TRUE;
    pbc->h = -1;
    pbc->nPoints = 6;
    pbc->nChequers = 6;
    pbc->fCompressed = FALSE;
    pbc->fGammon = FALSE;
    pbc->fND = FALSE;
    pbc->fCubeful = FALSE;
    pbc->nReads = 0;
    pbc->fHeuristic = TRUE;
    pbc->fMalloc = TRUE;
    pbc->p = HeuristicDatabase ( p );
    pbc->ph = NULL;
    pbc->szDir = szDir ? strdup( szDir ) : NULL;
    pbc->szFilename = szFilename ? strdup( szFilename ) : NULL;

    return pbc;
    
  }

  

  if ( ! szFilename || ! *szFilename )
    return NULL;

  /*
   * Allocate memory for bearoff context
   */

  if ( ! ( pbc = BearoffAlloc() ) ) {
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
    pbc->bc = BEAROFF_GNUBG;
  else if ( isExactBearoff ( sz ) )
    pbc->bc = BEAROFF_EXACT_BEAROFF;
  else
    pbc->bc = BEAROFF_UNKNOWN;

  pbc->szDir = szDir ? strdup( szDir ) : NULL;
  pbc->szFilename = szFilename ? strdup( szFilename ) : NULL;

  switch ( pbc->bc ) {

  case BEAROFF_GNUBG:

    /* one sided or two sided? */

    if ( ! strncmp ( sz + 6, "TS", 2 ) ) 
      pbc->bt = BEAROFF_TWOSIDED;
    else if ( ! strncmp ( sz + 6, "OS", 2 ) )
      pbc->bt = BEAROFF_ONESIDED;
    else if ( *(sz+6) == 'H' )
      pbc->bt = BEAROFF_HYPERGAMMON;
    else {
      fprintf ( stderr,
                _("%s: incomplete bearoff database\n"
                  "(type is illegal: '%2s')\n"),
                szFilename, sz + 6 );
      close ( pbc->h );
      free ( pbc );
      return NULL;
    }

    if ( pbc->bt == BEAROFF_TWOSIDED || pbc->bt == BEAROFF_ONESIDED ) {

      /* normal onesided or twosided bearoff database */

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
      if ( pbc->nChequers < 1 || pbc->nChequers > 15 ) {
        fprintf ( stderr, 
                  _("%s: incomplete bearoff database\n"
                    "(illegal number of chequers is %d)"), 
                  szFilename, pbc->nChequers );
        close ( pbc->h );
        free ( pbc );
        return NULL;
      }

    }
    else {

      /* hypergammon database */

      pbc->nPoints = 25;
      pbc->nChequers = atoi ( sz + 7 );

    }


    pbc->fCompressed = FALSE;
    pbc->fGammon = FALSE;
    pbc->fCubeful = FALSE;
    pbc->fND = FALSE;
    pbc->fHeuristic = FALSE;

    switch ( pbc->bt ) {
    case BEAROFF_TWOSIDED:
      /* options for two-sided dbs */
      pbc->fCubeful = atoi ( sz + 15 );
      break;
    case BEAROFF_ONESIDED:
      /* options for one-sided dbs */
      pbc->fGammon = atoi ( sz + 15 );
      pbc->fCompressed = atoi ( sz + 17 );
      pbc->fND = atoi ( sz + 19 );
      break;
    default:
      break;
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
                pbc->bt,
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

      if ( ! ( pbc = BearoffAlloc() ) ) {
        /* malloc failed */
        perror ( "bearoffcontext" );
        return NULL;
      }

      pbc->bc = BEAROFF_EXACT_BEAROFF;
      pbc->fInMemory = TRUE;
      pbc->nPoints = 6;
      pbc->nChequers = l;
      pbc->bt = BEAROFF_TWOSIDED;
      pbc->fCompressed = FALSE;
      pbc->fGammon = FALSE;
      pbc->fND = FALSE;
      pbc->fCubeful = TRUE;
      pbc->nReads = 0;
      pbc->fHeuristic = FALSE;
      pbc->fMalloc = FALSE;
      pbc->p = NULL;
      pbc->szDir = szDir ? strdup( szDir ) : NULL;
      pbc->szFilename = szFilename ? strdup( szFilename ) : NULL;

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


  /* create cache */

  if ( ! pbc->fInMemory ) {
    if ( ! ( pbc->ph = (hash *) malloc ( sizeof ( hash ) ) ) ||
         HashCreate ( pbc->ph, anCacheSize[ pbc->bt ], ahcmp[ pbc->bt ] ) < 0 )
      pbc->ph = NULL;
  }
  else
    pbc->ph = NULL;
  
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



static int
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
    return -1;
  }

  if ( read ( pbc->h, ac, 16 ) < 16 ) {
    if ( errno )
      perror ( "OS bearoff database" );
    else
      fprintf ( stderr, "error reading OS bearoff database" );
    return -1; 
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

  return 0;

}


static unsigned long
HashOneSided ( const unsigned int nPosID ) {
  return nPosID;
}


static void
AssignOneSided ( float arProb[ 32 ], float arGammonProb[ 32 ],
                 float ar[ 4 ],
                 unsigned short int ausProb[ 32 ], 
                 unsigned short int ausGammonProb[ 32 ],
                 const unsigned short int ausProbx[ 32 ],
                 const unsigned short int ausGammonProbx[ 32 ] ) {

  int i;
  float arx[ 64 ];

  if ( ausProb )
    memcpy ( ausProb, ausProbx, 32 * sizeof ( ausProb[0] ) );

  if ( ausGammonProb )
    memcpy ( ausGammonProb, ausGammonProbx, 32 * sizeof (ausGammonProbx[0]) );

  if ( ar || arProb || arGammonProb ) {
    for ( i = 0; i < 32; ++i ) 
      arx[ i ] = ausProbx[ i ] / 65535.0f;
    
    for ( i = 0; i < 32; ++i ) 
      arx[ 32 + i ] = ausGammonProbx[ i ] / 65535.0f;
  }

  if ( arProb )
    memcpy ( arProb, arx, 32 * sizeof ( float ) );
  if ( arGammonProb )
    memcpy ( arGammonProb, arx + 32, 32 * sizeof ( float ) );

  if ( ar ) {
    AverageRolls ( arx, ar );
    AverageRolls ( arx + 32, ar + 2 );
  }

}


static void
CopyBytes ( unsigned short int aus[ 64 ],
            const unsigned char ac[ 128 ],
            const unsigned int nz,
            const unsigned int ioff,
            const unsigned int nzg,
            const unsigned int ioffg ) {

  int i, j;

  i = 0;
  memset ( aus, 0, 64 * sizeof ( unsigned short int ) );
  for ( j = 0; j < nz; ++j, i += 2 )
    aus[ ioff + j ] = ac[ i ] | ac[ i + 1 ] << 8;

  for ( j = 0; j < nzg; ++j, i += 2 ) 
    aus[ 32 + ioffg + j ] = ac[ i ] | ac[ i + 1 ] << 8;

}


static unsigned short int *
GetDistCompressed ( bearoffcontext *pbc, const unsigned int nPosID ) {

  unsigned char *puch;
  unsigned char ac[ 128 ];
  off_t iOffset;
  int nBytes;
  int nPos = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  static unsigned short int aus[ 64 ];
      
  unsigned int ioff, nz, ioffg, nzg;

  /* find offsets and no. of non-zero elements */
  
  if ( pbc->fInMemory )
    /* database is in memory */
    puch = ( (unsigned char *) pbc->p ) + 40 + nPosID * 8;
  else {
    /* read from disk */
    if ( lseek ( pbc->h, 40 + nPosID * 8, SEEK_SET ) < 0 ) {
      perror ( "OS bearoff database" );
      return NULL;
    }

    if ( read ( pbc->h, ac, 8 ) < 8 ) {
      if ( errno )
        perror ( "OS bearoff database" );
      else
        fprintf ( stderr, "error reading OS bearoff database" );
      return NULL;
    }

    puch = ac;
  }
    
  /* find offset */
  
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
  
  /* get distribution */
  
  if ( pbc->fInMemory )
    /* from memory */
    puch = ( ( unsigned char *) pbc->p ) + iOffset;
  else {
    /* from disk */
    if ( lseek ( pbc->h, iOffset, SEEK_SET ) < 0 ) {
      perror ( "OS bearoff database" );
      return NULL;
    }
    
    if ( read ( pbc->h, ac, nBytes ) < nBytes ) {
      if ( errno )
        perror ( "OS bearoff database" );
      else
        fprintf ( stderr, "error reading OS bearoff database" );
      return NULL;
    }

    puch = ac;

  }
    
  CopyBytes ( aus, puch, nz, ioff, nzg, ioffg );

  return aus;

}


static unsigned short int *
GetDistUncompressed ( bearoffcontext *pbc, const unsigned int nPosID ) {

  unsigned char ac[ 128 ];
  static unsigned short int aus[ 64 ];
  unsigned char *puch;
  int iOffset;

  /* read from file */

  iOffset = 40 + 64 * nPosID * ( pbc->fGammon ? 2 : 1 );

  if ( pbc->fInMemory )
    /* from memory */
    puch = 
      ( ( unsigned char *) pbc->p ) + iOffset;
  else {
    /* from disk */

    lseek ( pbc->h, iOffset, SEEK_SET );
    read ( pbc->h, ac, pbc->fGammon ? 128 : 64 );
    puch = ac;
  }

  CopyBytes ( aus, puch, 32, 0, 32, 0 );

  return aus;

}


static int
ReadBearoffOneSidedExact ( bearoffcontext *pbc, const unsigned int nPosID,
                           float arProb[ 32 ], float arGammonProb[ 32 ],
                           float ar[ 4 ],
                           unsigned short int ausProb[ 32 ], 
                           unsigned short int ausGammonProb[ 32 ] ) {

  unsigned short int *pus = NULL;

  /* look in cache */

  if ( ! pbc->fInMemory && pbc->ph ) {
    
    hashentryonesided *phe;
    hashentryonesided he;
    
    he.nPosID = nPosID;
    if ( ( phe = HashLookup ( pbc->ph, HashOneSided ( nPosID ), &he ) ) ) 
      pus = phe->aus;

  }

  /* get distribution */
  if ( ! pus ) {
    if ( pbc->fCompressed )
      pus = GetDistCompressed ( pbc, nPosID );
    else
      pus = GetDistUncompressed ( pbc, nPosID );

    if ( ! pus ) {
      printf ( "argh!\n" );
      return -1;
    }

    if ( ! pbc->fInMemory ) {
      /* add to cache */
      hashentryonesided *phe = 
        (hashentryonesided *) malloc ( sizeof ( hashentryonesided ) );
      if ( phe ) {
        phe->nPosID = nPosID;
        memcpy ( phe->aus, pus, sizeof ( phe->aus ) );
        HashAdd ( pbc->ph, HashOneSided ( nPosID ), phe );
      }

    }

  }

  AssignOneSided ( arProb, arGammonProb, ar, ausProb, ausGammonProb,
                   pus, pus+32 );

  ++pbc->nReads;


  return 0;

}

extern int
BearoffDist ( bearoffcontext *pbc, const unsigned int nPosID,
              float arProb[ 32 ], float arGammonProb[ 32 ],
              float ar[ 4 ],
              unsigned short int ausProb[ 32 ], 
              unsigned short int ausGammonProb[ 32 ] ) {

  switch ( pbc->bc ) {
  case BEAROFF_GNUBG:

    assert ( pbc->bt == BEAROFF_ONESIDED );

    if ( pbc->fND ) 
      return ReadBearoffOneSidedND ( pbc, nPosID, arProb, arGammonProb, ar,
                                     ausProb, ausGammonProb );
    else
      return ReadBearoffOneSidedExact ( pbc, nPosID, arProb, arGammonProb, ar,
                                        ausProb, ausGammonProb );
    break;

  default:
    assert ( FALSE );
    break;
  }

  /* code not reachable */
  return 0;

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

  if ( ( nBack + nOppBack > 22 ) && ! pbc->bt == BEAROFF_HYPERGAMMON )
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



