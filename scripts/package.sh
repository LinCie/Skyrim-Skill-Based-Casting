#!/usr/bin/env bash
set -euo pipefail

configuration="${1:-Release}"
if [[ "$configuration" != Debug && "$configuration" != Release ]]; then
    echo "Usage: ./scripts/package.sh [Debug|Release] [output.zip]" >&2
    exit 2
fi
if (($# > 2)); then
    echo "Usage: ./scripts/package.sh [Debug|Release] [output.zip]" >&2
    exit 2
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
config_lower="${configuration,,}"
dll_path="$project_root/build/ubuntu-$config_lower/$configuration/SkillBasedCasting.dll"
ini_path="$project_root/config/SkillBasedCasting.ini"
output_path="${2:-$project_root/dist/SkillBasedCasting.zip}"

if [[ ! -f "$dll_path" ]]; then
    echo "Plugin DLL not found: $dll_path. Run ./scripts/build.sh $configuration first." >&2
    exit 1
fi
if ! file "$dll_path" | grep -Eq 'PE32[+].*x86-64'; then
    echo "Refusing to package a non-Windows x64 PE32+ DLL: $dll_path" >&2
    file "$dll_path" >&2 || true
    exit 1
fi

mkdir -p "$(dirname "$output_path")"
python3 - "$dll_path" "$ini_path" "$output_path" <<'PY'
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
import sys

dll_path, ini_path, output_path = map(Path, sys.argv[1:])
if not ini_path.is_file():
    raise SystemExit(f"INI not found: {ini_path}")
with ZipFile(output_path, "w", compression=ZIP_DEFLATED) as archive:
    archive.write(dll_path, "SKSE/Plugins/SkillBasedCasting.dll")
    archive.write(ini_path, "SKSE/Plugins/SkillBasedCasting.ini")
print(f"Created {output_path}")
PY
