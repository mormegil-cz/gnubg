
/*
 * osr.c
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

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>

#include "config.h"

#include "openurl.h"
#include "i18n.h"


#ifdef WIN32

extern void
OpenURL( const char *szURL ) {

  ShellExecute(NULL, NULL, szURL, NULL, ".\\", 0);

}

#else /* WIN32 */


extern void
OpenURL( const char *szURL ) {

  /* FIXME: implement other browsers */

  gchar *pchCommand;
  GError *error = NULL;

  pchCommand = g_strdup_printf( "mozilla \"%s\"", szURL );

  if ( ! g_spawn_command_line_async( pchCommand, &error ) ) {
    outputerr( _("Error launching browser: %s\n"), error->message );
    g_error_free( error );
  }

  g_free( pchCommand );

}



#endif /* ! WIN32 */
