/*
* widget3d.c
* by Jon Kinsey, 2003
*
* GtkGLExt widget for 3d drawing
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

#include "inc3d.h"
#include "gtkboard.h"
#include "shadow.h"
#include "gtkgame.h"

#include <gtk/gtkgl.h>

extern GdkGLConfig *getGlConfig()
{
	static GdkGLConfig *glconfig = NULL;
	if (!glconfig)
		glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_STENCIL);

	return glconfig;
}

void Draw(BoardData* bd)
{	/* Render board - need multiple passes for shadows */
	if (bd->rd->showShadows)
		shadowDisplay(drawBoard, bd);
	else
		drawBoard(bd, &bd->bd3d, bd->rd);
}

static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *notused, BoardData* bd)
{
	if (bd->rd->fDisplayType == DT_3D)
  {
    static int oldHeight = -1, oldWidth = -1;
  	int width = widget->allocation.width, height = widget->allocation.height;
  	if (width != oldWidth || height != oldHeight)
    {
      oldWidth = width, oldHeight = height;
	  {
    	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    
    	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
    		return FALSE;
  
    	glViewport(0, 0, width, height);
    	SetupViewingVolume3d(bd, &bd->bd3d, bd->rd);
    
    	RestrictiveRedraw();

    	gdk_gl_drawable_gl_end(gldrawable);
	  }
    }
  }
	return TRUE;
}

void realize(GtkWidget *widget, BoardData* bd)
{
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return;

	InitGL(bd);
	SetupViewingVolume3d(bd, &bd->bd3d, bd->rd);
	GetTextures(&bd->bd3d, bd->rd);
	preDraw3d(bd, &bd->bd3d, bd->rd);

	gdk_gl_drawable_gl_end(gldrawable);
}

void MakeCurrent3d(GtkWidget *widget)
{
	gdk_gl_drawable_make_current(gtk_widget_get_gl_drawable(widget), gtk_widget_get_gl_context(widget));
}

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, BoardData* bd)
{
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
		return TRUE;

	CheckOpenglError();

	if (!bd->rd->quickDraw)
		Draw(bd);
	else
	{	/* Quick drawing mode */
		if (numRestrictFrames > 0)
		{
			RestrictiveRender(bd, &bd->bd3d, bd->rd);
		}
		else if (numRestrictFrames < 0)
		{
			Draw(bd);
			numRestrictFrames = 0;
		}
	}

	gdk_gl_drawable_swap_buffers(gldrawable);

	gdk_gl_drawable_gl_end(gldrawable);

	return TRUE;
}

extern GtkWidget* CreateGLWidget(BoardData* bd)
{
	GtkWidget* drawing_area;
	/* Drawing area for OpenGL */
	drawing_area = gtk_drawing_area_new();
	/* Set OpenGL-capability to the widget - no list sharing */
	gtk_widget_set_gl_capability(drawing_area, getGlConfig(), NULL, TRUE, GDK_GL_RGBA_TYPE);

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
	gtk_gl_init(argc, argv);

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
	if (!getGlConfig())
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
	GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(board);

	if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(board)))
		return 1;
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
	gdk_gl_drawable_gl_end(gldrawable);

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
	ClearTextures(&bd->bd3d);
	GetTextures(&bd->bd3d, bd->rd);
	SetupViewingVolume3d(bd, &bd->bd3d, bd->rd);
	preDraw3d(bd, &bd->bd3d, bd->rd);
}

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

	InitBoard3d(bd, &bd->bd3d);

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
