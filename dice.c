/*
 * dice.c
 *
 * by Gary Wong, 1999
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

#if HAVE_LIBDL
#include <dlfcn.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_GMP_H
#include <gmp.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef WIN32
#include <errno.h>
#include <sys/types.h>

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#endif /* #if HAVE_SYS_SOCKET_H */

#else /* #ifndef WIN32 */
#include <winsock.h>

#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define ENAMETOOLONG            WSAENAMETOOLONG
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define ENOTEMPTY               WSAENOTEMPTY
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE
#endif /* #ifndef WIN32 */

#include "backgammon.h"
#include "dice.h"
#include "md5.h"
#include "mt19937int.h"
#include "isaac.h"
#include "i18n.h"
#include "external.h"
#include "path.h"

#if USE_GTK
#include "gtkgame.h"
#endif


char *aszRNG[ NUM_RNGS ] = {
   N_ ("ANSI"),
   N_ ("Blum, Blum and Shub"),
   N_ ("BSD"),
   N_ ("ISAAC"),
   N_ ("manual"),
   N_ ("MD5"),
   N_ ("Mersenne Twister"),
   N_ ("www.random.org"),
   N_ ("user supplied"),
   N_ ("read-from-file")
};


rng rngCurrent = RNG_MERSENNE;

static randctx rc;

static md5_uint32 nMD5; /* the current MD5 seed */

#if HAVE_LIBDL
static void (*pfUserRNGSeed) (unsigned long int);
static void (*pfUserRNGShowSeed) (void);
static long int (*pfUserRNGRandom) (void);
static void *pvUserRNGHandle;

static char szUserRNGSeed[ 32 ];
static char szUserRNGRandom[ 32 ];
#endif

char szDiceFilename[ BIG_PATH ];

static int
ReadDiceFile( void );


static int GetManualDice( int anDice[ 2 ] ) {

  char *sz, *pz;
  int i;

#if USE_GTK
  if( fX ) {
      if( GTKGetManualDice( anDice ) ) {
	  fInterrupt = 1;
	  return -1;
      } else
	  return 0;
  }
#endif
  
  for (;;) {
  TryAgain:
      if( fInterrupt ) {
	  anDice[ 0 ] = anDice[ 1 ] = 0;
	  return -1;
      }
      
      sz = GetInput( _("Enter dice: ") );

      if( fInterrupt ) {
	  anDice[ 0 ] = anDice[ 1 ] = 0;
	  return -1;
      }
      
      /* parse input and read a couple of dice */
      /* any string with two numbers is allowed */
    
      pz = sz;
      
      for ( i=0; i<2; i++ ) {
	  while ( *pz && ( ( *pz < '1' ) || ( *pz > '6' ) ) )
	      pz++;

	  if ( !*pz ) {
	      outputl( _("You must enter two numbers between 1 and 6.") );
	      goto TryAgain;
	  }
	  
	  anDice[ i ] = (int) (*pz - '0');
	  pz++;
      }

      free( sz );
      return 0;
  }
}

#if HAVE_LIBGMP

static mpz_t zModulus, zSeed, zZero, zOne;
static int fZInit;

static void InitRNGBBS( void ) {
        
    if( !fZInit ) {
	mpz_init( zModulus );
	mpz_init( zSeed );
	mpz_init_set_ui( zZero, 0 );
	mpz_init_set_ui( zOne, 1 );
	fZInit = TRUE;
    }
}

extern int InitRNGBBSModulus( char *sz ) {

    if( !sz )
	return -1;
    
    InitRNGBBS();
    
    if( mpz_set_str( zModulus, sz, 10 ) || mpz_sgn( zModulus ) < 1 )
	return -1;

    return 0;
}

static int BBSGood( mpz_t x ) {

    static mpz_t z19;
    static int f19;

    if( !f19 ) {
	mpz_init_set_ui( z19, 19 );
	f19 = TRUE;
    }
    
    return ( ( mpz_get_ui( x ) & 3 ) == 3 ) && mpz_cmp( x, z19 ) >= 0 &&
	mpz_probab_prime_p( x, 10 );
}

static int BBSFindGood( mpz_t x ) {

    do
	mpz_add_ui( x, x, 1 );
    while( !BBSGood( x ) );

    return 0;
}

