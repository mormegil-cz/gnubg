/*
 * render.h
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2002.
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

#ifndef _RENDER_H_
#define _RENDER_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    
typedef enum _woodtype {
    WOOD_ALDER, WOOD_ASH, WOOD_BASSWOOD, WOOD_BEECH, WOOD_CEDAR,
    WOOD_EBONY, WOOD_FIR, WOOD_MAPLE, WOOD_OAK, WOOD_PINE, WOOD_REDWOOD,
    WOOD_WALNUT, WOOD_WILLOW, WOOD_PAINT
} woodtype;

#if USE_BOARD3D
typedef enum _displaytype {
    DT_2D, DT_3D
} displaytype;

typedef enum _lighttype {
    LT_POSITIONAL, LT_DIRECTIONAL
} lighttype;

typedef struct _Texture
{
	int texID;
	int width;
	int height;
} Texture;

#define FILENAME_SIZE 15
#define NAME_SIZE 20

typedef enum _TextureFormat
{
	TF_BMP, TF_PNG, TF_COUNT
} TextureFormat;

typedef enum _TextureType
{
	TT_NONE = 1, TT_GENERAL = 2, TT_PIECE = 4, TT_HINGE = 8, TT_DISABLED = 16, TT_COUNT = 3
} TextureType;

typedef struct _TextureInfo
{
	char file[FILENAME_SIZE + 1];
	char name[NAME_SIZE + 1];
	TextureType type;
	TextureFormat format;
} TextureInfo;

typedef struct _Material
{
	float ambientColour[4];
	float diffuseColour[4];
	float specularColour[4];
	int shine;
	int alphaBlend;
	TextureInfo* textureInfo;
	Texture* pTexture;
} Material;

typedef enum _PieceType
{
	PT_ROUNDED, PT_FLAT, NUM_PIECE_TYPES
} PieceType;

extern void FindTexture(TextureInfo** textureInfo, char* file);
extern void FindNamedTexture(TextureInfo** textureInfo, char* name);

#endif

typedef struct _renderdata {
    woodtype wt;
    double aarColour[ 2 ][ 4 ]; /* RGBA for each player */
    double aarDiceColour[ 2 ][ 4 ]; /* RGB(A) of dice for each player */
    int afDieColour[ 2 ]; /* TRUE means same colour as chequers */
    double aarDiceDotColour[ 2 ][ 4 ]; /* RGB(A) of dice dot for each player */
    double arCubeColour[ 4 ]; /* RGB(A) of cube */
    unsigned char aanBoardColour[ 4 ][ 4 ]; /* RGB(A) backgr., border, pts */
    int aSpeckle[ 4 ]; /* speckle for background, border, pts */
    float arRefraction[ 2 ], arCoefficient[ 2 ],
	arExponent[ 2 ]; /* Phong parameters for chequers */
    float arDiceCoefficient[ 2 ], arDiceExponent[ 2 ]; /* Phong parameters */
    float arLight[ 3 ]; /* XYZ light vector */
    float rRound; /* shape of chequers */
    int nSize; /* basic unit of board size, in pixels -- a chequer's
		  diameter is 6 of these units (and is 2 units thick).
		  The board is 108x82(old:72) units, the dice 7x7 and cube 8x8. */
    int fHinges; /* TRUE if hinges should be drawn */
    int fLabels; /* TRUE if point numbers should be drawn */
    int fClockwise; /* orientation for board point numbers */
    int fDynamicLabels; /* TRUE if the point numbers are dynamic, i.e.,
                           they adjust depending on the player on roll */
	int showMoveIndicator;
#if USE_BOARD3D
	displaytype fDisplayType;	/* 2d or 3d display */
	int showShadows;	/* Show 3d shadows */
	int shadowDarkness;	/* How dark are shadows */
	int animateRoll;	/* Animate dice rolls */
	int animateFlag;	/* Animate resignation flag */
	int closeBoardOnExit;	/* Animate board close on quit */
	int curveAccuracy;	/* Round curve approximation accuracy */
	lighttype lightType;	/* Positional/Directional light source */
	float lightPos[3];	/* x,y,z pos of light source */
	int lightLevels[3];	/* amibient/diffuse/specular light levels */
	int boardAngle;	/* Angle board is tilted at */
	int testSkewFactor;	/* Debug FOV adjustment */
	int planView;	/* Ortho view? */
	float diceSize;	/* How big are the dice */
	int roundedEdges;	/* Rounded board edges? */
	PieceType pieceType;
	Material rdChequerMat[2];	/* Chequer colours */
	Material rdDiceMat[2], rdDiceDotMat[2];
	Material rdCubeMat, rdCubeNumberMat;
	Material rdBaseMat, rdPointMat[2];
	Material rdBoxMat, rdHingeMat;
	Material rdPointNumberMat;
	Material rdBackGroundMat;
