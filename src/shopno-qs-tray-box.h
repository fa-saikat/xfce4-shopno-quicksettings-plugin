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

#ifndef __SHOPNO_QS_TRAY_BOX_H__
#define __SHOPNO_QS_TRAY_BOX_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TRAY_BOX (shopno_qs_tray_box_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTrayBox, shopno_qs_tray_box, SHOPNO, QS_TRAY_BOX, GtkBox)

/* The tray icons on the panel, in the order the apps appeared */
GtkWidget *shopno_qs_tray_box_new           (void);

void       shopno_qs_tray_box_set_icon_size (ShopnoQsTrayBox *box,
                                             gint             size);

G_END_DECLS

#endif /* !__SHOPNO_QS_TRAY_BOX_H__ */
