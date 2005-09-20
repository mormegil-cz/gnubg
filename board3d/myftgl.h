/*
 * myftgl.h
 *
 * by Jon Kinsey, 2005
 *
 * Convert font to opengl using freetype
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

#ifndef _MYFTGL_H_
#define _MYFTGL_H_

#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct _Glyph
{
	int list;
	int advance;
} Glyph;

typedef struct _OGLFont
{
	FT_Face face;
	int hasKerningTable;
	Glyph glyph[10];
	float scale;
	float height;
} OGLFont;

extern int CreateOGLFont(OGLFont *pFont, unsigned char *pBufferBytes, unsigned int bufferSizeInBytes, int pointSize, float scale);
extern int RenderString(OGLFont *pFont, const char* str);
extern float getTextLen(OGLFont *pFont, const char* text);

#endif
