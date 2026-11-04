#!/usr/bin/env bash
# Writes the settings panel of the desktop interface as the app's assets
# (mobile_app.md, rule 7): `tisma --settings-layout` in English and Spanish,
# compacted. Run it after the engine's panel changes, with a host build of
# tisma (TISMA_BUILD_UI=ON): tools/settings_layout.sh path/to/tisma
set -euo pipefail
tisma=${1:?usage: settings_layout.sh path/to/tisma}
out="$(cd "$(dirname "$0")/.." && pwd)/app/src/main/assets/settings"
mkdir -p "$out"
for lang in en es; do
  "$tisma" --settings-layout - --lang "$lang" |
    python3 -c 'import json,sys; json.dump(json.load(sys.stdin), sys.stdout, ensure_ascii=False, separators=(",", ":"))' \
    > "$out/layout_$lang.json"
done
ls -l "$out"
