/*
 * event.c
 *
 * by Gary Wong, 1996
 *
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <list.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

#if EVENT_DEBUG
#include <stdio.h>
#endif

#include <event.h>

#define MAXTVSEC 0x7FFFFFFF
#define MAXTVUSEC 999999

static fd_set fdsRead, fdsWrite;

static int cDescriptors;

typedef event *eventpair[ 2 ];
static eventpair *aapev;

static list lev;
static event evNever;

static int fGo;

static int TimeEarlier( struct timeval *ptv0, struct timeval *ptv1 ) {

    if( ptv0->tv_sec < ptv1->tv_sec )
	return TRUE;

    return ( ptv0->tv_sec == ptv1->tv_sec ) &&
	( ptv0->tv_usec < ptv1->tv_usec );
}

extern int EventHandlerReady( event *pev, int fReady, long nTimeout ) {

    list *pl;
    event *pevSearch;
    struct timezone tz;

    assert( pev );

    assert( pev->h >= -1 );
    assert( pev->h < cDescriptors );

    if( !fReady && !pev->fHandlerReady )
	return 0;

    if( pev->fHandlerReady ) {
	pev->fHandlerReady = FALSE;

	if( pev->h > -1 ) {
	    if( pev->fWrite )
		FD_CLR( pev->h, &fdsWrite );
	    else
		FD_CLR( pev->h, &fdsRead );

	    aapev[ pev->h ][ pev->fWrite ] = NULL;
	}

	ListDelete( pev->pl );
    }

    if( fReady ) {        
	pev->fHandlerReady = TRUE;

	if( pev->h > -1 ) {
	    if( pev->fWrite )
		FD_SET( pev->h, &fdsWrite );
	    else
		FD_SET( pev->h, &fdsRead );

	    aapev[ pev->h ][ pev->fWrite ] = pev;
	}

	if( nTimeout >= 0 ) {
	    gettimeofday( &pev->tv, &tz );

	    pev->tv.tv_sec += nTimeout / 1000;
	    pev->tv.tv_usec += ( nTimeout % 1000 ) * 1000;

	    if( pev->tv.tv_usec > 1000000 ) {
		pev->tv.tv_usec -= 1000000;
		pev->tv.tv_sec++;
	    }
	} else {
	    pev->tv.tv_sec = MAXTVSEC;
	    pev->tv.tv_usec = MAXTVUSEC;
	}

#if EVENT_DEBUG
	printf( "adding event %x at time %ld.%06ld\n",
		pev, pev->tv.tv_sec, pev->tv.tv_usec );
#endif	
	
	for( pl = lev.plNext; ; pl = pl->plNext ) {
	    pevSearch = pl->p;

	    if( !TimeEarlier( &pevSearch->tv, &pev->tv ) ) {
		pev->pl = ListInsert( pevSearch->pl, pev );
		break;
	    }
	}
    }
    
    return 0;    
}

extern int EventCreate( event *pev, eventhandler *peh, void *pvData ) {

    pev->peh = peh;
    pev->fPending = FALSE;
    pev->fHandlerReady = FALSE;
    pev->pvData = pvData;
    pev->h = -1;

    return 0;
}

extern int EventDestroy( event *pev ) {

    if( pev->fHandlerReady )
	EventHandlerReady( pev, FALSE, -1 );

    return 0;
}

extern int EventProcess( event *pev ) {

    if( pev->peh && pev->peh->fnEvent )
	return pev->peh->fnEvent( pev, pev->pvData );

    pev->fPending = FALSE;
    
    return 0;
}

extern int EventTimeout( event *pev ) {

    if( pev->peh && pev->peh->fnTimeout )
	return pev->peh->fnTimeout( pev, pev->pvData );

    pev->fPending = FALSE;

    return 0;
}

extern int EventPending( event *pev, int fPending ) {

    /* FIXME should this call the handler if it wasn't previously pending? */
    
    pev->fPending = fPending;

    return 0;
}

extern int InitEvents( void ) {

    if( ( cDescriptors = getdtablesize() ) > FD_SETSIZE )
	cDescriptors = FD_SETSIZE;

    if( !( aapev = calloc( cDescriptors, sizeof( eventpair ) ) ) )
	return -1;
    
    FD_ZERO( &fdsRead );
    FD_ZERO( &fdsWrite );

    ListCreate( &lev );

    evNever.peh = NULL;
    evNever.fPending = FALSE;
    evNever.fHandlerReady = TRUE;
    evNever.pl = &lev;
    evNever.tv.tv_sec = MAXTVSEC;
    evNever.tv.tv_usec = MAXTVUSEC;
    evNever.h = -1;
    lev.p = &evNever;
    
    return 0;
}

