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

#ifndef __SHOPNO_QS_UTIL_H__
#define __SHOPNO_QS_UTIL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* The first of the NULL-terminated command lines whose program is
 * installed, or NULL. Returned string is owned by the caller. */
gchar     *shopno_qs_find_command (const gchar * const *commands);

/* Runs the first installed command and hides the flyout. FALSE when
 * none is installed. */
gboolean   shopno_qs_launch       (GtkWidget           *window,
                                   const gchar * const *commands);

/* A link-style button at the bottom of a detail page, such as "More
 * Wi-Fi settings". Hidden when none of the commands is installed. */
GtkWidget *shopno_qs_link_new     (GtkWidget           *window,
                                   const gchar         *label,
                                   const gchar * const *commands);

/* A row for the lists in detail pages: icon, title and an optional
 * subtitle, with an optional widget at the end */
GtkWidget *shopno_qs_row_new      (const gchar         *icon_name,
                                   const gchar         *title,
                                   const gchar         *subtitle,
                                   GtkWidget           *end);

G_END_DECLS

#endif /* !__SHOPNO_QS_UTIL_H__ */
