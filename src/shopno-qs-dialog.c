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

#define BINDING_FLAGS (G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE)

#define BLUR_GUIDE_URL "https://github.com/fa-saikat/xfce4-shopno-quicksettings-plugin#background-blur"

#define TILE_KEY "shopno-qs-tile-id"

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
