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

#define  USES_badSkill

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
#define GTK_ENABLE_BROKEN /* for GtkText */
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#if HAVE_GDK_GDKX_H
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#endif
#include <gtk/gtktext.h>

#if HAVE_STROPTS_H
#include <stropts.h>  /* I_SETSIG, S_RDNORM under solaris */
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "glib.h"

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
#include "i18n.h"
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

#define GNUBGMENURC ".gnubgmenurc"

#if USE_GTK2
#define GTK_STOCK_DIALOG_GNU "gtk-dialog-gnu" /* stock gnu head icon */
#endif

#if !GTK_CHECK_VERSION(1,3,10)
#define gtk_widget_get_parent(w) ((w)->parent)
#define gtk_style_get_font(s) ((s)->font)

static void gtk_style_set_font( GtkStyle *ps, GdkFont *pf ) {

    ps->font = pf;
    gdk_font_ref( pf );
}
#endif

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
    CMD_DATABASE_DUMP,
    CMD_DATABASE_GENERATE,
    CMD_DATABASE_ROLLOUT,
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
    CMD_SHOW_EXPORT,
    CMD_SHOW_GAMMONVALUES,
    CMD_SHOW_MARKETWINDOW,
    CMD_SHOW_MATCHEQUITYTABLE,
    CMD_SHOW_KLEINMAN,
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
    CMD_XCOPY,
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
    "database dump",
    "database generate",
    "database rollout",
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
    "show export",
    "show gammonvalues",
    "show marketwindow",
    "show matchequitytable",
    "show kleinman",
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
    "train td",
    "xcopy"
};
enum { TOGGLE_GAMELIST = NUM_CMDS + 1, TOGGLE_ANNOTATION, TOGGLE_MESSAGE };

