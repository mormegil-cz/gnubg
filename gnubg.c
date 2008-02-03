/*
 * gnubg.c
 *
 * by Gary Wong <gtw@gnu.org>, 1998, 1999, 2000, 2001, 2002, 2003.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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
#if USE_PYTHON
#undef HAVE_FSTAT
#include <Python.h>
#endif

#include <sys/types.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <signal.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <ctype.h>
#if WIN32
#include <io.h>
#endif

#if HAVE_LIBREADLINE
static char *gnubg_histfile;
#include <readline/history.h>
#include <readline/readline.h>
static int fReadingOther;
static char szCommandSeparators[] = " \t\n\r\v\f";
#endif

#include "analysis.h"
#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "eval.h"
#include "sgf.h"
#include "export.h"
#include "import.h"
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <locale.h>
#include "matchequity.h"
#include "matchid.h"
#include "positionid.h"
#include "render.h"
#include "renderprefs.h"
#include "rollout.h"
#include "sound.h"
#include "record.h"
#include "progress.h"
#include "osr.h"
#include "format.h"
#include "relational.h"
#include "credits.h"
#include "external.h"
#include "neuralnet.h"
#include "gnubgmodule.h"
#include "util.h"

#if HAVE_SOCKETS
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#endif /* #if HAVE_SYS_SOCKET_H */
#ifndef WIN32
#include <stdio.h>
#else /* #ifndef WIN32 */
#include <winsock2.h>
#include <ws2tcpip.h>

#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE

#define inet_aton(ip,addr)  (addr)->s_addr = inet_addr(ip), 1
#define inet_pton(fam,ip,addr) (addr)->s_addr = inet_addr(ip), 1

#endif /* #ifndef WIN32 */

#endif


#if USE_GTK
#include <gtk/gtk.h>
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#include "gtksplash.h"
#include "gtkchequer.h"
#include "gtkwindows.h"
#endif

#if USE_BOARD3D
#include "fun3d.h"
#endif
#include "multithread.h"
#include "openurl.h"

#if defined(MSDOS) || defined(__MSDOS__) || defined(WIN32)
#define NO_BACKSLASH_ESCAPES 1
#endif

#if USE_GTK
int fX = FALSE; /* use X display */
unsigned int nDelay = 300;
int fNeedPrompt = FALSE;
#if HAVE_LIBREADLINE
int fReadingCommand;
#endif
#endif

#if !defined(SIGIO) && defined(SIGPOLL)
#define SIGIO SIGPOLL /* The System V equivalent */
#endif

char *szLang=NULL;

char szDefaultPrompt[] = "(\\p) ",
    *szPrompt = szDefaultPrompt;
static int fInteractive, cOutputDisabled, cOutputPostponed;

matchstate ms = {
    {{0}, {0}}, /* anBoard */
    {0}, /* anDice */
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
    VARIATION_STANDARD, /*bgv */
    TRUE, /* fCubeUse */
    TRUE, /* fJacoby */
    GAME_NONE /* gs */
};
ConstTanBoard msBoard(void) {return (ConstTanBoard)ms.anBoard;}

matchinfo mi;

int fDisplay = TRUE, fAutoBearoff = FALSE, fAutoGame = TRUE, fAutoMove = FALSE,
    fAutoCrawford = 1, fAutoRoll = TRUE,
    fCubeUse = TRUE, 
    fConfirm = TRUE, fShowProgress, fJacoby = TRUE,
    fOutputRawboard = FALSE, 
    fAnalyseCube = TRUE,
    fAnalyseDice = TRUE, fAnalyseMove = TRUE, fRecord = TRUE,
    nToolbarStyle = 2, fStyledGamelist = TRUE, fFullScreen = FALSE;
unsigned int cAnalysisMoves = 1, cAutoDoubles = 0, nDefaultLength = 7, nBeavers = 3;
int fCubeEqualChequer = TRUE, fPlayersAreSame = TRUE, 
	fTruncEqualPlayer0 =TRUE;
int fInvertMET = FALSE;
int fConfirmSave = TRUE;
int fTutor = FALSE, fTutorCube = TRUE, fTutorChequer = TRUE;
int fTutorAnalysis = FALSE;
int nThreadPriority = 0;
int fCheat = FALSE;
unsigned int afCheatRoll[ 2 ] = { 0, 0 };
int fGotoFirstGame = FALSE;
float rRatingOffset = 2050;
#if USE_BOARD3D
int fSync = -1;	/* Not set */
int fResetSync = FALSE;	/* May need to wait for main window */
#endif

skilltype TutorSkill = SKILL_DOUBTFUL;
int nTutorSkillCurrent = 0;

char *szCurrentFileName = NULL;
char *szCurrentFolder = NULL;


int fNextTurn = FALSE, fComputing = FALSE;

float rEvalsPerSec = -1.0f;
float    arLuckLevel[] = {
	0.6f, /* LUCK_VERYBAD */
	0.3f, /* LUCK_BAD */
	0, /* LUCK_NONE */
	0.3f, /* LUCK_GOOD */
	0.6f /* LUCK_VERYGOOD */
    }, arSkillLevel[] = {
	0.16f, /* SKILL_VERYBAD */
	0.08f, /* SKILL_BAD */
	0.04f, /* SKILL_DOUBTFUL */
	0,     /* SKILL_NONE */
    };

#if defined (REDUCTION_CODE)
evalcontext ecTD = { FALSE, 0, 0, TRUE, 0.0 };
#else
evalcontext ecTD = { FALSE, 0, FALSE, TRUE, 0.0 };
#endif

/* this is the "normal" movefilter*/
#define MOVEFILTER \
  { { { 0,  8, 0.16f }, {  0, 0, 0 }, { 0, 0, 0    }, {  0, 0, 0 } } , \
    { { 0,  8, 0.16f }, { -1, 0, 0 }, { 0, 0, 0    }, {  0, 0, 0 } } , \
    { { 0,  8, 0.16f }, { -1, 0, 0 }, { 0, 2, 0.04f }, {  0, 0, 0 } }, \
    { { 0,  8, 0.16f }, { -1, 0, 0 }, { 0, 2, 0.04f }, { -1, 0, 0 } } , \
  }

void *rngctxRollout = NULL;

#if defined (REDUCTION_CODE)
rolloutcontext rcRollout =
{
  {
	/* player 0/1 cube decision */
        { TRUE, 0, 0, TRUE, 0.0 },
	{ TRUE, 0, 0, TRUE, 0.0 }
  }, 
  {
	/* player 0/1 chequerplay */
	{ TRUE, 0, 0, TRUE, 0.0 },
	{ TRUE, 0, 0, TRUE, 0.0 }
  }, 

  {
	/* player 0/1 late cube decision */
	{ TRUE, 0, 0, TRUE, 0.0 },
	{ TRUE, 0, 0, TRUE, 0.0 }
  }, 
  {
	/* player 0/1 late chequerplay */
	{ TRUE, 0, 0, TRUE, 0.0 },
	{ TRUE, 0, 0, TRUE, 0.0 } 
  }, 
  /* truncation point cube and chequerplay */
  { TRUE, 0, 0, TRUE, 0.0 },
  { TRUE, 0, 0, TRUE, 0.0 },

  /* move filters */
  { MOVEFILTER, MOVEFILTER },
  { MOVEFILTER, MOVEFILTER },

  FALSE, /* cubeful */
  TRUE, /* variance reduction */
  FALSE, /* initial position */
  TRUE, /* rotate */
  TRUE, /* truncate at BEAROFF2 for cubeless rollouts */
  TRUE, /* truncate at BEAROFF2_OS for cubeless rollouts */
  FALSE, /* late evaluations */
  FALSE,  /* Truncation disabled */
  FALSE,  /* no stop on STD */
  FALSE,  /* no stop on JSD */
  FALSE,  /* no move stop on JSD */
  10, /* truncation */
  1296, /* number of trials */
  5,  /* late evals start here */
  RNG_MERSENNE, /* RNG */
  0,  /* seed */
  144,    /* minimum games  */
  0.1,	  /* stop when std's are under 10% of value */
  144,    /* minimum games  */
  1.96,   /* stop when best has j.s.d. for 95% confidence */
  0

};

/* parameters for `eval' and `hint' */

#define EVALSETUP  { \
  /* evaltype */ \
  EVAL_EVAL, \
  /* evalcontext */ \
  { TRUE, 0, 0, TRUE, 0.0 }, \
  /* rolloutcontext */ \
  { \
    { \
      { FALSE, 0, 0, TRUE, 0.0 }, /* player 0 cube decision */ \
      { FALSE, 0, 0, TRUE, 0.0 } /* player 1 cube decision */ \
    }, \
    { \
      { FALSE, 0, 0, TRUE, 0.0 }, /* player 0 chequerplay */ \
      { FALSE, 0, 0, TRUE, 0.0 } /* player 1 chequerplay */ \
    }, \
    { \
      { FALSE, 0, 0, TRUE, 0.0 }, /* p 0 late cube decision */ \
      { FALSE, 0, 0, TRUE, 0.0 } /* p 1 late cube decision */ \
    }, \
    { \
      { FALSE, 0, 0, TRUE, 0.0 }, /* p 0 late chequerplay */ \
      { FALSE, 0, 0, TRUE, 0.0 } /* p 1 late chequerplay */ \
    }, \
    { FALSE, 0, 0, TRUE, 0.0 }, /* truncate cube decision */ \
    { FALSE, 0, 0, TRUE, 0.0 }, /* truncate chequerplay */ \
    { MOVEFILTER, MOVEFILTER }, \
    { MOVEFILTER, MOVEFILTER }, \
  FALSE, /* cubeful */ \
  TRUE, /* variance reduction */ \
  FALSE, /* initial position */ \
  TRUE, /* rotate */ \
  TRUE, /* truncate at BEAROFF2 for cubeless rollouts */ \
  TRUE, /* truncate at BEAROFF2_OS for cubeless rollouts */ \
  FALSE, /* late evaluations */ \
  TRUE,  /* Truncation enabled */ \
  FALSE,  /* no stop on STD */ \
  FALSE,  /* no stop on JSD */ \
  FALSE,  /* no move stop on JSD */ \
  10, /* truncation */ \
  36, /* number of trials */ \
  5,  /* late evals start here */ \
  RNG_MERSENNE, /* RNG */ \
  0,  /* seed */ \
  144,    /* minimum games  */ \
  0.1,	  /* stop when std's are under 10% of value */ \
  144,    /* minimum games  */ \
  1.96,   /* stop when best has j.s.d. for 95% confidence */ \
  0 \
  } \
} 
#else /* REDUCTION_CODE */

rolloutcontext rcRollout =
{ 
  {
	/* player 0/1 cube decision */
        { TRUE, 0, TRUE, TRUE, 0.0 },
	{ TRUE, 0, TRUE, TRUE, 0.0 }
  }, 
  {
	/* player 0/1 chequerplay */
	{ TRUE, 0, TRUE, TRUE, 0.0 },
	{ TRUE, 0, TRUE, TRUE, 0.0 }
  }, 

  {
	/* player 0/1 late cube decision */
	{ TRUE, 0, TRUE, TRUE, 0.0 },
	{ TRUE, 0, TRUE, TRUE, 0.0 }
  }, 
  {
	/* player 0/1 late chequerplay */
	{ TRUE, 0, TRUE, TRUE, 0.0 },
	{ TRUE, 0, TRUE, TRUE, 0.0 } 
  }, 
  /* truncation point cube and chequerplay */
  { TRUE, 0, TRUE, TRUE, 0.0 },
  { TRUE, 0, TRUE, TRUE, 0.0 },

  /* move filters */
  { MOVEFILTER, MOVEFILTER },
  { MOVEFILTER, MOVEFILTER },

  TRUE, /* cubeful */
  TRUE, /* variance reduction */
  FALSE, /* initial position */
  TRUE, /* rotate */
  TRUE, /* truncate at BEAROFF2 for cubeless rollouts */
  TRUE, /* truncate at BEAROFF2_OS for cubeless rollouts */
  FALSE, /* late evaluations */
  FALSE,  /* Truncation enabled */
  FALSE,  /* no stop on STD */
  FALSE,  /* no stop on JSD */
  FALSE,  /* no move stop on JSD */
  10, /* truncation */
  1296, /* number of trials */
  5,  /* late evals start here */
  RNG_MERSENNE, /* RNG */
  0,  /* seed */
  144,    /* minimum games  */
  0.1,	  /* stop when std's are under 10% of value */
  144,    /* minimum games  */
  1.96,   /* stop when best has j.s.d. for 95% confidence */
  0,      /* nGamesDone */
  0,      /* nSkip */
};

/* parameters for `eval' and `hint' */

#define EVALSETUP  { \
  /* evaltype */ \
  EVAL_EVAL, \
  /* evalcontext */ \
  { TRUE, 0, TRUE, TRUE, 0.0 }, \
  /* rolloutcontext */ \
  { \
    { \
      { FALSE, 0, TRUE, TRUE, 0.0 }, /* player 0 cube decision */ \
      { FALSE, 0, TRUE, TRUE, 0.0 } /* player 1 cube decision */ \
    }, \
    { \
      { FALSE, 0, TRUE, TRUE, 0.0 }, /* player 0 chequerplay */ \
      { FALSE, 0, TRUE, TRUE, 0.0 } /* player 1 chequerplay */ \
    }, \
    { \
      { FALSE, 0, TRUE, TRUE, 0.0 }, /* p 0 late cube decision */ \
      { FALSE, 0, TRUE, TRUE, 0.0 } /* p 1 late cube decision */ \
    }, \
    { \
      { FALSE, 0, TRUE, TRUE, 0.0 }, /* p 0 late chequerplay */ \
      { FALSE, 0, TRUE, TRUE, 0.0 } /* p 1 late chequerplay */ \
    }, \
    { FALSE, 0, TRUE, TRUE, 0.0 }, /* truncate cube decision */ \
    { FALSE, 0, TRUE, TRUE, 0.0 }, /* truncate chequerplay */ \
    { MOVEFILTER, MOVEFILTER }, \
    { MOVEFILTER, MOVEFILTER }, \
  FALSE, /* cubeful */ \
  TRUE, /* variance reduction */ \
  FALSE, /* initial position */ \
  TRUE, /* rotate */ \
  TRUE, /* truncate at BEAROFF2 for cubeless rollouts */ \
  TRUE, /* truncate at BEAROFF2_OS for cubeless rollouts */ \
  FALSE, /* late evaluations */ \
  TRUE,  /* Truncation enabled */ \
  FALSE,  /* no stop on STD */ \
  FALSE,  /* no stop on JSD */ \
  FALSE,  /* no move stop on JSD */ \
  10, /* truncation */ \
  1296, /* number of trials */ \
  5,  /* late evals start here */ \
  RNG_MERSENNE, /* RNG */ \
  0,  /* seed */ \
  144,    /* minimum games  */ \
  0.1,	  /* stop when std's are under 10% of value */ \
  144,    /* minimum games  */ \
  1.96,   /* stop when best has j.s.d. for 95% confidence */ \
  0, \
  0 \
  } \
} 
#endif

evalsetup esEvalChequer = EVALSETUP;
evalsetup esEvalCube = EVALSETUP;
evalsetup esAnalysisChequer = EVALSETUP;
evalsetup esAnalysisCube = EVALSETUP;

movefilter aamfEval[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] = MOVEFILTER;
movefilter aamfAnalysis[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] = MOVEFILTER;

exportsetup exsExport = {
  TRUE, /* include annotations */
  TRUE, /* include analysis */
  TRUE, /* include statistics */
  TRUE, /* include legend */
  TRUE, /* include match information */

  1, /* display board for all moves */
  -1, /* both players */

  5, /* display max 5 moves */
  TRUE, /* show detailed probabilities */
  /* do not show move parameters */
  { FALSE, TRUE },
  /* display all moves */
  { TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE },

  TRUE, /* show detailed prob. for cube decisions */
  { FALSE, TRUE }, /* do not show move parameters */
  /* display all cube decisions */
  { TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE },

  NULL, /* HTML url to pictures */
  HTML_EXPORT_TYPE_GNU,
  NULL,  /* HTML extension */
  HTML_EXPORT_CSS_HEAD, /* write CSS stylesheet in <head> */

  4, /* PNG size */
  4 /* Html size */
};

  
#define DEFAULT_NET_SIZE 128

storedmoves sm; /* sm.ml.amMoves is NULL, sm.anDice is [0,0] */
storedcube  sc; 

player ap[ 2 ] = {
    { "gnubg", PLAYER_GNU, EVALSETUP, EVALSETUP, MOVEFILTER, 0, NULL },
    { "user", PLAYER_HUMAN, EVALSETUP, EVALSETUP, MOVEFILTER, 0, NULL } 
};

/* Usage strings */
static char szDICE[] = N_("<die> <die>"),
    szCOMMAND[] = N_("<command>"),
    szCOMMENT[] = N_("<comment>"),
    szER[] = N_("evaluation|rollout"), 
    szFILENAME[] = N_("<filename>"),
    szKEYVALUE[] = N_("[<key>=<value> ...]"),
    szLENGTH[] = N_("<length>"),
    szLIMIT[] = N_("<limit>"),
    szMILLISECONDS[] = N_("<milliseconds>"),
    szMOVE[] = N_("<from> <to> ..."),
    szFILTER[] = N_ ( "<ply> <num.xjoin to accept (0 = skip)> "
                      "[<num. of extra moves to accept> <tolerance>]"),
    szNAME[] = N_("<name>"),
    szNAMEOPTENV[] = N_("<name> [env]"),
    szLANG[] = N_("system|<language code>"),
    szONOFF[] = N_("on|off"),
    szOPTCOMMAND[] = N_("[command]"),
    szOPTENVFORCE[] = N_("[env [force]]"),
    szOPTDEPTH[] = N_("[depth]"),
    szOPTFILENAME[] = N_("[filename]"),
    szOPTGENERATOROPTSEED[] = N_("[generator] [seed]"),
    szOPTLENGTH[] = N_("[length]"),
    szOPTLIMIT[] = N_("[limit]"),
    szOPTMODULUSOPTSEED[] = N_("[modulus <modulus>|factors <factor> <factor>] "
			       "[seed]"),
    szOPTNAME[] = N_("[name]"),
    szOPTPOSITION[] = N_("[position]"),
    szOPTSEED[] = N_("[seed]"),
    szOPTVALUE[] = N_("[value]"),
    szPLAYER[] = N_("<player>"),
    szPLAYEROPTRATING[] = N_("<player> [rating]"),
    szPLIES[] = N_("<plies>"),
    szPOSITION[] = N_("<position>"),
    szPRIORITY[] = N_("<priority>"),
    szPROMPT[] = N_("<prompt>"),
    szSCORE[] = N_("<score>"),
    szSIZE[] = N_("<size>"),
    szSTEP[] = N_("[game|roll|rolled|marked] [count]"),
    szTRIALS[] = N_("<trials>"),
    szVALUE[] = N_("<value>"),
    szMATCHID[] = N_("<matchid>"),
    szURL[] = N_("<URL>"),
    szMAXERR[] = N_("<fraction>"),
    szMINGAMES[] = N_("<minimum games to rollout>"),
    szFOLDER[] = N_("<folder>"),
#if USE_GTK
	szWARN[] = N_("[<warning>]"),
	szWARNYN[] = N_("<warning> on|off"),
