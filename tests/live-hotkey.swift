// Live test of the correction key on a Mac with AURA Switcher running (listen-only tap): a window of its own,
// synthetic modifier/key events, the result is read from the system (current keyboard layout, text in the window).
//   tests/live-hotkey.sh      — builds and runs; prints a markdown table «combination | expected | got | ✅/⚠️»
// Needs: the AURA app running with Accessibility/Input Monitoring, the process may post events (CGPreflightPostEventAccess),
// at least 2 keyboard layouts enabled (the cycle is checked on all of them). The layout the user had is restored.
import Cocoa
import Carbon

let defaults = UserDefaults(suiteName: "com.chernov.aura")!

func layoutIDs() -> [String] {
    let f = [kTISPropertyInputSourceCategory: kTISCategoryKeyboardInputSource!, kTISPropertyInputSourceIsSelectCapable: true] as CFDictionary
    return (TISCreateInputSourceList(f, false).takeRetainedValue() as! [TISInputSource]).map(idOf)
}
func idOf(_ s: TISInputSource) -> String { Unmanaged<CFString>.fromOpaque(TISGetInputSourceProperty(s, kTISPropertyInputSourceID)).takeUnretainedValue() as String }
func cur() -> String { idOf(TISCopyCurrentKeyboardInputSource().takeRetainedValue()) }
func select(_ id: String) {
    let f = [kTISPropertyInputSourceID: id] as CFDictionary
    if let s = (TISCreateInputSourceList(f, false).takeRetainedValue() as? [TISInputSource])?.first { TISSelectInputSource(s) }
}
let ids = layoutIDs()
let n = ids.count
func idx() -> Int { ids.firstIndex(of: cur()) ?? -1 }
func short(_ i: Int) -> String { i < 0 ? "?" : ids[i].replacingOccurrences(of: "com.apple.keylayout.", with: "") }

guard CGPreflightPostEventAccess(), n >= 2 else { print("need event-posting access and 2+ layouts"); exit(2) }
let startID = cur()

// the window lives in another process (tests/live-hotkey-host.swift, a real .app: it gets the focus from LaunchServices);
// this one posts the keys and reads the host's state from a file
let dir = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "/tmp/aura-live"
let hostBundle = "local.aura.livetest"
func hostState() -> (active: Bool, key: Bool, win: String, text: String) {
    let t = (try? String(contentsOfFile: dir + "/state", encoding: .utf8)) ?? ""
    var ls = t.components(separatedBy: "\n")
    let head = ls.first ?? ""; ls.removeFirst()
    let text = (ls.first ?? "").replacingOccurrences(of: "text=", with: "", options: .anchored).replacingOccurrences(of: "\\n", with: "\n")
    return (head.contains("active=1"), head.contains("key=1"), head.components(separatedBy: "win=").last ?? "", text)
}
func host(_ c: String) { try? (c + "\n").write(toFile: dir + "/cmd", atomically: true, encoding: .utf8); spin(0.4) }
func hostText() -> String { hostState().text }
func spin(_ s: Double) { RunLoop.current.run(until: Date().addingTimeInterval(s)) }

enum K { static let sh = (l: CGKeyCode(56), r: CGKeyCode(60), m: CGEventFlags.maskShift)
         static let ct = (l: CGKeyCode(59), r: CGKeyCode(62), m: CGEventFlags.maskControl)
         static let op = (l: CGKeyCode(58), r: CGKeyCode(61), m: CGEventFlags.maskAlternate) }
func mod(_ code: CGKeyCode, _ m: CGEventFlags, _ down: Bool, _ flags: CGEventFlags? = nil) {
    let e = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: down)!
    e.type = .flagsChanged; e.flags = down ? (flags ?? m) : []
    e.post(tap: .cghidEventTap); spin(0.02)
}
func key(_ code: CGKeyCode, flags: CGEventFlags = []) {
    for d in [true, false] { let e = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: d)!; e.flags = flags; e.post(tap: .cghidEventTap); spin(0.025) }
}
func tap(_ code: CGKeyCode, _ m: CGEventFlags, hold: Double = 0.04) { mod(code, m, true); spin(hold); mod(code, m, false) }
func dbl(_ code: CGKeyCode, _ m: CGEventFlags, gap: Double = 0.08, hold: Double = 0.04) { tap(code, m, hold: hold); spin(gap); tap(code, m, hold: hold) }
func type(_ s: String) {  // US keycodes for the few letters used
    let c: [Character: CGKeyCode] = ["h": 4, "e": 14, "l": 37, "o": 31, "a": 0, " ": 49]
    for ch in s { key(c[ch]!) }
}