#if ENABLE_TRAIN_MENU
static void DatabaseExport( gpointer *p, guint n, GtkWidget *pw );
static void DatabaseImport( gpointer *p, guint n, GtkWidget *pw );
#endif
static void CopyAsGOL( gpointer *p, guint n, GtkWidget *pw );
static void EnterCommand( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameGam( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportGamePDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportGamePostScript( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameText( gpointer *p, guint n, GtkWidget *pw );
static void ExportHTMLImages( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchMat( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchPDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchPostScript( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchText( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionEPS( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionPNG( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionPos( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionGammOnLine( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionText( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionSnowieTxt( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionPDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionPostScript( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionText( gpointer *p, guint n, GtkWidget *pw );
static void ImportBKG( gpointer *p, guint n, GtkWidget *pw );
static void ImportMat( gpointer *p, guint n, GtkWidget *pw );
static void ImportOldmoves( gpointer *p, guint n, GtkWidget *pw );
static void ImportPos( gpointer *p, guint n, GtkWidget *pw );
static void ImportSGG( gpointer *p, guint n, GtkWidget *pw );
static void ImportTMG( gpointer *p, guint n, GtkWidget *pw );
static void ImportSnowieTxt( gpointer *p, guint n, GtkWidget *pw );
static void LoadCommands( gpointer *p, guint n, GtkWidget *pw );
static void LoadGame( gpointer *p, guint n, GtkWidget *pw );
static void LoadMatch( gpointer *p, guint n, GtkWidget *pw );
static void LoadPosition( gpointer *p, guint n, GtkWidget *pw );
static void NewDialog( gpointer *p, guint n, GtkWidget *pw );
#if 0
static void NewMatch( gpointer *p, guint n, GtkWidget *pw );
static void NewWeights( gpointer *p, guint n, GtkWidget *pw );
#endif
static void SaveGame( gpointer *p, guint n, GtkWidget *pw );
static void SaveMatch( gpointer *p, guint n, GtkWidget *pw );
static void SavePosition( gpointer *p, guint n, GtkWidget *pw );
static void SaveWeights( gpointer *p, guint n, GtkWidget *pw );
static void SetAnalysis( gpointer *p, guint n, GtkWidget *pw );
static void SetOptions( gpointer *p, guint n, GtkWidget *pw );
static void SetPlayers( gpointer *p, guint n, GtkWidget *pw );
static void ShowManual( gpointer *p, guint n, GtkWidget *pw );
static void ShowManualWeb( gpointer *p, guint n, GtkWidget *pw );
static void ReportBug( gpointer *p, guint n, GtkWidget *pw );
static void ShowFAQ( gpointer *p, guint n, GtkWidget *pw );
static void FinishMove( gpointer *p, guint n, GtkWidget *pw );
static void PythonShell( gpointer *p, guint n, GtkWidget *pw );
static void HideAllPanels ( gpointer *p, guint n, GtkWidget *pw );
static void ShowAllPanels ( gpointer *p, guint n, GtkWidget *pw );
static void TogglePanel ( gpointer *p, guint n, GtkWidget *pw );
static void FullScreenMode( gpointer *p, guint n, GtkWidget *pw );

/* A dummy widget that can grab events when others shouldn't see them. */
GtkWidget *pwGrab;
GtkWidget *pwOldGrab;

GtkWidget *pwBoard, *pwMain, *pwMenuBar;
GtkWidget *pwToolbar;
static GtkWidget *pwStatus, *pwProgress, *pwGameList, *pom,
    *pwAnalysis, *pwCommentary;
static GtkWidget *pwHint = NULL;
static GtkWidget *pwAnnotation = NULL;
static GtkWidget *pwMessage = NULL, *pwMessageText;
static GtkWidget *pwGame = NULL;
static moverecord *pmrAnnotation;
static GtkAccelGroup *pagMain;
GtkTooltips *ptt;
static GtkStyle *psGameList, *psCurrent;
static int yCurrent, xCurrent; /* highlighted row/col in game record */
static GtkItemFactory *pif;
guint nNextTurn = 0; /* GTK idle function */
static int cchOutput;
static guint idOutput, idProgress;
static list lOutput;
int fTTY = TRUE;
int fGUISetWindowPos = TRUE;

static guint nStdin, nDisabledCount = 1;

static char *szCopied; /* buffer holding copied data */

#if GTK_CHECK_VERSION(1,3,0)
static char *ToUTF8( unsigned char *sz ) {

    static unsigned char szOut[ 128 ], *pch;

    for( pch = szOut; *sz; sz++ )
	if( *sz < 0x80 )
	    *pch++ = *sz;
	else {
	    *pch++ = 0xC0 | ( *sz >> 6 );
	    *pch++ = 0x80 | ( *sz & 0x3F );
	}

    *pch = 0;

    return szOut;
}
#define TRANS(x) ToUTF8(x)
#else
#define TRANS(x) (x)
#endif


static void
setWindowGeometry ( GtkWidget *pw, const windowgeometry *pwg ) {

  if ( ! pw || !fGUISetWindowPos)
    return;

#if GTK_CHECK_VERSION(2,0,0)

  gtk_window_set_default_size ( GTK_WINDOW ( pw ),
                      ( pwg->nWidth > 0 ) ? pwg->nWidth : -1,
                      ( pwg->nHeight > 0 ) ? pwg->nHeight : -1 );

  gtk_window_move ( GTK_WINDOW ( pw ),
                    ( pwg->nPosX >= 0 ) ? pwg->nPosX : 0, 
                    ( pwg->nPosY >= 0 ) ? pwg->nPosY : 0 );

#else

  gtk_window_set_default_size( GTK_WINDOW( pw ), 
                               ( pwg->nWidth > 0 ) ? pwg->nWidth : -1,
                               ( pwg->nHeight > 0 ) ? pwg->nHeight : -1 );
  
  gtk_widget_set_uposition ( pw, 
                             ( pwg->nPosX >= 0 ) ? pwg->nPosX : 0, 
                             ( pwg->nPosY >= 0 ) ? pwg->nPosY : 0 );

#endif /* ! GTK 2.0 */

}

static void
getWindowGeometry ( windowgeometry *pwg, GtkWidget *pw ) {

#if GTK_CHECK_VERSION(2,0,0)

  if ( ! pw )
    return;

  gtk_window_get_position ( GTK_WINDOW ( pw ),
                            &pwg->nPosX, &pwg->nPosY );

  gtk_window_get_size ( GTK_WINDOW ( pw ),
                        &pwg->nWidth, &pwg->nHeight );

#else

  if ( ! pw || ! pw->window )
    return;

  gdk_window_get_position ( pw->window,
                            &pwg->nPosX, &pwg->nPosY );

  gdk_window_get_size ( pw->window,
                        &pwg->nWidth, &pwg->nHeight );

#endif /* ! GTK 2.0 */

}

extern void
UpdateGeometry ( const gnubgwindow gw ) {

  GtkWidget *pw = 0;

  switch ( gw ) {
  case WINDOW_MAIN:
    pw = pwMain;
    break;
  case WINDOW_ANNOTATION:
#if USE_OLD_LAYOUT
    pw = pwAnnotation;
#endif
    break;
  case WINDOW_HINT:
#if USE_OLD_LAYOUT
    pw = pwHint; 
#endif
    break;
  case WINDOW_GAME:
#if USE_OLD_LAYOUT
    pw = pwGame; 
#endif
    break;
  case WINDOW_MESSAGE:
#if USE_OLD_LAYOUT
    pw = pwMessage; 
#endif
    break;
  default:
    assert ( FALSE );
  }

  setWindowGeometry ( pw, &awg[ gw ] );

}

extern void
RefreshGeometries ( void ) {

  getWindowGeometry ( &awg[ WINDOW_MAIN ], pwMain );
#if USE_OLD_LAYOUT
  getWindowGeometry ( &awg[ WINDOW_ANNOTATION ], pwAnnotation );
  getWindowGeometry ( &awg[ WINDOW_HINT ], pwHint );
  getWindowGeometry ( &awg[ WINDOW_GAME ], pwGame );
  getWindowGeometry ( &awg[ WINDOW_MESSAGE ], pwMessage );
#endif
}

typedef struct {
  void *owner;
  unsigned id;
} grabstack;

#define GRAB_STACK_SIZE 128
grabstack GrabStack[GRAB_STACK_SIZE];
int GrabStackPointer = 0;

extern void GTKSuspendInput( monitor *pm ) {
    
  /* Grab events so that the board window knows this is a re-entrant
     call, and won't allow commands like roll, move or double. */
  if( ( pm->fGrab = !GTK_WIDGET_HAS_GRAB( pwGrab ) ) ) {
    assert ((GrabStackPointer >= 0) && 
	    (GrabStackPointer < (GRAB_STACK_SIZE - 1 )));
    
    gtk_grab_add( pwGrab );
    GrabStack[GrabStackPointer].owner = pm;
    GrabStack[GrabStackPointer++].id =
      pm->idSignal = gtk_signal_connect_after( GTK_OBJECT( pwGrab ),
					       "key-press-event",
					       GTK_SIGNAL_FUNC( gtk_true ),
					       NULL );
    
  }
  
  /* Don't check stdin here; readline isn't ready yet. */
  GTKDisallowStdin();
}

extern void GTKResumeInput( monitor *pm ) {
    
  int i;

  GTKAllowStdin();

  /* go backwards down the stack looking for this signal's entry 
     if found, disconnect the signal and trim the stack back
     this should cope with grab owners who disappear without 
     cleaning up
     */
  for (i = GrabStackPointer - 1; i >= 0; --i) {
    if ((GrabStack[ i ].owner == pm) &&
	  (GrabStack[ i ].id == pm->idSignal)) {

      gtk_signal_disconnect( GTK_OBJECT( pwGrab ), pm->idSignal );
      GrabStackPointer = i;
    }
  }

  if( pm->fGrab )
    gtk_grab_remove( pwGrab );
    
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

    monitor m;
    
    SuspendInput( &m );
    
    while( !fInterrupt && !fEndDelay )
	gtk_main_iteration();
    
    fEndDelay = FALSE;
    
    ResumeInput( &m );
}

extern void HandleXAction( void ) {

    monitor m;
    
    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    SuspendInput( &m );
    
    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( gtk_events_pending() )
	gtk_main_iteration();

    ResumeInput( &m );
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
	
    default:
	UserCommand( aszCommands[ iCommand ] );
    }
}

static void DestroySetDice( GtkObject *po, GtkWidget *pw ) {

    gtk_widget_destroy( pw );
}

extern int GTKGetManualDice( int an[ 2 ] ) {

    GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Dice"),
					DT_QUESTION, NULL, NULL ),
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

    int an[ 2 ];
    char sz[ 20 ]; /* "set cube value 4096" */
    GtkWidget *pwDialog, *pwCube;

    if( ms.gs != GAME_PLAYING || ms.fCrawford || !ms.fCubeUse )
	return;
	
    pwDialog = GTKCreateDialog( _("GNU Backgammon - Cube"),
			     DT_QUESTION, NULL, NULL );
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
			GTK_SIGNAL_FUNC( DestroySetDice ), pwDialog );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( an[ 0 ] < 0 )
	return;

    outputpostpone();
    
    if( 1 << an[ 0 ] != ms.nCube ) {
	sprintf( sz, "set cube value %d", 1 << an[ 0 ] );
	UserCommand( sz );
    }

    if( an[ 1 ] != ms.fCubeOwner ) {
	if( an[ 1 ] >= 0 ) {
	    sprintf( sz, "set cube owner %d", an[ 1 ] );
	    UserCommand( sz );
	} else
	    UserCommand( "set cube centre" );
    }
    
    outputresume();
}

typedef struct _gamelistrow {
    moverecord *apmr[ 2 ]; /* moverecord entries for each column */
    int fCombined; /* this message's row is combined across both columns */
} gamelistrow;

/* Find a moverecord, given an index into the game (0 = player 0's first
   move, 1 = player 1's first move, 2 = player 0's second move, etc.).
   This function maps based on what's in the list window, so "combined"
   rows (e.g. "set board") will occupy TWO entries in the sequence space. */
static moverecord *GameListLookupMove( int i ) {

    gamelistrow *pglr;
    moverecord *pmr;
    
    for( ; ( pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ),
					    i >> 1 ) ); i++ )
	if( ( pmr = pglr->apmr[ pglr->fCombined ? 0 : i & 1 ] ) )
	    return pmr;

    return NULL;
}


static void GameListSelectRow( GtkCList *pcl, gint y, gint x,
			       GdkEventButton *pev, gpointer p ) {
#if USE_BOARD3D
	BoardData *bd = BOARD( pwBoard )->board_data;
#endif
    gamelistrow *pglr;
    moverecord *pmr, *pmrPrev = NULL;
    list *pl;
    int i, iPrev;
    
    if( x < 1 || x > 2 )
	return;

    pglr = gtk_clist_get_row_data( pcl, y );
    i = ( pglr && pglr->fCombined ) ? y << 1 : ( y << 1 ) | ( x - 1 );
    pmr = GameListLookupMove( i );
    
    for( iPrev = i - 1; iPrev >= 0; iPrev-- )
	if( ( pmrPrev = GameListLookupMove( iPrev ) ) && pmrPrev != pmr )
	    break;

    if( !pmr && !pmrPrev )
	return;

    for( pl = plGame->plPrev; pl != plGame; pl = pl->plPrev ) {
	assert( pl->p );
	if( pl == plGame->plPrev && pl->p == pmr && pmr->mt == MOVE_SETDICE )
	    break;

	if( pl->p == pmrPrev && pmr != pmrPrev ) {
	    // pmr = pmrPrev;
	    break;
	} else if( pl->plNext->p == pmr ) {
	    //pmr = pl->p;
	    break;
	}
    }

    if( pl == plGame )
	/* couldn't find the moverecord they selected */
	return;
    
    plLastMove = pl;
    
    CalculateBoard();

    if ( pmr && pmr->mt == MOVE_NORMAL ) {
       /* roll dice */
       ms.gs = GAME_PLAYING;
       ms.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
       ms.anDice[ 1 ] = pmr->n.anRoll[ 1 ];
   }
#if USE_BOARD3D
	if (rdAppearance.fDisplayType == DT_3D)
	{	/* Make sure dice are shown (and not rolled) */
		bd->diceShown = DICE_ON_BOARD;
		bd->diceRoll[0] = !ms.anDice[0];
	}
#endif

    UpdateSetting( &ms.nCube );
    UpdateSetting( &ms.fCubeOwner );
    UpdateSetting( &ms.fTurn );
    UpdateSetting( &ms.gs );
    
    SetMoveRecord( pl->p );
    
    ShowBoard();
}

static void ButtonCommand( GtkWidget *pw, char *szCommand ) {

    UserCommand( szCommand );
}

static GtkWidget *PixmapButton( GdkColormap *pcmap, char **xpm,
				char *szCommand ) {
    
    GdkPixmap *ppm;
    GdkBitmap *pbm;
    GtkWidget *pw, *pwButton;

    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL, pcmap, &pbm, NULL,
						 xpm );
    pw = gtk_pixmap_new( ppm, pbm );
    pwButton = gtk_button_new();
    gtk_container_add( GTK_CONTAINER( pwButton ), pw );
    
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( ButtonCommand ), szCommand );
    
    return pwButton;
}

static void DeleteAnnotation( void ) {
#if USE_OLD_LAYOUT
  getWindowGeometry ( &awg[ WINDOW_ANNOTATION ], pwAnnotation );
#endif
  fAnnotation = FALSE;
#if USE_OLD_LAYOUT
  UpdateSetting(&fAnnotation);
#endif
  gtk_widget_hide ( pwAnnotation );
#if !USE_OLD_LAYOUT
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Annotation")), FALSE);
#endif
}

static gboolean DeleteGame( void ) {
#if USE_OLD_LAYOUT
  getWindowGeometry ( &awg[ WINDOW_GAME ], pwGame );
#else
  fGameList = FALSE;
#endif
  gtk_widget_hide ( pwGame );
#if !USE_OLD_LAYOUT
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Game record")), FALSE);
#endif
  return TRUE;
}

static int fAutoCommentaryChange;

static void CommentaryChanged( GtkWidget *pw, void *p ) {

    char *pch;
    
    if( fAutoCommentaryChange )
	return;

    assert( pmrAnnotation );

    /* FIXME Copying the entire text every time it's changed is horribly
       inefficient, but the only alternatives seem to be lazy copying
       (which is much harder to get right) or requiring a specific command
       to update the text (which is probably inconvenient for the user). */

    if( pmrAnnotation->a.sz )
	free( pmrAnnotation->a.sz );
    
    if( gtk_text_get_length( GTK_TEXT( pw ) ) ) {
	pch = gtk_editable_get_chars( GTK_EDITABLE( pw ), 0, -1 );
	/* This copy is absolutely disgusting, but is necessary because GTK
	   insists on giving us something allocated with g_malloc() instead
	   of malloc(). */
	pmrAnnotation->a.sz = strdup( pch );
	g_free( pch );

#ifndef WIN32
	/* Strip trailing whitespace from the copy. */
	for( pch = strchr( pmrAnnotation->a.sz, 0 ) - 1;
	     pch > pmrAnnotation->a.sz && isspace( *pch ); pch-- )
	    *pch = 0;
#endif

    } else
	pmrAnnotation->a.sz = NULL;
}

static void DeleteMessage ( void ) {
#if USE_OLD_LAYOUT
  getWindowGeometry ( &awg[ WINDOW_MESSAGE ], pwMessage );
#endif
  fMessage = FALSE;
#if USE_OLD_LAYOUT
  UpdateSetting( &fMessage );
#endif
  gtk_widget_hide ( pwMessage );
#if !USE_OLD_LAYOUT
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Message")), FALSE);
#endif
}

static GtkWidget *CreateMessageWindow( void ) {

    GtkWidget *vscrollbar, *pwhbox;  
    GtkWidget *pwvbox = gtk_vbox_new ( TRUE, 0 ) ;
#if USE_OLD_LAYOUT
    GtkWidget *pwMessage = gtk_window_new( GTK_WINDOW_TOPLEVEL );


    gtk_window_set_title( GTK_WINDOW( pwMessage ),
			  _("GNU Backgammon - Messages") );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_window_set_role( GTK_WINDOW( pwMessage ), "messages" );
#if GTK_CHECK_VERSION(2,2,0)
    gtk_window_set_type_hint( GTK_WINDOW( pwMessage ),
			      GDK_WINDOW_TYPE_HINT_UTILITY );
#endif
#endif
    
    setWindowGeometry ( pwMessage, &awg[ WINDOW_MESSAGE ] );
    gtk_container_add( GTK_CONTAINER( pwMessage ), pwvbox);
#endif 

    gtk_box_pack_start ( GTK_BOX ( pwvbox ), 
                     pwhbox = gtk_hbox_new ( FALSE, 0 ), FALSE, TRUE, 0);
    
    pwMessageText = gtk_text_new ( NULL, NULL );
    
    gtk_text_set_word_wrap( GTK_TEXT( pwMessageText ), TRUE );
    gtk_text_set_editable( GTK_TEXT( pwMessageText ), FALSE );

    vscrollbar = gtk_vscrollbar_new (GTK_TEXT(pwMessageText)->vadj);
    gtk_box_pack_start(GTK_BOX(pwhbox), pwMessageText, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(pwhbox), vscrollbar, FALSE, FALSE, 0);
#if USE_OLD_LAYOUT
    gtk_signal_connect( GTK_OBJECT( pwMessage ), "delete_event",
			GTK_SIGNAL_FUNC( DeleteMessage ), NULL );
    return pwMessage;
#else
    gtk_widget_set_usize(GTK_WIDGET(pwvbox), 0, 80);
    return pwvbox;
#endif
}

static GtkWidget *CreateAnnotationWindow( void ) {

    GtkWidget *pwPaned = gtk_vpaned_new() ;
    GtkWidget *vscrollbar, *pHbox;    

#if USE_OLD_LAYOUT
    GtkWidget *pwAnnotation = gtk_window_new( GTK_WINDOW_TOPLEVEL );

    gtk_window_set_title( GTK_WINDOW( pwAnnotation ),
			  _("GNU Backgammon - Annotation") );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_window_set_role( GTK_WINDOW( pwAnnotation ), "annotation" );
#if GTK_CHECK_VERSION(2,2,0)
    gtk_window_set_type_hint( GTK_WINDOW( pwAnnotation ),
			      GDK_WINDOW_TYPE_HINT_UTILITY );
#endif
#endif

    setWindowGeometry ( pwAnnotation, &awg[ WINDOW_ANNOTATION ] );

    gtk_container_add( GTK_CONTAINER( pwAnnotation ), pwPaned); 
#endif
    
    gtk_paned_pack1( GTK_PANED( pwPaned ),
		     pwAnalysis = gtk_label_new( NULL ), TRUE, FALSE );
    
    gtk_paned_pack2( GTK_PANED( pwPaned ),
		     pHbox = gtk_hbox_new( FALSE, 0 ), FALSE, TRUE );
    
    pwCommentary = gtk_text_new( NULL, NULL ) ;

    gtk_text_set_word_wrap( GTK_TEXT( pwCommentary ), TRUE );
    gtk_text_set_editable( GTK_TEXT( pwCommentary ), TRUE );
    gtk_widget_set_sensitive( pwCommentary, FALSE );

    
    vscrollbar = gtk_vscrollbar_new (GTK_TEXT(pwCommentary)->vadj);
    gtk_box_pack_start(GTK_BOX(pHbox), pwCommentary, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(pHbox), vscrollbar, FALSE, FALSE, 0);

    gtk_signal_connect( GTK_OBJECT( pwCommentary ), "changed",
			GTK_SIGNAL_FUNC( CommentaryChanged ), NULL );
#if USE_OLD_LAYOUT    
    gtk_signal_connect( GTK_OBJECT( pwAnnotation ), "delete_event",
			GTK_SIGNAL_FUNC( DeleteAnnotation ), NULL );
    return pwAnnotation;
#else
    return pwPaned;
#endif
}

static GtkWidget *CreateGameWindow( void ) {

    static char *asz[] = { NULL, NULL, NULL };
    GtkWidget *psw = gtk_scrolled_window_new( NULL, NULL ),
	*pvbox = gtk_vbox_new( FALSE, 0 ),
	*phbox = gtk_hbox_new( FALSE, 0 ),
	*pm = gtk_menu_new(),
	*pw;
    GtkStyle *ps;
    GdkColormap *pcmap;
    gint nMaxWidth; 

#include "prevgame.xpm"
#include "prevmove.xpm"
#include "nextmove.xpm"
#include "nextgame.xpm"
#include "prevmarked.xpm"
#include "nextmarked.xpm"

    pcmap = gtk_widget_get_colormap( pwMain );
#if USE_OLD_LAYOUT 
    pwGame = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    
    gtk_window_set_title( GTK_WINDOW( pwGame ), _("GNU Backgammon - "
			  "Game record") );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_window_set_role( GTK_WINDOW( pwGame ), "game record" );
#if GTK_CHECK_VERSION(2,2,0)
    gtk_window_set_type_hint( GTK_WINDOW( pwGame ),
			      GDK_WINDOW_TYPE_HINT_UTILITY );
#endif
#endif

    setWindowGeometry ( pwGame, &awg[ WINDOW_GAME ] );
    
    gtk_container_add( GTK_CONTAINER( pwGame ), pvbox );
#endif
    gtk_box_pack_start( GTK_BOX( pvbox ), phbox, FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, prevgame_xpm,
					   "previous game" ),
			FALSE, FALSE, 4 );
    gtk_tooltips_set_tip( ptt, pw, _("Move back to the previous game"), "" );
    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, prevmove_xpm,
					   "previous roll" ),
			FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pw, _("Move back to the previous roll"), "" );
    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, nextmove_xpm,
					   "next roll" ),
			FALSE, FALSE, 4 );
    gtk_tooltips_set_tip( ptt, pw, _("Move ahead to the next roll"), "" );
    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, nextgame_xpm,
				      "next game" ),
			FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pw, _("Move ahead to the next game"), "" );

    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, prevmarked_xpm,
					   "previous marked" ),
			FALSE, FALSE, 4 );
    gtk_tooltips_set_tip( ptt, pw, _("Move back to the previous marked "
				     "decision" ), "" );
    gtk_box_pack_start( GTK_BOX( phbox ),
			pw = PixmapButton( pcmap, nextmarked_xpm,
					   "next marked" ),
			FALSE, FALSE, 0 );
    gtk_tooltips_set_tip( ptt, pw, _("Move ahead to the next marked "
				     "decision" ), "" );
        
    gtk_menu_append( GTK_MENU( pm ), gtk_menu_item_new_with_label(
	_("(no game)") ) );
    gtk_widget_show_all( pm );
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pom = gtk_option_menu_new() ),
			      pm );
    gtk_option_menu_set_history( GTK_OPTION_MENU( pom ), 0 );
    gtk_box_pack_start( GTK_BOX( phbox ), pom, TRUE, TRUE, 4 );
    
    gtk_container_add( GTK_CONTAINER( pvbox ), psw );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

    asz[ 0 ] = _( "#" );
    gtk_container_add( GTK_CONTAINER( psw ),
		       pwGameList = gtk_clist_new_with_titles( 3, asz ) );

    gtk_clist_set_selection_mode( GTK_CLIST( pwGameList ),
				  GTK_SELECTION_BROWSE );
    gtk_clist_column_titles_passive( GTK_CLIST( pwGameList ) );

    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 1, 
                                ( ap[0].szName ));
    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 2, 
                                ( ap[1].szName ));

    gtk_clist_set_column_justification( GTK_CLIST( pwGameList ), 0,
					GTK_JUSTIFY_RIGHT );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 0, FALSE );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 1, FALSE );
    gtk_clist_set_column_resizeable( GTK_CLIST( pwGameList ), 2, FALSE );

    gtk_widget_ensure_style( pwGameList );
    ps = gtk_style_new();
    ps->base[ GTK_STATE_SELECTED ] =
	ps->base[ GTK_STATE_ACTIVE ] =
	ps->base[ GTK_STATE_NORMAL ] =
	pwGameList->style->base[ GTK_STATE_NORMAL ];
    ps->fg[ GTK_STATE_SELECTED ] =
	ps->fg[ GTK_STATE_ACTIVE ] =
	ps->fg[ GTK_STATE_NORMAL ] =
	pwGameList->style->fg[ GTK_STATE_NORMAL ];
    gtk_style_set_font( ps, gtk_style_get_font( pwGameList->style ) );
    gtk_widget_set_style( pwGameList, ps );
    
    psGameList = gtk_style_copy( ps );
    psGameList->bg[ GTK_STATE_SELECTED ] = psGameList->bg[ GTK_STATE_NORMAL ] =
	ps->base[ GTK_STATE_NORMAL ];

    psCurrent = gtk_style_copy( psGameList );
    psCurrent->bg[ GTK_STATE_SELECTED ] = psCurrent->bg[ GTK_STATE_NORMAL ] =
	psCurrent->base[ GTK_STATE_SELECTED ] =
	psCurrent->base[ GTK_STATE_NORMAL ] =
	psGameList->fg[ GTK_STATE_NORMAL ];
    psCurrent->fg[ GTK_STATE_SELECTED ] = psCurrent->fg[ GTK_STATE_NORMAL ] =
	psGameList->bg[ GTK_STATE_NORMAL ];

    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ), _("99") );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 0, nMaxWidth );
    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ),
                                  " (set board AAAAAAAAAAAAAA)");
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 1, nMaxWidth - 30);
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 2, nMaxWidth - 30);
    
    gtk_signal_connect( GTK_OBJECT( pwGameList ), "select-row",
			GTK_SIGNAL_FUNC( GameListSelectRow ), NULL );
#if USE_OLD_LAYOUT
    gtk_signal_connect( GTK_OBJECT( pwGame ), "delete_event",
			GTK_SIGNAL_FUNC( DeleteGame ), NULL );
    return pwGame;
#else
    gtk_widget_set_usize(GTK_WIDGET(pvbox), 0, 300);
    return pvbox;
#endif
}

extern void ShowGameWindow( void ) {
#if USE_OLD_LAYOUT
  setWindowGeometry ( pwGame, &awg[ WINDOW_GAME ] );
    gtk_widget_show_all( pwGame );
    if( pwGame->window )
	gdk_window_raise( pwGame->window );
#else
    fGameList = TRUE;
    gtk_widget_show_all( pwGame );
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Game record")), TRUE);
#endif
}
static void ShowAnnotation( void ) {
    fAnnotation = TRUE;
    gtk_widget_show_all( pwAnnotation );
#if !USE_OLD_LAYOUT
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Annotation")), TRUE);
#endif
}
static void ShowMessage( void ) {
    fMessage = TRUE;
    gtk_widget_show_all( pwMessage );
#if !USE_OLD_LAYOUT
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif,
			  "/Windows/Message")), TRUE);
#endif
}

