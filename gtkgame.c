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
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
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
    CMD_SET_RNG_ANSI,
    CMD_SET_RNG_BSD,
    CMD_SET_RNG_ISAAC,
    CMD_SET_RNG_MANUAL,
    CMD_SET_RNG_MD5,
    CMD_SET_RNG_MERSENNE,
    CMD_SET_RNG_USER,
    CMD_SHOW_COPYING,
    CMD_SHOW_KLEINMAN,
    CMD_SHOW_PIPCOUNT,
    CMD_SHOW_THORP,
    CMD_SHOW_WARRANTY,
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
    "set rng ansi",
    "set rng bsd",
    "set rng isaac",
    "set rng manual",
    "set rng md5",
    "set rng mersenne",
    "set rng user",
    "show copying",
    "show kleinman",
    "show pipcount",
    "show thorp",
    "show warranty"
};

/* A dummy widget that can grab events when others shouldn't see them. */
static GtkWidget *pwGrab;

GtkWidget *pwMain, *pwBoard, *pwStatus;
guint nNextTurn = 0; /* GTK idle function */
static int cchOutput;
static guint idOutput;
static list lOutput;
int fGTKOutput = FALSE;

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

    if( GTK_IS_RADIO_MENU_ITEM( widget ) ) {
	if( !GTK_CHECK_MENU_ITEM( widget )->active )
	    return;
    } else if( GTK_IS_CHECK_MENU_ITEM( widget ) ) {
	/* FIXME */
	return;
    }
    
    UserCommand( aszCommands[ iCommand ] );
}

static gboolean main_delete( GtkWidget *dice, GdkEvent *event,
			     gpointer p ) {

    UserCommand( "quit" );
    
    return TRUE;
}

static guint nStdin, nDisabledCount = 1;

void AllowStdin( void ) {

    if( !nDisabledCount )
	return;
    
    if( !--nDisabledCount )
	nStdin = gtk_input_add_full( STDIN_FILENO, GDK_INPUT_READ,
				     StdinReadNotify, NULL, NULL, NULL );
}

