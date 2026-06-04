#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
DESKTOP_FILE="${1:-$HOME/.local/share/applications/Navicat.Premium.17.desktop}"
INSTALL_BIN="$HOME/.local/bin/navicat-darkmode-wrapper"
INSTALL_DATA_DIR="$HOME/.local/share/xfce-darkmode-switcher"
INSTALL_QSS="$INSTALL_DATA_DIR/navicat-dark.qss"

if [[ ! -f "$DESKTOP_FILE" ]]; then
    echo "Navicat desktop file not found: $DESKTOP_FILE" >&2
    echo "Pass the desktop file path as the first argument, or install Navicat first." >&2
    exit 1
fi

mkdir -p "$HOME/.local/bin" "$INSTALL_DATA_DIR"
install -m 755 "$SCRIPT_DIR/navicat-darkmode-wrapper.sh" "$INSTALL_BIN"
install -m 644 "$SCRIPT_DIR/navicat-dark.qss" "$INSTALL_QSS"

backup="$DESKTOP_FILE.bak.$(date +%Y%m%d%H%M%S)"
cp "$DESKTOP_FILE" "$backup"

python3 - "$DESKTOP_FILE" "$INSTALL_BIN" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
wrapper = sys.argv[2]
lines = p.read_text().splitlines()
out = []
changed = False
for line in lines:
    if line.startswith('Exec='):
        out.append(f'Exec={wrapper}')
        changed = True
    else:
        out.append(line)
if not changed:
    out.append(f'Exec={wrapper}')
p.write_text('\n'.join(out) + '\n')
PY

update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true

echo "Installed Navicat dark-mode wrapper."
echo "Desktop file backup: $backup"
echo "Wrapper: $INSTALL_BIN"
echo "Stylesheet: $INSTALL_QSS"
