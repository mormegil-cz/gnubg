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

#include <errno.h>
#if HAVE_GUILE_GTK_H
#include <guile-gtk.h>
#endif
#include <libguile.h>
#include <signal.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "backgammon.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "guile.h"
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

    SCM_ASSERT( SCM_CONSP( s ), s, SCM_ARGn, NULL );
    
    for( i = 0; i < 25; i++ ) {
	n = scm_vector_ref( SCM_CAR( s ), SCM_MAKINUM( i ) );
	SCM_ASSERT( SCM_INUMP( n ), n, SCM_ARGn, NULL );
	anBoard[ 0 ][ i ] = SCM_INUM( n );
	n = scm_vector_ref( SCM_CDR( s ), SCM_MAKINUM( i ) );
	SCM_ASSERT( SCM_INUMP( n ), n, SCM_ARGn, NULL );
	anBoard[ 1 ][ i ] = SCM_INUM( n );
    }
}

static SCM CubeInfoToSCM( cubeinfo *pci ) {

    SCM s = scm_make_vector( SCM_MAKINUM( 7 ), SCM_UNSPECIFIED );

    scm_vector_set_x( s, SCM_MAKINUM( 0 ), SCM_MAKINUM( pci->nCube ) );
    scm_vector_set_x( s, SCM_MAKINUM( 1 ), SCM_MAKINUM( pci->fCubeOwner ) );
    scm_vector_set_x( s, SCM_MAKINUM( 2 ), SCM_MAKINUM( pci->fMove ) );
    scm_vector_set_x( s, SCM_MAKINUM( 3 ),
		      scm_make_real( pci->arGammonPrice[ 0 ] ) );
    scm_vector_set_x( s, SCM_MAKINUM( 4 ),
		      scm_make_real( pci->arGammonPrice[ 1 ] ) );
    scm_vector_set_x( s, SCM_MAKINUM( 5 ),
		      scm_make_real( pci->arGammonPrice[ 2 ] ) );
    scm_vector_set_x( s, SCM_MAKINUM( 6 ),
		      scm_make_real( pci->arGammonPrice[ 3 ] ) );

    return s;
}

static void SCMToCubeInfo( SCM s, cubeinfo *pci ) {

    SCM sv;
    int i;
    
    sv = scm_vector_ref( s, SCM_MAKINUM( 0 ) );
    SCM_ASSERT( SCM_INUMP( sv ), sv, SCM_ARGn, NULL );
    pci->nCube = SCM_INUM( sv );

    sv = scm_vector_ref( s, SCM_MAKINUM( 1 ) );
    SCM_ASSERT( SCM_INUMP( sv ), sv, SCM_ARGn, NULL );
    pci->fCubeOwner = SCM_INUM( sv );

    sv = scm_vector_ref( s, SCM_MAKINUM( 2 ) );
    SCM_ASSERT( SCM_INUMP( sv ), sv, SCM_ARGn, NULL );
    pci->fMove = SCM_INUM( sv );

    for( i = 0; i < 4; i++ ) {
	sv = scm_vector_ref( s, SCM_MAKINUM( i + 3 ) );
	SCM_ASSERT( SCM_REALP( sv ), sv, SCM_ARGn, NULL );
	pci->arGammonPrice[ i ] = SCM_REAL_VALUE( sv );
    }
}

static SCM board_to_position_id( SCM sBoard ) {

    int anBoard[ 2 ][ 25 ];
    
    SCMToBoard( sBoard, anBoard );
    return scm_makfrom0str( PositionID( anBoard ) );
}

static SCM classify_position( SCM sBoard ) {

    int anBoard[ 2 ][ 25 ];
    
    SCMToBoard( sBoard, anBoard );
    return SCM_MAKINUM( ClassifyPosition( anBoard ) );
}