#endif
} renderdata;

typedef struct _renderimages {
    unsigned char *ach, *achChequer[ 2 ], *achChequerLabels, *achDice[ 2 ],
	*achPip[ 2 ], *achCube, *achCubeFaces;
    unsigned char *achResign;
    unsigned char *achResignFaces;
    unsigned short *asRefract[ 2 ];
    unsigned char *auchArrow[ 2 ];
    unsigned char *achLabels[ 2 ];
} renderimages;

extern renderdata rdDefault;
    
extern void RenderInitialise( void );
    
extern void CopyArea( unsigned char *puchDest, int nDestStride,
		      unsigned char *puchSrc, int nSrcStride,
		      int cx, int cy );
extern void CopyAreaRotateClip( unsigned char *puchDest, int nDestStride,
				int xDest, int yDest, int cxDest, int cyDest,
				unsigned char *puchSrc, int nSrcStride,
				int xSrc, int ySrc, int cx, int cy,
				int nTheta );
extern void AlphaBlend( unsigned char *puchDest, int nDestStride,
			unsigned char *puchBack, int nBackStride,
			unsigned char *puchFore, int nForeStride,
			int cx, int cy );
extern void AlphaBlendClip( unsigned char *puchDest, int nDestStride,
			    int xDest, int yDest, int cxDest, int cyDest,
			    unsigned char *puchBack, int nBackStride,
			    int xBack, int yBack,
			    unsigned char *puchFore, int nForeStride,
			    int xFore, int yFore, int cx, int cy );
extern void RefractBlend( unsigned char *puchDest, int nDestStride,
			  unsigned char *puchBack, int nBackStride,
			  unsigned char *puchFore, int nForeStride,
			  unsigned short *psRefract, int nRefractStride,
			  int cx, int cy );
extern void RefractBlendClip( unsigned char *puchDest, int nDestStride,
			      int xDest, int yDest, int cxDest, int cyDest,
			      unsigned char *puchBack, int nBackStride,
			      int xBack, int yBack,
			      unsigned char *puchFore, int nForeStride,
			      int xFore, int yFore,
			      unsigned short *psRefract, int nRefractStride,
			      int cx, int cy );
extern void AlphaBlendClip2( unsigned char *puchDest, int nDestStride,
			     int xDest, int yDest, int cxDest, int cyDest,
			     unsigned char *puchBack, int nBackStride,
			     int xBack, int yBack,
			     unsigned char *puchFore, int nForeStride,
			     int xFore, int yFore, int cx, int cy );
extern void RenderBoard( renderdata *prd, unsigned char *puch, int nStride );
extern void RenderChequers( renderdata *prd, unsigned char *puch0,
			    unsigned char *puch1, unsigned short *psRefract0,
			    unsigned short *psRefract1, int nStride );
extern void RenderChequerLabels( renderdata *prd, unsigned char *puch,
				 int nStride );
extern void RenderCube( renderdata *prd, unsigned char *puch, int nStride );
extern void RenderCubeFaces( renderdata *prd, unsigned char *puch,
			     int nStride, unsigned char *puchCube,
			     int nStrideCube );
extern void RenderDice( renderdata *prd, unsigned char *puch0,
			unsigned char *puch1, int nStride );
extern void RenderPips( renderdata *prd, unsigned char *puch0,
			unsigned char *puch1, int nStride );
extern void RenderImages( renderdata *prd, renderimages *pri );
#if HAVE_LIBART
extern void RenderArrows( renderdata *prd, unsigned char* puch0,
			  unsigned char* puch1, int nStride );
#endif

extern void
RenderBoardLabels( renderdata *prd, 
                   unsigned char *achLo, unsigned char *achHi, int nStride );

extern void FreeImages( renderimages *pri );
    
extern void CalculateArea( renderdata *prd, unsigned char *puch, int nStride,
			   renderimages *pri, int anBoard[ 2 ][ 25 ],
			   int anOff[ 2 ], int anDice[ 2 ],
			   int anDicePosition[ 2 ][ 2 ],
			   int fDiceColour, int anCubePosition[ 2 ],
			   int nLogCube, int nCubeOrientation,
                           int anResignPosition[ 2 ],
                           int fResign, int fResignOrientation,
                           int anArrowPosition[ 2 ],
			   int fPlaying, int nPlayer,
			   int x, int y, int cx, int cy );
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
