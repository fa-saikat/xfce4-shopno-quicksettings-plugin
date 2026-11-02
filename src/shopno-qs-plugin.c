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

#include "shopno-qs-dialog.h"
#include "shopno-qs-settings.h"
#include "shopno-qs-status.h"
#include "shopno-qs-overflow.h"
#include "shopno-qs-tray-box.h"
#include "shopno-qs-window.h"

/* Tile ids contain no ';', so the list is stored as one string. Xfconf
 * cannot store empty arrays, and an empty list must persist. */
#define LIST_SEPARATOR ";"

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
  GtkWidget        *status;
  GtkWidget        *box;
  GtkWidget        *tray;
  GtkWidget        *overflow_button;
  GtkWidget        *overflow_arrow;
  GtkWidget        *overflow;
  GtkWidget        *window;
};

/* Resident: the module registers GResources, static GTypes, and a
 * screen-wide CSS provider, none of which survive being unloaded */
XFCE_PANEL_DEFINE_PLUGIN_RESIDENT (ShopnoQsPlugin, shopno_qs_plugin)



/*
 * Settings storage
 */

static gchar *
shopno_qs_plugin_property_path (ShopnoQsPlugin *self,
                                const gchar    *name)
{
  return g_strconcat (xfce_panel_plugin_get_property_base (XFCE_PANEL_PLUGIN (self)), "/", name, NULL);
}



static void
shopno_qs_plugin_bind (ShopnoQsPlugin *self,
                       const gchar    *name,
                       GType           type)
{
  gchar *path = shopno_qs_plugin_property_path (self, name);

  xfconf_g_property_bind (self->channel, path, type, self->settings, name);
  g_free (path);
}



static void
shopno_qs_plugin_load_list (ShopnoQsPlugin *self,
                            const gchar    *name)
{
  gchar  *path = shopno_qs_plugin_property_path (self, name);
  gchar  *value;
  gchar **list;

  if (xfconf_channel_has_property (self->channel, path))
    {
      value = xfconf_channel_get_string (self->channel, path, "");
      list = *value != '\0' ? g_strsplit (value, LIST_SEPARATOR, -1) : g_new0 (gchar *, 1);
      g_object_set (self->settings, name, list, NULL);
      g_strfreev (list);
      g_free (value);
    }

  g_free (path);
}



static void
shopno_qs_plugin_save_list (ShopnoQsSettings *settings,
                            GParamSpec       *pspec,
                            ShopnoQsPlugin   *self)
{
  const gchar *name = g_param_spec_get_name (pspec);
  gchar       *path = shopno_qs_plugin_property_path (self, name);
  gchar      **list;
  gchar       *value;

  g_object_get (settings, name, &list, NULL);
  value = g_strjoinv (LIST_SEPARATOR, list);
  xfconf_channel_set_string (self->channel, path, value);

  g_free (value);
  g_strfreev (list);
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

  shopno_qs_plugin_load_list (self, "hidden-tiles");
  g_signal_connect (self->settings, "notify::hidden-tiles",
                    G_CALLBACK (shopno_qs_plugin_save_list), self);
  shopno_qs_plugin_load_list (self, "tray-pinned");
  g_signal_connect (self->settings, "notify::tray-pinned",
                    G_CALLBACK (shopno_qs_plugin_save_list), self);
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
  gint           x, y;
  GtkRequisition size;
  gboolean       grow_up;

  shopno_qs_plugin_create_window (self);

  gtk_window_set_screen (GTK_WINDOW (self->window), gtk_widget_get_screen (self->button));

  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), TRUE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), TRUE);

  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (self), self->window, self->button, &x, &y);
  shopno_qs_plugin_add_gap (self, &x, &y);

  /* Above a bottom panel, keep the bottom edge next to the panel and let
   * taller pages grow upward */
  grow_up = xfce_panel_plugin_arrow_type (XFCE_PANEL_PLUGIN (self)) == GTK_ARROW_UP;
  if (grow_up)
    {
      gtk_widget_get_preferred_size (self->window, NULL, &size);
      y += size.height;
    }

  shopno_qs_window_popup (SHOPNO_QS_WINDOW (self->window), x, y, grow_up);
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



static gboolean
shopno_qs_plugin_scroll_event (GtkWidget      *button,
                               GdkEventScroll *event,
                               ShopnoQsPlugin *self)
{
  return shopno_qs_status_scroll (SHOPNO_QS_STATUS (self->status), event);
}



static gboolean
shopno_qs_plugin_query_tooltip (GtkWidget      *button,
                                gint            x,
                                gint            y,
                                gboolean        keyboard,
                                GtkTooltip     *tooltip,
                                ShopnoQsPlugin *self)
{
  gchar *text;

  /* No tooltip over the open flyout */
  if (self->window != NULL && gtk_widget_get_visible (self->window))
    return FALSE;

  text = shopno_qs_status_get_tooltip (SHOPNO_QS_STATUS (self->status));
  gtk_tooltip_set_text (tooltip, text);
  g_free (text);

  return TRUE;
}



/*
 * Tray overflow
 */

/* The ^ points away from the panel edge, towards where the flyout opens */
static void
shopno_qs_plugin_update_arrow (ShopnoQsPlugin *self)
{
  const gchar *icon_name;

  switch (xfce_panel_plugin_arrow_type (XFCE_PANEL_PLUGIN (self)))
    {
    case GTK_ARROW_DOWN:
      icon_name = "pan-down-symbolic";
      break;

    case GTK_ARROW_LEFT:
      icon_name = "pan-start-symbolic";
      break;

    case GTK_ARROW_RIGHT:
      icon_name = "pan-end-symbolic";
      break;

    default:
      icon_name = "pan-up-symbolic";
      break;
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->overflow_arrow), icon_name, GTK_ICON_SIZE_MENU);
}



