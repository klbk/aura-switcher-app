import Cocoa
import Carbon
import ServiceManagement
import ApplicationServices
import os

// MARK: - Layouts

let kMagic: Int64 = 0x4B535754 // marks our own synthetic events
/// why a word was left alone — no typed text, only the reason (`log show --predicate 'subsystem == "com.chernov.aura"'`)
let auraLog = Logger(subsystem: "com.chernov.aura", category: "auto")
func langName(_ code: String) -> String { Locale(identifier: uiCode()).localizedString(forLanguageCode: code)?.capitalized ?? code }

/// A key press. `ch` is the character it actually produced (from the event), which tells which layout
/// was really active — more reliable than asking the system from a background app. `caps` (Caps Lock)
/// uppercases letters only, not digits or symbols. Neither is part of equality.
struct Key: Hashable {
    let code: UInt16; let shift: Bool; var caps = false; var ch: Character? = nil
    static func == (a: Key, b: Key) -> Bool { a.code == b.code && a.shift == b.shift }
    func hash(into h: inout Hasher) { h.combine(code); h.combine(shift) }
}

final class Layout {
    let lang: String
    let source: TISInputSource
    var toChar: [Key: Character] = [:]
    var toKey: [Character: Key] = [:]
    var dead: [Key: String] = [:]   // dead key (accent typed before the letter) → its combining marks

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
                                   OptionBits(1 << kUCKeyTranslateNoDeadKeysBit), &dead, 4, &len, &chars)
                    guard len == 1, chars[0] >= 0x20, let s = Unicode.Scalar(chars[0]) else { continue }
                    let c = Character(s)
                    let k = Key(code: code, shift: shift)
                    toChar[k] = c
                    if toKey[c] == nil { toKey[c] = k }
                    var state: UInt32 = 0
                    UCKeyTranslate(layout, code, UInt16(kUCKeyActionDown), mod, UInt32(LMGetKbdType()), 0, &state, 4, &len, &chars)
                    if len == 0, state != 0 {  // the spacing accent (´ ΄ ¨) decomposes to space + combining marks
                        let marks = String(String(c).decomposedStringWithCompatibilityMapping.unicodeScalars
                            .filter { $0.properties.generalCategory == .nonspacingMark }.map(Character.init))
                        if !marks.isEmpty { self.dead[k] = marks }
                    }
                }
            }
        }
    }

    /// What the keys type in this layout; a dead key and the letter after it make one accented letter (´ e → é).
    func text(_ keys: [Key]) -> String {
        var s = "", i = 0
        while i < keys.count {
            let k = keys[i]; i += 1
            guard var c = toChar[k].map(String.init) else { continue }
            if let n = keys[i...].first, let l = toChar[n], l.isLetter {
                if let m = dead[k] { c = (String(l) + m).precomposedStringWithCanonicalMapping; i += 1; if n.caps { c = c.uppercased() } }
            }
            if k.caps, c.count == 1, c.first!.isLetter { c = c.uppercased() }
            s += c
        }
        return s
    }

    func maps(_ keys: [Key]) -> Bool { keys.allSatisfy { toChar[$0] != nil } }

    /// Keys that type a character here: one key, or a dead key + letter for an accented one.
    func keys(for c: Character) -> [Key]? {
        if let k = toKey[c] { return [k] }
        let sc = String(c).decomposedStringWithCanonicalMapping.unicodeScalars
        guard let base = sc.first, let bk = toKey[Character(base)] else { return nil }
        let marks = String(sc.dropFirst().map(Character.init))
        guard !marks.isEmpty, let dk = dead.first(where: { $0.value == marks })?.key else { return nil }
        return [dk, bk]
    }
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

/// Language code written in the language's own script: en → EN, uk → УК, ru → РУ, el → ΕΛ
let nativeCodes = ["uk": "УК", "ru": "РУ", "be": "БЕ", "bg": "БГ", "sr": "СР", "mk": "МК", "kk": "ҚЗ", "ky": "КЫ",
                   "tg": "ТҶ", "mn": "МН", "el": "ΕΛ", "ka": "ქა", "hy": "ՀՅ", "he": "עב", "ar": "عر", "fa": "فا"]
func displayCode(_ lang: String?) -> String {
    guard let lang else { return "--" }
    return nativeCodes[lang] ?? lang.uppercased()
}

/// tests set the "active" layout here instead of switching the system's
var forcedLang: String? = nil
func currentLang() -> String? { forcedLang ?? sourceLang(TISCopyCurrentKeyboardInputSource().takeRetainedValue()) }

// MARK: - Language models

/// For languages whose spelling dictionary may be missing (lang/<code>.txt, built by tools/langmodel.py):
/// the most frequent words, and letter-trigram costs (−10·ln p) that tell a word of the language
/// typed in the wrong layout from an English typo.
/// tests read the models from the repo's lang/ folder instead of the app bundle
var langDir: URL? = nil

final class LangModel {
    let words: Set<String>, tri: [String: Int], floor: Int
    init?(_ lang: String) {
        guard let url = langDir.map({ $0.appendingPathComponent(lang + ".txt") })
                ?? Bundle.main.url(forResource: lang, withExtension: "txt", subdirectory: "lang"),
              let s = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        let lines = s.split(separator: "\n").filter { !$0.hasPrefix("#") }
        guard let first = lines.first else { return nil }
        words = Set(first.split(separator: " ").map(String.init))
        var t: [String: Int] = [:]
        for l in lines.dropFirst() { let p = l.split(separator: "\t"); if p.count == 2, let v = Int(p[1]) { t[String(p[0])] = v } }
        tri = t; floor = (t.values.max() ?? 100) + 10
    }
    func has(_ w: String) -> Bool { words.contains(w.lowercased()) }
    /// average trigram cost: lower = more typical of the language
    func cost(_ w: String) -> Double {
        let c = Array("^" + w.lowercased() + "$")
        guard c.count >= 3 else { return Double(floor) }
        return Double((0..<c.count - 2).reduce(0) { $0 + (tri[String(c[$1..<$1 + 3])] ?? floor) }) / Double(c.count - 2)
    }
}

// MARK: - Settings

final class Settings {
    let d = UserDefaults.standard
    var enabled: Bool { get { d.object(forKey: "enabled") as? Bool ?? true } set { d.set(newValue, forKey: "enabled") } }
    var auto: Bool { get { d.object(forKey: "auto") as? Bool ?? true } set { d.set(newValue, forKey: "auto") } }
    var excluded: [String] { get { d.stringArray(forKey: "excluded") ?? [] } set { d.set(newValue, forKey: "excluded") } }
    var bindings: [String: String] { get { d.dictionary(forKey: "bindings") as? [String: String] ?? [:] } set { d.set(newValue, forKey: "bindings") } }
    var uiLang: String? { get { d.string(forKey: "uiLang") } set { d.set(newValue, forKey: "uiLang") } } // nil = system
    var sound: Bool { get { d.object(forKey: "sound") as? Bool ?? true } set { d.set(newValue, forKey: "sound") } }
    /// user's words that are never auto-corrected (slang like «хз», «ща»), stored lowercased
    var words: [String] { get { d.stringArray(forKey: "words") ?? [] } set { d.set(newValue, forKey: "words") } }
    /// when the user last opened the releases page (seconds since 1970), 0 = never
    var updateCheckedAt: Double { get { d.double(forKey: "updateCheckedAt") } set { d.set(newValue, forKey: "updateCheckedAt") } }
}

