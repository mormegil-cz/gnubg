/*
 * gtktoolbar.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2003
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

#include <stdlib.h>

#include <gtk/gtk.h>

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "backgammon.h"
#include "gtktoolbar.h"
#include "gtkboard.h"
#include "gtkgame.h"
#include "gtk-multiview.h"
#include "gtkfile.h"
#include "drawboard.h"
#include "renderprefs.h"
#include "gnubgstock.h"
#if USE_BOARD3D
#include "fun3d.h"
#endif

extern void NewDialog( gpointer *p, guint n, GtkWidget *pw ); 

typedef struct _toolbarwidget {

  GtkWidget *pwNew;        /* button for "New" */
  GtkWidget *pwOpen;       /* button for "Open" */
  GtkWidget *pwSave;     /* button for "Double" */
  GtkWidget *pwDouble;
  GtkWidget *pwTake;       /* button for "Take" */
  GtkWidget *pwDrop;       /* button for "Drop" */
  GtkWidget *pwResign;       /* button for "Resign" */
  GtkWidget *pwEndGame;       /* button for "play game" */
  GtkWidget *pwHint;      /* button for "Hint" */
  GtkWidget *pwPrev;      /* button for "Previous Roll" */
  GtkWidget *pwPrevGame;      /* button for "Previous Game" */
  GtkWidget *pwNextGame;      /* button for "Next Game" */
  GtkWidget *pwNext;      /* button for "Next Roll" */
  GtkWidget *pwNextCMarked;      /* button for "Next CMarked" */
  GtkWidget *pwNextMarked;      /* button for "Next CMarked" */
  GtkWidget *pwReset;      /* button for "Reset" */
  GtkWidget *pwEdit;       /* button for "Edit" */
  GtkWidget *pwHideShowPanel; /* button hide/show panel */
  GtkWidget *pwButtonClockwise; /* button for clockwise */

} toolbarwidget;

static void ButtonClicked( GtkWidget *UNUSED(pw), char *sz ) {

    UserCommand( sz );
}


static void ButtonClickedYesNo( GtkWidget *UNUSED(pw), char *sz ) {

  if ( ms.fResigned ) {
    UserCommand ( ! strcmp ( sz, "yes" ) ? "accept" : "decline" );
    return;
  }

  if ( ms.fDoubled ) {
    UserCommand ( ! strcmp ( sz, "yes" ) ? "take" : "drop" );
    return;
  }

}


extern GtkWidget *
image_from_xpm_d ( char **xpm, GtkWidget *pw ) {

  GdkPixmap *ppm;
  GdkBitmap *mask;

  ppm = gdk_pixmap_colormap_create_from_xpm_d( NULL,
                 gtk_widget_get_colormap( pw ), &mask, NULL,
                                               xpm );
  if ( ! ppm )
    return NULL;
  
  return gtk_pixmap_new( ppm, mask );

}

static GtkWidget *toggle_button_from_images( GtkWidget *pwImageOff,
                           GtkWidget *pwImageOn, char *sz )
{
  GtkWidget **aapw;
  GtkWidget *pwm = gtk_multiview_new();
  GtkWidget *pw = gtk_toggle_button_new();
  GtkWidget *pwvbox = gtk_vbox_new(FALSE, 0); 

  aapw = (GtkWidget **) g_malloc( 3 * sizeof ( GtkWidget *) );

  aapw[ 0 ] = pwImageOff;
  aapw[ 1 ] = pwImageOn;
  aapw[ 2 ] = pwm;

  gtk_container_add( GTK_CONTAINER( pwvbox ), pwm );

  gtk_container_add( GTK_CONTAINER( pwm ), pwImageOff );
  gtk_container_add( GTK_CONTAINER( pwm ), pwImageOn );
  gtk_container_add( GTK_CONTAINER( pwvbox ), gtk_label_new(sz));
  gtk_container_add( GTK_CONTAINER( pw ), pwvbox);

  
  g_object_set_data_full( G_OBJECT( pw ), "toggle_images", aapw, g_free );

  return pw;
  
}

extern void
ToolbarSetPlaying( GtkWidget *pwToolbar, const int f ) {

  toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ),
		  "toolbarwidget" );
  
  gtk_widget_set_sensitive( ptw->pwReset, f );

}


