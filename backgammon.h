/*
 * backgammon.h
 *
 * by Gary Wong <gtw@gnu.org>, 1999, 2000, 2001, 2002.
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

#include <float.h>
#include <list.h>
#include <math.h>
#include <stdarg.h>

#include "analysis.h"
#include "eval.h"

#if !defined (__GNUC__) && !defined (__attribute__)
#define __attribute__(X)
#endif

#ifdef HUGE_VALF
#define ERR_VAL (-HUGE_VALF)
#elif defined (HUGE_VAL)
#define ERR_VAL (-HUGE_VAL)
#else
#define ERR_VAL (-FLT_MAX)
#endif

#if USE_GTK
#include <gtk/gtk.h>
extern GtkWidget *pwBoard;
extern int fX, nDelay, fNeedPrompt;
extern guint nNextTurn; /* GTK idle function */
#elif USE_EXT
#include <ext.h>
#include <event.h>
extern extwindow ewnd;
extern int fX, nDelay, fNeedPrompt;
extern event evNextTurn;
#endif

#if HAVE_SIGACTION
typedef struct sigaction psighandler;
#elif HAVE_SIGVEC
typedef struct sigvec psighandler;
#else
typedef RETSIGTYPE (*psighandler)( int );
#endif

#define MAX_CUBE ( 1 << 12 )
#define MAX_CUBE_STR "4096"

typedef struct _command {
    char *sz; /* Command name (NULL indicates end of list) */
    void ( *pf )( char * ); /* Command handler; NULL to use default
			       subcommand handler */
    char *szHelp, *szUsage; /* Documentation; NULL for abbreviations */
    struct _command *pc; /* List of subcommands (NULL if none) */
} command;

typedef enum _playertype {
    PLAYER_EXTERNAL, PLAYER_HUMAN, PLAYER_GNU, PLAYER_PUBEVAL
} playertype;

typedef struct _player {
    /* For all player types: */
    char szName[ 32 ];
    playertype pt;
    /* For PLAYER_GNU: */
    evalsetup esChequer, esCube;
    int h;
    /* For PLAYER_EXTERNAL: */
    char *szSocket;
} player;

typedef enum _movetype {
    MOVE_GAMEINFO, MOVE_NORMAL, MOVE_DOUBLE, MOVE_TAKE, MOVE_DROP, MOVE_RESIGN,
    MOVE_SETBOARD, MOVE_SETDICE, MOVE_SETCUBEVAL, MOVE_SETCUBEPOS
} movetype;

typedef struct _movegameinfo {
    movetype mt;
    char *sz;
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
    statcontext sc;
} movegameinfo;

typedef struct _movedouble {
    movetype mt;
    char *sz;
    int fPlayer;
    /* evaluation of cube action */
    float arDouble[ 4 ];
    evalsetup esDouble;
    skilltype st;
} movedouble;

typedef struct _movenormal {
    movetype mt;
    char *sz;
    int fPlayer;
    int anRoll[ 2 ];
    int anMove[ 8 ];
    /* evaluation setup for move analysis */
    evalsetup esChequer;
    /* evaluation of cube action before this move */
    float arDouble[ 4 ];
    evalsetup esDouble;
    /* evaluation of the moves */
    movelist ml;
    int iMove; /* index into the movelist of the move that was made */
    lucktype lt;
    float rLuck; /* ERR_VAL means unknown */
    skilltype st;
} movenormal;

typedef struct _moveresign {
    movetype mt;
    char *sz;
    int fPlayer;
    int nResigned;

    evalsetup esResign;
    float arResign[ NUM_ROLLOUT_OUTPUTS ];

    skilltype stResign;
    skilltype stAccept;
} moveresign;

typedef struct _movesetboard {
    movetype mt;
    char *sz;
    unsigned char auchKey[ 10 ]; /* always stored as if player 0 was on roll */
} movesetboard;

typedef struct _movesetdice {
    movetype mt;
    char *sz;
    int fPlayer;
    int anDice[ 2 ];
    lucktype lt;
    float rLuck; /* ERR_VAL means unknown */
} movesetdice;

typedef struct _movesetcubeval {
    movetype mt;
    char *sz;
    int nCube;
} movesetcubeval;

typedef struct _movesetcubepos {
    movetype mt;
    char *sz;
    int fCubeOwner;
} movesetcubepos;

typedef union _moverecord {
    movetype mt;
    struct _moverecordall {
	movetype mt;
	char *sz;
    } a;
    movegameinfo g;
    movedouble d; /* cube decisions */
    movenormal n;
    moveresign r;
    movesetboard sb;
    movesetdice sd;
    movesetcubeval scv;
    movesetcubepos scp;
} moverecord;

extern char *aszGameResult[], szDefaultPrompt[], *szPrompt;

