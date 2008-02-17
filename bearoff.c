/*
 * bearoff.c
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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
#if USE_MULTITHREAD
/*must be first here because of strange warning from mingw*/
#include "multithread.h"
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <glib/gi18n.h>
#include <glib.h>
#include "bearoffgammon.h"
#include "eval.h"
#include "positionid.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if WIN32
#include <io.h>
#else
#define O_BINARY 0
#endif

const char *aszBearoffGenerator[N_BEAROFFS] = {
  N_("GNU Backgammon"),
  N_("ExactBearoff"),
  N_("Unknown program")
};

static int setGammonProb(const TanBoard anBoard, unsigned int bp0, unsigned int bp1, float* g0, float* g1)
{
  int i;
  unsigned short int prob[32];

  unsigned int tot0 = 0;
  unsigned int tot1 = 0;
  
  for(i = 5; i >= 0; --i) {
    tot0 += anBoard[0][i];
    tot1 += anBoard[1][i];
  }

  { g_assert( tot0 == 15 || tot1 == 15 ); }

  *g0 = 0.0;
  *g1 = 0.0;

  if( tot0 == 15 ) {
    struct GammonProbs* gp = getBearoffGammonProbs(anBoard[0]);
    double make[3];

    if (BearoffDist ( pbc1, bp1, NULL, NULL, NULL, prob, NULL ))
		return -1;
    
    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));
    
    *g1 = (float)((prob[1] / 65535.0) +
	   (1 - make[0]) * (prob[2] / 65535.0) +
	   (1 - make[1]) * (prob[3] / 65535.0) +
	   (1 - make[2]) * (prob[4] / 65535.0));
  }

  if( tot1 == 15 ) {
    struct GammonProbs* gp = getBearoffGammonProbs(anBoard[1]);
    double make[3];

    if (BearoffDist ( pbc1, bp0, NULL, NULL, NULL, prob, NULL ))
		return -1;

    make[0] = gp->p0/36.0;
    make[1] = (make[0] + gp->p1/(36.0*36.0));
    make[2] = (make[1] + gp->p2/(36.0*36.0*36.0));

    *g0 = (float)((prob[1] / 65535.0) * (1-make[0]) + (prob[2]/65535.0) * (1-make[1])
	   + (prob[3]/65535.0) * (1-make[2]));
  }
  return 0;
}


static void 
AverageRolls(const float arProb[32], float *ar)
{
	float sx;
	float sx2;
	int i;

	sx = sx2 = 0.0f;

	for (i = 1; i < 32; i++)
	{
		float p = i * arProb[i];
		sx += p;
		sx2 += i * p;
	}

	ar[ 0 ] = sx;
	ar[ 1 ] = sqrtf(sx2 - sx *sx);
}

