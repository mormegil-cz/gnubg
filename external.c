/*
 * external.c
 *
 * by Gary Wong <gtw@gnu.org>, 2001.
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

#include <errno.h>
#include <signal.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
#include "external.h"

extern int ExternalRead( int h, char *pch, int cch ) {
#if HAVE_SOCKETS
    char *p = pch, *pEnd;
    int n;
    psighandler sh;
    
    while( cch ) {
	if( fAction )
	    fnAction();

	if( fInterrupt )
	    return -1;

	PortableSignal( SIGPIPE, SIG_IGN, &sh );
	n = read( h, p, cch );
	PortableSignalRestore( SIGPIPE, &sh );
	
	if( !n ) {
	    outputl( "External connection closed." );
	    return -1;
	} else if( n < 0 ) {
	    if( errno == EINTR )
		continue;

	    perror( "external connection" );
	    return -1;
	}
	
	if( ( pEnd = memchr( p, '\n', n ) ) ) {
	    *pEnd = 0;
	    return 0;
	}
	
	cch -= n;
	p += n;
	
    }

    p[ cch - 1 ] = 0;
    return 0;
#else
    assert( FALSE );
#endif
}

extern int ExternalWrite( int h, char *pch, int cch ) {
#if HAVE_SOCKETS
    char *p = pch;
    int n;
    psighandler sh;

    while( cch ) {
	if( fAction )
	    fnAction();

	if( fInterrupt )
	    return -1;

	PortableSignal( SIGPIPE, SIG_IGN, &sh );
	n = write( h, p, cch );
	PortableSignalRestore( SIGPIPE, &sh );
	
	if( !n )
	    return 0;
	else if( n < 0 ) {
	    if( errno == EINTR )
		continue;

	    perror( "external connection" );
	    return -1;
	}
	
	cch -= n;
	p += n;
    }

    return 0;
#else
    assert( FALSE );
#endif
}

extern void CommandExternal( char *sz ) {

#if !HAVE_SOCKETS
    outputl( "This installation of GNU Backgammon was compiled without\n"
	     "socket support, and does not implement external controllers." );
#else
    int h, hPeer;
    struct sockaddr_un sun;
    char szCommand[ 256 ], szResponse[ 32 ];
    char szName[ 32 ], szOpp[ 32 ];
    int anBoard[ 2 ][ 25 ], nMatchTo, anScore[ 2 ], anDice[ 2 ],
	nCube, fCubeOwner, fDoubled, fTurn, fCrawford, anMove[ 8 ];
    cubeinfo ci;
    
    if( !sz || !*sz ) {
	outputl( "You must specify the name of the socket to the external\n"
		 "controller -- try `help external'." );
	return;
    }

    if( ( h = socket( PF_LOCAL, SOCK_STREAM, 0 ) ) < 0 ) {
	perror( "socket" );
	return;
    }

    sun.sun_family = AF_LOCAL;
    strcpy( sun.sun_path, sz ); /* yuck!  opportunities for buffer overflow
				    here... but we didn't write the broken
				    interface */

    if( bind( h, (struct sockaddr *) &sun, SUN_LEN( &sun ) ) < 0 ) {
	perror( sz );
	close( h );
	return;
    }
    
    if( listen( h, 1 ) < 0 ) {
	perror( "listen" );
	close( h );
	return;
    }

    while( ( hPeer = accept( h, NULL, NULL ) ) < 0 ) {
	if( errno == EINTR ) {
	    if( fAction )
		fnAction();

	    if( fInterrupt ) {
		close( h );
		return;
	    }
	    
	    continue;
	}
	
	perror( "accept" );
	close( h );
	return;
    }

    close( h );
    unlink( sz );

    while( !ExternalRead( hPeer, szCommand, sizeof( szCommand ) ) )
	if( ParseFIBSBoard( szCommand, anBoard, szName, szOpp, &nMatchTo,
			     anScore, anScore + 1, anDice, &nCube,
			    &fCubeOwner, &fDoubled, &fTurn, &fCrawford ) )
	    outputl( "Warning: badly formed board from external controller." );
	else {
	    /* FIXME check if this is a double/take/resign decision */
	    
	    if( !fTurn )
		SwapSides( anBoard );
	    
	    SetCubeInfo ( &ci, nCube, fCubeOwner, fTurn, nMatchTo, anScore,
			  fCrawford, fJacoby, fBeavers );
	    
	    if( FindBestMove( anMove, anDice[ 0 ], anDice[ 1 ],
			      anBoard, &ci, &ecEval ) < 0 )
		break;

	    FormatMove( szResponse, anBoard, anMove );
	    strcat( szResponse, "\n" );
	    
	    if( ExternalWrite( hPeer, szResponse, strlen( szResponse ) ) )
		break;
	}

    close( hPeer );
#endif
}
