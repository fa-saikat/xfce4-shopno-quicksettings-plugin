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
 * The battery as UPower presents it: its DisplayDevice combines all
 * batteries of the computer into one, as the Windows battery icon does.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "shopno-qs-battery.h"

#define UPOWER_NAME   "org.freedesktop.UPower"
#define DISPLAY_PATH  "/org/freedesktop/UPower/devices/DisplayDevice"
#define DEVICE_IFACE  "org.freedesktop.UPower.Device"

/* UpDeviceKind and UpDeviceState */
#define KIND_BATTERY          2
#define STATE_CHARGING        1
#define STATE_FULLY_CHARGED   4
#define STATE_PENDING_CHARGE  5

enum
{
  CHANGED,
  N_SIGNALS
};

struct _ShopnoQsBattery
{
  GObject       __parent__;

  GCancellable *cancellable;
  GDBusProxy   *proxy;
  gchar        *icon_name;
};

static guint            signals[N_SIGNALS];
static ShopnoQsBattery *default_battery = NULL;

G_DEFINE_TYPE (ShopnoQsBattery, shopno_qs_battery, G_TYPE_OBJECT)



static GVariant *
shopno_qs_battery_get (ShopnoQsBattery    *self,
                       const gchar        *name,
                       const GVariantType *type)
{
  GVariant *value;

  if (self->proxy == NULL)
    return NULL;

  value = g_dbus_proxy_get_cached_property (self->proxy, name);
  if (value != NULL && !g_variant_is_of_type (value, type))
    g_clear_pointer (&value, g_variant_unref);

  return value;
}



static guint
shopno_qs_battery_get_uint (ShopnoQsBattery *self,
                            const gchar     *name)
{
  GVariant *value = shopno_qs_battery_get (self, name, G_VARIANT_TYPE_UINT32);
  guint     result = 0;

  if (value != NULL)
    {
      result = g_variant_get_uint32 (value);
      g_variant_unref (value);
    }

  return result;
}



static gint64
shopno_qs_battery_get_int64 (ShopnoQsBattery *self,
                             const gchar     *name)
{
  GVariant *value = shopno_qs_battery_get (self, name, G_VARIANT_TYPE_INT64);
  gint64    result = 0;

  if (value != NULL)
    {
      result = g_variant_get_int64 (value);
      g_variant_unref (value);
    }

  return result;
}



static void
shopno_qs_battery_emit_changed (ShopnoQsBattery *self)
{
  GVariant *value = shopno_qs_battery_get (self, "IconName", G_VARIANT_TYPE_STRING);

  /* UPower picks the icon for the level and state */
  g_free (self->icon_name);
  self->icon_name = value != NULL && *g_variant_get_string (value, NULL) != '\0'
                    ? g_variant_dup_string (value, NULL) : NULL;
  if (value != NULL)
    g_variant_unref (value);

  g_signal_emit (self, signals[CHANGED], 0);
}



static void
shopno_qs_battery_proxy_ready (GObject      *source,
                               GAsyncResult *result,
                               gpointer      data)
{
  ShopnoQsBattery *self;
  GDBusProxy      *proxy;
  GError          *error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to watch UPower: %s", error->message);
      g_error_free (error);
      return;
    }

  self = SHOPNO_QS_BATTERY (data);
  self->proxy = proxy;

  /* Follows upowerd restarting too, through the name owner */
  g_signal_connect_swapped (proxy, "g-properties-changed",
                            G_CALLBACK (shopno_qs_battery_emit_changed), self);
  g_signal_connect_swapped (proxy, "notify::g-name-owner",
                            G_CALLBACK (shopno_qs_battery_emit_changed), self);

  shopno_qs_battery_emit_changed (self);
}



static void
shopno_qs_battery_dispose (GObject *object)
{
  ShopnoQsBattery *self = SHOPNO_QS_BATTERY (object);

  g_cancellable_cancel (self->cancellable);

  if (self->proxy != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->proxy, self);
      g_clear_object (&self->proxy);
    }

  G_OBJECT_CLASS (shopno_qs_battery_parent_class)->dispose (object);
}



static void
shopno_qs_battery_finalize (GObject *object)
{
  ShopnoQsBattery *self = SHOPNO_QS_BATTERY (object);

  g_object_unref (self->cancellable);
  g_free (self->icon_name);

  G_OBJECT_CLASS (shopno_qs_battery_parent_class)->finalize (object);
}



