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


/* position of windows: main window, game list, and annotation */

typedef enum _gnubgwindow {
  WINDOW_MAIN = 0,
  WINDOW_GAME,
  WINDOW_ANNOTATION,
  WINDOW_HINT,
  WINDOW_MESSAGE,
  NUM_WINDOWS 
} gnubgwindow;

typedef struct _windowgeometry {
#if USE_GTK
  gint nWidth, nHeight;
  gint nPosX, nPosY;
#else
  int nWidth, nHeight;
  int nPosX, nPosY;
#endif
} windowgeometry;

/* predefined board designs */

extern windowgeometry awg[ NUM_WINDOWS ];


typedef struct _monitor {
#if USE_GTK
    int fGrab;
    int idSignal;
#endif
} monitor;

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
    movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
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
    float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
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
    float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float arDouble[ 4 ];
    evalsetup esDouble;
    /* evaluation of the moves */
    movelist ml;
    int iMove; /* index into the movelist of the move that was made */
    lucktype lt;
    float rLuck; /* ERR_VAL means unknown */
    skilltype stMove;
    skilltype stCube;
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

typedef struct _matchinfo { /* SGF match information */
    char *pchRating[ 2 ], *pchEvent, *pchRound, *pchPlace, *pchAnnotator,
	*pchComment; /* malloc()ed, or NULL if unknown */
    int nYear, nMonth, nDay; /* 0 for nYear means date unknown */
} matchinfo;

extern matchstate ms;
extern matchinfo mi;
extern int fNextTurn, fComputing;

/* User settings. */
extern int fAutoGame, fAutoMove, fAutoRoll, fAutoCrawford, cAutoDoubles,
    fCubeUse, fNackgammon, fDisplay, fAutoBearoff, fShowProgress,
    nBeavers, fOutputMWC, fOutputWinPC, fOutputMatchPC, fJacoby,
    fOutputRawboard, fAnnotation, cAnalysisMoves, fAnalyseCube,
    fAnalyseDice, fAnalyseMove, fRecord, fMessage, nDefaultLength;
extern int fInvertMET;
extern int fConfirm, fConfirmSave;
extern float rAlpha, rAnneal, rThreshold, arLuckLevel[ LUCK_VERYGOOD + 1 ],
    arSkillLevel[ SKILL_VERYGOOD + 1 ], rEvalsPerSec;
extern int nThreadPriority;
extern int fCheat;

/* GUI settings. */
#if USE_GTK
typedef enum _animation {
    ANIMATE_NONE, ANIMATE_BLINK, ANIMATE_SLIDE
} animation;
    
extern animation animGUI;
extern int nGUIAnimSpeed, fGUIBeep, fGUIDiceArea, fGUIHighDieFirst,
    fGUIIllegal, fGUIShowIDs, fGUIShowPips, fGUISetWindowPos,
    fGUIDragTargetHelp;
#endif

typedef enum _pathformat {
  PATH_EPS, PATH_GAM, PATH_HTML, PATH_LATEX, PATH_MAT, PATH_OLDMOVES,
  PATH_PDF, PATH_POS, PATH_POSTSCRIPT, PATH_SGF, PATH_SGG, PATH_TEXT, 
  PATH_MET, PATH_TMG, PATH_BKG,
  NUM_PATHS } 
pathformat;

extern char aaszPaths[ NUM_PATHS ][ 2 ][ 255 ];
extern char *aszExtensions[ NUM_PATHS ];
extern char *szCurrentFileName;

extern evalcontext ecTD;
extern evalcontext ecLuck;

extern evalsetup esEvalCube, esEvalChequer;
extern evalsetup esAnalysisCube, esAnalysisChequer;

extern movefilter aamfEval[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
extern movefilter aamfAnalysis[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];

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
    matchstate ms;
} storedmoves;
extern storedmoves sm;

/*
 * Store cube analysis
 *
 */

typedef struct _storedcube {
  float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  evalsetup es;
  matchstate ms;
} storedcube;
extern storedcube sc;


extern player ap[ 2 ];

