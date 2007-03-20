/*
 * sound.h
 *
 * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002
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
 * $Id$
 */

#ifndef _SOUND_H_
#define _SOUND_H_


typedef enum _gnubgsound {
  /* start & exit of gnubg */
  SOUND_START = 0,     
  /* commands */
  SOUND_AGREE,
  SOUND_DOUBLE,
  SOUND_DROP,
  SOUND_CHEQUER,
  SOUND_MOVE,
  SOUND_REDOUBLE,
  SOUND_RESIGN,
  SOUND_ROLL,
  SOUND_TAKE,
  /* events */
  SOUND_HUMAN_DANCE,
  SOUND_HUMAN_WIN_GAME,
  SOUND_HUMAN_WIN_MATCH,
  SOUND_BOT_DANCE,
  SOUND_BOT_WIN_GAME,
  SOUND_BOT_WIN_MATCH,
  SOUND_ANALYSIS_FINISHED,
  /* number of sounds */
#if USE_TIMECONTROL
  SOUND_HUMAN_TIMEOUT,
  SOUND_BOT_TIMEOUT,
#endif
  NUM_SOUNDS
} gnubgsound;

extern char *aszSoundDesc[ NUM_SOUNDS ];
extern char *aszSoundCommand[ NUM_SOUNDS ];

extern char szSoundCommand[ 80 ];

extern int fSound;

extern void
playSound ( const gnubgsound gs );
extern void SoundFlushCache( const gnubgsound gs );
extern void SoundWait( void );

extern char *GetDefaultSoundFile(int sound);
extern void playSoundFile(char *file);
extern void SetSoundFile(const gnubgsound gs, const char *szFilename);
extern char *GetSoundFile(gnubgsound sound);
extern char *sound_get_command(void);
extern char *sound_set_command(const char *sz);

#endif
