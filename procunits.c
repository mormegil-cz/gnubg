/*
 * procunits.c
 *
 * by Olivier Baur <olivier.baur@noos.fr>, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
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
 *
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if PROCESSING_UNITS

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
#include <netdb.h>
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

#if WIN32
#include <windows.h>
#endif

#include <signal.h>
#include <stddef.h>
#include <fcntl.h>

#if USE_GTK
#include "gtkgame.h"
#endif

#include "backgammon.h"
#include "procunits.h"
#include "threadglobals.h"

/* set to activate debugging */
#define RPU_DEBUG 0
#define RPU_DEBUG_PACK 0
#define RPU_DEBUG_NOTIF 0
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

#define DEFAULT_JOBSIZE (100 * 1024)


static int 		gfProcessingUnitsInitialised = FALSE;
static pthread_t	gMainThread;

/* TCP port and IP address mask for remote processing */
static int 		gRPU_IPMask = RPU_DEFAULT_IPMASK;
static int 		gRPU_TCPPort = RPU_DEFAULT_TCPPORT;
static int 		gRPU_SlaveTCPPort = RPU_DEFAULT_TCPPORT;
static int 		gRPU_MasterTCPPort = RPU_DEFAULT_TCPPORT;

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

/* this mutex to guarantee exclusive access to the procunit list */
pthread_mutex_t		mutexProcunitAccess;

/* this mutex for debugging purposes only */
pthread_mutex_t		mutexCPUAccess;


DECLARE_THREADSTATICGLOBAL (char *, gpPackedData, NULL);
DECLARE_THREADSTATICGLOBAL (int, gPackedDataPos, 0);
DECLARE_THREADSTATICGLOBAL (int, gPackedDataSize, 0);


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

#if USE_GTK
static void GTK_TouchProcunit (procunit *ppu);
#endif

static const char asProcunitType[][10] = { "None", "Local", "Remote" };
static const char asProcunitStatus[][14] = { "None", "N/A", "Ready", "Busy", 
                        "Stopped", "Connecting" };
static const char asTaskStatus[][12] = { "None", "To do", "In Progress", "Done" };
static const char asSlaveStatus[][20] = { "N/A", "Waiting for cx...", "Idle", 
                        "Processing..." };

/* mode of the local host: master or slave */
static 			pu_mode gProcunitsMode = pu_mode_master;

/* state of the local host when running in slave mode */
typedef enum { rpu_stat_na, rpu_stat_waiting, rpu_stat_idle, rpu_stat_processing } rpu_slavestatus;
static 			rpu_slavestatus gSlaveStatus = rpu_stat_na;

/* stats for the local host when running in slave mode */
rpu_slavestats 		gSlaveStats;

/* slave -> master notifications parameters */
typedef enum { rpu_method_none = 0, rpu_method_broadcast, rpu_method_host } rpu_notif_method;
static char		szNotifMethod[3][10] = { "none", "broadcast", "host" };
static rpu_notif_method	gRPU_SlaveNotifMethod = rpu_method_none;
static char		gRPU_SlaveNotifSpecificHost[MAX_HOSTNAME] = "";
int			gRPU_SlaveNotifDelay = 10; /* seconds */
int			gRPU_MasterNotifListen = FALSE;
int			gRPU_QueueSize = 1;

/* variables for slave availability notification */
int			gfRPU_Notification = FALSE;
int			gfRPU_NotificationBroadcast = FALSE;
struct sockaddr_in	ginRPU_NotificationAddr;
pthread_mutex_t		mutexRPU_Notification;
pthread_cond_t		condRPU_Notification;

typedef struct {
    struct sockaddr_in	inAddress;
    int			port;
    char		hostName[MAX_HOSTNAME];
} rpuNotificationMsg;


#if USE_GTK
static GtkWidget	*gpwSlaveWindow;
static GtkWidget	*gpwLabel_Status;
static GtkTextBuffer 	*gptbSlaveText;
static GtkWidget	*gpwSlave_Button_Stop;
static GtkWidget	*gpwSlave_Button_Start;
static GtkWidget	*gpwSlave_Button_Options;
static GtkWidget	*pwSlave_Label_Job;
static GtkWidget	*pwSlave_Table_Tasks;
static GtkWidget	*pwSlave_Label_Tasks[4][5];

static GtkWidget	*gpwMasterWindow;
static GtkWidget	*gpwTree;
static GtkWidget    	*gpwMaster_Button_AddLocal;
static GtkWidget    	*gpwMaster_Button_AddRemote;
static GtkWidget    	*gpwMaster_Button_Remove;
static GtkWidget    	*gpwMaster_Button_Start;
static GtkWidget    	*gpwMaster_Button_Stop;
static GtkWidget    	*gpwMaster_Button_Info;
static GtkWidget    	*gpwMaster_Button_Stats;
static GtkWidget    	*gpwMaster_Button_Queue;
static GtkWidget    	*gpwMaster_Button_Options;

static GtkWidget	*gpwOptionsWindow;

static GtkListStore 	*gplsProcunits = NULL;


#define GTK_YIELDTIME 	{if (IsMainThread () && fX) while (gtk_events_pending ()) gtk_main_iteration ();}

#else
static void		*gpwSlaveWindow = NULL;	/* for testing whether there's a slave window 
                                                    without first checking for USE_GTK */
#define GTK_YIELDTIME
                                                    
#endif



static int GetProcessorCount (void)
{
    int cProcessors = 1;
    int i;
    
    #if __APPLE__
        #include <CoreServices/CoreServices.h>
        if (!MPLibraryIsLoaded ())
            outputerrf ("*** Could not load Mac OS X Multiprocessing Library.\n");
        else
            cProcessors = MPProcessors ();
            
    #elif WIN32

        SYSTEM_INFO siSysInfo;
        GetSystemInfo( &siSysInfo );
        cProcessors = siSysInfo.dwNumberOfProcessors;
        
        ;

    #else

        /* try _NPROCESSORS_ONLN from sysconf */

        if ( ( i = sysconf( _SC_NPROCESSORS_ONLN ) ) > 0 )
          cProcessors = i;

        /* other ideas? */



    #endif

    
    if (PU_DEBUG) outputerrf ("%d processor(s) found.\n", cProcessors);
    
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
    int 		i;
    int 		nProcs = GetProcessorCount ();
    struct sigaction 	act;
    pthread_mutexattr_t	mutexAttrRecursive;
    
    
    if (gfProcessingUnitsInitialised) return;
    gfProcessingUnitsInitialised = TRUE;
    gMainThread = pthread_self ();
    
    /* ignore process-wide SIGPIPE signals that can be 
        raised by a send() call to a remote host that has 
        closed its connection, in function RPU_SendMessage() */
    bzero (&act, sizeof (act));
    act.sa_handler = SIG_IGN;
    if (sigaction (SIGPIPE, &act, NULL) < 0) {
        outputerr ("sigaction");
        assert (FALSE);
    }
        
    /* create inter-thread mutexes and conditions neeeded for
        multithreaded operation */
        
    pthread_mutexattr_init (&mutexAttrRecursive);
    pthread_mutexattr_settype (&mutexAttrRecursive, PTHREAD_MUTEX_RECURSIVE);
    
    pthread_mutex_init (&mutexTaskListAccess, NULL);
    pthread_mutex_init (&mutexResultsAvailable, NULL);
    pthread_cond_init (&condResultsAvailable, NULL);
    pthread_mutex_init (&mutexRPU_Notification, NULL);
    pthread_cond_init (&condRPU_Notification, NULL);
    pthread_mutex_init (&mutexProcunitAccess, &mutexAttrRecursive);
    pthread_mutex_init (&mutexCPUAccess, NULL);

    pthread_mutexattr_destroy (&mutexAttrRecursive);

#if USE_GTK
    if (fX && gplsProcunits == NULL) {
        /* create the list gplsProcunits; the gplsProcunits itself has only ONE column, which
            holds pointers to the installed procunits */
        gplsProcunits = gtk_list_store_new (1, G_TYPE_POINTER);                                                    
    }
#endif

    /* create local processing unit: each will execute one rollout thread;
       N should be set to the number of processors available on the host */

    for (i = 0; i < nProcs ; i++) {
        CreateProcessingUnit (pu_type_local, pu_stat_ready, 
                        pu_task_info|pu_task_rollout|pu_task_analysis, 1);
    }
    
    /* create remote processing units: according to the information returned
        by the gnubg remote processing units manager */
    
    
}


extern int IsMainThread (void)
{
    return (gMainThread == pthread_self ());
}


extern pu_mode GetProcessingUnitsMode (void)
{
    return gProcunitsMode;
}


static void SetProcessingUnitsMode (pu_mode mode)
{
    if (gProcunitsMode == mode) return;
    
    if (gProcunitsMode == pu_mode_slave) {
        fInterrupt = TRUE;
    }

    gProcunitsMode = mode;

    #if USE_GTK
        if (gpwSlaveWindow != NULL) {
            gtk_widget_set_sensitive (gpwSlave_Button_Stop, gProcunitsMode == pu_mode_slave);
            gtk_widget_set_sensitive (gpwSlave_Button_Start, gProcunitsMode == pu_mode_master);
            gtk_widget_set_sensitive (gpwSlave_Button_Options, gProcunitsMode == pu_mode_master);
            gtk_label_set_text (GTK_LABEL(gpwLabel_Status), "Status : n/a");    
        }
    #endif

}


static int GetProcessingUnitsCount (void)
{
    procunit 	*ppu;
    int		n = 0;
    
    pthread_mutex_lock (&mutexProcunitAccess);
    
    ppu = gpulist;
    while (ppu != NULL) {
        n ++;
        ppu = ppu->next;
    }
    
    pthread_mutex_unlock (&mutexProcunitAccess);

    return n;
}

static void GetProcessingUnitTasksString (procunit *ppu, char *sz)
{
    if (ppu->taskMask & pu_task_rollout) strcat (sz, "rollout ");
    if (ppu->taskMask & pu_task_eval) strcat (sz, "eval ");
    if (ppu->taskMask & pu_task_analysis) strcat (sz, "analysis ");
    if (sz[0] == 0) strcat (sz, "none");
}

static void GetProcessingUnitAddresssString (procunit *ppu, char *sz)
{
    if (ppu->type == pu_type_remote) 
        sprintf (sz, "%s:%d (%s)", inet_ntoa (ppu->info.remote.inAddress.sin_addr),
                                    ppu->info.remote.inAddress.sin_port,
                                    ppu->info.remote.hostName);
    else
        strcpy (sz, "n/a");
}


/* Creates and adds a new processing unit in the processing units list.
   Fills it in with passed arguments and default parameters.
   Returns a pointer to created procunit.
   
   Note: in the case of RPU's, doesn't START the RPU (no thread created)
*/
static procunit * CreateProcessingUnit (pu_type type, pu_status status, pu_task_type taskMask, int maxTasks)
{
    procunit **pppu = &gpulist;
    
    pthread_mutex_lock (&mutexProcunitAccess);

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
        ClearStats (&ppu->analysisStats);
        
        #if USE_GTK
        if (fX) {
            GtkTreeIter iter;
            if (!IsMainThread ()) gdk_threads_enter ();
            gtk_list_store_append (gplsProcunits, &iter);
            gtk_list_store_set (gplsProcunits, &iter, 0, ppu, -1);
            if (!IsMainThread ()) gdk_threads_leave ();
        }
        #endif

        pthread_mutex_init (&ppu->mutexStatusChanged, NULL);
        pthread_cond_init (&ppu->condStatusChanged, NULL);
    }

    pthread_mutex_unlock (&mutexProcunitAccess);

    return *pppu;
}


/* Create and START a remote processing unit */

static procunit * CreateRemoteProcessingUnit (char *szAddress, int fWait)
{

    pthread_mutex_lock (&mutexProcunitAccess);
    
    procunit 	*ppu = CreateProcessingUnit (pu_type_remote, pu_stat_connecting, pu_task_none, 0);
        /* create rpu in busy state so it can't be used for now (connecting);
            the task mask and queue size will be set during handshake with remote half of rpu */
    
    if (ppu != NULL) {
        bzero ((char *) &ppu->info.remote.inAddress, sizeof (ppu->info.remote.inAddress));
        strncpy (ppu->info.remote.hostName, szAddress, MAX_HOSTNAME);
        ppu->info.remote.fTasksAvailable = FALSE;
        pthread_mutex_init (&ppu->info.remote.mutexTasksAvailable, NULL);
        pthread_cond_init (&ppu->info.remote.condTasksAvailable, NULL);
    }
    
    pthread_mutex_unlock (&mutexProcunitAccess);
    
    if (ppu != NULL) StartProcessingUnit (ppu, fWait);
    
    return ppu;
}


static void DestroyProcessingUnit (int procunit_id)
{
    procunit **plpu;
    procunit *ppu;
        
    pthread_mutex_lock (&mutexProcunitAccess);
    
    plpu = &gpulist;
    ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);

    if (ppu != NULL) {
    
        StopProcessingUnit (ppu);
    
        /* remove ppu from list and free ppu */
        while (*plpu != NULL) {
            /*PrintProcessingUnitInfo (*plpu);*/
            if (*plpu == ppu) {
                #if USE_GTK
                if (fX) {
                    GtkTreeIter iter;      
                    if (!IsMainThread ()) gdk_threads_enter ();
                    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(gplsProcunits), &iter)) 
                        do {
                            procunit *ppu2;
                            gtk_tree_model_get (GTK_TREE_MODEL(gplsProcunits), &iter, 0, &ppu2, -1);
                            if (ppu2 == ppu) {
                                gtk_list_store_remove (gplsProcunits, &iter);
                                break;
                            }
                        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gplsProcunits), &iter));
                    if (!IsMainThread ()) gdk_threads_leave ();
                }
                #endif
                procunit *next = ppu->next;
                free ((char *) ppu);
                *plpu = next;
                break;
            }
            else
                plpu = &((*plpu)->next);
        }
    }
    
    pthread_mutex_unlock (&mutexProcunitAccess);
}