static int AddMoveRecordRow( void ) {

    gamelistrow *pglr;
    static char *aszEmpty[] = { NULL, NULL, NULL };
    char szIndex[ 5 ];    
    int i = gtk_clist_append( GTK_CLIST( pwGameList ), aszEmpty );
    
    sprintf( szIndex, "%d", GTK_CLIST( pwGameList )->rows );
    gtk_clist_set_text( GTK_CLIST( pwGameList ), i, 0, szIndex );
    gtk_clist_set_row_style( GTK_CLIST( pwGameList ), i, psGameList );
    gtk_clist_set_row_data_full( GTK_CLIST( pwGameList ), i,
				 pglr = malloc( sizeof( *pglr ) ), free );
    pglr->fCombined = FALSE;
    pglr->apmr[ 0 ] = pglr->apmr[ 1 ] = NULL;

    return i;
}

/* Add a moverecord to the game list window.  NOTE: This function must be
   called _before_ applying the moverecord, so it can be displayed
   correctly. */
extern void GTKAddMoveRecord( moverecord *pmr ) {

    gamelistrow *pglr;
    int i, fPlayer = 0;
    char *pch = 0;
    char sz[ 40 ];
    doubletype dt;
    
    switch( pmr->mt ) {
    case MOVE_GAMEINFO:
	/* no need to list this */
	return;

    case MOVE_NORMAL:
	fPlayer = pmr->n.fPlayer;
	pch = sz;
	sz[ 0 ] = pmr->n.anRoll[ 0 ] + '0';
	sz[ 1 ] = pmr->n.anRoll[ 1 ] + '0';
	sz[ 2 ] = ':';
	sz[ 3 ] = ' ';
	FormatMove( sz + 4, ms.anBoard, pmr->n.anMove );
	strcat( sz, aszSkillTypeAbbr[ pmr->n.stMove ] );
	strcat( sz, aszSkillTypeAbbr[ pmr->n.stCube ] );
	break;

    case MOVE_DOUBLE:
	fPlayer = pmr->d.fPlayer;
	pch = sz;

        switch ( ( dt = DoubleType ( ms.fDoubled, ms.fMove, ms.fTurn ) ) ) {
        case DT_NORMAL:
          sprintf( sz, ( ms.fCubeOwner == -1 )? 
                   _("Double to %d") : _("Redouble to %d"), 
                   ms.nCube << 1 );
          break;
        case DT_BEAVER:
        case DT_RACCOON:
          sprintf( sz, ( dt == DT_BEAVER ) ? 
                   _("Beaver to %d") : _("Raccoon to %d"), ms.nCube << 2 );
          break;
        default:
          assert ( FALSE );
          break;
        }
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_TAKE:
	fPlayer = pmr->d.fPlayer;
	strcpy( pch = sz, _("Take") );
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_DROP:
	fPlayer = pmr->d.fPlayer;
	strcpy( pch = sz, _("Drop") );
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_RESIGN:
	fPlayer = pmr->r.fPlayer;
	pch = _(" Resigns"); /* FIXME show value */
	break;

    case MOVE_SETDICE:
	fPlayer = pmr->sd.fPlayer;
	sprintf( pch = sz, "Rolled %d%d", pmr->sd.anDice[ 0 ],
		 pmr->sd.anDice[ 1 ] );
	break;

    case MOVE_SETBOARD:
	fPlayer = -1;
	sprintf( pch = sz, " (set board %s)",
		 PositionIDFromKey( pmr->sb.auchKey ) );
	break;

    case MOVE_SETCUBEPOS:
	fPlayer = -1;
	if( pmr->scp.fCubeOwner < 0 )
	    pch = " (set cube centre)";
	else
	    sprintf( pch = sz, " (set cube owner %s)",
		     ap[ pmr->scp.fCubeOwner ].szName );
	break;

    case MOVE_SETCUBEVAL:
	fPlayer = -1;
	sprintf( pch = sz, " (set cube value %d)", pmr->scv.nCube );
	break;
	
    default:
	assert( FALSE );
    }

    if( !GTK_CLIST( pwGameList )->rows || 
	( pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ),
					 GTK_CLIST( pwGameList )->rows - 1 ) )
	->fCombined || pglr->apmr[ 1 ] || ( fPlayer != 1 && pglr->apmr[ 0 ] ) )
	i = AddMoveRecordRow();
    else
	i = GTK_CLIST( pwGameList )->rows - 1;
    
    pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ), i );

    if( ( pglr->fCombined = fPlayer == -1 ) )
	fPlayer = 0;

    pglr->apmr[ fPlayer ] = pmr;
    
    gtk_clist_set_text( GTK_CLIST( pwGameList ), i, fPlayer + 1, pch );
}

extern void GTKFreeze( void ) {

    gtk_clist_freeze( GTK_CLIST( pwGameList ) );
}

extern void GTKThaw( void ) {

    gtk_clist_thaw( GTK_CLIST( pwGameList ) );
}

extern void GTKPopMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr = NULL;
    
    gtk_clist_freeze( pcl );

    while( pcl->rows ) {
	pglr = gtk_clist_get_row_data( pcl, pcl->rows - 1 );
	
	if( pglr->apmr[ 0 ] != pmr && pglr->apmr[ 1 ] != pmr )
	    gtk_clist_remove( pcl, pcl->rows - 1);
	else
	    break;
    }

    if( pcl->rows ) {
	if( pglr->apmr[ 0 ] == pmr )
	    /* the left column matches; delete the row */
	    gtk_clist_remove( pcl, pcl->rows - 1 );
	else {
	    /* the right column matches; delete that column only */
	    gtk_clist_set_text( pcl, pcl->rows - 1, 2, NULL );
	    pglr->apmr[ 1 ] = NULL;
	}
    }
    
    gtk_clist_thaw( pcl );
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

