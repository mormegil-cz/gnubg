
#include "config.h"

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include "multithread.h"
#include <glib.h>

#define MULTITHREADED 1

#ifdef MULTITHREADED
#define TRY_COUTING_PROCEESING_UNITS 1
#ifndef WIN32
#define GLIB_THREADS
#endif
#endif

extern int GetLogicalProcssingUnitCount(void);

typedef struct _ThreadData
{
#if MULTITHREADED
#ifdef GLIB_THREADS
	GMutex* queueLock;
	GMutex* condMutex;
	GCond* activity;
	int active;
	GCond* alldone;
#else
	HANDLE queueLock;
	HANDLE activity;
	HANDLE alldone;
#endif
	int addedTasks;
	int doneTasks;
	int totalTasks;
#endif
	GList *tasks;
	int result;
} ThreadData;

ThreadData td;

unsigned int numThreads = 0;

unsigned int MT_GetNumThreads()
{
	return numThreads;
}

static void CreateThreads(void);
static void CloseThreads(void);

void MT_SetNumThreads(unsigned int num)
{
	CloseThreads();
	numThreads = num;
	CreateThreads();
}

int MT_DoTask();
void MT_TaskDone(Task *pt);

#if MULTITHREADED
#ifdef GLIB_THREADS
gpointer MT_WorkerThreadFunction(gpointer notused)
#else
void MT_WorkerThreadFunction(void *notused)
#endif
{
	do
	{
#ifdef GLIB_THREADS
		if (!td.active)
		{
			g_mutex_lock(td.condMutex);
			g_cond_wait(td.activity, td.condMutex);
			g_mutex_unlock(td.condMutex);
		}
#else
		if (WaitForSingleObject(td.activity, INFINITE) != WAIT_OBJECT_0)
		{
			return;
		}
#endif
	} while (MT_DoTask());
#ifdef GLIB_THREADS
	return NULL;
#endif
}
#endif

static void CreateThreads(void)
{
#if MULTITHREADED
	unsigned int i;
	for (i = 0; i < numThreads; i++)
	{
#ifdef GLIB_THREADS
		if (!g_thread_create(MT_WorkerThreadFunction, NULL, FALSE, NULL))
#else
		if (_beginthread(MT_WorkerThreadFunction, 0, NULL) == 0)
#endif
			printf("Failed to create thread\n");
	}
#endif
}

static void CloseThreads(void)
{
#if MULTITHREADED
	unsigned int i;
	for (i = 0; i < numThreads; i++)
	{
		Task *pt = (Task*)malloc(sizeof(Task));
		pt->type = TT_CLOSE;
		MT_AddTask(pt);
	}
	MT_WaitForTasks();
#endif
}

void MT_InitThreads()
{
#if MULTITHREADED

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
#ifdef GLIB_THREADS
	td.queueLock = g_mutex_new();
	td.condMutex = g_mutex_new();
	td.activity = g_cond_new();
	td.alldone = g_cond_new();
	td.active = FALSE;
#else
	td.activity = CreateEvent(NULL, TRUE, FALSE, NULL);
	td.alldone = CreateEvent(NULL, FALSE, FALSE, NULL);
	td.queueLock = CreateMutex(NULL, FALSE, NULL);
 #endif

	CreateThreads();
#endif
}

#if MULTITHREADED

#ifdef GLIB_THREADS
#define MT_GetLock(mutex) g_mutex_lock(mutex)
#define MT_ReleaseLock(mutex) g_mutex_unlock(mutex)
#else
#define MT_GetLock(mutex) WaitForSingleObject(mutex, INFINITE)
#define MT_ReleaseLock(mutex) ReleaseMutex(mutex)
#endif

#endif

Task *MT_GetTask(void)
{
	Task *task = NULL;
	if (g_list_length(td.tasks) > 0)
	{
		task = (Task*)g_list_first(td.tasks)->data;
		td.tasks = g_list_delete_link(td.tasks, g_list_first(td.tasks));
#if MULTITHREADED
		if (g_list_length(td.tasks) == 0)
		{
#ifdef GLIB_THREADS
			g_mutex_lock(td.condMutex);
			td.active = FALSE;
			g_mutex_unlock(td.condMutex);
#else
			ResetEvent(td.activity);
#endif
		}
#endif
	}

	return task;
}

