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

#ifndef __SHOPNO_QS_TRAY_ITEM_H__
#define __SHOPNO_QS_TRAY_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TRAY_ITEM (shopno_qs_tray_item_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTrayItem, shopno_qs_tray_item, SHOPNO, QS_TRAY_ITEM, GObject)

/* key is "bus-name/object/path", as the watcher lists it */
ShopnoQsTrayItem *shopno_qs_tray_item_new             (const gchar      *key,
                                                       const gchar      *bus_name,
                                                       const gchar      *object_path);

const gchar      *shopno_qs_tray_item_get_key         (ShopnoQsTrayItem *item);

/* The app's id, stable across restarts; NULL until the item is ready */
const gchar      *shopno_qs_tray_item_get_id          (ShopnoQsTrayItem *item);

/* Ready and not Passive: worth showing */
gboolean          shopno_qs_tray_item_get_visible     (ShopnoQsTrayItem *item);

/* The icon to show, the attention icon while the app asks for attention:
 * a name (with the app's own icon directory, or NULL) or a pixbuf. Any of
 * them may be NULL. Owned by the item. */
void              shopno_qs_tray_item_get_icon        (ShopnoQsTrayItem *item,
                                                       const gchar     **icon_name,
                                                       const gchar     **theme_path,
                                                       GdkPixbuf       **pixbuf);

/* Title and tooltip for the hover text; free with g_free() */
gchar            *shopno_qs_tray_item_get_tooltip     (ShopnoQsTrayItem *item);
const gchar      *shopno_qs_tray_item_get_title       (ShopnoQsTrayItem *item);

/* The app's menu, built from its DBusMenu object and kept up to date by
 * libdbusmenu, or NULL when it has none. Owned by the item. */
GtkWidget        *shopno_qs_tray_item_get_menu        (ShopnoQsTrayItem *item);

/* The app's DBusMenu object, or NULL */
const gchar      *shopno_qs_tray_item_get_menu_path   (ShopnoQsTrayItem *item);
gboolean          shopno_qs_tray_item_get_is_menu     (ShopnoQsTrayItem *item);

/* Root coordinates, as the spec asks for */
void              shopno_qs_tray_item_activate        (ShopnoQsTrayItem *item,
                                                       gint              x,
                                                       gint              y);
void              shopno_qs_tray_item_secondary_activate (ShopnoQsTrayItem *item,
                                                       gint              x,
                                                       gint              y);
void              shopno_qs_tray_item_context_menu    (ShopnoQsTrayItem *item,
                                                       gint              x,
                                                       gint              y);
void              shopno_qs_tray_item_scroll          (ShopnoQsTrayItem *item,
                                                       gint              delta,
                                                       GtkOrientation    orientation);

G_END_DECLS

#endif /* !__SHOPNO_QS_TRAY_ITEM_H__ */
