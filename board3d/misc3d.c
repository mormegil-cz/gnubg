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
#include <stdio.h>
#include <memory.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include "shadow.h"
#include "renderprefs.h"
#include "sound.h"
#include "backgammon.h"
#include "path.h"

int stopNextTime;
int slide_move;
double animStartTime = 0;

extern double get_time();
extern int convert_point( int i, int player );
extern void setupFlag(BoardData* bd);
extern void setupDicePaths(BoardData* bd, Path dicePaths[2]);
extern void waveFlag(BoardData* bd, float wag);

/* Test function to show normal direction */
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

void CheckOpenglError()
{
	GLenum glErr = glGetError();
	if (glErr != GL_NO_ERROR)
		g_print("OpenGL Error: %s\n", gluErrorString(glErr));
}

void SetupLight3d(BoardData *bd, renderdata* prd)
{
	float lp[4];
	float al[4], dl[4], sl[4];

	copyPoint(lp, prd->lightPos);
	lp[3] = (float)(prd->lightType == LT_POSITIONAL);

	/* If directioinal vector is from origin */
	if (lp[3] == 0)
	{
		lp[0] -= getBoardWidth() / 2.0f;
		lp[1] -= getBoardHeight() / 2.0f;
	}
	glLightfv(GL_LIGHT0, GL_POSITION, lp);

	al[0] = al[1] = al[2] = prd->lightLevels[0] / 100.0f;
	al[3] = 1;
	glLightfv(GL_LIGHT0, GL_AMBIENT, al);

	dl[0] = dl[1] = dl[2] = prd->lightLevels[1] / 100.0f;
	dl[3] = 1;
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dl);

	sl[0] = sl[1] = sl[2] = prd->lightLevels[2] / 100.0f;
	sl[3] = 1;
	glLightfv(GL_LIGHT0, GL_SPECULAR, sl);

	/* Shadow light position */
	memcpy(bd->shadow_light_position, lp, sizeof(float[4]));
}

#if !GL_VERSION_1_2
/* Determine if a particular extension is supported */
int extensionSupported(const char *extension)
{
  static const GLubyte *extensions = NULL;
  const GLubyte *start;
  GLubyte *where, *terminator;

  where = (GLubyte *) strchr(extension, ' ');
  if (where || *extension == '\0')
    return 0;

  if (!extensions)
    extensions = glGetString(GL_EXTENSIONS);

  start = extensions;
  for (;;) {
    where = (GLubyte *) strstr((const char *) start, extension);
    if (!where)
      break;
    terminator = where + strlen(extension);
    if (where == start || *(where - 1) == ' ') {
      if (*terminator == ' ' || *terminator == '\0') {
        return 1;
      }
    }
    start = terminator;
  }
  return 0;
}

#ifndef GL_EXT_separate_specular_color
#define GL_EXT_separate_specular_color      1

#define GL_LIGHT_MODEL_COLOR_CONTROL_EXT    0x81F8
#define GL_SINGLE_COLOR_EXT                 0x81F9
#define GL_SEPARATE_SPECULAR_COLOR_EXT      0x81FA
#endif
#endif

void InitGL(BoardData *bd)
{
	float gal[4];
	/* Turn on light 0 */
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);

	/* No global ambient light */
	gal[0] = gal[1] = gal[2] = gal[3] = 0;
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, gal);

	/* Local specular viewpoint */
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	/* Default to <= depth testing */
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);

	/* Back face culling */
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	/* Nice hints */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	/* Default blend function for alpha-blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Generate normal co-ords for nurbs */
	glEnable(GL_AUTO_NORMAL);

	if (bd)
	{
		/* Setup some 3d things */
		BuildFont(bd);
		setupFlag(bd);
		shadowInit(bd);
	}

#if GL_VERSION_1_2
	glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#else
	if (extensionSupported("GL_EXT_separate_specular_color"))
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT, GL_SEPARATE_SPECULAR_COLOR_EXT);
#endif
}

void setMaterial(Material* pMat)
{
	glMaterialfv(GL_FRONT, GL_AMBIENT, pMat->ambientColour);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, pMat->diffuseColour);
	glMaterialfv(GL_FRONT, GL_SPECULAR, pMat->specularColour);
	glMateriali(GL_FRONT, GL_SHININESS, pMat->shine);

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

float Dist2d(float a, float b)
{	/* Find 3rd side size */
	float sqdD = a * a - b * b;
	if (sqdD > 0)
		return (float)sqrt(sqdD);
	else
		return 0;
}

