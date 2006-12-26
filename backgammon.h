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
#include "list.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "analysis.h"
#include "eval.h"
#include "common.h"

#if USE_TIMECONTROL && WIN32
#include <winsock2.h>
#endif

#include <glib.h>
#if USE_GTK
#include <gtk/gtk.h>
extern GtkWidget* pwBoard;
extern int fX, nDelay, fNeedPrompt;
extern guint nNextTurn; /* GTK idle function */
#if USE_TIMECONTROL
extern void GTKUpdateClock(void);
#endif
#endif

#define MAX_CUBE ( 1 << 12 )

typedef struct _command {
  /* Command name (NULL indicates end of list) */
  char* sz; 
  
  /* Command handler; NULL to use default subcommand handler */
  void ( *pf )( char* ); 

  /* Documentation; NULL for abbreviations */
  char* szHelp;
  char* szUsage; 

  /* List of subcommands (NULL if none) */
  struct _command* pc; 
} command;

typedef enum _playertype {
    PLAYER_EXTERNAL, PLAYER_HUMAN, PLAYER_GNU, PLAYER_PUBEVAL
} playertype;

typedef struct _player {
  /* For all player types: */
  char szName[ MAX_NAME_LEN ];

  playertype pt;

  /* For PLAYER_GNU: */
  evalsetup esChequer, esCube;
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
  int h;
  /* For PLAYER_EXTERNAL: */
  char* szSocket;
} player;

typedef enum _movetype {
    MOVE_GAMEINFO,
    MOVE_NORMAL,
    MOVE_DOUBLE,
    MOVE_TAKE,
    MOVE_DROP,
    MOVE_RESIGN,
    MOVE_SETBOARD,
    MOVE_SETDICE,
    MOVE_SETCUBEVAL,
    MOVE_SETCUBEPOS,
    MOVE_TIME
} movetype;

#if USE_TIMECONTROL

#include <time.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/* The different timing types
 * TC_PLAIN - normal chess clock timing
 * TC_FISCHER - Fischer clock - a fixed time is added to clock each move
 * TC_BRONSTEIN - Bronstein (FIDE) clock - The player gets a little "free"
 *	time each move, before the clock starts, but no time is added.
 * TC_HOURGLASS - Hourglass timing, what player A spends is added to B's
 *	clock and vice versa
 * TC_UNKNOWN - unknown timing type (e.g. read from file)
 */
typedef enum _tctiming {
TC_NONE=0, TC_PLAIN, TC_FISCHER, TC_BRONSTEIN, TC_HOURGLASS, TC_UNKNOWN
} tctiming;

/* The different penalty types
 * TC_POINT - a number of points - possibly 0 if there is no penalty -
 *	 is added to the other players score
 * TC_LOSS - the player loses the match
 */
typedef enum _tcpenalty {
TC_POINT, TC_LOSS
} tcpenalty;

typedef struct _tctransition {
   char *szName; 
   int (*pfDecision)(int);
} tctransition;

typedef struct _tctransitionnode {
   tctransition *ptrans;
   char *szNext;
   struct _tctransitionnode *next;
} tctransitionnode;

typedef struct _timecontrol {
    char *szName;
    tctiming timing;
    tcpenalty penalty;

    int nAddedTime;		/* A fixed time to add at the start of this
	time control */
    int nPointAllowance; 	/* Time to add per point remaining in match */
    double dMultiplier; 	/* How much of old time allowance to keep when
	starting this time control */
    int nMoveAllowance;		/* Time allowance per move */
    int nPenalty;		/* Point penalty for timing out.  May be 0. */

	/* Next time control for the player whose time runs out. 
	   NULL means reiterate same */
    char *szNext;
	/* The other guy's next time control
	   NULL means no change to his time control*/
    char *szNextB; 
    tctransitionnode *pTransitions;
} timecontrol;
   
typedef struct _tcnode {
    timecontrol *ptc;
    struct _tcnode *next;
} tcnode;

typedef struct _playerclock {
    struct timeval tvStamp; 
    struct timeval tvTimeleft;
    timecontrol tc;
   /*  int nTimeouts; */
} playerclock;
   
typedef struct _gameclock {
   playerclock pc[2];
   struct timeval pausedtime;
	/* whose clock is running, 0,1 or -1(paused) */
   int fPlayer; 
} gameclock;
    
