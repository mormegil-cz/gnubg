/* gtk-multiview.c
 * Copyright (C) 2000  Jonathan Blandford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

/* License changed from the GNU LGPL to the GNU GPL (as permitted
   under Term 3 of the GNU LGPL) by Gary Wong for distribution
   with GNU Backgammon. */

#include <gtk/gtk.h>
#include "gtk-multiview.h"

static void    gtk_multiview_init          (GtkMultiview      *multiview);
static void    gtk_multiview_class_init    (GtkMultiviewClass *klass);
static void    gtk_multiview_size_request  (GtkWidget         *widget,
					    GtkRequisition    *requisition);
static void    gtk_multiview_size_allocate (GtkWidget         *widget,
					    GtkAllocation     *allocation);
static void    gtk_multiview_map           (GtkWidget         *widget);
static void    gtk_multiview_unmap         (GtkWidget         *widget);
static int     gtk_multiview_expose        (GtkWidget         *widget,
					    GdkEventExpose    *event);
static GtkType gtk_multiview_child_type    (GtkContainer      *container);
static void    gtk_multiview_forall        (GtkContainer      *container,
					    gboolean           include_internals,
					    GtkCallback        callback,
					    gpointer           callback_data);
static void    gtk_multiview_add           (GtkContainer      *widget,
					    GtkWidget         *child);
static void    gtk_multiview_remove        (GtkContainer      *widget,
					    GtkWidget         *child);


static GtkContainerClass *parent_class = NULL;

GtkType
gtk_multiview_get_type (void)
{
  static GtkType multiview_type = 0;

  if (!multiview_type)
    {
      static const GtkTypeInfo multiview_info =
      {
        "GtkMultiview",
        sizeof (GtkMultiview),
        sizeof (GtkMultiviewClass),
        (GtkClassInitFunc) gtk_multiview_class_init,
        (GtkObjectInitFunc) gtk_multiview_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      multiview_type = gtk_type_unique (gtk_container_get_type (), &multiview_info);
    }

  return multiview_type;
}

static void
gtk_multiview_init (GtkMultiview *multiview)
{
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET (multiview), GTK_NO_WINDOW);

  multiview->current = NULL;
  multiview->children = NULL;
}

static void
gtk_multiview_class_init (GtkMultiviewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  parent_class = gtk_type_class (gtk_container_get_type ());

  widget_class->size_request = gtk_multiview_size_request;
  widget_class->size_allocate = gtk_multiview_size_allocate;
  widget_class->map = gtk_multiview_map;
  widget_class->unmap = gtk_multiview_unmap;
  widget_class->expose_event = gtk_multiview_expose;

  container_class->forall = gtk_multiview_forall;
  container_class->add = gtk_multiview_add;
  container_class->remove = gtk_multiview_remove;
  container_class->child_type = gtk_multiview_child_type;
}

static void
gtk_multiview_size_request  (GtkWidget      *widget,
			     GtkRequisition *requisition)
{
  GList *tmp_list;
  GtkMultiview *multiview;
  GtkRequisition child_requisition;
  GtkWidget *child;

  multiview = GTK_MULTIVIEW (widget);

  requisition->width = 0;
  requisition->height = 0;
  /* We find the maximum size of all children widgets */
  tmp_list = multiview->children;
  while (tmp_list)
    {
      child = GTK_WIDGET (tmp_list->data);
      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_size_request (child, &child_requisition);
	  requisition->width = MAX (requisition->width, child_requisition.width);
	  requisition->height = MAX (requisition->height, child_requisition.height);
	  if (GTK_WIDGET_MAPPED (child) && child != multiview->current)
	    {
	      gtk_widget_unmap (GTK_WIDGET(child));
	    }
	}
    }
}
static void
gtk_multiview_size_allocate (GtkWidget     *widget,
			     GtkAllocation *allocation)
{
  GtkMultiview *multiview;
  GList *tmp_list;
  GtkWidget *child;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (widget));

  multiview = GTK_MULTIVIEW (widget);
  widget->allocation = *allocation;

  tmp_list = multiview->children;
  while (tmp_list)
    {
      child = GTK_WIDGET (tmp_list->data);
      tmp_list = tmp_list->next;

      if (GTK_WIDGET_VISIBLE (child))
	{
	  gtk_widget_size_allocate (child, allocation);
	}
    }
}

static void
gtk_multiview_map (GtkWidget *widget)
{
  GtkMultiview *multiview;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (widget));

  multiview = GTK_MULTIVIEW (widget);
  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  if (multiview->current &&
      GTK_WIDGET_VISIBLE (multiview->current) &&
      !GTK_WIDGET_MAPPED (multiview->current))
    {
      gtk_widget_map (GTK_WIDGET (multiview->current));
    }
}

static void
gtk_multiview_unmap (GtkWidget *widget)
{
  GtkMultiview *multiview;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (widget));

  multiview = GTK_MULTIVIEW (widget);
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  if (multiview->current &&
      GTK_WIDGET_VISIBLE (multiview->current) &&
      GTK_WIDGET_MAPPED (multiview->current))
    {
      gtk_widget_unmap (GTK_WIDGET (multiview->current));
    }
}

