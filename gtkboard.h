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
#include "backgammon.h"

#if USE_BOARD3D
#include <GL/gl.h>
#include <GL/glu.h>
#include "board3d/mylist.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
extern int nGUIAnimSpeed, fGUIBeep, fGUIHighDieFirst,
    fGUIIllegal, fGUIShowPips, fGUISetWindowPos,
    fGUIDragTargetHelp, fGUIUseStatsPanel, fGUIShowEPCs;

typedef struct _Board {
    GtkVBox vbox;
    gpointer board_data;
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
#if USE_TIMECONTROL
extern void board_set_clock(Board *board, gchar *c0, gchar *c1);
extern void board_set_scores(Board *board, int s0, int s1);
#endif

#if USE_BOARD3D
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

/* Animation paths */
#define MAX_PATHS 3
typedef enum _PathType
{
	PATH_LINE, PATH_CURVE_9TO12, PATH_CURVE_12TO3, PATH_PARABOLA, PATH_PARABOLA_12TO3
} PathType;

typedef struct _Path
{
	float pts[MAX_PATHS + 1][3];
	PathType pathType[MAX_PATHS];
	int state;
	float mileStone;
	int numSegments;
} Path;

/* Occulsion model */
typedef struct _OccModel
{
	vector planes;
	vector edges;
	vector points;
} OccModel;

typedef struct Occluder_T
{
	float invMat[4][4];
	float trans[3];
	float rot[3];
	int rotator;

	OccModel* handle;
	int show;
} Occluder;

typedef enum _OcculderType {
	OCC_BOARD, OCC_CUBE, OCC_DICE1, OCC_DICE2, OCC_FLAG, OCC_HINGE1, OCC_HINGE2, OCC_PIECE
} OcculderType;
#define LAST_PIECE (OCC_PIECE + 29)

#define NUM_OCC (LAST_PIECE + 1)

#endif

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
    GtkWidget *epclabel0, *epclabel1;
    GtkWidget *epc0, *epc1;
#if USE_TIMECONTROL
    GtkWidget *clock0, *clock1;
#endif
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
    gint diceRoll[ 2 ]; /* 0, 0 if not rolled */
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

/* extra members for 3d board */
#if USE_BOARD3D
	GtkWidget *drawing_area3d;	/* main 3d widget */

	/* Bit of a hack - assign each possible position a set rotation */
	int pieceRotation[28][15];
	int movingPieceRotation;

	/* Misc 3d objects */
	Material gapColour;
	Material logoMat;
	Material flagMat, flagNumberMat;

	/* Store how "big" the screen maps to in 3d */
	float backGroundPos[2], backGroundSize[2];

	BoardState State;	/* Open/closed board */
	float perOpen;	/* Percentage open when opening/closing board */

	int moving;	/* Is a piece moving (animating) */
	Path piecePath;	/* Animation path for moving pieces */
	float rotateMovingPiece;	/* Piece going home? */
	int shakingDice;	/* Are dice being animated */
	Path dicePaths[2];	/* Dice shaking paths */

	/* Some positions of dice, moving/dragging pieces */
	float movingPos[3];
	float dragPos[3];
	float dicePos[2][3];
	float diceMovingPos[2][3];
	DiceRotation diceRotation[2];

	float flagWaved;	/* How much has flag waved */

	void *numberFont, *cubeFont;	/* FTGL fonts */

	/* Saved viewing values (used for picking) */
	float vertFrustrum, horFrustrum;
	float modelMatrix[16];

	/* Display list ids and quadratics */
	GLuint diceList, DCList, pieceList;
	GLUquadricObj *qobjTex, *qobj;

	/* Define nurbs surface - for flag */
	GLUnurbsObj *flagNurb;
	#define S_NUMPOINTS 4
	#define S_NUMKNOTS (S_NUMPOINTS * 2)
	#define T_NUMPOINTS 2
	#define T_NUMKNOTS (T_NUMPOINTS * 2)
	/* Control points for the flag. The Z values are modified to make it wave */
	float ctlpoints[S_NUMPOINTS][T_NUMPOINTS][3];

	/* Shadow casters */
	Occluder Occluders[NUM_OCC];

	float shadow_light_position[4];

	float ***boardPoints;	/* Used for rounded corners */

	/* Textures */
#define MAX_TEXTURES 10
	Texture textureList[MAX_TEXTURES];
	char* textureName[MAX_TEXTURES];
	int numTextures;
	int dotTexture;	/* Holds texture used to draw dots on dice */

#endif
	renderdata* rd;	/* The board colour settings */
} BoardData;

