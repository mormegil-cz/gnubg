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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#include "eval.h"
#include "positionid.h"
#include "getopt.h"
#include "i18n.h"
#include "bearoff.h"

static void
PrintPre ( FILE *pf ) {

  fputs (
         "/*\n"
         " * br1.c\n"
         " *\n"
         " * by Joern Thyssen <jthyssen@dk.ibm.com>, 2002.\n"
         " *\n"
         " * This program is free software; you can redistribute it and/or modify\n"
         " * it under the terms of version 2 of the GNU General Public License as\n"
         " * published by the Free Software Foundation.\n"
         " *\n"
         " * This program is distributed in the hope that it will be useful,\n"
         " * but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
         " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
         " * GNU General Public License for more details.\n"
         " *\n"
         " * You should have received a copy of the GNU General Public License\n"
         " * along with this program; if not, write to the Free Software\n"
         " * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
         " *\n"
         " * $Id$\n"
         " */\n"
         "\n\n\n"
         "#include <stdio.h>\n"
         "#include <stdlib.h>\n\n"
         "#include <string.h>\n\n"
         "#include \"config.h\"\n"
         "#include \"md5.h\"\n"
         "#include \"i18n.h\"\n"
         "#include \"bearoff.h\"\n\n\n",
         pf );
}


static void 
PrintCode ( FILE *pf ) {

  fputs ( "extern bearoffcontext *\n"
          "BearoffInitBuiltin ( void ) {\n"
          "\n"
          "  bearoffcontext *pbc;\n"
          "  static unsigned char achCorrect[ 16 ] = {\n"
          "    0x3D, 0xC7, 0xB8, 0x33, 0xC4, 0x67, 0x08, 0x49, \n"
          "    0xCE, 0xE0, 0x04, 0x79, 0xA9, 0xE2, 0x1B, 0x49 };\n"
          "  unsigned char ach[ 16 ];\n"
          "\n"
          "     /* check that the file is OK */\n"
          "\n"
          "  md5_buffer ( acBearoff1, sizeof ( acBearoff1 ), ach );\n"
          "  if ( memcmp ( ach, achCorrect, 16 )  ) {\n"
          "    fprintf ( stderr, _(\"Built-in database is not valid!\\n\") );\n"
          "    return NULL;\n"
          "  }\n"
          "\n"
          "\n"
          "  if ( ! ( pbc = (bearoffcontext *) malloc ( sizeof ( bearoffcontext ) ) ) ) {\n"
          "    /* malloc failed */\n"
          "    perror ( \"bearoffcontext\" );\n"
          "    return NULL;\n"
          "  }\n"
          "  \n"
          "  pbc->bt = BEAROFF_GNUBG;\n"
          "  pbc->fInMemory = TRUE;\n"
          "  pbc->h = -1;\n"
          "  pbc->nPoints = 6;\n"
          "  pbc->nChequers = 15;\n"
          "  pbc->fTwoSided = FALSE;\n"
          "  pbc->fMalloc = FALSE;\n"
          "  pbc->fCompressed = TRUE;\n"
          "  pbc->fGammon = TRUE;\n"
          "  pbc->fND = FALSE;\n"
          "  pbc->fCubeful = FALSE;\n"
          "  pbc->nReads = 0;\n"
          "  pbc->fHeuristic = FALSE;\n"
          "  pbc->p = (void *) acBearoff1;\n"
          "\n"
          "  return pbc;\n"
          "}\n\n\n",
          pf );

}


static void
usage ( char *arg0 ) {

  printf ( "usage:\n" );

}

static void
version ( void ) {

  printf ( "version:\n" );

}

extern int
main ( int argc, char **argv ) {

  char *szOutput = NULL;
  char *szInput = NULL;
  FILE *pfOut, *pfIn;
  char ch;
  char ac[ 128 ];
  int n, i;

  static struct option ao[] = {
    { "output", required_argument, NULL, 'o' },
    { "input", required_argument, NULL, 'i' },
    { "help", no_argument, NULL, 'h' },
    { "version", no_argument, NULL, 'v' },
    { NULL, 0, NULL, 0 }
  };

  while ( ( ch = getopt_long ( argc, argv, "o:i:hv", ao, NULL ) ) !=
          (char) -1 ) {
    switch ( ch ) {
    case 'o': /* output file */
      szOutput = strdup ( optarg );
      break;
    case 'i': /* input file */
      szInput = strdup ( optarg );
      break;
    case 'h': /* help */
      usage ( argv[ 0 ] );
      exit ( 0 );
      break;
    case 'v': /* version */
      version ();
      exit ( 0 );
      break;
    default:
      usage ( argv[ 0 ] );
      exit ( 1 );
    }
  }

  if ( ! szOutput || ! strcmp ( szOutput, "-" ) )
    pfOut = stdout;
  else if ( ! ( pfOut = fopen ( szOutput, "wb" ) ) ) {
    perror ( szOutput );
    exit(1);
  }

  if ( ! szInput || ! strcmp ( szInput, "-" ) )
    pfIn = stdin;
  else if ( ! ( pfIn = fopen ( szInput, "rb" ) ) ) {
    perror ( szInput );
    exit(1);
  }

  /* copy input byte for byte into output */

  PrintPre ( pfOut );

  fprintf ( pfOut, 
            "static unsigned char acBearoff1[] = { \n" 
            "   " );

  while ( ! feof ( pfIn ) && ( n = fread ( ac, 1, sizeof ( ac ), pfIn ) ) ) {

    for ( i = 0; i < n; ++i ) {

      fprintf ( pfOut, "0x%02hhX, ", (ac[ i ] & 0xff) );

      if ( ! ( ( i + 1 ) % 8 ) )
        fprintf ( pfOut, 
                  "\n"
                  "   " );

    }

  }

  fprintf ( pfOut, 
              "\n"
              "};\n\n\n" );

  PrintCode ( pfOut );

  if ( pfOut != stdout )
    fclose ( pfOut );
  if ( pfIn != stdin )
    fclose ( pfIn );

  return 0;

}

