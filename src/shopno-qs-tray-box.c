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

#include <gdk/gdkx.h>

#include "shopno-qs-tray.h"
#include "shopno-qs-tray-box.h"
#include "shopno-qs-tray-icon.h"
#include "shopno-qs-tray-socket.h"

/* A panel line never wraps */
#define ONE_LINE 256

#define CHILD_ITEM   "shopno-qs-tray-item"
#define CHILD_ZOMBIE "shopno-qs-tray-zombie"

enum
{
  CHANGED,
  ICON_ACTIVATED,
  ICON_MENU_DISMISSED,
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
  const gchar *id = shopno_qs_tray_item_get_id (item);

  if (!shopno_qs_tray_item_get_visible (item) || shopno_qs_settings_is_tray_hidden (self->settings, id))
    return FALSE;

  return shopno_qs_settings_is_tray_pinned (self->settings, id) == self->pinned;
}



static ShopnoQsTrayItem *
shopno_qs_tray_box_child_item (GtkWidget *child)
{
  return g_object_get_data (G_OBJECT (child), CHILD_ITEM);
}



/* In the order the apps appeared, wherever an icon was added */
static gint
shopno_qs_tray_box_sort (GtkFlowBoxChild *a,
                         GtkFlowBoxChild *b,
                         gpointer         data)
{
  ShopnoQsTrayBox *self = data;
  GPtrArray       *items = shopno_qs_tray_get_items (self->tray);
  guint            index_a = G_MAXUINT, index_b = G_MAXUINT;

  g_ptr_array_find (items, shopno_qs_tray_box_child_item (GTK_WIDGET (a)), &index_a);
  g_ptr_array_find (items, shopno_qs_tray_box_child_item (GTK_WIDGET (b)), &index_b);

  return index_a < index_b ? -1 : index_a > index_b;
}



static GtkWidget *
shopno_qs_tray_box_add_child (ShopnoQsTrayBox  *self,
                              ShopnoQsTrayItem *item,
                              GtkWidget        *widget)
{
  GtkWidget *child = gtk_flow_box_child_new ();

  gtk_widget_set_can_focus (child, FALSE);
  g_object_set_data_full (G_OBJECT (child), CHILD_ITEM, g_object_ref (item), g_object_unref);
  if (widget != NULL)
    {
      gtk_container_add (GTK_CONTAINER (child), widget);
      gtk_widget_show (widget);
    }
  gtk_widget_set_no_show_all (child, TRUE);
  gtk_container_add (GTK_CONTAINER (self), child);

  return child;
}



static void shopno_qs_tray_box_update (ShopnoQsTrayBox *self);



/* The socket holding an XEmbed item, wherever it is, or NULL */
static GtkWidget *
shopno_qs_tray_box_find_socket (ShopnoQsTrayBox  *self,
                                ShopnoQsTrayItem *item)
{
  GList     *children = gtk_container_get_children (GTK_CONTAINER (self));
  GtkWidget *socket = NULL;

  for (GList *l = children; l != NULL && socket == NULL; l = l->next)
    if (shopno_qs_tray_box_child_item (l->data) == item
        && g_object_get_data (G_OBJECT (l->data), CHILD_ZOMBIE) == NULL)
      socket = gtk_bin_get_child (GTK_BIN (l->data));

  g_list_free (children);

  return socket;
}



static gboolean
shopno_qs_tray_box_plug_removed_idle (gpointer data)
{
  GtkWidget        *socket = data;
  ShopnoQsTrayItem *item = shopno_qs_tray_socket_get_item (SHOPNO_QS_TRAY_SOCKET (socket));
  GtkWidget        *box = gtk_widget_get_ancestor (socket, SHOPNO_TYPE_QS_TRAY_BOX);

  /* The app closed its icon, or another tray took it */
  if (box != NULL && item != NULL && SHOPNO_QS_TRAY_BOX (box)->tray != NULL
      && gtk_socket_get_plug_window (GTK_SOCKET (socket)) == NULL)
    {
      shopno_qs_tray_item_set_xembed_host (item, NULL);
      shopno_qs_tray_remove_item (SHOPNO_QS_TRAY_BOX (box)->tray, item);
    }

  g_object_unref (socket);

  return G_SOURCE_REMOVE;
}



