#!/bin/sh
# Builds and runs the Linux auto-correction test (tests/linux.cpp). Needs g++ or clang++ and the libxkbcommon and
# libsystemd headers (the same as linux/build.sh); no desktop, no input devices.
#   tests/run-linux.sh            — run, compare with tests/baseline-linux.txt
#   tests/run-linux.sh --update   — accept the current results as the new baseline
#   tests/run-linux.sh -v         — print every result line
#   tests/run-linux.sh --dicts    — also use this machine's Hunspell dictionaries (not compared with the baseline)
set -e
cd "$(dirname "$0")/.."
mkdir -p build
CCACHE_DISABLE=1 ${CXX:-c++} -std=c++17 -O2 -Wa,-I"$PWD" tests/linux.cpp -o build/aura-test $(pkg-config --cflags --libs xkbcommon libsystemd) -ldl -lpthread
build/aura-test "$PWD" "$@"
