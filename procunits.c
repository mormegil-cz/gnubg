#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <signal.h>
#include <stddef.h>

#include "backgammon.h"
#include "procunits.h"
#include "threadglobals.h"

/* set to activate debugging */
#define RPU_DEBUG 0
#define RPU_DEBUG_PACK 0
#define PU_DEBUG_FREE 0

/* default TCP port and IP address mask for remote processing */
#define RPU_DEFAULT_TCPPORT 80
#define RPU_DEFAULT_IPMASK 0xFFFFFF00

/* timeout (in seconds) used in WaitForCondition() */
#define WAIT_TIME_SECONDS 1

/* timeout (in WAIT_TIME_SECONDS chuncks) used in remote processing 
    units communication (used for "quick" messages only, not for 
    polling rollout results!) */
#define RPU_TIMEOUT 5


static int 		gfProcessingUnitsInitialised = FALSE;

/* TCP port and IP address mask for remote processing */
static int 		gRPU_TCPPort = RPU_DEFAULT_TCPPORT;
static int 		gRPU_IPMask = RPU_DEFAULT_IPMASK;

/* unique task id */
static int 		taskId = 1;

/* unique procunit id */
static int 		procunitId = 1;

/* default max number of tasks that can be dispatched to a processing unit before
   it is full and refuses more tasks */
#define MAX_MAXTASKS 20

/* task list */
#define DEFAULT_TASKLISTMAX 20
/* FIXME: make the task list size dynamic; if the total queue size of all
    installed procunits is larger than the tasklist size, then the
    extra queue entries can't be assigned to todo tasks and the
    correpsonding procunits can't be put to work */ 
static int		taskListMax = DEFAULT_TASKLISTMAX;
static pu_task		**taskList = NULL;

/* global flag, set to TRUE by MarkTaskDone() when some tasks
    have been set to "done" state, along with mutex/condition */
int			gResultsAvailable = FALSE;
pthread_mutex_t 	mutexResultsAvailable;
pthread_cond_t		condResultsAvailable;


/* list of available processing units */
static procunit *gpulist = NULL;

/* this mutex to guarantee exclusive access to the task list */
pthread_mutex_t		mutexTaskListAccess;

/* this mutex for debugging purposes only */
pthread_mutex_t		mutexCPUAccess;


/* prototypes of threaded rollout functions */
extern void *Threaded_BearoffRollout (void *data);
extern void *Threaded_BasicCubefulRollout (void *data);

void * Thread_RemoteProcessingUnit (void *data);


static int StartProcessingUnit (procunit *ppu, int fWait);
static int StopProcessingUnit (procunit *ppu);
static void PrintProcessingUnitInfo (procunit *ppu);
static int StartRemoteProcessingUnit (procunit *ppu);
static int WaitForCondition (pthread_cond_t *cond, pthread_mutex_t *mutex, char *s, int *step);
static void RPU_DumpData (unsigned char *data, int len);

static procunit *CreateProcessingUnit (pu_type type, pu_status status, pu_task_type taskMask, int maxTasks);
static procunit *FindProcessingUnit (procunit *ppu, pu_type type, pu_status status, pu_task_type taskType, int procunit_id);



static const char asProcunitType[][10] = { "None", "Local", "Remote" };
static const char asProcunitStatus[][14] = { "None", "N/A", "Ready", "Busy", 
                        "Stopped", "Connecting" };
static const char asTaskStatus[][12] = { "None", "To do", "In Progress", "Done" };
static const char asSlaveStatus[][20] = { "N/A", "Waiting for cx...", "Idle", 
                        "Processing..." };

/* mode of the local host: master or slave */
static pu_mode gProcunitsMode = pu_mode_master;

/* state of the local host when running in slave mode */
typedef enum { rpu_stat_na, rpu_stat_waiting, rpu_stat_idle, rpu_stat_processing } rpu_slavestatus;
static rpu_slavestatus gSlaveStatus = rpu_stat_na;

/* stats for the local host when running in slave mode */
rpu_slavestats gSlaveStats;


static int GetProcessorCount (void)
{
    int cProcessors = 1;
    int i;
    
    #if __APPLE__
        #include <CoreServices/CoreServices.h>
        if (!MPLibraryIsLoaded ())
            fprintf (stderr, "*** Could not load Mac OS X Multiprocessing Library.\n");
        else
            cProcessors = MPProcessors ();
            
    #elif WIN32
        /* add here Win32 specific code */
        
        
        ;

    #else

        /* try _NPROCESSORS_ONLN from sysconf */

        if ( ( i = sysconf( _SC_NPROCESSORS_ONLN ) ) > 0 )
          i = cProcessors;

        /* other ideas? */



    #endif

    
    if (PU_DEBUG) fprintf (stderr, "%d processor(s) found.\n", cProcessors);
    
    return cProcessors;
}


static void ClearStats (pu_stats *s)
{
    s->avgSpeed = 0.0f;
    s->total = 0;
    s->totalDone = 0;
    s->totalFailed = 0;
}


/* Creates all processing units available on the host.
*/
extern void InitProcessingUnits (void)
{
    int i;
    int nProcs = GetProcessorCount ();
    struct sigaction act;
    
    if (gfProcessingUnitsInitialised) return;
    gfProcessingUnitsInitialised = TRUE;
    
    /* ignore process-wide SIGPIPE signals that can be 
        raised by a send() call to a remote host that has 
        closed its connection, in function RPU_SendMessage() */
    bzero (&act, sizeof (act));
    act.sa_handler = SIG_IGN;
    if (sigaction (SIGPIPE, &act, NULL) < 0) {
        perror ("sigaction");
        assert (FALSE);
    }
        
    /* create inter-thread mutexes and conditions neeeded for
        multithreaded operation */
    pthread_mutex_init (&mutexTaskListAccess, NULL);
    pthread_mutex_init (&mutexResultsAvailable, NULL);
    pthread_cond_init (&condResultsAvailable, NULL);
    //pthread_mutex_init (&mutexTasksAvailableForRPU, NULL);
    //pthread_cond_init (&condTasksAvailableForRPU, NULL);

    pthread_mutex_init (&mutexCPUAccess, NULL);

    /* create local processing unit: each will execute one rollout thread;
       N should be set to the number of processors available on the host */

    for (i = 0; i < nProcs ; i++) {
        CreateProcessingUnit (pu_type_local, pu_stat_ready, pu_task_info|pu_task_rollout, 1);
    }
    
    /* create remote processing units: according to the information returned
        by the gnubg remote processing units manager */
}


extern pu_mode GetProcessingUnitsMode (void)
{
    return gProcunitsMode;
}


static int GetProcessingUnitsCount (void)
{
    procunit 	*ppu = gpulist;
    int		n = 0;
    
    while (ppu != NULL) {
        n ++;
        ppu = ppu->next;
    }
    
    return n;
}


/* Creates and adds a new processing unit in the processing units list.
   Fills it in with passed arguments and default parameters.
   Returns a pointer to created procunit.
   
   Note: in the case of RPU's, doesn't START the RPU (no thread created)
*/
static procunit * CreateProcessingUnit (pu_type type, pu_status status, pu_task_type taskMask, int maxTasks)
{
    procunit **pppu = &gpulist;
    
    if (maxTasks < 0) maxTasks = 0;
    if (maxTasks > MAX_MAXTASKS) maxTasks = MAX_MAXTASKS;

    while (*pppu != NULL)
        pppu = &((*pppu)->next);
        
    *pppu = (procunit *) calloc (1, sizeof (procunit));
        
    if (*pppu != NULL) {
        procunit *ppu = *pppu;

        ppu->next = NULL;
        ppu->procunit_id = procunitId ++;
        ppu->type = type;
        ppu->status = status;
        ppu->taskMask = taskMask;
        ppu->maxTasks = maxTasks;

        switch (ppu->type) {
            case pu_type_local:
              break;
            case pu_type_remote:
              break;
            default:
              assert( FALSE );
              break;
        }

        ClearStats (&ppu->rolloutStats);
        ClearStats (&ppu->evalStats);

        pthread_mutex_init (&ppu->mutexStatusChanged, NULL);
        pthread_cond_init (&ppu->condStatusChanged, NULL);
    }

    return *pppu;
}


/* Create and START a remote processing unit */

static procunit * CreateRemoteProcessingUnit (struct in_addr *inAddress, int fWait)
{

    procunit 	*ppu = CreateProcessingUnit (pu_type_remote, 
                            pu_stat_busy, pu_task_info|pu_task_rollout, 0);
        /* create rpu in busy state so it can't be used for now (connecting) */
    
    if (ppu != NULL) {
        ppu->info.remote.inAddress = *inAddress;
        ppu->info.remote.fTasksAvailable = FALSE;
        pthread_mutex_init (&ppu->info.remote.mutexTasksAvailable, NULL);
        pthread_cond_init (&ppu->info.remote.condTasksAvailable, NULL);
        StartProcessingUnit (ppu, fWait);
    }
    
    return ppu;
}


static void DestroyProcessingUnit (int procunit_id)
{
    procunit **plpu = &gpulist;
    procunit *ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
    
    if (ppu == NULL) return;
    
    StopProcessingUnit (ppu);

    /* remove ppu from list and free ppu */
    while (*plpu != NULL) {
        PrintProcessingUnitInfo (*plpu);
        if (*plpu == ppu) {
            procunit *next = ppu->next;
            free ((char *) ppu);
            *plpu = next;
            break;
        }
        plpu = &((*plpu)->next);
    }
    
}



static void PrintProcessingUnitInfo (procunit *ppu)
{
    char asTasks[256] = "";
    char asAddress[256] = "n/a";
    /* make up "tasks" string */
    if (ppu->taskMask & pu_task_rollout) strcat (asTasks, "rollout ");
    if (ppu->taskMask & pu_task_eval) strcat (asTasks, "eval ");
    if (asTasks[0] == 0) strcat (asTasks, "none");
    /* make remote procunit address string */
    if (ppu->type == pu_type_remote) {
        sprintf (asAddress, "%s", inet_ntoa (ppu->info.remote.inAddress));
    }
    outputf ("  %2i  %-6s  %-10s %5d  %-20s  %-20s\n" ,
                ppu->procunit_id, 
                asProcunitType[ppu->type],
                asProcunitStatus[ppu->status],
                ppu->maxTasks,
                asTasks,
                asAddress);
}


