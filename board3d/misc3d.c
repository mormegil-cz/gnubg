/*
* misc3d.c
* by Jon Kinsey, 2003
*
* Helper functions for 3d drawing
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
#include <memory.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include "shadow.h"
#include "renderprefs.h"

extern void BuildFont();
extern void setupFlag();
extern void setupDicePaths(BoardData* bd, Path dicePaths[2]);
extern void waveFlag(float wag);
extern void SetTexture(BoardData *bd, Material* pMat, const char* filename);

/* Test function to show normal direction*/
void CheckNormal()
{
	float len;
	GLfloat norms[3];
	glGetFloatv(GL_CURRENT_NORMAL, norms);

	len = (float)sqrt(norms[0] * norms[0] + norms[1] * norms[1] + norms[2] * norms[2]);
	if (fabs(len - 1) > 0.000001)
		len=len; /*break here*/
	norms[0] *= .1f;
	norms[1] *= .1f;
	norms[2] *= .1f;

	glBegin(GL_LINES);
		glVertex3f(0, 0, 0);
		glVertex3f(norms[0], norms[1], norms[2]);
	glEnd();
	glPointSize(5);
	glBegin(GL_POINTS);
		glVertex3f(norms[0], norms[1], norms[2]);
	glEnd();
}

extern float (*light_position)[4];

void SetupLight3d(BoardData * bd)
{
	/* Ugly - store shadow light position here... 
		This is because this position needs to be adjusted (below) */
	static float lp[4];
	float al[4], dl[4], sl[4];

	/* Shadow light position */
	light_position = &lp;

	memcpy(lp, rdAppearance.lightPos, sizeof (float[3]));
	lp[3] = (float)(rdAppearance.lightType == LT_POSITIONAL);

	/* If directioinal vector is from origin */
	if (lp[3] == 0)
	{
		lp[0] -= getBoardWidth() / 2.0f;
		lp[1] -= getBoardHeight() / 2.0f;
	}
	glLightfv(GL_LIGHT0, GL_POSITION, lp);

	al[0] = al[1] = al[2] = rdAppearance.lightLevels[0] / 100.0f;
	al[3] = 1;
	glLightfv(GL_LIGHT0, GL_AMBIENT, al);

	dl[0] = dl[1] = dl[2] = rdAppearance.lightLevels[1] / 100.0f;
	dl[3] = 1;
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dl);

	sl[0] = sl[1] = sl[2] = rdAppearance.lightLevels[2] / 100.0f;
	sl[3] = 1;
	glLightfv(GL_LIGHT0, GL_SPECULAR, sl);
}

void InitGL()
{
	float gal[4];
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);

	/* No global ambient light */
	gal[0] = gal[1] = gal[2] = gal[3] = 0;
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gal);

	/* Local specular viewpoint */
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	/* Nice hints */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	/* Default blend function for alpha-blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	BuildFont();

	setupFlag();

	shadowInit();
}

void setMaterial(Material* pMat)
{
	glMaterialfv(GL_FRONT, GL_AMBIENT, pMat->ambientColour);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, pMat->diffuseColour);
	glMaterialfv(GL_FRONT, GL_SPECULAR, pMat->specularColour);
	glMateriali(GL_FRONT, GL_SHININESS, pMat->shininess);

	if (pMat->pTexture)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, pMat->pTexture->texID);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
}

/* Texture functions */
void DeleteTexture(Texture* texture)
{
	if (texture->texID)
		glDeleteTextures(1, &texture->texID);

	texture->texID = 0;
}

int LoadTexture(Texture* texture, const char* filename)
{
	unsigned char* bits;
	bits = LoadDIBitmap(filename, &texture->width, &texture->height);
	if (!bits)
	{
		g_print("Failed to load texture: %s\n", filename);
		return 0;	/* failed to load file */
	}

	glGenTextures(1, &texture->texID);
	glBindTexture(GL_TEXTURE_2D, texture->texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);

	free(bits);
	return 1;
}

