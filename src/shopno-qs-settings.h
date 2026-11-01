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

#ifndef __SHOPNO_QS_SETTINGS_H__
#define __SHOPNO_QS_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* Night light color temperature in kelvin; 6500 K is neutral */
#define SHOPNO_QS_NIGHT_LIGHT_MIN     1500
#define SHOPNO_QS_NIGHT_LIGHT_MAX     6000
#define SHOPNO_QS_NIGHT_LIGHT_DEFAULT 4000

#define SHOPNO_TYPE_QS_SETTINGS (shopno_qs_settings_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsSettings, shopno_qs_settings, SHOPNO, QS_SETTINGS, GObject)

ShopnoQsSettings *shopno_qs_settings_new             (void);

gboolean          shopno_qs_settings_get_enable_blur (ShopnoQsSettings *settings);

gboolean          shopno_qs_settings_get_night_light (ShopnoQsSettings *settings);
void              shopno_qs_settings_set_night_light (ShopnoQsSettings *settings,
                                                      gboolean          night_light);
guint             shopno_qs_settings_get_night_light_temperature (ShopnoQsSettings *settings);

gboolean          shopno_qs_settings_is_tile_hidden  (ShopnoQsSettings *settings,
                                                      const gchar      *id);
void              shopno_qs_settings_set_tile_hidden (ShopnoQsSettings *settings,
                                                      const gchar      *id,
                                                      gboolean          hidden);

G_END_DECLS

#endif /* !__SHOPNO_QS_SETTINGS_H__ */
