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
 * Bluetooth through BlueZ on the system bus, with no extra library: the
 * first adapter's power, and its paired devices.
 *
 * Pairing new devices needs an agent for PIN codes and is left to
 * blueman-manager for now.
 */

#include "config.h"

#include <fcntl.h>
#include <linux/rfkill.h>
#include <unistd.h>

#include <gio/gio.h>

#include "shopno-qs-bluetooth.h"

#define BLUEZ_NAME       "org.bluez"
#define ADAPTER_IFACE    "org.bluez.Adapter1"
#define DEVICE_IFACE     "org.bluez.Device1"
#define BATTERY_IFACE    "org.bluez.Battery1"
#define PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

/* Connecting to headphones can take a while */
#define CONNECT_TIMEOUT 30000

enum
{
  CHANGED,
  N_SIGNALS
};

struct _ShopnoQsBluetooth
{
  GObject             __parent__;

  GCancellable       *cancellable;
  GDBusObjectManager *manager;

  GDBusProxy         *adapter;
  GPtrArray          *devices;
  GHashTable         *busy;
  gchar              *failed_path;
  gchar              *connected_name;

  guint               update_id;
  guint               unblock_id;
};

static guint              signals[N_SIGNALS];
static ShopnoQsBluetooth *default_bluetooth = NULL;

G_DEFINE_TYPE (ShopnoQsBluetooth, shopno_qs_bluetooth, G_TYPE_OBJECT)

static void shopno_qs_bluetooth_schedule_update (ShopnoQsBluetooth *self);



static void
shopno_qs_bluetooth_device_free (gpointer data)
{
  ShopnoQsBluetoothDevice *device = data;

  g_free (device->path);
  g_free (device->name);
  g_free (device->icon_name);
  g_free (device);
}



static gboolean
shopno_qs_bluetooth_get_bool (GDBusProxy  *proxy,
                              const gchar *name)
{
  GVariant *value = g_dbus_proxy_get_cached_property (proxy, name);
  gboolean  result = FALSE;

  if (value != NULL)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        result = g_variant_get_boolean (value);
      g_variant_unref (value);
    }

  return result;
}



static gchar *
shopno_qs_bluetooth_get_string (GDBusProxy  *proxy,
                                const gchar *name)
{
  GVariant *value = g_dbus_proxy_get_cached_property (proxy, name);
  gchar    *result = NULL;

  if (value != NULL)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING)
          || g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH))
        result = g_variant_dup_string (value, NULL);
      g_variant_unref (value);
    }

  return result;
}



static gint
shopno_qs_bluetooth_compare_devices (gconstpointer a,
                                     gconstpointer b)
{
  const ShopnoQsBluetoothDevice *da = *(ShopnoQsBluetoothDevice **) a;
  const ShopnoQsBluetoothDevice *db = *(ShopnoQsBluetoothDevice **) b;

  if (da->connected != db->connected)
    return da->connected ? -1 : 1;

  return g_utf8_collate (da->name, db->name);
}



/*
 * Reading the state
 */