static SCM cube_info( SCM sCube, SCM sCubeOwner, SCM sMove, SCM sMatchTo,
		      SCM sScore, SCM sCrawford, SCM sJacoby, SCM sBeavers ) {

    cubeinfo ci;
    static char sz[] = "cube-info";
    int an[ 2 ];
    
    SCM_ASSERT( SCM_INUMP( sCube ) || sCube == SCM_UNDEFINED, sCube,
		SCM_ARG1, sz );
    SCM_ASSERT( SCM_INUMP( sCubeOwner ) || sCubeOwner == SCM_UNDEFINED,
		sCubeOwner, SCM_ARG2, sz );
    SCM_ASSERT( SCM_INUMP( sMove ) || sMove == SCM_UNDEFINED, sMove,
		SCM_ARG3, sz );
    SCM_ASSERT( SCM_INUMP( sMatchTo ) || sMatchTo == SCM_UNDEFINED,
		sMatchTo, SCM_ARG4, sz );
    SCM_ASSERT( ( SCM_CONSP( sScore ) && SCM_INUMP( SCM_CAR( sScore ) ) &&
		SCM_INUMP( SCM_CDR( sScore ) ) ) || sScore == SCM_UNDEFINED,
		sScore, SCM_ARG5, sz );

    if( sCube == SCM_UNDEFINED && ms.gs == GAME_NONE )
	/* no move specified, and no game in progress */
	return SCM_BOOL_F;

    if( sScore != SCM_UNDEFINED ) {
	an[ 0 ] = SCM_INUM( SCM_CAR( sScore ) );
	an[ 1 ] = SCM_INUM( SCM_CDR( sScore ) );
    }

    SCM_DEFER_INTS;
    
    SetCubeInfo( &ci, sCube == SCM_UNDEFINED ? ms.nCube : SCM_INUM( sCube ),
		 sCubeOwner == SCM_UNDEFINED ? ms.fCubeOwner :
		 SCM_INUM( sCubeOwner ),
		 sMove == SCM_UNDEFINED ? ms.fMove : SCM_INUM( sMove ),
		 sMatchTo == SCM_UNDEFINED ? ms.nMatchTo :
		 SCM_INUM( sMatchTo ),
		 sScore == SCM_UNDEFINED ? ms.anScore : an,
		 sCrawford == SCM_UNDEFINED ? ms.fCrawford :
		 SCM_NFALSEP( sCrawford ),
		 sJacoby == SCM_UNDEFINED ? fJacoby :
		 SCM_NFALSEP( sJacoby ),
		 sBeavers == SCM_UNDEFINED ? fBeavers :
		 SCM_NFALSEP( sBeavers ) );

    SCM_ALLOW_INTS;
    
    return CubeInfoToSCM( &ci );
}

static SCM cube_info_match( SCM sCube, SCM sCubeOwner, SCM sMove,
			    SCM sMatchTo, SCM sScore, SCM sCrawford ) {
    cubeinfo ci;
    static char sz[] = "cube-info-match";
    int an[ 2 ];
    
    SCM_ASSERT( SCM_INUMP( sCube ), sCube, SCM_ARG1, sz );
    SCM_ASSERT( SCM_INUMP( sCubeOwner ), sCubeOwner, SCM_ARG2, sz );
    SCM_ASSERT( SCM_INUMP( sMove ), sMove, SCM_ARG3, sz );
    SCM_ASSERT( SCM_INUMP( sMatchTo ), sMatchTo, SCM_ARG4, sz );
    SCM_ASSERT( SCM_CONSP( sScore ), sScore, SCM_ARG5, sz );
    SCM_ASSERT( SCM_INUMP( SCM_CAR( sScore ) ), sScore, SCM_ARG5, sz );
    SCM_ASSERT( SCM_INUMP( SCM_CDR( sScore ) ), sScore, SCM_ARG5, sz );

    an[ 0 ] = SCM_INUM( SCM_CAR( sScore ) );
    an[ 1 ] = SCM_INUM( SCM_CDR( sScore ) );

    SCM_DEFER_INTS;
    
    SetCubeInfoMatch( &ci, SCM_INUM( sCube ), SCM_INUM( sCubeOwner ),
		      SCM_INUM( sMove ), SCM_INUM( sMatchTo ), an,
		      SCM_NFALSEP( sCrawford ) );

    SCM_ALLOW_INTS;
    
    return CubeInfoToSCM( &ci );
}

