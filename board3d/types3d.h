/* This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef TYPES3D_H
#define TYPES3D_H

typedef struct _BoardData3d BoardData3d;
typedef struct _Material Material;
typedef struct _Path Path;
typedef struct _Texture Texture;
typedef struct _ClipBox ClipBox;
typedef struct _GraphData GraphData;
typedef struct _OGLFont OGLFont;
typedef struct _TextureInfo TextureInfo;
typedef struct _Flag3d Flag3d;

typedef enum _displaytype {
    DT_2D, DT_3D
} displaytype;

typedef enum _lighttype {
    LT_POSITIONAL, LT_DIRECTIONAL
} lighttype;

typedef enum _PieceType {
    PT_ROUNDED, PT_FLAT
} PieceType;

typedef enum _PieceTextureType {
    PTT_TOP, PTT_ALL, NUM_TEXTURE_TYPES
} PieceTextureType;

typedef enum _TextureType {
    TT_NONE = 1, TT_GENERAL = 2, TT_PIECE = 4, TT_HINGE = 8, TT_DISABLED = 16
} TextureType;
#define TT_COUNT 3              /* 3 texture types: general, piece and hinge */

typedef int idleFunc(BoardData3d * bd3d);

/* Work out which sides of dice to draw */
typedef struct _diceTest {
    int top, bottom, side[4];
} diceTest;

typedef struct _DiceRotation {
    float xRotStart, yRotStart;
    float xRot, yRot;
    float xRotFactor, yRotFactor;
} DiceRotation;

#define DF_VARIABLE_OPACITY 1
#define DF_NO_ALPHA 2
#define DF_FULL_ALPHA 4

#endif
