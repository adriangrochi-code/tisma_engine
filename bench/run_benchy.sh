#!/usr/bin/env bash
# Slices 3DBenchy (bench/3DBenchy.stl.xz, CC0 1.0, THIRD_PARTY.md) with the
# bench settings and prints the slicing time, the layers, the estimated print
# time and the filament. Usage: bench/run_benchy.sh [build dir] [settings.json]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:-$here/../build}"
settings="${2:-$here/../tests/data/settings.json}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
xz -dc "$here/3DBenchy.stl.xz" > "$work/3DBenchy.stl"
echo "6ab57f1c3f8e86bc3cbd302c6fa6270acf06277c6335454e922419c25d42e97e  $work/3DBenchy.stl" | sha256sum -c --quiet
for gpu in off auto; do
  printf '{"compute": {"gpu": "%s"}}\n' "$gpu" > "$work/gpu.json"
  start=$(date +%s%N)
  "$build/tisma-engine" slice "$work/3DBenchy.stl" --config "$settings" --config "$work/gpu.json" -o "$work/out.gcode" >/dev/null 2>&1
  end=$(date +%s%N)
  layers=$(grep -c '^;LAYER_CHANGE' "$work/out.gcode" || true)
  time=$(grep -m1 '^; estimated printing time =' "$work/out.gcode" | sed 's/.*= //')
  fil=$(grep -m1 '^; filament used \[mm\] =' "$work/out.gcode" | sed 's/.*= //')
  echo "gpu=$gpu slice=$(( (end - start) / 1000000 )) ms layers=$layers time=$time filament_mm=$fil"
done
