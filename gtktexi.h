/*
 * gtktexi.h
 *
 * by Gary Wong <gtw@gnu.org>, 2002
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

#ifndef _GTKTEXI_H_
#define _GTKTEXI_H_

#include <gtk/gtk.h>

typedef struct _texinfocontext texinfocontext;

extern texinfocontext *gtktexi_create( void );
extern GtkTextBuffer *gtktexi_get_buffer( texinfocontext *ptic );
extern int gtktexi_load( texinfocontext *ptic, char *szFile );
extern int gtktexi_render_node( texinfocontext *ptic, char *szTag );
extern void gtktexi_destroy( texinfocontext *ptic );

#endif
