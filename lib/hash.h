/*
 * hash.h
 *
 * by Gary Wong, 1997
 */

#ifndef _HASH_H_
#define _HASH_H_

typedef int ( *hashcomparefunc )( void *p0, void *p1 );

typedef struct _hashnode {
    struct _hashnode *phnNext;
    long l;
    void *p;
} hashnode;

typedef struct _hash {
    hashnode **aphn;
    int c, icp;
    hashcomparefunc phcf;
    unsigned long int cSize, cHits, cMisses, cLookups;
} hash;

extern int HashCreate( hash *ph, int c, hashcomparefunc phcf );
extern int HashDestroy( hash *ph );
extern int HashAdd( hash *ph, unsigned long l, void *p );
extern int HashDelete( hash *ph, unsigned long l, void *p );
extern void *HashLookup( hash *ph, unsigned long l, void *p );

extern long StringHash( char *sz );

#endif
