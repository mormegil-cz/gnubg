/*
 * gtkgame.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000.
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "backgammon.h"
#include "dice.h"
#include "drawboard.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtkprefs.h"
#include "positionid.h"

/* Enumeration to be used as index to the table of command strings below
   (since GTK will only let us put integers into a GtkItemFactoryEntry,
   and that might not be big enough to hold a pointer).  Must be kept in
   sync with the string array! */
typedef enum _gnubgcommand {
    CMD_AGREE,
    CMD_DATABASE_DUMP,
    CMD_DATABASE_GENERATE,
    CMD_DATABASE_ROLLOUT,
    CMD_DECLINE,
    CMD_DOUBLE,
    CMD_DROP,
    CMD_EVAL,
    CMD_HELP,
    CMD_HINT,
    CMD_NEW_GAME,
    CMD_NEW_SESSION,
    CMD_PLAY,
    CMD_QUIT,
    CMD_REDOUBLE,
    CMD_RESIGN_N,
    CMD_RESIGN_G,
    CMD_RESIGN_B,
    CMD_ROLL,
    CMD_ROLLOUT,
    CMD_SAVE_GAME,
    CMD_SAVE_MATCH,
    CMD_SAVE_SETTINGS,
    CMD_SAVE_WEIGHTS,
    CMD_SET_AUTO_BEAROFF,
    CMD_SET_AUTO_CRAWFORD,
    CMD_SET_AUTO_GAME,
    CMD_SET_AUTO_MOVE,
    CMD_SET_AUTO_ROLL,
    CMD_SET_COLOURS,
    CMD_SET_CONFIRM,
    CMD_SET_CUBE_CENTRE,
    CMD_SET_CUBE_OWNER_0,
    CMD_SET_CUBE_OWNER_1,
    CMD_SET_CUBE_USE,
    CMD_SET_CUBE_VALUE_1,
    CMD_SET_CUBE_VALUE_2,
    CMD_SET_CUBE_VALUE_4,
    CMD_SET_CUBE_VALUE_8,
    CMD_SET_CUBE_VALUE_16,
    CMD_SET_CUBE_VALUE_32,
    CMD_SET_CUBE_VALUE_64,
    CMD_SET_DISPLAY,
    CMD_SET_JACOBY,
    CMD_SET_NACKGAMMON,
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
    CMD_SHOW_KLEINMAN,
    CMD_SHOW_PIPCOUNT,
    CMD_SHOW_THORP,
    CMD_SHOW_WARRANTY,
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
    { &fAutoBearoff, CMD_SET_AUTO_BEAROFF },
    { &fAutoCrawford, CMD_SET_AUTO_CRAWFORD },
    { &fAutoGame, CMD_SET_AUTO_GAME },
    { &fAutoMove, CMD_SET_AUTO_MOVE },
    { &fAutoRoll, CMD_SET_AUTO_ROLL },
    { &fConfirm, CMD_SET_CONFIRM },
    { &fCubeUse, CMD_SET_CUBE_USE },
    { &fDisplay, CMD_SET_DISPLAY },
    { &fJacoby, CMD_SET_JACOBY },
    { &fNackgammon, CMD_SET_NACKGAMMON },
    { NULL }
};

static char *aszCommands[ NUM_CMDS ] = {
    "agree",
    "database dump",
    "database generate",
    "database rollout",
    "decline",
    "double",
    "drop",
    "eval",
    "help",
    "hint",
    "new game",
    "new session",
    "play",
    "quit",
    "redouble",
    "resign normal",
    "resign gammon",
    "resign backgammon",
    "roll",
    "rollout",
    "save game",
    "save match",
    "save settings",
    "save weights",
    "set automatic bearoff",
    "set automatic crawford",
    "set automatic game",
    "set automatic move",
    "set automatic roll",
    NULL, /* set colours */
    "set confirm",
    "set cube centre",
    NULL, /* set cube owner 0 */
    NULL, /* set cube owner 1 */
    "set cube use",
    "set cube value 1",
    "set cube value 2",
    "set cube value 4",
    "set cube value 8",
    "set cube value 16",
    "set cube value 32",
    "set cube value 64",
    "set display",
    "set jacoby",
    "set nackgammon",
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
    "show kleinman",
    "show pipcount",
    "show thorp",
    "show warranty",
    "take",
    "train database",
    "train td"
};

static void NewMatch( gpointer *p, guint n, GtkWidget *pw );
static void LoadGame( gpointer *p, guint n, GtkWidget *pw );
static void LoadMatch( gpointer *p, guint n, GtkWidget *pw );
static void SaveGame( gpointer *p, guint n, GtkWidget *pw );
static void SaveMatch( gpointer *p, guint n, GtkWidget *pw );

/* A dummy widget that can grab events when others shouldn't see them. */
static GtkWidget *pwGrab;

