resources = gnome.compile_resources(
  'shopno-qs-resources',
  'resources/shopno-quicksettings.gresource.xml',
  source_dir: 'resources',
  c_name: 'shopno_qs',
)

shopno_qs_deps = [
  gtk,
  gio_unix,
  libxfce4panel,
  libxfce4ui,
  libxfce4util,
  libxfconf,
  libpulse,
  libpulse_glib,
  libnm,
  dbusmenu,
  x11,
  xrandr,
  libm,
]

# Everything except the panel glue, shared with the preview tool
shopno_qs_core = static_library(
  'shopno-qs-core',
  sources: [
    'shopno-qs-audio.c',
    'shopno-qs-battery.c',
    'shopno-qs-bluetooth.c',
    'shopno-qs-bluetooth-ui.c',
    'shopno-qs-brightness.c',
    'shopno-qs-dialog.c',
    'shopno-qs-features.c',
    'shopno-qs-dnd.c',
    'shopno-qs-ethernet.c',
    'shopno-qs-footer.c',
    'shopno-qs-gamma.c',
    'shopno-qs-network.c',
    'shopno-qs-overflow.c',
    'shopno-qs-night-light.c',
    'shopno-qs-settings.c',
    'shopno-qs-slider.c',
    'shopno-qs-status.c',
    'shopno-qs-tile.c',
    'shopno-qs-tray.c',
    'shopno-qs-tray-box.c',
    'shopno-qs-tray-icon.c',
    'shopno-qs-tray-item.c',
    'shopno-qs-util.c',
    'shopno-qs-volume.c',
    'shopno-qs-wifi.c',
    'shopno-qs-window.c',
    resources,
  ],
  include_directories: config_inc,
  dependencies: shopno_qs_deps,
  gnu_symbol_visibility: 'hidden',
  pic: true,
)

shared_module(
  'shopno-quicksettings',
  sources: [
    'shopno-qs-plugin.c',
  ],
  include_directories: config_inc,
  dependencies: shopno_qs_deps,
  link_whole: shopno_qs_core,
  gnu_symbol_visibility: 'hidden',
  install: true,
  install_dir: libdir / 'xfce4' / 'panel' / 'plugins',
)

shopno_qs_inc = include_directories('.')
