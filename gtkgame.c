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

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <gtk/gtk.h>
#include <gdk/gdkx.h> /* for ConnectionNumber GTK_DISPLAY -- get rid of this */
#include <stdio.h>
#include <stdlib.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "backgammon.h"
#include "gtkboard.h"

/* Enumeration to be used as index to the table of command strings below
   (since GTK will only let us put integers into a GtkItemFactoryEntry,
   and that might not be big enough to hold a pointer).  Must be kept in
   sync with the string array! */
enum _gnubgcommands {
    CMD_DATABASE_DUMP = 0,
    CMD_DATABASE_GENERATE,
    CMD_DATABASE_ROLLOUT,
    CMD_DOUBLE,
    CMD_EVAL,
    CMD_HELP,
    CMD_HINT,
    CMD_NEW_GAME,
    CMD_NEW_SESSION,
    CMD_QUIT,
    CMD_RESIGN_N,
    CMD_RESIGN_G,
    CMD_RESIGN_B,
    CMD_ROLL,
    CMD_ROLLOUT,
    CMD_SAVE_GAME,
    CMD_SAVE_MATCH,
    CMD_SAVE_WEIGHTS,
    CMD_SHOW_KLEINMAN,
    CMD_SHOW_PIPCOUNT,
    CMD_SHOW_THORP,
    NUM_CMDS
};
   
static char *aszCommands[ NUM_CMDS ] = {
    "database dump",
    "database generate",
    "database rollout",
    "double",
    "eval",
    "help",
    "hint",
    "new game",
    "new session",
    "quit",
    "resign normal",
    "resign gammon",
    "resign backgammon",
    "roll",
    "rollout",
    "save game",
    "save match",
    "save weights",
    "show kleinman",
    "show pipcount",
    "show thorp"
};

/* A dummy widget that can grab events when others shouldn't see them. */
static GtkWidget *pwGrab;

extern void HandleXAction( void ) {
    /* It is safe to execute this function with SIGIO unblocked, because
       if a SIGIO occurs before fAction is reset, then the I/O it alerts
       us to will be processed anyway.  If one occurs after fAction is reset,
       that will cause this function to be executed again, so we will
       still process its I/O. */
    fAction = FALSE;

    /* Grab events so that the board window knows this is a re-entrant
       call, and won't allow commands like roll, move or double. */
    gtk_grab_add( pwGrab );

    /* Process incoming X events.  It's important to handle all of them,
       because we won't get another SIGIO for events that are buffered
       but not processed. */
    while( gtk_events_pending() )
	gtk_main_iteration();

    /* GTK-FIXME do we need to flush X output here? */

    gtk_grab_remove( pwGrab );
}

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

static void Command( gpointer *p, guint iCommand, GtkWidget *widget ) {

    UserCommand( aszCommands[ iCommand ] );
}

static gboolean main_delete( GtkWidget *dice, GdkEvent *event,
			     gpointer p ) {

    UserCommand( "quit" );
    
    return TRUE;
}

extern void RunGTK( void ) {

    GtkWidget *pwVbox;
    GtkItemFactory *pif;
    GtkAccelGroup *pag;
    int n;
    
    static GtkItemFactoryEntry aife[] = {
	{ "/_File", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New", NULL, NULL, 0, "<Branch>" },
	{ "/_File/_New/_Game", NULL, Command, CMD_NEW_GAME, NULL },
	{ "/_File/_New/_Match", NULL, NULL, 0, NULL },
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
	{ "/_Game/_Double", "<control>D", Command, CMD_DOUBLE, NULL },
	{ "/_Game/Re_sign", NULL, NULL, 0, "<Branch>" },
	{ "/_Game/Re_sign/_Normal", NULL, Command, CMD_RESIGN_N, NULL },
	{ "/_Game/Re_sign/_Gammon", NULL, Command, CMD_RESIGN_G, NULL },
	{ "/_Game/Re_sign/_Backgammon", NULL, Command, CMD_RESIGN_B, NULL },
	{ "/_Analyse", NULL, NULL, 0, "<Branch>" },
	{ "/_Analyse/_Evaluate", "<control>E", Command, CMD_EVAL, NULL },
	{ "/_Analyse/_Hint", "<control>H", Command, CMD_HINT, NULL },
	{ "/_Analyse/_Rollout", NULL, Command, CMD_ROLLOUT, NULL },
	{ "/_Analyse/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Analyse/_Pip count", NULL, Command, CMD_SHOW_PIPCOUNT, NULL },
	{ "/_Analyse/_Kleinman count", NULL, Command, CMD_SHOW_KLEINMAN,
	  NULL },
	{ "/_Analyse/_Thorp count", NULL, Command, CMD_SHOW_THORP, NULL },
	{ "/_Database", NULL, NULL, 0, "<Branch>" },
	{ "/_Database/_Dump", NULL, Command, CMD_DATABASE_DUMP, NULL },
	{ "/_Database/_Generate", NULL, Command, CMD_DATABASE_GENERATE, NULL },
	{ "/_Database/_Rollout", NULL, Command, CMD_DATABASE_ROLLOUT, NULL },
	{ "/_Settings", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Automatic", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Automatic/_Bearoff", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Crawford", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Game", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Move", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Automatic/_Roll", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Confirmation", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Crawford", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Cube", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Cube/_Owner", NULL, NULL, 0, "<Branch>" },
	{ "/_Settings/_Cube/_Use", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Display", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Jacoby", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Nackgammon", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Help", NULL, NULL, 0, "<Branch>" },
	{ "/_Help/_Commands", NULL, Command, CMD_HELP, NULL },
	{ "/_Help/-", NULL, NULL, 0, "<Separator>" },
	{ "/_Help/_About...", NULL, NULL, 0, NULL }
    };
    
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
    gtk_widget_show_all( pwMain );
    
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
    
    ShowBoard();

    gtk_input_add_full( STDIN_FILENO, GDK_INPUT_READ, StdinReadNotify,
			NULL, NULL, NULL );
    
    /* FIXME F_SETOWN is a BSDism... use SIOCSPGRP if necessary. */
    fnAction = HandleXAction;
    if( ( n = fcntl( ConnectionNumber( GDK_DISPLAY() ), F_GETFL ) ) != -1 ) {
	fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETOWN, getpid() );
	fcntl( ConnectionNumber( GDK_DISPLAY() ), F_SETFL, n | FASYNC );
    }
    
#if HAVE_LIBREADLINE
    fReadingCommand = TRUE;
    rl_callback_handler_install( FormatPrompt(), HandleInput );
    atexit( rl_callback_handler_remove );
#else
    Prompt();
#endif

    gtk_main();
}
