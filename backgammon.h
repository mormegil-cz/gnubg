/*
 * backgammon.h
 *
 * by Gary Wong, 1999, 2000.
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

#ifndef _BACKGAMMON_H_
#define _BACKGAMMON_H_

#include <stdarg.h>
#include <list.h>
#include "eval.h"

#if !defined (__GNUC__) && !defined (__attribute__)
#define __attribute__(X)
#endif

#if USE_GTK
#include <gtk/gtk.h>
extern GtkWidget *pwMain, *pwBoard;
extern int fX, nDelay, fNeedPrompt;
extern guint nNextTurn; /* GTK idle function */
#define DISPLAY GDK_DISPLAY()
#elif USE_EXT
#include <ext.h>
#include <event.h>
extern extwindow ewnd;
extern int fX, nDelay, fNeedPrompt;
extern event evNextTurn;
#define DISPLAY ewnd.pdsp
#endif

#define MAX_CUBE ( 1 << 12 )

typedef struct _command {
    char *sz; /* Command name (NULL indicates end of list) */
    void ( *pf )( char * ); /* Command handler; NULL to use default
			       subcommand handler */
    char *szHelp, *szUsage; /* Documentation */
    struct _command *pc; /* List of subcommands (NULL if none) */
} command;

typedef enum _playertype {
    PLAYER_HUMAN, PLAYER_GNU, PLAYER_PUBEVAL
} playertype;

typedef struct _player {
    char szName[ 32 ];
    playertype pt;
    evalcontext ec;
} player;

typedef enum _movetype {
    MOVE_GAMEINFO, MOVE_NORMAL, MOVE_DOUBLE, MOVE_TAKE, MOVE_DROP, MOVE_RESIGN,
    MOVE_SETBOARD, MOVE_SETCUBEVAL, MOVE_SETCUBEPOS
} movetype;

typedef struct _movegameinfo {
    movetype mt;
    int i, /* the number of the game within a match */
	nMatch, /* match length */
	anScore[ 2 ], /* match score BEFORE the game */
	fCrawford, /* the Crawford rule applies during this match */
	fCrawfordGame, /* this is the Crawford game */
	fJacoby,
	fWinner, /* who won (-1 = unfinished) */
	nPoints, /* how many points were scored by the winner */
	fResigned, /* the game was ended by resignation */
	nAutoDoubles; /* how many automatic doubles were rolled */
} movegameinfo;

typedef struct _movenormal {
    movetype mt;
    int fPlayer;
    int anRoll[ 2 ];
    int anMove[ 8 ];
} movenormal;

typedef struct _moveresign {
    movetype mt;
    int fPlayer;
    int nResigned;
} moveresign;

typedef struct _movesetboard {
    movetype mt;
    unsigned char auchKey[ 10 ]; /* always stored as if player 0 was on roll */
} movesetboard;

typedef struct _movesetcubeval {
    movetype mt;
    int nCube;
} movesetcubeval;

typedef struct _movesetcubepos {
    movetype mt;
    int fCubeOwner;
} movesetcubepos;

typedef union _moverecord {
    movetype mt;
    movegameinfo g;
    movenormal n;
    moveresign r;
    movesetboard sb;
    movesetcubeval scv;
    movesetcubepos scp;
} moverecord;

extern char *aszGameResult[], szDefaultPrompt[], *szPrompt;

extern int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn, fDisplay, fAutoBearoff,
    fAutoGame, fAutoMove, fResigned, fDoubled, 
    cGames, fAutoRoll,
    fAutoCrawford, cAutoDoubles, fCubeUse, fNackgammon,
    fVarRedn, nRollouts, nRolloutTruncate, fNextTurn, fConfirm,
    fShowProgress;

extern evalcontext ecEval, ecRollout, ecTD;

extern list lMatch, *plGame; /* (list of) list of moverecords */

extern player ap[ 2 ];

extern char *GetInput( char *szPrompt );
extern int GetInputYN( char *szPrompt );
extern void HandleCommand( char *sz, command *ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ] );
extern char *NextToken( char **ppch );
extern void NextTurn( void );
extern void TurnDone( void );
extern void CancelCubeAction( void );
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char *sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char *sz );
extern double ParseReal( char **ppch );
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff );
extern void ShowBoard( void );
extern char *FormatPrompt( void );
extern void UpdateSetting( void *p );
extern void ResetInterrupt( void );
extern void PromptForExit( void );
extern void Prompt( void );

/* Write a string to stdout/status bar/popup window */
extern void output( char *sz );
/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( char *sz );
/* Write a character to stdout/status bar/popup window */
extern void outputc( char ch );
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( char *sz, ... ) __attribute__((format(printf,1,2)));
/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( char *sz, va_list val )
    __attribute__((format(printf,1,0)));
/* Signifies that all output for the current command is complete */
extern void outputx( void );
/* Signifies that subsequent output is for a new command */
extern void outputnew( void );
/* Disable output */
extern void outputoff( void );
/* Enable output */
extern void outputon( void );