static gint
gtk_multiview_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkMultiview *multiview;
  GtkWidget *child;
  GList *tmp_list;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MULTIVIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      multiview = GTK_MULTIVIEW (widget);
      child_event = *event;

      tmp_list = multiview->children;
      while (tmp_list)
	{
	  child = GTK_WIDGET (tmp_list->data);
	  tmp_list = tmp_list->next;

	  if (GTK_WIDGET_DRAWABLE (child) &&
	      GTK_WIDGET_NO_WINDOW (child))
	    {
	      gtk_widget_event (child, (GdkEvent*) event);
	    }
	}
    }
  return FALSE;
}

static GtkType
gtk_multiview_child_type (GtkContainer *container)
{
  return gtk_widget_get_type ();
}

static void
gtk_multiview_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkWidget *child;
  GtkMultiview *multiview;
  GList *tmp_list;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (container));
  g_return_if_fail (callback != NULL);

  multiview = GTK_MULTIVIEW (container);

  tmp_list = multiview->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;
      (* callback) (GTK_WIDGET (child), callback_data);
    }
}

static void
gtk_multiview_add (GtkContainer *container,
		   GtkWidget    *child)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (container));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_multiview_append_child (GTK_MULTIVIEW (container), child);
}

static void
gtk_multiview_remove (GtkContainer *container,
		      GtkWidget    *child)
{
  GtkMultiview *multiview;
  GList *list;
	
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (container));
  g_return_if_fail (child != NULL);

  multiview = GTK_MULTIVIEW (container);

  list = g_list_find (multiview->children, child);
  g_return_if_fail (list != NULL);
  
  /* If we are mapped and visible, we want to deal with changing the page. */
  if ((GTK_WIDGET_MAPPED (GTK_WIDGET (container))) &&
      (list->data == (gpointer) multiview->current) &&
      (list->next != NULL))
    {
      gtk_multiview_set_current (multiview, GTK_WIDGET (list->next->data));
    }

  multiview->children = g_list_remove (multiview->children, child);
  gtk_widget_unparent (child);
}

/* Public Functions */
GtkWidget *
gtk_multiview_new ()
{
  return GTK_WIDGET (gtk_type_new (gtk_multiview_get_type ()));
}

void
gtk_multiview_prepend_child (GtkMultiview *multiview,
			     GtkWidget    *child)
{
  g_return_if_fail (multiview != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (multiview));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_multiview_insert_child (multiview, NULL, child);
}

void
gtk_multiview_insert_child (GtkMultiview *multiview,
			    GtkWidget    *back_child,
			    GtkWidget    *child)
{
  GList *list;

  g_return_if_fail (multiview != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (multiview));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  list = g_list_find (multiview->children, back_child);
  if (list == NULL)
    {
      multiview->children = g_list_prepend (multiview->children, child);
    }
  else
    {
      GList *new_el = g_list_alloc ();

      new_el->next = list->next;
      new_el->prev = list;
      if (new_el->next) 
	new_el->next->prev = new_el;
      new_el->prev->next = new_el;
      new_el->data = (gpointer) child;
    }
  gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (multiview));

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (multiview)))
    gtk_widget_realize (GTK_WIDGET (child));

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (multiview)) && GTK_WIDGET_VISIBLE (GTK_WIDGET (child)))
    {
      if (GTK_WIDGET_MAPPED (GTK_WIDGET (child)))
	gtk_widget_unmap (GTK_WIDGET (child));
      gtk_widget_queue_resize (GTK_WIDGET (multiview));
    }

  /* if it's the first and only entry, we want to bring it to the foreground. */
  if (multiview->children->next == NULL)
    gtk_multiview_set_current (multiview, child);
}

void
gtk_multiview_append_child (GtkMultiview *multiview,
			    GtkWidget    *child)
{
  GList *list;

  g_return_if_fail (multiview != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (multiview));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));
  
  list = g_list_last (multiview->children);
  if (list)
    {
      gtk_multiview_insert_child (multiview, GTK_WIDGET (list->data), child);
    }
  else
    {
      gtk_multiview_insert_child (multiview, NULL, child);
    }	
}

void
gtk_multiview_set_current (GtkMultiview *multiview,
			   GtkWidget    *child)
{
  GList *list;
  GtkWidget *old = NULL;

  g_return_if_fail (multiview != NULL);
  g_return_if_fail (GTK_IS_MULTIVIEW (multiview));
  g_return_if_fail (child != NULL);
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (multiview->current == child)
    return;
  
  list = g_list_find (multiview->children, child);
  g_return_if_fail (list != NULL);

  if ((multiview->current) &&
      (GTK_WIDGET_VISIBLE (multiview->current)) &&
      (GTK_WIDGET_MAPPED (multiview)))
    {
      old = GTK_WIDGET (multiview->current);
    }

  multiview->current = GTK_WIDGET (list->data);
  if (GTK_WIDGET_VISIBLE (multiview->current) &&
      (GTK_WIDGET_MAPPED (multiview)))
    {
      gtk_widget_map (multiview->current);
    }
  if (old && GTK_WIDGET_MAPPED (old))
    gtk_widget_unmap (old);
}
