/*
 * Copyright (C) 2026 FA Saikat
 * Copyright (C) 2017 Viktor Odintsev <ninetls@xfce.org>
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
 * The StatusNotifierItem tray: apps register their icons with a watcher,
 * and hosts like this plugin show them.
 *
 * The tray is both. It runs the watcher when nobody else does, and is a
 * host of whichever watcher runs, its own or another program's. The
 * structure follows sn-backend.c of Xfce's systray plugin.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>

#include "shopno-qs-tray.h"

#define WATCHER_NAME  "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH  "/StatusNotifierWatcher"
#define WATCHER_IFACE "org.kde.StatusNotifierWatcher"

static const gchar watcher_xml[] =
  "<node>"
  "  <interface name='org.kde.StatusNotifierWatcher'>"
  "    <method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
  "    <method name='RegisterStatusNotifierHost'><arg type='s' direction='in'/></method>"
  "    <property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
  "    <property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
  "    <property name='ProtocolVersion' type='i' access='read'/>"
  "    <signal name='StatusNotifierItemRegistered'><arg type='s'/></signal>"
  "    <signal name='StatusNotifierItemUnregistered'><arg type='s'/></signal>"
  "    <signal name='StatusNotifierHostRegistered'/>"
  "    <signal name='StatusNotifierHostUnregistered'/>"
  "  </interface>"
  "</node>";

enum
{
  ITEM_ADDED,
  ITEM_REMOVED,
  N_SIGNALS
};

struct _ShopnoQsTray
{
  GObject          __parent__;

  GDBusConnection *bus;
  GCancellable    *cancellable;

  /* Watcher: key -> name watch id, for items registered with us */
  guint            watcher_owner_id;
  guint            watcher_object_id;
  GHashTable      *watched;

  /* Host */
  gchar           *host_name;
  guint            host_owner_id;
  guint            host_watch_id;
  guint            registered_id;
  guint            unregistered_id;
  GPtrArray       *items;
};

static guint         signals[N_SIGNALS];
static ShopnoQsTray *default_tray = NULL;

G_DEFINE_TYPE (ShopnoQsTray, shopno_qs_tray, G_TYPE_OBJECT)



/* "bus.name/object/path" into its parts */
static gboolean
shopno_qs_tray_parse_key (const gchar  *key,
                          gchar       **bus_name,
                          gchar       **object_path)
{
  const gchar *slash = strchr (key, '/');

  if (slash == NULL || slash == key)
    return FALSE;

  *bus_name = g_strndup (key, slash - key);
  if (!g_dbus_is_name (*bus_name) || !g_variant_is_object_path (slash))
    {
      g_free (*bus_name);
      return FALSE;
    }

  *object_path = g_strdup (slash);
  return TRUE;
}



/*
 * Host
 */

static ShopnoQsTrayItem *
shopno_qs_tray_find (ShopnoQsTray *self,
                     const gchar  *key)
{
  for (guint i = 0; i < self->items->len; i++)
    if (g_strcmp0 (shopno_qs_tray_item_get_key (g_ptr_array_index (self->items, i)), key) == 0)
      return g_ptr_array_index (self->items, i);

  return NULL;
}



static void
shopno_qs_tray_add (ShopnoQsTray *self,
                    const gchar  *key)
{
  ShopnoQsTrayItem *item;
  gchar            *bus_name;
  gchar            *object_path;

  if (shopno_qs_tray_find (self, key) != NULL
      || !shopno_qs_tray_parse_key (key, &bus_name, &object_path))
    return;

  item = shopno_qs_tray_item_new (key, bus_name, object_path);
  g_ptr_array_add (self->items, item);
  g_signal_emit (self, signals[ITEM_ADDED], 0, item);

  g_free (bus_name);
  g_free (object_path);
}



static void
shopno_qs_tray_remove (ShopnoQsTray *self,
                       const gchar  *key)
{
  ShopnoQsTrayItem *item = shopno_qs_tray_find (self, key);

  if (item == NULL)
    return;

  g_object_ref (item);
  g_ptr_array_remove (self->items, item);
  g_signal_emit (self, signals[ITEM_REMOVED], 0, item);
  g_object_unref (item);
}



