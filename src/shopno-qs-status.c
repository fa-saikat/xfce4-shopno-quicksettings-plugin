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

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-audio.h"
#include "shopno-qs-battery.h"
#include "shopno-qs-network.h"
#include "shopno-qs-status.h"

/* Volume change per scroll step, as on Windows */
#define SCROLL_STEP 0.02

/* How long the volume notification stays, in milliseconds */
#define VOLUME_NOTIFICATION_TIMEOUT 2000

struct _ShopnoQsStatus
{
  GtkBox           __parent__;

  ShopnoQsNetwork *network;
  ShopnoQsAudio   *audio;
  ShopnoQsBattery *battery;

  GtkWidget       *network_icon;
  GtkWidget       *volume_icon;
  GtkWidget       *battery_icon;

  gint             icon_size;

  /* The volume notification, replaced on each scroll step */
  GCancellable    *cancellable;
  guint32          notification_id;
};

G_DEFINE_TYPE (ShopnoQsStatus, shopno_qs_status, GTK_TYPE_BOX)



static void
shopno_qs_status_set_icon (ShopnoQsStatus *self,
                           GtkWidget      *image,
                           const gchar    *icon_name)
{
  gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (image), self->icon_size);
}



static void
shopno_qs_status_update_network (ShopnoQsStatus *self)
{
  shopno_qs_status_set_icon (self, self->network_icon,
                             shopno_qs_network_get_icon_name (self->network));
}



static void
shopno_qs_status_update_volume (ShopnoQsStatus *self)
{
  shopno_qs_status_set_icon (self, self->volume_icon,
                             shopno_qs_audio_get_icon_name (self->audio));
}



static void
shopno_qs_status_update_battery (ShopnoQsStatus *self)
{
  gboolean present = shopno_qs_battery_get_present (self->battery);

  gtk_widget_set_visible (self->battery_icon, present);
  if (present)
    shopno_qs_status_set_icon (self, self->battery_icon,
                               shopno_qs_battery_get_icon_name (self->battery));
}



/*
 * Volume notification, as xfce4-pulseaudio-plugin shows: xfce4-notifyd
 * draws the "value" hint as a level bar
 */

static void
shopno_qs_status_notified (GObject      *source,
                           GAsyncResult *result,
                           gpointer      data)
{
  GVariant *reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, NULL);

  /* No notification server, or the status is gone */
  if (reply == NULL)
    return;

  g_variant_get (reply, "(u)", &SHOPNO_QS_STATUS (data)->notification_id);
  g_variant_unref (reply);
}



static void
shopno_qs_status_notify_volume (ShopnoQsStatus *self,
                                gdouble         volume)
{
  GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  gboolean         muted = shopno_qs_audio_get_muted (self->audio);
  gint             percent = (gint) (volume * 100.0 + 0.5);
  GVariantBuilder  hints;
  gchar           *summary;

  if (bus == NULL)
    return;

  summary = muted ? g_strdup_printf (_("Volume %d%% (muted)"), percent)
                  : g_strdup_printf (_("Volume %d%%"), percent);

  g_variant_builder_init (&hints, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&hints, "{sv}", "value", g_variant_new_int32 (percent));
  /* Replace, rather than stack, while replies are still on their way */
  g_variant_builder_add (&hints, "{sv}", "x-canonical-private-synchronous", g_variant_new_string ("volume"));
  /* Kept out of the notification log */
  g_variant_builder_add (&hints, "{sv}", "transient", g_variant_new_boolean (TRUE));

  g_dbus_connection_call (bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                          "org.freedesktop.Notifications", "Notify",
                          g_variant_new ("(susssasa{sv}i)", _("Quick Settings"), self->notification_id,
                                         shopno_qs_audio_icon_name_for (percent / 100.0, muted),
                                         summary, "", NULL, &hints, VOLUME_NOTIFICATION_TIMEOUT),
                          G_VARIANT_TYPE ("(u)"), G_DBUS_CALL_FLAGS_NONE, -1,
                          self->cancellable, shopno_qs_status_notified, self);

  g_free (summary);
  g_object_unref (bus);
}



static void
shopno_qs_status_dispose (GObject *object)
{
  ShopnoQsStatus *self = SHOPNO_QS_STATUS (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->network != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->network, self);
      g_clear_object (&self->network);
    }

  if (self->audio != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->audio, self);
      g_clear_object (&self->audio);
    }

  if (self->battery != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->battery, self);
      g_clear_object (&self->battery);
    }

  G_OBJECT_CLASS (shopno_qs_status_parent_class)->dispose (object);
}



