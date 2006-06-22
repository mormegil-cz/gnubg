/*
* inc3d.h
* by Jon Kinsey, 2003
*
* General definitions for 3d board
*
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
#ifndef _INC3D_H_
#define _INC3D_H_

#include <stdio.h>
#include <gtk/gtk.h>

#include "model.h"


#if defined(WIN32) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
/* MS gl.h needs windows.h to be included first */
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>

#if !defined(WIN32)
/* x-windows include */
#include <GL/glx.h>
#endif

typedef struct _OGLFont
{
	int glyphs;
	int advance;
	int kern[10][10];
	float scale;
	float height;
} OGLFont;

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

typedef enum _OcculderType {
	OCC_BOARD, OCC_CUBE, OCC_DICE1, OCC_DICE2, OCC_FLAG, OCC_HINGE1, OCC_HINGE2, OCC_PIECE
} OcculderType;
#define LAST_PIECE (OCC_PIECE + 29)

#define NUM_OCC (LAST_PIECE + 1)

typedef enum _displaytype {
    DT_2D, DT_3D
} displaytype;

typedef enum _lighttype {
    LT_POSITIONAL, LT_DIRECTIONAL
} lighttype;

typedef struct _Texture
{
	int texID;
	int width;
	int height;
} Texture;

#define FILENAME_SIZE 15
#define NAME_SIZE 20

typedef enum _TextureType
{
	TT_NONE = 1, TT_GENERAL = 2, TT_PIECE = 4, TT_HINGE = 8, TT_DISABLED = 16, TT_COUNT = 3
} TextureType;

typedef struct _TextureInfo
{
	char file[FILENAME_SIZE + 1];
	char name[NAME_SIZE + 1];
	TextureType type;
} TextureInfo;

typedef struct _Material
{
	float ambientColour[4];
	float diffuseColour[4];
	float specularColour[4];
	int shine;
	int alphaBlend;
	TextureInfo* textureInfo;
	Texture* pTexture;
} Material;

typedef enum _PieceType
{
	PT_ROUNDED, PT_FLAT, NUM_PIECE_TYPES
} PieceType;

typedef enum _PieceTextureType
{
	PTT_TOP, PTT_ALL, NUM_TEXTURE_TYPES
} PieceTextureType;

extern void FindTexture(TextureInfo** textureInfo, char* file);
extern void FindNamedTexture(TextureInfo** textureInfo, char* name);
extern int MaterialCompare(Material* pMat1, Material* pMat2);

typedef struct _BoardData3d
{
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

	OGLFont numberFont, cubeFont;	/* OpenGL fonts */

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
} BoardData3d;

/* Define relative sizes of objects from arbitrary unit .05 */
#define base_unit .05f

/* Piece/point size */
#define PIECE_HOLE (base_unit * 3.0f)
#define PIECE_DEPTH base_unit

/* Scale textures by this amount */
#define TEXTURE_SCALE (10.0f / base_unit)

#define copyPoint(to, from) memcpy(to, from, sizeof(float[3]))
#define SGN(x) (x / abs(x))

#define TEXTURE_PATH "textures/"
#define NO_TEXTURE_STRING _("No texture")

#define HINGE_SEGMENTS 6

/* Draw board parts (boxes) specially */
typedef enum _boxType
{
	BOX_ALL = 0, BOX_NOSIDES = 1, BOX_NOENDS = 2, BOX_SPLITTOP = 4, BOX_SPLITWIDTH = 8
} boxType;

/* Work out which sides of dice to draw */
typedef struct _diceTest
{
	int top, bottom, side[4];
} diceTest;

#endif