static void
shopno_qs_battery_class_init (ShopnoQsBatteryClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shopno_qs_battery_dispose;
  gobject_class->finalize = shopno_qs_battery_finalize;

  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_battery_init (ShopnoQsBattery *self)
{
  self->cancellable = g_cancellable_new ();

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            NULL, UPOWER_NAME, DISPLAY_PATH, DEVICE_IFACE,
                            self->cancellable, shopno_qs_battery_proxy_ready, self);
}



ShopnoQsBattery *
shopno_qs_battery_get_default (void)
{
  if (default_battery != NULL)
    return g_object_ref (default_battery);

  default_battery = g_object_new (SHOPNO_TYPE_QS_BATTERY, NULL);
  g_object_add_weak_pointer (G_OBJECT (default_battery), (gpointer *) &default_battery);

  return default_battery;
}



gboolean
shopno_qs_battery_get_present (ShopnoQsBattery *battery)
{
  GVariant *value;
  gboolean  present = FALSE;

  g_return_val_if_fail (SHOPNO_IS_QS_BATTERY (battery), FALSE);

  value = shopno_qs_battery_get (battery, "IsPresent", G_VARIANT_TYPE_BOOLEAN);
  if (value != NULL)
    {
      present = g_variant_get_boolean (value);
      g_variant_unref (value);
    }

  return present && shopno_qs_battery_get_uint (battery, "Type") == KIND_BATTERY;
}



gdouble
shopno_qs_battery_get_percentage (ShopnoQsBattery *battery)
{
  GVariant *value;
  gdouble   percentage = 0.0;

  g_return_val_if_fail (SHOPNO_IS_QS_BATTERY (battery), 0.0);

  value = shopno_qs_battery_get (battery, "Percentage", G_VARIANT_TYPE_DOUBLE);
  if (value != NULL)
    {
      percentage = g_variant_get_double (value);
      g_variant_unref (value);
    }

  return percentage;
}



gboolean
shopno_qs_battery_get_plugged_in (ShopnoQsBattery *battery)
{
  guint state;

  g_return_val_if_fail (SHOPNO_IS_QS_BATTERY (battery), FALSE);

  state = shopno_qs_battery_get_uint (battery, "State");

  return state == STATE_CHARGING || state == STATE_FULLY_CHARGED || state == STATE_PENDING_CHARGE;
}



const gchar *
shopno_qs_battery_get_icon_name (ShopnoQsBattery *battery)
{
  g_return_val_if_fail (SHOPNO_IS_QS_BATTERY (battery), NULL);

  return battery->icon_name != NULL ? battery->icon_name : "battery-missing-symbolic";
}



static gchar *
shopno_qs_battery_format_time (gint64 seconds)
{
  gint64 minutes = (seconds + 30) / 60;
  gint64 hours = minutes / 60;

  minutes %= 60;

  if (hours > 0 && minutes > 0)
    /* TRANSLATORS: hours and minutes, as in "2 h 10 min" */
    return g_strdup_printf (_("%d h %d min"), (gint) hours, (gint) minutes);
  if (hours > 0)
    return g_strdup_printf (_("%d h"), (gint) hours);

  return g_strdup_printf (_("%d min"), (gint) MAX (minutes, 1));
}



gchar *
shopno_qs_battery_get_status (ShopnoQsBattery *battery)
{
  guint   state;
  gint64  seconds;
  gchar  *time;
  gchar  *status;

  g_return_val_if_fail (SHOPNO_IS_QS_BATTERY (battery), NULL);

  state = shopno_qs_battery_get_uint (battery, "State");

  if (state == STATE_FULLY_CHARGED)
    return g_strdup (_("Fully charged"));

  if (state == STATE_PENDING_CHARGE)
    return g_strdup (_("Plugged in, not charging"));

  if (state == STATE_CHARGING)
    {
      seconds = shopno_qs_battery_get_int64 (battery, "TimeToFull");
      if (seconds <= 0)
        return g_strdup (_("Charging"));

      time = shopno_qs_battery_format_time (seconds);
      status = g_strdup_printf (_("Charging, %s until full"), time);
      g_free (time);
      return status;
    }

  seconds = shopno_qs_battery_get_int64 (battery, "TimeToEmpty");
  if (seconds <= 0)
    return g_strdup (_("On battery"));

  time = shopno_qs_battery_format_time (seconds);
  status = g_strdup_printf (_("%s remaining"), time);
  g_free (time);

  return status;
}
