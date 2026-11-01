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

#include "shopno-qs-settings.h"

enum
{
  PROP_0,
  PROP_ENABLE_BLUR,
  PROP_NIGHT_LIGHT,
  PROP_NIGHT_LIGHT_TEMPERATURE,
  N_PROPERTIES
};

struct _ShopnoQsSettings
{
  GObject  __parent__;

  gboolean enable_blur;
  gboolean night_light;
  guint    night_light_temperature;
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (ShopnoQsSettings, shopno_qs_settings, G_TYPE_OBJECT)



static void
shopno_qs_settings_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ShopnoQsSettings *self = SHOPNO_QS_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_ENABLE_BLUR:
      g_value_set_boolean (value, self->enable_blur);
      break;

    case PROP_NIGHT_LIGHT:
      g_value_set_boolean (value, self->night_light);
      break;

    case PROP_NIGHT_LIGHT_TEMPERATURE:
      g_value_set_uint (value, self->night_light_temperature);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_qs_settings_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ShopnoQsSettings *self = SHOPNO_QS_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_ENABLE_BLUR:
      if (self->enable_blur != g_value_get_boolean (value))
        {
          self->enable_blur = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_NIGHT_LIGHT:
      if (self->night_light != g_value_get_boolean (value))
        {
          self->night_light = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_NIGHT_LIGHT_TEMPERATURE:
      if (self->night_light_temperature != g_value_get_uint (value))
        {
          self->night_light_temperature = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_qs_settings_class_init (ShopnoQsSettingsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shopno_qs_settings_get_property;
  gobject_class->set_property = shopno_qs_settings_set_property;

  properties[PROP_ENABLE_BLUR] =
    g_param_spec_boolean ("enable-blur", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NIGHT_LIGHT] =
    g_param_spec_boolean ("night-light", NULL, NULL, FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_NIGHT_LIGHT_TEMPERATURE] =
    g_param_spec_uint ("night-light-temperature", NULL, NULL,
                       SHOPNO_QS_NIGHT_LIGHT_MIN, SHOPNO_QS_NIGHT_LIGHT_MAX,
                       SHOPNO_QS_NIGHT_LIGHT_DEFAULT,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_qs_settings_init (ShopnoQsSettings *self)
{
  self->night_light_temperature = SHOPNO_QS_NIGHT_LIGHT_DEFAULT;
}



ShopnoQsSettings *
shopno_qs_settings_new (void)
{
  return g_object_new (SHOPNO_TYPE_QS_SETTINGS, NULL);
}



gboolean
shopno_qs_settings_get_enable_blur (ShopnoQsSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), FALSE);

  return settings->enable_blur;
}



gboolean
shopno_qs_settings_get_night_light (ShopnoQsSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), FALSE);

  return settings->night_light;
}



void
shopno_qs_settings_set_night_light (ShopnoQsSettings *settings,
                                    gboolean          night_light)
{
  g_return_if_fail (SHOPNO_IS_QS_SETTINGS (settings));

  g_object_set (settings, "night-light", night_light, NULL);
}



guint
shopno_qs_settings_get_night_light_temperature (ShopnoQsSettings *settings)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), SHOPNO_QS_NIGHT_LIGHT_DEFAULT);

  return settings->night_light_temperature;
}
