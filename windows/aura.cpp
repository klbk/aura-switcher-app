// AURA Switcher for Windows — fixes text typed in the wrong keyboard layout.
// Made in Ukraine. Single file, Win32 only, no dependencies.
//
// Threads:
//   hook thread — low-level keyboard/mouse hooks; only bookkeeping, never blocks input
//   main thread — tray icon, menu, spell checking, text replacement

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <appmodel.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <objbase.h>
#include <spellcheck.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <uiautomation.h>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <ctime>
#include <map>
#include <set>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#ifndef APP_VERSION   // test builds: -DAPP_VERSION=L\"0.8.9\"
#define APP_VERSION L"0.10.2"
#endif
#ifndef BUILD_TIME
#define BUILD_TIME 0  // build.sh passes the build date (seconds since 1970)
#endif

using std::wstring;
using std::vector;

static const ULONG_PTR kMagic = 0x4B535754;  // marks our own synthetic input
enum { WM_TRAY = WM_APP + 1, WM_AUTO, WM_DSHIFT, WM_UPDATE, WM_UPDATE_READY, WM_ENTER, WM_EMPTYCHK };
enum { ID_ENABLED = 1, ID_AUTO, ID_SOUND, ID_WORDS, ID_DONATE, ID_REPORT, ID_UPDATE, ID_EXCLUDE, ID_NOBIND, ID_LOGIN, ID_UI_SYS, ID_QUIT,
       ID_BIND0 = 100, ID_HK0 = 600, ID_EXDEL0 = 200, ID_BDDEL0 = 300, ID_RECENT0 = 400, ID_UI0 = 500 };

// MARK: - Settings (HKCU\Software\AURA)

static const wchar_t* kRegKey = L"Software\\AURA";

static wstring regGet(const wchar_t* name, const wstring& def) {  // any length: ask the size first
    DWORD sz = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_SZ, nullptr, nullptr, &sz) != ERROR_SUCCESS || sz < sizeof(wchar_t)) return def;
    wstring buf(sz / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_SZ, nullptr, buf.data(), &sz) != ERROR_SUCCESS) return def;
    buf.resize(wcslen(buf.c_str()));
    return buf;
}
static void regSet(const wchar_t* name, const wstring& v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}
static vector<wstring> split(const wstring& s) {
    vector<wstring> r; size_t a = 0;
    while (a < s.size()) { size_t b = s.find(L';', a); if (b == wstring::npos) b = s.size(); if (b > a) r.push_back(s.substr(a, b - a)); a = b + 1; }
    return r;
}
static wstring join(const vector<wstring>& v) { wstring r; for (auto& s : v) r += s + L";"; return r; }

// the correction key (double tap, either side): VK_SHIFT (default), VK_CONTROL, VK_MENU (Alt) or 0 = off; read by the hook thread
static std::atomic<int> hkVk{VK_SHIFT};
static int hkFromName(const wstring& n) { return n == L"ctrl" ? VK_CONTROL : n == L"alt" ? VK_MENU : n == L"off" ? 0 : VK_SHIFT; }

struct Settings {
    bool enabled = regGet(L"enabled", L"1") == L"1";
    bool autoFix = regGet(L"auto", L"1") == L"1";
    bool sound = regGet(L"sound", L"1") == L"1";
    wstring hotkey = regGet(L"hotkey", L"shift");       // shift | ctrl | alt | off
    wstring uiLang = regGet(L"uiLang", L"");            // "" = system
    vector<wstring> excluded = split(regGet(L"excluded", L""));
    vector<wstring> words = split(regGet(L"words", L""));  // never auto-corrected («хз», «ща»), lowercased
    std::map<wstring, wstring> bindings;                // exe name → locale name
    Settings() { hkVk = hkFromName(hotkey); for (auto& p : split(regGet(L"bindings", L""))) { size_t e = p.find(L'='); if (e != wstring::npos) bindings[p.substr(0, e)] = p.substr(e + 1); } }
    void save() {
        hkVk = hkFromName(hotkey); regSet(L"hotkey", hotkey);
        regSet(L"enabled", enabled ? L"1" : L"0"); regSet(L"auto", autoFix ? L"1" : L"0"); regSet(L"sound", sound ? L"1" : L"0");
        regSet(L"uiLang", uiLang); regSet(L"excluded", join(excluded)); regSet(L"words", join(words));
        vector<wstring> b; for (auto& kv : bindings) b.push_back(kv.first + L"=" + kv.second); regSet(L"bindings", join(b));
    }
    bool isExcluded(const wstring& exe) { for (auto& e : excluded) if (!_wcsicmp(e.c_str(), exe.c_str())) return true; return false; }
    bool isUserWord(const wstring& w) { for (auto& x : words) if (!_wcsicmp(x.c_str(), w.c_str())) return true; return false; }
};
static Settings S;

