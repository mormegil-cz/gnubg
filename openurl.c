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

#if USE_GTK
#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#if !GTK_CHECK_VERSION(1,3,10)
#include <stdlib.h>
#endif
#endif

#include "openurl.h"
#include "i18n.h"
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#include "shellapi.h"
#endif /* WIN32 */

#include "backgammon.h"


extern void
OpenURL( const char *szURL ) {

#ifdef WIN32

  ShellExecute( NULL, TEXT("open"), szURL, NULL, ".\\", SW_SHOWMAXIMIZED );

#else /* ! WIN32 */

  /* FIXME: implement other browsers */

#ifdef __APPLE__
#define BROWSERCOMMAND "open %s"
#else
#define BROWSERCOMMAND "mozilla \"%s\""
#endif

#if GTK_CHECK_VERSION(1,3,10)

  gchar *pchCommand;
  GError *error = NULL;

  pchCommand = g_strdup_printf( BROWSERCOMMAND, szURL );

  if ( ! g_spawn_command_line_async( pchCommand, &error ) ) {
    outputerrf( _("Error launching browser: %s\n"), error->message );
    g_error_free( error );
  }

  g_free( pchCommand );

#else /* GTK 1.3 */

   
  gchar *pchCommand;
  pchCommand = g_strdup_printf( BROWSERCOMMAND, szURL );

  if ( system( pchCommand ) < 0 ) 
     outputerr( _("Error launching browser\n") );

  g_free( pchCommand );

#endif /* ! GTK 1.3 */

#endif /* ! WIN32 */

}
