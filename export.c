/*
 * export.c
 *
 * by Joern Thyssen  <jthyssen@dk.ibm.com>, 2002
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#if HAVE_LIBPNG
#include <png.h>
#endif

#include "analysis.h"
#include "backgammon.h"
#include "drawboard.h"
#include "export.h"
#include "eval.h"
#include "positionid.h"
#include "render.h"
#include "renderprefs.h"
#include "matchid.h"
#include "i18n.h"

/* size of html images in steps of 108x72 */

static void
ExportGameEquityEvolution ( FILE *pf, list *plGame, 
                            const int fPlayer,
                            const float rStartEquity,
                            float *prEndEquity,
                            const int iGame ) {

  list *pl;
  moverecord *pmr;
  matchstate msEE;

  float rMoveError, rMoveEquity, rMoveLuck;
  float rEquity;
  int iMove;
  int f;
  float r;
  unsigned char auch[ 10 ];
  cubeinfo ci;
  int i;
  int fEquity = FALSE, fError = FALSE, fLuck = FALSE;

  PushLocale ( "C" );

  iMove = 0;
  
  for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {

    pmr = pl->p;

    FixMatchState ( &msEE, pmr );

    switch ( pmr->mt ) {

    case MOVE_NORMAL:

      if( pmr->n.fPlayer != msEE.fMove ) {
        SwapSides( msEE.anBoard );
        msEE.fMove = pmr->n.fPlayer;
      }
      
      msEE.fTurn = msEE.fMove = pmr->n.fPlayer;
      msEE.anDice[ 0 ] = pmr->n.anRoll[ 0 ];
      msEE.anDice[ 1 ] = pmr->n.anRoll[ 1 ];

      GetMatchStateCubeInfo ( &ci, &msEE );

      rMoveError = 0.0;
      rMoveLuck = 0.0;
      rMoveEquity = 0.0;
      f = FALSE;

      if ( pmr->n.esDouble.et != EVAL_NONE ) {

        r = pmr->n.arDouble[ OUTPUT_NODOUBLE ] - 
          pmr->n.arDouble[ OUTPUT_OPTIMAL ];

        rMoveError = msEE.nMatchTo ? 
          eq2mwc( r, &ci ) - eq2mwc( 0.0f, &ci ) : msEE.nCube * r;

        rMoveEquity = msEE.nMatchTo ? 
          eq2mwc( pmr->n.arDouble[ OUTPUT_NODOUBLE ], &ci ) : msEE.nCube * r;

        fEquity = TRUE;
        
      }

      if ( pmr->n.esChequer.et != EVAL_NONE ) {

        int anBoardMove[ 2 ][ 25 ];

        /* find skill */

        memcpy( anBoardMove, msEE.anBoard, sizeof( anBoardMove ) );
        ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
        PositionKey ( anBoardMove, auch );
        r = 0.0f;
        
        for( i = 0; i < pmr->n.ml.cMoves; i++ ) 
          
          if( EqualKeys( auch,
                         pmr->n.ml.amMoves[ i ].auch ) ) {
            
            r = pmr->n.ml.amMoves[ i ].rScore;
            rMoveEquity = msEE.nMatchTo ? 
              eq2mwc( r, &ci ) : msEE.nCube * r;

            r = r - pmr->n.ml.amMoves[ 0 ].rScore;

            fEquity = TRUE;

            break;
          }
        
        rMoveError += msEE.nMatchTo ? 
          eq2mwc( r, &ci ) - eq2mwc( 0.0f, &ci ) : msEE.nCube * r;

        fError = TRUE;

      }

      if ( pmr->n.rLuck != ERR_VAL ) {

        rMoveLuck = msEE.nMatchTo ?
          eq2mwc( pmr->n.rLuck, &ci ) - eq2mwc( 0.0f, &ci ) :
          msEE.nCube * pmr->n.rLuck;

     }

      if ( fPlayer == pmr->n.fPlayer ) {

        if ( msEE.nMatchTo )
          rEquity = rMoveEquity;
        else
          rEquity = rStartEquity + rMoveEquity;

      }
      else {

        if ( msEE.nMatchTo ) 
          rEquity = 1.0 - rMoveEquity;
        else
          rEquity = rStartEquity - rMoveEquity;

      }

      fprintf ( pf, "%d\t%d", iGame + 1, iMove +1 );

      if ( fEquity )
        fprintf ( pf, "\t%8.4f", rEquity );
      else
        fputs ( "\t", pf );

      if ( fError )
        fprintf ( pf, "\t%8.4f", rMoveError );
      else
        fputs ( "\t", pf );

      if ( fLuck )
        fprintf ( pf, "\t%8.4f\n", rMoveLuck );
      else
        fputs ( "\t\n", pf );

      iMove++;

      break;

    case MOVE_DOUBLE:

      rMoveLuck = 0.0;
      rMoveError = 0.0;
      rMoveEquity = 0.0;
      
      if ( pmr->d.esDouble.et != EVAL_NONE ) {

        float *arDouble = pmr->d.arDouble;

        GetMatchStateCubeInfo ( &ci, &msEE );

        r = arDouble[ OUTPUT_TAKE ] <
          arDouble[ OUTPUT_DROP ] ?
          arDouble[ OUTPUT_TAKE ] - arDouble[ OUTPUT_OPTIMAL ] :
          arDouble[ OUTPUT_DROP ] - arDouble[ OUTPUT_OPTIMAL ];
	      
        if( r < 0.0f ) 
          /* it was not a double */
          rMoveError = msEE.nMatchTo ? eq2mwc( r, &ci ) - eq2mwc( 0.0f, &ci ) : 
            msEE.nCube * r;

        rMoveEquity =  msEE.nMatchTo ? 
          eq2mwc( arDouble[ OUTPUT_TAKE ], &ci ) :
          msEE.nCube * arDouble[ OUTPUT_TAKE ];

        if ( fPlayer == pmr->n.fPlayer ) {

          if ( msEE.nMatchTo )
            rEquity = rMoveEquity;
          else
            rEquity = rStartEquity + rMoveEquity;

        }
        else {

          if ( msEE.nMatchTo ) 
            rEquity = 1.0 - rMoveEquity;
          else
            rEquity = rStartEquity - rMoveEquity;

        }

        fprintf ( pf,
                  "%d\t%d\t%8.4f\t%8.4f\n",
                  iGame + 1, iMove + 1, rEquity, rMoveError );
      
      }
      else
        fprintf ( pf,
                  "%d\t%d\n", 
                  iGame + 1, iMove + 1 );

      iMove++;
        
      break;

    default:
      break;
      

    }

    ApplyMoveRecord ( &msEE, plGame, pmr );

  }
  
  PopLocale ();

}



