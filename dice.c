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
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
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

#if HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#endif

#include "backgammon.h"
#include "dice.h"
#include "md5.h"
#include "mt19937int.h"
#include "isaac.h"
#include "i18n.h"
#include "external.h"

#if USE_GTK
#include "gtkgame.h"
#endif


char *aszRNG[] = {
   N_ ("ANSI"),
   N_ ("BSD"),
   N_ ("ISAAC"),
   N_ ("manual"),
   N_ ("MD5"),
   N_ ("Mersenne Twister"),
   N_ ("www.random.org"),
   N_ ("user supplied")
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

extern void PrintRNGSeed( const rng rngx ) {

    switch( rngx ) {
    case RNG_MD5:
	outputf( _("The current seed is %u.\n"), nMD5 );
	break;
	
    case RNG_USER:
#if HAVE_LIBDL
	if( pfUserRNGShowSeed ) {
	    (*pfUserRNGShowSeed)();
	    break;
	}

	/* otherwise fall through */
#else
	abort();
#endif
    default:
	outputl( _("You cannot show the seed with this random number "
		 "generator.") );
    }
}

extern void InitRNGSeed( int n, const rng rngx ) {
    
    switch( rngx ) {
    case RNG_ANSI:
	srand( n );
	break;

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
	/* no-op */
      break;

    }
}

/* Returns TRUE if /dev/urandom was available, or FALSE if system clock was
   used. */
extern int InitRNG( int *pnSeed, int fSet, const rng rngx ) {

    int n, h, f = FALSE;

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

#ifdef HAVE_SOCKETS

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

    strcpy ( szHostname, "www.random.org:80" );

    if ( ( h = ExternalSocket ( &psa, &cb, szHostname ) ) < 0 ) {
      outputerr ( szHostname );
      return -1;
    }

    /* connect */

    if ( ( connect ( h, psa, cb ) ) < 0 ) {
      outputerr ( szHostname );
      return -1;
    }

    /* read next set of numbers */

    if ( ExternalWrite ( h, szHTTP, strlen ( szHTTP ) + 1 ) < 0 ) {
      outputerr ( szHTTP );
      close ( h );
      return -1;
    }

    /* read data from web-server */

    if ( ! ( nBytesRead = read ( h, acBuf, sizeof ( acBuf ) ) ) ) {
      outputerr ( "reading data" );
      close ( h );
      return -1;
    }

    /* close socket */

    close ( h );

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

extern int  UserRNGOpen() {

  char *error;

  /* 
   * (1)
   * Try opening shared object from standard and
   * LD_LIBRARY_PATH paths. 
   */

  pvUserRNGHandle = dlopen( "userrng.so", RTLD_LAZY );

  if (!pvUserRNGHandle )
    /*
     * (2)
     * Try opening shared object from current directory
     */
    pvUserRNGHandle = dlopen( "./userrng.so", RTLD_LAZY );

  if (!pvUserRNGHandle ) {
      /* 
       * Bugger! Can't load shared library
       */
      if( ( error = dlerror() ) )
	  outputerrf( "userrng.so: %s", error );
      else
	  outputerrf( _("Could not load shared library userrng.so.") );
    
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

extern void UserRNGClose() {

  dlclose(pvUserRNGHandle);

}

#endif /* HAVE_LIBDL */
