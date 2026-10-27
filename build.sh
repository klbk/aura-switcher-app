#!/bin/zsh
# Builds universal AURA.app (Apple Silicon + Intel).
#   ./build.sh            — install to /Applications and launch
#   ./build.sh --release  — pack dist/AURA-<version>.zip for testers, don't install
set -e
cd "$(dirname "$0")"
VERSION=0.4
APP=build/AURA.app
BIN=$APP/Contents/MacOS/AURA
rm -rf build && mkdir -p $APP/Contents/MacOS
for ARCH in arm64 x86_64; do
  swiftc -O -swift-version 5 -target $ARCH-apple-macos13 main.swift -o build/AURA-$ARCH
done
lipo -create build/AURA-arm64 build/AURA-x86_64 -output $BIN
mkdir -p $APP/Contents/Resources && cp AppIcon.icns $APP/Contents/Resources/
cat > $APP/Contents/Info.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>com.chernov.aura</string>
<key>CFBundleName</key><string>AURA</string>
<key>CFBundleExecutable</key><string>AURA</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>$VERSION</string>
<key>CFBundleVersion</key><string>$VERSION</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>LSUIElement</key><true/>
</dict></plist>
EOF
codesign --force -s - $APP

if [[ "$1" == "--release" ]]; then
  mkdir -p dist
  rm -f dist/AURA-$VERSION.zip
  ditto -c -k --keepParent $APP dist/AURA-$VERSION.zip
  rm -rf build
  echo "dist/AURA-$VERSION.zip"
  exit 0
fi

pkill -x AURA || true
pkill -x KeySwitch || true
rm -rf /Applications/KeySwitch.app
rm -rf /Applications/AURA.app && mv $APP /Applications/
rm -rf build
open /Applications/AURA.app
