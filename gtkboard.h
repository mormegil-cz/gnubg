/*
 * board.h
 *
 * by Gary Wong <gtw@gnu.org>, 1997-2000.
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
 */

#ifndef _GTKBOARD_H_
#define _GTKBOARD_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_BOARD ( board_get_type() )
#define BOARD( obj ) ( GTK_CHECK_CAST( (obj), TYPE_BOARD, Board ) )
#define BOARD_CLASS( c ) ( GTK_CHECK_CLASS_CAST( (c), TYPE_BOARD, \
	BoardClass ) )
#define IS_BOARD( obj ) ( GTK_CHECK_TYPE( (obj), TYPE_BOARD ) )
#define IS_BOARD_CLASS( c ) ( GTK_CHECK_CLASS_TYPE( (c), TYPE_BOARD ) )

typedef struct _Board {
    GtkVBox vbox;
    gpointer board_data;
} Board;

typedef struct _BoardClass {
    GtkVBoxClass parent_class;
} BoardClass;

extern GtkType board_get_type( void );    
extern GtkWidget *board_new( void );
extern gint board_set( Board *board, const gchar *board_text );
extern gint game_set( Board *board, gint points[ 2 ][ 25 ], int roll,
		      gchar *name, gchar *opp_name, gint match,
		      gint score, gint opp_score, gint die0, gint die1 );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
