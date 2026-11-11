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

#ifndef __SHOPNO_QS_DDC_H__
#define __SHOPNO_QS_DDC_H__

#include <glib.h>

G_BEGIN_DECLS

/*
 * Brightness of external monitors over DDC/CI. All I2C work happens on a
 * worker thread; the callback runs on the main thread after a scan or a
 * read, with the number of monitors that answered and the level of the
 * first one as a fraction, or -1 when it is not known.
 */

typedef struct _ShopnoQsDdc ShopnoQsDdc;

typedef void (*ShopnoQsDdcFunc) (guint    n_monitors,
                                 gdouble  level,
                                 gpointer data);

/* Starts the worker and looks for monitors */
ShopnoQsDdc *shopno_qs_ddc_new     (ShopnoQsDdcFunc  func,
                                    gpointer         data);

/* Looks for monitors again, after one is plugged in or removed */
void         shopno_qs_ddc_rescan  (ShopnoQsDdc     *ddc);

/* Reads the level again; the callback runs unless a write came after */
void         shopno_qs_ddc_refresh (ShopnoQsDdc     *ddc);

/* Sets every monitor. Levels that come in while one is being written
 * replace each other, so a fast drag sends only a few. */
void         shopno_qs_ddc_set     (ShopnoQsDdc     *ddc,
                                    gdouble          level);

/* Stops the worker. The callback does not run after this. */
void         shopno_qs_ddc_free    (ShopnoQsDdc     *ddc);

G_END_DECLS

#endif /* !__SHOPNO_QS_DDC_H__ */
