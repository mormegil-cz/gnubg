/*
 * renderprefs.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000, 2001, 2002, 2003.
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
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#if HAVE_STRING_H
#include <string.h>
#endif

#include "backgammon.h"
#include "i18n.h"
#include "render.h"
#include "renderprefs.h"

char *aszWoodName[] = {
    "alder", "ash", "basswood", "beech", "cedar", "ebony", "fir", "maple",
    "oak", "pine", "redwood", "walnut", "willow", "paint"
};

renderdata rdAppearance;

static char HexDigit( char ch ) {

    ch = toupper( ch );
    
    if( ch >= '0' && ch <= '9' )
	return ch - '0';
    else if( ch >= 'A' && ch <= 'F' )
	return ch - 'A' + 0xA;
    else
	return 0;
}

static int SetColour( char *sz, unsigned char anColour[] ) {

    int i;
    
    if( strlen( sz ) < 7 || *sz != '#' )
	return -1;

    sz++;

    for( i = 0; i < 3; i++ ) {
	anColour[ i ] = ( HexDigit( sz[ 0 ] ) << 4 ) |
	    HexDigit( sz[ 1 ] );
	sz += 2;
    }

    return 0;
}

static int SetColourSpeckle( char *sz, unsigned char anColour[],
			     int *pnSpeckle ) {
    
    char *pch;
    
    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( !SetColour( sz, anColour ) ) {
	if( pch ) {
            PushLocale ( "C" );
	    *pnSpeckle = atof( pch ) * 128;
            PopLocale ();
	    
	    if( *pnSpeckle < 0 )
		*pnSpeckle = 0;
	    else if( *pnSpeckle > 128 )
		*pnSpeckle = 128;
	}

	return 0;
    }

    return -1;
}

/* Set colour (with floats) */
static int SetColourX( gdouble arColour[ 4 ], char *sz ) {

    char *pch;
    unsigned char anColour[ 3 ];
    
    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( !SetColour( sz, anColour ) ) {
	arColour[ 0 ] = anColour[ 0 ] / 255.0f;
	arColour[ 1 ] = anColour[ 1 ] / 255.0f;
	arColour[ 2 ] = anColour[ 2 ] / 255.0f;
	return 0;
    }

    return -1;
}

