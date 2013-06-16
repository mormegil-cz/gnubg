/*
 * inc3d.h
 * by Jon Kinsey, 2003
 *
 * General definitions for 3d board
 *
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
#ifndef INC3D_H
#define INC3D_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#if defined(WIN32)
/* MS gl.h needs windows.h to be included first */
#include <windows.h>
#endif

#if defined(USE_APPLE_OPENGL)
#include <gl.h>
#include <glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#if defined(HAVE_GL_GLX_H)
#include <GL/glx.h>             /* x-windows file */
#endif

#include <gtk/gtkgl.h>
#include "fun3d.h"
#include "matrix.h"
#include "gtkwindows.h"

#include "model.h"
#include "drawboard.h"          /* for fClockwise decl */

/* float versions (to quiet compiler warnings) */
#define powi(arg1, arg2) (int)pow((double)(arg1), (double)(arg2))

/* Clipping planes */
#define zNear .1
#define zFar 70.0

extern int numRestrictFrames;

typedef enum _BoardState {
    BOARD_CLOSED, BOARD_CLOSING, BOARD_OPENING, BOARD_OPEN
} BoardState;

/* Font info */

#define FONT_PITCH 24
#define FONT_SIZE (base_unit / 20.0f)
#define FONT_HEIGHT_RATIO 1.f

#define CUBE_FONT_PITCH 34
#define CUBE_FONT_SIZE (base_unit / 24.0f)
#define CUBE_FONT_HEIGHT_RATIO 1.25f

#define FONT_VERA "fonts/Vera.ttf"
#define FONT_VERA_SERIF_BOLD "fonts/VeraSeBd.ttf"

/* Animation paths */
#define MAX_PATHS 3
typedef enum _PathType {
    PATH_LINE, PATH_CURVE_9TO12, PATH_CURVE_12TO3, PATH_PARABOLA, PATH_PARABOLA_12TO3
} PathType;

struct _Path {
    float pts[MAX_PATHS + 1][3];
    PathType pathType[MAX_PATHS];
    int state;
    float mileStone;
    int numSegments;
};

enum OcculderType { OCC_BOARD, OCC_CUBE, OCC_DICE1, OCC_DICE2, OCC_FLAG, OCC_HINGE1, OCC_HINGE2, OCC_PIECE };
#define FIRST_PIECE (int)OCC_PIECE
#define LAST_PIECE ((int)OCC_PIECE + 29)
#define NUM_OCC (LAST_PIECE + 1)

struct _Texture {
    unsigned int texID;
    int width;
    int height;
};

#define FILENAME_SIZE 15
#define NAME_SIZE 20

struct _TextureInfo {
    char file[FILENAME_SIZE + 1];
    char name[NAME_SIZE + 1];
    TextureType type;
};

struct _BoardData3d {
    GtkWidget *drawing_area3d;  /* main 3d widget */

    /* Bit of a hack - assign each possible position a set rotation */
    int pieceRotation[28][15];
    int movingPieceRotation;

    /* Misc 3d objects */
    Material gapColour;
    Material logoMat;
    Material flagMat, flagNumberMat;

    /* Store how "big" the screen maps to in 3d */
    float backGroundPos[2], backGroundSize[2];

    BoardState State;           /* Open/closed board */
    float perOpen;              /* Percentage open when opening/closing board */

    int moving;                 /* Is a piece moving (animating) */
    Path piecePath;             /* Animation path for moving pieces */
    float rotateMovingPiece;    /* Piece going home? */
    int shakingDice;            /* Are dice being animated */
    Path dicePaths[2];          /* Dice shaking paths */

    /* Some positions of dice, moving/dragging pieces */
    float movingPos[3];
    float dragPos[3];
    float dicePos[2][3];
    float diceMovingPos[2][3];
    DiceRotation diceRotation[2];

    float flagWaved;            /* How much has flag waved */

    OGLFont *numberFont, *cubeFont;     /* OpenGL fonts */

    /* Saved viewing values (used for picking) */
    double vertFrustrum, horFrustrum;
    float modelMatrix[16];

    /* Display list ids and quadratics */
    GLuint diceList, DCList, pieceList, piecePickList;
    GLUquadricObj *qobjTex, *qobj;

    /* Shadow casters */
    Occluder Occluders[NUM_OCC];
    float shadow_light_position[4];
    int shadowsInitialised;
    int fBasePreRendered;
    int fBuffers;
    int shadowsOutofDate;

