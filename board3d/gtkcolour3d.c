/*
* gtkcolour3d.c
* by Jon Kinsey, 2003
*
* 3d colour selection dialog and preview area
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

#include "inc3d.h"

#include "gtkcolour.h"
#include "gtkprefs.h"

static void RenderPreview(Material* pMat, unsigned char* buf);

static GtkWidget *pcpAmbient, *pcpDiffuse, *pcpSpecular;
static GtkAdjustment *padjShine, *padjOpacity;
static GtkWidget *psOpacity, *pOpacitylabel, *pTexturelabel, *pwPreview, *textureCombo;
static Material col3d;
static Material cancelValue;
static GdkPixmap *xppm = 0;
static int useOpacity, useTexture;
static float opacityValue;
static int bUpdate = TRUE;
static GLUquadricObj *qobj;
static char lastTextureStr[NAME_SIZE + 1];
static GdkWindow* refWind;

/* Store the previews details here */
#define MAX_DETAILS 15
static UpdateDetails details[MAX_DETAILS];
static int curDetail;

/* Size of screen widget */
#define PREVIEW_WIDTH 200
#define PREVIEW_HEIGHT 40

/* World sizes */
#define STRIP_WIDTH 100
#define STRIP_HEIGHT 10.f

static unsigned char auch[PREVIEW_WIDTH * PREVIEW_HEIGHT * 3];

static int previewLightLevels[3];

static int IsFlagSet(int flags, int bit)
{
	return ((flags & bit) == bit) ? TRUE : FALSE;
}

void SetPreviewLightLevel(const int levels[3])
{
	memcpy(previewLightLevels, levels, sizeof(int[3]));
}

static void SetupLight()
{
	float al[4], dl[4], sl[4];
	float lp[4] = {PREVIEW_WIDTH / 2, PREVIEW_HEIGHT / 2, 50, 1};
	glLightfv(GL_LIGHT0, GL_POSITION, lp);

	al[0] = al[1] = al[2] = previewLightLevels[0] / 100.0f;
	al[3] = 1;
	glLightfv(GL_LIGHT0, GL_AMBIENT, al);

	dl[0] = dl[1] = dl[2] = previewLightLevels[1] / 100.0f;
	dl[3] = 1;
	glLightfv(GL_LIGHT0, GL_DIFFUSE, dl);

	sl[0] = sl[1] = sl[2] = previewLightLevels[2] / 100.0f;
	sl[3] = 1;
	glLightfv(GL_LIGHT0, GL_SPECULAR, sl);
}

static void Draw(Material* pMat)
{
	Texture texture;
	int tempShin = pMat->shine;
	float edge = (1 / (float)PREVIEW_HEIGHT) * STRIP_HEIGHT;
	/* Accentuate shiness - so visible in preview */
	pMat->shine = tempShin / 3;

	if (pMat->textureInfo)
	{
		char buf[100];
		strcpy(buf, TEXTURE_PATH);
		strcat(buf, pMat->textureInfo->file);
		if (!LoadTexture(&texture, buf))
			return;

		gluQuadricTexture(qobj, GL_TRUE);

		pMat->pTexture = &texture;

		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		if (pMat->textureInfo->type == TT_HINGE)
		{
			glScalef(1.f, HINGE_SEGMENTS, 1.f);
		}
		else
		{
			glRotatef(-90.f, 0.f, 0.f, 1.f);
			glScalef(TEXTURE_SCALE / texture.width, TEXTURE_SCALE / texture.height, 1.f);
		}
		glMatrixMode(GL_MODELVIEW);
	}
	else
	{
		pMat->pTexture = 0;
		gluQuadricTexture(qobj, GL_FALSE);
	}

	setMaterial(pMat);
	pMat->shine = tempShin;

	if (pMat->alphaBlend)
		glEnable(GL_BLEND);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glTranslatef(edge, STRIP_HEIGHT / 2, 0.f);
	glRotatef(90.f, 0.f, 1.f, 0.f);
	/* -edge to give a black outline */
	gluCylinder(qobj, (double)STRIP_HEIGHT / 2 - edge, (double)STRIP_HEIGHT / 2 - edge, (double)STRIP_WIDTH - edge * 2, 36, 36);
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

static void UpdatePreviewBar(const Material* pMat, GdkPixmap *pixmap)
{
	GdkGC *gc;
	Material Copy = *pMat;

	RenderPreview(&Copy, auch);

	gc = gdk_gc_new(pixmap);
	gdk_draw_rgb_image(pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
					  auch, PREVIEW_WIDTH * 3 );
	g_object_unref( gc );
}

static void UpdateColourPreview(void *notused)
{
	double ambient[4], diffuse[4], specular[4];

	if (!bUpdate)
		return;

	if (useOpacity)
		opacityValue = (float)padjOpacity->value / 100.0f;

	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpAmbient), ambient);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpDiffuse), diffuse);
	gtk_colour_picker_get_colour(GTK_COLOUR_PICKER(pcpSpecular), specular);

	SetupMat(&col3d, (float)ambient[0], (float)ambient[1], (float)ambient[2],
		(float)diffuse[0], (float)diffuse[1], (float)diffuse[2],
		(float)specular[0], (float)specular[1], (float)specular[2],
		(int)padjShine->value, opacityValue);

	UpdatePreviewBar(&col3d, xppm);

	gtk_widget_queue_draw(pwPreview);
}