static gboolean
shopno_qs_bluetooth_update (gpointer data)
{
  ShopnoQsBluetooth *self = data;
  GList             *objects = NULL;
  const gchar       *adapter_path = NULL;

  self->update_id = 0;

  g_clear_object (&self->adapter);
  g_ptr_array_set_size (self->devices, 0);
  g_clear_pointer (&self->connected_name, g_free);

  if (self->manager != NULL)
    objects = g_dbus_object_manager_get_objects (self->manager);

  /* The first adapter */
  for (GList *l = objects; l != NULL && self->adapter == NULL; l = l->next)
    {
      GDBusInterface *adapter = g_dbus_object_get_interface (l->data, ADAPTER_IFACE);

      if (adapter != NULL)
        {
          self->adapter = G_DBUS_PROXY (adapter);
          adapter_path = g_dbus_object_get_object_path (l->data);
        }
    }

  /* and its paired devices */
  for (GList *l = objects; l != NULL && self->adapter != NULL; l = l->next)
    {
      GDBusProxy              *proxy;
      GDBusProxy              *battery;
      ShopnoQsBluetoothDevice *device;
      gchar                   *owner;
      gchar                   *icon;
      GVariant                *value;

      proxy = G_DBUS_PROXY (g_dbus_object_get_interface (l->data, DEVICE_IFACE));
      if (proxy == NULL)
        continue;

      owner = shopno_qs_bluetooth_get_string (proxy, "Adapter");
      if (!shopno_qs_bluetooth_get_bool (proxy, "Paired") || g_strcmp0 (owner, adapter_path) != 0)
        {
          g_free (owner);
          g_object_unref (proxy);
          continue;
        }
      g_free (owner);

      device = g_new0 (ShopnoQsBluetoothDevice, 1);
      device->path = g_strdup (g_dbus_proxy_get_object_path (proxy));
      device->name = shopno_qs_bluetooth_get_string (proxy, "Alias");
      if (device->name == NULL)
        device->name = shopno_qs_bluetooth_get_string (proxy, "Address");
      if (device->name == NULL)
        device->name = g_strdup (device->path);

      icon = shopno_qs_bluetooth_get_string (proxy, "Icon");
      device->icon_name = icon != NULL ? g_strconcat (icon, "-symbolic", NULL)
                                       : g_strdup ("bluetooth-active-symbolic");
      g_free (icon);

      device->connected = shopno_qs_bluetooth_get_bool (proxy, "Connected");
      device->busy = g_hash_table_contains (self->busy, device->path);
      device->failed = g_strcmp0 (device->path, self->failed_path) == 0;
      device->battery = -1;

      battery = G_DBUS_PROXY (g_dbus_object_get_interface (l->data, BATTERY_IFACE));
      if (battery != NULL)
        {
          value = g_dbus_proxy_get_cached_property (battery, "Percentage");
          if (value != NULL)
            {
              device->battery = g_variant_get_byte (value);
              g_variant_unref (value);
            }
          g_object_unref (battery);
        }

      if (device->connected && self->connected_name == NULL)
        self->connected_name = g_strdup (device->name);

      g_ptr_array_add (self->devices, device);
      g_object_unref (proxy);
    }

  g_list_free_full (objects, g_object_unref);
  g_ptr_array_sort (self->devices, shopno_qs_bluetooth_compare_devices);

  g_signal_emit (self, signals[CHANGED], 0);

  return G_SOURCE_REMOVE;
}



static void
shopno_qs_bluetooth_schedule_update (ShopnoQsBluetooth *self)
{
  if (self->update_id == 0)
    self->update_id = g_idle_add (shopno_qs_bluetooth_update, self);
}



static void
shopno_qs_bluetooth_manager_ready (GObject      *source,
                                   GAsyncResult *result,
                                   gpointer      data)
{
  ShopnoQsBluetooth  *self;
  GDBusObjectManager *manager;
  GError             *error = NULL;

  manager = g_dbus_object_manager_client_new_for_bus_finish (result, &error);
  if (manager == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to watch BlueZ: %s", error->message);
      g_error_free (error);
      return;
    }

  self = SHOPNO_QS_BLUETOOTH (data);
  self->manager = manager;

  /* Any change of objects, interfaces or properties */
  g_signal_connect_swapped (manager, "notify::name-owner",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);
  g_signal_connect_swapped (manager, "object-added",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);
  g_signal_connect_swapped (manager, "object-removed",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);
  g_signal_connect_swapped (manager, "interface-added",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);
  g_signal_connect_swapped (manager, "interface-removed",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);
  g_signal_connect_swapped (manager, "interface-proxy-properties-changed",
                            G_CALLBACK (shopno_qs_bluetooth_schedule_update), self);

  shopno_qs_bluetooth_schedule_update (self);
}



/*
 * Actions
 */

/* A soft rfkill block keeps the adapter from powering on. The device
 * node is writable by the user of the active session. */
static gboolean
shopno_qs_bluetooth_rfkill_unblock (void)
{
  struct rfkill_event event = { 0 };
  gint                fd;
  gboolean            ok;

  fd = open ("/dev/rfkill", O_WRONLY | O_CLOEXEC);
  if (fd < 0)
    return FALSE;

  event.type = RFKILL_TYPE_BLUETOOTH;
  event.op = RFKILL_OP_CHANGE_ALL;
  event.soft = 0;
  ok = write (fd, &event, sizeof (event)) == sizeof (event);
  close (fd);

  return ok;
}



