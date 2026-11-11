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
 * DDC/CI over the kernel's i2c-dev interface, with no helper program or
 * library. Monitors answer at I2C address 0x37 on the buses of the
 * graphics card; their EDID sits at 0x50 on the same bus.
 *
 * Only buses whose parent device is a display controller (PCI class 0x03)
 * are opened, so motherboard SMBus chips such as RAM SPD EEPROMs are never
 * touched. Monitors are slow: a reply is ready 40 ms after a request, and
 * the next command must wait 50 ms, so all of this runs on one worker
 * thread and the main thread only exchanges a few flags with it.
 *
 * Some monitors ignore a Set VCP now and then, without an error. After a
 * drag the worker reads the level back and sends it again if needed.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/i2c-dev.h>

#include "shopno-qs-ddc.h"

#define I2C_DEV_DIR    "/sys/class/i2c-dev"

#define EDID_ADDR      0x50
#define EDID_SIZE      128
#define DDC_ADDR       0x37

/* The checksum of a request starts from the monitor's address as a write
 * (0x37 << 1), that of a reply from the host's virtual address */
#define DDC_DEST       0x6e
#define DDC_HOST       0x51
#define DDC_REPLY_SUM  0x50

#define VCP_GET        0x01
#define VCP_GET_REPLY  0x02
#define VCP_SET        0x03
#define VCP_BRIGHTNESS 0x10

#define REPLY_DELAY    (40 * G_TIME_SPAN_MILLISECOND)
#define COMMAND_GAP    (50 * G_TIME_SPAN_MILLISECOND)
#define GET_TRIES      3

/* Monitors drop some of the commands of a fast drag, even with the gap.
 * Once the slider rests, the level is read back and sent again. */
#define SETTLE_DELAY   (150 * G_TIME_SPAN_MILLISECOND)
#define SETTLE_TRIES   3



typedef struct
{
  gint    fd;
  guint   max;
  guint   level;
  gint64  ready_at;
  guint8  edid[EDID_SIZE];
} Monitor;

struct _ShopnoQsDdc
{
  gint             ref_count;
  GMainContext    *context;

  /* Main thread only */
  ShopnoQsDdcFunc  func;
  gpointer         data;

  /* Shared with the worker, under the lock. serial counts the writes, so
   * a read that started before the last one is not shown. */
  GMutex           lock;
  GCond            cond;
  gboolean         quit;
  gboolean         want_scan;
  gboolean         want_read;
  gdouble          pending;    /* -1 when nothing is waiting */
  guint            serial;

  /* Worker only */
  GThread         *thread;
  GPtrArray       *monitors;
};

typedef struct
{
  ShopnoQsDdc *ddc;
  guint        n_monitors;
  gdouble      level;
  guint        serial;
} Result;



/*
 * The protocol
 */

static void
monitor_free (gpointer data)
{
  Monitor *monitor = data;

  if (monitor->fd >= 0)
    close (monitor->fd);
  g_free (monitor);
}



static void
monitor_wait (Monitor *monitor)
{
  gint64 now = g_get_monotonic_time ();

  if (monitor->ready_at > now)
    g_usleep (monitor->ready_at - now);
}



static gboolean
monitor_send (Monitor      *monitor,
              guint8       *message,
              gsize         length)
{
  guint8 sum = DDC_DEST;
  gsize  i;

  /* The last byte is the checksum */
  for (i = 0; i < length - 1; i++)
    sum ^= message[i];
  message[length - 1] = sum;

  monitor_wait (monitor);
  if (write (monitor->fd, message, length) != (gssize) length)
    return FALSE;

  monitor->ready_at = g_get_monotonic_time () + COMMAND_GAP;

  return TRUE;
}



