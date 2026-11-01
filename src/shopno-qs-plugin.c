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

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "shopno-qs-settings.h"
#include "shopno-qs-window.h"

/* Space between the flyout and the panel or the edges of the screen */
#define POPUP_GAP 12

typedef struct _ShopnoQsPluginClass ShopnoQsPluginClass;
typedef struct _ShopnoQsPlugin      ShopnoQsPlugin;

struct _ShopnoQsPluginClass
{
  XfcePanelPluginClass __parent__;
};

struct _ShopnoQsPlugin
{
  XfcePanelPlugin   __parent__;

  ShopnoQsSettings *settings;
  XfconfChannel    *channel;
  guint             create_window_id;

  GtkWidget        *button;
  GtkWidget        *box;
  GtkWidget        *window;
};

/* Resident: the module registers GResources, static GTypes, and a
 * screen-wide CSS provider, none of which survive being unloaded */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (ShopnoQsPlugin, shopno_qs_plugin)



/*
 * Settings storage
 */

static void
shopno_qs_plugin_bind (ShopnoQsPlugin *self,
                       const gchar    *name,
                       GType           type)
{
  gchar *path = g_strconcat (xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (self)),
                             "/", name, NULL);

  xfconf_g_property_bind (self->channel, path, type, self->settings, name);
  g_free (path);
}



static void
shopno_qs_plugin_load_settings (ShopnoQsPlugin *self)
{
  GError *error = NULL;

  if (!xfconf_init (&error))
    {
      g_warning ("Failed to connect to xfconf, settings will not be saved: %s", error->message);
      g_error_free (error);
      return;
    }

  self->channel = xfconf_channel_get ("xfce4-panel");

  shopno_qs_plugin_bind (self, "enable-blur", G_TYPE_BOOLEAN);
  shopno_qs_plugin_bind (self, "night-light", G_TYPE_BOOLEAN);
  shopno_qs_plugin_bind (self, "night-light-temperature", G_TYPE_UINT);
}



/*
 * Popup
 */

static void
shopno_qs_plugin_window_hidden (ShopnoQsPlugin *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), FALSE);
}



static void
shopno_qs_plugin_create_window (ShopnoQsPlugin *self)
{
  if (self->create_window_id != 0)
    {
      g_source_remove (self->create_window_id);
      self->create_window_id = 0;
    }

  if (self->window != NULL)
    return;

  self->window = shopno_qs_window_new (self->settings);
  g_signal_connect_swapped (self->window, "hide",
                            G_CALLBACK (shopno_qs_plugin_window_hidden), self);
}



static gboolean
shopno_qs_plugin_create_window_idle (gpointer data)
{
  ShopnoQsPlugin *self = data;

  self->create_window_id = 0;
  shopno_qs_plugin_create_window (self);

  return G_SOURCE_REMOVE;
}



static void
shopno_qs_plugin_add_gap (ShopnoQsPlugin *self,
                          gint           *x,
                          gint           *y)
{
  GdkDisplay     *display = gtk_widget_get_display (self->button);
  GdkMonitor     *monitor;
  GdkRectangle    geometry;
  GtkRequisition  size;
  gboolean        horizontal = TRUE;

  /* Move the flyout away from the panel, on the side it opens towards */
  switch (xfce_panel_plugin_arrow_type (XFCE_PANEL_PLUGIN (self)))
    {
    case GTK_ARROW_UP:
      *y -= POPUP_GAP;
      break;

    case GTK_ARROW_DOWN:
      *y += POPUP_GAP;
      break;

    case GTK_ARROW_LEFT:
      *x -= POPUP_GAP;
      horizontal = FALSE;
      break;

    case GTK_ARROW_RIGHT:
      *x += POPUP_GAP;
      horizontal = FALSE;
      break;

    default:
      break;
    }

  /* and keep it clear of the screen edges along the panel */
  monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (self->button));
  if (monitor == NULL)
    return;

  gdk_monitor_get_geometry (monitor, &geometry);
  gtk_widget_get_preferred_size (self->window, NULL, &size);

  if (horizontal)
    *x = MAX (geometry.x + POPUP_GAP, MIN (*x, geometry.x + geometry.width - size.width - POPUP_GAP));
  else
    *y = MAX (geometry.y + POPUP_GAP, MIN (*y, geometry.y + geometry.height - size.height - POPUP_GAP));
}