static void SetAnnotation( moverecord *pmr ) {

    GtkWidget *pwParent = pwAnalysis->parent, *pw = NULL, *pwBox, *pwAlign;
    int fMoveOld, fTurnOld;
    list *pl;
    char sz[ 64 ];
    GtkWidget *pwMoveAnalysis = NULL, *pwCubeAnalysis = NULL;
    doubletype dt;
    taketype tt;
    cubeinfo ci;
    
    /* Select the moverecord _after_ pmr.  FIXME this is very ugly! */
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
    gtk_editable_delete_text( GTK_EDITABLE( pwCommentary ), 0, -1 );
    fAutoCommentaryChange = FALSE;
    
    if( pmr ) {
	if( pmr->a.sz ) {
	    fAutoCommentaryChange = TRUE;
	    gtk_text_insert( GTK_TEXT( pwCommentary ), NULL, NULL, NULL,
			     (pmr->a.sz), -1 );
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

#if 0
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				RollAnalysis( pmr->n.anRoll[ 0 ],
					      pmr->n.anRoll[ 1 ],
					      pmr->n.rLuck, pmr->n.lt ),
				FALSE, FALSE, 4 );
#endif

	    ms.fMove = ms.fTurn = pmr->n.fPlayer;

            /* 
             * Skill and luck
             */

            /* Skill for cube */

            GetMatchStateCubeInfo ( &ci, &ms );
            if ( GetDPEq ( NULL, NULL, &ci ) ) {
#if 0
              gtk_box_pack_start ( GTK_BOX ( pwBox ),
                                   gtk_label_new ( _("Didn't double") ),
                                   FALSE, FALSE, 4 );
              gtk_box_pack_start ( GTK_BOX ( pwBox ),
                                   SkillMenu ( pmr->n.stCube, "cube" ),
                                   FALSE, FALSE, 4 );

#endif

              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   gtk_label_new ( _("Didn't double") ),
                                   0, 1, 0, 1 );
#if ANALYSIS_HORIZONTAL
              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   SkillMenu ( pmr->n.stCube, "cube" ),
                                   1, 2, 0, 1 );
#else
              gtk_table_attach_defaults( GTK_TABLE ( pwBox ),
                                   SkillMenu ( pmr->n.stCube, "cube" ),
                                   0, 1, 1, 2 );
#endif
            }

            /* luck */

	{
    char sz[ 64 ], *pch;
    cubeinfo ci;
    GtkWidget *pwMenu, *pwOptionMenu, *pwItem;
    lucktype lt;
    
    pch = sz + sprintf( sz, _("Rolled %d%d"), pmr->n.anRoll[0], pmr->n.anRoll[1] );
    
    if( pmr->n.rLuck != ERR_VAL ) {
	if( fOutputMWC && ms.nMatchTo ) {
	    GetMatchStateCubeInfo( &ci, &ms );
	    pch += sprintf( pch, " (%+0.3f%%)",
	     100.0f * ( eq2mwc( pmr->n.rLuck, &ci ) - eq2mwc( 0.0f, &ci ) ) );
	} else
	    pch += sprintf( pch, " (%+0.3f)", pmr->n.rLuck );
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
    gtk_option_menu_set_history( GTK_OPTION_MENU( pwOptionMenu ), pmr->n.lt );

    gtk_table_attach_defaults( GTK_TABLE( pwBox ), pwOptionMenu, 1, 2, 1, 2 );

	}

            /* chequer play skill */
#if 0
	    gtk_box_pack_end( GTK_BOX( pwBox ), 
                              SkillMenu( pmr->n.stMove, "move" ),
			      FALSE, FALSE, 4 );
	    strcpy( sz, _("Moved ") );
	    FormatMove( sz + 6, ms.anBoard, pmr->n.anMove );
	    gtk_box_pack_end( GTK_BOX( pwBox ),
			      gtk_label_new( sz ), FALSE, FALSE, 0 );

#endif

	    strcpy( sz, _("Moved ") );
	    FormatMove( sz + 6, ms.anBoard, pmr->n.anMove );

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

            pwCubeAnalysis = CreateCubeAnalysis( pmr->n.aarOutput, 
                                                 pmr->n.aarStdDev,
                                                 &pmr->n.esDouble, 
                                                 MOVE_NORMAL );


            /* move */
			      
	    if( pmr->n.ml.cMoves ) 
              pwMoveAnalysis = CreateMoveList( &pmr->n.ml, &pmr->n.iMove,
                                               TRUE, FALSE );


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
              gtk_box_pack_start ( GTK_BOX ( pwAnalysis ),
                                   pwMoveAnalysis, TRUE, TRUE, 0 );
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
                                SkillMenu( pmr->d.st, "double" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );

            if ( dt == DT_NORMAL ) {
	    
              if ( ( pw = CreateCubeAnalysis ( pmr->d.CubeDecPtr->aarOutput,
                                               pmr->d.CubeDecPtr->aarStdDev,
                                               &pmr->d.CubeDecPtr->esDouble,
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
                                SkillMenu( pmr->d.st, 
                                           ( pmr->d.mt == MOVE_TAKE ) ?
                                           "take" : "drop" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );

            if ( tt == TT_NORMAL ) {
              if ( ( pw = CreateCubeAnalysis ( pmr->d.CubeDecPtr->aarOutput,
                                               pmr->d.CubeDecPtr->aarStdDev,
                                               &pmr->d.CubeDecPtr->esDouble,
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
	    pwAnalysis = RollAnalysis( pmr->sd.anDice[ 0 ],
				       pmr->sd.anDice[ 1 ],
				       pmr->sd.rLuck, pmr->sd.lt );
	    break;

	default:
	    break;
	}
    }
	
    if( !pwAnalysis )
	pwAnalysis = gtk_label_new( _("No analysis available.") );
    
    gtk_paned_pack1( GTK_PANED( pwParent ), pwAnalysis, TRUE, FALSE );
    gtk_widget_show_all( pwAnalysis );


    if ( pmr && pmr->mt == MOVE_NORMAL && pwMoveAnalysis && pwCubeAnalysis ) {

      /* (FIXME) Not sure here about SKILL_GOOD */
      if ( badSkill(pmr->n.stCube) )
        gtk_notebook_set_page ( GTK_NOTEBOOK ( pw ), 1 );


    }

}


/* Select a moverecord as the "current" one.  NOTE: This function must be
   called _after_ applying the moverecord. */
extern void GTKSetMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr;
    int i;

    SetAnnotation( pmr );

    if( pwHint ) {

#ifdef UNDEF
	hintdata *phd = gtk_object_get_user_data( GTK_OBJECT( pwHint ) );

	phd->fButtonsValid = FALSE;
	CheckHintButtons( phd );
#endif
    }
    
    gtk_clist_set_cell_style( pcl, yCurrent, xCurrent, psGameList );

    yCurrent = xCurrent = -1;

    if( !pmr )
	return;
    
    if( pmr == plGame->plNext->p ) {
	assert( pmr->mt == MOVE_GAMEINFO );
	yCurrent = 0;
	
	if( plGame->plNext->plNext->p ) {
	    moverecord *pmrNext = plGame->plNext->plNext->p;

	    if( pmrNext->mt == MOVE_NORMAL && pmrNext->n.fPlayer == 1 )
		xCurrent = 2;
	    else
		xCurrent = 1;
	} else
	    xCurrent = 1;
    } else {
	for( i = pcl->rows - 1; i >= 0; i-- ) {
	    pglr = gtk_clist_get_row_data( pcl, i );
	    if( pglr->apmr[ 1 ] == pmr ) {
		xCurrent = 2;
		break;
	    } else if( pglr->apmr[ 0 ] == pmr ) {
		xCurrent = 1;
		break;
	    }
	}
	
	yCurrent = i;
	
	if( yCurrent >= 0 && !( pmr->mt == MOVE_SETDICE &&
				yCurrent == pcl->rows - 1 ) ) {
	    do {
		if( ++xCurrent > 2 ) {
		    xCurrent = 1;
		    yCurrent++;
		}

		pglr = gtk_clist_get_row_data( pcl, yCurrent );
	    } while( yCurrent < pcl->rows - 1 && !pglr->apmr[ xCurrent - 1 ] );
	    
	    if( yCurrent >= pcl->rows )
		AddMoveRecordRow();
	}
    }

    gtk_clist_set_cell_style( pcl, yCurrent, xCurrent, psCurrent );

    if( gtk_clist_row_is_visible( pcl, yCurrent ) == GTK_VISIBILITY_NONE )
	gtk_clist_moveto( pcl, yCurrent, xCurrent, 0.5, 0.5 );
}

extern void GTKClearMoveRecord( void ) {

    gtk_clist_clear( GTK_CLIST( pwGameList ) );
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
    
    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 1, 
                                ( ap[0].szName ));
    gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 2, 
                                ( ap[1].szName ));

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

#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 15 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 15 );
#else
    char sz[ 4096 ];
#endif

#if GTK_CHECK_VERSION(1,3,15)
    sprintf( sz, "%s/" GNUBGMENURC, szHomeDirectory );
    gtk_accel_map_save( sz );
#else
    GtkPatternSpec ps;

    gtk_pattern_spec_init( &ps, "*" );
    
    sprintf( sz, "%s/" GNUBGMENURC, szHomeDirectory );
    gtk_item_factory_dump_rc( sz, &ps, TRUE );

    gtk_pattern_spec_free_segs( &ps );
#endif
}

static gboolean main_delete( GtkWidget *pw ) {

  getWindowGeometry ( &awg[ WINDOW_MAIN ], pw );
    
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

static void MainSize( GtkWidget *pw, GtkRequisition *preq, gpointer p ) {

    /* Give the main window a size big enough that the board widget gets
       board_size=4, if it will fit on the screen. */
    
    if( GTK_WIDGET_REALIZED( pw ) )
	gtk_signal_disconnect_by_func( GTK_OBJECT( pw ),
				       GTK_SIGNAL_FUNC( MainSize ), p );
    else if ( awg[ WINDOW_MAIN ].nWidth && awg[ WINDOW_MAIN ].nHeight )
	gtk_window_set_default_size( GTK_WINDOW( pw ),
                                     awg[ WINDOW_MAIN ].nWidth,
                                     awg[ WINDOW_MAIN ].nHeight );
    else
	gtk_window_set_default_size( GTK_WINDOW( pw ),
				     MAX( 480, preq->width ),
				     MIN( preq->height + 79 * 3,
					  gdk_screen_height() - 20 ) );
}


static gchar *GTKTranslate ( const gchar *path, gpointer func_data ) {

  return (gchar *) gettext ( (const char *) path );

}


static void
MainGetSelection ( GtkWidget *pw, GtkSelectionData *psd,
                   guint n, guint t, void *unused ) {

#ifdef WIN32

  if ( szCopied )
    TextToClipboard ( szCopied );

#else /* WIN32 */

  if ( szCopied )
    gtk_selection_data_set ( psd, GDK_SELECTION_TYPE_STRING, 8,
                             szCopied, strlen ( szCopied ) );

#endif /* WIN32 */
 
}

static void
MainSelectionReceived ( GtkWidget *pw, GtkSelectionData *data,
                        guint time, gpointer user_data ) {

  if ( data->length < 0 ) 
    /* no data */
    return;

  if ( data->type != GDK_SELECTION_TYPE_STRING )
    /* no of type string */
    return;

  if ( szCopied )
    free ( szCopied );

  szCopied = strdup ( data->data );

}


extern void
GTKTextToClipboard( const char *sz ) {

  /* copy text into global pointer used by MainGetSelection */

  if ( szCopied )
    free ( szCopied );

  szCopied = strdup( sz );

  /* claim clipboard and primary selection */

  gtk_selection_owner_set ( pwMain, gdk_atom_intern ("CLIPBOARD", FALSE), 
                            GDK_CURRENT_TIME );
  gtk_selection_owner_set ( pwMain, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME );

}

extern int InitGTK( int *argc, char ***argv ) {
    
    GtkWidget *pwVbox, *pwHbox, *pwHandle; 
#if !USE_OLD_LAYOUT
    GtkWidget *pwVboxRight;
#endif
    static GtkItemFactoryEntry aife[] = {
	{ N_("/_File"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_New"), "<control>N", NewDialog, 0, NULL },
#if 0
	{ N_("/_File/_New/_Game"), "<control>N", Command, CMD_NEW_GAME, NULL },
	{ N_("/_File/_New/_Match..."), NULL, NewMatch, 0, NULL },
	{ N_("/_File/_New/_Session"), NULL, Command, CMD_NEW_SESSION, NULL },
	{ N_("/_File/_New/_Weights..."), NULL, NewWeights, 0, NULL },
#endif
	{ N_("/_File/_Open"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Open/_Commands..."), NULL, LoadCommands, 0, NULL },
	{ N_("/_File/_Open/_Game..."), NULL, LoadGame, 0, NULL },
	{ N_("/_File/_Open/_Match or session..."), "<control>O", 
          LoadMatch, 0, NULL },
	{ N_("/_File/_Open/_Position..."), NULL, LoadPosition, 0, NULL },
	{ N_("/_File/_Save"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Save/_Game..."), NULL, SaveGame, 0, NULL },
	{ N_("/_File/_Save/_Match or session..."), "<control>S", 
          SaveMatch, 0, NULL },
	{ N_("/_File/_Save/_Position..."), NULL, SavePosition, 0, NULL },
	{ N_("/_File/_Save/_Weights..."), NULL, SaveWeights, 0, NULL },
	{ N_("/_File/-"), NULL, NULL, 0, "<Separator>" },	
	{ N_("/_File/_Import"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Import/BKG session..."), NULL, ImportBKG, 0, NULL },
	{ N_("/_File/_Import/._mat match..."), NULL, ImportMat, 0, NULL },
	{ N_("/_File/_Import/._pos position..."), NULL, ImportPos, 0, NULL },
	{ N_("/_File/_Import/FIBS _oldmoves..."), 
          NULL, ImportOldmoves, 0, NULL },
	{ N_("/_File/_Import/._GamesGrid .sgg match..."), 
          NULL, ImportSGG, 0, NULL },
	{ N_("/_File/_Import/._TrueMoneyGames .tmg match..."), 
          NULL, ImportTMG, 0, NULL },
	{ N_("/_File/_Import/._Snowie standard text format..."), 
          NULL, ImportMat, 0, NULL },
	{ N_("/_File/_Import/._Snowie .txt position file..."), 
          NULL, ImportSnowieTxt, 0, NULL },
	{ N_("/_File/_Export"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Export/_Game"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Export/_Game/.gam..."), NULL, ExportGameGam, 0, NULL },
	{ N_("/_File/_Export/_Game/HTML..."), NULL, ExportGameHtml, 0, NULL },
	{ N_("/_File/_Export/_Game/LaTeX..."), NULL, ExportGameLaTeX, 0, NULL },
	{ N_("/_File/_Export/_Game/PDF..."), NULL, ExportGamePDF, 0, NULL },
	{ N_("/_File/_Export/_Game/PostScript..."), 
          NULL, ExportGamePostScript, 0, NULL },
	{ N_("/_File/_Export/_Game/Text..."), 
          NULL, ExportGameText, 0, NULL },
	{ N_("/_File/_Export/_Match"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Export/_Match/HTML..."), 
          NULL, ExportMatchHtml, 0, NULL },
	{ N_("/_File/_Export/_Match/LaTeX..."), 
          NULL, ExportMatchLaTeX, 0, NULL },
	{ N_("/_File/_Export/_Match/.mat..."), NULL, ExportMatchMat, 0, NULL },
	{ N_("/_File/_Export/_Match/PDF..."), NULL, ExportMatchPDF, 0, NULL },
	{ N_("/_File/_Export/_Match/PostScript..."), 
          NULL, ExportMatchPostScript, 0, NULL },
	{ N_("/_File/_Export/_Match/Text..."), 
          NULL, ExportMatchText, 0, NULL },
	{ N_("/_File/_Export/_Position"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Export/_Position/HTML..."), NULL,
	  ExportPositionHtml, 0, NULL },
	{ N_("/_File/_Export/_Position/GammOnLine (HTML)..."), NULL,
	  ExportPositionGammOnLine, 0, NULL },
	{ N_("/_File/_Export/_Position/Encapsulated PostScript..."), NULL,
	  ExportPositionEPS, 0, NULL },
	{ N_("/_File/_Export/_Position/PNG..."), NULL, ExportPositionPNG, 0,
	  NULL },
	{ N_("/_File/_Export/_Position/.pos..."), NULL, ExportPositionPos, 0,
	  NULL },
	{ N_("/_File/_Export/_Position/Snowie .txt..."), NULL, 
          ExportPositionSnowieTxt, 0,
	  NULL },
	{ N_("/_File/_Export/_Position/Text..."), NULL, ExportPositionText, 0,
	  NULL },
	{ N_("/_File/_Export/_Session"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_File/_Export/_Session/HTML..."), NULL, ExportSessionHtml, 0,
	  NULL },
	{ N_("/_File/_Export/_Session/LaTeX..."), NULL, ExportSessionLaTeX, 0,
	  NULL },
	{ N_("/_File/_Export/_Session/PDF..."), 
          NULL, ExportSessionPDF, 0, NULL },
	{ N_("/_File/_Export/_Session/PostScript..."), NULL,
	  ExportSessionPostScript, 0, NULL },
	{ N_("/_File/_Export/_Session/Text..."), NULL,
	  ExportSessionText, 0, NULL },
	{ N_("/_File/_Export/_HTML Images..."), NULL, ExportHTMLImages, 0,
	  NULL },
	{ N_("/_File/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_File/_Quit"), "<control>Q", Command, CMD_QUIT, NULL },
	{ N_("/_Edit"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Edit/_Undo"), "<control>Z", ShowBoard, 0, NULL },
	{ N_("/_Edit/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Edit/_Copy"), "<control>C", Command, CMD_XCOPY, NULL },
	{ N_("/_Edit/Copy as"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Edit/Copy as/Position as ASCII"), NULL,
	  CommandCopy, 0, NULL },
	{ N_("/_Edit/Copy as/GammOnLine (HTML)"), NULL,
	  CopyAsGOL, 0, NULL },
	{ N_("/_Edit/_Paste"), "<control>V", NULL, 0, NULL },
	{ N_("/_Edit/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Edit/_Enter command..."), NULL, EnterCommand, 0, NULL },
	{ N_("/_Game"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Game/_Roll"), "<control>R", Command, CMD_ROLL, NULL },
	{ N_("/_Game/_Finish move"), "<control>F", FinishMove, 0, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/_Double"), "<control>D", Command, CMD_DOUBLE, NULL },
	{ N_("/_Game/_Take"), "<control>T", Command, CMD_TAKE, NULL },
	{ N_("/_Game/Dro_p"), "<control>P", Command, CMD_DROP, NULL },
	{ N_("/_Game/R_edouble"), NULL, Command, CMD_REDOUBLE, NULL },
	{ N_("/_Game/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Game/Re_sign"), NULL, GTKResign, 0, NULL },
#if 0
	{ N_("/_Game/Re_sign/_Normal"), NULL, Command, CMD_RESIGN_N, NULL },
	{ N_("/_Game/Re_sign/_Gammon"), NULL, Command, CMD_RESIGN_G, NULL },
	{ N_("/_Game/Re_sign/_Backgammon"), 
          NULL, Command, CMD_RESIGN_B, NULL },
#endif
        { N_("/_Game/_Agree to resignation"), NULL, Command, CMD_AGREE, NULL },
	{ N_("/_Game/De_cline resignation"), 
          NULL, Command, CMD_DECLINE, NULL },
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
	{ N_("/_Navigate"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Navigate/Previous rol_l"), "Page_Up", 
          Command, CMD_PREV_ROLL, NULL },
	{ N_("/_Navigate/Next _roll"), "Page_Down",
	  Command, CMD_NEXT_ROLL, NULL },
	{ N_("/_Navigate/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Navigate/_Previous move"), "<shift>Page_Up", 
          Command, CMD_PREV, NULL },
	{ N_("/_Navigate/Next _move"), "<shift>Page_Down", 
          Command, CMD_NEXT, NULL },
	{ N_("/_Navigate/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Navigate/Previous chequer _play"), "<alt>Page_Up",
	  Command, CMD_PREV_ROLLED, NULL },
	{ N_("/_Navigate/Next _chequer play"), "<alt>Page_Down",
	  Command, CMD_NEXT_ROLLED, NULL },
	{ N_("/_Navigate/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Navigate/Previous marke_d move"), "<control><shift>Page_Up", 
          Command, CMD_PREV_MARKED, NULL },
	{ N_("/_Navigate/Next mar_ked move"), "<control><shift>Page_Down", 
          Command, CMD_NEXT_MARKED, NULL },
	{ N_("/_Navigate/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Navigate/Pre_vious game"), "<control>Page_Up", 
          Command, CMD_PREV_GAME, NULL },
	{ N_("/_Navigate/Next _game"), "<control>Page_Down",
	  Command, CMD_NEXT_GAME, NULL },
	{ N_("/_Analyse"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Analyse/_Evaluate"), "<control>E", Command, CMD_EVAL, NULL },
	{ N_("/_Analyse/_Hint"), "<control>H", Command, CMD_HINT, NULL },
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
          NULL, Command, CMD_ANALYSE_CLEAR_MOVE, NULL },
        { N_("/_Analyse/Clear analysis/_Game"), 
          NULL, Command, CMD_ANALYSE_CLEAR_GAME, NULL },
        { N_("/_Analyse/Clear analysis/_Match"), 
          NULL, Command, CMD_ANALYSE_CLEAR_MATCH, NULL },
        { N_("/_Analyse/Clear analysis/_Session"), 
          NULL, Command, CMD_ANALYSE_CLEAR_SESSION, NULL },
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
	  CMD_RECORD_ADD_GAME, NULL },
	{ N_("/_Analyse/Add to player records/Match statistics"), NULL,
	  Command, CMD_RECORD_ADD_MATCH, NULL },
	{ N_("/_Analyse/Add to player records/Session statistics"), NULL,
	  Command, CMD_RECORD_ADD_SESSION, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/_Pip count"), NULL, Command, CMD_SHOW_PIPCOUNT, NULL },
	{ N_("/_Analyse/_Kleinman count"), 
          NULL, Command, CMD_SHOW_KLEINMAN, NULL },
	{ N_("/_Analyse/_Thorp count"), NULL, Command, CMD_SHOW_THORP, NULL },
	{ N_("/_Analyse/One chequer race"), NULL, Command, 
          CMD_SHOW_ONECHEQUER, NULL },
	{ N_("/_Analyse/One sided rollout"), NULL, Command, 
          CMD_SHOW_ONESIDEDROLLOUT, NULL },
#if USE_GTK2
	{ N_("/_Analyse/Distribution of rolls"), NULL, Command, 
          CMD_SHOW_ROLLS, NULL },
#endif /* USE_GTK2 */
	{ N_("/_Analyse/Temperature Map"), NULL, Command, 
          CMD_SHOW_TEMPERATURE_MAP, NULL },
	{ N_("/_Analyse/Temperature Map (cube decision)"), NULL, Command, 
          CMD_SHOW_TEMPERATURE_MAP_CUBE, NULL },
	{ N_("/_Analyse/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Analyse/Bearoff Databases"), NULL, Command, 
          CMD_SHOW_BEAROFF, NULL },
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
#if ENABLE_TRAIN_MENU
	{ N_("/_Train"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Train/D_ump database"), 
          NULL, Command, CMD_DATABASE_DUMP, NULL },
	{ N_("/_Train/_Generate database"), 
          NULL, Command, CMD_DATABASE_GENERATE, NULL },
	{ N_("/_Train/_Rollout database"), NULL, Command, CMD_DATABASE_ROLLOUT,
	  NULL },
	{ N_("/_Train/_Import database..."), NULL, DatabaseImport, 0, NULL },
	{ N_("/_Train/_Export database..."), NULL, DatabaseExport, 0, NULL },
	{ N_("/_Train/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Train/Train from _database"), 
          NULL, Command, CMD_TRAIN_DATABASE, NULL },
	{ N_("/_Train/Train with _TD(0)"), NULL, Command, CMD_TRAIN_TD, NULL },
#endif
	{ N_("/_Settings"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Settings/Analysis..."), NULL, SetAnalysis, 0, NULL },
	{ N_("/_Settings/Appearance..."), NULL, Command, CMD_SET_APPEARANCE,
	  NULL },
	{ N_("/_Settings/_Evaluation..."), NULL, SetEvaluation, 0, NULL },
        { N_("/_Settings/E_xport..."), NULL, Command, CMD_SHOW_EXPORT,
          NULL },
	{ N_("/_Settings/_Players..."), NULL, SetPlayers, 0, NULL },
	{ N_("/_Settings/_Rollouts..."), NULL, SetRollouts, 0, NULL },
	{ N_("/_Settings/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Settings/Options..."), NULL, SetOptions, 0, NULL },
	{ N_("/_Settings/Paths..."), NULL, Command, CMD_SHOW_PATH, NULL },
	{ N_("/_Settings/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Settings/Save settings"), 
          NULL, Command, CMD_SAVE_SETTINGS, NULL },
	{ N_("/_Windows"), NULL, NULL, 0, "<Branch>" },
#if USE_OLD_LAYOUT
 	{ N_("/_Windows/_Game record"), NULL, Command, CMD_LIST_GAME, NULL },
	{ N_("/_Windows/_Annotation"), NULL, Command, CMD_SET_ANNOTATION_ON,
	  NULL },
	{ N_("/_Windows/_Message"), NULL, Command, CMD_SET_MESSAGE_ON,
	  NULL },
#else
	{ N_("/_Windows/_Game record"), NULL, TogglePanel, TOGGLE_GAMELIST,
	  "<CheckItem>" },
	{ N_("/_Windows/_Annotation"), NULL, TogglePanel, TOGGLE_ANNOTATION,
	  "<CheckItem>" },
	{ N_("/_Windows/_Message"), NULL, TogglePanel, TOGGLE_MESSAGE,
	  "<CheckItem>" },
#endif
	{ N_("/_Windows/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Windows/Show all panels"), NULL, ShowAllPanels, 0, NULL },
	{ N_("/_Windows/Hide all panels"), NULL, HideAllPanels, 0, NULL },
	{ N_("/_Windows/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Windows/Full screen (Test only!)"), NULL, FullScreenMode, 0, NULL },
	{ N_("/_Windows/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Windows/Gu_ile"), NULL, NULL, 0, NULL },
	{ N_("/_Windows/_Python shell (IDLE)..."), 
          NULL, PythonShell, 0, NULL },
	{ N_("/_Help"), NULL, NULL, 0, "<Branch>" },
	{ N_("/_Help/_Commands"), NULL, Command, CMD_HELP, NULL },
	{ N_("/_Help/gnubg _Manual"), NULL, ShowManual, 0, NULL },
	{ N_("/_Help/gnubg M_anual (web)"), NULL, ShowManualWeb, 0, NULL },
	{ N_("/_Help/_Frequently Asked Questions"), NULL, ShowFAQ, 0, NULL },
	{ N_("/_Help/Co_pying gnubg"), NULL, Command, CMD_SHOW_COPYING, NULL },
	{ N_("/_Help/gnubg _Warranty"), NULL, Command, CMD_SHOW_WARRANTY,
	  NULL },
	{ N_("/_Help/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Help/_Report bug"), NULL, ReportBug, 0, NULL },
	{ N_("/_Help/-"), NULL, NULL, 0, "<Separator>" },
	{ N_("/_Help/_About gnubg"), NULL, Command, CMD_SHOW_VERSION, NULL }
    };
#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 15 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 15 );
#else
    char sz[ 4096 ];
#endif

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
	/* Initialize openGL widget */
	InitGTK3d(argc, argv);
#endif

    fnAction = HandleXAction;

    gdk_rgb_init();
    gdk_rgb_set_min_colors( 2 * 2 * 2 );
    gtk_widget_set_default_colormap( gdk_rgb_get_cmap() );
    gtk_widget_set_default_visual( gdk_rgb_get_visual() );

#if USE_GTK2
    {
#include "gnu.xpm"
	GtkIconFactory *pif = gtk_icon_factory_new();

	gtk_icon_factory_add_default( pif );

	gtk_icon_factory_add( pif, GTK_STOCK_DIALOG_GNU,
			      gtk_icon_set_new_from_pixbuf(
				  gdk_pixbuf_new_from_xpm_data(
				      (const char **) gnu_xpm ) ) );
    }
#endif

    ptt = gtk_tooltips_new();
    
    pwMain = gtk_window_new( GTK_WINDOW_TOPLEVEL );
#if GTK_CHECK_VERSION(2,0,0)
    gtk_window_set_role( GTK_WINDOW( pwMain ), "main" );
    gtk_window_set_type_hint( GTK_WINDOW( pwMain ),
			      GDK_WINDOW_TYPE_HINT_NORMAL );
#endif
    /* NB: the following call to gtk_object_set_user_data is needed
       to work around a nasty bug in GTK+ (the problem is that calls to
       gtk_object_get_user_data relies on a side effect from a previous
       call to gtk_object_set_data).  Adding a call here guarantees that
       gtk_object_get_user_data will work as we expect later.
       FIXME: this can eventually be removed once GTK+ is fixed. */
    gtk_object_set_user_data( GTK_OBJECT( pwMain ), NULL );
    
    setWindowGeometry ( pwMain, &awg[ WINDOW_MAIN ] );

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
#if GTK_CHECK_VERSION(1,3,15)
    gtk_accel_map_load( sz );
#else
    gtk_item_factory_parse_rc( sz );
#endif
    
#if ENABLE_TRAIN_MENU
#if !HAVE_LIBGDBM
    gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	pif, CMD_DATABASE_DUMP ), FALSE );
    gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	pif, CMD_DATABASE_GENERATE ), FALSE );
    gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	pif, CMD_DATABASE_ROLLOUT ), FALSE );
    gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	pif, CMD_TRAIN_DATABASE ), FALSE );
    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pif, "/Train/Import database..." ), FALSE );
    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pif, "/Train/Export database..." ), FALSE );
#endif
#endif

    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pif, "/Windows/Guile" ), FALSE );

    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pif, "/Windows/Python shell (IDLE)..." ), 
#if USE_PYTHON 
                              TRUE
#else
                              FALSE
#endif
                              );
   gtk_box_pack_start( GTK_BOX( pwVbox ),
                       pwHandle = gtk_handle_box_new(), FALSE, TRUE, 0 );
   gtk_container_add( GTK_CONTAINER( pwHandle ),
                       pwToolbar = ToolbarNew() );
    
   pwGrab = GTK_WIDGET( ToolbarGetStopParent( pwToolbar ) );
#if USE_OLD_LAYOUT
   gtk_box_pack_start( GTK_BOX(pwVbox), pwBoard = board_new(), TRUE, TRUE, 0 );
#else
   gtk_container_add( GTK_CONTAINER( pwVbox ), pwHbox =gtk_hbox_new(FALSE, 0));
   gtk_box_pack_start( GTK_BOX(pwHbox), pwBoard = board_new(), TRUE, TRUE, 0 );
   gtk_box_pack_start( GTK_BOX(pwHbox), pwVboxRight = gtk_vbox_new( FALSE, 1 ),
		    FALSE, TRUE, 0);
   pwGame = CreateGameWindow();
   gtk_box_pack_start( GTK_BOX( pwVboxRight ), pwGame, TRUE, TRUE, 0 );

   pwAnnotation =CreateAnnotationWindow();
   gtk_box_pack_start( GTK_BOX( pwVboxRight ), pwAnnotation, TRUE, TRUE, 0 );

   pwMessage = CreateMessageWindow();
   gtk_box_pack_start( GTK_BOX( pwVboxRight ), pwMessage, FALSE, FALSE, 0 );

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
	   gtk_item_factory_get_widget( pif, "/Windows/Message")), TRUE);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
	   gtk_item_factory_get_widget( pif, "/Windows/Annotation")), TRUE);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
	   gtk_item_factory_get_widget( pif, "/Windows/Game record")), TRUE);
#endif
   /* Status bar */
					
   gtk_box_pack_end( GTK_BOX( pwVbox ), pwHbox = gtk_hbox_new( FALSE, 0 ),
		      FALSE, FALSE, 0 );
    
    gtk_box_pack_start( GTK_BOX( pwHbox ), pwStatus = gtk_statusbar_new(),
		      TRUE, TRUE, 0 );
#if GTK_CHECK_VERSION(1,3,10)
    gtk_statusbar_set_has_resize_grip( GTK_STATUSBAR( pwStatus ), FALSE );
#endif
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
#if GTK_CHECK_VERSION(1,3,10)
    gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR( pwProgress ), 0.0 );
#endif
    /* This is a kludge to work around an ugly bug in GTK: we don't want to
       show text in the progress bar yet, but we might later.  So we have to
       pretend we want text in order to be sized correctly, and then set the
       format string to something so we don't get the default text. */
    gtk_progress_set_show_text( GTK_PROGRESS( pwProgress ), TRUE );
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ), " " );
    
    gtk_selection_add_target( pwMain, 
                              gdk_atom_intern ("CLIPBOARD", FALSE),
                              GDK_SELECTION_TYPE_STRING, 0 );
    gtk_selection_add_target( pwMain, 
                              GDK_SELECTION_PRIMARY,
                              GDK_SELECTION_TYPE_STRING, 0 );

    gtk_signal_connect( GTK_OBJECT( pwMain ), "size-request",
			GTK_SIGNAL_FUNC( MainSize ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "selection_get", 
			GTK_SIGNAL_FUNC( MainGetSelection ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "selection_received", 
			GTK_SIGNAL_FUNC( MainSelectionReceived ), NULL );
#if USE_OLD_LAYOUT
    pwGame = CreateGameWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwGame ), pagMain );

    pwAnnotation = CreateAnnotationWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwAnnotation ), pagMain );
    
    pwMessage = CreateMessageWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwMessage ), pagMain );
