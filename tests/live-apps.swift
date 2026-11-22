// Posts the keys for one phase of tests/live-apps.sh into the FRONT app, and only if that is the app named in the arguments
// (retried up to 3 times, a second apart; then it gives up). Usage: live-apps <bundle id> <phase> [pid]
// With a pid the front app must be exactly that process (a test copy of Chrome or VS Code has the same bundle id as the user's).
//   phases: empty (double ⇧ on an empty field) · word (type «ghbdsn», double ⇧) · space (type «ghbdsn »)
//           select (type «ghbdsn», select all, double ⇧) · clear (⌘A, Delete) · tab (Tab) · addr (⌘L, type «ghbdsn», double ⇧, Esc)
// Prints the layout reached; the text is read by the shell script (AppleScript / the test page's title).
import Cocoa
import Carbon
func idOf(_ s: TISInputSource) -> String { Unmanaged<CFString>.fromOpaque(TISGetInputSourceProperty(s, kTISPropertyInputSourceID)).takeUnretainedValue() as String }
func cur() -> String { idOf(TISCopyCurrentKeyboardInputSource().takeRetainedValue()).replacingOccurrences(of: "com.apple.keylayout.", with: "") }
func select(_ id: String) { let f = [kTISPropertyInputSourceID: "com.apple.keylayout." + id] as CFDictionary; if let s = (TISCreateInputSourceList(f, false).takeRetainedValue() as? [TISInputSource])?.first { TISSelectInputSource(s) } }
func spin(_ s: Double) { RunLoop.current.run(until: Date().addingTimeInterval(s)) }
let wantPid: pid_t? = CommandLine.arguments.count > 3 ? pid_t(CommandLine.arguments[3]) : nil
func front() -> String {
    guard let a = NSWorkspace.shared.frontmostApplication else { return "?" }
    if let p = wantPid { return a.processIdentifier == p ? CommandLine.arguments[1] : "pid \(a.processIdentifier) (\(a.bundleIdentifier ?? "?"))" }
    return a.bundleIdentifier ?? "?"
}
func mod(_ c: CGKeyCode, _ m: CGEventFlags, _ d: Bool) { let e = CGEvent(keyboardEventSource: nil, virtualKey: c, keyDown: d)!; e.type = .flagsChanged; e.flags = d ? m : []; e.post(tap: .cghidEventTap); spin(0.02) }
func dbl() { for i in 0..<2 { mod(56, .maskShift, true); spin(0.04); mod(56, .maskShift, false); if i == 0 { spin(0.08) } }; spin(1.0) }
func key(_ c: CGKeyCode, _ f: CGEventFlags = []) { for d in [true, false] { let e = CGEvent(keyboardEventSource: nil, virtualKey: c, keyDown: d)!; e.flags = f; e.post(tap: .cghidEventTap); spin(0.03) } }
let ghbdsn: [CGKeyCode] = [5, 4, 11, 2, 1, 45]
func ok(_ want: String) -> Bool { for _ in 0..<3 { if front() == want { return true }; spin(1.0) }; return false }
let want = CommandLine.arguments[1], phase = CommandLine.arguments[2]
func mustBeFront() { if !ok(want) { print("SKIP front=\(front())"); exit(3) } }
mustBeFront()
if ["empty", "word", "space", "select", "addr"].contains(phase) { select("ABC"); spin(0.6); mustBeFront() }
switch phase {
case "empty": dbl()
case "word": ghbdsn.forEach { key($0) }; spin(0.2); mustBeFront(); dbl()
case "space": ghbdsn.forEach { key($0) }; key(49); spin(1.0)
case "select": ghbdsn.forEach { key($0) }; spin(0.2); mustBeFront(); key(0, .maskCommand); spin(0.2); dbl()
case "clear": key(0, .maskCommand); spin(0.1); key(51); spin(0.3)
case "tab": key(48); spin(0.4)
case "addr": key(37, .maskCommand); spin(0.4); mustBeFront(); ghbdsn.forEach { key($0) }; spin(0.2); dbl(); key(53)
default: break
}
print(cur())
