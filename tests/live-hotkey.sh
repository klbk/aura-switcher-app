#!/bin/zsh
# Live test of the correction key (Mac, AURA running): see tests/live-hotkey.swift. Don't touch the keyboard or mouse
# while it runs (~3 min). The window is a small .app (tests/live-hotkey-host.swift) opened with `open`, so it gets the
# focus; keys are posted by a separate process and only while that window is the key window of the front app.
set -e
cd "$(dirname "$0")/.."
mkdir -p build
APP=build/LiveTest.app; DIR=$(mktemp -d /tmp/aura-live.XXXXXX)
rm -rf $APP; mkdir -p $APP/Contents/MacOS
swiftc -O -swift-version 5 tests/live-hotkey-host.swift -o $APP/Contents/MacOS/LiveTest
cat > $APP/Contents/Info.plist <<P
<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>local.aura.livetest</string><key>CFBundleExecutable</key><string>LiveTest</string>
<key>CFBundleName</key><string>LiveTest</string><key>CFBundlePackageType</key><string>APPL</string></dict></plist>
P
codesign --force --sign - $APP >/dev/null 2>&1 || true
swiftc -O -swift-version 5 tests/live-hotkey.swift -o build/live-hotkey
open -n $APP --args $DIR
for i in {1..40}; do grep -q "active=1 key=1" $DIR/state 2>/dev/null && break; sleep 0.25; done
build/live-hotkey $DIR build/live-hotkey-results.md || true
pkill -x LiveTest 2>/dev/null || true
rm -rf $APP $DIR build/live-hotkey
