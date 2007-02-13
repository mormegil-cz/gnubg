#include "config.h"

#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "multithread.h"
#include <glib.h>
#include <glib/gthread.h>

#if USE_MULTITHREAD

#define TRY_COUTING_PROCEESING_UNITS 0
#if __GNUC__
#define GCC_ALIGN_HACK 1
#endif

extern int GetLogicalProcssingUnitCount(void);
extern void RolloutLoopMT();

#ifdef GLIB_THREADS
typedef struct _ManualEvent
{
    GCond* cond;
    int signalled;
} *ManualEvent;
typedef GPrivate* TLSItem;
typedef GCond* Event;
typedef GMutex* Mutex;
GMutex* condMutex;    /* Extra mutex needed for waiting */
#else
typedef HANDLE ManualEvent;
typedef DWORD TLSItem;
typedef HANDLE Event;
typedef HANDLE Mutex;
#endif

typedef struct _ThreadData
{
    ManualEvent activity;
    ManualEvent lockContention;
    ManualEvent contentionCleared;
    TLSItem tlsItem;
    Event alldone;
    Mutex queueLock;
    Mutex multiLock;

    int addedTasks;
    int doneTasks;
    int totalTasks;

    GList *tasks;
    int result;
} ThreadData;

ThreadData td;

#ifdef GLIB_THREADS

void TLSCreate(TLSItem *pItem)
{
    *pItem = g_private_new(free);
}

void TLSFree(TLSItem pItem)
{	/* Done automaticaly by glib */
}

void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    g_private_set(pItem, (gpointer)pNew);
}

#define TLSGet(item) *((int*)g_private_get(item))

void InitEvent(Event *pEvent)
{
    *pEvent = g_cond_new();
}

int WaitForEvent(Event event, int time)
{
    int result;
    GTimeVal tv = {0, 0};
    g_mutex_lock(condMutex);
    g_get_current_time(&tv);
    g_time_val_add(&tv, time * 1000);
    result = g_cond_timed_wait(event, condMutex, &tv);
    g_mutex_unlock(condMutex);
    return result;
}

void SignalEvent(Event event)
{
    g_cond_signal(event);
}

void FreeEvent(Event event)
{
    g_cond_free(event);
}

void InitManualEvent(ManualEvent *pME)
{
    ManualEvent pNewME = malloc(sizeof(*pNewME));
    pNewME->cond = g_cond_new();
    pNewME->signalled = FALSE;
    *pME = pNewME;
}

void FreeManualEvent(ManualEvent ME)
{
    g_cond_free(ME->cond);
    free(ME);
}

void WaitForManualEvent(ManualEvent ME)
{
    g_mutex_lock(condMutex);
    if (!ME->signalled)
        g_cond_wait(ME->cond, condMutex);

    g_mutex_unlock(condMutex);
}

void ResetManualEvent(ManualEvent ME)
{
    g_mutex_lock(condMutex);
    ME->signalled = FALSE;
    g_mutex_unlock(condMutex);
}

void SetManualEvent(ManualEvent ME)
{
    g_mutex_lock(condMutex);
    ME->signalled = TRUE;
    g_cond_broadcast(ME->cond);
    g_mutex_unlock(condMutex);
}

void InitMutex(Mutex *pMutex)
{
    *pMutex = g_mutex_new();
}

void FreeMutex(Mutex mutex)
{
    g_mutex_free(mutex);
}

#define Mutex_Lock(mutex) g_mutex_lock(mutex)
#define Mutex_Release(mutex) g_mutex_unlock(mutex)

#else    /* win32 */

void TLSCreate(TLSItem *ppItem)
{
    *ppItem = TlsAlloc();
}

void TLSFree(TLSItem pItem)
{
	free(TlsGetValue(pItem));
	TlsFree(pItem);
}

void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    TlsSetValue(pItem, pNew);
}

#define TLSGet(item) *((int*)TlsGetValue(item))

