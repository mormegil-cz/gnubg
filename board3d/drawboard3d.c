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

#ifdef BUILDING_LIB
#include "../../gnubg/renderprefs.h"
#endif

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
void glPrintPointNumbers(const char *text, int mode);
void glPrintCube(const char *text, int mode);
float getFontHeight();
void KillFont();

/* Other functions */
void initPath(Path* p, float start[3]);
void addPathSegment(Path* p, PathType type, float point[3]);
void initDTCube(diceTest* dt, int x, int y, int z);
void initDT(diceTest* dt, int x, int y, int z);
float ***Alloc3d(int x, int y, int z);
void Free3d(float ***array, int x, int y);

GLuint diceList, DCList, pieceList;
GLUquadricObj *qobjTex = 0, *qobj = 0;

/* Define nurbs surface */
GLUnurbsObj *flagNurb;

#define S_NUMPOINTS 4
#define S_NUMKNOTS (S_NUMPOINTS * 2)

#define T_NUMPOINTS 2
#define T_NUMKNOTS (T_NUMPOINTS * 2)

/* Control points for the flag. The Z values are modified to make it wave */
float ctlpoints[S_NUMPOINTS][T_NUMPOINTS][3];

/* All the board element sizes */
#define EDGE base_unit
/* Piece/point size */
#define PIECE_HOLE (base_unit * 3.0f)
/* Vertical distance between pieces */
#define PIECE_GAP (base_unit / 5.0f)
#define BOARD_WIDTH (PIECE_HOLE * 6.0f)
#define BAR_WIDTH (base_unit * 5.0f)
#define TOTAL_WIDTH ((EDGE * 2.0f + PIECE_HOLE + BOARD_WIDTH) * 2.0f + BAR_WIDTH)
#define DICE_AREA_WIDTH BOARD_WIDTH

/* Bottom + top edge */
#define SIDE_EDGE (base_unit * 1.5f)
/* Size of piece trays */
#define PIECE_HOLE_HEIGHT (base_unit * 20.0f)
#define POINT_HEIGHT (PIECE_HOLE_HEIGHT * .95f)
#define MID_SIDE_GAP (base_unit * 3.5f)
#define TOTAL_HEIGHT ((PIECE_HOLE_HEIGHT + SIDE_EDGE) * 2.0f + MID_SIDE_GAP)
#define DICE_AREA_HEIGHT (TOTAL_HEIGHT - (POINT_HEIGHT + SIDE_EDGE) * 2.0f)

#define EDGE_DEPTH (base_unit * 1.95f)
#define PIECE_HEIGHT base_unit
#define BASE_HEIGHT base_unit
 /* Slight offset from surface */
#define LIFT_OFF (base_unit / 50.0f)

#define DICE_SIZE (base_unit * 3.5f)
#define DOT_SIZE (DICE_SIZE / 14.0f)
#define DOUBLECUBE_SIZE (base_unit * 4.0f)

#define HINGE_GAP (base_unit / 12.0f)
#define HINGE_SEGMENTS 6
#define HINGE_WIDTH (base_unit / 2.0f)
#define HINGE_HEIGHT (base_unit * 7.0f)

#define DICE_STEP_SIZE0 (base_unit * 1.3f)
#define DICE_STEP_SIZE1 (base_unit * 1.7f)

#undef ARROW_SIZE
#define ARROW_SIZE (SIDE_EDGE * .8f)
#define ARROW_UNIT (ARROW_SIZE / 4.0f)

#define XROT_MIN 1
#define XROT_RANGE 1.5f

#define YROT_MIN -.5f
#define YROT_RANGE 1

#define FLAG_HEIGHT (base_unit * 3)
#define FLAG_WIDTH (FLAG_HEIGHT * 1.4f)
#define FLAG_WAG (FLAG_HEIGHT * .3f)
#define FLAGPOLE_WIDTH (base_unit * .2f)
#define FLAGPOLE_HEIGHT (FLAG_HEIGHT * 2.05f)

float getBoardWidth() {return TOTAL_WIDTH;}
float getBoardHeight() {return TOTAL_HEIGHT;}

void TidyShadows();

void Tidy3dObjects()
{
	KillFont();
	glDeleteLists(pieceList, 1);
	glDeleteLists(diceList, 1);
	glDeleteLists(DCList, 1);

	gluDeleteQuadric(qobjTex);
	gluDeleteQuadric(qobj);

	gluDeleteNurbsRenderer(flagNurb);
	
	TidyShadows();
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
	float height = PIECE_HEIGHT - 2 * lip;
	float ***p = Alloc3d(bd->step_accuracy + 1, bd->step_accuracy / 4 + 1, 3);
	float ***n = Alloc3d(bd->step_accuracy + 1, bd->step_accuracy / 4 + 1, 3);

	step = (2 * PI) / bd->step_accuracy;

	/* Draw top/bottom of piece */
	if (bd->checkerMat[0].pTexture)
	{	/* Note: texturing will be enabled at this point */
		glPushMatrix();
		glTranslatef(0, 0, PIECE_HEIGHT);
		glBindTexture(GL_TEXTURE_2D, bd->checkerMat[0].pTexture->texID);
		gluDisk(qobjTex, 0, discradius, bd->step_accuracy, 1);
		glPopMatrix();
		/* Draw bottom - faces other way */
		gluQuadricOrientation(qobjTex, GLU_INSIDE);
		gluDisk(qobjTex, 0, discradius, bd->step_accuracy, 1);
		gluQuadricOrientation(qobjTex, GLU_OUTSIDE);
		glDisable(GL_TEXTURE_2D);
	}
	else	
	{
		circle(discradius, PIECE_HEIGHT, bd->step_accuracy);
		circleRev(discradius, 0, bd->step_accuracy);
	}

	angle = 0;
	/* Draw side of piece */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < bd->step_accuracy + 1; i++)
	{
		glNormal3f((float)sin(angle), (float)cos(angle), 0);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, lip);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, lip + height);

		angle += step;
	}
	glEnd();

	/* Draw edges of piece */
	angle2 = 0;
	for (j = 0; j <= bd->step_accuracy / 4; j++)
	{
		latitude = (float)sin(angle2) * lip;
		angle = 0;
		new_radius = (float)sqrt((lip * lip) - (latitude * latitude));

		for (i = 0; i < bd->step_accuracy; i++)
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

	for (j = 0; j < bd->step_accuracy / 4; j++)
	{
		glBegin(GL_QUAD_STRIP);
		for (i = 0; i < bd->step_accuracy + 1; i++)
		{
			glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
			glVertex3f(p[i][j][0], p[i][j][1], p[i][j][2]);
			glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
			glVertex3f(p[i][j + 1][0], p[i][j + 1][1], p[i][j + 1][2]);
		}
		glEnd();
			
		glBegin(GL_QUAD_STRIP);
		for (i = 0; i < bd->step_accuracy + 1; i++)
		{
			glNormal3f((n[i][j + 1][0]) / lip, (n[i][j + 1][1]) / lip, n[i][j + 1][2] / lip);
			glVertex3f(p[i][j + 1][0], p[i][j + 1][1], PIECE_HEIGHT - p[i][j + 1][2]);
			glNormal3f((n[i][j][0]) / lip, (n[i][j][1]) / lip, n[i][j][2] / lip);
			glVertex3f(p[i][j][0], p[i][j][1], PIECE_HEIGHT - p[i][j][2]);
		}
		glEnd();
	}

	Free3d(p, bd->step_accuracy + 1, bd->step_accuracy / 4 + 1);
	Free3d(n, bd->step_accuracy + 1, bd->step_accuracy / 4 + 1);
}

void preDrawPiece1(BoardData* bd)
{
	float pieceRad, pieceBorder;

	pieceRad = PIECE_HOLE / 2.0f;
	pieceBorder = pieceRad * .9f;

	/* Draw top of piece */
	if (bd->checkerMat[0].pTexture)
	{	/* Note: texturing will be enabled at this point */
		glPushMatrix();
		glTranslatef(0, 0, PIECE_HEIGHT);
		glBindTexture(GL_TEXTURE_2D, bd->checkerMat[0].pTexture->texID);
		gluDisk(qobjTex, 0, pieceBorder, bd->step_accuracy, 1);
		glDisable(GL_TEXTURE_2D);
		gluDisk(qobj, pieceBorder, pieceRad, bd->step_accuracy, 1);
		glPopMatrix();
	}
	else
	{
		circle(pieceRad, PIECE_HEIGHT, bd->step_accuracy);
	}
	/* Draw plain bottom of piece */
	circleRev(pieceRad, 0, bd->step_accuracy);

	/* Edge of piece */
	gluCylinder(qobj, pieceRad, pieceRad, PIECE_HEIGHT, bd->step_accuracy, 1);
}