static SCM cube_info_money( SCM sCube, SCM sCubeOwner, SCM sMove,
			    SCM sJacoby, SCM sBeavers ) {

    cubeinfo ci;
    static char sz[] = "cube-info-money";

    SCM_ASSERT( SCM_INUMP( sCube ), sCube, SCM_ARG1, sz );
    SCM_ASSERT( SCM_INUMP( sCubeOwner ), sCubeOwner, SCM_ARG2, sz );
    SCM_ASSERT( SCM_INUMP( sMove ), sMove, SCM_ARG3, sz );

    SCM_DEFER_INTS;
    
    SetCubeInfoMoney( &ci, SCM_INUM( sCube ), SCM_INUM( sCubeOwner ),
		      SCM_INUM( sMove ), sJacoby == SCM_UNDEFINED ?
		      fJacoby : SCM_NFALSEP( sJacoby ),
		      sBeavers == SCM_UNDEFINED ? fBeavers :
		      SCM_NFALSEP( sBeavers ) );

    SCM_ALLOW_INTS;
    
    return CubeInfoToSCM( &ci );
}

static SCM current_board( void ) {

    return ms.gs == GAME_NONE ? SCM_BOOL_F : BoardToSCM( ms.anBoard );
}

static SCM current_score( void ) {

    SCM s;
    
    s = scm_make_vector( SCM_MAKINUM( 4 ), SCM_UNSPECIFIED );
    scm_vector_set_x( s, SCM_MAKINUM( 0 ), SCM_MAKINUM( ms.anScore[ 0 ] ) );
    scm_vector_set_x( s, SCM_MAKINUM( 1 ), SCM_MAKINUM( ms.anScore[ 1 ] ) );
    scm_vector_set_x( s, SCM_MAKINUM( 2 ), ms.fCrawford ? SCM_BOOL_T :
		      SCM_BOOL_F );
    scm_vector_set_x( s, SCM_MAKINUM( 3 ), SCM_MAKINUM( ms.cGames ) );

    return s;
}

static SCM evaluate_position( SCM sBoard, SCM sCube, SCM sEvalContext ) {

    int i, anBoard[ 2 ][ 25 ], n;
    float ar[ NUM_OUTPUTS ];
    SCM s;
    cubeinfo ci;
    psighandler sh;
    
    SCMToBoard( sBoard, anBoard );
    
    if( sCube == SCM_UNDEFINED )
	sCube = cube_info( SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED );

    SCMToCubeInfo( sCube, &ci );

    PortableSignal( SIGINT, HandleInterrupt, &sh, FALSE );
    n = EvaluatePosition( anBoard, ar, &ci, NULL );
    PortableSignalRestore( SIGINT, &sh );
    if( fInterrupt ) {
	raise( SIGINT );
	fInterrupt = FALSE;
    }
    
    if( n < 0 )
	return SCM_BOOL_F; /* FIXME throw error? */
    
    s = scm_make_vector( SCM_MAKINUM( NUM_OUTPUTS ), SCM_UNSPECIFIED );
    for( i = 0; i < NUM_OUTPUTS; i++ )
	scm_vector_set_x( s, SCM_MAKINUM( i ), scm_make_real( ar[ i ] ) );

    return s;
}

static SCM evaluate_position_cubeful( SCM sBoard, SCM sCube,
				      SCM sEvalContext ) {
    int i, anBoard[ 2 ][ 25 ], n;
    float ar[ NUM_OUTPUTS ], arCube[ NUM_CUBEFUL_OUTPUTS ];
    SCM s;
    cubeinfo ci;
    psighandler sh;
    
    SCMToBoard( sBoard, anBoard );
    
    if( sCube == SCM_UNDEFINED )
	sCube = cube_info( SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED );

    SCMToCubeInfo( sCube, &ci );

    PortableSignal( SIGINT, HandleInterrupt, &sh, FALSE );
    n = EvaluatePositionCubeful( anBoard, arCube, ar, &ci, NULL, 0 );
    PortableSignalRestore( SIGINT, &sh );
    if( fInterrupt ) {
	raise( SIGINT );
	fInterrupt = FALSE;
    }
    
    if( n < 0 )
	return SCM_BOOL_F; /* FIXME throw error? */
    
    s = scm_make_vector( SCM_MAKINUM( NUM_CUBEFUL_OUTPUTS ), SCM_UNSPECIFIED );
    for( i = 0; i < NUM_CUBEFUL_OUTPUTS; i++ )
	scm_vector_set_x( s, SCM_MAKINUM( i ), scm_make_real( arCube[ i ] ) );

    return s;
}

