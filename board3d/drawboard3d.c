/*
* drawboard3d.c
* by Jon Kinsey, 2003
*
* 3d board drawing code
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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include "matrix.h"
#include "shadow.h"
#include "renderprefs.h"

/* Used to calculate correct viewarea for board/fov angles */
typedef struct _viewArea
{
	float top;
	float bottom;
	float width;
} viewArea;

// My logcube - more than 32 then return 0 (show 64)
static int LogCube( const int n )
{
	int i;

	for( i = 0; ; i++ )
	if( n <= ( 1 << i ) )
		return i < 6 ? i : 0;
}

/* Helper functions in misc3d */
void circle(float radius, float height, int accuracy);
void circleOutline(float radius, float height, int accuracy);
void circleRev(float radius, float height, int accuracy);
void drawBox(boxType type, float x, float y, float z, float w, float h, float d, Texture* texture);
void drawCube(float size);
void drawRect(float x, float y, float z, float w, float h, Texture* texture);
void drawSplitRect(float x, float y, float z, float w, float h, Texture* texture);

/* font functions */
void glPrintPointNumbers(BoardData* bd, const char *text, int mode);
void glPrintCube(BoardData* bd, const char *text, int mode);
float getFontHeight(BoardData* bd);
void KillFont(BoardData* bd);

/* Other functions */
void initPath(Path* p, float start[3]);
void addPathSegment(Path* p, PathType type, float point[3]);
void initDT(diceTest* dt, int x, int y, int z);
float ***Alloc3d(int x, int y, int z);
void Free3d(float ***array, int x, int y);

/* Clipping planes */
#define zNear .1f
#define zFar 70

/* All the board element sizes - based on base_unit size */

/* Widths */

/* Side edge width of bearoff trays */
#define EDGE_WIDTH base_unit
/* Piece/point size */
#define PIECE_HOLE (base_unit * 3.0f)

#define TRAY_WIDTH (EDGE_WIDTH * 2.0f + PIECE_HOLE)
#define BOARD_WIDTH (PIECE_HOLE * 6.0f)
#define BAR_WIDTH (PIECE_HOLE * 1.7f)
#define DICE_AREA_CLICK_WIDTH BOARD_WIDTH

#define TOTAL_WIDTH ((TRAY_WIDTH + BOARD_WIDTH) * 2.0f + BAR_WIDTH)

/* Heights */

/* Bottom + top edge */
#define EDGE_HEIGHT (base_unit * 1.5f)

#define POINT_HEIGHT (PIECE_HOLE * 6)
#define TRAY_HEIGHT (EDGE_HEIGHT + POINT_HEIGHT)
#define MID_SIDE_GAP_HEIGHT (base_unit * 3.5f)
#define DICE_AREA_HEIGHT MID_SIDE_GAP_HEIGHT
/* Vertical gap between pieces */
#define PIECE_GAP_HEIGHT (base_unit / 5.0f)

#define TOTAL_HEIGHT (TRAY_HEIGHT * 2.0f + MID_SIDE_GAP_HEIGHT)

/* Depths */

#define EDGE_DEPTH (base_unit * 1.95f)
#define PIECE_DEPTH base_unit
#define BASE_DEPTH base_unit

/* Other objects */

#define DICE_SIZE (base_unit * 3)
#define DOT_SIZE (DICE_SIZE / 14.0f)
#define DOUBLECUBE_SIZE (base_unit * 4.0f)

/* Dice animation step size */
#define DICE_STEP_SIZE0 (base_unit * 1.3f)
#define DICE_STEP_SIZE1 (base_unit * 1.7f)

#define HINGE_GAP (base_unit / 12.0f)
#define HINGE_WIDTH (base_unit / 2.0f)
#define HINGE_HEIGHT (base_unit * 7.0f)

#undef ARROW_SIZE
#define ARROW_SIZE (EDGE_HEIGHT * .8f)

#define FLAG_HEIGHT (base_unit * 3)
#define FLAG_WIDTH (FLAG_HEIGHT * 1.4f)
#define FLAG_WAG (FLAG_HEIGHT * .3f)
#define FLAGPOLE_WIDTH (base_unit * .2f)
#define FLAGPOLE_HEIGHT (FLAG_HEIGHT * 2.05f)

/* Slight offset from surface - avoid using unless necessary */
#define LIFT_OFF (base_unit / 50.0f)

float getBoardWidth() {return TOTAL_WIDTH;}
float getBoardHeight() {return TOTAL_HEIGHT;}

void TidyShadows(BoardData* bd)
{
	freeOccluder(&bd->Occluders[OCC_BOARD]);
	freeOccluder(&bd->Occluders[OCC_CUBE]);
	freeOccluder(&bd->Occluders[OCC_DICE1]);
	freeOccluder(&bd->Occluders[OCC_FLAG]);
	freeOccluder(&bd->Occluders[OCC_HINGE1]);
	freeOccluder(&bd->Occluders[OCC_PIECE]);
}

void Tidy3dObjects(BoardData* bd, int glValid)
{
	if (glValid)
	{
		KillFont(bd);
		glDeleteLists(bd->pieceList, 1);
		glDeleteLists(bd->diceList, 1);
		glDeleteLists(bd->DCList, 1);

		gluDeleteQuadric(bd->qobjTex);
		gluDeleteQuadric(bd->qobj);

		gluDeleteNurbsRenderer(bd->flagNurb);
	}

	TidyShadows(bd);

	ClearTextures(bd, glValid);
}

void preDrawPiece0(BoardData* bd)
{
	int i, j;
	float angle, angle2, step;

	float latitude;
	float new_radius;

	float radius = PIECE_HOLE / 2.0f;
	float discradius = radius * 0.8f;
	float lip = radius - discradius;
	float height = PIECE_DEPTH - 2 * lip;
	float ***p = Alloc3d(bd->curveAccuracy + 1, bd->curveAccuracy / 4 + 1, 3);
	float ***n = Alloc3d(bd->curveAccuracy + 1, bd->curveAccuracy / 4 + 1, 3);

	step = (2 * PI) / bd->curveAccuracy;

	/* Draw top/bottom of piece */
	if (bd->chequerMat[0].pTexture)
	{	/* Texturing will be enabled */
		glPushMatrix();
		glTranslatef(0, 0, PIECE_DEPTH);
		glBindTexture(GL_TEXTURE_2D, bd->chequerMat[0].pTexture->texID);
		gluDisk(bd->qobjTex, 0, discradius, bd->curveAccuracy, 1);
		glPopMatrix();
		/* Draw bottom - faces other way */
		gluQuadricOrientation(bd->qobjTex, GLU_INSIDE);
		gluDisk(bd->qobjTex, 0, discradius, bd->curveAccuracy, 1);
		gluQuadricOrientation(bd->qobjTex, GLU_OUTSIDE);
		glDisable(GL_TEXTURE_2D);
	}
	else	
	{
		circleRev(discradius, 0, bd->curveAccuracy);
		circle(discradius, PIECE_DEPTH, bd->curveAccuracy);
	}
	/* Draw side of piece */
	angle = 0;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < bd->curveAccuracy + 1; i++)
	{
		glNormal3f((float)sin(angle), (float)cos(angle), 0);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, lip);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, lip + height);

		angle += step;
	}
	glEnd();

	/* Draw edges of piece */
	angle2 = 0;
	for (j = 0; j <= bd->curveAccuracy / 4; j++)
	{
		latitude = (float)sin(angle2) * lip;
		angle = 0;
		new_radius = (float)sqrt((lip * lip) - (latitude * latitude));

		for (i = 0; i < bd->curveAccuracy; i++)
		{
			n[i][j][0] = (float)sin(angle) * new_radius;
			p[i][j][0] = (float)sin(angle) * (discradius + new_radius);
			n[i][j][1] = (float)cos(angle) * new_radius;
			p[i][j][1] = (float)cos(angle) * (discradius + new_radius);
			p[i][j][2] = latitude + lip + height;
			n[i][j][2] = latitude;

			angle += step;
		}
		p[i][j][0] = p[0][j][0];
		p[i][j][1] = p[0][j][1];
		p[i][j][2] = p[0][j][2];
		n[i][j][0] = n[0][j][0];
		n[i][j][1] = n[0][j][1];
		n[i][j][2] = n[0][j][2];

		angle2 += step;
	}

	for (j = 0; j < bd->curveAccuracy / 4; j++)
	{
		glBegin(GL_QUAD_STRIP);
		for (i = 0; i < bd->curveAccuracy + 1; i++)
		{
			glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
			glVertex3f(p[i][j][0], p[i][j][1], p[i][j][2]);
			glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
			glVertex3f(p[i][j + 1][0], p[i][j + 1][1], p[i][j + 1][2]);
		}
		glEnd();
			
		glBegin(GL_QUAD_STRIP);
		for (i = 0; i < bd->curveAccuracy + 1; i++)
		{
			glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
			glVertex3f(p[i][j + 1][0], p[i][j + 1][1], PIECE_DEPTH - p[i][j + 1][2]);
			glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
			glVertex3f(p[i][j][0], p[i][j][1], PIECE_DEPTH - p[i][j][2]);
		}
		glEnd();
	}

	if (bd->chequerMat[0].pTexture)
		glEnable(GL_TEXTURE_2D);	/* Re-enable texturing */

	Free3d(p, bd->curveAccuracy + 1, bd->curveAccuracy / 4 + 1);
	Free3d(n, bd->curveAccuracy + 1, bd->curveAccuracy / 4 + 1);
}

void preDrawPiece1(BoardData* bd)
{
	float pieceRad, pieceBorder;

	pieceRad = PIECE_HOLE / 2.0f;
	pieceBorder = pieceRad * .9f;

	/* Draw top of piece */
	if (bd->chequerMat[0].pTexture)
	{	/* Texturing will be enabled */
		glPushMatrix();
		glTranslatef(0, 0, PIECE_DEPTH);
		glBindTexture(GL_TEXTURE_2D, bd->chequerMat[0].pTexture->texID);
		gluDisk(bd->qobjTex, 0, pieceBorder, bd->curveAccuracy, 1);
		glDisable(GL_TEXTURE_2D);
		gluDisk(bd->qobj, pieceBorder, pieceRad, bd->curveAccuracy, 1);
		glPopMatrix();
	}
	else
	{
		circle(pieceRad, PIECE_DEPTH, bd->curveAccuracy);
	}
	/* Draw plain bottom of piece */
	circleRev(pieceRad, 0, bd->curveAccuracy);

	/* Edge of piece */
	gluCylinder(bd->qobj, pieceRad, pieceRad, PIECE_DEPTH, bd->curveAccuracy, 1);

	if (bd->chequerMat[0].pTexture)
		glEnable(GL_TEXTURE_2D);	/* Re-enable texturing */
}

void preDrawPiece(BoardData* bd, int transparent)
{
	if (bd->pieceList)
		glDeleteLists(bd->pieceList, 1);

	bd->pieceList = glGenLists(1);
	glNewList(bd->pieceList, GL_COMPILE);

	switch(bd->pieceType)
	{
	case PT_ROUNDED:
		preDrawPiece0(bd);
		break;
	case PT_FLAT:
		preDrawPiece1(bd);
		break;
	default:
		g_print("Error: Unhandled piece type\n");
	}

	glEndList();
}

void UnitNormal(float x, float y, float z)
{
	// Calculate the length of the vector		
	float length = (float)sqrt((x * x) + (y * y) + (z * z));

	// Dividing each element by the length will result in a unit normal vector.
	glNormal3f(x / length, y / length, z / length);
}

