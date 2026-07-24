# Alpine/postmarketOS install and documentation design

## Context

The project is an Xfce panel plugin built with `make`, GTK 3, and `libxfce4panel-2.0`. The target system is postmarketOS edge (`ID_LIKE=alpine`) running an active Xfce session. The installed Xfce panel plugin directories are `/usr/lib/xfce4/panel/plugins` and `/usr/share/xfce4/panel/plugins`.

## Goals

- Install the plugin into the local Xfce environment on Alpine/postmarketOS.
- Use Alpine package names and library paths instead of Debian multiarch paths.
- Document Alpine/postmarketOS installation and verification steps.
- Note the Alpine/postmarketOS support in release notes.

## Approach

Use system installation with `PREFIX=/usr LIBDIR=/usr/lib`, which places the shared library and desktop metadata where Xfce on Alpine/postmarketOS already looks for panel plugins. Install missing dependencies through `apk` with passwordless `sudo`, build from source, run `make install`, restart `xfce4-panel`, and add the plugin with `xfce4-panel --add=darkmode-switcher`.

## Documentation updates

Add an Alpine/postmarketOS dependency section to the README and installation guide. Update troubleshooting verification paths to mention both Debian multiarch and Alpine `/usr/lib` layouts. Update the unreleased changelog with Alpine/postmarketOS install documentation.

## Verification

- `pkg-config --modversion gtk+-3.0 libxfce4panel-2.0` succeeds.
- `make clean && make all PREFIX=/usr LIBDIR=/usr/lib` succeeds.
- Installed files exist at `/usr/lib/xfce4/panel/plugins/libdarkmode-switcher.so` and `/usr/share/xfce4/panel/plugins/darkmode-switcher.desktop`.
- `xfce4-panel --add=darkmode-switcher` is attempted in the active Xfce session.