GtkWidget *pwBoard, *pwMain;
static GtkWidget *pwStatus, *pwGame, *pwGameList, *pom;
static GtkAccelGroup *pagMain;
static GtkStyle *psGameList, *psCurrent;
static int yCurrent, xCurrent; /* highlighted row/col in game record */
static GtkItemFactory *pif;
guint nNextTurn = 0; /* GTK idle function */
static int cchOutput;
static guint idOutput;
static list lOutput;
int fTTY = TRUE;
static guint nStdin, nDisabledCount = 1;

void StdinReadNotify( gpointer p, gint h, GdkInputCondition cond ) {
    
#if HAVE_LIBREADLINE
    /* Handle "next turn" processing before more input (otherwise we might
       not even have a readline handler installed!) */
    while( nNextTurn )
	NextTurnNotify( NULL );
    
    rl_callback_read_char();
#else
    char sz[ 2048 ], *pch;

    while( nNextTurn )
	NextTurnNotify( NULL );
    
    sz[ 0 ] = 0;
	
    fgets( sz, sizeof( sz ), stdin );

    if( ( pch = strchr( sz, '\n' ) ) )
	*pch = 0;
    
	
    if( feof( stdin ) ) {
	PromptForExit();
	return 0;
    }	

    fInterrupt = FALSE;

    HandleCommand( sz, acTop );

    ResetInterrupt();

    if( nNextTurn )
	fNeedPrompt = TRUE;
    else
	Prompt();
#endif
}

void AllowStdin( void ) {

    if( !fTTY || !nDisabledCount )
	return;
    
    if( !--nDisabledCount )
	nStdin = gtk_input_add_full( STDIN_FILENO, GDK_INPUT_READ,
				     StdinReadNotify, NULL, NULL, NULL );
}

void DisallowStdin( void ) {

    if( !fTTY )
	return;
    
    nDisabledCount++;
    
    if( nStdin ) {
	gtk_input_remove( nStdin );
	nStdin = 0;
    }
}

extern void HandleXAction( void ) {
    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    /* Grab events so that the board window knows this is a re-entrant
       call, and won't allow commands like roll, move or double. */
    if( !gtk_grab_get_current() )
	gtk_grab_add( pwGrab );

    /* Don't check stdin here; readline isn't ready yet. */
    DisallowStdin();
    
    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( gtk_events_pending() )
	gtk_main_iteration();

    AllowStdin();

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
    case CMD_SET_COLOURS:
	BoardPreferences( pwBoard );
	return;
	
    case CMD_SET_CUBE_OWNER_0:
    case CMD_SET_CUBE_OWNER_1:
	sprintf( sz, "set cube owner %s",
		 ap[ iCommand - CMD_SET_CUBE_OWNER_0 ].szName );
	UserCommand( sz );
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

    DisallowStdin();
    gtk_main();
    AllowStdin();

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
   move, 1 = player 1's first move, 2 = player 0's first move, etc.).
   This function maps based on what's in the list window, so "combined"
   rows (e.g. "set board") will occupy TWO entries in the sequence space. */
static moverecord *GameListLookupMove( int i ) {

    gamelistrow *pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ),
						i >> 1 );

    if( !pglr )
	return NULL;

    return pglr->apmr[ pglr->fCombined ? 0 : i & 1 ];
}