/* GTK is still handling the X event that says so: act on it later */
static gboolean
shopno_qs_tray_box_plug_removed (GtkSocket *socket)
{
  g_idle_add (shopno_qs_tray_box_plug_removed_idle, g_object_ref (socket));

  /* Not the default, which destroys the socket */
  return TRUE;
}



/* An XEmbed icon's window stays in one socket for good: GtkSocket leaves
 * an event filter on the window, which must not outlive the socket, and
 * GDK mishandles two of them. To move the icon, the socket moves, still
 * realized, from the box that held it to the one it now belongs in. */
static void
shopno_qs_tray_box_embed (ShopnoQsTrayBox  *self,
                          ShopnoQsTrayItem *item)
{
  ShopnoQsTrayBox *host = shopno_qs_tray_item_get_xembed_host (item);
  GtkWidget       *socket;
  GtkWidget       *child;
  GtkWidget       *old_child;

  if (host == self || !shopno_qs_tray_box_wants (self, item)
      || (host != NULL && shopno_qs_tray_box_wants (host, item))
      || !GTK_IS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))))
    return;

  if (host != NULL)
    {
      socket = shopno_qs_tray_box_find_socket (host, item);
      if (socket == NULL)
        return;

      old_child = gtk_widget_get_parent (socket);
      child = shopno_qs_tray_box_add_child (self, item, NULL);
      gtk_widget_show (child);
      gtk_widget_realize (child);

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_widget_reparent (socket, child);
      G_GNUC_END_IGNORE_DEPRECATIONS

      shopno_qs_tray_socket_moved (SHOPNO_QS_TRAY_SOCKET (socket));
      shopno_qs_tray_socket_set_size (SHOPNO_QS_TRAY_SOCKET (socket), self->icon_size);
      shopno_qs_tray_item_set_xembed_host (item, self);
      gtk_widget_destroy (old_child);
      shopno_qs_tray_box_update (host);
      return;
    }

  socket = shopno_qs_tray_socket_new (item);
  if (socket == NULL)
    {
      /* The app's window is gone already */
      shopno_qs_tray_remove_item (self->tray, item);
      return;
    }

  /* The app draws its icon to fill the socket: keep that at the icon
   * size, spaced like the other icons */
  shopno_qs_tray_socket_set_size (SHOPNO_QS_TRAY_SOCKET (socket), self->icon_size);
  gtk_widget_set_halign (socket, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (socket, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (socket, 4);
  gtk_widget_set_margin_end (socket, 4);
  g_signal_connect (socket, "plug-removed", G_CALLBACK (shopno_qs_tray_box_plug_removed), NULL);

  child = shopno_qs_tray_box_add_child (self, item, socket);
  gtk_widget_show (child);
  gtk_widget_realize (socket);

  shopno_qs_tray_item_set_xembed_host (item, self);
  gtk_socket_add_id (GTK_SOCKET (socket), shopno_qs_tray_item_get_xembed_window (item));
}



static void
shopno_qs_tray_box_zombie_free (gpointer  data,
                                GObject  *window)
{
  gtk_widget_destroy (data);
  g_object_unref (data);
}



/* A socket of a removed item. While GDK still knows the item's window,
 * the socket's filter on it stays, so the socket stays too, hidden. */
static void
shopno_qs_tray_box_remove_socket (GtkWidget *child)
{
  ShopnoQsTrayItem *item = shopno_qs_tray_box_child_item (child);
  GdkWindow        *window;

  window = gdk_x11_window_lookup_for_display (gtk_widget_get_display (child),
                                              shopno_qs_tray_item_get_xembed_window (item));
  if (window == NULL || gdk_window_is_destroyed (window))
    {
      gtk_widget_destroy (child);
      return;
    }

  g_object_set_data (G_OBJECT (child), CHILD_ZOMBIE, GINT_TO_POINTER (TRUE));
  gtk_widget_hide (child);
  g_object_weak_ref (G_OBJECT (window), shopno_qs_tray_box_zombie_free, g_object_ref (child));
}



/* Show the icons that belong here; the flow box skips hidden children */
static void
shopno_qs_tray_box_update (ShopnoQsTrayBox *self)
{
  GPtrArray *items = shopno_qs_tray_get_items (self->tray);
  GList     *children;

  for (guint i = 0; i < items->len; i++)
    if (shopno_qs_tray_item_get_xembed_window (g_ptr_array_index (items, i)) != 0)
      shopno_qs_tray_box_embed (self, g_ptr_array_index (items, i));

  /* Hidden: icons that belong to the other box, an XEmbed one until that
   * box takes it, and sockets of removed items */
  children = gtk_container_get_children (GTK_CONTAINER (self));
  for (GList *l = children; l != NULL; l = l->next)
    gtk_widget_set_visible (l->data,
                            g_object_get_data (G_OBJECT (l->data), CHILD_ZOMBIE) == NULL
                            && shopno_qs_tray_box_wants (self, shopno_qs_tray_box_child_item (l->data)));
  g_list_free (children);

  /* A flow box asks for room for its widest row, so a grid of few icons
   * would be wide and mostly empty. Its minimum size is measured at the
   * fewest columns allowed, and the panel places the flyout from that
   * size, so both ends are the same. */
  if (self->columns > 0)
    {
      guint columns = CLAMP (shopno_qs_tray_box_get_n_shown (self), 1, self->columns);

      gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self), columns);
      gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self), columns);
    }
  else
    {
      /* Given its minimum width, a flow box wraps icons small enough to
       * fit two rows in the panel's height */
      gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self),
                                              MAX (shopno_qs_tray_box_get_n_shown (self), 1));
    }

  g_signal_emit (self, signals[CHANGED], 0);
}



