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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#include "backgammon.h"
#include "dice.h"
#include "mt19937int.h"
#include "isaac.h"

rng rngCurrent = RNG_MERSENNE;

static randctx rc;

#if HAVE_LIBDL
static void (*pfUserRNGSeed) (unsigned long int);
static long int (*pfUserRNGRandom) (void);
static void *pvUserRNGHandle;

static char szUserRNGSeed[ 32 ];
static char szUserRNGRandom[ 32 ];
static char szUserRNG[ MAXPATHLEN ];
#endif

static int GetManualDice( int anDice[ 2 ] ) {

  char *sz, *pz;
  int i;

  for (;;) {
  TryAgain:
      if( fInterrupt ) {
	  anDice[ 0 ] = anDice[ 1 ] = 0;
	  return -1;
      }
      
      sz = GetInput( "Enter dice: " );

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
	      puts( "You must enter two numbers between 1 and 6." );
	      goto TryAgain;
	  }
	  
	  anDice[ i ] = (int) (*pz - '0');
	  pz++;
      }

      free( sz );
      return 0;
  }
}

extern void InitRNGSeed( int n ) {
    
    switch( rngCurrent ) {
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
	/* no-op */
      break;

    }
}

/* Returns TRUE if /dev/random was available, or FALSE if system clock was
   used. */
extern int InitRNG( void ) {

    int n, h, f = FALSE;

    if( ( h = open( "/dev/random", O_RDONLY ) ) >= 0 ) {
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
    
    InitRNGSeed( n );

    return f;
}

extern int RollDice( int anDice[ 2 ] ) {

    switch( rngCurrent ) {
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
  char szFileName[ MAXPATHLEN ];

  strcpy( szUserRNG, "userrng.so" );

  /* 
   * (1)
   * Try opening shared object from standard and
   * LD_LIBRARY_PATH paths. 
   */

  strcpy( szFileName, szUserRNG );

  pvUserRNGHandle = dlopen( szFileName, RTLD_LAZY );

  if (!pvUserRNGHandle ) {
    
    /*
     * (2)
     * Try opening shared object from current directory
     */

    strcpy( szFileName, "./" );
    strcat( szFileName, szUserRNG );
    
    pvUserRNGHandle = dlopen( szFileName, RTLD_LAZY );

  }

  if (!pvUserRNGHandle ) {
    
    /* 
     * Bugger! Can't load shared library
     */

    printf ( "Could not load shared library %s.\n", szUserRNG );
    
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
    
    fputs( error, stderr );
    fputs ( "\n", stderr );
    return 0;

  }
  
  pfUserRNGRandom = (long int (*)(void)) dlsym( pvUserRNGHandle,
						szUserRNGRandom );

  if ((error = dlerror()) != NULL)  {
    
    fputs( error, stderr );
    fputs ( "\n", stderr );
    return 0;

  }

  /*
   * Everthing should be fine...
   */

  return 1;
   
  
}

extern void UserRNGClose() {

  dlclose(pvUserRNGHandle);

}

#endif /* HAVE_LIBDL */




