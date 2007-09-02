/*
 * speed.c
 *
 * by Gary Wong <gtw@gnu.org>, 2003
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
#include "speed.h"

#include <isaac.h>
#include <time.h>

#include "backgammon.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include <glib/gi18n.h>
#include "multithread.h"
#include <stdlib.h>

#define EVALS_PER_ITERATION 1024

randctx rc;
double timeTaken;

extern void RunEvals(void)
{
	int aanBoard[ EVALS_PER_ITERATION ][ 2 ][ 25 ];
    int i, j, k;
	double t;
	float ar[ NUM_OUTPUTS ];

#if USE_MULTITHREAD
	MT_Exclusive();
#endif
	for( i = 0; i < EVALS_PER_ITERATION; i++ ) {
	    /* Generate a random board.  Don't allow chequers on the bar
	       or borne off, so we can trivially guarantee the position
	       is legal. */
	    for( j = 0; j < 25; j++ )
		aanBoard[ i ][ 0 ][ j ] =
		    aanBoard[ i ][ 1 ][ j ] = 0;

	    for( j = 0; j < 15; j++ ) {
		do
		{
		    k = irand( &rc ) % 24;
		} while( aanBoard[ i ][ 1 ][ 23 - k ] );
		aanBoard[ i ][ 0 ][ k ]++;

		do
		{
		    k = irand( &rc ) % 24;
		} while( aanBoard[ i ][ 0 ][ 23 - k ] );
		aanBoard[ i ][ 1 ][ k ]++;
	    }
	}

#if USE_MULTITHREAD
	MT_Release();
	MT_SyncStart();
#else
	t = get_time();
#endif

	for( i = 0; i < EVALS_PER_ITERATION; i++ )
	{
		EvaluatePosition( NULL, aanBoard[ i ], ar, &ciCubeless, NULL );
	}

#if USE_MULTITHREAD
	if ((t = MT_SyncEnd()) != 0)
		timeTaken += t;
#else
	timeTaken += (get_time() - t);
#endif
}

extern void CommandCalibrate( char *sz )
{
	int iIter, n = -1;
	unsigned int i;
#if USE_GTK
    void *pcc = NULL;
#endif

#if USE_MULTITHREAD
	MT_SyncInit();
#endif

    if( sz && *sz ) {
	n = ParseNumber( &sz );

	if( n < 1 ) {
	    outputl( _("If you specify a parameter to `calibrate', "
		       "it must be a number of iterations to run.") );
	    return;
	}
    }

	if (clock() < 0)
	{
	    outputl( _("Calibration not available.") );
		return;
	}

    rc.randrsl[ 0 ] = time( NULL );
    for( i = 0; i < RANDSIZ; i++ )
        rc.randrsl[ i ] = rc.randrsl[ 0 ];
    irandinit( &rc, TRUE);

#if USE_GTK
    if( fX )
	pcc = GTKCalibrationStart();
#endif
    
	timeTaken = 0;
    for( iIter = 0; iIter < n || n < 0; )
	{
		double spd;
		if (fInterrupt)
			break;

#if USE_MULTITHREAD
		mt_add_tasks(MT_GetNumThreads(), TT_RUNCALIBRATIONEVALS, NULL);
		MT_WaitForTasks(NULL, 0);
		iIter += MT_GetNumThreads();
#else
		RunEvals();
		iIter++;
#endif

		if (timeTaken == 0)
			spd = 0;
		else
			spd = iIter * (EVALS_PER_ITERATION * CLOCKS_PER_SEC / timeTaken);
#if USE_GTK
		if( fX )
			GTKCalibrationUpdate(pcc, (float)spd);
		else
#endif
		if( fShowProgress ) {
			outputf( "        \rCalibrating: %.0f static evaluations/second", spd);
			fflush( stdout );
		}
    }
    
#if USE_GTK
    if( fX )
	GTKCalibrationEnd( pcc );
#endif

    if( timeTaken ) {
	rEvalsPerSec = iIter * (float)(EVALS_PER_ITERATION *
	    CLOCKS_PER_SEC / timeTaken);
	outputf( "\rCalibration result: %.0f static evaluations/second.\n",
		 rEvalsPerSec );
    } else
	outputl( _("Calibration incomplete.") );
}
