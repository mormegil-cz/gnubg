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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef HAVE_ESD
#include <esd.h>
#endif

#ifdef HAVE_ARTSC
#include <artsc.h>
#endif

#ifdef HAVE_NAS
#include <audio/audiolib.h>
#endif

#ifdef WIN32
#include "windows.h" /* for PlaySound */
#endif

#include "glib.h"

#include "i18n.h"
#include "sound.h"

char *aszSoundDesc[ NUM_SOUNDS ] = {
  N_("Starting GNU Backgammon"),
  N_("Exiting GNU Backgammon"),
  N_("Agree"),
  N_("Doubling"),
  N_("Drop"),
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
  N_("Bot wins match")
};

char *aszSoundCommand[ NUM_SOUNDS ] = {
  "start",
  "exit",
  "agree",
  "double",
  "drop",
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
  "botwinsmatch"
};

char aszSound[ NUM_SOUNDS ][ 80 ] = {
  /* start and exit */
  "woohoo.wav",
  "t2-hasta_la_vista.wav",
  /* commands */
  "idiot.wav",
  "makeday.wav",
  "hmrnoooo.wav",
  "",
  "aycar.wav",
  "terminater.wav",
  "",
  "not.wav",
  /* events */
  "haha.wav",
  "haha.wav",
  "haha.wav",
  "bulwnk50.wav",
  "bulwnk50.wav",
  "bulwnk50.wav"
};

char szSoundCommand[ 80 ] = "/usr/bin/sox %s -t ossdsp /dev/dsp";

#  if defined (HAVE_ESD)
soundsystem ssSoundSystem = SOUND_SYSTEM_ESD;
#  elif defined (HAVE_ARTSC)
soundsystem ssSoundSystem = SOUND_SYSTEM_ARTSC;
#  elif defined (HAVE_NAS)
soundsystem ssSoundSystem = SOUND_SYSTEM_NAS;
#  elif defined (WIN32)
soundsystem ssSoundSystem = SOUND_SYSTEM_WINDOWS;
#  elif 
soundsystem ssSoundSystem = SOUND_SYSTEM_NORMAL;
#  endif

int fSound = TRUE;


char *aszSoundSystem[ NUM_SOUND_SYSTEMS ] = {
  N_("ArtsC"), 
  N_("External command"),
  N_("ESD"),
  N_("NAS"),
  N_("/dev/audio"),
  N_("MS Windows") 
};

char *aszSoundSystemCommand[ NUM_SOUND_SYSTEMS ] = {
  "artsc", 
  "command",
  "esd",
  "nas",
  "normal",
  "windows" 
};



static int check_dev(char *dev) {

  struct stat stat_buf;
  uid_t user = getuid();
  gid_t group = getgid(), other_groups[32];
  int i, numgroups;

  if ((numgroups = getgroups(32, other_groups)) == -1)
    return 0;
  if (stat(dev, &stat_buf))
    return 0;
  if (user == stat_buf.st_uid && stat_buf.st_mode & S_IWUSR)
    return 1;
  if (stat_buf.st_mode & S_IWGRP) {
    if (group == stat_buf.st_gid)
      return 1;
    for (i = 0; i < numgroups; i++)
      if (other_groups[i] == stat_buf.st_gid)
        return 1;
  }

  if (stat_buf.st_mode & S_IWOTH)
    return 1;
  return 0;

}


static void play_audio_file(const char *file) {

  /* here we can assume that we can write to /dev/audio */
  char *buf;
  struct stat info;
  int fd = open(file, O_RDONLY);

  if (fd <= 0) {
    return;
  }
  fstat(fd, &info);

  if (info.st_size < 24)
    return;

  buf = malloc(info.st_size + 1);
  read(fd, buf, 24);
  read(fd, buf, info.st_size - 24);
  close(fd);

  fd = open("/dev/audio", O_WRONLY | O_EXCL | O_NDELAY);
        
  if (fd < 0) {
    free(buf);
    return;
  }
  write(fd, buf, info.st_size - 24);
  free(buf);
  close(fd);

}

static int can_play_audio() {
  return check_dev("/dev/audio");
}


#ifdef HAVE_ARTSC

static int play_artsc(unsigned char *data, int size)
{
  arts_stream_t stream;
  guint16 *lineardata;
  int result = 1;
  int error;
  int i;

  lineardata = g_malloc(size * 2);

  for (i = 0; i < size; i++) {
    lineardata[i] = _af_ulaw2linear(data[i]);
  }

  stream = arts_play_stream(8012, 16, 1, "gaim");

  error = arts_write(stream, lineardata, size);
  if (error < 0) {
    result = 0;
  }

  arts_close_stream(stream);

  g_free(lineardata);

  arts_free();

  return result;
}

static int can_play_artsc()
{
  int error;

  error = arts_init();
  if (error < 0)
    return 0;

  return 1;
}

static int artsc_play_file(const char *file)
{
  struct stat stat_buf;
  unsigned char *buf = NULL;
  int result = 0;
  int fd = -1;

  if (!can_play_artsc())
    return 0;

  fd = open(file, O_RDONLY);
  if (fd < 0)
    return 0;

  if (fstat(fd, &stat_buf)) {
    close(fd);
    return 0;
  }

  if (!stat_buf.st_size) {
    close(fd);
    return 0;
  }

  buf = g_malloc(stat_buf.st_size);
  if (!buf) {
    close(fd);
    return 0;
  }

  if (read(fd, buf, stat_buf.st_size) < 0) {
    g_free(buf);
    close(fd);
    return 0;
  }

  result = play_artsc(buf, stat_buf.st_size);

  g_free(buf);
  close(fd);
  return result;
}

