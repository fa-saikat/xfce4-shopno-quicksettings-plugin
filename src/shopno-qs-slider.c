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
 * A slider row as in the Windows 11 quick settings: an icon button on the
 * left (mute for volume), the slider, and an optional chevron on the
 * right that opens a detail page.
 *
 * Signals: "value-changed" when the user moves the slider, "icon-clicked"
 * and "details".
 */

#include "config.h"

#include "shopno-qs-slider.h"

enum
{
  VALUE_CHANGED,
  ICON_CLICKED,
  DETAILS,
  N_SIGNALS
};

struct _ShopnoQsSlider
{
  GtkBox     __parent__;

  GtkWidget *icon_button;
  GtkWidget *icon;
  GtkWidget *scale;
  GtkWidget *details_button;

  gulong     value_changed_id;
  gboolean   dragging;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsSlider, shopno_qs_slider, GTK_TYPE_BOX)



static void
shopno_qs_slider_scale_changed (ShopnoQsSlider *self)
{
  g_signal_emit (self, signals[VALUE_CHANGED], 0, gtk_range_get_value (GTK_RANGE (self->scale)));
}



static gboolean
shopno_qs_slider_scale_pressed (ShopnoQsSlider *self)
{
  self->dragging = TRUE;
  return GDK_EVENT_PROPAGATE;
}



static gboolean
shopno_qs_slider_scale_released (ShopnoQsSlider *self)
{
  self->dragging = FALSE;
  return GDK_EVENT_PROPAGATE;
}



static void
shopno_qs_slider_icon_clicked (ShopnoQsSlider *self)
{
  g_signal_emit (self, signals[ICON_CLICKED], 0);
}



static void
shopno_qs_slider_details_clicked (ShopnoQsSlider *self)
{
  g_signal_emit (self, signals[DETAILS], 0);
}



static void
shopno_qs_slider_class_init (ShopnoQsSliderClass *klass)
{
  signals[VALUE_CHANGED] =
    g_signal_new ("value-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  signals[ICON_CLICKED] =
    g_signal_new ("icon-clicked", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[DETAILS] =
    g_signal_new ("details", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_slider_init (ShopnoQsSlider *self)
{
  gtk_box_set_spacing (GTK_BOX (self), 4);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-slider");

  self->icon_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (self->icon_button), GTK_RELIEF_NONE);
  gtk_widget_set_valign (self->icon_button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->icon_button), "shopno-qs-slider-icon");
  g_signal_connect_swapped (self->icon_button, "clicked",
                            G_CALLBACK (shopno_qs_slider_icon_clicked), self);
  gtk_box_pack_start (GTK_BOX (self), self->icon_button, FALSE, FALSE, 0);

  self->icon = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (self->icon_button), self->icon);

  self->scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
  gtk_scale_set_draw_value (GTK_SCALE (self->scale), FALSE);
  gtk_range_set_increments (GTK_RANGE (self->scale), 0.02, 0.1);
  gtk_widget_set_valign (self->scale, GTK_ALIGN_CENTER);
  self->value_changed_id =
    g_signal_connect_swapped (self->scale, "value-changed",
                              G_CALLBACK (shopno_qs_slider_scale_changed), self);
  g_signal_connect_swapped (self->scale, "button-press-event",
                            G_CALLBACK (shopno_qs_slider_scale_pressed), self);
  g_signal_connect_swapped (self->scale, "button-release-event",
                            G_CALLBACK (shopno_qs_slider_scale_released), self);
  gtk_box_pack_start (GTK_BOX (self), self->scale, TRUE, TRUE, 0);

  self->details_button = gtk_button_new_from_icon_name ("pan-end-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (self->details_button), GTK_RELIEF_NONE);
  gtk_widget_set_valign (self->details_button, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (self->details_button, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->details_button), "shopno-qs-slider-details");
  g_signal_connect_swapped (self->details_button, "clicked",
                            G_CALLBACK (shopno_qs_slider_details_clicked), self);
  gtk_box_pack_end (GTK_BOX (self), self->details_button, FALSE, FALSE, 0);
}



GtkWidget *
shopno_qs_slider_new (const gchar *icon_name,
                      const gchar *icon_tooltip,
                      gboolean     has_details)
{
  ShopnoQsSlider *slider = g_object_new (SHOPNO_TYPE_QS_SLIDER, NULL);

  shopno_qs_slider_set_icon_name (slider, icon_name);
  gtk_widget_set_tooltip_text (slider->icon_button, icon_tooltip);

  /* Without a tooltip the icon has no action, like the brightness icon */
  if (icon_tooltip == NULL)
    {
      gtk_widget_set_can_focus (slider->icon_button, FALSE);
      gtk_style_context_add_class (gtk_widget_get_style_context (slider->icon_button), "static");
    }

  /* Without a chevron, keep the slider the same length as one with it */
  gtk_widget_set_visible (slider->details_button, TRUE);
  gtk_widget_set_opacity (slider->details_button, has_details ? 1.0 : 0.0);
  gtk_widget_set_sensitive (slider->details_button, has_details);

  gtk_widget_show_all (GTK_WIDGET (slider));

  return GTK_WIDGET (slider);
}



void
shopno_qs_slider_set_icon_name (ShopnoQsSlider *slider,
                                const gchar    *icon_name)
{
  g_return_if_fail (SHOPNO_IS_QS_SLIDER (slider));

  gtk_image_set_from_icon_name (GTK_IMAGE (slider->icon), icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (slider->icon), 16);
}



void
shopno_qs_slider_set_value (ShopnoQsSlider *slider,
                            gdouble         value)
{
  g_return_if_fail (SHOPNO_IS_QS_SLIDER (slider));

  g_signal_handler_block (slider->scale, slider->value_changed_id);
  gtk_range_set_value (GTK_RANGE (slider->scale), value);
  g_signal_handler_unblock (slider->scale, slider->value_changed_id);
}



gdouble
shopno_qs_slider_get_value (ShopnoQsSlider *slider)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SLIDER (slider), 0.0);

  return gtk_range_get_value (GTK_RANGE (slider->scale));
}



gboolean
shopno_qs_slider_is_dragging (ShopnoQsSlider *slider)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SLIDER (slider), FALSE);

  return slider->dragging;
}
