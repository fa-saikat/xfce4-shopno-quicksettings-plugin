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
]

# Everything except the panel glue, shared with the preview tool
shopno_qs_core = static_library(
  'shopno-qs-core',
  sources: [
    'shopno-qs-settings.c',
    'shopno-qs-tile.c',
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
