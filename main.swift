import Cocoa
import Carbon
import ServiceManagement
import ApplicationServices

// MARK: - Layouts

let kMagic: Int64 = 0x4B535754 // marks our own synthetic events
func langName(_ code: String) -> String { Locale(identifier: uiCode()).localizedString(forLanguageCode: code)?.capitalized ?? code }

struct Key: Hashable { let code: UInt16; let shift: Bool }

final class Layout {
    let lang: String
    let source: TISInputSource
    var toChar: [Key: Character] = [:]
    var toKey: [Character: Key] = [:]

    init?(lang: String, source: TISInputSource) {
        self.lang = lang
        self.source = source
        guard let ptr = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData) else { return nil }
        let data = Unmanaged<CFData>.fromOpaque(ptr).takeUnretainedValue() as Data
        data.withUnsafeBytes { raw in
            guard let layout = raw.bindMemory(to: UCKeyboardLayout.self).baseAddress else { return }
            for code in UInt16(0)...50 where ![36, 48, 49].contains(code) {
                for shift in [false, true] {
                    var dead: UInt32 = 0, len = 0
                    var chars = [UniChar](repeating: 0, count: 4)
                    let mod: UInt32 = shift ? UInt32((shiftKey >> 8) & 0xFF) : 0
                    UCKeyTranslate(layout, code, UInt16(kUCKeyActionDown), mod, UInt32(LMGetKbdType()),
                                   OptionBits(kUCKeyTranslateNoDeadKeysBit), &dead, 4, &len, &chars)
                    guard len == 1, chars[0] >= 0x20, let s = Unicode.Scalar(chars[0]) else { continue }
                    let c = Character(s)
                    let k = Key(code: code, shift: shift)
                    toChar[k] = c
                    if toKey[c] == nil { toKey[c] = k }
                }
            }
        }
    }

    func text(_ keys: [Key]) -> String { String(keys.compactMap { toChar[$0] }) }
}

func loadLayouts() -> [Layout] {
    let filter = [kTISPropertyInputSourceCategory: kTISCategoryKeyboardInputSource!,
                  kTISPropertyInputSourceIsSelectCapable: true] as CFDictionary
    guard let list = TISCreateInputSourceList(filter, false)?.takeRetainedValue() as? [TISInputSource] else { return [] }
    // every enabled keyboard layout, one per language, in system order (IMEs without key data are skipped)
    var result: [Layout] = []
    for src in list {
        guard let l = sourceLang(src), !result.contains(where: { $0.lang == l }),
              let layout = Layout(lang: l, source: src) else { continue }
        result.append(layout)
    }
    return result
}

func sourceLang(_ src: TISInputSource) -> String? {
    guard let p = TISGetInputSourceProperty(src, kTISPropertyInputSourceLanguages) else { return nil }
    let langs = Unmanaged<CFArray>.fromOpaque(p).takeUnretainedValue() as? [String]
    return langs?.first.map { String($0.prefix(2)) }
}

func currentLang() -> String? { sourceLang(TISCopyCurrentKeyboardInputSource().takeRetainedValue()) }

// MARK: - Settings

final class Settings {
    let d = UserDefaults.standard
    var enabled: Bool { get { d.object(forKey: "enabled") as? Bool ?? true } set { d.set(newValue, forKey: "enabled") } }
    var auto: Bool { get { d.object(forKey: "auto") as? Bool ?? true } set { d.set(newValue, forKey: "auto") } }
    var excluded: [String] { get { d.stringArray(forKey: "excluded") ?? [] } set { d.set(newValue, forKey: "excluded") } }
    var bindings: [String: String] { get { d.dictionary(forKey: "bindings") as? [String: String] ?? [:] } set { d.set(newValue, forKey: "bindings") } }
    var uiLang: String? { get { d.string(forKey: "uiLang") } set { d.set(newValue, forKey: "uiLang") } } // nil = system
}

