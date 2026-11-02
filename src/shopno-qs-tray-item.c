/*
 * Copyright (C) 2026 FA Saikat
 * Copyright (C) 2017 Viktor Odintsev <ninetls@xfce.org>
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

/*
 * One StatusNotifierItem: an app's tray icon, published over D-Bus.
 *
 * Properties are read with GetAll and read again when the app signals a
 * change. The pixmap conversion follows sn-item.c of Xfce's systray
 * plugin.
 */

#include "config.h"

#include "shopno-qs-tray-item.h"

#define ITEM_IFACE       "org.kde.StatusNotifierItem"
#define PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

enum
{
  CHANGED,
  N_SIGNALS
};

struct _ShopnoQsTrayItem
{
  GObject          __parent__;

  gchar           *key;
  gchar           *bus_name;
  gchar           *object_path;

  GDBusConnection *bus;
  GCancellable    *cancellable;
  guint            signal_id;
  guint            refresh_id;

  gboolean         ready;
  gchar           *id;
  gchar           *status;
  gchar           *title;
  gchar           *tooltip_title;
  gchar           *tooltip_body;
  gchar           *icon_name;
  gchar           *icon_theme_path;
  GdkPixbuf       *icon_pixbuf;
  gchar           *attention_icon_name;
  GdkPixbuf       *attention_pixbuf;
  gchar           *menu_path;
  gboolean         item_is_menu;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsTrayItem, shopno_qs_tray_item, G_TYPE_OBJECT)

static void shopno_qs_tray_item_refresh (ShopnoQsTrayItem *self);



/*
 * Reading properties
 */

static void
shopno_qs_tray_item_free_pixels (guchar   *pixels,
                                 gpointer  data)
{
  g_free (pixels);
}



/* The largest image of an a(iiay) pixmap list, from ARGB32 in network
 * byte order to the RGBA a GdkPixbuf wants */
static GdkPixbuf *
shopno_qs_tray_item_extract_pixbuf (GVariant *variant)
{
  GVariantIter  iter;
  GVariant     *bytes;
  gint          width, height;
  gint          best_width = 0, best_height = 0;
  guchar       *pixels = NULL;

  if (!g_variant_is_of_type (variant, G_VARIANT_TYPE ("a(iiay)")))
    return NULL;

  g_variant_iter_init (&iter, variant);
  while (g_variant_iter_loop (&iter, "(ii@ay)", &width, &height, &bytes))
    {
      gsize size = g_variant_get_size (bytes);

      if (width <= 0 || height <= 0 || width * height <= best_width * best_height
          || size != (gsize) width * height * 4)
        continue;

      g_free (pixels);
      pixels = g_memdup2 (g_variant_get_data (bytes), size);
      best_width = width;
      best_height = height;
    }

  if (pixels == NULL)
    return NULL;

  for (gint i = 0; i < best_width * best_height * 4; i += 4)
    {
      guchar alpha = pixels[i];

      pixels[i] = pixels[i + 1];
      pixels[i + 1] = pixels[i + 2];
      pixels[i + 2] = pixels[i + 3];
      pixels[i + 3] = alpha;
    }

  return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8,
                                   best_width, best_height, best_width * 4,
                                   shopno_qs_tray_item_free_pixels, NULL);
}



/* Empty strings mean "not set" */
static void
shopno_qs_tray_item_set_string (gchar    **field,
                                GVariant  *value)
{
  const gchar *string = NULL;

  if (value != NULL && (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING)
                        || g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH)))
    string = g_variant_get_string (value, NULL);

  g_free (*field);
  *field = string != NULL && *string != '\0' ? g_strdup (string) : NULL;
}



static void
shopno_qs_tray_item_set_pixbuf (GdkPixbuf **field,
                                GVariant   *value)
{
  g_clear_object (field);
  if (value != NULL)
    *field = shopno_qs_tray_item_extract_pixbuf (value);
}



