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

#include "config.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#else
#include <io.h>
#endif
#include "glincl.h"
#include "inc3d.h"
#include "shadow.h"
#include "renderprefs.h"
#include "sound.h"
#include "backgammon.h"
#include "path.h"

int stopNextTime;
int slide_move;
double animStartTime = 0;

void CreateDotTexture(BoardData *bd);

extern double get_time();
extern int convert_point( int i, int player );
extern void setupFlag(BoardData* bd);
extern void setupDicePaths(BoardData* bd, Path dicePaths[2]);
extern void waveFlag(BoardData* bd, float wag);
extern float getDiceSize(BoardData* bd);

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
		if (!BuildFont3d(bd))
			g_print("Error creating fonts\n");

		setupFlag(bd);
		shadowInit(bd);
#if GL_VERSION_1_2
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
#else
		if (extensionSupported("GL_EXT_separate_specular_color"))
			glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL_EXT, GL_SEPARATE_SPECULAR_COLOR_EXT);
#endif
		CreateDotTexture(bd);
	}
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

list textures;

char* TextureTypeStrs[TT_COUNT] = {"general", "piece", "hinge"};
char* TextureFormatStrs[TF_COUNT] = {"bmp", "png"};

GList *GetTextureList(int type)
{
	GList *glist = NULL;
	list *pl;
	glist = g_list_append(glist, NO_TEXTURE_STRING);

	for (pl = textures.plNext; pl->p; pl = pl->plNext)
	{
		TextureInfo* text = (TextureInfo*)pl->p;
		if (IsSet(type, text->type))
			glist = g_list_append(glist, text->name);
	}
	return glist;
}

void FindNamedTexture(TextureInfo** textureInfo, char* name)
{
	list *pl;
	for (pl = textures.plNext; pl->p; pl = pl->plNext)
	{
		TextureInfo* text = (TextureInfo*)pl->p;
		if (!strcasecmp(text->name, name))
		{
			*textureInfo = text;
			return;
		}
	}
	*textureInfo = 0;
	/* Only warn user if textures.txt file has been loaded */
	if (!ListEmpty(&textures))
		g_print("Texture %s not in texture info file\n", name);
}

void FindTexture(TextureInfo** textureInfo, char* file)
{
	list *pl;
	for (pl = textures.plNext; pl->p; pl = pl->plNext)
	{
		TextureInfo* text = (TextureInfo*)pl->p;
		if (!strcasecmp(text->file, file))
		{
			*textureInfo = text;
			return;
		}
	}
	{	/* Not in texture list, see if old texture on disc */
		char *szFile = PathSearch( file, szDataDirectory );
		if (szFile && !access(szFile, R_OK))
		{
			int len = strlen(file);
			/* Add entry for unknown texture */
			TextureInfo text;
			strcpy(text.file, file);
			if (len > 4 && !strcasecmp(&file[len - 4], ".png"))
				text.format = TF_PNG;
			else
				text.format = TF_BMP;
			strcpy(text.name, file);
			text.type = TT_NONE;	/* Don't show in lists */

			*textureInfo = malloc(sizeof(TextureInfo));
			**textureInfo = text;
			ListInsert(&textures, *textureInfo);

			free( szFile );
			return;
		}
	}

	*textureInfo = 0;
	/* Only warn user if in 3d */
	if (GetMainAppearance()->fDisplayType == DT_3D)
		g_print("Texture %s not in texture info file\n", file);
}

#define TEXTURE_FILE "textures.txt"
#define TEXTURE_FILE_VERSION 2