static void PrintProcessingUnitInfo (procunit *ppu)
{
    char asTasks[256] = "";
    char asAddress[256] = "n/a";
    
    pthread_mutex_lock (&mutexProcunitAccess);
    
    GetProcessingUnitTasksString (ppu, asTasks);
    GetProcessingUnitAddresssString (ppu, asAddress);

    outputf ("  %2i  %-6s  %-10s %5d  %-20s  %-20s\n" ,
                ppu->procunit_id, 
                asProcunitType[ppu->type],
                asProcunitStatus[ppu->status],
                ppu->maxTasks,
                asTasks,
                asAddress);

    pthread_mutex_unlock (&mutexProcunitAccess);
}


extern void PrintProcessingUnitList (void)
{
    procunit *ppu;
    
    outputf ("  Id  Type    Status     Queue  Tasks               "
             "  Address  \n");
    
    pthread_mutex_lock (&mutexProcunitAccess);
    
    ppu = gpulist;
    while (ppu != NULL) {
        PrintProcessingUnitInfo (ppu);
        ppu = ppu->next;
    }
    
    pthread_mutex_unlock (&mutexProcunitAccess);
}


static void PrintProcessingUnitStats (int procunit_id)
{
    procunit *ppu = gpulist;
        
    pthread_mutex_lock (&mutexProcunitAccess);

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

    pthread_mutex_unlock (&mutexProcunitAccess);
}


/* Finds a processing unit in the available processing units list;
   search can be started from an arbitrary processing unit ppu in the list;
   pu_type_none, pu_stat_none and pu_task_none act as jokers for search 
   criteria type, status and taskType.
   Returns a pointer to the found processing unit, or NULL if none found.
*/
static procunit *FindProcessingUnit (procunit *ppu, pu_type type, pu_status status, pu_task_type taskType, int procunit_id)
{
    pthread_mutex_lock (&mutexProcunitAccess);

    if (ppu == NULL) ppu = gpulist;
    
    while (ppu != NULL) {
        if ((type == pu_type_none || type == ppu->type)
        &&  (status == pu_stat_none || status == ppu->status)
        &&  (taskType == pu_task_none || taskType & ppu->taskMask)
        &&  (procunit_id == 0 || procunit_id == ppu->procunit_id))
            break;
            
        ppu = ppu->next;
    }
    
    pthread_mutex_unlock (&mutexProcunitAccess);

    return ppu;
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
    
    #if USE_GTK
    if (fX) GTK_TouchProcunit (ppu);
    #endif

}


/* start a processing unit.
    - local procunit: merely set it to ready state; a thread will
        be launched according to assigned tasks by calling
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
            ChangeProcessingUnitStatus (ppu, pu_stat_ready);
            return 0;
    
        case pu_type_remote:
            if (fWait) {
                StartRemoteProcessingUnit (ppu);
                #if USE_GTK
                /* when running GTK, we get here called by gtk_main(), so
                    the GDK lock is already acquired and we must release it
                    to let the new thread update its state in the master window */
                gdk_threads_leave ();
                #endif
                pthread_mutex_lock (&ppu->mutexStatusChanged);
                if (ppu->status == pu_stat_connecting)
                    pthread_cond_wait (&ppu->condStatusChanged, 
                                        &ppu->mutexStatusChanged);
                pthread_mutex_unlock (&ppu->mutexStatusChanged);
                #if USE_GTK
                gdk_threads_enter ();
                #endif
                if (ppu->status != pu_stat_deactivated) 
                    return 0;
            }
            else
                return StartRemoteProcessingUnit (ppu);
            break;
                
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
        if (RPU_DEBUG) outputerrf ("Started remote procunit thread.\n");
    }
    else {
        outputerrf ("*** StartRemoteProcessingUnit: pthread_create() error (%d).\n", err);
        assert (FALSE);
    }
    
    return err;
}