/// Updates: the app goes online only when the user clicks «Check for updates…» (updater.swift: update.json on
/// klbk.github.io, the download from github.com); nothing typed and no personal data is ever sent. «What's new»
/// opens the site's update page (only the version and menu language are in its address), and some time after
/// this build (or after the last check) the menu and the icon remind that a newer version may be out. The build date is
/// written into Info.plist by build.sh. The site's update page: says whether a newer version is out, what's
/// new and how to update, in the menu language.
func updatePageURL() -> URL {
    let ver = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? ""
    return URL(string: "https://klbk.github.io/aura-switcher/update/?v=\(ver)&l=\(uiCode())")!
}
let updateNudgeDays = 30.0
let buildDate = (Bundle.main.object(forInfoDictionaryKey: "AURABuildDate") as? String).flatMap(Double.init) ?? 0
func updateDue() -> Bool {
    let since = max(buildDate, engine.settings.updateCheckedAt)
    return since > 0 && Date().timeIntervalSince1970 - since > updateNudgeDays * 86400
}

/// Games (App Store category "Games" and its subcategories): double ⇧ is often sprint there, and a
/// full-screen game hides the menu bar, so the app can't be excluded by hand — pause AURA automatically.
func isGame(_ app: NSRunningApplication?) -> Bool {
    guard let url = app?.bundleURL, let cat = Bundle(url: url)?.object(forInfoDictionaryKey: "LSApplicationCategoryType") as? String
    else { return false }
    return cat.hasSuffix("games")
}

/// VS Code, JetBrains IDEs and others copy the whole current line when nothing is selected —
/// a single line ending in a line break is that, not a selection to convert.
func isCopiedLine(_ s: String) -> Bool {
    guard s.last?.isNewline == true else { return false }  // "\r\n" is one Character in Swift
    return !s.dropLast().contains(where: \.isNewline)
}

/// "хз, ща яж" → ["хз", "ща", "яж"]
func parseWords(_ s: String) -> [String] {
    var seen = Set<String>()
    return s.lowercased().components(separatedBy: CharacterSet(charactersIn: ",;\n ").union(.whitespaces))
        .filter { !$0.isEmpty && seen.insert($0).inserted }
}

// MARK: - Sounds (synthesized, identical on Mac and Windows)

enum Sounds {
    /// A short quiet "tick" (fast attack, quick decay) for auto-correction, two rising ticks for double ⇧ —
    /// soft enough to stop noticing after a while.
    static func blip(_ freqs: [Double]) -> Data {
        let rate = 44100, toneLen = rate * 50 / 1000, gap = rate * 30 / 1000
        var pcm: [Int16] = []
        for (i, f) in freqs.enumerated() {
            if i > 0 { pcm += [Int16](repeating: 0, count: gap) }
            for n in 0..<toneLen {
                let t = Double(n) / Double(rate)
                let env = min(1, Double(n) / 40) * exp(-Double(n) / (Double(rate) * 0.012))  // no click, ~12 ms decay
                pcm.append(Int16(sin(2 * .pi * f * t) * env * 0.12 * 32767))
            }
        }
        var d = Data()
        func u32(_ v: Int) { var x = UInt32(v).littleEndian; d.append(Data(bytes: &x, count: 4)) }
        func u16(_ v: Int) { var x = UInt16(v).littleEndian; d.append(Data(bytes: &x, count: 2)) }
        d.append("RIFF".data(using: .ascii)!); u32(36 + pcm.count * 2); d.append("WAVEfmt ".data(using: .ascii)!)
        u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16)
        d.append("data".data(using: .ascii)!); u32(pcm.count * 2)
        pcm.withUnsafeBufferPointer { d.append(Data(buffer: $0)) }
        return d
    }
    static let auto = NSSound(data: blip([1568]))
    static let shift = NSSound(data: blip([1318, 1760]))
    static func play(_ s: NSSound?) {
        guard engine.settings.sound, let s else { return }
        s.stop(); s.play()
    }
}

// MARK: - Localization (21 languages; Russian system → Ukrainian)

