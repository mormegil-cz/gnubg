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

extern void CommandAnalyseGame( char *sz ) {

    if( !plGame ) {
	outputl( "No game is being played." );
	return;
    }
    
    ProgressStart( "Analysing game..." );
    
    AnalyzeGame( plGame, 0 );

    ProgressEnd();
}

extern void CommandAnalyseMatch( char *sz ) {

  int i;
  list *pl;

#if DEBUG_ANALYSIS
  printf( " %d point match\n\n", nMatchTo );
#endif
  
  ProgressStart( "Analysing match..." );
  
  for( i = 0, pl = lMatch.plNext; pl != &lMatch; i++, pl = pl->plNext )
    AnalyzeGame( pl->p, i );

  ProgressEnd();
}

extern void CommandAnalyseSession( char *sz ) {

    CommandAnalyseMatch( sz );
}

void
AnalyzeGame ( list *plGame, int iGame ) {

    list *pl;
    moverecord *pmr;
    int i = 0, nCube = 1, anBoard[ 2 ][ 25 ];
    int fCrawfordLocal = fCrawford, fPostCrawfordLocal = fPostCrawford,
	fJacobyLocal = fJacoby, fBeaversLocal = fBeavers,
	nMatchToLocal = nMatchTo;
    int anBoardMove[ 2 ][ 25 ];
    int anScore[ 2 ];
    int fCubeOwner = -1, fPlayer = -1;
    int anDice[ 2 ], j;
    int fFirstMove = 1;
    unsigned char auch[ 10 ];
    cubeinfo ci;
    float arDouble[ 4 ], arOutput [ NUM_OUTPUTS ];
    int fWinner, nPoints;
#if DEBUG_ANALYSIS
    char sz[ 2048 ];
    char *apch [ 7 ], szPlayer0[ 32 ], szPlayer1[ 32 ], szText[ 32 ];
    char szCube[ 32 ];
#endif

    InitBoard( anBoard );
    
    for( pl = plGame->plNext; pl != plGame; pl = pl->plNext ) {
      pmr = pl->p;

      Progress();
      
#if DEBUG_ANALYSIS
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
#endif
      
      switch( pmr->mt ) {
      case MOVE_NORMAL:

        anDice[ 0 ] = pmr->n.anRoll[ 0 ];
        anDice[ 1 ] = pmr->n.anRoll[ 1 ];
        fPlayer = pmr->n.fPlayer;

	/* FIXME calculate luck */
	
#if DEBUG_ANALYSIS
        apch [ fPlayer ? 5 : 1 ] = szText;
        sprintf( szText, "Rolled %d%d", anDice[ 0 ], anDice[ 1 ] );

	DrawBoard( sz, anBoard, TRUE, apch );
        puts ( sz );
#endif
	
        if ( ! fPlayer )
          SwapSides( anBoard );

        /* cube action? */

        if ( ! fFirstMove ) {

          float arDouble[ 4 ];

          SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
			anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

          if ( GetDPEq ( NULL, NULL, &ci ) ) {

            if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput, &ci,
                                           &ecEval, ecEval.nPlies ) < 0 ) 
              return;

            pmr->n.etDouble = EVAL_EVAL;
            pmr->n.esDouble.ec = ecEval;
            for ( j = 0; j < 4; j++ ) 
              pmr->n.arDouble[ j ] = arDouble[ j ];

#if DEBUG_ANALYSIS
            GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );

            puts ( sz );
#endif
          }
          else {
#if DEBUG_ANALYSIS
            printf ( "cube not available\n" );
#endif
            pmr->n.etDouble = EVAL_NONE;

          }

        }

        fFirstMove = 0;

        /* evaluate move */

        /* find auch for move */

        memcpy( anBoardMove, anBoard, sizeof( anBoardMove ) );
        ApplyMove( anBoardMove, pmr->n.anMove, FALSE );
        PositionKey ( anBoardMove, auch );

	if( pmr->n.ml.cMoves )
	    free( pmr->n.ml.amMoves );
	
        /* find best moves */

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal,
		      anScore, fCrawfordLocal, fJacobyLocal, fBeaversLocal );

        FindnSaveBestMoves ( &(pmr->n.ml), anDice[ 0 ], anDice[ 1 ],
                             anBoard, auch, &ci, &ecEval );

	for( pmr->n.iMove = 0; pmr->n.iMove < pmr->n.ml.cMoves;
	     pmr->n.iMove++ ) {
#if DEBUG_ANALYSIS
		printf( "%i %s %7.3f\n", pmr->n.iMove,
			FormatMove( sz, anBoard,
				    pmr->n.ml.amMoves[ pmr->n.iMove ].anMove ),
			pmr->n.ml.amMoves[ pmr->n.iMove ].rScore );
#endif
		if( EqualKeys( auch, pmr->n.ml.amMoves[ pmr->n.iMove ].auch ) )
		    break;
	}

	if( cAnalysisMoves >= 2 && pmr->n.ml.cMoves > cAnalysisMoves ) {
	    /* There are more legal moves than we want; throw some away. */
	    if( pmr->n.iMove >= cAnalysisMoves ) {
		/* The move made wasn't in the top n; move it up so it
		   won't be discarded. */
		memcpy( pmr->n.ml.amMoves + cAnalysisMoves - 1,
			pmr->n.ml.amMoves + pmr->n.iMove, sizeof( move ) );
		pmr->n.iMove = cAnalysisMoves - 1;
	    }
	    
	    realloc( pmr->n.ml.amMoves, cAnalysisMoves * sizeof( move ) );
	    pmr->n.ml.cMoves = cAnalysisMoves;
	}
	