// Updates: the app goes online only when the user clicks «Check for updates…» (update.json on klbk.github.io,
// the download from github.com); nothing typed and no personal data is ever sent. «What's new» opens the site's
// update page (only the version and menu language are in its address), and some time after this build (or
// after the last check) the menu and the tray icon remind that a newer version may be out.
static const long long kUpdateNudgeDays = 30;
static long long updateChecked = wcstoll(regGet(L"updateChecked", L"0").c_str(), nullptr, 10);  // last opened the releases page
// installed from the Microsoft Store (MSIX package): the Store updates it and starts it at login
static bool packaged() {   // GetCurrentPackageFullName is looked up at run time: it is absent before Windows 8
    static int r = -1;
    if (r < 0) {
        typedef LONG (WINAPI *PkgFn)(UINT32*, PWSTR);
        PkgFn f = (PkgFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetCurrentPackageFullName");
        UINT32 n = 0;
        r = f && f(&n, nullptr) != 15700 /* APPMODEL_ERROR_NO_PACKAGE */;
    }
    return r == 1;
}
static bool updateDue() {
    if (packaged()) return false;
    long long since = std::max<long long>(BUILD_TIME, updateChecked);
    return since > 0 && (long long)time(nullptr) - since > kUpdateNudgeDays * 86400;
}

static wstring lower(wstring s) { CharLowerBuffW(s.data(), DWORD(s.size())); return s; }

// "хз, ща яж" → {"хз", "ща", "яж"}
static vector<wstring> parseWords(const wstring& s) {
    vector<wstring> r; wstring cur;
    auto flush = [&] { if (!cur.empty()) { cur = lower(cur); bool dup = false; for (auto& x : r) dup |= x == cur; if (!dup) r.push_back(cur); cur.clear(); } };
    for (wchar_t c : s) { if (c == L',' || c == L';' || iswspace(c)) flush(); else cur += c; }
    flush();
    return r;
}

// MARK: - Localization (35 languages; Russian system → Ukrainian)

// BEGIN menu strings — generated by tools/menu_i18n.py, edit there
static const int UI_N = 48;
static const wchar_t* const UI_LANGS[UI_N] = {L"en", L"uk", L"pl", L"de", L"es", L"fr", L"it", L"pt", L"tr", L"nl", L"cs", L"sv", L"fi", L"ro", L"hu", L"el", L"bg", L"he", L"ar", L"hi", L"ko", L"be", L"kk", L"ky", L"ka", L"hy", L"az", L"sr", L"mk", L"sk", L"hr", L"sl", L"lt", L"lv", L"et", L"da", L"nb", L"is", L"sq", L"fa", L"vi", L"tg", L"mn", L"tt", L"uz", L"ga", L"mt", L"cy"};
static const wchar_t* const UI_NAMES[UI_N] = {L"English", L"Українська", L"Polski", L"Deutsch", L"Español", L"Français", L"Italiano", L"Português", L"Türkçe", L"Nederlands", L"Čeština", L"Svenska", L"Suomi", L"Română", L"Magyar", L"Ελληνικά", L"Български", L"עברית", L"العربية", L"हिन्दी", L"한국어", L"Беларуская", L"Қазақша", L"Кыргызча", L"ქართული", L"Հայերեն", L"Azərbaycan", L"Српски", L"Македонски", L"Slovenčina", L"Hrvatski", L"Slovenščina", L"Lietuvių", L"Latviešu", L"Eesti", L"Dansk", L"Norsk bokmål", L"Íslenska", L"Shqip", L"فارسی", L"Tiếng Việt", L"Тоҷикӣ", L"Монгол", L"Татарча", L"Oʻzbek", L"Gaeilge", L"Malti", L"Cymraeg"};
static const wchar_t* const UI_STR[][UI_N] = { // row: English first, then the other UI_LANGS
    {L"Enabled", L"Увімкнено", L"Włączone", L"Aktiviert", L"Activado", L"Activé", L"Attivo", L"Ativado", L"Etkin", L"Ingeschakeld", L"Zapnuto", L"Aktiverad", L"Käytössä", L"Activat", L"Bekapcsolva", L"Ενεργό", L"Включено", L"מופעל", L"مفعّل", L"चालू", L"켜기", L"Уключана", L"Қосулы", L"Күйгүзүлгөн", L"ჩართულია", L"Միացված է", L"Aktivdir", L"Укључено", L"Вклучено", L"Zapnuté", L"Uključeno", L"Vklopljeno", L"Įjungta", L"Ieslēgts", L"Sees", L"Aktiveret", L"På", L"Kveikt", L"Aktivizuar", L"روشن", L"Bật", L"Фаъол", L"Идэвхтэй", L"Кабызылган", L"Yoqilgan", L"Cumasaithe", L"Attivat", L"Wedi’i alluogi"},
    {L"Auto-correction", L"Автовиправлення", L"Autokorekta", L"Autokorrektur", L"Autocorrección", L"Correction automatique", L"Correzione automatica", L"Correção automática", L"Otomatik düzeltme", L"Autocorrectie", L"Automatické opravy", L"Autokorrigering", L"Automaattinen korjaus", L"Corectare automată", L"Automatikus javítás", L"Αυτόματη διόρθωση", L"Автокорекция", L"תיקון אוטומטי", L"التصحيح التلقائي", L"स्वतः सुधार", L"자동 수정", L"Аўтавыпраўленне", L"Автотүзету", L"Авто оңдоо", L"ავტოშესწორება", L"Ինքնաուղղում", L"Avtomatik düzəliş", L"Аутокорекција", L"Автокорекција", L"Automatické opravy", L"Automatski ispravak", L"Samodejni popravki", L"Automatinis taisymas", L"Automātiskā labošana", L"Automaatne parandus", L"Automatisk rettelse", L"Autokorrigering", L"Sjálfvirk leiðrétting", L"Korrigjim automatik", L"تصحیح خودکار", L"Tự động sửa", L"Ислоҳи худкор", L"Автомат засвар", L"Автоматик төзәтү", L"Avtomatik tuzatish", L"Ceartú uathoibríoch", L"Korrezzjoni awtomatika", L"Cywiro awtomatig"},
    {L"Sound", L"Звук", L"Dźwięk", L"Ton", L"Sonido", L"Son", L"Suono", L"Som", L"Ses", L"Geluid", L"Zvuk", L"Ljud", L"Ääni", L"Sunet", L"Hang", L"Ήχος", L"Звук", L"צליל", L"الصوت", L"ध्वनि", L"소리", L"Гук", L"Дыбыс", L"Үн", L"ხმა", L"Ձայն", L"Səs", L"Звук", L"Звук", L"Zvuk", L"Zvuk", L"Zvok", L"Garsas", L"Skaņa", L"Heli", L"Lyd", L"Lyd", L"Hljóð", L"Zëri", L"صدا", L"Âm thanh", L"Овоз", L"Дуу", L"Тавыш", L"Tovush", L"Fuaim", L"Ħoss", L"Sain"},
    {L"⚠︎ Grant access: Accessibility, Input Monitoring", L"⚠︎ Надайте доступ: Універсальний доступ, Моніторинг введення", L"⚠︎ Nadaj dostęp: Dostępność, Monitorowanie wejścia", L"⚠︎ Zugriff erlauben: Bedienungshilfen, Eingabeüberwachung", L"⚠︎ Conceder acceso: Accesibilidad, Monitorización de entrada", L"⚠︎ Autoriser l'accès : Accessibilité, Surveillance de l’entrée", L"⚠︎ Concedi l’accesso: Accessibilità, Monitoraggio input", L"⚠︎ Conceder acesso: Acessibilidade, Monitoramento de entrada", L"⚠︎ Erişim izni verin: Erişilebilirlik, Girdi İzleme", L"⚠︎ Geef toegang: Toegankelijkheid, Invoercontrole (Input Monitoring)", L"⚠︎ Udělte přístup: Zpřístupnění, Sledování vstupu", L"⚠︎ Ge åtkomst: Hjälpmedel, Indataövervakning (Input Monitoring)", L"⚠︎ Anna käyttöoikeus: Käyttöapu, Syötteen valvonta (Input Monitoring)", L"⚠︎ Acordați acces: Accesibilitate, Monitorizare intrare (Input Monitoring)", L"⚠︎ Hozzáférés megadása: Kisegítő lehetőségek, Bevitelfigyelés (Input Monitoring)", L"⚠︎ Δώστε πρόσβαση: Προσβασιμότητα, Παρακολούθηση εισόδου (Input Monitoring)", L"⚠︎ Дайте достъп: Достъпност, Input Monitoring", L"⚠︎ תנו גישה: נגישות, ניטור קלט (Input Monitoring)", L"⚠︎ امنح الوصول: تسهيلات الاستخدام، مراقبة الإدخال (Input Monitoring)", L"⚠︎ अनुमति दें: सुलभता, इनपुट मॉनिटरिंग (Input Monitoring)", L"⚠︎ 접근 권한 허용: 손쉬운 사용, 입력 모니터링", L"⚠︎ Дайце доступ: Спецыяльныя магчымасці, Input Monitoring", L"⚠︎ Рұқсат беріңіз: Арнайы мүмкіндіктер, Input Monitoring", L"⚠︎ Мүмкүнчүлүк бериңиз: Атайын мүмкүнчүлүктөр, Input Monitoring", L"⚠︎ მიეცით წვდომა: მისაწვდომობა, Input Monitoring", L"⚠︎ Տվեք թույլտվություն՝ Հասանելիություն, Input Monitoring", L"⚠︎ İcazə verin: Əlçatımlılıq, Input Monitoring", L"⚠︎ Дозволите приступ: Приступачност, Input Monitoring", L"⚠︎ Дозволете пристап: Пристапност, Input Monitoring", L"⚠︎ Povoľte prístup: Zjednodušenie ovládania, Monitorovanie vstupu (Input Monitoring)", L"⚠︎ Dopustite pristup: Pristupačnost, Praćenje unosa (Input Monitoring)", L"⚠︎ Dovolite dostop: Dostopnost, Input Monitoring", L"⚠︎ Suteikite prieigą: Pritaikymas neįgaliesiems, Input Monitoring", L"⚠︎ Atļaujiet piekļuvi: Pieejamība, Input Monitoring", L"⚠︎ Andke juurdepääs: Juurdepääsetavus, Input Monitoring", L"⚠︎ Giv adgang: Tilgængelighed, Inputovervågning (Input Monitoring)", L"⚠︎ Gi tilgang: Tilgjengelighet, Inndataovervåking (Input Monitoring)", L"⚠︎ Veittu aðgang: Algildur aðgangur, Input Monitoring", L"⚠︎ Jepni qasje: Përdorimi i lehtë, Input Monitoring", L"⚠︎ دسترسی بدهید: دسترسی‌پذیری، Input Monitoring", L"⚠︎ Cấp quyền: Trợ năng, Giám sát đầu vào (Input Monitoring)", L"⚠︎ Дастрасӣ диҳед: Дастрасии универсалӣ, Input Monitoring", L"⚠︎ Хандалт өгнө үү: Хүртээмж, Input Monitoring", L"⚠︎ Кереп җитү рөхсәт итегез: Махсус мөмкинлекләр, Input Monitoring", L"⚠︎ Ruxsat bering: Maxsus imkoniyatlar, Input Monitoring", L"⚠︎ Tabhair rochtain: Inrochtaineacht, Input Monitoring", L"⚠︎ Agħti aċċess: Aċċessibbiltà, Input Monitoring", L"⚠︎ Rhowch fynediad: Hygyrchedd, Input Monitoring"},
    {L"Current app: ", L"Поточна програма: ", L"Bieżąca aplikacja: ", L"Aktuelle App: ", L"App actual: ", L"App actuelle : ", L"App attuale: ", L"App atual: ", L"Geçerli uygulama: ", L"Huidige app: ", L"Aktuální aplikace: ", L"Aktuell app: ", L"Nykyinen ohjelma: ", L"Aplicația curentă: ", L"Jelenlegi alkalmazás: ", L"Τρέχουσα εφαρμογή: ", L"Текуща програма: ", L"יישום נוכחי: ", L"التطبيق الحالي: ", L"वर्तमान ऐप: ", L"현재 앱: ", L"Бягучая праграма: ", L"Ағымдағы қолданба: ", L"Учурдагы колдонмо: ", L"მიმდინარე აპლიკაცია: ", L"Ընթացիկ հավելված՝ ", L"Cari tətbiq: ", L"Тренутна апликација: ", L"Тековна апликација: ", L"Aktuálna aplikácia: ", L"Trenutna aplikacija: ", L"Trenutna aplikacija: ", L"Dabartinė programa: ", L"Pašreizējā lietotne: ", L"Praegune rakendus: ", L"Nuværende app: ", L"Gjeldende app: ", L"Núverandi forrit: ", L"Aplikacioni aktual: ", L"برنامه فعلی: ", L"Ứng dụng hiện tại: ", L"Барномаи ҷорӣ: ", L"Одоогийн апп: ", L"Хәзерге кушымта: ", L"Joriy ilova: ", L"Aip reatha: ", L"Applikazzjoni attwali: ", L"Ap cyfredol: "},
    {L"Don't work in this app", L"Не працювати в цій програмі", L"Nie działaj w tej aplikacji", L"In dieser App nicht aktiv", L"No funcionar en esta app", L"Ne pas fonctionner dans cette app", L"Disattiva in questa app", L"Não funcionar nesta app", L"Bu uygulamada çalışma", L"Niet werken in deze app", L"Nefungovat v této aplikaci", L"Arbeta inte i den här appen", L"Älä toimi tässä ohjelmassa", L"Nu funcționa în această aplicație", L"Ne működjön ebben az alkalmazásban", L"Να μη λειτουργεί σε αυτή την εφαρμογή", L"Да не работи в тази програма", L"לא לפעול ביישום זה", L"لا تعمل في هذا التطبيق", L"इस ऐप में काम न करें", L"이 앱에서 작동 안 함", L"Не працаваць у гэтай праграме", L"Бұл қолданбада жұмыс істемеу", L"Бул колдонмодо иштебөө", L"ამ აპლიკაციაში არ იმუშაოს", L"Չաշխատել այս հավելվածում", L"Bu tətbiqdə işləmə", L"Не радити у овој апликацији", L"Не работи во оваа апликација", L"V tejto aplikácii nepracovať", L"Ne radi u ovoj aplikaciji", L"V tej aplikaciji ne delajte", L"Šioje programoje nedirbti", L"Šajā lietotnē nedarboties", L"Selles rakenduses mitte töötada", L"Deaktiver i denne app", L"Ikke aktiv i denne appen", L"Ekki virkt í þessu forriti", L"Mos vepro në këtë aplikacion", L"در این برنامه کار نکن", L"Không hoạt động trong ứng dụng này", L"Дар ин барнома кор накунед", L"Энэ аппад ажиллахгүй", L"Бу кушымтада эшләмәскә", L"Bu ilovada ishlamasin", L"Ná oibrigh san aip seo", L"Taħdimx f’din l-applikazzjoni", L"Peidio â gweithio yn yr ap hwn"},
    {L"Game — AURA paused", L"Гра — AURA на паузі", L"Gra — AURA wstrzymana", L"Spiel — AURA pausiert", L"Juego — AURA en pausa", L"Jeu — AURA en pause", L"Gioco — AURA in pausa", L"Jogo — AURA em pausa", L"Oyun — AURA duraklatıldı", L"Game — AURA gepauzeerd", L"Hra — AURA pozastavena", L"Spel — AURA pausad", L"Peli — AURA tauolla", L"Joc — AURA în pauză", L"Játék — AURA szüneteltetve", L"Παιχνίδι — AURA σε παύση", L"Игра — AURA е на пауза", L"משחק — AURA מושהית", L"لعبة — AURA متوقف مؤقتًا", L"गेम — AURA रुका हुआ", L"게임 — AURA 일시 중지됨", L"Гульня — AURA на паўзе", L"Ойын — AURA кідіртілді", L"Оюн — AURA тыныгууда", L"თამაში — AURA დაპაუზებულია", L"Խաղ — AURA-ն դադարի վրա է", L"Oyun — AURA fasilədədir", L"Игра — AURA паузирана", L"Игра — AURA паузирана", L"Hra — AURA pozastavená", L"Igra — AURA pauzirana", L"Igra — AURA zaustavljena", L"Žaidimas — AURA pristabdyta", L"Spēle — AURA apturēta", L"Mäng — AURA peatatud", L"Spil — AURA på pause", L"Spill — AURA er satt på pause", L"Leikur — AURA í bið", L"Lojë — AURA në pauzë", L"بازی — AURA متوقف است", L"Trò chơi — AURA tạm dừng", L"Бозӣ — AURA таваққуф шудааст", L"Тоглоом — AURA түр зогссон", L"Уен — AURA туктатылган", L"Oʻyin — AURA pauzada", L"Cluiche — AURA ar sos", L"Logħba — AURA fil-pawża", L"Gêm — AURA wedi’i oedi"},
    {L"Language for this app", L"Мова для цієї програми", L"Język dla tej aplikacji", L"Sprache für diese App", L"Idioma para esta app", L"Langue pour cette app", L"Lingua per questa app", L"Idioma para esta app", L"Bu uygulama için dil", L"Taal voor deze app", L"Jazyk pro tuto aplikaci", L"Språk för den här appen", L"Kieli tälle ohjelmalle", L"Limba pentru această aplicație", L"Nyelv ehhez az alkalmazáshoz", L"Γλώσσα για αυτή την εφαρμογή", L"Език за тази програма", L"שפה ליישום זה", L"لغة هذا التطبيق", L"इस ऐप की भाषा", L"이 앱의 언어", L"Мова для гэтай праграмы", L"Осы қолданбаның тілі", L"Бул колдонмонун тили", L"ამ აპლიკაციის ენა", L"Այս հավելվածի լեզուն", L"Bu tətbiq üçün dil", L"Језик за ову апликацију", L"Јазик за оваа апликација", L"Jazyk pre túto aplikáciu", L"Jezik za ovu aplikaciju", L"Jezik za to aplikacijo", L"Kalba šiai programai", L"Valoda šai lietotnei", L"Selle rakenduse keel", L"Sprog for denne app", L"Språk for denne appen", L"Tungumál fyrir þetta forrit", L"Gjuha për këtë aplikacion", L"زبان این برنامه", L"Ngôn ngữ cho ứng dụng này", L"Забон барои ин барнома", L"Энэ аппын хэл", L"Бу кушымта өчен тел", L"Bu ilova uchun til", L"Teanga don aip seo", L"Lingwa għal din l-applikazzjoni", L"Iaith ar gyfer yr ap hwn"},
    {L"No binding", L"Без прив'язки", L"Bez przypisania", L"Keine Zuordnung", L"Sin asignación", L"Aucune association", L"Nessuna associazione", L"Sem associação", L"Atama yok", L"Geen koppeling", L"Bez přiřazení", L"Ingen koppling", L"Ei liitosta", L"Fără asociere", L"Nincs hozzárendelés", L"Χωρίς αντιστοίχιση", L"Без обвързване", L"ללא שיוך", L"بدون ربط", L"कोई बंधन नहीं", L"지정 안 함", L"Без прывязкі", L"Байланыссыз", L"Байланышсыз", L"მიბმის გარეშე", L"Առանց կապակցման", L"Bağlama yoxdur", L"Без повезивања", L"Без врзување", L"Bez priradenia", L"Bez povezivanja", L"Brez povezave", L"Be priskyrimo", L"Bez piesaistes", L"Seostamata", L"Ingen tilknytning", L"Ingen kobling", L"Engin tenging", L"Pa lidhje", L"بدون اتصال", L"Chưa gán", L"Бе пайвастшавӣ", L"Холбоогүй", L"Бәйләнешсез", L"Bogʻlanmagan", L"Gan nasc", L"L-ebda rabta", L"Dim cysylltiad"},
    {L"Exclusions", L"Винятки", L"Wyjątki", L"Ausnahmen", L"Excepciones", L"Exceptions", L"Esclusioni", L"Exceções", L"İstisnalar", L"Uitzonderingen", L"Výjimky", L"Undantag", L"Poikkeukset", L"Excepții", L"Kivételek", L"Εξαιρέσεις", L"Изключения", L"חריגים", L"الاستثناءات", L"अपवाद", L"예외", L"Выключэнні", L"Ерекшеліктер", L"Өзгөчөлүктөр", L"გამონაკლისები", L"Բացառություններ", L"İstisnalar", L"Изузеци", L"Исклучоци", L"Výnimky", L"Iznimke", L"Izjeme", L"Išimtys", L"Izņēmumi", L"Erandid", L"Undtagelser", L"Unntak", L"Undantekningar", L"Përjashtime", L"استثناها", L"Ngoại lệ", L"Истисноҳо", L"Үл хамаарах зүйлс", L"Аерымлыклар", L"Istisnolar", L"Eisceachtaí", L"Eċċezzjonijiet", L"Eithriadau"},
    {L"Don't work in (recent apps)", L"Не працювати в (останні програми)", L"Nie działaj w (ostatnie aplikacje)", L"Nicht aktiv in (letzte Apps)", L"No funcionar en (apps recientes)", L"Ne pas fonctionner dans (apps récentes)", L"Disattiva in (app recenti)", L"Não funcionar em (apps recentes)", L"Şurada çalışma (son uygulamalar)", L"Niet werken in (recente apps)", L"Nefungovat v (nedávné aplikace)", L"Arbeta inte i (senaste appar)", L"Älä toimi ohjelmassa (viimeisimmät)", L"Nu funcționa în (aplicații recente)", L"Ne működjön itt (legutóbbi alkalmazások)", L"Να μη λειτουργεί σε (πρόσφατες εφαρμογές)", L"Да не работи в (последни програми)", L"לא לפעול ב (יישומים אחרונים)", L"لا تعمل في (التطبيقات الأخيرة)", L"इनमें काम न करें (हाल के ऐप)", L"작동 안 함 (최근 앱)", L"Не працаваць у (нядаўнія праграмы)", L"Мында жұмыс істемеу (соңғы қолданбалар)", L"Төмөндө иштебөө (акыркы колдонмолор)", L"არ იმუშაოს (ბოლო აპლიკაციები)", L"Չաշխատել (վերջին հավելվածներ)", L"Burada işləmə (son tətbiqlər)", L"Не радити у (недавне апликације)", L"Не работи во (неодамнешни апликации)", L"Nepracovať v (nedávne aplikácie)", L"Ne radi u (nedavne aplikacije)", L"Ne delajte v (nedavne aplikacije)", L"Nedirbti (naujausios programos)", L"Nedarboties (nesenās lietotnes)", L"Mitte töötada (hiljutised rakendused)", L"Deaktiver i (seneste apps)", L"Ikke aktiv i (nylige apper)", L"Ekki virkt í (nýleg forrit)", L"Mos vepro në (aplikacionet e fundit)", L"در این‌ها کار نکن (برنامه‌های اخیر)", L"Không hoạt động trong (ứng dụng gần đây)", L"Кор накунед дар (барномаҳои охирин)", L"Ажиллахгүй (сүүлийн аппууд)", L"Эшләмәскә (соңгы кушымталар)", L"Ishlamasin (soʻnggi ilovalar)", L"Ná oibrigh i (aipeanna le déanaí)", L"Taħdimx fi (applikazzjonijiet reċenti)", L"Peidio â gweithio yn (apiau diweddar)"},
    {L"Language bindings", L"Прив'язки мов", L"Przypisania języków", L"Sprachzuordnungen", L"Idiomas asignados", L"Langues associées", L"Lingue associate", L"Idiomas associados", L"Dil atamaları", L"Taalkoppelingen", L"Přiřazení jazyků", L"Språkkopplingar", L"Kieliliitokset", L"Limbi asociate", L"Nyelvi hozzárendelések", L"Αντιστοιχίσεις γλωσσών", L"Обвързани езици", L"שיוכי שפה", L"ربط اللغات", L"भाषा बंधन", L"언어 지정", L"Прывязкі моў", L"Тіл байланыстары", L"Тил байланыштары", L"ენების მიბმა", L"Լեզուների կապակցումներ", L"Dil bağlamaları", L"Повезивање језика", L"Врзувања на јазици", L"Priradenia jazykov", L"Povezivanja jezika", L"Povezave jezikov", L"Kalbų priskyrimai", L"Valodu piesaistes", L"Keeleseosed", L"Sprogtilknytninger", L"Språkkoblinger", L"Tungumálatengingar", L"Lidhjet e gjuhëve", L"اتصال زبان‌ها", L"Gán ngôn ngữ", L"Пайвасткунии забонҳо", L"Хэлний холболтууд", L"Телләр бәйләнеше", L"Til bogʻlanishlari", L"Naisc teanga", L"Rabtiet tal-lingwi", L"Cysylltiadau iaith"},
    {L"Empty", L"Порожньо", L"Pusto", L"Leer", L"Vacío", L"Vide", L"Vuoto", L"Vazio", L"Boş", L"Leeg", L"Prázdné", L"Tomt", L"Tyhjä", L"Gol", L"Üres", L"Κενό", L"Празно", L"ריק", L"فارغ", L"खाली", L"비어 있음", L"Пуста", L"Бос", L"Бош", L"ცარიელია", L"Դատարկ է", L"Boşdur", L"Празно", L"Празно", L"Prázdne", L"Prazno", L"Prazno", L"Tuščia", L"Tukšs", L"Tühi", L"Tom", L"Tom", L"Tómt", L"Bosh", L"خالی", L"Trống", L"Холӣ", L"Хоосон", L"Буш", L"Boʻsh", L"Folamh", L"Vojt", L"Gwag"},
    {L"Launch at login", L"Запускати під час входу", L"Uruchamiaj przy logowaniu", L"Bei Anmeldung starten", L"Abrir al iniciar sesión", L"Ouvrir à la connexion", L"Apri al login", L"Abrir ao iniciar sessão", L"Oturum açılışında başlat", L"Starten bij inloggen", L"Spouštět po přihlášení", L"Starta vid inloggning", L"Käynnistä kirjautuessa", L"Pornește la autentificare", L"Indítás bejelentkezéskor", L"Εκκίνηση κατά τη σύνδεση", L"Стартиране при вход", L"הפעלה בעת התחברות", L"التشغيل عند تسجيل الدخول", L"लॉगिन पर शुरू करें", L"로그인 시 실행", L"Запускаць пры ўваходзе", L"Кіргенде іске қосу", L"Кирүүдө иштетүү", L"გაშვება შესვლისას", L"Գործարկել մուտքի ժամանակ", L"Girişdə başlat", L"Покрени при пријави", L"Стартувај при најава", L"Spúšťať po prihlásení", L"Pokreni pri prijavi", L"Zaženi ob prijavi", L"Paleisti prisijungiant", L"Palaist pieteikšanās brīdī", L"Käivita sisselogimisel", L"Start ved login", L"Start ved innlogging", L"Ræsa við innskráningu", L"Hap në hyrje", L"اجرا هنگام ورود", L"Chạy khi đăng nhập", L"Оғоз ҳангоми воридшавӣ", L"Нэвтрэхэд эхлүүлэх", L"Керүдә эшләтеп җибәрергә", L"Kirishda ishga tushirish", L"Seol ag logáil isteach", L"Ibda mad-dħul", L"Cychwyn wrth fewngofnodi"},
    {L"My words…", L"Мої слова…", L"Moje słowa…", L"Meine Wörter…", L"Mis palabras…", L"Mes mots…", L"Le mie parole…", L"As minhas palavras…", L"Kelimelerim…", L"Mijn woorden…", L"Moje slova…", L"Mina ord…", L"Omat sanat…", L"Cuvintele mele…", L"Saját szavak…", L"Οι λέξεις μου…", L"Моите думи…", L"המילים שלי…", L"كلماتي…", L"मेरे शब्द…", L"내 단어…", L"Мае словы…", L"Менің сөздерім…", L"Менин сөздөрүм…", L"ჩემი სიტყვები…", L"Իմ բառերը…", L"Mənim sözlərim…", L"Моје речи…", L"Моите зборови…", L"Moje slová…", L"Moje riječi…", L"Moje besede…", L"Mano žodžiai…", L"Mani vārdi…", L"Minu sõnad…", L"Mine ord…", L"Mine ord…", L"Mín orð…", L"Fjalët e mia…", L"واژه‌های من…", L"Từ của tôi…", L"Калимаҳои ман…", L"Миний үгс…", L"Минем сүзләр…", L"Mening soʻzlarim…", L"Mo chuid focal…", L"Kliemi…", L"Fy ngeiriau…"},
    {L"AURA Switcher — my words", L"AURA Switcher — мої слова", L"AURA Switcher — moje słowa", L"AURA Switcher — meine Wörter", L"AURA Switcher — mis palabras", L"AURA Switcher — mes mots", L"AURA Switcher — le mie parole", L"AURA Switcher — as minhas palavras", L"AURA Switcher — kelimelerim", L"AURA Switcher — mijn woorden", L"AURA Switcher — moje slova", L"AURA Switcher — mina ord", L"AURA Switcher — omat sanat", L"AURA Switcher — cuvintele mele", L"AURA Switcher — saját szavak", L"AURA Switcher — οι λέξεις μου", L"AURA Switcher — моите думи", L"AURA Switcher — המילים שלי", L"AURA Switcher — كلماتي", L"AURA Switcher — मेरे शब्द", L"AURA Switcher — 내 단어", L"AURA Switcher — мае словы", L"AURA Switcher — менің сөздерім", L"AURA Switcher — менин сөздөрүм", L"AURA Switcher — ჩემი სიტყვები", L"AURA Switcher — իմ բառերը", L"AURA Switcher — mənim sözlərim", L"AURA Switcher — моје речи", L"AURA Switcher — моите зборови", L"AURA Switcher — moje slová", L"AURA Switcher — moje riječi", L"AURA Switcher — moje besede", L"AURA Switcher — mano žodžiai", L"AURA Switcher — mani vārdi", L"AURA Switcher — minu sõnad", L"AURA Switcher — mine ord", L"AURA Switcher — mine ord", L"AURA Switcher — mín orð", L"AURA Switcher — fjalët e mia", L"AURA Switcher — واژه‌های من", L"AURA Switcher — từ của tôi", L"AURA Switcher — калимаҳои ман", L"AURA Switcher — миний үгс", L"AURA Switcher — минем сүзләр", L"AURA Switcher — mening soʻzlarim", L"AURA Switcher — mo chuid focal", L"AURA Switcher — kliemi", L"AURA Switcher — fy ngeiriau"},
    {L"AURA Switcher treats these words as correct: it never changes them, and fixes them when typed in the wrong layout. Separate them with commas.", L"Ці слова AURA Switcher вважає правильними: не змінює їх, а набрані не в тій розкладці — виправляє на них. Через кому.", L"AURA Switcher uznaje te słowa za poprawne: nigdy ich nie zmienia, a wpisane w złym układzie poprawia na nie. Oddziel je przecinkami.", L"AURA Switcher hält diese Wörter für richtig: ändert sie nie und korrigiert sie, wenn sie im falschen Layout getippt wurden. Durch Kommas trennen.", L"AURA Switcher considera correctas estas palabras: nunca las cambia y las corrige si se escriben con la distribución equivocada. Sepáralas con comas.", L"AURA Switcher considère ces mots comme corrects : il ne les modifie jamais et les corrige s'ils sont tapés avec la mauvaise disposition. Séparez-les par des virgules.", L"AURA Switcher considera corrette queste parole: non le modifica mai e le corregge se le digiti con il layout sbagliato. Separale con una virgola.", L"O AURA Switcher considera estas palavras corretas: nunca as altera e corrige-as se forem digitadas no layout errado. Separe-as com vírgulas.", L"AURA Switcher bu kelimeleri doğru kabul eder: asla değiştirmez, yanlış düzende yazılırsa onlara düzeltir. Virgülle ayırın.", L"AURA Switcher beschouwt deze woorden als juist: verandert ze nooit en verbetert ze als ze in de verkeerde indeling zijn getypt. Scheid ze met komma's.", L"AURA Switcher považuje tato slova za správná: nikdy je nemění a opraví je, když jsou napsána ve špatném rozložení. Oddělte je čárkami.", L"AURA Switcher ser dessa ord som rätt: ändrar dem aldrig och rättar dem om de skrivs med fel layout. Separera med kommatecken.", L"AURA Switcher pitää näitä sanoja oikeina: ei koskaan muuta niitä ja korjaa ne, jos ne on kirjoitettu väärällä asettelulla. Erota pilkuilla.", L"AURA Switcher consideră aceste cuvinte corecte: nu le schimbă niciodată și le corectează dacă sunt tastate în aranjamentul greșit. Separați-le prin virgulă.", L"Az AURA Switcher ezeket a szavakat helyesnek tekinti: soha nem módosítja őket, és kijavítja, ha rossz kiosztással gépelték. Vesszővel válassza el.", L"Το AURA Switcher θεωρεί σωστές αυτές τις λέξεις: δεν τις αλλάζει ποτέ και τις διορθώνει όταν πληκτρολογηθούν με λάθος διάταξη. Χωρίστε τις με κόμμα.", L"AURA Switcher смята тези думи за правилни: никога не ги променя и ги поправя, ако са написани с грешната подредба. Разделете ги със запетая.", L"AURA Switcher מתייחס למילים אלה כנכונות: לעולם לא משנה אותן, ומתקן אליהן כשהן מוקלדות בפריסה הלא נכונה. הפרידו בפסיקים.", L"يعدّ AURA Switcher هذه الكلمات صحيحة: لا يغيّرها أبدًا، ويصحّحها إليها إذا كُتبت بالتخطيط الخطأ. افصل بينها بفواصل.", L"AURA Switcher इन शब्दों को सही मानता है: इन्हें कभी नहीं बदलता, और गलत लेआउट में टाइप होने पर इन्हीं में सुधार देता है। अल्पविराम से अलग करें।", L"AURA Switcher는 이 단어들을 올바른 단어로 봅니다. 절대 바꾸지 않고, 잘못된 자판으로 입력되면 이 단어로 고칩니다. 쉼표로 구분하세요.", L"AURA Switcher лічыць гэтыя словы правільнымі: не мяняе іх, а набраныя ў няправільнай раскладцы — выпраўляе на іх. Праз коску.", L"AURA Switcher бұл сөздерді дұрыс деп санайды: оларды өзгертпейді, ал қате пернетақта орналасуымен терілсе, осыларға түзетеді. Үтірмен бөліңіз.", L"AURA Switcher бул сөздөрдү туура деп эсептейт: аларды өзгөртпөйт, ал эми клавиатуранын туура эмес жайгашуусунда терилсе, ушуларга оңдойт. Үтүр менен бөлүңүз.", L"AURA Switcher ამ სიტყვებს სწორად მიიჩნევს: არ ცვლის მათ, ხოლო არასწორი განლაგებით აკრეფილს ამათ ასწორებს. გამოყავით მძიმეებით.", L"AURA Switcher-ն այս բառերը ճիշտ է համարում՝ չի փոխում դրանք, իսկ սխալ դասավորությամբ տպվածները ուղղում է դրանց։ Բաժանեք ստորակետերով։", L"AURA Switcher bu sözləri düzgün sayır: onları dəyişmir, səhv düzülüşdə yığılanda isə bunlara düzəldir. Vergüllə ayırın.", L"AURA Switcher сматра ове речи исправним: не мења их, а када су откуцане у погрешном распореду — исправља их на ове. Одвојите зарезима.", L"AURA Switcher ги смета овие зборови за точни: не ги менува, а кога се напишани во погрешен распоред — ги исправа на нив. Одвојте ги со запирки.", L"AURA Switcher považuje tieto slová za správne: nikdy ich nemení a opravuje ich, keď sú napísané v nesprávnom rozložení. Oddeľte ich čiarkami.", L"AURA Switcher ove riječi smatra ispravnima: nikad ih ne mijenja, a ispravlja ih kad su utipkane u pogrešnom rasporedu. Odvojite ih zarezima.", L"AURA Switcher te besede šteje za pravilne: nikoli jih ne spremeni, popravi pa jih, če so natipkane v napačni razporeditvi. Ločite jih z vejicami.", L"AURA Switcher laiko šiuos žodžius teisingais: niekada jų nekeičia, o surinktus netinkamu išdėstymu — pataiso į juos. Atskirkite kableliais.", L"AURA Switcher uzskata šos vārdus par pareiziem: nekad tos nemaina un labo uz tiem, ja tie ievadīti nepareizā izkārtojumā. Atdaliet ar komatiem.", L"AURA Switcher peab neid sõnu õigeks: ei muuda neid kunagi ja parandab nendeks, kui need on sisestatud vales paigutuses. Eralda komadega.", L"AURA Switcher regner disse ord for korrekte: ændrer dem aldrig og retter til dem, når de er skrevet i forkert tastaturlayout. Adskil med komma.", L"AURA Switcher regner disse ordene som riktige: endrer dem aldri og retter til dem når de er skrevet med feil tastaturoppsett. Skill med komma.", L"AURA Switcher telur þessi orð rétt: breytir þeim aldrei og lagfærir í þau þegar þau eru rituð með röngu lyklaborðsskipulagi. Aðskildu með kommum.", L"AURA Switcher i konsideron këto fjalë të sakta: nuk i ndryshon kurrë dhe i korrigjon në to kur shkruhen me paraqitje të gabuar të tastierës. Ndajini me presje.", L"AURA Switcher این واژه‌ها را درست می‌داند: هرگز تغییرشان نمی‌دهد و اگر با چیدمان اشتباه تایپ شوند، به آن‌ها اصلاحشان می‌کند. با ویرگول جدا کنید.", L"AURA Switcher coi những từ này là đúng: không bao giờ đổi chúng và sửa thành chúng khi gõ sai bố cục bàn phím. Cách nhau bằng dấu phẩy.", L"AURA Switcher ин калимаҳоро дуруст мешуморад: онҳоро ҳаргиз тағйир намедиҳад ва агар бо тарҳбандии нодурусти клавиатура навишта шаванд, ба онҳо ислоҳ мекунад. Бо вергул ҷудо кунед.", L"AURA Switcher эдгээр үгсийг зөв гэж үздэг: хэзээ ч өөрчлөхгүй, буруу гарын байрлалаар бичигдсэн бол тэдгээр рүү засна. Таслалаар тусгаарлана уу.", L"AURA Switcher бу сүзләрне дөрес дип саный: аларны беркайчан да үзгәртми, ә дөрес булмаган клавиатура рәвешендә язылса, алар итеп төзәтә. Өтер белән аерыгыз.", L"AURA Switcher bu soʻzlarni toʻgʻri deb hisoblaydi: ularni hech qachon oʻzgartirmaydi, notoʻgʻri klaviatura terilishida yozilsa, ularga tuzatadi. Vergul bilan ajrating.", L"Measann AURA Switcher go bhfuil na focail seo ceart: ní athraíonn sé iad choíche, agus ceartaíonn sé chucu iad nuair a chlóscríobhtar le leagan amach mícheart iad. Deighil le camóga.", L"AURA Switcher iqis dan il-kliem bħala korrett: ma jbiddlu qatt, u jikkoreġi għalih meta jinkiteb bi tqassim żbaljat tat-tastiera. Aqsamhom b’virgola.", L"Mae AURA Switcher yn ystyried bod y geiriau hyn yn gywir: nid yw byth yn eu newid, ac mae’n cywiro iddyn nhw pan fyddan nhw wedi’u teipio gyda’r cynllun bysellfwrdd anghywir. Gwahanwch â choma."},
    {L"Save", L"Зберегти", L"Zapisz", L"Sichern", L"Guardar", L"Enregistrer", L"Salva", L"Guardar", L"Kaydet", L"Bewaar", L"Uložit", L"Spara", L"Tallenna", L"Salvează", L"Mentés", L"Αποθήκευση", L"Запази", L"שמירה", L"حفظ", L"सहेजें", L"저장", L"Захаваць", L"Сақтау", L"Сактоо", L"შენახვა", L"Պահպանել", L"Saxla", L"Сачувај", L"Зачувај", L"Uložiť", L"Spremi", L"Shrani", L"Išsaugoti", L"Saglabāt", L"Salvesta", L"Gem", L"Lagre", L"Vista", L"Ruaj", L"ذخیره", L"Lưu", L"Захира кардан", L"Хадгалах", L"Саклау", L"Saqlash", L"Sábháil", L"Salva", L"Cadw"},
    {L"Cancel", L"Скасувати", L"Anuluj", L"Abbrechen", L"Cancelar", L"Annuler", L"Annulla", L"Cancelar", L"İptal", L"Annuleer", L"Zrušit", L"Avbryt", L"Kumoa", L"Anulează", L"Mégse", L"Ακύρωση", L"Отказ", L"ביטול", L"إلغاء", L"रद्द करें", L"취소", L"Скасаваць", L"Бас тарту", L"Жокко чыгаруу", L"გაუქმება", L"Չեղարկել", L"Ləğv et", L"Откажи", L"Откажи", L"Zrušiť", L"Odustani", L"Prekliči", L"Atšaukti", L"Atcelt", L"Tühista", L"Annuller", L"Avbryt", L"Hætta við", L"Anulo", L"لغو", L"Hủy", L"Бекор кардан", L"Цуцлах", L"Баш тарту", L"Bekor qilish", L"Cealaigh", L"Ikkanċella", L"Diddymu"},
    {L"Menu language", L"Мова меню", L"Język menu", L"Menüsprache", L"Idioma del menú", L"Langue du menu", L"Lingua del menu", L"Idioma do menu", L"Menü dili", L"Menutaal", L"Jazyk nabídky", L"Menyspråk", L"Valikon kieli", L"Limba meniului", L"Menü nyelve", L"Γλώσσα μενού", L"Език на менюто", L"שפת התפריט", L"لغة القائمة", L"मेनू की भाषा", L"메뉴 언어", L"Мова меню", L"Мәзір тілі", L"Меню тили", L"მენიუს ენა", L"Ցանկի լեզուն", L"Menyu dili", L"Језик менија", L"Јазик на менито", L"Jazyk ponuky", L"Jezik izbornika", L"Jezik menija", L"Meniu kalba", L"Izvēlnes valoda", L"Menüü keel", L"Menusprog", L"Menyspråk", L"Tungumál valmyndar", L"Gjuha e menysë", L"زبان منو", L"Ngôn ngữ menu", L"Забони меню", L"Цэсний хэл", L"Меню теле", L"Menyu tili", L"Teanga an roghchláir", L"Lingwa tal-menu", L"Iaith y ddewislen"},
    {L"All languages", L"Усі мови", L"Wszystkie języki", L"Alle Sprachen", L"Todos los idiomas", L"Toutes les langues", L"Tutte le lingue", L"Todos os idiomas", L"Tüm diller", L"Alle talen", L"Všechny jazyky", L"Alla språk", L"Kaikki kielet", L"Toate limbile", L"Minden nyelv", L"Όλες οι γλώσσες", L"Всички езици", L"כל השפות", L"كل اللغات", L"सभी भाषाएँ", L"모든 언어", L"Усе мовы", L"Барлық тілдер", L"Бардык тилдер", L"ყველა ენა", L"Բոլոր լեզուները", L"Bütün dillər", L"Сви језици", L"Сите јазици", L"Všetky jazyky", L"Svi jezici", L"Vsi jeziki", L"Visos kalbos", L"Visas valodas", L"Kõik keeled", L"Alle sprog", L"Alle språk", L"Öll tungumál", L"Të gjitha gjuhët", L"همه زبان‌ها", L"Tất cả ngôn ngữ", L"Ҳамаи забонҳо", L"Бүх хэл", L"Барлык телләр", L"Barcha tillar", L"Gach teanga", L"Il-lingwi kollha", L"Pob iaith"},
    {L"Europe", L"Європа", L"Europa", L"Europa", L"Europa", L"Europe", L"Europa", L"Europa", L"Avrupa", L"Europa", L"Evropa", L"Europa", L"Eurooppa", L"Europa", L"Európa", L"Ευρώπη", L"Европа", L"אירופה", L"أوروبا", L"यूरोप", L"유럽", L"Еўропа", L"Еуропа", L"Европа", L"ევროპა", L"Եվրոպա", L"Avropa", L"Европа", L"Европа", L"Európa", L"Europa", L"Evropa", L"Europa", L"Eiropa", L"Euroopa", L"Europa", L"Europa", L"Evrópa", L"Evropa", L"اروپا", L"Châu Âu", L"Аврупо", L"Европ", L"Европа", L"Yevropa", L"An Eoraip", L"Ewropa", L"Ewrop"},
    {L"Caucasus & Central Asia", L"Кавказ і Центральна Азія", L"Kaukaz i Azja Środkowa", L"Kaukasus und Zentralasien", L"Cáucaso y Asia Central", L"Caucase et Asie centrale", L"Caucaso e Asia centrale", L"Cáucaso e Ásia Central", L"Kafkasya ve Orta Asya", L"Kaukasus en Centraal-Azië", L"Kavkaz a Střední Asie", L"Kaukasus och Centralasien", L"Kaukasia ja Keski-Aasia", L"Caucaz și Asia Centrală", L"Kaukázus és Közép-Ázsia", L"Καύκασος και Κεντρική Ασία", L"Кавказ и Централна Азия", L"הקווקז ומרכז אסיה", L"القوقاز وآسيا الوسطى", L"काकेशस और मध्य एशिया", L"캅카스·중앙아시아", L"Каўказ і Цэнтральная Азія", L"Кавказ және Орталық Азия", L"Кавказ жана Борбордук Азия", L"კავკასია და ცენტრალური აზია", L"Կովկաս և Կենտրոնական Ասիա", L"Qafqaz və Mərkəzi Asiya", L"Кавказ и Централна Азија", L"Кавказ и Централна Азија", L"Kaukaz a Stredná Ázia", L"Kavkaz i Srednja Azija", L"Kavkaz in Srednja Azija", L"Kaukazas ir Vidurio Azija", L"Kaukāzs un Centrālāzija", L"Kaukaasia ja Kesk-Aasia", L"Kaukasus og Centralasien", L"Kaukasus og Sentral-Asia", L"Kákasus og Mið-Asía", L"Kaukazi dhe Azia Qendrore", L"قفقاز و آسیای مرکزی", L"Kavkaz và Trung Á", L"Қафқоз ва Осиёи Марказӣ", L"Кавказ ба Төв Ази", L"Кавказ һәм Үзәк Азия", L"Kavkaz va Markaziy Osiyo", L"An Chugais agus an Áise Láir", L"Il-Kawkasu u l-Asja Ċentrali", L"Y Cawcasws a Chanolbarth Asia"},
    {L"Middle East", L"Близький Схід", L"Bliski Wschód", L"Naher Osten", L"Oriente Medio", L"Moyen-Orient", L"Medio Oriente", L"Médio Oriente", L"Orta Doğu", L"Midden-Oosten", L"Blízký východ", L"Mellanöstern", L"Lähi-itä", L"Orientul Mijlociu", L"Közel-Kelet", L"Μέση Ανατολή", L"Близкия изток", L"המזרח התיכון", L"الشرق الأوسط", L"मध्य पूर्व", L"중동", L"Блізкі Усход", L"Таяу Шығыс", L"Жакынкы Чыгыш", L"ახლო აღმოსავლეთი", L"Մերձավոր Արևելք", L"Yaxın Şərq", L"Блиски исток", L"Блиски Исток", L"Blízky východ", L"Bliski istok", L"Bližnji vzhod", L"Artimieji Rytai", L"Tuvie Austrumi", L"Lähis-Ida", L"Mellemøsten", L"Midtøsten", L"Mið-Austurlönd", L"Lindja e Mesme", L"خاورمیانه", L"Trung Đông", L"Шарқи Наздик", L"Ойрхи Дорнод", L"Якын Көнчыгыш", L"Yaqin Sharq", L"An Mheánoirthear", L"Il-Lvant Nofsani", L"Y Dwyrain Canol"},
    {L"Asia", L"Азія", L"Azja", L"Asien", L"Asia", L"Asie", L"Asia", L"Ásia", L"Asya", L"Azië", L"Asie", L"Asien", L"Aasia", L"Asia", L"Ázsia", L"Ασία", L"Азия", L"אסיה", L"آسيا", L"एशिया", L"아시아", L"Азія", L"Азия", L"Азия", L"აზია", L"Ասիա", L"Asiya", L"Азија", L"Азија", L"Ázia", L"Azija", L"Azija", L"Azija", L"Āzija", L"Aasia", L"Asien", L"Asia", L"Asía", L"Azia", L"آسیا", L"Châu Á", L"Осиё", L"Ази", L"Азия", L"Osiyo", L"An Áise", L"L-Asja", L"Asia"},
    {L"System", L"Системна", L"Systemowy", L"System", L"Sistema", L"Système", L"Sistema", L"Sistema", L"Sistem", L"Systeem", L"Systémový", L"System", L"Järjestelmä", L"Sistem", L"Rendszer", L"Συστήματος", L"Системен", L"מערכת", L"النظام", L"सिस्टम", L"시스템", L"Сістэмная", L"Жүйелік", L"Системалык", L"სისტემური", L"Համակարգային", L"Sistem", L"Системски", L"Системски", L"Systémový", L"Sustavski", L"Sistemski", L"Sistemos", L"Sistēmas", L"Süsteemne", L"System", L"System", L"Kerfið", L"Sistemi", L"سیستم", L"Hệ thống", L"Система", L"Систем", L"Система", L"Tizim", L"Córas", L"Sistema", L"System"},
    {L"Double ⇧ — convert word/selection", L"Подвійний ⇧ — змінити мову слова/виділення", L"Podwójny ⇧ — zmień układ słowa/zaznaczenia", L"Doppel-⇧ — Wort/Auswahl umwandeln", L"Doble ⇧ — convertir palabra/selección", L"Double ⇧ — convertir le mot/la sélection", L"Doppio ⇧ — converti parola o selezione", L"Duplo ⇧ — converter palavra/seleção", L"Çift ⇧ — kelimeyi/seçimi dönüştür", L"Dubbel ⇧ — woord/selectie omzetten", L"Dvojitý ⇧ — převést slovo/výběr", L"Dubbel-⇧ — konvertera ord/markering", L"Kaksois-⇧ — muunna sana/valinta", L"Dublu ⇧ — convertește cuvântul/selecția", L"Dupla ⇧ — szó/kijelölés átalakítása", L"Διπλό ⇧ — μετατροπή λέξης/επιλογής", L"Двоен ⇧ — смяна на езика на дума/селекция", L"⇧ כפול — המרת מילה/בחירה", L"⇧ مزدوج — تحويل الكلمة/التحديد", L"दो बार ⇧ — शब्द/चयन बदलें", L"⇧ 두 번 — 단어/선택 영역 변환", L"Падвойны ⇧ — змяніць мову слова/вылучэння", L"Қос ⇧ — сөзді/таңдалғанды түрлендіру", L"Кош ⇧ — сөздү/тандалганды өзгөртүү", L"ორმაგი ⇧ — სიტყვის/მონიშნულის გადაყვანა", L"Կրկնակի ⇧ — փոխարկել բառը/նշվածը", L"Qoşa ⇧ — sözü/seçilmişi çevir", L"Двоструки ⇧ — претвори реч/избор", L"Двоен ⇧ — претвори збор/избор", L"Dvojitý ⇧ — previesť slovo/výber", L"Dvostruki ⇧ — pretvori riječ/odabir", L"Dvojni ⇧ — pretvori besedo/izbor", L"Dvigubas ⇧ — konvertuoti žodį/pažymėtą tekstą", L"Dubults ⇧ — pārvērst vārdu/atlasi", L"Topelt ⇧ — teisenda sõna/valik", L"Dobbelt ⇧ — konvertér ord/markering", L"Dobbel ⇧ — konverter ord/markering", L"Tvöfalt ⇧ — umbreyta orði/vali", L"Dyfishi ⇧ — konverto fjalën/përzgjedhjen", L"دوبار ⇧ — تبدیل واژه/انتخاب", L"Nhấn đôi ⇧ — chuyển đổi từ/vùng chọn", L"Дукарата ⇧ — табдили калима/интихоб", L"Давхар ⇧ — үг/сонголтыг хөрвүүлэх", L"Икеләтә ⇧ — сүзне/сайланганны әйләндерү", L"Ikki marta ⇧ — soʻz/belgilashni aylantirish", L"⇧ faoi dhó — tiontaigh focal/roghnúchán", L"⇧ doppju — ikkonverti kelma/għażla", L"⇧ dwbl — trosi gair/dewisiad"},
    {L"Report a problem…", L"Повідомити про проблему…", L"Zgłoś problem…", L"Problem melden…", L"Informar de un problema…", L"Signaler un problème…", L"Segnala un problema…", L"Comunicar um problema…", L"Sorun bildir…", L"Probleem melden…", L"Nahlásit problém…", L"Rapportera ett problem…", L"Ilmoita ongelmasta…", L"Raportează o problemă…", L"Probléma bejelentése…", L"Αναφορά προβλήματος…", L"Съобщете за проблем…", L"דיווח על בעיה…", L"الإبلاغ عن مشكلة…", L"समस्या बताएँ…", L"문제 신고…", L"Паведаміць пра праблему…", L"Мәселе туралы хабарлау…", L"Көйгөй тууралуу кабарлоо…", L"პრობლემის შეტყობინება…", L"Հաղորդել խնդրի մասին…", L"Problem barədə bildir…", L"Пријави проблем…", L"Пријави проблем…", L"Nahlásiť problém…", L"Prijavi problem…", L"Prijavi težavo…", L"Pranešti apie problemą…", L"Ziņot par problēmu…", L"Teata probleemist…", L"Rapportér et problem…", L"Rapporter et problem…", L"Tilkynna vandamál…", L"Raporto një problem…", L"گزارش مشکل…", L"Báo cáo sự cố…", L"Хабар додан дар бораи мушкилот…", L"Асуудлын талаар мэдээлэх…", L"Проблема турында хәбәр итү…", L"Muammo haqida xabar berish…", L"Tuairiscigh fadhb…", L"Irrapporta problema…", L"Adrodd am broblem…"},
    {L"What happened and in which app:", L"Що сталося і в якій програмі:", L"Co się stało i w jakiej aplikacji:", L"Was ist passiert und in welcher App:", L"Qué pasó y en qué app:", L"Ce qui s'est passé et dans quelle app :", L"Cosa è successo e in quale app:", L"O que aconteceu e em que app:", L"Ne oldu ve hangi uygulamada:", L"Wat er gebeurde en in welke app:", L"Co se stalo a v jaké aplikaci:", L"Vad hände och i vilken app:", L"Mitä tapahtui ja missä ohjelmassa:", L"Ce s-a întâmplat și în ce aplicație:", L"Mi történt és melyik alkalmazásban:", L"Τι συνέβη και σε ποια εφαρμογή:", L"Какво се случи и в коя програма:", L"מה קרה ובאיזה יישום:", L"ماذا حدث وفي أي تطبيق:", L"क्या हुआ और किस ऐप में:", L"무슨 일이 어느 앱에서 있었나요:", L"Што здарылася і ў якой праграме:", L"Не болды және қай қолданбада:", L"Эмне болду жана кайсы колдонмодо:", L"რა მოხდა და რომელ აპლიკაციაში:", L"Ինչ է պատահել և որ հավելվածում՝", L"Nə baş verdi və hansı tətbiqdə:", L"Шта се догодило и у којој апликацији:", L"Што се случи и во која апликација:", L"Čo sa stalo a v ktorej aplikácii:", L"Što se dogodilo i u kojoj aplikaciji:", L"Kaj se je zgodilo in v kateri aplikaciji:", L"Kas nutiko ir kurioje programoje:", L"Kas notika un kurā lietotnē:", L"Mis juhtus ja millises rakenduses:", L"Hvad skete der, og i hvilken app:", L"Hva skjedde, og i hvilken app:", L"Hvað gerðist og í hvaða forriti:", L"Çfarë ndodhi dhe në cilin aplikacion:", L"چه اتفاقی افتاد و در کدام برنامه:", L"Chuyện gì đã xảy ra và trong ứng dụng nào:", L"Чӣ шуд ва дар кадом барнома:", L"Юу болсон, ямар аппад:", L"Нәрсә булды һәм кайсы кушымтада:", L"Nima boʻldi va qaysi ilovada:", L"Cad a tharla agus in cén aip:", L"X’ġara u f’liema applikazzjoni:", L"Beth ddigwyddodd ac ym mha ap:"},
    {L"Check for updates…", L"Перевірити оновлення…", L"Sprawdź aktualizacje…", L"Nach Updates suchen…", L"Buscar actualizaciones…", L"Rechercher des mises à jour…", L"Controlla aggiornamenti…", L"Procurar atualizações…", L"Güncellemeleri denetle…", L"Zoek naar updates…", L"Zkontrolovat aktualizace…", L"Sök efter uppdateringar…", L"Tarkista päivitykset…", L"Caută actualizări…", L"Frissítések keresése…", L"Έλεγχος για ενημερώσεις…", L"Проверка за актуализации…", L"בדיקת עדכונים…", L"التحقق من التحديثات…", L"अपडेट जाँचें…", L"업데이트 확인…", L"Праверыць абнаўленні…", L"Жаңартуларды тексеру…", L"Жаңыртууларды текшерүү…", L"განახლებების შემოწმება…", L"Ստուգել թարմացումները…", L"Yeniləmələri yoxla…", L"Провери ажурирања…", L"Провери за ажурирања…", L"Skontrolovať aktualizácie…", L"Provjeri ažuriranja…", L"Preverite posodobitve…", L"Tikrinti naujinimus…", L"Meklēt atjauninājumus…", L"Otsi uuendusi…", L"Søg efter opdateringer…", L"Se etter oppdateringer…", L"Athuga með uppfærslur…", L"Kontrollo për përditësime…", L"بررسی به‌روزرسانی…", L"Kiểm tra cập nhật…", L"Санҷиши навсозиҳо…", L"Шинэчлэлт шалгах…", L"Яңартуларны тикшерү…", L"Yangilanishlarni tekshirish…", L"Seiceáil le haghaidh nuashonruithe…", L"Iċċekkja għal aġġornamenti…", L"Gwirio am ddiweddariadau…"},
    {L"⬆︎ An update may be available — check", L"⬆︎ Можливо, вийшло оновлення — перевірити", L"⬆︎ Może być dostępna aktualizacja — sprawdź", L"⬆︎ Möglicherweise gibt es ein Update — prüfen", L"⬆︎ Puede haber una actualización — comprobar", L"⬆︎ Une mise à jour est peut-être disponible — vérifier", L"⬆︎ Potrebbe essere disponibile un aggiornamento — verifica", L"⬆︎ Pode haver uma atualização — verificar", L"⬆︎ Güncelleme olabilir — denetle", L"⬆︎ Er is misschien een update — controleren", L"⬆︎ Možná je k dispozici aktualizace — zkontrolovat", L"⬆︎ Det kan finnas en uppdatering — kontrollera", L"⬆︎ Päivitys saattaa olla saatavilla — tarkista", L"⬆︎ Poate există o actualizare — verifică", L"⬆︎ Lehet, hogy van frissítés — ellenőrzés", L"⬆︎ Ίσως υπάρχει ενημέρωση — έλεγχος", L"⬆︎ Може да има актуализация — проверете", L"⬆︎ ייתכן שיש עדכון — בדיקה", L"⬆︎ قد يتوفر تحديث — تحقّق", L"⬆︎ शायद नया अपडेट आया है — जाँचें", L"⬆︎ 업데이트가 있을 수 있습니다 — 확인", L"⬆︎ Магчыма, выйшла абнаўленне — праверыць", L"⬆︎ Жаңарту шыққан болуы мүмкін — тексеру", L"⬆︎ Жаңыртуу чыгып калышы мүмкүн — текшерүү", L"⬆︎ შესაძლოა განახლება გამოვიდა — შეამოწმეთ", L"⬆︎ Հնարավոր է՝ թարմացում է թողարկվել — ստուգել", L"⬆︎ Yeniləmə çıxmış ola bilər — yoxlayın", L"⬆︎ Можда је изашло ажурирање — провери", L"⬆︎ Можеби има ново ажурирање — провери", L"⬆︎ Možno vyšla aktualizácia — skontrolovať", L"⬆︎ Možda je izašlo ažuriranje — provjeri", L"⬆︎ Morda je izšla posodobitev — preverite", L"⬆︎ Galbūt išleistas naujinimas — patikrinkite", L"⬆︎ Iespējams, iznācis atjauninājums — pārbaudīt", L"⬆︎ Uuendus võib olla saadaval — kontrolli", L"⬆︎ Der kan være en opdatering — tjek", L"⬆︎ En oppdatering kan være tilgjengelig — sjekk", L"⬆︎ Uppfærsla gæti verið tiltæk — athuga", L"⬆︎ Mund të ketë një përditësim — kontrollo", L"⬆︎ ممکن است به‌روزرسانی موجود باشد — بررسی کنید", L"⬆︎ Có thể có bản cập nhật — kiểm tra", L"⬆︎ Шояд навсозӣ мавҷуд бошад — санҷед", L"⬆︎ Шинэчлэлт байж магадгүй — шалгах", L"⬆︎ Яңарту булырга мөмкин — тикшерегез", L"⬆︎ Yangilanish boʻlishi mumkin — tekshiring", L"⬆︎ B’fhéidir go bhfuil nuashonrú ar fáil — seiceáil", L"⬆︎ Jista’ jkun hemm aġġornament — iċċekkja", L"⬆︎ Efallai bod diweddariad ar gael — gwiriwch"},
    {L"Support AURA Switcher ☕", L"Підтримати AURA Switcher ☕", L"Wesprzyj AURA Switcher ☕", L"AURA Switcher unterstützen ☕", L"Apoyar AURA Switcher ☕", L"Soutenir AURA Switcher ☕", L"Sostieni AURA Switcher ☕", L"Apoiar o AURA Switcher ☕", L"AURA Switcher'ı destekle ☕", L"Steun AURA Switcher ☕", L"Podpořit AURA Switcher ☕", L"Stöd AURA Switcher ☕", L"Tue AURA Switcheriä ☕", L"Susține AURA Switcher ☕", L"Az AURA Switcher támogatása ☕", L"Υποστηρίξτε το AURA Switcher ☕", L"Подкрепете AURA Switcher ☕", L"תמכו ב-AURA Switcher ☕", L"ادعم AURA Switcher ☕", L"AURA Switcher का समर्थन करें ☕", L"AURA Switcher 후원하기 ☕", L"Падтрымаць AURA Switcher ☕", L"AURA Switcher жобасын қолдау ☕", L"AURA Switcher долбоорун колдоо ☕", L"AURA Switcher-ის მხარდაჭერა ☕", L"Աջակցել AURA Switcher-ին ☕", L"AURA Switcher-i dəstəkləyin ☕", L"Подржи AURA Switcher ☕", L"Поддржи го AURA Switcher ☕", L"Podporiť AURA Switcher ☕", L"Podrži AURA Switcher ☕", L"Podpri AURA Switcher ☕", L"Paremti AURA Switcher ☕", L"Atbalstīt AURA Switcher ☕", L"Toeta AURA Switcherit ☕", L"Støt AURA Switcher ☕", L"Støtt AURA Switcher ☕", L"Styðja AURA Switcher ☕", L"Mbështet AURA Switcher ☕", L"حمایت از AURA Switcher ☕", L"Ủng hộ AURA Switcher ☕", L"Дастгирии AURA Switcher ☕", L"AURA Switcher-ийг дэмжих ☕", L"AURA Switcher'ны хупларга ☕", L"AURA Switcher’ni qoʻllab-quvvatlash ☕", L"Tacaigh le AURA Switcher ☕", L"Appoġġa lil AURA Switcher ☕", L"Cefnogi AURA Switcher ☕"},
    {L"Quit", L"Вихід", L"Zakończ", L"Beenden", L"Salir", L"Quitter", L"Esci", L"Sair", L"Çık", L"Stop", L"Ukončit", L"Avsluta", L"Lopeta", L"Ieșire", L"Kilépés", L"Έξοδος", L"Изход", L"יציאה", L"إنهاء", L"बंद करें", L"종료", L"Выхад", L"Шығу", L"Чыгуу", L"გასვლა", L"Ելք", L"Çıxış", L"Изађи", L"Излези", L"Ukončiť", L"Izađi", L"Končaj", L"Išeiti", L"Iziet", L"Välju", L"Afslut", L"Avslutt", L"Hætta", L"Mbyll", L"خروج", L"Thoát", L"Баромадан", L"Гарах", L"Чыгу", L"Chiqish", L"Scoir", L"Ieqaf", L"Gadael"},
    {L"AURA Switcher {v} is out", L"Вийшла AURA Switcher {v}", L"Wyszła AURA Switcher {v}", L"AURA Switcher {v} ist da", L"Ya está aquí AURA Switcher {v}", L"AURA Switcher {v} est sortie", L"È disponibile AURA Switcher {v}", L"Saiu o AURA Switcher {v}", L"AURA Switcher {v} çıktı", L"AURA Switcher {v} is uit", L"Vyšel AURA Switcher {v}", L"AURA Switcher {v} är här", L"AURA Switcher {v} on julkaistu", L"A apărut AURA Switcher {v}", L"Megjelent az AURA Switcher {v}", L"Κυκλοφόρησε το AURA Switcher {v}", L"Излезе AURA Switcher {v}", L"יצאה AURA Switcher {v}", L"صدر AURA Switcher {v}", L"AURA Switcher {v} आ गया है", L"AURA Switcher {v} 출시", L"Выйшла AURA Switcher {v}", L"AURA Switcher {v} шықты", L"AURA Switcher {v} чыкты", L"გამოვიდა AURA Switcher {v}", L"Թողարկվել է AURA Switcher {v}", L"AURA Switcher {v} çıxdı", L"Изашао је AURA Switcher {v}", L"Излезе AURA Switcher {v}", L"Vyšiel AURA Switcher {v}", L"Izašao je AURA Switcher {v}", L"Izšel je AURA Switcher {v}", L"Išleistas AURA Switcher {v}", L"Iznācis AURA Switcher {v}", L"AURA Switcher {v} on väljas", L"AURA Switcher {v} er udkommet", L"AURA Switcher {v} er ute", L"AURA Switcher {v} er komið út", L"AURA Switcher {v} doli", L"AURA Switcher {v} منتشر شد", L"AURA Switcher {v} đã ra mắt", L"AURA Switcher {v} баромад", L"AURA Switcher {v} гарлаа", L"AURA Switcher {v} чыкты", L"AURA Switcher {v} chiqdi", L"Tá AURA Switcher {v} amuigh", L"AURA Switcher {v} ħareġ", L"Mae AURA Switcher {v} allan"},
    {L"You have the latest version", L"У вас остання версія", L"Masz najnowszą wersję", L"Du hast die neueste Version", L"Tienes la última versión", L"Vous avez la dernière version", L"Hai l’ultima versione", L"Tem a versão mais recente", L"En son sürümü kullanıyorsunuz", L"Je hebt de nieuwste versie", L"Máte nejnovější verzi", L"Du har den senaste versionen", L"Sinulla on uusin versio", L"Ai cea mai nouă versiune", L"A legújabb verzió van nálad", L"Έχετε την τελευταία έκδοση", L"Имате най-новата версия", L"יש לכם את הגרסה העדכנית", L"لديك أحدث إصدار", L"आपके पास नवीनतम संस्करण है", L"최신 버전을 사용 중입니다", L"У вас апошняя версія", L"Сізде соңғы нұсқа бар", L"Сизде акыркы версия бар", L"გაქვთ უახლესი ვერსია", L"Դուք ունեք վերջին տարբերակը", L"Ən son versiya sizdədir", L"Имате најновију верзију", L"Ја имате најновата верзија", L"Máte najnovšiu verziu", L"Imate najnoviju verziju", L"Imate najnovejšo različico", L"Turite naujausią versiją", L"Jums ir jaunākā versija", L"Sul on uusim versioon", L"Du har den nyeste version", L"Du har den nyeste versjonen", L"Þú ert með nýjustu útgáfuna", L"Keni versionin më të ri", L"جدیدترین نسخه را دارید", L"Bạn đang dùng phiên bản mới nhất", L"Шумо навтарин версияро доред", L"Танд хамгийн сүүлийн хувилбар байна", L"Сездә иң соңгы версия", L"Sizda eng soʻnggi versiya bor", L"Tá an leagan is déanaí agat", L"Għandek l-aħħar verżjoni", L"Mae gennych y fersiwn ddiweddaraf"},
    {L"What's new", L"Що нового", L"Co nowego", L"Was ist neu", L"Novedades", L"Nouveautés", L"Novità", L"Novidades", L"Yenilikler", L"Wat is er nieuw", L"Co je nového", L"Nyheter", L"Uutta", L"Ce e nou", L"Újdonságok", L"Τι νέο υπάρχει", L"Какво ново", L"מה חדש", L"ما الجديد", L"नया क्या है", L"새로운 기능", L"Што новага", L"Не жаңалық", L"Эмне жаңы", L"რა არის ახალი", L"Ինչ նոր է", L"Nə yenidir", L"Шта је ново", L"Што е ново", L"Čo je nové", L"Što je novo", L"Kaj je novega", L"Kas naujo", L"Kas jauns", L"Mis on uut", L"Nyheder", L"Nytt", L"Hvað er nýtt", L"Çfarë ka të re", L"چه چیز تازه است", L"Có gì mới", L"Чӣ нав аст", L"Шинэ зүйл", L"Нәрсә яңа", L"Yangiliklar", L"Cad atá nua", L"X’hemm ġdid", L"Beth sy’n newydd"},
    {L"Update", L"Оновити", L"Aktualizuj", L"Aktualisieren", L"Actualizar", L"Mettre à jour", L"Aggiorna", L"Atualizar", L"Güncelle", L"Bijwerken", L"Aktualizovat", L"Uppdatera", L"Päivitä", L"Actualizează", L"Frissítés", L"Ενημέρωση", L"Обнови", L"עדכן", L"تحديث", L"अपडेट करें", L"업데이트", L"Абнавіць", L"Жаңарту", L"Жаңыртуу", L"განახლება", L"Թարմացնել", L"Yenilə", L"Ажурирај", L"Ажурирај", L"Aktualizovať", L"Ažuriraj", L"Posodobi", L"Atnaujinti", L"Atjaunināt", L"Uuenda", L"Opdater", L"Oppdater", L"Uppfæra", L"Përditëso", L"به‌روزرسانی", L"Cập nhật", L"Навсозӣ", L"Шинэчлэх", L"Яңарту", L"Yangilash", L"Nuashonraigh", L"Aġġorna", L"Diweddaru"},
    {L"Later", L"Пізніше", L"Później", L"Später", L"Más tarde", L"Plus tard", L"Più tardi", L"Mais tarde", L"Daha sonra", L"Later", L"Později", L"Senare", L"Myöhemmin", L"Mai târziu", L"Később", L"Αργότερα", L"По-късно", L"מאוחר יותר", L"لاحقًا", L"बाद में", L"나중에", L"Пазней", L"Кейінірек", L"Кийинчерээк", L"მოგვიანებით", L"Հետո", L"Sonra", L"Касније", L"Подоцна", L"Neskôr", L"Kasnije", L"Pozneje", L"Vėliau", L"Vēlāk", L"Hiljem", L"Senere", L"Senere", L"Síðar", L"Më vonë", L"بعداً", L"Để sau", L"Баъдтар", L"Дараа", L"Соңрак", L"Keyinroq", L"Níos déanaí", L"Aktar tard", L"Yn nes ymlaen"},
    {L"Couldn't update automatically. Open the website to download it?", L"Не вдалося оновити автоматично. Відкрити сайт, щоб завантажити?", L"Nie udało się zaktualizować automatycznie. Otworzyć stronę, aby pobrać?", L"Automatisches Update fehlgeschlagen. Website zum Herunterladen öffnen?", L"No se pudo actualizar automáticamente. ¿Abrir la web para descargarla?", L"Mise à jour automatique impossible. Ouvrir le site pour la télécharger ?", L"Impossibile aggiornare automaticamente. Vuoi aprire il sito per scaricare l’aggiornamento?", L"Não foi possível atualizar automaticamente. Abrir o site para baixar?", L"Otomatik güncellenemedi. İndirmek için web sitesi açılsın mı?", L"Automatisch bijwerken is mislukt. De website openen om te downloaden?", L"Automatická aktualizace se nezdařila. Otevřít web a stáhnout?", L"Kunde inte uppdatera automatiskt. Öppna webbplatsen för att ladda ner?", L"Automaattinen päivitys epäonnistui. Avataanko sivusto latausta varten?", L"Actualizarea automată a eșuat. Deschizi site-ul pentru descărcare?", L"Az automatikus frissítés nem sikerült. Megnyitod a weboldalt a letöltéshez?", L"Η αυτόματη ενημέρωση απέτυχε. Άνοιγμα του ιστότοπου για λήψη;", L"Автоматичното обновяване не успя. Да се отвори ли сайтът за изтегляне?", L"העדכון האוטומטי נכשל. לפתוח את האתר להורדה?", L"تعذّر التحديث تلقائيًا. هل تريد فتح الموقع للتنزيل؟", L"अपने-आप अपडेट नहीं हो सका। डाउनलोड के लिए वेबसाइट खोलें?", L"자동으로 업데이트하지 못했습니다. 다운로드하려면 웹사이트를 열까요?", L"Не ўдалося абнавіць аўтаматычна. Адкрыць сайт, каб спампаваць?", L"Автоматты түрде жаңарту мүмкін болмады. Жүктеп алу үшін сайтты ашу керек пе?", L"Автоматтык түрдө жаңыртуу мүмкүн болгон жок. Жүктөп алуу үчүн сайтты ачайынбы?", L"ავტომატურად განახლება ვერ მოხერხდა. გავხსნა საიტი ჩამოსატვირთად?", L"Չհաջողվեց թարմացնել ավտոմատ կերպով։ Բացե՞լ կայքը՝ ներբեռնելու համար։", L"Avtomatik yeniləmək alınmadı. Yükləmək üçün sayt açılsın?", L"Аутоматско ажурирање није успело. Отворити сајт за преузимање?", L"Автоматското ажурирање не успеа. Да го отворам сајтот за преземање?", L"Automatická aktualizácia sa nepodarila. Otvoriť web na stiahnutie?", L"Automatsko ažuriranje nije uspjelo. Otvoriti web-stranicu za preuzimanje?", L"Samodejna posodobitev ni uspela. Odprem spletno mesto za prenos?", L"Nepavyko atnaujinti automatiškai. Atidaryti svetainę atsisiuntimui?", L"Automātiski atjaunināt neizdevās. Atvērt vietni lejupielādei?", L"Automaatne uuendamine ebaõnnestus. Avada veebileht allalaadimiseks?", L"Kunne ikke opdatere automatisk. Åbn hjemmesiden for at hente den?", L"Kunne ikke oppdatere automatisk. Åpne nettstedet for å laste den ned?", L"Ekki tókst að uppfæra sjálfkrafa. Opna vefsíðuna til að sækja hana?", L"Nuk u përditësua automatikisht. Të hapet faqja e internetit për ta shkarkuar?", L"به‌روزرسانی خودکار انجام نشد. وب‌سایت برای دانلود باز شود؟", L"Không thể tự động cập nhật. Mở trang web để tải về?", L"Навсозии худкор муяссар нашуд. Сайтро барои боргирӣ кушоед?", L"Автоматаар шинэчилж чадсангүй. Татаж авахын тулд вэбсайтыг нээх үү?", L"Автоматик рәвештә яңартып булмады. Йөкләү өчен сайтны ачарга?", L"Avtomatik yangilab boʻlmadi. Yuklab olish uchun saytni ochaymi?", L"Níorbh fhéidir nuashonrú go huathoibríoch. An bhfuil fonn ort an suíomh gréasáin a oscailt chun é a íoslódáil?", L"Ma setax jiġi aġġornat awtomatikament. Niftaħ is-sit biex tniżżlu?", L"Methu diweddaru’n awtomatig. Agor y wefan i’w lawrlwytho?"},
    {L"AURA Switcher is running. Its icon is in the notification area; click ^ next to the clock to open the menu", L"AURA Switcher працює. Значок — в області сповіщень; натисніть ^ біля годинника, щоб відкрити меню", L"AURA Switcher działa. Ikona jest w obszarze powiadomień; kliknij ^ obok zegara, aby otworzyć menu", L"AURA Switcher läuft. Das Symbol ist im Infobereich; klicken Sie neben der Uhr auf ^, um das Menü zu öffnen", L"AURA Switcher está funcionando. Su icono está en el área de notificaciones; haz clic en ^ junto al reloj para abrir el menú", L"AURA Switcher fonctionne. Son icône est dans la zone de notification ; cliquez sur ^ près de l’horloge pour ouvrir le menu", L"AURA Switcher è in esecuzione. L’icona è nell’area di notifica; fai clic su ^ accanto all’orologio per aprire il menu", L"O AURA Switcher está a funcionar. O ícone está na área de notificação; clique em ^ junto ao relógio para abrir o menu", L"AURA Switcher çalışıyor. Simgesi bildirim alanında; menüyü açmak için saatin yanındaki ^ simgesine tıklayın", L"AURA Switcher draait. Het pictogram staat in het systeemvak; klik op ^ naast de klok om het menu te openen", L"AURA Switcher běží. Ikona je v oznamovací oblasti; menu otevřete kliknutím na ^ vedle hodin", L"AURA Switcher körs. Ikonen finns i aviseringsfältet; klicka på ^ bredvid klockan för att öppna menyn", L"AURA Switcher on käynnissä. Kuvake on ilmoitusalueella; avaa valikko napsauttamalla kellon vieressä olevaa ^-merkkiä", L"AURA Switcher rulează. Pictograma este în zona de notificare; faceți clic pe ^ de lângă ceas pentru a deschide meniul", L"Az AURA Switcher fut. Az ikonja az értesítési területen van; a menü megnyitásához kattintson az óra melletti ^ jelre", L"Το AURA Switcher εκτελείται. Το εικονίδιο βρίσκεται στην περιοχή ειδοποιήσεων· κάντε κλικ στο ^ δίπλα στο ρολόι για να ανοίξετε το μενού", L"AURA Switcher работи. Иконата е в областта за известия; щракнете върху ^ до часовника, за да отворите менюто", L"AURA Switcher פועל. הסמל נמצא באזור ההתראות; לחצו על ^ ליד השעון כדי לפתוח את התפריט", L"AURA Switcher يعمل. الأيقونة في منطقة الإشعارات؛ انقر على ^ بجانب الساعة لفتح القائمة", L"AURA Switcher चल रहा है। इसका आइकन नोटिफ़िकेशन एरिया में है; मेनू खोलने के लिए घड़ी के पास ^ पर क्लिक करें", L"AURA Switcher가 실행 중입니다. 아이콘은 알림 영역에 있습니다. 시계 옆의 ^를 눌러 메뉴를 여세요", L"AURA Switcher працуе. Значок — у вобласці апавяшчэнняў; націсніце ^ побач з гадзіннікам, каб адкрыць меню", L"AURA Switcher жұмыс істеп тұр. Белгішесі хабарландыру аймағында; мәзірді ашу үшін сағаттың жанындағы ^ түймесін басыңыз", L"AURA Switcher иштеп жатат. Сөлөкөтү билдирмелер аймагында; менюну ачуу үчүн сааттын жанындагы ^ баскычын басыңыз", L"AURA Switcher მუშაობს. ხატულა შეტყობინებების არეშია; მენიუს გასახსნელად დააწკაპუნეთ საათის გვერდით ^-ზე", L"AURA Switcher-ը աշխատում է։ Պատկերակը ծանուցումների տարածքում է․ մենյուն բացելու համար սեղմեք ժամացույցի կողքի ^-ը", L"AURA Switcher işləyir. Nişan bildiriş sahəsindədir; menyunu açmaq üçün saatın yanındakı ^ işarəsinə klikləyin", L"AURA Switcher ради. Икона је у пољу за обавештења; кликните на ^ поред сата да бисте отворили мени", L"AURA Switcher работи. Иконата е во областа за известувања; кликнете на ^ покрај часовникот за да го отворите менито", L"AURA Switcher beží. Ikona je v oblasti oznámení; ponuku otvoríte kliknutím na ^ vedľa hodín", L"AURA Switcher radi. Ikona je u području obavijesti; kliknite ^ pored sata da otvorite izbornik", L"AURA Switcher deluje. Ikona je v območju za obvestila; kliknite ^ ob uri, da odprete meni", L"AURA Switcher veikia. Piktograma yra pranešimų srityje; spustelėkite ^ šalia laikrodžio, kad atidarytumėte meniu", L"AURA Switcher darbojas. Ikona ir paziņojumu apgabalā; noklikšķiniet uz ^ blakus pulkstenim, lai atvērtu izvēlni", L"AURA Switcher töötab. Ikoon on teavitusalas; menüü avamiseks klõpsake kella kõrval ^", L"AURA Switcher kører. Ikonet er i meddelelsesområdet; klik på ^ ved siden af uret for at åbne menuen", L"AURA Switcher kjører. Ikonet er i varslingsfeltet; klikk på ^ ved siden av klokken for å åpne menyen", L"AURA Switcher er í gangi. Táknið er á tilkynningasvæðinu; smelltu á ^ við hliðina á klukkunni til að opna valmyndina", L"AURA Switcher po funksionon. Ikona është në zonën e njoftimeve; kliko ^ pranë orës për të hapur menynë", L"AURA Switcher در حال اجراست. نماد آن در ناحیه اعلان‌هاست؛ برای باز کردن منو روی ^ کنار ساعت کلیک کنید", L"AURA Switcher đang chạy. Biểu tượng nằm ở khu vực thông báo; bấm ^ cạnh đồng hồ để mở menu", L"AURA Switcher кор карда истодааст. Нишона дар минтақаи огоҳиҳост; барои кушодани меню ^-ро дар назди соат пахш кунед", L"AURA Switcher ажиллаж байна. Дүрс тэмдэг мэдэгдлийн хэсэгт байна; цэсийг нээхийн тулд цагийн хажууох ^-г дарна уу", L"AURA Switcher эшли. Билгечә белдерүләр өлкәсендә; менюны ачу өчен сәгать янындагы ^ төймәсенә басыгыз", L"AURA Switcher ishlayapti. Belgisi bildirishnomalar hududida; menyuni ochish uchun soat yonidagi ^ belgisini bosing", L"Tá AURA Switcher ag rith. Tá an deilbhín sa limistéar fógraí; cliceáil ^ cois an chloig chun an roghchlár a oscailt", L"AURA Switcher qed jaħdem. L-ikona tinsab fiż-żona tan-notifiki; ikklikkja fuq ^ ħdejn l-arloġġ biex tiftaħ il-menu", L"Mae AURA Switcher yn rhedeg. Mae’r eicon yn yr ardal hysbysu; cliciwch ^ wrth ymyl y cloc i agor y ddewislen"},
    {L"Correction key", L"Клавіша виправлення", L"Klawisz poprawiania", L"Korrekturtaste", L"Tecla de corrección", L"Touche de correction", L"Tasto di correzione", L"Tecla de correção", L"Düzeltme tuşu", L"Correctietoets", L"Klávesa opravy", L"Korrigeringstangent", L"Korjausnäppäin", L"Tasta de corectare", L"Javítás gombja", L"Πλήκτρο διόρθωσης", L"Клавиш за поправка", L"מקש תיקון", L"مفتاح التصحيح", L"सुधार कुंजी", L"수정 키", L"Клавіша выпраўлення", L"Түзету пернесі", L"Оңдоо баскычы", L"შესწორების კლავიში", L"Ուղղման ստեղն", L"Düzəliş düyməsi", L"Тастер исправке", L"Копче за исправка", L"Kláves opravy", L"Tipka ispravka", L"Tipka popravka", L"Taisymo klavišas", L"Labošanas taustiņš", L"Parandusklahv", L"Rettetast", L"Rettetast", L"Leiðréttingartakki", L"Tasti i korrigjimit", L"کلید اصلاح", L"Phím sửa", L"Тугмаи ислоҳ", L"Засах товч", L"Төзәтү төймәсе", L"Tuzatish tugmasi", L"Eochair cheartúcháin", L"Ċavetta tal-korrezzjoni", L"Allwedd cywiro"},
    {L"Off", L"Вимкнено", L"Wyłączone", L"Aus", L"Desactivado", L"Désactivé", L"Disattivato", L"Desativado", L"Kapalı", L"Uit", L"Vypnuto", L"Av", L"Pois", L"Dezactivat", L"Ki", L"Ανενεργό", L"Изключено", L"כבוי", L"متوقف", L"बंद", L"끔", L"Выключана", L"Өшірулі", L"Өчүк", L"გამორთული", L"Անջատված", L"Söndürülüb", L"Искључено", L"Исклучено", L"Vypnuté", L"Isključeno", L"Izklopljeno", L"Išjungta", L"Izslēgts", L"Väljas", L"Fra", L"Av", L"Slökkt", L"Joaktiv", L"خاموش", L"Tắt", L"Хомӯш", L"Унтраасан", L"Сүндерелгән", L"Oʻchiq", L"As", L"Mitfi", L"I ffwrdd"},
    {L"Settings", L"Налаштування", L"Ustawienia", L"Einstellungen", L"Ajustes", L"Réglages", L"Impostazioni", L"Definições", L"Ayarlar", L"Instellingen", L"Nastavení", L"Inställningar", L"Asetukset", L"Setări", L"Beállítások", L"Ρυθμίσεις", L"Настройки", L"הגדרות", L"الإعدادات", L"सेटिंग्स", L"설정", L"Налады", L"Параметрлер", L"Жөндөөлөр", L"პარამეტრები", L"Կարգավորումներ", L"Ayarlar", L"Подешавања", L"Поставки", L"Nastavenia", L"Postavke", L"Nastavitve", L"Nustatymai", L"Iestatījumi", L"Seaded", L"Indstillinger", L"Innstillinger", L"Stillingar", L"Cilësimet", L"تنظیمات", L"Cài đặt", L"Танзимот", L"Тохиргоо", L"Көйләүләр", L"Sozlamalar", L"Socruithe", L"Settings", L"Gosodiadau"},
};
// END menu strings

static int uiIndex() {
    wstring code = S.uiLang;
    if (code.empty()) {
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = L"en";
        LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0);
        code = wstring(name).substr(0, 2);
        if (code == L"ru") code = L"uk";
    }
    for (int i = 0; i < UI_N; i++) if (code == UI_LANGS[i]) return i;
    return 0;
}
// menu-language regions (same as uiRegions in engine.swift): 0 Europe, 1 Caucasus & Central Asia, 2 Middle East, 3 Asia
static int uiRegion(const wchar_t* c) {
    for (const wchar_t* r : {L"kk", L"ky", L"ka", L"hy", L"az", L"tg", L"mn", L"uz"}) if (!wcscmp(c, r)) return 1;
    for (const wchar_t* r : {L"he", L"ar", L"fa", L"ku"}) if (!wcscmp(c, r)) return 2;
    for (const wchar_t* r : {L"hi", L"ko", L"zh", L"ja", L"vi"}) if (!wcscmp(c, r)) return 3;
    return 0;
}
// call sites keep English + Ukrainian inline for readability; the English text is the lookup key
static const wchar_t* L_(const wchar_t* en, const wchar_t* uk) {
    int i = uiIndex();
    if (i == 1) return uk;
    for (auto& row : UI_STR) if (!wcscmp(row[0], en)) return row[i];
    return en;
}

