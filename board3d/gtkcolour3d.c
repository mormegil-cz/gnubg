/*
 * gtkcolour3d.c
 * by Jon Kinsey, 2003
 *
 * 3d colour selection dialog and preview area
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

#include "gtkprefs.h"

typedef struct UpdateDetails_T {
    Material mat;
    Material *pBoardMat;
    GtkWidget *preview;
    int opacity;
    TextureType textureType;
} UpdateDetails;

static GtkWidget *pcpAmbient, *pcpDiffuse, *pcpSpecular, *pwColourDialog3d;
static GtkAdjustment *padjShine, *padjOpacity;
static GtkWidget *psOpacity, *pOpacitylabel, *pTexturelabel, *pwPreview, *textureComboBox;
static Material col3d;
static Material cancelValue;
static int useOpacity, useTexture;
static float opacityValue;
static int bUpdate = TRUE;
static GLUquadricObj *qobj;
static UpdateDetails *curDetails;

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

static int previewLightLevels[3];

static int
IsFlagSet(int flags, int bit)
{
    return ((flags & bit) == bit) ? TRUE : FALSE;
}

void
SetPreviewLightLevel(const int levels[3])
{
    memcpy(previewLightLevels, levels, sizeof(int[3]));
}

void
InitColourSelectionDialog(void)
{
    pwColourDialog3d = NULL;
}

static void
SetupLight(void)
{
    float al[4], dl[4], sl[4];
    float lp[4] = { PREVIEW_WIDTH / 2, PREVIEW_HEIGHT / 2, 50, 1 };
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

static void
Draw(Material * pMat)
{
    Texture texture;
    int tempShin = pMat->shine;
    float edge = (1 / (float) PREVIEW_HEIGHT) * STRIP_HEIGHT;
    /* Accentuate shiness - so visible in preview */
    pMat->shine = tempShin / 3;

    if (pMat->textureInfo) {
        char buf[100];
        strcpy(buf, TEXTURE_PATH);
        strcat(buf, pMat->textureInfo->file);
        if (!LoadTexture(&texture, buf))
            return;

        gluQuadricTexture(qobj, GL_TRUE);

        pMat->pTexture = &texture;

        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        if (pMat->textureInfo->type == TT_HINGE) {
            glScalef(1.f, HINGE_SEGMENTS, 1.f);
        } else {
            glRotatef(-90.f, 0.f, 0.f, 1.f);
            glScalef(TEXTURE_SCALE / texture.width, TEXTURE_SCALE / texture.height, 1.f);
        }
        glMatrixMode(GL_MODELVIEW);
    } else {
        pMat->pTexture = 0;
        gluQuadricTexture(qobj, GL_FALSE);
    }

    setMaterial(pMat);
    pMat->shine = tempShin;

    if (pMat->alphaBlend)
        glEnable(GL_BLEND);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(edge * 2, STRIP_HEIGHT / 2, 0.f);
    glRotatef(90.f, 0.f, 1.f, 0.f);
    /* -edge to give a black outline */
    gluCylinder(qobj, (double) STRIP_HEIGHT / 2 - edge, (double) STRIP_HEIGHT / 2 - edge,
                (double) STRIP_WIDTH - edge * 3, 36, 36);
    glPopMatrix();

    if (pMat->textureInfo) {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    if (pMat->alphaBlend)
        glDisable(GL_BLEND);
}

static void
UpdateColourPreview(void)
{
    TextureInfo *tempTexture;
    double ambient[4], diffuse[4], specular[4];

    if (!bUpdate)
        return;

    if (useOpacity)
        opacityValue = (float) gtk_adjustment_get_value(padjOpacity) / 100.0f;

    gtk_color_button_get_array(GTK_COLOR_BUTTON(pcpAmbient), ambient);
    gtk_color_button_get_array(GTK_COLOR_BUTTON(pcpDiffuse), diffuse);
    gtk_color_button_get_array(GTK_COLOR_BUTTON(pcpSpecular), specular);

    tempTexture = col3d.textureInfo;    /* Remeber texture, as setupmat resets it */
    SetupMat(&col3d, (float) ambient[0], (float) ambient[1], (float) ambient[2],
             (float) diffuse[0], (float) diffuse[1], (float) diffuse[2],
             (float) specular[0], (float) specular[1], (float) specular[2],
             (int) gtk_adjustment_get_value(padjShine), opacityValue);
    col3d.textureInfo = tempTexture;

    gtk_widget_queue_draw(pwPreview);
}

