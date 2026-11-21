# AURA Switcher — Linux (beta, tested on KDE Plasma 5 and 6 Wayland, Xfce and Cinnamon)

> Українською — [linux.uk.md](linux.uk.md) · ← [README](../README.md)

- **Where it works.** KDE Plasma: tested live on Plasma 6.8 and 5.27 Wayland (Qt, GTK 3/4, Chromium and XWayland apps); Plasma on X11 uses the same code and should work, but has not been tried. Other desktops on X11 go through XKB: tested live on Xfce 4.18 and Cinnamon 6.0 (Ubuntu 24.04 in a virtual machine); MATE, LXQt and the like should work the same — testers welcome. Not yet: GNOME, Sway, Hyprland and other Wayland desktops — each needs its own way to read and switch the layout (the `desk*()` functions in `linux/aura.cpp`); there the icon shows `!`.
- **Build:** `linux/build.sh` → `build/aura-switcher`; `linux/build.sh --install` also puts it into `~/.local/bin` with an app-menu entry. Needs a C++17 compiler, `pkg-config`, headers of `libxkbcommon` and `libsystemd`, and `libX11` for the X11 backend:
  - Arch: `sudo pacman -S --needed base-devel libxkbcommon systemd-libs libx11`
  - Debian/Ubuntu: `sudo apt install g++ pkg-config libxkbcommon-dev libsystemd-dev libx11-dev`
  - Fedora: `sudo dnf install gcc-c++ pkgconf-pkg-config libxkbcommon-devel systemd-devel libX11-devel`

  Used when present, none required: `hunspell` with dictionaries (word checking), `wl-clipboard` or `xclip`/`xsel` (converting a selection), `kdialog` or `zenity` ("My words…"), `pw-play`, `paplay` or `aplay` (sound), `xdg-open` (links from the menu).
- **Access (once).** AURA reads key presses from `/dev/input` and types through its own virtual keyboard (`/dev/uinput`) — the one way that works on Wayland and X11 alike. Linux doesn't give that to ordinary programs, so install the rule that grants it to the person at the computer (the active local session only):
  ```
  sudo cp linux/70-aura-switcher.rules /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
  ```
  Until then (or with only a part of it: keyboards but no mouse — AURA has to see clicks, they move the caret) the icon shows `!`. `aura-switcher --check` prints what AURA sees: desktop, layouts, keyboards, dictionaries, clipboard tool, login session, tray. Be aware: with this access any program you run could read the keyboard and type into any window too — the same is true of every layout switcher and key remapper on Wayland.
- Same features and menu as on Mac and Windows. The tray icon (yellow letters on a blue tile, grey — disabled) needs a tray that speaks StatusNotifierItem: Plasma, Xfce, LXQt have one, Cinnamon and MATE get it from `xapp-sn-watcher` (part of Linux Mint). Without a tray AURA still works; settings are in `~/.config/aura-switcher/settings`.
- **Differences**
  - AURA only listens to the keyboard and never holds a key back, so a word is checked when you press Space — not when Return follows it straight away.
  - After you go back into the text (Backspace over a space, arrows) the word is yours until the next space: AURA can't see the text field.
  - It can't tell a password field either. A password is only checked if it has a space in it; while the screen is locked or another session is in front (another user, the login screen) AURA is paused — it follows logind and the desktops' screen savers. For a password prompt with spaces add the app to exclusions.
  - Selected text is copied with Ctrl+Insert and typed back with the new layout's keys (several lines — pasted with Shift+Insert; the clipboard is restored). A clipboard manager (Klipper…) may keep the copied selection in its history; a selection equal to what the clipboard already holds isn't noticed. Needs `wl-clipboard` on Wayland, `xclip` or `xsel` on X11; without them, and in terminals, double ⇧ without a typed word just switches the layout. On Plasma 5 Wayland the clipboard goes through Klipper (applications there don't paste what `wl-copy` puts on it), and a selection of several lines is left alone: Klipper also replaces the primary selection, and GTK fields drop their selection.
  - Games: a full-screen window that is a Steam game, an app from the "Game" category, or runs under Wine/Proton.
  - Word checking: Hunspell dictionaries when they are installed (`/usr/share/hunspell`; packages like `hunspell-en_us` on Arch, `hunspell-en-us` on Debian/Ubuntu, `hunspell-uk`), AURA's own language models otherwise.
  - "My words…" opens the desktop's input box (`kdialog` or `zenity`). "Check for updates…" opens the update page: on Linux AURA is built from source.
  - Launch at login is off until you tick it in the menu (`~/.config/autostart`).
  - X11 with layouts set only by a `setxkbmap` command (no keyboard settings of a desktop): any keyboard that appears later resets them to the system's. AURA puts them back after its own virtual keyboard appears; for other keyboards set the layouts system-wide (`localectl set-x11-keymap us,ua`).
- **Key remappers** (keyd, kanata, xremap…) that grab every keyboard grab AURA's virtual one too and type its keys again through their own. AURA notices and doesn't take them for your typing (tested with keyd). If the remapper changes letter keys, exclude `AURA Switcher virtual keyboard` (`4155:5241`) in its config.
- How it works: `linux/aura.cpp`, a single C++ file — the engine of `windows/aura.cpp` with Linux around it. A reader thread follows `/dev/input` (evdev) and only records keystrokes; checking and replacement happen on the main thread. Character tables come from the system layouts (`libxkbcommon`), the language models are built into the binary, the tray and menu are plain D-Bus (`sd-bus`), the session's state comes from logind, and on Plasma a small KWin script tells which app is in front (its id and full-screen state, never the window title). Tests: `tests/run-linux.sh`.
