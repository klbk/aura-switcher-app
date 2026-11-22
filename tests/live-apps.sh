#!/bin/zsh
# Live check of the correction key (double ⇧) in real Mac apps, built so that it NEVER touches a window of the user's:
#  · TextEdit — one new document; its window id is remembered, read and closed by id only (and only while it is still untitled);
#  · Chrome   — a separate copy with a temporary profile (--user-data-dir), found and stopped by its own pid; the user's Chrome is untouched;
#  · VS Code  — the same: a separate copy with temporary user-data/extensions dirs, stopped by its own pid;
#  · Safari   — has no separate profile, so it is NOT driven here: check it by hand (the test page is written to the printed path).
# Only a new empty document / a local test page; nothing is sent or saved. Before and after, the windows of the apps are listed and compared.
# AURA must be running, enabled, key Shift; layouts ABC, Ukrainian-PC, RussianWin. Keys go to the front app only if it is the test copy.
#   tests/live-apps.sh [TextEdit] [Chrome] [Code]      (default: all three). Don't touch the Mac while it runs.
cd "$(dirname "$0")/.."
mkdir -p build; T=$(mktemp -d /tmp/aura-apps.XXXXXX)
swiftc -O -swift-version 5 tests/live-apps.swift -o $T/post || exit 1
cat > $T/page.html <<'H'
<!doctype html><meta charset=utf-8><title>T:|P:</title>
<input id=t autofocus><input id=p type=password>
<script>function u(){document.title='T:'+t.value+'|P:'+p.value}t.oninput=p.oninput=u;t.focus()</script>
H
echo "(Safari: open file://$T/page.html by hand in a NEW window, click the first field, then type ghbdsn and press ⇧⇧; Tab for the password field)" >&2
START=$(defaults read com.chernov.aura hotkey 2>/dev/null)
aura_ok() { pgrep -x "AURA Switcher" >/dev/null && [[ "$(defaults read com.chernov.aura hotkey 2>/dev/null || echo shift)" == shift ]] && [[ "$(defaults read com.chernov.aura enabled 2>/dev/null || echo 1)" != 0 ]]; }
windows() { # names of the windows of the user's apps, one line per window — before and after must be the same
  for a in TextEdit "Google Chrome" Safari "Visual Studio Code" "Code"; do
    pgrep -x "$a" >/dev/null || continue
    osascript -e "tell application \"System Events\" to tell process \"$a\" to get name of every window" 2>/dev/null | tr ',' '\n' | sed "s/^ */$a: /"
  done | sort; }
windows > $T/before.txt
echo "| app | case | expected | got | |"; echo "|---|---|---|---|---|"
row() { a=${${3:l}%% #}; b=${${4:l}%% #}; [[ "$a" == "$b" ]] && ok="✅" || ok="⚠️"; echo "| $1 | $2 | $3 | $4 | $ok |"; }
exp() { case $1 in TextEdit) echo "привіт$2";; Code) echo "(not readable)";; *) echo "T:привіт$2|P:";; esac; }
frontpid() { osascript -e 'tell application "System Events" to get unix id of first process whose frontmost is true'; }
TEWAS=0; pgrep -x TextEdit >/dev/null && TEWAS=1
APPPID=""; WID=""; BID=""
open_te() { WID=$(osascript -e 'tell application "TextEdit"' -e 'activate' -e 'make new document' -e 'return id of front window' -e 'end tell'); BID=com.apple.TextEdit; APPPID=$(pgrep -nx TextEdit); sleep 2; }
open_chrome() { open -na "Google Chrome" --args --user-data-dir=$T/chrome --no-first-run --no-default-browser-check --new-window "file://$T/page.html"; sleep 5
  APPPID=$(pgrep -f -- "--user-data-dir=$T/chrome" | sort -n | head -1); BID=com.google.Chrome; }