extern void PrintProcessingUnitList (void)
{
    procunit *ppu = gpulist;
    
    outputf ("  Id  Type    Status     Queue  Tasks               "
             "  Address  \n");
    
    while (ppu != NULL) {
        PrintProcessingUnitInfo (ppu);
        ppu = ppu->next;
    }

}


static void PrintProcessingUnitStats (int procunit_id)
{
    procunit *ppu = gpulist;
        
    if (procunit_id > 0) {
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        if (ppu != NULL) {
            float	rolloutSpeed = ppu->rolloutStats.totalDone == 0 ?
                        0.0f : ppu->rolloutStats.totalDone / ppu->rolloutStats.avgSpeed;
            outputf ("Processing unit id %d stats:\n"
                    "  Type: %s\n"
                    "  Rollout stats:\n"
                    "    Tasks done: %d (failed: %d)\n"
                    "    Average speed: %f tasks/sec\n"
                    "  Evaluation stats:\n"
                    "    Tasks done: %d (failed: %d)\n"
                    "    Average speed: %f tasks/sec\n",
                    procunit_id, 
                    asProcunitType[ppu->type],
                    ppu->rolloutStats.totalDone, 
                    ppu->rolloutStats.totalFailed, 
                    rolloutSpeed,
                    ppu->evalStats.totalDone, 
                    ppu->evalStats.totalFailed, 
                    0.0f);
        }
    }
    
    else {
        outputf ("  Id  Type    Status  Rollouts (tasks/s)  "
                 "  Evals (tasks/s) \n");

        while (ppu != NULL) {
            float	rolloutSpeed = ppu->rolloutStats.totalDone == 0 ?
                        0.0f : ppu->rolloutStats.totalDone / ppu->rolloutStats.avgSpeed;
            outputf ("  %-2i  %-6s  %-8s  %6d (%5.2f/s)   %6d (%5.2f/s)\n", 
                        ppu->procunit_id, 
                        asProcunitType[ppu->type],
                        asProcunitStatus[ppu->status],
                        ppu->rolloutStats.totalDone, 
                        rolloutSpeed,
                        ppu->evalStats.totalDone, 
                        0.0f);
            ppu = ppu->next;
        }
    }
}


/* Finds a processing unit in the available processing units list;
   search can be started from an arbitrary processing unit ppu in the list;
   pu_type_none, pu_stat_none and pu_task_none act as jokers for search 
   criteria type, status and taskType.
   Returns a pointer to the found processing unit, or NULL if none found.
*/
static procunit *FindProcessingUnit (procunit *ppu, pu_type type, pu_status status, pu_task_type taskType, int procunit_id)
{
    if (ppu == NULL) ppu = gpulist;
    
    while (ppu != NULL) {
        if ((type == pu_type_none || type == ppu->type)
        &&  (status == pu_stat_none || status == ppu->status)
        &&  (taskType == pu_task_none || taskType & ppu->taskMask)
        &&  (procunit_id == 0 || procunit_id == ppu->procunit_id))
            return ppu;
            
        ppu = ppu->next;
    }
    
    return NULL;
}


/* start a processing unit.
    - local procunit: mereley set it to ready state; a thread will
        be launched according to assigned tasks by, by calling
        Threaded_xxxRollout ();
    - remote procunit: create the thread for handling the connection
        with the slave; if fWait is set, we don't return till the 
        RPU is actually running (usage: give synchronous feedback to
        the user when he enters command "pu start <procunit_id>")
*/
static int StartProcessingUnit (procunit *ppu, int fWait)
{
    switch (ppu->type) {
    
        case pu_type_local:
            /* not much to do to start a local procunit  */
            ppu->status = pu_stat_ready;
            return 0;
    
        case pu_type_remote:
            if (fWait) {
                pthread_mutex_lock (&ppu->mutexStatusChanged);
                StartRemoteProcessingUnit (ppu);
                pthread_cond_wait (&ppu->condStatusChanged, 
                                    &ppu->mutexStatusChanged);
                pthread_mutex_unlock (&ppu->mutexStatusChanged);
                if (ppu->status != pu_stat_deactivated) 
                    return 0;
            }
            else
                StartRemoteProcessingUnit (ppu);
        default:
          assert ( FALSE );
          break;
    }
    
    return -1;
}


static int StartRemoteProcessingUnit (procunit *ppu)
{
    int err;
    
    ppu->info.remote.fStop = FALSE;
    ppu->info.remote.fInterrupt = FALSE;
    
    err = pthread_create (&ppu->info.remote.thread, 0L, Thread_RemoteProcessingUnit,  ppu);
    if (err == 0) pthread_detach (ppu->info.remote.thread);
    
    if (err == 0) {
        if (RPU_DEBUG) fprintf (stderr, "Started remote procunit thread.\n");
    }
    else {
        fprintf (stderr, "*** StartRemoteProcessingUnit: pthread_create() error (%d).\n", err);
        assert (FALSE);
    }
    
    return err;
}


static int StopProcessingUnit (procunit *ppu)
{
    int step = -3;	/* wait 3 secs before displaying wait message */
    
    switch (ppu->type) {
    
        case pu_type_local:
            ppu->status = pu_stat_deactivated;
            return 0;
    
        case pu_type_remote:
            pthread_mutex_lock (&ppu->mutexStatusChanged);
            ppu->info.remote.fInterrupt = TRUE;
            ppu->info.remote.fStop = TRUE;
            while (ppu->status != pu_stat_deactivated) {
                WaitForCondition (&ppu->condStatusChanged, 
                            &ppu->mutexStatusChanged,
                            "Waiting for remote processing unit to stop...", &step);
            }
            ppu->info.remote.fInterrupt = FALSE;
            pthread_mutex_unlock (&ppu->mutexStatusChanged);
            if (ppu->status == pu_stat_deactivated) 
                return 0;
    
        default:
          assert ( FALSE );
          break;

    }
    
    return -1;

}


/* ChangeProcessingUnitStatus ()
    Using this function instead of changing the ppu->status field
    will allow threads waiting for ppu->condStatusChanged to be
    immediately unlocked
*/
static void ChangeProcessingUnitStatus (procunit *ppu, pu_status status)
{
    assert (ppu != NULL);
    
    pthread_mutex_lock (&ppu->mutexStatusChanged);
    
    ppu->status = status;

    pthread_cond_broadcast (&ppu->condStatusChanged);
    pthread_mutex_unlock (&ppu->mutexStatusChanged);

}


static int WaitForAllProcessingUnits (void)
{
    procunit *ppu = gpulist;
    
    if (RPU_DEBUG) PrintProcessingUnitList ();
    
    while (ppu != NULL) {
        pthread_mutex_lock (&ppu->mutexStatusChanged);
        if (ppu->status != pu_stat_ready && ppu->status != pu_stat_deactivated) {
            int step = -3;
            char s[256];
            sprintf (s, "Waiting for procunit id=%d to interrupt...", ppu->procunit_id);
            while (ppu->status != pu_stat_ready && ppu->status != pu_stat_deactivated) {
                WaitForCondition (&ppu->condStatusChanged, &ppu->mutexStatusChanged,
                                    s, &step);
                if (RPU_DEBUG && step % 10 == 0) {
                    PrintTaskList ();
                    PrintProcessingUnitList ();
                }
            }
        }
        pthread_mutex_unlock (&ppu->mutexStatusChanged);
        ppu = ppu->next;
    } 
    

    if (RPU_DEBUG) fprintf (stderr, "### All procunits ready or deactivated.\n");
    if (RPU_DEBUG) PrintProcessingUnitList ();
    
    return 0;
}



static void CancelProcessingUnitTasks (procunit *ppu)
{
    int i;
    int procunit_id = ppu->procunit_id;
    
    if (taskList == NULL) return;
    
    pthread_mutex_lock (&mutexTaskListAccess);

    for (i = 0; i < taskListMax; i ++) {
        if (taskList[i] != NULL && taskList[i]->procunit_id == procunit_id) {
            if (taskList[i]->status == pu_task_inprogress) {
                switch (taskList[i]->type) {
                    case pu_task_rollout: ppu->rolloutStats.totalFailed ++; break;
                    case pu_task_eval: ppu->evalStats.totalFailed ++; break;
                    default:
                      assert( FALSE );
                      break;
                }
                taskList[i]->status = pu_task_todo;
            }
            taskList[i]->procunit_id = -1;
        }
    }

    pthread_mutex_unlock (&mutexTaskListAccess);
}


static int InterruptAllProcessingUnits ()
{
    procunit *ppu = gpulist;
    
     while (ppu != NULL) {
        
        switch (ppu->type) {
        
            case pu_type_local:
                break;
                
            case pu_type_remote:
                CancelProcessingUnitTasks (ppu);
                ppu->info.remote.fInterrupt = TRUE;
                break;

            default:
                assert( FALSE );
                 break;
        }
    
        ppu = ppu->next;
    } 
    
    return 0;
}



/* Returns TRUE if successfully started */
static int RunTaskOnLocalProcessingUnit (pu_task *pt, procunit *ppu)
{
    int	err = -1;
    
            
    switch (pt->type) {
    
        case pu_task_rollout:
            
            switch (pt->taskdata.rollout.type) {

                case pu_task_rollout_bearoff:
                    err = pthread_create (&ppu->info.local.thread, 0L, Threaded_BearoffRollout,  pt);
                    if (err == 0) pthread_detach (ppu->info.local.thread);
                    break;
                    
                case pu_task_rollout_basiccubeful:
                    err = pthread_create (&ppu->info.local.thread, 0L, Threaded_BasicCubefulRollout,  pt);
                    if (err == 0) pthread_detach (ppu->info.local.thread);
                    break;

            }
            break;
        
        case pu_task_eval:
            /* for future use */
            break;

       default:
           assert( FALSE );
           break;

    }
    
    if (err == 0) {
        pt->status = pu_task_inprogress;
        pt->timeStarted = clock ();
        pt->procunit_id = ppu->procunit_id;
        if (GetProcessingUnitsMode () == pu_mode_slave)
            Slave_UpdateStatus ();
    }
    
    if (PU_DEBUG) {
        if (err == 0) fprintf (stderr, "Started local procunit (0x%x)\n", 
                               (int) ppu->info.local.thread);
        else fprintf (stderr, "*** RunTaskOnLocalProcessingUnit(): Could not start local procunit!\n");
    }
    
    return ( err == 0 );
}