static void
SetupColourPreview(void)
{
    InitGL(0);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    qobj = gluNewQuadric();
    gluQuadricDrawStyle(qobj, GLU_FILL);
    gluQuadricNormals(qobj, GLU_FLAT);

    glViewport(0, 0, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double) STRIP_WIDTH, 0.0, (double) STRIP_HEIGHT, (double) -STRIP_HEIGHT, 0.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void
TextureChange(GtkComboBox * combo, gpointer UNUSED(data))
{
    char *current = gtk_combo_box_get_active_text(combo);

    if (!bUpdate)
        return;

    if (!strcmp(current, NO_TEXTURE_STRING))
        col3d.textureInfo = 0;
    else
        FindNamedTexture(&col3d.textureInfo, current);

    UpdateColourPreview();
}

static gboolean
expose_event_preview3d(GtkWidget * widget, GdkEventExpose * UNUSED(eventDetails), Material * pMat)
{
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);

    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
        return TRUE;

    CheckOpenglError();

    SetupLight();
    Draw(pMat);

    gdk_gl_drawable_swap_buffers(gldrawable);

    gdk_gl_drawable_gl_end(gldrawable);

    return TRUE;
}

static void
realize_preview3d(GtkWidget * widget, void *UNUSED(data))
{
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(widget);
    if (!gdk_gl_drawable_gl_begin(gldrawable, gtk_widget_get_gl_context(widget)))
        return;

    SetupColourPreview();

    gdk_gl_drawable_gl_end(gldrawable);
}

static GtkWidget *
CreateGLPreviewWidget(Material * pMat)
{                               /* Rename this (and the one above to CreateGLBoardWidget) */
    GtkWidget *p3dWidget = gtk_drawing_area_new();

    /* Set OpenGL-capability to the widget - no list sharing */
    if (!gtk_widget_set_gl_capability(p3dWidget, getGlConfig(), NULL, TRUE, GDK_GL_RGBA_TYPE)) {
        outputerrf("Can't create opengl capable widget\n");
        return NULL;
    }

    if (p3dWidget == NULL) {
        outputerrf("Can't create opengl drawing widget\n");
        return NULL;
    }

    g_signal_connect(G_OBJECT(p3dWidget), "realize", G_CALLBACK(realize_preview3d), NULL);
    g_signal_connect(G_OBJECT(p3dWidget), "expose_event", G_CALLBACK(expose_event_preview3d), pMat);

    return p3dWidget;
}

static gboolean
combo_box_select_text(GtkTreeModel * model, GtkTreePath * UNUSED(path), GtkTreeIter * iter,     /*lint -e{818} */
                      gpointer text)
{
    gchar *value;

    gtk_tree_model_get(model, iter, 0, &value, -1);
    if (strcmp(value, text) == 0) {
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(textureComboBox), iter);
        g_free(value);
        return TRUE;
    }
    g_free(value);
    return FALSE;
}

static void
texture_set_active(void)
{
    if (col3d.textureInfo) {
        gtk_tree_model_foreach(gtk_combo_box_get_model(GTK_COMBO_BOX(textureComboBox)),
                               combo_box_select_text, col3d.textureInfo->name);
    } else
        gtk_combo_box_set_active(GTK_COMBO_BOX(textureComboBox), 0);
}

