/*
 * backgammon.h
 *
 * by Gary Wong, 1999
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

#if !X_DISPLAY_MISSING
#include <ext.h>
#include <event.h>
extern extwindow ewnd;
extern int fX, nDelay;
extern event evNextTurn;
#endif

#include "eval.h"

#define MAX_CUBE ( 1 << 12 )

typedef struct _command {
    char *sz;
    void ( *pf )( char * );
    char *szHelp;
    struct _command *pc;
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
    MOVE_NORMAL, MOVE_DOUBLE, MOVE_TAKE, MOVE_DROP, MOVE_RESIGN
} movetype;

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

typedef union _moverecord {
    movetype mt;
    movenormal n;
    moveresign r;
} moverecord;

extern char *aszGameResult[], szDefaultPrompt[], *szPrompt;

extern int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn, fDisplay,
  fAutoBearoff, fAutoGame, fAutoMove, fResigned, fDoubled, 
  cGames, fAutoRoll, fAutoCrawford, cAutoDoubles,
  fCubeUse, fNackgammon, fVarRedn, nRollouts, nRolloutTruncate,
  fNextTurn, fConfirm;  

extern evalcontext ecEval, ecRollout, ecTD;

extern list lMatch, *plGame; /* (list of) list of moverecords */

extern player ap[ 2 ];

extern char *GetInput( char *szPrompt );
extern void HandleCommand( char *sz, command *ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ] );
extern char *NextToken( char **ppch );
extern void NextTurn( void );
extern void TurnDone( void );
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char *sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char *sz );
extern double ParseReal( char **ppch );
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff );
extern void ShowBoard( void );
extern void SetCube ( int nNewCube, int fNewCubeOwner );

#if !X_DISPLAY_MISSING
extern void UserCommand( char *sz );
#endif

extern void CommandAccept( char * ),
    CommandAgree( char * ),
    CommandDatabaseDump( char * ),
    CommandDatabaseRollout( char * ),
    CommandDatabaseGenerate( char * ),
    CommandDatabaseTrain( char * ),
    CommandDecline( char * ),
    CommandDouble( char * ),
    CommandDrop( char * ),
    CommandEval( char * ),
    CommandHelp( char * ),
    CommandHint( char * ),
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
    CommandSaveMatch( char * ),
    CommandSaveWeights( char * ),
    CommandSetAutoBearoff( char * ),
    CommandSetAutoCrawford( char * ),
    CommandSetAutoDoubles( char * ),
    CommandSetAutoGame( char * ),
    CommandSetAutoMove( char * ),
    CommandSetAutoRoll( char * ),
    CommandSetJacoby( char * ),
    CommandSetCrawford( char * ),
    CommandSetPostCrawford( char * ),
    CommandSetBoard( char * ),
    CommandSetCache( char * ),
    CommandSetCubeCentre( char * ),
    CommandSetCubeOwner( char * ),
    CommandSetCubeUse( char * ),
    CommandSetCubeValue( char * ),
    CommandSetDelay( char * ),
    CommandSetDice( char * ),
    CommandSetDisplay( char * ),
    CommandSetEvalCandidates( char * ),
    CommandSetEvalPlies( char * ),
    CommandSetEvalTolerance( char * ),
    CommandSetEvaluation( char * ),
    CommandSetNackgammon( char * ),
    CommandSetPlayerEvaluation( char * ),
    CommandSetPlayerGNU( char * ),
    CommandSetPlayerHuman( char * ),
    CommandSetPlayerName( char * ),
    CommandSetPlayerPlies( char * ),
    CommandSetPlayerPubeval( char * ),
    CommandSetPlayer( char * ),
    CommandSetPrompt( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGIsaac( char * ),
    CommandSetRNGManual( char * ),
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
    CommandShowCache( char * ),
    CommandShowCopying( char * ),
    CommandShowCrawford( char * ),
    CommandShowDice( char * ),
    CommandShowEvaluation( char * ),
    CommandShowJacoby( char * ),
    CommandShowPipCount( char * ),
    CommandShowPostCrawford( char * ),
    CommandShowPlayer( char * ),
    CommandShowRNG( char * ),
    CommandShowRollout( char * ),
    CommandShowScore( char * ),
    CommandShowTurn( char * ),
    CommandShowWarranty( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * );
#endif
