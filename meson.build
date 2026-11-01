project(
  'xfce4-shopno-quicksettings-plugin',
  'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.62.0',
  default_options: [
    'c_std=gnu11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
)

gnome = import('gnome')
i18n = import('i18n')

gettext_package = meson.project_name()

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')

glib = dependency('glib-2.0', version: '>= 2.70.0')
gio_unix = dependency('gio-unix-2.0', version: '>= 2.70.0')
gtk = dependency('gtk+-3.0', version: '>= 3.24.0')
libxfce4panel = dependency('libxfce4panel-2.0', version: '>= 4.18.0')
libxfce4ui = dependency('libxfce4ui-2', version: '>= 4.18.0')
libxfce4util = dependency('libxfce4util-1.0', version: '>= 4.18.0')
libxfconf = dependency('libxfconf-0', version: '>= 4.18.0')
libpulse = dependency('libpulse', version: '>= 15.0')
libpulse_glib = dependency('libpulse-mainloop-glib', version: '>= 15.0')

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', gettext_package)
conf.set_quoted('PACKAGE_LOCALE_DIR', localedir)
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

config_inc = include_directories('.')

add_project_arguments(
  '-DG_LOG_DOMAIN="shopno-quicksettings"',
  '-Wno-unused-parameter',
  language: 'c',
)

subdir('po')
subdir('data')
subdir('src')

if get_option('preview')
  subdir('tools')
endif

gnome.post_install(gtk_update_icon_cache: true)