// MARK: - Layouts

// caps (Caps Lock) only flips the case of letters, not digits or symbols; not part of comparison
// sc: scan code — the physical key. Virtual-key codes differ between layouts for the same key
// (the key right of L is VK_OEM_1 in US but VK_OEM_3 in German), so they can't be compared across layouts.
struct Key { BYTE sc; bool shift; bool caps = false; bool operator<(const Key& o) const { return sc != o.sc ? sc < o.sc : shift < o.shift; } bool operator==(const Key& o) const { return sc == o.sc && shift == o.shift; } };

static wstring normalize(const wstring& s, NORM_FORM f) {
    int n = NormalizeString(f, s.c_str(), int(s.size()), nullptr, 0);
    if (n <= 0) return s;
    wstring r(n, 0); n = NormalizeString(f, s.c_str(), int(s.size()), &r[0], n);
    r.resize(n > 0 ? n : 0); return n > 0 ? r : s;
}
static bool isMark(wchar_t c) { return c >= 0x300 && c <= 0x36F; }

struct Layout {
    HKL hkl; wstring locale;  // e.g. "uk-UA"
    std::map<Key, wchar_t> toChar;
    std::map<wchar_t, Key> toKey;
    std::map<Key, wstring> dead;  // dead key (accent typed before the letter) → its combining marks
    // what the keys type here; a dead key and the letter after it make one accented letter (´ e → é)
    wstring text(const vector<Key>& keys) const {
        wstring s;
        for (size_t i = 0; i < keys.size(); i++) {
            const Key& k = keys[i];
            auto it = toChar.find(k); if (it == toChar.end()) continue;
            wchar_t c = it->second;
            if (k.caps && IsCharAlphaW(c)) c = k.shift ? towlower(c) : towupper(c);  // Windows: Caps+Shift → lowercase
            auto d = dead.find(k);
            if (d != dead.end() && i + 1 < keys.size()) {
                auto n = toChar.find(keys[i + 1]);
                if (n != toChar.end() && IsCharAlphaW(n->second)) {
                    wchar_t l = n->second; if (keys[i + 1].caps) l = keys[i + 1].shift ? towlower(l) : towupper(l);
                    s += normalize(wstring(1, l) + d->second, NormalizationC); i++; continue;
                }
            }
            s += c;
        }
        return s;
    }
    bool maps(const vector<Key>& keys) const { for (auto& k : keys) if (!toChar.count(k)) return false; return true; }
    // keys that type a character here: one key, or a dead key + letter for an accented one
    bool keysFor(wchar_t c, vector<Key>& out) const {
        auto it = toKey.find(c);
        if (it != toKey.end()) { out.push_back(it->second); return true; }
        wstring d = normalize(wstring(1, c), NormalizationD);
        if (d.size() < 2) return false;
        auto b = toKey.find(d[0]); if (b == toKey.end()) return false;
        for (auto& kv : dead) if (kv.second == d.substr(1)) { out.push_back(kv.first); out.push_back(b->second); return true; }
        return false;
    }
    wstring code() const;
};