void renderDice(BoardData* bd, float size)
{
	int ns, lns;
	int i, j;
	int c;
	float lat_angle;
	float lat_step;
	float latitude;
	float new_radius;
	float radius;
	float angle, step;

	int corner_steps = (bd->curveAccuracy / 4) + 1;
	float ***corner_points = Alloc3d(corner_steps, corner_steps, 3);

	radius = size / 2.0f;
	step = (2 * PI) / bd->curveAccuracy;

	glPushMatrix();

	/* Draw 6 faces */
	for (c = 0; c < 6; c++)
	{
		circle(radius, radius, bd->curveAccuracy);

		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}

	lat_angle = 0;
	lns = (bd->curveAccuracy / 4);
	lat_step = (PI / 2) / lns;

	/* Calculate corner points */
	for (i = 0; i < lns + 1; i++)
	{
		latitude = (float)sin(lat_angle) * radius;
		new_radius = (float)sqrt(radius * radius - (latitude * latitude));

		ns = (bd->curveAccuracy / 4) - i;

		step = (PI / 2 - lat_angle) / (ns);
		angle = 0;

		for (j = 0; j <= ns; j++)
		{
			corner_points[i][j][0] = (float)cos(lat_angle) * radius;
			corner_points[i][j][1] = (float)cos(angle) * radius;
			corner_points[i][j][2] = (float)sin(angle + lat_angle) * radius;

			angle += step;
		}
		lat_angle += lat_step;
	}

	radius *= (float)sqrt(2);	/* 3d radius */
	/* Draw 8 corners */
	for (c = 0; c < 8; c++)
	{
		glPushMatrix();

		glRotatef((float)(c * 90), 0, 0, 1);

		for (i = 0; i < bd->curveAccuracy / 4; i++)
		{
			ns = (bd->curveAccuracy / 4) - i - 1;

			glBegin(GL_TRIANGLE_STRIP);
				UnitNormal(corner_points[i][0][0], corner_points[i][0][1], corner_points[i][0][2]);
				glVertex3f(corner_points[i][0][0], corner_points[i][0][1], corner_points[i][0][2]);
				for (j = 0; j <= ns; j++)
				{
					UnitNormal(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
					glVertex3f(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
					UnitNormal(corner_points[i][j + 1][0], corner_points[i][j + 1][1], corner_points[i][j + 1][2]);
					glVertex3f(corner_points[i][j + 1][0], corner_points[i][j + 1][1], corner_points[i][j + 1][2]);
				}
			glEnd();
		}

		glPopMatrix();
		if (c == 3)
			glRotatef(180, 1, 0, 0);
	}
	glPopMatrix();

	Free3d(corner_points, corner_steps, corner_steps);
}

void renderCube(BoardData* bd, float size)
{
	int ns;
	int i, j;
	int c;
	float lat_angle;
	float lat_step;
	float latitude;
	float new_radius;
	float radius;
	float angle, step;

	float ds = (size * 5.0f / 7.0f);
	float hds = (ds / 2);

	int corner_steps = (bd->curveAccuracy / 4) + 1;
	float ***corner_points = Alloc3d(corner_steps, corner_steps, 3);

	radius = size / 7.0f;

	step = (2 * PI) / bd->curveAccuracy;

	glPushMatrix();

	/* Draw 6 faces */
	for (c = 0; c < 6; c++)
	{
		glPushMatrix();
		glTranslatef(0, 0, hds + radius);

		glNormal3f(0, 0, 1);

		glBegin(GL_QUADS);
			glVertex3f(-hds, -hds, 0);
			glVertex3f(hds, -hds, 0);
			glVertex3f(hds, hds, 0);
			glVertex3f(-hds, hds, 0);
		glEnd();

		/* Draw 12 edges */
		for (i = 0; i < 2; i++)
		{
			glPushMatrix();
			glRotatef((float)(i * 90), 0, 0, 1);

			glTranslatef(hds, -hds, -radius);

			angle = 0;
			glBegin(GL_QUAD_STRIP);
			for (j = 0; j < bd->curveAccuracy / 4 + 1; j++)
			{
				glNormal3f((float)sin(angle), 0, (float)cos(angle));
				glVertex3f((float)sin(angle) * radius, ds, (float)cos(angle) * radius);
				glVertex3f((float)sin(angle) * radius, 0, (float)cos(angle) * radius);

				angle += step;
			}
				glEnd();
			glPopMatrix();
		}
		glPopMatrix();
		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}

	lat_angle = 0;
	lat_step = (2 * PI) / bd->curveAccuracy;

	/* Calculate corner 1/8th sphere points */
	for (i = 0; i < (bd->curveAccuracy / 4) + 1; i++)
	{
		latitude = (float)sin(lat_angle) * radius;
		angle = 0;
		new_radius = (float)sqrt(radius * radius - (latitude * latitude) );

		ns = (bd->curveAccuracy / 4) - i;
		step = (2 * PI) / (ns * 4);

		for (j = 0; j <= ns; j++)
		{
			corner_points[i][j][0] = (float)sin(angle) * new_radius;
			corner_points[i][j][1] = latitude;
			corner_points[i][j][2] = (float)cos(angle) * new_radius;

			angle += step;
		}
		lat_angle += lat_step;
	}

	/* Draw 8 corners */
	for (c = 0; c < 8; c++)
	{
		glPushMatrix();
		glTranslatef(0, 0, hds + radius);

		glRotatef((float)(c * 90), 0, 0, 1);

		glTranslatef(hds, -hds, -radius);
		glRotatef(-90, 0, 0, 1);

		for (i = 0; i < bd->curveAccuracy / 4; i++)
		{
			ns = (bd->curveAccuracy / 4) - i - 1;
			glBegin(GL_TRIANGLE_STRIP);
				glNormal3f(corner_points[i][ns + 1][0] / radius, corner_points[i][ns + 1][1] / radius, corner_points[i][ns + 1][2] / radius);
				glVertex3f(corner_points[i][ns + 1][0], corner_points[i][ns + 1][1], corner_points[i][ns + 1][2]);
				for (j = ns; j >= 0; j--)
				{
					glNormal3f(corner_points[i + 1][j][0] / radius, corner_points[i + 1][j][1] / radius, corner_points[i + 1][j][2] / radius);
					glVertex3f(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
					glNormal3f(corner_points[i][j][0] / radius, corner_points[i][j][1] / radius, corner_points[i][j][2] / radius);
					glVertex3f(corner_points[i][j][0], corner_points[i][j][1], corner_points[i][j][2]);
				}
			glEnd();
		}

		glPopMatrix();
		if (c == 3)
			glRotatef(180, 1, 0, 0);
	}
	glPopMatrix();

	Free3d(corner_points, corner_steps, corner_steps);
}

void preDrawDice(BoardData* bd)
{
	if (bd->diceList)
		glDeleteLists(bd->diceList, 1);

	bd->diceList = glGenLists(1);
	glNewList(bd->diceList, GL_COMPILE);
		renderDice(bd, DICE_SIZE);
	glEndList();

	if (bd->DCList)
		glDeleteLists(bd->DCList, 1);

	bd->DCList = glGenLists(1);
	glNewList(bd->DCList, GL_COMPILE);
		renderCube(bd, DOUBLECUBE_SIZE);
	glEndList();
}

void getDoubleCubePos(BoardData* bd, float v[3])
{
	if (bd->doubled != 0)
	{
		v[0] = TRAY_WIDTH + BOARD_WIDTH / 2;
		if (bd->doubled != 1)
			v[0] = TOTAL_WIDTH - v[0];

		v[1] = TOTAL_HEIGHT / 2.0f;
		v[2] = BASE_DEPTH + DOUBLECUBE_SIZE / 2.0f;
	}
	else
	{
		v[0] = TOTAL_WIDTH / 2.0f;
		switch(bd->cube_owner)
		{
		case 0:
			v[1] = TOTAL_HEIGHT / 2.0f;
			break;
		case -1:
			v[1] = EDGE_HEIGHT + DOUBLECUBE_SIZE / 2.0f;
			break;
		case 1:
			v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - DOUBLECUBE_SIZE / 2.0f;
			break;
		default:
		  v[1] = 0;	/* error */
		}
		v[2] = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE / 2.0f;
	}
}

void moveToDoubleCubePos(BoardData* bd)
{
	float v[3];
	getDoubleCubePos(bd, v);
	glTranslatef(v[0], v[1], v[2]);
}

void drawDCNumbers(BoardData* bd, int mode, diceTest* dt)
{
	int c, nice;
	float radius = DOUBLECUBE_SIZE / 7.0f;
	float ds = (DOUBLECUBE_SIZE * 5.0f / 7.0f);
	float hds = (ds / 2);
	float depth = hds + radius;

	char* sides[] = {"4", "16", "32", "64", "8", "2"};
	int side;

	glPushMatrix();
	for (c = 0; c < 6; c++)
	{
		if (c < 3)
			side = c;
		else
			side = 8 - c;
		/* Nicer top numbers */
		nice = (side == dt->top);

		/* Don't draw bottom number or back number */
		if (side == dt->top || side == dt->side[2] ||
			(bd->doubled && (side == dt->side[1] || side == dt->side[3]))
			|| (bd->cube_owner == -1 && side == dt->side[0]))
		{
			if (nice)
				glDisable(GL_DEPTH_TEST);

			glPushMatrix();
			glTranslatef(0, 0, depth + !nice * LIFT_OFF * 2);

			glPrintCube(bd, sides[side], mode);

			glPopMatrix();
			if (nice)
				glEnable(GL_DEPTH_TEST);
		}
		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}
	glPopMatrix();
}

void DrawDCNumbers(BoardData* bd)
{
	diceTest dt;
	int extraRot = 0;
	int rotDC[6][3] = {{1, 0, 0}, {2, 0, 3}, {0, 0, 0}, {0, 3, 1}, {0, 1, 0}, {3, 0, 3}};

	int cubeIndex;
	/* Rotate to correct number + rotation */
	if (!bd->doubled)
	{
		cubeIndex = LogCube(bd->cube);
		extraRot = bd->cube_owner + 1;
	}
	else
	{
		cubeIndex = LogCube(bd->cube * 2);	/* Show offered cube value */
		extraRot = bd->turn + 1;
	}

	glRotatef((rotDC[cubeIndex][2] + extraRot) * 90.0f, 0, 0, 1);
	glRotatef(rotDC[cubeIndex][0] * 90.0f, 1, 0, 0);
	glRotatef(rotDC[cubeIndex][1] * 90.0f, 0, 1, 0);

	initDT(&dt, rotDC[cubeIndex][0], rotDC[cubeIndex][1], rotDC[cubeIndex][2] + extraRot);

	setMaterial(&bd->cubeNumberMat);
	glNormal3f(0, 0, 1);

	/* Draw inside then anti-aliased outline of numbers */
	drawDCNumbers(bd, 0, &dt);

	glLineWidth(.5f);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	drawDCNumbers(bd, 1, &dt);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void drawDC(BoardData* bd)
{
	glPushMatrix();
	moveToDoubleCubePos(bd);

	setMaterial(&bd->cubeMat);
	glCallList(bd->DCList);

	DrawDCNumbers(bd);

	glPopMatrix();
}

/* Define position of dots on dice */
int dots1[] = {2, 2, 0};
int dots2[] = {1, 1, 3, 3, 0};
int dots3[] = {1, 3, 2, 2, 3, 1, 0};
int dots4[] = {1, 1, 1, 3, 3, 1, 3, 3, 0};
int dots5[] = {1, 1, 1, 3, 2, 2, 3, 1, 3, 3, 0};
int dots6[] = {1, 1, 1, 3, 2, 1, 2, 3, 3, 1, 3, 3, 0};
int *dots[6] = {dots1, dots2, dots3, dots4, dots5, dots6};
int dot_pos[] = {0, 20, 50, 80};	/* percentages across face */

void drawDots(BoardData* bd, float dotOffset, diceTest* dt, int showFront, int drawOutline)
{
	int dot;
	int c, nd;
	int* dp;
	float radius;
	float ds = (DICE_SIZE * 5.0f / 7.0f);
	float hds = (ds / 2);
	float x, y;

	radius = DICE_SIZE / 7.0f;

	glPushMatrix();
	for (c = 0; c < 6; c++)
	{
		if (c < 3)
			dot = c;
		else
			dot = 8 - c;

		/* Make sure top dot looks nice */
		nd = !bd->shakingDice && (dot == dt->top);

		if (bd->shakingDice
			|| (showFront && dot != dt->bottom && dot != dt->side[0])
			|| (!showFront && dot != dt->top && dot != dt->side[2]))
		{
			if (nd)
				glDisable(GL_DEPTH_TEST);
			glPushMatrix();
			glTranslatef(0, 0, hds + radius);

			glNormal3f(0, 0, 1);

			/* Show all the dots for this number */
			dp = dots[dot];
			do
			{
				x = (dot_pos[dp[0]] * ds) / 100;
				y = (dot_pos[dp[1]] * ds) / 100;

				glPushMatrix();
				glTranslatef(x - hds, y - hds, 0);

				if (drawOutline)
					circleOutline(DOT_SIZE, dotOffset, bd->curveAccuracy);
				else
					circle(DOT_SIZE, dotOffset, bd->curveAccuracy);

				glPopMatrix();

				dp += 2;
			} while (*dp);

			glPopMatrix();
			if (nd)
				glEnable(GL_DEPTH_TEST);
		}

		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}
	glPopMatrix();
}

void DrawDots(BoardData* bd, diceTest* dt)
{
	/* Outside dots - middle */
	drawDots(bd, LIFT_OFF, dt, 1, 0);

	glLineWidth(0.5f);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	/* Outside dots - smooth edges */
	drawDots(bd, LIFT_OFF, dt, 1, 1);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void getDicePos(BoardData* bd, int num, float v[3])
{
	if (bd->diceShown == DICE_BELOW_BOARD)
	{	/* Show below board */
		v[0] = DICE_SIZE * 1.5f;
		v[1] = -DICE_SIZE / 2.0f;
		v[2] = DICE_SIZE / 2.0f;

		if (bd->turn == 1)
			v[0] += TOTAL_WIDTH - DICE_SIZE * 4;
		if (num == 1)
			v[0] += DICE_SIZE;	/* Place 2nd dice by 1st */
	}
	else
	{
		v[0] = bd->dicePos[num][0];
		if (bd->turn == 1)
			v[0] = TOTAL_WIDTH - v[0];	/* Dice on right side */

		v[1] = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f + bd->dicePos[num][1];
		v[2] = BASE_DEPTH + LIFT_OFF + DICE_SIZE / 2.0f;
	}
}

void moveToDicePos(BoardData* bd, int num)
{
	float v[3];
	getDicePos(bd, num, v);
	glTranslatef(v[0], v[1], v[2]);

	if (bd->diceShown == DICE_ON_BOARD)
	{	/* Spin dice to required rotation if on board */
		glRotatef(bd->dicePos[num][2], 0, 0, 1);
	}
}

void drawDice2(BoardData* bd, int num)
{
	int value;
	int rotDice[6][2] = {{0, 0}, {0, 1}, {3, 0}, {1, 0}, {0, 3}, {2, 0}};
	int diceCol = (bd->turn == 1);
	int blend;
	int z;
	diceTest dt;

	/* Draw dice */
	value = bd->diceRoll[num];
	if (bd->diceShown == DICE_BELOW_BOARD)
		z = 0;
	else
		z = ((int)bd->dicePos[num][2] + 45) / 90;

	value--;	/* Zero based for array access */
	glRotatef(90.0f * rotDice[value][0], 1, 0, 0);
	glRotatef(90.0f * rotDice[value][1], 0, 1, 0);

	if (value >= 0)
		initDT(&dt, rotDice[value][0], rotDice[value][1], z);
	else
	{
		g_print("no value on dice?\n");
		return;
	}
	blend = (bd->diceMat[diceCol].alphaBlend);

	if (blend)
	{	/* Draw back of dice separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);

		setMaterial(&bd->diceMat[diceCol]);
		glCallList(bd->diceList);

		/* Place back dots inside dice */
		setMaterial(&bd->diceDotMat[diceCol]);
		drawDots(bd, -LIFT_OFF, &dt, 0, 0);

		glCullFace(GL_BACK);
	}
	setMaterial(&bd->diceMat[diceCol]);
	glCallList(bd->diceList);

	if (blend)
		glDisable(GL_BLEND);

	/* Draw front dots */
	setMaterial(&bd->diceDotMat[diceCol]);
	DrawDots(bd, &dt);
}

void drawDice(BoardData* bd, int num)
{
	glPushMatrix();
	moveToDicePos(bd, num);
	drawDice2(bd, num);
	glPopMatrix();
}

void getPiecePos(int point, int pos, int swap, float v[3])
{
	if (point == 0 || point == 25)
	{	/* bars */
		v[0] = TOTAL_WIDTH / 2.0f;
		v[1] = TOTAL_HEIGHT / 2.0f;
		v[2] = BASE_DEPTH + EDGE_DEPTH + ((pos - 1) / 3) * PIECE_DEPTH;
		pos = ((pos - 1) % 3) + 1;

		if (point == 25)
		{
			v[1] -= DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos + .5f);
		}
		else
		{
			v[1] += DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos + .5f);
		}
		v[1] -= PIECE_HOLE / 2.0f;
	}
	else if (point >= 26)
	{	/* homes */
		v[2] = BASE_DEPTH;
		if (swap)
			v[0] = TRAY_WIDTH / 2.0f;
		else
			v[0] = TOTAL_WIDTH - TRAY_WIDTH / 2.0f;

		if (point == 26)
			v[1] = EDGE_HEIGHT + (PIECE_DEPTH * 1.2f * (pos - 1));	/* 1.3 gives a gap between pieces */
		else
			v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - PIECE_DEPTH - (PIECE_DEPTH * 1.2f * (pos - 1));
	}
	else
	{
		v[2] = BASE_DEPTH + ((pos - 1) / 5) * PIECE_DEPTH;

		if (point < 13)
		{
			if (swap)
				point = 13 - point;

			if (pos > 10)
				pos -= 10;

			if (pos > 5)
				v[1] = EDGE_HEIGHT + (PIECE_HOLE / 2.0f) + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 5 - 1);
			else
				v[1] = EDGE_HEIGHT + (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 1);

			v[0] = TRAY_WIDTH + PIECE_HOLE * (12 - point);
			if (point < 7)
				v[0] += BAR_WIDTH;
		}
		else
		{
			if (swap)
				point = (24 + 13) - point;

			if (pos > 10)
				pos -= 10;

			if (pos > 5)
				v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - (PIECE_HOLE / 2.0f) - (PIECE_HOLE + PIECE_GAP_HEIGHT) * (pos - 5);
			else
				v[1] = TOTAL_HEIGHT - EDGE_HEIGHT - (PIECE_HOLE + PIECE_GAP_HEIGHT) * pos;

			v[0] = TRAY_WIDTH + PIECE_HOLE * (point - 13);
			if (point > 18)
				v[0] += BAR_WIDTH;
		}
		v[0] += PIECE_HOLE / 2.0f;
	}
	v[2] += LIFT_OFF * 2;

	/* Move to centre of piece */
	if (point < 26)
	{
		v[1] += PIECE_HOLE / 2.0f;
	}
	else
	{	/* Home pieces are sideways */
		if (point == 27)
			v[1] += PIECE_DEPTH;
		v[2] += PIECE_HOLE / 2.0f;
	}
}

void renderPiece(BoardData* bd, int rotation, int colour)
{
	glRotatef((float)rotation, 0, 0, 1);
	setMaterial(&bd->chequerMat[colour]);

	glCallList(bd->pieceList);
}

void renderSpecialPieces(BoardData* bd)
{
	if (bd->drag_point >= 0)
	{
		glPushMatrix();
		glTranslated(bd->dragPos[0], bd->dragPos[1], bd->dragPos[2]);
		renderPiece(bd, bd->movingPieceRotation, (bd->drag_colour == 1));
		glPopMatrix();
	}

	if (bd->moving)
	{
		glPushMatrix();
		glTranslatef(bd->movingPos[0], bd->movingPos[1], bd->movingPos[2]);
		if (bd->rotateMovingPiece > 0)
			glRotatef(-90 * bd->rotateMovingPiece * bd->turn, 1, 0, 0);
		renderPiece(bd, bd->movingPieceRotation, (bd->turn == 1));
		glPopMatrix();
	}
}

void drawSpecialPieces(BoardData* bd)
{	/* Draw animated or dragged pieces */
	int blend = (bd->chequerMat[0].alphaBlend) || (bd->chequerMat[1].alphaBlend);

	if (blend)
	{	/* Draw back of piece separately */
		glEnable(GL_BLEND);
		glCullFace(GL_FRONT);
		renderSpecialPieces(bd);
		glCullFace(GL_BACK);
	}
	renderSpecialPieces(bd);

	if (blend)
		glDisable(GL_BLEND);
}

void drawPiece(BoardData* bd, int point, int pos)
{
	float v[3];
	glPushMatrix();

	getPiecePos(point, pos, fClockwise, v);
	glTranslatef(v[0], v[1], v[2]);

	/* Home pieces are sideways */
	if (point == 26)
		glRotatef(-90, 1, 0, 0);
	if (point == 27)
		glRotatef(90, 1, 0, 0);

	glRotatef((float)bd->pieceRotation[point][pos - 1], 0, 0, 1);
	glCallList(bd->pieceList);

	glPopMatrix();
}

void drawPieces(BoardData* bd)
{
	int i, j;
	int blend = (bd->chequerMat[0].alphaBlend) || (bd->chequerMat[1].alphaBlend);

	if (blend)
	{	/* Draw back of piece separately */
		glEnable(GL_BLEND);
		glCullFace(GL_FRONT);

		setMaterial(&bd->chequerMat[0]);
		for (i = 0; i < 28; i++)
		{
			for (j = 1; j <= -bd->points[i]; j++)
			{
				drawPiece(bd, i, j);
			}
		}
		setMaterial(&bd->chequerMat[1]);
		for (i = 0; i < 28; i++)
		{
			for (j = 1; j <= bd->points[i]; j++)
			{
				drawPiece(bd, i, j);
			}
		}
		glCullFace(GL_BACK);
	}

	setMaterial(&bd->chequerMat[0]);
	for (i = 0; i < 28; i++)
	{
		for (j = 1; j <= -bd->points[i]; j++)
		{
			drawPiece(bd, i, j);
		}
	}
	setMaterial(&bd->chequerMat[1]);
	for (i = 0; i < 28; i++)
	{
		for (j = 1; j <= bd->points[i]; j++)
		{
			drawPiece(bd, i, j);
		}
	}

	if (blend)
		glDisable(GL_BLEND);

	if (bd->DragTargetHelp)
	{	/* highlight target points */
		glPolygonMode(GL_FRONT, GL_LINE);
		SetColour(0, 1, 0, 0);	/* Nice bright green... */

		for (i = 0; i <= 3; i++)
		{
			int target = bd->iTargetHelpPoints[i];
			if (target != -1)
			{	/* Make sure texturing is disabled */
				if (bd->chequerMat[0].pTexture)
					glDisable(GL_TEXTURE_2D);
				drawPiece(bd, target, abs(bd->points[target]) + 1);
			}
		}
		glPolygonMode(GL_FRONT, GL_FILL);
	}
}

void DrawNumbers(BoardData* bd, int sides, int mode)
{
	int i;
	char num[3];
	float x;
	float textHeight = getFontHeight(bd);
	int n;

	glPushMatrix();
	glTranslatef(0, (EDGE_HEIGHT - textHeight) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
	x = TRAY_WIDTH - PIECE_HOLE / 2.0f;

	for (i = 0; i < 12; i++)
	{
		x += PIECE_HOLE;
		if (i == 6)
			x += BAR_WIDTH;

		if ((i < 6 && (sides & 1)) || (i >= 6 && (sides & 2)))
		{
			glPushMatrix();
			glTranslatef(x, 0, 0);
			if (!fClockwise)
				n = 12 - i;
			else
				n = i + 1;

			if (bd->turn == -1)
				n = 25 - n;

			sprintf(num, "%d", n);
			glPrintPointNumbers(bd, num, mode);
			glPopMatrix();
		}
	}

	glPopMatrix();
	glPushMatrix();
	glTranslatef(0, TOTAL_HEIGHT - textHeight - (EDGE_HEIGHT - textHeight) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
	x = TRAY_WIDTH - PIECE_HOLE / 2.0f;

	for (i = 0; i < 12; i++)
	{
		x += PIECE_HOLE;
		if (i == 6)
			x += BAR_WIDTH;
		if ((i < 6 && (sides & 1)) || (i >= 6 && (sides & 2)))
		{
			glPushMatrix();
			glTranslatef(x, 0, 0);
			if (!fClockwise)
				n = 13 + i;
			else
				n = 24 - i;

			if (bd->turn == -1)
				n = 25 - n;

			sprintf(num, "%d", n);
			glPrintPointNumbers(bd, num, mode);
			glPopMatrix();
		}
	}
	glPopMatrix();
}

void drawNumbers(BoardData* bd, int sides)
{
	/* No need to depth test as on top of board (depth test could cause alias problems too) */
	glDisable(GL_DEPTH_TEST);
	/* Draw inside then anti-aliased outline of numbers */
	setMaterial(&bd->pointNumberMat);
	glNormal3f(0, 0, 1);

	DrawNumbers(bd, sides, 0);

	glLineWidth(1);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	DrawNumbers(bd, sides, 1);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	glEnable(GL_DEPTH_TEST);
}

void drawPoint(float tuv, int i, int p)
{	/* Draw point with correct texture co-ords */
	float w = PIECE_HOLE;
	float h = POINT_HEIGHT;
	float x, y;

	if (p)
	{
		x = TRAY_WIDTH - EDGE_WIDTH + PIECE_HOLE * i;
		y = -LIFT_OFF;
	}
	else
	{
		x = TRAY_WIDTH - EDGE_WIDTH + BOARD_WIDTH - (PIECE_HOLE * i);
		y = TOTAL_HEIGHT - EDGE_HEIGHT * 2 + LIFT_OFF;
		w = -w;
		h = -h;
	}

	glPushMatrix();
	glTranslatef(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH);

	glBegin(GL_TRIANGLES);
		glNormal3f(0, 0, 1);
		glTexCoord2f(x * tuv, y * tuv);
		glVertex3f(x, y, 0);
		glTexCoord2f((x + w) * tuv, y * tuv); 
		glVertex3f(x + w, y, 0);
		glTexCoord2f((x + w / 2) * tuv, (y + h) * tuv); 
		glVertex3f(x + w / 2, y + h, 0);
	glEnd();

	glPopMatrix();
}

void drawPoints(BoardData* bd)
{
	/* texture unit value */
	float tuv;

	/* Don't worry about depth testing (but set depth values) */
	glDepthFunc(GL_ALWAYS);

	setMaterial(&bd->baseMat);
	drawSplitRect(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH + TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, bd->baseMat.pTexture);

 	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_BLEND);

	if (bd->pointMat[0].pTexture)
		tuv = (TEXTURE_SCALE) / bd->pointMat[0].pTexture->width;
	else
		tuv = 0;
	setMaterial(&bd->pointMat[0]);
	drawPoint(tuv, 0, 0);
	drawPoint(tuv, 0, 1);
	drawPoint(tuv, 2, 0);
	drawPoint(tuv, 2, 1);
	drawPoint(tuv, 4, 0);
	drawPoint(tuv, 4, 1);

	if (bd->pointMat[1].pTexture)
		tuv = (TEXTURE_SCALE) / bd->pointMat[1].pTexture->width;
	else
		tuv = 0;
	setMaterial(&bd->pointMat[1]);
	drawPoint(tuv, 1, 0);
	drawPoint(tuv, 1, 1);
	drawPoint(tuv, 3, 0);
	drawPoint(tuv, 3, 1);
	drawPoint(tuv, 5, 0);
	drawPoint(tuv, 5, 1);

	glDisable(GL_BLEND);
	glDisable(GL_POLYGON_SMOOTH);
	glDepthFunc(GL_LEQUAL);
}

void drawBase(BoardData* bd, int sides)
{
	if (sides & 1)
		drawPoints(bd);

	if (sides & 2)
	{
		/* Rotate right board around */
		glPushMatrix();
		glTranslatef(TOTAL_WIDTH, TOTAL_HEIGHT, 0);
		glRotatef(180, 0, 0, 1);
		drawPoints(bd);
		glPopMatrix();
	}
}

void drawHinge(BoardData* bd, float height)
{
	setMaterial(&bd->hingeMat);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glScalef(1, HINGE_SEGMENTS, 1);
	glMatrixMode(GL_MODELVIEW);

	glPushMatrix();
	glTranslatef((TOTAL_WIDTH) / 2.0f, height, BASE_DEPTH + EDGE_DEPTH);
	glRotatef(-90, 1, 0, 0);
	gluCylinder(bd->qobjTex, HINGE_WIDTH, HINGE_WIDTH, HINGE_HEIGHT, bd->curveAccuracy, 1);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glRotatef(180, 1, 0, 0);
	gluDisk(bd->qobjTex, 0, HINGE_WIDTH, bd->curveAccuracy, 1);

	glPopMatrix();
}

void tidyEdges(BoardData* bd)
{	/* Anti-alias board edges */
	setMaterial(&bd->boxMat);

	glLineWidth(1);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glDepthMask(GL_FALSE);

	glNormal3f(0, 0, 1);

	glBegin(GL_LINES);
		/* bar */
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		/* left bear off tray */
		glVertex3f(0, 0, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(0, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		/* right bear off tray */
		glVertex3f(TOTAL_WIDTH, 0, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

		/* inner edges (sides) */
		glNormal3f(1, 0, 0);
		glVertex3f(EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);

		glNormal3f(-1, 0, 0);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - EDGE_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - TRAY_WIDTH + LIFT_OFF, TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH + LIFT_OFF);
	glEnd();
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void showMoveIndicator(BoardData* bd)
{
/* ARROW_UNIT used to draw sub-bits of arrow */
#define ARROW_UNIT (ARROW_SIZE / 4.0f)
	setMaterial(&bd->chequerMat[(bd->turn == 1)]);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glNormal3f(0, 0, 1);
	glPushMatrix();
	if (!fClockwise)
	{
		if (bd->turn == 1)
			glTranslatef(TOTAL_WIDTH - TRAY_WIDTH, (EDGE_HEIGHT - ARROW_SIZE) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
		else
			glTranslatef(TOTAL_WIDTH - TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT + (EDGE_HEIGHT - ARROW_SIZE) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
	}
	else
	{
		if (bd->turn == 1)
			glTranslatef(TRAY_WIDTH, (EDGE_HEIGHT + ARROW_SIZE) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
		else
			glTranslatef(TRAY_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT + (EDGE_HEIGHT + ARROW_SIZE) / 2.0f, BASE_DEPTH + EDGE_DEPTH);
		glRotatef(180, 0, 0, 1);
	}
	glBegin(GL_QUADS);
		glVertex2f(0, ARROW_UNIT);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT * 3);
		glVertex2f(0, ARROW_UNIT * 3);
	glEnd();
	glBegin(GL_TRIANGLES);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT * 4);
		glVertex2f(ARROW_UNIT * 2, 0);
		glVertex2f(ARROW_UNIT * 4, ARROW_UNIT * 2);
	glEnd();

	/* Outline arrow */
	SetColour(0, 0, 0, 1);	/* Black outline */

	glLineWidth(.5f);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINE_LOOP);
		glVertex2f(0, ARROW_UNIT);
		glVertex2f(0, ARROW_UNIT * 3);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT * 3);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT * 4);
		glVertex2f(ARROW_UNIT * 4, ARROW_UNIT * 2);
		glVertex2f(ARROW_UNIT * 2, 0);
		glVertex2f(ARROW_UNIT * 2, ARROW_UNIT);
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void RotateClosingBoard(BoardData* bd)
{
	float rotAngle = 90;
	float trans = getBoardHeight() * .4f;
	float zoom = .2f;

	glPopMatrix();

	glClearColor(bd->backGroundMat.ambientColour[0], bd->backGroundMat.ambientColour[1], bd->backGroundMat.ambientColour[2], 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if ((bd->State == BOARD_OPENING) || (bd->State == BOARD_CLOSING))
	{
		rotAngle *= bd->perOpen;
		trans *= bd->perOpen;
		zoom *= bd->perOpen;
	}
	glPushMatrix();
	glTranslatef(0, -trans, zoom);
	glTranslatef(getBoardWidth() / 2.0f, getBoardHeight() / 2.0f, 0);
	glRotatef(rotAngle, 0, 0, 1);
	glTranslatef(-getBoardWidth() / 2.0f, -getBoardHeight() / 2.0f, 0);
}

void drawTable(BoardData* bd)
{
	if (bd->State != BOARD_OPEN)
		RotateClosingBoard(bd);

	/* Draw background */
	setMaterial(&bd->backGroundMat);

	/* Set depth and default pixel values (as background covers entire screen) */
	glDepthFunc(GL_ALWAYS);
	drawRect(bd->backGroundPos[0], bd->backGroundPos[1], 0, bd->backGroundSize[0], bd->backGroundSize[1], bd->backGroundMat.pTexture);
	glDepthFunc(GL_LEQUAL);

	/* Right side of board */
	if (!bd->showHinges)
		drawBase(bd, 1 | 2);
	else
		drawBase(bd, 2);
	setMaterial(&bd->boxMat);

	/* Right edge */
	drawBox(BOX_SPLITTOP, TOTAL_WIDTH - EDGE_WIDTH, 0, 0, EDGE_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);

	/* Top + bottom edges and bar */
	if (!bd->showHinges)
	{
		drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE_WIDTH, 0, 0, TOTAL_WIDTH - EDGE_WIDTH * 2, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, 0, TOTAL_WIDTH - EDGE_WIDTH * 2, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BAR_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	}
	else
	{
		drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, 0, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, EDGE_HEIGHT, 0, (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT * 2, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);
	}

	/* Bear-off edge */
	drawBox(BOX_NOENDS | BOX_SPLITTOP, TOTAL_WIDTH - TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	drawBox(BOX_NOSIDES, TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH - LIFT_OFF, TRAY_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2 + LIFT_OFF, MID_SIDE_GAP_HEIGHT, EDGE_DEPTH, bd->boxMat.pTexture);

	if (rdAppearance.fLabels)
		drawNumbers(bd, 2);

	/* Left side of board*/
	glPushMatrix();

	if (bd->State != BOARD_OPEN)
	{
		float boardAngle = 180;
		if ((bd->State == BOARD_OPENING) || (bd->State == BOARD_CLOSING))
			boardAngle *= bd->perOpen;

		glTranslatef(TOTAL_WIDTH / 2.0f, 0, BASE_DEPTH + EDGE_DEPTH);
		glRotatef(boardAngle, 0, 1, 0);
		glTranslatef(-TOTAL_WIDTH / 2.0f, 0, -(BASE_DEPTH + EDGE_DEPTH));
	}

	if (bd->showHinges)
		drawBase(bd, 1);

	setMaterial(&bd->boxMat);

	if (bd->State != BOARD_OPEN)
	{	/* Back of board */
		float logoSize = (TOTAL_WIDTH * .3f) / 2.0f;
		drawRect(TOTAL_WIDTH / 2.0f, 0, 0, -(TOTAL_WIDTH / 2.0f), TOTAL_HEIGHT, bd->boxMat.pTexture);
		/* logo */
		glPushMatrix();
		glTranslatef(TOTAL_WIDTH / 4.0f, TOTAL_HEIGHT / 2.0f, -LIFT_OFF);
		glRotatef(90, 0, 0, 1);
		setMaterial(&bd->logoMat);
		glNormal3f(0, 0, 1);
		glBegin(GL_QUADS);
			glTexCoord2f(0, 0); glVertex3f(logoSize, -logoSize, 0);
			glTexCoord2f(1, 0); glVertex3f(-logoSize, -logoSize, 0);
			glTexCoord2f(1, 1); glVertex3f(-logoSize, logoSize, 0);
			glTexCoord2f(0, 1); glVertex3f(logoSize, logoSize, 0);
		glEnd();
		glPopMatrix();
		setMaterial(&bd->boxMat);
	}

	/* Bear off tray */
	drawBox(BOX_SPLITTOP, 0, 0, 0, EDGE_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);

	if (bd->showHinges)
	{	/* Top + bottom edges and bar */
		drawBox(BOX_ALL, EDGE_WIDTH, 0, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_ALL, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH + BOARD_WIDTH, EDGE_HEIGHT, 0, (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - EDGE_HEIGHT * 2, BASE_DEPTH + EDGE_DEPTH, bd->boxMat.pTexture);
	}

	/* Bear-off edge */
	drawBox(BOX_NOENDS | BOX_SPLITTOP, TRAY_WIDTH - EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	drawBox(BOX_NOSIDES, EDGE_WIDTH - LIFT_OFF, TRAY_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2 + LIFT_OFF * 2, MID_SIDE_GAP_HEIGHT, EDGE_DEPTH, bd->boxMat.pTexture);

	if (bd->showHinges)
	{
		drawHinge(bd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f);
		drawHinge(bd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f);

		if (bd->State == BOARD_OPEN)
		{	/* Shadow in gap between boards */
			setMaterial(&bd->gap);
			drawRect((TOTAL_WIDTH - HINGE_GAP) / 2.0f, 0, LIFT_OFF, HINGE_GAP, TOTAL_HEIGHT + LIFT_OFF, 0);
		}
	}

	if (rdAppearance.fLabels)
		drawNumbers(bd, 1);

	glPopMatrix();

	if (bd->showMoveIndicator)
		showMoveIndicator(bd);
}

int DiceShowing(BoardData* bd)
{
	return (bd->diceShown == DICE_ON_BOARD ||
		(fGUIDiceArea && bd->diceShown == DICE_BELOW_BOARD));
}

void drawPick(BoardData* bd)
{	/* Draw all the objects on the board to see if any have been selected */
	int i, j;
	float barHeight;

	/* pieces */
	for (i = 0; i < 28; i++)
	{
		glLoadName(i);
		for (j = 1; j <= abs(bd->points[i]); j++)
			drawPiece(bd, i, j);
	}

	/* points */
	for (i = 0; i < 6; i++)
	{
		glLoadName(fClockwise ? i + 1 : 12 - i);
		drawRect(TRAY_WIDTH + PIECE_HOLE * i, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, POINT_HEIGHT, 0);

		glLoadName(fClockwise ? i + 7 : 6 - i);
		drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * i, EDGE_HEIGHT, BASE_DEPTH, PIECE_HOLE, POINT_HEIGHT, 0);

		glLoadName(fClockwise ? i + 13 : 24 - i);
		drawRect(TOTAL_WIDTH - TRAY_WIDTH - (PIECE_HOLE * i), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE, -POINT_HEIGHT, 0);

		glLoadName(fClockwise ? i + 19 : 18 - i);
		drawRect(TRAY_WIDTH + BOARD_WIDTH - (PIECE_HOLE * i), TOTAL_HEIGHT - EDGE_HEIGHT, BASE_DEPTH, -PIECE_HOLE, -POINT_HEIGHT, 0);
	}
	/* bars + homes */
	barHeight = (PIECE_HOLE + PIECE_GAP_HEIGHT) * 4;
	glLoadName(25);
	drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f, TOTAL_HEIGHT / 2.0f - (DOUBLECUBE_SIZE / 2.0f + barHeight + PIECE_HOLE / 2.0f),
			BASE_DEPTH + EDGE_DEPTH, PIECE_HOLE, barHeight, 0);
	glLoadName(0);
	drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f, TOTAL_HEIGHT / 2.0f + (DOUBLECUBE_SIZE / 2.0f + PIECE_HOLE / 2.0f),
			BASE_DEPTH + EDGE_DEPTH, PIECE_HOLE, barHeight, 0);

	/* Home/unused trays depend on direction */
	glLoadName((!fClockwise) ? 26 : POINT_UNUSED0);
	drawRect(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, 0);
	glLoadName((fClockwise) ? 26 : POINT_UNUSED0);
	drawRect(EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, 0);

	glLoadName((!fClockwise) ? 27 : POINT_UNUSED1);
	drawRect(TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, POINT_HEIGHT, 0);
	glLoadName((fClockwise) ? 27 : POINT_UNUSED1);
	drawRect(EDGE_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, POINT_HEIGHT, 0);

	/* Dice areas */
	glLoadName(POINT_LEFT);
	drawRect(TRAY_WIDTH, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_DEPTH, DICE_AREA_CLICK_WIDTH, DICE_AREA_HEIGHT, 0);
	glLoadName(POINT_RIGHT);
	drawRect(TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_DEPTH, DICE_AREA_CLICK_WIDTH, DICE_AREA_HEIGHT, 0);

	/* Dice */
	if (DiceShowing(bd))
	{
		glLoadName(POINT_DICE);
		glPushMatrix();
		moveToDicePos(bd, 0);
		drawCube(DICE_SIZE * .95f);
		glPopMatrix();
		glPushMatrix();
		moveToDicePos(bd, 1);
		drawCube(DICE_SIZE * .95f);
		glPopMatrix();
	}

	/* Double Cube */
	glPushMatrix();
	glLoadName(POINT_CUBE);
	moveToDoubleCubePos(bd);
	drawCube(DOUBLECUBE_SIZE * .95f);
	glPopMatrix();
}

int NearestHit(int hits, GLuint* ptr)
{
	int i, sel = -1;
	GLuint names;
	float minDepth = 0, depth;

	for (i = 0; i < hits; i++)
	{	/* for each hit */
		names = *ptr++;
		depth = (float) *ptr++/0x7fffffff;
		ptr++;	/* Skip max depth */
		if (i == 0 || depth < minDepth)
		{	/* if closer */
			minDepth = depth;
			sel = *ptr;
		}
		ptr += names;
	}
	return sel;
}

void getProjectedPos(int x, int y, float atDepth, float pos[3])
{	/* Work out where point (x, y) projects to at depth atDepth */
	GLint viewport[4];
	GLdouble mvmatrix[16], projmatrix[16];
	double nX, nY, nZ, fX, fY, fZ, zRange;

	glGetIntegerv (GL_VIEWPORT, viewport);
	glGetDoublev (GL_MODELVIEW_MATRIX, mvmatrix);
	glGetDoublev (GL_PROJECTION_MATRIX, projmatrix);

	gluUnProject ((GLdouble)x, (GLdouble)y, 0.0, mvmatrix, projmatrix, viewport, &nX, &nY, &nZ);
	gluUnProject ((GLdouble)x, (GLdouble)y, 1.0, mvmatrix, projmatrix, viewport, &fX, &fY, &fZ);

	zRange = (fabs(nZ) - atDepth) / (fabs(fZ) + fabs(nZ));
	pos[0] = (float)(nX - (-fX + nX) * zRange);
	pos[1] = (float)(nY - (-fY + nY) * zRange);
	pos[2] = (float)(nZ - (-fZ + nZ) * zRange);
}

void getProjectedPieceDragPos(int x, int y, float pos[3])
{
	getProjectedPos(x, y, BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE + LIFT_OFF * 3, pos);
}

void calculateBackgroundSize(BoardData *bd, int viewport[4])
{
	float pos1[3], pos2[3], pos3[3];

	getProjectedPos(0, viewport[1] + viewport[3], 0, pos1);
	getProjectedPos(viewport[2], viewport[1] + viewport[3], 0, pos2);
	getProjectedPos(0, viewport[1], 0, pos3);

	bd->backGroundPos[0] = pos1[0];
	bd->backGroundPos[1] = pos3[1];
	bd->backGroundSize[0] = pos2[0] - pos1[0];
	bd->backGroundSize[1] = pos1[1] - pos3[1];
}

void getFlagPos(BoardData* bd, float v[3])
{
	v[0] = (TRAY_WIDTH + BOARD_WIDTH) / 2.0f;
	v[1] = TOTAL_HEIGHT / 2.0f;
	v[2] = BASE_DEPTH + EDGE_DEPTH;

	if (bd->turn == -1)	/* Move to other side of board */
		v[0] += BOARD_WIDTH + BAR_WIDTH;
}

void waveFlag(BoardData* bd, float wag)
{
	int i, j;

	/* wave the flag by rotating Z coords though a sine wave */
	for (i = 1; i < S_NUMPOINTS; i++)
		for (j = 0; j < T_NUMPOINTS; j++)
			bd->ctlpoints[i][j][2] = (float)sin((GLfloat)i + wag) * FLAG_WAG;
}

void drawFlagPick(BoardData* bd)
{
	int s;
	float v[3];

	waveFlag(bd, bd->flagWaved);

	glLoadName(POINT_RESIGN);

	glPushMatrix();

	getFlagPos(bd, v);
	glTranslatef(v[0], v[1], v[2]);

	/* Draw flag surface (approximation) */
	glBegin(GL_QUAD_STRIP);
	for (s = 0; s < S_NUMPOINTS; s++)
	{
		glVertex3fv(bd->ctlpoints[s][1]);
		glVertex3fv(bd->ctlpoints[s][0]);
	}
	glEnd();

	/* Draw flag pole */
	glTranslatef(0, -FLAG_HEIGHT, 0);

	glRotatef(-90, 1, 0, 0);
	gluCylinder(bd->qobj, FLAGPOLE_WIDTH, FLAGPOLE_WIDTH, FLAGPOLE_HEIGHT, bd->curveAccuracy, 1);

	circleRev(FLAGPOLE_WIDTH, 0, bd->curveAccuracy);
	circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, bd->curveAccuracy);

	glPopMatrix();
}

void drawPointPick(BoardData* bd, int point)
{	/* Draw sub parts of point to work out which part of point clicked */
	int i;
	float pos[3];

	if (point >= 0 && point <= 25)
	{	/* Split first point into 2 parts for zero and one selection */
#define SPLIT_PERCENT .40f
#define SPLIT_HEIGHT (PIECE_HOLE * SPLIT_PERCENT)

		getPiecePos(point, 1, fClockwise, pos);

		glLoadName(0);
		if (point > 12)
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] + (PIECE_HOLE / 2.0f) + PIECE_GAP_HEIGHT - SPLIT_HEIGHT, pos[2], PIECE_HOLE, SPLIT_HEIGHT, 0);
		else
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE, SPLIT_HEIGHT, 0);

		glLoadName(1);
		if (point > 12)
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE, PIECE_HOLE + PIECE_GAP_HEIGHT - SPLIT_HEIGHT, 0);
		else
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f) + SPLIT_HEIGHT, pos[2], PIECE_HOLE, PIECE_HOLE + PIECE_GAP_HEIGHT - SPLIT_HEIGHT, 0);

		for (i = 2; i < 6; i++)
		{
			/* Only 3 places for bar */
			if ((point == 0 || point == 25) && i == 4)
				break;
			getPiecePos(point, i, fClockwise, pos);
			glLoadName(i);
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE, PIECE_HOLE + PIECE_GAP_HEIGHT, 0);
		}
		/* extra bit */
