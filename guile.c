/*
 * guile.c
 *
 * by Gary Wong <gtw@gnu.org>, 2000
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

#if USE_GUILE

#include <libguile.h>

#include "backgammon.h"
#include "eval.h"
#include "positionid.h"
#include "rollout.h"

static SCM BoardToSCM( int anBoard[ 2 ][ 25 ] ) {

    SCM b0, b1;
    int i;
    
    b0 = scm_make_vector( SCM_MAKINUM( 25 ), SCM_UNSPECIFIED );
    b1 = scm_make_vector( SCM_MAKINUM( 25 ), SCM_UNSPECIFIED );

    for( i = 0; i < 25; i++ ) {
	scm_vector_set_x( b0, SCM_MAKINUM( i ),
			  SCM_MAKINUM( anBoard[ 0 ][ i ] ) );
	scm_vector_set_x( b1, SCM_MAKINUM( i ),
			  SCM_MAKINUM( anBoard[ 1 ][ i ] ) );
    }
    
    return scm_cons( b0, b1 );
}

static void SCMToBoard( SCM s, int anBoard[ 2 ][ 25 ] ) {

    int i;
    SCM n;

    SCM_ASSERT( SCM_NIMP( s ) && SCM_CONSP( s ), s, SCM_ARGn, NULL );
    
    for( i = 0; i < 25; i++ ) {
	n = scm_vector_ref( SCM_CAR( s ), SCM_MAKINUM( i ) );
	SCM_ASSERT( SCM_INUMP( n ), n, SCM_ARGn, NULL );
	anBoard[ 0 ][ i ] = SCM_INUM( n );
	n = scm_vector_ref( SCM_CDR( s ), SCM_MAKINUM( i ) );
	SCM_ASSERT( SCM_INUMP( n ), n, SCM_ARGn, NULL );
	anBoard[ 1 ][ i ] = SCM_INUM( n );
    }
}

static SCM board_to_position_id( SCM sBoard ) {

    int anBoard[ 2 ][ 25 ];
    
    SCMToBoard( sBoard, anBoard );
    return scm_makfrom0str( PositionID( anBoard ) );
}

static SCM current_board( void ) {

    return fTurn == -1 ? SCM_BOOL_F : BoardToSCM( anBoard );
}

static SCM evaluate_position( SCM sBoard, SCM sCube, SCM sEvalContext ) {

    int i, anBoard[ 2 ][ 25 ];
    float ar[ NUM_OUTPUTS ];
    SCM s;
    
    SCMToBoard( sBoard, anBoard );
    EvaluatePosition( anBoard, ar, NULL, NULL ); /* FIXME use all params */
    
    s = scm_make_vector( SCM_MAKINUM( NUM_OUTPUTS ), SCM_UNSPECIFIED );
    for( i = 0; i < NUM_OUTPUTS; i++ )
	scm_vector_set_x( s, SCM_MAKINUM( i ), scm_make_real( ar[ i ] ) );

    return s;
}

static SCM position_id_to_board( SCM sPosID ) {

    char sz[ 15 ];
    int c, anBoard[ 2 ][ 25 ];
    
    SCM_ASSERT( SCM_ROSTRINGP( sPosID ), sPosID, SCM_ARG1,
		"position-id->board" );
    if( ( c = SCM_LENGTH( sPosID ) ) > 14 )
	c = 14;
    
    memcpy( sz, SCM_ROCHARS( sPosID ), c );
    sz[ 14 ] = 0;

    if( PositionFromID( anBoard, sz ) )
	return SCM_BOOL_F;
    else
	return BoardToSCM( anBoard );
}

static SCM rollout_position( SCM sBoard, SCM sGames, SCM sTruncate,
			     SCM sVarRedn, SCM sCube, SCM sEvalContext ) {
    int i, anBoard[ 2 ][ 25 ];
    float ar[ NUM_ROLLOUT_OUTPUTS ], arStdDev[ NUM_ROLLOUT_OUTPUTS ];
    SCM s;
    cubeinfo ci;
    static char sz[] = "rollout-position";
    
    SCM_ASSERT( SCM_INUMP( sGames ) || sGames == SCM_UNSPECIFIED, sGames,
		SCM_ARG2, sz );
    SCM_ASSERT( SCM_INUMP( sTruncate ) || sTruncate == SCM_UNSPECIFIED,
		sTruncate, SCM_ARG3, sz );
    SCM_ASSERT( SCM_BOOLP( sVarRedn ) || sVarRedn == SCM_UNSPECIFIED,
		sVarRedn, SCM_ARG4, sz );
    
    SCMToBoard( sBoard, anBoard );
    
    SetCubeInfo ( &ci, nCube, fCubeOwner, fMove ); /* FIXME use sCube */
 
    if( Rollout( anBoard, ar, arStdDev, sTruncate == SCM_UNSPECIFIED ?
		 nRolloutTruncate : SCM_INUM( sTruncate ), sGames ==
		 SCM_UNSPECIFIED ? nRollouts : SCM_INUM( sGames ),
		 SCM_NFALSEP( sVarRedn ), &ci,
		 &ecRollout /* FIXME use sEvalContext */ ) < 0 )
	return SCM_UNSPECIFIED; /* FIXME throw error? */

    s = scm_cons( scm_make_vector( SCM_MAKINUM( NUM_ROLLOUT_OUTPUTS ),
				   SCM_UNSPECIFIED ),
		  scm_make_vector( SCM_MAKINUM( NUM_ROLLOUT_OUTPUTS ),
				   SCM_UNSPECIFIED ) );
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ ) {
	scm_vector_set_x( SCM_CAR( s ), SCM_MAKINUM( i ),
			  scm_make_real( ar[ i ] ) );
	scm_vector_set_x( SCM_CDR( s ), SCM_MAKINUM( i ),
			  scm_make_real( arStdDev[ i ] ) );
    }

    return s;
}

extern int GuileInitialise( void ) {

    scm_make_gsubr( "board->position-id", 1, 0, 0, board_to_position_id );
    scm_make_gsubr( "current-board", 0, 0, 0, current_board );
    scm_make_gsubr( "evaluate-position", 1, 2, 0, evaluate_position );
    scm_make_gsubr( "position-id->board", 1, 0, 0, position_id_to_board );
    scm_make_gsubr( "rollout-position", 1, 5, 0, rollout_position );
    
    return 0;
}

#endif
