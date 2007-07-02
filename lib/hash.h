/*
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
 * hash.h
 *
 * by Gary Wong, 1997
 * $Id$
 */

#ifndef _HASH_H_
#define _HASH_H_

typedef int ( *hashcomparefunc )( void *p0, void *p1 );

typedef struct _hashnode {
    struct _hashnode *phnNext;
    unsigned long l;
    void *p;
} hashnode;

typedef struct _hash {
    hashnode **aphn;
    int c, icp;
    hashcomparefunc phcf;
    unsigned long int cSize, cHits, cMisses, cLookups;
} hash;

extern int HashCreate( hash *ph, unsigned int c, hashcomparefunc phcf );
extern int HashDestroy( const hash *ph );
extern int HashAdd( hash *ph, unsigned long l, void *p );
extern int HashDelete( hash *ph, unsigned long l, void *p );
extern void *HashLookup( hash *ph, unsigned long l, void *p );

extern long StringHash( char *sz );

#endif