/* Return v position, d distance along path segment */
float moveAlong(float d, PathType type, float start[3], float end[3], float v[3], float* rotate)
{
	float per, lineLen;

	if (type == PATH_LINE)
	{
		float xDiff = end[0] - start[0];
		float yDiff = end[1] - start[1];
		float zDiff = end[2] - start[2];

		lineLen = (float)sqrt(xDiff * xDiff + yDiff * yDiff + zDiff * zDiff);
		if (d <= lineLen)
		{
			per = d / lineLen;
			v[0] = start[0] + xDiff * per;
			v[1] = start[1] + yDiff * per;
			v[2] = start[2] + zDiff * per;

			return -1;
		}
	}
	else if (type == PATH_PARABOLA)
	{
		float dist = end[0] - start[0];
		lineLen = (float)fabs(dist);
		if (d <= lineLen)
		{
			v[0] = start[0] + d * (dist / lineLen);
			v[1] = start[1];
			v[2] = start[2] + 10 * (-d * d + lineLen * d);

			return -1;
		}
	}
	else if (type == PATH_PARABOLA_12TO3)
	{
		float dist = end[0] - start[0];
		lineLen = (float)fabs(dist);
		if (d <= lineLen)
		{
			v[0] = start[0] + d * (dist / lineLen);
			v[1] = start[1];
			d += lineLen;
			v[2] = start[2] + 10 * (-d * d + lineLen * 2 * d) - 10 * lineLen * lineLen;
			return -1;
		}
	}
	else
	{
		float xCent, zCent, xRad, zRad;
		float yOff, yDiff = end[1] - start[1];

		xRad = end[0] - start[0];
		zRad = end[2] - start[2];
		lineLen = PI * (((float)fabs(xRad) + (float)fabs(zRad)) / 2.0f) / 2.0f;
		if (d <= lineLen)
		{
			per = d / lineLen;
			if (rotate)
				*rotate = per;

			if (type == PATH_CURVE_9TO12)
			{
				xCent = end[0];
				zCent = start[2];
				yOff = yDiff * (float)cos((PI / 2.0f) * per);
			}
			else
			{
				xCent = start[0];
				zCent = end[2];
				yOff = yDiff * (float)sin((PI / 2.0f) * per);
			}

			if (type == PATH_CURVE_9TO12)
			{
				v[0] = xCent - xRad * (float)cos((PI / 2.0f) * per);
				v[1] = end[1] - yOff;
				v[2] = zCent + zRad * (float)sin((PI / 2.0f) * per);
			}
			else
			{
				v[0] = xCent + xRad * (float)sin((PI / 2.0f) * per);
				v[1] = start[1] + yOff;
				v[2] = zCent - zRad * (float)cos((PI / 2.0f) * per);
			}
			return -1;
		}
	}
	/* Finished path segment */
	return lineLen;
}

/* Return v position, d distance along path p */
int movePath(Path* p, float d, float *rotate, float v[3])
{
	float done;
	d -= p->mileStone;

	while (!finishedPath(p) && 
		(done = moveAlong(d, p->pathType[p->state], p->pts[p->state], p->pts[p->state + 1], v, rotate)) != -1)
	{	/* Next path segment */
		d -= done;
		p->mileStone += done;
		p->state++;
	}
	return !finishedPath(p);
}

void initPath(Path* p, float start[3])
{
	p->state = 0;
	p->numSegments = 0;
	p->mileStone = 0;
	copyPoint(p->pts[0], start);
}

int finishedPath(Path* p)
{
	return (p->state == p->numSegments);
}

void addPathSegment(Path* p, PathType type, float point[3])
{
	if (type == PATH_PARABOLA_12TO3)
	{	/* Move start y up to top of parabola */
		float l = p->pts[p->numSegments][0] - point[0];
		p->pts[p->numSegments][2] += 10 * l * l;
	}

	p->pathType[p->numSegments] = type;
	p->numSegments++;
	copyPoint(p->pts[p->numSegments], point);
}

/* Return a random number in 0-range */
float randRange(float range)
{
	return range * ((float)rand() / (float)RAND_MAX);
}

