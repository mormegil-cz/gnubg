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

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <assert.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#define GTK_ENABLE_BROKEN /* for GtkText */
#include <gtk/gtk.h>
#if HAVE_GTKEXTRA_GTKSHEET_H
#include <gtkextra/gtksheet.h>
#endif
#include <gdk/gdkkeysyms.h>
#if HAVE_GDK_GDKX_H
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#endif

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
#include "gtkgame.h"
#include "gtkprefs.h"
#include "gtktexi.h"
#include "matchequity.h"
#include "positionid.h"



#define GNUBGMENURC ".gnubgmenurc"

#if !GTK_CHECK_VERSION(1,3,10)
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
    CMD_NEXT_ROLL,
    CMD_PLAY,
    CMD_PREV,
    CMD_PREV_GAME,
    CMD_PREV_ROLL,
    CMD_QUIT,
    CMD_REDOUBLE,
    CMD_RESIGN_N,
    CMD_RESIGN_G,
    CMD_RESIGN_B,
    CMD_ROLL,
    CMD_ROLLOUT,
    CMD_ROLLOUT_CUBE,
    CMD_SAVE_SETTINGS,
    CMD_SET_ANALYSIS_CUBE,
    CMD_SET_ANALYSIS_LUCK,
    CMD_SET_ANALYSIS_MOVES,
    CMD_SET_ANNOTATION_ON,
    CMD_SET_APPEARANCE,
    CMD_SET_AUTO_ANALYSIS,
    CMD_SET_AUTO_BEAROFF,
    CMD_SET_AUTO_CRAWFORD,
    CMD_SET_AUTO_GAME,
    CMD_SET_AUTO_MOVE,
    CMD_SET_AUTO_ROLL,
    CMD_SET_CLOCKWISE,
    CMD_SET_CONFIRM_NEW,
    CMD_SET_CONFIRM_SAVE,
    CMD_SET_CUBE_USE,
    CMD_SET_DISPLAY,
    CMD_SET_EGYPTIAN,
    CMD_SET_INVERT_MET,
    CMD_SET_JACOBY,
    CMD_SET_NACKGAMMON,
    CMD_SET_OUTPUT_MATCHPC,
    CMD_SET_OUTPUT_MWC,
    CMD_SET_OUTPUT_WINPC,
    CMD_SET_RECORD,
    CMD_SET_RNG_ANSI,
    CMD_SET_RNG_BSD,
    CMD_SET_RNG_ISAAC,
    CMD_SET_RNG_MANUAL,
    CMD_SET_RNG_MD5,
    CMD_SET_RNG_MERSENNE,
    CMD_SET_RNG_USER,
    CMD_SET_TURN_0,
    CMD_SET_TURN_1,
    CMD_SHOW_COPYING,
    CMD_SHOW_ENGINE,
    CMD_SHOW_GAMMONVALUES,
    CMD_SHOW_MARKETWINDOW,
    CMD_SHOW_MATCHEQUITYTABLE,
    CMD_SHOW_KLEINMAN,
    CMD_SHOW_PATH,
    CMD_SHOW_PIPCOUNT,
    CMD_SHOW_STATISTICS_GAME,
    CMD_SHOW_STATISTICS_MATCH,
    CMD_SHOW_STATISTICS_SESSION,
    CMD_SHOW_THORP,
    CMD_SHOW_VERSION,
    CMD_SHOW_WARRANTY,
    CMD_SWAP_PLAYERS,
    CMD_TAKE,
    CMD_TRAIN_DATABASE,
    CMD_TRAIN_TD,
    NUM_CMDS
} gnubgcommand;
   
typedef struct _togglecommand {
    int *p;
    gnubgcommand cmd;
} togglecommand;

static togglecommand atc[] = {
    { &fAnalyseCube, CMD_SET_ANALYSIS_CUBE },
    { &fAnalyseDice, CMD_SET_ANALYSIS_LUCK },
    { &fAnalyseMove, CMD_SET_ANALYSIS_MOVES },
    { &fAutoAnalysis, CMD_SET_AUTO_ANALYSIS },
    { &fAutoBearoff, CMD_SET_AUTO_BEAROFF },
    { &fAutoCrawford, CMD_SET_AUTO_CRAWFORD },
    { &fAutoGame, CMD_SET_AUTO_GAME },
    { &fAutoMove, CMD_SET_AUTO_MOVE },
    { &fAutoRoll, CMD_SET_AUTO_ROLL },
    { &fClockwise, CMD_SET_CLOCKWISE },
    { &fConfirm, CMD_SET_CONFIRM_NEW },
    { &fConfirmSave, CMD_SET_CONFIRM_SAVE },
    { &fCubeUse, CMD_SET_CUBE_USE },
    { &fDisplay, CMD_SET_DISPLAY },
    { &fEgyptian, CMD_SET_EGYPTIAN },
    { &fJacoby, CMD_SET_JACOBY },
    { &fNackgammon, CMD_SET_NACKGAMMON },
    { &fOutputMatchPC, CMD_SET_OUTPUT_MATCHPC },
    { &fOutputMWC, CMD_SET_OUTPUT_MWC },
    { &fOutputWinPC, CMD_SET_OUTPUT_WINPC },
    { &fRecord, CMD_SET_RECORD },
    { &fInvertMET, CMD_SET_INVERT_MET },
    { NULL }
};

static char *aszCommands[ NUM_CMDS ] = {
    "agree",
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
    "next roll",
    "play",
    "previous",
    "previous game",
    "previous roll",
    "quit",
    "redouble",
    "resign normal",
    "resign gammon",
    "resign backgammon",
    "roll",
    "rollout",
    "rollout =cube",
    "save settings",
    "set analysis cube",
    "set analysis luck",
    "set analysis moves",
    "set annotation on",
    NULL, /* set appearance */
    "set automatic analysis",
    "set automatic bearoff",
    "set automatic crawford",
    "set automatic game",
    "set automatic move",
    "set automatic roll",
    "set clockwise",
    "set confirm new",
    "set confirm save",
    "set cube use",
    "set display",
    "set egyptian",
    "set invert matchequitytable",
    "set jacoby",
    "set nackgammon",
    "set output matchpc",
    "set output mwc",
    "set output winpc",
    "set record",
    "set rng ansi",
    "set rng bsd",
    "set rng isaac",
    "set rng manual",
    "set rng md5",
    "set rng mersenne",
    "set rng user",
    NULL, /* set turn 0 */
    NULL, /* set turn 1 */
    "show copying",
    "show engine",
    "show gammonvalues",
    "show marketwindow",
    "show matchequitytable",
    "show kleinman",
    "show path",
    "show pipcount",
    "show statistics game",
    "show statistics match",
    "show statistics session",
    "show thorp",
    "show version",
    "show warranty",
    "swap players",
    "take",
    "train database",
    "train td"
};

static char szSetCubeValue[20]; /* set cube value XX */
static char szSetCubeOwner[20]; /* set cube owner X */

static void DatabaseExport( gpointer *p, guint n, GtkWidget *pw );
static void DatabaseImport( gpointer *p, guint n, GtkWidget *pw );
static void EnterCommand( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameGam( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportGamePDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportGameHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportGamePostScript( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchMat( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchPDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchPostScript( gpointer *p, guint n, GtkWidget *pw );
static void ExportMatchHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionEPS( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionPos( gpointer *p, guint n, GtkWidget *pw );
static void ExportPositionHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionLaTeX( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionPDF( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionHtml( gpointer *p, guint n, GtkWidget *pw );
static void ExportSessionPostScript( gpointer *p, guint n, GtkWidget *pw );
static void ImportMat( gpointer *p, guint n, GtkWidget *pw );
static void ImportOldmoves( gpointer *p, guint n, GtkWidget *pw );
static void ImportPos( gpointer *p, guint n, GtkWidget *pw );
static void ImportSGG( gpointer *p, guint n, GtkWidget *pw );
static void LoadCommands( gpointer *p, guint n, GtkWidget *pw );
static void LoadGame( gpointer *p, guint n, GtkWidget *pw );
static void LoadMatch( gpointer *p, guint n, GtkWidget *pw );
static void NewMatch( gpointer *p, guint n, GtkWidget *pw );
static void NewWeights( gpointer *p, guint n, GtkWidget *pw );
static void SaveGame( gpointer *p, guint n, GtkWidget *pw );
static void SaveMatch( gpointer *p, guint n, GtkWidget *pw );
static void SaveWeights( gpointer *p, guint n, GtkWidget *pw );
static void SetAlpha( gpointer *p, guint n, GtkWidget *pw );
static void SetAnalysis( gpointer *p, guint n, GtkWidget *pw );
static void SetAnalysisEvalChequer( gpointer *p, guint n, GtkWidget *pw );
static void SetAnalysisEvalCube( gpointer *p, guint n, GtkWidget *pw );
static void SetAnneal( gpointer *p, guint n, GtkWidget *pw );
static void SetAutoDoubles( gpointer *p, guint n, GtkWidget *pw );
static void SetBeavers( gpointer *p, guint n, GtkWidget *pw );
static void SetCache( gpointer *p, guint n, GtkWidget *pw );
static void SetDelay( gpointer *p, guint n, GtkWidget *pw );
static void SetEvalChequer( gpointer *p, guint n, GtkWidget *pw );
static void SetEvalCube( gpointer *p, guint n, GtkWidget *pw );
static void SetMET( gpointer *p, guint n, GtkWidget *pw );
static void SetPlayers( gpointer *p, guint n, GtkWidget *pw );
static void SetRollouts( gpointer *p, guint n, GtkWidget *pw );
static void SetSeed( gpointer *p, guint n, GtkWidget *pw );
static void SetThreshold( gpointer *p, guint n, GtkWidget *pw );
static void SetCubeValue( GtkWidget *wd, int data);
static void SetCubeOwner( GtkWidget *wd, int i);
static void ShowManual( gpointer *p, guint n, GtkWidget *pw );

/* A dummy widget that can grab events when others shouldn't see them. */
static GtkWidget *pwGrab;

GtkWidget *pwBoard, *pwMain, *pwMenuBar;
static GtkWidget *pwStatus, *pwProgress, *pwGame, *pwGameList, *pom,
    *pwAnnotation, *pwAnalysis, *pwCommentary, *pwHint, *pwSetCube;
static moverecord *pmrAnnotation;
static GtkAccelGroup *pagMain;
static GtkStyle *psGameList, *psCurrent;
static int yCurrent, xCurrent; /* highlighted row/col in game record */
static GtkItemFactory *pif;
guint nNextTurn = 0; /* GTK idle function */
static int cchOutput;
static guint idOutput, idProgress;
static list lOutput;
int fTTY = TRUE;
static guint nStdin, nDisabledCount = 1;

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

  gtk_window_set_default_size( GTK_WINDOW( pw ), 
                               ( pwg->nWidth >= 0 ) ? pwg->nWidth : 0,
                               ( pwg->nHeight >= 0 ) ? pwg->nHeight : 0 );
  
  gtk_widget_set_uposition ( pw, 
                             ( pwg->nPosX >= 0 ) ? pwg->nPosX : 0, 
                             ( pwg->nPosY >= 0 ) ? pwg->nPosY : 0 );

}

static void
getWindowGeometry ( windowgeometry *pwg, GtkWidget *pw ) {

  gdk_window_get_position ( pw->window,
                            &pwg->nPosX, &pwg->nPosY );

  gdk_window_get_size ( pw->window,
                        &pwg->nWidth, &pwg->nHeight );

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

    int f, id;
    
    GTKDisallowStdin();
    
    while( !fInterrupt && !fEndDelay ) {
	/* Grab events so that the board window knows this is a re-entrant
	   call, and won't allow commands like roll, move or double. */
	if( ( f = !GTK_WIDGET_HAS_GRAB( pwGrab ) ) )
	    gtk_grab_add( pwGrab );
	
	id = gtk_signal_connect_after( GTK_OBJECT( pwGrab ), "key-press-event",
				       GTK_SIGNAL_FUNC( gtk_true ), NULL );
	
	gtk_main_iteration();

	gtk_signal_disconnect( GTK_OBJECT( pwGrab ), id );
	
	if( f )
	    gtk_grab_remove( pwGrab );
    }
    
    fEndDelay = FALSE;
    
    GTKAllowStdin();
}

extern void HandleXAction( void ) {

    int f, id;
    
    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    /* Grab events so that the board window knows this is a re-entrant
       call, and won't allow commands like roll, move or double. */
    if( ( f = !GTK_WIDGET_HAS_GRAB( pwGrab ) ) )
	gtk_grab_add( pwGrab );
    
    id = gtk_signal_connect_after( GTK_OBJECT( pwGrab ), "key-press-event",
				   GTK_SIGNAL_FUNC( gtk_true ), NULL );
    
    /* Don't check stdin here; readline isn't ready yet. */
    GTKDisallowStdin();
    
    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( gtk_events_pending() )
	gtk_main_iteration();

    GTKAllowStdin();

    gtk_signal_disconnect( GTK_OBJECT( pwGrab ), id );
    
    if( f )
	gtk_grab_remove( pwGrab );
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

    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Dice", TRUE,
					NULL, NULL ),
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

static void SetDice( gpointer *p, guint n, GtkWidget *pw ) {

    int an[ 2 ];
    char sz[ 13 ]; /* "set dice x y" */

    if( !GTKGetManualDice( an ) ) {
	sprintf( sz, "set dice %d %d", an[ 0 ], an[ 1 ] );
	UserCommand( sz );
    }
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
	    pmr = pmrPrev;
	    break;
	} else if( pl->plNext->p == pmr ) {
	    pmr = pl->p;
	    break;
	}
    }

    if( pl == plGame )
	/* couldn't find the moverecord they selected */
	return;
    
    plLastMove = pl;
    
    CalculateBoard();

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

  getWindowGeometry ( &wgAnnotation, pwAnnotation );
  fAnnotation = FALSE;
  UpdateSetting( &fAnnotation );

}

static void DeleteGame( void ) {

  getWindowGeometry ( &wgGame, pwGame );
  gtk_widget_hide ( pwGame );

}

typedef struct _hintdata {
    GtkWidget *pwMove, *pwRollout, *pw;
    movelist *pml;
    int fButtonsValid;
} hintdata;

static int CheckHintButtons( void ) {

    int c;
    GList *pl;
    hintdata *phd = gtk_object_get_user_data( GTK_OBJECT( pwHint ) );
    GtkWidget *pw = phd->pw;

    for( c = 0, pl = GTK_CLIST( pw )->selection; c < 2 && pl; pl = pl->next )
	c++;

    gtk_widget_set_sensitive( phd->pwMove, c == 1 && phd->fButtonsValid );
    gtk_widget_set_sensitive( phd->pwRollout, c && phd->fButtonsValid );

    return c;
}

static GtkWidget *CreateMoveList( hintdata *phd, int iHighlight ) {

    static int aanColumns[][ 2 ] = {
	{ 2, OUTPUT_WIN },
	{ 3, OUTPUT_WINGAMMON },
	{ 4, OUTPUT_WINBACKGAMMON },
	{ 6, OUTPUT_LOSEGAMMON },
	{ 7, OUTPUT_LOSEBACKGAMMON }
    };
    static char *aszTitle[] = {
	"Rank", "Type", "Win", "W g", "W bg", "Lose", "L g", "L bg",
	"", "Diff.", "Move"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		       NULL, NULL };
    GtkWidget *pwMoves = gtk_clist_new_with_titles( 11, aszTitle );
    int i, j;
    char sz[ 32 ];
    float rBest;
    cubeinfo ci;
    movelist *pml = phd->pml;

    /* This function should only be called when the game state matches
       the move list. */
    assert( ms.fMove == 0 || ms.fMove == 1 );
    
    for( i = 0; i < 11; i++ ) {
	gtk_clist_set_column_auto_resize( GTK_CLIST( pwMoves ), i, TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( pwMoves ), i,
					    i == 1 || i == 10 ?
					    GTK_JUSTIFY_LEFT :
					    GTK_JUSTIFY_RIGHT );
    }
    gtk_clist_column_titles_passive( GTK_CLIST( pwMoves ) );
    gtk_clist_set_selection_mode( GTK_CLIST( pwMoves ),
				  GTK_SELECTION_MULTIPLE );

    GetMatchStateCubeInfo( &ci, &ms );
    
    if( fOutputMWC && ms.nMatchTo ) {
	gtk_clist_set_column_title( GTK_CLIST( pwMoves ), 8, "MWC" );
	rBest = 100.0f * eq2mwc ( pml->amMoves[ 0 ].rScore, &ci );
    } else {
	gtk_clist_set_column_title( GTK_CLIST( pwMoves ), 8, "Equity" );
	rBest = pml->amMoves[ 0 ].rScore;
    }
    
    for( i = 0; i < pml->cMoves; i++ ) {
	float *ar = pml->amMoves[ i ].arEvalMove;

	gtk_clist_append( GTK_CLIST( pwMoves ), aszEmpty );

	gtk_clist_set_row_data( GTK_CLIST( pwMoves ), i, pml->amMoves + i );

	if( i && i == pml->cMoves - 1 && i == iHighlight )
	    /* The move made is the last on the list.  Some moves might
	       have been deleted to fit this one in, so its rank isn't
	       known. */
	    strcpy( sz, "??" );
	else
	    sprintf( sz, "%d", i + 1 );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 0, sz );

	FormatEval( sz, &pml->amMoves[ i ].esMove );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 1, sz );

	for( j = 0; j < 5; j++ ) {
	    if( fOutputWinPC )
		sprintf( sz, "%5.1f%%", ar[ aanColumns[ j ][ 1 ] ] * 100.0f );
	    else
		sprintf( sz, "%5.3f", ar[ aanColumns[ j ][ 1 ] ] );
	    
	    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, aanColumns[ j ][ 0 ],
				sz );
	}

	if( fOutputWinPC )
	    sprintf( sz, "%5.1f%%", ( 1.0f - ar[ OUTPUT_WIN ] ) * 100.0f );
	else
	    sprintf( sz, "%5.3f", 1.0f - ar[ OUTPUT_WIN ] );
	    
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 5, sz );

	if( fOutputMWC && ms.nMatchTo )
	    sprintf( sz, "%7.3f%%", 100.0f * eq2mwc( pml->amMoves[ i ].rScore,
						     &ci ) );
	else
	    sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 8, sz );

	if( i ) {
	    if( fOutputMWC && ms.nMatchTo )
		sprintf( sz, "%7.3f%%", eq2mwc( pml->amMoves[ i ].rScore, &ci )
			 * 100.0f - rBest );
	    else
		sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore - rBest );
	    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 9, sz );
	}
	
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 10,
			    FormatMove( sz, ms.anBoard,
					pml->amMoves[ i ].anMove ) );
    }

    if( iHighlight >= 0 ) {
	GtkStyle *ps;

	gtk_widget_ensure_style( pwMoves );
	ps = gtk_style_copy( pwMoves->style );

	ps->fg[ GTK_STATE_NORMAL ].red = ps->fg[ GTK_STATE_ACTIVE ].red =
	    ps->fg[ GTK_STATE_SELECTED ].red = 0xFFFF;
	ps->fg[ GTK_STATE_NORMAL ].green = ps->fg[ GTK_STATE_ACTIVE ].green =
	    ps->fg[ GTK_STATE_SELECTED ].green = 0;
	ps->fg[ GTK_STATE_NORMAL ].blue = ps->fg[ GTK_STATE_ACTIVE ].blue =
	    ps->fg[ GTK_STATE_SELECTED ].blue = 0;

	gtk_clist_set_row_style( GTK_CLIST( pwMoves ), iHighlight, ps );

	gtk_style_unref( ps );
    }
    
    return pwMoves;
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

static void CreateAnnotationWindow( void ) {

    GtkWidget *pwPaned;
    GtkWidget *vscrollbar, *pHbox;    

    pwAnnotation = gtk_window_new( GTK_WINDOW_TOPLEVEL );


    gtk_window_set_title( GTK_WINDOW( pwAnnotation ),
			  "GNU Backgammon - Annotation" );
    gtk_window_set_wmclass( GTK_WINDOW( pwAnnotation ), "annotation",
			    "Annotation" );

    setWindowGeometry ( pwAnnotation, &wgAnnotation );

    gtk_container_add( GTK_CONTAINER( pwAnnotation ),
		       pwPaned = gtk_vpaned_new() );
    
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
    
    gtk_signal_connect( GTK_OBJECT( pwAnnotation ), "delete_event",
			GTK_SIGNAL_FUNC( DeleteAnnotation ), NULL );

}