// MARK: - Localization (English, Ukrainian; Russian system → Ukrainian)

let uiStrings: [String: [String]] = [ // key: [en, uk]
    "enabled": ["Enabled", "Увімкнено"],
    "auto": ["Auto-correction", "Автовиправлення"],
    "access": ["⚠︎ Grant access: Accessibility", "⚠︎ Надайте доступ: Універсальний доступ"],
    "app": ["App: ", "Програма: "],
    "exclude": ["Don't work in this app", "Не працювати в цій програмі"],
    "noBinding": ["No binding", "Без прив'язки"],
    "appLang": ["Language for this app", "Мова для цієї програми"],
    "empty": ["Empty", "Порожньо"],
    "exclusions": ["Exclusions", "Винятки"],
    "bindings": ["Language bindings", "Прив'язки мов"],
    "login": ["Launch at login", "Запускати під час входу"],
    "hint": ["Double ⇧ — convert word/selection", "Подвійний ⇧ — змінити мову слова/виділення"],
    "uiLang": ["Menu language", "Мова меню"],
    "system": ["System", "Системна"],
    "quit": ["Quit", "Вихід"],
]

func uiCode() -> String {
    if let l = engine.settings.uiLang { return l }
    let sys = Locale.preferredLanguages.first?.prefix(2) ?? "en"
    return sys == "uk" || sys == "ru" ? "uk" : "en"
}

func L(_ key: String) -> String { uiStrings[key]?[uiCode() == "uk" ? 1 : 0] ?? key }

// MARK: - Engine

final class Engine {
    let settings = Settings()
    var layouts: [Layout] = loadLayouts()
    var frontBundle = NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? ""
    var tap: CFMachPort?
    let src = CGEventSource(stateID: .privateState)

    var word: [Key] = []        // word being typed
    var lastWord: [Key] = []    // previous word, still convertible
    var trailing = 0            // spaces typed after lastWord

    var shiftDownAt: TimeInterval = 0, lastShiftTap: TimeInterval = 0, sawOther = false
    var lastSelOut: String?, lastSelLang = 0

    var allCodes: Set<UInt16> { Set(layouts.flatMap { $0.toChar.keys.map(\.code) }) }
    lazy var codes = allCodes

    var active: Bool { settings.enabled && !settings.excluded.contains(frontBundle) }

    func reset() { word = []; lastWord = []; trailing = 0 }

    func layout(_ lang: String?) -> Layout? { layouts.first { $0.lang == lang } }
    func index(_ lang: String?) -> Int { layouts.firstIndex { $0.lang == lang } ?? 0 }
    func select(_ l: Layout) { TISSelectInputSource(l.source) }

    // MARK: tap

    func start() -> Bool {
        let mask: CGEventMask = (1 << CGEventType.keyDown.rawValue) | (1 << CGEventType.flagsChanged.rawValue)
            | (1 << CGEventType.leftMouseDown.rawValue) | (1 << CGEventType.rightMouseDown.rawValue)
        tap = CGEvent.tapCreate(tap: .cgSessionEventTap, place: .tailAppendEventTap, options: .listenOnly,
                                eventsOfInterest: mask, callback: { _, type, event, _ in
            engine.handle(type, event)
        }, userInfo: nil)
        guard let tap else { return false }
        CFRunLoopAddSource(CFRunLoopGetMain(), CFMachPortCreateRunLoopSource(nil, tap, 0), .commonModes)
        CGEvent.tapEnable(tap: tap, enable: true)
        return true
    }

    func handle(_ type: CGEventType, _ e: CGEvent) -> Unmanaged<CGEvent>? {
        let pass = Unmanaged.passUnretained(e)
        if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
            if let tap { CGEvent.tapEnable(tap: tap, enable: true) }
            return pass
        }
        if e.getIntegerValueField(.eventSourceUserData) == kMagic || !active { return pass }

