/*
 * buffer.h
 *
 * by Gary Wong, 1996
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <event.h>
#include <fifo.h>

typedef struct _buffer {
    fifo fRead, fWrite;
    event evRead, evWrite;
    int nReadTimeout, nWriteTimeout;
    event *pevReadNotify, *pevWriteNotify;
    int fEOF;
} buffer;

extern int BufferCreate( buffer *pb, int hRead, int hWrite,
			 int cbRead, int cbWrite,
			 int nReadTimeout, int nWriteTimeout,
			 event *pevRead, event *pevWrite );
extern int BufferDestroy( buffer *pb );

extern int BufferUsed( buffer *pb );
extern int BufferRemaining( buffer *pb );
extern int BufferCopyTo( buffer *pb, char *p, int cch );
extern int BufferCopyFrom( buffer *pb, char *p, int cch );
extern int BufferProduce( buffer *pb, int cch );
extern int BufferConsume( buffer *pb, int cch );

extern int BufferWrite( buffer *pb, char *sz );
extern int BufferWritef( buffer *pb, char *szFormat, ... );

#endif
