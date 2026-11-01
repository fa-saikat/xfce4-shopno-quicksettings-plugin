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

#include "shopno-qs-features.h"



void
shopno_qs_features_attach (ShopnoQsWindow *window)
{
  /* Tiles */
  shopno_qs_wifi_attach (window);
  shopno_qs_bluetooth_attach (window);
  shopno_qs_focus_attach (window);
  shopno_qs_night_light_attach (window);

  /* Sliders */
  shopno_qs_brightness_attach (window);
  shopno_qs_volume_attach (window);
}
