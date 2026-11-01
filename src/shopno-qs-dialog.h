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

#ifndef __SHOPNO_QS_DIALOG_H__
#define __SHOPNO_QS_DIALOG_H__

#include <gtk/gtk.h>

#include "shopno-qs-settings.h"

G_BEGIN_DECLS

/* The properties dialog; it destroys itself when closed */
GtkWidget *shopno_qs_dialog_new (GtkWindow        *parent,
                                 ShopnoQsSettings *settings);

G_END_DECLS

#endif /* !__SHOPNO_QS_DIALOG_H__ */
