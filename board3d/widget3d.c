/*
 * widget3d.c
 * by Jon Kinsey, 2003
 *
 * GtkGLExt widget for 3d drawing
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 3 or later of the GNU General Public License as
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

#include "config.h"
#include "inc3d.h"
#include "tr.h"
#include "gtklocdefs.h"

gboolean gtk_gl_init_success = FALSE;

extern GdkGLConfig *
getGlConfig(void)
{
    static GdkGLConfig *glconfig = NULL;
    if (!glconfig)
        glconfig =
            gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_STENCIL);
    if (!glconfig) {
        glconfig = gdk_gl_config_new_by_mode(GDK_GL_MODE_RGB | GDK_GL_MODE_DEPTH | GDK_GL_MODE_DOUBLE);
        g_warning("Stencil buffer not available, no shadows\n");
    }
    if (!glconfig) {
        g_warning("*** No appropriate OpenGL-capable visual found.\n");
    }
    return glconfig;
}

static gboolean
configure_event_3d(GtkWidget * widget, GdkEventConfigure * UNUSED(eventDetails), void *data)
{
    BoardData *bd = (BoardData *) data;

    if (display_is_3d(bd->rd)) {
        static int curHeight = -1, curWidth = -1;
        GtkAllocation allocation;
        int width, height;
        gtk_widget_get_allocation(widget, &allocation);
        width = allocation.width;
        height = allocation.height;
        if (width != curWidth || height != curHeight) {
            GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

            if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
                return FALSE;

            glViewport(0, 0, width, height);
            SetupViewingVolume3d(bd, bd->bd3d, bd->rd);

            RestrictiveRedraw();
            RerenderBase(bd->bd3d);

            gdk_gl_drawable_gl_end(gldrawable);

            curWidth = width, curHeight = height;
        }
    }
    return TRUE;
}

static void
realize_3d(GtkWidget * widget, void *data)
{
    BoardData *bd = (BoardData *) data;
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
        return;

    InitGL(bd);
    GetTextures(bd->bd3d, bd->rd);
    preDraw3d(bd, bd->bd3d, bd->rd);
    /* Make sure viewing area is correct (in preview) */
    SetupViewingVolume3d(bd, bd->bd3d, bd->rd);
#ifdef WIN32
    if (fResetSync) {
        fResetSync = FALSE;
        (void) setVSync(fSync);
    }
#endif

    gdk_gl_drawable_gl_end(gldrawable);
}

void
MakeCurrent3d(const BoardData3d * bd3d)
{
    GtkWidget *widget = bd3d->drawing_area3d;
    if (!gdk_gl_drawable_make_current(gtk_widget_get_gl_drawable(widget), gtk_widget_get_gl_context(widget)))
        g_print("gdk_gl_drawable_make_current failed!\n");
}

void
UpdateShadows(BoardData3d * bd3d)
{
    bd3d->shadowsOutofDate = TRUE;
}

static gboolean
expose_event_3d(GtkWidget * widget, const GdkEventExpose * exposeEvent, const BoardData * bd)
{
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
        return TRUE;

    CheckOpenglError();

    if (bd->bd3d->shadowsOutofDate) {   /* Update shadow positions */
        bd->bd3d->shadowsOutofDate = FALSE;
        updateOccPos(bd);
    }
#ifdef TEST_HARNESS
    TestHarnessDraw(bd);
#else
    if (bd->rd->quickDraw) {    /* Quick drawing mode */
        if (numRestrictFrames >= 0) {
            if (numRestrictFrames == 0) {       /* Redraw obscured part of window - need to flip y co-ord */
                GtkAllocation allocation;
                gtk_widget_get_allocation(widget, &allocation);
                RestrictiveDrawFrameWindow(exposeEvent->area.x,
                                           allocation.height - (exposeEvent->area.y + exposeEvent->area.height),
                                           exposeEvent->area.width, exposeEvent->area.height);
            }

            /* Draw updated regions directly to screen */
            glDrawBuffer(GL_FRONT);
            RestrictiveRender(bd, bd->bd3d, bd->rd);
            glFlush();
        } else {                /* Full screen redraw (to back buffer and then swap) */
            glDrawBuffer(GL_BACK);
            numRestrictFrames = 0;
            drawBoard(bd, bd->bd3d, bd->rd);
            gdk_gl_drawable_swap_buffers(gldrawable);
        }

        gdk_gl_drawable_gl_end(gldrawable);

        return TRUE;
    }

    Draw3d(bd);
#endif

    gdk_gl_drawable_swap_buffers(gldrawable);

    gdk_gl_drawable_gl_end(gldrawable);

    return TRUE;
}

extern int
CreateGLWidget(BoardData * bd)
{
    GtkWidget *p3dWidget;
    bd->bd3d = (BoardData3d *) malloc(sizeof(BoardData3d));
    InitBoard3d(bd, bd->bd3d);
    /* Drawing area for OpenGL */
    p3dWidget = bd->bd3d->drawing_area3d = gtk_drawing_area_new();

    /* Set OpenGL-capability to the widget - no list sharing */
    if (!gtk_widget_set_gl_capability(p3dWidget, getGlConfig(), NULL, TRUE, GDK_GL_RGBA_TYPE)) {
        g_print("Can't create opengl capable widget\n");
        return FALSE;
    }

    if (p3dWidget == NULL) {
        g_print("Can't create opengl drawing widget\n");
        return FALSE;
    }
    /* set up events and signals for OpenGL widget */
    gtk_widget_set_events(p3dWidget, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK);
    g_signal_connect(G_OBJECT(p3dWidget), "button_press_event", G_CALLBACK(board_button_press), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "button_release_event", G_CALLBACK(board_button_release), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "motion_notify_event", G_CALLBACK(board_motion_notify), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "realize", G_CALLBACK(realize_3d), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "configure_event", G_CALLBACK(configure_event_3d), bd);
    g_signal_connect(G_OBJECT(p3dWidget), "expose_event", G_CALLBACK(expose_event_3d), bd);

    return TRUE;
}