static void SetupColourPreview()
{
	InitGL(0);
	glClearColor(0.f, 0.f, 0.f, 1.f);

	qobj = gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL);
	gluQuadricNormals(qobj, GLU_FLAT);

	glViewport(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, (double)STRIP_WIDTH, 0.0, (double)STRIP_HEIGHT, (double)-STRIP_HEIGHT, 0.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

GdkGLConfig *getglconfigSingle(void);

static GdkGLPixmap *glpixmap;
static GdkGLContext *glPixmapContext = NULL;

static void CreatePreview()
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

static void RenderPreview(Material* pMat, unsigned char* buf)
{
	/*** OpenGL BEGIN ***/
	GdkGLDrawable *gldrawable = GDK_GL_DRAWABLE(glpixmap);

	if (!gdk_gl_drawable_gl_begin (gldrawable, glPixmapContext))
		return;

	SetupLight();
	Draw(pMat);

	glReadPixels(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buf);

	gdk_gl_drawable_gl_end(gldrawable);
	/*** OpenGL END ***/
}

static void TextureChange(void)
{
	char* current = (char *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry));

	if (current && *current && *lastTextureStr && (strcmp(current, lastTextureStr)))
	{
		strcpy(lastTextureStr, current);

		if (!strcmp(current, NO_TEXTURE_STRING))
			col3d.textureInfo = 0;
		else
			FindNamedTexture(&col3d.textureInfo, current);

		UpdateColourPreview(0);
	}
}

static void AddWidgets(GtkWidget *window)
{
	GtkWidget *table, *label, *scale;
	table = gtk_table_new(5, 4, TRUE);
	gtk_container_add(GTK_CONTAINER(window), table);

	label = gtk_label_new(_("Ambient colour:"));
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 0, 1);
	pcpAmbient = gtk_colour_picker_new((ColorPickerFunc)UpdateColourPreview, 0);
	gtk_table_attach_defaults(GTK_TABLE (table), pcpAmbient, 1, 2, 0, 1);

	label = gtk_label_new(_("Diffuse colour:"));
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 1, 2);
	pcpDiffuse = gtk_colour_picker_new((ColorPickerFunc)UpdateColourPreview, 0);
	gtk_table_attach_defaults(GTK_TABLE (table), pcpDiffuse, 1, 2, 1, 2);

	label = gtk_label_new(_("Specular colour:"));
	gtk_table_attach_defaults(GTK_TABLE (table), label, 2, 3, 0, 1);
	pcpSpecular = gtk_colour_picker_new((ColorPickerFunc)UpdateColourPreview, 0);
	gtk_table_attach_defaults(GTK_TABLE (table), pcpSpecular, 3, 4, 0, 1);

	label = gtk_label_new(_("Shine:"));
	gtk_table_attach_defaults(GTK_TABLE (table), label, 2, 3, 1, 2);
	padjShine = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 128.0, 1.0, 10.0, 0.0));
	gtk_signal_connect_object( GTK_OBJECT( padjShine ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	scale = gtk_hscale_new(padjShine);
	gtk_scale_set_digits( GTK_SCALE( scale ), 0 );
	gtk_table_attach_defaults(GTK_TABLE (table), scale, 3, 4, 1, 2);

	pOpacitylabel = gtk_label_new(_("Opacity:"));
	gtk_table_attach_defaults(GTK_TABLE (table), pOpacitylabel, 0, 1, 2, 3);
	padjOpacity = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 1.0, 100.0, 1.0, 10.0, 0.0));
	gtk_signal_connect_object( GTK_OBJECT( padjOpacity ),
			       "value-changed",
			       GTK_SIGNAL_FUNC( UpdateColourPreview ),
			       0 );
	psOpacity = gtk_hscale_new(padjOpacity);
	gtk_scale_set_digits( GTK_SCALE( psOpacity ), 0 );
	gtk_table_attach_defaults(GTK_TABLE (table), psOpacity, 1, 2, 2, 3);

	pTexturelabel = gtk_label_new(_("Texture:"));
	gtk_table_attach_defaults(GTK_TABLE (table), pTexturelabel, 2, 3, 2, 3);
	textureCombo = gtk_combo_new();
	gtk_combo_set_value_in_list(GTK_COMBO(textureCombo), TRUE, FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(GTK_COMBO(textureCombo)->entry), FALSE);
	gtk_signal_connect(GTK_OBJECT(GTK_COMBO(textureCombo)->list), "selection-changed",
							GTK_SIGNAL_FUNC(TextureChange), 0);
	gtk_table_attach_defaults(GTK_TABLE (table), textureCombo, 2, 4, 3, 4);

	label = gtk_label_new(_("Preview:"));
	gtk_table_attach_defaults(GTK_TABLE (table), label, 0, 1, 3, 4);

	pwPreview = gtk_pixmap_new(xppm, NULL);
	gtk_table_attach_defaults(GTK_TABLE (table), pwPreview, 0, 2, 4, 5);
}

