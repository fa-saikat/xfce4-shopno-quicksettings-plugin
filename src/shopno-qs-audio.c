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
 * Volume of the default output through libpulse. PipeWire serves the same
 * API through pipewire-pulse, so this covers both sound servers.
 */

#include "config.h"

#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>

#include "shopno-qs-audio.h"

/* Wait before reconnecting after the sound server went away */
#define RECONNECT_DELAY 2

enum
{
  CHANGED,
  SINKS_CHANGED,
  N_SIGNALS
};

struct _ShopnoQsAudio
{
  GObject           __parent__;

  pa_glib_mainloop *loop;
  pa_context       *context;
  guint             reconnect_id;

  gboolean          available;
  gchar            *default_sink;
  guint32           sink_index;
  pa_cvolume        cvolume;
  gboolean          muted;

  GPtrArray        *sinks;
  GPtrArray        *pending_sinks;
};

static guint          signals[N_SIGNALS];
static ShopnoQsAudio *default_audio = NULL;

G_DEFINE_TYPE (ShopnoQsAudio, shopno_qs_audio, G_TYPE_OBJECT)

static void shopno_qs_audio_connect (ShopnoQsAudio *self);



static void
shopno_qs_audio_sink_free (gpointer data)
{
  ShopnoQsAudioSink *sink = data;

  g_free (sink->name);
  g_free (sink->description);
  g_free (sink->icon_name);
  g_free (sink);
}



static void
shopno_qs_audio_set_available (ShopnoQsAudio *self,
                               gboolean       available)
{
  if (self->available == available)
    return;

  self->available = available;
  if (!available)
    {
      self->sink_index = PA_INVALID_INDEX;
      g_ptr_array_set_size (self->sinks, 0);
      g_signal_emit (self, signals[SINKS_CHANGED], 0);
    }

  g_signal_emit (self, signals[CHANGED], 0);
}



/*
 * Reading the server state
 */

static void
shopno_qs_audio_sink_info (pa_context         *context,
                           const pa_sink_info *info,
                           int                 eol,
                           void               *data)
{
  ShopnoQsAudio *self = data;

  if (eol != 0 || info == NULL)
    return;

  self->sink_index = info->index;
  self->cvolume = info->volume;
  self->muted = info->mute != 0;
  self->available = TRUE;

  g_signal_emit (self, signals[CHANGED], 0);
}



static void
shopno_qs_audio_sink_list_info (pa_context         *context,
                                const pa_sink_info *info,
                                int                 eol,
                                void               *data)
{
  ShopnoQsAudio     *self = data;
  ShopnoQsAudioSink *sink;
  const gchar       *form_factor;
  const gchar       *icon_name;

  if (eol != 0)
    {
      /* The list is complete, replace the old one */
      if (self->pending_sinks != NULL)
        {
          g_ptr_array_unref (self->sinks);
          self->sinks = g_steal_pointer (&self->pending_sinks);
          g_signal_emit (self, signals[SINKS_CHANGED], 0);
        }
      return;
    }

  if (self->pending_sinks == NULL)
    self->pending_sinks = g_ptr_array_new_with_free_func (shopno_qs_audio_sink_free);

  form_factor = pa_proplist_gets (info->proplist, PA_PROP_DEVICE_FORM_FACTOR);
  if (g_strcmp0 (form_factor, "headphone") == 0 || g_strcmp0 (form_factor, "headset") == 0)
    icon_name = "audio-headphones-symbolic";
  else
    icon_name = "audio-speakers-symbolic";

  sink = g_new0 (ShopnoQsAudioSink, 1);
  sink->name = g_strdup (info->name);
  sink->description = g_strdup (info->description != NULL ? info->description : info->name);
  sink->icon_name = g_strdup (icon_name);
  sink->is_default = g_strcmp0 (info->name, self->default_sink) == 0;
  g_ptr_array_add (self->pending_sinks, sink);
}



static void
shopno_qs_audio_refresh_sinks (ShopnoQsAudio *self)
{
  /* A list still being read is dropped and read again */
  g_clear_pointer (&self->pending_sinks, g_ptr_array_unref);
  pa_operation_unref (pa_context_get_sink_info_list (self->context, shopno_qs_audio_sink_list_info, self));
}



static void
shopno_qs_audio_server_info (pa_context           *context,
                             const pa_server_info *info,
                             void                 *data)
{
  ShopnoQsAudio *self = data;

  if (info == NULL)
    return;

  g_free (self->default_sink);
  self->default_sink = g_strdup (info->default_sink_name);

  if (self->default_sink != NULL)
    pa_operation_unref (pa_context_get_sink_info_by_name (context, self->default_sink,
                                                          shopno_qs_audio_sink_info, self));
  else
    shopno_qs_audio_set_available (self, FALSE);

  shopno_qs_audio_refresh_sinks (self);
}



