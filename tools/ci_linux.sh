#!/usr/bin/env bash
# The Linux checks of the CI, run inside the work session before every push
# (owner's decision, 2026-10-24: Linux builds internally to save time; on
# GitHub the Linux jobs only run by hand, Windows runs on every push).
# The same steps as .github/workflows/ci.yml: the generated SPIR-V, then gcc
# and clang in Release and gcc with ASan + UBSan in Debug, every test with a
# Vulkan device required (lavapipe when there is no GPU).
# Usage: tools/ci_linux.sh   (from anywhere; build directories are reused)
set -euo pipefail
cd "$(dirname "$0")/.."
export TISMA_REQUIRE_GPU=1

python3 tools/compile_shaders.py --check

configure() {
    local dir=$1
    shift
    [ -f "$dir/build.ninja" ] || cmake -S . -B "$dir" -G Ninja "$@"
}
configure build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
configure build-clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
configure build-asan -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all"

for dir in build build-clang build-asan; do
    echo "== $dir"
    cmake --build "$dir"
    ctest --test-dir "$dir" --output-on-failure -j "$(nproc)"
done
echo "Linux checks passed"
