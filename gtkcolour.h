/*
 * gtkcolour.h
 *
 * by Gary Wong <gtw@gnu.org>, 2002.
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

#ifndef _GTKCOLOUR_H_
#define _GTKCOLOUR_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_COLOUR_PICKER (gtk_colour_picker_get_type())
#define GTK_COLOUR_PICKER(obj) (GTK_CHECK_CAST((obj), GTK_TYPE_COLOUR_PICKER, \
	GtkColourPicker))
#define GTK_COLOUR_PICKER_CLASS(c) (GTK_CHECK_CLASS_CAST((c), \
	GTK_TYPE_COLOUR_PICKER, GtkColourPickerClass))
#define GTK_IS_COLOUR_PICKER(obj) (GTK_CHECK_TYPE((obj), \
	GTK_TYPE_COLOUR_PICKER))
#define GTK_IS_COLOUR_PICKER_CLASS(c) (GTK_CHECK_CLASS_TYPE((c), \
	GTK_TYPE_COLOUR_PICKER))
#define GTK_COLOUR_PICKER_GET_CLASS(obj) (GTK_CHECK_GET_CLASS((obj), \
	GTK_TYPE_COLOUR_PICKER, GtkColourPickerClass))

typedef struct _GtkColourPicker GtkColourPicker;
typedef struct _GtkColourPickerClass GtkColourPickerClass;

struct _GtkColourPicker {
    GtkButton parent_instance;
    GtkWidget *pwColourSel, *pwDraw;
    GdkPixmap *ppm;
    gdouble arOrig[ 4 ];
};

struct _GtkColourPickerClass {
    GtkButtonClass parent_class;
};

extern GtkType gtk_colour_picker_get_type( void );
extern GtkWidget *gtk_colour_picker_new( void );
extern void gtk_colour_picker_set_has_opacity_control(
    GtkColourPicker *pcp, gboolean f );
extern void gtk_colour_picker_set_colour( GtkColourPicker *pcp,
					  gdouble *ar );
extern void gtk_colour_picker_get_colour( GtkColourPicker *pcp,
					  gdouble *ar );
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
