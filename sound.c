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
#include <assert.h>
#if HAVE_SYS_AUDIOIO_H
#include <sys/audioio.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#if HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

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

/* #include "glib.h"  - is this really necessary? */

#include "backgammon.h"
#include "eval.h"
#include "i18n.h"
#include "sound.h"

#if !defined(SIGIO) && defined(SIGPOLL)
#define SIGIO SIGPOLL /* The System V equivalent */
#endif

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
  "sounds/fanfare.wav",
  "sounds/haere-ra.wav",
  /* commands */
  "sounds/drop.wav",
  "sounds/double.wav",
  "sounds/drop.wav",
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
  "sounds/matchover.wav"
};

char szSoundCommand[ 80 ] = "/usr/bin/sox %s -t ossdsp /dev/dsp";

#  if defined (SIGIO)
soundsystem ssSoundSystem = SOUND_SYSTEM_NORMAL;
#  elif defined (HAVE_ESD)
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
  N_("/dev/dsp"), /* with fallback to /dev/audio */
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

#ifdef SIGIO

static int hSound = -1; /* file descriptor of dsp/audio device */
static char *pSound, *pSoundCurrent;
static size_t cbSound; /* bytes remaining */

#ifdef ITIMER_REAL
static int fHandler;
#endif

extern RETSIGTYPE SoundSIGIO( int idSignal ) {

    int cch;
    sigset_t ss;

    sigemptyset( &ss );
    sigaddset( &ss, SIGIO );
    sigaddset( &ss, SIGALRM );
    sigprocmask( SIG_BLOCK, &ss, NULL );
  
    if( cbSound ) {
	cch = write( hSound, pSoundCurrent, cbSound );
	if( cch < 0 && errno == EAGAIN )
	    return;
	else if( cch < 0 || !( cbSound -= cch ) ) {
	    cbSound = 0;
#ifdef SNDCTL_DSP_POST
	    ioctl( hSound, SNDCTL_DSP_POST, 0 );
#endif
#ifndef SNDCTL_DSP_GETODELAY
	    close( hSound );
	    hSound = -1;
#endif
	    /* We would like to free( pSound ) here, but it's not safe to
	       call free() from a signal handler, so we leave that job to
	       somebody else. */
	} else
	    pSoundCurrent += cch;
    }

#ifdef SNDCTL_DSP_GETODELAY
    if( hSound >= 0 && !cbSound ) {
	/* we're waiting for the buffer to drain */
	int n;
	
	ioctl( hSound, SNDCTL_DSP_GETODELAY, &n );
	if( !n ) {
	    close( hSound );
	    hSound = -1;
	}
    }
#endif
	    
#ifdef ITIMER_REAL
    if( idSignal == SIGALRM && hSound < 0 ) {
	struct itimerval itv = {}; /* disable timer */
	setitimer( ITIMER_REAL, &itv, NULL );
    }
#endif
}

#endif

#ifndef WIN32

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

static void MonoToStereo( int nBitDepth, int fSigned, unsigned char **pbuf,
			  int *pcb ) {

    unsigned char *bufNew = malloc( *pcb << 1 );
    unsigned char *pDest = bufNew, *pSrc = *pbuf;
    int i;

    nBitDepth /= 8;
    
    for( i = 0; i < *pcb / nBitDepth; i++ ) {
	memcpy( pDest, pSrc, nBitDepth );
	pDest += nBitDepth;
	memcpy( pDest, pSrc, nBitDepth );
	pDest += nBitDepth;
	pSrc += nBitDepth;
    }
    
    free( *pbuf );
    *pbuf = bufNew;
    *pcb <<= 1;
}

static void StereoToMono( int nBitDepth, int fSigned, unsigned char **pbuf,
			  int *pcb ) {

    unsigned char *pDest = *pbuf, *pSrc = *pbuf;
    int i;

    nBitDepth /= 8;
    
    for( i = 0; i < *pcb / nBitDepth; i++ ) {
	/* FIXME this code merely keeps the left channel and discards the
	   right... it would be better to mix them, but then we have to
	   worry about endianness and signedness -- yuck. */
	memcpy( pDest, pSrc, nBitDepth );
	pDest += nBitDepth;
	pSrc += nBitDepth << 1;
    }
    
    *pbuf = realloc( *pbuf, *pcb >>= 1 );
}

