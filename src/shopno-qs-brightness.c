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
 * Brightness slider for laptop screens.
 *
 * The level is read from /sys/class/backlight and written through
 * systemd-logind's Session.SetBrightness, which lets the user of the
 * active session change it without a root helper. The slider is hidden
 * when there is no backlight, as on Windows for desktop PCs.
 *
 * sysfs does not notify changes, so while the flyout is open the level is
 * read again twice a second to follow the brightness keys.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "shopno-qs-features.h"
#include "shopno-qs-slider.h"

#define BACKLIGHT_DIR "/sys/class/backlight"
#define POLL_INTERVAL 500

/* Never go fully dark: some panels turn off at 0 */
#define MIN_FRACTION 0.01

typedef struct
{
  GtkWidget       *slider;
  GCancellable    *cancellable;
  GDBusConnection *bus;

  gchar        *name;
  gchar        *path;
  guint         max;

  guint         poll_id;
  gboolean      writing;
  gint          pending;    /* -1 when nothing is waiting */
} Brightness;



/* Firmware interfaces know the panel best, raw ones the least */
static gint
brightness_type_rank (const gchar *type)
{
  if (g_strcmp0 (type, "firmware") == 0)
    return 3;
  if (g_strcmp0 (type, "platform") == 0)
    return 2;
  if (g_strcmp0 (type, "raw") == 0)
    return 1;

  return 0;
}



static gchar *
brightness_read_file (const gchar *dir,
                      const gchar *file)
{
  gchar *path = g_build_filename (dir, file, NULL);
  gchar *contents = NULL;

  g_file_get_contents (path, &contents, NULL, NULL);
  g_free (path);

  return contents != NULL ? g_strstrip (contents) : NULL;
}



static gboolean
brightness_find_device (Brightness *brightness)
{
  GDir        *dir = g_dir_open (BACKLIGHT_DIR, 0, NULL);
  const gchar *name;
  gint         best_rank = -1;

  if (dir == NULL)
    return FALSE;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      gchar *path = g_build_filename (BACKLIGHT_DIR, name, NULL);
      gchar *type = brightness_read_file (path, "type");
      gchar *max = brightness_read_file (path, "max_brightness");
      gint   rank = brightness_type_rank (type);

      if (max != NULL && g_ascii_strtoull (max, NULL, 10) > 0 && rank > best_rank)
        {
          best_rank = rank;
          g_free (brightness->name);
          g_free (brightness->path);
          brightness->name = g_strdup (name);
          brightness->path = g_steal_pointer (&path);
          brightness->max = g_ascii_strtoull (max, NULL, 10);
        }

      g_free (path);
      g_free (type);
      g_free (max);
    }

  g_dir_close (dir);

  return brightness->name != NULL;
}



static gint
brightness_read (Brightness *brightness)
{
  gchar *value = brightness_read_file (brightness->path, "brightness");
  gint   level = value != NULL ? (gint) g_ascii_strtoll (value, NULL, 10) : -1;

  g_free (value);

  return level;
}



static void
brightness_update (Brightness *brightness)
{
  ShopnoQsSlider *slider = SHOPNO_QS_SLIDER (brightness->slider);
  gint            level = brightness_read (brightness);
  gchar          *tooltip;

  /* Do not move the knob under the user's hand or while a change is on
   * its way to logind */
  if (level < 0 || shopno_qs_slider_is_dragging (slider)
      || brightness->writing || brightness->pending >= 0)
    return;

  shopno_qs_slider_set_value (slider, (gdouble) level / brightness->max);

  tooltip = g_strdup_printf (_("Brightness %d%%"), (gint) (100.0 * level / brightness->max + 0.5));
  gtk_widget_set_tooltip_text (brightness->slider, tooltip);
  g_free (tooltip);
}



/*
 * Writing through logind. One call at a time; values that come in while
 * one is running replace each other, so a fast drag sends only a few.
 */

static void brightness_write_next (Brightness *brightness);



static void
brightness_written (GObject      *source,
                    GAsyncResult *result,
                    gpointer      data)
{
  GVariant   *reply;
  GError     *error = NULL;
  Brightness *brightness;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (reply == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }

      g_warning ("Failed to set the brightness: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_variant_unref (reply);
    }

  brightness = data;
  brightness->writing = FALSE;
  brightness_write_next (brightness);
}



static void
brightness_write_next (Brightness *brightness)
{
  if (brightness->writing || brightness->pending < 0 || brightness->bus == NULL)
    return;

  brightness->writing = TRUE;
  g_dbus_connection_call (brightness->bus, "org.freedesktop.login1",
                          "/org/freedesktop/login1/session/auto",
                          "org.freedesktop.login1.Session", "SetBrightness",
                          g_variant_new ("(ssu)", "backlight", brightness->name,
                                         (guint32) brightness->pending),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, brightness->cancellable,
                          brightness_written, brightness);
  brightness->pending = -1;
}



static void
brightness_value_changed (ShopnoQsSlider *slider,
                          gdouble         value,
                          Brightness     *brightness)
{
  gchar *tooltip;

  value = MAX (value, MIN_FRACTION);
  brightness->pending = MAX (1, (gint) (value * brightness->max + 0.5));
  brightness_write_next (brightness);

  tooltip = g_strdup_printf (_("Brightness %d%%"), (gint) (value * 100.0 + 0.5));
  gtk_widget_set_tooltip_text (brightness->slider, tooltip);
  g_free (tooltip);
}



/*
 * Following the brightness keys while the flyout is open
 */

static gboolean
brightness_poll (gpointer data)
{
  brightness_update (data);
  return G_SOURCE_CONTINUE;
}



static void
brightness_window_mapped (Brightness *brightness)
{
  brightness_update (brightness);
  if (brightness->poll_id == 0)
    brightness->poll_id = g_timeout_add (POLL_INTERVAL, brightness_poll, brightness);
}



static void
brightness_window_unmapped (Brightness *brightness)
{
  if (brightness->poll_id != 0)
    {
      g_source_remove (brightness->poll_id);
      brightness->poll_id = 0;
    }
}



static void
brightness_free (gpointer data)
{
  Brightness *brightness = data;

  brightness_window_unmapped (brightness);
  g_cancellable_cancel (brightness->cancellable);
  g_object_unref (brightness->cancellable);
  g_clear_object (&brightness->bus);
  g_free (brightness->name);
  g_free (brightness->path);
  g_free (brightness);
}



void
shopno_qs_brightness_attach (ShopnoQsWindow *window)
{
  Brightness *brightness = g_new0 (Brightness, 1);

  brightness->pending = -1;
  brightness->cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-brightness", brightness, brightness_free);

  /* No backlight: a desktop PC with external monitors */
  if (!brightness_find_device (brightness))
    return;

  brightness->bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  brightness->slider = shopno_qs_slider_new ("display-brightness-symbolic", NULL, FALSE);
  g_signal_connect (brightness->slider, "value-changed", G_CALLBACK (brightness_value_changed), brightness);
  shopno_qs_window_add_slider (window, brightness->slider);

  g_signal_connect_swapped (window, "map", G_CALLBACK (brightness_window_mapped), brightness);
  g_signal_connect_swapped (window, "unmap", G_CALLBACK (brightness_window_unmapped), brightness);
  brightness_update (brightness);
}