void AbortTasks(void)
{
	Task *task;
#if MULTITHREADED
	MT_GetLock(td.queueLock);
#endif

	/* Remove tasks from list */
	while ((task = MT_GetTask()) != NULL)
		MT_TaskDone(task);

#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif
}

int MT_DoTask()
{
	int alive = TRUE;
	Task *task;
#if MULTITHREADED
	MT_GetLock(td.queueLock);
#endif
	task = MT_GetTask();
#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif

	if (task)
	{
		switch (task->type)
		{
		case TT_ANALYSEMOVE:
		{
			AnalyseMoveTask *amt = (AnalyseMoveTask *)task;

			if (AnalyzeMove (amt->pmr, &amt->ms, amt->plGame, amt->psc,
                          &esAnalysisChequer,
                          &esAnalysisCube, aamfAnalysis,
                          afAnalysePlayers ) < 0 ) {
				AbortTasks();
				td.result = -1;
			}
			break;
	    }

		case TT_TEST:
			printf("Test: waiting for a second");
			Sleep(1000);
			break;
		case TT_CLOSE:
			alive = FALSE;
			break;
		}
#if MULTITHREADED
		MT_GetLock(td.queueLock);
		MT_TaskDone(task);
		MT_ReleaseLock(td.queueLock);
#endif
		return alive;
	}
	else
		return -1;
}

void MT_TaskDone(Task *pt)
{
#if MULTITHREADED
	td.doneTasks++;
	if (td.doneTasks == td.totalTasks)
{
#ifdef GLIB_THREADS
		g_cond_signal(td.alldone);
#else
		SetEvent(td.alldone);
#endif
}
#endif
	free(pt);
}

void MT_AddTask(Task *pt)
{
#if MULTITHREADED
	MT_GetLock(td.queueLock);

	td.addedTasks++;
#endif
	td.tasks = g_list_append(td.tasks, pt);
	if (g_list_length(td.tasks) == 1)
	{	/* First task */
		td.result = 0;
#if MULTITHREADED
#ifdef GLIB_THREADS
		g_mutex_lock(td.condMutex);
		td.active = TRUE;
		g_cond_broadcast(td.activity);
		g_mutex_unlock(td.condMutex);
#else
		SetEvent(td.activity);
#endif
#endif
	}
#if MULTITHREADED
	MT_ReleaseLock(td.queueLock);
#endif
}

#if MULTITHREADED
int MT_WaitForTasks()
{
	int doneTasks, count = 0, done = FALSE;

	MT_GetLock(td.queueLock);
	td.totalTasks = td.addedTasks;
	if (td.doneTasks == td.totalTasks)
		done = TRUE;
	MT_ReleaseLock(td.queueLock);
	if (!done)
	{
#ifdef GLIB_THREADS
		g_mutex_lock(td.condMutex);
		for (;;)
		{
			GTimeVal tv = {0, 0};
			g_get_current_time (&tv);
			g_time_val_add (&tv, 250 * 1000);
			if (g_cond_timed_wait(td.alldone, td.condMutex, &tv) == TRUE)
				break;
#else
		while (WaitForSingleObject(td.alldone, 250) == WAIT_TIMEOUT)
		{
#endif
				doneTasks = td.doneTasks - count;
				if (doneTasks > 0)
				{
					ProgressValueAdd( doneTasks );
					count += doneTasks;
				}
				else
				{
					SuspendInput();

					while( gtk_events_pending() )
					gtk_main_iteration();

					ResumeInput();
				}
		}
#ifdef GLIB_THREADS
		g_mutex_unlock(td.condMutex);
#endif
	}
	/* Reset counters */
	td.totalTasks = td.addedTasks = td.doneTasks = 0;

	return td.result;
}
#else
int MT_WaitForTasks()
{
	while(MT_DoTask() != -1)
	{
		ProgressValueAdd( 1 );
	}
	return td.result;
}
#endif

void MT_Close()
{
#if MULTITHREADED
	CloseThreads();

#ifdef GLIB_THREADS
	g_mutex_free(td.queueLock);
	g_mutex_free(td.condMutex);
	g_cond_free(td.activity);
	g_cond_free(td.alldone);
#else
	CloseHandle(td.queueLock);
	CloseHandle(td.activity);
	CloseHandle(td.alldone);
#endif

#endif
}
