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
 */

#include "config.h"
#if USE_MULTITHREAD
#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gthread.h>
#if USE_GTK
#include <gtk/gtk.h>
#include <gtkgame.h>
#endif

#include "multithread.h"
#include "speed.h"
#include "rollout.h"
#include "util.h"

#define UI_UPDATETIME 250

#ifdef TRY_COUNTING_PROCEESING_UNITS
extern int GetLogicalProcssingUnitCount(void);
#endif

#ifdef GLIB_THREADS
typedef struct _ManualEvent
{
    GCond* cond;
    int signalled;
} *ManualEvent;
typedef GPrivate* TLSItem;
typedef GCond* Event;
typedef GMutex* Mutex;
GMutex* condMutex=NULL;    /* Extra mutex needed for waiting */
GAsyncQueue *async_queue=NULL; /* Needed for async waiting */
#else
typedef HANDLE ManualEvent;
typedef DWORD TLSItem;
typedef HANDLE Event;
typedef HANDLE Mutex;
#endif

typedef struct _ThreadData
{
    ManualEvent activity;
    TLSItem tlsItem;
    Mutex queueLock;
    Mutex multiLock;
	ManualEvent syncStart;
	ManualEvent syncEnd;

    int addedTasks;
    int doneTasks;
    int totalTasks;

    GList *tasks;
    int result;

	int closingThreads;
	unsigned int numThreads;
} ThreadData;

ThreadData td;

#ifdef GLIB_THREADS

static void TLSCreate(TLSItem *pItem)
{
    *pItem = g_private_new(free);
}

static void TLSFree(TLSItem pItem)
{	/* Done automaticaly by glib */
}

static void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    g_private_set(pItem, (gpointer)pNew);
}

#define TLSGet(item) *((int*)g_private_get(item))

static void InitManualEvent(ManualEvent *pME)
{
    ManualEvent pNewME = malloc(sizeof(*pNewME));
    pNewME->cond = g_cond_new();
    pNewME->signalled = FALSE;
    *pME = pNewME;
}

static void FreeManualEvent(ManualEvent ME)
{
    g_cond_free(ME->cond);
    free(ME);
}

static void WaitForManualEvent(ManualEvent ME)
{
	GTimeVal tv;
	multi_debug("wait for manual event locks");
	g_mutex_lock(condMutex);
	while (!ME->signalled) {
		multi_debug("waiting for manual event");
		g_get_current_time(&tv);
		g_time_val_add(&tv, 10 * 1000 * 1000);
		if (g_cond_timed_wait(ME->cond, condMutex, &tv))
			break;
		else
		{
			multi_debug("still waiting for manual event");
		}
	}

	g_mutex_unlock(condMutex);
	multi_debug("wait for manual event unlocks");
}

static void ResetManualEvent(ManualEvent ME)
{
	multi_debug("reset manual event locks");
    g_mutex_lock(condMutex);
    ME->signalled = FALSE;
    g_mutex_unlock(condMutex);
	multi_debug("reset manual event unlocks");
}

static void SetManualEvent(ManualEvent ME)
{
	multi_debug("reset manual event locks");
    g_mutex_lock(condMutex);
    ME->signalled = TRUE;
    g_cond_broadcast(ME->cond);
    g_mutex_unlock(condMutex);
	multi_debug("reset manual event unlocks");
}

static void InitMutex(Mutex *pMutex)
{
    *pMutex = g_mutex_new();
}

static void FreeMutex(Mutex mutex)
{
    g_mutex_free(mutex);
}

#define Mutex_Lock(mutex) g_mutex_lock(mutex)
#define Mutex_Release(mutex) g_mutex_unlock(mutex)

#else    /* win32 */

static void TLSCreate(TLSItem *ppItem)
{
    *ppItem = TlsAlloc();
	if (*ppItem == TLS_OUT_OF_INDEXES)
		PrintSystemError("calling TlsAlloc");
}

static void TLSFree(TLSItem pItem)
{
	free(TlsGetValue(pItem));
	TlsFree(pItem);
}

static void TLSSetValue(TLSItem pItem, int value)
{
	int *pNew = (int*)malloc(sizeof(int));
	*pNew = value;
    if (TlsSetValue(pItem, pNew) == 0)
		PrintSystemError("calling TLSSetValue");
}

#define TLSGet(item) *((int*)TlsGetValue(item))

static void InitManualEvent(ManualEvent *pME)
{
    *pME = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (*pME == NULL)
		PrintSystemError("creating manual event");
}

static void FreeManualEvent(ManualEvent ME)
{
    CloseHandle(ME);
}

