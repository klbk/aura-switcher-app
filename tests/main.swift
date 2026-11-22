// Auto-correction test: words typed in the wrong layout must be fixed, words typed right must stay.
// Runs the real engine (engine.swift) on macOS layouts and spell checkers, without the app or key events.
//   tests/run.sh            — run, compare with tests/baseline-macos<version>.txt
//   tests/run.sh --update   — accept the current results as the new baseline
//   tests/run.sh -v         — print every result line (by default: only what got worse, and OK/FAILED)
import Cocoa
import Carbon

let engine = Engine()
let root = URL(fileURLWithPath: CommandLine.arguments[1])
langDir = root.appendingPathComponent("lang")

func source(_ id: String) -> TISInputSource? {
    let f = [kTISPropertyInputSourceID: id] as CFDictionary
    return (TISCreateInputSourceList(f, true)?.takeRetainedValue() as? [TISInputSource])?.first
}
var cache: [String: Layout] = [:]
func layout(_ id: String) -> Layout {
    if let l = cache[id] { return l }
    guard let s = source(id), let lang = sourceLang(s), let l = Layout(lang: lang, source: s) else { fatalError("no layout \(id)") }
    cache[id] = l; return l
}

/// keys that type `word` in layout `target`, as they'd arrive while layout `typed` is active
func keys(_ word: String, in target: Layout, typedIn typed: Layout) -> [Key]? {
    var r: [Key] = []
    for c in word {
        guard let ks = target.keys(for: c) else { return nil }
        for k in ks { guard let ch = typed.toChar[k] else { return nil }; r.append(Key(code: k.code, shift: k.shift, ch: ch)) }
    }
    return r
}

/// what auto-correction makes of `word` (meant in `meant`) typed while `typed` is active: the fixed text, or nil = left alone
func run(_ word: String, meant: Layout, typed: Layout) -> String? {
    guard let k = keys(word, in: meant, typedIn: typed) else { return "?" }
    forcedLang = typed.lang
    return engine.autoFix(k)?.text(k)
}

let US = "com.apple.keylayout.US"
// system setups to test: the user's layouts in order
let setups: [(String, [String])] = [
    ("uk+en", [US, "com.apple.keylayout.Ukrainian-PC"]),
    ("uk+ru+en", [US, "com.apple.keylayout.Ukrainian-PC", "com.apple.keylayout.RussianWin"]),
    ("ru+en", [US, "com.apple.keylayout.RussianWin"]),
    ("de+en", [US, "com.apple.keylayout.German"]),
    ("el+en", [US, "com.apple.keylayout.Greek"]),
    ("bg+en", [US, "com.apple.keylayout.Bulgarian"]),
    ("he+en", [US, "com.apple.keylayout.Hebrew"]),
    ("ar+en", [US, "com.apple.keylayout.Arabic"]),
    ("pl+en", [US, "com.apple.keylayout.Polish"]),
    ("sr+en", [US, "com.apple.keylayout.Serbian"]),
    ("mk+en", [US, "com.apple.keylayout.Macedonian"]),
    ("sk+en", [US, "com.apple.keylayout.Slovak"]),
    ("hr+en", [US, "com.apple.keylayout.Croatian"]),
    ("sl+en", [US, "com.apple.keylayout.Slovenian"]),
    ("lt+en", [US, "com.apple.keylayout.Lithuanian"]),
    ("lv+en", [US, "com.apple.keylayout.Latvian"]),
    ("et+en", [US, "com.apple.keylayout.Estonian"]),
    ("nl+en", [US, "com.apple.keylayout.Dutch"]),
    ("da+en", [US, "com.apple.keylayout.Danish"]),
    ("nb+en", [US, "com.apple.keylayout.Norwegian"]),
    ("is+en", [US, "com.apple.keylayout.Icelandic"]),
    ("sq+en", [US, "com.apple.keylayout.Albanian"]),
    ("ka+en", [US, "com.apple.keylayout.Georgian-QWERTY"]),
    ("fa+en", [US, "com.apple.keylayout.Persian"]),
    ("vi+en", [US, "com.apple.keylayout.Vietnamese"]),
    ("ko+en", [US, "com.apple.keylayout.2SetHangul"]),
    ("kk+en", [US, "com.apple.keylayout.Kazakh"]),
    ("ky+en", [US, "com.apple.keylayout.Kyrgyz-Cyrillic"]),
    ("tg+en", [US, "com.apple.keylayout.Tajik-Cyrillic"]),
    ("mn+en", [US, "com.apple.keylayout.Mongolian-Cyrillic"]),
    ("hy+en", [US, "com.apple.keylayout.Armenian-HMQWERTY"]),
    ("az+en", [US, "com.apple.keylayout.Azeri"]),
    ("mt+en", [US, "com.apple.keylayout.Maltese"]),
    ("ga+en", [US, "com.apple.keylayout.IrishExtended"]),
    ("cy+en", [US, "com.apple.keylayout.Welsh"]),
    ("hi+en", [US, "com.apple.keylayout.Devanagari"]),
    ("be+en", [US, "com.apple.keylayout.Byelorussian"]),
    ("cs+en", [US, "com.apple.keylayout.Czech"]),
    ("fi+en", [US, "com.apple.keylayout.Finnish"]),
    ("ro+en", [US, "com.apple.keylayout.Romanian"]),
    ("hu+en", [US, "com.apple.keylayout.Hungarian"]),
    ("es+en", [US, "com.apple.keylayout.Spanish"]),
    ("fr+en", [US, "com.apple.keylayout.French"]),
    ("it+en", [US, "com.apple.keylayout.Italian"]),
    ("pt+en", [US, "com.apple.keylayout.Portuguese"]),
    ("tr+en", [US, "com.apple.keylayout.Turkish"]),
    ("sv+en", [US, "com.apple.keylayout.Swedish"]),
    ("uk+pl+en", [US, "com.apple.keylayout.Ukrainian-PC", "com.apple.keylayout.Polish"]),
    ("kk+ru+en", [US, "com.apple.keylayout.Kazakh", "com.apple.keylayout.RussianWin"]),
]

