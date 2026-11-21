# AURA Switcher — how it works

> Українською — [how-it-works.uk.md](how-it-works.uk.md) · ← [README](../README.md)

- Two Swift files: `engine.swift` (layouts, auto-correction, double Shift) and `main.swift` (menu, app), build script `build.sh` next to them; tests in `tests/` (`tests/run.sh` for Mac, `tests/run-windows.sh` for Windows, `tests/run-linux.sh` for Linux, all in CI). Public repo https://github.com/klbk/aura-switcher-app. No external dependencies. Universal build: Apple Silicon + Intel.
- Keystrokes are read with a **listen-only** `CGEventTap`: the system never waits for AURA, so it can't freeze the keyboard or mouse.
- Character tables are built from the macOS layouts themselves (`UCKeyTranslate`), so any layout works without manual setup.
- Word checking uses the system spell checker (`NSSpellChecker`), after the Space has already been typed. A dictionary macOS hasn't downloaded accepts any string, so each one is checked on nonsense first.
- Where there is no working dictionary, AURA uses its own language models (`lang/`, built by `tools/langmodel.py`): frequent words and letter trigrams. Data: [FrequencyWords](https://github.com/hermitdave/FrequencyWords) by Hermit Dave and Belarusian Wikipedia, both CC BY-SA 4.0.
- Website language flags (`site/flags/`): [Twemoji](https://github.com/jdecked/twemoji) graphics, © Twitter, Inc. and other contributors, CC-BY 4.0.
- Replacement uses synthetic keystrokes: Backspace × word length, then the corrected text.
- Selected text comes from the Accessibility API, or via ⌘C with clipboard restore if the app doesn't expose it.
- Settings are stored in `UserDefaults` (`com.chernov.aura`).