/* Setup dice test to initial pos */
void initDT(diceTest* dt, int x, int y, int z)
{
	dt->top = 0;
	dt->bottom = 5;

	dt->side[0] = 3;
	dt->side[1] = 1;
	dt->side[2] = 2;
	dt->side[3] = 4;

	while (x--)
	{
		int temp = dt->top;
		dt->top = dt->side[0];
		dt->side[0] = dt->bottom;
		dt->bottom = dt->side[2];
		dt->side[2] = temp;
	}
	while (y--)
	{
		int temp = dt->top;
		dt->top = dt->side[1];
		dt->side[1] = dt->bottom;
		dt->bottom = dt->side[3];
		dt->side[3] = temp;
	}
	while (z--)
	{
		int temp = dt->side[0];
		dt->side[0] = dt->side[3];
		dt->side[3] = dt->side[2];
		dt->side[2] = dt->side[1];
		dt->side[1] = temp;
	}
}

/* Cube dice has different number format */
void initDTCube(diceTest* dt, int x, int y, int z)
{
	dt->top = 0;
	dt->bottom = 5;

	dt->side[0] = 3;
	dt->side[1] = 1;
	dt->side[2] = 2;
	dt->side[3] = 4;

	while (x--)
	{
		int temp = dt->top;
		dt->top = dt->side[0];
		dt->side[0] = dt->bottom;
		dt->bottom = dt->side[2];
		dt->side[2] = temp;
	}
	while (y--)
	{
		int temp = dt->top;
		dt->top = dt->side[1];
		dt->side[1] = dt->bottom;
		dt->bottom = dt->side[3];
		dt->side[3] = temp;
	}
	while (z--)
	{
		int temp = dt->side[0];
		dt->side[0] = dt->side[3];
		dt->side[3] = dt->side[2];
		dt->side[2] = dt->side[1];
		dt->side[1] = temp;
	}
}

float ***Alloc3d(int x, int y, int z)
{	/* Allocate 3d array */
	int i, j;
	float ***array = (float ***)malloc(sizeof(float) * x);
	for (i = 0; i < x; i++)
	{
		array[i] = (float **)malloc(sizeof(float) * y);
		for (j = 0; j < y; j++)
			array[i][j] = (float *)malloc(sizeof(float) * z);
	}
	return array;
}

void Free3d(float ***array, int x, int y)
{	/* Free 3d array */
	int i, j;
	for (i = 0; i < x; i++)
	{
		for (j = 0; j < y; j++)
			free(array[i][j]);
		free(array[i]);
	}
	free(array);
}

void circleOutline(float radius, float height, int accuracy)
{	/* Draw an ouline of a disc in current z plane */
	int i;
	float angle, step;

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_LINE_STRIP);
	for (i = 0; i <= accuracy; i++)
	{
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);
		angle -= step;
	}
	glEnd();
}

void circle(float radius, float height, int accuracy)
{	/* Draw a disc in current z plane */
	int i;
	float angle, step;

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, height);
	for (i = 0; i <= accuracy; i++)
	{
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);
		angle -= step;
	}
	glEnd();
}

void circleRev(float radius, float height, int accuracy)
{	/* Draw a disc with reverse winding in current z plane */
	int i;
	float angle, step;

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(0, 0, height);
	for (i = 0; i <= accuracy; i++)
	{
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);
		angle += step;
	}
	glEnd();
}

