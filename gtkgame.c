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

#include <assert.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <gtk/gtk.h>
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

/* A dummy widget that can grab events when others shouldn't see them. */
static GtkWidget *pwGrab;

GtkWidget *pwBoard;
static GtkWidget *pwStatus, *pwMain, *pwGame, *pwGameList;
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
    rl_callback_read_char();
#else
    char sz[ 2048 ], *pch;

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

typedef struct _gamelistrow {
    moverecord *apmr[ 2 ]; /* moverecord entries for each column */
    int fCombined; /* this message's row is combined across both columns */
} gamelistrow;

static void GameListSelectRow( GtkCList *pcl, gint y, gint x,
			       GdkEventButton *pev, gpointer p ) {
    gamelistrow *pglr;
    moverecord *pmr;
    list *pl;
    
    if( x < 1 || x > 2 || !( pglr = gtk_clist_get_row_data( pcl, y ) ) ||
	!( pmr = pglr->apmr[ x - 1 ] ) )
	return;

    if( pmr->mt == MOVE_SETDICE )
	/* For "set dice" records, we want to set plLastMove _there_... */
	for( pl = plGame->plNext; pl->p != pmr; pl = pl->plNext )
	    assert( pl->p );
    else
	/* ...for everything else, we want the move _before_. */
	for( pl = plGame->plNext; pl->plNext->p != pmr; pl = pl->plNext )
	    assert( pl->plNext->p );

    plLastMove = pl;
    pmr = pl->p;
    
    SetMoveRecord( pmr );
    
    CalculateBoard();

    UpdateSetting( &nCube );
    UpdateSetting( &fCubeOwner );
    UpdateSetting( &fTurn );

    ShowBoard();
}