static void
shopno_qs_tray_box_icon_activated (ShopnoQsTrayBox *self)
{
  g_signal_emit (self, signals[ICON_ACTIVATED], 0);
}



static void
shopno_qs_tray_box_icon_menu_dismissed (ShopnoQsTrayBox *self)
{
  g_signal_emit (self, signals[ICON_MENU_DISMISSED], 0);
}



static void
shopno_qs_tray_box_item_added (ShopnoQsTrayBox  *self,
                               ShopnoQsTrayItem *item)
{
  GtkWidget *icon;

  /* XEmbed icons get their socket when embedded */
  if (shopno_qs_tray_item_get_xembed_window (item) == 0)
    {
      icon = shopno_qs_tray_icon_new (item);
      shopno_qs_tray_icon_set_icon_size (SHOPNO_QS_TRAY_ICON (icon), self->icon_size);
      g_signal_connect_swapped (icon, "activated", G_CALLBACK (shopno_qs_tray_box_icon_activated), self);
      g_signal_connect_swapped (icon, "menu-dismissed", G_CALLBACK (shopno_qs_tray_box_icon_menu_dismissed), self);
      shopno_qs_tray_box_add_child (self, item, icon);

      /* The id and status come later, and may change */
      g_signal_connect_object (item, "changed", G_CALLBACK (shopno_qs_tray_box_update),
                               self, G_CONNECT_SWAPPED | G_CONNECT_AFTER);
    }

  shopno_qs_tray_box_update (self);
}