/*		glLoadName(i);
		if (point > 12)
		{
			pos[1] = pos[1] - (PIECE_HOLE / 2.0f);
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT, pos[2], PIECE_HOLE, pos[1] - (TOTAL_HEIGHT - EDGE_HEIGHT - POINT_HEIGHT), 0);
		}
		else
		{
			pos[1] = pos[1] + (PIECE_HOLE / 2.0f) + PIECE_GAP_HEIGHT;
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1], pos[2], PIECE_HOLE, EDGE_HEIGHT + POINT_HEIGHT - pos[1], 0);
		}
*/	}
}

/* 20 allows for 5 hit records (more than enough) */
#define BUFSIZE 20

int BoardPoint3d(BoardData *bd, int x, int y, int point)
{	/* Identify if anything is below point (x,y) */
	GLuint selectBuf[BUFSIZE];
	GLint hits;
	GLint viewport[4];

	glSelectBuffer(BUFSIZE, selectBuf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	glGetIntegerv (GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	glLoadIdentity();
	gluPickMatrix(x, y, 1, 1, viewport);

	/* Setup projection matrix - using saved values */
	glFrustum(-bd->horFrustrum, bd->horFrustrum, -bd->vertFrustrum, bd->vertFrustrum, zNear, zFar);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(bd->modelMatrix);

	if (bd->resigned)
	{	/* Flag showing - just pick this */
		drawFlagPick(bd);
	}
	else
	{
		/* Either draw all the objects of just a single point */
		if (point == -1)
			drawPick(bd);
		else
			drawPointPick(bd, point);
	}

	glPopName();

	hits = glRenderMode(GL_RENDER);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix() ;
	glMatrixMode(GL_MODELVIEW);
	
	return NearestHit(hits, (GLuint*)selectBuf);
}

void setupPath(BoardData *bd, Path* p, float* pRotate, int fClockwise, int fromPoint, int fromDepth, int toPoint, int toDepth)
{	/* Work out the animation path for a moving piece */
	float point[3];
	float w, h, xDist, yDist;
	float xDiff, yDiff;
	float bar1Dist, bar1Ratio;
	float bar2Dist, bar2Ratio;
	float start[3], end[3], obj1, obj2, objHeight;
	int fromBoard = (fromPoint + 5) / 6;
	int toBoard = (toPoint - 1) / 6 + 1;
	int yDir = 0;

	/* First work out were piece has to move */
	/* Get start and end points */
	getPiecePos(fromPoint, fromDepth, fClockwise, start);
	getPiecePos(toPoint, toDepth, fClockwise, end);

	/* Only rotate piece going home */
	*pRotate = -1;

	/* Swap board if displaying other way around */
	if (fClockwise)
	{
		int swap[] = {0, 2, 1, 4, 3};
		fromBoard = swap[fromBoard];
		toBoard = swap[toBoard];
	}

	/* Work out what obstacle needs to be avoided */
	if ((fromBoard == 2 || fromBoard == 3) && (toBoard == 1 || toBoard == 4))
	{	/* left side to right side */
		obj1 = TRAY_WIDTH + BOARD_WIDTH;
		obj2 = obj1 + BAR_WIDTH;
		objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
	}
 	else if ((fromBoard == 1 || fromBoard == 4) && (toBoard == 2 || toBoard == 3))
	{	/* right side to left side */
		obj2 = TRAY_WIDTH + BOARD_WIDTH;
		obj1 = obj2 + BAR_WIDTH;
		objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
	}
	else if ((fromBoard == 1 && toBoard == 4) || (fromBoard == 2 && toBoard == 3))
	{	/* up same side */
		obj1 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
		obj2 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
		objHeight = BASE_DEPTH + DICE_SIZE;
		yDir = 1;
	}
	else if ((fromBoard == 4 && toBoard == 1) || (fromBoard == 3 && toBoard == 2))
	{	/* down same side */
		obj1 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
		obj2 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
		objHeight = BASE_DEPTH + DICE_SIZE;
		yDir = 1;
	}
	else if (fromBoard == toBoard)
	{
		if (fromBoard <= 2)
		{
			if (fClockwise)
			{
				fromPoint = 13 - fromPoint;
				toPoint = 13 - toPoint;
			}

			if (fromPoint < toPoint)
				toPoint--;
			else
				fromPoint--;

			fromPoint = 12 - fromPoint;
			toPoint = 12 - toPoint;
		}
		else
		{
			if (fClockwise)
			{
				fromPoint = 24 + 13 - fromPoint;
				toPoint = 24 + 13 - toPoint;
			}

			if (fromPoint < toPoint)
				fromPoint++;
			else
				toPoint++;
			fromPoint = fromPoint - 13;
			toPoint = toPoint - 13;
		}
		obj1 = TRAY_WIDTH + PIECE_HOLE * fromPoint;
		obj2 = TRAY_WIDTH + PIECE_HOLE * toPoint;
		if ((fromBoard == 1) || (fromBoard == 4))
		{
			obj1 += BAR_WIDTH;
			obj2 += BAR_WIDTH;
		}

		objHeight = BASE_DEPTH + PIECE_DEPTH * 3;
	}
	else
	{
		if (fromPoint == 0 || fromPoint == 25)
		{	/* Move from bar */
			if (!fClockwise)
			{
				obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
				if (fromPoint == 0)
				{
					obj2 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
					if (toPoint > 20)
						obj2 += PIECE_HOLE * (toPoint - 20);
				}
				else
				{
					obj2 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH;
					if (toPoint < 5)
						obj2 += PIECE_HOLE * (5 - toPoint);
				}
			}
			else
			{
				obj1 = TRAY_WIDTH + BOARD_WIDTH;
				if (fromPoint == 0)
				{
					obj2 = TRAY_WIDTH + PIECE_HOLE * (25 - toPoint);
					if (toPoint > 19)
						obj2 += PIECE_HOLE;
				}
				else
				{
					obj2 = TRAY_WIDTH + PIECE_HOLE * toPoint;
					if (toPoint < 6)
						obj2 += PIECE_HOLE;
				}
			}
			objHeight = BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE;
		}
		else
		{	/* Move home */
			if (!fClockwise)
			{
				if (toPoint == 26)
					obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (7 - fromPoint);
				else /* (toPoint == 27) */
					obj1 = TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * (fromPoint - 18);
			}
			else
			{
				if (toPoint == 26)
					obj1 = TRAY_WIDTH + PIECE_HOLE * (fromPoint - 1);
				else /* (toPoint == 27) */
					obj1 = TRAY_WIDTH + PIECE_HOLE * (24 - fromPoint);
			}

			if (!fClockwise)
				obj2 = TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH;
			else
				obj2 = TRAY_WIDTH - EDGE_WIDTH;

			*pRotate = 0;
			objHeight = BASE_DEPTH + EDGE_DEPTH + DICE_SIZE;
		}
	}
	if ((fromBoard == 3 && toBoard == 4) || (fromBoard == 4 && toBoard == 3))
	{	/* Special case when moving along top of board */
		if ((bd->cube_owner != 1) && (fromDepth <= 2) && (toDepth <= 2))
			objHeight = BASE_DEPTH + EDGE_DEPTH + PIECE_DEPTH;
	}

	/* Now setup path object */
	/* Start point */
	initPath(p, start);

	/* obstacle height */
	point[2] = objHeight;

	if (yDir)
	{	/* barriers are along y axis */
		xDiff = end[0] - start[0];
		yDiff = end[1] - start[1];
		bar1Dist = obj1 - start[1];
		bar1Ratio = bar1Dist / yDiff;
		bar2Dist = obj2 - start[1];
		bar2Ratio = bar2Dist / yDiff;

		/* 2nd point - moved up from 1st one */
		/* Work out whether 2nd point is further away than height required */
		xDist = xDiff * bar1Ratio;
		yDist = obj1 - start[1];
		h = objHeight - start[2];
		w = (float)sqrt(xDist * xDist + yDist * yDist);

		if (h > w)
		{
			point[0] = start[0] + xDiff * bar1Ratio;
			point[1] = obj1;
		}
		else
		{
			point[0] = start[0] + xDist * (h / w);
			point[1] = start[1] + yDist * (h / w);
		}
		addPathSegment(p, PATH_CURVE_9TO12, point);

		/* Work out whether 3rd point is further away than height required */
		yDist = end[1] - obj2;
		xDist = xDiff * (yDist / yDiff);
		w = (float)sqrt(xDist * xDist + yDist * yDist);
		h = objHeight - end[2];

		/* 3rd point - moved along from 2nd one */
		if (h > w)
		{
			point[0] = start[0] + xDiff * bar2Ratio;
			point[1] = obj2;
		}
		else
		{
			point[0] = end[0] - xDist * (h / w);
			point[1] = end[1] - yDist * (h / w);
		}
		addPathSegment(p, PATH_LINE, point);
	}
	else
	{	/* barriers are along x axis */
		xDiff = end[0] - start[0];
		yDiff = end[1] - start[1];
		bar1Dist = obj1 - start[0];
		bar1Ratio = bar1Dist / xDiff;
		bar2Dist = obj2 - start[0];
		bar2Ratio = bar2Dist / xDiff;

		/* Work out whether 2nd point is further away than height required */
		xDist = obj1 - start[0];
		yDist = yDiff * bar1Ratio;
		h = objHeight - start[2];
		w = (float)sqrt(xDist * xDist + yDist * yDist);

		/* 2nd point - moved up from 1st one */
		if (h > w)
		{
			point[0] = obj1;
			point[1] = start[1] + yDiff * bar1Ratio;
		}
		else
		{
			point[0] = start[0] + xDist * (h / w);
			point[1] = start[1] + yDist * (h / w);
		}
		addPathSegment(p, PATH_CURVE_9TO12, point);

		/* Work out whether 3rd point is further away than height required */
		xDist = end[0] - obj2;
		yDist = yDiff * (xDist / xDiff);
		w = (float)sqrt(xDist * xDist + yDist * yDist);
		h = objHeight - end[2];

		/* 3rd point - moved along from 2nd one */
		if (h > w)
		{
			point[0] = obj2;
			point[1] = start[1] + yDiff * bar2Ratio;
		}
		else
		{
			point[0] = end[0] - xDist * (h / w);
			point[1] = end[1] - yDist * (h / w);
		}
		addPathSegment(p, PATH_LINE, point);
	}
	/* End point */
	addPathSegment(p, PATH_CURVE_12TO3, end);
}

void setupDicePath(int num, float stepSize, Path* p, BoardData* bd)
{
	float point[3];
	getDicePos(bd, num, point);

	point[0] -= stepSize * 5;
	initPath(p, point);

	point[0] += stepSize * 2;
	addPathSegment(p, PATH_PARABOLA_12TO3, point);

	point[0] += stepSize * 2;
	addPathSegment(p, PATH_PARABOLA, point);

	point[0] += stepSize;
	addPathSegment(p, PATH_PARABOLA, point);
}

void randomDiceRotation(Path* p, DiceRotation* diceRotation, int dir)
{
/* Dice rotation range values */
#define XROT_MIN 1
#define XROT_RANGE 1.5f

#define YROT_MIN -.5f
#define YROT_RANGE 1

	diceRotation->xRotFactor = XROT_MIN + randRange(XROT_RANGE);
	diceRotation->xRotStart = 1 - (p->pts[p->numSegments][0] - p->pts[0][0]) * diceRotation->xRotFactor;
	diceRotation->xRot = diceRotation->xRotStart * 360;

	diceRotation->yRotFactor = YROT_MIN + randRange(YROT_RANGE);
	diceRotation->yRotStart = 1 - (p->pts[p->numSegments][0] - p->pts[0][0]) * diceRotation->yRotFactor;
	diceRotation->yRot = diceRotation->yRotStart * 360;

	if (dir == -1)
	{
		diceRotation->xRotFactor = -diceRotation->xRotFactor;
		diceRotation->yRotFactor = -diceRotation->yRotFactor;
	}
}

void setupDicePaths(BoardData* bd, Path dicePaths[2])
{
	int firstDie = (bd->turn == 1);
	int dir = (bd->turn == 1) ? -1 : 1;

	setupDicePath(firstDie, dir * DICE_STEP_SIZE0, &dicePaths[firstDie], bd);
	setupDicePath(!firstDie, dir * DICE_STEP_SIZE1, &dicePaths[!firstDie], bd);

	randomDiceRotation(&dicePaths[firstDie], &bd->diceRotation[firstDie], dir);
	randomDiceRotation(&dicePaths[!firstDie], &bd->diceRotation[!firstDie], dir);

	copyPoint(bd->diceMovingPos[0], dicePaths[0].pts[0]);
	copyPoint(bd->diceMovingPos[1], dicePaths[1].pts[0]);
}

void setDicePos(BoardData* bd)
{
	int iter = 0;
	float dist;
	float orgX[2];
	float firstX = TRAY_WIDTH + DICE_STEP_SIZE0 * 3 + DICE_SIZE * .75f;
	int firstDie = (bd->turn == 1);

	bd->dicePos[firstDie][0] = firstX + randRange(BOARD_WIDTH + TRAY_WIDTH - firstX - DICE_SIZE * 2);
	bd->dicePos[firstDie][1] = randRange(DICE_AREA_HEIGHT);

	do	/* insure dice are not too close together */
	{
		firstX = bd->dicePos[firstDie][0] + DICE_SIZE;
		bd->dicePos[!firstDie][0] = firstX + randRange(BOARD_WIDTH + TRAY_WIDTH - firstX - DICE_SIZE * .7f);
		bd->dicePos[!firstDie][1] = randRange(DICE_AREA_HEIGHT);

		orgX[0] = bd->dicePos[firstDie][0] - DICE_STEP_SIZE0 * 5;
		orgX[1] = bd->dicePos[!firstDie][0] - DICE_STEP_SIZE1 * 5;
		dist = (float)sqrt((orgX[1] - orgX[0]) * (orgX[1] - orgX[0])
				+ (bd->dicePos[!firstDie][1] - bd->dicePos[firstDie][1]) * (bd->dicePos[!firstDie][1] - bd->dicePos[firstDie][1]));

		if (++iter > 20)
		{	/* Trouble placing 2nd dice - replace 1st dice */
			setDicePos(bd);
			return;;
		}
	}
	while (dist < DICE_SIZE * 1.1f);

	bd->dicePos[firstDie][2] = (float)(rand() % 360);
	bd->dicePos[!firstDie][2] = (float)(rand() % 360);

	updateDiceOccPos(bd);
}

void drawMovingDice(BoardData* bd, int num)
{
	glPushMatrix();
	glTranslatef(bd->diceMovingPos[num][0], bd->diceMovingPos[num][1], bd->diceMovingPos[num][2]);
	glRotatef(bd->diceRotation[num].xRot, 0, 1, 0);
	glRotatef(bd->diceRotation[num].yRot, 1, 0, 0);
	glRotatef(bd->dicePos[num][2], 0, 0, 1);

	drawDice2(bd, num);
	
	glPopMatrix();
}

void drawDie(BoardData* bd)
{
	if (bd->shakingDice)
	{
		drawMovingDice(bd, 0);
		drawMovingDice(bd, 1);
	}
	else
	{
		drawDice(bd, 0);
		drawDice(bd, 1);
	}
}

void initViewArea(viewArea* pva)
{	/* Initialize values to extremes */
	pva->top = -base_unit * 1000;
	pva->bottom = base_unit * 1000;
	pva->width = 0;
}

float getViewAreaHeight(viewArea* pva)
{
	return pva->top - pva->bottom;
}

float getViewAreaWidth(viewArea* pva)
{
	return pva->width;
}

float getAreaRatio(viewArea* pva)
{
	return getViewAreaWidth(pva) / getViewAreaHeight(pva);
}

void addViewAreaHeightPoint(viewArea* pva, float halfRadianFOV, float boardRadAngle, float inY, float inZ)
{	/* Rotate points by board angle */
	float adj;
	float y, z;

	y = inY * (float)cos(boardRadAngle) - inZ * (float)sin(boardRadAngle);
	z = inZ * (float)cos(boardRadAngle) + inY * (float)sin(boardRadAngle);

	/* Project height to zero depth */
	adj = z * (float)tan(halfRadianFOV);
	if (y > 0)
		y += adj;
	else
		y -= adj;

	/* Store max / min heights */
	if (pva->top < y)
		pva->top = y;
	if (pva->bottom > y)
		pva->bottom = y;
}

void workOutWidth(viewArea* pva, float halfRadianFOV, float boardRadAngle, float aspectRatio, float ip[3])
{
	float halfRadianXFOV;
	float w = getViewAreaHeight(pva) * aspectRatio;
	float w2, l;

	float p[3];

	/* Work out X-FOV from Y-FOV */
	float halfViewHeight = getViewAreaHeight(pva) / 2;
	l = halfViewHeight / (float)tan(halfRadianFOV);
	halfRadianXFOV = (float)atan((halfViewHeight * aspectRatio) / l);

	/* Rotate z coord by board angle */
	copyPoint(p, ip);
	p[2] = ip[2] * (float)cos(boardRadAngle) + ip[1] * (float)sin(boardRadAngle);

	/* Adjust x coord by projected z value at relevant X-FOV */
	w2 = w / 2 - p[2] * (float)tan(halfRadianXFOV);
	l = w2 / (float)tan(halfRadianXFOV);
	p[0] += p[2] * (p[0] / l);

	if (pva->width < p[0] * 2)
		pva->width = p[0] * 2;
}

float GetFOVAngle(BoardData* bd)
{
	float temp = bd->boardAngle / 20.0f;
	float skewFactor = (bd->testSkewFactor / 100.0f) * (4 - .6f) + .6f;
	/* Magic numbers to normalize viewing distance */
	return (47 - 2.3f * temp * temp) / skewFactor;
}

void SetupPerspVolume(BoardData* bd, int viewport[4])
{
	float fovScale;
	float zoom;
	float aspectRatio = (float)viewport[2]/(float)(viewport[3]);

	float halfRadianFOV;
	float p[3];
	float boardRadAngle;
	viewArea va;
	initViewArea(&va);

	boardRadAngle = (bd->boardAngle * PI) / 180.0f;
	halfRadianFOV = ((GetFOVAngle(bd) * PI) / 180.0f) / 2.0f;

	if (aspectRatio < .5f)
	{
		int newHeight = viewport[2] * 2;
		viewport[1] = (viewport[3] - newHeight) / 2;
		viewport[3] = newHeight;
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		aspectRatio = .5f;
	}

	/* Sort out viewing area */
	addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, -getBoardHeight() / 2, 0);
	addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, -getBoardHeight() / 2, BASE_DEPTH + EDGE_DEPTH);

	if (fGUIDiceArea)
	{
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 + DICE_SIZE, 0);
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 + DICE_SIZE, BASE_DEPTH + DICE_SIZE);
	}
	else
	{	/* Bottom edge is defined by board */
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2, 0);
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2, BASE_DEPTH + EDGE_DEPTH);
	}

	if (!bd->doubled)
	{
		if (bd->cube_owner == 1)
			addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, -(getBoardHeight() / 2 - EDGE_HEIGHT), BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE);
		if (bd->cube_owner == -1)
			addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 - EDGE_HEIGHT, BASE_DEPTH + EDGE_DEPTH + DOUBLECUBE_SIZE);
	}

	p[0] = getBoardWidth() / 2;
	p[1] = getBoardHeight() / 2;
	p[2] = BASE_DEPTH + EDGE_DEPTH;
	workOutWidth(&va, halfRadianFOV, boardRadAngle, aspectRatio, p);

	fovScale = zNear * (float)tan(halfRadianFOV);

	if (aspectRatio > getAreaRatio(&va))
	{
		bd->vertFrustrum = fovScale;
		bd->horFrustrum = bd->vertFrustrum * aspectRatio;
	}
	else
	{
		bd->horFrustrum = fovScale * getAreaRatio(&va);
		bd->vertFrustrum = bd->horFrustrum / aspectRatio;
	}
	/* Setup projection matrix */
	glFrustum(-bd->horFrustrum, bd->horFrustrum, -bd->vertFrustrum, bd->vertFrustrum, zNear, zFar);

	/* Setup modelview matrix */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* Zoom back so image fills window */
	zoom = (getViewAreaHeight(&va) / 2) / (float)tan(halfRadianFOV);
	glTranslatef(0, 0, -zoom);

	/* Offset from centre because of perspective */
	glTranslatef(0, getViewAreaHeight(&va) / 2 + va.bottom, 0);

	/* Rotate board */
	glRotatef((float)bd->boardAngle, -1, 0, 0);

	/* Origin is bottom left, so move from centre */
	glTranslatef(-(getBoardWidth() / 2.0f), -((getBoardHeight()) / 2.0f), 0);

	/* Save matrix for later */
	glGetFloatv(GL_MODELVIEW_MATRIX, bd->modelMatrix);
}