static void CreateGameWindow( void ) {

    static char *asz[] = { "Move", NULL, NULL };
    GtkWidget *phbox = gtk_hbox_new( FALSE, 0 ),
	*psb = gtk_vscrollbar_new( NULL );
    GtkStyle *ps;
    
    pwGame = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title( GTK_WINDOW( pwGame ), "GNU Backgammon - "
			  "Game record" );
    gtk_window_set_wmclass( GTK_WINDOW( pwGame ), "gamerecord",
			    "GameRecord" );
    gtk_window_set_default_size( GTK_WINDOW( pwGame ), 0, 400 );

    gtk_container_add( GTK_CONTAINER( pwGame ), phbox );

    gtk_container_add( GTK_CONTAINER( phbox ),
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
    
    gtk_clist_set_vadjustment( GTK_CLIST( pwGameList ),
			       gtk_range_get_adjustment( GTK_RANGE( psb ) ) );

    gtk_container_add( GTK_CONTAINER( phbox ), psb );
    
    gtk_signal_connect( GTK_OBJECT( pwGameList ), "select-row",
			GTK_SIGNAL_FUNC( GameListSelectRow ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwGame ), "delete_event",
			GTK_SIGNAL_FUNC( gtk_widget_hide ), NULL );
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

    default:
	fPlayer = -1;
	pch = "FIXME";
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

    gtk_clist_set_cell_style( pcl, yCurrent, xCurrent, psCurrent );
    gtk_clist_moveto( pcl, yCurrent, xCurrent, 0.5, 0.5 );
}

extern void GTKClearMoveRecord( void ) {

    gtk_clist_clear( GTK_CLIST( pwGameList ) );
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
    GtkAccelGroup *pag;
    static GtkItemFactoryEntry aife[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New/_Game", NULL, Command, CMD_NEW_GAME, NULL },
	{ "/_File/_New/_Match...", NULL, NewMatch, 0, NULL },
	{ "/_File/_New/_Session", NULL, Command, CMD_NEW_SESSION, NULL },
	{ "/_File/_Open", NULL, NULL, 0, NULL },
	{ "/_File/_Save", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_Save/_Game", NULL, Command, CMD_SAVE_GAME, NULL },
	{ "/_File/_Save/_Match", NULL, Command, CMD_SAVE_MATCH, NULL },
	{ "/_File/_Save/_Session", NULL, NULL, 0, NULL },
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
	{ "/_Game/_Take", NULL, Command, CMD_TAKE, NULL },
	{ "/_Game/Dro_p", NULL, Command, CMD_DROP, NULL },
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
	{ "/_Game/_Dice...", NULL, NULL, 0, NULL },
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
	{ "/_Help/_Commands", NULL, Command, CMD_HELP, NULL },
	{ "/_Help/Co_pying gnubg", NULL, Command, CMD_SHOW_COPYING, NULL },
	{ "/_Help/gnubg _Warranty", NULL, Command, CMD_SHOW_WARRANTY, NULL },
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

    pwGrab = gtk_widget_new( GTK_TYPE_WIDGET, NULL );
    
    pwMain = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title( GTK_WINDOW( pwMain ), "GNU Backgammon" );
    gtk_container_add( GTK_CONTAINER( pwMain ),
		       pwVbox = gtk_vbox_new( FALSE, 0 ) );

    pag = gtk_accel_group_new();
    pif = gtk_item_factory_new( GTK_TYPE_MENU_BAR, "<main>", pag );
    gtk_item_factory_create_items( pif, sizeof( aife ) / sizeof( aife[ 0 ] ),
				   aife, NULL );
    gtk_window_add_accel_group( GTK_WINDOW( pwMain ), pag );
    gtk_box_pack_start( GTK_BOX( pwVbox ),
			gtk_item_factory_get_widget( pif, "<main>" ),
			FALSE, FALSE, 0 );
		       
    gtk_container_add( GTK_CONTAINER( pwVbox ), pwBoard = board_new() );

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

static GtkWidget *CreateDialog( char *szTitle, int fQuestion, GtkSignalFunc pf,
				void *p ) {

#include "gnu.xpm"
#include "question.xpm"

    GdkPixmap *ppm;
    GtkStyle *ps;
    GtkWidget *pwDialog = gtk_dialog_new(),
	*pwOK = gtk_button_new_with_label( "OK" ),
	*pwCancel,
	*pwHbox = gtk_hbox_new( FALSE, 0 ),
	*pwButtons = gtk_hbutton_box_new(),
	*pwPixmap;

    /* realise the dialog immediately, so the colourmap is available for the
       pixmap */
    gtk_widget_realize( pwDialog );
    
    ps = gtk_widget_get_style( pwDialog );
    ppm = gdk_pixmap_create_from_xpm_d( pwDialog->window, NULL,
					&ps->bg[ GTK_STATE_NORMAL ],
					fQuestion ? question_xpm : gnu_xpm );
    pwPixmap = gtk_pixmap_new( ppm, NULL );
    gtk_misc_set_padding( GTK_MISC( pwPixmap ), 8, 8 );
    
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
    
    gtk_window_set_title( GTK_WINDOW( pwDialog ), szTitle );

    GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( pwOK );

    return pwDialog;
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
    gtk_container_add( GTK_CONTAINER( gtk_container_children( GTK_CONTAINER(
	GTK_DIALOG( pwDialog )->vbox ) )->data ), pwPrompt );

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
    gtk_container_add( GTK_CONTAINER( gtk_container_children( GTK_CONTAINER(
	GTK_DIALOG( pwDialog )->vbox ) )->data ), pwPrompt );
    gtk_container_add( GTK_CONTAINER( gtk_container_children( GTK_CONTAINER(
	GTK_DIALOG( pwDialog )->vbox ) )->data ), pwLength );

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

extern void GTKHint( movelist *pml ) {

    static char *aszTitle[] = {
	"Win", "Win (g)", "Win (bg)", "Lose (g)", "Lose (bg)", "Equity",
	"Move"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL,
					NULL ),
	*pwMoves = gtk_clist_new_with_titles( 7, aszTitle );
    int i, j;
    char sz[ 32 ];

    for( i = 0; i < 7; i++ ) {
	gtk_clist_set_column_auto_resize( GTK_CLIST( pwMoves ), i, TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( pwMoves ), i,
					    i == 6 ? GTK_JUSTIFY_LEFT :
					    GTK_JUSTIFY_RIGHT );
    }
    gtk_clist_column_titles_passive( GTK_CLIST( pwMoves ) );
    
    for( i = 0; i < pml->cMoves; i++ ) {
	float *ar = pml->amMoves[ i ].arEvalMove;

	gtk_clist_append( GTK_CLIST( pwMoves ), aszEmpty );
	
	for( j = 0; j < 5; j++ ) {
	    sprintf( sz, "%5.3f", ar[ j ] );
	    gtk_clist_set_text( GTK_CLIST( pwMoves ), i, j, sz );
	}

	sprintf( sz, "%6.3f", ar[ 0 ] * 2.0 + ar[ 1 ] + ar[ 2 ] - ar[ 3 ] -
		 ar[ 4 ] - 1.0 );
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 5, sz );

	
	gtk_clist_set_text( GTK_CLIST( pwMoves ), i, 6,
			    FormatMove( sz, anBoard,
					pml->amMoves[ i ].anMove ) );
    }

    gtk_container_add( GTK_CONTAINER( gtk_container_children( GTK_CONTAINER(
	GTK_DIALOG( pwDialog )->vbox ) )->data ), pwMoves );

    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    
    gtk_widget_show_all( pwDialog );
}

static GtkWidget *pwRolloutDialog, *pwRolloutResult, *pwProgress;
static guint nRolloutSignal;

static void RolloutCancel( GtkObject *po, gpointer p ) {

    pwRolloutDialog = pwRolloutResult = pwProgress = NULL;
    fInterrupt = TRUE;
}

extern void GTKRollout( int c ) {
    
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

    gtk_clist_append( GTK_CLIST( pwRolloutResult ), aszEmpty );
    gtk_clist_append( GTK_CLIST( pwRolloutResult ), aszEmpty );

    gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), 0, 0, "Mean" );
    gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), 1, 0, "Standard error" );

    gtk_progress_configure( GTK_PROGRESS( pwProgress ), 0, 0, c );
    gtk_progress_set_show_text( GTK_PROGRESS( pwProgress ), TRUE );
    gtk_progress_set_format_string( GTK_PROGRESS( pwProgress ),
				    "%v/%u (%p%%)" );
    
    gtk_container_add( GTK_CONTAINER( gtk_container_children( GTK_CONTAINER(
	GTK_DIALOG( pwRolloutDialog )->vbox ) )->data ), pwVbox );

    gtk_window_set_modal( GTK_WINDOW( pwRolloutDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwRolloutDialog ),
				  GTK_WINDOW( pwMain ) );
    
    gtk_widget_show_all( pwRolloutDialog );

    DisallowStdin();
    while( gtk_events_pending() )
        gtk_main_iteration();
    AllowStdin();
}

extern int GTKRolloutUpdate( float arMu[], float arSigma[], int iGame,
			      int cGames ) {
    char sz[ 32 ];
    int i;

    if( !pwRolloutResult )
	return -1;
    
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {
	sprintf( sz, i == OUTPUT_EQUITY ? "%+6.3f" : "%5.3f", arMu[ i ] );
	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), 0, i + 1, sz );
	
	sprintf( sz, "%5.3f", arSigma[ i ] );
	gtk_clist_set_text( GTK_CLIST( pwRolloutResult ), 1, i + 1, sz );
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