static int StopProcessingUnit (procunit *ppu)
{
    int step = -3;	/* wait 3 secs before displaying wait message */
    
    switch (ppu->type) {
    
        case pu_type_local:
            ChangeProcessingUnitStatus (ppu, pu_stat_deactivated);
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
    

    if (RPU_DEBUG) outputerrf ("### All procunits ready or deactivated.\n");
    if (RPU_DEBUG) PrintProcessingUnitList ();
    
    return 0;
}



static void CancelProcessingUnitTasks (procunit *ppu)
{
    int i;
    int procunit_id = ppu->procunit_id;
    
    if (taskList == NULL) return;
    
    if (IsMainThread ()) gdk_threads_leave ();
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
    if (IsMainThread ()) gdk_threads_enter ();
}


static int InterruptAllProcessingUnits ()
{
    procunit *ppu;
    
    pthread_mutex_lock (&mutexProcunitAccess);

    ppu = gpulist;
    
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
    
    pthread_mutex_unlock (&mutexProcunitAccess);

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
        if (err == 0) outputerrf ("Started local procunit (0x%x)\n", 
                               (int) ppu->info.local.thread);
        else outputerrf ("*** RunTaskOnLocalProcessingUnit(): Could not start local procunit!\n");
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
    
    ChangeProcessingUnitStatus (ppu, pu_stat_busy);
    /*pt->status = pu_task_inprogress;  will be set by Thread_RemoteProcessingUnit() */
    pt->timeStarted = clock ();
    pt->procunit_id = ppu->procunit_id;

    /* wake up the local rpu half thread waiting for tasks to do */
    pthread_mutex_lock (&ppu->info.remote.mutexTasksAvailable);
    ppu->info.remote.fTasksAvailable = TRUE;
    pthread_cond_signal (&ppu->info.remote.condTasksAvailable);
    pthread_mutex_unlock (&ppu->info.remote.mutexTasksAvailable);
    
    if (PU_DEBUG && err == 0) outputerrf ("Signal sent to remote procunits\n");
    if (err != 0) outputerrf ("*** RunTaskOnRemoteProcessingUnit(): Could not "
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
        ChangeProcessingUnitStatus (ppu, pu_stat_busy);
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
                    outputerrf ("*** CreateTask(): calloc() error.\n");
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
        
    outputerrf ("*** AttachTask() failed.\n");
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
    
    if (PU_DEBUG) outputerrf ("Tasks to do: %d, Avail queue: %d, rFactor: %f\n",
                totalToDoTasks, totalAvailQueue, rFactor);
    
    pthread_mutex_lock (&mutexProcunitAccess);
    
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
                    if (PU_DEBUG) outputerrf ("Picked up task %d.%d for procunit %d (%d/%d/%d).\n", 
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

    pthread_mutex_unlock (&mutexProcunitAccess);

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
            if (RPU_DEBUG) outputerrf ("# (0x%x) RPU picked up jobtask #%d id=%d.\n", 
                                    (int) pthread_self (), cTasks, taskList[i]->task_id.taskId);
        }
    }
    
    return cTasks;
}


extern void MarkTaskDone (pu_task *pt, procunit *ppu)
{
    /*outputerrf ("# Task done.\n");*/
    
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
            ChangeProcessingUnitStatus (ppu, pu_stat_ready); 
                                        /* rpu's revert themselves to ready state when ALL
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
    if (PU_DEBUG) outputerrf ("Starting Task Engine...\n");
    InitTasks ();
    gResultsAvailable = FALSE;
}


extern void TaskEngine_Shutdown (void)
{
    if (PU_DEBUG) outputerrf ("Stopping Task Engine...\n");
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
    pu_task *pt = NULL;
    
    GTK_YIELDTIME;
    
    if (TaskEngine_Empty ()) return NULL;
    
    #if USE_GTK
        /* when we get here in the main thread, we are performing
        an analysis/rollout/eval triggered from a menu/button,
        and thus we are called from gtk_main() in RunGTK(), which
        means we already have acquired the GDK lock; we release it
        now that we are idle and waiting, to give other threads a
        chance to use GTK */
        if (IsMainThread ()) gdk_threads_leave ();
    #endif

    while (!done) {
        
        #if PROCESSING_UNITS
        gdk_threads_enter ();
        #endif
        GTK_YIELDTIME;
        #if PROCESSING_UNITS
        gdk_threads_leave ();
        #endif

        pthread_mutex_lock (&mutexTaskListAccess);
        
        /* sanity check */
        
        /* assign procunits to todo tasks */
        #if PROCESSING_UNITS
        gdk_threads_enter ();
        #endif
        AssignTasksToProcessingUnits ();
        #if PROCESSING_UNITS
        gdk_threads_leave ();
        #endif
        
        /* look for available results */
        pt = FindTask (pu_task_done, 0, 0, 0);

        pthread_mutex_unlock (&mutexTaskListAccess);

            if (RPU_DEBUG > 1 && pt != NULL) {
                switch (pt->type) {
                    case pu_task_rollout: {
                            float *pf = (float *) pt->taskdata.rollout.aar;
                            outputerrf ("   %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %d\n", 
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
            break;
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
    
    #if USE_GTK
        if (IsMainThread ()) gdk_threads_enter ();
    #endif

    return pt;
}





/* use (UN)PACKJOB to (un)pack a scalar, eg. char, int, float, etc. */
#define PACKJOB(x) 			n += RPU_PackJob ((char *) &(x), 1, FALSE,sizeof(x))
#define UNPACKJOB(x) 			n += RPU_UnpackJob ((char *) &(x), FALSE, sizeof (x), FALSE)

/* use (UN)PACKJOBN to (un)pack an array of scalars, eg. int foo[4] */
#define PACKJOBN(x,N) 			n += RPU_PackJob ((char *) (x), N, TRUE, sizeof(*x))
#define UNPACKJOBN(x) 			n += RPU_UnpackJob ((char *) &(x), TRUE, sizeof (*x), FALSE)

/* use (UN)PACKJOBN to (un)pack an alloced array of scalars, 
    eg. int *foo = malloc (4 * sizeof (int)) */
#define PACKJOBPTR(x,N,t) 		n += RPU_PackJob ((char *) (x), N, TRUE, sizeof(t))
#define UNPACKJOBPTR(x,t) 		n += RPU_UnpackJob ((char *) &(x), TRUE, sizeof(t), TRUE)

/* use (UN)PACKJOBSTRUCTPTR to (un)pack an alloced array of structs, 
    eg. cubeinfo *foo = malloc (4 * sizeof (cubeinfo)) */
#define PACKJOBSTRUCTPTR(x,N,t) 	n += RPU_PackJobStruct_##t ((x), N)
#define UNPACKJOBSTRUCTPTR(x,t) 	n += RPU_UnpackJobStruct_##t (&(x))


static void RPU_DumpData (unsigned char *data, int len)
{
    int i;
    
    outputerrf ("[");
    
    for (i = 0; i < len && i < 200; i ++) {
        if (i > 0 && i % 4 == 0) outputerrf (" ");
        if (i > 0 && i % 32 == 0) outputerrf ("\n ");
        outputerrf ("%02x", data[i]);
    }
    if (i < len) outputerrf (" (... +%d bytes) ", len - i);
    
    outputerrf ("]\n");
}



static int RPU_PackWrite (char *data, int size)
{
    char *p = THREADGLOBAL(gpPackedData);
    p += THREADGLOBAL(gPackedDataPos);
    
    #if __BIG_ENDIAN__
        memcpy (p, data, size);
    #else
    {
        int i;
        for (i = size - 1; i >= 0; i --) 
            *(p ++) = data[i];
    }
    #endif
    
    THREADGLOBAL(gPackedDataPos) += size;

    return size;
}

static int RPU_PackJob (char *data, int cItems, int fSeveral, int itemSize)
{
    int n = 0;
    int neededSize = cItems * itemSize + (fSeveral ? sizeof (int) : 0);
    
    if (RPU_DEBUG_PACK > 1) {
        outputerrf ("  > RPU_PackJob(): %d item(s) of size %d bytes, "
                        "total size=%d bytes at p=+%d\n", cItems, itemSize, 
                        neededSize, THREADGLOBAL(gPackedDataPos));
        RPU_DumpData (data, cItems * itemSize);
    }
    
    /* resize gPackedData if necessary */
    while (THREADGLOBAL(gPackedDataPos) + neededSize > THREADGLOBAL(gPackedDataSize)) {
      char *pNewPackedData;
        if (RPU_DEBUG_PACK) outputerrf ("[Resizing Job]");
        /* no more room, let's double the size of the job */
        THREADGLOBAL(gPackedDataSize) *= 2;
        pNewPackedData = realloc (THREADGLOBAL(gpPackedData), THREADGLOBAL(gPackedDataSize));
        if (pNewPackedData == NULL) {
            /* realloc won't work: let's do it the long way */
            pNewPackedData = malloc (THREADGLOBAL(gPackedDataSize));
            assert (pNewPackedData != NULL);
            memcpy (pNewPackedData, THREADGLOBAL(gpPackedData), THREADGLOBAL(gPackedDataSize) / 2);
            free (THREADGLOBAL(gpPackedData));
            THREADGLOBAL(gpPackedData) = pNewPackedData;
        }
    }
    
    if (fSeveral)
        n += RPU_PackWrite ((char *) &cItems, sizeof (int));
        
    while (cItems > 0) {
        n += RPU_PackWrite (data, itemSize);
        data += itemSize;
        cItems --;
    }
    
    return n;
}

static int RPU_PackJobStruct_cubeinfo (cubeinfo *pci, int cItems)
{
    int n = 0;
    
    PACKJOB (cItems);
    
    while (cItems > 0) {
        PACKJOB (pci->nCube);
        PACKJOB (pci->fCubeOwner);
        PACKJOB (pci->fMove);
        PACKJOB (pci->nMatchTo);
        PACKJOBN (pci->anScore, 2);
        PACKJOB (pci->fCrawford);
        PACKJOB (pci->fJacoby);
        PACKJOB (pci->fBeavers);
        PACKJOBN (pci->arGammonPrice, 4);
        PACKJOB (pci->bgv);
        cItems --;
    }

    return n;
}


static int RPU_PackJobStruct_rolloutstat (rolloutstat *prs, int cItems)
{
    int n = 0;
    
    PACKJOB (cItems);
    
    while (cItems > 0) {
        PACKJOBN (prs->acWin, STAT_MAXCUBE);
        PACKJOBN (prs->acWinGammon, STAT_MAXCUBE);
        PACKJOBN (prs->acWinBackgammon, STAT_MAXCUBE);
        PACKJOBN (prs->acDoubleDrop, STAT_MAXCUBE);
        PACKJOBN (prs->acDoubleTake, STAT_MAXCUBE);
        PACKJOB (prs->nOpponentHit);
        PACKJOB (prs->rOpponentHitMove);
        PACKJOB (prs->nBearoffMoves);
        PACKJOB (prs->nBearoffPipsLost);
        PACKJOB (prs->nOpponentClosedOut);
        PACKJOB (prs->rOpponentClosedOutMove);
        cItems --;
    }

    return n;
}


static rpu_jobtask * RPU_PackTaskToJobTask (pu_task *pt)
{    
    int n = 0;
    
    THREADGLOBAL(gPackedDataSize) = DEFAULT_JOBSIZE;
    THREADGLOBAL(gpPackedData) = malloc (THREADGLOBAL(gPackedDataSize));
    assert (THREADGLOBAL(gpPackedData) != NULL);
    
    ((rpu_jobtask *) THREADGLOBAL(gpPackedData))->type = pt->type;
    if (pt->task_id.src_taskId == 0)
        pt->task_id.src_taskId = pt->task_id.taskId;
        
    THREADGLOBAL(gPackedDataPos) = offsetof (rpu_jobtask, data);	    
    
    switch (pt->type) {
    
        case pu_task_rollout: 
            {
                pu_task_rollout_data *prd = &pt->taskdata.rollout;
                PACKJOB (pt->task_id.src_taskId);
                PACKJOB (prd->type);
                switch (prd->type) {
                    case pu_task_rollout_bearoff:
                        {
                            pu_task_rollout_bearoff_data *psb = &prd->specificdata.bearoff;
                            PACKJOBPTR (prd->aanBoardEval, 2 * 25, int);
                            PACKJOBPTR (prd->aar, NUM_ROLLOUT_OUTPUTS, float);
                            PACKJOB (psb->nTruncate);
                            PACKJOB (psb->nTrials);
                            PACKJOB (psb->bgv);
                        }
                        break;
                    case pu_task_rollout_basiccubeful:
                        {
                            pu_task_rollout_basiccubeful_data *psc = &prd->specificdata.basiccubeful;
                            PACKJOB (psc->cci);
                            PACKJOBPTR (prd->aanBoardEval, 2 * 25 * psc->cci, int);
                            PACKJOBPTR (prd->aar, NUM_ROLLOUT_OUTPUTS * psc->cci, float);
                            PACKJOBSTRUCTPTR (psc->aci, psc->cci, cubeinfo);
                            PACKJOBPTR (psc->afCubeDecTop, psc->cci, int);
                            PACKJOBSTRUCTPTR (psc->aaarStatistics, 2 * psc->cci, rolloutstat);
                        }
                        break;
                }
                PACKJOB (prd->rc);
                PACKJOB (prd->seed);
                PACKJOB (prd->iTurn);
                PACKJOB (prd->iGame);
            }    
            break;
            
        case pu_task_eval:
            break;

        case pu_task_analysis:
            break;

       default:
           assert( FALSE );
           break;
    
    }

    ((rpu_jobtask *) THREADGLOBAL(gpPackedData))->len = THREADGLOBAL(gPackedDataPos);
    if (RPU_DEBUG_PACK) outputerrf ("[JobTask packed, %d bytes]\n", THREADGLOBAL(gPackedDataPos));
    if (RPU_DEBUG_PACK) RPU_DumpData (THREADGLOBAL(gpPackedData), THREADGLOBAL(gPackedDataPos));
    
    return (rpu_jobtask *) THREADGLOBAL(gpPackedData);
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
        if (RPU_DEBUG_PACK) outputerrf ("[Job #%d packed to jobtask, %d bytes]\n", 
                                                i+1, apJobTasks[i]->len);
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
                if (RPU_DEBUG_PACK) outputerrf ("[Jobtask #%d moved to job, %d bytes]\n", 
                                                        i+1, apJobTasks[i]->len);
                if (RPU_DEBUG_PACK) RPU_DumpData ( (char *) p, apJobTasks[i]->len);
                p += apJobTasks[i]->len;
                free ((char *) apJobTasks[i]);
            }
            if (RPU_DEBUG_PACK) outputerrf ("[Job packed, %d task(s), %d bytes]\n", cTasks, pjoblen);
            if (RPU_DEBUG_PACK > 1) RPU_DumpData ( (char *) pJob, pjoblen);
        }
    }
    
    free ((char *) apJobTasks);
    if (PU_DEBUG_FREE) outputerrf ("free(): RPU_PackTaskListToJob\n");
    
    return pJob;
}



static int RPU_PackRead (char *dst, int size)
{
    #if __BIG_ENDIAN__
        memcpy (dst, THREADGLOBAL(gpPackedData) + THREADGLOBAL(gPackedDataPos), size);
        if (RPU_DEBUG_PACK > 1) {
            outputerrf (">> 0x%x + %d ", THREADGLOBAL(gpPackedData), THREADGLOBAL(gPackedDataPos));
            RPU_DumpData (THREADGLOBAL(gpPackedData) + THREADGLOBAL(gPackedDataPos), size);
            RPU_DumpData (dst, size);
        }
        THREADGLOBAL(gPackedDataPos) += size;
    #else
        int i;
        for (i = size - 1; i >= 0; i --) 
            dst[i] = THREADGLOBAL(gpPackedData)[THREADGLOBAL(gPackedDataPos) ++];
    #endif

    return size;
}


int RPU_UnpackJob (char *dst, int fSeveralItems, int itemSize, int fMalloc)
{
    int 	n = 0;
    int 	cItems = 1;
    
    
    if (fSeveralItems) {
        n += RPU_PackRead ((char *) &cItems, sizeof (cItems));
    }
    
    if (RPU_DEBUG_PACK) {
        char *p = THREADGLOBAL(gpPackedData) ;
        p += THREADGLOBAL(gPackedDataPos);
        p -= fSeveralItems ? sizeof (int) : 0;
        outputerrf ("Unpacking job at 0x%x/0x%x, %d item(s) of size = %d bytes %s\n",
                       THREADGLOBAL(gpPackedData), p, cItems, itemSize, fMalloc ? "w/malloc" : "");
        RPU_DumpData (p, cItems * itemSize + (fSeveralItems ? sizeof (int) : 0) );
    }

    if (fMalloc) {
        /* if fMalloc is set, we dont read data to dst, but rather to
            a newly alloced ptr, whose address we gplsProcunits in *dst */
        char *p = malloc (cItems * itemSize);
        assert (p != NULL);
        *((char **) dst) = p;
        dst = p;
    }

    /* read all data to dst */
    while (cItems > 0) {
        n += RPU_PackRead (dst, itemSize);
        dst += itemSize;
        cItems --;
    }
    
    return n;
}


static int RPU_UnpackJobStruct_cubeinfo (cubeinfo **ppci)
{
    int 	n = 0;
    int 	cItems;
    cubeinfo	*pci;
    
    UNPACKJOB (cItems);
    
    if (RPU_DEBUG_PACK) 
        outputerrf ("Unpacking job {cubeinfo}, %d item(s) of size = %d bytes\n",
                        cItems, sizeof (cubeinfo));

    pci = malloc (cItems * sizeof (cubeinfo));
    assert (pci != NULL);
    
    *ppci = pci;
    
    while (cItems > 0) {
        UNPACKJOB (pci->nCube);
        UNPACKJOB (pci->fCubeOwner);
        UNPACKJOB (pci->fMove);
        UNPACKJOB (pci->nMatchTo);
        UNPACKJOBN (pci->anScore);
        UNPACKJOB (pci->fCrawford);
        UNPACKJOB (pci->fJacoby);
        UNPACKJOB (pci->fBeavers);
        UNPACKJOBN (pci->arGammonPrice);
        UNPACKJOB (pci->bgv);
        cItems --;
    }

    return n;
}



static int RPU_UnpackJobStruct_rolloutstat (rolloutstat **pprs)
{
    int 	n = 0;
    int 	cItems;
    rolloutstat	*prs;
    
    UNPACKJOB (cItems);
    
    if (RPU_DEBUG_PACK) 
        outputerrf ("Unpacking job {rolloutstat}, %d item(s) of size = %d bytes\n",
                        cItems, sizeof (rolloutstat));

    prs = malloc (cItems * sizeof (rolloutstat));
    assert (prs != NULL);
    
    *pprs = prs;
    
    while (cItems > 0) {
        UNPACKJOBN (prs->acWin);
        UNPACKJOBN (prs->acWinGammon);
        UNPACKJOBN (prs->acWinBackgammon);
        UNPACKJOBN (prs->acDoubleDrop);
        UNPACKJOBN (prs->acDoubleTake);
        UNPACKJOB (prs->nOpponentHit);
        UNPACKJOB (prs->rOpponentHitMove);
        UNPACKJOB (prs->nBearoffMoves);
        UNPACKJOB (prs->nBearoffPipsLost);
        UNPACKJOB (prs->nOpponentClosedOut);
        UNPACKJOB (prs->rOpponentClosedOutMove);
        cItems --;
    }

    return n;
}





static pu_task * RPU_UnpackJobToTask (rpu_jobtask *pjt, int fDetached)
{
    pu_task	*pt;
    int		n = 0;
    
    
    if (RPU_DEBUG) outputerrf ("RPU_UnpackJobToTask...\n");
    if (RPU_DEBUG_PACK) RPU_DumpData ((char *) pjt, pjt->len);
    
    pt = CreateTask (pjt->type, fDetached);
    assert (pt != NULL);
    
    THREADGLOBAL(gpPackedData) = (char *) &pjt->data;
    THREADGLOBAL(gPackedDataPos) = 0;
    
    if (pt != NULL) {
            
        switch (pjt->type) {
        
            case pu_task_rollout:
                {
                    pu_task_rollout_data *prd = &pt->taskdata.rollout;
                    UNPACKJOB (pt->task_id.src_taskId);
                    UNPACKJOB (prd->type);
                    switch (prd->type) {
                        case pu_task_rollout_bearoff:
                            {
                                pu_task_rollout_bearoff_data *psb = &prd->specificdata.bearoff;
                                UNPACKJOBPTR (prd->aanBoardEval, int);
                                UNPACKJOBPTR (prd->aar, float);
                                UNPACKJOB (psb->nTruncate);
                                UNPACKJOB (psb->nTrials);
                                UNPACKJOB (psb->bgv);
                            }
                            break;
                        case pu_task_rollout_basiccubeful:
                            {
                                pu_task_rollout_basiccubeful_data *psc = &prd->specificdata.basiccubeful;
                                UNPACKJOB (psc->cci);
                                UNPACKJOBPTR (prd->aanBoardEval, int);
                                UNPACKJOBPTR (prd->aar, float);
                                UNPACKJOBSTRUCTPTR (psc->aci, cubeinfo);
                                UNPACKJOBPTR (psc->afCubeDecTop, int);
                                UNPACKJOBSTRUCTPTR (psc->aaarStatistics, rolloutstat);
                            }
                            break;
                    }
                    UNPACKJOB(prd->rc);
                    UNPACKJOB(prd->seed);
                    UNPACKJOB(prd->iTurn);
                    UNPACKJOB(prd->iGame);
                }
                break;
                
            case pu_task_eval:
                break;
                
            case pu_task_analysis:
                break;
                
            default:
                outputerrf ("*** Unknown type of jobtask (%d).\n", pjt->type);
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
    
    /*sigset_t	ss, oss;
    sigemptyset (&ss);
    sigaddset (&ss, SIGPIPE);
    pthread_sigmask (SIG_BLOCK, &ss, &oss);*/
    
    
    do {
        GTK_YIELDTIME;
        n = send (toSocket, msg, msg->len, 0);
        fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
        fShutdown = (n == 0 || n == ECONNRESET);
        if (fShutdown) { 
            if (RPU_DEBUG) outputerrf ("*** Connection closed by peer.\n");
            fError = FALSE; n = 0;
        }
    } while (n != msg->len  && !fError && !*pfInterrupt);

    /*pthread_sigmask (SIG_SETMASK, &oss, NULL);*/

    if (n != msg->len) {
        if (fError) outputerr ("*** RPU_SendMessage: send()"); 
        else outputerrf ("*** RPU_SendMessage: sent incomplete message.\n");
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
        outputerrf ("Enter RPU_ReceiveMessage()...\n");
    
    
    /* receive first chunck containing whole message length */
    do {
        GTK_YIELDTIME;
        n = recv (fromSocket, &len, sizeof (len), MSG_PEEK | MSG_WAITALL);
        fNoData = (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
        fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
        fShutdown = (n == 0 || n == ECONNRESET);
        if (fShutdown) { 
            if (RPU_DEBUG) outputerrf ("*** Connection closed by peer.\n");
            fError = FALSE; n = 0;
        }
        if (fNoData && --timeout == 0) fError = -1;
    } while (n != sizeof (len) && !fError && !fShutdown && !*pfInterrupt);
    
    if (n != sizeof (len) && !fShutdown && !*pfInterrupt) {
        if (fError) outputerr ("*** RPU_ReceiveMessage: recv() #1");
        else if (n != 0) outputerrf ("*** RPU_ReceiveMessage: received incomplete "
                            "message #1 (%d/%d bytes).\n", n, sizeof (len));
    }
    else if (!fError && !fShutdown && !*pfInterrupt) {
        /* allocate msg */
        msg = (rpu_message *) malloc (len);
        assert (msg != NULL);
        
        /* receive the whole message */
        do {
            GTK_YIELDTIME;
            n = recv (fromSocket, msg, len, MSG_WAITALL);
            fNoData = (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
            fError = (n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK));
            fShutdown = (n == 0 || n == ECONNRESET);
            if (fShutdown) { 
                if (RPU_DEBUG) outputerrf ("*** Connection closed by peer.\n");
                fError = FALSE; n = 0;
            }
            if (fNoData && --timeout == 0) fError = -1;
        } while (n != len  && !fError && !fShutdown && !*pfInterrupt);
        
        if (n != len || fError || fShutdown) {
            if (fError) outputerr ("*** RPU_ReceiveMessage: recv() #2");
            else if (n != 0) outputerrf ("*** RPU_ReceiveMessage: received incomplete "
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
        outputerrf ("# (0x%x) Received message version=0x%x "
                        "(implemented version=0x%x)\n", 
                        (int) pthread_self (), msg->version, RPU_MSG_VERSION);
        return NULL;
    }
    
    if (msg->type == mmClose) 
        return NULL;
        
    if (msg->type != msgType) {
        outputerrf ("# (0x%x) Was expecting message type=%d, received "
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
        outputerr ("setsockopt(SO_RCVTIMEO)");
        assert (FALSE);
    }
    
    err = setsockopt (s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof (tv));
    if (err != 0) {
        outputerr ("setsockopt(SO_SNDTIMEO)");
        assert (FALSE);
    }

#endif /* __APPLE__ */

    err = setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &fReuseAddr, sizeof (fReuseAddr));
    if (err != 0) {
        outputerr ("setsockopt(SO_REUSEADDR)");
        assert (FALSE);
    }

    err = setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, &fKeepAlive, sizeof (fKeepAlive));
    if (err != 0) {
        outputerr ("setsockopt(SO_KEEPALIVE)");
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
        outputerrf (">> 0x%x\n", localAddr.sin_addr.s_addr);
        outputerrf (">> 0x%x\n", remoteAddr.sin_addr.s_addr);
        outputerrf (">> 0x%x\n", gRPU_IPMask);
    }
    if ((localAddr.sin_addr.s_addr & gRPU_IPMask) == (remoteAddr.sin_addr.s_addr & gRPU_IPMask)) 
        return 1;
    
    return 0;
}


static int RPU_CheckClose (int s)
{
    rpu_message msg;
    int n;
    
    if (RPU_DEBUG_NOTIF) outputerrf ("[checking close]");
    
    n = recv (s, &msg, sizeof (msg), MSG_PEEK);
    
    if (n > 0 && msg.type == mmClose) {
        if (RPU_DEBUG_NOTIF) outputerrf (">>> Connection closed by slave.\n");
        return 1;
    }
        
    return 0;
}



static int RPU_ReadInternetAddress (struct sockaddr_in *inAddress, char *sz,
                                    int defaultTCPPort)
{
    char 		*szPort;
    struct hostent 	*phe;
    
    bzero ((char *) inAddress, sizeof (struct sockaddr_in));
    inAddress->sin_family = AF_INET;
    
    /* scan for port, eg 192.168.0.1:1234 or host.gnu.org:1234 */
    szPort = strchr (sz, ':');
    if (szPort != NULL) {
        /* port is specified */
        int	port;
        if (sscanf (szPort + 1, "%d", &port) != 1
        ||  port < 1 || port > 65335) {
            outputerrf ("*** Invalid TCP port (%d).\n", port);
            return -1;
        }
        inAddress->sin_port = htons (port);
        *szPort = 0x00;
    }
    else {
        /* use default port */
        inAddress->sin_port = htons (defaultTCPPort);
    }
    
    /* read the non-port part of the address */
    /* try the number version, eg 192.168.0.1 */
    if (inet_aton (sz, &inAddress->sin_addr) != 0) return 0;
    
    /* didn't work, it must be a host name, eg myhost.gnu.org */
    phe = gethostbyname (sz);
    if (phe == NULL) {
        outputerrf ("*** Unknown host (%s).\n", sz);
        if (szPort != NULL) *szPort = ':';
        return -1;
    }
    else inAddress->sin_addr = *(struct in_addr *) phe->h_addr;

    if (szPort != NULL) *szPort = ':';
    
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

    if (RPU_DEBUG) outputerrf ("Received job with %d task(s).\n", cJobTasks);
    
    if (RPU_DEBUG_PACK > 1) {
        outputerrf ("Received job (len=%d):\n", job->len);
        RPU_DumpData ( (char *) job, job->len);
    }
    
    for (i = 0, j = 0; (i < cJobTasks || j < cJobTasks) && err == 0; ) {
        
        GTK_YIELDTIME;
        
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
                    outputerrf ("Created new task (id=%d, org_id=%d) from job:\n", 
                                pt->task_id.taskId, pt->task_id.src_taskId);
                    PrintTaskList ();
                }
                ((char *) pjt) += pjt->len;
                i ++;
            }
            else {
                outputerrf ("*** Could not understand job.\n");
                err = -1;
                break;
            }
        }
        
        pthread_mutex_unlock (&mutexTaskListAccess);
    
        /* retrieve and send back all results from completed tasks */
        while ((pt = TaskEngine_GetCompletedTask ()) && !fInterrupt && err == 0) {
            /* pack task into jobtask */
            rpu_jobtask *pjt;
            
            GTK_YIELDTIME
            
            if (RPU_DEBUG) outputerrf ("Completed one task!\n");
                                
            if (RPU_DEBUG > 1 && pt != NULL) {
                switch (pt->type) {
                    case pu_task_rollout: {
                            float *pf = (float *) pt->taskdata.rollout.aar;
                            outputerrf ("<< %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %d\n", 
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
    
    if (gSlaveStatus != rpu_stat_waiting && gSlaveStatus != rpu_stat_na) {
        if (cInTaskList == 0) gSlaveStatus = rpu_stat_idle;
        else if (cInProgress > 0) gSlaveStatus = rpu_stat_processing;
    }

#if USE_GTK
    if (fX) {
        if (gpwSlaveWindow != NULL) {
            char 	sz[1024];
            int	row;
            
            /*sprintf (sz, "Tasks: %d in job\n"
                        "  To do: %d, In progress: %d, Done: %d\n\n"
                        "Rollouts: %d done\n"
                        "  (received: %d, sent: %d, failed: %d)\n\n"
                        "Evals: %d done\n"
                        "  (received: %d, sent: %d, failed: %d)\n\n",
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
                        gSlaveStats.eval.failed);
                        
            gtk_text_buffer_set_text (gptbSlaveText, sz, -1);*/
            
            
            sprintf (sz, "Tasks in job: %d (To do: %d, in progress: %d, done: %d)",
                        cInTaskList, cTodo, cInProgress,  cDone);
            gtk_label_set_text (GTK_LABEL(pwSlave_Label_Job), sz);
                        
            for (row = 1; row < 4; row ++) {
                rpu_stats	*pps;
                switch (row) {
                    case 1: pps = &gSlaveStats.rollout; break;
                    case 2: pps = &gSlaveStats.eval; break;
                    case 3: pps = &gSlaveStats.analysis; break;
                }
                sprintf (sz, "%d", pps->done);
                gtk_label_set_text (GTK_LABEL(pwSlave_Label_Tasks[row][1]), sz);
                sprintf (sz, "%d", pps->rcvd);
                gtk_label_set_text (GTK_LABEL(pwSlave_Label_Tasks[row][2]), sz);
                sprintf (sz, "%d", pps->sent);
                gtk_label_set_text (GTK_LABEL(pwSlave_Label_Tasks[row][3]), sz);
                sprintf (sz, "%d", pps->failed);
                gtk_label_set_text (GTK_LABEL(pwSlave_Label_Tasks[row][4]), sz);
            }
                        
            gtk_label_set_text (GTK_LABEL(gpwLabel_Status), asSlaveStatus[gSlaveStatus]);
            GTK_YIELDTIME;
            
        }
    }
    else {
#endif
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
#if USE_GTK
    }
#endif
}


static void Slave_PrintStatus (void)
{
    #if USE_GTK
    if (!fX) {
    #endif
        outputf ("Tasks (todo/prog/done)  "
                "Rollouts (rcvd/sent/fail)  "
                "Evals (rcvd/sent/fail)  "
                "Status  "
                "\n");
    #if USE_GTK
    }
    #endif

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

extern void *Thread_NotificationSender (void *data);

static void Slave (void)
{
    int			done = FALSE;
    int			listenSocket;
    pthread_t		notifier;
    
    if (RPU_DEBUG) outputerrf ("RPU Local Half started.\n");
    
    bzero (&gSlaveStats, sizeof (gSlaveStats));
    
    gSlaveStatus = rpu_stat_na;
    Slave_PrintStatus ();
    Slave_UpdateStatus ();
    
    GTK_YIELDTIME;
    
    /* start notifications */
    if (gfRPU_Notification) {
        if (pthread_create (&notifier, 0L, Thread_NotificationSender, NULL) == 0) {
            pthread_detach (notifier);
        }
        else {
            outputerrf ("*** Could not start notifier.\n");
            gfRPU_Notification = FALSE;
        }
    }
    
    /* create socket */
    listenSocket = socket (AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        outputerrf ("*** RPU could not create slave socket (err=%d).\n", 
            errno);
        outputerr ("socket_create");
    }
    else {
        /* bind local address to socket */
        struct sockaddr_in listenAddress;
        bzero((char *) &listenAddress, sizeof (struct sockaddr_in));
        listenAddress.sin_family	= AF_INET;
        listenAddress.sin_addr.s_addr 	= htonl(INADDR_ANY);
        listenAddress.sin_port 		= htons(gRPU_SlaveTCPPort);
        RPU_SetSocketOptions (listenSocket);
	if (bind (listenSocket,  (struct sockaddr *) &listenAddress, sizeof(listenAddress)) < 0) {
            outputerrf ("*** RPU could not bind slave socket address(err=%d).\n", errno);
            outputerr("socket_bind");
        }
        else {
        
            if (fcntl (listenSocket, F_SETFL, O_NONBLOCK) < 0) {
                outputerr ("fcntl F_SETFL, O_NONBLOCK");
                assert (FALSE);
            }

            while (!fInterrupt) {
            
                TaskEngine_Init ();
                GTK_YIELDTIME;
                
                /* listen for incoming connect from master */
                gSlaveStatus = rpu_stat_waiting;
                Slave_UpdateStatus ();
                
                if (listen (listenSocket, 8) < 0) {
                    outputerrf ("*** RPU could not listen to master socket (err=%d).\n", errno);
                    outputerr ("listen");
                }
                else {
                    /* incoming connection: accept it */
                    struct sockaddr_in masterAddress;
                    struct sockaddr_in slaveAddress;
                    int masterAddressLen = sizeof (masterAddress);
                    int slaveAddressLen = sizeof (slaveAddress);
                    int rpuSocket;
                    
                    do { 
                        GTK_YIELDTIME;
                        rpuSocket = accept (listenSocket, (struct sockaddr *) &masterAddress, 
                                                &masterAddressLen);
                        if (rpuSocket == -1 && errno != EAGAIN && errno != EWOULDBLOCK) 
                                fInterrupt = TRUE;
                    } while (rpuSocket < 0 && !fInterrupt);
                    
                    getsockname (rpuSocket, (struct sockaddr *) &slaveAddress, &slaveAddressLen);
                    
                    if (rpuSocket < 0) {
                        if (!fInterrupt) {
                            outputerrf ("*** RPU could not accept connection from master (err=%d).\n", errno);
                            outputerr ("accept");
                        }
                    }
                    else if (!RPU_AcceptConnection (slaveAddress, masterAddress)) {
                        outputerrf ("*** RPU refused connection on local IP address "
                                            "%s from remote IP address %s.\n", 
                                            inet_ntoa(slaveAddress.sin_addr), 
                                            inet_ntoa(masterAddress.sin_addr));
                    }
                    else {
                        /* RPU: local half connected with remote host */
                        if (RPU_DEBUG) outputerrf ("RPU connected.\n");
                        
                        RPU_SetSocketOptions (rpuSocket);
                        
                        gSlaveStatus = rpu_stat_idle;
                        Slave_UpdateStatus ();
                        
                        done = FALSE;
                        while (!done && !fInterrupt) {
                            /* receive messages from master */
                            int			err = 0;
                            rpu_message 	*msg = RPU_ReceiveMessage (rpuSocket, &fInterrupt, 0);
                            
                            if (msg != NULL) {
                                if (RPU_DEBUG) outputerrf ("Received message type=%d.\n", msg->type);
                                if (RPU_DEBUG_PACK) RPU_DumpData ((char *) msg, msg->len);
                                switch (msg->type) {
                                    case mmGetInfo:
                                        if (RPU_DEBUG) outputerrf ("Received GetInfo msg\n");
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
                                        outputerrf ("*** Connection closed by master.\n");
                                        done = TRUE;
                                        break; 
                                    default:
                                        outputerrf ("*** Received unknown message type=%d.\n", msg->type);
                                        err = -1;
                                        break;
                                } /* switch */
                                if (err != 0) {
                                    if (!fInterrupt)
                                        outputerrf ("*** Failed to answer message (err=%d).\n", err);
                                    done = TRUE;
                                }
                                free ((char *) msg);
                            }
                            else {
                                /*outputerrf ("\nConnection closed by master.\n");*/
                                done = TRUE;
                            }
                            
                        } /* while (!done && !fInterrupt) */
                        
                        if (fInterrupt) {
                            /* send close message to master */
                            rpu_close 		msgClose = {};
                            rpu_message 	*msg;
                            
                            msg = RPU_MakeMessage (mmClose, &msgClose, sizeof (msgClose));
                            assert (msg != NULL);
                            if (msg != NULL) {
                                if (RPU_DEBUG_NOTIF) outputerrf (">>> Sending close message.\n");
                                RPU_SendMessage (rpuSocket, msg, NULL);
                                free ((char *) msg);
                            }
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
        
        /*outputf ("\n\n");*/
        
    } /* socket() */

    TaskEngine_Shutdown ();

    /* stop notifications */
    if (gfRPU_Notification) {
        pthread_mutex_lock (&mutexRPU_Notification);
        gfRPU_Notification = FALSE;
        pthread_cond_broadcast (&condRPU_Notification);
        pthread_mutex_unlock (&mutexRPU_Notification);
    }
    
    fInterrupt = FALSE;
                        
    if (RPU_DEBUG) outputerrf ("RPU Local Half stopped.\n"); 
    SetProcessingUnitsMode (pu_mode_master);   
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
        if (*step == 0) outputerrf (s);
        if (*step > 0) outputerrf (".");
        fflush (stderr);
        ++ *step;
    }
                            
    if (PU_DEBUG > 1 && err == ETIMEDOUT) outputerrf ("[Timeout]\n");
    
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
            while (!ppu->info.remote.fTasksAvailable && !ppu->info.remote.fInterrupt) {
                WaitForCondition (&ppu->info.remote.condTasksAvailable, 
                                    &ppu->info.remote.mutexTasksAvailable, "", NULL);
                if (RPU_CheckClose (rpuSocket)) {
                    ppu->info.remote.fInterrupt = TRUE;
                    ppu->info.remote.fStop = TRUE;
                }
            }
            if (RPU_DEBUG) {
                if (ppu->info.remote.fTasksAvailable)
                    outputerrf ("\n# (0x%x) RPU woken up to perform assigned "
                        "todo tasks.\n", (int) pthread_self ());
                if (ppu->info.remote.fInterrupt) {
                    outputerrf ("\n# (0x%x) RPU interrupted.\n", 
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
                outputerrf ("Task list after RPU-assign:\n");
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
                    outputerrf ("# (0x%x) Waiting for job result (%d task(s) pending).\n",
                                                (int) pthread_self (), cJobTasks);
                }
                
                msg = RPU_ReceiveMessage (rpuSocket, &ppu->info.remote.fInterrupt, 0);
                
                if (msg == NULL) {
                    if (RPU_DEBUG) outputerrf ("# (0x%x) No result received for job.\n",
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
                        if (RPU_DEBUG) outputerrf ("# (0x%x) RPU received results.\n",
                                    (int) pthread_self ());
                        
                        /* note: pt is a "detached" task, not yet in the taskList */
                        pt = RPU_UnpackJobToTask (&msg->data.taskresult, TRUE);
                        if (pt == NULL) {
                            outputerrf ("# (0x%x) RPU could not unpack task result.\n",
                                    (int) pthread_self ());
                            err = -1;
                        }
                        else {
                            pu_task *oldpt;
                            if (RPU_DEBUG) outputerrf ("# (0x%x) RPU received task id=%d results.\n",
                                        (int) pthread_self (), pt->task_id.src_taskId);
                            pthread_mutex_lock (&mutexTaskListAccess);
                            /* gplsProcunits received task with results back to master task list */
                            /* remove original task... */
                            oldpt = FindTask (0, 0, pt->task_id.src_taskId, 0);
                            if (oldpt == NULL) {
                                /* we don't know what these results are for; probably from
                                    a previously failed/interrupted connection; just ignore
                                    and discard */
                                if (RPU_DEBUG) outputerrf ("### Discarded results for task id=%d.\n",
                                                            pt->task_id.src_taskId);
                                FreeTask (pt);
                            }
                            else {
                                pt->procunit_id = oldpt->procunit_id;
                                pt->timeCreated = oldpt->timeCreated;
                                pt->timeStarted = oldpt->timeStarted;
                                if (RPU_DEBUG) outputerrf ("# [removed org task]\n");
                                FreeTask (oldpt);
                                /* ...and replace with received task that contains the results:
                                    merely replace the received task id (just assigned by 
                                    RPU_UnpackJobToTask()) by the original task id */
                                pt->task_id.taskId = pt->task_id.src_taskId;
                                AttachTask (pt);
                                MarkTaskDone (pt, ppu);
                                if (RPU_DEBUG) { 
                                    outputerrf ("# (0x%x) RPU results for task id=%d "
                                                "integrated back to lisk:\n",
                                                (int) pthread_self (), pt->task_id.src_taskId);
                                    PrintTaskList ();
                                    outputerrf ("# (0x%x) RPU end of ilst.\n",
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
                if (!ppu->info.remote.fInterrupt)
                    outputerrf ("*** Remote processing unit id=%d error, deactivating.\n", 
                                ppu->procunit_id);
                CancelProcessingUnitTasks (ppu);
                ppu->info.remote.fInterrupt = TRUE;
                ppu->info.remote.fStop = TRUE;
            }
                                    
        } /* if (cJobTasks != 0) */
                            
        if (apJobTasks != NULL) {
            free ((char *) apJobTasks);
            if (PU_DEBUG_FREE) outputerrf ("free(): apJobTasks\n");
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
    int 		rpuSocket;
    procunit		*ppu = (procunit *) data;
    int			err = 0;
    
    CreateThreadGlobalStorage ();
    
    if (RPU_DEBUG) outputerrf ("# (0x%x) RPU Local Half started.\n", (int) pthread_self ());
    
    if (RPU_ReadInternetAddress (&ppu->info.remote.inAddress, ppu->info.remote.hostName,
                                    gRPU_MasterTCPPort) < 0) {
        outputerrf ("Could not resolve address \"%s\".\n", ppu->info.remote.hostName);
        ppu->info.remote.fStop = TRUE;
    }
    
    while (!ppu->info.remote.fStop) {
    
        /*ppu->status = pu_stat_connecting;*/
        ChangeProcessingUnitStatus (ppu, pu_stat_connecting);
        ppu->maxTasks = 0;
                
        /* create socket */
        rpuSocket = socket (AF_INET, SOCK_STREAM, 0);
        if (rpuSocket == -1) {
            outputerrf ("# (0x%x) RPU could not create socket (err=%d).\n", 
                (int) pthread_self (), errno);
            outputerr ("socket_create");
            ppu->info.remote.fStop = TRUE;
        }
        else {
            /* connect to remote host */
            if (connect (rpuSocket, (const struct sockaddr *) &ppu->info.remote.inAddress, 					sizeof(ppu->info.remote.inAddress)) < 0) {
                gdk_threads_enter ();
                outputerrf ("# (0x%x) RPU could not connect socket (err=%d).\n", 
                    (int) pthread_self (), errno);
                outputerr ("connect");
                gdk_threads_leave ();
                ppu->info.remote.fStop = TRUE;
            }
            else {
                /* RPU: local half connected with remote host */
                if (RPU_DEBUG) outputerrf ("# (0x%x) RPU connected.\n", (int) pthread_self ());
                
                RPU_SetSocketOptions (rpuSocket);
                
                /* handshake / retrieve info from remote host */
                {
                    rpu_getinfo msgGetInfo = {};
                    rpu_message *msg = RPU_MakeMessage (mmGetInfo, &msgGetInfo, sizeof (msgGetInfo));
                    assert (msg != NULL);
                    if (RPU_DEBUG_PACK) RPU_DumpData ((char *) msg, msg->len);
                    err = RPU_SendMessage (rpuSocket, msg, &ppu->info.remote.fInterrupt);
                    if (err != 0) {
                        outputerrf ("# (0x%x) RPU couldn't send GetInfo msg to slave host.\n", 
                                        (int) pthread_self ());
                    }
                    else {
                        free ((char *) msg);
                        msg = RPU_ReceiveMessage (rpuSocket, &ppu->info.remote.fInterrupt, RPU_TIMEOUT);
                        if (msg == NULL) {
                            outputerrf ("# (0x%x) RPU couldn't receive Info msg "
                                            "from slave host.\n", (int) pthread_self ());
                            err = -1;
                        }
                        else {
                            /* read info */
                            rpu_info *pinfo = RPU_GetMessageData (msg, mmInfo);
                            if (pinfo == NULL) {
                                outputerrf ("# (0x%x) RPU couldn't read Info msg.\n", (int) pthread_self ());
                                err = -1;
                            }
                            else {
                                /* retrieve interesting data; calculate whole local RPU queue
                                    size according to available procunits queues on slave */
                                int i;
                                #if 0
                                RPU_PrintInfo (pinfo);
                                #endif
                                ppu->maxTasks = 0;
                                ppu->taskMask = pu_task_none;
                                for (i = 0; i < pinfo->cProcunits; i ++) {
                                    ppu->maxTasks += pinfo->procunitInfo[i].maxTasks;
                                    ppu->taskMask |= pinfo->procunitInfo[i].taskMask;
                                }
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
    
    if (RPU_DEBUG) outputerrf ("# (0x%x) RPU Local Half terminated.\n", (int) pthread_self ());
    
    return 0;
}


extern void *Thread_NotificationSender (void *data)
{
    int		notifySocket;
    int		option;
    
    notifySocket = socket (AF_INET, SOCK_DGRAM, 0);

    if (notifySocket < 0) {
        outputerrf ("*** Could not create notification socket.\n");
        outputerr ("socket");
        return;
    }
    
    option = 1;
    if (gfRPU_NotificationBroadcast
    &&	setsockopt (notifySocket, SOL_SOCKET, SO_BROADCAST, &option, sizeof(option)) == -1) {
        outputerrf ("*** Could not set socket to broadcast mode.\n");        
        outputerr("setsockopt"); 
    }
    
    else {
        struct sockaddr_in localAddress;
        localAddress.sin_family        = AF_INET;
        localAddress.sin_addr.s_addr   = INADDR_ANY;
        localAddress.sin_port          = htons (0);        
        if (bind (notifySocket, (struct sockaddr *) &localAddress, sizeof (localAddress)) == -1) {
            outputerrf ("*** Thread_NotificationSender: Could not bind socket.\n");        
            outputerr("bind"); 
        }
        else {
            int 		done = FALSE;
            rpuNotificationMsg	msg;
            struct hostent 	*phe;
            
            if (RPU_DEBUG_NOTIF) outputerrf ("# (0x%x) Notification sender started.\n", 
                                                (int) pthread_self());
            msg.inAddress = localAddress;
            msg.port = gRPU_SlaveTCPPort;
            phe = gethostent ();
            if (phe != NULL) {
                sprintf (msg.hostName, "%s:%d", phe->h_name, gRPU_SlaveTCPPort);
            }
            
            while (!done && !fInterrupt && gfRPU_Notification) {
                int step = - gRPU_SlaveNotifDelay;
                switch (gSlaveStatus) {
                    case rpu_stat_waiting:
                        if (sendto (notifySocket, (char *) &msg, sizeof (msg), 0, 
                                    (struct sockaddr *) &ginRPU_NotificationAddr, 
                                    sizeof (ginRPU_NotificationAddr)) == -1) {
                            outputerrf ("*** Notification failure.\n");
                            outputerr ("sendto");
                            done = TRUE;
                            break;
                        }
                        if (RPU_DEBUG_NOTIF) outputerrf ("[Notification sent]");
                        break;
                    case rpu_stat_na:
                        step = -1; /* retry as soon as possible, slave is starting up */
                        if (RPU_DEBUG_NOTIF) outputerrf ("[slave not available]");
                        break;
                    default:
                        if (RPU_DEBUG_NOTIF) outputerrf ("[already active]");
                        break;
                }
                
                pthread_mutex_lock (&mutexRPU_Notification);
                while (step < 0 && !fInterrupt && gfRPU_Notification)
                    WaitForCondition (&condRPU_Notification, &mutexRPU_Notification, "", &step);
                pthread_mutex_unlock (&mutexRPU_Notification);
            }
            if (RPU_DEBUG_NOTIF) outputerrf ("# (0x%x) Notification sender stopped.\n", 
                                            (int) pthread_self());    
        }
    }

    close (notifySocket);
}


extern void *Thread_NotificationListener (void *data)
{
    int		notifySocket;
    int		option;
    
    notifySocket = socket (AF_INET, SOCK_DGRAM, 0);

    if (notifySocket < 0) {
        outputerrf ("*** Could not create notification socket.\n");
        outputerr ("socket");
    }
    
    else {

        struct sockaddr_in localAddress;
        localAddress.sin_family        = AF_INET;
        localAddress.sin_addr.s_addr   = INADDR_ANY;
        localAddress.sin_port          = htons (gRPU_MasterTCPPort);        
        if (bind (notifySocket, (struct sockaddr *) &localAddress, sizeof (localAddress)) == -1) {
            outputerrf ("*** Thread_NotificationListener: Could not bind socket.\n");        
            outputerr("bind"); 
        }
        else {
            int done = FALSE;
            if (RPU_DEBUG_NOTIF) outputerrf ("# (0x%x) Notification listener started.\n", 
                                            (int) pthread_self());
                                            
            while (!done && !fInterrupt && gRPU_MasterNotifListen
            && gProcunitsMode == pu_mode_master) {
                rpuNotificationMsg 	msg;
                struct sockaddr_in 	inOrgAddress;
                int 			n, len = sizeof (inOrgAddress);

                n = recvfrom (notifySocket, &msg, sizeof (msg), 0,
                                (struct sockaddr *) &inOrgAddress, &len);
                if (n == -1) {
                    outputerrf ("*** Notification failure.\n");
                    outputerr ("recvfrom");
                    done = TRUE;
                    break;
                }
                else if (n > 0 && !fInterrupt && gRPU_MasterNotifListen) {
                    char 	szSlaveAddress[MAX_HOSTNAME];
                    procunit	*ppu;
                    if (RPU_DEBUG_NOTIF) 
                        outputerrf ("[Notification received! (%d bytes from %s:%d) -- %s \"%s\"]\n", 
                                    n, 
                                    inet_ntoa (inOrgAddress.sin_addr), 
                                    ntohs (inOrgAddress.sin_port),
                                    inet_ntoa (msg.inAddress.sin_addr), msg.hostName);
                    sprintf (szSlaveAddress, "%s:%d", inet_ntoa (inOrgAddress.sin_addr), msg.port);
                    ppu = CreateRemoteProcessingUnit (szSlaveAddress, FALSE);
                    if (ppu == NULL || ppu->status == pu_stat_deactivated) {
                        outputerrf ("*** New slave could not join!\n");
                        if (RPU_DEBUG_NOTIF) PrintProcessingUnitList ();
                    }
                    else {
                        outputerrf (">>> New slave has joined!\n");
                        if (RPU_DEBUG_NOTIF) PrintProcessingUnitList ();
                    }
                }
                else {
                    if (RPU_DEBUG_NOTIF) outputerrf ("[nothing]");
                }
            }
            
            if (RPU_DEBUG_NOTIF) outputerrf ("# (0x%x) Notification listener stopped.\n", 
                                            (int) pthread_self());    
        }

        close (notifySocket);
    }
    
}


int StartNotificationListener (void)
{
    int err;
    pthread_t	notificationListener;
    
    if (RPU_DEBUG_NOTIF) outputerrf ("Starting notification listener...\n");
    
    err = pthread_create (&notificationListener, 0L, Thread_NotificationListener, NULL);
    if (err == 0) pthread_detach (notificationListener);
    
    return err;
}


extern void CommandProcunitsMaster ( char *sz ) 
{
    SetProcessingUnitsMode (pu_mode_master);
    output ( "Host set to MASTER mode.\n" );
    outputx ();
    
    StartNotificationListener ();
}


extern void CommandProcunitsSlave ( char *sz ) 
{
    char	sz2[256] = "";
    
    SetProcessingUnitsMode (pu_mode_slave);
    
    gfRPU_Notification = FALSE;
    
    if (sz == NULL || strlen(sz) == 0) {
        /* use default parameters */
        switch (gRPU_SlaveNotifMethod) {
            case rpu_method_none:
                break;
            case rpu_method_broadcast:
                sprintf (sz2, "*:%d", gRPU_SlaveTCPPort);
                sz = sz2;
                break;
            case rpu_method_host:
                sprintf (sz2, "%s:%d", gRPU_SlaveNotifSpecificHost, gRPU_SlaveTCPPort);
                sz = sz2;
                break;
        }
    }
    
    if (sz != NULL && sz[0] != 0) {
        /* read notification parameters */
        int	port = gRPU_SlaveTCPPort;
        
        if (sz[0] == '*') {
            /* broadcast notification, eg "pu slave *:100" */
            char *szPort;
            szPort = strchr (sz + 1, ':');
            if (szPort != NULL) {
                if (sscanf (szPort + 1, "%d", &port) != 1)
                    port = -1;
            }
            if (port < 1 || port > 65535) {
                outputerrf ("Invalid TCP port number.\n");
                return;
            }
            else 
                gRPU_SlaveTCPPort = port;
            bzero (&ginRPU_NotificationAddr, sizeof (ginRPU_NotificationAddr));
            ginRPU_NotificationAddr.sin_family 		= AF_INET;
            ginRPU_NotificationAddr.sin_addr.s_addr 	= htonl(INADDR_BROADCAST);
            ginRPU_NotificationAddr.sin_port 		= htons(gRPU_SlaveTCPPort);
            gfRPU_NotificationBroadcast = TRUE;
            gfRPU_Notification 		= TRUE; 
        }
        
        else {
            /* single host notification, eg "pu slave friendly.gnu.org:100" */
            if (RPU_ReadInternetAddress (&ginRPU_NotificationAddr, sz, gRPU_SlaveTCPPort) < 0) {
                outputerrf ("Could not resolve address \"%s\".\n", sz);
                return;
            }
            gfRPU_NotificationBroadcast = FALSE;
            gfRPU_Notification 		= TRUE; 
        }
        
    } /* if (sz != NULL) */
    
    outputf ( "Host set to SLAVE mode.\n");
    #if USE_GTK
    if (!fX) {
    #endif
        if (gfRPU_Notification) {
            if (gfRPU_NotificationBroadcast)
                outputf ("  Will broadcast availability to listening masters "
                        "on TCP port %d every %d secs.\n",
                    ntohs (ginRPU_NotificationAddr.sin_port), gRPU_SlaveNotifDelay);
            else
                outputf ("  Will notify availability to %s (%s:%d) every %d secs.\n",
                    sz, inet_ntoa (ginRPU_NotificationAddr.sin_addr), 
                    ntohs (ginRPU_NotificationAddr.sin_port), gRPU_SlaveNotifDelay);
        }
        outputf ("  Hit ^C to kill pending tasks and return to MASTER mode.\n\n");
    
        outputf ("Available processing units:\n");
        PrintProcessingUnitList ();
        outputf ("\n");
    #if USE_GTK
    }
    #endif
    outputx ();
    
    Slave ();
    
    #if USE_GTK
    if (!fX) {
    #endif
        outputf ("\n\nHost set back to MASTER mode.\n\n");
        outputx ();
    #if USE_GTK
    }
    #endif
}


extern void CommandProcunitsStart ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type \"pu info\" to get full list" );
    }
    else {
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        
        if (ppu == NULL) {
            outputf ( "No processing unit found with specified id (%d).\n"
                        "Type \"pu info\" to get full list\n", procunit_id );
        } 
        else {
            if (StartProcessingUnit (ppu, TRUE) < 0)
                outputf ( "Processing unit id=%d could not be started.\n", procunit_id);
            else
                outputf ( "Processing unit id=%d started.\n", procunit_id);    
        }
    }

    outputx ();
}


extern void CommandProcunitsStop ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type \"pu info\" to get full list" );
    }
    else {
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        
        if (ppu == NULL) {
            outputf ( "No processing unit found with specified id (%d).\n"
                        "Type \"pu info\" to get full list\n", procunit_id );
        } 
        else {
            if (StopProcessingUnit (ppu) < 0)
                outputf ( "Processing unit id=%d could not be stopped.\n", procunit_id);
            else
                outputf ( "Processing unit id=%d stopped.\n", procunit_id);    
        }
    }

    outputx ();
}


extern void CommandProcunitsRemove ( char *sz ) 
{
    int procunit_id = -1;
    procunit *ppu;
    
    if( *sz )  sscanf (sz, "%d", &procunit_id);
    
    if (procunit_id == -1) {
        outputl ( "No processing unit id specified.\n"
                    "Type \"pu info\" to get full list" );
    }
    else {
        ppu = FindProcessingUnit (NULL, pu_type_none, pu_stat_none, pu_task_none, procunit_id);
        
        if (ppu == NULL) {
            outputf ( "No processing unit found with specified id (%d).\n"
                        "Type \"pu info\" to get full list\n", procunit_id );
        } 
        else {
            DestroyProcessingUnit (procunit_id);
            
            outputf ( "Processing unit id=%d removed.\n", procunit_id);
        }
    }
    
    outputx ();
}


extern void CommandProcunitsAddLocal( char *sz ) 
{
    int n = 1;
    int done = 0;
    
    if( *sz ) {
        sscanf (sz, "%d", &n);
    }

    while (n > 0) {
        procunit *ppu = CreateProcessingUnit (pu_type_local, pu_stat_ready, 
                            pu_task_info|pu_task_rollout|pu_task_analysis, 1);
        if (ppu == NULL) {
            outputf ("Could not add local processing unit.\n");
            break;
        }
        else done ++;
        n --;
    }
    
    outputf ( "%d local processing unit(s) added.\n", done);
    outputx ();
}


extern void CommandProcunitsAddRemote( char *sz ) 
{    
    procunit *ppu;
    if (sz == NULL) {
        outputf ( "Remote processing unit address not specified.\n");
        return;
    }
    
    ppu = CreateRemoteProcessingUnit (sz, TRUE);
    if (ppu == NULL)
        outputf ("Remote processing unit could not be created.\n");
    else {
        outputf ("Remote processing unit id=%d created.\n", ppu->procunit_id);
        if (ppu->status == pu_stat_deactivated)
            outputf ("*** Warning: Remote processing unit id=%d could "
                    "not be started.\n", ppu->procunit_id);
    }

    outputx ();
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
                        "Type \"pu info\" to get full list\n", procunit_id );
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


extern void CommandShowProcunitsRemoteNotifListenEnabled (char *sz)
{
    outputf ("Listen to slave availability notifications: %s\n", 
                gRPU_MasterNotifListen ? "on" : "off");
}


extern void CommandShowProcunitsRemoteNotifListenPort (char *sz)
{
    outputf ("Slave availability notifications listened to on TCP port: %d\n", gRPU_MasterTCPPort);
}


extern void CommandShowProcunitsRemoteNotifSendMethod (char *sz)
{
    outputf ("Slave availability notification method: %s %s\n", szNotifMethod[gRPU_SlaveNotifMethod],
                gRPU_SlaveNotifMethod == rpu_method_host ? gRPU_SlaveNotifSpecificHost : "");
}


extern void CommandShowProcunitsRemoteNotifSendDelay (char *sz)
{
    outputf ("Slave availability notifications send delay: %d\n", gRPU_SlaveNotifDelay);
}


extern void CommandShowProcunitsRemoteNotifSendPort (char *sz)
{
    outputf ("Slave availability notifications sent on TCP port: %d\n", gRPU_SlaveTCPPort);
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
                    "Type \"pu info\" to get full list\n", procunit_id );
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


extern void CommandSetProcunitsRemoteNotifListenEnabled ( char *sz ) 
{
    SetToggle( "procunit remote notification listen enable", &gRPU_MasterNotifListen, sz,
		 "Master will listen to available slave notifications.",
		 "Master won't listen to available slave notifications." );
                 
    if (gRPU_MasterNotifListen) StartNotificationListener ();
}


extern void CommandSetProcunitsRemoteNotifListenPort ( char *sz ) 
{
    int	port = 0;
    
    if (*sz) sscanf (sz, "%d", &port);
    
    if (port <= 0 || port >= 65536)
        outputf ("Invalid TCP port.\n");
    else
        gRPU_MasterTCPPort = port;
}


extern void CommandSetProcunitsRemoteNotifSendMethodNone ( char *sz ) 
{
    gRPU_SlaveNotifMethod = rpu_method_none;
}


extern void CommandSetProcunitsRemoteNotifSendMethodBroadcast ( char *sz ) 
{
    gRPU_SlaveNotifMethod = rpu_method_broadcast;
}


extern void CommandSetProcunitsRemoteNotifSendMethodHost ( char *sz ) 
{
    strncpy (gRPU_SlaveNotifSpecificHost, sz, MAX_HOSTNAME);
    gRPU_SlaveNotifMethod = rpu_method_host;
}


extern void CommandSetProcunitsRemoteNotifSendPort ( char *sz ) 
{
    int	port = 0;
    
    if (*sz) sscanf (sz, "%d", &port);
    
    if (port <= 0 || port >= 65536)
        outputf ("Invalid TCP port.\n");
    else
        gRPU_SlaveTCPPort = port;
}


extern void CommandSetProcunitsRemoteNotifSendDelay ( char *sz ) 
{
    int	delay = 0;
    
    if (*sz) sscanf (sz, "%d", &delay);
    
    if (delay < 1 || delay > 3600)
        outputf ("Invalid delay.\n");
    else
        gRPU_SlaveNotifDelay = delay;
}



#if USE_GTK

typedef struct {
    GtkWidget		*pwWindow;
    
    GtkWidget		*pwOptions_Check_ListenNotifs;
    GtkWidget		*pwOptions_Spin_MasterTCPPort;
    GtkWidget		*pwOptions_HBox_MasterTCPPort;
    GtkWidget		*pwOptions_Spin_QueueSize;
    
    GtkWidget		*pwOptions_Radio_Notif[3];
    GtkWidget		*pwOptions_Entry_NotifSpecific;
    GtkWidget		*pwOptions_Spin_SlaveTCPPort;
    GtkWidget		*pwOptions_HBox_SlaveTCPPort;
} _optionsWidgets;

typedef struct {
    /* widgets */
    _optionsWidgets	w;
    /* general */
    /* master */
    int		fMasterListen;
    int		iMasterTCPPort;
    int		iMinimumQueueSize;
    /* slave */
    int		iNotifMethod;
    char	szNotifSpecific[MAX_HOSTNAME];
    int		iSlaveTCPPort;
} _puOptions;


static void GetOptions (_puOptions *po)
{
    /* master */
    po->fMasterListen = gRPU_MasterNotifListen;
    po->iMasterTCPPort = gRPU_MasterTCPPort;
    po->iMinimumQueueSize = gRPU_QueueSize;
    /* slave */
    po->iNotifMethod = gRPU_SlaveNotifMethod;
    strncpy (po->szNotifSpecific, gRPU_SlaveNotifSpecificHost, MAX_HOSTNAME);
    po->iSlaveTCPPort = gRPU_SlaveTCPPort;
}


static void GTK_SetOptions (_optionsWidgets *pw, _puOptions *po)
{
    /* master */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pw->pwOptions_Check_ListenNotifs), po->fMasterListen);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pw->pwOptions_Spin_MasterTCPPort), po->iMasterTCPPort);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pw->pwOptions_Spin_QueueSize), po->iMinimumQueueSize);
    /* slave */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pw->pwOptions_Radio_Notif[po->iNotifMethod]), TRUE);
    gtk_entry_set_text (GTK_ENTRY(pw->pwOptions_Entry_NotifSpecific), po->szNotifSpecific);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pw->pwOptions_Spin_SlaveTCPPort), po->iSlaveTCPPort);
}


#define CHECK_UPDATE(button,flag,string) \
   n = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON((button))); \
   if (n != (flag)){ \
           sprintf (sz, (string), n ? "on" : "off"); \
           UserCommand (sz); \
   }
#define SPIN_UPDATE(button,val,string) \
   n = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON((button))); \
   if (n != (val)){ \
           sprintf (sz, (string), (n)); \
           UserCommand (sz); \
   }
#define RADIO_UPDATE(button,val,n,string,arg) \
   if ((val)!=(n) && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON((button[n])))) { \
           if (arg != NULL) sprintf (sz, (string), (arg)); \
           else sprintf (sz, (string)); \
           UserCommand (sz); \
   }

static void GTK_Options_OK (GtkWidget *widget, gpointer data)
{
    _puOptions 	*po = (_puOptions *) data;
    int		n;
    char	sz[256];
    
    CHECK_UPDATE (po->w.pwOptions_Check_ListenNotifs, po->fMasterListen,
                    "set pu remote notification listen enable %s");
    SPIN_UPDATE (po->w.pwOptions_Spin_MasterTCPPort, po->iMasterTCPPort,
                    "set pu remote notification listen port %d");
    /*SPIN_UPDATE (po->w.pwOptions_Spin_QueueSize, po->iMinimumQueueSize,
                    "set pu remote notification listen port %s");*/
    RADIO_UPDATE (po->w.pwOptions_Radio_Notif, po->iNotifMethod, rpu_method_none,
                    "set pu remote notification send method none", NULL);
    RADIO_UPDATE (po->w.pwOptions_Radio_Notif, po->iNotifMethod, rpu_method_broadcast,
                    "set pu remote notification send method broadcast", NULL);
    RADIO_UPDATE (po->w.pwOptions_Radio_Notif, po->iNotifMethod, rpu_method_host,
                    "set pu remote notification send method host %s", 
                    gtk_entry_get_text (GTK_ENTRY(po->w.pwOptions_Entry_NotifSpecific)));
    SPIN_UPDATE (po->w.pwOptions_Spin_SlaveTCPPort, po->iSlaveTCPPort,
                    "set pu remote notification send port %d");
}


static void GTK_Options_ListenNotifs (GtkWidget *widget, gpointer data)
{
    _puOptions *po = (_puOptions *) data;
    gtk_widget_set_sensitive (GTK_WIDGET(po->w.pwOptions_HBox_MasterTCPPort), 
                                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)));
}


static void GTK_Options_RadioNotif (GtkWidget *widget, gpointer data)
{
    _puOptions *po = (_puOptions *) data;
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget))) return;
    gtk_widget_set_sensitive (GTK_WIDGET(po->w.pwOptions_Entry_NotifSpecific), 
                            widget == po->w.pwOptions_Radio_Notif[rpu_method_host]);
    gtk_widget_set_sensitive (GTK_WIDGET(po->w.pwOptions_HBox_SlaveTCPPort), 
                            widget != po->w.pwOptions_Radio_Notif[rpu_method_none]);
}


GtkWidget *GTK_Options_Page (_puOptions *po)
{
    GtkWidget		*pwOptions_Check_ListenNotifs;
    GtkWidget		*pwOptions_Label_MasterTCPPort;
    GtkWidget		*pwOptions_Spin_MasterTCPPort;
    GtkWidget		*pwOptions_HBox_MasterTCPPort;
    GtkWidget		*pwOptions_Label_QueueSize;
    GtkWidget		*pwOptions_Spin_QueueSize;
    GtkWidget		*pwOptions_HBox_QueueSize;
    
    GtkWidget		*pwOptions_Radio_NotifNone;
    GtkWidget		*pwOptions_Radio_NotifBroadcast;
    GtkWidget		*pwOptions_Radio_NotifSpecific;
    GtkWidget		*pwOptions_Entry_NotifSpecific;
    GtkWidget		*pwOptions_Label_SlaveTCPPort;
    GtkWidget		*pwOptions_Spin_SlaveTCPPort;
    GtkWidget		*pwOptions_HBox_SlaveTCPPort;

    GtkWidget		*pwVBox_Master;
    GtkWidget		*pwFrame_Master;
    GtkWidget		*pwVBox_Slave;
    GtkWidget		*pwFrame_Slave;
    GtkWidget		*pwHBox_Buttons;
    GtkWidget		*pwVBox;
        
    /* master options */
    pwOptions_Check_ListenNotifs = gtk_check_button_new_with_label (
                                            "Listen for slave availability notifications");
    pwOptions_Label_MasterTCPPort = gtk_label_new ("Listen on TCP Port:");
    pwOptions_Spin_MasterTCPPort = gtk_spin_button_new_with_range (1.0f, 65535.0f, 1.0f);
    pwOptions_HBox_MasterTCPPort = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_MasterTCPPort), pwOptions_Label_MasterTCPPort, FALSE, FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_MasterTCPPort), pwOptions_Spin_MasterTCPPort, FALSE, FALSE, 4);
    pwOptions_Label_QueueSize = gtk_label_new ("Minimum RPU queue size:");
    pwOptions_Spin_QueueSize = gtk_spin_button_new_with_range (1.0, 100.0f, 1.0f);
    pwOptions_HBox_QueueSize = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_QueueSize), pwOptions_Label_QueueSize, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_QueueSize), pwOptions_Spin_QueueSize, TRUE, TRUE, 4);

    pwVBox_Master = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwVBox_Master), pwOptions_Check_ListenNotifs, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX(pwVBox_Master), pwOptions_HBox_MasterTCPPort, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Master), pwOptions_HBox_QueueSize, TRUE, TRUE, 4);
    
    pwFrame_Master = gtk_frame_new ("Master options");
    gtk_container_set_border_width (GTK_CONTAINER(pwFrame_Master), 4);
    gtk_container_add (GTK_CONTAINER(pwFrame_Master), pwVBox_Master);

    /* slave options */
    pwVBox_Slave = gtk_vbox_new (FALSE, 4);
    pwOptions_Radio_NotifNone = gtk_radio_button_new_with_label (NULL, "No notification");
    pwOptions_Radio_NotifBroadcast = gtk_radio_button_new_with_label_from_widget (
                    GTK_RADIO_BUTTON(pwOptions_Radio_NotifNone), "Notify all masters on local network");
    pwOptions_Radio_NotifSpecific = gtk_radio_button_new_with_label_from_widget (
                    GTK_RADIO_BUTTON(pwOptions_Radio_NotifNone), "Notify specific master:");
    pwOptions_Entry_NotifSpecific = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY(pwOptions_Entry_NotifSpecific), 32);
    pwOptions_Label_SlaveTCPPort = gtk_label_new ("Notify on TCP Port:");
    pwOptions_Spin_SlaveTCPPort = gtk_spin_button_new_with_range (1.0f, 65535.0f, 1.0f);
    pwOptions_HBox_SlaveTCPPort = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_SlaveTCPPort), pwOptions_Label_SlaveTCPPort, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX(pwOptions_HBox_SlaveTCPPort), pwOptions_Spin_SlaveTCPPort, TRUE, TRUE, 4);

    gtk_box_pack_start (GTK_BOX(pwVBox_Slave), pwOptions_Radio_NotifNone, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Slave), pwOptions_Radio_NotifBroadcast, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Slave), pwOptions_Radio_NotifSpecific, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Slave), pwOptions_Entry_NotifSpecific, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Slave), pwOptions_HBox_SlaveTCPPort, TRUE, TRUE, 4);


    pwFrame_Slave = gtk_frame_new ("Slave options");
    gtk_container_set_border_width (GTK_CONTAINER(pwFrame_Slave), 4);
    gtk_container_add (GTK_CONTAINER(pwFrame_Slave), pwVBox_Slave);

    /* the whole dialog put together... */
    pwVBox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwVBox), pwFrame_Master, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX(pwVBox), pwFrame_Slave, FALSE, FALSE, 10);
        
    /* connect signals */
    po->w.pwOptions_Check_ListenNotifs = pwOptions_Check_ListenNotifs;
    po->w.pwOptions_Spin_MasterTCPPort = pwOptions_Spin_MasterTCPPort;
    po->w.pwOptions_Spin_QueueSize = pwOptions_Spin_QueueSize;
    po->w.pwOptions_Radio_Notif[rpu_method_none] = pwOptions_Radio_NotifNone;
    po->w.pwOptions_Radio_Notif[rpu_method_broadcast] = pwOptions_Radio_NotifBroadcast;
    po->w.pwOptions_Radio_Notif[rpu_method_host] = pwOptions_Radio_NotifSpecific;
    po->w.pwOptions_Entry_NotifSpecific = pwOptions_Entry_NotifSpecific;
    po->w.pwOptions_Spin_SlaveTCPPort = pwOptions_Spin_SlaveTCPPort;
    po->w.pwOptions_HBox_SlaveTCPPort = pwOptions_HBox_SlaveTCPPort;
    po->w.pwOptions_HBox_MasterTCPPort = pwOptions_HBox_MasterTCPPort;
    
    gtk_signal_connect (GTK_OBJECT(pwOptions_Check_ListenNotifs), "toggled",
			GTK_SIGNAL_FUNC(GTK_Options_ListenNotifs), (gpointer) po);
    gtk_signal_connect (GTK_OBJECT(pwOptions_Radio_NotifNone), "toggled",
			GTK_SIGNAL_FUNC(GTK_Options_RadioNotif), (gpointer) po);
    gtk_signal_connect (GTK_OBJECT(pwOptions_Radio_NotifBroadcast), "toggled",
			GTK_SIGNAL_FUNC(GTK_Options_RadioNotif), (gpointer) po);
    gtk_signal_connect (GTK_OBJECT(pwOptions_Radio_NotifSpecific), "toggled",
			GTK_SIGNAL_FUNC(GTK_Options_RadioNotif), (gpointer) po);
    
    /* set widget values */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(pwOptions_Check_ListenNotifs), 
                                    po->fMasterListen);
    gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(po->w.pwOptions_Check_ListenNotifs));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pwOptions_Spin_MasterTCPPort), 
                                    po->iMasterTCPPort);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pwOptions_Spin_QueueSize), 
                                    po->iMinimumQueueSize);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(po->w.pwOptions_Radio_Notif[gRPU_SlaveNotifMethod]), 
                                    TRUE);
    gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON(po->w.pwOptions_Radio_Notif[gRPU_SlaveNotifMethod]));
    gtk_entry_set_text (GTK_ENTRY(pwOptions_Entry_NotifSpecific), 
                                    po->szNotifSpecific);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pwOptions_Spin_SlaveTCPPort), 
                                    po->iSlaveTCPPort);
    
    return pwVBox;
}