void setupFlag(BoardData* bd)
{
	int i;
	float width = FLAG_WIDTH;
	float height = FLAG_HEIGHT;

	if (bd->flagNurb)
		gluDeleteNurbsRenderer(bd->flagNurb);
	bd->flagNurb = gluNewNurbsRenderer();

	for (i = 0; i < S_NUMPOINTS; i++)
	{
		bd->ctlpoints[i][0][0] = width / (S_NUMPOINTS - 1) * i;
		bd->ctlpoints[i][1][0] = width / (S_NUMPOINTS - 1) * i;
		bd->ctlpoints[i][0][1] = 0;
		bd->ctlpoints[i][1][1] = height;
	}
	for (i = 0; i < T_NUMPOINTS; i++)
		bd->ctlpoints[0][i][2] = 0;
}

void renderFlag(BoardData* bd)
{
	/* Simple knots i.e. no weighting */
	float s_knots[S_NUMKNOTS] = {0, 0, 0, 0, 1, 1, 1, 1};
	float t_knots[T_NUMKNOTS] = {0, 0, 1, 1};

	/* Draw flag surface */
	setMaterial(&bd->flagMat);

	/* Set size of polygons */
	gluNurbsProperty(bd->flagNurb, GLU_SAMPLING_TOLERANCE, 500.0f / bd->curveAccuracy);

	gluBeginSurface(bd->flagNurb);
		gluNurbsSurface(bd->flagNurb, S_NUMKNOTS, s_knots, T_NUMKNOTS, t_knots, 3 * T_NUMPOINTS, 3,
						(float*)bd->ctlpoints, S_NUMPOINTS, T_NUMPOINTS, GL_MAP2_VERTEX_3);
	gluEndSurface(bd->flagNurb);

	/* Draw flag pole */
	glPushMatrix();

	glTranslatef(-FLAGPOLE_WIDTH, -FLAG_HEIGHT, 0);

	glRotatef(-90, 1, 0, 0);
	SetColour(.2f, .2f, .4f, 0);	/* Blue pole */
	gluCylinder(bd->qobj, FLAGPOLE_WIDTH, FLAGPOLE_WIDTH, FLAGPOLE_HEIGHT, bd->curveAccuracy, 1);

	circleRev(FLAGPOLE_WIDTH, 0, bd->curveAccuracy);
	circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, bd->curveAccuracy);

	glPopMatrix();

	/* Draw number on flag */
	glDisable(GL_DEPTH_TEST);

	setMaterial(&bd->flagNumberMat);

	glPushMatrix();
	{
		/* Move to middle of flag */
		float v[3];
		v[0] = (bd->ctlpoints[1][0][0] + bd->ctlpoints[2][0][0]) / 2.0f;
		v[1] = (bd->ctlpoints[1][0][0] + bd->ctlpoints[1][1][0]) / 2.0f;
		v[2] = (bd->ctlpoints[1][0][2] + bd->ctlpoints[2][0][2]) / 2.0f;
		glTranslatef(v[0], v[1], v[2]);
	}
	{
		/* Work out approx angle of number based on control points */
		float ang = (float)atan(-(bd->ctlpoints[2][0][2] - bd->ctlpoints[1][0][2]) / (bd->ctlpoints[2][0][0] - bd->ctlpoints[1][0][0]));
		float degAng = (ang) * 180 / PI;

		glRotatef(degAng, 0, 1, 0);
	}

	{
		/* Draw number */
		char flagValue[2] = "x";
		flagValue[0] = '0' + abs(bd->resigned);
		glScalef(1.5f, 1.3f, 1);
		glPrintCube(bd, flagValue, 0);
	}
	glPopMatrix();

	glEnable(GL_DEPTH_TEST);
}