static void
shopno_qs_audio_refresh_server (ShopnoQsAudio *self)
{
  pa_operation_unref (pa_context_get_server_info (self->context, shopno_qs_audio_server_info, self));
}



static void
shopno_qs_audio_subscribe (pa_context                   *context,
                           pa_subscription_event_type_t  type,
                           uint32_t                      index,
                           void                         *data)
{
  ShopnoQsAudio *self = data;
  guint          facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
  guint          kind = type & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

  if (facility == PA_SUBSCRIPTION_EVENT_SERVER)
    {
      /* The default output may have changed */
      shopno_qs_audio_refresh_server (self);
    }
  else if (facility == PA_SUBSCRIPTION_EVENT_SINK)
    {
      if (kind == PA_SUBSCRIPTION_EVENT_CHANGE && index == self->sink_index)
        pa_operation_unref (pa_context_get_sink_info_by_index (context, index,
                                                               shopno_qs_audio_sink_info, self));
      else if (kind != PA_SUBSCRIPTION_EVENT_CHANGE)
        shopno_qs_audio_refresh_server (self);
    }
}



/*
 * Connection
 */

static gboolean
shopno_qs_audio_reconnect (gpointer data)
{
  ShopnoQsAudio *self = data;

  self->reconnect_id = 0;
  shopno_qs_audio_connect (self);

  return G_SOURCE_REMOVE;
}



static void
shopno_qs_audio_disconnect (ShopnoQsAudio *self)
{
  if (self->context == NULL)
    return;

  pa_context_set_state_callback (self->context, NULL, NULL);
  pa_context_set_subscribe_callback (self->context, NULL, NULL);
  pa_context_disconnect (self->context);
  pa_context_unref (self->context);
  self->context = NULL;
}



static void
shopno_qs_audio_state (pa_context *context,
                       void       *data)
{
  ShopnoQsAudio *self = data;

  switch (pa_context_get_state (context))
    {
    case PA_CONTEXT_READY:
      pa_context_set_subscribe_callback (context, shopno_qs_audio_subscribe, self);
      pa_operation_unref (pa_context_subscribe (context,
                                                PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER,
                                                NULL, NULL));
      shopno_qs_audio_refresh_server (self);
      break;

    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      shopno_qs_audio_disconnect (self);
      shopno_qs_audio_set_available (self, FALSE);
      if (self->reconnect_id == 0)
        self->reconnect_id = g_timeout_add_seconds (RECONNECT_DELAY, shopno_qs_audio_reconnect, self);
      break;

    default:
      break;
    }
}



static void
shopno_qs_audio_connect (ShopnoQsAudio *self)
{
  pa_proplist *props;

  props = pa_proplist_new ();
  pa_proplist_sets (props, PA_PROP_APPLICATION_NAME, "Shopno Quick Settings");
  pa_proplist_sets (props, PA_PROP_APPLICATION_ID, "org.shopnoos.quicksettings");
  pa_proplist_sets (props, PA_PROP_APPLICATION_ICON_NAME, "multimedia-volume-control");

  self->context = pa_context_new_with_proplist (pa_glib_mainloop_get_api (self->loop), NULL, props);
  pa_proplist_free (props);

  pa_context_set_state_callback (self->context, shopno_qs_audio_state, self);

  /* NOFAIL waits for a sound server that is not running yet */
  if (pa_context_connect (self->context, NULL, PA_CONTEXT_NOFAIL, NULL) < 0)
    {
      g_warning ("Failed to connect to the sound server: %s",
                 pa_strerror (pa_context_errno (self->context)));
      shopno_qs_audio_disconnect (self);
      self->reconnect_id = g_timeout_add_seconds (RECONNECT_DELAY, shopno_qs_audio_reconnect, self);
    }
}



/*
 * GObject
 */

static void
shopno_qs_audio_finalize (GObject *object)
{
  ShopnoQsAudio *self = SHOPNO_QS_AUDIO (object);

  if (self->reconnect_id != 0)
    g_source_remove (self->reconnect_id);

  shopno_qs_audio_disconnect (self);
  pa_glib_mainloop_free (self->loop);

  g_free (self->default_sink);
  g_ptr_array_unref (self->sinks);
  if (self->pending_sinks != NULL)
    g_ptr_array_unref (self->pending_sinks);

  G_OBJECT_CLASS (shopno_qs_audio_parent_class)->finalize (object);
}



