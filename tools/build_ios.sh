#!/usr/bin/env bash
# The engine as a library for iOS (PLAN.md, «macOS antes de la beta»): the
# static engine with its C entry tisma_run_cli (include/tisma/tisma_engine.h),
# built for iPhones (arm64) and for the simulator on Apple silicon (arm64),
# each merged with oneTBB and Clipper2 into one archive, and both put into
# TismaEngine.xcframework. Without the GPU for now. Runs on macOS with Xcode.
# From iOS 16.3: the C++ library's floating-point to_chars starts there.
#
#   tools/build_ios.sh [out_dir]      (default: build-ios)
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-build-ios}"
mkdir -p "$out"
out="$(cd "$out" && pwd)"

build() { # $1 sdk (iphoneos / iphonesimulator)
  local dir="$out/$1"
  cmake -S "$root" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$1" -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.3 \
    -DTISMA_BUILD_TESTS=OFF -DTISMA_BUILD_UI=OFF -DTISMA_GPU=OFF \
    -DTBB_TEST=OFF -DTBB_EXAMPLES=OFF -DTBB_STRICT=OFF
  # -k 0: every error of a run at once, while the iOS build is brought up.
  cmake --build "$dir" --target tisma_core -- -k 0
  # One archive with the engine and what it links.
  local libs
  libs=$(find "$dir" -name 'libtisma_core.a' -o -name 'libtbb*.a' -o -name 'libClipper2*.a' | sort)
  echo "$1: merging" $libs
  libtool -static -o "$dir/libtisma_engine.a" $libs
}

build iphoneos
build iphonesimulator
rm -rf "$out/TismaEngine.xcframework"
xcodebuild -create-xcframework \
  -library "$out/iphoneos/libtisma_engine.a" -headers "$root/include" \
  -library "$out/iphonesimulator/libtisma_engine.a" -headers "$root/include" \
  -output "$out/TismaEngine.xcframework"
echo "Built $out/TismaEngine.xcframework"
