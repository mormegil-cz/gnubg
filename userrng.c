/*
 * UserRNG.c
 *
 * Template for user supplied RNG.
 * Compile to shared library, eg. under GNU/Linux:
 *
 * gcc -O2 -g -shared -o userrng.so userrng.c
 *
 * Your should supply two functions setseed and getrandom:
 */
#ifdef UNDEF

extern void setseed ( unsigned long int seed );
extern long int getrandom ( void );

extern void setseed ( unsigned long int seed ) {

  /* Fill in your function that sets seed */

}

extern long int getrandom ( void ) {

  /* 
   * Fill in your RNG...
   * Function that return a (pseudo-)random 
   * long int.
   * Examples could be:
   * - The FIBS dice code
   * - Get random numbers from <URI:"http://www.random.org">
   *   (see further below)
   */

}
#endif
/*
 *
 * by Joern Thyssen, 1999
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

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>

extern void setseed ( unsigned long int seed );
extern long int getrandom ( void );

extern void setseed ( unsigned long int seed ) {

  /* Set seed */

  printf( "No need to set seed when using www.random.org\n" );

}

extern long int getrandom ( void ) {

  /* Get random numbers from www.random.org
   * Part of the code is based on inspiration from GNU wget.
   */

#define BUFLENGTH 500

  static int anCurrent = -1;
  static int anBuf [ BUFLENGTH ];
  static int anSocket;

  int anBytesRead, i, j;
  struct hostent *psHostEntry;
  struct sockaddr_in asSockName;
  char szHostname[] = "www.random.org";
  char acBuf [ 2048 ];

  /* 
   * Suggestions for improvements:
   * - use proxy
   */

  /*
   * Return random number
   */

  printf("anCurrent %i\n",anCurrent);
  if ( (anCurrent >= 0) && (anCurrent < BUFLENGTH) ) {
     i = anCurrent;
     anCurrent++;
     return anBuf [ i ];
  }

  /*
   * Initialize connection
   */

  if ( anCurrent == -1 ) {

    /*
     * Get IP address for www.random.org
     */

    psHostEntry = gethostbyname( szHostname );

    if ( psHostEntry == NULL )  {

      printf ( "Error getting IP address of %s\n", szHostname );
      exit(1);
    }

    /*
     * Set socket
     */

    /* Copy address of the host to socket */

    memcpy( &asSockName.sin_addr,
           psHostEntry->h_addr, psHostEntry->h_length );

    /* Set port and protocol */

    asSockName.sin_family = AF_INET;
    asSockName.sin_port = htons( 80 );

    /*
     * Make socket
     */

    if ( (anSocket = socket (AF_INET, SOCK_STREAM, 0)) == -1 ) {
       printf ( "could not open socket.\n" );
       exit(1);
    }

    /*
     * Connect the socket to the remote host
     */

    if ( connect( anSocket,
                  (struct sockaddr *) &asSockName,
                  sizeof( asSockName ) ) ) {

        printf ( "could not connect to host\n" );
        exit(1);

    }

    anCurrent = BUFLENGTH;


  } /* anCurrent == -1 */

  /*
   * Buffer is empy, get new data from www.random.org
   */

  if ( anCurrent == BUFLENGTH ) {

    strcpy( acBuf, "GET http://www.random.org/cgi-bin/randnum?num=500&min=0&max=5&col=1\n" );

    printf( "reading 500 random numbers from %s.\n", szHostname );


    /* write request to web-server */
    write( anSocket, acBuf, strlen(acBuf) );

    /* get data from web-server */
    anBytesRead = read ( anSocket, acBuf, sizeof ( acBuf ) );

    if ( !anBytesRead ) {
      printf( "Error reading data from www.random.org\n" );
      exit(1);
    }

    i = 0; j = 0;
    for ( i = 0; i < anBytesRead ; i++ ) {

      if ( ( acBuf[ i ] >= '0' ) && ( acBuf[ i ] <= '5' ) ) {
         anBuf[ j ] = (int) (acBuf[ i ] - '0');
         j++;
      }

    }

    anCurrent = 1;
    return anBuf[ 0 ];
  }

}
    





