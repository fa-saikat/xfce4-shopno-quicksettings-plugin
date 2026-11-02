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

#ifndef __SHOPNO_QS_TRAY_ICON_H__
#define __SHOPNO_QS_TRAY_ICON_H__

#include <gtk/gtk.h>

#include "shopno-qs-tray-item.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TRAY_ICON (shopno_qs_tray_icon_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTrayIcon, shopno_qs_tray_icon, SHOPNO, QS_TRAY_ICON, GtkButton)

/* A tray app's icon: click to activate, middle click for its secondary
 * action, right click for its menu, scroll to scroll it. Emits
 * "activated" just before any click reaches the app. */
GtkWidget        *shopno_qs_tray_icon_new           (ShopnoQsTrayItem *item);

ShopnoQsTrayItem *shopno_qs_tray_icon_get_item      (ShopnoQsTrayIcon *icon);

void              shopno_qs_tray_icon_set_icon_size (ShopnoQsTrayIcon *icon,
                                                     gint              size);

G_END_DECLS

#endif /* !__SHOPNO_QS_TRAY_ICON_H__ */