/* RunTaskOnRemoteProcessingUnit ()
    
    Returns TRUE if successfully started
    Caution: the task is considered started as soon as it is
    accepted by local half of the rpu; it does not assess anything
    about the task being processed (or even received) on the remote 
    (slave) half of the rpu 
*/
static int RunTaskOnRemoteProcessingUnit (pu_task *pt, procunit *ppu)
{
    int	err = 0;
    
    ppu->status = pu_stat_busy;
    //pt->status = pu_task_inprogress; // will be set by Thread_RemoteProcessingUnit()
    pt->timeStarted = clock ();
    pt->procunit_id = ppu->procunit_id;

    /* wake up the local rpu half thread waiting for tasks to do */
    pthread_mutex_lock (&ppu->info.remote.mutexTasksAvailable);
    ppu->info.remote.fTasksAvailable = TRUE;
    pthread_cond_signal (&ppu->info.remote.condTasksAvailable);
    pthread_mutex_unlock (&ppu->info.remote.mutexTasksAvailable);
    
    if (PU_DEBUG && err == 0) fprintf (stderr, "Signal sent to remote procunits\n");
    if (err != 0) fprintf (stderr, "*** RunTaskOnRemoteProcessingUnit(): Could not "
                        "start remote procunit processing!\n");
    
    return ( err == 0 );
}

static void RunTaskOnProcessingUnit (pu_task *pt, procunit *ppu)
{
    int started = 0;

    switch (ppu->type) {
        case pu_type_local:
            started = RunTaskOnLocalProcessingUnit (pt, ppu);
            break;
        case pu_type_remote:
            started = RunTaskOnRemoteProcessingUnit (pt, ppu);
            break;
        
        default:
            assert( FALSE );
            break;

    }

    if (started) {
        ppu->status = pu_stat_busy;
        switch (pt->type) {
            case pu_task_rollout:
                ppu->rolloutStats.total ++;
                break;
            case pu_task_eval:
                ppu->evalStats.total ++;
                break;
            default:
                assert( FALSE );
                break;
        }
    }
}


extern void InitTasks (void)
{
    int i;
    
    taskListMax = DEFAULT_TASKLISTMAX;
    
    taskList = (pu_task **) malloc (taskListMax * sizeof (pu_task *));
    assert (taskList != NULL);
    
    for (i = 0; i < taskListMax; i ++)
        taskList[i] = NULL;
}


static void ReleaseTasks (void)
{
    int i;
    
    if (taskList == NULL) return;

    for (i = 0; i < taskListMax; i ++)
        if (taskList[i] != NULL) {
            FreeTask (taskList[i]);
            taskList[i] = NULL;
        }
        
    free ((char *) taskList);
    taskList = NULL;
}



extern pu_task *CreateTask (pu_task_type type, int fDetached)
{
    pu_task	*pt = NULL;
    
    if (taskList == NULL) return NULL;

    if (fDetached) {
        pt = (pu_task *) calloc (1, sizeof (pu_task));
    }
    else {
        int 	i;
        for (i = 0; i < taskListMax; i++) {
            if (taskList[i] == NULL) {
                pt = (pu_task *) calloc (1, sizeof (pu_task));
                if (pt != NULL) {
                    taskList[i] = pt;
                    break;
                }
                else {
                    fprintf (stderr, "*** CreateTask(): calloc() error.\n");
                    assert (FALSE);
                }
            }
        }
    }
    
    if (pt != NULL) {
        pt->type = type;
        pt->status = pu_task_todo;
        pt->task_id.masterId = 1; 
        pt->task_id.taskId = fDetached ? 0 : taskId ++; 
        pt->task_id.src_taskId = 0; 
        pt->procunit_id = -1; /* not handled yet */
        pt->timeCreated = clock (); 
        pt->timeStarted = 0; 
    }
    
    return pt;
}


static pu_task *FindTask (pu_task_status status, pu_task_type taskMask, int taskId, int procunit_id)
{
    int i;
    
    if (taskList == NULL) return NULL;

    for (i = 0; i < taskListMax; i++)
        if ((taskList[i] != NULL) 
        &&  ((status == 0) || taskList[i]->status == status) 
        &&  ((taskMask == 0) || taskList[i]->type & taskMask)
        &&  ((taskId == 0) || taskList[i]->task_id.taskId == taskId)
        &&  ((procunit_id == 0) || taskList[i]->procunit_id == procunit_id))
            return taskList[i];
            
    return NULL;
}


static int GetTaskCount (pu_task_status status, pu_task_type taskMask)
{
    int i, n = 0;
    
    if (taskList == NULL) return 0;
    
    for (i = 0; i < taskListMax; i++)
        if ((taskList[i] != NULL) 
        &&  ((status == 0) || taskList[i]->status == status) 
        &&  ((taskMask == 0) || taskList[i]->type & taskMask))
            n ++;
            
    return n;
}


static pu_task *AttachTask (pu_task *pt)
{
    int i;
    
    if (taskList == NULL) return NULL;

    for (i = 0; i < taskListMax; i++)
        if (taskList[i] == NULL) {
            taskList[i] = pt;
            return pt;
        }
        
    fprintf (stderr, "*** AttachTask() failed.\n");
    assert (FALSE);
    
    return NULL;
}


extern void PrintTaskList (void)
{
    int i;
    
    if (taskList == NULL) return;

    for (i = 0; i < taskListMax; i++)
        if (taskList[i] != NULL) {
            outputf ("  Task #%d id=%d.%d status=%s (procunit id=%d)\n", i, taskList[i]->task_id.masterId,
                        taskList[i]->task_id.taskId, asTaskStatus[taskList[i]->status], taskList[i]->procunit_id);
        }
}


void FreeTask (pu_task *pt)
{
    int i;
    
    switch (pt->type) {
    
        case pu_task_rollout: {
            pu_task_rollout_data *prd = &pt->taskdata.rollout;
            
            free ((char *) prd->aanBoardEval);
            free ((char *) prd->aar);
            
            switch (prd->type) {
                
                case pu_task_rollout_bearoff: 
                break;
            
                case pu_task_rollout_basiccubeful: {
                        pu_task_rollout_basiccubeful_data *psd = &prd->specificdata.basiccubeful;
                        
                        free ((char *) psd->aci);
                        free ((char *) psd->afCubeDecTop);
                        free ((char *) psd->aaarStatistics);
                    }
                    break;
                
                default:
                    assert (FALSE);
            
            } /* switch */
        }
        break;
        
        case pu_task_eval:
            /* for future use */
            break;

        default:
            assert( FALSE );
            break;
    
    } /* switch */
    
    for (i = 0; i < taskListMax; i++)
        if (taskList[i] == pt) {
            taskList[i] = NULL;
            break;
        }
    
    free ((char *) pt);
}


static int AssignTasksToProcessingUnits (void)
{
    procunit 	*ppu = gpulist;
    int		totalAvailQueue = 0;
    int		totalToDoTasks = 0;
    int		cPickedUpTasks = 0;
    float	rFactor = 1.0f;

    while (ppu != NULL) {
        if (ppu->status == pu_stat_ready) 
            totalAvailQueue += ppu->maxTasks;
        ppu = ppu->next;
    }
    
    totalToDoTasks = GetTaskCount (pu_task_todo, 0);
    
    if (totalToDoTasks < totalAvailQueue) 
            rFactor = ((float) totalToDoTasks) / totalAvailQueue;
    
    if (PU_DEBUG) fprintf (stderr, "Tasks to do: %d, Avail queue: %d, rFactor: %f\n",
                totalToDoTasks, totalAvailQueue, rFactor);
                
    ppu = gpulist;
    while (ppu != NULL) {
        /* find a ready procunit */
        if (ppu->status == pu_stat_ready) {
            /* find a todo task, assigned to no-one, and that can 
                be handled by that procunit */
            int		maxTasks;
            int		cTasks = 0;
            maxTasks = ppu->maxTasks * rFactor + 0.99f; /* do all you can do, unless few tasks left */
            if (maxTasks < 1 && ppu->maxTasks > 0) maxTasks = 1; /* do one task at least */
            while (cTasks < maxTasks) {
                pu_task *pt = FindTask (pu_task_todo, ppu->taskMask, 0, -1);
                if (pt != NULL) {
                    if (PU_DEBUG) fprintf (stderr, "Picked up task %d.%d for procunit %d (%d/%d/%d).\n", 
                                    pt->task_id.masterId, pt->task_id.taskId, ppu->procunit_id,
                                    cTasks + 1, maxTasks, ppu->maxTasks);
                    RunTaskOnProcessingUnit (pt, ppu);
                    cTasks ++;
                }
                else break;
            }
            cPickedUpTasks += cTasks;
        }
        ppu = ppu->next;
    }

    return cPickedUpTasks;
}


/* creates an array of todo tasks (*papt) assigned to ppu;
    returns the number of tasks put in *papt (caller must
    free *papt after use), or 0 if none (*papt set to NULL) */
    
static int MakeAssignedTasksList (procunit *ppu, pu_task_status status, pu_task ***papt, int maxTasks)
{
    int 	i, cTasks = 0;
    
    *papt = NULL;
    
    if (taskList == NULL) return 0;

    if (maxTasks > ppu->maxTasks) maxTasks = ppu->maxTasks;
    if (maxTasks <= 0) return 0;
    
    *papt = (pu_task **) malloc (sizeof (pu_task *) * maxTasks);
    assert (*papt != NULL);
    
    for (i = 0; i < taskListMax &&  cTasks < maxTasks; i ++) {
        if (taskList[i] != NULL
        &&  taskList[i]->status == status
        &&  taskList[i]->procunit_id == ppu->procunit_id) {
            (*papt)[cTasks ++] = taskList[i];
            if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU picked up jobtask #%d id=%d.\n", 
                                    (int) pthread_self (), cTasks, taskList[i]->task_id.taskId);
        }
    }
    
    return cTasks;
}