int IsSet(int flags, int bit)
{
	return ((flags & bit) == bit) ? 1 : 0;
}

/* Texture functions */

myList textures;

char* TextureTypeStrs[TT_COUNT] = {"general", "piece", "hinge"};
char* TextureFormatStrs[TF_COUNT] = {"bmp", "png"};

GList *GetTextureList(int type)
{
	int i;
	GList *glist = NULL;
	glist = g_list_append(glist, NO_TEXTURE_STRING);
	for (i = 0; i < ListSize(&textures); i++)
	{
		TextureInfo* text = (TextureInfo*)ListGet(&textures, i);
		if (IsSet(type, text->type))
			glist = g_list_append(glist, text->name);
	}
	return glist;
}

void FindNamedTexture(TextureInfo** textureInfo, char* name)
{
	int i;
	for (i = 0; i < ListSize(&textures); i++)
	{
		TextureInfo* text = (TextureInfo*)ListGet(&textures, i);
		if (!strcasecmp(text->name, name))
		{
			*textureInfo = text;
			return;
		}
	}
	*textureInfo = 0;
	g_print("Texture %s not in texture info file\n", name);
}

void FindTexture(TextureInfo** textureInfo, char* file)
{
	int i;
	for (i = 0; i < ListSize(&textures); i++)
	{
		TextureInfo* text = (TextureInfo*)ListGet(&textures, i);
		if (!strcasecmp(text->file, file))
		{
			*textureInfo = text;
			return;
		}
	}
	*textureInfo = 0;
	g_print("Texture %s not in texture info file\n", file);
}

#define TEXTURE_FILE "textures.txt"
#define TEXTURE_FILE_VERSION 2

void LoadTextureInfo()
{
	FILE* fp;
	char *szFile;
	#define BUF_SIZE 100
	char buf[BUF_SIZE];

	ListInit(&textures, sizeof(TextureInfo));

	if ( ! ( szFile = PathSearch( TEXTURE_FILE, szDataDirectory ) ) ) {
		g_print( "PathSearch failed!\n" );
		return;
	}

	fp = fopen(szFile, "r");
	free(szFile);
	if (!fp)
	{
		g_print("Error: Texture file (%s) not found\n", TEXTURE_FILE);
		return;
	}

	if (!fgets(buf, BUF_SIZE, fp) || atoi(buf) != TEXTURE_FILE_VERSION)
	{
		g_print("Error: Texture file (%s) out of date\n", TEXTURE_FILE);
		return;
	}

	do
	{
		int err, found, i, len, val;
		TextureInfo text;

		err = 0;

		/* filename */
		if (!fgets(buf, BUF_SIZE, fp))
			return;	/* finished */
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
		{
			len--;
			buf[len] = '\0';
		}
		if (len > FILENAME_SIZE)
		{
			g_print("Texture filename %s too big, maximum length %d.  Entry ignored.\n", buf, FILENAME_SIZE);
			err = 1;
		}
		else
			strcpy(text.file, buf);

		/* format */
		if (!fgets(buf, BUF_SIZE, fp))
		{
			g_print("Error in texture file info.\n");
			return;
		}
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
		{
			len--;
			buf[len] = '\0';
		}
		found = -1;
		for (i = 0; i < TF_COUNT; i++)
		{
			if (!strcasecmp(buf, TextureFormatStrs[i]))
			{
				found = i;
				break;
			}
		}
		if (found == -1)
		{
			g_print("Unknown texture format %s.  Entry ignored.\n", buf);
			err = 1;
		}
		else
			text.format = (TextureFormat)i;

		/* name */
		if (!fgets(buf, BUF_SIZE, fp))
		{
			g_print("Error in texture file info.\n");
			return;
		}
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
		{
			len--;
			buf[len] = '\0';
		}
		if (len > NAME_SIZE)
		{
			g_print("Texture name %s too big, maximum length %d.  Entry ignored.\n", buf, NAME_SIZE);
			err = 1;
		}
		else
			strcpy(text.name, buf);

		/* type */
		if (!fgets(buf, BUF_SIZE, fp))
		{
			g_print("Error in texture file info.\n");
			return;
		}
		len = strlen(buf);
		if (len > 0 && buf[len - 1] == '\n')
		{
			len--;
			buf[len] = '\0';
		}
		found = -1;
		val = 2;
		for (i = 0; i < TT_COUNT; i++)
		{
			if (!strcasecmp(buf, TextureTypeStrs[i]))
			{
				found = i;
				break;
			}
			val *= 2;
		}
		if (found == -1)
		{
			g_print("Unknown texture type %s.  Entry ignored.\n", buf);
			err = 1;
		}
		else
			text.type = (TextureType)val;

		if (!err)
		{	/* Add texture type */
			ListAdd(&textures, &text);
		}
	} while (!feof(fp));
}

