/*
 * dice.c
 *
 * by Gary Wong, 1999
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
#include "backgammon.h"

#if HAVE_LIBDL
#include <dlfcn.h>
#endif
#include <fcntl.h>
#if HAVE_LIBGMP
#include <gmp.h>
#endif
#include <glib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include "string.h"

#ifdef WIN32
#include <io.h>
#endif

#include "dice.h"
#include "md5.h"
#include "mt19937ar.h"
#include "isaac.h"
#include <glib/gi18n.h>

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
void *rngctxCurrent = NULL;

static int (*getDiceRandomDotOrg) (void);
static int (*GetManualDice) (unsigned int[2]);

typedef struct _rngcontext {

  /* RNG_USER */
#if HAVE_LIBDL
  void *pvUserRNGHandle;
  void (*pfUserRNGSeed) (unsigned long int);
  void (*pfUserRNGShowSeed) (void);
  long int (*pfUserRNGRandom) (void);
  char szUserRNGSeed[ 32 ];
  char szUserRNGRandom[ 32 ];
#endif /* HAVE_LIBDL */

  /* RNG_FILE */
  int hDice;
  char *szDiceFilename;

  /* RNG_ISAAC */
  randctx rc;

  /* RNG_MD5 */
  md5_uint32 nMD5; /* the current MD5 seed */

  /* RNG_MERSENNE */
  int mti;
  unsigned long mt[ N ];

  /* RNG_BBS */

#if HAVE_LIBGMP
  mpz_t zModulus, zSeed, zZero, zOne;
  int fZInit;
#endif /* HAVE_LIBGMP */


  /* common */
  unsigned long c; /* counter */
#if HAVE_LIBGMP
  mpz_t nz; /* seed */
#endif
  int n; /* seed */

} rngcontext;
  

static int
ReadDiceFile( rngcontext *rngctx );


#if HAVE_LIBGMP

static void InitRNGBBS( rngcontext *rngctx ) {
        
    if( !rngctx->fZInit ) {
	mpz_init( rngctx->zModulus );
	mpz_init( rngctx->zSeed );
	mpz_init_set_ui( rngctx->zZero, 0 );
	mpz_init_set_ui( rngctx->zOne, 1 );
	rngctx->fZInit = TRUE;
    }
}

extern int InitRNGBBSModulus( char *sz, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    if( !sz )
	return -1;
    
    InitRNGBBS( rngctx );
    
    if( mpz_set_str( rngctx->zModulus, sz, 10 ) || 
        mpz_sgn( rngctx->zModulus ) < 1 )
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

extern int InitRNGBBSFactors( char *sz0, char *sz1, void *x ) {

    mpz_t p, q;
    char *pch;
    rngcontext *rngctx = (rngcontext *) x;
    
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
	g_print( _("%s is an invalid Blum factor; using %s instead.\n"),
		 sz0, pch );
	free( pch );
    }
	
    if( !BBSGood( q ) || !mpz_cmp( p, q ) ) {
	BBSFindGood( q );

	if( !mpz_cmp( p, q ) )
	    BBSFindGood( q );
	
	pch = mpz_get_str( NULL, 10, q );
	g_print( _("%s is an invalid Blum factor; using %s instead.\n"),
		 sz1, pch );
	free( pch );
    }
	
    InitRNGBBS( rngctx );
    
    mpz_mul( rngctx->zModulus, p, q );

    mpz_clear( p );
    mpz_clear( q );
    
    return 0;
}

static int BBSGetBit( rngcontext *rngctx ) {

    mpz_powm_ui( rngctx->zSeed, rngctx->zSeed, 2, rngctx->zModulus );
    return ( mpz_get_ui( rngctx->zSeed ) & 1 );
}

static int BBSGetTrit( rngcontext *rngctx ) {

    /* Return a trinary digit from a uniform distribution, given binary
       digits as inputs.  This function is perfectly distributed and
       uses the fewest number of bits on average. */

    int state = 0;

    while( 1 ) {
	switch( state ) {
	case 0:
	    state = BBSGetBit( rngctx ) + 1;
	    break;
	    
	case 1:
	    if( BBSGetBit( rngctx ) )
		state = 3;
	    else
		return 0;
	    break;
	    
	case 2:
	    if( BBSGetBit( rngctx ) )
		return 2;
	    else
		state = 4;
	    break;
	    
	case 3:
	    if( BBSGetBit( rngctx ) )
		return 1;
	    else
		state = 1;
	    break;
	    
	case 4:
	    if( BBSGetBit( rngctx ) )
		state = 2;
	    else
		return 1;
	    break;
	}
    }
}

