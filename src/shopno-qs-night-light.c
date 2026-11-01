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
 * Night light tile. Its state is a plugin setting, so it stays on across
 * restarts of the panel, and is applied again when monitors change.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-features.h"
#include "shopno-qs-gamma.h"
#include "shopno-qs-tile.h"

typedef struct
{
  ShopnoQsSettings *settings;
  GdkScreen        *screen;
  GtkWidget        *tile;
  gboolean          applied;
} NightLight;



static void
night_light_apply (NightLight *night)
{
  GdkDisplay *display = gdk_screen_get_display (night->screen);
  gboolean    on = shopno_qs_settings_get_night_light (night->settings);

  shopno_qs_tile_set_active (SHOPNO_QS_TILE (night->tile), on);

  /* Leave the ramps alone until Night light has been used, so other
   * tools that set them are not reset at startup */
  if (on)
    shopno_qs_gamma_set (display, shopno_qs_settings_get_night_light_temperature (night->settings));
  else if (night->applied)
    shopno_qs_gamma_set (display, SHOPNO_QS_GAMMA_NEUTRAL);

  night->applied = on;
}



static void
night_light_monitors_changed (NightLight *night)
{
  /* New outputs start with neutral ramps */
  if (night->applied)
    shopno_qs_gamma_set (gdk_screen_get_display (night->screen),
                         shopno_qs_settings_get_night_light_temperature (night->settings));
}



static void
night_light_free (gpointer data)
{
  NightLight *night = data;

  /* Do not leave the screen tinted after the plugin is removed */
  if (night->applied)
    shopno_qs_gamma_set (gdk_screen_get_display (night->screen), SHOPNO_QS_GAMMA_NEUTRAL);

  g_signal_handlers_disconnect_by_data (night->settings, night);
  g_signal_handlers_disconnect_by_data (night->screen, night);
  g_object_unref (night->settings);
  g_free (night);
}



static void
night_light_toggled (NightLight *night)
{
  shopno_qs_settings_set_night_light (night->settings,
                                      !shopno_qs_settings_get_night_light (night->settings));
}



void
shopno_qs_night_light_attach (ShopnoQsWindow *window)
{
  NightLight *night = g_new0 (NightLight, 1);

  night->settings = g_object_ref (shopno_qs_window_get_settings (window));
  night->screen = gtk_widget_get_screen (GTK_WIDGET (window));
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-night-light", night, night_light_free);

  night->tile = shopno_qs_tile_new ("night-light-symbolic", _("Night light"), FALSE);
  g_signal_connect_swapped (night->tile, "toggled", G_CALLBACK (night_light_toggled), night);
  shopno_qs_window_add_tile (window, night->tile);

  if (!shopno_qs_gamma_supported (gdk_screen_get_display (night->screen)))
    {
      shopno_qs_tile_set_available (SHOPNO_QS_TILE (night->tile), FALSE,
                                    _("Night light needs an X11 session with XRandR"));
      return;
    }

  g_signal_connect_swapped (night->settings, "notify::night-light",
                            G_CALLBACK (night_light_apply), night);
  g_signal_connect_swapped (night->settings, "notify::night-light-temperature",
                            G_CALLBACK (night_light_apply), night);
  g_signal_connect_swapped (night->screen, "monitors-changed",
                            G_CALLBACK (night_light_monitors_changed), night);
  night_light_apply (night);
}
