/*
 * sound.c from GAIM.
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 or later of the License, or
 * (at your option) any later version.
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
 * File modified by Joern Thyssen <jthyssen@dk.ibm.com> for use with
 * GNU Backgammon.
 *
 * $Id$
 */

#include "config.h"

#include <glib.h>
#include <string.h>
#include <stdlib.h>

#include "backgammon.h"
#include <glib/gi18n.h>

#if USE_GTK
#include "gtkgame.h"
#endif


#if defined(WIN32)
/* for PlaySound */
#include "windows.h"
#include <mmsystem.h>

#elif defined(__APPLE__)
#include <QuickTime/QuickTime.h>
#include <pthread.h>
#include "lib/list.h"

static int		fQTInitialised = FALSE;
static int 		fQTPlaying = FALSE;
listOLD			movielist;
static pthread_mutex_t 	mutexQTAccess;


void * Thread_PlaySound_QuickTime (void *data)
{
    int done = FALSE;
    
    fQTPlaying = TRUE;
    
    do {
        listOLD	*pl; 
        
        pthread_mutex_lock (&mutexQTAccess);
        
        /* give CPU time to QT to process all running movies */
        MoviesTask (NULL, 0);
        
        /* check if there are any running movie left */
        pl = &movielist;
        done = TRUE;
        do {
            listOLD *next = pl->plNext;
            if (pl->p != NULL) {
                Movie *movie = (Movie *) pl->p;
                if (IsMovieDone (*movie)) {
                    DisposeMovie (*movie);
                    free (movie);
                    ListDelete (pl);
                }
                else
                    done = FALSE;
            }
            pl = next;
        } while (pl != &movielist);
        
        pthread_mutex_unlock (&mutexQTAccess);
    } while (!done && fQTPlaying);
    
    fQTPlaying = FALSE;
    
    return NULL;
}


void PlaySound_QuickTime (const char *cSoundFilename)
{
    int 	err;
    Str255	pSoundFilename; 	/* file pathname in Pascal-string format */
    FSSpec	fsSoundFile;		/* movie file location descriptor */
    short	resRefNum;		/* open movie file reference */
    
    if (!fQTInitialised) {
        pthread_mutex_init (&mutexQTAccess, NULL);
        ListCreate (&movielist);
        fQTInitialised = TRUE;
    }
    
    /* QuickTime is NOT reentrant in Mac OS (it is in MS Windows!) */
    pthread_mutex_lock (&mutexQTAccess);

    EnterMovies ();	/* can be called multiple times */

	err = NativePathNameToFSSpec(cSoundFilename, &fsSoundFile, 0);    
    if (err != 0) {
        fprintf (stderr, "PlaySound_QuickTime: error #%d, can't find %s.\n", err, cSoundFilename);
    }
    else {
        /* open movie (WAV or whatever) file */
        err = OpenMovieFile (&fsSoundFile, &resRefNum, fsRdPerm);
        if (err != 0) {
            fprintf (stderr, "PlaySound_QuickTime: error #%d opening %s.\n", err, cSoundFilename);
        }
        else {
            /* create movie from movie file */
            Movie	*movie = (Movie *) malloc (sizeof (Movie));
            err = NewMovieFromFile (movie, resRefNum, NULL, NULL, 0, NULL);  
            CloseMovieFile (resRefNum);
            if (err != 0) {
                fprintf (stderr, "PlaySound_QuickTime: error #%d reading %s.\n", err, cSoundFilename);
            } 
            else {
                /* reset movie timebase */
                TimeRecord t = { 0 };
                t.base = GetMovieTimeBase (*movie);
                SetMovieTime (*movie, &t);
                /* add movie to list of running movies */
                ListInsert (&movielist, movie);
                /* run movie */
                StartMovie (*movie);  
            }
        }
    }

    pthread_mutex_unlock (&mutexQTAccess);

    if (!fQTPlaying) {
        /* launch playing thread if necessary */
        int err;
        pthread_t qtthread;
        fQTPlaying = TRUE;
        err = pthread_create (&qtthread, 0L, Thread_PlaySound_QuickTime, NULL);
        if (err == 0) pthread_detach (qtthread);
        else fQTPlaying = FALSE;
    }
}
#elif HAVE_GSTREAMER
#include <gst/gst.h>
static void print_gst_error(GstMessage *message, const char *sound)
{
	GError *err;
	gchar *debug;

	gst_message_parse_error(message, &err, &debug);
	outputerrf(_("Error playing %s: %s\n"), sound, err->message);
	g_error_free(err);
	g_free(debug);
}

