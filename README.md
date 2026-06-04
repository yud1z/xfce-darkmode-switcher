# Xfce Dark Mode Switcher

An Xfce panel plugin that toggles between a selected light theme and dark theme.
It also updates Xfce panel dark mode and publishes modern desktop color-scheme hints
so apps such as Thunar, Chrome/Chromium, Firefox, GTK4/libadwaita apps, and other
GSettings-aware apps can follow dark mode.

## Features

- One-click Light/Dark toggle from the Xfce panel.
- Properties dialog to choose the Light and Dark Xfce/GTK themes.
- Automatically lists themes from:
  - `~/.themes`
  - `~/.local/share/themes`
  - system XDG theme directories such as `/usr/share/themes`
- Toggles Xfce panel dark mode: `/panels/dark-mode`.
- Reloads `xfce4-panel` after toggling so the panel repaints reliably on Xfce 4.18.
- Starts `xfsettingsd` if missing so GTK apps such as Thunar can receive theme changes.
- Optional xfwm4 window-border theme switching when the selected theme includes `xfwm4` assets.
- Sets modern-app hints through:
  - `org.gnome.desktop.interface color-scheme`
  - `org.gnome.desktop.interface gtk-theme`
  - GTK 3/4 `settings.ini`
  - qt5ct/qt6ct palettes for Qt apps

## Quick install

Install dependencies:

```sh
sudo apt install build-essential pkg-config libgtk-3-dev xfce4-panel-dev libxfce4panel-2.0-dev xfconf gsettings-desktop-schemas
```

Build and install system-wide:

```sh
make clean
make all PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
sudo make install PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
xfce4-panel -r
```

Add it from:

```text
Panel → Add New Items → Dark Mode Switcher
```

Or from terminal:

```sh
xfce4-panel --add=darkmode-switcher
```

See [docs/INSTALL.md](docs/INSTALL.md) for more install options.

## Usage

- Click the panel icon to toggle Light/Dark mode.
- Right-click the plugin and open **Properties** to choose the light and dark themes.
- If a theme does not appear in the dropdown, install it into `~/.themes` or `~/.local/share/themes`, or type the theme name manually.

## How it works

When switching to dark mode, the plugin applies the selected dark theme and sets:

```sh
xfconf-query -c xsettings -p /Net/ThemeName -s '<dark-theme>'
xfconf-query -c xfce4-panel -p /panels/dark-mode -s true
gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
```

When switching to light mode, it applies the selected light theme and reverts the color-scheme hint.

For GTK apps such as Thunar, `xfsettingsd` must be running. The plugin starts it automatically if missing.
Qt apps using qt5ct/qt6ct get their palette files updated too. Some apps, especially AppImages such as Navicat, may still need to be restarted before they fully adopt the new mode.

## Test preview without installing

```sh
make test-ui
./darkmode-switcher-test
./darkmode-switcher-test --properties
```

## Troubleshooting

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

For Navicat AppImage, see [docs/NAVICAT.md](docs/NAVICAT.md).

## Release notes

See [CHANGELOG.md](CHANGELOG.md).