extern void MarkTaskDone (pu_task *pt, procunit *ppu)
{
    //fprintf (stderr, "# Task done.\n");
    
    /* mark the task done in task list; 
        this function is called by threaded procunits (like
        Threaded_BasicCubefulRollout), so we need to be
        careful not to access the task list while it is already 
        operated on by RolloutGeneral() in main thread */
    
    /* !!! check first that this task is still in the task list and still
        assigned to this procunit (it may have been removed or reassigned 
        to another procunit) !!! */

    if (ppu == NULL)
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, pt->procunit_id);

    pt->status = pu_task_done;
    pt->procunit_id = -1;
    
    if (ppu != NULL) {
        float duration = (clock () - pt->timeStarted) / CLOCKS_PER_SEC;
        switch (pt->type) {
            case pu_task_rollout:
                ppu->rolloutStats.totalDone ++;
                ppu->rolloutStats.avgSpeed += duration;
                break;
            case pu_task_eval:
                ppu->evalStats.totalDone ++;
                ppu->evalStats.avgSpeed += duration;
                break;
            default:
                assert( FALSE );
                break;
        }
        
        if (ppu->type == pu_type_local)
            ppu->status = pu_stat_ready; /* rpu's revert themselves to ready state when ALL
                                        the tasks in their current job have been completed */
    }
    
    /* signal to TaskEngine_GetCompletedTask(), called from
        RolloutGeneral() (master rollout) or Slave_DoJob() 
        (slave rollout), in main thread, that it may unblock */
    pthread_mutex_lock (&mutexResultsAvailable);
    gResultsAvailable = TRUE;
    pthread_cond_signal (&condResultsAvailable);
    pthread_mutex_unlock (&mutexResultsAvailable);
}


extern void TaskEngine_Init (void)
{
    if (PU_DEBUG) fprintf (stderr, "Starting Task Engine...\n");
    InitTasks ();
    gResultsAvailable = FALSE;
}


extern void TaskEngine_Shutdown (void)
{
    if (PU_DEBUG) fprintf (stderr, "Stopping Task Engine...\n");
    InterruptAllProcessingUnits ();
    WaitForAllProcessingUnits ();
    ReleaseTasks ();
}


extern int TaskEngine_Full (void)
{
    int i;
    
    for (i = 0; i < taskListMax; i ++)
        if (taskList[i] == NULL) return FALSE;
        
    return TRUE;
}


extern int TaskEngine_Empty (void)
{
    int i;
    
    for (i = 0; i < taskListMax; i ++)
        if (taskList[i] != NULL) return FALSE;
        
    return TRUE;
}


extern pu_task * TaskEngine_GetCompletedTask (void)
{
    int done = FALSE;
    static int foundLastTime = TRUE;
    
    
    if (TaskEngine_Empty ()) return NULL;
    
    while (!done) {
        pu_task *pt;
        
        pthread_mutex_lock (&mutexTaskListAccess);
        
        /* sanity check */
        
        /* assign procunits to todo tasks */
        AssignTasksToProcessingUnits ();
        
        /* look for available results */
        pt = FindTask (pu_task_done, 0, 0, 0);

        pthread_mutex_unlock (&mutexTaskListAccess);

            if (RPU_DEBUG > 1 && pt != NULL) {
                switch (pt->type) {
                    case pu_task_rollout: {
                            float *pf = (float *) pt->taskdata.rollout.aar;
                            fprintf (stderr, "   %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %d\n", 
                                pf[0], pf[1], pf[2], pf[3], pf[4], pf[5],
                                pt->taskdata.rollout.seed);
                        }
                    default:
                        assert( FALSE );
                        break;
                }
            }

        /* results available: return them */
        if (pt != NULL) {
            foundLastTime = TRUE;
            return pt;
        }

        /* if we returned results last call, return immediately
            control to caller without blocking for new results */
        if (foundLastTime) {
            foundLastTime = FALSE;
            break;
        }
        
        /* no results available now, so we */
        /* wait for new results to come in */
        pthread_mutex_lock (&mutexResultsAvailable);
        if (!gResultsAvailable) {
            while (!gResultsAvailable && !fInterrupt) 
                WaitForCondition (&condResultsAvailable, &mutexResultsAvailable, 
                                    NULL, NULL);
        }
        gResultsAvailable = FALSE;
        pthread_mutex_unlock (&mutexResultsAvailable);

    }
    
    return NULL;
}


static void RPU_DumpData (unsigned char *data, int len)
{
    int i;
    
    fprintf (stderr, "[");
    
    for (i = 0; i < len && i < 200; i ++) {
        if (i > 0 && i % 4 == 0) fprintf (stderr, " ");
        if (i > 0 && i % 32 == 0) fprintf (stderr, "\n ");
        fprintf (stderr, "%02x", data[i]);
    }
    if (i < len) fprintf (stderr, " (... +%d bytes) ", len - i);
    
    fprintf (stderr, "]\n");
}



static int RPU_PackJob (char **job, int *jobSize, int *p, void *data, int size)
{
    if (RPU_DEBUG_PACK) 
        fprintf (stderr, "  > RPU_PackJob(): size=%d bytes at p=+%d\n", size, *p);
        
    while ((*p) + size > *jobSize) {
      char *newJob;
        if (RPU_DEBUG_PACK) fprintf (stderr, "[Resizing Job]");
        /* no more room, let's double the size of the job */
        *jobSize *= 2;
        newJob = realloc (*job, *jobSize);
        if (newJob == NULL) {
            /* realloc won't work: let's do it the long way */
            newJob = malloc (*jobSize);
            assert (newJob != NULL);
            memcpy (newJob, *job, *jobSize / 2);
            free (*job);
            *job = newJob;
        }
    }
    
    memcpy ((*job) + *p, data, size);
    (*p) += size;
    
    if (RPU_DEBUG_PACK > 1) RPU_DumpData (data, size);
    return size;  
}

static int RPU_PackJobPtr (char **job, int *jobSize, int *p, char *data, int size, int q)
{
    int n = 0;
    int totalSize = sizeof (q) + q * size;
    
    if (RPU_DEBUG_PACK) 
        fprintf (stderr, "  > RPU_PackJobPtr(): size=%d bytes at p=+%d\n", 
                        totalSize, *p);
    n += RPU_PackJob (job, jobSize, p, &totalSize, sizeof (totalSize));
    n += RPU_PackJob (job, jobSize, p, data, q * size);
    
    return n;
}

#define PACKJOB(x) len += RPU_PackJob ((char **) &pjt, &jobSize, &p, \
                                    (char *) &(x), sizeof (x)); 
#define PACKJOBPTR(x,q,t) len += RPU_PackJobPtr ((char **) &pjt, &jobSize, &p, \
                                    (char *) (x), sizeof (t), (q))
#define DEFAULT_JOBSIZE (100 * 1024)

static rpu_jobtask * RPU_PackTaskToJobTask (pu_task *pt)
{
    int 		len = sizeof (rpu_jobtask);
    int			jobSize = DEFAULT_JOBSIZE;
    rpu_jobtask		*pjt;
    int			p;	/* offset from beginning of pjt */
    
    pu_task_rollout_data	*prd;
    pu_task_rollout_bearoff_data *psb;
    pu_task_rollout_basiccubeful_data *psc;
    
    pjt = malloc (jobSize);
    assert (pjt != NULL);

    pjt->type = pt->type;
    if (pt->task_id.src_taskId == 0)
        pt->task_id.src_taskId = pt->task_id.taskId;
    p = offsetof (rpu_jobtask, data);	    
    
    switch (pt->type) {
    
        case pu_task_rollout:
            prd = &pt->taskdata.rollout;
            PACKJOB (pt->task_id.src_taskId);
            PACKJOB (prd->type);
            switch (prd->type) {
                case pu_task_rollout_bearoff:
                    psb = &prd->specificdata.bearoff;
                    PACKJOBPTR (prd->aanBoardEval, 2 * 25, int);
                    PACKJOBPTR (prd->aar, NUM_ROLLOUT_OUTPUTS, float);
                    PACKJOB(psb->nTruncate);
                    PACKJOB(psb->nTrials);
                    PACKJOB(psb->bgv);
                    break;
                case pu_task_rollout_basiccubeful:
                    psc = &prd->specificdata.basiccubeful;
                    PACKJOBPTR (prd->aanBoardEval, 2 * 25 * psc->cci, int);
                    PACKJOBPTR (prd->aar, NUM_ROLLOUT_OUTPUTS * psc->cci, float);
                    PACKJOBPTR(psc->aci, psc->cci, cubeinfo);
                    PACKJOBPTR(psc->afCubeDecTop, psc->cci, int);
                    PACKJOB(psc->cci);
                    PACKJOBPTR(psc->aaarStatistics, 2 * psc->cci, rolloutstat);
                    break;
            }
            PACKJOB(prd->rc);
            PACKJOB(prd->seed);
            PACKJOB(prd->iTurn);
            PACKJOB(prd->iGame);
            break;
            
        case pu_task_eval:
            break;

       default:
           assert( FALSE );
           break;
    
    }

    pjt->len = len;
    if (RPU_DEBUG_PACK) fprintf (stderr, "[JobTask packed, %d bytes]\n", len);
    
    return pjt;
}


static rpu_job * RPU_PackTaskListToJob (pu_task **apTasks, int cTasks)
{
    rpu_job	*pJob = NULL;
    rpu_jobtask **apJobTasks;
    int		i, totalJobTasksSize = 0;
    
    if (cTasks <= 0) {
        *apTasks = NULL;
        return NULL;
    }    
     
    apJobTasks = (rpu_jobtask **) malloc (sizeof (rpu_jobtask *) * cTasks);
    assert (apJobTasks != NULL);
    
    /* pack individual tasks into individual jobtasks */
    for (i = 0; i < cTasks; i ++) {
        apJobTasks[i] = RPU_PackTaskToJobTask (apTasks[i]);
        assert (apJobTasks[i] != NULL);
        if (RPU_DEBUG_PACK) fprintf (stderr, "[Job #%d packed to jobtask, %d bytes]\n", 
                                                i, apJobTasks[i]->len);
        if (RPU_DEBUG_PACK > 1) RPU_DumpData ( (char *) apJobTasks[i], 
                                               apJobTasks[i]->len);
        totalJobTasksSize += apJobTasks[i]->len;
    }
    
    if (totalJobTasksSize != 0) {
        /* create a "to do" job to wrap jobtasks */
        int	pjoblen = sizeof (rpu_job) + totalJobTasksSize;
        
        pJob = malloc (pjoblen);
        assert (pJob != NULL);
        
        if (pJob != NULL) {
            /* embed jobtasks into the job */
            char *p = (char *) &(pJob->data);
            pJob->len = pjoblen;
            pJob->cJobTasks = cTasks;
            for (i = 0; i < cTasks; i ++) {
                memcpy (p, apJobTasks[i], apJobTasks[i]->len);
                if (RPU_DEBUG_PACK) fprintf (stderr, "[Jobtask #%d moved to job, %d bytes]\n", 
                                                        i, apJobTasks[i]->len);
                if (RPU_DEBUG_PACK > 1) RPU_DumpData ( (char *) p, apJobTasks[i]->len);
                p += apJobTasks[i]->len;
                free ((char *) apJobTasks[i]);
            }
            if (RPU_DEBUG_PACK) fprintf (stderr, "[Job packed, %d task(s), %d bytes]\n", cTasks, pjoblen);
            if (RPU_DEBUG_PACK > 1) RPU_DumpData ( (char *) pJob, pjoblen);
        }
    }
    
    free ((char *) apJobTasks);
    if (PU_DEBUG_FREE) fprintf (stderr, "free(): RPU_PackTaskListToJob\n");
    
    return pJob;
}

