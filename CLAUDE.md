# AURA Switcher

Menu-bar app for macOS + Windows: fixes text typed in the wrong keyboard layout. Public repo (GPL-3.0) — nothing private in commits.

## Layout
- `engine.swift` — layout-detection logic (shared by app and tests); `main.swift` — mac app/UI; `updater.swift` — update check.
- `windows/` — Win32 C++ version (`aura.cpp`), cross-built on Mac with Zig; `windows/store` — Microsoft Store.
- `lang/` — language models (built by `tools/langmodel.py`); `translations/`, `tools/menu_i18n.py` — menu strings.
- `site/` — klbk.github.io/aura-switcher; `docs/` — install/usage texts (uk + en); `promo/` — promo assets.

## Commands
- Mac build + install: `./build.sh`; release zip: `./build.sh --release`.
- Windows build: `windows/build.sh`.
- Tests: `tests/run.sh` (compares with `tests/baseline-*.txt`, `--update` accepts new results). Windows tests: `tests/run-windows.sh` (Windows/CI only).

## Rules
- Version is set in `build.sh` and `windows/build.sh` — bump both together.
- After changing `engine.swift` run `tests/run.sh`; don't accept a new baseline without showing what changed.
- Stats, site publishing, Store — skill `aura-ops`; release — `mac-app-release`, `windows-from-mac`; testing — `keyboard-app-testing`.
