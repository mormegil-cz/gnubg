/*
 * backgammon.h
 *
 * by Gary Wong, 1999
 *
 * $Id$
 */

#ifndef _BACKGAMMON_H_
#define _BACKGAMMON_H_

#if !X_DISPLAY_MISSING
#include <ext.h>
extern extwindow ewnd;
extern int fX;
#endif

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
    int nPlies;
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

extern int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn, fDisplay, fAutoBearoff,
    fAutoGame, fAutoMove, fResigned, fMove, fDoubled, nPliesEval, anScore[ 2 ],
    cGames, nCube, fCubeOwner, fAutoRoll, nMatchTo, fJacoby, fCrawford,
    fPostCrawford, fAutoCrawford, cAutoDoubles, fCubeUse;

extern list lMatch, *plGame; /* (list of) list of moverecords */

extern player ap[ 2 ];

extern void HandleCommand( char *sz, command *ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ] );
extern char *NextToken( char **ppch );
extern void NextTurn( void );
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char *sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char *sz );
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff );
extern void ShowBoard( void );
extern void SetCube ( int nNewCube, int fNewCubeOwner );
extern void 
CalcGammonPrice ( int nCube, int fCubeOwner ); 


extern void CommandAccept( char * ),
    CommandAgree( char * ),
    CommandDatabaseDump( char * ),
    CommandDatabaseEvaluate( char * ),
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
    CommandSetDice( char * ),
    CommandSetDisplay( char * ),
    CommandSetPlayerGNU( char * ),
    CommandSetPlayerHuman( char * ),
    CommandSetPlayerName( char * ),
    CommandSetPlayerPlies( char * ),
    CommandSetPlayerPubeval( char * ),
    CommandSetPlayer( char * ),
    CommandSetPlies( char * ),
    CommandSetPrompt( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGIsaac( char * ),
    CommandSetRNGManual( char * ),
    CommandSetRNGMersenne( char * ),
    CommandSetRNGUser( char * ),
    CommandSetScore( char * ),
    CommandSetSeed( char * ),
    CommandSetTurn( char * ),
    CommandShowBoard( char * ),
    CommandShowCache( char * ),
    CommandShowDice( char * ),
    CommandShowPipCount( char * ),
    CommandShowPlayer( char * ),
    CommandShowScore( char * ),
    CommandShowTurn( char * ),
    CommandShowRNG( char * ),
    CommandShowJacoby( char * ),
    CommandShowCrawford( char * ),
    CommandShowPostCrawford( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * );
#endif