#define UNPACKJOB(x) \
        if (RPU_DEBUG > 1) { \
            fprintf (stderr, "  > UNPACKJOB(): size=%d bytes at p=+%d\n", \
                sizeof (x), (int) p - (int) pjt); \
            RPU_DumpData (p, sizeof (x)); } \
        x = *((typeof (x) *) p); p += sizeof (x); 

#define UNPACKJOBPTR(x) \
        len = (*((int *) p)) - sizeof(int); \
        if (RPU_DEBUG_PACK > 1) { \
            fprintf (stderr, "  > UNPACKJOBPTR(): size=%d+4 bytes at p=+%d\n", \
                len, (int) p - (int) pjt); \
            RPU_DumpData (p + sizeof (int), len); } \
        p += sizeof (int); \
        x = malloc (len); memcpy (x, p, len); p += len; 

static pu_task * RPU_UnpackJobToTask (rpu_jobtask *pjt, int fDetached)
{
    pu_task	*pt;
    char	*p = (char *) &pjt->data;
    int		len;
    
    pu_task_rollout_data		*prd;
    pu_task_rollout_bearoff_data 	*psb;
    pu_task_rollout_basiccubeful_data 	*psc;
    
    if (RPU_DEBUG) fprintf (stderr, "RPU_UnpackJobToTask...\n");
    
    pt = CreateTask (pjt->type, fDetached);
    assert (pt != NULL);
    
    if (pt != NULL) {
            
        switch (pjt->type) {
        
            case pu_task_rollout:
                prd = &pt->taskdata.rollout;
                UNPACKJOB (pt->task_id.src_taskId);
                UNPACKJOB (prd->type);
                UNPACKJOBPTR (prd->aanBoardEval);
                UNPACKJOBPTR (prd->aar);
                switch (prd->type) {
                    case pu_task_rollout_bearoff:
                        psb = &prd->specificdata.bearoff;
                        UNPACKJOB(psb->nTruncate);
                        UNPACKJOB(psb->nTrials);
                        UNPACKJOB(psb->bgv);
                        break;
                    case pu_task_rollout_basiccubeful:
                        psc = &prd->specificdata.basiccubeful;
                        UNPACKJOBPTR(psc->aci);
                        UNPACKJOBPTR(psc->afCubeDecTop);
                        UNPACKJOB(psc->cci);
                        UNPACKJOBPTR(psc->aaarStatistics);
                        break;
                }
                UNPACKJOB(prd->rc);
                UNPACKJOB(prd->seed);
                UNPACKJOB(prd->iTurn);
                UNPACKJOB(prd->iGame);
                break;
                
            case pu_task_eval:
                break;
                
            default:
                fprintf (stderr, "*** Unknown type of jobtask (%d).\n", pjt->type);
                free ((char *) pt);
                pt = NULL;        
        }
    }

    return pt;
}


static int RPU_SendMessage (int toSocket, rpu_message *msg, volatile int *pfInterrupt)
{
    int 	n;
    int 	fError, fShutdown;
    
    //sigset_t	ss, oss;
    //sigemptyset (&ss);
    //sigaddset (&ss, SIGPIPE);
    //pthread_sigmask (SIG_BLOCK, &ss, &oss);
    
    
    do {
        n = send (toSocket, msg, msg->len, 0);
        fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
        fShutdown = (n == 0 || n == ECONNRESET);
        if (fShutdown) { 
            if (RPU_DEBUG) fprintf (stderr, "*** Connection closed by peer.\n");
            fError = FALSE; n = 0;
        }
    } while (n != msg->len  && !fError && !*pfInterrupt);

    //pthread_sigmask (SIG_SETMASK, &oss, NULL);

    if (n != msg->len) {
        if (fError) perror ("*** RPU_SendMessage: send()"); 
        else fprintf (stderr, "*** RPU_SendMessage: sent incomplete message.\n");
        return -1;
    }
    
    return 0;
}


/* timeout in seconds; timeout == 0 for no timeout */
static rpu_message * RPU_ReceiveMessage (int fromSocket, volatile int *pfInterrupt, int timeout)
{
    rpu_message *msg = NULL;
    int 	n, len;
    int 	fNoData, fError, fShutdown;
    
    if (RPU_DEBUG && GetProcessingUnitsMode () == pu_mode_slave)
        fprintf (stderr, "Enter RPU_ReceiveMessage()...\n");
    
    /* receive first chunck containing whole message length */
    do {
        n = recv (fromSocket, &len, sizeof (len), MSG_PEEK | MSG_WAITALL);
        fNoData = (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
        fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
        fShutdown = (n == 0 || n == ECONNRESET);
        if (fShutdown) { 
            if (RPU_DEBUG) fprintf (stderr, "*** Connection closed by peer.\n");
            fError = FALSE; n = 0;
        }
        if (fNoData && --timeout == 0) fError = -1;
    } while (n != sizeof (len) && !fError && !fShutdown && !*pfInterrupt);
    
    if (n != sizeof (len)) {
        if (fError) perror ("*** RPU_ReceiveMessage: recv() #1");
        else if (n != 0) fprintf (stderr, "*** RPU_ReceiveMessage: received incomplete "
                            "message #1 (%d/%d bytes).\n", n, sizeof (len));
    }
    else if (!fError && !fShutdown) {
        /* allocate msg */
        msg = (rpu_message *) malloc (len);
        assert (msg != NULL);
        
        /* receive the whole message */
        do {
            n = recv (fromSocket, msg, len, MSG_WAITALL);
            fNoData = (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
            fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
            fShutdown = (n == 0 || n == ECONNRESET);
            if (fShutdown) { 
                if (RPU_DEBUG) fprintf (stderr, "*** Connection closed by peer.\n");
                fError = FALSE; n = 0;
            }
            if (fNoData && --timeout == 0) fError = -1;
        } while (n != len  && !fError && !fShutdown && !*pfInterrupt);
        
        if (n != len || fError || fShutdown) {
            if (fError) perror ("*** RPU_ReceiveMessage: recv() #2");
            else if (n != 0) fprintf (stderr, "*** RPU_ReceiveMessage: received incomplete "
                            "message #2 (%d/%d bytes).\n", n, len);
            if (msg != NULL) free (msg);
            msg = NULL;
        }
    }
    
    return msg;
}


static rpu_message * RPU_MakeMessage (pu_message_type type, void *data, int datalen)
{
    int		msglen = offsetof (rpu_message, data) + datalen;
    rpu_message *msg = malloc (msglen);
    
    assert (msg != NULL);
    
    msg->len = msglen;
    msg->type = type;
    msg->version = RPU_MSG_VERSION;
    memcpy (&msg->data, data, datalen);
    
    return msg;
}


static void * RPU_GetMessageData (rpu_message *msg, pu_message_type msgType)
{
    if (msg == NULL) return NULL;
    
    if (msg->version > RPU_MSG_VERSION) {
        fprintf (stderr, "# (0x%x) Received message version=0x%x "
                        "(implemented version=0x%x)\n", 
                        (int) pthread_self (), msg->version, RPU_MSG_VERSION);
        return NULL;
    }
    
    if (msg->type != msgType) {
        fprintf (stderr, "# (0x%x) Was expecting message type=%d, received "
                    "message type=%d.\n", (int) pthread_self (), msgType, msg->type);
        return NULL;
    }

    return &msg->data;
}

static int RPU_SetSocketOptions (int s)
{
    struct timeval 	tv;
    int			err = 0;
    int			fReuseAddr = 1;
    int			fKeepAlive = 1;
    
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    
#if __APPLE__
    /* and possibly other platforms other than linux */
    err = setsockopt (s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof (tv));
    if (err != 0) {
        perror ("setsockopt(SO_RCVTIMEO)");
        assert (FALSE);
    }
    
    err = setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv));
    if (err != 0) {
        perror ("setsockopt(SO_SNDTIMEO)");
        assert (FALSE);
    }

#endif /* __APPLE__ */

    err = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &fReuseAddr, sizeof (fReuseAddr));
    if (err != 0) {
        perror ("setsockopt(SO_REUSEADDR)");
        assert (FALSE);
    }

    err = setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, &fKeepAlive, sizeof (fKeepAlive));
    if (err != 0) {
        perror ("setsockopt(SO_KEEPALIVE)");
        assert (FALSE);
    }

    return err;
}


static int RPU_CheckHostList (void)
{
    return 0;
}


static int RPU_AcceptConnection (struct sockaddr_in localAddr, struct sockaddr_in remoteAddr)
{
    /* check in authorized host list */
    if (RPU_CheckHostList ()) 
        return 1; 
    
    /* check with mask on IP addresses; eg. if local address is 192.168.0.1
        and the mask is set to 255.255.255.0, will accept connection from any
        address like 192.168.0.* and refuse everything else */
    if (RPU_DEBUG) {
        fprintf (stderr, ">> 0x%x\n", localAddr.sin_addr.s_addr);
        fprintf (stderr, ">> 0x%x\n", remoteAddr.sin_addr.s_addr);
        fprintf (stderr, ">> 0x%x\n", gRPU_IPMask);
    }
    if ((localAddr.sin_addr.s_addr & gRPU_IPMask) == (remoteAddr.sin_addr.s_addr & gRPU_IPMask)) 
        return 1;
    
    return 0;
}


static void RPU_PrintInfo (rpu_info *info)
{
    int i;
    outputf ("Processing units installed on RPU: %d\n", info->cProcunits);
    for (i = 0; i < info->cProcunits; i ++)
        PrintProcessingUnitInfo (&info->procunitInfo[i]);
}


