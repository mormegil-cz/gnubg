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

#include "eval.h"
#include "positionid.h"

static long idBoardSmob;

typedef struct _boardsmob {
    int anBoard[ 2 ][ 25 ];
} boardsmob;

static int BoardSmobPrint( SCM b, SCM port, scm_print_state *psps ) {

    boardsmob *pbs = (boardsmob *) SCM_CDR( b );
    
    scm_puts( "#<board ", port );
    scm_puts( PositionID( pbs->anBoard ), port );
    scm_puts( ">", port );
    
    return 1;
}

static SCM BoardSmobEqualP( SCM b0, SCM b1 ) {

    boardsmob *pbs0 = (boardsmob *) SCM_CDR( b0 ),
	*pbs1 = (boardsmob *) SCM_CDR( b1 );

    return EqualBoards( pbs0->anBoard, pbs1->anBoard ) ? SCM_BOOL_T :
	SCM_BOOL_F;
}

static SCM board( SCM s ) {

    boardsmob *pb;
    int i;
    
    pb = scm_must_malloc( sizeof( *pb ), "board" );

    /* FIXME initialise based on position ID, if specified */
    
    for( i = 0; i < 25; i++ ) {
	pb->anBoard[ 0 ][ i ] = 0;
	pb->anBoard[ 1 ][ i ] = 0;
    }

    SCM_RETURN_NEWSMOB( idBoardSmob, pb );
}

extern int GuileInitialise( void ) {

    idBoardSmob = scm_make_smob_type( "board", sizeof( boardsmob ) );

    scm_set_smob_print( idBoardSmob, BoardSmobPrint );
    scm_set_smob_equalp( idBoardSmob, BoardSmobEqualP );

    scm_make_gsubr( "board", 0, 1, 0, board );

    return 0;
}

#endif