static void CreateGameWindow( void ) {

    static char *asz[] = { "Move", NULL, NULL };
    GtkWidget *psw = gtk_scrolled_window_new( NULL, NULL ),
	*pvbox = gtk_vbox_new( FALSE, 0 ),
	*phbox = gtk_hbox_new( FALSE, 0 ),
	*pm = gtk_menu_new();
    GtkStyle *ps;
    GdkColormap *pcmap;
    gint nMaxWidth; 

#include "prevgame.xpm"
#include "prevmove.xpm"
#include "nextmove.xpm"
#include "nextgame.xpm"
    
    pwGame = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    pcmap = gtk_widget_get_colormap( pwGame );
    
    gtk_window_set_title( GTK_WINDOW( pwGame ), "GNU Backgammon - "
			  "Game record" );
    gtk_window_set_wmclass( GTK_WINDOW( pwGame ), "gamerecord",
			    "GameRecord" );

    setWindowGeometry ( pwGame, &wgGame );
    
    gtk_container_add( GTK_CONTAINER( pwGame ), pvbox );

    gtk_box_pack_start( GTK_BOX( pvbox ), phbox, FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, prevgame_xpm, "previous game" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, prevmove_xpm, "previous roll" ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, nextmove_xpm, "next roll" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, nextgame_xpm, "next game" ),
			FALSE, FALSE, 0 );
        
    gtk_menu_append( GTK_MENU( pm ), gtk_menu_item_new_with_label(
	"(no game)" ) );
    gtk_widget_show_all( pm );
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pom = gtk_option_menu_new() ),
			      pm );
    gtk_option_menu_set_history( GTK_OPTION_MENU( pom ), 0 );
    gtk_box_pack_start( GTK_BOX( phbox ), pom, TRUE, TRUE, 4 );
/*    gtk_box_pack_start( GTK_BOX( phbox ), pwButton, FALSE, FALSE, 4 ); */
    
    gtk_container_add( GTK_CONTAINER( pvbox ), psw );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

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

    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ), "Move" );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 0, nMaxWidth );
    nMaxWidth = gdk_string_width( gtk_style_get_font( psCurrent ),
                                  " (set board AAAAAAAAAAAAAA)");
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 1, nMaxWidth );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 2, nMaxWidth );
    
    gtk_signal_connect( GTK_OBJECT( pwGameList ), "select-row",
			GTK_SIGNAL_FUNC( GameListSelectRow ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwGame ), "delete_event",
			GTK_SIGNAL_FUNC( DeleteGame ), NULL );

    /* FIXME gtk_widget_hide is no good -- we want a function that returns
       TRUE to avoid running the default handler */
    /* FIXME actually we should unmap the window, and send a synthetic
       UnmapNotify event to the window manager -- see the ICCCM */
}

extern void ShowGameWindow( void ) {

  setWindowGeometry ( pwGame, &wgGame );
    gtk_widget_show_all( pwGame );
    if( pwGame->window )
	gdk_window_raise( pwGame->window );
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
    int i, fPlayer;
    char *pch, sz[ 40 ];
    
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
	
	if( ms.fDoubled )
	    sprintf( sz, "Redouble to %d", ms.nCube << 2 );
	else
	    sprintf( sz, "Double to %d", ms.nCube << 1 );
	    
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_TAKE:
	fPlayer = pmr->d.fPlayer;
	strcpy( pch = sz, "Take" );
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_DROP:
	fPlayer = pmr->d.fPlayer;
	strcpy( pch = sz, "Drop" );
	strcat( sz, aszSkillTypeAbbr[ pmr->d.st ] );
	break;
	
    case MOVE_RESIGN:
	fPlayer = pmr->r.fPlayer;
	pch = " Resigns"; /* FIXME show value */
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

    static char *aszSkillCmd[ SKILL_VERYGOOD + 1 ] = {
	"verybad", "bad", "doubtful", "clear skill", "interesting", "good",
	"verygood"
    };
    char sz[ 64 ];

    sprintf( sz, "annotate %s %s", 
             (char *) gtk_object_get_user_data( GTK_OBJECT( pw ) ),
             aszSkillCmd[ st ] );
    UserCommand( sz );

    GTKUpdateAnnotations();
}

static GtkWidget *SkillMenu( skilltype stSelect, char *szAnno ) {

    GtkWidget *pwMenu, *pwOptionMenu, *pwItem;
    skilltype st;
    
    pwMenu = gtk_menu_new();
    for( st = SKILL_VERYBAD; st <= SKILL_VERYGOOD; st++ ) {
	gtk_menu_append( GTK_MENU( pwMenu ),
			 pwItem = gtk_menu_item_new_with_label(
			     aszSkillType[ st ] ? aszSkillType[ st ] : "" ) );
        gtk_object_set_user_data( GTK_OBJECT( pwItem ), szAnno );
	gtk_signal_connect( GTK_OBJECT( pwItem ), "activate",
			    GTK_SIGNAL_FUNC( SkillMenuActivate ),
			    GINT_TO_POINTER( st ) );
    }
    gtk_widget_show_all( pwMenu );
    
    pwOptionMenu = gtk_option_menu_new();
    gtk_option_menu_set_menu( GTK_OPTION_MENU( pwOptionMenu ), pwMenu );
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

  pwLabel = gtk_label_new ( "Equity before resignation: " );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( "Equity after resignation: " );
  gtk_misc_set_alignment( GTK_MISC( pwLabel ), 0, 0.5 );
  gtk_table_attach ( GTK_TABLE ( pwTable ),
                     pwLabel,
                     0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 
                     8, 2 );

  pwLabel = gtk_label_new ( "Difference: " );
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


/*
 * Make cube analysis widget 
 *
 * Input:
 *   aarOutput, aarStdDev: evaluations
 *   pes: evaluation setup
 *
 * Returns:
 *   nice and pretty widget with cube analysis
 *
 */

static GtkWidget *CubeAnalysis( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                                float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
                                const evalsetup *pes,
                                const int fDouble ) {

    cubeinfo ci;

    GtkWidget *pw;
    GtkWidget *pwTable;
    GtkWidget *pwFrame;

    int iRow;
    int i;
    cubedecision cd;
    float r;

    int ai[ 3 ];
    const char *aszCube[] = {
      NULL, "No double", "Double, take", "Double, pass" };

    float arDouble[ 4 ];
    gchar *sz;


    if( pes->et == EVAL_NONE )
	return NULL;

    GetMatchStateCubeInfo( &ci, &ms );

    cd = FindCubeDecision ( arDouble, aarOutput, &ci );
    
    if( !GetDPEq( NULL, NULL, &ci ) && ! fDouble )
	/* No cube action possible */
	return NULL;

    /* header */

    pwFrame = gtk_frame_new ( "Cube analysis" );
    gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );

    pwTable = gtk_table_new ( 8, 4, FALSE );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwTable );

    /* if EVAL_EVAL include cubeless equity and winning percentages */

    iRow = 0;

    if ( pes->et == EVAL_EVAL ) {

      if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
        sz = g_strdup_printf ( "Cubeless %d-ply equity: %+7.3f", 
                               pes->ec.nPlies, 
                               Utility ( aarOutput[ 0 ], &ci ) );
      else
        sz = g_strdup_printf ( "Cubeless %d-ply MWC: %7.3f%%", 
                               pes->ec.nPlies,
                               100.0f * eq2mwc ( Utility ( aarOutput[ 0 ], 
                                                           &ci ), &ci ) );

      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 4, iRow, iRow + 1,
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      iRow++;


      sz = g_strdup_printf ( "%6.2f%% %6.2f%% %6.2f%% "
                             "%6.2f%% %6.2f%% %6.2f%%",
                             100.0f * aarOutput[ 0 ][ OUTPUT_WINBACKGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_WINGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_WIN ],
                             100.0f * ( 1.0 - aarOutput[ 0 ][ OUTPUT_WIN ] ),
                             100.0f * aarOutput[ 0 ][ OUTPUT_LOSEGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_LOSEBACKGAMMON ] );
      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
      g_free ( sz );
      
      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 4, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 4 );

      iRow++;

    }

    getCubeDecisionOrdering ( ai, arDouble, &ci );

    for ( i = 0; i < 3; i++ ) {

      /* numbering */

      sz = g_strdup_printf ( "%d.", i + 1 );
      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 1, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* label */

      pw = gtk_label_new ( aszCube[ ai[ i ] ] );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         1, 2, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* equity */

      if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
        sz = g_strdup_printf ( "%+7.3f", arDouble[ ai [ i ] ] );
      else
        sz = g_strdup_printf ( "%+7.3f%%", 
                               100.0f * eq2mwc( arDouble[ ai[ i ] ], &ci ) );

      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         2, 3, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* difference */

      if ( i ) {
        
        if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
          sz = g_strdup_printf ( "(%+7.3f)", 
                                 arDouble[ ai [ i ] ] - 
                                 arDouble[ OUTPUT_OPTIMAL ] );
        else
          sz = g_strdup_printf ( "(%+7.3f%%)", 
                                 100.0f * eq2mwc( arDouble[ ai[ i ] ], &ci ) -
                                 100.0f * eq2mwc( arDouble[ OUTPUT_OPTIMAL ], 
                                                  &ci ) );

        pw = gtk_label_new ( sz );
        gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
        g_free ( sz );
        
        gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                           3, 4, iRow, iRow + 1, 
                           GTK_EXPAND | GTK_FILL, 
                           GTK_EXPAND | GTK_FILL, 
                           8, 0 );

      }

      /* rollout details */
      
      if ( pes->et == EVAL_ROLLOUT && 
           ( ai[ i ] == OUTPUT_TAKE || ai[ i ] == OUTPUT_NODOUBLE ) ) {

        /* FIXME: output cubeless euqities and percentages for rollout */
        /*        probably along with rollout details */
        
      }
        
      iRow++;

    }

    /* proper cube action */

    pw = gtk_label_new ( "Proper cube action: " );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
        
    gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                       0, 2, iRow, iRow + 1, 
                       GTK_EXPAND | GTK_FILL, 
                       GTK_EXPAND | GTK_FILL, 
                       8, 8 );

    pw = gtk_label_new ( GetCubeRecommendation ( cd ) );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
        
    gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                       2, 3, iRow, iRow + 1, 
                       GTK_EXPAND | GTK_FILL, 
                       GTK_EXPAND | GTK_FILL, 
                       8, 8 );

    /* percent */

    if ( ( r = getPercent ( cd, arDouble ) ) >= 0.0 ) {

      sz = g_strdup_printf ( "(%.1f%%)", 100.0 * r );
      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         3, 4, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 8 );

    }

    return pwFrame;
}


static GtkWidget *TakeAnalysis( const movetype mt, 
                                float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                                float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
				const evalsetup *pes ) {

    cubeinfo ci;

    GtkWidget *pw;
    GtkWidget *pwTable;
    GtkWidget *pwFrame;

    int iRow;
    int i;
    cubedecision cd;

    int ai[ 2 ];
    const char *aszCube[] = {
      NULL, NULL, "Take", "Pass" };

    float arDouble[ 4 ];
    gchar *sz;


    if( pes->et == EVAL_NONE )
	return NULL;

    GetMatchStateCubeInfo( &ci, &ms );

    cd = FindCubeDecision ( arDouble, aarOutput, &ci );
    
    /* header */

    pwFrame = gtk_frame_new ( "Take analysis" );
    gtk_container_set_border_width ( GTK_CONTAINER ( pwFrame ), 8 );

    pwTable = gtk_table_new ( 5, 4, FALSE );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pwTable );

    /* if EVAL_EVAL include cubeless equity and winning percentages */

    iRow = 0;

    switch ( pes->et ) {

    case EVAL_EVAL:

      if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
        sz = g_strdup_printf ( "Cubeless %d-ply equity: %+7.3f", 
                               pes->ec.nPlies, 
                               Utility ( aarOutput[ 0 ], &ci ) );
      else
        sz = g_strdup_printf ( "Cubeless %d-ply MWC: %7.3f%%", 
                               pes->ec.nPlies,
                               100.0f * eq2mwc ( Utility ( aarOutput[ 0 ], 
                                                           &ci ), &ci ) );

      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 4, iRow, iRow + 1,
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      iRow++;


      sz = g_strdup_printf ( "%6.2f%% %6.2f%% %6.2f%% "
                             "%6.2f%% %6.2f%% %6.2f%%",
                             100.0f * aarOutput[ 0 ][ OUTPUT_WINBACKGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_WINGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_WIN ],
                             100.0f * ( 1.0 - aarOutput[ 0 ][ OUTPUT_WIN ] ),
                             100.0f * aarOutput[ 0 ][ OUTPUT_LOSEGAMMON ],
                             100.0f * aarOutput[ 0 ][ OUTPUT_LOSEBACKGAMMON ] );
      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
      g_free ( sz );
      
      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 4, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 4 );

      iRow++;

      break;

    case EVAL_ROLLOUT:

      /* FIXME: */

      break;

    default:

      assert ( FALSE );
      break;

    }

    if ( arDouble[ OUTPUT_TAKE ] < arDouble[ OUTPUT_DROP ] ) {
      ai[ 0 ] = OUTPUT_TAKE;
      ai[ 1 ] = OUTPUT_DROP;
    }
    else {
      ai[ 0 ] = OUTPUT_DROP;
      ai[ 1 ] = OUTPUT_TAKE;
    }
      

    for ( i = 0; i < 2; i++ ) {

      /* numbering */

      sz = g_strdup_printf ( "%d.", i + 1 );
      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         0, 1, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* label */

      pw = gtk_label_new ( aszCube[ ai[ i ] ] );
      gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         1, 2, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* equity */

      if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
        sz = g_strdup_printf ( "%+7.3f", -arDouble[ ai [ i ] ] );
      else
        sz = g_strdup_printf ( "%+7.3f%%", 
                               100.0f * eq2mwc( -arDouble[ ai[ i ] ], &ci ) );

      pw = gtk_label_new ( sz );
      gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
      g_free ( sz );

      gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                         2, 3, iRow, iRow + 1, 
                         GTK_EXPAND | GTK_FILL, 
                         GTK_EXPAND | GTK_FILL, 
                         8, 0 );

      /* difference */

      if ( i ) {
        
        if ( ! ci.nMatchTo || ( ci.nMatchTo && ! fOutputMWC ) )
          sz = g_strdup_printf ( "(%+7.3f)", 
                                 arDouble[ ai [ i ] ] - 
                                 arDouble[ OUTPUT_OPTIMAL ] );
        else
          sz = g_strdup_printf ( "(%+7.3f%%)", 
                                 100.0f * eq2mwc( -arDouble[ ai[ i ] ], &ci ) -
                                 100.0f * eq2mwc( -arDouble[ OUTPUT_OPTIMAL ], 
                                                  &ci ) );

        pw = gtk_label_new ( sz );
        gtk_misc_set_alignment( GTK_MISC( pw ), 1, 0.5 );
        g_free ( sz );
        
        gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                           3, 4, iRow, iRow + 1, 
                           GTK_EXPAND | GTK_FILL, 
                           GTK_EXPAND | GTK_FILL, 
                           8, 0 );

      }

      iRow++;

    }

    /* proper cube action */

    pw = gtk_label_new ( "Correct response: " );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
        
    gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                       0, 2, iRow, iRow + 1, 
                       GTK_EXPAND | GTK_FILL, 
                       GTK_EXPAND | GTK_FILL, 
                       8, 8 );

    switch ( cd ) {

    case DOUBLE_TAKE:
    case NODOUBLE_TAKE:
    case TOOGOOD_TAKE:
    case REDOUBLE_TAKE:
    case NO_REDOUBLE_TAKE:
    case TOOGOODRE_TAKE:
    case NODOUBLE_DEADCUBE:
    case NO_REDOUBLE_DEADCUBE:
      pw = gtk_label_new ( "Take" );
      break;

    case DOUBLE_PASS:
    case TOOGOOD_PASS:
    case REDOUBLE_PASS:
    case TOOGOODRE_PASS:
      pw = gtk_label_new ( "Pass" );
      break;

    case DOUBLE_BEAVER:
    case NODOUBLE_BEAVER:
    case NO_REDOUBLE_BEAVER:
      pw = gtk_label_new ( "Beaver!" );
      break;

    case NOT_AVAILABLE:
      pw = gtk_label_new ( "Eat it!" );
      break;

    }

    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
        
    gtk_table_attach ( GTK_TABLE ( pwTable ), pw,
                       2, 4, iRow, iRow + 1, 
                       GTK_EXPAND | GTK_FILL, 
                       GTK_EXPAND | GTK_FILL, 
                       8, 8 );

    return pwFrame;

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
    
    pch = sz + sprintf( sz, "Rolled %d%d", n0, n1 );
    
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
			     aszLuckType[ lt ] ? aszLuckType[ lt ] : "" ) );
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


/*
 * temporary hack for files sgf files 
 */

static void
fixOutput ( float arDouble[], float aarOutput[][ NUM_ROLLOUT_OUTPUTS ] ) {
  
  cubeinfo ci;

  GetMatchStateCubeInfo( &ci, &ms );

  if ( aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] < -10000.0 ) {
    if ( ci.nMatchTo )
      aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] = 
        eq2mwc ( arDouble[ OUTPUT_NODOUBLE ], &ci );
    else
      aarOutput[ 0 ][ OUTPUT_CUBEFUL_EQUITY ] = arDouble[ OUTPUT_NODOUBLE ];
  }


  if ( aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] < -10000.0 ) {
    if ( ci.nMatchTo )
      aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] = 
        eq2mwc ( arDouble[ OUTPUT_TAKE ], &ci );
    else
      aarOutput[ 1 ][ OUTPUT_CUBEFUL_EQUITY ] = arDouble[ OUTPUT_TAKE ];
  }

}



static void SetAnnotation( moverecord *pmr ) {

    GtkWidget *pwParent = pwAnalysis->parent, *pw, *pwBox, *pwAlign;
    int fMoveOld, fTurnOld;
    static hintdata hd = { NULL, NULL, NULL, NULL, FALSE };
    list *pl;
    char sz[ 64 ];
    
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
	    pwBox = gtk_hbox_new( FALSE, 0 );
	    
	    ms.fMove = ms.fTurn = pmr->n.fPlayer;

            fixOutput ( pmr->n.arDouble, pmr->n.aarOutput );
	    
	    if( ( pw = CubeAnalysis( pmr->n.aarOutput, pmr->n.aarStdDev,
				     &pmr->n.esDouble, FALSE ) ) ) {
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE, FALSE,
				    4 );
                /* FIXME: add cube skill */
            }

	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwBox, FALSE, FALSE,
				0 );
	    
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				RollAnalysis( pmr->n.anRoll[ 0 ],
					      pmr->n.anRoll[ 1 ],
					      pmr->n.rLuck, pmr->n.lt ),
				FALSE, FALSE, 4 );

	    gtk_box_pack_end( GTK_BOX( pwBox ), 
                              SkillMenu( pmr->n.stMove, "move" ),
			      FALSE, FALSE, 4 );
	    strcpy( sz, "Moved " );
	    FormatMove( sz + 6, ms.anBoard, pmr->n.anMove );
	    gtk_box_pack_end( GTK_BOX( pwBox ),
			      gtk_label_new( sz ), FALSE, FALSE, 0 );

            /* Skill for cube */

            pwBox = gtk_hbox_new ( FALSE, 0 );

            gtk_box_pack_start ( GTK_BOX ( pwAnalysis ), pwBox, FALSE, FALSE,
                                 0 );

            gtk_box_pack_start ( GTK_BOX ( pwBox ),
                                 gtk_label_new ( "Didn't double" ),
                                 FALSE, FALSE, 4 );
            gtk_box_pack_start ( GTK_BOX ( pwBox ),
                                 SkillMenu ( pmr->n.stCube, "cube" ),
                                 FALSE, FALSE, 4 );

            /* move */
			      
	    if( pmr->n.ml.cMoves ) {
		hd.pml = &pmr->n.ml;
		hd.pw = pw = gtk_scrolled_window_new( NULL, NULL );
		gtk_scrolled_window_set_policy(
		    GTK_SCROLLED_WINDOW( pw ),
		    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
		gtk_container_add( GTK_CONTAINER( pw ),
				   CreateMoveList( &hd, pmr->n.iMove ) );
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, TRUE, TRUE,
				    0 );
	    }

	    if( !g_list_first( GTK_BOX( pwAnalysis )->children ) ) {
		gtk_widget_destroy( pwAnalysis );
		pwAnalysis = NULL;
	    }
		
	    ms.fMove = fMoveOld;
	    ms.fTurn = fTurnOld;
	    break;

	case MOVE_DOUBLE:
	    pwAnalysis = gtk_vbox_new( FALSE, 0 );
	    
            fixOutput ( pmr->d.arDouble, pmr->d.aarOutput );

	    if( ( pw = CubeAnalysis( pmr->d.aarOutput, pmr->d.aarStdDev,
				     &pmr->d.esDouble, TRUE ) ) )
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE,
				    FALSE, 0 );

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( "Double" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->d.st, "double" ),
				FALSE, FALSE, 2 );

	    pwAlign = gtk_alignment_new( 0.5f, 0.5f, 0.0f, 0.0f );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwAlign, FALSE, FALSE,
				0 );

	    gtk_container_add( GTK_CONTAINER( pwAlign ), pwBox );
	    
	    break;

	case MOVE_TAKE:
	case MOVE_DROP:
	    pwAnalysis = gtk_vbox_new( FALSE, 0 );

            fixOutput ( pmr->d.arDouble, pmr->d.aarOutput );

	    if( ( pw = TakeAnalysis( pmr->mt, pmr->d.aarOutput,
                                     pmr->d.aarStdDev,
				     &pmr->d.esDouble ) ) )
		gtk_box_pack_start( GTK_BOX( pwAnalysis ), pw, FALSE,
				    FALSE, 0 );

	    pwBox = gtk_hbox_new( FALSE, 0 );
	    gtk_box_pack_start( GTK_BOX( pwBox ),
				gtk_label_new( pmr->mt == MOVE_TAKE ? "Take" :
				    "Drop" ),
				FALSE, FALSE, 2 );
	    gtk_box_pack_start( GTK_BOX( pwBox ), 
                                SkillMenu( pmr->d.st, 
                                           ( pmr->d.mt == MOVE_TAKE ) ?
                                           "take" : "drop" ),
				FALSE, FALSE, 2 );

	    pwAlign = gtk_alignment_new( 0.5f, 0.5f, 0.0f, 0.0f );
	    gtk_box_pack_start( GTK_BOX( pwAnalysis ), pwAlign, FALSE, FALSE,
				0 );

	    gtk_container_add( GTK_CONTAINER( pwAlign ), pwBox );
	    
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
				gtk_label_new( "Resign" ),
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
				gtk_label_new( "Accept" ),
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
	pwAnalysis = gtk_label_new( "No analysis available." );
    
    gtk_paned_pack1( GTK_PANED( pwParent ), pwAnalysis, TRUE, FALSE );
    gtk_widget_show_all( pwAnalysis );
}

