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

#include <glib/gi18n-lib.h>

#include "shopno-qs-features.h"
#include "shopno-qs-window.h"

/* Width of the flyout at 100% scale, as on Windows 11 */
#define WINDOW_WIDTH 360
#define TILE_COLUMNS 3

/* Detail pages scroll beyond this height */
#define PAGE_MAX_HEIGHT 400

#define TILE_ID_KEY "shopno-qs-tile-id"

/* Window roles, matched by the picom rules shipped in extras/picom */
#define ROLE_PLAIN "shopno-quicksettings"
#define ROLE_BLUR  "shopno-quicksettings-blur"

/* The pointer is often still grabbed by the panel button when the flyout
 * maps, so retry the seat grab a few times before giving up */
#define GRAB_RETRY_INTERVAL 50
#define GRAB_MAX_RETRIES    10

enum
{
  PROP_0,
  PROP_SETTINGS,
  N_PROPERTIES
};

struct _ShopnoQsWindow
{
  GtkWindow         __parent__;

  ShopnoQsSettings *settings;

  GtkWidget        *frame;
  GtkWidget        *stack;
  GtkWidget        *main_page;
  GtkWidget        *tiles;
  GtkWidget        *sliders;
  GtkWidget        *footer_start;
  GtkWidget        *footer_end;

  gboolean          grab_input;
  GdkSeat          *grab_seat;
  guint             grab_retry_id;
  guint             grab_retries;
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (ShopnoQsWindow, shopno_qs_window, GTK_TYPE_WINDOW)



static void
shopno_qs_window_load_css (GdkScreen *screen)
{
  static gboolean  loaded = FALSE;
  GtkCssProvider  *provider;

  if (loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/shopnoos/quicksettings/shopno-quicksettings.css");
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  loaded = TRUE;
}



static void
shopno_qs_window_update_visual (ShopnoQsWindow *self)
{
  GtkWidget       *widget = GTK_WIDGET (self);
  GdkScreen       *screen = gtk_widget_get_screen (widget);
  GdkVisual       *rgba_visual = gdk_screen_get_rgba_visual (screen);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  gboolean         composited = rgba_visual != NULL && gdk_screen_is_composited (screen);
  GdkVisual       *visual = composited ? rgba_visual : gdk_screen_get_system_visual (screen);

  /* The visual can only change while unrealized */
  if (visual != gtk_widget_get_visual (widget))
    {
      if (gtk_widget_get_realized (widget))
        gtk_widget_unrealize (widget);
      gtk_widget_set_visual (widget, visual);
    }

  gtk_widget_set_app_paintable (widget, composited);

  if (composited)
    gtk_style_context_add_class (context, "composited");
  else
    gtk_style_context_remove_class (context, "composited");
}



/* Windows 11 lightens hovered items in both light and dark mode, which a
 * single rule based on theme colors cannot do. Tell the CSS which one the
 * theme is. */
static void
shopno_qs_window_style_updated (GtkWidget *widget)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GdkRGBA          bg;
  gboolean         dark = FALSE;

  GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->style_updated (widget);

  if (gtk_style_context_lookup_color (context, "theme_bg_color", &bg))
    dark = 0.2126 * bg.red + 0.7152 * bg.green + 0.0722 * bg.blue < 0.5;

  /* Changing a class updates the style again, so only change it when needed */
  if (dark != gtk_style_context_has_class (context, "dark"))
    {
      if (dark)
        gtk_style_context_add_class (context, "dark");
      else
        gtk_style_context_remove_class (context, "dark");
    }
}



static void
shopno_qs_window_update_blur (ShopnoQsWindow *self)
{
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gboolean         blur = shopno_qs_settings_get_enable_blur (self->settings);

  /* With blur on, picom draws the blur, shadow and rounded corners.
   * Otherwise the window manager's compositor draws the shadow. */
  gtk_window_set_role (GTK_WINDOW (self), blur ? ROLE_BLUR : ROLE_PLAIN);

  if (blur)
    gtk_style_context_add_class (context, "blur");
  else
    gtk_style_context_remove_class (context, "blur");
}



/* Hidden tiles leave no gap: the flow box moves the others up */
static void
shopno_qs_window_update_hidden_tiles (ShopnoQsWindow *self)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (self->tiles));

  for (GList *l = children; l != NULL; l = l->next)
    {
      const gchar *id = g_object_get_data (G_OBJECT (l->data), TILE_ID_KEY);

      gtk_widget_set_visible (l->data, !shopno_qs_settings_is_tile_hidden (self->settings, id));
    }

  g_list_free (children);
}