/* Time for BlueZ to see an rfkill unblock before powering on */
#define UNBLOCK_DELAY 300



static void
shopno_qs_bluetooth_powered_done (GObject      *source,
                                  GAsyncResult *result,
                                  gpointer      data)
{
  GVariant *reply;
  GError   *error = NULL;
  gchar    *remote;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (reply != NULL)
    {
      g_variant_unref (reply);
    }
  else
    {
      remote = g_dbus_error_get_remote_error (error);

      /* Busy: the adapter is already switching, often by itself right
       * after an rfkill unblock */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
          && g_strcmp0 (remote, "org.bluez.Error.Busy") != 0)
        g_warning ("Failed to switch Bluetooth: %s", error->message);

      g_free (remote);
      g_error_free (error);
    }
}



static void
shopno_qs_bluetooth_call_set_powered (ShopnoQsBluetooth *self,
                                      gboolean           powered)
{
  if (self->adapter == NULL)
    return;

  g_dbus_connection_call (g_dbus_proxy_get_connection (self->adapter), BLUEZ_NAME,
                          g_dbus_proxy_get_object_path (self->adapter), PROPERTIES_IFACE, "Set",
                          g_variant_new ("(ssv)", ADAPTER_IFACE, "Powered", g_variant_new_boolean (powered)),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, self->cancellable,
                          shopno_qs_bluetooth_powered_done, NULL);
}



static gboolean
shopno_qs_bluetooth_power_on_unblocked (gpointer data)
{
  ShopnoQsBluetooth *self = data;

  self->unblock_id = 0;

  /* BlueZ may have powered the adapter on by itself after the unblock */
  if (!shopno_qs_bluetooth_get_powered (self))
    shopno_qs_bluetooth_call_set_powered (self, TRUE);

  return G_SOURCE_REMOVE;
}



typedef struct
{
  ShopnoQsBluetooth *bluetooth;
  gchar             *path;
} DeviceData;



static void
shopno_qs_bluetooth_device_done (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      data)
{
  DeviceData        *call = data;
  ShopnoQsBluetooth *self = call->bluetooth;
  GVariant          *reply;
  GError            *error = NULL;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  g_hash_table_remove (self->busy, call->path);

  if (reply != NULL)
    {
      g_variant_unref (reply);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_message ("Bluetooth device %s: %s", call->path, error->message);
          g_free (self->failed_path);
          self->failed_path = g_strdup (call->path);
        }
      g_error_free (error);
    }

  shopno_qs_bluetooth_schedule_update (self);

  g_free (call->path);
  g_object_unref (call->bluetooth);
  g_free (call);
}



/*
 * GObject
 */

static void
shopno_qs_bluetooth_dispose (GObject *object)
{
  ShopnoQsBluetooth *self = SHOPNO_QS_BLUETOOTH (object);

  g_cancellable_cancel (self->cancellable);

  if (self->update_id != 0)
    {
      g_source_remove (self->update_id);
      self->update_id = 0;
    }

  if (self->unblock_id != 0)
    {
      g_source_remove (self->unblock_id);
      self->unblock_id = 0;
    }

  if (self->manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->manager, self);
      g_clear_object (&self->manager);
    }

  g_clear_object (&self->adapter);

  G_OBJECT_CLASS (shopno_qs_bluetooth_parent_class)->dispose (object);
}



static void
shopno_qs_bluetooth_finalize (GObject *object)
{
  ShopnoQsBluetooth *self = SHOPNO_QS_BLUETOOTH (object);

  g_object_unref (self->cancellable);
  g_ptr_array_unref (self->devices);
  g_hash_table_destroy (self->busy);
  g_free (self->failed_path);
  g_free (self->connected_name);

  G_OBJECT_CLASS (shopno_qs_bluetooth_parent_class)->finalize (object);
}