func words(_ lang: String, _ n: Int) -> [String] {
    guard let s = try? String(contentsOf: langDir!.appendingPathComponent(lang + ".txt"), encoding: .utf8),
          let line = s.split(separator: "\n").first(where: { !$0.hasPrefix("#") }) else { return [] }
    // the lists come from film subtitles and have other languages' words (Russian in Ukrainian):
    // keep only what the language's own dictionary accepts, when it has one
    let dict = engine.dictionaryWorks(lang)
    return line.split(separator: " ").map(String.init)
        .filter { $0.count >= 2 && $0.allSatisfy(\.isLetter) && (!dict || engine.isWord($0, lang)) }.prefix(n).map { $0 }
}

var results: [(String, Int, Int)] = []   // name, ok, total
var failures: [String] = []
func check(_ name: String, _ cases: [(String, Layout, Layout, String?)]) {
    var ok = 0, total = 0
    for (w, meant, typed, want) in cases {
        let got = run(w, meant: meant, typed: typed)
        // nothing to fix: the word types the same in both layouts (most German ↔ English letters)
        if got == "?" || (want != nil && keys(w, in: meant, typedIn: typed).map(typed.text) == w) { continue }
        total += 1
        if got == want { ok += 1 } else if failures.count < 400 { failures.append("\(name): \(w) → \(got ?? "unchanged")") }
    }
    results.append((name, ok, total))
}

let N = 300
func isCyr(_ l: String) -> Bool { ["uk", "ru", "be", "bg", "kk"].contains(l) }
for (name, ids) in setups {
    let ls = ids.map(layout)
    engine.layouts = ls
    engine.reliable = [:]
    let en = ls[0]
    let enWords = words("en", N)
    for l in ls.dropFirst() {
        let own = words(l.lang, N)
        // meant in l, typed in English layout → fixed to the word; typed in l → left alone
        check("\(name) \(l.lang) typed in en: fixed", own.map { ($0, l, en, $0) })
        check("\(name) \(l.lang) typed right: kept", own.map { ($0, l, l, nil) })
        // English meant, typed in l → fixed
        check("\(name) en typed in \(l.lang): fixed", enWords.map { ($0, en, l, $0) })
    }
    check("\(name) en typed right: kept", enWords.map { ($0, en, en, nil) })
    // Cyrillic pairs: one language typed in another's layout (only words that look different there)
    for a in ls.dropFirst() { for b in ls.dropFirst() where a !== b && isCyr(a.lang) && isCyr(b.lang) {
        let ws = words(a.lang, N).filter { w in keys(w, in: a, typedIn: b).map { b.text($0) != w } ?? false }
        check("\(name) \(a.lang) typed in \(b.lang): fixed", ws.map { ($0, a, b, $0) })
    } }
}

// hand-picked cases that must always work (meant layout, typed layout, word, expected result; "-" = left alone)
engine.layouts = [layout(US), layout("com.apple.keylayout.Ukrainian-PC")]; engine.reliable = [:]
let must: [(String, String, String, String?)] = [
    ("uk", "en", "привіт", "привіт"), ("en", "uk", "hello", "hello"),
    ("en", "uk", "console.log(total);", "console.log(total);"), ("en", "uk", "fetchUser(id);", "fetchUser(id);"),
    ("en", "en", "test1", nil), ("en", "en", "h2o", nil), ("en", "en", "github", nil), ("uk", "uk", "гуглити", nil),
]
let byLang = Dictionary(uniqueKeysWithValues: engine.layouts.map { ($0.lang, $0) })
var mustFail = 0
for (m, t, w, want) in must {
    let got = run(w, meant: byLang[m]!, typed: byLang[t]!)
    if got != want { mustFail += 1; print("MUST FAIL: \(w) (\(m) typed in \(t)) → \(got ?? "unchanged"), expected \(want ?? "unchanged")") }
}