extern void
ToolbarSetClockwise( GtkWidget *pwToolbar, const int f ) {

  toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ),
		  "toolbarwidget" );
  
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( ptw->pwButtonClockwise ), f );

}

extern void click_swapdirection(void)
{
	if (!inCallback)
	{
		toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ), "toolbarwidget" );
		gtk_button_clicked( GTK_BUTTON( ptw->pwButtonClockwise ));
	}
}

static void ToolbarToggleClockwise( GtkWidget *pw, toolbarwidget *ptw )
{
  GtkWidget **aapw = (GtkWidget **) g_object_get_data( G_OBJECT( pw ), "toggle_images" );
  int f = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( pw ) );
  
  gtk_multiview_set_current( GTK_MULTIVIEW( aapw[ 2 ] ), aapw[ f ] );

  inCallback = TRUE;
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif, "/View/Play Clockwise" )), f);
  inCallback = FALSE;

  if ( fClockwise != f )
  {
    gchar *sz = g_strdup_printf( "set clockwise %s", f ? "on" : "off" );
    UserCommand( sz );
    g_free( sz );
  }
}

int editing = FALSE;

extern void click_edit(void)
{
	if (!inCallback)
	{
		toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ), "toolbarwidget" );
        gtk_button_clicked( GTK_BUTTON( ptw->pwEdit ));
	}
}

static void ToolbarToggleEdit(GtkWidget *pw)
{
	BoardData *pbd = BOARD(pwBoard)->board_data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw))) {
		if (ms.gs == GAME_NONE)
			edit_new(nDefaultLength);
		/* Undo any partial move that may have been made when entering edit mode */
		editing = TRUE;
		GTKUndo();
	} else
		editing = FALSE;

	inCallback = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(pif, "/Edit/Edit Position" )), editing);
	inCallback = FALSE;

	board_edit(pbd);
}

extern int ToolbarIsEditing(GtkWidget *UNUSED(pwToolbar))
{
  return editing;
}

extern toolbarcontrol
ToolbarUpdate ( GtkWidget *pwToolbar,
                const matchstate *pms,
                const DiceShown diceShown,
                const int fComputerTurn,
                const int fPlaying ) {

  toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ),
		  "toolbarwidget" );
  toolbarcontrol c;
  int fEdit = ToolbarIsEditing( pwToolbar );

  g_assert ( ptw );

  c = C_NONE;

  if ( diceShown == DICE_BELOW_BOARD )
    c = C_ROLLDOUBLE;

  if( pms->fDoubled )
    c = C_TAKEDROP;

  if( pms->fResigned )
    c = C_AGREEDECLINE;

  if( fComputerTurn )
    c = C_PLAY;

  if( fEdit || ! fPlaying )
    c = C_NONE;

  gtk_widget_set_sensitive ( ptw->pwTake,
                             c == C_TAKEDROP || c == C_AGREEDECLINE );
  gtk_widget_set_sensitive ( ptw->pwDrop,
                             c == C_TAKEDROP || c == C_AGREEDECLINE );
  gtk_widget_set_sensitive ( ptw->pwDouble,
                             (c == C_TAKEDROP && ! pms->nMatchTo) || c == C_ROLLDOUBLE );

  gtk_widget_set_sensitive ( ptw->pwSave,  plGame != NULL );
  gtk_widget_set_sensitive ( ptw->pwResign, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwHint, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwPrev, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwPrevGame, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwNextGame, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwNext, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwNextCMarked, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwNextMarked, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwEndGame, fPlaying  && !fEdit);
  gtk_widget_set_sensitive ( ptw->pwEdit, TRUE );

  return c;
}

static GtkWidget* ToolbarAddButton(GtkToolbar *pwToolbar, const char *stockID, const char *tooltip, GtkSignalFunc callback, void *data)
{
	GtkToolItem* but = gtk_tool_button_new_from_stock(stockID);
	gtk_widget_set_tooltip_text(GTK_WIDGET(but), tooltip);
	gtk_toolbar_insert(GTK_TOOLBAR(pwToolbar), but, -1);

	g_signal_connect(G_OBJECT(but), "clicked", callback, data);

	return GTK_WIDGET(but);
}