static void GTK_Options (GtkWidget *widget, gpointer data)
{
    GtkWidget 	*pwDialog;
    GtkWidget	*pwOptions;
    _puOptions	puOptions;
    
    GetOptions (&puOptions);
    
    /*
    puOptions.w.pwWindow = pwDialog = CreateDialog ("Processing Units Options",
                            DT_QUESTION, GTK_SIGNAL_FUNC(GTK_Options_OK), &puOptions);
    gtk_container_add (GTK_CONTAINER(DialogArea (pwDialog, DA_MAIN)),
                            pwOptions = GTK_Options_Page (&puOptions));
    gtk_widget_show_all (pwDialog);
    gtk_window_set_modal (GTK_WINDOW(pwDialog), TRUE);
    
    gtk_main();
    */
    
    puOptions.w.pwWindow =
    pwDialog = gtk_dialog_new_with_buttons ("Processing Units Options...",
                                             NULL,
                                             GTK_DIALOG_MODAL,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             NULL);
    gtk_container_add (GTK_CONTAINER(GTK_DIALOG(pwDialog)->vbox), 
                            pwOptions = GTK_Options_Page (&puOptions));
    gtk_widget_show_all (pwDialog);    
   
    if (gtk_dialog_run (GTK_DIALOG(pwDialog)) == GTK_RESPONSE_ACCEPT) {
        GTK_Options_OK (widget, &puOptions);
    }
    
    gtk_widget_destroy (pwDialog);
    GTK_YIELDTIME;
}


