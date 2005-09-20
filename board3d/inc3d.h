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

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

#include <stdio.h>
#include "i18n.h"

#if USE_GTK
#include <gtk/gtk.h>
#endif

#ifdef USE_MSDEV_TEST_HARNESS
	#define USE_GLUT
	#define USE_GLUT_FONT

	#define gtk_main_quit() 0
	#define gtk_main() 0
	#define g_print(a) 0
#endif

#include "eval.h"
#include "gtkboard.h"
#include "drawboard.h"
#include "boardpos.h"
#include "analysis.h"

/* Define relative sizes of objects from arbitrary unit .05 */
#define base_unit .05f

/* Piece/point size */
#define PIECE_HOLE (base_unit * 3.0f)
#define PIECE_DEPTH base_unit

/* Scale textures by this amount */
#define TEXTURE_SCALE (10.0f / base_unit)

#define PI 3.14159265358979323846f
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

/* Setup functions */
void InitGL(BoardData *bd);

/* font functions */
#ifndef __cplusplus
void glPrintPointNumbers(BoardData* bd, const char *text);
void glPrintCube(BoardData* bd, const char *text);
void glPrintNumbersRA(BoardData* bd, const char *text);
float getFontHeight(BoardData* bd);
void BuildFont(BoardData* bd);
#endif

/* Drawing functions */
void drawBoard(BoardData* bd);
void SetupPerspVolume(BoardData* bd, int viewport[4]);
float getBoardWidth();
float getBoardHeight();
void calculateBackgroundSize(BoardData *bd, int viewport[4]);

typedef struct _ClipBox
{
	float x;
	float y;
	float xx;
	float yy;
} ClipBox;

#define MAX_FRAMES 10
extern ClipBox cb[MAX_FRAMES];
extern int numRestrictFrames;

void RestrictiveRender(BoardData *bd);
void RestrictiveDrawFrame(float pos[3], float width, float height, float depth);
void RestrictiveDraw(ClipBox* pCb, float pos[3], float width, float height, float depth);
void EnlargeCurrentToBox(ClipBox* pOtherCb);
void RestrictiveDrawFlag(BoardData* bd);

extern void getPiecePos(int point, int pos, int swap, float v[3]);

/* Clipping planes */
#define zNear .1f
#define zFar 70

/* Graph functions*/
typedef struct _GraphData
{
	float ***data;
	int numGames;
	float maxY;
} GraphData;

GtkWidget* StatGraph(GraphData* pgd);
void SetNumGames(GraphData* pgd, int numGames);
void AddGameData(GraphData* pgd, int game, statcontext *psc);
void TidyGraphData(GraphData* pgd);

/* Misc functions */
void SetTexture(BoardData* bd, Material* pMat, const char* filename, TextureFormat format);
void GetTexture(BoardData* bd, Material* pMat);
void SetupSimpleMatAlpha(Material* pMat, float r, float g, float b, float a);
void SetupSimpleMat(Material* pMat, float r, float g, float b);
void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a);
void setMaterial(Material* pMat);
void SetColour(float r, float g, float b, float a);
unsigned char *LoadDIBTexture(FILE* fp, int *width, int *height);
unsigned char *LoadPNGTexture(FILE *fp, int *width, int *height);
float randRange(float range);
void setupPath(BoardData *bd, Path* p, float* pRotate, int fClockwise, int fromPoint, int fromDepth, int toPoint, int toDepth);
int movePath(Path* p, float d, float* pRotate, float v[3]);
int finishedPath(Path* p);
void updateHingeOccPos(BoardData* bd);
void getProjectedPieceDragPos(int x, int y, float pos[3]);
void updateMovingPieceOccPos(BoardData* bd);
void LoadTextureInfo(int FirstPass);
GList *GetTextureList(int type);
int IsSet(int flags, int bit);
float Dist2d(float a, float b);
float ***Alloc3d(int x, int y, int z);
void Free3d(float ***array, int x, int y);
int LoadTexture(Texture* texture, const char* Filename, TextureFormat format);
void CheckOpenglError();

typedef int idleFunc(BoardData* bd);
void setIdleFunc(BoardData* bd, idleFunc* pFun);

#endif
