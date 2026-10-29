import Cocoa
import Carbon
import ServiceManagement
import ApplicationServices

// MARK: - Layouts

let kMagic: Int64 = 0x4B535754 // marks our own synthetic events
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

    func text(_ keys: [Key]) -> String {
        keys.reduce(into: "") { s, k in
            guard let c = toChar[k] else { return }
            if k.caps, c.isLetter { s += c.uppercased() } else { s.append(c) }
        }
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

func currentLang() -> String? { sourceLang(TISCopyCurrentKeyboardInputSource().takeRetainedValue()) }

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

/// Updates without going online: the app never contacts a server. The menu opens the site's update page in
/// the browser (only the version and menu language are in its address), and some time after this build (or
/// after the last check) the menu and the icon remind that a newer version may be out. The build date is
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
    /// Short soft sine blips: one tone for auto-correction, two rising tones for double ⇧.
    static func blip(_ freqs: [Double]) -> Data {
        let rate = 44100, toneLen = rate * 45 / 1000, gap = rate * 25 / 1000
        var pcm: [Int16] = []
        for (i, f) in freqs.enumerated() {
            if i > 0 { pcm += [Int16](repeating: 0, count: gap) }
            for n in 0..<toneLen {
                let t = Double(n) / Double(rate)
                let env = min(1, Double(n) / 200) * min(1, Double(toneLen - n) / 800)  // no clicks
                pcm.append(Int16(sin(2 * .pi * f * t) * env * 0.22 * 32767))
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
    static let auto = NSSound(data: blip([1175]))
    static let shift = NSSound(data: blip([880, 1320]))
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
    "access": ["⚠︎ Grant access: Accessibility", "⚠︎ Надайте доступ: Універсальний доступ", "⚠︎ Nadaj dostęp: Dostępność", "⚠︎ Zugriff erlauben: Bedienungshilfen", "⚠︎ Conceder acceso: Accesibilidad", "⚠︎ Autoriser l'accès : Accessibilité", "⚠︎ Concedi accesso: Accessibilità", "⚠︎ Conceder acesso: Acessibilidade", "⚠︎ Erişim izni verin: Erişilebilirlik", "⚠︎ Geef toegang: Toegankelijkheid", "⚠︎ Udělte přístup: Zpřístupnění", "⚠︎ Ge åtkomst: Hjälpmedel", "⚠︎ Anna käyttöoikeus: Käyttöapu", "⚠︎ Acordați acces: Accesibilitate", "⚠︎ Hozzáférés megadása: Kisegítő lehetőségek", "⚠︎ Δώστε πρόσβαση: Προσβασιμότητα", "⚠︎ Дайте достъп: Достъпност", "⚠︎ תנו גישה: נגישות", "⚠︎ امنح الوصول: تسهيلات الاستخدام", "⚠︎ अनुमति दें: सुलभता", "⚠︎ 접근 권한 허용: 손쉬운 사용"],
    "app": ["App: ", "Програма: ", "Aplikacja: ", "App: ", "App: ", "App : ", "App: ", "App: ", "Uygulama: ", "App: ", "Aplikace: ", "App: ", "Ohjelma: ", "Aplicație: ", "Alkalmazás: ", "Εφαρμογή: ", "Програма: ", "יישום: ", "التطبيق: ", "ऐप: ", "앱: "],
    "exclude": ["Don't work in this app", "Не працювати в цій програмі", "Nie działaj w tej aplikacji", "In dieser App nicht aktiv", "No funcionar en esta app", "Ne pas fonctionner dans cette app", "Non funzionare in questa app", "Não funcionar nesta app", "Bu uygulamada çalışma", "Niet werken in deze app", "Nefungovat v této aplikaci", "Arbeta inte i den här appen", "Älä toimi tässä ohjelmassa", "Nu funcționa în această aplicație", "Ne működjön ebben az alkalmazásban", "Να μη λειτουργεί σε αυτή την εφαρμογή", "Да не работи в тази програма", "לא לפעול ביישום זה", "لا تعمل في هذا التطبيق", "इस ऐप में काम न करें", "이 앱에서 작동 안 함"],
    "game": ["Game — AURA paused", "Гра — AURA на паузі", "Gra — AURA wstrzymana", "Spiel — AURA pausiert", "Juego — AURA en pausa", "Jeu — AURA en pause", "Gioco — AURA in pausa", "Jogo — AURA em pausa", "Oyun — AURA duraklatıldı", "Game — AURA gepauzeerd", "Hra — AURA pozastavena", "Spel — AURA pausad", "Peli — AURA tauolla", "Joc — AURA în pauză", "Játék — AURA szüneteltetve", "Παιχνίδι — AURA σε παύση", "Игра — AURA е на пауза", "משחק — AURA מושהית", "لعبة — AURA متوقف مؤقتًا", "गेम — AURA रुका हुआ", "게임 — AURA 일시 중지됨"],
    "appLang": ["Language for this app", "Мова для цієї програми", "Język dla tej aplikacji", "Sprache für diese App", "Idioma para esta app", "Langue pour cette app", "Lingua per questa app", "Idioma para esta app", "Bu uygulama için dil", "Taal voor deze app", "Jazyk pro tuto aplikaci", "Språk för den här appen", "Kieli tälle ohjelmalle", "Limba pentru această aplicație", "Nyelv ehhez az alkalmazáshoz", "Γλώσσα για αυτή την εφαρμογή", "Език за тази програма", "שפה ליישום זה", "لغة هذا التطبيق", "इस ऐप की भाषा", "이 앱의 언어"],
    "noBinding": ["No binding", "Без прив'язки", "Bez przypisania", "Keine Zuordnung", "Sin asignación", "Aucune association", "Nessuna associazione", "Sem associação", "Atama yok", "Geen koppeling", "Bez přiřazení", "Ingen koppling", "Ei liitosta", "Fără asociere", "Nincs hozzárendelés", "Χωρίς αντιστοίχιση", "Без обвързване", "ללא שיוך", "بدون ربط", "कोई बंधन नहीं", "지정 안 함"],
    "exclusions": ["Exclusions", "Винятки", "Wyjątki", "Ausnahmen", "Excepciones", "Exceptions", "Eccezioni", "Exceções", "İstisnalar", "Uitzonderingen", "Výjimky", "Undantag", "Poikkeukset", "Excepții", "Kivételek", "Εξαιρέσεις", "Изключения", "חריגים", "الاستثناءات", "अपवाद", "예외"],
    "recent": ["Don't work in (recent apps)", "Не працювати в (останні програми)", "Nie działaj w (ostatnie aplikacje)", "Nicht aktiv in (letzte Apps)", "No funcionar en (apps recientes)", "Ne pas fonctionner dans (apps récentes)", "Non funzionare in (app recenti)", "Não funcionar em (apps recentes)", "Şurada çalışma (son uygulamalar)", "Niet werken in (recente apps)", "Nefungovat v (nedávné aplikace)", "Arbeta inte i (senaste appar)", "Älä toimi ohjelmassa (viimeisimmät)", "Nu funcționa în (aplicații recente)", "Ne működjön itt (legutóbbi alkalmazások)", "Να μη λειτουργεί σε (πρόσφατες εφαρμογές)", "Да не работи в (последни програми)", "לא לפעול ב (יישומים אחרונים)", "لا تعمل في (التطبيقات الأخيرة)", "इनमें काम न करें (हाल के ऐप)", "작동 안 함 (최근 앱)"],
    "bindings": ["Language bindings", "Прив'язки мов", "Przypisania języków", "Sprachzuordnungen", "Idiomas asignados", "Langues associées", "Lingue associate", "Idiomas associados", "Dil atamaları", "Taalkoppelingen", "Přiřazení jazyků", "Språkkopplingar", "Kieliliitokset", "Limbi asociate", "Nyelvi hozzárendelések", "Αντιστοιχίσεις γλωσσών", "Обвързани езици", "שיוכי שפה", "ربط اللغات", "भाषा बंधन", "언어 지정"],
    "empty": ["Empty", "Порожньо", "Pusto", "Leer", "Vacío", "Vide", "Vuoto", "Vazio", "Boş", "Leeg", "Prázdné", "Tomt", "Tyhjä", "Gol", "Üres", "Κενό", "Празно", "ריק", "فارغ", "खाली", "비어 있음"],
    "login": ["Launch at login", "Запускати під час входу", "Uruchamiaj przy logowaniu", "Bei Anmeldung starten", "Abrir al iniciar sesión", "Ouvrir à la connexion", "Apri al login", "Abrir ao iniciar sessão", "Oturum açılışında başlat", "Starten bij inloggen", "Spouštět po přihlášení", "Starta vid inloggning", "Käynnistä kirjautuessa", "Pornește la autentificare", "Indítás bejelentkezéskor", "Εκκίνηση κατά τη σύνδεση", "Стартиране при вход", "הפעלה בעת התחברות", "التشغيل عند تسجيل الدخول", "लॉगिन पर शुरू करें", "로그인 시 실행"],
    "words": ["Word exceptions…", "Слова-винятки…", "Wyjątki słów…", "Wortausnahmen…", "Palabras excluidas…", "Mots exclus…", "Parole escluse…", "Palavras excluídas…", "Kelime istisnaları…", "Woorduitzonderingen…", "Výjimky slov…", "Ordundantag…", "Sanapoikkeukset…", "Cuvinte excluse…", "Szókivételek…", "Εξαιρέσεις λέξεων…", "Думи-изключения…", "מילים חריגות…", "كلمات مستثناة…", "शब्द अपवाद…", "예외 단어…"],
    "wordsTitle": ["AURA Switcher — word exceptions", "AURA Switcher — слова-винятки", "AURA Switcher — wyjątki słów", "AURA Switcher — Wortausnahmen", "AURA Switcher — palabras excluidas", "AURA Switcher — mots exclus", "AURA Switcher — parole escluse", "AURA Switcher — palavras excluídas", "AURA Switcher — kelime istisnaları", "AURA Switcher — woorduitzonderingen", "AURA Switcher — výjimky slov", "AURA Switcher — ordundantag", "AURA Switcher — sanapoikkeukset", "AURA Switcher — cuvinte excluse", "AURA Switcher — szókivételek", "AURA Switcher — εξαιρέσεις λέξεων", "AURA Switcher — думи-изключения", "AURA Switcher — מילים חריגות", "AURA Switcher — كلمات مستثناة", "AURA Switcher — शब्द अपवाद", "AURA Switcher — 예외 단어"],
    "wordsInfo": ["AURA Switcher will never auto-correct these words. Separate them with commas.", "Ці слова AURA Switcher ніколи не виправлятиме автоматично. Через кому.", "AURA Switcher nigdy nie poprawi automatycznie tych słów. Oddziel je przecinkami.", "Diese Wörter korrigiert AURA Switcher nie automatisch. Durch Kommas trennen.", "AURA Switcher nunca corregirá estas palabras automáticamente. Sepáralas con comas.", "AURA Switcher ne corrigera jamais ces mots automatiquement. Séparez-les par des virgules.", "AURA Switcher non correggerà mai automaticamente queste parole. Separale con virgole.", "O AURA Switcher nunca corrigirá estas palavras automaticamente. Separe-as com vírgulas.", "AURA Switcher bu kelimeleri asla otomatik düzeltmez. Virgülle ayırın.", "AURA Switcher corrigeert deze woorden nooit automatisch. Scheid ze met komma's.", "Tato slova AURA Switcher nikdy automaticky neopraví. Oddělte je čárkami.", "AURA Switcher rättar aldrig dessa ord automatiskt. Separera med kommatecken.", "AURA Switcher ei koskaan korjaa näitä sanoja automaattisesti. Erota pilkuilla.", "AURA Switcher nu va corecta niciodată automat aceste cuvinte. Separați-le prin virgulă.", "Ezeket a szavakat az AURA Switcher soha nem javítja automatikusan. Vesszővel válassza el.", "Το AURA Switcher δεν θα διορθώνει ποτέ αυτόματα αυτές τις λέξεις. Χωρίστε τις με κόμμα.", "AURA Switcher никога няма да поправя автоматично тези думи. Разделете ги със запетая.", "AURA Switcher לעולם לא יתקן מילים אלה אוטומטית. הפרידו בפסיקים.", "لن يصحح AURA Switcher هذه الكلمات تلقائيًا أبدًا. افصل بينها بفواصل.", "AURA Switcher इन शब्दों को कभी अपने आप नहीं सुधारेगा। अल्पविराम से अलग करें।", "AURA Switcher는 이 단어들을 자동으로 수정하지 않습니다. 쉼표로 구분하세요."],
    "save": ["Save", "Зберегти", "Zapisz", "Sichern", "Guardar", "Enregistrer", "Salva", "Guardar", "Kaydet", "Bewaar", "Uložit", "Spara", "Tallenna", "Salvează", "Mentés", "Αποθήκευση", "Запази", "שמירה", "حفظ", "सहेजें", "저장"],
    "cancel": ["Cancel", "Скасувати", "Anuluj", "Abbrechen", "Cancelar", "Annuler", "Annulla", "Cancelar", "İptal", "Annuleer", "Zrušit", "Avbryt", "Kumoa", "Anulează", "Mégse", "Ακύρωση", "Отказ", "ביטול", "إلغاء", "रद्द करें", "취소"],
    "uiLang": ["Menu language", "Мова меню", "Język menu", "Menüsprache", "Idioma del menú", "Langue du menu", "Lingua del menu", "Idioma do menu", "Menü dili", "Menutaal", "Jazyk nabídky", "Menyspråk", "Valikon kieli", "Limba meniului", "Menü nyelve", "Γλώσσα μενού", "Език на менюто", "שפת התפריט", "لغة القائمة", "मेनू की भाषा", "메뉴 언어"],
    "system": ["System", "Системна", "Systemowy", "System", "Sistema", "Système", "Sistema", "Sistema", "Sistem", "Systeem", "Systémový", "System", "Järjestelmä", "Sistem", "Rendszer", "Συστήματος", "Системен", "מערכת", "النظام", "सिस्टम", "시스템"],
    "hint": ["Double ⇧ — convert word/selection", "Подвійний ⇧ — змінити мову слова/виділення", "Podwójny ⇧ — zmień układ słowa/zaznaczenia", "Doppel-⇧ — Wort/Auswahl umwandeln", "Doble ⇧ — convertir palabra/selección", "Double ⇧ — convertir le mot/la sélection", "Doppio ⇧ — converti parola/selezione", "Duplo ⇧ — converter palavra/seleção", "Çift ⇧ — kelimeyi/seçimi dönüştür", "Dubbel ⇧ — woord/selectie omzetten", "Dvojitý ⇧ — převést slovo/výběr", "Dubbel-⇧ — konvertera ord/markering", "Kaksois-⇧ — muunna sana/valinta", "Dublu ⇧ — convertește cuvântul/selecția", "Dupla ⇧ — szó/kijelölés átalakítása", "Διπλό ⇧ — μετατροπή λέξης/επιλογής", "Двоен ⇧ — смяна на езика на дума/селекция", "⇧ כפול — המרת מילה/בחירה", "⇧ مزدوج — تحويل الكلمة/التحديد", "दो बार ⇧ — शब्द/चयन बदलें", "⇧ 두 번 — 단어/선택 영역 변환"],
    "report": ["Report a problem…", "Повідомити про проблему…", "Zgłoś problem…", "Problem melden…", "Informar de un problema…", "Signaler un problème…", "Segnala un problema…", "Comunicar um problema…", "Sorun bildir…", "Probleem melden…", "Nahlásit problém…", "Rapportera ett problem…", "Ilmoita ongelmasta…", "Raportează o problemă…", "Probléma bejelentése…", "Αναφορά προβλήματος…", "Съобщете за проблем…", "דיווח על בעיה…", "الإبلاغ عن مشكلة…", "समस्या बताएँ…", "문제 신고…"],
    "reportBody": ["What happened and in which app:", "Що сталося і в якій програмі:", "Co się stało i w jakiej aplikacji:", "Was ist passiert und in welcher App:", "Qué pasó y en qué app:", "Ce qui s'est passé et dans quelle app :", "Cosa è successo e in quale app:", "O que aconteceu e em que app:", "Ne oldu ve hangi uygulamada:", "Wat er gebeurde en in welke app:", "Co se stalo a v jaké aplikaci:", "Vad hände och i vilken app:", "Mitä tapahtui ja missä ohjelmassa:", "Ce s-a întâmplat și în ce aplicație:", "Mi történt és melyik alkalmazásban:", "Τι συνέβη και σε ποια εφαρμογή:", "Какво се случи и в коя програма:", "מה קרה ובאיזה יישום:", "ماذا حدث وفي أي تطبيق:", "क्या हुआ और किस ऐप में:", "무슨 일이 어느 앱에서 있었나요:"],
    "update": ["Check for updates…", "Перевірити оновлення…", "Sprawdź aktualizacje…", "Nach Updates suchen…", "Buscar actualizaciones…", "Rechercher des mises à jour…", "Controlla aggiornamenti…", "Procurar atualizações…", "Güncellemeleri denetle…", "Zoek naar updates…", "Zkontrolovat aktualizace…", "Sök efter uppdateringar…", "Tarkista päivitykset…", "Caută actualizări…", "Frissítések keresése…", "Έλεγχος για ενημερώσεις…", "Проверка за актуализации…", "בדיקת עדכונים…", "التحقق من التحديثات…", "अपडेट जाँचें…", "업데이트 확인…"],
    "updateDue": ["⬆︎ An update may be available — check", "⬆︎ Можливо, вийшло оновлення — перевірити", "⬆︎ Może być dostępna aktualizacja — sprawdź", "⬆︎ Möglicherweise gibt es ein Update — prüfen", "⬆︎ Puede haber una actualización — comprobar", "⬆︎ Une mise à jour est peut-être disponible — vérifier", "⬆︎ Potrebbe esserci un aggiornamento — controlla", "⬆︎ Pode haver uma atualização — verificar", "⬆︎ Güncelleme olabilir — denetle", "⬆︎ Er is misschien een update — controleren", "⬆︎ Možná je k dispozici aktualizace — zkontrolovat", "⬆︎ Det kan finnas en uppdatering — kontrollera", "⬆︎ Päivitys saattaa olla saatavilla — tarkista", "⬆︎ Poate există o actualizare — verifică", "⬆︎ Lehet, hogy van frissítés — ellenőrzés", "⬆︎ Ίσως υπάρχει ενημέρωση — έλεγχος", "⬆︎ Може да има актуализация — проверете", "⬆︎ ייתכן שיש עדכון — בדיקה", "⬆︎ قد يتوفر تحديث — تحقّق", "⬆︎ शायद नया अपडेट आया है — जाँचें", "⬆︎ 업데이트가 있을 수 있습니다 — 확인"],
    "donate": ["Support AURA Switcher ☕", "Підтримати AURA Switcher ☕", "Wesprzyj AURA Switcher ☕", "AURA Switcher unterstützen ☕", "Apoyar AURA Switcher ☕", "Soutenir AURA Switcher ☕", "Sostieni AURA Switcher ☕", "Apoiar o AURA Switcher ☕", "AURA Switcher'ı destekle ☕", "Steun AURA Switcher ☕", "Podpořit AURA Switcher ☕", "Stöd AURA Switcher ☕", "Tue AURA Switcheriä ☕", "Susține AURA Switcher ☕", "Az AURA Switcher támogatása ☕", "Υποστηρίξτε το AURA Switcher ☕", "Подкрепете AURA Switcher ☕", "תמכו ב-AURA Switcher ☕", "ادعم AURA Switcher ☕", "AURA Switcher का समर्थन करें ☕", "AURA Switcher 후원하기 ☕"],
    "quit": ["Quit", "Вихід", "Zakończ", "Beenden", "Salir", "Quitter", "Esci", "Sair", "Çık", "Stop", "Ukončit", "Avsluta", "Lopeta", "Ieșire", "Kilépés", "Έξοδος", "Изход", "יציאה", "إنهاء", "बंद करें", "종료"],
]
// END menu strings

func uiCode() -> String {
    if let l = engine.settings.uiLang, uiLangs.contains(l) { return l }
    let sys = String(Locale.preferredLanguages.first?.prefix(2) ?? "en")
    return sys == "ru" ? "uk" : uiLangs.contains(sys) ? sys : "en"
}

func L(_ key: String) -> String { uiStrings[key]?[uiLangs.firstIndex(of: uiCode()) ?? 0] ?? key }

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

    var shiftDownAt: TimeInterval = 0, lastShiftTap: TimeInterval = 0, sawOther = false
    var lastSelOut: String?, lastSelLang = 0

    var allCodes: Set<UInt16> { Set(layouts.flatMap { $0.toChar.keys.map(\.code) }) }
    lazy var codes = allCodes

    var active: Bool { settings.enabled && !frontIsGame && !settings.excluded.contains(frontBundle) }

    func reset() { word = []; lastWord = []; trailing = 0 }

    func layout(_ lang: String?) -> Layout? { layouts.first { $0.lang == lang } }
    func index(_ lang: String?) -> Int { layouts.firstIndex { $0.lang == lang } ?? 0 }
    func select(_ l: Layout) { TISSelectInputSource(l.source) }

    /// The first spell check in a language loads its dictionary (can take a moment). If that happens
    /// while the user types on, the correction is skipped — so load all dictionaries up front.
    func warmUpDictionaries() { for l in layouts { _ = isWord("aura", l.lang) } }

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
                var len = 0
                var buf = [UniChar](repeating: 0, count: 4)
                e.keyboardGetUnicodeString(maxStringLength: 4, actualStringLength: &len, unicodeString: &buf)
                let ch = len == 1 ? Unicode.Scalar(buf[0]).map(Character.init) : nil
                word.append(Key(code: code, shift: f.contains(.maskShift), caps: f.contains(.maskAlphaShift), ch: ch))
            } else { reset() }
        }
    }

    func autoCorrect(_ keys: [Key]) {
        // user kept typing meanwhile — skip
        guard lastWord == keys, word.isEmpty, trailing == 1, let fix = autoFix(keys) else { return }
        let out = fix.text(keys)
        let typed = typedLayout(keys)?.text(keys)
        // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
        if out != typed { replace(count: keys.count + 1, with: out + " ") }
        select(fix)
        Sounds.play(Sounds.auto)
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

    func autoFix(_ keys: [Key]) -> Layout? {
        guard keys.count >= 2, let cur = typedLayout(keys) else { return nil }
        let typed = cur.text(keys)
        if settings.words.contains(typed.lowercased()) || isWord(typed, cur.lang) { return nil }
        let start = index(cur.lang)
        for i in 1..<max(layouts.count, 1) {
            let l = layouts[(start + i) % layouts.count]
            let t = l.text(keys)
            // near-identical Latin layouts (QWERTY/QWERTZ/AZERTY): a typo there isn't a layout mistake.
            // Cyrillic UK/RU differ in few keys but that's exactly what users mix up, so they're allowed.
            let same = zip(t, typed).filter { $0 == $1 }.count
            if isLatin(t), isLatin(typed), same * 2 > keys.count { continue }
            if t.count == keys.count, isWord(t, l.lang) { return l }
        }
        return nil
    }

    // MARK: double shift

    func doubleShift() {
        Sounds.play(Sounds.shift)
        if let sel = axSelectedText(), !sel.isEmpty { convertSelection(sel); return }
        let keys = word.isEmpty ? lastWord : word
        if !keys.isEmpty { cycleWord(keys, trailing: word.isEmpty ? trailing : 0); return }
        clipboardSelection { sel in
            if let sel, !sel.isEmpty, !isCopiedLine(sel) { self.convertSelection(sel) } else { self.cycleLayout() }
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
        if text == lastSelOut, lastSelLang < layouts.count { srcIdx = lastSelLang } else {
            let cur = index(currentLang())
            let scores = layouts.map { l in text.filter { l.toKey[$0] != nil && $0.isLetter }.count }
            srcIdx = scores.indices.max { a, b in
                scores[a] != scores[b] ? scores[a] < scores[b] : (a != cur && b == cur)
            } ?? cur
        }
        let srcL = layouts[srcIdx], dstIdx = (srcIdx + 1) % layouts.count, dst = layouts[dstIdx]
        let out = String(text.map { c in srcL.toKey[c].flatMap { dst.toChar[$0] } ?? c })
        typeText(out)
        for _ in 0..<out.count { postKey(123, flags: .maskShift) } // reselect (⇧← moves by character)
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
                                                            object: nil, queue: .main) { _ in self.updateTitle() }
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
        if s.excluded.isEmpty { add(exMenu, L("empty")) }
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
        if s.bindings.isEmpty { add(bMenu, L("empty")) }
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
            NSWorkspace.shared.open(updatePageURL())
            self.updateTitle()
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