void DeleteTexture(Texture* texture)
{
	if (texture->texID)
		glDeleteTextures(1, &texture->texID);

	texture->texID = 0;
}

int LoadTexture(Texture* texture, const char* filename, TextureFormat format)
{
	unsigned char* bits = 0;
	int n;
	char *szFile = PathSearch( filename, szDataDirectory );
	FILE *fp;

	if ( ! szFile )
		return 0;

	if ( ! ( fp = fopen( szFile, "rb") ) ) {
		g_print("Failed to open texture: %s\n", szFile );
		free( szFile );
		return 0;	/* failed to load file */
	}

	free( szFile );

	switch(format)
	{
	case TF_BMP:
		bits = LoadDIBTexture(fp, &texture->width, &texture->height);
		break;
	case TF_PNG:
#if HAVE_LIBPNG
		bits = LoadPNGTexture(fp, &texture->width, &texture->height);
#endif
		break;
	default:
		g_print("Unknown texture type for texture %s\n", filename);
		return 0;	/* failed to load file */
	}

	fclose(fp);

	if (!bits)
	{
		g_print("Failed to load texture: %s\n", filename);
		return 0;	/* failed to load file */
	}
	if (texture->width != texture->height)
	{
		g_print("Failed to load texture %s. width (%d) different to height (%d)\n",
			filename, texture->width, texture->height);
		return 0;	/* failed to load file */
	}
	/* Check size is a power of 2 */
	frexp(texture->width, &n);
	if (texture->width != pow(2, n - 1))
	{
		g_print("Failed to load texture %s, size (%d) isn't a power of 2\n",
			filename, texture->width);
		return 0;	/* failed to load file */
	}

	/* Create texture */
	glGenTextures(1, &texture->texID);
	glBindTexture(GL_TEXTURE_2D, texture->texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* Read bits */
	glTexImage2D(GL_TEXTURE_2D, 0, 3, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);
	free(bits);	/* Release loaded image data */

	return 1;
}

void GetTexture(BoardData* bd, Material* pMat)
{
	if (pMat->textureInfo)
	{
		char buf[100];
		strcpy(buf, TEXTURE_PATH);
		strcat(buf, pMat->textureInfo->file);
		SetTexture(bd, pMat, buf, pMat->textureInfo->format);
	}
	else
		pMat->pTexture = 0;
}

void GetTextures(BoardData* bd)
{
	GetTexture(bd, &bd->chequerMat[0]);
	GetTexture(bd, &bd->chequerMat[1]);
	GetTexture(bd, &bd->baseMat);
	GetTexture(bd, &bd->pointMat[0]);
	GetTexture(bd, &bd->pointMat[1]);
	GetTexture(bd, &bd->boxMat);
	GetTexture(bd, &bd->hingeMat);
	GetTexture(bd, &bd->backGroundMat);
}

void Set3dSettings(BoardData* bd, const renderdata *prd)
{
	bd->pieceType = prd->pieceType;
	bd->pieceTextureType = prd->pieceTextureType;
	bd->showHinges = prd->fHinges;
	bd->showMoveIndicator = prd->showMoveIndicator;
	bd->showShadows = prd->showShadows;
	bd->roundedEdges = prd->roundedEdges;
	bd->bgInTrays = prd->bgInTrays;
	bd->shadowDarkness = prd->shadowDarkness;
	SetShadowDimness3d(bd);
	bd->curveAccuracy = prd->curveAccuracy;
	bd->testSkewFactor = prd->testSkewFactor;
	bd->boardAngle = prd->boardAngle;
	bd->diceSize = prd->diceSize * base_unit;
	bd->planView = prd->planView;

	memcpy(bd->chequerMat, prd->rdChequerMat, sizeof(Material[2]));
	memcpy(&bd->diceMat[0], prd->afDieColour[0] ? &prd->rdChequerMat[0] : &prd->rdDiceMat[0], sizeof(Material));
	memcpy(&bd->diceMat[1], prd->afDieColour[1] ? &prd->rdChequerMat[1] : &prd->rdDiceMat[1], sizeof(Material));
	bd->diceMat[0].textureInfo = bd->diceMat[1].textureInfo = 0;
	bd->diceMat[0].pTexture = bd->diceMat[1].pTexture = 0;
	/* Set alpha values of dice (if opaque) - .5 value used for anti-aliasing dice */
	if (!bd->diceMat[0].alphaBlend)
		bd->diceMat[0].ambientColour[3] = bd->diceMat[0].diffuseColour[3] = bd->diceMat[0].specularColour[3] = 0.5f;
	if (!bd->diceMat[1].alphaBlend)
		bd->diceMat[1].ambientColour[3] = bd->diceMat[1].diffuseColour[3] = bd->diceMat[1].specularColour[3] = 0.5f;

	memcpy(bd->diceDotMat, prd->rdDiceDotMat, sizeof(Material[2]));

	memcpy(&bd->cubeMat, &prd->rdCubeMat, sizeof(Material));
	memcpy(&bd->cubeNumberMat, &prd->rdCubeNumberMat, sizeof(Material));

	memcpy(&bd->baseMat, &prd->rdBaseMat, sizeof(Material));
	memcpy(bd->pointMat, prd->rdPointMat, sizeof(Material[2]));

	memcpy(&bd->boxMat, &prd->rdBoxMat, sizeof(Material));
	memcpy(&bd->hingeMat, &prd->rdHingeMat, sizeof(Material));
	memcpy(&bd->pointNumberMat, &prd->rdPointNumberMat, sizeof(Material));
	memcpy(&bd->backGroundMat, &prd->rdBackGroundMat, sizeof(Material));
}

void CopySettings3d(BoardData* from, BoardData* to)
{	/* Just copy the whole thing */
	memcpy(to, from, sizeof(BoardData));
	/* Shallow copy, so reset allocated points */
	to->boardPoints = 0;
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

	/* Simulate rotations to determine actually dice position */
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

void cylinder(float radius, float height, int accuracy, Texture* texture)
{
	int i;
	float angle = 0;
	float circum = PI * radius * 2 / (accuracy + 1);
	float step = (2 * PI) / accuracy;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < accuracy + 1; i++)
	{
		glNormal3f((float)sin(angle), (float)cos(angle), 0);
		if (texture)
			glTexCoord2f(circum * i / (radius * 2), 0);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, 0);

		if (texture)
			glTexCoord2f(circum * i / (radius * 2), height / (radius * 2));
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);

		angle += step;
	}
	glEnd();
}