// Language code written in the language's own script: en → EN, uk → УК, ru → РУ, el → ΕΛ
static wstring displayCode(const wstring& locale) {
    static const std::map<wstring, wstring> native = {
        {L"uk", L"УК"}, {L"ru", L"РУ"}, {L"be", L"БЕ"}, {L"bg", L"БГ"}, {L"sr", L"СР"}, {L"mk", L"МК"}, {L"kk", L"ҚЗ"}, {L"ky", L"КЫ"},
        {L"tg", L"ТҶ"}, {L"mn", L"МН"}, {L"el", L"ΕΛ"}, {L"ka", L"ქა"}, {L"hy", L"ՀՅ"}, {L"he", L"עב"}, {L"ar", L"عر"}, {L"fa", L"فا"}};
    wstring c = locale.substr(0, 2);
    auto it = native.find(c);
    if (it != native.end()) return it->second;
    for (auto& ch : c) ch = towupper(ch);
    return c;
}
wstring Layout::code() const { return displayCode(locale); }

static bool isLetterVk(BYTE vk) {
    return (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || (vk >= VK_OEM_1 && vk <= VK_OEM_3) || (vk >= VK_OEM_4 && vk <= VK_OEM_8) || vk == VK_OEM_102;
}

// what each character key types in layout h (tests build layouts this way too)
static bool buildLayout(HKL h, Layout& L) {
    L.hkl = h;
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    if (!LCIDToLocaleName(MAKELCID(LOWORD((UINT_PTR)h), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0)) return false;
    L.locale = name;
    BYTE state[256] = {};
    // character keys by scan code: number row, three letter rows, the extra ISO key
    for (UINT sc = 0x02; sc <= 0x56; sc++) {
        if (sc == 0x0E || sc == 0x0F || sc == 0x1C || sc == 0x1D || sc == 0x2A || (sc > 0x35 && sc < 0x56)) continue;
        UINT vk = MapVirtualKeyExW(sc, MAPVK_VSC_TO_VK, h);
        if (!vk || !isLetterVk(BYTE(vk))) continue;
        for (int sh = 0; sh < 2; sh++) {
            state[VK_SHIFT] = sh ? 0x80 : 0;
            wchar_t buf[4];
            int len = ToUnicodeEx(vk, sc, state, buf, 4, 4 /* don't change kbd state */, h);
            if ((len != 1 && len != -1) || buf[0] < 0x20) continue;
            Key k{BYTE(sc), sh != 0};
            L.toChar[k] = buf[0];
            if (!L.toKey.count(buf[0])) L.toKey[buf[0]] = k;
            if (len == -1) {  // dead key: the spacing accent (´ ΄ ¨) decomposes to space + combining marks
                wstring m; for (wchar_t c : normalize(wstring(1, buf[0]), NormalizationKD)) if (isMark(c)) m += c;
                if (!m.empty()) L.dead[k] = m;
            }
        }
    }
    return !L.toChar.empty();
}
static vector<Layout> loadLayouts() {
    vector<Layout> r;
    HKL list[64]; int n = GetKeyboardLayoutList(64, list);
    for (int i = 0; i < n; i++) {
        Layout L;
        if (!buildLayout(list[i], L)) continue;
        bool dup = false; for (auto& o : r) if (o.locale.substr(0, 2) == L.locale.substr(0, 2)) dup = true;
        if (!dup) r.push_back(L);
    }
    return r;
}

static vector<Layout> layouts;
static HWND hwndMain;

static HKL forcedLayout = nullptr;  // tests set the "active" layout here instead of switching the system's
// The window that takes the keys: in the Windows 11 Notepad the text field («RichEditD2DPT») runs in another
// thread than the main window, with its own layout — Win+Space changes only the field's. Read and switch there.
static HWND focusWindow() {
    HWND fg = GetForegroundWindow();
    GUITHREADINFO gi{}; gi.cbSize = sizeof gi;
    if (fg && GetGUIThreadInfo(GetWindowThreadProcessId(fg, nullptr), &gi) && gi.hwndFocus) return gi.hwndFocus;
    return fg;
}
static HKL fgLayout() {
    if (forcedLayout) return forcedLayout;
    return GetKeyboardLayout(GetWindowThreadProcessId(focusWindow(), nullptr));
}
static int layoutIndex(HKL h) {
    // layouts are one per language (en-US and en-GB merged), so match by language, not by full LANGID
    for (size_t i = 0; i < layouts.size(); i++) if (PRIMARYLANGID(LOWORD((UINT_PTR)layouts[i].hkl)) == PRIMARYLANGID(LOWORD((UINT_PTR)h))) return int(i);
    return 0;
}
static void selectLayout(const Layout& l) {
    PostMessageW(focusWindow(), WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)l.hkl);
}
static wstring fgExe() {
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t path[MAX_PATH]; DWORD sz = MAX_PATH; wstring r;
    if (QueryFullProcessImageNameW(h, 0, path, &sz)) { r = path; r = r.substr(r.find_last_of(L'\\') + 1); }
    CloseHandle(h);
    return r;
}

// MARK: - Hook thread state (touched only by the hook thread)

static vector<Key> word, lastWord, before;  // before: one-letter word right before `word` («z буду»), fixed together with it
static int trailing = 0;
static std::atomic<unsigned> seq{0};            // bumps on every real key; main thread uses it to detect typing
static std::atomic<bool> resetReq{false}, active{true};
static ULONGLONG shiftDownAt = 0, lastShiftTap = 0; static bool sawOther = false, shiftHeld = false;

struct Snapshot { vector<Key> keys; int trailing; unsigned seq; vector<Key> before; };

// the user went back into the text (Backspace over a space, arrows, a Ctrl/Alt/Win combo in the middle of a word):
// until the next space AURA neither checks nor converts what is typed — it's the user's own word
static bool handsOff = false;
static bool returnHeld = false;  // Return held for a moment to fix the word before it (see WM_ENTER); swallow its key-up
static std::atomic<DWORD> returnScan{0}, returnFlags{0};  // the held Return's scan code and extended flag (numpad Enter)
static void resetState() { word.clear(); lastWord.clear(); before.clear(); trailing = 0; handsOff = false; }

static Key readKey(const KBDLLHOOKSTRUCT* k) {
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    return {BYTE(k->scanCode), shift, caps};
}

static LRESULT CALLBACK kbHook(int code, WPARAM wp, LPARAM lp) {
    auto* k = (KBDLLHOOKSTRUCT*)lp;
    if (code != HC_ACTION || k->dwExtraInfo == kMagic || !active) return CallNextHookEx(nullptr, code, wp, lp);
    if (resetReq.exchange(false)) resetState();
    bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
    BYTE vk = BYTE(k->vkCode);
    ULONGLONG now = GetTickCount64();

    int hk = hkVk;
    bool isShiftKey = vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT;
    bool isCtrlKey = (vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_CONTROL) && k->scanCode != 0x21D;  // 0x21D: the Ctrl AltGr adds
    bool isAltKey = vk == VK_LMENU || vk == VK_RMENU || vk == VK_MENU;
    if (hk && (hk == VK_SHIFT ? isShiftKey : hk == VK_CONTROL ? isCtrlKey : isAltKey)) {
        auto held = [](int v) { return (GetAsyncKeyState(v) & 0x8000) != 0; };
        bool mods = held(VK_LWIN) || held(VK_RWIN) || (hk != VK_SHIFT && held(VK_SHIFT)) || (hk != VK_CONTROL && held(VK_CONTROL)) || (hk != VK_MENU && held(VK_MENU));
        if (down) {
            if (!shiftHeld) {
                shiftHeld = true; shiftDownAt = now; sawOther = false;
                if (hk == VK_MENU) {   // an unassigned key while Alt is down: a lone Alt tap must not open the program's menu bar
                    INPUT in[2] = {}; in[0].type = in[1].type = INPUT_KEYBOARD;
                    in[0].ki.wVk = in[1].ki.wVk = 0xE8; in[0].ki.dwExtraInfo = in[1].ki.dwExtraInfo = kMagic; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(2, in, sizeof(INPUT));
                }
            }
            if (mods) lastShiftTap = 0;
        } else {
            shiftHeld = false;
            if (sawOther || mods || now - shiftDownAt > 300) lastShiftTap = 0;
            else if (lastShiftTap && now - lastShiftTap < 450) {
                lastShiftTap = 0;
                // the word being typed, or the previous one right after a single space; after two or more spaces
                // the double tap only switches the layout. In the user's own word it converts what was typed since the edit
                bool cur = !word.empty();
                auto* s = new Snapshot{cur ? word : trailing == 1 ? lastWord : vector<Key>(), cur ? 0 : trailing, seq.load()};
                PostMessageW(hwndMain, WM_DSHIFT, 0, (LPARAM)s);
            } else lastShiftTap = now;
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }
    if (isShiftKey && hk != VK_SHIFT) {   // Shift is not the correction key: it only cancels a tap in progress, as before it never touched the word
        if (down) { sawOther = true; lastShiftTap = 0; }
        return CallNextHookEx(nullptr, code, wp, lp);
    }
    if (!down) {
        if (vk == VK_RETURN && returnHeld) { returnHeld = false; return 1; }  // its key-down is being re-sent
        return CallNextHookEx(nullptr, code, wp, lp);
    }

    sawOther = true; lastShiftTap = 0; seq++;
    bool ctrlAlt = (GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
    if (ctrlAlt) { bool mid = !word.empty(); resetState(); handsOff = mid; return CallNextHookEx(nullptr, code, wp, lp); }

    if (vk == VK_RETURN && !word.empty() && !handsOff && !returnHeld) {
        // Return right after a word («z elfkbk⏎» in a chat): checked only on a space, the message went out unfixed.
        // Hold Return, let the main thread fix the word, it sends Return on
        returnHeld = true;
        returnScan = k->scanCode; returnFlags = k->flags & LLKHF_EXTENDED;
        PostMessageW(hwndMain, WM_ENTER, 0, (LPARAM)new Snapshot{word, 0, seq.load(), before});
        resetState();
        return 1;
    }
    if (vk == VK_BACK) {
        // a typo while the word is being typed: AURA keeps watching it. Back over the space into a finished word
        // (or into older text): the word is the user's now
        if (!word.empty() && trailing == 0) word.pop_back();
        else { resetState(); handsOff = true; PostMessageW(hwndMain, WM_EMPTYCHK, 0, (LPARAM)seq.load()); }
    } else if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN || vk == VK_HOME || vk == VK_END
               || vk == VK_PRIOR || vk == VK_NEXT || vk == VK_DELETE) {
        resetState(); handsOff = true;  // editing the text
        if (vk == VK_DELETE) PostMessageW(hwndMain, WM_EMPTYCHK, 0, (LPARAM)seq.load());
    } else if (handsOff) {
        // the user's own word: not checked, but its keys are kept — double ⇧ is an explicit command and converts it
        if (isLetterVk(vk)) word.push_back(readKey(k));
        else resetState();  // a space, Return, Tab…: the user's word is over
    } else if (vk == VK_SPACE) {
        if (!word.empty()) {
            lastWord = word; word.clear(); trailing = 1;
            PostMessageW(hwndMain, WM_AUTO, 0, (LPARAM)new Snapshot{lastWord, 1, seq.load(), before});
        } else if (!lastWord.empty()) trailing++;
    } else if (isLetterVk(vk)) {
        if (word.empty()) before = lastWord.size() == 1 && trailing == 1 ? lastWord : vector<Key>();
        if (trailing > 0 || !lastWord.empty()) { lastWord.clear(); trailing = 0; }
        word.push_back(readKey(k));
    } else resetState();
    return CallNextHookEx(nullptr, code, wp, lp);
}

static LRESULT CALLBACK mouseHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && (wp == WM_LBUTTONDOWN || wp == WM_RBUTTONDOWN)) { resetState(); lastShiftTap = 0; }
    return CallNextHookEx(nullptr, code, wp, lp);
}