void InitEvent(Event *pEvent)
{
    *pEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

int WaitForEvent(Event event, int time)
{
    return (WaitForSingleObject(event, time) != WAIT_TIMEOUT);
}

void SignalEvent(Event event)
{
    SetEvent(event);
}

void FreeEvent(Event event)
{
    CloseHandle(event);
}

void InitManualEvent(ManualEvent *pME)
{
    *pME = CreateEvent(NULL, TRUE, FALSE, NULL);
}

void FreeManualEvent(ManualEvent ME)
{
    CloseHandle(ME);
}

#define WaitForManualEvent(ME) WaitForSingleObject(ME, INFINITE)
#define ResetManualEvent(ME) ResetEvent(ME)
#define SetManualEvent(ME) SetEvent(ME)

void InitMutex(Mutex *pMutex)
{
    *pMutex = CreateMutex(NULL, FALSE, NULL);
}

void FreeMutex(Mutex mutex)
{
    CloseHandle(mutex);
}

#define Mutex_Lock(mutex) WaitForSingleObject(mutex, INFINITE)
#define Mutex_Release(mutex) ReleaseMutex(mutex)

#endif

unsigned int numThreads = 0;

unsigned int MT_GetNumThreads()
{
    return numThreads;
}

int MT_Enabled(void)
{
    return TRUE;
}

static void MT_CreateThreads(void);
static void MT_CloseThreads(void);

void MT_SetNumThreads(unsigned int num)
{
    MT_CloseThreads();
    numThreads = num;
    MT_CreateThreads();
}

int MT_DoTask();
void MT_TaskDone(Task *pt);

#if GCC_ALIGN_HACK

void MT_ActualWorkerThreadFunction(void *id);

gpointer MT_WorkerThreadFunction(gpointer id)
{    /* Align stack and call actual function */
	asm  __volatile__  ("andl $-16, %%esp" : : : "%esp");
	MT_ActualWorkerThreadFunction(id);
	return NULL;
}

void MT_ActualWorkerThreadFunction(void *id)
{
#else
void MT_WorkerThreadFunction(void *id)
{
#endif
	int *pID = (int*)id;
    TLSSetValue(td.tlsItem, *pID);
    free(pID);
    MT_TaskDone(NULL);    /* Thread created */
    do
    {
        WaitForManualEvent(td.activity);
    } while (MT_DoTask());
}

static void MT_CreateThreads(void)
{
    unsigned int i;
    td.totalTasks = numThreads;    /* Use to monitor number created */
    for (i = 0; i < numThreads; i++)
    {
    	int *pID = (int*)malloc(sizeof(int));
    	*pID = i;
#ifdef GLIB_THREADS
        if (!g_thread_create(MT_WorkerThreadFunction, pID, FALSE, NULL))
#else
        if (_beginthread(MT_WorkerThreadFunction, 0, pID) == 0)
#endif
            printf("Failed to create thread\n");
    }
    if (td.doneTasks != td.totalTasks)
    {    /* Wait for all the threads to be created (timeout after 20 seconds) */
        if (!WaitForEvent(td.alldone, 20 * 1000))
            printf("Not sure all threads created!\n");
    }
    /* Reset counters */
    td.totalTasks = td.doneTasks = 0;
}

static void MT_CloseThreads(void)
{
    unsigned int i;
    for (i = 0; i < numThreads; i++)
    {
        Task *pt = (Task*)malloc(sizeof(Task));
        pt->type = TT_CLOSE;
        pt->pLinkedTask = NULL;
        MT_AddTask(pt);
    }
    MT_WaitForTasks(NULL, 0);
}

extern void MT_InitThreads()
{
#ifdef GLIB_THREADS
    g_thread_init(NULL);
    if (!g_thread_supported())
        printf("Glib threads not supported!\n");
#endif

    if (numThreads == 0)
#if TRY_COUTING_PROCEESING_UNITS
        numThreads = GetLogicalProcssingUnitCount();
#else
        numThreads = 1;
#endif
    td.tasks = NULL;
    td.totalTasks = td.addedTasks = td.doneTasks = 0;
    InitManualEvent(&td.activity);
    InitManualEvent(&td.lockContention);
    InitManualEvent(&td.contentionCleared);
    TLSCreate(&td.tlsItem);
	TLSSetValue(td.tlsItem, 0);	/* Main thread shares id 0 */
    InitEvent(&td.alldone);
    InitMutex(&td.multiLock);
    InitMutex(&td.queueLock);
#ifdef GLIB_THREADS
    if (condMutex == NULL)
        condMutex = g_mutex_new();
#endif

    MT_CreateThreads();
}

Task *MT_GetTask(void)
{
    Task *task = NULL;
    if (g_list_length(td.tasks) > 0)
    {
        task = (Task*)g_list_first(td.tasks)->data;
        td.tasks = g_list_delete_link(td.tasks, g_list_first(td.tasks));
        if (g_list_length(td.tasks) == 0)
        {
            ResetManualEvent(td.activity);
        }
    }
    return task;
}

void MT_AbortTasks(void)
{
    Task *task;
    Mutex_Lock(td.queueLock);

    /* Remove tasks from list */
    while ((task = MT_GetTask()) != NULL)
        MT_TaskDone(task);

    Mutex_Release(td.queueLock);
}

int MT_DoTask()
{
    int alive = TRUE;
    Task *task;
    Mutex_Lock(td.queueLock);
    task = MT_GetTask();
    Mutex_Release(td.queueLock);

    if (task)
    {
        switch (task->type)
        {
        case TT_ANALYSEMOVE:
        {
            float doubleError;
            AnalyseMoveTask *amt;
AnalyzeDoubleDecison:
            amt = (AnalyseMoveTask *)task;
            if (AnalyzeMove (amt->pmr, &amt->ms, amt->plGame, amt->psc,
                        &esAnalysisChequer,
                        &esAnalysisCube, aamfAnalysis,
                        afAnalysePlayers, &doubleError ) < 0 )
            {
                MT_AbortTasks();
                td.result = -1;
            }
            if (task->pLinkedTask)
            {    /* Need to analyze take/drop decision in sequence */
                task = task->pLinkedTask;
                goto AnalyzeDoubleDecison;
            }
            break;
        }
        case TT_ROLLOUTLOOP:
            RolloutLoopMT();
            break;

        case TT_TEST:
            printf("Test: waiting for a second");
            g_usleep(1000000);
            break;
        case TT_CLOSE:
            alive = FALSE;
            break;
        }
        Mutex_Lock(td.queueLock);
        MT_TaskDone(task);
        Mutex_Release(td.queueLock);
        return alive;
    }
    else
        return -1;
}

void MT_TaskDone(Task *pt)
{
    td.doneTasks++;
    if (td.doneTasks == td.totalTasks)
        SignalEvent(td.alldone);

    if (pt)
    {
        free(pt->pLinkedTask);
        free(pt);
    }
}

void MT_AddTask(Task *pt)
{
    Mutex_Lock(td.queueLock);

    td.addedTasks++;
    td.tasks = g_list_append(td.tasks, pt);
    if (g_list_length(td.tasks) == 1)
    {    /* First task */
        td.result = 0;
        SetManualEvent(td.activity);
    }
    Mutex_Release(td.queueLock);
}

int MT_WaitForTasks(void (*pCallback)(), int callbackTime)
{
    int done = FALSE;
#define UI_UPDATETIME 250
    int callbackLoops = callbackTime / UI_UPDATETIME;
    int waits = 0;

    Mutex_Lock(td.queueLock);
    td.totalTasks = td.addedTasks;
    if (td.doneTasks == td.totalTasks)
        done = TRUE;
    Mutex_Release(td.queueLock);
    if (!done)
    {
        while (WaitForEvent(td.alldone, UI_UPDATETIME) == FALSE)
        {
            waits++;
            if (pCallback && waits == callbackLoops)
            {
                waits = 0;
                pCallback();
            }

            SuspendInput();

            while( gtk_events_pending() )
            gtk_main_iteration();

            ResumeInput();
        }
    }
    /* Reset counters */
    td.totalTasks = td.addedTasks = td.doneTasks = 0;

    return td.result;
}

extern void MT_Close()
{
    MT_CloseThreads();

    FreeManualEvent(td.activity);
    FreeManualEvent(td.lockContention);
    FreeManualEvent(td.contentionCleared);
    FreeEvent(td.alldone);
    FreeMutex(td.multiLock);
    FreeMutex(td.queueLock);
    TLSFree(td.tlsItem);
}

int MT_GetThreadID()
{
    return TLSGet(td.tlsItem);
}

void MT_Lock(int *lock)
{
    while (MT_SafeInc(lock) != 1)
    {
        WaitForManualEvent(td.lockContention);
        if (MT_SafeDec(lock))
        {    /* Found something that's cleared */
            SetManualEvent(td.contentionCleared);
            ResetManualEvent(td.lockContention);
        }
        else
        {
            WaitForManualEvent(td.contentionCleared);
        }
    }
}

void MT_Unlock(long *lock)
{
    if (!MT_SafeDec(lock))
    {    /* Clear contention */
        ResetManualEvent(td.contentionCleared);	/* Force multiple threads to wait for contention reduction */
        SetManualEvent(td.lockContention);	/* Release any waiting threads */
    }
}

extern void MT_Exclusive()
{
    Mutex_Lock(td.multiLock);
}

extern void MT_Release()
{
    Mutex_Release(td.multiLock);
}

extern int MT_GetDoneTasks()
{
    return td.doneTasks;
}

#endif
