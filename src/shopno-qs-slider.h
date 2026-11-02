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

#ifndef __SHOPNO_QS_SLIDER_H__
#define __SHOPNO_QS_SLIDER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHOPNO_TYPE_QS_SLIDER (shopno_qs_slider_get_type ())
G_DECLARE_FINAL_TYPE (ShopnoQsSlider, shopno_qs_slider, SHOPNO, QS_SLIDER, GtkBox)

GtkWidget *shopno_qs_slider_new           (const gchar    *icon_name,
                                           const gchar    *icon_tooltip,
                                           gboolean        has_details);

void       shopno_qs_slider_set_icon_name (ShopnoQsSlider *slider,
                                           const gchar    *icon_name);

/* 0.0 to 1.0. Setting the value from code does not emit "value-changed". */
void       shopno_qs_slider_set_value     (ShopnoQsSlider *slider,
                                           gdouble         value);

/* While the user drags, values from the system are ignored so the knob
 * does not jump back */
gboolean   shopno_qs_slider_is_dragging   (ShopnoQsSlider *slider);

G_END_DECLS

#endif /* !__SHOPNO_QS_SLIDER_H__ */
