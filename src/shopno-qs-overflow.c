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

#include "shopno-qs-overflow.h"
#include "shopno-qs-tray-box.h"

#define COLUMNS   4
#define ICON_SIZE 20

/* The pointer is often still grabbed by the panel button when the popup
 * maps, so retry the seat grab a few times */
#define GRAB_RETRY_INTERVAL 50
#define GRAB_MAX_RETRIES    10

enum
{
  CHANGED,
  N_SIGNALS
};

struct _ShopnoQsOverflow
{
  GtkWindow  __parent__;

  GtkWidget *frame;
  GtkWidget *box;

  GdkSeat   *grab_seat;
  guint      grab_retry_id;
  guint      grab_retries;
  gint       x, y;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (ShopnoQsOverflow, shopno_qs_overflow, GTK_TYPE_WINDOW)



static gboolean
shopno_qs_overflow_try_grab (gpointer data)
{
  ShopnoQsOverflow *self = data;
  GdkSeat          *seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));

  if (gdk_seat_grab (seat, gtk_widget_get_window (GTK_WIDGET (self)), GDK_SEAT_CAPABILITY_ALL,
                     TRUE, NULL, NULL, NULL, NULL) == GDK_GRAB_SUCCESS)
    {
      self->grab_seat = seat;
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (++self->grab_retries >= GRAB_MAX_RETRIES)
    {
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}



static void
shopno_qs_overflow_ungrab (ShopnoQsOverflow *self)
{
  if (self->grab_retry_id != 0)
    {
      g_source_remove (self->grab_retry_id);
      self->grab_retry_id = 0;
    }

  if (self->grab_seat != NULL)
    {
      gdk_seat_ungrab (self->grab_seat);
      self->grab_seat = NULL;
    }
}



static void
shopno_qs_overflow_box_changed (ShopnoQsOverflow *self)
{
  /* The last hidden icon went to the panel or away */
  if (shopno_qs_overflow_get_n_icons (self) == 0)
    gtk_widget_hide (GTK_WIDGET (self));

  g_signal_emit (self, signals[CHANGED], 0);
}



static void
shopno_qs_overflow_icon_activated (ShopnoQsOverflow *self)
{
  /* Close first: the app may open a window or its own menu */
  gtk_widget_hide (GTK_WIDGET (self));
}



/*
 * GtkWidget
 */

static gboolean
shopno_qs_overflow_map_event (GtkWidget   *widget,
                              GdkEventAny *event)
{
  ShopnoQsOverflow *self = SHOPNO_QS_OVERFLOW (widget);

  /* xfwm4 ignores moves made while the window was hidden */
  gtk_window_move (GTK_WINDOW (self), self->x, self->y);

  self->grab_retries = 0;
  if (shopno_qs_overflow_try_grab (self) == G_SOURCE_CONTINUE)
    self->grab_retry_id = g_timeout_add (GRAB_RETRY_INTERVAL, shopno_qs_overflow_try_grab, self);

  return GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->map_event (widget, event);
}



static void
shopno_qs_overflow_hide (GtkWidget *widget)
{
  shopno_qs_overflow_ungrab (SHOPNO_QS_OVERFLOW (widget));

  GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->hide (widget);
}



static gboolean
shopno_qs_overflow_button_press_event (GtkWidget      *widget,
                                       GdkEventButton *event)
{
  ShopnoQsOverflow *self = SHOPNO_QS_OVERFLOW (widget);
  GtkAllocation     alloc;
  gint              x, y;

  /* With the seat grabbed, clicks anywhere arrive here; close on one
   * outside the frame */
  gdk_window_get_origin (gtk_widget_get_window (widget), &x, &y);
  gtk_widget_get_allocation (self->frame, &alloc);
  x += alloc.x;
  y += alloc.y;

  if (event->x_root < x || event->x_root >= x + alloc.width
      || event->y_root < y || event->y_root >= y + alloc.height)
    {
      gtk_widget_hide (widget);
      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->button_press_event (widget, event);
}



static gboolean
shopno_qs_overflow_key_press_event (GtkWidget   *widget,
                                    GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_hide (widget);
      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->key_press_event (widget, event);
}



static gboolean
shopno_qs_overflow_focus_out_event (GtkWidget     *widget,
                                    GdkEventFocus *event)
{
  /* Fallback for when the grab failed */
  if (SHOPNO_QS_OVERFLOW (widget)->grab_seat == NULL)
    gtk_widget_hide (widget);

  return GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->focus_out_event (widget, event);
}



/* A popup is never drawn in the dimmed style of an unfocused window */
static gboolean
shopno_qs_overflow_window_state_event (GtkWidget           *widget,
                                       GdkEventWindowState *event)
{
  gboolean result = GTK_WIDGET_CLASS (shopno_qs_overflow_parent_class)->window_state_event (widget, event);

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);

  return result;
}



/*
 * GObject
 */

static void
shopno_qs_overflow_dispose (GObject *object)
{
  shopno_qs_overflow_ungrab (SHOPNO_QS_OVERFLOW (object));

  G_OBJECT_CLASS (shopno_qs_overflow_parent_class)->dispose (object);
}



static void
shopno_qs_overflow_class_init (ShopnoQsOverflowClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = shopno_qs_overflow_dispose;

  widget_class->map_event = shopno_qs_overflow_map_event;
  widget_class->hide = shopno_qs_overflow_hide;
  widget_class->button_press_event = shopno_qs_overflow_button_press_event;
  widget_class->key_press_event = shopno_qs_overflow_key_press_event;
  widget_class->focus_out_event = shopno_qs_overflow_focus_out_event;
  widget_class->window_state_event = shopno_qs_overflow_window_state_event;

  signals[CHANGED] =
    g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}



static void
shopno_qs_overflow_init (ShopnoQsOverflow *self)
{
  GtkWindow *window = GTK_WINDOW (self);
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (self));
  GdkVisual *visual = gdk_screen_get_rgba_visual (screen);

  gtk_window_set_title (window, "Tray Overflow");
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  gtk_window_stick (window);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_MENU);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_PRESS_MASK);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-window");

  /* Rounded like the quick settings flyout when composited */
  if (visual != NULL && gdk_screen_is_composited (screen))
    {
      gtk_widget_set_visual (GTK_WIDGET (self), visual);
      gtk_widget_set_app_paintable (GTK_WIDGET (self), TRUE);
      gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "composited");
    }

  self->frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-qs-frame");
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-qs-overflow");
  gtk_container_add (GTK_CONTAINER (self), self->frame);
}