static void GameListSelectRow( GtkCList *pcl, gint y, gint x,
			       GdkEventButton *pev, gpointer p ) {
    gamelistrow *pglr;
    moverecord *pmr, *pmrPrev;
    list *pl;
    int i, iPrev;
    
    if( x < 1 || x > 2 )
	return;

    pglr = gtk_clist_get_row_data( pcl, y );
    i = ( pglr && pglr->fCombined ) ? y << 1 : ( y << 1 ) | ( x - 1 );
    pmr = GameListLookupMove( i );
    
    for( iPrev = i - 1; iPrev >= 0; iPrev-- )
	if( ( pmrPrev = GameListLookupMove( iPrev ) ) )
	    break;

    if( !pmr && !pmrPrev )
	return;

    for( pl = plGame->plPrev; pl != plGame; pl = pl->plPrev ) {
	assert( pl->p );
	if( pl->p == pmr && pmr->mt == MOVE_SETDICE )
	    break;
	if( pl->p == pmrPrev ) {
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
    
    SetMoveRecord( pmr );
    
    CalculateBoard();

    UpdateSetting( &nCube );
    UpdateSetting( &fCubeOwner );
    UpdateSetting( &fTurn );

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

static void CreateGameWindow( void ) {

    static char *asz[] = { "Move", NULL, NULL };
    GtkWidget *psw = gtk_scrolled_window_new( NULL, NULL ),
	*pvbox = gtk_vbox_new( FALSE, 0 ),
	*phbox = gtk_hbox_new( FALSE, 0 ),
	*pm = gtk_menu_new();
    GtkStyle *ps;
    GdkColormap *pcmap;
    
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
    gtk_window_set_default_size( GTK_WINDOW( pwGame ), 0, 400 );
    
    gtk_container_add( GTK_CONTAINER( pwGame ), pvbox );

    gtk_box_pack_start( GTK_BOX( pvbox ), phbox, FALSE, FALSE, 4 );

    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, prevgame_xpm, "previous game" ),
			FALSE, FALSE, 4 );
    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, prevmove_xpm, "previous" ),
			FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( phbox ),
			PixmapButton( pcmap, nextmove_xpm, "next" ),
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
    
    gtk_container_add( GTK_CONTAINER( pvbox ), psw );
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
				    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
    
    gtk_container_add( GTK_CONTAINER( psw ),
		       pwGameList = gtk_clist_new_with_titles( 3, asz ) );
    gtk_clist_set_selection_mode( GTK_CLIST( pwGameList ),
				  GTK_SELECTION_BROWSE );
    gtk_clist_column_titles_passive( GTK_CLIST( pwGameList ) );
    gtk_clist_set_column_justification( GTK_CLIST( pwGameList ), 0,
					GTK_JUSTIFY_RIGHT );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 0, 40 );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 1, 200 );
    gtk_clist_set_column_width( GTK_CLIST( pwGameList ), 2, 200 );
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
    gdk_font_ref( ps->font = pwGameList->style->font );
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
    
    gtk_signal_connect( GTK_OBJECT( pwGameList ), "select-row",
			GTK_SIGNAL_FUNC( GameListSelectRow ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwGame ), "delete_event",
			GTK_SIGNAL_FUNC( gtk_widget_hide ), NULL );
    /* FIXME gtk_widget_hide is no good -- we want a function that returns
       TRUE to avoid running the default handler */
    /* FIXME actually we should unmap the window, and send a synthetic
       UnmapNotify event to the window manager -- see the ICCCM */
}

extern void ShowGameWindow( void ) {

    gtk_widget_show_all( pwGame );
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
	FormatMove( sz + 4, anBoard, pmr->n.anMove );
	break;

    case MOVE_DOUBLE:
	fPlayer = pmr->d.fPlayer;
	pch = " Double"; /* FIXME show value */
	break;
	
    case MOVE_TAKE:
	fPlayer = pmr->t.fPlayer;
	pch = " Take";
	break;
	
    case MOVE_DROP:
	fPlayer = pmr->t.fPlayer;
	pch = " Drop";
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
	->fCombined || pglr->apmr[ 1 ] || ( !fPlayer && pglr->apmr[ 0 ] ) )
	i = AddMoveRecordRow();
    else
	i = GTK_CLIST( pwGameList )->rows - 1;
    
    pglr = gtk_clist_get_row_data( GTK_CLIST( pwGameList ), i );

    if( ( pglr->fCombined = fPlayer == -1 ) )
	fPlayer = 0;

    pglr->apmr[ fPlayer ] = pmr;
    
    gtk_clist_set_text( GTK_CLIST( pwGameList ), i, fPlayer + 1, pch );
}

extern void GTKPopMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr;
    
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

