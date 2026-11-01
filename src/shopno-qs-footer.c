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
 * The footer band: battery level on the left, settings on the right, as
 * in the Windows 11 quick settings.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "shopno-qs-battery.h"
#include "shopno-qs-features.h"
#include "shopno-qs-util.h"

typedef struct
{
  ShopnoQsWindow  *window;
  ShopnoQsBattery *battery;
  GtkWidget       *button;
  GtkWidget       *icon;
  GtkWidget       *label;
} Footer;

static const gchar * const power_commands[] =
{
  "xfce4-power-manager-settings",
  NULL
};

static const gchar * const settings_commands[] =
{
  "xfce4-settings-manager",
  NULL
};



static void
footer_free (gpointer data)
{
  Footer *footer = data;

  g_signal_handlers_disconnect_by_data (footer->battery, footer);
  g_object_unref (footer->battery);
  g_free (footer);
}



static void
footer_update_battery (Footer *footer)
{
  gchar *text;
  gchar *status;
  gchar *tooltip;

  if (!shopno_qs_battery_get_present (footer->battery))
    {
      gtk_widget_hide (footer->button);
      return;
    }

  gtk_image_set_from_icon_name (GTK_IMAGE (footer->icon),
                                shopno_qs_battery_get_icon_name (footer->battery),
                                GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (footer->icon), 16);

  text = g_strdup_printf ("%d%%", (gint) (shopno_qs_battery_get_percentage (footer->battery) + 0.5));
  gtk_label_set_text (GTK_LABEL (footer->label), text);

  status = shopno_qs_battery_get_status (footer->battery);
  tooltip = g_strdup_printf ("%s\n%s", text, status);
  gtk_widget_set_tooltip_text (footer->button, tooltip);

  gtk_widget_show (footer->button);

  g_free (tooltip);
  g_free (status);
  g_free (text);
}



static void
footer_launch_power (Footer *footer)
{
  shopno_qs_launch (GTK_WIDGET (footer->window), power_commands);
}



static void
footer_launch_settings (GtkWidget *button,
                        Footer    *footer)
{
  shopno_qs_launch (GTK_WIDGET (footer->window), settings_commands);
}



void
shopno_qs_footer_attach (ShopnoQsWindow *window)
{
  Footer    *footer = g_new0 (Footer, 1);
  GtkWidget *box;
  GtkWidget *button;
  gchar     *command;

  footer->window = window;
  footer->battery = shopno_qs_battery_get_default ();
  g_object_set_data_full (G_OBJECT (window), "shopno-qs-footer", footer, footer_free);

  /* Battery: icon and percentage; opens the power settings */
  footer->button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (footer->button), GTK_RELIEF_NONE);
  gtk_widget_set_valign (footer->button, GTK_ALIGN_CENTER);
  gtk_style_context_add_class (gtk_widget_get_style_context (footer->button), "shopno-qs-battery");
  g_signal_connect_swapped (footer->button, "clicked", G_CALLBACK (footer_launch_power), footer);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (footer->button), box);
  footer->icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), footer->icon, FALSE, FALSE, 0);
  footer->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), footer->label, FALSE, FALSE, 0);
  gtk_widget_show_all (box);

  /* Without the power manager the button still shows the level */
  command = shopno_qs_find_command (power_commands);
  if (command == NULL)
    gtk_style_context_add_class (gtk_widget_get_style_context (footer->button), "static");
  g_free (command);

  gtk_widget_set_no_show_all (footer->button, TRUE);
  shopno_qs_window_add_footer (window, footer->button, FALSE);

  /* Settings */
  command = shopno_qs_find_command (settings_commands);
  if (command != NULL)
    {
      button = gtk_button_new_from_icon_name ("preferences-system-symbolic", GTK_ICON_SIZE_BUTTON);
      gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_widget_set_tooltip_text (button, _("All settings"));
      g_signal_connect (button, "clicked", G_CALLBACK (footer_launch_settings), footer);
      gtk_widget_show (button);
      shopno_qs_window_add_footer (window, button, TRUE);
      g_free (command);
    }

  g_signal_connect_swapped (footer->battery, "changed", G_CALLBACK (footer_update_battery), footer);
  footer_update_battery (footer);
}