/* Select a moverecord as the "current" one.  NOTE: This function must be
   called _after_ applying the moverecord. */
extern void GTKSetMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr;
    int i;

    SetAnnotation( pmr );

    if( pwHint ) {
	hintdata *phd = gtk_object_get_user_data( GTK_OBJECT( pwHint ) );

	phd->fButtonsValid = FALSE;
	CheckHintButtons();
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
    
    sprintf( sz, "Game %d: %s %d, %s %d", pmr->g.i + 1, ap[ 0 ].szName,
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
	ApplyMoveRecord( &ms, pl->p );
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

  getWindowGeometry ( &wgMain, pw );
    
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
    else
	gtk_window_set_default_size( GTK_WINDOW( pw ),
				     MAX( 480, preq->width ),
				     MIN( preq->height + 79 * 3,
					  gdk_screen_height() - 20 ) );
}

extern int InitGTK( int *argc, char ***argv ) {
    
    GtkWidget *pwVbox, *pwHbox;
    static GtkItemFactoryEntry aife[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New/_Game", "<control>N", Command, CMD_NEW_GAME, NULL },
	{ "/_File/_New/_Match...", NULL, NewMatch, 0, NULL },
	{ "/_File/_New/_Session", NULL, Command, CMD_NEW_SESSION, NULL },
	{ "/_File/_New/_Weights...", NULL, NewWeights, 0, NULL },
	{ "/_File/_Open", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Open/_Commands...", NULL, LoadCommands, 0, NULL },
	{ "/_File/_Open/_Game...", NULL, LoadGame, 0, NULL },
	{ "/_File/_Open/_Match or session...", NULL, LoadMatch, 0, NULL },
	{ "/_File/_Save", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Save/_Game...", NULL, SaveGame, 0, NULL },
	{ "/_File/_Save/_Match or session...", NULL, SaveMatch, 0, NULL },
	{ "/_File/_Save/_Weights...", NULL, SaveWeights, 0, NULL },
	{ "/_File/-", NULL, NULL, 0, "<Separator>" },	
	{ "/_File/_Import", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Import/._mat match...", NULL, ImportMat, 0, NULL },
	{ "/_File/_Import/._pos position...", NULL, ImportPos, 0, NULL },
	{ "/_File/_Import/FIBS _oldmoves...", NULL, ImportOldmoves, 0, NULL },
	{ "/_File/_Import/._sgg match...", NULL, ImportSGG, 0, NULL },
	{ "/_File/_Export", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Export/_Game", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Export/_Game/.gam...", NULL, ExportGameGam, 0, NULL },
	{ "/_File/_Export/_Game/HTML...", NULL, ExportGameHtml, 0, NULL },
	{ "/_File/_Export/_Game/LaTeX...", NULL, ExportGameLaTeX, 0, NULL },
	{ "/_File/_Export/_Game/PDF...", NULL, ExportGamePDF, 0, NULL },
	{ "/_File/_Export/_Game/PostScript...", NULL, ExportGamePostScript, 0,
	  NULL },
	{ "/_File/_Export/_Match", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Export/_Match/HTML...", NULL, ExportMatchHtml, 0, NULL },
	{ "/_File/_Export/_Match/LaTeX...", NULL, ExportMatchLaTeX, 0, NULL },
	{ "/_File/_Export/_Match/.mat...", NULL, ExportMatchMat, 0, NULL },
	{ "/_File/_Export/_Match/PDF...", NULL, ExportMatchPDF, 0, NULL },
	{ "/_File/_Export/_Match/PostScript...", NULL, ExportMatchPostScript,
	  0, NULL },
	{ "/_File/_Export/_Position", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Export/_Position/HTML...", NULL,
	  ExportPositionHtml, 0, NULL },
	{ "/_File/_Export/_Position/Encapsulated PostScript...", NULL,
	  ExportPositionEPS, 0, NULL },
	{ "/_File/_Export/_Position/.pos...", NULL, ExportPositionPos, 0,
	  NULL },
	{ "/_File/_Export/_Session", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Export/_Session/HTML...", NULL, ExportSessionHtml, 0,
	  NULL },
	{ "/_File/_Export/_Session/LaTeX...", NULL, ExportSessionLaTeX, 0,
	  NULL },
	{ "/_File/_Export/_Session/PDF...", NULL, ExportSessionPDF, 0, NULL },
	{ "/_File/_Export/_Session/PostScript...", NULL,
	  ExportSessionPostScript, 0, NULL },
	{ "/_File/-", NULL, NULL, 0, "<Separator>" },
	{ "/_File/_Quit", "<control>Q", Command, CMD_QUIT, NULL },
	{ "/_Edit", NULL, NULL, 0, "<Branch>" },
	{ "/_Edit/_Undo", "<control>Z", ShowBoard, 0, NULL },
	{ "/_Edit/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Edit/_Copy", "<control>C", NULL, 0, NULL },
	{ "/_Edit/_Paste", "<control>V", NULL, 0, NULL },
	{ "/_Edit/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Edit/_Enter command...", NULL, EnterCommand, 0, NULL },
	{ "/_Game", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/_Roll", "<control>R", Command, CMD_ROLL, NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/_Double", "<control>D", Command, CMD_DOUBLE, NULL },
	{ "/_Game/_Take", "<control>T", Command, CMD_TAKE, NULL },
	{ "/_Game/Dro_p", "<control>P", Command, CMD_DROP, NULL },
	{ "/_Game/R_edouble", NULL, Command, CMD_REDOUBLE, NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/Re_sign", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/Re_sign/_Normal", NULL, Command, CMD_RESIGN_N, NULL },
	{ "/_Game/Re_sign/_Gammon", NULL, Command, CMD_RESIGN_G, NULL },
	{ "/_Game/Re_sign/_Backgammon", NULL, Command, CMD_RESIGN_B, NULL },
	{ "/_Game/_Agree to resignation", NULL, Command, CMD_AGREE, NULL },
	{ "/_Game/De_cline resignation", NULL, Command, CMD_DECLINE, NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/Play computer turn", NULL, Command, CMD_PLAY, NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/Swap players", NULL, Command, CMD_SWAP_PLAYERS, NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/Next roll", "Page_Down", Command, CMD_NEXT_ROLL, NULL },
	{ "/_Game/Previous roll", "Page_Up", Command, CMD_PREV_ROLL, NULL },
	{ "/_Game/Next move", "<shift>Page_Down", Command, CMD_NEXT, NULL },
	{ "/_Game/Previous move", "<shift>Page_Up", Command, CMD_PREV, NULL },
	{ "/_Game/Next game", "<control>Page_Down", Command, CMD_NEXT_GAME,
	  NULL },
	{ "/_Game/Previous game", "<control>Page_Up", Command, CMD_PREV_GAME,
	  NULL },
	{ "/_Game/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Game/Set cube...", NULL, GTKSetCube, 0, NULL },
	{ "/_Game/Set _dice...", NULL, SetDice, 0, NULL },
	{ "/_Game/Set _turn", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/Set turn/0", NULL, Command, CMD_SET_TURN_0, "<RadioItem>" },
	{ "/_Game/Set turn/1", NULL, Command, CMD_SET_TURN_1,
	  "/Game/Set turn/0" },
	{ "/_Analyse", NULL, NULL, 0, "<Branch>" },
	{ "/_Analyse/_Evaluate", "<control>E", Command, CMD_EVAL, NULL },
	{ "/_Analyse/_Hint", "<control>H", Command, CMD_HINT, NULL },
	{ "/_Analyse/_Rollout", NULL, Command, CMD_ROLLOUT, NULL },
	{ "/_Analyse/Rollout _cube decision", NULL, Command, CMD_ROLLOUT_CUBE,
	  NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/Analyse move", NULL, Command, CMD_ANALYSE_MOVE, NULL },
	{ "/_Analyse/Analyse game", NULL, Command, CMD_ANALYSE_GAME, NULL },
	{ "/_Analyse/Analyse match", NULL, Command, CMD_ANALYSE_MATCH, NULL },
	{ "/_Analyse/Analyse session", NULL, Command, CMD_ANALYSE_SESSION,
	  NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/Game statistics", NULL, Command,
          CMD_SHOW_STATISTICS_GAME, NULL },
	{ "/_Analyse/Match statistics", NULL, Command,
          CMD_SHOW_STATISTICS_MATCH, NULL },
	{ "/_Analyse/Session statistics", NULL, Command,
          CMD_SHOW_STATISTICS_SESSION, NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/_Pip count", NULL, Command, CMD_SHOW_PIPCOUNT, NULL },
	{ "/_Analyse/_Kleinman count", NULL, Command, CMD_SHOW_KLEINMAN,
	  NULL },
	{ "/_Analyse/_Thorp count", NULL, Command, CMD_SHOW_THORP, NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/_Gammon values", NULL, Command, CMD_SHOW_GAMMONVALUES,
	  NULL },
	{ "/_Analyse/_Market window", NULL, Command, CMD_SHOW_MARKETWINDOW,
	  NULL },
	{ "/_Analyse/M_atch equity table", NULL, Command,
	  CMD_SHOW_MATCHEQUITYTABLE, NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/Evaluation engine", NULL, Command,
	  CMD_SHOW_ENGINE, NULL },
	{ "/_Train", NULL, NULL, 0, "<Branch>" },
	{ "/_Train/D_ump database", NULL, Command, CMD_DATABASE_DUMP, NULL },
	{ "/_Train/_Generate database", NULL, Command, CMD_DATABASE_GENERATE,
	  NULL },
	{ "/_Train/_Rollout database", NULL, Command, CMD_DATABASE_ROLLOUT,
	  NULL },
	{ "/_Train/_Import database...", NULL, DatabaseImport, 0, NULL },
	{ "/_Train/_Export database...", NULL, DatabaseExport, 0, NULL },
	{ "/_Train/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Train/Train from _database", NULL, Command, CMD_TRAIN_DATABASE,
	  NULL },
	{ "/_Train/Train with _TD(0)", NULL, Command, CMD_TRAIN_TD, NULL },
	{ "/_Settings", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/Analysis", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/Analysis/_Chequer play", NULL, Command,
	  CMD_SET_ANALYSIS_MOVES, "<CheckItem>" },
	{ "/_Settings/Analysis/_Cube action", NULL, Command,
	  CMD_SET_ANALYSIS_CUBE, "<CheckItem>" },
	{ "/_Settings/Analysis/_Dice rolls", NULL, Command,
	  CMD_SET_ANALYSIS_LUCK, "<CheckItem>" },
	{ "/_Settings/Analysis/Move limit...", NULL, SetAnalysis, 0, NULL },
	{ "/_Settings/Analysis/Evaluation", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/Analysis/Evaluation/Chequer play...", NULL, 
          SetAnalysisEvalChequer, 0, NULL },
	{ "/_Settings/Analysis/Evaluation/Cube decisions...", NULL, 
          SetAnalysisEvalCube, 0, NULL },
	{ "/_Settings/Appearance...", NULL, Command, CMD_SET_APPEARANCE,
	  NULL },
	{ "/_Settings/_Automatic", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Automatic/_Analysis", NULL, Command,
	  CMD_SET_AUTO_ANALYSIS, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Bearoff", NULL, Command,
	  CMD_SET_AUTO_BEAROFF, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Crawford", NULL, Command,
	  CMD_SET_AUTO_CRAWFORD, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Doubles...", NULL, SetAutoDoubles, 0, NULL },
	{ "/_Settings/_Automatic/_Game", NULL, Command, CMD_SET_AUTO_GAME,
	  "<CheckItem>" },
	{ "/_Settings/_Automatic/_Move", NULL, Command, CMD_SET_AUTO_MOVE,
	  "<CheckItem>" },
	{ "/_Settings/_Automatic/_Roll", NULL, Command, CMD_SET_AUTO_ROLL,
	  "<CheckItem>" },
	{ "/_Settings/_Beavers...", NULL, SetBeavers, 0, NULL },
	{ "/_Settings/Cache...", NULL, SetCache, 0, NULL },
	{ "/_Settings/Clockwise movement", NULL, Command, CMD_SET_CLOCKWISE,
	  "<CheckItem>" },
	{ "/_Settings/_Confirmation", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Confirmation/_Starting new game", NULL, Command, 
          CMD_SET_CONFIRM_NEW, "<CheckItem>" },
	{ "/_Settings/_Confirmation/_Overwriting existing files", NULL, 
          Command, CMD_SET_CONFIRM_SAVE, "<CheckItem>" },
	{ "/_Settings/De_lay...", NULL, SetDelay, 0, NULL },
	{ "/_Settings/_Dice generation", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Dice generation/_ANSI", NULL, Command, CMD_SET_RNG_ANSI,
	  "<RadioItem>" },
	{ "/_Settings/_Dice generation/_BSD", NULL, Command, CMD_SET_RNG_BSD,
	  "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/_ISAAC", NULL, Command,
	  CMD_SET_RNG_ISAAC, "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/Ma_nual", NULL, Command,
	  CMD_SET_RNG_MANUAL, "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/_MD5", NULL, Command, CMD_SET_RNG_MD5,
	  "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/Mersenne _Twister", NULL, Command,
	  CMD_SET_RNG_MERSENNE, "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/_User", NULL, Command, CMD_SET_RNG_USER,
	  "/Settings/Dice generation/ANSI" },
	{ "/_Settings/_Dice generation/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Settings/_Dice generation/_Seed...", NULL, SetSeed, 0, NULL },
	{ "/_Settings/Di_splay", NULL, Command, CMD_SET_DISPLAY,
	  "<CheckItem>" },
	{ "/_Settings/Doubling cube", NULL, Command, CMD_SET_CUBE_USE,
	  "<CheckItem>" },
	{ "/_Settings/_Evaluation", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Evaluation/Chequer play...", NULL, SetEvalChequer, 0,
	  NULL },
	{ "/_Settings/_Evaluation/Cube decisions...", NULL, SetEvalCube, 0,
	  NULL },
        { "/_Settings/_Egyptian rule", NULL, Command, CMD_SET_EGYPTIAN,
          "<CheckItem>" },
	{ "/_Settings/_Invert/_Match equity table", NULL, Command, 
          CMD_SET_INVERT_MET,
	  "<CheckItem>" },
	{ "/_Settings/_Jacoby rule", NULL, Command, CMD_SET_JACOBY,
	  "<CheckItem>" },
	{ "/_Settings/_Match equity table...", NULL, SetMET, 0, NULL },
	{ "/_Settings/_Nackgammon", NULL, Command, CMD_SET_NACKGAMMON,
	  "<CheckItem>" },
	{ "/_Settings/_Output", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Output/_Match equity as MWC", NULL, Command,
	  CMD_SET_OUTPUT_MWC, "<CheckItem>" },
	{ "/_Settings/_Output/_GWC as percentage", NULL, Command,
	  CMD_SET_OUTPUT_WINPC, "<CheckItem>" },
	{ "/_Settings/_Output/_MWC as percentage", NULL, Command,
	  CMD_SET_OUTPUT_MATCHPC, "<CheckItem>" },
	{ "/_Settings/Paths...", NULL, Command, CMD_SHOW_PATH, NULL },
	{ "/_Settings/_Players...", NULL, SetPlayers, 0, NULL },
	{ "/_Settings/Prompt...", NULL, NULL, 0, NULL },
	{ "/_Settings/Record all games", NULL, Command, CMD_SET_RECORD,
	  "<CheckItem>" },
	{ "/_Settings/_Rollouts...", NULL, SetRollouts, 0, NULL },
	{ "/_Settings/_Training", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Training/_Learning rate...", NULL, SetAlpha, 0, NULL },
	{ "/_Settings/_Training/_Annealing rate...", NULL, SetAnneal, 0,
	  NULL },
	{ "/_Settings/_Training/_Threshold...", NULL, SetThreshold, 0, NULL },
	{ "/_Settings/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Settings/Save settings", NULL, Command, CMD_SAVE_SETTINGS, NULL },
	{ "/_Windows", NULL, NULL, 0, "<Branch>" },
	{ "/_Windows/_Game record", NULL, Command, CMD_LIST_GAME, NULL },
	{ "/_Windows/_Annotation", NULL, Command, CMD_SET_ANNOTATION_ON,
	  NULL },
	{ "/_Windows/Gu_ile", NULL, NULL, 0, NULL },
	{ "/_Help", NULL, NULL, 0, "<Branch>" },
	{ "/_Help/_Commands", NULL, Command, CMD_HELP, NULL },
	{ "/_Help/gnubg _Manual", NULL, ShowManual, 0, NULL },
	{ "/_Help/Co_pying gnubg", NULL, Command, CMD_SHOW_COPYING, NULL },
	{ "/_Help/gnubg _Warranty", NULL, Command, CMD_SHOW_WARRANTY,
	  NULL },
	{ "/_Help/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Help/_About gnubg", NULL, Command, CMD_SHOW_VERSION, NULL }
    };
#if __GNUC__
    char sz[ strlen( szHomeDirectory ) + 15 ];
#elif HAVE_ALLOCA
    char *sz = alloca( strlen( szHomeDirectory ) + 15 );
#else
    char sz[ 4096 ];
#endif

    sprintf( sz, "%s/.gnubg.gtkrc", szHomeDirectory );
    if( !access( sz, R_OK ) )
	gtk_rc_add_default_file( sz );
    else if( !access( PKGDATADIR "/gnubg.gtkrc", R_OK ) )
	gtk_rc_add_default_file( PKGDATADIR "/gnubg.gtkrc" );
    else
	gtk_rc_add_default_file( "gnubg.gtkrc" );
    
    if( !gtk_init_check( argc, argv ) )
	return FALSE;
    
    fnAction = HandleXAction;

    gdk_rgb_init();
    gdk_rgb_set_min_colors( 2 * 2 * 2 );
    gtk_widget_set_default_colormap( gdk_rgb_get_cmap() );
    gtk_widget_set_default_visual( gdk_rgb_get_visual() );

    pwMain = gtk_window_new( GTK_WINDOW_TOPLEVEL );

    setWindowGeometry ( pwMain, &wgMain );

    gtk_window_set_title( GTK_WINDOW( pwMain ), "GNU Backgammon" );
    /* FIXME add an icon */
    gtk_container_add( GTK_CONTAINER( pwMain ),
		       pwVbox = gtk_vbox_new( FALSE, 0 ) );

    pagMain = gtk_accel_group_new();
    pif = gtk_item_factory_new( GTK_TYPE_MENU_BAR, "<main>", pagMain );
    gtk_item_factory_create_items( pif, sizeof( aife ) / sizeof( aife[ 0 ] ),
				   aife, NULL );
    gtk_window_add_accel_group( GTK_WINDOW( pwMain ), pagMain );
    gtk_box_pack_start( GTK_BOX( pwVbox ),
			pwMenuBar = gtk_item_factory_get_widget( pif,
								 "<main>" ),
			FALSE, FALSE, 0 );
    sprintf( sz, "%s/" GNUBGMENURC, szHomeDirectory );
#if GTK_CHECK_VERSION(1,3,15)
    gtk_accel_map_load( sz );
#else
    gtk_item_factory_parse_rc( sz );
#endif
    
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

    gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	pif, "/Windows/Guile" ), FALSE );

    gtk_container_add( GTK_CONTAINER( pwVbox ), pwBoard = board_new() );
    pwGrab = ( (BoardData *) BOARD( pwBoard )->board_data )->stop;

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
    
    gtk_signal_connect( GTK_OBJECT( pwMain ), "size-request",
			GTK_SIGNAL_FUNC( MainSize ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    CreateGameWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwGame ), pagMain );

    CreateAnnotationWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwAnnotation ), pagMain );
    
    ListCreate( &lOutput );

    return TRUE;
}

extern void RunGTK( void ) {

    togglecommand *ptc;
    
    /* Ensure all menu item settings are initialised to the correct state. */
    for( ptc = atc; ptc->p; ptc++ )
	GTKSet( ptc->p );
    GTKSet( &rngCurrent );
    GTKSet( &ms.fCubeOwner );
    GTKSet( &ms.nCube );
    GTKSet( ap );
    GTKSet( &ms.fTurn );
    GTKSet( &ms.gs );
    
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
    
    gtk_main();
}

static void DestroyList( gpointer p ) {

    *( (void **) p ) = NULL;
}

extern void ShowList( char *psz[], char *szTitle ) {

    static GtkWidget *pwDialog;
    GtkWidget *pwList = gtk_list_new(),
	*pwScroll = gtk_scrolled_window_new( NULL, NULL ),
	*pwOK = gtk_button_new_with_label( "OK" );

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

extern GtkWidget *CreateDialog( char *szTitle, int fQuestion, GtkSignalFunc pf,
				void *p ) {
#include "gnu.xpm"
#include "question.xpm"

    GdkPixmap *ppm;
    GtkWidget *pwDialog = gtk_dialog_new(),
	*pwOK = gtk_button_new_with_label( "OK" ),
	*pwCancel = gtk_button_new_with_label( "Cancel" ),
	*pwHbox = gtk_hbox_new( FALSE, 0 ),
	*pwButtons = gtk_hbutton_box_new(),
	*pwPixmap;
    GtkAccelGroup *pag = gtk_accel_group_new();

    ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
	 gtk_widget_get_colormap( pwDialog ), NULL, NULL,
	 fQuestion ? question_xpm : gnu_xpm );
    pwPixmap = gtk_pixmap_new( ppm, NULL );
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
    gtk_widget_add_accelerator( fQuestion ? pwCancel : pwOK, "clicked", pag,
				GDK_Escape, 0, 0 );

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

    default:
	abort();
    }
}

static int Message( char *sz, int fQuestion ) {

    int f = FALSE, fRestoreNextTurn;
    GtkWidget *pwDialog = CreateDialog( fQuestion ? "GNU Backgammon - Question"
					: "GNU Backgammon - Message",
					fQuestion, NULL, &f ),
	*pwPrompt = gtk_label_new( sz );

    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_label_set_justify( GTK_LABEL( pwPrompt ), GTK_JUSTIFY_LEFT );
    gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );

    gtk_window_set_policy( GTK_WINDOW( pwDialog ), FALSE, FALSE, FALSE );
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

    return Message( szPrompt, TRUE );
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

    if( cchOutput > 80 || strchr( sz, '\n' ) )
	/* Long message; display in dialog. */
	/* FIXME if fDisplay is false, skip to the last line and display
	   in status bar. */
	Message( sz, FALSE );
    else
	/* Short message; display in status bar. */
	gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idOutput, sz );
    
    cchOutput = 0;
    g_free( sz );
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

static void NumberOK( GtkWidget *pw, int *pf ) {

    *pf = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( pwEntry ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static int ReadNumber( char *szTitle, char *szPrompt, int nDefault,
		       int nMin, int nMax, int nInc ) {

    int n = INT_MIN;
    GtkObject *pa = gtk_adjustment_new( nDefault, nMin, nMax, nInc, nInc,
					0 );
    GtkWidget *pwDialog = CreateDialog( szTitle, TRUE,
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

static void StringOK( GtkWidget *pw, char **ppch ) {

    *ppch = gtk_editable_get_chars( GTK_EDITABLE( pwEntry ), 0, -1 );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static char *ReadString( char *szTitle, char *szPrompt, char *szDefault ) {

    char *sz = NULL;
    GtkWidget *pwDialog = CreateDialog( szTitle, TRUE,
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

static void RealOK( GtkWidget *pw, float *pr ) {

    *pr = gtk_spin_button_get_value_as_float( GTK_SPIN_BUTTON( pwEntry ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static float ReadReal( char *szTitle, char *szPrompt, double rDefault,
			double rMin, double rMax, double rInc ) {

    float r = ERR_VAL;
    GtkObject *pa = gtk_adjustment_new( rDefault, rMin, rMax, rInc, rInc,
					0 );
    GtkWidget *pwDialog = CreateDialog( szTitle, TRUE,
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

static void EnterCommand( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch = ReadString( "GNU Backgammon - Enter command", FormatPrompt(),
			    "" );

    if( pch ) {
	UserCommand( pch );
	g_free( pch );
    }
}

static void NewMatch( gpointer *p, guint n, GtkWidget *pw ) {

    int nLength = ReadNumber( "GNU Backgammon - New Match",
			      "Match length:", 7, 1, 99, 1 );
    
    if( nLength > 0 ) {
	char sz[ 32 ];

	sprintf( sz, "new match %d", nLength );
	UserCommand( sz );
    }
}

static void NewWeights( gpointer *p, guint n, GtkWidget *pw ) {

    int nSize = ReadNumber( "GNU Backgammon - New Weights",
			      "Number of hidden nodes:", 128, 1, 1024, 1 );
    
    if( nSize > 0 ) {
	char sz[ 32 ];

	sprintf( sz, "new weights %d", nSize );
	UserCommand( sz );
    }
}

static void SetAlpha( gpointer *p, guint k, GtkWidget *pw ) {

    float r = ReadReal( "GNU Backgammon - Learning rate",
			"Learning rate:", rAlpha, 0.0f, 1.0f, 0.01f );

    if( r >= 0.0f ) {
	char sz[ 32 ];

	sprintf( sz, "set training alpha %f", r );
	UserCommand( sz );
    }
}

static void SetAnalysis( gpointer *p, guint k, GtkWidget *pw ) {

    /* FIXME make this into a proper dialog, with more settings, and
     change the "limit" widget into a check button (whether there is a
     limit at all) and a spin button (what the limit is) */
    int n = ReadNumber( "GNU Backgammon - Analysis",
			"Move limit:", cAnalysisMoves < 0 ? 0 : cAnalysisMoves,
			0, 100, 1 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set analysis limit %d", n );
	UserCommand( sz );
    }
}

static void SetAnneal( gpointer *p, guint k, GtkWidget *pw ) {

    float r = ReadReal( "GNU Backgammon - Annealing rate",
			"Annealing rate:", rAnneal, -5.0f, 5.0f, 0.1f );

    if( r >= -5.0f ) {
	char sz[ 32 ];

	sprintf( sz, "set training anneal %f", r );
	UserCommand( sz );
    }
}

static void SetAutoDoubles( gpointer *p, guint k, GtkWidget *pw ) {

    int n = ReadNumber( "GNU Backgammon - Automatic doubles",
			"Number of automatic doubles permitted:",
			cAutoDoubles, 0, 6, 1 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set automatic doubles %d", n );
	UserCommand( sz );
    }
}

static void SetBeavers( gpointer *p, guint k, GtkWidget *pw ) {
    
    int n = ReadNumber( "GNU Backgammon - Beavers",
			"Number of beavers permitted:",
			nBeavers, 0, 6, 1 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set beavers %d", n );
	UserCommand( sz );
    }
}

static void SetCache( gpointer *p, guint k, GtkWidget *pw ) {

    int n = ReadNumber( "GNU Backgammon - Position cache",
			"Cache size (entries):", 8192, 0, 1048576, 1 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set cache %d", n );
	UserCommand( sz );
    }
}

static void SetDelay( gpointer *p, guint k, GtkWidget *pw ) {
    
    int n = ReadNumber( "GNU Backgammon - Delay",
			"Delay (ms):", nDelay, 0, 10000, 100 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set delay %d", n );
	UserCommand( sz );
    }
}

static void SetSeed( gpointer *p, guint k, GtkWidget *pw ) {

    int nRandom, n;

    InitRNG( &nRandom, FALSE );
    n = ReadNumber( "GNU Backgammon - Seed", "Seed:", abs( nRandom ), 0,
		    INT_MAX, 1 );

    if( n >= 0 ) {
	char sz[ 32 ];

	sprintf( sz, "set seed %d", n );
	UserCommand( sz );
    }
}

static void SetThreshold( gpointer *p, guint k, GtkWidget *pw ) {

    float r = ReadReal( "GNU Backgammon - Error threshold",
			"Error threshold:", rThreshold, 0.0f, 6.0f, 0.01f );

    if( r >= 0.0f ) {
	char sz[ 32 ];

	sprintf( sz, "set training threshold %f", r );
	UserCommand( sz );
    }
}

static void FileOK( GtkWidget *pw, char **ppch ) {

    GtkWidget *pwFile = gtk_widget_get_toplevel( pw );

    *ppch = g_strdup( gtk_file_selection_get_filename(
	GTK_FILE_SELECTION( pwFile ) ) );
    gtk_widget_destroy( pwFile );
}

static char *SelectFile( char *szTitle, char *szDefault ) {

    char *pch = NULL;
    GtkWidget *pw = gtk_file_selection_new( szTitle );

    if( szDefault )
	gtk_file_selection_set_filename( GTK_FILE_SELECTION( pw ), szDefault );
    
    gtk_signal_connect( GTK_OBJECT( GTK_FILE_SELECTION( pw )->ok_button ),
			"clicked", GTK_SIGNAL_FUNC( FileOK ), &pch );
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

    return pch;
}

static void FileCommand( char *szPrompt, char *szDefault, char *szCommand ) {

    char *pch;
    
    if( ( pch = SelectFile( szPrompt, szDefault ) ) ) {
#if __GNUC__
	char sz[ strlen( pch ) + strlen( szCommand ) + 2 ];
#elif HAVE_ALLOCA
	char *sz = alloca( strlen( pch ) + strlen( szCommand ) + 2 );
#else
	char sz[ 1024 ];
#endif
	sprintf( sz, "%s \"%s\"", szCommand, pch );
	UserCommand( sz );

	g_free( pch );
    }
}

static void LoadCommands( gpointer *p, guint n, GtkWidget *pw ) {

    FileCommand( "Open commands", NULL, "load commands" );
}

static void SetMET( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch = PathSearch( "met/", szDataDirectory );

    if( pch && access( pch, R_OK ) ) {
	free( pch );
	pch = NULL;
    }
    
    FileCommand( "Set match equity table", pch, "set matchequitytable " );

    if( pch )
	free( pch );
}

static void LoadGame( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGF );
  FileCommand( "Open game", sz, "load game" );
  if ( sz ) 
    free ( sz );

}

static void LoadMatch( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGF );
  FileCommand( "Open match or session", sz, "load match" );
  if ( sz ) 
    free ( sz );

}

static void ImportMat( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_MAT );
  FileCommand( "Import .mat match", sz, "import mat" );
  if ( sz ) 
    free ( sz );

}

static void ImportPos( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_POS );
  FileCommand( "Import .pos position", sz, "import pos" );
  if ( sz ) 
    free ( sz );

}

static void ImportOldmoves( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_OLDMOVES );
  FileCommand( "Import FIBS oldmoves", sz, "import oldmoves" );
  if ( sz ) 
    free ( sz );

}

static void ImportSGG( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultPath ( PATH_SGG );
  FileCommand( "Import .sgg match", sz, "import sgg" );
  if ( sz ) 
    free ( sz );

}

static void SaveGame( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  FileCommand( "Save game", sz, "save game" );
  if ( sz ) 
    free ( sz );

}

static void SaveMatch( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_SGF );
  FileCommand( "Save match or session", sz, "save match" );
  if ( sz ) 
    free ( sz );

}

static void SaveWeights( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.weights" );
  FileCommand( "Save weights", sz, "save weights" );
  if ( sz ) 
    free ( sz );

}

static void ExportGameGam( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_GAM );
  FileCommand( "Export .gam game", sz, "export game gam" );
  if ( sz ) 
    free ( sz );

}

static void ExportGameHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  FileCommand( "Export HTML game", sz, "export game html" );
  if ( sz ) 
    free ( sz );

}

static void ExportGameLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  FileCommand( "Export LaTeX game", sz, "export game latex" );
  if ( sz ) 
    free ( sz );

}

static void ExportGamePDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  FileCommand( "Export PDF game", sz, "export game pdf" );
  if ( sz ) 
    free ( sz );

}

static void ExportGamePostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  FileCommand( "Export PostScript game", sz, "export game postscript" );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  FileCommand( "Export LaTeX match", sz, "export match latex" );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  FileCommand( "Export HTML match", sz, "export match html" );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchMat( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_MAT );
  FileCommand( "Export .mat match", sz, "export match mat" );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchPDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  FileCommand( "Export PDF match", sz, "export match pdf" );
  if ( sz ) 
    free ( sz );

}

static void ExportMatchPostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  FileCommand( "Export PostScript match", sz, "export match postscript" );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionEPS( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_EPS );
  FileCommand( "Export EPS position", sz, "export position eps" );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  FileCommand( "Export HTML position", sz, "export position html" );
  if ( sz ) 
    free ( sz );

}

static void ExportPositionPos( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POS );
  FileCommand( "Export .pos position", sz, "export position pos" );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionLaTeX( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_LATEX );
  FileCommand( "Export LaTeX session", sz, "export session latex" );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionPDF( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_PDF );
  FileCommand( "Export PDF session", sz, "export session pdf" );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionHtml( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_HTML );
  FileCommand( "Export HTML session", sz, "export session html" );
  if ( sz ) 
    free ( sz );

}

static void ExportSessionPostScript( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = getDefaultFileName ( PATH_POSTSCRIPT );
  FileCommand( "Export PostScript session", sz,
               "export session postscript" );
  if ( sz ) 
    free ( sz );

}

static void DatabaseExport( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.gdbm" );
  FileCommand( "Export database", sz, "database export" );
  if ( sz ) 
    free ( sz );

}

static void DatabaseImport( gpointer *p, guint n, GtkWidget *pw ) {

  char *sz = strdup ( PKGDATADIR "/gnubg.gdbm" );
  FileCommand( "Import database", sz, "database import" );
  if ( sz ) 
    free ( sz );

}

typedef struct _evalwidget {
    evalcontext *pec;
    GtkWidget *pwCubeful, *pwReduced, *pwSearchCandidates, *pwSearchTolerance,
	*pwDeterministic, *pwNoOnePlyPrune;
    GtkAdjustment *padjPlies, *padjSearchCandidates, *padjSearchTolerance,
	*padjNoise;
    int *pfOK;
  GtkWidget *pwOptionMenu;
  GtkWidget *pwSearchSpaceMenu;
} evalwidget;

static void EvalGetValues ( evalcontext *pec, evalwidget *pew ) {

    pec->nPlies = pew->padjPlies->value;
    pec->nSearchCandidates = pew->padjSearchCandidates->value;
    pec->rSearchTolerance = pew->padjSearchTolerance->value;
    pec->fCubeful = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwCubeful ) );
    pec->nReduced = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwReduced ) ) ? 7 : 0;
    pec->fNoOnePlyPrune = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwNoOnePlyPrune ) ) ? TRUE : FALSE;
    pec->rNoise = pew->padjNoise->value;
    pec->fDeterministic = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( pew->pwDeterministic ) );

}


