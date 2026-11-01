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

#ifndef __SHOPNO_QS_STATUS_H__
#define __SHOPNO_QS_STATUS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_STATUS (shopno_qs_status_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsStatus, shopno_qs_status, SHOPNO, QS_STATUS, GtkBox)

/* Network, volume and battery icons, as on the Windows 11 taskbar */
GtkWidget *shopno_qs_status_new           (void);

void       shopno_qs_status_set_icon_size (ShopnoQsStatus *status,
                                           gint            size);

/* Changes the volume for a scroll on the panel button */
gboolean   shopno_qs_status_scroll        (ShopnoQsStatus *status,
                                           GdkEventScroll *event);

/* One line per icon, for the button's tooltip */
gchar     *shopno_qs_status_get_tooltip   (ShopnoQsStatus *status);

G_END_DECLS

#endif /* !__SHOPNO_QS_STATUS_H__ */
