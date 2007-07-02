/* This program is free software; you can redistribute it and/or modify
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
 *
 * dynarray.h
 *
 * by Gary Wong, 1996
 * $Id$
 */

#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

typedef struct _dynarray {
    void **ap;
    unsigned int c, cp, iFinish;
    int fCompact;
} dynarray;

extern int DynArrayCreate( dynarray *pda, unsigned int c, int fCompact );
extern void DynArrayDestroy( const dynarray *pda );
extern unsigned int DynArrayAdd( dynarray *pda, void *p );
extern int DynArrayDelete( dynarray *pda, unsigned int i );
extern int DynArrayRemove( dynarray *pda, const void *p );
extern int DynArraySet( dynarray *pda, unsigned int i, void *p );

#endif