        switch type {
        case .leftMouseDown, .rightMouseDown:
            reset(); lastShiftTap = 0
        case .flagsChanged:
            handleShift(e)
        case .keyDown:
            sawOther = true; lastShiftTap = 0
            handleKey(e)
        default: break
        }
        return pass
    }

    func handleShift(_ e: CGEvent) {
        let code = e.getIntegerValueField(.keyboardEventKeycode)
        let now = ProcessInfo.processInfo.systemUptime
        guard code == 56 || code == 60 else { lastShiftTap = 0; return }
        if e.flags.contains(.maskShift) {
            if !e.flags.intersection([.maskCommand, .maskControl, .maskAlternate]).isEmpty { lastShiftTap = 0; return }
            shiftDownAt = now; sawOther = false
        } else {
            guard !sawOther, now - shiftDownAt < 0.3 else { lastShiftTap = 0; return }
            if now - lastShiftTap < 0.45 {
                lastShiftTap = 0
                DispatchQueue.main.async { self.doubleShift() }
            } else { lastShiftTap = now }
        }
    }

    /// listen-only: never blocks input; heavy work runs after the tap returns
    func handleKey(_ e: CGEvent) {
        let code = UInt16(e.getIntegerValueField(.keyboardEventKeycode))
        let f = e.flags
        if !f.intersection([.maskCommand, .maskControl, .maskAlternate]).isEmpty { reset(); return }
        switch code {
        case 51: // backspace
            if trailing > 0 { trailing -= 1; if trailing == 0 { word = lastWord; lastWord = [] } }
            else if !word.isEmpty { word.removeLast() }
        case 49: // space (already delivered to the app)
            if !word.isEmpty {
                lastWord = word; word = []; trailing = 1
                if settings.auto, settings.bindings[frontBundle] == nil {
                    let keys = lastWord
                    DispatchQueue.main.async { self.autoCorrect(keys) }
                }
            } else if !lastWord.isEmpty { trailing += 1 }
        default:
            if codes.contains(code) {
                if trailing > 0 || !lastWord.isEmpty { lastWord = []; trailing = 0 }
                word.append(Key(code: code, shift: f.contains(.maskShift) || f.contains(.maskAlphaShift)))
            } else { reset() }
        }
    }

    func autoCorrect(_ keys: [Key]) {
        // user kept typing meanwhile — skip
        guard lastWord == keys, word.isEmpty, trailing == 1, let fix = autoFix(keys) else { return }
        let out = fix.layout.text(keys)
        // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
        if out != layout(currentLang())?.text(keys) { replace(count: keys.count + 1, with: out + " ") }
        select(fix.layout)
    }

    // MARK: auto detection

    func isWord(_ s: String, _ lang: String) -> Bool {
        let w = s.trimmingCharacters(in: .punctuationCharacters.union(.symbols))
        guard w.count >= 2, w.allSatisfy(\.isLetter) || w.contains("'") || w.contains("-") else { return false }
        let checker = NSSpellChecker.shared
        guard let l = checker.availableLanguages.first(where: { $0.hasPrefix(lang) }) else { return false }
        return checker.checkSpelling(of: w, startingAt: 0, language: l, wrap: false,
                                     inSpellDocumentWithTag: 0, wordCount: nil).location == NSNotFound
    }

    func isLatin(_ s: String) -> Bool { s.unicodeScalars.allSatisfy { $0.value < 0x250 } }

    func autoFix(_ keys: [Key]) -> (layout: Layout, Void)? {
        guard keys.count >= 2, let cur = layout(currentLang()) else { return nil }
        let typed = cur.text(keys)
        if isWord(typed, cur.lang) { return nil }
        let start = index(cur.lang)
        for i in 1..<max(layouts.count, 1) {
            let l = layouts[(start + i) % layouts.count]
            let t = l.text(keys)
            // near-identical Latin layouts (QWERTY/QWERTZ/AZERTY): a typo there isn't a layout mistake.
            // Cyrillic UK/RU differ in few keys but that's exactly what users mix up, so they're allowed.
            let same = zip(t, typed).filter { $0 == $1 }.count
            if isLatin(t), isLatin(typed), same * 2 > keys.count { continue }
            if t.count == keys.count, isWord(t, l.lang) { return (l, ()) }
        }
        return nil
    }

    // MARK: double shift

    func doubleShift() {
        if let sel = axSelectedText(), !sel.isEmpty { convertSelection(sel); return }
        let keys = word.isEmpty ? lastWord : word
        if !keys.isEmpty { cycleWord(keys, trailing: word.isEmpty ? trailing : 0); return }
        clipboardSelection { sel in
            if let sel, !sel.isEmpty { self.convertSelection(sel) } else { self.cycleLayout() }
        }
    }

    func next(after lang: String?) -> Layout? {
        layouts.isEmpty ? nil : layouts[(index(lang) + 1) % layouts.count]
    }

    func cycleLayout() { if let n = next(after: currentLang()) { select(n) } }

    func cycleWord(_ keys: [Key], trailing: Int) {
        guard let n = next(after: currentLang()) else { return }
        replace(count: keys.count + trailing, with: n.text(keys) + String(repeating: " ", count: trailing))
        select(n)
    }

    func convertSelection(_ text: String) {
        guard !layouts.isEmpty else { return }
        var srcIdx: Int
        if text == lastSelOut { srcIdx = lastSelLang } else {
            let cur = index(currentLang())
            let scores = layouts.map { l in text.filter { l.toKey[$0] != nil && $0.isLetter }.count }
            srcIdx = scores.indices.max { a, b in
                scores[a] != scores[b] ? scores[a] < scores[b] : (a != cur && b == cur)
            } ?? cur
        }
        let srcL = layouts[srcIdx], dstIdx = (srcIdx + 1) % layouts.count, dst = layouts[dstIdx]
        let out = String(text.map { c in srcL.toKey[c].flatMap { dst.toChar[$0] } ?? c })
        typeText(out)
        for _ in 0..<out.utf16.count { postKey(123, flags: .maskShift) } // reselect
        lastSelOut = out; lastSelLang = dstIdx
        select(dst)
    }

    // MARK: selection

    func axSelectedText() -> String? {
        let sys = AXUIElementCreateSystemWide()
        var focused: CFTypeRef?
        guard AXUIElementCopyAttributeValue(sys, kAXFocusedUIElementAttribute as CFString, &focused) == .success,
              let el = focused else { return nil }
        var sel: CFTypeRef?
        AXUIElementCopyAttributeValue(el as! AXUIElement, kAXSelectedTextAttribute as CFString, &sel)
        return sel as? String
    }

    func clipboardSelection(_ done: @escaping (String?) -> Void) {
        let pb = NSPasteboard.general
        let saved = pb.string(forType: .string), count = pb.changeCount
        postKey(8, flags: .maskCommand) // ⌘C
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.12) {
            let changed = pb.changeCount != count
            let s = changed ? pb.string(forType: .string) : nil
            if changed { pb.clearContents(); if let saved { pb.setString(saved, forType: .string) } }
            done(s)
        }
    }

    // MARK: synthetic input

    func postKey(_ code: CGKeyCode, flags: CGEventFlags = []) {
        for down in [true, false] {
            guard let e = CGEvent(keyboardEventSource: src, virtualKey: code, keyDown: down) else { continue }
            e.flags = flags
            e.setIntegerValueField(.eventSourceUserData, value: kMagic)
            e.post(tap: .cghidEventTap)
        }
    }

    func typeText(_ s: String) {
        let u = Array(s.utf16)
        for i in stride(from: 0, to: u.count, by: 16) {
            var chunk = Array(u[i..<min(i + 16, u.count)])
            for down in [true, false] {
                guard let e = CGEvent(keyboardEventSource: src, virtualKey: 0, keyDown: down) else { continue }
                e.flags = []
                e.keyboardSetUnicodeString(stringLength: chunk.count, unicodeString: &chunk)
                e.setIntegerValueField(.eventSourceUserData, value: kMagic)
                e.post(tap: .cghidEventTap)
            }
        }
    }

    func replace(count: Int, with s: String) {
        for _ in 0..<count { postKey(51) }
        typeText(s)
    }
}

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
                                                            object: nil, queue: .main) { _ in self.updateTitle() }
        DistributedNotificationCenter.default().addObserver(forName: NSNotification.Name(kTISNotifyEnabledKeyboardInputSourcesChanged as String),
                                                            object: nil, queue: .main) { _ in engine.layouts = loadLayouts(); engine.codes = engine.allCodes }
        NSWorkspace.shared.notificationCenter.addObserver(forName: NSWorkspace.didActivateApplicationNotification,
                                                          object: nil, queue: .main) { note in
            guard let app = note.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication,
                  let id = app.bundleIdentifier, id != Bundle.main.bundleIdentifier else { return }
            engine.frontBundle = id
            engine.reset()
            if engine.settings.enabled, let lang = engine.settings.bindings[id], let l = engine.layout(lang) { engine.select(l) }
        }

        let opts = [kAXTrustedCheckOptionPrompt.takeUnretainedValue(): true] as CFDictionary
        AXIsProcessTrustedWithOptions(opts)
        startTap()
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
        let l = (currentLang() ?? "--").uppercased()
        item.button?.title = s.enabled ? l : l.lowercased()
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

    func menuNeedsUpdate(_ m: NSMenu) {
        m.removeAllItems()
        add(m, L("enabled"), s.enabled) { self.s.enabled.toggle(); self.updateTitle() }
        add(m, L("auto"), s.auto, enabled: s.enabled) { self.s.auto.toggle() }
        if engine.tap == nil { add(m, L("access"), enabled: true) {
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility")!) } }
        m.addItem(.separator())

        let id = engine.frontBundle
        add(m, L("app") + appName(id))
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
        if s.excluded.isEmpty { add(exMenu, L("empty")) }
        for e in s.excluded { add(exMenu, "✕ \(appName(e))") { self.s.excluded = self.s.excluded.filter { $0 != e } } }
        let ei = NSMenuItem(title: L("exclusions"), action: nil, keyEquivalent: ""); ei.submenu = exMenu; m.addItem(ei)

        let bMenu = NSMenu()
        if s.bindings.isEmpty { add(bMenu, L("empty")) }
        for (b, l) in s.bindings.sorted(by: { $0.key < $1.key }) {
            add(bMenu, "✕ \(appName(b)) → \(l.uppercased())") { self.s.bindings[b] = nil }
        }
        let bi = NSMenuItem(title: L("bindings"), action: nil, keyEquivalent: ""); bi.submenu = bMenu; m.addItem(bi)
        m.addItem(.separator())

        let login = SMAppService.mainApp.status == .enabled
        add(m, L("login"), login) {
            if login { try? SMAppService.mainApp.unregister() } else { try? SMAppService.mainApp.register() }
        }
        let uiMenu = NSMenu()
        add(uiMenu, L("system"), s.uiLang == nil) { self.s.uiLang = nil }
        add(uiMenu, "English", s.uiLang == "en") { self.s.uiLang = "en" }
        add(uiMenu, "Українська", s.uiLang == "uk") { self.s.uiLang = "uk" }
        let ui = NSMenuItem(title: L("uiLang"), action: nil, keyEquivalent: ""); ui.submenu = uiMenu; m.addItem(ui)
        add(m, L("hint"))
        add(m, "AURA \(Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "") beta")
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

// single instance: a second copy would double every correction
let me = NSRunningApplication.current
if NSRunningApplication.runningApplications(withBundleIdentifier: Bundle.main.bundleIdentifier ?? "")
    .contains(where: { $0 != me }) { exit(0) }

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
