/*
 * htmlimages.c
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_ALLOCA_H
#include <alloca.h>
#endif

#if HAVE_ALLOCA
#ifndef alloca
#define alloca __builtin_alloca
#endif
#endif

#include <errno.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <string.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_LIBART
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_point.h>
#include <libart_lgpl/art_vpath.h>
#include <libart_lgpl/art_bpath.h>
#include <libart_lgpl/art_vpath_bpath.h>
#include <libart_lgpl/art_svp.h>
#include <libart_lgpl/art_svp_vpath.h>
#include <libart_lgpl/art_gray_svp.h>
#include <libart_lgpl/art_rgb.h>
#include <libart_lgpl/art_rgb_svp.h>
#endif

#include "backgammon.h"
#include "export.h"
#include "i18n.h"
#include "render.h"
#include "renderprefs.h"
#include "boardpos.h"
#include "boarddim.h"


#if HAVE_LIBPNG

static void DrawPips( unsigned char *auchDest, int nStride,
		      unsigned char *auchPip, int nPipStride,
		      int nSize, int n ) {
    
    int ix, iy, afPip[ 9 ];
	    
    afPip[ 0 ] = afPip[ 8 ] = ( n == 2 ) || ( n == 3 ) || ( n == 4 ) ||
	( n == 5 ) || ( n == 6 );
    afPip[ 1 ] = afPip[ 7 ] = 0;
    afPip[ 2 ] = afPip[ 6 ] = ( n == 4 ) || ( n == 5 ) || ( n == 6 );
    afPip[ 3 ] = afPip[ 5 ] = n == 6;
    afPip[ 4 ] = n & 1;

    for( iy = 0; iy < 3; iy++ )
	for( ix = 0; ix < 3; ix++ )
	    if( afPip[ iy * 3 + ix ] )
		CopyArea( auchDest + ( 1 + 2 * ix ) * nSize * 3 +
			  ( 1 + 2 * iy ) * nSize * nStride, nStride,
			  auchPip, nPipStride, nSize, nSize );
}

#endif
		      
extern void CommandExportHTMLImages( char *sz ) {

#if HAVE_LIBPNG

#if HAVE_ALLOCA
    char *szFile;
#else
    char szFile[ 4096 ];
#endif
    char *pchFile;
    renderdata rd;
    int nStride = BOARD_WIDTH * 4 * 3, s = 4, ss = s * 3 / 4, i, j, k;
    /* indices are: g/r, d/u, x/o */
    unsigned char auchPoint[ 2 ][ 2 ][ 2 ][ POINT_WIDTH * 4 * 3 * DISPLAY_POINT_HEIGHT * 4 ],
	auchOff[ 2 ][ BEAROFF_WIDTH * 4 * 3 * DISPLAY_BEAROFF_HEIGHT * 4 ],
	auchBar[ 2 ][ BAR_WIDTH * 4 * 3 * ( 3 * CHEQUER_HEIGHT + 4 ) * 4 ],
	auchBoard[ BOARD_WIDTH * 4 * BOARD_HEIGHT * 4 * 3 ],
	auchChequer[ 2 ][ CHEQUER_WIDTH * 4 * CHEQUER_HEIGHT * 4 * 4 ],
	auchChequerLabels[ 12 * 4 * 4 * 4 * 4 * 3 ],
	auchCube[ CUBE_WIDTH * 3 * CUBE_HEIGHT * 3 * 4 ],
	auchCubeFaces[ 12 * CUBE_LABEL_WIDTH * 3 * CUBE_LABEL_HEIGHT * 3 * 3 ],
	auchDice[ 2 ][ DIE_WIDTH * 3 * DIE_HEIGHT * 3 * 4 ],
	auchPips[ 2 ][ 3 * 3 * 3 ],
	auchMidBoard[ BOARD_CENTER_WIDTH * 4 * 3 * BOARD_CENTER_HEIGHT * 4 ];
    unsigned short asRefract[ 2 ][ CHEQUER_WIDTH * 4 * CHEQUER_HEIGHT * 4 ];
    static char *aszCube[ 3 ] = { "ct", "cb" };
#if HAVE_LIBART
    unsigned char *auchArrow[ 2 ];
    unsigned char auchMidlb[ BOARD_WIDTH * 4 * BOARD_HEIGHT * 4 * 3 ];
    int x, y;
#endif
    unsigned char auchLo[ BOARD_WIDTH * 4 * BORDER_HEIGHT * 4 * 4 ];
    unsigned char auchHi[ BOARD_WIDTH * 4 * BORDER_HEIGHT * 4 * 4 ];
    unsigned char auchLabel[ BOARD_WIDTH * 4 * BORDER_HEIGHT * 4 * 3 ];
#if 0
    int xx = 0;
#endif
	
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputf( _("You must specify a file to export to (see `%s')\n" ),
		 "help export htmlimages" );
	return;
    }
  
    if( mkdir( sz
#ifndef WIN32
	       , 0777
#endif
	    ) < 0 && errno != EEXIST ) {
	outputerr ( sz );
	return;
    }

    ProgressStartValue( _("Generating image:"), 342 );
    