static void
shopno_qs_status_class_init (ShopnoQsStatusClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = shopno_qs_status_dispose;
}



static void
shopno_qs_status_init (ShopnoQsStatus *self)
{
  self->icon_size = 16;
  self->cancellable = g_cancellable_new ();
  gtk_box_set_spacing (GTK_BOX (self), 8);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-status");

  self->network_icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (self), self->network_icon, FALSE, FALSE, 0);
  self->volume_icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (self), self->volume_icon, FALSE, FALSE, 0);
  self->battery_icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (self), self->battery_icon, FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_widget_set_no_show_all (self->battery_icon, TRUE);

  self->network = shopno_qs_network_get_default ();
  self->audio = shopno_qs_audio_get_default ();
  self->battery = shopno_qs_battery_get_default ();

  g_signal_connect_swapped (self->network, "changed",
                            G_CALLBACK (shopno_qs_status_update_network), self);
  g_signal_connect_swapped (self->audio, "changed",
                            G_CALLBACK (shopno_qs_status_update_volume), self);
  g_signal_connect_swapped (self->battery, "changed",
                            G_CALLBACK (shopno_qs_status_update_battery), self);

  shopno_qs_status_update_network (self);
  shopno_qs_status_update_volume (self);
  shopno_qs_status_update_battery (self);
}



GtkWidget *
shopno_qs_status_new (void)
{
  return g_object_new (SHOPNO_TYPE_QS_STATUS, NULL);
}



void
shopno_qs_status_set_icon_size (ShopnoQsStatus *status,
                                gint            size)
{
  g_return_if_fail (SHOPNO_IS_QS_STATUS (status));

  status->icon_size = size;
  gtk_image_set_pixel_size (GTK_IMAGE (status->network_icon), size);
  gtk_image_set_pixel_size (GTK_IMAGE (status->volume_icon), size);
  gtk_image_set_pixel_size (GTK_IMAGE (status->battery_icon), size);
}



gboolean
shopno_qs_status_scroll (ShopnoQsStatus *status,
                         GdkEventScroll *event)
{
  gint steps = 0;

  g_return_val_if_fail (SHOPNO_IS_QS_STATUS (status), FALSE);

  if (!shopno_qs_audio_get_available (status->audio))
    return FALSE;

  switch (event->direction)
    {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_RIGHT:
      steps = 1;
      break;

    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_LEFT:
      steps = -1;
      break;

    default:
      break;
    }

  if (steps != 0)
    {
      gdouble volume = CLAMP (shopno_qs_audio_get_volume (status->audio) + steps * SCROLL_STEP, 0.0, 1.0);

      shopno_qs_audio_set_volume (status->audio, volume);
      shopno_qs_status_notify_volume (status, volume);
    }

  return TRUE;
}



gchar *
shopno_qs_status_get_tooltip (ShopnoQsStatus *status)
{
  GString     *tooltip = g_string_new (NULL);
  const gchar *ssid;
  gchar       *battery;

  g_return_val_if_fail (SHOPNO_IS_QS_STATUS (status), NULL);

  /* Network */
  ssid = shopno_qs_network_get_active_ssid (status->network);
  if (ssid != NULL)
    g_string_append_printf (tooltip, _("%s\nConnected"), ssid);
  else if (g_str_has_prefix (shopno_qs_network_get_icon_name (status->network), "network-wired")
           || g_str_has_prefix (shopno_qs_network_get_icon_name (status->network), "network-vpn"))
    g_string_append (tooltip, _("Network\nConnected"));
  else
    g_string_append (tooltip, _("Not connected"));

  /* Volume */
  g_string_append_c (tooltip, '\n');
  if (!shopno_qs_audio_get_available (status->audio))
    g_string_append (tooltip, _("No sound output"));
  else if (shopno_qs_audio_get_muted (status->audio))
    g_string_append (tooltip, _("Muted"));
  else
    g_string_append_printf (tooltip, _("Volume: %d%%"),
                            (gint) (shopno_qs_audio_get_volume (status->audio) * 100.0 + 0.5));

  /* Battery */
  if (shopno_qs_battery_get_present (status->battery))
    {
      battery = shopno_qs_battery_get_status (status->battery);
      g_string_append_printf (tooltip, _("\nBattery: %d%%, %s"),
                              (gint) (shopno_qs_battery_get_percentage (status->battery) + 0.5),
                              battery);
      g_free (battery);
    }

  return g_string_free (tooltip, FALSE);
}
