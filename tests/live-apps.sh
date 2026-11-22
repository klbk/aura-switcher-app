#!/bin/zsh
# Live check of the correction key (double ⇧) in real Mac apps: TextEdit, Safari, Chrome, VS Code — only new empty
# documents / windows and a local test page, nothing is sent or saved. AURA must be running, layouts ABC, Ukrainian-PC, RussianWin.
# Keys go to the front app only if it is the app under test (tests/live-apps.swift). Don't touch the Mac while it runs.
#   tests/live-apps.sh [TextEdit] [Safari] [Chrome] [Code]      (default: all)
cd "$(dirname "$0")/.."
mkdir -p build; T=$(mktemp -d /tmp/aura-apps.XXXXXX)
swiftc -O -swift-version 5 tests/live-apps.swift -o $T/post || exit 1
cat > $T/page.html <<'H'
<!doctype html><meta charset=utf-8><title>T:|P:</title>
<input id=t autofocus><input id=p type=password>
<script>function u(){document.title='T:'+t.value+'|P:'+p.value}t.oninput=p.oninput=u;t.focus()</script>
H
START=$(defaults read com.chernov.aura hotkey 2>/dev/null)
echo "| app | case | expected | got | |"; echo "|---|---|---|---|---|"
row() { # app case expected got — compared ignoring case and trailing spaces (TextEdit capitalizes the first letter)
  a=${${3:l}%% #}; b=${${4:l}%% #}; [[ "$a" == "$b" ]] && ok="✅" || ok="⚠️"; echo "| $1 | $2 | $3 | $4 | $ok |"; }
exp() { case $1 in TextEdit) echo "привіт$2";; Code) echo "(not readable)";; *) echo "T:привіт$2|P:";; esac; }
post() { $T/post "$1" "$2" 2>&1 | tail -1; }
typeset -A BUNDLE; BUNDLE=(TextEdit com.apple.TextEdit Safari com.apple.Safari Chrome com.google.Chrome Code com.microsoft.VSCode)
text() { # app → current text
  case $1 in
    TextEdit) osascript -e 'tell application "System Events" to key code 53'; sleep 0.5; osascript -e 'tell application "TextEdit" to get text of document 1' ;;
    Safari) osascript -e 'tell application "Safari" to get name of current tab of front window' ;;
    Chrome) osascript -e 'tell application "Google Chrome" to get title of active tab of front window' ;;
    *) echo "(not readable)" ;;
  esac; }
openapp() {
  case $1 in
    TextEdit) osascript -e 'tell application "TextEdit"' -e 'activate' -e 'make new document' -e 'end tell' >/dev/null ;;
    Safari) osascript -e 'tell application "Safari"' -e 'activate' -e "make new document with properties {URL:\"file://$T/page.html\"}" -e 'end tell' >/dev/null ;;
    Chrome) osascript -e 'tell application "Google Chrome"' -e 'activate' -e 'make new window' -e "set URL of active tab of front window to \"file://$T/page.html\"" -e 'end tell' >/dev/null ;;
    Code) open -a "Visual Studio Code" --args --new-window; sleep 4; osascript -e 'tell application "Visual Studio Code" to activate'; sleep 1
          osascript -e 'tell application "System Events" to keystroke "n" using command down' ;;
  esac; sleep 3; }
closeapp() {
  case $1 in
    TextEdit) osascript -e 'tell application "TextEdit" to close document 1 saving no' -e 'tell application "TextEdit" to quit' >/dev/null 2>&1 ;;
    Safari) osascript -e 'tell application "Safari" to close front document' >/dev/null 2>&1 ;;
    Chrome) osascript -e 'tell application "Google Chrome" to close front window' >/dev/null 2>&1 ;;
    Code) osascript -e 'tell application "Visual Studio Code" to activate' -e 'tell application "System Events" to keystroke "w" using command down' -e 'tell application "System Events" to keystroke "w" using {command down, shift down}' >/dev/null 2>&1 ;;
  esac; }
aura_ok() { pgrep -x "AURA Switcher" >/dev/null && [[ "$(defaults read com.chernov.aura hotkey 2>/dev/null || echo shift)" == shift ]] && [[ "$(defaults read com.chernov.aura enabled 2>/dev/null || echo 1)" != 0 ]]; }
for app in ${@:-TextEdit Safari Chrome Code}; do
  aura_ok || { echo "| $app | — | — | SKIPPED: AURA is not running / not enabled / key is not Shift | ⚠️ |"; continue; }
  b=$BUNDLE[$app]; openapp $app
  [[ "$(osascript -e 'tell application "System Events" to get bundle identifier of first process whose frontmost is true')" == "$b" ]] || { echo "| $app | — | — | SKIPPED: not in front | ⚠️ |"; closeapp $app; continue; }
  row $app "empty field, double ⇧" UK "$(post $b empty | sed 's/Ukrainian-PC/UK/')"
  post $b word >/dev/null; row $app "«ghbdsn» + double ⇧, text" "$(exp $app)" "$(text $app)"
  post $b clear >/dev/null
  post $b space >/dev/null; row $app "«ghbdsn» + space (auto-correction), text" "$(exp $app " ")" "$(text $app)"
  post $b clear >/dev/null
  post $b select >/dev/null; row $app "«ghbdsn», select all, double ⇧, text" "$(exp $app)" "$(text $app)"
  post $b clear >/dev/null
  if [[ $app == Safari || $app == Chrome ]]; then
    post $b tab >/dev/null; post $b word >/dev/null; row $app "password field: «ghbdsn» + double ⇧, value" "T:|P:ghbdsn" "$(text $app)"
    row $app "password field: layout" "ABC→UK (switches)" "$($T/post $b clear | sed 's/Ukrainian-PC/ABC→UK (switches)/;s/^ABC$/unchanged/')"
    osascript -e 'tell application "System Events" to key code 48 using shift down' >/dev/null; post $b clear >/dev/null
    post $b addr >/dev/null; row $app "address bar: layout after double ⇧" "UK" "$(post $b empty | sed 's/Ukrainian-PC/UK/')"
  fi
  closeapp $app; sleep 1
done
defaults write com.chernov.aura hotkey "${START:-shift}"
osascript -e 'tell application "System Events" to tell (first process whose frontmost is true) to keystroke ""' >/dev/null 2>&1
rm -rf $T
