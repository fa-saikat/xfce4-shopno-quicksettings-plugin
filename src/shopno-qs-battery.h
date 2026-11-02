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

#ifndef __SHOPNO_QS_BATTERY_H__
#define __SHOPNO_QS_BATTERY_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_BATTERY (shopno_qs_battery_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsBattery, shopno_qs_battery, SHOPNO, QS_BATTERY, GObject)

/* Shared by the flyout and the panel button. Returns a new reference. */
ShopnoQsBattery *shopno_qs_battery_get_default    (void);

/* The computer has a battery */
gboolean         shopno_qs_battery_get_present    (ShopnoQsBattery *battery);
gdouble          shopno_qs_battery_get_percentage (ShopnoQsBattery *battery);

const gchar     *shopno_qs_battery_get_icon_name  (ShopnoQsBattery *battery);

/* "2 hours 10 minutes remaining", "Fully charged", ...; free with g_free() */
gchar           *shopno_qs_battery_get_status     (ShopnoQsBattery *battery);

G_END_DECLS

#endif /* !__SHOPNO_QS_BATTERY_H__ */