void drawFlag(BoardData* bd)
{
	float v[4];
	int isStencil = glIsEnabled(GL_STENCIL_TEST);

	if (isStencil)
		glDisable(GL_STENCIL_TEST);

	waveFlag(bd, bd->flagWaved);

	glPushMatrix();

	getFlagPos(bd, v);
	glTranslatef(v[0], v[1], v[2]);

	renderFlag(bd);

	glPopMatrix();
	if (isStencil)
		glEnable(GL_STENCIL_TEST);
}

void updateDieOccPos(BoardData* bd, Occluder* pOcc, int num)
{
	float id[4][4];

	if (bd->shakingDice)
	{
		copyPoint(pOcc->trans, bd->diceMovingPos[num]);

		pOcc->rot[0] = bd->diceRotation[num].xRot;
		pOcc->rot[1] = bd->diceRotation[num].yRot;
		pOcc->rot[2] = bd->dicePos[num][2];

		makeInverseRotateMatrixZ(pOcc->invMat, pOcc->rot[2]);

		makeInverseRotateMatrixX(id, pOcc->rot[1]);
		matrixmult(pOcc->invMat, id);

		makeInverseRotateMatrixY(id, pOcc->rot[0]);
		matrixmult(pOcc->invMat, id);

		makeInverseTransposeMatrix(id, pOcc->trans);
		matrixmult(pOcc->invMat, id);
	}
	else
	{
		getDicePos(bd, num, pOcc->trans);

		makeInverseTransposeMatrix(id, pOcc->trans);

		if (bd->diceShown == DICE_ON_BOARD)
		{
			pOcc->rot[0] = pOcc->rot[1] = 0;
			pOcc->rot[2] = bd->dicePos[num][2];

			makeInverseRotateMatrixZ(pOcc->invMat, pOcc->rot[2]);
			matrixmult(pOcc->invMat, id);
		}
		else
		{
			pOcc->rot[0] = pOcc->rot[1] = pOcc->rot[2] = 0;
			copyMatrix(pOcc->invMat, id);
		}
	}
}