static SCM game_state( void ) {

    return SCM_MAKINUM( ms.gs );
}

static SCM gnubg_command( SCM sCommand ) {

    int cch;
    char *sz;
    psighandler sh;

    SCM_ASSERT( SCM_ROSTRINGP( sCommand ), sCommand, SCM_ARG1,
		"gnubg-command" );

    cch = strlen( SCM_ROCHARS( sCommand ) );
    /* FIXME use alloca if we can */    
    strcpy( sz = malloc( cch + 1 ), SCM_ROCHARS( sCommand ) );

    PortableSignal( SIGINT, HandleInterrupt, &sh, FALSE );
    HandleCommand( sz, acTop );
    NextTurn( FALSE );
    outputx();
    free( sz );
    PortableSignalRestore( SIGINT, &sh );
    if( fInterrupt ) {
	raise( SIGINT );
	fInterrupt = FALSE;
    }
    
    return SCM_UNSPECIFIED;
}

static SCM menu_bar( void ) {
#if USE_GTK && HAVE_LIBGUILEGTK_1_2
    if( fX && pwMenuBar )
	return sgtk_wrap_gtkobj( GTK_OBJECT( pwMenuBar ) );
    else
#endif
	return SCM_BOOL_F;
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
			     SCM sVarRedn, SCM sCube, SCM sEvalContext,
			     SCM sDesc, SCM sInvert ) {
    int i, anBoard[ 2 ][ 25 ], n;
    float ar[ NUM_ROLLOUT_OUTPUTS ], arStdDev[ NUM_ROLLOUT_OUTPUTS ];
    SCM s;
    cubeinfo ci;
    static char sz[] = "rollout-position";
    psighandler sh;
    
    SCM_ASSERT( SCM_INUMP( sGames ) || sGames == SCM_UNDEFINED, sGames,
		SCM_ARG2, sz );
    SCM_ASSERT( SCM_INUMP( sTruncate ) || sTruncate == SCM_UNDEFINED,
		sTruncate, SCM_ARG3, sz );
    if( sVarRedn == SCM_UNDEFINED )
	sVarRedn = SCM_BOOL_F;
    SCM_ASSERT( SCM_STRINGP( sDesc ) || sDesc == SCM_UNDEFINED, sDesc,
		SCM_ARG7, sz );
    if( sInvert == SCM_UNDEFINED )
	sInvert = SCM_BOOL_F;
    
    SCMToBoard( sBoard, anBoard );

    if( sCube == SCM_UNDEFINED )
	sCube = cube_info( SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED, SCM_UNDEFINED,
			   SCM_UNDEFINED, SCM_UNDEFINED );

    SCMToCubeInfo( sCube, &ci );

    PortableSignal( SIGINT, HandleInterrupt, &sh, FALSE );    
    n = Rollout( anBoard, sDesc == SCM_UNDEFINED ?
		 PositionID( anBoard ) : SCM_CHARS( sDesc ),
		 ar, arStdDev, sTruncate == SCM_UNDEFINED ?
		 nRolloutTruncate : SCM_INUM( sTruncate ), sGames ==
		 SCM_UNDEFINED ? nRollouts : SCM_INUM( sGames ),
		 SCM_NFALSEP( sVarRedn ), &ci,
		 &ecRollout /* FIXME use sEvalContext */,
		 SCM_NFALSEP( sInvert ) );
    PortableSignalRestore( SIGINT, &sh );
    if( fInterrupt ) {
	raise( SIGINT );
	fInterrupt = FALSE;
    }
    
    if( n < 0 )
	return SCM_BOOL_F; /* FIXME throw error? */

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

    /* FIXME n (i.e. the number of rollouts completed) should be returned
       to the caller somehow */
    
    return s;
}

static SCM sInterrupt;

extern int GuileStartIntHandler( void ) {

    if( isatty( STDIN_FILENO ) ) {
	scm_sigaction( SCM_MAKINUM( SIGINT ), sInterrupt, SCM_MAKINUM( 0 ) );
	scm_unmask_signals();
    }
    
    return 0;
}