extern void
CommandExportGameEquityEvolution ( char *sz ) {

  FILE *pf;
    
  float fEndEquity;
    
  sz = NextToken( &sz );
  if( !plGame ) {
    outputl( _("No game in progress (type `new game' to start one).") );
    return;
  }
    
  if( !sz || !*sz ) {
    outputf( _("You must specify a file to export to (see `%s')\n" ),
             "help exportgame equityevolution" );
    return;
  }

  if ( ! confirmOverwrite ( sz, fConfirmSave ) )
    return;

  if( !strcmp( sz, "-" ) )
    pf = stdout;
  else if( !( pf = fopen( sz, "w" ) ) ) {
    outputerr( sz );
    return;
  }

  ExportGameEquityEvolution( pf, plGame, 0, 0.0, &fEndEquity, 
                             getGameNumber ( plGame ) );

  if( pf != stdout )
    fclose( pf );

  // setDefaultFileName ( sz, PATH_EE );
    


}


extern void
CommandExportMatchEquityEvolution ( char *sz ) {



}

#if HAVE_LIBPNG


extern int
WritePNG ( const char *sz, unsigned char *puch, int nStride,
           const int nSizeX, const int nSizeY ) {

  FILE *pf;
  png_structp ppng;
  png_infop pinfo;
  png_text atext[ 3 ];
  int i;

  if ( ! ( pf = fopen ( sz, "wb" ) ) )
    return -1;

  if ( ! ( ppng = png_create_write_struct ( PNG_LIBPNG_VER_STRING,
                                            NULL, NULL, NULL ) ) ) {
    fclose ( pf );
    return -1;

  }

  if ( ! ( pinfo = png_create_info_struct ( ppng ) ) ) {
    fclose ( pf );
    png_destroy_write_struct ( &ppng, NULL );
    return -1;
  }

  if ( setjmp ( png_jmpbuf ( ppng ) ) ) {
    outputerr( sz );
    fclose ( pf );
    png_destroy_write_struct ( &ppng, &pinfo );
    return -1;
  }

  png_init_io(ppng, pf );

  png_set_IHDR ( ppng, pinfo, nSizeX, nSizeY, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, 
                 PNG_FILTER_TYPE_BASE );

  /* text */

  atext[ 0 ].key = "Title";
  atext[ 0 ].text = "Backgammon board";
  atext[ 0 ].compression = PNG_TEXT_COMPRESSION_NONE;
  
  atext[ 1 ].key = "author";
  atext[ 1 ].text = "GNU Backgammon" VERSION;
  atext[ 1 ].compression = PNG_TEXT_COMPRESSION_NONE;
  
#ifdef PNG_iTXt_SUPPORTED
   text_ptr[0].lang = NULL;
   text_ptr[1].lang = NULL;
#endif
   png_set_text( ppng, pinfo, atext, 2 );
  
    png_write_info( ppng, pinfo );

 {
   png_bytep aprow[ nSizeY ];
   for ( i = 0; i < nSizeY; ++i )
     aprow[ i ] = puch + nStride * i;

   png_write_image ( ppng, aprow );

 }

 png_write_end( ppng, pinfo );

 png_destroy_write_struct(&ppng, &pinfo );

   fclose ( pf );

   return 0;

}




