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
 * A quick action tile: a button with an icon and a label under it, as in
 * the Windows 11 quick settings. Split tiles have a second part with a
 * chevron that opens a detail page.
 *
 * The tile does not change its own state when clicked. It emits "toggled"
 * and the feature module sets the state once the system has changed, so
 * the tile never shows something that did not happen.
 */

#include "config.h"

#include "shopno-qs-tile.h"

enum
{
  TOGGLED,
  DETAILS,
  N_SIGNALS
};

struct _ShopnoQsTile
{
  GtkBox     __parent__;

  GtkWidget *buttons;
  GtkWidget *main_button;
  GtkWidget *icon;
  GtkWidget *details_button;
  GtkWidget *label;

  gboolean   active;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsTile, shopno_qs_tile, GTK_TYPE_BOX)



static void
shopno_qs_tile_main_clicked (ShopnoQsTile *self)
{
  g_signal_emit (self, signals[TOGGLED], 0);
}



static void
shopno_qs_tile_details_clicked (ShopnoQsTile *self)
{
  g_signal_emit (self, signals[DETAILS], 0);
}



static void
shopno_qs_tile_class_init (ShopnoQsTileClass *klass)
{
  signals[TOGGLED] =
    g_signal_new ("toggled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[DETAILS] =
    g_signal_new ("details", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_tile_init (ShopnoQsTile *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 10);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-tile");

  self->buttons = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->buttons), "shopno-qs-tile-buttons");
  gtk_box_pack_start (GTK_BOX (self), self->buttons, FALSE, FALSE, 0);

  self->main_button = gtk_button_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (self->main_button), "shopno-qs-tile-main");
  g_signal_connect_swapped (self->main_button, "clicked",
                            G_CALLBACK (shopno_qs_tile_main_clicked), self);
  gtk_box_pack_start (GTK_BOX (self->buttons), self->main_button, TRUE, TRUE, 0);

  self->icon = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (self->icon), 16);
  gtk_container_add (GTK_CONTAINER (self->main_button), self->icon);

  self->details_button = gtk_button_new_from_icon_name ("pan-end-symbolic", GTK_ICON_SIZE_MENU);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->details_button), "shopno-qs-tile-details");
  gtk_widget_set_no_show_all (self->details_button, TRUE);
  g_signal_connect_swapped (self->details_button, "clicked",
                            G_CALLBACK (shopno_qs_tile_details_clicked), self);
  gtk_box_pack_start (GTK_BOX (self->buttons), self->details_button, FALSE, FALSE, 0);

  /* Up to two lines, so a long network name stays readable */
  self->label = gtk_label_new (NULL);
  gtk_label_set_line_wrap (GTK_LABEL (self->label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (self->label), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_lines (GTK_LABEL (self->label), 2);
  gtk_label_set_ellipsize (GTK_LABEL (self->label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (self->label), 1);
  gtk_label_set_justify (GTK_LABEL (self->label), GTK_JUSTIFY_CENTER);
  gtk_widget_set_valign (self->label, GTK_ALIGN_START);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "shopno-qs-tile-label");
  gtk_box_pack_start (GTK_BOX (self), self->label, FALSE, FALSE, 0);
}



GtkWidget *
shopno_qs_tile_new (const gchar *icon_name,
                    const gchar *label,
                    gboolean     has_details)
{
  ShopnoQsTile *tile = g_object_new (SHOPNO_TYPE_QS_TILE, NULL);

  shopno_qs_tile_set_icon_name (tile, icon_name);
  shopno_qs_tile_set_label (tile, label);

  if (has_details)
    {
      gtk_widget_show (tile->details_button);
      gtk_style_context_add_class (gtk_widget_get_style_context (tile->buttons), "split");
      gtk_widget_set_tooltip_text (tile->details_button, label);
    }

  gtk_widget_show_all (GTK_WIDGET (tile));

  return GTK_WIDGET (tile);
}



void
shopno_qs_tile_set_icon_name (ShopnoQsTile *tile,
                              const gchar  *icon_name)
{
  g_return_if_fail (SHOPNO_IS_QS_TILE (tile));

  gtk_image_set_from_icon_name (GTK_IMAGE (tile->icon), icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (tile->icon), 16);
}



void
shopno_qs_tile_set_label (ShopnoQsTile *tile,
                          const gchar  *label)
{
  g_return_if_fail (SHOPNO_IS_QS_TILE (tile));

  gtk_label_set_text (GTK_LABEL (tile->label), label);
  gtk_widget_set_tooltip_text (tile->main_button, label);
}



void
shopno_qs_tile_set_active (ShopnoQsTile *tile,
                           gboolean      active)
{
  g_return_if_fail (SHOPNO_IS_QS_TILE (tile));

  tile->active = active;

  if (active)
    gtk_style_context_add_class (gtk_widget_get_style_context (tile->buttons), "active");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (tile->buttons), "active");
}



gboolean
shopno_qs_tile_get_active (ShopnoQsTile *tile)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TILE (tile), FALSE);

  return tile->active;
}



void
shopno_qs_tile_set_available (ShopnoQsTile *tile,
                              gboolean      available,
                              const gchar  *reason)
{
  g_return_if_fail (SHOPNO_IS_QS_TILE (tile));

  gtk_widget_set_sensitive (tile->buttons, available);
  gtk_widget_set_sensitive (tile->label, available);
  gtk_widget_set_tooltip_text (GTK_WIDGET (tile), available ? NULL : reason);

  if (!available)
    shopno_qs_tile_set_active (tile, FALSE);
}
