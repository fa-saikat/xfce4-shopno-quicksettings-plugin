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
  N_PROPERTIES
};

struct _ShopnoQsSettings
{
  GObject  __parent__;

  gboolean enable_blur;
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

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_qs_settings_init (ShopnoQsSettings *self)
{
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
