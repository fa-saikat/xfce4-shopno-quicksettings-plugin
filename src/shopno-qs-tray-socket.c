/*
 * Copyright (C) 2026 FA Saikat
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * An icon with an alpha channel is composited over what is behind it, as
 * in systray-socket.c of Xfce's systray plugin. GTK leaves the area under
 * a native child window unpainted, so the backgrounds of the socket's
 * parents are drawn there first.
 *
 * An icon without one draws on the socket's background, the color behind
 * it. Xfce's plugin lets the panel's background show through instead,
 * which needs the same depth all the way up: not so in an external
 * plugin, nor across a move between the panel and the flyout.
 */

#include "config.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "shopno-qs-tray-socket.h"

struct _ShopnoQsTraySocket
{
  GtkSocket         __parent__;

  ShopnoQsTrayItem *item;
  gint              size;
  gboolean          composited;
};

G_DEFINE_TYPE (ShopnoQsTraySocket, shopno_qs_tray_socket, GTK_TYPE_SOCKET)



/* The backgrounds of the socket's parents, from the toplevel down, in
 * the coordinates of @origin */
static void
shopno_qs_tray_socket_render_parents (ShopnoQsTraySocket *self,
                                      GtkWidget          *origin,
                                      cairo_t            *cr)
{
  GSList *ancestors = NULL;

  for (GtkWidget *ancestor = gtk_widget_get_parent (GTK_WIDGET (self));
       ancestor != NULL;
       ancestor = gtk_widget_get_parent (ancestor))
    ancestors = g_slist_prepend (ancestors, ancestor);

  for (GSList *l = ancestors; l != NULL; l = l->next)
    {
      GtkWidget *ancestor = l->data;
      gint       x, y;

      if (gtk_widget_translate_coordinates (ancestor, origin, 0, 0, &x, &y))
        gtk_render_background (gtk_widget_get_style_context (ancestor), cr, x, y,
                               gtk_widget_get_allocated_width (ancestor),
                               gtk_widget_get_allocated_height (ancestor));
    }

  g_slist_free (ancestors);
}



/* The color behind the middle of the socket, for an icon without alpha */
static void
shopno_qs_tray_socket_update_background (ShopnoQsTraySocket *self)
{
  GtkWidget       *widget = GTK_WIDGET (self);
  GtkWidget       *toplevel = gtk_widget_get_toplevel (widget);
  cairo_surface_t *surface;
  cairo_t         *cr;
  guint32          pixel;
  GdkRGBA          color;
  gint             x, y;

  if (self->composited || !gtk_widget_get_realized (widget))
    return;

  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 1, 1);
  cr = cairo_create (surface);

  if (gtk_widget_translate_coordinates (widget, toplevel,
                                        gtk_widget_get_allocated_width (widget) / 2,
                                        gtk_widget_get_allocated_height (widget) / 2, &x, &y))
    {
      cairo_translate (cr, -x, -y);
      shopno_qs_tray_socket_render_parents (self, toplevel, cr);
    }

  cairo_destroy (cr);
  cairo_surface_flush (surface);
  pixel = *(guint32 *) cairo_image_surface_get_data (surface);
  cairo_surface_destroy (surface);

  color.red = ((pixel >> 16) & 0xff) / 255.0;
  color.green = ((pixel >> 8) & 0xff) / 255.0;
  color.blue = (pixel & 0xff) / 255.0;
  color.alpha = 1.0;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_set_background_rgba (gtk_widget_get_window (widget), &color);
  G_GNUC_END_IGNORE_DEPRECATIONS
}



/* A composited socket's window is drawn by its parent */
static gboolean
shopno_qs_tray_socket_parent_draw (GtkWidget          *parent,
                                   cairo_t            *cr,
                                   ShopnoQsTraySocket *self)
{
  GtkWidget     *widget = GTK_WIDGET (self);
  GtkAllocation  alloc;
  gint           x, y;

  if (!self->composited || !gtk_widget_get_mapped (widget)
      || !gtk_widget_translate_coordinates (widget, parent, 0, 0, &x, &y))
    return GDK_EVENT_PROPAGATE;

  gtk_widget_get_allocation (widget, &alloc);

  cairo_save (cr);
  cairo_rectangle (cr, x, y, alloc.width, alloc.height);
  cairo_clip (cr);
  shopno_qs_tray_socket_render_parents (self, parent, cr);
  gdk_cairo_set_source_window (cr, gtk_widget_get_window (widget), x, y);
  cairo_paint (cr);
  cairo_restore (cr);

  return GDK_EVENT_PROPAGATE;
}



/*
 * GtkWidget
 */

/* The icon size, not the size the app asks for: apps draw their icon to
 * fit whatever they get, and remember the last size */
static void
shopno_qs_tray_socket_get_preferred_size (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
  *minimum = *natural = SHOPNO_QS_TRAY_SOCKET (widget)->size;
}



static void
shopno_qs_tray_socket_realize (GtkWidget *widget)
{
  ShopnoQsTraySocket *self = SHOPNO_QS_TRAY_SOCKET (widget);
  GdkRGBA             transparent = { 0.0, 0.0, 0.0, 0.0 };
  GdkWindow          *window;

  GTK_WIDGET_CLASS (shopno_qs_tray_socket_parent_class)->realize (widget);

  window = gtk_widget_get_window (widget);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (self->composited)
    {
      gdk_window_set_background_rgba (window, &transparent);
      gdk_window_set_composited (window, TRUE);
      gtk_widget_set_double_buffered (widget, FALSE);
    }
  G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_set_app_paintable (widget, self->composited);
}