void GTK_Procunit_Options (gpointer *p, guint n, GtkWidget *pw)
{
    GTK_Options (pw, NULL);
}


static void GTK_Slave_Start (GtkWidget *widget, gpointer data)
{
    CommandProcunitsSlave ("");
}


static void GTK_Slave_Stop (GtkWidget *widget, gpointer data)
{
    CommandProcunitsMaster ("");
}


static void GTK_Slave_Destroy (GtkWidget *widget, gpointer data)
{
    gpwSlaveWindow = NULL;
}


void GTK_Procunit_Slave (gpointer *p, guint n, GtkWidget *pw)
{
    GtkWidget		*pwVBox;

    GtkWidget		*pwTextBuffer;
    GtkWidget		*pwHBox_Buttons;
    
    char		szDefaultSlaveText[] = "(No information yet)";
    static char		aszTaskColNames[][10] = { "", "Done", "Received", "Sent", "Failed" };
    static char		aszTaskRowNames[][10] = { "", "Rollout", "Analysis", "Eval" };
    
    int			row, col;
    
    if (gpwSlaveWindow != NULL) {
        gtk_window_present (GTK_WINDOW(gpwSlaveWindow));
        return;
    }
    
    gpwSlaveWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (gpwSlaveWindow), "Slave Mode Control");
    gtk_container_set_border_width (GTK_CONTAINER(gpwSlaveWindow), 8);
    
    gpwLabel_Status = gtk_label_new ("Status : n/a");
    
    gpwSlave_Button_Options = gtk_button_new_with_label ("Options...");
    gpwSlave_Button_Stop = gtk_button_new_with_label ("Stop");
    gpwSlave_Button_Start = gtk_button_new_with_label ("Start");

    pwHBox_Buttons = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwHBox_Buttons), gpwSlave_Button_Options, TRUE, TRUE, 80);
    gtk_box_pack_start (GTK_BOX(pwHBox_Buttons), gpwSlave_Button_Stop, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(pwHBox_Buttons), gpwSlave_Button_Start, TRUE, TRUE, 20);
    
    pwSlave_Label_Job = gtk_label_new ("Tasks in job: 0");
    
    pwSlave_Table_Tasks = gtk_table_new (4, 5, TRUE);
    for (row = 0; row < 4; row ++)
        for (col = 0; col < 5; col ++) {
            pwSlave_Label_Tasks[row][col] = gtk_label_new (row == 0 ? aszTaskColNames[col] 
                                            : (col == 0 ? aszTaskRowNames[row] : "0"));
            gtk_table_attach_defaults (GTK_TABLE(pwSlave_Table_Tasks), pwSlave_Label_Tasks[row][col],
                                        col, col+1, row, row+1);
        }

    pwVBox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwVBox), gpwLabel_Status, FALSE, FALSE, 10);
    /*gtk_box_pack_start (GTK_BOX(pwVBox), pwTextBuffer, TRUE, TRUE, 10);*/
    gtk_box_pack_start (GTK_BOX(pwVBox), pwSlave_Label_Job, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX(pwVBox), pwSlave_Table_Tasks, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX(pwVBox), pwHBox_Buttons, FALSE, FALSE, 10);
    
    gtk_container_add (GTK_CONTAINER(gpwSlaveWindow), pwVBox);
    
    gtk_widget_set_sensitive (gpwSlave_Button_Stop, FALSE);
    GTK_WIDGET_SET_FLAGS (gpwSlave_Button_Stop, GTK_CAN_DEFAULT);
    GTK_WIDGET_SET_FLAGS (gpwSlave_Button_Start, GTK_CAN_DEFAULT);
    gtk_widget_grab_default (gpwSlave_Button_Start);
    
    gtk_signal_connect (GTK_OBJECT(gpwSlaveWindow), "destroy",
			GTK_SIGNAL_FUNC(GTK_Slave_Destroy), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwSlave_Button_Start), "clicked",
			GTK_SIGNAL_FUNC(GTK_Slave_Start), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwSlave_Button_Stop), "clicked",
			GTK_SIGNAL_FUNC(GTK_Slave_Stop), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwSlave_Button_Options), "clicked",
			GTK_SIGNAL_FUNC(GTK_Options), (gpointer) NULL);

    gtk_widget_show_all (gpwSlaveWindow);
}


