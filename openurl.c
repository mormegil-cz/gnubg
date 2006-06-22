/*
 * openurl.c
 *
 * by Jørn Thyssen <jthyssen@dk.ibm.com>, 2002.
 * (after inspiration from osr.cc from fibs2html <fibs2html.sourceforge.net>
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
#include <stdlib.h>
#include <glib.h>

#if USE_GTK
#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#endif

#include "openurl.h"
#include <glib/gi18n.h>
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#include "shellapi.h"
#endif /* WIN32 */

#include "backgammon.h"


#ifndef WIN32

static int
RunCommand( const gchar *szCmd ) {

  if ( system( szCmd ) < 0 ) {
     outputerr( _("Error launching browser\n") );
     return -1;
  }

  return 0;

}

#endif


extern void
OpenURL( const char *szURL ) {

#ifdef WIN32

  ShellExecute( NULL, TEXT("open"), szURL, NULL, ".\\", SW_SHOWMAXIMIZED );

#else /* ! WIN32 */

  gchar *pchBrowser;
  gchar **ppchCommands;
  gchar *pchTemp;
  gchar *pchCommand;
  const gchar *pch;
  int i;
  int rc;

  /*
   * Launch browser
   *
   * Respect $BROWSER
   * (http://www.catb.org/~esr/BROWSER/index.html)
   */

  if ( ( pch = g_getenv( "BROWSER" ) ) )
    /* found $BROWSER */
    pchBrowser = g_strdup( pch );
  else {
    /* use default */
#ifdef __APPLE__
    pchBrowser = g_strdup( "open %s" );
#else
    pchBrowser = g_strdup( "mozilla \"%s\":lynx \"%s\"" );
#endif
  }

  /* loop through commands */

  ppchCommands = g_strsplit( pchBrowser, ":", -1 );

  for ( i = 0; ( pchCommand = ppchCommands[ i ] ); ++i ) {

    if ( ! strstr( pchCommand, "%s" ) ) {
      /* no "%s" in string: add %s */
      gchar *pch = g_strconcat( pchCommand, " \"%s\"", NULL );
      pchTemp = g_strdup_printf( pch, szURL );
      g_free( pch );
    }
    else
      pchTemp = g_strdup_printf( pchCommand, szURL );
    rc = RunCommand( pchTemp );
    g_free( pchTemp );

    if ( !rc )
      break;

  }

  g_strfreev( ppchCommands );

#endif /* ! WIN32 */

}