    float ***boardPoints;       /* Used for rounded corners */
#ifdef WIN32
    HANDLE wglBuffer;
#endif

    /* Textures */
#define MAX_TEXTURES 10
    Texture textureList[MAX_TEXTURES];
    char *textureName[MAX_TEXTURES];
    int numTextures;
    unsigned int dotTexture;    /* Holds texture used to draw dots on dice */
};

struct _Flag3d {
    /* Define nurbs surface - for flag */
    GLUnurbsObj *flagNurb;
#define S_NUMPOINTS 4
#define S_NUMKNOTS (S_NUMPOINTS * 2)
#define T_NUMPOINTS 2
#define T_NUMKNOTS (T_NUMPOINTS * 2)
    /* Control points for the flag. The Z values are modified to make it wave */
    float ctlpoints[S_NUMPOINTS][T_NUMPOINTS][3];
};

/* Define relative sizes of objects from arbitrary unit .05 */
#define base_unit .05f

/* Piece/point size */
#define PIECE_HOLE (base_unit * 3.0f)
#define PIECE_DEPTH base_unit

/* Scale textures by this amount */
#define TEXTURE_SCALE (10.0f / base_unit)

#define copyPoint(to, from) memcpy(to, from, sizeof(float[3]))

#define TEXTURE_PATH "textures/"
#define NO_TEXTURE_STRING _("No texture")

#define HINGE_SEGMENTS 6.f

#define CUBE_TWODIGIT_FACTOR .9f

/* Draw board parts (boxes) specially */
enum { /*boxType */ BOX_ALL = 0, BOX_NOSIDES = 1, BOX_NOENDS = 2, BOX_SPLITTOP = 4, BOX_SPLITWIDTH = 8 };

struct _ClipBox {
    float x;
    float y;
    float xx;
    float yy;
};
/* functions used in test harness (static in gnubg) */
#ifndef TEST_HARNESS
#define NTH_STATIC static
#else
#define NTH_STATIC extern
extern void TestHarnessDraw(const BoardData * bd);
#endif

/* Some 3d functions */
extern float getDiceSize(const renderdata * prd);
extern void SetupFlag(void);
extern void setupDicePaths(const BoardData * bd, Path dicePaths[2], float diceMovingPos[2][3],
                           DiceRotation diceRotation[2]);
extern void waveFlag(float wag);
extern GdkGLConfig *getGlConfig(void);

/* Helper functions in misc3d */
void cylinder(float radius, float height, unsigned int accuracy, const Texture * texture);
void circle(float radius, float height, unsigned int accuracy);
void circleRev(float radius, float height, unsigned int accuracy);
void circleTex(float radius, float height, unsigned int accuracy, const Texture * texture);
void circleRevTex(float radius, float height, unsigned int accuracy, const Texture * texture);
void circleOutlineOutward(float radius, float height, unsigned int accuracy);
void circleOutline(float radius, float height, unsigned int accuracy);
void circleSloped(float radius, float startHeight, float endHeight, unsigned int accuracy);
void drawBox(int boxType, float x, float y, float z, float w, float h, float d, const Texture * texture);
void drawCube(float size);
void drawRect(float x, float y, float z, float w, float h, const Texture * texture);
void drawSplitRect(float x, float y, float z, float w, float h, const Texture * texture);
void drawChequeredRect(float x, float y, float z, float w, float h, int across, int down, const Texture * texture);
void QuarterCylinder(float radius, float len, unsigned int accuracy, const Texture * texture);
void QuarterCylinderSplayed(float radius, float len, unsigned int accuracy, const Texture * texture);
void QuarterCylinderSplayedRev(float radius, float len, unsigned int accuracy, const Texture * texture);
void drawCornerEigth(float **const *boardPoints, float radius, unsigned int accuracy);
void calculateEigthPoints(float ****boardPoints, float radius, unsigned int accuracy);
void drawBoardTop(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd);
#ifdef WIN32
void drawBasePreRender(const BoardData * bd, const BoardData3d * bd3d, const renderdata * prd);
#endif

/* Other functions */
void initPath(Path * p, const float start[3]);
void addPathSegment(Path * p, PathType type, const float point[3]);
void initDT(diceTest * dt, int x, int y, int z);

#endif
