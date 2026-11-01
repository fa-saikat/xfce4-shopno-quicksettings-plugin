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

#include "shopno-qs-dialog.h"
#include "shopno-qs-status.h"
#include "shopno-qs-window.h"

static gboolean opt_blur = FALSE;
static gboolean opt_no_grab = FALSE;
static gboolean opt_status = FALSE;
static gboolean opt_dialog = FALSE;
static gchar  **opt_hidden = NULL;

static const GOptionEntry entries[] =
{
  { "blur", 0, 0, G_OPTION_ARG_NONE, &opt_blur, "Use the blur style", NULL },
  { "no-grab", 0, 0, G_OPTION_ARG_NONE, &opt_no_grab, "Do not grab the keyboard and pointer", NULL },
  { "dialog", 0, 0, G_OPTION_ARG_NONE, &opt_dialog, "Also open the properties dialog", NULL },
  { "hide", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_hidden, "Hide a tile by id (repeatable)", "ID" },
  { "status", 0, 0, G_OPTION_ARG_NONE, &opt_status, "Also show the panel button's status icons", NULL },
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
  for (guint i = 0; opt_hidden != NULL && opt_hidden[i] != NULL; i++)
    shopno_qs_settings_set_tile_hidden (settings, opt_hidden[i], TRUE);

  window = shopno_qs_window_new (settings);
  shopno_qs_window_set_grab_input (SHOPNO_QS_WINDOW (window), !opt_no_grab);
  g_signal_connect (window, "hide", G_CALLBACK (gtk_main_quit), NULL);

  if (opt_status)
    {
      GtkWidget *status_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      GtkWidget *status = shopno_qs_status_new ();

      gtk_window_set_title (GTK_WINDOW (status_window), "Status icons");
      gtk_container_set_border_width (GTK_CONTAINER (status_window), 8);
      gtk_container_add (GTK_CONTAINER (status_window), status);
      gtk_window_move (GTK_WINDOW (status_window), 520, 100);
      gtk_widget_add_events (status_window, GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
      g_signal_connect_swapped (status_window, "scroll-event",
                                G_CALLBACK (shopno_qs_status_scroll), status);
      gtk_widget_show (status_window);
    }

  if (opt_dialog)
    {
      GtkWidget *dialog = shopno_qs_dialog_new (NULL, settings);

      gtk_window_move (GTK_WINDOW (dialog), 480, 60);
      gtk_widget_show (dialog);
    }

  /* As above a bottom panel: the bottom edge stays at y = 840 */
  shopno_qs_window_popup (SHOPNO_QS_WINDOW (window), 100, 840, TRUE);
  gtk_main ();

  gtk_widget_destroy (window);
  g_object_unref (settings);
  g_strfreev (opt_hidden);

  return 0;
}