typedef enum _gamestate {
    GAME_NONE, GAME_PLAYING, GAME_OVER, GAME_RESIGNED, GAME_DROP
} gamestate; 

/* The match state is represented by the board position (anBoard),
   fTurn (indicating which player makes the next decision), fMove
   (which indicates which player is on roll: normally the same as
   fTurn, but occasionally different, e.g. if a double has been
   offered).  anDice indicate the roll to be played (0,0 indicates the
   roll has not been made). */
typedef struct _matchstate {
    int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn, fResigned,
	fResignationDeclined, fDoubled, cGames, fMove, fCubeOwner, fCrawford,
	fPostCrawford, nMatchTo, anScore[ 2 ], nCube, cBeavers;
    gamestate gs;
} matchstate;

extern matchstate ms;
extern int fNextTurn, fComputing;

/* User settings. */
extern int fAutoGame, fAutoMove, fAutoRoll, fAutoCrawford, cAutoDoubles,
    fCubeUse, fNackgammon, fConfirm, fDisplay, fAutoBearoff, fShowProgress,
    nBeavers, fOutputMWC, fEgyptian, fOutputWinPC, fOutputMatchPC, fJacoby,
    fOutputRawboard, fAnnotation, cAnalysisMoves, fAnalyseCube,
    fAnalyseDice, fAnalyseMove, fRecord;
extern float rAlpha, rAnneal, rThreshold, arLuckLevel[ LUCK_VERYGOOD + 1 ],
    arSkillLevel[ SKILL_VERYGOOD + 1 ];

extern evalcontext ecTD;

extern evalsetup esEvalCube, esEvalChequer;
extern evalsetup esAnalysisCube, esAnalysisChequer;

extern rolloutcontext rcRollout;

/* plGame is the list of moverecords representing the current game;
   plLastMove points to a move within it (typically the most recently
   one played, but "previous" and "next" commands navigate back and forth).
   lMatch is a list of games (i.e. a list of list of moverecords),
   and plGame points to a game within it (again, typically the last). */
extern list lMatch, *plGame, *plLastMove;
extern statcontext scMatch;

/* There is a global storedmoves struct to maintain the list of moves
   for "=n" notation (e.g. "hint", "rollout =1 =2 =4").

   Anything that _writes_ stored moves ("hint", "show moves", "add move")
   should free the old dynamic move list first (sm.ml.amMoves), if it is
   non-NULL.

   Anything that _reads_ stored moves should check that the move is still
   valid (i.e. auchKey matches the current board and anDice matches the
   current dice). */
typedef struct _storedmoves {
    movelist ml;
    unsigned char auchKey[ 10 ];
    int anDice[ 2 ];
} storedmoves;
extern storedmoves sm;

extern player ap[ 2 ];

extern char *GetInput( char *szPrompt );
extern int GetInputYN( char *szPrompt );
extern void HandleCommand( char *sz, command *ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ] );
extern char *NextToken( char **ppch );
extern int NextTurn( int fPlayNext );
extern void TurnDone( void );
extern void AddMoveRecord( void *pmr );
extern void ApplyMoveRecord( matchstate *pms, moverecord *pmr );
extern void SetMoveRecord( void *pmr );
extern void ClearMoveRecord( void );
extern void AddGame( moverecord *pmr );
extern void ChangeGame( list *plGameNew );
extern void CalculateBoard( void );
extern void CancelCubeAction( void );
extern int ComputerTurn( void );
extern void ClearMatch( void );
extern void FreeMatch( void );
extern int GetMatchStateCubeInfo( cubeinfo *pci, matchstate *pms );
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char *sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char **ppch, char *pchDesc );
extern double ParseReal( char **ppch );
extern int ParseKeyValue( char **ppch, char *apch[ 2 ] );
extern int CompareNames( char *sz0, char *sz1 );
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff );
extern void ShowBoard( void );
extern void SetMatchID ( char *szMatchID );
extern char*
FormatCubePosition ( char *sz, cubeinfo *pci );
extern char *FormatPrompt( void );
extern char *FormatMoveHint( char *sz, matchstate *pms, movelist *pml,
			     int i, int fRankKnown );
extern void UpdateSetting( void *p );
extern void UpdateSettings( void );
extern void ResetInterrupt( void );
extern void PromptForExit( void );
extern void Prompt( void );

extern void PortableSignal( int nSignal, RETSIGTYPE (*p)(int),
			    psighandler *pOld, int fRestart );
extern void PortableSignalRestore( int nSignal, psighandler *p );
extern RETSIGTYPE HandleInterrupt( int idSignal );

/* Like strncpy, except it does the right thing */
extern char *strcpyn( char *szDest, const char *szSrc, int cch );

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
/* Temporarily disable outputx() calls */
extern void outputpostpone( void );
/* Re-enable outputx() calls */
extern void outputresume( void );
/* Signifies that subsequent output is for a new command */
extern void outputnew( void );
/* Disable output */
extern void outputoff( void );
/* Enable output */
extern void outputon( void );