void circleOutlineOutward(float radius, float height, int accuracy)
{	/* Draw an ouline of a disc in current z plane with outfacing normals */
	int i;
	float angle, step;

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_LINE_STRIP);
	for (i = 0; i <= accuracy; i++)
	{
		glNormal3f((float)sin(angle), (float)cos(angle), 0);
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);
		angle -= step;
	}
	glEnd();
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

void circleTex(float radius, float height, int accuracy, Texture* texture)
{	/* Draw a disc in current z plane with a texture */
	int i;
	float angle, step;

	if (!texture)
	{
		circle(radius, height, accuracy);
		return;
	}

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(.5f, .5f);
	glVertex3f(0, 0, height);
	for (i = 0; i <= accuracy; i++)
	{
		glTexCoord2f((float)(sin(angle) * radius + radius) / (radius * 2), ((float)cos(angle) * radius + radius) / (radius * 2));
		glVertex3f((float)sin(angle) * radius, (float)cos(angle) * radius, height);
		angle -= step;
	}
	glEnd();
}

void circleRevTex(float radius, float height, int accuracy, Texture* texture)
{	/* Draw a disc with reverse winding in current z plane with a texture */
	int i;
	float angle, step;

	if (!texture)
	{
		circleRev(radius, height, accuracy);
		return;
	}

	step = (2 * PI) / accuracy;
	angle = 0;
	glNormal3f(0, 0, 1);
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(.5f, .5f);
	glVertex3f(0, 0, height);
	for (i = 0; i <= accuracy; i++)
	{
		glTexCoord2f((float)(sin(angle) * radius + radius) / (radius * 2), ((float)cos(angle) * radius + radius) / (radius * 2));
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
		glNormal3f(0, 0, normZ);
		if (type & BOX_SPLITTOP)
		{
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
		if (type & BOX_SPLITWIDTH)
		{
			glVertex3f(-1, -1, 1);
			glVertex3f(0, -1, 1);
			glVertex3f(0, 1, 1);
			glVertex3f(-1, 1, 1);

			glVertex3f(0, -1, 1);
			glVertex3f(1, -1, 1);
			glVertex3f(1, 1, 1);
			glVertex3f(0, 1, 1);
		}
		else
		{
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

void drawQuarterRect(float x, float y, float z, float w, float h, Texture* texture)
{	/* Draw a rectangle in 4 quarters */
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
			glTexCoord2f(repX / 2.0f, 0); glVertex3f(0, -1, 0);
			glTexCoord2f(repX / 2.0f, repY / 2.0f); glVertex3f(0, 0, 0);
			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 0);

			glTexCoord2f(0, repY / 2.0f); glVertex3f(-1, 0, 0);
			glTexCoord2f(repX / 2.0f, repY / 2.0f); glVertex3f(0, 0, 0);
			glTexCoord2f(repX / 2.0f, repY); glVertex3f(0, 1, 0);
			glTexCoord2f(0, repY); glVertex3f(-1, 1, 0);

			glTexCoord2f(repX / 2.0f, 0); glVertex3f(0, -1, 0);
			glTexCoord2f(repX, 0); glVertex3f(1, -1, 0);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 0);
			glTexCoord2f(repX / 2.0f, repY / 2.0f); glVertex3f(0, 0, 0);

			glTexCoord2f(repX / 2.0f, repY / 2.0f); glVertex3f(0, 0, 0);
			glTexCoord2f(repX, repY / 2.0f); glVertex3f(1, 0, 0);
			glTexCoord2f(repX, repY); glVertex3f(1, 1, 0);
			glTexCoord2f(repX / 2.0f, repY); glVertex3f(0, 1, 0);
		glEnd();
	}
	else
	{
		glBegin(GL_QUADS);
			glVertex3f(-1, -1, 0);
			glVertex3f(0, -1, 0);
			glVertex3f(0, 0, 0);
			glVertex3f(-1, 0, 0);

			glVertex3f(-1, 0, 0);
			glVertex3f(0, 0, 0);
			glVertex3f(0, 1, 0);
			glVertex3f(-1, 1, 0);

			glVertex3f(0, -1, 0);
			glVertex3f(1, -1, 0);
			glVertex3f(1, 0, 0);
			glVertex3f(0, 0, 0);

			glVertex3f(0, 0, 0);
			glVertex3f(1, 0, 0);
			glVertex3f(1, 1, 0);
			glVertex3f(0, 1, 0);
		glEnd();
	}

	glPopMatrix();
}

void QuarterCylinder(float radius, float len, int accuracy, Texture* texture)
{
	int i;
	float angle;
	float d;
	float sar, car;
	float dInc = 0;

	/* texture unit value */
	float tuv;
	if (texture)
	{
		float st = (float)sin((2 * PI) / accuracy) * radius;
		float ct = ((float)cos((2 * PI) / accuracy) - 1) * radius;
		dInc = (float)sqrt(st * st + ct * ct);
		tuv = (TEXTURE_SCALE) / texture->width;
	}
	else
		tuv = 0;

	d = 0;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < accuracy / 4 + 1; i++)
	{
		angle = (i * 2 * PI) / accuracy;
		glNormal3f((float)sin(angle), 0, (float)cos(angle));

		sar = (float)sin(angle) * radius;
		car = (float)cos(angle) * radius;

		if (tuv)
			glTexCoord2f(len * tuv, d * tuv);
		glVertex3f(sar, len, car);

		if (tuv)
		{
			glTexCoord2f(0, d * tuv);
			d -= dInc;
		}
		glVertex3f(sar, 0, car);
	}
	glEnd();
}

