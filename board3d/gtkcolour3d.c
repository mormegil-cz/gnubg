
#include "inc3d.h"
#include <gdk/gdkkeysyms.h>
#include <GL/glu.h>

#if HAVE_GTKGLEXT
#include <gtk/gtkgl.h>
#else
#include <gtkgl/gtkglarea.h>
#endif

#include "gtkcolour.h"

#define COLOUR_SEL_DIA( pcp ) GTK_COLOR_SELECTION_DIALOG( GTK_COLOUR_PICKER( \
	pcp )->pwColourSel )
#define COLOUR_SEL( pcp ) GTK_COLOR_SELECTION( COLOUR_SEL_DIA(pcp)->colorsel )

GtkWidget *pcpAmbient, *pcpDiffuse, *pcpSpecular;
GtkWidget *pwColourDialog3d, *pwParent;
GtkAdjustment *padjShine, *padjOpacity;
GtkWidget *glarea, *psOpacity, *pwPreview;
Material *col3d;
Material cancelValue;
static GdkPixmap *xppm;

/* Size of screen widget */
#define PREVIEW_WIDTH 200
#define PREVIEW_HEIGHT 40

/* World sizes */
#define STRIP_WIDTH 100
#define STRIP_HEIGHT 20

extern void InitGL(BoardData *bd);
extern void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a);
extern void setMaterial(Material* pMat);
extern void CheckOpenglError();

GLUquadricObj *qobj;

void SetupLight()
{
	float al[4], dl[4], sl[4];
	float lp[4] = {PREVIEW_WIDTH / 2, PREVIEW_HEIGHT / 2, 50, 1};
	glLightfv(GL_LIGHT0, GL_POSITION, lp);

	al[0] = al[1] = al[2] = 50 / 100.0f;
	al[3] = 1;
	glLightfv(GL_LIGHT0, GL_AMBIENT, al);

	dl[0] = dl[1] = dl[2] = 70 / 100.0f;
	dl[3] = 1;
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dl);

	sl[0] = sl[1] = sl[2] = 100 / 100.0f;
	sl[3] = 1;
	glLightfv(GL_LIGHT0, GL_SPECULAR, sl);
}

static void Draw(Material* pMat)
{
	int tempShin = pMat->shininess;
	pMat->shininess = tempShin / 3;
	/* Accentuate shiness - so visible in preview */
	setMaterial(pMat);
	pMat->shininess = tempShin;

	if (pMat->alphaBlend)
		glEnable(GL_BLEND);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glTranslatef(1, STRIP_HEIGHT / 2, 0);
	glRotatef(90, 0, 1, 0);
	/* -1 (and -2) to give a black outline */
	gluCylinder(qobj, STRIP_HEIGHT / 2 - 1, STRIP_HEIGHT / 2 - 1, STRIP_WIDTH - 2, 36, 36);
	glPopMatrix();

	if (pMat->alphaBlend)
		glDisable(GL_BLEND);
}

void RenderPreview(Material* pMat, unsigned char* buf);

unsigned char auch[PREVIEW_WIDTH * PREVIEW_HEIGHT * 3];

void UpdatePreviewBar(Material* pMat, GdkPixmap *pixmap)
{
	GdkGC *gc;

	RenderPreview(pMat, auch);

	gc = gdk_gc_new(pixmap);
	gdk_draw_rgb_image(pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
					  auch, PREVIEW_WIDTH * 3 );
	gdk_gc_unref( gc );
}

int bUpdate = TRUE;

void UpdateColourPreview(void *arg)
{
	double ambient[4], diffuse[4], specular[4];

if (!bUpdate)
	return;

	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpAmbient), ambient);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpDiffuse), diffuse);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpSpecular), specular);

	SetupMat(col3d, 
		ambient[0], ambient[1], ambient[2],
		diffuse[0], diffuse[1], diffuse[2],
		specular[0], specular[1], specular[2],
		padjShine->value, padjOpacity->value / 100);

	UpdatePreviewBar(col3d, xppm);

	gtk_widget_queue_draw(pwPreview);
}

void SetupColourPreview()
{
	InitGL(0);
	glClearColor(0,0,0,1);

	qobj = gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL);
	gluQuadricNormals(qobj, GLU_FLAT);
	gluQuadricTexture(qobj, GL_FALSE);

	glViewport(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, STRIP_WIDTH, 0, STRIP_HEIGHT, -10, 10);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	SetupLight();
}

#if HAVE_GTKGLEXT

GdkGLConfig *getglconfigSingle();

GdkGLPixmap *glpixmap;
static GdkGLContext *glPixmapContext = NULL;