static void
shopno_qs_tray_clear (ShopnoQsTray *self)
{
  while (self->items->len > 0)
    shopno_qs_tray_remove (self, shopno_qs_tray_item_get_key (g_ptr_array_index (self->items, 0)));
}



static void
shopno_qs_tray_watcher_signal (GDBusConnection *connection,
                               const gchar     *sender,
                               const gchar     *path,
                               const gchar     *interface,
                               const gchar     *signal,
                               GVariant        *parameters,
                               gpointer         data)
{
  ShopnoQsTray *self = data;
  const gchar  *key;

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(s)")))
    return;

  g_variant_get (parameters, "(&s)", &key);

  if (g_strcmp0 (signal, "StatusNotifierItemRegistered") == 0)
    shopno_qs_tray_add (self, key);
  else
    shopno_qs_tray_remove (self, key);
}



static void
shopno_qs_tray_got_items (GObject      *source,
                          GAsyncResult *result,
                          gpointer      data)
{
  GVariant     *reply;
  GVariant     *value;
  GVariantIter  iter;
  const gchar  *key;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, NULL);
  if (reply == NULL)
    return;

  g_variant_get (reply, "(v)", &value);
  if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING_ARRAY))
    {
      g_variant_iter_init (&iter, value);
      while (g_variant_iter_next (&iter, "&s", &key))
        shopno_qs_tray_add (data, key);
    }

  g_variant_unref (value);
  g_variant_unref (reply);
}



static void
shopno_qs_tray_watcher_appeared (GDBusConnection *connection,
                                 const gchar     *name,
                                 const gchar     *owner,
                                 gpointer         data)
{
  ShopnoQsTray *self = data;

  self->registered_id =
    g_dbus_connection_signal_subscribe (connection, owner, WATCHER_IFACE,
                                        "StatusNotifierItemRegistered", WATCHER_PATH, NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        shopno_qs_tray_watcher_signal, self, NULL);
  self->unregistered_id =
    g_dbus_connection_signal_subscribe (connection, owner, WATCHER_IFACE,
                                        "StatusNotifierItemUnregistered", WATCHER_PATH, NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        shopno_qs_tray_watcher_signal, self, NULL);

  /* Some apps only show an icon once a host is registered */
  g_dbus_connection_call (connection, owner, WATCHER_PATH, WATCHER_IFACE,
                          "RegisterStatusNotifierHost", g_variant_new ("(s)", self->host_name),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, self->cancellable, NULL, NULL);

  g_dbus_connection_call (connection, owner, WATCHER_PATH, "org.freedesktop.DBus.Properties",
                          "Get", g_variant_new ("(ss)", WATCHER_IFACE, "RegisteredStatusNotifierItems"),
                          G_VARIANT_TYPE ("(v)"), G_DBUS_CALL_FLAGS_NONE, -1,
                          self->cancellable, shopno_qs_tray_got_items, self);
}



static void
shopno_qs_tray_watcher_vanished (GDBusConnection *connection,
                                 const gchar     *name,
                                 gpointer         data)
{
  ShopnoQsTray *self = data;

  if (self->registered_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (self->bus, self->registered_id);
      g_dbus_connection_signal_unsubscribe (self->bus, self->unregistered_id);
      self->registered_id = self->unregistered_id = 0;
    }

  /* Items register again with the next watcher */
  shopno_qs_tray_clear (self);
}



/*
 * Watcher
 */

typedef struct
{
  ShopnoQsTray *tray;
  gchar        *key;
} WatchedItem;



static void
shopno_qs_tray_watched_free (gpointer data)
{
  WatchedItem *watched = data;

  g_free (watched->key);
  g_free (watched);
}



static void
shopno_qs_tray_emit_watcher_signal (ShopnoQsTray *self,
                                    const gchar  *signal,
                                    const gchar  *key)
{
  g_dbus_connection_emit_signal (self->bus, NULL, WATCHER_PATH, WATCHER_IFACE, signal,
                                 key != NULL ? g_variant_new ("(s)", key) : NULL, NULL);
}



static void
shopno_qs_tray_item_vanished (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         data)
{
  WatchedItem  *watched = data;
  ShopnoQsTray *self = watched->tray;
  gchar        *key = g_strdup (watched->key);

  /* Removing the entry frees the watch and its data */
  g_hash_table_remove (self->watched, key);
  shopno_qs_tray_emit_watcher_signal (self, "StatusNotifierItemUnregistered", key);
  g_free (key);
}