/*
 * Input grab
 */

static gboolean
shopno_qs_window_try_grab (gpointer data)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (data);
  GdkWindow      *window = gtk_widget_get_window (GTK_WIDGET (self));
  GdkSeat        *seat;

  seat = gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (self)));

  /* owner_events is TRUE so widgets of this process, including the panel
   * button, keep receiving their own events while the flyout is open */
  if (gdk_seat_grab (seat, window, GDK_SEAT_CAPABILITY_ALL, TRUE,
                     NULL, NULL, NULL, NULL) == GDK_GRAB_SUCCESS)
    {
      self->grab_seat = seat;
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  if (++self->grab_retries >= GRAB_MAX_RETRIES)
    {
      g_warning ("Failed to grab input, the flyout will close on focus loss only");
      self->grab_retry_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}



static void
shopno_qs_window_ungrab (ShopnoQsWindow *self)
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



/*
 * GtkWidget
 */

static gboolean
shopno_qs_window_map_event (GtkWidget   *widget,
                            GdkEventAny *event)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (widget);

  if (self->grab_input)
    {
      self->grab_retries = 0;
      if (shopno_qs_window_try_grab (self) == G_SOURCE_CONTINUE)
        self->grab_retry_id = g_timeout_add (GRAB_RETRY_INTERVAL, shopno_qs_window_try_grab, self);
    }

  if (GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->map_event != NULL)
    return GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->map_event (widget, event);

  return GDK_EVENT_PROPAGATE;
}



static void
shopno_qs_window_hide (GtkWidget *widget)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (widget);

  shopno_qs_window_ungrab (self);

  GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->hide (widget);

  /* Open on the main page next time, like Windows does */
  gtk_stack_set_visible_child_full (GTK_STACK (self->stack), "main", GTK_STACK_TRANSITION_TYPE_NONE);
}



static gboolean
shopno_qs_window_button_press_event (GtkWidget      *widget,
                                     GdkEventButton *event)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (widget);
  GtkAllocation   alloc;
  gint            x, y;

  /* With the seat grabbed, clicks anywhere on screen arrive here. Close the
   * flyout when the click lands outside the visible frame. */
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

  if (GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->button_press_event != NULL)
    return GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->button_press_event (widget, event);

  return GDK_EVENT_PROPAGATE;
}



static gboolean
shopno_qs_window_key_press_event (GtkWidget   *widget,
                                  GdkEventKey *event)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (widget);

  if (event->keyval == GDK_KEY_Escape)
    {
      /* Step back to the main page first, then close */
      if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "main") != 0)
        shopno_qs_window_show_main (self);
      else
        gtk_widget_hide (widget);

      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->key_press_event (widget, event);
}



/* xfwm4 does not give keyboard focus to menu-type windows; the seat grab
 * delivers the input instead. GTK would then draw the whole flyout in the
 * dimmed backdrop style of an unfocused window, which a menu never is. */
static gboolean
shopno_qs_window_window_state_event (GtkWidget           *widget,
                                     GdkEventWindowState *event)
{
  gboolean result = GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->window_state_event (widget, event);

  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_BACKDROP);

  return result;
}



static gboolean
shopno_qs_window_focus_out_event (GtkWidget     *widget,
                                  GdkEventFocus *event)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (widget);

  /* Fallback for when the input grab failed */
  if (self->grab_input && self->grab_seat == NULL)
    gtk_widget_hide (widget);

  return GTK_WIDGET_CLASS (shopno_qs_window_parent_class)->focus_out_event (widget, event);
}



/*
 * GObject
 */