#endif
    ListCreate( &lOutput );
    return TRUE;
}

extern void RunGTK( GtkWidget *pwSplash ) {

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
#ifdef ConnectionNumber /* FIXME use configure somehow to detect this */
#if O_ASYNC
    /* BSD O_ASYNC-style I/O notification */
    {
	int n;
	
	if( ( n = fcntl( ConnectionNumber( GDK_DISPLAY() ),
			 F_GETFL ) ) != -1 ) {
	    fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETOWN, getpid() );
	    fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETFL, n | O_ASYNC );
	}
    }
#else
    /* System V SIGPOLL-style I/O notification */
    ioctl( ConnectionNumber( GDK_DISPLAY() ), I_SETSIG, S_RDNORM );
#endif
#endif
    
#if HAVE_LIBREADLINE
	if( fReadline ) {
	    fReadingCommand = TRUE;
	    rl_callback_handler_install( FormatPrompt(), HandleInput );
	    atexit( rl_callback_handler_remove );
	} else
#endif
	    Prompt();
    }
    
    gtk_widget_show_all( pwMain );

#if USE_BOARD3D
	DisplayCorrectBoardType();
#endif

    DestroySplash ( pwSplash );

    /* force update of board; needed to display board correctly if user
       has special settings, e.g., clockwise or nackgammon */
    ShowBoard();

    gtk_main();
}

static void DestroyList( gpointer p ) {

    *( (void **) p ) = NULL;
}

extern void ShowList( char *psz[], char *szTitle ) {

    static GtkWidget *pwDialog;
    GtkWidget *pwList = gtk_list_new(),
	*pwScroll = gtk_scrolled_window_new( NULL, NULL ),
	*pwOK = gtk_button_new_with_label( _("OK") );

    if( pwDialog )
	gtk_widget_destroy( pwDialog );

    pwDialog = gtk_dialog_new();
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
#if USE_GTK2
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
#else
#include "gnu.xpm"
#include "question.xpm"
    GdkPixmap *ppm;
    
    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
	 gtk_widget_get_colormap( pwDialog ), NULL, NULL,
	 fQuestion ? question_xpm : gnu_xpm );
    pwPixmap = gtk_pixmap_new( ppm, NULL );
#endif
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

#if GTK_CHECK_VERSION(1,3,15)
    gtk_window_add_accel_group( GTK_WINDOW( pwDialog ), pag );
#else
    gtk_accel_group_attach( pag, GTK_OBJECT( pwDialog ) );
#endif
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
				  GTK_WINDOW( pwMain ) );
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

#include "question.xpm"

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

#if GTK_CHECK_VERSION(1,3,15)
    gtk_window_add_accel_group( GTK_WINDOW( pwTutorDialog ), pag );
#else
    gtk_accel_group_attach( pag, GTK_OBJECT( pwTutorDialog ) );
#endif
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

    if( !cchOutput )
	return;
    
    pchDest = sz = g_malloc( cchOutput + 1 );

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
      /* FIXME if fDisplay is false, skip to the last line and display
         in status bar. */
      if ( ! fMessage )
        GTKMessage( sz, DT_INFO );
    }
    else
      /* Short message; display in status bar. */
      gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idOutput, sz );
    
    if ( fMessage && *sz ) {
      strcat ( sz, "\n" );
      gtk_text_insert( GTK_TEXT( pwMessageText ), NULL, NULL, NULL,
                       sz, -1 );
#if !USE_GTK2
      /* Hack to make sure message is drawn correctly with gtk 1 */
      gtk_text_freeze(GTK_TEXT( pwMessageText ));
      gtk_text_thaw(GTK_TEXT( pwMessageText ));
#endif

    }
      
    cchOutput = 0;
    g_free( sz );
}

extern void GTKOutputErr( char *sz ) {

    GTKMessage( sz, DT_ERROR );
    
    if( fMessage ) {
	gtk_text_insert( GTK_TEXT( pwMessageText ), NULL, NULL, NULL,
			 sz, -1 );
	gtk_text_insert( GTK_TEXT( pwMessageText ), NULL, NULL, NULL,
			 "\n", 1 );
#if !USE_GTK2
		/* Hack to make sure message is drawn correctly with gtk 1 */
		gtk_text_freeze(GTK_TEXT( pwMessageText ));
		gtk_text_thaw(GTK_TEXT( pwMessageText ));
#endif
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

static GtkWidget *pwEntry;

#if 0
static void NumberOK( GtkWidget *pw, int *pf ) {

    *pf = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( pwEntry ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}
extern int 
GTKReadNumber( char *szTitle, char *szPrompt, int nDefault,
               int nMin, int nMax, int nInc ) {

    int n = INT_MIN;
    GtkObject *pa = gtk_adjustment_new( nDefault, nMin, nMax, nInc, nInc,
					0 );
    GtkWidget *pwDialog = GTKCreateDialog( szTitle, DT_QUESTION,
					GTK_SIGNAL_FUNC( NumberOK ), &n ),
	*pwPrompt = gtk_label_new( szPrompt );

    pwEntry = gtk_spin_button_new( GTK_ADJUSTMENT( pa ), nInc, 0 );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( pwEntry ), TRUE );

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwEntry );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect_after( GTK_OBJECT( pwEntry ), "activate",
			GTK_SIGNAL_FUNC( NumberOK ), &n );
    
    gtk_widget_grab_focus( pwEntry );
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    return n;
}
#endif
static void StringOK( GtkWidget *pw, char **ppch ) {

    *ppch = gtk_editable_get_chars( GTK_EDITABLE( pwEntry ), 0, -1 );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static char *ReadString( char *szTitle, char *szPrompt, char *szDefault ) {

    char *sz = NULL;
    GtkWidget *pwDialog = GTKCreateDialog( szTitle, DT_QUESTION,
					GTK_SIGNAL_FUNC( StringOK ), &sz ),
	*pwPrompt = gtk_label_new( szPrompt );

    pwEntry = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY( pwEntry ), szDefault );

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwEntry );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect_after( GTK_OBJECT( pwEntry ), "activate",
			GTK_SIGNAL_FUNC( StringOK ), &sz );

    gtk_widget_grab_focus( pwEntry );
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    return sz;
}
#if 0