static void
shopno_qs_tray_unwatch (gpointer data)
{
  g_bus_unwatch_name (GPOINTER_TO_UINT (data));
}



static void
shopno_qs_tray_register_item (ShopnoQsTray          *self,
                              GDBusMethodInvocation *invocation,
                              const gchar           *service)
{
  const gchar *sender = g_dbus_method_invocation_get_sender (invocation);
  const gchar *bus_name;
  const gchar *object_path;
  gchar       *key;
  WatchedItem *watched;
  guint        watch_id;

  /* Apps send their bus name, or with libappindicator, an object path
   * on the bus they called from */
  if (service[0] == '/')
    {
      bus_name = sender;
      object_path = service;
    }
  else
    {
      bus_name = service;
      object_path = "/StatusNotifierItem";
    }

  if (!g_dbus_is_name (bus_name) || !g_variant_is_object_path (object_path))
    {
      g_dbus_method_invocation_return_error_literal (invocation, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                                     "Invalid StatusNotifierItem");
      return;
    }

  key = g_strconcat (bus_name, object_path, NULL);

  if (!g_hash_table_contains (self->watched, key))
    {
      watched = g_new0 (WatchedItem, 1);
      watched->tray = self;
      watched->key = g_strdup (key);
      watch_id = g_bus_watch_name_on_connection (self->bus, bus_name, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                 NULL, shopno_qs_tray_item_vanished,
                                                 watched, shopno_qs_tray_watched_free);
      g_hash_table_insert (self->watched, g_strdup (key), GUINT_TO_POINTER (watch_id));
    }

  g_dbus_method_invocation_return_value (invocation, NULL);
  shopno_qs_tray_emit_watcher_signal (self, "StatusNotifierItemRegistered", key);
  g_free (key);
}



static void
shopno_qs_tray_watcher_method (GDBusConnection       *connection,
                               const gchar           *sender,
                               const gchar           *path,
                               const gchar           *interface,
                               const gchar           *method,
                               GVariant              *parameters,
                               GDBusMethodInvocation *invocation,
                               gpointer               data)
{
  ShopnoQsTray *self = data;
  const gchar  *service;

  g_variant_get (parameters, "(&s)", &service);

  if (g_strcmp0 (method, "RegisterStatusNotifierItem") == 0)
    {
      shopno_qs_tray_register_item (self, invocation, service);
    }
  else
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
      shopno_qs_tray_emit_watcher_signal (self, "StatusNotifierHostRegistered", NULL);
    }
}



static GVariant *
shopno_qs_tray_watcher_property (GDBusConnection  *connection,
                                 const gchar      *sender,
                                 const gchar      *path,
                                 const gchar      *interface,
                                 const gchar      *property,
                                 GError          **error,
                                 gpointer          data)
{
  ShopnoQsTray    *self = data;
  GVariantBuilder  builder;
  GHashTableIter   iter;
  gpointer         key;

  if (g_strcmp0 (property, "IsStatusNotifierHostRegistered") == 0)
    return g_variant_new_boolean (TRUE);

  if (g_strcmp0 (property, "ProtocolVersion") == 0)
    return g_variant_new_int32 (0);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  g_hash_table_iter_init (&iter, self->watched);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_variant_builder_add (&builder, "s", key);

  return g_variant_builder_end (&builder);
}



static const GDBusInterfaceVTable watcher_vtable =
{
  shopno_qs_tray_watcher_method,
  shopno_qs_tray_watcher_property,
  NULL,
  { NULL }
};



static void
shopno_qs_tray_watcher_acquired (GDBusConnection *connection,
                                 const gchar     *name,
                                 gpointer         data)
{
  ShopnoQsTray  *self = data;
  GDBusNodeInfo *info;
  GError        *error = NULL;

  if (self->watcher_object_id != 0)
    return;

  info = g_dbus_node_info_new_for_xml (watcher_xml, NULL);
  self->watcher_object_id = g_dbus_connection_register_object (connection, WATCHER_PATH,
                                                               info->interfaces[0], &watcher_vtable,
                                                               self, NULL, &error);
  g_dbus_node_info_unref (info);

  if (self->watcher_object_id == 0)
    {
      g_warning ("Failed to run the tray watcher: %s", error->message);
      g_error_free (error);
    }
}