static void play_audio_file(const char *file, const char *device) {

    /* here we can assume that we can write to /dev/audio */
    unsigned char *buf, tmp[ 256 ];
    struct stat info;
    int fd, n, cb, nSampleRate, nChannels, nBitDepth, fBigEndian;
    int fSigned; /* 0 = unsigned, 1 = signed, -1 = mu-law */
    int fMonoToStereo = FALSE, fStereoToMono = FALSE;
    
#ifdef SIGIO
    sigset_t ss, ssOld;

    sigemptyset( &ss );
    sigaddset( &ss, SIGIO );
    sigaddset( &ss, SIGALRM );
    sigprocmask( SIG_BLOCK, &ss, &ssOld );
  
    if( pSound ) {
	free( pSound );
	pSound = NULL;
	cbSound = 0;
	if( hSound >= 0 ) {
#ifdef SNDCTL_DSP_RESET
	    ioctl( hSound, SNDCTL_DSP_RESET, 0 );
#endif
	    close( hSound );
	    hSound = -1;
	}
    }

    sigprocmask( SIG_SETMASK, &ssOld, NULL );
#endif
  
    if( ( fd = open(file, O_RDONLY) ) < 0 )
	return;

    fstat( fd, &info );

    if( read( fd, tmp, 4 ) < 4 ) {
	close( fd );
	return;
    }

    if( !memcmp( tmp, ".snd", 4 ) ) {
	/* .au format */
	unsigned long nOffset;

	if( read( fd, tmp + 4, 20 ) < 20 ) {
	    close( fd );
	    return;
	}
	
	nOffset = ( tmp[ 4 ] << 24 ) | ( tmp[ 5 ] << 16 ) | ( tmp[ 6 ] << 8 ) |
	    tmp[ 7 ];
	nSampleRate = ( tmp[ 16 ] << 24 ) | ( tmp[ 17 ] << 16 ) |
	    ( tmp[ 18 ] << 8 ) | tmp[ 19 ];
	nChannels = tmp[ 23 ];
	fBigEndian = TRUE;
	switch( tmp[ 15 ] ) {
	case 1:
	    /* 8 bit mu-law */
	    nBitDepth = 8;
	    fSigned = -1;
	    break;
	    
	case 2:
	    /* 8 bit linear */
	    nBitDepth = 8;
	    fSigned = 1;
	    break;
	    
	case 3:
	    /* 16 bit linear */
	    nBitDepth = 16;
	    fSigned = 1;
	    break;
	    
	case 4:
	    /* 24 bit linear */
	    nBitDepth = 24;
	    fSigned = 1;
	    break;
	    
	case 5:
	    /* 32 bit linear */
	    nBitDepth = 32;
	    fSigned = 1;
	    break;
	    
	default:
	    /* unknown encoding */
	    close( fd );
	    return;
	}

	if( nOffset != 24 )
	    lseek( fd, nOffset, SEEK_SET );

	buf = malloc( cb = info.st_size - nOffset );
	if( ( cb = read( fd, buf, cb ) ) < 0 ) {
	    free( buf );
	    close( fd );
	    return;
	}
    } else if( !memcmp( tmp, "RIFF", 4 ) ) {
	long nLength;
	
	/* .wav format */
	if( read( fd, tmp, 8 ) < 8 ) {
	    close( fd );
	    return;
	}

	if( memcmp( tmp + 4, "WAVE", 4 ) ) {
	    /* it wasn't in .wav format after all -- whoops */
	    close( fd );
	    return;
	}

	while( 1 ) {
	    if( read( fd, tmp, 8 ) < 8 ) {
		close( fd );
		return;
	    }

	    if( ( nLength = ( tmp[ 7 ] << 24 ) | ( tmp[ 6 ] << 16 ) |
		  ( tmp[ 5 ] << 8 ) | tmp[ 4 ] ) < 0 ) {
		close( fd );
		return;
	    }

	    if( !memcmp( tmp, "fmt ", 4 ) )
		break;

	    lseek( fd, nLength, SEEK_CUR );
	}

	if( read( fd, tmp, 16 ) < 16 ) {
	    close( fd );
	    return;
	}

	if( tmp[ 0 ] != 1 ) {
	    /* unknown encoding */
	    close( fd );
	    return;
	}
	
	nSampleRate = ( tmp[ 7 ] << 24 ) | ( tmp[ 6 ] << 16 ) |
	    ( tmp[ 5 ] << 8 ) | tmp[ 4 ];
	nChannels = tmp[ 2 ];
	nBitDepth = tmp[ 14 ];
	fSigned = nBitDepth > 8;
	fBigEndian = FALSE;
	
	lseek( fd, nLength - 16, SEEK_CUR );
	
	while( 1 ) {
	    if( read( fd, tmp, 8 ) < 8 ) {
		close( fd );
		return;
	    }

	    if( ( nLength = ( tmp[ 7 ] << 24 ) | ( tmp[ 6 ] << 16 ) |
		  ( tmp[ 5 ] << 8 ) | tmp[ 4 ] ) < 0 ) {
		close( fd );
		return;
	    }

	    if( !memcmp( tmp, "data", 4 ) )
		break;

	    lseek( fd, nLength, SEEK_CUR );
	}
	
	buf = malloc( cb = nLength );
	cb = nLength;
	if( ( cb = read( fd, buf, cb ) ) < 0 ) {
	    free( buf );
	    close( fd );
	    return;
	}
    } else {
	/* unknown format -- ignore */
	close( fd );
	return;
    }
    
    if( ( fd = open(device, O_WRONLY | O_NDELAY) ) < 0 ) {
	free(buf);
	return;
    }

#if defined( SNDCTL_DSP_SETFMT )
    {
	int n, nDesired;
	
	if( fSigned < 0 )
	    n = AFMT_MU_LAW;
	else if( fSigned ) {
	    if( nBitDepth == 8 )
		n = AFMT_S8;
	    else if( fBigEndian )
		n = AFMT_S16_BE;
	    else
		n = AFMT_S16_LE;
	} else {
	    if( nBitDepth == 8 )
		n = AFMT_U8;
	    else if( fBigEndian )
		n = AFMT_U16_BE;
	    else
		n = AFMT_U16_LE;
	}

	ioctl( fd, SNDCTL_DSP_SETFMT, &n );
	/* FIXME check for re-encoding */
	nDesired = nChannels;
	ioctl( fd, SNDCTL_DSP_CHANNELS, &nChannels );
	fMonoToStereo = nChannels == 2 && nDesired == 1;
	fStereoToMono = nChannels == 1 && nDesired == 2;
	ioctl( fd, SNDCTL_DSP_SPEED, &nSampleRate );
	/* FIXME check for resampling */
    }
#elif defined( AUDIO_SETINFO )
    {
	struct audio_info_t ait;

	AUDIO_INITINFO( &ait );
	ait.play.sample_rate = nSampleRate;
	ait.play.channels = nChannels;
	ait.play.precision = nBitDepth;
	ait.play.encoding = fSigned < 0 ? AUDIO_ENCODING_ULAW :
	    AUDIO_ENCODING_LINEAR;
	ioctl( fd, AUDIO_SETINFO, &ait );
	/* FIXME check for re-encoding or resampling */
    }
#endif

    if( fMonoToStereo )
	MonoToStereo( nBitDepth, fSigned, &buf, &cb );
    
    if( fStereoToMono )
	StereoToMono( nBitDepth, fSigned, &buf, &cb );

#ifdef SIGIO
    sigemptyset( &ss );
    sigaddset( &ss, SIGIO );
    sigaddset( &ss, SIGALRM );
    sigprocmask( SIG_BLOCK, &ss, &ssOld );
    
    pSound = pSoundCurrent = buf;
    cbSound = cb;
    hSound = fd;
    
#if O_ASYNC
    /* BSD O_ASYNC-style I/O notification */
    if( ( n = fcntl( fd, F_GETFL ) ) != -1 ) {
	fcntl( fd, F_SETOWN, getpid() );
	fcntl( fd, F_SETFL, n | O_ASYNC | O_NONBLOCK );
    }
#else
    /* System V SIGPOLL-style I/O notification */
    if( ( n = fcntl( fd, F_GETFL ) ) != -1 )
	fcntl( fd, F_SETFL, n | O_NONBLOCK );
    ioctl( fd, I_SETSIG, S_OUTPUT );
#endif
    
#ifdef ITIMER_REAL
    {
	/* Unfortunately we can't rely on a SIGIO being generated for the
	   audio device on all systems (e.g. GNU/Linux does not), so we
	   also add an interval timer to make sure we keep the buffer full. */
	struct itimerval itv;

	if( !fHandler ) {
	    fHandler = TRUE;
	    PortableSignal( SIGALRM, SoundSIGIO, NULL, TRUE );
	}
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 20000; /* 50Hz */
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 20000; /* 50Hz */
	setitimer( ITIMER_REAL, &itv, NULL );
    }
#endif

    SoundSIGIO( -1 );
    
    sigprocmask( SIG_SETMASK, &ssOld, NULL );
#else
    write(fd, buf, cb);
    free(buf);
    close(fd);
#endif
}

