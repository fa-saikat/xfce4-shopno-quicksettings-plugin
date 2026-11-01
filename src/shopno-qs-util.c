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

#include <libxfce4ui/libxfce4ui.h>

#include "shopno-qs-util.h"

#define COMMANDS_KEY "shopno-qs-commands"



gchar *
shopno_qs_find_command (const gchar * const *commands)
{
  for (guint i = 0; commands != NULL && commands[i] != NULL; i++)
    {
      gchar **argv = NULL;
      gchar  *path;

      if (!g_shell_parse_argv (commands[i], NULL, &argv, NULL))
        continue;

      path = g_find_program_in_path (argv[0]);
      g_strfreev (argv);

      if (path != NULL)
        {
          g_free (path);
          return g_strdup (commands[i]);
        }
    }

  return NULL;
}



gboolean
shopno_qs_launch (GtkWidget           *window,
                  const gchar * const *commands)
{
  gchar  *command = shopno_qs_find_command (commands);
  GError *error = NULL;

  if (command == NULL)
    return FALSE;

  gtk_widget_hide (window);

  if (!xfce_spawn_command_line (gtk_widget_get_screen (window), command,
                                FALSE, TRUE, FALSE, &error))
    {
      g_warning ("Failed to run \"%s\": %s", command, error->message);
      g_error_free (error);
    }

  g_free (command);

  return TRUE;
}



static void
shopno_qs_link_clicked (GtkWidget *button,
                        GtkWidget *window)
{
  shopno_qs_launch (window, g_object_get_data (G_OBJECT (button), COMMANDS_KEY));
}



GtkWidget *
shopno_qs_link_new (GtkWidget           *window,
                    const gchar         *label,
                    const gchar * const *commands)
{
  GtkWidget *button;
  GtkWidget *text;
  gchar     *command = shopno_qs_find_command (commands);

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "shopno-qs-page-link");

  text = gtk_label_new (label);
  gtk_label_set_xalign (GTK_LABEL (text), 0.0);
  gtk_container_add (GTK_CONTAINER (button), text);

  /* The commands are static strings owned by the caller */
  g_object_set_data (G_OBJECT (button), COMMANDS_KEY, (gpointer) commands);
  g_signal_connect (button, "clicked", G_CALLBACK (shopno_qs_link_clicked), window);

  gtk_widget_show_all (button);
  gtk_widget_set_no_show_all (button, TRUE);
  gtk_widget_set_visible (button, command != NULL);
  g_free (command);

  return button;
}



GtkWidget *
shopno_qs_row_new (const gchar *icon_name,
                   const gchar *title,
                   const gchar *subtitle,
                   GtkWidget   *end)
{
  GtkWidget *box;
  GtkWidget *text;
  GtkWidget *label;
  GtkWidget *image;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 16);
  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);

  text = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_valign (text, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), text, TRUE, TRUE, 0);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (text), label, FALSE, FALSE, 0);

  if (subtitle != NULL && *subtitle != '\0')
    {
      label = gtk_label_new (subtitle);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_style_context_add_class (gtk_widget_get_style_context (label), "shopno-qs-row-subtitle");
      gtk_box_pack_start (GTK_BOX (text), label, FALSE, FALSE, 0);
    }

  if (end != NULL)
    {
      gtk_widget_set_valign (end, GTK_ALIGN_CENTER);
      gtk_box_pack_end (GTK_BOX (box), end, FALSE, FALSE, 0);
    }

  gtk_widget_show_all (box);

  return box;
}