void QuarterCylinderSplayedRev(float radius, float len, int accuracy, Texture* texture)
{
	int i;
	float angle;
	float d;
	float sar, car;
	float dInc = 0;

	/* texture unit value */
	float tuv;
	if (texture)
	{
		float st = (float)sin((2 * PI) / accuracy) * radius;
		float ct = ((float)cos((2 * PI) / accuracy) - 1) * radius;
		dInc = (float)sqrt(st * st + ct * ct);
		tuv = (TEXTURE_SCALE) / texture->width;
	}
	else
		tuv = 0;

	d = 0;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < accuracy / 4 + 1; i++)
	{
		angle = (i * 2 * PI) / accuracy;
		glNormal3f((float)sin(angle), 0, (float)cos(angle));

		sar = (float)sin(angle) * radius;
		car = (float)cos(angle) * radius;

		if (tuv)
			glTexCoord2f((len + car) * tuv, d * tuv);
		glVertex3f(sar, len + car, car);

		if (tuv)
		{
			glTexCoord2f(-car * tuv, d * tuv);
			d -= dInc;
		}
		glVertex3f(sar, -car, car);
	}
	glEnd();
}

void QuarterCylinderSplayed(float radius, float len, int accuracy, Texture* texture)
{
	int i;
	float angle;
	float d;
	float sar, car;
	float dInc = 0;

	/* texture unit value */
	float tuv;
	if (texture)
	{
		float st = (float)sin((2 * PI) / accuracy) * radius;
		float ct = ((float)cos((2 * PI) / accuracy) - 1) * radius;
		dInc = (float)sqrt(st * st + ct * ct);
		tuv = (TEXTURE_SCALE) / texture->width;
	}
	else
		tuv = 0;

	d = 0;
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i < accuracy / 4 + 1; i++)
	{
		angle = (i * 2 * PI) / accuracy;
		glNormal3f((float)sin(angle), 0, (float)cos(angle));

		sar = (float)sin(angle) * radius;
		car = (float)cos(angle) * radius;

		if (tuv)
			glTexCoord2f((len - car) * tuv, d * tuv);
		glVertex3f(sar, len - car, car);

		if (tuv)
		{
			glTexCoord2f(car * tuv, d * tuv);
			d -= dInc;
		}
		glVertex3f(sar, car, car);
	}
	glEnd();
}

