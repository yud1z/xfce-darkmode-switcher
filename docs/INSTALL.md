# Installation

## Dependencies

On Debian, Ubuntu, Xubuntu, Linux Mint XFCE, or similar:

```sh
sudo apt install build-essential pkg-config libgtk-3-dev xfce4-panel-dev libxfce4panel-2.0-dev xfconf gsettings-desktop-schemas
```

Runtime tools used by the plugin:

- `xfconf-query` for Xfce theme and panel settings
- `gsettings` for modern app color-scheme hints
- `xfsettingsd` for GTK apps such as Thunar
- `xfce4-panel` for panel refresh after switching

## Build

```sh
make
```

## Recommended system install

Xfce 4.18 may not reliably discover user-local panel plugins. System install is recommended:

```sh
make clean
make all PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
sudo make install PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
xfce4-panel -r
```

If your distro does not use `/usr/lib/x86_64-linux-gnu`, use:

```sh
sudo make install PREFIX=/usr
xfce4-panel -r
```

## Add to panel

Open:

```text
Panel → Add New Items → Dark Mode Switcher
```

Or add it from the terminal:

```sh
xfce4-panel --add=darkmode-switcher
```

## User-local install

This may work on newer Xfce versions:

```sh
make install PREFIX="$HOME/.local"
xfce4-panel -r
```

If the item does not appear in the panel items list, use the system install above.

## Uninstall

Use the same `PREFIX` and `LIBDIR` that you used for install:

```sh
sudo make uninstall PREFIX=/usr LIBDIR=/usr/lib/x86_64-linux-gnu
xfce4-panel -r
```
