#!/bin/zsh
# Cross-compiles AURA.exe for Windows 10/11 (x64 + ARM64) on macOS using Zig.
#   ./build.sh  →  dist/AURA-<version>-windows.zip
set -e
cd "$(dirname "$0")"
VERSION=0.4
ZIG=${ZIG:-zig}
rm -rf build && mkdir -p build/x64 build/arm64 ../dist
for T in x86_64:x64 aarch64:arm64; do
  "$ZIG" c++ -target ${T%%:*}-windows-gnu -O2 -s -std=c++17 -municode -Wl,--subsystem,windows \
    aura.cpp aura.rc -o build/${T##*:}/AURA.exe \
    -lole32 -lwinmm -luser32 -lshell32 -lgdi32 -ladvapi32 -static
done
rm -f build/*/*.pdb(N)
(cd build && zip -qr ../../dist/AURA-$VERSION-windows.zip x64 arm64)
rm -rf build
echo "dist/AURA-$VERSION-windows.zip"