static void EvalChanged ( GtkWidget *pw, evalwidget *pew ) {

  int i;
  evalcontext ecCurrent;
  int fFound = FALSE;

  EvalGetValues ( &ecCurrent, pew );

  /* update predefined settings menu */

  for ( i = 0; i < NUM_SETTINGS; i++ )

    if ( ! cmp_evalcontext ( &aecSettings[ i ], &ecCurrent ) ) {

      /* current settings equal to a predefined setting */

      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwOptionMenu ), i );
      fFound = TRUE;
      break;

    }


  /* user defined setting */

  if ( ! fFound )
    gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwOptionMenu ),
                                  NUM_SETTINGS );

  /* update search space menu */

  fFound = FALSE;

  for ( i = 0; i < NUM_SEARCHSPACES; i++ )

    if ( ecCurrent.nSearchCandidates == anSearchCandidates[ i ] &&
         ecCurrent.rSearchTolerance == arSearchTolerances[ i ] ) {

      /* current search spaces settings equal to a predefind setting */

      gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwSearchSpaceMenu ),
                                    i );
      fFound = TRUE;
      break;

    }

  /* user defined setting */

  if ( ! fFound )
    gtk_option_menu_set_history ( GTK_OPTION_MENU ( pew->pwSearchSpaceMenu ),
                                  NUM_SEARCHSPACES );
         

}


static void EvalNoiseValueChanged( GtkAdjustment *padj, evalwidget *pew ) {

    gtk_widget_set_sensitive( pew->pwDeterministic, padj->value != 0.0f );

    EvalChanged ( NULL, pew );
    
}

static void EvalPliesValueChanged( GtkAdjustment *padj, evalwidget *pew ) {

    gtk_widget_set_sensitive( pew->pwSearchCandidates, padj->value > 0 );
    gtk_widget_set_sensitive( pew->pwSearchTolerance, padj->value > 0 );
    gtk_widget_set_sensitive( pew->pwSearchSpaceMenu, padj->value > 0 );
    gtk_widget_set_sensitive( pew->pwReduced, padj->value == 2 );
    gtk_widget_set_sensitive( pew->pwNoOnePlyPrune, padj->value > 1 );

    EvalChanged ( NULL, pew );

}

static void SearchSpaceMenuActivate ( GtkWidget *pwItem,
                                      evalwidget *pew ) {

  int *piSelected;

  piSelected = gtk_object_get_data ( GTK_OBJECT ( pwItem ), "user_data" );

  if ( *piSelected == NUM_SEARCHSPACES )
    return; /* user defined */

  /* set all widgets to predefined values */

  gtk_adjustment_set_value ( pew->padjSearchCandidates,
                             anSearchCandidates[ *piSelected ] );
  gtk_adjustment_set_value ( pew->padjSearchTolerance,
                             arSearchTolerances [ *piSelected ] );

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
  gtk_adjustment_set_value ( pew->padjSearchCandidates,
                             pec->nSearchCandidates );
  gtk_adjustment_set_value ( pew->padjSearchTolerance,
                             pec->rSearchTolerance );
  gtk_adjustment_set_value ( pew->padjNoise, pec->rNoise );

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwReduced ),
                                pec->nReduced );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwNoOnePlyPrune ),
                                pec->fNoOnePlyPrune );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwCubeful ),
                                pec->fCubeful );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwDeterministic ),
                                pec->fDeterministic );


}

/*
 * Create widget for displaying evaluation settings
 *
 */

