/*
 * Copyright (C) 2026 FA Saikat
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2007-2010 Nick Schermer <nick@xfce.org>
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
 * The XEmbed system tray protocol: the tray owns a selection, and an app
 * sends it a dock request with the window to embed. Balloon messages are
 * not supported. Follows systray-manager.c of Xfce's systray plugin.
 */

#include "config.h"

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "shopno-qs-xembed.h"

#define OPCODE_REQUEST_DOCK    0
#define ORIENTATION_HORIZONTAL 0

enum
{
  DOCK,
  N_SIGNALS
};

struct _ShopnoQsXembed
{
  GObject    __parent__;

  GtkWidget *invisible;
  GdkAtom    selection_atom;
  Atom       opcode_atom;
  gboolean   owns;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsXembed, shopno_qs_xembed, G_TYPE_OBJECT)

static GdkFilterReturn shopno_qs_xembed_filter (GdkXEvent *gdk_xevent,
                                                GdkEvent  *event,
                                                gpointer   data);



static void
shopno_qs_xembed_unregister (ShopnoQsXembed *self)
{
  GdkWindow *window;

  if (self->invisible == NULL)
    return;

  window = gtk_widget_get_window (self->invisible);
  gdk_window_remove_filter (window, shopno_qs_xembed_filter, self);

  if (self->owns
      && gdk_selection_owner_get_for_display (gtk_widget_get_display (self->invisible),
                                              self->selection_atom) == window)
    gdk_selection_owner_set_for_display (gtk_widget_get_display (self->invisible), NULL,
                                         self->selection_atom, gdk_x11_get_server_time (window), TRUE);

  self->owns = FALSE;
  gtk_widget_destroy (self->invisible);
  g_clear_object (&self->invisible);
}



static GdkFilterReturn
shopno_qs_xembed_filter (GdkXEvent *gdk_xevent,
                         GdkEvent  *event,
                         gpointer   data)
{
  ShopnoQsXembed *self = data;
  XEvent         *xevent = gdk_xevent;

  if (xevent->type == ClientMessage
      && xevent->xclient.message_type == self->opcode_atom
      && xevent->xclient.data.l[1] == OPCODE_REQUEST_DOCK)
    {
      g_signal_emit (self, signals[DOCK], 0, (gulong) xevent->xclient.data.l[2]);
      return GDK_FILTER_REMOVE;
    }

  /* Another tray took over; apps dock with it now */
  if (xevent->type == SelectionClear)
    self->owns = FALSE;

  return GDK_FILTER_CONTINUE;
}



static void
shopno_qs_xembed_set_cardinal (GdkWindow   *window,
                               const gchar *name,
                               gulong       value)
{
  GdkDisplay *display = gdk_window_get_display (window);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                   gdk_x11_get_xatom_by_name_for_display (display, name),
                   g_strcmp0 (name, "_NET_SYSTEM_TRAY_VISUAL") == 0 ? XA_VISUALID : XA_CARDINAL,
                   32, PropModeReplace, (guchar *) &value, 1);
}



static void
shopno_qs_xembed_register (ShopnoQsXembed *self,
                           GdkScreen      *screen)
{
  GdkDisplay          *display = gdk_screen_get_display (screen);
  GdkWindow           *window;
  GdkVisual           *visual;
  gchar               *name;
  guint32              timestamp;
  XClientMessageEvent  xevent;
  Window               root;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d", gdk_screen_get_number (screen));
  G_GNUC_END_IGNORE_DEPRECATIONS
  self->selection_atom = gdk_atom_intern (name, FALSE);
  g_free (name);

  /* Leave the icons to a tray that is already running */
  if (XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
                          gdk_x11_atom_to_xatom_for_display (display, self->selection_atom)) != None)
    return;

  self->invisible = g_object_ref_sink (gtk_invisible_new_for_screen (screen));
  gtk_widget_realize (self->invisible);
  gtk_widget_add_events (self->invisible, GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);
  window = gtk_widget_get_window (self->invisible);

  /* Icons with an alpha channel, when the screen can show one */
  visual = gdk_screen_get_rgba_visual (screen);
  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);
  shopno_qs_xembed_set_cardinal (window, "_NET_SYSTEM_TRAY_VISUAL",
                                 XVisualIDFromVisual (GDK_VISUAL_XVISUAL (visual)));
  shopno_qs_xembed_set_cardinal (window, "_NET_SYSTEM_TRAY_ORIENTATION", ORIENTATION_HORIZONTAL);

  timestamp = gdk_x11_get_server_time (window);
  if (!gdk_selection_owner_set_for_display (display, window, self->selection_atom, timestamp, TRUE))
    {
      gtk_widget_destroy (self->invisible);
      g_clear_object (&self->invisible);
      return;
    }

  self->owns = TRUE;
  self->opcode_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_SYSTEM_TRAY_OPCODE");
  gdk_window_add_filter (window, shopno_qs_xembed_filter, self);

  /* Apps that started first wait for this to dock */
  root = RootWindowOfScreen (GDK_SCREEN_XSCREEN (screen));
  xevent.type = ClientMessage;
  xevent.window = root;
  xevent.message_type = gdk_x11_get_xatom_by_name_for_display (display, "MANAGER");
  xevent.format = 32;
  xevent.data.l[0] = timestamp;
  xevent.data.l[1] = gdk_x11_atom_to_xatom_for_display (display, self->selection_atom);
  xevent.data.l[2] = GDK_WINDOW_XID (window);
  xevent.data.l[3] = 0;
  xevent.data.l[4] = 0;
  XSendEvent (GDK_DISPLAY_XDISPLAY (display), root, False, StructureNotifyMask, (XEvent *) &xevent);
}



static void
shopno_qs_xembed_dispose (GObject *object)
{
  shopno_qs_xembed_unregister (SHOPNO_QS_XEMBED (object));

  G_OBJECT_CLASS (shopno_qs_xembed_parent_class)->dispose (object);
}



static void
shopno_qs_xembed_class_init (ShopnoQsXembedClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = shopno_qs_xembed_dispose;

  signals[DOCK] =
    g_signal_new ("dock", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ULONG);
}



static void
shopno_qs_xembed_init (ShopnoQsXembed *self)
{
}



ShopnoQsXembed *
shopno_qs_xembed_new (void)
{
  GdkScreen      *screen = gdk_screen_get_default ();
  ShopnoQsXembed *self;

  if (screen == NULL || !GDK_IS_X11_SCREEN (screen))
    return NULL;

  self = g_object_new (SHOPNO_TYPE_QS_XEMBED, NULL);
  shopno_qs_xembed_register (self, screen);

  return self;
}



gboolean
shopno_qs_xembed_get_owns (ShopnoQsXembed *xembed)
{
  g_return_val_if_fail (SHOPNO_IS_QS_XEMBED (xembed), FALSE);

  return xembed->owns;
}