// BEGIN menu strings — generated by tools/menu_i18n.py, edit there
let uiLangs = ["en", "uk", "pl", "de", "es", "fr", "it", "pt", "tr", "nl", "cs", "sv", "fi", "ro", "hu", "el", "bg", "he", "ar", "hi", "ko"]
let uiLangNames = ["English", "Українська", "Polski", "Deutsch", "Español", "Français", "Italiano", "Português", "Türkçe", "Nederlands", "Čeština", "Svenska", "Suomi", "Română", "Magyar", "Ελληνικά", "Български", "עברית", "العربية", "हिन्दी", "한국어"]
let uiStrings: [String: [String]] = [ // key: one string per uiLangs
    "enabled": ["Enabled", "Увімкнено", "Włączone", "Aktiviert", "Activado", "Activé", "Attivo", "Ativado", "Etkin", "Ingeschakeld", "Zapnuto", "Aktiverad", "Käytössä", "Activat", "Bekapcsolva", "Ενεργό", "Включено", "מופעל", "مفعّل", "चालू", "켜기"],
    "auto": ["Auto-correction", "Автовиправлення", "Autokorekta", "Autokorrektur", "Autocorrección", "Correction automatique", "Correzione automatica", "Correção automática", "Otomatik düzeltme", "Autocorrectie", "Automatické opravy", "Autokorrigering", "Automaattinen korjaus", "Corectare automată", "Automatikus javítás", "Αυτόματη διόρθωση", "Автокорекция", "תיקון אוטומטי", "التصحيح التلقائي", "स्वतः सुधार", "자동 수정"],
    "sound": ["Sound", "Звук", "Dźwięk", "Ton", "Sonido", "Son", "Suono", "Som", "Ses", "Geluid", "Zvuk", "Ljud", "Ääni", "Sunet", "Hang", "Ήχος", "Звук", "צליל", "الصوت", "ध्वनि", "소리"],
    "access": ["⚠︎ Grant access: Accessibility", "⚠︎ Надайте доступ: Універсальний доступ", "⚠︎ Nadaj dostęp: Dostępność", "⚠︎ Zugriff erlauben: Bedienungshilfen", "⚠︎ Conceder acceso: Accesibilidad", "⚠︎ Autoriser l'accès : Accessibilité", "⚠︎ Concedi l’accesso: Accessibilità", "⚠︎ Conceder acesso: Acessibilidade", "⚠︎ Erişim izni verin: Erişilebilirlik", "⚠︎ Geef toegang: Toegankelijkheid", "⚠︎ Udělte přístup: Zpřístupnění", "⚠︎ Ge åtkomst: Hjälpmedel", "⚠︎ Anna käyttöoikeus: Käyttöapu", "⚠︎ Acordați acces: Accesibilitate", "⚠︎ Hozzáférés megadása: Kisegítő lehetőségek", "⚠︎ Δώστε πρόσβαση: Προσβασιμότητα", "⚠︎ Дайте достъп: Достъпност", "⚠︎ תנו גישה: נגישות", "⚠︎ امنح الوصول: تسهيلات الاستخدام", "⚠︎ अनुमति दें: सुलभता", "⚠︎ 접근 권한 허용: 손쉬운 사용"],
    "app": ["App: ", "Програма: ", "Aplikacja: ", "App: ", "App: ", "App : ", "App: ", "App: ", "Uygulama: ", "App: ", "Aplikace: ", "App: ", "Ohjelma: ", "Aplicație: ", "Alkalmazás: ", "Εφαρμογή: ", "Програма: ", "יישום: ", "التطبيق: ", "ऐप: ", "앱: "],
    "exclude": ["Don't work in this app", "Не працювати в цій програмі", "Nie działaj w tej aplikacji", "In dieser App nicht aktiv", "No funcionar en esta app", "Ne pas fonctionner dans cette app", "Disattiva in questa app", "Não funcionar nesta app", "Bu uygulamada çalışma", "Niet werken in deze app", "Nefungovat v této aplikaci", "Arbeta inte i den här appen", "Älä toimi tässä ohjelmassa", "Nu funcționa în această aplicație", "Ne működjön ebben az alkalmazásban", "Να μη λειτουργεί σε αυτή την εφαρμογή", "Да не работи в тази програма", "לא לפעול ביישום זה", "لا تعمل في هذا التطبيق", "इस ऐप में काम न करें", "이 앱에서 작동 안 함"],
    "game": ["Game — AURA paused", "Гра — AURA на паузі", "Gra — AURA wstrzymana", "Spiel — AURA pausiert", "Juego — AURA en pausa", "Jeu — AURA en pause", "Gioco — AURA in pausa", "Jogo — AURA em pausa", "Oyun — AURA duraklatıldı", "Game — AURA gepauzeerd", "Hra — AURA pozastavena", "Spel — AURA pausad", "Peli — AURA tauolla", "Joc — AURA în pauză", "Játék — AURA szüneteltetve", "Παιχνίδι — AURA σε παύση", "Игра — AURA е на пауза", "משחק — AURA מושהית", "لعبة — AURA متوقف مؤقتًا", "गेम — AURA रुका हुआ", "게임 — AURA 일시 중지됨"],
    "appLang": ["Language for this app", "Мова для цієї програми", "Język dla tej aplikacji", "Sprache für diese App", "Idioma para esta app", "Langue pour cette app", "Lingua per questa app", "Idioma para esta app", "Bu uygulama için dil", "Taal voor deze app", "Jazyk pro tuto aplikaci", "Språk för den här appen", "Kieli tälle ohjelmalle", "Limba pentru această aplicație", "Nyelv ehhez az alkalmazáshoz", "Γλώσσα για αυτή την εφαρμογή", "Език за тази програма", "שפה ליישום זה", "لغة هذا التطبيق", "इस ऐप की भाषा", "이 앱의 언어"],
    "noBinding": ["No binding", "Без прив'язки", "Bez przypisania", "Keine Zuordnung", "Sin asignación", "Aucune association", "Nessuna associazione", "Sem associação", "Atama yok", "Geen koppeling", "Bez přiřazení", "Ingen koppling", "Ei liitosta", "Fără asociere", "Nincs hozzárendelés", "Χωρίς αντιστοίχιση", "Без обвързване", "ללא שיוך", "بدون ربط", "कोई बंधन नहीं", "지정 안 함"],
    "exclusions": ["Exclusions", "Винятки", "Wyjątki", "Ausnahmen", "Excepciones", "Exceptions", "Esclusioni", "Exceções", "İstisnalar", "Uitzonderingen", "Výjimky", "Undantag", "Poikkeukset", "Excepții", "Kivételek", "Εξαιρέσεις", "Изключения", "חריגים", "الاستثناءات", "अपवाद", "예외"],
    "recent": ["Don't work in (recent apps)", "Не працювати в (останні програми)", "Nie działaj w (ostatnie aplikacje)", "Nicht aktiv in (letzte Apps)", "No funcionar en (apps recientes)", "Ne pas fonctionner dans (apps récentes)", "Disattiva in (app recenti)", "Não funcionar em (apps recentes)", "Şurada çalışma (son uygulamalar)", "Niet werken in (recente apps)", "Nefungovat v (nedávné aplikace)", "Arbeta inte i (senaste appar)", "Älä toimi ohjelmassa (viimeisimmät)", "Nu funcționa în (aplicații recente)", "Ne működjön itt (legutóbbi alkalmazások)", "Να μη λειτουργεί σε (πρόσφατες εφαρμογές)", "Да не работи в (последни програми)", "לא לפעול ב (יישומים אחרונים)", "لا تعمل في (التطبيقات الأخيرة)", "इनमें काम न करें (हाल के ऐप)", "작동 안 함 (최근 앱)"],
    "bindings": ["Language bindings", "Прив'язки мов", "Przypisania języków", "Sprachzuordnungen", "Idiomas asignados", "Langues associées", "Lingue associate", "Idiomas associados", "Dil atamaları", "Taalkoppelingen", "Přiřazení jazyků", "Språkkopplingar", "Kieliliitokset", "Limbi asociate", "Nyelvi hozzárendelések", "Αντιστοιχίσεις γλωσσών", "Обвързани езици", "שיוכי שפה", "ربط اللغات", "भाषा बंधन", "언어 지정"],
    "empty": ["Empty", "Порожньо", "Pusto", "Leer", "Vacío", "Vide", "Vuoto", "Vazio", "Boş", "Leeg", "Prázdné", "Tomt", "Tyhjä", "Gol", "Üres", "Κενό", "Празно", "ריק", "فارغ", "खाली", "비어 있음"],
    "login": ["Launch at login", "Запускати під час входу", "Uruchamiaj przy logowaniu", "Bei Anmeldung starten", "Abrir al iniciar sesión", "Ouvrir à la connexion", "Apri al login", "Abrir ao iniciar sessão", "Oturum açılışında başlat", "Starten bij inloggen", "Spouštět po přihlášení", "Starta vid inloggning", "Käynnistä kirjautuessa", "Pornește la autentificare", "Indítás bejelentkezéskor", "Εκκίνηση κατά τη σύνδεση", "Стартиране при вход", "הפעלה בעת התחברות", "التشغيل عند تسجيل الدخول", "लॉगिन पर शुरू करें", "로그인 시 실행"],
    "words": ["My words…", "Мої слова…", "Moje słowa…", "Meine Wörter…", "Mis palabras…", "Mes mots…", "Le mie parole…", "As minhas palavras…", "Kelimelerim…", "Mijn woorden…", "Moje slova…", "Mina ord…", "Omat sanat…", "Cuvintele mele…", "Saját szavak…", "Οι λέξεις μου…", "Моите думи…", "המילים שלי…", "كلماتي…", "मेरे शब्द…", "내 단어…"],
    "wordsTitle": ["AURA Switcher — my words", "AURA Switcher — мої слова", "AURA Switcher — moje słowa", "AURA Switcher — meine Wörter", "AURA Switcher — mis palabras", "AURA Switcher — mes mots", "AURA Switcher — le mie parole", "AURA Switcher — as minhas palavras", "AURA Switcher — kelimelerim", "AURA Switcher — mijn woorden", "AURA Switcher — moje slova", "AURA Switcher — mina ord", "AURA Switcher — omat sanat", "AURA Switcher — cuvintele mele", "AURA Switcher — saját szavak", "AURA Switcher — οι λέξεις μου", "AURA Switcher — моите думи", "AURA Switcher — המילים שלי", "AURA Switcher — كلماتي", "AURA Switcher — मेरे शब्द", "AURA Switcher — 내 단어"],
    "wordsInfo": ["AURA Switcher treats these words as correct: it never changes them, and fixes them when typed in the wrong layout. Separate them with commas.", "Ці слова AURA Switcher вважає правильними: не змінює їх, а набрані не в тій розкладці — виправляє на них. Через кому.", "AURA Switcher uznaje te słowa za poprawne: nigdy ich nie zmienia, a wpisane w złym układzie poprawia na nie. Oddziel je przecinkami.", "AURA Switcher hält diese Wörter für richtig: ändert sie nie und korrigiert sie, wenn sie im falschen Layout getippt wurden. Durch Kommas trennen.", "AURA Switcher considera correctas estas palabras: nunca las cambia y las corrige si se escriben con la distribución equivocada. Sepáralas con comas.", "AURA Switcher considère ces mots comme corrects : il ne les modifie jamais et les corrige s'ils sont tapés avec la mauvaise disposition. Séparez-les par des virgules.", "AURA Switcher considera corrette queste parole: non le modifica mai e le corregge se le digiti con il layout sbagliato. Separale con una virgola.", "O AURA Switcher considera estas palavras corretas: nunca as altera e corrige-as se forem digitadas no layout errado. Separe-as com vírgulas.", "AURA Switcher bu kelimeleri doğru kabul eder: asla değiştirmez, yanlış düzende yazılırsa onlara düzeltir. Virgülle ayırın.", "AURA Switcher beschouwt deze woorden als juist: verandert ze nooit en verbetert ze als ze in de verkeerde indeling zijn getypt. Scheid ze met komma's.", "AURA Switcher považuje tato slova za správná: nikdy je nemění a opraví je, když jsou napsána ve špatném rozložení. Oddělte je čárkami.", "AURA Switcher ser dessa ord som rätt: ändrar dem aldrig och rättar dem om de skrivs med fel layout. Separera med kommatecken.", "AURA Switcher pitää näitä sanoja oikeina: ei koskaan muuta niitä ja korjaa ne, jos ne on kirjoitettu väärällä asettelulla. Erota pilkuilla.", "AURA Switcher consideră aceste cuvinte corecte: nu le schimbă niciodată și le corectează dacă sunt tastate în aranjamentul greșit. Separați-le prin virgulă.", "Az AURA Switcher ezeket a szavakat helyesnek tekinti: soha nem módosítja őket, és kijavítja, ha rossz kiosztással gépelték. Vesszővel válassza el.", "Το AURA Switcher θεωρεί σωστές αυτές τις λέξεις: δεν τις αλλάζει ποτέ και τις διορθώνει όταν πληκτρολογηθούν με λάθος διάταξη. Χωρίστε τις με κόμμα.", "AURA Switcher смята тези думи за правилни: никога не ги променя и ги поправя, ако са написани с грешната подредба. Разделете ги със запетая.", "AURA Switcher מתייחס למילים אלה כנכונות: לעולם לא משנה אותן, ומתקן אליהן כשהן מוקלדות בפריסה הלא נכונה. הפרידו בפסיקים.", "يعدّ AURA Switcher هذه الكلمات صحيحة: لا يغيّرها أبدًا، ويصحّحها إليها إذا كُتبت بالتخطيط الخطأ. افصل بينها بفواصل.", "AURA Switcher इन शब्दों को सही मानता है: इन्हें कभी नहीं बदलता, और गलत लेआउट में टाइप होने पर इन्हीं में सुधार देता है। अल्पविराम से अलग करें।", "AURA Switcher는 이 단어들을 올바른 단어로 봅니다. 절대 바꾸지 않고, 잘못된 자판으로 입력되면 이 단어로 고칩니다. 쉼표로 구분하세요."],
    "save": ["Save", "Зберегти", "Zapisz", "Sichern", "Guardar", "Enregistrer", "Salva", "Guardar", "Kaydet", "Bewaar", "Uložit", "Spara", "Tallenna", "Salvează", "Mentés", "Αποθήκευση", "Запази", "שמירה", "حفظ", "सहेजें", "저장"],
    "cancel": ["Cancel", "Скасувати", "Anuluj", "Abbrechen", "Cancelar", "Annuler", "Annulla", "Cancelar", "İptal", "Annuleer", "Zrušit", "Avbryt", "Kumoa", "Anulează", "Mégse", "Ακύρωση", "Отказ", "ביטול", "إلغاء", "रद्द करें", "취소"],
    "uiLang": ["Menu language", "Мова меню", "Język menu", "Menüsprache", "Idioma del menú", "Langue du menu", "Lingua del menu", "Idioma do menu", "Menü dili", "Menutaal", "Jazyk nabídky", "Menyspråk", "Valikon kieli", "Limba meniului", "Menü nyelve", "Γλώσσα μενού", "Език на менюто", "שפת התפריט", "لغة القائمة", "मेनू की भाषा", "메뉴 언어"],
    "system": ["System", "Системна", "Systemowy", "System", "Sistema", "Système", "Sistema", "Sistema", "Sistem", "Systeem", "Systémový", "System", "Järjestelmä", "Sistem", "Rendszer", "Συστήματος", "Системен", "מערכת", "النظام", "सिस्टम", "시스템"],
    "hint": ["Double ⇧ — convert word/selection", "Подвійний ⇧ — змінити мову слова/виділення", "Podwójny ⇧ — zmień układ słowa/zaznaczenia", "Doppel-⇧ — Wort/Auswahl umwandeln", "Doble ⇧ — convertir palabra/selección", "Double ⇧ — convertir le mot/la sélection", "Doppio ⇧ — converti parola o selezione", "Duplo ⇧ — converter palavra/seleção", "Çift ⇧ — kelimeyi/seçimi dönüştür", "Dubbel ⇧ — woord/selectie omzetten", "Dvojitý ⇧ — převést slovo/výběr", "Dubbel-⇧ — konvertera ord/markering", "Kaksois-⇧ — muunna sana/valinta", "Dublu ⇧ — convertește cuvântul/selecția", "Dupla ⇧ — szó/kijelölés átalakítása", "Διπλό ⇧ — μετατροπή λέξης/επιλογής", "Двоен ⇧ — смяна на езика на дума/селекция", "⇧ כפול — המרת מילה/בחירה", "⇧ مزدوج — تحويل الكلمة/التحديد", "दो बार ⇧ — शब्द/चयन बदलें", "⇧ 두 번 — 단어/선택 영역 변환"],
    "report": ["Report a problem…", "Повідомити про проблему…", "Zgłoś problem…", "Problem melden…", "Informar de un problema…", "Signaler un problème…", "Segnala un problema…", "Comunicar um problema…", "Sorun bildir…", "Probleem melden…", "Nahlásit problém…", "Rapportera ett problem…", "Ilmoita ongelmasta…", "Raportează o problemă…", "Probléma bejelentése…", "Αναφορά προβλήματος…", "Съобщете за проблем…", "דיווח על בעיה…", "الإبلاغ عن مشكلة…", "समस्या बताएँ…", "문제 신고…"],
    "reportBody": ["What happened and in which app:", "Що сталося і в якій програмі:", "Co się stało i w jakiej aplikacji:", "Was ist passiert und in welcher App:", "Qué pasó y en qué app:", "Ce qui s'est passé et dans quelle app :", "Cosa è successo e in quale app:", "O que aconteceu e em que app:", "Ne oldu ve hangi uygulamada:", "Wat er gebeurde en in welke app:", "Co se stalo a v jaké aplikaci:", "Vad hände och i vilken app:", "Mitä tapahtui ja missä ohjelmassa:", "Ce s-a întâmplat și în ce aplicație:", "Mi történt és melyik alkalmazásban:", "Τι συνέβη και σε ποια εφαρμογή:", "Какво се случи и в коя програма:", "מה קרה ובאיזה יישום:", "ماذا حدث وفي أي تطبيق:", "क्या हुआ और किस ऐप में:", "무슨 일이 어느 앱에서 있었나요:"],
    "update": ["Check for updates…", "Перевірити оновлення…", "Sprawdź aktualizacje…", "Nach Updates suchen…", "Buscar actualizaciones…", "Rechercher des mises à jour…", "Controlla aggiornamenti…", "Procurar atualizações…", "Güncellemeleri denetle…", "Zoek naar updates…", "Zkontrolovat aktualizace…", "Sök efter uppdateringar…", "Tarkista päivitykset…", "Caută actualizări…", "Frissítések keresése…", "Έλεγχος για ενημερώσεις…", "Проверка за актуализации…", "בדיקת עדכונים…", "التحقق من التحديثات…", "अपडेट जाँचें…", "업데이트 확인…"],
    "updateDue": ["⬆︎ An update may be available — check", "⬆︎ Можливо, вийшло оновлення — перевірити", "⬆︎ Może być dostępna aktualizacja — sprawdź", "⬆︎ Möglicherweise gibt es ein Update — prüfen", "⬆︎ Puede haber una actualización — comprobar", "⬆︎ Une mise à jour est peut-être disponible — vérifier", "⬆︎ Potrebbe essere disponibile un aggiornamento — verifica", "⬆︎ Pode haver uma atualização — verificar", "⬆︎ Güncelleme olabilir — denetle", "⬆︎ Er is misschien een update — controleren", "⬆︎ Možná je k dispozici aktualizace — zkontrolovat", "⬆︎ Det kan finnas en uppdatering — kontrollera", "⬆︎ Päivitys saattaa olla saatavilla — tarkista", "⬆︎ Poate există o actualizare — verifică", "⬆︎ Lehet, hogy van frissítés — ellenőrzés", "⬆︎ Ίσως υπάρχει ενημέρωση — έλεγχος", "⬆︎ Може да има актуализация — проверете", "⬆︎ ייתכן שיש עדכון — בדיקה", "⬆︎ قد يتوفر تحديث — تحقّق", "⬆︎ शायद नया अपडेट आया है — जाँचें", "⬆︎ 업데이트가 있을 수 있습니다 — 확인"],
    "donate": ["Support AURA Switcher ☕", "Підтримати AURA Switcher ☕", "Wesprzyj AURA Switcher ☕", "AURA Switcher unterstützen ☕", "Apoyar AURA Switcher ☕", "Soutenir AURA Switcher ☕", "Sostieni AURA Switcher ☕", "Apoiar o AURA Switcher ☕", "AURA Switcher'ı destekle ☕", "Steun AURA Switcher ☕", "Podpořit AURA Switcher ☕", "Stöd AURA Switcher ☕", "Tue AURA Switcheriä ☕", "Susține AURA Switcher ☕", "Az AURA Switcher támogatása ☕", "Υποστηρίξτε το AURA Switcher ☕", "Подкрепете AURA Switcher ☕", "תמכו ב-AURA Switcher ☕", "ادعم AURA Switcher ☕", "AURA Switcher का समर्थन करें ☕", "AURA Switcher 후원하기 ☕"],
    "quit": ["Quit", "Вихід", "Zakończ", "Beenden", "Salir", "Quitter", "Esci", "Sair", "Çık", "Stop", "Ukončit", "Avsluta", "Lopeta", "Ieșire", "Kilépés", "Έξοδος", "Изход", "יציאה", "إنهاء", "बंद करें", "종료"],
    "updNew": ["AURA Switcher {v} is out", "Вийшла AURA Switcher {v}", "Wyszła AURA Switcher {v}", "AURA Switcher {v} ist da", "Ya está aquí AURA Switcher {v}", "AURA Switcher {v} est sortie", "È disponibile AURA Switcher {v}", "Saiu o AURA Switcher {v}", "AURA Switcher {v} çıktı", "AURA Switcher {v} is uit", "Vyšel AURA Switcher {v}", "AURA Switcher {v} är här", "AURA Switcher {v} on julkaistu", "A apărut AURA Switcher {v}", "Megjelent az AURA Switcher {v}", "Κυκλοφόρησε το AURA Switcher {v}", "Излезе AURA Switcher {v}", "יצאה AURA Switcher {v}", "صدر AURA Switcher {v}", "AURA Switcher {v} आ गया है", "AURA Switcher {v} 출시"],
    "updLatest": ["You have the latest version", "У вас остання версія", "Masz najnowszą wersję", "Du hast die neueste Version", "Tienes la última versión", "Vous avez la dernière version", "Hai l’ultima versione", "Tem a versão mais recente", "En son sürümü kullanıyorsunuz", "Je hebt de nieuwste versie", "Máte nejnovější verzi", "Du har den senaste versionen", "Sinulla on uusin versio", "Ai cea mai nouă versiune", "A legújabb verzió van nálad", "Έχετε την τελευταία έκδοση", "Имате най-новата версия", "יש לכם את הגרסה העדכנית", "لديك أحدث إصدار", "आपके पास नवीनतम संस्करण है", "최신 버전을 사용 중입니다"],
    "updNotes": ["What's new", "Що нового", "Co nowego", "Was ist neu", "Novedades", "Nouveautés", "Novità", "Novidades", "Yenilikler", "Wat is er nieuw", "Co je nového", "Nyheter", "Uutta", "Ce e nou", "Újdonságok", "Τι νέο υπάρχει", "Какво ново", "מה חדש", "ما الجديد", "नया क्या है", "새로운 기능"],
    "updNow": ["Update", "Оновити", "Aktualizuj", "Aktualisieren", "Actualizar", "Mettre à jour", "Aggiorna", "Atualizar", "Güncelle", "Bijwerken", "Aktualizovat", "Uppdatera", "Päivitä", "Actualizează", "Frissítés", "Ενημέρωση", "Обнови", "עדכן", "تحديث", "अपडेट करें", "업데이트"],
    "updLater": ["Later", "Пізніше", "Później", "Später", "Más tarde", "Plus tard", "Più tardi", "Mais tarde", "Daha sonra", "Later", "Později", "Senare", "Myöhemmin", "Mai târziu", "Később", "Αργότερα", "По-късно", "מאוחר יותר", "لاحقًا", "बाद में", "나중에"],
    "updFail": ["Couldn't update automatically. Open the website to download it?", "Не вдалося оновити автоматично. Відкрити сайт, щоб завантажити?", "Nie udało się zaktualizować automatycznie. Otworzyć stronę, aby pobrać?", "Automatisches Update fehlgeschlagen. Website zum Herunterladen öffnen?", "No se pudo actualizar automáticamente. ¿Abrir la web para descargarla?", "Mise à jour automatique impossible. Ouvrir le site pour la télécharger ?", "Impossibile aggiornare automaticamente. Vuoi aprire il sito per scaricare l’aggiornamento?", "Não foi possível atualizar automaticamente. Abrir o site para baixar?", "Otomatik güncellenemedi. İndirmek için web sitesi açılsın mı?", "Automatisch bijwerken is mislukt. De website openen om te downloaden?", "Automatická aktualizace se nezdařila. Otevřít web a stáhnout?", "Kunde inte uppdatera automatiskt. Öppna webbplatsen för att ladda ner?", "Automaattinen päivitys epäonnistui. Avataanko sivusto latausta varten?", "Actualizarea automată a eșuat. Deschizi site-ul pentru descărcare?", "Az automatikus frissítés nem sikerült. Megnyitod a weboldalt a letöltéshez?", "Η αυτόματη ενημέρωση απέτυχε. Άνοιγμα του ιστότοπου για λήψη;", "Автоматичното обновяване не успя. Да се отвори ли сайтът за изтегляне?", "העדכון האוטומטי נכשל. לפתוח את האתר להורדה?", "تعذّر التحديث تلقائيًا. هل تريد فتح الموقع للتنزيل؟", "अपने-आप अपडेट नहीं हो सका। डाउनलोड के लिए वेबसाइट खोलें?", "자동으로 업데이트하지 못했습니다. 다운로드하려면 웹사이트를 열까요?"],
]
// END menu strings

