#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_dir=${XDG_DATA_HOME:-"$HOME/.local/share"}
config_dir=${XDG_CONFIG_HOME:-"$HOME/.config"}

mkdir -p "$data_dir/applications" "$data_dir/mime/packages" "$data_dir/solid/actions" "$config_dir"
cp "$script_dir/photo-cd-player.desktop" "$data_dir/applications/photo-cd-player.desktop"
cp "$script_dir/x-kodak-photo-cd.xml" "$data_dir/mime/packages/x-kodak-photo-cd.xml"
cp "$script_dir/kodak-photo-cd-solid-action.desktop" "$data_dir/solid/actions/kodak-photo-cd-viewer.desktop"
update-mime-database "$data_dir/mime"
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$data_dir/applications"
fi
python3 - "$config_dir/mimeapps.list" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
lines = path.read_text(encoding="utf-8").splitlines() if path.exists() else []
section = "[Default Applications]"
key = "x-content/x-kodak-photo-cd="
start = next((i for i, line in enumerate(lines) if line.strip() == section), None)
if start is None:
    if lines and lines[-1].strip():
        lines.append("")
    lines.extend([section, key + "photo-cd-player.desktop;"])
else:
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines))
    existing = next((i for i in range(start + 1, end) if lines[i].startswith(key)), None)
    if existing is None:
        lines.insert(end, key + "photo-cd-player.desktop;")
    else:
        lines[existing] = key + "photo-cd-player.desktop;"
path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
PY

printf '%s\n' "Installed user-level Photo CD opening integration in $data_dir."
printf '%s\n' "KDE Plasma also has an Open with Kodak Photo CD Viewer action in Disks & Devices."
printf '%s\n' "Opening on insertion remains subject to the desktop environment's removable-media policy."
