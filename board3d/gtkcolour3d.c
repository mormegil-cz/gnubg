
#include <GL/gl.h>
#include <GL/glu.h>
#include "inc3d.h"
#include <gdk/gdkkeysyms.h>

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
GtkWidget *psOpacity, *pOpacitylabel, *pTexturelabel, *pwPreview, *textureCombo;
Material *col3d;
Material cancelValue;
int okPressed;
GdkPixmap *xppm;
int useOpacity, useTexture;
float opacityValue;
int bUpdate = TRUE;
GLUquadricObj *qobj;
char lastTextureStr[NAME_SIZE + 1];

/* Store the previews details here */
#define MAX_DETAILS 15
UpdateDetails details[MAX_DETAILS];
int curDetail;

/* Size of screen widget */
#define PREVIEW_WIDTH 200
#define PREVIEW_HEIGHT 40

/* World sizes */
#define STRIP_WIDTH 100
#define STRIP_HEIGHT 10

unsigned char auch[PREVIEW_WIDTH * PREVIEW_HEIGHT * 3];

extern void InitGL(BoardData *bd);
extern void SetupMat(Material* pMat, float r, float g, float b, float dr, float dg, float db, float sr, float sg, float sb, int shin, float a);
extern void setMaterial(Material* pMat);
extern void CheckOpenglError();
extern void UpdatePreview(GtkWidget **ppw);
extern void RenderPreview(Material* pMat, unsigned char* buf);
extern int LoadTexture(Texture* texture, const char* Filename);
extern BoardData bd3d;

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
	Texture texture;
	int tempShin = pMat->shininess;
	float edge = (1 / (float)PREVIEW_HEIGHT) * STRIP_HEIGHT;
	/* Accentuate shiness - so visible in preview */
	pMat->shininess = tempShin / 3;

	if (pMat->textureInfo)
	{
		char buf[100];
		strcpy(buf, TEXTURE_PATH);
		strcat(buf, pMat->textureInfo->file);
		LoadTexture(&texture, buf);

		gluQuadricTexture(qobj, GL_TRUE);

		pMat->pTexture = &texture;

		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		if (pMat->textureInfo->type == TT_HINGE)
		{
			glScalef(1, HINGE_SEGMENTS, 1);
		}
		else
		{
			glRotatef(-90, 0, 0, 1);
			glScalef(TEXTURE_SCALE / texture.width, TEXTURE_SCALE / texture.height, 1);
		}
		glMatrixMode(GL_MODELVIEW);
	}
	else
	{
		pMat->pTexture = 0;
		gluQuadricTexture(qobj, GL_FALSE);
	}

	setMaterial(pMat);
	pMat->shininess = tempShin;

	if (pMat->alphaBlend)
		glEnable(GL_BLEND);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glTranslatef(edge, STRIP_HEIGHT / 2, 0);
	glRotatef(90, 0, 1, 0);
	/* -edge to give a black outline */
	gluCylinder(qobj, STRIP_HEIGHT / 2 - edge, STRIP_HEIGHT / 2 - edge, STRIP_WIDTH - edge * 2, 36, 36);
	glPopMatrix();

	if (pMat->textureInfo)
	{
		glMatrixMode(GL_TEXTURE);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}

	if (pMat->alphaBlend)
		glDisable(GL_BLEND);
}

void UpdatePreviewBar(Material* pMat, GdkPixmap *pixmap)
{
	GdkGC *gc;

	RenderPreview(pMat, auch);

	gc = gdk_gc_new(pixmap);
	gdk_draw_rgb_image(pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
					  auch, PREVIEW_WIDTH * 3 );
	gdk_gc_unref( gc );
}

void UpdateColourPreview(void *arg)
{
	double ambient[4], diffuse[4], specular[4];

	if (!bUpdate)
		return;

	if (useOpacity)
		opacityValue = padjOpacity->value / 100.0f;

	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpAmbient), ambient);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpDiffuse), diffuse);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpSpecular), specular);

	SetupMat(col3d, 
		ambient[0], ambient[1], ambient[2],
		diffuse[0], diffuse[1], diffuse[2],
		specular[0], specular[1], specular[2],
		padjShine->value, opacityValue);

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

	glViewport(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, STRIP_WIDTH, 0, STRIP_HEIGHT, -STRIP_HEIGHT, 0);
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

void TextureChange(GtkList *list, gpointer user_data)
{
	char* current = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry));

	if (current && *current && *lastTextureStr && (strcmp(current, lastTextureStr)))
	{
		strcpy(lastTextureStr, current);

		if (!strcmp(current, NO_TEXTURE_STRING))
			col3d->textureInfo = 0;
		else
			FindNamedTexture(&col3d->textureInfo, current);

		UpdateColourPreview(0);
	}
}

