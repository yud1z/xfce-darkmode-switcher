# Changelog

## Unreleased

### Added

- qt5ct/qt6ct palette switching for Qt apps.

## v0.1.0 - 2026-06-04

Initial public release.

### Added

- Xfce panel plugin for one-click Light/Dark switching.
- Properties dialog to select light and dark GTK/Xfce themes.
- Theme discovery from user and system theme directories.
- Xfce GTK theme switching through `xfconf-query`.
- Xfce panel dark-mode switching through `/panels/dark-mode`.
- Panel reload after toggle for reliable repainting on Xfce 4.18.
- Optional xfwm4 window-border theme switching when theme assets exist.
- Modern app dark-mode hints through GNOME `gsettings` color-scheme.
- GTK 3/4 `settings.ini` updates.
- Automatic `xfsettingsd` startup if missing, so apps like Thunar receive theme changes.
- qt5ct/qt6ct palette switching for Qt apps.
- Standalone GTK preview/test target: `make test-ui`.