/* Make a plausible bearoff move (used to create approximate bearoff database). */
static unsigned int HeuristicBearoff( unsigned int anBoard[ 6 ], const unsigned int anRoll[ 2 ] )
{
    unsigned int i, /* current die being played */
	c, /* number of dice to play */
	nMax, /* highest occupied point */
	anDice[ 4 ],
	j, iSearch, nTotal;
	int n; /* point to play from */

    if( anRoll[ 0 ] == anRoll[ 1 ] )
	{
		/* doubles */
		anDice[ 0 ] = anDice[ 1 ] = anDice[ 2 ] = anDice[ 3 ] = anRoll[ 0 ];
		c = 4;
	}
	else
	{
		/* non-doubles */
		g_assert( anRoll[ 0 ] > anRoll[ 1 ] );

		anDice[ 0 ] = anRoll[ 0 ];
		anDice[ 1 ] = anRoll[ 1 ];
		c = 2;
	}

    for( i = 0; i < c; i++ )
	{
		for( nMax = 5; nMax > 0; nMax-- )
		{
			if (anBoard[nMax])
				break;
		}

		if( !anBoard[nMax] )
			/* finished bearoff */
			break;

		do
		{
			if( anBoard[ anDice[ i ] - 1 ] )
			{
				/* bear off exactly */
				n = (int)anDice[ i ] - 1;
				break;
			}

			if( anDice[ i ] - 1 > nMax )
			{
				/* bear off highest chequer */
				n = (int)nMax;
				break;
			}

			nTotal = anDice[ i ] - 1;
			for( j = i + 1; j < c; j++ )
			{
				nTotal += anDice[ j ];
				if( nTotal < 6 && anBoard[ nTotal ] )
				{
					/* there's a chequer we can bear off with subsequent dice;
						do it */
					n = (int)nTotal;
					break;
				}
			}

			for( n = -1, iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
			{
				if ( anBoard[ iSearch ] >= 2 && /* at least 2 on source point */
						!anBoard[ iSearch - anDice[ i ] ] && /* dest empty */
						( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ) )
					n = (int)iSearch;
			}
			if( n >= 0 )
				break;

			/* find the point with the most on it (or least on dest) */
			for( iSearch = anDice[ i ]; iSearch <= nMax; iSearch++ )
				if ( n == -1 || anBoard[ iSearch ] > anBoard[ n ] ||
						( anBoard[ iSearch ] == anBoard[ n ] &&
						anBoard[ iSearch - anDice[ i ] ] <
						anBoard[ (unsigned int)n - anDice[ i ] ] ) )
					n = (int)iSearch;

			g_assert( n >= 0 );
		} while(n < 0);	/* Dummy loop to remove goto's */

		g_assert( anBoard[ n ] );
		anBoard[ n ]--;

		if( n >= (int)anDice[ i ] )
			anBoard[ n - (int)anDice[ i ] ]++;
	}

    return PositionBearoff( anBoard, 6, 15 );
}

static void GenerateBearoff( unsigned char *p, unsigned int nId )
{
    unsigned int anRoll[ 2 ], anBoard[ 6 ], aProb[ 32 ];
	unsigned int i, iBest;
    unsigned short us;

    for( i = 0; i < 32; i++ )
		aProb[ i ] = 0;
    
    for( anRoll[ 0 ] = 1; anRoll[ 0 ] <= 6; anRoll[ 0 ]++ )
	for( anRoll[ 1 ] = 1; anRoll[ 1 ] <= anRoll[ 0 ]; anRoll[ 1 ]++ )
	{
	    PositionFromBearoff( anBoard, nId, 6, 15 );
	    iBest = HeuristicBearoff( anBoard, anRoll );

	    g_assert( iBest < nId );
	    
	    if( anRoll[ 0 ] == anRoll[ 1 ] )
			for( i = 0; i < 31; i++ )
				aProb[ i + 1 ] += p[ ( iBest << 6 ) | ( i << 1 ) ] +
				( p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 );
	    else
			for( i = 0; i < 31; i++ )
				aProb[ i + 1 ] += ( p[ ( iBest << 6 ) | ( i << 1 ) ] +
				( (unsigned int)p[ ( iBest << 6 ) | ( i << 1 ) | 1 ] << 8 ) ) << 1;
	}

    for( i = 0; i < 32; i++ )
	{
		us = (unsigned short)(( aProb[ i ] + 18 ) / 36);
		p[ ( nId << 6 ) | ( i << 1 ) ] = us & 0xFF;
		p[ ( nId << 6 ) | ( i << 1 ) | 1 ] = us >> 8;
    }
}

static unsigned char *HeuristicDatabase( void (*pfProgress)(unsigned int) )
{
    unsigned char *pm = malloc( 40 + 54264 * 64 );
    unsigned char *p;
    unsigned int i;
    
    if (!pm)
		return NULL;

    p = pm + 40;
    p[ 0 ] = p[ 1 ] = 0xFF;
    for( i = 2; i < 64; i++ )
		p[ i ] = 0;

    for( i = 1; i < 54264; i++ )
	{
		GenerateBearoff( p, i );
		if( pfProgress && !( i % 1000 ) )
			pfProgress( i );
    }

    return pm;
}


static void ReadBearoffFile(const bearoffcontext *pbc, unsigned int offset, unsigned char *buf, unsigned int nBytes)
{
#if USE_MULTITHREAD
	MT_Exclusive();
#endif

	if ((lseek(pbc->h, (long)offset, SEEK_SET ) < 0) || (read(pbc->h, buf, nBytes) < (int)nBytes))
	{
		if (errno)
			perror("OS bearoff database");
		else
			fprintf(stderr, "error reading OS bearoff database");

		memset(buf, 0, nBytes);
		return;
	}

#if USE_MULTITHREAD
	MT_Release();
#endif
}

/* BEAROFF_GNUBG: read two sided bearoff database */
static void ReadTwoSidedBearoff ( const bearoffcontext *pbc,
                      const unsigned int iPos,
                      float ar[ 4 ], unsigned short int aus[ 4 ] )
{
	unsigned int i, k = ( pbc->fCubeful ) ? 4 : 1;
	unsigned char ac[ 8 ];
	unsigned char *pc = NULL;
	unsigned short int us;

	if ( pbc->fInMemory )
		pc = ((unsigned char *) pbc->p) + 40 + 2 * iPos * k;
	else
	{
		ReadBearoffFile(pbc, 40 + 2 * iPos * k, ac, k * 2);
		pc = ac;
	}
	/* add to cache */

	for ( i = 0; i < k; ++i )
	{
		us = pc[ 2 * i ] | ( pc[ 2 * i + 1 ] ) << 8;
		if ( aus )
			aus[ i ] = us;
		if ( ar )
			ar[ i ] = us / 32767.5f - 1.0f;
	}      

	++((bearoffcontext *)pbc)->nReads;	/* nReads only used for stats info */
}

static unsigned int countBoard ( const unsigned int nDistance, const unsigned int nChequers )
{
  unsigned int i, id = 0;

  if ( nDistance == 1 || ! nChequers )
    return 1;
  else
  {
    for ( i = 0; i <= nChequers; ++i )
      id += countBoard ( nDistance - 1, nChequers - i );
    return id;
  }
}

static unsigned int boardNr ( const unsigned int anBoard[], const unsigned int nDistance, const unsigned int nPoints, const unsigned int nChequers )
{
  unsigned int i, id;

  if ( nDistance == 1 || ! nChequers )
    return 0;

  id = boardNr ( anBoard, nDistance - 1, nPoints, nChequers - anBoard [ nDistance - 2 ] );
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
ReadExactBearoff ( const bearoffcontext *pbc, 
                   const unsigned int iPos,
                   float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  unsigned char ac[ 12 ];
  unsigned int offset;
  unsigned long ul;
  const float storefactor = 4194000.0f;
  int i;
  unsigned int nUs, nThem;
  unsigned int n = Combination ( pbc->nChequers + pbc->nPoints, pbc->nPoints );
  TanBoard anBoard;

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

  ReadBearoffFile(pbc, offset, ac, 12);

  for ( i = 0; i < 4; ++i ) {
    ul = ac[ 3 * i ] | ac[ 3 * i + 1 ] << 8 | ac[ 3 * i + 2 ] << 16;

    if ( ar )
      ar[ i ] = ul / storefactor - 2.0f;
    if ( aus )
      aus[ i ] = (unsigned short)(ul >> 8);

  }
  
  ++((bearoffcontext *)pbc)->nReads;	/* nReads only used for stats info */

  return 0;

}

extern int
BearoffCubeful ( const bearoffcontext *pbc,
                 const unsigned int iPos,
                 float ar[ 4 ], unsigned short int aus[ 4 ] ) {

  switch ( pbc->bc )
  {
  case BEAROFF_GNUBG:

    if ( ! pbc->fCubeful )
      return -1;
    else {
      ReadTwoSidedBearoff ( pbc, iPos, ar, aus );
	  return 0;
    }

  case BEAROFF_EXACT_BEAROFF:

    return ReadExactBearoff ( pbc, iPos, ar, aus );

  case BEAROFF_UNKNOWN:
  default:
	/* Never reached */
    return -1;
  }
}


static int
BearoffEvalTwoSided ( const bearoffcontext *pbc, 
                      const TanBoard anBoard, float arOutput[] ) {

  unsigned int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  unsigned int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  unsigned int iPos = nUs * n + nThem;
  float ar[ 4 ];
  
  ReadTwoSidedBearoff ( pbc, iPos, ar, NULL );

  memset ( arOutput, 0, 5 * sizeof ( float ) );
  arOutput[ OUTPUT_WIN ] = ar[ 0 ] / 2.0f + 0.5f;
  
  return 0;

}


static int
ReadHypergammon( const bearoffcontext *pbc,
                 const unsigned int iPos,
                 float arOutput[ NUM_OUTPUTS ],
                 float arEquity[ 4 ] ) {

  unsigned char ac[ 28 ];
  unsigned char *pc = NULL;
  unsigned int us;
  int i;
  const int x = 28;

  if ( pbc->fInMemory )
    pc = ((unsigned char *)pbc->p) + 40 + x * iPos;
  else {
    ReadBearoffFile(pbc, 40 + x * iPos, ac, x);
    pc = ac;
  }

  if ( arOutput )
    for ( i = 0; i < NUM_OUTPUTS; ++i ) {
      us = pc[ 3 * i ] | ( pc[ 3 * i + 1 ] ) << 8 | ( pc[ 3 * i + 2 ] ) << 16;
      arOutput[ i ] = us / 16777215.0f;
    }

  if ( arEquity )
    for ( i = 0; i < 4; ++i ) {
      us = pc[ 15 + 3 * i ] | 
         ( pc[ 15 + 3 * i + 1 ] ) << 8 | 
         ( pc[ 15 + 3 * i + 2 ] ) << 16;
      arEquity[ i ] = ( us / 16777215.0f - 0.5f ) * 6.0f;
    }

  ++((bearoffcontext *)pbc)->nReads;	/* nReads only used for stats info */

  return 0;

}


static int BearoffEvalOneSided ( const bearoffcontext *pbc, 
                      const TanBoard anBoard, float arOutput[] )
{
  int i, j;
  float aarProb[ 2 ][ 32 ];
  float aarGammonProb[ 2 ][ 32 ];
  float r;
  unsigned int anOn[ 2 ];
  unsigned int an[ 2 ];
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
    else
	{
      
		if (setGammonProb(anBoard, an[ 0 ], an[ 1 ],
                    &arOutput[ OUTPUT_LOSEGAMMON ],
                    &arOutput[ OUTPUT_WINGAMMON ] ))
			return -1;
      
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
BearoffHyper( const bearoffcontext *pbc,
              const unsigned int iPos,
              float arOutput[], float arEquity[] ) {

  return ReadHypergammon( pbc, iPos, arOutput, arEquity );

}


static int
BearoffEvalHypergammon ( const bearoffcontext *pbc, 
                         const TanBoard anBoard, float arOutput[] ) {

  unsigned int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  unsigned int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  unsigned int iPos = nUs * n + nThem;

  return ReadHypergammon ( pbc, iPos, arOutput, NULL );
}

extern int BearoffEval ( const bearoffcontext *pbc, const TanBoard anBoard, float arOutput[] )
{
  if (!pbc)
    return 0;

  switch ( pbc->bc ) {
  case BEAROFF_GNUBG:

    switch ( pbc->bt ) {
    case BEAROFF_TWOSIDED:
      return BearoffEvalTwoSided ( pbc, anBoard, arOutput );
    case BEAROFF_ONESIDED:
      return BearoffEvalOneSided ( pbc, anBoard, arOutput );
    case BEAROFF_HYPERGAMMON:
      return BearoffEvalHypergammon ( pbc, anBoard, arOutput );
    }

    break;

  case BEAROFF_EXACT_BEAROFF:

    g_assert ( pbc->bt == BEAROFF_TWOSIDED );
    return BearoffEvalTwoSided ( pbc, anBoard, arOutput );

  case BEAROFF_UNKNOWN:
  default:

    g_assert ( FALSE );
  }

  return 0;
}

extern void
BearoffStatus ( const bearoffcontext *pbc, char *sz ) {

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

	sprintf( sz + strlen(sz),
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

}


static int BearoffDumpTwoSided ( const bearoffcontext *pbc, const TanBoard anBoard, char *sz )
{
  unsigned int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  unsigned int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  unsigned int iPos = nUs * n + nThem;
  float ar[ 4 ];
  unsigned int i;
  static const char *aszEquity[] = {
    N_("Cubeless equity"),
    N_("Owned cube"),
    N_("Centered cube"),
    N_("Opponent owns cube")
  };

  sprintf ( sz + strlen(sz),
            _("             Player       Opponent\n"
            "Position %12d  %12d\n\n"), 
            nUs, nThem );

  ReadTwoSidedBearoff ( pbc, iPos, ar, NULL );

  if ( pbc->fCubeful )
    for ( i = 0; i < 4 ; ++i )
      sprintf ( sz + strlen(sz),
                "%-30.30s: %+7.4f\n", 
                gettext ( aszEquity[ i ] ), ar[ i ] );
  else
    sprintf ( sz + strlen(sz),
              "%-30.30s: %+7.4f\n",
              gettext ( aszEquity[ 0 ] ), 2.0 * ar[ 0 ] - 1.0f );

  strcat ( sz, "\n" );

  return 0;

}


static int BearoffDumpOneSided ( const bearoffcontext *pbc, const TanBoard anBoard, char *sz )
{
  unsigned int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  float ar[ 2 ][ 4 ];
  unsigned int i;
  float aarProb[ 2 ][ 32 ], aarGammonProb[ 2 ][ 32 ];
  int f0, f1, f2, f3;
  unsigned int anPips[ 2 ];
  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
              5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
              1 * 16 + 1 * 20 + 1 * 24 ) / 36.0f;


  if ( BearoffDist ( pbc, nUs, aarProb[ 0 ], aarGammonProb[ 0 ], ar[ 0 ], 
                     NULL, NULL ) )
    return -1;
  if ( BearoffDist ( pbc, nThem, aarProb[ 1 ], aarGammonProb[ 1 ], ar[ 1 ],
                     NULL, NULL ) )
    return -1;

  sprintf ( sz + strlen(sz),
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
      
      sprintf( sz = sz + strlen(sz),
               "%5d\t%7.3f\t%7.3f" "\t\t",
               i, 
               aarProb[ 0 ][ i ] * 100.0f,
               aarProb[ 1 ][ i ] * 100.0f );

      if ( pbc->fGammon )
        sprintf ( sz = sz + strlen(sz),
                  "%7.3f\t%7.3f\n", 
                  aarGammonProb[ 0 ][ i ] * 100.0f,
                  aarGammonProb[ 1 ][ i ] * 100.0f );
      else
        sprintf ( sz = sz + strlen(sz),
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

  sprintf ( sz = sz + strlen(sz),
            _("Mean\t%7.3f\t%7.3f\t\t"),
            ar[ 0 ][ 0 ], ar[ 1 ][ 0 ] );

  if ( pbc->fGammon )
    sprintf ( sz = sz + strlen(sz),
              "%7.3f\t%7.3f\n",
              ar[ 0 ][ 2 ], ar[ 1 ][ 2 ] );
  else
    sprintf ( sz = sz + strlen(sz),
              "%-7.7s\t%-7.7s\n",
              _("n/a"), _("n/a" ) );

  /* std. dev */

  sprintf ( sz = sz + strlen(sz),
            _("Std dev\t%7.3f\t%7.3f\t\t"),
            ar[ 0 ][ 1 ], ar[ 1 ][ 1 ] );

  if ( pbc->fGammon )
    sprintf ( sz = sz + strlen(sz),
              "%7.3f\t%7.3f\n",
              ar[ 0 ][ 3 ], ar[ 1 ][ 3 ] );
  else
    sprintf ( sz = sz + strlen(sz),
              "%-7.7s\t%-7.7s\n",
              _("n/a"), _("n/a" ) );


  /* effective pip count */

  PipCount( anBoard, anPips );

  strcat( sz + strlen(sz), _("\nEffective pip count:\n" ) );
  strcat( sz + strlen(sz), _("\tPlayer\tOpponent\n" ) );
  sprintf( sz + strlen(sz), _("EPC\t%7.3f\t%7.3f\n"
                              "Wastage\t%7.3f\t%7.3f\n\n" ),
           ar[ 0 ][ 0 ] * x, ar[ 1 ][ 0 ] * x,
           ar[ 0 ][ 0 ] * x - anPips[ 1 ],
           ar[ 1 ][ 0 ] * x - anPips[ 0 ] );

  sprintf( sz + strlen(sz),
          _("EPC = %5.3f * Average rolls\n"
            "Wastage = EPC - pips\n\n" ), x );

  return 0;

}


static int
BearoffDumpHyper( const bearoffcontext *pbc, const TanBoard anBoard, char *sz )
{
  unsigned int nUs = PositionBearoff ( anBoard[ 1 ], pbc->nPoints, pbc->nChequers );
  unsigned int nThem = PositionBearoff ( anBoard[ 0 ], pbc->nPoints, pbc->nChequers );
  unsigned int n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
  unsigned int iPos = nUs * n + nThem;
  float ar[ 4 ];
  unsigned int i;
  static const char *aszEquity[] = {
    N_("Owned cube"),
    N_("Centered cube"),
    N_("Centered cube (Jacoby rule)"),
    N_("Opponent owns cube")
  };

  if ( BearoffHyper ( pbc, iPos, NULL, ar ) )
    return -1;

  sprintf ( sz + strlen(sz),
            "             Player       Opponent\n"
            "Position %12d  %12d\n\n", 
            nUs, nThem );

  for ( i = 0; i < 4 ; ++i )
    sprintf ( sz + strlen(sz),
              "%-30.30s: %+7.4f\n", 
              gettext ( aszEquity[ i ] ), ar[ i ] );

  return 0;

}

extern int BearoffDump ( const bearoffcontext *pbc, const TanBoard anBoard, char *sz )
{
  switch ( pbc->bc )
  {
  case BEAROFF_GNUBG:

    switch ( pbc->bt ) {
    case BEAROFF_TWOSIDED:
      return BearoffDumpTwoSided ( pbc, anBoard, sz );
    case BEAROFF_ONESIDED:
      return BearoffDumpOneSided ( pbc, anBoard, sz );
    case BEAROFF_HYPERGAMMON:
      return BearoffDumpHyper ( pbc, anBoard, sz );
    }
    break;

  case BEAROFF_EXACT_BEAROFF:

    return BearoffDumpTwoSided ( pbc, anBoard, sz );

  case BEAROFF_UNKNOWN:
  default:
	  g_assert(FALSE);
  }
  /* code not reachable */
  return -1;
}

extern void BearoffClose(bearoffcontext *ppbc)
{
  if (!ppbc)
    return;

  if ( !ppbc->fInMemory )
    close ( ppbc->h );
  else if ( ppbc->p && ppbc->fMalloc )
    free ( ppbc->p );

  if ( ppbc->szFilename )
    g_free( ppbc->szFilename );

  if ( ppbc->ah ) {
    int i;
    for ( i = 0; i < ppbc->nFiles; ++i )
      close( ppbc->ah[ i ] );
    free( ppbc->ah );
  }

  free(ppbc);
}

static int ReadIntoMemory ( bearoffcontext *pbc, const int iOffset, const unsigned int nSize )
{
  pbc->fMalloc = TRUE;

#if HAVE_MMAP
  if ( ( pbc->p = mmap ( NULL, nSize, PROT_READ, 
                           MAP_SHARED, pbc->h, iOffset ) ) == (void *) -1 ) {
    /* perror ( "mmap" ); */
    /* mmap failed */
#endif /* HAVE_MMAP */

    /* allocate memory for database */

    if ( ( pbc->p = malloc ( nSize ) ) == NULL )
	{
      perror ( "pbc->p" );
      return -1;
    }

    if ( lseek( pbc->h, iOffset, SEEK_SET ) == (off_t) -1 ) {
      perror ( "lseek" );
      return -1;
    }

    if ( (unsigned int)read ( pbc->h, pbc->p, nSize ) < nSize ) {
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
isExactBearoff ( const unsigned char ac[ 8 ] ) {

  long id = ( ac[ 0 ] | ac[ 1 ] << 8 | ac[ 2 ] << 16 | ac[ 3 ] << 24 );
  long ver = ( ac[ 4 ] | ac[ 5 ] << 8 | ac[ 6 ] << 16 | ac[ 7 ] << 24 );

  return id == 73457356 && ver == 100;

}


extern bearoffcontext *
BearoffAlloc( void ) {

  bearoffcontext *pbc;

  if( ( pbc = ( bearoffcontext *) malloc( sizeof ( bearoffcontext ) ) ) == NULL )
    return NULL;
  
  pbc->h = -1;
  pbc->ah = NULL;
  pbc->nFiles = 0;
  pbc->bt = (bearofftype)-1;
  pbc->bc = (bearoffcreator)-1;
  pbc->nPoints = 0;
  pbc->nChequers = 0;
  pbc->fInMemory = FALSE;
  pbc->fMalloc = FALSE;
  pbc->szFilename = NULL;
  pbc->fCompressed = TRUE;
  pbc->fGammon = TRUE;
  pbc->fND = FALSE;
  pbc->fHeuristic = FALSE;
  pbc->nOffsetBuffer = -1;
  pbc->puchBuffer = NULL;
  pbc->puchA = NULL;
  pbc->fCubeful = TRUE;
  pbc->p = NULL;

  return pbc;

}

static unsigned int MakeInt(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	return (a | (unsigned char)b << 8 | (unsigned char)c << 16 | (unsigned char)d << 24);
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
 */
extern bearoffcontext *BearoffInit(const char *szFilename, const int bo, void (*p)(unsigned int))
{
  bearoffcontext *pbc;
  char sz[ 41 ];
  int iOffset = 0;

  if ( ( pbc = BearoffAlloc() ) == NULL ) {
    /* malloc failed */
    perror ( "bearoffcontext" );
    return NULL;
  }

  pbc->nReads = 0;

  if ( bo & (int)BO_HEURISTIC )
  {
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
    pbc->fHeuristic = TRUE;
    pbc->fMalloc = TRUE;
    pbc->p = HeuristicDatabase ( p );
    pbc->szFilename = szFilename ? g_strdup( szFilename ) : NULL;

    return pbc;
    
  }

  

  if ( ! szFilename || ! *szFilename )
    return NULL;

  /*
   * Allocate memory for bearoff context
   */

  pbc->fInMemory = bo & (int)BO_IN_MEMORY;

  /*
   * Open bearoff file
   */

  if ( ( pbc->h = open(szFilename, O_RDONLY | O_BINARY ) ) < 0 ) {
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
  else if ( isExactBearoff ( (unsigned char*)sz ) )
    pbc->bc = BEAROFF_EXACT_BEAROFF;
  else
    pbc->bc = BEAROFF_UNKNOWN;

  pbc->szFilename = szFilename ? g_strdup( szFilename ) : NULL;

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
      
      pbc->nPoints = (unsigned)atoi ( sz + 9 );
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
      
      pbc->nChequers = (unsigned)atoi ( sz + 12 );
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
      pbc->nChequers = (unsigned)atoi ( sz + 7 );

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
	case BEAROFF_HYPERGAMMON:
    default:
      break;
    }

    iOffset = 0;

    break;

  case BEAROFF_EXACT_BEAROFF: 
    {
      unsigned int l, m;
      
      l = MakeInt((unsigned char)sz[8], (unsigned char)sz[9], (unsigned char)sz[10], (unsigned char)sz[11]);
      m = MakeInt((unsigned char)sz[12], (unsigned char)sz[13], (unsigned char)sz[14], (unsigned char)sz[15]);
      printf ( "nBottom %d\n", l );
      printf ( "nTop %d\n", m );

      if ( m != l ) {
        fprintf ( stderr, _("GNU Backgammon can only read ExactBearoff "
                            "databases with an\n"
                            "equal number of chequers on both sides\n"
                            "(read: botton %d, top %d)\n"),
                  l, m );
      }

      if ( ( pbc = BearoffAlloc() ) == NULL ) {
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
      pbc->fHeuristic = FALSE;
      pbc->fMalloc = FALSE;
      pbc->p = NULL;
      pbc->szFilename = szFilename ? g_strdup( szFilename ) : NULL;

      iOffset = 0;
    
    }
    break;

  case BEAROFF_UNKNOWN: 
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

  if ( pbc->fInMemory )
  {
    struct stat st;
    if ( fstat ( pbc->h, &st ) )
	{
		perror ( szFilename );
		close ( pbc->h );
		free ( pbc );
		return NULL;
    }
    
    if ( ReadIntoMemory ( pbc, iOffset, (unsigned int)st.st_size ) ) {
      
      close ( pbc->h );
      free ( pbc );
      return NULL;

    }
    close ( pbc->h );
    pbc->h = -1;
  }
  return pbc;
}


extern float
fnd ( const float x, const float mu, const float sigma  ) {

   const float epsilon = 1.0e-7f;

   if ( sigma <= epsilon )
      /* dirac delta function */
      return ( fabsf ( mu - x ) < epsilon ) ? 1.0f : 0.0f;
   else {

     float xm = ( x - mu ) / sigma;

     return 1.0f / (( sigma * sqrtf ( 2.0 * G_PI ) ) * ((float)(exp ( - xm * xm / 2.0 ))));

   }

}



static int
ReadBearoffOneSidedND ( const bearoffcontext *pbc, 
                        const unsigned int nPosID,
                        float arProb[ 32 ], float arGammonProb[ 32 ],
                        float *ar,
                        unsigned short int ausProb[ 32 ], 
                        unsigned short int ausGammonProb[ 32 ] ) {
  
  unsigned char ac[ 16 ];
  float arx[ 4 ];
  int i;
  float r;

  ReadBearoffFile(pbc, 40 + nPosID * 16, ac, 16);

  memcpy ( arx, ac, 16 );

  if ( arProb || ausProb )
     for ( i = 0; i < 32; ++i ) {
       r = fnd ( 1.0f * i, arx[ 0 ], arx[ 1 ] );
       if ( arProb )
         arProb[ i ] = r;
       if ( ausProb )
         ausProb[ i ] = (unsigned short)(r * 65535.0f);
     }
     


  if ( arGammonProb || ausGammonProb )
    for ( i = 0; i < 32; ++i ) {
      r = fnd ( 1.0f * i, arx[ 2 ], arx[ 3 ] );
      if ( arGammonProb )
        arGammonProb[ i ] = r;
      if ( ausGammonProb )
        ausGammonProb[ i ] = (unsigned short)(r * 65535.0f);
    }
  
  if ( ar )
    memcpy ( ar, arx, 16 );

  ++((bearoffcontext *)pbc)->nReads;	/* nReads only used for stats info */

  return 0;

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

	if ( ar || arProb || arGammonProb )
	{
		for ( i = 0; i < 32; ++i ) 
			arx[ i ] = ausProbx[ i ] / 65535.0f;

		for ( i = 0; i < 32; ++i ) 
			arx[ 32 + i ] = ausGammonProbx[ i ] / 65535.0f;

		if ( arProb )
			memcpy ( arProb, arx, 32 * sizeof ( float ) );
		if ( arGammonProb )
			memcpy ( arGammonProb, arx + 32, 32 * sizeof ( float ) );
		if ( ar )
		{
			AverageRolls ( arx, ar );
			AverageRolls ( arx + 32, ar + 2 );
		}
	}
}


static void
CopyBytes ( unsigned short int aus[ 64 ],
            const unsigned char ac[ 128 ],
            const unsigned int nz,
            const unsigned int ioff,
            const unsigned int nzg,
            const unsigned int ioffg ) {

  unsigned int i, j;

  i = 0;
  memset ( aus, 0, 64 * sizeof ( unsigned short int ) );
  for ( j = 0; j < nz; ++j, i += 2 )
    aus[ ioff + j ] = ac[ i ] | ac[ i + 1 ] << 8;

  for ( j = 0; j < nzg; ++j, i += 2 ) 
    aus[ 32 + ioffg + j ] = ac[ i ] | ac[ i + 1 ] << 8;
}

static unsigned short int *GetDistCompressed ( unsigned short int aus[ 64 ], const bearoffcontext *pbc, const unsigned int nPosID )
{
  unsigned char *puch;
  unsigned char ac[ 128 ];
  unsigned int iOffset;
  unsigned int nBytes;
  unsigned int ioff, nz, ioffg, nzg;
  unsigned int nPos = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );

  /* find offsets and no. of non-zero elements */
  
  if ( pbc->fInMemory )
    /* database is in memory */
    puch = ( (unsigned char *) pbc->p ) + 40 + nPosID * 8;
  else {
    ReadBearoffFile(pbc, 40 + nPosID * 8, ac, 8);
    puch = ac;
  }
    
  /* find offset */
  
  iOffset = MakeInt(puch[ 0 ], puch[ 1 ], puch[ 2 ], puch[ 3 ]);

  nz = puch[ 4 ];
  ioff = puch[ 5 ];
  nzg = puch[ 6 ];
  ioffg = puch[ 7 ];

  /* Sanity checks */

  if ( ( iOffset > 64 * nPos && 64 * nPos > 0 ) || 
       nz > 32 || ioff > 32 || 
       nzg > 32 || ioffg > 32) {
    fprintf( stderr, 
             "The bearoff file '%s' is likely to be corrupted.\n"
             "Please check that the MD5 sum is the same as documented "
             "in the GNU Backgammon manual.\n"
             "Offset %lu, dist size %u (offset %u), "
             "gammon dist size %u (offset %u)\n",
             pbc->szFilename, (unsigned long) iOffset, nz, ioff, nzg, ioffg );
    g_assert( FALSE );
  }

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
    ReadBearoffFile(pbc, iOffset, ac, nBytes);
    puch = ac;

  }
    
  CopyBytes ( aus, puch, nz, ioff, nzg, ioffg );

  return aus;
}

static unsigned short int *GetDistUncompressed ( unsigned short int aus[ 64 ], const bearoffcontext *pbc, const unsigned int nPosID )
{
  unsigned char ac[ 128 ];
  unsigned char *puch;
  unsigned int iOffset;

  /* read from file */

  iOffset = 40 + 64 * nPosID * ( pbc->fGammon ? 2 : 1 );

  if ( pbc->fInMemory )
    /* from memory */
    puch = 
      ( ( unsigned char *) pbc->p ) + iOffset;
  else {
    /* from disk */

	ReadBearoffFile(pbc, iOffset, ac, pbc->fGammon ? 128 : 64);
    puch = ac;
  }

  CopyBytes ( aus, puch, 32, 0, 32, 0 );

  return aus;

}


static int
ReadBearoffOneSidedExact ( const bearoffcontext *pbc, const unsigned int nPosID,
                           float arProb[ 32 ], float arGammonProb[ 32 ],
                           float ar[ 4 ],
                           unsigned short int ausProb[ 32 ], 
                           unsigned short int ausGammonProb[ 32 ] )
{
	unsigned short int aus[ 64 ];
	unsigned short int *pus = NULL;

	/* get distribution */
	if ( pbc->fCompressed )
		pus = GetDistCompressed ( aus, pbc, nPosID );
	else
		pus = GetDistUncompressed ( aus, pbc, nPosID );

	if (!pus)
	{
		printf ( "argh!\n" );
		return -1;
	}

	AssignOneSided ( arProb, arGammonProb, ar, ausProb, ausGammonProb,
					pus, pus+32 );

	++((bearoffcontext *)pbc)->nReads;	/* nReads only used for stats info */

	return 0;
}

extern int BearoffDist ( const bearoffcontext *pbc, const unsigned int nPosID,
              float arProb[ 32 ], float arGammonProb[ 32 ],
              float ar[ 4 ],
              unsigned short int ausProb[ 32 ], 
              unsigned short int ausGammonProb[ 32 ] )
{
  switch ( pbc->bc )
  {
  case BEAROFF_GNUBG:

    g_assert ( pbc->bt == BEAROFF_ONESIDED );

    if ( pbc->fND ) 
      return ReadBearoffOneSidedND ( pbc, nPosID, arProb, arGammonProb, ar,
                                     ausProb, ausGammonProb );
    else
      return ReadBearoffOneSidedExact ( pbc, nPosID, arProb, arGammonProb, ar,
                                        ausProb, ausGammonProb );

  case BEAROFF_EXACT_BEAROFF:
  case BEAROFF_UNKNOWN:
  default:
	/* code not reachable */
	return -1;
  }
}

extern int isBearoff(const bearoffcontext *pbc, const TanBoard anBoard)
{
  unsigned int i, nOppBack, nBack;
  unsigned int n = 0, nOpp = 0;

  if (!pbc)
    return FALSE;

  for (nOppBack = 24; nOppBack > 0; nOppBack--)
  {
    if (anBoard[0][nOppBack])
      break;
  }
  for (nBack = 24; nBack > 0; nBack--)
  {
    if (anBoard[1][nBack])
      break;
  }
  if (!anBoard[0][nOppBack] || !anBoard[1][nBack])
    /* the game is over */
    return FALSE;

  if ( ( nBack + nOppBack > 22 ) && !(pbc->bt == BEAROFF_HYPERGAMMON) )
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
