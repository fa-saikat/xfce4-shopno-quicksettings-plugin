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

#ifndef __SHOPNO_QS_WINDOW_H__
#define __SHOPNO_QS_WINDOW_H__

#include <gtk/gtk.h>

#include "shopno-qs-settings.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_WINDOW (shopno_qs_window_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsWindow, shopno_qs_window, SHOPNO, QS_WINDOW, GtkWindow)

GtkWidget *shopno_qs_window_new            (ShopnoQsSettings *settings);

void       shopno_qs_window_popup          (ShopnoQsWindow   *window,
                                            gint              x,
                                            gint              y);

void       shopno_qs_window_set_grab_input (ShopnoQsWindow   *window,
                                            gboolean          grab_input);

ShopnoQsSettings *shopno_qs_window_get_settings (ShopnoQsWindow *window);

/* Content, filled in by the feature modules */
void       shopno_qs_window_add_tile       (ShopnoQsWindow   *window,
                                            GtkWidget        *tile);

void       shopno_qs_window_add_slider     (ShopnoQsWindow   *window,
                                            GtkWidget        *slider);

void       shopno_qs_window_add_footer     (ShopnoQsWindow   *window,
                                            GtkWidget        *widget,
                                            gboolean          end);

/* Detail pages, opened by the chevron of a split tile. The content sits
 * in a scrolled area under a header with a back button and the title. */
void       shopno_qs_window_add_page       (ShopnoQsWindow   *window,
                                            const gchar      *name,
                                            const gchar      *title,
                                            GtkWidget        *content);

void       shopno_qs_window_show_page      (ShopnoQsWindow   *window,
                                            const gchar      *name);

void       shopno_qs_window_show_main      (ShopnoQsWindow   *window);

G_END_DECLS

#endif /* !__SHOPNO_QS_WINDOW_H__ */
