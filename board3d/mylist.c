/*
* mylist.c
* by Jon Kinsey, 2003
*
* Simple list
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

#include <malloc.h>
#include <memory.h>
#include "mylist.h"

#define ALLOC_STEP 50

void ListInit(myList* l, int eleSize)
{
	l->eleSize = eleSize;
	l->numElements = 0;
	l->curAllocated = 0;
	l->data = 0;
}

void ListClear(myList* l)
{
	free(l->data);
	l->eleSize = 0;
	l->numElements = 0;
	l->curAllocated = 0;
	l->data = 0;
}

void ListAdd(myList* l, void* ele)
{
	if (l->numElements == l->curAllocated)
	{
		l->curAllocated += ALLOC_STEP;
		l->data = realloc(l->data, l->curAllocated * l->eleSize);
	}
	memcpy(ListGet(l, l->numElements), ele, l->eleSize);
	l->numElements++;
}

int ListSize(myList* l)
{
	return l->numElements;
}

void* ListGet(myList* l, int pos)
{
	return &((char*)l->data)[pos * l->eleSize];
}

int ListFind(myList* l, void* ele)
{
	int i;
	for (i = 0; i < ListSize(l); i++)
		if (!memcmp(ele, ListGet(l, i), l->eleSize))
			return i;

	return -1;
}
