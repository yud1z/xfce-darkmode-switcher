# Installation

## Dependencies

On Debian, Ubuntu, Xubuntu, Linux Mint XFCE, or similar:

```sh
sudo apt install build-essential pkg-config libgtk-3-dev xfce4-panel-dev libxfce4panel-2.0-dev xfconf gsettings-desktop-schemas
```

On Alpine Linux, postmarketOS, or similar:

```sh
sudo apk add build-base pkgconf gtk+3.0-dev xfce4-panel-dev xfconf gsettings-desktop-schemas
```

The required pkg-config modules are `gtk+-3.0` and `libxfce4panel-2.0`.

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

On Alpine Linux or postmarketOS, Xfce panel plugins are normally discovered under `/usr/lib`, so use:

```sh
make clean
make all PREFIX=/usr LIBDIR=/usr/lib
sudo make install PREFIX=/usr LIBDIR=/usr/lib
xfce4-panel -r
```

If another distro does not use `/usr/lib/x86_64-linux-gnu`, use the panel plugin library directory for that distro. For many non-multiarch systems this is:

```sh
sudo make install PREFIX=/usr LIBDIR=/usr/lib
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

On Alpine Linux or postmarketOS:

```sh
sudo make uninstall PREFIX=/usr LIBDIR=/usr/lib
xfce4-panel -r
```