static char *can_play_audio() {

    static char *asz[] = { "/dev/dsp", "/dev/audio", NULL };
    char **ppch;

    for( ppch = asz; *ppch; ppch++ )
	if( check_dev( *ppch ) )
	    return *ppch;

    return NULL;
}
#endif  /* #ifndef WIN32 */

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

#endif /* HAVE_NAS */

static void 
play_file_child(const char *filename) {

    switch ( ssSoundSystem ) {

    case SOUND_SYSTEM_COMMAND:
#ifndef WIN32
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
#else
      assert( FALSE );
#endif
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

      if (play_nas_file(filename))
        _exit(0);

#else
      assert ( FALSE );
#endif
      
      break;

    case SOUND_SYSTEM_NORMAL:
#ifndef WIN32
    {
	char *pch;
	if( ( pch = can_play_audio() ) ) {
	    play_audio_file(filename, pch);
#ifndef SIGIO
	    _exit(0);
#endif
	}
    }
#else
      assert( FALSE );
#endif
      break;

    case SOUND_SYSTEM_WINDOWS:

#ifdef WIN32
      PlaySound ( filename, NULL, SND_FILENAME | SND_ASYNC );
#else
      assert ( FALSE );
#endif
      break;

    default:

      break;

    }

}

static void 
play_file(const char *filename) {

#ifndef WIN32

  int pid;

#ifdef SIGIO
  if( ssSoundSystem == SOUND_SYSTEM_NORMAL) {
      /* we can play directly without forking */
      play_file_child( filename );
      return;
  }
#endif
  
  /* fork, so we don't have to wait for the sound to finish */
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

    play_file_child( filename );
    
#ifndef WIN32

    _exit(0);

  }

#else

  /* we didn't fork */

#endif

}

extern void
playSound ( const gnubgsound gs ) {

    char *szFile;
    
    if ( ! fSound )
	/* no sounds for this user */
	return;
    
    if ( ! *aszSound[ gs ] )
	/* no sound defined for event */
	return;

    szFile = PathSearch( aszSound[ gs ], szDataDirectory );
    play_file( szFile );
    free( szFile );
}


extern void SoundWait( void ) {

    switch( ssSoundSystem ) {
#ifdef SIGIO
    case SOUND_SYSTEM_NORMAL:
    {
	sigset_t ss, ssOld;

	sigemptyset( &ss );
	sigaddset( &ss, SIGIO );
	sigaddset( &ss, SIGALRM );
	sigprocmask( SIG_BLOCK, &ss, &ssOld );
	
	while( hSound >= 0 )
	    sigsuspend( &ssOld );
	
	sigprocmask( SIG_SETMASK, &ss, &ssOld );
	
	return;
    }
#endif
    default:
	return;
    }
}