#endif

typedef struct _movegameinfo {

  /* ordinal number of the game within a match */
  int i;

  /* match length */  
  int nMatch;

  /* match score BEFORE the game */
  int anScore[2];
  
  /* the Crawford rule applies during this match */
  int fCrawford;

  /* this is the Crawford game */
  int fCrawfordGame;

  int fJacoby;

  /* who won (-1 = unfinished) */
  int fWinner;

  /* how many points were scored by the winner */
  int nPoints;

  /* the game was ended by resignation */
  int fResigned;

  /* how many automatic doubles were rolled */
  int nAutoDoubles; 

  /* Type of game */ 
  bgvariation bgv;

  /* Cube used in game */
  int fCubeUse;

  statcontext sc;
#if USE_TIMECONTROL
  /* the game ended on time */ 
    int fTimeout; 
  /* how many timeouts (clock expiry) have happened during the match */
    int nTimeouts[2];
#endif
} xmovegameinfo;

typedef struct cubedecisiondata {
  float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  evalsetup esDouble;
} cubedecisiondata;

typedef struct _movenormal {

  /* Move made. */
  int anMove[ 8 ];

  /* index into the movelist of the move that was made */
  int iMove; 

  skilltype stMove;
} xmovenormal;

typedef struct _moveresign {
    int nResigned;

    evalsetup esResign;
    float arResign[ NUM_ROLLOUT_OUTPUTS ];

    skilltype stResign;
    skilltype stAccept;
} xmoveresign;

typedef struct _movesetboard {
    unsigned char auchKey[ 10 ]; /* always stored as if player 0 was on roll */
} xmovesetboard;

typedef struct _movesetcubeval {
    int nCube;
} xmovesetcubeval;

typedef struct _movesetcubepos {
    int fCubeOwner;
} xmovesetcubepos;

#if USE_TIMECONTROL
typedef struct _movetime {
    int nPoints;

    /* evaluation of loss */
    float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    evalsetup es;
} xmovetime;
#endif

typedef struct _moverecord {

  /* 
   * Common variables
   */

  /* type of the move */
    movetype mt;

  /* annotation */
  char* sz;

  /* stuff for timing */
#if USE_TIMECONTROL
  struct timeval tl[2];
#endif

  /* move record is for player */
  int fPlayer;

  /* luck analysis (shared between MOVE_SETDICE and MOVE_NORMAL) */

  /* dice rolled */
  int anDice[ 2 ];

  /* classification of luck */
  lucktype lt;

  /* magnitude of luck */
  float rLuck; /* ERR_VAL means unknown */

  /* move analysis (shared between MOVE_SETDICE and MOVE_NORMAL) */

  /* evaluation setup for move analysis */
  evalsetup esChequer;

  /* evaluation of the moves */
  movelist ml;

  /* cube analysis (shared between MOVE_NORMAL and MOVE_DOUBLE) */

  /* 0 in match play, even numbers are doubles, raccoons 
     odd numbers are beavers, aardvarken, etc. */
  int nAnimals;    

  /* the evaluation and settings */
  cubedecisiondata* CubeDecPtr;
  cubedecisiondata  CubeDec;

  /* skill for the cube decision */
  skilltype stCube;

  /* "private" data */

  xmovegameinfo g;     /* game information */
  xmovenormal n;       /* chequerplay move */
  xmoveresign r;       /* resignation */
  xmovesetboard sb;    /* setting up board */
  xmovesetcubeval scv; /* setting cube */
  xmovesetcubepos scp; /* setting cube owner */
#if USE_TIMECONTROL
  xmovetime t;         /* time controls */
#endif
} moverecord;

extern char* aszGameResult[], szDefaultPrompt[], *szPrompt;

typedef enum {
    GAME_NONE, GAME_PLAYING, GAME_OVER, GAME_RESIGNED, GAME_DROP
#if USE_TIMECONTROL
	, GAME_TIMEOUT
#endif
} gamestate; 

/* The match state is represented by the board position (anBoard),
   fTurn (indicating which player makes the next decision), fMove
   (which indicates which player is on roll: normally the same as
   fTurn, but occasionally different, e.g. if a double has been
   offered).  anDice indicate the roll to be played (0,0 indicates the
   roll has not been made). */
