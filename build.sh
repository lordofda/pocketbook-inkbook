#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sdk="${POCKETBOOK_SDK_CMAKE:-/opt/pocketbook-build/pocketbook-sdk-qt6/cmake}"

cmake -S "$root" -B "$root/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$sdk/pocketbook.toolchain.cmake" \
  -DPOCKETBOOK_SDK_CMAKE="$sdk"

cmake --build "$root/build"
mkdir -p "$root/dist"
cp "$root/build/komga-reader.app" "$root/dist/"
file "$root/dist/komga-reader.app"
