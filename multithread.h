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

#if USE_MULTITHREAD
#include "backgammon.h"
#include <glib.h>

#ifdef WIN32
#include <windows.h>
#endif

#define MAX_NUMTHREADS 16

typedef enum _TaskType {TT_ANALYSEMOVE, TT_ROLLOUTLOOP, TT_TEST, TT_RUNCALIBRATIONEVALS, TT_CLOSE} TaskType;

typedef struct _Task
{
	TaskType type;
	struct _Task *pLinkedTask;
} Task;

typedef struct _AnalyseMoveTask
{
	Task task;
	moverecord *pmr;
	listOLD *plGame;
	statcontext *psc;
	matchstate ms;
} AnalyseMoveTask;

extern unsigned int MT_GetNumThreads(void);
extern void MT_InitThreads(void);
extern void MT_StartThreads(void);
extern void MT_Close(void);
extern void MT_AddTask(Task *pt, gboolean lock);
extern int MT_WaitForTasks(void (*pCallback)(), int callbackTime);
extern void MT_SetNumThreads(unsigned int num);
extern int MT_Enabled(void);
extern int MT_GetThreadID(void);
extern void mt_add_tasks(int num_tasks, TaskType tt, gpointer linked);
extern void MT_Release(void);
extern int MT_GetDoneTasks(void);
extern void MT_SyncInit(void);
extern void MT_SyncStart(void);
extern double MT_SyncEnd(void);
extern void MT_Exclusive(void);
#ifdef GLIB_THREADS
  #define MT_SafeInc(x) g_atomic_int_exchange_and_add(x, 1)
  #define MT_SafeIncValue(x) (g_atomic_int_exchange_and_add(x, 1) + 1)
  #define MT_SafeIncCheck(x) (g_atomic_int_exchange_and_add(x, 1))
  #define MT_SafeAdd(x, y) g_atomic_int_exchange_and_add(x, y)
  #define MT_SafeDec(x) g_atomic_int_add(x, 1)
#else
  #define MT_SafeInc(x) InterlockedIncrement((long*)x)
  #define MT_SafeIncValue(x) InterlockedIncrement((long*)x)
  #define MT_SafeIncCheck(x) (InterlockedIncrement((long*)x) > 1)
  #define MT_SafeAdd(x, y) InterlockedExchangeAdd((long*)x, y)
  #define MT_SafeDec(x) InterlockedDecrement((long*)x)
  #define MT_SafeDecCheck(x) (InterlockedDecrement((long*)x) == 0)
#endif

#else /*USE_MULTITHREAD*/
#define MT_SafeInc(x) (++(*x))
#define MT_SafeIncValue(x) (++(*x))
#define MT_SafeIncCheck(x) ((*x)++)
#define MT_SafeAdd(x, y) ((*x) += y)
#define MT_SafeDec(x) (--(*x))
#define MT_SafeDecCheck(x) ((--(*x)) == 0)
#define MT_GetThreadID(x) 0
#endif