static int Slave_DoJob (int toSocket, rpu_job *job)
{
    pu_task 	*pt;
    rpu_jobtask *pjt = (rpu_jobtask *) &job->data;
    int		i, j, cJobTasks = job->cJobTasks;
    int		err = 0;

    Slave_UpdateStatus ();

    if (RPU_DEBUG) fprintf (stderr, "Received job with %d task(s).\n", cJobTasks);
    
    if (RPU_DEBUG_PACK > 1) {
        fprintf (stderr, "Received job (len=%d):\n", job->len);
        RPU_DumpData ( (char *) job, job->len);
    }
    
    for (i = 0, j = 0; (i < cJobTasks || j < cJobTasks) && err == 0; ) {
        
        pthread_mutex_lock (&mutexTaskListAccess);
        
        while (i < cJobTasks && !TaskEngine_Full () && !fInterrupt && err == 0) {
            /* create tasks from jobtasks into taskList */
            pt = RPU_UnpackJobToTask (pjt, FALSE);
            if (pt != NULL) {
                switch (pt->type) {
                    case pu_task_rollout: gSlaveStats.rollout.rcvd ++; break;
                    case pu_task_eval: gSlaveStats.eval.rcvd ++; break;
                    default: assert ( FALSE ); break;
                }
                Slave_UpdateStatus ();
                if (RPU_DEBUG) {
                    fprintf (stderr, "Created new task (id=%d, org_id=%d) from job:\n", 
                                pt->task_id.taskId, pt->task_id.src_taskId);
                    PrintTaskList ();
                }
                ((char *) pjt) += pjt->len;
                i ++;
            }
            else {
                fprintf (stderr, "*** Could not understand job.\n");
                err = -1;
                break;
            }
        }
        
        pthread_mutex_unlock (&mutexTaskListAccess);
    
        /* retrieve and send back all results from completed tasks */
        while ((pt = TaskEngine_GetCompletedTask ()) && !fInterrupt && err == 0) {
            /* pack task into jobtask */
            rpu_jobtask *pjt;
            
            if (RPU_DEBUG) fprintf (stderr, "Completed one task!\n");
                                
            if (RPU_DEBUG > 1 && pt != NULL) {
                switch (pt->type) {
                    case pu_task_rollout: {
                            float *pf = (float *) pt->taskdata.rollout.aar;
                            fprintf (stderr, "<< %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %d\n", 
                                pf[0], pf[1], pf[2], pf[3], pf[4], pf[5], 
                                pt->taskdata.rollout.seed);
                        }
                    default:
                        assert( FALSE );
                        break;
                }
            }
            
            pjt = RPU_PackTaskToJobTask (pt);
            
            assert (pjt != NULL);   
            if (pjt == NULL) {
                err = -1;
            }
            else {
                /* embed jobtask into a message */
                rpu_message *msg = RPU_MakeMessage (mmTaskResult, pjt, pjt->len);
                assert (msg != NULL);
                if (msg == NULL) {
                    err = -1;
                }
                else {
                    /* send message back to master */
                    err = RPU_SendMessage (toSocket, msg, &fInterrupt);
                    switch (pt->type) {
                        case pu_task_rollout: gSlaveStats.rollout.sent ++; break;
                        case pu_task_eval: gSlaveStats.eval.sent ++; break;
                        default: assert( FALSE ); break;
                    }
                    Slave_UpdateStatus ();
                    free ((char *) msg);
                }
                free ((char *) pjt);
            }
            FreeTask (pt);
            j ++;
        }
        
        if( fInterrupt )
            break;
            
    } /* for (...) */
    
    Slave_UpdateStatus ();

    return err;
}


static int Slave_GetInfo (int toSocket)
{
    int 	i, err;
    int 	cProcunits = GetProcessingUnitsCount ();
    int		len = sizeof (rpu_info) + sizeof (procunit) * cProcunits;
    rpu_info 	*info = malloc (len);
    procunit	*ppu = gpulist;
    rpu_message	*msg;
    
    assert (info != NULL);
    
    /* copy individual local processing units info */
    info->cProcunits = cProcunits;
    for (i = 0; i < cProcunits && ppu != NULL; i++) {
        memcpy (&info->procunitInfo[i], ppu, sizeof (procunit));
        ppu = ppu->next;
    }
    
    /* make a mmInfo message with this info */
    msg = RPU_MakeMessage (mmInfo, info, len);
    assert (msg != NULL);
    
    if (msg != NULL) {
        /* send the info message back */
        err = RPU_SendMessage (toSocket, msg, &fInterrupt);
        free ((char *) msg);
    }
    
    return err;
}

extern void Slave_UpdateStatus (void)
{
    int cInTaskList = GetTaskCount (0, 0);
    int cTodo = GetTaskCount (pu_task_todo, 0);
    int cInProgress = GetTaskCount (pu_task_inprogress, 0);
    int cDone = GetTaskCount (pu_task_done, 0);
    
    if (gSlaveStatus != rpu_stat_waiting) {
        if (cInTaskList == 0) gSlaveStatus = rpu_stat_idle;
        else if (cInProgress > 0) gSlaveStatus = rpu_stat_processing;
    }

    outputf ("%4d (%4d/%4d/%4d)  "
             "%4d (%4d/%4d/%4d)  "
             "%4d (%4d/%4d/%4d)  "
             "%-16s  "
             "\r",
             
             cInTaskList,
             cTodo,
             cInProgress, 
             cDone,
             
             gSlaveStats.rollout.done,
             gSlaveStats.rollout.rcvd,
             gSlaveStats.rollout.sent,
             gSlaveStats.rollout.failed,
             
             gSlaveStats.eval.done,
             gSlaveStats.eval.rcvd,
             gSlaveStats.eval.sent,
             gSlaveStats.eval.failed,
             
             asSlaveStatus[gSlaveStatus]
            );
            
    fflush( stdout );
}


static void Slave_PrintStatus (void)
{
    outputf ("Tasks (todo/prog/done)  "
             "Rollouts (rcvd/sent/fail)  "
             "Evals (rcvd/sent/fail)  "
             "Status  "
             "\n");
    Slave_UpdateStatus ();
}

/* listen for incoming connections from MASTERS, create tasks
    with incoming tasks, assign the tasks to available 
    processing units, and return completed tasks to their masters.
   Its counterpart is the Thread_RemoteProcessingUnit() function,
   executed in a RPU-specific thread on the master host.
*/

/* local host = SLAVE, acts as a server (listen for incoming
                            master connections, receives messages
                            and responds to them)
    remote host = MASTER, acts as a client (connects to our local
                            host, sends messages/requests to slave,
                             and expects responses)
*/

static void Slave (void)
{
    int	done = FALSE;
    int	listenSocket;
    
    if (RPU_DEBUG) fprintf (stderr, "RPU Local Half started.\n");
    
    bzero (&gSlaveStats, sizeof (gSlaveStats));
    
    gSlaveStatus = rpu_stat_na;
    Slave_PrintStatus ();
    Slave_UpdateStatus ();
    
    
    /* create socket */
    listenSocket = socket (AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        fprintf (stderr, "*** RPU could not create slave socket (err=%d).\n", 
            errno);
        perror ("socket_create");
    }
    else {
        /* bind local address to socket */
        struct sockaddr_in listenAddress;
        bzero((char *) &listenAddress, sizeof (struct sockaddr_in));
        listenAddress.sin_family= AF_INET;
        listenAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        listenAddress.sin_port = htons(gRPU_TCPPort);
        RPU_SetSocketOptions (listenSocket);
	if (bind (listenSocket,  (struct sockaddr *) &listenAddress, sizeof(listenAddress)) < 0) {
            fprintf (stderr, "*** RPU could not bind slave socket address(err=%d).\n", errno);
            perror("socket_bind");
        }
        else {

            while (!fInterrupt) {
            
                TaskEngine_Init ();
    
                /* listen for incoming connect from master */
                gSlaveStatus = rpu_stat_waiting;
                Slave_UpdateStatus ();
                if (listen (listenSocket, 8) < 0) {
                    fprintf (stderr, "*** RPU could not listen to master socket (err=%d).\n", errno);
                    perror ("listen");
                }
                else {
                    /* incoming connection: accept it */
                    struct sockaddr_in masterAddress;
                    struct sockaddr_in slaveAddress;
                    int masterAddressLen = sizeof (masterAddress);
                    int slaveAddressLen = sizeof (slaveAddress);
                    
                    int rpuSocket = accept (listenSocket, (struct sockaddr *) &masterAddress, 
                                                &masterAddressLen);
                    getsockname (rpuSocket, (struct sockaddr *) &slaveAddress, &slaveAddressLen);
                    
                    if (rpuSocket < 0) {
                        fprintf (stderr, "*** RPU could not accept connection from master (err=%d).\n", errno);
                        perror ("accept");
                    }
                    else if (!RPU_AcceptConnection (slaveAddress, masterAddress)) {
                        fprintf (stderr, "*** RPU refused connection on local IP address "
                                            "%s from remote IP address %s.\n", 
                                            inet_ntoa(slaveAddress.sin_addr), 
                                            inet_ntoa(masterAddress.sin_addr));
                    }
                    else {
                        /* RPU: local half connected with remote host */
                        if (RPU_DEBUG) fprintf (stderr, "RPU connected.\n");
                        
                        RPU_SetSocketOptions (rpuSocket);
                        
                        gSlaveStatus = rpu_stat_idle;
                        Slave_UpdateStatus ();
                        
                        done = FALSE;
                        while (!done && !fInterrupt) {
                            /* receive messages from master */
                            int			err = 0;
                            rpu_message 	*msg = RPU_ReceiveMessage (rpuSocket, &fInterrupt, 0);
                            
                            if (msg != NULL) {
                                if (RPU_DEBUG) fprintf (stderr, "Received message type=%d.\n", msg->type);
                                if (RPU_DEBUG_PACK) RPU_DumpData ((char *) msg, msg->len);
                                switch (msg->type) {
                                    case mmGetInfo:
                                        if (RPU_DEBUG) fprintf (stderr, "Received GetInfo msg\n");
                                        err = Slave_GetInfo (rpuSocket);
                                        break;                    
                                    case mmDoJob:
                                        err = Slave_DoJob (rpuSocket, 
                                                    (rpu_job *) RPU_GetMessageData (msg, mmDoJob));
                                        break;                    
                                    case mmMET:
                                        break;                    
                                    case mmNeuralNet:
                                        break;                    
                                    case mmClose:
                                        fprintf (stderr, "*** Connection closed by master.\n");
                                        done = TRUE;
                                        break; 
                                    default:
                                        fprintf (stderr, "*** Received unknown message type=%d.\n", msg->type);
                                        err = -1;
                                        break;
                                } /* switch */
                                if (err != 0) {
                                    fprintf (stderr, "*** Failed to answer message (err=%d).\n", err);
                                    done = TRUE;
                                }
                                free ((char *) msg);
                            }
                            else {
                                //fprintf (stderr, "\nConnection closed by master.\n");
                                done = TRUE;
                            }
                            
                        } /* while (!done && !fInterrupt) */
                        
                        if (fInterrupt) {
                        
                        }
                        
                        /* close connection */
                        shutdown (rpuSocket, SHUT_RDWR);
                        close (rpuSocket);
                        
                    } /* accept() */
                    
                } /* listen() */
                
                TaskEngine_Shutdown ();
                                
            } /* while (!fInterrupt) */
            
        } /* bind() */
        
        /* close connection listening */
        shutdown (listenSocket, SHUT_RDWR);
        close (listenSocket);
        
        gSlaveStatus = rpu_stat_na;
        Slave_UpdateStatus ();
        
        outputf ("\n\n");
        
    } /* socket() */

    TaskEngine_Shutdown ();

    if (RPU_DEBUG) fprintf (stderr, "RPU Local Half stopped.\n");    
}

