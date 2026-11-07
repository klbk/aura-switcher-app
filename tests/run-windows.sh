#!/bin/bash
# Builds and runs the Windows auto-correction test (tests/windows.cpp). Needs Windows (CI: windows-latest) and Zig.
#   tests/run-windows.sh            — run, compare with tests/baseline-windows.txt
#   tests/run-windows.sh --update   — accept the current results as the new baseline
#   tests/run-windows.sh -v         — print every result line
set -e
cd "$(dirname "$0")/.."
mkdir -p build
(cd windows && zig c++ -target x86_64-windows-gnu -O2 -std=c++17 -municode \
  ../tests/windows.cpp aura.rc -o ../build/aura-test.exe \
  -lole32 -lwinmm -luser32 -lshell32 -lgdi32 -ladvapi32 -lwinhttp -lbcrypt -lcomctl32 -static)
build/aura-test.exe "$(pwd -W 2>/dev/null || pwd)" "$@"