static gboolean bus_callback(GstBus * bus, GstMessage * message,
				gpointer play)
{
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_ERROR:
		{
			print_gst_error(message, _("sound"));
			gst_element_set_state(play, GST_STATE_NULL);
			gst_object_unref(GST_OBJECT(play));
			return FALSE;
		}
	case GST_MESSAGE_EOS:
		{
			gst_element_set_state(play, GST_STATE_NULL);
			gst_object_unref(GST_OBJECT(play));
			return FALSE;
		}
	default:
		return TRUE;
	}

}

static void PlaySoundGst(const char *fn, gboolean sync)
{
#define MAX_PLAY_TIME 6*GST_SECOND
	GstElement *play;
	GstBus *bus;
	gchar *uri;

	g_return_if_fail(g_file_test(fn, G_FILE_TEST_EXISTS));
	g_return_if_fail(g_path_is_absolute(fn));
	uri = g_filename_to_uri(fn, NULL, NULL);
	g_return_if_fail(uri);

	play = gst_element_factory_make("playbin", "play");
	g_object_set(G_OBJECT(play), "uri", uri, NULL);

	if (gst_element_set_state(play, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
	{
		outputerrf("Failed to play sound file '%s'", fn);
		gst_element_set_state(play, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(play));
		return;
	}


	bus = gst_pipeline_get_bus (GST_PIPELINE (play));
	if (sync)
	{
		GstMessage *msg;
		msg = gst_bus_poll (bus, GST_MESSAGE_EOS | GST_MESSAGE_ERROR, MAX_PLAY_TIME);
		if (msg)
		{
			if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				print_gst_error(msg, fn);
			gst_message_unref(msg);
		}
		gst_object_unref (bus);
		gst_element_set_state(play, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(play));
	}
	else
	{
		gst_bus_add_watch (bus, bus_callback, play);
		gst_object_unref (bus);
	}
}
#endif

#include "sound.h"
#include "util.h"

const char *sound_description[ NUM_SOUNDS ] = {
  N_("Starting GNU Backgammon"),
  N_("Exiting GNU Backgammon"),
  N_("Agree"),
  N_("Doubling"),
  N_("Drop"),
  N_("Chequer movement"),
  N_("Move"),
  N_("Redouble"),
  N_("Resign"),
  N_("Roll"),
  N_("Take"),
  N_("Human fans"),
  N_("Human wins game"),
  N_("Human wins match"),
  N_("Bot fans"),
  N_("Bot wins game"),
  N_("Bot wins match"),
  N_("Analysis is finished")
};

const char *sound_command[ NUM_SOUNDS ] = {
  "start",
  "exit",
  "agree",
  "double",
  "drop",
  "chequer",
  "move",
  "redouble",
  "resign",
  "roll",
  "take",
  "humanfans",
  "humanwinsgame",
  "humanwinsmatch",
  "botfans",
  "botwinsgame",
  "botwinsmatch",
  "analysisfinished"
};

int fSound = TRUE;
static char *sound_cmd = NULL;

void
playSoundFile (char *file, gboolean sync)
{
    GError *error = NULL;
    if (!g_file_test(file, G_FILE_TEST_EXISTS))
    {
            outputf(_("The sound file (%s) doesn't exist.\n"), file);
            return;
    }

    if (sound_cmd && *sound_cmd)
	{
		char *commandString;

		commandString = g_strdup_printf ("%s %s", sound_cmd, file);
		if (!g_spawn_command_line_async (commandString, &error))
		{
			outputf (_("sound command (%s) could not be launched: %s\n"),
				commandString, error->message);
			g_error_free (error);
		}
		return;
	}

#if defined(WIN32)
    SetLastError (0);
    while (!PlaySound
	   (file, NULL,
	    SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT))
      {
	  static int soundDeviceAttached = -1;
	  if (soundDeviceAttached == -1)
	    {			/* Check for sound card */
		soundDeviceAttached = (int)waveOutGetNumDevs ();
	    }
	  if (!soundDeviceAttached)
	    {			/* No sound card found - disable sound */
		g_print ("No soundcard found - sounds disabled\n");
		fSound = FALSE;
		return;
	    }
	  /* Check for errors */
	  if (GetLastError ())
	    {
		PrintSystemError("Playing sound");
		SetLastError (0);
		return;
	    }
	  Sleep (1);		/* Wait (1ms) for current sound to finish */
      }
#elif defined(__APPLE__)
	PlaySound_QuickTime (file);
#elif HAVE_GSTREAMER
	PlaySoundGst(file, sync);
#endif
}

extern void playSound ( const gnubgsound gs )
{
	char *sound;

	if ( ! fSound )
		/* no sounds for this user */
		return;

	sound = GetSoundFile(gs);
	if ( !*sound )
	{
		g_free(sound);
		return;
	}
#if USE_GTK
	if (!fX || gs == SOUND_EXIT)
		playSoundFile( sound, TRUE );
	else 
		playSoundFile( sound, FALSE );
#else
	playSoundFile( sound, TRUE );
#endif

	g_free(sound);
}


extern void SoundWait( void )
{
	if (!fSound)
		return;
#ifdef WIN32
	/* Wait 1/10 of a second to make sure sound has started */
	Sleep(100);
	while (!PlaySound(NULL, NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT))
		Sleep(1);	/* Wait (1ms) for previous sound to finish */
	return;
#endif
}

char *sound_file[ NUM_SOUNDS ] = {0};

extern char *GetDefaultSoundFile(gnubgsound sound)
{
  static char aszDefaultSound[ NUM_SOUNDS ][ 80 ] = {
  /* start and exit */
  "fanfare.wav",
  "haere-ra.wav",
  /* commands */
  "drop.wav",
  "double.wav",
  "drop.wav",
  "chequer.wav",
  "move.wav",
  "double.wav",
  "resign.wav",
  "roll.wav",
  "take.wav",
  /* events */
  "dance.wav",
  "gameover.wav",
  "matchover.wav",
  "dance.wav",
  "gameover.wav",
  "matchover.wav",
  "fanfare.wav"
  };

	return BuildFilename2("sounds", aszDefaultSound[sound]);
}

extern char *GetSoundFile(gnubgsound sound)
{
	if (!sound_file[sound])
		return GetDefaultSoundFile(sound);

	if (!(*sound_file[sound]))
		return g_strdup("");

	if (g_file_test(sound_file[sound], G_FILE_TEST_EXISTS))
		return g_strdup(sound_file[sound]);

	if (g_path_is_absolute(sound_file[sound]))
		return GetDefaultSoundFile(sound);

	return BuildFilename(sound_file[sound]);
}

extern void SetSoundFile(gnubgsound sound, const char *file)
{
	char *old_file = GetSoundFile(sound);
	const char *new_file = file ? file : "";
	if (!strcmp(new_file, old_file))
	{
		g_free(old_file);
		return;		/* No change */
	}
	g_free(old_file);

	if (!*new_file) {
		outputf(_("No sound played for: %s\n"),
			gettext(sound_description[sound]));
	} else {
		outputf(_("Sound for: %s: %s\n"),
			gettext(sound_description[sound]),
			new_file);
	}
	g_free(sound_file[sound]);
	sound_file[sound] = g_strdup(new_file);
}

extern const char *sound_get_command(void)
{
	return (sound_cmd ? sound_cmd : "");
}

extern char *sound_set_command(const char *sz)
{
	g_free(sound_cmd);
	sound_cmd = g_strdup(sz ? sz : "");
	return sound_cmd;
}

extern void SetExitSoundOff(void)
{
	sound_file[SOUND_EXIT] = g_strdup("");
}