void DisallowStdin( void ) {

    nDisabledCount++;
    
    if( nStdin ) {
	gtk_input_remove( nStdin );
	nStdin = 0;
    }
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
	{ "/_Settings/Di_splay", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Jacoby", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Settings/_Nackgammon", NULL, NULL, 0, "<CheckItem>" },
	{ "/_Help", NULL, NULL, 0, "<Branch>" },
	{ "/_Help/_Commands", NULL, Command, CMD_HELP, NULL },
	{ "/_Help/Co_pying gnubg", NULL, Command, CMD_SHOW_COPYING, NULL },
	{ "/_Help/gnubg _Warranty", NULL, Command, CMD_SHOW_WARRANTY, NULL },
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

    gtk_box_pack_end( GTK_BOX( pwVbox ), pwStatus = gtk_statusbar_new(),
		      FALSE, FALSE, 0 );
    idOutput = gtk_statusbar_get_context_id( GTK_STATUSBAR( pwStatus ),
					     "gnubg output" );
    
    /* Make sure the window is reasonably big, but will fit on a 640x480
       screen. */
    gtk_window_set_default_size( GTK_WINDOW( pwMain ), 500, 450 );
    gtk_widget_show_all( pwMain );
    
    gtk_signal_connect( GTK_OBJECT( pwMain ), "delete_event",
			GTK_SIGNAL_FUNC( main_delete ), NULL );
    gtk_signal_connect( GTK_OBJECT( pwMain ), "destroy_event",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );

    ListCreate( &lOutput );
    fGTKOutput = TRUE;
    
    ShowBoard();

    AllowStdin();
    
    /* FIXME F_SETOWN is a BSDism... use SIOCSPGRP if necessary. */
    fnAction = HandleXAction;
#ifdef ConnectionNumber /* FIXME use configure somehow to detect this */
    if( ( n = fcntl( ConnectionNumber( GDK_DISPLAY() ), F_GETFL ) ) != -1 ) {
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

    gtk_main();
}

extern void ShowList( char *psz[], char *szTitle ) {

    GtkWidget *pwDialog = gtk_dialog_new(), *pwList = gtk_list_new(),
	*pwScroll = gtk_scrolled_window_new( NULL, NULL ),
	*pwOK = gtk_button_new_with_label( "OK" );

    /* FIXME this shouldn't be modal -- pop up the window and return
       immediately.  Improve the look of the dialog, too. */
    
    while( *psz )
	gtk_container_add( GTK_CONTAINER( pwList ),
			   gtk_list_item_new_with_label( *psz++ ) );
    
    gtk_signal_connect( GTK_OBJECT( pwDialog ), "destroy",
			GTK_SIGNAL_FUNC( gtk_main_quit ), NULL );
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

    gtk_window_set_default_size( GTK_WINDOW( pwDialog ), 560, 400 );
    gtk_window_set_title( GTK_WINDOW( pwDialog ), szTitle );

    gtk_window_set_modal( GTK_WINDOW( pwDialog ), TRUE );
    gtk_window_set_transient_for( GTK_WINDOW( pwDialog ),
				  GTK_WINDOW( pwMain ) );
    gtk_widget_show_all( pwDialog );

    DisallowStdin();
    gtk_main();
    AllowStdin();
}

static void OK( GtkWidget *pw, int *pf ) {

    if( pf )
	*pf = TRUE;
    
    gtk_widget_destroy( gtk_widget_get_toplevel( pw ) );
}

static GtkWidget *CreateDialog( char *szTitle, int fQuestion, int *pResult ) {

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

    gtk_signal_connect( GTK_OBJECT( pwOK ), "clicked", GTK_SIGNAL_FUNC( OK ),
			pResult );

    if( fQuestion ) {
	pwCancel = gtk_button_new_with_label( "Cancel" );
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwCancel );
	gtk_signal_connect_object( GTK_OBJECT( pwCancel ), "clicked",
				   GTK_SIGNAL_FUNC( gtk_widget_destroy ),
				   GTK_OBJECT( pwDialog ) );
    }
    
    gtk_window_set_title( GTK_WINDOW( pwDialog ), szTitle );

/* FIXME    gtk_window_set_default( GTK_WINDOW( pwDialog ), pwOK ); ??? */

    return pwDialog;
}

static int Message( char *sz, int fQuestion ) {

    int f = FALSE, fRestoreNextTurn;
    GtkWidget *pwDialog = CreateDialog( fQuestion ? "GNU Backgammon - Question"
					: "GNU Backgammon - Message",
					fQuestion, &f ),
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

    /* Always clear any previous message from the status bar. */
    gtk_statusbar_pop( GTK_STATUSBAR( pwStatus ), idOutput );
    
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
	Message( sz, FALSE );
    else
	/* Short message; display in status bar. */
	gtk_statusbar_push( GTK_STATUSBAR( pwStatus ), idOutput, sz );
    
    cchOutput = 0;
    g_free( sz );
}

extern void GTKHint( movelist *pml ) {

    static char *aszTitle[] = {
	"Win", "Win (g)", "Win (bg)", "Lose (g)", "Lose (bg)", "Equity",
	"Move"
    }, *aszEmpty[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    GtkWidget *pwDialog = CreateDialog( "GNU Backgammon - Hint", FALSE, NULL ),
	*pwMoves = gtk_clist_new_with_titles( 7, aszTitle );
    int i, j;
    char sz[ 32 ];

    for( i = 0; i < 7; i++ ) {
	gtk_clist_set_column_auto_resize( GTK_CLIST( pwMoves ), i, TRUE );
	gtk_clist_set_column_justification( GTK_CLIST( pwMoves ), i,
					    i == 6 ? GTK_JUSTIFY_LEFT :
					    GTK_JUSTIFY_RIGHT );
    }
    
    for( i = 0; i < pml->cMoves; i++ ) {
	float *ar = pml->amMoves[ i ].pEval;

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