static DWORD WINAPI hookThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetWindowsHookExW(WH_KEYBOARD_LL, kbHook, GetModuleHandleW(nullptr), 0);
    SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
    MSG m; while (GetMessageW(&m, nullptr, 0, 0)) {}
    return 0;
}

// MARK: - Synthetic input

static void sendKey(WORD vk, bool shift = false) {
    vector<INPUT> in;
    auto add = [&](WORD v, bool up) { INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wVk = v; i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0; i.ki.dwExtraInfo = kMagic; in.push_back(i); };
    if (shift) add(VK_SHIFT, false);
    add(vk, false); add(vk, true);
    if (shift) add(VK_SHIFT, true);
    SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
}
// Sleep(2) really sleeps a whole timer tick (~15.6 ms); a high-resolution waitable timer (Windows 10 1803+)
// keeps short pauses short. Older systems fall back to Sleep.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
static void pauseMs(int ms) {
    static HANDLE t = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!t) { Sleep(ms); return; }
    LARGE_INTEGER due; due.QuadPart = -10000LL * ms;
    if (SetWaitableTimer(t, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(t, 100); else Sleep(ms);
}
// One character per SendInput with a short pause: the Windows 11 Notepad (WinUI + TSF) turns a solid burst of
// VK_PACKET characters into spaces — «ghbdsn » became 7 spaces. Classic edit boxes don't care either way.
static void typeText(const wstring& s) {
    for (size_t n = 0; n < s.size(); n++) {
        wchar_t c = s[n];
        INPUT in[2] = {};
        for (int up = 0; up < 2; up++) {
            INPUT& i = in[up]; i.type = INPUT_KEYBOARD; i.ki.dwExtraInfo = kMagic;
            if (c == L'\n' || c == L'\r') { i.ki.wVk = VK_RETURN; i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0; }  // line break = Enter
            else { i.ki.wScan = c; i.ki.dwFlags = KEYEVENTF_UNICODE | (up ? KEYEVENTF_KEYUP : 0); }
        }
        SendInput(2, in, sizeof(INPUT));
        // a space inside the text («я удалил»): Win11 Notepad handles a word end (its spell check) and drops or
        // repeats the next letters if they come right away
        pauseMs(c == L' ' && n + 1 < s.size() ? 40 : 3);
    }
}

// Replace the word with `s` in layout `l`: switch the window to `l` first and wait until it has it, then type `s`
// with that layout's real keys, like a person. Win11 Notepad (TSF) sometimes blanks or drops VK_PACKET characters
// right after Backspaces whatever the pause; real keys it takes in order. Characters the layout lacks (or with
// Caps Lock on) still go as VK_PACKET.
static void typeKeysIn(const wstring& s, HKL hkl) {
    bool caps = GetKeyState(VK_CAPITAL) & 1;
    for (size_t at = 0; at < s.size(); at++) {
        wchar_t c = s[at];
        SHORT v = caps ? -1 : VkKeyScanExW(c, hkl);
        if (v == -1 || (HIBYTE(v) & ~1)) { typeText(wstring(1, c)); continue; }  // not there, or needs Ctrl/Alt
        WORD vk = LOBYTE(v); bool shift = HIBYTE(v) & 1;
        INPUT in[4] = {}; int n = 0;
        auto add = [&](WORD k, bool up) { INPUT& i = in[n++]; i.type = INPUT_KEYBOARD; i.ki.wVk = k;
            i.ki.wScan = WORD(MapVirtualKeyExW(k, MAPVK_VK_TO_VSC, hkl)); i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0; i.ki.dwExtraInfo = kMagic; };
        if (shift) add(VK_SHIFT, false);
        add(vk, false); add(vk, true);
        if (shift) add(VK_SHIFT, true);
        SendInput(n, in, sizeof(INPUT));
        // a person's pace, not a burst: Win11 Notepad loses real keys 5 ms apart in a longer replacement
        // («я удалил» → «я удил»), and after a word's space it's busy for a moment (its spell check)
        pauseMs(c == L' ' && at + 1 < s.size() ? 40 : 12);
    }
}
static void replaceIn(int count, const wstring& s, const Layout& l) {
    selectLayout(l);
    bool switched = false;
    for (int i = 0; i < 40 && !(switched = GetKeyboardLayout(GetWindowThreadProcessId(focusWindow(), nullptr)) == l.hkl); i++) pauseMs(5);
    // let the app finish the user's own space first (Notepad dropped a whole Backspace burst right after it), and
    // apply the Backspaces before the text («ыыло» instead of «было» in Notepad)
    pauseMs(25);
    for (int i = 0; i < count; i++) sendKey(VK_BACK);
    // Win11 Notepad sometimes applies the last Backspaces after the first new letters («ривіт»): 30 ms lost
    // 2–4 of 20 replacements, double ⇧ and auto-correction alike; 100 ms lost none of 60
    pauseMs(100);
    if (switched) typeKeysIn(s, l.hkl); else typeText(s);
}

// MARK: - Spell checking (Windows Spell Checking API)

static ISpellCheckerFactory* factory = nullptr;
static std::map<wstring, ISpellChecker*> checkers;

static ISpellChecker* checkerFor(const wstring& locale) {
    auto it = checkers.find(locale);
    if (it != checkers.end()) return it->second;
    ISpellChecker* c = nullptr; BOOL ok = FALSE;
    if (factory && SUCCEEDED(factory->IsSupported(locale.c_str(), &ok)) && ok) factory->CreateSpellChecker(locale.c_str(), &c);
    checkers[locale] = c;
    return c;
}
static bool spell(const wstring& w, const wstring& locale) {
    ISpellChecker* c = checkerFor(locale);
    if (!c) return false;
    IEnumSpellingError* errs = nullptr;
    if (FAILED(c->Check(w.c_str(), &errs)) || !errs) return false;
    ISpellingError* e = nullptr;
    bool clean = errs->Next(&e) != S_OK;
    if (e) e->Release();
    errs->Release();
    return clean;
}

// a dictionary that accepts nonsense typed in its own layout can't tell words apart:
// English typos would turn into gibberish in that language. Checked once per language.
static std::map<wstring, bool> reliable;
static bool dictionaryWorks(const wstring& locale) {
    auto it = reliable.find(locale);
    if (it != reliable.end()) return it->second;
    bool r = checkerFor(locale) != nullptr;  // no dictionary at all: not "works" — the language model takes over
    for (auto& l : layouts) {
        if (!r) break;
        if (l.locale != locale) continue;
        // nonsense from the layout's own letters (keys like Q or Z may type several characters elsewhere)
        // only the layout's own script (a Cyrillic layout may also type a Latin letter the spell check skips)
        // no upper case: Windows spell checking skips ALL-CAPS words, so «AAAAAAA» always passes
        // (not "lower case only": scripts without case — Hebrew, Arabic, Hindi, Korean — have no lower-case letters at all)
        std::set<wchar_t> set, own; for (auto& kv : l.toChar) if (IsCharAlphaW(kv.second) && !IsCharUpperW(kv.second)) { set.insert(kv.second); if (kv.second >= 0x250) own.insert(kv.second); }
        vector<wchar_t> letters = own.size() >= 3 ? vector<wchar_t>(own.begin(), own.end()) : vector<wchar_t>(set.begin(), set.end());
        if (letters.size() >= 3) {
            wstring a(7, letters[0]), b; for (int i = 0; i < 7; i++) b += letters[1 + i % 2];
            if (spell(a, locale) || spell(b, locale)) r = false;
        }
        break;
    }
    return reliable[locale] = r;
}

// the spell checker takes any two letters for a word (":D" → «ЖВ»): only common ones count
static bool isWord(wstring w, const wstring& locale) {
    static const std::map<wstring, wstring> two = {
        {L"uk", L" як що не та до на за по ти ми ви їх це її ще бо чи ні де то ну ой аж би зі із ця ці ті ок "},
        {L"ru", L" не на по за до от из во ко то же бы ли мы вы он её их да но ни уж ой ну ах ох со об ты та те эй ок "},
        {L"en", L" is in on at to it he we me my of or an as be by do go if no so up us am hi ok oh ah ha eh yo id "}};
    if (w.size() < 2) return false;
    for (wchar_t c : w) if (!IsCharAlphaW(c) && c != L'\'' && c != L'-') return false;
    wstring low = w; CharLowerBuffW(&low[0], DWORD(low.size()));
    auto t = two.find(locale.substr(0, 2));
    if (w.size() == 2 && t != two.end()) return t->second.find(L" " + low + L" ") != wstring::npos;
    wstring up = w; CharUpperBuffW(&up[0], DWORD(up.size()));
    if (w == up) w = low;  // all caps passes any spell check
    if (!dictionaryWorks(locale)) return false;
    if (spell(w, locale)) return true;
    // names typed in lower case: «вінницею» is only in the dictionary as «Вінницею». Not for Latin
    // (English names like «Tot» would make gibberish a word) and not for short words
    if (w != low || w.size() < 5 || w[0] < 0x250) return false;
    wstring cap = w; CharUpperBuffW(&cap[0], 1);
    return spell(cap, locale);
}

// five Cyrillic consonants in a row: gibberish the spell checker let through ("brachii," → «икфсршшб»)
static bool consonantRun(const wstring& s) {
    int run = 0;
    for (wchar_t c : s) {
        wchar_t l = wchar_t(UINT_PTR(CharLowerW(LPWSTR(UINT_PTR(c)))));
        run = wcschr(L"бвгґджзйклмнпрстфхцчшщ", l) && l ? run + 1 : 0;
        if (run >= 5) return true;
    }
    return false;
}

static bool isCyrillic(const wstring& s) { for (wchar_t c : s) if (c >= 0x400 && c <= 0x52F) return true; return false; }
static bool isLatin(const wstring& s) { for (wchar_t c : s) if (c >= 0x250) return false; return true; }

// The first spell check in a language loads its dictionary (can take a moment). If that happens
// while the user types on, the correction is skipped — so load all dictionaries up front.
// MARK: - Language models
// For languages whose dictionary may be missing (lang/<code>.txt, embedded as LM_<CODE>): the most frequent
// words, and letter-trigram costs (−10·ln p) that tell a word typed in the wrong layout from an English typo.
struct LangModel {
    std::unordered_set<wstring> words; std::unordered_map<wstring, int> tri; int floor = 100;
    bool has(wstring w) const { CharLowerBuffW(&w[0], DWORD(w.size())); return words.count(w) > 0; }
    double cost(wstring w) const {  // average trigram cost: lower = more typical of the language
        CharLowerBuffW(&w[0], DWORD(w.size()));
        wstring c = L"^" + w + L"$";
        if (c.size() < 3) return floor;
        int sum = 0;
        for (size_t i = 0; i + 3 <= c.size(); i++) { auto it = tri.find(c.substr(i, 3)); sum += it == tri.end() ? floor : it->second; }
        return double(sum) / double(c.size() - 2);
    }
};
static std::map<wstring, std::unique_ptr<LangModel>> models;
static const LangModel* model(const wstring& locale) {
    wstring code = locale.substr(0, 2);
    auto it = models.find(code);
    if (it != models.end()) return it->second.get();
    std::unique_ptr<LangModel> m;
    wstring name = L"LM_" + code; CharUpperBuffW(&name[0], DWORD(name.size()));
    if (HRSRC r = FindResourceW(nullptr, name.c_str(), RT_RCDATA)) {
        const char* p = (const char*)LockResource(LoadResource(nullptr, r)); int n = int(SizeofResource(nullptr, r));
        int wn = MultiByteToWideChar(CP_UTF8, 0, p, n, nullptr, 0);
        wstring s(wn, 0); MultiByteToWideChar(CP_UTF8, 0, p, n, &s[0], wn);
        m.reset(new LangModel());
        bool first = true; int maxCost = 0; size_t pos = 0;
        while (pos < s.size()) {
            size_t e = s.find(L'\n', pos); if (e == wstring::npos) e = s.size();
            wstring line = s.substr(pos, e - pos); pos = e + 1;
            if (line.empty() || line[0] == L'#') continue;
            if (first) { first = false; size_t a = 0; while (a < line.size()) { size_t b = line.find(L' ', a); if (b == wstring::npos) b = line.size(); if (b > a) m->words.insert(line.substr(a, b - a)); a = b + 1; } continue; }
            size_t t = line.find(L'\t');
            if (t != wstring::npos) { int v = _wtoi(line.c_str() + t + 1); m->tri[line.substr(0, t)] = v; maxCost = std::max(maxCost, v); }
        }
        m->floor = maxCost + 10;
    }
    return (models[code] = std::move(m)).get();
}
// how much more typical `t` is of its language than `typed` of its own; -1e9 without both models
static double margin(const wstring& typed, const wstring& cur, const wstring& t, const wstring& lang) {
    const LangModel* a = model(cur); const LangModel* b = model(lang);
    if (!a || !b) return -1e9;
    // a comma or «?» stuck to the word («шейдерам,») is an unknown trigram on one side only: judge the letters
    auto trim = [](const wstring& w) {
        size_t x = w.find_first_not_of(L",.;:!?"), y = w.find_last_not_of(L",.;:!?");
        return x == wstring::npos ? wstring() : w.substr(x, y - x + 1);
    };
    wstring x = trim(typed), y = trim(t);
    return a->cost(x.size() >= 2 ? x : typed) - b->cost(y.size() >= 2 ? y : t);
}
// language of the last non-Latin word that was fixed or typed right: with equal evidence it wins
// («ns» is both «ты» (ru) and «ті» (uk)); remembered for 10 minutes. Not set by the tests, which call autoFixTarget alone
static wstring recentLang; static ULONGLONG recentAt = 0;
static bool lastWasWord = false;  // autoFixTarget found nothing because the word is right where it was typed
// tuned on ~2000 words per language (tools/langmodel.py): a word with no dictionary must look clearly
// more typical than what was typed; a real dictionary word only somewhat more
static const double kMarginNeeded = 20, kMarginForDictWord = 5, kMarginCyrillic = 11;  // Belarusian ↔ Russian: «длінный» 13, true «сіла» 4

// Code typed in the wrong layout — «сщтіщдуюдщп(ещефд)ж» is console.log(total); — is one "word" with
// dots and brackets: starts with a letter, and every part (split on non-letters and camelCase) is a word.
static bool isCode(const wstring& t, const wstring& locale) {
    if (t.empty() || !IsCharAlphaW(t[0])) return false;
    vector<wstring> parts; wstring cur;
    for (wchar_t c : t) {
        if (!IsCharAlphaW(c)) { if (!cur.empty()) parts.push_back(cur); cur.clear(); continue; }
        if (IsCharUpperW(c) && !cur.empty() && IsCharLowerW(cur.back())) { parts.push_back(cur); cur.clear(); }
        cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    size_t end = t.find_last_not_of(L";,.)");
    bool punctuated = false; for (size_t i = 0; end != wstring::npos && i <= end; i++) if (!IsCharAlphaW(t[i])) punctuated = true;
    bool long3 = false; for (auto& p : parts) { if (p.size() >= 3) long3 = true; if (p.size() < 2 || !isWord(p, locale)) return false; }
    return (parts.size() > 1 || punctuated) && long3;
}

static void warmUpDictionaries() { reliable.clear(); for (auto& l : layouts) isWord(L"aura", l.locale); }

static int autoFixTarget(const vector<Key>& keys) {
    lastWasWord = false;
    if (keys.size() < 2 || layouts.empty()) return -1;
    int cur = layoutIndex(fgLayout());
    const wstring& curLoc = layouts[cur].locale;
    bool curDict = dictionaryWorks(curLoc);
    if (!curDict && !model(curLoc)) return -1;  // typed in a language we can't check at all: may well be right
    wstring typed = layouts[cur].text(keys);
    if (S.isUserWord(typed)) return -1;
    // Windows dictionaries accept many short letter groups («kai», «zum», «еру»): a short word the dictionary
    // allows but the model doesn't know as frequent is weak — it still yields to a frequent word elsewhere
    const LangModel* curLm = model(curLoc);
    bool weak = false;
    if (curDict ? isWord(typed, curLoc) : curLm->has(typed)) {
        weak = curDict && typed.size() <= 4 && curLm && !curLm->has(typed);
        if (!weak) { lastWasWord = true; return -1; }
    }
    // every other layout is a candidate; take the strongest kind of evidence, not the first layout in the
    // list: with RU, UK, EN in that order «ghbdsn» must become the Ukrainian word «привіт», not the
    // Russian look-alike «привыт» that only passes on letter statistics
    // equal evidence (both are dictionary words: «было» / «біло» from the same keys) — a frequent word of its
    // language wins over a rare one, then the letter statistics decide
    // «ns» is both «ты» (ru) and «ті» (uk): with equal evidence the language of the last non-Latin word wins
    wstring recent = GetTickCount64() - recentAt < 600000 ? recentLang : wstring();
    int best = -1, bestTier = 0, bestFreq = 0, bestRec = 0; double bestM = 0;
    for (size_t i = 1; i < layouts.size(); i++) {
        int j = int((cur + i) % layouts.size());
        wstring t = layouts[j].text(keys);
        // same text in two Latin layouts (an English typo that is a Polish word): nothing to fix.
        // Cyrillic UK/RU words spelled the same still switch the layout — that's what users mix up.
        if (t == typed && isLatin(t)) continue;
        if (!layouts[j].maps(keys) || consonantRun(t)) continue;
        double m = margin(typed, curLoc, t, layouts[j].locale);
        int tier = 0;
        if (S.isUserWord(t)) tier = 4;  // the user's own word («скілі»)
        else if (isLatin(t) && !isLatin(typed) && isCode(t, layouts[j].locale)) tier = 3;
        else {
            if (dictionaryWorks(layouts[j].locale)) {
                // without the typed language's dictionary, also ask the models which side looks real;
                // close languages in one script (Belarusian ↔ Russian) need the stricter margin
                double need = isCyrillic(typed) && isCyrillic(t) ? kMarginCyrillic : kMarginForDictWord;
                if (isWord(t, layouts[j].locale) && (curDict || m >= need)) tier = 2;
                // slang the dictionary doesn't know («гуглити»): long, another script, clearly typical of the language
                else if (t.size() >= 5 && isLatin(typed) != isLatin(t) && m >= kMarginNeeded) tier = 1;
            }
            // the model's evidence counts too (a dictionary only adds certainty): a frequent word,
            // or clearly more typical than what was typed
            if (const LangModel* lm = model(layouts[j].locale)) {
                if (lm->has(t)) tier = std::max(tier, 2); else if (m >= kMarginNeeded) tier = std::max(tier, 1);
            }
            // Russian layout, not a Russian word, and the same keys give letters only Ukrainian has
            // («вынницею» → «вінницею»): the Ukrainian side only has to look more typical
            if (!tier && curDict && isCyrillic(typed) && isCyrillic(t) && m > 0 && curLoc.compare(0, 2, L"ru") == 0 &&
                layouts[j].locale.compare(0, 2, L"uk") == 0 && t.find_first_of(L"іїєґІЇЄҐ") != wstring::npos) tier = 1;
        }
        if (weak) { const LangModel* lm = model(layouts[j].locale); if (!lm || !lm->has(t)) tier = 0; }
        const LangModel* fm = model(layouts[j].locale); int freq = fm && fm->has(t) ? 1 : 0;
        int rec = !recent.empty() && layouts[j].locale == recent ? 1 : 0;
        if (tier > bestTier || (tier && tier == bestTier && (freq > bestFreq || (freq == bestFreq && (rec > bestRec || (rec == bestRec && m > bestM)))))) {
            best = j; bestTier = tier; bestFreq = freq; bestRec = rec; bestM = m;
        }
    }
    return best;
}

// MARK: - Actions (main thread)

static wstring lastSelOut; static int lastSelLang = 0;

static bool openClipboard() { for (int i = 0; i < 10; i++) { if (OpenClipboard(hwndMain)) return true; Sleep(10); } return false; }
static bool clipboardText(wstring& out) {
    if (!openClipboard()) return false;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    bool ok = false;
    if (h) { auto* p = (const wchar_t*)GlobalLock(h); if (p) { out = p; ok = true; GlobalUnlock(h); } }
    CloseClipboard();
    return ok;
}
// The user's clipboard, every format (text, rich text, images as DIB, files), to put back after ⌘C.
struct ClipFormat { UINT fmt; vector<BYTE> data; };
static vector<ClipFormat> saveClipboard() {
    vector<ClipFormat> r;
    if (!openClipboard()) return r;
    for (UINT f = EnumClipboardFormats(0); f; f = EnumClipboardFormats(f)) {
        // GDI handles and private formats aren't plain memory; images survive as CF_DIB/CF_DIBV5
        if (f == CF_BITMAP || f == CF_METAFILEPICT || f == CF_PALETTE || f == CF_ENHMETAFILE || f == CF_OWNERDISPLAY ||
            f == CF_DSPBITMAP || f == CF_DSPMETAFILEPICT || f == CF_DSPENHMETAFILE || (f >= CF_PRIVATEFIRST && f <= CF_GDIOBJLAST)) continue;
        HANDLE h = GetClipboardData(f); if (!h) continue;
        SIZE_T n = GlobalSize(h); auto* p = (const BYTE*)GlobalLock(h);
        if (!p) continue;
        if (n) r.push_back({f, vector<BYTE>(p, p + n)});
        GlobalUnlock(h);
    }
    CloseClipboard();
    return r;
}
static void restoreClipboard(const vector<ClipFormat>& saved) {
    if (!openClipboard()) return;
    EmptyClipboard();
    for (auto& c : saved) {
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, c.data.size()); if (!g) continue;
        memcpy(GlobalLock(g), c.data.data(), c.data.size()); GlobalUnlock(g);
        if (!SetClipboardData(c.fmt, g)) GlobalFree(g);
    }
    CloseClipboard();
}
enum { TIMER_TRAY = 1, TIMER_LATE_CLIP = 2 };
static vector<ClipFormat> lateClip; static DWORD lateClipSeq = 0;  // copy that may still land after we gave up

static bool copySelection(wstring& sel) {
    vector<ClipFormat> saved = saveClipboard();
    DWORD before = GetClipboardSequenceNumber();
    INPUT in[4] = {};
    for (int i = 0; i < 4; i++) { in[i].type = INPUT_KEYBOARD; in[i].ki.dwExtraInfo = kMagic; }
    in[0].ki.wVk = VK_CONTROL; in[1].ki.wVk = 'C'; in[2].ki.wVk = 'C'; in[2].ki.dwFlags = KEYEVENTF_KEYUP; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    for (int i = 0; i < 40 && GetClipboardSequenceNumber() == before; i++) Sleep(10);
    if (GetClipboardSequenceNumber() == before) {  // slow app: if the copy lands later, still put the clipboard back
        lateClip = std::move(saved); lateClipSeq = before; SetTimer(hwndMain, TIMER_LATE_CLIP, 600, nullptr);
        return false;
    }
    Sleep(20);
    bool ok = clipboardText(sel);
    restoreClipboard(saved);
    return ok && !sel.empty();
}

// VS Code, JetBrains IDEs and others copy the whole current line when nothing is selected —
// a single line ending in a line break is that, not a selection to convert.
static bool isCopiedLine(const wstring& s) {
    if (s.empty() || s.back() != L'\n') return false;
    size_t end = s.size() - 1; if (end && s[end - 1] == L'\r') end--;
    return s.find_first_of(L"\r\n") == end;
}

// cmd / PowerShell (conhost), Windows Terminal, Git Bash (mintty): Ctrl+C without a selection is an interrupt
static bool fgIsConsole() {
    wchar_t cls[64] = L""; GetClassNameW(GetForegroundWindow(), cls, 64);
    return !wcscmp(cls, L"ConsoleWindowClass") || !wcscmp(cls, L"CASCADIA_HOSTING_WINDOW_CLASS") || !wcscmp(cls, L"mintty");
}
static void cycleLayout() { if (!layouts.empty()) selectLayout(layouts[(layoutIndex(fgLayout()) + 1) % layouts.size()]); }

static void convertSelection(wstring text) {
    if (layouts.empty()) return;
    // clipboard line breaks are \r\n; typed as two characters they become two line breaks
    for (size_t i; (i = text.find(L"\r\n")) != wstring::npos;) text.erase(i, 1);
    int src;
    if (text == lastSelOut && lastSelLang < int(layouts.size())) src = lastSelLang;
    else {
        int cur = layoutIndex(fgLayout()), best = cur, bestScore = -1;
        for (size_t i = 0; i < layouts.size(); i++) {
            int sc = 0; for (wchar_t c : text) { vector<Key> k; if (IsCharAlphaW(c) && layouts[i].keysFor(c, k)) sc++; }
            if (sc > bestScore || (sc == bestScore && int(i) == cur)) { best = int(i); bestScore = sc; }
        }
        src = best;
    }
    int dst = int((src + 1) % layouts.size());
    // runs of convertible characters go through the keys (so accents compose), the rest stays
    wstring out; vector<Key> run;
    for (wchar_t c : text) {
        vector<Key> k;
        if (layouts[src].keysFor(c, k) && layouts[dst].maps(k)) { run.insert(run.end(), k.begin(), k.end()); continue; }
        out += layouts[dst].text(run) + c; run.clear();
    }
    out += layouts[dst].text(run);
    typeText(out);
    for (wchar_t c : out) if (!IS_LOW_SURROGATE(c)) sendKey(VK_LEFT, true);  // reselect: ⇧← moves by character, not wchar_t
    lastSelOut = out; lastSelLang = dst;
    selectLayout(layouts[dst]);
}

// MARK: - Sounds (synthesized, identical on Mac and Windows)
// A short quiet "tick" (fast attack, quick decay) for auto-correction, two rising ticks for double ⇧ —
// soft enough to stop noticing after a while.

static vector<BYTE> blip(std::initializer_list<double> freqs) {
    const int rate = 44100, toneLen = rate * 50 / 1000, gap = rate * 30 / 1000;
    vector<short> pcm; int i = 0;
    for (double f : freqs) {
        if (i++) pcm.insert(pcm.end(), gap, 0);
        for (int n = 0; n < toneLen; n++) {
            double env = std::min(1.0, n / 40.0) * exp(-n / (rate * 0.012));  // no click, ~12 ms decay
            pcm.push_back(short(sin(2 * 3.14159265358979 * f * n / rate) * env * 0.12 * 32767));
        }
    }
    vector<BYTE> w;
    auto u32 = [&](DWORD v) { for (int b = 0; b < 4; b++) w.push_back(BYTE(v >> (8 * b))); };
    auto u16 = [&](WORD v) { w.push_back(BYTE(v)); w.push_back(BYTE(v >> 8)); };
    auto tag = [&](const char* t) { w.insert(w.end(), t, t + 4); };
    tag("RIFF"); u32(DWORD(36 + pcm.size() * 2)); tag("WAVE"); tag("fmt ");
    u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    tag("data"); u32(DWORD(pcm.size() * 2));
    const BYTE* p = (const BYTE*)pcm.data(); w.insert(w.end(), p, p + pcm.size() * 2);
    return w;
}
static const vector<BYTE> sndAuto = blip({1568}), sndShift = blip({1318, 1760});
static void playSound(const vector<BYTE>& s) {
    if (S.sound) PlaySoundW((LPCWSTR)s.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static void onDoubleShift(Snapshot* s) {
    if (layouts.empty()) return;
    playSound(sndShift);
    if (!s->keys.empty()) {
        // first press: the most likely layout (as auto-correction would pick it), then the next ones in turn
        // remember where the word went ourselves: right after a switch the window may still report the old layout.
        // «Again» = no real key since the previous double ⇧ (the hook's key counter didn't move)
        static unsigned lastSeq = ~0u; static int lastIdx = -1;
        bool again = s->seq == lastSeq && lastIdx >= 0 && lastIdx < int(layouts.size());
        int curIdx = again ? lastIdx : layoutIndex(fgLayout());
        int best = again ? -1 : autoFixTarget(s->keys);
        int nIdx = best >= 0 ? best : (curIdx + 1) % int(layouts.size());
        lastSeq = s->seq; lastIdx = nIdx;
        const Layout& cur = layouts[curIdx];
        const Layout& n = layouts[nIdx];
        replaceIn(int(cur.text(s->keys).size()) + s->trailing, n.text(s->keys) + wstring(s->trailing, L' '), n);
        return;
    }
    if (fgIsConsole()) { cycleLayout(); return; }  // Ctrl+C there would interrupt the running command
    wstring sel;
    if (copySelection(sel) && !isCopiedLine(sel)) convertSelection(sel); else cycleLayout();
}

// one-letter words: a single letter before a fixed word is fixed too («z буду» → «я буду»),
// unless it is already a word where it was typed (English «a»)
static bool isLetterWord(const wstring& c, const wstring& locale) {
    static const std::map<wstring, wstring> w = {{L"uk", L"авзіїоуйя"}, {L"ru", L"авжикосуя"}, {L"en", L"ai"}};
    auto it = w.find(locale.substr(0, 2));
    return c.size() == 1 && it != w.end() && it->second.find(wchar_t(UINT_PTR(CharLowerW(LPWSTR(UINT_PTR(c[0])))))) != wstring::npos;
}

// «b tot» = «и еще»: «tot» is an English word, so alone it stays; but «b» isn't a word in English and
// «и» is in Russian, where «еще» is a real word too — the pair says which language was meant
static int pairFixTarget(const vector<Key>& keys, const vector<Key>& before) {
    if (before.size() != 1 || layouts.empty()) return -1;
    int cur = layoutIndex(fgLayout());
    if (isLetterWord(layouts[cur].text(before), layouts[cur].locale)) return -1;
    for (size_t j = 0; j < layouts.size(); j++) {
        if (int(j) == cur || !layouts[j].maps(keys) || !layouts[j].maps(before)) continue;
        wstring b = layouts[j].text(before), t = layouts[j].text(keys);
        if (!isLetterWord(b, layouts[j].locale) || consonantRun(t)) continue;
        const LangModel* lm = model(layouts[j].locale);
        if (dictionaryWorks(layouts[j].locale) ? isWord(t, layouts[j].locale) : (lm && lm->has(t))) return int(j);
    }
    return -1;
}

// Windows lets a keyboard hook see what's typed into password fields too: never touch a word there
// (UI Automation's IsPassword; asked only when a fix is about to happen)
static bool focusIsPassword() {
    static IUIAutomation* uia = nullptr;
    static const GUID clsid = {0xff48dba4, 0x60ef, 0x4201, {0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e}};  // CUIAutomation
    static const GUID iid = {0x30cbe57d, 0xd9d0, 0x452a, {0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee}};    // IUIAutomation
    if (!uia && FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid, (void**)&uia))) { uia = nullptr; return false; }
    IUIAutomationElement* el = nullptr; BOOL pw = FALSE;
    if (SUCCEEDED(uia->GetFocusedElement(&el)) && el) { el->get_CurrentIsPassword(&pw); el->Release(); }
    return pw;
}

// Backspace/Delete emptied the whole field (Ctrl+A, Delete): no word to go back into, so the first word typed now
// is checked as usual. Asked a moment after the key (the app has to handle it first), only if nothing was typed
// since; if the field can't tell (no Value/Text pattern), handsOff stays
static unsigned emptySeq = 0;
static const UINT_PTR TIMER_EMPTY = 0x7e57;
static bool focusIsEmpty() {
    static IUIAutomation* uia = nullptr;
    static const GUID clsid = {0xff48dba4, 0x60ef, 0x4201, {0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e}};
    static const GUID iid = {0x30cbe57d, 0xd9d0, 0x452a, {0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee}};
    if (!uia && FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid, (void**)&uia))) { uia = nullptr; return false; }
    IUIAutomationElement* el = nullptr; bool empty = false;
    if (SUCCEEDED(uia->GetFocusedElement(&el)) && el) {
        BOOL pw = FALSE; el->get_CurrentIsPassword(&pw);
        if (!pw) {
            IUnknown* p = nullptr;
            if (SUCCEEDED(el->GetCurrentPattern(UIA_TextPatternId, &p)) && p) {
                IUIAutomationTextPattern* tp = nullptr; IUIAutomationTextRange* r = nullptr; BSTR t = nullptr;
                if (SUCCEEDED(p->QueryInterface(__uuidof(IUIAutomationTextPattern), (void**)&tp)) && tp
                    && SUCCEEDED(tp->get_DocumentRange(&r)) && r && SUCCEEDED(r->GetText(1, &t)))
                    empty = SysStringLen(t) == 0 || (SysStringLen(t) == 1 && t[0] == 0xFFFC);  // Chrome: empty field = one U+FFFC
                if (t) SysFreeString(t);
                if (r) r->Release();
                if (tp) tp->Release();
                p->Release();
            } else {
                IUnknown* v = nullptr; BSTR t = nullptr;
                if (SUCCEEDED(el->GetCurrentPattern(UIA_ValuePatternId, &v)) && v) {
                    IUIAutomationValuePattern* vp = nullptr;
                    if (SUCCEEDED(v->QueryInterface(__uuidof(IUIAutomationValuePattern), (void**)&vp)) && vp && SUCCEEDED(vp->get_CurrentValue(&t)))
                        empty = SysStringLen(t) == 0;
                    if (t) SysFreeString(t);
                    if (vp) vp->Release();
                    v->Release();
                }
            }
        }
        el->Release();
    }
    return empty;
}