extern int StopEvents( void ) {

    fGo = FALSE;

    return 0;
}

extern int HandleEvents( void ) {

    struct timeval tv;
    struct timezone tz;
    event *pev;
    fd_set fdsReadable, fdsWritable;
    list *pl;
    int ftv;
    
    fGo = TRUE;

    while( TRUE ) {
	gettimeofday( &tv, &tz );

	for( pl = lev.plNext; pev = pl->p, pl != &lev; ) {
	    list *plNext = pl->plNext;

#if EVENT_DEBUG	    
	    printf( "found event %x\n", pev );
#endif	    
	    if( TimeEarlier( &tv, &pev->tv ) )
		break;

/* FIXME BUG BUG BUG... what happens if the timeout handler frees
   plNext? */

#if EVENT_DEBUG	    
	    printf( "it has expired and is%s pending", pev->fPending ? "" :
		    " not" );
#endif
	    if( !pev->fPending )
		EventTimeout( pev );
#if 0
	    /* hack; only process one timeout */
	    break;
#else
	    /* buggy if timeouts free each other */
	    pl = plNext;
#endif	    
	}

	/* NB have to traverse list again in case the timeout
	   handler added another that wasn't seen in the
	   initial traversal: */
	for( pl = lev.plNext; pev = pl->p, pl != &lev; ) {
	    list *plNext = pl->plNext;

	    if( TimeEarlier( &tv, &pev->tv ) )
		break;

	    pl = plNext;
	}

	if( ( ftv = ( pev->tv.tv_sec < MAXTVSEC ) ) ) {
	    tv.tv_sec = pev->tv.tv_sec - tv.tv_sec;
	    tv.tv_usec = pev->tv.tv_usec - tv.tv_usec;

	    if( tv.tv_usec < 0 ) {
		tv.tv_usec += 1000000;
		tv.tv_sec--;
	    }
	}
	
	for( pl = lev.plNext; pl != &lev; pl = pl->plNext ) {
	    pev = pl->p;
	    
	    if( pev->fPending ) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ftv = TRUE;
		
		break;
	    }
	}

	fdsReadable = fdsRead;
	fdsWritable = fdsWrite;

#if EVENT_DEBUG
	{
	    int i;
	    
	    for( i = 0; i < 64; i++ ) {
		int fRead = FD_ISSET( i, &fdsReadable );
		int fWrite = FD_ISSET( i, &fdsWritable );
		
		if( fRead )
		    putc( fWrite ? '*' : 'r', stderr );
		else
		    putc( fWrite ? 'w' : '.', stderr );
	    }

	    putc( '\n', stderr );
	}
#endif

	if( select( cDescriptors, &fdsReadable, &fdsWritable, NULL,
		    ftv ? &tv : NULL ) < 0 ) {
	    if( errno == EINTR )
		continue;

	    syslog( LOG_ERR, "select: %m" );

	    return -1;
	}

#if EVENT_DEBUG
	{
	    int i;
	    
	    for( i = 0; i < 64; i++ ) {
		int fRead = FD_ISSET( i, &fdsReadable );
		int fWrite = FD_ISSET( i, &fdsWritable );
		
		if( fRead )
		    putc( fWrite ? '*' : 'r', stderr );
		else
		    putc( fWrite ? 'w' : '.', stderr );
	    }

	    putc( '\n', stderr );
	}
#endif

	for( pl = lev.plNext; pl != &lev; pl = pl->plNext ) {
	    pev = pl->p;
	    
	    if( pev->h > -1 )
		if( FD_ISSET( pev->h, pev->fWrite ? &fdsWritable :
			      &fdsReadable ) )
		    pev->fPending = TRUE;
	}

	for( pl = lev.plNext; pl != &lev; ) {
	    list *plNext = pl->plNext;

	    pev = pl->p;
	    
/* FIXME BUG BUG BUG... what happens if the handler frees
   plNext? */

	    if( pev->fPending ) {
		EventProcess( pev );

		if( !fGo )
		    return 0;
	    }

	    pl = plNext;
	}
    }

    return 0;
}
