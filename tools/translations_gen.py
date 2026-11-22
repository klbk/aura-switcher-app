#!/usr/bin/env python3
"""Proofreading templates: translations/<code>.md for every language, built from the current sources.
Run by hand:  python3 tools/translations_gen.py        (no dependencies, safe to run again)

Sources: tools/menu_i18n.py (KEYS, LANGS — app menu), site/i18n-more.js (MORE), site/shared.js (T, SUPPORT, CONTACT,
EXTRA, EXTRA_KEYS), site/seo.js (SEO), site/update/index.html (U — update page). Version: build.sh.
A language missing a key gets the English text. Files of languages in REVIEWED (already proofread by a native speaker)
are never rewritten: the script only prints which keys differ from the sources.
The date line is ignored when deciding whether a file changed, so a second run changes nothing."""
import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "translations"

# Proofread by native speakers: only compared with the sources, never overwritten. Add a code here when a PR is accepted.
REVIEWED = ["it"]

ISSUE = "https://github.com/klbk/aura-switcher-app/issues/6"
SKIP = {"en", "uk"}  # the source languages — nothing to proofread

NAMES = {
    "pl": "Polish", "de": "German", "es": "Spanish", "fr": "French", "it": "Italian", "pt": "Portuguese", "tr": "Turkish",
    "nl": "Dutch", "cs": "Czech", "sv": "Swedish", "fi": "Finnish", "ro": "Romanian", "hu": "Hungarian", "el": "Greek",
    "bg": "Bulgarian", "he": "Hebrew", "ar": "Arabic", "hi": "Hindi", "ko": "Korean", "be": "Belarusian", "kk": "Kazakh",
    "ky": "Kyrgyz", "ka": "Georgian", "hy": "Armenian", "az": "Azerbaijani", "sr": "Serbian", "mk": "Macedonian",
    "sk": "Slovak", "hr": "Croatian", "sl": "Slovenian", "lt": "Lithuanian", "lv": "Latvian", "et": "Estonian",
    "da": "Danish", "nb": "Norwegian Bokmål", "is": "Icelandic", "sq": "Albanian", "fa": "Persian", "vi": "Vietnamese",
    "tg": "Tajik", "mn": "Mongolian", "tt": "Tatar", "uz": "Uzbek", "ga": "Irish", "mt": "Maltese", "cy": "Welsh",
}

# ---------- a tiny reader for the JS object/array literals used in the site files ----------

class JS:
    def __init__(self, text, env):
        self.s, self.i, self.env = text, 0, env

    def ws(self):
        s = self.s
        while self.i < len(s):
            if s[self.i] in " \t\r\n":
                self.i += 1
            elif s.startswith("//", self.i):
                self.i = s.find("\n", self.i) if "\n" in s[self.i:] else len(s)
            elif s.startswith("/*", self.i):
                self.i = s.index("*/", self.i) + 2
            else:
                break

    def value(self):
        v = self.atom()
        self.ws()
        while self.s.startswith("+", self.i):  # "a" + GH + "b"
            self.i += 1
            self.ws()
            v += self.atom()
            self.ws()
        return v

    def atom(self):
        self.ws()
        s, c = self.s, self.s[self.i]
        if c == "{":
            self.i += 1
            d = {}
            while True:
                self.ws()
                if s[self.i] == "}":
                    self.i += 1
                    return d
                if s[self.i] in "\"'":
                    k = self.string()
                else:
                    m = re.compile(r"[A-Za-z_$][\w$-]*").match(s, self.i)
                    k, self.i = m.group(), m.end()
                self.ws()
                assert s[self.i] == ":", (k, s[self.i:self.i + 40])
                self.i += 1
                d[k] = self.value()
                self.ws()
                if s[self.i] == ",":
                    self.i += 1
        if c == "[":
            self.i += 1
            a = []
            while True:
                self.ws()
                if s[self.i] == "]":
                    self.i += 1
                    return a
                a.append(self.value())
                self.ws()
                if s[self.i] == ",":
                    self.i += 1
        if c in "\"'`":
            return self.string()
        m = re.compile(r"[A-Za-z_$][\w$]*|-?\d+(\.\d+)?").match(s, self.i)
        self.i = m.end()
        w = m.group()
        if w in ("true", "false", "null"):
            return {"true": True, "false": False, "null": None}[w]
        if re.fullmatch(r"-?\d+(\.\d+)?", w):
            return float(w)
        return self.env[w]

    def string(self):
        s, q = self.s, self.s[self.i]
        self.i += 1
        out = []
        while s[self.i] != q:
            c = s[self.i]
            if c == "\\":
                n = s[self.i + 1]
                if n == "u":
                    out.append(chr(int(s[self.i + 2:self.i + 6], 16)))
                    self.i += 6
                    continue
                out.append({"n": "\n", "t": "\t"}.get(n, n))
                self.i += 2
            elif q == "`" and s.startswith("${", self.i):
                j = s.index("}", self.i)
                out.append(self.env[s[self.i + 2:j].strip()])
                self.i = j + 1
            else:
                out.append(c)
                self.i += 1
        self.i += 1
        return "".join(out)