extern int InitRNGBBSFactors( char *sz0, char *sz1 ) {

    mpz_t p, q;
    char *pch;
    
    if( !sz0 || !sz1 )
	return -1;
    
    if( mpz_init_set_str( p, sz0, 10 ) || mpz_sgn( p ) < 1 ) {
	mpz_clear( p );
	return -1;
    }
    
    if( mpz_init_set_str( q, sz1, 10 ) || mpz_sgn( p ) < 1 ) {
	mpz_clear( p );
	mpz_clear( q );
	return -1;
    }
    
    if( !BBSGood( p ) ) {
	BBSFindGood( p );

	pch = mpz_get_str( NULL, 10, p );
	outputf( _("%s is an invalid Blum factor; using %s instead.\n"),
		 sz0, pch );
	free( pch );
    }
	
    if( !BBSGood( q ) || !mpz_cmp( p, q ) ) {
	BBSFindGood( q );

	if( !mpz_cmp( p, q ) )
	    BBSFindGood( q );
	
	pch = mpz_get_str( NULL, 10, q );
	outputf( _("%s is an invalid Blum factor; using %s instead.\n"),
		 sz1, pch );
	free( pch );
    }
	
    InitRNGBBS();
    
    mpz_mul( zModulus, p, q );

    mpz_clear( p );
    mpz_clear( q );
    
    return 0;
}

static int BBSGetBit( void ) {

    mpz_powm_ui( zSeed, zSeed, 2, zModulus );
    return ( mpz_get_ui( zSeed ) & 1 );
}

static int BBSGetTrit( void ) {

    /* Return a trinary digit from a uniform distribution, given binary
       digits as inputs.  This function is perfectly distributed and
       uses the fewest number of bits on average. */

    int state = 0;

    while( 1 ) {
	switch( state ) {
	case 0:
	    state = BBSGetBit() + 1;
	    break;
	    
	case 1:
	    if( BBSGetBit() )
		state = 3;
	    else
		return 0;
	    break;
	    
	case 2:
	    if( BBSGetBit() )
		return 2;
	    else
		state = 4;
	    break;
	    
	case 3:
	    if( BBSGetBit() )
		return 1;
	    else
		state = 1;
	    break;
	    
	case 4:
	    if( BBSGetBit() )
		state = 2;
	    else
		return 1;
	    break;
	}
    }
}

static int BBSCheck( void ) {

    return ( mpz_cmp( zSeed, zZero ) && mpz_cmp( zSeed, zOne ) ) ? 0 : -1;
}

static int BBSInitialSeedFailure( void ) {

    outputl( _("That is not a valid initial state for the Blum, Blum and Shub "
	       "generator.\n"
	       "Please choose a different seed and/or modulus." ) );
    mpz_set( zSeed, zZero ); /* so that BBSCheck will fail */
    
    return -1;
}

static int BBSCheckInitialSeed( void ) {

    mpz_t z, zCycle;
    int i, iAttempt;
    
    if( mpz_sgn( zSeed ) < 1 )
	return BBSInitialSeedFailure();

    for( iAttempt = 0; iAttempt < 32; iAttempt++ ) {
	mpz_init_set( z, zSeed );
    
	for( i = 0; i < 8; i++ )
	    mpz_powm_ui( z, z, 2, zModulus );

	mpz_init_set( zCycle, z );

	for( i = 0; i < 16; i++ ) {
	    mpz_powm_ui( z, z, 2, zModulus );
	    if( !mpz_cmp( z, zCycle ) )
		/* short cycle detected */
		break;
	}

	if( i == 16 )
	    /* we found a cycle that meets the minimum length */
	    break;

	mpz_add_ui( zSeed, zSeed, 1 );
    }

    if( iAttempt == 32 )
	/* we couldn't find any good seeds */
	BBSInitialSeedFailure();    

    /* FIXME print some sort of warning if we had to modify the seed */
    
    mpz_clear( z );
    mpz_clear( zCycle );
    
    return iAttempt < 32 ? 0 : -1;
}
#endif

extern void PrintRNGSeed( const rng rngx ) {

    switch( rngx ) {
    case RNG_BBS:
#if HAVE_LIBGMP
    {
	char *pch;

	pch = mpz_get_str( NULL, 10, zSeed );
	outputf( _("The current seed is %s, "), pch );
	free( pch );
	
	pch = mpz_get_str( NULL, 10, zModulus );
	outputf( _("and the modulus is %s.\n"), pch );
	free( pch );
	
	return;
    }
#else
	abort();
#endif
	
    case RNG_MD5:
	outputf( _("The current seed is %u.\n"), nMD5 );
	break;

    case RNG_FILE:
        outputf( _("GNU Backgammon is reading dice from file: %s\n"), 
                 szDiceFilename );
        break;
	
    case RNG_USER:
#if HAVE_LIBDL
	if( pfUserRNGShowSeed ) {
	    (*pfUserRNGShowSeed)();
	    return;
	}

	break;
#else
	abort();
#endif
    default:
	break;
    }

    outputl( _("You cannot show the seed with this random number "
	       "generator.") );
}

