/*
 * gnubg.c
 *
 * by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002.
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

#include "config.h"

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#if HAVE_PWD_H
#include <pwd.h>
#endif
#include <signal.h>
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
static int fReadingOther;
static char szCommandSeparators[] = " \t\n\r\v\f";
#endif

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "getopt.h"
#include "positionid.h"
#include "rollout.h"
#include "matchequity.h"
#include "analysis.h"
#include "import.h"
#include "export.h"

#if USE_GUILE
#include <libguile.h>
#include "guile.h"
#endif

#if USE_GTK
#include <gtk/gtk.h>
#if HAVE_GDK_GDKX_H
#include <gdk/gdkx.h>
#endif
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#elif USE_EXT
#include <ext.h>
#include <extwin.h>
#include "xgame.h"

extwindow ewnd;
event evNextTurn;
#endif

#ifdef WIN32
#include<windows.h>
#endif

#if defined(MSDOS) || defined(__MSDOS__) || defined(WIN32)
#define NO_BACKSLASH_ESCAPES 1
#endif

#if USE_GUI
int fX = TRUE; /* use X display */
int nDelay = 300;
int fNeedPrompt = FALSE;
#if HAVE_LIBREADLINE
int fReadingCommand;
#endif
#endif

#if HAVE_LIBREADLINE
int fReadline = TRUE;
#endif

#if !defined(SIGIO) && defined(SIGPOLL)
#define SIGIO SIGPOLL /* The System V equivalent */
#endif

char szDefaultPrompt[] = "(\\p) ",
    *szPrompt = szDefaultPrompt;
static int fInteractive, cOutputDisabled, cOutputPostponed;

matchstate ms = {
    {}, /* anBoard */
    {}, /* anDice */
    -1, /* fTurn */
    0, /* fResigned */
    0, /* fResignationDeclined */
    FALSE, /* fDoubled */
    0, /* cGames */
    -1, /* fMove */
    -1, /* fCubeOwner */
    FALSE, /* fCrawford */
    FALSE, /* fPostCrawford */
    0, /* nMatchTo */
    { 0, 0 }, /* anScore */
    1, /* nCube */
    0, /* cBeavers */
    GAME_NONE /* gs */
};

int fDisplay = TRUE, fAutoBearoff = FALSE, fAutoGame = TRUE, fAutoMove = FALSE,
    fAutoCrawford = 1, fAutoRoll = TRUE, cAutoDoubles = 0,
    fCubeUse = TRUE, fNackgammon = FALSE, 
    fConfirm = TRUE, fShowProgress, fJacoby = TRUE,
    nBeavers = 3, fOutputMWC = TRUE, fOutputWinPC = FALSE,
    fOutputMatchPC = TRUE, fOutputRawboard = FALSE, 
    fAnnotation = FALSE, cAnalysisMoves = 20, fAnalyseCube = TRUE,
    fAnalyseDice = TRUE, fAnalyseMove = TRUE, fRecord = TRUE;

int fNextTurn = FALSE, fComputing = FALSE;

float rAlpha = 0.1f, rAnneal = 0.3f, rThreshold = 0.1f,
    arLuckLevel[] = {
	0.6f, /* LUCK_VERYBAD */
	0.3f, /* LUCK_BAD */
	0, /* LUCK_NONE */
	0.3f, /* LUCK_GOOD */
	0.6f /* LUCK_VERYGOOD */
    }, arSkillLevel[] = {
	0.16f, /* SKILL_VERYBAD */
	0.08f, /* SKILL_BAD */
	0.04f, /* SKILL_DOUBTFUL */
	0, /* SKILL_NONE */
	0, /* SKILL_INTERESTING */
	0.02f, /* SKILL_GOOD */
	0.04f /* SKILL_VERYGOOD	*/
    };

evalcontext ecTD = { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 };

rolloutcontext rcRollout =
{ 
  {
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 }, /* player 0 cube decision */
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 } /* player 1 cube decision */
  }, 
  {
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 }, /* player 0 chequerplay */
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 } /* player 1 chequerplay */
  }, 
  FALSE, /* cubeful */
  FALSE, /* variance reduction */
  FALSE, /* initial position */
  7, /* truncation */
  36, /* number of trials */
  RNG_MERSENNE, /* RNG */
  0 /* seed */
};

/* parameters for `eval' and `hint' */

#define EVALSETUP { \
  /* evaltype */ \
  EVAL_EVAL, \
  /* evalcontext */ \
  { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 }, \
  /* rolloutcontext */ \
  { \
    { \
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 }, /* player 0 cube decision */ \
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 } /* player 1 cube decision */ \
    }, \
    { \
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 }, /* player 0 chequerplay */ \
      { 8, FALSE, 0, 0, TRUE, 0.16, 0.0 } /* player 1 chequerplay */ \
    }, \
    FALSE, /* cubeful */ \
    FALSE, /* variance reduction */ \
    FALSE, /* initial position */ \
    7, /* truncation */ \
    36, /* number of trials */ \
    RNG_MERSENNE, /* RNG */ \
    0 /* seed */ \
  } \
} 

evalsetup esEvalChequer = EVALSETUP;
evalsetup esEvalCube = EVALSETUP;
evalsetup esAnalysisChequer = EVALSETUP;
evalsetup esAnalysisCube = EVALSETUP;

exportsetup exsExport = {
  TRUE, /* include annotations */
  TRUE, /* include analysis */
  TRUE, /* include statistics */
  TRUE, /* include legend */
  1, /* display board for all moves */
  EXPORT_SIDE_PLAYER0 | EXPORT_SIDE_PLAYER1, 
  5, /* display max 5 moves */
  TRUE, /* show detailed probabilities */
  EXPORT_MOVES_NONE, /* do not show move parameters */
  EXPORT_MOVES_ERROR | EXPORT_MOVES_BLUNDER | EXPORT_MOVES_ALL,
  TRUE, /* show detailed prob. */
  EXPORT_CUBE_NONE, /* do not show move parameters */
  EXPORT_CUBE_ACTUAL | EXPORT_CUBE_MISSED | EXPORT_CUBE_CLOSE 
};

  
#define DEFAULT_NET_SIZE 128

storedmoves sm; /* sm.ml.amMoves is NULL, sm.anDice is [0,0] */

player ap[ 2 ] = {
    { "gnubg", PLAYER_GNU, EVALSETUP, EVALSETUP },
    { "user", PLAYER_HUMAN, EVALSETUP, EVALSETUP } 
};

/* Usage strings */
static char szDICE[] = "<die> <die>",
    szER[] = "evaluation|rollout", 
    szFILENAME[] = "<filename>",
    szKEYVALUE[] = "[<key>=<value> ...]",
    szLENGTH[] = "<length>",
    szLIMIT[] = "<limit>",
    szMILLISECONDS[] = "<milliseconds>",
    szMOVE[] = "<from> <to> ...",
    szONOFF[] = "on|off",
    szOPTCOMMAND[] = "[command]",
    szOPTFILENAME[] = "[filename]",
    szOPTLIMIT[] = "[limit]",
    szOPTPOSITION[] = "[position]",
    szOPTSEED[] = "[seed]",
    szOPTSIZE[] = "[size]",
    szOPTVALUE[] = "[value]",
    szPLAYER[] = "<player>",
    szPLIES[] = "<plies>",
    szPOSITION[] = "<position>",
    szPROMPT[] = "<prompt>",
    szRATE[] = "<rate>",
    szSCORE[] = "<score>",
    szSIZE[] = "<size>",
    szSTEP[] = "[game|roll] [count]",
    szTRIALS[] = "<trials>",
    szVALUE[] = "<value>",
    szMATCHID[] = "<matchid>";