void LoadTextureInfo(int FirstPass)
{	/* May be called several times, no errors shown on first pass */
	FILE* fp;
	char *szFile;
	#define BUF_SIZE 100
	char buf[BUF_SIZE];

	if (FirstPass)
		ListCreate(&textures);
	else if (!ListEmpty(&textures))
		return;	/* Ignore multiple calls after a successful load */

	szFile = PathSearch( TEXTURE_FILE, szDataDirectory );
	fp = fopen(szFile, "r");
	free(szFile);
	if (!fp)
	{
		if (!FirstPass)
			g_print("Error: Texture file (%s) not found\n", TEXTURE_FILE);
		return;
	}

	if (!fgets(buf, BUF_SIZE, fp) || atoi(buf) != TEXTURE_FILE_VERSION)
	{
		if (!FirstPass)
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
			break;	/* finished */

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
			TextureInfo* pNewText = malloc(sizeof(TextureInfo));
			*pNewText = text;
			ListInsert(&textures, pNewText);
		}
	} while (!feof(fp));
}

void DeleteTexture(Texture* texture)
{
	if (texture->texID)
		glDeleteTextures(1, &texture->texID);

	texture->texID = 0;
}

void CreateTexture(int* pID, int width, int height, unsigned char* bits)
{
	/* Create texture */
	glGenTextures(1, pID);
	glBindTexture(GL_TEXTURE_2D, *pID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* Read bits */
	glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, bits);
}