static int BBSCheck( rngcontext *rngctx ) {

    return ( mpz_cmp( rngctx->zSeed, rngctx->zZero ) && 
             mpz_cmp( rngctx->zSeed, rngctx->zOne ) ) ? 0 : -1;
}

static int BBSInitialSeedFailure( rngcontext *rngctx ) {

    g_print( _("That is not a valid initial state for the Blum, Blum and Shub "
	       "generator.\n"
	       "Please choose a different seed and/or modulus.\n" ) );
    mpz_set( rngctx->zSeed, rngctx->zZero ); /* so that BBSCheck will fail */
    
    return -1;
}

static int BBSCheckInitialSeed( rngcontext *rngctx ) {

    mpz_t z, zCycle;
    int i, iAttempt;
    
    if( mpz_sgn( rngctx->zSeed ) < 1 )
	return BBSInitialSeedFailure( rngctx );

    for( iAttempt = 0; iAttempt < 32; iAttempt++ ) {
	mpz_init_set( z, rngctx->zSeed );
    
	for( i = 0; i < 8; i++ )
	    mpz_powm_ui( z, z, 2, rngctx->zModulus );

	mpz_init_set( zCycle, z );

	for( i = 0; i < 16; i++ ) {
	    mpz_powm_ui( z, z, 2, rngctx->zModulus );
	    if( !mpz_cmp( z, zCycle ) )
		/* short cycle detected */
		break;
	}

	if( i == 16 )
	    /* we found a cycle that meets the minimum length */
	    break;

	mpz_add_ui( rngctx->zSeed, rngctx->zSeed, 1 );
    }

    if( iAttempt == 32 )
	/* we couldn't find any good seeds */
	BBSInitialSeedFailure( rngctx );    

    /* FIXME print some sort of warning if we had to modify the seed */
    
    mpz_clear( z );
    mpz_clear( zCycle );
    
    return iAttempt < 32 ? 0 : -1;
}
#endif

static void
PrintRNGWarning( void ) {

  g_print( _("WARNING: this number may not be correct if the same \n"
             "RNG is used for, say, both rollouts and interactive play.\n") );

}

extern void
PrintRNGCounter( const rng rngx, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    switch( rngx ) {
    case RNG_ANSI:
    case RNG_BSD:
    case RNG_USER:
      g_print( _("Number of calls since last seed: %lu.\n"), rngctx->c );
      PrintRNGWarning();
      break;

    case RNG_BBS:
    case RNG_ISAAC:
    case RNG_MD5:
      g_print( _("Number of calls since last seed: %lu.\n"), rngctx->c );
      
      break;
      
    case RNG_RANDOM_DOT_ORG:
      g_print( _("Number of dice used in current batch: %lu.\n"), rngctx->c );
      break;

    case RNG_FILE:
      g_print( _("Number of dice read from current file: %lu.\n"), rngctx->c );
      break;
      
    default:
      break;

    }

}


#if HAVE_LIBGMP

static void
PrintRNGSeedMP( mpz_t n ) {

  char *pch;
  pch = mpz_get_str( NULL, 10, n );
  g_print( _("The current seed is %s.\n"), pch );
  free( pch );

}

#else

static void
PrintRNGSeedNormal( int n ) {

  g_print( _("The current seed is %d.\n"), n );

}
#endif /* HAVE_LIBGMP */


extern void PrintRNGSeed( const rng rngx, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    switch( rngx ) {
    case RNG_BBS:
#if HAVE_LIBGMP
    {
	char *pch;

	pch = mpz_get_str( NULL, 10, rngctx->zSeed );
	g_print( _("The current seed is %s, "), pch );
	free( pch );
	
	pch = mpz_get_str( NULL, 10, rngctx->zModulus );
	g_print( _("and the modulus is %s.\n"), pch );
	free( pch );
	
	return;
    }
#else
	abort();
#endif
	
    case RNG_MD5:
	g_print( _("The current seed is %u.\n"), rngctx->nMD5 );
	break;

    case RNG_FILE:
        g_print( _("GNU Backgammon is reading dice from file: %s\n"), 
                 rngctx->szDiceFilename );
        break;
	
    case RNG_USER:
#if HAVE_LIBDL
	if( rngctx->pfUserRNGShowSeed ) {
	    (*rngctx->pfUserRNGShowSeed)();
	    return;
	}

	break;
#else
	abort();
#endif

    case RNG_ISAAC:
    case RNG_MERSENNE:
#if HAVE_LIBGMP
      PrintRNGSeedMP( rngctx->nz );
#else
      PrintRNGSeedNormal( rngctx->n );
#endif
      return;
      break;

    case RNG_BSD:
    case RNG_ANSI:
#if HAVE_LIBGMP
      PrintRNGSeedMP( rngctx->nz );
#else
      PrintRNGSeedNormal( rngctx->n );
#endif
      PrintRNGWarning();
      return;
      break;

    default:
	break;
    }

    g_printerr( _("You cannot show the seed with this random number "
	       "generator.\n") );
}

