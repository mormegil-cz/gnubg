/*
 * gtkboard.h
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2001.
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

#ifndef _GTKBOARD_H_
#define _GTKBOARD_H_

#include "render.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_BOARD ( board_get_type() )
#define BOARD( obj ) ( GTK_CHECK_CAST( (obj), TYPE_BOARD, Board ) )
#define BOARD_CLASS( c ) ( GTK_CHECK_CLASS_CAST( (c), TYPE_BOARD, \
	BoardClass ) )
#define IS_BOARD( obj ) ( GTK_CHECK_TYPE( (obj), TYPE_BOARD ) )
#define IS_BOARD_CLASS( c ) ( GTK_CHECK_CLASS_TYPE( (c), TYPE_BOARD ) )

/* minimum time in milliseconds before a drag to the
	same point is considered a real drag rather than a click */
#define CLICK_TIME 200

typedef enum _animation {
    ANIMATE_NONE, ANIMATE_BLINK, ANIMATE_SLIDE
} animation;

extern animation animGUI;
extern int nGUIAnimSpeed, fGUIBeep, fGUIDiceArea, fGUIHighDieFirst,
    fGUIIllegal, fGUIShowIDs, fGUIShowPips, fGUISetWindowPos,
    fGUIDragTargetHelp;

typedef struct _Board {
    GtkVBox vbox;
    gpointer board_data;
} Board;

typedef struct _BoardClass {
    GtkVBoxClass parent_class;
} BoardClass;

extern GtkType board_get_type( void );    
extern GtkWidget *board_new( void );
extern GtkWidget *board_cube_widget( Board *board );
extern GtkWidget *board_dice_widget( Board *board );
extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1,
		      gint computer_turn, gint nchequers );
extern gint game_set_old_dice( Board *board, gint die0, gint die1 );
extern void board_set_playing( Board *board, gboolean f );
extern void board_animate( Board *board, int move[ 8 ], int player );

extern GtkWidget *
image_from_xpm_d ( char **xpm, GtkWidget *pw );

#if USE_BOARD3D
/* New 3d types */
typedef struct _Texture
{
	int texID;
	int width;
	int height;
} Texture;

typedef struct _Material
{
	float ambientColour[4];
	float diffuseColour[4];
	float specularColour[4];
	int shininess;
	int alphaBlend;
	Texture* pTexture;
} Material;

typedef struct _DiceRotation
{
	float xRotStart, yRotStart;
	float xRot, yRot;
	float xRotFactor, yRotFactor;
} DiceRotation;

typedef enum _BoardState
{
	BOARD_CLOSED, BOARD_CLOSING, BOARD_OPENING, BOARD_OPEN
} BoardState;

#define MAX_TEXTURES 10

#endif

