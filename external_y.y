%{
  /*
   * command_y.y -- command parser for gnubg server
   *
   */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "external.h"

extcmd ec; 

static void reset_command();

void ( *ExtErrorHandler )( char *, char *, int ) = NULL;


  %}

%union {
  int number;
  char *sval;
}

%token <sval> STRING
%token <number> NUMBER
%token EVALUATION PLIES CUBE CUBEFUL CUBELESS NOISE REDUCED
%token FIBSBOARD
%token <sval> AFIBSBOARD
%token ON
%token OFF

%name-prefix="ext"

%%

the_command    : /* empty */  
               | reset_command command      
               ;

command        : evaluation
               | cmdfibsboard
               ;

reset_command  : /* empty */ {
  reset_command();
}
;
 
cmdfibsboard   : AFIBSBOARD { 
  ec.ct = COMMAND_FIBSBOARD;
  g_free( ec.szFIBSBoard );
  ec.szFIBSBoard = g_strdup( $1 );
}
               ;

fibsboard      : AFIBSBOARD { 
  g_free( ec.szFIBSBoard );
  ec.szFIBSBoard = g_strdup( $1 );
}
               ;

optplies       : PLIES NUMBER { ec.nPlies = $2; }
               | /* empty */
               ;

optcube        : CUBE ON  { ec.fCubeful = TRUE; }
               | CUBE OFF { ec.fCubeful = FALSE; }
               | /* empty */
               ;

optcubeful     : CUBEFUL { ec.fCubeful = TRUE; }
               | /* empty */
               ;

optcubeless    : CUBELESS { ec.fCubeful = FALSE; }
               | /* empty */
               ;

optnoise       : NOISE NUMBER /* FIXME: FLOAT */ { ec.rNoise = $2; }
               | /* empty */
               ;

optreduced     : REDUCED NUMBER { ec.nReduced = $2; };
               | /* empty */
               ;

evalcontext    : optplies optcube optcubeful optcubeless optnoise optreduced
               ;

optevalcontext : evalcontext
               | /* empty */
               ;

evaluation     : EVALUATION FIBSBOARD fibsboard optevalcontext { 
  ec.ct = COMMAND_EVALUATION;
}
;

%%

/* lexer interface */

extern FILE *extin;
extern char *exttext;

extern int
exterror( const char *s ) {

  if ( ExtErrorHandler )
    ExtErrorHandler( s, exttext && exttext[ 0 ] ? exttext : "<EOT>", 1);
  else
    fprintf( "Error: %s at %s\n", 
             s, exttext && exttext[ 0 ] ? exttext : "<EOT>" );
}

static void
reset_command() {

  ec.ct = COMMAND_NONE;
  ec.nPlies = 0;
  ec.rNoise = 0;
  ec.fDeterministic = 1;
  ec.fCubeful = 0;
  ec.nReduced = 0;
  ec.szFIBSBoard = NULL;

}


#if EXTERNAL_TEST

extern int
main( int argc, char *argv[] ) {

  FILE *pf = NULL;

  if ( argc > 1 )
    if ( ! ( pf = fopen( argv[ 1 ], "r" ) ) ) {
      perror( argv[ 1 ] );
      return 1;
    }

  extin = pf ? pf : stdin;
  extparse();
  fclose( extin );

  printf( "command type %d\n"
          "plies %d\n"
          "noise %f\n"
          "deterministic %d\n"
          "cubeful %d\n"
          "reduced %d\n"
          "fibsboard %s\n",
          ec.ct, ec.nPlies, ec.rNoise, ec.fDeterministic, ec.fCubeful,
          ec.nReduced, ec.szFIBSBoard );

  return 0;

}

#endif