static void
AddWidgets(GtkWidget * window)
{
    GtkWidget *table, *label, *scale;
    table = gtk_table_new(5, 4, TRUE);
    gtk_container_add(GTK_CONTAINER(window), table);

    label = gtk_label_new(_("Ambient colour:"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
    pcpAmbient = gtk_color_button_new();
    g_signal_connect(G_OBJECT(pcpAmbient), "color-set", UpdateColourPreview, NULL);
    gtk_table_attach_defaults(GTK_TABLE(table), pcpAmbient, 1, 2, 0, 1);

    label = gtk_label_new(_("Diffuse colour:"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    pcpDiffuse = gtk_color_button_new();
    g_signal_connect(G_OBJECT(pcpDiffuse), "color-set", UpdateColourPreview, NULL);
    gtk_table_attach_defaults(GTK_TABLE(table), pcpDiffuse, 1, 2, 1, 2);

    label = gtk_label_new(_("Specular colour:"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, 0, 1);
    pcpSpecular = gtk_color_button_new();
    g_signal_connect(G_OBJECT(pcpSpecular), "color-set", UpdateColourPreview, NULL);
    gtk_table_attach_defaults(GTK_TABLE(table), pcpSpecular, 3, 4, 0, 1);

    label = gtk_label_new(_("Shine:"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, 1, 2);
    padjShine = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 128.0, 1.0, 10.0, 0.0));
    g_signal_connect(G_OBJECT(padjShine), "value-changed", G_CALLBACK(UpdateColourPreview), NULL);
    scale = gtk_hscale_new(padjShine);
    gtk_scale_set_digits(GTK_SCALE(scale), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), scale, 3, 4, 1, 2);

    pOpacitylabel = gtk_label_new(_("Opacity:"));
    gtk_table_attach_defaults(GTK_TABLE(table), pOpacitylabel, 0, 1, 2, 3);
    padjOpacity = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 1.0, 100.0, 1.0, 10.0, 0.0));
    g_signal_connect(G_OBJECT(padjOpacity), "value-changed", G_CALLBACK(UpdateColourPreview), NULL);
    psOpacity = gtk_hscale_new(padjOpacity);
    gtk_scale_set_digits(GTK_SCALE(psOpacity), 0);
    gtk_table_attach_defaults(GTK_TABLE(table), psOpacity, 1, 2, 2, 3);

    pTexturelabel = gtk_label_new(_("Texture:"));
    gtk_table_attach_defaults(GTK_TABLE(table), pTexturelabel, 2, 3, 2, 3);
    textureComboBox = gtk_combo_box_new_text();
    texture_set_active();
    gtk_widget_set_sensitive(textureComboBox, FALSE);
    g_signal_connect(textureComboBox, "changed", G_CALLBACK(TextureChange), NULL);
    gtk_table_attach_defaults(GTK_TABLE(table), textureComboBox, 2, 4, 3, 4);

    label = gtk_label_new(_("Preview:"));
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 3, 4);

    pwPreview = CreateGLPreviewWidget(&col3d);
    gtk_table_attach_defaults(GTK_TABLE(table), pwPreview, 0, 2, 4, 5);
}

static void
DialogClose(GtkDialog * UNUSED(dialog), gint response, void *UNUSED(data))
{
    if ((GtkResponseType) response == GTK_RESPONSE_OK) {        /* Apply new settings */
        char *texStr;

        /* Copy new settings to preview material */
        curDetails->mat = col3d;
        *curDetails->pBoardMat = col3d;

        if (useTexture) {
            texStr = gtk_combo_box_get_active_text(GTK_COMBO_BOX(textureComboBox));

            if (!strcmp(texStr, NO_TEXTURE_STRING))
                col3d.textureInfo = 0;
            else {
                BoardData *bd = (BOARD(pwPrevBoard))->board_data;
                ClearTextures(bd->bd3d);
                GetTextures(bd->bd3d, bd->rd);
            }
        }
        UpdatePreview();
        gtk_widget_queue_draw(curDetails->preview);
    }

    /* Hide dialog so can be reshown quickly */
    gtk_widget_hide(pwColourDialog3d);
}

static void
append_to_combo_box( /*lint -e{818} */ gpointer data, gpointer combo)
{
    gtk_combo_box_append_text(combo, data);
}

static void
gtk_color_button_set_from_farray(GtkColorButton * button, const float col[4])
{
    int i;
    double cold[4];
    for (i = 0; i < 4; i++)
        cold[i] = col[i];
    gtk_color_button_set_from_array(button, cold);
}