static void
shopno_qs_tray_item_got_properties (GObject      *source,
                                    GAsyncResult *result,
                                    gpointer      data)
{
  ShopnoQsTrayItem *self;
  GVariant         *reply;
  GVariant         *properties;
  GVariantIter      iter;
  const gchar      *name;
  GVariant         *value;
  GError           *error = NULL;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Tray item %s: %s", ((ShopnoQsTrayItem *) data)->key, error->message);
      g_error_free (error);
      return;
    }

  self = SHOPNO_QS_TRAY_ITEM (data);
  properties = g_variant_get_child_value (reply, 0);

  g_variant_iter_init (&iter, properties);
  while (g_variant_iter_loop (&iter, "{&sv}", &name, &value))
    {
      if (g_strcmp0 (name, "Id") == 0)
        shopno_qs_tray_item_set_string (&self->id, value);
      else if (g_strcmp0 (name, "Status") == 0)
        shopno_qs_tray_item_set_string (&self->status, value);
      else if (g_strcmp0 (name, "Title") == 0)
        shopno_qs_tray_item_set_string (&self->title, value);
      else if (g_strcmp0 (name, "IconName") == 0)
        shopno_qs_tray_item_set_string (&self->icon_name, value);
      else if (g_strcmp0 (name, "IconThemePath") == 0)
        shopno_qs_tray_item_set_string (&self->icon_theme_path, value);
      else if (g_strcmp0 (name, "IconPixmap") == 0)
        shopno_qs_tray_item_set_pixbuf (&self->icon_pixbuf, value);
      else if (g_strcmp0 (name, "AttentionIconName") == 0)
        shopno_qs_tray_item_set_string (&self->attention_icon_name, value);
      else if (g_strcmp0 (name, "AttentionIconPixmap") == 0)
        shopno_qs_tray_item_set_pixbuf (&self->attention_pixbuf, value);
      else if (g_strcmp0 (name, "Menu") == 0)
        shopno_qs_tray_item_set_string (&self->menu_path, value);
      else if (g_strcmp0 (name, "ItemIsMenu") == 0 && g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        self->item_is_menu = g_variant_get_boolean (value);
      else if (g_strcmp0 (name, "ToolTip") == 0)
        {
          /* (icon name, icon pixmap, title, body); some apps send a string */
          if (g_variant_is_of_type (value, G_VARIANT_TYPE ("(sa(iiay)ss)")))
            {
              GVariant *title = g_variant_get_child_value (value, 2);
              GVariant *body = g_variant_get_child_value (value, 3);

              shopno_qs_tray_item_set_string (&self->tooltip_title, title);
              shopno_qs_tray_item_set_string (&self->tooltip_body, body);
              g_variant_unref (title);
              g_variant_unref (body);
            }
          else
            {
              shopno_qs_tray_item_set_string (&self->tooltip_title, value);
              g_clear_pointer (&self->tooltip_body, g_free);
            }
        }
    }

  g_variant_unref (properties);
  g_variant_unref (reply);

  /* An item without an id is not set up yet */
  self->ready = self->id != NULL;
  g_signal_emit (self, signals[CHANGED], 0);
}



static gboolean
shopno_qs_tray_item_refresh_idle (gpointer data)
{
  ShopnoQsTrayItem *self = data;

  self->refresh_id = 0;
  g_dbus_connection_call (self->bus, self->bus_name, self->object_path,
                          PROPERTIES_IFACE, "GetAll", g_variant_new ("(s)", ITEM_IFACE),
                          G_VARIANT_TYPE ("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, -1,
                          self->cancellable, shopno_qs_tray_item_got_properties, self);

  return G_SOURCE_REMOVE;
}



/* Apps often send several New* signals together; read once for all */
static void
shopno_qs_tray_item_refresh (ShopnoQsTrayItem *self)
{
  if (self->refresh_id == 0 && self->bus != NULL)
    self->refresh_id = g_idle_add (shopno_qs_tray_item_refresh_idle, self);
}



static void
shopno_qs_tray_item_signal (GDBusConnection *connection,
                            const gchar     *sender,
                            const gchar     *path,
                            const gchar     *interface,
                            const gchar     *signal,
                            GVariant        *parameters,
                            gpointer         data)
{
  /* NewIcon, NewAttentionIcon, NewTitle, NewToolTip, NewStatus, ... */
  shopno_qs_tray_item_refresh (data);
}



/*
 * Actions
 */

static void
shopno_qs_tray_item_call (ShopnoQsTrayItem *self,
                          const gchar      *method,
                          GVariant         *parameters)
{
  if (self->bus == NULL)
    {
      g_variant_unref (g_variant_ref_sink (parameters));
      return;
    }

  /* Fire and forget: many apps do not implement every method */
  g_dbus_connection_call (self->bus, self->bus_name, self->object_path, ITEM_IFACE, method,
                          parameters, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}



/*
 * GObject
 */

static void
shopno_qs_tray_item_dispose (GObject *object)
{
  ShopnoQsTrayItem *self = SHOPNO_QS_TRAY_ITEM (object);

  g_cancellable_cancel (self->cancellable);

  if (self->refresh_id != 0)
    {
      g_source_remove (self->refresh_id);
      self->refresh_id = 0;
    }

  if (self->signal_id != 0)
    {
      g_dbus_connection_signal_unsubscribe (self->bus, self->signal_id);
      self->signal_id = 0;
    }

  g_clear_object (&self->bus);

  G_OBJECT_CLASS (shopno_qs_tray_item_parent_class)->dispose (object);
}



static void
shopno_qs_tray_item_finalize (GObject *object)
{
  ShopnoQsTrayItem *self = SHOPNO_QS_TRAY_ITEM (object);

  g_object_unref (self->cancellable);
  g_free (self->key);
  g_free (self->bus_name);
  g_free (self->object_path);
  g_free (self->id);
  g_free (self->status);
  g_free (self->title);
  g_free (self->tooltip_title);
  g_free (self->tooltip_body);
  g_free (self->icon_name);
  g_free (self->icon_theme_path);
  g_clear_object (&self->icon_pixbuf);
  g_free (self->attention_icon_name);
  g_clear_object (&self->attention_pixbuf);
  g_free (self->menu_path);

  G_OBJECT_CLASS (shopno_qs_tray_item_parent_class)->finalize (object);
}



static void
shopno_qs_tray_item_class_init (ShopnoQsTrayItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = shopno_qs_tray_item_dispose;
  gobject_class->finalize = shopno_qs_tray_item_finalize;

  /* Icon, tooltip, status or menu changed, or the item became ready */
  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_tray_item_init (ShopnoQsTrayItem *self)
{
  self->cancellable = g_cancellable_new ();
}



ShopnoQsTrayItem *
shopno_qs_tray_item_new (const gchar *key,
                         const gchar *bus_name,
                         const gchar *object_path)
{
  ShopnoQsTrayItem *self = g_object_new (SHOPNO_TYPE_QS_TRAY_ITEM, NULL);

  self->key = g_strdup (key);
  self->bus_name = g_strdup (bus_name);
  self->object_path = g_strdup (object_path);

  self->bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (self->bus != NULL)
    {
      self->signal_id = g_dbus_connection_signal_subscribe (self->bus, bus_name, ITEM_IFACE, NULL,
                                                            object_path, NULL,
                                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                                            shopno_qs_tray_item_signal,
                                                            self, NULL);
      shopno_qs_tray_item_refresh (self);
    }

  return self;
}



const gchar *
shopno_qs_tray_item_get_key (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  return item->key;
}



const gchar *
shopno_qs_tray_item_get_id (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  return item->id;
}



gboolean
shopno_qs_tray_item_get_visible (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), FALSE);

  return item->ready && g_strcmp0 (item->status, "Passive") != 0;
}



void
shopno_qs_tray_item_get_icon (ShopnoQsTrayItem  *item,
                              const gchar      **icon_name,
                              const gchar      **theme_path,
                              GdkPixbuf        **pixbuf)
{
  gboolean attention;

  g_return_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item));

  attention = g_strcmp0 (item->status, "NeedsAttention") == 0
              && (item->attention_icon_name != NULL || item->attention_pixbuf != NULL);

  if (icon_name != NULL)
    *icon_name = attention ? item->attention_icon_name : item->icon_name;
  if (theme_path != NULL)
    *theme_path = item->icon_theme_path;
  if (pixbuf != NULL)
    *pixbuf = attention ? item->attention_pixbuf : item->icon_pixbuf;
}



