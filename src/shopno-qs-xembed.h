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

#ifndef __SHOPNO_QS_XEMBED_H__
#define __SHOPNO_QS_XEMBED_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_XEMBED (shopno_qs_xembed_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsXembed, shopno_qs_xembed, SHOPNO, QS_XEMBED, GObject)

/* The XEmbed system tray of older apps: owns the _NET_SYSTEM_TRAY_S<n>
 * selection unless another tray does, and emits "dock" with the X window
 * of each icon that asks to be embedded. X11 only: NULL elsewhere. */
ShopnoQsXembed *shopno_qs_xembed_new      (void);

/* FALSE while another program, such as Xfce's System Tray plugin, owns
 * the selection */
gboolean        shopno_qs_xembed_get_owns (ShopnoQsXembed *xembed);

G_END_DECLS

#endif /* !__SHOPNO_QS_XEMBED_H__ */