static gboolean OkClicked(GtkWidget *pw, const UpdateDetails* pDetails)
{	/* Apply new settings */
	char* texStr;
	GdkGC *gc;

	/* Copy new settings to preview material */
	*pDetails->pMat = col3d;

	if (useTexture)
	{
		texStr = (char *)gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry));

		if (!strcmp(texStr, NO_TEXTURE_STRING))
			col3d.textureInfo = 0;
		else
		{
			BoardData *bd = (BOARD(pwPrevBoard))->board_data;
			ClearTextures(bd->bd3d);
			GetTextures(bd->bd3d, bd->rd);
		}
	}
	gc = gdk_gc_new(pDetails->pixmap);
	gdk_draw_rgb_image(pDetails->pixmap, gc, 0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT, GDK_RGB_DITHER_MAX,
						auch, PREVIEW_WIDTH * 3 );
	g_object_unref( gc );

	gtk_widget_queue_draw(pDetails->preview);
	UpdatePreview(0);

	/* Close dialog */
    gtk_widget_destroy(gtk_widget_get_toplevel(pw));

	return TRUE;
}

static void setCol(GtkColourPicker* pCP, const float val[4])
{
	double dval[4];
	int i;
	for (i = 0; i < 4; i++)
		dval[i] = val[i];

	gtk_colour_picker_set_colour(pCP, dval);
}

static void UpdateColour3d(GtkWidget *notused, UpdateDetails* pDetails)
{
	GtkWidget* pwColourDialog3d = GTKCreateDialog(_("3d Colour selection"), DT_QUESTION, 
		pDetails->preview, DIALOG_FLAG_MODAL, GTK_SIGNAL_FUNC(OkClicked), pDetails);

	gtk_signal_connect(GTK_OBJECT(pwColourDialog3d), "realize", GTK_SIGNAL_FUNC(UpdateColourPreview), 0 );

	AddWidgets(DialogArea(pwColourDialog3d, DA_MAIN));

	col3d = *pDetails->pMat;

	/* Avoid updating preview */
	bUpdate = FALSE;

	/* Setup widgets */
	setCol(GTK_COLOUR_PICKER(pcpAmbient), col3d.ambientColour);
	setCol(GTK_COLOUR_PICKER(pcpDiffuse), col3d.diffuseColour);
	setCol(GTK_COLOUR_PICKER(pcpSpecular), col3d.specularColour);

	gtk_adjustment_set_value (padjShine, (double)col3d.shine);
	if (IsFlagSet(pDetails->opacity, DF_VARIABLE_OPACITY))
	{
		useOpacity = 1;
		gtk_adjustment_set_value(padjOpacity, 
			col3d.alphaBlend ? (col3d.ambientColour[3] + .001) * 100 : 100.0);
	}
	else
	{
		useOpacity = 0;
		if (IsFlagSet(pDetails->opacity, DF_FULL_ALPHA))
			opacityValue = 1;
		else
			opacityValue = 0;
	}

	if (IsFlagSet(pDetails->textureType, TT_NONE))
		useTexture = 0;
	else
	{
		GList *glist = GetTextureList(pDetails->textureType);
		*lastTextureStr = '\0';	/* Don't update values */
		gtk_combo_set_popdown_strings(GTK_COMBO(textureCombo), glist);
		g_list_free(glist);

		if (col3d.textureInfo)
		{
			gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(textureCombo)->entry), col3d.textureInfo->name);
			strcpy(lastTextureStr, col3d.textureInfo->name);
		}
		else
			strcpy(lastTextureStr, NO_TEXTURE_STRING);
		useTexture = 1;
		gtk_widget_set_sensitive(GTK_WIDGET(textureCombo), !IsFlagSet(pDetails->textureType, TT_DISABLED));
	}

	/* Copy material - to reset if cancel pressed */
	memcpy(&cancelValue, &col3d, sizeof(Material));

	/* show dialog */
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

	gtk_main();
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

void Setup3dColourPicker(GtkWidget* notused, GdkWindow* wind)
{
	refWind = wind;
	if (!xppm)
		xppm = gdk_pixmap_new(refWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);
	CreatePreview();
}

GtkWidget* gtk_colour_picker_new3d(Material* pMat, int opacity, TextureType textureType)
{
	GtkWidget *pixmapwid, *button;
	GdkPixmap *pixmap;
	pixmap = gdk_pixmap_new(refWind, PREVIEW_WIDTH, PREVIEW_HEIGHT, -1);

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
	details[curDetail].opacity = opacity;
	details[curDetail].textureType = textureType;

	gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   GTK_SIGNAL_FUNC(UpdateColour3d), &details[curDetail]);

	UpdatePreviewBar(pMat, pixmap);
	curDetail++;
	return button;
}