void updateDiceOccPos(BoardData *bd)
{
	int show = DiceShowing(bd);

	bd->Occluders[OCC_DICE1].show = bd->Occluders[OCC_DICE2].show = show;
	if (show)
	{
		updateDieOccPos(bd, &bd->Occluders[OCC_DICE1], 0);
		updateDieOccPos(bd, &bd->Occluders[OCC_DICE2], 1);
	}
}

void updateCubeOccPos(BoardData* bd)
{
	getDoubleCubePos(bd, bd->Occluders[OCC_CUBE].trans);
	makeInverseTransposeMatrix(bd->Occluders[OCC_CUBE].invMat, bd->Occluders[OCC_CUBE].trans);

	bd->Occluders[OCC_CUBE].show = (bd->cube_use && !bd->crawford_game);
}

void updateMovingPieceOccPos(BoardData* bd)
{
	if (bd->drag_point >= 0)
	{
		copyPoint(bd->Occluders[LAST_PIECE].trans, bd->dragPos);
		makeInverseTransposeMatrix(bd->Occluders[LAST_PIECE].invMat, bd->Occluders[LAST_PIECE].trans);
	}
	else /* if (bd->moving) */
	{
		copyPoint(bd->Occluders[LAST_PIECE].trans, bd->movingPos);

		if (bd->rotateMovingPiece > 0)
		{	/* rotate shadow as piece is put in bear off tray */
			float id[4][4];

			bd->Occluders[LAST_PIECE].rotator = 1;
			bd->Occluders[LAST_PIECE].rot[1] = -90 * bd->rotateMovingPiece * bd->turn;
			makeInverseTransposeMatrix(id, bd->Occluders[LAST_PIECE].trans);
			makeInverseRotateMatrixX(bd->Occluders[LAST_PIECE].invMat, bd->Occluders[LAST_PIECE].rot[1]);
			matrixmult(bd->Occluders[LAST_PIECE].invMat, id);
		}
		else
			makeInverseTransposeMatrix(bd->Occluders[LAST_PIECE].invMat, bd->Occluders[LAST_PIECE].trans);
	}
}