void AddWidgets(GdkWindow* pixWind, GtkWidget *window)
{
	GtkWidget *table, *label, *scale;
	table = gtk_table_new(5, 4, TRUE);
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

	pOpacitylabel = gtk_label_new("Opacity:");
	gtk_table_attach_defaults(GTK_TABLE (table), pOpacitylabel, 0, 1, 2, 3);
	padjOpacity = GTK_ADJUSTMENT(gtk_adjustment_new(0, 1, 100, 1, 10, 0));
	gtk_signal_connect_object( GTK_OBJECT( padjOpacity ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	psOpacity = gtk_hscale_new(padjOpacity);
	gtk_scale_set_digits( GTK_SCALE( psOpacity ), 0 );
	gtk_table_attach_defaults(GTK_TABLE (table), psOpacity, 1, 2, 2, 3);

	pTexturelabel = gtk_label_new("Texture:");
	gtk_table_attach_defaults(GTK_TABLE (table), pTexturelabel, 2, 3, 2, 3);
	textureCombo = gtk_combo_new();
	gtk_combo_set_value_in_list(GTK_COMBO(textureCombo), TRUE, FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(GTK_COMBO(textureCombo)->entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(textureCombo)->list), "selection-changed",
							GTK_SIGNAL_FUNC(TextureChange), 0);
	gtk_table_attach_defaults(GTK_TABLE (table), textureCombo, 2, 4, 3, 4);

	label = gtk_label_new("Preview:");
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 3, 4);

	xppm = gdk_pixmap_new(pixWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);
	CreatePreview();
	pwPreview = gtk_pixmap_new(xppm, NULL),
		gtk_table_attach_defaults(GTK_TABLE (table), pwPreview, 0, 2, 4, 5);
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

int IsSet(int flags, int bit)
{
	return ((flags & bit) == bit) ? 1 : 0;
}

void SetColour3d(GtkWidget *pw, UpdateDetails* pDetails)
{
	col3d = pDetails->pMat;

	/* Avoid updating preview */
	bUpdate = FALSE;

	/* Setup widgets */
	setCol(GTK_COLOUR_PICKER(pcpAmbient), col3d->ambientColour);
	setCol(GTK_COLOUR_PICKER(pcpDiffuse), col3d->diffuseColour);
	setCol(GTK_COLOUR_PICKER(pcpSpecular), col3d->specularColour);

	gtk_adjustment_set_value (padjShine, col3d->shininess);
	if (IsSet(pDetails->opacity, DF_VARIABLE_OPACITY))
	{
		useOpacity = 1;
		gtk_adjustment_set_value(padjOpacity, (col3d->ambientColour[3] + .001f) * 100);
	}
	else
	{
		useOpacity = 0;
		if (IsSet(pDetails->opacity, DF_FULL_ALPHA))
			opacityValue = 1;
		else
			opacityValue = 0;
	}

	if (IsSet(pDetails->texture, TT_NONE))
		useTexture = 0;
	else
	{
		GList *glist = GetTextureList(pDetails->texture);
		*lastTextureStr = '\0';	/* Don't update values */
		gtk_combo_set_popdown_strings(GTK_COMBO(textureCombo), glist);
		g_list_free(glist);

		if (pDetails->pMat->textureInfo)
		{
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry), pDetails->pMat->textureInfo->name);
			strcpy(lastTextureStr, pDetails->pMat->textureInfo->name);
		}
		else
			strcpy(lastTextureStr, NO_TEXTURE_STRING);
		useTexture = 1;

		gtk_widget_set_sensitive(GTK_WIDGET(textureCombo), !IsSet(pDetails->texture, TT_DISABLED));
	}

	/* Copy material - to reset if cancel pressed */
	memcpy(&cancelValue, col3d, sizeof(Material));

	/* show dialog */
	gtk_window_set_modal( GTK_WINDOW( pwColourDialog3d ), TRUE );
	gtk_window_set_transient_for( GTK_WINDOW( pwColourDialog3d ),
                                GTK_WINDOW( pwParent ) );
	gtk_widget_show_all( pwColourDialog3d );

	if (!useOpacity)
	{
		gtk_widget_hide(psOpacity);
		gtk_widget_hide(pOpacitylabel);
	}
	if (!useTexture)
	{
		gtk_widget_hide(textureCombo);
		gtk_widget_hide(pTexturelabel);
	}

	/* Update preview */
	bUpdate = TRUE;
	UpdateColourPreview(0);

	okPressed = FALSE;

	gtk_main();

	if (okPressed)
	{	/* Apply new settings */
		char* texStr;
		GdkGC *gc;
	
		if (useTexture)
		{
			texStr = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry));

			if (!strcmp(texStr, NO_TEXTURE_STRING))
				col3d->textureInfo = 0;
			else
				FindNamedTexture(&col3d->textureInfo, texStr);
		}
		gc = gdk_gc_new(pDetails->pixmap);
		gdk_draw_rgb_image(pDetails->pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
						  auch, PREVIEW_WIDTH * 3 );
		gdk_gc_unref( gc );

		gtk_widget_queue_draw(pDetails->preview);
		UpdatePreview(pDetails->parentPreview);
	}
}

void ResetPreviews()
{
	curDetail = 0;
}

int GetPreviewId()
{
	return curDetail - 1;
}

void UpdateColPreview(int ID)
{
	UpdatePreviewBar(details[ID].pMat, details[ID].pixmap);
	gtk_widget_queue_draw(details[ID].preview);
}

void UpdateColPreviews()
{
	int i;
	for (i = 0; i < curDetail; i++)
		UpdatePreviewBar(details[i].pMat, details[i].pixmap);
}

GtkWidget* gtk_colour_picker_new3d(GtkWidget** parentPreview, GdkWindow* pixWind, Material* pMat, int opacity, int texture)
{
	GtkWidget *pixmapwid, *button;
	GdkPixmap *pixmap;
	pixmap = gdk_pixmap_new(pixWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);

	button = gtk_button_new();
	pixmapwid = gtk_pixmap_new(pixmap, NULL);
	gtk_container_add(GTK_CONTAINER(button), pixmapwid);

	if (curDetail == MAX_DETAILS)
	{
		g_print("Error: Too many 3d colour previews\n");
		return 0;
	}
	details[curDetail].pMat = pMat;
	details[curDetail].pixmap = pixmap;
	details[curDetail].preview = button;
	details[curDetail].parentPreview = parentPreview;
	details[curDetail].opacity = opacity;
	details[curDetail].texture = texture;

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(SetColour3d), &details[curDetail]);

	UpdatePreviewBar(pMat, pixmap);
	curDetail++;
	return button;
}