#if USE_GUI
#if USE_GTK
extern gint NextTurnNotify( gpointer p );
#else
extern int NextTurnNotify( event *pev, void *p );
#endif
extern void UserCommand( char *sz );
extern void HandleXAction( void );
#if HAVE_LIBREADLINE
extern int fReadingCommand;
extern void HandleInput( char *sz );
#endif
#endif

extern command acDatabase[], acNew[], acSave[], acSetAutomatic[],
    acSetCube[], acSetEvaluation[], acSetPlayer[], acSetRNG[], acSetRollout[],
    acSet[], acShow[], acTrain[], acTop[], acSetMET[];

extern void CommandAccept( char * ),
    CommandAgree( char * ),
    CommandAnalysis ( char * ),
    CommandDatabaseDump( char * ),
    CommandDatabaseRollout( char * ),
    CommandDatabaseGenerate( char * ),
    CommandDatabaseTrain( char * ),
    CommandDecline( char * ),
    CommandDouble( char * ),
    CommandDrop( char * ),
    CommandEval( char * ),
    CommandExportGame( char * ),
    CommandExportMatch( char * ),
    CommandHelp( char * ),
    CommandHint( char * ),
    CommandLoadCommands( char * ),
    CommandLoadGame( char * ),
    CommandLoadMatch( char * ),
    CommandMove( char * ),
    CommandNewGame( char * ),
    CommandNewMatch( char * ),
    CommandNewSession( char * ),
    CommandNotImplemented( char * ),
    CommandPlay( char * ),
    CommandQuit( char * ),
    CommandRedouble( char * ),
    CommandReject( char * ),
    CommandResign( char * ),
    CommandRoll( char * ),
    CommandRollout( char * ),
    CommandSaveGame( char * ),
    CommandSaveMatch( char * ),
    CommandSaveSettings( char * ),
    CommandSaveWeights( char * ),
    CommandSetAutoBearoff( char * ),
    CommandSetAutoCrawford( char * ),
    CommandSetAutoDoubles( char * ),
    CommandSetAutoGame( char * ),
    CommandSetAutoMove( char * ),
    CommandSetAutoRoll( char * ),
    CommandSetBoard( char * ),
    CommandSetBeavers( char * ),
    CommandSetCache( char * ),
    CommandSetConfirm( char * ),
    CommandSetCrawford( char * ),
    CommandSetCubeCentre( char * ),
    CommandSetCubeOwner( char * ),
    CommandSetCubeUse( char * ),
    CommandSetCubeValue( char * ),
    CommandSetDelay( char * ),
    CommandSetDice( char * ),
    CommandSetDisplay( char * ),
    CommandSetEvalCandidates( char * ),
    CommandSetEvalConsistency( char * ),
    CommandSetEvalPlies( char * ),
    CommandSetEvalReduced ( char * ),
    CommandSetEvalTolerance( char * ),
    CommandSetEvaluation( char * ),
    CommandSetJacoby( char * ),
    CommandSetMETZadeh( char * ),
    CommandSetMETWoolsey( char * ),
    CommandSetMETSnowie( char * ),
    CommandSetMETJacobs( char * ),
    CommandSetJacoby( char * ),
    CommandSetNackgammon( char * ),
    CommandSetOutputMWC ( char * ),
    CommandSetPlayerEvaluation( char * ),
    CommandSetPlayerGNU( char * ),
    CommandSetPlayerHuman( char * ),
    CommandSetPlayerName( char * ),
    CommandSetPlayerPlies( char * ),
    CommandSetPlayerPubeval( char * ),
    CommandSetPlayer( char * ),
    CommandSetPostCrawford( char * ),
    CommandSetPrompt( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGIsaac( char * ),
    CommandSetRNGManual( char * ),
    CommandSetRNGMD5( char * ),
    CommandSetRNGMersenne( char * ),
    CommandSetRNGUser( char * ),
    CommandSetRolloutEvaluation( char * ),
    CommandSetRolloutTrials( char * ),
    CommandSetRolloutTruncation( char * ),
    CommandSetRolloutVarRedn( char * ),
    CommandSetScore( char * ),
    CommandSetSeed( char * ),
    CommandSetTurn( char * ),
    CommandShowAutomatic( char * ),
    CommandShowBoard( char * ),
    CommandShowBeavers( char * ),
    CommandShowCache( char * ),
    CommandShowConfirm( char * ),
    CommandShowCopying( char * ),
    CommandShowCrawford( char * ),
    CommandShowCube( char * ),
    CommandShowDelay( char * ),
    CommandShowDice( char * ),
    CommandShowDisplay( char * ),
    CommandShowEvaluation( char * ),
    CommandShowJacoby( char * ),
    CommandShowGammonPrice( char * ),
    CommandShowNackgammon( char * ),
    CommandShowMatchEquityTable( char * ),
    CommandShowOutputMWC ( char * ),
    CommandShowPipCount( char * ),
    CommandShowPostCrawford( char * ),
    CommandShowPlayer( char * ),
    CommandShowPrompt( char * ),
    CommandShowRNG( char * ),
    CommandShowRollout( char * ),
    CommandShowScore( char * ),
    CommandShowSeed( char * ),
    CommandShowTurn( char * ),
    CommandShowWarranty( char * ),
    CommandShowKleinman( char * ),
    CommandShowThorp( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * );
#endif
