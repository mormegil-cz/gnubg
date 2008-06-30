/*
 * util.c
 *
 * by Christian Anthon 2007
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
#include "util.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>

extern char * getInstallDir( void )
{
  static char *ret = NULL;
  if (!ret)
  {
	char buf[FILENAME_MAX];
	if (GetModuleFileName(NULL, buf, sizeof(buf)) != 0)
	{
		char *p1 = strrchr(buf, '/'), *p2 = strrchr(buf, '\\');
		int pos1 = (p1 != NULL) ? (int)(p1 - buf) : -1;
		int pos2 = (p2 != NULL) ? (int)(p2 - buf) : -1;
		int pos = MAX(pos1, pos2);
		if (pos > 0)
			buf[pos] = '\0';
		ret = g_strdup(buf);
	}
  }
  return ret;
}

extern char *getDocDir(void)
{
	static char *docdir = NULL;
	if (!docdir)
		docdir = g_build_filename(getInstallDir(), "doc", NULL);
	return docdir;
}

extern void PrintSystemError(const char* message)
{
	LPVOID lpMsgBuf;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS ,
		NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf, 0, NULL) != 0)
	{
		g_print("** Windows error while %s **\n", message);
		g_print(": %s", (LPCTSTR)lpMsgBuf);

		LocalFree(lpMsgBuf);
	}
}
#else
#include <errno.h>

extern void PrintSystemError(const char* message)
{
	printf("Unknown system error while %s!\n", message);
}
#endif

void PrintError(const char* str)
{
	g_printerr("%s: %s", str, strerror(errno));
}

/* Non-Ansi compliant function */
#ifdef __STRICT_ANSI__
FILE *fdopen(int, const char *);
#endif

extern FILE *GetTemporaryFile(const char *nameTemplate, char **retName)
{
    FILE *pf;
    int tmpd = g_file_open_tmp(nameTemplate, retName, NULL); 
    if (tmpd < 0)
	{
      PrintError("creating temporary file");
      return NULL;
    }
	pf = fdopen(tmpd, "w+" );
	if (pf == NULL)
	{
		g_free(retName);
		PrintError("opening temporary file");
		return NULL;
	}
	else
		return pf;
}
