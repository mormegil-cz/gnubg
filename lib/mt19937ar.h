/*
 * mt19937int.h
 *
 * $Id$
 */

#ifndef _MT19937AR_H_
#define _MT19937AR_H_

#define N 624

extern void init_genrand( unsigned long s, int *mti, unsigned long mt[ N ] );
extern unsigned long genrand_int32( int *mti, unsigned long mt[ N ] );
    
#endif
