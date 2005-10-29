/*
* widget3d.c
* by Jon Kinsey, 2003
*
* Gtkglarea/Gtkglext widget for 3d drawing
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

#include <config.h>
#include "glincl.h"
#include "inc3d.h"
#include "shadow.h"
#include "renderprefs.h"
#include "backgammon.h"
#include "gtkgame.h"
#include "gtkprefs.h"

#if HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#else
#include <gtkgl/gtkglarea.h>
#endif

#if HAVE_GTKGLEXT
GdkGLConfig *glconfig;

GdkGLConfig *getGlConfig()
{
	if (!glconfig)
		glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_STENCIL);

	return glconfig;
}
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
{	/* Animation has finished (or could have been interruptted) */
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

void Draw(BoardData* bd)
{	/* Render board - need multiple passes for shadows */
	if (bd->rd->showShadows)
		shadowDisplay(drawBoard, bd);
	else
		drawBoard(bd);
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *notused, BoardData* bd)
{
	int width, height;
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
	width = widget->allocation.width;
	height = widget->allocation.height;

	glViewport(0, 0, width, height);
	SetupViewingVolume3d(bd);

	RestrictiveRedraw();

#if HAVE_GTKGLEXT
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif

	return TRUE;
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

	InitGL(bd);
	SetupViewingVolume3d(bd);
	GetTextures(bd);
	preDraw3d(bd);

#if HAVE_GTKGLEXT
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif
}

void MakeCurrent3d(GtkWidget *widget)
{
#if HAVE_GTKGLEXT
	gdk_gl_drawable_make_current(gtk_widget_get_gl_drawable(widget), gtk_widget_get_gl_context(widget));
#else
	gtk_gl_area_make_current(GTK_GL_AREA(widget));
#endif
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, BoardData* bd)
{
#if HAVE_GTKGLEXT
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return TRUE;
#else
	/* OpenGL functions can be called only if make_current returns true */
	if (!gtk_gl_area_make_current(GTK_GL_AREA(widget)))
		return TRUE;
#endif
	CheckOpenglError();

	if (!bd->rd->quickDraw)
		Draw(bd);
	else
	{	/* Quick drawing mode */
		if (numRestrictFrames > 0)
		{
			RestrictiveRender(bd);
		}
		else if (numRestrictFrames < 0)
		{
			Draw(bd);
			numRestrictFrames = 0;
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

extern GtkWidget* CreateGLWidget(BoardData* bd)
{
	GtkWidget* drawing_area;
#if HAVE_GTKGLEXT
	/* Drawing area for OpenGL */
	drawing_area = gtk_drawing_area_new();
	/* Set OpenGL-capability to the widget - no list sharing */
	gtk_widget_set_gl_capability(drawing_area, getGlConfig(), NULL, TRUE, GDK_GL_RGBA_TYPE);
#else
	drawing_area = gtk_gl_area_new_vargs(NULL, /* no sharing of lists */
			 GDK_GL_RGBA, GDK_GL_DOUBLEBUFFER, GDK_GL_DEPTH_SIZE, 1, GDK_GL_STENCIL_SIZE, 1, GDK_GL_NONE);
#endif
	if (drawing_area == NULL)
	{
		g_print("Can't create opengl drawing widget\n");
		return NULL;
	}

	gtk_widget_set_events(drawing_area, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
			GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "button_press_event", GTK_SIGNAL_FUNC(button_press_event), bd);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "button_release_event", GTK_SIGNAL_FUNC(button_release_event), bd);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "motion_notify_event", GTK_SIGNAL_FUNC(motion_notify_event), bd);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "realize", GTK_SIGNAL_FUNC(realize), bd);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "configure_event", GTK_SIGNAL_FUNC(configure_event), bd);
	gtk_signal_connect(GTK_OBJECT(drawing_area), "expose_event", GTK_SIGNAL_FUNC(expose_event), bd);

	return drawing_area;
}

void InitGTK3d(int *argc, char ***argv)
{
#if HAVE_GTKGLEXT
	gtk_gl_init(argc, argv);
#endif

	/* Call LoadTextureInfo to get texture details from textures.txt */
	LoadTextureInfo(TRUE);
}

void Init3d()
{	/* May be called several times - only init on first call */
	static int initilized = FALSE;
	if (initilized)
		return;
	initilized = TRUE;

	/* Check for opengl support */
#if HAVE_GTKGLEXT
	if (!getGlConfig())
#else
	if (!gdk_gl_query())
#endif
		g_print("*** Cannot find OpenGL-capable visual ***\n");

	/* Second call to LoadTextureInfo to show any error messages */
	LoadTextureInfo(FALSE);
}

/* This doesn't seem to work even on windows anymore... */
#ifdef TEMP_REMOVE

#ifndef PFD_GENERIC_ACCELERATED
#define PFD_GENERIC_ACCELERATED     0x00001000
#endif

void getFormatDetails(HDC hdc, int* accl, int* dbl, int* col, int* depth, int* stencil, int * accum)
{
	PIXELFORMATDESCRIPTOR pfd;

	/* Use current format */
	int format = GetPixelFormat(hdc);
	if (format == 0)
	{
		g_print("Unable to find pixel format.\n");
		return;
	}
	ZeroMemory(&pfd,sizeof(pfd));
	pfd.nSize=sizeof(pfd);
	pfd.nVersion=1;

	if (!DescribePixelFormat(hdc, format, sizeof(PIXELFORMATDESCRIPTOR), &pfd))
	{
		g_print("Unable to describe pixel format.\n");
		return;
	}

	*accl = !((pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED));
	*dbl = pfd.dwFlags & PFD_DOUBLEBUFFER;
	*col = pfd.cColorBits;
	*depth = pfd.cDepthBits;
	*stencil = pfd.cStencilBits;
	*accum = pfd.cAccumBits;
}