extern char *GetInput( char *szPrompt );
extern int GetInputYN( char *szPrompt );
extern void HandleCommand( char *sz, command *ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ] );
extern char *NextToken( char **ppch );
extern int NextTurn( int fPlayNext );
extern void TurnDone( void );
extern void AddMoveRecord( void *pmr );
extern void ApplyMoveRecord( matchstate *pms, list *plGame, moverecord *pmr );
extern void SetMoveRecord( void *pmr );
extern void ClearMoveRecord( void );
extern void AddGame( moverecord *pmr );
extern void ChangeGame( list *plGameNew );
extern void
FixMatchState ( matchstate *pms, const moverecord *pmr );
extern void CalculateBoard( void );
extern void CancelCubeAction( void );
extern int ComputerTurn( void );
extern void ClearMatch( void );
extern void FreeMatch( void );
extern void SetMatchDate( matchinfo *pmi );
extern int GetMatchStateCubeInfo( cubeinfo *pci, matchstate *pms );
extern int ParseHighlightColour( char *sz );
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char *sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char **ppch, char *pchDesc );
extern double ParseReal( char **ppch );
extern int ParseKeyValue( char **ppch, char *apch[ 2 ] );
extern int CompareNames( char *sz0, char *sz1 );
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff );
extern void ShowBoard( void );
extern void SetMatchID ( const char *szMatchID );
extern char*
FormatCubePosition ( char *sz, cubeinfo *pci );
extern char *FormatPrompt( void );
extern char *FormatMoveHint( char *sz, matchstate *pms, movelist *pml,
			     int i, int fRankKnown,
                             int fDetailProb, int fShowParameters );
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
extern void output( const char *sz );
/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( const char *sz );
/* Write a character to stdout/status bar/popup window */
extern void outputc( const char ch );
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( const char *sz, ... ) __attribute__((format(printf,1,2)));
/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( const char *sz, va_list val )
    __attribute__((format(printf,1,0)));
/* Write an error message, perror() style */
extern void outputerr( const char *sz );
/* Write an error message, fprintf() style */
extern void outputerrf( const char *sz, ... )
    __attribute__((format(printf,1,2)));
/* Write an error message, vfprintf() style */
extern void outputerrv( const char *sz, va_list val )
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

/* Temporarily ignore TTY/GUI input. */
extern void SuspendInput( monitor *pm );
/* Resume input (must match a previous SuspendInput). */
extern void ResumeInput( monitor *pm );

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

#ifdef WIN32
#define DIR_SEPARATOR  '\\'
#define DIR_SEPARATOR_S  "\\"
#else
#define DIR_SEPARATOR  '/'
#define DIR_SEPARATOR_S  "/"
#endif


#if HAVE_LIBREADLINE
extern int fReadline;
#endif

extern int
AnalyzeMove ( moverecord *pmr, matchstate *pms, list *plGame, statcontext *psc,
              evalsetup *pesChequer, evalsetup *pesCube,
              movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
	      int fUpdateStatistics );

extern int
confirmOverwrite ( const char *sz, const int f );

extern void
setDefaultPath ( const char *sz, const pathformat f );

extern void
setDefaultFileName ( const char *sz, const pathformat f );

extern char *
getDefaultFileName ( const pathformat f );

extern char *
getDefaultPath ( const pathformat f );

extern char *GetLuckAnalysis( matchstate *pms, float rLuck );

extern moverecord *
getCurrentMoveRecord ( int *pfHistory );

extern void
UpdateStoredMoves ( const movelist *pml, const matchstate *pms );

extern void
UpdateStoredCube ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   const evalsetup *pes,
                   const matchstate *pms );

extern void
InvalidateStoredMoves( void );

#ifdef WIN32
extern void WinCopy( char *szOut );
#endif

extern char *aszVersion[], *szHomeDirectory, *szDataDirectory,
    *szTerminalCharset;

extern char *aszSkillType[], *aszSkillTypeAbbr[], *aszLuckType[],
    *aszLuckTypeAbbr[], *aszSkillTypeCommand[], *aszLuckTypeCommand[];

extern command acDatabase[], acNew[], acSave[], acSetAutomatic[],
    acSetCube[], acSetEvaluation[], acSetPlayer[], acSetRNG[], 
    acSetRollout[], acSetRolloutLate[], acSetTruncation [], 
    acSet[], acShow[], acTrain[], acTop[], acSetMET[], acSetEvalParam[],
    acSetRolloutPlayer[], acSetRolloutLatePlayer[], cOnOff, cFilename,
    cHighlightColour;

extern command acAnnotateMove[];
extern command acSetExportParameters[];
extern command acSetGeometryValues[];
extern command acSetHighlightIntensity[];