static void
UpdateColour3d(GtkButton * UNUSED(button), UpdateDetails * pDetails)
{
    curDetails = pDetails;
    col3d = pDetails->mat;

    if (pwColourDialog3d == NULL) {
        pwColourDialog3d = GTKCreateDialog(_("3d Colour selection"), DT_QUESTION,
                                           pDetails->preview, DIALOG_FLAG_MODAL | DIALOG_FLAG_NORESPONSE, NULL, NULL);
        AddWidgets(DialogArea(pwColourDialog3d, DA_MAIN));
        g_signal_connect(pwColourDialog3d, "response", G_CALLBACK(DialogClose), NULL);
    } else
        gtk_widget_show(pwColourDialog3d);

    /* Avoid updating preview */
    bUpdate = FALSE;

    /* Setup widgets */
    gtk_color_button_set_from_farray(GTK_COLOR_BUTTON(pcpAmbient), col3d.ambientColour);
    gtk_color_button_set_from_farray(GTK_COLOR_BUTTON(pcpDiffuse), col3d.diffuseColour);
    gtk_color_button_set_from_farray(GTK_COLOR_BUTTON(pcpSpecular), col3d.specularColour);

    gtk_adjustment_set_value(padjShine, (double) col3d.shine);
    if (IsFlagSet(pDetails->opacity, DF_VARIABLE_OPACITY)) {
        useOpacity = 1;
        gtk_adjustment_set_value(padjOpacity, col3d.alphaBlend ? (col3d.ambientColour[3] + .001) * 100 : 100.0);
    } else {
        useOpacity = 0;
        if (IsFlagSet(pDetails->opacity, DF_FULL_ALPHA))
            opacityValue = 1;
        else
            opacityValue = 0;
    }

    if (IsFlagSet(pDetails->textureType, TT_NONE))
        useTexture = 0;
    else {
        GList *glist = GetTextureList(pDetails->textureType);
        gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(textureComboBox))));
        g_list_foreach(glist, append_to_combo_box, textureComboBox);
        g_list_free(glist);

        texture_set_active();

        useTexture = 1;
        gtk_widget_set_sensitive(GTK_WIDGET(textureComboBox), !IsFlagSet(pDetails->textureType, TT_DISABLED));
    }

    /* Copy material - to reset if cancel pressed */
    memcpy(&cancelValue, &col3d, sizeof(Material));

    /* show dialog */
    gtk_widget_show_all(pwColourDialog3d);

    if (!useOpacity) {
        gtk_widget_hide(psOpacity);
        gtk_widget_hide(pOpacitylabel);
    }
    if (!useTexture) {
        gtk_widget_hide(textureComboBox);
        gtk_widget_hide(pTexturelabel);
    }

    /* Update preview */
    bUpdate = TRUE;
    UpdateColourPreview();

    gtk_main();
}

extern void
ResetPreviews(void)
{
    curDetail = 0;
}

extern int
GetPreviewId(void)
{
    return curDetail - 1;
}

void
UpdateColPreview(int ID)
{
    details[ID].mat = *details[ID].pBoardMat;
    gtk_widget_queue_draw(details[ID].preview);
}

extern void
UpdateColPreviews(void)
{
    int i;
    for (i = 0; i < curDetail; i++)
        UpdateColPreview(i);
}

GtkWidget *
gtk_colour_picker_new3d(Material * pMat, int opacity, TextureType textureType)
{
    GtkWidget *previewWidget, *button = gtk_button_new();

    if (curDetail == MAX_DETAILS) {
        outputerrf("Error: Too many 3d colour previews\n");
        return 0;
    }
    details[curDetail].mat = *pMat;
    previewWidget = CreateGLPreviewWidget(&details[curDetail].mat);
    gtk_widget_set_size_request(previewWidget, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    gtk_container_add(GTK_CONTAINER(button), previewWidget);

    details[curDetail].pBoardMat = pMat;        /* Material on board */
    details[curDetail].preview = button;
    details[curDetail].opacity = opacity;
    details[curDetail].textureType = textureType;

    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(UpdateColour3d), &details[curDetail]);

    curDetail++;

    return button;
}
