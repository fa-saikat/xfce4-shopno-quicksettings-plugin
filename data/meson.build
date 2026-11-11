i18n.merge_file(
  input: 'shopno-quicksettings.desktop.in',
  output: 'shopno-quicksettings.desktop',
  type: 'desktop',
  po_dir: meson.project_source_root() / 'po',
  install: true,
  install_dir: datadir / 'xfce4' / 'panel' / 'plugins',
)

# The ShopnoOS logo for now, until the plugin has its own icon
install_data(
  'icons/shopno-quicksettings.svg',
  install_dir: datadir / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

# Monitor brightness over DDC/CI: /dev/i2c-* for graphics cards, opened to
# the active session. These live under /usr/lib even on multiarch systems.
install_data(
  '60-shopno-quicksettings-i2c.rules',
  install_dir: prefix / 'lib' / 'udev' / 'rules.d',
)

install_data(
  'shopno-quicksettings-i2c.conf',
  install_dir: prefix / 'lib' / 'modules-load.d',
)
