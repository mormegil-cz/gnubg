/*
* font3d.c
* by Jon Kinsey, 2005
*
* Draw 3d numbers using luxi font and freetype library
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
#include "backgammon.h"
#include "inc3d.h"
#include "myftgl.h"

#define FONT_SIZE (base_unit / 20.0f)
#define CUBE_FONT_SIZE (base_unit / 24.0f)

extern unsigned char auchLuxiSR[];
extern unsigned int cbLuxiSR;
extern unsigned char auchLuxiRB[];
extern unsigned int cbLuxiRB;

void BuildFont(BoardData* bd)
{
	CreateOGLFont(&bd->numberFont, auchLuxiSR, cbLuxiSR, 24, FONT_SIZE);
	CreateOGLFont(&bd->cubeFont, auchLuxiRB, cbLuxiRB, 44, CUBE_FONT_SIZE);
}

extern float getFontHeight(BoardData* bd)
{
	return bd->numberFont.height;
}

extern void glPrintPointNumbers(BoardData* bd, const char *text)
{
	/* Align horizontally */
	glTranslatef(-getTextLen(&bd->numberFont, text) / 2.0f, 0, 0);
	RenderString(&bd->numberFont, text);
}

extern void glPrintCube(BoardData* bd, const char *text)
{
	/* Align horizontally and vertically */
	glTranslatef(-getTextLen(&bd->cubeFont, text) / 2.0f, -bd->cubeFont.height / 2.0f, 0);
	RenderString(&bd->cubeFont, text);
}

extern void glPrintNumbersRA(BoardData* bd, const char *text)
{
	/* Right align */
	glTranslatef(-getTextLen(&bd->numberFont, text), 0, 0);
	RenderString(&bd->numberFont, text);
}
