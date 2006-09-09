/*
 * gtkgame.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001, 2002.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if USE_BOARD3D
#include "board3d/inc3d.h"
#endif

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <assert.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtktext.h>

#if HAVE_STROPTS_H
#include <stropts.h>  /* I_SETSIG, S_RDNORM under solaris */
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include <glib.h>

#include "analysis.h"
#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "gtkchequer.h"
#include "gtkcube.h"
#include "gtkgame.h"
#include "gtkmet.h"
#include "gtkmovefilter.h"
#include "gtkprefs.h"
#include "gtksplash.h"
#include "gtktexi.h"
#include <glib/gi18n.h>
#include "matchequity.h"
#include "openurl.h"
#include "path.h"
#include "positionid.h"
#include "record.h"
#include "sound.h"
#include "gtkoptions.h"
#include "gtktoolbar.h"
#include "format.h"
#include "formatgs.h"
#include "renderprefs.h"
#include "credits.h"
#include "matchid.h"

#if USE_TIMECONTROL
#include "timecontrol.h"
#endif

#define KEY_ESCAPE -229
#define KEY_TAB -247

/* Offset action to avoid predefined values */
#define TOOLBAR_ACTION_OFFSET 10000
#define MENU_OFFSET 50

#define GNUBGMENURC ".gnubgmenurc"

void DockPanels();

#if !HAVE_GTK_OPTION_MENU_GET_HISTORY
extern gint gtk_option_menu_get_history (GtkOptionMenu *option_menu) {
    
    GtkWidget *active_widget;
  
    g_return_val_if_fail (GTK_IS_OPTION_MENU (option_menu), -1);

    if (option_menu->menu) {
	active_widget = gtk_menu_get_active (GTK_MENU (option_menu->menu));

	if (active_widget)
	    return g_list_index (GTK_MENU_SHELL (option_menu->menu)->children,
				 active_widget);
	else
	    return -1;
    } else
	return -1;
}
#endif

#if USE_PYTHON
static void ClearText(GtkTextView* pwText)
{
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(pwText), "", -1);
}

static char* GetText(GtkTextView* pwText)
{
  GtkTextIter start, end;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(pwText);
	char *pch;

  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
	pch = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  return pch;
}
#endif

#define GTK_STOCK_DIALOG_GNU "gtk-dialog-gnu" /* stock gnu head icon */

struct CommandEntryData_T cedDialog, cedPanel;

char* warningStrings[WARN_NUM_WARNINGS] =
{
	N_("Press escape to exit full screen mode"),
	N_("This option will speed up the 3d drawing, but may not work correctly on all machines"),
	N_("Drawing shadows is only supported on the latest graphics cards\n"
		"Disable this option if performance is poor"),
	N_("No hardware accelerated graphics card found, performance may be slow")
};

char* warningNames[WARN_NUM_WARNINGS] =
{"fullscreenexit", "quickdraw", "shadows", "unaccelerated"};

int warningEnabled[WARN_NUM_WARNINGS] = {TRUE, TRUE, TRUE, TRUE};

/* Enumeration to be used as index to the table of command strings below
   (since GTK will only let us put integers into a GtkItemFactoryEntry,
   and that might not be big enough to hold a pointer).  Must be kept in
   sync with the string array! */
typedef enum _gnubgcommand {
    CMD_AGREE,
    CMD_ANALYSE_CLEAR_MOVE,
    CMD_ANALYSE_CLEAR_GAME,
    CMD_ANALYSE_CLEAR_MATCH,
    CMD_ANALYSE_CLEAR_SESSION,
    CMD_ANALYSE_MOVE,
    CMD_ANALYSE_GAME,
    CMD_ANALYSE_MATCH,
    CMD_ANALYSE_SESSION,
    CMD_DECLINE,
    CMD_DOUBLE,
    CMD_DROP,
    CMD_EVAL,
    CMD_HELP,
    CMD_HINT,
    CMD_LIST_GAME,
    CMD_NEW_GAME,
    CMD_NEW_SESSION,
    CMD_NEXT,
    CMD_NEXT_GAME,
    CMD_NEXT_MARKED,
    CMD_NEXT_ROLL,
    CMD_NEXT_ROLLED,
    CMD_PLAY,
    CMD_PREV,
    CMD_PREV_GAME,
    CMD_PREV_MARKED,
    CMD_PREV_ROLL,
    CMD_PREV_ROLLED,
    CMD_QUIT,
    CMD_RECORD_ADD_GAME,
    CMD_RECORD_ADD_MATCH,
    CMD_RECORD_ADD_SESSION,
    CMD_RECORD_SHOW,
    CMD_RELATIONAL_ADD_MATCH,
    CMD_RELATIONAL_TEST,
    CMD_RELATIONAL_HELP,
    CMD_REDOUBLE,
    CMD_RESIGN_N,
    CMD_RESIGN_G,
    CMD_RESIGN_B,
    CMD_ROLL,
    CMD_ROLLOUT,
    CMD_ROLLOUT_CUBE,
    CMD_SAVE_SETTINGS,
    CMD_SET_ANNOTATION_ON,
    CMD_SET_APPEARANCE,
    CMD_SET_MESSAGE_ON,
    CMD_SET_TURN_0,
    CMD_SET_TURN_1,
    CMD_SHOW_BEAROFF,
    CMD_SHOW_CALIBRATION,
    CMD_SHOW_COPYING,
    CMD_SHOW_ENGINE,
    CMD_SHOW_EPC,
    CMD_SHOW_EXPORT,
    CMD_SHOW_GAMMONVALUES,
    CMD_SHOW_MARKETWINDOW,
    CMD_SHOW_MATCHEQUITYTABLE,
    CMD_SHOW_KLEINMAN,
    CMD_SHOW_MANUAL_GUI,
    CMD_SHOW_MANUAL_WEB,
    CMD_SHOW_ONECHEQUER,
    CMD_SHOW_ONESIDEDROLLOUT,
    CMD_SHOW_PATH,
    CMD_SHOW_PIPCOUNT,
    CMD_SHOW_ROLLS,
    CMD_SHOW_STATISTICS_GAME,
    CMD_SHOW_STATISTICS_MATCH,
    CMD_SHOW_STATISTICS_SESSION,
    CMD_SHOW_TEMPERATURE_MAP,
    CMD_SHOW_TEMPERATURE_MAP_CUBE,
    CMD_SHOW_THORP,
    CMD_SHOW_VERSION,
    CMD_SHOW_WARRANTY,
    CMD_SWAP_PLAYERS,
    CMD_TAKE,
    CMD_TRAIN_DATABASE,
    CMD_TRAIN_TD,
#if USE_TIMECONTROL
    CMD_SET_TC_OFF,
    CMD_SET_TC,
#endif
    NUM_CMDS
} gnubgcommand;
   

static char *aszCommands[ NUM_CMDS ] = {
    "agree",
    "analyse clear move",
    "analyse clear game",
    "analyse clear match",
    "analyse clear session",
    "analyse move",
    "analyse game",
    "analyse match",
    "analyse session",
    "decline",
    "double",
    "drop",
    "eval",
    "help",
    "hint",
    "list game",
    "new game",
    "new session",
    "next",
    "next game",
    "next marked",
    "next roll",
	"next rolled",
    "play",
    "previous",
    "previous game",
    "previous marked",
    "previous roll",
	"previous rolled",
    "quit",
    "record add game",
    "record add match",
    "record add session",
    "record show",
    "relational add match",
    "relational test",
    "relational help",
    "redouble",
    "resign normal",
    "resign gammon",
    "resign backgammon",
    "roll",
    "rollout",
    "rollout =cube",
    "save settings",
    "set annotation on",
    NULL, /* set appearance */
    "set message on",
    NULL, /* set turn 0 */
    NULL, /* set turn 1 */
    "show bearoff",
    "show calibration",
    "show copying",
    "show engine",
    "show epc",
    "show export",
    "show gammonvalues",
    "show marketwindow",
    "show matchequitytable",
    "show kleinman",
    "show manual gui",
    "show manual web",
    "show onechequer",
    "show onesidedrollout",
    "show path",
    "show pipcount",
    "show rolls",
    "show statistics game",
    "show statistics match",
    "show statistics session",
    "show temperaturemap",
    "show temperaturemap =cube",
    "show thorp",
    "show version",
    "show warranty",
    "swap players",
    "take",
    "train database",
    "train td"
#if USE_TIMECONTROL
    , "set tc off"
    , "set tc"
#endif
};
enum { TOGGLE_GAMELIST = NUM_CMDS + 1, TOGGLE_ANALYSIS, TOGGLE_COMMENTARY, TOGGLE_MESSAGE, TOGGLE_THEORY, TOGGLE_COMMAND };

static void CopyAsGOL( gpointer *p, guint n, GtkWidget *pw );
static void CopyAsIDs( gpointer *p, guint n, GtkWidget *pw );
static void ExportHTMLImages( gpointer *p, guint n, GtkWidget *pw );
static void GtkManageRelationalEnvs( gpointer *p, guint n, GtkWidget *pw );
static void GtkShowRelational( gpointer *p, guint n, GtkWidget *pw );
static void GtkRelationalAddMatch( gpointer *p, guint n, GtkWidget *pw );
static void LoadCommands( gpointer *p, guint n, GtkWidget *pw );
static void NewClicked( gpointer *p, guint n, GtkWidget *pw );
static void OpenClicked( gpointer *p, guint n, GtkWidget *pw );
static void SaveClicked( gpointer *p, guint n, GtkWidget *pw );
static void ImportClicked( gpointer *p, guint n, GtkWidget *pw );
static void ExportClicked( gpointer *p, guint n, GtkWidget *pw );
static void SetAnalysis( gpointer *p, guint n, GtkWidget *pw );
static void SetOptions( gpointer *p, guint n, GtkWidget *pw );
static void SetPlayers( gpointer *p, guint n, GtkWidget *pw );
static void ReportBug( gpointer *p, guint n, GtkWidget *pw );
static void ShowFAQ( gpointer *p, guint n, GtkWidget *pw );
static void FinishMove( gpointer *p, guint n, GtkWidget *pw );
static void PythonShell( gpointer *p, guint n, GtkWidget *pw );
static void FullScreenMode( gpointer *p, guint n, GtkWidget *pw );
#if USE_BOARD3D
static void SwitchDisplayMode( gpointer *p, guint n, GtkWidget *pw );
#endif
#if USE_TIMECONTROL
static void DefineTimeControl( gpointer *p, guint n, GtkWidget *pw );
#endif
static void TogglePanel ( gpointer *p, guint n, GtkWidget *pw );

/* Game list functions */
extern void GL_Freeze();
extern void GL_Thaw();
extern void GL_SetNames();

/* A dummy widget that can grab events when others shouldn't see them. */
GtkWidget *pwGrab;
GtkWidget *pwOldGrab;

GtkWidget *pwBoard, *pwMain, *pwMenuBar;
GtkWidget *pwToolbar;

GtkWidget *pom = 0;
static GtkWidget *pwStatus, *pwProgress;
GtkWidget *pwMessageText, *pwPanelVbox, *pwAnalysis, *pwCommentary;
static moverecord *pmrAnnotation;
moverecord *pmrCurAnn;
GtkAccelGroup *pagMain;
GtkTooltips *ptt;
GtkItemFactory *pif;
guint nNextTurn = 0; /* GTK idle function */
static int cchOutput;
static guint idOutput, idProgress;
static list lOutput;
int fTTY = TRUE;
int fGUISetWindowPos = TRUE;
int frozen = FALSE;

static guint nStdin, nDisabledCount = 1;


#if USE_TIMECONTROL
extern void GTKUpdateClock(void)
{
char szTime0[20], szTime1[20];
    sprintf(szTime0, (TC_NONE == ms.gc.pc[0].tc.timing) ?  _("n/a") :
	(0 == ms.nTimeouts[0]) ? "%s" :
	(1 == ms.nTimeouts[0]) ? "%s F" : "%s Fx%d",
	 FormatClock(&ms.tvTimeleft[0], 0), ms.nTimeouts[0]);
    sprintf(szTime1, (TC_NONE == ms.gc.pc[1].tc.timing) ?  _("n/a") :
	(0 == ms.nTimeouts[1]) ? "%s" :
	(1 == ms.nTimeouts[1]) ? "%s F" : "%s Fx%d",
	 FormatClock(&ms.tvTimeleft[1], 0), ms.nTimeouts[1]);
	
    board_set_clock(BOARD( pwBoard ),  szTime0, szTime1);
}

extern void GTKUpdateScores(void)
{
    board_set_scores(BOARD( pwBoard ),  ms.anScore[0], ms.anScore[1]);
}
#endif

int grabIdSignal;
int suspendCount = 0;
GtkWidget* grabbedWidget;	/* for testing */

extern void GTKSuspendInput()
{
	if (suspendCount == 0)
	{	/* Grab events so that the board window knows this is a re-entrant
		call, and won't allow commands like roll, move or double. */
		grabbedWidget = pwGrab;
		gtk_grab_add(pwGrab);
		grabIdSignal = gtk_signal_connect_after(GTK_OBJECT(pwGrab),
				"key-press-event", GTK_SIGNAL_FUNC(gtk_true), NULL);
	}

	/* Don't check stdin here; readline isn't ready yet. */
	GTKDisallowStdin();
	suspendCount++;
}

extern void GTKResumeInput()
{
	assert(suspendCount > 0);
	suspendCount--;
	if (suspendCount == 0)
	{
		if (GTK_IS_WIDGET(grabbedWidget) && GTK_WIDGET_HAS_GRAB(grabbedWidget))
		{
			gtk_signal_disconnect(GTK_OBJECT(grabbedWidget), grabIdSignal);
			gtk_grab_remove(grabbedWidget);
		}
	}

	GTKAllowStdin();
}

static void StdinReadNotify( gpointer p, gint h, GdkInputCondition cond ) {
    
    char sz[ 2048 ], *pch;
    
#if HAVE_LIBREADLINE
    if( fReadline ) {
	/* Handle "next turn" processing before more input (otherwise we might
	   not even have a readline handler installed!) */
	while( nNextTurn )
	    NextTurnNotify( NULL );
	
	rl_callback_read_char();

	return;
    }
#endif

    while( nNextTurn )
	NextTurnNotify( NULL );
    
    sz[ 0 ] = 0;
	
    fgets( sz, sizeof( sz ), stdin );

    if( ( pch = strchr( sz, '\n' ) ) )
	*pch = 0;
    
	
    if( feof( stdin ) ) {
	if( !isatty( STDIN_FILENO ) )
	    exit( EXIT_SUCCESS );
	
	PromptForExit();
	return;
    }	

    fInterrupt = FALSE;

    HandleCommand( sz, acTop );

    ResetInterrupt();

    if( nNextTurn )
	fNeedPrompt = TRUE;
    else
	Prompt();
}


extern void GTKAllowStdin( void ) {

    if( !fTTY || !nDisabledCount )
	return;
    
    if( !--nDisabledCount )
	nStdin = gtk_input_add_full( STDIN_FILENO, GDK_INPUT_READ,
				     StdinReadNotify, NULL, NULL, NULL );
}

extern void GTKDisallowStdin( void ) {

    if( !fTTY )
	return;
    
    nDisabledCount++;
    
    if( nStdin ) {
	gtk_input_remove( nStdin );
	nStdin = 0;
    }
}

int fEndDelay;

extern void GTKDelay( void ) {

    SuspendInput();
    
    while( !fInterrupt && !fEndDelay )
	gtk_main_iteration();
    
    fEndDelay = FALSE;
    
    ResumeInput();
}

extern void HandleXAction( void ) {

    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    SuspendInput();
    
    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( gtk_events_pending() )
	gtk_main_iteration();

    ResumeInput();
}

/* TRUE if gnubg is automatically setting the state of a menu item. */
static int fAutoCommand;

static void Command( gpointer *p, guint iCommand, GtkWidget *widget ) {

    char sz[ 80 ];

    if( fAutoCommand )
	return;

    /* FIXME this isn't very good -- if UserCommand fails, the setting
       won't have been changed, but the widget will automatically have
       updated itself. */
    
    if( GTK_IS_RADIO_MENU_ITEM( widget ) ) {
	if( !GTK_CHECK_MENU_ITEM( widget )->active )
	    return;
    } else if( GTK_IS_CHECK_MENU_ITEM( widget ) ) {
	sprintf( sz, "%s %s", aszCommands[ iCommand ],
		 GTK_CHECK_MENU_ITEM( widget )->active ? "on" : "off" );
	UserCommand( sz );
	
	return;
    }

    switch( iCommand ) {
    case CMD_SET_APPEARANCE:
	BoardPreferences( pwBoard );
	return;
	
    case CMD_SET_TURN_0:
    case CMD_SET_TURN_1:
	sprintf( sz, "set turn %s",
		 ap[ iCommand - CMD_SET_TURN_0 ].szName );
	UserCommand( sz );
	return;	
	
#if USE_TIMECONTROL
    case CMD_SET_TC:
	sprintf( sz, "%s %s", aszCommands[ iCommand ], (char*)p);
	UserCommand( sz );
	break;
#endif

    default:
	UserCommand( aszCommands[ iCommand ] );
    }
}

extern int GTKGetManualDice( int an[ 2 ] ) {

    GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Dice"),
					DT_INFO, NULL, NULL ),
	*pwDice = board_dice_widget( BOARD( pwBoard ) );

    an[ 0 ] = 0;
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwDice );
    gtk_object_set_user_data( GTK_OBJECT( pwDice ), an );
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwDice ), "destroy",
			GTK_SIGNAL_FUNC( DestroySetDice ), pwDialog );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    return an[ 0 ] ? 0 : -1;
}

extern void GTKSetDice( gpointer *p, guint n, GtkWidget *pw ) {

    int an[ 2 ];
    char sz[ 13 ]; /* "set dice x y" */

    if( !GTKGetManualDice( an ) ) {
	sprintf( sz, "set dice %d %d", an[ 0 ], an[ 1 ] );
	UserCommand( sz );
    }
}

extern void GTKSetCube( gpointer *p, guint n, GtkWidget *pw ) {

    int valChanged;
    int an[ 2 ];
    char sz[ 20 ]; /* "set cube value 4096" */
    GtkWidget *pwDialog, *pwCube;

    if( ms.gs != GAME_PLAYING || ms.fCrawford || !ms.fCubeUse )
	return;
	
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Cube"),
			     DT_INFO, NULL, NULL );
    pwCube = board_cube_widget( BOARD( pwBoard ) );

    an[ 0 ] = -1;
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwCube );
    gtk_object_set_user_data( GTK_OBJECT( pwCube ), an );
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwCube ), "destroy",
			GTK_SIGNAL_FUNC( DestroySetCube ), pwDialog );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( an[ 0 ] < 0 )
	return;

    outputpostpone();

    valChanged = (1 << an[ 0 ] != ms.nCube);
    if (valChanged) {
	sprintf( sz, "set cube value %d", 1 << an[ 0 ] );
	UserCommand( sz );
    }

    if( an[ 1 ] != ms.fCubeOwner ) {
	if (valChanged) {
		/* Avoid 2 line message, as message box gets in the way */
	    char* pMsg = lOutput.plNext->p;
	    pMsg[strlen(pMsg) - 1] = ' ';
	}
	if( an[ 1 ] >= 0 ) {
	    sprintf( sz, "set cube owner %d", an[ 1 ] );
	    UserCommand( sz );
	} else
	    UserCommand( "set cube centre" );
    }
    
    outputresume();
}

static int fAutoCommentaryChange;

extern void CommentaryChanged( GtkWidget *pw, GtkTextBuffer *buffer ) {

    char *pch;
    GtkTextIter begin, end;
    
    if( fAutoCommentaryChange )
	return;

    assert( pmrAnnotation );

    /* FIXME Copying the entire text every time it's changed is horribly
       inefficient, but the only alternatives seem to be lazy copying
       (which is much harder to get right) or requiring a specific command
       to update the text (which is probably inconvenient for the user). */

    if( pmrAnnotation->sz )
	free( pmrAnnotation->sz );
    
    gtk_text_buffer_get_bounds (buffer, &begin, &end);
    pch = gtk_text_buffer_get_text(buffer, &begin, &end, FALSE);
	/* This copy is absolutely disgusting, but is necessary because GTK
	   insists on giving us something allocated with g_malloc() instead
	   of malloc(). */
	pmrAnnotation->sz = strdup( pch );
	g_free( pch );
}

extern void GTKFreeze( void ) {

	GL_Freeze();
	frozen = TRUE;
}

extern void GTKThaw( void ) {

	GL_Thaw();
	frozen = FALSE;
	/* Make sure analysis window is correct */
	if (plLastMove)
		GTKSetMoveRecord( plLastMove->p );
}

static void SkillMenuActivate( GtkWidget *pw, skilltype st ) {

    static char* aszSkillCmd[ N_SKILLS ] = {
      "verybad", "bad", "doubtful", "clear skill", "good",
    };
    char sz[ 64 ];

    sprintf( sz, "annotate %s %s", 
             (char *) gtk_object_get_user_data( GTK_OBJECT( pw ) ),
             aszSkillCmd[ st ] );
    UserCommand( sz );

    GTKUpdateAnnotations();
}

static GtkWidget*
SkillMenu(skilltype stSelect, char* szAnno)
{
  GtkWidget* pwMenu = gtk_menu_new();
  GtkWidget* pwOptionMenu;
  skilltype st;
    
  for( st = SKILL_VERYBAD; st < N_SKILLS; st++ ) {
    if( st == SKILL_GOOD ) {
      continue;
    }
    {
      const char* l = aszSkillType[st] ? gettext(aszSkillType[st]) : "";
      GtkWidget* pwItem = gtk_menu_item_new_with_label(l);
    
      gtk_menu_append( GTK_MENU( pwMenu ), pwItem);
      gtk_object_set_user_data( GTK_OBJECT( pwItem ), szAnno );
      gtk_signal_connect( GTK_OBJECT( pwItem ), "activate",
			  GTK_SIGNAL_FUNC( SkillMenuActivate ),
			  GINT_TO_POINTER( st ) );
    }
  }
    
  gtk_widget_show_all( pwMenu );
    
  pwOptionMenu = gtk_option_menu_new();
  gtk_option_menu_set_menu( GTK_OPTION_MENU( pwOptionMenu ), pwMenu );

  /* show only bad skills */
  if( stSelect == SKILL_GOOD ) {
    stSelect = SKILL_NONE;
  }
  gtk_option_menu_set_history( GTK_OPTION_MENU( pwOptionMenu ), stSelect );

  return pwOptionMenu;
}


#if USE_TIMECONTROL

/*
 * Display time penalty
 *
 * Parameters: pmr (move time)
 *             pms (match state)
 *
 * Returns:    gtk widget
 *
 */

static GtkWidget *
TimeAnalysis( const moverecord *pmr, const matchstate *pms )
{
  cubeinfo ci;
  GtkWidget *pwTable = gtk_table_new ( 4, 2, FALSE );
  GtkWidget *pwLabel;

  char *sz;

  if ( pmr->t.es.et == EVAL_NONE )
    return NULL;

  GetMatchStateCubeInfo ( &ci, pms );

  sz = g_strdup_printf( ngettext("Time penalty: %s loses %d point",
				 "Time penalty: %s loses %d points",
				 pmr->t.nPoints),
			ap[ pmr->fPlayer ].szName, pmr->t.nPoints );
  pwLabel = gtk_label_new ( sz );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
		     pwLabel,
		     0, 2, 0, 1,
		     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
		     8, 2 );

  g_free( sz );

  /* First column with text */

  pwLabel = gtk_label_new ( _("Equity before time penalty:") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( _("Equity after time penalty:") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( _("Loss from penalty:") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  /* Second column: equities/mwc */

  pwLabel = 
    gtk_label_new ( OutputMWC( pmr->t.aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ], &ci, TRUE ) );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );


  pwLabel = 
    gtk_label_new ( OutputMWC( pmr->t.aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ], &ci , TRUE ) );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = 
    gtk_label_new ( OutputMWCDiff( pmr->t.aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] , pmr->t.aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ], &ci ) );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 3, 4,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );


  return pwTable;

}

#endif /* USE_TIMECONTROL */

static GtkWidget *
ResignAnalysis ( float arResign[ NUM_ROLLOUT_OUTPUTS ],
                 int nResigned,
                 evalsetup *pesResign ) {

  cubeinfo ci;
  GtkWidget *pwTable = gtk_table_new ( 3, 2, FALSE );
  GtkWidget *pwLabel;

  float rAfter, rBefore;

  char sz [ 64 ];

  if ( pesResign->et == EVAL_NONE ) 
    return NULL;

  GetMatchStateCubeInfo ( &ci, &ms );


  /* First column with text */

  pwLabel = gtk_label_new ( _("Equity before resignation: ") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( _("Equity after resignation: ") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( _("Difference: ") );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  /* Second column: equities/mwc */

  getResignEquities ( arResign, &ci, nResigned, &rBefore, &rAfter );

  if( fOutputMWC && ms.nMatchTo )
    sprintf ( sz, "%6.2f%%", eq2mwc ( rBefore, &ci ) * 100.0f );
  else
    sprintf ( sz, "%+6.3f", rBefore );

  pwLabel = gtk_label_new ( sz );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );


  if( fOutputMWC && ms.nMatchTo )
    sprintf ( sz, "%6.2f%%", eq2mwc ( rAfter, &ci ) * 100.0f );
  else
    sprintf ( sz, "%+6.3f", rAfter );

  pwLabel = gtk_label_new ( sz );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );


  if( fOutputMWC && ms.nMatchTo )
    sprintf ( sz, "%+6.2f%%",
              ( eq2mwc ( rAfter, &ci ) - eq2mwc ( rBefore, &ci ) ) * 100.0f );
  else
    sprintf ( sz, "%+6.3f", rAfter - rBefore );

  pwLabel = gtk_label_new ( sz );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 1, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );


  return pwTable;

}




static void LuckMenuActivate( GtkWidget *pw, lucktype lt ) {

    static char *aszLuckCmd[ LUCK_VERYGOOD + 1 ] = {
	"veryunlucky", "unlucky", "clear luck", "lucky", "verylucky"
    };
    char sz[ 64 ];
    
    sprintf( sz, "annotate roll %s", aszLuckCmd[ lt ] );
    UserCommand( sz );
}

static GtkWidget *RollAnalysis( int n0, int n1, float rLuck,
				lucktype ltSelect ) {

    char sz[ 64 ], *pch;
    cubeinfo ci;
    GtkWidget *pw = gtk_hbox_new( FALSE, 4 ), *pwMenu, *pwOptionMenu,
	*pwItem;
    lucktype lt;
    
    pch = sz + sprintf( sz, _("Rolled %d%d"), n0, n1 );
    
    if( rLuck != ERR_VAL ) {
	if( fOutputMWC && ms.nMatchTo ) {
	    GetMatchStateCubeInfo( &ci, &ms );
	    
	    pch += sprintf( pch, " (%+0.3f%%)",
		     100.0f * ( eq2mwc( rLuck, &ci ) - eq2mwc( 0.0f, &ci ) ) );
	} else
	    pch += sprintf( pch, " (%+0.3f)", rLuck );
    }

    gtk_box_pack_start( GTK_BOX( pw ), gtk_label_new( sz ), FALSE, FALSE, 4 );

    pwMenu = gtk_menu_new();
    for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ ) {
	gtk_menu_append( GTK_MENU( pwMenu ),
			 pwItem = gtk_menu_item_new_with_label(
			     aszLuckType[ lt ] ? 
                                gettext ( aszLuckType[ lt ] ) : "" ) );
	gtk_signal_connect( GTK_OBJECT( pwItem ), "activate",
			    GTK_SIGNAL_FUNC( LuckMenuActivate ),
			    GINT_TO_POINTER( lt ) );
    }
    gtk_widget_show_all( pwMenu );
    
    pwOptionMenu = gtk_option_menu_new();
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pwOptionMenu ), pwMenu );
    gtk_option_menu_set_history( GTK_OPTION_MENU( pwOptionMenu ), ltSelect );
    
    gtk_box_pack_start( GTK_BOX( pw ), pwOptionMenu, FALSE, FALSE, 0 );

    return pw;
}


#define ANALYSIS_HORIZONTAL 0

GtkWidget *pwMoveAnalysis = NULL;

extern void SetAnnotation( moverecord *pmr ) {

    GtkWidget *pwParent = pwAnalysis->parent, *pw = NULL, *pwBox, *pwAlign;
    int fMoveOld, fTurnOld;
    list *pl;
    char sz[ 64 ];
    GtkWidget *pwCubeAnalysis = NULL;
    doubletype dt;
    taketype tt;
    cubeinfo ci;
    pwMoveAnalysis = NULL;
    GtkTextBuffer *buffer;

    /* Select the moverecord _after_ pmr.  FIXME this is very ugly! */
    pmrCurAnn = pmr;

    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext )
	if( pl->p == pmr ) {
	    pmr = pl->plNext->p;
	    break;
	}

    if( pl == plGame )
	pmr = NULL;

    pmrAnnotation = pmr;

    /* FIXME optimise by ignoring set if pmr is unchanged */
    
    if( pwAnalysis ) {
	gtk_widget_destroy( pwAnalysis );
	pwAnalysis = NULL;
    }

    gtk_widget_set_sensitive( pwCommentary, pmr != NULL );
    fAutoCommentaryChange = TRUE;
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(pwCommentary));
    gtk_text_buffer_set_text (buffer, "", -1);
    fAutoCommentaryChange = FALSE;
    
    if( pmr ) {
	if( pmr->sz ) {
	    fAutoCommentaryChange = TRUE;
	    gtk_text_buffer_set_text (buffer, pmr->sz, -1);
	    fAutoCommentaryChange = FALSE;
	}

	switch( pmr->mt ) {
	case MOVE_NORMAL:
	    fMoveOld = ms.fMove;
	    fTurnOld = ms.fTurn;

	    pwAnalysis = gtk_vbox_new( FALSE, 0 );

#if ANALYSIS_HORIZONTAL
	    pwBox = gtk_table_new( 3, 2, FALSE );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );
#else
	    pwBox = gtk_table_new( 2, 3, FALSE );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				4 );
#endif

	    ms.fMove = ms.fTurn = pmr->fPlayer;

            /* 
             * Skill and luck
             */

            /* Skill for cube */

            GetMatchStateCubeInfo ( &ci, &ms );
            if ( GetDPEq ( NULL, NULL, &ci ) ) {
              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   gtk_label_new ( _("Didn't double") ),
                                   0, 1, 0, 1 );
#if ANALYSIS_HORIZONTAL
              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   SkillMenu ( pmr->stCube, "cube" ),
                                   1, 2, 0, 1 );
#else
              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   SkillMenu ( pmr->stCube, "cube" ),
                                   0, 1, 1, 2 );
#endif
            }

            /* luck */

	{
    char sz[ 64 ], *pch;
    cubeinfo ci;
    GtkWidget *pwMenu, *pwOptionMenu, *pwItem;
    lucktype lt;
    
    pch = sz + sprintf( sz, _("Rolled %d%d"), pmr->anDice[0], pmr->anDice[1] );
    
    if( pmr->rLuck != ERR_VAL ) {
	if( fOutputMWC && ms.nMatchTo ) {
	    GetMatchStateCubeInfo( &ci, &ms );
	    pch += sprintf( pch, " (%+0.3f%%)",
	     100.0f * ( eq2mwc( pmr->rLuck, &ci ) - eq2mwc( 0.0f, &ci ) ) );
	} else
	    pch += sprintf( pch, " (%+0.3f)", pmr->rLuck );
    }
#if ANALYSIS_HORIZONTAL
    gtk_table_attach_defaults( GTK_TABLE( pwBox ),
				gtk_label_new( sz ), 0, 1, 1, 2 );
#else
    gtk_table_attach_defaults( GTK_TABLE( pwBox ),
				gtk_label_new( sz ), 1, 2, 0, 1 );
#endif

    pwMenu = gtk_menu_new();
    for( lt = LUCK_VERYBAD; lt <= LUCK_VERYGOOD; lt++ ) {
	gtk_menu_append( GTK_MENU( pwMenu ),
			 pwItem = gtk_menu_item_new_with_label(
			     aszLuckType[ lt ] ? 
                                gettext ( aszLuckType[ lt ] ) : "" ) );
	gtk_signal_connect( GTK_OBJECT( pwItem ), "activate",
			    GTK_SIGNAL_FUNC( LuckMenuActivate ),
			    GINT_TO_POINTER( lt ) );
    }
    gtk_widget_show_all( pwMenu );
    
    pwOptionMenu = gtk_option_menu_new();
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pwOptionMenu ), pwMenu );
    gtk_option_menu_set_history( GTK_OPTION_MENU( pwOptionMenu ), pmr->lt );

    gtk_table_attach_defaults( GTK_TABLE( pwBox ), pwOptionMenu, 1, 2, 1, 2 );

	}

            /* chequer play skill */
	    strcpy( sz, _("Moved ") );
	    FormatMove( sz + strlen(_("Moved ")), ms.anBoard, pmr->n.anMove );

#if ANALYSIS_HORIZONTAL
	    gtk_table_attach_defaults( GTK_TABLE( pwBox ),
			      gtk_label_new( sz ), 
			      0, 1, 2, 3 );
#else
	    gtk_table_attach_defaults( GTK_TABLE( pwBox ),
			      gtk_label_new( sz ), 
			      2, 3, 0, 1 );
#endif

#if ANALYSIS_HORIZONTAL
	    gtk_table_attach_defaults( GTK_TABLE( pwBox ), 
                              SkillMenu( pmr->n.stMove, "move" ),
			      1, 2, 2, 3 );
#else
	    gtk_table_attach_defaults( GTK_TABLE( pwBox ), 
                              SkillMenu( pmr->n.stMove, "move" ),
			      2, 3, 1, 2 );
#endif

#undef ANALYSIS_HORIZONTAL

            /* cube */

            pwCubeAnalysis = CreateCubeAnalysis( pmr->CubeDecPtr->aarOutput, 
                                                 pmr->CubeDecPtr->aarStdDev,
                                                 &pmr->CubeDecPtr->esDouble, 
                                                 MOVE_NORMAL );


            /* move */
			      
	    if( pmr->ml.cMoves ) 
              pwMoveAnalysis = CreateMoveList( &pmr->ml, &pmr->n.iMove,
                                               TRUE, FALSE, !IsPanelDocked(WINDOW_ANALYSIS));

            if ( pwMoveAnalysis && pwCubeAnalysis ) {
              /* notebook with analysis */
              pw = gtk_notebook_new ();

              gtk_box_pack_start ( GTK_BOX ( pwAnalysis ), pw, TRUE, TRUE, 0 );

              gtk_notebook_append_page ( GTK_NOTEBOOK ( pw ),
                                         pwMoveAnalysis,
                                         gtk_label_new ( _("Chequer play") ) );

              gtk_notebook_append_page ( GTK_NOTEBOOK ( pw ),
                                         pwCubeAnalysis,
                                         gtk_label_new ( _("Cube decision") ) );


            }
            else if ( pwMoveAnalysis )
			{
				if (IsPanelDocked(WINDOW_ANALYSIS))
					gtk_widget_set_usize(GTK_WIDGET(pwMoveAnalysis), 0, 200);

				gtk_box_pack_start ( GTK_BOX ( pwAnalysis ),
                                   pwMoveAnalysis, TRUE, TRUE, 0 );
			}
            else if ( pwCubeAnalysis )
              gtk_box_pack_start ( GTK_BOX ( pwAnalysis ),
                                   pwCubeAnalysis, TRUE, TRUE, 0 );


	    if( !g_list_first( GTK_BOX( pwAnalysis )->children ) ) {
		gtk_widget_destroy( pwAnalysis );
		pwAnalysis = NULL;
	    }
		
	    ms.fMove = fMoveOld;
	    ms.fTurn = fTurnOld;
	    break;

	case MOVE_DOUBLE:

            dt = DoubleType ( ms.fDoubled, ms.fMove, ms.fTurn );

	    pwAnalysis = gtk_vbox_new( FALSE, 0 );

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                gtk_label_new( 
                                   gettext ( aszDoubleTypes[ dt ] ) ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->stCube, "double" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );

            if ( dt == DT_NORMAL ) {
	    
              if ( ( pw = CreateCubeAnalysis ( pmr->CubeDecPtr->aarOutput,
                                               pmr->CubeDecPtr->aarStdDev,
                                               &pmr->CubeDecPtr->esDouble,
                                               MOVE_DOUBLE ) ) )
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE,
				    FALSE, 0 );

            }
            else 
              gtk_box_pack_start ( GTK_BOX ( pwAnalysis ),
                                   gtk_label_new ( _("GNU Backgammon cannot "
                                                     "analyse neither beavers "
                                                     "nor raccoons yet") ),
                                   FALSE, FALSE, 0 );

	    break;

	case MOVE_TAKE:
	case MOVE_DROP:

            tt = (taketype) DoubleType ( ms.fDoubled, ms.fMove, ms.fTurn );

	    pwAnalysis = gtk_vbox_new( FALSE, 0 );

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				gtk_label_new( pmr->mt == MOVE_TAKE ? _("Take") :
				    _("Drop") ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->stCube, 
                                           ( pmr->mt == MOVE_TAKE ) ?
                                           "take" : "drop" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );

            if ( tt == TT_NORMAL ) {
              if ( ( pw = CreateCubeAnalysis ( pmr->CubeDecPtr->aarOutput,
                                               pmr->CubeDecPtr->aarStdDev,
                                               &pmr->CubeDecPtr->esDouble,
                                               pmr->mt ) ) )
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE,
				    FALSE, 0 );
            }
            else
              gtk_box_pack_start ( GTK_BOX ( pwAnalysis ),
                                   gtk_label_new ( _("GNU Backgammon cannot "
                                                     "analyse neither beavers "
                                                     "nor raccoons yet") ),
                                   FALSE, FALSE, 0 );

	    break;
	    
	case MOVE_RESIGN:
	    pwAnalysis = gtk_vbox_new( FALSE, 0 );

            /* equities for resign */
            
	    if( ( pw = ResignAnalysis( pmr->r.arResign,
                                       pmr->r.nResigned,
                                       &pmr->r.esResign ) ) )
              gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE,
                                  FALSE, 0 );

            /* skill for resignation */

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				gtk_label_new( _("Resign") ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->r.stResign, "resign" ),
				FALSE, FALSE, 2 );

	    pwAlign = gtk_alignment_new( 0.5f, 0.5f, 0.0f, 0.0f );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwAlign, FALSE, FALSE,
				0 );

	    gtk_container_add( GTK_CONTAINER( pwAlign ), pwBox );

            /* skill for accept */

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				gtk_label_new( _("Accept") ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->r.stAccept, "accept" ),
				FALSE, FALSE, 2 );

	    pwAlign = gtk_alignment_new( 0.5f, 0.5f, 0.0f, 0.0f );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwAlign, FALSE, FALSE,
				0 );

	    gtk_container_add( GTK_CONTAINER( pwAlign ), pwBox );
	    
	    break;
	    
	case MOVE_SETDICE:
	    pwAnalysis = RollAnalysis( pmr->anDice[ 0 ],
				       pmr->anDice[ 1 ],
				       pmr->rLuck, pmr->lt );
	    break;

#if USE_TIMECONTROL

        case MOVE_TIME:

          pwAnalysis = TimeAnalysis( pmr, &ms );
          break;

#endif /* USE_TIMECONTROL */

	default:
	    break;
	}
    }
	
    if( !pwAnalysis )
	pwAnalysis = gtk_label_new( _("No analysis available.") );
    
	if (!IsPanelDocked(WINDOW_ANALYSIS))
		gtk_paned_pack1( GTK_PANED( pwParent ), pwAnalysis, TRUE, FALSE );
	else
		gtk_box_pack_start( GTK_BOX( pwParent ), pwAnalysis, TRUE, TRUE, 0 );

	gtk_widget_show_all( pwAnalysis );


    if ( pmr && pmr->mt == MOVE_NORMAL && pwMoveAnalysis && pwCubeAnalysis ) {

      /* (FIXME) Not sure here about SKILL_GOOD */
      if ( badSkill(pmr->stCube) )
        gtk_notebook_set_page ( GTK_NOTEBOOK ( pw ), 1 );


    }
}

static void MenuDelete( GtkWidget *pwChild, GtkWidget *pwMenu ) {

    gtk_container_remove( GTK_CONTAINER( pwMenu ), pwChild );
}

static void SelectGame( GtkWidget *pw, void *p ) {

    int i = GPOINTER_TO_INT( p );
    list *pl;
    
    assert( plGame );

    for( pl = lMatch.plNext; i && pl->plNext->p; i--, pl = pl->plNext )
	;

    if( pl->p == plGame )
	return;

    ChangeGame( pl->p );    
}

static int fGameMenuUsed;

extern void GTKAddGame( moverecord *pmr ) {

    GtkWidget *pw,
	*pwMenu = gtk_option_menu_get_menu( GTK_OPTION_MENU( pom ) );
    GList *pl;
    int c;
    char sz[ 128 ];
    
    if( !fGameMenuUsed ) {
	/* Delete the "(no game)" item. */
	gtk_container_foreach( GTK_CONTAINER( pwMenu ),
			       (GtkCallback) MenuDelete, pwMenu );
	fGameMenuUsed = TRUE;
    }
    
    sprintf( sz, _("Game %d: %s %d, %s %d"), pmr->g.i + 1, ap[ 0 ].szName,
	     pmr->g.anScore[ 0 ], ap[ 1 ].szName, pmr->g.anScore[ 1 ] );
    pw = gtk_menu_item_new_with_label( sz );
	
    pl = gtk_container_children( GTK_CONTAINER( pwMenu ) );

    gtk_signal_connect( GTK_OBJECT( pw ), "activate",
			GTK_SIGNAL_FUNC( SelectGame ),
			GINT_TO_POINTER( c = g_list_length( pl ) ) );

    g_list_free( pl );
    
    gtk_widget_show( pw );
    gtk_menu_append( GTK_MENU( pwMenu ), pw );
    
    GTKSetGame( c );
}

/* Delete i and subsequent games. */
extern void GTKPopGame( int i ) {

    GtkWidget *pwMenu = gtk_option_menu_get_menu( GTK_OPTION_MENU( pom ) );
    GList *pl, *plOrig;

    plOrig = pl = gtk_container_children( GTK_CONTAINER( pwMenu ) );

    while( i-- )
	pl = pl->next;

    while( pl ) {
	gtk_container_remove( GTK_CONTAINER( pwMenu ), pl->data );
	pl = pl->next;
    }

    g_list_free( plOrig );
}

extern void GTKSetGame( int i ) {

    gtk_option_menu_set_history( GTK_OPTION_MENU( pom ), i );
}

/* Recompute the option menu in the game record window (used when the
   player names change, or the score a game was started at is modified). */
extern void GTKRegenerateGames( void ) {

    list *pl, *plGame;
    int i = gtk_option_menu_get_history( GTK_OPTION_MENU( pom ) );

    if( !fGameMenuUsed )
	return;

    /* update player names */
    GL_SetNames();

    /* add games */

    GTKPopGame( 0 );

    for( pl = lMatch.plNext; pl->p; pl = pl->plNext ) {
	plGame = pl->p;
	GTKAddGame( plGame->plNext->p );
    }

    GTKSetGame( i );
}

/* The annotation for one or more moves has been modified.  We refresh
   the entire game and annotation windows, just to be safe. */
extern void GTKUpdateAnnotations( void ) {

    list *pl;
    
    if (!plGame)
    	return;

    GTKFreeze();
    
    GTKClearMoveRecord();

    for( pl = plGame->plNext; pl->p; pl = pl->plNext ) {
	GTKAddMoveRecord( pl->p );
        FixMatchState ( &ms, pl->p );
	ApplyMoveRecord( &ms, plGame, pl->p );
    }

    CalculateBoard();

    GTKSetMoveRecord( plLastMove->p );

    GTKThaw();
}

extern void GTKSaveSettings( void ) {

    char *sz = g_alloca( strlen( szHomeDirectory ) + 15 );
    sprintf( sz, "%s/" GNUBGMENURC, szHomeDirectory );
    gtk_accel_map_save( sz );
}

static gboolean main_delete( GtkWidget *pw ) {

    getWindowGeometry(WINDOW_MAIN);
    
    UserCommand( "quit" );
    
    return TRUE;
}

/* The brain-damaged gtk_statusbar_pop interface doesn't return a value,
   so we have to use a signal to see if anything was actually popped. */
static int fFinishedPopping;

static void TextPopped( GtkWidget *pw, guint id, gchar *text, void *p ) {

    if( !text )
	fFinishedPopping = TRUE;
}

GtkWidget *hpaned, *pwGameBox, *pwPanelGameBox, *pwEventBox;
int panelSize = 325;

extern int GetPanelSize()
{
    if(  (fX) && GTK_WIDGET_REALIZED( pwMain ))
	{
		int pos = gtk_paned_get_position(GTK_PANED(hpaned));
		return pwMain->allocation.width - pos;
	}
	else
		return panelSize;
}

extern void SetPanelWidth(int size)
{
	panelSize = size;
    if( GTK_WIDGET_REALIZED( pwMain ) )
	{
		if (panelSize > pwMain->allocation.width * .8)
			panelSize = pwMain->allocation.width * .8;
		gtk_paned_set_position(GTK_PANED(hpaned), pwMain->allocation.width - panelSize);
		/* Wait while gtk resizes things and try again (in case window had to grow */
		while(gtk_events_pending())
			gtk_main_iteration();
		gtk_paned_set_position(GTK_PANED(hpaned), pwMain->allocation.width - panelSize);
	}
}

extern void SwapBoardToPanel(int ToPanel)
{	/* Show/Hide panel on right of screen */

	/* Need to hide these, as handle box seems to be buggy and gets confused */
	gtk_widget_hide(gtk_widget_get_parent(pwMenuBar));
	gtk_widget_hide(gtk_widget_get_parent(pwToolbar));

	if (ToPanel)
	{
		gtk_widget_reparent(pwEventBox, pwPanelGameBox);
		gtk_widget_show(hpaned);
		while(gtk_events_pending())
			gtk_main_iteration();
		gtk_widget_hide(pwGameBox);
		SetPanelWidth(panelSize);
	}
	else
	{
		gtk_widget_reparent(pwEventBox, pwGameBox);
		gtk_widget_show(pwGameBox);
		while(gtk_events_pending())
			gtk_main_iteration();
		if (GTK_WIDGET_VISIBLE(hpaned))
		{
			panelSize = GetPanelSize();
			gtk_widget_hide(hpaned);
		}
	}
	gtk_widget_show(gtk_widget_get_parent(pwMenuBar));
	gtk_widget_show(gtk_widget_get_parent(pwToolbar));
}

static void MainSize( GtkWidget *pw, GtkRequisition *preq, gpointer p ) {

    /* Give the main window a size big enough that the board widget gets
       board_size=4, if it will fit on the screen. */
    
	int width;

    if( GTK_WIDGET_REALIZED( pw ) )
	gtk_signal_disconnect_by_func( GTK_OBJECT( pw ),
				       GTK_SIGNAL_FUNC( MainSize ), p );
    else if (!SetMainWindowSize())
		gtk_window_set_default_size( GTK_WINDOW( pw ),
				     MAX( 480, preq->width ),
				     MIN( preq->height + 79 * 3,
					  gdk_screen_height() - 20 ) );

	width = GetPanelWidth(WINDOW_MAIN);
	if (width)
		gtk_paned_set_position(GTK_PANED(hpaned), width - panelSize);
	else
		gtk_paned_set_position(GTK_PANED(hpaned), preq->width - panelSize);
}


static gchar *GTKTranslate ( const gchar *path, gpointer func_data ) {

  return (gchar *) gettext ( (const char *) path );

}

GtkWidget* firstChild(GtkWidget* widget)
{
	return g_list_nth_data(gtk_container_children(GTK_CONTAINER(widget)), 0);
}

static void SetToolbarItemStyle(gpointer data, gpointer user_data)
{
	GtkWidget* widget = GTK_WIDGET(data);
	int style = (int)user_data;
	/* Find icon and text widgets from parent object */
	GList* buttonParts;
	GtkWidget *icon, *text;
	GtkWidget* buttonParent;
	if (!GTK_IS_CONTAINER(widget))
		return;
	/* Stop button has event box parent - skip */
	if (GTK_IS_EVENT_BOX(widget))
		widget = firstChild(widget);
	buttonParent = firstChild(widget);
	buttonParts = gtk_container_children(GTK_CONTAINER(buttonParent));
	icon = g_list_nth_data(buttonParts, 0);
	text = g_list_nth_data(buttonParts, 1);

	if (!icon || !text)
		return;	/* Didn't find them */

	/* Hide correct parts dependent on style value */
	if (style == GTK_TOOLBAR_ICONS || style == GTK_TOOLBAR_BOTH)
		gtk_widget_show(icon);
	else
		gtk_widget_hide(icon);
	if (style == GTK_TOOLBAR_TEXT || style == GTK_TOOLBAR_BOTH)
		gtk_widget_show(text);
	else
		gtk_widget_hide(text);
}

extern void SetToolbarStyle(int value)
{
	GtkWidget* pwMainToolbar = firstChild(pwToolbar);
	/* Set each toolbar item separately */
	GList* toolbarItems = gtk_container_children(GTK_CONTAINER(pwMainToolbar));
	g_list_foreach(toolbarItems, SetToolbarItemStyle, (gpointer)value);
	g_list_free(toolbarItems);

	/* Resize handle box parent */
	gtk_widget_queue_resize(gtk_widget_get_parent(pwToolbar));
	nToolbarStyle = value;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget_by_action(pif, value + TOOLBAR_ACTION_OFFSET)), TRUE);
}

static void ToolbarStyle(gpointer    callback_data,
                       guint       callback_action,
                       GtkWidget  *widget)
{
	if(GTK_CHECK_MENU_ITEM(widget)->active)
	{	/* If radio button has been selected set style */
		SetToolbarStyle(callback_action - TOOLBAR_ACTION_OFFSET);
	}
}

GtkClipboard *clipboard = NULL;

static void CopyText()
{
	GtkWidget *pFocus;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	const char *text;

	pFocus = gtk_window_get_focus(GTK_WINDOW(pwMain));
	if (!pFocus)
		return;
	if (GTK_IS_ENTRY(pFocus)) {
		text = gtk_entry_get_text(GTK_ENTRY(pFocus));
	} else if (GTK_IS_TEXT_VIEW(pFocus)) {
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pFocus));
		gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
		text =
		    gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	} else
		return;
	GTKTextToClipboard(text);
}

static void PasteText()
{
	GtkWidget *pFocus;
	GtkTextBuffer *buffer;
	char *text;

	pFocus = gtk_window_get_focus(GTK_WINDOW(pwMain));
	text = gtk_clipboard_wait_for_text(clipboard);
	if (!pFocus || !text)
		return;
	if (GTK_IS_ENTRY(pFocus)) {
		gtk_entry_set_text(GTK_ENTRY(pFocus), text);
		BoardData *bd = BOARD(pwBoard)->board_data;
		if (pFocus == bd->position_id)
			board_set_position(0, bd);
		else if (pFocus == bd->match_id)
			board_set_matchid(0, bd);
	} else if (GTK_IS_TEXT_VIEW(pFocus)) {
		if (gtk_text_view_get_editable(GTK_TEXT_VIEW(pFocus))) {
			buffer =
			    gtk_text_view_get_buffer(GTK_TEXT_VIEW
						     (pFocus));
			gtk_text_buffer_insert_at_cursor(buffer, text, -1);
		}
	}
	g_free(text);
}

extern void GTKTextToClipboard(const char *text)
{
  gtk_clipboard_set_text(clipboard, text, -1);
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *eCon, void* null)
{	/* Maintain panel size */
	if (DockedPanelsShowing())
		gtk_paned_set_position(GTK_PANED(hpaned), eCon->width - GetPanelSize());

	return FALSE;
}

extern int InitGTK( int *argc, char ***argv ) {
    
    GtkWidget *pwVbox, *pwHbox, *pwHandle, *pwPanelHbox;

    static GtkItemFactoryEntry aife[] = {
	{ N_("/_File"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_New..."), "<control>N", NewClicked, 0,
		"<StockItem>", GTK_STOCK_NEW
	},
	{ N_("/_File/_Open..."), "<control>O", OpenClicked, 0, 
		"<StockItem>", GTK_STOCK_OPEN
	},
	{ N_("/_File/Open _Commands..."), NULL, LoadCommands, 0, NULL },
	{ N_("/_File/_Save..."), "<control>S", SaveClicked, 0, 
		"<StockItem>", GTK_STOCK_SAVE
	},
	{ N_("/_File/_Import..."), NULL, ImportClicked, 0, NULL },
	{ N_("/_File/_Export..."), NULL, ExportClicked, 0, NULL },
	{ N_("/_File/Generate _HTML Images..."), NULL, ExportHTMLImages, 0,
	  NULL },
	{ N_("/_File/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_File/_Quit"), "<control>Q", Command, CMD_QUIT,
		"<StockItem>", GTK_STOCK_QUIT
	},
	{ N_("/_Edit"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Edit/_Undo"), "<control>Z", Undo, 0, 
		"<StockItem>", GTK_STOCK_UNDO
	},
	{ N_("/_Edit/-"), NULL, NULL, 0, "<Separator>" },

	{ N_("/_Edit/_Copy"), "<control>C", CopyText, 0,
		"<StockItem>", GTK_STOCK_COPY
	},

	{ N_("/_Edit/Copy as"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Edit/Copy as/Position as ASCII"), NULL,
	  CommandCopy, 0, NULL },
	{ N_("/_Edit/Copy as/GammOnLine (HTML)"), NULL,
	  CopyAsGOL, 0, NULL },
	{ N_("/_Edit/Copy as/Position and Match IDs"), NULL,
	  CopyAsIDs, 0, NULL },

	{ N_("/_Edit/_Paste"), "<control>V", PasteText, 0,
		"<StockItem>", GTK_STOCK_PASTE
	},
	{ N_("/_View"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_View/_Game record"), NULL, TogglePanel, TOGGLE_GAMELIST,
	  "<CheckItem>" },
	{ N_("/_View/_Analysis"), NULL, TogglePanel, TOGGLE_ANALYSIS,
	  "<CheckItem>" },
	{ N_("/_View/_Commentary"), NULL, TogglePanel, TOGGLE_COMMENTARY,
	  "<CheckItem>" },
	{ N_("/_View/_Message"), NULL, TogglePanel, TOGGLE_MESSAGE,
	  "<CheckItem>" },
	{ N_("/_View/_Theory"), NULL, TogglePanel, TOGGLE_THEORY,
	  "<CheckItem>" },
	{ N_("/_View/_Command"), NULL, TogglePanel, TOGGLE_COMMAND,
	  "<CheckItem>" },
        { N_("/_View/_Python shell (IDLE)..."),  
                           NULL, PythonShell, 0, NULL },
	{ N_("/_View/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_View/_Dock panels"), NULL, ToggleDockPanels, 0, "<CheckItem>" },
	{ N_("/_View/Restore panels"), NULL, ShowAllPanels, 0, NULL },
	{ N_("/_View/Hide panels"), NULL, HideAllPanels, 0, NULL },
	{ N_("/_View/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_View/_Toolbar"), NULL, NULL, 0, "<Branch>"},
	{ N_("/_View/_Toolbar/Text only"), NULL, ToolbarStyle, TOOLBAR_ACTION_OFFSET + GTK_TOOLBAR_TEXT,
	  "<RadioItem>" },
	{ N_("/_View/_Toolbar/Icons only"), NULL, ToolbarStyle, TOOLBAR_ACTION_OFFSET + GTK_TOOLBAR_ICONS,
	  "/View/Toolbar/Text only" },
	{ N_("/_View/_Toolbar/Both"), NULL, ToolbarStyle, TOOLBAR_ACTION_OFFSET + GTK_TOOLBAR_BOTH,
	  "/View/Toolbar/Text only" },
	{ N_("/_View/Full screen"), NULL, FullScreenMode, 0, "<CheckItem>" },
#if USE_BOARD3D
	{ N_("/_View/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_View/Switch to xD view"), NULL, SwitchDisplayMode, TOOLBAR_ACTION_OFFSET + MENU_OFFSET, NULL },
#endif
	{ N_("/_Game"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Game/_Roll"), "<control>R", Command, CMD_ROLL, NULL },
	{ N_("/_Game/_Finish move"), "<control>F", FinishMove, 0, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/_Double"), "<control>D", Command, CMD_DOUBLE, NULL },
	{ N_("/_Game/_Take"), "<control>T", Command, CMD_TAKE,
		"<StockItem>", GTK_STOCK_APPLY
	},
	{ N_("/_Game/Dro_p"), "<control>P", Command, CMD_DROP,
		"<StockItem>", GTK_STOCK_CANCEL
	},
	{ N_("/_Game/B_eaver"), NULL, Command, CMD_REDOUBLE, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Re_sign"), NULL, GTKResign, 0, NULL },
        { N_("/_Game/_Agree to resignation"), NULL, Command, CMD_AGREE,
		"<StockItem>", GTK_STOCK_APPLY
	},
	{ N_("/_Game/De_cline resignation"), 
          NULL, Command, CMD_DECLINE,
		"<StockItem>", GTK_STOCK_CANCEL
	},
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Play computer turn"), NULL, Command, CMD_PLAY, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Swap players"), NULL, Command, CMD_SWAP_PLAYERS, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Set cube..."), NULL, GTKSetCube, 0, NULL },
	{ N_("/_Game/Set _dice..."), NULL, GTKSetDice, 0, NULL },
	{ N_("/_Game/Set _turn"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Game/Set turn/0"), 
          NULL, Command, CMD_SET_TURN_0, "<RadioItem>" },
	{ N_("/_Game/Set turn/1"), NULL, Command, CMD_SET_TURN_1,
	  "/Game/Set turn/0" },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Match information..."), NULL, GTKMatchInfo, 0, NULL },
	{ N_("/_Analyse"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Analyse/_Evaluate"), "<control>E", Command, CMD_EVAL, NULL },
	{ N_("/_Analyse/_Hint"), "<control>H", Command, CMD_HINT,
		"<StockItem>", GTK_STOCK_DIALOG_INFO
	},
	{ N_("/_Analyse/_Rollout"), NULL, Command, CMD_ROLLOUT, NULL },
	{ N_("/_Analyse/Rollout _cube decision"), 
          NULL, Command, CMD_ROLLOUT_CUBE, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Analyse move"), 
          NULL, Command, CMD_ANALYSE_MOVE, NULL },
	{ N_("/_Analyse/Analyse game"), 
          NULL, Command, CMD_ANALYSE_GAME, NULL },
	{ N_("/_Analyse/Analyse match"), 
          NULL, Command, CMD_ANALYSE_MATCH, NULL },
	{ N_("/_Analyse/Analyse session"), 
          NULL, Command, CMD_ANALYSE_SESSION, NULL },
        { N_("/_Analyse/Clear analysis"), NULL, NULL, 0, "<Branch>" },
        { N_("/_Analyse/Clear analysis/Move"), 
          NULL, Command, CMD_ANALYSE_CLEAR_MOVE, 
		"<StockItem>", GTK_STOCK_CLEAR
	},
        { N_("/_Analyse/Clear analysis/_Game"), 
          NULL, Command, CMD_ANALYSE_CLEAR_GAME,
		"<StockItem>", GTK_STOCK_CLEAR
	},
        { N_("/_Analyse/Clear analysis/_Match"), 
          NULL, Command, CMD_ANALYSE_CLEAR_MATCH,
		"<StockItem>", GTK_STOCK_CLEAR
	},
        { N_("/_Analyse/Clear analysis/_Session"), 
          NULL, Command, CMD_ANALYSE_CLEAR_SESSION,
		"<StockItem>", GTK_STOCK_CLEAR
	},
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Game statistics"), NULL, Command,
          CMD_SHOW_STATISTICS_GAME, NULL },
	{ N_("/_Analyse/Match statistics"), NULL, Command,
          CMD_SHOW_STATISTICS_MATCH, NULL },
	{ N_("/_Analyse/Session statistics"), NULL, Command,
          CMD_SHOW_STATISTICS_SESSION, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Player records"), NULL, Command,
	  CMD_RECORD_SHOW, NULL },
	{ N_("/_Analyse/Add to player records"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Analyse/Add to player records/Game statistics"), NULL, Command,
	  CMD_RECORD_ADD_GAME,
		"<StockItem>", GTK_STOCK_ADD
	},
	{ N_("/_Analyse/Add to player records/Match statistics"), NULL,
	  Command, CMD_RECORD_ADD_MATCH,
		"<StockItem>", GTK_STOCK_ADD
	},
	{ N_("/_Analyse/Add to player records/Session statistics"), NULL,
	  Command, CMD_RECORD_ADD_SESSION,
		"<StockItem>", GTK_STOCK_ADD
	},
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
        { N_("/_Analyse/Relational database/Add match or session"), NULL,
          GtkRelationalAddMatch, 0,
		"<StockItem>", GTK_STOCK_ADD
	},
        { N_("/_Analyse/Relational database/Show Records"), NULL,
          GtkShowRelational, 0, NULL },
        { N_("/_Analyse/Relational database/Manage Environments"), NULL,
          GtkManageRelationalEnvs, 0, NULL },
        { N_("/_Analyse/Relational database/Test"), NULL,
          Command, CMD_RELATIONAL_TEST, NULL },
        { N_("/_Analyse/Relational database/Help"), NULL,
          Command, CMD_RELATIONAL_HELP, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/_Pip count"), NULL, Command, CMD_SHOW_PIPCOUNT, NULL },
	{ N_("/_Analyse/_Kleinman count"), 
          NULL, Command, CMD_SHOW_KLEINMAN, NULL },
	{ N_("/_Analyse/_Thorp count"), NULL, Command, CMD_SHOW_THORP, NULL },
	{ N_("/_Analyse/One chequer race"), NULL, Command, 
          CMD_SHOW_ONECHEQUER, NULL },
	{ N_("/_Analyse/One sided rollout"), NULL, Command, 
          CMD_SHOW_ONESIDEDROLLOUT, NULL },
	{ N_("/_Analyse/Distribution of rolls"), NULL, Command, 
          CMD_SHOW_ROLLS, NULL },
	{ N_("/_Analyse/Temperature Map"), NULL, Command, 
          CMD_SHOW_TEMPERATURE_MAP, NULL },
	{ N_("/_Analyse/Temperature Map (cube decision)"), NULL, Command, 
          CMD_SHOW_TEMPERATURE_MAP_CUBE, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Bearoff Databases"), NULL, Command, 
          CMD_SHOW_BEAROFF, NULL },
	{ N_("/_Analyse/Effective Pip Count"), NULL, Command, 
          CMD_SHOW_EPC, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/_Gammon values"), NULL, Command, CMD_SHOW_GAMMONVALUES,
	  NULL },
	{ N_("/_Analyse/_Market window"), NULL, Command, CMD_SHOW_MARKETWINDOW,
	  NULL },
	{ N_("/_Analyse/M_atch equity table"), NULL, Command,
	  CMD_SHOW_MATCHEQUITYTABLE, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Evaluation engine"), NULL, Command,
	  CMD_SHOW_ENGINE, NULL },
	{ N_("/_Analyse/Evaluation speed"), NULL, Command,
	  CMD_SHOW_CALIBRATION, NULL },
	{ N_("/_Settings"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Settings/Analysis..."), NULL, SetAnalysis, 0, NULL },
	{ N_("/_Settings/Appearance..."), NULL, Command, CMD_SET_APPEARANCE,
	  NULL },
	{ N_("/_Settings/_Evaluation..."), NULL, SetEvaluation, 0, NULL },
        { N_("/_Settings/E_xport..."), NULL, Command, CMD_SHOW_EXPORT,
          NULL },
	{ N_("/_Settings/_Players..."), NULL, SetPlayers, 0, NULL },
	{ N_("/_Settings/_Rollouts..."), NULL, SetRollouts, 0, NULL },
#if USE_TIMECONTROL
	{ N_("/_Settings/_Time Control"), NULL, NULL, 0, "<Branch>"},
	{ N_("/_Settings/Time Control/_Define..."), NULL, DefineTimeControl, 0, NULL },
	{ N_("/_Settings/Time Control/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Settings/Time Control/Off"), NULL, Command, CMD_SET_TC_OFF, "<RadioItem>"},
#endif
	{ N_("/_Settings/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Settings/Options..."), NULL, SetOptions, 0, NULL },
	{ N_("/_Settings/Paths..."), NULL, Command, CMD_SHOW_PATH, NULL },
	{ N_("/_Settings/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Settings/Save settings"), 
          NULL, Command, CMD_SAVE_SETTINGS, NULL },
	{ N_("/_Go"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Go/Previous rol_l"), "Page_Up", 
          Command, CMD_PREV_ROLL,
		"<StockItem>", GTK_STOCK_GO_BACK
	},
	{ N_("/_Go/Next _roll"), "Page_Down",
	  Command, CMD_NEXT_ROLL, 
		"<StockItem>", GTK_STOCK_GO_FORWARD
	},
	{ N_("/_Go/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Go/_Previous move"), "<shift>Page_Up", 
          Command, CMD_PREV, NULL },
	{ N_("/_Go/Next _move"), "<shift>Page_Down", 
          Command, CMD_NEXT, NULL },
	{ N_("/_Go/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Go/Previous chequer _play"), "<alt>Page_Up",
	  Command, CMD_PREV_ROLLED, NULL },
	{ N_("/_Go/Next _chequer play"), "<alt>Page_Down",
	  Command, CMD_NEXT_ROLLED, NULL },
	{ N_("/_Go/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Go/Previous marke_d move"), "<control><shift>Page_Up", 
          Command, CMD_PREV_MARKED, NULL },
	{ N_("/_Go/Next mar_ked move"), "<control><shift>Page_Down", 
          Command, CMD_NEXT_MARKED, NULL },
	{ N_("/_Go/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Go/Pre_vious game"), "<control>Page_Up", 
          Command, CMD_PREV_GAME,
		"<StockItem>", GTK_STOCK_GOTO_FIRST
	},
	{ N_("/_Go/Next _game"), "<control>Page_Down",
	  Command, CMD_NEXT_GAME,
		"<StockItem>", GTK_STOCK_GOTO_LAST
	},
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Help/_Commands"), NULL, Command, CMD_HELP, NULL },
	{ N_("/_Help/gnubg _Manual"), NULL, Command, 
          CMD_SHOW_MANUAL_GUI, NULL },
	{ N_("/_Help/gnubg M_anual (web)"), NULL, Command, 
          CMD_SHOW_MANUAL_WEB, NULL },
	{ N_("/_Help/_Frequently Asked Questions"), NULL, ShowFAQ, 0, NULL },
	{ N_("/_Help/Co_pying gnubg"), NULL, Command, CMD_SHOW_COPYING, NULL },
	{ N_("/_Help/gnubg _Warranty"), NULL, Command, CMD_SHOW_WARRANTY,
	  NULL },
	{ N_("/_Help/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Help/_Report bug"), NULL, ReportBug, 0, NULL },
	{ N_("/_Help/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Help/_About gnubg"), NULL, Command, CMD_SHOW_VERSION,
		"<StockItem>", GTK_STOCK_ABOUT
	}
    };
    
    char *sz = g_alloca( strlen( szHomeDirectory ) + 15 );

    gtk_set_locale ();

    sprintf( sz, "%s/.gnubg.gtkrc", szHomeDirectory );
    if( !access( sz, R_OK ) )
	gtk_rc_add_default_file( sz );
    else if( !access( PKGDATADIR "/gnubg.gtkrc", R_OK ) )
	gtk_rc_add_default_file( PKGDATADIR "/gnubg.gtkrc" );
    else
	gtk_rc_add_default_file( "gnubg.gtkrc" );
    
    if( !gtk_init_check( argc, argv ) )
	return FALSE;
    
#if USE_BOARD3D
	/* Initialize openGL widget library */
	InitGTK3d(argc, argv);
#endif

    fnAction = HandleXAction;

    gdk_rgb_init();
    gdk_rgb_set_min_colors( 2 * 2 * 2 );
    gtk_widget_set_default_colormap( gdk_rgb_get_cmap() );
    gtk_widget_set_default_visual( gdk_rgb_get_visual() );

    {
#include "xpm/gnu.xpm"
	GtkIconFactory *pif = gtk_icon_factory_new();

	gtk_icon_factory_add_default( pif );

	gtk_icon_factory_add( pif, GTK_STOCK_DIALOG_GNU,
			      gtk_icon_set_new_from_pixbuf(
				  gdk_pixbuf_new_from_xpm_data(
				      (const char **) gnu_xpm ) ) );
    }

    ptt = gtk_tooltips_new();
    
    pwMain = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	SetPanelWidget(WINDOW_MAIN, pwMain);
    gtk_window_set_role( GTK_WINDOW( pwMain ), "main" );
    gtk_window_set_type_hint( GTK_WINDOW( pwMain ),
			      GDK_WINDOW_TYPE_HINT_NORMAL );
    /* NB: the following call to gtk_object_set_user_data is needed
       to work around a nasty bug in GTK+ (the problem is that calls to
       gtk_object_get_user_data relies on a side effect from a previous
       call to gtk_object_set_data).  Adding a call here guarantees that
       gtk_object_get_user_data will work as we expect later.
       FIXME: this can eventually be removed once GTK+ is fixed. */
    gtk_object_set_user_data( GTK_OBJECT( pwMain ), NULL );
    
    gtk_window_set_title( GTK_WINDOW( pwMain ), _("GNU Backgammon") );
    /* FIXME add an icon */
    gtk_container_add( GTK_CONTAINER( pwMain ),
		       pwVbox = gtk_vbox_new( FALSE, 0 ) );

    pagMain = gtk_accel_group_new();
    pif = gtk_item_factory_new( GTK_TYPE_MENU_BAR, "<main>", pagMain );

    gtk_item_factory_set_translate_func ( pif, GTKTranslate, NULL, NULL );

    gtk_item_factory_create_items( pif, sizeof( aife ) / sizeof( aife[ 0 ] ),
				   aife, NULL );
    gtk_window_add_accel_group( GTK_WINDOW( pwMain ), pagMain );

    gtk_box_pack_start( GTK_BOX( pwVbox ),
			pwHandle = gtk_handle_box_new(),
			FALSE, FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwHandle ),
			pwMenuBar = gtk_item_factory_get_widget( pif,
								 "<main>" ));

    sprintf( sz, "%s/" GNUBGMENURC, szHomeDirectory );
    gtk_accel_map_load( sz );
   
   gtk_box_pack_start( GTK_BOX( pwVbox ),
                       pwHandle = gtk_handle_box_new(), FALSE, TRUE, 0 );
   gtk_container_add( GTK_CONTAINER( pwHandle ),
                       pwToolbar = ToolbarNew() );
    
   pwGrab = GTK_WIDGET( ToolbarGetStopParent( pwToolbar ) );

   gtk_box_pack_start( GTK_BOX( pwVbox ),
			pwGameBox = gtk_hbox_new(FALSE, 0),
			TRUE, TRUE, 0 );
   gtk_box_pack_start( GTK_BOX( pwVbox ),
			hpaned = gtk_hpaned_new(),
			TRUE, TRUE, 0 );

   gtk_paned_add1(GTK_PANED(hpaned), pwPanelGameBox = gtk_hbox_new(FALSE, 0));
   gtk_container_add(GTK_CONTAINER(pwPanelGameBox), pwEventBox = gtk_event_box_new());
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwEventBox), FALSE);

   gtk_container_add(GTK_CONTAINER(pwEventBox), pwBoard = board_new(GetMainAppearance()));
   gtk_signal_connect(GTK_OBJECT(pwEventBox), "button-press-event", GTK_SIGNAL_FUNC(button_press_event),
	   BOARD(pwBoard)->board_data);

   pwPanelHbox = gtk_hbox_new(FALSE, 0);
   gtk_paned_add2(GTK_PANED(hpaned), pwPanelHbox);
   gtk_box_pack_start( GTK_BOX( pwPanelHbox ), pwPanelVbox = gtk_vbox_new( FALSE, 1 ), TRUE, TRUE, 0);

   DockPanels();

   /* Status bar */
					
   gtk_box_pack_end( GTK_BOX( pwVbox ), pwHbox = gtk_hbox_new( FALSE, 0 ),
		      FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwStatus = gtk_statusbar_new(),
		      TRUE, TRUE, 0 );
    gtk_statusbar_set_has_resize_grip( GTK_STATUSBAR( pwStatus ), FALSE );
    /* It's a bit naughty to access pwStatus->label, but its default alignment
       is ugly, and GTK gives us no other way to change it. */
    gtk_misc_set_alignment( GTK_MISC( GTK_STATUSBAR( pwStatus )->label ),
			    0.0f, 0.5f );
    idOutput = gtk_statusbar_get_context_id( GTK_STATUSBAR( pwStatus ),
					     "gnubg output" );
    idProgress = gtk_statusbar_get_context_id( GTK_STATUSBAR( pwStatus ),
					       "progress" );
    gtk_signal_connect( GTK_OBJECT( pwStatus ), "text-popped",
			GTK_SIGNAL_FUNC( TextPopped ), NULL );
    
    gtk_box_pack_start( GTK_BOX( pwHbox ),
			pwProgress = gtk_progress_bar_new(),
			FALSE, FALSE, 0 );
    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( pwProgress ), 0.0 );
    /* This is a kludge to work around an ugly bug in GTK: we don't want to
       show text in the progress bar yet, but we might later.  So we have to
       pretend we want text in order to be sized correctly, and then set the
       format string to something so we don't get the default text. */
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR( pwProgress ), " " );

    gtk_signal_connect(GTK_OBJECT(pwMain), "configure_event",
			GTK_SIGNAL_FUNC(configure_event), NULL);
    gtk_signal_connect( GTK_OBJECT( pwMain ), "size-request",
			GTK_SIGNAL_FUNC( MainSize ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    ListCreate( &lOutput );

    cedDialog.showHelp = 0;
    cedDialog.numHistory = 0;
    cedDialog.completing = 0;
    cedDialog.modal = TRUE;

    cedPanel.showHelp = 0;
    cedPanel.numHistory = 0;
    cedPanel.completing = 0;
    cedPanel.modal = FALSE;

{
  GdkAtom cb = gdk_atom_intern("CLIPBOARD", TRUE);
  clipboard = gtk_clipboard_get(cb);
  if (!clipboard)
    printf("Failed to get clipboard!\n");
}

    return TRUE;
}

extern void RunGTK( GtkWidget *pwSplash ) {

    int anBoardTemp[ 2 ][ 25 ];
    int i;
    GTKSet( &ms.fCubeOwner );
    GTKSet( &ms.nCube );
    GTKSet( ap );
    GTKSet( &ms.fTurn );
    GTKSet( &ms.gs );
    
    PushSplash ( pwSplash, 
                 _("Rendering"), _("Board"), 0 );

    ShowBoard();

    GTKAllowStdin();
    
    if( fTTY ) {
#if HAVE_LIBREADLINE
	if( fReadline ) {
	    fReadingCommand = TRUE;
	    rl_callback_handler_install( FormatPrompt(), HandleInput );
	    atexit( rl_callback_handler_remove );
	} else
#endif
	    Prompt();
    }

	/* Show everything */
	gtk_widget_show_all( pwMain );

	/* Make sure toolbar looks correct */
	SetToolbarStyle(nToolbarStyle);

#if USE_BOARD3D
{
	BoardData *bd = BOARD( pwBoard )->board_data;
	BoardData3d *bd3d = &bd->bd3d;
	renderdata *prd = bd->rd;

	SetSwitchModeMenuText();
	DisplayCorrectBoardType(bd, bd3d, prd);
}
#endif

	DestroySplash ( pwSplash );

	/* Display any other windows now */
	DisplayWindows();

	/* Make sure some things stay hidden */
	if (!ArePanelsDocked())
	{
		gtk_widget_hide(hpaned);
		gtk_widget_hide(gtk_item_factory_get_widget(pif, "/View/Commentary"));
		gtk_widget_hide(gtk_item_factory_get_widget(pif, "/View/Hide panels"));
		gtk_widget_hide(gtk_item_factory_get_widget(pif, "/View/Restore panels"));
	}
	else if (ArePanelsShowing())
		gtk_widget_hide(pwGameBox);

	/* Make sure main window is on top */
	gdk_window_raise(pwMain->window);

	/* force update of board; needed to display board correctly if user
		has special settings, e.g., clockwise or nackgammon */
	ShowBoard();

	/* Clear board at startup */
	for( i = 0; i < 25; i++ )
		anBoardTemp[ 0 ][ i ] = anBoardTemp[ 1 ][ i ] = 0;
	game_set(BOARD(pwBoard), anBoardTemp, 0, "", "", 0, 0, 0, -1, -1, FALSE, anChequers[ms.bgv]);

	gtk_main();
}

static void DestroyList( gpointer p ) {

    *( (void **) p ) = NULL;
}

extern void ShowList( char *psz[], char *szTitle, GtkWidget *pwParent ) {

    static GtkWidget *pwDialog;
    GtkWidget *pwList = gtk_list_new(),
	*pwScroll = gtk_scrolled_window_new( NULL, NULL ),
	*pwOK = gtk_button_new_with_label( _("OK") );

    if( pwDialog )
	gtk_widget_destroy( pwDialog );

    pwDialog = gtk_dialog_new();
	if (pwParent)
	{	/* Modal */
		gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
		gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
						GTK_WINDOW( pwParent ) );
	}
    gtk_object_weakref( GTK_OBJECT( pwDialog ), DestroyList, &pwDialog );

    while( *psz )
	gtk_container_add( GTK_CONTAINER( pwList ),
			   gtk_list_item_new_with_label( *psz++ ) );
    
    gtk_signal_connect_object( GTK_OBJECT( pwOK ), "clicked",
			       GTK_SIGNAL_FUNC( gtk_widget_destroy ),
			       GTK_OBJECT( pwDialog ) );

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScroll ),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC );
    gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( pwScroll ),
					   pwList );
    
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->vbox ),
		       pwScroll );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->action_area ),
		       pwOK );
    
    GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( pwOK );
    
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 560, 400 );
    gtk_window_set_title( GTK_WINDOW( pwDialog ), szTitle );

    gtk_widget_show_all( pwDialog );
}

static void OK( GtkWidget *pw, int *pf ) {

    if( pf )
	*pf = TRUE;
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}


extern GtkWidget *GTKCreateDialog( const char *szTitle, const dialogtype dt, 
                                   GtkSignalFunc pf, void *p ) {
    GtkWidget *pwDialog = gtk_dialog_new(),
	*pwOK = gtk_button_new_with_label( _("OK") ),
	*pwCancel = gtk_button_new_with_label( _("Cancel") ),
	*pwHbox = gtk_hbox_new( FALSE, 0 ),
	*pwButtons = gtk_hbutton_box_new(),
	*pwPixmap;
    GtkAccelGroup *pag = gtk_accel_group_new();
    int fQuestion = dt == DT_QUESTION || dt == DT_AREYOUSURE;
    static char *aszStockItem[ NUM_DIALOG_TYPES ] = {
	GTK_STOCK_DIALOG_INFO,
	GTK_STOCK_DIALOG_QUESTION,
	GTK_STOCK_DIALOG_WARNING,
	GTK_STOCK_DIALOG_WARNING,
	GTK_STOCK_DIALOG_ERROR,
	GTK_STOCK_DIALOG_GNU
    };
    pwPixmap = gtk_image_new_from_stock( aszStockItem[ dt ],
					 GTK_ICON_SIZE_DIALOG );
    gtk_misc_set_padding( GTK_MISC( pwPixmap ), 8, 8 );

    gtk_button_box_set_layout( GTK_BUTTON_BOX( pwButtons ),
			       GTK_BUTTONBOX_SPREAD );
    
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwPixmap, FALSE, FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->vbox ),
		       pwHbox );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->action_area ),
		       pwButtons );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwOK );

    gtk_signal_connect( GTK_OBJECT( pwOK ), "clicked", pf ? pf :
			GTK_SIGNAL_FUNC( OK ), p );

    if( fQuestion ) {
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwCancel );
	gtk_signal_connect_object( GTK_OBJECT( pwCancel ), "clicked",
				   GTK_SIGNAL_FUNC( gtk_widget_destroy ),
				   GTK_OBJECT( pwDialog ) );
    }

    gtk_window_add_accel_group( GTK_WINDOW( pwDialog ), pag );
    gtk_widget_add_accelerator( fQuestion ? pwCancel : pwOK,
				"clicked", pag, GDK_Escape, 0, 0 );

    gtk_window_set_title( GTK_WINDOW( pwDialog ), szTitle );

    GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( pwOK );

    return pwDialog;
}

extern GtkWidget *DialogArea( GtkWidget *pw, dialogarea da ) {

    GList *pl;
    GtkWidget *pwChild;
    
    switch( da ) {
    case DA_MAIN:
    case DA_BUTTONS:
	pl = gtk_container_children( GTK_CONTAINER(
	    da == DA_MAIN ? GTK_DIALOG( pw )->vbox :
	    GTK_DIALOG( pw )->action_area ) );
	pwChild = pl->data;
	g_list_free( pl );
	return pwChild;

    case DA_OK:
	pl = gtk_container_children( GTK_CONTAINER(
					 DialogArea( pw, DA_BUTTONS ) ) );
	pwChild = pl->data;
	g_list_free( pl );
	return pwChild;
	
    default:
	abort();
    }
}

/* Use to temporarily set the parent dialog for nested dialogs */
GtkWidget *pwCurrentParent = NULL;
void GTKSetCurrentParent(GtkWidget *parent)
{
	pwCurrentParent = parent;
}
GtkWidget *GTKGetCurrentParent()
{
	GtkWidget *current = pwCurrentParent ? pwCurrentParent : pwMain;
	pwCurrentParent = NULL;
	return current;
}

extern int 
GTKMessage( char *sz, dialogtype dt ) {

    int f = FALSE, fRestoreNextTurn;
    static char *aszTitle[ NUM_DIALOG_TYPES - 1 ] = {
	N_("GNU Backgammon - Message"),
	N_("GNU Backgammon - Question"),
	N_("GNU Backgammon - Warning"), /* are you sure */
	N_("GNU Backgammon - Warning"),
	N_("GNU Backgammon - Error"),
    };
    GtkWidget *pwDialog = GTKCreateDialog( gettext( aszTitle[ dt ] ),
					   dt, NULL, &f );
    GtkWidget *psw;
    GtkWidget *pwPrompt = gtk_label_new( sz );
    GtkRequisition req;

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_label_set_justify( GTK_LABEL( pwPrompt ), GTK_JUSTIFY_LEFT );
    gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );

    gtk_widget_size_request( GTK_WIDGET( pwPrompt ), &req );

    if ( req.height > 500 ) {
	psw = gtk_scrolled_window_new( NULL, NULL );
	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
			   psw );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( psw ),
					       pwPrompt );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
	gtk_window_set_default_size( GTK_WINDOW( pwDialog ),
				     req.width, req.height );
	gtk_window_set_policy( GTK_WINDOW( pwDialog ), FALSE, TRUE, TRUE );
    }
    else {
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );
    gtk_window_set_policy( GTK_WINDOW( pwDialog ), FALSE, FALSE, FALSE );
    }

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
		GTK_WINDOW(GTKGetCurrentParent()));
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    /* This dialog should be REALLY modal -- disable "next turn" idle
       processing and stdin handler, to avoid reentrancy problems. */
    if( ( fRestoreNextTurn = nNextTurn ) )
	gtk_idle_remove( nNextTurn );
	    
    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fRestoreNextTurn )
	nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
    
    return f;
}

extern int GTKGetInputYN( char *szPrompt ) {

    return GTKMessage( szPrompt, DT_AREYOUSURE );
}

char* inputString;
static void GetInputOk( GtkWidget *pw, GtkWidget *pwEntry )
{
	inputString = strdup(gtk_entry_get_text(GTK_ENTRY(pwEntry)));
    gtk_widget_destroy(gtk_widget_get_toplevel(pw));
}

extern char* GTKGetInput(char* title, char* prompt)
{
	GtkWidget *pwDialog, *pwHbox, *pwEntry;
	pwEntry = gtk_entry_new();
	inputString = NULL;
	pwDialog = GTKCreateDialog(title, DT_QUESTION, GTK_SIGNAL_FUNC(GetInputOk), pwEntry );

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), 
		pwHbox = gtk_hbox_new(FALSE, 0));

	gtk_box_pack_start(GTK_BOX(pwHbox), gtk_label_new(prompt), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwEntry, FALSE, FALSE, 0);
	gtk_widget_grab_focus(pwEntry);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_widget_show_all( pwDialog );

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
	return inputString;
}

/*
 * put up a tutor window with a message about how bad the intended
 * play is. Allow selections:
 * Continue (play it anyway)
 * Cancel   (rethink)
 * returns TRUE if play it anyway
 */

static void TutorEnd( GtkWidget *pw, int *pf ) {

    if( pf )
	*pf = TRUE;
    
	fTutor = FALSE;
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void
TutorHint ( GtkWidget *pw, void *unused ) {

  gtk_widget_destroy ( gtk_widget_get_toplevel( pw ) );
  UserCommand ( "hint" );

}

static void
TutorRethink ( GtkWidget *pw, void *unused ) {

#if USE_BOARD3D
	RestrictiveRedraw();
#endif
  gtk_widget_destroy ( gtk_widget_get_toplevel( pw ) );
  ShowBoard ();

}

extern int GtkTutor ( char *sz ) {

    int f = FALSE, fRestoreNextTurn;
    GdkPixmap *ppm;
    GtkWidget *pwTutorDialog, *pwOK, *pwCancel, *pwEndTutor, *pwHbox,
          *pwButtons, *pwPixmap, *pwPrompt;
    GtkWidget *pwHint;
    GtkAccelGroup *pag;

#include "xpm/question.xpm"

	pwTutorDialog = gtk_dialog_new();
	pwOK = gtk_button_new_with_label( _("Play Anyway") );
	pwCancel = gtk_button_new_with_label( _("Rethink") );
	pwEndTutor = gtk_button_new_with_label ( _("End Tutor Mode") );
	pwHint = gtk_button_new_with_label ( _("Hint") );
	pwHbox = gtk_hbox_new( FALSE, 0 );
	pwButtons = gtk_hbutton_box_new();

    pag = gtk_accel_group_new();
    
    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
                   gtk_widget_get_colormap( pwTutorDialog ), NULL, NULL,
                   question_xpm);
    pwPixmap = gtk_pixmap_new( ppm, NULL );
    gtk_misc_set_padding( GTK_MISC( pwPixmap ), 8, 8 );
    
    gtk_button_box_set_layout( GTK_BUTTON_BOX( pwButtons ),
			       GTK_BUTTONBOX_SPREAD );
    
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwPixmap, FALSE, FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwTutorDialog )->vbox ),
		       pwHbox );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwTutorDialog )->action_area ),
		       pwButtons );
    
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwOK );
    gtk_signal_connect( GTK_OBJECT( pwOK ), "clicked", 
			GTK_SIGNAL_FUNC( OK ), (void *) &f );
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwCancel );
	gtk_signal_connect( GTK_OBJECT( pwCancel ), "clicked",
				   GTK_SIGNAL_FUNC( TutorRethink ),
				   (void *) &f );

	gtk_container_add( GTK_CONTAINER( pwButtons ), pwEndTutor );
	gtk_signal_connect( GTK_OBJECT( pwEndTutor ), "clicked",
				   GTK_SIGNAL_FUNC( TutorEnd ), (void *) &f );

	gtk_container_add( GTK_CONTAINER( pwButtons ), pwHint );
	gtk_signal_connect( GTK_OBJECT( pwHint ), "clicked",
				   GTK_SIGNAL_FUNC( TutorHint ), (void *) &f );

    gtk_window_add_accel_group( GTK_WINDOW( pwTutorDialog ), pag );
    gtk_widget_add_accelerator( pwCancel, "clicked", pag,
				GDK_Escape, 0, 0 );
    
    gtk_window_set_title( GTK_WINDOW( pwTutorDialog ), 
                          _("GNU Backgammon - Tutor") );

    GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( pwOK );
    
    pwPrompt = gtk_label_new( sz );

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_label_set_justify( GTK_LABEL( pwPrompt ), GTK_JUSTIFY_LEFT );
    gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwTutorDialog, DA_MAIN ) ),
		       pwPrompt );
    
    gtk_window_set_policy( GTK_WINDOW( pwTutorDialog ), FALSE, FALSE, FALSE );
    gtk_window_set_modal( GTK_WINDOW( pwTutorDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwTutorDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwTutorDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    gtk_widget_show_all( pwTutorDialog );
    
    /* This dialog should be REALLY modal -- disable "next turn" idle
       processing and stdin handler, to avoid reentrancy problems. */
    if( ( fRestoreNextTurn = nNextTurn ) )
      gtk_idle_remove( nNextTurn );
    
    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();
    
    if( fRestoreNextTurn )
      nNextTurn = gtk_idle_add( NextTurnNotify, NULL );
    
    /* if tutor mode was disabled, update the checklist */
    if ( !fTutor) {
      GTKSet ( (void *) &fTutor);
    }
    
    return f;
}

extern void GTKOutput( char *sz ) {

    if( !sz || !*sz )
	return;
    
    cchOutput += strlen( sz );

    ListInsert( &lOutput, sz );
}

extern void GTKOutputX( void ) {

    char *sz, *pchSrc, *pchDest;
    list *pl;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    if( !cchOutput )
	return;
    
    pchDest = sz = g_malloc( cchOutput + 2 );	/* + 2 as /n/0 may be added */

    for( pl = lOutput.plNext; pl != &lOutput; pl = pl->plNext ) {
	pchSrc = pl->p;

	while( *pchSrc )
	    *pchDest++ = *pchSrc++;

	g_free( pl->p );
    }

    while( lOutput.plNext != &lOutput )
	ListDelete( lOutput.plNext );

    while( pchDest > sz && pchDest[ -1 ] == '\n' )
	pchDest--;
    
    *pchDest = 0;

    if( cchOutput > 80 || strchr( sz, '\n' ) ) {
      /* Long message; display in dialog. */
      if (!PanelShowing(WINDOW_MESSAGE))
        GTKMessage( sz, DT_INFO );
    }
    else
      /* Short message; display in status bar. */
      gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idOutput, sz );
    
    if (PanelShowing(WINDOW_MESSAGE) && *sz ) {
      strcat ( sz, "\n" );
      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwMessageText));
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_buffer_insert( buffer, &iter, sz, -1);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW(pwMessageText), &iter, 0.0, FALSE, 0.0, 1.0 );
    }
      
    cchOutput = 0;
    g_free( sz );
}

extern void GTKOutputErr( char *sz ) {

    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GTKMessage( sz, DT_ERROR );
    
    if (PanelShowing(WINDOW_MESSAGE)) {
      buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwMessageText));
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_buffer_insert( buffer, &iter, sz, -1);
      gtk_text_buffer_insert( buffer, &iter, "\n", -1);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW(pwMessageText), &iter, 0.0, FALSE, 0.0, 1.0 );
    }
}

extern void GTKOutputNew( void ) {

    /* This is horribly ugly, but fFinishedPopping will never be set if
       the progress bar leaves a message in the status stack.  There should
       be at most one message, so we get rid of it here. */
    gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idProgress );
    
    fFinishedPopping = FALSE;
    
    do
	gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idOutput );
    while( !fFinishedPopping );
}

/* Show dynamic help as command entered */

extern command *FindHelpCommand( command *pcBase, char *sz,
				 char *pchCommand, char *pchUsage );
extern char* CheckCommand(char *sz, command *ac);

/* Display help for command (pStr) in widget (pwText) */
extern void ShowHelp(GtkWidget *pwText, char* pStr)
{
	command *pc, *pcFull;
	char szCommand[128], szUsage[128], szBuf[255], *cc, *pTemp;
	command cTop = { NULL, NULL, NULL, NULL, acTop };
        GtkTextBuffer *buffer;
        GtkTextIter iter;

	/* Copy string as token striping corrupts string */
	pTemp = malloc(strlen(pStr) + 1);
	strcpy(pTemp, pStr);
	cc = CheckCommand(pTemp, acTop);

        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwText));

	if (cc)
	{
		sprintf(szBuf, _("Unknown keyword: %s\n"), cc);
                gtk_text_buffer_set_text(buffer, szBuf, -1);
	}
	else if ((pc = FindHelpCommand(&cTop, pStr, szCommand, szUsage)))
	{
                gtk_text_buffer_set_text(buffer, "", -1);
                gtk_text_buffer_get_end_iter (buffer, &iter);
		if (!*pStr)
		{
                        gtk_text_buffer_insert( buffer, &iter,
				_("Available commands:\n"), -1);
		}
		else
		{
			if(!pc->szHelp )
			{	/* The command is an abbreviation, search for the full version */
				for( pcFull = acTop; pcFull->sz; pcFull++ )
				{
					if( pcFull->pf == pc->pf && pcFull->szHelp )
					{
						pc = pcFull;
						strcpy(szCommand, pc->sz);
						break;
					}
				}
			}

			sprintf(szBuf, "Command: %s\n", szCommand);
                        gtk_text_buffer_insert( buffer, &iter, szBuf, -1);
                        gtk_text_buffer_insert( buffer, &iter, gettext ( pc->szHelp ), -1);
			sprintf(szBuf, "\n\nUsage: %s", szUsage);
                        gtk_text_buffer_insert( buffer, &iter, szBuf, -1);

			if(!( pc->pc && pc->pc->sz ))
                                gtk_text_buffer_insert( buffer, &iter, "\n", -1);
			else
			{
                                gtk_text_buffer_insert( buffer, &iter, _("<subcommand>\n"), -1);
                                gtk_text_buffer_insert( buffer, &iter,
					_("Available subcommands:\n"), -1);
			}
		}

		pc = pc->pc;

		while(pc && pc->sz)
		{
			if( pc->szHelp )
			{
				sprintf(szBuf, "%-15s\t%s\n", pc->sz, gettext ( pc->szHelp ) );
                                gtk_text_buffer_insert( buffer, &iter, szBuf, -1);
			}
			pc++;
		}
	}
	free(pTemp);
}

extern void PopulateCommandHistory(struct CommandEntryData_T *pData)
{
	GList *glist;
	int i;

	glist = NULL;
	for (i = 0; i < pData->numHistory; i++)
		glist = g_list_append(glist, pData->cmdHistory[i]);
	if (glist)
	{
		gtk_combo_set_popdown_strings(GTK_COMBO(pData->cmdEntryCombo), glist);
		g_list_free(glist);
	}
}

extern void CommandOK( GtkWidget *pw, struct CommandEntryData_T *pData )
{
	int i, found = -1;
	pData->cmdString = gtk_editable_get_chars( GTK_EDITABLE( pData->pwEntry ), 0, -1 );

	/* Update command history */

	/* See if already in history */
	for (i = 0; i < pData->numHistory; i++)
	{
		if (!strcasecmp(pData->cmdString, pData->cmdHistory[i]))
		{
			found = i;
			break;
		}
	}
	if (found != -1)
	{	/* Remove old entry */
		free(pData->cmdHistory[found]);
		pData->numHistory--;
		for (i = found; i < pData->numHistory; i++)
			pData->cmdHistory[i] = pData->cmdHistory[i + 1];
	}

	if (pData->numHistory == NUM_CMD_HISTORY)
	{
		free(pData->cmdHistory[NUM_CMD_HISTORY - 1]);
		pData->numHistory--;
	}
	for (i = pData->numHistory; i > 0; i--)
		pData->cmdHistory[i] = pData->cmdHistory[i - 1];

	pData->cmdHistory[0] = malloc(strlen(pData->cmdString) + 1);
	strcpy(pData->cmdHistory[0], pData->cmdString);
	pData->numHistory++;

	if (pData->modal)
		gtk_widget_destroy(gtk_widget_get_toplevel(pw));
	else
	{
		PopulateCommandHistory(pData);
		gtk_entry_set_text(GTK_ENTRY(pData->pwEntry), "");
	}

	if (pData->cmdString)
	{
		UserCommand(pData->cmdString);
		g_free(pData->cmdString);
	}
}

extern void CommandTextChange(GtkEntry *entry, struct CommandEntryData_T *pData)
{
	if (pData->showHelp)
	{
		if (!pData->modal)
		{	/* Make sure the message window is showing */
			if (!PanelShowing(WINDOW_MESSAGE))
				PanelShow(WINDOW_MESSAGE);
		}
		ShowHelp(pData->pwHelpText, gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1));
	}
}

static void CreateHelpText(struct CommandEntryData_T *pData)
{
        GtkWidget *psw;
	pData->pwHelpText = gtk_text_view_new ();
	gtk_widget_set_usize(pData->pwHelpText, 400, 300);
	psw = gtk_scrolled_window_new( NULL, NULL );
        gtk_container_add(GTK_CONTAINER(psw), pData->pwHelpText);
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	CommandTextChange(GTK_ENTRY(pData->pwEntry), pData);
}

extern void ShowHelpToggled(GtkWidget *widget, struct CommandEntryData_T *pData)
{
	if (!pData->pwHelpText)
		CreateHelpText(pData);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
	{
		pData->showHelp = 1;
		if (pData->modal)
		{
			gtk_widget_show(pData->pwHelpText);
		}
		CommandTextChange(GTK_ENTRY(pData->pwEntry), pData);
	}
	else
	{
		pData->showHelp = 0;
		if (pData->modal)
		{
			gtk_widget_hide(pData->pwHelpText);
		}
	}
	gtk_widget_grab_focus(pData->pwEntry);
}

/* Capitalize first letter of each word */
static void Capitalize(char* str)
{
	int cap = 1;
	while (*str)
	{
		if (cap)
		{
			if (*str >= 'a' && *str <= 'z')
				*str += 'A' - 'a';
			cap = 0;
		}
		else
		{
			if (*str == ' ')
				cap = 1;
			if (*str >= 'A' && *str <= 'Z')
				*str -= 'A' - 'a';
		}
		str++;
	}
}

extern gboolean CommandKeyPress(GtkWidget *widget, GdkEventKey *event, struct CommandEntryData_T *pData)
{
	short k = event->keyval;

	if (k == KEY_TAB)
	{	/* Tab press - auto complete */
		command *pc;
		char szCommand[128], szUsage[128];
		command cTop = { NULL, NULL, NULL, NULL, acTop };
		if ((pc = FindHelpCommand(&cTop, gtk_editable_get_chars( GTK_EDITABLE( pData->pwEntry ), 0, -1 ), szCommand, szUsage)))
		{
			Capitalize(szCommand);
			gtk_entry_set_text( GTK_ENTRY( pData->pwEntry ), szCommand );
		}
		/* Gtk 1 not good at stoping focus moving - so just move back later */
		pData->completing = 1;
	}
	return FALSE;
}

extern gboolean CommandFocusIn(GtkWidget *widget, GdkEventFocus *event, struct CommandEntryData_T *pData)
{
	if (pData->completing)
	{
		/* Gtk 1 not good at stoping focus moving - so just move back now */
		pData->completing = 0;
		gtk_widget_grab_focus( pData->pwEntry );
		gtk_editable_set_position(GTK_EDITABLE(pData->pwEntry), strlen(gtk_editable_get_chars( GTK_EDITABLE( pData->pwEntry ), 0, -1 )));
		return TRUE;
	}
	else
		return FALSE;
}	

typedef struct _newwidget {
  GtkWidget *pwG, *pwM, *pwS, *pwP, *pwCPS, *pwGNUvsHuman,
      *pwHumanHuman, *pwManualDice, *pwTutorMode;
  GtkAdjustment *padjML;
} newwidget;

static GtkWidget *
button_from_image ( GtkWidget *pwImage ) {

  GtkWidget *pw = gtk_button_new ();

  gtk_container_add ( GTK_CONTAINER ( pw ), pwImage );

  return pw;

}
static void UpdatePlayerSettings( newwidget *pnw ) {
	
  int fManDice = 
	  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwManualDice));
  int fTM = 
	  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwTutorMode));
  int fCPS = 
	  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwCPS));
  int fGH = 
	  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwGNUvsHuman));
  int fHH = 
	  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwHumanHuman));
  
  if(!fCPS){
	  if (fGH){
		  UserCommand("set player 0 gnubg");
		  UserCommand("set player 1 human");
	  }
	  if (fHH){
		  UserCommand("set player 0 human");
		  UserCommand("set player 1 human");
	  }
  }

  
  if((fManDice) && (rngCurrent != RNG_MANUAL))
	  UserCommand("set rng manual");
  
  if((!fManDice) && (rngCurrent == RNG_MANUAL))
	  UserCommand("set rng mersenne");
  
  if((fTM) && (!fTutor))
	  UserCommand("set tutor mode on");

  if((!fTM) && (fTutor))
	  UserCommand("set tutor mode off");

}

static void SettingsPressed( GtkWidget *pw, gpointer data ) {
  SetPlayers( NULL, 0, NULL);
}

static void ToolButtonPressedMS( GtkWidget *pw, newwidget *pnw ) {
  UpdatePlayerSettings( pnw ); 
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
  UserCommand("new session");
}

static void ToolButtonPressed( GtkWidget *pw, newwidget *pnw ) {
  char sz[40];
  int *pi;

  pi = (int *) gtk_object_get_user_data ( GTK_OBJECT ( pw ) );
  sprintf(sz, "new match %d", *pi);
  UpdatePlayerSettings( pnw );
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
  UserCommand(sz);
}

static GtkWidget *NewWidget( newwidget *pnw){
  int i, j = 1 ;
  char **apXPM[10];
  GtkWidget *pwVbox, *pwHbox, *pwLabel, *pwToolbar;
  GtkWidget *pwSpin, *pwButtons, *pwFrame, *pwVbox2; 
#include "xpm/stock_new_all.xpm"
#include "xpm/stock_new_money.xpm"
  pwVbox = gtk_vbox_new(FALSE, 0);
  pwToolbar = gtk_toolbar_new ();
  gtk_toolbar_set_orientation ( GTK_TOOLBAR ( pwToolbar ),
                                GTK_ORIENTATION_HORIZONTAL );
  gtk_toolbar_set_style ( GTK_TOOLBAR ( pwToolbar ),
                          GTK_TOOLBAR_ICONS );
  pwFrame = gtk_frame_new(_("Shortcut buttons"));
  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pwFrame, TRUE, TRUE, 0);
  gtk_container_add( GTK_CONTAINER( pwFrame ), pwToolbar);
  gtk_container_set_border_width( GTK_CONTAINER( pwToolbar ), 4);
  
  pwButtons = button_from_image( image_from_xpm_d ( stock_new_money_xpm,
                                                      pwToolbar ) );
  gtk_toolbar_append_widget( GTK_TOOLBAR( pwToolbar ),
                   pwButtons, _("Start a new money game session"), NULL );
  gtk_signal_connect( GTK_OBJECT( pwButtons ), "clicked",
	    GTK_SIGNAL_FUNC( ToolButtonPressedMS ), pnw );

  gtk_toolbar_append_space(GTK_TOOLBAR(pwToolbar));

  apXPM[0] = stock_new_xpm;
  apXPM[1] = stock_new1_xpm;
  apXPM[2] = stock_new3_xpm;
  apXPM[3] = stock_new5_xpm;
  apXPM[4] = stock_new7_xpm;
  apXPM[5] = stock_new9_xpm;
  apXPM[6] = stock_new11_xpm;
  apXPM[7] = stock_new13_xpm;
  apXPM[8] = stock_new15_xpm;
  apXPM[9] = stock_new17_xpm;
  
  for(i = 1; i < 19; i=i+2, j++ ){			     
     char sz[40];
     int *pi;

     sprintf(sz, _("Start a new %d point match"), i);
     pwButtons = button_from_image( image_from_xpm_d ( apXPM[j],
                                                      pwToolbar ) );
     gtk_toolbar_append_widget( GTK_TOOLBAR( pwToolbar ),
                             pwButtons, sz, NULL );
     
      pi = malloc ( sizeof ( int ) );
      *pi = i;
     gtk_object_set_data_full( GTK_OBJECT( pwButtons ), "user_data",
                                  pi, free );

     gtk_signal_connect( GTK_OBJECT( pwButtons ), "clicked",
		    GTK_SIGNAL_FUNC( ToolButtonPressed ), pnw );
  }
  pnw->pwG = gtk_radio_button_new_with_label(NULL, _("Game"));
  pnw->pwM =
       gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pnw->pwG),
      _("Match"));
  pnw->pwS =
       gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pnw->pwG),
       _("Money game session"));
  pnw->pwP =
       gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pnw->pwG),
       _("Position"));
  
  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pnw->pwG, FALSE, TRUE, 0);
  pwHbox = gtk_hbox_new(TRUE, 0);
  
  gtk_box_pack_start ( GTK_BOX ( pwHbox ), pnw->pwM, FALSE, TRUE, 0);
  
  gtk_box_pack_start ( GTK_BOX ( pwHbox ),
		  pwLabel = gtk_label_new(_("Match length:")), TRUE, TRUE, 0);
  
  pnw->padjML = GTK_ADJUSTMENT( gtk_adjustment_new( nDefaultLength, 
			  1, 99, 1, 10, 10 ) );
  
  gtk_label_set_justify (GTK_LABEL (pwLabel), GTK_JUSTIFY_RIGHT);
  pwSpin = gtk_spin_button_new (GTK_ADJUSTMENT(pnw->padjML), 1, 0);
  
  gtk_box_pack_start ( GTK_BOX ( pwHbox ), pwSpin , FALSE, TRUE, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pwSpin), TRUE);

  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pwHbox, FALSE, TRUE, 0);
  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pnw->pwS, FALSE, TRUE, 0);
  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pnw->pwP, FALSE, TRUE, 0);

  /* Here the simplified player settings starts */
  
  pwFrame = gtk_frame_new(_("Player settings"));
  
  pwHbox = gtk_hbox_new(FALSE, 0);
  pwVbox2 = gtk_vbox_new(FALSE, 0);

  gtk_container_add(GTK_CONTAINER(pwHbox), pwVbox2);

  pnw->pwCPS = gtk_radio_button_new_with_label( NULL, _("Current player settings"));
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwCPS, FALSE, FALSE, 0);
  
  pnw->pwGNUvsHuman = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS), _("GNU Backgammon vs. Human"));
  pnw->pwHumanHuman = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS), _("Human vs. Human"));
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwGNUvsHuman, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwHumanHuman, FALSE, FALSE, 0);
  
  pwButtons = gtk_button_new_with_label(_("Modify player settings..."));
  gtk_container_set_border_width(GTK_CONTAINER(pwButtons), 10);
  
  gtk_container_add(GTK_CONTAINER(pwVbox2), pwButtons );

  gtk_signal_connect(GTK_OBJECT(pwButtons), "clicked",
      GTK_SIGNAL_FUNC( SettingsPressed ), NULL );

  pwVbox2 = gtk_vbox_new( FALSE, 0);
  
  gtk_container_add(GTK_CONTAINER(pwHbox), pwVbox2);

  pnw->pwManualDice = gtk_check_button_new_with_label(_("Manual dice"));
  pnw->pwTutorMode = gtk_check_button_new_with_label(_("Tutor mode"));

  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwManualDice, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwTutorMode, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pwFrame), pwHbox);
  gtk_container_add(GTK_CONTAINER(pwVbox), pwFrame);
  
  return pwVbox;
  
} 

static void NewOK( GtkWidget *pw, newwidget *pnw ) {
  
  int G = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwG));
  int M = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwM));
  int S = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwS));
  int P = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pnw->pwP));
  int Mlength = pnw->padjML->value; 

  UpdatePlayerSettings(pnw);

  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

  if (G)
	  UserCommand("new game");
  else if(M){
	  char sz[40];
	  sprintf(sz, "new match %d", Mlength );
	  UserCommand(sz);
  }
  else if(S)
	  UserCommand("new session");
  else if(P){
	  /* FIXME */
  }

}

static void NewSet( newwidget *pnw) {
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pnw->pwM ), TRUE );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pnw->pwTutorMode ),
                                fTutor );
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pnw->pwManualDice), rngCurrent == RNG_MANUAL);
  gtk_adjustment_set_value( GTK_ADJUSTMENT( pnw->padjML ), nDefaultLength );

}

static void NewClicked( gpointer *p, guint n, GtkWidget *pw ) {
  GTKNew();
}

static void OpenClicked( gpointer *p, guint n, GtkWidget *pw ) {
  GTKOpen();
}

static void SaveClicked( gpointer *p, guint n, GtkWidget *pw ) {
  GTKSave();
}

static void ImportClicked( gpointer *p, guint n, GtkWidget *pw ) {
  GTKImport();
}

static void ExportClicked( gpointer *p, guint n, GtkWidget *pw ) {
  GTKExport();
}

extern void 
GTKOpen( void ) {

  char *sz = getDefaultPath ( PATH_SGF );

  GTKFileCommand(_("Open match, session, game or position"), 
		   sz, "load match", "sgf", FDT_NONE_OPEN, PATH_SGF);
  if ( sz ) 
    free ( sz );
}

extern void 
GTKSave( void ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  GTKFileCommand(_("Save match, session, game or position"), 
		   sz, "save", "sgf", FDT_SAVE, PATH_SGF);
  if ( sz ) 
    free ( sz );
}

extern void 
GTKImport( void ) {

	/* Order of import types in menu */
	int impTypes[] = {PATH_BKG, PATH_MAT, PATH_POS, PATH_OLDMOVES, PATH_SGG,
		PATH_TMG, PATH_MAT, PATH_SNOWIE_TXT};
	char* sz = NULL;

	if (lastImportType != -1)
		sz = getDefaultPath(impTypes[lastImportType]);

	GTKFileCommand(_("Import match, session, game or position"), 
		   sz, "import", "N", FDT_IMPORT, impTypes[lastImportType]);

	if (sz)
		free(sz);
}

extern void 
GTKExport( void ) {

	/* Order of export types in menu */
	int expTypes[] = {PATH_HTML, PATH_GAM, PATH_POS, PATH_MAT, PATH_GAM, 
		PATH_LATEX, PATH_PDF, PATH_POSTSCRIPT, PATH_EPS, -1, PATH_TEXT, -1, PATH_SNOWIE_TXT};
	char* sz = NULL;

	if (lastExportType != -1 && expTypes[lastExportType] != -1)
		sz = getDefaultPath(expTypes[lastExportType]);

	GTKFileCommand(_("Export match, session, game or position"), 
		   sz, "export", "N", FDT_EXPORT_FULL, expTypes[lastExportType]);

	if (sz)
		free(sz);
}

extern void GTKNew( void ){
	
  GtkWidget *pwDialog, *pwPage;
  newwidget nw;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - New"),
			   DT_QUESTION, GTK_SIGNAL_FUNC( NewOK ), &nw );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
 		        pwPage = NewWidget(&nw));

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );

  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
  
  gtk_widget_show_all( pwDialog );

  NewSet( &nw );
 
  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

}

typedef struct _filethings {
	GtkWidget *pwom;
	GtkWidget *pwRBMatch, *pwRBGame, *pwRBPosition;
	filedialogtype fdt;
	int n;
	char *pch;
} filethings;

static GtkWidget *
get_menu_item( GtkWidget *pwMenu, const gint index ) {

   GList *children = GTK_MENU_SHELL( pwMenu )->children;
   GList *child = g_list_nth( children, index );
   return GTK_WIDGET( child->data );
}

static void
ClickedRadioButton(GtkWidget *pw, filethings *pft) {
  
  GtkWidget *pwMenu = gtk_option_menu_get_menu(GTK_OPTION_MENU(pft->pwom));
  int i;
  int afSens[13][3] = { { TRUE, TRUE, TRUE },     /* HTML */
                        { FALSE, FALSE, TRUE },   /* GammOnLine */
                        { FALSE, FALSE, TRUE },   /* .pos */
                        { TRUE, FALSE, FALSE },   /* .mat */
                        { FALSE, TRUE, FALSE },   /* .gam */
                        { TRUE, TRUE, FALSE  },   /* LaTeX */
                        { TRUE, TRUE, FALSE  },   /* PDF */
                        { TRUE, TRUE, FALSE  },   /* PostScript */
                        { FALSE, FALSE, TRUE },   /* EPS */
                        { FALSE, FALSE, TRUE },   /* PNG */
			{ TRUE, TRUE, TRUE },     /* Text */
			{ TRUE, TRUE, FALSE },    /* Eq. evolution */
                        { FALSE, FALSE, TRUE }};  /* Snowie txt position */

  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pft->pwRBMatch)))
    for (i = 0; i < 13; i++)
      gtk_widget_set_sensitive(GTK_WIDGET( get_menu_item( pwMenu, i )),
	    afSens[i][0]);

  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pft->pwRBGame)))
    for (i = 0; i < 13; i++)
      gtk_widget_set_sensitive(GTK_WIDGET( get_menu_item( pwMenu, i )),
	    afSens[i][1]);

  if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pft->pwRBPosition)))
    for (i = 0; i < 13; i++)
      gtk_widget_set_sensitive(GTK_WIDGET( get_menu_item( pwMenu, i )),
	    afSens[i][2]);
}
  
static void FileOK( GtkWidget *pw, filethings *pft ) {
	
    static char *aszImpFormat[] = { "bkg", "mat", "pos", "oldmoves", "sgg",
	    "tmg", "mat", "snowietxt"};
    static char *aszExpFormat[] = { "html", "gammonline", "pos", "mat", "gam",
	    "latex", "pdf", "postscript", "eps", "png", "text" ,
            "equityevolution", "snowietxt" };

    GtkWidget *pwFile = gtk_widget_get_toplevel( pw );
    
    switch( pft->fdt) {
      case FDT_NONE:
      case FDT_NONE_OPEN:
      case FDT_EXPORT:
        pft->pch = g_strdup_printf("\"%s\"",
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	break;

      case FDT_IMPORT:
		pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));
		lastImportType = pft->n;
		pft->pch = g_strdup_printf("%s \"%s\"", aszImpFormat[pft->n], 
		gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
        break;

      case FDT_SAVE:
      case FDT_NONE_SAVE:
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBMatch )))
           pft->pch = g_strdup_printf("match \"%s\"",  
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBGame)))
           pft->pch = g_strdup_printf("game \"%s\"",  
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBPosition )))
           pft->pch = g_strdup_printf("position \"%s\"",  
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );

        break;
      case FDT_EXPORT_FULL:
        pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));
        lastExportType = pft->n;
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBMatch )))
           pft->pch = g_strdup_printf("match %s \"%s\"", aszExpFormat[pft->n],  
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBGame)))
           pft->pch = g_strdup_printf("game %s \"%s\"",  aszExpFormat[pft->n], 
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pft->pwRBPosition )))
           pft->pch = g_strdup_printf("position %s \"%s\"", aszExpFormat[pft->n], 
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );

    }
    
    gtk_widget_destroy( pwFile );
}
static void 
GetDefaultFilename ( GtkWidget *pw, filethings *pft ) {

  GtkWidget *pwFileDialog = gtk_widget_get_toplevel(pw);
  char *sz;
  pathformat apf[13] = { PATH_HTML, PATH_HTML, PATH_POS, PATH_MAT, PATH_GAM,
	  PATH_LATEX, PATH_PDF, PATH_POSTSCRIPT, PATH_EPS, PATH_PNG, PATH_TEXT,
	  PATH_TEXT, PATH_SNOWIE_TXT };
	  
  pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));

  sz = getDefaultFileName ( apf[pft->n] );
  if( sz )
     gtk_file_selection_set_filename( GTK_FILE_SELECTION( pwFileDialog ), sz );
  
  if( sz )
    free ( sz );
}

static void 
GetDefaultPath ( GtkWidget *pw, filethings *pft ) {

  GtkWidget *pwFileDialog = gtk_widget_get_toplevel(pw);
  char *sz;
  pathformat apf[8] = { PATH_BKG, PATH_MAT, PATH_POS, PATH_OLDMOVES, PATH_SGG,
	  PATH_TMG, PATH_MAT, PATH_SNOWIE_TXT };
	  
  pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));

  sz = getDefaultPath ( apf[pft->n] );
  if( sz )
     gtk_file_selection_set_filename( GTK_FILE_SELECTION( pwFileDialog ), sz );
  
  if( sz )
    free ( sz );
}
static void SetDefaultPathFromImportExport( GtkWidget *pw, filethings *pft ) {

    char *pch;
    char *pc;
    char *szFile = g_strdup ( gtk_file_selection_get_filename(
			       GTK_FILE_SELECTION( gtk_widget_get_ancestor(
                               pw, GTK_TYPE_FILE_SELECTION ) ) ) );
    char sz[1028];
    static char *aszImport[8] = { "bgk", "mat" , "pos", "oldmoves", "sgg",
      "tmg", "mat", "snowietxt" };
    static char *aszExport[13] = { "html", "html", "pos", "mat", "gam",
	    "latex", "pdf", "postscript", "eps", "png", "text" ,
            "text", "snowietxt" };

	    
    pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));
    
    if (pft->fdt == FDT_IMPORT)
       sprintf(sz, "%s", aszImport[pft->n]);
    if (pft->fdt == FDT_EXPORT_FULL)
       sprintf(sz, "%s", aszExport[pft->n]);
    
    if ( ( pc = strrchr ( szFile, DIR_SEPARATOR ) ) )
      *pc = 0;

    pch = g_strdup_printf( "set path %s \"%s\"", sz, szFile );
    g_free ( szFile );

    UserCommand( pch );
    g_free( pch );
}

static void SetDefaultPath( GtkWidget *pw, char *sz ) {

    char *pch;
    char *pc;
    char *szFile = g_strdup ( gtk_file_selection_get_filename(
			       GTK_FILE_SELECTION( gtk_widget_get_ancestor(
                               pw, GTK_TYPE_FILE_SELECTION ) ) ) );

    if ( ( pc = strrchr ( szFile, DIR_SEPARATOR ) ) )
      *pc = 0;

    pch = g_strdup_printf( "set path %s \"%s\"", sz, szFile );
    g_free ( szFile );

    UserCommand( pch );
    g_free( pch );
}

extern char *SelectFile( char *szTitle, char *szDefault, char *szPath,
		filedialogtype fdt ) {

    GtkWidget *pw = gtk_file_selection_new( szTitle ),
	*pwButton, *pwExpSet; 

    filethings ft;
    ft.pch = NULL;
    ft.fdt = fdt;
    ft.n = 0;
    ft.pwom = NULL;
    
    if( szPath ) {
	pwButton = gtk_button_new_with_mnemonic( _("Set Default _Path") );
	
	gtk_widget_show( pwButton );
	gtk_container_add( GTK_CONTAINER(
			       gtk_widget_get_parent(
				   GTK_FILE_SELECTION( pw )->fileop_c_dir ) ),
			   pwButton );
	if ((fdt == FDT_IMPORT) || (fdt == FDT_EXPORT_FULL))
	    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			    GTK_SIGNAL_FUNC( SetDefaultPathFromImportExport ),
			    &ft );
        else
	    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			    GTK_SIGNAL_FUNC( SetDefaultPath ), szPath );
	
    }
    
    if( (fdt == FDT_EXPORT) || (fdt == FDT_EXPORT_FULL) ) {
	    
	pwExpSet = gtk_button_new_with_label( _("Export settings...") );
	
	gtk_container_set_border_width(GTK_CONTAINER(pwExpSet), 5);
	
	gtk_widget_show( pwExpSet );
	gtk_container_add( GTK_CONTAINER(
			       gtk_widget_get_parent(
				   GTK_FILE_SELECTION( pw )->ok_button ) ),
			   pwExpSet );
	gtk_signal_connect( GTK_OBJECT( pwExpSet ), "clicked",
			    GTK_SIGNAL_FUNC( CommandShowExport ), szPath );
    }
    
    if( fdt == FDT_SAVE ) {
	GtkWidget *pwFrame, *pwVbox;

	pwFrame = gtk_frame_new(ms.nMatchTo ? _("Save match, game or position") :
			_("Save session, game or position"));
	ft.pwRBMatch = gtk_radio_button_new_with_label( NULL, 
			ms.nMatchTo ? _("Match") : _("Session"));
	ft.pwRBGame = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(ft.pwRBMatch), _("Game")); 
	ft.pwRBPosition = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(ft.pwRBMatch), _("Position")); 

	pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBMatch);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBGame);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBPosition);
	gtk_container_add(GTK_CONTAINER(pwFrame), pwVbox);

        gtk_container_add( GTK_CONTAINER( 
            gtk_widget_get_parent(gtk_widget_get_parent(
	    GTK_FILE_SELECTION( pw )->ok_button ) )), pwFrame);
	gtk_widget_show_all(pwFrame);
    }
    
    if( fdt == FDT_EXPORT_FULL ) {
	GtkWidget *pwFrame, *pwHbox, *pwVbox, *pwGetDefault, *pwm, *pwMI;
        char **ppch;
        static char *aszExportFormats[] = { N_("HTML"),
	      N_("GammOnLine (HTML)"), N_(".pos"),
	      N_(".mat"), N_(".gam"),
	      N_("LaTeX"), N_("PDF"), 
	      N_("PostScript"),
	      N_("Encapsulated PostScript"),
	      N_("PNG"),
	      N_("Text"),
	      N_("Equity evolution"),
	      N_("Snowie .txt position file"),
              NULL };

	pwHbox = gtk_hbox_new(FALSE, 0);
	
	pwFrame = gtk_frame_new(ms.nMatchTo ? _("Save match, game or position") :
			_("Save session, game or position"));
	ft.pwRBMatch = gtk_radio_button_new_with_label( NULL, 
			ms.nMatchTo ? _("Match") : _("Session"));
	ft.pwRBGame = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(ft.pwRBMatch), _("Game")); 
	ft.pwRBPosition = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(ft.pwRBMatch), _("Position")); 


	gtk_signal_connect(GTK_OBJECT(ft.pwRBMatch), "clicked",
			GTK_SIGNAL_FUNC(ClickedRadioButton), &ft);

	gtk_signal_connect(GTK_OBJECT(ft.pwRBGame), "clicked",
			GTK_SIGNAL_FUNC(ClickedRadioButton), &ft);

	gtk_signal_connect(GTK_OBJECT(ft.pwRBPosition), "clicked",
			GTK_SIGNAL_FUNC(ClickedRadioButton), &ft);

	pwVbox = gtk_vbox_new(FALSE, 0);
	pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBMatch);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBGame);
	gtk_container_add(GTK_CONTAINER(pwVbox), ft.pwRBPosition);
	gtk_container_add(GTK_CONTAINER(pwFrame), pwVbox);
	gtk_container_add(GTK_CONTAINER(pwHbox), pwFrame);

	pwVbox = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(pwVbox), 
		gtk_label_new(_("Export to format: ")), FALSE, FALSE, 0);
	
        ft.pwom = gtk_option_menu_new ();
	
	gtk_container_set_border_width(GTK_CONTAINER(ft.pwom), 5);

	gtk_box_pack_start( GTK_BOX ( pwVbox ), ft.pwom, 
					  FALSE, FALSE, 4);
	pwm = gtk_menu_new ();
	for( ppch = aszExportFormats; *ppch; ++ppch )
		gtk_menu_append( GTK_MENU( pwm ),
			pwMI = gtk_menu_item_new_with_label(
			gettext( *ppch ) ) );

	gtk_widget_show_all( pwm );
	gtk_option_menu_set_menu( GTK_OPTION_MENU ( ft.pwom ), pwm );
	if (lastExportType != -1)
		gtk_option_menu_set_history( GTK_OPTION_MENU ( ft.pwom ), lastExportType );

	/* Get the sensitivities right */
	ClickedRadioButton( NULL, &ft );

	pwGetDefault = gtk_button_new_with_label(_("Get default filename"));
	gtk_container_set_border_width(GTK_CONTAINER(pwGetDefault), 5);
	gtk_signal_connect( GTK_OBJECT( pwGetDefault ), "clicked",
			    GTK_SIGNAL_FUNC( GetDefaultFilename ), &ft );
	gtk_box_pack_start(GTK_BOX(pwVbox), pwGetDefault, FALSE, FALSE, 2 );
	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox, FALSE, FALSE, 0 );
	
        gtk_container_add( GTK_CONTAINER( 
            gtk_widget_get_parent(gtk_widget_get_parent(
	    GTK_FILE_SELECTION( pw )->ok_button ) )), pwHbox);
	gtk_widget_show_all(pwHbox);
    }
    
    if(fdt == FDT_IMPORT){
	    
      GtkWidget *pwhbox, *pwMI, *pwm, *pwGetPath; 
      char **ppch;
      static char *aszImportFormats[] = { N_("BKG session"),
	      N_(".mat match"), N_(".pos position"),
	      N_("FIBS oldmoves"), N_("GamesGrid .sgg"), 
	      N_("TrueMoneyGames .tmg match"),
	      N_("Snowie standard text format"),
	      N_("Snowie .txt position file"),
              NULL };
      
      pwhbox = gtk_hbox_new(FALSE,0);
      gtk_box_pack_start(GTK_BOX(pwhbox), 
		gtk_label_new(_("Import from format: ")), FALSE, FALSE, 0);
      ft.pwom = gtk_option_menu_new ();
      gtk_box_pack_start( GTK_BOX ( pwhbox ), ft.pwom, 
                          FALSE, FALSE, 0);

      pwm = gtk_menu_new ();
      for( ppch = aszImportFormats; *ppch; ++ppch )
	gtk_menu_append( GTK_MENU( pwm ),
			 pwMI = gtk_menu_item_new_with_label(
			     gettext( *ppch ) ) );
      
      gtk_widget_show_all( pwm );
      gtk_option_menu_set_menu( GTK_OPTION_MENU ( ft.pwom ), pwm );
      if (lastImportType != -1)
		gtk_option_menu_set_history( GTK_OPTION_MENU ( ft.pwom ), lastImportType );

      pwGetPath = gtk_button_new_with_label(_("Go to default path"));
      gtk_container_set_border_width(GTK_CONTAINER(pwGetPath), 5);
      gtk_signal_connect( GTK_OBJECT( pwGetPath ), "clicked",
			    GTK_SIGNAL_FUNC( GetDefaultPath ), &ft );
	gtk_box_pack_start(GTK_BOX(pwhbox), pwGetPath, FALSE, FALSE, 2 );
      
      gtk_container_add( GTK_CONTAINER( 
          gtk_widget_get_parent(gtk_widget_get_parent(
          GTK_FILE_SELECTION( pw )->ok_button ) )), pwhbox);
      gtk_widget_show_all(pwhbox);
    }

    if( szDefault )
	gtk_file_selection_set_filename( GTK_FILE_SELECTION( pw ), szDefault );

    gtk_signal_connect( GTK_OBJECT( GTK_FILE_SELECTION( pw )->ok_button ),
			"clicked", GTK_SIGNAL_FUNC( FileOK ), &ft );
    gtk_signal_connect_object( GTK_OBJECT( GTK_FILE_SELECTION( pw )->
					   cancel_button ), "clicked",
			       GTK_SIGNAL_FUNC( gtk_widget_destroy ),
			       GTK_OBJECT( pw ) );
    
    gtk_window_set_modal( GTK_WINDOW( pw ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pw ), GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pw ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show( pw );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    return ft.pch;
}

extern void GTKFileCommand( char *szPrompt, char *szDefault, char *szCommand,
                       char *szPath, filedialogtype fdt, pathformat pathId ) {

  char *pch;

  if (fdt == FDT_NONE_OPEN || fdt == FDT_NONE_SAVE) {
    GTKFileCommand24(szPrompt, szDefault, szCommand, szPath, fdt, pathId);
    return;
  }

  if (pathId == PATH_NULL || fdt == FDT_NONE_OPEN || fdt == FDT_NONE_SAVE)
                     fdt = FDT_NONE;

    if( ( pch = SelectFile( szPrompt, szDefault, szPath, fdt ) ) ) {
	char *sz = g_alloca( strlen( pch ) + strlen( szCommand ) + 4 );
	sprintf( sz, "%s %s", szCommand, pch );
	UserCommand( sz );

	g_free( pch );
    }
}

static void LoadCommands( gpointer *p, guint n, GtkWidget *pw ) {

    GTKFileCommand( _("Open commands"), NULL, "load commands", NULL, FDT_NONE_OPEN, PATH_NULL );
}

extern void SetMET( GtkWidget *pw, gpointer p ) {

    char *pchMet = NULL, *pch = NULL;

    pchMet = getDefaultPath( PATH_MET );
    if ( pchMet && *pchMet )
	pch = PathSearch( ".", pchMet );
    else
	pch = PathSearch( "met/", szDataDirectory );

    if( pch && access( pch, R_OK ) ) {
	free( pch );
	pch = NULL;
    }
    
    GTKFileCommand( _("Set match equity table"), pch, "set matchequitytable ",
		 "met", FDT_NONE_OPEN, PATH_MET );

    /* update filename on option page */
    if ( p && GTK_WIDGET_VISIBLE ( p ) )
        gtk_label_set_text ( GTK_LABEL ( p ), ( char * ) miCurrent.szFileName );

    if( pch )
	free( pch );
    if( pchMet )
	free( pchMet );
}

static void CopyAsGOL( gpointer *p, guint n, GtkWidget *pw ) {

  UserCommand("export position gol2clipboard");

}

static void CopyAsIDs(gpointer *p, guint n, GtkWidget *pw)
{ /* Copy the position and match ids to the clipboard */
  char buffer[1024];

  sprintf(buffer, "%s %s\n%s %s\n", _("Position ID:"), PositionID(ms.anBoard),
    _("Match ID:"), MatchIDFromMatchState(&ms));

  GTKTextToClipboard(buffer);
}

static void ExportHTMLImages( gpointer *p, guint n, GtkWidget *pw ) {

    char *sz = strdup( PKGDATADIR "/html-images" );
    GTKFileCommand( _("Export HTML images"), sz, "export htmlimages", NULL, FDT_EXPORT, PATH_NULL );
    if ( sz ) 
	free ( sz );
}

typedef struct _evalwidget {
    evalcontext *pec;
    movefilter *pmf;
    GtkWidget *pwCubeful,
#if defined (REDUCTION_CODE)
        *pwReduced, 
#else
        *pwUsePrune,
#endif
        *pwDeterministic;
	
    GtkAdjustment *padjPlies, *padjSearchCandidates, *padjSearchTolerance,
	*padjNoise;
    int *pfOK;
  GtkWidget *pwOptionMenu;
  int fMoveFilter;
  GtkWidget *pwMoveFilter;
} evalwidget;

static void EvalGetValues ( evalcontext *pec, evalwidget *pew ) {

#if defined( REDUCTION_CODE )
  GtkWidget *pwMenu, *pwItem;
  int *pi;
#endif
  pec->nPlies = pew->padjPlies->value;
  pec->fCubeful =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pew->pwCubeful ) );

  /* reduced */
#if defined( REDUCTION_CODE )
  pwMenu = gtk_option_menu_get_menu ( GTK_OPTION_MENU ( pew->pwReduced ) );
  pwItem = gtk_menu_get_active ( GTK_MENU ( pwMenu ) );
  pi = (int *) gtk_object_get_user_data ( GTK_OBJECT ( pwItem ) );
  pec->nReduced = *pi;
#else
  pec->fUsePrune =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pew->pwUsePrune ) );
  
#endif

  pec->rNoise = pew->padjNoise->value;
  pec->fDeterministic =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( pew->pwDeterministic ) );
}


static void EvalChanged ( GtkWidget *pw, evalwidget *pew ) {

  int i;
  evalcontext ecCurrent;
  int fFound = FALSE;
  int fEval, fMoveFilter;

  EvalGetValues ( &ecCurrent, pew );

  /* update predefined settings menu */

  for ( i = 0; i < NUM_SETTINGS; i++ ) {

    fEval = ! cmp_evalcontext ( &aecSettings[ i ], &ecCurrent );
    fMoveFilter = ! aecSettings[ i ].nPlies ||
      ( ! pew->fMoveFilter || 
        equal_movefilters ( (movefilter (*)[MAX_FILTER_PLIES]) pew->pmf, 
                            aaamfMoveFilterSettings[ aiSettingsMoveFilter[ i ] ] ) );

    if ( fEval && fMoveFilter ) {

      /* current settings equal to a predefined setting */

      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwOptionMenu ), i );
      fFound = TRUE;
      break;

    }

  }


  /* user defined setting */

  if ( ! fFound )
    gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwOptionMenu ),
                                  NUM_SETTINGS );

  
  if ( pew->fMoveFilter )
    gtk_widget_set_sensitive ( GTK_WIDGET ( pew->pwMoveFilter ),
                               ecCurrent.nPlies );

}


static void EvalNoiseValueChanged( GtkAdjustment *padj, evalwidget *pew ) {

    gtk_widget_set_sensitive( pew->pwDeterministic, padj->value != 0.0f );

    EvalChanged ( NULL, pew );
    
}

static void EvalPliesValueChanged( GtkAdjustment *padj, evalwidget *pew ) {
#if defined (REDUCTION_CODE)
    gtk_widget_set_sensitive( pew->pwReduced, padj->value > 0 );
#else
    gtk_widget_set_sensitive( pew->pwUsePrune, padj->value > 0 );
#endif
    EvalChanged ( NULL, pew );

}

static void SettingsMenuActivate ( GtkWidget *pwItem,
                                   evalwidget *pew ) {
  
  evalcontext *pec;
  int *piSelected;
  

  piSelected = gtk_object_get_data ( GTK_OBJECT ( pwItem ), "user_data" );

  if ( *piSelected == NUM_SETTINGS )
    return; /* user defined */

  /* set all widgets to predefined values */

  pec = &aecSettings[ *piSelected ];
 
  gtk_adjustment_set_value ( pew->padjPlies, pec->nPlies );
  gtk_adjustment_set_value ( pew->padjNoise, pec->rNoise );

#if defined( REDUCTION_CODE )
  gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwReduced ), 
                                pec->nReduced );
#else
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwUsePrune ),
                                pec->fUsePrune );
#endif
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwCubeful ),
                                pec->fCubeful );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwDeterministic ),
                                pec->fDeterministic );

  if ( pew->fMoveFilter )
    MoveFilterSetPredefined ( pew->pwMoveFilter, 
                              aiSettingsMoveFilter[ *piSelected ] );

}

/*
 * Create widget for displaying evaluation settings
 *
 */

static GtkWidget *EvalWidget( evalcontext *pec, movefilter *pmf, 
                              int *pfOK, const int fMoveFilter ) {

    evalwidget *pew;
    GtkWidget *pwEval, *pw;

    GtkWidget *pwFrame, *pwFrame2;
    GtkWidget *pw2, *pw3;

    GtkWidget *pwMenu;
    GtkWidget *pwItem;

    GtkWidget *pwev;
#if defined( REDUCTION_CODE )
	GtkWidget *pw4;
	char *pch;
    const char *aszReduced[] = {
      N_("No reduction"),
      NULL,
      N_("50%% speed"),
      N_("33%% speed"),
      N_("25%% speed") 
    };
#endif

    int i;
    int *pi;

    if( pfOK )
	*pfOK = FALSE;

    pwEval = gtk_vbox_new( FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwEval ), 8 );
    
    pew = malloc( sizeof *pew );

    /*
     * Frame with prefined settings 
     */

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_container_add ( GTK_CONTAINER ( pwEval ), pwev );

    pwFrame = gtk_frame_new ( _("Predefined settings") );
    gtk_container_add ( GTK_CONTAINER ( pwev ), pwFrame );

    pw2 = gtk_vbox_new ( FALSE, 8 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw2 );
    gtk_container_set_border_width ( GTK_CONTAINER ( pw2 ), 8 );

    /* option menu with selection of predefined settings */

    gtk_container_add ( GTK_CONTAINER ( pw2 ),
                        gtk_label_new ( _("Select a predefined setting:") ) );

    gtk_tooltips_set_tip( ptt, pwev,
                          _("Select a predefined setting, ranging from "
                            "beginner's play to the grandmaster setting "
                            "that will test your patience"), NULL );

    pwMenu = gtk_menu_new ();

    for ( i = 0; i <= NUM_SETTINGS; i++ ) {

      if ( i < NUM_SETTINGS )
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label ( 
                          gettext ( aszSettings[ i ] ) ) );
      else
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label (
                          _("user defined") ) );

      pi = malloc ( sizeof ( int ) );
      *pi = i;
      gtk_object_set_data_full( GTK_OBJECT( pwItem ), "user_data", 
                                pi, free );

      gtk_signal_connect ( GTK_OBJECT ( pwItem ), "activate",
                           GTK_SIGNAL_FUNC ( SettingsMenuActivate ),
                           (void *) pew );

    }

    pew->pwOptionMenu = gtk_option_menu_new ();
    gtk_option_menu_set_menu ( GTK_OPTION_MENU ( pew->pwOptionMenu ), pwMenu );


    gtk_container_add ( GTK_CONTAINER ( pw2 ), pew->pwOptionMenu );
                                                               


    /*
     * Frame with user settings 
     */

    pwFrame = gtk_frame_new ( _("User defined settings") );
    gtk_container_add ( GTK_CONTAINER ( pwEval ), pwFrame );
    
    pw2 = gtk_vbox_new ( FALSE, 8 );
    gtk_container_set_border_width( GTK_CONTAINER( pw2 ), 8 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw2 );

    /* lookahead */

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwev );

    gtk_tooltips_set_tip( ptt, pwev,
                          _("Specify how many rolls GNU Backgammon should "
                            "lookahead. Each ply costs approximately a factor "
                            "of 21 in computational time. Also note that "
                            "2-ply is equivalent to Snowie's 3-ply setting."),
                          NULL );

    pwFrame2 = gtk_frame_new ( _("Lookahead") );
    gtk_container_add ( GTK_CONTAINER ( pwev ), pwFrame2 );

    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame2 ), pw );

    pew->padjPlies = GTK_ADJUSTMENT( gtk_adjustment_new( pec->nPlies, 0, 7,
							 1, 1, 0 ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( _("Plies:") ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_spin_button_new( pew->padjPlies, 1, 0 ) );

#if defined( REDUCTION_CODE )
    /* reduced evaluation */

    /* FIXME if and when we support different values for nReduced, this
       check button won't work */

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwev );

    gtk_tooltips_set_tip( ptt, pwev,
				/* xgettext: no-c-format */
                          _("Instead of averaging over all 21 possible "
                            "dice rolls it is possible to average over a "
                            "reduced set, for example 7 rolls for the 33% "
                            "speed option. The 33% speed option will "
                            "typically be three times faster than the "
                            "full search without reduction."), NULL );
                          
    pwFrame2 = gtk_frame_new ( _("Reduced evaluations") );
    gtk_container_add ( GTK_CONTAINER ( pwev ), pwFrame2 );
    pw4 = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame2 ), pw4 );

    pwMenu = gtk_menu_new ();
    for ( i = 0; i < 5; ++i ) {
      if ( i == 1 )
        continue;
      
      pch = g_strdup_printf( gettext( aszReduced[ i ] ) );
      pwItem = gtk_menu_item_new_with_label ( pch );
      g_free( pch );
      gtk_menu_append ( GTK_MENU ( pwMenu ), pwItem );
      pi = g_malloc ( sizeof ( int ) );
      *pi = i;
      gtk_object_set_data_full ( GTK_OBJECT ( pwItem ), "user_data", 
                                 pi, g_free );

    }
    
    gtk_signal_connect ( GTK_OBJECT( pwMenu ), "selection-done",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    pew->pwReduced = gtk_option_menu_new ();
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pew->pwReduced ), pwMenu );

    gtk_container_add( GTK_CONTAINER( pw4 ), pew->pwReduced );

    /* UGLY fix for the fact the menu has entries only for values
     * 0, 2, 3, 4 
     */
	gtk_option_menu_set_history( GTK_OPTION_MENU( pew->pwReduced ), 
                                 (pec->nReduced < 2) ? 0 : 
                                  pec->nReduced - 1 );
#else
	
    /* Use pruning neural nets */
    
    pwFrame2 = gtk_frame_new ( _("Pruning neural nets") );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    gtk_container_add( GTK_CONTAINER( pwFrame2 ),
		       pew->pwUsePrune = gtk_check_button_new_with_label(
			   _("Use neural net pruning") ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwUsePrune ),
				  pec->fUsePrune );
    /* FIXME This needs a tool tip */

#endif

    /* cubeful */
    
    pwFrame2 = gtk_frame_new ( _("Cubeful evaluations") );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    gtk_container_add( GTK_CONTAINER( pwFrame2 ),
		       pew->pwCubeful = gtk_check_button_new_with_label(
			   _("Cubeful chequer evaluation") ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwCubeful ),
				  pec->fCubeful );

    if ( fMoveFilter ) 
      /* checker play */
      gtk_tooltips_set_tip( ptt, pew->pwCubeful,
                            _("Instruct GNU Backgammon to use cubeful "
                              "evaluations, i.e., include the value of "
                              "cube ownership in the evaluations. It is "
                              "recommended to enable this option."), NULL );
    else
      /* checker play */
      gtk_tooltips_set_tip( ptt, pew->pwCubeful,
                            _("GNU Backgammon will always perform "
                              "cubeful evaluations for cube decisions. "
                              "Disabling this option will make GNU Backgammon "
                              "use cubeless evaluations in the interval nodes "
                              "of higher ply evaluations. It is recommended "
                              "to enable this option"), NULL );

    /* noise */

    pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwev );

    gtk_tooltips_set_tip( ptt, pwev,
                          _("You can use this option to introduce noise "
                            "or errors in the evaluations. This is useful for "
                            "introducing levels below 0-ply. The lower rated "
                            "bots (e.g., GGotter) on the GamesGrid backgammon "
                            "server uses this technique. "
                            "The introduced noise can be "
                            "deterministic, i.e., always the same noise for "
                            "the same position, or it can be random"), NULL );

    pwFrame2 = gtk_frame_new ( _("Noise") );
    gtk_container_add ( GTK_CONTAINER ( pwev ), pwFrame2 );

    pw3 = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame2 ), pw3 );


    pew->padjNoise = GTK_ADJUSTMENT( gtk_adjustment_new(
	pec->rNoise, 0, 1, 0.001, 0.001, 0.0 ) );
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pw3 ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( _("Noise:") ) );
    gtk_container_add( GTK_CONTAINER( pw ), gtk_spin_button_new(
	pew->padjNoise, 0.001, 3 ) );
    
    gtk_container_add( GTK_CONTAINER( pw3 ),
		       pew->pwDeterministic = gtk_check_button_new_with_label(
			   _("Deterministic noise") ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwDeterministic ),
				  pec->fDeterministic );

    /* misc data */

    pew->fMoveFilter = fMoveFilter;
    pew->pec = pec;
    pew->pmf = pmf;
    pew->pfOK = pfOK;

    /* move filter */

    if ( fMoveFilter ) {
      
      pew->pwMoveFilter = MoveFilterWidget ( pmf, pfOK, 
                                             GTK_SIGNAL_FUNC ( EvalChanged ), 
                                             pew );

      pwev = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(pwev), FALSE);
      gtk_container_add ( GTK_CONTAINER ( pwEval ), pwev ); 
      gtk_container_add ( GTK_CONTAINER ( pwev ), pew->pwMoveFilter );

      gtk_tooltips_set_tip( ptt, pwev,
                            _("GNU Backgammon will evaluate all moves at "
                              "0-ply. The move filter controls how many "
                              "moves to be evaluted at higher plies. "
                              "A \"smaller\" filter will be faster, but "
                              "GNU Backgammon may not find the best move. "
                              "Power users may set up their own filters "
                              "by clicking on the [Modify] button"), NULL );

    }
    else
      pew->pwMoveFilter = NULL;

    /* setup signals */

    gtk_signal_connect( GTK_OBJECT( pew->padjPlies ), "value-changed",
			GTK_SIGNAL_FUNC( EvalPliesValueChanged ), pew );
    EvalPliesValueChanged( pew->padjPlies, pew );
    
    gtk_signal_connect( GTK_OBJECT( pew->padjNoise ), "value-changed",
			GTK_SIGNAL_FUNC( EvalNoiseValueChanged ), pew );
    EvalNoiseValueChanged( pew->padjNoise, pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwDeterministic ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwCubeful ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );
    
#if defined (REDUCTION_CODE)
    gtk_signal_connect ( GTK_OBJECT( pew->pwReduced ), "changed",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );
#else
    gtk_signal_connect ( GTK_OBJECT( pew->pwUsePrune ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );
#endif 
    
    gtk_object_set_data_full( GTK_OBJECT( pwEval ), "user_data", pew, free );

    return pwEval;
}

static void EvalOK( GtkWidget *pw, void *p ) {

    GtkWidget *pwEval = GTK_WIDGET( p );
    evalwidget *pew = gtk_object_get_user_data( GTK_OBJECT( pwEval ) );

    if( pew->pfOK )
	*pew->pfOK = TRUE;

    EvalGetValues ( pew->pec, pew );
	
    if( pew->pfOK )
	gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void SetEvalCommands( char *szPrefix, evalcontext *pec,
			     evalcontext *pecOrig ) {

    char sz[ 256 ];

    outputpostpone();

    if( pec->nPlies != pecOrig->nPlies ) {
	sprintf( sz, "%s plies %d", szPrefix, pec->nPlies );
	UserCommand( sz );
    }

#if defined( REDUCTION_CODE )
    if( pec->nReduced != pecOrig->nReduced ) {
	sprintf( sz, "%s reduced %d", szPrefix, pec->nReduced );
	UserCommand( sz );
    }
#else
    if( pec->fUsePrune != pecOrig->fUsePrune ) {
	sprintf( sz, "%s prune %s", szPrefix, pec->fUsePrune ? "on" : "off" );
	UserCommand( sz );
    }
#endif

    if( pec->fCubeful != pecOrig->fCubeful ) {
	sprintf( sz, "%s cubeful %s", szPrefix, pec->fCubeful ? "on" : "off" );
	UserCommand( sz );
    }

    if( pec->rNoise != pecOrig->rNoise ) {
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	sprintf( sz, "%s noise %s", szPrefix, 
                                  g_ascii_formatd( buf, G_ASCII_DTOSTR_BUF_SIZE,
			           "%.3f", pec->rNoise ));
	UserCommand( sz );
    }
    
    if( pec->fDeterministic != pecOrig->fDeterministic ) {
	sprintf( sz, "%s deterministic %s", szPrefix, pec->fDeterministic ?
		 "on" : "off" );
	UserCommand( sz );
    }

    outputresume();
}


typedef struct _setevalwidget {
  GtkWidget *pwCube, *pwChequer;
  int *pfOK;
} setevalwidget;


static void
EvaluationOK ( GtkWidget *pw, setevalwidget *psew ) {

  evalwidget *pew;

  if ( psew->pfOK )
    *psew->pfOK = TRUE;

  pew = gtk_object_get_user_data ( GTK_OBJECT ( psew->pwChequer ) );
  EvalGetValues ( pew->pec, pew );

  pew = gtk_object_get_user_data ( GTK_OBJECT ( psew->pwCube ) );
  EvalGetValues ( pew->pec, pew );

  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}


extern void
SetEvaluation ( gpointer *p, guint n, GtkWidget *pw ) {


    evalcontext ecChequer, ecCube;
    GtkWidget *pwDialog;
    GtkWidget *pwhbox, *pwFrame;
    GtkWidget *pwvbox;
    int fOK = FALSE;
    movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];
    setevalwidget sew;
    
    memcpy( &ecChequer, &esEvalChequer.ec, sizeof ecChequer );
    memcpy( &ecCube, &esEvalCube.ec, sizeof ecCube );
    memcpy( aamf, aamfEval, sizeof ( aamfEval ) );

    /* widgets */

    sew.pwChequer = EvalWidget( &ecChequer, (movefilter *) aamf, NULL, TRUE );
    sew.pwCube = EvalWidget( &ecCube, NULL, NULL, FALSE );
    sew.pfOK = &fOK;

    pwhbox = gtk_hbox_new ( FALSE, 4 );

    pwFrame = gtk_frame_new ( _("Chequer play") );
    gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwFrame, FALSE, FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), sew.pwChequer );
    
    pwFrame = gtk_frame_new ( _("Cube decisions") );
    gtk_box_pack_start ( GTK_BOX ( pwhbox ), pwFrame, FALSE, FALSE, 0 );
    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwvbox );
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), sew.pwCube, FALSE, FALSE, 0 );
    
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Evaluation settings"), 
                             DT_QUESTION,
                             GTK_SIGNAL_FUNC( EvaluationOK ), &sew );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwhbox );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK ) {

      SetEvalCommands( "set evaluation chequer eval", &ecChequer,
                       &esEvalChequer.ec );
      SetEvalCommands( "set evaluation cubedecision eval", &ecCube,
                       &esEvalCube.ec );
      SetMovefilterCommands ( "set evaluation movefilter",
                              aamf, aamfEval );
    }



}


typedef struct _playerswidget {
    int *pfOK;
    player *ap;
    GtkWidget *apwName[ 2 ], *apwRadio[ 2 ][ 4 ], *apwEvalCube[ 2 ], *apwEvalChequer[ 2 ],
	*apwSocket[ 2 ], *apwExternal[ 2 ];
    char aszSocket[ 2 ][ 128 ];
} playerswidget;

static void PlayerTypeToggled( GtkWidget *pw, playerswidget *ppw ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	gtk_widget_set_sensitive(
	    ppw->apwEvalCube[ i ], gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON( ppw->apwRadio[ i ][ 1 ] ) ) );
	gtk_widget_set_sensitive(
	    ppw->apwEvalChequer[ i ], gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON( ppw->apwRadio[ i ][ 1 ] ) ) );
	gtk_widget_set_sensitive(
	    ppw->apwExternal[ i ], gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON( ppw->apwRadio[ i ][ 3 ] ) ) );
    }
}

static GtkWidget *PlayersPage( playerswidget *ppw, int i ) {

    GtkWidget *pwPage, *pw;
    static int aiRadioButton[ PLAYER_PUBEVAL + 1 ] = { 3, 0, 1, 2 };
    GtkWidget *pwHBox;
    GtkWidget *pwFrame;
    GtkWidget *pwvbox;

    pwPage = gtk_vbox_new( FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwPage ), 8 );
    
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwPage ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( _("Name:") ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       ppw->apwName[ i ] = gtk_entry_new() );
    gtk_entry_set_text( GTK_ENTRY( ppw->apwName[ i ] ),
			(ppw->ap[ i ].szName) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 0 ] =
		       gtk_radio_button_new_with_label( NULL, _("Human") ) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 1 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   _("GNU Backgammon") ) );

    pwHBox = gtk_hbox_new ( FALSE, 4 );
    gtk_container_add ( GTK_CONTAINER ( pwPage ), pwHBox );

    pwFrame = gtk_frame_new ( _("Chequer play") );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pwFrame, FALSE, FALSE, 0 );

    gtk_container_add( GTK_CONTAINER( pwFrame ), ppw->apwEvalChequer[ i ] =
		       EvalWidget( &ppw->ap[ i ].esChequer.ec, 
                                   (movefilter *) ppw->ap[ i ].aamf, 
                                   NULL, TRUE ) );
    gtk_widget_set_sensitive( ppw->apwEvalChequer[ i ],
			      ap[ i ].pt == PLAYER_GNU );

    
    pwFrame = gtk_frame_new ( _("Cube decisions") );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pwFrame, FALSE, FALSE, 0 );

    pwvbox = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame ), pwvbox );
    
    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                         ppw->apwEvalCube[ i ] = 
                            EvalWidget( &ppw->ap[ i ].esCube.ec, 
                                        NULL, NULL, FALSE ),
                         FALSE, FALSE, 0 );
    gtk_widget_set_sensitive( ppw->apwEvalCube[ i ],
			      ap[ i ].pt == PLAYER_GNU );

    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 2 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   _("Pubeval") ) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 3 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   _("External") ) );
    ppw->apwExternal[ i ] = pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
    gtk_widget_set_sensitive( pw, ap[ i ].pt == PLAYER_EXTERNAL );
    gtk_container_add( GTK_CONTAINER( pwPage ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( _("Socket:") ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       ppw->apwSocket[ i ] = gtk_entry_new() );
    if( ap[ i ].szSocket )
	gtk_entry_set_text( GTK_ENTRY( ppw->apwSocket[ i ] ),
			    ap[ i ].szSocket );
    
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(
	ppw->apwRadio[ i ][ aiRadioButton[ ap[ i ].pt ] ] ), TRUE );

    gtk_signal_connect( GTK_OBJECT( ppw->apwRadio[ i ][ 1 ] ), "toggled",
			GTK_SIGNAL_FUNC( PlayerTypeToggled ), ppw );
    gtk_signal_connect( GTK_OBJECT( ppw->apwRadio[ i ][ 3 ] ), "toggled",
			GTK_SIGNAL_FUNC( PlayerTypeToggled ), ppw );
    
    return pwPage;
}

static void PlayersOK( GtkWidget *pw, playerswidget *pplw ) {

    int i,j ;
    static playertype apt[ 4 ] = { PLAYER_HUMAN, PLAYER_GNU, PLAYER_PUBEVAL,
				   PLAYER_EXTERNAL };
    *pplw->pfOK = TRUE;

    for( i = 0; i < 2; i++ ) {
	strcpyn( pplw->ap[ i ].szName, gtk_entry_get_text(
	    GTK_ENTRY( pplw->apwName[ i ] ) ), MAX_NAME_LEN );
	
	for( j = 0; j < 4; j++ )
	    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
		pplw->apwRadio[ i ][ j ] ) ) ) {
		pplw->ap[ i ].pt = apt[ j ];
		break;
	    }
	assert( j < 4 );

	EvalOK( pplw->apwEvalChequer[ i ], pplw->apwEvalChequer[ i ] );
	EvalOK( pplw->apwEvalCube[ i ], pplw->apwEvalCube[ i ] );

	strcpyn( pplw->aszSocket[ i ], gtk_entry_get_text(
	    GTK_ENTRY( pplw->apwSocket[ i ] ) ), 128 );
    }

    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void SetPlayers( gpointer *p, guint n, GtkWidget *pw ) {

    GtkWidget *pwDialog, *pwNotebook;
    int i, fOK = FALSE;
    player apTemp[ 2 ];
    playerswidget plw;
    char sz[ 256 ];
    
    memcpy( apTemp, ap, sizeof ap );
    plw.ap = apTemp;
    plw.pfOK = &fOK;
    
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Players"), DT_QUESTION,
			     GTK_SIGNAL_FUNC( PlayersOK ), &plw );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwNotebook = gtk_notebook_new() );
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PlayersPage( &plw, 0 ),
			      gtk_label_new( _("Player 0") ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PlayersPage( &plw, 1 ),
			      gtk_label_new( _("Player 1") ) );
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK ) {
	outputpostpone();

	if (!CompareNames(apTemp[0].szName, ap[1].szName) && 
		CompareNames(apTemp[0].szName, apTemp[1].szName))
	{	/* Trying to swap names - change current name to avoid error */
		sprintf(ap[1].szName, "_%s", apTemp[0].szName);
	}
	for( i = 0; i < 2; i++ ) {
	    /* NB: this comparison is case-sensitive, and does not use
	       CompareNames(), so that the user can modify the case of
	       names. */
	    if( strcmp( ap[ i ].szName, apTemp[ i ].szName ) ) {
		sprintf( sz, "set player %d name %s", i, apTemp[ i ].szName );
		UserCommand( sz );
	    }
	    
	    switch( apTemp[ i ].pt ) {
	    case PLAYER_HUMAN:
		if( ap[ i ].pt != PLAYER_HUMAN ) {
		    sprintf( sz, "set player %d human", i );
		    UserCommand( sz );
		}
		break;
		
	    case PLAYER_GNU:
		if( ap[ i ].pt != PLAYER_GNU ) {
		    sprintf( sz, "set player %d gnubg", i );
		    UserCommand( sz );
		}

		/* FIXME another temporary hack (should be some way to set
		   chequer and cube parameters independently) */
		sprintf( sz, "set player %d chequer evaluation", i );
		SetEvalCommands( sz, &apTemp[ i ].esChequer.ec,
				 &ap[ i ].esChequer.ec );
                sprintf ( sz, "set player %d movefilter", i );
                SetMovefilterCommands ( sz, apTemp[ i ].aamf, ap[ i ].aamf );
		sprintf( sz, "set player %d cube evaluation", i );
		SetEvalCommands( sz, &apTemp[ i ].esCube.ec,
				 &ap[ i ].esCube.ec );
		break;
		
	    case PLAYER_PUBEVAL:
		if( ap[ i ].pt != PLAYER_PUBEVAL ) {
		    sprintf( sz, "set player %d pubeval", i );
		    UserCommand( sz );
		}
		break;
		
	    case PLAYER_EXTERNAL:
		if( ap[ i ].pt != PLAYER_EXTERNAL ||
		    strcmp( ap[ i ].szSocket, plw.aszSocket[ i ] ) ) {
		    sprintf( sz, "set player %d external %s", i,
			     plw.aszSocket[ i ] );
		    UserCommand( sz );
		}
		break;
	    }
	}
    
	outputresume();
    }
}

typedef struct _analysiswidget {

  evalsetup esChequer;
  evalsetup esCube; 
  movefilter aamf[ MAX_FILTER_PLIES ][ MAX_FILTER_PLIES ];

  GtkAdjustment *padjMoves;
  GtkAdjustment *apadjSkill[5], *apadjLuck[4];
  GtkWidget *pwMoves, *pwCube, *pwLuck;
  GtkWidget *pwEvalCube, *pwEvalChequer;
  GtkWidget *apwAnalysePlayers[ 2 ];
    
} analysiswidget;

static void AnalysisCheckToggled( GtkWidget *pw, analysiswidget *paw ) {

  gint fMov = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( paw->pwMoves ) ); 
  gint fCub = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( paw->pwCube ) ); 
  /* FIXME: The Luck frame should be set inactive if Luck is checked off */  
  /* FIXME: The Skill frame should be set inactive if both Moves and Chequer
            are checked off */  

  /*   gint fDic = 
     gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( paw->pwLuck ) ); */

  gtk_widget_set_sensitive( paw->pwEvalCube, fCub );
  gtk_widget_set_sensitive( paw->pwEvalChequer, fMov );

} 

static GtkWidget *AnalysisPage( analysiswidget *paw ) {

  char *aszSkillLabel[5] = { N_("Very good:"), N_("Good:"),
	  N_("Doubtful:"), N_("Bad:"), N_("Very bad:") };
  char *aszLuckLabel[4] = { N_("Very lucky:"), N_("Lucky:"),
	  N_("Unlucky:"), N_("Very unlucky:") };
  int i;

  GtkWidget *pwPage, *pwFrame, *pwLabel, *pwSpin, *pwTable; 
  GtkWidget *hbox1, *vbox1, *vbox2, *hbox2;
  GtkWidget *pwvbox;

  pwPage = gtk_vbox_new ( FALSE, 0 );
  gtk_container_set_border_width( GTK_CONTAINER( pwPage ), 8 );
  
  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (pwPage), hbox1, TRUE, TRUE, 0);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox1, TRUE, TRUE, 0);

  pwFrame = gtk_frame_new (_("Analysis"));
  gtk_box_pack_start (GTK_BOX (vbox1), pwFrame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pwFrame), 4);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (pwFrame), vbox2);

  paw->pwMoves = gtk_check_button_new_with_label (_("Chequer play"));
  gtk_box_pack_start (GTK_BOX (vbox2), paw->pwMoves, FALSE, FALSE, 0);

  paw->pwCube = gtk_check_button_new_with_label (_("Cube decisions"));
  gtk_box_pack_start (GTK_BOX (vbox2), paw->pwCube, FALSE, FALSE, 0);

  paw->pwLuck = gtk_check_button_new_with_label (_("Luck"));
  gtk_box_pack_start (GTK_BOX (vbox2), paw->pwLuck, FALSE, FALSE, 0);

  for ( i = 0; i < 2; ++i ) {

    char *sz = g_strdup_printf( _("Analyse player %s"), ap[ i ].szName );

    paw->apwAnalysePlayers[ i ] = gtk_check_button_new_with_label ( sz );
    gtk_box_pack_start (GTK_BOX (vbox2), paw->apwAnalysePlayers[ i ], 
                        FALSE, FALSE, 0);

  }

  gtk_signal_connect ( GTK_OBJECT ( paw->pwMoves ), "toggled",
                       GTK_SIGNAL_FUNC ( AnalysisCheckToggled ), paw );
  gtk_signal_connect ( GTK_OBJECT ( paw->pwCube ), "toggled",
                       GTK_SIGNAL_FUNC ( AnalysisCheckToggled ), paw );
  gtk_signal_connect ( GTK_OBJECT ( paw->pwLuck ), "toggled",
                       GTK_SIGNAL_FUNC ( AnalysisCheckToggled ), paw );

  hbox2 = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), hbox2, TRUE, TRUE, 0);

  pwLabel = gtk_label_new (_("Move limit:"));
  gtk_box_pack_start (GTK_BOX (hbox2), pwLabel, FALSE, FALSE, 0);

  paw->padjMoves =  
    GTK_ADJUSTMENT( gtk_adjustment_new( 0, 0, 1000, 1, 1, 0 ) );

  pwSpin = gtk_spin_button_new (GTK_ADJUSTMENT (paw->padjMoves), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox2), pwSpin, TRUE, TRUE, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pwSpin), TRUE);

  pwFrame = gtk_frame_new (_("Skill thresholds"));
  gtk_box_pack_start (GTK_BOX (vbox1), pwFrame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pwFrame), 4);

  pwTable = gtk_table_new (5, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (pwFrame), pwTable);

  for (i = 0; i < 5; i++){
    pwLabel = gtk_label_new ( gettext ( aszSkillLabel[i] ) );
    gtk_table_attach (GTK_TABLE (pwTable), pwLabel, 0, 1, i, i+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (pwLabel), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment (GTK_MISC (pwLabel), 0, 0.5);
  }
  
  for (i = 0; i < 5; i++){
    paw->apadjSkill[i] = 
	  GTK_ADJUSTMENT( gtk_adjustment_new( 1, 0, 1, 0.01, 10, 10 ) );

    pwSpin = 
	    gtk_spin_button_new (GTK_ADJUSTMENT (paw->apadjSkill[i]), 1, 2);
    /* Disable good and very good thresholds, as not used at the moment */
    if (i == 0 || i == 1)
      gtk_widget_set_sensitive(pwSpin, FALSE);
    
    gtk_table_attach (GTK_TABLE (pwTable), pwSpin, 1, 2, i, i+1,
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pwSpin), TRUE);
  }
  
  pwFrame = gtk_frame_new (_("Luck thresholds"));
  gtk_box_pack_start (GTK_BOX (vbox1), pwFrame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pwFrame), 4);

  pwTable = gtk_table_new (4, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (pwFrame), pwTable);
 
  for (i = 0; i < 4; i++){
    pwLabel = gtk_label_new ( gettext ( aszLuckLabel[i] ) );
    gtk_table_attach (GTK_TABLE (pwTable), pwLabel, 0, 1, i, i+1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (pwLabel), GTK_JUSTIFY_RIGHT);
    gtk_misc_set_alignment (GTK_MISC (pwLabel), 0, 0.5);
  }
  
  for (i = 0; i < 4; i++){
    paw->apadjLuck[i] = 
	  GTK_ADJUSTMENT( gtk_adjustment_new( 1, 0, 1, 0.01, 10, 10 ) );

    pwSpin = 
	    gtk_spin_button_new (GTK_ADJUSTMENT (paw->apadjLuck[i]), 1, 2);
    
    gtk_table_attach (GTK_TABLE (pwTable), pwSpin, 1, 2, i, i+1,
                      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                      (GtkAttachOptions) (0), 0, 0);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (pwSpin), TRUE);
  }
  
  pwFrame = gtk_frame_new (_("Chequer play"));
  gtk_box_pack_start (GTK_BOX (hbox1), pwFrame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pwFrame), 4);

  gtk_container_add(GTK_CONTAINER (pwFrame ), 
		  paw->pwEvalChequer = EvalWidget( &paw->esChequer.ec, 
                                                   (movefilter *) paw->aamf,
                                                   NULL, TRUE ));

  pwFrame = gtk_frame_new (_("Cube decisions"));
  gtk_box_pack_start (GTK_BOX (hbox1), pwFrame, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (pwFrame), 4);
  
  pwvbox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwFrame ), pwvbox );
    
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       paw->pwEvalCube = EvalWidget( &paw->esCube.ec, NULL,
                                                     NULL, FALSE ),
                       FALSE, FALSE, 0 );

  return pwPage;
}

#define CHECKUPDATE(button,flag,string) \
   n = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( (button) ) ); \
   if ( n != (flag)){ \
           sprintf(sz, (string), n ? "on" : "off"); \
           UserCommand(sz); \
   }

#define ADJUSTSKILLUPDATE(button,flag,string) \
  if( paw->apadjSkill[(button)]->value != arSkillLevel[(flag)] ) \
  { \
     gchar buf[G_ASCII_DTOSTR_BUF_SIZE]; \
     sprintf(sz,  (string), g_ascii_formatd( buf, G_ASCII_DTOSTR_BUF_SIZE, \
			           "%0.3f", (gdouble) paw->apadjSkill[(button)]->value )); \
     UserCommand(sz); \
  }

#define ADJUSTLUCKUPDATE(button,flag,string) \
  if( paw->apadjLuck[(button)]->value != arLuckLevel[(flag)] ) \
  { \
     gchar buf[G_ASCII_DTOSTR_BUF_SIZE]; \
     sprintf(sz,  (string), g_ascii_formatd( buf, G_ASCII_DTOSTR_BUF_SIZE, \
			           "%0.3f", (gdouble) paw->apadjLuck[(button)]->value )); \
     UserCommand(sz); \
  }
     
static void AnalysisOK( GtkWidget *pw, analysiswidget *paw ) {

  char sz[128]; 
  int n;

  gtk_widget_hide( gtk_widget_get_toplevel( pw ) );

  CHECKUPDATE(paw->pwMoves,fAnalyseMove, "set analysis moves %s")
  CHECKUPDATE(paw->pwCube,fAnalyseCube, "set analysis cube %s")
  CHECKUPDATE(paw->pwLuck,fAnalyseDice, "set analysis luck %s")
  CHECKUPDATE(paw->apwAnalysePlayers[ 0 ], afAnalysePlayers[ 0 ],
              "set analysis player 0 analyse %s")
  CHECKUPDATE(paw->apwAnalysePlayers[ 1 ], afAnalysePlayers[ 1 ],
              "set analysis player 1 analyse %s")

  if((n = paw->padjMoves->value) != cAnalysisMoves) {
    sprintf(sz, "set analysis limit %d", n );
    UserCommand(sz); 
  }

/*   ADJUSTSKILLUPDATE( 0, SKILL_VERYGOOD, "set analysis threshold verygood %.3f" ) */
/*   ADJUSTSKILLUPDATE( 1, SKILL_GOOD, "set analysis threshold good %.3f" ) */
  ADJUSTSKILLUPDATE( 2, SKILL_DOUBTFUL, "set analysis threshold doubtful %s" )
  ADJUSTSKILLUPDATE( 3, SKILL_BAD, "set analysis threshold bad %s" )
  ADJUSTSKILLUPDATE( 4, SKILL_VERYBAD, "set analysis threshold verybad %s" )

  ADJUSTLUCKUPDATE( 0, LUCK_VERYGOOD, "set analysis threshold verylucky %s" )
  ADJUSTLUCKUPDATE( 1, LUCK_GOOD, "set analysis threshold lucky %s" )
  ADJUSTLUCKUPDATE( 2, LUCK_BAD, "set analysis threshold unlucky %s" )
  ADJUSTLUCKUPDATE( 3, LUCK_VERYBAD, "set analysis threshold veryunlucky %s" )

  EvalOK( paw->pwEvalChequer, paw->pwEvalChequer );
  EvalOK( paw->pwEvalCube, paw->pwEvalCube );
  
  SetEvalCommands( "set analysis chequerplay eval", &paw->esChequer.ec,
		  &esAnalysisChequer.ec );
  SetMovefilterCommands ( "set analysis movefilter", paw->aamf, aamfAnalysis );
  SetEvalCommands( "set analysis cubedecision eval", &paw->esCube.ec,
		  &esAnalysisCube.ec );

  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}
#undef CHECKUPDATE
#undef ADJUSTSKILLUPDATE
#undef ADJUSTLUCKUPDATE

static void AnalysisSet( analysiswidget *paw) {

  int i;

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( paw->pwMoves ),
                                fAnalyseMove );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( paw->pwCube ),
                                fAnalyseCube );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( paw->pwLuck ),
                                fAnalyseDice );

  for ( i = 0; i < 2; ++i )
    gtk_toggle_button_set_active( 
        GTK_TOGGLE_BUTTON( paw->apwAnalysePlayers[ i ] ), 
        afAnalysePlayers[ i ] );

  gtk_adjustment_set_value ( paw->padjMoves, cAnalysisMoves );
  
/*   gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjSkill[0] ),  */
/* 		 arSkillLevel[SKILL_VERYGOOD] ); */
/*   gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjSkill[1] ), */
/* 		 arSkillLevel[SKILL_GOOD] ); */
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjSkill[2] ),
		 arSkillLevel[SKILL_DOUBTFUL] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjSkill[3] ),
		 arSkillLevel[SKILL_BAD] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjSkill[4] ),
		 arSkillLevel[SKILL_VERYBAD] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjLuck[0] ),
		 arLuckLevel[LUCK_VERYGOOD] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjLuck[1] ),
		 arLuckLevel[LUCK_GOOD] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjLuck[2] ),
		 arLuckLevel[LUCK_BAD] );
  gtk_adjustment_set_value ( GTK_ADJUSTMENT( paw->apadjLuck[3] ),
		 arLuckLevel[LUCK_VERYBAD] );
}  

static void SetAnalysis( gpointer *p, guint n, GtkWidget *pw ) {

  GtkWidget *pwDialog;
  analysiswidget aw;

  memcpy( &aw.esCube, &esAnalysisCube, sizeof( aw.esCube ) );
  memcpy( &aw.esChequer, &esAnalysisChequer, sizeof( aw.esChequer ) );
  memcpy( &aw.aamf, aamfAnalysis, sizeof( aw.aamf ) );

  pwDialog = GTKCreateDialog( _("GNU Backgammon - Analysis Settings"),
			   DT_QUESTION, GTK_SIGNAL_FUNC( AnalysisOK ), &aw );
  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
 		         AnalysisPage( &aw ) );
  gtk_widget_show_all( pwDialog );

  AnalysisSet ( &aw );
 
  gtk_main();

}


typedef struct _rolloutpagewidget {
  int *pfOK;
  GtkWidget *arpwEvCube, *arpwEvCheq;
  evalcontext *precCube, *precCheq;
  movefilter *pmf;
} rolloutpagewidget;

typedef struct _rolloutpagegeneral {
  int *pfOK;
  GtkWidget *pwCubeful, *pwVarRedn, *pwInitial, *pwRotate, *pwDoLate;
  GtkWidget *pwDoTrunc, *pwCubeEqualChequer, *pwPlayersAreSame;
  GtkWidget *pwTruncEqualPlayer0;
  GtkWidget *pwTruncBearoff2, *pwTruncBearoffOS, *pwTruncBearoffOpts;
  GtkWidget *pwAdjLatePlies, *pwAdjTruncPlies, *pwAdjMinGames;
  GtkWidget *pwDoSTDStop, *pwAdjMaxError;
  GtkWidget *pwJsdDoStop, *pwJsdDoMoveStop;
  GtkWidget *pwJsdMinGames, *pwJsdAdjMinGames, *pwJsdAdjLimit;
  GtkAdjustment *padjTrials, *padjTruncPlies, *padjLatePlies;
  GtkAdjustment *padjSeed, *padjMinGames, *padjMaxError;
  GtkAdjustment *padjJsdMinGames, *padjJsdLimit;
  GtkWidget *arpwGeneral;
  GtkWidget *pwMinGames, *pwMaxError;
} rolloutpagegeneral;

typedef struct _rolloutwidget {
  rolloutcontext  rcRollout;
  rolloutpagegeneral  *prwGeneral;
  rolloutpagewidget *prpwPages[4], *prpwTrunc;
  GtkWidget *RolloutNotebook;    
  int  fCubeEqualChequer, fPlayersAreSame, fTruncEqualPlayer0;
  int *pfOK;
} rolloutwidget;
/*
int fCubeEqualChequer = 1;
int fPlayersAreSame = 1;
int fTruncEqualPlayer0 = 1;
*/ 

/***************************************************************************
 *****
 *****  Change SGF_ROLLOUT_VER in eval.h if rollout settings change 
 *****  such that previous .sgf files won't be able to extend rollouts
 *****
 ***************************************************************************/
static void SetRolloutsOK( GtkWidget *pw, rolloutwidget *prw ) {
  int   p0, p1, i;
  int fCubeEqChequer, fPlayersAreSame, nTruncPlies;
  *prw->pfOK = TRUE;

  prw->rcRollout.nTrials = prw->prwGeneral->padjTrials->value;
  nTruncPlies = prw->rcRollout.nTruncate = 
    prw->prwGeneral->padjTruncPlies->value;

  prw->rcRollout.nSeed = prw->prwGeneral->padjSeed->value;

  prw->rcRollout.fCubeful = gtk_toggle_button_get_active(
                                                         GTK_TOGGLE_BUTTON( prw->prwGeneral->pwCubeful ) );

  prw->rcRollout.fVarRedn = gtk_toggle_button_get_active(
                                                         GTK_TOGGLE_BUTTON( prw->prwGeneral->pwVarRedn ) );

  prw->rcRollout.fRotate = gtk_toggle_button_get_active(
                                                        GTK_TOGGLE_BUTTON( prw->prwGeneral->pwRotate ) );

  prw->rcRollout.fInitial = gtk_toggle_button_get_active(
                                                         GTK_TOGGLE_BUTTON( prw->prwGeneral->pwInitial ) );

  prw->rcRollout.fDoTruncate = gtk_toggle_button_get_active(
                                                            GTK_TOGGLE_BUTTON( prw->prwGeneral->pwDoTrunc ) );

  prw->rcRollout.fTruncBearoff2 = gtk_toggle_button_get_active(
                                                               GTK_TOGGLE_BUTTON( prw->prwGeneral->pwTruncBearoff2 ) );

  prw->rcRollout.fTruncBearoffOS = gtk_toggle_button_get_active(
                                                                GTK_TOGGLE_BUTTON( prw->prwGeneral->pwTruncBearoffOS ) );

  if (nTruncPlies == 0) 
    prw->rcRollout.fDoTruncate = FALSE;

  prw->rcRollout.fLateEvals = gtk_toggle_button_get_active(
                                                           GTK_TOGGLE_BUTTON( prw->prwGeneral->pwDoLate ) );

  prw->rcRollout.nLate = prw->prwGeneral->padjLatePlies->value;

  fCubeEqChequer = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                    prw->prwGeneral->pwCubeEqualChequer ) );

  fPlayersAreSame = 
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( 
                                                    prw->prwGeneral->pwPlayersAreSame ) );

  prw->rcRollout.fStopOnSTD = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(
                                            prw->prwGeneral->pwDoSTDStop));
  prw->rcRollout.nMinimumGames = (unsigned int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (prw->prwGeneral->pwMinGames));
  prw->rcRollout.rStdLimit = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (prw->prwGeneral->pwMaxError));

  prw->rcRollout.fStopOnJsd = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(prw->prwGeneral->pwJsdDoStop));
  prw->rcRollout.fStopMoveOnJsd = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(prw->prwGeneral->pwJsdDoMoveStop));
  prw->rcRollout.nMinimumJsdGames = (unsigned int) gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (prw->prwGeneral->pwJsdMinGames));
  prw->rcRollout.rJsdLimit = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (prw->prwGeneral->pwJsdAdjLimit));

  /* get all the evaluations out of the widgets */
  for (i = 0; i < 4; ++i) {
    EvalOK(prw->prpwPages[i]->arpwEvCube, prw->prpwPages[i]->arpwEvCube);
    EvalOK(prw->prpwPages[i]->arpwEvCheq, prw->prpwPages[i]->arpwEvCheq);
  }


  EvalOK(prw->prpwTrunc->arpwEvCube, prw->prpwTrunc->arpwEvCube);
  EvalOK(prw->prpwTrunc->arpwEvCheq, prw->prpwTrunc->arpwEvCheq);

  /* if the players are the same, copy player 0 settings to player 1 */
  if (fPlayersAreSame) {
    for (p0 = 0, p1 = 1; p0 < 4; p0 += 2, p1 += 2) {
      memcpy (prw->prpwPages[p1]->precCheq, prw->prpwPages[p0]->precCheq, 
              sizeof (evalcontext));
      memcpy (prw->prpwPages[p1]->precCube, prw->prpwPages[p0]->precCube, 
              sizeof (evalcontext));

      memcpy (prw->prpwPages[p1]->pmf, prw->prpwPages[p0]->pmf,
              MAX_FILTER_PLIES * MAX_FILTER_PLIES * sizeof ( movefilter ) );
    }
  }

  /* if cube is same as chequer, copy the chequer settings to the
     cube settings */

  if (fCubeEqChequer) {
    for (i = 0; i < 4; ++i) {
      memcpy (prw->prpwPages[i]->precCube, prw->prpwPages[i]->precCheq,
              sizeof (evalcontext));
    }
	  
    memcpy (prw->prpwTrunc->precCube, prw->prpwTrunc->precCheq,
            sizeof (evalcontext));
  }

  if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON ( 
													   prw->prwGeneral->pwTruncEqualPlayer0))) {
	memcpy (prw->prpwTrunc->precCube, prw->prpwPages[0]->precCheq,
			sizeof (evalcontext));
	memcpy (prw->prpwTrunc->precCheq, prw->prpwPages[0]->precCube,
			sizeof (evalcontext));
  }

  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}

/* create one page for rollout settings  for playes & truncation */

static GtkWidget *RolloutPage( rolloutpagewidget *prpw, 
                               const int fMoveFilter ) {

  GtkWidget *pwPage;
  GtkWidget *pwHBox;
  GtkWidget *pwFrame;
  GtkWidget *pwvbox;

  pwPage = gtk_vbox_new( FALSE, 0 );
  gtk_container_set_border_width( GTK_CONTAINER( pwPage ), 8 );
    
  pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwPage ), pwHBox );
    
  pwFrame = gtk_frame_new ( _("Chequer play") );
  gtk_box_pack_start ( GTK_BOX ( pwHBox ), pwFrame, FALSE, FALSE, 0 );

  pwvbox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwFrame ), pwvbox );
    
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       prpw->arpwEvCheq =
                       EvalWidget( prpw->precCheq, 
                                   prpw->pmf, NULL, fMoveFilter ),
                       FALSE, FALSE, 0 );

  pwFrame = gtk_frame_new ( _("Cube decisions") );
  gtk_box_pack_start ( GTK_BOX ( pwHBox ), pwFrame, FALSE, FALSE, 0 );

  pwvbox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwFrame ), pwvbox );
    
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                       prpw->arpwEvCube =
                       EvalWidget( prpw->precCube, NULL, NULL, FALSE ),
                       FALSE, FALSE, 0 );

  return pwPage;
}

typedef enum _rolloutpages { 
  ROLL_GENERAL = 0, ROLL_P0, ROLL_P1, ROLL_P0_LATE, ROLL_P1_LATE, ROLL_TRUNC
} rolloutpages;

static void LateEvalToggled( GtkWidget *pw, rolloutwidget *prw) {

  int do_late = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                 prw->prwGeneral->pwDoLate ) );
  int   are_same = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                                     prw->prwGeneral->pwPlayersAreSame ) );

  /* turn on/off the late pages */
  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK 
                            (prw->RolloutNotebook), ROLL_P0_LATE), do_late);
  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK 
                             (prw->RolloutNotebook), ROLL_P1_LATE), 
                             do_late && !are_same);

  /* turn on/off the ply setting in the general page */
  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwAdjLatePlies),
                            do_late);
}

static void STDStopToggled( GtkWidget *pw, rolloutwidget *prw) {

  int do_std_stop = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                       prw->prwGeneral->pwDoSTDStop ) );

  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwAdjMinGames),
                            do_std_stop);
  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwAdjMaxError),
                            do_std_stop);
}

static void JsdStopToggled( GtkWidget *pw, rolloutwidget *prw) {

  int do_jsd_stop = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (prw->prwGeneral->pwJsdDoStop ) );
  int do_jsd_move_stop = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (prw->prwGeneral->pwJsdDoMoveStop) );
  int enable = do_jsd_stop | do_jsd_move_stop;
  
  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwJsdAdjLimit), enable);

  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwJsdAdjMinGames), enable);

}

static void TruncEnableToggled( GtkWidget *pw, rolloutwidget *prw) {
 
  int do_trunc = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                                   prw->prwGeneral->pwDoTrunc ) );

  int sameas_p0 = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
			prw->prwGeneral->pwTruncEqualPlayer0));

  /* turn on/off the truncation page */
  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK
                                                        (prw->RolloutNotebook), ROLL_TRUNC), do_trunc && !sameas_p0);

  /* turn on/off the truncation ply setting */
  gtk_widget_set_sensitive (GTK_WIDGET (prw->prwGeneral->pwAdjTruncPlies ),
                            do_trunc);

}

static void TruncEqualPlayer0Toggled( GtkWidget *pw, rolloutwidget *prw) {

  int do_trunc = 
	gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
									 prw->prwGeneral->pwDoTrunc ) );
  int sameas_p0 = 
	gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
									prw->prwGeneral->pwTruncEqualPlayer0));

  prw->fTruncEqualPlayer0 = sameas_p0;
  /* turn on/off the truncation page */
  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK
               (prw->RolloutNotebook), ROLL_TRUNC), do_trunc && !sameas_p0);
}
  

static void CubeEqCheqToggled( GtkWidget *pw, rolloutwidget *prw) {

  int  i;
  int are_same = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                                   prw->prwGeneral->pwCubeEqualChequer ) );

  prw->fCubeEqualChequer = are_same;

  /* turn the cube evals on/off in the rollout pages */
  for (i = 0; i < 4; ++i) {
    gtk_widget_set_sensitive (prw->prpwPages[i]->arpwEvCube, !are_same);
  }

  gtk_widget_set_sensitive (prw->prpwTrunc->arpwEvCube, !are_same);
}

static void
CubefulToggled ( GtkWidget *pw, rolloutwidget *prw ) {

  int f = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON ( 
                                                            prw->prwGeneral->pwCubeful ) );
  
  gtk_widget_set_sensitive ( GTK_WIDGET ( 
                                         prw->prwGeneral->pwTruncBearoffOS ), ! f );

}

static void PlayersSameToggled( GtkWidget *pw, rolloutwidget *prw) {

  int   are_same = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                                     prw->prwGeneral->pwPlayersAreSame ) );
  int do_late = gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (
                                                                  prw->prwGeneral->pwDoLate ) );

  prw->fPlayersAreSame = are_same;
  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK
                                                        (prw->RolloutNotebook), ROLL_P1), !are_same);

  gtk_widget_set_sensitive ( gtk_notebook_get_nth_page (GTK_NOTEBOOK
                                                        (prw->RolloutNotebook), ROLL_P1_LATE), !are_same && do_late);

  gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (prw->RolloutNotebook),
                                   gtk_notebook_get_nth_page (GTK_NOTEBOOK (prw->RolloutNotebook),
                                                              ROLL_P0), are_same ? _("First Play Both") : _("First Play (0) ") );

  gtk_notebook_set_tab_label_text (GTK_NOTEBOOK (prw->RolloutNotebook),
                                   gtk_notebook_get_nth_page (GTK_NOTEBOOK (prw->RolloutNotebook),
                                                              ROLL_P0_LATE), are_same ? _("Later Play Both") : _("Later Play (0) ") );

}  

/* create the General page for rollouts */

static GtkWidget *
RolloutPageGeneral (rolloutpagegeneral *prpw, rolloutwidget *prw) {
  GtkWidget *pwPage, *pw, *pwv;
  GtkWidget *pwHBox, *pwVBox;
  GtkWidget *pwFrame;

  pwPage = gtk_vbox_new( FALSE, 0 );
  gtk_container_set_border_width( GTK_CONTAINER( pwPage ), 8 );

  prpw->padjTrials = GTK_ADJUSTMENT(gtk_adjustment_new(
                                                       prw->rcRollout.nTrials, 1,
                                                       1296 * 1296, 36, 36, 0 ) );
  pw = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwPage), pw );
  gtk_container_add( GTK_CONTAINER( pw ),
                     gtk_label_new( _("Trials:") ) );
  gtk_container_add( GTK_CONTAINER( pw ),
                     gtk_spin_button_new( prpw->padjTrials, 36, 0 ) );

  pwFrame = gtk_frame_new ( _("Truncation") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), pwFrame );

  pw = gtk_hbox_new( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw);
   
  prpw->pwDoTrunc = gtk_check_button_new_with_label (
                                                     _( "Truncate Rollouts" ) );
  gtk_container_add( GTK_CONTAINER( pw ), prpw->pwDoTrunc );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( prpw->pwDoTrunc ),
                                 prw->rcRollout.fDoTruncate);
  gtk_signal_connect( GTK_OBJECT( prpw->pwDoTrunc ), "toggled",
                      GTK_SIGNAL_FUNC (TruncEnableToggled), prw);

  prpw->pwAdjTruncPlies = pwVBox = gtk_vbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pw ), pwVBox);
  gtk_container_add( GTK_CONTAINER( pwVBox ), 
                     gtk_label_new( _("Truncate at ply:" ) ) );

  prpw->padjTruncPlies = GTK_ADJUSTMENT( gtk_adjustment_new( 
                                                            prw->rcRollout.nTruncate, 0, 1000, 1, 1, 0 ) );
  gtk_container_add( GTK_CONTAINER( pwVBox ), gtk_spin_button_new( 
                                                                  prpw->padjTruncPlies, 1, 0 ) );


  pwFrame = gtk_frame_new ( _("Evaluation for later plies") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), pwFrame );

  pw = gtk_hbox_new( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw);
   
  prpw->pwDoLate = gtk_check_button_new_with_label (
                                        _( "Enable separate evaluations " ) );
  gtk_container_add( GTK_CONTAINER( pw ), prpw->pwDoLate );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( 
                                      prw->prwGeneral->pwDoLate ), 
                                 prw->rcRollout.fLateEvals);
  gtk_signal_connect( GTK_OBJECT( prw->prwGeneral->pwDoLate ), 
                      "toggled", GTK_SIGNAL_FUNC (LateEvalToggled), prw);

  prpw->pwAdjLatePlies = pwVBox = gtk_vbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pw ), pwVBox);
  gtk_container_add( GTK_CONTAINER( pwVBox ), 
                     gtk_label_new( _("Change eval after ply:" ) ) );

  prpw->padjLatePlies = GTK_ADJUSTMENT( gtk_adjustment_new( 
                                       prw->rcRollout.nLate, 0, 1000, 1, 1, 0 ) );
  gtk_container_add( GTK_CONTAINER( pwVBox ), gtk_spin_button_new( 
                                                prpw->padjLatePlies, 1, 0 ) );

  pwFrame = gtk_frame_new ( _("Stop when result is accurate") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), pwFrame );

  /* an hbox for the pane */
  pw = gtk_hbox_new( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw);
   
  prpw->pwDoSTDStop = gtk_check_button_new_with_label (
                               _( "Stop when STDs are small enough " ) );
  gtk_container_add( GTK_CONTAINER( pw ), prpw->pwDoSTDStop );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( 
                                      prw->prwGeneral->pwDoSTDStop ), 
                                 prw->rcRollout.fStopOnSTD);
  gtk_signal_connect( GTK_OBJECT( prw->prwGeneral->pwDoSTDStop ), 
                      "toggled", GTK_SIGNAL_FUNC (STDStopToggled), prw);

  /* a vbox for the adjusters */
  pwv = gtk_vbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pw ), pwv);

  
  prpw->pwAdjMinGames = pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwv ), pwHBox);
  gtk_container_add( GTK_CONTAINER( pwHBox ), 
                     gtk_label_new( _("Minimum Trials:" ) ) );

  prpw->padjMinGames = GTK_ADJUSTMENT( gtk_adjustment_new( 
                                       prw->rcRollout.nMinimumGames, 1, 1296 * 1296, 36, 36, 0 ) );

  prpw->pwMinGames = gtk_spin_button_new(prpw->padjMinGames, 1, 0 ) ;

  gtk_container_add( GTK_CONTAINER( pwHBox ), prpw->pwMinGames);

  prpw->pwAdjMaxError = pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwv ), pwHBox);
  gtk_container_add( GTK_CONTAINER( pwHBox ), 
                   gtk_label_new( _("Ratio |Standard Deviation/Value|:" ) ) );

  prpw->padjMaxError = GTK_ADJUSTMENT( gtk_adjustment_new( 
                       prw->rcRollout.rStdLimit, 0, 1, .0001, .0001, 0.001 ) );

  prpw->pwMaxError = gtk_spin_button_new(prpw->padjMaxError, .0001, 4 );

  gtk_container_add( GTK_CONTAINER( pwHBox ), prpw->pwMaxError);


  pwFrame = gtk_frame_new ( _("Stop Rollouts of multiple moves based on j.s.d.") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), pwFrame );

  /* an hbox for the frame */
  pw = gtk_hbox_new( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw);
  
  /* a vbox for the check boxes */
  pwv = gtk_vbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pw ), pwv);

  prpw->pwJsdDoStop = gtk_check_button_new_with_label (_( "Stop rollout when one move appears to be best " ) );
  gtk_container_add( GTK_CONTAINER( pwv ), prpw->pwJsdDoStop );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( prw->prwGeneral->pwJsdDoStop ), prw->rcRollout.fStopOnJsd);
  gtk_signal_connect( GTK_OBJECT( prw->prwGeneral->pwJsdDoStop ), "toggled", GTK_SIGNAL_FUNC (JsdStopToggled), prw);

  prpw->pwJsdDoMoveStop = gtk_check_button_new_with_label (_( "Stop rollout of move when best move j.s.d. appears better " ) );
  gtk_container_add( GTK_CONTAINER( pwv ), prpw->pwJsdDoMoveStop );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( prw->prwGeneral->pwJsdDoMoveStop ), prw->rcRollout.fStopMoveOnJsd);
  gtk_signal_connect( GTK_OBJECT( prw->prwGeneral->pwJsdDoMoveStop ), "toggled", GTK_SIGNAL_FUNC (JsdStopToggled), prw);

  /* a vbox for the adjusters */
  pwv = gtk_vbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pw ), pwv);

  prpw->pwJsdAdjMinGames = pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwv ), pwHBox);
  gtk_container_add( GTK_CONTAINER( pwHBox ), gtk_label_new( _("Minimum Trials:" ) ) );

  prpw->padjJsdMinGames = GTK_ADJUSTMENT( gtk_adjustment_new( prw->rcRollout.nMinimumJsdGames, 1, 1296 * 1296, 36, 36, 0 ) );

  prpw->pwJsdMinGames = gtk_spin_button_new(prpw->padjJsdMinGames, 1, 0 ) ;

  gtk_container_add( GTK_CONTAINER( pwHBox ), prpw->pwJsdMinGames);

  prpw->pwJsdAdjLimit = pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwv ), pwHBox);
  gtk_container_add( GTK_CONTAINER( pwHBox ), 
                   gtk_label_new( _("No of j.s.d.s from best move" ) ) );

  prpw->padjJsdLimit = GTK_ADJUSTMENT( gtk_adjustment_new( 
                       prw->rcRollout.rJsdLimit, 0, 8, .0001, .0001, 0.001 ) );

  prpw->pwJsdAdjLimit = gtk_spin_button_new(prpw->padjJsdLimit, .0001, 4 );

  gtk_container_add( GTK_CONTAINER( pwHBox ), prpw->pwJsdAdjLimit);


  prpw->pwCubeful = gtk_check_button_new_with_label ( _("Cubeful") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwCubeful );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwCubeful ),
                                prw->rcRollout.fCubeful );

  gtk_signal_connect( GTK_OBJECT( prpw->pwCubeful ), "toggled",
                      GTK_SIGNAL_FUNC( CubefulToggled ), prw );

  pwFrame = gtk_frame_new ( _("Bearoff Truncation") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), pwFrame );
  prpw->pwTruncBearoffOpts = pw = gtk_vbox_new( FALSE, 8 );
  gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
  gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw);

  prpw->pwTruncBearoff2 = gtk_check_button_new_with_label (
                                                           _( "Truncate cubeless (and cubeful money) at exact bearoff database" ) );

  gtk_container_add( GTK_CONTAINER( pw ), prpw->pwTruncBearoff2 );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON ( prpw->pwTruncBearoff2 ),
                                 prw->rcRollout.fTruncBearoff2 );

  prpw->pwTruncBearoffOS = gtk_check_button_new_with_label (
                                                            _( "Truncate cubeless at one-sided bearoff database" ) );

  gtk_container_add( GTK_CONTAINER( pw ), prpw->pwTruncBearoffOS );
  gtk_toggle_button_set_active ( GTK_TOGGLE_BUTTON 
                                 (prpw->pwTruncBearoffOS ),
                                 prw->rcRollout.fTruncBearoffOS );

  prpw->pwVarRedn = gtk_check_button_new_with_label ( 
                                                     _("Variance reduction") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwVarRedn );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwVarRedn ),
                                prw->rcRollout.fVarRedn );

  prpw->pwRotate = gtk_check_button_new_with_label ( 
                                                    _("Use quasi-random dice") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwRotate );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwRotate ),
                                prw->rcRollout.fRotate );

  prpw->pwInitial = gtk_check_button_new_with_label ( 
                                                     _("Rollout as initial position") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwInitial );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwInitial ),
                                prw->rcRollout.fInitial );

  pwHBox = gtk_hbox_new( FALSE, 0 );
  gtk_container_add( GTK_CONTAINER( pwPage ), pwHBox);
  gtk_container_add( GTK_CONTAINER( pwHBox ),
                     gtk_label_new( _("Seed:") ) );
  prpw->padjSeed = GTK_ADJUSTMENT( gtk_adjustment_new(
                                                      abs( prw->rcRollout.nSeed ), 0, INT_MAX, 1, 1, 0 ) );
  gtk_container_add( GTK_CONTAINER( pwHBox ), gtk_spin_button_new(
                                                                  prpw->padjSeed, 1, 0 ) );
 
  prpw->pwCubeEqualChequer = gtk_check_button_new_with_label (
                                                              _("Cube decisions use same settings as Chequer play") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwCubeEqualChequer );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwCubeEqualChequer),
                                prw->fCubeEqualChequer);
  gtk_signal_connect( GTK_OBJECT( prpw->pwCubeEqualChequer ), "toggled",
                      GTK_SIGNAL_FUNC ( CubeEqCheqToggled ), prw);

  prpw->pwPlayersAreSame = gtk_check_button_new_with_label (
                                                            _("Use same settings for both players") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwPlayersAreSame );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwPlayersAreSame),
                                prw->fPlayersAreSame);
  gtk_signal_connect( GTK_OBJECT( prpw->pwPlayersAreSame ), "toggled",
                      GTK_SIGNAL_FUNC ( PlayersSameToggled ), prw);

  prpw->pwTruncEqualPlayer0 = gtk_check_button_new_with_label (
            _("Use player0 setting for truncation point") );
  gtk_container_add ( GTK_CONTAINER (pwPage ), prpw->pwTruncEqualPlayer0 );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( prpw->pwTruncEqualPlayer0),
                                prw->fTruncEqualPlayer0);
  gtk_signal_connect( GTK_OBJECT( prpw->pwTruncEqualPlayer0 ), "toggled",
                      GTK_SIGNAL_FUNC ( TruncEqualPlayer0Toggled), prw);

  return pwPage;
}

extern void SetRollouts( gpointer *p, guint n, GtkWidget *pwIgnore ) {

  GtkWidget *pwDialog;
  int fOK = FALSE;
  rolloutwidget rw;
  rolloutpagegeneral RPGeneral;
  rolloutpagewidget  RPPlayer0, RPPlayer1, RPPlayer0Late, RPPlayer1Late,
    RPTrunc;
  char sz[ 256 ];
  int  i;
  const float epsilon = 1.0e-6;

  memcpy (&rw.rcRollout, &rcRollout, sizeof (rcRollout));
  rw.prwGeneral = &RPGeneral;
  rw.prpwPages[0] = &RPPlayer0;
  rw.prpwPages[1] = &RPPlayer1;
  rw.prpwPages[2] = &RPPlayer0Late;
  rw.prpwPages[3] = &RPPlayer1Late;
  rw.prpwTrunc = &RPTrunc;
  rw.fCubeEqualChequer = fCubeEqualChequer;
  rw.fPlayersAreSame = fPlayersAreSame;
  rw.fTruncEqualPlayer0 = fTruncEqualPlayer0;
  rw.pfOK = &fOK;

  RPPlayer0.precCube = &rw.rcRollout.aecCube[0];
  RPPlayer0.precCheq = &rw.rcRollout.aecChequer[0];
  RPPlayer0.pmf = (movefilter *) rw.rcRollout.aaamfChequer[ 0 ];

  RPPlayer0Late.precCube = &rw.rcRollout.aecCubeLate[0];
  RPPlayer0Late.precCheq = &rw.rcRollout.aecChequerLate[0];
  RPPlayer0Late.pmf = (movefilter *) rw.rcRollout.aaamfLate[ 0 ];

  RPPlayer1.precCube = &rw.rcRollout.aecCube[1];
  RPPlayer1.precCheq = &rw.rcRollout.aecChequer[1];
  RPPlayer1.pmf = (movefilter *) rw.rcRollout.aaamfChequer[ 1 ];

  RPPlayer1Late.precCube = &rw.rcRollout.aecCubeLate[1];
  RPPlayer1Late.precCheq = &rw.rcRollout.aecChequerLate[1];
  RPPlayer1Late.pmf = (movefilter *) rw.rcRollout.aaamfLate[ 1 ];

  RPTrunc.precCube = &rw.rcRollout.aecCubeTrunc;
  RPTrunc.precCheq = &rw.rcRollout.aecChequerTrunc;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - Rollouts"), DT_QUESTION,
                           GTK_SIGNAL_FUNC( SetRolloutsOK ), &rw );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     rw.RolloutNotebook = gtk_notebook_new() );
  gtk_container_set_border_width( GTK_CONTAINER( rw.RolloutNotebook ), 4 );


  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPageGeneral (rw.prwGeneral, &rw),
                            gtk_label_new ( _("General Settings" ) ) );

  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPage (rw.prpwPages[0], TRUE ),
                            gtk_label_new ( _("First Play (0) ") ) );

  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPage (rw.prpwPages[1], TRUE ),
                            gtk_label_new ( _("First Play (1) ") ) );

  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPage (rw.prpwPages[2], TRUE ),
                            gtk_label_new ( _("Later Play (0) ") ) );

  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPage (rw.prpwPages[3], TRUE ),
                            gtk_label_new ( _("Later Play (1) ") ) );

  gtk_notebook_append_page( GTK_NOTEBOOK( rw.RolloutNotebook ), 
                            RolloutPage (rw.prpwTrunc, FALSE ),
                            gtk_label_new ( _("Truncation Pt.") ) );

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
  gtk_widget_show_all( pwDialog );
   
  GTKDisallowStdin();

  /* cheap and nasty way to get things set correctly */
  LateEvalToggled (NULL, &rw);
  STDStopToggled (NULL, &rw);
  JsdStopToggled (NULL, &rw);
  TruncEnableToggled (NULL, &rw);
  CubeEqCheqToggled (NULL, &rw);
  PlayersSameToggled (NULL, &rw);
  CubefulToggled (NULL, &rw);
  
  gtk_main();

  GTKAllowStdin();

  if( fOK ) {
    int fCubeful;
    outputpostpone();

    if((fCubeful = rw.rcRollout.fCubeful) != rcRollout.fCubeful ) {
      sprintf( sz, "set rollout cubeful %s",
              fCubeful ? "on" : "off" );
      UserCommand( sz );

		for (i = 0; i < 2; ++i) {
		  /* replicate main page cubeful on/of in all evals */
		  rw.rcRollout.aecChequer[i].fCubeful = fCubeful;
		  rw.rcRollout.aecChequerLate[i].fCubeful = fCubeful;
		  rw.rcRollout.aecCube[i].fCubeful = fCubeful;
		  rw.rcRollout.aecCubeLate[i].fCubeful = fCubeful;
		}
		rw.rcRollout.aecCubeTrunc.fCubeful = 
		  rw.rcRollout.aecChequerTrunc.fCubeful = fCubeful;
	
    }

    for (i = 0; i < 2; ++i) {


      if (EvalCmp (&rw.rcRollout.aecCube[i], &rcRollout.aecCube[i], 1) ) {
        sprintf (sz, "set rollout player %d cubedecision", i);
        SetEvalCommands( sz, &rw.rcRollout.aecCube[i], 
                         &rcRollout.aecCube[ i ] );
      }
      if (EvalCmp (&rw.rcRollout.aecChequer[i], 
                   &rcRollout.aecChequer[i], 1)) {
        sprintf (sz, "set rollout player %d chequer", i);
        SetEvalCommands( sz, &rw.rcRollout.aecChequer[i], 
                         &rcRollout.aecChequer[ i ] );
      }
      
      sprintf ( sz, "set rollout player %d movefilter", i );
      SetMovefilterCommands ( sz, 
                              rw.rcRollout.aaamfChequer[ i ], 
                              rcRollout.aaamfChequer[ i ] );

      if (EvalCmp (&rw.rcRollout.aecCubeLate[i], 
                   &rcRollout.aecCubeLate[i], 1 ) ) {
        sprintf (sz, "set rollout late player %d cube", i);
        SetEvalCommands( sz, &rw.rcRollout.aecCubeLate[i], 
                         &rcRollout.aecCubeLate[ i ] );
      }

      if (EvalCmp (&rw.rcRollout.aecChequerLate[i], 
                   &rcRollout.aecChequerLate[i], 1 ) ) {
        sprintf (sz, "set rollout late player %d chequer", i);
        SetEvalCommands( sz, &rw.rcRollout.aecChequerLate[i], 
                         &rcRollout.aecChequerLate[ i ] );
      }

      sprintf ( sz, "set rollout late player %d movefilter", i );
      SetMovefilterCommands ( sz, 
                              rw.rcRollout.aaamfLate[ i ], 
                              rcRollout.aaamfLate[ i ] );

    }

    if (EvalCmp (&rw.rcRollout.aecCubeTrunc, &rcRollout.aecCubeTrunc, 1) ) {
      SetEvalCommands( "set rollout truncation cube", 
                       &rw.rcRollout.aecCubeTrunc, &rcRollout.aecCubeTrunc );
    }

    if (EvalCmp (&rw.rcRollout.aecChequerTrunc, 
                 &rcRollout.aecChequerTrunc, 1) ) {
      SetEvalCommands( "set rollout truncation chequer", 
                       &rw.rcRollout.aecChequerTrunc, 
                       &rcRollout.aecChequerTrunc );
    }

    if( rw.rcRollout.fCubeful != rcRollout.fCubeful ) {
      sprintf( sz, "set rollout cubeful %s",
               rw.rcRollout.fCubeful ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fVarRedn != rcRollout.fVarRedn ) {
      sprintf( sz, "set rollout varredn %s",
               rw.rcRollout.fVarRedn ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fInitial != rcRollout.fInitial ) {
      sprintf( sz, "set rollout initial %s",
               rw.rcRollout.fInitial ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fRotate != rcRollout.fRotate ) {
      sprintf( sz, "set rollout quasirandom %s",
               rw.rcRollout.fRotate ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fLateEvals != rcRollout.fLateEvals ) {
      sprintf( sz, "set rollout late enable  %s",
               rw.rcRollout.fLateEvals ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fDoTruncate != rcRollout.fDoTruncate ) {
      sprintf( sz, "set rollout truncation enable  %s",
               rw.rcRollout.fDoTruncate ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.nTruncate != rcRollout.nTruncate ) {
      sprintf( sz, "set rollout truncation plies %d", 
               rw.rcRollout.nTruncate );
      UserCommand( sz );
    }

    if( rw.rcRollout.nTrials != rcRollout.nTrials ) {
      sprintf( sz, "set rollout trials %d", rw.rcRollout.nTrials );
      UserCommand( sz );
    }

    if( rw.rcRollout.fStopOnSTD != rcRollout.fStopOnSTD ) {
      sprintf( sz, "set rollout limit enable %s", rw.rcRollout.fStopOnSTD ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.nMinimumGames != rcRollout.nMinimumGames ) {
      sprintf( sz, "set rollout limit minimumgames %d", rw.rcRollout.nMinimumGames );
      UserCommand( sz );
    }

    if( fabs( rw.rcRollout.rStdLimit - rcRollout.rStdLimit ) > epsilon ) {
      gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
      sprintf( sz, "set rollout limit maxerr %s",
                                  g_ascii_formatd( buf, G_ASCII_DTOSTR_BUF_SIZE,
			           "%5.4f", rw.rcRollout.rStdLimit ));
      UserCommand( sz );
    }

    /* ======================= */

    if( rw.rcRollout.fStopOnJsd != rcRollout.fStopOnJsd ) {
      sprintf( sz, "set rollout jsd stop %s", rw.rcRollout.fStopOnJsd ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fStopMoveOnJsd != rcRollout.fStopMoveOnJsd ) {
      sprintf( sz, "set rollout jsd move %s", rw.rcRollout.fStopMoveOnJsd ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.nMinimumJsdGames != rcRollout.nMinimumJsdGames ) {
      sprintf( sz, "set rollout jsd minimumgames %d", rw.rcRollout.nMinimumJsdGames );
      UserCommand( sz );
    }

    if( fabs( rw.rcRollout.rJsdLimit - rcRollout.rJsdLimit ) > epsilon ) {
      gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
      sprintf( sz, "set rollout jsd limit %s",
                                  g_ascii_formatd( buf, G_ASCII_DTOSTR_BUF_SIZE,
			           "%5.4f", rw.rcRollout.rJsdLimit ));
      UserCommand( sz );
    }


    if( rw.rcRollout.nLate != rcRollout.nLate ) {
      sprintf( sz, "set rollout late plies %d", rw.rcRollout.nLate );
      UserCommand( sz );
    }

    if( abs(rw.rcRollout.nSeed) != abs(rcRollout.nSeed) ) {
      sprintf( sz, "set rollout seed %d", rw.rcRollout.nSeed );
      UserCommand( sz );
    }

    if( rw.rcRollout.fTruncBearoff2 != rcRollout.fTruncBearoff2 ) {
      sprintf( sz, "set rollout bearofftruncation exact %s", 
               rw.rcRollout.fTruncBearoff2 ? "on" : "off" );
      UserCommand( sz );
    }

    if( rw.rcRollout.fTruncBearoffOS != rcRollout.fTruncBearoffOS ) {
      sprintf( sz, "set rollout bearofftruncation onesided %s", 
               rw.rcRollout.fTruncBearoffOS ? "on" : "off" );
      UserCommand( sz );
    }

	if ( rw.fCubeEqualChequer != fCubeEqualChequer) {
	  sprintf( sz, "set rollout cube-equal-chequer %s",
               rw.fCubeEqualChequer ? "on" : "off" );
	  UserCommand( sz );
	}

	if ( rw.fPlayersAreSame != fPlayersAreSame) {
	  sprintf( sz, "set rollout players-are-same %s",
               rw.fPlayersAreSame ? "on" : "off" );
	  UserCommand( sz );
	}

	if ( rw.fTruncEqualPlayer0 != fTruncEqualPlayer0) {
	  sprintf( sz, "set rollout truncate-equal-player0 %s",
               rw.fTruncEqualPlayer0 ? "on" : "off" );
	  UserCommand( sz );
	}

    outputresume();
  }	
}


extern void GTKEval( char *szOutput ) {

    GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Evaluation"), DT_INFO, NULL, NULL );
    GtkWidget *pwText;
    GtkWidget *sw;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    pwText = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (pwText), GTK_WRAP_NONE);

    buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_create_tag (buffer, "monospace", "family", "monospace", NULL);
    gtk_text_buffer_get_end_iter (buffer, &iter);
    gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, szOutput, -1, "monospace", NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW (pwText), buffer);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (sw), pwText);

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       sw );
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 700, 600 );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ), GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy", GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
 
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();


}

static void DestroyHint( gpointer p ) {

  movelist *pml = p;

  if ( pml ) {
    if ( pml->amMoves )
      free ( pml->amMoves );
    
    free ( pml );
  }

  SetPanelWidget(WINDOW_HINT, NULL);
}

static void
HintOK ( GtkWidget *pw, void *unused )
{
	getWindowGeometry(WINDOW_HINT);
	DestroyPanel(WINDOW_HINT);
}

extern void GTKCubeHint( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
			 float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
			  const evalsetup *pes ) {
    
    static evalsetup es;
    GtkWidget *pw, *pwHint;

    if (GetPanelWidget(WINDOW_HINT))
	gtk_widget_destroy(GetPanelWidget(WINDOW_HINT));

	pwHint = GTKCreateDialog( _("GNU Backgammon - Hint"), DT_INFO,
			   GTK_SIGNAL_FUNC( HintOK ), NULL );
	SetPanelWidget(WINDOW_HINT, pwHint);

    memcpy ( &es, pes, sizeof ( evalsetup ) );

    pw = CreateCubeAnalysis ( aarOutput, aarStdDev, &es, MOVE_NORMAL );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwHint, DA_MAIN ) ),
                       pw );

    gtk_widget_grab_focus( DialogArea( pwHint, DA_OK ) );
    
    setWindowGeometry(WINDOW_HINT);
    gtk_object_weakref( GTK_OBJECT( pwHint ), DestroyHint, NULL );
    
    gtk_window_set_default_size(GTK_WINDOW(pwHint), 400, 300);
    
    gtk_widget_show_all( pwHint );


}

/*
 * Give hints for resignation
 *
 * Input:
 *    rEqBefore: equity before resignation
 *    rEqAfter: equity after resignation
 *    pci: cubeinfo
 *    fOUtputMWC: output in MWC or equity
 *
 * FIXME: include arOutput in the dialog, so the the user
 *        can see how many gammons/backgammons she'll win.
 */

extern void
GTKResignHint( float arOutput[], float rEqBefore, float rEqAfter,
               cubeinfo *pci, int fMWC ) {
    
    GtkWidget *pwDialog =
	GTKCreateDialog( _("GNU Backgammon - Hint"), DT_INFO, NULL, NULL );
    GtkWidget *pw;
    GtkWidget *pwTable = gtk_table_new( 2, 3, FALSE );

    char *pch, sz[ 16 ];

    /* equity before resignation */
    
    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new(
	fMWC ? _("MWC before resignation") : _("Equity before resignation") ), 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

    if( fMWC )
      sprintf( sz, "%6.2f%%", 100.0 * ( eq2mwc ( - rEqBefore, pci ) ) );
    else
      sprintf( sz, "%+6.3f", -rEqBefore );

    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new( sz ),
		      1, 2, 0, 1, GTK_EXPAND | GTK_FILL,
		      GTK_EXPAND | GTK_FILL, 4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );

    /* equity after resignation */

    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new(
	fMWC ? _("MWC after resignation") : _("Equity after resignation") ), 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    
    if( fMWC )
      sprintf( sz, "%6.2f%%",
               100.0 * eq2mwc ( - rEqAfter, pci ) );
    else
      sprintf( sz, "%+6.3f", - rEqAfter );

    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new( sz ),
		      1, 2, 1, 2, GTK_EXPAND | GTK_FILL,
		      GTK_EXPAND | GTK_FILL, 4, 0 );
    gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );

    if ( -rEqAfter >= -rEqBefore )
	pch = _("You should accept the resignation!");
    else
	pch = _("You should reject the resignation!");
    
    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new( pch ),
		      0, 2, 2, 3, GTK_EXPAND | GTK_FILL,
		      GTK_EXPAND | GTK_FILL, 0, 8 );

    gtk_container_set_border_width( GTK_CONTAINER( pwTable ), 8 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwTable );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
 
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();
}

extern void GTKWinCopy( GtkWidget *widget, gpointer data) {
   TextToClipboard( (char *) data);
}

extern void 
GTKHint( movelist *pmlOrig, const int iMove) {

    GtkWidget *pwButtons, *pwMoves, *pwHint;
    movelist *pml;
    static int n;
    
    if (GetPanelWidget(WINDOW_HINT))
	gtk_widget_destroy(GetPanelWidget(WINDOW_HINT));

    pml = malloc( sizeof( *pml ) );
    memcpy( pml, pmlOrig, sizeof( *pml ) );
    
    pml->amMoves = malloc( pmlOrig->cMoves * sizeof( move ) );
    memcpy( pml->amMoves, pmlOrig->amMoves, pmlOrig->cMoves * sizeof( move ) );

    n = iMove;
    pwMoves = CreateMoveList( pml, ( n < 0 ) ? NULL : &n, TRUE, TRUE, TRUE );

    /* create dialog */

	pwHint = GTKCreateDialog( _("GNU Backgammon - Hint"), DT_INFO,
			   GTK_SIGNAL_FUNC( HintOK ), NULL );
	SetPanelWidget(WINDOW_HINT, pwHint);
    pwButtons = DialogArea(pwHint, DA_BUTTONS );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwHint, DA_MAIN ) ), 
                       pwMoves );

    setWindowGeometry(WINDOW_HINT);
    gtk_object_weakref( GTK_OBJECT( pwHint ), DestroyHint, pml );

	if (!IsPanelDocked(WINDOW_HINT))
		gtk_window_set_default_size(GTK_WINDOW(pwHint), 400, 300);

    gtk_widget_show_all( pwHint );
}

static void SetMouseCursor(GdkCursorType cursorType)
{
	if (cursorType)
	{
		GdkCursor *cursor;
		cursor = gdk_cursor_new(cursorType);
		gdk_window_set_cursor(pwMain->window, cursor);
		gdk_cursor_unref(cursor);
	}
	else
		gdk_window_set_cursor(pwMain->window, NULL);
}

extern void GTKProgressStart( char *sz ) {

    if( sz )
	gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idProgress, sz );

	SetMouseCursor(GDK_WATCH);
}


extern void
GTKProgressStartValue( char *sz, int iMax ) {


  if( sz )
    gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idProgress, sz );

	SetMouseCursor(GDK_WATCH);
}

extern void
GTKProgressValue ( int iValue, int iMax ) {

    gchar *gsz;
    gdouble frac = 1.0 * iValue / (1.0 * iMax );
    gsz = g_strdup_printf("%d/%d (%.0f%%)", iValue, iMax, 100 * frac);
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR( pwProgress ), gsz);
    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( pwProgress ), frac);
    g_free(gsz);

    SuspendInput();

    while( gtk_events_pending() )
	gtk_main_iteration();

    ResumeInput();
}


extern void GTKProgress( void ) {

    gtk_progress_bar_pulse( GTK_PROGRESS_BAR( pwProgress ) );

    SuspendInput();

    while( gtk_events_pending() )
        gtk_main_iteration();

    ResumeInput();
}

extern void GTKProgressEnd( void ) {

    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( pwProgress ), 0.0 );
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR( pwProgress ), " " );
    gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idProgress );

	SetMouseCursor(0);
}

int colWidth;

void MoveListIntoView(GtkWidget *pwList, int row)
{
  if (gtk_clist_row_is_visible(GTK_CLIST(pwList), row) != GTK_VISIBILITY_FULL)
  {
    gtk_clist_moveto(GTK_CLIST(pwList), row, 0, 0, 0);
    gtk_widget_set_usize(GTK_WIDGET(pwList), colWidth * 2 + 50, -1);
  }
}

extern void GTKShowScoreSheet( void )
{
	GtkWidget *pwDialog, *pwBox;
	GtkWidget *hbox;
	GtkWidget *pwList;
	GtkWidget* pwScrolled;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	int width1, width2;
	int i;
	int numRows = 0;
	char title[100];
	char *titles[2];
	char *data[2];
	list *pl;

	sprintf(title, _("Score Sheet - "));
	if ( ms.nMatchTo > 0 )
		sprintf(title + strlen(title),
			ngettext("Match to %d point",
				 "Match to %d points",
				 ms.nMatchTo),
			ms.nMatchTo);
	else
		strcat(title, _("Money Session"));

	pwDialog = GTKCreateDialog(title, DT_INFO, NULL, NULL );
	pwBox = gtk_vbox_new( FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(pwBox), 8);

	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), pwBox);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );


	gtk_widget_set_usize(GTK_WIDGET (pwDialog), -1, 200);
	gtk_container_set_border_width(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), 4);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), hbox);

	titles[0] = ap[0].szName;
	titles[1] = ap[1].szName;

	pwList = gtk_clist_new_with_titles( 2, titles );
	GTK_WIDGET_UNSET_FLAGS(pwList, GTK_CAN_FOCUS);
	gtk_clist_column_titles_passive( GTK_CLIST( pwList ) );

	layout = gtk_widget_create_pango_layout(pwList, titles[0]);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	g_object_unref (layout);
	width1 = logical_rect.width;

	layout = gtk_widget_create_pango_layout(pwList, titles[1]);
	pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	g_object_unref (layout);
	width2 = logical_rect.width;

	colWidth = MAX(width1, width2);

	data[0] = malloc(10);
	data[1] = malloc(10);

	for (pl = lMatch.plNext; pl->p; pl = pl->plNext )
	{
		int score[2];
		list *plGame = pl->plNext->p;

		if (plGame)
		{
			moverecord *pmr = plGame->plNext->p;
			score[0] = pmr->g.anScore[0];
			score[1] = pmr->g.anScore[1];
		}
		else
		{
			moverecord *pmr;
			list *plGame = pl->p;
			if (!plGame)
				continue;
			pmr = plGame->plNext->p;
			score[0] = pmr->g.anScore[0];
			score[1] = pmr->g.anScore[1];
			if (pmr->g.fWinner == -1)
			{
				if (pl == lMatch.plNext)
				{	/* First game */
					score[0] = score[1] = 0;
				}
				else
					continue;
			}
			else
				score[pmr->g.fWinner] += pmr->g.nPoints;
		}
		sprintf(data[0], "%d", score[0]);
		sprintf(data[1], "%d", score[1]);
		gtk_clist_append(GTK_CLIST(pwList), data);
		numRows++;
	}

	free(data[0]);
	free(data[1]);

	for( i = 0; i < 2; i++ )
	{
	gtk_clist_set_column_justification(GTK_CLIST(pwList), i, GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_width( GTK_CLIST( pwList ), i, colWidth);
	gtk_clist_set_column_resizeable(GTK_CLIST(pwList), i, FALSE);
	}

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(pwScrolled), pwList);

	gtk_widget_set_usize(GTK_WIDGET(pwList), colWidth * 2 + 20, -1);

	gtk_box_pack_start(GTK_BOX(hbox), pwScrolled, TRUE, FALSE, 0);

	gtk_clist_select_row(GTK_CLIST(pwList), numRows - 1, 1);

	gtk_signal_connect(GTK_OBJECT(pwList), "realize",
			GTK_SIGNAL_FUNC(MoveListIntoView), (gpointer *)(numRows - 1) );

	gtk_widget_show_all( pwDialog );
	gtk_main();
}

extern char *aszCopying[], *aszWarranty[];

void GtkShowCopying(GtkWidget *pwWidget)
{
	ShowList( aszCopying, _("Copying"), gtk_widget_get_toplevel(pwWidget) );
}

void GtkShowWarranty(GtkWidget *pwWidget)
{
	ShowList( aszWarranty, _("Warranty"), gtk_widget_get_toplevel(pwWidget) );
}

extern void GTKShowVersion( void ) {

#include "xpm/gnubg-big.xpm"
	GtkWidget *pwDialog;
	GtkWidget *pwTopHBox = gtk_hbox_new( FALSE, -8 ),
		*pwImageVBox = gtk_vbox_new( FALSE, 0 ),
		*pwButtonBox = gtk_vbox_new( FALSE, 0 ),
		*pwOK, *pwPrompt, *pwImage, *pwButton;
	GtkRcStyle *ps = gtk_rc_style_new();
	char PromptStr[255];
	GtkAccelGroup *pag = gtk_accel_group_new();

	pwDialog = gtk_dialog_new();
	gtk_window_set_title( GTK_WINDOW( pwDialog ), _("About GNU Backgammon") );
	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );

	gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->vbox ), pwTopHBox );

	gtk_box_pack_start( GTK_BOX( pwTopHBox ), pwImageVBox, FALSE, FALSE, 0 );
	/* image */
	pwImage = image_from_xpm_d ( gnubg_big_xpm, pwDialog );
	gtk_misc_set_padding( GTK_MISC( pwImage ), 8, 8 );
	gtk_box_pack_start( GTK_BOX( pwImageVBox ), pwImage, FALSE, FALSE, 0 );

	sprintf(PromptStr, "%s", _(VERSION_STRING));
	pwPrompt = gtk_label_new(PromptStr);
	gtk_box_pack_start( GTK_BOX( pwImageVBox ), pwPrompt, FALSE, FALSE, 0 );
	ps->font_desc = pango_font_description_new();
	pango_font_description_set_family_static( ps->font_desc, "serif" );
	pango_font_description_set_size( ps->font_desc, 24 * PANGO_SCALE );    
	gtk_widget_modify_style( pwPrompt, ps );
	gtk_rc_style_unref( ps );

	pwOK = gtk_button_new_with_label( _("OK") );

	gtk_window_add_accel_group( GTK_WINDOW( pwDialog ), pag );
	gtk_widget_add_accelerator( pwOK, "clicked", pag, GDK_Escape, 0, 0 );

	gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwDialog )->action_area ), pwOK );
	GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
	gtk_widget_grab_default( pwOK );
	gtk_signal_connect( GTK_OBJECT( pwOK ), "clicked", GTK_SIGNAL_FUNC( OK ), NULL );

	/* Buttons on right side */
	gtk_box_pack_start( GTK_BOX( pwTopHBox ), pwButtonBox, FALSE, FALSE, 8 );

	gtk_box_pack_start( GTK_BOX( pwButtonBox ), 
	pwButton = gtk_button_new_with_label(_("Credits") ),
	FALSE, FALSE, 8 );
	gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
		GTK_SIGNAL_FUNC( GTKCommandShowCredits ), NULL );
	
	gtk_box_pack_start( GTK_BOX( pwButtonBox ), 
	pwButton = gtk_button_new_with_label(_("Build Info") ),
	FALSE, FALSE, 8 );
	gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
		GTK_SIGNAL_FUNC( GTKShowBuildInfo ), NULL );
	
	gtk_box_pack_start( GTK_BOX( pwButtonBox ), 
	pwButton = gtk_button_new_with_label(_("Copying conditions") ),
	FALSE, FALSE, 8 );
	gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
		GTK_SIGNAL_FUNC( GtkShowCopying ), NULL );
	
	gtk_box_pack_start( GTK_BOX( pwButtonBox ), 
	pwButton = gtk_button_new_with_label(_("Warranty") ),
	FALSE, FALSE, 8 );
	gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
		GTK_SIGNAL_FUNC( GtkShowWarranty ), NULL );

	gtk_widget_show_all( pwDialog );
}

GtkWidget* SelectableLabel(GtkWidget* reference, char* text)
{
	GtkWidget* pwLabel = gtk_label_new(text);
	gtk_label_set_selectable(GTK_LABEL(pwLabel), TRUE);
	return pwLabel;
}

extern void GTKShowBuildInfo(GtkWidget *pwParent)
{
	GtkWidget *pwDialog, *pwBox, *pwPrompt;
	char* pch;

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Build Info"),
					DT_INFO, NULL, NULL );
	pwBox = gtk_vbox_new( FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(pwBox), 8);

	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), pwBox);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( gtk_widget_get_toplevel(pwParent) ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_box_pack_start( GTK_BOX( pwBox ), SelectableLabel(pwDialog, "Version " VERSION 
#ifdef WIN32
		" (build " __DATE__ ")"
#endif
		), FALSE, FALSE, 4 );

	gtk_box_pack_start(GTK_BOX(pwBox), gtk_hseparator_new(), FALSE, FALSE, 4);

    while((pch = GetBuildInfoString()))
		gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt = gtk_label_new( gettext(pch) ),
			FALSE, FALSE, 0 );

	gtk_box_pack_start(GTK_BOX(pwBox), gtk_hseparator_new(), FALSE, FALSE, 4);

	gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( _(aszCOPYRIGHT) ), FALSE, FALSE, 4 );

	gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt = gtk_label_new(
	_("GNU Backgammon is free software, covered by the GNU General Public "
	"License version 2, and you are welcome to change it and/or "
	"distribute copies of it under certain conditions.  There is "
	"absolutely no warranty for GNU Backgammon.") ), FALSE, FALSE, 4 );
	gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );
	
	gtk_widget_show_all( pwDialog );
	gtk_main();
}

/* Stores names in credits so not duplicated in list at bottom */
list names;

static void AddTitle(GtkWidget* pwBox, char* Title)
{
	GtkRcStyle *ps = gtk_rc_style_new();
	GtkWidget* pwTitle = gtk_label_new(Title),
		*pwHBox = gtk_hbox_new( TRUE, 0);
	gtk_box_pack_start(GTK_BOX(pwBox), pwHBox, FALSE, FALSE, 4);

	ps->font_desc = pango_font_description_new();
	pango_font_description_set_family_static( ps->font_desc, "serif" );
	pango_font_description_set_size( ps->font_desc, 16 * PANGO_SCALE );    
	gtk_widget_modify_style( pwTitle, ps );
	gtk_rc_style_unref( ps );

	gtk_box_pack_start(GTK_BOX(pwHBox), pwTitle, TRUE, FALSE, 0);
}

static void AddName(GtkWidget* pwBox, char* name, char* type)
{
	char buf[255];
	if (type)
		sprintf(buf, "%s: %s", type, name);
	else
		strcpy(buf, name);

	gtk_box_pack_start(GTK_BOX(pwBox), gtk_label_new(buf), FALSE, FALSE, 0);
	ListInsert(&names, name);
}

static int FindName(list* pList, char* name)
{
	list *pl;
	for (pl = pList->plNext; pl != pList; pl = pl->plNext )
	{
		if (!strcmp(pl->p, name))
			return TRUE;
	}
	return FALSE;
}

extern void GTKCommandShowCredits(GtkWidget *pwParent)
{
	GtkWidget *pwDialog, *pwBox, *pwMainHBox, *pwHBox = 0, *pwVBox,
		*pwList = gtk_list_new(),
		*pwScrolled = gtk_scrolled_window_new( NULL, NULL );
	int i = 0;
	credits *credit = &creditList[0];
	credEntry* ce;

	ListCreate(&names);

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Credits"),
					DT_INFO, NULL, NULL );

	pwMainHBox = gtk_hbox_new(FALSE, 0);

	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), pwMainHBox);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( gtk_widget_get_toplevel(pwParent) ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	pwBox = gtk_vbox_new( FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwMainHBox), pwBox, FALSE, FALSE, 0);
	gtk_container_set_border_width( GTK_CONTAINER(pwBox), 8);

	while (credit->Title)
	{
		/* Two columns, so new hbox every-other one */
		if (i / 2 == (i + 1) / 2)
		{
			pwHBox = gtk_hbox_new( TRUE, 0);
			gtk_box_pack_start(GTK_BOX(pwBox), pwHBox, TRUE, FALSE, 0);
		}

		pwVBox = gtk_vbox_new( FALSE, 0);
		gtk_box_pack_start(GTK_BOX(pwHBox), pwVBox, FALSE, FALSE, 0);

		AddTitle(pwVBox, _(credit->Title));

		ce = credit->Entry;
		while(ce->Name)
		{
			AddName(pwVBox, ce->Name, _(ce->Type));
			ce++;
		}
		if (i == 1)
			gtk_box_pack_start(GTK_BOX(pwBox), gtk_hseparator_new(), FALSE, FALSE, 4);
		credit++;
		i++;
	}

	pwVBox = gtk_vbox_new( FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwMainHBox), pwVBox, FALSE, FALSE, 0);

	AddTitle(pwVBox, _("Special thanks"));

	gtk_container_set_border_width( GTK_CONTAINER(pwVBox), 8);
	gtk_box_pack_start( GTK_BOX( pwVBox ), pwScrolled, TRUE, TRUE, 0 );
	gtk_widget_set_usize( pwScrolled, 150, -1 );
	gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( pwScrolled ),
						pwList );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolled ),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );

	for( i = 0; ceCredits[ i ].Name; i++ ) {
		if (!FindName(&names, ceCredits[ i ].Name ))
		gtk_container_add( GTK_CONTAINER( pwList ),
			gtk_list_item_new_with_label(ceCredits[ i ].Name)) ;
	}

	while(names.plNext->p)
		ListDelete(names.plNext );

	gtk_widget_show_all( pwDialog );
	gtk_main();
}

#if HAVE_GTKTEXI
static int ShowManualSection( char *szTitle, char *szNode )
{
    static GtkWidget *pw = NULL;
    char *pch;
    
    if( pw ) {
	gtk_window_present( GTK_WINDOW( pw ) );
	gtk_texi_render_node( GTK_TEXI( pw ), szNode );
	return 0;
    }

    if( !( pch = PathSearch( "doc/gnubg.xml", szDataDirectory ) ) )
	return -1;
    else if( access( pch, R_OK ) ) {
	outputerr( pch );
	free( pch );
	return -1;
    }
    
    pw = gtk_texi_new();
    g_object_add_weak_pointer( G_OBJECT( pw ), (gpointer *) &pw );
    gtk_window_set_title( GTK_WINDOW( pw ), _(szTitle) );
    gtk_window_set_default_size( GTK_WINDOW( pw ), 600, 400 );
    gtk_widget_show_all( pw );

    gtk_texi_load( GTK_TEXI( pw ), pch );
    gtk_texi_render_node( GTK_TEXI( pw ), szNode );

    free( pch );
    return 0;
}
#endif

static void NoManual( void ) {
    
    outputl( _("The online manual is not available with this installation of "
	     "GNU Backgammon.  You can view the manual on the WWW at:\n"
	     "\n"
	     "    http://www.gnubg.org/win32/gnubg/gnubg.html") );
    /* Temporary place of manual, while Oystein is fixing the web system.
       (Work in progress) */
    
    outputx();
}

static void NoFAQ( void ) {
    
    outputl( _("The Frequently Asked Questions are not available with this "
	       "installation of GNU Backgammon.  You can view them on "
	       "the WWW at:\n"
	       "\n"
	       "    http://www.gnubg.org/docs/faq/") );

    outputx();
}

extern void
GTKShowManual( void ) {

#if HAVE_GTKTEXI
    if( !ShowManualSection( _("GNU Backgammon - Manual"), "Top" ) )
	return;
#endif

    NoManual();
}


static void
ReportBug( gpointer *p, guint n, GtkWidget *pwEvent ) {

	char *pchOS = "109";
        char sz[ 1024 ];

#if WIN32

	OSVERSIONINFO VersionInfo;

	memset( &VersionInfo, 0, sizeof( OSVERSIONINFO ) );
	VersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	GetVersionEx( &VersionInfo );

	switch ( VersionInfo.dwPlatformId ) {
		case VER_PLATFORM_WIN32s:
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
		case VER_PLATFORM_WIN32_NT:
			switch ( VersionInfo.dwMajorVersion ) {
				case 3:
					/* Windows  NT 3.51 */
					pchOS = "106";
					break;
				case 4:
					switch ( VersionInfo.dwMinorVersion ) {
						case 0:
							switch ( VersionInfo.dwPlatformId ) {
								case VER_PLATFORM_WIN32_WINDOWS:
									/* Windows  95 */
									pchOS = "104";
									break;
								case VER_PLATFORM_WIN32_NT:
									/* Windows NT 4.0 */
									pchOS = "106";
									break;
							}
							break;
						case 10:
							if ( VersionInfo.szCSDVersion[1] == 'A' )
								/* Windows  98 SE */
								pchOS = "110";
							else
								/* Windows  98 */
								pchOS = "105";
							break;
						case 90:
							/* Windows  Me */
							pchOS = "105";
							break;
					}
					break;
				case 5:
					switch ( VersionInfo.dwMinorVersion ) {
						case 0:
							/* Windows  2000 */
							pchOS = "107";
							break;
						case 1:
							/* Windows XP */
							pchOS = "108";
							break;
						case 2:
							/* Windows Server 2003 */
							pchOS = "109";
							break;
					}
					break;
			}
			break;
	}
#elif HAVE_SYS_UTSNAME_H /* WIN32 */

 {
   struct utsname u;

   if ( uname( &u ) )
     pchOS = "";
   else {

     if ( ! strcasecmp( u.sysname, "linux" ) )
       pchOS = "101";
     else if ( ! strcasecmp( u.sysname, "sunos" ) )
       pchOS = "102";
     else if ( ! strcasecmp( u.sysname, "freebsd" ) )
       pchOS = "103";
     else if ( ! strcasecmp( u.sysname, "rhapsody" ) ||
               ! strcasecmp( u.sysname, "darwin" ) ) 
       pchOS = "112";

   }

 }

#endif /* HAVE_SYS_UTSNAME_H */

	sprintf( sz, "http://savannah.gnu.org/bugs/?func=additem&group=gnubg"
		     "&release_id="	"110"
		     "&custom_tf1="	__DATE__
		     "&platform_version_id=%s", pchOS );
 	OpenURL( sz );
}


static void ShowFAQ( gpointer *p, guint n, GtkWidget *pwEvent ) {
#if HAVE_GTKTEXI
    if( !ShowManualSection( _("GNU Backgammon - Frequently Asked Questions"),
			    "Frequently Asked Questions" ) )
	return;
#endif

    NoFAQ();
}

static GtkWidget *pwHelpTree, *pwHelpLabel;

static void GTKHelpAdd( GtkTreeStore *pts, GtkTreeIter *ptiParent,
			command *pc ) {
    GtkTreeIter ti;
    
    for( ; pc->sz; pc++ )
	if( pc->szHelp ) {
	    gtk_tree_store_append( pts, &ti, ptiParent );
	    gtk_tree_store_set( pts, &ti, 0, pc->sz, 1, pc->szHelp,
				2, pc, -1 );
	    if( pc->pc && pc->pc->sz )
		GTKHelpAdd( pts, &ti, pc->pc );
	}
}

static void GTKHelpSelect( GtkTreeSelection *pts, gpointer p ) {

    GtkTreeModel *ptm;
    GtkTreeIter ti;
    GtkTreePath *ptp;
    command **apc;
    int i, c;
    char szCommand[ 128 ], *pchCommand = szCommand,
	szUsage[ 128 ], *pchUsage = szUsage, *pch;
    
    if( gtk_tree_selection_get_selected( pts, &ptm, &ti ) ) {
	ptp = gtk_tree_model_get_path( ptm, &ti );
	c = gtk_tree_path_get_depth( ptp );
	apc = malloc( c * sizeof( command * ) );
	for( i = c - 1; ; i-- ) {
	    gtk_tree_model_get( ptm, &ti, 2, apc + i, -1 );
	    if( !i )
		break;
	    gtk_tree_path_up( ptp );
	    gtk_tree_model_get_iter( ptm, &ti, ptp );
	}

	for( i = 0; i < c; i++ ) {
	    /* accumulate command and usage strings from path */
	    /* FIXME use markup a la gtk_label_set_markup for this */
	    pch = apc[ i ]->sz;
	    while( *pch )
		*pchCommand++ = *pchUsage++ = *pch++;
	    *pchCommand++ = ' '; *pchCommand = 0;
	    *pchUsage++ = ' '; *pchUsage = 0;

	    if( ( pch = apc[ i ]->szUsage ) ) {
		while( *pch )
		    *pchUsage++ = *pch++;
		*pchUsage++ = ' '; *pchUsage = 0;	
	    }		
	}

	pch = g_strdup_printf( _("%s- %s\n\nUsage: %s%s\n"), szCommand,
			       apc[ c - 1 ]->szHelp, szUsage,
			       ( apc[ c - 1 ]->pc && apc[ c - 1 ]->pc->sz ) ?
			       " <subcommand>" : "" );
	gtk_label_set_text( GTK_LABEL( pwHelpLabel ), pch );
	g_free( pch );
	
	free( apc );
	gtk_tree_path_free( ptp );
    } else
	gtk_label_set_text( GTK_LABEL( pwHelpLabel ), NULL );
}

extern void GTKHelp( char *sz ) {

    static GtkWidget *pw;
    GtkWidget *pwPaned, *pwScrolled;
    GtkTreeStore *pts;
    GtkTreeIter ti, tiSearch;
    GtkTreePath *ptp, *ptpExpand;
    char *pch;
    command *pc, *pcTest, *pcStart;
    int cch, i, c, *pn;
    void ( *pf )( char * );
    
    if( pw ) {
	gtk_window_present( GTK_WINDOW( pw ) );
	gtk_tree_view_collapse_all( GTK_TREE_VIEW( pwHelpTree ) );
	pts = GTK_TREE_STORE( gtk_tree_view_get_model(
	    GTK_TREE_VIEW( pwHelpTree ) ) );
    } else {
	pts = gtk_tree_store_new( 3, G_TYPE_STRING, G_TYPE_STRING,
				  G_TYPE_POINTER );

	GTKHelpAdd( pts, NULL, acTop );
	
	pw = gtk_dialog_new();
	g_object_add_weak_pointer( G_OBJECT( pw ), (gpointer *) &pw );
	gtk_window_set_title( GTK_WINDOW( pw ), _("Help - command reference") );
	gtk_window_set_default_size( GTK_WINDOW( pw ), 500, 400 );
	gtk_dialog_add_button(GTK_DIALOG(pw), GTK_STOCK_CLOSE, 
			GTK_RESPONSE_CLOSE);

	g_signal_connect_swapped (pw, "response",
			G_CALLBACK (gtk_widget_destroy), pw);

	gtk_container_add( GTK_CONTAINER( GTK_DIALOG(pw)->vbox ),
			pwPaned = gtk_vpaned_new() );

	gtk_paned_set_position( GTK_PANED( pwPaned ), 300 );
	
	gtk_paned_pack1( GTK_PANED( pwPaned ),
			 pwScrolled = gtk_scrolled_window_new( NULL, NULL ),
			 TRUE, FALSE );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(
	    pwScrolled ), GTK_SHADOW_IN );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolled ),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( pwScrolled ),
			   pwHelpTree = gtk_tree_view_new_with_model(
			       GTK_TREE_MODEL( pts ) ) );
	g_object_unref( G_OBJECT( pts ) );
	gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( pwHelpTree ),
					   FALSE );
	gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(
	    pwHelpTree ), 0, NULL, gtk_cell_renderer_text_new(),
						     "text", 0, NULL );
	gtk_tree_view_insert_column_with_attributes( GTK_TREE_VIEW(
	    pwHelpTree ), 1, NULL, gtk_cell_renderer_text_new(),
						     "text", 1, NULL );
	g_signal_connect( G_OBJECT( gtk_tree_view_get_selection(
	    GTK_TREE_VIEW( pwHelpTree ) ) ), "changed",
			  G_CALLBACK( GTKHelpSelect ), NULL );
	
	gtk_paned_pack2( GTK_PANED( pwPaned ),
			 pwHelpLabel = gtk_label_new( NULL ), FALSE, FALSE );
	gtk_label_set_selectable( GTK_LABEL( pwHelpLabel ), TRUE );
	
	gtk_widget_show_all( pw );
    }

    gtk_tree_model_get_iter_first( GTK_TREE_MODEL( pts ), &ti );
    tiSearch = ti;
    pc = acTop;
    c = 0;
    while( pc && sz && ( pch = NextToken( &sz ) ) ) {
	pcStart = pc;
	cch = strlen( pch );
	for( ; pc->sz; pc++ )
	    if( !strncasecmp( pch, pc->sz, cch ) )
		break;

	if( !pc->sz )
	    break;

	if( !pc->szHelp ) {
	    /* they gave a synonym; find the canonical version */
	    pf = pc->pf;
	    for( pc = pcStart; pc->sz; pc++ )
		if( pc->pf == pf && pc->szHelp )
		    break;

	    if( !pc->sz )
		break;
	}

	do
	    gtk_tree_model_get( GTK_TREE_MODEL( pts ), &tiSearch, 2,
				&pcTest, -1 );
	while( pcTest != pc &&
	       gtk_tree_model_iter_next( GTK_TREE_MODEL( pts ), &tiSearch ) );

	if( pcTest == pc ) {
	    /* found!  now try the next level down... */
	    c++;
	    ti = tiSearch;
	    pc = pc->pc;
	    gtk_tree_model_iter_children( GTK_TREE_MODEL( pts ), &tiSearch,
					  &ti );
	} else
	    break;
    }
    
    ptp = gtk_tree_model_get_path( GTK_TREE_MODEL( pts ), &ti );
    pn = gtk_tree_path_get_indices( ptp );
    ptpExpand = gtk_tree_path_new();
    for( i = 0; i < c; i++ ) {
	gtk_tree_path_append_index( ptpExpand, pn[ i ] );
	gtk_tree_view_expand_row( GTK_TREE_VIEW( pwHelpTree ), ptpExpand,
				  FALSE );
    }
    gtk_tree_selection_select_iter(
	gtk_tree_view_get_selection( GTK_TREE_VIEW( pwHelpTree ) ), &ti );
    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( pwHelpTree ), ptp,
				  NULL, TRUE, 0.5, 0 );
    gtk_tree_path_free( ptp );
    gtk_tree_path_free( ptpExpand );
}

static void GTKBearoffProgressCancel( void ) {

#ifdef SIGINT
    raise( SIGINT );
#endif
    exit( EXIT_FAILURE );
}

/* Show a dialog box with a progress bar to be used during initialisation
   if a heuristic bearoff database must be created.  Most of gnubg hasn't
   been initialised yet, so this function is restricted in many ways. */
extern void GTKBearoffProgress( int i ) {

    static GtkWidget *pwDialog, *pw, *pwAlign;
    gchar *gsz;
    
    if( !pwDialog ) {
	pwDialog = GTKCreateDialog( _("GNU Backgammon"), DT_INFO, NULL, NULL );
	gtk_window_set_role( GTK_WINDOW( pwDialog ), "progress" );
	gtk_window_set_type_hint( GTK_WINDOW(pwDialog),
			      GDK_WINDOW_TYPE_HINT_DIALOG );
	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			    GTK_SIGNAL_FUNC( GTKBearoffProgressCancel ),
			    NULL );

	gtk_box_pack_start( GTK_BOX( DialogArea( pwDialog, DA_MAIN ) ),
			    pwAlign = gtk_alignment_new( 0.5, 0.5, 1, 0 ),
			    TRUE, TRUE, 8 );
	gtk_container_add( GTK_CONTAINER( pwAlign ),
			   pw = gtk_progress_bar_new() );
	
	gtk_widget_show_all( pwDialog );
    }

    gsz = g_strdup_printf("Generating bearoff database (%.0f %%)", i / 542.64);
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR( pw ), gsz );
    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( pw ), i / 54264.0 );
    g_free(gsz);

    if( i >= 54000 ) {
	gtk_signal_disconnect_by_func(
	    GTK_OBJECT( pwDialog ),
	    GTK_SIGNAL_FUNC( GTKBearoffProgressCancel ), NULL );

	gtk_widget_destroy( pwDialog );
    }

    while( gtk_events_pending() )
	gtk_main_iteration();
}

static void enable_menu( GtkWidget *pw, int f );

static void enable_sub_menu( GtkWidget *pw, int f ) {

    GtkMenuShell *pms = GTK_MENU_SHELL( pw );

    g_list_foreach( pms->children, (GFunc) enable_menu, (gpointer) f );
}

static void enable_menu( GtkWidget *pw, int f ) {

    GtkMenuItem *pmi = GTK_MENU_ITEM( pw );

    if( pmi->submenu )
	enable_sub_menu( pmi->submenu, f );
    else
	gtk_widget_set_sensitive( pw, f );
}

/* A global setting has changed; update entry in Settings menu if necessary. */
extern void GTKSet( void *p ) {
	
    BoardData *bd = BOARD( pwBoard )->board_data;

    if( p == ap ) {
	/* Handle the player names. */
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_0 )
	    )->child ), (ap[ 0 ].szName) );
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_1 )
	    )->child ), (ap[ 1 ].szName) );

    GL_SetNames();

	GTKRegenerateGames();
    } else if( p == &ms.fTurn ) {
	/* Handle the player on roll. */
	fAutoCommand = TRUE;
	
	if( ms.fTurn >= 0 )
	    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_0 +
						       ms.fTurn ) ), TRUE );

        enable_menu ( gtk_item_factory_get_widget ( pif, "/Game/Roll" ),
                      ms.fMove == ms.fTurn && 
                      ap[ ms.fMove ].pt == PLAYER_HUMAN );

	fAutoCommand = FALSE;
    } else if( p == &ms.gs ) {
	/* Handle the game state. */
	fAutoCommand = TRUE;

	board_set_playing( BOARD( pwBoard ), ms.gs == GAME_PLAYING );
        ToolbarSetPlaying( pwToolbar, ms.gs == GAME_PLAYING );

	gtk_widget_set_sensitive( gtk_item_factory_get_widget( pif,
				"/File/Save..." ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget( pif,
	                        "/File/Export..." ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
				      pif, "/File/Generate HTML Images..." ),
				  TRUE );
	
	enable_sub_menu( gtk_item_factory_get_widget( pif, "/Game" ),
			 ms.gs == GAME_PLAYING );

        enable_menu ( gtk_item_factory_get_widget ( pif, "/Game/Roll" ),
                      ms.fMove == ms.fTurn && 
                      ap[ ms.fMove ].pt == PLAYER_HUMAN );

	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_ROLL ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_ROLL ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_ROLLED ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_ROLLED ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_MARKED ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_MARKED ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_GAME ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_GAME ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
				      pif, "/Game/Match information..." ),
				  TRUE );
	
	enable_sub_menu( gtk_item_factory_get_widget( pif, "/Analyse" ),
			 ms.gs == GAME_PLAYING );

	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
            pif, CMD_ANALYSE_MOVE ), 
            plLastMove && plLastMove->plNext && plLastMove->plNext->p );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_GAME ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_MATCH ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_SESSION ), !ListEmpty( &lMatch ) );

	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
            pif, CMD_ANALYSE_CLEAR_MOVE ), 
            plLastMove && plLastMove->plNext && plLastMove->plNext->p );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_CLEAR_GAME ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_CLEAR_MATCH ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_ANALYSE_CLEAR_SESSION ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_STATISTICS_GAME ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_STATISTICS_MATCH ), !ListEmpty ( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_STATISTICS_SESSION ), !ListEmpty ( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_RECORD_SHOW ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_RECORD_ADD_GAME ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_RECORD_ADD_MATCH ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_RECORD_ADD_SESSION ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_MATCHEQUITYTABLE ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_CALIBRATION ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_ENGINE ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SWAP_PLAYERS ), !ListEmpty( &lMatch ) );

       gtk_widget_set_sensitive( 
          gtk_item_factory_get_widget( pif,
                                       "/Analyse/"
                                       "Relational database/Test" ), 
          TRUE );
       gtk_widget_set_sensitive( 
          gtk_item_factory_get_widget( pif,
                                       "/Analyse/"
                                       "Relational database/Help" ), 
          TRUE );

#if USE_PYTHON	
    gtk_widget_set_sensitive( 
       gtk_item_factory_get_widget( pif,
                                    "/Analyse/Relational database/"
                                    "Add match or session" ), 
       !ListEmpty( &lMatch ) );

    gtk_widget_set_sensitive( 
          gtk_item_factory_get_widget( pif,
                                       "/Analyse/"
                                       "Relational database/Show Records" ), 
          TRUE );
    gtk_widget_set_sensitive( 
          gtk_item_factory_get_widget( pif,
                                       "/Analyse/"
                                       "Relational database/Manage Environments" ), 
          TRUE );
#endif /* USE_PYTHON */

	fAutoCommand = FALSE;
    } else if( p == &ms.fCrawford )
	ShowBoard(); /* this is overkill, but it works */
    else if (IsPanelShowVar(WINDOW_ANNOTATION, p)) {
	if (PanelShowing(WINDOW_ANNOTATION))
		ShowHidePanel(WINDOW_ANNOTATION);
    } else if (IsPanelShowVar(WINDOW_GAME, p)) {
		ShowHidePanel(WINDOW_GAME);
    } else if (IsPanelShowVar(WINDOW_ANALYSIS, p)) {
		ShowHidePanel(WINDOW_ANALYSIS);
    } else if (IsPanelShowVar(WINDOW_MESSAGE, p)) {
		ShowHidePanel(WINDOW_MESSAGE);
    } else if (IsPanelShowVar(WINDOW_THEORY, p)) {
		ShowHidePanel(WINDOW_THEORY);
    } else if (IsPanelShowVar(WINDOW_COMMAND, p)) {
		ShowHidePanel(WINDOW_COMMAND);
	} else if( p == &bd->rd->fDiceArea ) {
	if( GTK_WIDGET_REALIZED( pwBoard ) )
	{
#if USE_BOARD3D
		/* If in 3d mode may need to update sizes */
		if (bd->rd->fDisplayType == DT_3D)
			SetupViewingVolume3d(bd, &bd->bd3d, bd->rd);
		else
#endif
		{    
			if( GTK_WIDGET_REALIZED( pwBoard ) ) {
			    if( GTK_WIDGET_VISIBLE( bd->dice_area ) && !bd->rd->fDiceArea )
				gtk_widget_hide( bd->dice_area );
			    else if( ! GTK_WIDGET_VISIBLE( bd->dice_area ) && bd->rd->fDiceArea )
				gtk_widget_show_all( bd->dice_area );
			}
		}}
    } else if( p == &bd->rd->fShowIDs ) {

	if( GTK_WIDGET_REALIZED( pwBoard ) ) {
	    if( GTK_WIDGET_VISIBLE( bd->vbox_ids ) && !bd->rd->fShowIDs )
		gtk_widget_hide( bd->vbox_ids );
	    else if( !GTK_WIDGET_VISIBLE( bd->vbox_ids ) && bd->rd->fShowIDs )
		gtk_widget_show_all( bd->vbox_ids );
		gtk_widget_queue_resize(pwBoard);
	}
    } else if( p == &fGUIShowPips )
	ShowBoard(); /* this is overkill, but it works */
	else if (p == &fOutputWinPC)
	{
		MoveListRefreshSize();
	}
	else if (p == &showMoveListDetail)
	{
		if (pwMoveAnalysis && pwDetails)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pwDetails), showMoveListDetail);
	}
}


/* Match stats variables */
#define FORMATGS_ALL -1
#define NUM_STAT_TYPES 4
char *aszStatHeading [ NUM_STAT_TYPES ] = {
  N_("Chequer Play Statistics:"), 
  N_("Cube Statistics:"),
  N_("Luck Statistics:"),
  N_("Overall Statistics:")};
static GtkWidget* statLists[NUM_STAT_TYPES], *pwList;
static int numStatGames, curStatGame;
static GtkWidget* statPom;
GtkWidget *pwStatDialog;
int fGUIUseStatsPanel = TRUE;
GtkWidget *pwUsePanels;
GtkWidget *pswList;
GtkWidget *pwNotebook;

static gboolean ContextCopyMenu(GtkWidget *widget, GdkEventButton *event, GtkWidget* copyMenu)
{
	if (event->type != GDK_BUTTON_PRESS || event->button != 3)
		return FALSE;

	gtk_menu_popup (GTK_MENU (copyMenu), NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static void AddList(char* pStr, GtkCList* pList, const char* pTitle)
{
	int i;
	gchar *sz;

	sprintf ( strchr ( pStr, 0 ), "%s\n", pTitle);

	for (i = 0; i < pList->rows; i++ )
	{
		sprintf ( strchr ( pStr, 0 ), "%-37s ", 
			  ( gtk_clist_get_text ( pList, i, 0, &sz ) ) ?
			  sz : "" );

		sprintf ( strchr ( pStr, 0 ), "%-20s ", 
			  ( gtk_clist_get_text ( pList, i, 1, &sz ) ) ?
			  sz : "" );

		sprintf ( strchr ( pStr, 0 ), "%-20s\n", 
			  ( gtk_clist_get_text ( pList, i, 2, &sz ) ) ?
			  sz : "" );
	}
	sprintf ( strchr ( pStr, 0 ), "\n");
}

static void CopyData(GtkWidget *pwNotebook, enum _formatgs page)
{
	char szOutput[4096];

	sprintf(szOutput, "%-37s %-20s %-20s\n", "", ap[ 0 ].szName, ap[ 1 ].szName);

	if (page == FORMATGS_CHEQUER || page == FORMATGS_ALL)
		AddList(szOutput, GTK_CLIST(statLists[FORMATGS_CHEQUER]), aszStatHeading[FORMATGS_CHEQUER]);
	if (page == FORMATGS_LUCK || page == FORMATGS_ALL)
		AddList(szOutput, GTK_CLIST(statLists[FORMATGS_LUCK]), aszStatHeading[FORMATGS_LUCK]);
	if (page == FORMATGS_CUBE || page == FORMATGS_ALL)
		AddList(szOutput, GTK_CLIST(statLists[FORMATGS_CUBE]), aszStatHeading[FORMATGS_CUBE]);
	if (page == FORMATGS_OVERALL || page == FORMATGS_ALL)
		AddList(szOutput, GTK_CLIST(statLists[FORMATGS_OVERALL]), aszStatHeading[FORMATGS_OVERALL]);

	TextToClipboard(szOutput);
}

static void CopyPage( GtkWidget *pwWidget, GtkWidget *pwNotebook )
{
	switch(gtk_notebook_get_current_page(GTK_NOTEBOOK(pwNotebook)))
	{
	case 0:
		CopyData(pwNotebook, FORMATGS_OVERALL);
		break;
	case 1:
		CopyData(pwNotebook, FORMATGS_CHEQUER);
		break;
	case 2:
		CopyData(pwNotebook, FORMATGS_CUBE);
		break;
	case 3:
		CopyData(pwNotebook, FORMATGS_LUCK);
		break;
	}
}

static void CopyAll( GtkWidget *pwWidget, GtkWidget *pwNotebook )
{
	CopyData(pwNotebook, FORMATGS_ALL);
}

static void FillStats(const statcontext *psc, const matchstate *pms,
                      const enum _formatgs gs, GtkWidget* statList )
{

  int fIsMatch = (curStatGame == 0);
  GList *list = formatGS( psc, pms, fIsMatch, gs );
  GList *pl;
  
  for ( pl = g_list_first( list ); pl; pl = g_list_next( pl ) ) {
    
    char **aasz = pl->data;
    
    gtk_clist_append( GTK_CLIST( statList ), aasz );
    
  }
  
  freeGS( list );
  
}

static void SetStats(const statcontext *psc)
{
    char *aszLine[] = { NULL, NULL, NULL };
	int i;

	for ( i = 0; i < NUM_STAT_TYPES; ++i )
		gtk_clist_clear(GTK_CLIST(statLists[i]));

	for ( i = 0; i < NUM_STAT_TYPES; ++i )
		FillStats( psc, &ms, i, statLists[ i ] );

	gtk_clist_clear(GTK_CLIST(pwList));

	aszLine[0] = aszStatHeading[FORMATGS_CHEQUER];
	gtk_clist_append( GTK_CLIST( pwList ), aszLine );
	FillStats( psc, &ms, FORMATGS_CHEQUER, pwList );
	FillStats( psc, &ms, FORMATGS_LUCK, pwList );
	
	aszLine[0] = aszStatHeading[FORMATGS_CUBE];
	gtk_clist_append( GTK_CLIST( pwList ), aszLine );
	FillStats( psc, &ms, FORMATGS_CUBE, pwList );
	
	aszLine[0] = aszStatHeading[FORMATGS_OVERALL];
	gtk_clist_append( GTK_CLIST( pwList ), aszLine );
	FillStats( psc, &ms, FORMATGS_OVERALL, pwList );
}

const statcontext *GetStatContext(int game)
{
	xmovegameinfo *pmgi;
	int i;

	if (!game)
		return &scMatch;
	else
	{
		list *plGame, *pl = lMatch.plNext;
		for (i = 1; i < game; i++)
			pl = pl->plNext;

		plGame = pl->p;
                pmgi = &((moverecord *) plGame->plNext->p)->g;
		return &pmgi->sc;
	}
}

static void StatsSelectGame(GtkWidget *pw, int i)
{
	curStatGame = i;

    gtk_option_menu_set_history( GTK_OPTION_MENU( statPom ), curStatGame );

	if (!curStatGame)
	{
		gtk_window_set_title(GTK_WINDOW(pwStatDialog), _("Statistics for all games"));
	}
	else
	{
		char sz[100];
		strcpy(sz, _("Statistics for game "));
		sprintf(sz + strlen(sz), "%d", curStatGame);
		gtk_window_set_title(GTK_WINDOW(pwStatDialog), sz);
	}
	SetStats(GetStatContext(curStatGame));
}

static void StatsAllGames( GtkWidget *pw, char *szCommand )
{
	if (curStatGame != 0)
		StatsSelectGame(pw, 0);
}

static void StatsFirstGame( GtkWidget *pw, char *szCommand )
{
	if (curStatGame != 1)
		StatsSelectGame(pw, 1);
}

static void StatsLastGame( GtkWidget *pw, char *szCommand )
{
	if (curStatGame != numStatGames)
		StatsSelectGame(pw, numStatGames);
}

static void StatsPreviousGame( GtkWidget *pw, char *szCommand )
{
	if (curStatGame > 1)
		StatsSelectGame(pw, curStatGame - 1);
}

static void StatsNextGame( GtkWidget *pw, char *szCommand )
{
	if (curStatGame < numStatGames)
		StatsSelectGame(pw, curStatGame + 1);
}

extern GtkWidget *StatsPixmapButton(GdkColormap *pcmap, char **xpm,
				void (*fn)())
{
    GdkPixmap *ppm;
    GdkBitmap *pbm;
    GtkWidget *pw, *pwButton;

    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL, pcmap, &pbm, NULL,
						 xpm );
    pw = gtk_pixmap_new( ppm, pbm );
    pwButton = gtk_button_new();
    gtk_container_add( GTK_CONTAINER( pwButton ), pw );
    
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( fn ), 0 );
    
    return pwButton;
}

static void AddNavigation(GtkWidget* pvbox)
{
	GtkWidget *phbox, *pm, *pw;
	GdkColormap *pcmap;
    char sz[128];
    list *pl;

#include "xpm/prevgame.xpm"
#include "xpm/prevmove.xpm"
#include "xpm/nextmove.xpm"
#include "xpm/nextgame.xpm"
#include "xpm/allgames.xpm"

	pcmap = gtk_widget_get_colormap( pwMain );

	phbox = gtk_hbox_new( FALSE, 0 ),
	gtk_box_pack_start( GTK_BOX( pvbox ), phbox, FALSE, FALSE, 4 );

	gtk_box_pack_start( GTK_BOX( phbox ),
			pw = StatsPixmapButton(pcmap, allgames_xpm, StatsAllGames),
			FALSE, FALSE, 4 );
	gtk_tooltips_set_tip( ptt, pw, _("Show all games"), "" );
	gtk_box_pack_start( GTK_BOX( phbox ),
			pw = StatsPixmapButton(pcmap, prevgame_xpm, StatsFirstGame),
			FALSE, FALSE, 4 );
	gtk_tooltips_set_tip( ptt, pw, _("Move to first game"), "" );
	gtk_box_pack_start( GTK_BOX( phbox ),
			pw = StatsPixmapButton(pcmap, prevmove_xpm, StatsPreviousGame),
			FALSE, FALSE, 0 );
	gtk_tooltips_set_tip( ptt, pw, _("Move back to the previous game"), "" );
	gtk_box_pack_start( GTK_BOX( phbox ),
			pw = StatsPixmapButton(pcmap, nextmove_xpm, StatsNextGame),
			FALSE, FALSE, 4 );
	gtk_tooltips_set_tip( ptt, pw, _("Move ahead to the next game"), "" );
	gtk_box_pack_start( GTK_BOX( phbox ),
			pw = StatsPixmapButton(pcmap, nextgame_xpm, StatsLastGame),
			FALSE, FALSE, 0 );
	gtk_tooltips_set_tip( ptt, pw, _("Move ahead to last game"), "" );

	pm = gtk_menu_new();

	{
		int anFinalScore[ 2 ];

		if ( getFinalScore( anFinalScore ) )
			sprintf( sz, _("All games: %s %d, %s %d"), ap[ 0 ].szName,
				 anFinalScore[ 0 ], ap[ 1 ].szName, anFinalScore[ 1 ] );
		else
			sprintf( sz, _("All games: %s, %s"), ap[ 0 ].szName,
				 ap[ 1 ].szName );
	}

	pw = gtk_menu_item_new_with_label(sz);
	gtk_menu_append(GTK_MENU(pm), pw);
	gtk_signal_connect( GTK_OBJECT( pw ), "activate",
			GTK_SIGNAL_FUNC(StatsSelectGame), 0);

	numStatGames = 0;
	curStatGame = 0;
	for (pl = lMatch.plNext; pl->p; pl = pl->plNext )
	{
		list *plGame = pl->p;
		moverecord *pmr = plGame->plNext->p;
		numStatGames++;

		sprintf(sz, _("Game %d: %s %d, %s %d"), pmr->g.i + 1, ap[ 0 ].szName,
			 pmr->g.anScore[ 0 ], ap[ 1 ].szName, pmr->g.anScore[ 1 ] );
		pw = gtk_menu_item_new_with_label(sz);

		gtk_signal_connect( GTK_OBJECT( pw ), "activate",
				GTK_SIGNAL_FUNC(StatsSelectGame), GINT_TO_POINTER(numStatGames));

		gtk_widget_show( pw );
		gtk_menu_append( GTK_MENU( pm ), pw );
	}

    gtk_widget_show_all( pm );
    gtk_option_menu_set_menu( GTK_OPTION_MENU( statPom = gtk_option_menu_new() ), pm );
    gtk_option_menu_set_history( GTK_OPTION_MENU( statPom ), 0 );

    gtk_box_pack_start( GTK_BOX( phbox ), statPom, TRUE, TRUE, 4 );
}

void toggle_fGUIUseStatsPanel(GtkWidget *widget, GtkWidget *pw)
{
	fGUIUseStatsPanel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	if (fGUIUseStatsPanel)
	{
		gtk_widget_hide(pswList);
		gtk_widget_show(pwNotebook);
	}
	else
	{
		gtk_widget_hide(pwNotebook);
		gtk_widget_show(pswList);
	}
}

gint 
compare_func( gconstpointer a, gconstpointer b ) {

  gint i = GPOINTER_TO_INT( a );
  gint j = GPOINTER_TO_INT( b );

  if ( i < j )
    return -1;
  else if ( i == j )
    return 0;
  else
    return 1;

}

static void StatcontextCopy(GtkWidget *pw, GtkWidget *pwList)
{
  GList *pl;
  GList *plCopy;
  int i;
  static char szOutput[ 4096 ];
  char *pc;
  gchar *sz;

  sprintf ( szOutput, 
            "%-37s %-20s %-20s\n",
            "", ap[ 0 ].szName, ap[ 1 ].szName );

  /* copy list (note that the integers in the list are NOT copied) */
  plCopy = g_list_copy( GTK_CLIST ( pwList )->selection );

  /* sort list; otherwise the lines are returned in whatever order the
     user clicked the lines (bug #4160) */
  plCopy = g_list_sort( plCopy, compare_func );

  for ( pl = plCopy; pl; pl = pl->next ) {

    i = GPOINTER_TO_INT( pl->data );

    sprintf ( pc = strchr ( szOutput, 0 ), "%-37s ", 
              ( gtk_clist_get_text ( GTK_CLIST ( pwList ), i, 0, &sz ) ) ?
              sz : "" );
      
    sprintf ( pc = strchr ( szOutput, 0 ), "%-20s ", 
              ( gtk_clist_get_text ( GTK_CLIST ( pwList ), i, 1, &sz ) ) ?
              sz : "" );
      
    sprintf ( pc = strchr ( szOutput, 0 ), "%-20s\n", 
              ( gtk_clist_get_text ( GTK_CLIST ( pwList ), i, 2, &sz ) ) ?
              sz : "" );
      
  }

  /* garbage collect */
  g_list_free(plCopy);
  
  GTKTextToClipboard(szOutput);
}


static GtkWidget *CreateList()
{
	int i;
        static char *aszEmpty[] = { NULL, NULL, NULL };
	GtkWidget *pwList = gtk_clist_new_with_titles( 3, aszEmpty );

	for( i = 0; i < 3; i++ ) {
          gtk_clist_set_column_auto_resize( GTK_CLIST( pwList ), i, TRUE );
          gtk_clist_set_column_justification( GTK_CLIST( pwList ), 
                                              i, GTK_JUSTIFY_RIGHT );
	}

	gtk_clist_column_titles_passive( GTK_CLIST( pwList ) );
  
	gtk_clist_set_column_title( GTK_CLIST( pwList ), 1, (ap[0].szName));
	gtk_clist_set_column_title( GTK_CLIST( pwList ), 2, (ap[1].szName));

	gtk_clist_set_selection_mode( GTK_CLIST( pwList ), 
                                      GTK_SELECTION_EXTENDED );

	return pwList;
}


extern void GTKDumpStatcontext( int game )
{
	GtkWidget *copyMenu, *menu_item, *pvbox;
#if USE_BOARD3D
	int i;
	GraphData gd;
	GtkWidget *pw;
	list *pl;
#endif
	pwStatDialog = GTKCreateDialog( "", DT_INFO, NULL, NULL );

	pwNotebook = gtk_notebook_new();
	gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
	gtk_notebook_popup_disable( GTK_NOTEBOOK( pwNotebook ) );

/* Not sure if this is a good idea...
	gtk_tooltips_set_tip( ptt, pwNotebook, _("Right click to copy statistics"), "" );
*/

	pvbox = gtk_vbox_new( FALSE, 0 ),
    gtk_box_pack_start( GTK_BOX( pvbox ), pwNotebook, TRUE, TRUE, 0);

	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), statLists[FORMATGS_OVERALL] = CreateList(),
					  gtk_label_new(_("Overall")));

	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), statLists[FORMATGS_CHEQUER] = CreateList(),
					  gtk_label_new(_("Chequer play")));

	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), statLists[FORMATGS_CUBE] = CreateList(),
					  gtk_label_new(_("Cube decisions")));

	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), statLists[FORMATGS_LUCK] = CreateList(),
					  gtk_label_new(_("Luck")));

	pwList = CreateList();

	pswList = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pswList ),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_container_add( GTK_CONTAINER( pswList ), pwList );

	gtk_box_pack_start (GTK_BOX (pvbox), pswList, TRUE, TRUE, 0);

	AddNavigation(pvbox);
	gtk_container_add( GTK_CONTAINER( DialogArea( pwStatDialog, DA_MAIN ) ), pvbox );

#if USE_BOARD3D
	SetNumGames(&gd, numStatGames);

	pl = lMatch.plNext;
	for (i = 0; i < numStatGames; i++)
	{
		list *plGame = pl->p;
		moverecord *mr = plGame->plNext->p;
		xmovegameinfo *pmgi = &mr->g;
		AddGameData(&gd, i, &pmgi->sc);
		pl = pl->plNext;
	}
	/* Total values */
	AddGameData(&gd, i, &scMatch);

	pw = StatGraph(&gd);
	gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ), pw,
					  gtk_label_new(_("Graph")));
    gtk_tooltips_set_tip( ptt, pw, _("This graph shows the total error rates per game for each player."
		" The games are along the bottom and the error rates up the side."
		" Chequer error in green, cube error in blue."), "" );
#endif

	pwUsePanels = gtk_check_button_new_with_label(_("Split statistics into panels"));
	gtk_tooltips_set_tip(ptt, pwUsePanels, "Show data in a single list or split other several panels", 0);
	gtk_box_pack_start (GTK_BOX (pvbox), pwUsePanels, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pwUsePanels), fGUIUseStatsPanel);
	gtk_signal_connect(GTK_OBJECT(pwUsePanels), "toggled", GTK_SIGNAL_FUNC(toggle_fGUIUseStatsPanel), NULL);

	/* list view (selections) */
	copyMenu = gtk_menu_new ();

	menu_item = gtk_menu_item_new_with_label ("Copy selection");
	gtk_menu_shell_append (GTK_MENU_SHELL (copyMenu), menu_item);
	gtk_widget_show (menu_item);
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( StatcontextCopy ), pwList );

	menu_item = gtk_menu_item_new_with_label ("Copy all");
	gtk_menu_shell_append (GTK_MENU_SHELL (copyMenu), menu_item);
	gtk_widget_show (menu_item);
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( CopyAll ), pwNotebook );

	gtk_signal_connect( GTK_OBJECT( pwList ), "button-press-event", GTK_SIGNAL_FUNC( ContextCopyMenu ), copyMenu );

	/* modality */
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwUsePanels ) ) )
	    gtk_window_set_default_size( GTK_WINDOW( pwStatDialog ), 0, 300 );
	else {
	    GtkRequisition req;
	    gtk_widget_size_request( GTK_WIDGET( pwStatDialog ), &req );
	    if ( req.height < 600 )
		gtk_window_set_default_size( GTK_WINDOW( pwStatDialog ), 0, 600 );
	}
	gtk_window_set_modal( GTK_WINDOW( pwStatDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwStatDialog ),
								GTK_WINDOW( pwMain ) );

	copyMenu = gtk_menu_new ();

	menu_item = gtk_menu_item_new_with_label ("Copy page");
	gtk_menu_shell_append (GTK_MENU_SHELL (copyMenu), menu_item);
	gtk_widget_show (menu_item);
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( CopyPage ), pwNotebook );

	menu_item = gtk_menu_item_new_with_label ("Copy all pages");
	gtk_menu_shell_append (GTK_MENU_SHELL (copyMenu), menu_item);
	gtk_widget_show (menu_item);
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( CopyAll ), pwNotebook );

	gtk_signal_connect( GTK_OBJECT( pwNotebook ), "button-press-event", GTK_SIGNAL_FUNC( ContextCopyMenu ), copyMenu );

	gtk_signal_connect( GTK_OBJECT( pwStatDialog ), "destroy",
					  GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_widget_show_all( pwStatDialog );

	StatsSelectGame(0, game);
	toggle_fGUIUseStatsPanel(pwUsePanels, 0);

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();

#if USE_BOARD3D
	TidyGraphData(&gd);
#endif
}

static void SetOptions( gpointer *p, guint n, GtkWidget *pw ) {

  GTKSetOptions();

}

extern int
GTKGetMove ( int anMove[ 8 ] ) {

  BoardData *bd = BOARD ( pwBoard )->board_data;

  if ( !bd->valid_move )
    return 0;
  
  memcpy ( anMove, bd->valid_move->anMove, 8 * sizeof ( int ) );

  return 1;

}

typedef struct _recordwindowinfo {
    GtkWidget *pwList, *pwTable, *pwErase, *apwStats[ 22 ];
    int nRow;
} recordwindowinfo;

static void RecordSelect( GtkCList *pw, gint nRow, gint nCol,
			  GdkEventButton *pev, recordwindowinfo *prwi ) {
    char *pch;
    int i;
    
    gtk_widget_set_sensitive( prwi->pwErase, TRUE );

    for( i = 0; i < 22; i++ ) {
	gtk_clist_get_text( pw, nRow, i, &pch );
	gtk_label_set_text( GTK_LABEL( prwi->apwStats[ i ] ), pch );
    }

    prwi->nRow = nRow;
}

static void RecordUnselect( GtkCList *pw, gint nRow, gint nCol,
			    GdkEventButton *pev, recordwindowinfo *prwi ) {
    int i;
    
    gtk_widget_set_sensitive( prwi->pwErase, FALSE );

    for( i = 0; i < 22; i++ )
	gtk_label_set_text( GTK_LABEL( prwi->apwStats[ i ] ), NULL );
}

static void RecordErase( GtkWidget *pw, recordwindowinfo *prwi ) {

    char *pch;
    char sz[ 64 ];
    
    gtk_clist_get_text( GTK_CLIST( prwi->pwList ), prwi->nRow, 0, &pch );
    sprintf( sz, "record erase \"%s\"", pch );
    UserCommand( sz );
    gtk_clist_remove( GTK_CLIST( prwi->pwList ), prwi->nRow );
}

static void RecordEraseAll( GtkWidget *pw, recordwindowinfo *prwi ) {

    FILE *pf;
    char *sz = g_alloca( strlen( szHomeDirectory ) + 10 );
    
    UserCommand( "record eraseall" );

    /* FIXME this is a horrible hack to determine whether the records were
       really erased */
    
    sprintf( sz, "%s/.gnubgpr", szHomeDirectory );
    if( ( pf = fopen( sz, "r" ) ) ) {
	fclose( pf );
	return;
    }

    gtk_clist_clear( GTK_CLIST( prwi->pwList ) );
}

static gint RecordRowCompare( GtkCList *pcl, GtkCListRow *p0,
			      GtkCListRow *p1 ) {

    return strcasecmp( GTK_CELL_TEXT( p0->cell[ pcl->sort_column ] )->text,
		       GTK_CELL_TEXT( p1->cell[ pcl->sort_column ] )->text );
}

/* 0: name [visible]
   1: chequer (20)
   2: cube (20)
   3: combined (20)
   4: chequer (100)
   5: cube (100)
   6: combined (100)
   7: chequer (500)
   8: cube (500)
   9: combined (500)
   10: chequer (total) [visible]
   11: cube (total) [visible]
   12: combined (total) [visible]
   13: luck (20)
   14: luck (100)
   15: luck (500)
   16: luck (total) [visible]
   17: games [visible]
   18: chequer rating
   19: cube rating
   20: combined rating
   21: luck rating */

extern void GTKRecordShow( FILE *pfIn, char *szFile, char *szPlayer ) {

	GtkWidget *pw = NULL, *pwList = NULL, *pwScrolled, *pwHbox, *pwVbox,
	*pwEraseAll;
    static int ay[ 22 ] = { 0, 3, 5, 7, 3, 5, 7, 3, 5, 7, 3, 5, 7, 9, 9, 9, 9,
			    1, 4, 6, 8, 10 };
    static int ax[ 22 ] = { 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 1, 2, 3, 4,
			    1, 1, 1, 1, 1 };
    static int axEnd[ 22 ] = { 5, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 2, 3, 4,
			       5, 5, 5, 5, 5, 5 };
    char *asz[ 22 ], sz[ 16 ];
    int i, f = FALSE;
    playerrecord pr;
    recordwindowinfo rwi;
    
    while( !RecordReadItem( pfIn, szFile, &pr ) ) {
	if( !f ) {
	    f = TRUE;
	    pw = GTKCreateDialog( _("GNU Backgammon - Player records"), DT_INFO,
			       NULL, NULL );
	    
	    for( i = 0; i < 22; i++ )
		asz[ i ] = "";
	    
	    rwi.pwList = pwList = gtk_clist_new_with_titles( 22, asz );
	    gtk_clist_set_compare_func(
		GTK_CLIST( pwList ), (GtkCListCompareFunc) RecordRowCompare );
	    
	    for( i = 0; i < 22; i++ ) {
		gtk_clist_set_column_auto_resize( GTK_CLIST( pwList ), i,
						  TRUE );
		gtk_clist_set_column_justification( GTK_CLIST( pwList ), i,
						    i ? GTK_JUSTIFY_RIGHT :
						    GTK_JUSTIFY_LEFT );
		gtk_clist_set_column_visibility( GTK_CLIST( pwList ), i,
						 FALSE );
	    }

	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 0, _("Name") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 0, TRUE );
	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 10,
					_("Chequer") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 10, TRUE );
	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 11, _("Cube") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 11, TRUE );
	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 12,
					_("Overall") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 12, TRUE );
	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 16, _("Luck") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 16, TRUE );
	    gtk_clist_set_column_title( GTK_CLIST( pwList ), 17, _("Games") );
	    gtk_clist_set_column_visibility( GTK_CLIST( pwList ), 17, TRUE );
	    
	    gtk_clist_column_titles_passive( GTK_CLIST( pwList ) );
	    gtk_signal_connect( GTK_OBJECT( pwList ), "select-row",
			GTK_SIGNAL_FUNC( RecordSelect ), &rwi );
	    gtk_signal_connect( GTK_OBJECT( pwList ), "unselect-row",
			GTK_SIGNAL_FUNC( RecordUnselect ), &rwi );
	    pwScrolled = gtk_scrolled_window_new( NULL, NULL );
	    pwHbox = gtk_hbox_new( FALSE, 0 );
	    pwVbox = gtk_vbox_new( FALSE, 0 );
	    gtk_container_add( GTK_CONTAINER( DialogArea( pw, DA_MAIN ) ),
			       pwHbox );
	    gtk_container_add( GTK_CONTAINER( pwHbox ), pwScrolled );
	    gtk_container_add( GTK_CONTAINER( pwScrolled ), pwList );
	    gtk_container_add( GTK_CONTAINER( pwHbox ), pwVbox );
	    rwi.pwTable = gtk_table_new( 11, 5, TRUE );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Name:") ), 0, 1,
				       0, 1 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Games:") ), 0, 1,
				       1, 2 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Chequer:") ), 0, 1,
				       3, 5 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Cube:") ), 0, 1,
				       5, 7 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Overall:") ), 0, 1,
				       7, 9 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Luck:") ), 0, 1,
				       9, 11 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("20") ), 1, 2,
				       2, 3 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("100") ), 2, 3,
				       2, 3 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("500") ), 3, 4,
				       2, 3 );
	    gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
				       gtk_label_new( _("Total") ), 4, 5,
				       2, 3 );
	    for( i = 0; i < 22; i++ )
		gtk_table_attach_defaults( GTK_TABLE( rwi.pwTable ),
					   rwi.apwStats[ i ] =
					   gtk_label_new( NULL ),
					   ax[ i ], axEnd[ i ],
					   ay[ i ], ay[ i ] + 1 );
	    gtk_container_add( GTK_CONTAINER( pwVbox ), rwi.pwTable );
	    rwi.pwErase = gtk_button_new_with_label( _("Erase" ) );
	    gtk_signal_connect( GTK_OBJECT( rwi.pwErase ), "clicked",
				GTK_SIGNAL_FUNC( RecordErase ), &rwi );
	    gtk_box_pack_start( GTK_BOX( pwVbox ), rwi.pwErase, FALSE, FALSE,
				0 );
	    pwEraseAll = gtk_button_new_with_label( _("Erase All" ) );
	    gtk_signal_connect( GTK_OBJECT( pwEraseAll ), "clicked",
			GTK_SIGNAL_FUNC( RecordEraseAll ), &rwi );
	    gtk_box_pack_start( GTK_BOX( pwVbox ), pwEraseAll, FALSE, FALSE,
				0 );
	}

	i = gtk_clist_append( GTK_CLIST( pwList ), asz );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 0, pr.szName );
	
	if( pr.cGames >= 20 )
	    sprintf( sz, "%.4f", -pr.arErrorChequerplay[ EXPAVG_20 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 1, sz );
	
	if( pr.cGames >= 20 )
	    sprintf( sz, "%.4f", -pr.arErrorCube[ EXPAVG_20 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 2, sz );
	
	if( pr.cGames >= 20 )
	    sprintf( sz, "%.4f", -pr.arErrorCombined[ EXPAVG_20 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 3, sz );
	
	if( pr.cGames >= 100 )
	    sprintf( sz, "%.4f", -pr.arErrorChequerplay[ EXPAVG_100 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 4, sz );
	
	if( pr.cGames >= 100 )
	    sprintf( sz, "%.4f", -pr.arErrorCube[ EXPAVG_100 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 5, sz );
	
	if( pr.cGames >= 100 )
	    sprintf( sz, "%.4f", -pr.arErrorCombined[ EXPAVG_100 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 6, sz );
	
	if( pr.cGames >= 500 )
	    sprintf( sz, "%.4f", -pr.arErrorChequerplay[ EXPAVG_500 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 7, sz );
	
	if( pr.cGames >= 500 )
	    sprintf( sz, "%.4f", -pr.arErrorCube[ EXPAVG_500 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 8, sz );
	
	if( pr.cGames >= 500 )
	    sprintf( sz, "%.4f", -pr.arErrorCombined[ EXPAVG_500 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 9, sz );
	
	sprintf( sz, "%.4f", -pr.arErrorChequerplay[ EXPAVG_TOTAL ] );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 10, sz );
	
	sprintf( sz, "%.4f", -pr.arErrorCube[ EXPAVG_TOTAL ] );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 11, sz );
	
	sprintf( sz, "%.4f", -pr.arErrorCombined[ EXPAVG_TOTAL ] );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 12, sz );
	
	if( pr.cGames >= 20 )
	    sprintf( sz, "%.4f", pr.arLuck[ EXPAVG_20 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 13, sz );
	
	if( pr.cGames >= 100 )
	    sprintf( sz, "%.4f", pr.arLuck[ EXPAVG_100 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 14, sz );
	
	if( pr.cGames >= 500 )
	    sprintf( sz, "%.4f", pr.arLuck[ EXPAVG_500 ] );
	else
	    strcpy( sz, _("n/a") );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 15, sz );
	
	sprintf( sz, "%.4f", pr.arLuck[ EXPAVG_TOTAL ] );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 16, sz );

	sprintf( sz, "%d", pr.cGames );
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 17, sz );

	gtk_clist_set_text( GTK_CLIST( pwList ), i, 18,
			    gettext ( aszRating[ GetRating( pr.arErrorChequerplay[
				EXPAVG_TOTAL ] ) ] ) );
	
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 19,
			    gettext ( aszRating[ GetRating( pr.arErrorCube[
				EXPAVG_TOTAL ] ) ] ) );
	
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 20,
			    gettext ( aszRating[ GetRating( pr.arErrorCombined[
				EXPAVG_TOTAL ] ) ] ) );
	
	gtk_clist_set_text( GTK_CLIST( pwList ), i, 21,
			    gettext ( aszLuckRating[ getLuckRating( pr.arLuck[
				EXPAVG_TOTAL ] / 20 ) ] ) );
	
	if( !CompareNames( pr.szName, szPlayer ) )
	    gtk_clist_select_row( GTK_CLIST( pwList ), i, 0 );
    }

    if( ferror( pfIn ) )
	outputerr( szFile );
    else if( !f )
	outputl( _("No player records found.") );

    fclose( pfIn );    

    if( f ) {
	gtk_clist_sort( GTK_CLIST( pwList ) );
	
	gtk_window_set_default_size( GTK_WINDOW( pw ), 600, 400 );
	gtk_window_set_modal( GTK_WINDOW( pw ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pw ), GTK_WINDOW( pwMain ) );

	gtk_widget_show_all( pw );
	
	gtk_signal_connect( GTK_OBJECT( pw ), "destroy",
			    GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
	
	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
    }
}

static void UpdateMatchinfo( const char *pch, char *szParam, char **ppch ) {

    char *szCommand, *pchOld = *ppch ? *ppch : "";
    
    if( !strcmp( pch, pchOld ) )
	/* no change */
	return;

    szCommand = g_strdup_printf( "set matchinfo %s %s", szParam, pch );
    UserCommand( szCommand );
    g_free( szCommand );
}

static void MatchInfoOK( GtkWidget *pw, int *pf ) {

    *pf = TRUE;
    gtk_main_quit();
}

static void AddToTable(GtkWidget* pwTable, char* str, int x, int y)
{
	GtkWidget* pw = gtk_label_new(str);
	/* Right align */
	gtk_misc_set_alignment(GTK_MISC(pw), 1, .5);
	gtk_table_attach(GTK_TABLE(pwTable), pw, x, x + 1, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
}

extern void GTKMatchInfo( void ) {

    int fOK = FALSE;
    GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Match "
					  "information"),
					DT_QUESTION,
					GTK_SIGNAL_FUNC( MatchInfoOK ), &fOK ),
	*pwTable, *apwRating[ 2 ], *pwDate, *pwEvent, *pwRound, *pwPlace,
	*pwAnnotator, *pwComment;
    char sz[ 128 ], *pch;
    gulong id;
    GtkTextBuffer *buffer;
    GtkTextIter begin, end;
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    id = gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			     GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    pwTable = gtk_table_new( 5, 7, FALSE );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwTable );

    sprintf( sz, _("%s's rating:"), ap[ 0 ].szName );
	AddToTable(pwTable, sz, 0, 0);

    sprintf( sz, _("%s's rating:"), ap[ 1 ].szName );
	AddToTable(pwTable, sz, 0, 1);

	AddToTable(pwTable, _("Date:"), 0, 2);
	AddToTable(pwTable, _("Event:"), 0, 3);
	AddToTable(pwTable, _("Round:"), 0, 4);
	AddToTable(pwTable, _("Place:"), 0, 5);
	AddToTable(pwTable, _("Annotator:"), 0, 6);

    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Comments:") ),
		      2, 3, 0, 1, 0, 0, 0, 0 );

    apwRating[ 0 ] = gtk_entry_new();
    if( mi.pchRating[ 0 ] )
	gtk_entry_set_text( GTK_ENTRY( apwRating[ 0 ] ), mi.pchRating[ 0 ] );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), apwRating[ 0 ], 1, 2,
			       0, 1 );
    
    apwRating[ 1 ] = gtk_entry_new();
    if( mi.pchRating[ 1 ] )
	gtk_entry_set_text( GTK_ENTRY( apwRating[ 1 ] ), mi.pchRating[ 1 ] );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), apwRating[ 1 ], 1, 2,
			       1, 2 );
    
    pwDate = gtk_calendar_new();
    if( mi.nYear ) {
	gtk_calendar_select_month( GTK_CALENDAR( pwDate ), mi.nMonth - 1,
				   mi.nYear );
	gtk_calendar_select_day( GTK_CALENDAR( pwDate ), mi.nDay );
    } else
	gtk_calendar_select_day( GTK_CALENDAR( pwDate ), 0 );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwDate, 1, 2, 2, 3 );

    pwEvent = gtk_entry_new();
    if( mi.pchEvent )
	gtk_entry_set_text( GTK_ENTRY( pwEvent ), mi.pchEvent );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwEvent, 1, 2, 3, 4 );
    
    pwRound = gtk_entry_new();
    if( mi.pchRound )
	gtk_entry_set_text( GTK_ENTRY( pwRound ), mi.pchRound );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwRound, 1, 2, 4, 5 );
    
    pwPlace = gtk_entry_new();
    if( mi.pchPlace )
	gtk_entry_set_text( GTK_ENTRY( pwPlace ), mi.pchPlace );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwPlace, 1, 2, 5, 6 );
    
    pwAnnotator = gtk_entry_new();
    if( mi.pchAnnotator )
	gtk_entry_set_text( GTK_ENTRY( pwAnnotator ), mi.pchAnnotator );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwAnnotator, 1, 2, 6, 7 );
        
    pwComment = gtk_text_view_new() ;
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwComment));
    if( mi.pchComment ) {
	gtk_text_buffer_set_text(buffer, mi.pchComment, -1);
    }
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW( pwComment ), GTK_WRAP_WORD );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwComment, 2, 5, 1, 7 );

    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 500, 0 );
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK ) {
	unsigned int nYear, nMonth, nDay;
	
	gtk_signal_disconnect( GTK_OBJECT( pwDialog ), id );
	
	outputpostpone();

	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( apwRating[ 0 ] ) ),
			 "rating 0", &mi.pchRating[ 0 ] );
	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( apwRating[ 1 ] ) ),
			 "rating 1", &mi.pchRating[ 1 ] );
	
	gtk_calendar_get_date( GTK_CALENDAR( pwDate ), &nYear, &nMonth,
			       &nDay );
	nMonth++;
	if( mi.nYear && !nDay )
	    UserCommand( "set matchinfo date" );
	else if( nDay && ( !mi.nYear || mi.nYear != nYear ||
			   mi.nMonth != nMonth || mi.nDay != nDay ) ) {
	    char sz[ 64 ];
	    sprintf( sz, "set matchinfo date %04d-%02d-%02d", nYear, nMonth,
		     nDay );
	    UserCommand( sz );
	}
	    
	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( pwEvent ) ),
			 "event", &mi.pchEvent );
	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( pwRound ) ),
			 "round", &mi.pchRound );
	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( pwPlace ) ),
			 "place", &mi.pchPlace );
	UpdateMatchinfo( gtk_entry_get_text( GTK_ENTRY( pwAnnotator ) ),
			 "annotator", &mi.pchAnnotator );

        gtk_text_buffer_get_bounds (buffer, &begin, &end);
        pch = gtk_text_buffer_get_text(buffer, &begin, &end, FALSE);
	UpdateMatchinfo( pch, "comment", &mi.pchComment );
	g_free( pch );

	gtk_widget_destroy( pwDialog );
	
	outputresume();
    }
}

static void CalibrationOK( GtkWidget *pw, GtkWidget **ppw ) {

    char sz[ 128 ];
    GtkAdjustment *padj = gtk_spin_button_get_adjustment(
	GTK_SPIN_BUTTON( *ppw ) );
    
    if( GTK_WIDGET_IS_SENSITIVE( *ppw ) ) {
	if( padj->value != rEvalsPerSec ) {
	    sprintf( sz, "set calibration %.0f", padj->value );
	    UserCommand( sz );
	}
    } else if( rEvalsPerSec > 0 )
	UserCommand( "set calibration" );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void CalibrationEnable( GtkWidget *pw, GtkWidget *pwspin ) {

    gtk_widget_set_sensitive( pwspin, gtk_toggle_button_get_active(
				  GTK_TOGGLE_BUTTON( pw ) ) );
}

static void CalibrationGo( GtkWidget *pw, GtkWidget *apw[ 2 ] ) {

	GTKSetCurrentParent(gtk_widget_get_toplevel(pw));
    UserCommand( "calibrate" );

    fInterrupt = FALSE;
    
    if( rEvalsPerSec > 0 ) {
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( apw[ 0 ] ), TRUE );
	gtk_adjustment_set_value( gtk_spin_button_get_adjustment(
				      GTK_SPIN_BUTTON( apw[ 1 ] ) ),
				  rEvalsPerSec );
    }
}

extern void GTKShowCalibration( void ) {

    GtkAdjustment *padj;
    GtkWidget *pwDialog, *pwvbox, *pwhbox, *pwenable, *pwspin, *pwbutton,
	*apw[ 2 ];
    
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Speed estimate"),
			     DT_QUESTION, GTK_SIGNAL_FUNC( CalibrationOK ),
			     &pwspin );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwvbox = gtk_vbox_new( FALSE, 8 ) );
    gtk_container_set_border_width( GTK_CONTAINER( pwvbox ), 8 );
    gtk_container_add( GTK_CONTAINER( pwvbox ),
		       pwhbox = gtk_hbox_new( FALSE, 8 ) );
    gtk_container_add( GTK_CONTAINER( pwhbox ),
		       pwenable = gtk_check_button_new_with_label(
			   _("Speed recorded:") ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pwenable ),
				  rEvalsPerSec > 0 );
    
    padj = GTK_ADJUSTMENT( gtk_adjustment_new( rEvalsPerSec > 0 ?
					       rEvalsPerSec : 10000,
					       2, G_MAXFLOAT, 100,
					       1000, 0 ) );
    pwspin = gtk_spin_button_new( padj, 100, 0 );
    gtk_container_add( GTK_CONTAINER( pwhbox ), pwspin );
    gtk_widget_set_sensitive( pwspin, rEvalsPerSec > 0 );
    
    gtk_container_add( GTK_CONTAINER( pwhbox ), gtk_label_new(
			   _("static evaluations/second") ) );

    gtk_container_add( GTK_CONTAINER( pwvbox ),
		       pwbutton = gtk_button_new_with_label(
			   _("Calibrate") ) );
    apw[ 0 ] = pwenable;
    apw[ 1 ] = pwspin;
    gtk_signal_connect( GTK_OBJECT( pwbutton ), "clicked",
			GTK_SIGNAL_FUNC( CalibrationGo ), apw );

    gtk_signal_connect( GTK_OBJECT( pwenable ), "toggled",
			GTK_SIGNAL_FUNC( CalibrationEnable ), pwspin );
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();
}

static gboolean CalibrationCancel( GtkObject *po, gpointer p ) {

    fInterrupt = TRUE;

    return TRUE;
}

extern void *GTKCalibrationStart( void ) {

    GtkWidget *pwDialog, *pwhbox, *pwResult;
    
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Calibration"), DT_INFO,
			     GTK_SIGNAL_FUNC( CalibrationCancel ), NULL );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwhbox = gtk_hbox_new( FALSE, 8 ) );
    gtk_container_set_border_width( GTK_CONTAINER( pwhbox ), 8 );
    gtk_container_add( GTK_CONTAINER( pwhbox ),
		       gtk_label_new( _("Calibrating:") ) );
    gtk_container_add( GTK_CONTAINER( pwhbox ),
		       pwResult = gtk_label_new( _("       (n/a)       ") ) );
    gtk_container_add( GTK_CONTAINER( pwhbox ),
		       gtk_label_new( _("static evaluations/second") ) );
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( GTKGetCurrentParent() ) );

    pwOldGrab = pwGrab;
    pwGrab = pwDialog;
    
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "delete_event",
			GTK_SIGNAL_FUNC( CalibrationCancel ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    GTKAllowStdin();

    gtk_widget_ref( pwResult );
    
    return pwResult;
}

extern void GTKCalibrationUpdate( void *context, float rEvalsPerSec ) {

    char sz[ 32 ];

    sprintf( sz, "%.0f", rEvalsPerSec );
    gtk_label_set_text( GTK_LABEL( context ), sz );
    
    GTKDisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    GTKAllowStdin();
}

extern void GTKCalibrationEnd( void *context ) {

    gtk_widget_unref( GTK_WIDGET( context ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( GTK_WIDGET( context ) ) );

    pwGrab = pwOldGrab;
}

extern char *
GTKChangeDisk( const char *szMsg, const int fChange, 
               const char *szMissingFile ) {

  GtkWidget *pwDialog;
  gchar *pch;
  int f = FALSE;

  pwDialog = GTKCreateDialog( _("Change Disk"), DT_QUESTION, NULL, &f );

  pch = g_strdup_printf( szMsg, szMissingFile );
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     gtk_label_new( pch ) );
  g_free( pch );

  /* FIXME: add call to a file dialog for change of paths */
  /* FIXME: handle interrupt */

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
  
  gtk_widget_show_all( pwDialog );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();
  
  if ( !f )
    fInterrupt = TRUE;
  
  return NULL;

}


static void 
FinishMove( gpointer *p, guint n, GtkWidget *pw ) {

  int anMove[ 8 ];
  char sz[ 65 ];

  if ( ! GTKGetMove( anMove ) )
    /* no valid move */
    return;

  UserCommand( FormatMove( sz, ms.anBoard, anMove ) );

}

static void CallbackResign(GtkWidget *pw, gpointer data){

    int i = (int ) data;
    char *asz[3]= { "normal", "gammon", "backgammon" };
    char sz[20];
    
    if( i == -1 ){
        gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
	return;
    }

    sprintf(sz, "resign %s", asz[i]);
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
    UserCommand(sz);

    return;
}
    
extern void GTKResign( gpointer *p, guint n, GtkWidget *pw ) {

    GtkWidget *pwWindow, *pwVbox, *pwHbox, *pwButtons;
    int i;
    char **apXPM[3];
    char *asz[3] = { _("Resign normal"),
		 _("Resign gammon"),
		 _("Resign backgammon") };
		 
#include "xpm/resigns.xpm"

    apXPM[0] = resign_n_xpm;
    apXPM[1] = resign_g_xpm;
    apXPM[2] = resign_bg_xpm;

    pwWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pwWindow), _("Resign"));

    pwVbox = gtk_vbox_new(TRUE, 5);

    for (i = 0; i < 3 ; i++){
	pwButtons = gtk_button_new();
	pwHbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pwButtons), pwHbox); 
    	gtk_box_pack_start(GTK_BOX(pwHbox), 
		image_from_xpm_d(apXPM[i], pwButtons), FALSE,FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwHbox), gtk_label_new(asz[i]), TRUE, TRUE, 10);
	gtk_container_add(GTK_CONTAINER(pwVbox), pwButtons);
	gtk_signal_connect(GTK_OBJECT(pwButtons), "clicked",
		GTK_SIGNAL_FUNC( CallbackResign ), (int *) i );
    }

    pwButtons = gtk_button_new_with_label(_("Cancel"));
    gtk_container_add(GTK_CONTAINER(pwVbox), pwButtons);
    gtk_signal_connect(GTK_OBJECT(pwButtons), "clicked",
		GTK_SIGNAL_FUNC( CallbackResign ), (int *) -1 );
    gtk_signal_connect(GTK_OBJECT(pwWindow), "destroy",
		GTK_SIGNAL_FUNC( CallbackResign ), (int *) -1 );

    gtk_container_add(GTK_CONTAINER(pwWindow), pwVbox);
    gtk_widget_show_all(pwWindow);
    
    gtk_main();
}

/* PyShell expects sys.argv to be defined. '-n' makes PyShell run without a
 * subshell, which is needed because of some conflict with the gnubg module. */
static void PythonShell(gpointer * p, guint n, GtkWidget * pw)
{
	char *pch;
	pch =
	    strdup(">import sys;"
		   "sys.argv=['','-n'];"
		   "import idlelib.PyShell;" "idlelib.PyShell.main()");
	UserCommand(pch);
	free(pch);
}

GtkWidget *pwTick;

static void
WarningOK ( GtkWidget *pw, warnings warning )
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pwTick)))
	{	/* if tick set, disable warning */
		char cmd[200];
		sprintf(cmd, "set warning %s off", warningNames[warning]);
		UserCommand(cmd);
	}
	gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

extern void GTKShowWarning(warnings warning)
{
	if (warningEnabled[warning])
	{
		GtkWidget *pwDialog, *pwMsg, *pwv;
		
		pwDialog = GTKCreateDialog( _("GNU Backgammon - Warning"),
					DT_WARNING, GTK_SIGNAL_FUNC ( WarningOK ), (void*)warning );

		pwv = gtk_vbox_new ( FALSE, 8 );
		gtk_container_add ( GTK_CONTAINER (DialogArea( pwDialog, DA_MAIN ) ), pwv );

                pwMsg = gtk_label_new( gettext( warningStrings[warning] ) );
		gtk_box_pack_start( GTK_BOX( pwv ), pwMsg, TRUE, TRUE, 0 );

		pwTick = gtk_check_button_new_with_label (_("Don't show this again"));
		gtk_tooltips_set_tip(ptt, pwTick, _("If set, this message won't appear again"), 0);
		gtk_box_pack_start( GTK_BOX( pwv ), pwTick, TRUE, TRUE, 0 );

		gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
		gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					  GTK_WINDOW( pwMain ) );
		gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
				GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

		gtk_widget_show_all( pwDialog );

		GTKDisallowStdin();
		gtk_main();
		GTKAllowStdin();
	}
}

static gboolean EndFullScreen(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	short k = event->keyval;

	if (k == KEY_ESCAPE)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif, "/View/Full screen")), 0);

	return FALSE;
}

#if USE_BOARD3D

void SetSwitchModeMenuText()
{	/* Update menu text */
	BoardData *bd = BOARD( pwBoard )->board_data;
	GtkWidget *pMenuItem = gtk_item_factory_get_widget_by_action(pif, TOOLBAR_ACTION_OFFSET + MENU_OFFSET);
	char *text;
	if (bd->rd->fDisplayType == DT_2D)
		text = _("Switch to 3D view");
	else
		text = _("Switch to 2D view");
	gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(pMenuItem))), text);
}

static void
SwitchDisplayMode( gpointer *p, guint n, GtkWidget *pw )
{
	BoardData *bd = BOARD( pwBoard )->board_data;
	BoardData3d *bd3d = &bd->bd3d;
	renderdata *prd = bd->rd;

	if (prd->fDisplayType == DT_2D)
	{
		prd->fDisplayType = DT_3D;
		/* Reset 3d settings */
		MakeCurrent3d(bd3d->drawing_area3d);
		preDraw3d(bd, bd3d, prd);
		updateOccPos(bd);	/* Make sure shadows are in correct place */
		if (bd->diceShown == DICE_ON_BOARD)
			setDicePos(bd, bd3d);	/* Make sure dice appear ok */
		RestrictiveRedraw();
	}
	else
	{
		prd->fDisplayType = DT_2D;
		/* Make sure 2d pixmaps are correct */
		board_free_pixmaps(bd);
		board_create_pixmaps(pwBoard, bd);
		/* Make sure dice are visible if rolled */
		if (bd->diceShown == DICE_ON_BOARD && bd->x_dice[0] <= 0)
			RollDice2d(bd);
	}

	DisplayCorrectBoardType(bd, bd3d, prd);
	SetSwitchModeMenuText();
}
#endif

static void
FullScreenMode( gpointer *p, guint n, GtkWidget *pw ) {
	BoardData *bd = BOARD( pwBoard )->board_data;
	GtkWindow* ptl = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(bd->table)));
	GtkWidget *pwHandle = gtk_widget_get_parent(pwToolbar);
	static gulong id;
	static int showingPanels;
	static int showIDs;
	static int maximised;
	static int changedRP, changedDP;

	int state = !GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif, "/View/Full screen"))->active;
	GtkWidget* pmiRP = gtk_item_factory_get_widget(pif, "/View/Restore panels");
	GtkWidget* pmiDP = gtk_item_factory_get_widget(pif, "/View/Dock panels");

	bd->rd->fShowGameInfo = state;

	if (!state)
	{
		GTKShowWarning(WARN_FULLSCREEN_EXIT);

		if (pmiRP && GTK_WIDGET_VISIBLE(pmiRP) && GTK_WIDGET_IS_SENSITIVE(pmiRP))
			changedRP = TRUE;
		if (pmiDP && GTK_WIDGET_VISIBLE(pmiDP) && GTK_WIDGET_IS_SENSITIVE(pmiDP))
			changedDP = TRUE;

		/* Check if window is maximized */
		{
		GdkWindowState state = gdk_window_get_state(GTK_WIDGET(ptl)->window);
		maximised = ((state & GDK_WINDOW_STATE_MAXIMIZED) == GDK_WINDOW_STATE_MAXIMIZED);
		}

		id = gtk_signal_connect(GTK_OBJECT(ptl), "key-press-event", GTK_SIGNAL_FUNC(EndFullScreen), 0);

		gtk_widget_hide(GTK_WIDGET(bd->table));
		gtk_widget_hide(GTK_WIDGET(bd->dice_area));
		gtk_widget_hide(pwStatus);
		gtk_widget_hide(pwProgress);

		showingPanels = ArePanelsShowing();

		showIDs = bd->rd->fShowIDs;
		bd->rd->fShowIDs = 0;

		HideAllPanels(NULL, 0, NULL);

		gtk_widget_hide(pwToolbar);
		gtk_widget_hide(pwHandle);

		/* Max window maximal */
		if (!maximised)
			gtk_window_maximize(ptl);
		gtk_window_set_decorated(ptl, FALSE);
		if (pmiRP)
			gtk_widget_set_sensitive(pmiRP, FALSE);
		if (pmiDP)
			gtk_widget_set_sensitive(pmiDP, FALSE);
	}
	else
	{
		gtk_widget_show(pwMenuBar);
		gtk_widget_show(pwToolbar);
		gtk_widget_show(pwHandle);
		gtk_widget_show(GTK_WIDGET(bd->table));
#if USE_BOARD3D
	/* Only show 2d dice below board if in 2d */
  	if (bd->rd->fDisplayType == DT_2D)
#endif
		  gtk_widget_show(GTK_WIDGET(bd->dice_area));
		gtk_widget_show(pwStatus);
		gtk_widget_show(pwProgress);

		bd->rd->fShowIDs = showIDs;

		gtk_signal_disconnect(GTK_OBJECT(ptl), id);

		/* Restore window */
		if (!maximised)
			gtk_window_unmaximize(ptl);
		gtk_window_set_decorated(ptl, TRUE);

		if (showingPanels)
			ShowAllPanels(NULL, 0, NULL);

		if (changedRP)
		{
			gtk_widget_set_sensitive(pmiRP, TRUE);
			changedRP = FALSE;
		}
		if (changedDP)
		{
			gtk_widget_set_sensitive(pmiDP, TRUE);
			changedDP = FALSE;
		}
	}
	UpdateSetting(&bd->rd->fShowIDs);
}

extern void Undo()
{
#if USE_BOARD3D
	RestrictiveRedraw();
#endif

	ShowBoard();

        UpdateTheoryData(BOARD( pwBoard )->board_data, TT_RETURNHITS, ms.anBoard);
}

static void
TogglePanel ( gpointer *p, guint n, GtkWidget *pw ) {
  int f;
  gnubgwindow panel = 0;

  g_assert( GTK_IS_CHECK_MENU_ITEM( pw ) );
  
  f = GTK_CHECK_MENU_ITEM( pw )->active ;
  switch( n ) {
	  case TOGGLE_ANALYSIS:
		  panel = WINDOW_ANALYSIS;
		break;
	  case TOGGLE_COMMENTARY:
		  panel = WINDOW_ANNOTATION;
		break;
	  case TOGGLE_GAMELIST:
		  panel = WINDOW_GAME;
		break;
	  case TOGGLE_MESSAGE:
		  panel = WINDOW_MESSAGE;
		break;
	  case TOGGLE_THEORY:
		  panel = WINDOW_THEORY;
		break;
	  case TOGGLE_COMMAND:
		  panel = WINDOW_COMMAND;
		break;
	  default:
		  assert(FALSE);
  }
  if (f)
	PanelShow(panel);
  else
	PanelHide(panel);

  /* Resize screen */
  SetMainWindowSize();
}

#if USE_PYTHON
static GtkWidget* GetRelList(RowSet* pRow)
{
	int i;
	PangoRectangle logical_rect;
	PangoLayout *layout;
	GtkWidget *pwList = gtk_clist_new(pRow->cols);
	gtk_clist_column_titles_show(GTK_CLIST(pwList));
	gtk_clist_column_titles_passive(GTK_CLIST(pwList));
	for (i = 0; i < pRow->cols; i++)
	{
		char* widthStr = malloc(pRow->widths[i] + 1);
		int width;
		memset(widthStr, 'a', pRow->widths[i]);
		widthStr[pRow->widths[i]] = '\0';

		layout = gtk_widget_create_pango_layout(pwList, widthStr);
		pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
		g_object_unref (layout);
		width = logical_rect.width;
		gtk_clist_set_column_title(GTK_CLIST(pwList), i, pRow->data[0][i]);

		gtk_clist_set_column_width( GTK_CLIST( pwList ), i, width);
		gtk_clist_set_column_resizeable(GTK_CLIST(pwList), i, FALSE);
	}
	GTK_WIDGET_UNSET_FLAGS(pwList, GTK_CAN_FOCUS);

	for (i = 1; i < pRow->rows; i++)
	{
		gtk_clist_append(GTK_CLIST(pwList), pRow->data[i]);
	}
	return pwList;
}

int curPlayerId, curRow;
GtkWidget *pwPlayerName, *pwPlayerNotes, *pwQueryText, *pwQueryResult, *pwQueryBox,
	*aliases, *pwAliasList;

static void ShowRelationalSelect(GtkWidget *pw, int y, int x, GdkEventButton *peb, GtkWidget *pwCopy)
{
	char *pName, *pEnv;
	RowSet r, r2;
	char query[1024];
	int i;

	gtk_clist_get_text(GTK_CLIST(pw), y, 0, &pName);
	gtk_clist_get_text(GTK_CLIST(pw), y, 1, &pEnv);

	sprintf(query, "person.person_id, person.name, person.notes"
		" FROM nick INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE nick.name = '%s' AND env.place = '%s'",
		pName, pEnv);

	ClearText(GTK_TEXT_VIEW(pwPlayerNotes));
	if( pwAliasList ) {
	  ClearText(GTK_TEXT_VIEW(pwAliasList));
	}
	if (!RunQuery(&r, query))
	{
		gtk_entry_set_text(GTK_ENTRY(pwPlayerName), "");
		return;
	}

	assert(r.rows == 2);	/* Should be exactly one entry */

	curRow = y;
	curPlayerId = atoi(r.data[1][0]);
	gtk_entry_set_text(GTK_ENTRY(pwPlayerName), r.data[1][1]);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwPlayerNotes)), r.data[1][2], -1);

	sprintf(query, _("%s is:"), r.data[1][1]);
	if( aliases )
	  gtk_label_set_text(GTK_LABEL(aliases), query);

	sprintf(query, "nick.name, env.place"
		" FROM nick INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE person.person_id = %d", curPlayerId);

	if (!RunQuery(&r2, query)) {
		return;
	}

	if( pwAliasList)
  {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(pwAliasList));
    GtkTextIter it;
    gtk_text_buffer_get_end_iter(buffer, &it);
  	for (i = 1; i < r2.rows; i++)
  	{
  		char line[100];
  		sprintf(line, _("%s on %s\n"), r2.data[i][0], r2.data[i][1]);
  		  gtk_text_buffer_insert(buffer, &it, line, -1);
  	}
	}
	FreeRowset(&r);
	FreeRowset(&r2);

}

static void RelationalQuery(GtkWidget *pw, GtkWidget *pwVbox)
{
	RowSet r;
	char *pch, *query;

  pch = GetText(GTK_TEXT_VIEW(pwQueryText));

	if (!strncasecmp("select ", pch, strlen("select ")))
		query = pch + strlen("select ");
	else
		query = pch;

	if (RunQuery(&r, query))
	{
		gtk_widget_destroy(pwQueryResult);
		pwQueryResult = GetRelList(&r);
		gtk_box_pack_start(GTK_BOX(pwQueryBox), pwQueryResult, TRUE, TRUE, 0);
		gtk_widget_show(pwQueryResult);
		FreeRowset(&r);
	}

	g_free(pch);
}

static void UpdatePlayerDetails(GtkWidget *pw, int *dummy)
{
	char *pch;
	if (curPlayerId == -1)
		return;
		
  pch = GetText(GTK_TEXT_VIEW(pwPlayerNotes));
	RelationalUpdatePlayerDetails(curPlayerId, gtk_entry_get_text(GTK_ENTRY(pwPlayerName)), pch);
	g_free(pch);
}

char envString[100], curEnvString[100], linkPlayer[100];
int currentLinkRow;

static void LinkPlayerSelect(GtkWidget *pw, int y, int x, GdkEventButton *peb, void* null)
{
	currentLinkRow = y;
}

static void SelectLinkOK(GtkWidget *pw, GtkWidget *pwList)
{
	char* current;
	if (currentLinkRow != -1)
	{
		gtk_clist_get_text(GTK_CLIST(pwList), currentLinkRow, 0, &current);
		strcpy(linkPlayer, current);
		gtk_widget_destroy(gtk_widget_get_toplevel(pw));
	}
}

static void RelationalLinkPlayers(GtkWidget *pw, GtkWidget *pwRelList)
{
	char query[1024];
	RowSet r;
	GtkWidget *pwDialog, *pwList;
	char *linkNick, *linkEnv;
	if (curPlayerId == -1)
		return;

	gtk_clist_get_text(GTK_CLIST(pwRelList), curRow, 0, &linkNick);
	gtk_clist_get_text(GTK_CLIST(pwRelList), curRow, 1, &linkEnv);

	/* Pick suitable linking players with this query... */
	sprintf(query, "person.name AS Player FROM person WHERE person_id NOT IN"
		" (SELECT person.person_id FROM nick" 
		" INNER JOIN env ON nick.env_id = env.env_id"
		" INNER JOIN person ON nick.person_id = person.person_id"
		" WHERE env.env_id IN"
		" (SELECT env_id FROM nick INNER JOIN person"
		" ON nick.person_id = person.person_id"
		" WHERE person.name = '%s'))",
		gtk_entry_get_text(GTK_ENTRY(pwPlayerName)));
	if (!RunQuery(&r, query))
		return;

	if (r.rows < 2)
	{
		GTKMessage("No players to link to", DT_ERROR);
		FreeRowset(&r);
		return;
	}

	pwList = GetRelList(&r);
	FreeRowset(&r);

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Select Linked Player"),
						   DT_QUESTION, GTK_SIGNAL_FUNC(SelectLinkOK), pwList);

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), pwList);
	gtk_signal_connect( GTK_OBJECT( pwList ), "select-row",
							GTK_SIGNAL_FUNC( LinkPlayerSelect ), pwList );

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_widget_show_all( pwDialog );

	*linkPlayer = '\0';
	currentLinkRow = -1;

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();

	if (*linkPlayer)
	{
		RelationalLinkNick(linkNick, linkEnv, linkPlayer);
		ShowRelationalSelect(pwRelList, curRow, 0, 0, 0);
		/* update new details list...
		   >> change query so person not picked if nick in selected env!
		*/
	}
}

static void RelationalOpen(GtkWidget *pw, GtkWidget* pwList)
{
    char *player, *env;
	char buf[200];

	if (curPlayerId == -1)
		return;

	/* Get player name and env from list */
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 1, &env);

	sprintf(buf, "Open (%s, %s)", player, env);
	output(buf);
	outputx();
}

static void RelationalErase(GtkWidget *pw, GtkWidget* pwList)
{
    char *player, *env;
	char buf[200];

	if (curPlayerId == -1)
		return;

	/* Get player name and env from list */
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 0, &player);
	gtk_clist_get_text(GTK_CLIST(pwList), curRow, 1, &env);

	sprintf(buf, _("Remove all data for %s?"), player);
	if(!GetInputYN(buf))
	    return;

	sprintf(buf, "\"%s\" \"%s\"", player, env);
	CommandRelationalErase(buf);

	gtk_clist_remove(GTK_CLIST(pwList), curRow);
}

char envString[100], curEnvString[100];
GtkWidget *envP1, *envP2;
RowSet *pNick1Envs, *pNick2Envs, *pEnvRows;

static void SelectEnvOK(GtkWidget *pw, GtkWidget *pwCombo)
{
	char* current = (char *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pwCombo)->entry));
	strcpy(envString, current);
	gtk_widget_destroy(gtk_widget_get_toplevel(pw));
}

void SelEnvChange(GtkList *list, GtkWidget* pwCombo)
{
	char* current = (char *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pwCombo)->entry));
	if (current && *current && (!*curEnvString || strcmp(current, curEnvString)))
	{
		int i, id;
		strcpy(curEnvString, current);

		/* Get id from selected string */
		i = 1;
		while (strcmp(pEnvRows->data[i][1], curEnvString))
			i++;
		id = atoi(pEnvRows->data[i][0]);

		/* Set player names for env (if available) */
		for (i = 1; i < pNick1Envs->rows; i++)
		{
			if (atoi(pNick1Envs->data[i][0]) == id)
				break;
		}
		if (i < pNick1Envs->rows)
			gtk_label_set_text(GTK_LABEL(envP1), pNick1Envs->data[i][1]);
		else
			gtk_label_set_text(GTK_LABEL(envP1), "[new to env]");

		for (i = 1; i < pNick2Envs->rows; i++)
		{
			if (atoi(pNick2Envs->data[i][0]) == id)
				break;
		}
		if (i < pNick2Envs->rows)
			gtk_label_set_text(GTK_LABEL(envP2), pNick2Envs->data[i][1]);
		else
			gtk_label_set_text(GTK_LABEL(envP2), "[new to env]");
	}
}

static int GtkGetEnv(char* env)
{	/* Get an environment for the match to be added */
	int envID;
	/* See if more than one env (if not no choice) */
	RowSet envRows;
	char *query = "env_id, place FROM env ORDER BY env_id";

	if (!RunQuery(&envRows, query))
	{
		GTKMessage(_("Database error getting environment"), DT_ERROR);
		return -1;
	}

	if (envRows.rows == 2)
	{	/* Just one env */
		strcpy(env, envRows.data[1][1]);
	}
	else
	{
		/* find out what envs players nicks are in */
		int found, cur1, cur2, i, NextEnv;
		RowSet nick1, nick2;
		char query[1024];
		char *queryText = "env_id, person.name"
				" FROM nick INNER JOIN person"
				" ON nick.person_id = person.person_id"
				" WHERE nick.name = '%s' ORDER BY env_id";
		
		pNick1Envs = &nick1;
		pNick2Envs = &nick2;
		pEnvRows = &envRows;

		sprintf(query, queryText, ap[0].szName);
		if (!RunQuery(&nick1, query))
		{
			GTKMessage(_("Database error getting environment"), DT_ERROR);
			return -1;
		}

		sprintf(query, queryText, ap[1].szName);
		if (!RunQuery(&nick2, query))
		{
			GTKMessage(_("Database error getting environment"), DT_ERROR);
			return -1;
		}

		/* pick best (first with both players or first with either player) */
		found = 0;
		cur1 = cur2 = 1;
		envID = atoi(envRows.data[1][0]);

		for (i = 1; i < envRows.rows; i++)
		{
			int oneFound = FALSE;
			int curID = atoi(envRows.data[i][0]);
			if (nick1.rows > cur1)
			{
				NextEnv = atoi(nick1.data[cur1][0]);
				if (NextEnv == curID)
				{
					if (found == 0)
					{	/* Found at least one */
						found = 1;
						envID = curID;
					}
					oneFound = TRUE;
					cur1++;
				}
			}
			if (nick2.rows > cur2)
			{
				NextEnv = atoi(nick2.data[cur2][0]);
				if (NextEnv == curID)
				{
					if (found == 0)
					{	/* Found at least one */
						found = 1;
						envID = curID;
					}
					if (oneFound == 1)
					{	/* Found env with both nicks */
						envID = curID;
						break;
					}
				}
			}
		}
		/* show dialog to select env */
		{
			GList *glist;
			GtkWidget *pwEnvCombo, *pwDialog, *pwHbox, *pwVbox;
			pwEnvCombo = gtk_combo_new();

			pwDialog = GTKCreateDialog( _("GNU Backgammon - Select Environment"),
							DT_QUESTION, GTK_SIGNAL_FUNC(SelectEnvOK), pwEnvCombo);

			gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), 
				pwHbox = gtk_hbox_new(FALSE, 0));

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(pwVbox), gtk_label_new(_("Environment")), FALSE, FALSE, 0);

			gtk_combo_set_value_in_list(GTK_COMBO(pwEnvCombo), TRUE, FALSE);
			glist = NULL;
			for (i = 1; i < envRows.rows; i++)
				glist = g_list_append(glist, envRows.data[i][1]);
			gtk_combo_set_popdown_strings(GTK_COMBO(pwEnvCombo), glist);
			g_list_free(glist);
			gtk_box_pack_start(GTK_BOX(pwVbox), pwEnvCombo, FALSE, FALSE, 0);

			/* Select best env */
			i = 1;
			while (atoi(envRows.data[i][0]) != envID)
				i++;

			*curEnvString = '\0';
			gtk_widget_set_sensitive(GTK_WIDGET(GTK_COMBO(pwEnvCombo)->entry), FALSE);
			gtk_signal_connect(GTK_OBJECT(GTK_COMBO(pwEnvCombo)->list), "selection-changed",
							GTK_SIGNAL_FUNC(SelEnvChange), pwEnvCombo);

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 10);
			gtk_box_pack_start(GTK_BOX(pwVbox), gtk_label_new(ap[0].szName), FALSE, FALSE, 0);
			envP1 = gtk_label_new(NULL);
			gtk_box_pack_start(GTK_BOX(pwVbox), envP1, FALSE, FALSE, 0);

			gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 10);
			gtk_box_pack_start(GTK_BOX(pwVbox), gtk_label_new(ap[1].szName), FALSE, FALSE, 0);
			envP2 = gtk_label_new(NULL);
			gtk_box_pack_start(GTK_BOX(pwVbox), envP2, FALSE, FALSE, 0);

			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(pwEnvCombo)->entry), envRows.data[i][1]);

			gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
			gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
							GTK_WINDOW( pwMain ) );
			gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
					GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

			gtk_widget_show_all( pwDialog );

			*envString = '\0';

			GTKDisallowStdin();
			gtk_main();
			GTKAllowStdin();

			if (*envString)
			{
				strcpy(env, envString);
			}
			else
				*env = '\0';
		}

		FreeRowset(&nick1);
		FreeRowset(&nick2);
	}

	FreeRowset(&envRows);
	return 0;
}

static void GtkRelationalAddMatch( gpointer *p, guint n, GtkWidget *pw )
{
	char env[100];
	char buf[200];
	int exists = RelationalMatchExists();
	if (exists == -1 ||
		(exists == 1 && !GetInputYN(_("Match exists, overwrite?"))))
		return;

	if (GtkGetEnv(env) < 0)
		return;

	/* Pass in env id and force addition */
	sprintf(buf, "\"%s\" Yes", env);
	CommandRelationalAddMatch(buf);

	outputx();
}

#define PACK_OFFSET 4
#define OUTSIDE_FRAME_GAP PACK_OFFSET
#define INSIDE_FRAME_GAP PACK_OFFSET
#define NAME_NOTES_VGAP PACK_OFFSET
#define BUTTON_GAP PACK_OFFSET
#define QUERY_BORDER 1

static void GtkShowRelational( gpointer *p, guint n, GtkWidget *pw )
{
	RowSet r;
	GtkWidget *pwRun, *pwList, *pwDialog, *pwHbox2, *pwVbox2,
		*pwPlayerFrame, *pwUpdate, *pwHbox, *pwVbox, *pwErase, *pwOpen, *pwn,
		*pwLabel, *pwLink, *pwScrolled;
	int multipleEnv;


	pwAliasList = 0;
	aliases = 0;

	/* See if there's more than one environment */
	if (!RunQuery(&r, "env_id FROM env"))
	{
		GTKMessage(_("Database error"), DT_ERROR);
		return;
	}
	multipleEnv = (r.rows > 2);
	FreeRowset(&r);

	curPlayerId = -1;

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Relational Database"),
					DT_INFO, NULL, NULL );

	pwn = gtk_notebook_new();
  gtk_container_set_border_width(GTK_CONTAINER(pwn), 0);
	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), pwn);

/*******************************************************
** Start of (left hand side) of player screen...
*******************************************************/

  pwHbox = gtk_hbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(pwn), pwHbox, gtk_label_new(_("Players")));

	pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox), INSIDE_FRAME_GAP);

	if (!RunQuery(&r, "name AS Nickname, place AS env FROM nick INNER JOIN env"
		" ON nick.env_id = env.env_id ORDER BY name"))
		return;

	if (r.rows < 2)
	{
		GTKMessage(_("No data in database"), DT_ERROR);
		return;
	}

	pwList = GetRelList(&r);
	gtk_signal_connect( GTK_OBJECT( pwList ), "select-row",
							GTK_SIGNAL_FUNC( ShowRelationalSelect ), pwList );
	FreeRowset(&r);

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(pwScrolled), pwList);
		gtk_box_pack_start(GTK_BOX(pwVbox), pwScrolled, TRUE, TRUE, 0);


	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);

	pwOpen = gtk_button_new_with_label( "Open" );
	gtk_signal_connect(GTK_OBJECT(pwOpen), "clicked",
				GTK_SIGNAL_FUNC(RelationalOpen), pwList);
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwOpen, FALSE, FALSE, 0);

	pwErase = gtk_button_new_with_label( "Erase" );
	gtk_signal_connect(GTK_OBJECT(pwErase), "clicked",
				GTK_SIGNAL_FUNC(RelationalErase), pwList);
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwErase, FALSE, FALSE, BUTTON_GAP);

/*******************************************************
** Start of right hand side of player screen...
*******************************************************/

	pwPlayerFrame = gtk_frame_new("Player");
	gtk_container_set_border_width(GTK_CONTAINER(pwPlayerFrame), OUTSIDE_FRAME_GAP);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwPlayerFrame, TRUE, TRUE, 0);

	pwVbox = gtk_vbox_new(FALSE, NAME_NOTES_VGAP);
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox), INSIDE_FRAME_GAP);
	gtk_container_add(GTK_CONTAINER(pwPlayerFrame), pwVbox);

	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);

  pwLabel = gtk_label_new("Name");
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwLabel, FALSE, FALSE, 0);
	pwPlayerName = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwPlayerName, TRUE, TRUE, 0);

	pwVbox2 = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwVbox2, TRUE, TRUE, 0);

	pwLabel = gtk_label_new("Notes");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwVbox2), pwLabel, FALSE, FALSE, 0);

	pwPlayerNotes = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(pwPlayerNotes), TRUE);

  pwScrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(pwScrolled), pwPlayerNotes);
	gtk_box_pack_start(GTK_BOX(pwVbox2), pwScrolled, TRUE, TRUE, 0);

	pwHbox2 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox2, FALSE, FALSE, 0);
	
	pwUpdate = gtk_button_new_with_label("Update Details");
	gtk_box_pack_start(GTK_BOX(pwHbox2), pwUpdate, FALSE, FALSE, 0);
	gtk_signal_connect_object(GTK_OBJECT(pwUpdate), "clicked",
					GTK_SIGNAL_FUNC(UpdatePlayerDetails), NULL);

/*******************************************************
** End of right hand side of player screen...
*******************************************************/

	/* If more than one environment, show linked nicknames */
	if (multipleEnv)
	{	/* Multiple environments */
		gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);
		aliases = gtk_label_new(NULL);
		gtk_misc_set_alignment(GTK_MISC(aliases), 0, 0);
		gtk_box_pack_start(GTK_BOX(pwVbox), aliases, FALSE, FALSE, 0);

		pwAliasList = gtk_text_view_new();
		gtk_text_view_set_editable(GTK_TEXT_VIEW(pwAliasList), FALSE);
		gtk_box_pack_start(GTK_BOX(pwVbox), pwAliasList, TRUE, TRUE, 0);

		pwLink = gtk_button_new_with_label(_("Link players"));
		gtk_signal_connect(GTK_OBJECT(pwLink), "clicked",
			GTK_SIGNAL_FUNC(RelationalLinkPlayers), pwList);
		gtk_box_pack_start(GTK_BOX(pwVbox), pwLink, FALSE, TRUE, 0);
	}

	/* Query sheet */
  pwVbox = gtk_vbox_new(FALSE, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(pwn), pwVbox, gtk_label_new(_("Query")));
	gtk_container_set_border_width(GTK_CONTAINER(pwVbox), INSIDE_FRAME_GAP);

	pwLabel = gtk_label_new("Query text");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwLabel, FALSE, FALSE, 0);

	pwQueryText = gtk_text_view_new();
  gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText), GTK_TEXT_WINDOW_TOP, QUERY_BORDER);
  gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText), GTK_TEXT_WINDOW_RIGHT, QUERY_BORDER);
  gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText), GTK_TEXT_WINDOW_BOTTOM, QUERY_BORDER);
  gtk_text_view_set_border_window_size(GTK_TEXT_VIEW(pwQueryText), GTK_TEXT_WINDOW_LEFT, QUERY_BORDER);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(pwQueryText), TRUE);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwQueryText, FALSE, FALSE, 0);
	gtk_widget_set_usize(pwQueryText, 250, 80);

  pwHbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwHbox, FALSE, FALSE, 0);
	pwLabel = gtk_label_new("Result");
	gtk_misc_set_alignment(GTK_MISC(pwLabel), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwLabel, TRUE, TRUE, 0);

	pwRun = gtk_button_new_with_label( "Run Query" );
	gtk_signal_connect(GTK_OBJECT(pwRun), "clicked",
				GTK_SIGNAL_FUNC(RelationalQuery), pwVbox);
	gtk_box_pack_start(GTK_BOX(pwHbox), pwRun, FALSE, FALSE, 0);

	pwQueryResult = gtk_clist_new(1);
	gtk_clist_set_column_title(GTK_CLIST(pwQueryResult), 0, " ");
	gtk_clist_column_titles_show(GTK_CLIST(pwQueryResult));
	gtk_clist_column_titles_passive(GTK_CLIST(pwQueryResult));

	pwQueryBox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(pwQueryBox), pwQueryResult, TRUE, TRUE, 0);

	pwScrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pwScrolled),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pwScrolled), pwQueryBox);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwScrolled, TRUE, TRUE, 0);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_widget_show_all( pwDialog );

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
}

extern void GtkShowQuery(RowSet* pRow)
{
	GtkWidget *pwDialog;

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Database Result"),
					DT_INFO, NULL, NULL );
	
	gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       GetRelList(pRow));

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
 
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();
}

int curEnvRow, numEnvRows;
GtkWidget *pwRemoveEnv, *pwRenameEnv;

static void ManageEnvSelect(GtkWidget *pw, int y, int x, GdkEventButton *peb, GtkWidget *pwCopy)
{
	gtk_widget_set_sensitive(pwRemoveEnv, TRUE);
	gtk_widget_set_sensitive(pwRenameEnv, TRUE);

	curEnvRow = y;
}

extern int env_added;

static void RelationalNewEnv(GtkWidget *pw, GtkWidget* pwList)
{
    char *newName;

	if ((newName = GTKGetInput(_("New Environment"), _("Enter name"))))
	{
		char *aszRow[1];
		CommandRelationalAddEnvironment(newName);

		if (env_added)
		{
			aszRow[0] = newName;
			gtk_clist_append(GTK_CLIST(pwList), aszRow);
			free(newName);

			gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
			gtk_widget_set_sensitive(pwRenameEnv, FALSE);

			numEnvRows++;
		}
	}
}

extern int env_deleted;

static void RelationalRemoveEnv(GtkWidget *pw, GtkWidget* pwList)
{
    char *pch;

	if (numEnvRows == 1)
	{
		GTKMessage(_("You must have at least one environment"), DT_WARNING);
		return;
	}

	gtk_clist_get_text(GTK_CLIST(pwList), curEnvRow, 0, &pch);

	CommandRelationalEraseEnv(pch);

	if (env_deleted)
	{
		gtk_clist_remove(GTK_CLIST(pwList), curEnvRow);
		gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
		gtk_widget_set_sensitive(pwRenameEnv, FALSE);

		numEnvRows--;
	}
}

static void RelationalRenameEnv(GtkWidget *pw, GtkWidget* pwList)
{
    char *pch, *newName;
	gtk_clist_get_text(GTK_CLIST(pwList), curEnvRow, 0, &pch);

	if ((newName = GTKGetInput(_("Rename Environment"), _("Enter new name"))))
	{
		char str[1024];
		sprintf(str, "\"%s\" \"%s\"", pch, newName);
		CommandRelationalRenameEnv(str);
		gtk_clist_set_text(GTK_CLIST(pwList), curEnvRow, 0, newName);
		free(newName);
	}
}

static void GtkManageRelationalEnvs( gpointer *p, guint n, GtkWidget *pw )
{
	RowSet r;
	GtkWidget *pwList, *pwDialog, *pwHbox, *pwVbox, *pwBut;

	pwDialog = GTKCreateDialog( _("GNU Backgammon - Manage Environments"),
					DT_INFO, NULL, NULL );

	gtk_container_add(GTK_CONTAINER(DialogArea(pwDialog, DA_MAIN)), 
		pwHbox = gtk_hbox_new(FALSE, 0));

	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);

	if (!RunQuery(&r, "place as environment FROM env"))
		return;

	numEnvRows = r.rows - 1; /* -1 as header row counted */

	pwList = GetRelList(&r);
	gtk_signal_connect( GTK_OBJECT( pwList ), "select-row",
							GTK_SIGNAL_FUNC( ManageEnvSelect ), pwList );
	gtk_box_pack_start(GTK_BOX(pwVbox), pwList, TRUE, TRUE, 0);
	FreeRowset(&r);

	gtk_box_pack_start(GTK_BOX(pwHbox), pwVbox = gtk_vbox_new(FALSE, 0), FALSE, FALSE, 0);

	pwBut = gtk_button_new_with_label( _("New Env" ) );
	gtk_signal_connect(GTK_OBJECT(pwBut), "clicked",
				GTK_SIGNAL_FUNC(RelationalNewEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwBut, FALSE, FALSE, 4);

	pwRemoveEnv = gtk_button_new_with_label( _("Remove Env" ) );
	gtk_signal_connect(GTK_OBJECT(pwRemoveEnv), "clicked",
				GTK_SIGNAL_FUNC(RelationalRemoveEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwRemoveEnv, FALSE, FALSE, 4);

	pwRenameEnv = gtk_button_new_with_label( _("Rename Env" ) );
	gtk_signal_connect(GTK_OBJECT(pwRenameEnv), "clicked",
				GTK_SIGNAL_FUNC(RelationalRenameEnv), pwList);
	gtk_box_pack_start(GTK_BOX(pwVbox), pwRenameEnv, FALSE, FALSE, 4);

	gtk_widget_set_sensitive(pwRemoveEnv, FALSE);
	gtk_widget_set_sensitive(pwRenameEnv, FALSE);

	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
					GTK_WINDOW( pwMain ) );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

	gtk_widget_show_all( pwDialog );

	GTKDisallowStdin();
	gtk_main();
	GTKAllowStdin();
}
#else
static void GtkShowRelational( gpointer *p, guint n, GtkWidget *pw )
{
}
static void GtkRelationalAddMatch( gpointer *p, guint n, GtkWidget *pw )
{
}
static void GtkManageRelationalEnvs( gpointer *p, guint n, GtkWidget *pw )
{
}
#endif

#if USE_TIMECONTROL

extern void GTKCheckTimeControl( char *szName) {
   char path[128];
   sprintf(path ,"%s%s", N_("/Settings/Time Control/"), szName);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
	   gtk_item_factory_get_widget( pif, path)), TRUE);
}

extern void GTKAddTimeControl( char *szName) {
    char path[128];
    GtkItemFactoryEntry newEntry  = { NULL, NULL, Command, CMD_SET_TC, "/Settings/Time Control/Off"};
    sprintf(path ,"%s%s", N_("/Settings/Time Control/"), szName);
    newEntry.path = path;
    gtk_item_factory_create_items(pif, 1, &newEntry, szName); 
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
	   gtk_item_factory_get_widget( pif, path)), TRUE);
} 

extern void GTKRemoveTimeControl( char *szName) {
    char path[128];
    sprintf(path ,"%s%s", N_("/Settings/Time Control/"), szName);
    gtk_item_factory_delete_item(pif, path);
} 

typedef struct _definetcwidget {
	GtkWidget *pwName, *pwType, *pwTime, *pwPoint,
		*pwMove, *pwMult,  *pwPenalty,
		*pwNext, *pwNextB ;
} definetcwidget;

static GtkWidget *DefineTCWidget( definetcwidget *pdtcw) 
{
  GtkWidget *pwVbox, *pwHbox, *pwLabel;
  GtkWidget *pwFrame, *pwVbox2; 
  GtkWidget *pwMenu;

  pwVbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add( GTK_CONTAINER ( pwVbox ),
	pwFrame = gtk_frame_new(_("Not implemented - work in progress") ));
  gtk_container_add( GTK_CONTAINER ( pwFrame),
  	pwVbox2 = gtk_vbox_new(FALSE, 0));
  gtk_container_set_border_width( GTK_CONTAINER( pwVbox2 ), 8 );
  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time control name (tcname):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwName= gtk_entry_new() );

  pwMenu = gtk_menu_new();
  
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("None")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("Plain")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("Bronstein")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("Fischer")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("Hourglass")));
  gtk_widget_show_all(pwMenu);
  pdtcw->pwType = gtk_option_menu_new();
  gtk_option_menu_set_menu( GTK_OPTION_MENU( pdtcw->pwType ), pwMenu );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time control type (tctype):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ), pdtcw->pwType );

  pwMenu = gtk_menu_new();
  
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("Lose")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("1 point")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("2 points")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("3 points")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("4 points")));
  gtk_menu_append(GTK_MENU(pwMenu), (GtkWidget *) gtk_menu_item_new_with_label(_("5 points")));
  gtk_widget_show_all(pwMenu);
  pdtcw->pwPenalty = gtk_option_menu_new();
  gtk_option_menu_set_menu( GTK_OPTION_MENU( pdtcw->pwPenalty ), pwMenu );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Penalty type (tctype):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ), pdtcw->pwPenalty );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time to add (tctime):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwTime = gtk_entry_new() );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time to add (tctime):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwTime = gtk_entry_new() );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time to add / point (tcpoint):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwPoint = gtk_entry_new() );

  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Time to add / move (tcmove):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwMove = gtk_entry_new() );
  
  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Scale old time by (tcmult):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwMult = gtk_entry_new() );
  
  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Next time control (tcnext):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwNext = gtk_entry_new() );
  
  gtk_container_add( GTK_CONTAINER ( pwVbox2 ),
  	pwHbox = gtk_hbox_new(FALSE, 0));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pwLabel = gtk_label_new(_("Opponent's next time control (tcnext):")));
  gtk_container_add( GTK_CONTAINER ( pwHbox ),
	pdtcw->pwNextB = gtk_entry_new() );
  
  return pwVbox;
}

static void DefineTCOK( GtkWidget *pw, definetcwidget *pdtcw ) {
printf("Name is: %s\n", gtk_entry_get_text(GTK_ENTRY(pdtcw->pwName) ) );
     gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void DefineTimeControl( gpointer *p, guint n, GtkWidget *pw ) {
    
  GtkWidget *pwDialog, *pwPage;
  definetcwidget dtcw;

  pwDialog = GTKCreateDialog( _("GNU Backgammon - Define Time Control"),
                           DT_QUESTION, GTK_SIGNAL_FUNC( DefineTCOK ), &dtcw );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                        pwPage = DefineTCWidget(&dtcw));

  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );

  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  gtk_widget_show_all( pwDialog );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();
}

#endif