static void RealOK( GtkWidget *pw, float *pr ) {

    *pr = gtk_spin_button_get_value_as_float( GTK_SPIN_BUTTON( pwEntry ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static float ReadReal( char *szTitle, char *szPrompt, double rDefault,
			double rMin, double rMax, double rInc ) {

    float r = ERR_VAL;
    GtkObject *pa = gtk_adjustment_new( rDefault, rMin, rMax, rInc, rInc,
					0 );
    GtkWidget *pwDialog = GTKCreateDialog( szTitle, DT_QUESTION,
					GTK_SIGNAL_FUNC( RealOK ), &r ),
	*pwPrompt = gtk_label_new( szPrompt );

    pwEntry = gtk_spin_button_new( GTK_ADJUSTMENT( pa ), rInc, 2 );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( pwEntry ), TRUE );

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwEntry );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    gtk_signal_connect_after( GTK_OBJECT( pwEntry ), "activate",
			GTK_SIGNAL_FUNC( RealOK ), &r );
    
    gtk_widget_grab_focus( pwEntry );
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    return r;
}
#endif
static void EnterCommand( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch = ReadString( _("GNU Backgammon - Enter command"), FormatPrompt(),
			    "" );

    if( pch ) {
	UserCommand( pch );
	g_free( pch );
    }
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
#if 0
#include "xpm/computer.xpm"
#include "xpm/human2.xpm"
#endif
  pwVbox = gtk_vbox_new(FALSE, 0);
#if USE_GTK2
  pwToolbar = gtk_toolbar_new ();
  gtk_toolbar_set_orientation ( GTK_TOOLBAR ( pwToolbar ),
                                GTK_ORIENTATION_HORIZONTAL );
  gtk_toolbar_set_style ( GTK_TOOLBAR ( pwToolbar ),
                          GTK_TOOLBAR_ICONS );
#else
  pwToolbar = gtk_toolbar_new ( GTK_ORIENTATION_HORIZONTAL,
                                GTK_TOOLBAR_ICONS );
#endif /* ! USE_GTK2 */
  pwFrame = gtk_frame_new(_("Shortcut buttons"));
  gtk_box_pack_start ( GTK_BOX ( pwVbox ), pwFrame, TRUE, TRUE, 0);
  gtk_container_add( GTK_CONTAINER( pwFrame ), pwToolbar);
  gtk_container_set_border_width( GTK_CONTAINER( pwToolbar ), 4);
  
  pwButtons = button_from_image( image_from_xpm_d ( stock_new_money_xpm,
                                                      pwToolbar ) );
  gtk_toolbar_append_widget( GTK_TOOLBAR( pwToolbar ),
                   pwButtons, "Start a new money game session", NULL );
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
#if 0
  pnw->pwGNUvsHuman = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS));
  pnw->pwHumanHuman = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS));
  
  pwHbox2 = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      image_from_xpm_d(computer_xpm, pnw->pwCPS));
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      gtk_label_new(_("vs.")));
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      image_from_xpm_d(human2_xpm, pnw->pwCPS));

  gtk_container_add(GTK_CONTAINER(pnw->pwGNUvsHuman), pwHbox2);
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwGNUvsHuman, FALSE, FALSE, 0);
  
  pwHbox2 = gtk_hbox_new(FALSE, 0);
  
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      image_from_xpm_d(human2_xpm, pnw->pwCPS));
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      gtk_label_new(_("vs.")));
  gtk_container_add(GTK_CONTAINER(pwHbox2),
      image_from_xpm_d(human2_xpm, pnw->pwCPS));
  
  gtk_container_add(GTK_CONTAINER(pnw->pwHumanHuman), pwHbox2);
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwHumanHuman, FALSE, FALSE, 0);
#else
  pnw->pwGNUvsHuman = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS), _("GNU Backgammon vs. Human"));
  pnw->pwHumanHuman = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(pnw->pwCPS), _("Human vs. Human"));
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwGNUvsHuman, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(pwVbox2), pnw->pwHumanHuman, FALSE, FALSE, 0);
  
#endif
  
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
  
  gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
		  
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
  gtk_adjustment_set_value( GTK_ADJUSTMENT( pnw->padjML ), nDefaultLength );
}