void drawBox(boxType type, float x, float y, float z, float w, float h, float d, Texture* texture)
{	/* Draw a box with normals and optional textures */
	float repX, repY;
	float normX, normY, normZ;

	glPushMatrix();

	glTranslatef(x + w / 2, y + h / 2, z + d / 2);

	glScalef(w / 2.0f, h / 2.0f, d / 2.0f);

	/* Scale normals */
	normX = (w / 2);
	normY = (h / 2);
	normZ = (d / 2);

	if (texture)
	{
		repX = (w * TEXTURE_SCALE) / texture->width;
		repY = (h * TEXTURE_SCALE) / texture->height;
	 
	glBegin(GL_QUADS);
		/* Front Face */
		glNormal3f(0, 0, normZ);
		if (type & BOX_SPLITTOP)
		{
			glTexCoord2f(0, 0); glVertex3f(-1, -1, 1);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 1);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 1);
			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 1);

			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 1);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 1);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 1);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 1);
		}
		else
		if (type & BOX_SPLITWIDTH)
		{
			glTexCoord2f(0, 0); glVertex3f(-1, -1, 1);
			glTexCoord2f(repX / 2.0f, 0); glVertex3f(0, -1, 1);
			glTexCoord2f(repX / 2.0f, repY); glVertex3f(0, 1, 1);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 1);

			glTexCoord2f(repX / 2.0f, 0); glVertex3f(0, -1, 1);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 1);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 1);
			glTexCoord2f(repX / 2.0f, repY); glVertex3f(0, 1, 1);
		}
		else
		{
			glTexCoord2f(0, 0); glVertex3f(-1, -1, 1);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 1);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 1);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 1);
		}
		if (!(type & BOX_NOENDS))
		{
			/* Top Face */
			glNormal3f(0, normY, 0);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, -1);
			glTexCoord2f(0, 0); glVertex3f(-1, 1, 1);
			glTexCoord2f(repX, 0); glVertex3f(1, 1, 1);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, -1);
			/* Bottom Face */
			glNormal3f(0, -normY, 0);
			glTexCoord2f(repX, repY); glVertex3f(-1, -1, -1);
			glTexCoord2f(0, repY); glVertex3f(1, -1, -1);
			glTexCoord2f(0, 0); glVertex3f(1, -1, 1);
			glTexCoord2f(repX, 0); glVertex3f(-1, -1, 1);
		}
		if (!(type & BOX_NOSIDES))
		{
			/* Right face */
			glNormal3f(normX, 0, 0);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, -1);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, -1);
			glTexCoord2f(0, repY); glVertex3f(1, 1, 1);
			glTexCoord2f(0, 0); glVertex3f(1, -1, 1);
			/* Left Face */
			glNormal3f(-normX, 0, 0);
			glTexCoord2f(0, 0); glVertex3f(-1, -1, -1);
			glTexCoord2f(repX, 0); glVertex3f(-1, -1, 1);
			glTexCoord2f(repX, repY); glVertex3f(-1, 1, 1);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, -1);
		}
	glEnd();
	}
	else
	{	/* no texture co-ords */
	glBegin(GL_QUADS);
		/* Front Face */
		if (type & BOX_SPLITTOP)
		{
			glNormal3f(0, 0, normZ);
			glVertex3f(-1, -1, 1);
			glVertex3f(1, -1, 1);
			glVertex3f(1, 0, 1);
			glVertex3f(-1, 0, 1);

			glVertex3f(-1, 0, 1);
			glVertex3f(1, 0, 1);
			glVertex3f(1, 1, 1);
			glVertex3f(-1, 1, 1);
		}
		else
		{
			glNormal3f(0, 0, normZ);
			glVertex3f(-1, -1, 1);
			glVertex3f(1, -1, 1);
			glVertex3f(1, 1, 1);
			glVertex3f(-1, 1, 1);
		}
		if (!(type & BOX_NOENDS))
		{
			/* Top Face */
			glNormal3f(0, normY, 0);
			glVertex3f(-1, 1, -1);
			glVertex3f(-1, 1, 1);
			glVertex3f(1, 1, 1);
			glVertex3f(1, 1, -1);
			/* Bottom Face */
			glNormal3f(0, -normY, 0);
			glVertex3f(-1, -1, -1);
			glVertex3f(1, -1, -1);
			glVertex3f(1, -1, 1);
			glVertex3f(-1, -1, 1);
		}
		if (!(type & BOX_NOSIDES))
		{
			/* Right face */
			glNormal3f(normX, 0, 0);
			glVertex3f(1, -1, -1);
			glVertex3f(1, 1, -1);
			glVertex3f(1, 1, 1);
			glVertex3f(1, -1, 1);
			/* Left Face */
			glNormal3f(-normX, 0, 0);
			glVertex3f(-1, -1, -1);
			glVertex3f(-1, -1, 1);
			glVertex3f(-1, 1, 1);
			glVertex3f(-1, 1, -1);
		}
	glEnd();
	}
	glPopMatrix();
}

