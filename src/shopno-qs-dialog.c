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

#include <glib/gi18n-lib.h>
#include <libxfce4ui/libxfce4ui.h>

#include "shopno-qs-dialog.h"
#include "shopno-qs-features.h"
#include "shopno-qs-tray.h"

#define BINDING_FLAGS (G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE)

#define BLUR_GUIDE_URL "https://github.com/fa-saikat/xfce4-shopno-quicksettings-plugin#background-blur"

#define TILE_KEY "shopno-qs-tile-id"
#define TRAY_KEY "shopno-qs-tray-id"

static const struct
{
  const gchar *id;
  const gchar *label;
}
tiles[] =
{
  { SHOPNO_QS_TILE_WIFI, N_("_Wi-Fi") },
  { SHOPNO_QS_TILE_BLUETOOTH, N_("_Bluetooth") },
  { SHOPNO_QS_TILE_FOCUS, N_("_Focus assist") },
  { SHOPNO_QS_TILE_NIGHT_LIGHT, N_("_Night light") },
};



static void
shopno_qs_dialog_tile_toggled (GtkToggleButton  *check,
                               ShopnoQsSettings *settings)
{
  shopno_qs_settings_set_tile_hidden (settings, g_object_get_data (G_OBJECT (check), TILE_KEY),
                                      !gtk_toggle_button_get_active (check));
}



static void
shopno_qs_dialog_tray_toggled (GtkToggleButton  *check,
                               ShopnoQsSettings *settings)
{
  shopno_qs_settings_set_tray_pinned (settings, g_object_get_data (G_OBJECT (check), TRAY_KEY),
                                      gtk_toggle_button_get_active (check));
}



/* One "Show on the panel" box per tray app: the running ones, and the
 * pinned ones that are not running, so a pin can still be undone */
static void
shopno_qs_dialog_fill_tray_apps (GtkWidget *list)
{
  ShopnoQsSettings *settings = g_object_get_data (G_OBJECT (list), "settings");
  ShopnoQsTray     *tray = g_object_get_data (G_OBJECT (list), "tray");
  GPtrArray        *items = shopno_qs_tray_get_items (tray);
  GHashTable       *titles = g_hash_table_new (g_str_hash, g_str_equal);
  gchar           **pinned;
  GList            *ids;
  GList            *children;

  children = gtk_container_get_children (GTK_CONTAINER (list));
  g_list_free_full (children, (GDestroyNotify) gtk_widget_destroy);

  for (guint i = 0; i < items->len; i++)
    {
      ShopnoQsTrayItem *item = g_ptr_array_index (items, i);
      const gchar      *id = shopno_qs_tray_item_get_id (item);

      if (id != NULL)
        g_hash_table_insert (titles, (gpointer) id, (gpointer) shopno_qs_tray_item_get_title (item));
    }

  g_object_get (settings, "tray-pinned", &pinned, NULL);
  for (guint i = 0; pinned[i] != NULL; i++)
    if (!g_hash_table_contains (titles, pinned[i]))
      g_hash_table_insert (titles, pinned[i], pinned[i]);

  ids = g_list_sort (g_hash_table_get_keys (titles), (GCompareFunc) g_utf8_collate);
  for (GList *l = ids; l != NULL; l = l->next)
    {
      const gchar *title = g_hash_table_lookup (titles, l->data);
      GtkWidget   *check = gtk_check_button_new_with_label (title != NULL ? title : l->data);

      gtk_widget_set_tooltip_text (check, l->data);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                    shopno_qs_settings_is_tray_pinned (settings, l->data));
      g_object_set_data_full (G_OBJECT (check), TRAY_KEY, g_strdup (l->data), g_free);
      g_signal_connect (check, "toggled", G_CALLBACK (shopno_qs_dialog_tray_toggled), settings);
      gtk_box_pack_start (GTK_BOX (list), check, FALSE, FALSE, 0);
    }

  if (ids == NULL)
    {
      GtkWidget *label = gtk_label_new (_("No apps are showing tray icons right now."));

      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
      gtk_box_pack_start (GTK_BOX (list), label, FALSE, FALSE, 0);
    }

  gtk_widget_show_all (list);

  g_list_free (ids);
  g_strfreev (pinned);
  g_hash_table_destroy (titles);
}



static gboolean
shopno_qs_dialog_refill_idle (gpointer data)
{
  g_object_set_data (data, "refill-id", NULL);

  /* Not if the dialog closed meanwhile */
  if (gtk_widget_get_parent (data) != NULL)
    shopno_qs_dialog_fill_tray_apps (data);

  return G_SOURCE_REMOVE;
}



/* Apps come, go, and send their names a little after appearing */
static void
shopno_qs_dialog_tray_changed (GtkWidget *list)
{
  guint id;

  if (g_object_get_data (G_OBJECT (list), "refill-id") != NULL)
    return;

  id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, shopno_qs_dialog_refill_idle,
                        g_object_ref (list), g_object_unref);
  g_object_set_data (G_OBJECT (list), "refill-id", GUINT_TO_POINTER (id));
}



static void
shopno_qs_dialog_tray_item_added (GtkWidget        *list,
                                  ShopnoQsTrayItem *item)
{
  g_signal_connect_object (item, "changed", G_CALLBACK (shopno_qs_dialog_tray_changed),
                           list, G_CONNECT_SWAPPED);
  shopno_qs_dialog_tray_changed (list);
}