typedef struct {
    int anBoard[ 2 ][ 25 ], anDice[ 2 ], fTurn, fResigned,
	fResignationDeclined, fDoubled, cGames, fMove, fCubeOwner, fCrawford,
	fPostCrawford, nMatchTo, anScore[ 2 ], nCube, cBeavers;
    bgvariation bgv;
    int fCubeUse;
    int fJacoby;
    gamestate gs;
#if USE_TIMECONTROL
    gameclock gc;
    struct timeval tvTimeleft[2];
    int nTimeouts[2];
#endif
} matchstate;

typedef struct _matchinfo { /* SGF match information */
    char* pchRating[ 2 ], *pchEvent, *pchRound, *pchPlace, *pchAnnotator,
	*pchComment; /* malloc()ed, or NULL if unknown */
    int nYear, nMonth, nDay; /* 0 for nYear means date unknown */
} matchinfo;

extern matchstate ms;
extern matchinfo mi;
extern int fNextTurn, fComputing;

/* User settings. */
extern int fAutoGame, fAutoMove, fAutoRoll, fAutoCrawford, cAutoDoubles,
    fCubeUse, fDisplay, fAutoBearoff, fShowProgress,
    nBeavers, fJacoby,
    fOutputRawboard, cAnalysisMoves, fAnalyseCube,
    fAnalyseDice, fAnalyseMove, fRecord,
	nDefaultLength, nToolbarStyle, fStyledGamelist, fFullScreen;
extern int fInvertMET;
extern int fConfirm, fConfirmSave;
extern float rAlpha, rAnneal, rThreshold, arLuckLevel[ LUCK_VERYGOOD + 1 ],
    arSkillLevel[ N_SKILLS ], rEvalsPerSec;
extern int nThreadPriority;
extern int fCheat;
extern int afCheatRoll[ 2 ];
extern int fGotoFirstGame;
extern int fSconyers15x15DVD;
extern char szPathSconyers15x15DVD[ BIG_PATH ];      
extern int fSconyers15x15Disk;
extern char szPathSconyers15x15Disk[ BIG_PATH ];      
extern char szLang[ 32 ];   

extern char *szCurrentFileName, *szCurrentFolder;

extern gchar *default_import_folder, *default_export_folder, *default_sgf_folder;

extern evalcontext ecTD;
extern evalcontext ecLuck;

extern evalsetup esEvalCube, esEvalChequer;
extern evalsetup esAnalysisCube, esAnalysisChequer;