#define WaitForManualEvent(ME) WaitForSingleObject(ME, INFINITE)
#define ResetManualEvent(ME) ResetEvent(ME)
#define SetManualEvent(ME) SetEvent(ME)

static void InitMutex(Mutex *pMutex)
{
    *pMutex = CreateMutex(NULL, FALSE, NULL);
	if (*pMutex == NULL)
		PrintSystemError("creating mutex");
}

static void FreeMutex(Mutex mutex)
{
    CloseHandle(mutex);
}

#define Mutex_Lock(mutex) WaitForSingleObject(mutex, INFINITE)
#define Mutex_Release(mutex) ReleaseMutex(mutex)

#endif

extern unsigned int MT_GetNumThreads(void)
{
    return td.numThreads;
}

static void CloseThread(void *unused)
{
	g_assert(td.closingThreads);
	MT_SafeInc(&td.result);
}

static void MT_CloseThreads(void)
{
	td.closingThreads = TRUE;
    mt_add_tasks(td.numThreads, CloseThread, NULL, NULL);
    if (MT_WaitForTasks(NULL, 0) != (int)td.numThreads)
		g_print("Error closing threads!\n");
}

static void MT_TaskDone(Task *pt)
{
    MT_SafeInc(&td.doneTasks);

    if (pt)
    {
        free(pt->pLinkedTask);
        free(pt);
    }
}

static Task *MT_GetTask(void)
{
    Task *task = NULL;

    multi_debug("get task: lock");
    Mutex_Lock(td.queueLock);

    if (g_list_length(td.tasks) > 0)
    {
        task = (Task*)g_list_first(td.tasks)->data;
        td.tasks = g_list_delete_link(td.tasks, g_list_first(td.tasks));
        if (g_list_length(td.tasks) == 0)
        {
            ResetManualEvent(td.activity);
        }
    }

    Mutex_Release(td.queueLock);
    multi_debug("get task: release");

    return task;
}

extern void MT_AbortTasks(void)
{
    Task *task;
    /* Remove tasks from list */
    while ((task = MT_GetTask()) != NULL)
        MT_TaskDone(task);

	td.result = -1;
}

#ifdef GLIB_THREADS
static gpointer MT_WorkerThreadFunction(void *id)
#else
static void MT_WorkerThreadFunction(void *id)
#endif
{
#if __GNUC__ && USE_SSE_VECTORIZE
	asm  __volatile__  ("andl $-16, %%esp" : : : "%esp");
#endif
	{
		int *pID = (int*)id;
		TLSSetValue(td.tlsItem, *pID);
		free(pID);
		MT_SafeInc(&td.result);
		MT_TaskDone(NULL);    /* Thread created */
		do
		{
			Task *task;
			WaitForManualEvent(td.activity);
			task = MT_GetTask();
			if (task)
			{
				task->fun(task->data);
				MT_TaskDone(task);
			}
		} while (!td.closingThreads);

#ifdef GLIB_THREADS
		g_usleep(0);	/* Avoid odd crash */
		return NULL;
#endif
	}
}

static void WaitingForThreads(void)
{	/* Unlikely to be called */
	multi_debug("Waiting for threads to be created!");
}

static void MT_CreateThreads(void)
{
    unsigned int i;
	td.result = 0;
	td.closingThreads = FALSE;
    for (i = 0; i < td.numThreads; i++)
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
	td.addedTasks = td.numThreads;
	/* Wait for all the threads to be created (timeout after 1 second) */
	if (MT_WaitForTasks(WaitingForThreads, 1000) != (int)td.numThreads)
		g_print("Error creating threads!\n");
}

void MT_SetNumThreads(unsigned int num)
{
	if (num != td.numThreads)
	{
		MT_CloseThreads();
		td.numThreads = num;
		MT_CreateThreads();
	}
}

extern void MT_InitThreads(void)
{
#ifdef GLIB_THREADS
	if (!g_thread_supported ())
		g_thread_init (NULL);
	g_assert(g_thread_supported());
#endif

    td.tasks = NULL;
	td.doneTasks = td.addedTasks = 0;
	td.totalTasks = -1;
    InitManualEvent(&td.activity);
    TLSCreate(&td.tlsItem);
	TLSSetValue(td.tlsItem, 0);	/* Main thread shares id 0 */
    InitMutex(&td.multiLock);
    InitMutex(&td.queueLock);
	InitManualEvent(&td.syncStart);
	InitManualEvent(&td.syncEnd);
#ifdef GLIB_THREADS
    if (condMutex == NULL)
        condMutex = g_mutex_new();
#endif
	td.numThreads = 0;
}

