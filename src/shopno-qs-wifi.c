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
 * Wi-Fi tile and network page, as in the Windows 11 quick settings.
 *
 * Clicking a network expands it in place with its actions: Connect,
 * a password field for new secured networks, or Disconnect. Rows are
 * kept and updated by network name, so a scan does not collapse the row
 * or clear a password being typed.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-features.h"
#include "shopno-qs-network.h"
#include "shopno-qs-tile.h"
#include "shopno-qs-util.h"

#define PAGE_NAME "wifi"
#define ROW_KEY   "shopno-qs-wifi-row"

typedef struct _Wifi Wifi;

typedef struct
{
  Wifi                 *wifi;
  gchar                *ssid;
  ShopnoQsWifiSecurity  security;
  gboolean              known;
  gboolean              active;
  guint                 position;

  GtkWidget            *row;
  GtkWidget            *icon;
  GtkWidget            *lock;
  GtkWidget            *subtitle;
  GtkWidget            *revealer;
  GtkWidget            *message;
  GtkWidget            *entry;
  GtkWidget            *connect;
  GtkWidget            *disconnect;
  GtkWidget            *editor;
} WifiRow;

struct _Wifi
{
  ShopnoQsWindow  *window;
  ShopnoQsNetwork *network;
  GtkWidget       *tile;
  GtkWidget       *toggle;
  GtkWidget       *list;
  GtkWidget       *off_label;
  GHashTable      *rows;

  /* The last failed connection, until another one is tried */
  gchar           *failed_ssid;
  gboolean         failed_password;
};

static const gchar * const editor_commands[] =
{
  "nm-connection-editor",
  NULL
};



static void
wifi_free (gpointer data)
{
  Wifi *wifi = data;

  g_signal_handlers_disconnect_by_data (wifi->network, wifi);
  g_object_unref (wifi->network);
  g_hash_table_destroy (wifi->rows);
  g_free (wifi->failed_ssid);
  g_free (wifi);
}



/*
 * Network rows
 */

static void
wifi_row_free (gpointer data)
{
  WifiRow *row = data;

  g_free (row->ssid);
  g_free (row);
}



static gboolean
wifi_row_needs_password (WifiRow *row)
{
  if (row->security != SHOPNO_QS_WIFI_PSK)
    return FALSE;

  if (!row->known)
    return TRUE;

  return g_strcmp0 (row->ssid, row->wifi->failed_ssid) == 0 && row->wifi->failed_password;
}



static void
wifi_row_update (WifiRow *row)
{
  Wifi        *wifi = row->wifi;
  const gchar *connecting = shopno_qs_network_get_connecting_ssid (wifi->network);
  gboolean     is_connecting = g_strcmp0 (connecting, row->ssid) == 0;
  gboolean     failed = g_strcmp0 (wifi->failed_ssid, row->ssid) == 0;
  gboolean     needs_password = wifi_row_needs_password (row);
  gboolean     enterprise = row->security == SHOPNO_QS_WIFI_ENTERPRISE && !row->known;
  const gchar *subtitle;
  const gchar *message = NULL;

  if (is_connecting)
    subtitle = _("Connecting…");
  else if (row->active)
    subtitle = row->security == SHOPNO_QS_WIFI_OPEN ? _("Connected") : _("Connected, secured");
  else if (row->security == SHOPNO_QS_WIFI_OPEN)
    subtitle = _("Open");
  else
    subtitle = _("Secured");

  if (failed)
    message = wifi->failed_password ? _("The password is incorrect. Try again.")
                                    : _("Can't connect to this network.");
  else if (enterprise)
    message = _("This network needs more details. Set it up in the network settings.");

  gtk_label_set_text (GTK_LABEL (row->subtitle), subtitle);
  gtk_widget_set_visible (row->lock, row->security != SHOPNO_QS_WIFI_OPEN);

  gtk_label_set_text (GTK_LABEL (row->message), message);
  gtk_widget_set_visible (row->message, message != NULL);

  gtk_widget_set_visible (row->disconnect, row->active);
  gtk_widget_set_visible (row->editor, !row->active && enterprise);
  gtk_widget_set_visible (row->connect, !row->active && !enterprise);
  gtk_widget_set_visible (row->entry, !row->active && needs_password);
  gtk_widget_set_sensitive (row->connect,
                            !is_connecting
                            && (!needs_password || *gtk_entry_get_text (GTK_ENTRY (row->entry)) != '\0'));

  if (row->active || is_connecting)
    gtk_style_context_add_class (gtk_widget_get_style_context (row->row), "current");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (row->row), "current");
}



