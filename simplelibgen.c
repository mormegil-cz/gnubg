/*
 * simplelibgen.c
 *
 * by Øystein Johansen <ojohans@statoil.com>, 2002.
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
 * $Id $
 */

/*
 * This code is taken from glibc-2.2, and modified for Windows systems.
 */

#include <string.h>

#ifdef WIN32
#define DIR_SEPARATOR  '\\'
#define DIR_SEPARATOR_S  "\\"
#else
#define DIR_SEPARATOR  '/'
#define DIR_SEPARATOR_S  "/"
#endif

extern char *
basename (const char *filename) 
{ 
  char *p1 = strrchr (filename, DIR_SEPARATOR); 
  return p1 ? p1 + 1 : (char *) filename;
} 

extern char *
dirname (char *path)
{
  static const char dot[] = ".";
  char *last_slash;

  /* Find last DIR_SEPARATOR.  */
  last_slash = path != NULL ? strrchr (path, DIR_SEPARATOR) : NULL;

  if (last_slash != NULL && last_slash != path && last_slash[1] == '\0')
    {
      /* Determine whether all remaining characters are slashes.  */
      char *runp;

      for (runp = last_slash; runp != path; --runp)
	if (runp[-1] != DIR_SEPARATOR)
	  break;

      /* The DIR_SEPARATOR is the last character, we have to look further.  */
      //if (runp != path)
//	last_slash = __memrchr (path, DIR_SEPARATOR, runp - path);
    }

  if (last_slash != NULL)
    {
      /* Determine whether all remaining characters are slashes.  */
      char *runp;

      for (runp = last_slash; runp != path; --runp)
	if (runp[-1] != DIR_SEPARATOR)
	  break;

      /* Terminate the path.  */
      if (runp == path)
	{
	  /* The last slash is the first character in the string.  We have to
	     return "/".  As a special case we have to return "//" if there
	     are exactly two slashes at the beginning of the string.  See
	     XBD 4.10 Path Name Resolution for more information.  */
	  if (last_slash == path + 1)
	    ++last_slash;
	  else
	    last_slash = path + 1;
	}

      last_slash[0] = '\0';
    }
  else
    /* This assignment is ill-designed but the XPG specs require to
       return a string containing "." in any case no directory part is
       found and so a static and constant string is required.  */
    path = (char *) dot;

  return path;
}

