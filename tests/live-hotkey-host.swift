// The window side of tests/live-hotkey.swift: a real .app (so LaunchServices gives it the focus and AURA sees a bundle id).
// It only shows a text field and reports its state to a file; the process that posts the keys is a separate one
// (it has the permission to post events). Commands arrive through a file. Args: <dir>
import Cocoa

let dir = CommandLine.arguments[1]
let stateFile = dir + "/state", cmdFile = dir + "/cmd"
UserDefaults.standard.set(false, forKey: "NSAutomaticPeriodSubstitutionEnabled")  // this app only: «hello  » must stay as typed
let app = NSApplication.shared; app.setActivationPolicy(.regular)
func make(_ title: String, secure: Bool, x: CGFloat) -> (NSWindow, NSResponder) {
    let w = NSWindow(contentRect: NSRect(x: x, y: 250, width: 420, height: 140), styleMask: [.titled], backing: .buffered, defer: false)
    w.title = title
    w.isReleasedWhenClosed = false   // close() would release it once more under ARC (crash in the timer's autorelease pool)
    let r: NSResponder
    if secure { let f = NSSecureTextField(frame: NSRect(x: 10, y: 50, width: 400, height: 24)); w.contentView!.addSubview(f); r = f }
    else {
        let tv = NSTextView(frame: w.contentView!.bounds)
        tv.isAutomaticQuoteSubstitutionEnabled = false; tv.isAutomaticTextReplacementEnabled = false; tv.isAutomaticDashSubstitutionEnabled = false; tv.isAutomaticTextCompletionEnabled = false; tv.isAutomaticSpellingCorrectionEnabled = false
        w.contentView!.addSubview(tv); r = tv
    }
    w.makeKeyAndOrderFront(nil); w.makeFirstResponder(r)
    return (w, r)
}
var (main, mainR) = make("AURA live test", secure: false, x: 200)
var extra: NSWindow?
var kind = "plain"
var tv: NSTextView { mainR as! NSTextView }
func focus() { app.activate(ignoringOtherApps: true) }
focus()

func command(_ c: String) {
    switch c {
    case "clear": if kind == "plain" { tv.string = "" }; main.makeKeyAndOrderFront(nil); main.makeFirstResponder(mainR); focus()
    case "selectall": tv.selectAll(nil)
    case "secure": main.close(); (main, mainR) = make("AURA live test (password)", secure: true, x: 200); kind = "secure"; focus()
    case "plain": main.close(); (main, mainR) = make("AURA live test", secure: false, x: 200); kind = "plain"; focus()
    case "second": extra = make("AURA live test 2", secure: false, x: 700).0; focus()
    case "first": extra?.close(); extra = nil; main.makeKeyAndOrderFront(nil); focus()
    case "quit": NSApp.terminate(nil)
    default: break
    }
}
Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { _ in
    if let c = try? String(contentsOfFile: cmdFile, encoding: .utf8), !c.isEmpty {
        try? FileManager.default.removeItem(atPath: cmdFile)
        for l in c.split(separator: "\n") { command(String(l)) }
    }
    let keyWin = NSApp.keyWindow
    let text = (keyWin === main && kind == "plain") ? tv.string : (keyWin === extra ? ((extra?.contentView?.subviews.first as? NSTextView)?.string ?? "") : "")
    let s = "active=\(NSApp.isActive ? 1 : 0) key=\(keyWin != nil ? 1 : 0) kind=\(kind) win=\(keyWin?.title ?? "-")\ntext=\(text.replacingOccurrences(of: "\n", with: "\\n"))\n"
    try? s.write(toFile: stateFile, atomically: true, encoding: .utf8)
}
app.run()
