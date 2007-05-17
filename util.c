/*
 * util.c
 *
 * by Christian Anthon 2007
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
char *aszRNG[1]; 
char *aszSkillType[ 1 ]; 
int exsExport;
int ap;
#if WIN32
#include <windows.h>
#include <glib/glib.h>
#include <glib/gstdio.h>
extern char * getInstallDir( void ) {

  char buf[_MAX_PATH];
  char *p;
  static char *ret = NULL;
  if (ret)
	  return (ret);
  GetModuleFileName(NULL, buf, sizeof(buf));
  p = MAX(strrchr(buf, '/'), strrchr(buf, '\\'));
  if (p)
	  *p = '\0';
  ret = g_strdup(buf);
  return ret;
}

#endif

extern void MT_Lock(long *lock)
{
}

extern void MT_Unlock(long *lock)
{
}

extern int MT_GetThreadID()
{
  return (0);
}