/* private data */
typedef struct _BoardData {
    GtkWidget *drawing_area, *dice_area, *table, *move,
	*position_id, *reset, *edit, *name0, *name1, *score0, *score1, 
	*crawford, *widget, *key0, *key1, *stop, *stopparent, 
	*doub, *lname0, *lname1,
	*lscore0, *lscore1, *mname0, *mname1, *mscore0, *mscore1, *play,
        *match_id;
    GtkWidget *mmatch, *lmatch, *match;
    GtkAdjustment *amatch, *ascore0, *ascore1;
    GtkWidget *toolbar, *vbox_toolbar;
    GtkWidget *roll;
    GtkWidget *take, *drop, *redouble;
    GtkWidget *vbox_ids;
    GtkWidget *pipcount0, *pipcount1;
    GdkGC *gc_and, *gc_or, *gc_copy, *gc_cube;
    GdkPixmap *appmKey[ 2 ];
    GtkWidget *button_clockwise;
    
    gboolean playing, computer_turn;
    gint drag_point, drag_colour, x_drag, y_drag, x_dice[ 2 ], y_dice[ 2 ],
	old_board[ 2 ][ 25 ], drag_button, click_time,
	cube_use, dice_roll[ 2 ]; /* roll showing on the off-board dice */
    gint cube_owner; /* -1 = bottom, 0 = centred, 1 = top */
    gint qedit_point; /* used to remember last point in quick edit mode */
    gint resigned;
    gint nchequers;
    move *all_moves, *valid_move;
    movelist move_list;

    renderimages ri;
    
    /* remainder is from FIBS board: data */
    char name[ 32 ], name_opponent[ 32 ];
    gint match_to, score, score_opponent;
    gint points[ 28 ]; /* 0 and 25 are the bars */
    gint turn; /* -1 is X, 1 is O, 0 if game over */
    gint dice[ 2 ], dice_opponent[ 2 ]; /* 0, 0 if not rolled */
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

#if USE_BOARD3D
/* extra members for 3d board */
	GtkWidget *drawing_area3d;	/* main 3d widget */

	/* Bit of a hack - assign each possible position a set rotation */
	int pieceRotation[28][15];
	int movingPieceRotation;

	/* Materials for 3d objects */
	Material diceMat[2], diceDotMat[2];
	Material cubeMat, cubeNumberMat;
	Material checkerMat[2];
	Material baseMat, boxMat, pointMat[2];
	Material hingeMat;
	Material pointNumberMat;
	Material backGroundMat;
	Material gap;
	Material logoMat;
	Material flagMat;
	/* Store how "big" the screen maps to in 3d */
	float backGroundPos[2], backGroundSize[2];

	/* List of textures */
	Texture textureList[MAX_TEXTURES];
	char* textureNames[MAX_TEXTURES];
	int numTextures;

	int pieceType;	/* Different piece models */

	BoardState State;	/* Open/closed board */
	float perOpen;	/* Percentage open when opening/closing board */

	int moving;	/* Is a piece moving (animating) */
	float rotateMovingPiece;
	int shakingDice;	/* Are dice being animated */

	float movingPos[3];
	float dragPos[3];
	float dicePos[2][3];
	float diceMovingPos[2][3];
	DiceRotation diceRotation[2];

	float flagWaved;	/* How much has flag waved */

	int iTargetHelpPoints[4];	/* Drag target position */
	int DragTargetHelp;	/* Currently showing draw targets? */
#endif
} BoardData;

extern void board_create_pixmaps( GtkWidget *board, BoardData *bd );
extern void board_free_pixmaps( BoardData *bd );

#if USE_BOARD3D
/* Functions for 3d board */
extern int InitGTK3d(int *argc, char ***argv);
extern void SetupViewingVolume3d();
extern void DisplayCorrectBoardType();
extern void ShowBoard3d(BoardData *bd);
extern void CreateBoard3d(BoardData* bd, GtkWidget** drawing_area);
extern void RollDice3d(BoardData *bd);
extern void AnimateMove3d(BoardData *bd);
extern void ShowFlag3d(BoardData *bd);
extern void StopIdle3d();
extern void SetShadowDimness3d();
extern void preDraw3d();
extern void CloseBoard3d(BoardData* bd);
extern void SetSkin(BoardData *bd, int num);
void SetupLight3d(BoardData *bd);

extern void PlaceMovingPieceRotation(BoardData* bd, int dest, int src);
extern void SetMovingPieceRotation(BoardData* bd, int pt);
extern void updateOccPos(BoardData* bd);
extern void updatePieceOccPos(BoardData* bd);
extern void updateHingeOccPos(BoardData* bd);
extern void updateFlagOccPos(BoardData* bd);
extern void CheckAccelerated();
#endif

extern void read_board( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_pipcount ( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void write_board ( BoardData *bd, int anBoard[ 2 ][ 25 ] );
extern void board_beep( BoardData *bd );
extern void Confirm( BoardData *bd );
extern int ForcedMove ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] );
extern int GreadyBearoff ( int anBoard[ 2 ][ 25 ], int anDice[ 2 ] );
extern int update_move(BoardData *bd);
extern gboolean place_chequer_or_revert(BoardData *bd, int dest);
extern gboolean LegalDestPoints( BoardData *bd, int iDestPoints[4] );
extern void InitBoardData();

extern void
DrawDie( GdkDrawable *pd, 
         unsigned char *achDice[ 2 ], unsigned char *achPip[ 2 ],
         const int s, GdkGC *gc, int x, int y, int fColour, int n );

extern int UpdateMove( BoardData *bd, int anBoard[ 2 ][ 25 ] );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
