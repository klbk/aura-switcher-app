import Cocoa
import Carbon
import ServiceManagement
import ApplicationServices

// single instance: a second copy would double every correction. Two copies started at the same moment
// (login item + manual launch) see each other — the earliest launched (then the lowest pid) stays.
let me = NSRunningApplication.current
func launchedBefore(_ a: NSRunningApplication, _ b: NSRunningApplication) -> Bool {
    let da = a.launchDate ?? .distantFuture, db = b.launchDate ?? .distantFuture
    return da != db ? da < db : a.processIdentifier < b.processIdentifier
}
if NSRunningApplication.runningApplications(withBundleIdentifier: Bundle.main.bundleIdentifier ?? "")
    .contains(where: { $0 != me && launchedBefore($0, me) }) { exit(0) }

let engine = Engine()

// MARK: - App / menu

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {
    var item: NSStatusItem!
    let menu = NSMenu()
    var s: Settings { engine.settings }

    func applicationDidFinishLaunching(_ n: Notification) {
        item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        menu.delegate = self
        item.menu = menu
        updateTitle()

        if UserDefaults.standard.object(forKey: "loginSet") == nil {
            try? SMAppService.mainApp.register()
            UserDefaults.standard.set(true, forKey: "loginSet")
        }

        DistributedNotificationCenter.default().addObserver(forName: NSNotification.Name(kTISNotifySelectedKeyboardInputSourceChanged as String),
                                                            object: nil, queue: .main) { _ in
            let now = currentLang()
            if now != engine.ownLang {  // the user switched: key events carry the right letters again
                engine.ownLang = nil
                // …in the middle of a word: they're mixing layouts on purpose («RghРак») — it's their word
                // (macOS sometimes repeats or delays the notice: only a real change from the word's layout counts)
                if !engine.word.isEmpty, now != engine.wordStartLang { engine.reset(); engine.handsOff = true }
            }
            auraLog.notice("layout now: \(now ?? "?", privacy: .public) in \(engine.frontBundle, privacy: .public)")
            self.updateTitle()
        }
        DistributedNotificationCenter.default().addObserver(forName: NSNotification.Name(kTISNotifyEnabledKeyboardInputSourcesChanged as String),
                                                            object: nil, queue: .main) { _ in engine.layouts = loadLayouts(); engine.codes = engine.allCodes; engine.lastSelOut = nil; engine.warmUpDictionaries() }
        NSWorkspace.shared.notificationCenter.addObserver(forName: NSWorkspace.didActivateApplicationNotification,
                                                          object: nil, queue: .main) { note in
            guard let app = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication,
                  let id = app.bundleIdentifier, id != Bundle.main.bundleIdentifier else { return }
            engine.frontBundle = id
            engine.frontIsGame = isGame(app)
            engine.recent = Array(([id] + engine.recent.filter { $0 != id }).prefix(6))
            engine.reset()
            if engine.settings.enabled, let lang = engine.settings.bindings[id], let l = engine.layout(lang) { engine.select(l) }
        }

        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue(): true] as CFDictionary
        AXIsProcessTrustedWithOptions(opts)
        engine.onTapLost = { [weak self] in self?.item.button?.title = "⚠︎"; DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self?.startTap() } }
        startTap()
        // the selection-changed notification is sometimes missed — re-check the indicator twice a second
        Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { _ in
            if engine.tap != nil, !AXIsProcessTrusted() { engine.dropTap(); return }
            self.updateTitle()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) { engine.warmUpDictionaries() }
    }

    var retrying = false
    func startTap() {
        guard engine.tap == nil else { return }
        NSLog("AURA trusted=\(AXIsProcessTrusted()) listen=\(CGPreflightListenEventAccess()) layouts=\(engine.layouts.map(\.lang)) spell=\(NSSpellChecker.shared.availableLanguages) cur=\(currentLang() ?? "nil")")
        // listen-only taps are created even without access but then receive nothing
        if AXIsProcessTrusted(), engine.start() { retrying = false; NSLog("AURA tap ok"); updateTitle(); return }
        item.button?.title = "⚠︎"
        if retrying { return }
        retrying = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.retrying = false; self.startTap() }
    }

    func updateTitle() {
        guard engine.tap != nil else { return }
        let l = displayCode(currentLang())
        let title = (engine.active ? l : l.lowercased()) + (updateDue() ? "•" : "")
        if item.button?.title != title { item.button?.title = title }
    }

    func add(_ m: NSMenu, _ title: String, _ on: Bool? = nil, enabled: Bool = true, _ action: (() -> Void)? = nil) {
        let i = ClosureItem(title: title, action: action)
        if let on { i.state = on ? .on : .off }
        i.isEnabled = enabled && action != nil
        m.addItem(i)
    }

    /// checkbox pictures for the on/off switches, drawn once; template images follow the light/dark theme and the highlight
    static let boxOn = checkbox(on: true), boxOff = checkbox(on: false)
    static func checkbox(on: Bool) -> NSImage {
        let img = NSImage(size: NSSize(width: 14, height: 14), flipped: false) { _ in
            let r = NSRect(x: 1.5, y: 1.5, width: 11, height: 11)
            let box = NSBezierPath(roundedRect: r, xRadius: 3, yRadius: 3)
            NSColor.black.setStroke(); NSColor.black.setFill()
            if on {
                box.fill()   // filled square, the tick is cut out of it
                let tick = NSBezierPath()
                tick.move(to: NSPoint(x: 4.0, y: 7.2)); tick.line(to: NSPoint(x: 6.2, y: 4.9)); tick.line(to: NSPoint(x: 10.2, y: 9.6))
                tick.lineWidth = 1.5; tick.lineCapStyle = .round; tick.lineJoinStyle = .round
                NSGraphicsContext.current?.compositingOperation = .clear
                tick.stroke()
            } else {
                box.lineWidth = 1.2; box.stroke()
            }
            return true
        }
        img.isTemplate = true
        return img
    }

    /// an on/off switch of the menu: a drawn checkbox picture (on or off), not the native check mark
    func toggle(_ m: NSMenu, _ title: String, _ on: Bool, enabled: Bool = true, _ action: @escaping () -> Void) {
        let i = ClosureItem(title: title, action: action)
        // the pictures go to the state column (same alignment as the other items); an item image is not drawn in this menu
        i.onStateImage = AppDelegate.boxOn; i.offStateImage = AppDelegate.boxOff
        i.state = on ? .on : .off
        i.isEnabled = enabled
        m.addItem(i)
    }

    func appName(_ id: String) -> String {
        guard let url = NSWorkspace.shared.urlForApplication(withBundleIdentifier: id) else { return id }
        return FileManager.default.displayName(atPath: url.path).replacingOccurrences(of: ".app", with: "")
    }

    /// Opens an email to the developer with version, macOS and layouts already filled in.
    func reportProblem() {
        let ver = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "?"
        let os = ProcessInfo.processInfo.operatingSystemVersionString
        let body = "\(L("reportBody"))\n\n\n---\nAURA Switcher \(ver) · macOS \(os) · \(engine.layouts.map { displayCode($0.lang) }.joined(separator: ", "))"
        var c = URLComponents(); c.scheme = "mailto"; c.path = "auraswitcher@gmail.com"
        c.queryItems = [URLQueryItem(name: "subject", value: "AURA Switcher \(ver) — macOS"), URLQueryItem(name: "body", value: body)]
        if let url = c.url { NSWorkspace.shared.open(url) }
    }

    func editWords() {
        let a = NSAlert()
        a.messageText = L("words").replacingOccurrences(of: "…", with: "")
        a.informativeText = L("wordsInfo")
        let field = NSTextField(frame: NSRect(x: 0, y: 0, width: 320, height: 24))
        field.stringValue = s.words.joined(separator: ", ")
        field.placeholderString = "хз, ща, яж"
        a.accessoryView = field
        a.addButton(withTitle: L("save")); a.addButton(withTitle: L("cancel"))
        a.window.initialFirstResponder = field
        NSApp.activate(ignoringOtherApps: true)
        if a.runModal() == .alertFirstButtonReturn { s.words = parseWords(field.stringValue) }
    }

    func menuNeedsUpdate(_ m: NSMenu) {
        m.removeAllItems()
        toggle(m, L("enabled"), s.enabled) { self.s.enabled.toggle(); self.updateTitle() }
        toggle(m, L("auto"), s.auto, enabled: s.enabled) { self.s.auto.toggle() }
        toggle(m, L("sound"), s.sound, enabled: s.enabled) { self.s.sound.toggle(); if self.s.sound { Sounds.play(Sounds.auto) } }
        let login = SMAppService.mainApp.status == .enabled
        toggle(m, L("login"), login) {
            if login { try? SMAppService.mainApp.unregister() } else { try? SMAppService.mainApp.register() }
        }
        let hk = s.hotkey
        let hkSym = ["shift": "⇧", "ctrl": "⌃", "alt": "⌥"][hk]
        let hkNames = [("shift", "⇧⇧ Shift"), ("ctrl", "⌃⌃ Ctrl"), ("alt", "⌥⌥ Option")]
        let hkMenu = NSMenu()
        add(hkMenu, L("hotkey"))   // title of the submenu (not clickable)
        hkMenu.addItem(.separator())
        for (k, n) in hkNames { add(hkMenu, n, hk == k) { self.s.hotkey = k } }
        add(hkMenu, L("off"), hk == "off") { self.s.hotkey = "off" }
        // one line: the hint with the chosen key's symbol in place of the translated ⇧; «Correction key: off» when it is off
        let hkTitle = hkSym.map { L("hint").replacingOccurrences(of: "⇧", with: $0) } ?? L("hotkey") + ": " + L("off").lowercased()
        let hi = NSMenuItem(title: hkTitle, action: nil, keyEquivalent: ""); hi.submenu = hkMenu
        m.addItem(hi)
        if engine.tap == nil { add(m, L("access"), enabled: true) {
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!) } }
        m.addItem(.separator())

        let id = engine.frontBundle
        add(m, L("app") + appName(id))
        if engine.frontIsGame { add(m, L("game")) }
        let ex = s.excluded.contains(id)
        toggle(m, L("exclude"), ex) {
            self.s.excluded = ex ? self.s.excluded.filter { $0 != id } : self.s.excluded + [id]
        }
        let langMenu = NSMenu()
        add(langMenu, L("noBinding"), s.bindings[id] == nil) { self.s.bindings[id] = nil }
        for l in engine.layouts {
            add(langMenu, langName(l.lang), s.bindings[id] == l.lang) { self.s.bindings[id] = l.lang; engine.select(l) }
        }
        let li = NSMenuItem(title: L("appLang"), action: nil, keyEquivalent: "")
        li.submenu = langMenu
        m.addItem(li)
        m.addItem(.separator())

        let setMenu = NSMenu()
        let exMenu = NSMenu()
        if s.excluded.isEmpty { add(exMenu, emptyHint("exclude")) }
        for e in s.excluded { add(exMenu, "✕ \(appName(e))") { self.s.excluded = self.s.excluded.filter { $0 != e } } }

        exMenu.addItem(.separator())
        if engine.recent.isEmpty { add(exMenu, L("empty")) }
        for r in engine.recent {
            let on = s.excluded.contains(r)
            add(exMenu, appName(r), on) { self.s.excluded = on ? self.s.excluded.filter { $0 != r } : self.s.excluded + [r] }
        }

        let ei = NSMenuItem(title: L("exclusions"), action: nil, keyEquivalent: ""); ei.submenu = exMenu; setMenu.addItem(ei)
        let bMenu = NSMenu()
        if s.bindings.isEmpty { add(bMenu, emptyHint("appLang")) }
        for (b, l) in s.bindings.sorted(by: { $0.key < $1.key }) {
            add(bMenu, "✕ \(appName(b)) → \(displayCode(l))") { self.s.bindings[b] = nil }
        }

        let bi = NSMenuItem(title: L("bindings"), action: nil, keyEquivalent: ""); bi.submenu = bMenu; setMenu.addItem(bi)
        add(setMenu, L("words")) { self.editWords() }
        let uiMenu = NSMenu()
        add(uiMenu, L("system"), s.uiLang == nil) { self.s.uiLang = nil }
        let nameOf = Dictionary(uniqueKeysWithValues: zip(uiLangs, uiLangNames))
        func addUI(_ menu: NSMenu, _ c: String) { add(menu, nameOf[c] ?? c, s.uiLang == c) { self.s.uiLang = c } }
        uiMenu.addItem(.separator())
        for c in uiTopLangs(current: uiCode(), layoutLangs: engine.layouts.map(\.lang)) { addUI(uiMenu, c) }
        uiMenu.addItem(.separator())
        let allMenu = NSMenu()   // every language, by region, so the list stays short when there are many
        for (i, (key, _)) in uiRegions.enumerated() {
            let codes = uiLangs.filter { uiRegionIndex($0) == i }
            if codes.isEmpty { continue }
            let rm = NSMenu(); for c in codes { addUI(rm, c) }
            let ri = NSMenuItem(title: L(key), action: nil, keyEquivalent: ""); ri.submenu = rm; allMenu.addItem(ri)
        }
        let ai = NSMenuItem(title: L("allLangs"), action: nil, keyEquivalent: ""); ai.submenu = allMenu; uiMenu.addItem(ai)
        let ui = NSMenuItem(title: L("uiLang"), action: nil, keyEquivalent: ""); ui.submenu = uiMenu; setMenu.addItem(ui)
        let si = NSMenuItem(title: L("settings"), action: nil, keyEquivalent: ""); si.submenu = setMenu; m.addItem(si)
        m.addItem(.separator())
        let ver = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? ""
        add(m, updateDue() ? L("updateDue") : L("update") + " (\(ver) beta)") {
            self.s.updateCheckedAt = Date().timeIntervalSince1970
            self.updateTitle()
            Updater.check()
        }
        add(m, L("report")) { self.reportProblem() }
        add(m, L("donate")) { NSWorkspace.shared.open(URL(string: "https://donatello.to/aura-switcher")!) }
        add(m, L("quit")) { NSApp.terminate(nil) }
    }
}

final class ClosureItem: NSMenuItem {
    var handler: (() -> Void)?
    init(title: String, action: (() -> Void)?) {
        handler = action
        super.init(title: title, action: #selector(run), keyEquivalent: "")
        target = self
    }
    required init(coder: NSCoder) { fatalError() }
    @objc func run() { handler?() }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