void drawCornerEigth(float ***boardPoints, float radius, int accuracy)
{
	int i, j, ns;

	for (i = 0; i < accuracy / 4; i++)
	{
		ns = (accuracy / 4) - i - 1;
		glBegin(GL_TRIANGLE_STRIP);
			glNormal3f(boardPoints[i][ns + 1][0] / radius, boardPoints[i][ns + 1][1] / radius, boardPoints[i][ns + 1][2] / radius);
			glVertex3f(boardPoints[i][ns + 1][0], boardPoints[i][ns + 1][1], boardPoints[i][ns + 1][2]);
			for (j = ns; j >= 0; j--)
			{
				glNormal3f(boardPoints[i + 1][j][0] / radius, boardPoints[i + 1][j][1] / radius, boardPoints[i + 1][j][2] / radius);
				glVertex3f(boardPoints[i + 1][j][0], boardPoints[i + 1][j][1], boardPoints[i + 1][j][2]);
				glNormal3f(boardPoints[i][j][0] / radius, boardPoints[i][j][1] / radius, boardPoints[i][j][2] / radius);
				glVertex3f(boardPoints[i][j][0], boardPoints[i][j][1], boardPoints[i][j][2]);
			}
		glEnd();
	}
}

void calculateEigthPoints(float ****boardPoints, float radius, int accuracy)
{
	int i, j, ns;

	float lat_angle;
	float lat_step;
	float latitude;
	float new_radius;
	float angle;
	float step;
	int corner_steps = (accuracy / 4) + 1;
	*boardPoints = Alloc3d(corner_steps, corner_steps, 3);

	lat_angle = 0;
	lat_step = (2 * PI) / accuracy;

	/* Calculate corner 1/8th sphere points */
	for (i = 0; i < (accuracy / 4) + 1; i++)
	{
		latitude = (float)sin(lat_angle) * radius;
		new_radius = Dist2d(radius, latitude);

		angle = 0;
		ns = (accuracy / 4) - i;
		step = (2 * PI) / (ns * 4);

		for (j = 0; j <= ns; j++)
		{
			(*boardPoints)[i][j][0] = (float)sin(angle) * new_radius;
			(*boardPoints)[i][j][1] = latitude;
			(*boardPoints)[i][j][2] = (float)cos(angle) * new_radius;

			angle += step;
		}
		lat_angle += lat_step;
	}
}

