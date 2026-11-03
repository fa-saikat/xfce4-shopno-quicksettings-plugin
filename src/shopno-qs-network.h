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

#ifndef __SHOPNO_QS_NETWORK_H__
#define __SHOPNO_QS_NETWORK_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_NETWORK (shopno_qs_network_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsNetwork, shopno_qs_network, SHOPNO, QS_NETWORK, GObject)

typedef enum
{
  SHOPNO_QS_WIFI_OPEN,
  SHOPNO_QS_WIFI_PSK,         /* WEP, WPA/WPA2 personal or WPA3 SAE: a password */
  SHOPNO_QS_WIFI_ENTERPRISE,  /* 802.1X: needs the full connection editor */
} ShopnoQsWifiSecurity;

typedef struct
{
  gchar                *ssid;
  guint8                strength;
  ShopnoQsWifiSecurity  security;
  gboolean              active;
  gboolean              known;
} ShopnoQsWifiNetwork;

/* All wired ports together: the furthest along of them */
typedef enum
{
  SHOPNO_QS_WIRED_NONE,          /* No Ethernet port, or none managed */
  SHOPNO_QS_WIRED_UNPLUGGED,     /* No cable */
  SHOPNO_QS_WIRED_DISCONNECTED,
  SHOPNO_QS_WIRED_CONNECTING,
  SHOPNO_QS_WIRED_CONNECTED,
} ShopnoQsWiredState;

/* Shared by the flyout and the panel button. Returns a new reference. */
ShopnoQsNetwork *shopno_qs_network_get_default       (void);

/* NetworkManager is running and there is a Wi-Fi device */
gboolean         shopno_qs_network_get_wifi_available (ShopnoQsNetwork *network);
gboolean         shopno_qs_network_get_wifi_enabled   (ShopnoQsNetwork *network);
void             shopno_qs_network_set_wifi_enabled   (ShopnoQsNetwork *network,
                                                       gboolean         enabled);

/* The network Wi-Fi is connected to, or NULL */
const gchar     *shopno_qs_network_get_active_ssid    (ShopnoQsNetwork *network);

/* The network being connected to right now, or NULL */
const gchar     *shopno_qs_network_get_connecting_ssid (ShopnoQsNetwork *network);

/* Visible networks as ShopnoQsWifiNetwork, one per name, the connected
 * one first and then by signal strength. Owned by the backend. */
GPtrArray       *shopno_qs_network_get_wifi_networks  (ShopnoQsNetwork *network);

void             shopno_qs_network_request_scan       (ShopnoQsNetwork *network);

/* Reads the state again, in case a change was missed */
void             shopno_qs_network_refresh            (ShopnoQsNetwork *network);

/* Connects to a visible network. The password is only used for secured
 * networks; NULL activates a saved connection as it is. */
void             shopno_qs_network_connect            (ShopnoQsNetwork *network,
                                                       const gchar     *ssid,
                                                       const gchar     *password);
void             shopno_qs_network_disconnect         (ShopnoQsNetwork *network);

ShopnoQsWiredState shopno_qs_network_get_wired_state   (ShopnoQsNetwork *network);

/* Connects every plugged-in port, or disconnects every port. A port
 * disconnected this way stays down until it is connected again. */
void             shopno_qs_network_set_wired_connected (ShopnoQsNetwork *network,
                                                        gboolean         connected);

/* An icon name for the primary connection, for the panel button */
const gchar     *shopno_qs_network_get_icon_name      (ShopnoQsNetwork *network);

/* An icon name for a signal strength */
const gchar     *shopno_qs_network_strength_icon      (guint8           strength);

G_END_DECLS

#endif /* !__SHOPNO_QS_NETWORK_H__ */