static gboolean
monitor_get_brightness (Monitor *monitor)
{
  gint tries;

  for (tries = 0; tries < GET_TRIES; tries++)
    {
      guint8 request[] = { DDC_HOST, 0x82, VCP_GET, VCP_BRIGHTNESS, 0 };
      guint8 reply[11];
      guint8 sum = DDC_REPLY_SUM;
      gint   i;

      /* Nobody acknowledged: no monitor, or it is asleep */
      if (!monitor_send (monitor, request, sizeof (request)))
        return FALSE;

      g_usleep (REPLY_DELAY);
      if (read (monitor->fd, reply, sizeof (reply)) != sizeof (reply))
        continue;
      monitor->ready_at = g_get_monotonic_time () + COMMAND_GAP;

      for (i = 0; i < 10; i++)
        sum ^= reply[i];

      /* A busy monitor sends a null message; try again */
      if (reply[0] != DDC_DEST || reply[1] != 0x88 || reply[2] != VCP_GET_REPLY
          || reply[4] != VCP_BRIGHTNESS || reply[10] != sum)
        continue;

      /* The monitor does not know brightness */
      if (reply[3] != 0)
        return FALSE;

      monitor->max = (reply[6] << 8) | reply[7];
      monitor->level = (reply[8] << 8) | reply[9];

      return monitor->max > 0;
    }

  return FALSE;
}



static guint
monitor_value (Monitor *monitor,
               gdouble  level)
{
  return (guint) (level * monitor->max + 0.5);
}



static void
monitor_set_brightness (Monitor *monitor,
                        gdouble  level)
{
  guint  value = monitor_value (monitor, level);
  guint8 request[] = { DDC_HOST, 0x84, VCP_SET, VCP_BRIGHTNESS, value >> 8, value & 0xff, 0 };

  if (monitor_send (monitor, request, sizeof (request)))
    monitor->level = value;
}



/* Sends the level again to the monitors that dropped it. TRUE when all of
 * them have it, or do not answer. */
static gboolean
ddc_verify (ShopnoQsDdc *ddc,
            gdouble      level)
{
  gboolean settled = TRUE;
  guint    i;

  for (i = 0; i < ddc->monitors->len; i++)
    {
      Monitor *monitor = ddc->monitors->pdata[i];

      if (monitor_get_brightness (monitor) && monitor->level != monitor_value (monitor, level))
        {
          monitor_set_brightness (monitor, level);
          settled = FALSE;
        }
    }

  return settled;
}



/*
 * Finding monitors
 */

/* Walks up from the I2C adapter to the first device with a PCI class */
static gboolean
ddc_is_display_bus (const gchar *name)
{
  gchar    *link = g_build_filename (I2C_DEV_DIR, name, "device", NULL);
  gchar     path[PATH_MAX];
  gboolean  display = FALSE;

  if (realpath (link, path) != NULL)
    {
      gchar *dir = g_strdup (path);

      while (g_str_has_prefix (dir, "/sys/devices/"))
        {
          gchar *file = g_build_filename (dir, "class", NULL);
          gchar *class = NULL;
          gchar *parent;

          if (g_file_get_contents (file, &class, NULL, NULL))
            {
              display = g_str_has_prefix (class, "0x03");
              g_free (class);
              g_free (file);
              break;
            }
          g_free (file);

          parent = g_path_get_dirname (dir);
          g_free (dir);
          dir = parent;
        }

      g_free (dir);
    }

  g_free (link);

  return display;
}



static gboolean
monitor_read_edid (Monitor *monitor)
{
  static const guint8 header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
  guint8              offset = 0;

  /* A kernel driver may have claimed the EEPROM; reading it is harmless */
  if (ioctl (monitor->fd, I2C_SLAVE, EDID_ADDR) < 0
      && ioctl (monitor->fd, I2C_SLAVE_FORCE, EDID_ADDR) < 0)
    return FALSE;

  return write (monitor->fd, &offset, 1) == 1
         && read (monitor->fd, monitor->edid, EDID_SIZE) == EDID_SIZE
         && memcmp (monitor->edid, header, sizeof (header)) == 0;
}



static Monitor *
monitor_open (const gchar *name,
              gboolean    *denied)
{
  gchar   *device = g_build_filename ("/dev", name, NULL);
  Monitor *monitor = g_new0 (Monitor, 1);

  const gchar *failure = NULL;

  monitor->fd = open (device, O_RDWR | O_CLOEXEC);
  if (monitor->fd < 0 && (errno == EACCES || errno == EPERM))
    *denied = TRUE;
  g_free (device);

  /* No EDID: nothing is plugged into this connector, or the driver
   * cannot reach it right now */
  if (monitor->fd < 0)
    failure = g_strerror (errno);
  else if (!monitor_read_edid (monitor))
    failure = "no EDID";
  else if (ioctl (monitor->fd, I2C_SLAVE, DDC_ADDR) < 0)
    failure = g_strerror (errno);
  else if (!monitor_get_brightness (monitor))
    failure = "no DDC/CI answer";

  if (failure != NULL)
    {
      g_debug ("DDC/CI: %s: %s", name, failure);
      monitor_free (monitor);
      return NULL;
    }

  g_debug ("DDC/CI: %s: brightness %u of %u", name, monitor->level, monitor->max);

  return monitor;
}



