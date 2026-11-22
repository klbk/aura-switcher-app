#!/bin/sh
# Fails if the menu strings generated into engine.swift and windows/aura.cpp differ from tools/menu_i18n.py
# (someone edited a generated block by hand, or changed the generator without running it). Needs only Python 3.
set -e
cd "$(dirname "$0")/.."
python3 tools/menu_i18n.py --check
