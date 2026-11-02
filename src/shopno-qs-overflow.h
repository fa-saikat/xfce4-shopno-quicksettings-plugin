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

#ifndef __SHOPNO_QS_OVERFLOW_H__
#define __SHOPNO_QS_OVERFLOW_H__

#include <gtk/gtk.h>

#include "shopno-qs-settings.h"

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_OVERFLOW (shopno_qs_overflow_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsOverflow, shopno_qs_overflow, SHOPNO, QS_OVERFLOW, GtkWindow)

/* The flyout of tray icons not shown on the panel, opened by the ^ button
 * as on Windows 11. Emits "changed" when it gains or loses icons. */
GtkWidget *shopno_qs_overflow_new        (ShopnoQsSettings *settings);

guint      shopno_qs_overflow_get_n_icons (ShopnoQsOverflow *overflow);

void       shopno_qs_overflow_popup      (ShopnoQsOverflow *overflow,
                                          gint              x,
                                          gint              y);

G_END_DECLS

#endif /* !__SHOPNO_QS_OVERFLOW_H__ */
