#!/bin/zsh
# Cross-compiles "AURA Switcher.exe" for Windows 10/11 (x64 + ARM64) on macOS using Zig.
#   ./build.sh  →  ../dist/AURA-Switcher-<version>-windows.zip (both) + -windows-x64.zip (for users)
set -e
cd "$(dirname "$0")"
NAME="AURA Switcher"
FILE=AURA-Switcher
VERSION=0.5
ZIG=${ZIG:-$HOME/.local/bin/zig}
rm -rf build && mkdir -p build/x64 build/arm64 ../dist
for T in x86_64:x64 aarch64:arm64; do
  "$ZIG" c++ -target ${T%%:*}-windows-gnu -O2 -s -std=c++17 -municode -Wl,--subsystem,windows \
    aura.cpp aura.rc -o "build/${T##*:}/$NAME.exe" \
    -lole32 -lwinmm -luser32 -lshell32 -lgdi32 -ladvapi32 -static
done
rm -f build/*/*.pdb(N)
rm -f ../dist/$FILE-$VERSION-windows.zip ../dist/$FILE-$VERSION-windows-x64.zip
(cd build && zip -qr ../../dist/$FILE-$VERSION-windows.zip x64 arm64)
# x64-only archive for regular users: one folder, one exe
mkdir -p "build/pkg/$NAME" && cp "build/x64/$NAME.exe" "build/pkg/$NAME/"
cp ../docs/install-windows.uk.txt "build/pkg/$NAME/Як встановити.txt"
cp ../docs/install-windows.en.txt "build/pkg/$NAME/How to install.txt"
# python zipfile marks non-ASCII names as UTF-8, so Windows shows «Як встановити.txt» correctly (zip doesn't)
(cd build/pkg && python3 -I -c 'import sys,zipfile,os
z=zipfile.ZipFile(sys.argv[1],"w",zipfile.ZIP_DEFLATED,compresslevel=9)
for r,_,fs in os.walk(sys.argv[2]):
  for f in sorted(fs): z.write(os.path.join(r,f))
z.close()' ../../../dist/$FILE-$VERSION-windows-x64.zip "$NAME")
rm -rf build
echo "dist/$FILE-$VERSION-windows-x64.zip"
