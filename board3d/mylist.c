/*
* mylist.c
* by Jon Kinsey, 2003
*
* Simple vector
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of version 2 of the GNU General Public License as
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

#include <stdlib.h>
#include <memory.h>
#include "mylist.h"

#define ALLOC_STEP 50

void VectorInit(vector* v, int eleSize)
{
	v->eleSize = eleSize;
	v->numElements = 0;
	v->curAllocated = 0;
	v->data = 0;
}

void VectorClear(vector* v)
{
	free(v->data);
	v->eleSize = 0;
	v->numElements = 0;
	v->curAllocated = 0;
	v->data = 0;
}

void VectorAdd(vector* v, void* ele)
{
	if (v->numElements == v->curAllocated)
	{
		v->curAllocated += ALLOC_STEP;
		v->data = realloc(v->data, v->curAllocated * v->eleSize);
	}
	memcpy(VectorGet(v, v->numElements), ele, v->eleSize);
	v->numElements++;
}

int VectorSize(vector* v)
{
	return v->numElements;
}

void* VectorGet(vector* v, int pos)
{
	return &((char*)v->data)[pos * v->eleSize];
}

int VectorFind(vector* v, void* ele)
{
	int i;
	for (i = 0; i < VectorSize(v); i++)
		if (!memcmp(ele, VectorGet(v, i), v->eleSize))
			return i;

	return -1;
}