static GtkWidget *EvalWidget( evalcontext *pec, int *pfOK ) {

    evalwidget *pew;
    GtkWidget *pwEval, *pw;

    GtkWidget *pwFrame, *pwFrame2;
    GtkWidget *pw2, *pw3, *pw4;

    GtkWidget *pwMenu;
    GtkWidget *pwItem;

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

    pwFrame = gtk_frame_new ( "Predefined settings" );

    gtk_container_add ( GTK_CONTAINER ( pwEval ), pwFrame );

    pw2 = gtk_vbox_new ( FALSE, 8 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw2 );
    gtk_container_set_border_width ( GTK_CONTAINER ( pw2 ), 8 );

    /* option menu with selection of predefined settings */

    gtk_container_add ( GTK_CONTAINER ( pw2 ),
                        gtk_label_new ( "Select a predefined setting:" ) );

    pwMenu = gtk_menu_new ();

    for ( i = 0; i <= NUM_SETTINGS; i++ ) {

      if ( i < NUM_SETTINGS )
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label ( 
                          aszSettings[ i ] ) );
      else
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label (
                          "user defined" ) );

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

    pwFrame = gtk_frame_new ( "User defined settings" );
    gtk_container_add ( GTK_CONTAINER ( pwEval ), pwFrame );
    
    pw2 = gtk_vbox_new ( FALSE, 8 );
    gtk_container_set_border_width( GTK_CONTAINER( pw2 ), 8 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame ), pw2 );

    /* lookahead */

    pwFrame2 = gtk_frame_new ( "Lookahead" );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwFrame2 ), pw );

    pew->padjPlies = GTK_ADJUSTMENT( gtk_adjustment_new( pec->nPlies, 0, 7,
							 1, 1, 0 ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Plies:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_spin_button_new( pew->padjPlies, 1, 0 ) );


    /*
     * search space
     */

    pwFrame2 = gtk_frame_new ( "Search space" );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    pw3 = gtk_vbox_new ( FALSE, 8 );
    gtk_container_set_border_width( GTK_CONTAINER( pw3 ), 8 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame2 ), pw3 );

    /* predefined search spaces */

    pwMenu = gtk_menu_new ();

    for ( i = 0; i <= NUM_SEARCHSPACES; i++ ) {

      if ( i < NUM_SEARCHSPACES )
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label ( 
                          aszSearchSpaces[ i ] ) );
      else
        gtk_menu_append ( GTK_MENU ( pwMenu ),
                          pwItem = gtk_menu_item_new_with_label (
                          "user defined" ) );

      pi = malloc ( sizeof ( int ) );
      *pi = i;
      gtk_object_set_data_full( GTK_OBJECT( pwItem ), "user_data", 
                                pi, free );

      gtk_signal_connect ( GTK_OBJECT ( pwItem ), "activate",
                           GTK_SIGNAL_FUNC ( SearchSpaceMenuActivate ),
                           (void *) pew );

    }

    pew->pwSearchSpaceMenu = gtk_option_menu_new ();
    gtk_option_menu_set_menu ( GTK_OPTION_MENU ( pew->pwSearchSpaceMenu ), 
                               pwMenu );

    gtk_container_add ( GTK_CONTAINER ( pw3 ), pew->pwSearchSpaceMenu );

                                                               
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pw3 ), pw );

    pew->padjSearchCandidates = GTK_ADJUSTMENT( gtk_adjustment_new(
	pec->nSearchCandidates, 2, MAX_SEARCH_CANDIDATES, 1, 1, 0 ) );

    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Search candidates:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       pew->pwSearchCandidates = gtk_spin_button_new(
			   pew->padjSearchCandidates, 1, 0 ) );


    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pw3 ), pw );

    pew->padjSearchTolerance = GTK_ADJUSTMENT( gtk_adjustment_new(
	pec->rSearchTolerance, 0, 1, 0.01, 0.01, 0.0 ) );

    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Search tolerance:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       pew->pwSearchTolerance = gtk_spin_button_new(
			   pew->padjSearchTolerance, 0.01, 3 ) );

    /* reduced evaluation */

    /* FIXME if and when we support different values for nReduced, this
       check button won't work */

    pwFrame2 = gtk_frame_new ( "Reduced evaluations" );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );
    pw4 = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame2 ), pw4 );

    gtk_container_add( GTK_CONTAINER( pw4 ),
		       pew->pwReduced = gtk_check_button_new_with_label(
			   "Reduced 2 ply evaluation" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwReduced ),
				  pec->nReduced );
    gtk_container_add( GTK_CONTAINER( pw4 ),
		       pew->pwNoOnePlyPrune = gtk_check_button_new_with_label(
			   "No 1-ply pruning" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwNoOnePlyPrune ),
				  pec->fNoOnePlyPrune );

    /* cubeful */
    
    pwFrame2 = gtk_frame_new ( "Cubeful evaluations" );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    gtk_container_add( GTK_CONTAINER( pwFrame2 ),
		       pew->pwCubeful = gtk_check_button_new_with_label(
			   "Cubeful chequer evaluation" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwCubeful ),
				  pec->fCubeful );

    /* noise */

    pwFrame2 = gtk_frame_new ( "Noise" );
    gtk_container_add ( GTK_CONTAINER ( pw2 ), pwFrame2 );

    pw3 = gtk_vbox_new ( FALSE, 0 );
    gtk_container_add ( GTK_CONTAINER ( pwFrame2 ), pw3 );


    pew->padjNoise = GTK_ADJUSTMENT( gtk_adjustment_new(
	pec->rNoise, 0, 1, 0.001, 0.001, 0.0 ) );
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pw3 ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Noise:" ) );
    gtk_container_add( GTK_CONTAINER( pw ), gtk_spin_button_new(
	pew->padjNoise, 0.001, 3 ) );
    
    gtk_container_add( GTK_CONTAINER( pw3 ),
		       pew->pwDeterministic = gtk_check_button_new_with_label(
			   "Deterministic noise" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( pew->pwDeterministic ),
				  pec->fDeterministic );

    /* setup signals */

    pew->pec = pec;
    pew->pfOK = pfOK;

    gtk_signal_connect( GTK_OBJECT( pew->padjPlies ), "value-changed",
			GTK_SIGNAL_FUNC( EvalPliesValueChanged ), pew );
    EvalPliesValueChanged( pew->padjPlies, pew );
    
    gtk_signal_connect( GTK_OBJECT( pew->padjNoise ), "value-changed",
			GTK_SIGNAL_FUNC( EvalNoiseValueChanged ), pew );
    EvalNoiseValueChanged( pew->padjNoise, pew );

    gtk_signal_connect( GTK_OBJECT( pew->padjSearchCandidates ), 
                        "value-changed",
			GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect( GTK_OBJECT( pew->padjSearchTolerance ), 
                        "value-changed",
			GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwDeterministic ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwCubeful ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwReduced ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    gtk_signal_connect ( GTK_OBJECT( pew->pwNoOnePlyPrune ), "toggled",
                         GTK_SIGNAL_FUNC( EvalChanged ), pew );

    
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

    if( pec->nSearchCandidates != pecOrig->nSearchCandidates ) {
	sprintf( sz, "%s candidates %d", szPrefix, pec->nSearchCandidates );
	UserCommand( sz );
    }

    if( pec->rSearchTolerance != pecOrig->rSearchTolerance ) {
	sprintf( sz, "%s tolerance %.3f", szPrefix, pec->rSearchTolerance );
	UserCommand( sz );
    }
    
    if( pec->nReduced != pecOrig->nReduced ) {
	sprintf( sz, "%s reduced %d", szPrefix, pec->nReduced );
	UserCommand( sz );
    }

    if( pec->fNoOnePlyPrune != pecOrig->fNoOnePlyPrune ) {
	sprintf( sz, "%s nooneplyprune %s", szPrefix,
			pec->fNoOnePlyPrune ? "on" : "off" );
	UserCommand( sz );
    }

    if( pec->fCubeful != pecOrig->fCubeful ) {
	sprintf( sz, "%s cubeful %s", szPrefix, pec->fCubeful ? "on" : "off" );
	UserCommand( sz );
    }

    if( pec->rNoise != pecOrig->rNoise ) {
	sprintf( sz, "%s noise %.3f", szPrefix, pec->rNoise );
	UserCommand( sz );
    }
    
    if( pec->fDeterministic != pecOrig->fDeterministic ) {
	sprintf( sz, "%s deterministic %s", szPrefix, pec->fDeterministic ?
		 "on" : "off" );
	UserCommand( sz );
    }

    outputresume();
}

static void SetAnalysisEvalCube( gpointer *p, guint n, GtkWidget *pw ) {

    /* FIXME Consider this code as a temporary hack, we should later
       make a better dialog box for setting all analysis options */

    evalcontext ec;
    GtkWidget *pwDialog, *pwEval;
    int fOK;
    
    memcpy( &ec, &esAnalysisCube.ec, sizeof ec );

    pwEval = EvalWidget( &ec, &fOK );
    
    pwDialog = CreateDialog( "GNU Backgammon - Analysis cube decisions", TRUE,
                             GTK_SIGNAL_FUNC( EvalOK ), pwEval );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwEval );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK )
        SetEvalCommands( "set analysis cubedecision eval", &ec,
			 &esAnalysisCube.ec );
}

static void SetAnalysisEvalChequer( gpointer *p, guint n, GtkWidget *pw ) {

    /* FIXME Consider this code as a temporary hack, we should later
       make a better dialog box for setting all analysis options */

    evalcontext ec;
    GtkWidget *pwDialog, *pwEval;
    int fOK;
    
    memcpy( &ec, &esAnalysisChequer.ec, sizeof ec );

    pwEval = EvalWidget( &ec, &fOK );
    
    pwDialog = CreateDialog( "GNU Backgammon - Analysis chequer play", TRUE,
                             GTK_SIGNAL_FUNC( EvalOK ), pwEval );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwEval );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK )
        SetEvalCommands( "set analysis chequer eval", &ec,
                         &esAnalysisChequer.ec );
}

static void SetEvalChequer( gpointer *p, guint n, GtkWidget *pw ) {

    evalcontext ec;
    GtkWidget *pwDialog, *pwEval;
    int fOK;
    
    memcpy( &ec, &esEvalChequer.ec, sizeof ec );

    pwEval = EvalWidget( &ec, &fOK );
    
    pwDialog = CreateDialog( "GNU Backgammon - Chequer play", TRUE,
                             GTK_SIGNAL_FUNC( EvalOK ), pwEval );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwEval );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK )
        SetEvalCommands( "set evaluation chequer eval", &ec,
                         &esEvalChequer.ec );
}

static void SetEvalCube( gpointer *p, guint n, GtkWidget *pw ) {

    evalcontext ec;
    GtkWidget *pwDialog, *pwEval;
    int fOK;
    
    memcpy( &ec, &esEvalCube.ec, sizeof ec );

    pwEval = EvalWidget( &ec, &fOK );
    
    pwDialog = CreateDialog( "GNU Backgammon - Cube decisions", TRUE,
                             GTK_SIGNAL_FUNC( EvalOK ), pwEval );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwEval );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                        GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

    if( fOK )
        SetEvalCommands( "set evaluation cube eval", &ec, &esEvalCube.ec );
}
 
typedef struct _playerswidget {
    int *pfOK;
    player *ap;
    GtkWidget *apwName[ 2 ], *apwRadio[ 2 ][ 4 ], *apwEval[ 2 ],
	*apwSocket[ 2 ], *apwExternal[ 2 ];
    char aszSocket[ 2 ][ 128 ];
} playerswidget;

static void PlayerTypeToggled( GtkWidget *pw, playerswidget *ppw ) {

    int i;

    for( i = 0; i < 2; i++ ) {
	gtk_widget_set_sensitive(
	    ppw->apwEval[ i ], gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON( ppw->apwRadio[ i ][ 1 ] ) ) );
	gtk_widget_set_sensitive(
	    ppw->apwExternal[ i ], gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON( ppw->apwRadio[ i ][ 3 ] ) ) );
    }
}

static GtkWidget *PlayersPage( playerswidget *ppw, int i ) {

    GtkWidget *pwPage, *pw;
    static int aiRadioButton[ PLAYER_PUBEVAL + 1 ] = { 3, 0, 1, 2 };

    pwPage = gtk_vbox_new( FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pwPage ), 8 );
    
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwPage ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Name:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       ppw->apwName[ i ] = gtk_entry_new() );
    gtk_entry_set_text( GTK_ENTRY( ppw->apwName[ i ] ),
			(ppw->ap[ i ].szName) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 0 ] =
		       gtk_radio_button_new_with_label( NULL, "Human" ) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 1 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   "GNU Backgammon" ) );

    gtk_container_add( GTK_CONTAINER( pwPage ), ppw->apwEval[ i ] =
		       EvalWidget( &ppw->ap[ i ].esChequer.ec, NULL ) );
    gtk_widget_set_sensitive( ppw->apwEval[ i ],
			      ap[ i ].pt == PLAYER_GNU );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 2 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   "Pubeval" ) );
    
    gtk_container_add( GTK_CONTAINER( pwPage ),
		       ppw->apwRadio[ i ][ 3 ] =
		       gtk_radio_button_new_with_label_from_widget(
			   GTK_RADIO_BUTTON( ppw->apwRadio[ i ][ 0 ] ),
			   "External" ) );
    ppw->apwExternal[ i ] = pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_set_border_width( GTK_CONTAINER( pw ), 8 );
    gtk_widget_set_sensitive( pw, ap[ i ].pt == PLAYER_EXTERNAL );
    gtk_container_add( GTK_CONTAINER( pwPage ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Socket:" ) );
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

	EvalOK( pplw->apwEval[ i ], pplw->apwEval[ i ] );

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
    
    pwDialog = CreateDialog( "GNU Backgammon - Players", TRUE,
			     GTK_SIGNAL_FUNC( PlayersOK ), &plw );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwNotebook = gtk_notebook_new() );
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PlayersPage( &plw, 0 ),
			      gtk_label_new( "Player 0" ) );
    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
			      PlayersPage( &plw, 1 ),
			      gtk_label_new( "Player 1" ) );
    
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

typedef struct _rolloutwidget {
    evalcontext ec;
    int nTrials, nTruncate, fCubeful, fVarRedn, nSeed, fInitial;
    GtkWidget *pwEval, *pwCubeful, *pwVarRedn, *pwInitial;
    GtkAdjustment *padjTrials, *padjTrunc, *padjSeed;
    int *pfOK;
} rolloutwidget;

static void SetRolloutsOK( GtkWidget *pw, rolloutwidget *prw ) {

    *prw->pfOK = TRUE;

    prw->nTrials = prw->padjTrials->value;
    prw->nTruncate = prw->padjTrunc->value;
    prw->nSeed = prw->padjSeed->value;
    prw->fCubeful = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( prw->pwCubeful ) );
    prw->fVarRedn = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( prw->pwVarRedn ) );
    prw->fInitial = gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON( prw->pwInitial ) );
    
    EvalOK( prw->pwEval, prw->pwEval );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void SetRollouts( gpointer *p, guint n, GtkWidget *pwIgnore ) {

    GtkWidget *pwDialog, *pwBox, *pw;
    int fOK = FALSE;
    rolloutwidget rw;
    char sz[ 256 ];
    
    memcpy( &rw.ec, &rcRollout.aecChequer[ 0 ], sizeof( rw.ec ) );
    rw.pfOK = &fOK;
    
    pwDialog = CreateDialog( "GNU Backgammon - Rollouts", TRUE,
			     GTK_SIGNAL_FUNC( SetRolloutsOK ), &rw );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwBox = gtk_vbox_new( FALSE, 0 ) );
    gtk_container_set_border_width( GTK_CONTAINER( pwBox ), 8 );

    rw.padjTrials = GTK_ADJUSTMENT( gtk_adjustment_new( rcRollout.nTrials, 1,
							1296 * 1296,
							36, 36, 0 ) );
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwBox ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Trials:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_spin_button_new( rw.padjTrials, 36, 0 ) );
    
    rw.padjTrunc = GTK_ADJUSTMENT( gtk_adjustment_new( rcRollout.nTruncate, 0,
						       1000, 1, 1, 0 ) );
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwBox ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Truncation:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_spin_button_new( rw.padjTrunc, 1, 0 ) );

    gtk_container_add( GTK_CONTAINER( pwBox ),
		       rw.pwCubeful = gtk_check_button_new_with_label(
			   "Cubeful" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( rw.pwCubeful ),
				  rcRollout.fCubeful );

    gtk_container_add( GTK_CONTAINER( pwBox ),
		       rw.pwVarRedn = gtk_check_button_new_with_label(
			   "Variance reduction" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( rw.pwVarRedn ),
				  rcRollout.fVarRedn );

    gtk_container_add( GTK_CONTAINER( pwBox ),
		       rw.pwInitial = gtk_check_button_new_with_label(
			   "Rollout as initial position" ) );
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( rw.pwInitial ),
				  rcRollout.fInitial );

    rw.padjSeed = GTK_ADJUSTMENT( gtk_adjustment_new( abs( rcRollout.nSeed ),
						      0, INT_MAX, 1, 1, 0 ) );
    pw = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( pwBox ), pw );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_label_new( "Seed:" ) );
    gtk_container_add( GTK_CONTAINER( pw ),
		       gtk_spin_button_new( rw.padjSeed, 1, 0 ) );

    gtk_container_add( GTK_CONTAINER( pwBox ), pw =
		       gtk_frame_new( "Evaluation" ) );
    gtk_container_add( GTK_CONTAINER( pw ), rw.pwEval =
		       EvalWidget( &rw.ec, NULL ) );
	
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

	if( rw.nTrials != rcRollout.nTrials ) {
	    sprintf( sz, "set rollout trials %d", rw.nTrials );
	    UserCommand( sz );
	}

	if( rw.nTruncate != rcRollout.nTruncate ) {
	    sprintf( sz, "set rollout truncation %d", rw.nTruncate );
	    UserCommand( sz );
	}

	if( rw.fCubeful != rcRollout.fCubeful ) {
	    sprintf( sz, "set rollout cubeful %s",
		     rw.fCubeful ? "on" : "off" );
	    UserCommand( sz );
	}

	if( rw.fVarRedn != rcRollout.fVarRedn ) {
	    sprintf( sz, "set rollout varredn %s",
		     rw.fVarRedn ? "on" : "off" );
	    UserCommand( sz );
	}

	if( rw.fInitial != rcRollout.fInitial ) {
	    sprintf( sz, "set rollout initial %s",
		     rw.fVarRedn ? "on" : "off" );
	    UserCommand( sz );
	}

	if( rw.nSeed != rcRollout.nSeed ) {
	    sprintf( sz, "set rollout seed %d", rw.nSeed );
	    UserCommand( sz );
	}

	/* FIXME another temporary hack (should be able to set chequer and
	   cube parameters independently) */
	SetEvalCommands( "set rollout chequer", &rw.ec,
			 &rcRollout.aecChequer[ 0 ] );
	SetEvalCommands( "set rollout cube", &rw.ec,
			 &rcRollout.aecCube[ 0 ] );
	
	outputresume();
    }
}

extern void GTKEval( char *szOutput ) {

    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Evaluation",
					FALSE, NULL, NULL ),
	*pwText = gtk_text_new( NULL, NULL );
    GdkFont *pf;
#if WIN32
    GtkWidget *pwButtons,
        *pwCopy = gtk_button_new_with_label( "Copy" );
#endif

#if WIN32
    /* Windows fonts come out smaller than you ask for, for some reason... */
    pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-14-"
			"*-*-*-m-*-iso8859-1" );
#else
    pf = gdk_font_load( "-b&h-lucidatypewriter-medium-r-normal-sans-12-"
			"*-*-*-m-*-iso8859-1" );
#endif

#if WIN32
    /* FIXME There should be some way to extract the text on Unix as well */
    pwButtons = DialogArea( pwDialog, DA_BUTTONS );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwCopy );
    gtk_signal_connect( GTK_OBJECT( pwCopy ), "clicked",
			GTK_SIGNAL_FUNC( GTKWinCopy ), (gpointer) szOutput );
#endif
    
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

static void HintCubeRollout( GtkWidget *pw, void *p ) {

    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
    
    UserCommand( "rollout =cube" );
}

extern void GTKDoubleHint( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                           float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                           const evalsetup *pes ) {

    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL,
					NULL ),
      *pw, *pwRollout = gtk_button_new_with_label( "Rollout" );

    pw = CubeAnalysis ( aarOutput, aarStdDev, pes, FALSE );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pw );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwRollout );
    gtk_signal_connect( GTK_OBJECT( pwRollout ), "clicked",
			GTK_SIGNAL_FUNC( HintCubeRollout ), NULL );
    
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

extern void GTKTakeHint( float aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                         float aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ], 
                         const evalsetup *pes ) {

    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL,
					NULL ),
      *pw, *pwRollout = gtk_button_new_with_label( "Rollout" );

    pw = TakeAnalysis ( MOVE_NORMAL, aarOutput, aarStdDev, pes );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pw );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwRollout );
    gtk_signal_connect( GTK_OBJECT( pwRollout ), "clicked",
			GTK_SIGNAL_FUNC( HintCubeRollout ), NULL );
    
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
      CreateDialog( "GNU Backgammon - Hint", FALSE, NULL, NULL );
    GtkWidget *pw;
    GtkWidget *pwTable = gtk_table_new( 2, 3, FALSE );

    char *pch, sz[ 16 ];

    /* equity before resignation */
    
    gtk_table_attach( GTK_TABLE( pwTable ), pw = gtk_label_new(
	fMWC ? "MWC before resignation" : "Equity before resignation" ), 0, 1, 0, 1,
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
	fMWC ? "MWC after resignation" : "Equity after resignation" ), 0, 1, 1, 2,
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
	pch = "You should accept the resignation!";
    else
	pch = "You should reject the resignation!";
    
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



#if WIN32
extern void GTKWinCopy( GtkWidget *widget, gpointer data) {
   WinCopy( (char *) data);
}
#endif

static void HintMove( GtkWidget *pw, GtkWidget *pwMoves ) {

    move *pm;
    char move[ 40 ];
    int i;
    
    assert( GTK_CLIST( pwMoves )->selection );

    i = GPOINTER_TO_INT( GTK_CLIST( pwMoves )->selection->data );
    pm = gtk_clist_get_row_data( GTK_CLIST( pwMoves ), i );

    FormatMove( move, ms.anBoard, pm->anMove );
    UserCommand( move );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pwMoves ) );
}

static void HintRollout( GtkWidget *pw, GtkWidget *pwMoves ) {

    int c;
    GList *pl;
    char *sz, *pch;
    
    assert( GTK_CLIST( pwMoves )->selection );

    /* FIXME selection is not in order... should we roll the moves out
       in the same order they're listed? */
    
    for( c = 0, pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next )
	c++;

#if HAVE_ALLOCA
    sz = alloca( c * 6 + 9 ); /* "rollout " plus c * "=9999 " plus \0 */
#else
    sz = malloc( c * 6 + 9 );
#endif

    strcpy( sz, "rollout " );
    pch = sz + 8;

    for( pl = GTK_CLIST( pwMoves )->selection; pl; pl = pl->next ) {
	sprintf( pch, "=%d ", GPOINTER_TO_INT( pl->data ) + 1 );
	pch = strchr( pch, 0 );
    }
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pwMoves ) );
    
    UserCommand( sz );
    
#if !HAVE_ALLOCA
    free( sz );
#endif        
}

static void HintSelect( GtkWidget *pw, int y, int x, GdkEventButton *peb,
			hintdata *phd ) {
    
    int c = CheckHintButtons();
    
    if( c && peb )
	gtk_selection_owner_set( pw, GDK_SELECTION_PRIMARY, peb->time );

    /* Double clicking a row makes that move. */
    if( c == 1 && peb && peb->type == GDK_2BUTTON_PRESS && phd->fButtonsValid )
	gtk_button_clicked( GTK_BUTTON( phd->pwMove ) );
}

