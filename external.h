/*
 * external.h
 *
 * by Gary Wong <gtw@gnu.org>, 2001.
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

#ifndef _EXTERNAL_H_
#define _EXTERNAL_H_

#if HAVE_SOCKETS

#ifndef WIN32

#if HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#endif /* #if HAVE_SYS_SOCKET_H */

#else /* #ifndef WIN32 */
#include <winsock.h>
#endif /* #ifndef WIN32 */

extern int ExternalSocket( struct sockaddr **ppsa, int *pcb, char *sz );
extern int ExternalRead( int h, char *pch, int cch );
extern int ExternalWrite( int h, char *pch, int cch );

#endif /* #if HAVE_SOCKETS */

#endif /* #ifndef _EXTERNAL_H_ */