static void
shopno_qs_tray_box_item_removed (ShopnoQsTrayBox  *self,
                                 ShopnoQsTrayItem *item)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self));

  for (GList *l = children; l != NULL; l = l->next)
    if (shopno_qs_tray_box_child_item (l->data) == item
        && g_object_get_data (G_OBJECT (l->data), CHILD_ZOMBIE) == NULL)
      {
        if (shopno_qs_tray_item_get_xembed_window (item) != 0)
          shopno_qs_tray_box_remove_socket (l->data);
        else
          gtk_widget_destroy (l->data);
      }

  if (shopno_qs_tray_item_get_xembed_host (item) == self)
    shopno_qs_tray_item_set_xembed_host (item, NULL);

  g_list_free (children);

  /* Fewer icons may need fewer columns */
  shopno_qs_tray_box_update (self);
}



static void
shopno_qs_tray_box_dispose (GObject *object)
{
  ShopnoQsTrayBox *self = SHOPNO_QS_TRAY_BOX (object);

  if (self->tray != NULL)
    {
      GPtrArray *items = shopno_qs_tray_get_items (self->tray);

      for (guint i = 0; i < items->len; i++)
        if (shopno_qs_tray_item_get_xembed_host (g_ptr_array_index (items, i)) == self)
          shopno_qs_tray_item_set_xembed_host (g_ptr_array_index (items, i), NULL);

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

  /* An icon's menu closed without a choice */
  signals[ICON_MENU_DISMISSED] =
    g_signal_new ("icon-menu-dismissed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_tray_box_init (ShopnoQsTrayBox *self)
{
  self->icon_size = 16;

  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self), TRUE);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self), ONE_LINE);
  gtk_flow_box_set_sort_func (GTK_FLOW_BOX (self), shopno_qs_tray_box_sort, self, NULL);
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
  g_signal_connect_swapped (settings, "notify::tray-hidden", G_CALLBACK (shopno_qs_tray_box_update), self);

  self->tray = shopno_qs_tray_get_default ();

  /* XEmbed icons wait for a toplevel window */
  g_signal_connect (self, "hierarchy-changed", G_CALLBACK (shopno_qs_tray_box_update), NULL);
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
    {
      GtkWidget *widget = gtk_bin_get_child (GTK_BIN (l->data));

      if (SHOPNO_IS_QS_TRAY_ICON (widget))
        shopno_qs_tray_icon_set_icon_size (SHOPNO_QS_TRAY_ICON (widget), size);
      else
        shopno_qs_tray_socket_set_size (SHOPNO_QS_TRAY_SOCKET (widget), size);
    }
  g_list_free (children);
}



void
shopno_qs_tray_box_set_columns (ShopnoQsTrayBox *box,
                                guint            columns)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_BOX (box));

  box->columns = columns;
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



void
shopno_qs_tray_box_reset_icons (ShopnoQsTrayBox *box)
{
  GList *children;

  g_return_if_fail (SHOPNO_IS_QS_TRAY_BOX (box));

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (GList *l = children; l != NULL; l = l->next)
    gtk_widget_unset_state_flags (gtk_bin_get_child (GTK_BIN (l->data)),
                                  GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);
  g_list_free (children);
}



gboolean
shopno_qs_tray_box_socket_at (ShopnoQsTrayBox *box,
                              gint             x_root,
                              gint             y_root)
{
  GList    *children;
  gboolean  found = FALSE;

  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_BOX (box), FALSE);

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (GList *l = children; l != NULL && !found; l = l->next)
    {
      GtkWidget     *widget = gtk_bin_get_child (GTK_BIN (l->data));
      GtkAllocation  alloc;
      gint           x, y;

      if (!SHOPNO_IS_QS_TRAY_SOCKET (widget) || !gtk_widget_get_mapped (widget))
        continue;

      gdk_window_get_origin (gtk_widget_get_window (widget), &x, &y);
      gtk_widget_get_allocation (widget, &alloc);
      found = x_root >= x && x_root < x + alloc.width && y_root >= y && y_root < y + alloc.height;
    }
  g_list_free (children);

  return found;
}
