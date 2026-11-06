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
            auraLog.notice("layout now: \(currentLang() ?? "?", privacy: .public) in \(engine.frontBundle, privacy: .public)")
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
        startTap()
        // the selection-changed notification is sometimes missed — re-check the indicator twice a second
        Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { _ in self.updateTitle() }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) { engine.warmUpDictionaries() }
    }

    func startTap() {
        NSLog("AURA trusted=\(AXIsProcessTrusted()) listen=\(CGPreflightListenEventAccess()) layouts=\(engine.layouts.map(\.lang)) spell=\(NSSpellChecker.shared.availableLanguages) cur=\(currentLang() ?? "nil")")
        // listen-only taps are created even without access but then receive nothing
        if AXIsProcessTrusted(), engine.start() { NSLog("AURA tap ok"); updateTitle(); return }
        item.button?.title = "⚠︎"
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { self.startTap() }
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
        add(m, L("enabled"), s.enabled) { self.s.enabled.toggle(); self.updateTitle() }
        add(m, L("auto"), s.auto, enabled: s.enabled) { self.s.auto.toggle() }
        add(m, L("sound"), s.sound, enabled: s.enabled) { self.s.sound.toggle(); if self.s.sound { Sounds.play(Sounds.auto) } }
        if engine.tap == nil { add(m, L("access"), enabled: true) {
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!) } }
        m.addItem(.separator())

        let id = engine.frontBundle
        add(m, L("app") + appName(id))
        if engine.frontIsGame { add(m, L("game")) }
        let ex = s.excluded.contains(id)
        add(m, L("exclude"), ex) {
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

        let exMenu = NSMenu()
        if s.excluded.isEmpty { add(exMenu, emptyHint("exclude")) }
        for e in s.excluded { add(exMenu, "✕ \(appName(e))") { self.s.excluded = self.s.excluded.filter { $0 != e } } }
        let ei = NSMenuItem(title: L("exclusions"), action: nil, keyEquivalent: ""); ei.submenu = exMenu; m.addItem(ei)

        let rMenu = NSMenu()
        if engine.recent.isEmpty { add(rMenu, L("empty")) }
        for r in engine.recent {
            let on = s.excluded.contains(r)
            add(rMenu, appName(r), on) { self.s.excluded = on ? self.s.excluded.filter { $0 != r } : self.s.excluded + [r] }
        }
        let ri = NSMenuItem(title: L("recent"), action: nil, keyEquivalent: ""); ri.submenu = rMenu; m.addItem(ri)

        let bMenu = NSMenu()
        if s.bindings.isEmpty { add(bMenu, emptyHint("appLang")) }
        for (b, l) in s.bindings.sorted(by: { $0.key < $1.key }) {
            add(bMenu, "✕ \(appName(b)) → \(displayCode(l))") { self.s.bindings[b] = nil }
        }
        let bi = NSMenuItem(title: L("bindings"), action: nil, keyEquivalent: ""); bi.submenu = bMenu; m.addItem(bi)
        m.addItem(.separator())

        let login = SMAppService.mainApp.status == .enabled
        add(m, L("login"), login) {
            if login { try? SMAppService.mainApp.unregister() } else { try? SMAppService.mainApp.register() }
        }
        add(m, L("words")) { self.editWords() }
        let uiMenu = NSMenu()
        add(uiMenu, L("system"), s.uiLang == nil) { self.s.uiLang = nil }
        for (c, name) in zip(uiLangs, uiLangNames) { add(uiMenu, name, s.uiLang == c) { self.s.uiLang = c } }
        let ui = NSMenuItem(title: L("uiLang"), action: nil, keyEquivalent: ""); ui.submenu = uiMenu; m.addItem(ui)
        add(m, L("hint"))
        add(m, updateDue() ? L("updateDue") : L("update")) {
            self.s.updateCheckedAt = Date().timeIntervalSince1970
            self.updateTitle()
            Updater.check()
        }
        add(m, L("report")) { self.reportProblem() }
        add(m, L("donate")) { NSWorkspace.shared.open(URL(string: "https://donatello.to/aura-switcher")!) }
        add(m, "AURA Switcher \(Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "") beta · 🇺🇦")
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
