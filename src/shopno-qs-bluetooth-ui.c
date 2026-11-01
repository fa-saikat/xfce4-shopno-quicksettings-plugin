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

/*
 * Bluetooth tile and device page, as in the Windows 11 quick settings.
 * Clicking a paired device connects or disconnects it.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-bluetooth.h"
#include "shopno-qs-features.h"
#include "shopno-qs-tile.h"
#include "shopno-qs-util.h"

#define PAGE_NAME "bluetooth"
#define PATH_KEY  "shopno-qs-device"

typedef struct
{
  ShopnoQsWindow    *window;
  ShopnoQsBluetooth *bluetooth;
  GtkWidget         *tile;
  GtkWidget         *toggle;
  GtkWidget         *list;
  GtkWidget         *empty;
} Bluetooth;

static const gchar * const manager_commands[] =
{
  "blueman-manager",
  "blueberry",
  NULL
};



static void
bluetooth_free (gpointer data)
{
  Bluetooth *bluetooth = data;

  g_signal_handlers_disconnect_by_data (bluetooth->bluetooth, bluetooth);
  g_object_unref (bluetooth->bluetooth);
  g_free (bluetooth);
}



static const gchar *
bluetooth_icon_name (const gchar *icon_name)
{
  /* BlueZ names a device class; the icon theme may not have it */
  if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), icon_name))
    return icon_name;

  return "bluetooth-active-symbolic";
}



static void
bluetooth_update_devices (Bluetooth *bluetooth)
{
  GPtrArray *devices = shopno_qs_bluetooth_get_devices (bluetooth->bluetooth);
  gboolean   powered = shopno_qs_bluetooth_get_powered (bluetooth->bluetooth);
  GList     *children;

  children = gtk_container_get_children (GTK_CONTAINER (bluetooth->list));
  g_list_free_full (children, (GDestroyNotify) gtk_widget_destroy);

  for (guint i = 0; powered && i < devices->len; i++)
    {
      ShopnoQsBluetoothDevice *device = g_ptr_array_index (devices, i);
      GtkWidget               *row;
      GtkWidget               *end = NULL;
      gchar                   *subtitle;

      if (device->busy)
        subtitle = g_strdup (device->connected ? _("Disconnecting…") : _("Connecting…"));
      else if (device->failed)
        subtitle = g_strdup (_("Couldn't connect"));
      else if (device->connected && device->battery >= 0)
        subtitle = g_strdup_printf (_("Connected, battery %d%%"), device->battery);
      else if (device->connected)
        subtitle = g_strdup (_("Connected"));
      else
        subtitle = g_strdup (_("Paired"));

      if (device->busy)
        {
          end = gtk_spinner_new ();
          gtk_spinner_start (GTK_SPINNER (end));
        }

      row = gtk_list_box_row_new ();
      gtk_container_add (GTK_CONTAINER (row),
                         shopno_qs_row_new (bluetooth_icon_name (device->icon_name),
                                            device->name, subtitle, end));
      gtk_widget_set_tooltip_text (row, device->connected ? _("Click to disconnect")
                                                          : _("Click to connect"));
      g_object_set_data_full (G_OBJECT (row), PATH_KEY, g_strdup (device->path), g_free);
      if (device->connected)
        gtk_style_context_add_class (gtk_widget_get_style_context (row), "current");

      gtk_widget_show (row);
      gtk_container_add (GTK_CONTAINER (bluetooth->list), row);
      g_free (subtitle);
    }

  gtk_widget_set_visible (bluetooth->list, powered && devices->len > 0);
  gtk_widget_set_visible (bluetooth->empty, !powered || devices->len == 0);
  gtk_label_set_text (GTK_LABEL (bluetooth->empty),
                      !powered ? _("Bluetooth is turned off")
                               : _("No paired devices. Pair new devices in the Bluetooth settings."));
}



static void
bluetooth_update (Bluetooth *bluetooth)
{
  ShopnoQsTile *tile = SHOPNO_QS_TILE (bluetooth->tile);
  gboolean      available = shopno_qs_bluetooth_get_available (bluetooth->bluetooth);
  gboolean      powered = available && shopno_qs_bluetooth_get_powered (bluetooth->bluetooth);
  const gchar  *connected = shopno_qs_bluetooth_get_connected_name (bluetooth->bluetooth);

  shopno_qs_tile_set_icon_name (tile, powered ? "bluetooth-active-symbolic" : "bluetooth-disabled-symbolic");
  shopno_qs_tile_set_label (tile, powered && connected != NULL ? connected : _("Bluetooth"));
  shopno_qs_tile_set_available (tile, available, _("No Bluetooth adapter found"));
  shopno_qs_tile_set_active (tile, powered);

  /* Setting the state does not emit "state-set" */
  gtk_switch_set_state (GTK_SWITCH (bluetooth->toggle), powered);
  gtk_widget_set_sensitive (bluetooth->toggle, available);

  bluetooth_update_devices (bluetooth);
}