void drawCube(float size)
{	/* Draw a simple cube */
	glPushMatrix();
	glScalef(size / 2.0f, size / 2.0f, size / 2.0f);

	glBegin(GL_QUADS);
		/* Front Face */
		glVertex3f(-1, -1, 1);
		glVertex3f(1, -1, 1);
		glVertex3f(1, 1, 1);
		glVertex3f(-1, 1, 1);
		/* Top Face */
		glVertex3f(-1, 1, -1);
		glVertex3f(-1, 1, 1);
		glVertex3f(1, 1, 1);
		glVertex3f(1, 1, -1);
		/* Bottom Face */
		glVertex3f(-1, -1, -1);
		glVertex3f(1, -1, -1);
		glVertex3f(1, -1, 1);
		glVertex3f(-1, -1, 1);
		/* Right face */
		glVertex3f(1, -1, -1);
		glVertex3f(1, 1, -1);
		glVertex3f(1, 1, 1);
		glVertex3f(1, -1, 1);
		/* Left Face */
		glVertex3f(-1, -1, -1);
		glVertex3f(-1, -1, 1);
		glVertex3f(-1, 1, 1);
		glVertex3f(-1, 1, -1);
	glEnd();

	glPopMatrix();
}

void drawRect(float x, float y, float z, float w, float h, Texture* texture)
{	/* Draw a rectangle */
	float repX, repY, tuv;
	
	glPushMatrix();
	glTranslatef(x + w / 2, y + h / 2, z);
	glScalef(w / 2.0f, h / 2.0f, 1);
	glNormal3f(0, 0, 1);
	
	if (texture)
	{
		tuv = TEXTURE_SCALE / texture->width;
		repX = w * tuv;
		repY = h * tuv;

		glBegin(GL_QUADS);
			glTexCoord2f(0, 0); glVertex3f(-1, -1, 0);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 0);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 0);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 0);
		glEnd();
	}
	else
	{
		glBegin(GL_QUADS);
			glVertex3f(-1, -1, 0);
			glVertex3f(1, -1, 0);
			glVertex3f(1, 1, 0);
			glVertex3f(-1, 1, 0);
		glEnd();
	}

	glPopMatrix();
}

void drawSplitRect(float x, float y, float z, float w, float h, Texture* texture)
{	/* Draw a rectangle in 2 bits */
	float repX, repY, tuv;

	glPushMatrix();
	glTranslatef(x + w / 2, y + h / 2, z);
	glScalef(w / 2.0f, h / 2.0f, 1);
	glNormal3f(0, 0, 1);
	
	if (texture)
	{
		tuv = TEXTURE_SCALE / texture->width;
		repX = w * tuv;
		repY = h * tuv;

		glBegin(GL_QUADS);
			glTexCoord2f(0, 0); glVertex3f(-1, -1, 0);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 0);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 0);
			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 0);

			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 0);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 0);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 0);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 0);
		glEnd();
	}
	else
	{
		glBegin(GL_QUADS);
			glVertex3f(-1, -1, 0);
			glVertex3f(1, -1, 0);
			glVertex3f(1, 0, 0);
			glVertex3f(-1, 0, 0);

			glVertex3f(-1, 0, 0);
			glVertex3f(1, 0, 0);
			glVertex3f(1, 1, 0);
			glVertex3f(-1, 1, 0);
		glEnd();
	}

	glPopMatrix();
}

void SetColour(float r, float g, float b, float a)
{
	Material col;
	SetupSimpleMatAlpha(&col, r, g, b, a);
	setMaterial(&col);
}

void SetMovingPieceRotation(BoardData* bd, int pt)
{	/* Make sure piece is rotated correctly while dragging */
	bd->movingPieceRotation = bd->pieceRotation[pt][abs(bd->points[pt])];
}