extern void GTKSetMoveRecord( moverecord *pmr ) {

    GtkCList *pcl = GTK_CLIST( pwGameList );
    gamelistrow *pglr;
    int i;
    
    gtk_clist_set_cell_style( pcl, yCurrent, xCurrent, psGameList );

    yCurrent = xCurrent = -1;

    if( !pmr )
	return;
    
    if( pmr == plGame->plNext->p ) {
	yCurrent = 0;
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
	
	if( yCurrent >= 0 && pmr->mt != MOVE_SETDICE ) {
	    if( ++xCurrent > 2 ) {
		xCurrent = 1;
		yCurrent++;
	    }
	    
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

extern void GTKAddGame( char *sz ) {

    GtkWidget *pw = gtk_menu_item_new_with_label( sz ),
	*pwMenu = gtk_option_menu_get_menu( GTK_OPTION_MENU( pom ) );
    
    if( !cGames )
	/* Delete the "(no game)" item. */
	gtk_container_foreach( GTK_CONTAINER( pwMenu ),
			       (GtkCallback) MenuDelete, pwMenu );

    gtk_signal_connect( GTK_OBJECT( pw ), "activate",
			GTK_SIGNAL_FUNC( SelectGame ),
			GINT_TO_POINTER( cGames ) );
    
    gtk_widget_show( pw );
    gtk_menu_append( GTK_MENU( pwMenu ), pw );
    
    GTKSetGame( cGames );
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

static gboolean main_delete( GtkWidget *pw ) {
    
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

extern int InitGTK( int *argc, char ***argv ) {
    
    GtkWidget *pwVbox;
    static GtkItemFactoryEntry aife[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New/_Game", "<control>N", Command, CMD_NEW_GAME, NULL },
	{ "/_File/_New/_Match...", NULL, NewMatch, 0, NULL },
	{ "/_File/_New/_Session", NULL, Command, CMD_NEW_SESSION, NULL },
	{ "/_File/_Open", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Open/_Game...", NULL, LoadGame, 0, NULL },
	{ "/_File/_Open/_Match...", NULL, LoadMatch, 0, NULL },
	{ "/_File/_Open/_Session...", NULL, NULL, 0, NULL },
	{ "/_File/_Save", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Save/_Game...", NULL, SaveGame, 0, NULL },
	{ "/_File/_Save/_Match...", NULL, SaveMatch, 0, NULL },
	{ "/_File/_Save/_Session...", NULL, NULL, 0, NULL },
	{ "/_File/_Save/_Weights", NULL, Command, CMD_SAVE_WEIGHTS, NULL },
	{ "/_File/-", NULL, NULL, 0, "<Separator>" },
	{ "/_File/_Quit", "<control>Q", Command, CMD_QUIT, NULL },
	{ "/_Edit", NULL, NULL, 0, "<Branch>" },
	{ "/_Edit/_Undo", NULL, NULL, 0, NULL },
	{ "/_Edit/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Edit/_Copy", "<control>C", NULL, 0, NULL },
	{ "/_Edit/_Paste", "<control>V", NULL, 0, NULL },
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
	{ "/_Game/_Cube", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/_Cube/_Owner", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/_Cube/_Owner/Centred", NULL, Command,
	  CMD_SET_CUBE_CENTRE, "<RadioItem>" },
	{ "/_Game/_Cube/_Owner/0", NULL, Command, CMD_SET_CUBE_OWNER_0,
	  "/Game/Cube/Owner/Centred" },
	{ "/_Game/_Cube/_Owner/1", NULL, Command, CMD_SET_CUBE_OWNER_1,
	  "/Game/Cube/Owner/Centred" },
	{ "/_Game/_Cube/_Use", NULL, Command, CMD_SET_CUBE_USE,
	  "<CheckItem>" },
	{ "/_Game/_Cube/_Value", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/_Cube/_Value/_1", NULL, Command, CMD_SET_CUBE_VALUE_1,
	  "<RadioItem>" },
	{ "/_Game/_Cube/_Value/_2", NULL, Command, CMD_SET_CUBE_VALUE_2,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Cube/_Value/_4", NULL, Command, CMD_SET_CUBE_VALUE_4,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Cube/_Value/_8", NULL, Command, CMD_SET_CUBE_VALUE_8,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Cube/_Value/16", NULL, Command, CMD_SET_CUBE_VALUE_16,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Cube/_Value/32", NULL, Command, CMD_SET_CUBE_VALUE_32,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Cube/_Value/64", NULL, Command, CMD_SET_CUBE_VALUE_64,
	  "/Game/Cube/Value/1" },
	{ "/_Game/_Dice...", NULL, SetDice, 0, NULL },
	{ "/_Game/_Score...", NULL, NULL, 0, NULL },
	{ "/_Game/_Seed...", NULL, NULL, 0, NULL },
	{ "/_Game/_Turn", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/_Turn/0", NULL, Command, CMD_SET_TURN_0, "<RadioItem>" },
	{ "/_Game/_Turn/1", NULL, Command, CMD_SET_TURN_1,
	  "/Game/Turn/0" },
	{ "/_Analyse", NULL, NULL, 0, "<Branch>" },
	{ "/_Analyse/_Evaluate", "<control>E", Command, CMD_EVAL, NULL },
	{ "/_Analyse/_Hint", "<control>H", Command, CMD_HINT, NULL },
	{ "/_Analyse/_Rollout", NULL, Command, CMD_ROLLOUT, NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/_Pip count", NULL, Command, CMD_SHOW_PIPCOUNT, NULL },
	{ "/_Analyse/_Kleinman count", NULL, Command, CMD_SHOW_KLEINMAN,
	  NULL },
	{ "/_Analyse/_Thorp count", NULL, Command, CMD_SHOW_THORP, NULL },
	{ "/_Train", NULL, NULL, 0, "<Branch>" },
	{ "/_Train/_Dump database", NULL, Command, CMD_DATABASE_DUMP, NULL },
	{ "/_Train/_Generate database", NULL, Command, CMD_DATABASE_GENERATE,
	  NULL },
	{ "/_Train/_Rollout database", NULL, Command, CMD_DATABASE_ROLLOUT,
	  NULL },
	{ "/_Train/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Train/Train from _database", NULL, Command, CMD_TRAIN_DATABASE,
	  NULL },
	{ "/_Train/Train with _TD(0)", NULL, Command, CMD_TRAIN_TD, NULL },
	{ "/_Settings", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Automatic", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Automatic/_Bearoff", NULL, Command,
	  CMD_SET_AUTO_BEAROFF, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Crawford", NULL, Command,
	  CMD_SET_AUTO_CRAWFORD, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Game", NULL, Command, CMD_SET_AUTO_GAME,
	  "<CheckItem>" },
	{ "/_Settings/_Automatic/_Move", NULL, Command, CMD_SET_AUTO_MOVE,
	  "<CheckItem>" },
	{ "/_Settings/_Automatic/_Roll", NULL, Command, CMD_SET_AUTO_ROLL,
	  "<CheckItem>" },
	{ "/_Settings/Cache...", NULL, NULL, 0, NULL },
	{ "/_Settings/Colours...", NULL, Command, CMD_SET_COLOURS, NULL },
	{ "/_Settings/_Confirmation", NULL, Command, CMD_SET_CONFIRM,
	  "<CheckItem>" },
	{ "/_Settings/De_lay...", NULL, NULL, 0, NULL },
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
	{ "/_Settings/Di_splay", NULL, Command, CMD_SET_DISPLAY,
	  "<CheckItem>" },
	{ "/_Settings/_Evaluation...", NULL, NULL, 0, NULL },
	{ "/_Settings/_Jacoby", NULL, Command, CMD_SET_JACOBY, "<CheckItem>" },
	{ "/_Settings/_Nackgammon", NULL, Command, CMD_SET_NACKGAMMON,
	  "<CheckItem>" },
	{ "/_Settings/_Players...", NULL, NULL, 0, NULL },
	{ "/_Settings/Prompt...", NULL, NULL, 0, NULL },
	{ "/_Settings/_Rollouts...", NULL, NULL, 0, NULL },
	{ "/_Settings/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Settings/Save settings", NULL, Command, CMD_SAVE_SETTINGS, NULL },
	{ "/_Help", NULL, NULL, 0, "<Branch>" },
	{ "/_Help/_Commands...", NULL, Command, CMD_HELP, NULL },
	{ "/_Help/Co_pying gnubg...", NULL, Command, CMD_SHOW_COPYING, NULL },
	{ "/_Help/gnubg _Warranty...", NULL, Command, CMD_SHOW_WARRANTY,
	  NULL },
	{ "/_Help/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Help/_About...", NULL, NULL, 0, NULL }
    };

    if( !gtk_init_check( argc, argv ) )
	return FALSE;
    
    fnAction = HandleXAction;

    gdk_rgb_init();
    gdk_rgb_set_min_colors( 2 * 2 * 2 );
    gtk_widget_set_default_colormap( gdk_rgb_get_cmap() );
    gtk_widget_set_default_visual( gdk_rgb_get_visual() );

    pwMain = gtk_window_new( GTK_WINDOW_TOPLEVEL );
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
			gtk_item_factory_get_widget( pif, "<main>" ),
			FALSE, FALSE, 0 );
		       
    gtk_container_add( GTK_CONTAINER( pwVbox ), pwBoard = board_new() );
    pwGrab = ( (BoardData *) BOARD( pwBoard )->board_data )->stop;
    
    gtk_box_pack_end( GTK_BOX( pwVbox ), pwStatus = gtk_statusbar_new(),
		      FALSE, FALSE, 0 );
    idOutput = gtk_statusbar_get_context_id( GTK_STATUSBAR( pwStatus ),
					     "gnubg output" );
    gtk_signal_connect( GTK_OBJECT( pwStatus ), "text-popped",
			GTK_SIGNAL_FUNC( TextPopped ), NULL );
    
    /* Make sure the window is reasonably big, but will fit on a 640x480
       screen. */
    gtk_window_set_default_size( GTK_WINDOW( pwMain ), 500, 465 );
    
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    CreateGameWindow();
    gtk_window_add_accel_group( GTK_WINDOW( pwGame ), pagMain );
    
    ListCreate( &lOutput );

    return TRUE;
}

extern void RunGTK( void ) {

    int n;
    togglecommand *ptc;
    
    /* Ensure all menu item settings are initialised to the correct state. */
    for( ptc = atc; ptc->p; ptc++ )
	GTKSet( ptc->p );
    GTKSet( &rngCurrent );
    GTKSet( &fCubeOwner );
    GTKSet( &nCube );
    GTKSet( ap );
    GTKSet( &fTurn );
    
    ShowBoard();

    AllowStdin();
    
    if( fTTY ) {
#ifdef ConnectionNumber /* FIXME use configure somehow to detect this */
	if( ( n = fcntl( ConnectionNumber( GDK_DISPLAY() ),
			 F_GETFL ) ) != -1 ) {
	    /* FIXME F_SETOWN is a BSDism... use SIOCSPGRP if necessary. */
	    fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETOWN, getpid() );
	    fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETFL, n | FASYNC );
	}
#endif
    
#if HAVE_LIBREADLINE
	fReadingCommand = TRUE;
	rl_callback_handler_install( FormatPrompt(), HandleInput );
	atexit( rl_callback_handler_remove );
#else
	Prompt();
#endif
    }
    
    gtk_widget_show_all( pwMain );
    
    gtk_main();
}

extern void ShowList( char *psz[], char *szTitle ) {

    GtkWidget *pwDialog = gtk_dialog_new(), *pwList = gtk_list_new(),
	*pwScroll = gtk_scrolled_window_new( NULL, NULL ),
	*pwOK = gtk_button_new_with_label( "OK" );

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

    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
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
	*pwCancel,
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
	pwCancel = gtk_button_new_with_label( "Cancel" );
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwCancel );
	gtk_signal_connect_object( GTK_OBJECT( pwCancel ), "clicked",
				   GTK_SIGNAL_FUNC( gtk_widget_destroy ),
				   GTK_OBJECT( pwDialog ) );
    }
    
    gtk_accel_group_attach( pag, GTK_OBJECT( pwDialog ) );
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
	    
    DisallowStdin();
    gtk_main();
    AllowStdin();

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

    fFinishedPopping = FALSE;
    
    do
	gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idOutput );
    while( !fFinishedPopping );
}

static GtkWidget *pwLength;

static void NewMatchOK( GtkWidget *pw, int *pf ) {

    *pf = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON( pwLength ) );
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static void NewMatch( gpointer *p, guint n, GtkWidget *pw ) {

    int nLength = 0;
    GtkObject *pa = gtk_adjustment_new( 1, 1, 99, 1, 1, 1 );
    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - New Match", TRUE,
					GTK_SIGNAL_FUNC( NewMatchOK ),
					&nLength ),
	*pwPrompt = gtk_label_new( "Match length:" );
    
    pwLength = gtk_spin_button_new( GTK_ADJUSTMENT( pa ), 1, 0 );
    gtk_spin_button_set_numeric( GTK_SPIN_BUTTON( pwLength ), TRUE );
    
    gtk_misc_set_padding( GTK_MISC( pwPrompt ), 8, 8 );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwPrompt );
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwLength );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    DisallowStdin();
    gtk_main();
    AllowStdin();

    if( nLength ) {
	char sz[ 32 ];

	sprintf( sz, "new match %d", nLength );
	UserCommand( sz );
    }
}