extern void InitRNGSeed( int n, const rng rngx ) {
    
    switch( rngx ) {
    case RNG_ANSI:
	srand( n );
	break;

    case RNG_BBS:
#if HAVE_LIBGMP
	assert( fZInit );
	mpz_set_ui( zSeed, n );
	BBSCheckInitialSeed();
	break;
#else
	abort();
#endif
	
    case RNG_BSD:
#if HAVE_RANDOM
	srandom( n );
	break;
#else
        abort();
#endif

    case RNG_ISAAC: {
	int i;

	for( i = 0; i < RANDSIZ; i++ )
	    rc.randrsl[ i ] = n;

	irandinit( &rc, TRUE );
	
	break;
    }

    case RNG_MD5:
	nMD5 = n;
	break;
    
    case RNG_MERSENNE:
	sgenrand( n );
	break;

    case RNG_USER:
#if HAVE_LIBDL
	(*pfUserRNGSeed) ( n );
	break;
#else
	abort();
#endif

    case RNG_MANUAL:
    case RNG_RANDOM_DOT_ORG:
    case RNG_FILE:
	/* no-op */
      break;

    default:
      break;

    }
}

#if HAVE_LIBGMP
static void InitRNGSeedMP( mpz_t n, rng rng ) {

    switch( rng ) {
    case RNG_ANSI:
    case RNG_BSD:
    case RNG_MERSENNE:
    case RNG_MD5: /* FIXME MD5 seed can be extended to 128 bits */
    case RNG_USER: /* FIXME add a pfUserRNGSeedMP */
	InitRNGSeed( mpz_get_ui( n ), rng );
	break;
	    
    case RNG_BBS:
	assert( fZInit );
	mpz_set( zSeed, n );
	BBSCheckInitialSeed();
	break;
	
    case RNG_ISAAC: {
	ub4 *achState;
	size_t cb;
	int i;

	achState = mpz_export( NULL, &cb, -1, sizeof( ub4 ), 0, 0, n );
	
	for( i = 0; i < RANDSIZ && i < cb; i++ )
	    rc.randrsl[ i ] = achState[ i ];

	for( ; i < RANDSIZ; i++ )
	    rc.randrsl[ i ] = 0;
	
	irandinit( &rc, TRUE );

	free( achState );
	
	break;
    }
    
    case RNG_MANUAL:
    case RNG_RANDOM_DOT_ORG:
    case RNG_FILE:
	/* no-op */
	break;
        
    default:
      break;

    }
}
    
extern int InitRNGSeedLong( char *sz, rng rng ) {

    mpz_t n;
    
    if( mpz_init_set_str( n, sz, 10 ) || mpz_sgn( n ) < 1 ) {
	mpz_clear( n );
	return -1;
    }

    InitRNGSeedMP( n, rng );

    mpz_clear( n );
    
    return 0;
}
#endif

/* Returns TRUE if /dev/urandom was available, or FALSE if system clock was
   used. */
extern int InitRNG( int *pnSeed, int fSet, const rng rngx ) {

    int n, h, f = FALSE;

#if HAVE_LIBGMP
    if( !pnSeed && fSet ) {
	/* We can use long seeds and don't have to save the seed anywhere,
	   so try 512 bits of state instead of 32. */
	if( ( h = open( "/dev/urandom", O_RDONLY ) ) >= 0 ) {
	    char achState[ 64 ];

	    if( read( h, achState, 64 ) == 64 ) {
		mpz_t n;
		
		close( h );

		mpz_init( n );
		mpz_import( n, 16, -1, 4, 0, 0, achState );
		InitRNGSeedMP( n, rngx );
		mpz_clear( n );
		
		return TRUE;
	    } else
		close( h );
	}
    }
#endif
    
    if( ( h = open( "/dev/urandom", O_RDONLY ) ) >= 0 ) {
	f = read( h, &n, sizeof n ) == sizeof n;
	close( h );
    }

    if( !f ) {
#if HAVE_GETTIMEOFDAY
	struct timeval tv;
	struct timezone tz;

	if( !gettimeofday( &tv, &tz ) )
	    n = tv.tv_sec ^ tv.tv_usec;
	else
#endif
	    n = time( NULL );
    }

    if( pnSeed )
	*pnSeed = n;

    if( fSet )
	InitRNGSeed( n, rngx );

    return f;
}


/* 
 * Fetch random numbers from www.random.org
 *
 */

#if HAVE_SOCKETS