void PlaceMovingPieceRotation(BoardData* bd, int dest, int src)
{	/* Make sure rotation is the correct in new position */
	bd->pieceRotation[src][abs(bd->points[src])] = bd->pieceRotation[dest][abs(bd->points[dest] - 1)];
	bd->pieceRotation[dest][abs(bd->points[dest]) - 1] = bd->movingPieceRotation;
}

Path path, dicePaths[2];

void updateDicePos(Path* path, DiceRotation *diceRot, float dist, float pos[3])
{
	if (movePath(path, dist, 0, pos))
	{
		diceRot->xRot = (dist * diceRot->xRotFactor + diceRot->xRotStart) * 360;
		diceRot->yRot = (dist * diceRot->yRotFactor + diceRot->yRotStart) * 360;
	}
	else
	{	/* Finished - set to end point */
		copyPoint(pos, path->pts[path->numSegments]);
		diceRot->xRot = diceRot->yRot = 0;
	}
}

#if BUILDING_LIB

#include "../config.h"
#include "../backgammon.h"
#include "../renderprefs.h"
#include "../sound.h"

double animStartTime = 0;
extern int animate_player, *animate_move_list, animation_finished;
int stopNextTime;
int slide_move;
extern int convert_point( int i, int player );

void SetupMove(BoardData* bd)
{
	int destDepth;
    int target = convert_point( animate_move_list[slide_move], animate_player);
    int dest = convert_point( animate_move_list[slide_move + 1], animate_player);
    int dir = animate_player ? 1 : -1;

	bd->points[target] -= dir;

	animStartTime = get_time();

	destDepth = abs(bd->points[dest]) + 1;
	if ((abs(bd->points[dest]) == 1) && (dir != SGN(bd->points[dest])))
		destDepth--;

	setupPath(bd, &path, &bd->rotateMovingPiece, fClockwise, target, abs(bd->points[target]) + 1, dest, destDepth);
	copyPoint(bd->movingPos, path.pts[0]);

	SetMovingPieceRotation(bd, target);

	updatePieceOccPos(bd);
}

int idleAnimate(BoardData* bd)
{
	float elapsedTime = (float)((get_time() - animStartTime) / 1000.0f);
	float vel = .2f + nGUIAnimSpeed * .3f;
	float animateDistance = elapsedTime * vel;

	if (stopNextTime)
	{	/* Stop now - last animation frame has been drawn */
		stopIdleFunc();
		gtk_main_quit();
		return 1;
	}

	if (bd->moving)
	{
		float *pRotate = 0;
		if (bd->rotateMovingPiece != -1 && path.state == 2)
			pRotate = &bd->rotateMovingPiece;

		if (!movePath(&path, animateDistance, pRotate, bd->movingPos))
		{
			int points[2][25];
		    int moveStart = convert_point(animate_move_list[slide_move], animate_player);
			int moveDest = convert_point(animate_move_list[slide_move + 1], animate_player);

			if ((abs(bd->points[moveDest]) == 1) && (bd->turn != SGN(bd->points[moveDest])))
			{	// huff
				if (bd->turn == 1)
					bd->points[0]--;
				else
					bd->points[25]++;
				bd->points[moveDest] = 0;
			}

			bd->points[moveDest] += bd->turn;

			/* Update pip-count mid move */
			read_board(bd, points);
			update_pipcount(bd, points);

			PlaceMovingPieceRotation(bd, moveDest, moveStart);

			/* Next piece */
			slide_move += 2;

			if (slide_move >= 8 || animate_move_list[ slide_move ] < 0)
			{	/* All done */
				bd->moving = 0;
				updatePieceOccPos(bd);
				animation_finished = TRUE;
				stopNextTime = 1;
			}
			else
				SetupMove(bd);

			playSound( SOUND_CHEQUER );
		}
		else
			updateMovingPieceOccPos(bd);
	}

	if (bd->shakingDice)
	{
		if (!finishedPath(&dicePaths[0]))
			updateDicePos(&dicePaths[0], &bd->diceRotation[0], animateDistance / 2.0f, bd->diceMovingPos[0]);
		if (!finishedPath(&dicePaths[1]))
			updateDicePos(&dicePaths[1], &bd->diceRotation[1], animateDistance / 2.0f, bd->diceMovingPos[1]);

		if (finishedPath(&dicePaths[0]) && finishedPath(&dicePaths[1]))
		{
			bd->shakingDice = 0;
			stopNextTime = 1;
		}
		updateDiceOccPos(bd);
	}

	return 1;
}

