/*
 * erftest.c
 *
 * by Gary Wong, 2001
 *
 * $Id$
 */

#include <math.h>
#include <stdio.h>

#include <config.h>

#define erf erftest
#include "erf.c"
#undef erf

int main( void ) {

#if !HAVE_ERF
    return 77; /* Automake magic number: Ignore test. */
#else
    double r;

    for( r = -8.0; r < 8.0; r += 1.0 / 0x400 )
	if( abs( erf( r ) - erftest( r ) ) >= 0.000001 ) {
	    fprintf( stderr, "System erf(%5.3f)=%.6f; our erf(%5.3f)=%.6f\n",
		     r, erf( r ), r, erftest( r ) );
	    return 1;
	}
    
    return 0;
#endif
}