#if HAVE_ALLOCA
    szFile = alloca( strlen( sz ) + 32 );
#endif
    strcpy( szFile, sz );
    if( szFile[ strlen( szFile ) - 1 ] != '/' )
	strcat( szFile, "/" );
    pchFile = strchr( szFile, 0 );

    memcpy( &rd, &rdAppearance, sizeof( renderdata ) );
    
    rd.fLabels = TRUE; 
    rd.nSize = s;

    RenderBoard( &rd, auchBoard, BOARD_WIDTH * 4 * 3 );
    RenderChequers( &rd, auchChequer[ 0 ], auchChequer[ 1 ], asRefract[ 0 ],
		    asRefract[ 1 ], CHEQUER_WIDTH * 4 * 4 );
    RenderChequerLabels( &rd, auchChequerLabels, CHEQUER_LABEL_WIDTH * 4 * 3 );

#if HAVE_LIBART
    for ( i = 0; i < 2; ++i )
      auchArrow[ i ] = 
        art_new( art_u8, s * ARROW_WIDTH * 4 * s * ARROW_HEIGHT );

    RenderArrows( &rd, auchArrow[0], auchArrow[1], s * ARROW_WIDTH * 4 );
#endif /* HAVE_LIBART */

    RenderBoardLabels( &rd, auchLo, auchHi, BOARD_WIDTH * 4 * 4 );

    /* cubes and dices are rendered a bit smaller */

    rd.nSize = ss;

    RenderCube( &rd, auchCube, CUBE_WIDTH * ss * 4 );
    RenderCubeFaces( &rd, auchCubeFaces, CUBE_LABEL_WIDTH * ss * 3, auchCube, CUBE_WIDTH * ss * 4 );
    RenderDice( &rd, auchDice[ 0 ], auchDice[ 1 ], DIE_WIDTH * ss * 4 );
    RenderPips( &rd, auchPips[ 0 ], auchPips[ 1 ], ss * 3 );

    

