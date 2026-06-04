#!/usr/bin/env bash
set -euo pipefail

# Wrapper for Navicat AppImage. Navicat bundles Qt and ignores system qt5ct,
# but it accepts Qt's -stylesheet argument. This wrapper applies a dark QSS
# only when the desktop is currently in dark mode.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
QSS_PATH="${NAVICAT_DARK_QSS:-$HOME/.local/share/xfce-darkmode-switcher/navicat-dark.qss}"

find_navicat_appimage() {
    if [[ -n "${NAVICAT_APPIMAGE:-}" && -x "${NAVICAT_APPIMAGE}" ]]; then
        printf '%s\n' "${NAVICAT_APPIMAGE}"
        return 0
    fi

    local desktop="$HOME/.local/share/applications/Navicat.Premium.17.desktop"
    if [[ -f "$desktop" ]]; then
        local exec_line
        exec_line="$(grep -E '^Exec=' "$desktop" | head -n1 | sed 's/^Exec=//')"
        # shellcheck disable=SC2086
        set -- $exec_line
        if [[ -n "${1:-}" && -x "$1" && "$1" != *navicat-darkmode-wrapper* ]]; then
            printf '%s\n' "$1"
            return 0
        fi
    fi

    local candidate
    candidate="$(find "$HOME/Downloads" "$HOME" -maxdepth 2 -type f -name 'navicat*.AppImage' -perm -u+x 2>/dev/null | sort | head -n1 || true)"
    if [[ -n "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
    fi

    echo "Could not find Navicat AppImage. Set NAVICAT_APPIMAGE=/path/to/navicat.AppImage" >&2
    return 1
}

is_dark_mode() {
    if command -v gsettings >/dev/null 2>&1; then
        if gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null | grep -q "prefer-dark"; then
            return 0
        fi
    fi
    if command -v xfconf-query >/dev/null 2>&1; then
        if [[ "$(xfconf-query -c xfce4-panel -p /panels/dark-mode 2>/dev/null || true)" == "true" ]]; then
            return 0
        fi
    fi
    return 1
}

APPIMAGE="$(find_navicat_appimage)"

if is_dark_mode && [[ -f "$QSS_PATH" ]]; then
    exec "$APPIMAGE" -stylesheet "$QSS_PATH" "$@"
else
    exec "$APPIMAGE" "$@"
fi
