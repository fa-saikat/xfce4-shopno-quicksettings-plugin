/*
 * Copyright (C) 2026 FA Saikat
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

#include "config.h"

#include "shopno-qs-tray-icon.h"

struct _ShopnoQsTrayIcon
{
  GtkButton         __parent__;

  ShopnoQsTrayItem *item;
  GtkWidget        *image;
  gint              icon_size;
  gdouble           smooth_delta;
};

G_DEFINE_TYPE (ShopnoQsTrayIcon, shopno_qs_tray_icon, GTK_TYPE_BUTTON)



/* Apps that ship their own icons name a directory for them. Keep one
 * icon theme per directory, falling back to the user's theme. */
static GtkIconTheme *
shopno_qs_tray_icon_theme_for_path (const gchar *path)
{
  static GHashTable *themes = NULL;
  GtkIconTheme      *theme;

  if (themes == NULL)
    themes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  theme = g_hash_table_lookup (themes, path);
  if (theme == NULL)
    {
      theme = gtk_icon_theme_new ();
      gtk_icon_theme_set_screen (theme, gdk_screen_get_default ());
      gtk_icon_theme_prepend_search_path (theme, path);
      g_hash_table_insert (themes, g_strdup (path), theme);
    }

  return theme;
}



static GdkPixbuf *
shopno_qs_tray_icon_load (ShopnoQsTrayIcon *self,
                          gint              scale)
{
  const gchar *name;
  const gchar *path;
  GdkPixbuf   *pixbuf = NULL;
  GdkPixbuf   *pixmap;
  GtkIconInfo *info = NULL;
  gint         size = self->icon_size * scale;

  shopno_qs_tray_item_get_icon (self->item, &name, &path, &pixmap);

  if (name != NULL && g_path_is_absolute (name))
    {
      pixbuf = gdk_pixbuf_new_from_file_at_size (name, size, size, NULL);
    }
  else if (name != NULL)
    {
      if (path != NULL)
        info = gtk_icon_theme_lookup_icon_for_scale (shopno_qs_tray_icon_theme_for_path (path), name,
                                                     self->icon_size, scale, GTK_ICON_LOOKUP_FORCE_SIZE);
      if (info == NULL)
        info = gtk_icon_theme_lookup_icon_for_scale (gtk_icon_theme_get_default (), name,
                                                     self->icon_size, scale, GTK_ICON_LOOKUP_FORCE_SIZE);
      if (info != NULL)
        {
          /* Symbolic icons take the panel's text color */
          pixbuf = gtk_icon_info_load_symbolic_for_context (info,
                                                            gtk_widget_get_style_context (GTK_WIDGET (self)),
                                                            NULL, NULL);
          g_object_unref (info);
        }
    }

  if (pixbuf == NULL && pixmap != NULL)
    pixbuf = gdk_pixbuf_scale_simple (pixmap, size, size, GDK_INTERP_BILINEAR);

  return pixbuf;
}



static void
shopno_qs_tray_icon_update (ShopnoQsTrayIcon *self)
{
  gint             scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  GdkPixbuf       *pixbuf;
  cairo_surface_t *surface;
  gchar           *tooltip;

  gtk_widget_set_visible (GTK_WIDGET (self), shopno_qs_tray_item_get_visible (self->item));

  pixbuf = shopno_qs_tray_icon_load (self, scale);
  if (pixbuf != NULL)
    {
      surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
      gtk_image_set_from_surface (GTK_IMAGE (self->image), surface);
      cairo_surface_destroy (surface);
      g_object_unref (pixbuf);
    }
  else
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (self->image), "image-missing", GTK_ICON_SIZE_BUTTON);
      gtk_image_set_pixel_size (GTK_IMAGE (self->image), self->icon_size);
    }

  tooltip = shopno_qs_tray_item_get_tooltip (self->item);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), tooltip);
  g_free (tooltip);
}



/*
 * Input
 */

static gboolean
shopno_qs_tray_icon_button_release_event (GtkWidget      *widget,
                                          GdkEventButton *event)
{
  ShopnoQsTrayIcon *self = SHOPNO_QS_TRAY_ICON (widget);
  gint              x = (gint) event->x_root;
  gint              y = (gint) event->y_root;

  GTK_WIDGET_CLASS (shopno_qs_tray_icon_parent_class)->button_release_event (widget, event);

  switch (event->button)
    {
    case 1:
      /* Some apps only have a menu */
      if (shopno_qs_tray_item_get_is_menu (self->item))
        shopno_qs_tray_item_context_menu (self->item, x, y);
      else
        shopno_qs_tray_item_activate (self->item, x, y);
      break;

    case 2:
      shopno_qs_tray_item_secondary_activate (self->item, x, y);
      break;

    case 3:
      shopno_qs_tray_item_context_menu (self->item, x, y);
      break;

    default:
      break;
    }

  return GDK_EVENT_STOP;
}



