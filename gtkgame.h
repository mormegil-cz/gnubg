/*
 * gtkgame.h
 *
 * by Gary Wong <gtw@gnu.org>, 2000.
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

#ifndef _GTKGAME_H_
#define _GTKGAME_H_

#include "backgammon.h"

typedef enum _dialogarea {
    DA_MAIN,
    DA_BUTTONS
} dialogarea;

extern GtkWidget *pwMain;

extern int fEndDelay;

extern void ShowGameWindow( void );

extern void GTKAddMoveRecord( moverecord *pmr );
extern void GTKPopMoveRecord( moverecord *pmr );
extern void GTKSetMoveRecord( moverecord *pmr );
extern void GTKClearMoveRecord( void );

extern void GTKAddGame( char *sz );
extern void GTKPopGame( int c );
extern void GTKSetGame( int i );

extern int InitGTK( int *argc, char ***argv );
extern void RunGTK( void );
extern void GTKAllowStdin( void );
extern void GTKDisallowStdin( void );
extern void GTKDelay( void );
extern void ShowList( char *asz[], char *szTitle );

extern GtkWidget *CreateDialog( char *szTitle, int fQuestion, GtkSignalFunc pf,
				void *p );
extern GtkWidget *DialogArea( GtkWidget *pw, dialogarea da );
    
extern int GTKGetInputYN( char *szPrompt );
extern void GTKOutput( char *sz );
extern void GTKOutputX( void );
extern void GTKOutputNew( void );
extern void GTKEval( char *szOutput );
extern void GTKHint( movelist *pml );
extern void GTKRollout( int c, char asz[][ 40 ], int cGames );
extern void GTKRolloutRow( int i );
extern int GTKRolloutUpdate( float arMu[], float arSigma[], int i, int c );
extern void GTKRolloutDone( void );
extern void GTKSet( void *p );
extern void GTKShowMatchEquityTable( int n );
extern int GTKGetManualDice( int an[ 2 ] );
extern void GTKShowVersion( void );

extern int fTTY;

#endif