static void
wifi_row_connect (WifiRow *row)
{
  Wifi        *wifi = row->wifi;
  const gchar *password = NULL;

  if (wifi_row_needs_password (row))
    {
      password = gtk_entry_get_text (GTK_ENTRY (row->entry));
      if (*password == '\0')
        return;
    }

  g_clear_pointer (&wifi->failed_ssid, g_free);
  shopno_qs_network_connect (wifi->network, row->ssid, password);
  gtk_entry_set_text (GTK_ENTRY (row->entry), "");
  wifi_row_update (row);
}



static void
wifi_row_disconnect (WifiRow *row)
{
  shopno_qs_network_disconnect (row->wifi->network);
  gtk_revealer_set_reveal_child (GTK_REVEALER (row->revealer), FALSE);
}



static void
wifi_row_open_editor (WifiRow *row)
{
  shopno_qs_launch (GTK_WIDGET (row->wifi->window), editor_commands);
}



static void
wifi_row_reveal_password (GtkEntry             *entry,
                          GtkEntryIconPosition  position,
                          GdkEvent             *event,
                          gpointer              data)
{
  gboolean visible = !gtk_entry_get_visibility (entry);

  gtk_entry_set_visibility (entry, visible);
  gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY,
                                     visible ? "view-conceal-symbolic" : "view-reveal-symbolic");
  gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY,
                                   visible ? _("Hide password") : _("Show password"));
}