extern void InitRNGSeed( int n, const rng rngx, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    rngctx->n = n;
    rngctx->c = 0;
    
    switch( rngx ) {
    case RNG_ANSI:
	srand( n );
	break;

    case RNG_BBS:
#if HAVE_LIBGMP
	g_assert( rngctx->fZInit );
	mpz_set_ui( rngctx->zSeed, n );
	BBSCheckInitialSeed( rngctx );
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
          rngctx->rc.randrsl[ i ] = n;
        
	irandinit( &rngctx->rc, TRUE );
	
	break;
    }

    case RNG_MD5:
        rngctx->nMD5 = n;
	break;
    
    case RNG_MERSENNE:
	init_genrand( n, &rngctx->mti, rngctx->mt );
	break;

    case RNG_USER:
#if HAVE_LIBDL
	(*rngctx->pfUserRNGSeed) ( n );
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
static void InitRNGSeedMP( mpz_t n, rng rng, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    mpz_set( rngctx->nz, n );
    rngctx->c = 0;

    switch( rng ) {
    case RNG_ANSI:
    case RNG_BSD:
    case RNG_MERSENNE:
    case RNG_MD5: /* FIXME MD5 seed can be extended to 128 bits */
    case RNG_USER: /* FIXME add a pfUserRNGSeedMP */
	InitRNGSeed( mpz_get_ui( n ), rng, rngctx );
	break;
	    
    case RNG_BBS:
	g_assert( rngctx->fZInit );
	mpz_set( rngctx->zSeed, n );
	BBSCheckInitialSeed( rngctx );
	break;
	
    case RNG_ISAAC: {
	ub4 *achState;
	size_t cb;
	uint i;

	achState = mpz_export( NULL, &cb, -1, sizeof( ub4 ), 0, 0, n );
	
	for( i = 0; i < RANDSIZ && i < cb; i++ )
	    rngctx->rc.randrsl[ i ] = achState[ i ];

	for( ; i < RANDSIZ; i++ )
	    rngctx->rc.randrsl[ i ] = 0;
	
	irandinit( &rngctx->rc, TRUE );

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
    
extern int InitRNGSeedLong( char *sz, rng rng, void *rngctx ) {

    mpz_t n;
    
    if( mpz_init_set_str( n, sz, 10 ) || mpz_sgn( n ) < 0 ) {
	mpz_clear( n );
	return -1;
    }

    InitRNGSeedMP( n, rng, rngctx );

    mpz_clear( n );
    
    return 0;
}
#endif

extern void
CloseRNG( const rng rngx, void *p ) {

  rngcontext *rngctx = (rngcontext *) p;

  switch ( rngx ) {
  case RNG_USER:
    /* Dispose dynamically linked user module if necesary */
#if HAVE_LIBDL
    dlclose(rngctx->pvUserRNGHandle);
#else
    abort();
#endif /* HAVE_LIBDL */
    break;
  case RNG_FILE:
    /* close file */
    CloseDiceFile( rngctx );

  default:
    /* no-op */
    ;

  }
}


extern int
RNGSystemSeed( const rng rngx, void *p, unsigned long *pnSeed ) {

  int h;
  int f = FALSE;
  rngcontext *rngctx = (rngcontext *) p;
  int n;

#if HAVE_LIBGMP
    if( !pnSeed ) {
	/* We can use long seeds and don't have to save the seed anywhere,
	   so try 512 bits of state instead of 32. */
	if( ( h = open( "/dev/urandom", O_RDONLY ) ) >= 0 ) {
	    char achState[ 64 ];

	    if( read( h, achState, 64 ) == 64 ) {
		mpz_t n;
		
		close( h );

		mpz_init( n );
		mpz_import( n, 16, -1, 4, 0, 0, achState );
		InitRNGSeedMP( n, rngx, rngctx );
		mpz_clear( n );
                
		return TRUE;
	    } else
		close( h );
	}
    }
#endif /* HAVE_LIBGMP */
    
    if( ( h = open( "/dev/urandom", O_RDONLY ) ) >= 0 ) {
	f = read( h, &n, sizeof n ) == sizeof n;
	close( h );
    }

    if( !f ) {
	    GTimeVal tv;
	    g_get_current_time(&tv);
	    n = tv.tv_sec ^ tv.tv_usec;
    }

    InitRNGSeed( n, rngx, rngctx );

    if ( pnSeed )
      *pnSeed = n;

    return f;

}

extern void dice_init_callback(int (*rdo_callback) (void),
				int (*gmd_callback) (unsigned int[2]))
{
	getDiceRandomDotOrg = rdo_callback;
	GetManualDice = gmd_callback;
}

extern void *InitRNG( unsigned long *pnSeed, int *pfInitFrom,
		const int fSet, const rng rngx)
{
	int f = FALSE;
    rngcontext *rngctx =  g_new0(rngcontext, 1);

    /* misc. initialisation */

    /* Mersenne-Twister */
    rngctx->mti = N + 1;

#if HAVE_LIBGMP
    /* BBS */
    rngctx->fZInit = FALSE;
    mpz_init( rngctx->nz );
#endif /* HAVE_LIBGMP */

    /* common */
    rngctx->c = 0;

    /* */

    if ( fSet )
      f = RNGSystemSeed( rngx, rngctx, pnSeed );

    if( pfInitFrom )
      *pfInitFrom = f;

    return rngctx;

}

extern int RollDice( unsigned int anDice[ 2 ], const rng rngx, void *p ) {

    rngcontext *rngctx = (rngcontext *) p;

    switch( rngx ) {
    case RNG_ANSI:
	anDice[ 0 ] = 1+(int) (6.0*rand()/(RAND_MAX+1.0));
	anDice[ 1 ] = 1+(int) (6.0*rand()/(RAND_MAX+1.0));
        rngctx->c += 2;
	return 0;
	
    case RNG_BBS:
#if HAVE_LIBGMP
	if( BBSCheck( rngctx ) ) {
	    g_printerr( _( "The Blum, Blum and Shub generator is in an illegal "
			"state.  Please reset the\n"
			"seed and/or modulus before continuing.\n" ) );
	    return -1;
	}
	
	anDice[ 0 ] = BBSGetTrit( rngctx ) + BBSGetBit( rngctx ) * 3 + 1;
	anDice[ 1 ] = BBSGetTrit( rngctx ) + BBSGetBit( rngctx ) * 3 + 1;
        rngctx->c += 2;
	return 0;
#else
	abort();
#endif
	
    case RNG_BSD:
#if HAVE_RANDOM
	anDice[ 0 ] = 1+(int) (6.0*random()/(RAND_MAX+1.0));
	anDice[ 1 ] = 1+(int) (6.0*random()/(RAND_MAX+1.0));
        rngctx->c += 2;
	return 0;
#else
	abort();
#endif
	
    case RNG_ISAAC:
	anDice[ 0 ] = 1+(int) (6.0*irand( &rngctx->rc )/(0xFFFFFFFF+1.0));
	anDice[ 1 ] = 1+(int) (6.0*irand( &rngctx->rc )/(0xFFFFFFFF+1.0));
        rngctx->c += 2;
	return 0;
	
    case RNG_MANUAL:
	return GetManualDice( anDice );

    case RNG_MD5: {
	union _hash {
	    char ach[ 16 ];
	    md5_uint32 an[ 2 ];
	} h;
	
	md5_buffer( (char *) &rngctx->nMD5, sizeof rngctx->nMD5, &h );

	anDice[ 0 ] = h.an[ 0 ] / 715827882 + 1;
	anDice[ 1 ] = h.an[ 1 ] / 715827882 + 1;

	rngctx->nMD5++;
        rngctx->c += 2;
	
	return 0;
    }
	
	
    case RNG_MERSENNE:
	anDice[ 0 ] = 
          1+(int) (6.0*genrand_int32(&rngctx->mti, 
                                     rngctx->mt)/(0xFFFFFFFF+1.0));
	anDice[ 1 ] = 
          1+(int) (6.0*genrand_int32(&rngctx->mti,
                                     rngctx->mt)/(0xFFFFFFFF+1.0));
        rngctx->c += 2;
	return 0;
	
    case RNG_USER:
#if HAVE_LIBDL
      anDice[ 0 ] = 1 + (int) (6.0*rngctx->pfUserRNGRandom()/(0x7FFFFFFL+1.0));
      ++rngctx->c;
      if ( anDice[ 0 ] <= 0 )
        return -1;

      anDice[ 1 ] = 1 + (int) (6.0*rngctx->pfUserRNGRandom()/(0x7FFFFFFL+1.0));
      ++rngctx->c;
      if ( anDice[ 1 ] <= 0 )
        return -1;

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

      g_assert ( FALSE );

#endif /* !HAVE_SOCKETS */

      break;

    case RNG_FILE:

      anDice[ 0 ] = ReadDiceFile( rngctx );
      anDice[ 1 ] = ReadDiceFile( rngctx );
      rngctx->c += 2;

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

extern int UserRNGOpen( void *p, char *sz ) {

  char *error;
  char *szCWD;
  rngcontext *rngctx = (rngcontext *) p;

  /* 
   * (1)
   * Try opening shared object from standard and
   * LD_LIBRARY_PATH paths. 
   */

  rngctx->pvUserRNGHandle = dlopen( sz, RTLD_LAZY );

  if (!rngctx->pvUserRNGHandle ) {
    /*
     * (2)
     * Try opening shared object from current directory
     */
      szCWD = g_strdup_printf( "./%s", sz );
      rngctx->pvUserRNGHandle = dlopen( szCWD, RTLD_LAZY );
      g_free(szCWD);
  }
  
  if (!rngctx->pvUserRNGHandle ) {
      /* 
       * Bugger! Can't load shared library
       */
      if( ( error = dlerror() ) )
	  g_printerr( "%s", error );
      else
	  g_printerr( _("Could not load shared object %s."), sz );
    
      return 0;
  } 
    
  /* 
   * Shared library should now be open.
   * Get addresses for seed and random in user's RNG 
   */

  strcpy( rngctx->szUserRNGSeed , "setseed" );
  strcpy( rngctx->szUserRNGRandom , "getrandom" );

  rngctx->pfUserRNGSeed = (void (*)(unsigned long int))
    dlsym( rngctx->pvUserRNGHandle, rngctx->szUserRNGSeed );

  if ((error = dlerror()) != NULL)  {
      g_printerr( "%s: %s", rngctx->szUserRNGSeed, error );
      return 0;
  }
  
  rngctx->pfUserRNGRandom = 
    (long int (*)(void)) dlsym( rngctx->pvUserRNGHandle, 
                                rngctx->szUserRNGRandom );

  if ((error = dlerror()) != NULL)  {
      g_printerr( "%s: %s", rngctx->szUserRNGRandom, error );
      return 0;
  }

  /* this one is allowed to fail */
  rngctx->pfUserRNGShowSeed = 
    ( void (*)( void ) ) dlsym( rngctx->pvUserRNGHandle, "showseed" );
  
  /*
   * Everthing should be fine...
   */

  return 1;
   
  
}

#endif /* HAVE_LIBDL */


extern int
OpenDiceFile( void *p, const char *sz ) {

  rngcontext *rngctx = (rngcontext *) p;

  g_free(rngctx->szDiceFilename); /*initialized to NULL*/
  rngctx->szDiceFilename = g_strdup(sz);

  return ( rngctx->hDice = open(sz, O_RDONLY));

}

extern void
CloseDiceFile ( void *p ) {

  rngcontext *rngctx = (rngcontext *) p;

  if ( rngctx->hDice >= 0 )
    close( rngctx->hDice );

}


static int
ReadDiceFile( rngcontext *rngctx ) {

  unsigned char uch;
  int n;

uglyloop:
  {
  
    n = read( rngctx->hDice, &uch, 1 );

    if ( !n ) {
      /* end of file */
      g_print( _("Rewinding dice file (%s)\n"), rngctx->szDiceFilename );
      lseek( rngctx->hDice, 0, SEEK_SET );
    }
    else if ( n < 0 ) {
      g_printerr(rngctx->szDiceFilename);
      return -1;
    }
    else if ( uch >= '1' && uch <= '6' )
      return (uch - '0');

  }
  goto uglyloop;	/* This logic should be reconsidered */

  return -1;

}

extern char *
GetDiceFileName( void *p ) {

  rngcontext *rngctx = (rngcontext *) p;

  return rngctx->szDiceFilename;

}