static gint HintClearSelection( GtkWidget *pw, GdkEventSelection *pes,
				hintdata *phd ) {
    
    gtk_clist_unselect_all( GTK_CLIST( pw ) );

    return TRUE;
}

typedef int ( *cfunc )( const void *, const void * );

static int CompareInts( int *p0, int *p1 ) {

    return *p0 - *p1;
}

static void HintGetSelection( GtkWidget *pw, GtkSelectionData *psd,
			      guint n, guint t, hintdata *phd ) {
    int c, i;
    GList *pl;
    char *sz, *pch;
    int *an;
    
    for( c = 0, pl = GTK_CLIST( pw )->selection; pl; pl = pl->next )
	c++;

#if HAVE_ALLOCA
    an = alloca( c * sizeof( an[ 0 ] ) );
    sz = alloca( c * 160 );
#else
    an = malloc( c * sizeof( an[ 0 ] ) );
    sz = malloc( c * 160 );
#endif

    *sz = 0;
    
    for( i = 0, pl = GTK_CLIST( pw )->selection; pl;
	 pl = pl->next, i++ )
	an[ i ] = GPOINTER_TO_INT( pl->data );

    qsort( an, c, sizeof( an[ 0 ] ), (cfunc) CompareInts );

    for( i = 0, pch = sz; i < c; i++, pch = strchr( pch, 0 ) )
	FormatMoveHint( pch, &ms, phd->pml, an[ i ], TRUE );
        
    gtk_selection_data_set( psd, GDK_SELECTION_TYPE_STRING, 8,
			    sz, strlen( sz ) );
    
#if !HAVE_ALLOCA
    free( an );
    free( sz );
#endif        
}

static void DestroyHint( gpointer p ) {

    hintdata *phd = gtk_object_get_user_data( GTK_OBJECT( pwHint ) );

    free( phd->pml->amMoves );
    free( phd->pml );
    
    pwHint = NULL;
}

extern void GTKHint( movelist *pmlOrig ) {

    GtkWidget  *psw = gtk_scrolled_window_new( NULL, NULL ),
	*pwButtons,
	*pwMove = gtk_button_new_with_label( "Move" ),
	*pwRollout = gtk_button_new_with_label( "Rollout" ),
	*pwMoves;
    static hintdata hd;
    movelist *pml;
    
    if( pwHint )
	gtk_widget_destroy( pwHint );

    pml = malloc( sizeof( *pml ) );
    memcpy( pml, pmlOrig, sizeof( *pml ) );
    
    pml->amMoves = malloc( pmlOrig->cMoves * sizeof( move ) );
    memcpy( pml->amMoves, pmlOrig->amMoves, pmlOrig->cMoves * sizeof( move ) );

    hd.pwMove = pwMove;
    hd.pwRollout = pwRollout;
    hd.pml = pml;
    hd.fButtonsValid = TRUE;
    hd.pw = pwMoves = CreateMoveList( &hd, -1 /* FIXME change if they've
						 already moved */ );
    
    pwHint = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL, NULL );
    gtk_object_set_user_data( GTK_OBJECT( pwHint ), &hd );
    pwButtons = DialogArea( pwHint, DA_BUTTONS );
    
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
				    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( psw ), pwMoves );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwHint, DA_MAIN ) ), psw );

    gtk_signal_connect( GTK_OBJECT( pwMove ), "clicked",
			GTK_SIGNAL_FUNC( HintMove ), pwMoves );
    gtk_signal_connect( GTK_OBJECT( pwRollout ), "clicked",
			GTK_SIGNAL_FUNC( HintRollout ), pwMoves );

    gtk_container_add( GTK_CONTAINER( pwButtons ), pwMove );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwRollout );

    gtk_selection_add_target( pwMoves, GDK_SELECTION_PRIMARY,
			      GDK_SELECTION_TYPE_STRING, 0 );
    
    HintSelect( pwMoves, 0, 0, NULL, &hd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "select-row",
			GTK_SIGNAL_FUNC( HintSelect ), &hd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "unselect-row",
			GTK_SIGNAL_FUNC( HintSelect ), &hd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "selection_clear_event",
			GTK_SIGNAL_FUNC( HintClearSelection ), &hd );
    gtk_signal_connect( GTK_OBJECT( pwMoves ), "selection_get",
			GTK_SIGNAL_FUNC( HintGetSelection ), &hd );
    
    gtk_window_set_default_size( GTK_WINDOW( pwHint ), 0, 300 );
    
    gtk_object_weakref( GTK_OBJECT( pwHint ), DestroyHint, &pwHint );
    
    gtk_widget_show_all( pwHint );
}

static GtkWidget *pwRolloutDialog, *pwRolloutResult, *pwRolloutProgress;
static int iRolloutRow;
static guint nRolloutSignal;

static void RolloutCancel( GtkObject *po, gpointer p ) {

    pwRolloutDialog = pwRolloutResult = pwRolloutProgress = NULL;
    fInterrupt = TRUE;
}

extern void GTKRollout( int c, char asz[][ 40 ], int cGames,
			rolloutstat ars[][ 2 ] ) {
    
    static char *aszTitle[] = {
	"", "Win", "Win (g)", "Win (bg)", "Lose (g)", "Lose (bg)",
        "Cubeless", "Cubeful"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    int i;
    GtkWidget *pwVbox;
    GtkWidget *pwButtons,
#ifdef WIN32
        *pwCopy = gtk_button_new_with_label( "Copy rollout results" );
#else
        *pwCopy = gtk_button_new_with_label( "Dump rollout results" );
#endif    
    GtkWidget *pwViewStat = gtk_button_new_with_label ( "View statistics" );

    pwRolloutDialog = CreateDialog( "GNU Backgammon - Rollout", FALSE, NULL,
				    NULL );

    nRolloutSignal = gtk_signal_connect( GTK_OBJECT( pwRolloutDialog ),
					 "destroy",
					 GTK_SIGNAL_FUNC( RolloutCancel ),
					 NULL );

    /* Buttons */

    pwButtons = DialogArea( pwRolloutDialog, DA_BUTTONS );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwCopy );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwViewStat );

    /* Setup signals */

    gtk_signal_connect( GTK_OBJECT( pwCopy ), "clicked",
			GTK_SIGNAL_FUNC( GTKDumpRolloutResults ), NULL );

    gtk_signal_connect( GTK_OBJECT( pwViewStat ), "clicked",
			GTK_SIGNAL_FUNC( GTKViewRolloutStatistics ),
			(gpointer) ars );

    pwVbox = gtk_vbox_new( FALSE, 4 );
	
    pwRolloutResult = gtk_clist_new_with_titles( 8, aszTitle );
    gtk_clist_column_titles_passive( GTK_CLIST( pwRolloutResult ) );
    
    pwRolloutProgress = gtk_progress_bar_new();
    gtk_progress_set_show_text ( GTK_PROGRESS ( pwRolloutProgress ), TRUE );

    gtk_box_pack_start( GTK_BOX( pwVbox ), pwRolloutResult, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwVbox ), pwRolloutProgress, FALSE, FALSE,
			0 );
    
    for( i = 0; i < 7; i++ ) {
	gtk_clist_set_column_auto_resize( GTK_CLIST( pwRolloutResult ), i,
					  TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( pwRolloutResult ), i,
					    GTK_JUSTIFY_RIGHT );
    }

    for( i = 0; i < c; i++ ) {
	gtk_clist_append( GTK_CLIST( pwRolloutResult ), aszEmpty );
	gtk_clist_append( GTK_CLIST( pwRolloutResult ), aszEmpty );

	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), i << 1, 0,
			    asz[ i ] );
	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), ( i << 1 ) | 1, 0,
			    "Standard error" );
    }

    gtk_progress_configure( GTK_PROGRESS( pwRolloutProgress ), 0, 0, cGames );
    gtk_progress_set_format_string( GTK_PROGRESS( pwRolloutProgress ),
				    "%v/%u (%p%%)" );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwRolloutDialog, DA_MAIN ) ),
		       pwVbox );

    gtk_window_set_modal( GTK_WINDOW( pwRolloutDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwRolloutDialog ),
				  GTK_WINDOW( pwMain ) );
    
    gtk_widget_show_all( pwRolloutDialog );

    GTKDisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    GTKAllowStdin();
}

extern void GTKDumpRolloutResults(GtkWidget *widget, gpointer data) {

    /* FIXME - This function should also dump the rollout settings? Which?
               At least the number of trials.  */

    char sz[4096];           
    char szL[40];           
    gchar *szTemp;
    int i, j;

    sprintf( sz, "                              Win    W(g)   W(bg)  L(g)"
        "   L(bg) Equity  Cubeful\n" );
   
    for ( j = 0; j < GTK_CLIST(pwRolloutResult)->rows ; j++){
       for ( i = 0; i < 8 ; i++) {
           gtk_clist_get_text (GTK_CLIST( pwRolloutResult ), j, i, &szTemp);
           if (i == 0) 
              sprintf( szL, "%28.28s", szTemp);
           else if ( i < OUTPUT_EQUITY + 1 )
              sprintf( szL, "%7.7s", szTemp); 
           else 
              sprintf( szL, "%8.8s", szTemp); 

           strcat(sz , szL);
       }
       strcat( sz , "\n" );
    }
#ifdef WIN32
    WinCopy( sz );
#else
    printf("%s", sz);
#endif
}


/*
 * Make pages with statistics.
 */

static GtkWidget *
GTKStatPageWin ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 7 ];

  static char *aszRow[ 7 ];
  int i;
  int anTotal[ 6 ];

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( "Win statistics" );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* table with results */

  aszColumnTitle[ 0 ] = strdup ( "Cube" );
  for ( i = 0; i < 2; i++ ) {
    aszColumnTitle[ 3 * i + 1 ] = 
      g_strdup_printf ( "Win Single\n%s", ap[ i ].szName );
    aszColumnTitle[ 3 * i + 2 ] = 
      g_strdup_printf ( "Win Gammon\n%s", ap[ i ].szName );
    aszColumnTitle[ 3 * i + 3 ] = 
      g_strdup_printf ( "Win BG\n%s", ap[ i ].szName );
  }

  pwStat = gtk_clist_new_with_titles( 7, aszColumnTitle );

  for ( i = 0; i < 7; i++ )
    g_free ( aszColumnTitle[ i ] );

  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 7; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  memset ( anTotal, 0, sizeof ( anTotal ) );
  
  for ( i = 0; i < STAT_MAXCUBE; i++ ) {

    sprintf ( aszRow[ 0 ], ( i < STAT_MAXCUBE - 1 ) ?
	      "%d-cube" : ">= %d-cube", 1 << i);
    sprintf ( aszRow[ 1 ], "%d", prs->acWin[ i ] );
    sprintf ( aszRow[ 2 ], "%d", prs->acWinGammon[ i ] );
    sprintf ( aszRow[ 3 ], "%d", prs->acWinBackgammon[ i ] );

    sprintf ( aszRow[ 4 ], "%d", (prs+1)->acWin[ i ] );
    sprintf ( aszRow[ 5 ], "%d", (prs+1)->acWinGammon[ i ] );
    sprintf ( aszRow[ 6 ], "%d", (prs+1)->acWinBackgammon[ i ] );

    anTotal[ 0 ] += prs->acWin[ i ];
    anTotal[ 1 ] += prs->acWinGammon[ i ];
    anTotal[ 2 ] += prs->acWinBackgammon[ i ];
    anTotal[ 3 ] += (prs+1)->acWin[ i ];
    anTotal[ 4 ] += (prs+1)->acWinGammon[ i ];
    anTotal[ 5 ] += (prs+1)->acWinBackgammon[ i ];

    gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  }


  sprintf ( aszRow[ 0 ], "Total" );
  for ( i = 0; i < 6; i++ )
    sprintf ( aszRow[ i + 1 ], "%d", anTotal[ i ] );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  sprintf ( aszRow[ 0 ], "%%" );
  for ( i = 0; i < 6; i++ )
    sprintf ( aszRow[ i + 1 ], "%6.2f%%",
	      100.0 * (float) anTotal[ i ] / (float) cGames );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  for ( i = 0; i < 7; i++ ) free ( aszRow[ i ] );


  return pw;

}


static GtkWidget *
GTKStatPageCube ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 5 ];

  static char *aszRow[ 5 ];
  int i;
  char sz[ 100 ];
  int anTotal[ 4 ];

  aszColumnTitle[ 0 ] = strdup ( "Cube" );
  for ( i = 0; i < 2; i++ ) {
    aszColumnTitle[ 2 * i + 1 ] = 
      g_strdup_printf ( "#Double, take\n%s", ap[ i ].szName );
    aszColumnTitle[ 2 * i + 2 ] = 
      g_strdup_printf ( "#Double, pass\n%s", ap[ i ].szName );
  }

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( "Cube statistics" );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 5, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  for ( i = 0; i < 5; i++ )
    g_free ( aszColumnTitle[ i ] );
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 5; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  memset ( anTotal, 0, sizeof ( anTotal ) );
  
  for ( i = 0; i < STAT_MAXCUBE; i++ ) {

    sprintf ( aszRow[ 0 ], ( i < STAT_MAXCUBE - 1 ) ?
	      "%d-cube" : ">= %d-cube", 2 << i );
    sprintf ( aszRow[ 1 ], "%d", prs->acDoubleTake[ i ] );
    sprintf ( aszRow[ 2 ], "%d", prs->acDoubleDrop[ i ] );

    sprintf ( aszRow[ 3 ], "%d", (prs+1)->acDoubleTake[ i ] );
    sprintf ( aszRow[ 4 ], "%d", (prs+1)->acDoubleDrop[ i ] );

    anTotal[ 0 ] += prs->acDoubleTake[ i ];
    anTotal[ 1 ] += prs->acDoubleDrop[ i ];
    anTotal[ 2 ] += (prs+1)->acDoubleTake[ i ];
    anTotal[ 3 ] += (prs+1)->acDoubleDrop[ i ];

    gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  }


  sprintf ( aszRow[ 0 ], "Total" );
  for ( i = 0; i < 4; i++ )
    sprintf ( aszRow[ i + 1 ], "%d", anTotal[ i ] );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  sprintf ( aszRow[ 0 ], "%%" );
  for ( i = 0; i < 4; i++ )
    sprintf ( aszRow[ i + 1 ], "%6.2f%%",
	      100.0 * (float) anTotal[ i ] / (float) cGames );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );


  for ( i = 0; i < 5; i++ ) free ( aszRow[ i ] );

  if ( anTotal[ 1 ] + anTotal[ 0 ] ) {
    sprintf ( sz, "Cube efficiency for %s: %7.4f",
              ap[ 0 ].szName,
	      (float) anTotal[ 0 ] / ( anTotal[ 1 ] + anTotal[ 0 ] ) );
    pwLabel = gtk_label_new ( sz );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );
  }

  if ( anTotal[ 2 ] + anTotal[ 3 ] ) {
    sprintf ( sz, "Cube efficiency for p%s: %7.4f",
              ap[ 0 ].szName,
	      (float) anTotal[ 2 ] / ( anTotal[ 3 ] + anTotal[ 2 ] ) );
    pwLabel = gtk_label_new ( sz );
    gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );
  }

  return pw;

}

static GtkWidget *
GTKStatPageBearoff ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( "Bearoff statistics" );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );
    

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  
  sprintf ( aszRow[ 0 ], "Moves with bearoff" );
  sprintf ( aszRow[ 1 ], "%d", prs->nBearoffMoves );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nBearoffMoves );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], "Pips lost" );
  sprintf ( aszRow[ 1 ], "%d", prs->nBearoffPipsLost );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nBearoffPipsLost );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], "Average Pips lost" );

  if ( prs->nBearoffMoves )
    sprintf ( aszRow[ 1 ], "%7.2f",
  	    (float) prs->nBearoffPipsLost / prs->nBearoffMoves );
  else
    sprintf ( aszRow[ 1 ], "n/a" );

  if ( (prs+1)->nBearoffMoves )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      (float) (prs+1)->nBearoffPipsLost / (prs+1)->nBearoffMoves );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}


static GtkWidget *
GTKStatPageClosedOut ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( "Closed out statistics" );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  sprintf ( aszRow[ 0 ], "Number of close-outs" );
  sprintf ( aszRow[ 1 ], "%d", prs->nOpponentClosedOut );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nOpponentClosedOut );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], "Average move number for close out" );
  if ( prs->nOpponentClosedOut )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentClosedOutMove / prs->nOpponentClosedOut );
  else
    sprintf ( aszRow[ 1 ], "n/a" );


  if ( (prs+1)->nOpponentClosedOut )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      1.0f + (prs+1)->rOpponentClosedOutMove /
	      (prs+1)->nOpponentClosedOut );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}


static GtkWidget *
GTKStatPageHit ( const rolloutstat *prs, const int cGames ) {

  GtkWidget *pw;
  GtkWidget *pwLabel;
  GtkWidget *pwStat;

  static char *aszColumnTitle[ 3 ];

  static char *aszRow[ 3 ];
  int i;

  pw = gtk_vbox_new ( FALSE, 0 );

  pwLabel = gtk_label_new ( "Hit statistics" );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwLabel, FALSE, FALSE, 4 );

  /* column titles */

  aszColumnTitle[ 0 ] = strdup ( "" );
  for ( i = 0; i < 2; i++ )
    aszColumnTitle[ i + 1 ] = strdup ( ap[ i ].szName );

  /* table with results */

  pwStat = gtk_clist_new_with_titles( 3, aszColumnTitle );
  gtk_clist_column_titles_passive( GTK_CLIST( pwStat ) );

  /* garbage collect */

  for ( i = 0; i < 3; i++ )
    free ( aszColumnTitle[ i ] );

  /* content */
    
  gtk_box_pack_start( GTK_BOX( pw ), pwStat, TRUE, TRUE, 0 );
    
  for( i = 0; i < 3; i++ ) {
    gtk_clist_set_column_auto_resize( GTK_CLIST( pwStat ), i,
				      TRUE );
    gtk_clist_set_column_justification( GTK_CLIST( pwStat ), i,
					GTK_JUSTIFY_RIGHT );

    aszRow[ i ] = malloc ( 100 );
  }

  sprintf ( aszRow[ 0 ], "Number of games with hit(s)" );
  sprintf ( aszRow[ 1 ], "%d", prs->nOpponentHit );
  sprintf ( aszRow[ 2 ], "%d", (prs+1)->nOpponentHit );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], "Percent games with hits" );
  sprintf ( aszRow[ 1 ], "%7.2f%%", 
            100.0 * (prs)->nOpponentHit / ( 1.0 * cGames ) );
  sprintf ( aszRow[ 2 ], "%7.2f%%", 
            100.0 * (prs+1)->nOpponentHit / (1.0 * cGames ) );
  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  sprintf ( aszRow[ 0 ], "Average move number for first hit" );
  if ( prs->nOpponentHit )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentHitMove / prs->nOpponentHit );
  else
    sprintf ( aszRow[ 1 ], "n/a" );


  sprintf ( aszRow[ 0 ], "Average move number for first hit" );
  if ( prs->nOpponentHit )
    sprintf ( aszRow[ 1 ], "%7.2f",
	      1.0f + prs->rOpponentHitMove / (1.0 * prs->nOpponentHit ) );
  else
    sprintf ( aszRow[ 1 ], "n/a" );

  if ( (prs+1)->nOpponentHit )
    sprintf ( aszRow[ 2 ], "%7.2f",
	      1.0f + (prs+1)->rOpponentHitMove /
	      ( 1.0 * (prs+1)->nOpponentHit ) );
  else
    sprintf ( aszRow[ 2 ], "n/a" );


  gtk_clist_append ( GTK_CLIST ( pwStat ), aszRow );

  for ( i = 0; i < 2; i++ ) free ( aszRow[ i ] );

  return pw;

}




/*
 * 
 */

static GtkWidget *
GTKRolloutStatPage ( const rolloutstat *prs,
		     const int cGames ) {


  /* GTK Widgets */

  GtkWidget *pw;
  GtkWidget *pwWin, *pwCube, *pwHit, *pwBearoff, *pwClosedOut;

  GtkWidget *psw;

  /* Create notebook pages */

  pw = gtk_vbox_new ( FALSE, 0 );

  pwWin = GTKStatPageWin ( prs, cGames );
  pwCube = GTKStatPageCube ( prs, cGames );
  pwBearoff = GTKStatPageBearoff ( prs, cGames );
  pwHit =  GTKStatPageHit ( prs, cGames );
  pwClosedOut =  GTKStatPageClosedOut ( prs, cGames );

  /*
  pwNotebook = gtk_notebook_new ();

  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
			     pwWin,
			     gtk_label_new ( "Win" ) );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
			     pwCube,
			     gtk_label_new ( "Cube" ) );

  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
			     pwBearoff,
			     gtk_label_new ( "Bearoff" ) );
  
  gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
			     pwHit,
			     gtk_label_new ( "Hit" ) );

  gtk_box_pack_start( GTK_BOX( pw ), pwNotebook, FALSE, FALSE, 0 );
  */

  psw = gtk_scrolled_window_new ( NULL, NULL );

  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW ( psw ),
				   GTK_POLICY_NEVER,
				   GTK_POLICY_AUTOMATIC );

  gtk_box_pack_start ( GTK_BOX ( pw ), pwWin, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwCube, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwBearoff, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwClosedOut, FALSE, FALSE, 0 );
  gtk_box_pack_start ( GTK_BOX ( pw ), pwHit, FALSE, FALSE, 0 );

  gtk_scrolled_window_add_with_viewport (
      GTK_SCROLLED_WINDOW ( psw), pw);


  return psw;
  
}