void CreatePreview()
{
	GdkGLDrawable *gldrawable;
	glpixmap = gdk_pixmap_set_gl_capability(xppm, getglconfigSingle(), NULL);
	gldrawable = GDK_GL_DRAWABLE(glpixmap);
	glPixmapContext = gdk_gl_context_new(gldrawable, NULL, FALSE, GDK_GL_RGBA_TYPE);

	if (!gdk_gl_drawable_gl_begin (gldrawable, glPixmapContext))
		return;

	SetupColourPreview();

	gdk_gl_drawable_gl_end (gldrawable);
}

void RenderPreview(Material* pMat, unsigned char* buf)
{
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = GDK_GL_DRAWABLE(glpixmap);

	if (!gdk_gl_drawable_gl_begin (gldrawable, glPixmapContext))
		return;

	Draw(pMat);

	glReadPixels(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buf);

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
}

#else

extern GdkVisual *visual;
void SetupVisual();

void CreatePreview()
{
	SetupVisual();
}

void RenderPreview(Material* pMat, unsigned char* buf)
{
	GdkGLPixmap *glpixmap;
	GdkGLContext *glPixmapContext;

	glPixmapContext = gdk_gl_context_new(visual);
	glpixmap = gdk_gl_pixmap_new(visual, xppm);

	if (!gdk_gl_pixmap_make_current(glpixmap, glPixmapContext))
		return;

	SetupColourPreview();

	Draw(pMat);

	glReadPixels(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buf);

	gdk_gl_pixmap_unref(glpixmap);
	gdk_gl_context_unref(glPixmapContext);
}

#endif

