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

#include "shopno-qs-window.h"

/* Width of the flyout at 100% scale, as on Windows 11 */
#define WINDOW_WIDTH 360

enum
{
  PROP_0,
  PROP_SETTINGS,
  N_PROPERTIES
};

struct _ShopnoQsWindow
{
  GtkWindow         __parent__;

  ShopnoQsSettings *settings;
  gboolean          grab_input;

  GtkWidget        *frame;
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (ShopnoQsWindow, shopno_qs_window, GTK_TYPE_WINDOW)



static void
shopno_qs_window_load_css (GdkScreen *screen)
{
  static gboolean  loaded = FALSE;
  GtkCssProvider  *provider;

  if (loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/shopnoos/quicksettings/shopno-quicksettings.css");
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  loaded = TRUE;
}



static void
shopno_qs_window_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_qs_window_constructed (GObject *object)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  G_OBJECT_CLASS (shopno_qs_window_parent_class)->constructed (object);

  if (self->settings == NULL)
    self->settings = shopno_qs_settings_new ();
}



static void
shopno_qs_window_dispose (GObject *object)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  if (self->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->settings, self);
      g_clear_object (&self->settings);
    }

  G_OBJECT_CLASS (shopno_qs_window_parent_class)->dispose (object);
}



static void
shopno_qs_window_class_init (ShopnoQsWindowClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = shopno_qs_window_set_property;
  gobject_class->constructed = shopno_qs_window_constructed;
  gobject_class->dispose = shopno_qs_window_dispose;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL, SHOPNO_TYPE_QS_SETTINGS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_qs_window_init (ShopnoQsWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);

  self->grab_input = TRUE;

  shopno_qs_window_load_css (gtk_widget_get_screen (GTK_WIDGET (self)));

  gtk_window_set_title (window, _("Quick Settings"));
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-window");

  self->frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (self->frame, WINDOW_WIDTH, -1);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-qs-frame");
  gtk_container_add (GTK_CONTAINER (self), self->frame);

  gtk_widget_show_all (self->frame);
}



GtkWidget *
shopno_qs_window_new (ShopnoQsSettings *settings)
{
  return g_object_new (SHOPNO_TYPE_QS_WINDOW, "settings", settings, NULL);
}



void
shopno_qs_window_popup (ShopnoQsWindow *window,
                        gint            x,
                        gint            y)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
}



/* Disabling the grab is only meant for the preview tool, so that a flyout
 * opened on a developer's desktop does not capture all input */
void
shopno_qs_window_set_grab_input (ShopnoQsWindow *window,
                                 gboolean        grab_input)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  window->grab_input = grab_input;
}
