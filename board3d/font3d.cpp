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

#include "../config.h"

/* Duplicate definition of base_unit as including inc3d.h caused problems */
#define base_unit .05f

#if HAVE_FREETYPE

#include "FTGLPolygonFont.h"
#include "FTGLOutlineFont.h"
#include "FTFont.h"

/* Avoid FTGLOutlineFont::render() as expensive to call repeatedly */
class MyOutlineFont : public FTGLOutlineFont
{
	void render(const char* string) {FTFont::render(string);}
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
		fonts[0] = new FTGLPolygonFont();
		fonts[0]->Open(fd, fs);
		fonts[0]->FaceSize(ptSize);
	
		fonts[1] = new MyOutlineFont();
		fonts[1]->Open(fd, fs);
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
		fonts[mode]->render(text);
	}

	void printHorVertAlign(const char* text, int mode)
	{
		glTranslatef(0, -height / 2.0f, 0);
		glScalef(size, size, 1);
		glTranslatef(-getTextLen(text) / 2.0f, 0, 0);
		fonts[mode]->render(text);
	}

private:
	FTFont* fonts[2];
	float height;
	float size;
};

font *numberFont, *cubeFont;

extern "C" void BuildFont()
{
	numberFont = new font(24, FONT_SIZE, auchLuxiSR, cbLuxiSR);
	cubeFont = new font(48, CUBE_FONT_SIZE, auchLuxiRB, cbLuxiRB);
}

extern "C" void KillFont()
{
	delete numberFont;
	delete cubeFont;
}

extern "C" float getFontHeight()
{
	return numberFont->getHeight();
}

extern "C" void glPrintPointNumbers(const char *text, int mode)
{
	numberFont->printHorAlign(text, mode);
}

extern "C" void glPrintCube(const char *text, int mode)
{
	cubeFont->printHorVertAlign(text, mode);
}

#else

/* Simple glut alternative font for testing */
#include <GL/glut.h>

extern "C" void KillFont() {}
extern "C" float getFontHeight(){return base_unit;}
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
