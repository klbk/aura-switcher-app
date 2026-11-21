# AURA Switcher

Menu-bar app for macOS + Windows + Linux: fixes text typed in the wrong keyboard layout. Public repo (GPL-3.0) — nothing private in commits.

## Layout
- `engine.swift` — layout-detection logic (shared by app and tests); `main.swift` — mac app/UI; `updater.swift` — update check.
- `windows/` — Win32 C++ version (`aura.cpp`), cross-built on Mac with Zig; `windows/store` — Microsoft Store.
- `linux/` — Linux C++ version (`aura.cpp`: the Windows engine with evdev/uinput, libxkbcommon and D-Bus around it; KDE Plasma and X11 desktops), built on Linux; `70-aura-switcher.rules` — udev rule for keyboard access.
- `lang/` — language models, 49 languages (built by `tools/langmodel.py`; word lists from FrequencyWords, or Wikipedia via `tools/wikiwords.py`; the file name is the code the OS reports for the layout, e.g. `nb`). `tools/menu_i18n.py` generates the menu strings (48 languages) in `engine.swift`, `windows/aura.cpp` and `linux/aura.cpp` — edit it, don't edit the generated blocks. `translations/<code>.md` — proofreading templates for AI-written translations.
- `site/` — klbk.github.io/aura-switcher; `docs/` — topic docs (`how-it-works`, `windows`, `INSTALL-mac`, each uk + en) and the `.txt` texts that go into the zips (`install-*`, `usage`); `promo/` — promo assets.

## Commands
- Mac build + install: `./build.sh`; release zip: `./build.sh --release`.
- Windows build: `windows/build.sh`.
- Linux build (on Linux): `linux/build.sh`, `--install` puts it into `~/.local/bin`.
- Tests: `tests/run.sh` (compares with `tests/baseline-*.txt`, `--update` accepts new results). Windows tests: `tests/run-windows.sh` (Windows/CI only). Linux tests: `tests/run-linux.sh` (Linux/CI only; baseline `tests/baseline-linux.txt` is the language models alone).

## Rules
- Version is set in `build.sh`, `windows/build.sh` and `linux/build.sh` — bump all three together.
- Engine logic (`autoFixTarget` and around) lives three times: `engine.swift`, `windows/aura.cpp`, `linux/aura.cpp` — the Linux one mirrors the Windows one function for function; change them together. A new `lang/` file also goes into `AURA_LANGS` in `linux/aura.cpp` (and `windows/aura.rc`).
- After changing `engine.swift` run `tests/run.sh`; don't accept a new baseline without showing what changed.
- Mac event tap is **listen-only** on purpose: with an active tap, removing AURA from Accessibility makes macOS hold the keyboard and clicks for a minute or more, and the app cannot detect it (`AXIsProcessTrusted()` keeps answering yes from a cache). Don't go back to an active tap. Two permissions: Accessibility (editing text) and Input Monitoring (listening).
- Mac builds are signed with the stable self-signed identity "AURA Switcher" (`build.sh`), so macOS keeps the permissions across updates; a new certificate would reset them for every user.
- When testing anything that revokes permissions, run a background failsafe (`sleep N; pkill -9 -x "AURA Switcher"`). In zsh call `/usr/bin/log`, not `log`.
- Stats, site publishing, Store — skill `aura-ops`; release — `mac-app-release`, `windows-from-mac`; testing — `keyboard-app-testing`.

## Docs & release order
- Docs chain: README (uk/en pair) → one topic file in `docs/` (max one level deeper). Keep files short; split rather than grow. Every doc has a uk and an en twin that link to each other.
- Numbers that go stale — version, language count (49 in `lang/`, 48 menu languages), beta label — live in `build.sh`, `windows/build.sh`, `linux/build.sh`, `lang/`; grep `0.10`, `49`, `48`, `alpha` across README, `docs/`, `translations/`, `site/` when bumping.
- Release order: tests green on the release commit (CI + local + Mac live + Windows live) → both builds → `tools/update_manifest.py <version>` → GitHub release (tag `v<version>-beta`) → site (`site/latest.js`, badges) → Store. Nothing public without the user's yes.
- Project history and decisions are in the Obsidian vault (`AURA Switcher/`, hub note `AURA Switcher.md`), not in this repo.