#if DEBUG_ANALYSIS
	if( !pmr->n.ml.cMoves )
	    printf ("no legal moves..\n" );
#endif
	  
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

        SetCubeInfo ( &ci, nCube, fCubeOwner, fPlayer, nMatchToLocal, anScore,
		      fCrawfordLocal, fJacobyLocal, fBeaversLocal );

        if ( GetDPEq ( NULL, NULL, &ci ) ) {

          if ( EvaluatePositionCubeful ( anBoard, arDouble, arOutput, &ci,
                                         &ecEval, ecEval.nPlies ) < 0 ) 
            return;

          pmr->d.etDouble = EVAL_EVAL;
          pmr->d.esDouble.ec = ecEval;
          for ( j = 0; j < 4; j++ ) 
            pmr->d.arDouble[ j ] = arDouble[ j ];
#if DEBUG_ANALYSIS
          GetCubeActionSz ( arDouble, sz, &ci, fOutputMWC, FALSE );

          puts ( sz );
#endif
        }
        
        if ( ! fPlayer )
          SwapSides( anBoard );
#if DEBUG_ANALYSIS
        sprintf( sz, " Doubles => %d", nCube <<= 1 );
#endif
        fCubeOwner = ! fPlayer;

        break;
      case MOVE_TAKE:
        /* Don't store evaluation since it's stored in the 
           previous move, which we hope is a MOVE_DOUBLE */
#if DEBUG_ANALYSIS
        strcpy( sz, " Takes" ); /* FIXME beavers? */
#endif
        break;
      case MOVE_DROP:
        /* Don't store evaluation since it's stored in the 
           previous move, which we hope is a MOVE_DOUBLE */
#if DEBUG_ANALYSIS
        strcpy( sz, " Drops\n" );
#endif
        anScore[ ( i + 1 ) & 1 ] += nCube / 2;
        break;
      case MOVE_RESIGN:
        /* FIXME how does JF do it? */
        /* FIXME: evaluate is resignation is OK */
        break;
      case MOVE_GAMEINFO:

        fWinner = pmr->g.fWinner;
        nPoints = pmr->g.nPoints;

        anScore[ 0 ] = pmr->g.anScore[ 0 ];
        anScore[ 1 ] = pmr->g.anScore[ 1 ];
        fCrawfordLocal = pmr->g.fCrawfordGame;
        nMatchToLocal = pmr->g.nMatch;

        if ( ! fCrawfordLocal &&
             ( nMatchToLocal - anScore[ 0 ] == 1 ||
               nMatchToLocal - anScore[ 1 ] == 1 ) )
          fPostCrawfordLocal = 1;

#if DEBUG_ANALYSIS
        if( iGame >= 0 )
          printf( " Game %d\n", iGame + 1 );

        printf ("Score:\n"
                "%s: %d\n" "%s: %d (match to %i)\n",
                ap[ 0 ].szName, anScore[ 0 ],
                ap[ 1 ].szName, anScore[ 1 ],
                nMatchToLocal );

        printf ("Crawford    : %i\n"
                "PostCrawford: %i\n", fCrawfordLocal, fPostCrawfordLocal );
#endif
    
        break;
	
      case MOVE_SETBOARD:
	  PositionFromKey( anBoard, pmr->sb.auchKey );
	  
	  if( fMove < 0 )
	      fTurn = fMove = 0;
	  
	  if( fMove )
	      SwapSides( anBoard );
	  
	  break;
	  
      case MOVE_SETDICE:
	  anDice[ 0 ] = pmr->sd.anDice[ 0 ];
	  anDice[ 1 ] = pmr->sd.anDice[ 1 ];
	  fPlayer = pmr->sd.fPlayer;
	  
	  /* FIXME calculate luck */
	  fDoubled = FALSE;
	  break;
	  
      case MOVE_SETCUBEVAL:
	  if( fPlayer < 0 )
	      fPlayer = 0;
	  
	  nCube = pmr->scv.nCube;
	  fDoubled = FALSE;
	  break;
	  
      case MOVE_SETCUBEPOS:
	  if( fPlayer < 0 )
	      fPlayer = 0;
	  
	  fCubeOwner = pmr->scp.fCubeOwner;
	  fDoubled = FALSE;
	  break;
      }
      
      if( !i && pmr->mt == MOVE_NORMAL && pmr->n.fPlayer ) i++;

      i++;
    }

    if ( fWinner != -1 )
      anScore[ fWinner ] += nPoints;

#if DEBUG_ANALYSIS
    printf ( "%s wins %i points...\n",
             ap [ fWinner ].szName, nPoints );
#endif
}







