/*
 * buffer.c
 *
 * by Gary Wong, 1996
 */

#include "config.h"

#include <buffer.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int BufferReadNotify( event *pev, buffer *pb ) {

    struct iovec aiov[ 2 ];
    int cch;
    
    if( ( cch = readv( pev->h, aiov, FifoIOVec( &pb->fRead, aiov,
						 TRUE ) ) ) < 0 )
	return -1;
    else if( !cch )
	pb->fEOF = TRUE;
    else
	FifoProduce( &pb->fRead, cch );

    EventPending( pev, FALSE );
    
    if( !FifoRemaining( &pb->fRead ) )
	EventHandlerReady( &pb->evRead, FALSE, -1 );

    EventPending( pb->pevReadNotify, TRUE );
    
    return 0;
}

int BufferReadTimeout( event *pev, buffer *pb ) {

    EventTimeout( pb->pevReadNotify );
    
    return 0;
}

int BufferWriteNotify( event *pev, buffer *pb ) {

    struct iovec aiov[ 2 ];
    int cch;
    
    if( ( cch = writev( pev->h, aiov, FifoIOVec( &pb->fWrite, aiov,
						 FALSE ) ) ) < 0 )
	return -1;
    else
	FifoConsume( &pb->fWrite, cch );

    EventPending( pev, FALSE );
    
    if( !FifoUsed( &pb->fWrite ) )
	EventHandlerReady( &pb->evWrite, FALSE, -1 );
    
    EventPending( pb->pevWriteNotify, TRUE );

    return 0;
}

int BufferWriteTimeout( event *pev, buffer *pb ) {

    EventTimeout( pb->pevWriteNotify );

    return 0;
}

static eventhandler BufferReadHandler = {
    (eventhandlerfunc) BufferReadNotify,
    (eventhandlerfunc) BufferReadTimeout
};

static eventhandler BufferWriteHandler = {
    (eventhandlerfunc) BufferWriteNotify,
    (eventhandlerfunc) BufferWriteTimeout
};

int BufferCreate( buffer *pb, int hRead, int hWrite, int cbRead, int cbWrite,
		  int nReadTimeout, int nWriteTimeout,
		  event *pevRead, event *pevWrite ) {

    if( !pb ) {
	errno = EINVAL;
	return -1;
    }
    
    if( cbRead && FifoCreate( &pb->fRead, cbRead ) < 0 )
	return -1;
    
    if( cbWrite && FifoCreate( &pb->fWrite, cbWrite ) < 0 ) {
	FifoDestroy( &pb->fRead );
	return -1;
    }

    if( cbRead ) {
	EventCreate( &pb->evRead, &BufferReadHandler, pb );
	pb->evRead.h = hRead;
	pb->evRead.fWrite = FALSE;
    }

    if( cbWrite ) {
	EventCreate( &pb->evWrite, &BufferWriteHandler, pb );
	pb->evWrite.h = hWrite;
	pb->evWrite.fWrite = TRUE;
    }

    pb->fEOF = FALSE;
    
    pb->nReadTimeout = nReadTimeout;
    pb->nWriteTimeout = nWriteTimeout;

    pb->pevReadNotify = pevRead;
    pb->pevWriteNotify = pevWrite;
    
    return cbRead ? EventHandlerReady( &pb->evRead, TRUE, pb->nReadTimeout )
	: 0;
}

int BufferDestroy( buffer *pb ) {

    int hRead, hWrite;

    hRead = pb->evRead.h;
    hWrite = pb->evWrite.h;
    
    /* FIXME should hang around and write out data */
    FifoDestroy( &pb->fRead );
    FifoDestroy( &pb->fWrite );

    EventDestroy( &pb->evRead );
    EventDestroy( &pb->evWrite );

    close( hRead );
    if( hRead != hWrite )
	close( hWrite );
    
    return 0;
}

int BufferUsed( buffer *pb ) {

    return FifoUsed( &pb->fRead );
}

int BufferRemaining( buffer *pb ) {

    return FifoRemaining( &pb->fWrite );
}

int BufferCopyTo( buffer *pb, char *p, int cch ) {

    return FifoCopyTo( &pb->fWrite, p, cch );
}

int BufferCopyFrom( buffer *pb, char *p, int cch ) {

    if( pb->fEOF && !FifoUsed( &pb->fRead ) ) {
	errno = EPIPE;
	return -1;
    }
    
    return FifoCopyFrom( &pb->fRead, p, cch );
}

int BufferProduce( buffer *pb, int cch ) {
    
    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    }
    
    if( !cch )
	return 0;

    if( !pb->evWrite.fHandlerReady )
	EventHandlerReady( &pb->evWrite, TRUE, pb->nWriteTimeout );

    return FifoProduce( &pb->fWrite, cch );
}

int BufferConsume( buffer *pb, int cch ) {

    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    }
    
    if( !cch )
	return 0;

    if( !pb->evRead.fHandlerReady )
	EventHandlerReady( &pb->evRead, TRUE, pb->nReadTimeout );
    
    return FifoConsume( &pb->fRead, cch );
}

int BufferWrite( buffer *pb, char *sz ) {

    int cch = strlen( sz );

    if( BufferCopyTo( pb, sz, cch ) < cch )
	return -1;

    BufferProduce( pb, cch );

    return cch;
}

int BufferWritef( buffer *pb, char *szFormat, ... ) {

    va_list val;
    /* FIXME this is terrible... there's no limit on the vsprintf()
       buffer size!! */
    char sz[ FifoRemaining( &pb->fWrite ) > 65536 ?
	   FifoRemaining( &pb->fWrite ) : 65536 ];
    int cch;
    
    va_start( val, szFormat );
    cch = vsprintf( sz, szFormat, val );
    va_end( val );

    if( BufferCopyTo( pb, sz, cch ) < cch )
	return -1;

    BufferProduce( pb, cch );

    return cch;
}