func uiCode() -> String {
    if let l = engine.settings.uiLang, uiLangs.contains(l) { return l }
    let sys = String(Locale.preferredLanguages.first?.prefix(2) ?? "en")
    return sys == "ru" ? "uk" : uiLangs.contains(sys) ? sys : "en"
}

func L(_ key: String) -> String { uiStrings[key]?[uiLangs.firstIndex(of: uiCode()) ?? 0] ?? key }
/// empty submenu: tell where it gets filled — «Порожньо — ↑ «Не працювати в цій програмі»»
func emptyHint(_ fillKey: String) -> String { "\(L("empty")) — ↑ «\(L(fillKey).trimmingCharacters(in: .whitespaces))»" }

// MARK: - Engine

final class Engine {
    let settings = Settings()
    var layouts: [Layout] = loadLayouts()
    var frontBundle = NSWorkspace.shared.frontmostApplication?.bundleIdentifier ?? ""
    lazy var frontIsGame = isGame(NSWorkspace.shared.frontmostApplication)
    var recent: [String] = []   // last apps switched to, newest first — to exclude a full-screen game after leaving it
    var tap: CFMachPort?
    let src = CGEventSource(stateID: .privateState)

    var word: [Key] = []        // word being typed
    var lastWord: [Key] = []    // previous word, still convertible
    var trailing = 0            // spaces typed after lastWord
    var before: [Key] = []      // one-letter word right before `word` («z буду»), fixed together with it