static gboolean
shopno_qs_tray_icon_button_press_event (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  GTK_WIDGET_CLASS (shopno_qs_tray_icon_parent_class)->button_press_event (widget, event);

  /* Keep right clicks from opening the panel's own menu */
  return GDK_EVENT_STOP;
}



static gboolean
shopno_qs_tray_icon_scroll_event (GtkWidget      *widget,
                                  GdkEventScroll *event)
{
  ShopnoQsTrayIcon *self = SHOPNO_QS_TRAY_ICON (widget);
  gdouble           dx, dy;
  gint              steps;

  switch (event->direction)
    {
    case GDK_SCROLL_UP:
      shopno_qs_tray_item_scroll (self->item, -1, GTK_ORIENTATION_VERTICAL);
      break;

    case GDK_SCROLL_DOWN:
      shopno_qs_tray_item_scroll (self->item, 1, GTK_ORIENTATION_VERTICAL);
      break;

    case GDK_SCROLL_LEFT:
      shopno_qs_tray_item_scroll (self->item, -1, GTK_ORIENTATION_HORIZONTAL);
      break;

    case GDK_SCROLL_RIGHT:
      shopno_qs_tray_item_scroll (self->item, 1, GTK_ORIENTATION_HORIZONTAL);
      break;

    case GDK_SCROLL_SMOOTH:
      /* Touchpads: one step per unit of scrolling */
      gdk_event_get_scroll_deltas ((GdkEvent *) event, &dx, &dy);
      self->smooth_delta += dy;
      steps = (gint) self->smooth_delta;
      self->smooth_delta -= steps;
      if (steps != 0)
        shopno_qs_tray_item_scroll (self->item, steps, GTK_ORIENTATION_VERTICAL);
      break;

    default:
      break;
    }

  return GDK_EVENT_STOP;
}



/*
 * GObject
 */

static void
shopno_qs_tray_icon_style_updated (GtkWidget *widget)
{
  ShopnoQsTrayIcon *self = SHOPNO_QS_TRAY_ICON (widget);

  GTK_WIDGET_CLASS (shopno_qs_tray_icon_parent_class)->style_updated (widget);

  /* Symbolic icons follow the text color of the theme */
  if (self->item != NULL)
    shopno_qs_tray_icon_update (self);
}



static void
shopno_qs_tray_icon_dispose (GObject *object)
{
  ShopnoQsTrayIcon *self = SHOPNO_QS_TRAY_ICON (object);

  if (self->item != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->item, self);
      g_clear_object (&self->item);
    }

  G_OBJECT_CLASS (shopno_qs_tray_icon_parent_class)->dispose (object);
}



static void
shopno_qs_tray_icon_class_init (ShopnoQsTrayIconClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = shopno_qs_tray_icon_dispose;

  widget_class->button_press_event = shopno_qs_tray_icon_button_press_event;
  widget_class->button_release_event = shopno_qs_tray_icon_button_release_event;
  widget_class->scroll_event = shopno_qs_tray_icon_scroll_event;
  widget_class->style_updated = shopno_qs_tray_icon_style_updated;
}



static void
shopno_qs_tray_icon_init (ShopnoQsTrayIcon *self)
{
  self->icon_size = 16;

  gtk_button_set_relief (GTK_BUTTON (self), GTK_RELIEF_NONE);
  gtk_widget_set_focus_on_click (GTK_WIDGET (self), FALSE);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-tray-icon");

  self->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (self), self->image);
  gtk_widget_show (self->image);
}



GtkWidget *
shopno_qs_tray_icon_new (ShopnoQsTrayItem *item)
{
  ShopnoQsTrayIcon *self = g_object_new (SHOPNO_TYPE_QS_TRAY_ICON, NULL);

  self->item = g_object_ref (item);
  g_signal_connect_swapped (item, "changed", G_CALLBACK (shopno_qs_tray_icon_update), self);
  shopno_qs_tray_icon_update (self);

  return GTK_WIDGET (self);
}



ShopnoQsTrayItem *
shopno_qs_tray_icon_get_item (ShopnoQsTrayIcon *icon)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ICON (icon), NULL);

  return icon->item;
}



void
shopno_qs_tray_icon_set_icon_size (ShopnoQsTrayIcon *icon,
                                   gint              size)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_ICON (icon));

  if (icon->icon_size == size)
    return;

  icon->icon_size = size;
  shopno_qs_tray_icon_update (icon);
}
