#if defined( __GNUG__ )

#include <assert.h>

extern "C" {
#include "positionid.h"
#include "eval.h"
}

#include <string.h> // memmove, memset

#include "racetab.h"
#include "raceprobs.h"

static inline
uint max(uint a, uint b) {
  return a > b ? a : b;
}

//  bearoff info
struct B {
  // number of probabilities
  uint len : 16;

  // p[0] is probablity of bearing off in 'start' rolls
  uint start : 16;

  // array of probabilities.
  float* p;
};


// probabilities for bearoff position 'm'
const B*
probs(uint const m[6])
{
  int x[6];
  for(uint i = 0; i < 6; ++i) {
    x[i] = m[5-i];
  }
  unsigned short nOpp = PositionBearoff(x);

  extern unsigned char* pBearoff1;

  float f[32];
  static B b;

  b.start = 0;
  for(/**/; b.start < 31; b.start += 1) {
    int i = b.start;
    
    int const p = pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) ] +
      ( pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) | 1 ] << 8 );

    if( p != 0 ) {
      break;
    }
  }

  b.len = 32 - b.start;
  
  for(int i = 31; i >= 0; --i) {
    int const p = pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) ] +
      ( pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) | 1 ] << 8 );
    
    if( p != 0 ) {
      break;
    }

    b.len -= 1;
  }
  
  for(uint i = b.start; i < b.start + b.len; ++i) {
    int const p = pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) ] +
      ( pBearoff1[ ( nOpp << 6 ) | ( i << 1 ) | 1 ] << 8 );

    f[i-b.start] = p / 65535.0;
  }

  b.p = f;

  return &b;
}


struct P {
  P(void);
  
  uint minNrolls;
  uint maxNrolls;

  uint maxFirstOff;
  
  static uint const np = 50;
  static uint const nf = 50;

  void	norm(void);

  void incNrolls(uint n);
  
  float prob(uint k) const {
    {                             assert( k >= minNrolls && k <= maxNrolls ); }
    
    return probs[k - minNrolls];
  }

  float& prob(uint k) {
    {                             assert( k >= minNrolls && k <= maxNrolls ); }
    
    return probs[k - minNrolls];
  }
  
  float probs[np];
  
  float firstOff[nf];
};

inline
P::P(void) :
  minNrolls(1),
  maxNrolls(np),
  maxFirstOff(nf)
{
  for(uint k = 0; k < np; ++k) {
    probs[k] = 0.0;
  }
      
  for(uint k = 0; k < nf; ++k) {
    firstOff[k] = 0.0;
  }
}

void
P::norm(void)
{
  for(uint k = 0; k < np; ++k) {
    if( probs[k] > 0 ) {
      minNrolls = k + 1;

      for(uint j = k+1; j < np; ++j) {
	if( probs[j] == 0.0 ) {
	  memmove(&probs[0], &probs[k], (j - k) * sizeof(probs[0]));
	  maxNrolls = j - 1;

	  // icc
	  for(uint j1 = j+1; j1 < np; ++j1) {
	    assert( probs[j1] == 0.0 );
	  }
	  
	  break;
	}
      }
      break;
    }
  }

  for(uint k1 = 0; k1 < nf; ++k1) {
    if( firstOff[k1] != 0.0 ) {
      for(uint k = k1+1; k < nf; ++k) {
	if( firstOff[k] == 0.0 ) {
	  maxFirstOff = k;
      
	  for(uint j1 = k+1; j1 < nf; ++j1) {
	    assert( firstOff[j1] == 0.0 );
	  }
	  break;
	}
      }
      break;
    }
  }
}

void
P::incNrolls(uint const n)
{
  minNrolls += n;
  maxNrolls += n;

  memmove(firstOff + n, firstOff, (nf - n) * sizeof(firstOff[0]));

  for(uint k = 0; k < n; ++k) {
    firstOff[k] = 0.0;
  }
  maxFirstOff += n;
}


static void
raceProbs(P& p, NpipsRace const& s, uint const nOut, uint const b[6])
{
  uint mh[6];
 
  for(uint i = 0; i < s.nInfo; ++i) {
    NpipsRace::Info const& pn = s.info[i];

    memcpy(mh, b, sizeof(mh)); 

    uint r = pn.remainNpips;

    // determine how many you can bear off
    uint nBearOff = 0;

    if( r >= 10 || (nOut == 1 && r >= 6) ) {
      nBearOff = r / 6;
      r -= nBearOff * 6;
    }

    // Assume you can bear off at least one with any roll.
    p.firstOff[pn.n + (nBearOff == 0) - 1] += pn.p();

    // speard nOut in home to get r
    {
      for(uint t = nOut; t > 0; --t) {
	uint const k = r / t;
	{                                                    assert( k < 6 ); }
	++mh[k];
	r -= k;
      }
    }

    while( nBearOff > 0 ) {
      for(uint j = 0; j < 6; ++j) {
	if( mh[j] > 0 ) {
	  --mh[j];
	  break;
	}
      }
      --nBearOff;
    }

    const B* const b = probs(mh);
    
    for(uint j = 0; j < b->len; ++j) {
      uint const n = pn.n + (j + b->start);

      {                            assert( p.minNrolls <= n <= p.maxNrolls ); }

      p.prob(n) += b->p[j] * pn.p();
    }
  }

  p.norm();
}