static WifiRow *
wifi_row_new (Wifi                      *wifi,
              const ShopnoQsWifiNetwork *network)
{
  WifiRow   *row = g_new0 (WifiRow, 1);
  GtkWidget *box;
  GtkWidget *header;
  GtkWidget *text;
  GtkWidget *label;
  GtkWidget *actions;
  GtkWidget *buttons;

  row->wifi = wifi;
  row->ssid = g_strdup (network->ssid);

  row->row = gtk_list_box_row_new ();
  g_object_set_data_full (G_OBJECT (row->row), ROW_KEY, row, wifi_row_free);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (row->row), box);

  /* Signal icon with a lock, name, and state */
  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (box), header, FALSE, FALSE, 0);

  row->icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (header), row->icon, FALSE, FALSE, 0);

  text = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_valign (text, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (header), text, TRUE, TRUE, 0);

  label = gtk_label_new (network->ssid);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (text), label, FALSE, FALSE, 0);

  row->subtitle = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (row->subtitle), 0.0);
  gtk_style_context_add_class (gtk_widget_get_style_context (row->subtitle), "shopno-qs-row-subtitle");
  gtk_box_pack_start (GTK_BOX (text), row->subtitle, FALSE, FALSE, 0);

  row->lock = gtk_image_new_from_icon_name ("changes-prevent-symbolic", GTK_ICON_SIZE_MENU);
  gtk_style_context_add_class (gtk_widget_get_style_context (row->lock), "shopno-qs-dim");
  gtk_box_pack_end (GTK_BOX (header), row->lock, FALSE, FALSE, 0);

  /* Actions, revealed when the row is clicked */
  row->revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_duration (GTK_REVEALER (row->revealer), 120);
  gtk_box_pack_start (GTK_BOX (box), row->revealer, FALSE, FALSE, 0);

  actions = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_style_context_add_class (gtk_widget_get_style_context (actions), "shopno-qs-row-actions");
  gtk_container_add (GTK_CONTAINER (row->revealer), actions);

  row->message = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (row->message), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (row->message), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (row->message), 1);
  gtk_style_context_add_class (gtk_widget_get_style_context (row->message), "shopno-qs-row-message");
  gtk_box_pack_start (GTK_BOX (actions), row->message, FALSE, FALSE, 0);

  row->entry = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (row->entry), FALSE);
  gtk_entry_set_input_purpose (GTK_ENTRY (row->entry), GTK_INPUT_PURPOSE_PASSWORD);
  gtk_entry_set_placeholder_text (GTK_ENTRY (row->entry), _("Enter the network password"));
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (row->entry), GTK_ENTRY_ICON_SECONDARY,
                                     "view-reveal-symbolic");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (row->entry), GTK_ENTRY_ICON_SECONDARY,
                                   _("Show password"));
  g_signal_connect (row->entry, "icon-press", G_CALLBACK (wifi_row_reveal_password), NULL);
  g_signal_connect_swapped (row->entry, "changed", G_CALLBACK (wifi_row_update), row);
  g_signal_connect_swapped (row->entry, "activate", G_CALLBACK (wifi_row_connect), row);
  gtk_box_pack_start (GTK_BOX (actions), row->entry, FALSE, FALSE, 0);

  buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign (buttons, GTK_ALIGN_END);
  gtk_box_pack_start (GTK_BOX (actions), buttons, FALSE, FALSE, 0);

  row->connect = gtk_button_new_with_label (_("Connect"));
  gtk_style_context_add_class (gtk_widget_get_style_context (row->connect), "suggested-action");
  g_signal_connect_swapped (row->connect, "clicked", G_CALLBACK (wifi_row_connect), row);
  gtk_box_pack_end (GTK_BOX (buttons), row->connect, FALSE, FALSE, 0);

  row->disconnect = gtk_button_new_with_label (_("Disconnect"));
  g_signal_connect_swapped (row->disconnect, "clicked", G_CALLBACK (wifi_row_disconnect), row);
  gtk_box_pack_end (GTK_BOX (buttons), row->disconnect, FALSE, FALSE, 0);

  row->editor = gtk_button_new_with_label (_("Open network settings"));
  g_signal_connect_swapped (row->editor, "clicked", G_CALLBACK (wifi_row_open_editor), row);
  gtk_box_pack_end (GTK_BOX (buttons), row->editor, FALSE, FALSE, 0);

  gtk_widget_show_all (row->row);

  /* Shown and hidden by wifi_row_update() only */
  gtk_widget_set_no_show_all (row->message, TRUE);
  gtk_widget_set_no_show_all (row->entry, TRUE);
  gtk_widget_set_no_show_all (row->connect, TRUE);
  gtk_widget_set_no_show_all (row->disconnect, TRUE);
  gtk_widget_set_no_show_all (row->editor, TRUE);
  gtk_widget_set_no_show_all (row->lock, TRUE);

  return row;
}



static void
wifi_row_activated (GtkListBox    *list,
                    GtkListBoxRow *list_row,
                    Wifi          *wifi)
{
  WifiRow  *row = g_object_get_data (G_OBJECT (list_row), ROW_KEY);
  gboolean  reveal;
  GList    *children;

  if (row == NULL)
    return;

  reveal = !gtk_revealer_get_reveal_child (GTK_REVEALER (row->revealer));

  /* One open row at a time */
  children = gtk_container_get_children (GTK_CONTAINER (list));
  for (GList *l = children; l != NULL; l = l->next)
    {
      WifiRow *other = g_object_get_data (G_OBJECT (l->data), ROW_KEY);

      if (other != NULL && other != row)
        gtk_revealer_set_reveal_child (GTK_REVEALER (other->revealer), FALSE);
    }
  g_list_free (children);

  gtk_revealer_set_reveal_child (GTK_REVEALER (row->revealer), reveal);

  if (reveal && gtk_widget_get_visible (row->entry))
    gtk_widget_grab_focus (row->entry);
}



static gint
wifi_sort (GtkListBoxRow *a,
           GtkListBoxRow *b,
           gpointer       data)
{
  WifiRow *ra = g_object_get_data (G_OBJECT (a), ROW_KEY);
  WifiRow *rb = g_object_get_data (G_OBJECT (b), ROW_KEY);

  return (gint) ra->position - (gint) rb->position;
}