/// SECTIONS=4,5 runs only those sections (1 keys, 2 cycle, 3 contexts, 3b windows, 4 timing, 5 system shortcut); default all
let sectionsEnv = ProcessInfo.processInfo.environment["SECTIONS"]?.split(separator: ",").map(String.init)
func want(_ n: String) -> Bool { sectionsEnv?.contains(n) ?? true }
var rows: [(String, String, String, Bool)] = []
setvbuf(stdout, nil, _IONBF, 0)
let resultsPath = CommandLine.arguments.count > 2 ? CommandLine.arguments[2] : "build/live-hotkey-results.md"
/// every row is printed and appended to the results file at once: a stop in the middle loses nothing
func record(_ r: (String, String, String, Bool)) {
    rows.append(r)
    let line = "| \(r.0) | \(r.1) | \(r.2) | \(r.3 ? "✅" : "⚠️") |\n"
    print(line, terminator: "")
    if let h = FileHandle(forWritingAtPath: resultsPath) { h.seekToEndOfFile(); h.write(line.data(using: .utf8)!); h.closeFile() }
}
try? "| combination | expected | got | |\n|---|---|---|---|\n".write(toFile: resultsPath, atomically: true, encoding: .utf8)
/// events go to the front app: never post them unless the host window is the front app's key window
/// (a remote-control app, the user's editor… would get the keys). The test stops at once otherwise.
func ownWindowFront() -> Bool {
    NSWorkspace.shared.frontmostApplication?.bundleIdentifier == hostBundle && hostState().active && hostState().key
}
func stop(_ why: String) -> Never {
    print("STOP: \(why) — front app: \(NSWorkspace.shared.frontmostApplication?.localizedName ?? "?")")
    defaults.set("shift", forKey: "hotkey"); select(startID); host("quit"); exit(3)
}
func reset(hotkey: String = "shift", layout: Int = 0) {
    defaults.set(hotkey, forKey: "hotkey"); spin(0.3)
    select(ids[layout]); spin(0.5)
    host("clear"); spin(0.4)
    if !ownWindowFront() { stop("the test window is not the key window of the front app (nothing was posted)") }
    // the text was cleared by the host, AURA still holds the keys of the last word: Return makes it forget them (as a real Return would)
    key(36); host("clear"); spin(0.3)
    if !ownWindowFront() { stop("the test window lost the focus after Return (nothing more was posted)") }
}
func next(_ i: Int, _ by: Int = 1) -> Int { (i + by) % n }
/// one case: setup, action, expected layout shift (nil = none) and optional expected text
func run(_ name: String, _ expected: String, hotkey: String = "shift", from: Int = 0, text: String? = nil, wait: Double = 0.9,
         _ action: () -> Void, check: () -> (String, Bool)) {
    reset(hotkey: hotkey, layout: from); action(); spin(wait)
    let (got, ok) = check(); record((name, expected, got, ok))
}
func shifted(_ from: Int, _ want: Int?) -> () -> (String, Bool) {  // want = steps forward in the cycle, nil = no change
    { let now = idx(); let d = ((now - from) % n + n) % n
      let got = d == 0 ? "no switch" : "→ \(short(now)) (+\(d))"
      return (got, d == (want ?? 0) % n) }
}