static void
shopno_qs_plugin_popup (ShopnoQsPlugin *self)
{
  gint x, y;

  shopno_qs_plugin_create_window (self);

  gtk_window_set_screen (GTK_WINDOW (self->window), gtk_widget_get_screen (self->button));

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);

  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (self), self->window, self->button, &x, &y);
  shopno_qs_plugin_add_gap (self, &x, &y);
  shopno_qs_window_popup (SHOPNO_QS_WINDOW (self->window), x, y);
}



static void
shopno_qs_plugin_toggle (ShopnoQsPlugin *self)
{
  if (self->window != NULL && gtk_widget_get_visible (self->window))
    gtk_widget_hide (self->window);
  else
    shopno_qs_plugin_popup (self);
}



static gboolean
shopno_qs_plugin_button_press_event (GtkWidget      *button,
                                     GdkEventButton *event,
                                     ShopnoQsPlugin *self)
{
  /* Leave Ctrl+click and other buttons to the panel */
  if (event->button != 1
      || event->type != GDK_BUTTON_PRESS
      || (event->state & GDK_CONTROL_MASK) != 0)
    return GDK_EVENT_PROPAGATE;

  shopno_qs_plugin_toggle (self);

  return GDK_EVENT_STOP;
}



/*
 * XfcePanelPlugin
 */

static void
shopno_qs_plugin_construct (XfcePanelPlugin *plugin)
{
  ShopnoQsPlugin *self = (ShopnoQsPlugin *) plugin;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  xfce_panel_plugin_menu_show_about (plugin);

  shopno_qs_plugin_load_settings (self);

  /* Create the flyout before the first click: Night light must be
   * applied at login, and the panel icons follow the same services */
  self->create_window_id = g_idle_add (shopno_qs_plugin_create_window_idle, self);
}



static void
shopno_qs_plugin_free_data (XfcePanelPlugin *plugin)
{
  ShopnoQsPlugin *self = (ShopnoQsPlugin *) plugin;

  if (self->create_window_id != 0)
    g_source_remove (self->create_window_id);

  if (self->window != NULL)
    gtk_widget_destroy (self->window);

  if (self->channel != NULL)
    {
      xfconf_g_property_unbind_all (self->settings);
      xfconf_shutdown ();
    }
  g_object_unref (self->settings);
}



static gboolean
shopno_qs_plugin_size_changed (XfcePanelPlugin *plugin,
                               gint             size)
{
  ShopnoQsPlugin *self = (ShopnoQsPlugin *) plugin;
  GtkOrientation  orientation = xfce_panel_plugin_get_orientation (plugin);

  size /= xfce_panel_plugin_get_nrows (plugin);

  /* The button grows along the panel to fit its icons */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
  else
    gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->box), orientation);

  return TRUE;
}



static void
shopno_qs_plugin_about (XfcePanelPlugin *plugin)
{
  const gchar *authors[] = { "FA Saikat <fahimabrar.saikat@gmail.com>", NULL };

  gtk_show_about_dialog (NULL,
                         "program-name", _("Shopno Quick Settings"),
                         "version", PACKAGE_VERSION,
                         "comments", _("Windows 11-style quick settings for the Xfce panel"),
                         "website", "https://github.com/fa-saikat/xfce4-shopno-quicksettings-plugin",
                         "license-type", GTK_LICENSE_GPL_2_0,
                         "authors", authors,
                         "logo-icon-name", "shopno-quicksettings",
                         NULL);
}



static void
shopno_qs_plugin_class_init (ShopnoQsPluginClass *klass)
{
  XfcePanelPluginClass *plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);

  plugin_class->construct = shopno_qs_plugin_construct;
  plugin_class->free_data = shopno_qs_plugin_free_data;
  plugin_class->size_changed = shopno_qs_plugin_size_changed;
  plugin_class->about = shopno_qs_plugin_about;
}



static void
shopno_qs_plugin_init (ShopnoQsPlugin *self)
{
  self->settings = shopno_qs_settings_new ();

  self->button = xfce_panel_create_toggle_button ();
  gtk_widget_set_tooltip_text (self->button, _("Quick settings"));
  g_signal_connect (self->button, "button-press-event",
                    G_CALLBACK (shopno_qs_plugin_button_press_event), self);
  gtk_container_add (GTK_CONTAINER (self), self->button);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (self), self->button);

  /* Holds the status icons, like the Windows 11 taskbar */
  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (self->button), self->box);
  gtk_box_pack_start (GTK_BOX (self->box),
                      gtk_image_new_from_icon_name ("shopno-quicksettings", GTK_ICON_SIZE_BUTTON),
                      FALSE, FALSE, 0);

  gtk_widget_show_all (self->button);
}