extern void ProgressStart( char *sz );
extern void ProgressStartValue( char *sz, int iMax );
extern void Progress( void );
extern void ProgressValue ( int iValue );
extern void ProgressValueAdd ( int iValue );
extern void ProgressEnd( void );

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

#if HAVE_LIBREADLINE
extern int fReadline;
#endif

#ifdef WIN32
extern void WinCopy( char *szOut );
#endif

extern int iProgressMax, iProgressValue;
extern char *pcProgress;

extern char *aszVersion[], *szHomeDirectory;

extern char *aszSkillType[], *aszSkillTypeAbbr[], *aszLuckType[],
    *aszLuckTypeAbbr[];

extern command acDatabase[], acNew[], acSave[], acSetAutomatic[],
    acSetCube[], acSetEvaluation[], acSetPlayer[], acSetRNG[], acSetRollout[],
    acSet[], acShow[], acTrain[], acTop[], acSetMET[], acSetEvalParam[],
    acSetRolloutPlayer[], cOnOff, cFilename;

extern void CommandAccept( char * ),
    CommandAgree( char * ),
    CommandAnalyseGame( char * ),
    CommandAnalyseMatch( char * ),
    CommandAnalyseSession( char * ),
    CommandAnnotateBad( char * ),
    CommandAnnotateClearComment( char * ),
    CommandAnnotateClearLuck( char * ),
    CommandAnnotateClearSkill( char * ),
    CommandAnnotateDoubtful( char * ),
    CommandAnnotateGood( char * ),
    CommandAnnotateInteresting( char * ),
    CommandAnnotateLucky( char * ),
    CommandAnnotateUnlucky( char * ),
    CommandAnnotateVeryBad( char * ),
    CommandAnnotateVeryGood( char * ),
    CommandAnnotateVeryLucky( char * ),
    CommandAnnotateVeryUnlucky( char * ),
    CommandCopy ( char * ),
    CommandDatabaseDump( char * ),
    CommandDatabaseExport( char * ),
    CommandDatabaseImport( char * ),
    CommandDatabaseRollout( char * ),
    CommandDatabaseGenerate( char * ),
    CommandDatabaseTrain( char * ),
    CommandDatabaseVerify( char * ),
    CommandDecline( char * ),
    CommandDouble( char * ),
    CommandDrop( char * ),
    CommandEval( char * ),
    CommandEq2MWC( char * ),
    CommandExportGameGam( char * ),
    CommandExportGameHtml( char * ),
    CommandExportGameLaTeX( char * ),
    CommandExportGamePDF( char * ),
    CommandExportGamePostScript( char * ),
    CommandExportMatchMat( char * ),
    CommandExportMatchHtml( char * ),
    CommandExportMatchLaTeX( char * ),
    CommandExportMatchPDF( char * ),
    CommandExportMatchPostScript( char * ),
    CommandExportPositionEPS( char * ),
    CommandExportPositionHtml( char * ),
    CommandExternal( char * ),
    CommandHelp( char * ),
    CommandHint( char * ),
    CommandImportJF( char * ),
    CommandImportMat( char * ),
    CommandImportOldmoves( char * ),
    CommandImportSGG( char * ),
    CommandListGame( char * ),
    CommandListMatch( char * ),
    CommandLoadCommands( char * ),
    CommandLoadGame( char * ),
    CommandLoadMatch( char * ),
    CommandMove( char * ),
    CommandMWC2Eq( char * ),
    CommandNewGame( char * ),
    CommandNewMatch( char * ),
    CommandNewSession( char * ),
    CommandNewWeights( char * ),
    CommandNext( char * ),
    CommandNotImplemented( char * ),
    CommandPlay( char * ),
    CommandPrevious( char * ),
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
    CommandSetAnalysisChequerplay( char * ),
    CommandSetAnalysisCube( char * ),
    CommandSetAnalysisCubedecision( char * ),
    CommandSetAnalysisLimit( char * ),
    CommandSetAnalysisLuck( char * ),
    CommandSetAnalysisMoves( char * ),
    CommandSetAnalysisThresholdBad( char * ),
    CommandSetAnalysisThresholdDoubtful( char * ),
    CommandSetAnalysisThresholdGood( char * ),
    CommandSetAnalysisThresholdInteresting( char * ),
    CommandSetAnalysisThresholdLucky( char * ),
    CommandSetAnalysisThresholdUnlucky( char * ),
    CommandSetAnalysisThresholdVeryBad( char * ),
    CommandSetAnalysisThresholdVeryGood( char * ),
    CommandSetAnalysisThresholdVeryLucky( char * ),
    CommandSetAnalysisThresholdVeryUnlucky( char * ),
    CommandSetAnnotation( char * ),
    CommandSetAppearance( char * ),
    CommandSetAutoBearoff( char * ),
    CommandSetAutoCrawford( char * ),
    CommandSetAutoDoubles( char * ),
    CommandSetAutoGame( char * ),
    CommandSetAutoMove( char * ),
    CommandSetAutoRoll( char * ),
    CommandSetBoard( char * ),
    CommandSetBeavers( char * ),
    CommandSetCache( char * ),
    CommandSetClockwise( char * ),
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
    CommandSetEvalCubeful( char * ),
    CommandSetEvalDeterministic( char * ),
    CommandSetEvalNoise( char * ),    
    CommandSetEvalPlies( char * ),
    CommandSetEvalReduced ( char * ),
    CommandSetEvalTolerance( char * ),
    CommandSetEvaluation( char * ),
    CommandSetEvalParamType( char * ),
    CommandSetEvalParamRollout( char * ),
    CommandSetEvalParamEvaluation( char * ),
    CommandSetEvalChequerplay ( char * ),
    CommandSetEvalCubedecision ( char * ),
    CommandSetEgyptian( char * ),
    CommandSetJacoby( char * ),
    CommandSetMatchID ( char * ),
    CommandSetMET( char * ),
    CommandSetJacoby( char * ),
    CommandSetNackgammon( char * ),
    CommandSetOutputMatchPC( char * ),
    CommandSetOutputMWC ( char * ),
    CommandSetOutputRawboard( char * ),
    CommandSetOutputWinPC( char * ),
    CommandSetPlayerChequerplay( char * ),
    CommandSetPlayerCubedecision( char * ),
    CommandSetPlayerExternal( char * ),
    CommandSetPlayerGNU( char * ),
    CommandSetPlayerHuman( char * ),
    CommandSetPlayerName( char * ),
    CommandSetPlayerPlies( char * ),
    CommandSetPlayerPubeval( char * ),
    CommandSetPlayer( char * ),
    CommandSetPostCrawford( char * ),
    CommandSetPrompt( char * ),
    CommandSetRecord( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGIsaac( char * ),
    CommandSetRNGManual( char * ),
    CommandSetRNGMD5( char * ),
    CommandSetRNGMersenne( char * ),
    CommandSetRNGUser( char * ),
    CommandSetRollout ( char * ),
    CommandSetRolloutCubedecision ( char * ),
    CommandSetRolloutCubeful ( char * ),
    CommandSetRolloutChequerplay ( char * ),
    CommandSetRolloutInitial( char * ),
    CommandSetRolloutPlayer ( char * ),
    CommandSetRolloutPlayerChequerplay ( char * ),
    CommandSetRolloutPlayerCubedecision ( char * ),
    CommandSetRolloutRNG ( char * ),
    CommandSetRolloutSeed( char * ),
    CommandSetRolloutTrials( char * ),
    CommandSetRolloutTruncation( char * ),
    CommandSetRolloutVarRedn( char * ),
    CommandSetScore( char * ),
    CommandSetSeed( char * ),
    CommandSetTrainingAlpha( char * ),
    CommandSetTrainingAnneal( char * ),
    CommandSetTrainingThreshold( char * ),
    CommandSetTurn( char * ),
    CommandShowAnalysis( char * ),
    CommandShowAutomatic( char * ),
    CommandShowBoard( char * ),
    CommandShowBeavers( char * ),
    CommandShowCache( char * ),
    CommandShowClockwise( char * ),
    CommandShowCommands( char * ),
    CommandShowConfirm( char * ),
    CommandShowCopying( char * ),
    CommandShowCrawford( char * ),
    CommandShowCube( char * ),
    CommandShowDelay( char * ),
    CommandShowDice( char * ),
    CommandShowDisplay( char * ),
    CommandShowEngine( char * ),
    CommandShowEvaluation( char * ),
    CommandShowGammonValues( char * ),
    CommandShowEgyptian( char * ),
    CommandShowJacoby( char * ),
    CommandShowKleinman( char * ),
    CommandShowMarketWindow( char * ),
    CommandShowNackgammon( char * ),
    CommandShowMatchEquityTable( char * ),
    CommandShowOutput( char * ),
    CommandShowPipCount( char * ),
    CommandShowPostCrawford( char * ),
    CommandShowPlayer( char * ),
    CommandShowPrompt( char * ),
    CommandShowRNG( char * ),
    CommandShowRollout( char * ),
    CommandShowScore( char * ),
    CommandShowSeed( char * ),
    CommandShowStatisticsGame( char * ),
    CommandShowStatisticsMatch( char * ),
    CommandShowStatisticsSession( char * ),
    CommandShowThorp( char * ),
    CommandShowTraining( char * ),
    CommandShowTurn( char * ),
    CommandShowVersion( char * ),
    CommandShowWarranty( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * );
#endif