static void
shopno_qs_bluetooth_class_init (ShopnoQsBluetoothClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shopno_qs_bluetooth_dispose;
  gobject_class->finalize = shopno_qs_bluetooth_finalize;

  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_bluetooth_init (ShopnoQsBluetooth *self)
{
  self->cancellable = g_cancellable_new ();
  self->devices = g_ptr_array_new_with_free_func (shopno_qs_bluetooth_device_free);
  self->busy = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Also works while bluetoothd is not running, and follows it when it
   * starts or stops */
  g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                            BLUEZ_NAME, "/", NULL, NULL, NULL,
                                            self->cancellable,
                                            shopno_qs_bluetooth_manager_ready, self);
}



/*
 * Public API
 */

ShopnoQsBluetooth *
shopno_qs_bluetooth_get_default (void)
{
  if (default_bluetooth != NULL)
    return g_object_ref (default_bluetooth);

  default_bluetooth = g_object_new (SHOPNO_TYPE_QS_BLUETOOTH, NULL);
  g_object_add_weak_pointer (G_OBJECT (default_bluetooth), (gpointer *) &default_bluetooth);

  return default_bluetooth;
}



gboolean
shopno_qs_bluetooth_get_available (ShopnoQsBluetooth *bluetooth)
{
  g_return_val_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth), FALSE);

  return bluetooth->adapter != NULL;
}



gboolean
shopno_qs_bluetooth_get_powered (ShopnoQsBluetooth *bluetooth)
{
  g_return_val_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth), FALSE);

  return bluetooth->adapter != NULL && shopno_qs_bluetooth_get_bool (bluetooth->adapter, "Powered");
}



void
shopno_qs_bluetooth_set_powered (ShopnoQsBluetooth *bluetooth,
                                 gboolean           powered)
{
  g_return_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth));

  /* BlueZ only answers "Failed" while rfkill blocks the adapter, so
   * check first and unblock, as turning Bluetooth on means on Windows */
  if (powered && bluetooth->adapter != NULL)
    {
      gchar *state = shopno_qs_bluetooth_get_string (bluetooth->adapter, "PowerState");

      if (g_strcmp0 (state, "off-blocked") == 0 && shopno_qs_bluetooth_rfkill_unblock ())
        {
          g_free (state);
          if (bluetooth->unblock_id == 0)
            bluetooth->unblock_id = g_timeout_add (UNBLOCK_DELAY, shopno_qs_bluetooth_power_on_unblocked,
                                                   bluetooth);
          return;
        }

      g_free (state);
    }

  shopno_qs_bluetooth_call_set_powered (bluetooth, powered);
}



GPtrArray *
shopno_qs_bluetooth_get_devices (ShopnoQsBluetooth *bluetooth)
{
  g_return_val_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth), NULL);

  return bluetooth->devices;
}



const gchar *
shopno_qs_bluetooth_get_connected_name (ShopnoQsBluetooth *bluetooth)
{
  g_return_val_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth), NULL);

  return bluetooth->connected_name;
}



void
shopno_qs_bluetooth_toggle_device (ShopnoQsBluetooth *bluetooth,
                                   const gchar       *path)
{
  ShopnoQsBluetoothDevice *device = NULL;
  DeviceData              *call;

  g_return_if_fail (SHOPNO_IS_QS_BLUETOOTH (bluetooth));

  for (guint i = 0; i < bluetooth->devices->len && device == NULL; i++)
    if (g_strcmp0 (((ShopnoQsBluetoothDevice *) g_ptr_array_index (bluetooth->devices, i))->path, path) == 0)
      device = g_ptr_array_index (bluetooth->devices, i);

  if (device == NULL || g_hash_table_contains (bluetooth->busy, path) || bluetooth->adapter == NULL)
    return;

  g_clear_pointer (&bluetooth->failed_path, g_free);
  g_hash_table_add (bluetooth->busy, g_strdup (path));

  call = g_new0 (DeviceData, 1);
  call->bluetooth = g_object_ref (bluetooth);
  call->path = g_strdup (path);

  g_dbus_connection_call (g_dbus_proxy_get_connection (bluetooth->adapter), BLUEZ_NAME, path,
                          DEVICE_IFACE, device->connected ? "Disconnect" : "Connect",
                          NULL, NULL, G_DBUS_CALL_FLAGS_NONE, CONNECT_TIMEOUT,
                          bluetooth->cancellable, shopno_qs_bluetooth_device_done, call);

  shopno_qs_bluetooth_schedule_update (bluetooth);
}
