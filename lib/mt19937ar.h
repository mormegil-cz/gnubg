/*
 * mt19937int.h
 *
 * $Id$
 */

#ifndef MT19937AR_H
#define MT19937AR_H

#define N 624

extern void init_genrand( unsigned long s, int *mti, unsigned long mt[ N ] );
extern unsigned long genrand_int32( int *mti, unsigned long mt[ N ] );
    
#endif
