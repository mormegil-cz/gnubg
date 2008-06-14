/*
 * util.h
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "stdio.h"

#ifdef WIN32
#define DOCDIR getDocDir()
#define PKGDATADIR getInstallDir()
#endif

#define BuildFilename(file) g_build_filename(PKGDATADIR, file, NULL)
#define BuildFilename2(file1, file2) g_build_filename(PKGDATADIR, file1, file2, NULL)

#ifdef WIN32
extern char *getInstallDir( void );
extern char *getDocDir( void );
#endif

extern void PrintSystemError(const char* message);
extern void PrintError(const char* message);
extern FILE *GetTemporaryFile(const char *nameTemplate, char **retName);

#endif
