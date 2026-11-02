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

#ifndef __SHOPNO_QS_TILE_H__
#define __SHOPNO_QS_TILE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_TILE (shopno_qs_tile_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsTile, shopno_qs_tile, SHOPNO, QS_TILE, GtkBox)

GtkWidget *shopno_qs_tile_new             (const gchar  *icon_name,
                                           const gchar  *label,
                                           gboolean      has_details);

void       shopno_qs_tile_set_icon_name   (ShopnoQsTile *tile,
                                           const gchar  *icon_name);
void       shopno_qs_tile_set_label       (ShopnoQsTile *tile,
                                           const gchar  *label);
void       shopno_qs_tile_set_active      (ShopnoQsTile *tile,
                                           gboolean      active);

/* Insensitive keeps the tile visible but greyed out, like Windows does
 * for hardware that is switched off. The tooltip says why. */
void       shopno_qs_tile_set_available   (ShopnoQsTile *tile,
                                           gboolean      available,
                                           const gchar  *reason);

G_END_DECLS

#endif /* !__SHOPNO_QS_TILE_H__ */
