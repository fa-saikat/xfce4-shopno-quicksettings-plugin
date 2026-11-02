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

/* A panel line never wraps */
#define ONE_LINE 256

enum
{
  CHANGED,
  ICON_ACTIVATED,
  N_SIGNALS
};

struct _ShopnoQsTrayBox
{
  GtkFlowBox        __parent__;

  ShopnoQsTray     *tray;
  ShopnoQsSettings *settings;
  gboolean          pinned;
  gint              icon_size;
  guint             columns;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsTrayBox, shopno_qs_tray_box, GTK_TYPE_FLOW_BOX)



static gboolean
shopno_qs_tray_box_wants (ShopnoQsTrayBox  *self,
                          ShopnoQsTrayItem *item)
{
  if (!shopno_qs_tray_item_get_visible (item))
    return FALSE;

  return shopno_qs_settings_is_tray_pinned (self->settings, shopno_qs_tray_item_get_id (item)) == self->pinned;
}



/* Show the icons that belong here; the flow box skips hidden children */
static void
shopno_qs_tray_box_update (ShopnoQsTrayBox *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self));

  for (GList *l = children; l != NULL; l = l->next)
    {
      GtkWidget *icon = gtk_bin_get_child (GTK_BIN (l->data));

      gtk_widget_set_visible (l->data,
                              shopno_qs_tray_box_wants (self, shopno_qs_tray_icon_get_item (SHOPNO_QS_TRAY_ICON (icon))));
    }

  g_list_free (children);

  /* A flow box asks for room for its widest row, so a grid of few icons
   * would be wide and mostly empty */
  if (self->columns > 0)
    gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self),
                                            CLAMP (shopno_qs_tray_box_get_n_shown (self), 1, self->columns));

  g_signal_emit (self, signals[CHANGED], 0);
}



static void
shopno_qs_tray_box_icon_activated (ShopnoQsTrayBox *self)
{
  g_signal_emit (self, signals[ICON_ACTIVATED], 0);
}



static void
shopno_qs_tray_box_item_added (ShopnoQsTrayBox  *self,
                               ShopnoQsTrayItem *item)
{
  GtkWidget *child = gtk_flow_box_child_new ();
  GtkWidget *icon = shopno_qs_tray_icon_new (item);

  gtk_widget_set_can_focus (child, FALSE);
  shopno_qs_tray_icon_set_icon_size (SHOPNO_QS_TRAY_ICON (icon), self->icon_size);
  gtk_container_add (GTK_CONTAINER (child), icon);
  gtk_widget_show (icon);
  gtk_widget_set_no_show_all (child, TRUE);
  g_signal_connect_swapped (icon, "activated", G_CALLBACK (shopno_qs_tray_box_icon_activated), self);
  gtk_container_add (GTK_CONTAINER (self), child);

  /* The id and status come later, and may change */
  g_signal_connect_object (item, "changed", G_CALLBACK (shopno_qs_tray_box_update),
                           self, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  shopno_qs_tray_box_update (self);
}



static void
shopno_qs_tray_box_item_removed (ShopnoQsTrayBox  *self,
                                 ShopnoQsTrayItem *item)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self));

  for (GList *l = children; l != NULL; l = l->next)
    if (shopno_qs_tray_icon_get_item (SHOPNO_QS_TRAY_ICON (gtk_bin_get_child (GTK_BIN (l->data)))) == item)
      gtk_widget_destroy (l->data);

  g_list_free (children);
  g_signal_emit (self, signals[CHANGED], 0);
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

  if (self->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->settings, self);
      g_clear_object (&self->settings);
    }

  G_OBJECT_CLASS (shopno_qs_tray_box_parent_class)->dispose (object);
}



static void
shopno_qs_tray_box_class_init (ShopnoQsTrayBoxClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = shopno_qs_tray_box_dispose;

  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /* A click on one of the icons, before the app acts on it */
  signals[ICON_ACTIVATED] =
    g_signal_new ("icon-activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_tray_box_init (ShopnoQsTrayBox *self)
{
  self->icon_size = 16;

  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self), TRUE);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self), ONE_LINE);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-tray");
}



GtkWidget *
shopno_qs_tray_box_new (ShopnoQsSettings *settings,
                        gboolean          pinned)
{
  ShopnoQsTrayBox *self = g_object_new (SHOPNO_TYPE_QS_TRAY_BOX, NULL);
  GPtrArray       *items;

  self->settings = g_object_ref (settings);
  self->pinned = pinned;
  g_signal_connect_swapped (settings, "notify::tray-pinned", G_CALLBACK (shopno_qs_tray_box_update), self);

  self->tray = shopno_qs_tray_get_default ();
  g_signal_connect_swapped (self->tray, "item-added", G_CALLBACK (shopno_qs_tray_box_item_added), self);
  g_signal_connect_swapped (self->tray, "item-removed", G_CALLBACK (shopno_qs_tray_box_item_removed), self);

  items = shopno_qs_tray_get_items (self->tray);
  for (guint i = 0; i < items->len; i++)
    shopno_qs_tray_box_item_added (self, g_ptr_array_index (items, i));

  return GTK_WIDGET (self);
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
    shopno_qs_tray_icon_set_icon_size (SHOPNO_QS_TRAY_ICON (gtk_bin_get_child (GTK_BIN (l->data))), size);
  g_list_free (children);
}



void
shopno_qs_tray_box_set_columns (ShopnoQsTrayBox *box,
                                guint            columns)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_BOX (box));

  box->columns = columns;
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 1);
  shopno_qs_tray_box_update (box);
}



guint
shopno_qs_tray_box_get_n_shown (ShopnoQsTrayBox *box)
{
  GList *children;
  guint  shown = 0;

  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_BOX (box), 0);

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (GList *l = children; l != NULL; l = l->next)
    if (gtk_widget_get_visible (l->data))
      shown++;
  g_list_free (children);

  return shown;
}