static GtkWidget* ToolbarAddWidget(GtkToolbar *pwToolbar, GtkWidget *pWidget, const char *tooltip)
{
	GtkToolItem* ti = gtk_tool_item_new();
	gtk_widget_set_tooltip_text(GTK_WIDGET(ti), tooltip);
	gtk_container_add(GTK_CONTAINER(ti), pWidget);

	gtk_toolbar_insert(GTK_TOOLBAR(pwToolbar), ti, -1);

	return GTK_WIDGET(ti);
}

static void ToolbarAddSeparator(GtkToolbar *pwToolbar)
{
	GtkToolItem *sep = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(pwToolbar), sep, -1);
}

extern GtkWidget *ToolbarNew(void)
{
	GtkWidget *vbox_toolbar;
	GtkToolItem *ti;
	GtkWidget *pwToolbar, *pwvbox;

	toolbarwidget *ptw;

#include "xpm/tb_anticlockwise.xpm"
#include "xpm/tb_clockwise.xpm"
    
	/* 
	* Create toolbar 
	*/

	ptw = (toolbarwidget *) g_malloc ( sizeof ( toolbarwidget ) );

	vbox_toolbar = gtk_vbox_new (FALSE, 0);

	g_object_set_data_full ( G_OBJECT ( vbox_toolbar ), "toolbarwidget",
							 ptw, g_free );
	pwToolbar = gtk_toolbar_new ();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(pwToolbar), GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_toolbar_set_orientation ( GTK_TOOLBAR ( pwToolbar ),
								GTK_ORIENTATION_HORIZONTAL );
	gtk_toolbar_set_style ( GTK_TOOLBAR ( pwToolbar ), GTK_TOOLBAR_BOTH );
	gtk_box_pack_start( GTK_BOX( vbox_toolbar ), pwToolbar, 
					  FALSE, FALSE, 0 );

	gtk_toolbar_set_tooltips ( GTK_TOOLBAR ( pwToolbar ), TRUE );

	/* New button */
	ptw -> pwNew = ToolbarAddButton(GTK_TOOLBAR ( pwToolbar ), GTK_STOCK_NEW, _("Start new game, match, session or position"), G_CALLBACK( GTKNew ), NULL);

	/* Open button */
	ptw -> pwOpen = ToolbarAddButton(GTK_TOOLBAR ( pwToolbar ), GTK_STOCK_OPEN, _("Open game, match, session or position"), G_CALLBACK( GTKOpen ), NULL);

	/* Save button */
	ptw->pwSave = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GTK_STOCK_SAVE, _("Save match, session, game or position"),  G_CALLBACK(GTKSave), NULL);
  
	ToolbarAddSeparator(GTK_TOOLBAR(pwToolbar));

	/* Take/accept button */
	ptw->pwTake = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_ACCEPT, _("Take the offered cube or accept the offered resignation"), G_CALLBACK(ButtonClickedYesNo), "yes");

	/* drop button */
	ptw->pwDrop = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_REJECT, _("Drop the offered cube or decline the offered resignation"), G_CALLBACK(ButtonClickedYesNo), "no");

	/* Double button */
	ptw->pwDouble = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_DOUBLE, _("Double or redouble(beaver)"), G_CALLBACK(ButtonClicked), "double");
  
	/* Resign button */
	ptw->pwResign = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_RESIGN, _("Resign the current game"), G_CALLBACK(GTKResign), NULL);

	/* End game button */
	ptw->pwEndGame = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_END_GAME, _("Let the computer end the game"), G_CALLBACK(ButtonClicked), "end game");
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(ptw->pwEndGame), FALSE);

	ToolbarAddSeparator(GTK_TOOLBAR(pwToolbar));
  
	/* reset button */
	ptw-> pwReset = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GTK_STOCK_UNDO, _("Undo moves"), G_CALLBACK(GTKUndo), NULL);

	/* Hint button */
	ptw->pwHint = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_HINT, _("Show the best moves or cube action"), G_CALLBACK(ButtonClicked), "hint");

	/* edit button */
	ptw->pwEdit = gtk_toggle_button_new();
	pwvbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pwvbox), gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_LARGE_TOOLBAR));
	gtk_container_add(GTK_CONTAINER(pwvbox), gtk_label_new(_("Edit")));
	gtk_container_add(GTK_CONTAINER(ptw->pwEdit), pwvbox);
	g_signal_connect(G_OBJECT(ptw->pwEdit), "toggled", G_CALLBACK(ToolbarToggleEdit), NULL);
	ti = GTK_TOOL_ITEM(ToolbarAddWidget(GTK_TOOLBAR(pwToolbar), ptw->pwEdit, _("Toggle Edit Mode")));
	gtk_tool_item_set_homogeneous(ti, TRUE);

	/* direction of play */
	ptw->pwButtonClockwise = toggle_button_from_images ( 
		  image_from_xpm_d( tb_anticlockwise_xpm, pwToolbar),
		  image_from_xpm_d( tb_clockwise_xpm, pwToolbar),
                  _("Direction"));
	g_signal_connect(G_OBJECT(ptw->pwButtonClockwise), "toggled", 
		  G_CALLBACK( ToolbarToggleClockwise ), ptw );

	ToolbarAddWidget(GTK_TOOLBAR(pwToolbar), ptw->pwButtonClockwise, _("Reverse direction of play"));

	ti = gtk_separator_tool_item_new();
	gtk_tool_item_set_expand(GTK_TOOL_ITEM(ti), TRUE);
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(ti), FALSE);
	gtk_toolbar_insert(GTK_TOOLBAR(pwToolbar), ti, -1);

	ptw->pwPrev = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_PREV, _("Go to Previous Roll"), G_CALLBACK(ButtonClicked), "previous roll");
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(ptw->pwPrev), FALSE);
	ptw->pwPrevGame = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_PREV_GAME, _("Go to Previous Game"), G_CALLBACK(ButtonClicked), "previous game");
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(ptw->pwPrevGame), FALSE);
	ptw->pwNextGame = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_NEXT_GAME, _("Go to Next Game"), G_CALLBACK(ButtonClicked), "next game");
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(ptw->pwNextGame), FALSE);
	ptw->pwNext = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_NEXT, _("Go to Next Roll"), G_CALLBACK(ButtonClicked), "next roll");
	ptw->pwNextCMarked = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_NEXT_CMARKED, _("Go to Next CMarked"), G_CALLBACK(ButtonClicked), "next cmarked");
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(ptw->pwNextCMarked), FALSE);
	ptw->pwNextMarked = ToolbarAddButton(GTK_TOOLBAR(pwToolbar), GNUBG_STOCK_GO_NEXT_MARKED, _("Go to Next Marked"), G_CALLBACK(ButtonClicked), "next marked");

	return vbox_toolbar;
}

