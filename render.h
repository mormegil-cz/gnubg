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
		  The board is 108x72 units, the dice 7x7 and cube 8x8. */
    int fHinges; /* TRUE if hinges should be drawn */
    int fLabels; /* TRUE if point numbers should be drawn */
    int fClockwise; /* orientation for board point numbers */
} renderdata;

typedef struct _renderimages {
    unsigned char *ach, *achChequer[ 2 ], *achChequerLabels, *achDice[ 2 ],
	*achPip[ 2 ], *achCube, *achCubeFaces;
    unsigned short *asRefract[ 2 ];
} renderimages;
    
extern void RenderInitialise( void );
    
extern void AlphaBlend( unsigned char *puchDest, int nDestStride,
			unsigned char *puchBack, int nBackStride,
			unsigned char *puchFore, int nForeStride,
			int cx, int cy );
extern void RefractBlend( unsigned char *puchDest, int nDestStride,
			  unsigned char *puchBack, int nBackStride,
			  unsigned char *puchFore, int nForeStride,
			  unsigned short *psRefract, int nRefractStride,
			  int cx, int cy );
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
extern void FreeImages( renderimages *pri );
    
extern void CalculateArea( renderdata *prd, unsigned char *puch, int nStride,
			   renderimages *pri, int anBoard[ 2 ][ 25 ],
			   int anDice[ 2 ], int anDicePosition[ 2 ][ 2 ],
			   int fDiceColour, int anCubePosition[ 2 ],
			   int nLogCube, int nCubeOrientation,
			   int x, int y, int cx, int cy );
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