command cER = {
    /* dummy command used for evaluation/rollout parameters */
    NULL, NULL, NULL, NULL, &cER
}, cFilename = {
    /* dummy command used for filename parameters */
    NULL, NULL, NULL, NULL, &cFilename
}, cOnOff = {
    /* dummy command used for on/off parameters */
    NULL, NULL, NULL, NULL, &cOnOff
}, cPlayer = {
    /* dummy command used for player parameters */
    NULL, NULL, NULL, NULL, &cPlayer
}, cPlayerBoth = {
    /* dummy command used for player parameters; "both" also permitted */
    NULL, NULL, NULL, NULL, &cPlayerBoth
}, acAnalyse[] = {
    { "game", CommandAnalyseGame, "Compute analysis and annotate current game",
      NULL, NULL },
    { "match", CommandAnalyseMatch, "Compute analysis and annotate every game "
      "in the match", NULL, NULL },
    { "session", CommandAnalyseSession, "Compute analysis and annotate every "
      "game in the session", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotateClear[] = {
    { "comment", CommandAnnotateClearComment, "Erase commentary about a move",
      NULL, NULL },
    { "luck", CommandAnnotateClearLuck, "Erase annotations for a dice roll",
      NULL, NULL },
    { "skill", CommandAnnotateClearSkill, "Erase skill annotations for a move",
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotate[] = {
    { "bad", CommandAnnotateBad, "Mark a bad move", NULL, NULL },
    { "clear", NULL, "Remove annotations from a move", NULL, acAnnotateClear },
    { "doubtful", CommandAnnotateDoubtful, "Mark a doubtful move", NULL,
      NULL },
    { "good", CommandAnnotateGood, "Mark a good move", NULL, NULL },
    { "interesting", CommandAnnotateInteresting, "Mark an interesting move",
      NULL, NULL },
    { "lucky", CommandAnnotateLucky, "Mark a lucky dice roll", NULL, NULL },
    { "unlucky", CommandAnnotateUnlucky, "Mark an unlucky dice roll",
      NULL, NULL },
    { "verybad", CommandAnnotateVeryBad, "Mark a very bad move", NULL, NULL },
    { "verygood", CommandAnnotateVeryGood, "Mark a very good move", NULL,
      NULL },
    { "verylucky", CommandAnnotateVeryLucky, "Mark a very lucky dice roll",
      NULL, NULL },
    { "veryunlucky", CommandAnnotateVeryUnlucky, "Mark an very unlucky dice "
      "roll", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acDatabase[] = {
    { "dump", CommandDatabaseDump, "List the positions in the database",
      NULL, NULL },
    { "export", CommandDatabaseExport, "Write the positions in the database "
      "to a portable format", szFILENAME, &cFilename },
    { "generate", CommandDatabaseGenerate, "Generate database positions by "
      "self-play", szOPTVALUE, NULL },
    { "import", CommandDatabaseImport, "Merge positions into the database",
      szFILENAME, &cFilename },
    { "rollout", CommandDatabaseRollout, "Evaluate positions in database "
      "for future training", NULL, NULL },
    { "train", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL, NULL },
    { "verify", CommandDatabaseVerify, "Measure the current network error "
      "against the database", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportGame[] = {
    { "gam", CommandExportGameGam, "Records a log of the game in .gam "
      "format", szFILENAME, &cFilename },
    { "html", CommandExportGameHtml,
      "Records a log of the game in .html format", szFILENAME,
      &cFilename },
    { "latex", CommandExportGameLaTeX, "Records a log of the game in LaTeX "
      "format", szFILENAME, &cFilename },
    { "pdf", CommandExportGamePDF, "Records a log of the game in the "
      "Portable Document Format", szFILENAME, &cFilename },
    { "postscript", CommandExportGamePostScript, "Records a log of the game "
      "in PostScript format", szFILENAME, &cFilename },
    { "ps", CommandExportGamePostScript, NULL, szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportMatch[] = {
    { "mat", CommandExportMatchMat, "Records a log of the match in .mat "
      "format", szFILENAME, &cFilename },
    { "html", CommandExportMatchHtml,
      "Records a log of the match in .html format", szFILENAME,
      &cFilename },
    { "latex", CommandExportMatchLaTeX, "Records a log of the match in LaTeX "
      "format", szFILENAME, &cFilename },
    { "pdf", CommandExportMatchPDF, "Records a log of the match in the "
      "Portable Document Format", szFILENAME, &cFilename },
    { "postscript", CommandExportMatchPostScript, "Records a log of the match "
      "in PostScript format", szFILENAME, &cFilename },
    { "ps", CommandExportMatchPostScript, NULL, szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportPosition[] = {
    { "eps", CommandExportPositionEPS, "Save the current position in "
      "Encapsulated PostScript format", szFILENAME, &cFilename },
    { "html", CommandExportPositionHtml,
      "Save the current position in .html format", szFILENAME, &cFilename },
    { "pos", CommandNotImplemented, "Save the current position in .pos "
      "format", szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportSession[] = {
    { "latex", CommandExportMatchLaTeX, "Records a log of the session in "
      "LaTeX format", szFILENAME, &cFilename },
    { "pdf", CommandExportMatchPDF, "Records a log of the session in the "
      "Portable Document Format", szFILENAME, &cFilename },
    { "postscript", CommandExportMatchPostScript, "Records a log of the "
      "session in PostScript format", szFILENAME, &cFilename },
    { "ps", CommandExportMatchPostScript, NULL, szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExport[] = {
    { "database", CommandDatabaseExport, "Write the positions in the database "
      "to a portable format", szFILENAME, &cFilename },
    { "game", NULL, "Record a log of the game so far to a file", NULL,
      acExportGame },
    { "match", NULL, "Record a log of the match so far to a file", NULL,
      acExportMatch },
    { "position", NULL, "Write the current position to a file", NULL,
      acExportPosition },
    { "session", NULL, "Record a log of the session so far to a file", NULL,
      acExportSession },
    { NULL, NULL, NULL, NULL, NULL }
}, acImport[] = {
    { "database", CommandDatabaseImport, "Merge positions into the database",
      szFILENAME, &cFilename },
    { "mat", CommandImportMat, "Import a Jellyfish match", szFILENAME,
      &cFilename },
    { "oldmoves", CommandImportOldmoves, "Import a FIBS oldmoves file",
      szFILENAME, &cFilename },
    { "pos", CommandImportJF, "Import a Jellyfish position file", szFILENAME,
      &cFilename },
    { "sgg", CommandImportSGG, "Import an SGG match", szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acList[] = {
    { "game", CommandListGame, "Show the moves made in this game", NULL,
      NULL },
    { "match", CommandListMatch, "Show the games played in this match", NULL,
      NULL },
    { "session", CommandListMatch, "Show the games played in this session",
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acLoad[] = {
    { "commands", CommandLoadCommands, "Read commands from a script file",
      szFILENAME, &cFilename },
    { "game", CommandLoadGame, "Read a saved game from a file", szFILENAME,
      &cFilename },
    { "match", CommandLoadMatch, "Read a saved match from a file", szFILENAME,
      &cFilename },
    { "weights", CommandNotImplemented, "Read neural net weights from a file",
      szOPTFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acNew[] = {
    { "game", CommandNewGame, "Start a new game within the current match or "
      "session", NULL, NULL },
    { "match", CommandNewMatch, "Play a new match to some number of points",
      szLENGTH, NULL },
    { "session", CommandNewSession, "Start a new (money) session", NULL,
      NULL },
    { "weights", CommandNewWeights, "Create new (random) neural net "
      "weights", szOPTSIZE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSave[] = {
    { "game", CommandSaveGame, "Record a log of the game so far to a "
      "file", szFILENAME, &cFilename },
    { "match", CommandSaveMatch, "Record a log of the match so far to a file",
      szFILENAME, &cFilename },
    { "settings", CommandSaveSettings, "Use the current settings in future "
      "sessions", NULL, NULL },
    { "weights", CommandSaveWeights, "Write the neural net weights to a file",
      szOPTFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetAnalysisThreshold[] = {
    { "bad", CommandSetAnalysisThresholdBad, "Specify the equity loss for a "
      "bad move", szVALUE, NULL },
    { "doubtful", CommandSetAnalysisThresholdDoubtful, 
      "Specify the equity loss for a doubtful move", szVALUE, NULL },
    { "good", CommandSetAnalysisThresholdGood, "Specify the equity gain for a "
      "good move", szVALUE, NULL },
    { "interesting", CommandSetAnalysisThresholdInteresting, "Specify the "
      "equity gain for an interesting move", szVALUE, NULL },
    { "lucky", CommandSetAnalysisThresholdLucky, "Specify the equity gain for "
      "a lucky roll", szVALUE, NULL },
    { "unlucky", CommandSetAnalysisThresholdUnlucky, "Specify the equity loss "
      "for an unlucky roll", szVALUE, NULL },
    { "verybad", CommandSetAnalysisThresholdVeryBad, "Specify the equity loss "
      "for a very bad move", szVALUE, NULL },
    { "verygood", CommandSetAnalysisThresholdVeryGood, "Specify the equity "
      "gain for a very good move", szVALUE, NULL },
    { "verylucky", CommandSetAnalysisThresholdVeryLucky, "Specify the equity "
      "gain for a very lucky roll", szVALUE, NULL },
    { "veryunlucky", CommandSetAnalysisThresholdVeryUnlucky, "Specify the "
      "equity loss for a very unlucky roll", szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetEvalParam[] = {
  { "type", CommandSetEvalParamType,
    "Specify type (evaluation or rollout)", szER, &cER },
  { "evaluation", CommandSetEvalParamEvaluation,
    "Speficy parameters for neural net evaluation", NULL,
    acSetEvaluation },
  { "rollout", CommandSetEvalParamRollout,
    "Speficy parameters for rollout", NULL,
    acSetRollout },
  { NULL, NULL, NULL, NULL, NULL }
}, acSetAnalysis[] = {
    { "chequerplay", CommandSetAnalysisChequerplay, "Specify parameters "
      "for the analysis of chequerplay", NULL, acSetEvalParam },
    { "cube", CommandSetAnalysisCube, "Select whether cube action will be "
      "analysed", szONOFF, &cOnOff },
    { "cubedecision", CommandSetAnalysisCubedecision, "Specify parameters "
      "for the analysis of cube decisions", NULL,
      acSetEvalParam },
    { "limit", CommandSetAnalysisLimit, "Specify the maximum number of "
      "possible moves analysed", szOPTLIMIT, NULL },
    { "luck", CommandSetAnalysisLuck, "Select whether dice rolls will be "
      "analysed", szONOFF, &cOnOff },
    { "moves", CommandSetAnalysisMoves, "Select whether chequer play will be "
      "analysed", szONOFF, &cOnOff },
    { "threshold", NULL, "Specify levels for marking moves", NULL,
      acSetAnalysisThreshold },
    { NULL, NULL, NULL, NULL, NULL }    
}, acSetAutomatic[] = {
    { "bearoff", CommandSetAutoBearoff, "Automatically bear off as many "
      "chequers as possible", szONOFF, &cOnOff },
    { "crawford", CommandSetAutoCrawford, "Enable the Crawford game "
      "based on match score", szONOFF, &cOnOff },
    { "doubles", CommandSetAutoDoubles, "Control automatic doubles "
      "during (money) session play", szLIMIT, NULL },
    { "game", CommandSetAutoGame, "Select whether to start new games "
      "after wins", szONOFF, &cOnOff },
    { "move", CommandSetAutoMove, "Select whether forced moves will be "
      "made automatically", szONOFF, &cOnOff },
    { "roll", CommandSetAutoRoll, "Control whether dice will be rolled "
      "automatically", szONOFF, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetCube[] = {
    { "center", CommandSetCubeCentre, "The U.S.A. spelling of `centre'",
      NULL, NULL },
    { "centre", CommandSetCubeCentre, "Allow both players access to the "
      "cube", NULL, NULL },
    { "owner", CommandSetCubeOwner, "Allow only one player to double",
      szPLAYER, &cPlayerBoth },
    { "use", CommandSetCubeUse, "Control use of the doubling cube", szONOFF,
      &cOnOff },
    { "value", CommandSetCubeValue, "Fix what the cube stake has been set to",
      szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetOutput[] = {
    { "matchpc", CommandSetOutputMatchPC,
      "Show match equities as percentages (on) or probabilities (off)",
      szONOFF, &cOnOff },
    { "mwc", CommandSetOutputMWC, "Show output in MWC (on) or "
      "equity (off) (match play only)", szONOFF, &cOnOff },
    { "rawboard", CommandSetOutputRawboard, "Give FIBS \"boardstyle 3\" "
      "output (on), or an ASCII board (off)", szONOFF, &cOnOff },
    { "winpc", CommandSetOutputWinPC,
      "Show winning chances as percentages (on) or probabilities (off)",
      szONOFF, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRNG[] = {
    { "ansi", CommandSetRNGAnsi, "Use the ANSI C rand() (usually linear "
      "congruential) generator", szOPTSEED, NULL },
    { "bsd", CommandSetRNGBsd, "Use the BSD random() non-linear additive "
      "feedback generator", szOPTSEED, NULL },
    { "isaac", CommandSetRNGIsaac, "Use the I.S.A.A.C. generator", szOPTSEED,
      NULL },
    { "manual", CommandSetRNGManual, "Enter all dice rolls manually", NULL,
      NULL },
    { "md5", CommandSetRNGMD5, "Use the MD5 generator", szOPTSEED, NULL },
    { "mersenne", CommandSetRNGMersenne, "Use the Mersenne Twister generator",
      szOPTSEED, NULL },
    { "user", CommandSetRNGUser, "Specify an external generator", szOPTSEED,
      NULL,},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutPlayer[] = {
    { "chequerplay", CommandSetRolloutPlayerChequerplay, "Specify parameters "
      "for chequerplay during rollouts", NULL, acSetEvaluation },
    { "cubedecision", CommandSetRolloutPlayerCubedecision,
      "Specify parameters for cube decisions during rollouts",
      NULL, acSetEvaluation },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRollout[] = {
    { "chequerplay", CommandSetRolloutChequerplay, "Specify parameters "
      "for chequerplay during rollouts", NULL, acSetEvaluation },
    { "cubedecision", CommandSetRolloutCubedecision, "Specify parameters "
      "for cube decisions during rollouts", NULL, acSetEvaluation },
    { "cubeful", CommandSetRolloutCubeful, "Specify whether the "
      "rollout is cubeful or cubeless", szONOFF, &cOnOff },
    { "initial", CommandSetRolloutInitial, "Roll out as the initial position "
      "of a game", szONOFF, &cOnOff },
    { "player", CommandSetRolloutPlayer, "Control evaluation "
      "parameters for each side individually", szPLAYER, acSetRolloutPlayer }, 
    { "rng", CommandSetRolloutRNG, "Specify the random number "
      "generator algorithm for rollouts", NULL, acSetRNG },
    { "seed", CommandSetRolloutSeed, "Specify the base pseudo-random seed "
      "to use for rollouts", szOPTSEED, NULL },
    { "trials", CommandSetRolloutTrials, "Control how many rollouts to "
      "perform", szTRIALS, NULL },
    { "truncation", CommandSetRolloutTruncation, "End rollouts at a "
      "particular depth", szPLIES, NULL },
    { "varredn", CommandSetRolloutVarRedn, "Use lookahead during rollouts "
      "to reduce variance", szONOFF, &cOnOff },
    /* FIXME add commands for cubeful rollouts, cube variance reduction,
       quasi-random dice, settlements... */
    { NULL, NULL, NULL, NULL, NULL }
}, acSetTraining[] = {
    { "alpha", CommandSetTrainingAlpha, "Control magnitude of backpropagation "
      "of errors", szVALUE, NULL },
    { "anneal", CommandSetTrainingAnneal, "Decrease alpha as training "
      "progresses", szRATE, NULL },
    { "threshold", CommandSetTrainingThreshold, "Require a minimum error in "
      "position database generation", szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acSetEval[] = {
  { "chequerplay", CommandSetEvalChequerplay,
    "Set evaluation parameters for chequer play", NULL,
    acSetEvalParam },
  { "cubedecision", CommandSetEvalCubedecision,
    "Set evaluation parameters for cube decisions", NULL,
    acSetEvalParam },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSet[] = {
    { "analysis", NULL, "Control parameters used when analysing moves",
      NULL, acSetAnalysis },
    { "annotation", CommandSetAnnotation, "Select whether move analysis and "
      "commentary are shown", szONOFF, &cOnOff },
    { "appearance", CommandSetAppearance, "Modify the look and feel of the "
      "graphical interface", szKEYVALUE, NULL },
    { "automatic", NULL, "Perform certain functions without user input",
      NULL, acSetAutomatic },
    { "beavers", CommandSetBeavers, 
      "Set whether beavers are allowed in money game or not", 
      szONOFF, &cOnOff },
    { "board", CommandSetBoard, "Set up the board in a particular "
      "position", szPOSITION, NULL },
    { "cache", CommandSetCache, "Set the size of the evaluation cache",
      szSIZE, NULL },
    { "clockwise", CommandSetClockwise, "Control the board orientation",
      szONOFF, &cOnOff },
    { "colours", CommandSetAppearance, "Synonym for `set appearance'", NULL,
      NULL },
    { "confirm", CommandSetConfirm, "Ask for confirmation before aborting "
      "a game in progress", szONOFF, &cOnOff },
    { "crawford", CommandSetCrawford, 
      "Set whether this is the Crawford game", szONOFF, &cOnOff },
    { "cube", NULL, "Set the cube owner and/or value", NULL, acSetCube },
    { "delay", CommandSetDelay, "Limit the speed at which moves are made",
      szMILLISECONDS, NULL },
    { "dice", CommandSetDice, "Select the roll for the current move",
      szDICE, NULL },
    { "display", CommandSetDisplay, "Select whether the board is updated on "
      "the computer's turn", szONOFF, &cOnOff },
    { "evaluation", NULL, "Control position evaluation "
      "parameters", NULL, acSetEval },
    { "egyptian", CommandSetEgyptian, "Set whether to use the Egyptian rule in "      "games", szONOFF, &cOnOff },
    { "jacoby", CommandSetJacoby, "Set whether to use the Jacoby rule in "
      "money games", szONOFF, &cOnOff },
    { "matchequitytable", CommandSetMET,
      "Read match equity table from XML file", szFILENAME, &cFilename },
    { "matchid", CommandSetMatchID, "set Match ID", szMATCHID, NULL },
    { "met", CommandSetMET,
      "Synonym for `set matchequitytable'", szFILENAME, &cFilename },
    { "nackgammon", CommandSetNackgammon, "Set the starting position",
      szONOFF, &cOnOff },
    { "output", NULL, "Modify options for formatting results", NULL,
      acSetOutput },
    { "player", CommandSetPlayer, "Change options for one or both "
      "players", szPLAYER, acSetPlayer },
    { "postcrawford", CommandSetPostCrawford, 
      "Set whether this is a post-Crawford game", szONOFF, &cOnOff },
    { "prompt", CommandSetPrompt, "Customise the prompt gnubg prints when "
      "ready for commands", szPROMPT, NULL },
    { "record", CommandSetRecord, "Set whether all games in a session are "
      "recorded", szONOFF, &cOnOff },
    { "rng", NULL, "Select the random number generator algorithm", NULL,
      acSetRNG },
    { "rollout", CommandSetRollout, "Control rollout parameters",
      NULL, acSetRollout }, 
    { "score", CommandSetScore, "Set the match or session score ",
      szSCORE, NULL },
    { "seed", CommandSetSeed, "Set the dice generator seed", szOPTSEED, NULL },
    { "training", NULL, "Control training parameters", NULL, acSetTraining },
    { "turn", CommandSetTurn, "Set which player is on roll", szPLAYER,
      &cPlayer },
    { NULL, NULL, NULL, NULL, NULL }
}, acShowStatistics[] = {
    { "game", CommandShowStatisticsGame, "Compute statistics for current game",
      NULL, NULL },
    { "match", CommandShowStatisticsMatch, "Compute statistics for every game "
      "in the match", NULL, NULL },
    { "session", CommandShowStatisticsSession, "Compute statistics for every "
      "game in the session", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acShow[] = {
    { "analysis", CommandShowAnalysis, "Show parameters used for analysing "
      "moves", NULL, NULL },
    { "automatic", CommandShowAutomatic, "List which functions will be "
      "performed without user input", NULL, NULL },
    { "beavers", CommandShowBeavers, 
      "Show whether beavers are allowed in money game or not", 
      NULL, NULL },
    { "board", CommandShowBoard, "Redisplay the board position", szOPTPOSITION,
      NULL },
    { "cache", CommandShowCache, "Display statistics on the evaluation "
      "cache", NULL, NULL },
    { "clockwise", CommandShowClockwise, "Display the board orientation",
      NULL, NULL },
    { "commands", CommandShowCommands, "List all available commands",
      NULL, NULL },
    { "confirm", CommandShowConfirm, "Show whether confirmation is required "
      "before aborting a game", NULL, NULL },
    { "copying", CommandShowCopying, "Conditions for redistributing copies "
      "of GNU Backgammon", NULL, NULL },
    { "crawford", CommandShowCrawford, 
      "See if this is the Crawford game", NULL, NULL },
    { "cube", CommandShowCube, "Display the current cube value and owner",
      NULL, NULL },
    { "delay", CommandShowDelay, "See what the current delay setting is", 
      NULL, NULL },
    { "dice", CommandShowDice, "See what the current dice roll is", NULL,
      NULL },
    { "display", CommandShowDisplay, "Show whether the board will be updated "
      "on the computer's turn", NULL, NULL },
    { "engine", CommandShowEngine, "Display the status of the evaluation "
      "engine", NULL, NULL },
    { "evaluation", CommandShowEvaluation, "Display evaluation settings "
      "and statistics", NULL, NULL },
    { "gammonvalues", CommandShowGammonValues, "Show gammon values",
      NULL, NULL },
    { "egyptian", CommandShowEgyptian,
      "See if the Egyptian rule is used in sessions", NULL, NULL },
    { "jacoby", CommandShowJacoby, 
      "See if the Jacoby rule is used in money sessions", NULL, NULL },
    { "kleinman", CommandShowKleinman, "Calculate Kleinman count for "
      "position", szOPTPOSITION, NULL },
    { "marketwindow", CommandShowMarketWindow, 
      "show market window for doubles", NULL, NULL },
    { "matchequitytable", CommandShowMatchEquityTable, 
      "Show match equity table", szOPTVALUE, NULL },
    { "met", CommandShowMatchEquityTable, 
      "Synonym for `show matchequitytable'", szOPTVALUE, NULL },
    { "nackgammon", CommandShowNackgammon, "Display which starting position "
      "will be used", NULL, NULL },
    { "output", CommandShowOutput, "Show how results will be formatted",
      NULL, NULL },
    { "pipcount", CommandShowPipCount, "Count the number of pips each player "
      "must move to bear off", szOPTPOSITION, NULL },
    { "player", CommandShowPlayer, "View per-player options", NULL, NULL },
    { "postcrawford", CommandShowCrawford, 
      "See if this is post-Crawford play", NULL, NULL },
    { "prompt", CommandShowPrompt, "Show the prompt that will be printed "
      "when ready for commands", NULL, NULL },
    { "rng", CommandShowRNG, "Display which random number generator "
      "is being used", NULL, NULL },
    { "rollout", CommandShowRollout, "Display the evaluation settings used "
      "during rollouts", NULL, NULL },
    { "score", CommandShowScore, "View the match or session score ",
      NULL, NULL },
    { "seed", CommandShowSeed, "Show the dice generator seed", NULL, NULL },
    { "statistics", NULL, "Show statistics", NULL, acShowStatistics },
    { "thorp", CommandShowThorp, "Calculate Thorp Count for "
      "position", szOPTPOSITION, NULL },
    { "training", CommandShowTraining, "Display the training parameters",
      NULL, NULL },
    { "turn", CommandShowTurn, "Show which player is on roll", NULL, NULL },
    { "version", CommandShowVersion, "Describe this version of GNU Backgammon",
      NULL, NULL },
    { "warranty", CommandShowWarranty, "Various kinds of warranty you do "
      "not have", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acTrain[] = {
    { "database", CommandDatabaseTrain, "Train the network from a database of "
      "positions", NULL, NULL },
    { "td", CommandTrainTD, "Train the network by TD(0) zero-knowledge "
      "self-play", NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acTop[] = {
    { "accept", CommandAccept, "Accept a cube or resignation",
      NULL, NULL },
    { "agree", CommandAgree, "Agree to a resignation", NULL, NULL },
    { "analyse", NULL, "Run analysis", NULL, acAnalyse },
    { "analysis", NULL, NULL, NULL, acAnalyse },
    { "analyze", NULL, NULL, NULL, acAnalyse },
    { "annotate", NULL, "Record notes about a game", NULL, acAnnotate },
    { "beaver", CommandRedouble, "Synonym for `redouble'", NULL, NULL },
    { "copy", CommandCopy, "Copy current position to clipboard", NULL, NULL },
    { "database", NULL, "Manipulate a database of positions", NULL,
      acDatabase },
    { "decline", CommandDecline, "Decline a resignation", NULL, NULL },
    { "double", CommandDouble, "Offer a double", NULL, NULL },
    { "drop", CommandDrop, "Decline an offered double", NULL, NULL },
    { "eq2mwc", CommandEq2MWC,
      "Convert normalised money equity to match winning chance",
      szVALUE, NULL },
    { "eval", CommandEval, "Display evaluation of a position", szOPTPOSITION,
      NULL },
    { "exit", CommandQuit, "Leave GNU Backgammon", NULL, NULL },
    { "export", NULL, "Write data for use by other programs", NULL, acExport },
    { "external", CommandExternal, "Make moves for an external controller",
      szFILENAME, &cFilename },
    { "help", CommandHelp, "Describe commands", szOPTCOMMAND, NULL },
    { "hint", CommandHint,  "Give hints on cube action or best legal moves", 
      szOPTVALUE, NULL }, 
    { "import", NULL, "Import matches, games or positions from other programs",
      NULL, acImport },
    { "list", NULL, "Show a list of games or moves", NULL, acList },
    { "load", NULL, "Read data from a file", NULL, acLoad },
    { "move", CommandMove, "Make a backgammon move", szMOVE, NULL },
    { "mwc2eq", CommandMWC2Eq,
      "Convert match winning chance to normalised money equity",
      szVALUE, NULL },
    { "n", CommandNext, NULL, szSTEP, NULL },
    { "new", NULL, "Start a new game, match or session", NULL, acNew },
    { "next", CommandNext, "Step ahead within the game", szSTEP, NULL },
    { "p", CommandPrevious, NULL, szSTEP, NULL },
    { "pass", CommandDrop, "Synonym for `drop'", NULL, NULL },
    { "play", CommandPlay, "Force the computer to move", NULL, NULL },
    { "previous", CommandPrevious, "Step backward within the game", szSTEP,
      NULL },
    { "quit", CommandQuit, "Leave GNU Backgammon", NULL, NULL },
    { "r", CommandRoll, NULL, NULL, NULL },
    { "redouble", CommandRedouble, "Accept the cube one level higher "
      "than it was offered", NULL, NULL },
    { "reject", CommandReject, "Reject a cube or resignation", NULL, NULL },
    { "resign", CommandResign, "Offer to end the current game", szVALUE,
      NULL },
    { "roll", CommandRoll, "Roll the dice", NULL, NULL },
    { "rollout", CommandRollout, "Have gnubg perform rollouts of a position",
      szOPTPOSITION, NULL },
    { "save", NULL, "Write data to a file", NULL, acSave },
    { "set", NULL, "Modify program parameters", NULL, acSet },
    { "show", NULL, "View program parameters", NULL, acShow },
    { "take", CommandTake, "Agree to an offered double", NULL, NULL },
    { "train", NULL, "Update gnubg's weights from training data", NULL,
      acTrain },
    { "?", CommandHelp, "Describe commands", szOPTCOMMAND, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, cTop = { NULL, NULL, NULL, NULL, acTop };

static int iProgressMax, iProgressValue, fInProgress;
static char *pcProgress;

char *aszVersion[] = {
    "GNU Backgammon " VERSION,
#if USE_GUILE
    "Guile supported.",
#endif
#if HAVE_LIBGDBM
    "Position databases supported.",
#endif
#if USE_GUI
    "Window system supported.",
#endif
#if HAVE_SOCKETS
    "External players supported.",
#endif
    NULL
};

char *szHomeDirectory, *szDataDirectory;

extern char *NextToken( char **ppch ) {

    char *pch, *pchSave, chQuote = 0;
    int fEnd = FALSE;
#ifndef NDEBUG
    char *pchEnd;
#endif
    
    if( !*ppch )
	return NULL;

#ifndef NDEBUG
    pchEnd = strchr( *ppch, 0 );
#endif
    
    /* skip leading whitespace */
    while( isspace( **ppch ) )
	( *ppch )++;

    if( !*( pch = pchSave = *ppch ) )
	/* nothing left */
	return NULL;

    while( !fEnd ) {
	switch( **ppch ) {
	case ' ':
	case '\t':
	case '\n':
	case '\r':
	case '\v':
	case '\f':
	    /* whitespace -- ends token if not quoted */
	    if( !chQuote )
		fEnd = TRUE;
	    else
		*pchSave++ = **ppch;
	    break;
	    
	case '\'':
	case '"':
	    /* quote mark */
	    if( !chQuote )
		/* start quoting */
		chQuote = **ppch;
	    else if( chQuote == **ppch )
		/* end quoting */
		chQuote = 0;
	    else
		/* literal */
		*pchSave++ = **ppch;
	    break;

#if NO_BACKSLASH_ESCAPES
	case '%':
#else
	case '\\':
#endif
	    /* backslash */
	    if( chQuote == '\'' )
		/* literal */
		*pchSave++ = **ppch;
	    else {
		( *ppch )++;

		if( **ppch )
		    /* next character is literal */
		    *pchSave++ = **ppch;
		else {
		    /* end of string -- the backlash doesn't quote anything */
#if NO_BACKSLASH_ESCAPES
		    *pchSave++ = '%';
#else
		    *pchSave++ = '\\';
#endif
		    fEnd = TRUE;
		}
	    }
	    break;
	    
	case 0:
	    /* end of string -- always ends token */
	    fEnd = TRUE;
	    break;
	    
	default:
	    *pchSave++ = **ppch;
	}

	if( !fEnd )
	    ( *ppch )++;
    }

    while( isspace( **ppch ) )
	( *ppch )++;

    *pchSave = 0;

    assert( pchSave <= pchEnd );
    assert( *ppch <= pchEnd );
    assert( pch <= pchEnd );
    
    return pch;
}

static int CountTokens( char *pch ) {

    int c = 0;

    do {
	while( isspace( *pch ) )
	    pch++;

	if( *pch ) {
	    c++;

	    while( *pch && !isspace( *pch ) )
		pch++;
	}
    } while( *pch );
    
    return c;
}

extern double ParseReal( char **ppch ) {

    char *pch, *pchOrig;
    double r;
    
    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return ERR_VAL;

    r = strtod( pchOrig, &pch );

    return *pch ? ERR_VAL : r;
}

extern int ParseNumber( char **ppch ) {

    char *pch, *pchOrig;

    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return INT_MIN;

    for( pch = pchOrig; *pch; pch++ )
	if( !isdigit( *pch ) && *pch != '-' )
	    return INT_MIN;

    return atoi( pchOrig );
}

extern int ParsePlayer( char *sz ) {

    int i;

    if( !sz )
	return -1;
    
    if( ( *sz == '0' || *sz == '1' ) && !sz[ 1 ] )
	return *sz - '0';

    for( i = 0; i < 2; i++ )
	if( !CompareNames( sz, ap[ i ].szName ) )
	    return i;

    if( !strncasecmp( sz, "both", strlen( sz ) ) )
	return 2;

    return -1;
}

/* Convert a string to a board array.  Currently allows the string to
   be a position ID, "=n" notation, or empty (in which case the current
   board is used).

   The input string should be specified in *ppch; this string must be
   modifiable, and the pointer will be updated to point to the token
   following a board specification if possible (see NextToken()).  The
   board will be returned in an, and if pchDesc is non-NULL, then
   descriptive text (the position ID, formatted move, or "Current
   position", depending on the input) will be stored there.
   
   Returns -1 on failure, 0 on success, or 1 on success if the position
   specified has the opponent on roll (e.g. because it used "=n" notation). */
extern int ParsePosition( int an[ 2 ][ 25 ], char **ppch, char *pchDesc ) {

    int i;
    char *pch;
    unsigned char auchKey[ 10 ];
    
    /* FIXME allow more formats, e.g. FIBS "boardstyle 3" */

    if( !ppch || !( pch = NextToken( ppch ) ) ) { 
	memcpy( an, ms.anBoard, sizeof( ms.anBoard ) );

	if( pchDesc )
	    strcpy( pchDesc, "Current position" );
	
	return 0;
    }

    if ( ! strcmp ( pch, "simple" ) ) {
   
       /* board given as 26 integers.
        * integer 1   : # of my chequers on the bar
        * integer 2-25: number of chequers on point 1-24
        *               positive ints: my chequers 
        *               negative ints: opp chequers 
        * integer 26  : # of opp chequers on the bar
        */
 
       int n;

       for ( i = 0; i < 26; i++ ) {

          if ( ( n = ParseNumber ( ppch ) ) == INT_MIN ) {
             outputf ("`simple' must be followed by 26 integers; "
                      "found only %d\n", i );
             return -1;
          }

          if ( i == 0 ) {
             /* my chequers on the bar */
             an[ 1 ][ 24 ] = abs(n);
          }
          else if ( i == 25 ) {
             /* opp chequers on the bar */
             an[ 0 ][ 24 ] = abs(n);
          } else {

             an[ 1 ][ i - 1 ] = 0;
             an[ 0 ][ 24 - i ] = 0;

             if ( n < 0 )
                an[ 0 ][ 24 - i ] = -n;
             else if ( n > 0 )
                an[ 1 ][ i - 1 ] = n;
             
          }
      
       }

       if( pchDesc )
          strcpy( pchDesc, *ppch );

       *ppch = NULL;
       
       return CheckPosition ( an );
    }

    if( *pch == '=' ) {
	if( !( i = atoi( pch + 1 ) ) ) {
	    outputl( "You must specify the number of the move to apply." );
	    return -1;
	}

	PositionKey( ms.anBoard, auchKey );
	
	if( !ms.anDice[ 0 ] || !EqualKeys( auchKey, sm.auchKey ) || 
	    ms.anDice[ 0 ] != sm.anDice[ 0 ] ||
	    ms.anDice[ 1 ] != sm.anDice[ 1 ] ) {
	    outputl( "There is no valid move list." );
	    return -1;
	}

	if( i > sm.ml.cMoves ) {
	    outputf( "Move =%d is out of range.\n", i );
	    return -1;
	}

	PositionFromKey( an, sm.ml.amMoves[ i - 1 ].auch );

	if( pchDesc )
	    FormatMove( pchDesc, ms.anBoard, sm.ml.amMoves[ i - 1 ].anMove );
	
	if( !ms.fMove )
	    SwapSides( an );
	
	return 1;
    }

    if( PositionFromID( an, pch ) ) {
	outputl( "Illegal position." );
	return -1;
    }

    if( pchDesc )
	strcpy( pchDesc, pch );
    
    return 0;
}

/* Parse "key=value" pairs on a command line.  PPCH takes a pointer to
   a command line on input, and returns a pointer to the next parameter.
   The key is returned in apch[ 0 ], and the value in apch[ 1 ].
   The function return value is the number of parts successfully read
   (0 = no key was found, 1 = key only, 2 = both key and value). */
extern int ParseKeyValue( char **ppch, char *apch[ 2 ] ) {

    if( !ppch || !( apch[ 0 ] = NextToken( ppch ) ) )
	return 0;

    if( !( apch[ 1 ] = strchr( apch[ 0 ], '=' ) ) )
	return 1;

    *apch[ 1 ] = 0;
    apch[ 1 ]++;
    return 2;
}

/* Compare player names.  Performed case insensitively, and with all
   whitespace characters and underscore considered identical. */
extern int CompareNames( char *sz0, char *sz1 ) {

    static char ach[] = " \t\r\n\f\v_";
    
    for( ; *sz0 || *sz1; sz0++, sz1++ )
	if( toupper( *sz0 ) != toupper( *sz1 ) &&
	    ( !strchr( ach, *sz0 ) || !strchr( ach, *sz1 ) ) )
	    return toupper( *sz0 ) - toupper( *sz1 );
    
    return 0;
}

extern void UpdateSetting( void *p ) {
#if USE_GTK
    if( fX )
	GTKSet( p );
#endif
}

extern void UpdateSettings( void ) {

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    ShowBoard();
}

extern int SetToggle( char *szName, int *pf, char *sz, char *szOn,
		       char *szOff ) {

    char *pch = NextToken( &sz );
    int cch;
    
    if( !pch ) {
	outputf( "You must specify whether to set %s on or off (see `help set "
		"%s').\n", szName, szName );

	return -1;
    }

    cch = strlen( pch );
    
    if( !strcasecmp( "on", pch ) || !strncasecmp( "yes", pch, cch ) ||
	!strncasecmp( "true", pch, cch ) ) {
	if( !*pf ) {
	    *pf = TRUE;
	    UpdateSetting( pf );
	}
	
	outputl( szOn );

	return TRUE;
    }

    if( !strcasecmp( "off", pch ) || !strncasecmp( "no", pch, cch ) ||
	!strncasecmp( "false", pch, cch ) ) {
	if( pf ) {
	    *pf = FALSE;
	    UpdateSetting( pf );
	}
	
	outputl( szOff );

	return FALSE;
    }

    outputf( "Illegal keyword `%s' -- try `help set %s'.\n", pch, szName );

    return -1;
}

extern void PortableSignal( int nSignal, RETSIGTYPE (*p)(int),
			    psighandler *pOld, int fRestart ) {
#if HAVE_SIGACTION
    struct sigaction sa;

    sa.sa_handler = p;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags =
# if SA_RESTART
	( fRestart ? SA_RESTART : 0 ) |
# endif
# if SA_NOCLDSTOP
	SA_NOCLDSTOP |
# endif
	0;
    sigaction( nSignal, p ? &sa : NULL, pOld );
#elif HAVE_SIGVEC
    struct sigvec sv;

    sv.sv_handler = p;
    sigemptyset( &sv.sv_mask );
    sv.sv_flags = nSignal == SIGINT || nSignal == SIGIO ? SV_INTERRUPT : 0;

    sigvec( nSignal, p ? &sv : NULL, pOld );
#else
    if( pOld )
	*pOld = signal( nSignal, p );
    else if( p )
	signal( nSignal, p );
#endif
}

extern void PortableSignalRestore( int nSignal, psighandler *p ) {
#if HAVE_SIGACTION
    sigaction( nSignal, p, NULL );
#elif HAVE_SIGVEC
    sigvec( nSignal, p, NULL );
#else
    signal( nSignal, (RETSIGTYPE (*)(int)) p );
#endif
}

/* Reset the SIGINT handler, on return to the main command loop.  Notify
   the user if processing had been interrupted. */
extern void ResetInterrupt( void ) {
    
    if( fInterrupt ) {
	outputl( "(Interrupted)" );
	outputx();
	
	fInterrupt = FALSE;
	
#if USE_GTK
	if( nNextTurn ) {
	    gtk_idle_remove( nNextTurn );
	    nNextTurn = 0;
	}
#elif USE_EXT
        EventPending( &evNextTurn, FALSE );
#endif
    }
}

#if USE_GUI && HAVE_FORK
static int fChildDied;

static RETSIGTYPE HandleChild( int n ) {

    fChildDied = TRUE;
}
#endif

void ShellEscape( char *pch ) {
#if HAVE_FORK
    pid_t pid;
    char *pchShell;
    psighandler shQuit;
#if USE_GUI
    psighandler shChild;
    
    PortableSignal( SIGCHLD, HandleChild, &shChild, TRUE );
#endif
    PortableSignal( SIGQUIT, SIG_IGN, &shQuit, TRUE );
    
    if( ( pid = fork() ) < 0 ) {
	/* Error */
	perror( "fork" );

#if USE_GUI
	PortableSignalRestore( SIGCHLD, &shChild );
#endif
	PortableSignalRestore( SIGQUIT, &shQuit );
	
	return;
    } else if( !pid ) {
	/* Child */
	PortableSignalRestore( SIGQUIT, &shQuit );	
	
	if( pch && *pch )
	    execl( "/bin/sh", "sh", "-c", pch, NULL );
	else {
	    if( !( pchShell = getenv( "SHELL" ) ) )
		pchShell = "/bin/sh";
	    execl( pchShell, pchShell, NULL );
	}
	_exit( EXIT_FAILURE );
    }
    
    /* Parent */
#if USE_GUI
 TryAgain:
#if HAVE_SIGPROCMASK
    {
	sigset_t ss, ssOld;

	sigemptyset( &ss );
	sigaddset( &ss, SIGCHLD );
	sigaddset( &ss, SIGIO );
	sigprocmask( SIG_BLOCK, &ss, &ssOld );
	
	while( !fChildDied ) {
	    sigsuspend( &ssOld );
	    if( fAction )
		HandleXAction();
	}

	fChildDied = FALSE;
	sigprocmask( SIG_UNBLOCK, &ss, NULL );
    }
#elif HAVE_SIGBLOCK
    {
	int n;
	
	n = sigblock( sigmask( SIGCHLD ) | sigmask( SIGIO ) );

	while( !fChildDied ) {
	    sigpause( n );
	    if( fAction )
		HandleXAction();
	}
	fChildDied = FALSE;
	sigsetmask( n );
    }
#else
    /* NB: Without reliable signal handling semantics (sigsuspend or
       sigpause), we can't avoid a race condition here because the
       test of fChildDied and pause() are not atomic. */
    while( !fChildDied ) {
	pause();
	if( fAction )
	    HandleXAction();
    }
    fChildDied = FALSE;
#endif
    
    if( !waitpid( pid, NULL, WNOHANG ) )
	/* Presumably the child is stopped and not dead. */
	goto TryAgain;
    
    PortableSignalRestore( SIGCHLD, &shChild );
#else
    while( !waitpid( pid, NULL, 0 ) )
	;
#endif

    PortableSignalRestore( SIGCHLD, &shQuit );
#else
    outputl( "This system does not support shell escapes." );
#endif
}

extern void HandleCommand( char *sz, command *ac ) {

    command *pc;
    char *pch;
    int cch;
    
    if( ac == acTop ) {
	outputnew();
    
	if( *sz == '!' ) {
	    /* Shell escape */
	    for( pch = sz + 1; isspace( *pch ); pch++ )
		;

	    ShellEscape( pch );
	    outputx();
	    return;
	} else if( *sz == ':' ) {
	    /* Guile escape */
#if USE_GUILE
	    /* FIXME can we modify our SIGIO handler to handle X events
	       directly (rather than setting fAction) while in Guile?
	       If we do that, we have to reset the handler to the flag-setter
	       when executing any callbacks from Guile.  It's probably
	       a good idea to prohibit the execution of the ":" gnubg
	       command from Guile... that's far too much reentrancy for
	       good taste! */
#if USE_GTK
	    if( fX )
		GTKDisallowStdin();
#endif
	    if( sz[ 1 ] ) {
		/* Expression specified -- evaluate it */
		SCM sResult;
		psighandler sh;

		PortableSignal( SIGINT, NULL, &sh, FALSE );
		GuileStartIntHandler();
		if( ( sResult = scm_internal_catch( SCM_BOOL_T,
				    (scm_catch_body_t) scm_eval_0str,
				    sz + 1, scm_handle_by_message_noexit,
				    NULL ) ) != SCM_UNSPECIFIED &&
		    !cOutputDisabled ) {
		    scm_write( sResult, SCM_UNDEFINED );
		    scm_newline( SCM_UNDEFINED );
		}
		GuileEndIntHandler();
		PortableSignalRestore( SIGINT, &sh );
	    } else
		/* No command -- start a Scheme shell */
		scm_eval_0str( "(top-repl)" );
#if USE_GTK
	    if( fX )
		GTKAllowStdin();
#endif
#else
	    outputl( "This installation of GNU Backgammon was compiled "
		     "without Guile support." );
	    outputx();
#endif
	    return;
	}
    }
    
    if( !( pch = NextToken( &sz ) ) ) {
	if( ac != acTop )
	    outputl( "Incomplete command -- try `help'." );

	outputx();
	return;
    }

    cch = strlen( pch );

    if( ac == acTop && ( isdigit( *pch ) ||
			 !strncasecmp( pch, "bar/", cch > 4 ? 4 : cch ) ) ) {
	if( pch + cch < sz )
	    pch[ cch ] = ' ';
	
	CommandMove( pch );

	outputx();
	return;
    }

    for( pc = ac; pc->sz; pc++ )
	if( !strncasecmp( pch, pc->sz, cch ) )
	    break;

    if( !pc->sz ) {
	outputf( "Unknown keyword `%s' -- try `help'.\n", pch );

	outputx();
	return;
    }

    if( pc->pf ) {
	pc->pf( sz );
	
	outputx();
    } else
	HandleCommand( sz, pc->pc );
}

extern void InitBoard( int anBoard[ 2 ][ 25 ] ) {

    int i;
    
    for( i = 0; i < 25; i++ )
	anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ] = 0;

    anBoard[ 0 ][ 5 ] = anBoard[ 1 ][ 5 ] =
	anBoard[ 0 ][ 12 ] = anBoard[ 1 ][ 12 ] = fNackgammon ? 4 : 5;
    anBoard[ 0 ][ 7 ] = anBoard[ 1 ][ 7 ] = 3;
    anBoard[ 0 ][ 23 ] = anBoard[ 1 ][ 23 ] = 2;

    if( fNackgammon )
	anBoard[ 0 ][ 22 ] = anBoard[ 1 ][ 22 ] = 2;
}

#if USE_GTK && HAVE_GDK_GDKX_H
static unsigned long nLastRequest;
static guint nUpdate;

static gint UpdateBoard( gpointer p ) {

    /* we've waited long enough -- force this update */
    nLastRequest = LastKnownRequestProcessed( GDK_DISPLAY() );
    
    ShowBoard();

    nUpdate = 0;

    return FALSE; /* remove idle handler */
}
#endif

extern int GetMatchStateCubeInfo( cubeinfo *pci, matchstate *pms ) {

    return SetCubeInfo( pci, pms->nCube, pms->fCubeOwner, pms->fMove,
			pms->nMatchTo, pms->anScore, pms->fCrawford,
			fJacoby, nBeavers );
}

static void DisplayCubeAnalysis( float arDouble[ 4 ], evalsetup *pes ) {

    cubeinfo ci;
    char sz[ 1024 ];

    if( pes->et == EVAL_NONE )
	return;

    GetMatchStateCubeInfo( &ci, &ms );
    
    if( !GetDPEq( NULL, NULL, &ci ) )
	/* No cube action possible */
	return;
    
    GetCubeActionSz( arDouble, sz, &ci, fOutputMWC, FALSE );

    outputl( sz );
}

static char *GetLuckAnalysis( matchstate *pms, float rLuck ) {

    static char sz[ 16 ];
    cubeinfo ci;
    
    if( fOutputMWC && pms->nMatchTo ) {
	GetMatchStateCubeInfo( &ci, pms );
	
	sprintf( sz, "%+0.3f%%", 100.0f * ( eq2mwc( rLuck, &ci ) -
					    eq2mwc( 0.0f, &ci ) ) );
    } else
	sprintf( sz, "%+0.3f", rLuck );

    return sz;
}

static void DisplayAnalysis( moverecord *pmr ) {

    int i;
    char szBuf[ 1024 ];
    
    switch( pmr->mt ) {
    case MOVE_NORMAL:
	DisplayCubeAnalysis( pmr->n.arDouble, &pmr->n.esDouble );

	outputf( "Rolled %d%d", pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );

	if( pmr->n.rLuck != ERR_VAL )
	    outputf( " (%s):\n", GetLuckAnalysis( &ms, pmr->n.rLuck ) );
	else
	    outputl( ":" );
	
	for( i = 0; i < pmr->n.ml.cMoves; i++ ) {
	    if( i >= 10 /* FIXME allow user to choose limit */ &&
		i != pmr->n.iMove )
		continue;
	    outputc( i == pmr->n.iMove ? '*' : ' ' );
	    output( FormatMoveHint( szBuf, &ms, &pmr->n.ml, i,
				    i != pmr->n.iMove ||
				    i != pmr->n.ml.cMoves - 1 ) );
	}
	
	break;

    case MOVE_DOUBLE:
	DisplayCubeAnalysis( pmr->d.arDouble, &pmr->n.esDouble );
	break;

    case MOVE_TAKE:
    case MOVE_DROP:
	/* FIXME */
	break;
	
    case MOVE_SETDICE:
	if( pmr->n.rLuck != ERR_VAL )
	    outputf( "Rolled %d%d (%s):\n", pmr->sd.anDice[ 0 ],
		     pmr->sd.anDice[ 1 ], GetLuckAnalysis( &ms,
							   pmr->sd.rLuck ) );
	break;
	
    default:
	break;
    }
}

extern void ShowBoard( void ) {

    char szBoard[ 2048 ];
    char sz[ 32 ], szCube[ 32 ], szPlayer0[ 35 ], szPlayer1[ 35 ],
	szScore0[ 35 ], szScore1[ 35 ], szMatch[ 35 ];
    char *apch[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
#if USE_GUI
    int anBoardTemp[ 2 ][ 25 ];
#endif
    moverecord *pmr;
    
    if( cOutputDisabled )
	return;

#if USE_GTK
    /* FIXME it's ugly to access ...->animate_computer_moves here, but
       postponing updates can lead to animating based on the wrong
       display */
    if( fX && !nDelay && !( (BoardData *) BOARD( pwBoard )->board_data )->
	animate_computer_moves ) {
	/* Always let the board widget know about dice rolls, even if the
	   board update is elided (see below). */
	if( ms.anDice[ 0 ] )
	    game_set_old_dice( BOARD( pwBoard ), ms.anDice[ 0 ],
			       ms.anDice[ 1 ] );
	
	/* Is the server still processing our last request?  If so, don't
	   give it more until it's finished what it has.  (Always update
	   the board immediately if nDelay is set, though -- show the user
	   something while they're waiting!) */
#if HAVE_GDK_GDKX_H
	XEventsQueued( GDK_DISPLAY(), QueuedAfterReading );

	/* Subtract and compare as signed, just in case the request numbers
	   wrap around. */
	if( (long) ( LastKnownRequestProcessed( GDK_DISPLAY() ) -
		     nLastRequest ) < 0 ) {
	    if( !nUpdate )
		nUpdate = gtk_idle_add( UpdateBoard, NULL );

	    return;
	}
#endif
    }
#endif
    
    if( ms.gs == GAME_NONE ) {
#if USE_GUI
	if( fX ) {
	    InitBoard( anBoardTemp );
#if USE_GTK
	    game_set( BOARD( pwBoard ), anBoardTemp, 0, ap[ 1 ].szName,
		      ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		      ms.anScore[ 0 ], -1, -1, FALSE );
#if HAVE_GDK_GDKX_H
	    nLastRequest = NextRequest( GDK_DISPLAY() ) - 1;
#endif
#else
            GameSet( &ewnd, anBoardTemp, 0, ap[ 1 ].szName, ap[ 0 ].szName,
                     ms.nMatchTo, ms.anScore[ 1 ], ms.anScore[ 0 ], -1, -1 );
#endif
	} else
#endif
	    outputl( "No game in progress." );
	
	return;
    }
    
#if USE_GUI
    if( !fX ) {
#endif
	if( fOutputRawboard ) {
	    if( !ms.fMove )
		SwapSides( ms.anBoard );
	    
	    outputl( FIBSBoard( szBoard, ms.anBoard, ms.fMove, ap[ 1 ].szName,
				ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
				ms.anScore[ 0 ], ms.anDice[ 0 ],
				ms.anDice[ 1 ], ms.nCube,
				ms.fCubeOwner, ms.fDoubled, ms.fTurn,
				ms.fCrawford ) );
	    if( !ms.fMove )
		SwapSides( ms.anBoard );
	    
	    return;
	}
	
	apch[ 0 ] = szPlayer0;
	apch[ 6 ] = szPlayer1;
	sprintf( apch[ 1 ] = szScore0, "%d point%c", ms.anScore[ 0 ],
		 ms.anScore[ 0 ] != 1 ? 's' : 0 );
	sprintf( apch[ 5 ] = szScore1, "%d point%c", ms.anScore[ 1 ],
		 ms.anScore[ 1 ] != 1 ? 's' : 0 );

	if( ms.fDoubled ) {
	    apch[ ms.fTurn ? 4 : 2 ] = szCube;

	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );
	    sprintf( szCube, "Cube offered at %d", ms.nCube << 1 );
	} else {
	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

	    apch[ ms.fMove ? 4 : 2 ] = sz;
	
	    if( ms.anDice[ 0 ] )
		sprintf( sz, "Rolled %d%d", ms.anDice[ 0 ], ms.anDice[ 1 ] );
	    else if( !GameStatus( ms.anBoard ) )
		strcpy( sz, "On roll" );
	    else
		sz[ 0 ] = 0;
	    
	    if( ms.fCubeOwner < 0 ) {
		apch[ 3 ] = szCube;

		if( ms.nMatchTo )
		    sprintf( szCube, "%d point match (Cube: %d)", ms.nMatchTo,
			     ms.nCube );
		else
		    sprintf( szCube, "(Cube: %d)", ms.nCube );
	    } else {
		int cch = strlen( ap[ ms.fCubeOwner ].szName );
		
		if( cch > 20 )
		    cch = 20;
		
		sprintf( szCube, "%c: %*s (Cube: %d)", ms.fCubeOwner ? 'X' :
			 'O', cch, ap[ ms.fCubeOwner ].szName, ms.nCube );

		apch[ ms.fCubeOwner ? 6 : 0 ] = szCube;

		if( ms.nMatchTo )
		    sprintf( apch[ 3 ] = szMatch, "%d point match",
			     ms.nMatchTo );
	    }
	}
    
	if( ms.fResigned )
	    sprintf( strchr( sz, 0 ), ", resigns %s",
		     aszGameResult[ ms.fResigned - 1 ] );
	
	if( !ms.fMove )
	    SwapSides( ms.anBoard );
	
	outputl( DrawBoard( szBoard, ms.anBoard, ms.fMove, apch ) );

	if( fAnnotation && plLastMove && ( pmr = plLastMove->plNext->p ) ) {
	    DisplayAnalysis( pmr );
	    if( pmr->a.sz )
		outputl( pmr->a.sz ); /* FIXME word wrap */
	}
	
	if( !ms.fMove )
	    SwapSides( ms.anBoard );
#if USE_GUI
    } else {
	if( !ms.fMove )
	    SwapSides( ms.anBoard );

#if USE_GTK
	game_set( BOARD( pwBoard ), ms.anBoard, ms.fMove, ap[ 1 ].szName,
		  ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		  ms.anScore[ 0 ], ms.anDice[ 0 ], ms.anDice[ 1 ],
		  ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputing &&
		  !nNextTurn );
#if HAVE_GDK_GDKX_H
	nLastRequest = NextRequest( GDK_DISPLAY() ) - 1;
#endif
#else
        GameSet( &ewnd, ms.anBoard, ms.fMove, ap[ 1 ].szName, ap[ 0 ].szName,
                 ms.nMatchTo, ms.anScore[ 1 ], ms.anScore[ 0 ], ms.anDice[ 0 ],
                 ms.anDice[ 1 ] );	
#endif
	if( !ms.fMove )
	    SwapSides( ms.anBoard );
#if USE_GTK
#if HAVE_GDK_GDKX_H	
	XFlush( GDK_DISPLAY() );
#endif
#else
	XFlush( ewnd.pdsp );
#endif
    }    
#endif    

#ifdef UNDEF
    {
      char *pc;

      printf ( "MatchID: %s\n", pc = MatchIDFromMatchState ( &ms ) );

      MatchStateFromID ( &ms, pc );

    }
#endif

}

extern char *FormatPrompt( void ) {

    static char sz[ 128 ]; /* FIXME check for overflow in rest of function */
    char *pch = szPrompt, *pchDest = sz;
    int anPips[ 2 ];

    while( *pch )
	if( *pch == '\\' ) {
	    pch++;
	    switch( *pch ) {
	    case 0:
		goto done;
		
	    case 'c':
	    case 'C':
		/* Pip count */
		if( ms.gs == GAME_NONE )
		    strcpy( pchDest, "No game" );
		else {
		    PipCount( ms.anBoard, anPips );
		    sprintf( pchDest, "%d:%d", anPips[ 1 ], anPips[ 0 ] );
		}
		break;

	    case 'p':
	    case 'P':
		/* Player on roll */
		switch( ms.gs ) {
		case GAME_NONE:
		    strcpy( pchDest, "No game" );
		    break;

		case GAME_PLAYING:
		    strcpy( pchDest, ap[ ms.fTurn ].szName );
		    break;

		case GAME_OVER:
		case GAME_RESIGNED:
		case GAME_DROP:
		    strcpy( pchDest, "Game over" );
		    break;
		}
		break;
		
	    case 's':
	    case 'S':
		/* Match score */
		sprintf( pchDest, "%d:%d", ms.anScore[ 0 ], ms.anScore[ 1 ] );
		break;

	    case 'v':
	    case 'V':
		/* Version */
		strcpy( pchDest, VERSION );
		break;
		
	    default:
		*pchDest++ = *pch;
		*pchDest = 0;
	    }

	    pchDest = strchr( pchDest, 0 );
	    pch++;
	} else
	    *pchDest++ = *pch++;
    
 done:
    *pchDest = 0;

    return sz;
}

extern void CommandEval( char *sz ) {

    char szOutput[ 2048 ];
    int n, an[ 2 ][ 25 ];
    cubeinfo ci;
    
    if( !*sz && ms.gs == GAME_NONE ) {
	outputl( "No position specified and no game in progress." );
	return;
    }

    if( ( n = ParsePosition( an, &sz, NULL ) ) < 0 )
	return;

    if( n && ms.fMove )
	/* =n notation used; the opponent is on roll in the position given. */
	SwapSides( an );

    if( ms.gs == GAME_NONE )
	memcpy( &ci, &ciCubeless, sizeof( ci ) );
    else
	SetCubeInfo( &ci, ms.nCube, ms.fCubeOwner, n ? !ms.fMove : ms.fMove,
		     ms.nMatchTo, ms.anScore, ms.fCrawford, fJacoby,
		     nBeavers );    

    ProgressStart( "Evaluating position..." );
    if( !DumpPosition( an, szOutput, &esEvalCube.ec, &ci,
                       fOutputMWC, fOutputWinPC, n ) ) {
	ProgressEnd();
#if USE_GTK
	if( fX )
	    GTKEval( szOutput );
	else
#endif
	    outputl( szOutput );
    }
    ProgressEnd();
}

static command *FindHelpCommand( command *pcBase, char *sz,
				 char *pchCommand, char *pchUsage ) {

    command *pc;
    char *pch;
    int cch;
    
    if( !( pch = NextToken( &sz ) ) )
	return pcBase;

    cch = strlen( pch );

    for( pc = pcBase->pc; pc && pc->sz; pc++ )
	if( !strncasecmp( pch, pc->sz, cch ) )
	    break;

    if( !pc || !pc->sz )
	return NULL;

    pch = pc->sz;
    while( *pch )
	*pchCommand++ = *pchUsage++ = *pch++;
    *pchCommand++ = ' '; *pchCommand = 0;
    *pchUsage++ = ' '; *pchUsage = 0;

    if( pc->szUsage ) {
	pch = pc->szUsage;
	while( *pch )
	    *pchUsage++ = *pch++;
	*pchUsage++ = ' '; *pchUsage = 0;	
    }
    
    if( pc->pc )
	/* subcommand */
	return FindHelpCommand( pc, sz, pchCommand, pchUsage );
    else
	/* terminal command */
	return pc;
}

extern void CommandHelp( char *sz ) {

    command *pc, *pcFull;
    char szCommand[ 128 ], szUsage[ 128 ], *szHelp;
    
#if USE_GTK 
# if GTK_CHECK_VERSION(2,0,0)
    if( fX )
	return GTKHelp( sz );
# endif
#endif
    
    if( !( pc = FindHelpCommand( &cTop, sz, szCommand, szUsage ) ) ) {
	outputf( "No help available for topic `%s' -- try `help' for a list "
		 "of topics.\n", sz );

	return;
    }

    if( pc->szHelp )
	/* the command has its own help text */
	szHelp = pc->szHelp;
    else if( pc == &cTop )
	/* top-level help isn't for any command */
	szHelp = NULL;
    else {
	/* perhaps the command is an abbreviation; search for the full
	   version */
	szHelp = NULL;

	for( pcFull = acTop; pcFull->sz; pcFull++ )
	    if( pcFull->pf == pc->pf && pcFull->szHelp ) {
		szHelp = pcFull->szHelp;
		break;
	    }
    }

    if( szHelp ) {
	outputf( "%s- %s\n\nUsage: %s", szCommand, szHelp, szUsage );

	if( pc->pc && pc->pc->sz )
	    outputl( "<subcommand>\n" );
	else
	    outputc( '\n' );
    }

    if( pc->pc && pc->pc->sz ) {
	outputl( pc == &cTop ? "Available commands:" :
		 "Available subcommands:" );

	pc = pc->pc;
	
	for( ; pc->sz; pc++ )
	    if( pc->szHelp )
		outputf( "%-15s\t%s\n", pc->sz, pc->szHelp );
    }
}

extern char *FormatMoveHint( char *sz, matchstate *pms, movelist *pml,
			     int i, int fRankKnown ) {
    
    cubeinfo ci;
    char szTemp[ 1024 ], szMove[ 32 ];

    GetMatchStateCubeInfo( &ci, pms );
    
    if ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) ) {
	/* output in equity */
        float *ar, rEq, rEqTop;

	ar = pml->amMoves[ 0 ].arEvalMove;
	rEqTop = pml->amMoves[ 0 ].rScore;

	if( !i ) {
	    if( fOutputWinPC )
		sprintf( sz, " %4i. %-14s   %-28s Eq.: %+6.3f\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n",
			 1, FormatEval ( szTemp, &pml->amMoves[ 0 ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ 0 ].anMove ),
			 rEqTop, 
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz, " %4i. %-14s   %-28s Eq.: %+6.3f\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n",
			 1, FormatEval ( szTemp, &pml->amMoves[ 0 ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ 0 ].anMove ),
			 rEqTop, 
			 ar[ 0 ], ar[ 1 ], ar[ 2 ],
			 ( 1.0 - ar[ 0 ] ) , ar[ 3 ], 
			 ar[ 4 ] );
	} else {
	    ar = pml->amMoves[ i ].arEvalMove;
	    rEq = pml->amMoves[ i ].rScore;

	    if( fRankKnown )
		sprintf( sz, " %4i.", i + 1 );
	    else
		strcpy( sz, "   ?? " );
	    
	    if( fOutputWinPC )
		sprintf( sz + 6, " %-14s   %-28s Eq.: %+6.3f (%+6.3f)\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n",
			 FormatEval ( szTemp, &pml->amMoves[ i ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ i ].anMove ),
			 rEq, rEq - rEqTop,
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz + 6, " %-14s   %-28s Eq.: %+6.3f (%+6.3f)\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n",
			 FormatEval ( szTemp, &pml->amMoves[ i ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ i ].anMove ),
			 rEq, rEq - rEqTop,
			 ar[ 0 ], ar[ 1 ], ar[ 2 ],
			 ( 1.0 - ar[ 0 ] ) , ar[ 3 ], 
			 ar[ 4 ] );
	}
    } else {
 	/* output in mwc */

        float *ar, rMWC, rMWCTop;
	
	ar = pml->amMoves[ 0 ].arEvalMove;
	rMWCTop = 100.0 * eq2mwc ( pml->amMoves[ 0 ].rScore, &ci );

	if( !i ) {
	    if( fOutputWinPC )
		sprintf( sz, " %4i. %-14s   %-28s Mwc: %7.3f%%\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n",
			 1, FormatEval ( szTemp, &pml->amMoves[ 0 ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ 0 ].anMove ),
			 rMWCTop, 
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz, " %4i. %-14s   %-28s Mwc: %7.3f%%\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n",
			 1, FormatEval ( szTemp, &pml->amMoves[ 0 ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ 0 ].anMove ),
			 rMWCTop, 
			 ar[ 0 ], ar[ 1 ], ar[ 2 ],
			 ( 1.0 - ar[ 0 ] ) , ar[ 3 ], 
			 ar[ 4 ] );
        } else {
	    ar = pml->amMoves[ i ].arEvalMove;
	    rMWC = 100.0 * eq2mwc ( pml->amMoves[ i ].rScore, &ci );

	    if( fRankKnown )
		sprintf( sz, " %4i.", i + 1 );
	    else
		strcpy( sz, "   ?? " );
	    
	    if( fOutputWinPC )
		sprintf( sz + 6, " %-14s   %-28s Mwc: %7.3f%% (%+7.3f%%)\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n",
			 FormatEval ( szTemp, &pml->amMoves[ i ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ i ].anMove ),
			 rMWC, rMWC - rMWCTop,
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz + 6, " %-14s   %-28s Mwc: %7.3f%% (%+7.3f%%)\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n",
			 FormatEval ( szTemp, &pml->amMoves[ i ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ i ].anMove ),
			 rMWC, rMWC - rMWCTop,
			 ar[ 0 ], ar[ 1 ], ar[ 2 ],
			 ( 1.0 - ar[ 0 ] ) , ar[ 3 ], 
			 ar[ 4 ] );
	}
    }

    return sz;
}

extern void CommandHint( char *sz ) {

    movelist ml;
    int i;
    char szBuf[ 1024 ];
    float arDouble[ 4 ], aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    cubeinfo ci;
    int n = ParseNumber ( &sz );
    
    if( ms.gs != GAME_PLAYING ) {
      outputl( "You must set up a board first." );
      
      return;
    }

    if( !ms.anDice[ 0 ] && !ms.fDoubled && ! ms.fResigned ) {
	GetMatchStateCubeInfo( &ci, &ms );

	if ( GetDPEq ( NULL, NULL, &ci ) ) {
	    /* Give hint on cube action */

	    ProgressStart( "Considering cube action..." );
	    if ( GeneralCubeDecisionE ( aarOutput, ms.anBoard, &ci, 
					&esEvalCube.ec ) < 0 ) {
		ProgressEnd();
		return;
	    }
	    ProgressEnd();
	    
	    FindCubeDecision ( arDouble, aarOutput, &ci );  

	    GetCubeActionSz ( arDouble, szBuf, &ci, fOutputMWC, FALSE );

#if USE_GTK
	    if ( fX ) {
		GTKDoubleHint( szBuf );
		return;
	    }
#endif
	    outputl ( szBuf );
	    
	    return;
	    
	} else {
	    
	    outputl( "You cannot double." );
	    
	    return;
	    
	}
	
    }

    /* Give hints on resignation */

    if ( ms.fResigned ) {

      float rEqBefore, rEqAfter;

      GetMatchStateCubeInfo( &ci, &ms );

      /* evaluate current position */

      ProgressStart( "Considering resignation..." );
      if ( GeneralEvaluationE ( aarOutput[ 0 ],
                                ms.anBoard,
                                &ci, &esEvalCube.ec ) < 0 ) {
	  ProgressEnd();
	  return;
      }
      ProgressEnd();
      
      getResignEquities ( aarOutput[ 0 ], &ci, ms.fResigned, 
                          &rEqBefore, &rEqAfter );

#if USE_GTK
      if ( fX ) {
        
        GTKResignHint ( aarOutput[ 0 ], rEqBefore, rEqAfter, &ci, 
                        ms.nMatchTo && fOutputMWC );

        return;

      }
#endif

      if ( ! ms.nMatchTo || ( ms.nMatchTo && ! fOutputMWC ) ) {

        outputf ( "Equity before resignation: %+6.3f\n",
                  - rEqBefore );
        outputf ( "Equity after resignation : %+6.3f (%+6.3f)\n\n",
                  - rEqAfter, rEqBefore - rEqAfter );
        outputf ( "Correct resign decision  : %s\n\n",
                  ( rEqBefore - rEqAfter >= 0 ) ?
                  "Accept" : "Reject" );

      }
      else {
        
        rEqBefore = eq2mwc ( - rEqBefore, &ci );
        rEqAfter  = eq2mwc ( - rEqAfter, &ci );

        outputf ( "Equity before resignation: %6.2f%%\n",
                  rEqBefore * 100.0f );
        outputf ( "Equity after resignation : %6.2f%% (%6.2f%%)\n\n",
                  rEqAfter * 100.0f,
                  100.0f * ( rEqAfter - rEqBefore ) );
        outputf ( "Correct resign decision  : %s\n\n",
                  ( rEqAfter - rEqBefore >= 0 ) ?
                  "Accept" : "Reject" );

      }

      return;


    }

    if ( ms.fDoubled ) {
	/* Give hint on take decision */
	GetMatchStateCubeInfo( &ci, &ms );

	ProgressStart( "Considering cube action..." );
	if ( GeneralCubeDecisionE ( aarOutput, ms.anBoard, &ci, 
				    &esEvalCube.ec ) < 0 ) {
	    ProgressEnd();
	    return;
	}
	ProgressEnd();

        FindCubeDecision ( arDouble, aarOutput, &ci );
	
#if USE_GTK
	if ( fX ) {
	    GTKTakeHint( arDouble, ms.nMatchTo && fOutputMWC,
			 !ms.nMatchTo && ms.cBeavers < nBeavers, &ci );
	    return;
	}
#endif
	
	outputl ( "Take decision:\n" );
	
	if ( ! ms.nMatchTo || ( ms.nMatchTo && ! fOutputMWC ) ) {
	    
	    outputf ( "Equity for take: %+6.3f\n", -arDouble[ 2 ] );
	    outputf ( "Equity for pass: %+6.3f\n\n", -arDouble[ 3 ] );
	    
	}
	else {
	    outputf ( "Mwc for take: %6.2f%%\n", 
		      100.0 * ( 1.0 - eq2mwc ( arDouble[ 2 ], &ci ) ) );
	    outputf ( "Mwc for pass: %6.2f%%\n", 
		      100.0 * ( 1.0 - eq2mwc ( arDouble[ 3 ], &ci ) ) );
	}
	
	if ( arDouble[ 2 ] < 0 && !ms.nMatchTo && ms.cBeavers < nBeavers )
	    outputl ( "Your proper cube action: Beaver!\n" );
	else if ( arDouble[ 2 ] <= arDouble[ 3 ] )
	    outputl ( "Your proper cube action: Take.\n" );
	else
	    outputl ( "Your proper cube action: Pass.\n" );
	
	return;
	
    }
    
    if ( ms.anDice[ 0 ] ) {
	
	/* Give hints on move */
	
	if ( n <= 0 )
	    n = 10;

	GetMatchStateCubeInfo( &ci, &ms );

	ProgressStart( "Considering moves..." );
	if( FindnSaveBestMoves( &ml, ms.anDice[ 0 ], ms.anDice[ 1 ],
				ms.anBoard, NULL, &ci,
				&esEvalChequer.ec ) < 0 || fInterrupt ) {
	    ProgressEnd();
	    return;
	}
	ProgressEnd();
	
	n = ( ml.cMoves > n ) ? n : ml.cMoves;

	if( !ml.cMoves ) {
	    outputl( "There are no legal moves." );
	    return;
	}
	
	if( sm.ml.amMoves )
	    free( sm.ml.amMoves );
	
	memcpy( &sm.ml, &ml, sizeof( ml ) );
	PositionKey( ms.anBoard, sm.auchKey );
	sm.anDice[ 0 ] = ms.anDice[ 0 ];
	sm.anDice[ 1 ] = ms.anDice[ 1 ];
      
#if USE_GTK
	if( fX ) {
	    GTKHint( &ml );
	    return;
	}
#endif
	
	for( i = 0; i < n; i++ )
	    output( FormatMoveHint( szBuf, &ms, &ml, i, TRUE ) );
    }
}

/* Called on various exit commands -- e.g. EOF on stdin, "quit" command,
   etc.  If stdin is not a TTY, this should always exit immediately (to
   avoid enless loops on EOF).  If stdin is a TTY, and fConfirm is set,
   and a game is in progress, then we ask the user if they're sure. */
extern void PromptForExit( void ) {

    static int fExiting;

    if( !fExiting && fInteractive && fConfirm && ms.gs == GAME_PLAYING ) {
	fExiting = TRUE;
	fInterrupt = FALSE;
	
	if( !GetInputYN( "Are you sure you want to exit and abort the game in "
			 "progress? " ) ) {
	    fInterrupt = FALSE;
	    fExiting = FALSE;
	    return;
	}
    }

    exit( EXIT_SUCCESS );
}

extern void CommandNotImplemented( char *sz ) {

    outputl( "That command is not yet implemented." );
}

extern void CommandQuit( char *sz ) {

    PromptForExit();
}


extern char *FormatCubePosition ( char *sz, cubeinfo *pci ) {

  if ( pci->fCubeOwner == -1 )
    sprintf ( sz, "Centered %d-cube", pci->nCube );
  else 
    sprintf ( sz, "Player %s owns %d-cube",
              ap[ pci->fCubeOwner ].szName, pci->nCube );

  return sz;

}


extern void 
CommandRollout( char *sz ) {
    
    float ar[ NUM_ROLLOUT_OUTPUTS ], arStdDev[ NUM_ROLLOUT_OUTPUTS ];
    int i, c, n, fOpponent = FALSE, cGames, fCubeDecTop = TRUE;
    cubeinfo ci;
#if HAVE_ALLOCA
    int ( *aan )[ 2 ][ 25 ];
    char ( *asz )[ 40 ];
    rolloutstat ( *aars)[ 2 ];
#else
    int aan[ 10 ][ 2 ][ 25 ];
    char asz[ 10 ][ 40 ];
    rolloutstat aars[ 10 ][ 2 ];
#endif

    if( !( c = CountTokens( sz ) ) ) {
	if( ms.gs != GAME_PLAYING ) {
	    outputl( "No position specified and no game in progress." );
	    return;
	} else
	    c = 1; /* current position */
    }

    /* check for `rollout =cube' */    
    if ( c == 1 && ! strncmp ( sz, "=cube", 5 ) ) {
      float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
      float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
      rolloutstat aarsStatistics[ 2 ][ 2 ];

      if( ms.gs != GAME_PLAYING ) {
	  outputl( "No game in progress." );
	  return;
      }
      
      GetMatchStateCubeInfo( &ci, &ms );

      GeneralCubeDecisionR ( "", aarOutput, aarStdDev, aarsStatistics,
                             ms.anBoard, &ci, &rcRollout );
      return;

    }


#if HAVE_ALLOCA
    aan = alloca( 50 * c * sizeof( int ) );
    asz = alloca( 40 * c );
    aars = alloca( 2 * c * sizeof ( rolloutstat ) );
#else
    if( c > 10 )
	c = 10;
#endif
    
    for( i = 0; i < c; i++ ) {
	if( ( n = ParsePosition( aan[ i ], &sz, asz[ i ] ) ) < 0 )
	    return;
	else if( n ) {
	    if( ms.fMove )
		SwapSides( aan[ i ] );
	    
	    fOpponent = TRUE;
	}

	if( !sz ) {
	    /* that was the last parameter */
	    c = i + 1;
	    break;
	}
    }

    /*
    if( fOpponent ) {
	an[ 0 ] = ms.anScore[ 1 ];
	an[ 1 ] = ms.anScore[ 0 ];
    } else {
	an[ 0 ] = ms.anScore[ 0 ];
	an[ 1 ] = ms.anScore[ 1 ];
    }
    */
    
    SetCubeInfo ( &ci, ms.nCube, ms.fCubeOwner, fOpponent ? !ms.fMove :
		  ms.fMove, ms.nMatchTo, ms.anScore, ms.fCrawford, fJacoby,
                  nBeavers );

#if USE_GTK
    if( fX )
	GTKRollout( c, asz, rcRollout.nTrials, aars );
    else
#endif
	outputl( "                               Win  W(g) W(bg)  L(g) L(bg) "
		 "Equity   Cube E    n" );
	
    for( i = 0; i < c; i++ ) {
#if USE_GTK
	if( fX )
	    GTKRolloutRow( i );
#endif
	if( ( cGames = RolloutGeneral( aan[ i ], &asz[ i ], &ar, &arStdDev,
                                       &aars[ i ], &rcRollout, &ci, &fCubeDecTop,
                                       1, fOpponent ) ) <= 0 )
	    return;

#if USE_GTK
	if( !fX )
#endif
	    {
		if( rcRollout.fCubeful )
		    outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) "
			     "%6.3f %4d\n"
			     "              Standard error %5.3f %5.3f %5.3f "
			     "%5.3f %5.3f (%6.3f) %6.3f\n\n",
			     asz[ i ], ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ],
			     ar[ 4 ], ar[ 5 ],ar[ 6 ], cGames, arStdDev[ 0 ],
			     arStdDev[ 1 ], arStdDev[ 2 ], arStdDev[ 3 ],
			     arStdDev[ 4 ], arStdDev[ 5 ], arStdDev[ 6 ] );
		else
		    outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f)    "
			     "n/a %4d\n"
			     "              Standard error %5.3f %5.3f %5.3f "
			     "%5.3f %5.3f (%6.3f)    n/a\n\n",
			     asz[ i ], ar[ 0 ], ar[ 1 ], ar[ 2 ], ar[ 3 ],
			     ar[ 4 ], ar[ 5 ], cGames, arStdDev[ 0 ],
			     arStdDev[ 1 ], arStdDev[ 2 ], arStdDev[ 3 ],
			     arStdDev[ 4 ], arStdDev[ 5 ] );
	    }
    }
    
#if USE_GTK
    if( fX )
	GTKRolloutDone();
#endif	
}

static void ExportGameJF( FILE *pf, list *plGame, int iGame,
			  int anScore[ 2 ] ) {
    list *pl;
    moverecord *pmr;
    char sz[ 128 ];
    int i = 0, n, nFileCube = 1, anBoard[ 2 ][ 25 ], fWarned = FALSE;

    /* FIXME It would be nice if this function was updated to use the
       new matchstate struct and ApplyMoveRecord()... but otherwise
       it's not broken, so I won't fix it. */
    
    if( iGame >= 0 )
	fprintf( pf, " Game %d\n", iGame + 1 );

    if( anScore ) {
	sprintf( sz, "%s : %d", ap[ 0 ].szName, anScore[ 0 ] );
	fprintf( pf, " %-31s%s : %d\n", sz, ap[ 1 ].szName, anScore[ 1 ] );
    } else
	fprintf( pf, " %-31s%s\n", ap[ 0 ].szName, ap[ 1 ].szName );

    
    InitBoard( anBoard );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
	pmr = pl->p;
	switch( pmr->mt ) {
	case MOVE_GAMEINFO:
	    /* no-op */
	    /* FIXME what about automatic doubles? */
          continue;
	    break;
	case MOVE_NORMAL:
	    sprintf( sz, "%d%d: ", pmr->n.anRoll[ 0 ], pmr->n.anRoll[ 1 ] );
	    FormatMovePlain( sz + 4, anBoard, pmr->n.anMove );
	    ApplyMove( anBoard, pmr->n.anMove, FALSE );
	    SwapSides( anBoard );
         /*   if (( sz[ strlen(sz)-1 ] == ' ') && (strlen(sz) > 5 ))
              sz[ strlen(sz) - 1 ] = 0;  Don't need this..  */
	    break;
	case MOVE_DOUBLE:
	    sprintf( sz, " Doubles => %d", nFileCube <<= 1 );
	    break;
	case MOVE_TAKE:
	    strcpy( sz, " Takes" ); /* FIXME beavers? */
	    break;
	case MOVE_DROP:
            sprintf( sz, " Drops%sWins %d point%s",
                   (i & 1) ? "\n      " : "                       ",
                   nFileCube / 2, (nFileCube == 2) ? "" :"s" );
	    if( anScore )
		anScore[ ( i + 1 ) & 1 ] += nFileCube / 2;
	    break;
        case MOVE_RESIGN:
            if (pmr->r.fPlayer)
              sprintf( sz, "%s      Wins %d point%s\n", (i & 1) ? "\n" : "",
                       pmr->r.nResigned * nFileCube,
                       ((pmr->r.nResigned * nFileCube ) > 1) ? "s" : "");
            else
              sprintf( sz, "%sWins %d point%s\n", (i & 1) ? " " :
                        "                                  ",
                        pmr->r.nResigned * nFileCube,
                        ((pmr->r.nResigned * nFileCube ) > 1) ? "s" : "");
            if( anScore )
                anScore[ !pmr->r.fPlayer ] += pmr->r.nResigned * nFileCube;
            break;
	case MOVE_SETDICE:
	    /* ignore */
	    break;
	case MOVE_SETBOARD:
	case MOVE_SETCUBEVAL:
	case MOVE_SETCUBEPOS:
	    if( !fWarned ) {
		fWarned = TRUE;
		outputl( "Warning: this game was edited during play, and "
			 "cannot be recorded in this format." );
	    }
	    break;
	}

	if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) {
	    fputs( "  1)                             ", pf );
	    i++;
	}

	if(( i & 1 ) || (pmr->mt == MOVE_RESIGN)) {
	    fputs( sz, pf );
	    fputc( '\n', pf );
	} else 
	    fprintf( pf, "%3d) %-27s ", ( i >> 1 ) + 1, sz );

        if ( pmr->mt == MOVE_DROP ) {
          fputc( '\n', pf );
          if ( ! ( i & 1 ) )
            fputc( '\n', pf );
        }

	if( ( n = GameStatus( anBoard ) ) ) {
	    fprintf( pf, "%sWins %d point%s%s\n\n",
		   i & 1 ? "                                  " : "\n      ",
		   n * nFileCube, n * nFileCube > 1 ? "s" : "",
		   "" /* FIXME " and the match" if appropriate */ );

	    if( anScore )
		anScore[ i & 1 ] += n * nFileCube;
	}
	
	i++;
    }
}

#if USE_GUILE
static SCM LoadCommandsGuileCatch( void *p, SCM sTag, SCM sArgs ) {

    if( SCM_NFALSEP( scm_eq_p( sTag, SCM_CAR( scm_intern0( "quit" ) ) ) ) ||
	SCM_NFALSEP( scm_eq_p( sTag, SCM_CAR( scm_intern0(
	    "end-of-file" ) ) ) ) )
	return SCM_BOOL_T;
    else
	return scm_handle_by_message_noexit( p, sTag, sArgs ); /* SCM_BOOL_F */
}

static void LoadCommandsGuile( SCM s ) {

    for(;;)
	scm_read_and_eval_x( s );
}
#endif

static void LoadCommands( FILE *pf, char *szFile ) {
    
    char sz[ 2048 ], *pch;

#if USE_GUILE
    /* We have to be conservative with input buffering, because if there
       is a Guile escape in the file, we will want Guile to take over
       parsing for us, and we won't want to have buffered ahead. */
    setvbuf( pf, NULL, _IONBF, 0 );
#endif
    
    for(;;) {
	sz[ 0 ] = 0;
	
	/* FIXME shouldn't restart sys calls on signals during this fgets */
	fgets( sz, sizeof( sz ), pf );

	if( ( pch = strchr( sz, '\n' ) ) )
	    *pch = 0;

	if( ferror( pf ) ) {
	    perror( szFile );
	    return;
	}
	
	if( fAction )
	    fnAction();
	
	if( feof( pf ) || fInterrupt )
	    return;

	if( *sz == '#' ) /* Comment */
	    continue;

#if USE_GUILE
	if( !strcmp( sz, ":" ) ) {
	    /* Guile escape.  If we let HandleCommand() take care of this,
	       it will start a REPL, which is not what we want; we need to
	       read and evaluate expressions from the file ourselves. */
	    SCM s;
	    psighandler sh;

#if USE_GTK
	    if( fX )
		GTKDisallowStdin();
#endif
	    PortableSignal( SIGINT, NULL, &sh, FALSE );
	    GuileStartIntHandler();

	    s = scm_fdes_to_port( fileno( pf ), "r0",
				  scm_makfrom0str( szFile ) );
	    scm_set_port_revealed_x( s, SCM_MAKINUM( 1 ) );

	    while( SCM_FALSEP( scm_internal_catch(
		SCM_BOOL_T, (scm_catch_body_t) LoadCommandsGuile,
		(void *) s, LoadCommandsGuileCatch, NULL ) ) )
		;
	    
	    GuileEndIntHandler();
	    PortableSignalRestore( SIGINT, &sh );
#if USE_GTK
	    if( fX )
		GTKAllowStdin();
#endif
	    continue;
	}
#endif
	
	HandleCommand( sz, acTop );

	/* FIXME handle NextTurn events? */
    }
}

extern void CommandLoadCommands( char *sz ) {

    FILE *pf;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to load from (see `help load "
		 "commands')." );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    } else
	perror( sz );
}

extern void CommandImportJF( char *sz ) {

    FILE *pf;

    sz = NextToken( &sz );
    
    if( ms.gs != GAME_PLAYING ) {
	outputl( "There must be a game in progress to import a Jellyfish "
                 "position." );

	return;
    }

    if( !sz || !*sz ) {
	outputl( "You must specify a position file to import (see `help "
		 "import pos')." );
	return;
    }

    if( ( pf = fopen( sz, "rb" ) ) ) {
	ImportJF( pf, sz );
	fclose( pf );
    } else
	perror( sz );

    ShowBoard();
}

extern void CommandImportMat( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a match file to import (see `help "
		 "import mat')." );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	ImportMat( pf, sz );
	fclose( pf );
    } else
	perror( sz );
}

extern void CommandImportOldmoves( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify an oldmoves file to import (see `help "
		 "import oldmoves')." );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	ImportOldmoves( pf, sz );
	fclose( pf );
    } else
	perror( sz );
}

extern void CommandImportSGG( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify an SGG file to import (see `help "
		 "import oldmoves')." );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	ImportSGG( pf, sz );
	fclose( pf );
    } else
	perror( sz );
}

extern void CommandCopy( char *sz ) {
  char *aps[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  char szOut[ 2048 ];

  /* FIXME - Rewrite for new WinCopy command */

#ifdef WIN32
  DrawBoard( szOut, ms.anBoard, 1, aps );
  strcat(szOut, "\n");

  if(OpenClipboard(0)) {

    HGLOBAL clipbuffer;
    char * buf;

    EmptyClipboard();
    clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(szOut)+1 );
    buf = (char*)GlobalLock(clipbuffer);
    strcpy(buf, szOut);
    GlobalUnlock(clipbuffer);
    SetClipboardData(CF_TEXT, clipbuffer);
    CloseClipboard();

    } else {

    outputl( "Can't open clipboard" ); 
  }
#else
  puts( DrawBoard( szOut, ms.anBoard, 1, aps ) );
#endif
}

#ifdef WIN32
extern void WinCopy( char *szOut ){

  if(OpenClipboard(0)) {

    HGLOBAL clipbuffer;
    char * buf;

    EmptyClipboard();
    clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(szOut)+1 );
    buf = (char*)GlobalLock(clipbuffer);
    strcpy(buf, szOut);
    GlobalUnlock(clipbuffer);
    SetClipboardData(CF_TEXT, clipbuffer);
    CloseClipboard();

    } else {

    outputl( "Can't open clipboard" ); 
  }
}
#endif

static void LoadRCFiles( void ) {

#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 14 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 14 );
#else
    char sz[ 4096 ];
#endif
    FILE *pf;

    outputoff();
    
    sprintf( sz, "%s/.gnubgautorc", szHomeDirectory );

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    }
    
    sprintf( sz, "%s/.gnubgrc", szHomeDirectory );

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    }

    outputon();
}

extern void CommandExportGameGam( char *sz ) {
    
    FILE *pf;

    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export"
		 "game gam')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    ExportGameJF( pf, plGame, -1, NULL );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandExportMatchMat( char *sz ) {

    FILE *pf;
    int i, anScore[ 2 ];
    list *pl;

    /* FIXME what should be done if nMatchTo == 0? */
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "match mat')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    fprintf( pf, " %d point match\n\n", ms.nMatchTo );

    anScore[ 0 ] = anScore[ 1 ] = 0;
    
    for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext )
	ExportGameJF( pf, pl->p, i, anScore );
    
    if( pf != stdout )
	fclose( pf );
}

extern void CommandNewWeights( char *sz ) {

    int n;
    
    if( sz && *sz ) {
	if( ( n = ParseNumber( &sz ) ) < 1 ) {
	    outputl( "You must specify a valid number of hidden nodes "
		     "(try `help new weights').\n" );
	    return;
	}
    } else
	n = DEFAULT_NET_SIZE;

    EvalNewWeights( n );

    outputf( "A new neural net with %d hidden nodes has been created.\n", n );
}


static void
SaveRNGSettings ( FILE *pf, char *sz, rng rngCurrent ) {

    switch( rngCurrent ) {
    case RNG_ANSI:
	fprintf( pf, "%s rng ansi\n", sz );
	break;
    case RNG_BSD:
	fprintf( pf, "%s rng bsd\n", sz );
	break;
    case RNG_ISAAC:
	fprintf( pf, "%s rng isaac\n", sz );
	break;
    case RNG_MANUAL:
	fprintf( pf, "%s rng manual\n", sz );
	break;
    case RNG_MD5:
	fprintf( pf, "%s rng md5\n", sz );
	break;
    case RNG_MERSENNE:
	fprintf( pf, "%s rng mersenne\n", sz );
	break;
    case RNG_USER:
	/* don't save user RNGs */
	break;
    }

}


static void 
SaveEvalSettings( FILE *pf, char *sz, evalcontext *pec ) {

    fprintf( pf, "%s plies %d\n"
	     "%s candidates %d\n"
	     "%s tolerance %.3f\n"
	     "%s reduced %d\n"
	     "%s cubeful %s\n"
	     "%s noise %.3f\n"
	     "%s deterministic %s\n",
	     sz, pec->nPlies, sz, pec->nSearchCandidates,
	     sz, pec->rSearchTolerance, sz, pec->nReduced,
	     sz, pec->fCubeful ? "on" : "off",
	     sz, pec->rNoise, sz, pec->fDeterministic ? "on" : "off" );
}


static void
SaveRolloutSettings ( FILE *pf, char *sz, rolloutcontext *prc ) {

  char *pch;
  int i;

  /* flags and stuff */

  fprintf ( pf,
            "%s cubeful %s\n"
            "%s varredn %s\n"
	    "%s initial %s\n"
            "%s truncation %d\n"
            "%s trials %d\n",
            sz, prc->fCubeful ? "on" : "off",
            sz, prc->fVarRedn ? "on" : "off",
	    sz, prc->fInitial ? "on" : "off",
            sz, prc->nTruncate,
            sz, prc->nTrials );

  SaveRNGSettings ( pf, sz, prc->rngRollout );

  /* chequer play and cube decision evalcontexts */

  pch = malloc ( strlen ( sz ) + 50 );

  strcpy ( pch, sz );

  for ( i = 0; i < 2; i++ ) {

    sprintf ( pch, "%s player %i chequerplay", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecChequer[ i ] );

    sprintf ( pch, "%s player %i cubedecision", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecCube[ i ] );

  }

  free ( pch );

}

static void 
SaveEvalSetupSettings( FILE *pf, char *sz, evalsetup *pes ) {

  char szTemp[ 1024 ];

  switch ( pes->et ) {
  case EVAL_EVAL:
    fprintf (pf, "%s type evaluation\n", sz );
    break;
  case EVAL_ROLLOUT:
    fprintf (pf, "%s type rollout\n", sz );
    break;
  default:
    break;
  }

  strcpy ( szTemp, sz );
  SaveEvalSettings (pf, strcat ( szTemp, " evaluation" ), &pes->ec );

  strcpy ( szTemp, sz );
  SaveRolloutSettings (pf, strcat ( szTemp, " rollout" ), &pes->rc );

}


extern void CommandSaveSettings( char *szParam ) {

#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 14 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 14 );
#else
    char sz[ 16384 ];
#endif
    char szTemp[ 1024 ];
    FILE *pf;
    int i, cCache;
    
    sprintf( sz, "%s/.gnubgautorc", szHomeDirectory ); /* FIXME accept param */

    if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    errno = 0;

    fputs( "#\n"
	   "# GNU Backgammon command file\n"
	   "#   generated by GNU Backgammon " VERSION "\n"
	   "#\n"
	   "# WARNING: The file `.gnubgautorc' is automatically generated "
	   "by the\n"
	   "# `save settings' command, and will be overwritten the next "
	   "time settings\n"
	   "# are saved.  If you want to add startup commands manually, "
	   "you should\n"
	   "# use `.gnubgrc' instead.\n"
	   "\n", pf );

    SaveEvalSetupSettings ( pf, "set analysis chequerplay",
			    &esAnalysisChequer );
    SaveEvalSetupSettings ( pf, "set analysis cubedecision",
			    &esAnalysisCube );
    
    fprintf( pf, "set analysis limit %d\n", cAnalysisMoves );

    fprintf( pf, "set analysis threshold bad %.3f\n"
	     "set analysis threshold doubtful %.3f\n"
	     "set analysis threshold good %.3f\n"
	     "set analysis threshold interesting %.3f\n"
	     "set analysis threshold lucky %.3f\n"
	     "set analysis threshold unlucky %.3f\n"
	     "set analysis threshold verybad %.3f\n"
	     "set analysis threshold verygood %.3f\n"
	     "set analysis threshold verylucky %.3f\n"
	     "set analysis threshold veryunlucky %.3f\n",
	     arSkillLevel[ SKILL_BAD ],
	     arSkillLevel[ SKILL_DOUBTFUL ],
	     arSkillLevel[ SKILL_GOOD ],
	     arSkillLevel[ SKILL_INTERESTING ],
	     arLuckLevel[ LUCK_GOOD ],
	     arLuckLevel[ LUCK_BAD ],
	     arSkillLevel[ SKILL_VERYBAD ],
	     arSkillLevel[ SKILL_VERYGOOD ],
	     arLuckLevel[ LUCK_VERYGOOD ],
	     arLuckLevel[ LUCK_VERYBAD ] );
    
#if USE_GTK
    if ( fX ) {
        fputs( BoardPreferencesCommand( pwBoard, szTemp ), pf );
        fputc( '\n', pf );
    }  /* FIXME This will overwrite the gtk-preferences if you
                save the settings in tty mode  */
#endif
    
    fprintf( pf, "set automatic bearoff %s\n"
	     "set automatic crawford %s\n"
	     "set automatic doubles %d\n"
	     "set automatic game %s\n"
	     "set automatic move %s\n"
	     "set automatic roll %s\n"
	     "set beavers %d\n",
	     fAutoBearoff ? "on" : "off",
	     fAutoCrawford ? "on" : "off",
	     cAutoDoubles,
	     fAutoGame ? "on" : "off",
	     fAutoMove ? "on" : "off",
	     fAutoRoll ? "on" : "off",
	     nBeavers );

    EvalCacheStats( NULL, &cCache, NULL, NULL );
    fprintf( pf, "set cache %d\n", cCache );

    fprintf( pf, "set clockwise %s\n"
	     "set confirm %s\n"
	     "set cube use %s\n"
#if USE_GUI
	     "set delay %d\n"
#endif
	     "set display %s\n"
	     "set egyptian %s\n",
	     fClockwise ? "on" : "off", fConfirm ? "on" : "off",
	     fCubeUse ? "on" : "off",
#if USE_GUI
	     nDelay,
#endif
	     fDisplay ? "on" : "off", fEgyptian ? "on" : "off" );

    SaveEvalSetupSettings ( pf, "set evaluation chequerplay", &esEvalChequer );
    SaveEvalSetupSettings ( pf, "set evaluation cubedecision", &esEvalCube );

    fprintf( pf, "set jacoby %s\n", fJacoby ? "on" : "off" );

    fprintf( pf, "set matchequitytable %s\n", miCurrent.szFileName );
    
    fprintf( pf, "set nackgammon %s\n", fNackgammon ? "on" : "off" );

    fprintf( pf, "set output matchpc %s\n"
	     "set output mwc %s\n"
	     "set output rawboard %s\n"
	     "set output winpc %s\n",
	     fOutputMatchPC ? "on" : "off",
	     fOutputMWC ? "on" : "off",
	     fOutputRawboard ? "on" : "off",
	     fOutputWinPC ? "on" : "off" );
    
    for( i = 0; i < 2; i++ ) {
	fprintf( pf, "set player %d name %s\n", i, ap[ i ].szName );
	
	switch( ap[ i ].pt ) {
	case PLAYER_GNU:
	    fprintf( pf, "set player %d gnubg\n", i );
	    sprintf( szTemp, "set player %d chequerplay", i );
	    SaveEvalSetupSettings( pf, szTemp, &ap[ i ].esChequer );
	    sprintf( szTemp, "set player %d cubedecision", i );
	    SaveEvalSetupSettings( pf, szTemp, &ap[ i ].esCube );
	    break;
	    
	case PLAYER_HUMAN:
	    fprintf( pf, "set player %d human\n", i );
	    break;
	    
	case PLAYER_PUBEVAL:
	    fprintf( pf, "set player %d pubeval\n", i );
	    break;
	    
	case PLAYER_EXTERNAL:
	    /* don't save external players */
	    break;
	}
    }

    fprintf( pf, "set prompt %s\n", szPrompt );

    SaveRNGSettings ( pf, "set", rngCurrent );

    SaveRolloutSettings( pf, "set rollout", &rcRollout );
    
    fprintf( pf, "set training alpha %f\n"
	     "set training anneal %f\n"
	     "set training threshold %f\n",
	     rAlpha, rAnneal, rThreshold );
    
    fclose( pf );
    
    if( errno )
	perror( sz );
    else
	outputl( "Settings saved." );

#if USE_GTK
    if( fX )
	GTKSaveSettings();
#endif
}

extern void CommandSaveWeights( char *sz ) {

    sz = NextToken( &sz );
    
    if( !sz || !*sz )
	sz = GNUBG_WEIGHTS;

    if( EvalSave( sz ) )
	perror( sz );
    else
	outputf( "Evaluator weights saved to %s.\n", sz );
}

extern void CommandTrainTD( char *sz ) {

    int c = 0, n;
    int anBoardTrain[ 2 ][ 25 ], anBoardOld[ 2 ][ 25 ];
    int anDiceTrain[ 2 ];
    float ar[ NUM_OUTPUTS ];
    
    if( sz && *sz ) {
	if( ( n = ParseNumber( &sz ) ) < 1 ) {
	    outputl( "If you specify a parameter to `train td', it\n"
		     "must be a number of positions to train on." );
	    return;
	}
    } else
	n = 0;

    ProgressStart( "Training..." );
    
    while( ( !n || c <= n ) && !fInterrupt ) {
	InitBoard( anBoardTrain );
	
	do {    
	    if( !( ++c % 100 ) )
		Progress();
	    
	    RollDice( anDiceTrain );
	    
	    if( fInterrupt )
		break;
	    
	    memcpy( anBoardOld, anBoardTrain, sizeof( anBoardOld ) );
	    
	    FindBestMove( NULL, anDiceTrain[ 0 ], anDiceTrain[ 1 ],
			  anBoardTrain, &ciCubeless, &ecTD );
	    
	    if( fAction )
		fnAction();
	
	    if( fInterrupt )
		break;
	    
	    SwapSides( anBoardTrain );
	    
	    EvaluatePosition( anBoardTrain, ar, &ciCubeless, &ecTD );
	    
	    InvertEvaluation( ar );
	    if( TrainPosition( anBoardOld, ar, rAlpha, rAnneal ) )
		break;
	    
	    /* FIXME can stop as soon as perfect */
	} while( ( !n || c <= n ) && !fInterrupt &&
		 !GameStatus( anBoardTrain ) );
    }

    ProgressEnd();
}

#if HAVE_LIBREADLINE
static command *pcCompleteContext;

static char *NullGenerator( const char *sz, int nState ) {

  return NULL;
}

static char *GenerateKeywords( const char *sz, int nState ) {

    static int cch;
    static command *pc;
    char *szDup;

    if( !nState ) {
      cch = strlen( sz );
      pc = pcCompleteContext;
    }

    while( pc && pc->sz ) {
      if( !strncasecmp( sz, pc->sz, cch ) && pc->szHelp ) {
        if( !( szDup = malloc( strlen( pc->sz ) + 1 ) ) )
          return NULL;

        strcpy( szDup, pc->sz );
        
        pc++;
	    
        return szDup;
      }
	
      pc++;
    }
    
    return NULL;
}
static char *ERCompletion( const char *sz, int nState ) {

    static int i, cch;
    char *pch, *szDup;

    if( !nState ) {
	cch = strlen( sz );
	i = 0;
    }

    while( i < 2 ) {
	pch = i++ ? "rollout" : "evaluation";
	
	if( !strncasecmp( sz, pch, cch ) ) {
	    if( !( szDup = malloc( strlen( pch ) + 1 ) ) )
		return NULL;
	    
	    return strcpy( szDup, pch );
	}
    }

    return NULL;
}

static char *OnOffCompletion( const char *sz, int nState ) {

    static int i, cch;
    static char *asz[] = { "false", "no", "off", "on", "true", "yes" };
    char *pch, *szDup;
    
    if( !nState ) {
	cch = strlen( sz );
	i = 0;
    }

    while( i < DIM( asz ) ) {
	pch = asz[ i++ ];

	if( !strncasecmp( sz, pch, cch ) ) {
	    if( !( szDup = malloc( strlen( pch ) + 1 ) ) )
		return NULL;

	    return strcpy( szDup, pch );
	}
    }
    
    return NULL;
}

static char *PlayerCompletionGen( const char *sz, int nState, int fBoth ) {

    static int i, cch;
    char *pch, *szDup;
    
    if( !nState ) {
	cch = strlen( sz );
	i = 0;
    }

    while( i < ( fBoth ? 5 : 4 ) ) {
	switch( i ) {
	case 0:
	    pch = "0";
	    break;
	case 1:
	    pch = "1";
	    break;
	case 2:
	    pch = ap[ 0 ].szName;
	    break;
	case 3:
	    pch = ap[ 1 ].szName;
	    break;
	case 4:
	    pch = "both";
	    break;
	default:
	    abort();
	}

	i++;

	if( !strncasecmp( sz, pch, cch ) ) {
	    if( !( szDup = malloc( strlen( pch ) + 1 ) ) )
		return NULL;

	    return strcpy( szDup, pch );
	}
    }
    
    return NULL;
}

static char *PlayerCompletion( const char *sz, int nState ) {

    return PlayerCompletionGen( sz, nState, FALSE );
}

static char *PlayerCompletionBoth( const char *sz, int nState ) {

    return PlayerCompletionGen( sz, nState, TRUE );
}

static command *FindContext( command *pc, char *szOrig, int ich ) {

#if __GNUC__
    char sz[ strlen( szOrig ) + 1 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szOrig ) + 1 );
#else
    char sz[ 4096 ];
#endif
    char *pch, *pchCurrent;
    command *pcResume = NULL;
    
    pch = strcpy( sz, szOrig );
    pch[ ich ] = 0;
    
    do {
	if( !( pchCurrent = NextToken( &pch ) ) )
	    /* no command */
	    return pc;

	if( !pch )
	    /* incomplete command */
	    return pc;

	if( pcResume ) {
	    pc = pcResume;
	    pcResume = NULL;
	    continue;
	}
	
        while( pc && pc->sz ) {
            if( !strncasecmp( pchCurrent, pc->sz, strlen( pchCurrent ) ) ) {
		pc = pc->pc;
		
		if( pc == acSetPlayer || pc == acSetRolloutPlayer ) {
		    pcResume = pc;
		    pc = &cPlayerBoth;
		}
		
                break;
            }

            pc++;
        }
    } while( pcResume || ( pc && pc->sz ) );

    if( pc && pc->pc ) {
	/* dummy command for parameter completion */
	if( !NextToken( &pch ) )
	    return pc;
    }

    /* the command is already complete */
    return NULL;
}

#if !HAVE_RL_COMPLETION_MATCHES
    /* assume obselete version of readline */
#define rl_completion_matches( text, func ) \
	completion_matches( (char *) (text), (func) )
#define rl_filename_completion_function filename_completion_function
#endif

static char **CompleteKeyword( const char *szText, int iStart, int iEnd ) {

    if( fReadingOther )
	return rl_completion_matches( szText, NullGenerator );
    
    pcCompleteContext = FindContext( acTop, rl_line_buffer, iStart );

    if( !pcCompleteContext )
	return NULL;

    if( pcCompleteContext == &cER )
	return rl_completion_matches( szText, ERCompletion );
    else if( pcCompleteContext == &cFilename ) {
	rl_filename_completion_desired = TRUE;
	return rl_completion_matches( szText,
				      rl_filename_completion_function );
    } else if( pcCompleteContext == &cOnOff )
	return rl_completion_matches( szText, OnOffCompletion );
    else if( pcCompleteContext == &cPlayer )
	return rl_completion_matches( szText, PlayerCompletion );
    else if( pcCompleteContext == &cPlayerBoth )
	return rl_completion_matches( szText, PlayerCompletionBoth );
    else
	return rl_completion_matches( szText, GenerateKeywords );
}
#endif

extern void Prompt( void ) {

    if( !fInteractive || !isatty( STDIN_FILENO ) )
	return;

    ProgressEnd();
    
    output( FormatPrompt() );
    fflush( stdout );    
}

#if USE_GUI
#if HAVE_LIBREADLINE
static void ProcessInput( char *sz, int fFree ) {
    
    rl_callback_handler_remove();
    fReadingCommand = FALSE;
    
    if( !sz ) {
	outputc( '\n' );
	PromptForExit();
	sz = "";
    }
    
    fInterrupt = FALSE;
    
    if( *sz )
	add_history( sz );
	
    HandleCommand( sz, acTop );

    ResetInterrupt();

    if( fFree )
	free( sz );

    /* Recalculate prompt -- if we call nothing, then readline will
       redisplay the old prompt.  This isn't what we want: we either
       want no prompt at all, yet (if NextTurn is going to be called),
       or if we do want to prompt immediately, we recalculate it in
       case the information in the old one is out of date. */
#if USE_GTK
    if( nNextTurn )
#else
    if( evNextTurn.fPending )
#endif
	fNeedPrompt = TRUE;
    else {
	ProgressEnd();
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	fReadingCommand = TRUE;
    }
}

extern void HandleInput( char *sz ) {

    ProcessInput( sz, TRUE );
}

static char *szInput;
static int fInputAgain;

void HandleInputRecursive( char *sz ) {

    if( !sz ) {
	outputc( '\n' );
	PromptForExit();
	szInput = NULL;
	fInputAgain = TRUE;
	return;
    }

    szInput = sz;

    rl_callback_handler_remove();
}
#endif

/* Handle a command as if it had been typed by the user. */
extern void UserCommand( char *szCommand ) {

#if HAVE_LIBREADLINE
    int nOldEnd;
#endif
    int cch = strlen( szCommand ) + 1;
#if __GNUC__
    char sz[ cch ];
#elif HAVE_ALLOCA
    char *sz = alloca( cch );
#else
    char sz[ 1024 ];
    assert( cch <= 1024 );
#endif
    
    /* Unfortunately we need to copy the command, because it might be in
       read-only storage and HandleCommand might want to modify it. */
    strcpy( sz, szCommand );

#if USE_GTK
    if( !fTTY ) {
	fInterrupt = FALSE;
	HandleCommand( sz, acTop );
	ResetInterrupt();

	return;
    }
#endif

    /* Note that the command is always echoed to stdout; the output*()
       functions are bypassed. */
#if HAVE_LIBREADLINE
    if( fReadline ) {
	nOldEnd = rl_end;
	rl_end = 0;
	rl_redisplay();
	puts( sz );
	ProcessInput( sz, FALSE );
	return;
    }
#endif

    if( fInteractive ) {
	putchar( '\n' );
	Prompt();
	puts( sz );
    }
    
    fInterrupt = FALSE;
    
    HandleCommand( sz, acTop );

    ResetInterrupt();
    
#if USE_GTK
    if( nNextTurn )
#else
    if( evNextTurn.fPending )
#endif
	Prompt();
    else
	fNeedPrompt = TRUE;
}

#if USE_GTK
extern gint NextTurnNotify( gpointer p )
#else
extern int NextTurnNotify( event *pev, void *p )
#endif
{

    NextTurn( TRUE );

    ResetInterrupt();

#if USE_GTK
    if( fNeedPrompt )
#else
    if( !pev->fPending && fNeedPrompt )
#endif
    {
#if HAVE_LIBREADLINE
	if( fReadline ) {
	    ProgressEnd();
	    rl_callback_handler_install( FormatPrompt(), HandleInput );
	    fReadingCommand = TRUE;
	} else
#endif
	    Prompt();
	
	fNeedPrompt = FALSE;
    }
    
    return FALSE; /* remove idle handler, if GTK */
}
#endif

/* Read a line from stdin, and handle X and readline input if
 * appropriate.  This function blocks until a line is ready, and does
 * not call HandleEvents(), and because fBusy will be set some X
 * commands will not work.  Therefore, it should not be used for
 * reading top level commands.  The line it returns has been allocated
 * with malloc (as with readline()). */
extern char *GetInput( char *szPrompt ) {

    /* FIXME handle interrupts and EOF in this function (looks like it's
       done already). */
    
    char *sz;
    char *pch;

#if USE_GTK
    assert( fTTY && !fX );
#endif

#if USE_EXT
    fd_set fds;

    if( fX ) {
#if HAVE_LIBREADLINE
	if( fReadline ) {
	    /* Using readline and X. */
	    char *szOldPrompt, *szOldInput;
	    int nOldEnd, nOldMark, nOldPoint, fWasReadingCommand;
	
	    if( fInterrupt )
		return NULL;

	    fReadingOther = TRUE;
	    
	    if( ( fWasReadingCommand = fReadingCommand ) ) {
		/* Save old readline context. */
		szOldPrompt = rl_prompt;
		szOldInput = rl_copy_text( 0, rl_end );
		nOldEnd = rl_end;
		nOldMark = rl_mark;
		nOldPoint = rl_point;
		fReadingCommand = FALSE;
		/* FIXME this is unnecessary when handling an X command! */
		outputc( '\n' );
	    }
	    
	    szInput = FALSE;
	    
	    rl_callback_handler_install( szPrompt, HandleInputRecursive );
	    
	    while( !szInput ) {
		FD_ZERO( &fds );
		FD_SET( STDIN_FILENO, &fds );
		FD_SET( ConnectionNumber( ewnd.pdsp ), &fds );
		
		select( ConnectionNumber( ewnd.pdsp ) + 1, &fds, NULL, NULL,
			NULL );

		if( fInterrupt ) {
		    outputc( '\n' );
		    break;
		}
		
		if( FD_ISSET( STDIN_FILENO, &fds ) ) {
		    rl_callback_read_char();
		    if( fInputAgain ) {
			rl_callback_handler_install( szPrompt,
						     HandleInputRecursive );
			szInput = NULL;
			fInputAgain = FALSE;
		    }
		}

		if( FD_ISSET( ConnectionNumber( ewnd.pdsp ), &fds ) )
		    HandleXAction();
	    }

	    if( fWasReadingCommand ) {
		/* Restore old readline context. */
		rl_callback_handler_install( szOldPrompt, HandleInput );
		rl_insert_text( szOldInput );
		free( szOldInput );
		rl_end = nOldEnd;
		rl_mark = nOldMark;
		rl_point = nOldPoint;
		rl_redisplay();
		fReadingCommand = TRUE;
	    } else
		rl_callback_handler_remove();	
	    
	    fReadingOther = FALSE;
	    
	    return szInput;
	}
#endif
	/* Using X, but not readline. */
	if( fInterrupt )
	    return NULL;

	outputc( '\n' );
	output( szPrompt );
	fflush( stdout );

	do {
	    FD_ZERO( &fds );
	    FD_SET( STDIN_FILENO, &fds );
	    FD_SET( ConnectionNumber( ewnd.pdsp ), &fds );

	    select( ConnectionNumber( ewnd.pdsp ) + 1, &fds, NULL, NULL,
		    NULL );

	    if( fInterrupt ) {
		outputc( '\n' );
		return NULL;
	    }
	    
	    if( FD_ISSET( ConnectionNumber( ewnd.pdsp ), &fds ) )
		HandleXAction();
	} while( !FD_ISSET( STDIN_FILENO, &fds ) );

	goto ReadDirect;
    }
#endif
#if HAVE_LIBREADLINE
    if( fReadline ) {
	/* Using readline, but not X. */
	if( fInterrupt )
	    return NULL;
	
	fReadingOther = TRUE;
	
	while( !( sz = readline( szPrompt ) ) ) {
	    outputc( '\n' );
	    PromptForExit();
	}
	
	fReadingOther = FALSE;
	
	if( fInterrupt )
	    return NULL;
	
	return sz;
    }
#endif
    /* Not using readline or X. */
    if( fInterrupt )
	return NULL;

    output( szPrompt );
    fflush( stdout );

#if USE_EXT
 ReadDirect:
#endif
    sz = malloc( 256 ); /* FIXME it would be nice to handle longer strings */

    clearerr( stdin );
    pch = fgets( sz, 256, stdin );

    if( fInterrupt ) {
	free( sz );
	return NULL;
    }
    
    if( !pch ) {
	if( !isatty( STDIN_FILENO ) )
	    exit( EXIT_SUCCESS );
	
	outputc( '\n' );
	PromptForExit();
    }
    
    if( ( pch = strchr( sz, '\n' ) ) )
	*pch = 0;
    
    return sz;
}

/* Ask a yes/no question.  Interrupting the question is considered a "no"
   answer. */
extern int GetInputYN( char *szPrompt ) {

    char *pch;

#if USE_GTK
    if( fX )
	return GTKGetInputYN( szPrompt );
#endif
    
    if( fInterrupt )
	return FALSE;

    while( 1 ) {
	pch = GetInput( szPrompt );

	if( pch )
	    switch( *pch ) {
	    case 'y':
	    case 'Y':
		free( pch );
		return TRUE;
	    case 'n':
	    case 'N':
		free( pch );
		return FALSE;
	    default:
		free( pch );
	    }

	if( fInterrupt )
	    return FALSE;
	
	outputl( "Please answer `y' or `n'." );
    }
}

/* Like strncpy, except it does the right thing */
extern char *strcpyn( char *szDest, const char *szSrc, int cch ) {

    char *pchDest = szDest;
    const char *pchSrc = szSrc;

    if( cch-- < 1 )
	return szDest;
    
    while( cch-- )
	if( !( *pchDest++ = *pchSrc++ ) )
	    return szDest;

    *pchDest = 0;

    return szDest;
}

/* Write a string to stdout/status bar/popup window */
extern void output( char *sz ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX ) {
	GTKOutput( g_strdup( sz ) );
	return;
    }
#endif
    fputs( sz, stdout );
}

/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( char *sz ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX ) {
	int cch;
	char *pch;

	cch = strlen( sz );
	pch = g_malloc( cch + 2 );
	strcpy( pch, sz );
	pch[ cch ] = '\n';
	pch[ cch + 1 ] = 0;
	GTKOutput( pch );
	return;
    }
#endif
    puts( sz );
}
    
/* Write a character to stdout/status bar/popup window */
extern void outputc( char ch ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX ) {
	char *pch;

	pch = g_malloc( 2 );
	*pch = ch;
	pch[ 1 ] = 0;
	GTKOutput( pch );
	return;
    }
#endif
    putchar( ch );
}
    
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( char *sz, ... ) {

    va_list val;

    va_start( val, sz );
    outputv( sz, val );
    va_end( val );
}

/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( char *sz, va_list val ) {

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX ) {
	GTKOutput( g_strdup_vprintf( sz, val ));
	return;
    }
#endif
    vprintf( sz, val );
}

/* Signifies that all output for the current command is complete */
extern void outputx( void ) {
    
    if( cOutputDisabled || cOutputPostponed )
	return;
    
#if USE_GTK
    if( fX )
	GTKOutputX();
#endif
}

/* Signifies that subsequent output is for a new command */
extern void outputnew( void ) {
    
    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX )
	GTKOutputNew();
#endif
}

/* Disable output */
extern void outputoff( void ) {

    cOutputDisabled++;
}

/* Enable output */
extern void outputon( void ) {

    assert( cOutputDisabled );

    cOutputDisabled--;
}

/* Temporarily disable outputx() calls */
extern void outputpostpone( void ) {

    cOutputPostponed++;
}

/* Re-enable outputx() calls */
extern void outputresume( void ) {

    assert( cOutputPostponed );

    if( !--cOutputPostponed )
	outputx();
}

extern void ProgressStart( char *sz ) {

    if( !fShowProgress )
	return;

    fInProgress = TRUE;
    
#if USE_GTK
    if( fX ) {
	GTKProgressStart( sz );
	return;
    }
#endif

    if( sz ) {
	fputs( sz, stdout );
	fflush( stdout );
    }
}


extern void
ProgressStartValue ( char *sz, int iMax ) {

  if ( !fShowProgress )
    return;

  iProgressMax = iMax;
  iProgressValue = 0;
  pcProgress = sz;

#if USE_GTK
  if( fX ) {
    GTKProgressStartValue( sz, iMax );
    return;
  }
#endif

  if( sz ) {
    fputs( sz, stdout );
    fflush( stdout );
  }

}


extern void
ProgressValue ( int iValue ) {

  iProgressValue = iValue;

#if USE_GTK
  if( fX ) {
    GTKProgressValue( iValue );
    return;
  }
#endif

  outputf ( "\r%s %d/%d\r", pcProgress, iProgressValue, iProgressMax );
  fflush ( stdout );

}


extern void
ProgressValueAdd ( int iValue ) {

  ProgressValue ( iProgressValue + iValue );

}


extern void Progress( void ) {

    static int i = 0;
    static char ach[ 4 ] = "/-\\|";
    
    if( !fShowProgress )
	return;

#if USE_GTK
    if( fX ) {
	GTKProgress();
	return;
    }
#endif

    putchar( ach[ i++ ] );
    i &= 0x03;
    putchar( '\b' );
    fflush( stdout );
}

static void CallbackProgress( void ) {

#if USE_GTK
    if( fX )
	while( gtk_events_pending() )
	    gtk_main_iteration();
#endif
    
    if( fInProgress && !iProgressMax )
	Progress();
}

extern void ProgressEnd( void ) {

    int i;
    
    if( !fShowProgress )
	return;

    fInProgress = FALSE;
    iProgressMax = 0;
    iProgressValue = 0;
    pcProgress = NULL;
    
#if USE_GTK
    if( fX ) {
	GTKProgressEnd();
	return;
    }
#endif

    putchar( '\r' );
    for( i = 0; i < 79; i++ )
	putchar( ' ' );
    putchar( '\r' );
    fflush( stdout );

}

extern RETSIGTYPE HandleInterrupt( int idSignal ) {

    /* NB: It is safe to write to fInterrupt even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    fInterrupt = TRUE;
}

#if USE_GUI && defined(SIGIO)
static RETSIGTYPE HandleIO( int idSignal ) {

    /* NB: It is safe to write to fAction even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    if( fX )
	fAction = TRUE;
}
#endif

static void BearoffProgress( int i ) {

#if USE_GTK
    if( fX ) {
	GTKBearoffProgress( i );
	return;
    }
#endif
    putchar( "\\|/-"[ ( i / 1000 ) % 4 ] );
    putchar( '\r' );
    fflush( stdout );
}

static void usage( char *argv0 ) {

#if USE_GUI

    printf(
"Usage: %s [options] [saved-game-file]\n"
"Options:\n"
"  -b, --no-bearoff          Do not use bearoff database\n"
"  -c FILE, --commands FILE  Read commands from FILE and exit\n"
"  -d DIR, --datadir DIR     Read database and weight files from directory "
"DIR\n"
"  -h, --help                Display usage and exit\n"
"  -n[S], --new-weights[=S]  Create new neural net (of size S)\n"
"  -r, --no-rc               Do not read .gnubgrc and .gnubgautorc commands\n"
"  -s FILE, --script FILE    Evaluate Scheme code in FILE and exit\n"
"  -t, --tty                 Start on tty instead of using window system\n"
"  -v, --version             Show version information and exit\n"
"  -w, --window-system-only  Ignore tty input when using window system\n"
"\n"
"For more information, type `help' from within gnubg.\n"
"Please report bugs to <bug-gnubg@gnu.org>.\n", argv0 );
#else

    printf(
"Usage: %s [options] [saved-game-file]\n"
"Options:\n"
"  -b, --no-bearoff          Do not use bearoff database\n"
"  -d DIR, --datadir DIR     Read database and weight files from directory "
"DIR\n"
"  -h, --help                Display usage and exit\n"
"  -n[S], --new-weights[=S]  Create new neural net (of size S)\n"
"  -r, --no-rc               Do not read .gnubgrc and .gnubgautorc commands\n"
"  -v, --version             Show version information and exit\n"
"\n"
"For more information, type `help' from within gnubg.\n"
"Please report bugs to <bug-gnubg@gnu.org>.\n", argv0 );
#endif

}

static void version( void ) {

    char **ppch = aszVersion;

    while( *ppch )
	puts( *ppch++ );
}

static void real_main( void *closure, int argc, char *argv[] ) {

    char ch, *pch, *pchCommands = NULL, *pchScript = NULL;
    int n, nNewWeights = 0, fNoRC = FALSE, fNoBearoff = FALSE;
    static struct option ao[] = {
	{ "datadir", required_argument, NULL, 'd' },
	{ "no-bearoff", no_argument, NULL, 'b' },
	{ "no-rc", no_argument, NULL, 'r' },
	{ "new-weights", optional_argument, NULL, 'n' },
	{ "window-system-only", no_argument, NULL, 'w' },
	/* these options must come last -- see below. */
	{ "commands", required_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
	{ "script", required_argument, NULL, 's' },
        { "tty", no_argument, NULL, 't' },
        { "version", no_argument, NULL, 'v' },
        { NULL, 0, NULL, 0 }
    };
#if HAVE_GETPWUID
    struct passwd *ppwd;
#endif
#if HAVE_LIBREADLINE
    char *sz;
#endif

    if( !( szHomeDirectory = getenv( "HOME" ) ) )
	/* FIXME what should non-POSIX systems do? */
	szHomeDirectory = ".";
    
#if USE_GUI
    /* The GTK interface is fairly grotty; it makes it impossible to
       separate argv handling from attempting to open the display, so
       we have to check for -t before the other options to avoid connecting
       to the X server if it is specified.

       We use the last five element of ao to get the "--help", "--tty",
       "--commands", "--script" and "--version" options only. */
    
    opterr = 0;
    
    while( ( ch = getopt_long( argc, argv, "chstv", ao + sizeof( ao ) /
			       sizeof( ao[ 0 ] ) - 6, NULL ) ) != (char) -1 )
	switch( ch ) {
	case 's': /* script */
	case 'c': /* commands */
	case 't': /* tty */
	    fX = FALSE;
	    break;
	case 'h': /* help */
            usage( argv[ 0 ] );
	    exit( EXIT_SUCCESS );
	case 'v': /* version */
	    version();
	    exit( EXIT_SUCCESS );
	}
    
    optind = 0;
    opterr = 1;

    if( fX )
#if USE_GTK
	fX = InitGTK( &argc, &argv );
#else
        if( !getenv( "DISPLAY" ) )
	    fX = FALSE;
#endif

    if( fX ) {
#if WIN32
	/* The MS Windows port cannot support multiplexed GUI/TTY input;
	   disable the TTY (as if the -w option was specified). */
	fTTY = FALSE;
#endif
	fInteractive = fShowProgress = TRUE;
#if HAVE_LIBREADLINE
	fReadline = isatty( STDIN_FILENO );
#endif
    } else 
#endif
	{
#if HAVE_LIBREADLINE
	    fReadline =
#endif
		fInteractive = isatty( STDIN_FILENO );
	    fShowProgress = isatty( STDOUT_FILENO );
	}

#if HAVE_FSTAT && HAVE_SETVBUF
    {
	/* Use line buffering if stdout/stderr are pipes or sockets;
	   Jens Hoefkens points out that buffering causes problems
	   for other processes issuing gnubg commands via IPC. */
	struct stat st;
	
	if( !fstat( STDOUT_FILENO, &st ) && ( S_ISFIFO( st.st_mode )
#ifdef S_ISSOCK
					      || S_ISSOCK( st.st_mode )
#endif
	    ) )
	    setvbuf( stdout, NULL, _IOLBF, 0 );
	
	if( !fstat( STDERR_FILENO, &st ) && ( S_ISFIFO( st.st_mode )
#ifdef S_ISSOCK
					      || S_ISSOCK( st.st_mode )
#endif
	    ) )
	    setvbuf( stderr, NULL, _IOLBF, 0 );
    }
#endif
		
    while( ( ch = getopt_long( argc, argv, "bc:d:hn::rs:tvw", ao, NULL ) ) !=
           (char) -1 )
	switch( ch ) {
	case 'b': /* no-bearoff */
	    fNoBearoff = TRUE;
	    break;
	case 'c': /* commands */
	    pchCommands = optarg;
	    fInteractive = FALSE;
	    break;
	case 'd': /* datadir */
	    szDataDirectory = optarg;
	    break;
	case 'h': /* help */
            usage( argv[ 0 ] );
	    exit( EXIT_SUCCESS );
	case 'n': /* new-weights */
	    if( optarg )
		nNewWeights = atoi( optarg );

	    if( nNewWeights < 1 )
		nNewWeights = DEFAULT_NET_SIZE;

	    break;
	case 'r': /* no-rc */
	    fNoRC = TRUE;
	    break;
	case 's': /* script */
#if !USE_GUILE
	    fprintf( stderr, "%s: option `-s' requires Guile\n", argv[ 0 ] );
	    exit( EXIT_FAILURE );
#endif
	    pchScript = optarg;
	    fInteractive = FALSE;
	    break;
	case 't':
	    /* silently ignore (if it was relevant, it was handled earlier). */
	    break;
	case 'v': /* version */
	    version();
	    exit( EXIT_SUCCESS );
	case 'w':
#if USE_GTK
	    if( fX )
		fTTY = FALSE;
#else
	    /* silently ignore */
#endif
	    break;
	default:
	    usage( argv[ 0 ] );
	    exit( EXIT_FAILURE );
	}

#if USE_GTK
    if( fTTY )
#endif
	puts( "GNU Backgammon " VERSION "  Copyright 1999, 2000, 2001, 2002 "
	      "Gary Wong.\n"
	      "GNU Backgammon is free software, covered by the GNU "
	      "General Public License\n"
	      "version 2, and you are welcome to change it and/or "
	      "distribute copies of it\n"
	      "under certain conditions.  Type \"show copying\" to see "
	      "the conditions.\n"
	      "There is absolutely no warranty for GNU Backgammon.  "
	      "Type \"show warranty\" for\n"
	      "details." );
    
    InitRNG( NULL, TRUE );
    InitRNG( &rcRollout.nSeed, FALSE );
    /* we don't want rollouts to use the same seed as normal dice (which
       could happen if InitRNG had to use the current time as a seed) -- mix
       it up a little bit */
    rcRollout.nSeed ^= 0x792A584B;
    
    InitMatchEquity ( "met/zadeh.xml", szDataDirectory );
    
    if( ( n = EvalInitialise( nNewWeights ? NULL : GNUBG_WEIGHTS,
			      nNewWeights ? NULL : GNUBG_WEIGHTS_BINARY,
			      fNoBearoff ? NULL : GNUBG_BEAROFF,
			      szDataDirectory, nNewWeights,
			      fShowProgress ? BearoffProgress : NULL ) ) < 0 )
	exit( EXIT_FAILURE );
    else if( n > 0 && !nNewWeights ) {
	outputl( "WARNING: No neural net weights were found.  GNU Backgammon "
		 "will create an\n"
		 "initial random network, but this will be unsuitable for "
		 "use until training\n"
		 "is complete.  Please consult the manual for information "
		 "about training, or\n"
		 "directions for obtaining a pre-trained network." );
	outputx();
    }
#if USE_GUILE
    GuileInitialise( szDataDirectory );
#endif
    
    if( ( pch = getenv( "LOGNAME" ) ) )
	strcpy( ap[ 1 ].szName, pch );
    else if( ( pch = getenv( "USER" ) ) )
	strcpy( ap[ 1 ].szName, pch );
#if HAVE_GETLOGIN
    else if( ( pch = getlogin() ) )
	strcpy( ap[ 1 ].szName, pch );
#endif
#if HAVE_GETPWUID
    else if( ( ppwd = getpwuid( getuid() ) ) )
	strcpy( ap[ 1 ].szName, ppwd->pw_name );
#endif
    
    ListCreate( &lMatch );
    IniStatcontext( &scMatch );
    
#if USE_GTK
    if( fTTY )
#endif
	if( fInteractive ) {
	    PortableSignal( SIGINT, HandleInterrupt, NULL, FALSE );
	    
#if HAVE_LIBREADLINE
	    rl_readline_name = "gnubg";
	    rl_basic_word_break_characters = rl_filename_quote_characters =
		szCommandSeparators;
	    rl_completer_quote_characters = "\"'";
#if HAVE_RL_COMPLETION_MATCHES
	    /* assume readline 4.2 or later */
	    rl_completion_entry_function = NullGenerator;
	    rl_attempted_completion_function = CompleteKeyword;
#else
	    /* assume old readline */
	    rl_completion_entry_function = (Function *) NullGenerator;
	    rl_attempted_completion_function = (CPPFunction *) CompleteKeyword;
#endif
#endif
	}
    
#if USE_GUI && defined(SIGIO)
    if( fX )
	PortableSignal( SIGIO, HandleIO, NULL, TRUE );
#endif

    fnTick = CallbackProgress;
    
    if( !fNoRC )
	LoadRCFiles();
    
    if( optind < argc && *argv[ optind ] )
       CommandLoadMatch( argv[ optind ] );

    fflush( stdout );
    fflush( stderr );

    if( pchCommands ) {
	CommandLoadCommands( pchCommands );
	exit( EXIT_SUCCESS );
    }

#if USE_GUILE
    if( pchScript ) {
	scm_primitive_load( scm_makfrom0str( pchScript ) );
	exit( EXIT_SUCCESS );
    }
#endif
    
#if USE_GTK
    if( fX ) {
	RunGTK();

	exit( EXIT_SUCCESS );
    }
#elif USE_EXT
    if( fX ) {
        RunExt();

	fputs( "Could not open X display.  Continuing on TTY.\n", stderr );
        fX = FALSE;
    }
#endif
    
    for(;;) {
#if HAVE_LIBREADLINE
	if( fReadline ) {
	    while( !( sz = readline( FormatPrompt() ) ) ) {
		outputc( '\n' );
		PromptForExit();
	    }
	    
	    fInterrupt = FALSE;
	    
	    if( *sz )
		add_history( sz );
	    
	    HandleCommand( sz, acTop );
	    
	    free( sz );
	} else
#endif
	    {
		char sz[ 2048 ], *pch;
	
		sz[ 0 ] = 0;
		
		Prompt();
		
		clearerr( stdin );		    
		/* FIXME shouldn't restart sys calls on signals during this
		   fgets */
		fgets( sz, sizeof( sz ), stdin );

		if( ( pch = strchr( sz, '\n' ) ) )
		    *pch = 0;
		
		
		if( feof( stdin ) ) {
		    if( !isatty( STDIN_FILENO ) )
			exit( EXIT_SUCCESS );
		    
		    outputc( '\n' );
		    
		    if( !sz[ 0 ] )
			PromptForExit();
		    
		    continue;
		}	
		
		fInterrupt = FALSE;
	
		HandleCommand( sz, acTop );
	    }

	while( fNextTurn )
	    NextTurn( TRUE );

	ResetInterrupt();
    }
}

extern int main( int argc, char *argv[] ) {

#if USE_GUILE
    scm_boot_guile( argc, argv, real_main, NULL );
#else
    real_main( NULL, argc, argv );
#endif
    /* should never return */
    
    return EXIT_FAILURE;
}

/*
 * Command: convert normalised money equity to match winning chance.
 *
 * The result is written to stdout.
 * FIXME: implement GTK version.
 * FIXME: allow more parameters (match score, match length)
 *
 * Input: 
 *   sz: string with equity
 *
 * Output:
 *   none.
 *
 */

extern void CommandEq2MWC ( char *sz ) {

  float rEq;
  cubeinfo ci;

  if ( ! ms.nMatchTo ) {
    outputl ( "Command only valid in match play" );
    return;
  }


  rEq = ParseReal ( &sz );

  if ( rEq == ERR_VAL ) rEq = 0.0;

  GetMatchStateCubeInfo( &ci, &ms );

  outputf ( "MWC for equity = %+6.3f: %6.2f%%\n",
            -1.0, 100.0 * eq2mwc ( -1.0, &ci ) );
  outputf ( "MWC for equity = %+6.3f: %6.2f%%\n",
            +1.0, 100.0 * eq2mwc ( +1.0, &ci ) );
  outputf ( "By linear interpolation:\n" );
  outputf ( "MWC for equity = %+6.3f: %6.2f%%\n",
            rEq, 100.0 * eq2mwc ( rEq, &ci ) );

}


/*
 * Command: convert match winning chance to normalised money equity.
 *
 * The result is written to stdout.
 * FIXME: implement GTK version.
 * FIXME: allow more parameters (match score, match length)
 *
 * Input: 
 *   sz: string with match winning chance
 *
 * Output:
 *   none.
 *
 */

extern void CommandMWC2Eq ( char *sz ) {

  float rMwc;
  cubeinfo ci;

  if ( ! ms.nMatchTo ) {
    outputl ( "Command only valid in match play" );
    return;
  }

  GetMatchStateCubeInfo( &ci, &ms );

  rMwc = ParseReal ( &sz );

  if ( rMwc == ERR_VAL ) rMwc = eq2mwc ( 0.0, &ci );

  if ( rMwc > 1.0 ) rMwc /= 100.0;

  outputf ( "Equity for MWC = %6.2f%%: %+6.3f\n",
            100.0 * eq2mwc ( -1.0, &ci ), -1.0 );
  outputf ( "Equity for MWC = %6.2f%%: %+6.3f\n",
            100.0 * eq2mwc ( +1.0, &ci ), +1.0 );
  outputf ( "By linear interpolation:\n" );
  outputf ( "Equity for MWC = %6.2f%%: %+6.3f\n",
            100.0 * rMwc, mwc2eq ( rMwc, &ci ) );


}



