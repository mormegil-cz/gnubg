/*
 * gtkboard.h
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2001.
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

#ifndef _GTKBOARD_H_
#define _GTKBOARD_H_

#include "backgammon.h"
#include <gtk/gtk.h>
#include "eval.h"
#include "gtkpanels.h"
#include "common.h"
#include "render.h"

#if USE_BOARD3D
#include "types3d.h"
#endif

#define TYPE_BOARD ( board_get_type() )
#define BOARD( obj ) ( GTK_CHECK_CAST( (obj), TYPE_BOARD, Board ) )
#define BOARD_CLASS( c ) ( GTK_CHECK_CLASS_CAST( (c), TYPE_BOARD, \
	BoardClass ) )
#define IS_BOARD( obj ) ( GTK_CHECK_TYPE( (obj), TYPE_BOARD ) )
#define IS_BOARD_CLASS( c ) ( GTK_CHECK_CLASS_TYPE( (c), TYPE_BOARD ) )

typedef enum _DiceShown {
	DICE_NOT_SHOWN = 0, DICE_BELOW_BOARD, DICE_ON_BOARD, DICE_ROLLING
} DiceShown;

typedef enum _animation {
    ANIMATE_NONE, ANIMATE_BLINK, ANIMATE_SLIDE
} animation;

extern animation animGUI;
extern int fGUIBeep, fGUIHighDieFirst,
    fGUIIllegal, fGUIShowPips, fGUISetWindowPos,
    fGUIDragTargetHelp, fGUIUseStatsPanel, fGUIShowEPCs;
extern unsigned int nGUIAnimSpeed;

struct _BoardData;      /* Forward declaration for use in Board */
typedef struct _Board {
    GtkVBox vbox;
    struct _BoardData *board_data;
} Board;

typedef struct _BoardClass {
    GtkVBoxClass parent_class;
} BoardClass;

extern GtkType board_get_type( void );    
extern GtkWidget *board_new(renderdata* prd);
extern GtkWidget *board_cube_widget( Board *board );
extern void DestroySetCube(GtkObject *po, GtkWidget *pw);
extern void Copy3dDiceColour(renderdata* prd);
extern GtkWidget *board_dice_widget( Board *board );
extern void DestroySetDice(GtkObject *po, GtkWidget *pw);
extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1,
		      gint computer_turn, gint nchequers );
extern void board_set_playing( Board *board, gboolean f );
extern void board_animate( Board *board, int move[ 8 ], int player );
extern unsigned int convert_point( int i, int player );

/* private data */
typedef struct _BoardData {
    GtkWidget *drawing_area, *dice_area, *table, *wmove,
	*position_id, *reset, *edit, *name0, *name1, *score0, *score1, 
	*crawford, *widget, *key0, *key1, *stop, *stopparent, 
	*doub, *lname0, *lname1,
	*lscore0, *lscore1, *mname0, *mname1, *mscore0, *mscore1, *play,
        *match_id;
    GtkWidget *mmatch, *lmatch, *match;
    GtkAdjustment *amatch, *ascore0, *ascore1;
    GtkWidget *roll;
    GtkWidget *take, *drop, *redouble;
    GtkWidget *vbox_ids;
    GtkWidget *pipcount0, *pipcount1;
    GtkWidget *pipcountlabel0, *pipcountlabel1;

    GdkGC *gc_and, *gc_or, *gc_copy, *gc_cube;
    GdkPixmap *appmKey[ 2 ];
    
    gboolean playing, computer_turn;
    gint drag_point, drag_colour, x_drag, y_drag, x_dice[ 2 ], y_dice[ 2 ],
	old_board[ 2 ][ 25 ], drag_button, click_time,
	cube_use; /* roll showing on the off-board dice */
	DiceShown diceShown;

    gint cube_owner; /* -1 = bottom, 0 = centred, 1 = top */
    gint qedit_point; /* used to remember last point in quick edit mode */
    gint resigned;
    gint nchequers;
    move *all_moves, *valid_move;
    movelist move_list;

    renderimages ri;
    
    /* remainder is from FIBS board: data */
    char name[ MAX_NAME_LEN ], name_opponent[ MAX_NAME_LEN ];
    gint match_to, score, score_opponent;
    gint points[ 28 ]; /* 0 and 25 are the bars */
    gint turn; /* -1 is X, 1 is O, 0 if game over */
    unsigned int diceRoll[ 2 ]; /* 0, 0 if not rolled */
    gint cube;
    gint can_double, opponent_can_double; /* allowed to double */
    gint doubled; /* -1 if X is doubling, 1 if O is doubling */
    gint colour; /* -1 for player X, 1 for player O */
    gint direction; /* -1 playing from 24 to 1, 1 playing from 1 to 24 */
    gint home, bar; /* 0 or 25 depending on fDirection */
    gint off, off_opponent; /* number of men borne off */
    gint on_bar, on_bar_opponent; /* number of men on bar */
    gint to_move; /* 0 to 4 -- number of pieces to move */
    gint forced, crawford_game; /* unused, Crawford game flag */
    gint redoubles; /* number of instant redoubles allowed */
	int DragTargetHelp;	/* Currently showing draw targets? */
	int iTargetHelpPoints[4];	/* Drag target position */

#if USE_BOARD3D
	BoardData3d *bd3d;	/* extra members for 3d board */
#endif
	renderdata* rd;	/* The board colour settings */
} BoardData;

extern void board_create_pixmaps( GtkWidget *board, BoardData *bd );
extern void board_free_pixmaps( BoardData *bd );
extern void board_edit( BoardData *bd );

extern void InitBoardPreview(BoardData *bd);

extern int animate_player, *animate_move_list, animation_finished;

enum TheoryTypes{TT_PIPCOUNT = 1, TT_EPC = 2, TT_RETURNHITS = 4, TT_KLEINCOUNT = 8};
void UpdateTheoryData(BoardData* bd, int UpdateTypes, int points[2][25]);

extern void read_board( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_pipcount ( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void write_board ( BoardData *bd, int anBoard[ 2 ][ 25 ] );
extern void board_beep( BoardData *bd );
extern void Confirm( BoardData *bd );
extern int update_move(BoardData *bd);
extern gboolean place_chequer_or_revert(BoardData *bd, int dest);
extern gboolean LegalDestPoints( BoardData *bd, int iDestPoints[4] );
extern void InitBoardData(BoardData* bd);
extern gboolean button_press_event(GtkWidget *board, GdkEventButton *event, BoardData* bd);
extern gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, BoardData* bd);
extern gboolean button_release_event(GtkWidget *board, GdkEventButton *event, BoardData* bd);
extern void RollDice2d(BoardData* bd);
extern void DestroyPanel(gnubgwindow window);
extern void board_set_matchid( GtkWidget *pw, BoardData *bd );
extern void board_set_position( GtkWidget *pw, BoardData *bd );

extern void
DrawDie( GdkDrawable *pd, 
         unsigned char *achDice[ 2 ], unsigned char *achPip[ 2 ],
         const int s, GdkGC *gc, int x, int y, int fColour, int n );

extern int UpdateMove( BoardData *bd, int anBoard[ 2 ][ 25 ] );

#endif