#endif
    szJSDS[] = N_("<joint standard deviations>");

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
}, cExportParameters = {
    /* dummy command used for export parameters */
    NULL, NULL, NULL, NULL, &cExportParameters
}, cExportMovesDisplay = {
    /* dummy command used for export moves to display */
    NULL, NULL, NULL, NULL, &cExportMovesDisplay
}, cExportCubeDisplay = {
    /* dummy command used for player cube to display */
    NULL, NULL, NULL, NULL, &cExportCubeDisplay
}, cRecordName = {
    /* dummy command used for player record names */
    NULL, NULL, NULL, NULL, &cRecordName
}, acAnalyseClear[] = {
    { "game", CommandAnalyseClearGame, 
      N_("Clear analysis for this game"), NULL, NULL },
    { "match", CommandAnalyseClearMatch, 
      N_("Clear analysis for entire match"), NULL, NULL },
    { "session", CommandAnalyseClearMatch, 
      N_("Clear analysis for entire session"), NULL, NULL },
    { "move", CommandAnalyseClearMove, 
      N_("Clear analysis for this move"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnalyse[] = {
    { "clear", NULL, 
      N_("Clear previous analysis"), NULL, acAnalyseClear },
    { "game", CommandAnalyseGame, 
      N_("Compute analysis and annotate current game"),
      NULL, NULL },
    { "match", CommandAnalyseMatch, 
      N_("Compute analysis and annotate every game "
      "in the match"), NULL, NULL },
    { "move", CommandAnalyseMove, 
      N_("Compute analysis and annotate the current "
      "move"), NULL, NULL },
    { "session", CommandAnalyseSession, 
      N_("Compute analysis and annotate every "
      "game in the session"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotateClear[] = {
    { "comment", CommandAnnotateClearComment, 
      N_("Erase commentary about a move"),
      NULL, NULL },
    { "luck", CommandAnnotateClearLuck, 
      N_("Erase annotations for a dice roll"),
      NULL, NULL },
    { "skill", CommandAnnotateClearSkill, 
      N_("Erase skill annotations for a move"),
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotateMove[] = {
    { "comment", CommandAnnotateAddComment, N_("Add commentary about a move"), szCOMMENT, NULL },
    { "bad", CommandAnnotateBad, N_("Mark as bad"), NULL, NULL },
    { "clear", CommandAnnotateClearSkill, 
      N_("Remove annotations"), NULL, NULL },
    { "doubtful", CommandAnnotateDoubtful, N_("Mark as doubtful"), NULL, NULL },
    { "verybad", CommandAnnotateVeryBad, N_("Mark as very bad"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotateRoll[] = {
    { "clear", CommandAnnotateClearLuck, 
      N_("Remove annotations"), NULL, NULL },
    { "lucky", CommandAnnotateLucky, 
      N_("Mark a lucky dice roll"), NULL, NULL },
    { "unlucky", CommandAnnotateUnlucky, N_("Mark an unlucky dice roll"),
      NULL, NULL },
    { "verylucky", CommandAnnotateVeryLucky, N_("Mark a very lucky dice roll"),
      NULL, NULL },
    { "veryunlucky", CommandAnnotateVeryUnlucky, 
      N_("Mark a very unlucky dice roll"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acAnnotate[] = {
    { "clear", NULL, N_("Clear annotation"), NULL, acAnnotateClear },
    { "move", CommandAnnotateMove, N_("Mark a move"), NULL, acAnnotateMove },
    { "roll", NULL, N_("Mark a roll"), NULL, acAnnotateRoll },
    { "cube", CommandAnnotateCube, N_("Mark a cube decision"), 
      NULL, acAnnotateMove },
    { "double", CommandAnnotateDouble, 
      N_("Mark a double"), NULL, acAnnotateMove },
    { "accept", CommandAnnotateAccept, N_("Mark an accept decision"), 
      NULL, acAnnotateMove },
    { "drop", CommandAnnotateDrop, N_("Mark a drop decision"), 
      NULL, acAnnotateMove },
    { "pass", CommandAnnotateDrop, N_("Mark a pass decision"), 
      NULL, acAnnotateMove },
    { "reject", CommandAnnotateReject, N_("Mark a reject decision"), 
      NULL, acAnnotateMove },
    { "resign", CommandAnnotateResign, N_("Mark a resign decision"), 
      NULL, acAnnotateMove },
    { "take", CommandAnnotateAccept, N_("Mark a take decision"), 
      NULL, acAnnotateMove },
    { NULL, NULL, NULL, NULL, NULL }
}, acClear[] = {
  { "hint", CommandClearHint, 
    N_("Clear analysis used for `hint'"), NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }
}, acExportGame[] = {
    { "equityevolution", CommandExportGameEquityEvolution, 
      N_("Exports the equity evolution of the game (for import into a spreadsheet)"), 
      szFILENAME, &cFilename },
    { "gam", CommandExportGameGam, N_("Records a log of the game in .gam "
      "format"), szFILENAME, &cFilename },
    { "html", CommandExportGameHtml,
      N_("Records a log of the game in .html format"), szFILENAME,
      &cFilename },
    { "latex", CommandExportGameLaTeX, N_("Records a log of the game in LaTeX "
      "format"), szFILENAME, &cFilename },
    { "text", CommandExportGameText, N_("Export a log of the game in text format"), 
      szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportMatch[] = {
    { "equityevolution", CommandExportMatchEquityEvolution, 
      N_("Exports the equity evolution of the match (for import into a spreadsheet)"), 
      szFILENAME, &cFilename },
    { "mat", CommandExportMatchMat, N_("Records a log of the match in .mat "
      "format"), szFILENAME, &cFilename },
    { "html", CommandExportMatchHtml,
      N_("Records a log of the match in .html format"), szFILENAME,
      &cFilename },
    { "latex", CommandExportMatchLaTeX, 
      N_("Records a log of the match in LaTeX "
      "format"), szFILENAME, &cFilename },
    { "text", CommandExportMatchText, 
      N_("Records a log of the match in text format"), 
      szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportPosition[] = {
    { "gammonline", CommandExportPositionGammOnLine,
      N_("Save the current position in .html format "
         "(special for GammOnLine)"), 
      szFILENAME, &cFilename },
    { "gol2clipboard", CommandExportPositionGOL2Clipboard,
      N_("Copy the current position in .html format to clipboard"
         "(special for GammOnLine)"), 
      szFILENAME, &cFilename },
    { "ps", CommandExportPositionPS, N_("Save the current position in "
      "PostScript format"), szFILENAME, &cFilename },
    { "pdf", CommandExportPositionPDF, N_("Save the current position in "
      "PDF format"), szFILENAME, &cFilename },
    { "svg", CommandExportPositionSVG, N_("Save the current position in "
      "SVG format"), szFILENAME, &cFilename },
    { "html", CommandExportPositionHtml,
      N_("Save the current position in .html format"), 
      szFILENAME, &cFilename },
#if HAVE_LIBPNG
    { "png", CommandExportPositionPNG, N_("Save the current position in "
      "Portable Network Graphics (PNG) format"), szFILENAME, &cFilename },
#endif /* HAVE_LIBPNG */
    { "pos", CommandExportPositionJF, N_("Save the current position in .pos "
      "format"), szFILENAME, &cFilename },
    { "snowietxt", CommandExportPositionSnowieTxt,
      N_("Save the current position in Snowie .txt format"), 
      szFILENAME, &cFilename },
    { "text", CommandExportPositionText,
      N_("Save the current position in text format"), 
      szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExportSession[] = {
    { "html", CommandExportMatchHtml,
      N_("Records a log of the session in .html format"), szFILENAME,
      &cFilename },
    { "latex", CommandExportMatchLaTeX, N_("Records a log of the session in "
      "LaTeX format"), szFILENAME, &cFilename },
    { "text", CommandExportMatchText,
      N_("Records a log of the session in text format"),
      szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acExport[] = {
    { "game", NULL, N_("Record a log of the game so far to a file"), NULL,
      acExportGame },
    { "htmlimages", CommandExportHTMLImages, N_("Generate images to be used "
      "by the HTML export commands"), szFILENAME, &cFilename },
    { "match", NULL, N_("Record a log of the match so far to a file"), NULL,
      acExportMatch },
    { "position", NULL, N_("Write the current position to a file"), NULL,
      acExportPosition },
    { "session", NULL, N_("Record a log of the session so far to a file"), 
      NULL, acExportSession },
    { NULL, NULL, NULL, NULL, NULL }
}, acFirst[] = {
  { "game", CommandFirstGame,
    N_("Goto first game of the match or session"), NULL, NULL },
  { "move", CommandFirstMove,
    N_("Goto first move of the current game"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acImport[] = {
    { "auto", CommandImportAuto, N_("Import from any known format"),
      szFILENAME, &cFilename },
    { "bkg", CommandImportBKG, N_("Import from Hans Berliner's BKG format"),
      szFILENAME, &cFilename },
    { "mat", CommandImportMat, N_("Import a Jellyfish match"), szFILENAME,
      &cFilename },
    { "gam", CommandImportMat, N_("Import a Jellyfish game"), szFILENAME,
      &cFilename },
    { "oldmoves", CommandImportOldmoves, N_("Import a FIBS oldmoves file"),
      szFILENAME, &cFilename },
    { "empire", CommandImportEmpire, N_("Import a GammonEmpire game file"),
      szFILENAME, &cFilename },
    { "party", CommandImportParty, N_("Import a PartyGammon game file"),
      szFILENAME, &cFilename },
    { "bgroom", CommandImportBGRoom, N_("Import a BackGammonRoom game file"),
      szFILENAME, &cFilename },
    { "pos", CommandImportJF, 
      N_("Import a Jellyfish position file"), szFILENAME,
      &cFilename },
    { "sgg", CommandImportSGG, 
      N_("Import an SGG match"), szFILENAME, &cFilename },
    { "snowietxt", CommandImportSnowieTxt, 
      N_("Import a Snowie .txt match"), szFILENAME, &cFilename },
    { "tmg", CommandImportTMG, 
      N_("Import an TMG match"), szFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acList[] = {
    { "game", CommandListGame, N_("Show the moves made in this game"), NULL,
      NULL },
    { "match", CommandListMatch, 
      N_("Show the games played in this match"), NULL,
      NULL },
    { "session", CommandListMatch, N_("Show the games played in this session"),
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acLoad[] = {
    { "commands", CommandLoadCommands, N_("Read commands from a script file"),
      szFILENAME, &cFilename },
    { "game", CommandLoadGame, N_("Read a saved game from a file"), szFILENAME,
      &cFilename },
    { "match", CommandLoadMatch, 
      N_("Read a saved match from a file"), szFILENAME,
      &cFilename },
    { "position", CommandLoadPosition, 
      N_("Read a saved position from a file"), szFILENAME, &cFilename },
    { "python", CommandLoadPython,
      N_("Load a python script from a file"), szFILENAME, &cFilename },
    { "weights", CommandNotImplemented, 
      N_("Read neural net weights from a file"),
      szOPTFILENAME, &cFilename },
    { NULL, NULL, NULL, NULL, NULL }
}, acNew[] = {
    { "game", CommandNewGame, 
      N_("Start a new game within the current match or session"), NULL, NULL },
    { "match", CommandNewMatch, 
      N_("Play a new match to some number of points"), szOPTLENGTH, NULL },
    { "session", CommandNewSession, N_("Start a new (money) session"), NULL,
      NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acRecordAdd[] = {
    { "game", CommandRecordAddGame,
      N_("Log the game statistics to the player records"), NULL, NULL },
    { "match", CommandRecordAddMatch, 
      N_("Log the match statistics to the player records"), NULL, NULL },
    { "session", CommandRecordAddSession,
      N_("Log the session statistics to the player records"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acRecord[] = {
    { "add", NULL, N_("Enter statistics into the player records"), NULL,
      acRecordAdd },
    { "erase", CommandRecordErase, N_("Remove all statistics from one "
				      "player's record"), szNAME, NULL },
    { "eraseall", CommandRecordEraseAll,
      N_("Remove all player record statistics"), NULL, NULL },
    { "show", CommandRecordShow, N_("View the player records"), szOPTNAME,
      &cRecordName },
    { NULL, NULL, NULL, NULL, NULL }    
}, acRelationalAdd[] = {
    { "match", CommandRelationalAddMatch,
      N_("Log the match to the external relational database"), 
      szOPTENVFORCE, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acRelationalErase[] = {
    { "player", CommandRelationalErase, N_("Remove all statistics from one player "
			"in the relational database"), szNAMEOPTENV, NULL },
    { "allplayers", CommandRelationalEraseAll,
      N_("Remove all player statistics in the relational database"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acRelationalShow[] = {
    { "details", CommandRelationalShowDetails, 
      N_("Show details of the matches for a given player in the database"), 
      szNAMEOPTENV, NULL },
    { "players", CommandRelationalShowPlayers, 
      N_("Show a list of all the players in the database"), 
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acRelational[] = {
    { "add", NULL, N_("Add to the external relational database"), NULL,
      acRelationalAdd },
    { "erase", NULL, N_("Remove from the external relational database"), NULL,
      acRelationalErase },
    { "help", CommandRelationalHelp, 
      N_("Help and instructions for using and setting up "
         "the external relational database"), NULL, NULL },
    { "select", CommandRelationalSelect, N_("Query the relational database"),
      szCOMMAND, NULL },
    { "show", NULL, N_("Show information from the relational database"),
      NULL, acRelationalShow },
    { "test", CommandRelationalTest, 
      N_("Test the external relational database"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acSave[] = {
    { "game", CommandSaveGame, N_("Record a log of the game so far to a "
      "file"), szFILENAME, &cFilename },
    { "match", CommandSaveMatch, 
      N_("Record a log of the match so far to a file"),
      szFILENAME, &cFilename },
    { "position", CommandSavePosition, N_("Record the current board position "
      "to a file"), szFILENAME, &cFilename },
    { "settings", CommandSaveSettings, N_("Use the current settings in future "
      "sessions"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetAnalysisPlayer[] = {
    { "analyse", CommandSetAnalysisPlayerAnalyse, 
      N_("Specify if this player is to be analysed"), szONOFF, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetAnalysisThreshold[] = {
    { "bad", CommandSetAnalysisThresholdBad, 
      N_("Specify the equity loss for a bad move"), szVALUE, NULL },
    { "doubtful", CommandSetAnalysisThresholdDoubtful, 
      N_("Specify the equity loss for a doubtful move"), szVALUE, NULL },
    { "lucky", CommandSetAnalysisThresholdLucky, 
      N_("Specify the equity gain for "
      "a lucky roll"), szVALUE, NULL },
    { "unlucky", CommandSetAnalysisThresholdUnlucky, 
      N_("Specify the equity loss "
      "for an unlucky roll"), szVALUE, NULL },
    { "verybad", CommandSetAnalysisThresholdVeryBad, 
      N_("Specify the equity loss "
      "for a very bad move"), szVALUE, NULL },
    { "verylucky", CommandSetAnalysisThresholdVeryLucky, 
      N_("Specify the equity "
      "gain for a very lucky roll"), szVALUE, NULL },
    { "veryunlucky", CommandSetAnalysisThresholdVeryUnlucky, N_("Specify the "
      "equity loss for a very unlucky roll"), szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetEvalParam[] = {
  { "type", CommandSetEvalParamType,
    N_("Specify type (evaluation or rollout)"), szER, &cER },
  { "evaluation", CommandSetEvalParamEvaluation,
    N_("Specify parameters for neural net evaluation"), NULL,
    acSetEvaluation },
  { "rollout", CommandSetEvalParamRollout,
    N_("Specify parameters for rollout"), NULL,
    acSetRollout },
  { NULL, NULL, NULL, NULL, NULL }
}, acSetAnalysis[] = {
    { "chequerplay", CommandSetAnalysisChequerplay, N_("Specify parameters "
      "for the analysis of chequerplay"), NULL, acSetEvalParam },
    { "cube", CommandSetAnalysisCube, N_("Select whether cube action will be "
      "analysed"), szONOFF, &cOnOff },
    { "cubedecision", CommandSetAnalysisCubedecision, N_("Specify parameters "
      "for the analysis of cube decisions"), NULL,
      acSetEvalParam },
    { "limit", CommandSetAnalysisLimit, N_("Specify the maximum number of "
      "possible moves analysed"), szOPTLIMIT, NULL },
    { "luck", CommandSetAnalysisLuck, N_("Select whether dice rolls will be "
      "analysed"), szONOFF, &cOnOff },
    { "luckanalysis", CommandSetAnalysisLuckAnalysis,
      N_("Specify parameters for the luck analysis"), NULL, acSetEvaluation },
    { "movefilter", CommandSetAnalysisMoveFilter, 
      N_("Set parameters for choosing moves to evaluate"), 
      szFILTER, NULL},
    { "moves", CommandSetAnalysisMoves, 
      N_("Select whether chequer play will be "
      "analysed"), szONOFF, &cOnOff },
    { "player", CommandSetAnalysisPlayer,
      N_("Player specific options"), szPLAYER, acSetAnalysisPlayer },
    { "threshold", NULL, N_("Specify levels for marking moves"), NULL,
      acSetAnalysisThreshold },
#if USE_GTK
    { "window", CommandSetAnalysisWindows, N_("Display window with analysis"),
      szONOFF, &cOnOff },
#endif
    { NULL, NULL, NULL, NULL, NULL }    
}, acSetAutomatic[] = {
    { "bearoff", CommandSetAutoBearoff, N_("Automatically bear off as many "
      "chequers as possible"), szONOFF, &cOnOff },
    { "crawford", CommandSetAutoCrawford, N_("Enable the Crawford game "
      "based on match score"), szONOFF, &cOnOff },
    { "doubles", CommandSetAutoDoubles, N_("Control automatic doubles "
      "during (money) session play"), szLIMIT, NULL },
    { "game", CommandSetAutoGame, N_("Select whether to start new games "
      "after wins"), szONOFF, &cOnOff },
    { "move", CommandSetAutoMove, N_("Select whether forced moves will be "
      "made automatically"), szONOFF, &cOnOff },
    { "roll", CommandSetAutoRoll, N_("Control whether dice will be rolled "
      "automatically"), szONOFF, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetConfirm[] = {
    { "new", CommandSetConfirmNew, N_("Ask for confirmation before aborting "
      "a game in progress"), szONOFF, &cOnOff },
    { "save", CommandSetConfirmSave, N_("Ask for confirmation before "
      "overwriting existing files"), szONOFF, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetCube[] = {
    { "center", CommandSetCubeCentre, N_("The U.S.A. spelling of `centre'"),
      NULL, NULL },
    { "centre", CommandSetCubeCentre, N_("Allow both players access to the "
      "cube"), NULL, NULL },
    { "owner", CommandSetCubeOwner, N_("Allow only one player to double"),
      szPLAYER, &cPlayerBoth },
    { "use", CommandSetCubeUse, 
      N_("Control use of the doubling cube"), szONOFF, &cOnOff },
    { "value", CommandSetCubeValue, 
      N_("Fix what the cube stake has been set to"),
      szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetCubeEfficiencyRace[] = {
    { "factor", CommandSetCubeEfficiencyRaceFactor, 
      N_("Set cube efficiency race factor"),
      szVALUE, NULL },
    { "coefficient", CommandSetCubeEfficiencyRaceCoefficient, 
      N_("Set cube efficiency race coefficient"),
      szVALUE, NULL },
    { "min", CommandSetCubeEfficiencyRaceMin, 
      N_("Set cube efficiency race minimum value"),
      szVALUE, NULL },
    { "max", CommandSetCubeEfficiencyRaceMax, 
      N_("Set cube efficiency race maximum value"),
      szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetCubeEfficiency[] = {
    { "os", CommandSetCubeEfficiencyOS, 
      N_("Set cube efficiency for one sided bearoff positions"),
      szVALUE, NULL },
    { "crashed", CommandSetCubeEfficiencyCrashed, 
      N_("Set cube efficiency for crashed positions"),
      szVALUE, NULL },
    { "contact", CommandSetCubeEfficiencyContact, 
      N_("Set cube efficiency for contact positions"),
      szVALUE, NULL },
    { "race", NULL, 
      N_("Set cube efficiency parameters for race positions"),
      szVALUE, acSetCubeEfficiencyRace },
    { NULL, NULL, NULL, NULL, NULL }
#if USE_GTK
}, acSetGeometryValues[] = {
    { "width", CommandSetGeometryWidth, N_("set width of window"), 
      szVALUE, NULL },
    { "height", CommandSetGeometryHeight, N_("set height of window"),
      szVALUE, NULL },
    { "xpos", CommandSetGeometryPosX, N_("set x-position of window"),
      szVALUE, NULL },
    { "ypos", CommandSetGeometryPosY, N_("set y-position of window"),
      szVALUE, NULL },
    { "max", CommandSetGeometryMax, N_("set maximised state of window"),
      szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetGeometry[] = {
    { "analysis", CommandSetGeometryAnalysis,
      N_("set geometry of analysis window"), NULL, acSetGeometryValues },
    { "command", CommandSetGeometryCommand,
      N_("set geometry of command window"), NULL, acSetGeometryValues },
    { "game", CommandSetGeometryGame,
      N_("set geometry of game-list window"), NULL, acSetGeometryValues },
    { "hint", CommandSetGeometryHint,
      N_("set geometry of game-list window"), NULL, acSetGeometryValues },
    { "main", CommandSetGeometryMain,
      N_("set geometry of main window"), NULL, acSetGeometryValues },
    { "message", CommandSetGeometryMessage,
      N_("set geometry of message window"), NULL, acSetGeometryValues },
    { "theory", CommandSetGeometryTheory,
      N_("set geometry of theory window"), NULL, acSetGeometryValues },
    { NULL, NULL, NULL, NULL, NULL }
#endif
}, acSetGUIAnimation[] = {
    { "blink", CommandSetGUIAnimationBlink,
      N_("Blink chequers being moves"), NULL, NULL },
    { "none", CommandSetGUIAnimationNone,
      N_("Do not animate moving chequers"), NULL, NULL },
    { "slide", CommandSetGUIAnimationSlide,
      N_("Slide chequers across board when moved"), NULL, NULL },
    { "speed", CommandSetGUIAnimationSpeed,
      N_("Specify animation rate for moving chequers"), szVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetGUI[] = {
    { "animation", NULL, N_("Control how moving chequers are displayed"), NULL,
      acSetGUIAnimation },
    { "beep", CommandSetGUIBeep, N_("Enable beeping on illegal input"),
      szONOFF, NULL },
    { "dicearea", CommandSetGUIDiceArea,
      N_("Show dice icon when human player on roll"), szONOFF, NULL },
#if USE_GTK
    { "dragtargethelp", CommandSetGUIDragTargetHelp,
      N_("Show target help while dragging a chequer"), szONOFF, NULL },
    { "usestatspanel", CommandSetGUIUseStatsPanel,
      N_("Show statistics in a panel"), szONOFF, NULL },
    { "movelistdetail", CommandSetGUIMoveListDetail,
      N_("Show win/loss stats in move list"), szONOFF, NULL },
#endif
    { "highdiefirst", CommandSetGUIHighDieFirst,
      N_("Show the higher die on the left"), szONOFF, NULL },
    { "illegal", CommandSetGUIIllegal,
      N_("Permit dragging chequers to illegal points"), szONOFF, NULL },
    { "showepcs", CommandSetGUIShowEPCs,
      N_("Show the effective pip counts (EPCs) below the board"), 
      szONOFF, NULL },
    { "showids", CommandSetGUIShowIDs,
      N_("Show the position and match IDs above the board"), szONOFF, NULL },
    { "showpips", CommandSetGUIShowPips,
      N_("Show the pip counts below the board"), szONOFF, NULL },
    { "windowpositions", CommandSetGUIWindowPositions,
      N_("Save and restore window positions and sizes"), szONOFF, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetMatchInfo[] = {
    { "annotator", CommandSetMatchAnnotator,
      N_("Record the name of the match commentator"), szOPTNAME, NULL },
    { "comment", CommandSetMatchComment,
      N_("Record miscellaneous notes about the match"), szOPTVALUE, NULL },
    { "date", CommandSetMatchDate,
      N_("Record the date when the match was played"), szOPTVALUE, NULL },
    { "event", CommandSetMatchEvent,
      N_("Record the name of the event the match is from"), szOPTVALUE, NULL },
    { "place", CommandSetMatchPlace,
      N_("Record the location where the match was played"), szOPTVALUE, NULL },
    { "rating", CommandSetMatchRating,
      N_("Record the ratings of the players"), szPLAYEROPTRATING, &cPlayer },
    { "round", CommandSetMatchRound,
      N_("Record the round of the match within the event"), szOPTVALUE, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetOutput[] = {
    { "matchpc", CommandSetOutputMatchPC,
      N_("Show match equities as percentages (on) or probabilities (off)"),
      szONOFF, &cOnOff },
    { "mwc", CommandSetOutputMWC, N_("Show output in MWC (on) or "
      "equity (off) (match play only)"), szONOFF, &cOnOff },
    { "rawboard", CommandSetOutputRawboard, N_("Give FIBS \"boardstyle 3\" "
      "output (on), or an ASCII board (off)"), szONOFF, &cOnOff },
    { "winpc", CommandSetOutputWinPC,
      N_("Show winning chances as percentages (on) or probabilities (off)"),
      szONOFF, &cOnOff },
    { "digits", CommandSetOutputDigits,
      N_("Set number of digits after the decimal point in outputs"),
      szVALUE, NULL},
    { "errorratefactor", CommandSetOutputErrorRateFactor,
      N_("The factor used for multiplying error rates"), szVALUE, NULL},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRNG[] = {
    { "ansi", CommandSetRNGAnsi, N_("Use the ANSI C rand() (usually linear "
      "congruential) generator"), szOPTSEED, NULL },
    { "bbs", CommandSetRNGBBS, N_("Use the Blum, Blum and Shub generator"),
      szOPTMODULUSOPTSEED, NULL },
    { "bsd", CommandSetRNGBsd, N_("Use the BSD random() non-linear additive "
      "feedback generator"), szOPTSEED, NULL },
    { "file", CommandSetRNGFile, 
      N_("Read dice from file"), szFILENAME, NULL },
    { "isaac", CommandSetRNGIsaac, N_("Use the I.S.A.A.C. generator"), 
      szOPTSEED, NULL },
    { "manual", CommandSetRNGManual, 
      N_("Enter all dice rolls manually"), NULL, NULL },
    { "md5", CommandSetRNGMD5, N_("Use the MD5 generator"), szOPTSEED, NULL },
    { "mersenne", CommandSetRNGMersenne, 
      N_("Use the Mersenne Twister generator"),
      szOPTSEED, NULL },
    { "random.org", CommandSetRNGRandomDotOrg, 
      N_("Use random numbers fetched from <www.random.org>"),
      NULL, NULL },
    { "user", CommandSetRNGUser, 
      N_("Specify an external generator"), szOPTGENERATOROPTSEED, NULL,},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutLatePlayer[] = {
    { "chequerplay", CommandSetRolloutPlayerLateChequerplay, 
      N_("Specify parameters "
         "for chequerplay during later plies of rollouts"), 
      NULL, acSetEvaluation },
    { "cubedecision", CommandSetRolloutPlayerLateCubedecision,
      N_("Specify parameters "
         "for cube decisions during later plies of rollouts"),
      NULL, acSetEvaluation },
    { "movefilter", CommandSetRolloutPlayerLateMoveFilter, 
      N_("Set parameters for choosing moves to evaluate"), 
      szFILTER, NULL},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutLimit[] = {
    { "enable", CommandSetRolloutLimitEnable,
      N_("Stop rollouts when STD's are small enough"),
      szONOFF, &cOnOff },
    { "minimumgames", CommandSetRolloutLimitMinGames, 
      N_("Always rollout at least this many games"),
      szMINGAMES, NULL},
    { "maxerror", CommandSetRolloutMaxError,
      N_("Stop when all ratios |std/value| are less than this "),
      szMAXERR, NULL},
    { NULL, NULL, NULL, NULL, NULL }
},acSetRolloutJsd[] = {
  { "limit", CommandSetRolloutJsdLimit, 
    N_("Stop when equities differ by this many j.s.d.s"),
    szJSDS, NULL},
  { "minimumgames", CommandSetRolloutJsdMinGames,
      N_("Always rollout at least this many games"),
      szMINGAMES, NULL},
  { "move", CommandSetRolloutJsdMoveEnable,
    N_("Stop rollout of move when J.S.D. is large enough"),
    szONOFF, &cOnOff },
  { "stop", CommandSetRolloutJsdEnable,
    N_("Stop entire rollout when J.S.D.s are large enough"),
    szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutPlayer[] = {
    { "chequerplay", CommandSetRolloutPlayerChequerplay, 
      N_("Specify parameters "
      "for chequerplay during rollouts"), NULL, acSetEvaluation },
    { "cubedecision", CommandSetRolloutPlayerCubedecision,
      N_("Specify parameters for cube decisions during rollouts"),
      NULL, acSetEvaluation },
    { "movefilter", CommandSetRolloutPlayerMoveFilter, 
      N_("Set parameters for choosing moves to evaluate"), 
      szFILTER, NULL},
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutBearoffTruncation[] = {
    { "exact", CommandSetRolloutBearoffTruncationExact, 
      N_("Truncate *cubeless* rollouts at exact bearoff database"),
      NULL, &cOnOff },
    { "onesided", CommandSetRolloutBearoffTruncationOS, 
      N_("Truncate *cubeless* rollouts when reaching "
         "one-sided bearoff database"),
      NULL, &cOnOff },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetRolloutLate[] = {
  { "chequerplay", CommandSetRolloutLateChequerplay, 
    N_("Specify parameters for chequerplay during later plies of rollouts"), 
    NULL, acSetEvaluation },
  { "cubedecision", CommandSetRolloutLateCubedecision, 
    N_("Specify parameters "
       "for cube decisions during later plies of rollouts"), 
    NULL, acSetEvaluation },
  { "enable", CommandSetRolloutLateEnable, 
    N_("Enable/Disable different evaluations for later plies of rollouts"), 
    szONOFF, &cOnOff },
  { "plies", CommandSetRolloutLatePlies, 
    N_("Change evaluations for later plies in rollouts"),
    szPLIES, NULL },
  { "movefilter", CommandSetRolloutLateMoveFilter, 
    N_("Set parameters for choosing moves to evaluate"), 
    szFILTER, NULL},
  { "player", CommandSetRolloutLatePlayer, 
    N_("Control eval parameters on later plies for each side individually"), 
    szPLAYER, acSetRolloutLatePlayer }, 
  { NULL, NULL, NULL, NULL, NULL }
}, acSetRollout[] = {
    { "bearofftruncation", NULL, 
      N_("Control truncation of rollout when reaching bearoff databases"),
      NULL, acSetRolloutBearoffTruncation },
    { "chequerplay", CommandSetRolloutChequerplay, N_("Specify parameters "
      "for chequerplay during rollouts"), NULL, acSetEvaluation },
    { "cubedecision", CommandSetRolloutCubedecision, N_("Specify parameters "
      "for cube decisions during rollouts"), NULL, acSetEvaluation },
	{ "cube-equal-chequer", CommandSetRolloutCubeEqualChequer,
	  N_("Use same rollout evaluations for cube and chequer play"),
      szONOFF, &cOnOff },
    { "cubeful", CommandSetRolloutCubeful, N_("Specify whether the "
      "rollout is cubeful or cubeless"), szONOFF, &cOnOff },
    { "initial", CommandSetRolloutInitial, 
      N_("Roll out as the initial position of a game"), szONOFF, &cOnOff },
    { "jsd", CommandSetRolloutJsd, 
      N_("Stop truncations based on j.s.d. of equities"),
      NULL, acSetRolloutJsd},
    {"later", CommandSetRolloutLate,
     N_("Control evaluation parameters for later plies of rollout"),
     NULL, acSetRolloutLate },
    {"limit", CommandSetRolloutLimit,
     N_("Stop rollouts based on Standard Deviations"),
     NULL, acSetRolloutLimit },
    {"log", CommandSetRolloutLogEnable,
     N_("Enable recording of rolled out games"),
     szONOFF, &cOnOff },
    {"logfile", CommandSetRolloutLogFile,
     N_("Set template file name for rollout .sgf files"),
     szFILENAME, NULL },
    { "movefilter", CommandSetRolloutMoveFilter, 
      N_("Set parameters for choosing moves to evaluate"), 
      szFILTER, NULL},
    { "player", CommandSetRolloutPlayer, 
      N_("Control evaluation parameters for each side individually"), 
      szPLAYER, acSetRolloutPlayer }, 
    { "players-are-same", CommandSetRolloutPlayersAreSame,
      N_("Use same settings for both players in rollouts"), szONOFF, &cOnOff },
    { "quasirandom", CommandSetRolloutRotate, 
      N_("Permute the dice rolls according to a uniform distribution"),
      szONOFF, &cOnOff },
    { "rng", CommandSetRolloutRNG, N_("Specify the random number "
      "generator algorithm for rollouts"), NULL, acSetRNG },
    { "rotate", CommandSetRolloutRotate, 
      N_("Synonym for `quasirandom'"), szONOFF, &cOnOff },
    { "seed", CommandSetRolloutSeed, N_("Specify the base pseudo-random seed "
      "to use for rollouts"), szOPTSEED, NULL },
    { "trials", CommandSetRolloutTrials, N_("Control how many rollouts to "
      "perform"), szTRIALS, NULL },
	{ "truncation", CommandSetRolloutTruncation, N_("Set parameters for "
      "truncating rollouts"), NULL, acSetTruncation },
    { "truncate-equal-player0", CommandSetRolloutTruncationEqualPlayer0,
      N_("Use player 0 settings for rollout evaluation at truncation point"),
      szONOFF, &cOnOff },
    { "varredn", CommandSetRolloutVarRedn, N_("Use lookahead during rollouts "
      "to reduce variance"), szONOFF, &cOnOff },
    /* FIXME add commands for cube variance reduction, settlements... */
    { NULL, NULL, NULL, NULL, NULL }
}, acSetTruncation[] = {
    { "chequerplay", CommandSetRolloutTruncationChequer,
	  N_("Set chequerplay evaluation when truncating a rollout"), NULL,
	  acSetEvaluation},
    { "cubedecision", CommandSetRolloutTruncationCube,
	  N_("Set cubedecision evaluation when truncating a rollout"), NULL,
	  acSetEvaluation},
    { "enable", CommandSetRolloutTruncationEnable, 
	N_("Enable/Disable truncated rollouts"), szONOFF, &cOnOff },
    { "plies", CommandSetRolloutTruncationPlies,
	N_("End rollouts at a particular depth"), szPLIES, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acSetEval[] = {
  { "chequerplay", CommandSetEvalChequerplay,
    N_("Set evaluation parameters for chequer play"), NULL,
    acSetEvalParam },
  { "cubedecision", CommandSetEvalCubedecision,
    N_("Set evaluation parameters for cube decisions"), NULL,
    acSetEvalParam },
  { "movefilter", CommandSetEvalMoveFilter, 
    N_("Set parameters for choosing moves to evaluate"), 
    szFILTER, NULL},
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportParameters[] = {
  { "evaluation", CommandSetExportParametersEvaluation,
    N_("show detailed parameters for evaluations"), szONOFF, &cOnOff },
  { "rollout", CommandSetExportParametersRollout,
    N_("show detailed parameters for rollouts"), szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportHTMLType[] = {
  { "bbs", CommandSetExportHTMLTypeBBS,
    N_("Use BBS-type images for posting to, e.g., GammOnLine"), NULL, NULL },
  { "fibs2html", CommandSetExportHTMLTypeFibs2html,
    N_("Use fibs2html-type pictures"), NULL, NULL },
  { "gnu", CommandSetExportHTMLTypeGNU,
    N_("Default images"), NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportCSS[] = {
  { "head", CommandSetExportHTMLCSSHead,
    N_("Write CSS stylesheet in <head>"), NULL, NULL },
  { "external", CommandSetExportHTMLCSSExternal,
    N_("Write stylesheet to external file (\"gnubg.css\")"), NULL, NULL },
  { "inline", CommandSetExportHTMLCSSInline,
    N_("Write stylesheet inside tags"), NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportHTML[] = {
  { "css", NULL,
    N_("Control how the CSS stylesheet is written"), szVALUE, acSetExportCSS },
  { "pictureurl", CommandSetExportHTMLPictureURL,
    N_("set URL to pictures used in HTML export"), szURL, NULL },
  { "type", NULL,
    N_("set type of HTML boards used in HTML export"), NULL, 
    acSetExportHTMLType },
  { "size", CommandSetExportHtmlSize, 
      N_("Set size of board for Html image export"), 
      szVALUE, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportMovesDisplay[] = {
  { "verybad", CommandSetExportMovesDisplayVeryBad,
    N_("show very bad moves"), szONOFF, &cOnOff },
  { "bad", CommandSetExportMovesDisplayBad,
    N_("show bad moves"), szONOFF, &cOnOff },
  { "doubtful", CommandSetExportMovesDisplayDoubtful,
    N_("show doubtful moves"), szONOFF, &cOnOff },
  { "unmarked", CommandSetExportMovesDisplayUnmarked,
    N_("show unmarked moves"), szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportCubeDisplay[] = {
  { "actual", CommandSetExportCubeDisplayActual,
    N_("show actual cube decisions"), szONOFF, &cOnOff },
  { "bad", CommandSetExportCubeDisplayBad,
    N_("show bad cube decisions"), szONOFF, &cOnOff },
  { "close", CommandSetExportCubeDisplayClose,
    N_("show close cube decisions"), szONOFF, &cOnOff },
  { "doubtful", CommandSetExportCubeDisplayDoubtful,
    N_("show doubtful cube decisions"), szONOFF, &cOnOff },
  { "missed", CommandSetExportCubeDisplayMissed,
    N_("show missed doubles"), szONOFF, &cOnOff },
  { "unmarked", CommandSetExportCubeDisplayUnmarked,
    N_("show unmarked cube decisions"), szONOFF, &cOnOff },
  { "verybad", CommandSetExportCubeDisplayVeryBad,
    N_("show very bad cube decisions"), szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportInclude[] = {
  { "annotations", CommandSetExportIncludeAnnotations,
    N_("include annotations"), szONOFF, &cOnOff },
  { "analysis", CommandSetExportIncludeAnalysis,
    N_("include analysis (evaluations/rollouts)"), szONOFF, &cOnOff },
  { "statistics", CommandSetExportIncludeStatistics,
    N_("include statistics (# of bad moves, # of jokers, etc)"), 
    szONOFF, &cOnOff },
  { "legend", CommandSetExportIncludeLegend,
    N_("include a legend that describes the output of the export"), 
    szONOFF, &cOnOff },
  { "matchinfo", CommandSetExportIncludeMatchInfo,
    N_("include information about the match" ),
    szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportShow[] = {
  { "board", CommandSetExportShowBoard,
    N_("show board every [value] move (0 for never)"), szVALUE, NULL },
  { "player", CommandSetExportShowPlayer,
    N_("which player(s) to show"), szPLAYER, &cPlayerBoth },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportMoves[] = {
  { "number", CommandSetExportMovesNumber,
    N_("show at most [value] moves"), szVALUE, NULL },
  { "probabilities", CommandSetExportMovesProb,
    N_("show detailed probabilities"), szONOFF, &cOnOff },
  { "parameters", CommandSetExportMovesParameters,
    N_("show detailed rollout/evaluation parameters"), NULL, 
    acSetExportParameters },
  { "display", NULL, N_("when to show moves"), NULL, acSetExportMovesDisplay },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportCube[] = {
  { "probabilities", CommandSetExportCubeProb,
    N_("show detailed probabilities"), szONOFF, &cOnOff },
  { "parameters", CommandSetExportCubeParameters,
    N_("show detailed rollout/evaluation parameters"), NULL, 
    acSetExportParameters },
  { "display", NULL, N_("when to show moves"), NULL, acSetExportCubeDisplay },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetExportPNG[] = {
    { "size", CommandSetExportPNGSize,
      N_("Set size of board for PNG export"),
      szVALUE, NULL },
  { NULL, NULL, NULL, NULL, NULL }
}, acSetExport[] = {
  { "folder", CommandSetExportFolder, N_("Set default folder "
      "for export"), szFOLDER, &cFilename },
  { "html", NULL,
    N_("Set options for HTML export"), NULL, acSetExportHTML },
  { "include", NULL,
    N_("Control which blocks to include in exports"), 
    NULL, acSetExportInclude },
  { "png", NULL,
    N_("Set options for PNG export"), NULL, acSetExportPNG },
  { "show", NULL,
    N_("Control display of boards/players in exports"), NULL, acSetExportShow },
  { "moves", NULL,
    N_("Control display of moves in exports"), NULL, acSetExportMoves },
  { "cube", NULL,
    N_("Control display of cube in exports"), NULL, acSetExportCube },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetImport[] = {
  { "folder", CommandSetImportFolder, N_("Set default folder "
      "for import"), szFOLDER, &cFilename },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetInvert[] = {
  { "matchequitytable", CommandSetInvertMatchEquityTable,
    N_("invert match equity table"), szONOFF, &cOnOff },
  { "met", CommandSetInvertMatchEquityTable,
    N_("alias for 'set invert matchequitytable'"), szONOFF, &cOnOff },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetPriority[] = {
  { "abovenormal", CommandSetPriorityAboveNormal,
    N_("Set priority to above normal"), NULL, NULL },
  { "belownormal", CommandSetPriorityBelowNormal,
    N_("Set priority to below normal"), NULL, NULL },
  { "highest", CommandSetPriorityHighest, 
    N_("Set priority to highest"), NULL, NULL },
  { "idle", CommandSetPriorityIdle,
    N_("Set priority to idle"), NULL, NULL },
  { "nice", CommandSetPriorityNice, N_("Specify priority numerically"),
    szPRIORITY, NULL },
  { "normal", CommandSetPriorityNormal,
    N_("Set priority to normal"), NULL, NULL },
  { "timecritical", CommandSetPriorityTimeCritical,
    N_("Set priority to time critical"), NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }
}, acSetSGF[] = {
  { "folder", CommandSetSGFFolder, N_("Set default folder "
      "for import"), szFOLDER, &cFilename },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetSoundSystem[] = {
  { "command", CommandSetSoundSystemCommand, 
    N_("Specify external command for playing sounds"), szCOMMAND, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetSoundSound[] = {
  { "agree", CommandSetSoundSoundAgree, 
    N_("Agree"), szOPTFILENAME, &cFilename },
  { "analysisfinished", CommandSetSoundSoundAnalysisFinished, 
    N_("Analysis is finished"), szOPTFILENAME, &cFilename },
  { "botfans", CommandSetSoundSoundBotDance, 
    N_("Bot fans"), szOPTFILENAME, &cFilename },
  { "botwinsgame", CommandSetSoundSoundBotWinGame, 
    N_("Bot wins game"), szOPTFILENAME, &cFilename },
  { "botwinsmatch", CommandSetSoundSoundBotWinMatch, 
    N_("Bot wins match"), szOPTFILENAME, &cFilename },
  { "chequer", CommandSetSoundSoundChequer, 
    N_("Chequer movement"), szOPTFILENAME, &cFilename },
  { "double", CommandSetSoundSoundDouble, 
    N_("Double"), szOPTFILENAME, &cFilename },
  { "drop", CommandSetSoundSoundDrop, 
    N_("Drop"), szOPTFILENAME, &cFilename },
  { "exit", CommandSetSoundSoundExit, 	 
	N_("Exiting of GNU Backgammon"), szOPTFILENAME, &cFilename },
  { "humanfans", CommandSetSoundSoundHumanDance, 
    N_("Human fans"), szOPTFILENAME, &cFilename },
  { "humanwinsgame", CommandSetSoundSoundHumanWinGame, 
    N_("Human wins game"), szOPTFILENAME, &cFilename },
  { "humanwinsmatch", CommandSetSoundSoundHumanWinMatch, 
    N_("Human wins match"), szOPTFILENAME, &cFilename },
  { "move", CommandSetSoundSoundMove, 
    N_("Move"), szOPTFILENAME, &cFilename },
  { "redouble", CommandSetSoundSoundRedouble, 
    N_("Redouble"), szOPTFILENAME, &cFilename },
  { "resign", CommandSetSoundSoundResign, 
    N_("Resign"), szOPTFILENAME, &cFilename },
  { "roll", CommandSetSoundSoundRoll, 
    N_("Roll"), szOPTFILENAME, &cFilename },
  { "start", CommandSetSoundSoundStart, 
    N_("Starting of GNU Backgammon"), szOPTFILENAME, &cFilename },
  { "take", CommandSetSoundSoundTake, 
    N_("Take"), szOPTFILENAME, &cFilename },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetSound[] = {
  { "sound", NULL, 
    N_("Set sound files"), NULL, acSetSoundSound }, 
  { "enable", CommandSetSoundEnable, 
    N_("Select whether sounds should be played or not"), szONOFF, &cOnOff },
  { "system", NULL, 
    N_("Select sound system"), NULL, acSetSoundSystem },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetTutorSkill[] = {
  { "doubtful", CommandSetTutorSkillDoubtful, N_("Warn about `doubtful' play"),
    NULL, NULL },
  { "bad", CommandSetTutorSkillBad, N_("Warn about `bad' play"),
    NULL, NULL },
  { "verybad", CommandSetTutorSkillVeryBad, N_("Warn about `very bad' play"),
    NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetTutor[] = {
  { "mode", CommandSetTutorMode, N_("Give advice on possible errors"),
    szONOFF, &cOnOff },
  { "cube", CommandSetTutorCube, N_("Include cube play in advice"),
    szONOFF, &cOnOff },
  { "chequer", CommandSetTutorChequer, 
    N_("Include chequer play in advice"),
    szONOFF, &cOnOff },
  { "eval", CommandSetTutorEval, 
    N_("Use Analysis settings for Tutor"), szONOFF, &cOnOff },
  { "skill", NULL, N_("Set level for tutor warnings"), 
    NULL, acSetTutorSkill },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetCheat[] = {
  { "enable", CommandSetCheatEnable,
   N_("Control whether GNU Backgammon is allowed to manipulate the dice"), 
    szONOFF, &cOnOff },
  { "player", CommandSetCheatPlayer,
   N_("Parameters for the dice manipulation"), szPLAYER, acSetCheatPlayer },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetCheatPlayer[] = {
  { "roll", CommandSetCheatPlayerRoll,
   N_("Which roll should GNU Backgammon choose (1=Best and 21=Worst)"), 
    szVALUE, NULL },
  { NULL, NULL, NULL, NULL, NULL }    
}, acSetVariation[] = {
  { "1-chequer-hypergammon", CommandSetVariation1ChequerHypergammon,
    N_("Play 1-chequer hypergammon"), NULL, NULL },
  { "2-chequer-hypergammon", CommandSetVariation2ChequerHypergammon,
    N_("Play 2-chequer hypergammon"), NULL, NULL },
  { "3-chequer-hypergammon", CommandSetVariation3ChequerHypergammon,
    N_("Play 3-chequer hypergammon"), NULL, NULL },
  { "nackgammon", CommandSetVariationNackgammon,
    N_("Play standard backgammon with Nackgammon starting position"), 
    NULL, NULL },
  { "standard", CommandSetVariationStandard,
    N_("Play standard backgammon"), 
    NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL }  
}, acSet[] = {
    { "analysis", NULL, N_("Control parameters used when analysing moves"),
      NULL, acSetAnalysis },
#if USE_GTK
    { "annotation", CommandSetAnnotation, N_("Select whether move analysis and "
      "commentary are shown"), szONOFF, &cOnOff },
#endif
    { "appearance", CommandSetAppearance, N_("Modify the look and feel of the "
      "graphical interface"), szKEYVALUE, NULL },
    { "automatic", NULL, N_("Perform certain functions without user input"),
      NULL, acSetAutomatic },
    { "beavers", CommandSetBeavers, 
      N_("Set whether beavers are allowed in money game or not"), 
      szVALUE, NULL },
    { "board", CommandSetBoard, N_("Set up the board in a particular "
      "position. Accepted formats are:\n"
      " set board =2 (sets the board to match the second position in the hint list.)\n"
      " set board simple 3 2 4 0 -3 -5 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 2\n"
      " set board 4PPgASjgc/ABMA (sets the board to match the position id.)\n"
	      ), szPOSITION, NULL },
    { "browser", CommandSetBrowser, 
      N_("Set web browser"), szOPTCOMMAND, NULL },
    { "cache", CommandSetCache, N_("Set the size of the evaluation cache"),
      szSIZE, NULL },
    { "calibration", CommandSetCalibration,
      N_("Specify the evaluation speed to be assumed for time estimates"),
      szOPTVALUE, NULL },
    { "cheat", NULL, 
      N_("Control GNU Backgammon's manipulation of the dice"),
      NULL, acSetCheat },
    { "clockwise", CommandSetClockwise, N_("Control the board orientation"),
      szONOFF, &cOnOff },
    { "colours", CommandSetAppearance, 
      N_("Synonym for `set appearance'"), NULL, NULL },
#if USE_GTK
    { "commandwindow", CommandSetCommandWindow, N_("Display command window"),
      szONOFF, &cOnOff },
#endif
    { "confirm", NULL, N_("Confirmation settings"), NULL, acSetConfirm },
    { "crawford", CommandSetCrawford, 
      N_("Set whether this is the Crawford game"), szONOFF, &cOnOff },
    { "cube", NULL, N_("Set the cube owner and/or value"), NULL, acSetCube },
    { "cubeefficiency", NULL, 
      N_("Set parameters for cube evaluations"), NULL, acSetCubeEfficiency },
    { "delay", CommandSetDelay, N_("Limit the speed at which moves are made"),
      szMILLISECONDS, NULL },
    { "dice", CommandSetDice, N_("Select the roll for the current move"),
      szDICE, NULL },
    { "display", CommandSetDisplay, 
      N_("Select whether the board is updated on the computer's turn"), 
      szONOFF, &cOnOff },
#if USE_GTK
    { "dockpanels", CommandSetDockPanels, N_("Dock or float windows"),
      szONOFF, &cOnOff },
#endif
    { "evaluation", NULL, N_("Control position evaluation "
      "parameters"), NULL, acSetEval },
    { "export", NULL, N_("Set settings for export"), NULL, acSetExport },
    { "fullscreen", CommandSetFullScreen, N_("Change to full screen mode"),
      szONOFF, &cOnOff },
#if USE_GTK
    { "gamelist", CommandSetGameList, N_("Display game window with moves"),
      szONOFF, &cOnOff },
    { "geometry", NULL, N_("Set geometry of windows"), NULL, acSetGeometry },
#endif
    { "gotofirstgame", CommandSetGotoFirstGame, 
      N_("Control whether you want to go to the first or last game "
         "when loading matches or sessions"), NULL, NULL },
#if USE_GTK
    { "gui", NULL, N_("Control parameters for the graphical interface"), NULL,
      acSetGUI },
#endif
    { "import", NULL, N_("Set settings for import"), NULL, acSetImport },
    { "sgf", NULL, N_("Set settings for sgf"), NULL, acSetSGF },
    { "invert", NULL, N_("Invert match equity table"), NULL, acSetInvert },
    { "jacoby", CommandSetJacoby, N_("Set whether to use the Jacoby rule in "
      "money games"), szONOFF, &cOnOff },
    { "lang", CommandSetLang, N_("Set your language preference"),
      szLANG, NULL },
    { "matchequitytable", CommandSetMET,
      N_("Read match equity table from XML file"), szFILENAME, &cFilename },
    { "matchid", CommandSetMatchID, N_("set Match ID"), szMATCHID, NULL },
    { "matchinfo", NULL, N_("Record auxiliary match information"), NULL,
      acSetMatchInfo },
    { "matchlength", CommandSetMatchLength,
      N_("Specify the default length for new matches"), szLENGTH, NULL },
#if USE_GTK
	{ "message", CommandSetMessage, N_("Display window with messages"),
      szONOFF, &cOnOff },
#endif
    { "met", CommandSetMET,
      N_("Synonym for `set matchequitytable'"), szFILENAME, &cFilename },
    { "output", NULL, N_("Modify options for formatting results"), NULL,
      acSetOutput },
#if USE_GTK
    { "panels", CommandSetDisplayPanels, 
      N_("Display game list, annotation and message panels/windows"), 
	 szONOFF, &cOnOff },
#endif
    { "panelwidth", CommandSetPanelWidth, N_("Set the width of the docked panels"),
      szVALUE, NULL },
    { "player", CommandSetPlayer, N_("Change options for one or both "
      "players"), szPLAYER, acSetPlayer },
    { "postcrawford", CommandSetPostCrawford, 
      N_("Set whether this is a post-Crawford game"), szONOFF, &cOnOff },
    { "priority", NULL, N_("Set the priority of gnubg"), NULL, acSetPriority },
    { "prompt", CommandSetPrompt, N_("Customise the prompt gnubg prints when "
      "ready for commands"), szPROMPT, NULL },
    { "ratingoffset", CommandSetRatingOffset,
      N_("Set rating offset used for estimating abs. rating"),
      szVALUE, NULL },
    { "record", CommandSetRecord, N_("Set whether all games in a session are "
      "recorded"), szONOFF, &cOnOff },
    { "rng", CommandSetRNG, N_("Select the random number generator algorithm"), NULL,
      acSetRNG },
    { "rollout", CommandSetRollout, N_("Control rollout parameters"),
      NULL, acSetRollout }, 
    { "score", CommandSetScore, N_("Set the match or session score "),
      szSCORE, NULL },
    { "seed", CommandSetSeed, 
      N_("Set the dice generator seed"), szOPTSEED, NULL },
    { "sound", NULL, 
      N_("Control audio parameters"), NULL, acSetSound },
    { "styledgamelist", CommandSetStyledGameList, N_("Display colours for marked moves in game window"),
      szONOFF, &cOnOff },
#if USE_GTK
    { "theorywindow", CommandSetTheoryWindow, N_("Display game theory in window"),
      szONOFF, &cOnOff },
#endif
#if USE_MULTITHREAD
    { "threads", CommandSetThreads, N_("Set the number of calculation threads"),
      szSIZE, NULL },
#endif
    { "toolbar", CommandSetToolbar, N_("Change if icons and/or text are shown on toolbar"),
      szVALUE, NULL },
    { "turn", CommandSetTurn, N_("Set which player is on roll"), szPLAYER,
      &cPlayer },
    { "tutor", NULL, N_("Control tutor setup"), NULL, acSetTutor }, 
    { "variation", NULL, N_("Set which variation of backgammon is used"), 
      NULL, acSetVariation }, 
    { "vsync3d", CommandSetVsync3d, N_("Turn vertical sync on/off"), szONOFF, &cOnOff },
#if USE_GTK
    { "warning", CommandSetWarning, N_("Turn warnings on or off"), szWARNYN, NULL},
#endif
    { NULL, NULL, NULL, NULL, NULL }
}, acShowStatistics[] = {
    { "game", CommandShowStatisticsGame, 
      N_("Compute statistics for current game"), NULL, NULL },
    { "match", CommandShowStatisticsMatch, 
      N_("Compute statistics for every game in the match"), NULL, NULL },
    { "session", CommandShowStatisticsSession, 
      N_("Compute statistics for every game in the session"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acShowManual[] = {
    { "about", CommandShowManualAbout, N_("Show all about gnu backgammon in a web browser"), 
      NULL, NULL },
    { "web", CommandShowManualWeb, N_("Show manual in a web browser"), 
      NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acShow[] = {
    { "analysis", CommandShowAnalysis, N_("Show parameters used for analysing "
      "moves"), NULL, NULL },
    { "automatic", CommandShowAutomatic, N_("List which functions will be "
      "performed without user input"), NULL, NULL },
    { "beavers", CommandShowBeavers, 
      N_("Show whether beavers are allowed in money game or not"), 
      NULL, NULL },
    { "bearoff", CommandShowBearoff, 
      N_("Lookup data in various bearoff databases "), NULL, NULL },
    { "board", CommandShowBoard, 
      N_("Redisplay the board position"), szOPTPOSITION, NULL },
    { "buildinfo", CommandShowBuildInfo, 
      N_("Display details of this build of gnubg"), NULL, NULL },
    { "browser", CommandShowBrowser, 
      N_("Display the currently used web browser"), NULL, NULL },
    { "cache", CommandShowCache, N_("Display statistics on the evaluation "
      "cache"), NULL, NULL },
    { "calibration", CommandShowCalibration,
      N_("Show the previously recorded evaluation speed"), NULL, NULL },
    { "cheat", CommandShowCheat,
      N_("Show parameters for dice manipulation"), NULL, NULL },
    { "clockwise", CommandShowClockwise, N_("Display the board orientation"),
      NULL, NULL },
    { "commands", CommandShowCommands, N_("List all available commands"),
      NULL, NULL },
    { "confirm", CommandShowConfirm, 
      N_("Show whether confirmation is required before aborting a game"), 
      NULL, NULL },
    { "copying", CommandShowCopying, N_("Conditions for redistributing copies "
      "of GNU Backgammon"), NULL, NULL },
    { "crawford", CommandShowCrawford, 
      N_("See if this is the Crawford game"), NULL, NULL },
    { "credits", CommandShowCredits, 
      N_("Display contributors to gnubg"), NULL, NULL },
    { "cube", CommandShowCube, N_("Display the current cube value and owner"),
      NULL, NULL },
    { "cubeefficiency", CommandShowCubeEfficiency, 
      N_("Show parameters for cube evaluations"), NULL, NULL },
    { "delay", CommandShowDelay, N_("See what the current delay setting is"), 
      NULL, NULL },
    { "dice", CommandShowDice, N_("See what the current dice roll is"), NULL,
      NULL },
    { "display", CommandShowDisplay, 
      N_("Show whether the board will be updated on the computer's turn"), 
      NULL, NULL },
    { "engine", CommandShowEngine, N_("Display the status of the evaluation "
      "engine"), NULL, NULL },
    { "evaluation", CommandShowEvaluation, N_("Display evaluation settings "
      "and statistics"), NULL, NULL },
    { "fullboard", CommandShowFullBoard, 
      N_("Redisplay the board position"), szOPTPOSITION, NULL },
    { "gammonvalues", CommandShowGammonValues, N_("Show gammon values"),
      NULL, NULL },
    { "export", CommandShowExport, N_("Show current export settings"), 
      NULL, NULL },
#if USE_GTK
    { "geometry", CommandShowGeometry, N_("Show geometry settings"), 
      NULL, NULL },
#endif
    { "jacoby", CommandShowJacoby, 
      N_("See if the Jacoby rule is used in money sessions"), NULL, NULL },
    { "8912", CommandShow8912, N_("Use 8912 rule to predict cube action"),
        szOPTPOSITION, NULL },
    { "keith", CommandShowKeith, N_("Calculate Keith Count for "
      "position"), szOPTPOSITION, NULL },
    { "kleinman", CommandShowKleinman, N_("Calculate Kleinman count for "
      "position"), szOPTPOSITION, NULL },
    { "lang", CommandShowLang, N_("Display your language preference"),
      NULL, NULL },
    { "manual", NULL, N_("Show manual"), NULL, acShowManual },
    { "marketwindow", CommandShowMarketWindow, 
      N_("show market window for doubles"), NULL, NULL },
    { "matchequitytable", CommandShowMatchEquityTable, 
      N_("Show match equity table"), szOPTVALUE, NULL },
    { "matchinfo", CommandShowMatchInfo,
      N_("Display auxiliary match information"), NULL, NULL },
    { "matchlength", CommandShowMatchLength,
      N_("Show default match length"), NULL, NULL },
    { "matchresult", CommandShowMatchResult,
      N_("Show the actual and luck adjusted result for each game "
         "and the entire match"), NULL, NULL },
    { "met", CommandShowMatchEquityTable, 
      N_("Synonym for `show matchequitytable'"), szOPTVALUE, NULL },
    { "onesidedrollout", CommandShowOneSidedRollout, 
      N_("Show misc race theory"), NULL, NULL },
    { "output", CommandShowOutput, N_("Show how results will be formatted"),
      NULL, NULL },
    { "pipcount", CommandShowPipCount, 
      N_("Count the number of pips each player must move to bear off"), 
      szOPTPOSITION, NULL },
    { "player", CommandShowPlayer, N_("View per-player options"), NULL, NULL },
    { "postcrawford", CommandShowCrawford, 
      N_("See if this is post-Crawford play"), NULL, NULL },
    { "prompt", CommandShowPrompt, N_("Show the prompt that will be printed "
      "when ready for commands"), NULL, NULL },
    { "record", CommandRecordShow, N_("View the player records"), szOPTNAME,
      &cRecordName },
    { "rng", CommandShowRNG, N_("Display which random number generator "
      "is being used"), NULL, NULL },
    { "rollout", CommandShowRollout, N_("Display the evaluation settings used "
      "during rollouts"), NULL, NULL },
    { "rolls", CommandShowRolls, N_("Display evaluations for all rolls "),
      szOPTDEPTH, NULL },
    { "score", CommandShowScore, N_("View the match or session score "),
      NULL, NULL },
    { "scoresheet", CommandShowScoreSheet,
      N_("View the score sheet for the match or session"), NULL, NULL },
    { "seed", CommandShowSeed, N_("Show the dice generator seed"), 
      NULL, NULL },
    { "sound", CommandShowSound, N_("Show information abount sounds"), 
      NULL, NULL },
    { "statistics", NULL, N_("Show statistics"), NULL, acShowStatistics },
    { "temperaturemap", CommandShowTemperatureMap, 
      N_("Show temperature map (graphic overview of dice distribution)"), 
      NULL, NULL },
#if USE_MULTITHREAD
    { "threads", CommandShowThreads, N_("Show number of calculation threads"),
	NULL, NULL },
#endif
    { "thorp", CommandShowThorp, N_("Calculate Thorp Count for "
      "position"), szOPTPOSITION, NULL },
    { "turn", CommandShowTurn, 
      N_("Show which player is on roll"), NULL, NULL },
    { "version", CommandShowVersion, 
      N_("Describe this version of GNU Backgammon"), NULL, NULL },
    { "tutor", CommandShowTutor, 
      N_("Give warnings for possible errors in play"), NULL, NULL },
    { "variation", CommandShowVariation, 
      N_("Give information about which variation of backgammon is being played"),
      NULL, NULL },
#if USE_GTK
    { "warning", CommandShowWarning, N_("Show warning settings"), szWARN, NULL},
#endif
    { "warranty", CommandShowWarranty, 
      N_("Various kinds of warranty you do not have"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }    
}, acSwap[] = {
    { "players", CommandSwapPlayers, N_("Swap players"), NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, acTop[] = {
    { "accept", CommandAccept, N_("Accept a cube or resignation"),
      NULL, NULL },
    { "agree", CommandAgree, N_("Agree to a resignation"), NULL, NULL },
    { "analyse", NULL, N_("Run analysis"), NULL, acAnalyse },
    { "analysis", NULL, NULL, NULL, acAnalyse },
    { "analyze", NULL, NULL, NULL, acAnalyse },
    { "annotate", NULL, N_("Record notes about a game"), NULL, acAnnotate },
    { "beaver", CommandRedouble, N_("Synonym for `redouble'"), NULL, NULL },
    { "calibrate", CommandCalibrate,
      N_("Measure evaluation speed, for later time estimates"), szOPTVALUE,
      NULL },
    { "clear", NULL, N_("Clear information"), NULL, acClear },
    { "copy", CommandCopy, N_("Copy current position to clipboard"), 
      NULL, NULL },
    { "decline", CommandDecline, N_("Decline a resignation"), NULL, NULL },
    { "dicerolls", CommandDiceRolls, N_("Generate a list of rolls"), 
      NULL, NULL },
    { "double", CommandDouble, N_("Offer a double"), NULL, NULL },
    { "drop", CommandDrop, N_("Decline an offered double"), NULL, NULL },
    { "eq2mwc", CommandEq2MWC,
      N_("Convert normalised money equity to match winning chance"),
      szVALUE, NULL },
    { "eval", CommandEval, N_("Display evaluation of a position"), 
      szOPTPOSITION, NULL },
    { "exit", CommandQuit, N_("Leave GNU Backgammon"), NULL, NULL },
    { "export", NULL, N_("Write data for use by other programs"), 
      NULL, acExport },
    { "external", CommandExternal, N_("Make moves for an external controller"),
      szFILENAME, &cFilename },
    { "first", NULL, N_("Goto first move or game"),
      NULL, acFirst },
    { "help", CommandHelp, N_("Describe commands"), szOPTCOMMAND, NULL },
    { "hint", CommandHint,  
      N_("Give hints on cube action or best legal moves"), 
      szOPTVALUE, NULL },
#if HAVE_LIBREADLINE
    { "history", CommandHistory, N_("Display current history"), NULL, NULL },
#endif /* HAVE_LIBREADLINE */
    { "invert", NULL, N_("invert match equity tables, etc."), 
      NULL, acSetInvert },
    { "import", NULL, 
      N_("Import matches, games or positions from other programs"),
      NULL, acImport },
    { "list", NULL, N_("Show a list of games or moves"), NULL, acList },
    { "load", NULL, N_("Read data from a file"), NULL, acLoad },
    { "move", CommandMove, N_("Make a backgammon move"), szMOVE, NULL },
    { "mwc2eq", CommandMWC2Eq,
      N_("Convert match winning chance to normalised money equity"),
      szVALUE, NULL },
    { "n", CommandNext, NULL, szSTEP, NULL },
    { "new", NULL, N_("Start a new game, match or session"), NULL, acNew },
    { "next", CommandNext, N_("Step ahead within the game"), szSTEP, NULL },
    { "p", CommandPrevious, NULL, szSTEP, NULL },
    { "pass", CommandDrop, N_("Synonym for `drop'"), NULL, NULL },
    { "play", CommandPlay, N_("Force the computer to move"), NULL, NULL },
    { "previous", CommandPrevious, N_("Step backward within the game"), szSTEP,
      NULL },
    { "quit", CommandQuit, N_("Leave GNU Backgammon"), NULL, NULL },
    { "r", CommandRoll, NULL, NULL, NULL },
    { "redouble", CommandRedouble, N_("Accept the cube one level higher "
      "than it was offered"), NULL, NULL },
    { "reject", CommandReject, N_("Reject a cube or resignation"), 
      NULL, NULL },
    { "record", NULL, N_("Keep statistics on player histories"), NULL,
      acRecord },
    { "relational", NULL, 
      N_("Log matches to an external relational database"), NULL,
      acRelational },
    { "resign", CommandResign, N_("Offer to end the current game"), szVALUE,
      NULL },
    { "roll", CommandRoll, N_("Roll the dice"), NULL, NULL },
    { "rollout", CommandRollout, 
      N_("Have gnubg perform rollouts of a position"),
      szOPTPOSITION, NULL },
    { "save", NULL, N_("Write data to a file"), NULL, acSave },
    { "set", NULL, N_("Modify program parameters"), NULL, acSet },
    { "show", NULL, N_("View program parameters"), NULL, acShow },
    { "swap", NULL, N_("Swap players"), NULL, acSwap },
    { "take", CommandTake, N_("Agree to an offered double"), NULL, NULL },
    { "?", CommandHelp, N_("Describe commands"), szOPTCOMMAND, NULL },
    { NULL, NULL, NULL, NULL, NULL }
}, cTop = { NULL, NULL, NULL, NULL, acTop };

static int iProgressMax, iProgressValue, fInProgress;
static char *pcProgress;
static psighandler shInterruptOld;
char *default_import_folder = NULL;
char *default_export_folder = NULL;
char *default_sgf_folder = NULL;

const char *szHomeDirectory;

char *aszBuildInfo[] = {
#if USE_PYTHON
    N_ ("Python supported."),
#endif
#if USE_GTK
    N_ ("Window system supported."),
#endif
#if HAVE_SOCKETS
    N_("External players supported."),
#endif
#if HAVE_LIBXML2
    N_("XML match equity files supported."),
#endif
#if HAVE_LIBGMP
    N_("Long RNG seeds supported."),
#endif
#if USE_BOARD3D
    N_("3d Boards supported."),
#endif
#if HAVE_SOCKETS
    N_("External commands supported."),
#endif
#ifdef WIN32
    N_("Windows sound system supported."),
#else
#if HAVE_ESD
    N_("ESD sound system supported."),
#endif
#endif
#if USE_MULTITHREAD
    N_("Multiple threads supported."),
#endif
    NULL,
};

extern char *GetBuildInfoString(void)
{
	static char **ppch = aszBuildInfo;

	if (!*ppch)
	{
#if USE_SSE_VECTORIZE
		static int sseShown = 0;
		if (!sseShown)
		{
			sseShown = 1;
			if (SSE_Supported())
				return N_("SSE supported and available.");
			else
				return N_("SSE supported but not available.");
		}
		sseShown = 0;
#endif

		ppch = aszBuildInfo;
		return NULL;
	}
	return *ppch++;
}

/*
 * general token extraction
   input: ppch pointer to pointer to command
          szToekns - string of token separators
   output: NULL if no token found
           ptr to extracted token if found. Token is in original location
               in input string, but null terminated if not quoted, token
               will have been moved forward over quote character when quoted
           ie: 
           input:  '  abcd efgh'
           output  '  abcd\0efgh'
                   return value points to abcd, ppch points to efgh
           input   '  "jklm" nopq'
           output  ;  jklm\0 nopq'
                   return value points to jklm, ppch points to space before 
                   the 'n'
          ppch points past null terminator

   ignores leading whitespace, advances ppch over token and trailing
          whitespace

   matching single or double quotes are allowed, any character outside
   of quotes or in doubly quoated strings can be escaped with a
   backslash and will be taken as literal.  Backslashes within single
   quoted strings are taken literally. Multiple quoted strins can be
   concatentated.  

   For example: input ' abc\"d"e f\"g h i"jk'l m n \" o p q'rst uvwzyz'
   with the terminator list ' \t\r\n\v\f'
   The returned token will be the string
   <abc"de f"g h j ijkl m n \" o p qrst>
   ppch will point to the 'u'
   The \" between c and d is not in a single quoted string, so is reduced to 
   a double quote and is *not* the start of a quoted string.
   The " before the 'd' begins a double quoted string, so spaces and tabs are
   not terminators. The \" between f and g is reduced to a double quote and 
   does not teminate the quoted string. which ends with the double quote 
   between i and j. The \" between n and o is taken as a pair of literal
   characters because they are within the single quoted string beginning
   before l and ending after q.
   It is not possible to put a single quote within a single quoted string. 
   You can have single quotes unescaped withing double quoted strings and
   double quotes unescaped within single quoted strings.
 */
extern char *
NextTokenGeneral( char **ppch, const char *szTokens ) {


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

      if ( **ppch && strchr( szTokens, **ppch ) ) {
        /* this character ends token */
        if( !chQuote ) {
          fEnd = TRUE;
          (*ppch)++; /* step over token */
        }
        else
          *pchSave++ = **ppch;
      }
      else {
	switch( **ppch ) {
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

      }

      if( !fEnd )
        ( *ppch )++;
    }

    while( isspace( **ppch ) )
      ( *ppch )++;

    *pchSave = 0;

#ifndef NDEBUG
    g_assert( pchSave <= pchEnd );
    g_assert( *ppch <= pchEnd );
    g_assert( pch <= pchEnd );
#endif

    return pch;

}

/* extrace a token from a string. Tokens are terminated by tab, newline, 
   carriage return, vertical tab or form feed.
   Input:

     ppch = pointer to pointer to input string. This will be updated
        to point past any token found. If the string is exhausetd, 
        the pointer at ppch will point to the terminating NULL, so it is
        safe to call this function repeatedly after failure
    
   Output:
       null terminated token if found or NULL if no tokens present.
*/
extern char *NextToken( char **ppch )
{

  return NextTokenGeneral( ppch, " \t\n\r\v\f" ); 

}

/* return a count of the number of separate runs of one or more
   non-whitespace characters. This is the number of tokens that
   NextToken() will return. It may not be the number of tokens
   NextTokenGeneral() will return, as it does not count quoted strings
   containing whitespace as single tokens
*/
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

/* extract a token and convert to double. On error or no token, return 
   ERR_VAL (a very large negative double.
*/

extern double ParseReal( char **ppch )
{

    char *pch, *pchOrig;
    double r;
    
    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return ERR_VAL;

    r = g_ascii_strtod( pchOrig, &pch );

    return *pch ? ERR_VAL : r;
}

/* get the next token from the input and convert as an
   integer. Returns INT_MIN on empty input or non-numerics found. Does
   handle negative integers. On failure, one token (if any were available
   will have been consumed, it is not pushed back into the input.
*/
extern int ParseNumber( char **ppch )
{

    char *pch, *pchOrig;

    if( !ppch || !( pchOrig = NextToken( ppch ) ) )
	return INT_MIN;

    for( pch = pchOrig; *pch; pch++ )
	if( !isdigit( *pch ) && *pch != '-' )
	    return INT_MIN;

    return atoi( pchOrig );
}

/* get a player either by name or as player 0 or 1 (indicated by the single
   character '0' or '1'. Returns -1 on no input, 2 if not a recoginsed name
   Note - this is not a token extracting routine, it expects to be handed
   an already extracted token
*/
extern int ParsePlayer( char *sz )
{

    int i;

    if( !sz )
	return -1;
    
    if( ( *sz == '0' || *sz == '1' ) && !sz[ 1 ] )
	return *sz - '0';

    for( i = 0; i < 2; i++ )
	if( !CompareNames( sz, ap[ i ].szName ) )
	    return i;

    if( !StrNCaseCmp( sz, "both", strlen( sz ) ) )
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
extern int ParsePosition( TanBoard an, char **ppch, char *pchDesc )
{

    int i;
    char *pch;
    
    /* FIXME allow more formats, e.g. FIBS "boardstyle 3" */

    if( !ppch || !( pch = NextToken( ppch ) ) ) { 
	memcpy( an, msBoard(), sizeof(TanBoard) );

	if( pchDesc )
	    strcpy( pchDesc, _("Current position") );
	
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
             outputf (_("`simple' must be followed by 26 integers; "
                      "found only %d\n"), i );
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
       
       return CheckPosition((ConstTanBoard)an) ? 0 : -1;
    }

    if( *pch == '=' ) {
	    i = atoi( pch + 1 );
	if( i < 1) {
	    outputl( _("You must specify the number of the move to apply.") );
	    return -1;
	}

        if ( memcmp ( &ms, &sm.ms, sizeof ( matchstate ) ) ) {
            outputl( _("There is no valid move list.") );
            return -1;
	}

	if ((unsigned int)i > sm.ml.cMoves ) {
	    outputf( _("Move =%d is out of range.\n"), i );
	    return -1;
	}

	PositionFromKey( an, sm.ml.amMoves[ i - 1 ].auch );

	if( pchDesc )
	    FormatMove( pchDesc, msBoard(), sm.ml.amMoves[ i - 1 ].anMove );
	
	if( !ms.fMove )
	    SwapSides( an );
	
	return 1;
    }

    if( !PositionFromID( an, pch ) ) {
	outputl( _("Illegal position.") );
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
extern int ParseKeyValue( char **ppch, char *apch[ 2 ] )
{

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
extern int CompareNames( char *sz0, char *sz1 )
{

    static char ach[] = " \t\r\n\f\v_";
    
    for( ; *sz0 || *sz1; sz0++, sz1++ )
	if( toupper( *sz0 ) != toupper( *sz1 ) &&
	    ( !strchr( ach, *sz0 ) || !strchr( ach, *sz1 ) ) )
	    return toupper( *sz0 ) - toupper( *sz1 );
    
    return 0;
}

extern void UpdateSetting( void *p )
{
#if USE_GTK
    if( fX )
	GTKSet( p );
#endif
}

extern void UpdateSettings( void )
{

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.nMatchTo );
    UpdateSetting( &ms.fCrawford );
    UpdateSetting( &ms.gs );
    
    ShowBoard();

#if USE_BOARD3D
	RestrictiveRedraw();
#endif
}

/* handle turning a setting on / off
   inputs: szName - the setting being adjusted
           pf = pointer to current on/off state (will be updated)
           sz = pointer to command line - a token will be extracted, 
                but furhter calls to NextToken will return only the on/off
                value, so you can't have commands in the form
                set something on <more tokens>
           szOn - text to output when turning setting on
           szOff - text to output when turning setting off
    output: -1 on error
             0 setting is now off
             1 setting is now on
    acceptable tokens are on/off yes/no true/false
 */
extern int SetToggle( char *szName, int *pf, char *sz, char *szOn, char *szOff )
{
    char *pch = NextToken( &sz );
    int cch;
    
    if( !pch ) {
	outputf( _("You must specify whether to set %s on or off (see `help set "
		"%s').\n"), szName, szName );

	return -1;
    }

    cch = strlen( pch );
    
    if( !StrCaseCmp( "on", pch ) || !StrNCaseCmp( "yes", pch, cch ) ||
	!StrNCaseCmp( "true", pch, cch ) ) {
	if( *pf != TRUE ) {
	    *pf = TRUE;
	    UpdateSetting( pf );
	}
	
	outputl( szOn );

	return TRUE;
    }

    if( !StrCaseCmp( "off", pch ) || !StrNCaseCmp( "no", pch, cch ) ||
	!StrNCaseCmp( "false", pch, cch ) ) {
	if( *pf != FALSE ) {
	    *pf = FALSE;
	    UpdateSetting( pf );
	}
	
	outputl( szOff );

	return FALSE;
    }

    outputf( _("Illegal keyword `%s' -- try `help set %s'.\n"), pch, szName );

    return -1;
}

extern void PortableSignal( int nSignal, RETSIGTYPE (*p)(int),
			    psighandler *pOld, int fRestart ) {
#if HAVE_SIGACTION
    struct sigaction sa;

    sa.sa_handler = p;
    sigemptyset( &sa.sa_mask );
    sa.sa_flags =
#if SA_RESTART
	( fRestart ? SA_RESTART : 0 ) |
#endif
#if SA_NOCLDSTOP
	SA_NOCLDSTOP |
#endif
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

extern void PortableSignalRestore( int nSignal, psighandler *p )
{
#if HAVE_SIGACTION
    sigaction( nSignal, p, NULL );
#elif HAVE_SIGVEC
    sigvec( nSignal, p, NULL );
#else
    signal( nSignal, *p );
#endif
}

/* Reset the SIGINT handler, on return to the main command loop.  Notify
   the user if processing had been interrupted. */
extern void ResetInterrupt( void )
{
    if( fInterrupt ) {
	{
	outputl( _("(Interrupted)") );
	outputx();
	}
	
	fInterrupt = FALSE;
	
#if USE_GTK
	if( nNextTurn ) {
	    g_source_remove( nNextTurn );
	    nNextTurn = 0;
	}
#endif
    }
}


extern void HandleCommand( char *sz, command *ac )
{

    command *pc;
    char *pch;
    int cch;
    
    if( ac == acTop ) {
	outputnew();
    
        if( *sz == '#' ) /* Comment */
                return;
        else if( *sz == ':' ) {
                return;
        }
        else if ( *sz == '>' ) {

          while ( *sz == '>' )
            ++sz;

          /* leading white space confuses Python :-) */

          while ( isspace( *sz ) )
            ++sz;

#if USE_PYTHON
#if USE_GTK
	    if( fX )
		GTKDisallowStdin();
#endif
          if ( *sz ) {
            PyRun_SimpleString( sz );
          }
          else {
            PyRun_SimpleString( "import sys; print 'Python', sys.version" );
            PyRun_AnyFile( stdin, NULL );
          }
#if USE_GTK
	    if( fX )
		GTKAllowStdin();
#endif
#else
	    outputl( _("This installation of GNU Backgammon was compiled "
		     "without Python support.") );
	    outputx();
#endif
            return;
        }

    }
    
    if( !( pch = NextToken( &sz ) ) ) {
	if( ac != acTop )
	    outputl( _("Incomplete command -- try `help'.") );

	outputx();
	return;
    }

    cch = strlen( pch );

    if( ac == acTop && ( isdigit( *pch ) ||
			 !StrNCaseCmp( pch, "bar/", cch > 4 ? 4 : cch ) ) ) {
	if( pch + cch < sz )
	    pch[ cch ] = ' ';
	
	CommandMove( pch );

	outputx();
	return;
    }

    for( pc = ac; pc->sz; pc++ )
	if( !StrNCaseCmp( pch, pc->sz, cch ) )
	    break;

    if( !pc->sz ) {
	outputf( _("Unknown keyword `%s' -- try `help'.\n"), pch );

	outputx();
	return;
    }

    if( pc->pf ) {
	pc->pf( sz );
	
	outputx();
    } else
	HandleCommand( sz, pc->pc );
}

extern void InitBoard( TanBoard anBoard, const bgvariation bgv )
{

  int i;
  unsigned int j;

  for( i = 0; i < 25; i++ )
    anBoard[ 0 ][ i ] = anBoard[ 1 ][ i ] = 0;

  switch( bgv ) {
  case VARIATION_STANDARD:
  case VARIATION_NACKGAMMON:
    
    anBoard[ 0 ][ 5 ] = anBoard[ 1 ][ 5 ] =
	anBoard[ 0 ][ 12 ] = anBoard[ 1 ][ 12 ] = 
      ( bgv == VARIATION_NACKGAMMON ) ? 4 : 5;
    anBoard[ 0 ][ 7 ] = anBoard[ 1 ][ 7 ] = 3;
    anBoard[ 0 ][ 23 ] = anBoard[ 1 ][ 23 ] = 2;

    if( bgv == VARIATION_NACKGAMMON )
	anBoard[ 0 ][ 22 ] = anBoard[ 1 ][ 22 ] = 2;

    break;

  case VARIATION_HYPERGAMMON_1:
  case VARIATION_HYPERGAMMON_2:
  case VARIATION_HYPERGAMMON_3:

    for ( i = 0; i < 2; ++i )
      for ( j = 0; j < ( bgv - VARIATION_HYPERGAMMON_1 + 1 ); ++j )
        anBoard[ i ][ 23 - j ] = 1;
    
    break;
    
  default:

    g_assert ( FALSE );
    break;

  }

}


extern void GetMatchStateCubeInfo( cubeinfo* pci, const matchstate* pms )
{

    SetCubeInfo( pci, pms->nCube, pms->fCubeOwner, pms->fMove,
			pms->nMatchTo, pms->anScore, pms->fCrawford,
			pms->fJacoby, nBeavers, pms->bgv );
}

static void
DisplayCubeAnalysis( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
		     float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
		     const evalsetup* pes ) {

    cubeinfo ci;

    if( pes->et == EVAL_NONE )
	return;

    GetMatchStateCubeInfo( &ci, &ms );
    
    outputl( OutputCubeAnalysis( aarOutput, aarStdDev, pes, &ci ) );
}

extern char *GetLuckAnalysis( const matchstate *pms, float rLuck )
{

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

    unsigned int i;
    char szBuf[ 1024 ];
    
    switch( pmr->mt ) {
    case MOVE_NORMAL:
        DisplayCubeAnalysis(  pmr->CubeDecPtr->aarOutput,
			      pmr->CubeDecPtr->aarStdDev,
                             &pmr->CubeDecPtr->esDouble );

	outputf( _("Rolled %d%d"), pmr->anDice[ 0 ], pmr->anDice[ 1 ] );

	if( pmr->rLuck != ERR_VAL )
	    outputf( " (%s):\n", GetLuckAnalysis( &ms, pmr->rLuck ) );
	else
	    outputl( ":" );
	
	for( i = 0; i < pmr->ml.cMoves; i++ ) {
	    if( i >= 10 /* FIXME allow user to choose limit */ &&
		i != pmr->n.iMove )
		continue;
	    outputc( i == pmr->n.iMove ? '*' : ' ' );
	    output( FormatMoveHint( szBuf, &ms, &pmr->ml, i,
				    i != pmr->n.iMove ||
				    i != pmr->ml.cMoves - 1, TRUE, TRUE ) );

	}
	
	break;

    case MOVE_DOUBLE:
      DisplayCubeAnalysis(  pmr->CubeDecPtr->aarOutput,
                            pmr->CubeDecPtr->aarStdDev,
                           &pmr->CubeDecPtr->esDouble );
	break;

    case MOVE_TAKE:
    case MOVE_DROP:
	/* FIXME */
	break;
	
    case MOVE_SETDICE:
	if( pmr->rLuck != ERR_VAL )
	    outputf( _("Rolled %d%d (%s):\n"), 
                     pmr->anDice[ 0 ], pmr->anDice[ 1 ], 
                     GetLuckAnalysis( &ms, pmr->rLuck ) );
	break;

    default:
	break;
    }
}

extern void ShowBoard( void )
{
    char szBoard[ 2048 ];
    char sz[ 50 ], szCube[ 50 ], szPlayer0[ MAX_NAME_LEN + 3 ], szPlayer1[ MAX_NAME_LEN + 3 ],
	szScore0[ 50 ], szScore1[ 50 ], szMatch[ 50 ];
    char *apch[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    moverecord *pmr;
	TanBoard an;

	if( cOutputDisabled )
		return;

    if( ms.gs == GAME_NONE ) {
#if USE_GTK
	if( fX ) {
	    TanBoard anBoardTemp;
	    InitBoard( anBoardTemp, ms.bgv );
	    game_set( BOARD( pwBoard ), anBoardTemp, 0, ap[ 1 ].szName,
		      ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		      ms.anScore[ 0 ], 0, 0, FALSE, anChequers[ ms.bgv ] );
	} else
#endif

	    outputl( _("No game in progress.") );
	
	return;
    }

	memcpy( an, msBoard(), sizeof(TanBoard) );
	if( !ms.fMove )
		SwapSides( an );

#if USE_GTK
    if( !fX ) {
#endif
	if( fOutputRawboard )
	{
	    outputl( FIBSBoard( szBoard, an, ms.fMove, ap[ 1 ].szName,
				ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
				ms.anScore[ 0 ], ms.anDice[ 0 ],
				ms.anDice[ 1 ], ms.nCube,
				ms.fCubeOwner, ms.fDoubled, ms.fTurn,
				ms.fCrawford, anChequers[ ms.bgv ] ) );
	    
	    return;
	}
	
	apch[ 0 ] = szPlayer0;
	apch[ 6 ] = szPlayer1;

	sprintf( apch[ 1 ] = szScore0,
		 ngettext( "%d point", "%d points", ms.anScore[ 0 ] ),
		 ms.anScore[ 0 ] );

	sprintf( apch[ 5 ] = szScore1,
		 ngettext( "%d point", "%d points", ms.anScore[ 1 ] ),
		 ms.anScore[ 1 ] );

	if( ms.fDoubled ) {
	    apch[ ms.fTurn ? 4 : 2 ] = szCube;

	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );
	    sprintf( szCube, _("Cube offered at %d"), ms.nCube << 1 );
	} else {
	    sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
	    sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

	    apch[ ms.fMove ? 4 : 2 ] = sz;
	
	    if( ms.anDice[ 0 ] )
		sprintf( sz, 
                         _("Rolled %d%d"), ms.anDice[ 0 ], ms.anDice[ 1 ] );
	    else if( !GameStatus( msBoard(), ms.bgv ) )
		strcpy( sz, _("On roll") );
	    else
		sz[ 0 ] = 0;
	    
	    if( ms.fCubeOwner < 0 ) {
		apch[ 3 ] = szCube;

		if( ms.nMatchTo )
		    sprintf( szCube, 
                             _("%d point match (Cube: %d)"), ms.nMatchTo,
			     ms.nCube );
		else
		    sprintf( szCube, _("(Cube: %d)"), ms.nCube );
	    } else {
		int cch = strlen( ap[ ms.fCubeOwner ].szName );
		
		if( cch > 20 )
		    cch = 20;
		
		sprintf( szCube, _("%c: %*s (Cube: %d)"), ms.fCubeOwner ? 'X' :
			 'O', cch, ap[ ms.fCubeOwner ].szName, ms.nCube );

		apch[ ms.fCubeOwner ? 6 : 0 ] = szCube;

		if( ms.nMatchTo )
		    sprintf( apch[ 3 ] = szMatch, _("%d point match"),
			     ms.nMatchTo );
	    }
	}
	if( ms.fResigned )
	    /* FIXME it's not necessarily the player on roll that resigned */
	    sprintf( strchr( sz, 0 ), _(", resigns %s"),
		     gettext ( aszGameResult[ ms.fResigned - 1 ] ) );
	
	outputl( DrawBoard( szBoard, (ConstTanBoard)an, ms.fMove, apch,
                            MatchIDFromMatchState ( &ms ), 
                            anChequers[ ms.bgv ] ) );

	if (
#if USE_GTK
		PanelShowing(WINDOW_ANALYSIS) &&
#endif
		plLastMove && ( pmr = plLastMove->plNext->p ) ) {
	    DisplayAnalysis( pmr );
	    if( pmr->sz )
		outputl( pmr->sz ); /* FIXME word wrap */
	}
#if USE_GTK
    }
	else
	{
		game_set( BOARD( pwBoard ), an, ms.fMove, ap[ 1 ].szName,
		ap[ 0 ].szName, ms.nMatchTo, ms.anScore[ 1 ],
		ms.anScore[ 0 ], ms.anDice[ 0 ], ms.anDice[ 1 ],
		ap[ ms.fTurn ].pt != PLAYER_HUMAN && !fComputing &&
		!nNextTurn, anChequers[ ms.bgv ] );
	}
#endif    

#ifdef UNDEF
    {
      char *pc;
      printf ( _("MatchID: %s\n"), pc = MatchIDFromMatchState ( &ms ) );
      MatchStateFromID ( &ms, pc );
    }
#endif
}

extern char *FormatPrompt( void )
{

    static char sz[ 128 ]; /* FIXME check for overflow in rest of function */
    char *pch = szPrompt, *pchDest = sz;
    unsigned int anPips[ 2 ];

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
		    strcpy( pchDest, _("No game") );
		else {
		    PipCount( msBoard(), anPips );
		    sprintf( pchDest, "%d:%d", anPips[ 1 ], anPips[ 0 ] );
		}
		break;

	    case 'p':
	    case 'P':
		/* Player on roll */
		switch( ms.gs ) {
		case GAME_NONE:
		    strcpy( pchDest, _("No game") );
		    break;

		case GAME_PLAYING:
		    strcpy( pchDest, ap[ ms.fTurn ].szName );
		    break;

		case GAME_OVER:
		case GAME_RESIGNED:
		case GAME_DROP:
		    strcpy( pchDest, _("Game over") );
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

extern void CommandEval( char *sz )
{

    char szOutput[ 4096 ];
    int n;
	TanBoard an;
    cubeinfo ci;
    
    if( !*sz && ms.gs == GAME_NONE ) {
	outputl( _("No position specified and no game in progress.") );
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
		     ms.nMatchTo, ms.anScore, ms.fCrawford, ms.fJacoby,
		     nBeavers, ms.bgv );    

    ProgressStart( _("Evaluating position...") );
    if( !DumpPosition( (ConstTanBoard)an, szOutput, &esEvalCube.ec, &ci,
                       fOutputMWC, fOutputWinPC, n, 
                       MatchIDFromMatchState( &ms ) ) ) {
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

extern command *FindHelpCommand( command *pcBase, char *sz,
				 char *pchCommand, char *pchUsage ) {

    command *pc;
    const char *pch;
    int cch;
    
    if( !( pch = NextToken( &sz ) ) )
	return pcBase;

    cch = strlen( pch );

    for( pc = pcBase->pc; pc && pc->sz; pc++ )
	if( !StrNCaseCmp( pch, pc->sz, cch ) )
	    break;

    if( !pc || !pc->sz )
	return NULL;

    pch = pc->sz;
    while( *pch )
	*pchCommand++ = *pchUsage++ = *pch++;
    *pchCommand++ = ' '; *pchCommand = 0;
    *pchUsage++ = ' '; *pchUsage = 0;

    if( pc->szUsage ) {
	pch = gettext ( pc->szUsage );
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

extern char* CheckCommand(char *sz, command *ac)
{
	command *pc;
	int cch;
    char *pch = NextToken(&sz);
	if (!pch)
		return 0;

    cch = strlen( pch );
	for (pc = ac; pc->sz; pc++)
		if (!StrNCaseCmp(pch, pc->sz, cch))
			break;
    if (!pc->sz)
		return pch;

    if (pc->pf)
	{
		return 0;
    }
	else
	{
		return CheckCommand(sz, pc->pc);
	}
}

extern void CommandHelp( char *sz )
{

    command *pc, *pcFull;
    char szCommand[ 128 ], szUsage[ 128 ], *szHelp;
    
#if USE_GTK 
    if( fX ){
        GTKHelp( sz );
        return;
    }
#endif
    
    if( !( pc = FindHelpCommand( &cTop, sz, szCommand, szUsage ) ) ) {
	outputf( _("No help available for topic `%s' -- try `help' for a list "
		 "of topics.\n"), sz );

	return;
    }

    if( pc->szHelp )
	/* the command has its own help text */
	szHelp = gettext ( pc->szHelp );
    else if( pc == &cTop )
	/* top-level help isn't for any command */
	szHelp = NULL;
    else {
	/* perhaps the command is an abbreviation; search for the full
	   version */
	szHelp = NULL;

	for( pcFull = acTop; pcFull->sz; pcFull++ )
	    if( pcFull->pf == pc->pf && pcFull->szHelp ) {
		szHelp = gettext ( pcFull->szHelp );
		break;
	    }
    }

    if( szHelp ) {
	outputf( _("%s- %s\n\nUsage: %s"), szCommand, szHelp, szUsage );

	if( pc->pc && pc->pc->sz )
	    outputl( _("<subcommand>\n") );
	else
	    outputc( '\n' );
    }

    if( pc->pc && pc->pc->sz ) {
	outputl( pc == &cTop ? _("Available commands:") :
		 _("Available subcommands:") );

	pc = pc->pc;
	
	for( ; pc->sz; pc++ )
	    if( pc->szHelp )
		outputf( "%-15s\t%s\n", pc->sz, gettext ( pc->szHelp ) );
    }
}


extern char *FormatMoveHint( char *sz, const matchstate *pms, movelist *pml,
			     int i, int fRankKnown,
                             int fDetailProb, int fShowParameters ) {
    
    cubeinfo ci;
    char szTemp[ 2048 ], szMove[ 32 ];
    char *pc;
    float *ar, *arStdDev;
    float rEq, rEqTop;

    GetMatchStateCubeInfo( &ci, pms );

    strcpy ( sz, "" );

    /* number */

    if ( i && ! fRankKnown )
      strcat( sz, "   ??  " );
    else
      sprintf ( pc = strchr ( sz, 0 ),
                " %4i. ", i + 1 );

    /* eval */

    sprintf ( pc = strchr ( sz, 0 ),
              "%-14s   %-28s %s: ",
              FormatEval ( szTemp, &pml->amMoves[ i ].esMove ),
              FormatMove( szMove, pms->anBoard, 
                          pml->amMoves[ i ].anMove ),
              ( !pms->nMatchTo || ( pms->nMatchTo && ! fOutputMWC ) ) ?
              _("Eq.") : _("MWC") );

    /* equity or mwc for move */

    ar = pml->amMoves[ i ].arEvalMove;
    arStdDev = pml->amMoves[ i ].arEvalStdDev;
    rEq = pml->amMoves[ i ].rScore;
    rEqTop = pml->amMoves[ 0 ].rScore;
    
    strcat ( sz, OutputEquity ( rEq, &ci, TRUE ) );

    /* difference */
   
    if ( i ) 
      sprintf ( pc = strchr ( sz, 0 ),
                " (%s)\n",
                OutputEquityDiff ( rEq, rEqTop, &ci ) );
    else
      strcat ( sz, "\n" );

    /* percentages */

    if ( fDetailProb ) {

      switch ( pml->amMoves[ i ].esMove.et ) {
      case EVAL_EVAL:
        /* FIXME: add cubeless and cubeful equities */
        strcat ( sz, "       " );
        strcat ( sz, OutputPercents ( ar, TRUE ) );
        strcat ( sz, "\n" );
        break;
      case EVAL_ROLLOUT:
        strcat ( sz, 
                 OutputRolloutResult ( "     ",
                                       NULL,
                                       ( float (*)[NUM_ROLLOUT_OUTPUTS] )
                                       ar,
                                       ( float (*)[NUM_ROLLOUT_OUTPUTS] )
                                       arStdDev,
                                       &ci,
                                       1, 
                                       pml->amMoves[ i ].esMove.rc.fCubeful ) );
        break;
      default:
        break;

      }
    }

    /* eval parameters */

    if ( fShowParameters ) {

      switch ( pml->amMoves[ i ].esMove.et ) {
      case EVAL_EVAL:
        strcat ( sz, "        " );
        strcat ( sz, 
                 OutputEvalContext ( &pml->amMoves[ i ].esMove.ec, TRUE ) );
        strcat ( sz, "\n" );
        break;
      case EVAL_ROLLOUT:
        strcat ( sz,
                 OutputRolloutContext ( "        ",
                                        &pml->amMoves[ i ].esMove ) );
        break;

      default:
        break;

      }

    }

    return sz;

#if 0

    
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
		sprintf( sz, _(" %4i. %-14s   %-28s MWC: %7.3f%%\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n"),
			 1, FormatEval ( szTemp, &pml->amMoves[ 0 ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ 0 ].anMove ),
			 rMWCTop, 
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz, _(" %4i. %-14s   %-28s MWC: %7.3f%%\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n"),
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
		sprintf( sz + 6, _(" %-14s   %-28s MWC: %7.3f%% (%+7.3f%%)\n"
			 "       %5.1f%% %5.1f%% %5.1f%%  -"
			 " %5.1f%% %5.1f%% %5.1f%%\n"),
			 FormatEval ( szTemp, &pml->amMoves[ i ].esMove ), 
			 FormatMove( szMove, pms->anBoard, 
				     pml->amMoves[ i ].anMove ),
			 rMWC, rMWC - rMWCTop,
			 100.0 * ar[ 0 ], 100.0 * ar[ 1 ], 100.0 * ar[ 2 ],
			 100.0 * ( 1.0 - ar[ 0 ] ) , 100.0 * ar[ 3 ], 
			 100.0 * ar[ 4 ] );
	    else
		sprintf( sz + 6, _(" %-14s   %-28s MWC: %7.3f%% (%+7.3f%%)\n"
			 "       %5.3f %5.3f %5.3f  -"
			 " %5.3f %5.3f %5.3f\n"),
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
#endif
}


static void
HintCube( void ) {
          
  static cubeinfo ci;
  static float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
  static float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];

  GetMatchStateCubeInfo( &ci, &ms );
    
  if ( memcmp ( &sc.ms, &ms, sizeof ( matchstate ) ) ) {
      
    /* no analysis performed yet */
      
    if ( GetDPEq ( NULL, NULL, &ci ) ) {
        
      /* calculate cube action */
      
      ProgressStart( _("Considering cube action...") );
      if ( GeneralCubeDecisionE ( aarOutput, msBoard(), &ci, 
                                  &esEvalCube.ec, 0 ) < 0 ) {
        ProgressEnd();
        return;
      }
      ProgressEnd();
      
      UpdateStoredCube ( aarOutput, aarStdDev, &esEvalCube, &ms );
      
    } else {
      
      outputl( _("You cannot double.") );
      return;
      
    }
    
  }

#if USE_GTK
  if ( fX ) {
    GTKCubeHint( sc.aarOutput, sc.aarStdDev, &sc.es );
    return;
  }
#endif

  outputl( OutputCubeAnalysis(  sc.aarOutput,
			        sc.aarStdDev,
                               &sc.es, &ci ) );
}
    

static void
HintResigned( void ) {

  float rEqBefore, rEqAfter;
  static cubeinfo ci;
  static float arOutput[ NUM_ROLLOUT_OUTPUTS ];

  GetMatchStateCubeInfo( &ci, &ms );

  /* evaluate current position */

  ProgressStart( _("Considering resignation...") );
  if ( GeneralEvaluationE ( arOutput,
                            msBoard(),
                            &ci, &esEvalCube.ec ) < 0 ) {
    ProgressEnd();
    return;
  }
  ProgressEnd();
  
  getResignEquities ( arOutput, &ci, ms.fResigned, 
                      &rEqBefore, &rEqAfter );
  
#if USE_GTK
  if ( fX ) {
    
    GTKResignHint ( arOutput, rEqBefore, rEqAfter, &ci, 
                    ms.nMatchTo && fOutputMWC );
    
    return;
    
  }
#endif
  
  if ( ! ms.nMatchTo || ( ms.nMatchTo && ! fOutputMWC ) ) {
    
    outputf ( _("Equity before resignation: %+6.3f\n"),
              - rEqBefore );
    outputf ( _("Equity after resignation : %+6.3f (%+6.3f)\n\n"),
              - rEqAfter, rEqBefore - rEqAfter );
    outputf ( _("Correct resign decision  : %s\n\n"),
              ( rEqBefore - rEqAfter >= 0 ) ?
              _("Accept") : _("Reject") );
    
  }
  else {
    
    rEqBefore = eq2mwc ( - rEqBefore, &ci );
    rEqAfter  = eq2mwc ( - rEqAfter, &ci );
    
    outputf ( _("Equity before resignation: %6.2f%%\n"),
              rEqBefore * 100.0f );
    outputf ( _("Equity after resignation : %6.2f%% (%6.2f%%)\n\n"),
              rEqAfter * 100.0f,
              100.0f * ( rEqAfter - rEqBefore ) );
    outputf ( _("Correct resign decision  : %s\n\n"),
              ( rEqAfter - rEqBefore >= 0 ) ?
              _("Accept") : _("Reject") );
    
  }
  
  return;
  

}


static void
HintTake( void ) {

  static cubeinfo ci;
  static float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
#if USE_GTK
  static float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
#endif
  static float arDouble[ 4 ];

  /* Give hint on take decision */
  GetMatchStateCubeInfo( &ci, &ms );

  ProgressStart( _("Considering cube action...") );
  if ( GeneralCubeDecisionE ( aarOutput, msBoard(), &ci, 
                              &esEvalCube.ec, &esEvalCube ) < 0 ) {
    ProgressEnd();
    return;
  }
  ProgressEnd();

  FindCubeDecision ( arDouble,  aarOutput, &ci );
	
#if USE_GTK
  if ( fX ) {
    GTKCubeHint( aarOutput, aarStdDev, &esEvalCube );
    return;
  }
#endif
	
  outputl ( _("Take decision:\n") );
	
  if ( ! ms.nMatchTo || ( ms.nMatchTo && ! fOutputMWC ) ) {
	    
    outputf ( _("Equity for take: %+6.3f\n"), -arDouble[ 2 ] );
    outputf ( _("Equity for pass: %+6.3f\n\n"), -arDouble[ 3 ] );
	    
  }
  else {
    outputf ( _("MWC for take: %6.2f%%\n"), 
              100.0 * ( 1.0 - eq2mwc ( arDouble[ 2 ], &ci ) ) );
    outputf ( _("MWC for pass: %6.2f%%\n"), 
              100.0 * ( 1.0 - eq2mwc ( arDouble[ 3 ], &ci ) ) );
  }
	
  if ( arDouble[ 2 ] < 0 && !ms.nMatchTo && ms.cBeavers < nBeavers )
    outputl ( _("Your proper cube action: Beaver!\n") );
  else if ( arDouble[ 2 ] <= arDouble[ 3 ] )
    outputl ( _("Your proper cube action: Take.\n") );
  else
    outputl ( _("Your proper cube action: Pass.\n") );

  return;
	
}
    

static void
HintChequer( char *sz ) {

  movelist ml;
  unsigned int i;
  char szBuf[ 1024 ];
  int parse_n = ParseNumber ( &sz );
  unsigned int n = (parse_n <= 0) ? 10 : parse_n;
  int anMove[ 8 ];
  moverecord *pmr;
  unsigned char auch[ 10 ];
  int fHasMoved;
  cubeinfo ci;
  
  GetMatchStateCubeInfo( &ci, &ms );

  /* 
   * Find out if a move has been made:
   * (a) by having moved something in the GUI
   * (b) by going back in the match and doing a hint on an already
   *     stored move
   *
   */
     
  fHasMoved = FALSE;

#if USE_GTK

  if ( fX && GTKGetMove( anMove ) ) {
    /* we have a legal move in the GUI */
    /* Note that we override the move from the movelist */
    MoveKey ( msBoard(), anMove, auch );
    fHasMoved = TRUE;
  }
#endif /* USE_GTK */

  if ( !fHasMoved && plLastMove && ( pmr = plLastMove->plNext->p ) && 
       pmr->mt == MOVE_NORMAL ) {
    /* we have an old stored move */
    memcpy( anMove, pmr->n.anMove, sizeof anMove );
    MoveKey( msBoard(), anMove, auch );
    fHasMoved = TRUE;
  }
  
  if ( memcmp ( &sm.ms, &ms, sizeof ( matchstate ) ) ) {

    ProgressStart( _("Considering move...") );
    if( FindnSaveBestMoves( &ml, ms.anDice[ 0 ], ms.anDice[ 1 ],
                            msBoard(), 
                            fHasMoved ? auch : NULL, 
                            arSkillLevel[ SKILL_DOUBTFUL ],
                            &ci, &esEvalChequer.ec,
                            aamfEval ) < 0 || fInterrupt ) {
      ProgressEnd();
      return;
    }
    ProgressEnd();
	
    UpdateStoredMoves ( &ml, &ms );

    if ( ml.amMoves )
      free ( ml.amMoves );

  }

  n = ( sm.ml.cMoves > n ) ? n : sm.ml.cMoves;

  if( !sm.ml.cMoves ) {
    outputl( _("There are no legal moves.") );
    return;
  }

#if USE_GTK
  if( fX ) {
    GTKHint( &sm.ml, locateMove ( msBoard(), anMove, &sm.ml ) );
    return;
  }
#endif
	
  for( i = 0; i < n; i++ )
    output( FormatMoveHint( szBuf, &ms, &sm.ml, i, 
                            TRUE, TRUE, TRUE ) );

}




extern void CommandHint( char *sz )
{

  if( ms.gs != GAME_PLAYING ) {
    outputl( _("You must set up a board first.") );
    
    return;
  }

  /* hint on cube decision */
  
  if( !ms.anDice[ 0 ] && !ms.fDoubled && ! ms.fResigned ) {
    HintCube();
    return;
  }

  /* Give hint on resignation */

  if ( ms.fResigned ) {
    HintResigned();
    return;
  }

  /* Give hint on take decision */

  if ( ms.fDoubled ) {
    HintTake();
    return;
  }

  /* Give hint on chequer play decision */

  if ( ms.anDice[ 0 ] ) {
    HintChequer( sz );
    return;
  }

}

static void
Shutdown( void ) {

  RenderFinalise();

  g_free(rngctxCurrent);
  g_free(rngctxRollout);

  FreeMatch();
  ClearMatch();

#if USE_MULTITHREAD
  MT_Close();
#endif
  EvalShutdown();

#if USE_PYTHON
  PythonShutdown();
#endif

#if HAVE_SOCKETS
#ifdef WIN32
  WSACleanup();
#endif
#endif

  SoundWait();
}

/* Called on various exit commands -- e.g. EOF on stdin, "quit" command,
   etc.  If stdin is not a TTY, this should always exit immediately (to
   avoid enless loops on EOF).  If stdin is a TTY, and fConfirm is set,
   and a game is in progress, then we ask the user if they're sure. */
extern void PromptForExit( void )
{

    static int fExiting = FALSE;
#if USE_GTK
	BoardData* bd = NULL;
	
	if (fX)
	  bd = BOARD(pwBoard)->board_data;
#endif

    if( !fExiting && fInteractive && fConfirm && ms.gs == GAME_PLAYING ) {
	fExiting = TRUE;
	fInterrupt = FALSE;
	
	if( !GetInputYN( _("Are you sure you want to exit and abort the game in "
			 "progress? ") ) ) {
	    fInterrupt = FALSE;
	    fExiting = FALSE;
	    return;
	}
    }

#if USE_BOARD3D
    if (fX && (display_is_3d(bd->rd)))
	{	/* Stop any 3d animations */
		StopIdle3d(bd, bd->bd3d);
	}
#endif
#if HAVE_SOCKETS
	/* Close any open connections */
	if( ap[0].pt == PLAYER_EXTERNAL )
		closesocket( ap[0].h );
	if( ap[1].pt == PLAYER_EXTERNAL )
		closesocket( ap[1].h );
#endif

	playSound ( SOUND_EXIT );

#if USE_BOARD3D
	if (fX && display_is_3d(bd->rd) && bd->rd->closeBoardOnExit && bd->rd->fHinges3d)
		CloseBoard3d(bd, bd->bd3d, bd->rd);
#endif
#if USE_GTK
    if( fX ) {
	while( gtk_events_pending() )
	    gtk_main_iteration();
    }
#endif

	SoundWait();    /* Wait for sound to finish before final close */

    if( fInteractive )
	PortableSignalRestore( SIGINT, &shInterruptOld );
    
#if USE_GTK
	if (fX)
		board_free_pixmaps(bd);
#if USE_BOARD3D
	if (fX && gtk_gl_init_success)
		Tidy3dObjects(bd->bd3d, bd->rd);
#endif
#endif

#if HAVE_LIBREADLINE
        write_history( gnubg_histfile );
#endif /* HAVE_READLINE */

#if USE_GTK
	if (gtk_main_level() == 1)
		gtk_main_quit();
	else
#endif
	{
		Shutdown();
		exit( EXIT_SUCCESS );
	}
}

extern void CommandNotImplemented( char *sz )
{

    outputl( _("That command is not yet implemented.") );
}

extern void CommandQuit( char *sz )
{

    PromptForExit();
}


static move *
GetMove ( const TanBoard anBoard ) {

  unsigned int i;
  unsigned char auch[ 10 ];
  TanBoard an;

  if ( memcmp ( &ms, &sm.ms, sizeof ( matchstate ) ) )
    return NULL;

  memcpy ( an, anBoard, sizeof ( an ) );
  SwapSides ( an );
  PositionKey ( (ConstTanBoard)an, auch );

  for ( i = 0; i < sm.ml.cMoves; ++i ) 
    if ( EqualKeys ( auch, sm.ml.amMoves[ i ].auch ) ) 
      return &sm.ml.amMoves[ i ];

  return NULL;

}


extern void 
CommandRollout( char *sz ) {
    
    int i, c, n, fOpponent = FALSE, cGames;
    cubeinfo ci;
    move *pm = 0;
    int num_args;

    unsigned int ( *aan )[ 2 ][ 25 ];
    char ( *asz )[ 40 ];

    num_args = c = CountTokens( sz );
  if( !c ) {
    if( ms.gs != GAME_PLAYING ) {
      outputl( _("No position specified and no game in progress.") );
      return;
    } else
      c = 1; /* current position */
  }
  else if ( rcRollout.fInitial ) {

    if ( c == 1 && ! strncmp ( sz, "=cube", 5 ) )
      outputl ( _("You cannot do a cube decision rollout for the initial"
                  " position.\n"
                  "Please 'set rollout initial off'.") );
      else
        outputl ( _("You cannot rollout moves as initial position.\n"
                  "Please 'set rollout initial off'.") );
      
      return;
  }

  /* check for `rollout =cube' */    
  if ( c == 1 && ! strncmp ( sz, "=cube", 5 ) ) {
    float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    rolloutstat aarsStatistics[ 2 ][ 2 ];
    void *p;
    char aszCube[ 2 ][ 40 ];

    evalsetup es;

    if( ms.gs != GAME_PLAYING ) {
      outputl( _("No game in progress.") );
      return;
    }

    es.et = EVAL_NONE;
    memcpy (&es.rc, &rcRollout, sizeof (rcRollout));

    GetMatchStateCubeInfo( &ci, &ms );

    FormatCubePositions( &ci, aszCube );

    RolloutProgressStart( &ci, 2, aarsStatistics, &es.rc, aszCube, &p );
    
    GeneralCubeDecisionR ( aarOutput, aarStdDev, aarsStatistics,
			   msBoard(), &ci, &es.rc, &es, RolloutProgress, p );

    UpdateStoredCube ( aarOutput, aarStdDev, &es, &ms );

    RolloutProgressEnd( &p );

    /* bring up Hint-dialog with cube rollout */
    HintCube();

    return;

  }


    aan = g_alloca( 50 * c * sizeof( int ) );
    asz = g_alloca( 40 * c );

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
		  ms.fMove, ms.nMatchTo, ms.anScore, ms.fCrawford, ms.fJacoby,
                  nBeavers, ms.bgv );

    {
      /* create all the explicit pointer arrays for RolloutGeneral */
      /* cdecl is your friend */
      float aarNoOutput[ NUM_ROLLOUT_OUTPUTS ];
      float aarNoStdDev[ NUM_ROLLOUT_OUTPUTS ];
      evalsetup NoEs;

      /* only allow doubling before first dice roll when rolling out moves*/
      int fCubeDecTop = num_args ? TRUE : FALSE;

      void *p;
	  ConstTanBoard *apBoard;
      float       (** apOutput)[ NUM_ROLLOUT_OUTPUTS ];
      float       (** apStdDev)[ NUM_ROLLOUT_OUTPUTS ];
      evalsetup   (** apes);
      const cubeinfo    (** apci);
      int         (** apCubeDecTop);
      move	  (** apMoves);

      apBoard = g_alloca (c * sizeof (int *));
      apOutput = g_alloca (c * sizeof (float *));
      apStdDev = g_alloca (c * sizeof (float *));
      apes = g_alloca (c * sizeof (evalsetup *));
      apci = g_alloca (c * sizeof (cubeinfo *));
      apCubeDecTop = g_alloca (c * sizeof (int *));
      apMoves = g_alloca (c * sizeof (move *));

      for( i = 0; i < c; i++ ) {
	/* set up to call RolloutGeneral for all the moves at once */
	if ( fOpponent ) 
	  apMoves[ i ] = pm = GetMove ( (ConstTanBoard)aan[ i ] );
    
	apBoard[ i ] = (ConstTanBoard)(aan + i);
	if (pm) {
	  apOutput[ i ] = &pm->arEvalMove;
	  apStdDev[ i ] = &pm->arEvalStdDev;
	  apes[ i ] = &pm->esMove;
	} else {
	  apOutput[ i ] = &aarNoOutput;
	  apStdDev[ i ] = &aarNoStdDev;
	  apes[ i ] = &NoEs;
	  memcpy (&NoEs.rc, &rcRollout, sizeof (rolloutcontext));
	}
	apci[ i ] = &ci;
	apCubeDecTop[ i ] = &fCubeDecTop;
      }

      RolloutProgressStart (&ci, c, NULL, &rcRollout, asz, &p);

      if( ( cGames = 
	    RolloutGeneral( apBoard, apOutput, apStdDev,
			    NULL, apes, apci,
			    apCubeDecTop, c, fOpponent, FALSE,
                           RolloutProgress, p )) <= 0 ) {
        RolloutProgressEnd( &p );
	return;
      }
      
      RolloutProgressEnd( &p );

      /* save in current movelist */

      if ( fOpponent ) {
        for (i = 0; i < c; ++i) {
          
          move *pm = apMoves[ i ];
          /* it was the =1 =2 notation */
          
          cubeinfo cix;
          
          memcpy( &cix, &ci, sizeof( cubeinfo ) );
          cix.fMove = ! cix.fMove;
          
          /* Score for move:
             rScore is the primary score (cubeful/cubeless)
             rScore2 is the secondary score (cubeless) */
          
          if ( pm->esMove.rc.fCubeful ) {
            if ( cix.nMatchTo )
              pm->rScore = 
                mwc2eq ( pm->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ], &cix );
            else
              pm->rScore = pm->arEvalMove[ OUTPUT_CUBEFUL_EQUITY ];
          }
          else
            pm->rScore = pm->arEvalMove[ OUTPUT_EQUITY ];
          
          pm->rScore2 = pm->arEvalMove[ OUTPUT_EQUITY ];
          
        }
        
        /* bring up Hint-dialog with chequerplay rollout */
        HintChequer( NULL );
      
      }


    }

}


static void LoadCommands( FILE *pf, char *szFile )
{
    char sz[ 2048 ], *pch;

    outputpostpone();
    
    for(;;) {
	sz[ 0 ] = 0;
	
	/* FIXME shouldn't restart sys calls on signals during this fgets */
	fgets( sz, sizeof( sz ), pf );

	if( ( pch = strchr( sz, '\n' ) ) ) *pch = 0;
	if( ( pch = strchr( sz, '\r' ) ) ) *pch = 0;

	if( ferror( pf ) ) {
	    outputerr( szFile );
	    outputresume();
	    return;
	}
	
	if( fAction )
	    fnAction();
	
	if( feof( pf ) || fInterrupt ) {
	    outputresume();
	    return;
	}

	if( *sz == '#' ) /* Comment */
	    continue;

#if USE_PYTHON

        if ( ! strcmp( sz, ">" ) ) {

          /* Python escape. */

          /* Ideally we should be able to handle both
           * > print 1+1
           * and
           * >
           * print 1+1
           * sys.exit()
           *
           * but so far we only handle the latter...
           */

#  if USE_GTK
          if( fX )
            GTKDisallowStdin();
#  endif

          g_assert( FALSE ); /* FIXME... */

#if USE_GTK
          if( fX )
            GTKAllowStdin();
#endif
          
          continue;

        }

#endif /* USE_PYTHON */

	HandleCommand( sz, acTop );

	/* FIXME handle NextTurn events? */
    }
    
    outputresume();
}


extern void CommandLoadPython(char *sz)
{

#if !USE_PYTHON
	output(_("This build of GNU Backgammon does not support Python"));
	return;
#else
	FILE *pf;
	char *path = NULL;

	sz = NextToken(&sz);

	if (!sz || !*sz) {
		outputl(_
			("You must specify a file to load from (see `help load "
			 "python')."));
		return;
	}

	if (g_file_test(sz, G_FILE_TEST_EXISTS))
		path = g_strdup(sz);
	else {
		path = BuildFilename2("/scripts", sz);
		if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
			g_free(path);
			path = g_build_filename("scripts", sz, NULL);
		}
	}
	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_free(path);
		outputerrf("Python file (%s) not found\n", sz);
		return;
	}

	pf = fopen(path, "r");

	if (pf) {
		if (PyRun_AnyFile(pf, path) != 0)
			outputerrf(_("An error occured while running the "
						"python file %s\n"), path);
		fclose(pf);
	} else
		outputerr(path);
	g_free(path);

#endif
}


extern void CommandLoadCommands( char *sz )
{

    FILE *pf;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( _("You must specify a file to load from (see `help load "
		 "commands').") );
	return;
    }

    if( ( pf = fopen( sz, "r" ) ) ) {
	LoadCommands( pf, sz );
	fclose( pf );
    } else
	outputerr( sz );
}


extern void CommandCopy (char *sz)
{
    char *aps[ 7 ] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
  char szOut[2048];
  char szCube[32], szPlayer0[MAX_NAME_LEN + 3], szPlayer1[MAX_NAME_LEN + 3],
    szScore0[35], szScore1[35], szMatch[35];
  char szRolled[ 32 ];
  TanBoard anBoardTemp;
    
  aps[0] = szPlayer0;
  aps[6] = szPlayer1;

  sprintf (aps[1] = szScore0,
	   ngettext("%d point", "%d points", ms.anScore[0]),
	   ms.anScore[0]);

  sprintf (aps[5] = szScore1,
	   ngettext("%d point", "%d points", ms.anScore[1]),
	   ms.anScore[1]);

  if (ms.fDoubled)
    {
      aps[ms.fTurn ? 4 : 2] = szCube;

      sprintf (szPlayer0, "O: %s", ap[0].szName);
      sprintf (szPlayer1, "X: %s", ap[1].szName);
      sprintf (szCube, _("Cube offered at %d"), ms.nCube << 1);
    }
  else
    {
      sprintf (szPlayer0, "O: %s", ap[0].szName);
      sprintf (szPlayer1, "X: %s", ap[1].szName);

      aps[ms.fMove ? 4 : 2] = szRolled;

      if (ms.anDice[0])
	sprintf (szRolled, _("Rolled %d%d"), ms.anDice[0], ms.anDice[1]);
      else if (!GameStatus (msBoard(), ms.bgv))
	strcpy (szRolled, _("On roll"));
      else
	szRolled[0] = 0;

      if (ms.fCubeOwner < 0)
	{
	  aps[3] = szCube;

	  if (ms.nMatchTo)
	    sprintf (szCube, _("%d point match (Cube: %d)"), ms.nMatchTo,
		     ms.nCube);
	  else
	    sprintf (szCube, _("(Cube: %d)"), ms.nCube);
	}
      else
	{
	  int cch = strlen (ap[ms.fCubeOwner].szName);

	  if (cch > 20)
	    cch = 20;

	  sprintf (szCube, _("%c: %*s (Cube: %d)"), ms.fCubeOwner ? 'X' :
		   'O', cch, ap[ms.fCubeOwner].szName, ms.nCube);

	  aps[ms.fCubeOwner ? 6 : 0] = szCube;

	  if (ms.nMatchTo)
	    sprintf (aps[3] = szMatch, _("%d point match"), ms.nMatchTo);
	}
    }

  memcpy ( anBoardTemp, msBoard(), sizeof(TanBoard) );

  if ( ! ms.fMove )
    SwapSides ( anBoardTemp );

  DrawBoard (szOut, (ConstTanBoard)anBoardTemp, ms.fMove, aps, MatchIDFromMatchState (&ms),
             anChequers[ ms.bgv ] );
  strcat (szOut, "\n");
  TextToClipboard (szOut);
}

static void LoadRCFiles(void)
{
	char *sz, *szz;

	outputoff();
	sz = g_build_filename(szHomeDirectory, "gnubgautorc", NULL);
	szz = g_strdup_printf("'%s'", sz);
	if (g_file_test(sz, G_FILE_TEST_EXISTS))
		CommandLoadCommands(szz);
	g_free(sz);
	g_free(szz);

	sz = g_build_filename(szHomeDirectory, "gnubgrc", NULL);
	szz = g_strdup_printf("'%s'", sz);
	if (g_file_test(sz, G_FILE_TEST_EXISTS))
		CommandLoadCommands(szz);
	g_free(sz);
	g_free(szz);

	outputon();
}


static void
SaveRNGSettings ( FILE *pf, char *sz, rng rngCurrent, void *rngctx ) {

    switch( rngCurrent ) {
    case RNG_ANSI:
	fprintf( pf, "%s rng ansi\n", sz );
	break;
    case RNG_BBS:
	fprintf( pf, "%s rng bbs\n", sz ); /* FIXME save modulus */
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
    case RNG_RANDOM_DOT_ORG:
        fprintf( pf, "%s rng random.org\n", sz );
	break;
    case RNG_USER:
	/* don't save user RNGs */
	break;
    case RNG_FILE:
        fprintf( pf, "%s rng file \"%s\"\n", sz, GetDiceFileName( rngctx ) );
	break;
    default:
        break;
    }

}


static void
SaveMoveFilterSettings ( FILE *pf, 
                         const char *sz,
                         movefilter aamf [ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ] ) {

    int i, j;
    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

    for (i = 0; i < MAX_FILTER_PLIES; ++i) 
      for (j = 0; j <= i; ++j) {
	fprintf (pf, "%s %d  %d  %d %d %s\n",
                 sz, 
		 i+1, j, 
		 aamf[i][j].Accept,
		 aamf[i][j].Extra,
	         g_ascii_formatd(buf, G_ASCII_DTOSTR_BUF_SIZE,
			 "%0.3g", aamf[i][j].Threshold));
      }
}




static void 
SaveEvalSettings( FILE *pf, char *sz, evalcontext *pec ) {

    gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
    gchar *szNoise = g_ascii_formatd(buf, G_ASCII_DTOSTR_BUF_SIZE, "%0.3f",  pec->rNoise);
    fprintf( pf, "%s plies %d\n"
#if defined( REDUCTION_CODE )
	     "%s reduced %d\n"
#else
	     "%s prune %s\n"
#endif
	     "%s cubeful %s\n"
	     "%s noise %s\n"
	     "%s deterministic %s\n",
	     sz, pec->nPlies, 
#if defined( REDUCTION_CODE )
	     sz, pec->nReduced,
#else
	     sz, pec->fUsePrune ? "on" : "off",
#endif
	     sz, pec->fCubeful ? "on" : "off",
	     sz, szNoise,
             sz, pec->fDeterministic ? "on" : "off" );
}


extern void
SaveRolloutSettings ( FILE *pf, char *sz, rolloutcontext *prc ) {

  char *pch;
  int i; /* flags and stuff */
  gchar szTemp1[G_ASCII_DTOSTR_BUF_SIZE];
  gchar szTemp2[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd(szTemp1, G_ASCII_DTOSTR_BUF_SIZE, "%05.4f",  prc->rStdLimit);
  g_ascii_formatd(szTemp2, G_ASCII_DTOSTR_BUF_SIZE, "%05.4f",  prc->rJsdLimit);

  fprintf ( pf,
            "%s cubeful %s\n"
            "%s varredn %s\n"
            "%s quasirandom %s\n"
            "%s initial %s\n"
            "%s truncation enable %s\n"
            "%s truncation plies %d\n"
            "%s bearofftruncation exact %s\n"
            "%s bearofftruncation onesided %s\n"
            "%s later enable %s\n"
            "%s later plies %d\n"
            "%s trials %d\n"
            "%s cube-equal-chequer %s\n"
            "%s players-are-same %s\n"
            "%s truncate-equal-player0 %s\n"
            "%s limit enable %s\n"
            "%s limit minimumgames %d\n"
            "%s limit maxerror %s\n"
	    "%s jsd stop %s\n"
            "%s jsd move %s\n"
            "%s jsd minimumgames %d\n"
            "%s jsd limit %s\n",
            sz, prc->fCubeful ? "on" : "off",
            sz, prc->fVarRedn ? "on" : "off",
            sz, prc->fRotate ? "on" : "off",
            sz, prc->fInitial ? "on" : "off",
            sz, prc->fDoTruncate ? "on" : "off",
            sz, prc->nTruncate,
            sz, prc->fTruncBearoff2 ? "on" : "off",
            sz, prc->fTruncBearoffOS ? "on" : "off", 
            sz, prc->fLateEvals ? "on" : "off",
            sz, prc->nLate,
            sz, prc->nTrials,
            sz, fCubeEqualChequer ? "on" : "off",
            sz, fPlayersAreSame ? "on" : "off",
            sz, fTruncEqualPlayer0 ? "on" : "off",
	    sz, prc->fStopOnSTD ? "on" : "off",
	    sz, prc->nMinimumGames,
	    sz, szTemp1,
            sz, prc->fStopOnJsd ? "on" : "off",
            sz, prc->fStopMoveOnJsd ? "on" : "off",
            sz, prc->nMinimumJsdGames,
	    sz, szTemp2
            );
  
  SaveRNGSettings ( pf, sz, prc->rngRollout, rngctxRollout );

  /* chequer play and cube decision evalcontexts */

  pch = malloc ( strlen ( sz ) + 50 );

  strcpy ( pch, sz );

  for ( i = 0; i < 2; i++ ) {

    sprintf ( pch, "%s player %i chequerplay", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecChequer[ i ] );

    sprintf ( pch, "%s player %i cubedecision", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecCube[ i ] );

    sprintf ( pch, "%s player %i movefilter", sz, i );
    SaveMoveFilterSettings ( pf, pch, prc->aaamfChequer[ i ] );

  }

  for ( i = 0; i < 2; i++ ) {

    sprintf ( pch, "%s later player %i chequerplay", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecChequerLate[ i ] );

    sprintf ( pch, "%s later player %i cubedecision", sz, i );
    SaveEvalSettings ( pf, pch, &prc->aecCubeLate[ i ] );

    sprintf ( pch, "%s later player %i movefilter", sz, i );
    SaveMoveFilterSettings ( pf, pch, prc->aaamfLate[ i ] );

  }

  sprintf (pch, "%s truncation cubedecision", sz);
  SaveEvalSettings ( pf, pch, &prc->aecCubeTrunc );
  sprintf (pch, "%s truncation chequerplay", sz );
  SaveEvalSettings ( pf, pch, &prc->aecChequerTrunc);

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

extern void CommandSaveSettings( char *szParam )
{
    FILE *pf;
    int i;
    unsigned int cCache; 
    char *szFile;
    char szTemp[ 4096 ];
#if USE_GTK
    char *aszAnimation[] = {"none", "blink", "slide"};
#endif
    gchar buf[ G_ASCII_DTOSTR_BUF_SIZE ];
    gchar aszThr[7][ G_ASCII_DTOSTR_BUF_SIZE ];
    
    g_ascii_formatd(aszThr[0], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arSkillLevel[ SKILL_BAD ]);
    g_ascii_formatd(aszThr[1], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arSkillLevel[ SKILL_DOUBTFUL ]);
    g_ascii_formatd(aszThr[2], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arLuckLevel[ LUCK_GOOD ]);
    g_ascii_formatd(aszThr[3], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arLuckLevel[ LUCK_BAD ]);
    g_ascii_formatd(aszThr[4], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arSkillLevel[ SKILL_VERYBAD ]);
    g_ascii_formatd(aszThr[5], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arLuckLevel[ LUCK_VERYGOOD ]);
    g_ascii_formatd(aszThr[6], G_ASCII_DTOSTR_BUF_SIZE, "%0.3f", arLuckLevel[ LUCK_VERYBAD ] );

    szParam = NextToken ( &szParam );
    
    if ( !szParam || ! *szParam ) {
      /* no filename parameter given -- save to default location */
      szFile = g_build_filename(szHomeDirectory, "gnubgautorc", NULL);
    }
    else 
      szFile = g_strdup ( szParam );
      

    if( ! strcmp( szFile, "-" ) )
      pf = stdout;
    else 
      pf = g_fopen( szFile, "w" );

    if ( ! pf ) {
      outputerr( szFile );
      g_free ( szFile );
      return;
    }

    errno = 0;

    fprintf ( pf, 
              _( "#\n"
                 "# GNU Backgammon command file\n"
                 "#   generated by %s\n"
                 "#\n"
                 "# WARNING: The file `.gnubgautorc' is automatically "
                 "generated by the\n"
                 "# `save settings' command, and will be overwritten the next "
                 "time settings\n"
                 "# are saved.  If you want to add startup commands manually, "
                 "you should\n"
                 "# use `.gnubgrc' instead.\n"
                 "\n"), 
              VERSION_STRING );

    /* language preference */

    fprintf( pf, "set lang %s\n", szLang );

    /* analysis settings */

    SaveEvalSetupSettings ( pf, "set analysis chequerplay",
			    &esAnalysisChequer );
    SaveEvalSetupSettings ( pf, "set analysis cubedecision",
			    &esAnalysisCube );
    SaveMoveFilterSettings ( pf, "set analysis movefilter", aamfAnalysis );

    SaveEvalSettings ( pf, "set analysis luckanalysis", &ecLuck );
    
    fprintf( pf, "set analysis limit %d\n", cAnalysisMoves );

    fprintf( pf, "set analysis threshold bad %s\n"
	     "set analysis threshold doubtful %s\n"
	     "set analysis threshold lucky %s\n"
	     "set analysis threshold unlucky %s\n"
	     "set analysis threshold verybad %s\n"
	     "set analysis threshold verylucky %s\n"
	     "set analysis threshold veryunlucky %s\n", 
	     aszThr[0], aszThr[1], aszThr[2], aszThr[3], aszThr[4], aszThr[5], aszThr[6]);

    fprintf ( pf,
              "set analysis cube %s\n"
              "set analysis luck %s\n"
              "set analysis moves %s\n",
              fAnalyseCube ? "on" : "off",
              fAnalyseDice ? "on" : "off",
              fAnalyseMove ? "on" : "off" );

    for ( i = 0; i < 2; ++i )
      fprintf( pf, "set analysis player %d analyse %s\n",
               i, afAnalysePlayers[ i ] ? "yes" : "no" );

    /* Render preferences */

    fputs( RenderPreferencesCommand( GetMainAppearance(), szTemp ), pf );
    fputc( '\n', pf );
    
    fprintf( pf, 
             "set automatic bearoff %s\n"
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

#if USE_MULTITHREAD
    fprintf( pf, "set threads %d\n", MT_GetNumThreads() );
#endif
#if USE_BOARD3D
	if (fSync != -1)
		fprintf( pf, "set vsync3d %s\n", fSync ? "yes" : "no" );
#endif

    fprintf( pf, "set clockwise %s\n"
		    "set tutor mode %s\n"
		    "set tutor cube %s\n"
		    "set tutor chequer %s\n"
		    "set tutor eval %s\n"
		    "set tutor skill %s\n"
		    "set confirm new %s\n"
		    "set confirm save %s\n"
		    "set cube use %s\n"
#if USE_GTK
		    "set delay %d\n"
#endif
		    "set display %s\n",
		    fClockwise ? "on" : "off", 
		    fTutor ? "on" : "off",
		    fTutorCube ? "on" : "off",
		    fTutorChequer ? "on" : "off",
		    fTutorAnalysis ? "on" : "off",
		    ((TutorSkill == SKILL_VERYBAD) ? "very bad" :
		     (TutorSkill == SKILL_BAD) ? "bad" : "doubtful"),
		    fConfirm ? "on" : "off",
                    fConfirmSave ? "on" : "off",
                    fCubeUse ? "on" : "off",
#if USE_GTK
                    nDelay,
#endif
                    fDisplay ? "on" : "off");

    SaveEvalSetupSettings ( pf, "set evaluation chequerplay", &esEvalChequer );
    SaveEvalSetupSettings ( pf, "set evaluation cubedecision", &esEvalCube );
    SaveMoveFilterSettings ( pf, "set evaluation movefilter", aamfEval );

    fprintf( pf, "set cheat enable %s\n", fCheat ? "on" : "off" );
    for ( i = 0; i < 2; ++i )
      fprintf( pf, "set cheat player %d roll %d\n", i, afCheatRoll[ i ] );
               

#if USE_GTK
{
	int dummy;
	if (fFullScreen)
		GetFullscreenWindowSettings(&dummy, &GetMainAppearance()->fShowIDs, &dummy);

    fprintf( pf, "set gui animation %s\n"
	     "set gui animation speed %d\n"
	     "set gui beep %s\n"
	     "set gui dicearea %s\n"
	     "set gui highdiefirst %s\n"
	     "set gui illegal %s\n"
	     "set gui showids %s\n"
	     "set gui showpips %s\n"
	     "set gui dragtargethelp %s\n"
		 "set gui usestatspanel %s\n"
		 "set gui movelistdetail %s\n",
	     aszAnimation[ animGUI ], nGUIAnimSpeed,
	     fGUIBeep ? "on" : "off",
	     GetMainAppearance()->fDiceArea ? "on" : "off",
	     fGUIHighDieFirst ? "on" : "off",
	     fGUIIllegal ? "on" : "off",
	     GetMainAppearance()->fShowIDs ? "on" : "off",
	     fGUIShowPips ? "on" : "off",
	     fGUIDragTargetHelp ? "on" : "off",
		 fGUIUseStatsPanel ? "on" : "off",
		 showMoveListDetail ? "on" : "off");

	if (fFullScreen)
		GetMainAppearance()->fShowIDs = FALSE;
}
#endif
    
    fprintf( pf, "set jacoby %s\n", fJacoby ? "on" : "off" );
    fprintf( pf, "set gotofirstgame %s\n", fGotoFirstGame ? "on" : "off" );

    fprintf( pf, "set matchequitytable \"%s\"\n", miCurrent.szFileName );
    fprintf( pf, "set matchlength %d\n", nDefaultLength );
    
    fprintf( pf, "set variation %s\n", aszVariationCommands[ bgvDefault ] );

    fprintf( pf, "set output matchpc %s\n"
	     "set output mwc %s\n"
	     "set output rawboard %s\n"
	     "set output winpc %s\n"
             "set output digits %d\n"
             "set output errorratefactor %s\n",
	     fOutputMatchPC ? "on" : "off",
	     fOutputMWC ? "on" : "off",
	     fOutputRawboard ? "on" : "off",
	     fOutputWinPC ? "on" : "off",
             fOutputDigits,
	         g_ascii_formatd(buf, G_ASCII_DTOSTR_BUF_SIZE, "%f", rErrorRateFactor ));
    
    for( i = 0; i < 2; i++ ) {
	fprintf( pf, "set player %d name %s\n", i, ap[ i ].szName );
	
	switch( ap[ i ].pt ) {
	case PLAYER_GNU:
	    fprintf( pf, "set player %d gnubg\n", i );
	    sprintf( szTemp, "set player %d chequerplay", i );
	    SaveEvalSetupSettings( pf, szTemp, &ap[ i ].esChequer );
	    sprintf( szTemp, "set player %d cubedecision", i );
	    SaveEvalSetupSettings( pf, szTemp, &ap[ i ].esCube );
	    sprintf( szTemp, "set player %d movefilter", i );
            SaveMoveFilterSettings ( pf, szTemp, ap[ i ].aamf );
	    break;
	    
	case PLAYER_HUMAN:
	    fprintf( pf, "set player %d human\n", i );
	    break;
	    
	case PLAYER_EXTERNAL:
	    /* don't save external players */
	    break;
	}
    }

    fprintf( pf, "set prompt %s\n", szPrompt );

    SaveRNGSettings ( pf, "set", rngCurrent, rngctxCurrent );

    SaveRolloutSettings( pf, "set rollout", &rcRollout );

    if (default_import_folder && *default_import_folder)
	fprintf (pf, "set import folder \"%s\"\n", default_import_folder);
    if (default_export_folder && *default_export_folder)
	fprintf (pf, "set export folder \"%s\"\n", default_export_folder);
    if (default_sgf_folder && *default_sgf_folder)
	fprintf (pf, "set sgf folder \"%s\"\n", default_sgf_folder);

    fprintf ( pf, 
              "set export include annotations %s\n"
              "set export include analysis %s\n"
              "set export include statistics %s\n"
              "set export include legend %s\n"
              "set export include matchinfo %s\n",
              exsExport.fIncludeAnnotation ? "yes" : "no",
              exsExport.fIncludeAnalysis ? "yes" : "no",
              exsExport.fIncludeStatistics ? "yes" : "no",
              exsExport.fIncludeLegend ? "yes" : "no",
              exsExport.fIncludeMatchInfo ? "yes" : "no" );

    fprintf ( pf, "set export show board %d\n", exsExport.fDisplayBoard );

    if ( exsExport.fSide == 3 )
      fprintf ( pf, "set export show player both\n" );
    else if ( exsExport.fSide )
      fprintf ( pf, "set export show player %d\n", exsExport.fSide - 1 );

    fprintf ( pf, "set export move number %d\n", exsExport.nMoves );

    fprintf ( pf,
              "set export moves parameters evaluation %s\n"
              "set export moves parameters rollout %s\n"
              "set export moves probabilities %s\n",
              exsExport.afMovesParameters[ 0 ] ? "yes" : "no",
              exsExport.afMovesParameters[ 1 ] ? "yes" : "no",
              exsExport.fMovesDetailProb  ? "yes" : "no" );

    for ( i = 0; i < N_SKILLS; i++ ) {
      if ( i == SKILL_NONE ) 
        fprintf ( pf, "set export moves display unmarked %s\n", 
                  exsExport.afMovesDisplay[ i ] ? "yes" : "no" );
      else
        fprintf ( pf, "set export moves display %s %s\n", 
                  aszSkillTypeCommand[ i ], 
                  exsExport.afMovesDisplay[ i ] ? "yes" : "no" );
    }

    fprintf ( pf,
              "set export cube parameters evaluation %s\n"
              "set export cube parameters rollout %s\n"
              "set export cube probabilities %s\n",
              exsExport.afCubeParameters[ 0 ] ? "yes" : "no",
              exsExport.afCubeParameters[ 1 ] ? "yes" : "no",
              exsExport.fCubeDetailProb ? "yes" : "no" );

    for ( i = 0; i < N_SKILLS; i++ ) {
      if ( i == SKILL_NONE )
        fprintf ( pf, "set export cube display unmarked %s\n", 
                  exsExport.afCubeDisplay[ i ] ? "yes" : "no" );
      else
        fprintf ( pf, "set export cube display %s %s\n", 
                  aszSkillTypeCommand[ i ], 
                  exsExport.afCubeDisplay[ i ] ? "yes" : "no" );
    }
    
    fprintf ( pf, "set export cube display actual %s\n", 
              exsExport.afCubeDisplay[ EXPORT_CUBE_ACTUAL ] ? "yes" : "no" );
    fprintf ( pf, "set export cube display missed %s\n", 
              exsExport.afCubeDisplay[ EXPORT_CUBE_MISSED ] ? "yes" : "no" );
    fprintf ( pf, "set export cube display close %s\n", 
              exsExport.afCubeDisplay[ EXPORT_CUBE_CLOSE ] ? "yes" : "no" );

    if ( *exsExport.szHTMLPictureURL != '"' &&
       strchr ( exsExport.szHTMLPictureURL, ' ' ) )
       fprintf ( pf, "set export html pictureurl \"%s\"\n",
                  exsExport.szHTMLPictureURL );
    else
      fprintf ( pf, "set export html pictureurl  %s\n",
                exsExport.szHTMLPictureURL );

    fprintf ( pf, "set export html type \"%s\"\n",
              aszHTMLExportType[ exsExport.het ] );

    fprintf ( pf, "set export html css %s\n",
              aszHTMLExportCSSCommand[ exsExport.hecss ] );

    fprintf ( pf, "set export png size %d\n", exsExport.nPNGSize );
    fprintf ( pf, "set export html size %d\n", exsExport.nHtmlSize );

    /* invert settings */

    fprintf ( pf, 
              "set invert matchequitytable %s\n",
              fInvertMET ? "on" : "off" );

    /* geometries */
    /* "set gui windowpositions" must come first */
#if USE_GTK
    fprintf( pf, "set gui windowpositions %s\n",
	     fGUISetWindowPos ? "on" : "off" );
    if ( fX )
       RefreshGeometries ();

	SaveWindowSettings(pf);
#endif

    /* sounds */
    fprintf ( pf, "set sound enable %s\n", 
              fSound ? "yes" : "no" );
    fprintf ( pf, "set sound system command %s\n",
              sound_get_command());

    for ( i = 0; i < NUM_SOUNDS; ++i ) 
    {
       char *file = GetSoundFile(i);
       fprintf ( pf, "set sound sound %s \"%s\"\n",
		       sound_command [ i ], file);
       g_free(file);
    }

    fprintf( pf, "set browser \"%s\"\n", get_web_browser());

    fprintf( pf, "set priority nice %d\n", nThreadPriority );

    /* rating offset */
    
    fprintf( pf, "set ratingoffset %s\n",
       g_ascii_formatd(buf, G_ASCII_DTOSTR_BUF_SIZE, "%f", rRatingOffset ));
	/* warnings */
#if USE_GTK
	for (i = 0; i < WARN_NUM_WARNINGS; i++)
	{
		if (!warningEnabled[i])
			fprintf(pf, "set warning %s off\n", warningNames[i]);
	}
#endif
	/* Save toolbar style */
	fprintf(pf, "set toolbar %d\n", nToolbarStyle);

	/* Save gamelist style on/off (if not set - default is set) */
	if (!fStyledGamelist)
		fputs("set styledgamelist off\n", pf);

	if (fFullScreen)
		fputs("set fullscreen on\n", pf);

    /* the end */

    
    if ( pf != stdout )
      fclose( pf );
    
    if( errno )
      outputerr( szFile );
    else
      outputf( _("Settings saved to %s.\n"),
               ( ! strcmp ( szFile, "-" ) ) ? _("standard output stream") :
               szFile );
    g_free ( szFile );

#if USE_GTK
    if( fX )
	GTKSaveSettings();
#endif

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
      if( !StrNCaseCmp( sz, pc->sz, cch ) && pc->szHelp ) {
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
	
	if( !StrNCaseCmp( sz, pch, cch ) ) {
	    if( !( szDup = malloc( strlen( pch ) + 1 ) ) )
		return NULL;
	    
	    return strcpy( szDup, pch );
	}
	++i;
    }

    return NULL;
}

static char *OnOffCompletion( const char *sz, int nState ) {

	static unsigned int i;
    static int cch;
    static char *asz[] = { "false", "no", "off", "on", "true", "yes" };
    char *pch, *szDup;
    
    if( !nState ) {
	cch = strlen( sz );
	i = 0;
    }

    while( i < sizeof(asz)/sizeof(asz[0]) ) {
	pch = asz[ i++ ];

	if( !StrNCaseCmp( sz, pch, cch ) ) {
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

	if( !StrNCaseCmp( sz, pch, cch ) ) {
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

    char *sz = (char*) g_alloca(strlen( szOrig )  * sizeof(char) + 1);
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
            if( !StrNCaseCmp( pchCurrent, pc->sz, strlen( pchCurrent ) ) ) {
		pc = pc->pc;

		if( pc == acSetPlayer || pc == acSetRolloutPlayer || 
                    pc == acSetRolloutLatePlayer ||
                    pc == acSetAnalysisPlayer || pc == acSetCheatPlayer ) {
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

static char **CompleteKeyword( const char *szText, int iStart, int iEnd)
{

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

extern void Prompt( void )
{

#if HAVE_LIBREADLINE
    if( !fInteractive || !isatty( STDIN_FILENO ) )
	return;
#endif

    ProgressEnd();
    
    g_print( FormatPrompt() );
    fflush( stdout );    
}

#if USE_GTK
#if HAVE_LIBREADLINE
extern void ProcessInput( char *sz)
{

    char *pchExpanded;

    rl_callback_handler_remove();
    fReadingCommand = FALSE;
    
    if( !sz ) {
	outputc( '\n' );
	PromptForExit();
	sz = "";
    }
    
    fInterrupt = FALSE;

    /* expand history */

    history_expand( sz, &pchExpanded );
    
    if( *pchExpanded )
	add_history( pchExpanded );
	
    if( fX )
	GTKDisallowStdin();

    HandleCommand( pchExpanded, acTop );
    free( pchExpanded );
    
    if( fX )
	GTKAllowStdin();

    ResetInterrupt();


    /* Recalculate prompt -- if we call nothing, then readline will
       redisplay the old prompt.  This isn't what we want: we either
       want no prompt at all, yet (if NextTurn is going to be called),
       or if we do want to prompt immediately, we recalculate it in
       case the information in the old one is out of date. */
    if( nNextTurn )
	fNeedPrompt = TRUE;
    else {
	    char *sz = locale_from_utf8(FormatPrompt());
	    ProgressEnd();
	    rl_callback_handler_install( sz, ProcessInput );
	    g_free(sz);
	    fReadingCommand = TRUE;
    }
}

#endif

/* Handle a command as if it had been typed by the user. */
extern void UserCommand( char *szCommand )
{

#if HAVE_LIBREADLINE
    int nOldEnd;
#endif
    int cch = strlen( szCommand ) + 1;
    char *sz = (char*) g_alloca(cch * sizeof(char));
    
    /* Unfortunately we need to copy the command, because it might be in
       read-only storage and HandleCommand might want to modify it. */
    strcpy( sz, szCommand );

    if( !fTTY ) {
	fInterrupt = FALSE;
	HandleCommand( sz, acTop );
	ResetInterrupt();
	return;
    }

    /* Note that the command is always echoed to stdout; the output*()
       functions are bypassed. */
#if HAVE_LIBREADLINE
    if( fInteractive ) {
	nOldEnd = rl_end;
	rl_end = 0;
	rl_redisplay();
	g_print("%s\n", sz);
	ProcessInput( sz);
	return;
    }
#endif

    if( fInteractive ) {
	putchar( '\n' );
	Prompt();
	g_print("%s\n", sz);
    }
    
    fInterrupt = FALSE;
    
    HandleCommand( sz, acTop );

    ResetInterrupt();
    
    if( nNextTurn )
	Prompt();
    else
	fNeedPrompt = TRUE;
}

extern gint NextTurnNotify( gpointer p )
{

    NextTurn( TRUE );

    ResetInterrupt();

    if( fNeedPrompt )
    {
#if HAVE_LIBREADLINE
	if( fInteractive ) {
	    char *sz = locale_from_utf8(FormatPrompt());
	    ProgressEnd();
	    rl_callback_handler_install( sz, ProcessInput );
	    g_free(sz);
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
extern char *GetInput( char *szPrompt )
{

    char *sz;
    char *pch;
    char *pchConverted;
#if USE_GTK
    g_assert( fTTY && !fX );
#endif

#if HAVE_LIBREADLINE
    if( fInteractive ) {
	char *prompt;
	/* Using readline, but not X. */
	if( fInterrupt )
	    return NULL;
	
	fReadingOther = TRUE;
	
	prompt =  locale_from_utf8(szPrompt);
	while( !( sz = readline( prompt ) ) ) {
	    outputc( '\n' );
	    PromptForExit();
	}
	g_free(prompt);
	
	fReadingOther = FALSE;
	
	if( fInterrupt )
	    return NULL;
	
	pchConverted = locale_to_utf8(sz);
	free( sz );
	return pchConverted;
    }
#endif
    /* Not using readline or X. */
    if( fInterrupt )
	return NULL;

    g_print( szPrompt );
    fflush( stdout );

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
    
    if( ( pch = strchr( sz, '\n' ) ) ) *pch = 0;
    if( ( pch = strchr( sz, '\r' ) ) ) *pch = 0;
    
    pchConverted = locale_to_utf8(sz);
    free( sz );
    return pchConverted;
}

/* Ask a yes/no question.  Interrupting the question is considered a "no"
   answer. */
extern int GetInputYN( char *szPrompt )
{

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
		g_free( pch );
		return TRUE;
	    case 'n':
	    case 'N':
		g_free( pch );
		return FALSE;
	    default:
		g_free( pch );
	    }

	if( fInterrupt )
	    return FALSE;
	
	outputl( _("Please answer `y' or `n'.") );
    }
}

/* Like strncpy, except it does the right thing */
extern char *strcpyn( char *szDest, const char *szSrc, int cch )
{

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
extern void output( const char *sz )
{

    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX ) {
	GTKOutput( g_strdup( sz ) );
	return;
    }
#endif
    g_print("%s", sz);
    if( !isatty( STDOUT_FILENO ) ) 
       fflush( stdout );

}

/* Write a string to stdout/status bar/popup window, and append \n */
extern void outputl( const char *sz )
{

    
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
    g_print("%s\n", sz);
    if( !isatty( STDOUT_FILENO ) ) 
       fflush( stdout );
}
    
/* Write a character to stdout/status bar/popup window */
extern void outputc( const char ch )
{

    char sz[2];
	sz[0] = ch;
	sz[1] = '\0';
    
    output( sz );
}
    
/* Write a string to stdout/status bar/popup window, printf style */
extern void outputf( const char *sz, ... )
{

    va_list val;

    va_start( val, sz );
    outputv( sz, val );
    va_end( val );
}

/* Write a string to stdout/status bar/popup window, vprintf style */
extern void outputv( const char *sz, va_list val )
{

    char *szFormatted;
    if( cOutputDisabled )
	return;
    szFormatted = g_strdup_vprintf( sz, val );
    output( szFormatted );
    g_free( szFormatted );
}

/* Write an error message, perror() style */
extern void outputerr( const char *sz )
{

    /* FIXME we probably shouldn't convert the charset of strerror() - yuck! */
    
    outputerrf( "%s: %s", sz, strerror( errno ) );
}

/* Write an error message, fprintf() style */
extern void outputerrf( const char *sz, ... )
{

    va_list val;

    va_start( val, sz );
    outputerrv( sz, val );
    va_end( val );
}

/* Write an error message, vfprintf() style */
extern void outputerrv( const char *sz, va_list val )
{

    char *szFormatted;
    szFormatted = g_strdup_vprintf( sz, val );

#if USE_GTK
    if( fX )
	GTKOutputErr( szFormatted );
#else
	g_printerr("%s", szFormatted);
    if( !isatty( STDOUT_FILENO ) ) 
       fflush( stdout );
    putc( '\n', stderr );
#endif
    g_free( szFormatted );
}

/* Signifies that all output for the current command is complete */
extern void outputx( void )
{
    
    if( cOutputDisabled || cOutputPostponed )
	return;

#if USE_GTK
    if( fX )
	GTKOutputX();
#endif
}

/* Signifies that subsequent output is for a new command */
extern void outputnew( void )
{
    
    if( cOutputDisabled )
	return;
    
#if USE_GTK
    if( fX )
	GTKOutputNew();
#endif
}

/* Disable output */
extern void outputoff( void )
{

    cOutputDisabled++;
}

/* Enable output */
extern void outputon( void )
{

    g_assert( cOutputDisabled );

    cOutputDisabled--;
}

/* Temporarily disable outputx() calls */
extern void outputpostpone( void )
{

    cOutputPostponed++;
}

/* Re-enable outputx() calls */
extern void outputresume( void )
{

    g_assert( cOutputPostponed );

    if( !--cOutputPostponed )
    {
	outputx();
    }
}

/* Temporarily ignore TTY/GUI input. */
extern void SuspendInput(void)
{

#if USE_GTK
    if ( fX )
       GTKSuspendInput();
#endif
}

/* Resume input (must match a previous SuspendInput). */
extern void ResumeInput(void)
{

#if USE_GTK
    if ( fX )
       GTKResumeInput();
#endif
}

static GTimeVal tvProgress;

static int ProgressThrottle( void ) {
    GTimeVal tv, tvDiff;
    g_get_current_time(&tv);
    
    tvDiff.tv_sec = tv.tv_sec - tvProgress.tv_sec;
    if( ( tvDiff.tv_usec = tv.tv_usec + 1000000 - tvProgress.tv_usec ) >=
	1000000 )
	tvDiff.tv_usec -= 1000000;
    else
	tvDiff.tv_sec--;

    if( tvDiff.tv_sec || tvDiff.tv_usec >= 100000 ) {
	/* sufficient time elapsed; record current time */
	tvProgress.tv_sec = tv.tv_sec;
	tvProgress.tv_usec = tv.tv_usec;
	return 0;
    }

    /* insufficient time elapsed */
    return -1;
}

extern void ProgressStart( char *sz )
{

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

  fInProgress = TRUE;

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

  if ( !fShowProgress || iProgressValue == iValue )
    return;

  iProgressValue = iValue;

  if( ProgressThrottle() )
      return;
#if USE_GTK
  if( fX ) {
    GTKProgressValue( iValue, iProgressMax );
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


static void Progress( void )
{

    static int i = 0;
    static char ach[ 4 ] = "/-\\|";
   
    if( !fShowProgress )
	return;

    if( ProgressThrottle() )
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

static void CallbackProgress( void )
{
#if USE_GTK
	if( fX )
	{
		GTKDisallowStdin();
		if (fInProgress)
			SuspendInput();

		while( gtk_events_pending() )
			gtk_main_iteration();

		if (fInProgress)
			ResumeInput();
		GTKAllowStdin();
	}
#endif

    if( fInProgress && !iProgressMax )
	Progress();
}

extern void ProgressEnd( void )
{

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

static void
move_rc_files (void)
{
  /* Move files to the new location. Remove this part when all users have had
   * their files moved.*/
  char *olddir, *oldfile, *newfile;
#if WIN32
  olddir = g_strdup (getInstallDir());
#else
  olddir = g_build_filename (szHomeDirectory, "..", NULL);
#endif

  newfile = g_build_filename (szHomeDirectory, "gnubgautorc", NULL);
  oldfile = g_build_filename (olddir, ".gnubgautorc", NULL);
  g_rename (oldfile, newfile);
  g_free (oldfile);
  g_free (newfile);

  newfile = g_build_filename (szHomeDirectory, "gnubgrc", NULL);
  oldfile = g_build_filename (olddir, ".gnubgrc", NULL);
  g_rename (oldfile, newfile);
  g_free (oldfile);
  g_free (newfile);

}

/*
 * Create $HOME/.gnubg if not existing
 *
 */

static int
CreateGnubgDirectory (void)
{
  char *newfile;

  if (!g_file_test (szHomeDirectory, G_FILE_TEST_IS_DIR))
    {
      if (g_mkdir (szHomeDirectory, 0700) < 0)
	{
	  outputerr (szHomeDirectory);
	  return -1;
	}
    }
  newfile = g_build_filename (szHomeDirectory, "gnubgautorc", NULL);
  if (!g_file_test (newfile, G_FILE_TEST_IS_REGULAR))
    move_rc_files ();
  g_free (newfile);
  return 0;
}


extern RETSIGTYPE HandleInterrupt( int idSignal )
{

    /* NB: It is safe to write to fInterrupt even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    fInterrupt = TRUE;
}

#if USE_GTK  && defined(SIGIO)
static RETSIGTYPE HandleIO( int idSignal ) {
    /* NB: It is safe to write to fAction even if it cannot be read
       atomically, because it is only used to hold a binary value. */
    if( fX )
	fAction = TRUE;
}
#endif

static void BearoffProgress( unsigned int i )
{
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

static void version(void)
{

	char *pch;
	g_print(_(VERSION_STRING));
	g_print("\n%s\n", _(aszCOPYRIGHT));
	g_print(_("GNU Backgammon is free software, covered by the GNU "
		 "General Public License\n"
		 "version 3 or later, and you are welcome to change it and/or "
		 "distribute copies of it\n"
		 "under certain conditions.  Type \"show copying\" to see "
		 "the conditions.\n"
		 "There is absolutely no warranty for GNU Backgammon.  "
		 "Type \"show warranty\" for\n" "details.\n\n"));

	while ((pch = GetBuildInfoString()))
		g_print("%s\n", gettext(pch));
}

#if HAVE_LIBREADLINE
static char *get_readline(void)
{
	char *pchExpanded;
	char *szInput;
	char *sz;
	char *prompt = locale_from_utf8(FormatPrompt());
	while (!(szInput = readline(prompt))) {
		outputc('\n');
		PromptForExit();
	}
	g_free(prompt);
	sz = locale_to_utf8(szInput);
	free(szInput);
	fInterrupt = FALSE;
	history_expand(sz, &pchExpanded);
	g_free(sz);
	if (*pchExpanded)
		add_history(pchExpanded);
	return pchExpanded;
}
#endif

static char *get_stdin_line(void)
{
	char sz[2048], *pch;

	sz[0] = 0;
	Prompt();

	clearerr(stdin);

	/* FIXME shouldn't restart sys calls on signals during this
	   fgets */
	fgets(sz, sizeof(sz), stdin);

	if ((pch = strchr(sz, '\n')))
		*pch = 0;


	if (feof(stdin)) {
		if (!isatty(STDIN_FILENO)) {
			Shutdown();
			exit(EXIT_SUCCESS);
		}

		outputc('\n');

		if (!sz[0])
			PromptForExit();
		return NULL;
	}

	fInterrupt = FALSE;
    return g_strdup(sz);
}



static void run_cl(void)
{
	char *line;
	for (;;) {
#if HAVE_LIBREADLINE
		if (fInteractive) {
			line = get_readline();
			HandleCommand(line, acTop);
			free(line);
		} else
#endif
		{
			line = get_stdin_line();
			HandleCommand(line, acTop);
			g_free(line);
		}
		while (fNextTurn)
			NextTurn(TRUE);
		ResetInterrupt();
	}
}
static void init_language(char **lang)
{
	char *szFile, szTemp[4096];
	char *pch;
	FILE *pf;

	outputoff();
	szFile = g_build_filename(szHomeDirectory, "gnubgautorc", NULL);

	if (!*lang && (pf = g_fopen(szFile, "r"))) {

		for (;;) {

			szTemp[0] = 0;
			fgets(szTemp, sizeof(szTemp), pf);

			if ((pch = strchr(szTemp, '\n'))) *pch = 0;
			if ((pch = strchr(szTemp, '\r'))) *pch = 0;

			if (ferror(pf)) {
				outputerr(szFile);
				break;
			}

			if (feof(pf)) {
				break;
			}

			if (!strncmp("set lang", szTemp, 8)) {
				*lang = g_strdup(szTemp+9);
				break;
			}
		}

		fclose(pf);
	}
	g_free(szFile);
	CommandSetLang(*lang);
	g_free(*lang);
	outputon();
}

static void setup_readline(void)
{
#if HAVE_LIBREADLINE
	char *pch;
	int i;
	gnubg_histfile = g_build_filename(szHomeDirectory, "history", NULL);
	rl_readline_name = "gnubg";
	rl_basic_word_break_characters =
	    rl_filename_quote_characters = szCommandSeparators;
	rl_completer_quote_characters = "\"'";
	/* assume readline 4.2 or later */
	rl_completion_entry_function = NullGenerator;
	rl_attempted_completion_function = CompleteKeyword;
	/* setup history */
	read_history(gnubg_histfile);
	using_history();
	if ((pch = getenv("HISTSIZE")) && ((i = atoi(pch)) > 0))
		stifle_history(i);
#endif
}

#ifndef USE_GTK
static void PushSplash(char *unused, char *heading, char *message, int wait)
{
}
#endif

static void init_nets(int nNewWeights, int fNoBearoff)
{
	char *gnubg_weights = BuildFilename("gnubg.weights");
	char *gnubg_weights_binary =  BuildFilename("gnubg.wd");
	EvalInitialise(gnubg_weights, gnubg_weights_binary, fNoBearoff, 
			   fShowProgress ? BearoffProgress : NULL);
	g_free(gnubg_weights);
	g_free(gnubg_weights_binary);
}

static int GetManualDice(unsigned int anDice[2])
{

	char *pz;
	char *sz = NULL;
	int i;

#if USE_GTK
	if (fX) {
		if (GTKGetManualDice(anDice)) {
			fInterrupt = 1;
			return -1;
		} else
			return 0;
	}
#endif

	for (;;) {
	      TryAgain:
		if (fInterrupt) {
			anDice[0] = anDice[1] = 0;
			return -1;
		}

		sz = GetInput(_("Enter dice: "));

		if (fInterrupt) {
			g_free(sz);
			anDice[0] = anDice[1] = 0;
			return -1;
		}

		/* parse input and read a couple of dice */
		/* any string with two numbers is allowed */

		pz = sz;

		for (i = 0; i < 2; i++) {
			while (*pz && ((*pz < '1') || (*pz > '6')))
				pz++;

			if (!*pz) {
				outputl(_
					("You must enter two numbers between 1 and 6."));
				goto TryAgain;
			}

			anDice[i] = (int) (*pz - '0');
			pz++;
		}

		g_free(sz);
		return 0;
	}
}

/* 
 * Fetch random numbers from www.random.org
 *
 */

#if HAVE_SOCKETS

static int getDiceRandomDotOrg(void)
{

#define BUFLENGTH 500

	static int nCurrent = -1;
	static int anBuf[BUFLENGTH];
	static int nRead;


	int h;
	int cb;

	int nBytesRead, i;
	struct sockaddr *psa;
	char szHostname[80];
	char szHTTP[] =
	    "GET http://www.random.org/cgi-bin/randnum?num=500&min=0&max=5&col=1\n";
	char acBuf[2048];

	/* 
	 * Suggestions for improvements:
	 * - use proxy
	 */

	/*
	 * Return random number
	 */

	if ((nCurrent >= 0) && (nCurrent < nRead))
		return anBuf[nCurrent++];
	else {

		outputf(_
			("Fetching %d random numbers from <www.random.org>\n"),
			BUFLENGTH);
		outputx();

		/* fetch new numbers */

		/* open socket */

		strcpy(szHostname, "www.random.org:80");

		if ((h = ExternalSocket(&psa, &cb, szHostname)) < 0) {
			SockErr(szHostname);
			return -1;
		}

		/* connect */

#ifdef WIN32
		if (connect((SOCKET) h, (const struct sockaddr *) psa, cb)
		    < 0) {
#else
		if ((connect(h, psa, cb)) < 0) {
#endif				/* WIN32 */
			SockErr(szHostname);
			return -1;
		}

		/* read next set of numbers */

		if (ExternalWrite(h, szHTTP, strlen(szHTTP) + 1) < 0) {
			SockErr(szHTTP);
			closesocket(h);
			return -1;
		}

		/* read data from web-server */

#ifdef WIN32
		/* reading from sockets doesn't work on Windows
		   use recv instead */
		if (!
		    (nBytesRead =
		     recv((SOCKET) h, acBuf, sizeof(acBuf), 0))) {
#else
		if (!(nBytesRead = read(h, acBuf, sizeof(acBuf)))) {
#endif
			SockErr("reading data");
			closesocket(h);
			return -1;
		}

		/* close socket */
		closesocket(h);

		/* parse string */
		outputl(_("Done."));
		outputx();

		i = 0;
		nRead = 0;
		for (i = 0; i < nBytesRead; i++) {

			if ((acBuf[i] >= '0') && (acBuf[i] <= '5')) {
				anBuf[nRead] = 1 + (int) (acBuf[i] - '0');
				nRead++;
			}

		}

		nCurrent = 1;
		return anBuf[0];
	}

}

#else
static int (*getRandomDiceDotOrg)(void) = NULL;
#endif /* HAVE_SOCKETS */

static void init_rng(void)
{
	if (!(rngctxCurrent = InitRNG(NULL, NULL, TRUE, rngCurrent))) {
		printf(_("Failure setting up RNG\n"));
		exit(EXIT_FAILURE);
	}
	if (!(rngctxRollout = InitRNG(&rcRollout.nSeed, NULL,
				      TRUE, rcRollout.rngRollout))) {
		printf(_("Failure setting up RNG for rollout.\n"));
		exit(EXIT_FAILURE);
	}

	/* we don't want rollouts to use the same seed as normal dice (which
	   could happen if InitRNG had to use the current time as a seed) -- mix
	   it up a little bit */
	rcRollout.nSeed ^= 0x792A584B;

	dice_init_callback(getDiceRandomDotOrg, GetManualDice);
}

#if defined(WIN32) && HAVE_SOCKETS
static void init_winsock()
{
		short wVersionRequested;
		WSADATA wsaData;
		wVersionRequested = MAKEWORD(1, 1);
		if (WSAStartup(wVersionRequested, &wsaData) != 0) {
			outputerr("Windows sockets initialisation");
		}
}
#endif

static char *matchfile_from_argv(char *sz)
{
	char *pchMatch;
#ifdef WIN32
	if (g_path_is_absolute(sz))
		pchMatch = g_strdup_printf("'%s'", sz);
	else {
		char *tmp =
		    g_build_filename(g_get_current_dir(), sz, NULL);
		pchMatch = g_strdup_printf("'%s'", tmp);
		g_free(tmp);
	}
#else
	pchMatch = g_strdup_printf("'%s'", sz);
#endif
	return pchMatch;
}

static void init_defaults(void)
{
	/* init some html export options */

	exsExport.szHTMLPictureURL = g_strdup("html-images/");
	exsExport.szHTMLExtension = g_strdup("png");

	SetMatchDate(&mi);

	strcpy(ap[1].szName, g_get_user_name());

	ListCreate(&lMatch);
	IniStatcontext(&scMatch);

	szHomeDirectory =
	    g_build_filename(g_get_home_dir(), ".gnubg", NULL);

}

static void null_debug (const gchar* dom, GLogLevelFlags logflags, const gchar* message, gpointer unused)
{
}

int main(int argc, char *argv[])
{
#if USE_GTK
	GtkWidget *pwSplash = NULL;
#else
        char *pwSplash = NULL;
#endif
	char *pchMatch = NULL;
	char *met = NULL;

	static char *pchCommands = NULL, *pchPythonScript = NULL, *lang = NULL;
	static int nNewWeights = 0, fNoRC = FALSE, fNoBearoff = FALSE, fQuiet =
	    FALSE, fNoX = FALSE, fSplash = FALSE, fNoTTY =
	    FALSE, show_version = FALSE, debug = FALSE;

	GOptionEntry ao[] = {
		{"no-bearoff", 'b', 0, G_OPTION_ARG_NONE, &fNoBearoff,
		 N_("Do not use bearoff database"), NULL},
		{"commands", 'c', 0, G_OPTION_ARG_FILENAME, &pchCommands,
		 N_("Evaluate commands in FILE and exit"), "FILE"},
		{"lang", 'l', 0, G_OPTION_ARG_STRING, &lang,
		 N_("Set language to LANG"), "LANG"},
		{"new-weights", 'n', 0, G_OPTION_ARG_INT, &nNewWeights,
		 N_("Create new neural net (of size N)"), "N"},
		{"python", 'p', 0, G_OPTION_ARG_FILENAME, &pchPythonScript,
		 N_("Evaluate Python code in FILE and exit"), "FILE"},
		{"quiet", 'q', 0, G_OPTION_ARG_NONE, &fQuiet,
		 N_("Disable sound effects"), NULL},
		{"no-rc", 'r', 0, G_OPTION_ARG_NONE, &fNoRC,
		 N_("Do not read .gnubgrc and .gnubgautorc commands"),
		 NULL},
		{"splash", 'S', 0, G_OPTION_ARG_NONE, &fSplash,
		 N_("Don't show gtk splash screen"), NULL},
		{"tty", 't', 0, G_OPTION_ARG_NONE, &fNoX,
		 N_("Start on tty instead of using window system"), NULL},
		{"version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
		 N_("Show version information and exit"), NULL},
		{"window-system-only", 'w', 0, G_OPTION_ARG_NONE, &fNoTTY,
		 N_("Ignore tty input when using window system"), NULL},
		{"debug", 'd', 0, G_OPTION_ARG_NONE, &debug,
		 N_("Turn on debug"), NULL},
		{NULL, 0, 0, 0, NULL, NULL, NULL}
	};
	GError *error = NULL;
	GOptionContext *context;

        
	/* set language */
        init_defaults();
#if USE_GTK
	gtk_disable_setlocale();
#endif
	init_language(&lang);
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	bind_textdomain_codeset(PACKAGE, GNUBG_CHARSET);

        /* parse command line options*/
	context = g_option_context_new("[file.sgf]");
	g_option_context_add_main_entries(context, ao, PACKAGE);
#if USE_GTK
	g_option_context_add_group(context, gtk_get_option_group(FALSE));
#endif
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);
	if (error) {
		outputerrf("%s\n", error->message);
		exit(EXIT_FAILURE);
	}
	if (argc > 1 && *argv[1])
		pchMatch = matchfile_from_argv(argv[1]);

	if (!debug)
		g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, &null_debug, NULL);

#ifdef WIN32
	/* data directory: initialise to the path where gnubg is installed */
	{
	const char *szDataDirectory = getInstallDir();
	_chdir(szDataDirectory);
	}
#if defined(_MSC_VER) && HAVE_LIBXML2
	xmlMemSetup(g_free, g_malloc, g_realloc, g_strdup);
#endif
#endif

       /* print version and exit if -v option given */
        version();
	if (show_version)
		exit(EXIT_SUCCESS);

        /* -q option given */
	if (fQuiet)
		fSound = FALSE;

	if (CreateGnubgDirectory())
		exit(EXIT_FAILURE);

#if WIN32
        fNoTTY = TRUE;
#endif
#if USE_GTK
        /* -t option not given*/
	if (!fNoX)
		InitGTK(&argc, &argv);
        if (fX)
        {
                fTTY = !fNoTTY;
                fInteractive = fShowProgress = TRUE;
                if (fSplash)
                        pwSplash = CreateSplash();
#if defined(SIGIO)
                PortableSignal(SIGIO, HandleIO, NULL, TRUE);
#endif
	} else
#endif
	{
		fInteractive = isatty(STDIN_FILENO);
		fShowProgress = isatty(STDOUT_FILENO);
	}

        if (fInteractive) {
                PortableSignal(SIGINT, HandleInterrupt,
                                &shInterruptOld, FALSE);
                setup_readline();
        }
	fnTick = CallbackProgress;

	PushSplash(pwSplash, _("Initialising"), _("Random number generator"), 250);
        init_rng();

	PushSplash(pwSplash, _("Initialising"), _("match equity table"), 250);
	met = BuildFilename2("met", "g11.xml");
	InitMatchEquity(met);
	g_free(met);

#if USE_MULTITHREAD
	PushSplash(pwSplash, _("Initialising"), _("threads"), 250);
	MT_InitThreads();
#endif
	PushSplash(pwSplash, _("Initialising"), _("neural nets"), 250);
        init_nets(nNewWeights, fNoBearoff);

#if defined(WIN32) && HAVE_SOCKETS
	PushSplash(pwSplash, _("Initialising"), _("Windows sockets"), 250);
        init_winsock();
#endif

#if USE_PYTHON
	PushSplash(pwSplash, _("Initialising"), _("Python"), 250);
	PythonInitialise();
#endif

	PushSplash(pwSplash, _("Initialising"), _("Board Images"), 250);
	RenderInitialise();

	SetExitSoundOff();

        /* -r option given */
        if (!fNoRC)
        {
                PushSplash(pwSplash, _("Loading"), _("User Settings"), 250);
                LoadRCFiles();
        }

	fflush(stdout);
	fflush(stderr);

#if USE_MULTITHREAD
	/* Make sure threads started */
	MT_StartThreads();
#endif

	/* start-up sound */
	playSound(SOUND_START);

#if USE_GTK
	if (fX)
        {
		if (!fTTY)
		{
			g_set_print_handler(&GTKOutput);
			g_set_printerr_handler(&GTKOutputErr);
		}
		RunGTK(pwSplash, pchCommands, pchPythonScript, pchMatch);
                Shutdown();
                exit(EXIT_SUCCESS);
        }
#endif

	if (pchMatch)
		CommandLoadMatch(pchMatch);

        /* -c option given */
	if (pchCommands) {
		fInteractive = FALSE;
		CommandLoadCommands(pchCommands);
		Shutdown();
		exit(EXIT_SUCCESS);
	}

        /* -p option given */
	if (pchPythonScript) {
#if USE_PYTHON
		fInteractive = FALSE;
		CommandLoadPython(pchPythonScript);
		Shutdown();
		exit(EXIT_SUCCESS);
#else
		outputerrf(_("GNU Backgammon build without Python.\n"));
		exit(EXIT_FAILURE);
#endif				/* USE_PYTHON */
	}
	run_cl();
	return (EXIT_FAILURE);
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

extern void CommandEq2MWC ( char *sz )
{

  float rEq;
  cubeinfo ci;

  if ( ! ms.nMatchTo ) {
    outputl ( _("Command only valid in match play") );
    return;
  }


  rEq = (float)ParseReal ( &sz );

  if ( rEq == ERR_VAL ) rEq = 0.0;

  GetMatchStateCubeInfo( &ci, &ms );

  outputf ( _("MWC for equity = %+6.3f: %6.2f%%\n"),
            -1.0, 100.0 * eq2mwc ( -1.0, &ci ) );
  outputf ( _("MWC for equity = %+6.3f: %6.2f%%\n"),
            +1.0, 100.0 * eq2mwc ( +1.0, &ci ) );
  outputf ( _("By linear interpolation:\n") );
  outputf ( _("MWC for equity = %+6.3f: %6.2f%%\n"),
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

extern void CommandMWC2Eq ( char *sz )
{

  float rMwc;
  cubeinfo ci;

  if ( ! ms.nMatchTo ) {
    outputl ( _("Command only valid in match play") );
    return;
  }

  GetMatchStateCubeInfo( &ci, &ms );

  rMwc = (float)ParseReal ( &sz );

  if ( rMwc == ERR_VAL ) rMwc = eq2mwc ( 0.0, &ci );

  if ( rMwc > 1.0 ) rMwc /= 100.0;

  outputf ( _("Equity for MWC = %6.2f%%: %+6.3f\n"),
            100.0 * eq2mwc ( -1.0, &ci ), -1.0 );
  outputf ( _("Equity for MWC = %6.2f%%: %+6.3f\n"),
            100.0 * eq2mwc ( +1.0, &ci ), +1.0 );
  outputf ( _("By linear interpolation:\n") );
  outputf ( _("Equity for MWC = %6.2f%%: %+6.3f\n"),
            100.0 * rMwc, mwc2eq ( rMwc, &ci ) );


}

static void 
swapGame ( listOLD *plGame ) {

  listOLD *pl;
  moverecord *pmr;
  int n;

  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
      
    pmr = pl->p;

    switch ( pmr->mt ) {
    case MOVE_GAMEINFO:

      /* swap score */

      n = pmr->g.anScore[ 0 ];
      pmr->g.anScore[ 0 ] = pmr->g.anScore[ 1 ];
      pmr->g.anScore[ 1 ] = n;

      /* swap winner */

      if ( pmr->g.fWinner > -1 )
        pmr->g.fWinner = ! pmr->g.fWinner;

      /* swap statcontext */

      /* recalculate statcontext later ... */

      break;

    case MOVE_DOUBLE:
    case MOVE_TAKE:
    case MOVE_DROP:
    case MOVE_NORMAL:
    case MOVE_RESIGN:
    case MOVE_SETDICE:

      pmr->fPlayer = ! pmr->fPlayer;
      break;

    case MOVE_SETBOARD:
    case MOVE_SETCUBEVAL:

      /*no op */
      break;

    case MOVE_SETCUBEPOS:

      if ( pmr->scp.fCubeOwner > -1 )
        pmr->scp.fCubeOwner = ! pmr->scp.fCubeOwner;
      break;

    }

  }

}



extern void CommandSwapPlayers ( char *sz )
{

  listOLD *pl;
  char *pc;
  int n;

  /* swap individual move records */

  for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext ) {

    swapGame ( pl->p );

  }

  /* swap player names */

  pc = g_strdup ( ap[ 0 ].szName );
  strcpy ( ap[ 0 ].szName, ap[ 1 ].szName );
  strcpy ( ap[ 1 ].szName, pc );
  free ( pc );

  /* swap player ratings */

  pc = mi.pchRating[ 0 ];
  mi.pchRating[ 0 ] = mi.pchRating[ 1 ];
  mi.pchRating[ 1 ] = pc;

  /* swap current matchstate */

  if ( ms.fTurn > -1 )
    ms.fTurn = ! ms.fTurn;
  if ( ms.fMove > -1 )
    ms.fMove = ! ms.fMove;
  if ( ms.fCubeOwner > -1 )
    ms.fCubeOwner = ! ms.fCubeOwner;
  n = ms.anScore[ 0 ];
  ms.anScore[ 1 ] = ms.anScore[ 0 ];
  ms.anScore[ 0 ] = n;
  SwapSides ( ms.anBoard );


#if USE_GTK
  if ( fX ) {
    GTKSet ( ap );
    GTKRegenerateGames();
    GTKUpdateAnnotations();
  }
#endif

  ShowBoard();

}


extern int
confirmOverwrite ( const char *sz, const int f ) {

  char *szPrompt;
  int i;

  /* check for existing file */

  if ( f && ! access ( sz, F_OK ) ) {

    szPrompt = (char *) malloc ( 50 + strlen ( sz ) );
    sprintf ( szPrompt, _("File \"%s\" exists. Overwrite? "), sz );
    i = GetInputYN ( szPrompt );
    free ( szPrompt );
    return i;

  }
  else
    return TRUE;


}

extern void
setDefaultFileName (char *path)
{
  g_free (szCurrentFolder);
  g_free (szCurrentFileName);
  DisectPath (path, NULL, &szCurrentFileName, &szCurrentFolder);
#if USE_GTK
  if (fX)
    {
      gchar *title =
	g_strdup_printf (_("GNU Backgammon (%s)"), szCurrentFileName);
      gtk_window_set_title (GTK_WINDOW (pwMain), title);
      g_free (title);
    }
#endif
}

extern void
DisectPath (const char *path, const char *extension, char **name, char **folder)
{
  char *fnn, *pc;
  if (!path)
  {
	  *folder = NULL;
	  *name = NULL;
	  return;
  }
  *folder = g_path_get_dirname (path);
  fnn = g_path_get_basename (path);
  pc = strrchr (fnn, '.');
  if (pc)
    *pc = '\0';
  *name = g_strconcat (fnn, extension, NULL);
  g_free (fnn);
}


extern void
InvalidateStoredMoves ( void ) {

  sm.ms.nMatchTo = -1;

}


extern void
InvalidateStoredCube ( void ) {

  sc.ms.nMatchTo = -1;

}


extern void
UpdateStoredMoves ( const movelist *pml, const matchstate *pms ) {

  if( sm.ml.amMoves )
    free( sm.ml.amMoves );

  CopyMoveList ( &sm.ml, pml );

  sm.ms = *pms;

}


extern void
UpdateStoredCube ( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                   const evalsetup *pes,
                   const matchstate *pms ) {

  memcpy ( sc.aarOutput, aarOutput, 
           2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

  memcpy ( sc.aarStdDev, aarStdDev, 
           2 * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

  sc.ms = *pms;
  sc.es = *pes;

}

/* ask for confirmation if this is a sub-optimal play 
 * returns TRUE if player wants to re-think the move
 */

static int GetAdviceAnswer( char *sz ) {

  char	*pch;
#if USE_GTK
  if( fX )
	return GtkTutor ( sz );
#endif

    if( fInterrupt )
    return FALSE;

    while( 1 ) {
	pch = GetInput( sz );

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
	
	outputl( _("Please answer `y' or `n'.") );
    }
}

extern int GiveAdvice( skilltype Skill )
{

  char *sz;

  /* should never happen */
  if ( !fTutor )
	return FALSE;
  
	switch (Skill) {

	case SKILL_VERYBAD:
	  sz = _( "You may be about to make a very bad play. Are you sure? " );
	  break;

	case SKILL_BAD:
	  sz = _( "You may be about to make a bad play. Are you sure? " );
	  break;
	  
	case SKILL_DOUBTFUL:
	  sz = _( "You may be about to make a doubtful play. Are you sure? " );
	  break;

	default:
	  return (TRUE);
	}

	if (Skill > TutorSkill)
	  return (TRUE);

	return GetAdviceAnswer( sz );
}

extern void TextToClipboard(const char *sz)
{
#if USE_GTK
  if (fX)
    GTKTextToClipboard(sz);
#else
  /* no clipboard: just write string */
  outputl(sz);
#endif
}

void 
CommandDiceRolls (char *sz) {
  int    n;
  char	*pch;
  unsigned int	 anDice[2];

  if ( (pch = NextToken( &sz ) ) ) {
    n = ParseNumber( &pch );

    while (n-- > 0) {
      RollDice( anDice, rngCurrent, rngctxCurrent );

      printf( "%d %d\n", anDice[ 0 ], anDice[ 1 ] );

    }

  }
}


#if HAVE_LIBREADLINE
extern void
CommandHistory( char *sz ) {

  int i;
  HIST_ENTRY *phe;

  for ( i = 0; i < history_length; ++i ) {
    phe = history_get( i + history_base );
    outputf( "%6d %s\n", i + history_base, phe->line );
  }
  

}

#endif /* HAVE_LIBREADLINE */

extern void
CommandClearHint( char *sz ) {

  InvalidateStoredMoves();
  InvalidateStoredCube();

  outputl( _("Analysis used for `hint' has been cleared") );

}


/*
 * Description:  Calculate Effective Pip Counts (EPC), either
 *               by lookup in one-sided databases or by doing a
 *               one-sided rollout.  
 * Parameters :  
 *   Input       anBoard (the board)
 *               fOnlyRace: only calculate EPCs for race positions
 *   Output      arEPC (the calculate EPCs)
 *               pfSource (source of EPC; 0 = database, 1 = OSR)
 *
 * Returns:      0 on success, non-zero otherwise 
 *               
 */

extern int
EPC( const TanBoard anBoard, float *arEPC, float *arMu, float *arSigma, 
     int *pfSource, const int fOnlyBearoff ) {

  const float x = ( 2 * 3 + 3 * 4 + 4 * 5 + 4 * 6 + 6 * 7 +
                    5* 8  + 4 * 9 + 2 * 10 + 2 * 11 + 1 * 12 + 
                    1 * 16 + 1 * 20 + 1 * 24 ) / 36.0f;

  if ( isBearoff ( pbc1, anBoard ) ) {
    /* one sided in-memory database */
    float ar[ 4 ];
    unsigned int n;
    int i;

    for ( i = 0; i < 2; ++i ) {
      n = PositionBearoff( anBoard[ i ], pbc1->nPoints, pbc1->nChequers );

      if ( BearoffDist( pbc1, n, NULL, NULL, ar, NULL, NULL ) )
        return -1;

      if ( arEPC )
        arEPC[ i ] = x * ar[ 0 ];
      if ( arMu )
        arMu[ i ] = ar[ 0 ];
      if ( arSigma )
        arSigma[ i ] = ar[ 1 ];

    }

    if ( pfSource )
      *pfSource = 0;

    return 0;

  }
  else if ( isBearoff( pbcOS, anBoard ) ) {
    /* one sided in-memory database */
    float ar[ 4 ];
    unsigned int n;
    int i;

    for ( i = 0; i < 2; ++i ) {
      n = PositionBearoff( anBoard[ i ], pbcOS->nPoints, pbcOS->nChequers );

      if ( BearoffDist( pbcOS, n, NULL, NULL, ar, NULL, NULL ) )
        return -1;

      if ( arEPC )
        arEPC[ i ] = x * ar[ 0 ];
      if ( arMu )
        arMu[ i ] = ar[ 0 ];
      if ( arSigma )
        arSigma[ i ] = ar[ 1 ];

    }

    if ( pfSource )
      *pfSource = 0;

    return 0;

  }
  else if ( ! fOnlyBearoff ) {

    /* one-sided rollout */

    int nTrials = 5760;
    float arMux[ 2 ];
    float ar[ 5 ];
    int i;

    raceProbs ( anBoard, nTrials, ar, arMux );

    for ( i = 0; i < 2; ++i ) {
      if ( arEPC )
        arEPC[ i ] = x * arMux[ i ];
      if ( arMu )
        arMu[ i ] = arMux[ i ];
      if ( arSigma )
        arSigma[ i ] = -1.0f; /* raceProbs cannot calculate sigma! */
    }

    if ( pfSource )
      *pfSource = 1;

    return 0;

  }

  /* code not reachable */
  return -1;
}

#if HAVE_LIBREADLINE
extern char *locale_from_utf8(const char *sz)
{
	char *ret;
	GError *error = NULL;
	g_assert(sz);
	ret = g_locale_from_utf8(sz, strlen(sz), NULL, NULL, &error);
	if (error) {
		g_print("locale_from_utf8 failed: %s\n", error->message);
		g_error_free(error);
		ret = g_strdup(sz);
	}
	return ret;
}
#endif
extern char *locale_to_utf8(const char *sz)
{
	char *ret;
	GError *error = NULL;
	g_assert(sz);
	ret = g_locale_to_utf8(sz, strlen(sz), NULL, NULL, &error);
	if (error) {
		g_print("locale_to_utf8 failed: %s\n", error->message);
		g_error_free(error);
		ret = g_strdup(sz);
	}
	return ret;
}
#ifdef WIN32 
/* WIN32 setlocale must be manipulated through putenv to be gettext compatible */
char * SetupLanguage (char *newLangCode)
{
	static char *org_lang=NULL;
	char *lang;
	if (!org_lang)
		org_lang = g_win32_getlocale();
	if (!newLangCode || !strcmp (newLangCode, "system") || !strcmp (newLangCode, ""))
		lang = g_strdup_printf("LANG=%s", org_lang);
	else
		lang = g_strdup_printf("LANG=%s", newLangCode);
	putenv(lang); 	 
	g_free(lang);
	return(setlocale(LC_ALL, ""));
}
#else
char *SetupLanguage (char *newLangCode)
{
	static char *org_lang=NULL;
	char *result=NULL;

	if (!org_lang)
	{
		if (!(org_lang=g_strdup(setlocale(LC_ALL, ""))))
			org_lang="C";
	}

	if (newLangCode && *newLangCode && strcmp (newLangCode, "system") != 0)
	{
		g_setenv("LC_ALL", newLangCode, TRUE);
		result = setlocale (LC_ALL, newLangCode);
	}

	if (!result)
	{
		g_setenv("LC_ALL", org_lang, TRUE);
		result = setlocale (LC_ALL, org_lang);
	}
	return(result);
}
#endif