const gchar *
shopno_qs_tray_item_get_title (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  return item->title != NULL ? item->title : item->id;
}



gchar *
shopno_qs_tray_item_get_tooltip (ShopnoQsTrayItem *item)
{
  const gchar *title;

  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  title = item->tooltip_title != NULL ? item->tooltip_title : shopno_qs_tray_item_get_title (item);

  if (item->tooltip_body != NULL && g_strcmp0 (item->tooltip_body, title) != 0)
    return g_strdup_printf ("%s\n%s", title != NULL ? title : "", item->tooltip_body);

  return g_strdup (title);
}



const gchar *
shopno_qs_tray_item_get_bus_name (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  return item->bus_name;
}



const gchar *
shopno_qs_tray_item_get_menu_path (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), NULL);

  /* "/NO_DBUSMENU" is what some apps send for no menu */
  if (item->menu_path == NULL || g_strcmp0 (item->menu_path, "/NO_DBUSMENU") == 0)
    return NULL;

  return item->menu_path;
}



gboolean
shopno_qs_tray_item_get_is_menu (ShopnoQsTrayItem *item)
{
  g_return_val_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item), FALSE);

  return item->item_is_menu;
}



void
shopno_qs_tray_item_activate (ShopnoQsTrayItem *item,
                              gint              x,
                              gint              y)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item));

  shopno_qs_tray_item_call (item, "Activate", g_variant_new ("(ii)", x, y));
}



void
shopno_qs_tray_item_secondary_activate (ShopnoQsTrayItem *item,
                                        gint              x,
                                        gint              y)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item));

  shopno_qs_tray_item_call (item, "SecondaryActivate", g_variant_new ("(ii)", x, y));
}



void
shopno_qs_tray_item_context_menu (ShopnoQsTrayItem *item,
                                  gint              x,
                                  gint              y)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item));

  shopno_qs_tray_item_call (item, "ContextMenu", g_variant_new ("(ii)", x, y));
}



void
shopno_qs_tray_item_scroll (ShopnoQsTrayItem *item,
                            gint              delta,
                            GtkOrientation    orientation)
{
  g_return_if_fail (SHOPNO_IS_QS_TRAY_ITEM (item));

  shopno_qs_tray_item_call (item, "Scroll",
                            g_variant_new ("(is)", delta,
                                           orientation == GTK_ORIENTATION_HORIZONTAL
                                           ? "horizontal" : "vertical"));
}
