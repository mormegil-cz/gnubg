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

#include "analysis.h"
#include "backgammon.h"
#include "drawboard.h"
#include "export.h"
#include "eval.h"
#include "positionid.h"
#include "matchid.h"

#include "i18n.h"


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

    ApplyMoveRecord ( &msEE, pmr );

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
    perror( sz );
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