def const(text, name, env):
    """value of `const NAME = <literal>` (top-level)"""
    m = re.search(r"^(?:const|let|var) " + name + r"\s*=\s*", text, re.M)
    assert m, name
    p = JS(text, env)
    p.i = m.end()
    return p.value()


# ---------- load the sources ----------

def load():
    site = ROOT / "site"
    shared = (site / "shared.js").read_text()
    env = {"GH": re.search(r"^const GH = '(.*)';", shared, re.M).group(1)}
    more_src = (site / "i18n-more.js").read_text()
    upd_src = (site / "update" / "index.html").read_text()
    seo_src = (site / "seo.js").read_text()
    src = {
        "T": const(shared, "T", env) if re.search(r"^const T = \{", shared, re.M) else {},
        "SUPPORT": const(shared, "SUPPORT", env), "CONTACT": const(shared, "CONTACT", env),
        "EXTRA": const(shared, "EXTRA", env), "EXTRA_KEYS": const(shared, "EXTRA_KEYS", env),
        "PHONE_NOTE": const(shared, "PHONE_NOTE", env), "MORE": const(more_src, "MORE", env), "SEO": const(seo_src, "SEO", env), "U": const(upd_src, "U", env),
    }
    mi = (ROOT / "tools" / "menu_i18n.py").read_text()
    keys_ns = {}
    exec(re.search(r"^KEYS = \[.*?\]", mi, re.M | re.S).group(), keys_ns)
    blocks = re.search(r'^LANGS = """\n(.*?)"""', mi, re.M | re.S).group(1).strip().split("\n\n")
    menu = {}
    new_ns = {}   # 0.11 strings live in NEW (menu_i18n.py): appended keys + the reworded "app"
    exec(re.search(r"^NEW = \{.*?^\}", mi, re.M | re.S).group(), new_ns)
    extra = ("hotkey", "off", "settings")
    for b in blocks:
        head, *lines = b.split("\n")
        code, name = head.split(" ", 1)
        lines += [new_ns["NEW"][k][code] for k in extra]
        lines[keys_ns["KEYS"].index("app")] = new_ns["NEW"]["app"][code]
        assert len(lines) == len(keys_ns["KEYS"]), code
        menu[code] = (name, dict(zip(keys_ns["KEYS"], lines)))
    src["MENU_KEYS"], src["MENU"] = keys_ns["KEYS"], menu
    ver = re.search(r"^VERSION=\$\{VERSION:-([\d.]+)\}", (ROOT / "build.sh").read_text(), re.M).group(1)
    return src, ver


WEB_ORDER = ["name", "title", "badge", "h1", "lead", "dlMac", "dlWin", "demoWrong", "demoFixed", "fTitle",
             "f1h", "f1p", "f2h", "f2p", "f3h", "f3p", "f4h", "f4p", "f5h", "f5p", "f6h", "f6p", "pTitle", "pMac", "pWin",
             "iTitle", "s1h", "s1p", "s2h", "s2p", "s3h", "s3p", "ctaH", "ctaP", "made", "foot",
             "donate", "supportP", "contact"]


