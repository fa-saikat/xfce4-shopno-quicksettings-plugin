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

#ifndef __SHOPNO_QS_GAMMA_H__
#define __SHOPNO_QS_GAMMA_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* Neutral white: gamma ramps back to linear */
#define SHOPNO_QS_GAMMA_NEUTRAL 6500

/* Gamma ramps need XRandR, so X11 only */
gboolean shopno_qs_gamma_supported (GdkDisplay *display);

/* Sets every CRTC to the color of a black body at this temperature */
void     shopno_qs_gamma_set       (GdkDisplay *display,
                                    guint       temperature);

G_END_DECLS

#endif /* !__SHOPNO_QS_GAMMA_H__ */
