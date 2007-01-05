
#include "backgammon.h"

typedef enum _TaskType {TT_ANALYSEMOVE, TT_TEST, TT_CLOSE} TaskType;

typedef struct _Task
{
	TaskType type;
} Task;

typedef struct _AnalyseMoveTask
{
	TaskType type;

moverecord *pmr;
list *plGame;
statcontext *psc;
matchstate ms;

} AnalyseMoveTask;

void MT_InitThreads();
void MT_Close();
void MT_AddTask(Task *pt);
int MT_WaitForTasks();
unsigned int MT_GetNumThreads();
void MT_SetNumThreads(unsigned int num);
