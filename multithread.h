
#include "backgammon.h"

#define MAX_NUMTHREADS 16

typedef enum _TaskType {TT_ANALYSEMOVE, TT_TEST, TT_CLOSE} TaskType;

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
extern int MT_WaitForTasks();
extern unsigned int MT_GetNumThreads();
extern void MT_SetNumThreads(unsigned int num);
extern int MT_Enabled(void);
extern int MT_GetThreadID();