extern void MT_StartThreads(void)
{
    if (td.numThreads == 0)
	{
#ifdef TRY_COUNTING_PROCEESING_UNITS
        td.numThreads = GetLogicalProcssingUnitCount();
#else
        td.numThreads = 1;
#endif
		MT_CreateThreads();
	}
}

void MT_AddTask(Task *pt, gboolean lock)
{
	if (lock)
	{
		multi_debug("add task: lock");
		Mutex_Lock(td.queueLock);
	}
	if (td.addedTasks == 0)
		td.result = 0;	/* Reset result for new tasks */
	td.addedTasks++;
	td.tasks = g_list_append(td.tasks, pt);
	if (g_list_length(td.tasks) == 1)
	{    /* New tasks */
		SetManualEvent(td.activity);
	}
	if (lock)
	{
		Mutex_Release(td.queueLock);
		multi_debug("add task: release");
	}
}

void mt_add_tasks(int num_tasks, AsyncFun pFun, void *taskData, gpointer linked)
{
	int i;
	multi_debug("add many tasks: lock");
	Mutex_Lock(td.queueLock);
	for (i = 0; i < num_tasks; i++)
	{
		Task *pt = (Task*)malloc(sizeof(Task));
		pt->fun = pFun;
		pt->data = taskData;
		pt->pLinkedTask = linked;
		MT_AddTask(pt, FALSE);
	}
	Mutex_Release(td.queueLock);
	multi_debug("add many release: lock");
}

static gboolean WaitForAllTasks(int time)
{
	int j=0;
	while (td.doneTasks != td.totalTasks)
	{
		if (j == 10)
			return FALSE;	/* Not done yet */

		j++;
		g_usleep(100*time);
	}
	return TRUE;
}

int MT_WaitForTasks(void (*pCallback)(void), int callbackTime)
{
    int callbackLoops = callbackTime / UI_UPDATETIME;
    int waits = 0;
	int polltime = callbackLoops ? UI_UPDATETIME : callbackTime;

    multi_debug("wait for tasks: lock(1)");
    Mutex_Lock(td.queueLock);
    td.totalTasks = td.addedTasks;
    Mutex_Release(td.queueLock);
    multi_debug("wait for tasks: release(1)");
#if USE_GTK
	GTKSuspendInput();
#endif

	multi_debug("while waiting for all tasks");
    while (!WaitForAllTasks(polltime))
    {
        waits++;
        if (pCallback && waits >= callbackLoops)
        {
            waits = 0;
            pCallback();
        }

#if USE_GTK
		else
			ProcessGtkEvents();
#endif
	}
	multi_debug("done while waiting for all tasks");

	td.doneTasks = td.addedTasks = 0;
	td.totalTasks = -1;

#if USE_GTK
	GTKResumeInput();
#endif
    return td.result;
}

extern void MT_SetResultFailed(void)
{
	td.result = -1;
}

extern void MT_Close(void)
{
    MT_CloseThreads();

    FreeManualEvent(td.activity);
    FreeMutex(td.multiLock);
    FreeMutex(td.queueLock);

	FreeManualEvent(td.syncStart);
	FreeManualEvent(td.syncEnd);
    TLSFree(td.tlsItem);
}

extern int MT_GetThreadID(void)
{
    return TLSGet(td.tlsItem);
}

extern void MT_Exclusive(void)
{
	Mutex_Lock(td.multiLock);
}

extern void MT_Release(void)
{
	Mutex_Release(td.multiLock);
}

extern int MT_GetDoneTasks(void)
{
    return td.doneTasks;
}

/* Code below used in calibrate to try and get a resonable figure for multiple threads */

static double start; /* used for timekeeping */

extern void MT_SyncInit(void)
{
	ResetManualEvent(td.syncStart);
	ResetManualEvent(td.syncEnd);
}

extern void MT_SyncStart(void)
{
	static int count = 0;

	/* Wait for all threads to get here */
	if (MT_SafeIncValue(&count) == (int)td.numThreads)
	{
		count--;
		start = get_time();
		SetManualEvent(td.syncStart);
	}
	else
	{
		WaitForManualEvent(td.syncStart);
		if (MT_SafeDecCheck(&count))
			ResetManualEvent(td.syncStart);
	}
}

extern double MT_SyncEnd(void)
{
	static int count = 0;
	double now;

	/* Wait for all threads to get here */
	if (MT_SafeIncValue(&count) == (int)td.numThreads)
	{
		now = get_time();
		count--;
		SetManualEvent(td.syncEnd);
		return now - start;
	}
	else
	{
		WaitForManualEvent(td.syncEnd);
		if (MT_SafeDecCheck(&count))
			ResetManualEvent(td.syncEnd);
		return 0;
	}
}

#else
int asyncRet;
#endif
