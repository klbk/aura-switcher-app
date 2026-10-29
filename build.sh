#!/bin/zsh
# Builds universal "AURA Switcher.app" (Apple Silicon + Intel).
#   ./build.sh            — install to /Applications and launch
#   ./build.sh --release  — pack dist/AURA-Switcher-<version>.zip, don't install
set -e
cd "$(dirname "$0")"
NAME="AURA Switcher"
FILE=AURA-Switcher            # file names without spaces
BUNDLE_ID=com.chernov.aura    # unchanged: keeps users' settings
VERSION=0.6.1
APP="build/$NAME.app"
rm -rf build && mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
for ARCH in arm64 x86_64; do
  swiftc -O -swift-version 5 -target $ARCH-apple-macos13 main.swift -o build/bin-$ARCH
done
lipo -create build/bin-arm64 build/bin-x86_64 -output "$APP/Contents/MacOS/$NAME"
cp AppIcon.icns "$APP/Contents/Resources/"
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
<key>CFBundleName</key><string>$NAME</string>
<key>CFBundleDisplayName</key><string>$NAME</string>
<key>CFBundleExecutable</key><string>$NAME</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>$VERSION</string>
<key>CFBundleVersion</key><string>$VERSION</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>LSUIElement</key><true/>
<key>AURABuildDate</key><string>$(date +%s)</string>
</dict></plist>
EOF
codesign --force -s - "$APP"

if [[ "$1" == "--release" ]]; then
  mkdir -p dist && rm -f dist/$FILE-$VERSION.zip
  ditto -c -k --keepParent "$APP" dist/$FILE-$VERSION.zip
  rm -rf build; echo "dist/$FILE-$VERSION.zip"; exit 0
fi

pkill -x "$NAME" || true
pkill -x AURA || true                      # pre-rename build
rm -rf /Applications/AURA.app /Applications/KeySwitch.app
rm -rf "/Applications/$NAME.app" && mv "$APP" /Applications/
rm -rf build
open "/Applications/$NAME.app"