static int
GenerateImage ( renderimages *pri, renderdata *prd,
                int anBoard[ 2 ][ 25 ],
                const char *szName,
                const int nSize, const int nSizeX, const int nSizeY,
                const int nOffsetX, const int nOffsetY,
                const int fMove, const int fTurn, const int fCube,
                const int anDice[ 2 ], const int nCube, const int fDoubled,
                const int fCubeOwner ) {

  unsigned char *puch;
  int anCubePosition[ 2 ];
  int anDicePosition[ 2 ][ 2 ];
  int nOrient;
  int doubled, color;

  if ( ! fMove )
    SwapSides ( anBoard );

  /* allocate memory for board */

  if ( ! ( puch = (unsigned char *) malloc ( 108 * 72 * 
                                             nSize * nSize * 3 ) ) ) {
    outputerr ( "malloc" );
    return -1;
  }

  /* calculate cube position */

  if ( fDoubled )
    doubled = fTurn ? -1 : 1;
  else
    doubled = 0;

  

  if ( fCube ) {
    if ( doubled || fCubeOwner == -1 ) {
      anCubePosition[ 0 ] = 50 - 24 * doubled;
      anCubePosition[ 1 ] = 32;
      nOrient = doubled;
    }
    else {
      nOrient = fCubeOwner ? -1 : +1;
      anCubePosition[ 0 ] = 50;
      anCubePosition[ 1 ] = 32 - nOrient * 29;
    }
  }
  else {
    nOrient = -1;
    anCubePosition[ 0 ] = anCubePosition[ 1 ] = -0x8000;
  }

  /* calculate dice position */

  /* FIXME */

  if ( anDice[ 0 ] ) {
    anDicePosition[ 0 ][ 0 ] = 22 + fMove * 48;
    anDicePosition[ 0 ][ 1 ] = 32;
    anDicePosition[ 1 ][ 0 ] = 32 + fMove * 48;
    anDicePosition[ 1 ][ 1 ] = 32;
  }
  else {
    anDicePosition[ 0 ][ 0 ] = -7 * nSize;
    anDicePosition[ 0 ][ 1 ] = 0;
    anDicePosition[ 1 ][ 0 ] = -7 * nSize;
    anDicePosition[ 1 ][ 1 ] = -1;
  }

  /* render board */

  color = fMove;

  CalculateArea ( prd, puch, 108 * nSize * 3, pri, anBoard, NULL,
                  (int *) anDice, anDicePosition, 
                  color, anCubePosition, 
                  LogCube( nCube ) + ( doubled != 0 ),
                  nOrient,
                  0, 0, 108 * nSize, 72 * nSize );

  /* crop */

  if ( nSizeX < 108 || nSizeY < 72 ) {
    int i, j, k;

    j = 0;
    k = nOffsetY * nSize * 108 + nOffsetX;
    for ( i = 0; i < nSizeY * nSize; ++i ) {
      /* loop over each row */
      memmove ( puch + j, puch + k, nSizeX * nSize * 3 );

      j += nSizeX * nSize * 3;
      k += 108 * nSize;
      
    }

  }

  /* write png */

  WritePNG( szName, puch, nSizeX * nSize * 3, nSizeX * nSize, nSizeY * nSize );
  
  return 0;
}



extern void
CommandExportPositionPNG ( char *sz ) {

  renderimages ri;
  renderdata rd;
  
  sz = NextToken( &sz );

  if( ms.gs == GAME_NONE ) {
    outputl( _("No game in progress (type `new game' to start one).") );
    return;
    }
    
  if( !sz || !*sz ) {
    outputl( _("You must specify a file to export to (see `help export "
               "position png').") );
    return;
  }

  if ( ! confirmOverwrite ( sz, fConfirmSave ) )
    return;

  /* generate PNG image */

  memcpy( &rd, &rdAppearance, sizeof rd );
  
  rd.nSize = exsExport.nPNGSize;

  assert( rd.nSize >= 1 );
  
  RenderImages ( &rd, &ri );

  GenerateImage ( &ri, &rd, ms.anBoard, sz, 
                  exsExport.nPNGSize, 108, 72, 0, 0, 
                  ms.fMove, ms.fTurn, fCubeUse, ms.anDice, ms.nCube,
                  ms.fDoubled, ms.fCubeOwner );

  FreeImages ( &ri );


}


#endif /* HAVE_LIBPNG */