static int
getDiceRandomDotOrg ( void ) {

#define BUFLENGTH 500

  static int nCurrent = -1;
  static int anBuf [ BUFLENGTH ]; 
  static int nRead;
 

  int h;
  int cb;

  int nBytesRead, i;
  struct sockaddr *psa;
  char szHostname[ 80 ];
  char szHTTP[] = 
    "GET http://www.random.org/cgi-bin/randnum?num=500&min=0&max=5&col=1\n";
  char acBuf [ 2048 ];

  /* 
   * Suggestions for improvements:
   * - use proxy
   */

  /*
   * Return random number
   */

  if ( ( nCurrent >= 0) && ( nCurrent < nRead ) )
     return anBuf [ nCurrent++ ];
  else {

    outputf ( _("Fetching %d random numbers from <www.random.org>\n"), BUFLENGTH );
    outputx ();

    /* fetch new numbers */

    /* open socket */

    strcpy( szHostname, "www.random.org:80" );

    if ( ( h = ExternalSocket( &psa, &cb, szHostname ) ) < 0 ) {
      outputerr ( szHostname );
      return -1;
    }

    /* connect */

#ifdef WIN32
    if ( connect( (SOCKET) h, (const struct sockaddr*) psa, cb ) < 0 ) {
#else
    if ( ( connect( h, psa, cb ) ) < 0 ) {
#endif /* WIN32 */
      outputerr( szHostname );
      return -1;
    }

    /* read next set of numbers */

    if ( ExternalWrite( h, szHTTP, strlen ( szHTTP ) + 1 ) < 0 ) {
      outputerr( szHTTP );
      close( h );
      return -1;
    }

    /* read data from web-server */

#ifdef WIN32
	/* reading from sockets doesn't work on Windows
	   use recv instead */
	if ( ! ( nBytesRead = recv( (SOCKET) h, acBuf, sizeof ( acBuf ), 0 ) ) ) {
#else
	if ( ! ( nBytesRead = read( h, acBuf, sizeof ( acBuf ) ) ) ) {
#endif
      outputerr( "reading data" );
      close( h );
      return -1;
    }

    /* close socket */

#ifdef WIN32
    closesocket( (SOCKET) h ); 
#else
    close ( h );
#endif

    /* parse string */

    outputl ( _("Done." ) );
    outputx ();

    i = 0; nRead = 0;
    for ( i = 0; i < nBytesRead ; i++ ) {

      if ( ( acBuf[ i ] >= '0' ) && ( acBuf[ i ] <= '5' ) ) {
         anBuf[ nRead ] = 1 + (int) (acBuf[ i ] - '0');
         nRead++;
      }

    }

    nCurrent = 1;
    return anBuf[ 0 ];
  }

}

#endif /* HAVE_SOCKETS */


extern int RollDice( int anDice[ 2 ], const rng rngx ) {

    switch( rngx ) {
    case RNG_ANSI:
	anDice[ 0 ] = ( rand() % 6 ) + 1;
	anDice[ 1 ] = ( rand() % 6 ) + 1;
	return 0;
	
    case RNG_BBS:
#if HAVE_LIBGMP
	if( BBSCheck() ) {
	    outputl( _( "The Blum, Blum and Shub generator is in an illegal "
			"state.  Please reset the\n"
			"seed and/or modulus before continuing." ) );
	    return -1;
	}
	
	anDice[ 0 ] = BBSGetTrit() + BBSGetBit() * 3 + 1;
	anDice[ 1 ] = BBSGetTrit() + BBSGetBit() * 3 + 1;
	return 0;
#else
	abort();
#endif
	
    case RNG_BSD:
#if HAVE_RANDOM
	anDice[ 0 ] = ( random() % 6 ) + 1;
	anDice[ 1 ] = ( random() % 6 ) + 1;
	return 0;
#else
	abort();
#endif
	
    case RNG_ISAAC:
	anDice[ 0 ] = ( irand( &rc ) % 6 ) + 1;
	anDice[ 1 ] = ( irand( &rc ) % 6 ) + 1;
	return 0;
	
    case RNG_MANUAL:
	return GetManualDice( anDice );

    case RNG_MD5: {
	union _hash {
	    char ach[ 16 ];
	    md5_uint32 an[ 2 ];
	} h;
	
	md5_buffer( (char *) &nMD5, sizeof nMD5, &h );

	anDice[ 0 ] = h.an[ 0 ] / 715827882 + 1;
	anDice[ 1 ] = h.an[ 1 ] / 715827882 + 1;

	nMD5++;
	
	return 0;
    }
	
	
    case RNG_MERSENNE:
	anDice[ 0 ] = ( genrand() % 6 ) + 1;
	anDice[ 1 ] = ( genrand() % 6 ) + 1;
	return 0;
	
    case RNG_USER:
#if HAVE_LIBDL
	if( ( anDice[ 0 ] = ( (*pfUserRNGRandom) () % 6 ) + 1 ) <= 0 ||
	    ( anDice[ 1 ] = ( (*pfUserRNGRandom) () % 6 ) + 1 ) <= 0 )
	    return -1;
	else
	    return 0;
#else
	abort();
#endif

    case RNG_RANDOM_DOT_ORG:
#if HAVE_SOCKETS

      anDice[ 0 ] = getDiceRandomDotOrg();
      anDice[ 1 ] = getDiceRandomDotOrg();

      if ( anDice[ 0 ] <= 0 || anDice[ 1 ] <= 0 )
        return -1;
      else
        return 0;

#else /* HAVE_SOCKETS */

      assert ( FALSE );

#endif /* !HAVE_SOCKETS */

      break;

    case RNG_FILE:

      anDice[ 0 ] = ReadDiceFile();
      anDice[ 1 ] = ReadDiceFile();

      if ( anDice[ 0 ] <= 0 || anDice[ 1 ] <= 0 )
        return -1;
      else
        return 0;

      break;

    default:
      break;

    }

    return -1;
}

#if HAVE_LIBDL
/*
 * Functions for handling the user supplied RNGs
 * Ideas for further development:
 * - read szUserRNG, szUserRNGSeed, and szUserRNGRandom
 *   from user input
 */

extern int UserRNGOpen( char *sz ) {

  char *error;
#if __GNUC__
  char szCWD[ strlen( sz ) + 3 ];
#elif HAVE_ALLOCA
  char *szCWD = alloca( strlen( szOrig ) + 3 );
#else
  char szCWD[ 4096 ];
#endif

  /* 
   * (1)
   * Try opening shared object from standard and
   * LD_LIBRARY_PATH paths. 
   */

  pvUserRNGHandle = dlopen( sz, RTLD_LAZY );

  if (!pvUserRNGHandle ) {
    /*
     * (2)
     * Try opening shared object from current directory
     */
      sprintf( szCWD, "./%s", sz );
      pvUserRNGHandle = dlopen( szCWD, RTLD_LAZY );
  }
  
  if (!pvUserRNGHandle ) {
      /* 
       * Bugger! Can't load shared library
       */
      if( ( error = dlerror() ) )
	  outputerrf( "%s", error );
      else
	  outputerrf( _("Could not load shared object %s."), sz );
    
      return 0;
  } 
    
  /* 
   * Shared library should now be open.
   * Get addresses for seed and random in user's RNG 
   */

  strcpy( szUserRNGSeed , "setseed" );
  strcpy( szUserRNGRandom , "getrandom" );

  pfUserRNGSeed = (void (*)(unsigned long int))
      dlsym( pvUserRNGHandle, szUserRNGSeed );

  if ((error = dlerror()) != NULL)  {
      outputerrf( "%s: %s", szUserRNGSeed, error );
      return 0;
  }
  
  pfUserRNGRandom = (long int (*)(void)) dlsym( pvUserRNGHandle,
						szUserRNGRandom );

  if ((error = dlerror()) != NULL)  {
      outputerrf( "%s: %s", szUserRNGRandom, error );
      return 0;
  }

  /* this one is allowed to fail */
  pfUserRNGShowSeed = ( void (*)( void ) ) dlsym( pvUserRNGHandle,
						  "showseed" );
  
  /*
   * Everthing should be fine...
   */

  return 1;
   
  
}

extern void UserRNGClose( void ) {

  dlclose(pvUserRNGHandle);

}

#endif /* HAVE_LIBDL */


static int hDice = -1;

extern int
OpenDiceFile( const char *sz ) {

  strcpy( szDiceFilename, sz );

  return ( hDice = PathOpen( sz, NULL, 0 ) );

}

extern void
CloseDiceFile ( void ) {

  if ( hDice >= 0 )
    close( hDice );

}


static int
ReadDiceFile( void ) {

  unsigned char uch;
  int n;

  while ( 1 ) {
  
    n = read( hDice, &uch, 1 );

    if ( !n ) {
      /* end of file */
      outputf( _("Rewinding dice file (%s)\n"), szDiceFilename );
      lseek( hDice, 0, SEEK_SET );
    }
    else if ( n < 0 ) {
      outputerr(szDiceFilename);
      return -1;
    }
    else if ( uch >= '1' && uch <= '6' )
      return (uch - '0');

  }

  return -1;

}