void AddWidgets(GdkWindow* pixWind, GtkWidget *window)
{
	GtkWidget *table, *label, *scale;
	table = gtk_table_new(4, 4, TRUE);
	gtk_container_add(GTK_CONTAINER(window), table);

	label = gtk_label_new("Ambient colour:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 0, 1);
	pcpAmbient = gtk_colour_picker_new();
	gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL(pcpAmbient) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	gtk_table_attach_defaults(GTK_TABLE (table), pcpAmbient, 1, 2, 0, 1);

	label = gtk_label_new("Diffuse colour:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 1, 2);
	pcpDiffuse = gtk_colour_picker_new();
	gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL(pcpDiffuse) ),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	gtk_table_attach_defaults(GTK_TABLE (table), pcpDiffuse, 1, 2, 1, 2);

	label = gtk_label_new("Specular colour:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 2, 3, 0, 1);
	pcpSpecular = gtk_colour_picker_new();
	gtk_signal_connect_object( GTK_OBJECT( COLOUR_SEL( pcpSpecular )),
			       "color-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	gtk_table_attach_defaults(GTK_TABLE (table), pcpSpecular, 3, 4, 0, 1);

	label = gtk_label_new("Shineness:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 2, 3, 1, 2);
	padjShine = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 128, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjShine ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	scale = gtk_hscale_new(padjShine);
	gtk_scale_set_digits( GTK_SCALE( scale ), 0 );
	gtk_table_attach_defaults(GTK_TABLE (table), scale, 3, 4, 1, 2);

	label = gtk_label_new("Opacity:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 2, 3);
	padjOpacity = GTK_ADJUSTMENT(gtk_adjustment_new(0, 1, 100, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjOpacity ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	psOpacity = gtk_hscale_new(padjOpacity);
	gtk_scale_set_digits( GTK_SCALE( psOpacity ), 0 );
	gtk_table_attach_defaults(GTK_TABLE (table), psOpacity, 1, 2, 2, 3);

	label = gtk_label_new("Texture:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 3, 4);
	label = gtk_label_new("(texture val)");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 1, 2, 3, 4);

	label = gtk_label_new("Preview:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 2, 3, 2, 3);

xppm = gdk_pixmap_new(pixWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);

CreatePreview();

pwPreview = gtk_pixmap_new(xppm, NULL),
	gtk_table_attach_defaults(GTK_TABLE (table), pwPreview, 2, 4, 3, 4);
}

static gboolean cancel( GtkWidget *pw, GdkEvent *pev, void* arg )
{
	/* Restore to original colour */
	memcpy(col3d, &cancelValue, sizeof(Material));

	gtk_window_set_modal( GTK_WINDOW( pwColourDialog3d ), FALSE );
	gtk_widget_hide( pwColourDialog3d );

	gtk_main_quit();
	return TRUE;
}

int okPressed;

static gboolean ok(GtkWidget *pw, GdkEvent *pev, void* arg)
{
	gtk_window_set_modal( GTK_WINDOW( pwColourDialog3d ), FALSE );
	gtk_widget_hide( pwColourDialog3d );
	gtk_main_quit();
	okPressed = TRUE;
	return TRUE;
}

GtkWidget* Create3dColourDialog(GdkWindow* pixWind, GtkWidget* pParent)
{
	GtkWidget *pwOK, *pwCancel, *pwButtons;
	GtkAccelGroup *pag = gtk_accel_group_new();

	pwParent = pParent;	

	pwColourDialog3d = gtk_dialog_new();

	pwButtons = gtk_hbutton_box_new();
	gtk_button_box_set_layout( GTK_BUTTON_BOX( pwButtons ),
			       GTK_BUTTONBOX_SPREAD );
	gtk_container_add( GTK_CONTAINER( GTK_DIALOG( pwColourDialog3d )->action_area ),
		       pwButtons );
	pwOK = gtk_button_new_with_label( "OK" ),
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwOK );
	pwCancel = gtk_button_new_with_label( "Cancel" ),
	gtk_container_add( GTK_CONTAINER( pwButtons ), pwCancel );

	gtk_signal_connect(GTK_OBJECT(pwOK), "clicked", GTK_SIGNAL_FUNC(ok), 0);
	gtk_signal_connect(GTK_OBJECT(pwCancel), "clicked", GTK_SIGNAL_FUNC(cancel), 0);	
	gtk_signal_connect(GTK_OBJECT(pwColourDialog3d), "delete-event",
			GTK_SIGNAL_FUNC(cancel), 0 );
	gtk_signal_connect(GTK_OBJECT(pwColourDialog3d), "realize",
			GTK_SIGNAL_FUNC(UpdateColourPreview), 0 );

#if GTK_CHECK_VERSION(1,3,15)
    gtk_window_add_accel_group( GTK_WINDOW( pwColourDialog3d ), pag );
#else
    gtk_accel_group_attach( pag, GTK_OBJECT( pwColourDialog3d ) );
#endif
    gtk_widget_add_accelerator( pwCancel, "clicked", pag, GDK_Escape, 0, 0 );

    gtk_window_set_title( GTK_WINDOW( pwColourDialog3d ), "3d Colour selection" );

    GTK_WIDGET_SET_FLAGS( pwOK, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( pwOK );

	AddWidgets(pixWind, GTK_DIALOG(pwColourDialog3d)->vbox);

return pwColourDialog3d;
}

void setCol(GtkColourPicker* pCP, float val[4])
{
	double dval[4];
	int i;
	for (i = 0; i < 4; i++)
		dval[i] = val[i];

	gtk_colour_picker_set_colour(pCP, dval);
}

void UpdatePreview(GtkWidget **ppw);

typedef struct UpdateDetails_T
{
	Material* pMat;
	GdkPixmap* pixmap;
	GtkWidget* preview;
	GtkWidget** parentPreview;
} UpdateDetails;

void SetColour3d(GtkWidget *pw, UpdateDetails* pDetails)
{
	col3d = pDetails->pMat;

	/* Avoid updating preview */
	bUpdate = FALSE;

	setCol(GTK_COLOUR_PICKER(pcpAmbient), col3d->ambientColour);
	setCol(GTK_COLOUR_PICKER(pcpDiffuse), col3d->diffuseColour);
	setCol(GTK_COLOUR_PICKER(pcpSpecular), col3d->specularColour);

	padjShine->value = col3d->shininess;
	padjOpacity->value = col3d->ambientColour[3] * 100;

// Update slider values...

	/* Copy material - to reset if cancel pressed */
	memcpy(&cancelValue, col3d, sizeof(Material));

	/* show dialog */
	gtk_window_set_modal( GTK_WINDOW( pwColourDialog3d ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwColourDialog3d ),
                                GTK_WINDOW( pwParent ) );
	gtk_widget_show_all( pwColourDialog3d );

	/* Update preview */
	bUpdate = TRUE;
	UpdateColourPreview(0);

	okPressed = FALSE;

	gtk_main();

	if (okPressed)
	{	/* Apply new settings */
		GdkGC *gc;
	
		gc = gdk_gc_new(pDetails->pixmap);
		gdk_draw_rgb_image(pDetails->pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
						  auch, PREVIEW_WIDTH * 3 );
		gdk_gc_unref( gc );
	
		gtk_widget_queue_draw(pDetails->preview);
		UpdatePreview(pDetails->parentPreview);
	}
}

UpdateDetails test[2];

GtkWidget* gtk_colour_picker_new3d(GtkWidget** parentPreview, GdkWindow* pixWind, Material* pMat)
{
static int t = 1;

	GtkWidget *pixmapwid, *button;
	GdkPixmap *pixmap;
	pixmap = gdk_pixmap_new(pixWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);

	button = gtk_button_new();
	pixmapwid = gtk_pixmap_new(pixmap, NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmapwid);

t=!t;
test[t].pMat = pMat;
test[t].pixmap = pixmap;
test[t].preview = button;
test[t].parentPreview = parentPreview;

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(SetColour3d), &test[t]);

UpdatePreviewBar(pMat, pixmap);
	return button;
}