extern void board_create_pixmaps( GtkWidget *board, BoardData *bd );
extern void board_free_pixmaps( BoardData *bd );
extern void board_edit( BoardData *bd );

extern void InitBoardPreview(BoardData *bd);

#if USE_BOARD3D
/* Functions for 3d board */
extern void InitGTK3d(int *argc, char ***argv);
extern void Init3d();
extern void InitBoard3d(BoardData *bd);
extern void freeEigthPoints(float ****boardPoints, int accuracy);
extern void SetupVisual();
extern void SetupViewingVolume3d(BoardData *bd);
extern void DisplayCorrectBoardType(BoardData* bd);
extern void SetupLight3d(BoardData *bd, renderdata* prd);
extern GtkWidget* CreateGLWidget(BoardData* bd);
extern int DoAcceleratedCheck(GtkWidget* board);

extern void *CreatePreviewBoard3d(BoardData* bd, GdkPixmap *ppm);
extern void RollDice3d(BoardData *bd);
extern void AnimateMove3d(BoardData *bd);
extern void ShowFlag3d(BoardData *bd);
extern void StopIdle3d(BoardData* bd);
extern void preDraw3d();
extern void CloseBoard3d(BoardData* bd);
extern int BoardPoint3d(BoardData *bd, int x, int y, int point);
extern int MouseMove3d(BoardData *bd, int x, int y);
extern void RenderBoard3d(BoardData* bd, renderdata* prd, void *glpixmap, unsigned char* buf);
extern void Tidy3dObjects(BoardData* bd);
extern float TestPerformance3d(BoardData* bd);
extern void Set3dSettings(renderdata *prdnew, const renderdata *prd);
extern void CopySettings3d(BoardData* from, BoardData* to);
extern void MakeCurrent3d(GtkWidget *widget);
extern void GetTextures(BoardData* bd);
extern void ClearTextures(BoardData* bd);

extern void PlaceMovingPieceRotation(BoardData* bd, int dest, int src);
extern void SetMovingPieceRotation(BoardData* bd, int pt);
extern void updateOccPos(BoardData* bd);
extern void updateDiceOccPos(BoardData *bd);
extern void updatePieceOccPos(BoardData* bd);
extern void updateHingeOccPos(BoardData* bd);
extern void updateFlagOccPos(BoardData* bd);

extern void RestrictiveRedraw();
extern void RestrictiveDrawPiece(int pos, int depth);
extern void RestrictiveStartMouseMove(int pos, int depth);
extern void RestrictiveEndMouseMove(int pos, int depth);
extern void RestrictiveDrawDice(BoardData* bd);
extern void RestrictiveDrawCube(BoardData* bd, int old_doubled, int old_cube_owner);
extern void RestrictiveDrawMoveIndicator(BoardData* bd);
extern void RestrictiveDrawBoardNumbers(BoardData* bd);

#endif

extern int animate_player, *animate_move_list, animation_finished;

extern void read_board( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_position_id( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void update_pipcount ( BoardData *bd, gint points[ 2 ][ 25 ] );
extern void write_board ( BoardData *bd, int anBoard[ 2 ][ 25 ] );
extern void board_beep( BoardData *bd );
extern void Confirm( BoardData *bd );
extern int update_move(BoardData *bd);
extern gboolean place_chequer_or_revert(BoardData *bd, int dest);
extern gboolean LegalDestPoints( BoardData *bd, int iDestPoints[4] );
extern void setDicePos(BoardData* bd);
extern int DiceTooClose(BoardData* bd);
extern void InitBoardData(BoardData* bd);
extern gboolean button_press_event(GtkWidget *board, GdkEventButton *event, BoardData* bd);
extern gboolean motion_notify_event(GtkWidget *widget, GdkEventMotion *event, BoardData* bd);
extern gboolean button_release_event(GtkWidget *board, GdkEventButton *event, BoardData* bd);
extern void RollDice2d(BoardData* bd);

extern void
DrawDie( GdkDrawable *pd, 
         unsigned char *achDice[ 2 ], unsigned char *achPip[ 2 ],
         const int s, GdkGC *gc, int x, int y, int fColour, int n );

extern int UpdateMove( BoardData *bd, int anBoard[ 2 ][ 25 ] );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