static void
shopno_qs_tray_watcher_lost (GDBusConnection *connection,
                             const gchar     *name,
                             gpointer         data)
{
  ShopnoQsTray *self = data;

  /* Another program runs the watcher; this tray stays a host of it */
  g_hash_table_remove_all (self->watched);

  if (self->watcher_object_id != 0)
    {
      g_dbus_connection_unregister_object (self->bus, self->watcher_object_id);
      self->watcher_object_id = 0;
    }
}



/*
 * GObject
 */

static void
shopno_qs_tray_dispose (GObject *object)
{
  ShopnoQsTray *self = SHOPNO_QS_TRAY (object);

  g_cancellable_cancel (self->cancellable);

  if (self->host_watch_id != 0)
    {
      g_bus_unwatch_name (self->host_watch_id);
      self->host_watch_id = 0;
    }

  if (self->host_owner_id != 0)
    {
      g_bus_unown_name (self->host_owner_id);
      self->host_owner_id = 0;
    }

  if (self->watcher_owner_id != 0)
    {
      g_bus_unown_name (self->watcher_owner_id);
      self->watcher_owner_id = 0;
    }

  if (self->bus != NULL)
    {
      shopno_qs_tray_watcher_lost (self->bus, NULL, self);
      shopno_qs_tray_watcher_vanished (self->bus, NULL, self);
    }

  g_clear_object (&self->bus);

  G_OBJECT_CLASS (shopno_qs_tray_parent_class)->dispose (object);
}



static void
shopno_qs_tray_finalize (GObject *object)
{
  ShopnoQsTray *self = SHOPNO_QS_TRAY (object);

  g_object_unref (self->cancellable);
  g_hash_table_destroy (self->watched);
  g_ptr_array_unref (self->items);
  g_free (self->host_name);

  G_OBJECT_CLASS (shopno_qs_tray_parent_class)->finalize (object);
}



static void
shopno_qs_tray_class_init (ShopnoQsTrayClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shopno_qs_tray_dispose;
  gobject_class->finalize = shopno_qs_tray_finalize;

  signals[ITEM_ADDED] =
    g_signal_new ("item-added", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, SHOPNO_TYPE_QS_TRAY_ITEM);

  signals[ITEM_REMOVED] =
    g_signal_new ("item-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, SHOPNO_TYPE_QS_TRAY_ITEM);
}



static void
shopno_qs_tray_init (ShopnoQsTray *self)
{
  static guint counter = 0;

  self->cancellable = g_cancellable_new ();
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->watched = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, shopno_qs_tray_unwatch);

  self->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (self->bus == NULL)
    return;

  /* Run the watcher unless another program already does */
  self->watcher_owner_id = g_bus_own_name_on_connection (self->bus, WATCHER_NAME,
                                                         G_BUS_NAME_OWNER_FLAGS_NONE,
                                                         shopno_qs_tray_watcher_acquired,
                                                         shopno_qs_tray_watcher_lost,
                                                         self, NULL);

  self->host_name = g_strdup_printf ("org.kde.StatusNotifierHost-%d-%u", (gint) getpid (), ++counter);
  self->host_owner_id = g_bus_own_name_on_connection (self->bus, self->host_name,
                                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                                      NULL, NULL, NULL, NULL);

  self->host_watch_id = g_bus_watch_name_on_connection (self->bus, WATCHER_NAME,
                                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                        shopno_qs_tray_watcher_appeared,
                                                        shopno_qs_tray_watcher_vanished,
                                                        self, NULL);
}



ShopnoQsTray *
shopno_qs_tray_get_default (void)
{
  if (default_tray != NULL)
    return g_object_ref (default_tray);

  default_tray = g_object_new (SHOPNO_TYPE_QS_TRAY, NULL);
  g_object_add_weak_pointer (G_OBJECT (default_tray), (gpointer *) &default_tray);

  return default_tray;
}



GPtrArray *
shopno_qs_tray_get_items (ShopnoQsTray *tray)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY (tray), NULL);

  return tray->items;
}
