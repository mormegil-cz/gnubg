/*
 * makebearoff.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1997-1999.
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "getopt.h"
#include "positionid.h"
#include "bearoff.h"

/* ugly fixes */
char *aszRNG[];
char *aszSkillType[ 1 ];
int exsExport;
int ap;

extern bearoffcontext *
BearoffInitBuiltin ( void ) {

  printf ( "Make makebearoff build (avoid resursive rules Makefile :-)\n" );
  return NULL;

}
/* end ugly fixes */

static void
show_usage( const char *arg0 ) {

  printf( "Usage: %s file -n id | -p PosID\n"
          "where file is a bearoff database and id is the number "
          "of the position to dump. Alternatively, give a "
          "position ID.\n", arg0 );

}


extern int
main( int argc, char **argv ) {

  char ch, *filename, *szPosID = NULL;
  int id = 0;
  bearoffcontext *pbc;
  char sz[ 4096 ];
  int n;
  int nUs;
  int nThem;
  int anBoard[ 2 ][ 25 ];
  int fIsPosID = FALSE;

  const struct option ao[] = {
    { "index", required_argument, NULL, 'n' },
    { "posid", required_argument, NULL, 'p' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
  };

  if ( argc != 4 ) {
    show_usage( argv[ 0 ] );
    exit( 1 );
  }

  filename = argv[ 1 ];

  while ( ( ch = getopt_long( argc, argv, "n:p:h", ao, NULL ) )
          != (char) -1 )
  switch( ch ) {
  case 'n': /* position index in db */
    id = atoi( optarg );
    break;
  case 'p': /* position index in db */
    szPosID = optarg;
    fIsPosID = TRUE;
    break;
  case 'h': /* help */
    show_usage( argv[ 0 ] );
    exit( 0 );
    break;
  default:
    show_usage( argv[ 0 ] );
    exit( 1 );
}

  printf( "Bearoff database: %s\n", filename );
  if ( fIsPosID ) {
    printf( "Position ID     : %s\n", szPosID );
  }
  else {
    printf( "Position number : %d\n", id );
  }

  if (  ! ( pbc = BearoffInit ( filename, NULL, BO_NONE, NULL ) ) ) {
    printf( "Failed to initialise bearoff database %s\n", filename );
    exit(-1);
  }

  /* information about bearoff database */

  printf( "\n"
          "Information about database:\n\n" );

  *sz = 0;
  BearoffStatus( pbc, sz );
  puts( sz );

  /* set up board */

  memset( anBoard, 0, sizeof anBoard );

  if ( fIsPosID ) {
    printf( "\n"
            "Dump of position ID: %s\n\n", szPosID );

    PositionFromID( anBoard, szPosID );
  }
  else {
    printf( "\n"
            "Dump of position#: %d\n\n", id );

    n = Combination ( pbc->nPoints + pbc->nChequers, pbc->nPoints );
    nUs = id / n;
    nThem = id % n;
    PositionFromBearoff( anBoard[ 0 ], nThem, pbc->nPoints, pbc->nChequers );
    PositionFromBearoff( anBoard[ 1 ], nUs, pbc->nPoints, pbc->nChequers );
  }

  /* dump req. position */

  *sz = 0;
  BearoffDump( pbc, anBoard, sz );
  puts( sz );

  BearoffClose( &pbc );

  return 0;

}
