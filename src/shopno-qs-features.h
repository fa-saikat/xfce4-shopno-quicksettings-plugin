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

#ifndef __SHOPNO_QS_FEATURES_H__
#define __SHOPNO_QS_FEATURES_H__

#include "shopno-qs-window.h"

G_BEGIN_DECLS

/* Each feature adds its tiles, sliders, pages and footer items to the
 * window, and lives as long as the window */
void shopno_qs_wifi_attach   (ShopnoQsWindow *window);
void shopno_qs_volume_attach (ShopnoQsWindow *window);

/* Adds every feature, in the order they appear on Windows 11 */
void shopno_qs_features_attach (ShopnoQsWindow *window);

G_END_DECLS

#endif /* !__SHOPNO_QS_FEATURES_H__ */