static void FileOK( GtkWidget *pw, char **ppch ) {

    GtkWidget *pwFile = gtk_widget_get_toplevel( pw );

    *ppch = g_strdup( gtk_file_selection_get_filename(
	GTK_FILE_SELECTION( pwFile ) ) );
    gtk_widget_destroy( pwFile );
}

static char *SelectFile( char *szTitle ) {

    char *pch = NULL;
    GtkWidget *pw = gtk_file_selection_new( szTitle );

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

    DisallowStdin();
    gtk_main();
    AllowStdin();

    return pch;
}

static void LoadGame( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch;
    
    if( ( pch = SelectFile( "Open game" ) ) ) {
	CommandLoadGame( pch );
	g_free( pch );
    }
}

static void LoadMatch( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch;
    
    if( ( pch = SelectFile( "Open match" ) ) ) {
	CommandLoadMatch( pch );
	g_free( pch );
    }
}

static void SaveGame( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch;
    
    if( gs == GAME_NONE ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }
    
    if( ( pch = SelectFile( "Save game" ) ) ) {
	CommandSaveGame( pch );
	g_free( pch );
    }
}

static void SaveMatch( gpointer *p, guint n, GtkWidget *pw ) {

    char *pch;
    
    if( gs == GAME_NONE ) {
	outputl( "No game in progress (type `new game' to start one)." );

	return;
    }

    /* FIXME what if nMatch == 0? */
    
    if( ( pch = SelectFile( "Save match" ) ) ) {
	CommandSaveMatch( pch );
	g_free( pch );
    }
}

