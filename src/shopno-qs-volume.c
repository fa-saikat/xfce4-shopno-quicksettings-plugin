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
 * Volume slider with mute on its icon, and a page to pick the output
 * device, as in the Windows 11 quick settings.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-audio.h"
#include "shopno-qs-features.h"
#include "shopno-qs-slider.h"
#include "shopno-qs-util.h"

#define PAGE_NAME "volume"
#define SINK_KEY  "shopno-qs-sink"

typedef struct
{
  ShopnoQsWindow *window;
  ShopnoQsAudio  *audio;
  GtkWidget      *slider;
  GtkWidget      *list;
  GtkWidget      *empty;
} Volume;

static const gchar * const mixer_commands[] =
{
  "pavucontrol",
  "pavucontrol-qt",
  "xfce4-mixer",
  NULL
};



static void
volume_free (gpointer data)
{
  Volume *volume = data;

  g_signal_handlers_disconnect_by_data (volume->audio, volume);
  g_object_unref (volume->audio);
  g_free (volume);
}



static void
volume_update (Volume *volume)
{
  ShopnoQsSlider *slider = SHOPNO_QS_SLIDER (volume->slider);
  gdouble         level = shopno_qs_audio_get_volume (volume->audio);
  gchar          *tooltip;

  shopno_qs_slider_set_icon_name (slider, shopno_qs_audio_get_icon_name (volume->audio));
  gtk_widget_set_sensitive (volume->slider, shopno_qs_audio_get_available (volume->audio));

  if (!shopno_qs_slider_is_dragging (slider))
    shopno_qs_slider_set_value (slider, level);

  if (!shopno_qs_audio_get_available (volume->audio))
    tooltip = g_strdup (_("No sound output"));
  else if (shopno_qs_audio_get_muted (volume->audio))
    tooltip = g_strdup (_("Muted"));
  else
    tooltip = g_strdup_printf (_("Volume %d%%"), (gint) (level * 100.0 + 0.5));

  gtk_widget_set_tooltip_text (volume->slider, tooltip);
  g_free (tooltip);
}



static void
volume_row_activated (GtkListBox    *list,
                      GtkListBoxRow *row,
                      Volume        *volume)
{
  const gchar *name = g_object_get_data (G_OBJECT (row), SINK_KEY);

  if (name != NULL)
    shopno_qs_audio_set_default_sink (volume->audio, name);
}



static void
volume_update_sinks (Volume *volume)
{
  GPtrArray *sinks = shopno_qs_audio_get_sinks (volume->audio);
  GList     *children;

  children = gtk_container_get_children (GTK_CONTAINER (volume->list));
  g_list_free_full (children, (GDestroyNotify) gtk_widget_destroy);

  for (guint i = 0; i < sinks->len; i++)
    {
      ShopnoQsAudioSink *sink = g_ptr_array_index (sinks, i);
      GtkWidget         *row;
      GtkWidget         *check = NULL;

      if (sink->is_default)
        check = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);

      row = gtk_list_box_row_new ();
      gtk_container_add (GTK_CONTAINER (row),
                         shopno_qs_row_new (sink->icon_name, sink->description, NULL, check));
      g_object_set_data_full (G_OBJECT (row), SINK_KEY, g_strdup (sink->name), g_free);
      if (sink->is_default)
        gtk_style_context_add_class (gtk_widget_get_style_context (row), "current");

      gtk_widget_show (row);
      gtk_container_add (GTK_CONTAINER (volume->list), row);
    }

  gtk_widget_set_visible (volume->empty, sinks->len == 0);
}



static void
volume_value_changed (ShopnoQsSlider *slider,
                      gdouble         value,
                      Volume         *volume)
{
  shopno_qs_audio_set_volume (volume->audio, value);
}



static void
volume_icon_clicked (Volume *volume)
{
  shopno_qs_audio_set_muted (volume->audio, !shopno_qs_audio_get_muted (volume->audio));
}



static void
volume_details (Volume *volume)
{
  shopno_qs_window_show_page (volume->window, PAGE_NAME);
}



void
shopno_qs_volume_attach (ShopnoQsWindow *window)
{
  Volume    *volume = g_new0 (Volume, 1);
  GtkWidget *page;

  volume->window = window;
  volume->audio = shopno_qs_audio_get_default ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-volume", volume, volume_free);

  volume->slider = shopno_qs_slider_new ("audio-volume-high-symbolic", _("Mute"), TRUE);
  g_signal_connect (volume->slider, "value-changed", G_CALLBACK (volume_value_changed), volume);
  g_signal_connect_swapped (volume->slider, "icon-clicked", G_CALLBACK (volume_icon_clicked), volume);
  g_signal_connect_swapped (volume->slider, "details", G_CALLBACK (volume_details), volume);
  shopno_qs_window_add_slider (window, volume->slider);

  /* Output device page */
  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  volume->list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (volume->list), GTK_SELECTION_NONE);
  gtk_style_context_add_class (gtk_widget_get_style_context (volume->list), "shopno-qs-list");
  g_signal_connect (volume->list, "row-activated", G_CALLBACK (volume_row_activated), volume);
  gtk_box_pack_start (GTK_BOX (page), volume->list, FALSE, FALSE, 0);

  volume->empty = gtk_label_new (_("No sound output devices found"));
  gtk_style_context_add_class (gtk_widget_get_style_context (volume->empty), "shopno-qs-empty");
  gtk_box_pack_start (GTK_BOX (page), volume->empty, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (page),
                      shopno_qs_link_new (GTK_WIDGET (window), _("More volume settings"), mixer_commands),
                      FALSE, FALSE, 0);

  shopno_qs_window_add_page (window, PAGE_NAME, _("Sound output"), page);
  gtk_widget_set_no_show_all (volume->empty, TRUE);

  g_signal_connect_swapped (volume->audio, "changed", G_CALLBACK (volume_update), volume);
  g_signal_connect_swapped (volume->audio, "sinks-changed", G_CALLBACK (volume_update_sinks), volume);
  volume_update (volume);
  volume_update_sinks (volume);
}
