/*
 * gtksplash.c
 *
 * by Joern Thyssen <jth@gnubg.org>, 2002
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backgammon.h"
#include "eval.h"
#include "gtksplash.h"
#include "gtkboard.h"
#include "i18n.h"

#if GTK_CHECK_VERSION(2,0,0)
#define USLEEP(x) g_usleep(x)
#elif HAVE_USLEEP
#define USLEEP(x) usleep(x)
#else
#define USLEEP(x)
#endif

typedef struct _gtksplash {
  GtkWidget *pwWindow;
  GtkWidget *apwStatus[ 2 ];
} gtksplash;


extern GtkWidget *
CreateSplash () {

  gtksplash *pgs;
  GtkWidget *pwvbox, *pwFrame, *pwb;
  GtkWidget *pwImage;
  int i;
#include "xpm/gnubg-big.xpm"

  pgs = (gtksplash *) g_malloc ( sizeof ( gtksplash ) );

  pgs->pwWindow = gtk_window_new ( GTK_WINDOW_TOPLEVEL );
#if GTK_CHECK_VERSION(2,0,0)
  gtk_window_set_role( GTK_WINDOW( pgs->pwWindow ),
		       "splash screen" );
#if GTK_CHECK_VERSION(2,2,0)
  gtk_window_set_type_hint( GTK_WINDOW( pgs->pwWindow ),
			    GDK_WINDOW_TYPE_HINT_SPLASHSCREEN );
#endif
#endif
  gtk_window_set_title ( GTK_WINDOW ( pgs->pwWindow ), 
                         _("Starting GNU Backgammon " VERSION ) );
  gtk_window_set_position ( GTK_WINDOW ( pgs->pwWindow ), GTK_WIN_POS_CENTER );
  
  gtk_widget_realize ( GTK_WIDGET ( pgs->pwWindow ) );

  /* gdk_window_set_decorations ( GTK_WIDGET ( pgs->pwWindow )->window, 0 ); */

  /* content of page */
                               
  pwvbox = gtk_vbox_new ( FALSE, 0 );
  gtk_container_add ( GTK_CONTAINER ( pgs->pwWindow ), pwvbox );

  /* image */

  pwImage = image_from_xpm_d ( gnubg_big_xpm, GTK_WIDGET ( pgs->pwWindow ) );
  gtk_box_pack_start ( GTK_BOX ( pwvbox ), pwImage, FALSE, FALSE, 0 );

  gtk_box_pack_start( GTK_BOX( pwvbox ),
		      pwFrame = gtk_frame_new( NULL ), FALSE, FALSE, 0 );
  gtk_frame_set_shadow_type( GTK_FRAME( pwFrame ), GTK_SHADOW_ETCHED_OUT );
  
  gtk_container_add( GTK_CONTAINER( pwFrame ),
		     pwb = gtk_vbox_new( FALSE, 0 ) );
  
  /* status bar */
  
  for ( i = 0; i < 2; ++i ) {
    pgs->apwStatus[ i ] = gtk_label_new ( NULL );
    gtk_box_pack_start ( GTK_BOX ( pwb ), pgs->apwStatus[ i ], 
                         FALSE, FALSE, 4 );
  }


  /* signals */

  gtk_object_set_data_full ( GTK_OBJECT ( pgs->pwWindow ), "user_data", 
                             pgs, g_free );

  gtk_widget_show_all ( GTK_WIDGET ( pgs->pwWindow ) );

  while( gtk_events_pending() )
    gtk_main_iteration();

  return pgs->pwWindow;

}


extern void
DestroySplash ( GtkWidget *pwSplash ) {

  if ( ! pwSplash )
    return;
  
  USLEEP( 1000 );

  gtk_widget_destroy ( pwSplash );

}


extern void
PushSplash ( GtkWidget *pwSplash, 
             const gchar *szText0, const gchar *szText1,
             const unsigned long nMuSec ) {
  
  gtksplash *pgs;

  if ( ! pwSplash )
    return;
  
  pgs = gtk_object_get_data ( GTK_OBJECT ( pwSplash ),
                                         "user_data" );

  gtk_label_set_text ( GTK_LABEL ( pgs->apwStatus[ 0 ] ), szText0 );
  gtk_label_set_text ( GTK_LABEL ( pgs->apwStatus[ 1 ] ), szText1 );

  while( gtk_events_pending() )
    gtk_main_iteration();

  USLEEP( nMuSec );

}