extern void GTKEval( char *szOutput ) {

    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Evaluation",
					FALSE, NULL, NULL ),
	*pwText = gtk_text_new( NULL, NULL );
    GdkFont *pf;

    pf = gdk_font_load( "fixed" );
    
    gtk_text_set_editable( GTK_TEXT( pwText ), FALSE );
    gtk_text_insert( GTK_TEXT( pwText ), pf, NULL, NULL, szOutput, -1 );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ),
		       pwText );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 500, 600 );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    gtk_widget_show_all( pwDialog );

    DisallowStdin();
    gtk_main();
    AllowStdin();    
}

static void HintMove( GtkWidget *pw, GtkWidget *pwMoves ) {

    move *pm;
    char move[ 40 ];
    int i;
    
    assert( GTK_CLIST( pwMoves )->selection );

    i = GPOINTER_TO_INT( GTK_CLIST( pwMoves )->selection->data );
    pm = gtk_clist_get_row_data( GTK_CLIST( pwMoves ), i );

    FormatMove( move, anBoard, pm->anMove );
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

typedef struct _hintdata {
    GtkWidget *pwMove, *pwRollout;
    movelist *pml;
} hintdata;

static void HintSelect( GtkWidget *pw, int y, int x, GdkEventButton *peb,
			hintdata *phd ) {

    int c;
    GList *pl;
    
    for( c = 0, pl = GTK_CLIST( pw )->selection; c < 2 && pl; pl = pl->next )
	c++;

    if( c && peb )
	gtk_selection_owner_set( pw, GDK_SELECTION_PRIMARY, peb->time );

    gtk_widget_set_sensitive( phd->pwMove, c == 1 );
    gtk_widget_set_sensitive( phd->pwRollout, c );

    /* Double clicking a row makes that move. */
    if( c == 1 && peb && peb->type == GDK_2BUTTON_PRESS )
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
	FormatMoveHint( pch, phd->pml, an[ i ] );
        
    gtk_selection_data_set( psd, GDK_SELECTION_TYPE_STRING, 8,
			    sz, strlen( sz ) );
    
#if !HAVE_ALLOCA
    free( an );
    free( sz );
#endif        
}

extern void GTKHint( movelist *pml ) {

    static char *aszTitle[] = {
	"Rank", "Type", "Win", "W g", "W bg", "Lose", "L g", "L bg",
	"", "Diff.", "Move"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		       NULL, NULL };
    static int aanColumns[][ 2 ] = {
	{ 2, OUTPUT_WIN },
	{ 3, OUTPUT_WINGAMMON },
	{ 4, OUTPUT_WINBACKGAMMON },
	{ 6, OUTPUT_LOSEGAMMON },
	{ 7, OUTPUT_LOSEBACKGAMMON }
    };
    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL,
					NULL ),
	*psw = gtk_scrolled_window_new( NULL, NULL ),
	*pwButtons = DialogArea( pwDialog, DA_BUTTONS ),
	*pwMove = gtk_button_new_with_label( "Move" ),
	*pwRollout = gtk_button_new_with_label( "Rollout" ),
	*pwMoves = gtk_clist_new_with_titles( 11, aszTitle );
    int i, j;
    char sz[ 32 ];
    hintdata hd = { pwMove, pwRollout, pml };
    float rBest;
    cubeinfo ci;
    
    SetCubeInfo ( &ci, nCube, fCubeOwner, fMove, nMatchTo, anScore,
		  fCrawford, fJacoby, fBeavers );
    
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

    if( fOutputMWC && nMatchTo ) {
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

	sprintf( sz, "%d", i + 1 );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 0, sz );

	FormatEval( sz, pml->amMoves[ i ].etMove, pml->amMoves[ i ].esMove );
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

	if( fOutputMWC && nMatchTo )
	    sprintf( sz, "%7.3f%%", 100.0f * eq2mwc( pml->amMoves[ i ].rScore,
						     &ci ) );
	else
	    sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 8, sz );

	if( i ) {
	    if( fOutputMWC && nMatchTo )
		sprintf( sz, "%7.3f%%", eq2mwc( pml->amMoves[ i ].rScore, &ci )
			 * 100.0f - rBest );
	    else
		sprintf( sz, "%6.3f", pml->amMoves[ i ].rScore - rBest );
	    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 9, sz );
	}
	
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 10,
			    FormatMove( sz, anBoard,
					pml->amMoves[ i ].anMove ) );
    }

    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( psw ),
				    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
    gtk_container_add( GTK_CONTAINER( psw ), pwMoves );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwDialog, DA_MAIN ) ), psw );

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
    
    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 0, 300 );
    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    
    gtk_widget_show_all( pwDialog );
    
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    DisallowStdin();
    gtk_main();
    AllowStdin();
}

