# Navicat AppImage dark-mode support

Navicat Premium Lite 17 for Linux is distributed as an AppImage with bundled Qt.
On Xfce, it may ignore normal system dark-mode mechanisms such as:

- GTK theme
- `gsettings org.gnome.desktop.interface color-scheme`
- `qt5ct` / `qt6ct`

During testing, Navicat inherited `QT_QPA_PLATFORMTHEME=qt5ct`, but its AppImage
restricted Qt plugin search paths to `/tmp/.mount_navicat.../usr/plugins`, so the
system `libqt5ct.so` platform theme was not loaded.

## Working workaround

Navicat does accept Qt's `-stylesheet` argument. This project includes a wrapper
that launches Navicat with a dark QSS stylesheet only when desktop dark mode is active.

Install the wrapper:

```sh
./scripts/install-navicat-wrapper.sh
```

It patches:

```text
~/.local/share/applications/Navicat.Premium.17.desktop
```

and creates:

```text
~/.local/bin/navicat-darkmode-wrapper
~/.local/share/xfce-darkmode-switcher/navicat-dark.qss
```

Restart Navicat after switching modes. Existing Navicat windows will not live-refresh.

## Manual test

```sh
/home/$USER/Downloads/navicat17-premium-lite-en-x86_64.AppImage \
  -stylesheet ~/.local/share/xfce-darkmode-switcher/navicat-dark.qss
```

## Undo

The installer creates a timestamped backup next to the desktop file, for example:

```text
~/.local/share/applications/Navicat.Premium.17.desktop.bak.YYYYMMDDHHMMSS
```

Restore it with:

```sh
cp ~/.local/share/applications/Navicat.Premium.17.desktop.bak.YYYYMMDDHHMMSS \
   ~/.local/share/applications/Navicat.Premium.17.desktop
```