static void
shopno_qs_window_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
shopno_qs_window_constructed (GObject *object)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  G_OBJECT_CLASS (shopno_qs_window_parent_class)->constructed (object);

  if (self->settings == NULL)
    self->settings = shopno_qs_settings_new ();

  g_signal_connect_swapped (self->settings, "notify::enable-blur",
                            G_CALLBACK (shopno_qs_window_update_blur), self);
  g_signal_connect_swapped (self->settings, "notify::hidden-tiles",
                            G_CALLBACK (shopno_qs_window_update_hidden_tiles), self);
  shopno_qs_window_update_blur (self);

  shopno_qs_features_attach (self);
}



static void
shopno_qs_window_dispose (GObject *object)
{
  ShopnoQsWindow *self = SHOPNO_QS_WINDOW (object);

  shopno_qs_window_ungrab (self);

  if (self->settings != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->settings, self);
      g_clear_object (&self->settings);
    }

  G_OBJECT_CLASS (shopno_qs_window_parent_class)->dispose (object);
}



static void
shopno_qs_window_class_init (ShopnoQsWindowClass *klass)
{
  GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = shopno_qs_window_set_property;
  gobject_class->constructed = shopno_qs_window_constructed;
  gobject_class->dispose = shopno_qs_window_dispose;

  widget_class->map_event = shopno_qs_window_map_event;
  widget_class->hide = shopno_qs_window_hide;
  widget_class->button_press_event = shopno_qs_window_button_press_event;
  widget_class->key_press_event = shopno_qs_window_key_press_event;
  widget_class->focus_out_event = shopno_qs_window_focus_out_event;
  widget_class->window_state_event = shopno_qs_window_window_state_event;
  widget_class->style_updated = shopno_qs_window_style_updated;

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL, SHOPNO_TYPE_QS_SETTINGS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, properties);
}



static void
shopno_qs_window_init (ShopnoQsWindow *self)
{
  GtkWindow *window = GTK_WINDOW (self);
  GtkWidget *footer;

  self->grab_input = TRUE;

  shopno_qs_window_load_css (gtk_widget_get_screen (GTK_WIDGET (self)));

  gtk_window_set_title (window, _("Quick Settings"));
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  gtk_window_stick (window);
  /* Same type as Shopno Menu, which xfwm4 gives a regular menu shadow */
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_MENU);
  gtk_widget_add_events (GTK_WIDGET (self), GDK_BUTTON_PRESS_MASK);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "shopno-qs-window");

  shopno_qs_window_update_visual (self);

  self->frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (self->frame, WINDOW_WIDTH, -1);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->frame), "shopno-qs-frame");
  gtk_container_add (GTK_CONTAINER (self), self->frame);

  /* The main page and the detail pages share the space above the footer */
  self->stack = gtk_stack_new ();
  gtk_stack_set_transition_duration (GTK_STACK (self->stack), 150);
  gtk_stack_set_vhomogeneous (GTK_STACK (self->stack), FALSE);
  gtk_stack_set_interpolate_size (GTK_STACK (self->stack), TRUE);
  gtk_box_pack_start (GTK_BOX (self->frame), self->stack, TRUE, TRUE, 0);

  self->main_page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->main_page), "shopno-qs-main");
  gtk_stack_add_named (GTK_STACK (self->stack), self->main_page, "main");

  /* Quick action tiles, three to a row. A flow box reflows when tiles
   * are hidden. */
  self->tiles = gtk_flow_box_new ();
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->tiles), TRUE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self->tiles), TILE_COLUMNS);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->tiles), TILE_COLUMNS);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->tiles), GTK_SELECTION_NONE);
  gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (self->tiles), FALSE);
  gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (self->tiles), 12);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (self->tiles), 22);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->tiles), "shopno-qs-tiles");
  gtk_box_pack_start (GTK_BOX (self->main_page), self->tiles, FALSE, FALSE, 0);

  self->sliders = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->sliders), "shopno-qs-sliders");
  gtk_box_pack_start (GTK_BOX (self->main_page), self->sliders, FALSE, FALSE, 0);

  /* Footer band: battery on the left, settings on the right */
  footer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (footer), "shopno-qs-footer");
  gtk_box_pack_end (GTK_BOX (self->frame), footer, FALSE, FALSE, 0);

  self->footer_start = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (footer), self->footer_start, FALSE, FALSE, 0);
  self->footer_end = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_end (GTK_BOX (footer), self->footer_end, FALSE, FALSE, 0);

  gtk_widget_show_all (self->frame);
}