def sections(src, code):
    """{section: [(key, text)]} for one language; a missing text falls back to English"""
    def web(c):
        d = {}
        d.update(src["T"].get(c, {}))
        d.update({k: v for k, v in src["MORE"].get(c, {}).items() if k != "dir"})
        seo = src["SEO"].get(c, {})
        for k in ("title", "desc", "altH", "altP"):
            if k in seo:
                d[k] = seo[k]
        if c in src["SUPPORT"]:
            d["donate"], d["supportP"] = src["SUPPORT"][c][:2]
        if c in src["CONTACT"]:
            d["contact"] = src["CONTACT"][c]
        if c in src["PHONE_NOTE"]:
            d["phoneNote"] = src["PHONE_NOTE"][c]
        if c in src["EXTRA"]:
            for k, v in zip(src["EXTRA_KEYS"], src["EXTRA"][c]):
                d.setdefault(k, v)
        return d
    order = WEB_ORDER + ["phoneNote"] + src["EXTRA_KEYS"] + ["desc", "altH", "altP"]
    w, we = web(code), web("en")
    en_menu, menu = src["MENU"]["en"], src["MENU"].get(code, ("", {}))
    mk = src["MENU_KEYS"]
    app = [("name", menu[0] or en_menu[0])] + [(k, menu[1].get(k, en_menu[1][k]).rstrip()) for k in mk]
    web_rows = [(k, w.get(k) or we[k]) for k in order if k in we or k in w]
    u, ue = src["U"].get(code), src["U"]["en"]
    upd = [(f"update{i + 1}", (u or ue)[i]) for i in range(12)]
    return {"App menu (Mac and Windows)": app, "Website": web_rows, "Update page": upd}


def render(code, secs, version, date, ai):
    name = NAMES[code]
    who = "written by an AI, not by a native speaker" if ai else "already proofread"
    head = f"""# AURA Switcher — {name} ({code}) translation

{name} is already in the app and on the site (https://klbk.github.io/aura-switcher/{code}/), but it was {who}. Please proofread it: fix anything that sounds unnatural or wrong, and leave good lines as they are.
Keep the key before `=`, HTML tags (<br>, <kbd>, <span …>), {{v}} and symbols (⇧ ⚠︎ ⬆︎ ☕ →) unchanged.
When done, open a pull request with this file. The maintainer will move the texts into the app and the site.
Questions, or want to talk it over first? {ISSUE}

Generated by tools/translations_gen.py from AURA Switcher {version} · {date}. Do not edit the header.
"""
    body = ""
    for title, rows in secs.items():
        body += f"\n## {title}\n\n```\n" + "\n".join(f"{k} = {v}" for k, v in rows) + "\n```\n"
    return head + body


def parse(text):
    out, sec = {}, None
    for line in text.splitlines():
        if line.startswith("## "):
            sec = line[3:].split(" (")[0]
        elif " = " in line and sec and not line.startswith("#"):
            k, v = line.split(" = ", 1)
            out[(sec, k)] = v.strip()
    return out


def main():
    src, ver = load()
    date = datetime.date.today().isoformat()
    codes = [c for c in src["MENU"] if c not in SKIP]
    new = same = 0
    for code in codes:
        secs = sections(src, code)
        path = OUT / f"{code}.md"
        if code in REVIEWED:
            if not path.exists():
                print(f"{code}: reviewed file is missing", file=sys.stderr)
                continue
            have = parse(path.read_text())
            diff = []
            for title, rows in secs.items():
                t = title.split(" (")[0]
                for k, v in rows:
                    if (t, k) in have and have[(t, k)] != v.strip():
                        diff.append((t, k))
                    elif (t, k) not in have:
                        diff.append((t, k, "missing"))
            print(f"{code}: reviewed, not rewritten; {len(diff)} keys differ from the sources" + (":" if diff else ""))
            for d in diff:
                print("   ", " / ".join(d))
            continue
        text = render(code, secs, ver, date, ai=True)
        if path.exists() and re.sub(r" · \d{4}-\d\d-\d\d\.", ".", path.read_text()) == re.sub(r" · \d{4}-\d\d-\d\d\.", ".", text):
            same += 1
            continue
        path.write_text(text)
        new += 1
    print(f"written {new}, unchanged {same} (version {ver})")


if __name__ == "__main__":
    main()