int LoadTexture(Texture* texture, const char* filename, TextureFormat format)
{
	unsigned char* bits = 0;
	int n;
	char *szFile = PathSearch( filename, szDataDirectory );
	FILE *fp;

	if (!szFile)
		return 0;

	if (!(fp = fopen( szFile, "rb"))) {
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
#if HAVE_LIBPNG
	case TF_PNG:
		bits = LoadPNGTexture(fp, &texture->width, &texture->height);
		break;
#endif
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

	CreateTexture(&texture->texID, texture->width, texture->height, bits);

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
	GetTexture(bd, &bd->rd->ChequerMat[0]);
	GetTexture(bd, &bd->rd->ChequerMat[1]);
	GetTexture(bd, &bd->rd->BaseMat);
	GetTexture(bd, &bd->rd->PointMat[0]);
	GetTexture(bd, &bd->rd->PointMat[1]);
	GetTexture(bd, &bd->rd->BoxMat);
	GetTexture(bd, &bd->rd->HingeMat);
	GetTexture(bd, &bd->rd->BackGroundMat);
}

#define DOT_SIZE 32
#define MAX_DIST ((DOT_SIZE / 2) * (DOT_SIZE / 2))
#define MIN_DIST ((DOT_SIZE / 2) * .70f * (DOT_SIZE / 2) * .70f)

void CreateDotTexture(BoardData *bd)
{
	int i, j;
	unsigned char* data = malloc(sizeof(*data) * DOT_SIZE * DOT_SIZE * 3);
	unsigned char* pData = data;

	for (i = 0; i < DOT_SIZE; i++)
	{
		for (j = 0; j < DOT_SIZE; j++)
		{
			float xdiff = ((float)i) + .5f - DOT_SIZE / 2;
			float ydiff = ((float)j) + .5f - DOT_SIZE / 2;
			float dist = xdiff * xdiff + ydiff * ydiff;
			float percentage = 1 - ((dist - MIN_DIST) / (MAX_DIST - MIN_DIST));
			unsigned char value;
			if (percentage <= 0)
				value = 0;
			else if (percentage >= 1)
				value = 255;
			else
				value = (unsigned char)(255 * percentage);
			pData[0] = pData[1] = pData[2] = value;
			pData += 3;
		}
	}
	CreateTexture(&bd->dotTexture, DOT_SIZE, DOT_SIZE, data);
	free(data);
}

void Set3dSettings(renderdata *prdnew, const renderdata *prd)
{
	prdnew->pieceType = prd->pieceType;
	prdnew->pieceTextureType = prd->pieceTextureType;
	prdnew->fHinges3d = prd->fHinges3d;
	prdnew->showMoveIndicator = prd->showMoveIndicator;
	prdnew->showShadows = prd->showShadows;
	prdnew->quickDraw = prd->quickDraw;
	prdnew->roundedEdges = prd->roundedEdges;
	prdnew->bgInTrays = prd->bgInTrays;
	prdnew->roundedPoints = prd->roundedPoints;
	prdnew->shadowDarkness = prd->shadowDarkness;
	prdnew->curveAccuracy = prd->curveAccuracy;
	prdnew->skewFactor = prd->skewFactor;
	prdnew->boardAngle = prd->boardAngle;
	prdnew->diceSize = prd->diceSize;
	prdnew->planView = prd->planView;

	memcpy(prdnew->ChequerMat, prd->ChequerMat, sizeof(Material[2]));
	memcpy(&prdnew->DiceMat[0], prd->afDieColour3d[0] ? &prd->ChequerMat[0] : &prd->DiceMat[0], sizeof(Material));
	memcpy(&prdnew->DiceMat[1], prd->afDieColour3d[1] ? &prd->ChequerMat[1] : &prd->DiceMat[1], sizeof(Material));
	prdnew->DiceMat[0].textureInfo = prdnew->DiceMat[1].textureInfo = 0;
	prdnew->DiceMat[0].pTexture = prdnew->DiceMat[1].pTexture = 0;
	memcpy(prdnew->DiceDotMat, prd->DiceDotMat, sizeof(Material[2]));

	memcpy(&prdnew->CubeMat, &prd->CubeMat, sizeof(Material));
	memcpy(&prdnew->CubeNumberMat, &prd->CubeNumberMat, sizeof(Material));

	memcpy(&prdnew->BaseMat, &prd->BaseMat, sizeof(Material));
	memcpy(prdnew->PointMat, prd->PointMat, sizeof(Material[2]));

	memcpy(&prdnew->BoxMat, &prd->BoxMat, sizeof(Material));
	memcpy(&prdnew->HingeMat, &prd->HingeMat, sizeof(Material));
	memcpy(&prdnew->PointNumberMat, &prd->PointNumberMat, sizeof(Material));
	memcpy(&prdnew->BackGroundMat, &prd->BackGroundMat, sizeof(Material));
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

	/* Simulate rotations to determine actual dice position */
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
	float w2 = w / 2.0f, h2 = h / 2.0f, d2 = d / 2.0f;

	glPushMatrix();
	glTranslatef(x + w2, y + h2, z + d2);
	glScalef(w2, h2, d2);

	/* Scale normals */
	normX = w2;
	normY = h2;
	normZ = d2;

	glBegin(GL_QUADS);

	if (texture)
	{
		repX = (w * TEXTURE_SCALE) / texture->width;
		repY = (h * TEXTURE_SCALE) / texture->height;
	 
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
	}
	else
	{	/* no texture co-ords */
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
	}
	glEnd();
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

void drawChequeredRect(float x, float y, float z, float w, float h, int across, int down, Texture* texture)
{	/* Draw a rectangle split into (across x down) chequers */
	int i, j;
	float hh = h / down;
	float ww = w / across;

	glPushMatrix();
	glTranslatef(x, y, z);
	glNormal3f(0, 0, 1);

	if (texture)
	{
		float tuv = TEXTURE_SCALE / texture->width;
		float tw = ww * tuv;
		float th = hh * tuv;
		float ty = 0;

		for (i = 0; i < down; i++)
		{
			float xx = 0, tx = 0;
			glPushMatrix();
			glTranslatef(0, hh * i, 0);
			glBegin(GL_QUAD_STRIP);
			for (j = 0; j <= across; j++)
			{
				glTexCoord2f(tx, ty + th); glVertex2f(xx, hh);
				glTexCoord2f(tx, ty); glVertex2f(xx, 0);
				xx += ww;
				tx += tw;
			}
			ty += th;
			glEnd();
			glPopMatrix();
		}
	}
	else
	{
		for (i = 0; i < down; i++)
		{
			float xx = 0;
			glPushMatrix();
			glTranslatef(0, hh * i, 0);
			glBegin(GL_QUAD_STRIP);
			for (j = 0; j <= across; j++)
			{
				glVertex2f(xx, hh);
				glVertex2f(xx, 0);
				xx += ww;
			}
			glEnd();
			glPopMatrix();
		}
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

void freeEigthPoints(float ****boardPoints, int accuracy)
{
	int corner_steps = (accuracy / 4) + 1;
	if (*boardPoints)
		Free3d(*boardPoints, corner_steps, corner_steps);
	*boardPoints = 0;
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

void getProjectedCoord(float pos[3], float* x, float* y)
{	/* Work out where point (x, y, z) is on the screen */
	GLint viewport[4];
	GLdouble mvmatrix[16], projmatrix[16], xd, yd, zd;

	glGetIntegerv (GL_VIEWPORT, viewport);
	glGetDoublev (GL_MODELVIEW_MATRIX, mvmatrix);
	glGetDoublev (GL_PROJECTION_MATRIX, projmatrix);

	gluProject ((GLdouble)pos[0], (GLdouble)pos[1], (GLdouble)pos[2], mvmatrix, projmatrix, viewport, &xd, &yd, &zd);
	*x = (float)xd;
	*y = (float)yd;
}

int freezeRestrict = 0;
ClipBox cb[MAX_FRAMES], eraseCb, lastCb;
int numRestrictFrames = 0;

float BoxWidth(ClipBox* pCb)
{
	return pCb->xx - pCb->x;
}

float BoxHeight(ClipBox* pCb)
{
	return pCb->yy - pCb->y;
}

float BoxMidWidth(ClipBox* pCb)
{
	return pCb->x + BoxWidth(pCb) / 2;
}

float BoxMidHeight(ClipBox* pCb)
{
	return pCb->y + BoxHeight(pCb) / 2;
}

void CopyBox(ClipBox* pTo, ClipBox* pFrom)
{
	*pTo = *pFrom;
}

void EnlargeTo(ClipBox* pCb, float x, float y)
{
	if (x < pCb->x)
		pCb->x = x;
	if (y < pCb->y)
		pCb->y = y;
	if (x > pCb->xx)
		pCb->xx = x;
	if (y > pCb->yy)
		pCb->yy = y;
}

void EnlargeCurrentToBox(ClipBox* pOtherCb)
{
	EnlargeTo(&cb[numRestrictFrames], pOtherCb->x, pOtherCb->y);
	EnlargeTo(&cb[numRestrictFrames], pOtherCb->xx, pOtherCb->yy);
}

void InitBox(ClipBox* pCb, float x, float y)
{
	pCb->x = pCb->xx = x;
	pCb->y = pCb->yy = y;
}

void RationalizeBox(ClipBox* pCb)
{
	int midX, midY, maxXoff, maxYoff;
	/* Make things a bit bigger to avoid slight drawing errors */
	pCb->x -= .5f;
	pCb->xx += .5f;
	pCb->y -= .5f;
	pCb->yy += .5f;
	midX = (int)BoxMidWidth(pCb);
	midY = (int)BoxMidHeight(pCb);
	maxXoff = MAX(midX - (int)pCb->x, (int)pCb->xx - midX) + 1;
	maxYoff = MAX(midY - (int)pCb->y, (int)pCb->yy - midY) + 1;
	pCb->x = (float)(midX - maxXoff);
	pCb->xx = (float)(midX + maxXoff);
	pCb->y = (float)(midY - maxYoff);
	pCb->yy = (float)(midY + maxYoff);
}

void RestrictiveRedraw()
{
	numRestrictFrames = -1;
}

void RestrictiveDraw(ClipBox* pCb, float pos[3], float width, float height, float depth)
{
	float tpos[3];
	float x, y;

	copyPoint(tpos, pos);
	tpos[0] -= width / 2.0f;
	tpos[1] -= height / 2.0f;

	getProjectedCoord(tpos, &x, &y);
	InitBox(pCb, x, y);

	tpos[0] += width;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[1] += height;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[0] -= width;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[1] -= height;
	tpos[2] += depth;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[0] += width;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[1] += height;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);

	tpos[0] -= width;
	getProjectedCoord(tpos, &x, &y);
	EnlargeTo(pCb, x, y);
}

void RestrictiveDrawFrame(float pos[3], float width, float height, float depth)
{
	if (numRestrictFrames != -1)
	{
		numRestrictFrames++;
		if (numRestrictFrames == MAX_FRAMES)
		{	/* Too many drawing requests - just redraw whole screen */
			RestrictiveRedraw();
			return;
		}
		RestrictiveDraw(&cb[numRestrictFrames], pos, width, height, depth);
	}
}

void RestrictiveRender(BoardData *bd)
{
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();

	while (numRestrictFrames > 0)
	{
		RationalizeBox(&cb[numRestrictFrames]);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPickMatrix(BoxMidWidth(&cb[numRestrictFrames]), BoxMidHeight(&cb[numRestrictFrames]),
			BoxWidth(&cb[numRestrictFrames]), BoxHeight(&cb[numRestrictFrames]), viewport);

		/* Setup projection matrix - using saved values */
		if (bd->rd->planView)
			glOrtho(-bd->horFrustrum, bd->horFrustrum, -bd->vertFrustrum, bd->vertFrustrum, 0, 5);
		else
			glFrustum(-bd->horFrustrum, bd->horFrustrum, -bd->vertFrustrum, bd->vertFrustrum, zNear, zFar);

		glMatrixMode(GL_MODELVIEW);
		glViewport((int)(cb[numRestrictFrames].x), (int)(cb[numRestrictFrames].y),
				   (int)BoxWidth(&cb[numRestrictFrames]), (int)BoxHeight(&cb[numRestrictFrames]));

		drawBoard(bd);

		if (!freezeRestrict)
			numRestrictFrames--;
		else
		{
			if (numRestrictFrames > 1)
				numRestrictFrames--;
			else
			{
				freezeRestrict = 0;
				break;
			}
		}
	}
	/* Restore matrixes */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

int MouseMove3d(BoardData *bd, int x, int y)
{
	if (bd->drag_point >= 0)
	{
		getProjectedPieceDragPos(x, y, bd->dragPos);
		updateMovingPieceOccPos(bd);

		if(bd->rd->quickDraw && numRestrictFrames != -1)
		{
			if (!freezeRestrict)
				CopyBox(&eraseCb, &lastCb);

			RestrictiveDraw(&cb[numRestrictFrames], bd->dragPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
			freezeRestrict++;

			CopyBox(&lastCb, &cb[numRestrictFrames]);
			EnlargeCurrentToBox(&eraseCb);
		}
		return 1;
	}
	else
		return 0;
}

void RestrictiveStartMouseMove(int pos, int depth)
{
	float erasePos[3];
	if (numRestrictFrames != -1)
	{
		getPiecePos(pos, depth, fClockwise, erasePos);
		RestrictiveDrawFrame(erasePos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
		CopyBox(&eraseCb, &cb[numRestrictFrames]);
	}
	freezeRestrict = 1;
}

void RestrictiveEndMouseMove(int pos, int depth)
{
	float newPos[3];
	getPiecePos(pos, depth, fClockwise, newPos);

	if (numRestrictFrames == -1)
		return;

	if (pos == 26 || pos == 27)
	{
		newPos[2] -= PIECE_HOLE / 2.0f;
		RestrictiveDraw(&cb[numRestrictFrames], newPos, PIECE_HOLE, PIECE_HOLE, PIECE_HOLE);
	}
	else
		RestrictiveDraw(&cb[numRestrictFrames], newPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);

	if (freezeRestrict)
		EnlargeCurrentToBox(&eraseCb);
	else
		EnlargeCurrentToBox(&lastCb);

	freezeRestrict = 0;
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

int firstFrame;

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
		float old_pos[3];
		ClipBox temp;
		float *pRotate = 0;
		if (bd->rotateMovingPiece != -1 && bd->piecePath.state == 2)
			pRotate = &bd->rotateMovingPiece;

		copyPoint(old_pos, bd->movingPos);

		if (!movePath(&bd->piecePath, animateDistance, pRotate, bd->movingPos))
		{
			int points[2][25];
		    int moveStart = convert_point(animate_move_list[slide_move], animate_player);
			int moveDest = convert_point(animate_move_list[slide_move + 1], animate_player);

			if ((abs(bd->points[moveDest]) == 1) && (bd->turn != SGN(bd->points[moveDest])))
			{	/* huff */
				int bar;
				if (bd->turn == 1)
					bar = 0;
				else
					bar = 25;
				bd->points[bar] -= bd->turn;
				bd->points[moveDest] = 0;

				if (bd->rd->quickDraw)
					RestrictiveDrawPiece(bar, abs(bd->points[bar]));
			}

			bd->points[moveDest] += bd->turn;

			/* Update pip-count mid move */
			read_board(bd, points);
			update_pipcount(bd, points);

			PlaceMovingPieceRotation(bd, moveDest, moveStart);

			if (bd->rd->quickDraw && numRestrictFrames != -1)
			{
				float new_pos[3];
				getPiecePos(moveDest, abs(bd->points[moveDest]), fClockwise, new_pos);
				if (moveDest == 26 || moveDest == 27)
				{
					new_pos[2] -= PIECE_HOLE / 2.0f;
					RestrictiveDraw(&temp, new_pos, PIECE_HOLE, PIECE_HOLE, PIECE_HOLE);
				}
				else
					RestrictiveDraw(&temp, new_pos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
			}

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
		{
			updateMovingPieceOccPos(bd);
			if (bd->rd->quickDraw && numRestrictFrames != -1)
				RestrictiveDraw(&temp, bd->movingPos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
		}
		if (bd->rd->quickDraw && numRestrictFrames != -1)
		{
			RestrictiveDrawFrame(old_pos, PIECE_HOLE, PIECE_HOLE, PIECE_DEPTH);
			EnlargeCurrentToBox(&temp);
		}
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

		if (bd->rd->quickDraw && numRestrictFrames != -1)
		{
			float pos[3];
			float overSize;
			ClipBox temp;

			overSize = getDiceSize(bd) * 1.5f;
			copyPoint(pos, bd->diceMovingPos[0]);
			pos[2] -= getDiceSize(bd) / 2.0f;
			RestrictiveDrawFrame(pos, overSize, overSize, overSize);

			copyPoint(pos, bd->diceMovingPos[1]);
			pos[2] -= getDiceSize(bd) / 2.0f;
			RestrictiveDraw(&temp, pos, overSize, overSize, overSize);
			EnlargeCurrentToBox(&temp);

			CopyBox(&temp, &cb[numRestrictFrames]);
			if (!firstFrame)
				EnlargeCurrentToBox(&eraseCb);
			else if (firstFrame == -1)
				RestrictiveRedraw();
			else
				firstFrame = 0;

			CopyBox(&eraseCb, &temp);
		}
	}

	return 1;
}

void RollDice3d(BoardData *bd)
{	/* animate the dice roll if not below board */
	setDicePos(bd);
	SuspendInput();

	if (bd->rd->animateRoll)
	{
		animStartTime = get_time();

		bd->shakingDice = 1;
		stopNextTime = 0;
		setIdleFunc(bd, idleAnimate);

		setupDicePaths(bd, bd->dicePaths);
		/* Make sure shadows are in correct place */
		updateOccPos(bd);
		if (bd->rd->quickDraw)
		{	/* Mark this as the first frame (or -1 to indicate full draw in progress) */
			if (numRestrictFrames == -1)
				firstFrame = -1;
			else
				firstFrame = 1;
		}
		gtk_main();
	}
	else
	{
		/* Show dice on board */
		gtk_widget_queue_draw(bd->drawing_area3d);
		while(gtk_events_pending())
			gtk_main_iteration();	
	}
	ResumeInput();
}

void AnimateMove3d(BoardData *bd)
{
	SuspendInput();
	slide_move = 0;
	bd->moving = 1;

	SetupMove(bd);

	stopNextTime = 0;
	setIdleFunc(bd, idleAnimate);
	gtk_main();
	ResumeInput();
}

int idleWaveFlag(BoardData* bd)
{
	float elapsedTime = (float)(get_time() - animStartTime);
	bd->flagWaved = elapsedTime / 200;
	updateFlagOccPos(bd);
	RestrictiveDrawFlag(bd);
	return 1;
}

void ShowFlag3d(BoardData *bd)
{
	bd->flagWaved = 0;

	if (bd->rd->animateFlag && bd->resigned && ms.gs == GAME_PLAYING && bd->playing &&
		(ap[bd->turn == 1 ? 0 : 1].pt == PLAYER_HUMAN))		/* not for computer turn */
	{
		animStartTime = get_time();
		setIdleFunc(bd, idleWaveFlag);
	}
	else
		StopIdle3d(bd);

	waveFlag(bd, 0);
	updateFlagOccPos(bd);

	RestrictiveDrawFlag(bd);
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

float TestPerformance3d(BoardData* bd)
{
	float elapsedTime;

	setIdleFunc(bd, idleTestPerformance);
	testStartTime = get_time();
	numFrames = 0;
	gtk_main();
	elapsedTime = (float)(get_time() - testStartTime);

	return (numFrames / (elapsedTime / 1000.0f));
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

	bd->rd->showShadows = 0;
	bd->rd->showMoveIndicator = 0;
	bd->rd->fLabels = 0;
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

void SetupViewingVolume3d(BoardData *bd)
{
	GLint viewport[4];
	float tempMatrix[16];
	glGetIntegerv(GL_VIEWPORT, viewport);

	memcpy(tempMatrix, bd->modelMatrix, sizeof(float[16]));

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	SetupPerspVolume(bd, viewport);

	SetupLight3d(bd, bd->rd);
	calculateBackgroundSize(bd, viewport);
	if (memcmp(tempMatrix, bd->modelMatrix, sizeof(float[16])))
		RestrictiveRedraw();
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

void ClearTextures(BoardData* bd)
{
	int i;
	if (!bd->numTextures)
		return;

	MakeCurrent3d(bd->drawing_area3d);

	for (i = 0; i < bd->numTextures; i++)
	{
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

	SetupSimpleMat(&bd->gapColour, 0, 0, 0);
	SetupSimpleMat(&bd->logoMat, 1, 1, 1);
	SetupMat(&bd->flagMat, 1, 1, 1, 1, 1, 1, 1, 1, 1, 50, 0);
	SetupMat(&bd->flagNumberMat, 0, 0, .4f, 0, 0, .4f, 1, 1, 1, 100, 1);

	bd->diceList = bd->DCList = bd->pieceList = 0;
	bd->qobjTex = bd->qobj = 0;
	bd->flagNurb = 0;

	bd->numTextures = 0;

	bd->boardPoints = NULL;

	memset(bd->modelMatrix, 0, sizeof(float[16]));
}

extern void glPrintPointNumbers(BoardData* bd, const char *text)
{
	/* Align horizontally */
	glTranslatef(-getTextLen3d(&bd->numberFont, text) / 2.0f, 0, 0);
	RenderString3d(&bd->numberFont, text);
}

extern void glPrintCube(BoardData* bd, const char *text)
{
	/* Align horizontally and vertically */
	glTranslatef(-getTextLen3d(&bd->cubeFont, text) / 2.0f, -bd->cubeFont.height / 2.0f, 0);
	RenderString3d(&bd->cubeFont, text);
}

extern void glPrintNumbersRA(BoardData* bd, const char *text)
{
	/* Right align */
	glTranslatef(-getTextLen3d(&bd->numberFont, text), 0, 0);
	RenderString3d(&bd->numberFont, text);
}