static void
shopno_qs_plugin_overflow_changed (ShopnoQsPlugin *self)
{
  gtk_widget_set_visible (self->overflow_button,
                          shopno_qs_overflow_get_n_icons (SHOPNO_QS_OVERFLOW (self->overflow)) > 0);
}



static void
shopno_qs_plugin_overflow_hidden (ShopnoQsPlugin *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->overflow_button), FALSE);
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), FALSE);
}



static void
shopno_qs_plugin_overflow_toggled (ShopnoQsPlugin *self)
{
  gint x, y;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->overflow_button)))
    {
      gtk_widget_hide (self->overflow);
      return;
    }

  if (gtk_widget_get_visible (self->overflow))
    return;

  gtk_window_set_screen (GTK_WINDOW (self->overflow), gtk_widget_get_screen (self->overflow_button));
  xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (self), TRUE);
  xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (self), self->overflow, self->overflow_button, &x, &y);

  switch (xfce_panel_plugin_arrow_type (XFCE_PANEL_PLUGIN (self)))
    {
    case GTK_ARROW_UP:
      y -= POPUP_GAP;
      break;

    case GTK_ARROW_DOWN:
      y += POPUP_GAP;
      break;

    case GTK_ARROW_LEFT:
      x -= POPUP_GAP;
      break;

    case GTK_ARROW_RIGHT:
      x += POPUP_GAP;
      break;

    default:
      break;
    }

  shopno_qs_overflow_popup (SHOPNO_QS_OVERFLOW (self->overflow), x, y);
}



/*
 * XfcePanelPlugin
 */

static void
shopno_qs_plugin_construct (XfcePanelPlugin *plugin)
{
  ShopnoQsPlugin *self = (ShopnoQsPlugin *) plugin;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  xfce_panel_plugin_menu_show_configure (plugin);
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

  g_signal_handlers_disconnect_by_data (self->overflow, self);
  gtk_widget_destroy (self->overflow);

  g_signal_handlers_disconnect_by_data (self->settings, self);
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

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->status), orientation);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->box), orientation);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->tray), orientation);
  shopno_qs_plugin_update_arrow (self);
  shopno_qs_tray_box_set_icon_size (SHOPNO_QS_TRAY_BOX (self->tray), xfce_panel_plugin_get_icon_size (plugin));
  shopno_qs_status_set_icon_size (SHOPNO_QS_STATUS (self->status), xfce_panel_plugin_get_icon_size (plugin));

  return TRUE;
}



static void
shopno_qs_plugin_configure_plugin (XfcePanelPlugin *plugin)
{
  GtkWidget *dialog;

  dialog = shopno_qs_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                 ((ShopnoQsPlugin *) plugin)->settings);

  xfce_panel_plugin_block_menu (plugin);
  g_signal_connect_swapped (dialog, "destroy", G_CALLBACK (xfce_panel_plugin_unblock_menu), plugin);
  gtk_widget_show (dialog);
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
  plugin_class->configure_plugin = shopno_qs_plugin_configure_plugin;
  plugin_class->about = shopno_qs_plugin_about;
}



static void
shopno_qs_plugin_init (ShopnoQsPlugin *self)
{
  self->settings = shopno_qs_settings_new ();

  self->button = xfce_panel_create_toggle_button ();
  gtk_widget_set_has_tooltip (self->button, TRUE);
  g_signal_connect (self->button, "query-tooltip",
                    G_CALLBACK (shopno_qs_plugin_query_tooltip), self);
  gtk_widget_add_events (self->button, GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  g_signal_connect (self->button, "scroll-event",
                    G_CALLBACK (shopno_qs_plugin_scroll_event), self);
  g_signal_connect (self->button, "button-press-event",
                    G_CALLBACK (shopno_qs_plugin_button_press_event), self);
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (self), self->button);

  /* Tray icons of other apps, then the quick settings button, like the
   * Windows 11 taskbar */
  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_container_add (GTK_CONTAINER (self), self->box);

  /* ^ opens the tray icons that are not pinned to the panel */
  self->overflow_button = xfce_panel_create_toggle_button ();
  gtk_widget_set_tooltip_text (self->overflow_button, _("Show hidden icons"));
  gtk_style_context_add_class (gtk_widget_get_style_context (self->overflow_button), "shopno-qs-overflow-button");
  g_signal_connect_swapped (self->overflow_button, "toggled",
                            G_CALLBACK (shopno_qs_plugin_overflow_toggled), self);
  self->overflow_arrow = gtk_image_new_from_icon_name ("pan-up-symbolic", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (self->overflow_button), self->overflow_arrow);
  gtk_box_pack_start (GTK_BOX (self->box), self->overflow_button, FALSE, FALSE, 0);

  self->tray = shopno_qs_tray_box_new (self->settings, TRUE);
  gtk_box_pack_start (GTK_BOX (self->box), self->tray, FALSE, FALSE, 0);

  self->overflow = shopno_qs_overflow_new (self->settings);
  g_signal_connect_swapped (self->overflow, "changed",
                            G_CALLBACK (shopno_qs_plugin_overflow_changed), self);
  g_signal_connect_swapped (self->overflow, "hide",
                            G_CALLBACK (shopno_qs_plugin_overflow_hidden), self);
  gtk_box_pack_start (GTK_BOX (self->box), self->button, FALSE, FALSE, 0);

  /* Network, volume and battery, like the Windows 11 taskbar */
  self->status = shopno_qs_status_new ();
  gtk_container_add (GTK_CONTAINER (self->button), self->status);

  gtk_widget_show_all (self->box);
  gtk_widget_set_no_show_all (self->overflow_button, TRUE);
  shopno_qs_plugin_overflow_changed (self);
}