static gboolean
ddc_quitting (ShopnoQsDdc *ddc)
{
  gboolean quit;

  g_mutex_lock (&ddc->lock);
  quit = ddc->quit;
  g_mutex_unlock (&ddc->lock);

  return quit;
}



static void
ddc_scan (ShopnoQsDdc *ddc)
{
  static gboolean  warned = FALSE;
  GPtrArray       *found = g_ptr_array_new_with_free_func (monitor_free);
  GDir            *dir;
  const gchar     *name;
  gboolean         denied = FALSE;

  /* Close the old buses first, so a monitor is not opened twice */
  g_ptr_array_set_size (ddc->monitors, 0);

  dir = g_dir_open (I2C_DEV_DIR, 0, NULL);
  while (dir != NULL && (name = g_dir_read_name (dir)) != NULL && !ddc_quitting (ddc))
    {
      Monitor *monitor;
      guint    i;

      if (!ddc_is_display_bus (name) || (monitor = monitor_open (name, &denied)) == NULL)
        continue;

      /* Some cards reach one monitor through two buses */
      for (i = 0; i < found->len; i++)
        if (memcmp (((Monitor *) found->pdata[i])->edid, monitor->edid, EDID_SIZE) == 0)
          break;

      if (i < found->len)
        monitor_free (monitor);
      else
        g_ptr_array_add (found, monitor);
    }

  if (dir != NULL)
    g_dir_close (dir);

  if (found->len == 0 && denied && !warned)
    {
      g_message ("No access to the I2C buses of the graphics card, so monitor "
                 "brightness cannot be changed. Install the udev rule that "
                 "comes with the plugin, or add the user to the i2c group.");
      warned = TRUE;
    }

  g_ptr_array_unref (ddc->monitors);
  ddc->monitors = found;
}



/*
 * The worker thread
 */

static ShopnoQsDdc *
ddc_ref (ShopnoQsDdc *ddc)
{
  g_atomic_int_inc (&ddc->ref_count);

  return ddc;
}



static void
ddc_unref (ShopnoQsDdc *ddc)
{
  if (!g_atomic_int_dec_and_test (&ddc->ref_count))
    return;

  g_ptr_array_unref (ddc->monitors);
  g_mutex_clear (&ddc->lock);
  g_cond_clear (&ddc->cond);
  g_main_context_unref (ddc->context);
  g_free (ddc);
}



static gboolean
ddc_result_dispatch (gpointer data)
{
  Result      *result = data;
  ShopnoQsDdc *ddc = result->ddc;

  /* Freed, or the user moved the slider since the read began */
  if (ddc->func != NULL)
    ddc->func (result->n_monitors,
               result->serial == ddc->serial ? result->level : -1.0,
               ddc->data);

  return G_SOURCE_REMOVE;
}



static void
ddc_result_free (gpointer data)
{
  Result *result = data;

  ddc_unref (result->ddc);
  g_free (result);
}



static void
ddc_post (ShopnoQsDdc *ddc,
          guint        serial)
{
  Result  *result = g_new0 (Result, 1);
  GSource *source;
  Monitor *first = ddc->monitors->len > 0 ? ddc->monitors->pdata[0] : NULL;

  result->ddc = ddc_ref (ddc);
  result->n_monitors = ddc->monitors->len;
  result->level = first != NULL ? (gdouble) first->level / first->max : -1.0;
  result->serial = serial;

  /* Always on the main thread: g_main_context_invoke() would run it here
   * when the main loop is not running */
  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_callback (source, ddc_result_dispatch, result, ddc_result_free);
  g_source_attach (source, ddc->context);
  g_source_unref (source);
}



