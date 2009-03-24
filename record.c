/*
 * record.c
 *
 * by Gary Wong <gtw@gnu.org>, 2002.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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

#if USE_GTK
#include "gtkgame.h"
#else
#include "backgammon.h"
#include <glib.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "record.h"
#include "util.h"

static int nVersion; /* file format when reading player record file */
/* 0: unknown format */
/* 1: original format (locale dependent) */
/* 2: locale independent format, with version header */

extern int RecordReadItem( FILE *pf, char *pch, playerrecord *ppr ) {

    int ch, i;
    int ea;
    char ach[ 80 ];

	do
	{
		do
		{
			ch = getc( pf );
		} while(isspace(ch));

		if( ch == EOF )
		{
			nVersion = 0;

			if( feof( pf ) )
				return 1;
			else
			{
				outputerr( pch );
				return -1;
			}
		}

		if( !nVersion )
		{
			/* we don't know which format we're parsing yet */
			if (fgets( ach, 80, pf ) == NULL)
			{
				nVersion = 0;
				outputerrf( _("%s: invalid record file version"), pch );
				return -1;
			}

			if( ch != '#' || strncmp( ach, " %Version:", 10 ) ) {
				/* no version header; must be version 1 */
				nVersion = 1;
				ch = EOF;	/* reread */
				rewind( pf );
			}
			else if( ( nVersion = atoi( ach + 10 ) ) < 2 )
			{
				nVersion = 0;
				outputerrf( _("%s: invalid record file version"), pch );
				return -1;
			}
		}
	} while (ch == EOF);

    /* read and unescape name */
    i = 0;
    do
	{
		if( ch == EOF )
		{
			if( feof( pf ) )
				outputerrf( _("%s: invalid record file"), pch );
			else
				outputerr( pch );

			nVersion = 0;
			
			return -1;
		}
		else if( ch == '\\' )
		{
			unsigned int part1 = getc( pf ) & 007;
			unsigned int part2 = getc( pf ) & 007;
			unsigned int part3 = getc( pf ) & 007;
			ppr->szName[ i ] = (char)(part1 << 6 | part2 << 3 | part3);
		}
		else
			ppr->szName[ i++ ] = (char)ch;
	} while( i < 31 && !isspace( ch = getc( pf ) ) );
    ppr->szName[ i ] = 0;

    if (fscanf( pf, " %d ", &ppr->cGames ) < 1)
	{
		if( ferror( pf ) )
			outputerr( pch );
		else
			outputerrf( _("%s: invalid record file"), pch );

		nVersion = 0;

		return -1;
	}

	if( ppr->cGames < 0 )
		ppr->cGames = 0;
    
    for( ea = 0; ea < NUM_AVG; ea++ )
	{
			/* Not really cute, but let me guess that this will
				* be cleaned out anytime in the near future. */
		gchar str1[G_ASCII_DTOSTR_BUF_SIZE];
		gchar str2[G_ASCII_DTOSTR_BUF_SIZE];
		gchar str3[G_ASCII_DTOSTR_BUF_SIZE];
		gchar str4[G_ASCII_DTOSTR_BUF_SIZE];

		if( fscanf( pf, "%s %s %s %s ", str1, str2, str3, str4) < 4)
		{
			if( ferror( pf ) )
				outputerr( pch );
			else
				outputerrf( _("%s: invalid record file"), pch );

			nVersion = 0;

			return -1;
		}

		ppr->arErrorChequerplay[ ea ] = g_ascii_strtod(str1, NULL);
		ppr->arErrorCube[ ea ] = g_ascii_strtod(str2, NULL);
		ppr->arErrorCombined[ ea ] = g_ascii_strtod(str3, NULL);
		ppr->arLuck[ ea ] = g_ascii_strtod(str4, NULL);
	}

    return 0;
}

extern void CommandRecordEraseAll( char *notused )
{
    char *sz = g_build_filename(szHomeDirectory, GNUBGPR, NULL);
    
    if( fConfirmSave && !GetInputYN( _("Are you sure you want to erase all "
				       "player records?") ) )
		return;

    if( g_unlink( sz ) && errno != ENOENT )
	{
		/* do not complain if file is not found */
		outputerr( sz );
        g_free(sz);
		return;
    }
    g_free(sz);

    outputl( _("All player records erased.") );
}

extern gboolean records_exist(void)
{
    char *sz = g_build_filename(szHomeDirectory, GNUBGPR, NULL);
    gboolean ret = g_file_test(sz, G_FILE_TEST_IS_REGULAR);
    g_free(sz);
    return ret;
}

extern void CommandRecordShow( char *szPlayer )
{
    FILE *pfIn;
    int f = FALSE;
    playerrecord pr;
    char *sz = g_build_filename(szHomeDirectory, GNUBGPR, NULL);
    
    if( ( pfIn = g_fopen( sz, "r" ) ) == 0 )
	{
		if( errno == ENOENT )
			outputl( _("No player records found.") );
		else
			outputerr( sz );
		g_free(sz);
		return;
    }

#if USE_GTK
    if( fX )
	{
		GTKRecordShow( pfIn, sz, szPlayer );
		return;
	}
#endif
    
    while( !RecordReadItem( pfIn, sz, &pr ) )
	{
		if( !szPlayer || !*szPlayer || !CompareNames( szPlayer, pr.szName ) )
		{
			if( !f )
			{
				outputf("%-31s %-11s %-11s %-12s %-6s %10s\n", "", _("Short-term"), _("Long-term"), _("Total"), _("Total"), "");
				outputf("%-31s %-11s %-11s %-12s %-6s %10s\n", "", _("error rate"), _("error rate"), _("error rate"), _("luck"), "");
				outputf("%-31s %-11s %-11s %-12s %-6s %10s\n", "Name", _("Cheq. Cube"),_("Cheq. Cube"),_("Cheq. Cube"), _("rate"), _("Games"));
				f = TRUE;
			}
			outputf( "%-31s %5.3f %5.3f %5.3f %5.3f %5.3f %5.3f %6.3f %11d\n",
				pr.szName, pr.arErrorChequerplay[ EXPAVG_20 ],
				pr.arErrorCube[ EXPAVG_20 ],
				pr.arErrorChequerplay[ EXPAVG_100 ],
				pr.arErrorCube[ EXPAVG_100 ],
				pr.arErrorChequerplay[ EXPAVG_TOTAL ],
				pr.arErrorCube[ EXPAVG_TOTAL ],
				pr.arLuck[ EXPAVG_TOTAL ], pr.cGames );
		}
	}

    if( ferror( pfIn ) )
		outputerr( sz );
    else if( !f )
	{
		if( szPlayer && *szPlayer )
			outputf( _("No record for player `%s' found.\n"), szPlayer );
		else
			outputl( _("No player records found.") );
    }
    
    fclose( pfIn );
    g_free(sz);
}