// puts the fixed word on screen: `trailing` = spaces typed after it (1 after a space, 0 before Return)
static void applyFix(int j, Snapshot* s, int trailing) {
    if (focusIsPassword()) return;
    wstring out = layouts[j].text(s->keys);
    const Layout& cur = layouts[layoutIndex(fgLayout())];
    wstring b = layouts[j].text(s->before), bTyped = cur.text(s->before);
    bool fixBefore = s->before.size() == 1 && b != bTyped && isLetterWord(b, layouts[j].locale) && !isLetterWord(bTyped, cur.locale);
    // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
    int shown = int(cur.text(s->keys).size());  // on screen: accented letters are one character
    wstring sp(trailing, L' ');
    if (fixBefore) replaceIn(int(bTyped.size()) + shown + 1 + trailing, b + L" " + out + sp, layouts[j]);
    else if (out != cur.text(s->keys)) replaceIn(shown + trailing, out + sp, layouts[j]);
    else selectLayout(layouts[j]);
    playSound(sndAuto);
}

// autoFixTarget + remembering the language of the last non-Latin word (fixed, or typed right)
static int autoFixRemember(const vector<Key>& keys, int pair) {
    int j = pair >= 0 ? pair : autoFixTarget(keys);
    if (j < 0 && lastWasWord && keys.size() >= 3) {
        const Layout& c = layouts[layoutIndex(fgLayout())];
        if (!isLatin(c.text(keys))) { recentLang = c.locale; recentAt = GetTickCount64(); }
    } else if (j >= 0 && !isLatin(layouts[j].text(keys))) { recentLang = layouts[j].locale; recentAt = GetTickCount64(); }
    return j;
}

