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
 * Opens the flyout in a normal window, without the panel, so layout and
 * CSS changes can be tried without reinstalling the plugin. Settings are
 * not saved. Build with -Dpreview=true.
 */

#include "config.h"

#include <gtk/gtk.h>

#include "shopno-qs-window.h"

static gboolean opt_blur = FALSE;
static gboolean opt_no_grab = FALSE;

static const GOptionEntry entries[] =
{
  { "blur", 0, 0, G_OPTION_ARG_NONE, &opt_blur, "Use the blur style", NULL },
  { "no-grab", 0, 0, G_OPTION_ARG_NONE, &opt_no_grab, "Do not grab the keyboard and pointer", NULL },
  { NULL }
};



int
main (int    argc,
      char **argv)
{
  ShopnoQsSettings *settings;
  GtkWidget        *window;
  GError           *error = NULL;

  if (!gtk_init_with_args (&argc, &argv, NULL, entries, NULL, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      return 1;
    }

  settings = shopno_qs_settings_new ();
  g_object_set (settings, "enable-blur", opt_blur, NULL);

  window = shopno_qs_window_new (settings);
  shopno_qs_window_set_grab_input (SHOPNO_QS_WINDOW (window), !opt_no_grab);
  g_signal_connect (window, "hide", G_CALLBACK (gtk_main_quit), NULL);

  shopno_qs_window_popup (SHOPNO_QS_WINDOW (window), 100, 100);
  gtk_main ();

  gtk_widget_destroy (window);
  g_object_unref (settings);

  return 0;
}