enum {
    col_Id,
    col_Type,
    col_Status,
    col_Queue,
    col_Tasks,
    col_Address,
    COLCOUNT
};


/* *ppPath must be gtk_tree_path_free'd by caller */
static int Tree_Find_Procunit (int procunit_id, GtkTreePath **ppPath, GtkTreeIter *pIter)
{
    int			fFound = FALSE;

    pthread_mutex_lock (&mutexProcunitAccess);

    /* find the row containing procunit_id */
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(gplsProcunits), pIter)) {
        do {
            procunit *ppu;
            gtk_tree_model_get (GTK_TREE_MODEL(gplsProcunits), pIter, 0, &ppu, -1);
            if (ppu->procunit_id == procunit_id) {
                *ppPath = gtk_tree_model_get_path (GTK_TREE_MODEL(gplsProcunits), pIter);
                fFound = TRUE;
                break;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gplsProcunits), pIter));
    }

    pthread_mutex_unlock (&mutexProcunitAccess);
    
    return fFound;
}


static void Tree_CellDataFunc_Procunit (GtkTreeViewColumn *tree_column,
                                    GtkCellRenderer *cell,
                                    GtkTreeModel *tree_model,
                                    GtkTreeIter *iter,
                                    gpointer data)
{
    procunit 	*ppu;
    char	sz[256] = "";
    
    gtk_tree_model_get (tree_model, iter, 0, &ppu, -1);
    
    switch ((int) data) {
        case col_Id:
            sprintf (sz, "%d", ppu->procunit_id);
            g_object_set (cell, "text", sz, NULL);
            break;
        case col_Type:
            /*fprintf (stderr, ".");*/
            g_object_set (cell, "text", asProcunitType[ppu->type], NULL);
            break;
        case col_Status:
            g_object_set (cell, "text", asProcunitStatus[ppu->status], NULL);
            break;
        case col_Queue:
            sprintf (sz, "%d", ppu->maxTasks);
            g_object_set (cell, "text", sz, NULL);
            break;
        case col_Tasks:
            GetProcessingUnitTasksString (ppu, sz);
            g_object_set (cell, "text", sz, NULL);
            break;
        case col_Address:
            GetProcessingUnitAddresssString (ppu, sz); 
            g_object_set (cell, "text", sz, NULL);
            break;
        default:
            g_object_set (cell, "text", "???");
    }
}