GtkWidget *
shopno_qs_window_new (ShopnoQsSettings *settings)
{
  return g_object_new (SHOPNO_TYPE_QS_WINDOW, "settings", settings, NULL);
}



void
shopno_qs_window_popup (ShopnoQsWindow *window,
                        gint            x,
                        gint            y)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  /* The compositor may have been toggled since the last popup */
  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    shopno_qs_window_update_visual (window);

  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_window_present_with_time (GTK_WINDOW (window), gtk_get_current_event_time ());
}



/* Disabling the grab is only meant for the preview tool, so that a flyout
 * opened on a developer's desktop does not capture all input */
void
shopno_qs_window_set_grab_input (ShopnoQsWindow *window,
                                 gboolean        grab_input)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  window->grab_input = grab_input;
}



void
shopno_qs_window_add_tile (ShopnoQsWindow *window,
                           const gchar    *id,
                           GtkWidget      *tile)
{
  GtkWidget *child;

  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));
  g_return_if_fail (id != NULL);

  /* The flow box child only places the tile; the tile handles focus */
  child = gtk_flow_box_child_new ();
  gtk_widget_set_can_focus (child, FALSE);
  g_object_set_data_full (G_OBJECT (child), TILE_ID_KEY, g_strdup (id), g_free);
  gtk_container_add (GTK_CONTAINER (child), tile);
  gtk_widget_show (child);
  gtk_widget_set_no_show_all (child, TRUE);
  gtk_container_add (GTK_CONTAINER (window->tiles), child);

  shopno_qs_window_update_hidden_tiles (window);
}



void
shopno_qs_window_add_slider (ShopnoQsWindow *window,
                             GtkWidget      *slider)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  gtk_box_pack_start (GTK_BOX (window->sliders), slider, FALSE, FALSE, 0);
}



void
shopno_qs_window_add_footer (ShopnoQsWindow *window,
                             GtkWidget      *widget,
                             gboolean        end)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  if (end)
    gtk_box_pack_end (GTK_BOX (window->footer_end), widget, FALSE, FALSE, 0);
  else
    gtk_box_pack_start (GTK_BOX (window->footer_start), widget, FALSE, FALSE, 0);
}



void
shopno_qs_window_add_page (ShopnoQsWindow *window,
                           const gchar    *name,
                           const gchar    *title,
                           GtkWidget      *content)
{
  GtkWidget *page;
  GtkWidget *header;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *scroll;

  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));
  g_return_if_fail (g_strcmp0 (name, "main") != 0);

  page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (page), "shopno-qs-page");

  header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "shopno-qs-page-header");
  gtk_box_pack_start (GTK_BOX (page), header, FALSE, FALSE, 0);

  button = gtk_button_new_from_icon_name ("pan-start-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (button, _("Back"));
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (shopno_qs_window_show_main), window);
  gtk_box_pack_start (GTK_BOX (header), button, FALSE, FALSE, 0);

  label = gtk_label_new (title);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "shopno-qs-page-title");
  gtk_box_pack_start (GTK_BOX (header), label, TRUE, TRUE, 0);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroll), TRUE);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroll), PAGE_MAX_HEIGHT);
  gtk_container_add (GTK_CONTAINER (scroll), content);
  gtk_box_pack_start (GTK_BOX (page), scroll, TRUE, TRUE, 0);

  gtk_widget_show_all (page);
  gtk_stack_add_named (GTK_STACK (window->stack), page, name);
}



void
shopno_qs_window_show_page (ShopnoQsWindow *window,
                            const gchar    *name)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  gtk_stack_set_visible_child_full (GTK_STACK (window->stack), name,
                                    GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT);
}



void
shopno_qs_window_show_main (ShopnoQsWindow *window)
{
  g_return_if_fail (SHOPNO_IS_QS_WINDOW (window));

  gtk_stack_set_visible_child_full (GTK_STACK (window->stack), "main",
                                    GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT);
}



ShopnoQsSettings *
shopno_qs_window_get_settings (ShopnoQsWindow *window)
{
  g_return_val_if_fail (SHOPNO_IS_QS_WINDOW (window), NULL);

  return window->settings;
}