static GtkWidget *
shopno_qs_dialog_tray_apps_new (ShopnoQsSettings *settings)
{
  GtkWidget    *list = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  ShopnoQsTray *tray = shopno_qs_tray_get_default ();
  GPtrArray    *items = shopno_qs_tray_get_items (tray);

  gtk_widget_set_margin_start (list, 12);
  g_object_set_data (G_OBJECT (list), "settings", settings);
  g_object_set_data_full (G_OBJECT (list), "tray", tray, g_object_unref);

  g_signal_connect_object (tray, "item-added", G_CALLBACK (shopno_qs_dialog_tray_item_added),
                           list, G_CONNECT_SWAPPED);
  g_signal_connect_object (tray, "item-removed", G_CALLBACK (shopno_qs_dialog_tray_changed),
                           list, G_CONNECT_SWAPPED);
  for (guint i = 0; i < items->len; i++)
    g_signal_connect_object (g_ptr_array_index (items, i), "changed",
                             G_CALLBACK (shopno_qs_dialog_tray_changed), list, G_CONNECT_SWAPPED);

  shopno_qs_dialog_fill_tray_apps (list);

  return list;
}



static GtkWidget *
shopno_qs_dialog_heading_new (const gchar *text)
{
  GtkWidget *label = gtk_label_new (NULL);
  gchar     *markup = g_markup_printf_escaped ("<b>%s</b>", text);

  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  g_free (markup);

  return label;
}



static gchar *
shopno_qs_dialog_format_temperature (GtkScale *scale,
                                     gdouble   value)
{
  return g_strdup_printf ("%d K", (gint) value);
}



GtkWidget *
shopno_qs_dialog_new (GtkWindow        *parent,
                      ShopnoQsSettings *settings)
{
  GtkWidget     *dialog;
  GtkWidget     *grid;
  GtkWidget     *label;
  GtkWidget     *check;
  GtkWidget     *scale;
  GtkAdjustment *adjustment;
  gchar         *markup;
  gint           row = 0;

  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), NULL);

  dialog = xfce_titled_dialog_new_with_mixed_buttons (_("Shopno Quick Settings"), parent,
                                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                                      "window-close-symbolic", _("_Close"), GTK_RESPONSE_CLOSE,
                                                      NULL);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "preferences-desktop");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid, TRUE, TRUE, 0);

  /* Tiles */
  gtk_grid_attach (GTK_GRID (grid), shopno_qs_dialog_heading_new (_("Tiles")), 0, row++, 2, 1);

  for (guint i = 0; i < G_N_ELEMENTS (tiles); i++)
    {
      check = gtk_check_button_new_with_mnemonic (_(tiles[i].label));
      gtk_widget_set_margin_start (check, 12);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
                                    !shopno_qs_settings_is_tile_hidden (settings, tiles[i].id));
      g_object_set_data (G_OBJECT (check), TILE_KEY, (gpointer) tiles[i].id);
      g_signal_connect (check, "toggled", G_CALLBACK (shopno_qs_dialog_tile_toggled), settings);
      gtk_grid_attach (GTK_GRID (grid), check, 0, row++, 2, 1);
    }

  /* Night light */
  label = shopno_qs_dialog_heading_new (_("Night light"));
  gtk_widget_set_margin_top (label, 12);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);

  label = gtk_label_new_with_mnemonic (_("Color _temperature:"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_margin_start (label, 12);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);

  adjustment = gtk_adjustment_new (SHOPNO_QS_NIGHT_LIGHT_DEFAULT,
                                   SHOPNO_QS_NIGHT_LIGHT_MIN, SHOPNO_QS_NIGHT_LIGHT_MAX,
                                   100, 500, 0);
  scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_RIGHT);
  gtk_scale_add_mark (GTK_SCALE (scale), SHOPNO_QS_NIGHT_LIGHT_MIN, GTK_POS_BOTTOM, _("Warmer"));
  gtk_scale_add_mark (GTK_SCALE (scale), SHOPNO_QS_NIGHT_LIGHT_MAX, GTK_POS_BOTTOM, _("Cooler"));
  gtk_widget_set_hexpand (scale, TRUE);
  gtk_widget_set_size_request (scale, 220, -1);
  g_signal_connect (scale, "format-value", G_CALLBACK (shopno_qs_dialog_format_temperature), NULL);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), scale);
  g_object_bind_property (settings, "night-light-temperature", adjustment, "value", BINDING_FLAGS);
  gtk_grid_attach (GTK_GRID (grid), scale, 1, row++, 1, 1);

  /* Tray icons: the others wait behind ^, as on Windows 11 */
  label = shopno_qs_dialog_heading_new (_("Tray icons shown on the panel"));
  gtk_widget_set_margin_top (label, 12);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);
  gtk_grid_attach (GTK_GRID (grid), shopno_qs_dialog_tray_apps_new (settings), 0, row++, 2, 1);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label),
                        _("<small>Other icons are behind the ^ button next to them.</small>"));
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_margin_start (label, 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);

  /* Appearance */
  label = shopno_qs_dialog_heading_new (_("Appearance"));
  gtk_widget_set_margin_top (label, 12);
  gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);

  check = gtk_check_button_new_with_mnemonic (_("Bl_ur the background behind the flyout"));
  gtk_widget_set_margin_start (check, 12);
  g_object_bind_property (settings, "enable-blur", check, "active", BINDING_FLAGS);
  gtk_grid_attach (GTK_GRID (grid), check, 0, row++, 2, 1);

  markup = g_strdup_printf ("<small>%s <a href=\"%s\">%s</a></small>",
                            _("Blur needs the picom compositor with the Shopno Quick Settings rules."),
                            BLUR_GUIDE_URL, _("How to set it up"));
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
  gtk_widget_set_margin_start (label, 36);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
  gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);
  g_free (markup);

  gtk_widget_show_all (grid);

  return dialog;
}