static void NewDialog( gpointer *p, guint n, GtkWidget *pw ) {
  GTKNew();
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


#if 0
static void NewWeights( gpointer *p, guint n, GtkWidget *pw ) {

    int nSize = GTKReadNumber( _("GNU Backgammon - New Weights"),
                               _("Number of hidden nodes:"), 128, 1, 1024, 1 );
    
    if( nSize > 0 ) {
	char sz[ 32 ];

	sprintf( sz, "new weights %d", nSize );
	UserCommand( sz );
    }
}
#endif

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
      case FDT_EXPORT:
        pft->pch = g_strdup_printf("\"%s\"",
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
	break;

      case FDT_IMPORT:
        pft->n = gtk_option_menu_get_history(GTK_OPTION_MENU(pft->pwom));
	pft->pch = g_strdup_printf("%s \"%s\"", aszImpFormat[pft->n], 
             gtk_file_selection_get_filename( GTK_FILE_SELECTION( pwFile ) ) );
        break;

      case FDT_SAVE:
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
	  PATH_LATEX, PATH_PDF, PATH_POSTSCRIPT, PATH_EPS, PATH_EPS, PATH_TEXT,
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

static char *SelectFile( char *szTitle, char *szDefault, char *szPath,
		filedialogtype fdt ) {

    GtkWidget *pw = gtk_file_selection_new( szTitle ),
	*pwButton, *pwExpSet; 

    filethings ft;
    ft.pch = NULL;
    ft.fdt = fdt;
    ft.n = 0;
    ft.pwom = NULL;
    
    if( szPath ) {
#if USE_GTK2
	pwButton = gtk_button_new_with_mnemonic( _("Set Default _Path") );
#else
	pwButton = gtk_button_new_with_label( _("Set Default Path") );
#endif
	
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

      pwGetPath = gtk_button_new_with_label("Go to default path");
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
                            char *szPath, filedialogtype fdt ) {

    char *pch;
    
    if( ( pch = SelectFile( szPrompt, szDefault, szPath, fdt ) ) ) {
#if __GNUC__
	char sz[ strlen( pch ) + strlen( szCommand ) + 4 ];
#elif HAVE_ALLOCA
	char *sz = alloca( strlen( pch ) + strlen( szCommand ) + 4 );
#else
	char sz[ 1024 ];
#endif
	sprintf( sz, "%s %s", szCommand, pch );
	UserCommand( sz );

	g_free( pch );
    }
}

static void LoadCommands( gpointer *p, guint n, GtkWidget *pw ) {

    GTKFileCommand( _("Open commands"), NULL, "load commands", NULL, FDT_NONE );
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
		 "met", FDT_NONE );

    /* update filename on option page */
    if ( p && GTK_WIDGET_VISIBLE( p ) )
	gtk_label_set_text( GTK_LABEL( p ), miCurrent.szFileName );

    if( pch )
	free( pch );
    if( pchMet )
	free( pchMet );
}

static void LoadGame( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGF );
  GTKFileCommand( _("Open game"), sz, "load game", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void LoadMatch( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGF );
  GTKFileCommand( _("Open match or session"), sz, "load match", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void LoadPosition( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGF );
  GTKFileCommand( _("Open position"), sz, "load position", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportBKG( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_BKG );
  GTKFileCommand( _("Import BKG session"), sz, "import bkg", "bkg", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportMat( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_MAT );
  GTKFileCommand( _("Import .mat match"), sz, "import mat", "mat", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportPos( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_POS );
  GTKFileCommand( _("Import .pos position"), sz, "import pos", "pos", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportOldmoves( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_OLDMOVES );
  GTKFileCommand( _("Import FIBS oldmoves"), sz, "import oldmoves", "oldmoves", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportSGG( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGG );
  GTKFileCommand( _("Import .sgg match"), sz, "import sgg", "sgg", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportTMG( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_TMG );
  GTKFileCommand( _("Import .tmg match"), sz, "import tmg", "tmg", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ImportSnowieTxt( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SNOWIE_TXT ); 
  GTKFileCommand( _("Import Snowie .txt position"), sz, "import snowietxt", 
               "snowietxt", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void SaveGame( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  GTKFileCommand( _("Save game"), sz, "save game", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void SaveMatch( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  GTKFileCommand( _("Save match or session"), sz, "save match", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void SavePosition( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  GTKFileCommand( _("Save position"), sz, "save position", "sgf", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void SaveWeights( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.weights" );
  GTKFileCommand( _("Save weights"), sz, "save weights", NULL, FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ExportGameGam( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_GAM );
  GTKFileCommand( _("Export .gam game"), sz, "export game gam", "gam", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportGameHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  GTKFileCommand( _("Export HTML game"), sz, "export game html", "html", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportGameLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  GTKFileCommand( _("Export LaTeX game"), sz, "export game latex", "latex", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportGamePDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  GTKFileCommand( _("Export PDF game"), sz, "export game pdf", "pdf", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportGamePostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  GTKFileCommand( _("Export PostScript game"), sz, "export game postscript",
	       "postscript", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void
ExportGameText( gpointer *p, guint n, GtkWidget *pw )
{
  char *sz = getDefaultFileName ( PATH_TEXT );
  GTKFileCommand( _("Export text game"), sz, "export game text", "text", FDT_EXPORT );
  if ( sz ) 
    free ( sz );
}

static void ExportMatchLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  GTKFileCommand( _("Export LaTeX match"), sz, "export match latex", "latex", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  GTKFileCommand( _("Export HTML match"), sz, "export match html", "html", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchMat( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_MAT );
  GTKFileCommand( _("Export .mat match"), sz, "export match mat", "mat", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchPDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  GTKFileCommand( _("Export PDF match"), sz, "export match pdf", "pdf", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchPostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  GTKFileCommand( _("Export PostScript match"), sz, "export match postscript",
	       "postscript", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchText( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_TEXT );
  GTKFileCommand( _("Export text match"), sz, "export match text", "text", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionEPS( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_EPS );
  GTKFileCommand( _("Export EPS position"), sz, "export position eps", "eps", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  GTKFileCommand( _("Export HTML position"), sz, "export position html", "html", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionGammOnLine( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  GTKFileCommand( _("Export position to GammOnLine (HTML)"), 
               sz, "export position gammonline", "html", FDT_NONE );
  if ( sz ) 
    free ( sz );
}

static void CopyAsGOL( gpointer *p, guint n, GtkWidget *pw ) {

  UserCommand("export position gol2clipboard");

}

static void ExportPositionPos( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POS );
  GTKFileCommand( _("Export .pos position"), sz, "export position pos", "pos", FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionPNG( gpointer *p, guint n, GtkWidget *pw ) {

  /* char *sz = getDefaultFileName ( PATH_PNG ); */
  GTKFileCommand( _("Export PNG position"), NULL, "export position png", "png", FDT_EXPORT );
  /* if ( sz ) 
     free ( sz ); */

}

static void ExportPositionText( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_TEXT );
  GTKFileCommand( _("Export text position"), sz, "export position text", "text", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionSnowieTxt( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SNOWIE_TXT );
  GTKFileCommand( _("Export Snowie .txt position"), sz, 
               "export position snowietxt", "snowietxt", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  GTKFileCommand( _("Export LaTeX session"), sz, "export session latex",
	       "latex", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionPDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  GTKFileCommand( _("Export PDF session"), sz, "export session pdf", "pdf", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  GTKFileCommand( _("Export HTML session"), sz, "export session html", "html", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionPostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  GTKFileCommand( _("Export PostScript session"), sz,
               "export session postscript", "postscript", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionText( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_TEXT );
  GTKFileCommand( _("Export text session"), sz, "export session text", "text", FDT_EXPORT );
  if ( sz ) 
    free ( sz );

}

static void ExportHTMLImages( gpointer *p, guint n, GtkWidget *pw ) {

    char *sz = strdup( PKGDATADIR "/html-images" );
    GTKFileCommand( _("Export HTML images"), sz, "export htmlimages", NULL, FDT_EXPORT );
    if ( sz ) 
	free ( sz );
}

#if ENABLE_TRAIN_MENU

static void DatabaseExport( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.gdbm" );
  GTKFileCommand( _("Export database"), sz, "database export", NULL, FDT_NONE );
  if ( sz ) 
    free ( sz );

}

static void DatabaseImport( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.gdbm" );
  GTKFileCommand( _("Import database"), sz, "database import", NULL, FDT_NONE );
  if ( sz ) 
    free ( sz );

}

#endif /* ENABLE_TRAIN_MENU */

typedef struct _evalwidget {
    evalcontext *pec;
    movefilter *pmf;
    GtkWidget *pwCubeful, *pwReduced, *pwDeterministic;
    GtkAdjustment *padjPlies, *padjSearchCandidates, *padjSearchTolerance,
	*padjNoise;
    int *pfOK;
  GtkWidget *pwOptionMenu;
  int fMoveFilter;
  GtkWidget *pwMoveFilter;
} evalwidget;

static void EvalGetValues ( evalcontext *pec, evalwidget *pew ) {

  GtkWidget *pwMenu, *pwItem;
  int *pi;

    pec->nPlies = pew->padjPlies->value;
    pec->fCubeful = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwCubeful ) );

    /* reduced */
    pwMenu = gtk_option_menu_get_menu ( GTK_OPTION_MENU ( pew->pwReduced ) );
    pwItem = gtk_menu_get_active ( GTK_MENU ( pwMenu ) );
    pi = (int *) gtk_object_get_user_data ( GTK_OBJECT ( pwItem ) );
    pec->nReduced = *pi;

    pec->rNoise = pew->padjNoise->value;
    pec->fDeterministic = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwDeterministic ) );

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
        equal_movefilters ( (movefilter (*)[]) pew->pmf, 
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

    gtk_widget_set_sensitive( pew->pwReduced, padj->value > 0 );

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

  gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwReduced ), 
                                pec->nReduced );
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
    GtkWidget *pw2, *pw3, *pw4;

    GtkWidget *pwMenu;
    GtkWidget *pwItem;

    GtkWidget *pwev;

    const char *aszReduced[] = {
      N_("No reduction"),
      NULL,
      N_("50%% speed"),
      N_("33%% speed"),
      N_("25%% speed") 
    };
    gchar *pch;

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

    /* reduced evaluation */

    /* FIXME if and when we support different values for nReduced, this
       check button won't work */

    pwev = gtk_event_box_new();
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwev );

    gtk_tooltips_set_tip( ptt, pwev,
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
    
#if GTK_CHECK_VERSION(2,0,0)
    gtk_signal_connect ( GTK_OBJECT( pwMenu ), "selection-done",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );
#endif

    pew->pwReduced = gtk_option_menu_new ();
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pew->pwReduced ), pwMenu );

    gtk_container_add( GTK_CONTAINER( pw4 ), pew->pwReduced );

    /* UGLY fix for the fact the menu has entries only for values
     * 0, 2, 3, 4 
     */
	gtk_option_menu_set_history( GTK_OPTION_MENU( pew->pwReduced ), 
                                 (pec->nReduced < 2) ? 0 : 
                                  pec->nReduced - 1 );

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

#if GTK_CHECK_VERSION(2,0,0)
    gtk_signal_connect ( GTK_OBJECT( pew->pwReduced ), "changed",
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

    if( pec->nReduced != pecOrig->nReduced ) {
	sprintf( sz, "%s reduced %d", szPrefix, pec->nReduced );
	UserCommand( sz );
    }

    if( pec->fCubeful != pecOrig->fCubeful ) {
	sprintf( sz, "%s cubeful %s", szPrefix, pec->fCubeful ? "on" : "off" );
	UserCommand( sz );
    }

    if( pec->rNoise != pecOrig->rNoise ) {
	lisprintf( sz, "%s noise %.3f", szPrefix, pec->rNoise );
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
	    GTK_ENTRY( pplw->apwName[ i ] ) ), 32 );
	
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
     lisprintf(sz,  (string), paw->apadjSkill[(button)]->value ); \
     UserCommand(sz); \
  }

#define ADJUSTLUCKUPDATE(button,flag,string) \
  if( paw->apadjLuck[(button)]->value != arLuckLevel[(flag)] ) \
  { \
     lisprintf(sz, (string), paw->apadjLuck[(button)]->value ); \
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
  ADJUSTSKILLUPDATE( 2, SKILL_DOUBTFUL, "set analysis threshold doubtful %.3f" )
  ADJUSTSKILLUPDATE( 3, SKILL_BAD, "set analysis threshold bad %.3f" )
  ADJUSTSKILLUPDATE( 4, SKILL_VERYBAD, "set analysis threshold verybad %.3f" )

  ADJUSTLUCKUPDATE( 0, LUCK_VERYGOOD, "set analysis threshold verylucky %.3f" )
  ADJUSTLUCKUPDATE( 1, LUCK_GOOD, "set analysis threshold lucky %.3f" )
  ADJUSTLUCKUPDATE( 2, LUCK_BAD, "set analysis threshold unlucky %.3f" )
  ADJUSTLUCKUPDATE( 3, LUCK_VERYBAD, "set analysis threshold veryunlucky %.3f" )

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

    if( rw.rcRollout.rStdLimit != rcRollout.rStdLimit ) {
      lisprintf( sz, "set rollout limit maxerr %5.4f", rw.rcRollout.rStdLimit );
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

    if( rw.rcRollout.rJsdLimit != rcRollout.rJsdLimit ) {
      lisprintf( sz, "set rollout jsd limit %5.4f", rw.rcRollout.rJsdLimit );
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

    GtkWidget *pwDialog = GTKCreateDialog( _("GNU Backgammon - Evaluation"),
					DT_INFO, NULL, NULL ),
	*pwText = gtk_text_new( NULL, NULL );
    GdkFont *pf;
    GtkWidget *pwButtons,
        *pwCopy = gtk_button_new_with_label( "Copy" );

#if WIN32
    /* Windows fonts come out smaller than you ask for, for some reason... */
    pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-14-"
			"*-*-*-m-*-iso8859-1" );
#else
    pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-12-"
			"*-*-*-m-*-iso8859-1" );
#endif

    /* FIXME There should be some way to extract the text on Unix as well */
    pwButtons = DialogArea( pwDialog, DA_BUTTONS );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwCopy );
    gtk_signal_connect( GTK_OBJECT( pwCopy ), "clicked",
			GTK_SIGNAL_FUNC( GTKWinCopy ), (gpointer) szOutput );
    
    gtk_text_set_editable( GTK_TEXT( pwText ), FALSE );
    gtk_text_insert( GTK_TEXT( pwText ), pf, NULL, NULL, szOutput, -1 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwText );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 600, 600 );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
 
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    gdk_font_unref( pf );
}

static void DestroyHint( gpointer p ) {

  movelist *pml = p;

  if ( pml ) {
    if ( pml->amMoves )
      free ( pml->amMoves );
    
    free ( pml );
  }

  pwHint = NULL;

}

static void
HintOK ( GtkWidget *pw, void *unused ) {

#if USE_OLD_LAYOUT
 getWindowGeometry ( &awg[ WINDOW_HINT ], pwHint );
#endif
 gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

 pwHint = NULL;
 
}

extern void GTKCubeHint( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
			 float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
			  const evalsetup *pes ) {
    
    static evalsetup es;
    GtkWidget *pw;

    if ( pwHint )
	gtk_widget_destroy( pwHint );
      
    pwHint = GTKCreateDialog( _("GNU Backgammon - Hint"), DT_INFO,
			   GTK_SIGNAL_FUNC( HintOK ), NULL );

    memcpy ( &es, pes, sizeof ( evalsetup ) );

    pw = CreateCubeAnalysis ( aarOutput, aarStdDev, &es, MOVE_NORMAL );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwHint, DA_MAIN ) ),
                       pw );

    gtk_widget_grab_focus( DialogArea( pwHint, DA_OK ) );
    
    setWindowGeometry ( pwHint, &awg[ WINDOW_HINT ] );
    
    gtk_object_weakref( GTK_OBJECT( pwHint ), DestroyHint, NULL );

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
GTKHint( movelist *pmlOrig, const int iMove ) {

    GtkWidget *pwButtons, *pwMoves;
    movelist *pml;
    static int n;
    
    if( pwHint )
	gtk_widget_destroy( pwHint );

    pml = malloc( sizeof( *pml ) );
    memcpy( pml, pmlOrig, sizeof( *pml ) );
    
    pml->amMoves = malloc( pmlOrig->cMoves * sizeof( move ) );
    memcpy( pml->amMoves, pmlOrig->amMoves, pmlOrig->cMoves * sizeof( move ) );

    n = iMove;
    pwMoves = CreateMoveList( pml, ( n < 0 ) ? NULL : &n, TRUE, TRUE );

    /* create dialog */
    
    pwHint = GTKCreateDialog( _("GNU Backgammon - Hint"), DT_INFO,
			   GTK_SIGNAL_FUNC( HintOK ), NULL );
    pwButtons = DialogArea( pwHint, DA_BUTTONS );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwHint, DA_MAIN ) ), 
                       pwMoves );

#if USE_OLD_LAYOUT
    setWindowGeometry ( pwHint, &awg[ WINDOW_HINT ] );
#endif
    gtk_object_weakref( GTK_OBJECT( pwHint ), DestroyHint, pml );
#if !USE_OLD_LAYOUT
    gtk_window_set_default_size(GTK_WINDOW(pwHint), 400, 300);
#endif
    gtk_widget_show_all( pwHint );
}



extern void GTKProgressStart( char *sz ) {

    gtk_progress_set_activity_mode( GTK_PROGRESS( pwProgress ), TRUE );
    gtk_progress_bar_set_activity_step( GTK_PROGRESS_BAR( pwProgress ), 5 );
    gtk_progress_bar_set_activity_blocks( GTK_PROGRESS_BAR( pwProgress ), 5 );

    if( sz )
	gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idProgress, sz );
}


extern void
GTKProgressStartValue( char *sz, int iMax ) {

  gtk_progress_set_activity_mode ( GTK_PROGRESS ( pwProgress ), FALSE );
  gtk_progress_configure ( GTK_PROGRESS ( pwProgress ),
                           0, 0, iMax );
  gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ),
				    "%v/%u (%p%%)" );

  if( sz )
    gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idProgress, sz );

}

extern void
GTKProgressValue ( int iValue ) {

    monitor m;
        
    gtk_progress_set_value( GTK_PROGRESS ( pwProgress ), iValue );

    SuspendInput( &m );
    
    while( gtk_events_pending() )
	gtk_main_iteration();
    
    ResumeInput( &m );
}


extern void GTKProgress( void ) {

    static int i;
    monitor m;

    gtk_progress_set_value( GTK_PROGRESS( pwProgress ), i ^= 1 );

    SuspendInput( &m );

    while( gtk_events_pending() )
        gtk_main_iteration();

    ResumeInput( &m );
}

extern void GTKProgressEnd( void ) {

    gtk_progress_set_activity_mode( GTK_PROGRESS( pwProgress ), FALSE );
    gtk_progress_set_value( GTK_PROGRESS( pwProgress ), 0 );
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ), " " );
    gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idProgress );
}

static void DestroyAbout( gpointer p ) {

    *( (void **) p ) = NULL;
}

extern void GTKShowVersion( void ) {

    static GtkWidget *pwDialog;
    GtkWidget *pwBox = gtk_vbox_new( FALSE, 0 ),
	*pwHBox = gtk_hbox_new( FALSE, 8 ),
	*pwPrompt = gtk_label_new( _("GNU Backgammon") ),
	*pwList = gtk_list_new(),
	*pwScrolled = gtk_scrolled_window_new( NULL, NULL ),
	*pwButton;
    GtkRcStyle *ps = gtk_rc_style_new();
    int i;
    static char *aszAuthors[] = { "Joseph Heled", "Øystein Johansen",
				  "David Montgomery",
				  "Jim Segrave",
				  "Jrn Thyssen", "Gary Wong" };
    extern char *aszCredits[];

    if( pwDialog )
	gtk_widget_destroy( pwDialog );
    
    pwDialog = GTKCreateDialog( _("About GNU Backgammon"), DT_GNU,
			     NULL, NULL );
    gtk_object_weakref( GTK_OBJECT( pwDialog ), DestroyAbout, &pwDialog );

    gtk_container_set_border_width( GTK_CONTAINER( pwBox ), 4 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwBox );

    gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt, FALSE, FALSE, 0 );
#if GTK_CHECK_VERSION(1,3,10)
    ps->font_desc = pango_font_description_new();
    pango_font_description_set_family_static( ps->font_desc, "serif" );
    pango_font_description_set_size( ps->font_desc, 64 * PANGO_SCALE );    
#else
    ps->font_name = g_strdup( "-*-times-medium-r-normal-*-64-*-*-*-p-*-"
			      "iso8859-1" );
#endif
    gtk_widget_modify_style( pwPrompt, ps );
    gtk_rc_style_unref( ps );
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( "version " VERSION 
#ifdef WIN32
                                       " (build " __DATE__ ")"
#endif
                                       ), 
                        FALSE, FALSE, 0 );

    ps = gtk_rc_style_new();
    gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt =
			gtk_label_new( "Copyright 1999, 2000, 2001, 2002, "
				       "2003 Gary Wong" ), FALSE, FALSE, 4 );
#if GTK_CHECK_VERSION(1,3,10)
    ps->font_desc = pango_font_description_new();
    pango_font_description_set_family_static( ps->font_desc, "sans" );
    pango_font_description_set_size( ps->font_desc, 8 * PANGO_SCALE );    
#else
    ps->font_name = g_strdup( "-*-helvetica-medium-r-normal-*-8-*-*-*-p-*-"
			      "iso8859-1" );
#endif
    gtk_widget_modify_style( pwPrompt, ps );

    for( i = 1; aszVersion[ i ]; i++ ) {
	gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt =
			    gtk_label_new( gettext ( aszVersion[ i ] ) ),
			    FALSE, FALSE, 0 );
	gtk_widget_modify_style( pwPrompt, ps );
    }
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( _("GNU Backgammon was written by:") ),
			FALSE, FALSE, 8 );
    
    gtk_box_pack_start( GTK_BOX( pwBox ), pwHBox, FALSE, FALSE, 0 );

    for( i = 0; i < 6; i++ )
	gtk_container_add( GTK_CONTAINER( pwHBox ),
			   gtk_label_new( TRANS( aszAuthors[ i ] ) ) );
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( _("With special thanks to:") ),
			FALSE, FALSE, 8 );

    gtk_box_pack_start( GTK_BOX( pwBox ), pwHBox = gtk_hbox_new( FALSE, 0 ),
			TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( pwHBox ), pwScrolled, TRUE, FALSE, 0 );
    gtk_widget_set_usize( pwScrolled, 200, 100 );
    gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( pwScrolled ),
					   pwList );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolled ),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    
    for( i = 0; aszCredits[ i ]; i++ ) {
	gtk_container_add( GTK_CONTAINER( pwList ),
			   gtk_list_item_new_with_label(
			       TRANS( aszCredits[ i ] ) ) );
    }

    gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt = gtk_label_new(
	_("GNU Backgammon is free software, covered by the GNU General Public "
	"License version 2, and you are welcome to change it and/or "
	"distribute copies of it under certain conditions.  There is "
	"absolutely no warranty for GNU Backgammon.") ), FALSE, FALSE, 4 );
    gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );
    gtk_widget_modify_style( pwPrompt, ps );
    gtk_rc_style_unref( ps );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwButton = gtk_button_new_with_label(
			   _("Copying conditions") ) );
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( CommandShowCopying ), NULL );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwButton = gtk_button_new_with_label(
			   _("Warranty") ) );
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( CommandShowWarranty ), NULL );
    
    gtk_widget_show_all( pwDialog );
}

#if HAVE_GTKTEXI
static int ShowManualSection( char *szTitle, char *szNode ) {
    
    static GtkWidget *pw;
    char *pch;
    
    if( pw ) {
	gtk_window_present( GTK_WINDOW( pw ) );
	gtk_texi_render_node( GTK_TEXI( pw ), szNode );
	return 0;
    }

    if( !( pch = PathSearch( "gnubg.xml", szDataDirectory ) ) )
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
    free( pch );
    gtk_texi_render_node( GTK_TEXI( pw ), szNode );

    return 0;
}
#endif

static void NoManual( void ) {
    
    outputl( _("The online manual is not available with this installation of "
	     "GNU Backgammon.  You can view the manual on the WWW at:\n"
	     "\n"
	     "    http://www.gnu.org/manual/gnubg/") );

    outputx();
}

static void NoFAQ( void ) {
    
    outputl( _("The Frequently Asked Questions are not available with this "
	       "installation of GNU Backgammon.  You can view them on "
	       "the WWW at:\n"
	       "\n"
	       "    http://mole.dnsalias.org/~acepoint/GnuBG/gnubg-faq/") );

    outputx();
}

static void ShowManual( gpointer *p, guint n, GtkWidget *pwEvent ) {
#if HAVE_GTKTEXI
    if( !ShowManualSection( _("GNU Backgammon - Manual"), "Top" ) )
	return;
#endif

    NoManual();
}


static void
ShowManualWeb( gpointer *p, guint n, GtkWidget *pwEvent ) {

  OpenURL( "http://www.gnu.org/manual/gnubg/" );

}

static void
ReportBug( gpointer *p, guint n, GtkWidget *pwEvent ) {

  OpenURL( "http://savannah.gnu.org/bugs/?func=addbug&group=gnubg" );

}


static void ShowFAQ( gpointer *p, guint n, GtkWidget *pwEvent ) {
#if HAVE_GTKTEXI
    if( !ShowManualSection( _("GNU Backgammon - Frequently Asked Questions"),
			    "Frequently Asked Questions" ) )
	return;
#endif

    NoFAQ();
}

#if GTK_CHECK_VERSION(2,0,0)
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
	
	pw = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	g_object_add_weak_pointer( G_OBJECT( pw ), (gpointer *) &pw );
	gtk_window_set_title( GTK_WINDOW( pw ), _("GNU Backgammon - Help") );
	gtk_window_set_default_size( GTK_WINDOW( pw ), 500, 400 );

	gtk_container_add( GTK_CONTAINER( pw ), pwPaned = gtk_vpaned_new() );
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
#endif

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

    if( !pwDialog ) {
	pwDialog = GTKCreateDialog( _("GNU Backgammon"), DT_INFO, NULL, NULL );
#if GTK_CHECK_VERSION(2,0,0)
	gtk_window_set_role( GTK_WINDOW( pwDialog ), "progress" );
	gtk_window_set_type_hint( GTK_WINDOW( pwAnnotation ),
			      GDK_WINDOW_TYPE_HINT_DIALOG );
#endif
	gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
	gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			    GTK_SIGNAL_FUNC( GTKBearoffProgressCancel ),
			    NULL );

	gtk_box_pack_start( GTK_BOX( DialogArea( pwDialog, DA_MAIN ) ),
			    pwAlign = gtk_alignment_new( 0.5, 0.5, 1, 0 ),
			    TRUE, TRUE, 8 );
	gtk_container_add( GTK_CONTAINER( pwAlign ),
			   pw = gtk_progress_bar_new() );
	
	gtk_progress_set_format_string( GTK_PROGRESS( pw ),
					_("Generating bearoff database (%p%%)") );
	gtk_progress_set_show_text( GTK_PROGRESS( pw ), TRUE );
	
	gtk_widget_show_all( pwDialog );
    }

    gtk_progress_set_percentage( GTK_PROGRESS( pw ), i / 54264.0 );

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

	gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 1,
				    (ap[ 0 ].szName) );
	gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 2,
				    (ap[ 1 ].szName) );

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

	enable_sub_menu( gtk_item_factory_get_widget( pif, "/File/Save" ),
			 plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	    pif, "/File/Save/Weights..." ), TRUE );
	enable_sub_menu( gtk_item_factory_get_widget( pif, "/File/Export" ),
			 plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
				      pif, "/File/Export/HTML Images..." ),
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
	
	fAutoCommand = FALSE;
    } else if( p == &ms.fCrawford )
	ShowBoard(); /* this is overkill, but it works */
    else if( p == &fAnnotation ) {
	if( fAnnotation ) {
#if USE_OLD_LAYOUT
          setWindowGeometry ( pwAnnotation, &awg[ WINDOW_ANNOTATION ] );
          gtk_widget_show_all( pwAnnotation );
          if( pwAnnotation->window )
		gdk_window_raise( pwAnnotation->window ); 
#else
	    ShowAnnotation();
#endif
	} else {
#if USE_OLD_LAYOUT
	    /* FIXME actually we should unmap the window, and send a synthetic
	       UnmapNotify event to the window manager -- see the ICCCM */
          getWindowGeometry ( &awg[ WINDOW_ANNOTATION ], pwAnnotation );
	  gtk_widget_hide(pwAnnotation);
#else
          DeleteAnnotation();
        }
    }
    else if( p == &fGameList ) {
	if( fGameList ) {
          ShowGameWindow();
	} else {
          DeleteGame();
#endif
        }
    }
    else if( p == &fMessage ) {
	if( fMessage ) {
#if USE_OLD_LAYOUT
          setWindowGeometry ( pwMessage, &awg[ WINDOW_MESSAGE ] );
          gtk_widget_show_all( pwMessage );
          if( pwMessage->window )
            gdk_window_raise( pwMessage->window );
#else
          gtk_widget_show_all( pwMessage );
#endif
	} else {
#if USE_OLD_LAYOUT
          /* FIXME actually we should unmap the window, and send a synthetic
             UnmapNotify event to the window manager -- see the ICCCM */
          getWindowGeometry ( &awg[ WINDOW_MESSAGE ], pwMessage );
#endif
          gtk_widget_hide( pwMessage );
        }
    } else if( p == &fGUIDiceArea ) {
	if( GTK_WIDGET_REALIZED( pwBoard ) )
	{
#if USE_BOARD3D
		/* If in 3d mode may need to update sizes */
		if (rdAppearance.fDisplayType == DT_3D)
			SetupViewingVolume3d(bd, &rdAppearance);
		else
#endif
		{    
			if( GTK_WIDGET_REALIZED( pwBoard ) ) {
			    if( GTK_WIDGET_VISIBLE( bd->dice_area ) && !fGUIDiceArea )
				gtk_widget_hide( bd->dice_area );
			    else if( ! GTK_WIDGET_VISIBLE( bd->dice_area ) && fGUIDiceArea )
				gtk_widget_show_all( bd->dice_area );
			}
		}}
    } else if( p == &fGUIShowIDs ) {
	BoardData *bd = BOARD( pwBoard )->board_data;
    
	if( GTK_WIDGET_REALIZED( pwBoard ) ) {
	    if( GTK_WIDGET_VISIBLE( bd->vbox_ids ) && !fGUIShowIDs )
		gtk_widget_hide( bd->vbox_ids );
	    else if( !GTK_WIDGET_VISIBLE( bd->vbox_ids ) && fGUIShowIDs )
		gtk_widget_show_all( bd->vbox_ids );
	}
    } else if( p == &fGUIShowPips )
	ShowBoard(); /* this is overkill, but it works */
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
		sprintf ( strchr ( pStr, 0 ), "%-37.37s ", 
			  ( gtk_clist_get_text ( pList, i, 0, &sz ) ) ?
			  sz : "" );

		sprintf ( strchr ( pStr, 0 ), "%-20.20s ", 
			  ( gtk_clist_get_text ( pList, i, 1, &sz ) ) ?
			  sz : "" );

		sprintf ( strchr ( pStr, 0 ), "%-20.20s\n", 
			  ( gtk_clist_get_text ( pList, i, 2, &sz ) ) ?
			  sz : "" );
	}
	sprintf ( strchr ( pStr, 0 ), "\n");
}

