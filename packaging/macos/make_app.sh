#!/bin/bash
# Tisma.app from the release folder (PLAN.md, «macOS antes de la beta»;
# app_updates.md, rule 13: the update package is the .zip with the app).
# Usage: packaging/macos/make_app.sh <package folder> <version>
# The programs go in Contents/MacOS, the rest of the folder in
# Contents/Resources. Unsigned for now. Runs on macOS (sips, iconutil).
set -euo pipefail
package="$1"
version="$2"
here="$(cd "$(dirname "$0")" && pwd)"
app="$package/Tisma.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
mv "$package/tisma" "$package/tisma-engine" "$app/Contents/MacOS/"
sed "s/@VERSION@/$version/g" "$here/Info.plist.in" > "$app/Contents/Info.plist"
# The icon, every size iconutil wants, from the 512 px one.
iconset="$(mktemp -d)/tisma.iconset"
mkdir -p "$iconset"
src="$here/../../assets/brand/app/tisma_512px.png"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$src" --out "$iconset/icon_${size}x${size}.png" > /dev/null
  double=$((size * 2))
  if [ "$double" -le 512 ]; then
    sips -z "$double" "$double" "$src" --out "$iconset/icon_${size}x${size}@2x.png" > /dev/null
  fi
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/tisma.icns"
# Everything else in the folder (licenses, docs) beside the app, and a copy
# inside it so the .zip update carries them.
for item in "$package"/*; do
  [ "$item" = "$app" ] && continue
  cp -R "$item" "$app/Contents/Resources/"
done
