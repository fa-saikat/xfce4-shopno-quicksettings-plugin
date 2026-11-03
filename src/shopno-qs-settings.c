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
  PROP_HIDDEN_TILES,
  PROP_TRAY_PINNED,
  PROP_TRAY_HIDDEN,
  N_PROPERTIES
};

struct _ShopnoQsSettings
{
  GObject  __parent__;

  gboolean enable_blur;
  gboolean night_light;
  guint    night_light_temperature;
  gchar  **hidden_tiles;
  gchar  **tray_pinned;
  gchar  **tray_hidden;
};

/* Tray apps that show what the plugin shows already */
static const gchar * const default_tray_hidden[] =
{
  "nm-applet",
  NULL
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

    case PROP_HIDDEN_TILES:
      g_value_set_boxed (value, self->hidden_tiles);
      break;

    case PROP_TRAY_PINNED:
      g_value_set_boxed (value, self->tray_pinned);
      break;

    case PROP_TRAY_HIDDEN:
      g_value_set_boxed (value, self->tray_hidden);
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
  gchar           **old;

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

    case PROP_HIDDEN_TILES:
      /* The new list may be made of the old one's strings */
      old = self->hidden_tiles;
      self->hidden_tiles = g_value_dup_boxed (value);
      g_strfreev (old);
      if (self->hidden_tiles == NULL)
        self->hidden_tiles = g_new0 (gchar *, 1);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_TRAY_PINNED:
      /* The new list may be made of the old one's strings */
      old = self->tray_pinned;
      self->tray_pinned = g_value_dup_boxed (value);
      g_strfreev (old);
      if (self->tray_pinned == NULL)
        self->tray_pinned = g_new0 (gchar *, 1);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_TRAY_HIDDEN:
      old = self->tray_hidden;
      self->tray_hidden = g_value_dup_boxed (value);
      g_strfreev (old);
      if (self->tray_hidden == NULL)
        self->tray_hidden = g_new0 (gchar *, 1);
      g_object_notify_by_pspec (object, pspec);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_qs_settings_finalize (GObject *object)
{
  g_strfreev (SHOPNO_QS_SETTINGS (object)->hidden_tiles);
  g_strfreev (SHOPNO_QS_SETTINGS (object)->tray_pinned);
  g_strfreev (SHOPNO_QS_SETTINGS (object)->tray_hidden);

  G_OBJECT_CLASS (shopno_qs_settings_parent_class)->finalize (object);
}



static void
shopno_qs_settings_class_init (ShopnoQsSettingsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shopno_qs_settings_get_property;
  gobject_class->set_property = shopno_qs_settings_set_property;
  gobject_class->finalize = shopno_qs_settings_finalize;

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

  properties[PROP_HIDDEN_TILES] =
    g_param_spec_boxed ("hidden-tiles", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TRAY_PINNED] =
    g_param_spec_boxed ("tray-pinned", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /* Saved once changed; until then the default list applies */
  properties[PROP_TRAY_HIDDEN] =
    g_param_spec_boxed ("tray-hidden", NULL, NULL, G_TYPE_STRV,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_qs_settings_init (ShopnoQsSettings *self)
{
  self->night_light_temperature = SHOPNO_QS_NIGHT_LIGHT_DEFAULT;
  self->hidden_tiles = g_new0 (gchar *, 1);
  self->tray_pinned = g_new0 (gchar *, 1);
  self->tray_hidden = g_strdupv ((gchar **) default_tray_hidden);
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



gboolean
shopno_qs_settings_is_tile_hidden (ShopnoQsSettings *settings,
                                   const gchar      *id)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), FALSE);

  return id != NULL && g_strv_contains ((const gchar * const *) settings->hidden_tiles, id);
}



/* Adds or removes one id in a string list property */
static void
shopno_qs_settings_set_in_list (ShopnoQsSettings  *settings,
                                const gchar       *property,
                                gchar            **current,
                                const gchar       *id,
                                gboolean           present)
{
  GPtrArray *list;

  if (g_strv_contains ((const gchar * const *) current, id) == present)
    return;

  list = g_ptr_array_new ();
  for (guint i = 0; current[i] != NULL; i++)
    if (g_strcmp0 (current[i], id) != 0)
      g_ptr_array_add (list, current[i]);
  if (present)
    g_ptr_array_add (list, (gpointer) id);
  g_ptr_array_add (list, NULL);

  g_object_set (settings, property, list->pdata, NULL);
  g_ptr_array_free (list, TRUE);
}



void
shopno_qs_settings_set_tile_hidden (ShopnoQsSettings *settings,
                                    const gchar      *id,
                                    gboolean          hidden)
{
  g_return_if_fail (SHOPNO_IS_QS_SETTINGS (settings));
  g_return_if_fail (id != NULL);

  shopno_qs_settings_set_in_list (settings, "hidden-tiles", settings->hidden_tiles, id, hidden);
}



gboolean
shopno_qs_settings_is_tray_pinned (ShopnoQsSettings *settings,
                                   const gchar      *id)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), FALSE);

  return id != NULL && g_strv_contains ((const gchar * const *) settings->tray_pinned, id);
}



void
shopno_qs_settings_set_tray_pinned (ShopnoQsSettings *settings,
                                    const gchar      *id,
                                    gboolean          pinned)
{
  g_return_if_fail (SHOPNO_IS_QS_SETTINGS (settings));
  g_return_if_fail (id != NULL);

  shopno_qs_settings_set_in_list (settings, "tray-pinned", settings->tray_pinned, id, pinned);
}



gboolean
shopno_qs_settings_is_tray_hidden (ShopnoQsSettings *settings,
                                   const gchar      *id)
{
  g_return_val_if_fail (SHOPNO_IS_QS_SETTINGS (settings), FALSE);

  return id != NULL && g_strv_contains ((const gchar * const *) settings->tray_hidden, id);
}



void
shopno_qs_settings_set_tray_hidden (ShopnoQsSettings *settings,
                                    const gchar      *id,
                                    gboolean          hidden)
{
  g_return_if_fail (SHOPNO_IS_QS_SETTINGS (settings));
  g_return_if_fail (id != NULL);

  shopno_qs_settings_set_in_list (settings, "tray-hidden", settings->tray_hidden, id, hidden);
}