static int WaitForCondition (pthread_cond_t *cond, pthread_mutex_t *mutex, char *s, int *step)
{
    int			err;
    struct timespec   	ts;
    struct timeval    	tp;    
    
    /* compute wait time-limit */
    gettimeofday(&tp, NULL);                    
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += WAIT_TIME_SECONDS;
    
    /* wait till condition met or time limit exceeded */
    err = pthread_cond_timedwait (cond, mutex, &ts);
    
    if (step != NULL) {
        if (*step == 0) fprintf (stderr, s);
        if (*step > 0) fprintf (stderr, ".");
        fflush (stderr);
        ++ *step;
    }
                            
    if (PU_DEBUG > 1 && err == ETIMEDOUT) fprintf (stderr, "[Timeout]\n");
    
    return 0;
}



static void Thread_RPU_Loop (procunit *ppu, int rpuSocket)
{
    int err = 0;
    
    /* look for tasks to perform until we are stopped */
    while (err == 0 && !ppu->info.remote.fStop) {
        
        int 	cJobTasks = 0;
        pu_task	**apJobTasks = NULL;    
        
        ChangeProcessingUnitStatus (ppu, pu_stat_ready);
        
        /* wait for new tasks assigned to us (we receive the
            unlocking signal from RunTaskOnRemoteProcessingUnit()
            when it assigns us a task to perform) */
        pthread_mutex_lock (&ppu->info.remote.mutexTasksAvailable);
        if (!ppu->info.remote.fTasksAvailable) {
            while (!ppu->info.remote.fTasksAvailable && !ppu->info.remote.fInterrupt) 
                WaitForCondition (&ppu->info.remote.condTasksAvailable, 
                                    &ppu->info.remote.mutexTasksAvailable, NULL, NULL);
            if (RPU_DEBUG) {
                if (ppu->info.remote.fTasksAvailable)
                    fprintf (stderr, "\n# (0x%x) RPU woken up to perform assigned "
                        "todo tasks.\n", (int) pthread_self ());
                if (ppu->info.remote.fInterrupt) {
                    fprintf (stderr, "\n# (0x%x) RPU interrupted.\n", 
                             (int) pthread_self ());
                }
            }
            ppu->info.remote.fInterrupt = FALSE;
        }
        ppu->info.remote.fTasksAvailable = FALSE;
        pthread_mutex_unlock (&ppu->info.remote.mutexTasksAvailable);
        

        if (ppu->info.remote.fStop) 
            break;
                
        
        /* find all tasks that have been assigned to us */
        pthread_mutex_lock (&mutexTaskListAccess);
        cJobTasks = MakeAssignedTasksList (ppu, pu_task_todo, 
                                        &apJobTasks, ppu->maxTasks);
       
        if (cJobTasks == 0) {
            pthread_mutex_unlock (&mutexTaskListAccess);
        }
        else {
            /* pack all assigned tasks into one job */
            int		i;
            rpu_job 	*pjob;

            for (i = 0; i < cJobTasks; i ++) {
                /* mark them "in progress" */
                apJobTasks[i]->status = pu_task_inprogress;
            }
    
            if (RPU_DEBUG) {
                fprintf (stderr, "Task list after RPU-assign:\n");
                PrintTaskList ();
            }

            pjob = RPU_PackTaskListToJob (apJobTasks, cJobTasks);
            assert (pjob != NULL); 
            
            pthread_mutex_unlock (&mutexTaskListAccess);

            ChangeProcessingUnitStatus (ppu, pu_stat_busy);
            
            if (pjob != NULL) {
                /* embed job into a message */
                rpu_message *msg = RPU_MakeMessage (mmDoJob, pjob, pjob->len);
                assert (msg != NULL);
                if (msg == NULL) {
                    err = -1;
                }
                else {
                    /* send message to remote half (slave) */
                    err = RPU_SendMessage (rpuSocket, msg, &ppu->info.remote.fInterrupt);
                    free ((char *) msg);
                }
                free ((char *) pjob);
            }
        
            /* wait for job results to come in till we have them all or 
                till we get interrupted or stopped */
                
            while (err == 0 && cJobTasks > 0 && !ppu->info.remote.fInterrupt) {
            
                rpu_message *msg;
                
                if (RPU_DEBUG) {
                    PrintTaskList ();
                    fprintf (stderr, "# (0x%x) Waiting for job result (%d task(s) pending).\n",
                                                (int) pthread_self (), cJobTasks);
                }
                
                msg = RPU_ReceiveMessage (rpuSocket, &ppu->info.remote.fInterrupt, 0);
                
                if (msg == NULL) {
                    if (RPU_DEBUG) fprintf (stderr, "# (0x%x) No result received for job.\n",
                                                (int) pthread_self ());
                    err = -1;
                }
                else {
                    /* read received message */
                    rpu_jobtask *pjt = RPU_GetMessageData (msg, mmTaskResult); 
                    
                    if (pjt == NULL) {
                        err = -1;
                    }
                    else {
                        pu_task *pt;
                        /* extract task results from received message */
                        if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU received results.\n",
                                    (int) pthread_self ());
                        
                        /* note: pt is a "detached" task, not yet in the taskList */
                        pt = RPU_UnpackJobToTask (&msg->data.taskresult, TRUE);
                        if (pt == NULL) {
                            fprintf (stderr, "# (0x%x) RPU could not unpack task result.\n",
                                    (int) pthread_self ());
                            err = -1;
                        }
                        else {
                            pu_task *oldpt;
                            if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU received task id=%d results.\n",
                                        (int) pthread_self (), pt->task_id.src_taskId);
                            pthread_mutex_lock (&mutexTaskListAccess);
                            /* store received task with results back to master task list */
                            /* remove original task... */
                            oldpt = FindTask (0, 0, pt->task_id.src_taskId, 0);
                            if (oldpt == NULL) {
                                /* we don't know what these results are for; probably from
                                    a previously failed/interrupted connection; just ignore
                                    and discard */
                                if (RPU_DEBUG) fprintf (stderr, "### Discarded results for task id=%d.\n",
                                                            pt->task_id.src_taskId);
                                FreeTask (pt);
                            }
                            else {
                                pt->procunit_id = oldpt->procunit_id;
                                pt->timeCreated = oldpt->timeCreated;
                                pt->timeStarted = oldpt->timeStarted;
                                if (RPU_DEBUG) fprintf (stderr, "# [removed org task]\n");
                                FreeTask (oldpt);
                                /* ...and replace with received task that contains the results:
                                    merely replace the received task id (just assigned by 
                                    RPU_UnpackJobToTask()) by the original task id */
                                pt->task_id.taskId = pt->task_id.src_taskId;
                                AttachTask (pt);
                                MarkTaskDone (pt, ppu);
                                if (RPU_DEBUG) { 
                                    fprintf (stderr, "# (0x%x) RPU results for task id=%d "
                                                "integrated back to lisk:\n",
                                                (int) pthread_self (), pt->task_id.src_taskId);
                                    PrintTaskList ();
                                    fprintf (stderr, "# (0x%x) RPU end of ilst.\n",
                                                (int) pthread_self ());
                                }
                                cJobTasks --;
                            }
                            pthread_mutex_unlock (&mutexTaskListAccess);  
                        }
                        
                    } /* if (pjt != NULL) */
                    
                } /* if (msg != NULL) */
                
            } /* while (err == 0 && cJobTasks > 0 & !ppu->info.remote.fInterrupt) */

            if (err != 0) {
                fprintf (stderr, "*** Remote processing unit id=%d error, deactivating.\n", 
                            ppu->procunit_id);
                CancelProcessingUnitTasks (ppu);
                ppu->info.remote.fInterrupt = TRUE;
                ppu->info.remote.fStop = TRUE;
            }
                                    
        } /* if (cJobTasks != 0) */
                            
        if (apJobTasks != NULL) {
            free ((char *) apJobTasks);
            if (PU_DEBUG_FREE) fprintf (stderr, "free(): apJobTasks\n");
        }
            
    } /* while (err == 0 && !ppu->info.remote.fStop) */
}


/*  Thread_RemoteProcessingUnit ()

    this function is called at master remote procunit (rpu) creation
    time and lasts for the duration of the local half of the rpu; 
    it is responsible for managing the connection with the
    remote (slave) half of the rpu.
    Its counterpart is the Slave() function, executed on slave hosts.
    
    local host = MASTER, acts as a client (connects to remote slave
                            hosts and sends messages)
    remote hosts = SLAVES, act as servers
*/

