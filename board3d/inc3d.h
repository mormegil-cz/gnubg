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

#include "config.h"
#include <gtk/gtk.h>

//#undef USE_GTK
#if USE_GTK
	#define BUILDING_LIB 1

	#include "eval.h"

#else
	#define USE_GLUT

	#define AlphaBlend ab
	/* Comment out next line to switch test harness off */
	#define TEST
	/* Comment out next line to remove glut library (needed for debug font) */
	#define USE_GLUT_FONT

	#include "eval.h"
	#include "renderprefs.h"
	#define g_print(a)     ((void)0)
#endif

#include "gtkboard.h"

#define SGN(x) (x / abs(x))

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

/* Draw board parts specially */
typedef enum _boxType
{
	BOX_ALL = 0, BOX_NOSIDES = 1, BOX_NOENDS = 2, BOX_SPLITTOP = 4, BOX_SPLITWIDTH = 8
} boxType;

/* Work out which sides of dice to draw */
typedef struct _diceTest
{
	int top, bottom, side[4];
} diceTest;

/* Define relative sizes of objects from arbitrary unit .05 */
#define base_unit .05f

/* Scale textures by this amount */
#define TEXTURE_SCALE (10.0f / base_unit)

#include "boardpos.h"

#define PI 3.14159265358979323846f

#define copyPoint(to, from)	memcpy(to, from, sizeof(float[3]))

extern int fClockwise; /* Player 1 moves clockwise */
extern int fGUIDiceArea; /* Show dice below board */

/* Setup functions */
void InitBoard3d(BoardData *bd);
void InitialPos(BoardData *bd);
void EmptyPos(BoardData *bd);
void CloseBoard(BoardData* bd);
void TidyBoard(BoardData *bd);
void InitGL();

/* Drawing functions */
void drawBoard(BoardData* bd);
void SetupPerspVolume(BoardData* bd, int viewport[4]);
float getBoardWidth();
float getBoardHeight();
void calculateBackgroundSize(BoardData *bd, int viewport[4]);

// Misc functions
void SetupSimpleMatAlpha(Material* pMat, float r, float g, float b, float a);
void SetupSimpleMat(Material* pMat, float r, float g, float b);
void setMaterial(Material* pMat);
void SetColour(float r, float g, float b, float a);
unsigned char *LoadDIBitmap(const char *filename, int *width, int *height);
int DiceBelowBoard(BoardData *bd);
float randRange(float range);
void setupPath(BoardData *bd, Path* p, float* pRotate, int fClockwise, int fromPoint, int fromDepth, int toPoint, int toDepth);
int movePath(Path* p, float d, float* pRotate, float v[3]);
int finishedPath(Path* p);
void updateHingeOccPos(BoardData* bd);
double get_time();
void getProjectedPieceDragPos(int x, int y, float pos[3]);
void updateMovingPieceOccPos(BoardData* bd);
void updateDiceOccPos(BoardData *bd);
void setDicePos(BoardData* bd);

typedef int idleFunc(BoardData* bd);

void setIdleFunc(BoardData* bd, idleFunc* pFun);
