/*
* font3d.cpp
* by Jon Kinsey, 2003
*
* Draw 3d numbers using luxi font and FTGL library
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

#if HAVE_FTGL

#include "FTGLPolygonFont.h"
#include "FTGLOutlineFont.h"
#include "FTFont.h"
#include "inc3d.h"

/* Avoid FTGLOutlineFont::Render() as expensive to call repeatedly */
class MyOutlineFont : public FTGLOutlineFont
{
public:
	MyOutlineFont(const unsigned char *pBufferBytes, size_t bufferSizeInBytes) :
		FTGLOutlineFont(pBufferBytes, bufferSizeInBytes) {}

	void render(const char* string) {FTFont::Render(string);}
};

#define FONT_SIZE (base_unit / 20.0f)
#define CUBE_FONT_SIZE (base_unit / 24.0f)

extern "C" unsigned char auchLuxiSR[];
extern "C" unsigned int cbLuxiSR;
extern "C" unsigned char auchLuxiRB[];
extern "C" unsigned int cbLuxiRB;

class font
{
public:
	font(int ptSize, float size, unsigned char* fd, unsigned int fs)
	{
		fonts[0] = new FTGLPolygonFont(fd, fs);
		fonts[0]->FaceSize(ptSize);
	
		fonts[1] = new MyOutlineFont(fd, fs);
		fonts[1]->FaceSize(ptSize);

		float sx, sy, sz, fx, fy, fz;
		fonts[1]->BBox("1", sx, sy, sz, fx, fy, fz);
		height = (fy - sy) * size;
		
		this->size = size;
	}

	~font() {delete fonts[0]; delete fonts[1];}

	float getHeight() {return height;}

	float getTextLen(const char* text)
	{
		float sx, sy, sz, fx, fy, fz;
		fonts[1]->BBox(text, sx, sy, sz, fx, fy, fz);
		return fx;
	}

	void printHorAlign(const char* text, int mode)
	{
		glScalef(size, size, 1);
		glTranslatef(-getTextLen(text) / 2.0f, 0, 0);
		fonts[mode]->Render(text);
	}

	void printHorVertAlign(const char* text, int mode)
	{
		glTranslatef(0, -height / 2.0f, 0);
		glScalef(size, size, 1);
		glTranslatef(-getTextLen(text) / 2.0f, 0, 0);
		fonts[mode]->Render(text);
	}

private:
	FTFont* fonts[2];
	float height;
	float size;
};

extern "C" void BuildFont(BoardData* bd)
{
	bd->numberFont = new font(24, FONT_SIZE, auchLuxiSR, cbLuxiSR);
	bd->cubeFont = new font(48, CUBE_FONT_SIZE, auchLuxiRB, cbLuxiRB);
}

extern "C" void KillFont(BoardData* bd)
{
	delete (font*)bd->numberFont;
	delete (font*)bd->cubeFont;
}

extern "C" float getFontHeight(BoardData* bd)
{
	return ((font*)bd->numberFont)->getHeight();
}

extern "C" void glPrintPointNumbers(BoardData* bd, const char *text, int mode)
{
	((font*)bd->numberFont)->printHorAlign(text, mode);
}

extern "C" void glPrintCube(BoardData* bd, const char *text, int mode)
{
	((font*)bd->cubeFont)->printHorVertAlign(text, mode);
}

#else

/* Simple glut alternative font for testing */
#include <GL/glut.h>

extern "C" void KillFont() {}
extern "C" float getFontHeight() {return base_unit;}
extern "C" void BuildFont(){}

float getTextLen(const char* text)
{
	float width = 0;
	while (*text)
	{
		width += glutStrokeWidth((void *)GLUT_STROKE_ROMAN, *text);
		text++;
	}
	return width;
}

void glutPrint(const char *text, float fontSize)
{
	glPushMatrix();
	glScalef(fontSize, fontSize, fontSize);
	glTranslatef(-getTextLen(text) / 2.0f, 0, 0);

	while (*text)
	{
		glutStrokeCharacter((void *)GLUT_STROKE_ROMAN, *text);
		text++;
	}
	glPopMatrix();
}

extern "C" void glPrintPointNumbers(const char *text, int mode)
{
	glutPrint(text, base_unit / 100);
}

extern "C" void glPrintCube(const char *text, int mode)
{
	glutPrint(text, base_unit / 80);
}

#endif
