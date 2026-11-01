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

/*
 * Night light through XRandR gamma ramps, the way redshift does it in
 * its randr method. Running both at once makes them fight over the
 * ramps, so turn redshift off when using Night light.
 */

#include "config.h"

#include <math.h>

#include <gdk/gdkx.h>
#include <X11/extensions/Xrandr.h>

#include "shopno-qs-gamma.h"



/* White point of a black body, approximated per channel (Tanner
 * Helland's fit of the CIE data), as 0.0 to 1.0 */
static void
shopno_qs_gamma_white_point (guint    temperature,
                             gdouble *red,
                             gdouble *green,
                             gdouble *blue)
{
  gdouble t = temperature / 100.0;
  gdouble r, g, b;

  if (t <= 66.0)
    {
      r = 255.0;
      g = 99.4708025861 * log (t) - 161.1195681661;
    }
  else
    {
      r = 329.698727446 * pow (t - 60.0, -0.1332047592);
      g = 288.1221695283 * pow (t - 60.0, -0.0755148492);
    }

  if (t >= 66.0)
    b = 255.0;
  else if (t <= 19.0)
    b = 0.0;
  else
    b = 138.5177312231 * log (t - 10.0) - 305.0447927307;

  *red = CLAMP (r, 0.0, 255.0) / 255.0;
  *green = CLAMP (g, 0.0, 255.0) / 255.0;
  *blue = CLAMP (b, 0.0, 255.0) / 255.0;
}



gboolean
shopno_qs_gamma_supported (GdkDisplay *display)
{
  Display *xdisplay;
  gint     event_base, error_base, major = 0, minor = 0;

  if (!GDK_IS_X11_DISPLAY (display))
    return FALSE;

  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  /* Per-CRTC gamma needs RandR 1.2 */
  return XRRQueryExtension (xdisplay, &event_base, &error_base)
         && XRRQueryVersion (xdisplay, &major, &minor)
         && (major > 1 || (major == 1 && minor >= 2));
}



void
shopno_qs_gamma_set (GdkDisplay *display,
                     guint       temperature)
{
  Display            *xdisplay;
  XRRScreenResources *resources;
  gdouble             red = 1.0, green = 1.0, blue = 1.0;

  if (!shopno_qs_gamma_supported (display))
    return;

  if (temperature < SHOPNO_QS_GAMMA_NEUTRAL)
    shopno_qs_gamma_white_point (temperature, &red, &green, &blue);

  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  resources = XRRGetScreenResourcesCurrent (xdisplay, DefaultRootWindow (xdisplay));
  if (resources == NULL)
    return;

  gdk_x11_display_error_trap_push (display);

  for (gint i = 0; i < resources->ncrtc; i++)
    {
      RRCrtc    crtc = resources->crtcs[i];
      gint      size = XRRGetCrtcGammaSize (xdisplay, crtc);
      XRRCrtcGamma *gamma;

      if (size < 2)
        continue;

      gamma = XRRAllocGamma (size);
      for (gint j = 0; j < size; j++)
        {
          gdouble level = (gdouble) j / (size - 1) * 65535.0;

          gamma->red[j] = (gushort) (level * red);
          gamma->green[j] = (gushort) (level * green);
          gamma->blue[j] = (gushort) (level * blue);
        }

      XRRSetCrtcGamma (xdisplay, crtc, gamma);
      XRRFreeGamma (gamma);
    }

  gdk_x11_display_error_trap_pop_ignored (display);
  XRRFreeScreenResources (resources);
}
