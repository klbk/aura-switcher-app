# Promo video (12.5 s, 1080×1920)

`video.html?t=<seconds>` draws one frame of the animation; `encode.swift` turns PNG frames into an H.264 MP4 (AVFoundation, no ffmpeg).

```sh
mkdir -p /tmp/v/frames && cp video.html ../icon.svg /tmp/v/
for i in $(seq 0 374); do "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --hide-scrollbars \
  --force-device-scale-factor=1 --window-size=1080,1920 --screenshot=/tmp/v/frames/$(printf %04d $i).png \
  "file:///tmp/v/video.html?t=$(echo "scale=4;$i/30"|bc)&l=uk"; done
swiftc -O encode.swift -o /tmp/v/enc && /tmp/v/enc /tmp/v/frames aura-promo-uk.mp4 30 && rm -rf /tmp/v
```
For the English version use `&l=en` and name the output `aura-promo-en.mp4`.
