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

#ifndef __SHOPNO_QS_BLUETOOTH_H__
#define __SHOPNO_QS_BLUETOOTH_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_BLUETOOTH (shopno_qs_bluetooth_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsBluetooth, shopno_qs_bluetooth, SHOPNO, QS_BLUETOOTH, GObject)

typedef struct
{
  gchar    *path;
  gchar    *name;
  gchar    *icon_name;
  gboolean  connected;
  gboolean  busy;
  gboolean  failed;   /* the last connection attempt failed */
  gint      battery;    /* -1 when unknown */
} ShopnoQsBluetoothDevice;

ShopnoQsBluetooth *shopno_qs_bluetooth_get_default    (void);

/* bluetoothd is running and there is an adapter */
gboolean           shopno_qs_bluetooth_get_available  (ShopnoQsBluetooth *bluetooth);
gboolean           shopno_qs_bluetooth_get_powered    (ShopnoQsBluetooth *bluetooth);
void               shopno_qs_bluetooth_set_powered    (ShopnoQsBluetooth *bluetooth,
                                                       gboolean           powered);

/* Paired devices as ShopnoQsBluetoothDevice, connected ones first.
 * Owned by the backend. */
GPtrArray         *shopno_qs_bluetooth_get_devices    (ShopnoQsBluetooth *bluetooth);

/* The name of a connected device, or NULL */
const gchar       *shopno_qs_bluetooth_get_connected_name (ShopnoQsBluetooth *bluetooth);

void               shopno_qs_bluetooth_toggle_device  (ShopnoQsBluetooth *bluetooth,
                                                       const gchar       *path);

G_END_DECLS

#endif /* !__SHOPNO_QS_BLUETOOTH_H__ */
