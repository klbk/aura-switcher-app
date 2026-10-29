#!/usr/bin/env python3
"""Build the public site into OUT: a pre-rendered page per language (/uk/, /en/, …) with its own
title, description, canonical and hreflang, plus sitemap.xml (robots.txt only works at the domain root, so none) — so search engines
see every language. Rendering uses headless Chrome (texts live in JS).
Usage: python3 build.py <out-dir>   (out-dir = clone of klbk/aura-switcher)"""
import re, shutil, subprocess, sys, tempfile
from pathlib import Path

BASE = "https://klbk.github.io/aura-switcher/"
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
SRC = Path(__file__).resolve().parent
ASSETS = ["icon.svg", "latest.js", "i18n-more.js", "seo.js", "shared.js", "google8a7be41a4753a620.html", "f9834e666c1005df874f5c53d176ecc3.txt"]  # last two: Search Console + IndexNow (Bing) keys, keep

out = Path(sys.argv[1]).resolve()
langs = re.findall(r"^  (\w\w): \{", (SRC / "seo.js").read_text(), re.M)
src = (SRC / "index.html").read_text()

def head_links(canonical):
    alts = "".join(f'<link rel="alternate" hreflang="{l}" href="{BASE}{l}/">' for l in langs)
    return (f'<link rel="canonical" href="{canonical}">{alts}<link rel="alternate" hreflang="x-default" href="{BASE}">'
            f'<meta property="og:type" content="website"><meta property="og:url" content="{canonical}">'
            )

def render(html):
    """Run the page's JS in headless Chrome and return the resulting HTML."""
    with tempfile.TemporaryDirectory() as tmp:
        for a in ASSETS: shutil.copy(SRC / a, tmp)
        page = Path(tmp) / "p.html"
        page.write_text(html.replace('"../', '"'))  # assets sit next to the temp page
        dom = subprocess.run([CHROME, "--headless", "--disable-gpu", "--virtual-time-budget=1500",
                              "--dump-dom", page.as_uri()], capture_output=True, text=True, check=True).stdout
    return "<!doctype html>\n" + dom

def finish(dom, canonical, rel):
    title = re.search(r"<title>(.*?)</title>", dom).group(1)
    desc = re.search(r'<meta name="description" content="(.*?)">', dom).group(1)
    og = f'<meta property="og:title" content="{title}"><meta property="og:description" content="{desc}">'
    dom = dom.replace("</head>", head_links(canonical) + og + "</head>", 1)
    if rel: dom = dom.replace('src="i18n-more.js"', 'src="../i18n-more.js"').replace('src="seo.js"', 'src="../seo.js"') \
                     .replace('src="shared.js"', 'src="../shared.js"').replace('"icon.svg"', '"../icon.svg"')
    return dom

for a in ASSETS: shutil.copy(SRC / a, out)
(out / "update").mkdir(exist_ok=True)  # update page (opened from the app's menu): not pre-rendered, not in the sitemap
shutil.copy(SRC / "update" / "index.html", out / "update" / "index.html")
for l in langs:
    page = src.replace('<html lang="uk">', f'<html lang="{l}" data-page="{l}">')
    (out / l).mkdir(exist_ok=True)
    (out / l / "index.html").write_text(finish(render(page), f"{BASE}{l}/", True))
(out / "index.html").write_text(finish(render(src), BASE, False))
(out / "sitemap.xml").write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
    + "".join(f"  <url><loc>{BASE}{p}</loc></url>\n" for p in [""] + [f"{l}/" for l in langs]) + "</urlset>\n")
print(f"built {len(langs)} languages → {out}")