open_code() { open -na "Visual Studio Code" --args --user-data-dir=$T/code --extensions-dir=$T/ext --new-window; sleep 6
  APPPID=$(pgrep -f -- "--user-data-dir=$T/code" | sort -n | head -1); BID=com.microsoft.VSCode
  [[ -n $APPPID ]] && osascript -e "tell application \"System Events\" to set frontmost of (first process whose unix id is $APPPID) to true" >/dev/null && sleep 1 \
    && osascript -e 'tell application "System Events" to keystroke "n" using command down' >/dev/null; sleep 2; }
text() { # current text of the test document / page (never reads anything else)
  case $1 in
    TextEdit) osascript -e 'tell application "System Events" to key code 53'; sleep 0.5
              osascript -e "tell application \"TextEdit\" to get text of document of window id $WID" ;;
    Chrome) osascript -e "tell application \"System Events\" to get name of front window of (first process whose unix id is $APPPID)" | sed 's/ - Google Chrome.*//;s/ — .*//' ;;
    *) echo "(not readable)" ;;
  esac; }
post() { $T/post $BID "$1" $APPPID 2>&1 | tail -1; }
close_app() {
  case $1 in
    TextEdit) # only our window, only while it is untitled
              n=$(osascript -e "tell application \"TextEdit\" to get name of window id $WID" 2>/dev/null)
              if [[ "$n" == (Untitled*|Без*) ]]; then osascript -e "tell application \"TextEdit\" to close window id $WID saving no" >/dev/null 2>&1
              else echo "(TextEdit: window $WID is not untitled any more — left open)" >&2; fi
              [[ $TEWAS == 0 ]] && osascript -e 'tell application "TextEdit" to quit' >/dev/null 2>&1 ;;
    Chrome|Code) [[ -n $APPPID ]] && pkill -f -- "--user-data-dir=$T/${1:l}" ;;   # the test copy only (its own profile dir in the command line)
  esac; }
for app in ${@:-TextEdit Chrome Code}; do
  aura_ok || { echo "| $app | — | — | SKIPPED: AURA is not running / not enabled / key is not Shift | ⚠️ |"; continue; }
  case $app in TextEdit) open_te;; Chrome) open_chrome;; Code) open_code;; *) echo "| $app | — | — | not driven here (check by hand) | ⚠️ |"; continue;; esac
  [[ -n $APPPID ]] || { echo "| $app | — | — | SKIPPED: test copy did not start | ⚠️ |"; continue; }
  [[ "$(frontpid)" == "$APPPID" ]] || { echo "| $app | — | — | SKIPPED: the test copy is not in front | ⚠️ |"; close_app $app; continue; }
  if [[ $app == Chrome ]]; then t0=$(text Chrome); [[ "$t0" == T:* ]] || { echo "| $app | — | — | SKIPPED: front window is not the test page ($t0) | ⚠️ |"; close_app $app; continue; }; fi
  row $app "empty field, double ⇧" UK "$(post empty | sed 's/Ukrainian-PC/UK/')"
  post word >/dev/null; row $app "«ghbdsn» + double ⇧, text" "$(exp $app)" "$(text $app)"
  post clear >/dev/null
  post space >/dev/null; row $app "«ghbdsn» + space (auto-correction), text" "$(exp $app " ")" "$(text $app)"
  post clear >/dev/null
  post select >/dev/null; row $app "«ghbdsn», select all, double ⇧, text" "$(exp $app)" "$(text $app)"
  post clear >/dev/null
  if [[ $app == Chrome ]]; then
    post tab >/dev/null; post word >/dev/null; row $app "password field: «ghbdsn» + double ⇧, value" "T:|P:ghbdsn" "$(text $app)"
    row $app "password field: layout" "UK (switches)" "$(post clear | sed 's/Ukrainian-PC/UK (switches)/')"
    post addr >/dev/null; row $app "address bar: layout after double ⇧" "UK" "$(post empty | sed 's/Ukrainian-PC/UK/')"
  fi
  close_app $app; sleep 2
done
defaults write com.chernov.aura hotkey "${START:-shift}"
sleep 1; windows > $T/after.txt
if diff -q $T/before.txt $T/after.txt >/dev/null; then echo "windows of the user's apps before/after: unchanged ✅"
else echo "WINDOWS DIFFER ⚠️:"; diff $T/before.txt $T/after.txt; fi
rm -rf $T