static gpointer
ddc_worker (gpointer data)
{
  ShopnoQsDdc *ddc = data;
  gdouble      written = -1.0;
  gint64       settle_at = 0;    /* 0 when the last write is checked */
  gint         settle_tries = 0;

  for (;;)
    {
      gboolean scan, fetch, settle = FALSE;
      gdouble  level = -1.0;
      guint    serial, i;

      g_mutex_lock (&ddc->lock);
      while (!ddc->quit && !ddc->want_scan && !ddc->want_read && ddc->pending < 0)
        {
          if (settle_at == 0)
            g_cond_wait (&ddc->cond, &ddc->lock);
          else if (!g_cond_wait_until (&ddc->cond, &ddc->lock, settle_at))
            {
              settle = TRUE;
              break;
            }
        }

      if (ddc->quit)
        {
          g_mutex_unlock (&ddc->lock);
          break;
        }

      if (settle)
        {
          g_mutex_unlock (&ddc->lock);

          settle_at = 0;
          if (!ddc_verify (ddc, written) && ++settle_tries < SETTLE_TRIES)
            settle_at = g_get_monotonic_time () + SETTLE_DELAY;
          continue;
        }

      /* A scan reads the level too; a write goes before a read */
      scan = ddc->want_scan;
      if (!scan)
        {
          level = ddc->pending;
          ddc->pending = -1.0;
        }
      fetch = scan || (level < 0 && ddc->want_read);
      if (fetch)
        ddc->want_read = FALSE;
      ddc->want_scan = FALSE;
      serial = ddc->serial;
      g_mutex_unlock (&ddc->lock);

      if (scan)
        {
          ddc_scan (ddc);
          settle_at = 0;
        }
      else if (level >= 0)
        {
          for (i = 0; i < ddc->monitors->len; i++)
            monitor_set_brightness (ddc->monitors->pdata[i], level);

          written = level;
          settle_tries = 0;
          settle_at = g_get_monotonic_time () + SETTLE_DELAY;
          continue;
        }
      else if (settle_at != 0)
        {
          /* Check the last write first, so the level shown is the one
           * the monitors end up with */
          settle_at = 0;
          if (!ddc_verify (ddc, written))
            {
              settle_tries = 1;
              settle_at = g_get_monotonic_time () + SETTLE_DELAY;
            }
        }
      else if (ddc->monitors->len > 0)
        {
          /* Keep the last level when the monitor does not answer */
          monitor_get_brightness (ddc->monitors->pdata[0]);
        }

      ddc_post (ddc, serial);
    }

  return NULL;
}



/*
 * Public API, main thread
 */

ShopnoQsDdc *
shopno_qs_ddc_new (ShopnoQsDdcFunc func,
                   gpointer        data)
{
  ShopnoQsDdc *ddc = g_new0 (ShopnoQsDdc, 1);

  ddc->ref_count = 1;
  ddc->context = g_main_context_ref_thread_default ();
  ddc->func = func;
  ddc->data = data;
  ddc->pending = -1.0;
  ddc->want_scan = TRUE;
  ddc->monitors = g_ptr_array_new_with_free_func (monitor_free);
  g_mutex_init (&ddc->lock);
  g_cond_init (&ddc->cond);

  ddc->thread = g_thread_new ("shopno-qs-ddc", ddc_worker, ddc);

  return ddc;
}



void
shopno_qs_ddc_rescan (ShopnoQsDdc *ddc)
{
  g_mutex_lock (&ddc->lock);
  ddc->want_scan = TRUE;
  g_cond_signal (&ddc->cond);
  g_mutex_unlock (&ddc->lock);
}



void
shopno_qs_ddc_refresh (ShopnoQsDdc *ddc)
{
  g_mutex_lock (&ddc->lock);
  ddc->want_read = TRUE;
  g_cond_signal (&ddc->cond);
  g_mutex_unlock (&ddc->lock);
}



void
shopno_qs_ddc_set (ShopnoQsDdc *ddc,
                   gdouble      level)
{
  g_mutex_lock (&ddc->lock);
  ddc->pending = CLAMP (level, 0.0, 1.0);
  ddc->serial++;
  g_cond_signal (&ddc->cond);
  g_mutex_unlock (&ddc->lock);
}



void
shopno_qs_ddc_free (ShopnoQsDdc *ddc)
{
  ddc->func = NULL;

  /* The worker stops between buses, so this waits at most one command */
  g_mutex_lock (&ddc->lock);
  ddc->quit = TRUE;
  g_cond_signal (&ddc->cond);
  g_mutex_unlock (&ddc->lock);
  g_thread_join (ddc->thread);

  ddc_unref (ddc);
}
