
typedef struct _list
{
	int eleSize;
	int numElements;
	void* data;
	int curAllocated;
} list;

extern void ListInit(list* l, int eleSize);
extern void ListAdd(list* l, void* ele);
extern int ListSize(list* l);
extern void* ListGet(list* l, int pos);
extern int ListFind(list* l, void* ele);
extern void ListClear(list* l);
