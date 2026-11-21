#!/bin/sh
# Builds "aura-switcher" for Linux (on Linux, for this machine's architecture).
#   linux/build.sh            →  build/aura-switcher
#   linux/build.sh --install  →  also copies it to ~/.local/bin and adds the app-menu entry and icon
# Needs a C++17 compiler (g++ or clang++), pkg-config and the headers of libxkbcommon and libsystemd (sd-bus);
# with the libX11 headers the X11 backend is built in too. Access to the keyboard: docs/linux.md.
set -e
cd "$(dirname "$0")/.."   # the repository root
VERSION=0.10.2
LIBS="xkbcommon libsystemd"
X11=""
if pkg-config --exists x11; then LIBS="$LIBS x11"; X11="-DAURA_X11"; fi
mkdir -p build
# -Wa,-I: the assembler finds lang/*.txt (built into the binary) from any directory; ccache can't see those files
CCACHE_DISABLE=1 ${CXX:-c++} -std=c++17 -O2 -s -Wa,-I"$PWD" -DAPP_VERSION="L\"$VERSION\"" $X11 linux/aura.cpp -o build/aura-switcher \
  $(pkg-config --cflags --libs $LIBS) -ldl -lpthread
echo "build/aura-switcher${X11:+ (with X11)}"
if [ "$1" = "--install" ]; then
  BIN="$HOME/.local/bin"; DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
  mkdir -p "$BIN" "$DATA/applications" "$DATA/icons/hicolor/scalable/apps"
  install -m 755 build/aura-switcher "$BIN/aura-switcher"
  install -m 644 icon.svg "$DATA/icons/hicolor/scalable/apps/aura-switcher.svg"
  cat > "$DATA/applications/aura-switcher.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=AURA Switcher
Comment=Fixes text typed in the wrong keyboard layout
Exec="$BIN/aura-switcher"
Icon=aura-switcher
Categories=Utility;
DESKTOP
  echo "installed: $BIN/aura-switcher (check: aura-switcher --check)"
fi
