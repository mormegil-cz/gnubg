
typedef struct _myList
{
	int eleSize;
	int numElements;
	void* data;
	int curAllocated;
} myList;

extern void ListInit(myList* l, int eleSize);
extern void ListAdd(myList* l, void* ele);
extern int ListSize(myList* l);
extern void* ListGet(myList* l, int pos);
extern int ListFind(myList* l, void* ele);
extern void ListClear(myList* l);