static void
wifi_update_networks (Wifi *wifi)
{
  GPtrArray      *networks = shopno_qs_network_get_wifi_networks (wifi->network);
  GHashTable     *seen = g_hash_table_new (g_str_hash, g_str_equal);
  GHashTableIter  iter;
  gpointer        key, value;
  gboolean        reorder = FALSE;

  for (guint i = 0; i < networks->len; i++)
    {
      ShopnoQsWifiNetwork *network = g_ptr_array_index (networks, i);
      WifiRow             *row = g_hash_table_lookup (wifi->rows, network->ssid);

      if (row == NULL)
        {
          row = wifi_row_new (wifi, network);
          g_hash_table_insert (wifi->rows, row->ssid, row);
          gtk_container_add (GTK_CONTAINER (wifi->list), row->row);
          reorder = TRUE;
        }

      /* Only reorder when a network comes, goes or connects, not for
       * every change in signal strength, so rows stay under the pointer */
      if (row->active != network->active)
        reorder = TRUE;

      row->security = network->security;
      row->known = network->known;
      row->active = network->active;
      row->position = i;

      gtk_image_set_from_icon_name (GTK_IMAGE (row->icon),
                                    shopno_qs_network_strength_icon (network->strength),
                                    GTK_ICON_SIZE_BUTTON);
      wifi_row_update (row);
      g_hash_table_add (seen, row->ssid);
    }

  /* Remove networks that are out of range */
  g_hash_table_iter_init (&iter, wifi->rows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      WifiRow *row = value;

      if (!g_hash_table_contains (seen, key))
        {
          g_hash_table_iter_remove (&iter);
          gtk_widget_destroy (row->row);
        }
    }

  g_hash_table_destroy (seen);

  if (reorder)
    gtk_list_box_invalidate_sort (GTK_LIST_BOX (wifi->list));
}



/*
 * Tile and page
 */

static void
wifi_update (Wifi *wifi)
{
  ShopnoQsTile *tile = SHOPNO_QS_TILE (wifi->tile);
  gboolean      available = shopno_qs_network_get_wifi_available (wifi->network);
  gboolean      enabled = available && shopno_qs_network_get_wifi_enabled (wifi->network);
  const gchar  *active = shopno_qs_network_get_active_ssid (wifi->network);
  const gchar  *connecting = shopno_qs_network_get_connecting_ssid (wifi->network);
  const gchar  *icon_name;

  if (!enabled)
    icon_name = "network-wireless-disabled-symbolic";
  else if (connecting != NULL)
    icon_name = "network-wireless-acquiring-symbolic";
  else if (active != NULL)
    icon_name = shopno_qs_network_get_icon_name (wifi->network);
  else
    icon_name = "network-wireless-symbolic";

  shopno_qs_tile_set_icon_name (tile, icon_name);
  shopno_qs_tile_set_label (tile, connecting != NULL ? connecting : active != NULL ? active : _("Wi-Fi"));
  shopno_qs_tile_set_available (tile, available, _("No Wi-Fi adapter found"));
  shopno_qs_tile_set_active (tile, enabled);

  /* Setting the state does not emit "state-set" */
  gtk_switch_set_state (GTK_SWITCH (wifi->toggle), enabled);
  gtk_widget_set_sensitive (wifi->toggle, available);

  gtk_widget_set_visible (wifi->list, enabled);
  gtk_widget_set_visible (wifi->off_label, !enabled);

  /* The connecting state is not part of the network list */
  wifi_update_networks (wifi);
}



static void
wifi_toggled (Wifi *wifi)
{
  shopno_qs_network_set_wifi_enabled (wifi->network,
                                      !shopno_qs_network_get_wifi_enabled (wifi->network));
}



static gboolean
wifi_switch_state_set (GtkSwitch *toggle,
                       gboolean   state,
                       Wifi      *wifi)
{
  /* The switch follows the radio once NetworkManager has changed it */
  shopno_qs_network_set_wifi_enabled (wifi->network, state);
  return TRUE;
}



