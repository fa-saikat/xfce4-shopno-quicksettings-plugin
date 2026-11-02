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

#ifndef __SHOPNO_QS_AUDIO_H__
#define __SHOPNO_QS_AUDIO_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_AUDIO (shopno_qs_audio_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsAudio, shopno_qs_audio, SHOPNO, QS_AUDIO, GObject)

typedef struct
{
  gchar    *name;
  gchar    *description;
  gchar    *icon_name;
  gboolean  is_default;
} ShopnoQsAudioSink;

/* Shared by the flyout and the panel button. Returns a new reference. */
ShopnoQsAudio *shopno_qs_audio_get_default      (void);

gboolean       shopno_qs_audio_get_available    (ShopnoQsAudio *audio);

/* 0.0 to 1.0, where 1.0 is 100% */
gdouble        shopno_qs_audio_get_volume       (ShopnoQsAudio *audio);
void           shopno_qs_audio_set_volume       (ShopnoQsAudio *audio,
                                                 gdouble        volume);

gboolean       shopno_qs_audio_get_muted        (ShopnoQsAudio *audio);
void           shopno_qs_audio_set_muted        (ShopnoQsAudio *audio,
                                                 gboolean       muted);

/* Output devices, as ShopnoQsAudioSink, owned by the backend */
GPtrArray     *shopno_qs_audio_get_sinks        (ShopnoQsAudio *audio);
void           shopno_qs_audio_set_default_sink (ShopnoQsAudio *audio,
                                                 const gchar   *name);

/* An icon name for the current volume and mute state */
const gchar   *shopno_qs_audio_get_icon_name    (ShopnoQsAudio *audio);

/* The same for a given volume, 0.0 to 1.0 */
const gchar   *shopno_qs_audio_icon_name_for    (gdouble        volume,
                                                 gboolean       muted);

G_END_DECLS

#endif /* !__SHOPNO_QS_AUDIO_H__ */
