
#include "backgammon.h"

#define MAX_NUMTHREADS 16

typedef enum _TaskType {TT_ANALYSEMOVE, TT_ROLLOUTLOOP, TT_TEST, TT_CLOSE} TaskType;

typedef struct _Task
{
	TaskType type;
	struct _Task *pLinkedTask;
} Task;

typedef struct _AnalyseMoveTask
{
	Task task;
	moverecord *pmr;
	list *plGame;
	statcontext *psc;
	matchstate ms;
} AnalyseMoveTask;

extern void MT_InitThreads();
extern void MT_Close();
extern void MT_AddTask(Task *pt);
extern int MT_WaitForTasks(void (*pCallback)(), int callbackTime);
extern unsigned int MT_GetNumThreads();
extern void MT_SetNumThreads(unsigned int num);
extern int MT_Enabled(void);
extern int MT_GetThreadID();

#if USE_MULTITHREAD
 #ifdef WIN32
  #define MT_SafeInc(x) InterlockedIncrement(&x)
  #define MT_SafeAdd(x, y) InterlockedExchangeAdd(&x, y)
 #else
  #define MT_SafeInc(x) (g_atomic_int_exchange_and_add(&x, 1) + 1)
  #define MT_SafeAdd(x, y) (g_atomic_int_exchange_and_add(&x, y) + y)
 #endif
#else
 #define MT_SafeInc(x) (++x)
 #define MT_SafeAdd(x, y) (x += y)
#endif

extern void MT_Exclusive();
extern void MT_Release();

extern int MT_GetDoneTasks();