static void
wifi_details (Wifi *wifi)
{
  shopno_qs_network_request_scan (wifi->network);
  shopno_qs_window_show_page (wifi->window, PAGE_NAME);
}



static void
wifi_connect_failed (ShopnoQsNetwork *network,
                     const gchar     *ssid,
                     gboolean         needs_password,
                     Wifi            *wifi)
{
  WifiRow *row;

  g_free (wifi->failed_ssid);
  wifi->failed_ssid = g_strdup (ssid);
  wifi->failed_password = needs_password;

  row = ssid != NULL ? g_hash_table_lookup (wifi->rows, ssid) : NULL;
  if (row != NULL)
    {
      wifi_row_update (row);
      gtk_revealer_set_reveal_child (GTK_REVEALER (row->revealer), TRUE);
      if (gtk_widget_get_visible (row->entry))
        gtk_widget_grab_focus (row->entry);
    }
}



void
shopno_qs_wifi_attach (ShopnoQsWindow *window)
{
  Wifi      *wifi = g_new0 (Wifi, 1);
  GtkWidget *page;
  GtkWidget *header;
  GtkWidget *label;

  wifi->window = window;
  wifi->network = shopno_qs_network_get_default ();
  wifi->rows = g_hash_table_new (g_str_hash, g_str_equal);
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-wifi", wifi, wifi_free);

  wifi->tile = shopno_qs_tile_new ("network-wireless-symbolic", _("Wi-Fi"), TRUE);
  g_signal_connect_swapped (wifi->tile, "toggled", G_CALLBACK (wifi_toggled), wifi);
  g_signal_connect_swapped (wifi->tile, "details", G_CALLBACK (wifi_details), wifi);
  shopno_qs_window_add_tile (window, SHOPNO_QS_TILE_WIFI, wifi->tile);

  /* Network page: the radio switch, then the networks in range */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "shopno-qs-page-switch");
  gtk_box_pack_start (GTK_BOX (page), header, FALSE, FALSE, 0);

  label = gtk_label_new (_("Wi-Fi"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_pack_start (GTK_BOX (header), label, TRUE, TRUE, 0);

  wifi->toggle = gtk_switch_new ();
  gtk_widget_set_valign (wifi->toggle, GTK_ALIGN_CENTER);
  g_signal_connect (wifi->toggle, "state-set", G_CALLBACK (wifi_switch_state_set), wifi);
  gtk_box_pack_end (GTK_BOX (header), wifi->toggle, FALSE, FALSE, 0);

  wifi->list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (wifi->list), GTK_SELECTION_NONE);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (wifi->list), wifi_sort, NULL, NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (wifi->list), "shopno-qs-list");
  g_signal_connect (wifi->list, "row-activated", G_CALLBACK (wifi_row_activated), wifi);
  gtk_box_pack_start (GTK_BOX (page), wifi->list, FALSE, FALSE, 0);

  wifi->off_label = gtk_label_new (_("Wi-Fi is turned off"));
  gtk_style_context_add_class (gtk_widget_get_style_context (wifi->off_label), "shopno-qs-empty");
  gtk_box_pack_start (GTK_BOX (page), wifi->off_label, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (page),
                      shopno_qs_link_new (GTK_WIDGET (window), _("More Wi-Fi settings"), editor_commands),
                      FALSE, FALSE, 0);

  shopno_qs_window_add_page (window, PAGE_NAME, _("Wi-Fi"), page);
  gtk_widget_set_no_show_all (wifi->list, TRUE);
  gtk_widget_set_no_show_all (wifi->off_label, TRUE);

  g_signal_connect_swapped (wifi->network, "changed", G_CALLBACK (wifi_update), wifi);
  g_signal_connect_swapped (wifi->network, "networks-changed", G_CALLBACK (wifi_update_networks), wifi);
  g_signal_connect (wifi->network, "connect-failed", G_CALLBACK (wifi_connect_failed), wifi);
  wifi_update (wifi);
}
