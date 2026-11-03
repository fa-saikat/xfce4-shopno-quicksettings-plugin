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

#ifndef __SHOPNO_QS_TRAY_SOCKET_H__
#define __SHOPNO_QS_TRAY_SOCKET_H__

#include <gtk/gtkx.h>

#include "shopno-qs-tray-item.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TRAY_SOCKET (shopno_qs_tray_socket_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTraySocket, shopno_qs_tray_socket, SHOPNO, QS_TRAY_SOCKET, GtkSocket)

/* Holds the icon window of an XEmbed tray item, drawn and clicked by the
 * app itself. NULL when the window is gone. Embed it with
 * gtk_socket_add_id() once it is in a toplevel window. */
GtkWidget        *shopno_qs_tray_socket_new      (ShopnoQsTrayItem   *item);

ShopnoQsTrayItem *shopno_qs_tray_socket_get_item (ShopnoQsTraySocket *socket);

/* Square, whatever size the app asks for */
void              shopno_qs_tray_socket_set_size (ShopnoQsTraySocket *socket,
                                                  gint                size);

/* After gtk_widget_reparent() to another window */
void              shopno_qs_tray_socket_moved    (ShopnoQsTraySocket *socket);

G_END_DECLS

#endif /* !__SHOPNO_QS_TRAY_SOCKET_H__ */
