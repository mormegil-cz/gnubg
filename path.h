/*
 * path.h
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1998-2001.
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

#ifndef _PATH_H_
#define _PATH_H_

extern char *PathSearch( const char *szFile, const char *szDir );

extern int 
PathOpen( const char *szFile, const char *szDir, const int f );

extern int
BackupFile ( const char *sz );

#endif