void freeEigthPoints(float ***boardPoints, int accuracy)
{
	int corner_steps = (accuracy / 4) + 1;
	Free3d(boardPoints, corner_steps, corner_steps);
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
{	/* Make sure rotation is correct in new position */
	bd->pieceRotation[src][abs(bd->points[src])] = bd->pieceRotation[dest][abs(bd->points[dest] - 1)];
	bd->pieceRotation[dest][abs(bd->points[dest]) - 1] = bd->movingPieceRotation;
}

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

	setupPath(bd, &bd->piecePath, &bd->rotateMovingPiece, fClockwise, target, abs(bd->points[target]) + 1, dest, destDepth);
	copyPoint(bd->movingPos, bd->piecePath.pts[0]);

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
		StopIdle3d(bd);
		gtk_main_quit();
		return 1;
	}

	if (bd->moving)
	{
		float *pRotate = 0;
		if (bd->rotateMovingPiece != -1 && bd->piecePath.state == 2)
			pRotate = &bd->rotateMovingPiece;

		if (!movePath(&bd->piecePath, animateDistance, pRotate, bd->movingPos))
		{
			int points[2][25];
		    int moveStart = convert_point(animate_move_list[slide_move], animate_player);
			int moveDest = convert_point(animate_move_list[slide_move + 1], animate_player);

			if ((abs(bd->points[moveDest]) == 1) && (bd->turn != SGN(bd->points[moveDest])))
			{	/* huff */
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
		if (!finishedPath(&bd->dicePaths[0]))
			updateDicePos(&bd->dicePaths[0], &bd->diceRotation[0], animateDistance / 2.0f, bd->diceMovingPos[0]);
		if (!finishedPath(&bd->dicePaths[1]))
			updateDicePos(&bd->dicePaths[1], &bd->diceRotation[1], animateDistance / 2.0f, bd->diceMovingPos[1]);

		if (finishedPath(&bd->dicePaths[0]) && finishedPath(&bd->dicePaths[1]))
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
		setIdleFunc(bd, idleAnimate);

		setupDicePaths(bd, bd->dicePaths);
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
	setIdleFunc(bd, idleAnimate);
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

	if (rdAppearance.animateFlag && bd->resigned && ms.gs == GAME_PLAYING && bd->playing &&
		(ap[bd->turn == 1 ? 0 : 1].pt == PLAYER_HUMAN))		/* not for computer turn */
	{
		animStartTime = get_time();
		setIdleFunc(bd, idleWaveFlag);
	}
	else
		StopIdle3d(bd);

	waveFlag(bd, 0);
	updateFlagOccPos(bd);
}

int idleCloseBoard(BoardData* bd)
{
	float elapsedTime = (float)(get_time() - animStartTime);
	if (bd->State == BOARD_CLOSED)
	{	/* finished */
		StopIdle3d(bd);
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

/* Performance test data */
double testStartTime;
int numFrames;
#define TEST_TIME 3000.0f

int idleTestPerformance(BoardData* bd)
{
	float elapsedTime = (float)(get_time() - testStartTime);
	if (elapsedTime > TEST_TIME)
	{
		StopIdle3d(bd);
		gtk_main_quit();
		return 0;
	}
	numFrames++;
	return 1;
}

int TestPerformance3d(BoardData* bd)
{
	float elapsedTime;

	setIdleFunc(bd, idleTestPerformance);
	testStartTime = get_time();
	numFrames = 0;
	gtk_main();
	elapsedTime = (float)(get_time() - testStartTime);

	return (int)(numFrames / (elapsedTime / 1000.0f));
}

void InitialPos(BoardData *bd)
{	/* Set up initial board position */
	int ip[] = {0,-2,0,0,0,0,5,0,3,0,0,0,-5,5,0,0,0,-3,0,-5,0,0,0,0,2,0,0,0};
	memcpy(bd->points, ip, sizeof(bd->points));
	updatePieceOccPos(bd);
}

void EmptyPos(BoardData *bd)
{	/* All checkers home */
	int ip[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,15,-15};
	memcpy(bd->points, ip, sizeof(bd->points));
	updatePieceOccPos(bd);
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

	bd->showShadows = 0;
	bd->showMoveIndicator = 0;
	rdAppearance.fLabels = 0;
	bd->diceShown = DICE_NOT_SHOWN;
	bd->State = BOARD_CLOSING;

	/* Random logo */
	if (rand() % 2)
		SetTexture(bd, &bd->logoMat, TEXTURE_PATH"logo.bmp", TF_BMP);
	else
		SetTexture(bd, &bd->logoMat, TEXTURE_PATH"logo2.bmp", TF_BMP);

	animStartTime = get_time();
	bd->perOpen = 0;
	setIdleFunc(bd, idleCloseBoard);
	/* Push matrix as idleCloseBoard assumes last matrix is on stack */
	glPushMatrix();
	
	gtk_main();
}

int MouseMove3d(BoardData *bd, int x, int y)
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

void SetupViewingVolume3d(BoardData *bd, renderdata* prd)
{
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	SetupPerspVolume(bd, viewport);

	SetupLight3d(bd, prd);
	calculateBackgroundSize(bd, viewport);
}

void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a)
{
	pMat->ambientColour[0] = r;
	pMat->ambientColour[1] = g;
	pMat->ambientColour[2] = b;
	pMat->ambientColour[3] = a;

	pMat->diffuseColour [0] = dr;
	pMat->diffuseColour [1] = dg;
	pMat->diffuseColour [2] = db;
	pMat->diffuseColour [3] = a;

	pMat->specularColour[0] = sr;
	pMat->specularColour[1] = sg;
	pMat->specularColour[2] = sb;
	pMat->specularColour[3] = a;
	pMat->shine = shin;

	pMat->alphaBlend = (a != 1) && (a != 0);

	pMat->pTexture = 0;
}

void SetupSimpleMatAlpha(Material* pMat, float r, float g, float b, float a)
{
	SetupMat(pMat, r, g, b, r, g, b, 0, 0, 0, 0, a);
}

void SetupSimpleMat(Material* pMat, float r, float g, float b)
{
	SetupMat(pMat, r, g, b, r, g, b, 0, 0, 0, 0, 0);
}

void SetTexture(BoardData* bd, Material* pMat, const char* filename, TextureFormat format)
{
	/* See if already loaded */
	int i;
	const char* nameStart = filename;
	/* Find start of name in filename */
	char* newStart = 0;

	do
	{
		if (!(newStart = strchr(nameStart, '\\')))
			newStart = strchr(nameStart, '/');

		if (newStart)
			nameStart = newStart + 1;
	} while(newStart);

	/* Search for name in cached list */
	for (i = 0; i < bd->numTextures; i++)
	{
		if (!strcasecmp(nameStart, bd->textureName[i]))
		{	/* found */
			pMat->pTexture = &bd->textureList[i];
			return;
		}
	}

	/* Not found - Load new texture */
	if (bd->numTextures == MAX_TEXTURES - 1)
	{
		g_print("Error: Too many textures loaded...\n");
		return;
	}

	if (LoadTexture(&bd->textureList[bd->numTextures], filename, format))
	{
		/* Remeber name */
		bd->textureName[bd->numTextures] = malloc(strlen(nameStart) + 1);
		strcpy(bd->textureName[bd->numTextures], nameStart);

		pMat->pTexture = &bd->textureList[bd->numTextures];
		bd->numTextures++;
	}
}

/* Not currently used
void RemoveTexture(Material* pMat)
{
	if (pMat->pTexture)
	{
		int i = 0;
		while (&bd->textureList[i] != pMat->pTexture)
			i++;

		DeleteTexture(&bd->textureList[i]);
		free(bd->textureName[i]);

		while (i != bd->numTextures - 1)
		{
			bd->textureList[i] = bd->textureList[i + 1];
			bd->textureName[i] = bd->textureName[i + 1];
			i++;
		}
		bd->numTextures--;
		pMat->pTexture = 0;
	}
}
*/

void ClearTextures(BoardData* bd, int glValid)
{
	int i;

	for (i = 0; i < bd->numTextures; i++)
	{
		if (glValid)
			DeleteTexture(&bd->textureList[i]);
		free(bd->textureName[i]);
	}
	bd->numTextures = 0;
}

void InitBoard3d(BoardData *bd)
{	/* Initilize 3d parts of boarddata */
	int i, j;
	/* Assign random rotation to each board position */
	for (i = 0; i < 28; i++)
		for (j = 0; j < 15; j++)
			bd->pieceRotation[i][j] = rand() % 360;

	bd->State = BOARD_OPEN;
	bd->moving = 0;
	bd->shakingDice = 0;
	bd->drag_point = -1;
	bd->DragTargetHelp = 0;
	setDicePos(bd);

	SetupSimpleMat(&bd->gap, 0, 0, 0);
	SetupSimpleMat(&bd->logoMat, 1, 1, 1);
	SetupMat(&bd->flagMat, 1, 1, 1, 1, 1, 1, 1, 1, 1, 50, 0);
	SetupMat(&bd->flagNumberMat, 0, 0, .4f, 0, 0, .4f, 1, 1, 1, 100, 0);

	bd->diceList = bd->DCList = bd->pieceList = 0;
	bd->qobjTex = bd->qobj = 0;
	bd->flagNurb = 0;

	bd->numTextures = 0;

	bd->boardPoints = NULL;
}

void InitBoardPreview(BoardData *bd)
{
	InitBoard3d(bd);
	/* Show set position */
	InitialPos(bd);
	bd->cube_use = 1;
	bd->crawford_game = 0;
	bd->doubled = bd->cube_owner = bd->cube = 0;
	bd->resigned = 0;
	bd->diceShown = DICE_ON_BOARD;
	bd->diceRoll[0] = 4;
	bd->diceRoll[1] = 3;
	bd->turn = 1;
}