static GtkWidget *pwRolloutDialog, *pwRolloutResult, *pwProgress;
static int iRolloutRow;
static guint nRolloutSignal;

static void RolloutCancel( GtkObject *po, gpointer p ) {

    pwRolloutDialog = pwRolloutResult = pwProgress = NULL;
    fInterrupt = TRUE;
}

extern void GTKRollout( int c, char asz[][ 40 ], int cGames ) {
    
    static char *aszTitle[] = {
	"", "Win", "Win (g)", "Win (bg)", "Lose (g)", "Lose (bg)", "Equity"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    int i;
    GtkWidget *pwVbox;
    
    pwRolloutDialog = CreateDialog( "GNU Backgammon - Rollout", FALSE, NULL,
				    NULL );

    nRolloutSignal = gtk_signal_connect( GTK_OBJECT( pwRolloutDialog ),
					 "destroy",
					 GTK_SIGNAL_FUNC( RolloutCancel ),
					 NULL );

    pwVbox = gtk_vbox_new( FALSE, 4 );
	
    pwRolloutResult = gtk_clist_new_with_titles( 7, aszTitle );
    gtk_clist_column_titles_passive( GTK_CLIST( pwRolloutResult ) );
    
    pwProgress = gtk_progress_bar_new();

    gtk_box_pack_start( GTK_BOX( pwVbox ), pwRolloutResult, TRUE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX( pwVbox ), pwProgress, FALSE, FALSE, 0 );
    
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

    gtk_progress_configure( GTK_PROGRESS( pwProgress ), 0, 0, cGames );
    gtk_progress_set_show_text( GTK_PROGRESS( pwProgress ), TRUE );
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ),
				    "%v/%u (%p%%)" );
    
    gtk_container_add( GTK_CONTAINER( DialogArea( pwRolloutDialog, DA_MAIN ) ),
		       pwVbox );

    gtk_window_set_modal( GTK_WINDOW( pwRolloutDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwRolloutDialog ),
				  GTK_WINDOW( pwMain ) );
    
    gtk_widget_show_all( pwRolloutDialog );

    DisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    AllowStdin();
}

