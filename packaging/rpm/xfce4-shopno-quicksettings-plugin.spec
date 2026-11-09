Name:           xfce4-shopno-quicksettings-plugin
Version:        0.3.0
Release:        1%{?dist}
Summary:        Windows 11 style quick settings for the Xfce panel

License:        GPL-2.0-or-later
URL:            https://github.com/fa-saikat/xfce4-shopno-quicksettings-plugin
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  meson
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(dbusmenu-gtk3-0.4)
BuildRequires:  pkgconfig(libnm)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libpulse-mainloop-glib)
BuildRequires:  pkgconfig(libxfce4panel-2.0)
BuildRequires:  pkgconfig(libxfce4ui-2)
BuildRequires:  pkgconfig(libxfce4util-1.0)
BuildRequires:  pkgconfig(libxfconf-0)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xrandr)

Requires:       xfce4-panel
Recommends:     NetworkManager
Recommends:     bluez
Recommends:     upower
Recommends:     xfce4-notifyd
Suggests:       blueman
Suggests:       nm-connection-editor
Suggests:       pavucontrol
Suggests:       picom
Suggests:       xfce4-power-manager

%description
Shopno Quick Settings is a panel plugin for Xfce modelled on the Windows 11
quick settings flyout.

It has tiles for Wi-Fi, Bluetooth, Do not disturb and Night light, sliders
for screen brightness and volume, the battery level, and network, volume and
battery icons on the panel button. It also shows other apps' tray icons
(StatusNotifierItem), with an overflow flyout for hidden ones, and replaces
Xfce's System Tray plugin. It talks to NetworkManager, BlueZ,
PulseAudio or PipeWire, UPower and logind directly, sets the brightness of
external monitors over DDC/CI, and follows the GTK and icon theme.

%prep
%autosetup

%build
%meson -Dpreview=false
%meson_build

%install
%meson_install
# Add %%find_lang and "%%files -f %%{name}.lang" once po/LINGUAS lists a
# language. Until then rpm rejects the empty list.

%files
%license COPYING
%doc README.md extras/picom/shopno-quicksettings.conf
%{_libdir}/xfce4/panel/plugins/libshopno-quicksettings.so
%{_datadir}/xfce4/panel/plugins/shopno-quicksettings.desktop
%{_datadir}/icons/hicolor/scalable/apps/shopno-quicksettings.svg
%{_prefix}/lib/udev/rules.d/60-shopno-quicksettings-i2c.rules
%{_prefix}/lib/modules-load.d/shopno-quicksettings-i2c.conf

%changelog
* Wed Nov 04 2026 FA Saikat <fahimabrar.saikat@gmail.com> - 0.3.0-1
- Tray icons of older apps that use the XEmbed tray, such as ibus on
  Xfce, Wine and Java programs
- Each tray app can be shown on the panel, behind ^, or not at all;
  nm-applet is not shown by default
- Ethernet tile to disconnect or connect a wired connection, shown while
  a cable is plugged in
- Hiding a tile no longer unpins tray icons until the panel restarts

* Tue Nov 03 2026 FA Saikat <fahimabrar.saikat@gmail.com> - 0.2.0-1
- System tray: other apps' StatusNotifierItem icons next to the quick
  settings button, with their menus; replaces Xfce's System Tray plugin
- Overflow flyout behind ^ for icons not pinned to the panel; choose the
  pinned ones in Properties
- Volume notification with a level bar while scrolling over the button
- Wheel scrolling on the panel button and tray icons with smooth-scrolling
  mice
- Focus assist renamed to Do not disturb, with a bell icon
- Overflow flyout keeps its place, shrinks when an app quits, and stays
  open while an icon's menu is shown

* Sun Nov 01 2026 FA Saikat <fahimabrar.saikat@gmail.com> - 0.1.0-1
- Initial release
- Wi-Fi tile and network page: switch the radio, connect with a password,
  disconnect
- Bluetooth tile and device page: switch the adapter, lifting an rfkill
  block, and connect or disconnect paired devices
- Focus assist (Do Not Disturb of xfce4-notifyd) and Night light tiles
- Brightness slider for laptop screens, through logind
- Volume slider with mute and an output device page
- Battery level and a settings button in the footer
- Network, volume and battery icons on the panel button; scroll for volume
- Properties: tiles to show, Night light temperature, background blur