static void onAuto(Snapshot* s) {
    if (!S.autoFix || S.bindings.count(fgExe())) return;
    // «b yt»: both «и не» (ru) and «не» alone (uk) fit; the pair that makes both words a language wins
    int j = autoFixRemember(s->keys, pairFixTarget(s->keys, s->before));
    if (j < 0 || seq.load() != s->seq) return;  // user kept typing meanwhile — skip
    applyFix(j, s, 1);
}

// Return held by the hook after a word: fix the word if it needs it, then send Return on
static void onEnter(Snapshot* s) {
    if (active && S.autoFix && !S.bindings.count(fgExe())) {
        int j = autoFixRemember(s->keys, pairFixTarget(s->keys, s->before));
        if (j >= 0) applyFix(j, s, 0);
    }
    // Return goes on as the user pressed it: same scan code, numpad Enter stays numpad Enter
    INPUT in[2] = {};
    for (int up = 0; up < 2; up++) {
        INPUT& i = in[up]; i.type = INPUT_KEYBOARD; i.ki.wVk = VK_RETURN; i.ki.dwExtraInfo = kMagic;
        i.ki.wScan = WORD(returnScan ? returnScan.load() : MapVirtualKeyW(VK_RETURN, MAPVK_VK_TO_VSC));
        i.ki.dwFlags = (returnFlags ? KEYEVENTF_EXTENDEDKEY : 0) | (up ? KEYEVENTF_KEYUP : 0);
    }
    SendInput(2, in, sizeof(INPUT));
}

// MARK: - Tray

static NOTIFYICONDATAW nid = {};
static wstring shownCode;

// Tray icon: language code in yellow on a rounded Ukrainian-blue tile (grey when off).
// Drawn at the exact tray size for the current DPI (Windows blurs a scaled-down icon),
// supersampled 4× for smooth letters and rounded corners.
static HICON makeTextIcon(const wstring& text, bool on, bool dot = false) {
    const int sz = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem()), k = 4, big = sz * k;
    const COLORREF bgc = on ? RGB(0x00, 0x57, 0xb7) : RGB(0x6b, 0x72, 0x80), fg = on ? RGB(0xff, 0xd5, 0x00) : RGB(0xff, 0xff, 0xff);

    BITMAPINFO bi = {}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), big, -big, 1, 32, BI_RGB};
    void* src; HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP hb = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &src, nullptr, 0);
    HGDIOBJ old = SelectObject(dc, hb);
    RECT rc{0, 0, big, big};
    HBRUSH br = CreateSolidBrush(bgc); FillRect(dc, &rc, br); DeleteObject(br);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, fg);
    // largest bold font that fits ~88% of the width
    for (int h = big * 3 / 4; h > 4; h -= k) {
        HFONT f = CreateFontW(-h, 0, 0, 0, FW_HEAVY, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
        HGDIOBJ of = SelectObject(dc, f);
        RECT m{0, 0, 0, 0}; DrawTextW(dc, text.c_str(), -1, &m, DT_CALCRECT | DT_SINGLELINE);
        if (m.right <= big * 88 / 100) { DrawTextW(dc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE); SelectObject(dc, of); DeleteObject(f); break; }
        SelectObject(dc, of); DeleteObject(f);
    }
    if (dot) {  // update reminder: red dot in the top-right corner, ringed with the tile colour
        auto circle = [&](double cx, double cy, double r, COLORREF c) {
            HBRUSH b = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, int(cx - r), int(cy - r), int(cx + r), int(cy + r));
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
        };
        circle(big * 0.78, big * 0.22, big * 0.19, bgc);
        circle(big * 0.78, big * 0.22, big * 0.13, RGB(0xff, 0x3b, 0x30));
    }

    BITMAPINFO si = {}; si.bmiHeader = {sizeof(BITMAPINFOHEADER), sz, -sz, 1, 32, BI_RGB};
    void* dst; HBITMAP color = CreateDIBSection(dc, &si, DIB_RGB_COLORS, &dst, nullptr, 0);
    const double r = big * 0.24;  // corner radius
    auto inside = [&](double x, double y) {
        double cx = x < r ? r : x > big - r ? big - r : x, cy = y < r ? r : y > big - r ? big - r : y;
        return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
    };
    for (int y = 0; y < sz; y++) for (int x = 0; x < sz; x++) {
        unsigned rs = 0, gs = 0, bs = 0, in = 0;
        for (int j = 0; j < k; j++) for (int i = 0; i < k; i++) {
            int X = x * k + i, Y = y * k + j;
            DWORD p = ((DWORD*)src)[Y * big + X];
            rs += (p >> 16) & 255; gs += (p >> 8) & 255; bs += p & 255;
            in += inside(X + 0.5, Y + 0.5);
        }
        unsigned n = k * k;
        ((DWORD*)dst)[y * sz + x] = ((in * 255 / n) << 24) | ((rs / n) << 16) | ((gs / n) << 8) | (bs / n);
    }
    SelectObject(dc, old); DeleteObject(hb); DeleteDC(dc);
    HBITMAP mask = CreateBitmap(sz, sz, 1, 1, nullptr);
    ICONINFO ii{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask); DeleteObject(color);
    return icon;
}

static void updateTray(bool force = false) {
    wstring code = layouts.empty() ? L"--" : layouts[layoutIndex(fgLayout())].code();
    bool due = updateDue();
    wstring key = code + (active ? L"1" : L"0") + (due ? L"u" : L"");
    if (!force && key == shownCode) return;
    shownCode = key;
    HICON old = nid.hIcon;
    nid.hIcon = makeTextIcon(code, active, due);
    wcscpy_s(nid.szTip, L"AURA Switcher " APP_VERSION L" beta");
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    if (old) DestroyIcon(old);
}

// MARK: - Login item

static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static bool loginEnabled() { return RegGetValueW(HKEY_CURRENT_USER, kRunKey, L"AURA", RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS; }
static void setLogin(bool on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t path[MAX_PATH]; GetModuleFileNameW(nullptr, path, MAX_PATH);
        wstring v = L"\"" + wstring(path) + L"\"";
        RegSetValueExW(k, L"AURA", 0, REG_SZ, (const BYTE*)v.c_str(), DWORD((v.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"AURA");
    RegCloseKey(k);
}

// MARK: - Update in one click
// Only when the user clicks «Check for updates…», never in the background: read the site's update.json and
// update.sig, check the signature (ECDSA P-256, private key offline with the maintainer — tools/update_manifest.py),
// download this architecture's exe from our GitHub Releases on a worker thread, check its SHA-256, put it in
// place of this exe (a running exe can be renamed) and start it. Anything goes wrong → the site's update page.
#if defined(__aarch64__) || defined(_M_ARM64)
static const char* kUpdArch = "win-arm64";
#else
static const char* kUpdArch = "win-x64";
#endif
static const UCHAR kUpdKey[64] = {   // public key (X, Y) of the update.json signature
    0xac, 0xff, 0x1e, 0xf6, 0x5b, 0xd8, 0xdb, 0x22, 0xa9, 0x07, 0x88, 0xc7, 0xe5, 0x01, 0x6e, 0x4a,
    0x1d, 0xe9, 0x49, 0x47, 0x49, 0x32, 0xe1, 0xa4, 0x63, 0xec, 0xbf, 0x71, 0x83, 0x2d, 0x85, 0xb4,
    0xcb, 0x5b, 0x01, 0x95, 0x5e, 0x75, 0x23, 0xf0, 0x58, 0xf6, 0xf4, 0x24, 0xfe, 0xa0, 0xf2, 0x16,
    0xed, 0x9d, 0xd9, 0xc3, 0x36, 0x55, 0x54, 0x72, 0xc2, 0xee, 0xd9, 0x28, 0x16, 0x8c, 0xb9, 0xc4};
static const char* kUpdDownloads = "https://github.com/klbk/aura-switcher/releases/download/";
static const size_t kUpdMaxExe = 50u << 20;
static HANDLE instanceMutex = nullptr;
static std::atomic<bool> updBusy{false};   // a check or a download is running
struct UpdateInfo { std::string version, url, sha256, data; bool ok = false; };

static bool httpGet(const wstring& url, std::string& out, size_t maxBytes) {
    URL_COMPONENTS u = {sizeof(u)}; wchar_t host[256], path[2048];
    u.lpszHostName = host; u.dwHostNameLength = 256; u.lpszUrlPath = path; u.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) return false;
    HINTERNET s = WinHttpOpen(L"AURA Switcher/" APP_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = r && WinHttpSendRequest(r, nullptr, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr);
    DWORD status = 0, len = sizeof(status);
    if (ok) ok = WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &len, nullptr) && status == 200;
    out.clear();
    for (DWORD n = 0; ok && WinHttpQueryDataAvailable(r, &n) && n; ) {
        if (out.size() + n > maxBytes) { ok = false; break; }   // far bigger than expected: not ours
        size_t at = out.size(); out.resize(at + n); DWORD got = 0;
        if (!WinHttpReadData(r, &out[at], n, &got)) ok = false;
        out.resize(at + got);
    }
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
    return ok;
}
// "key":"value" right after `after` (the manifest is ours and flat — no JSON library needed)
static std::string jsonStr(const std::string& j, const std::string& key, size_t after = 0) {
    size_t k = j.find("\"" + key + "\"", after); if (k == std::string::npos) return "";
    size_t a = j.find('"', j.find(':', k) + 1); size_t b = a == std::string::npos ? a : j.find('"', a + 1);
    return b == std::string::npos ? "" : j.substr(a + 1, b - a - 1);
}
static bool newerVersion(const std::string& a, const std::string& b) {
    int x[4] = {}, y[4] = {}; sscanf(a.c_str(), "%d.%d.%d.%d", x, x + 1, x + 2, x + 3); sscanf(b.c_str(), "%d.%d.%d.%d", y, y + 1, y + 2, y + 3);
    for (int i = 0; i < 4; i++) if (x[i] != y[i]) return x[i] > y[i];
    return false;
}
static bool sha256(const std::string& data, UCHAR h[32]) {
    BCRYPT_ALG_HANDLE alg;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) return false;
    bool ok = !BCryptHash(alg, nullptr, 0, (PUCHAR)data.data(), ULONG(data.size()), h, 32);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
static std::string sha256hex(const std::string& data) {
    UCHAR h[32]; std::string r; if (!sha256(data, h)) return r;
    char b[3]; for (UCHAR c : h) { snprintf(b, 3, "%02x", c); r += b; }
    return r;
}
// update.sig: hex of the raw ECDSA P-256 signature (r||s) of update.json's exact bytes
static bool manifestSigned(const std::string& json, const std::string& sigHex) {
    UCHAR sig[64], h[32];
    if (sigHex.size() < 128) return false;
    for (int i = 0; i < 64; i++) { unsigned v; if (sscanf(sigHex.c_str() + 2 * i, "%2x", &v) != 1) return false; sig[i] = UCHAR(v); }
    if (!sha256(json, h)) return false;
    struct { BCRYPT_ECCKEY_BLOB hdr; UCHAR xy[64]; } blob = {{BCRYPT_ECDSA_PUBLIC_P256_MAGIC, 32}, {}};
    memcpy(blob.xy, kUpdKey, 64);
    BCRYPT_ALG_HANDLE alg; BCRYPT_KEY_HANDLE key = nullptr; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0)) return false;
    if (!BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, (PUCHAR)&blob, sizeof(blob), 0))
        ok = !BCryptVerifySignature(key, nullptr, h, 32, sig, 64, 0);
    if (key) BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}
static DWORD WINAPI updateCheckThread(LPVOID) {
    auto* u = new UpdateInfo; std::string j, sig;
    if (httpGet(L"https://klbk.github.io/aura-switcher/update.json", j, 64 << 10) &&
        httpGet(L"https://klbk.github.io/aura-switcher/update.sig", sig, 1 << 10) && manifestSigned(j, sig)) {
        size_t sec = j.find(std::string("\"") + kUpdArch + "\"");
        u->version = jsonStr(j, "version");
        if (sec != std::string::npos) { u->url = jsonStr(j, "url", sec); u->sha256 = jsonStr(j, "sha256", sec); }
        u->ok = !u->version.empty() && u->url.rfind(kUpdDownloads, 0) == 0 && u->sha256.size() == 64;
    }
    PostMessageW(hwndMain, WM_UPDATE, 0, (LPARAM)u);
    return 0;
}
// worker thread: download the exe and check its hash; the swap itself happens on the main thread (WM_UPDATE_READY)
static DWORD WINAPI updateDownloadThread(LPVOID p) {
    auto* u = (UpdateInfo*)p;
    u->ok = httpGet(wstring(u->url.begin(), u->url.end()), u->data, kUpdMaxExe) && u->data.size() >= 100000 && sha256hex(u->data) == u->sha256;
    PostMessageW(hwndMain, WM_UPDATE_READY, 0, (LPARAM)u);
    return 0;
}
static void openUpdatePage() {
    wstring url = wstring(L"https://klbk.github.io/aura-switcher/update/?v=" APP_VERSION L"&l=") + UI_LANGS[uiIndex()];
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
static int askButtons(const wstring& text, std::initializer_list<const wchar_t*> buttons) {
    std::vector<TASKDIALOG_BUTTON> b; int id = 100; for (auto t : buttons) b.push_back({id++, t});
    TASKDIALOGCONFIG c = {sizeof(c)}; c.hwndParent = hwndMain; c.pszWindowTitle = L"AURA Switcher";
    c.pszMainInstruction = text.c_str(); c.pButtons = b.data(); c.cButtons = UINT(b.size()); c.dwFlags = TDF_POSITION_RELATIVE_TO_WINDOW;
    int pressed = 0; SetForegroundWindow(hwndMain);
    return SUCCEEDED(TaskDialogIndirect(&c, &pressed, nullptr, nullptr)) ? pressed - 100 : -1;
}
static void updateFailed() {
    updBusy = false;
    if (askButtons(L_(L"Couldn't update automatically. Open the website to download it?", L"Не вдалося оновити автоматично. Відкрити сайт, щоб завантажити?"),
                   {L"OK", L_(L"Later", L"Пізніше")}) == 0) openUpdatePage();
}
static bool installUpdate(const std::string& data) {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    wstring exe = self, fresh = exe + L".new", old = exe + L".old";
    HANDLE f = CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0; bool ok = WriteFile(f, data.data(), DWORD(data.size()), &w, nullptr) && w == data.size(); CloseHandle(f);
    if (!ok || !MoveFileExW(exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(fresh.c_str()); return false; }
    if (!MoveFileExW(fresh.c_str(), exe.c_str(), 0)) { MoveFileExW(old.c_str(), exe.c_str(), 0); return false; }
    if (instanceMutex) { CloseHandle(instanceMutex); instanceMutex = nullptr; }   // let the new copy start
    STARTUPINFOW si = {sizeof(si)}; PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        // the new exe didn't start: put this one back and keep running as the only copy
        DeleteFileW(exe.c_str()); MoveFileExW(old.c_str(), exe.c_str(), 0);
        instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\AURA-single-instance");
        return false;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
}
static void onUpdateInfo(UpdateInfo* u) {
    std::unique_ptr<UpdateInfo> keep(u);
    if (!u->ok) return updateFailed();
    std::string cur; for (const wchar_t* p = APP_VERSION; *p; p++) cur += char(*p);
    if (!newerVersion(u->version, cur)) {
        updBusy = false;
        askButtons(L_(L"You have the latest version", L"У вас остання версія"), {L"OK"});
        return;
    }
    wstring msg = L_(L"AURA Switcher {v} is out", L"Вийшла AURA Switcher {v}");
    size_t at = msg.find(L"{v}"); if (at != wstring::npos) msg.replace(at, 3, wstring(u->version.begin(), u->version.end()));
    int b = askButtons(msg, {L_(L"Update", L"Оновити"), L_(L"What's new", L"Що нового"), L_(L"Later", L"Пізніше")});
    if (b == 1) openUpdatePage();
    if (b != 0) { updBusy = false; return; }
    HANDLE t = CreateThread(nullptr, 0, updateDownloadThread, keep.get(), 0, nullptr);
    if (!t) return updateFailed();
    CloseHandle(t); keep.release();   // the thread posts it back
}
static void onUpdateReady(UpdateInfo* u) {
    std::unique_ptr<UpdateInfo> keep(u);
    if (u->ok && installUpdate(u->data)) PostQuitMessage(0);
    else updateFailed();
}
// the previous exe (renamed during an update) can be deleted only once its process has quit
static DWORD WINAPI deleteOldExe(LPVOID) {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    wstring old = wstring(self) + L".old";
    for (int i = 0; i < 30 && GetFileAttributesW(old.c_str()) != INVALID_FILE_ATTRIBUTES; i++)
        if (!DeleteFileW(old.c_str())) Sleep(1000);
    return 0;
}

// MARK: - Menu

// empty submenu: tell where it gets filled — «Порожньо — ↑ «Не працювати в цій програмі»»
static wstring emptyHint(const wchar_t* fillItem) {
    return wstring(L_(L"Empty", L"Порожньо")) + L" — ↑ «" + fillItem + L"»";
}

static wstring menuExe;  // app that was in front when the menu opened
static vector<wstring> menuExDel, menuBdDel, recentExe;  // recentExe: last apps, newest first
static bool fgGame = false;

static wstring langName(const wstring& locale) {
    wchar_t buf[128];
    if (GetLocaleInfoEx(locale.c_str(), LOCALE_SNATIVELANGUAGENAME, buf, 128)) { wstring s = buf; if (!s.empty()) s[0] = towupper(s[0]); return s; }
    return locale;
}

// MARK: - Word exceptions dialog (small modal window, no resources)

static LRESULT CALLBACK wordsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND && (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)) { SetWindowLongPtrW(h, GWLP_USERDATA, LOWORD(wp)); return 0; }
    if (msg == WM_CLOSE) { SetWindowLongPtrW(h, GWLP_USERDATA, IDCANCEL); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

static void editWords() {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = wordsProc; wc.hInstance = inst; wc.lpszClassName = L"AURAWords";
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc); registered = true;
    }
    int dpi = GetDpiForSystem(); auto D = [&](int v) { return MulDiv(v, dpi, 96); };
    NONCLIENTMETRICSW ncm = {sizeof(ncm)}; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -D(14);
    HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);

    RECT rc{0, 0, D(420), D(150)};
    AdjustWindowRectEx(&rc, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int w = rc.right - rc.left, hgt = rc.bottom - rc.top;
    POINT p; GetCursorPos(&p);
    HMONITOR mon = MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST); MONITORINFO mi = {sizeof(mi)}; GetMonitorInfoW(mon, &mi);
    int x = (mi.rcWork.left + mi.rcWork.right - w) / 2, y = (mi.rcWork.top + mi.rcWork.bottom - hgt) / 3;
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"AURAWords", L_(L"AURA Switcher — word exceptions", L"AURA Switcher — слова-винятки"),
                               WS_CAPTION | WS_SYSMENU, x, y, w, hgt, nullptr, nullptr, inst, nullptr);
    auto ctl = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int cx, int cy, int cw, int ch, int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, D(cx), D(cy), D(cw), D(ch), dlg, (HMENU)(INT_PTR)id, inst, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };
    ctl(L"STATIC", L_(L"AURA Switcher will never auto-correct these words. Separate them with commas.",
                      L"Ці слова AURA Switcher ніколи не виправлятиме автоматично. Через кому."), 0, 16, 14, 388, 40, 0);
    wstring cur; for (size_t i = 0; i < S.words.size(); i++) cur += (i ? L", " : L"") + S.words[i];
    HWND edit = ctl(L"EDIT", cur.c_str(), WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 16, 60, 388, 28, 100);
    SendMessageW(edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"хз, ща, яж");
    ctl(L"BUTTON", L_(L"Save", L"Зберегти"), WS_TABSTOP | BS_DEFPUSHBUTTON, 204, 106, 96, 30, IDOK);
    ctl(L"BUTTON", L_(L"Cancel", L"Скасувати"), WS_TABSTOP, 308, 106, 96, 30, IDCANCEL);

    ShowWindow(dlg, SW_SHOW); SetForegroundWindow(dlg); SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, -1);
    MSG m;
    while (!GetWindowLongPtrW(dlg, GWLP_USERDATA) && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if (GetWindowLongPtrW(dlg, GWLP_USERDATA) == IDOK) {
        int len = GetWindowTextLengthW(edit); wstring buf(len + 1, 0);
        GetWindowTextW(edit, buf.data(), len + 1); buf.resize(len);
        S.words = parseWords(buf);
    }
    DestroyWindow(dlg); DeleteObject(font);
}