void
InitGTK3d(int *argc, char ***argv)
{
    gtk_gl_init_success = gtk_gl_init_check(argc, argv);
    /* Call LoadTextureInfo to get texture details from textures.txt */
    LoadTextureInfo();
    SetupFlag();
}

/* This doesn't seem to work even on windows anymore... */
#ifdef TEMP_REMOVE

#ifndef PFD_GENERIC_ACCELERATED
#define PFD_GENERIC_ACCELERATED     0x00001000
#endif

void
getFormatDetails(HDC hdc, int *accl, int *dbl, int *col, int *depth, int *stencil, int *accum)
{
    PIXELFORMATDESCRIPTOR pfd;

    /* Use current format */
    int format = GetPixelFormat(hdc);
    if (format == 0) {
        g_print("Unable to find pixel format.\n");
        return;
    }
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;

    if (!DescribePixelFormat(hdc, format, sizeof(PIXELFORMATDESCRIPTOR), &pfd)) {
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

static int
CheckAccelerated(GtkWidget * board)
{
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(board);

    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(board)))
        return 1;
    {
        int accl = 0, dbl, col, depth, stencil, accum;
        HDC dc = wglGetCurrentDC();

        /* Display graphics card details
         * HGLRC hglrc = wglGetCurrentContext();
         * g_print("(hglrc:%p, dc:%p).\n", hglrc, dc);
         * 
         * const char* vendor = glGetString(GL_VENDOR);
         * const char* renderer = glGetString(GL_RENDERER);
         * const char* version = glGetString(GL_VERSION);
         * 
         * g_print("\n3d graphics card details\n\n");
         * g_print("Vendor: %s\n", vendor);
         * g_print("Renderer: %s\n", renderer);
         * g_print("Version: %s\n", version);
         */

        if (!dc)
            g_print("No DC found.\n");
        else {
            getFormatDetails(dc, &accl, &dbl, &col, &depth, &stencil, &accum);
            /*
             * g_print("%sAccelerated\n(%s, %d bit col, %d bit depth, %d bit stencil, %d bit accum)\n",
             * accl ? "" : "NOT ", dbl?"Double buffered":"Single buffered", col, depth, stencil, accum);
             */
        }
        gdk_gl_drawable_gl_end(gldrawable);

        return accl;
    }
}

/* Unsupported (linux) version */

static int
CheckAccelerated(GtkWidget * notused)
{
    /* Commented out check for non-windows systems,
     * as doesn't work very well... */
    return TRUE;
    /*
     * Display* display = glXGetCurrentDisplay();
     * GLXContext context = glXGetCurrentContext();
     * if (!display || !context)
     * {
     * g_print("Unable to get current display information.\n");
     * return 1;
     * }
     * return glXIsDirect(display, context);
     */
}

#endif

int
DoAcceleratedCheck(const BoardData3d * UNUSED(bd3d), GtkWidget * UNUSED(pwParent))
{                               /* Currently not supported (code out of date/unwritten) */
#ifdef TEMP_REMOVE
    if (!CheckAccelerated(bd3d->drawing_area3d)) {      /* Display warning message as performance may be bad */
        GTKShowWarning(WARN_UNACCELERATED, pwParent);
        return 0;
    } else
#endif
        return 1;
}

void
RenderToBuffer3d(const BoardData * bd, BoardData3d * bd3d, unsigned int width, unsigned int height, unsigned char *buf)
{
    TRcontext *tr;
    GtkAllocation allocation;
    GtkWidget *widget = bd3d->drawing_area3d;
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    int fSaveBufs = bd3d->fBuffers;
    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
        return;

    /* Sort out tile rendering stuff */
    tr = trNew();
#define BORDER 10
    gtk_widget_get_allocation(widget, &allocation);
    trTileSize(tr, allocation.width, allocation.height, BORDER);
    trImageSize(tr, width, height);
    trImageBuffer(tr, GL_RGB, GL_UNSIGNED_BYTE, buf);

    /* Sort out viewing perspective */
    glViewport(0, 0, (int) width, (int) height);
    SetupViewingVolume3d(bd, bd3d, bd->rd);

    if (bd->rd->planView)
        trOrtho(tr, -bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, 0.0, 5.0);
    else
        trFrustum(tr, -bd3d->horFrustrum, bd3d->horFrustrum, -bd3d->vertFrustrum, bd3d->vertFrustrum, zNear, zFar);

    bd3d->fBuffers = FALSE;     /* Turn this off whilst drawing */

    /* Draw tiles */
    do {
        trBeginTile(tr);
        Draw3d(bd);
    } while (trEndTile(tr));

    bd3d->fBuffers = fSaveBufs;

    trDelete(tr);

    /* Reset viewing volume for main screen */
    SetupViewingVolume3d(bd, bd->bd3d, bd->rd);

    gdk_gl_drawable_gl_end(gldrawable);
}