extern void CommandAccept( char * ),
    CommandAgree( char * ),
    CommandAnalyseClearGame( char * ),
    CommandAnalyseClearMatch( char * ),
    CommandAnalyseClearMove( char * ),
    CommandAnalyseGame( char * ),
    CommandAnalyseMatch( char * ),
    CommandAnalyseMove( char * ),
    CommandAnalyseSession( char * ),
    CommandAnnotateAccept ( char * ),
    CommandAnnotateBad( char * ),
    CommandAnnotateClearComment( char * ),
    CommandAnnotateClearLuck( char * ),
    CommandAnnotateClearSkill( char * ),
    CommandAnnotateCube ( char * ),
    CommandAnnotateDouble ( char * ),
    CommandAnnotateDoubtful( char * ),
    CommandAnnotateDrop ( char * ),
    CommandAnnotateGood( char * ),
    CommandAnnotateInteresting( char * ),
    CommandAnnotateLucky( char * ),
    CommandAnnotateMove ( char * ),
    CommandAnnotateReject ( char * ),
    CommandAnnotateResign ( char * ),
    CommandAnnotateUnlucky( char * ),
    CommandAnnotateVeryBad( char * ),
    CommandAnnotateVeryGood( char * ),
    CommandAnnotateVeryLucky( char * ),
    CommandAnnotateVeryUnlucky( char * ),
    CommandCalibrate( char *sz ),
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
    CommandExportGameText( char * ),
    CommandExportGameEquityEvolution ( char * ),
    CommandExportHTMLImages ( char * ),
    CommandExportMatchMat( char * ),
    CommandExportMatchHtml( char * ),
    CommandExportMatchLaTeX( char * ),
    CommandExportMatchPDF( char * ),
    CommandExportMatchPostScript( char * ),
    CommandExportMatchText( char * ),
    CommandExportMatchEquityEvolution ( char * ),
    CommandExportPositionEPS( char * ),
    CommandExportPositionPNG( char * ),
    CommandExportPositionHtml( char * ),
    CommandExportPositionText( char * ),
    CommandExportPositionGammOnLine ( char * ),
    CommandExternal( char * ),
    CommandHelp( char * ),
    CommandHint( char * ),
    CommandImportBKG( char * ),
    CommandImportJF( char * ),
    CommandImportMat( char * ),
    CommandImportOldmoves( char * ),
    CommandImportSGG( char * ),
    CommandImportTMG( char * ),
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
    CommandRecordAddGame( char * ),
    CommandRecordAddMatch( char * ),
    CommandRecordAddSession( char * ),
    CommandRecordErase( char * ),
    CommandRecordEraseAll( char * ),
    CommandRecordShow( char * ),
    CommandResign( char * ),
    CommandRoll( char * ),
    CommandRollout( char * ),
    CommandSaveGame( char * ),
    CommandSaveMatch( char * ),
    CommandSavePosition( char * ),
    CommandSaveSettings( char * ),
    CommandSaveWeights( char * ),
    CommandSetAnalysisChequerplay( char * ),
    CommandSetAnalysisCube( char * ),
    CommandSetAnalysisCubedecision( char * ),
    CommandSetAnalysisLimit( char * ),
    CommandSetAnalysisLuckAnalysis( char * ),
    CommandSetAnalysisLuck( char * ),
    CommandSetAnalysisMoveFilter( char * ),
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
    CommandSetAutoAnalysis( char * ),
    CommandSetAutoBearoff( char * ),
    CommandSetAutoCrawford( char * ),
    CommandSetAutoDoubles( char * ),
    CommandSetAutoGame( char * ),
    CommandSetAutoMove( char * ),
    CommandSetAutoRoll( char * ),
    CommandSetBoard( char * ),
    CommandSetBeavers( char * ),
    CommandSetCache( char * ),
    CommandSetCalibration( char * ),
    CommandSetCheat ( char * ),
    CommandSetClockwise( char * ),
    CommandSetConfirmNew( char * ),
    CommandSetConfirmSave( char * ),
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
    CommandSetEvalNoOnePlyPrune( char * ),
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
    CommandSetEvalMoveFilter ( char * ),
    CommandSetEgyptian( char * ),
    CommandSetExportIncludeAnnotations ( char * ),
    CommandSetExportIncludeAnalysis ( char * ),
    CommandSetExportIncludeStatistics ( char * ),
    CommandSetExportIncludeLegend ( char * ),
    CommandSetExportIncludeMatchInfo ( char * ),
    CommandSetExportShowBoard ( char * ),
    CommandSetExportShowPlayer ( char * ),
    CommandSetExportMovesNumber ( char * ),
    CommandSetExportMovesProb ( char * ),
    CommandSetExportMovesParameters ( char * ),
    CommandSetExportMovesDisplayVeryBad ( char * ),
    CommandSetExportMovesDisplayBad ( char * ),
    CommandSetExportMovesDisplayDoubtful ( char * ),
    CommandSetExportMovesDisplayUnmarked ( char * ),
    CommandSetExportMovesDisplayInteresting ( char * ),
    CommandSetExportMovesDisplayGood ( char * ),
    CommandSetExportMovesDisplayVeryGood ( char * ),
    CommandSetExportCubeProb ( char * ),
    CommandSetExportCubeParameters ( char * ),
    CommandSetExportCubeDisplayVeryBad ( char * ),
    CommandSetExportCubeDisplayBad ( char * ),
    CommandSetExportCubeDisplayDoubtful ( char * ),
    CommandSetExportCubeDisplayUnmarked ( char * ),
    CommandSetExportCubeDisplayInteresting ( char * ),
    CommandSetExportCubeDisplayGood ( char * ),
    CommandSetExportCubeDisplayVeryGood ( char * ),
    CommandSetExportCubeDisplayActual ( char * ),
    CommandSetExportCubeDisplayMissed ( char * ),    
    CommandSetExportCubeDisplayClose ( char * ),
    CommandSetExportHTMLCSSExternal ( char * ),
    CommandSetExportHTMLCSSHead ( char * ),
    CommandSetExportHTMLCSSInline ( char * ),
    CommandSetExportHTMLPictureURL ( char * ),
    CommandSetExportHTMLTypeBBS ( char * ),
    CommandSetExportHTMLTypeFibs2html ( char * ),
    CommandSetExportHTMLTypeGNU ( char * ),
    CommandSetExportParametersEvaluation ( char * ),
    CommandSetExportParametersRollout ( char * ),
    CommandSetExportPNGSize ( char *),
    CommandSetGeometryAnnotation ( char * ),
    CommandSetGeometryGame ( char * ),
    CommandSetGeometryHint ( char * ),
    CommandSetGeometryMain ( char * ),
    CommandSetGeometryMessage ( char * ),
    CommandSetGeometryWidth ( char * ),
    CommandSetGeometryHeight ( char * ),
    CommandSetGeometryPosX ( char * ),
    CommandSetGeometryPosY ( char * ),
    CommandSetGUIAnimationBlink( char * ),
    CommandSetGUIAnimationNone( char * ),
    CommandSetGUIAnimationSlide( char * ),
    CommandSetGUIAnimationSpeed( char * ),
    CommandSetGUIBeep( char * ),
    CommandSetGUIDiceArea( char * ),
    CommandSetGUIHighDieFirst( char * ),
    CommandSetGUIIllegal( char * ),
    CommandSetGUIWindowPositions( char * ),
    CommandSetGUIShowIDs( char * ),
    CommandSetGUIShowPips( char * ),
    CommandSetGUIDragTargetHelp( char * ),
    CommandSetHighlight ( char * ),
    CommandSetHighlightColour ( char * ),
    CommandSetHighlightDark ( char * ),
    CommandSetHighlightLight ( char * ),
    CommandSetHighlightMedium ( char * ),
    CommandSetInvertMatchEquityTable( char * ),
    CommandSetJacoby( char * ),
    CommandSetMatchAnnotator( char * ),
    CommandSetMatchComment( char * ),
    CommandSetMatchDate( char * ),
    CommandSetMatchEvent( char * ),
    CommandSetMatchLength( char * ),
    CommandSetMatchPlace( char * ),
    CommandSetMatchRating( char * ),
    CommandSetMatchRound( char * ),
    CommandSetMatchID ( char * ),
    CommandSetMessage ( char * ),
    CommandSetMET( char * ),
    CommandSetMoveFilter( char * ),
    CommandSetNackgammon( char * ),
    CommandSetOutputMatchPC( char * ),
    CommandSetOutputMWC ( char * ),
    CommandSetOutputRawboard( char * ),
    CommandSetOutputWinPC( char * ),
    CommandSetPathEPS( char * ),
    CommandSetPathSGF( char * ),
    CommandSetPathLaTeX( char * ),
    CommandSetPathPDF( char * ),
    CommandSetPathHTML( char * ),
    CommandSetPathMat( char * ),
    CommandSetPathMET( char * ),
    CommandSetPathSGG( char * ),
    CommandSetPathTMG( char * ),
    CommandSetPathOldMoves( char * ),
    CommandSetPathPos( char * ),
    CommandSetPathGam( char * ),
    CommandSetPathPostScript( char * ),
    CommandSetPathText( char * ),
    CommandSetPlayerChequerplay( char * ),
    CommandSetPlayerCubedecision( char * ),
    CommandSetPlayerExternal( char * ),
    CommandSetPlayerGNU( char * ),
    CommandSetPlayerHuman( char * ),
    CommandSetPlayerName( char * ),
    CommandSetPlayerMoveFilter( char * ),
    CommandSetPlayerPlies( char * ),
    CommandSetPlayerPubeval( char * ),
    CommandSetPlayer( char * ),
    CommandSetPostCrawford( char * ),
    CommandSetPriorityAboveNormal ( char * ),
    CommandSetPriorityBelowNormal ( char * ),
    CommandSetPriorityHighest ( char * ),
    CommandSetPriorityIdle ( char * ),
    CommandSetPriorityNice( char * ),
    CommandSetPriorityNormal ( char * ),
    CommandSetPriorityTimeCritical ( char * ),
    CommandSetPrompt( char * ),
    CommandSetRecord( char * ),
    CommandSetRNG( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBBS( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGIsaac( char * ),
    CommandSetRNGManual( char * ),
    CommandSetRNGMD5( char * ),
    CommandSetRNGMersenne( char * ),
    CommandSetRNGRandomDotOrg( char * ),
    CommandSetRNGUser( char * ),
    CommandSetRollout ( char * ),
    CommandSetRolloutBearoffTruncationExact ( char * ),
    CommandSetRolloutBearoffTruncationOS ( char * ),
    CommandSetRolloutCubedecision ( char * ),
    CommandSetRolloutLateCubedecision ( char * ),
    CommandSetRolloutCubeful ( char * ),
    CommandSetRolloutChequerplay ( char * ),
    CommandSetRolloutInitial( char * ),
    CommandSetRolloutMoveFilter( char * ),
    CommandSetRolloutPlayer ( char * ),
    CommandSetRolloutPlayerChequerplay ( char * ),
    CommandSetRolloutPlayerCubedecision ( char * ),
    CommandSetRolloutPlayerMoveFilter( char * ),
    CommandSetRolloutPlayerLateChequerplay ( char * ),
    CommandSetRolloutPlayerLateCubedecision ( char * ),
    CommandSetRolloutPlayerLateMoveFilter( char * ),
    CommandSetRolloutLate ( char * ),
    CommandSetRolloutLateChequerplay ( char * ),
    CommandSetRolloutLateMoveFilter( char * ),
    CommandSetRolloutLateEnable ( char * ),
    CommandSetRolloutLatePlayer ( char * ),
    CommandSetRolloutLatePlies ( char * ),
    CommandSetRolloutTruncationChequer ( char * ),
    CommandSetRolloutTruncationCube ( char * ),
    CommandSetRolloutTruncationEnable ( char * ),
    CommandSetRolloutTruncationPlies ( char * ),    
    CommandSetRolloutRNG ( char * ),
    CommandSetRolloutRotate ( char * ),
    CommandSetRolloutSeed( char * ),
    CommandSetRolloutTrials( char * ),
    CommandSetRolloutTruncation( char * ),
    CommandSetRolloutVarRedn( char * ),
    CommandSetScore( char * ),
    CommandSetSeed( char * ),
    CommandSetSoundEnable ( char * ),
    CommandSetSoundSystemArtsc ( char * ),
    CommandSetSoundSystemCommand ( char * ),
    CommandSetSoundSystemESD ( char * ),
    CommandSetSoundSystemNAS ( char * ),
    CommandSetSoundSystemNormal ( char * ),
    CommandSetSoundSystemWindows ( char * ),
    CommandSetSoundSoundAgree ( char * ),
    CommandSetSoundSoundBotDance ( char * ),
    CommandSetSoundSoundBotWinGame ( char * ),
    CommandSetSoundSoundBotWinMatch ( char * ),
    CommandSetSoundSoundDouble ( char * ),
    CommandSetSoundSoundDrop ( char * ),
    CommandSetSoundSoundExit ( char * ),
    CommandSetSoundSoundHumanDance ( char * ),
    CommandSetSoundSoundHumanWinGame ( char * ),
    CommandSetSoundSoundHumanWinMatch ( char * ),
    CommandSetSoundSoundMove ( char * ),
    CommandSetSoundSoundRedouble ( char * ),
    CommandSetSoundSoundResign ( char * ),
    CommandSetSoundSoundRoll ( char * ),
    CommandSetSoundSoundStart ( char * ),
    CommandSetSoundSoundTake ( char * ),
    CommandSetTrainingAlpha( char * ),
    CommandSetTrainingAnneal( char * ),
    CommandSetTrainingThreshold( char * ),
    CommandSetTurn( char * ),
    CommandSetTutorCube( char * ),
    CommandSetTutorEval( char *sz ),
    CommandSetTutorChequer( char * ),
    CommandSetTutorMode( char * ),  
    CommandSetTutorSkillDoubtful( char *sz ),
    CommandSetTutorSkillBad( char *sz ), 
    CommandSetTutorSkillVeryBad( char *sz ),
    CommandShowAnalysis( char * ),
    CommandShowAutomatic( char * ),
    CommandShowBoard( char * ),
    CommandShowBeavers( char * ),
    CommandShowCache( char * ),
    CommandShowCalibration( char * ),
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
    CommandShowExport ( char * ),
    CommandShowFullBoard( char * ),
    CommandShowGammonValues( char * ),
    CommandShowGeometry ( char * ),
    CommandShowHighlightColour ( char * ),
    CommandShowEgyptian( char * ),
    CommandShowJacoby( char * ),
    CommandShowKleinman( char * ),
    CommandShowMarketWindow( char * ),
    CommandShowMatchInfo( char * ),
    CommandShowMatchLength( char * ),
    CommandShowMatchEquityTable( char * ),
    CommandShowNackgammon( char * ),
    CommandShowOneChequer ( char * ),
    CommandShowOneSidedRollout ( char * ),
    CommandShowOutput( char * ),
    CommandShowPath( char * ),
    CommandShowPipCount( char * ),
    CommandShowPostCrawford( char * ),
    CommandShowPlayer( char * ),
    CommandShowPrompt( char * ),
    CommandShowRNG( char * ),
    CommandShowRollout( char * ),
    CommandShowRolls ( char * ),
    CommandShowScore( char * ),
    CommandShowSeed( char * ),
    CommandShowSound( char * ),
    CommandShowStatisticsGame( char * ),
    CommandShowStatisticsMatch( char * ),
    CommandShowStatisticsSession( char * ),
    CommandShowThorp( char * ),
    CommandShowTraining( char * ),
    CommandShowTurn( char * ),
    CommandShowTutor( char * ), 
    CommandShowVersion( char * ),
    CommandShowWarranty( char * ),
    CommandSwapPlayers ( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * ),
    CommandXCopy ( char * );


extern int fTutor, fTutorCube, fTutorChequer, nTutorSkillCurrent;

extern int GiveAdvice ( skilltype Skill );
extern skilltype TutorSkill;
extern int fTutorAnalysis;

extern int EvalCmp ( const evalcontext *, const evalcontext *, const int);

#ifndef HAVE_BASENAME
extern char *
basename ( const char *filename );
#endif

#if USE_GTK
#  if GTK_CHECK_VERSION(1,3,0) || defined (WIN32)
#    define GNUBG_CHARSET "UTF-8"
#  else
#    define GNUBG_CHARSET "ISO-8859-1"
#  endif
#else
#  define GNUBG_CHARSET "ISO-8859-1"
#endif

extern char *
Convert ( const char *sz, 
          const char *szSourceCharset, const char *szDestCharset );

extern void
OptimumRoll ( int anBoard[ 2 ][ 25 ], 
              const cubeinfo *pci, const evalcontext *pec,
              const int fBest, int *pnDice0, int *pnDice1 );



typedef struct _highlightcolour {
  int   rgbs[3][3];
  char *colourname;
} highlightcolour;

extern highlightcolour *HighlightColour, HighlightColourTable[];
extern int HighlightIntensity;
extern int *Highlightrgb;

extern void
SetMatchInfo( char **ppch, char *sz, char *szMessage );

extern void
TextToClipboard ( const char * sz );


#endif