static void
shopno_qs_tray_socket_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  ShopnoQsTraySocket *self = SHOPNO_QS_TRAY_SOCKET (widget);
  GtkAllocation       old;
  gboolean            changed;

  gtk_widget_get_allocation (widget, &old);
  changed = allocation->x != old.x || allocation->y != old.y
            || allocation->width != old.width || allocation->height != old.height;

  if (changed && self->composited && gtk_widget_get_mapped (widget))
    gdk_window_invalidate_rect (gdk_window_get_parent (gtk_widget_get_window (widget)), &old, FALSE);

  GTK_WIDGET_CLASS (shopno_qs_tray_socket_parent_class)->size_allocate (widget, allocation);

  if (changed && self->composited && gtk_widget_get_mapped (widget))
    gdk_window_invalidate_rect (gdk_window_get_parent (gtk_widget_get_window (widget)), allocation, FALSE);
  else if (changed)
    shopno_qs_tray_socket_update_background (self);
}



static gboolean
shopno_qs_tray_socket_draw (GtkWidget *widget,
                            cairo_t   *cr)
{
  /* Transparent, for the icon to be composited over */
  if (SHOPNO_QS_TRAY_SOCKET (widget)->composited)
    {
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_paint (cr);
    }

  return GDK_EVENT_PROPAGATE;
}



static void
shopno_qs_tray_socket_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (shopno_qs_tray_socket_parent_class)->style_updated (widget);

  shopno_qs_tray_socket_update_background (SHOPNO_QS_TRAY_SOCKET (widget));
}



static void
shopno_qs_tray_socket_parent_set (GtkWidget *widget,
                                  GtkWidget *old_parent)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);

  if (old_parent != NULL)
    g_signal_handlers_disconnect_by_func (old_parent, shopno_qs_tray_socket_parent_draw, widget);

  if (parent != NULL)
    g_signal_connect_after (parent, "draw", G_CALLBACK (shopno_qs_tray_socket_parent_draw), widget);
}



/*
 * GObject
 */

static void
shopno_qs_tray_socket_dispose (GObject *object)
{
  g_clear_object (&SHOPNO_QS_TRAY_SOCKET (object)->item);

  G_OBJECT_CLASS (shopno_qs_tray_socket_parent_class)->dispose (object);
}



static void
shopno_qs_tray_socket_class_init (ShopnoQsTraySocketClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = shopno_qs_tray_socket_dispose;

  widget_class->get_preferred_width = shopno_qs_tray_socket_get_preferred_size;
  widget_class->get_preferred_height = shopno_qs_tray_socket_get_preferred_size;
  widget_class->realize = shopno_qs_tray_socket_realize;
  widget_class->size_allocate = shopno_qs_tray_socket_size_allocate;
  widget_class->draw = shopno_qs_tray_socket_draw;
  widget_class->style_updated = shopno_qs_tray_socket_style_updated;
  widget_class->parent_set = shopno_qs_tray_socket_parent_set;
}



static void
shopno_qs_tray_socket_init (ShopnoQsTraySocket *self)
{
  self->size = 16;
}



GtkWidget *
shopno_qs_tray_socket_new (ShopnoQsTrayItem *item)
{
  GdkScreen          *screen = gdk_screen_get_default ();
  GdkDisplay         *display = gdk_screen_get_display (screen);
  ShopnoQsTraySocket *self;
  XWindowAttributes   attr;
  GdkVisual          *visual;
  gint                red, green, blue;
  gint                result;

  gdk_x11_display_error_trap_push (display);
  result = XGetWindowAttributes (GDK_DISPLAY_XDISPLAY (display),
                                 shopno_qs_tray_item_get_xembed_window (item), &attr);
  if (gdk_x11_display_error_trap_pop (display) != 0 || result == 0)
    return NULL;

  /* The socket takes the icon's visual, alpha channel and all */
  visual = gdk_x11_screen_lookup_visual (screen, attr.visual->visualid);
  if (visual == NULL)
    return NULL;

  self = g_object_new (SHOPNO_TYPE_QS_TRAY_SOCKET, NULL);
  self->item = g_object_ref (item);
  gtk_widget_set_visual (GTK_WIDGET (self), visual);

  gdk_visual_get_red_pixel_details (visual, NULL, NULL, &red);
  gdk_visual_get_green_pixel_details (visual, NULL, NULL, &green);
  gdk_visual_get_blue_pixel_details (visual, NULL, NULL, &blue);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  self->composited = red + green + blue < gdk_visual_get_depth (visual)
                     && gdk_display_supports_composite (display);
  G_GNUC_END_IGNORE_DEPRECATIONS

  return GTK_WIDGET (self);
}



ShopnoQsTrayItem *
shopno_qs_tray_socket_get_item (ShopnoQsTraySocket *socket)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_SOCKET (socket), NULL);

  return socket->item;
}



void
shopno_qs_tray_socket_set_size (ShopnoQsTraySocket *socket,
                                gint                size)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_SOCKET (socket));

  socket->size = size;
  gtk_widget_queue_resize (GTK_WIDGET (socket));
}



void
shopno_qs_tray_socket_moved (ShopnoQsTraySocket *socket)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_SOCKET (socket));

  shopno_qs_tray_socket_update_background (socket);
}
