/*
 * fifo.c
 *
 * by Gary Wong, 1996
 *
 */

#include "config.h"

#include <errno.h>
#include <fifo.h>
#include <stdlib.h>
#include <string.h>

int FifoCreate( fifo *pf, int cb ) {

    if( !( pf->pchBuffer = malloc( cb ) ) )
	return -1;

    pf->cbSize = cb;
    pf->iHead = pf->cb = 0;

    return 0;
}

int FifoDestroy( fifo *pf ) {

    free( pf->pchBuffer );

    return 0;
}

int FifoUsed( fifo *pf ) {

    return pf->cb;
}

int FifoRemaining( fifo *pf ) {

    return pf->cbSize - pf->cb;
}

int FifoCopyTo( fifo *pf, char *pch, int cch ) {

    int cFirst;
    
    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    } else if( !cch )
	return 0;
    
    if( cch > pf->cbSize - pf->cb )
	cch = pf->cbSize - pf->cb;

    cFirst = pf->iHead + pf->cb > pf->cbSize ? pf->cbSize - pf->cb :
	pf->cbSize - pf->iHead - pf->cb;

    if( cFirst > cch )
	cFirst = cch;

    memcpy( pf->pchBuffer + ( pf->iHead + pf->cb ) % pf->cbSize, pch, cFirst );

    if( cch > cFirst )
	memcpy( pf->pchBuffer, pch + cFirst, cch - cFirst );

    return cch;
}

int FifoCopyFrom( fifo *pf, char *pch, int cch ) {

    int cFirst;
    
    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    } else if( !cch )
	return 0;
    
    if( cch > pf->cb )
	cch = pf->cb;

    cFirst = pf->iHead + pf->cb > pf->cbSize ? pf->cbSize - pf->iHead : pf->cb;

    if( cFirst > cch )
	cFirst = cch;
    
    memcpy( pch, pf->pchBuffer + pf->iHead, cFirst );

    if( cch > cFirst )
	memcpy( pch + cFirst, pf->pchBuffer, cch - cFirst );

    return cch;
}

int FifoProduce( fifo *pf, int cch ) {

    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    }
    
    pf->cb += cch;

    return 0;
}

int FifoConsume( fifo *pf, int cch ) {

    if( cch < 0 ) {
	errno = EINVAL;
	return -1;
    }
    
    if( pf->cb -= cch )
	pf->iHead = ( pf->iHead + cch ) % pf->cbSize;
    else
	pf->iHead = 0;

    return 0;
}

#if HAVE_FIFOIOVEC
int FifoIOVec( fifo *pf, struct iovec *aiov, int fIn ) {

    if( fIn ) {
	if( pf->cb == pf->cbSize )
	    return 0;
	else if( pf->iHead && ( pf->iHead + pf->cb < pf->cbSize ) ) {
	    aiov[ 0 ].iov_base = pf->pchBuffer + pf->iHead + pf->cb;
	    aiov[ 0 ].iov_len = pf->cbSize - pf->iHead - pf->cb;
	    aiov[ 1 ].iov_base = pf->pchBuffer;
	    aiov[ 1 ].iov_len = pf->iHead;
	    
	    return 2;
	} else {
	    aiov[ 0 ].iov_base = pf->pchBuffer + ( ( pf->iHead + pf->cb ) %
		pf->cbSize );
	    aiov[ 0 ].iov_len = pf->cbSize - pf->cb;

	    return 1;
	}
    } else {
	if( !pf->cb )
	    return 0;
	else if( pf->iHead + pf->cb > pf->cbSize ) {
	    aiov[ 0 ].iov_base = pf->pchBuffer + pf->iHead;
	    aiov[ 0 ].iov_len = pf->cbSize - pf->iHead;
	    aiov[ 1 ].iov_base = pf->pchBuffer;
	    aiov[ 1 ].iov_len = ( pf->iHead + pf->cb ) % pf->cbSize;

	    return 2;
	} else {
	    aiov[ 0 ].iov_base = pf->pchBuffer + pf->iHead;
	    aiov[ 0 ].iov_len = pf->cb;

	    return 1;
	}
    }
}
#endif
