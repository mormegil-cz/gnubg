#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <assert.h>

#include "threadglobals.h"


/* THREAD GLOBALS FUNCTIONS
   --------------------------------------------------
*/

/* key to thread-dependent global storage */
pthread_key_t tlsThreadGlobalsKey;

/* called when a thread which has called CreateThreadGlobalStorage() quits;
    release memory allocated for the thread's global storage */
static void DestroyThreadGlobalStorage (void *tg)
{
    free (tg);
    pthread_setspecific (tlsThreadGlobalsKey, NULL);
}

/* InitThreadGlobalStorage() must be called ONCE from main thread
    before any call to CreateThreadGlobalStorage() can be made */
extern int InitThreadGlobalStorage (void)
{
    int err;
    static int fInitialised = 0;

    if (fInitialised) return 0;
    fInitialised = 1;

    err = pthread_key_create (&tlsThreadGlobalsKey, DestroyThreadGlobalStorage);
    if (err != 0) {
        fprintf (stderr, "*** InitThreadGlobalStorage()/pthread_key_create() failed (err %d).\n", err);
        assert (FALSE);
    }

    return 0;
}

/* CreateThreadGlobalStorage() must be called ONCE from EACH thread
    which wants to have thread-dependent storage for the globals
    declared in the threadglobals structure (see "threadglobals.h");
    those globals must  be accessed through the ThreadGlobal() macro;
    the initialisers (default values) for the globals must be written
    in the GLOBALS_INITIALISERS macro */
extern int CreateThreadGlobalStorage (void)
{
    /* allocate global storage for calling thread */
    threadglobals *ptg = (threadglobals *) calloc (1, sizeof (threadglobals));

    if (ptg != NULL ) {
        int err;
        /* initialise globals -- macro defined in "threadglobals.h" */
        GLOBALS_INITIALISERS (ptg);
        /* bind this local storage to the "global storage" key;
            each thread gets its own globals storage with this key */
        err = pthread_setspecific (tlsThreadGlobalsKey, ptg);
        if (err != 0) {
            fprintf (stderr, "*** CreateThreadGlobalStorage(): pthread_setspecific() failed (err %d).\n", err);
            assert (FALSE);
        }
    }
    else {
        fprintf (stderr, "*** CreateThreadGlobalStorage(): malloc() failed.\n");        assert (FALSE);
    }

    return 0;
}