    var shiftDownAt: TimeInterval = 0, lastShiftTap: TimeInterval = 0, sawOther = false
    var lastSelOut: String?, lastSelLang = 0

    var allCodes: Set<UInt16> { Set(layouts.flatMap { $0.toChar.keys.map(\.code) }) }
    lazy var codes = allCodes

    var active: Bool { settings.enabled && !frontIsGame && !settings.excluded.contains(frontBundle) }

    func reset() { word = []; lastWord = []; trailing = 0; before = []; lastCycled = []; lastCycledLang = nil }

    func layout(_ lang: String?) -> Layout? { layouts.first { $0.lang == lang } }
    func index(_ lang: String?) -> Int { layouts.firstIndex { $0.lang == lang } ?? 0 }
    /// the layout AURA selected itself, until the user switches to another one (see handleKey)
    var ownLang: String?
    func select(_ l: Layout) { ownLang = l.lang; TISSelectInputSource(l.source) }

    /// The first spell check in a language loads its dictionary (can take a moment). If that happens
    /// while the user types on, the correction is skipped — so load all dictionaries up front.
    func warmUpDictionaries() { reliable = [:]; for l in layouts { _ = isWord("aura", l.lang) } }

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
        if e.getIntegerValueField(.eventSourceUserData) == kMagic { return pass }
        if !active {
            if type == .keyDown, e.getIntegerValueField(.keyboardEventKeycode) == 49 {
                auraLog.notice("skip: paused (\(self.settings.enabled ? (self.frontIsGame ? "game" : "excluded app") : "turned off", privacy: .public) \(self.frontBundle, privacy: .public))")
            }
            return pass
        }

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
            if trailing > 0 { trailing -= 1; if trailing == 0 { word = lastWord; lastWord = []; before = [] } }
            else if !word.isEmpty { word.removeLast() }
        case 49: // space (already delivered to the app)
            if !word.isEmpty {
                lastWord = word; word = []; trailing = 1
                if settings.auto, settings.bindings[frontBundle] == nil {
                    let keys = lastWord, pre = before, at = ProcessInfo.processInfo.systemUptime
                    DispatchQueue.main.async { self.autoCorrect(keys, before: pre, spaceAt: at) }
                }
            } else if !lastWord.isEmpty { trailing += 1 }
        default:
            if codes.contains(code) {
                if word.isEmpty { before = lastWord.count == 1 && trailing == 1 ? lastWord : []; lastCycled = []; lastCycledLang = nil }
                if trailing > 0 || !lastWord.isEmpty { lastWord = []; trailing = 0 }
                var len = 0
                var buf = [UniChar](repeating: 0, count: 4)
                e.keyboardGetUnicodeString(maxStringLength: 4, actualStringLength: &len, unicodeString: &buf)
                var ch = len == 1 ? Unicode.Scalar(buf[0]).map(Character.init) : nil
                // after AURA switched the layout itself, key events keep the letters of the old one (macOS doesn't
                // update them), while the app already types in the new one: read the key with the layout we selected
                if let own = ownLang, let l = layout(own), let c = l.toChar[Key(code: code, shift: f.contains(.maskShift))] {
                    let caps = f.contains(.maskAlphaShift) && !f.contains(.maskShift)
                    ch = caps ? Character(c.uppercased()) : c
                }
                word.append(Key(code: code, shift: f.contains(.maskShift), caps: f.contains(.maskAlphaShift), ch: ch))
            } else { reset() }
        }
    }

    // one-letter words: a single letter before a fixed word is fixed too («z буду» → «я буду»),
    // unless it is already a word where it was typed (English «a»)
    static let letterWords = ["uk": "авзіїоуйя", "ru": "авжикосуя", "en": "ai"]

    /// «b tot» = «и еще»: «tot» is an English word, so alone it stays; but «b» isn't a word in English and
    /// «и» is in Russian, where «еще» is a real word too — the pair says which language was meant
    func pairFix(_ keys: [Key], before: [Key]) -> Layout? {
        guard before.count == 1, let src = typedLayout(keys) else { return nil }
        let bTyped = src.text(before)
        guard Engine.letterWords[src.lang]?.contains(bTyped.lowercased()) != true else { return nil }
        for l in layouts where l.lang != src.lang && l.maps(keys) && l.maps(before) {
            let b = l.text(before), t = l.text(keys)
            guard Engine.letterWords[l.lang]?.contains(b.lowercased()) == true, !consonantRun(t) else { continue }
            if dictionaryWorks(l.lang) ? isWord(t, l.lang) : model(l.lang)?.has(t) == true { return l }
        }
        return nil
    }

    func autoCorrect(_ keys: [Key], before: [Key] = [], spaceAt: Double = 0) {
        let t0 = ProcessInfo.processInfo.systemUptime
        // user kept typing meanwhile — skip
        guard lastWord == keys, word.isEmpty, trailing == 1 else {
            auraLog.notice("skip: kept typing (\(keys.count) letters, waited \(Int((t0 - spaceAt) * 1000)) ms)"); return
        }
        // the word was just switched by double ⇧: the user chose its layout, don't undo it
        // (its keys still carry the letters first typed, so the check would see the old language)
        if keys == lastCycled { return }
        noFix = ""
        let found = autoFix(keys) ?? pairFix(keys, before: before)
        let ms = Int((ProcessInfo.processInfo.systemUptime - t0) * 1000)
        if ms > 100 { auraLog.notice("slow check: \(ms) ms") }
        guard let fix = found else {
            if !noFix.isEmpty { auraLog.notice("skip: \(self.noFix, privacy: .public) (\(keys.count) letters)") }
            return
        }
        if lastWord != keys || !word.isEmpty || trailing != 1 {
            auraLog.notice("skip: kept typing during a \(ms) ms check"); return
        }
        let out = fix.text(keys)
        let src = typedLayout(keys), typed = src?.text(keys)
        let b = fix.text(before), bTyped = src?.text(before) ?? ""
        let fixBefore = before.count == 1 && b.count == 1 && b != bTyped
            && Engine.letterWords[fix.lang]?.contains(b.lowercased()) == true
            && Engine.letterWords[src?.lang ?? ""]?.contains(bTyped.lowercased()) != true
        // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
        let shown = typed?.count ?? keys.count  // on screen: accented letters are one character
        // switch first, then retype: switched after the text, the macOS indicator at the caret showed the old layout
        select(fix)
        if fixBefore { replace(count: bTyped.count + shown + 2, with: b + " " + out + " ") }
        else if out != typed { replace(count: shown + 1, with: out + " ") }
        auraLog.notice("fixed: \(src?.lang ?? "?", privacy: .public) → \(fix.lang, privacy: .public) (\(keys.count) letters)")
        Sounds.play(Sounds.auto)
    }

    // MARK: auto detection

    // the spell checker takes any two letters for a word (":D" → «ЖВ», "in" → «шт»): only common ones count
    static let twoLetterWords: [String: Set<String>] = [
        "uk": ["як", "що", "не", "та", "до", "на", "за", "по", "ти", "ми", "ви", "їх", "це", "її", "ще", "бо", "чи", "ні",
               "де", "то", "ну", "ой", "аж", "би", "зі", "із", "ця", "ці", "ті", "ок"],
        "ru": ["не", "на", "по", "за", "до", "от", "из", "во", "ко", "то", "же", "бы", "ли", "мы", "вы", "он", "её", "их",
               "да", "но", "ни", "уж", "ой", "ну", "ах", "ох", "со", "об", "ты", "та", "те", "эй", "ок"],
        "en": ["is", "in", "on", "at", "to", "it", "he", "we", "me", "my", "of", "or", "an", "as", "be", "by", "do", "go",
               "if", "no", "so", "up", "us", "am", "hi", "ok", "oh", "ah", "ha", "eh", "yo", "id"]]

    func isWord(_ s: String, _ lang: String) -> Bool {
        var w = s.trimmingCharacters(in: .punctuationCharacters.union(.symbols))
        guard w.count >= 2, w.allSatisfy(\.isLetter) || w.contains("'") || w.contains("-") else { return false }
        if w.count == 2, let set = Engine.twoLetterWords[lang] { return set.contains(w.lowercased()) }
        if w == w.uppercased() { w = w.lowercased() }  // all caps passes any spell check
        guard dictionaryWorks(lang) else { return false }
        if spell(w, lang) { return true }
        // names typed in lower case: «вінницею» is only in the dictionary as «Вінницею». Not for Latin
        // (English names like «Tot» would make gibberish a word) and not for short words
        return w.count >= 5 && !isLatin(w) && w == w.lowercased() && spell(w.prefix(1).uppercased() + w.dropFirst(), lang)
    }

    func spell(_ w: String, _ lang: String) -> Bool {
        let checker = NSSpellChecker.shared
        guard let l = checker.availableLanguages.first(where: { $0.hasPrefix(lang) }) else { return false }
        return checker.checkSpelling(of: w, startingAt: 0, language: l, wrap: false,
                                     inSpellDocumentWithTag: 0, wordCount: nil).location == NSNotFound
    }

    /// A language whose dictionary macOS hasn't downloaded (not a system language) accepts any string:
    /// English typos would turn into gibberish in it. Checked once per language on nonsense typed in its layout.
    var reliable: [String: Bool] = [:]
    func dictionaryWorks(_ lang: String) -> Bool {
        if let r = reliable[lang] { return r }
        // no dictionary at all (Belarusian): not "works" — the language model takes over
        var r = NSSpellChecker.shared.availableLanguages.contains { $0.hasPrefix(lang) }
        if r, let l = layouts.first(where: { $0.lang == lang }) {
            // nonsense from the layout's own letters (keys like Q or Z may type several characters elsewhere)
            // only the layout's own script: macOS «Russian – PC» also types a Latin «Ë», and «ËËËËËËË» passes
            // the Russian spell check (it skips Latin words) — that wrongly switched the Russian dictionary off
            let all = Array(Set(l.toChar.values.filter(\.isLetter))).sorted()
            let own = all.filter { !isLatin(String($0)) }
            let letters = own.count >= 3 ? own : all
            if letters.count >= 3 {
                for w in [String(repeating: letters[0], count: 7), String((0..<7).map { letters[1 + $0 % 2] })] where spell(w, lang) { r = false }
            }
        }
        reliable[lang] = r
        return r
    }

    /// five Cyrillic consonants in a row: gibberish the spell checker let through ("brachii," → «икфсршшб»)
    func consonantRun(_ s: String) -> Bool {
        var run = 0
        for c in s.lowercased() { run = "бвгґджзйклмнпрстфхцчшщ".contains(c) ? run + 1 : 0; if run >= 5 { return true } }
        return false
    }

    /// Code typed in the wrong layout — «сщтіщдуюдщп(ещефд)ж» is console.log(total); — is one "word" with
    /// dots and brackets: starts with a letter, and every part (split on non-letters and camelCase) is a word.
    func isCode(_ t: String, _ lang: String) -> Bool {
        guard t.first?.isLetter == true else { return false }
        var parts: [String] = [], cur = ""
        for c in t {
            if !c.isLetter { if !cur.isEmpty { parts.append(cur) }; cur = ""; continue }
            if c.isUppercase, let last = cur.last, last.isLowercase { parts.append(cur); cur = "" }
            cur.append(c)
        }
        if !cur.isEmpty { parts.append(cur) }
        let punctuated = t.trimmingCharacters(in: CharacterSet(charactersIn: ";,.)")).contains { !$0.isLetter }
        return (parts.count > 1 || punctuated) && parts.contains { $0.count >= 3 }
            && parts.allSatisfy { $0.count >= 2 && isWord($0.lowercased(), lang) }
    }

    func isCyrillic(_ s: String) -> Bool { s.unicodeScalars.contains { (0x400...0x52F).contains($0.value) } }
    func isLatin(_ s: String) -> Bool { s.unicodeScalars.allSatisfy { $0.value < 0x250 } }

    /// Layout the word was really typed in: the one whose characters match what the key events produced
    /// (prefers the system's current layout when several match, e.g. UK/RU words without і/ы).
    func typedLayout(_ keys: [Key]) -> Layout? {
        let sys = layout(currentLang())
        let actual = String(keys.compactMap(\.ch))
        guard actual.count == keys.count else { return sys }
        let matches = layouts.filter { $0.text(keys) == actual }
        if let sys, matches.contains(where: { $0 === sys }) { return sys }
        return matches.first ?? sys
    }

    var models: [String: LangModel?] = [:]
    func model(_ lang: String) -> LangModel? {
        if let m = models[lang] { return m }
        let m = LangModel(lang); models[lang] = m; return m
    }
    /// how much more typical `t` is of its language than `typed` of its own (trigram costs); nil without both models
    func margin(_ typed: String, _ cur: String, _ t: String, _ lang: String) -> Double? {
        guard let a = model(cur), let b = model(lang) else { return nil }
        return a.cost(typed) - b.cost(t)
    }
    // tuned on ~2000 words per language (tools/langmodel.py): a word with no dictionary must look clearly
    // more typical than what was typed; a real dictionary word only somewhat more
    static let marginNeeded = 20.0, marginForDictWord = 5.0, marginCyrillic = 11.0  // Belarusian ↔ Russian: «длінный» 13, true «сіла» 4

    var noFix = ""  // why autoFix found nothing (for the log)
    func autoFix(_ keys: [Key]) -> Layout? {
        guard keys.count >= 2, let cur = typedLayout(keys) else { noFix = keys.count >= 2 ? "layout not recognized" : ""; return nil }
        let typed = cur.text(keys)
        let curDict = dictionaryWorks(cur.lang)
        // typed in a language we can't check at all: it may well be right — leave it
        guard curDict || model(cur.lang) != nil else { noFix = "no dictionary for \(cur.lang)"; return nil }
        if settings.words.contains(typed.lowercased()) || (curDict ? isWord(typed, cur.lang) : model(cur.lang)!.has(typed)) { noFix = "already a word in \(cur.lang)"; return nil }
        // every other layout is a candidate; take the strongest kind of evidence, not the first layout in the
        // list: with RU, UK, EN in that order «ghbdsn» must become the Ukrainian word «привіт», not the
        // Russian look-alike «привыт» that only passes on letter statistics
        // equal evidence (both are dictionary words: «было» / «біло» from the same keys) — a frequent word of its
        // language wins over a rare one, then the letter statistics decide
        var best: (tier: Int, freq: Int, m: Double, l: Layout)? = nil
        let start = index(cur.lang)
        for i in 1..<max(layouts.count, 1) {
            let l = layouts[(start + i) % layouts.count]
            let t = l.text(keys)
            // same text in two Latin layouts (an English typo that is a Polish word): nothing to fix.
            // Cyrillic UK/RU words spelled the same still switch the layout — that's what users mix up.
            if t == typed, isLatin(t) { continue }
            guard l.maps(keys), !consonantRun(t) else { continue }
            let m = margin(typed, cur.lang, t, l.lang) ?? 0
            var tier = 0
            if settings.words.contains(t.lowercased()) { tier = 4 }  // the user's own word («скілі»)
            else if isLatin(t), !isLatin(typed), isCode(t, l.lang) { tier = 3 }
            else if dictionaryWorks(l.lang) {
                // without the typed language's dictionary, also ask the models which side looks real;
                // close languages in one script (Belarusian ↔ Russian) need the stricter margin
                let need = isCyrillic(typed) && isCyrillic(t) ? Engine.marginCyrillic : Engine.marginForDictWord
                if isWord(t, l.lang), curDict || m >= need { tier = 2 }
                // slang the dictionary doesn't know («гуглити»): long, another script, clearly typical of the language
                else if t.count >= 5, isLatin(typed) != isLatin(t), m >= Engine.marginNeeded { tier = 1 }
            } else if let lm = model(l.lang) {
                // no dictionary there: a frequent word, or clearly more typical than what was typed
                if lm.has(t) { tier = 2 } else if m >= Engine.marginNeeded { tier = 1 }
            }
            let freq = model(l.lang)?.has(t) == true ? 1 : 0
            if tier > 0, best == nil || (tier, freq, m) > (best!.tier, best!.freq, best!.m) { best = (tier, freq, m, l) }
        }
        if best == nil { noFix = "not a word in \(cur.lang), nothing better in other layouts" }
        return best?.l
    }

    // MARK: double shift

    func doubleShift() {
        Sounds.play(Sounds.shift)
        let sel = axSelectedText()
        if let sel, !sel.isEmpty { convertSelection(sel); return }
        // the word being typed, or the previous one right after a single space; after two or more spaces the text
        // stays as it is and double ⇧ only switches the layout
        let keys = !word.isEmpty ? word : trailing == 1 ? lastWord : []
        if !keys.isEmpty { cycleWord(keys, trailing: word.isEmpty ? trailing : 0); return }
        // the app says nothing is selected and can be trusted: switch now, without the ⌘C round trip (up to 0.4 s)
        if sel != nil, !webEngine(frontBundle) { cycleLayout(); return }
        clipboardSelection { sel in
            if let sel, !sel.isEmpty, !isCopiedLine(sel) { self.convertSelection(sel) } else { self.cycleLayout() }
        }
    }

    func next(after lang: String?) -> Layout? {
        layouts.isEmpty ? nil : layouts[(index(lang) + 1) % layouts.count]
    }

    func cycleLayout() { if let n = next(after: currentLang()) { select(n) } }

    /// the word the last double ⇧ converted and where it went: pressing again keeps cycling from there
    /// (right after a switch the system may still report the old layout)
    var lastCycled: [Key] = [], lastCycledLang: String? = nil

    func cycleWord(_ keys: [Key], trailing: Int) {
        // first press: the most likely layout (as auto-correction would pick it), then the next ones in turn
        let again = keys == lastCycled && lastCycledLang != nil
        // (not the layout the word is already in: right after auto-correction that would change nothing)
        let cur = currentLang()
        guard let n = (again ? nil : autoFix(keys).flatMap { $0.lang == cur ? nil : $0 }) ?? next(after: again ? lastCycledLang : cur) else { return }
        lastCycled = keys; lastCycledLang = n.lang
        auraLog.notice("double ⇧: word → \(n.lang, privacy: .public)")
        let shown = typedLayout(keys)?.text(keys).count ?? keys.count
        select(n)  // before the text: see autoCorrect
        replace(count: shown + trailing, with: n.text(keys) + String(repeating: " ", count: trailing))
    }

    func convertSelection(_ text: String) {
        guard !layouts.isEmpty else { return }
        var srcIdx: Int
        if text == lastSelOut, lastSelLang < layouts.count { srcIdx = lastSelLang } else {
            let cur = index(currentLang())
            let scores = layouts.map { l in text.filter { $0.isLetter && l.keys(for: $0) != nil }.count }
            srcIdx = scores.indices.max { a, b in
                scores[a] != scores[b] ? scores[a] < scores[b] : (a != cur && b == cur)
            } ?? cur
        }
        let srcL = layouts[srcIdx], dstIdx = (srcIdx + 1) % layouts.count, dst = layouts[dstIdx]
        // runs of convertible characters go through the keys (so accents compose), the rest stays
        var out = "", run: [Key] = []
        for c in text {
            if let ks = srcL.keys(for: c), ks.allSatisfy({ dst.toChar[$0] != nil }) { run += ks; continue }
            out += dst.text(run) + String(c); run = []
        }
        out += dst.text(run)
        typeText(out)
        for _ in 0..<out.count { postKey(123, flags: .maskShift) } // reselect (⇧← moves by character)
        lastSelOut = out; lastSelLang = dstIdx
        select(dst)
    }

    // MARK: selection

    /// the selection as the focused element reports it: "" = nothing selected, nil = the app doesn't say
    func axSelectedText() -> String? {
        let sys = AXUIElementCreateSystemWide()
        var focused: CFTypeRef?
        guard AXUIElementCopyAttributeValue(sys, kAXFocusedUIElementAttribute as CFString, &focused) == .success,
              let el = focused else { return nil }
        var sel: CFTypeRef?
        AXUIElementCopyAttributeValue(el as! AXUIElement, kAXSelectedTextAttribute as CFString, &sel)
        guard let s = sel as? String else { return nil }
        if s.isEmpty {  // empty text with a non-empty range: the app reports it wrong
            var r: CFTypeRef?, range = CFRange()
            guard AXUIElementCopyAttributeValue(el as! AXUIElement, kAXSelectedTextRangeAttribute as CFString, &r) == .success,
                  let r, AXValueGetValue(r as! AXValue, .cfRange, &range), range.length == 0 else { return nil }
        }
        return s
    }

    /// Chrome-based browsers and Electron apps (VS Code, Slack, Discord) report an empty selection while
    /// text is selected when their accessibility is on: there the "" isn't trusted
    var webEngineCache: [String: Bool] = [:]
    func webEngine(_ bundle: String) -> Bool {
        if let r = webEngineCache[bundle] { return r }
        var r = true  // unknown app: the safe way
        if let url = NSWorkspace.shared.urlForApplication(withBundleIdentifier: bundle) {
            let fw = (try? FileManager.default.contentsOfDirectory(atPath: url.appendingPathComponent("Contents/Frameworks").path)) ?? []
            r = fw.contains { $0.contains("Electron") || $0.contains("Chrom") || $0.hasSuffix(" Framework.framework") }
        }
        webEngineCache[bundle] = r
        return r
    }

    /// Copies the selection with ⌘C and puts the user's clipboard back exactly as it was
    /// (every item and type: images, files, rich text — not only plain text).
    func clipboardSelection(_ done: @escaping (String?) -> Void) {
        let pb = NSPasteboard.general
        let saved: [NSPasteboardItem] = (pb.pasteboardItems ?? []).map { item in
            let copy = NSPasteboardItem()
            for t in item.types { if let d = item.data(forType: t) { copy.setData(d, forType: t) } }
            return copy
        }
        let count = pb.changeCount
        func restore() { pb.clearContents(); if !saved.isEmpty { pb.writeObjects(saved) } }
        postKey(8, flags: .maskCommand) // ⌘C
        // slow apps may copy late: wait up to 0.4 s, and if the copy lands even later, still restore
        func poll(_ left: Int) {
            if pb.changeCount != count {
                let s = pb.string(forType: .string); restore(); done(s); return
            }
            if left > 0 { DispatchQueue.main.asyncAfter(deadline: .now() + 0.02) { poll(left - 1) }; return }
            done(nil)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) { if pb.changeCount == count + 1 { restore() } }
        }
        poll(20)
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
