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
 * Do not disturb tile: Do Not Disturb of xfce4-notifyd, which keeps it in
 * the /do-not-disturb property of its xfconf channel. Changes made in
 * the notification settings show up here too.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <xfconf/xfconf.h>

#include "shopno-qs-features.h"
#include "shopno-qs-tile.h"

#define CHANNEL  "xfce4-notifyd"
#define PROPERTY "/do-not-disturb"

#define ICON_OFF "preferences-system-notifications-symbolic"
#define ICON_ON  "notifications-disabled-symbolic"

/* The name xfce4-notifyd gives in GetServerInformation */
#define NOTIFYD_NAME "Xfce Notify Daemon"

typedef struct
{
  GtkWidget     *tile;
  XfconfChannel *channel;
  GCancellable  *cancellable;
  gboolean       other_daemon;
} Dnd;



static void
dnd_free (gpointer data)
{
  Dnd *dnd = data;

  g_cancellable_cancel (dnd->cancellable);
  g_object_unref (dnd->cancellable);

  if (dnd->channel != NULL)
    {
      g_signal_handlers_disconnect_by_data (dnd->channel, dnd);
      xfconf_shutdown ();
    }

  g_free (dnd);
}



static void
dnd_update (Dnd *dnd)
{
  ShopnoQsTile *tile = SHOPNO_QS_TILE (dnd->tile);
  gboolean      available = dnd->channel != NULL && !dnd->other_daemon;
  gboolean      active;

  shopno_qs_tile_set_available (tile, available,
                                dnd->channel == NULL
                                ? _("The settings service is not running")
                                : _("Needs the Xfce notification daemon"));

  /* A bell, crossed out while notifications are held back */
  active = available && xfconf_channel_get_bool (dnd->channel, PROPERTY, FALSE);
  shopno_qs_tile_set_icon_name (tile, active ? ICON_ON : ICON_OFF);
  if (available)
    shopno_qs_tile_set_active (tile, active);
}



static void
dnd_toggled (Dnd *dnd)
{
  if (dnd->channel == NULL)
    return;

  /* The tile follows through "property-changed" */
  xfconf_channel_set_bool (dnd->channel, PROPERTY,
                           !xfconf_channel_get_bool (dnd->channel, PROPERTY, FALSE));
}



static void
dnd_server_info (GObject      *source,
                   GAsyncResult *result,
                   gpointer      data)
{
  GVariant    *reply;
  GError      *error = NULL;
  const gchar *name = NULL;
  Dnd       *dnd;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (reply == NULL && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  dnd = data;

  if (reply != NULL)
    {
      g_variant_get (reply, "(&s&s&s&s)", &name, NULL, NULL, NULL);
      dnd->other_daemon = g_strcmp0 (name, NOTIFYD_NAME) != 0;
      g_variant_unref (reply);
    }
  else
    {
      /* No daemon running: xfce4-notifyd is started on demand */
      dnd->other_daemon = FALSE;
      g_error_free (error);
    }

  dnd_update (dnd);
}



static void
dnd_check_daemon (Dnd *dnd)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (bus == NULL)
    return;

  /* Do not start a daemon just to ask its name */
  g_dbus_connection_call (bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications", "GetServerInformation",
                          NULL, G_VARIANT_TYPE ("(ssss)"), G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1, dnd->cancellable, dnd_server_info, dnd);
  g_object_unref (bus);
}



void
shopno_qs_dnd_attach (ShopnoQsWindow *window)
{
  Dnd  *dnd = g_new0 (Dnd, 1);
  GError *error = NULL;

  dnd->cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-dnd", dnd, dnd_free);

  if (xfconf_init (&error))
    {
      dnd->channel = xfconf_channel_get (CHANNEL);
      g_signal_connect_swapped (dnd->channel, "property-changed::" PROPERTY,
                                G_CALLBACK (dnd_update), dnd);
    }
  else
    {
      g_warning ("Failed to connect to xfconf: %s", error->message);
      g_error_free (error);
    }

  dnd->tile = shopno_qs_tile_new (ICON_OFF, _("Do not disturb"), FALSE);
  g_signal_connect_swapped (dnd->tile, "toggled", G_CALLBACK (dnd_toggled), dnd);
  shopno_qs_window_add_tile (window, SHOPNO_QS_TILE_DND, dnd->tile);

  /* The daemon may change while the panel runs, so ask again each time
   * the flyout opens */
  g_signal_connect_swapped (window, "map", G_CALLBACK (dnd_check_daemon), dnd);

  dnd_update (dnd);
  dnd_check_daemon (dnd);
}