void updatePieceOccPos(BoardData* bd)
{
	int i, j, p = OCC_PIECE;

	for (i = 0; i < 28; i++)
	{
		for (j = 1; j <= abs(bd->points[i]); j++)
		{
			getPiecePos(i, j, fClockwise, bd->Occluders[p].trans);

			if (i == 26 || i == 27)
			{	/* bars */
				float id[4][4];

				bd->Occluders[p].rotator = 1;
				if (i == 26)
					bd->Occluders[p].rot[1] = -90;
				else
					bd->Occluders[p].rot[1] = 90;
				makeInverseTransposeMatrix(id, bd->Occluders[p].trans);
				makeInverseRotateMatrixX(bd->Occluders[p].invMat, bd->Occluders[p].rot[1]);
				matrixmult(bd->Occluders[p].invMat, id);
			}
			else
			{
				makeInverseTransposeMatrix(bd->Occluders[p].invMat, bd->Occluders[p].trans);
				bd->Occluders[p].rotator = 0;
			}
			p++;
		}
	}
	if (p == LAST_PIECE)
	{
		updateMovingPieceOccPos(bd);
		bd->Occluders[p].rotator = 0;
	}
}

void updateFlagOccPos(BoardData* bd)
{
	if (bd->resigned)
	{
		freeOccluder(&bd->Occluders[OCC_FLAG]);
		initOccluder(&bd->Occluders[OCC_FLAG]);

		bd->Occluders[OCC_FLAG].show = 1;

		getFlagPos(bd, bd->Occluders[OCC_FLAG].trans);
		makeInverseTransposeMatrix(bd->Occluders[OCC_FLAG].invMat, bd->Occluders[OCC_FLAG].trans);

		/* Flag pole */
		addCube(&bd->Occluders[OCC_FLAG], -FLAGPOLE_WIDTH * 2, -FLAG_HEIGHT, -FLAGPOLE_WIDTH, FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, FLAGPOLE_WIDTH * 2);

		/* Flag surface (approximation) */
		{
			int s;
			/* Change first ctlpoint to better match flag shape */
			float p1x = bd->ctlpoints[1][0][2];
			bd->ctlpoints[1][0][2] *= .7f;

			for (s = 0; s < S_NUMPOINTS - 1; s++)
			{	/* Reduce shadow size a bit to remove artifacts */
				float h = (bd->ctlpoints[s][1][1] - bd->ctlpoints[s][0][1]) * .92f - (FLAG_HEIGHT * .05f);
				float y = bd->ctlpoints[s][0][1] + FLAG_HEIGHT * .05f;
				float w = bd->ctlpoints[s + 1][0][0] - bd->ctlpoints[s][0][0];
				if (s == 2)
					w *= .95f;
				addWonkyCube(&bd->Occluders[OCC_FLAG], bd->ctlpoints[s][0][0], y, bd->ctlpoints[s][0][2],
					w, h, base_unit / 10.0f,
					bd->ctlpoints[s + 1][0][2] - bd->ctlpoints[s][0][2], s);
			}
			bd->ctlpoints[1][0][2] = p1x;
		}
	}
	else
	{
		bd->Occluders[OCC_FLAG].show = 0;
	}
}