if want("1") {
// 1. which key fires, by setting (left/right)
for (hk, name) in [("shift", "Shift"), ("ctrl", "Ctrl"), ("alt", "Option"), ("off", "off")] {
    for (kn, kk) in [("Shift", K.sh), ("Ctrl", K.ct), ("Option", K.op)] { for (side, code) in [("L", kk.l), ("R", kk.r)] {
        let fires = (hk == "shift" && kn == "Shift") || (hk == "ctrl" && kn == "Ctrl") || (hk == "alt" && kn == "Option")
        run("setting \(name): double \(kn) \(side)", fires ? "switch +1" : "nothing", hotkey: hk, { dbl(code, kk.m) }, check: shifted(0, fires ? 1 : nil))
    } }
}
}
if want("2") {
// 2. cycle over all layouts (each press switches one step; wraps around)
reset(); var seq: [String] = []; var ok = true
for i in 1...(n + 1) { dbl(K.sh.l, K.sh.m); spin(0.8); let now = idx(); seq.append(short(now)); if now != i % n { ok = false } }
record(("cycle: \(n + 1) double Shifts from \(short(0))", ids.indices.map { short(($0 + 1) % n) }.joined(separator: " → ") + " → \(short(1 % n))", seq.joined(separator: " → "), ok))
}
if want("3") {
// 3. contexts
run("empty field", "switch +1", { dbl(K.sh.l, K.sh.m) }, check: shifted(0, 1))
run("word «hello», no space", "word converted to next layout, layout +1", {
    type("hello"); spin(0.2); dbl(K.sh.l, K.sh.m) }, check: { let (g, o) = shifted(0, 1)(); let t = hostText(); return ("\(g), text «\(t)»", o && t == "руддщ") })
run("word typed in the 2nd layout (keys of «hello»)", "converted back to «hello», layout → 1st", from: 1, {
    type("hello"); spin(0.2); dbl(K.sh.l, K.sh.m) }, check: { let t = hostText(); return ("layout \(short(idx())), text «\(t)»", idx() == 0 && t == "hello") })
run("word «hello» + one space", "previous word converted, layout +1", {
    type("hello "); spin(0.3); dbl(K.sh.l, K.sh.m) }, check: { let (g, o) = shifted(0, 1)(); let t = hostText(); return ("\(g), text «\(t)»", o && t == "руддщ ") })
run("word «hello» + two spaces", "text unchanged, layout +1", {
    type("hello  "); spin(0.3); dbl(K.sh.l, K.sh.m) }, check: { let (g, o) = shifted(0, 1)(); let t = hostText(); return ("\(g), text «\(t)»", o && t == "hello  ") })
run("selected word", "selection converted, layout +1", {
    type("hello"); spin(0.2); host("selectall"); dbl(K.sh.l, K.sh.m) }, check: { let (g, o) = shifted(0, 1)(); let t = hostText(); return ("\(g), text «\(t)»", o && t == "руддщ") })
}
if want("3b") {
do { // password field
    reset(); host("secure"); spin(0.4); if !ownWindowFront() { stop("password window not front") }
    dbl(K.sh.l, K.sh.m); spin(1.0); let (g, o) = shifted(0, 1)(); record(("password field, empty", "switch +1 (nothing is typed or copied)", g, o))
    reset(); host("secure"); spin(0.4); if !ownWindowFront() { stop("password window not front") }
    type("hello"); spin(0.3); dbl(K.sh.l, K.sh.m); spin(1.0); let (g2, o2) = shifted(0, 1)(); let t = hostText()
    record(("password field, typed «hello»", "text stays «hello», layout +1", "\(g2), text «\(t)»", o2 && t == "hello")); host("plain")
}
do { // another window of the app, right after
    reset(); host("second"); spin(0.4); if !ownWindowFront() { stop("second window not front") }
    dbl(K.sh.l, K.sh.m); spin(0.9); let a = idx(); host("first"); spin(0.4); if !ownWindowFront() { stop("first window not front") }
    dbl(K.sh.l, K.sh.m); spin(0.9); let b = idx()
    record(("two windows in a row", "+1 in each", "\(short(a)), then \(short(b))", a == next(0) && b == next(0, 2)))
}
}
if want("4") {
// 4. timing and interference (Shift L unless noted)
run("slow: 0.55 s between taps", "nothing", { tap(K.sh.l, K.sh.m); spin(0.55); tap(K.sh.l, K.sh.m) }, check: shifted(0, nil))
run("0.30 s between taps", "switch +1", { tap(K.sh.l, K.sh.m); spin(0.3); tap(K.sh.l, K.sh.m) }, check: shifted(0, 1))
run("3 fast taps", "exactly one switch", { for _ in 0..<3 { tap(K.sh.l, K.sh.m); spin(0.08) } }, check: shifted(0, 1))
run("4 fast taps", "two switches", wait: 1.5, { for _ in 0..<4 { tap(K.sh.l, K.sh.m); spin(0.08) } }, check: shifted(0, 2))
run("L then R", "counts as a double: switch +1", { tap(K.sh.l, K.sh.m); spin(0.08); tap(K.sh.r, K.sh.m) }, check: shifted(0, 1))
run("hold 0.5 s, then tap", "nothing (a hold is not a tap)", { tap(K.sh.l, K.sh.m, hold: 0.5); spin(0.08); tap(K.sh.l, K.sh.m) }, check: shifted(0, nil))
run("tap, then hold 0.5 s", "nothing", { tap(K.sh.l, K.sh.m); spin(0.08); tap(K.sh.l, K.sh.m, hold: 0.5) }, check: shifted(0, nil))
run("⇧A twice", "nothing", { for _ in 0..<2 { mod(K.sh.l, K.sh.m, true); key(0, flags: .maskShift); mod(K.sh.l, K.sh.m, false); spin(0.08) } }, check: shifted(0, nil))
run("tap, letter between, tap", "nothing", { tap(K.sh.l, K.sh.m); spin(0.05); key(0); spin(0.05); tap(K.sh.l, K.sh.m) }, check: shifted(0, nil))
run("⌃C twice (Ctrl key)", "nothing", hotkey: "ctrl", { for _ in 0..<2 { mod(K.ct.l, K.ct.m, true); key(8, flags: .maskControl); mod(K.ct.l, K.ct.m, false); spin(0.08) } }, check: shifted(0, nil))
run("⌥+letter twice (Option key)", "nothing", hotkey: "alt", { for _ in 0..<2 { mod(K.op.l, K.op.m, true); key(0, flags: .maskAlternate); mod(K.op.l, K.op.m, false); spin(0.08) } }, check: shifted(0, nil))
run("Shift + Ctrl held together, twice", "nothing", { for _ in 0..<2 { mod(K.sh.l, K.sh.m, true); mod(K.ct.l, K.ct.m, true, [.maskShift, .maskControl]); mod(K.ct.l, K.ct.m, false); mod(K.sh.l, K.sh.m, false); spin(0.08) } }, check: shifted(0, nil))
run("Caps Lock between taps", "nothing", { tap(K.sh.l, K.sh.m); spin(0.05); mod(57, .maskAlphaShift, true); mod(57, .maskAlphaShift, false); spin(0.05); tap(K.sh.l, K.sh.m) }, check: shifted(0, nil))
if CGEventSource.flagsState(.combinedSessionState).contains(.maskAlphaShift) { print("(Caps Lock got switched on by the test — turn it off)") }
}
if want("5") {
// 5. system shortcut Ctrl+Space (enabled: «select previous input source») together with double Ctrl
run("Ctrl+Space (system) with Ctrl as the key", "one system switch, AURA adds none", hotkey: "ctrl", {
    mod(K.ct.l, K.ct.m, true); key(49, flags: .maskControl); mod(K.ct.l, K.ct.m, false) }, check: { let now = idx(); let d = ((now - 0) % n + n) % n; return ("moved \(d) step(s) → \(short(now))", d == 1 || d == n - 1) })
run("tap Ctrl, then Ctrl+Space", "one system switch only", hotkey: "ctrl", {
    tap(K.ct.l, K.ct.m); spin(0.1); mod(K.ct.l, K.ct.m, true); key(49, flags: .maskControl); mod(K.ct.l, K.ct.m, false) }, check: { let now = idx(); let d = ((now - 0) % n + n) % n; return ("moved \(d) step(s) → \(short(now))", d == 1 || d == n - 1) })
}
if want("6"), let ruIdx = ids.firstIndex(where: { $0.hasSuffix(".Russian") || $0.hasSuffix(".RussianWin") }) {
    // «слово, » typed in a Russian layout: the same text with auto-correction on and with AURA switched off (Apple «Russian»
    // has the comma on ⇧6 — ⇧/ types «?»; RussianWin has it on ⇧/)
    var texts: [String] = []
    for on in [true, false] {
        defaults.set(on, forKey: "enabled"); reset(layout: ruIdx)
        for c: CGKeyCode in [5, 4, 11, 2, 17, 45] { key(c) }   // ghbdtn = привет
        key(44, flags: .maskShift); key(49); spin(0.8)
        texts.append(hostText())
    }
    defaults.removeObject(forKey: "enabled")
    record(("\(short(ruIdx)): «привет» + ⇧/ + space", "same text with AURA on and off", "on «\(texts[0])», off «\(texts[1])»", texts[0] == texts[1]))
}
// restore
defaults.set("shift", forKey: "hotkey"); select(startID); spin(0.5)
host("quit")
print("\n\(rows.filter { $0.3 }.count)/\(rows.count) as expected; layouts: \(ids.map { short(ids.firstIndex(of: $0)!) }.joined(separator: ", "))")