static void
bluetooth_toggled (Bluetooth *bluetooth)
{
  shopno_qs_bluetooth_set_powered (bluetooth->bluetooth,
                                   !shopno_qs_bluetooth_get_powered (bluetooth->bluetooth));
}



static gboolean
bluetooth_switch_state_set (GtkSwitch *toggle,
                            gboolean   state,
                            Bluetooth *bluetooth)
{
  /* The switch follows the adapter once BlueZ has changed it */
  shopno_qs_bluetooth_set_powered (bluetooth->bluetooth, state);
  return TRUE;
}



static void
bluetooth_details (Bluetooth *bluetooth)
{
  shopno_qs_window_show_page (bluetooth->window, PAGE_NAME);
}



static void
bluetooth_row_activated (GtkListBox    *list,
                         GtkListBoxRow *row,
                         Bluetooth     *bluetooth)
{
  const gchar *path = g_object_get_data (G_OBJECT (row), PATH_KEY);

  if (path != NULL)
    shopno_qs_bluetooth_toggle_device (bluetooth->bluetooth, path);
}



void
shopno_qs_bluetooth_attach (ShopnoQsWindow *window)
{
  Bluetooth *bluetooth = g_new0 (Bluetooth, 1);
  GtkWidget *page;
  GtkWidget *header;
  GtkWidget *label;

  bluetooth->window = window;
  bluetooth->bluetooth = shopno_qs_bluetooth_get_default ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-bluetooth", bluetooth, bluetooth_free);

  bluetooth->tile = shopno_qs_tile_new ("bluetooth-active-symbolic", _("Bluetooth"), TRUE);
  g_signal_connect_swapped (bluetooth->tile, "toggled", G_CALLBACK (bluetooth_toggled), bluetooth);
  g_signal_connect_swapped (bluetooth->tile, "details", G_CALLBACK (bluetooth_details), bluetooth);
  shopno_qs_window_add_tile (window, SHOPNO_QS_TILE_BLUETOOTH, bluetooth->tile);

  /* Device page: the adapter switch, then the paired devices */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "shopno-qs-page-switch");
  gtk_box_pack_start (GTK_BOX (page), header, FALSE, FALSE, 0);

  label = gtk_label_new (_("Bluetooth"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_pack_start (GTK_BOX (header), label, TRUE, TRUE, 0);

  bluetooth->toggle = gtk_switch_new ();
  gtk_widget_set_valign (bluetooth->toggle, GTK_ALIGN_CENTER);
  g_signal_connect (bluetooth->toggle, "state-set", G_CALLBACK (bluetooth_switch_state_set), bluetooth);
  gtk_box_pack_end (GTK_BOX (header), bluetooth->toggle, FALSE, FALSE, 0);

  bluetooth->list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (bluetooth->list), GTK_SELECTION_NONE);
  gtk_style_context_add_class (gtk_widget_get_style_context (bluetooth->list), "shopno-qs-list");
  g_signal_connect (bluetooth->list, "row-activated", G_CALLBACK (bluetooth_row_activated), bluetooth);
  gtk_box_pack_start (GTK_BOX (page), bluetooth->list, FALSE, FALSE, 0);

  bluetooth->empty = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (bluetooth->empty), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (bluetooth->empty), 1);
  gtk_label_set_justify (GTK_LABEL (bluetooth->empty), GTK_JUSTIFY_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (bluetooth->empty), "shopno-qs-empty");
  gtk_box_pack_start (GTK_BOX (page), bluetooth->empty, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (page),
                      shopno_qs_link_new (GTK_WIDGET (window), _("More Bluetooth settings"), manager_commands),
                      FALSE, FALSE, 0);

  shopno_qs_window_add_page (window, PAGE_NAME, _("Bluetooth"), page);
  gtk_widget_set_no_show_all (bluetooth->list, TRUE);
  gtk_widget_set_no_show_all (bluetooth->empty, TRUE);

  g_signal_connect_swapped (bluetooth->bluetooth, "changed", G_CALLBACK (bluetooth_update), bluetooth);
  bluetooth_update (bluetooth);
}