extern movefilter aamfEval[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
extern movefilter aamfAnalysis[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];

extern void *rngctxRollout;
extern rolloutcontext rcRollout;

extern int fCubeEqualChequer, fPlayersAreSame, fTruncEqualPlayer0;

extern int log_rollouts;
extern char *log_file_name;

/* The current match.
  A list of games. Each game is a list of moverecords.
  Note that the first list element is empty. The first game is in
  lMatch.plNext->p. Same is true for games.
*/
extern list lMatch;

/*  List of moverecords representing the current game. One of the elements in
    lMatch.
    Typically the last game in the match).
*/
extern list* plGame;

/* Current move inside plGame (typically the most recently
   one played, but "previous" and "next" commands navigate back and forth).
*/
extern list* plLastMove;

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

extern char* GetInput( char* szPrompt );
extern int GetInputYN( char* szPrompt );
extern void HandleCommand( char* sz, command* ac );
extern void InitBoard( int anBoard[ 2 ][ 25 ], const bgvariation bgv );
extern char* NextToken( char **ppch );
extern char* NextTokenGeneral( char **ppch, const char* szTokens );
extern int NextTurn( int fPlayNext );
extern void TurnDone( void );
extern moverecord *
NewMoveRecord( void );
extern void AddMoveRecord( void* pmr );
extern moverecord* LinkToDouble( moverecord* pmr);
extern void ApplyMoveRecord( matchstate* pms, const list* plGame,
			     const moverecord* pmr );
extern void SetMoveRecord( void* pmr );
extern void ClearMoveRecord( void );
extern void AddGame( moverecord* pmr );
extern void ChangeGame( list* plGameNew );
extern void
FixMatchState ( matchstate* pms, const moverecord* pmr );
extern void CalculateBoard( void );
extern void CancelCubeAction( void );
extern int ComputerTurn( void );
extern void ClearMatch( void );
extern void FreeMatch( void );
extern void SetMatchDate( matchinfo* pmi );
extern int GetMatchStateCubeInfo( cubeinfo* pci, const matchstate* pms);
extern int ParseNumber( char **ppch );
extern int ParsePlayer( char* sz );
extern int ParsePosition( int an[ 2 ][ 25 ], char **ppch, char* pchDesc );
extern double ParseReal( char **ppch );
extern int ParseKeyValue( char **ppch, char* apch[ 2 ] );
extern int CompareNames( char* sz0, char* sz1 );
extern int SetToggle( char* szName, int* pf, char* sz, char* szOn,
		       char* szOff );
extern void ShowBoard( void );
extern void SetMatchID ( const char* szMatchID );
extern char* FormatPrompt( void );
extern char* FormatMoveHint( char* sz, matchstate* pms, movelist* pml,
			     int i, int fRankKnown,
                             int fDetailProb, int fShowParameters );
extern void UpdateSetting( void* p );
extern void UpdateSettings( void );
extern void ResetInterrupt( void );
extern void PromptForExit( void );
extern void Prompt( void );
extern void PortableSignal( int nSignal, RETSIGTYPE (*p)(int),
			    psighandler* pOld, int fRestart );
extern void PortableSignalRestore( int nSignal, psighandler* p );
extern RETSIGTYPE HandleInterrupt( int idSignal );

/* Like strncpy, except it does the right thing */
extern char* strcpyn( char* szDest, const char* szSrc, int cch );

/* Write a string to stdout/status bar/popup window */
extern void output( const char* sz );
/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( const char* sz );
/* Write a character to stdout/status bar/popup window */
extern void outputc( const char ch );
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( const char* sz, ... ) __attribute__((format(printf,1,2)));
/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( const char* sz, va_list val )
    __attribute__((format(printf,1,0)));
/* Write an error message, perror() style */
extern void outputerr( const char* sz );
/* Write an error message, fprintf() style */
extern void outputerrf( const char* sz, ... )
    __attribute__((format(printf,1,2)));
/* Write an error message, vfprintf() style */
extern void outputerrv( const char* sz, va_list val )
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
extern void SuspendInput(void);
/* Resume input (must match a previous SuspendInput). */
extern void ResumeInput(void);

extern void ProgressStart( char* sz );
extern void ProgressStartValue( char* sz, int iMax );
extern void Progress( void );
extern void ProgressValue ( int iValue );
extern void ProgressValueAdd ( int iValue );
extern void ProgressEnd( void );

#if USE_GTK
extern gint NextTurnNotify( gpointer p );
extern void UserCommand( char* sz );
extern void HandleXAction( void );
#if HAVE_LIBREADLINE
extern int fReadingCommand;
extern void ProcessInput( char* sz );
#endif
extern void HideAllPanels ( gpointer *p, guint n, GtkWidget *pw );
extern void ShowAllPanels ( gpointer *p, guint n, GtkWidget *pw );
#endif

#if HAVE_LIBREADLINE
extern int fReadline;
#endif

extern int
AnalyzeMove ( moverecord* pmr, matchstate* pms, list* plGame, statcontext* psc,
              evalsetup* pesChequer, evalsetup* pesCube,
              movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ],
	      int fUpdateStatistics, const int afAnalysePlayers[ 2 ] );

extern int
confirmOverwrite ( const char* sz, const int f );

extern void
setDefaultFileName ( char* sz );

extern void DisectPath (char *path, char *extension, char **name, char **folder);

extern char* GetLuckAnalysis( matchstate* pms, float rLuck );

extern moverecord *
getCurrentMoveRecord ( int* pfHistory );

extern int
getFinalScore( int* anScore );

extern char* GetMoveString(moverecord *pmr, int* pPlayer);

extern void
UpdateStoredMoves ( const movelist* pml, const matchstate* pms );

extern void
UpdateStoredCube ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   const evalsetup* pes,
                   const matchstate* pms );

extern void
InvalidateStoredMoves( void );

extern void
InvalidateStoredCube( void );

#define VERSION_STRING "GNU Backgammon " VERSION

extern char *GetBuildInfoString();
extern const char *szHomeDirectory;
extern char *szDataDirectory;

extern char* aszSkillType[], *aszSkillTypeAbbr[], *aszLuckType[],
    *aszLuckTypeAbbr[], *aszSkillTypeCommand[], *aszLuckTypeCommand[];