extern void * Thread_RemoteProcessingUnit (void *data)
{
    int 	rpuSocket;
    procunit	*ppu = (procunit *) data;
    int		err = 0;
    
    if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU Local Half started.\n", (int) pthread_self ());
    
    while (!ppu->info.remote.fStop) {
    
        ppu->status = pu_stat_connecting;
        ppu->maxTasks = 0;
        
        /* create socket */
        rpuSocket = socket (AF_INET, SOCK_STREAM, 0);
        if (rpuSocket == -1) {
            fprintf (stderr, "# (0x%x) RPU could not create socket (err=%d).\n", 
                (int) pthread_self (), errno);
            perror ("socket_create");
            ppu->info.remote.fStop = TRUE;
        }
        else {
            /* connect to remote host */
            struct sockaddr_in remoteAddress;
            bzero((char *) &remoteAddress, sizeof (struct sockaddr_in));
            remoteAddress.sin_family= AF_INET;
            remoteAddress.sin_addr.s_addr = ppu->info.remote.inAddress.s_addr;
            remoteAddress.sin_port = htons(gRPU_TCPPort);
    
            if (connect (rpuSocket, (const struct sockaddr *) &remoteAddress, sizeof(remoteAddress)) < 0) {
                fprintf (stderr, "# (0x%x) RPU could not connect socket (err=%d).\n", 
                    (int) pthread_self (), errno);
                perror ("connect");
                ppu->info.remote.fStop = TRUE;
            }
            else {
                /* RPU: local half connected with remote host */
                if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU connected.\n", (int) pthread_self ());
                
                RPU_SetSocketOptions (rpuSocket);
                
                /* handshake / retrieve info from remote host */
                {
                    rpu_getinfo msgGetInfo = {};
                    rpu_message *msg = RPU_MakeMessage (mmGetInfo, &msgGetInfo, sizeof (msgGetInfo));
                    assert (msg != NULL);
                    if (RPU_DEBUG_PACK) RPU_DumpData ((char *) msg, msg->len);
                    err = RPU_SendMessage (rpuSocket, msg, &ppu->info.remote.fInterrupt);
                    if (err != 0) {
                        fprintf (stderr, "# (0x%x) RPU couldn't send GetInfo msg to slave host.\n", 
                                        (int) pthread_self ());
                    }
                    else {
                        free ((char *) msg);
                        msg = RPU_ReceiveMessage (rpuSocket, &ppu->info.remote.fInterrupt, RPU_TIMEOUT);
                        if (msg == NULL) {
                            fprintf (stderr, "# (0x%x) RPU couldn't receive Info msg "
                                            "from slave host.\n", (int) pthread_self ());
                            err = -1;
                        }
                        else {
                            /* read info */
                            rpu_info *pinfo = RPU_GetMessageData (msg, mmInfo);
                            if (pinfo == NULL) {
                                fprintf (stderr, "# (0x%x) RPU couldn't read Info msg.\n", (int) pthread_self ());
                                err = -1;
                            }
                            else {
                                /* retrieve interesting data; calculate whole local RPU queue
                                    size according to available procunits queues on slave */
                                int i;
                                RPU_PrintInfo (pinfo);
                                ppu->maxTasks = 0;
                                for (i = 0; i < pinfo->cProcunits; i ++)
                                    ppu->maxTasks += pinfo->procunitInfo[i].maxTasks;
                            }
                            free ((char *) msg);
                        }
                    }
                    if (err != 0) ppu->info.remote.fStop = TRUE;
                } /* handshake */
                
                if (err == 0) {
                    /* look for tasks to send to remote slave rpu till we get stopped */
                    Thread_RPU_Loop (ppu, rpuSocket);
                    
                    /* send close message to slave */
                    if (ppu->info.remote.fInterrupt) {
                        rpu_close msgClose = {};
                        rpu_message *msg = RPU_MakeMessage (mmClose, &msgClose, sizeof (msgClose));
                        assert (msg != NULL);
                        /* ignore errors when sending close msg */
                        (void) RPU_SendMessage (rpuSocket, msg, &ppu->info.remote.fInterrupt);
                    }
                }
                
                CancelProcessingUnitTasks (ppu);
                ppu->info.remote.fInterrupt = FALSE;
                
                ChangeProcessingUnitStatus (ppu, !ppu->info.remote.fStop ? pu_stat_ready
                                                    : pu_stat_deactivated);
                                
                shutdown (rpuSocket, SHUT_RDWR);
                
            } /* connect() */
            
            close (rpuSocket);
            
        } /* socket() */
        
     } /* while (!ppu->info.remote.fStop) */
     
     ppu->info.remote.fStop = FALSE;

    ChangeProcessingUnitStatus (ppu, pu_stat_deactivated);
    
    if (RPU_DEBUG) fprintf (stderr, "# (0x%x) RPU Local Half terminated.\n", (int) pthread_self ());
    
    return 0;
}


extern void CommandProcunitsMaster ( char *sz ) 
{
    gProcunitsMode = pu_mode_master;
    outputl ( "Host set to MASTER mode.\n" );
}


extern void CommandProcunitsSlave ( char *sz ) 
{
    gProcunitsMode = pu_mode_slave;
    outputf ( "Host set to SLAVE mode.\n"
                "Hit ^C to kill pending tasks and return to MASTER mode.\n\n");
    outputf ("Available processing units:\n");
    PrintProcessingUnitList ();
    outputf ("\n");
    
    Slave ();
    
    outputf ("\n\nHost set back to MASTER mode.\n\n");
}


extern void CommandProcunitsStart ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type 'pu info' to get full list" );
        return;
    }
    
    ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
    
    if (ppu == NULL) {
        outputf ( "No processing unit found with specified id (%d).\n"
                    "Type 'pu info' to get full list\n", procunit_id );
        return;
     } 
    
    if (StartProcessingUnit (ppu, TRUE) < 0)
        outputf ( "Processing unit id=%d could not be started.\n", procunit_id);
    else
        outputf ( "Processing unit id=%d started.\n", procunit_id);    
}


extern void CommandProcunitsStop ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type 'pu info' to get full list" );
        return;
    }
    
    ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
    
    if (ppu == NULL) {
        outputf ( "No processing unit found with specified id (%d).\n"
                    "Type 'pu info' to get full list\n", procunit_id );
        return;
     } 
    
    if (StopProcessingUnit (ppu) < 0)
        outputf ( "Processing unit id=%d could not be stopped.\n", procunit_id);
    else
        outputf ( "Processing unit id=%d stopped.\n", procunit_id);    
}


extern void CommandProcunitsRemove ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type 'pu info' to get full list" );
        return;
    }
    
    ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
    
    if (ppu == NULL) {
        outputf ( "No processing unit found with specified id (%d).\n"
                    "Type 'pu info' to get full list\n", procunit_id );
        return;
     } 
    
    DestroyProcessingUnit (procunit_id);
    
    outputf ( "Processing unit id=%d removed.\n", procunit_id);
}


extern void CommandProcunitsAddLocal( char *sz ) 
{
    int n = 1;
    int done = 0;
    
    if( *sz ) {
        sscanf (sz, "%d", &n);
    }

    while (n > 0) {
        procunit *ppu = CreateProcessingUnit (pu_type_local, pu_stat_ready, pu_task_info|pu_task_rollout, 1);
        if (ppu == NULL) {
            outputf ("Could not add local processing unit.\n");
            break;
        }
        else done ++;
        n --;
    }
    
    outputf ( "%d local processing unit(s) added.\n", done);
}


extern void CommandProcunitsAddRemote( char *sz ) 
{    
    struct in_addr	inAddress;

    if (sz == NULL) {
        outputf ( "Remote processing unit address not specified.\n");
        return;
    }
    
    if (inet_aton (sz, &inAddress) == 0) {
        outputf ( "Remote processing unit address invalid.\n");
    }
    else {
        procunit *ppu = CreateRemoteProcessingUnit (&inAddress, TRUE);
        if (ppu == NULL)
            outputf ("Remote processing unit could not be created.\n");
        else {
            outputf ("Remote processing unit id=%d created.\n", ppu->procunit_id);
            if (ppu->status == pu_stat_deactivated)
                outputf ("*** Warning: Remote processing unit id=%d could "
                        "not be started.\n", ppu->procunit_id);
        }
    }
    
}



extern void CommandShowProcunitsInfo ( char *sz ) 
{
    int nProcs = GetProcessorCount ();
    int nProcunits = 0;
    int nLocal = 0;
    int nRemote = 0;
    procunit *ppu = gpulist;
    
    while (ppu != NULL) {
        switch (ppu->type) {
            case pu_type_local:
                nProcunits ++;
                nLocal ++;
                break;
            case pu_type_remote:
                nProcunits ++;
                nRemote ++;
                break;
            default:
                assert( FALSE );
                break;
        }
        ppu = ppu->next;
    }
    
    outputf ( "Processing units info:\n"
                "  Installed processors: %d\n"
                "  Processing units: %d (%d local, %d remote)\n"
                "Processing units details:\n",
                nProcs, nProcunits, nLocal, nRemote);
                
    PrintProcessingUnitList ();
}



extern void CommandShowProcunitsStats ( char *sz ) 
{
    int procunit_id = 0;
    
    if (sz != NULL) {
        procunit *ppu;
        sscanf (sz, "%d", &procunit_id);
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        
        if (ppu == NULL) {
            outputf ( "No processing unit found with specified id (%d).\n"
                        "Type 'pu info' to get full list\n", procunit_id );
            return;
        } 
    }
            
    PrintProcessingUnitStats (procunit_id);
}


extern void CommandShowProcunitsRemoteMask ( char *sz ) 
{
    struct in_addr inMask;
    
    inMask.s_addr = gRPU_IPMask;
    outputf ("Processing units remote IP mask: %s\n", inet_ntoa (inMask));
}


extern void CommandShowProcunitsRemotePort ( char *sz ) 
{
    outputf ("Processing units remote TCP port: %d\n", gRPU_TCPPort);
}


extern void CommandSetProcunitsEnabled ( char *sz ) 
{
}


extern void CommandSetProcunitsRemoteMode ( char *sz ) 
{
    outputf ( "Remote processing units not implemented yet.\n");
}


extern void CommandSetProcunitsRemoteMask ( char *sz ) 
{
    struct in_addr	inMask;

    if (sz == NULL) {
        outputf ( "IP mask not specified.\n");
        return;
    }
    
    if (inet_aton (sz, &inMask) == 0) {
        outputf ( "IP mask invalid.\n");
    }
    else {
        gRPU_IPMask = inMask.s_addr;
    }
}

extern void CommandSetProcunitsRemotePort ( char *sz ) 
{
    int	port = 0;
    
    if (*sz) sscanf (sz, "%d", &port);
    
    if (port <= 0 || port >= 65536)
        outputf ("Invalid TCP port.\n");
    else
        gRPU_TCPPort = port;
}


extern void CommandSetProcunitsRemoteQueue ( char *sz ) 
{
    int		procunit_id = 0;
    int		queueSize = 0;
    procunit 	*ppu;
    
    if (*sz) sscanf (sz, "%d %d", &procunit_id, &queueSize);
    
    ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        
    if (ppu == NULL) {
        outputf ( "No processing unit found with specified id (%d).\n"
                    "Type 'pu info' to get full list\n", procunit_id );
        return;
    } 
    
    if (ppu->type == pu_type_local) {
        outputf ( "Local processing units queue sizes can't be modified.\n" );
        return;
    }
    
    if (queueSize < 1)
        outputf ("Invalid queue size.\n");
    else
        ppu->maxTasks = queueSize;
}

