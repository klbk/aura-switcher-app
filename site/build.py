#!/usr/bin/env python3
"""Build the public site into OUT: a pre-rendered page per language (/uk/, /en/, …) with its own
title, description, canonical and hreflang, plus sitemap.xml (robots.txt only works at the domain root, so none) — so search engines
see every language. Rendering uses headless Chrome (texts live in JS).
Usage: python3 build.py <out-dir>   (out-dir = clone of klbk/aura-switcher)"""
import html, json, re, shutil, subprocess, sys, tempfile
from pathlib import Path

BASE = "https://klbk.github.io/aura-switcher/"
# visit counter (GoatCounter: no cookies, no personal data); added only to published pages, not while rendering
COUNTER = '<script data-goatcounter="https://aura-switcher.goatcounter.com/count" async src="//gc.zgo.at/count.js"></script>'
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
SRC = Path(__file__).resolve().parent
ASSETS = ["icon.svg", "og.jpg", "latest.js", "i18n-more.js", "seo.js", "shared.js", "google8a7be41a4753a620.html", "f9834e666c1005df874f5c53d176ecc3.txt"]  # last two: Search Console + IndexNow (Bing) keys, keep

out = Path(sys.argv[1]).resolve()
VERSION = re.search(r'version: "([^"]+)"', (SRC / "latest.js").read_text()).group(1)
langs = re.findall(r"^  (\w\w): \{", (SRC / "seo.js").read_text(), re.M)
src = (SRC / "index.html").read_text()

def head_links(canonical):
    alts = "".join(f'<link rel="alternate" hreflang="{l}" href="{BASE}{l}/">' for l in langs)
    return (f'<link rel="canonical" href="{canonical}">{alts}<link rel="alternate" hreflang="x-default" href="{BASE}">'
            f'<meta property="og:type" content="website"><meta property="og:url" content="{canonical}">'
            f'<meta property="og:image" content="{BASE}og.jpg"><meta property="og:image:width" content="1200"><meta property="og:image:height" content="630">'
            '<meta name="twitter:card" content="summary_large_image">'
            )

def render(html):
    """Run the page's JS in headless Chrome and return the resulting HTML."""
    with tempfile.TemporaryDirectory() as tmp:
        for a in ASSETS: shutil.copy(SRC / a, tmp)
        page = Path(tmp) / "p.html"
        page.write_text(html.replace('"../', '"'))  # assets sit next to the temp page
        # headless Chrome sometimes hangs on a page: give it a minute, then try again
        for attempt in range(3):
            try:
                dom = subprocess.run([CHROME, "--headless", "--disable-gpu", "--virtual-time-budget=1500",
                                      "--dump-dom", page.as_uri()], capture_output=True, text=True, check=True,
                                     timeout=60).stdout
                break
            except subprocess.TimeoutExpired:
                if attempt == 2: raise
    return "<!doctype html>\n" + dom

def finish(dom, canonical, rel):
    title = re.search(r"<title>(.*?)</title>", dom).group(1)
    desc = re.search(r'<meta name="description" content="(.*?)">', dom).group(1)
    og = f'<meta property="og:title" content="{title}"><meta property="og:description" content="{desc}">'
    lang = re.search(r'<html lang="(\w+)"', dom).group(1)
    ld = {"@context": "https://schema.org", "@type": "SoftwareApplication", "name": "AURA Switcher", "url": canonical,
          "description": html.unescape(desc), "inLanguage": lang, "applicationCategory": "UtilitiesApplication",
          "operatingSystem": "macOS 13+, Windows 10/11", "softwareVersion": VERSION, "image": BASE + "og.jpg",
          "downloadUrl": "https://github.com/klbk/aura-switcher/releases/latest", "license": "https://www.gnu.org/licenses/gpl-3.0.html",
          "offers": {"@type": "Offer", "price": "0", "priceCurrency": "USD"}}
    og += '<script type="application/ld+json">' + json.dumps(ld, ensure_ascii=False).replace("</", "<\\/") + '</script>'
    dom = dom.replace("</head>", head_links(canonical) + og + COUNTER + "</head>", 1)
    if rel: dom = dom.replace('src="i18n-more.js"', 'src="../i18n-more.js"').replace('src="seo.js"', 'src="../seo.js"') \
                     .replace('src="shared.js"', 'src="../shared.js"').replace('"icon.svg"', '"../icon.svg"')
    return dom

for a in ASSETS: shutil.copy(SRC / a, out)
for f in ("update.json", "update.sig"):   # «Check for updates» in the apps (tools/update_manifest.py); .sig — its signature
    if (SRC / f).exists(): shutil.copy(SRC / f, out)
(out / "update").mkdir(exist_ok=True)  # update page (opened from the app's menu): not pre-rendered, not in the sitemap
shutil.copy(SRC / "update" / "index.html", out / "update" / "index.html")
(out / "privacy").mkdir(exist_ok=True)  # privacy policy (linked from the Microsoft Store listing)
shutil.copy(SRC / "privacy" / "index.html", out / "privacy" / "index.html")
for l in langs:
    page = src.replace('<html lang="uk">', f'<html lang="{l}" data-page="{l}">')
    (out / l).mkdir(exist_ok=True)
    (out / l / "index.html").write_text(finish(render(page), f"{BASE}{l}/", True))
(out / "index.html").write_text(finish(render(src), BASE, False))
today = __import__("datetime").date.today().isoformat()
alts = "".join(f'<xhtml:link rel="alternate" hreflang="{l}" href="{BASE}{l}/"/>' for l in langs) + f'<xhtml:link rel="alternate" hreflang="x-default" href="{BASE}"/>'
(out / "sitemap.xml").write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">\n'
    + "".join(f"  <url><loc>{BASE}{p}</loc><lastmod>{today}</lastmod>{alts}</url>\n" for p in [""] + [f"{l}/" for l in langs]) + "</urlset>\n")
print(f"built {len(langs)} languages → {out}")
