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

#ifndef __SHOPNO_QS_TRAY_H__
#define __SHOPNO_QS_TRAY_H__

#include "shopno-qs-tray-item.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TRAY (shopno_qs_tray_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTray, shopno_qs_tray, SHOPNO, QS_TRAY, GObject)

/* Shared by every panel button. Returns a new reference. Signals:
 * "item-added" and "item-removed", with the ShopnoQsTrayItem. */
ShopnoQsTray *shopno_qs_tray_get_default (void);

/* The current items, in the order they appeared; owned by the tray */
GPtrArray    *shopno_qs_tray_get_items   (ShopnoQsTray *tray);

G_END_DECLS

#endif /* !__SHOPNO_QS_TRAY_H__ */