void RollDice3d(BoardData *bd)
{	/* animate the dice roll if not below board */
	setDicePos(bd);

	if (rdAppearance.animateRoll)
	{
		monitor m;
		SuspendInput( &m );
		animStartTime = get_time();

		bd->shakingDice = 1;
		stopNextTime = 0;
		setIdleFunc(idleAnimate);

		setupDicePaths(bd, dicePaths);
		/* Make sure shadows are in correct place */
		updateOccPos(bd);

		gtk_main();
		ResumeInput( &m );
	}
}

void AnimateMove3d(BoardData *bd)
{
	monitor m;
	SuspendInput( &m );
	slide_move = 0;
	bd->moving = 1;

	SetupMove(bd);

	stopNextTime = 0;
	setIdleFunc(idleAnimate);
	gtk_main();
	ResumeInput( &m );
}

int idleWaveFlag(BoardData* bd)
{
	float elapsedTime = (float)(get_time() - animStartTime);
	bd->flagWaved = elapsedTime / 200;
	updateFlagOccPos(bd);
	return 1;
}

void ShowFlag3d(BoardData *bd)
{
	bd->flagWaved = 0;
	if (rdAppearance.animateFlag && bd->resigned)
	{
		animStartTime = get_time();
		setIdleFunc(idleWaveFlag);
	}
	else
		stopIdleFunc();

	waveFlag(0);
	updateFlagOccPos(bd);
}

#define PATH "Data//"

#else

#define PATH "../Data//"
extern double animStartTime;

#define gtk_main_quit() 0
#define gtk_main() 0

#endif

int idleCloseBoard(BoardData* bd)
{
	float elapsedTime = (float)(get_time() - animStartTime);
	if (bd->State == BOARD_CLOSED)
	{	/* finished */
		stopIdleFunc();
		gtk_main_quit();

		return 1;
	}

	bd->perOpen = (elapsedTime / 3000);
	if (bd->perOpen >= 1)
	{
		bd->perOpen = 1;
		bd->State = BOARD_CLOSED;
	}

	return 1;
}

void CloseBoard3d(BoardData* bd)
{
	EmptyPos(bd);
	bd->State = BOARD_CLOSED;
	/* Turn off most things so they don't interfere when board closed/opening */
	bd->cube_use = 0;
	bd->colour = 0;
	bd->direction = 1;
	bd->resigned = 0;
	fClockwise = 0;

	rdAppearance.showShadows = 0;
	rdAppearance.showMoveIndicator = 0;
	rdAppearance.fLabels = 0;
	bd->dice_roll[0] = 0;
	bd->State = BOARD_CLOSING;

	/* Sort out logo */
	SetupSimpleMat(&bd->logoMat, bd->boxMat.ambientColour[0] * 1.5f, bd->boxMat.ambientColour[1] * 1.5f, bd->boxMat.ambientColour[2] * 1.5f);
	if (bd->boxMat.pTexture)
	{
		if (rand() % 2)
			SetTexture(bd, &bd->logoMat, PATH"logo.bmp");
		else
			SetTexture(bd, &bd->logoMat, PATH"logo2.bmp");
	}
	animStartTime = get_time();
	bd->perOpen = 0;
	setIdleFunc(idleCloseBoard);
	/* As idleCloseBoard assumes last matrix is on stack */
	glPushMatrix();
	
	gtk_main();
}

int MouseMove(BoardData *bd, int x, int y)
{
	if (bd->drag_point >= 0)
	{
		getProjectedPieceDragPos(x, y, bd->dragPos);
		updateMovingPieceOccPos(bd);
		return 1;
	}
	else
		return 0;
}

void Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	SetupViewingVolume();
}