extern command acDatabase[], acNew[], acSave[], acSetAutomatic[],
    acSetCube[], acSetEvaluation[], acSetPlayer[], acSetRNG[], 
    acSetRollout[], acSetRolloutLate[], acSetRolloutLimit[], 
  acSetTruncation [], acSetRolloutJsd[],
    acSet[], acShow[], acTrain[], acTop[], acSetMET[], acSetEvalParam[],
    acSetRolloutPlayer[], acSetRolloutLatePlayer[], cOnOff, cFilename;
extern command acSetCheatPlayer[];
extern command acSetAnalysisPlayer[];

extern command acAnnotateMove[];
extern command acSetExportParameters[];
extern command acSetGeometryValues[];

extern float rRatingOffset;


extern int
InternalCommandNext(int fMarkedMoves, int n);

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
    CommandAnnotateAddComment( char * ),
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
    CommandCalibrate( char* sz ),
    CommandClearHint( char * ),
    CommandCopy ( char * ),
    CommandDatabaseDump( char * ),
    CommandDatabaseExport( char * ),
    CommandDatabaseImport( char * ),
    CommandDatabaseRollout( char * ),
    CommandDatabaseGenerate( char * ),
    CommandDatabaseTrain( char * ),
    CommandDatabaseVerify( char * ),
    CommandDecline( char * ),
    CommandDiceRolls( char * ),
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
    CommandExportMatchSGG( char * ),
    CommandExportMatchTMG( char * ),
    CommandExportPositionEPS( char * ),
    CommandExportPositionPNG( char * ),
    CommandExportPositionJF( char * ),
    CommandExportPositionHtml( char * ),
    CommandExportPositionText( char * ),
    CommandExportPositionGammOnLine ( char * ),
    CommandExportPositionGOL2Clipboard ( char * ),
    CommandExportPositionSnowieTxt( char * ),
    CommandExternal( char * ),
    CommandFirstGame( char * ),
    CommandFirstMove( char * ),
    CommandHelp( char * ),
    CommandHint( char * ),
    CommandHistory( char * ),
    CommandImportBKG( char * ),
	CommandImportEmpire( char * ),
    CommandImportJF( char * ),
    CommandImportMat( char * ),
    CommandImportOldmoves( char * ),
	CommandImportParty( char * ),
    CommandImportSGG( char * ),
    CommandImportSnowieTxt( char * ),
    CommandImportTMG( char * ),
    CommandListGame( char * ),
    CommandListMatch( char * ),
    CommandLoadCommands( char * ),
    CommandLoadGame( char * ),
    CommandLoadMatch( char * ),
    CommandLoadPosition( char * ),
    CommandLoadPython( char * ),
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
    CommandRelationalAddMatch( char * ),
    CommandRelationalAddEnvironment( char * ),
    CommandRelationalErase( char * ),
    CommandRelationalEraseEnv( char * ),
    CommandRelationalEraseAll( char * ),
    CommandRelationalRenameEnv( char * ),
    CommandRelationalSelect( char * ),
    CommandRelationalShowEnvironments( char * ),
    CommandRelationalShowPlayers( char * ),
    CommandRelationalShowDetails( char * ),
    CommandRelationalTest( char * ),
    CommandRelationalHelp( char * ),
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
    CommandSetAnalysisPlayer( char * ),
    CommandSetAnalysisPlayerAnalyse( char * ),
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
    CommandSetAnalysisWindows ( char * ),
    CommandSetAnnotation( char * ),
    CommandSetAppearance( char * ),
    CommandSetAutoAnalysis( char * ),
    CommandSetAutoBearoff( char * ),
    CommandSetAutoCrawford( char * ),
    CommandSetAutoDoubles( char * ),
    CommandSetAutoGame( char * ),
    CommandSetAutoMove( char * ),
    CommandSetAutoRoll( char * ),
    CommandSetBearoffSconyers15x15DVDEnable( char * ),
    CommandSetBearoffSconyers15x15DVDPath( char * ),
    CommandSetBearoffSconyers15x15DiskEnable( char * ),
    CommandSetBearoffSconyers15x15DiskPath( char * ),
    CommandSetBoard( char * ),
    CommandSetBeavers( char * ),
    CommandSetCache( char * ),
    CommandSetCalibration( char * ),
    CommandSetCheatEnable ( char * ),
    CommandSetCheatPlayer ( char * ),
    CommandSetCheatPlayerRoll ( char * ),
    CommandSetClockwise( char * ),
    CommandSetCommandWindow ( char * ),
    CommandSetConfirmNew( char * ),
    CommandSetConfirmSave( char * ),
    CommandSetCrawford( char * ),
    CommandSetCubeCentre( char * ),
    CommandSetCubeOwner( char * ),
    CommandSetCubeUse( char * ),
    CommandSetCubeValue( char * ),
    CommandSetCubeEfficiencyOS( char * ),
    CommandSetCubeEfficiencyRaceFactor( char * ),
    CommandSetCubeEfficiencyRaceCoefficient( char * ),
    CommandSetCubeEfficiencyRaceMax( char * ),
    CommandSetCubeEfficiencyRaceMin( char * ),
    CommandSetCubeEfficiencyCrashed( char * ),
    CommandSetCubeEfficiencyContact( char * ),
    CommandSetDelay( char * ),
    CommandSetDice( char * ),
    CommandSetDisplay( char * ),
    CommandSetDisplayPanels( char *),
    CommandSetDockPanels( char *),
    CommandSetEvalCandidates( char * ),
    CommandSetEvalCubeful( char * ),
    CommandSetEvalDeterministic( char * ),
    CommandSetEvalNoOnePlyPrune( char * ),
    CommandSetEvalNoise( char * ),    
    CommandSetEvalPlies( char * ),
    CommandSetEvalReduced ( char * ),
    CommandSetEvalPrune ( char * ),
    CommandSetEvalTolerance( char * ),
    CommandSetEvaluation( char * ),
    CommandSetEvalParamType( char * ),
    CommandSetEvalParamRollout( char * ),
    CommandSetEvalParamEvaluation( char * ),
    CommandSetEvalChequerplay ( char * ),
    CommandSetEvalCubedecision ( char * ),
    CommandSetEvalMoveFilter ( char * ),
    CommandSetEgyptian( char * ),
    CommandSetExportFormat(char *sz),
    CommandSetExportFolder(char *sz),
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
    CommandSetExportHtmlSize ( char *),
    CommandSetFullScreen ( char * ),
    CommandSetGameList ( char * ),
    CommandSetGeometryAnalysis( char * ),
    CommandSetGeometryCommand ( char * ),
    CommandSetGeometryGame ( char * ),
    CommandSetGeometryHint ( char * ),
    CommandSetGeometryMain ( char * ),
    CommandSetGeometryMessage ( char * ),
    CommandSetGeometryTheory ( char * ),
    CommandSetGeometryWidth ( char * ),
    CommandSetGeometryHeight ( char * ),
    CommandSetGeometryPosX ( char * ),
    CommandSetGeometryPosY ( char * ),
    CommandSetGeometryMax ( char * ),
    CommandSetGotoFirstGame( char * ),
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
    CommandSetGUIShowEPCs( char * ),
    CommandSetGUIDragTargetHelp( char * ),
    CommandSetGUIUseStatsPanel( char * ),
    CommandSetGUIMoveListDetail( char * ),
    CommandSetImportFormat(char *sz),
    CommandSetImportFolder(char *sz),
    CommandSetInvertMatchEquityTable( char * ),
    CommandSetJacoby( char * ),
    CommandSetLang( char * ),
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
    CommandSetOutputDigits( char *),
    CommandSetOutputMatchPC( char * ),
    CommandSetOutputMWC ( char * ),
    CommandSetOutputRawboard( char * ),
    CommandSetOutputWinPC( char * ),
    CommandSetOutputErrorRateFactor( char * ),
    CommandSetPanelWidth( char * ),
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
    CommandSetPathPNG( char * ),
    CommandSetPathPos( char * ),
    CommandSetPathGam( char * ),
    CommandSetPathPostScript( char * ),
    CommandSetPathSnowieTxt( char * ),
    CommandSetPathText( char * ),
    CommandSetPathBKG( char * ),
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
    CommandSetRatingOffset( char * ),
    CommandSetRecord( char * ),
    CommandSetRNG( char * ),
    CommandSetRNGAnsi( char * ),
    CommandSetRNGBBS( char * ),
    CommandSetRNGBsd( char * ),
    CommandSetRNGFile( char * ),
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
    CommandSetRolloutLogEnable ( char * ),
    CommandSetRolloutLogFile ( char * ),
    CommandSetRolloutCubeful ( char * ),
    CommandSetRolloutChequerplay ( char * ),
    CommandSetRolloutCubeEqualChequer ( char * ),
    CommandSetRolloutInitial( char * ),
    CommandSetRolloutJsd( char * ),
    CommandSetRolloutJsdEnable( char * ),
    CommandSetRolloutJsdLimit( char * ),
    CommandSetRolloutJsdMinGames( char * ),
    CommandSetRolloutJsdMoveEnable( char * ),
    CommandSetRolloutLimit( char * ),
    CommandSetRolloutLimitEnable( char * ),
    CommandSetRolloutLimitMinGames( char * ),
    CommandSetRolloutMaxError( char * ),
    CommandSetRolloutMoveFilter( char * ),
    CommandSetRolloutPlayer ( char * ),
    CommandSetRolloutPlayerChequerplay ( char * ),
    CommandSetRolloutPlayerCubedecision ( char * ),
    CommandSetRolloutPlayerMoveFilter( char * ),
    CommandSetRolloutPlayerLateChequerplay ( char * ),
    CommandSetRolloutPlayerLateCubedecision ( char * ),
    CommandSetRolloutPlayerLateMoveFilter( char * ),
    CommandSetRolloutPlayersAreSame( char * ),
    CommandSetRolloutLate ( char * ),
    CommandSetRolloutLateChequerplay ( char * ),
    CommandSetRolloutLateMoveFilter( char * ),
    CommandSetRolloutLateEnable ( char * ),
    CommandSetRolloutLatePlayer ( char * ),
    CommandSetRolloutLatePlies ( char * ),
    CommandSetRolloutTruncationChequer ( char * ),
    CommandSetRolloutTruncationCube ( char * ),
    CommandSetRolloutTruncationEnable ( char * ),
    CommandSetRolloutTruncationEqualPlayer0 (char *),
    CommandSetRolloutTruncationPlies ( char * ),    
    CommandSetRolloutRNG ( char * ),
    CommandSetRolloutRotate ( char * ),
    CommandSetRolloutSeed( char * ),
    CommandSetRolloutTrials( char * ),
    CommandSetRolloutTruncation( char * ),
    CommandSetRolloutVarRedn( char * ),
    CommandSetScore( char * ),
    CommandSetSeed( char * ),
    CommandSetSGFFolder(char *sz),
    CommandSetSoundEnable ( char * ),
    CommandSetSoundSystemArtsc ( char * ),
    CommandSetSoundSystemCommand ( char * ),
    CommandSetSoundSystemESD ( char * ),
    CommandSetSoundSystemNAS ( char * ),
    CommandSetSoundSystemNormal ( char * ),
    CommandSetSoundSystemWindows ( char * ),
    CommandSetSoundSystemQuickTime ( char * ),
    CommandSetSoundSoundAgree ( char * ),
    CommandSetSoundSoundAnalysisFinished ( char * ),
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
    CommandSetStyledGameList ( char * ),
    CommandSetSoundSoundTake ( char * ),