void preDrawPiece(BoardData* bd, int transparent)
{
	if (pieceList)
		glDeleteLists(pieceList, 1);

	pieceList = glGenLists(1);

	glNewList(pieceList, GL_COMPILE);

	switch(bd->pieceType)
	{
	case 0:
		preDrawPiece0(bd);
		break;
	case 1:
		preDrawPiece1(bd);
		break;
	}

	glEndList();
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

	int corner_steps = (bd->step_accuracy / 4) + 1;
	float ***corner_points = Alloc3d(corner_steps, corner_steps, 3);

	radius = size / 2.0f;
	step = (2 * PI) / bd->step_accuracy;

	glPushMatrix();

	/* Draw 6 faces */
	for (c = 0; c < 6; c++)
	{
		circle(radius, radius, bd->step_accuracy);

		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}

	lat_angle = 0;
	lns = (bd->step_accuracy / 4);
	lat_step = (PI / 2) / lns;

	/* Calculate corner points */
	for (i = 0; i < lns + 1; i++)
	{
		latitude = (float)sin(lat_angle) * radius;
		new_radius = (float)sqrt(radius * radius - (latitude * latitude));

		ns = (bd->step_accuracy / 4) - i;

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

		for (i = 0; i < bd->step_accuracy / 4; i++)
		{
			ns = (bd->step_accuracy / 4) - i - 1;
			glBegin(GL_TRIANGLE_STRIP);
				glNormal3f(corner_points[i][0][0] / radius, corner_points[i][0][1] / radius, corner_points[i][0][2] / radius);
				glVertex3f(corner_points[i][0][0], corner_points[i][0][1], corner_points[i][0][2]);
				for (j = 0; j <= ns; j++)
				{
					glNormal3f(corner_points[i + 1][j][0] / radius, corner_points[i + 1][j][1] / radius, corner_points[i + 1][j][2] / radius);
					glVertex3f(corner_points[i + 1][j][0], corner_points[i + 1][j][1], corner_points[i + 1][j][2]);
					glNormal3f(corner_points[i][j + 1][0] / radius, corner_points[i][j + 1][1] / radius, corner_points[i][j + 1][2] / radius);
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

	int corner_steps = (bd->step_accuracy / 4) + 1;
	float ***corner_points = Alloc3d(corner_steps, corner_steps, 3);

	radius = size / 7.0f;

	step = (2 * PI) / bd->step_accuracy;

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
			for (j = 0; j < bd->step_accuracy / 4 + 1; j++)
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
	lat_step = (2 * PI) / bd->step_accuracy;

	/* Calculate corner 1/8th sphere points */
	for (i = 0; i < (bd->step_accuracy / 4) + 1; i++)
	{
		latitude = (float)sin(lat_angle) * radius;
		angle = 0;
		new_radius = (float)sqrt(radius * radius - (latitude * latitude) );

		ns = (bd->step_accuracy / 4) - i;
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

		for (i = 0; i < bd->step_accuracy / 4; i++)
		{
			ns = (bd->step_accuracy / 4) - i - 1;
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
	if (diceList)
		glDeleteLists(diceList, 1);

	diceList = glGenLists(1);
	glNewList(diceList, GL_COMPILE);
		renderDice(bd, DICE_SIZE);
	glEndList();

	if (DCList)
		glDeleteLists(DCList, 1);

	DCList = glGenLists(1);
	glNewList(DCList, GL_COMPILE);
		renderCube(bd, DOUBLECUBE_SIZE);
	glEndList();
}

void getDoubleCubePos(BoardData* bd, float v[3])
{
	if (bd->doubled != 0)
	{
		if (bd->doubled == 1)
			v[0] = EDGE * 2 + PIECE_HOLE + BOARD_WIDTH / 2;
		else
			v[0] = TOTAL_WIDTH - (EDGE * 2 + PIECE_HOLE + BOARD_WIDTH / 2);
		v[1] = TOTAL_HEIGHT / 2.0f;
		v[2] = BASE_HEIGHT + DOUBLECUBE_SIZE / 2.0f;
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
			v[1] = SIDE_EDGE + DOUBLECUBE_SIZE / 2.0f;
			break;
		case 1:
			v[1] = TOTAL_HEIGHT - SIDE_EDGE - DOUBLECUBE_SIZE / 2.0f;
			break;
		default:
		  v[1] = 0;	/* error */
		}
		v[2] = BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE / 2.0f;
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
	int c, tf;
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
		/* Top or Front */
		tf = (side == dt->top || side == dt->side[2]);

		/* Don't draw bottom number or back number */
		if (tf || (bd->doubled && (side == dt->side[1] || side == dt->side[3])))
		{
			if (tf)
				glDisable(GL_DEPTH_TEST);

			glPushMatrix();
			glTranslatef(0, 0, depth + !tf * LIFT_OFF);

			glPrintCube(sides[side], mode);

			glPopMatrix();
			if (tf)
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

	initDTCube(&dt, rotDC[cubeIndex][0], rotDC[cubeIndex][1], rotDC[cubeIndex][2] + extraRot);

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
	glCallList(DCList);

	DrawDCNumbers(bd);

	glPopMatrix();
}

void drawDots(BoardData* bd, float dotOffset, diceTest* dt, int showFront, int drawOutline)
{
	int dots1[] = {2, 2, 0};
	int dots2[] = {1, 1, 3, 3, 0};
	int dots3[] = {1, 3, 2, 2, 3, 1, 0};
	int dots4[] = {1, 1, 1, 3, 3, 1, 3, 3, 0};
	int dots5[] = {1, 1, 1, 3, 2, 2, 3, 1, 3, 3, 0};
	int dots6[] = {1, 1, 1, 3, 2, 1, 2, 3, 3, 1, 3, 3, 0};
	int *dots[6] = {dots1, dots2, dots3, dots4, dots5, dots6};
	int dot_pos[] = {0, 20, 50, 80};	/* percentages across face */
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
		if (nd)
			glDisable(GL_DEPTH_TEST);

		if (bd->shakingDice || 
			(showFront && dot != dt->bottom && dot != dt->side[0])
			|| (!showFront && dot != dt->top))
		{
			glPushMatrix();
			glTranslatef(0, 0, hds + radius);

			glNormal3f(0, 0, 1);

			dp = dots[dot];
			do
			{
				x = (dot_pos[dp[0]] * ds) / 100;
				y = (dot_pos[dp[1]] * ds) / 100;

				glPushMatrix();
				glTranslatef(x - hds, y - hds, 0);

				if (drawOutline)
					circleOutline(DOT_SIZE, dotOffset, bd->step_accuracy);
				else
					circle(DOT_SIZE, dotOffset, bd->step_accuracy);

				glPopMatrix();

				dp += 2;
			} while (*dp);

			glPopMatrix();
		}
		if (nd)
			glEnable(GL_DEPTH_TEST);

		if (c % 2 == 0)
			glRotatef(-90, 0, 1, 0);
		else
			glRotatef(90, 1, 0, 0);
	}
	glPopMatrix();
}

void DrawDots(BoardData* bd, diceTest* dt)
{
	drawDots(bd, LIFT_OFF, dt, 1, 0);

	glLineWidth(0.5f);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	drawDots(bd, LIFT_OFF, dt, 1, 1);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void getDicePos(BoardData* bd, int num, float v[3])
{
	if (DiceBelowBoard(bd))
	{	/* Show below board */
		v[0] = DICE_SIZE * 1.5f;
		v[1] = -DICE_SIZE / 2.0f;
		v[2] = DICE_SIZE / 2.0f;

		if (bd->turn == 1)
			v[0] += TOTAL_WIDTH - DICE_SIZE * 4;
		if (num > 0)
			v[0] += DICE_SIZE;
	}
	else
	{
		if (bd->turn == -1)
		{
			v[0] = EDGE * 2 + PIECE_HOLE;
			v[0] += bd->dicePos[num][0];
		}
		else
		{
			v[0] = TOTAL_WIDTH - EDGE * 2 - PIECE_HOLE;
			v[0] -= bd->dicePos[num][0];
		}
		v[1] = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f + bd->dicePos[num][1];
		v[2] = BASE_HEIGHT + LIFT_OFF + DICE_SIZE / 2.0f;
	}
}

void moveToDicePos(BoardData* bd, int num)
{
	float v[3];
	getDicePos(bd, num, v);
	glTranslatef(v[0], v[1], v[2]);
	if (!DiceBelowBoard(bd))
	{
		/* Spin dice to required rotation if on board */
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
	if (DiceBelowBoard(bd))
	{
		value = bd->dice_roll[num];
		z = 0;
	}
	else
	{
		value = bd->dice[num];
		z = ((int)bd->dicePos[num][2]) / 90;
		if (bd->direction == 1)
			z++;
	}

	glRotatef(90.0f * rotDice[value - 1][0], 1, 0, 0);
	glRotatef(90.0f * rotDice[value - 1][1], 0, 1, 0);

	if (value)
		initDT(&dt, rotDice[value - 1][0], rotDice[value - 1][1], z);
	else
		g_print("no value on dice?\n");

	blend = (bd->diceMat[diceCol].alphaBlend);

	if (blend)
	{	/* Draw back of dice separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);

		setMaterial(&bd->diceMat[diceCol]);
		glCallList(diceList);

		/* Place back dots inside dice */
		setMaterial(&bd->diceDotMat[diceCol]);
		drawDots(bd, -LIFT_OFF, &dt, 0, 0);

		glCullFace(GL_BACK);
	}
	setMaterial(&bd->diceMat[diceCol]);
	glCallList(diceList);

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
		v[2] = BASE_HEIGHT + EDGE_DEPTH + ((pos - 1) / 3) * PIECE_HEIGHT;
		pos = ((pos - 1) % 3) + 1;

		if (point == 25)
		{
			v[1] -= DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP) * (pos + 1);
		}
		else
		{
			v[1] += DOUBLECUBE_SIZE / 2.0f + (PIECE_HOLE + PIECE_GAP) * (pos + 1);
		}
		v[0] -= PIECE_HOLE / 2.0f;
		v[1] -= PIECE_HOLE / 2.0f;
	}
	else if (point >= 26)
	{	/* homes */
		v[2] = BASE_HEIGHT;
		if (swap)
			v[0] = EDGE;
		else
			v[0] = TOTAL_WIDTH - EDGE - PIECE_HOLE;

		if (point == 26)
			v[1] = SIDE_EDGE + (PIECE_HEIGHT * 1.3f * (pos - 1));	/* 1.3 gives a gap between pieces */
		else
			v[1] = TOTAL_HEIGHT - SIDE_EDGE - PIECE_HEIGHT - (PIECE_HEIGHT * 1.3f * (pos - 1));
	}
	else
	{
		v[2] = BASE_HEIGHT + ((pos - 1) / 5) * PIECE_HEIGHT;

		if (point < 13)
		{
			if (swap)
				point = 13 - point;

			if (pos > 10)
				pos -= 10;

			if (pos > 5)
				v[1] = SIDE_EDGE + (PIECE_HOLE / 2.0f) + (PIECE_HOLE + PIECE_GAP) * (pos - 5 - 1);
			else
				v[1] = SIDE_EDGE + (PIECE_HOLE + PIECE_GAP) * (pos - 1);

			v[0] = EDGE * 2.0f + PIECE_HOLE * (13 - point);
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
				v[1] = TOTAL_HEIGHT - SIDE_EDGE - (PIECE_HOLE / 2.0f) - (PIECE_HOLE + PIECE_GAP) * (pos - 5);
			else
				v[1] = TOTAL_HEIGHT - SIDE_EDGE - (PIECE_HOLE + PIECE_GAP) * pos;

			v[0] = EDGE * 2.0f + PIECE_HOLE * (point - 12);
			if (point > 18)
				v[0] += BAR_WIDTH;
		}
	}
	v[2] += LIFT_OFF * 2;

	/* Move to centre of piece */
	v[0] += PIECE_HOLE / 2.0f;
	if (point < 26)
	{
		v[1] += PIECE_HOLE / 2.0f;
	}
	else
	{	/* Home pieces are sideways */
		if (point == 27)
			v[1] += PIECE_HEIGHT;
		v[2] += PIECE_HOLE / 2.0f;
	}
}

void renderPiece(BoardData* bd, int rotation, int colour)
{
	glRotatef((float)rotation, 0, 0, 1);

	setMaterial(&bd->checkerMat[colour]);

	glCallList(pieceList);
}

void drawPiece(BoardData* bd, int point, int pos)
{
	float v[3];
	getPiecePos(point, pos, fClockwise, v);

	glPushMatrix();
	glTranslatef(v[0], v[1], v[2]);

	/* Home pieces are sideways */
	if (point == 26)
		glRotatef(-90, 1, 0, 0);
	if (point == 27)
		glRotatef(90, 1, 0, 0);

	renderPiece(bd, bd->pieceRotation[point][pos - 1], (bd->points[point] > 0));

	glPopMatrix();
}

void drawDraggedPiece(BoardData* bd)
{
	if (bd->drag_point >= 0)
	{
		glPushMatrix();
		glTranslated(bd->dragPos[0], bd->dragPos[1], bd->dragPos[2]);

		renderPiece(bd, bd->movingPieceRotation, (bd->turn == 1));

		glPopMatrix();
	}
}

void drawMovingPiece(BoardData* bd)
{
	if (bd->moving)
	{
		glPushMatrix();
		glTranslatef(bd->movingPos[0], bd->movingPos[1], bd->movingPos[2]);
		if (bd->rotateMovingPiece > 0)
			glRotatef(-90 * bd->rotateMovingPiece * bd->direction, 1, 0, 0);

		renderPiece(bd, bd->movingPieceRotation, (bd->turn == 1));

		glPopMatrix();
	}
}

void drawPieces(BoardData* bd)
{
	int i, j;
	int blend = (bd->checkerMat[0].alphaBlend) || (bd->checkerMat[1].alphaBlend);

	if (blend)
	{	/* Draw back of piece separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);

		for (i = 0; i < 28; i++)
		{
			for (j = 1; j <= abs(bd->points[i]); j++)
			{
				drawPiece(bd, i, j);
			}
		}

		glCullFace(GL_BACK);
	}

	for (i = 0; i < 28; i++)
	{
		for (j = 1; j <= abs(bd->points[i]); j++)
		{
			drawPiece(bd, i, j);
		}
	}

	if (blend)
	glDisable(GL_BLEND);
}

void drawSpecialPieces(BoardData* bd)
{	/* Draw animated or dragged pieces */
	int blend = (bd->checkerMat[0].alphaBlend) || (bd->checkerMat[1].alphaBlend);

	if (blend)
	{	/* Draw back of piece separately */
		glCullFace(GL_FRONT);
		glEnable(GL_BLEND);

		drawMovingPiece(bd);
		drawDraggedPiece(bd);

		glCullFace(GL_BACK);
	}

	drawMovingPiece(bd);
	drawDraggedPiece(bd);

	glDisable(GL_BLEND);
}

void DrawNumbers(BoardData* bd, int sides, int mode)
{
	int i;
	char num[3];
	float x;
	float textHeight = getFontHeight();
	int n;

	glPushMatrix();
	glTranslatef(0, (SIDE_EDGE - textHeight) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
	x = EDGE * 2.0f + PIECE_HOLE / 2.0f;

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
			glPrintPointNumbers(num, mode);
			glPopMatrix();
		}
	}

	glPopMatrix();
	glPushMatrix();
	glTranslatef(0, TOTAL_HEIGHT - textHeight - (SIDE_EDGE - textHeight) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
	x = EDGE * 2.0f + PIECE_HOLE / 2.0f;

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
			glPrintPointNumbers(num, mode);
			glPopMatrix();
		}
	}
	glPopMatrix();
}

void drawNumbers(BoardData* bd, int sides)
{
	/* No need to depth test as on top of board (depth test could cause alias prolems too) */
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
		x = EDGE + PIECE_HOLE * (i + 1);
		y = 0;
	}
	else
	{
		x = EDGE + PIECE_HOLE + BOARD_WIDTH - (PIECE_HOLE * i);
		y = TOTAL_HEIGHT - SIDE_EDGE * 2;
		w = -w;
		h = -h;
	}

	glPushMatrix();
	glTranslatef(EDGE, SIDE_EDGE, BASE_HEIGHT);

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

extern float zebedee;

void drawPoints(BoardData* bd)
{
	int i;
	/* texture unit value */
	float tuv;

	/* Don't worry about depth testing (but set depth values) */
	glDepthFunc(GL_ALWAYS);

	setMaterial(&bd->baseMat);
	drawSplitRect(EDGE, SIDE_EDGE, BASE_HEIGHT, BOARD_WIDTH + EDGE + PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE * 2, bd->baseMat.pTexture);

 	glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_BLEND);

	if (bd->baseMat.pTexture)
		tuv = (TEXTURE_SCALE) / bd->baseMat.pTexture->width;
	else
		tuv = 0;

	for (i = 0; i < 6; i++)
	{
		setMaterial(&bd->pointMat[!(i % 2)]);

		drawPoint(tuv, i, 0);
		drawPoint(tuv, i, 1);
	}
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
	glTranslatef((TOTAL_WIDTH) / 2.0f, height, BASE_HEIGHT + EDGE_DEPTH);
	glRotatef(-90, 1, 0, 0);
	gluCylinder(qobjTex, HINGE_WIDTH, HINGE_WIDTH, HINGE_HEIGHT, bd->step_accuracy, 1);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glRotatef(180, 1, 0, 0);
	gluDisk(qobjTex, 0, HINGE_WIDTH, bd->step_accuracy, 1);

	glPopMatrix();
}

void tidyEdges(BoardData* bd)
{	/* Anti-alias board edges */
	setMaterial(&bd->boxMat);

	glLineWidth(1);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	glNormal3f(0, 0, 1);

glDepthMask(GL_FALSE);
	glBegin(GL_LINES);
		/* bar */
		glVertex3f(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + BAR_WIDTH, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + BAR_WIDTH, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
	glEnd();
glDepthMask(GL_TRUE);

	glBegin(GL_LINES);
		/* left bear off tray */
		glVertex3f(0, 0, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(0, TOTAL_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(EDGE + PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE + PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE + PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE + PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(EDGE * 2 + PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(EDGE * 2 + PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		/* right bear off tray */
		glVertex3f(TOTAL_WIDTH, 0, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH, TOTAL_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE - PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		glVertex3f(TOTAL_WIDTH - EDGE * 2 - PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);
		glVertex3f(TOTAL_WIDTH - EDGE * 2 - PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH);

		/* inner edges (sides) */
		glNormal3f(1, 0, 0);
		glVertex3f(EDGE + LIFT_OFF, SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(EDGE + LIFT_OFF, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(EDGE * 2 + PIECE_HOLE + LIFT_OFF, SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(EDGE * 2 + PIECE_HOLE + LIFT_OFF, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);

		glNormal3f(-1, 0, 0);
		glVertex3f(TOTAL_WIDTH - EDGE + LIFT_OFF, SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - EDGE + LIFT_OFF, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - EDGE * 2 - PIECE_HOLE + LIFT_OFF, SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
		glVertex3f(TOTAL_WIDTH - EDGE * 2 - PIECE_HOLE + LIFT_OFF, TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT + LIFT_OFF);
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

void showMoveIndicator(BoardData* bd)
{
	setMaterial(&bd->checkerMat[(bd->turn == 1)]);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glNormal3f(0, 0, 1);
	glPushMatrix();
	if (!fClockwise)
	{
		if (bd->turn == 1)
			glTranslatef(TOTAL_WIDTH - EDGE - PIECE_HOLE, (SIDE_EDGE - ARROW_SIZE) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
		else
			glTranslatef(TOTAL_WIDTH - EDGE - PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE + (SIDE_EDGE - ARROW_SIZE) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
	}
	else
	{
		if (bd->turn == 1)
			glTranslatef(EDGE + PIECE_HOLE, (SIDE_EDGE + ARROW_SIZE) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
		else
			glTranslatef(EDGE + PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE + (SIDE_EDGE + ARROW_SIZE) / 2.0f, BASE_HEIGHT + EDGE_DEPTH);
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
	SetColour(0, 0, 0, 1);

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

void drawTable(BoardData* bd)
{
	/* Draw background */
	setMaterial(&bd->backGroundMat);

	/* Set depth and default pixel values (as background covers entire screen) */
	glDepthFunc(GL_ALWAYS);
	drawRect(bd->backGroundPos[0], bd->backGroundPos[1], 0, bd->backGroundSize[0], bd->backGroundSize[1], bd->backGroundMat.pTexture);
	glDepthFunc(GL_LEQUAL);

	/* Right side of board */
	if (!rdAppearance.fHinges)
		drawBase(bd, 1 | 2);
	else
		drawBase(bd, 2);
	setMaterial(&bd->boxMat);

	/* Right edge */
	drawBox(BOX_SPLITTOP, TOTAL_WIDTH - EDGE, 0, 0, EDGE, TOTAL_HEIGHT, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);

	/* Top + bottom edges and bar */
	if (!rdAppearance.fHinges)
	{
		drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE, 0, 0, TOTAL_WIDTH - EDGE * 2, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_NOSIDES | BOX_SPLITWIDTH, EDGE, TOTAL_HEIGHT - SIDE_EDGE, 0, TOTAL_WIDTH - EDGE * 2, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, EDGE * 2 + PIECE_HOLE + BOARD_WIDTH, SIDE_EDGE, BASE_HEIGHT, BAR_WIDTH, TOTAL_HEIGHT - SIDE_EDGE * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	}
	else
	{
		drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, 0, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_ALL, (TOTAL_WIDTH + HINGE_GAP) / 2.0f, TOTAL_HEIGHT - SIDE_EDGE, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + (BAR_WIDTH + HINGE_GAP) / 2.0f, SIDE_EDGE, 0, (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - SIDE_EDGE * 2, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);
	}

	/* Bear-off edge */
	drawBox(BOX_NOENDS | BOX_SPLITTOP, TOTAL_WIDTH - (EDGE * 2 + PIECE_HOLE), SIDE_EDGE, BASE_HEIGHT, EDGE, TOTAL_HEIGHT - SIDE_EDGE * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	drawBox(BOX_NOSIDES, TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT, PIECE_HOLE, MID_SIDE_GAP, EDGE_DEPTH, bd->boxMat.pTexture);

	if (rdAppearance.fLabels)
		drawNumbers(bd, 2);

	/* Left side of board*/
	glPushMatrix();

	if (bd->State != BOARD_OPEN)
	{
		float boardAngle = 180;
		if ((bd->State == BOARD_OPENING) || (bd->State == BOARD_CLOSING))
			boardAngle *= bd->perOpen;

		glTranslatef(TOTAL_WIDTH / 2.0f, 0, BASE_HEIGHT + EDGE_DEPTH);
		glRotatef(boardAngle, 0, 1, 0);
		glTranslatef(-TOTAL_WIDTH / 2.0f, 0, -(BASE_HEIGHT + EDGE_DEPTH));
	}

	if (rdAppearance.fHinges)
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
	drawBox(BOX_SPLITTOP, 0, 0, 0, EDGE, TOTAL_HEIGHT, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);

	if (rdAppearance.fHinges)
	{	/* Top + bottom edges and bar */
		drawBox(BOX_ALL, EDGE, 0, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);
		drawBox(BOX_ALL, EDGE, TOTAL_HEIGHT - SIDE_EDGE, 0, (TOTAL_WIDTH - HINGE_GAP) / 2.0f - EDGE, SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);

		drawBox(BOX_NOENDS | BOX_SPLITTOP, EDGE * 2 + PIECE_HOLE + BOARD_WIDTH, SIDE_EDGE, 0, (BAR_WIDTH - HINGE_GAP) / 2.0f, TOTAL_HEIGHT - SIDE_EDGE * 2, BASE_HEIGHT + EDGE_DEPTH, bd->boxMat.pTexture);
	}

	/* Bear-off edge */
	drawBox(BOX_NOENDS | BOX_SPLITTOP, EDGE + PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT, EDGE, TOTAL_HEIGHT - SIDE_EDGE * 2, EDGE_DEPTH, bd->boxMat.pTexture);
	drawBox(BOX_NOSIDES, EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT, BASE_HEIGHT, PIECE_HOLE, MID_SIDE_GAP, EDGE_DEPTH, bd->boxMat.pTexture);

	if (rdAppearance.fHinges)
	{
		drawHinge(bd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f);
		drawHinge(bd, ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f);

		if (bd->State == BOARD_OPEN)
		{	/* Shadow in gap between boards */
			setMaterial(&bd->gap);
			drawRect((TOTAL_WIDTH - HINGE_GAP) / 2.0f, 0, 0, HINGE_GAP, TOTAL_HEIGHT, 0);
		}
	}

	if (rdAppearance.fLabels)
		drawNumbers(bd, 1);

	glPopMatrix();

	if (bd->State == BOARD_OPEN)
		tidyEdges(bd);

	if (bd->showIndicator)
		showMoveIndicator(bd);
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
		{
			drawPiece(bd, i, j);
		}
	}

	/* points */
	for (i = 0; i < 6; i++)
	{
		if (!fClockwise)
			glLoadName(12 - i);
		else
			glLoadName(i + 1);
		drawRect(EDGE * 2 + PIECE_HOLE * (i + 1), SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);

		if (!fClockwise)
			glLoadName(6 - i);
		else
			glLoadName(7 + i);
		drawRect(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + BAR_WIDTH + PIECE_HOLE * i, SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);

		if (!fClockwise)
			glLoadName(24 - i);
		else
			glLoadName(13 + i);
		drawRect(TOTAL_WIDTH - (EDGE * 2 + PIECE_HOLE) - (PIECE_HOLE * i), TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT, -PIECE_HOLE, -POINT_HEIGHT, 0);

		if (!fClockwise)
			glLoadName(18 - i);
		else
			glLoadName(19 + i);
		drawRect(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH - (PIECE_HOLE * i), TOTAL_HEIGHT - SIDE_EDGE, BASE_HEIGHT, -PIECE_HOLE, -POINT_HEIGHT, 0);
	}
	/* bars + homes */
	barHeight = (PIECE_HOLE + PIECE_GAP) * 4;
	glLoadName(25);
	drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f, TOTAL_HEIGHT / 2.0f - (DOUBLECUBE_SIZE / 2.0f + barHeight + PIECE_HOLE / 2.0f),
			BASE_HEIGHT + EDGE_DEPTH, PIECE_HOLE, barHeight, 0);
	glLoadName(0);
	drawRect(TOTAL_WIDTH / 2.0f - PIECE_HOLE / 2.0f, TOTAL_HEIGHT / 2.0f + (DOUBLECUBE_SIZE / 2.0f + PIECE_HOLE / 2.0f),
			BASE_HEIGHT + EDGE_DEPTH, PIECE_HOLE, barHeight, 0);

	glLoadName((!fClockwise) ? 26 : POINT_UNUSED0);
	drawRect(TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);
	glLoadName((fClockwise) ? 26 : POINT_UNUSED0);
	drawRect(EDGE, SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);

	glLoadName((!fClockwise) ? 27 : POINT_UNUSED1);
	drawRect(TOTAL_WIDTH - EDGE - PIECE_HOLE, TOTAL_HEIGHT - SIDE_EDGE - POINT_HEIGHT, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);
	glLoadName((fClockwise) ? 27 : POINT_UNUSED1);
	drawRect(EDGE, TOTAL_HEIGHT - SIDE_EDGE - POINT_HEIGHT, BASE_HEIGHT, PIECE_HOLE, POINT_HEIGHT, 0);

	/* Dice areas */
	glLoadName(POINT_LEFT);
	drawRect(EDGE * 2 + PIECE_HOLE, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_HEIGHT, DICE_AREA_WIDTH, DICE_AREA_HEIGHT, 0);
	glLoadName(POINT_RIGHT);
	drawRect(EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + BAR_WIDTH, (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f, BASE_HEIGHT, DICE_AREA_WIDTH, DICE_AREA_HEIGHT, 0);

	/* Dice */
	if (fGUIDiceArea || !DiceBelowBoard(bd))
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
	getProjectedPos(x, y, BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE + LIFT_OFF * 3, pos);
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
	v[0] = EDGE * 2.0f + PIECE_HOLE / 2.0f + BOARD_WIDTH / 2.0f;
	v[1] = TOTAL_HEIGHT / 2.0f;
	v[2] = BASE_HEIGHT + EDGE_DEPTH;

	if (bd->turn == -1)	/* Move to other side of board */
		v[0] += BOARD_WIDTH + BAR_WIDTH;
}

void waveFlag(float wag)
{
	int i, j;

	/* wave the flag by rotating Z coords though a sine wave */
	for (i = 1; i < S_NUMPOINTS; i++)
		for (j = 0; j < T_NUMPOINTS; j++)
			ctlpoints[i][j][2] = (float)sin((GLfloat)i + wag) * FLAG_WAG;
}

void drawFlagPick(BoardData* bd)
{
	int s;
	float v[3];

	waveFlag(bd->flagWaved);

	glLoadName(POINT_RESIGN);

	glPushMatrix();

	getFlagPos(bd, v);
	glTranslatef(v[0], v[1], v[2]);

	/* Draw flag surface (approximation) */
	glBegin(GL_QUAD_STRIP);
	for (s = 0; s < S_NUMPOINTS; s++)
	{
		glVertex3fv(ctlpoints[s][1]);
		glVertex3fv(ctlpoints[s][0]);
	}
	glEnd();

	/* Draw flag pole */
	glTranslatef(0, -FLAG_HEIGHT, 0);

	glRotatef(-90, 1, 0, 0);
	gluCylinder(qobj, FLAGPOLE_WIDTH, FLAGPOLE_WIDTH, FLAGPOLE_HEIGHT, bd->step_accuracy, 1);

	circleRev(FLAGPOLE_WIDTH, 0, bd->step_accuracy);
	circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, bd->step_accuracy);

	glPopMatrix();
}

void drawPointPick(BoardData* bd, int point)
{	/* Draw sub parts of point to work out which part of point clicked */
	int i;
	float pos[3];

	if (point >= 0 && point <= 25)
	{
		for (i = 1; i < 6; i++)
		{
			if ((point == 0 || point == 25) && i == 4)
				break;
			getPiecePos(point, i, fClockwise, pos);
			glLoadName(i);
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1] - (PIECE_HOLE / 2.0f), pos[2], PIECE_HOLE, PIECE_HOLE + PIECE_GAP, 0);
		}
		/* extra bit */
		glLoadName(i);
		if (point > 12)
		{
			pos[1] = pos[1] - (PIECE_HOLE / 2.0f);
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), TOTAL_HEIGHT - SIDE_EDGE - POINT_HEIGHT, pos[2], PIECE_HOLE, pos[1] - (TOTAL_HEIGHT - SIDE_EDGE - POINT_HEIGHT), 0);
		}
		else
		{
			pos[1] = pos[1] + (PIECE_HOLE / 2.0f) + PIECE_GAP;
			drawRect(pos[0] - (PIECE_HOLE / 2.0f), pos[1], pos[2], PIECE_HOLE, SIDE_EDGE + POINT_HEIGHT - pos[1], 0);
		}
	}
}

int board_point(BoardData *bd, int x, int y, int point)
{	/* Identify if anything is below point (x,y) */
	GLint viewport[4];
	#define BUFSIZE 512
	GLuint selectBuf[BUFSIZE];
	GLint hits;

	glSelectBuffer(BUFSIZE, selectBuf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	glGetIntegerv (GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	glLoadIdentity();
	gluPickMatrix(x, y, 1, 1, viewport);
	SetupPerspVolume(bd, viewport);

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
		switch (fromBoard)
		{
		case 1:
			fromBoard = 2;
			break;
		case 2:
			fromBoard = 1;
			break;
		case 3:
			fromBoard = 4;
			break;
		case 4:
			fromBoard = 3;
			break;
		}
		switch (toBoard)
		{
		case 1:
			toBoard = 2;
			break;
		case 2:
			toBoard = 1;
			break;
		case 3:
			toBoard = 4;
			break;
		case 4:
			toBoard = 3;
			break;
		}
	}

	/* Work out what obstacle needs to be avoided */
	if ((fromBoard == 2 || fromBoard == 3) && (toBoard == 1 || toBoard == 4))
	{	/* left side to right side */
		obj1 = EDGE * 2.0f + PIECE_HOLE * 7;
		obj2 = obj1 + BAR_WIDTH;
		objHeight = BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE;
	}
 	else if ((fromBoard == 1 || fromBoard == 4) && (toBoard == 2 || toBoard == 3))
	{	/* right side to left side */
		obj2 = EDGE * 2.0f + PIECE_HOLE * 7;
		obj1 = obj2 + BAR_WIDTH;
		objHeight = BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE;
	}
	else if ((fromBoard == 1 && toBoard == 4) || (fromBoard == 2 && toBoard == 3))
	{	/* up same side */
		obj1 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
		obj2 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
		objHeight = BASE_HEIGHT + DICE_SIZE;
		yDir = 1;
	}
	else if ((fromBoard == 4 && toBoard == 1) || (fromBoard == 3 && toBoard == 2))
	{	/* down same side */
		obj1 = (TOTAL_HEIGHT + DICE_AREA_HEIGHT) / 2.0f;
		obj2 = (TOTAL_HEIGHT - DICE_AREA_HEIGHT) / 2.0f;
		objHeight = BASE_HEIGHT + DICE_SIZE;
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
			fromPoint = 13 - fromPoint;
			toPoint = 13 - toPoint;
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
			fromPoint = fromPoint - 12;
			toPoint = toPoint - 12;
		}
		obj1 = EDGE * 2.0f + PIECE_HOLE * fromPoint;
		obj2 = EDGE * 2.0f + PIECE_HOLE * toPoint;
		if ((fromBoard == 1) || (fromBoard == 4))
		{
			obj1 += BAR_WIDTH;
			obj2 += BAR_WIDTH;
		}

		objHeight = BASE_HEIGHT + PIECE_HEIGHT * 3;
	}
	else
	{
		if (fromPoint == 0 || fromPoint == 25)
		{	/* Move from bar */
			if (!fClockwise)
			{
				obj1 = EDGE * 2.0f + PIECE_HOLE * 7 + BAR_WIDTH;
				if (fromPoint == 0)
					obj2 = EDGE * 2.0f + BAR_WIDTH + PIECE_HOLE * (toPoint - 13);
				else
					obj2 = EDGE * 2.0f + BAR_WIDTH + PIECE_HOLE * (13 - toPoint);
			}
			else
			{
				obj1 = EDGE * 2.0f + PIECE_HOLE * 7;
				if (fromPoint == 0)
					obj2 = EDGE * 2.0f + PIECE_HOLE * (26 - toPoint);
				else
					obj2 = EDGE * 2.0f + PIECE_HOLE * (toPoint + 1);
			}
			objHeight = BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE;
		}
		else
		{	/* Move home */
			if (!fClockwise)
			{
				if (toPoint == 26)
					obj1 = EDGE * 2.0f + BAR_WIDTH + PIECE_HOLE * (14 - fromPoint);
				else /* (toPoint == 27) */
					obj1 = EDGE * 2.0f + BAR_WIDTH + PIECE_HOLE * (fromPoint - 11);
			}
			else
			{
				if (toPoint == 26)
					obj1 = EDGE * 2.0f + PIECE_HOLE * (fromPoint);
				else /* (toPoint == 27) */
					obj1 = EDGE * 2.0f + PIECE_HOLE * (25 - fromPoint);
			}

			if (!fClockwise)
				obj2 = TOTAL_WIDTH - EDGE - PIECE_HOLE;
			else
				obj2 = EDGE + PIECE_HOLE;

//			if (toPoint == 26)
//				end[1] -= PIECE_HEIGHT;
			*pRotate = 0;
			objHeight = BASE_HEIGHT + EDGE_DEPTH + DICE_SIZE;
		}
	}
	if ((fromBoard == 3 && toBoard == 4) || (fromBoard == 4 && toBoard == 3))
	{	/* Special case when moving along top of board */
		if ((bd->cube_owner != 1) && (fromDepth <= 2) && (toDepth <= 2))
			objHeight = BASE_HEIGHT + EDGE_DEPTH + PIECE_HEIGHT;
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
	int dir = (bd->turn == 1) ? -1 : 1;
	setupDicePath(0, dir * DICE_STEP_SIZE0, &dicePaths[0], bd);
	setupDicePath(1, dir * DICE_STEP_SIZE1, &dicePaths[1], bd);

	randomDiceRotation(&dicePaths[0], &bd->diceRotation[0], dir);
	randomDiceRotation(&dicePaths[1], &bd->diceRotation[1], dir);
	copyPoint(bd->diceMovingPos[0], dicePaths[0].pts[0]);
	copyPoint(bd->diceMovingPos[1], dicePaths[1].pts[0]);
}

void updateDiceOccPos(BoardData *bd);

void setDicePos(BoardData* bd)
{
	float dist;
	float orgX[2];
	float firstX = (DICE_STEP_SIZE0 * 3 + DICE_SIZE / 2.0f);
	bd->dicePos[0][0] = firstX + randRange(DICE_AREA_WIDTH - firstX - DICE_SIZE * 2);
	bd->dicePos[0][1] = randRange(DICE_AREA_HEIGHT);

	do	/* insure dice are not too close together */
	{
		firstX = bd->dicePos[0][0] + DICE_SIZE;
		bd->dicePos[1][0] = firstX + randRange(DICE_AREA_WIDTH - firstX - DICE_SIZE * .7f);
		bd->dicePos[1][1] = randRange(DICE_AREA_HEIGHT);

		orgX[0] = bd->dicePos[0][0] - DICE_STEP_SIZE0 * 5;
		orgX[1] = bd->dicePos[1][0] - DICE_STEP_SIZE1 * 5;
		dist = (float)sqrt((orgX[1] - orgX[0]) * (orgX[1] - orgX[0])
				+ (bd->dicePos[1][1] - bd->dicePos[0][1]) * (bd->dicePos[1][1] - bd->dicePos[0][1]));
	}
	while (dist < DICE_SIZE * 1.1f);

	bd->dicePos[0][2] = (float)(rand() % 360);
	bd->dicePos[1][2] = (float)(rand() % 360);

	if (bd->turn == 1)	/* Swap dice on right side */
	{
		float temp[3];
		copyPoint(temp, bd->dicePos[0]);
		copyPoint(bd->dicePos[0], bd->dicePos[1]);
		copyPoint(bd->dicePos[1], temp);
	}

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

/* Calculate correct viewarea for board/fov angles */
typedef struct _viewArea
{
	float top;
	float bottom;
	float width;
} viewArea;

void initViewArea(viewArea* pva)
{	/* Initialize values to extreme */
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

void SetupPerspVolume(BoardData* bd, int viewport[4])
{
	float fovScale, vert, hor;
	float zoom;
	float aspectRatio = (float)viewport[2]/(float)(viewport[3]);

	float halfRadianFOV;
	float p[3];
	float boardRadAngle;
	viewArea va;
	initViewArea(&va);

	boardRadAngle = (bd->boardAngle * PI) / 180.0f;
	halfRadianFOV = ((bd->fovAngle * PI) / 180.0f) / 2.0f;

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
	addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, -getBoardHeight() / 2, BASE_HEIGHT + EDGE_DEPTH);

	if (fGUIDiceArea)
	{
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 + DICE_SIZE, 0);
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 + DICE_SIZE, BASE_HEIGHT + DICE_SIZE);
	}
	else
	{	/* Bottom edge is defined by board */
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2, 0);
		addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2, BASE_HEIGHT + EDGE_DEPTH);
	}

	if (!bd->doubled)
	{
		if (bd->cube_owner == 1)
			addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, -(getBoardHeight() / 2 - SIDE_EDGE), BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE);
		if (bd->cube_owner == -1)
			addViewAreaHeightPoint(&va, halfRadianFOV, boardRadAngle, getBoardHeight() / 2 - SIDE_EDGE, BASE_HEIGHT + EDGE_DEPTH + DOUBLECUBE_SIZE);
	}

	p[0] = getBoardWidth() / 2;
	p[1] = getBoardHeight() / 2;
	p[2] = BASE_HEIGHT + EDGE_DEPTH;
	workOutWidth(&va, halfRadianFOV, boardRadAngle, aspectRatio, p);

	#define zNear .1f
	#define zFar 70

	fovScale = zNear * (float)tan(halfRadianFOV);

	if (aspectRatio > getAreaRatio(&va))
	{
		vert = fovScale;
		hor = vert * aspectRatio;
	}
	else
	{
		hor = fovScale * getAreaRatio(&va);
		vert = hor / aspectRatio;
	}
	/* Setup projection matrix */
	glFrustum(-hor, hor, -vert, vert, zNear, zFar);

	/* Setup modelview matrix */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* Zoom back so image fills window */
	zoom = (getViewAreaHeight(&va) / 2) / (float)tan(halfRadianFOV);
	glTranslatef(0, 0, -zoom);

	/* Offset from centre because of perspective */
	glTranslatef(0, getViewAreaHeight(&va) / 2 + va.bottom, 0);

	/* Rotate board */
	glRotatef(bd->boardAngle, -1, 0, 0);

	/* Origin is bottom left, so move from centre */
	glTranslatef(-(getBoardWidth() / 2.0f), -((getBoardHeight()) / 2.0f), 0);
}

void setupFlag()
{
	int i;
	float width = FLAG_WIDTH;
	float height = FLAG_HEIGHT;

	/* Generate normal co-ords for nurbs */
	glEnable(GL_AUTO_NORMAL);

	flagNurb = gluNewNurbsRenderer();

	for (i = 0; i < S_NUMPOINTS; i++)
	{
		ctlpoints[i][0][0] = width / (S_NUMPOINTS - 1) * i;
		ctlpoints[i][1][0] = width / (S_NUMPOINTS - 1) * i;
		ctlpoints[i][0][1] = 0;
		ctlpoints[i][1][1] = height;
	}
	for (i = 0; i < T_NUMPOINTS; i++)
		ctlpoints[0][i][2] = 0;
}

void renderFlag(BoardData* bd)
{
	/* Simple knots i.e. no weighting */
	float s_knots[S_NUMKNOTS] = {0, 0, 0, 0, 1, 1, 1, 1};
	float t_knots[T_NUMKNOTS] = {0, 0, 1, 1};

	/* Draw flag surface */
	setMaterial(&bd->flagMat);

	/* Set size of polygons */
	gluNurbsProperty(flagNurb, GLU_SAMPLING_TOLERANCE, 500.0f / bd->step_accuracy);

	gluBeginSurface(flagNurb);
		gluNurbsSurface(flagNurb, S_NUMKNOTS, s_knots, T_NUMKNOTS, t_knots, 3 * T_NUMPOINTS, 3,
						(float*)ctlpoints, S_NUMPOINTS, T_NUMPOINTS, GL_MAP2_VERTEX_3);
	gluEndSurface(flagNurb);

	/* Draw flag pole */
	glPushMatrix();

	glTranslatef(-FLAGPOLE_WIDTH, -FLAG_HEIGHT, 0);

	glRotatef(-90, 1, 0, 0);
	SetColour(.2f, .2f, .4f, 0);	/* Blue pole */
	gluCylinder(qobj, FLAGPOLE_WIDTH, FLAGPOLE_WIDTH, FLAGPOLE_HEIGHT, bd->step_accuracy, 1);

	circleRev(FLAGPOLE_WIDTH, 0, bd->step_accuracy);
	circleRev(FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, bd->step_accuracy);

	glPopMatrix();

	/* Draw number on flag */
	glDisable(GL_DEPTH_TEST);

	setMaterial(&bd->cubeNumberMat);

	glPushMatrix();
	{
		/* Move to middle of flag */
		float v[3];
		v[0] = (ctlpoints[1][0][0] + ctlpoints[2][0][0]) / 2.0f;
		v[1] = (ctlpoints[1][0][0] + ctlpoints[1][1][0]) / 2.0f;
		v[2] = (ctlpoints[1][0][2] + ctlpoints[2][0][2]) / 2.0f;
		glTranslatef(v[0], v[1], v[2]);
	}
	{
		/* Work out approx angle of number based on control points */
		float ang = (float)atan(-(ctlpoints[2][0][2] - ctlpoints[1][0][2]) / (ctlpoints[2][0][0] - ctlpoints[1][0][0]));
		float degAng = (ang) * 180 / PI;

		glRotatef(degAng, 0, 1, 0);
	}

	{
		/* Draw number */
		char flagValue[2] = "x";
		flagValue[0] = '0' + abs(bd->resigned);
		glScalef(1.5f, 1.3f, 1);
		glPrintCube(flagValue, 0);
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

	waveFlag(bd->flagWaved);

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

		if (!DiceBelowBoard(bd))
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
	int show = (bd->dice_roll[0] > 0 && (fGUIDiceArea || !DiceBelowBoard(bd)));

	Occluders[OCC_DICE1].show = Occluders[OCC_DICE2].show = show;

	updateDieOccPos(bd, &Occluders[OCC_DICE1], 0);
	updateDieOccPos(bd, &Occluders[OCC_DICE2], 1);
}

void updateCubeOccPos(BoardData* bd)
{
	getDoubleCubePos(bd, Occluders[OCC_CUBE].trans);
	makeInverseTransposeMatrix(Occluders[OCC_CUBE].invMat, Occluders[OCC_CUBE].trans);

	Occluders[OCC_CUBE].show = (bd->cube_use && !bd->crawford_game);
}

void updateMovingPieceOccPos(BoardData* bd)
{
	if (bd->drag_point >= 0)
	{
		copyPoint(Occluders[LAST_PIECE].trans, bd->dragPos);
		makeInverseTransposeMatrix(Occluders[LAST_PIECE].invMat, Occluders[LAST_PIECE].trans);
	}
	else /* if (bd->moving) */
	{
		copyPoint(Occluders[LAST_PIECE].trans, bd->movingPos);

		if (bd->rotateMovingPiece > 0)
		{	/* rotate shadow as piece is put in bear off tray */
			float id[4][4];

			Occluders[LAST_PIECE].rotator = 1;
			Occluders[LAST_PIECE].rot[1] = -90 * bd->rotateMovingPiece * bd->direction;
			makeInverseTransposeMatrix(id, Occluders[LAST_PIECE].trans);
			makeInverseRotateMatrixX(Occluders[LAST_PIECE].invMat, Occluders[LAST_PIECE].rot[1]);
			matrixmult(Occluders[LAST_PIECE].invMat, id);
		}
		else
			makeInverseTransposeMatrix(Occluders[LAST_PIECE].invMat, Occluders[LAST_PIECE].trans);
	}
}

void updatePieceOccPos(BoardData* bd)
{
	int i, j, p = OCC_PIECE;

	for (i = 0; i < 28; i++)
	{
		for (j = 1; j <= abs(bd->points[i]); j++)
		{
			getPiecePos(i, j, fClockwise, Occluders[p].trans);

			if (i == 26 || i == 27)
			{	/* bars */
				float id[4][4];

				Occluders[p].rotator = 1;
				if (i == 26)
					Occluders[p].rot[1] = -90;
				else
					Occluders[p].rot[1] = 90;
				makeInverseTransposeMatrix(id, Occluders[p].trans);
				makeInverseRotateMatrixX(Occluders[p].invMat, Occluders[p].rot[1]);
				matrixmult(Occluders[p].invMat, id);
			}
			else
			{
				makeInverseTransposeMatrix(Occluders[p].invMat, Occluders[p].trans);
				Occluders[p].rotator = 0;
			}
			p++;
		}
	}
	if (p == LAST_PIECE)
	{
		updateMovingPieceOccPos(bd);
		Occluders[p].rotator = 0;
	}
}

void updateFlagOccPos(BoardData* bd)
{
	int s;
	if (bd->resigned)
	{
		freeOccluder(&Occluders[OCC_FLAG]);
		initOccluder(&Occluders[OCC_FLAG]);

		Occluders[OCC_FLAG].show = 1;

		getFlagPos(bd, Occluders[OCC_FLAG].trans);
		makeInverseTransposeMatrix(Occluders[OCC_FLAG].invMat, Occluders[OCC_FLAG].trans);

		/* Flag pole */
		addCube(&Occluders[OCC_FLAG], -FLAGPOLE_WIDTH * 2, -FLAG_HEIGHT, -FLAGPOLE_WIDTH, FLAGPOLE_WIDTH * 2, FLAGPOLE_HEIGHT, FLAGPOLE_WIDTH * 2);

		/* Flag surface (approximation) */
		{
		/* Change first ctlpoint to better match flag shape */
		float p1x = ctlpoints[1][0][2];
		ctlpoints[1][0][2] *= .7f;

		for (s = 0; s < S_NUMPOINTS - 1; s++)
		{
			addWonkyCube(&Occluders[OCC_FLAG], ctlpoints[s][0][0], ctlpoints[s][0][1], ctlpoints[s][0][2],
				ctlpoints[s + 1][0][0] - ctlpoints[s][0][0], ctlpoints[s][1][1] - ctlpoints[s][0][1], 
				base_unit / 10.0f,
				ctlpoints[s + 1][0][2] - ctlpoints[s][0][2], s);
		}
		ctlpoints[1][0][2] = p1x;
		}
	}
	else
	{
		Occluders[OCC_FLAG].show = 0;
	}
}

void updateHingeOccPos(BoardData* bd)
{
	Occluders[OCC_HINGE1].show = Occluders[OCC_HINGE2].show = rdAppearance.fHinges;
}

void updateOccPos(BoardData* bd)
{	/* Make sure shadows are in correct place */
	updateCubeOccPos(bd);
	updateDiceOccPos(bd);
	updatePieceOccPos(bd);
}

void make_model(BoardData* bd)
{
	int i;

	TidyShadows();

	initOccluder(&Occluders[OCC_BOARD]);
	addClosedSquare(&Occluders[OCC_BOARD], EDGE, SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, PIECE_HOLE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&Occluders[OCC_BOARD], EDGE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT, PIECE_HOLE, PIECE_HOLE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&Occluders[OCC_BOARD], TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT, PIECE_HOLE, PIECE_HOLE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&Occluders[OCC_BOARD], TOTAL_WIDTH - EDGE - PIECE_HOLE, SIDE_EDGE + PIECE_HOLE_HEIGHT + MID_SIDE_GAP, BASE_HEIGHT, PIECE_HOLE, PIECE_HOLE_HEIGHT, EDGE_DEPTH);
	addClosedSquare(&Occluders[OCC_BOARD], EDGE * 2 + PIECE_HOLE, SIDE_EDGE, BASE_HEIGHT, BOARD_WIDTH, TOTAL_HEIGHT - SIDE_EDGE * 2, EDGE_DEPTH);
	addClosedSquare(&Occluders[OCC_BOARD], EDGE * 2 + PIECE_HOLE + BOARD_WIDTH + BAR_WIDTH, SIDE_EDGE, BASE_HEIGHT, BOARD_WIDTH, TOTAL_HEIGHT - SIDE_EDGE * 2, EDGE_DEPTH);
	addSquare(&Occluders[OCC_BOARD], 0, 0, 0, TOTAL_WIDTH, TOTAL_HEIGHT, BASE_HEIGHT + EDGE_DEPTH);

	setIdMatrix(Occluders[OCC_BOARD].invMat);
	Occluders[OCC_BOARD].trans[0] = Occluders[OCC_BOARD].trans[1] = Occluders[OCC_BOARD].trans[2] = 0;

	initOccluder(&Occluders[OCC_HINGE1]);
	copyOccluder(&Occluders[OCC_HINGE1], &Occluders[OCC_HINGE2]);

	addHalfTube(&Occluders[OCC_HINGE1], HINGE_WIDTH, HINGE_HEIGHT, bd->step_accuracy / 2);

	Occluders[OCC_HINGE1].trans[0] = Occluders[OCC_HINGE2].trans[0] = (TOTAL_WIDTH) / 2.0f;
	Occluders[OCC_HINGE1].trans[2] = Occluders[OCC_HINGE2].trans[2] = BASE_HEIGHT + EDGE_DEPTH;
	Occluders[OCC_HINGE1].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT) / 2.0f;
	Occluders[OCC_HINGE2].trans[1] = ((TOTAL_HEIGHT / 2.0f) - HINGE_HEIGHT + TOTAL_HEIGHT) / 2.0f;

	makeInverseTransposeMatrix(Occluders[OCC_HINGE1].invMat, Occluders[OCC_HINGE1].trans);
	makeInverseTransposeMatrix(Occluders[OCC_HINGE2].invMat, Occluders[OCC_HINGE2].trans);

	updateHingeOccPos(bd);

	initOccluder(&Occluders[OCC_CUBE]);
	addSquareCentered(&Occluders[OCC_CUBE], 0, 0, 0, DOUBLECUBE_SIZE * .88f, DOUBLECUBE_SIZE * .88f, DOUBLECUBE_SIZE * .88f);

	updateCubeOccPos(bd);

	initOccluder(&Occluders[OCC_DICE1]);
	addCubeCentered(&Occluders[OCC_DICE1], 0, 0, 0, DICE_SIZE * .86f, DICE_SIZE * .86f, DICE_SIZE * .86f);
	copyOccluder(&Occluders[OCC_DICE1], &Occluders[OCC_DICE2]);
	Occluders[OCC_DICE1].rotator = Occluders[OCC_DICE2].rotator = 1;

	initOccluder(&Occluders[OCC_PIECE]);
	{
		float radius = PIECE_HOLE / 2.0f;
		float discradius = radius * 0.8f;
		float lip = radius - discradius;
		float height = PIECE_HEIGHT - 2 * lip;

		addCylinder(&Occluders[OCC_PIECE], 0, 0, lip, PIECE_HOLE / 2.0f, height, bd->step_accuracy);
	}
	for (i = OCC_PIECE; i < OCC_PIECE + 30; i++)
	{
		Occluders[i].rot[0] = 0;
		Occluders[i].rot[2] = 0;
		if (i > OCC_PIECE)
			copyOccluder(&Occluders[OCC_PIECE], &Occluders[i]);
	}

	updatePieceOccPos(bd);

	updateFlagOccPos(bd);
}

void preDrawThings(BoardData* bd)
{
	int transparentPieces = (bd->checkerMat[0].alphaBlend) || (bd->checkerMat[1].alphaBlend);

	if (!qobjTex)
	{
		qobjTex = gluNewQuadric();
		gluQuadricDrawStyle(qobjTex, GLU_FILL);
		gluQuadricNormals(qobjTex, GLU_FLAT);
		gluQuadricTexture(qobjTex, GL_TRUE);
	}
	if (!qobj)
	{
		qobj = gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL);
		gluQuadricNormals(qobj, GLU_FLAT);
		gluQuadricTexture(qobj, GL_FALSE);
	}

	preDrawPiece(bd, transparentPieces);
	preDrawDice(bd);

	make_model(bd);
}

void TidyShadows()
{
	freeOccluder(&Occluders[OCC_BOARD]);
	freeOccluder(&Occluders[OCC_CUBE]);
	freeOccluder(&Occluders[OCC_DICE1]);
	freeOccluder(&Occluders[OCC_FLAG]);
	freeOccluder(&Occluders[OCC_HINGE1]);
	freeOccluder(&Occluders[OCC_PIECE]);
}

void SetShadowDimness(BoardData* bd, int percent)
{
	dim = (bd->LightDiffuse * (100 - percent)) / 100;
}

void drawBoard(BoardData* bd)
{
	drawTable(bd);

	if (bd->cube_use && !bd->crawford_game)
		drawDC(bd);

	/* Draw things in correct order so transparency works correctly */
	/* First pieces, then dice, then moving pieces */
	drawPieces(bd);

	if (bd->dice_roll[0] > 0 && (fGUIDiceArea || !DiceBelowBoard(bd)))
		drawDie(bd);

	if (bd->moving || bd->drag_point >= 0)
		drawSpecialPieces(bd);

	if (bd->resigned)
		drawFlag(bd);
}