static void CopyData(GtkWidget *pwNotebook, enum _formatgs page)
{
	char szOutput[4096];

	sprintf(szOutput, "%-37.37s %-20.20s %-20.20s\n", "", ap[ 0 ].szName, ap[ 1 ].szName);

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

static GtkWidget *CreateList()
{
	int i;
        static char *aszEmpty[] = { NULL, NULL, NULL };
	GtkWidget *pwList = gtk_clist_new_with_titles( 3, aszEmpty );

	for( i = 0; i < 3; i++ )
	{
		gtk_clist_set_column_auto_resize( GTK_CLIST( pwList ), i, TRUE );
		gtk_clist_set_column_justification( GTK_CLIST( pwList ), i, GTK_JUSTIFY_RIGHT );
	}
	gtk_clist_column_titles_passive( GTK_CLIST( pwList ) );
  
	gtk_clist_set_column_title( GTK_CLIST( pwList ), 1, (ap[0].szName));
	gtk_clist_set_column_title( GTK_CLIST( pwList ), 2, (ap[1].szName));

	gtk_clist_set_selection_mode( GTK_CLIST( pwList ), GTK_SELECTION_SINGLE);

	return pwList;
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
	movegameinfo *pmgi;
	int i;

	if (!game)
		return &scMatch;
	else
	{
		list *plGame, *pl = lMatch.plNext;
		for (i = 1; i < game; i++)
			pl = pl->plNext;

		plGame = pl->p;
		pmgi = plGame->plNext->p;
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

static GtkWidget *StatsPixmapButton(GdkColormap *pcmap, char **xpm,
				void *fn )
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

#include "prevgame.xpm"
#include "prevmove.xpm"
#include "nextmove.xpm"
#include "nextgame.xpm"
#include "allgames.xpm"

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

static void
StatcontextSelect ( GtkWidget *pw, int y, int x, GdkEventButton *peb,
                    GtkWidget *pwCopy ) {

  int c;
  GList *pl;

  for ( c = 0, pl = GTK_CLIST ( pw )->selection; c < 2 && pl; pl = pl->next )
    c++;

  if ( c && peb )
    gtk_selection_owner_set ( pw, GDK_SELECTION_PRIMARY, peb->time );

  gtk_widget_set_sensitive ( GTK_WIDGET ( pwCopy ), c );

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

static void
StatcontextGetSelection ( GtkWidget *pw, GtkSelectionData *psd,
                          guint n, guint t, void *unused ) {

  GList *pl;
  GList *plCopy;
  int i;
  static char szOutput[ 4096 ];
  char *pc;
  gchar *sz;

  sprintf ( szOutput, 
            "%-37.37s %-20.20s %-20.20s\n",
            "", ap[ 0 ].szName, ap[ 1 ].szName );

  /* copy list (note that the integers in the list are NOT copied) */
  plCopy = g_list_copy( GTK_CLIST ( pw )->selection );

  /* sort list; otherwise the lines are returned in whatever order the
     user clicked the lines (bug #4160) */
  plCopy = g_list_sort( plCopy, compare_func );

  for ( pl = plCopy; pl; pl = pl->next ) {

    i = GPOINTER_TO_INT( pl->data );

    sprintf ( pc = strchr ( szOutput, 0 ), "%-37.37s ", 
              ( gtk_clist_get_text ( GTK_CLIST ( pw ), i, 0, &sz ) ) ?
              sz : "" );
      
    sprintf ( pc = strchr ( szOutput, 0 ), "%-20.20s ", 
              ( gtk_clist_get_text ( GTK_CLIST ( pw ), i, 1, &sz ) ) ?
              sz : "" );
      
    sprintf ( pc = strchr ( szOutput, 0 ), "%-20.20s\n", 
              ( gtk_clist_get_text ( GTK_CLIST ( pw ), i, 2, &sz ) ) ?
              sz : "" );
      
  }

  /* garbage collect */
  g_list_free( plCopy );

  gtk_selection_data_set( psd, GDK_SELECTION_TYPE_STRING, 8,
                          szOutput, strlen( szOutput ) );

}

static gint
StatcontextClearSelection  ( GtkWidget *pw, GdkEventSelection *pes,
                             void *unused ) {

  gtk_clist_unselect_all ( GTK_CLIST ( pw ) );

  return TRUE;

}

static void
StatcontextCopy ( GtkWidget *pw, void *unused ) {

  UserCommand ( "xcopy" );

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
		movegameinfo *pmgi = plGame->plNext->p;
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
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( StatcontextCopy ), pwNotebook );
    gtk_widget_set_sensitive( menu_item, FALSE );

	gtk_signal_connect( GTK_OBJECT( pwList ), "select-row",
					  GTK_SIGNAL_FUNC( StatcontextSelect ), menu_item );
	gtk_signal_connect( GTK_OBJECT( pwList ), "unselect-row",
					  GTK_SIGNAL_FUNC( StatcontextSelect ), menu_item );
	gtk_signal_connect( GTK_OBJECT( pwList ), "selection_clear_event",
					  GTK_SIGNAL_FUNC( StatcontextClearSelection ), 0 );
	gtk_signal_connect( GTK_OBJECT( pwList ), "selection_get",
					  GTK_SIGNAL_FUNC( StatcontextGetSelection ), 0 );

	menu_item = gtk_menu_item_new_with_label ("Copy all");
	gtk_menu_shell_append (GTK_MENU_SHELL (copyMenu), menu_item);
	gtk_widget_show (menu_item);
	gtk_signal_connect( GTK_OBJECT( menu_item ), "activate", GTK_SIGNAL_FUNC( CopyAll ), pwNotebook );

	gtk_signal_connect( GTK_OBJECT( pwList ), "button-press-event", GTK_SIGNAL_FUNC( ContextCopyMenu ), copyMenu );

	gtk_clist_set_selection_mode( GTK_CLIST( pwList ), GTK_SELECTION_EXTENDED );

	gtk_selection_add_target( pwList, GDK_SELECTION_PRIMARY,
							GDK_SELECTION_TYPE_STRING, 0 );

	/* modality */
	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pwUsePanels ) ) )
	    gtk_window_set_default_size( GTK_WINDOW( pwStatDialog ), 0, 300 );
	else
	    gtk_window_set_default_size( GTK_WINDOW( pwStatDialog ), 0, 600 );
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

/*
 * Copy
 * 
 * - Obtain the contents of the primary selection
 * - claim the clipboard
 *
 */

extern void
GTKCopy ( void ) {

  gtk_selection_owner_set ( pwMain, gdk_atom_intern ("CLIPBOARD", FALSE), 
                            GDK_CURRENT_TIME );

  /* get contents of primary selection */

  gtk_selection_convert ( pwMain, GDK_SELECTION_PRIMARY, 
                          GDK_SELECTION_TYPE_STRING, 
                          GDK_CURRENT_TIME );

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
#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 10 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 10 );
#else
    char sz[ 4096 ];
#endif
    
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
    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    id = gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			     GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    pwTable = gtk_table_new( 2, 9, FALSE );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwTable );

    sprintf( sz, _("%s's rating:"), ap[ 0 ].szName );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( sz ),
		      0, 1, 0, 1, 0, 0, 0, 0 );
    sprintf( sz, _("%s's rating:"), ap[ 1 ].szName );
    gtk_table_attach( GTK_TABLE( pwTable ), gtk_label_new( sz ),
		      0, 1, 1, 2, 0, 0, 0, 0 );    
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Date:") ),
		      0, 1, 2, 3, 0, 0, 0, 0 );
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Event:") ),
		      0, 1, 3, 4, 0, 0, 0, 0 );
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Round:") ),
		      0, 1, 4, 5, 0, 0, 0, 0 );
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Place:") ),
		      0, 1, 5, 6, 0, 0, 0, 0 );
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Annotator:") ),
		      0, 1, 6, 7, 0, 0, 0, 0 );
    gtk_table_attach( GTK_TABLE( pwTable ),
		      gtk_label_new( _("Comments:") ),
		      0, 1, 7, 8, 0, 0, 0, 0 );

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
        
    pwComment = gtk_text_new( NULL, NULL ) ;
    gtk_text_set_word_wrap( GTK_TEXT( pwComment ), TRUE );
    gtk_text_set_editable( GTK_TEXT( pwComment ), TRUE );
    gtk_table_attach_defaults( GTK_TABLE( pwTable ), pwComment, 0, 2, 8, 9 );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK ) {
	int nYear, nMonth, nDay;
	
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
	pch = gtk_editable_get_chars( GTK_EDITABLE( pwComment ), 0, -1 );
	UpdateMatchinfo( pch, "comment", &mi.pchComment );

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
					       2, G_MAXDOUBLE, 100,
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
				  GTK_WINDOW( pwMain ) );

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


static void 
PythonShell( gpointer *p, guint n, GtkWidget *pw ) {

  char *pch = g_strdup( ">import idle.PyShell; idle.PyShell.main()\n" );

  UserCommand( pch );

  g_free( pch );

}

static void
ShowAllPanels ( gpointer *p, guint n, GtkWidget *pw ) {
  ShowAnnotation();
  ShowMessage();
  ShowGameWindow();
}

static void
HideAllPanels ( gpointer *p, guint n, GtkWidget *pw ) {
  DeleteAnnotation();
  DeleteMessage();
  DeleteGame();
  gtk_window_set_default_size( GTK_WINDOW( pwMain ),
                                     awg[ WINDOW_MAIN ].nWidth,
                                     awg[ WINDOW_MAIN ].nHeight );
}

static void
TogglePanel ( gpointer *p, guint n, GtkWidget *pw ) {
  int f;

  g_assert( GTK_IS_CHECK_MENU_ITEM( pw ) );
  
  f = GTK_CHECK_MENU_ITEM( pw )->active ;
  switch( n ) {
	  case TOGGLE_ANNOTATION:
		if (f)
			ShowAnnotation();
		else
		        DeleteAnnotation();
                break;
	  case TOGGLE_GAMELIST:
		if(f)
			ShowGameWindow();
		else
			DeleteGame();
                break;
	  case TOGGLE_MESSAGE:
		if(f)
			ShowMessage();
		else
	                DeleteMessage();
                break;
  }
  gtk_window_set_default_size( GTK_WINDOW( pwMain ),
                                     awg[ WINDOW_MAIN ].nWidth,
                                     awg[ WINDOW_MAIN ].nHeight );
}

static void
FullScreenMode( gpointer *p, guint n, GtkWidget *pw ) {
	BoardData *bd = BOARD( pwBoard )->board_data;
	GtkWidget *pwHandle = gtk_widget_get_parent(pwToolbar);
	fGUIShowIDs = FALSE;
	UpdateSetting(&fGUIShowIDs);

	fGUIShowGameInfo = FALSE;
	
	gtk_widget_hide(pwMenuBar);
	gtk_widget_hide(pwToolbar);
	gtk_widget_hide(pwHandle);
	gtk_widget_hide(GTK_WIDGET(bd->table));
	gtk_widget_hide(GTK_WIDGET(bd->dice_area));
	gtk_widget_hide(pwStatus);
	gtk_widget_hide(pwProgress);
        HideAllPanels(NULL, 0, NULL);

	/* How can I maximize the window ?? */
#if USE_GTK2
{
	GtkWindow* ptl = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(bd->table)));
	gtk_window_maximize(ptl);
	gtk_window_set_decorated(ptl, FALSE);
}
#else
	/* Not sure about gtk1...
	gdk_window_set_decorations(GTK_WIDGET(ptl)->window, 0);
	*/
#endif
}