extern void
GTKViewRolloutStatistics(GtkWidget *widget, gpointer data){ 

  /* Rollout statistics information */

  rolloutstat *prs = data;
  int cGames = gtk_progress_get_value ( GTK_PROGRESS( pwRolloutProgress ) );
  int nRollouts = GTK_CLIST( pwRolloutResult )->rows / 2;
  int i;

  /* GTK Widgets */

  GtkWidget *pwDialog;
  GtkWidget *pwNotebook;

  /* Temporary variables */

  char *sz;

  /* Create dialog */

  pwDialog = CreateDialog ( "Rollout statistics", FALSE,
			    NULL, NULL );

  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 0, 400 );

  /* Create notebook pages */

  pwNotebook = gtk_notebook_new ();

  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		     pwNotebook );

  for ( i = 0; i < nRollouts; i++ ) {

    gtk_clist_get_text ( GTK_CLIST ( pwRolloutResult ),
			      i * 2, 0, &sz );

    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
			       GTKRolloutStatPage ( &prs[ i * 2 ], 
						    cGames ), 
			       gtk_label_new ( sz ) );

  }


  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
		      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
  
  gtk_widget_show_all( pwDialog );

  gtk_main();


}


extern void GTKRolloutRow( int i ) {

    iRolloutRow = i;
}

extern int
GTKRolloutUpdate( float aarMu[][ NUM_ROLLOUT_OUTPUTS ],
                  float aarSigma[][ NUM_ROLLOUT_OUTPUTS ],
                  int iGame, int cGames, int fCubeful, int cRows,
                  cubeinfo aci[] ) {

    char sz[ 32 ];
    int i, j, iRow;

    for ( j = 0; j < cRows; j++ ) {

      iRow = j + iRolloutRow;

      if( !pwRolloutResult )
	return -1;
    
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {
        
        if ( i < OUTPUT_EQUITY )
          sprintf( sz, "%6.4f", aarMu[ j ][ i ] );
        else if ( i == OUTPUT_EQUITY ) {

          if ( ! ms.nMatchTo )
            /* money game */
            sprintf( sz, "%+7.4f", aarMu[ j ][ i ] );
          else if ( fOutputMWC )
            /* match play (mwc) */
            sprintf( sz, "%7.3f%%", 100.0f * eq2mwc ( aarMu[ j ][ i ], &aci[ j ] ) );
          else 
            /* match play (equity) */
            sprintf( sz, "%+7.4f",
                     mwc2eq ( eq2mwc ( aarMu[ j ][ i ], &aci[ j ] ), &aci[ 0 ] ) );

        }
        else {
          if ( fCubeful ) {
            if ( ! ms.nMatchTo ) 
              /* money game */
              sprintf( sz, "%+7.4f", aarMu[ j ][ i ] );
            else if ( fOutputMWC )
              /* match play (mwc) */
              sprintf( sz, "%7.3f%%", 100.0f * aarMu[ j ][ i ] );
            else
              /* match play (equity) */
              sprintf( sz, "%+7.4f", mwc2eq ( aarMu[ j ][ i ], &aci[ 0 ] ) );
          }
          else {
            strcpy ( sz, "n/a" );
          }
        }

	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), iRow << 1,
			    i + 1, sz );
	
        if ( i < OUTPUT_EQUITY )
          /* standard error on winning chance etc. */
          sprintf( sz, "%6.4f", aarSigma[ j ][ i ] );
        else if ( i == OUTPUT_EQUITY ) {

          /* standard error for equity */

          if ( ! ms.nMatchTo )
            /* money game */
            sprintf( sz, "%7.4f", aarSigma[ j ][ i ] );
          else if ( fOutputMWC )
            /* match play (mwc) */
            sprintf( sz, "%7.3f%%", 
                     100.0f * se_eq2mwc ( aarSigma[ j ][ i ], &aci[ j ] ) );
          else 
            /* match play (equity) */
            sprintf( sz, "%7.4f",
                     se_mwc2eq ( se_eq2mwc ( aarSigma[ j ][ i ], &aci[ j ] ), 
                              &aci[ 0 ] ) );

        }
        else if ( fCubeful ) {

          /* standard error in cubeful equity */

          if ( ! ms.nMatchTo ) 
            /* money game */
            sprintf( sz, "%+7.4f", aarSigma[ j ][ i ] );
          else if ( fOutputMWC )
            /* match play (mwc) */
            sprintf( sz, "%7.3f%%", 100.0f * aarSigma[ j ][ i ] );
          else
            /* match play (equity) */
            sprintf( sz, "%7.4f", se_mwc2eq ( aarSigma[ j ][ i ], 
                                              &aci[ 0 ] ) );

        }
        else
            strcpy ( sz, "n/a" );

	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), ( iRow << 1 )
			    | 1, i + 1, sz );
      }

    }
    
    gtk_progress_configure( GTK_PROGRESS( pwRolloutProgress ),
			    iGame + 1, 0, cGames );
    
    GTKDisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    GTKAllowStdin();

    return 0;
}

extern void GTKRolloutDone( void ) {

    /* if they cancelled the rollout early, pwRolloutDialog has already been
       destroyed */
    if( !pwRolloutDialog )
	return;
    
    gtk_progress_set_format_string( GTK_PROGRESS( pwRolloutProgress ),
				    "Finished (%v trials)" );
    
    gtk_signal_disconnect( GTK_OBJECT( pwRolloutDialog ), nRolloutSignal );
    gtk_signal_connect( GTK_OBJECT( pwRolloutDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();
    
    pwRolloutProgress = NULL;
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

    gtk_progress_set_value( GTK_PROGRESS ( pwProgress ), iValue );

    GTKDisallowStdin();
    while( gtk_events_pending() )
	gtk_main_iteration();
    GTKAllowStdin();    
}


extern void GTKProgress( void ) {

    static int i;

    gtk_progress_set_value( GTK_PROGRESS( pwProgress ), i ^= 1 );

    GTKDisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    GTKAllowStdin();    
}

extern void GTKProgressEnd( void ) {

    gtk_progress_set_activity_mode( GTK_PROGRESS( pwProgress ), FALSE );
    gtk_progress_set_value( GTK_PROGRESS( pwProgress ), 0 );
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ), " " );
    gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idProgress );
}

static GtkWidget 
*GTKWriteMET ( float aafMET[ MAXSCORE ][ MAXSCORE ],
               const int nRows, const int nCols, const int fInvert ) {

    int i, j;
    char sz[ 16 ];
    GtkWidget *pwScrolledWindow = gtk_scrolled_window_new( NULL, NULL );
#if HAVE_LIBGTKEXTRA
    GtkWidget *pwTable = gtk_sheet_new_browser( nRows, nCols, "" );
#else
    GtkWidget *pwTable = gtk_table_new( nRows + 1, nCols + 1, TRUE );
#endif
    GtkWidget *pwBox = gtk_vbox_new( FALSE, 0 );
    

    gtk_box_pack_start( GTK_BOX( pwBox ), gtk_label_new( miCurrent.szName ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwBox ),
                        gtk_label_new( miCurrent.szFileName ),
                        FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( pwBox ),
                        gtk_label_new( miCurrent.szDescription ),
                        FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( pwBox ), pwScrolledWindow, TRUE, TRUE, 0 );

#if HAVE_LIBGTKEXTRA
    gtk_container_add( GTK_CONTAINER( pwScrolledWindow ), pwTable );
#else
    gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW(
	pwScrolledWindow ), pwTable );
#endif

    /* header for rows */
    
    for( i = 0; i < nCols; i++ ) {
	sprintf( sz, "%d-away", i + 1 );
#if HAVE_LIBGTKEXTRA
	gtk_sheet_row_button_add_label( GTK_SHEET( pwTable ), i, sz );
#else
	gtk_table_attach_defaults( GTK_TABLE( pwTable ),
				   gtk_label_new( sz ),
				   i + 1, i + 2, 0, 1 );
#endif
    }

    /* header for columns */

    for( i = 0; i < nRows; i++ ) {
	sprintf( sz, "%d-away", i + 1 );
#if HAVE_LIBGTKEXTRA
	gtk_sheet_column_button_add_label( GTK_SHEET( pwTable ), i, sz );
#else
	gtk_table_attach_defaults( GTK_TABLE( pwTable ),
				   gtk_label_new( sz ),
				   0, 1, i + 1, i + 2 );
#endif

    }

    /* fill out table */
    
    for( i = 0; i < nRows; i++ )
	for( j = 0; j < nCols; j++ ) {

          if ( fInvert )
	    sprintf( sz, "%8.4f", GET_MET( j, i, aafMET ) * 100.0f );
          else
	    sprintf( sz, "%8.4f", GET_MET( i, j, aafMET ) * 100.0f );

#if HAVE_LIBGTKEXTRA
	    gtk_sheet_set_cell( GTK_SHEET( pwTable ), i, j, GTK_JUSTIFY_RIGHT,
				sz );
#else
	    gtk_table_attach_defaults( GTK_TABLE( pwTable ),
				       gtk_label_new( sz ),
				       j + 1, j + 2, i + 1, i + 2 );
#endif
	}

#if !HAVE_LIBGTKEXTRA
    gtk_table_set_col_spacings( GTK_TABLE( pwTable ), 4 );
#endif

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( pwScrolledWindow ),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC );

    return pwBox;

}

static void invertMETlocal( GtkWidget *pw, gpointer data){

    if(fInvertMET)
      UserCommand( "set invert met off" );
    else
      UserCommand( "set invert met on" );
}


extern void GTKShowMatchEquityTable( int n ) {

    int i;
    char sz[ 50 ];
    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Match equity table",
                                        FALSE, NULL, NULL );
    GtkWidget *pwNotebook = gtk_notebook_new ();
    
    GtkWidget *pwInvertButton = gtk_button_new_with_label("Invert table"); 
    
    gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                       pwNotebook );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwInvertButton );

    gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                               GTKWriteMET ( aafMET, n, n, FALSE ),
                               gtk_label_new ( "Pre-Crawford" ) );

    for ( i = 0; i < 2; i++ ) {
      
      sprintf ( sz, "Post-Crawford for player %s", ap[ i ].szName );
      
      gtk_notebook_append_page ( GTK_NOTEBOOK ( pwNotebook ),
                                 GTKWriteMET ( (float (*)[ MAXSCORE ])
                                               aafMETPostCrawford[ i ], 
                                               n, 1, TRUE ),
                                 gtk_label_new ( sz ) );

    }

    
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 500, 300 );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwInvertButton ), "clicked",
			GTK_SIGNAL_FUNC( invertMETlocal ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    GTKDisallowStdin();
    gtk_main();
    GTKAllowStdin();

}


static void DestroyAbout( gpointer p ) {

    *( (void **) p ) = NULL;
}

extern void GTKShowVersion( void ) {

    static GtkWidget *pwDialog;
    GtkWidget *pwBox = gtk_vbox_new( FALSE, 0 ),
	*pwHBox = gtk_hbox_new( FALSE, 8 ),
	*pwPrompt = gtk_label_new( "GNU Backgammon" ),
	*pwList = gtk_list_new(),
	*pwScrolled = gtk_scrolled_window_new( NULL, NULL ),
	*pwButton;
    GtkRcStyle *ps = gtk_rc_style_new();
    int i;
    static char *aszAuthors[] = { "Joseph Heled", "Øystein Johansen",
				  "David Montgomery",
				  "Jørn Thyssen", "Gary Wong" };
    extern char *aszCredits[];

    if( pwDialog )
	gtk_widget_destroy( pwDialog );
    
    pwDialog = CreateDialog( "About GNU Backgammon", FALSE,
			     NULL, NULL );
    gtk_object_weakref( GTK_OBJECT( pwDialog ), DestroyAbout, &pwDialog );

    gtk_container_set_border_width( GTK_CONTAINER( pwBox ), 4 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwBox );

    gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt, FALSE, FALSE, 0 );
#if GTK_CHECK_VERSION(1,3,10)
    ps->font_desc = pango_font_description_new();
    pango_font_description_set_family_static( ps->font_desc, "times" );
    pango_font_description_set_size( ps->font_desc, 64 * PANGO_SCALE );    
#else
    ps->font_name = g_strdup( "-*-times-medium-r-normal-*-64-*-*-*-p-*-"
			      "iso8859-1" );
#endif
    gtk_widget_modify_style( pwPrompt, ps );
    gtk_rc_style_unref( ps );
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( "version " VERSION ), FALSE, FALSE, 0 );

    ps = gtk_rc_style_new();
    gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt =
			gtk_label_new( "Copyright 1999, 2000, 2001, 2002 "
				       "Gary Wong" ), FALSE, FALSE, 4 );
#if GTK_CHECK_VERSION(1,3,10)
    ps->font_desc = pango_font_description_new();
    pango_font_description_set_family_static( ps->font_desc, "helvetica" );
    pango_font_description_set_size( ps->font_desc, 8 * PANGO_SCALE );    
#else
    ps->font_name = g_strdup( "-*-helvetica-medium-r-normal-*-8-*-*-*-p-*-"
			      "iso8859-1" );
#endif
    gtk_widget_modify_style( pwPrompt, ps );

    for( i = 1; aszVersion[ i ]; i++ ) {
	gtk_box_pack_start( GTK_BOX( pwBox ), pwPrompt =
			    gtk_label_new( aszVersion[ i ] ),
			    FALSE, FALSE, 0 );
	gtk_widget_modify_style( pwPrompt, ps );
    }
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( "GNU Backgammon was written by:" ),
			FALSE, FALSE, 8 );
    
    gtk_box_pack_start( GTK_BOX( pwBox ), pwHBox, FALSE, FALSE, 0 );

    for( i = 0; i < 5; i++ )
	gtk_container_add( GTK_CONTAINER( pwHBox ),
			   gtk_label_new( TRANS( aszAuthors[ i ] ) ) );
    
    gtk_box_pack_start( GTK_BOX( pwBox ),
			gtk_label_new( "With special thanks to:" ),
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
	"GNU Backgammon is free software, covered by the GNU General Public "
	"License version 2, and you are welcome to change it and/or "
	"distribute copies of it under certain conditions.  There is "
	"absolutely no warranty for GNU Backgammon." ), FALSE, FALSE, 4 );
    gtk_label_set_line_wrap( GTK_LABEL( pwPrompt ), TRUE );
    gtk_widget_modify_style( pwPrompt, ps );
    gtk_rc_style_unref( ps );

    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwButton = gtk_button_new_with_label(
			   "Copying conditions" ) );
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( CommandShowCopying ), NULL );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
		       pwButton = gtk_button_new_with_label(
			   "Warranty" ) );
    gtk_signal_connect( GTK_OBJECT( pwButton ), "clicked",
			GTK_SIGNAL_FUNC( CommandShowWarranty ), NULL );
    
    gtk_widget_show_all( pwDialog );
}

static void NoManual( void ) {
    
    outputl( "The online manual is not available with this installation of "
	     "GNU Backgammon.  You can view the manual on the WWW at:\n"
	     "\n"
	     "    http://www.gnu.org/manual/gnubg/" );

    outputx();
}

static void ShowManual( gpointer *p, guint n, GtkWidget *pwEvent ) {
#if HAVE_GTKTEXI
    static GtkWidget *pw;
    char *pch;
    
    if( pw ) {
	gtk_window_present( GTK_WINDOW( pw ) );
	return;
    }

    if( !( pch = PathSearch( "gnubg.xml", szDataDirectory ) ) ) {
	NoManual();
	return;
    } else if( access( pch, R_OK ) ) {
	perror( pch );
	free( pch );
	NoManual();
	return;
    }
    
    pw = gtk_texi_new();
    g_object_add_weak_pointer( G_OBJECT( pw ), (gpointer *) &pw );
    gtk_window_set_title( GTK_WINDOW( pw ), "GNU Backgammon - Manual" );
    gtk_window_set_default_size( GTK_WINDOW( pw ), 600, 400 );
    gtk_widget_show_all( pw );

    gtk_texi_load( GTK_TEXI( pw ), pch );
    free( pch );
    gtk_texi_render_node( GTK_TEXI( pw ), "Top" );
#else
    NoManual();
#endif
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

	pch = g_strdup_printf( "%s- %s\n\nUsage: %s%s\n", szCommand,
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
	gtk_window_set_title( GTK_WINDOW( pw ), "GNU Backgammon - Help" );
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
	pwDialog = CreateDialog( "GNU Backgammon", FALSE, NULL, NULL );
	gtk_window_set_wmclass( GTK_WINDOW( pwDialog ), "progress",
				"Progress" );
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
					"Generating bearoff database (%p%%)" );
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

static void UpdateToggle( gnubgcommand cmd, int *pf ) {

    GtkWidget *pw = gtk_item_factory_get_widget_by_action( pif, cmd );

    g_assert( GTK_IS_CHECK_MENU_ITEM( pw ) );

    fAutoCommand = TRUE;
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( pw ), *pf );
    fAutoCommand = FALSE;
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

    togglecommand *ptc;

    /* Handle normal `toggle' menu items. */
    for( ptc = atc; ptc->p; ptc++ )
	if( p == ptc->p ) {
	    UpdateToggle( ptc->cmd, p );
	    return;
	}

    if( p == &rngCurrent ) {
	/* Handle the RNG radio items. */
	fAutoCommand = TRUE;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_RNG_ANSI +
						   rngCurrent - RNG_ANSI ) ),
	    TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	    pif, "/Settings/Dice generation/Seed..." ),
				  rngCurrent != RNG_MANUAL );
	fAutoCommand = FALSE;
    } else if( p == ap ) {
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
	fAutoCommand = FALSE;
    } else if( p == &ms.gs ) {
	/* Handle the game state. */
	fAutoCommand = TRUE;

	board_set_playing( BOARD( pwBoard ), ms.gs == GAME_PLAYING );

	enable_sub_menu( gtk_item_factory_get_widget( pif, "/File/Save" ),
			 plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	    pif, "/File/Save/Weights..." ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget(
	    pif, "/File/Import/.pos position..." ),
				  ms.gs == GAME_PLAYING );
	enable_sub_menu( gtk_item_factory_get_widget( pif, "/File/Export" ),
			 plGame != NULL );
	
	enable_sub_menu( gtk_item_factory_get_widget( pif, "/Game" ),
			 ms.gs == GAME_PLAYING );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_ROLL ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_ROLL ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_NEXT_GAME ), !ListEmpty( &lMatch ) );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_PREV_GAME ), !ListEmpty( &lMatch ) );
	
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
	    pif, CMD_SHOW_STATISTICS_GAME ), plGame != NULL );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_STATISTICS_MATCH ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_STATISTICS_SESSION ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_MATCHEQUITYTABLE ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SHOW_ENGINE ), TRUE );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SWAP_PLAYERS ), !ListEmpty( &lMatch ) );
	
	fAutoCommand = FALSE;
    } else if( p == &ms.fCrawford )
	ShowBoard(); /* this is overkill, but it works */
    else if( p == &fAnnotation ) {
	if( fAnnotation ) {
          setWindowGeometry ( pwAnnotation, &wgAnnotation );
	    gtk_widget_show_all( pwAnnotation );
	    if( pwAnnotation->window )
		gdk_window_raise( pwAnnotation->window );
	} else {
	    /* FIXME actually we should unmap the window, and send a synthetic
	       UnmapNotify event to the window manager -- see the ICCCM */
          getWindowGeometry ( &wgAnnotation, pwAnnotation );
          gtk_widget_hide( pwAnnotation );
        }
    }
}

static void FormatStatEquity( char *sz, float ar[ 2 ], int nDenominator,
			      int nMatchTo ) {

    if( !nDenominator ) {
	strcpy( sz, "n/a" );
	return;
    }

    if( nMatchTo )
	sprintf( sz, "%+.3f (%+.3f%%)", ar[ 0 ] / nDenominator,
		 ar[ 1 ] * 100.0f / nDenominator );
    else
	sprintf( sz, "%+.3f (%+.3f)", ar[ 0 ] / nDenominator,
		 ar[ 1 ] / nDenominator );
}

static void FormatStatCubeError( char *sz, int n, float ar[ 2 ],
				 int nMatchTo ) {

    if( nMatchTo )
	sprintf( sz, "%d (%+.3f, %+.3f%%)", n, ar[ 0 ], ar[ 1 ] * 100.0f );
    else
	sprintf( sz, "%d (%+.3f, %+.3f)", n, ar[ 0 ], ar[ 1 ] );
}

