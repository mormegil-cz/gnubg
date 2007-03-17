
#ifndef _TYPES3D_H
#define _TYPES3D_H

typedef struct _BoardData3d BoardData3d;
typedef struct _Material Material;
typedef struct _Path Path;
typedef struct _Texture Texture;
typedef struct _ClipBox ClipBox;
typedef struct _GraphData GraphData;
typedef struct _OGLFont OGLFont;
typedef struct _TextureInfo TextureInfo;

typedef enum _displaytype {
    DT_2D, DT_3D
} displaytype;

typedef enum _lighttype {
    LT_POSITIONAL, LT_DIRECTIONAL
} lighttype;

typedef enum _PieceType
{
	PT_ROUNDED, PT_FLAT
} PieceType;

typedef enum _PieceTextureType
{
	PTT_TOP, PTT_ALL, NUM_TEXTURE_TYPES
} PieceTextureType;

typedef enum _TextureType
{
	TT_NONE = 1, TT_GENERAL = 2, TT_PIECE = 4, TT_HINGE = 8, TT_DISABLED = 16
} TextureType;
#define TT_COUNT 3	/* 3 texture types: general, piece and hinge */

#endif
