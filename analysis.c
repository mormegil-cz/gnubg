/*
 * analysis.h
 *
 * by Joern Thyssen <joern@thyssen.nu>, 2000
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
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "eval.h"
#include "positionid.h"
#include "analysis.h"


void
AnalyzeGame ( list *plGame, int iGame ) ;

void
AnalyzeMatch( );

extern void 
CommandAnalysis ( char *sz ) {

  AnalyzeMatch ( );

}


void
AnalyzeMatch( ) {

  int i;
  list *pl;

  printf( " %d point match\n\n", nMatchTo );

  anScore[ 0 ] = anScore[ 1 ] = 0;
    
  for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext )
    AnalyzeGame( pl->p, i );
    
}


void
AnalyzeGame ( list *plGame, int iGame ) {

    list *pl;
    moverecord *pmr;
    char sz[ 2048 ];
    int i = 0, nCube = 1, anBoard[ 2 ][ 25 ];
    int anBoardMove[ 2 ][ 25 ];
    int fCubeOwner = -1, fPlayer;
    char *apch [ 7 ], szPlayer0[ 32 ], szPlayer1[ 32 ], szText[ 32 ];
    char szCube[ 32 ];
    int anDice[ 2 ], j;
    int fFirstMove = 1;
    unsigned char auch[ 10 ];
    evalcontext ecDouble = { 1, 0, 0, 0, TRUE };  
    evalcontext ecMove   = { 1, 8, 0.16, 0, FALSE };  
    cubeinfo ci;
    float arDouble[ 4 ], arOutput [ NUM_OUTPUTS ];
    int fWinner, nPoints;


    InitBoard( anBoard );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {

      pmr = pl->p;

      /* set apch string for call to drawboard */

      apch[ 0 ] = szPlayer0;
      apch[ 1 ] = NULL;
      apch[ 2 ] = NULL;
      apch[ 3 ] = NULL;
      apch[ 4 ] = NULL;
      apch[ 5 ] = NULL;
      apch[ 6 ] = szPlayer1;

      sprintf( szPlayer0, "O: %s", ap[ 0 ].szName );
      sprintf( szPlayer1, "X: %s", ap[ 1 ].szName );

      if ( pmr->mt != MOVE_DOUBLE ) {

        if( fCubeOwner < 0 ) {
          apch[ 3 ] = szCube;
        
          sprintf( szCube, "(Cube: %d)", nCube );
        } else {
          int cch = strlen( ap[ fCubeOwner ].szName );
        
          if( cch > 20 )
            cch = 20;
        
          sprintf( szCube, "%*s (Cube: %d)", cch,
                   ap[ fCubeOwner ].szName, nCube );
        
          apch[ fCubeOwner ? 6 : 0 ] = szCube;
        }
      }

      switch( pmr->mt ) {
      case MOVE_NORMAL:

        anDice[ 0 ] = pmr->n.anRoll[ 0 ];
        anDice[ 1 ] = pmr->n.anRoll[ 1 ];

        fPlayer = pmr->n.fPlayer;

        apch [ fPlayer ? 5 : 1 ] = szText;
        sprintf( szText, "Rolled %d%d", anDice[ 0 ], anDice[ 1 ] );

	DrawBoard( sz, anBoard, TRUE, apch );
        puts ( sz );

        if ( ! fPlayer )
          SwapSides( anBoard );

        /* cube action? */

        if ( ! fFirstMove ) {

          float arDouble[ 4 ];

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchTo,
			anScore, fCrawford, fJacoby, fBeavers );

          if ( GetDPEq ( NULL, NULL, &ci ) ) {

            if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput, &ci,
                                           &ecDouble,
                                           ecDouble.nPlies ) < 0 ) 
              return;

            pmr->n.etDouble = EVAL_EVAL;
            pmr->n.esDouble.ec = ecDouble;
            for ( j = 0; j < 4; j++ ) 
              pmr->n.arDouble[ j ] = arDouble[ j ];

            GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );

            puts ( sz );

          }
          else {

            printf ( "cube not available\n" );
            pmr->n.etDouble = EVAL_NONE;

          }

        }

        fFirstMove = 0;

        /* evaluate move */

        /* find auch for move */

        memcpy( anBoardMove, anBoard, sizeof( anBoardMove ) );
        ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
        PositionKey ( anBoardMove, auch );

        /* find best moves */

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchTo,
		      anScore, fCrawford, fJacoby, fBeavers );

        FindnSaveBestMoves ( &(pmr->n.ml), anDice[ 0 ], anDice[ 1 ],
                             anBoard, auch, &ci, &ecMove );

        if ( pmr->n.ml.cMoves ) {
          j = -1;
          do {
            j++;

            printf ( "%i %s %7.3f\n",
                     j, FormatMove ( sz, anBoard,
                                     pmr->n.ml.amMoves[ j ].anMove ),
                     pmr->n.ml.amMoves[ j ].rScore );
          }
          while ( ! EqualKeys ( auch, pmr->n.ml.amMoves[ j ].auch ) );
        }
        else {
          printf ("no legal moves..\n" );
        }
                                
        /* copy board back */

        memcpy ( anBoard, anBoardMove, sizeof ( anBoard ) );
        if ( ! fPlayer )
          SwapSides( anBoard );

        break;
      case MOVE_DOUBLE:

        fPlayer = pmr->d.fPlayer;

        if ( ! fPlayer )
          SwapSides( anBoard );

        /* cube action */

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchTo, anScore,
		      fCrawford, fJacoby, fBeavers );

        if ( GetDPEq ( NULL, NULL, &ci ) ) {

          if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput, &ci,
                                         &ecDouble,
                                         ecDouble.nPlies ) < 0 ) 
            return;

          pmr->d.etDouble = EVAL_EVAL;
          pmr->d.esDouble.ec = ecDouble;
          for ( j = 0; j < 4; j++ ) 
            pmr->d.arDouble[ j ] = arDouble[ j ];

          GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );

          puts ( sz );

        }
        
        if ( ! fPlayer )
          SwapSides( anBoard );

        sprintf( sz, " Doubles => %d", nCube <<= 1 );
        fCubeOwner = ! fPlayer;

        break;
      case MOVE_TAKE:
        /* Don't store evaluation since it's stored in the 
           previous move, which we hope is a MOVE_DOUBLE */
        strcpy( sz, " Takes" ); /* FIXME beavers? */
        break;
      case MOVE_DROP:
        /* Don't store evaluation since it's stored in the 
           previous move, which we hope is a MOVE_DOUBLE */
        strcpy( sz, " Drops\n" );
        anScore[ ( i + 1 ) & 1 ] += nCube / 2;
        break;
      case MOVE_RESIGN:
        /* FIXME how does JF do it? */
        /* FIXME: evaluate is resignation is OK */
        break;
      case MOVE_GAMEINFO:

        fWinner = pmr->g.fWinner;
        nPoints = pmr->g.nPoints;

        /* set global variables... */

        anScore[ 0 ] = pmr->g.anScore[ 0 ];
        anScore[ 1 ] = pmr->g.anScore[ 1 ];
        fCrawford = pmr->g.fCrawfordGame;
        nMatchTo = pmr->g.nMatch;

        if ( ! fCrawford &&
             ( nMatchTo - anScore[ 0 ] == 1 ||
               nMatchTo - anScore[ 1 ] == 1 ) )
          fPostCrawford = 1;

        if( iGame >= 0 )
          printf( " Game %d\n", iGame + 1 );

        printf ("Score:\n"
                "%s: %d\n" "%s: %d (match to %i)\n",
                ap[ 0 ].szName, anScore[ 0 ],
                ap[ 1 ].szName, anScore[ 1 ],
                nMatchTo );

        printf ("Crawford    : %i\n"
                "PostCrawford: %i\n", fCrawford, fPostCrawford );
          
    
        break;
	
      case MOVE_SETBOARD:
      case MOVE_SETDICE:
      case MOVE_SETCUBEVAL:
      case MOVE_SETCUBEPOS:
	  /* FIXME apply these records */
	  ;
      }

      if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) i++;

      i++;
    }

    if ( fWinner != -1 )
      anScore[ fWinner ] += nPoints;

    printf ( "%s wins %i points...\n",
             ap [ fWinner ].szName, nPoints );
}







