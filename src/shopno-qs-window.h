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

/* With grow_up, y is the bottom edge, for a panel below the flyout: the
 * flyout then grows upward. Otherwise y is the top edge. */
void       shopno_qs_window_popup          (ShopnoQsWindow   *window,
                                            gint              x,
                                            gint              y,
                                            gboolean          grow_up);

void       shopno_qs_window_set_grab_input (ShopnoQsWindow   *window,
                                            gboolean          grab_input);

ShopnoQsSettings *shopno_qs_window_get_settings (ShopnoQsWindow *window);

/* Content, filled in by the feature modules */
/* The id names the tile in the hidden-tiles setting */
void       shopno_qs_window_add_tile       (ShopnoQsWindow   *window,
                                            const gchar      *id,
                                            GtkWidget        *tile);

/* For hardware that comes and goes, such as an Ethernet port on a dock:
 * an absent tile is not shown, whatever the hidden-tiles setting says */
void       shopno_qs_window_set_tile_present (ShopnoQsWindow *window,
                                              GtkWidget      *tile,
                                              gboolean        present);

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