static void
shopno_qs_audio_class_init (ShopnoQsAudioClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shopno_qs_audio_finalize;

  /* Volume, mute or availability changed */
  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals[SINKS_CHANGED] =
    g_signal_new ("sinks-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_audio_init (ShopnoQsAudio *self)
{
  self->sink_index = PA_INVALID_INDEX;
  pa_cvolume_init (&self->cvolume);
  self->sinks = g_ptr_array_new_with_free_func (shopno_qs_audio_sink_free);
  self->loop = pa_glib_mainloop_new (NULL);

  shopno_qs_audio_connect (self);
}



/*
 * Public API
 */

ShopnoQsAudio *
shopno_qs_audio_get_default (void)
{
  if (default_audio != NULL)
    return g_object_ref (default_audio);

  default_audio = g_object_new (SHOPNO_TYPE_QS_AUDIO, NULL);
  g_object_add_weak_pointer (G_OBJECT (default_audio), (gpointer *) &default_audio);

  return default_audio;
}



gboolean
shopno_qs_audio_get_available (ShopnoQsAudio *audio)
{
  g_return_val_if_fail (SHOPNO_IS_QS_AUDIO (audio), FALSE);

  return audio->available;
}



gdouble
shopno_qs_audio_get_volume (ShopnoQsAudio *audio)
{
  g_return_val_if_fail (SHOPNO_IS_QS_AUDIO (audio), 0.0);

  if (!pa_cvolume_valid (&audio->cvolume))
    return 0.0;

  return (gdouble) pa_cvolume_max (&audio->cvolume) / PA_VOLUME_NORM;
}



void
shopno_qs_audio_set_volume (ShopnoQsAudio *audio,
                            gdouble        volume)
{
  pa_volume_t level;

  g_return_if_fail (SHOPNO_IS_QS_AUDIO (audio));

  if (audio->context == NULL || audio->sink_index == PA_INVALID_INDEX
      || !pa_cvolume_valid (&audio->cvolume))
    return;

  level = (pa_volume_t) (CLAMP (volume, 0.0, 1.0) * PA_VOLUME_NORM);

  /* Scaling keeps the balance between the channels */
  if (pa_cvolume_max (&audio->cvolume) == PA_VOLUME_MUTED)
    pa_cvolume_set (&audio->cvolume, audio->cvolume.channels, level);
  else
    pa_cvolume_scale (&audio->cvolume, level);

  pa_operation_unref (pa_context_set_sink_volume_by_index (audio->context, audio->sink_index,
                                                           &audio->cvolume, NULL, NULL));

  /* Raising the volume unmutes, as on Windows */
  if (audio->muted && level > PA_VOLUME_MUTED)
    shopno_qs_audio_set_muted (audio, FALSE);
}



gboolean
shopno_qs_audio_get_muted (ShopnoQsAudio *audio)
{
  g_return_val_if_fail (SHOPNO_IS_QS_AUDIO (audio), FALSE);

  return audio->muted;
}



void
shopno_qs_audio_set_muted (ShopnoQsAudio *audio,
                           gboolean       muted)
{
  g_return_if_fail (SHOPNO_IS_QS_AUDIO (audio));

  if (audio->context == NULL || audio->sink_index == PA_INVALID_INDEX)
    return;

  pa_operation_unref (pa_context_set_sink_mute_by_index (audio->context, audio->sink_index,
                                                         muted, NULL, NULL));
}



GPtrArray *
shopno_qs_audio_get_sinks (ShopnoQsAudio *audio)
{
  g_return_val_if_fail (SHOPNO_IS_QS_AUDIO (audio), NULL);

  return audio->sinks;
}



void
shopno_qs_audio_set_default_sink (ShopnoQsAudio *audio,
                                  const gchar   *name)
{
  g_return_if_fail (SHOPNO_IS_QS_AUDIO (audio));

  if (audio->context == NULL)
    return;

  pa_operation_unref (pa_context_set_default_sink (audio->context, name, NULL, NULL));
}



const gchar *
shopno_qs_audio_get_icon_name (ShopnoQsAudio *audio)
{
  g_return_val_if_fail (SHOPNO_IS_QS_AUDIO (audio), NULL);

  return shopno_qs_audio_icon_name_for (shopno_qs_audio_get_volume (audio),
                                        !audio->available || audio->muted);
}



const gchar *
shopno_qs_audio_icon_name_for (gdouble  volume,
                               gboolean muted)
{
  if (muted || volume <= 0.0)
    return "audio-volume-muted-symbolic";
  if (volume < 0.34)
    return "audio-volume-low-symbolic";
  if (volume < 0.67)
    return "audio-volume-medium-symbolic";

  return "audio-volume-high-symbolic";
}
