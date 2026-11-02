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

#include "config.h"

#include "shopno-qs-tray.h"
#include "shopno-qs-tray-box.h"
#include "shopno-qs-tray-icon.h"

struct _ShopnoQsTrayBox
{
  GtkBox        __parent__;

  ShopnoQsTray *tray;
  gint          icon_size;
};

G_DEFINE_TYPE (ShopnoQsTrayBox, shopno_qs_tray_box, GTK_TYPE_BOX)



static void
shopno_qs_tray_box_item_added (ShopnoQsTrayBox  *self,
                               ShopnoQsTrayItem *item)
{
  GtkWidget *icon = shopno_qs_tray_icon_new (item);

  shopno_qs_tray_icon_set_icon_size (SHOPNO_QS_TRAY_ICON (icon), self->icon_size);
  /* The icon shows itself once the app is ready and not passive */
  gtk_widget_set_no_show_all (icon, TRUE);
  gtk_box_pack_start (GTK_BOX (self), icon, FALSE, FALSE, 0);
}



static void
shopno_qs_tray_box_item_removed (ShopnoQsTrayBox  *self,
                                 ShopnoQsTrayItem *item)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self));

  for (GList *l = children; l != NULL; l = l->next)
    if (shopno_qs_tray_icon_get_item (l->data) == item)
      gtk_widget_destroy (l->data);

  g_list_free (children);
}



static void
shopno_qs_tray_box_dispose (GObject *object)
{
  ShopnoQsTrayBox *self = SHOPNO_QS_TRAY_BOX (object);

  if (self->tray != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->tray, self);
      g_clear_object (&self->tray);
    }

  G_OBJECT_CLASS (shopno_qs_tray_box_parent_class)->dispose (object);
}



static void
shopno_qs_tray_box_class_init (ShopnoQsTrayBoxClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = shopno_qs_tray_box_dispose;
}



static void
shopno_qs_tray_box_init (ShopnoQsTrayBox *self)
{
  GPtrArray *items;

  self->icon_size = 16;
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-tray");

  self->tray = shopno_qs_tray_get_default ();
  g_signal_connect_swapped (self->tray, "item-added", G_CALLBACK (shopno_qs_tray_box_item_added), self);
  g_signal_connect_swapped (self->tray, "item-removed", G_CALLBACK (shopno_qs_tray_box_item_removed), self);

  items = shopno_qs_tray_get_items (self->tray);
  for (guint i = 0; i < items->len; i++)
    shopno_qs_tray_box_item_added (self, g_ptr_array_index (items, i));
}



GtkWidget *
shopno_qs_tray_box_new (void)
{
  return g_object_new (SHOPNO_TYPE_QS_TRAY_BOX, NULL);
}



void
shopno_qs_tray_box_set_icon_size (ShopnoQsTrayBox *box,
                                  gint             size)
{
  GList *children;

  g_return_if_fail (SHOPNO_IS_QS_TRAY_BOX (box));

  box->icon_size = size;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (GList *l = children; l != NULL; l = l->next)
    shopno_qs_tray_icon_set_icon_size (l->data, size);
  g_list_free (children);
}
