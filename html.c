/*
 * html.c
 *
 * by Joern Thyssen  <jthyssen@dk.ibm.com>, 2002
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
#include <string.h>

#include "backgammon.h"
#include "drawboard.h"
#include "export.h"

extern void CommandExportGameHtml( char *sz ) {

    FILE *pf;
    
    sz = NextToken( &sz );
    
    if( !plGame ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export"
		 "game html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /*
    LaTeXPrologue( pf );
    
    ExportGameLaTeX( pf, plGame );

    LaTeXEpilogue( pf );
    */
    
    if( pf != stdout )
	fclose( pf );

}

extern void CommandExportMatchHtml( char *sz ) {
    
    FILE *pf;
    list *pl;

    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "match html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /* LaTeXPrologue( pf ); */

    /* FIXME write match introduction? */
    
    for( pl = lMatch.plNext; pl != &lMatch; pl = pl->plNext )
      /* ExportGameLaTeX( pf, pl->p ) */; 
    
    /* LaTeXEpilogue( pf ); */
    
    if( pf != stdout )
	fclose( pf );
}


extern void CommandExportPositionHtml( char *sz ) {

    FILE *pf;
    char szTitle[ 32 ];
	
    sz = NextToken( &sz );
    
    if( ms.gs == GAME_NONE ) {
	outputl( "No game in progress (type `new game' to start one)." );
	return;
    }
    
    if( !sz || !*sz ) {
	outputl( "You must specify a file to export to (see `help export "
		 "position html')." );
	return;
    }

    if( !strcmp( sz, "-" ) )
	pf = stdout;
    else if( !( pf = fopen( sz, "w" ) ) ) {
	perror( sz );
	return;
    }

    /*
    fPDF = FALSE;
    sprintf( szTitle, "Position %s", PositionID( ms.anBoard ) );
    PostScriptPrologue( pf, TRUE, szTitle );

    PrintPostScriptBoard( pf, &ms, ms.fTurn );
    
    PostScriptEpilogue( pf );
    */

    if( pf != stdout )
	fclose( pf );
}