// signs next to a word: «ghbdsn,» (the comma key is «б» in Ukrainian) and «"ghbdsn» (the quote key is «Є») must still be fixed,
// the signs stay as typed; a real sign after a right word changes nothing. (meant word with the letters the sign keys give, typed in, expected)
engine.layouts = [layout(US), layout("com.apple.keylayout.Ukrainian-PC")]; engine.reliable = [:]
let uk = engine.layouts[1], enL = engine.layouts[0], ruApple = layout("com.apple.keylayout.Russian")
engine.layouts.append(ruApple)
for (meant, m, t, want) in [("привітб", uk, enL, "привіт,"), ("привітю", uk, enL, "привіт."), ("Єпривіт", uk, enL, "\"привіт"),
                            ("Єпривітб", uk, enL, "\"привіт,"), ("hello,", enL, enL, nil), ("привіт,", uk, uk, nil),
                            // Apple «Russian»: the comma is ⇧6, and ⇧/ types «?» — a right word with either sign is left alone
                            ("привет?", ruApple, ruApple, nil), ("привет,", ruApple, ruApple, nil)] as [(String, Layout, Layout, String?)] {
    guard let k = keys(meant, in: m, typedIn: t) else { mustFail += 1; print("MUST FAIL: no keys for \(meant)"); continue }
    forcedLang = t.lang
    let r = engine.autoFixEdges(k)
    let got = r.map { engine.textKeepingEdges(k, $0.layout, typed: t, lead: $0.lead, tail: $0.tail) }
    if got != want { mustFail += 1; print("MUST FAIL: signs \(meant) → \(got ?? "unchanged"), expected \(want ?? "unchanged")") }
}

// same keys give a frequent Russian word and a rare Ukrainian one («было» / «біло»): the frequent one wins
engine.layouts = [layout(US), layout("com.apple.keylayout.Ukrainian-PC"), layout("com.apple.keylayout.RussianWin")]; engine.reliable = [:]
for w in ["было", "была", "были"] {
    let got = run(w, meant: engine.layouts[2], typed: engine.layouts[0])
    if got != w { mustFail += 1; print("MUST FAIL: \(w) (ru typed in en, uk+ru+en) → \(got ?? "unchanged")") }
}

// handsOff: Backspace/Delete on an emptied field must not leave the next word unchecked (audit P1 №1)
func press(_ code: CGKeyCode) { engine.handleKey(CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: true)!) }
for (name, code, empty, want) in [("backspace, field empty", CGKeyCode(51), Bool?.some(true), false),
                                  ("forward delete, field empty", 117, true, false),
                                  ("backspace, text left", 51, false, true),
                                  ("backspace, app doesn't say", 51, nil, true),
                                  ("arrow, field empty", 123, true, true)] {
    engine.reset(); engine.fieldEmpty = { empty }
    press(code); if code == 51 || code == 117 { engine.releaseIfEmptyNow() }
    if engine.handsOff != want { mustFail += 1; print("MUST FAIL: handsOff \(name): got \(engine.handsOff), want \(want)") }
}
engine.reset(); engine.fieldEmpty = { false }; press(0); press(49); press(51)  // «a⎵⌫»: back over the space — the word is the user's
engine.releaseIfEmptyNow()
if !engine.handsOff { mustFail += 1; print("MUST FAIL: handsOff after backspace over a space") }
engine.reset(); engine.fieldEmpty = { Engine.axFieldEmpty() }

// report and compare with the baseline
let out = results.map { String(format: "%@\t%.1f%%\t%d/%d", $0.0, 100 * Double($0.1) / Double(max($0.2, 1)), $0.1, $0.2) }
if CommandLine.arguments.contains("-v") { for l in out { print(l) } }
let failFile = root.appendingPathComponent("tests/failures.txt")
try? failures.joined(separator: "\n").write(to: failFile, atomically: true, encoding: .utf8)
// spell-check dictionaries differ between macOS versions: one baseline per version
let baseFile = root.appendingPathComponent("tests/baseline-macos\(ProcessInfo.processInfo.operatingSystemVersion.majorVersion).txt")
if CommandLine.arguments.contains("--update") {
    try! (out.joined(separator: "\n") + "\n").write(to: baseFile, atomically: true, encoding: .utf8)
    print("baseline updated"); exit(mustFail == 0 ? 0 : 1)
}
var worse = 0
if let base = try? String(contentsOf: baseFile, encoding: .utf8) {
    var old: [String: Double] = [:]
    for l in base.split(separator: "\n") { let p = l.split(separator: "\t"); if p.count >= 2 { old[String(p[0])] = Double(p[1].dropLast()) } }
    for (name, ok, total) in results {
        let now = 100 * Double(ok) / Double(max(total, 1))
        if let was = old[name], now < was - 1.0 { worse += 1; print(String(format: "WORSE: %@ %.1f%% (was %.1f%%)", name, now, was)) }
    }
}
print(mustFail == 0 && worse == 0 ? "OK" : "FAILED: \(mustFail) must-pass, \(worse) worse than baseline (details: tests/failures.txt)")
exit(mustFail == 0 && worse == 0 ? 0 : 1)
