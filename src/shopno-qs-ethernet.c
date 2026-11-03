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
 * Ethernet tile, shown while a cable is plugged in, as GNOME's Wired
 * tile is; Windows 11 has none. Clicking it disconnects the cable's
 * connection, or connects it again.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-features.h"
#include "shopno-qs-network.h"
#include "shopno-qs-tile.h"

typedef struct
{
  ShopnoQsWindow  *window;
  ShopnoQsNetwork *network;
  GtkWidget       *tile;
} Ethernet;



static void
ethernet_free (gpointer data)
{
  Ethernet *ethernet = data;

  g_signal_handlers_disconnect_by_data (ethernet->network, ethernet);
  g_object_unref (ethernet->network);
  g_free (ethernet);
}



static void
ethernet_update (Ethernet *ethernet)
{
  ShopnoQsTile       *tile = SHOPNO_QS_TILE (ethernet->tile);
  ShopnoQsWiredState  state = shopno_qs_network_get_wired_state (ethernet->network);
  const gchar        *icon_name;

  switch (state)
    {
    case SHOPNO_QS_WIRED_CONNECTED:
      icon_name = "network-wired-symbolic";
      break;

    case SHOPNO_QS_WIRED_CONNECTING:
      icon_name = "network-wired-acquiring-symbolic";
      break;

    case SHOPNO_QS_WIRED_NONE:
    case SHOPNO_QS_WIRED_UNPLUGGED:
    case SHOPNO_QS_WIRED_DISCONNECTED:
    default:
      icon_name = "network-wired-disconnected-symbolic";
      break;
    }

  shopno_qs_tile_set_icon_name (tile, icon_name);
  shopno_qs_tile_set_active (tile, state >= SHOPNO_QS_WIRED_CONNECTING);

  /* A port without a cable has nothing to switch */
  shopno_qs_window_set_tile_present (ethernet->window, ethernet->tile, state >= SHOPNO_QS_WIRED_DISCONNECTED);
}



static void
ethernet_toggled (Ethernet *ethernet)
{
  ShopnoQsWiredState state = shopno_qs_network_get_wired_state (ethernet->network);

  shopno_qs_network_set_wired_connected (ethernet->network, state < SHOPNO_QS_WIRED_CONNECTING);
}



void
shopno_qs_ethernet_attach (ShopnoQsWindow *window)
{
  Ethernet *ethernet = g_new0 (Ethernet, 1);

  ethernet->window = window;
  ethernet->network = shopno_qs_network_get_default ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-ethernet", ethernet, ethernet_free);

  ethernet->tile = shopno_qs_tile_new ("network-wired-symbolic", _("Ethernet"), FALSE);
  g_signal_connect_swapped (ethernet->tile, "toggled", G_CALLBACK (ethernet_toggled), ethernet);
  shopno_qs_window_add_tile (window, SHOPNO_QS_TILE_ETHERNET, ethernet->tile);

  g_signal_connect_swapped (ethernet->network, "changed", G_CALLBACK (ethernet_update), ethernet);
  ethernet_update (ethernet);
}