static GtkWidget* firstChild(GtkWidget* widget)
{
	GList *list = gtk_container_get_children(GTK_CONTAINER(widget));
	GtkWidget *child = g_list_nth_data(list, 0);
	g_list_free(list);
	return child;
}

static void SetToolbarItemStyle(gpointer data, gpointer user_data)
{
	GtkWidget* widget = GTK_WIDGET(data);
	int style = GPOINTER_TO_INT(user_data);
	/* Find icon and text widgets from parent object */
	GList* buttonParts;
	GtkWidget *icon, *text;
	GtkWidget* buttonParent;

	buttonParent = firstChild(widget);
	buttonParts = gtk_container_get_children(GTK_CONTAINER(buttonParent));
	icon = g_list_nth_data(buttonParts, 0);
	text = g_list_nth_data(buttonParts, 1);
	g_list_free(buttonParts);

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
	if (value != nToolbarStyle)
	{
		toolbarwidget *ptw = g_object_get_data ( G_OBJECT ( pwToolbar ), "toolbarwidget" );
		/* Set last 2 toolbar items separately - as special widgets not covered in gtk_toolbar_set_style */
		SetToolbarItemStyle(ptw->pwEdit, GINT_TO_POINTER(value));
		SetToolbarItemStyle(ptw->pwButtonClockwise, GINT_TO_POINTER(value));
		gtk_toolbar_set_style(GTK_TOOLBAR(firstChild(pwToolbar)), (GtkToolbarStyle)value);

		/* Resize handle box parent */
		gtk_widget_queue_resize(pwToolbar);
		nToolbarStyle = value;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget_by_action(pif, value + TOOLBAR_ACTION_OFFSET)), TRUE);
	}
}
