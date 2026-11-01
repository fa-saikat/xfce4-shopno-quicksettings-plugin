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