// Opens an email to the developer with version, Windows build and layouts already filled in.
static wstring urlEncode(const wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string u(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u.data(), n, nullptr, nullptr); u.pop_back();
    wstring r; wchar_t buf[4];
    for (unsigned char c : u) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.') r += wchar_t(c);
        else { swprintf(buf, 4, L"%%%02X", c); r += buf; }
    }
    return r;
}
static void reportProblem() {
    wchar_t build[64] = L"?"; DWORD sz = sizeof(build);
    RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild", RRF_RT_REG_SZ, nullptr, build, &sz);
    wstring codes; for (auto& l : layouts) codes += (codes.empty() ? L"" : L", ") + l.code();
    wstring body = wstring(L_(L"What happened and in which app:", L"Що сталося і в якій програмі:")) +
                   L"\n\n\n---\nAURA Switcher " APP_VERSION L" · Windows build " + build + L" · " + codes;
    wstring url = L"mailto:auraswitcher@gmail.com?subject=" + urlEncode(L"AURA Switcher " APP_VERSION L" — Windows") + L"&body=" + urlEncode(body);
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void toggleExcluded(const wstring& exe) {
    if (S.isExcluded(exe)) { vector<wstring> r; for (auto& e : S.excluded) if (_wcsicmp(e.c_str(), exe.c_str())) r.push_back(e); S.excluded = r; }
    else S.excluded.push_back(exe);
}

// the menu hint has the translated ⇧ built in: show the chosen key's name instead (searching on from the end of each
// replacement, so the loop always ends)
static wstring hotkeyHint(wstring h, const wchar_t* sym) {
    for (size_t at = 0; (at = h.find(L"⇧", at)) != wstring::npos; at += wcslen(sym)) h.replace(at, 1, sym);
    return h;
}

static void showMenu() {
    menuExe = fgExe();
    HMENU m = CreatePopupMenu();
    auto item = [](HMENU h, UINT id, const wstring& t, bool checked = false, bool enabled = true) {
        AppendMenuW(h, MF_STRING | (checked ? MF_CHECKED : 0) | (enabled ? 0 : MF_GRAYED), id, t.c_str());
    };
    // on/off switches show ☑/☐ in the title, so the off state shows too (the native check mark is not used)
    auto tog = [&](HMENU h, UINT id, const wstring& t, bool on, bool enabled = true) { item(h, id, (on ? L"☑ " : L"☐ ") + t, false, enabled); };
    tog(m, ID_ENABLED, L_(L"Enabled", L"Увімкнено"), S.enabled);
    tog(m, ID_AUTO, L_(L"Auto-correction", L"Автовиправлення"), S.autoFix, S.enabled);
    tog(m, ID_SOUND, L_(L"Sound", L"Звук"), S.sound, S.enabled);
    // Store version: login start is the package's startup task, switched in Windows Settings → Apps → Startup
    tog(m, ID_LOGIN, L_(L"Launch at login", L"Запускати під час входу"), packaged() || loginEnabled());
    // one line: the hint with the chosen key's symbol in place of the translated ⇧; «Correction key: off» when it is off
    static const wchar_t* const hkSym[] = {L"Shift", L"Ctrl", L"Alt"};   // Windows shows key names, not symbols
    static const wchar_t* const hkKeys[] = {L"shift", L"ctrl", L"alt", L"off"};
    static const wchar_t* const hkNames[] = {L"Shift", L"Ctrl", L"Alt"};
    int hkI = 0; for (int i = 0; i < 4; i++) if (S.hotkey == hkKeys[i]) hkI = i;
    wstring hkTitle;
    if (hkI < 3) {
        hkTitle = L_(L"Double ⇧ — convert word/selection", L"Подвійний ⇧ — змінити мову слова/виділення");
        hkTitle = hotkeyHint(hkTitle, hkSym[hkI]);
    } else {
        wstring off = L_(L"Off", L"Вимкнено"); CharLowerBuffW(off.data(), (DWORD)off.size());
        hkTitle = wstring(L_(L"Correction key", L"Клавіша виправлення")) + L": " + off;
    }
    HMENU hm = CreatePopupMenu();
    item(hm, 0, L_(L"Correction key", L"Клавіша виправлення"), false, false);   // title of the submenu (not clickable)
    AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < 4; i++) item(hm, UINT(ID_HK0 + i), i < 3 ? wstring(hkNames[i]) : wstring(L_(L"Off", L"Вимкнено")));
    CheckMenuRadioItem(hm, ID_HK0, ID_HK0 + 3, ID_HK0 + hkI, MF_BYCOMMAND);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)hm, hkTitle.c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    if (!menuExe.empty()) {
        item(m, 0, wstring(L_(L"Current app: ", L"Поточна програма: ")) + menuExe, false, false);
        tog(m, ID_EXCLUDE, L_(L"Don't work in this app", L"Не працювати в цій програмі"), S.isExcluded(menuExe));
        HMENU lm = CreatePopupMenu();
        auto b = S.bindings.find(menuExe);
        item(lm, ID_NOBIND, L_(L"No binding", L"Без прив'язки"), b == S.bindings.end());
        for (size_t i = 0; i < layouts.size(); i++)
            item(lm, UINT(ID_BIND0 + i), langName(layouts[i].locale), b != S.bindings.end() && b->second == layouts[i].locale);
        AppendMenuW(m, MF_POPUP, (UINT_PTR)lm, L_(L"Language for this app", L"Мова для цієї програми"));
        if (fgGame) item(m, 0, L_(L"Game — AURA paused", L"Гра — AURA на паузі"), false, false);
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    }

    HMENU sm = CreatePopupMenu();
    HMENU em = CreatePopupMenu(); menuExDel = S.excluded;
    if (menuExDel.empty()) item(em, 0, emptyHint(L_(L"Don't work in this app", L"Не працювати в цій програмі")), false, false);
    for (size_t i = 0; i < menuExDel.size(); i++) item(em, UINT(ID_EXDEL0 + i), L"✕ " + menuExDel[i]);

    // a full-screen game hides the tray — let it be excluded after leaving it
    AppendMenuW(em, MF_SEPARATOR, 0, nullptr);
    if (recentExe.empty()) item(em, 0, L_(L"Empty", L"Порожньо"), false, false);
    for (size_t i = 0; i < recentExe.size(); i++) item(em, UINT(ID_RECENT0 + i), recentExe[i], S.isExcluded(recentExe[i]));
    AppendMenuW(sm, MF_POPUP, (UINT_PTR)em, L_(L"Exclusions", L"Винятки"));

    HMENU bm = CreatePopupMenu(); menuBdDel.clear();
    if (S.bindings.empty()) item(bm, 0, emptyHint(L_(L"Language for this app", L"Мова для цієї програми")), false, false);
    for (auto& kv : S.bindings) {
        item(bm, UINT(ID_BDDEL0 + menuBdDel.size()), L"✕ " + kv.first + L" → " + displayCode(kv.second));
        menuBdDel.push_back(kv.first);
    }
    AppendMenuW(sm, MF_POPUP, (UINT_PTR)bm, L_(L"Language bindings", L"Прив'язки мов"));

    item(sm, ID_WORDS, L_(L"My words…", L"Мої слова…"));
    HMENU um = CreatePopupMenu();
    item(um, ID_UI_SYS, L_(L"System", L"Системна"), S.uiLang.empty());
    // a few likely languages on top (current, English, the languages of the user's layouts), the rest by region
    AppendMenuW(um, MF_SEPARATOR, 0, nullptr);
    vector<int> top;
    auto addTop = [&](int i) { if (top.size() < 6 && std::find(top.begin(), top.end(), i) == top.end()) top.push_back(i); };
    addTop(uiIndex()); addTop(0);
    for (auto& l : layouts) for (int i = 0; i < UI_N; i++) if (l.locale.compare(0, 2, UI_LANGS[i]) == 0) addTop(i);
    for (int i : top) item(um, UINT(ID_UI0 + i), UI_NAMES[i], S.uiLang == UI_LANGS[i]);
    AppendMenuW(um, MF_SEPARATOR, 0, nullptr);
    HMENU allm = CreatePopupMenu();
    static const wchar_t* const regEn[] = {L"Europe", L"Caucasus & Central Asia", L"Middle East", L"Asia"};
    static const wchar_t* const regUk[] = {L"Європа", L"Кавказ і Центральна Азія", L"Близький Схід", L"Азія"};
    for (int r = 0; r < 4; r++) {
        HMENU rm = CreatePopupMenu(); int n = 0;
        for (int i = 0; i < UI_N; i++) if (uiRegion(UI_LANGS[i]) == r) { item(rm, UINT(ID_UI0 + i), UI_NAMES[i], S.uiLang == UI_LANGS[i]); n++; }
        if (n) AppendMenuW(allm, MF_POPUP, (UINT_PTR)rm, L_(regEn[r], regUk[r])); else DestroyMenu(rm);
    }
    AppendMenuW(um, MF_POPUP, (UINT_PTR)allm, L_(L"All languages", L"Усі мови"));
    AppendMenuW(sm, MF_POPUP, (UINT_PTR)um, L_(L"Menu language", L"Мова меню"));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)sm, L_(L"Settings", L"Налаштування"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    if (!packaged() && updBusy) item(m, 0, L_(L"Updating…", L"Оновлення…"), false, false);
    else item(m, ID_UPDATE, !packaged() && updateDue() ? L_(L"⬆︎ An update may be available — check", L"⬆︎ Можливо, вийшло оновлення — перевірити")
                                        : wstring(L_(L"Check for updates…", L"Перевірити оновлення…")) + L" (" APP_VERSION L" beta)");
    item(m, ID_REPORT, L_(L"Report a problem…", L"Повідомити про проблему…"));
    if (!packaged()) item(m, ID_DONATE, L_(L"Support AURA Switcher ☕", L"Підтримати AURA Switcher ☕"));  // Store: no outside payment links
    item(m, ID_QUIT, L_(L"Quit", L"Вихід"));

    POINT p; GetCursorPos(&p);
    SetForegroundWindow(hwndMain);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwndMain, nullptr);
    DestroyMenu(m);

    if (cmd == ID_ENABLED) { S.enabled = !S.enabled; }
    else if (cmd == ID_AUTO) { S.autoFix = !S.autoFix; }
    else if (cmd == ID_SOUND) { S.sound = !S.sound; playSound(sndAuto); }
    else if (cmd == ID_EXCLUDE) toggleExcluded(menuExe);
    else if (cmd >= ID_RECENT0 && cmd < ID_RECENT0 + recentExe.size()) toggleExcluded(recentExe[cmd - ID_RECENT0]);
    else if (cmd == ID_NOBIND) S.bindings.erase(menuExe);
    else if (cmd >= ID_BIND0 && cmd < ID_BIND0 + layouts.size()) S.bindings[menuExe] = layouts[cmd - ID_BIND0].locale;
    else if (cmd >= ID_EXDEL0 && cmd < ID_EXDEL0 + menuExDel.size()) {
        wstring d = menuExDel[cmd - ID_EXDEL0]; vector<wstring> r; for (auto& e : S.excluded) if (e != d) r.push_back(e); S.excluded = r;
    }
    else if (cmd >= ID_BDDEL0 && cmd < ID_BDDEL0 + menuBdDel.size()) S.bindings.erase(menuBdDel[cmd - ID_BDDEL0]);
    else if (cmd == ID_LOGIN) {
        if (packaged()) ShellExecuteW(nullptr, L"open", L"ms-settings:startupapps", nullptr, nullptr, SW_SHOWNORMAL);
        else setLogin(!loginEnabled());
    }
    else if (cmd == ID_WORDS) editWords();
    else if (cmd == ID_REPORT) reportProblem();
    else if (cmd == ID_UPDATE && packaged())   // Store: the Store updates the app; open its page (Update / Open button)
        ShellExecuteW(nullptr, L"open", L"ms-windows-store://pdp/?productid=9PP3MK8CFCJK", nullptr, nullptr, SW_SHOWNORMAL);
    else if (cmd == ID_UPDATE && !updBusy.exchange(true)) {
        updateChecked = (long long)time(nullptr); regSet(L"updateChecked", std::to_wstring(updateChecked));
        CloseHandle(CreateThread(nullptr, 0, updateCheckThread, nullptr, 0, nullptr));
    }
    else if (cmd == ID_DONATE) ShellExecuteW(nullptr, L"open", L"https://donatello.to/aura-switcher", nullptr, nullptr, SW_SHOWNORMAL);
    else if (cmd >= ID_HK0 && cmd < ID_HK0 + 4) S.hotkey = hkKeys[cmd - ID_HK0];
    else if (cmd == ID_UI_SYS) S.uiLang = L"";
    else if (cmd >= ID_UI0 && cmd < ID_UI0 + UI_N) S.uiLang = UI_LANGS[cmd - ID_UI0];
    else if (cmd == ID_QUIT) { PostQuitMessage(0); return; }
    if (cmd) { S.save(); updateTray(true); }
}

// MARK: - Foreground tracking

static wstring curExe;

// Full-screen game: Windows reports exclusive Direct3D full screen; borderless games are a captionless
// window covering the whole monitor. Double ⇧ is often sprint there, and the tray is hidden — pause.
static bool fullscreenGame() {
    QUERY_USER_NOTIFICATION_STATE st;
    if (SUCCEEDED(SHQueryUserNotificationState(&st)) && st == QUNS_RUNNING_D3D_FULL_SCREEN) return true;
    HWND fg = GetForegroundWindow();
    if (!fg || fg == GetShellWindow() || fg == GetDesktopWindow()) return false;
    wchar_t cls[32]; GetClassNameW(fg, cls, 32);
    if (!wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"Progman")) return false;
    // games are captionless, non-resizable and not "maximized"; apps with their own title bar (VS Code,
    // Discord) keep a resizable frame, so a maximized one with an auto-hidden taskbar isn't taken for a game
    LONG style = GetWindowLongW(fg, GWL_STYLE);
    if ((style & (WS_CAPTION | WS_THICKFRAME)) || IsZoomed(fg)) return false;
    // a browser in full screen (F11, video) is still a place to type
    static const wchar_t* const browsers[] = {L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe", L"opera.exe", L"vivaldi.exe"};
    wstring exe = fgExe();
    for (auto b : browsers) if (!_wcsicmp(exe.c_str(), b)) return false;
    RECT w; MONITORINFO mi = {sizeof(mi)};
    if (!GetWindowRect(fg, &w) || !GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    return w.left <= mi.rcMonitor.left && w.top <= mi.rcMonitor.top && w.right >= mi.rcMonitor.right && w.bottom >= mi.rcMonitor.bottom;
}

static void refreshActive() {
    curExe = fgExe();
    fgGame = fullscreenGame();
    active = S.enabled && !fgGame && !S.isExcluded(curExe);
}

static void CALLBACK onForeground(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    static wstring seenExe;  // own copy: the 300 ms timer also refreshes curExe and could hide the switch
    refreshActive();
    if (curExe == seenExe) return;
    seenExe = curExe;
    resetReq = true;
    static const wstring self = [] { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); wstring s = p; return s.substr(s.find_last_of(L'\\') + 1); }();
    if (!curExe.empty() && _wcsicmp(curExe.c_str(), self.c_str()) && _wcsicmp(curExe.c_str(), L"explorer.exe")) {
        vector<wstring> r = {curExe};
        for (auto& e : recentExe) if (_wcsicmp(e.c_str(), curExe.c_str()) && r.size() < 6) r.push_back(e);
        recentExe = r;
    }
    auto b = S.bindings.find(curExe);
    if (S.enabled && b != S.bindings.end())
        for (auto& l : layouts) if (l.locale == b->second) { selectLayout(l); break; }
}

// MARK: - Sticky Keys prompt
// Windows pops up "Sticky Keys" after 5 Shift presses — three double-⇧ in a row.
// Turn off only that shortcut, only for this session (not saved), and restore it on quit.

static STICKYKEYS savedSticky = {sizeof(STICKYKEYS)};
static bool stickyChanged = false;

static void disableStickyPrompt() {
    if (!SystemParametersInfoW(SPI_GETSTICKYKEYS, sizeof(savedSticky), &savedSticky, 0)) return;
    if (!(savedSticky.dwFlags & SKF_HOTKEYACTIVE)) return;
    STICKYKEYS sk = savedSticky;
    sk.dwFlags &= ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY);
    stickyChanged = SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(sk), &sk, 0) != 0;  // no SPIF_UPDATEINIFILE: not persisted
}
static void restoreStickyPrompt() {
    if (stickyChanged) SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(savedSticky), &savedSticky, 0);
}

// MARK: - Window

static UINT WM_TASKBARCREATED;

static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP) showMenu();
        return 0;
    case WM_AUTO: { auto* s = (Snapshot*)lp; if (active) onAuto(s); delete s; return 0; }
    case WM_DSHIFT: { auto* s = (Snapshot*)lp; if (active) onDoubleShift(s); delete s; return 0; }
    case WM_EMPTYCHK: emptySeq = unsigned(lp); SetTimer(h, TIMER_EMPTY, 60, nullptr); return 0;
    case WM_ENTER: { auto* s = (Snapshot*)lp; onEnter(s); delete s; return 0; }  // always sends Return on
    case WM_UPDATE: onUpdateInfo((UpdateInfo*)lp); return 0;
    case WM_UPDATE_READY: onUpdateReady((UpdateInfo*)lp); return 0;
    case WM_TIMER:
        if (wp == TIMER_LATE_CLIP) {
            KillTimer(h, TIMER_LATE_CLIP);
            if (GetClipboardSequenceNumber() != lateClipSeq) restoreClipboard(lateClip);
            lateClip.clear(); return 0;
        }
        if (wp == TIMER_EMPTY) {
            KillTimer(h, TIMER_EMPTY);
            if (seq.load() == emptySeq && focusIsEmpty()) resetReq = true;  // the hook drops handsOff before the next key
            return 0;
        }
        refreshActive(); updateTray(); return 0;
    case WM_INPUTLANGCHANGE: case WM_SETTINGCHANGE: {  // sent for any system setting: reload only if the list changed
        static vector<HKL> known;
        HKL list[64]; int n = GetKeyboardLayoutList(64, list);
        vector<HKL> now(list, list + std::max(n, 0));
        if (now != known) { known = now; layouts = loadLayouts(); lastSelOut.clear(); warmUpDictionaries(); }
        updateTray(true); return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    if (msg == WM_TASKBARCREATED) { Shell_NotifyIconW(NIM_ADD, &nid); updateTray(true); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

#ifndef AURA_TEST  // tests/windows.cpp includes this file with its own main
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // single instance: a second copy would double every correction
    instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\AURA-single-instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    CloseHandle(CreateThread(nullptr, 0, deleteOldExe, nullptr, 0, nullptr));   // left over from an update

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, __uuidof(ISpellCheckerFactory), (void**)&factory);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc = {}; wc.lpfnWndProc = wndProc; wc.hInstance = inst; wc.lpszClassName = L"AURA";
    RegisterClassW(&wc);
    hwndMain = CreateWindowW(L"AURA", L"AURA", 0, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);

    layouts = loadLayouts();
    disableStickyPrompt();
    warmUpDictionaries();
    // a real first launch: no AURA settings key yet (an update or restart always finds it)
    bool firstRun = false;
    { HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &k) == ERROR_SUCCESS) RegCloseKey(k); else firstRun = true; }
    if (!packaged() && regGet(L"loginSet", L"") != L"1") { setLogin(true); regSet(L"loginSet", L"1"); }

    nid.cbSize = sizeof(nid); nid.hWnd = hwndMain; nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = makeTextIcon(L"--", true);
    Shell_NotifyIconW(NIM_ADD, &nid);
    if (firstRun) {  // the icon starts hidden behind ^ — tell the user once where it is
        regSet(L"welcomed", L"1");
        NOTIFYICONDATAW n = {}; n.cbSize = sizeof(n); n.hWnd = hwndMain; n.uID = 1; n.uFlags = NIF_INFO; n.dwInfoFlags = NIIF_INFO;
        wcscpy_s(n.szInfoTitle, L"AURA Switcher");
        wcsncpy_s(n.szInfo, L_(L"AURA Switcher is running. Its icon is in the notification area; click ^ next to the clock to open the menu",
                               L"AURA Switcher працює. Значок — в області сповіщень; натисніть ^ біля годинника, щоб відкрити меню"), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &n);
    }
    refreshActive();
    updateTray(true);

    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onForeground, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetTimer(hwndMain, TIMER_TRAY, 300, nullptr);  // tray language code follows the active layout
    CreateThread(nullptr, 0, hookThread, nullptr, 0, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    Shell_NotifyIconW(NIM_DELETE, &nid);
    restoreStickyPrompt();
    for (auto& kv : checkers) if (kv.second) kv.second->Release();
    if (factory) factory->Release();
    CoUninitialize();
    return 0;
}
#endif