extern void GTKRolloutRow( int i ) {

    iRolloutRow = i;
}

extern int GTKRolloutUpdate( float arMu[], float arSigma[], int iGame,
			      int cGames ) {
    char sz[ 32 ];
    int i;

    if( !pwRolloutResult )
	return -1;
    
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {
	sprintf( sz, i == OUTPUT_EQUITY ? "%+6.3f" : "%5.3f", arMu[ i ] );
	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), iRolloutRow << 1,
			    i + 1, sz );
	
	sprintf( sz, "%5.3f", arSigma[ i ] );
	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), ( iRolloutRow << 1 )
			    | 1, i + 1, sz );
    }
    
    gtk_progress_configure( GTK_PROGRESS( pwProgress ), iGame + 1, 0, cGames );
    
    DisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    AllowStdin();

    return 0;
}

extern void GTKRolloutDone( void ) {

    /* if they cancelled the rollout early, pwRolloutDialog has already been
       destroyed */
    if( !pwRolloutDialog )
	return;
    
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ),
				    "Finished (%v trials)" );
    
    gtk_signal_disconnect( GTK_OBJECT( pwRolloutDialog ), nRolloutSignal );
    gtk_signal_connect( GTK_OBJECT( pwRolloutDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    DisallowStdin();
    gtk_main();
    AllowStdin();
}

static void UpdateToggle( gnubgcommand cmd, int *pf ) {

    GtkWidget *pw = gtk_item_factory_get_widget_by_action( pif, cmd );

    g_assert( GTK_IS_CHECK_MENU_ITEM( pw ) );

    fAutoCommand = TRUE;
    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM( pw ), *pf );
    fAutoCommand = FALSE;
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
	fAutoCommand = FALSE;
    } else if( p == &fCubeOwner ) {
	/* Handle the cube owner radio items. */
	fAutoCommand = TRUE;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_CUBE_OWNER_0 +
		fCubeOwner ) ), TRUE );
	fAutoCommand = FALSE;
    } else if( p == &nCube ) {
	/* Handle the cube value radio items. */
	int i;

	/* Find log_2 of the cube value. */
	for( i = 0; nCube >> ( i + 1 ); i++ )
	    ;
	
	fAutoCommand = TRUE;
	gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_CUBE_VALUE_1 +
		i) ), TRUE );
	fAutoCommand = FALSE;
    } else if( p == ap ) {
	/* Handle the player names. */
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_CUBE_OWNER_0 )
	    )->child ), ap[ 0 ].szName );
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_CUBE_OWNER_1 )
	    )->child ), ap[ 1 ].szName );
	
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_0 )
	    )->child ), ap[ 0 ].szName );
	gtk_label_set_text( GTK_LABEL( GTK_BIN(
	    gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_1 )
	    )->child ), ap[ 1 ].szName );

	gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 1,
				    ap[ 0 ].szName );
	gtk_clist_set_column_title( GTK_CLIST( pwGameList ), 2,
				    ap[ 1 ].szName );
    } else if( p == &fTurn ) {
	/* Handle the player on roll. */
	fAutoCommand = TRUE;

	/* FIXME it would be nicer to set the "/Game/Turn" menu item
	   insensitive, but GTK doesn't do that very well. */
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SET_TURN_0 ), fTurn >= 0 );
	gtk_widget_set_sensitive( gtk_item_factory_get_widget_by_action(
	    pif, CMD_SET_TURN_1 ), fTurn >= 0 );

	/* FIXME enable/disable other menu items which are only valid when a
	   game is in progress */
	
	if( fTurn >= 0 )
	    gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM(
		gtk_item_factory_get_widget_by_action( pif, CMD_SET_TURN_0 +
						       fTurn ) ), TRUE );
	fAutoCommand = FALSE;
    } else if( p == &fCrawford )
	ShowBoard(); /* this is overkill, but it works */
}