static int CheckAccelerated(GtkWidget* board)
{
#if HAVE_GTKGLEXT
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(board);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(board)))
		return 1;
#else
	/* OpenGL functions can be called only if make_current returns true */
	if (!gtk_gl_area_make_current(GTK_GL_AREA(board)))
		return 1;
#endif
{
	int accl = 0, dbl, col, depth, stencil, accum;
	HDC dc = wglGetCurrentDC();

/* Display graphics card details
	HGLRC hglrc = wglGetCurrentContext();
	g_print("(hglrc:%p, dc:%p).\n", hglrc, dc);

	const char* vendor = glGetString(GL_VENDOR);
	const char* renderer = glGetString(GL_RENDERER);
	const char* version = glGetString(GL_VERSION);

	g_print("\n3d graphics card details\n\n");
	g_print("Vendor: %s\n", vendor);
	g_print("Renderer: %s\n", renderer);
	g_print("Version: %s\n", version);
*/

	if (!dc)
		g_print("No DC found.\n");
	else
	{
		getFormatDetails(dc, &accl, &dbl, &col, &depth, &stencil, &accum);
/*
		g_print("%sAccelerated\n(%s, %d bit col, %d bit depth, %d bit stencil, %d bit accum)\n",
			accl ? "" : "NOT ", dbl?"Double buffered":"Single buffered", col, depth, stencil, accum);
*/
	}
#if HAVE_GTKGLEXT
	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
#endif

	return accl;
}
}

#else

static int CheckAccelerated(GtkWidget* board)
{
/* Commented out check for non-windows systems,
	as doesn't work very well... */
	return TRUE;
/*
	Display* display = glXGetCurrentDisplay();
	GLXContext context = glXGetCurrentContext();
	if (!display || !context)
	{
		g_print("Unable to get current display information.\n");
		return 1;
	}
	return glXIsDirect(display, context);
*/
}

#endif

int DoAcceleratedCheck(GtkWidget* board)
{
	if (!CheckAccelerated(board))
	{	/* Display warning message as performance may be bad */
		GTKShowWarning(WARN_UNACCELERATED);
		return 0;
	}
	else
		return 1;
}

/* Drawing direct to pixmap */

GdkGLContext *glPixmapContext = NULL;

void SetupPreview(BoardData* bd, renderdata* prd)
{
	ClearTextures(bd);
	GetTextures(bd);
	SetupViewingVolume3d(bd);
	preDraw3d(bd);
}

#if HAVE_GTKGLEXT

GdkGLConfig *glconfigSingle = NULL;

GdkGLConfig *getglconfigSingle()
{
	if (!glconfigSingle)
		glconfigSingle = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_SINGLE | GDK_GL_MODE_STENCIL);

	return glconfigSingle;
}

void *CreatePreviewBoard3d(BoardData* bd, GdkPixmap *ppm)
{
	GdkGLPixmap *glpixmap;
	
	GdkGLDrawable *gldrawable;

	glpixmap = gdk_pixmap_set_gl_capability(ppm, getglconfigSingle(), NULL);
	gldrawable = GDK_GL_DRAWABLE(glpixmap);
	glPixmapContext = gdk_gl_context_new (gldrawable, NULL, FALSE, GDK_GL_RGBA_TYPE);

	InitBoardPreview(bd);

	if (!gdk_gl_drawable_gl_begin (gldrawable, glPixmapContext))
		return 0;

	InitGL(bd);

	gdk_gl_drawable_gl_end (gldrawable);

	return glpixmap;
}

void RenderBoard3d(BoardData* bd, renderdata* prd, void *glpixmap, unsigned char* buf)
{
	GLint viewport[4];
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = GDK_GL_DRAWABLE((GdkGLPixmap *)glpixmap);

	if (!gdk_gl_drawable_gl_begin (gldrawable, glPixmapContext))
		return;

	SetupPreview(bd, prd);

	Draw(bd);

	glGetIntegerv (GL_VIEWPORT, viewport);
	glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, buf);

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
}

#else

GdkVisual *visual = NULL;

void SetupVisual()
{
	int visual_attributes[] = {GDK_GL_RGBA,
				GDK_GL_DEPTH_SIZE, 1,
				GDK_GL_STENCIL_SIZE, 1,
			    GDK_GL_NONE};

	if (!visual)
	{
		visual = gdk_gl_choose_visual(visual_attributes);
		if (visual == NULL)
		{
			g_print("Can't get visual\n");
			return;
		}
	}
}

void *CreatePreviewBoard3d(BoardData* bd, GdkPixmap *ppm)
{
	SetupVisual();

	InitBoardPreview(bd);

	return ppm;
}

void RenderBoard3d(BoardData* bd, renderdata* prd, void *ppm, unsigned char* buf)
{
	GLint viewport[4];
	GdkGLPixmap *glpixmap;

	glPixmapContext = gdk_gl_context_new(visual);
	glpixmap = gdk_gl_pixmap_new(visual, (GdkPixmap*)ppm);

	if (!gdk_gl_pixmap_make_current(glpixmap, glPixmapContext))
		return;

	InitGL(bd);

	SetupPreview(bd, prd);

	Draw(bd);

	glGetIntegerv (GL_VIEWPORT, viewport);
	glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, buf);

	gdk_gl_pixmap_unref(glpixmap);
	gdk_gl_context_unref(glPixmapContext);
}

#endif
