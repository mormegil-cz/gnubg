
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
