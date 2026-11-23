#!/usr/bin/env python3
"""Writes site/update.json for «Check for updates» in the apps: version, download links, SHA-256 of the Windows exes.
Run after both builds, before publishing the release: python3 tools/update_manifest.py 0.11
Release assets (tag v<version>-beta): AURA-Switcher-mac.zip, AURA-Switcher-windows-x64.exe / -arm64.exe (+ the zips).
Also writes site/update.sig: ECDSA P-256 signature of update.json's exact bytes (raw r||s, hex), made with the
private key ~/.config/aura/update-key.pem (backup with the signing certificate). The Windows app trusts the
manifest only with a valid signature (public key in windows/aura.cpp, kUpdKey)."""
import hashlib, json, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
v = sys.argv[1]
base = f"https://github.com/klbk/aura-switcher/releases/download/v{v}-beta/"
m = {"version": v, "mac": base + "AURA-Switcher-mac.zip"}
for arch in ("x64", "arm64"):
    exe = ROOT / "dist" / f"AURA-Switcher-windows-{arch}.exe"
    m[f"win-{arch}"] = {"url": base + exe.name, "sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
body = (json.dumps(m, indent=1) + "\n").encode()
(ROOT / "site/update.json").write_bytes(body)

def der_to_raw(d):  # SEQUENCE { INTEGER r, INTEGER s } -> r||s, 32 bytes each
    i, out = 2 + (d[1] == 0x81), b""
    for _ in range(2):
        n = d[i + 1]; out += d[i + 2:i + 2 + n].lstrip(b"\0").rjust(32, b"\0"); i += 2 + n
    return out
key = Path.home() / ".config/aura/update-key.pem"
der = subprocess.run(["openssl", "dgst", "-sha256", "-sign", str(key)], input=body, capture_output=True, check=True).stdout
(ROOT / "site/update.sig").write_text(der_to_raw(der).hex() + "\n")
print(json.dumps(m, indent=1))
