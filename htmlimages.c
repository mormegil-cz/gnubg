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

#include <errno.h>
#if USE_GTK
#include <gtk/gtk.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <string.h>
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include "backgammon.h"
#include "export.h"
#if USE_GTK
#include "gtkboard.h"
#include "gtkgame.h"
#endif 
#include "i18n.h"
#include "render.h"

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
		      
extern void CommandExportHTMLImages( char *sz ) {

#if HAVE_LIBPNG

#if USE_GTK
    BoardData *bd;
#endif
#if HAVE_ALLOCA
    char *szFile;
#else
    char szFile[ 4096 ];
#endif
    char *pchFile;
    renderdata rd;
    int nStride = 108 * 4 * 3, s = 4, ss = s * 3 / 4, i, j, k;
    /* indices are: g/r, d/u, x/o */
    unsigned char auchPoint[ 2 ][ 2 ][ 2 ][ 6 * 4 * 3 * 30 * 4 ],
	auchOff[ 2 ][ 12 * 4 * 3 * 30 * 4 ],
	auchBar[ 2 ][ 12 * 4 * 3 * 22 * 4 ],
	auchBoard[ 108 * 4 * 72 * 4 * 3 ],
	auchChequer[ 2 ][ 6 * 4 * 6 * 4 * 4 ],
	auchChequerLabels[ 12 * 4 * 4 * 4 * 4 * 3 ],
	auchCube[ 8 * 3 * 8 * 3 * 4 ],
	auchCubeFaces[ 12 * 6 * 3 * 6 * 3 * 3 ],
	auchDice[ 2 ][ 7 * 3 * 7 * 3 * 4 ],
	auchPips[ 2 ][ 3 * 3 * 3 ],
	auchMidBoard[ 36 * 4 * 3 * 6 * 4 ];
    unsigned short asRefract[ 2 ][ 6 * 4 * 6 * 4 ];
    static char *aszCube[ 3 ] = { "ct", "midc", "cb" };
	
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

    ProgressStartValue( _("Generating image:"), 332 );
    
#if HAVE_ALLOCA
    szFile = alloca( strlen( sz ) + 32 );
#endif
    strcpy( szFile, sz );
    if( szFile[ strlen( szFile ) - 1 ] != '/' )
	strcat( szFile, "/" );
    pchFile = strchr( szFile, 0 );

#if USE_GTK
    if( fX ) {
	bd = BOARD ( pwBoard )->board_data;

	memcpy ( &rd, &bd->rd, sizeof ( renderdata ) );
    } else
#endif
	memcpy( &rd, &rdDefault, sizeof( renderdata ) );
    
    rd.fLabels = FALSE; /* HTML export draws labels outside the image */
    rd.nSize = s;

    RenderBoard( &rd, auchBoard, 108 * 4 * 3 );
    RenderChequers( &rd, auchChequer[ 0 ], auchChequer[ 1 ], asRefract[ 0 ],
		    asRefract[ 1 ], 6 * 4 * 4 );
    RenderChequerLabels( &rd, auchChequerLabels, 4 * 4 * 3 );

    rd.nSize = ss;

    RenderCube( &rd, auchCube, 8 * ss * 4 );
    RenderCubeFaces( &rd, auchCubeFaces, 6 * ss * 3, auchCube, 8 * ss * 4 );
    RenderDice( &rd, auchDice[ 0 ], auchDice[ 1 ], 7 * ss * 4 );
    RenderPips( &rd, auchPips[ 0 ], auchPips[ 1 ], ss * 3 );

#define WRITE( img, stride, cx, cy ) \
    if( WritePNG( szFile, (img), (stride), (cx), (cy) ) ) { \
	ProgressEnd(); \
	return; \
    } else \
        ProgressValueAdd( 1 )

    /* top border */
    strcpy( pchFile, "b-hitop.png" );
    WRITE( auchBoard, nStride, 108 * s, 3 * s );
    
    strcpy( pchFile, "b-lotop.png" );
    WRITE( auchBoard, nStride, 108 * s, 3 * s );

    /* empty points */
    strcpy( pchFile, "b-gd.png" );
    WRITE( auchBoard + s * nStride * 3 + 12 * s * 3, nStride,
	   6 * s, 30 * s );
    
    strcpy( pchFile, "b-rd.png" );
    WRITE( auchBoard + s * nStride * 3 + 18 * s * 3, nStride,
	   6 * s, 30 * s );
    
    strcpy( pchFile, "b-ru.png" );
    WRITE( auchBoard + s * nStride * 39 + 12 * s * 3, nStride,
	   6 * s, 30 * s );
    
    strcpy( pchFile, "b-gu.png" );
    WRITE( auchBoard + s * nStride * 39 + 18 * s * 3, nStride,
	   6 * s, 30 * s );

    /* bearoff tray */
    strcpy( pchFile, "b-roff.png" );
    WRITE( auchBoard + s * nStride * 3, nStride, 12 * s, 30 * s );
    
    /* bar top */
    strcpy( pchFile, "b-ct.png" );
    WRITE( auchBoard + s * nStride * 3 + 48 * s * 3, nStride,
	   12 * s, 8 * s );

    /* bearoff tray dividers */
    strcpy( pchFile, "b-midlb.png" );
    WRITE( auchBoard + s * nStride * 33, nStride, 12 * s, 6 * s );

    /* left board centre */
    strcpy( pchFile, "b-midl.png" );
    WRITE( auchBoard + s * nStride * 33 + s * 12 * 3, nStride,
	   36 * s, 6 * s );

    /* bar centre */
    strcpy( pchFile, "b-midc.png" );
    WRITE( auchBoard + s * nStride * 33 + s * 48 * 3, nStride,
	   12 * s, 6 * s );

    /* right board centre */
    strcpy( pchFile, "b-midr.png" );
    WRITE( auchBoard + s * nStride * 33 + s * 60 * 3, nStride,
	   36 * s, 6 * s );

    /* bar bottom */
    strcpy( pchFile, "b-cb.png" );
    WRITE( auchBoard + s * nStride * 61 + 48 * s * 3, nStride,
	   12 * s, 8 * s );

    /* bottom border */
    strcpy( pchFile, "b-hibot.png" );
    WRITE( auchBoard + s * nStride * 69, nStride, 108 * s,
	   3 * s );
    
    strcpy( pchFile, "b-lobot.png" );
    WRITE( auchBoard + s * nStride * 69, nStride, 108 * s,
	   3 * s );

    CopyArea( auchPoint[ 0 ][ 0 ][ 0 ], 6 * 4 * 3, auchBoard +
	      3 * s * nStride + 12 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 0 ][ 0 ][ 1 ], 6 * 4 * 3, auchBoard +
	      3 * s * nStride + 12 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 0 ][ 1 ][ 0 ], 6 * 4 * 3, auchBoard +
	      39 * s * nStride + 18 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 0 ][ 1 ][ 1 ], 6 * 4 * 3, auchBoard +
	      39 * s * nStride + 18 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 1 ][ 0 ][ 0 ], 6 * 4 * 3, auchBoard +
	      3 * s * nStride + 18 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 1 ][ 0 ][ 1 ], 6 * 4 * 3, auchBoard +
	      3 * s * nStride + 18 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 1 ][ 1 ][ 0 ], 6 * 4 * 3, auchBoard +
	      39 * s * nStride + 12 * s * 3, nStride, 6 * s, 30 * s );
    CopyArea( auchPoint[ 1 ][ 1 ][ 1 ], 6 * 4 * 3, auchBoard +
	      39 * s * nStride + 12 * s * 3, nStride, 6 * s, 30 * s );

    CopyArea( auchOff[ 0 ], 12 * 4 * 3, auchBoard +
	      3 * s * nStride + 96 * s * 3, nStride, 12 * s, 30 * s );
    CopyArea( auchOff[ 1 ], 12 * 4 * 3,
	      auchBoard + 39 * s * nStride + 96 * s * 3, nStride, 12 * s,
	      30 * s );
    
    CopyArea( auchBar[ 0 ], 12 * 4 * 3,
	      auchBoard + 11 * s * nStride + 48 * s * 3, nStride,
	      12 * s, 22 * s );
    CopyArea( auchBar[ 1 ], 12 * 4 * 3,
	      auchBoard + 39 * s * nStride + 48 * s * 3, nStride,
	      12 * s, 22 * s );
    
    for( i = 1; i <= 5; i++ ) {
	for( j = 0; j < 2; j++ ) {
	    /* gd */
	    sprintf( pchFile, "b-gd-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 0 ][ 0 ][ j ] + 6 * 4 * ( i - 1 ) *
			  6 * 4 * 3, 6 * 4 * 3,
			  auchBoard + ( 3 + 6 * ( i - 1 ) ) * s * nStride +
			  12 * s * 3, nStride,
			  auchChequer[ j ], 6 * 4 * 4,
			  asRefract[ j ], 6 * 4, 6 * 4, 6 * 4 );
	    WRITE( auchPoint[ 0 ][ 0 ][ j ], 6 * 4 * 3,
		   6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* rd */
	    sprintf( pchFile, "b-rd-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 1 ][ 0 ][ j ] + 6 * 4 * ( i - 1 ) *
			  6 * 4 * 3, 6 * 4 * 3,
			  auchBoard + ( 3 + 6 * ( i - 1 ) ) * s * nStride +
			  18 * s * 3, nStride,
			  auchChequer[ j ], 6 * 4 * 4,
			  asRefract[ j ], 6 * 4, 6 * 4, 6 * 4 );
	    WRITE( auchPoint[ 1 ][ 0 ][ j ], 6 * 4 * 3,
		   6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* gu */
	    sprintf( pchFile, "b-gu-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 0 ][ 1 ][ j ] + 6 * 4 * ( 5 - i ) *
			  6 * 4 * 3, 6 * 4 * 3,
			  auchBoard + ( 39 + 6 * ( 5 - i ) ) * s * nStride +
			  18 * s * 3, nStride,
			  auchChequer[ j ], 6 * 4 * 4,
			  asRefract[ j ], 6 * 4, 6 * 4, 6 * 4 );
	    WRITE( auchPoint[ 0 ][ 1 ][ j ], 6 * 4 * 3,
		   6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* ru */
	    sprintf( pchFile, "b-ru-%c%d.png", j ? 'o' : 'x', i );
	    RefractBlend( auchPoint[ 1 ][ 1 ][ j ] + 6 * 4 * ( 5 - i ) *
			  6 * 4 * 3, 6 * 4 * 3,
			  auchBoard + ( 39 + 6 * ( 5 - i ) ) * s * nStride +
			  12 * s * 3, nStride,
			  auchChequer[ j ], 6 * 4 * 4,
			  asRefract[ j ], 6 * 4, 6 * 4, 6 * 4 );
	    WRITE( auchPoint[ 1 ][ 1 ][ j ], 6 * 4 * 3,
		   6 * s, 30 * s );
	}
	
	/* off */
	sprintf( pchFile, "b-roff-x%d.png", i );
	RefractBlend( auchOff[ 0 ] + 3 * 4 * 3 + 6 * 4 * ( i - 1 ) *
		      12 * 4 * 3, 12 * 4 * 3,
		      auchBoard + ( 3 + 6 * ( i - 1 ) ) * s * nStride +
		      99 * s * 3, nStride,
		      auchChequer[ 0 ], 6 * 4 * 4,
		      asRefract[ 0 ], 6 * 4, 6 * 4, 6 * 4 );
	WRITE( auchOff[ 0 ], 12 * 4 * 3, 12 * s, 30 * s );

	sprintf( pchFile, "b-roff-o%d.png", i );
	RefractBlend( auchOff[ 1 ] + 3 * 4 * 3 + 6 * 4 * ( 5 - i ) *
		      12 * 4 * 3, 12 * 4 * 3,
		      auchBoard + ( 39 + 6 * ( 5 - i ) ) * s * nStride +
		      99 * s * 3, nStride,
		      auchChequer[ 1 ], 6 * 4 * 4,
		      asRefract[ 1 ], 6 * 4, 6 * 4, 6 * 4 );
	WRITE( auchOff[ 1 ], 12 * 4 * 3, 12 * s, 30 * s );
    }

    for( i = 6; i <= 15; i++ ) {
	for( j = 0; j < 2; j++ ) {
	    /* gd */
	    sprintf( pchFile, "b-gd-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 0 ][ 0 ][ j ] + 6 * 4 * s * 6 * s * 3 +
		      s * 3 + s * 6 * s * 3, 6 * s * 3,
		      auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		      4 * s * 3, 4 * s, 4 * s );
	    WRITE( auchPoint[ 0 ][ 0 ][ j ], 6 * 4 * 3, 6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* rd */
	    sprintf( pchFile, "b-rd-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 1 ][ 0 ][ j ] + 6 * 4 * s * 6 * s * 3 +
		      s * 3 + s * 6 * s * 3, 6 * s * 3,
		      auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		      4 * s * 3, 4 * s, 4 * s );
	    WRITE( auchPoint[ 1 ][ 0 ][ j ], 6 * 4 * 3, 6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* gu */
	    sprintf( pchFile, "b-gu-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 0 ][ 1 ][ j ] +
		      s * 3 + s * 6 * s * 3, 6 * s * 3,
		      auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		      4 * s * 3, 4 * s, 4 * s );
	    WRITE( auchPoint[ 0 ][ 1 ][ j ], 6 * 4 * 3, 6 * s, 30 * s );
	}
	
	for( j = 0; j < 2; j++ ) {
	    /* ru */
	    sprintf( pchFile, "b-ru-%c%d.png", j ? 'o' : 'x', i );
	    CopyArea( auchPoint[ 1 ][ 1 ][ j ] +
		      s * 3 + s * 6 * s * 3, 6 * s * 3,
		      auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		      4 * s * 3, 4 * s, 4 * s );
	    WRITE( auchPoint[ 1 ][ 1 ][ j ], 6 * 4 * 3, 6 * s, 30 * s );
	}
	
	/* off */
	sprintf( pchFile, "b-roff-x%d.png", i );
	CopyArea( auchOff[ 0 ] + 6 * 4 * s * 12 * s * 3 +
		  4 * s * 3 + s * 12 * s * 3, 12 * s * 3,
		  auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		  4 * s * 3, 4 * s, 4 * s );
	WRITE( auchOff[ 0 ], 12 * 4 * 3, 12 * s, 30 * s );

	sprintf( pchFile, "b-roff-o%d.png", i );
	CopyArea( auchOff[ 1 ] +
		  4 * s * 3 + s * 12 * s * 3, 12 * s * 3,
		  auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		  4 * s * 3, 4 * s, 4 * s );
	WRITE( auchOff[ 1 ], 12 * 4 * 3, 12 * s, 30 * s );
    }

    /* chequers on bar */
    strcpy( pchFile, "b-bar-o0.png" );
    WRITE( auchBar[ 0 ], 12 * s * 3, 12 * s, 22 * s );
    
    strcpy( pchFile, "b-bar-x0.png" );
    WRITE( auchBar[ 1 ], 12 * s * 3, 12 * s, 22 * s );
    
    for( i = 1; i <= 3; i++ ) {
	sprintf( pchFile, "b-bar-o%d.png", i );
	RefractBlend( auchBar[ 0 ] + 3 * 4 * 3 + 7 * 4 * ( 3 - i ) *
		      12 * 4 * 3 + 12 * 4 * 3 * 4, 12 * 4 * 3,
		      auchBoard + ( 12 + 7 * ( 3 - i ) ) * s * nStride +
		      51 * s * 3, nStride,
		      auchChequer[ 0 ], 6 * 4 * 4,
		      asRefract[ 0 ], 6 * 4, 6 * 4, 6 * 4 );
	WRITE( auchBar[ 0 ], 12 * 4 * 3, 12 * s, 22 * s );

	sprintf( pchFile, "b-bar-x%d.png", i );
	RefractBlend( auchBar[ 1 ] + 3 * 4 * 3 + 7 * 4 * ( i - 1 ) *
		      12 * 4 * 3 + 12 * 4 * 3 * 4, 12 * 4 * 3,
		      auchBoard + ( 40 + 7 * ( i - 1 ) ) * s * nStride +
		      51 * s * 3, nStride,
		      auchChequer[ 1 ], 6 * 4 * 4,
		      asRefract[ 1 ], 6 * 4, 6 * 4, 6 * 4 );
	WRITE( auchBar[ 1 ], 12 * 4 * 3, 12 * s, 22 * s );
    }
    
    for( i = 4; i <= 15; i++ ) {
	sprintf( pchFile, "b-bar-o%d.png", i );
	CopyArea( auchBar[ 0 ] + 4 * s * 3 + 2 * s * 12 * s * 3, 12 * s * 3,
		  auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		  4 * s * 3, 4 * s, 4 * s );
	WRITE( auchBar[ 0 ], 12 * 4 * 3, 12 * s, 22 * s );

	sprintf( pchFile, "b-bar-x%d.png", i );
	CopyArea( auchBar[ 1 ] + 4 * s * 3 + 16 * s * 12 * s * 3, 12 * s * 3,
		  auchChequerLabels + ( i - 4 ) * 4 * s * 4 * s * 3,
		  4 * s * 3, 4 * s, 4 * s );
	WRITE( auchBar[ 1 ], 12 * 4 * 3, 12 * s, 22 * s );
    }

    /* cube */
    for( i = 0; i < 3; i++ ) {
	AlphaBlend( auchBoard + 51 * s * 3 + ( 4 + 29 * i ) * s * nStride,
		    nStride, auchBoard + 51 * s * 3 + ( 4 + 29 * i ) * s *
		    nStride, nStride, auchCube, 8 * ss * 4, 8 * ss, 8 * ss );
	for( j = 0; j < 12; j++ ) {
	    CopyAreaRotateClip( auchBoard, nStride, 51 * s + ss,
				( 4 + 29 * i ) * s + ss, 108 * s, 72 * s,
				auchCubeFaces, ss * 6 * 3, 0, ss * 6 * j,
				ss * 6, ss * 6, 2 - i );
	    
	    sprintf( pchFile, "b-%s-%d.png", aszCube[ i ], 2 << j );
	    WRITE( auchBoard + 48 * s * 3 + ( 3 + 29 * i + ( i == 1 ) ) *
		   s * nStride, nStride, 12 * s, i == 1 ? 6 * s : 8 * s );
	    
	    if( j == 5 ) {
		/* 64 cube is also the cube for 1 */
		sprintf( pchFile, "b-%s-1.png", aszCube[ i ] );
		WRITE( auchBoard + 48 * s * 3 + ( 3 + 29 * i + ( i == 1 ) ) *
		       s * nStride, nStride, 12 * s, i == 1 ? 6 * s : 8 * s );
	    }
	}
    }

    for( i = 0; i < 2; i++ ) {
	CopyArea( auchMidBoard, 36 * s * 3, auchBoard + ( 12 + 48 * i ) * 3 *
		  s + 33 * s * nStride, nStride, 36 * s, 6 * s );
	AlphaBlend( auchMidBoard + ( 18 * s - 4 * ss ) * 3, 36 * s * 3,
		    auchMidBoard + ( 18 * s - 4 * ss ) * 3, 36 * s * 3,
		    auchCube, 8 * ss * 4, 8 * ss, 8 * ss );
	
	for( j = 0; j < 12; j++ ) {
	    CopyAreaRotateClip( auchMidBoard, 36 * s * 3, 18 * s - 3 * ss,
				ss, 36 * s, 6 * s, auchCubeFaces, ss * 6 * 3,
				0, ss * 6 * j, ss * 6, ss * 6, i << 1 );
	    
	    sprintf( pchFile, "b-mid%c-c%d.png", i ? 'r' : 'l', 2 << j );
	    WRITE( auchMidBoard, 36 * s * 3, 36 * s, 6 * s );
	}
    }

    /* dice rolls */
    for( i = 0; i < 2; i++ ) {
	AlphaBlend( auchBoard + ( 30 + 48 * i ) * 3 * s - 8 * ss * 3 +
		    ( 33 * s + 1 ) * nStride, nStride, auchBoard +
		    ( 30 + 48 * i ) * 3 * s - 8 * ss * 3 + ( 33 * s + 1 ) *
		    nStride, nStride, auchDice[ i ], 7 * ss * 4, 7 * ss,
		    7 * ss );
	AlphaBlend( auchBoard + ( 30 + 48 * i ) * 3 * s + ss * 3 +
		    ( 33 * s + 1 ) * nStride, nStride, auchBoard +
		    ( 30 + 48 * i ) * 3 * s + ss * 3 + ( 33 * s + 1 ) *
		    nStride, nStride, auchDice[ i ], 7 * ss * 4, 7 * ss,
		    7 * ss );
	for( j = 0; j < 6; j++ )
	    for( k = 0; k < 6; k++ ) {
		CopyArea( auchMidBoard, 36 * s * 3, auchBoard +
			  ( 12 + 48 * i ) * 3 * s + 33 * s * nStride, nStride,
			  36 * s, 6 * s );

		DrawPips( auchMidBoard + 18 * s * 3 - 8 * ss * 3 + 36 * s * 3,
			  36 * s * 3, auchPips[ i ], ss * 3, ss, j + 1 );
		DrawPips( auchMidBoard + 18 * s * 3 + ss * 3 + 36 * s * 3,
			  36 * s * 3, auchPips[ i ], ss * 3, ss, k + 1 );
		
		sprintf( pchFile, "b-mid%c-%c%d%d.png", i ? 'r' : 'l',
			 i ? 'o' : 'x', j + 1, k + 1 );
		WRITE( auchMidBoard, 36 * s * 3, 36 * s, 6 * s );
	    }
    }
    
#else
    outputl( _("This installation of GNU Backgammon was compiled without\n"
	     "support for writing HTML images.") );
#endif
}