void updateHingeOccPos(BoardData* bd)
{
	bd->Occluders[OCC_HINGE1].show = bd->Occluders[OCC_HINGE2].show = bd->showHinges;
}

void updateOccPos(BoardData* bd)
{	/* Make sure shadows are in correct place */
	updateCubeOccPos(bd);
	updateDiceOccPos(bd);
	updatePieceOccPos(bd);
}

void MakeShadowModel(BoardData* bd)
{
	int i;
	TidyShadows(bd);

	initOccluder(&bd->Occluders[OCC_BOARD]);
	addClosedSquare(&bd->Occluders[OCC_BOARD], EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&bd->Occluders[OCC_BOARD], EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&bd->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, EDGE_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&bd->Occluders[OCC_BOARD], TOTAL_WIDTH - TRAY_WIDTH + EDGE_WIDTH, TRAY_HEIGHT + MID_SIDE_GAP_HEIGHT, BASE_DEPTH, TRAY_WIDTH - EDGE_WIDTH * 2, TRAY_HEIGHT - EDGE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&bd->Occluders[OCC_BOARD], TRAY_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH);
	addClosedSquare(&bd->Occluders[OCC_BOARD], TRAY_WIDTH + BOARD_WIDTH + BAR_WIDTH, EDGE_HEIGHT, BASE_DEPTH, BOARD_WIDTH, TOTAL_HEIGHT - EDGE_HEIGHT * 2, EDGE_DEPTH);
	addSquare(&bd->Occluders[OCC_BOARD], 0, 0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, BASE_DEPTH + EDGE_DEPTH);

	setIdMatrix(bd->Occluders[OCC_BOARD].invMat);
	bd->Occluders[OCC_BOARD].trans[0] = bd->Occluders[OCC_BOARD].trans[1] = bd->Occluders[OCC_BOARD].trans[2] = 0;

	initOccluder(&bd->Occluders[OCC_HINGE1]);
	copyOccluder(&bd->Occluders[OCC_HINGE1], &bd->Occluders[OCC_HINGE2]);

	addHalfTube(&bd->Occluders[OCC_HINGE1], HINGE_WIDTH, HINGE_HEIGHT, bd->curveAccuracy / 2);

	bd->Occluders[OCC_HINGE1].trans[0] = bd->Occluders[OCC_HINGE2].trans[0] = (TOTAL_WIDTH) / 2.0f;
	bd->Occluders[OCC_HINGE1].trans[2] = bd->Occluders[OCC_HINGE2].trans[2] = BASE_DEPTH + EDGE_DEPTH;
	bd->Occluders[OCC_HINGE1].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f;
	bd->Occluders[OCC_HINGE2].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f;

	makeInverseTransposeMatrix(bd->Occluders[OCC_HINGE1].invMat, bd->Occluders[OCC_HINGE1].trans);
	makeInverseTransposeMatrix(bd->Occluders[OCC_HINGE2].invMat, bd->Occluders[OCC_HINGE2].trans);

	updateHingeOccPos(bd);

	initOccluder(&bd->Occluders[OCC_CUBE]);
	addSquareCentered(&bd->Occluders[OCC_CUBE], 0, 0, 0, DOUBLECUBE_SIZE * .88f, DOUBLECUBE_SIZE * .88f, DOUBLECUBE_SIZE * .88f);

	updateCubeOccPos(bd);

	initOccluder(&bd->Occluders[OCC_DICE1]);
	addDice(&bd->Occluders[OCC_DICE1], DICE_SIZE / 2.0f);
	copyOccluder(&bd->Occluders[OCC_DICE1], &bd->Occluders[OCC_DICE2]);
	bd->Occluders[OCC_DICE1].rotator = bd->Occluders[OCC_DICE2].rotator = 1;
	updateDiceOccPos(bd);

	initOccluder(&bd->Occluders[OCC_PIECE]);
	{
		float radius = PIECE_HOLE / 2.0f;
		float discradius = radius * 0.8f;
		float lip = radius - discradius;
		float height = PIECE_DEPTH - 2 * lip;

		addCylinder(&bd->Occluders[OCC_PIECE], 0, 0, lip, PIECE_HOLE / 2.0f - LIFT_OFF, height, bd->curveAccuracy);
	}
	for (i = OCC_PIECE; i < OCC_PIECE + 30; i++)
	{
		bd->Occluders[i].rot[0] = 0;
		bd->Occluders[i].rot[2] = 0;
		if (i > OCC_PIECE)
			copyOccluder(&bd->Occluders[OCC_PIECE], &bd->Occluders[i]);
	}

	updatePieceOccPos(bd);
	updateFlagOccPos(bd);
}

void preDraw3d(BoardData* bd)
{
	int transparentPieces = (bd->chequerMat[0].alphaBlend) || (bd->chequerMat[1].alphaBlend);

	if (!bd->qobjTex)
	{
		bd->qobjTex = gluNewQuadric();
		gluQuadricDrawStyle(bd->qobjTex, GLU_FILL);
		gluQuadricNormals(bd->qobjTex, GLU_FLAT);
		gluQuadricTexture(bd->qobjTex, GL_TRUE);
	}
	if (!bd->qobj)
	{
		bd->qobj = gluNewQuadric();
		gluQuadricDrawStyle(bd->qobj, GLU_FILL);
		gluQuadricNormals(bd->qobj, GLU_FLAT);
		gluQuadricTexture(bd->qobj, GL_FALSE);
	}

	preDrawPiece(bd, transparentPieces);
	preDrawDice(bd);

	MakeShadowModel(bd);
}

void SetShadowDimness3d(BoardData* bd)
{	/* Darnkess as percentage of ambient light */
	bd->dim = ((rdAppearance.lightLevels[1] / 100.0f) * (100 - bd->shadowDarkness)) / 100;
}

void drawBoard(BoardData* bd)
{
	drawTable(bd);

	if (bd->cube_use && !bd->crawford_game)
		drawDC(bd);

	/* Draw things in correct order so transparency works correctly */
	/* First pieces, then dice, then moving pieces */
	drawPieces(bd);

	if (bd->State == BOARD_OPEN)
		tidyEdges(bd);

	if (DiceShowing(bd))
		drawDie(bd);

	if (bd->moving || bd->drag_point >= 0)
		drawSpecialPieces(bd);

	if (bd->resigned)
		drawFlag(bd);
}