static void GTK_Master_AddLocal (GtkWidget *widget, gpointer data)
{
    CommandProcunitsAddLocal ("");    
}
                                    

static void GTK_Master_AddRemote (GtkWidget *widget, gpointer data)
{
    GtkWidget 	*pwDialog;

    GtkWidget 	*pwLabel_Address;
    GtkWidget 	*pwEntry_Address;
    GtkWidget 	*pwHBox_Address;
    
    /* create dialog */
    pwDialog = gtk_dialog_new_with_buttons ("Add Remote Processing Unit...",
                                             GTK_WINDOW(gpwMasterWindow),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             NULL);

    /* add host address entry */
    pwLabel_Address = gtk_label_new ("Slave host address: ");
    pwEntry_Address = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY(pwEntry_Address), 32);
    pwHBox_Address = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwHBox_Address), pwLabel_Address, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwHBox_Address), pwEntry_Address, FALSE, FALSE, 0);
    
    gtk_container_add (GTK_CONTAINER(GTK_DIALOG(pwDialog)->vbox), pwHBox_Address);
    gtk_widget_show_all (pwDialog);    
   
    /* run dialog */
    if (gtk_dialog_run (GTK_DIALOG(pwDialog)) == GTK_RESPONSE_ACCEPT) {
        CommandProcunitsAddRemote ((char *) gtk_entry_get_text (GTK_ENTRY(pwEntry_Address)));    
    }
    
    gtk_widget_destroy (pwDialog);
}