#if USE_TIMECONTROL
    CommandSetTCMove( char * ),
    CommandSetTCMultiplier( char * ),
    CommandSetTCName( char * ),
    CommandSetTCNext( char * ),
    CommandSetTCPenalty( char * ),
    CommandSetTCPoint( char * ),
    CommandSetTCTime( char * ),
    CommandSetTCTransition( char * ),
    CommandSetTCType( char * ),
    CommandSetTCUnname( char * ),
    CommandSetTimeControl( char * ),
#endif
    CommandSetTheoryWindow ( char * ),
    CommandSetToolbar( char * ),
    CommandSetTrainingAlpha( char * ),
    CommandSetTrainingAnneal( char * ),
    CommandSetTrainingThreshold( char * ),
    CommandSetTurn( char * ),
    CommandSetTutorCube( char * ),
    CommandSetTutorEval( char* sz ),
    CommandSetTutorChequer( char * ),
    CommandSetTutorMode( char * ),  
    CommandSetTutorSkillDoubtful( char* sz ),
    CommandSetTutorSkillBad( char* sz ), 
    CommandSetTutorSkillVeryBad( char* sz ),
    CommandSetVariationStandard( char * sz ),
    CommandSetVariationNackgammon( char * sz ),
    CommandSetVariation1ChequerHypergammon( char * sz ),
    CommandSetVariation2ChequerHypergammon( char * sz ),
    CommandSetVariation3ChequerHypergammon( char * sz ),
    CommandSetWarning( char * ),
    CommandShowAnalysis( char * ),
    CommandShowAutomatic( char * ),
    CommandShowBoard( char * ),
    CommandShowBeavers( char * ),
    CommandShowBuildInfo( char * ),
    CommandShowCache( char * ),
    CommandShowCalibration( char * ),
    CommandShowCheat( char * ),
    CommandShowClockwise( char * ),
    CommandShowCommands( char * ),
    CommandShowConfirm( char * ),
    CommandShowCopying( char * ),
    CommandShowCrawford( char * ),
    CommandShowCredits( char * ),
    CommandShowCube( char * ),
    CommandShowCubeEfficiency( char * ),
    CommandShowDelay( char * ),
    CommandShowDice( char * ),
    CommandShowDisplay( char * ),
    CommandShowDisplayPanels( char * ),
    CommandShowEngine( char * ),
    CommandShowEPC( char * ),
    CommandShowEvaluation( char * ),
    CommandShowExport ( char * ),
    CommandShowFullBoard( char * ),
    CommandShowGammonValues( char * ),
    CommandShowGeometry ( char * ),
    CommandShowEgyptian( char * ),
    CommandShowJacoby( char * ),
    CommandShowKeith( char * ),
    CommandShowKleinman( char * ),
    CommandShowLang( char * ),
    CommandShowManualGUI( char * ),
    CommandShowManualWeb( char * ),
    CommandShowMarketWindow( char * ),
    CommandShowMatchInfo( char * ),
    CommandShowMatchLength( char * ),
    CommandShowMatchEquityTable( char * ),
    CommandShowMatchResult( char * ),
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
    CommandShowScoreSheet( char * ),
    CommandShowBearoff( char * ),
    CommandShowSeed( char * ),
    CommandShowSound( char * ),
    CommandShowStatisticsGame( char * ),
    CommandShowStatisticsMatch( char * ),
    CommandShowStatisticsSession( char * ),
