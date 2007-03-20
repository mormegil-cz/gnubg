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
#include "backgammon.h"
#include <glib/gi18n.h>
#include "openurl.h"
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#include "shellapi.h"
#endif /* WIN32 */
static gchar *web_browser = NULL;

extern gchar *
get_web_browser (void)
{
  const gchar *pch;
#ifdef WIN32
  if (!web_browser)
    return NULL;
#endif
  if (web_browser && *web_browser)
    return web_browser;
  if (!(pch = g_getenv ("BROWSER")))
#ifdef __APPLE__
    pch = g_strdup ("open");
#else
    pch = g_strdup ("firefox");
#endif
  set_web_browser (pch);
  return web_browser;
}


extern char *
set_web_browser (const char *sz)
{
  g_free (web_browser);
  web_browser = g_strdup (sz ? sz : "");
  return web_browser;
};

extern void
OpenURL (const char *szURL)
{
  gchar *command;
  GError *error = NULL;
  gchar *browser = get_web_browser ();
#ifdef WIN32
  if (!(browser) || !(*browser))
    {
      ShellExecute (NULL, TEXT ("open"), szURL, NULL, ".\\",
		    SW_SHOWMAXIMIZED);
      return;
    }
#else
  command = g_strdup_printf ("%s %s", browser, szURL);
  if (!g_spawn_command_line_async (command, &error))
    {
      g_printerr ("browser couldn't open file (%s): %s\n",
		  command, error->message);
      g_error_free (error);
    }
  return;
#endif
}
