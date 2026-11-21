# AURA Switcher — what it does and doesn't do

> Українською — [what-aura-does.uk.md](what-aura-does.uk.md) · ← [README](../README.md)

## Trust: why it is not a keylogger
- **Open source (GPL-3.0).** Everything is in the public repo; you can read it and build it yourself.
- **Network only when you ask.** AURA goes online only when you click Menu → "Check for updates…". No background checks, no analytics, no accounts. Typed text is never sent anywhere or written to a log.
- **Password fields.** On Windows AURA does nothing when the focused field is a password field. On Mac it does not read or edit secure text fields. On Linux it cannot tell a password field: a password is checked only if it has a space in it, and AURA is paused while the screen is locked — see [Linux](linux.md).
- Full policy: [privacy page](https://klbk.github.io/aura-switcher/privacy/).

## When AURA leaves a word alone
AURA fixes a word only when it is **not a word** in the current layout but **is a word** in another one. So `userName`, `git` or `ls` stay as typed. A short Latin token whose Cyrillic twin is also a word (for example `rjl` → "код") can still be fixed; double ⇧ switches it back.

- **Terminals and special apps.** Menu → "Don't work in this app" adds the current app to exclusions; "Don't work in (recent apps)" picks an app you just left. Games pause AURA automatically.
- **Per-app language.** An app can be bound to a layout (e.g. Terminal → EN); auto-correction is then off there.

## Russian
Auto-correction and layout switching **work with the Russian layout**. Only the app's menu is not translated into Russian (a Russian system language shows the menu in Ukrainian).