typedef void (*Command_Func) (char *);

static void GTK_Master_Command_ForEach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                                        gpointer data) /* data contains Commandxxx to execute */
{
    procunit	*ppu;
    char	sz[256];
    
    gtk_tree_model_get (model, iter, 0, &ppu, -1);

    sprintf (sz, "%d", ppu->procunit_id);
    ((Command_Func) data) (sz);
}
                                        

static void GTK_Master_Remove (GtkWidget *widget, gpointer data)
{
    /* can't use gtk_tree_selection_selected_foreach here: 
        the iterator fails when we remove the objects being
        iterated over... */
    /*
    gtk_tree_selection_selected_foreach ((GtkTreeSelection *) data, GTK_Master_Command_ForEach, 
                                            (gpointer) CommandProcunitsRemove);    
    */
                                            
    GtkTreeIter 	iter, nextiter;
    int			done;
    GtkTreeSelection 	*selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(gpwTree));
    
    done = !gtk_tree_model_get_iter_first (GTK_TREE_MODEL(gplsProcunits), &iter);
    while (!done) {
        procunit *ppu;
        nextiter = iter;
        done = !gtk_tree_model_iter_next (GTK_TREE_MODEL(gplsProcunits), &nextiter);
        gtk_tree_model_get (GTK_TREE_MODEL(gplsProcunits), &iter, 0, &ppu, -1);
        if (gtk_tree_selection_iter_is_selected (selection, &iter)) {
            char sz[256];
            sprintf (sz, "%d", ppu->procunit_id);
            CommandProcunitsRemove (sz);
        }
        iter = nextiter;
    }
    
}
                                    

static void GTK_Master_Start (GtkWidget *widget, gpointer data)
{
    gtk_tree_selection_selected_foreach ((GtkTreeSelection *) data, GTK_Master_Command_ForEach, 
                                            (gpointer) CommandProcunitsStart);    
}
                                    

static void GTK_Master_Stop (GtkWidget *widget, gpointer data)
{
    gtk_tree_selection_selected_foreach ((GtkTreeSelection *) data, GTK_Master_Command_ForEach, 
                                            (gpointer) CommandProcunitsStop);    
}
                                    

static void GTK_Master_SelectionChanged (GtkTreeSelection *selection, gpointer data)
{
    GtkTreeIter 	iter;
    GtkTreeModel 	*model;

    int			iSelected = 0;
    int			fRemote = FALSE;
    int			fLocal = FALSE;
    int			fReady = FALSE;
    int			fNotReady = FALSE;

    /* collect info about the selected procunits */
    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(gplsProcunits), &iter)) {
        do {
            if (gtk_tree_selection_iter_is_selected (selection, &iter)) {
                procunit *ppu;
                gtk_tree_model_get (GTK_TREE_MODEL(gplsProcunits), &iter, 0, &ppu, -1);
                iSelected ++;
                if (ppu->type == pu_type_remote) fRemote = TRUE;
                if (ppu->type == pu_type_local) fLocal = TRUE;
                if (ppu->status == pu_stat_ready) fReady = TRUE;
                if (ppu->status != pu_stat_ready) fNotReady = TRUE;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gplsProcunits), &iter));
    }
    
    /* adjust button sensitivity */
    gtk_widget_set_sensitive (gpwMaster_Button_Remove, iSelected > 0);
    gtk_widget_set_sensitive (gpwMaster_Button_Start, fNotReady);
    gtk_widget_set_sensitive (gpwMaster_Button_Stop, fReady);
    gtk_widget_set_sensitive (gpwMaster_Button_Queue, fRemote);
    gtk_widget_set_sensitive (gpwMaster_Button_Stats, iSelected > 0);
}


static void GTK_TouchProcunit (procunit *ppu)
{
    GtkTreeIter 	iter;
    GtkTreePath		*pPath;
    
    
    if (!IsMainThread ()) gdk_threads_enter ();
    
    if (Tree_Find_Procunit (ppu->procunit_id, &pPath, &iter)) {
        gtk_tree_model_row_changed (GTK_TREE_MODEL(gplsProcunits), pPath, &iter);
        gtk_tree_path_free (pPath);
        if (gpwMasterWindow != NULL)
            GTK_Master_SelectionChanged (gtk_tree_view_get_selection (GTK_TREE_VIEW(gpwTree)), NULL);
        GTK_YIELDTIME; /* FIXME: necessary? */
    }
    
    if (!IsMainThread ()) gdk_threads_leave ();
    
}


static void GTK_Master_Queue_ForEach_Preflight (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                                        gpointer data)	/* data contains ptr to max queue size */
{
    procunit	*ppu;
    char	sz[256];
    
    gtk_tree_model_get (model, iter, 0, &ppu, -1);

    if (ppu->maxTasks > *((int *) data))
        *((int *) data) = ppu->maxTasks;
}


static void GTK_Master_Queue_ForEach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                                        gpointer data)	/* data contains ptr to queue size */
{
    procunit	*ppu;
    char	sz[256];
    
    gtk_tree_model_get (model, iter, 0, &ppu, -1);

    sprintf (sz, "%d %d", ppu->procunit_id, *((int *) data));
    CommandSetProcunitsRemoteQueue (sz);
    GTK_TouchProcunit (ppu);
}


static void GTK_Master_Queue (GtkWidget *widget, gpointer data)
{
    GtkWidget 	*pwDialog;

    GtkWidget 	*pwLabel_Queue;
    GtkWidget 	*pwSpin_Queue;
    GtkWidget 	*pwHBox_Queue;
    
    int 	iQueueSize = 1;

    /* find biggest queue size */
    gtk_tree_selection_selected_foreach ((GtkTreeSelection *) data, GTK_Master_Queue_ForEach_Preflight, 
                                            (gpointer) &iQueueSize);
    
    /* create dialog */
    pwDialog = gtk_dialog_new_with_buttons ("Set RPU Queue Size...",
                                             GTK_WINDOW(gpwMasterWindow),
                                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                             GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                             NULL);

    /* add host address entry */
    pwLabel_Queue = gtk_label_new ("RPU Queue Size: ");
    pwSpin_Queue = gtk_spin_button_new_with_range (1.0f, 100.0f, 1.0f);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(pwSpin_Queue), iQueueSize);
    pwHBox_Queue = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwHBox_Queue), pwLabel_Queue, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwHBox_Queue), pwSpin_Queue, FALSE, FALSE, 0);
    
    gtk_container_add (GTK_CONTAINER(GTK_DIALOG(pwDialog)->vbox), pwHBox_Queue);
    gtk_widget_show_all (pwDialog);    
   
    /* run dialog */
    if (gtk_dialog_run (GTK_DIALOG(pwDialog)) == GTK_RESPONSE_ACCEPT) {
        iQueueSize = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON(pwSpin_Queue));    
        gtk_tree_selection_selected_foreach ((GtkTreeSelection *) data, GTK_Master_Queue_ForEach, 
                                            (gpointer) &iQueueSize);
    }
    
    gtk_widget_destroy (pwDialog);
}


static void GTK_Master_Destroy (GtkWidget *widget, gpointer data)
{
    gpwMasterWindow = NULL;
}


void GTK_Procunit_Master (gpointer *p, guint n, GtkWidget *pw)
{
    GtkTreeViewColumn 	*column;    
    GtkCellRenderer 	*renderer;
    GtkTreeSelection 	*selection;
    
    GtkWidget		*pwVBox_Buttons;
    GtkWidget		*pwHBox;

    
    if (gpwMasterWindow != NULL) {
        gtk_window_present (GTK_WINDOW(gpwMasterWindow));
        return;
    }
    
    gpwMasterWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (gpwMasterWindow), "Master Mode Control");
    gtk_container_set_border_width (GTK_CONTAINER(gpwMasterWindow), 8);

    /* create the list view; this view contains several columns; each column
        is rendered in text with the help of a user datafunction which converts 
        the relevant info in the procunit struct (pointed to by the list gplsProcunits) 
        to readable text */
    gpwTree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (gplsProcunits));

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Id", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Id, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Type", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Type, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Status", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Status, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Queue", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Queue, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Tasks", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Tasks, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Address", renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, Tree_CellDataFunc_Procunit, 
                                            (gpointer) col_Address, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (gpwTree), column);
    
    /* create the vbox containing the buttons */
    gpwMaster_Button_AddLocal = gtk_button_new_with_label ("Add Local");
    gpwMaster_Button_AddRemote = gtk_button_new_with_label ("Add Remote...");
    gpwMaster_Button_Remove = gtk_button_new_with_label ("Remove");
    gpwMaster_Button_Start = gtk_button_new_with_label ("Start");
    gpwMaster_Button_Stop = gtk_button_new_with_label ("Stop");
    
    gpwMaster_Button_Info = gtk_button_new_with_label ("Show Info...");
    gpwMaster_Button_Stats = gtk_button_new_with_label ("Show Stats...");
    gpwMaster_Button_Queue = gtk_button_new_with_label ("Set Queue Size...");
    gpwMaster_Button_Options = gtk_button_new_with_label ("Options...");
    
    pwVBox_Buttons = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_AddLocal, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_AddRemote, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Remove, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gtk_hseparator_new (), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Start, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Stop, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gtk_hseparator_new (), FALSE, FALSE, 0);
    /*gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Info, FALSE, FALSE, 0);*/
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Stats, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Queue, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(pwVBox_Buttons), gpwMaster_Button_Options, FALSE, FALSE, 0);
    
    /* put everything together */
    pwHBox = gtk_hbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX(pwHBox), gpwTree, TRUE, TRUE, 8);
    gtk_box_pack_start (GTK_BOX(pwHBox), pwVBox_Buttons, FALSE, FALSE, 8);

    gtk_container_add (GTK_CONTAINER(gpwMasterWindow), pwHBox);

    /* connect signals */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(gpwTree));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
    g_signal_connect (G_OBJECT (selection), "changed",
                        G_CALLBACK (GTK_Master_SelectionChanged), NULL);

    gtk_signal_connect (GTK_OBJECT(gpwMasterWindow), "destroy",
			GTK_SIGNAL_FUNC(GTK_Master_Destroy), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_AddLocal), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_AddLocal), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_AddRemote), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_AddRemote), (gpointer) NULL);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_Remove), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_Remove), (gpointer) selection);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_Start), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_Start), (gpointer) selection);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_Stop), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_Stop), (gpointer) selection);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_Queue), "clicked",
			GTK_SIGNAL_FUNC(GTK_Master_Queue), (gpointer) selection);
    gtk_signal_connect (GTK_OBJECT(gpwMaster_Button_Options), "clicked",
			GTK_SIGNAL_FUNC(GTK_Options), (gpointer) NULL);
    
                  
    gtk_widget_show_all (gpwMasterWindow);
}


#endif


#endif