/* Set colour, alpha, refraction, shine, specular. */
static int SetColourARSS( double aarColour[ 2 ][ 4 ], 
                          float arRefraction[ 2 ],
                          float arCoefficient[ 2 ],
                          float arExponent[ 2 ],
                          char *sz, int i ) {

    char *pch;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( !SetColourX( aarColour[ i ], sz ) ) {
        PushLocale ( "C" );

	if( pch ) {
	    /* alpha */
	    aarColour[ i ][ 3 ] = atof( pch );

	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    aarColour[ i ][ 3 ] = 1.0f; /* opaque */

	if( pch ) {
	    /* refraction */
	    arRefraction[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arRefraction[ i ] = 1.5f;

	if( pch ) {
	    /* shine */
	    arCoefficient[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arCoefficient[ i ] = 0.5f;

	if( pch ) {
	    /* specular */
	    arExponent[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arExponent[ i ] = 10.0f;	

        PopLocale ();

	return 0;
    }

    return -1;
}

/* Set colour, shine, specular, flag. */
static int SetColourSSF( gdouble aarColour[ 2 ][ 4 ], 
                         gfloat arCoefficient[ 2 ],
                         gfloat arExponent[ 2 ],
                         int afDieColour[ 2 ],
                         char *sz, int i ) {

    char *pch;

    if( ( pch = strchr( sz, ';' ) ) )
	*pch++ = 0;

    if( !SetColourX( aarColour[ i ], sz ) ) {
        PushLocale ( "C" );

	if( pch ) {
	    /* shine */
	    arCoefficient[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arCoefficient[ i ] = 0.5f;

	if( pch ) {
	    /* specular */
	    arExponent[ i ] = atof( pch );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
	    arExponent[ i ] = 10.0f;	

	if( pch ) {
            /* die colour same as chequer colour */
            afDieColour[ i ] = ( toupper ( *pch ) == 'Y' );
	    
	    if( ( pch = strchr( pch, ';' ) ) )
		*pch++ = 0;
	} else
            afDieColour[ i ] = TRUE;

        PopLocale ();

	return 0;
    }

    return -1;
}

static int SetWood( char *sz, woodtype *pbw ) {

    woodtype bw;
    int cch = strlen( sz );
    
    for( bw = 0; bw <= WOOD_PAINT; bw++ )
	if( !strncasecmp( sz, aszWoodName[ bw ], cch ) ) {
	    *pbw = bw;
	    return 0;
	}

    return -1;
}

extern void RenderPreferencesParam( renderdata *prd, char *szParam,
				    char *szValue ) {

    int c, fValueError = FALSE;
    
    if( !szParam || !*szParam )
	return;

    if( !szValue )
	szValue = "";
    
    c = strlen( szParam );
    
    if( !strncasecmp( szParam, "board", c ) )
	/* board=colour;speckle */
	fValueError = SetColourSpeckle( szValue, prd->aanBoardColour[ 0 ],
				        &prd->aSpeckle[ 0 ] );
    else if( !strncasecmp( szParam, "border", c ) )
	/* border=colour */
	fValueError = SetColour( szValue, prd->aanBoardColour[ 1 ] );
    else if( !strncasecmp( szParam, "cube", c ) )
	/* cube=colour */
        fValueError = SetColourX( prd->arCubeColour, szValue );
    else if( !strncasecmp( szParam, "translucent", c ) )
	/* deprecated option "translucent"; ignore */
	;
    else if( !strncasecmp( szParam, "labels", c ) )
	/* labels=bool */
	prd->fLabels = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "diceicon", c ) )
	/* FIXME deprecated in favour of "set gui dicearea" */
	fGUIDiceArea = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "show_ids", c ) )
	/* FIXME deprecated in favour of "set gui showids" */
	fGUIShowIDs = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "show_pips", c ) )
	/* FIXME deprecated in favour of "set gui showpips" */
	fGUIShowPips = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "illegal", c ) )
	/* FIXME deprecated in favour of "set gui illegal" */
	fGUIIllegal = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "beep", c ) )
	/* FIXME deprecated in favour of "set gui beep" */
	fGUIBeep = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "highdie", c ) )
	/* FIXME deprecated in favour of "set gui highdie" */
	fGUIHighDieFirst = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "wood", c ) )
	fValueError = SetWood( szValue, &prd->wt );
    else if( !strncasecmp( szParam, "hinges", c ) )
	prd->fHinges = toupper( *szValue ) == 'Y';
    else if( !strncasecmp( szParam, "animate", c ) ) {
	/* FIXME deprecated in favour of "set gui animation ..." */
	switch( toupper( *szValue ) ) {
	case 'B':
	    animGUI = ANIMATE_BLINK;
	    break;
	case 'S':
	    animGUI = ANIMATE_SLIDE;
	    break;
	default:
	    animGUI = ANIMATE_NONE;
	    break;
	}
    } else if( !strncasecmp( szParam, "speed", c ) ) {
	/* FIXME deprecated in favour of "set gui animation speed" */
	int n= atoi( szValue );

	if( n < 0 || n > 7 )
	    fValueError = TRUE;
	else
	    nGUIAnimSpeed = n;
    } else if( !strncasecmp( szParam, "light", c ) ) {
	/* light=azimuth;elevation */
	float rAzimuth, rElevation;

	PushLocale ( "C" );
	if( sscanf( szValue, "%f;%f", &rAzimuth, &rElevation ) < 2 )
	    fValueError = TRUE;
	else {
	    if( rElevation < 0.0f )
		rElevation = 0.0f;
	    else if( rElevation > 90.0f )
		rElevation = 90.0f;
	    
	    prd->arLight[ 2 ] = sinf( rElevation / 180 * M_PI );
	    prd->arLight[ 0 ] = cosf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
	    prd->arLight[ 1 ] = sinf( rAzimuth / 180 * M_PI ) *
		sqrt( 1.0 - prd->arLight[ 2 ] * prd->arLight[ 2 ] );
	}
        PopLocale ();
    } else if( !strncasecmp( szParam, "shape", c ) ) {
	float rRound;

	PushLocale ( "C" );
	if( sscanf( szValue, "%f", &rRound ) < 1 )
	    fValueError = TRUE;
	else
	    prd->rRound = 1.0 - rRound;
        PopLocale ();
    } else if( c > 1 &&
	       ( !strncasecmp( szParam, "chequers", c - 1 ) ||
		 !strncasecmp( szParam, "checkers", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* chequers=colour;alpha;refrac;shine;spec */
	fValueError = SetColourARSS( prd->aarColour, 
                                     prd->arRefraction,
                                     prd->arCoefficient,
                                     prd->arExponent,
                                     szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !strncasecmp( szParam, "dice", c - 1 ) ||
               !strncasecmp( szParam, "dice", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* dice=colour;shine;spec;flag */
        fValueError = SetColourSSF( prd->aarDiceColour, 
                                    prd->arDiceCoefficient,
                                    prd->arDiceExponent,
                                    prd->afDieColour,
                                    szValue, szParam[ c - 1 ] - '0' );
    else if( c > 1 &&
             ( !strncasecmp( szParam, "dot", c - 1 ) ||
               !strncasecmp( szParam, "dot", c - 1 ) ) &&
	       ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
        /* dot=colour */
	fValueError = 
          SetColourX( prd->aarDiceDotColour [ szParam[ c - 1 ] - '0' ],
                      szValue );
    else if( c > 1 && !strncasecmp( szParam, "points", c - 1 ) &&
	     ( szParam[ c - 1 ] == '0' || szParam[ c - 1 ] == '1' ) )
	/* pointsn=colour;speckle */
	fValueError = SetColourSpeckle( szValue,
					prd->aanBoardColour[
					    szParam[ c - 1 ] - '0' + 2 ],
					&prd->aSpeckle[
					    szParam[ c - 1 ] - '0' + 2 ] );
    else
	outputf( _("Unknown setting `%s'.\n"), szParam );

    if( fValueError )
	outputf( _("`%s' is not a legal value for parameter `%s'.\n"), szValue,
		 szParam );

}

extern char *RenderPreferencesCommand( renderdata *prd, char *sz ) {

    float rAzimuth, rElevation;
    rElevation = asinf( prd->arLight[ 2 ] ) * 180 / M_PI;
    rAzimuth = ( fabs ( prd->arLight[ 2 ] - 1.0f ) < 1e-5 ) ? 0.0f : 
      acosf( prd->arLight[ 0 ] / sqrt( 1.0 - prd->arLight[ 2 ] *
                                      prd->arLight[ 2 ] ) ) * 180 / M_PI;

    if( prd->arLight[ 1 ] < 0 )
	rAzimuth = 360 - rAzimuth;

    PushLocale ( "C" );
    
    sprintf( sz, 
             "set appearance board=#%02X%02X%02X;%0.2f "
	     "border=#%02X%02X%02X "
	     "labels=%c wood=%s hinges=%c "
	     "light=%0.0f;%0.0f shape=%0.1f " 
	     "chequers0=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "chequers1=#%02X%02X%02X;%0.2f;%0.2f;%0.2f;%0.2f "
	     "dice0=#%02X%02X%02X;%0.2f;%0.2f;%c "
	     "dice1=#%02X%02X%02X;%0.2f;%0.2f;%c "
	     "dot0=#%02X%02X%02X "
	     "dot1=#%02X%02X%02X "
	     "cube=#%02X%02X%02X "
	     "points0=#%02X%02X%02X;%0.2f "
	     "points1=#%02X%02X%02X;%0.2f",
             /* board */
	     prd->aanBoardColour[ 0 ][ 0 ], prd->aanBoardColour[ 0 ][ 1 ], 
	     prd->aanBoardColour[ 0 ][ 2 ], prd->aSpeckle[ 0 ] / 128.0f,
             /* border */
	     prd->aanBoardColour[ 1 ][ 0 ], prd->aanBoardColour[ 1 ][ 1 ], 
	     prd->aanBoardColour[ 1 ][ 2 ],
             /* labels ... */
             prd->fLabels ? 'y' : 'n',
	     aszWoodName[ prd->wt ],
	     prd->fHinges ? 'y' : 'n',
	     rAzimuth, rElevation, 1.0 - prd->rRound,
             /* chequers0 */
             (int) ( prd->aarColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarColour[ 0 ][ 2 ] * 0xFF ), 
             prd->aarColour[ 0 ][ 3 ], prd->arRefraction[ 0 ], 
             prd->arCoefficient[ 0 ], prd->arExponent[ 0 ],
             /* chequers1 */
	     (int) ( prd->aarColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarColour[ 1 ][ 2 ] * 0xFF ), 
             prd->aarColour[ 1 ][ 3 ], prd->arRefraction[ 1 ], 
             prd->arCoefficient[ 1 ], prd->arExponent[ 1 ],
             /* dice0 */
             (int) ( prd->aarDiceColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 0 ][ 2 ] * 0xFF ), 
             prd->arDiceCoefficient[ 0 ], prd->arDiceExponent[ 0 ],
             prd->afDieColour[ 0 ] ? 'y' : 'n',
             /* dice1 */
	     (int) ( prd->aarDiceColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceColour[ 1 ][ 2 ] * 0xFF ), 
             prd->arDiceCoefficient[ 1 ], prd->arDiceExponent[ 1 ],
             prd->afDieColour[ 1 ] ? 'y' : 'n',
             /* dot0 */
	     (int) ( prd->aarDiceDotColour[ 0 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceDotColour[ 0 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceDotColour[ 0 ][ 2 ] * 0xFF ), 
             /* dot1 */
	     (int) ( prd->aarDiceDotColour[ 1 ][ 0 ] * 0xFF ),
	     (int) ( prd->aarDiceDotColour[ 1 ][ 1 ] * 0xFF ), 
	     (int) ( prd->aarDiceDotColour[ 1 ][ 2 ] * 0xFF ), 
             /* cube */
	     (int) ( prd->arCubeColour[ 0 ] * 0xFF ),
	     (int) ( prd->arCubeColour[ 1 ] * 0xFF ), 
	     (int) ( prd->arCubeColour[ 2 ] * 0xFF ), 
             /* points0 */
	     prd->aanBoardColour[ 2 ][ 0 ], prd->aanBoardColour[ 2 ][ 1 ], 
	     prd->aanBoardColour[ 2 ][ 2 ], prd->aSpeckle[ 2 ] / 128.0f,
             /* points1 */
	     prd->aanBoardColour[ 3 ][ 0 ], prd->aanBoardColour[ 3 ][ 1 ], 
	     prd->aanBoardColour[ 3 ][ 2 ], prd->aSpeckle[ 3 ] / 128.0f );

    PopLocale ();

    return sz;
}

