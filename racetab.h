// -*- C++ -*-
#if !defined( RACETAB_H )
#define RACETAB_H

#if defined( __GNUG__ )

typedef unsigned int uint;

class NpipsRace {
public:
  
  class Info {
  public:
    //  n rolls
    uint	n           : 5;

    // number of leftover pips
    uint	remainNpips : 5;

    // probablity of event as int
    uint        ip          : 22;

    float	p(void) const {
      return ip / (1.0 * (0x1L << 22));
    }
  };

  // race info for N pips
  Info*	info;

  // length of above array
  uint	nInfo;
};

extern NpipsRace
sbr[93];

#endif

#endif