#define WRITE( img, stride, cx, cy ) \
    if( WritePNG( szFile, (img), (stride), (cx), (cy) ) ) { \
	ProgressEnd(); \
	return; \
    } else \
        ProgressValueAdd( 1 )

    /* top border */
    
    CopyArea( auchLabel, BOARD_WIDTH * s * 3,
              auchBoard, BOARD_WIDTH * s * 3,
              BOARD_WIDTH * s, BORDER_HEIGHT * s );

    AlphaBlendClip( auchLabel, BOARD_WIDTH * s * 3,
                    0, 0, 
                    BOARD_WIDTH * s, BORDER_HEIGHT * s,
                    auchLabel, BOARD_WIDTH * s * 3,
                    0, 0,
                    auchHi,
                    BOARD_WIDTH * s * 4,
                    0, 0, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    strcpy( pchFile, "b-hitop.png" );
    WRITE( auchLabel, nStride, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    CopyArea( auchLabel, BOARD_WIDTH * s * 3,
              auchBoard, BOARD_WIDTH * s * 3,
              BOARD_WIDTH * s, BORDER_HEIGHT * s );

    AlphaBlendClip( auchLabel, BOARD_WIDTH * s * 3,
                    0, 0, 
                    BOARD_WIDTH * s, BORDER_HEIGHT * s,
                    auchLabel, BOARD_WIDTH * s * 3,
                    0, 0,
                    auchLo,
                    BOARD_WIDTH * s * 4,
                    0, 0, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    strcpy( pchFile, "b-lotop.png" );
    WRITE( auchLabel, nStride, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    /* empty points */
    strcpy( pchFile, "b-gd.png" );
    WRITE( auchBoard + s * nStride * 3 + BEAROFF_WIDTH * s * 3, nStride,
	   POINT_WIDTH * s, POINT_HEIGHT * s );
    
    strcpy( pchFile, "b-rd.png" );
    WRITE( auchBoard + s * nStride * 3 + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	   POINT_WIDTH * s, POINT_HEIGHT * s );
    
    strcpy( pchFile, "b-ru.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) + BEAROFF_WIDTH * s * 3, nStride,
	   POINT_WIDTH * s, POINT_HEIGHT * s );
    
    strcpy( pchFile, "b-gu.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	   POINT_WIDTH * s, POINT_HEIGHT * s );

    /* upper left bearoff tray */

    strcpy( pchFile, "b-loff-x0.png" );
    WRITE( auchBoard 
           + s * nStride * BORDER_HEIGHT, 
           nStride,
	   BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

    /* upper right bearoff tray */

    strcpy( pchFile, "b-roff-x0.png" );
    WRITE( auchBoard 
           + s * nStride * BORDER_HEIGHT 
           + 3 * s * ( BOARD_WIDTH - BEAROFF_WIDTH ), 
           nStride,
	   BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

    /* lower right bearoff tray */

    strcpy( pchFile, "b-roff-o0.png" );
    WRITE( auchBoard 
           + s * nStride * ( BORDER_HEIGHT + POINT_HEIGHT + 
                             BOARD_CENTER_HEIGHT )
           + 3 * s * ( BOARD_WIDTH - BEAROFF_WIDTH ), 
           nStride,
	   BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

    /* lower left bearoff tray */

    strcpy( pchFile, "b-loff-o0.png" );
    WRITE( auchBoard 
           + s * nStride * ( BORDER_HEIGHT + POINT_HEIGHT + 
                             BOARD_CENTER_HEIGHT ),
           nStride,
	   BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

    

    
    /* bar top */
    strcpy( pchFile, "b-ct.png" );
    WRITE( auchBoard + s * nStride * 3 + ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3, nStride,
	   BAR_WIDTH * s, CUBE_HEIGHT * s );

    /* bearoff tray dividers */

#if HAVE_LIBART

    /* the code below is a bit ugly, but what the heck: it works! */

    for ( i = 0; i < 2; ++i ) {
      /* 0 = left, 1 = right */

      int offset_x = 
        i * ( 3 * s * ( BOARD_WIDTH - BEAROFF_WIDTH ) );

      for ( j = 0; j < 2; ++j ) {
        /* 0 = player 0, 1 = player 1 */

        memcpy( auchMidlb, auchBoard, BOARD_WIDTH * 4 * BOARD_HEIGHT * 4 * 3 );
        ArrowPosition( i /* rd.fClockwise */, s, &x, &y );

        AlphaBlendClip2( auchMidlb, nStride,
                         x, y,
                         BOARD_WIDTH * s, BOARD_HEIGHT * s, 
                         auchMidlb, nStride,
                         x, y, 
                         auchArrow[ j ],
                         s * ARROW_WIDTH * 4,
                         0, 0,
                         s * ARROW_WIDTH,
                         s * ARROW_HEIGHT );
        
        sprintf( pchFile, "b-mid%cb-%c.png", i ? 'r' : 'l', j ? 'o' : 'x' );
        WRITE( auchMidlb + 
               s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 )
               + offset_x,
               nStride, BEAROFF_WIDTH * s, BOARD_CENTER_HEIGHT * s );

      }

    }

#else
    for ( i = 0; i < 2; ++i ) {
      sprintf( pchFile, "b-midlb-%c.png", i ? 'x' : 'o' );
      WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ),
	     nStride, BEAROFF_WIDTH * s, BOARD_CENTER_HEIGHT * s );
    }
#endif

    /* left bearoff centre */

    strcpy( pchFile, "b-midlb.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ),
	   nStride, BEAROFF_WIDTH * s, BOARD_CENTER_HEIGHT * s );

    /* right bearoff centre */

    strcpy( pchFile, "b-midrb.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) + 3 * s * ( BOARD_WIDTH - BEAROFF_WIDTH ),
	   nStride, BEAROFF_WIDTH * s, BOARD_CENTER_HEIGHT * s );

    /* left board centre */
    strcpy( pchFile, "b-midl.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) + s * BEAROFF_WIDTH * 3,
	   nStride, BOARD_CENTER_WIDTH * s, BOARD_CENTER_HEIGHT * s );

    /* bar centre */
    strcpy( pchFile, "b-midc.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) + s * ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * 3, nStride,
	   BAR_WIDTH * s, BOARD_CENTER_HEIGHT * s );

    /* right board centre */
    strcpy( pchFile, "b-midr.png" );
    WRITE( auchBoard + s * nStride * ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) + s * ( BOARD_WIDTH / 2 + BAR_WIDTH / 2 ) * 3, nStride,
	   BOARD_CENTER_WIDTH * s, BOARD_CENTER_HEIGHT * s );

    /* bar bottom */
    strcpy( pchFile, "b-cb.png" );
    WRITE( auchBoard 
           + s * nStride * ( BOARD_HEIGHT - CUBE_HEIGHT - BORDER_HEIGHT ) 
           + ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3, 
           nStride,
	   BAR_WIDTH * s, CUBE_HEIGHT * s );

    /* bottom border */
    CopyArea( auchLabel, BOARD_WIDTH * s * 3,
              auchBoard + s * nStride * ( BOARD_HEIGHT - BORDER_HEIGHT ),
	      BOARD_WIDTH * s * 3, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    AlphaBlendClip( auchLabel, BOARD_WIDTH * s * 3,
                    0, 0, 
                    BOARD_WIDTH * s, BORDER_HEIGHT * s,
                    auchLabel, BOARD_WIDTH * s * 3,
                    0, 0,
                    auchHi,
                    BOARD_WIDTH * s * 4,
                    0, 0, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    strcpy( pchFile, "b-hibot.png" );
    WRITE( auchLabel, nStride, BOARD_WIDTH * s, BORDER_HEIGHT * s );
    
    CopyArea( auchLabel, BOARD_WIDTH * s * 3,
              auchBoard + s * nStride * ( BOARD_HEIGHT - BORDER_HEIGHT ),
	      BOARD_WIDTH * s * 3, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    AlphaBlendClip( auchLabel, BOARD_WIDTH * s * 3,
                    0, 0, 
                    BOARD_WIDTH * s, BORDER_HEIGHT * s,
                    auchLabel, BOARD_WIDTH * s * 3,
                    0, 0,
                    auchLo,
                    BOARD_WIDTH * s * 4,
                    0, 0, BOARD_WIDTH * s, BORDER_HEIGHT * s );

    strcpy( pchFile, "b-lobot.png" );
    WRITE( auchLabel, nStride, BOARD_WIDTH * s, BORDER_HEIGHT * s );


    CopyArea( auchPoint[ 0 ][ 0 ][ 0 ], POINT_WIDTH * 4 * 3,
	      auchBoard + BORDER_HEIGHT * s * nStride + BEAROFF_WIDTH * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 0 ][ 0 ][ 1 ], POINT_WIDTH * 4 * 3,
	      auchBoard + BORDER_HEIGHT * s * nStride + BEAROFF_WIDTH * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 0 ][ 1 ][ 0 ], POINT_WIDTH * 4 * 3,
	      auchBoard + ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 0 ][ 1 ][ 1 ], POINT_WIDTH * 4 * 3,
	      auchBoard + ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 1 ][ 0 ][ 0 ], POINT_WIDTH * 4 * 3,
	      auchBoard + BORDER_HEIGHT * s * nStride + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 1 ][ 0 ][ 1 ], POINT_WIDTH * 4 * 3,
	      auchBoard + BORDER_HEIGHT * s * nStride + ( BEAROFF_WIDTH + POINT_WIDTH ) * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 1 ][ 1 ][ 0 ], POINT_WIDTH * 4 * 3,
	      auchBoard + ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride + BEAROFF_WIDTH * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );
    CopyArea( auchPoint[ 1 ][ 1 ][ 1 ], POINT_WIDTH * 4 * 3,
	      auchBoard + ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride + BEAROFF_WIDTH * s * 3, nStride,
	      POINT_WIDTH * s, POINT_HEIGHT * s );

    CopyArea( auchOff[ 0 ], BEAROFF_WIDTH * 4 * 3,
	      auchBoard + BORDER_HEIGHT * s * nStride +
	      ( BOARD_WIDTH - BEAROFF_WIDTH ) * s * 3, nStride,
	      BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );
    CopyArea( auchOff[ 1 ], BEAROFF_WIDTH * 4 * 3,
	      auchBoard + ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride +
	      ( BOARD_WIDTH - BEAROFF_WIDTH ) * s * 3, nStride,
	      BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

    CopyArea( auchBar[ 0 ], 
              BAR_WIDTH * 4 * 3,
	      auchBoard 
              + ( BORDER_HEIGHT + CUBE_HEIGHT ) * s * nStride +
	      ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3, 
              nStride,
	      BAR_WIDTH * s, 
              ( POINT_HEIGHT - CUBE_HEIGHT ) * s );
    CopyArea( auchBar[ 1 ], 
              BAR_WIDTH * 4 * 3,
	      auchBoard + 
              ( BOARD_HEIGHT / 2 + BOARD_CENTER_HEIGHT / 2 ) * s * nStride +
	      ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3, 
              nStride,
	      BAR_WIDTH * s, 
              ( POINT_HEIGHT - CUBE_HEIGHT ) * s );

    for( i = 1; i <= 5; i++ ) {
	for( j = 0; j < 2; j++ ) {
	    /* gd */
	    sprintf( pchFile, "b-gd-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 0 ][ 0 ][ j ] + CHEQUER_HEIGHT * 4 * ( i - 1 ) *
			  CHEQUER_WIDTH * 4 * 3, CHEQUER_WIDTH * 4 * 3,
			  auchBoard + ( BORDER_HEIGHT + CHEQUER_HEIGHT * ( i - 1 ) ) * s * nStride +
			  BEAROFF_WIDTH * s * 3, nStride,
			  auchChequer[ j ], CHEQUER_WIDTH * 4 * 4,
			  asRefract[ j ], CHEQUER_WIDTH * 4,
			  CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	    WRITE( auchPoint[ 0 ][ 0 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}

	for( j = 0; j < 2; j++ ) {
	    /* rd */
	    sprintf( pchFile, "b-rd-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 1 ][ 0 ][ j ] + CHEQUER_HEIGHT * 4 * ( i - 1 ) *
			  CHEQUER_WIDTH * 4 * 3, CHEQUER_WIDTH * 4 * 3,
			  auchBoard + ( BORDER_HEIGHT + CHEQUER_HEIGHT * ( i - 1 ) ) * s * nStride +
			  ( BEAROFF_WIDTH + CHEQUER_WIDTH ) * s * 3, nStride,
			  auchChequer[ j ], CHEQUER_WIDTH * 4 * 4,
			  asRefract[ j ], CHEQUER_WIDTH * 4,
			  CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	    WRITE( auchPoint[ 1 ][ 0 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}

	for( j = 0; j < 2; j++ ) {
	    /* gu */
	    sprintf( pchFile, "b-gu-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 0 ][ 1 ][ j ] + CHEQUER_HEIGHT * 4 * ( 5 - i ) *
			  CHEQUER_WIDTH * 4 * 3, CHEQUER_WIDTH * 4 * 3,
			  auchBoard + ( ( BOARD_HEIGHT / 2 + BEAROFF_DIVIDER_HEIGHT / 2 ) + CHEQUER_HEIGHT * ( 5 - i ) ) * s * nStride +
			  ( BEAROFF_WIDTH + CHEQUER_WIDTH ) * s * 3, nStride,
			  auchChequer[ j ], CHEQUER_WIDTH * 4 * 4,
			  asRefract[ j ], CHEQUER_WIDTH * 4,
			  CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	    WRITE( auchPoint[ 0 ][ 1 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}

	for( j = 0; j < 2; j++ ) {
	    /* ru */
	    sprintf( pchFile, "b-ru-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 1 ][ 1 ][ j ] + CHEQUER_HEIGHT * 4 * ( 5 - i ) *
			  CHEQUER_WIDTH * 4 * 3, CHEQUER_WIDTH * 4 * 3,
			  auchBoard + ( ( BOARD_HEIGHT / 2 + BEAROFF_DIVIDER_HEIGHT / 2 ) + CHEQUER_HEIGHT * ( 5 - i ) ) * s * nStride +
			  BEAROFF_WIDTH * s * 3, nStride,
			  auchChequer[ j ], CHEQUER_WIDTH * 4 * 4,
			  asRefract[ j ], CHEQUER_WIDTH * 4,
			  CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	    WRITE( auchPoint[ 1 ][ 1 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}

	/* off */
	sprintf( pchFile, "b-roff-x%d.png", i );
	RefractBlend( auchOff[ 0 ] + BORDER_WIDTH * 4 * 3 + CHEQUER_HEIGHT * 4 * ( i - 1 ) *
		      BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * 4 * 3,
		      auchBoard + ( BORDER_WIDTH + CHEQUER_HEIGHT * ( i - 1 ) ) * s * nStride +
		      ( BOARD_WIDTH - BORDER_WIDTH - CHEQUER_WIDTH ) * s * 3, nStride,
		      auchChequer[ 0 ], CHEQUER_WIDTH * 4 * 4,
		      asRefract[ 0 ], CHEQUER_WIDTH * 4, CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	WRITE( auchOff[ 0 ], BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

	sprintf( pchFile, "b-roff-o%d.png", i );
	RefractBlend( auchOff[ 1 ] + BORDER_WIDTH * 4 * 3 + CHEQUER_HEIGHT * 4 * ( 5 - i ) *
		      BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * 4 * 3,
		      auchBoard + ( ( BOARD_HEIGHT / 2 + BEAROFF_DIVIDER_HEIGHT / 2 ) + CHEQUER_WIDTH * ( 5 - i ) ) * s * nStride +
		      ( BOARD_WIDTH - BORDER_WIDTH - CHEQUER_WIDTH ) * s * 3, nStride,
		      auchChequer[ 1 ], CHEQUER_WIDTH * 4 * 4,
		      asRefract[ 1 ], CHEQUER_WIDTH * 4, CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	WRITE( auchOff[ 1 ], BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );
    }

    for( i = 6; i <= 15; i++ ) {
	for( j = 0; j < 2; j++ ) {
	    /* gd */
	    sprintf( pchFile, "b-gd-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 0 ][ 0 ][ j ] + CHEQUER_HEIGHT * 4 * s * POINT_WIDTH * s * 3 +
		      s * 3 + s * POINT_WIDTH * s * 3, POINT_WIDTH * s * 3,
		      auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		      CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	    WRITE( auchPoint[ 0 ][ 0 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* rd */
	    sprintf( pchFile, "b-rd-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 1 ][ 0 ][ j ] + CHEQUER_HEIGHT * 4 * s * POINT_WIDTH * s * 3 +
		      s * 3 + s * POINT_WIDTH * s * 3, POINT_WIDTH * s * 3,
		      auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		      CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, 4 * s );
	    WRITE( auchPoint[ 1 ][ 0 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* gu */
	    sprintf( pchFile, "b-gu-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 0 ][ 1 ][ j ] +
		      s * 3 + s * POINT_WIDTH * s * 3, POINT_WIDTH * s * 3,
		      auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		      CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	    WRITE( auchPoint[ 0 ][ 1 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* ru */
	    sprintf( pchFile, "b-ru-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 1 ][ 1 ][ j ] +
		      s * 3 + s * POINT_WIDTH * s * 3, POINT_WIDTH * s * 3,
		      auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		      CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	    WRITE( auchPoint[ 1 ][ 1 ][ j ], POINT_WIDTH * 4 * 3,
		   POINT_WIDTH * s, POINT_HEIGHT * s );
	}
	
	/* off */
	sprintf( pchFile, "b-roff-x%d.png", i );
	CopyArea( auchOff[ 0 ] + CHEQUER_HEIGHT * 4 * s * BEAROFF_WIDTH * s * 3 +
		  CHEQUER_LABEL_WIDTH * s * 3 + s * BEAROFF_WIDTH * s * 3, BEAROFF_WIDTH * s * 3,
		  auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		  CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	WRITE( auchOff[ 0 ], BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );

	sprintf( pchFile, "b-roff-o%d.png", i );
	CopyArea( auchOff[ 1 ] +
		  CHEQUER_LABEL_WIDTH * s * 3 + s * BEAROFF_WIDTH * s * 3, BEAROFF_WIDTH * s * 3,
		  auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s * CHEQUER_LABEL_WIDTH * s * 3,
		  CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	WRITE( auchOff[ 1 ], BEAROFF_WIDTH * 4 * 3, BEAROFF_WIDTH * s, BEAROFF_HEIGHT * s );
    }

    /* chequers on bar */
    strcpy( pchFile, "b-bar-o0.png" );
    WRITE( auchBar[ 0 ], BAR_WIDTH * s * 3, BAR_WIDTH * s, 
           ( POINT_HEIGHT - CUBE_HEIGHT ) * s );

    strcpy( pchFile, "b-bar-x0.png" );
    WRITE( auchBar[ 1 ], BAR_WIDTH * s * 3, BAR_WIDTH * s, 
           ( POINT_HEIGHT - CUBE_HEIGHT ) * s );

    for( i = 1; i <= 3; i++ ) {
	sprintf( pchFile, "b-bar-o%d.png", i );
	RefractBlend( auchBar[ 0 ] + ( BAR_WIDTH / 2 - CHEQUER_WIDTH / 2 ) * 4 * 3 + ( CHEQUER_HEIGHT + 1 ) * 4 * ( 3 - i ) *
		      BAR_WIDTH * 4 * 3 + BAR_WIDTH * 4 * 3 * 4, BAR_WIDTH * 4 * 3,
		      auchBoard + ( 12 + ( CHEQUER_HEIGHT + 1 ) * ( 3 - i ) ) * s * nStride +
		      ( BOARD_WIDTH / 2 - CHEQUER_WIDTH / 2 ) * s * 3, nStride,
		      auchChequer[ 0 ], CHEQUER_WIDTH * 4 * 4,
		      asRefract[ 0 ], CHEQUER_WIDTH * 4,
		      CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	WRITE( auchBar[ 0 ], BAR_WIDTH * 4 * 3, BAR_WIDTH * s, ( 3 * CHEQUER_HEIGHT + 4 ) * s );

	sprintf( pchFile, "b-bar-x%d.png", i );
	RefractBlend( auchBar[ 1 ] + ( BAR_WIDTH / 2 - CHEQUER_WIDTH / 2 ) * 4 * 3 + ( CHEQUER_HEIGHT + 1 ) * 4 * ( i - 1 ) *
		      BAR_WIDTH * 4 * 3 + BAR_WIDTH * 4 * 3 * 4, BAR_WIDTH * 4 * 3,
		      auchBoard + ( 40 + ( CHEQUER_HEIGHT + 1 ) * ( i - 1 ) ) * s * nStride +
		      ( BOARD_WIDTH / 2 - CHEQUER_WIDTH / 2 ) * s * 3, nStride,
		      auchChequer[ 1 ], CHEQUER_WIDTH * 4 * 4,
		      asRefract[ 1 ], CHEQUER_WIDTH * 4,
		      CHEQUER_WIDTH * 4, CHEQUER_HEIGHT * 4 );
	WRITE( auchBar[ 1 ], BAR_WIDTH * 4 * 3, BAR_WIDTH * s, ( 3 * CHEQUER_HEIGHT + 4 ) * s );
    }
    
    for( i = 4; i <= 15; i++ ) {
	sprintf( pchFile, "b-bar-o%d.png", i );
	CopyArea( auchBar[ 0 ] + 4 * s * 3 + 2 * s * BAR_WIDTH * s * 3, BAR_WIDTH * s * 3,
		  auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s *
		  CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s * 3,
		  CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	WRITE( auchBar[ 0 ], BAR_WIDTH * 4 * 3, BAR_WIDTH * s, ( 3 * CHEQUER_HEIGHT + 4 ) * s );

	sprintf( pchFile, "b-bar-x%d.png", i );
	CopyArea( auchBar[ 1 ] + 4 * s * 3 + 16 * s * BAR_WIDTH * s * 3, BAR_WIDTH * s * 3,
		  auchChequerLabels + ( i - 4 ) * CHEQUER_LABEL_HEIGHT * s *
		  CHEQUER_LABEL_WIDTH * s * 3, CHEQUER_LABEL_WIDTH * s * 3,
		  CHEQUER_LABEL_WIDTH * s, CHEQUER_LABEL_HEIGHT * s );
	WRITE( auchBar[ 1 ], BAR_WIDTH * 4 * 3, BAR_WIDTH * s, ( 3 * CHEQUER_HEIGHT + 4 ) * s );
    }

    /* cube */
    for( i = 0; i < 2; i++ ) {

      int cube_y = i ? 
        ( ( BOARD_HEIGHT - BORDER_HEIGHT ) * s - CUBE_HEIGHT *ss ) : 
        ( BORDER_HEIGHT * s );
      int offset = i ? 
        nStride * s * ( BOARD_HEIGHT - BORDER_HEIGHT - CUBE_HEIGHT ) + 
        ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3
        :
        ( s * nStride * 3 + 
          ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3 );

      AlphaBlend( auchBoard + 
                  cube_y * nStride +
                  ( BOARD_WIDTH / 2 ) * s * 3 - CUBE_WIDTH / 2  * ss * 3, 
                  nStride,
                  auchBoard +
                  cube_y * nStride +
                  ( BOARD_WIDTH / 2 ) * s * 3 - CUBE_WIDTH / 2  * ss * 3, 
                  nStride,
                  auchCube, CUBE_WIDTH * ss *4 , CUBE_WIDTH * ss, 
                  CUBE_HEIGHT * ss );

	for( j = 0; j < 12; j++ ) {

	    CopyAreaRotateClip( auchBoard, nStride, 
                                ( BOARD_WIDTH / 2 ) * s - 
                                CUBE_WIDTH / 2  * ss + ss, 
                                cube_y + ss,
				BOARD_WIDTH * s, BOARD_HEIGHT * s,
				auchCubeFaces, ss * CUBE_LABEL_WIDTH * 3,
				0, ss * CUBE_LABEL_WIDTH * j,
				ss * CUBE_LABEL_WIDTH, ss * CUBE_LABEL_HEIGHT,
				2 - 2 * i );
	    
	    sprintf( pchFile, "b-%s-%d.png", aszCube[ i ], 2 << j );
            WRITE( auchBoard + offset, nStride, 
                   BAR_WIDTH * s, CUBE_HEIGHT * s );
	    
	    if( j == 5 ) {
              sprintf( pchFile, "b-%sc-1.png", aszCube[ i ] );
              WRITE( auchBoard + offset, nStride, 
                     BAR_WIDTH * s, CUBE_HEIGHT * s );
            }
	}
    }

    /* doubles */

    for( i = 0; i < 2; i++ ) {

        int offset = 
          + 3 * s * 6 * POINT_WIDTH * ( s * BOARD_CENTER_HEIGHT / 2 - 
                                        ss * CUBE_HEIGHT / 2 )
          + 3 * s * 3 * POINT_WIDTH - 3 * ss * CUBE_WIDTH / 2;
      
	CopyArea( auchMidBoard, 
                  3 * s * 6 * POINT_WIDTH, 
                  auchBoard
                  + s * nStride * ( BOARD_HEIGHT / 2 - 
                                    BOARD_CENTER_HEIGHT / 2 )
                  + 3 * s * ( BEAROFF_WIDTH + i * ( 6 * POINT_WIDTH +
                                                    BAR_WIDTH ) ),
                  nStride, 
                  s * 6 * POINT_WIDTH, 
                  s * BOARD_CENTER_HEIGHT );

        AlphaBlend( auchMidBoard + offset,
                    3 * s * 6 * POINT_WIDTH,
                    auchMidBoard + offset,
                    3 * s * 6 * POINT_WIDTH,
		    auchCube, 
                    CUBE_WIDTH * ss * 4,
		    CUBE_WIDTH * ss, 
                    CUBE_HEIGHT * ss );
	
	for( j = 0; j < 12; j++ ) {
            CopyAreaRotateClip( auchMidBoard,
                                3 * s * 6 * POINT_WIDTH,
                                s * 3 * POINT_WIDTH - ss * CUBE_LABEL_WIDTH / 2,
				s * BOARD_CENTER_HEIGHT / 2 - 
                                ss * CUBE_LABEL_HEIGHT / 2, 
                                s * 6 * POINT_WIDTH,
                                s * BOARD_CENTER_HEIGHT,
				auchCubeFaces, 
                                ss * CUBE_LABEL_WIDTH * 3,
				0, 
                                ss * CUBE_LABEL_WIDTH * j,
				ss * CUBE_LABEL_WIDTH, 
                                ss * CUBE_LABEL_HEIGHT,
				i << 1 ); 

	    sprintf( pchFile, "b-mid%c-c%d.png", i ? 'r' : 'l', 2 << j );
	    WRITE( auchMidBoard, 
                   3 * s * 6 * POINT_WIDTH,
                   s * 6 * POINT_WIDTH, 
                   s * BOARD_CENTER_HEIGHT );
	}
    }

    /* center cube */

    AlphaBlend( auchBoard + 
                ( s * BOARD_HEIGHT / 2  - ss * CUBE_HEIGHT / 2 ) * nStride +
                ( BOARD_WIDTH / 2 ) * s * 3 - CUBE_WIDTH / 2  * ss * 3, 
                nStride,
                auchBoard +
                ( s * BOARD_HEIGHT / 2  - ss * CUBE_HEIGHT / 2 ) * nStride +
                ( BOARD_WIDTH / 2 ) * s * 3 - CUBE_WIDTH / 2  * ss * 3, 
                nStride,
                auchCube, CUBE_WIDTH * ss *4 , CUBE_WIDTH * ss, 
                CUBE_HEIGHT * ss );

    for( j = 0; j < 12; j++ ) {

      CopyAreaRotateClip( auchBoard, nStride, 
                          ( BOARD_WIDTH / 2 ) * s - CUBE_WIDTH / 2  * ss + ss, 
                          s * BOARD_HEIGHT / 2  - 
                          ss * CUBE_HEIGHT / 2 + ss,
                          BOARD_WIDTH * s, BOARD_HEIGHT * s,
                          auchCubeFaces, ss * CUBE_LABEL_WIDTH * 3,
                          0, ss * CUBE_LABEL_WIDTH * j,
                          ss * CUBE_LABEL_WIDTH, ss * CUBE_LABEL_HEIGHT,
                          1 );

      sprintf( pchFile, "b-midc-%d.png", 2 << j );
      WRITE( auchBoard + 
             ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) * nStride * s + 
             ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3,
             nStride, BAR_WIDTH * s, BOARD_CENTER_HEIGHT * s );
      
      if( j == 5 ) {
        /* 64 cube is also the cube for 1 */
        strcpy( pchFile, "b-midc-1.png" );
        WRITE( auchBoard + 
               ( BOARD_HEIGHT / 2 - BOARD_CENTER_HEIGHT / 2 ) * nStride * s + 
               ( BOARD_WIDTH / 2 - BAR_WIDTH / 2 ) * s * 3,
               nStride, BAR_WIDTH * s, BOARD_CENTER_HEIGHT * s );
      }
    }


    /* dice rolls */
    for( i = 0; i < 2; i++ ) {

      int dice_x = 3 * s * ( BEAROFF_WIDTH + 
                             3 * POINT_WIDTH + 
                             ( 6 * POINT_WIDTH + BAR_WIDTH ) * i )
        - 3 * ss * DIE_WIDTH / 2;

      int dice_y = ( BOARD_HEIGHT / 2 * s - DIE_HEIGHT / 2 * ss ) * nStride;

	AlphaBlend( auchBoard + 
                    dice_x - 3 * ss * DIE_WIDTH +
                    dice_y,
		    nStride, 
                    auchBoard +
                    dice_x - 3 * ss * DIE_WIDTH +
                    dice_y,
		    nStride,
		    auchDice[ i ], 
                    DIE_WIDTH * ss * 4,
		    DIE_WIDTH * ss, 
                    DIE_HEIGHT * ss );

	AlphaBlend( auchBoard + 
                    dice_x + 3 * ss * DIE_WIDTH +
                    dice_y,
                    nStride, 
                    auchBoard +
                    dice_x + 3 * ss * DIE_WIDTH +
                    dice_y,
                    nStride, 
                    auchDice[ i ], 
                    DIE_WIDTH * ss * 4,
		    DIE_WIDTH * ss, 
                    DIE_HEIGHT * ss );

	for( j = 0; j < 6; j++ )
	    for( k = 0; k < 6; k++ ) {
		CopyArea( auchMidBoard, 
                          6 * POINT_WIDTH * s * 3, 
                          auchBoard +
                          nStride * s * ( BORDER_HEIGHT + POINT_HEIGHT ) +
                          3 * s * ( BEAROFF_WIDTH + 
                                    i * ( 6 * POINT_WIDTH + BAR_WIDTH ) ),
			  nStride,
                          s * 6 * POINT_WIDTH, s * BOARD_CENTER_HEIGHT );

		DrawPips( auchMidBoard
                          + 3 * s * 3 * POINT_WIDTH 
                          - 3 * ss * DIE_WIDTH / 2
                          - 3 * ss * DIE_WIDTH
                          + 3 * 6 * POINT_WIDTH * s * ( BOARD_CENTER_HEIGHT / 2 * s - DIE_HEIGHT / 2 * ss )
                          ,
			  6 * POINT_WIDTH * s * 3, 
                          auchPips[ i ], 
                          ss * 3, 
                          ss, 
                          j + 1 );
		DrawPips( auchMidBoard + 
                          + 3 * s * 3 * POINT_WIDTH 
                          - 3 * ss * DIE_WIDTH / 2
                          + 3 * ss * DIE_WIDTH
                          + 3 * 6 * POINT_WIDTH * s * ( BOARD_CENTER_HEIGHT / 2 * s - DIE_HEIGHT / 2 * ss )
                          ,
			  6 * POINT_WIDTH * s * 3, 
                          auchPips[ i ], 
                          ss * 3, 
                          ss, 
                          k + 1 );
		
		sprintf( pchFile, "b-mid%c-%c%d%d.png", i ? 'r' : 'l',
			 i ? 'o' : 'x', j + 1, k + 1 );
		WRITE( auchMidBoard, 
                       BOARD_CENTER_WIDTH * s * 3, 
                       BOARD_CENTER_WIDTH * s, BOARD_CENTER_HEIGHT * s );
	    }
    }

#if HAVE_LIBART
    for ( i = 0; i < 2; ++i )
      art_free( auchArrow[ i ] );
#endif /* HAVE_LIBART */

    ProgressEnd ();
    
#else
    outputl( _("This installation of GNU Backgammon was compiled without\n"
	     "support for writing HTML images.") );
#endif
}

#if 0
      sprintf( pchFile, "debug-%d.png", xx++ );
      WRITE( auchBoard, nStride, BOARD_WIDTH * s, BOARD_HEIGHT * s );
#endif      
