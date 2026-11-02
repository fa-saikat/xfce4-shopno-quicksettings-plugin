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

enum
{
  ACTIVATED,
  MENU_DISMISSED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

/* Whether an item of the open menu was chosen; one menu is open at a time */
static gboolean menu_item_chosen;

struct _ShopnoQsTrayIcon
{
  GtkButton         __parent__;

  ShopnoQsTrayItem *item;
  GtkWidget        *image;
  gint              icon_size;

  /* The item changed while the icon was off screen */
  gboolean          stale;
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



/* Each app has an icon on the panel and one in the overflow flyout, and
 * at most one of them is on screen; the other catches up when shown */
static void
shopno_qs_tray_icon_changed (ShopnoQsTrayIcon *self)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    shopno_qs_tray_icon_update (self);
  else
    self->stale = TRUE;
}



static void
shopno_qs_tray_icon_map (GtkWidget *widget)
{
  ShopnoQsTrayIcon *self = SHOPNO_QS_TRAY_ICON (widget);

  GTK_WIDGET_CLASS (shopno_qs_tray_icon_parent_class)->map (widget);

  if (self->stale)
    {
      self->stale = FALSE;
      shopno_qs_tray_icon_update (self);
    }
}



/*
 * Input
 */

/* Any menu item, in any menu: opening a submenu is not a choice */
static gboolean
shopno_qs_tray_icon_menu_item_hook (GSignalInvocationHint *hint,
                                    guint                  n_params,
                                    const GValue          *params,
                                    gpointer               data)
{
  GtkMenuItem *item = g_value_get_object (&params[0]);

  if (gtk_menu_item_get_submenu (item) == NULL)
    menu_item_chosen = TRUE;

  return TRUE;
}



/* After the chosen item, if any, was activated */
static void
shopno_qs_tray_icon_menu_done (GtkMenuShell     *menu,
                               ShopnoQsTrayIcon *self)
{
  g_signal_handlers_disconnect_by_func (menu, shopno_qs_tray_icon_menu_done, self);

  /* The menu held the pointer, so the icon saw no leave */
  gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);

  g_signal_emit (self, signals[menu_item_chosen ? ACTIVATED : MENU_DISMISSED], 0);
}



/* The app's own menu, at the pointer as on Windows; apps without one are
 * asked to show theirs */
static void
shopno_qs_tray_icon_show_menu (ShopnoQsTrayIcon *self,
                               GdkEventButton   *event)
{
  static gsize  hooked = 0;
  GtkWidget    *menu = shopno_qs_tray_item_get_menu (self->item);

  if (menu == NULL)
    {
      /* A popup holding the input closes so the app's menu can take it */
      g_signal_emit (self, signals[ACTIVATED], 0);
      shopno_qs_tray_item_context_menu (self->item, (gint) event->x_root, (gint) event->y_root);
      return;
    }

  if (g_once_init_enter (&hooked))
    {
      g_type_class_ref (GTK_TYPE_MENU_ITEM);
      g_signal_add_emission_hook (g_signal_lookup ("activate", GTK_TYPE_MENU_ITEM), 0,
                                  shopno_qs_tray_icon_menu_item_hook, NULL, NULL);
      g_once_init_leave (&hooked, 1);
    }

  /* Our menu grabs the input itself, so a popup holding it stays open
   * until a choice is made */
  menu_item_chosen = FALSE;
  g_signal_handlers_disconnect_by_func (menu, shopno_qs_tray_icon_menu_done, self);
  g_signal_connect_object (menu, "selection-done", G_CALLBACK (shopno_qs_tray_icon_menu_done), self, 0);

  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (self)));
  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
}



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
        {
          shopno_qs_tray_icon_show_menu (self, event);
        }
      else
        {
          /* Before the app acts, so a popup holding the input can close
           * and a window the app opens can take it */
          g_signal_emit (self, signals[ACTIVATED], 0);
          shopno_qs_tray_item_activate (self->item, x, y);
        }
      break;

    case 2:
      g_signal_emit (self, signals[ACTIVATED], 0);
      shopno_qs_tray_item_secondary_activate (self->item, x, y);
      break;

    case 3:
      shopno_qs_tray_icon_show_menu (self, event);
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
    shopno_qs_tray_icon_changed (self);
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

  widget_class->map = shopno_qs_tray_icon_map;
  widget_class->button_press_event = shopno_qs_tray_icon_button_press_event;
  widget_class->button_release_event = shopno_qs_tray_icon_button_release_event;
  widget_class->scroll_event = shopno_qs_tray_icon_scroll_event;
  widget_class->style_updated = shopno_qs_tray_icon_style_updated;

  signals[ACTIVATED] =
    g_signal_new ("activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[MENU_DISMISSED] =
    g_signal_new ("menu-dismissed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_tray_icon_init (ShopnoQsTrayIcon *self)
{
  self->icon_size = 16;

  gtk_button_set_relief (GTK_BUTTON (self), GTK_RELIEF_NONE);
  gtk_widget_set_focus_on_click (GTK_WIDGET (self), FALSE);
  /* Not smooth scrolling: on the panel, each wheel step comes wrapped in a
   * leave and enter, after which GTK reports a smooth step as no movement */
  gtk_widget_add_events (GTK_WIDGET (self), GDK_SCROLL_MASK);
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
  g_signal_connect_swapped (item, "changed", G_CALLBACK (shopno_qs_tray_icon_changed), self);
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

  /* Now even off screen: the size is part of the size request */
  icon->icon_size = size;
  shopno_qs_tray_icon_update (icon);
}
