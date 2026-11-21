# AURA Switcher

> 🇺🇦  Designed and developed in Ukraine.
> Українська версія — [README.uk.md](README.uk.md)

A macOS & Windows app that fixes text typed in the wrong keyboard layout ("ghbdsn" → "привіт"). It lives in the menu bar, runs in the background and starts automatically at login. There is a Linux version too (beta, built from source) — [docs/linux.md](docs/linux.md).

Quick start — [docs/INSTALL-mac.md](docs/INSTALL-mac.md).

Website — https://klbk.github.io/aura-switcher/ · Support ☕ — https://donatello.to/aura-switcher

## Open source & support

- **Open source:** https://github.com/klbk/aura-switcher-app (GPL-3.0). The app sees everything you type, so anyone can check that it stores nothing and never sends what you type.
- **Free.** If AURA Switcher helps you, support development: **https://donatello.to/aura-switcher** ☕ (any country's card). The link is also on the website and in the app menu ("Support AURA Switcher ☕").
- **Download:** https://github.com/klbk/aura-switcher/releases/latest · Windows — [Microsoft Store](https://apps.microsoft.com/detail/9PP3MK8CFCJK)
- **Contact & bugs:** auraswitcher@gmail.com — or "Report a problem…" in the app menu (the email opens with version, system and layouts filled in).

## Features

- **Auto-correction.** Typed a word in the wrong layout and pressed Space — AURA erases the word, retypes it in the right layout and switches the layout.
- **Double ⇧** (two quick Shift taps):
  - text selected — converts it to the next layout (press again for the next one, in a cycle);
  - no selection — converts the last typed word;
  - nothing to convert — just switches the layout.
- **Exclusions.** AURA does nothing in the apps you choose.
- **Per-app language.** The layout is set automatically when you switch to an app (e.g. Terminal → EN). Auto-correction is off in apps with a bound language.
- **Single instance.** A second running copy of AURA quits immediately, so text is never corrected twice.
- **Games.** AURA pauses itself in games (double ⇧ is often sprint there): on Mac — apps in the Games category, on Windows — full-screen games, on Linux — full-screen Steam, Wine and "Game"-category apps. A game AURA didn't recognise can be excluded after leaving it: menu → "Don't work in (recent apps)". The pause covers the game's chat too: a word typed there in the wrong layout stays as typed.
- **Update in one click, only when you ask.** AURA goes online only when you click Menu → "Check for updates…": it reads the version file on klbk.github.io and, if you agree, downloads the new version from GitHub Releases, checks it (Mac: AURA's signature and version; Windows: the signed version file and the exe's SHA-256), puts it in place of the old one and restarts. Nothing you typed and no personal data is sent. If anything goes wrong, it offers the site's update page instead (its address holds only the version number and menu language). 30 days after the build (or after the last check) a dot appears next to the language code and the menu says "⬆︎ An update may be available — check".

## Languages

AURA uses **every layout enabled in macOS**: System Settings → Keyboard → Input Sources → "+". New layouts are picked up instantly.

| Feature | Works with |
|---|---|
| Double ⇧ | any regular keyboard layout |
| Auto-correction | 49 languages (the list is the `lang/` folder: Ukrainian, English, Russian, German, French, Spanish, Polish, Arabic, Hebrew, Greek…) — with the macOS dictionary, or AURA's own language model when the dictionary isn't there |
| Not supported | Japanese and other candidate-based input (not via a layout); auto-correction for Korean and Hindi — later |

**UK ↔ RU** is corrected: "міло" → "мыло", "вібор" → "выбор". If a word is spelled the same in both layouts ("привет"), the text stays and AURA only switches the layout. Words valid in both languages ("сир", "мі") are left alone — use double ⇧ for those.

Between Latin layouts (EN ↔ DE ↔ FR…) a word is corrected when it is a real word only in the other layout: "sch;n" → "schön". Accents typed with a dead key (´ then e) count as one letter.

## Menu bar

The icon shows the current language: `EN`, `УК`, `РУ`… — each language in its own script (`ΕΛ` for Greek, `DE` for German). Lowercase (`en`, `ук`) — AURA is off or paused in this app (exclusion, game). `⚠︎` — no access.

- **Enabled** — master switch.
- **Auto-correction** — on/off (double ⇧ always works).
- **Sound** — a short beep: one tone on auto-correction, two on double ⇧. Same on Mac and Windows.
- **Don't work in this app** — add the current app to exclusions.
- **Language for this app** — bind a layout.
- **Exclusions / Language bindings** — lists; click an item to remove it.
- **My words…** — words AURA treats as correct (slang like «хз», «ща», «скілі»): it never changes them, and fixes them when typed in the wrong layout (`crsks` → «скілі»). Comma-separated.
- **Launch at login**.
- **Menu language** — System or one of 48 languages (same as the site). "System" follows the OS language; Russian → Ukrainian; a language not in the list → English.
- **Quit**.

## Install and run

1. Build and install (also launches it):
   ```
   ./build.sh
   ```
   Requires Xcode or Command Line Tools (`swiftc`). Installs to `/Applications/AURA Switcher.app`.
2. Grant access (two permissions): System Settings → Privacy & Security → **Accessibility** → enable AURA, then **Input Monitoring** → enable AURA. Until then the menu shows `⚠︎`. After Input Monitoring macOS offers to restart AURA — agree (or quit and open it again). Updating from an older version: only Input Monitoring is new, once.
3. After that it starts at login. Manually — Spotlight (⌘Space) → "AURA".

Archive for other Macs: `./build.sh --release` → `dist/AURA-Switcher-<version>.zip`.

## Troubleshooting

- **Doesn't correct, icon shows `⚠︎`** — no access, see step 2 (both permissions). If access is on but nothing works: toggle AURA off and on in the Accessibility and Input Monitoring lists.
- **Wrong correction** — add the app to exclusions or turn auto-correction off; double ⇧ brings the word back.
- **Something hangs** — icon → "Quit", or `pkill "AURA Switcher"` in Terminal.
- **Uninstall** — "Quit", uncheck "Launch at login", delete `/Applications/AURA Switcher.app`.

## How it works, Windows and Linux

- [How it works](docs/how-it-works.md) — keystrokes, dictionaries, language models, files.
- [What AURA does and doesn't do](docs/what-aura-does.md) — why it is not a keylogger, when it leaves a word alone, Russian.
- [Windows version](docs/windows.md) — install, differences, limits (beta).
- [Linux version](docs/linux.md) — build, access to the keyboard, supported desktops, differences (beta).

## Limitations

- macOS 13+ and Windows 10/11; Linux — KDE Plasma (Wayland, X11) and other X11 desktops. On iPhone/iPad/Android only as a separate keyboard, with little practical value.
- Auto-correction only sees continuous typing: a mouse click, arrow keys or ⌘/⌥/⌃ shortcuts reset the current word.
- Password fields and some apps (terminals, games) may behave differently — add them to exclusions.

## Help with translations

The menu and the website are available in 48 languages. Most were translated by an AI and have not been proofread by native speakers — if you speak one of them, please help: see [issue #6](https://github.com/klbk/aura-switcher-app/issues/6) and the [`translations/`](translations/) folder (one file per language, a pull request or a comment is enough).

## Thanks

- **Andrew Vrublevskyi** ([@andreyrubleff](https://github.com/andreyrubleff)) — beta testing and the code review of 0.9.2 that made the one-click update safer (0.9.3).
- **mrCabez** ([@mrCabez](https://github.com/mrCabez)) — Italian translation proofread by a native speaker (menu, site, update page).

---
🇺🇦 AURA Switcher — made in Ukraine.
