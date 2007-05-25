/*
 * sound.c from GAIM.
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include <config.h>

#include "backgammon.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

#if HAVE_ESD
#include <esd.h>
#endif

#ifdef WIN32
/* for PlaySound */
#include "windows.h"
#include <mmsystem.h>
#endif

#include "eval.h"
#include "sound.h"

char *sound_description[ NUM_SOUNDS ] = {
  N_("Starting GNU Backgammon"),
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

char *sound_command[ NUM_SOUNDS ] = {
  "start",
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

#ifdef WIN32
void PrintWinError()
{
	LPVOID lpMsgBuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS ,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf, 0, NULL);

	g_print("Windows error: ");
	g_print((LPCTSTR)lpMsgBuf);

	LocalFree(lpMsgBuf);
}
#endif

void
playSoundFile (char *file)
{
    char *command;
    GError *error = NULL;
    if (!g_file_test(file, G_FILE_TEST_EXISTS))
    {
            outputerrf("The sound file (%s) couldn't be found", file);
            return;
    }

    if (sound_cmd && *sound_cmd)
      {
	  command = g_strdup_printf ("%s %s", sound_cmd, file);
	  if (!g_spawn_command_line_async (command, &error))
	    {
		outputerrf ("sound command (%s) could not be launched: %s\n",
			    command, error->message);
		g_error_free (error);
	    }
          return;
      }

#if HAVE_ESD
    esd_play_file (NULL, file, 1);
#endif

#ifdef WIN32
    SetLastError (0);
    while (!PlaySound
	   (file, NULL,
	    SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NODEFAULT))
      {
	  static int soundDeviceAttached = -1;
	  if (soundDeviceAttached == -1)
	    {			/* Check for sound card */
		soundDeviceAttached = waveOutGetNumDevs ();
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
		PrintWinError ();
		SetLastError (0);
		return;
	    }
	  Sleep (1);		/* Wait (1ms) for current sound to finish */
      }
#endif
}

extern void playSound ( const gnubgsound gs )
{
	char *sound;

	if ( ! fSound )
		/* no sounds for this user */
		return;
	
	sound = GetSoundFile(gs);
	if ( *sound )
		playSoundFile( sound );
	g_free(sound);
}


extern void SoundWait( void ) {

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

extern char *GetDefaultSoundFile(int sound)
{
  static char aszDefaultSound[ NUM_SOUNDS ][ 80 ] = {
  /* start and exit */
  "sounds/fanfare.wav",
  /* commands */
  "sounds/drop.wav",
  "sounds/double.wav",
  "sounds/drop.wav",
  "sounds/chequer.wav",
  "sounds/move.wav",
  "sounds/double.wav",
  "sounds/resign.wav",
  "sounds/roll.wav",
  "sounds/take.wav",
  /* events */
  "sounds/dance.wav",
  "sounds/gameover.wav",
  "sounds/matchover.wav",
  "sounds/dance.wav",
  "sounds/gameover.wav",
  "sounds/matchover.wav",
  "sounds/fanfare.wav"
  };

	return g_build_filename(PKGDATADIR, aszDefaultSound[sound], NULL);
}

extern char *GetSoundFile(gnubgsound sound)
{
	if (!sound_file[sound])
		return GetDefaultSoundFile(sound);
	return g_strdup(sound_file[sound]);
}

extern void SetSoundFile(gnubgsound sound, const char *file)
{
	char *old_file =  GetSoundFile(sound);
	if (!file || !strcmp(file, old_file))
	{
		g_free(old_file);
		return;		/* No change */
	}
	g_free(old_file);

	if (!*file) {
		outputf(_("No sound played for: %s\n"),
			gettext(sound_description[sound]));
	} else {
		outputf(_("Sound for: %s: %s\n"),
			gettext(sound_description[sound]),
			file);
	}
	g_free(sound_file[sound]);
	sound_file[sound] = g_strdup(file);
}

extern char *sound_get_command(void)
{
	return (sound_cmd ? sound_cmd : "");
};

extern char *sound_set_command(const char *sz)
{
	g_free(sound_cmd);
	sound_cmd = g_strdup(sz ? sz : "");
	return sound_cmd;
};