#if USE_TIMECONTROL
    CommandShowTCList( char * ),
    CommandShowTCTutorial( char * ),
    CommandShowTimeControl( char * ),
#endif
    CommandShowTemperatureMap( char * ),
    CommandShowThorp( char * ),
    CommandShowTraining( char * ),
    CommandShowTurn( char * ),
    CommandShowTutor( char * ), 
    CommandShowVariation( char * ),
    CommandShowVersion( char * ),
    CommandShowWarning( char * ),
    CommandShowWarranty( char * ),
    CommandSwapPlayers ( char * ),
    CommandTake( char * ),
    CommandTrainTD( char * );


extern int fTutor, fTutorCube, fTutorChequer, nTutorSkillCurrent;

extern int GiveAdvice ( skilltype Skill );
extern skilltype TutorSkill;
extern int fTutorAnalysis;

extern int EvalCmp ( const evalcontext *, const evalcontext *, const int);

#define GNUBG_CHARSET "UTF-8"

extern char * locale_from_utf8 (const char * sz);

extern char * locale_to_utf8 (const char * sz);

extern void
OptimumRoll ( int anBoard[ 2 ][ 25 ], 
              const cubeinfo* pci, const evalcontext* pec,
              const int fBest, int anDice[ 2 ] );

extern void
SetMatchInfo( char **ppch, char* sz, char* szMessage );

extern void
TextToClipboard ( const char * sz );

extern void
PrintCheatRoll( const int fPlayer, const int n );

extern void
ShowBearoff( char* sz, matchstate* pms, bearoffcontext* pbc );

extern int
EPC( int anBoard[ 2 ][ 25 ], float *arEPC, float *arMu, float *arSigma, 
     int *pfSource, const int fOnlyBearoff );

extern char *
ShowEPC( int anBoard[ 2 ][ 25 ] );

extern void SetupLanguage(char *newLangCode);
extern void SaveRolloutSettings ( FILE *pf, char *sz, rolloutcontext *prc );

#endif