static void
raceProbs(P& p, uint const m[24])
{
  {                                                      assert( m[0] == 0 ); }

  uint nOut = 0;
  uint tot = 0;
  for(uint k = 1; k < 18; ++k) {
    if( m[k] ) {
      tot += (18 - k) * m[k];
      nOut += m[k];
    }
  }

  
  if( tot == 0 ) {
    B const& b = *probs(m + (24-6));

    p.minNrolls = b.start;
    p.maxNrolls = b.start + b.len - 1;
    memcpy(p.probs, b.p, b.len * sizeof(*p.probs));

    // Assume you can bear off at least one with any roll.
    
    p.firstOff[0] += 1.0;
    p.maxFirstOff = 1;
    
    return;
  }


  uint const nsbr = sizeof(sbr)/sizeof(sbr[0]);
  
  if( tot - 1 < nsbr ) {
    NpipsRace const& s = sbr[tot-1];

    raceProbs(p, s, nOut, m + (24-6));
  } else {
    // very long race. Assume an average of 8 1/6 per roll, until we reach
    // table range. Very little error in those rar cases.
    
    uint n = 0;
    uint r = 0;
    
    while( tot-1 >= nsbr ) {
      tot -= 8;
      ++n;
      
      ++r;
      if( r == 6 ) {
	tot -= 1;
	r = 0;
      }
    }
    if( r >= 3 ) {
      --tot;
    }

    NpipsRace const& s = sbr[tot-1];

    raceProbs(p, s, nOut, m + (24-6));

    p.incNrolls(n);
  }
}


static void
raceProbs(float& wx, float& gx, float& go, float& bgx, float& bgo,
	  short int const m[26])
{
  {                                       assert( m[0] == 0 && m[25] == 0 ); }
  uint x[24];
  uint o[24];

  uint totx = 0;
  uint toto = 0;
  
  for(uint j = 0; j < 24; ++j) {
    int const oj = m[j+1];
    
    o[j] = oj > 0 ? oj : 0;

    toto += o[j];
    
    int const xj = m[25 - (j+1)];
    
    x[j] = xj < 0 ? -xj : 0;

    totx += x[j];
  }
  
  P px;
  P po;

  raceProbs(px, x);
  raceProbs(po, o);

  wx = 0;

  // x on roll
  
  for(uint k = px.minNrolls; k <= px.maxNrolls; ++k) {
    // x wins if o takes more

    float s = 0.0;
    for(uint j = max(k, po.minNrolls); j <= po.maxNrolls; ++j) {
      s += po.prob(j);
    }

    wx += px.prob(k) * s;
  }

  gx = bgx = 0.0;

  if( toto == 15 ) {
    for(uint k = px.minNrolls; k <= px.maxNrolls; ++k) {
      // x bears of in k, o not even 1 in less than k
    
      float s = 0.0;
      for(uint j = k-1; j < po.maxFirstOff; ++j) {
	s += po.firstOff[j];
      }
      gx += s * px.prob(k);
    }

    if( gx > 0.0 ) {
      uint totPipsHome = 0;
      
      for(uint k = 1; k < 6; ++k) {
	if( m[k+1] > 0 ) {
	  totPipsHome += m[k+1] * (6 - k);
	}
      }

      if( totPipsHome > 0 ) {
	if( ((totx+3) / 4) - 1  <= (totPipsHome+2) / 3 ) {
	  // backgammon possible

	  uint h[6];
	  
	  h[0] = 0;
	  for(uint k = 1; k < 6; ++k) {
	    h[k] = m[k+1] > 0 ? m[k+1] : 0;
	  }
	  
	  B const& b = *probs(h);

	  for(uint k = px.minNrolls; k <= px.maxNrolls; ++k) {
	    // x bears of in k, o not even out of back home

	    float s = 0.0;
	    for(uint j = 0; j < b.len; ++j) {
	      if( b.start + j >= k ) {
		s += b.p[j];
	      }
	    }

	    bgx += s * px.prob(k);
	  }
	}
      }
    }		  
  }

  go = bgo = 0.0;
    
  if( totx == 15 ) {
    for(uint k = po.minNrolls; k <= po.maxNrolls; ++k) {
      // o bears of in k, x not even 1 in less than k-1
    
      float s = 0.0;
      for(uint j = k+1-1; j < px.maxFirstOff; ++j) {
	s += px.firstOff[j];
      }
      go += s * po.prob(k);
    }

    if( go > 0.0 ) {
      uint totPipsHome = 0;
      
      for(uint k = 1; k < 6; ++k) {
	if( m[25 - (k+1)] < 0 ) {
	  totPipsHome += -m[25 - (k+1)] * (6 - k);
	}
      }

      if( totPipsHome > 0 ) {
	if( ((toto+3) / 4) <= (totPipsHome+2) / 3 ) {
	  // backgammon possible

	  uint h[6];
	  
	  h[0] = 0;
	  for(uint k = 1; k < 6; ++k) {
	    h[k] = m[25 - (k+1)] < 0 ? -m[25 - (k+1)] : 0;
	  }
	  
	  B const& b = *probs(h);

	  for(uint k = po.minNrolls; k <= po.maxNrolls; ++k) {
	    // o bears of in k, x not even out of back home

	    float s = 0.0;
	    for(uint j = 0; j < b.len; ++j) {
	      if( b.start + j > k ) {
		s += b.p[j];
	      }
	    }

	    bgo += s * po.prob(k);
	  }
	}
      }
    }
  }
}

extern "C"
void
raceProbs(int anBoard[ 2 ][ 25 ], float ar[5])
{
  short int m[26];
  memset(m, 0, sizeof(m));
  
  for(uint k = 0; k < 25; ++k) {
    if( anBoard[0][k] > 0 ) {
      m[24-k] = anBoard[0][k];
    }

    if( anBoard[1][k] > 0 ) {
      m[k+1] = -anBoard[1][k];
    }
  }

  raceProbs(ar[OUTPUT_WIN], ar[OUTPUT_WINGAMMON], ar[OUTPUT_LOSEGAMMON],
	    ar[OUTPUT_WINBACKGAMMON], ar[OUTPUT_LOSEBACKGAMMON],
	    m);
}

#endif