extern void GTKDumpStatcontext( statcontext *psc, matchstate *pms,
				char *szTitle ) {
    
  static char *aszEmpty[] = { NULL, NULL, NULL };
  static char *aszLabels[] = { 
         "Checkerplay statistics:",
         "Total moves",
         "Unforced moves",
         "Moves marked very good",
         "Moves marked good",
         "Moves marked interesting",
         "Moves unmarked",
         "Moves marked doubtful",
         "Moves marked bad",
         "Moves marked very bad",
         "Error rate (total)",
         "Error rate (pr. move)",
         "Rolls marked very lucky",
         "Rolls marked lucky",
         "Rolls unmarked",
         "Rolls marked unlucky",
         "Rolls marked very unlucky",
         "Luck rate (total)",
         "Luck rate (pr. move)",
         "Checker play rating",
         "Cube decision statistics:",
         "Total cube decisions",
         "Doubles",
         "Takes",
         "Pass",
         "Missed doubles around DP",
         "Missed doubles around TG",
         "Wrong doubles around DP",
         "Wrong doubles around TG",
         "Wrong takes",
         "Wrong passes",
         "Cube decision rating",
         "Overall rating" };

  GtkWidget *pwDialog = CreateDialog( szTitle,
                                       FALSE, NULL, NULL ),
      *psw = gtk_scrolled_window_new( NULL, NULL ),
      *pwStats = gtk_clist_new_with_titles( 3, aszEmpty );
  int i;
  char sz[ 32 ];
  ratingtype rt[ 2 ];
#if WIN32
  char szOutput[4096];
    GtkWidget *pwButtons,
        *pwCopy = gtk_button_new_with_label( "Copy" );
#endif

#if WIN32
    /* FIXME There should be some way to extract the text on Unix as well */
    pwButtons = DialogArea( pwDialog, DA_BUTTONS );
    gtk_container_add( GTK_CONTAINER( pwButtons ), pwCopy );
    DumpStatcontext( szOutput, psc, szTitle);
    gtk_signal_connect( GTK_OBJECT( pwCopy ), "clicked",
			GTK_SIGNAL_FUNC( GTKWinCopy ), (gpointer) szOutput );
#endif

  for( i = 0; i < 3; i++ ) {
      gtk_clist_set_column_auto_resize( GTK_CLIST( pwStats ), i, TRUE );
      gtk_clist_set_column_justification( GTK_CLIST( pwStats ), i,
                                          GTK_JUSTIFY_RIGHT );
  }
  gtk_clist_column_titles_passive( GTK_CLIST( pwStats ) );
      
  gtk_clist_set_column_title( GTK_CLIST( pwStats ), 1, (ap[0].szName));
  gtk_clist_set_column_title( GTK_CLIST( pwStats ), 2, (ap[1].szName));

  for (i = 0; i < 33; i++) {
    gtk_clist_append( GTK_CLIST( pwStats ), aszEmpty );
    gtk_clist_set_text( GTK_CLIST( pwStats ), i, 0, aszLabels[i]);
  }
        
  sprintf(sz,"%d", psc->anTotalMoves[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 1, 1, sz);
  sprintf(sz,"%d", psc->anTotalMoves[ 1 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 1, 2, sz);
  sprintf(sz,"%d", psc->anUnforcedMoves[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 2, 1, sz);
  sprintf(sz,"%d", psc->anUnforcedMoves[ 1 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 2, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_VERYGOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 3, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_VERYGOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 3, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_GOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 4, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_GOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 4, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_INTERESTING ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 5, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_INTERESTING ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 5, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_NONE ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 6, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_NONE ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 6, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_DOUBTFUL ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 7, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_DOUBTFUL ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 7, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_BAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 8, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_BAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 8, 2, sz);
  sprintf(sz,"%d", psc->anMoves[ 0 ][ SKILL_VERYBAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 9, 1, sz);
  sprintf(sz,"%d", psc->anMoves[ 1 ][ SKILL_VERYBAD ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 9, 2, sz);

  FormatStatEquity( sz, psc->arErrorCheckerplay[ 0 ], 1, pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 10, 1, sz);
  FormatStatEquity( sz, psc->arErrorCheckerplay[ 1 ], 1, pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 10, 2, sz);
  
  FormatStatEquity( sz, psc->arErrorCheckerplay[ 0 ],
		    psc->anUnforcedMoves[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 11, 1, sz);
  FormatStatEquity( sz, psc->arErrorCheckerplay[ 1 ],
		    psc->anUnforcedMoves[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 11, 2, sz);
  
  sprintf(sz, "%d", psc->anLuck[ 0 ][ LUCK_VERYGOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 12, 1, sz);
  sprintf(sz, "%d", psc->anLuck[ 1 ][ LUCK_VERYGOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 12, 2, sz);
  sprintf(sz, "%d", psc->anLuck[ 0 ][ LUCK_GOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 13, 1, sz);
  sprintf(sz, "%d", psc->anLuck[ 1 ][ LUCK_GOOD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 13, 2, sz);
  sprintf(sz, "%d", psc->anLuck[ 0 ][ LUCK_NONE ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 14, 1, sz);
  sprintf(sz, "%d", psc->anLuck[ 1 ][ LUCK_NONE ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 14, 2, sz);
  sprintf(sz, "%d", psc->anLuck[ 0 ][ LUCK_BAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 15, 1, sz);
  sprintf(sz, "%d", psc->anLuck[ 1 ][ LUCK_BAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 15, 2, sz);
  sprintf(sz, "%d", psc->anLuck[ 0 ][ LUCK_VERYBAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 16, 1, sz);
  sprintf(sz, "%d", psc->anLuck[ 1 ][ LUCK_VERYBAD ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 16, 2, sz);

  FormatStatEquity( sz, psc->arLuck[ 0 ], 1, pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 17, 1, sz);
  FormatStatEquity( sz, psc->arLuck[ 1 ], 1, pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 17, 2, sz);
  
  FormatStatEquity( sz, psc->arLuck[ 0 ], psc->anTotalMoves[ 0 ],
		    pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 18, 1, sz);
  FormatStatEquity( sz, psc->arLuck[ 1 ], psc->anTotalMoves[ 1 ],
		    pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 18, 2, sz);
  
  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( psc->arErrorCheckerplay[ i ][ 0 ] /
                          psc->anUnforcedMoves[ i ] );

  sprintf ( sz, "%-15s", aszRating[ rt [ 0 ] ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 19, 1, sz);
  sprintf ( sz, "%-15s", aszRating[ rt [ 1 ] ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 19, 2, sz);

  sprintf(sz,"%d", psc->anTotalCube[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 21, 1, sz);
  sprintf(sz,"%d", psc->anTotalCube[ 1 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 21, 2, sz);
  sprintf(sz,"%d", psc->anDouble[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 22, 1, sz);
  sprintf(sz,"%d", psc->anDouble[ 1 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 22, 2, sz);
  sprintf(sz,"%d", psc->anTake[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 23, 1, sz);
  sprintf(sz,"%d", psc->anTake[ 1 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 23, 2, sz);
  sprintf(sz,"%d", psc->anPass[ 0 ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 24, 1, sz);
  sprintf(sz,"%d", psc->anPass[ 1 ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 24, 2, sz);

  FormatStatCubeError( sz, psc->anCubeMissedDoubleDP[ 0 ],
		       psc->arErrorMissedDoubleDP[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 25, 1, sz);
  FormatStatCubeError( sz, psc->anCubeMissedDoubleDP[ 1 ],
		       psc->arErrorMissedDoubleDP[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 25, 2, sz);
  
  FormatStatCubeError( sz, psc->anCubeMissedDoubleTG[ 0 ],
		       psc->arErrorMissedDoubleTG[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 26, 1, sz);
  FormatStatCubeError( sz, psc->anCubeMissedDoubleTG[ 1 ],
		       psc->arErrorMissedDoubleTG[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 26, 2, sz);
  
  FormatStatCubeError( sz, psc->anCubeWrongDoubleDP[ 0 ],
		       psc->arErrorWrongDoubleDP[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 27, 1, sz);
  FormatStatCubeError( sz, psc->anCubeWrongDoubleDP[ 1 ],
		       psc->arErrorWrongDoubleDP[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 27, 2, sz);
  
  FormatStatCubeError( sz, psc->anCubeWrongDoubleTG[ 0 ],
		       psc->arErrorWrongDoubleTG[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 28, 1, sz);
  FormatStatCubeError( sz, psc->anCubeWrongDoubleTG[ 1 ],
		       psc->arErrorWrongDoubleTG[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 28, 2, sz);
  
  FormatStatCubeError( sz, psc->anCubeWrongTake[ 0 ],
		       psc->arErrorWrongTake[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 29, 1, sz);
  FormatStatCubeError( sz, psc->anCubeWrongTake[ 1 ],
		       psc->arErrorWrongTake[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 29, 2, sz);
  
  FormatStatCubeError( sz, psc->anCubeWrongPass[ 0 ],
		       psc->arErrorWrongPass[ 0 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 30, 1, sz);
  FormatStatCubeError( sz, psc->anCubeWrongPass[ 1 ],
		       psc->arErrorWrongPass[ 1 ], pms->nMatchTo );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 30, 2, sz);
  
  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                            + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                            + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongTake[ i ][ 0 ]
                            + psc->arErrorWrongPass[ i ][ 0 ] ) /
                          psc->anTotalCube[ i ] );

  sprintf ( sz, "%-15s", aszRating[ rt [ 0 ] ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 31, 1, sz);
  sprintf ( sz, "%-15s", aszRating[ rt [ 1 ] ] );
  gtk_clist_set_text( GTK_CLIST( pwStats ), 31, 2, sz);

  for ( i = 0 ; i < 2; i++ )
    rt[ i ] = GetRating ( ( psc->arErrorMissedDoubleDP[ i ][ 0 ]
                            + psc->arErrorMissedDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongDoubleDP[ i ][ 0 ]
                            + psc->arErrorWrongDoubleTG[ i ][ 0 ]
                            + psc->arErrorWrongTake[ i ][ 0 ]
                            + psc->arErrorWrongPass[ i ][ 0 ] 
                            + psc->arErrorCheckerplay[ i ][ 0 ] ) /
                          ( psc->anTotalCube[ i ] +
                            psc->anUnforcedMoves[ i ] ) );

  sprintf ( sz, "%-15s", aszRating[ rt [ 0 ] ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 32, 1, sz);
  sprintf ( sz, "%-15s", aszRating[ rt [ 1 ] ]);
  gtk_clist_set_text( GTK_CLIST( pwStats ), 32, 2, sz);

  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
  gtk_container_add( GTK_CONTAINER( psw ), pwStats );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), psw );

  gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 0, 300 );
  gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
                                GTK_WINDOW( pwMain ) );

  gtk_widget_show_all( pwDialog );

  gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
                      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();
} 

static GtkWidget* CreateSetCubeDialog ( int *pfOK )
{

  GtkWidget *SetCubeWindow, *hbox1, *frame1, *vbox1, *vbox2,
    *pwRBValue[7], *pwRBOwner[3];
  GSList *value_group = NULL, *owner_group = NULL;
  int i, nCubeTurns;

  for( nCubeTurns = 0; ms.nCube >> ( nCubeTurns + 1 ); nCubeTurns++ )
       ;

  *pfOK = FALSE;
  SetCubeWindow = CreateDialog( "GNU Backgammon - Cube", TRUE, NULL, pfOK );
  gtk_object_set_data (GTK_OBJECT (SetCubeWindow), "SetCubeWindow",
                         SetCubeWindow);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox1);
  gtk_container_add (GTK_CONTAINER (DialogArea( SetCubeWindow, DA_MAIN )),
		     hbox1);

  frame1 = gtk_frame_new ("Value");
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (hbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 3);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_container_add (GTK_CONTAINER (frame1), vbox1);

  for (i = 0; i < 7; i++){
    char szNum[3];

    sprintf(szNum, "%d", 1 << i);
    pwRBValue[i] = gtk_radio_button_new_with_label (value_group, szNum);
    value_group = gtk_radio_button_group (GTK_RADIO_BUTTON (pwRBValue[i]));
    gtk_box_pack_start (GTK_BOX (vbox1), pwRBValue[i], FALSE, FALSE, 0);
  }

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (hbox1), vbox1, TRUE, TRUE, 0);

  frame1 = gtk_frame_new ("Owner");
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox1), frame1, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 3);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (frame1), vbox2);

  pwRBOwner[0] = gtk_radio_button_new_with_label (owner_group, "Centred");
  owner_group = gtk_radio_button_group (GTK_RADIO_BUTTON (pwRBOwner[0]));
  gtk_widget_show (pwRBOwner[0]);
  gtk_box_pack_start (GTK_BOX (vbox2), pwRBOwner[0], FALSE, FALSE, 0);

  pwRBOwner[1] = gtk_radio_button_new_with_label (owner_group, ap[0].szName);
  owner_group = gtk_radio_button_group (GTK_RADIO_BUTTON (pwRBOwner[1]));
  gtk_widget_show (pwRBOwner[1]);
  gtk_box_pack_start (GTK_BOX (vbox2), pwRBOwner[1], FALSE, FALSE, 0);

  pwRBOwner[2] = gtk_radio_button_new_with_label (owner_group, ap[1].szName);
  owner_group = gtk_radio_button_group (GTK_RADIO_BUTTON (pwRBOwner[2]));
  gtk_widget_show (pwRBOwner[2]);
  gtk_box_pack_start (GTK_BOX (vbox2), pwRBOwner[2], FALSE, FALSE, 0);

  for (i = 0; i < 7 ; i++) {
    gtk_signal_connect (GTK_OBJECT (pwRBValue[i]), "clicked",
                        GTK_SIGNAL_FUNC (SetCubeValue),
               (unsigned int *) (1 << i) );
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON( pwRBValue[i] ),
                  ( i == nCubeTurns) );
  }

  for (i = -1; i < 2; i++){
    gtk_signal_connect (GTK_OBJECT (pwRBOwner[i+1]), "clicked",
                        GTK_SIGNAL_FUNC (SetCubeOwner), (int *) i);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON( pwRBOwner[i+1] ),
                        ( i == ms.fCubeOwner ) );
  }

  return SetCubeWindow;
}

extern void GTKSetCube( gpointer *p, guint n, GtkWidget *pw ) {

  int fOK;
    
  pwSetCube = CreateSetCubeDialog( &fOK );

  gtk_window_set_modal( GTK_WINDOW( pwSetCube ), TRUE );
  gtk_window_set_transient_for( GTK_WINDOW( pwSetCube ),
				GTK_WINDOW( pwMain ) );
  gtk_signal_connect( GTK_OBJECT( pwSetCube ), "destroy",
		      GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
  gtk_widget_show_all( pwSetCube );

  GTKDisallowStdin();
  gtk_main();
  GTKAllowStdin();

  if( fOK ) {
      UserCommand(szSetCubeValue);
      UserCommand(szSetCubeOwner);
  }
}

static void SetCubeValue( GtkWidget *wd, int data) {

  sprintf(szSetCubeValue, "set cube value %d", data);

}

static void SetCubeOwner( GtkWidget *wd, int i) {

  if ( i == -1 ) 
    sprintf(szSetCubeOwner,"set cube centre");
  else
    sprintf(szSetCubeOwner,"set cube owner %d", i);

}


typedef struct _pathdata {

  GtkWidget *apwPath[ PATH_MET + 1 ];

} pathdata;


static void
SetPathAsDefault ( GtkWidget *pw, pathdata *ppd ) {
  
  int *pi;

  pi = gtk_object_get_data ( GTK_OBJECT ( pw ), "user_data" );

  gtk_label_set_text ( GTK_LABEL ( ppd->apwPath[ *pi ] ), 
                       aaszPaths[ *pi ][ 1 ] );

}


static void
ModifyPath ( GtkWidget *pw, pathdata *ppd ) {

  /*
    
    int *pi;
    
    pi = gtk_object_get_data ( GTK_OBJECT ( pw ), "user_data" );

    FIXME: implement SelectPath 

    gtk_label_set_text ( GTK_LABEL ( ppd->apwPath[ *pi ] ), 
    pc = SelectPath ( "Select Path", 
    aaszPaths[ *pi ][ 0 ] ) );
  */

  Message ( "NOT IMPLEMENTED!\n"
            "Use the \"set path\" command instead!", FALSE );

}


static void 
SetPath ( GtkWidget *pw, pathdata *ppd, int fOK ) {

  int i;
  gchar *pc;

  for ( i = 0; i <= PATH_MET; i++ ) {
    gtk_label_get ( GTK_LABEL ( ppd->apwPath[ i ] ), &pc );
    strcpy ( aaszPaths[ i ][ 0 ], pc );
  }


  if( fOK )
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );

}


static void 
PathOK ( GtkWidget *pw, void *p ) {
  SetPath ( pw, p, TRUE );
}


static void 
PathApply ( GtkWidget *pw, void *p ) {
  SetPath ( pw, p, FALSE );
}


extern void
GTKShowPath ( void ) {

  GtkWidget *pwDialog;
  GtkWidget *pwApply;
  GtkWidget *pwVBox;
  GtkWidget *pw;
  GtkWidget *pwHBox;
  GtkWidget *pwNotebook;

  pathdata pd;

  int i;
  int *pi;

  char *aaszPathNames[][ PATH_MET + 1 ] = {
    { "Export of Encapsulated PostScript .eps files" , 
      "Encapsulated PostScript" },
    { "Import or export of Jellyfish .gam files" , 
      "Jellyfish .gam" },
    { "Export of HTML files" , 
      "HTML" },
    { "Export of LaTeX files" , 
      "LaTeX" },
    { "Import or export of Jellyfish .mat files" , 
      "Jellyfish .mat" },
    { "Import of FIBS oldmoves files" , 
      "FIBS oldmoves" },
    { "Export of PDF files" , 
      "PDF" },
    { "Import of Jellyfish .pos files" , 
      "Jellyfish .pos" },
    { "Export of PostScript files" , 
      "PostScript" },
    { "Load and save of SGF files" , 
      "SGF (gnubg)" },
    { "Import of GamesGrid SGG files" , 
      "GamesGrid SGG" },
    { "Loading of match equity files (.xml)", 
      "Match Equity Tables" } };

  
  pwDialog = CreateDialog( "GNU Backgammon - Paths", TRUE, 
                           GTK_SIGNAL_FUNC ( PathOK ), &pd );
    
  pwApply = gtk_button_new_with_label( "Apply" );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_BUTTONS ) ),
                     pwApply );
  gtk_signal_connect( GTK_OBJECT( pwApply ), "clicked",
                      GTK_SIGNAL_FUNC( PathApply ), &pd );

  gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
                     pwNotebook = gtk_notebook_new() );
  gtk_notebook_set_scrollable( GTK_NOTEBOOK( pwNotebook ), TRUE );
  gtk_container_set_border_width( GTK_CONTAINER( pwNotebook ), 4 );

  /* content of widget */

  for ( i = 0; i <= PATH_MET; i++ ) {

    pwVBox = gtk_vbox_new ( FALSE, 4 );

    pw = gtk_label_new ( aaszPathNames[ i ][ 0 ] );
    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pw, FALSE, TRUE, 4 );

    /* default */
    
    pi = malloc ( sizeof ( int ) );
    *pi = i;

    pwHBox = gtk_hbox_new ( FALSE, 4 );

    pw = gtk_label_new ( "Default: " );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = pd.apwPath[ i ] = gtk_label_new ( aaszPaths[ i ][ 0 ] );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_button_new_with_label ( "Modify..." );
    gtk_box_pack_end ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );
    gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                         GTK_SIGNAL_FUNC ( ModifyPath ),
                         &pd );
    gtk_object_set_data_full( GTK_OBJECT( pw ), "user_data", pi, free );

    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pwHBox, FALSE, TRUE, 4 );

    /* current */

    pi = malloc ( sizeof ( int ) );
    *pi = i;

    pwHBox = gtk_hbox_new ( FALSE, 4 );

    pw = gtk_label_new ( "Current: " );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_label_new ( aaszPaths[ i ][ 1 ] );
    gtk_misc_set_alignment( GTK_MISC( pw ), 0, 0.5 );
    gtk_box_pack_start ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    pw = gtk_button_new_with_label ( "Set as default" );
    gtk_box_pack_end ( GTK_BOX ( pwHBox ), pw, FALSE, TRUE, 4 );

    gtk_signal_connect ( GTK_OBJECT ( pw ), "clicked",
                         GTK_SIGNAL_FUNC ( SetPathAsDefault ),
                         &pd );
    gtk_object_set_data_full( GTK_OBJECT( pw ), "user_data", pi, free );


    gtk_box_pack_start ( GTK_BOX ( pwVBox ), pwHBox, FALSE, TRUE, 4 );

    /* add page */

    gtk_notebook_append_page( GTK_NOTEBOOK( pwNotebook ),
                              pwVBox,
			      gtk_label_new( aaszPathNames[ i ][ 1 ] ) );

  }


  /* signals, modality, etc */

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
