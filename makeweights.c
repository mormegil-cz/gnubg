/*
 * makeweights.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 2000.
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

#include <neuralnet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "i18n.h"

#include "eval.h" /* for WEIGHTS_VERSION */

static void 
usage (char *prog) {
  fprintf (stderr, "Usage: %s [ -f filename]\n"
	   "  -f filename  Output to file instead of stdout\n",
	   prog);

  exit (1);
}

extern int main( int argc, char *argv[] ) {

    neuralnet nn;
    char szFileVersion[ 16 ];
    static float ar[ 2 ] = { WEIGHTS_MAGIC_BINARY, WEIGHTS_VERSION_BINARY };
    int c;
    FILE *output = stdout;

    switch (argc) {
    case 1:
      break;

    case 3:
      if (strcmp (argv[1], "-f") != 0)
	usage (argv[0]);

      if ((output = fopen (argv[2], "wb")) == 0) {
	perror ("Can't open output file");
	exit (1);
      }
	
      break;

    default:
      usage (argv[0]);
    }

    /* i18n */

#if HAVE_SETLOCALE
    setlocale (LC_ALL, "");
#endif
    bindtextdomain (PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);

    /* generate weights */
    
    if( scanf( "GNU Backgammon %15s\n", szFileVersion ) != 1 ) {
	fprintf( stderr, _("%s: invalid weights file\n"), argv[ 0 ] );
	return EXIT_FAILURE;
    }

    if( strcmp( szFileVersion, WEIGHTS_VERSION ) ) {
      fprintf( stderr, _("%s: incorrect weights version\n"
                         "(version %s is required, "
                         "but these weights are %s)\n" ),
               argv[ 0 ], WEIGHTS_VERSION, szFileVersion );
      return EXIT_FAILURE;
    }

	
    fwrite( ar, sizeof( ar[ 0 ] ), 2, output );

    PushLocale ( "C" );

    for( c = 0; !NeuralNetLoad( &nn, stdin ); c++ )
      if( NeuralNetSaveBinary( &nn, output ) )
	    return EXIT_FAILURE;

    PopLocale ();

    fprintf( stderr, _("%d nets converted\n"), c );

    return EXIT_SUCCESS;

}






