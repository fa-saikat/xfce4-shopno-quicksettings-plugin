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
 * Focus assist tile: Do Not Disturb of xfce4-notifyd, which keeps it in
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

/* The name xfce4-notifyd gives in GetServerInformation */
#define NOTIFYD_NAME "Xfce Notify Daemon"

typedef struct
{
  GtkWidget     *tile;
  XfconfChannel *channel;
  GCancellable  *cancellable;
  gboolean       other_daemon;
} Focus;



static void
focus_free (gpointer data)
{
  Focus *focus = data;

  g_cancellable_cancel (focus->cancellable);
  g_object_unref (focus->cancellable);

  if (focus->channel != NULL)
    {
      g_signal_handlers_disconnect_by_data (focus->channel, focus);
      xfconf_shutdown ();
    }

  g_free (focus);
}



static void
focus_update (Focus *focus)
{
  ShopnoQsTile *tile = SHOPNO_QS_TILE (focus->tile);
  gboolean      available = focus->channel != NULL && !focus->other_daemon;

  shopno_qs_tile_set_available (tile, available,
                                focus->channel == NULL
                                ? _("The settings service is not running")
                                : _("Needs the Xfce notification daemon"));

  if (available)
    shopno_qs_tile_set_active (tile, xfconf_channel_get_bool (focus->channel, PROPERTY, FALSE));
}



static void
focus_toggled (Focus *focus)
{
  if (focus->channel == NULL)
    return;

  /* The tile follows through "property-changed" */
  xfconf_channel_set_bool (focus->channel, PROPERTY,
                           !xfconf_channel_get_bool (focus->channel, PROPERTY, FALSE));
}



static void
focus_server_info (GObject      *source,
                   GAsyncResult *result,
                   gpointer      data)
{
  GVariant    *reply;
  GError      *error = NULL;
  const gchar *name = NULL;
  Focus       *focus;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (reply == NULL && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  focus = data;

  if (reply != NULL)
    {
      g_variant_get (reply, "(&s&s&s&s)", &name, NULL, NULL, NULL);
      focus->other_daemon = g_strcmp0 (name, NOTIFYD_NAME) != 0;
      g_variant_unref (reply);
    }
  else
    {
      /* No daemon running: xfce4-notifyd is started on demand */
      focus->other_daemon = FALSE;
      g_error_free (error);
    }

  focus_update (focus);
}



static void
focus_check_daemon (Focus *focus)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (bus == NULL)
    return;

  /* Do not start a daemon just to ask its name */
  g_dbus_connection_call (bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications", "GetServerInformation",
                          NULL, G_VARIANT_TYPE ("(ssss)"), G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1, focus->cancellable, focus_server_info, focus);
  g_object_unref (bus);
}



void
shopno_qs_focus_attach (ShopnoQsWindow *window)
{
  Focus  *focus = g_new0 (Focus, 1);
  GError *error = NULL;

  focus->cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-focus", focus, focus_free);

  if (xfconf_init (&error))
    {
      focus->channel = xfconf_channel_get (CHANNEL);
      g_signal_connect_swapped (focus->channel, "property-changed::" PROPERTY,
                                G_CALLBACK (focus_update), focus);
    }
  else
    {
      g_warning ("Failed to connect to xfconf: %s", error->message);
      g_error_free (error);
    }

  focus->tile = shopno_qs_tile_new ("weather-clear-night-symbolic", _("Focus assist"), FALSE);
  g_signal_connect_swapped (focus->tile, "toggled", G_CALLBACK (focus_toggled), focus);
  shopno_qs_window_add_tile (window, SHOPNO_QS_TILE_FOCUS, focus->tile);

  /* The daemon may change while the panel runs, so ask again each time
   * the flyout opens */
  g_signal_connect_swapped (window, "map", G_CALLBACK (focus_check_daemon), focus);

  focus_update (focus);
  focus_check_daemon (focus);
}
