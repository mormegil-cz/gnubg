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

#if !HAVE_CONFIG_H
/* Compiling standalone; assume all dependencies are satisfied. */
#define HAVE_LIBXML2 1
#define USE_GTK2 1
#endif

#if HAVE_LIBXML2 && USE_GTK2

#define HAVE_GTKTEXI 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_TEXI (gtk_texi_get_type())
#define GTK_TEXI(t) (GTK_CHECK_CAST( (t), GTK_TYPE_TEXI, GtkTexi))
#define GTK_TEXI_CLASS(c) (GTK_CHECK_CLASS_CAST( (c), GTK_TYPE_TEXI, GtkTexiClass))
#define GTK_IS_TEXI(t) (GTK_CHECK_TYPE( (t), GTK_TYPE_TEXI ))
#define GTK_IS_TEXI_CLASS(c) (GTK_CHECK_CLASS_TYPE( (c), GTK_TYPE_TEXI ))
#define GTK_TEXI_GET_CLASS (GTK_CHECK_GET_CLASS( (c), GTK_TYPE_TEXI, GtkTexiClass))

typedef struct _gtktexicontext gtktexicontext;

typedef struct _GtkTexi {
    GtkWindow parent_instance;
    GtkWidget *pwText, *pwScrolled, *apwLabel[ 3 ], *apwButton[ 2 ],
	*apwNavMenu[ 3 ], *apwGoMenu[ 2 ];
    GtkTextBuffer *ptb;
    GtkItemFactory *pif;
    gtktexicontext *ptic;
} GtkTexi;

typedef struct _GtkTexiClass {
    GtkWindowClass parent_class;
} GtkTexiClass;

extern GtkType gtk_texi_get_type( void );
extern GtkWidget *gtk_texi_new( void );
extern int gtk_texi_load( GtkTexi *pw, char *szFile );
extern int gtk_texi_render_node( GtkTexi *pw, char *szTag );

G_END_DECLS

#endif
#endif
