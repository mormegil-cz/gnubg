/*
 * speed.c
 *
 * by Gary Wong <gtw@gnu.org>, 2003
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <isaac.h>
#include <time.h>

#include "backgammon.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "i18n.h"

#define EVALS_PER_ITERATION 1024

extern void CommandCalibrate( char *sz ) {

    int iIter, i, j, k, n = -1;
    int aanBoard[ EVALS_PER_ITERATION ][ 2 ][ 25 ];
    randctx rc;
    clock_t c0, c1, c = 0;
    float ar[ NUM_OUTPUTS ];
#if USE_GTK
    void *pcc;
#endif
    
    if( sz && *sz ) {
	n = ParseNumber( &sz );

	if( n < 1 ) {
	    outputl( _("If you specify a parameter to `calibrate', "
		       "it must be a number of iterations to run.") );
	    return;
	}
    }

    irandinit( &rc, FALSE );

#if USE_GTK
    if( fX )
	pcc = GTKCalibrationStart();
#endif
    
    for( iIter = 0; iIter < n || n < 0; iIter++ ) {
	for( i = 0; i < EVALS_PER_ITERATION; i++ ) {
	    /* Generate a random board.  Don't allow chequers on the bar
	       or borne off, so we can trivially guarantee the position
	       is legal. */
	    for( j = 0; j < 25; j++ )
		aanBoard[ i ][ 0 ][ j ] =
		    aanBoard[ i ][ 1 ][ j ] = 0;

	    for( j = 0; j < 15; j++ ) {
		do
		    k = irand( &rc ) % 24;
		while( aanBoard[ i ][ 1 ][ 23 - k ] );
		aanBoard[ i ][ 0 ][ k ]++;

		do
		    k = irand( &rc ) % 24;
		while( aanBoard[ i ][ 0 ][ 23 - k ] );
		aanBoard[ i ][ 1 ][ k ]++;
	    }
	}

	if( ( c0 = clock() ) < 0 ) {
	    outputl( _("Calibration not available.") );
#if USE_GTK
	    if( fX )
		GTKCalibrationEnd( pcc );
#endif
	    return;
	}

	for( i = 0; i < EVALS_PER_ITERATION; i++ ) {
	    EvaluatePosition( aanBoard[ i ], ar, &ciCubeless, NULL );
	    if( fInterrupt )
		break;
	}
	
	if( fInterrupt )
	    break;
	
	if( ( c1 = clock() ) < 0 ) {
	    outputl( _("Calibration not available.") );
#if USE_GTK
	    if( fX )
		GTKCalibrationEnd( pcc );
#endif
	    return;
	}
	
	c += c1 - c0;

#if USE_GTK
	if( fX )
	    GTKCalibrationUpdate( pcc, ( iIter + 1.0 ) * EVALS_PER_ITERATION *
				  CLOCKS_PER_SEC / c );
	else
#endif
	if( fShowProgress && c ) {
	    outputf( "        \rCalibrating: %.0f static evaluations/second",
		     ( iIter + 1.0 ) * EVALS_PER_ITERATION *
		     CLOCKS_PER_SEC / c );
	    fflush( stdout );
	}
    }
    
#if USE_GTK
    if( fX )
	GTKCalibrationEnd( pcc );
#endif

    if( c ) {
	rEvalsPerSec = (float) iIter * EVALS_PER_ITERATION *
	    CLOCKS_PER_SEC / c;
	outputf( "\rCalibration result: %.0f static evaluations/second.\n",
		 rEvalsPerSec );
    } else
	outputl( _("Calibration incomplete.") );
}