#endif /* HAVE_ARTSC */

#ifdef HAVE_NAS

char nas_server[] = "localhost";
AuServer *nas_serv = NULL;

static AuBool NasEventHandler(AuServer * aud, AuEvent * ev, AuEventHandlerRec * handler)
{
  AuElementNotifyEvent *event = (AuElementNotifyEvent *) ev;

  if (ev->type == AuEventTypeElementNotify) {
    switch (event->kind) {
    case AuElementNotifyKindState:
      switch (event->cur_state) {
      case AuStateStop:
        _exit(0);
      }
      break;
    }
  }
  return AuTrue;
}


static int play_nas(unsigned char *data, int size)
{
  AuDeviceID device = AuNone;
  AuFlowID flow;
  AuElement elements[3];
  int i, n, w;

  /* look for an output device */
  for (i = 0; i < AuServerNumDevices(nas_serv); i++) {
    if ((AuDeviceKind(AuServerDevice(nas_serv, i)) ==
         AuComponentKindPhysicalOutput) &&
        AuDeviceNumTracks(AuServerDevice(nas_serv, i)) == 1) {
      device = AuDeviceIdentifier(AuServerDevice(nas_serv, i));
      break;
    }
  }

  if (device == AuNone)
    return 0;

  if (!(flow = AuCreateFlow(nas_serv, NULL)))
    return 0;


  AuMakeElementImportClient(&elements[0], 8012, AuFormatULAW8, 1, AuTrue, size, size / 2, 0, NULL);
  AuMakeElementExportDevice(&elements[1], 0, device, 8012, AuUnlimitedSamples, 0, NULL);
  AuSetElements(nas_serv, flow, AuTrue, 2, elements, NULL);

  AuStartFlow(nas_serv, flow, NULL);

  AuWriteElement(nas_serv, flow, 0, size, data, AuTrue, NULL);

  AuRegisterEventHandler(nas_serv, AuEventHandlerIDMask, 0, flow, NasEventHandler, NULL);

  while (1) {
    AuHandleEvents(nas_serv);
  }

  return 1;
}

static int can_play_nas()
{
  if ((nas_serv = AuOpenServer(NULL, 0, NULL, 0, NULL, NULL)))
    return 1;
  return 0;
}

static int play_nas_file(char *file)
{
  struct stat stat_buf;
  char *buf;
  int ret;
  int fd = open(file, O_RDONLY);
  if (fd <= 0)
    return 0;

  if (!can_play_nas())
    return 0;

  if (stat(file, &stat_buf))
    return 0;

  if (!stat_buf.st_size)
    return 0;

  buf = malloc(stat_buf.st_size);
  read(fd, buf, stat_buf.st_size);
  ret = play_nas(buf, stat_buf.st_size);
  free(buf);
  return ret;
}

#endif

static void 
play_file(const char *filename) {

  int pid;

  /* fork, so we don't have to wait for the sound to finish */

#ifndef WIN32

  pid = fork();

  if (pid < 0)
    /* parent */
    return;
  else if (pid == 0) {
    /* child */
          
    /* kill after 30 secs */
    alarm(30);

#else

    /* don't fork with windows as PlaySound can play async. */

#endif
	
    switch ( ssSoundSystem ) {

    case SOUND_SYSTEM_COMMAND:

      if ( *szSoundCommand ) {
	
        char *args[4];
        char command[4096];

        g_snprintf(command, sizeof(command), szSoundCommand, filename);
        
        args[0] = "sh";
        args[1] = "-c";
        args[2] = command;
        args[3] = NULL;
        execvp(args[0], args);
        _exit(0);
      }

      break;

    case SOUND_SYSTEM_ESD:

#ifdef HAVE_ESD

      if (esd_play_file(NULL, filename, 1) )
        _exit(0);

#else
      assert ( FALSE );
#endif

      break;

    case SOUND_SYSTEM_ARTSC:

#ifdef HAVE_ARTSC

      if (artsc_play_file(filename))
        _exit(0);
#else
      assert ( FALSE );
#endif

    case SOUND_SYSTEM_NAS:

#ifdef HAVE_NAS
    else if (sound_options & OPT_SOUND_NAS) {
      if (play_nas_file(filename))
        _exit(0);
    }
#else
      assert ( FALSE );
#endif
      
      break;

    case SOUND_SYSTEM_NORMAL:

      if ( can_play_audio() ) {
        play_audio_file(filename);
        _exit(0);
      }
      
      break;

    case SOUND_SYSTEM_WINDOWS:

#ifdef WIN32
      PlaySound ( filename, NULL, SND_FILENAME | SND_ASYNC );
      _exit(0);
#else
      assert ( FALSE );
#endif
      break;

    default:

      break;

    }

#ifndef WIN32

    _exit(0);

  }

#else

  /* we didn't fork */

#endif

}


extern void
playSound ( const gnubgsound gs ) {

  if ( ! fSound )
    /* no sounds for this user */
    return;

  if ( ! *aszSound[ gs ] )
    /* no sound defined for event */
    return;

  play_file ( aszSound[ gs ] );

}