GtkWidget *
shopno_qs_overflow_new (ShopnoQsSettings *settings)
{
  ShopnoQsOverflow *self = g_object_new (SHOPNO_TYPE_QS_OVERFLOW, "type", GTK_WINDOW_TOPLEVEL, NULL);

  self->box = shopno_qs_tray_box_new (settings, FALSE);
  shopno_qs_tray_box_set_columns (SHOPNO_QS_TRAY_BOX (self->box), COLUMNS);
  shopno_qs_tray_box_set_icon_size (SHOPNO_QS_TRAY_BOX (self->box), ICON_SIZE);
  g_signal_connect_swapped (self->box, "changed", G_CALLBACK (shopno_qs_overflow_box_changed), self);
  g_signal_connect_swapped (self->box, "icon-activated", G_CALLBACK (shopno_qs_overflow_icon_activated), self);
  gtk_box_pack_start (GTK_BOX (self->frame), self->box, FALSE, FALSE, 0);

  gtk_widget_show_all (self->frame);

  return GTK_WIDGET (self);
}



guint
shopno_qs_overflow_get_n_icons (ShopnoQsOverflow *overflow)
{
  g_return_val_if_fail (SHOPNO_IS_QS_OVERFLOW (overflow), 0);

  return shopno_qs_tray_box_get_n_shown (SHOPNO_QS_TRAY_BOX (overflow->box));
}



void
shopno_qs_overflow_popup (ShopnoQsOverflow *overflow,
                          gint              x,
                          gint              y)
{
  g_return_if_fail (SHOPNO_IS_QS_OVERFLOW (overflow));

  overflow->x = x;
  overflow->y = y;
  gtk_window_move (GTK_WINDOW (overflow), x, y);
  gtk_window_present_with_time (GTK_WINDOW (overflow), gtk_get_current_event_time ());
}