extern int GuileEndIntHandler( void ) {

    if( isatty( STDIN_FILENO ) ) {
	scm_mask_signals();
	scm_sigaction( SCM_MAKINUM( SIGINT ), SCM_BOOL_F, SCM_MAKINUM( 0 ) );
    }
    
    return 0;
}

static void LoadGuile( char *sz ) {
    
    scm_internal_catch( SCM_BOOL_T, (scm_catch_body_t) scm_primitive_load,
			(void *) scm_makfrom0str( sz ),
			scm_handle_by_message_noexit, NULL );
}

extern int GuileInitialise( char *szDir ) {

    char szPath[ PATH_MAX ];

    scm_sysintern( "CLASS_OVER", SCM_MAKINUM( CLASS_OVER ) );
    scm_sysintern( "CLASS_BEAROFF2", SCM_MAKINUM( CLASS_BEAROFF2 ) );
    scm_sysintern( "CLASS_BEAROFF1", SCM_MAKINUM( CLASS_BEAROFF1 ) );
    scm_sysintern( "CLASS_RACE", SCM_MAKINUM( CLASS_RACE ) );
    scm_sysintern( "CLASS_CONTACT", SCM_MAKINUM( CLASS_CONTACT ) );

    scm_sysintern( "GAME_NONE", SCM_MAKINUM( GAME_NONE ) );
    scm_sysintern( "GAME_PLAYING", SCM_MAKINUM( GAME_PLAYING ) );
    scm_sysintern( "GAME_OVER", SCM_MAKINUM( GAME_OVER ) );
    scm_sysintern( "GAME_RESIGNED", SCM_MAKINUM( GAME_RESIGNED ) );
    scm_sysintern( "GAME_DROP", SCM_MAKINUM( GAME_DROP ) );
    
    scm_make_gsubr( "board->position-id", 1, 0, 0, board_to_position_id );
    scm_make_gsubr( "classify-position", 1, 0, 0, classify_position );
    scm_make_gsubr( "cube-info", 0, 8, 0, cube_info );
    scm_make_gsubr( "cube-info-match", 6, 0, 0, cube_info_match );
    scm_make_gsubr( "cube-info-money", 3, 2, 0, cube_info_money );
    scm_make_gsubr( "current-board", 0, 0, 0, current_board );
    scm_make_gsubr( "current-score", 0, 0, 0, current_score );
    scm_make_gsubr( "evaluate-position", 1, 2, 0, evaluate_position );
    scm_make_gsubr( "evaluate-position-cubeful", 1, 2, 0,
		    evaluate_position_cubeful );
    scm_make_gsubr( "game-state", 0, 0, 0, game_state );
    scm_make_gsubr( "gnubg-command", 1, 0, 0, gnubg_command );
    scm_make_gsubr( "menu-bar", 0, 0, 0, menu_bar );
    scm_make_gsubr( "position-id->board", 1, 0, 0, position_id_to_board );
    scm_make_gsubr( "rollout-position", 1, 7, 0, rollout_position );

    /* This is an ugly way to get something to pass to scm_sigaction,
       but it works. */
    sInterrupt = scm_eval_0str( "(lambda (sig) "
				"(scm-error 'signal #f "
				"\"User interrupt\" #f (list SIGINT)))" );
    scm_protect_object( sInterrupt );

#if USE_GTK && HAVE_LIBGUILEGTK_1_2
    if( fX )
	sgtk_init();
#endif
    
    if( szDir ) {
	sprintf( szPath, "%s/" GNUBG_SCM, szDir );
	if( !access( szPath, R_OK ) ) {
	    LoadGuile( szPath );
	    return 0;
	}
    }

    if( !access( GNUBG_SCM, R_OK ) ) {
	LoadGuile( GNUBG_SCM );
	return 0;
    }

    if( !access( PKGDATADIR "/" GNUBG_SCM, R_OK ) ) {
	LoadGuile( PKGDATADIR "/" GNUBG_SCM );
	return 0;
    }
    
    perror( GNUBG_SCM );
    return -1;
}
#endif
