/*
* widget3d.c
* by Jon Kinsey, 2003
*
* Gtkglext widget for 3d drawing
*
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

#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include "shadow.h"
#include "renderprefs.h"

#if HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#else
#include <gtkgl/gtkglarea.h>
#endif

#if HAVE_GTKGLEXT
GdkGLConfig *glconfig;
#endif

guint idleId = 0;
idleFunc *pIdleFun;

gboolean idle(BoardData* bd)
{
	if (pIdleFun(bd))
		gtk_widget_queue_draw(bd->drawing_area3d);

	return TRUE;
}

void StopIdle3d(BoardData* bd)
{	/* NB. Animation could have been interruptted */
	if (bd->shakingDice)
	{
		bd->shakingDice = 0;
		updateDiceOccPos(bd);
		gtk_main_quit();
	}
	if (bd->moving)
	{
		bd->moving = 0;
		updatePieceOccPos(bd);
		animation_finished = TRUE;
		gtk_main_quit();
	}

	if (idleId)
	{
		g_idle_remove_by_data(bd);
		idleId = 0;
	}
}

void setIdleFunc(BoardData* bd, idleFunc* pFun)
{
	if (idleId)
	{
		g_idle_remove_by_data(bd);
		idleId = 0;
	}
	pIdleFun = pFun;

	idleId = g_idle_add((GtkFunction)idle, bd);
}

void realize(GtkWidget *widget, BoardData* bd)
{
#if HAVE_GTKGLEXT
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return;
#else
    if (!gtk_gl_area_make_current(GTK_GL_AREA(widget)))
		return;
#endif

	InitGL();
	SetSkin(bd, rdAppearance.skin3d);

#if HAVE_GTKGLEXT
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event, BoardData* bd)
{
#if HAVE_GTKGLEXT
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return FALSE;
#else
	/* OpenGL functions can be called only if make_current returns true */
	if (!gtk_gl_area_make_current(GTK_GL_AREA(widget)))
		return FALSE;
#endif

	glViewport(0, 0, widget->allocation.width, widget->allocation.height);
	SetupViewingVolume3d(bd);

#if HAVE_GTKGLEXT
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif

	return TRUE;
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, BoardData* bd)
{
/* Debug timer stuff */
#define NUM_SAMPLES 10
static int count = 0;
static double total = 0;
double end, start = 0;

#if HAVE_GTKGLEXT
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return TRUE;
#else
	/* Draw only last expose. */
	if (event->count > 0)
		return TRUE;

	/* OpenGL functions can be called only if make_current returns true */
	if (!gtk_gl_area_make_current(GTK_GL_AREA(widget)))
		return TRUE;
#endif

if (rdAppearance.debugTime)
	start = get_time();

	/* Render board - need multiple passes for shadows */
	if (rdAppearance.showShadows)
		shadowDisplay((void*)drawBoard, bd);
	else
		drawBoard(bd);

/* Debug timer stuff */
if (rdAppearance.debugTime)
{
	glFinish();	/* Execute opengl commands */
	end = get_time();
	total += end - start;
	count++;
	if (!(count % NUM_SAMPLES))
	{
		double ave = total / NUM_SAMPLES;
		if (count > NUM_SAMPLES)	/* Skip first NUM_SAMPLES */
		{
			double fps = 1000 / ave;
			g_print("Average draw time:%.1fms.\n", ave);
			if (fps > 30)
				g_print("3d Performance is very fast.\n");
			else if (fps > 10)
				g_print("3d Performance is good.\n");
			else if (fps > 5)
				g_print("3d Performance is poor.\n");
			else
				g_print("3d Performance is very poor.\n");
			
			count = 0;
		}
		total = 0;
	}
}

#if HAVE_GTKGLEXT
	gdk_gl_drawable_swap_buffers(gldrawable);

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#else
	gtk_gl_area_swapbuffers(GTK_GL_AREA(widget));
#endif

	return TRUE;
}

void CreateGLWidget(BoardData* bd, GtkWidget **drawing_area)
{
#if HAVE_GTKGLEXT
	/* Drawing area for OpenGL */
	*drawing_area = gtk_drawing_area_new();

	/* Set OpenGL-capability to the widget. */
	gtk_widget_set_gl_capability(*drawing_area, glconfig, NULL, TRUE, GDK_GL_RGBA_TYPE);

	gtk_widget_set_events(*drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
				GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);

	g_signal_connect(G_OBJECT(*drawing_area), "realize", G_CALLBACK(realize), bd);
	g_signal_connect(G_OBJECT(*drawing_area), "configure_event", G_CALLBACK(configure_event), bd);
	g_signal_connect(G_OBJECT(*drawing_area), "expose_event", G_CALLBACK(expose_event), bd);
	g_signal_connect(G_OBJECT(*drawing_area), "button_press_event", G_CALLBACK(button_press_event), bd);
	g_signal_connect(G_OBJECT(*drawing_area), "button_release_event", G_CALLBACK(button_release_event), bd);
	g_signal_connect(G_OBJECT(*drawing_area), "motion_notify_event", G_CALLBACK(motion_notify_event), bd);

#else
	/* create new OpenGL widget */
	*drawing_area = gtk_gl_area_new_vargs(NULL, /* no sharing */
			 GDK_GL_RGBA,
			 GDK_GL_DEPTH_SIZE,1,
			 GDK_GL_DOUBLEBUFFER,
			 GDK_GL_NONE);
	if (*drawing_area == NULL)
	{
		g_print("Can't create GtkGLArea widget\n");
		return FALSE;
	}
	gtk_widget_set_events(*drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
				GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);

	gtk_signal_connect(GTK_OBJECT(*drawing_area), "realize", GTK_SIGNAL_FUNC(realize), bd);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "configure_event", GTK_SIGNAL_FUNC(configure_event), bd);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "expose_event", GTK_SIGNAL_FUNC(expose_event), bd);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "button_press_event", GTK_SIGNAL_FUNC(button_press_event), bd);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "button_release_event", GTK_SIGNAL_FUNC(button_release_event), bd);
	gtk_signal_connect(GTK_OBJECT(*drawing_area), "motion_notify_event", GTK_SIGNAL_FUNC(motion_notify_event), bd);
#endif
}

void CreateBoard3d(BoardData* bd, GtkWidget** drawing_area)
{
	CreateGLWidget(bd, drawing_area);
	InitBoard3d(bd);
}

int InitGTK3d(int *argc, char ***argv)
{
#if HAVE_GTKGLEXT
	gtk_gl_init(argc, argv);

	/* Configure OpenGL-capable visual */
	glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_STENCIL);
	if (glconfig == NULL)
#else
	if (gdk_gl_query() == FALSE)
#endif
	{
		g_print("*** Cannot find OpenGL-capable visual.\n");
		return 1;
	}

	return 0;
}
