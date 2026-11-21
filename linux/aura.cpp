// AURA Switcher for Linux — fixes text typed in the wrong keyboard layout.
// Made in Ukraine. Single file: evdev + uinput for the keys, libxkbcommon for the layouts, D-Bus (sd-bus) for the
// tray and the desktop. The engine is windows/aura.cpp's, function for function where the platform allows.
//
// Threads:
//   reader thread — reads key events from /dev/input (listen-only: never grabs a device, never delays a key)
//   main thread   — tray icon, menu, spell checking, text replacement
//
// Desktops: KDE Plasma (Wayland and X11) through org.kde.keyboard + a KWin script; any other X11 desktop through
// XKB (built with -DAURA_X11). A new desktop needs the five desk*() functions below and nothing else.

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <iconv.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <systemd/sd-bus.h>
#ifdef AURA_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#endif
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef APP_VERSION   // build.sh passes the version
#define APP_VERSION L"0.10.2"
#endif

using std::wstring;
using std::vector;

extern char** environ;

enum { ID_ENABLED = 1, ID_AUTO, ID_SOUND, ID_WORDS, ID_DONATE, ID_REPORT, ID_UPDATE, ID_EXCLUDE, ID_NOBIND, ID_LOGIN, ID_UI_SYS, ID_QUIT, ID_ACCESS,
       ID_BIND0 = 100, ID_EXDEL0 = 200, ID_BDDEL0 = 300, ID_RECENT0 = 400, ID_UI0 = 500, ID_UITOP0 = 600,
       ID_SEP0 = 700, ID_SUB0 = 800, ID_INFO0 = 900 };

// MARK: - Small helpers

static bool debugLog = getenv("AURA_DEBUG") != nullptr;
// decisions and counts only — never the text that was typed
static void logf(const char* fmt, ...) {
    if (!debugLog) return;
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    va_list a; va_start(a, fmt); fprintf(stderr, "aura %ld.%03ld: ", long(t.tv_sec % 1000), t.tv_nsec / 1000000); vfprintf(stderr, fmt, a); fputc('\n', stderr); va_end(a);
}
static long long nowMs() { timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000LL + t.tv_nsec / 1000000; }
static void pauseUs(long us) { timespec t{us / 1000000, (us % 1000000) * 1000}; nanosleep(&t, nullptr); }
static void pauseMs(int ms) { pauseUs(ms * 1000L); }

static std::string utf8(const wstring& w) {
    std::string s;
    for (wchar_t wc : w) {
        unsigned c = unsigned(wc);
        if (c < 0x80) s += char(c);
        else if (c < 0x800) { s += char(0xC0 | (c >> 6)); s += char(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { s += char(0xE0 | (c >> 12)); s += char(0x80 | ((c >> 6) & 0x3F)); s += char(0x80 | (c & 0x3F)); }
        else { s += char(0xF0 | (c >> 18)); s += char(0x80 | ((c >> 12) & 0x3F)); s += char(0x80 | ((c >> 6) & 0x3F)); s += char(0x80 | (c & 0x3F)); }
    }
    return s;
}
static wstring wide(const std::string& s) {
    wstring w;
    for (size_t i = 0; i < s.size();) {
        unsigned char b = (unsigned char)s[i]; unsigned c; int n;
        if (b < 0x80) { c = b; n = 0; } else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; n = 1; }
        else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; n = 2; } else if ((b & 0xF8) == 0xF0) { c = b & 0x07; n = 3; }
        else { i++; continue; }  // not UTF-8: skip the byte
        if (i + n >= s.size() + (n ? 0 : 1)) break;
        for (int k = 1; k <= n; k++) c = (c << 6) | ((unsigned char)s[i + k] & 0x3F);
        w += wchar_t(c); i += n + 1;
    }
    return w;
}
static wstring lower(wstring s) { for (auto& c : s) c = towlower(c); return s; }
static wstring upper(wstring s) { for (auto& c : s) c = towupper(c); return s; }
static bool isAlpha(wchar_t c) { return iswalpha(c) != 0; }

static std::string envOr(const char* name, const std::string& def) { const char* v = getenv(name); return v && *v ? v : def; }
static void makeDirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++) if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0700);
}
static std::string configDir() { return envOr("XDG_CONFIG_HOME", envOr("HOME", "") + "/.config"); }
static std::string runtimeDir() {
    static std::string d = [] { std::string r = envOr("XDG_RUNTIME_DIR", "/tmp/aura-" + std::to_string(getuid())) + "/aura-switcher"; makeDirs(r); return r; }();
    return d;
}
static bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    std::stringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}
static bool writeFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc); if (!f) return false;
    f << data; return bool(f);
}

// MARK: - Helper programs (clipboard, sound, dialogs, opening a link) — started directly, never through a shell

static bool have(const char* cmd) {
    static std::map<std::string, bool> known;
    auto it = known.find(cmd); if (it != known.end()) return it->second;
    bool ok = false; std::stringstream ss(envOr("PATH", "/usr/bin:/bin")); std::string d;
    while (!ok && std::getline(ss, d, ':')) ok = !d.empty() && access((d + "/" + cmd).c_str(), X_OK) == 0;
    return known[cmd] = ok;
}
// waits for the program: `in` goes to its stdin, its stdout comes back in `out`; false = failed or took too long
static bool run(const vector<std::string>& args, const std::string* in = nullptr, std::string* out = nullptr, int timeoutMs = 2000) {
    int pin[2] = {-1, -1}, pout[2] = {-1, -1};
    if ((in && pipe2(pin, O_CLOEXEC)) || (out && pipe2(pout, O_CLOEXEC))) return false;
    posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa);
    if (in) posix_spawn_file_actions_adddup2(&fa, pin[0], 0); else posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    if (out) posix_spawn_file_actions_adddup2(&fa, pout[1], 1); else posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    vector<char*> argv; for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str())); argv.push_back(nullptr);
    pid_t pid = 0; int err = posix_spawnp(&pid, argv[0], &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (in) close(pin[0]);
    if (out) close(pout[1]);
    if (err) { if (in) close(pin[1]); if (out) close(pout[0]); return false; }
    long long deadline = timeoutMs > 0 ? nowMs() + timeoutMs : 0;
    if (in) { size_t at = 0; while (at < in->size()) { ssize_t n = write(pin[1], in->data() + at, in->size() - at); if (n <= 0) break; at += size_t(n); } close(pin[1]); }
    bool ok = true;
    if (out) {
        out->clear(); char buf[65536];
        for (;;) {
            pollfd p{pout[0], POLLIN, 0};
            int left = deadline ? int(deadline - nowMs()) : -1;
            if (deadline && left <= 0) { ok = false; break; }
            if (poll(&p, 1, left) <= 0) { if (errno == EINTR) continue; ok = false; break; }
            ssize_t n = read(pout[0], buf, sizeof buf);
            if (n <= 0) break;
            out->append(buf, size_t(n));
            if (out->size() > (64u << 20)) { ok = false; break; }
        }
        close(pout[0]);
    }
    int st = 0;
    for (;;) {
        pid_t r = waitpid(pid, &st, deadline ? WNOHANG : 0);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) return false;
        if (deadline && (!ok || nowMs() > deadline)) { kill(pid, SIGKILL); waitpid(pid, &st, 0); return false; }
        if (deadline) pauseMs(2);
    }
    return ok && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
// starts the program and forgets it (a sound, a browser): detached, so it never becomes a zombie of ours
static void launch(const vector<std::string>& args) {
    vector<char*> argv; for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str())); argv.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        if (fork() == 0) {
            int n = open("/dev/null", O_RDWR); if (n >= 0) { dup2(n, 0); dup2(n, 1); dup2(n, 2); }
            execvp(argv[0], argv.data());
        }
        _exit(0);
    }
    if (pid > 0) waitpid(pid, nullptr, 0);
}
static void openUrl(const std::string& url) { launch({"xdg-open", url}); }

// MARK: - Settings (~/.config/aura-switcher/settings: one name=value per line)

static std::string settingsPath() { return configDir() + "/aura-switcher/settings"; }
static std::map<std::string, std::string>& conf() {
    static std::map<std::string, std::string> m;
    static bool loaded = false;
    if (!loaded) {
        loaded = true; std::string s;
        readFile(settingsPath(), s);
        std::stringstream ss(s); std::string line;
        while (std::getline(ss, line)) { size_t e = line.find('='); if (e != std::string::npos) m[line.substr(0, e)] = line.substr(e + 1); }
    }
    return m;
}
static wstring cfgGet(const char* name, const wstring& def) { auto it = conf().find(name); return it == conf().end() ? def : wide(it->second); }
static void cfgSet(const char* name, const wstring& v) { conf()[name] = utf8(v); }
static void cfgFlush() {
    std::string s; for (auto& kv : conf()) s += kv.first + "=" + kv.second + "\n";
    makeDirs(configDir() + "/aura-switcher");
    writeFile(settingsPath() + ".tmp", s); rename((settingsPath() + ".tmp").c_str(), settingsPath().c_str());
}
static vector<wstring> split(const wstring& s) {
    vector<wstring> r; size_t a = 0;
    while (a < s.size()) { size_t b = s.find(L';', a); if (b == wstring::npos) b = s.size(); if (b > a) r.push_back(s.substr(a, b - a)); a = b + 1; }
    return r;
}
static wstring join(const vector<wstring>& v) { wstring r; for (auto& s : v) r += s + L";"; return r; }

struct Settings {
    bool enabled = cfgGet("enabled", L"1") == L"1";
    bool autoFix = cfgGet("auto", L"1") == L"1";
    bool sound = cfgGet("sound", L"1") == L"1";
    wstring uiLang = cfgGet("uiLang", L"");            // "" = system
    vector<wstring> excluded = split(cfgGet("excluded", L""));
    vector<wstring> words = split(cfgGet("words", L""));  // never auto-corrected («хз», «ща»), lowercased
    std::map<wstring, wstring> bindings;                // app → layout
    Settings() { for (auto& p : split(cfgGet("bindings", L""))) { size_t e = p.find(L'='); if (e != wstring::npos) bindings[p.substr(0, e)] = p.substr(e + 1); } }
    void save() {
        cfgSet("enabled", enabled ? L"1" : L"0"); cfgSet("auto", autoFix ? L"1" : L"0"); cfgSet("sound", sound ? L"1" : L"0");
        cfgSet("uiLang", uiLang); cfgSet("excluded", join(excluded)); cfgSet("words", join(words));
        vector<wstring> b; for (auto& kv : bindings) b.push_back(kv.first + L"=" + kv.second); cfgSet("bindings", join(b));
        cfgFlush();
    }
    bool isExcluded(const wstring& app) { for (auto& e : excluded) if (!wcscasecmp(e.c_str(), app.c_str())) return true; return false; }
    bool isUserWord(const wstring& w) { for (auto& x : words) if (!wcscasecmp(x.c_str(), w.c_str())) return true; return false; }
};
static Settings S;

// "хз, ща яж" → {"хз", "ща", "яж"}
static vector<wstring> parseWords(const wstring& s) {
    vector<wstring> r; wstring cur;
    auto flush = [&] { if (!cur.empty()) { cur = lower(cur); bool dup = false; for (auto& x : r) dup |= x == cur; if (!dup) r.push_back(cur); cur.clear(); } };
    for (wchar_t c : s) { if (c == L',' || c == L';' || iswspace(c)) flush(); else cur += c; }
    flush();
    return r;
}

// MARK: - Localization (same strings as the Windows menu; Russian system → Ukrainian)

// BEGIN menu strings — generated by tools/menu_i18n.py, edit there
static const int UI_N = 48;
static const wchar_t* const UI_LANGS[UI_N] = {L"en", L"uk", L"pl", L"de", L"es", L"fr", L"it", L"pt", L"tr", L"nl", L"cs", L"sv", L"fi", L"ro", L"hu", L"el", L"bg", L"he", L"ar", L"hi", L"ko", L"be", L"kk", L"ky", L"ka", L"hy", L"az", L"sr", L"mk", L"sk", L"hr", L"sl", L"lt", L"lv", L"et", L"da", L"nb", L"is", L"sq", L"fa", L"vi", L"tg", L"mn", L"tt", L"uz", L"ga", L"mt", L"cy"};
static const wchar_t* const UI_NAMES[UI_N] = {L"English", L"Українська", L"Polski", L"Deutsch", L"Español", L"Français", L"Italiano", L"Português", L"Türkçe", L"Nederlands", L"Čeština", L"Svenska", L"Suomi", L"Română", L"Magyar", L"Ελληνικά", L"Български", L"עברית", L"العربية", L"हिन्दी", L"한국어", L"Беларуская", L"Қазақша", L"Кыргызча", L"ქართული", L"Հայերեն", L"Azərbaycan", L"Српски", L"Македонски", L"Slovenčina", L"Hrvatski", L"Slovenščina", L"Lietuvių", L"Latviešu", L"Eesti", L"Dansk", L"Norsk bokmål", L"Íslenska", L"Shqip", L"فارسی", L"Tiếng Việt", L"Тоҷикӣ", L"Монгол", L"Татарча", L"Oʻzbek", L"Gaeilge", L"Malti", L"Cymraeg"};
static const wchar_t* const UI_STR[][UI_N] = { // row: English first, then the other UI_LANGS
    {L"Enabled", L"Увімкнено", L"Włączone", L"Aktiviert", L"Activado", L"Activé", L"Attivo", L"Ativado", L"Etkin", L"Ingeschakeld", L"Zapnuto", L"Aktiverad", L"Käytössä", L"Activat", L"Bekapcsolva", L"Ενεργό", L"Включено", L"מופעל", L"مفعّل", L"चालू", L"켜기", L"Уключана", L"Қосулы", L"Күйгүзүлгөн", L"ჩართულია", L"Միացված է", L"Aktivdir", L"Укључено", L"Вклучено", L"Zapnuté", L"Uključeno", L"Vklopljeno", L"Įjungta", L"Ieslēgts", L"Sees", L"Aktiveret", L"På", L"Kveikt", L"Aktivizuar", L"روشن", L"Bật", L"Фаъол", L"Идэвхтэй", L"Кабызылган", L"Yoqilgan", L"Cumasaithe", L"Attivat", L"Wedi’i alluogi"},
    {L"Auto-correction", L"Автовиправлення", L"Autokorekta", L"Autokorrektur", L"Autocorrección", L"Correction automatique", L"Correzione automatica", L"Correção automática", L"Otomatik düzeltme", L"Autocorrectie", L"Automatické opravy", L"Autokorrigering", L"Automaattinen korjaus", L"Corectare automată", L"Automatikus javítás", L"Αυτόματη διόρθωση", L"Автокорекция", L"תיקון אוטומטי", L"التصحيح التلقائي", L"स्वतः सुधार", L"자동 수정", L"Аўтавыпраўленне", L"Автотүзету", L"Авто оңдоо", L"ავტოშესწორება", L"Ինքնաուղղում", L"Avtomatik düzəliş", L"Аутокорекција", L"Автокорекција", L"Automatické opravy", L"Automatski ispravak", L"Samodejni popravki", L"Automatinis taisymas", L"Automātiskā labošana", L"Automaatne parandus", L"Automatisk rettelse", L"Autokorrigering", L"Sjálfvirk leiðrétting", L"Korrigjim automatik", L"تصحیح خودکار", L"Tự động sửa", L"Ислоҳи худкор", L"Автомат засвар", L"Автоматик төзәтү", L"Avtomatik tuzatish", L"Ceartú uathoibríoch", L"Korrezzjoni awtomatika", L"Cywiro awtomatig"},
    {L"Sound", L"Звук", L"Dźwięk", L"Ton", L"Sonido", L"Son", L"Suono", L"Som", L"Ses", L"Geluid", L"Zvuk", L"Ljud", L"Ääni", L"Sunet", L"Hang", L"Ήχος", L"Звук", L"צליל", L"الصوت", L"ध्वनि", L"소리", L"Гук", L"Дыбыс", L"Үн", L"ხმა", L"Ձայն", L"Səs", L"Звук", L"Звук", L"Zvuk", L"Zvuk", L"Zvok", L"Garsas", L"Skaņa", L"Heli", L"Lyd", L"Lyd", L"Hljóð", L"Zëri", L"صدا", L"Âm thanh", L"Овоз", L"Дуу", L"Тавыш", L"Tovush", L"Fuaim", L"Ħoss", L"Sain"},
    {L"⚠︎ Grant access: Accessibility, Input Monitoring", L"⚠︎ Надайте доступ: Універсальний доступ, Моніторинг введення", L"⚠︎ Nadaj dostęp: Dostępność, Monitorowanie wejścia", L"⚠︎ Zugriff erlauben: Bedienungshilfen, Eingabeüberwachung", L"⚠︎ Conceder acceso: Accesibilidad, Monitorización de entrada", L"⚠︎ Autoriser l'accès : Accessibilité, Surveillance de l’entrée", L"⚠︎ Concedi l’accesso: Accessibilità, Monitoraggio input", L"⚠︎ Conceder acesso: Acessibilidade, Monitoramento de entrada", L"⚠︎ Erişim izni verin: Erişilebilirlik, Girdi İzleme", L"⚠︎ Geef toegang: Toegankelijkheid, Invoercontrole (Input Monitoring)", L"⚠︎ Udělte přístup: Zpřístupnění, Sledování vstupu", L"⚠︎ Ge åtkomst: Hjälpmedel, Indataövervakning (Input Monitoring)", L"⚠︎ Anna käyttöoikeus: Käyttöapu, Syötteen valvonta (Input Monitoring)", L"⚠︎ Acordați acces: Accesibilitate, Monitorizare intrare (Input Monitoring)", L"⚠︎ Hozzáférés megadása: Kisegítő lehetőségek, Bevitelfigyelés (Input Monitoring)", L"⚠︎ Δώστε πρόσβαση: Προσβασιμότητα, Παρακολούθηση εισόδου (Input Monitoring)", L"⚠︎ Дайте достъп: Достъпност, Input Monitoring", L"⚠︎ תנו גישה: נגישות, ניטור קלט (Input Monitoring)", L"⚠︎ امنح الوصول: تسهيلات الاستخدام، مراقبة الإدخال (Input Monitoring)", L"⚠︎ अनुमति दें: सुलभता, इनपुट मॉनिटरिंग (Input Monitoring)", L"⚠︎ 접근 권한 허용: 손쉬운 사용, 입력 모니터링", L"⚠︎ Дайце доступ: Спецыяльныя магчымасці, Input Monitoring", L"⚠︎ Рұқсат беріңіз: Арнайы мүмкіндіктер, Input Monitoring", L"⚠︎ Мүмкүнчүлүк бериңиз: Атайын мүмкүнчүлүктөр, Input Monitoring", L"⚠︎ მიეცით წვდომა: მისაწვდომობა, Input Monitoring", L"⚠︎ Տվեք թույլտվություն՝ Հասանելիություն, Input Monitoring", L"⚠︎ İcazə verin: Əlçatımlılıq, Input Monitoring", L"⚠︎ Дозволите приступ: Приступачност, Input Monitoring", L"⚠︎ Дозволете пристап: Пристапност, Input Monitoring", L"⚠︎ Povoľte prístup: Zjednodušenie ovládania, Monitorovanie vstupu (Input Monitoring)", L"⚠︎ Dopustite pristup: Pristupačnost, Praćenje unosa (Input Monitoring)", L"⚠︎ Dovolite dostop: Dostopnost, Input Monitoring", L"⚠︎ Suteikite prieigą: Pritaikymas neįgaliesiems, Input Monitoring", L"⚠︎ Atļaujiet piekļuvi: Pieejamība, Input Monitoring", L"⚠︎ Andke juurdepääs: Juurdepääsetavus, Input Monitoring", L"⚠︎ Giv adgang: Tilgængelighed, Inputovervågning (Input Monitoring)", L"⚠︎ Gi tilgang: Tilgjengelighet, Inndataovervåking (Input Monitoring)", L"⚠︎ Veittu aðgang: Algildur aðgangur, Input Monitoring", L"⚠︎ Jepni qasje: Përdorimi i lehtë, Input Monitoring", L"⚠︎ دسترسی بدهید: دسترسی‌پذیری، Input Monitoring", L"⚠︎ Cấp quyền: Trợ năng, Giám sát đầu vào (Input Monitoring)", L"⚠︎ Дастрасӣ диҳед: Дастрасии универсалӣ, Input Monitoring", L"⚠︎ Хандалт өгнө үү: Хүртээмж, Input Monitoring", L"⚠︎ Кереп җитү рөхсәт итегез: Махсус мөмкинлекләр, Input Monitoring", L"⚠︎ Ruxsat bering: Maxsus imkoniyatlar, Input Monitoring", L"⚠︎ Tabhair rochtain: Inrochtaineacht, Input Monitoring", L"⚠︎ Agħti aċċess: Aċċessibbiltà, Input Monitoring", L"⚠︎ Rhowch fynediad: Hygyrchedd, Input Monitoring"},
    {L"App: ", L"Програма: ", L"Aplikacja: ", L"App: ", L"App: ", L"App : ", L"App: ", L"App: ", L"Uygulama: ", L"App: ", L"Aplikace: ", L"App: ", L"Ohjelma: ", L"Aplicație: ", L"Alkalmazás: ", L"Εφαρμογή: ", L"Програма: ", L"יישום: ", L"التطبيق: ", L"ऐप: ", L"앱: ", L"Праграма: ", L"Қолданба: ", L"Колдонмо: ", L"აპლიკაცია: ", L"Հավելված՝ ", L"Tətbiq: ", L"Апликација: ", L"Апликација: ", L"Aplikácia: ", L"Aplikacija: ", L"Aplikacija: ", L"Programa: ", L"Lietotne: ", L"Rakendus: ", L"App: ", L"App: ", L"Forrit: ", L"Aplikacioni: ", L"برنامه: ", L"Ứng dụng: ", L"Барнома: ", L"Апп: ", L"Кушымта: ", L"Ilova: ", L"Aip: ", L"Applikazzjoni: ", L"Ap: "},
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
};
// END menu strings

static int uiIndex() {
    wstring code = S.uiLang;
    if (code.empty()) {
        std::string l = envOr("LC_ALL", envOr("LC_MESSAGES", envOr("LANG", "en")));
        code = wide(l).substr(0, 2);
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
// call sites keep English + Ukrainian inline for readability; the English text is the lookup key.
// Linux-only strings aren't in the table yet: they show in Ukrainian or in English
static const wchar_t* L_(const wchar_t* en, const wchar_t* uk) {
    int i = uiIndex();
    if (i == 1) return uk;
    for (auto& row : UI_STR) if (!wcscmp(row[0], en)) return row[i];
    return en;
}

// MARK: - Layouts

// caps (Caps Lock) only flips the case of letters, not digits or symbols; not part of comparison
// code: evdev key code — the physical key, the same in every layout
struct Key { uint16_t code; bool shift; bool caps = false; bool operator<(const Key& o) const { return code != o.code ? code < o.code : shift < o.shift; } bool operator==(const Key& o) const { return code == o.code && shift == o.shift; } };

// character keys: number row, three letter rows, the extra ISO key (the same keys windows/aura.cpp walks by scan code)
static bool isCharKey(int c) {
    return (c >= KEY_1 && c <= KEY_EQUAL) || (c >= KEY_Q && c <= KEY_RIGHTBRACE) || (c >= KEY_A && c <= KEY_GRAVE) ||
           (c >= KEY_BACKSLASH && c <= KEY_SLASH) || c == KEY_102ND;
}

// a layout as the desktop names it: xkb layout + variant («ua», «de» + «nodeadkeys»)
struct LayoutId { std::string layout, variant, desc; };

struct Layout {
    int group = 0;        // its number in the desktop's layout list — what the desktop switches by
    wstring locale;       // language + xkb layout: "uk-ua", "en-us"
    wstring name;         // the desktop's name for it («Ukrainian»), may be empty
    std::map<Key, wchar_t> toChar;
    std::map<wchar_t, Key> toKey;
    std::set<Key> dead;   // dead keys (accent typed before the letter)
    std::map<std::pair<Key, wchar_t>, wchar_t> composed;       // dead key + letter → the accented letter (´ e → é)
    std::map<wchar_t, std::pair<Key, wchar_t>> decomposed;     // and back
    static wchar_t cased(wchar_t c, const Key& k) { return k.caps && isAlpha(c) ? (k.shift ? towlower(c) : towupper(c)) : c; }  // Caps+Shift → lowercase
    // what the keys type here; a dead key and the letter after it make one accented letter (´ e → é)
    wstring text(const vector<Key>& keys) const {
        wstring s;
        for (size_t i = 0; i < keys.size(); i++) {
            const Key& k = keys[i];
            auto it = toChar.find(k); if (it == toChar.end()) continue;
            if (dead.count(k) && i + 1 < keys.size()) {
                auto n = toChar.find(keys[i + 1]);
                if (n != toChar.end()) {
                    auto c = composed.find({k, cased(n->second, keys[i + 1])});
                    if (c != composed.end()) { s += c->second; i++; continue; }
                }
            }
            s += cased(it->second, k);
        }
        return s;
    }
    bool maps(const vector<Key>& keys) const { for (auto& k : keys) if (!toChar.count(k)) return false; return true; }
    // keys that type a character here: one key, or a dead key + letter for an accented one
    bool keysFor(wchar_t c, vector<Key>& out) const {
        auto it = toKey.find(c);
        if (it != toKey.end()) { out.push_back(it->second); return true; }
        auto d = decomposed.find(c); if (d == decomposed.end()) return false;
        auto b = toKey.find(d->second.second); if (b == toKey.end()) return false;
        out.push_back(d->second.first); out.push_back(b->second); return true;
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
    return upper(c);
}
wstring Layout::code() const { return displayCode(locale); }

// xkb names layouts by country («ua», «gr»), the language models by language («uk», «el»).
// A layout that isn't here keeps its xkb name: it still converts on double ⇧, auto-correction just has no model for it
static wstring langOf(const LayoutId& n) {
    static const std::map<std::string, const char*> t = {
        {"us", "en"}, {"gb", "en"}, {"au", "en"}, {"nz", "en"}, {"za", "en"}, {"ie", "en"}, {"ie:CloGaelach", "ga"}, {"in:eng", "en"},
        {"ca:eng", "en"}, {"ca", "fr"}, {"fr", "fr"}, {"be", "nl"}, {"ch", "de"}, {"ch:fr", "fr"}, {"ch:fr_nodeadkeys", "fr"},
        {"de", "de"}, {"at", "de"}, {"es", "es"}, {"latam", "es"}, {"it", "it"}, {"pt", "pt"}, {"br", "pt"}, {"nl", "nl"},
        {"pl", "pl"}, {"cz", "cs"}, {"sk", "sk"}, {"hu", "hu"}, {"ro", "ro"}, {"md", "ro"}, {"hr", "hr"}, {"si", "sl"}, {"al", "sq"},
        {"se", "sv"}, {"fi", "fi"}, {"no", "nb"}, {"dk", "da"}, {"is", "is"}, {"ee", "et"}, {"lv", "lv"}, {"lt", "lt"}, {"mt", "mt"},
        {"gb:gla", "ga"}, {"tr", "tr"}, {"az", "az"}, {"vn", "vi"},
        {"ua", "uk"}, {"ru", "ru"}, {"ru:tt", "tt"}, {"by", "be"}, {"bg", "bg"}, {"mk", "mk"}, {"rs", "sr"},
        {"kz", "kk"}, {"kg", "ky"}, {"tj", "tg"}, {"mn", "mn"}, {"uz", "uz"}, {"ge", "ka"}, {"am", "hy"}, {"gr", "el"},
        {"il", "he"}, {"ara", "ar"}, {"eg", "ar"}, {"iq", "ar"}, {"sy", "ar"}, {"ma", "ar"}, {"dz", "ar"}, {"ir", "fa"},
        {"in", "hi"}, {"kr", "ko"}};
    auto it = t.find(n.layout + ":" + n.variant);
    if (it == t.end()) it = t.find(n.layout);
    return it == t.end() ? wide(n.layout).substr(0, 2) : wide(it->second);
}

static xkb_context* xkbCtx() { static xkb_context* c = xkb_context_new(XKB_CONTEXT_NO_FLAGS); return c; }
// dead key + letter → accented letter, the way applications compose them (the Compose table of the UTF-8 locales)
static wchar_t compose(xkb_keysym_t a, xkb_keysym_t b) {
    static xkb_compose_table* table = xkbCtx() ? xkb_compose_table_new_from_locale(xkbCtx(), "en_US.UTF-8", XKB_COMPOSE_COMPILE_NO_FLAGS) : nullptr;
    static xkb_compose_state* st = table ? xkb_compose_state_new(table, XKB_COMPOSE_STATE_NO_FLAGS) : nullptr;
    if (!st) return 0;
    xkb_compose_state_reset(st);
    xkb_compose_state_feed(st, a); xkb_compose_state_feed(st, b);
    if (xkb_compose_state_get_status(st) != XKB_COMPOSE_COMPOSED) return 0;
    char buf[32] = ""; xkb_compose_state_get_utf8(st, buf, sizeof buf);
    wstring w = wide(buf);
    return w.size() == 1 ? w[0] : 0;
}

// what each character key types in the layout (tests build layouts this way too)
static bool buildLayout(const LayoutId& n, int group, Layout& L) {
    if (!xkbCtx()) return false;
    xkb_rule_names names = {"evdev", "pc105", n.layout.c_str(), n.variant.c_str(), ""};
    xkb_keymap* km = xkb_keymap_new_from_names(xkbCtx(), &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!km) return false;
    xkb_state* st = xkb_state_new(km);
    xkb_mod_mask_t shiftMask = 1u << xkb_keymap_mod_get_index(km, XKB_MOD_NAME_SHIFT);
    L.group = group; L.locale = langOf(n) + L"-" + wide(n.layout); L.name = wide(n.desc);
    std::map<Key, xkb_keysym_t> syms;
    for (int code = KEY_1; code <= KEY_102ND; code++) {
        if (!isCharKey(code)) continue;
        for (int sh = 0; sh < 2; sh++) {
            xkb_state_update_mask(st, sh ? shiftMask : 0, 0, 0, 0, 0, 0);
            xkb_keysym_t sym = xkb_state_key_get_one_sym(st, xkb_keycode_t(code + 8));
            Key k{uint16_t(code), sh != 0};
            if (sym >= XKB_KEY_dead_grave && sym <= XKB_KEY_dead_greek) {  // dead key: shown as its spacing accent (´ ¨)
                wchar_t c = compose(sym, sym); if (!c) c = compose(sym, XKB_KEY_space);
                if (c) { L.toChar[k] = c; L.dead.insert(k); syms[k] = sym; }
                continue;
            }
            uint32_t c = xkb_keysym_to_utf32(sym);
            if (c < 0x20 || c == 0x7f) continue;
            L.toChar[k] = wchar_t(c); syms[k] = sym;
            if (!L.toKey.count(wchar_t(c))) L.toKey[wchar_t(c)] = k;
        }
    }
    for (auto& d : L.dead) for (auto& kv : L.toChar) {
        if (L.dead.count(kv.first) || !isAlpha(kv.second)) continue;
        wchar_t c = compose(syms[d], syms[kv.first]); if (!c) continue;
        L.composed[{d, kv.second}] = c;
        if (!L.toKey.count(c) && !L.decomposed.count(c)) L.decomposed[c] = {d, kv.second};
    }
    xkb_state_unref(st); xkb_keymap_unref(km);
    return !L.toChar.empty();
}

static vector<Layout> layouts;      // one per language (en-US and en-GB merged)
static vector<wstring> groupLang;   // the language of every layout in the desktop's list, by its number

// MARK: - Desktop: the layout list, the current layout, switching it, the app in front
// KDE Plasma (Wayland and X11): D-Bus service org.kde.keyboard, a KWin script reports the active window.
// Other X11 desktops: XKB groups and the EWMH properties of the root window.

enum Desk { DESK_NONE, DESK_KDE, DESK_X11 };
static Desk desk = DESK_NONE;
static sd_bus* bus = nullptr;
static const char* kBusName = "io.github.klbk.AuraSwitcher";
static int forcedLayout = -1;  // tests set the "active" layout here instead of switching the system's

#ifdef AURA_X11
static Display* dpy = nullptr;
static int xkbEventBase = 0;
static Window xActive = 0;
static Atom aActive, aRules, aPid, aState, aFull;
static int xIgnoreError(Display*, XErrorEvent*) { return 0; }  // a window may be gone by the time we ask about it
static std::string xProp(Window w, Atom a, unsigned long* items = nullptr) {
    Atom type; int fmt; unsigned long n = 0, left; unsigned char* data = nullptr; std::string r;
    if (XGetWindowProperty(dpy, w, a, 0, 4096, False, AnyPropertyType, &type, &fmt, &n, &left, &data) == Success && data) {
        r.assign((char*)data, n * (fmt == 32 ? sizeof(long) : size_t(fmt / 8)));
        XFree(data);
    }
    if (items) *items = n;
    return r;
}
static vector<LayoutId> x11Layouts() {  // _XKB_RULES_NAMES: rules, model, layout, variant, options — each ends with \0
    std::string raw = xProp(DefaultRootWindow(dpy), aRules);
    vector<std::string> part; size_t a = 0;
    while (a <= raw.size() && part.size() < 5) { size_t b = raw.find('\0', a); if (b == std::string::npos) b = raw.size(); part.push_back(raw.substr(a, b - a)); a = b + 1; }
    auto list = [](const std::string& s) { vector<std::string> r; size_t from = 0; for (;;) { size_t to = s.find(',', from); r.push_back(s.substr(from, to == std::string::npos ? to : to - from)); if (to == std::string::npos) break; from = to + 1; } return r; };
    vector<LayoutId> r;
    if (part.size() < 3 || part[2].empty()) return r;
    vector<std::string> l = list(part[2]), v = list(part.size() > 3 ? part[3] : "");
    for (size_t i = 0; i < l.size(); i++) r.push_back({l[i], i < v.size() ? v[i] : "", ""});
    return r;
}
static int x11Current() { XkbStateRec st; return XkbGetState(dpy, XkbUseCoreKbd, &st) == Success ? st.group : 0; }
static void x11Select(int g) { XkbLockGroup(dpy, XkbUseCoreKbd, unsigned(g)); XSync(dpy, False); }
#endif

static vector<LayoutId> kdeLayouts() {
    vector<LayoutId> v; sd_bus_message* r = nullptr;
    if (sd_bus_call_method(bus, "org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts", "getLayoutsList", nullptr, &r, "") < 0) return v;
    if (sd_bus_message_enter_container(r, 'a', "(sss)") > 0) {
        const char *a, *b, *c;
        while (sd_bus_message_read(r, "(sss)", &a, &b, &c) > 0) v.push_back({a, b, c});
    }
    sd_bus_message_unref(r);
    return v;
}
static int kdeCurrent() {
    sd_bus_message* r = nullptr; uint32_t i = 0;
    if (sd_bus_call_method(bus, "org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts", "getLayout", nullptr, &r, "") < 0) return 0;
    sd_bus_message_read(r, "u", &i); sd_bus_message_unref(r);
    return int(i);
}
static void kdeSelect(int g) {
    sd_bus_message* r = nullptr;
    sd_bus_call_method(bus, "org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts", "setLayout", nullptr, &r, "u", uint32_t(g));
    sd_bus_message_unref(r);
}

static vector<LayoutId> deskLayouts() {
    if (desk == DESK_KDE) return kdeLayouts();
#ifdef AURA_X11
    if (desk == DESK_X11) return x11Layouts();
#endif
    return {};
}
static int deskCurrent() {
    if (forcedLayout >= 0) return forcedLayout;
    if (desk == DESK_KDE) return kdeCurrent();
#ifdef AURA_X11
    if (desk == DESK_X11) return x11Current();
#endif
    return 0;
}
static void deskSelect(int g) {
    if (desk == DESK_KDE) kdeSelect(g);
#ifdef AURA_X11
    if (desk == DESK_X11) x11Select(g);
#endif
}
// how long a key is held before it starts repeating: a held key types an unknown number of letters
static int deskRepeatMs() {
    if (desk == DESK_KDE) {
        std::string s; int ms = 600;  // Plasma's default
        if (readFile(configDir() + "/kcminputrc", s)) {
            size_t sec = s.find("[Keyboard]"), k = sec == std::string::npos ? sec : s.find("RepeatDelay=", sec), next = sec == std::string::npos ? sec : s.find("\n[", sec);
            if (k != std::string::npos && (next == std::string::npos || k < next)) ms = atoi(s.c_str() + k + 12);
        }
        return ms > 0 ? ms : 600;
    }
#ifdef AURA_X11
    if (desk == DESK_X11) { unsigned d = 0, i = 0; if (XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &d, &i) && d) return int(d); }
#endif
    return 500;
}

static vector<Layout> loadLayouts() {
    vector<Layout> r; groupLang.clear();
    vector<LayoutId> names = deskLayouts();
    for (size_t i = 0; i < names.size(); i++) {
        Layout L;
        groupLang.push_back(langOf(names[i]));
        if (!buildLayout(names[i], int(i), L)) continue;
        bool dup = false; for (auto& o : r) if (o.locale.substr(0, 2) == L.locale.substr(0, 2)) dup = true;
        if (!dup) r.push_back(L);
    }
    return r;
}
// layouts are one per language (en-US and en-GB merged), so match by language, not by the layout's number
static int layoutIndex(int group) {
    for (size_t i = 0; i < layouts.size(); i++) if (layouts[i].group == group) return int(i);
    if (group >= 0 && group < int(groupLang.size()))
        for (size_t i = 0; i < layouts.size(); i++) if (layouts[i].locale.substr(0, 2) == groupLang[group]) return int(i);
    return 0;
}
static int curLayout() { return layoutIndex(deskCurrent()); }
static void selectLayout(const Layout& l) { if (forcedLayout < 0) deskSelect(l.group); }

// MARK: - Reader thread state (touched only by the reader thread)

static vector<Key> word, lastWord, before;  // before: one-letter word right before `word` («z буду»), fixed together with it
static int trailing = 0;
static std::atomic<unsigned> seq{0};            // bumps on every real key; main thread uses it to detect typing
static std::atomic<bool> resetReq{false}, active{true};
static std::atomic<bool> modsHeld{false};       // Shift, Ctrl, Alt or Super is down right now
static std::atomic<bool> capsNow{false};        // Caps Lock was on at the last letter typed
static std::atomic<int> repeatMs{500};
static std::atomic<int> keyboards{0}, denied{0};  // keyboards we read; keyboards and mice we weren't allowed to open
static long long shiftDownAt = 0, lastShiftTap = 0; static bool sawOther = false, shiftHeld = false;

struct Snapshot { vector<Key> keys; int trailing; unsigned seq; vector<Key> before; };

// the user went back into the text (Backspace over a space, arrows, a Ctrl/Alt/Super combo in the middle of a word):
// until the next space AURA neither checks nor converts what is typed — it's the user's own word
static bool handsOff = false;
static void resetState() { word.clear(); lastWord.clear(); before.clear(); trailing = 0; handsOff = false; }

// messages for the main thread: a small struct through a pipe
enum { MSG_AUTO = 1, MSG_DSHIFT, MSG_WORDS, MSG_DEVICES };
struct Msg { int type; void* p; };
static int msgPipe[2] = {-1, -1};
static void post(int type, void* p = nullptr) { Msg m{type, p}; if (write(msgPipe[1], &m, sizeof m) != sizeof m) { /* main thread is gone */ } }

// Our own keys come back to us when a key remapper (keyd, kanata, xremap…) grabs AURA's virtual keyboard and types
// them again through its own: the reader must not take them for the user's typing. Only then (echoMode) every key
// we send is remembered here and dropped when it shows up.
struct Echo { int code, value; long long at; };
static std::mutex echoMu; static std::deque<Echo> echo; static std::atomic<bool> echoMode{false};
static bool isEcho(int code, int value, long long now) {
    std::lock_guard<std::mutex> g(echoMu);
    while (!echo.empty() && now - echo.front().at > 1000) echo.pop_front();
    // the next expected key, or one a few places on: a remapper may swallow or change a key of ours
    for (size_t i = 0; i < echo.size() && i < 8; i++)
        if (echo[i].code == code && echo[i].value == value) { echo.erase(echo.begin(), echo.begin() + long(i) + 1); return true; }
    return false;
}

static const char* kVirtualName = "AURA Switcher virtual keyboard";
struct Dev { int fd; std::string path; bool keyboard, leds, touchpad; long long touchAt; };
static vector<Dev> devs;
static std::set<int> held;                    // keys down right now, all keyboards together
static std::map<int, long long> downAt;

static int deniedNow = 0;  // counted during a scan; `denied` gets the total at its end (the main thread reads it any time)
static bool bit(const unsigned long* a, int n) { return (a[n / (8 * sizeof(long))] >> (n % (8 * sizeof(long)))) & 1; }
// what a device we may not open is, from sysfs (readable by everyone): its key bits as hex words, highest first
static bool sysHasKey(const std::string& event, std::initializer_list<int> keys) {
    std::string s; if (!readFile("/sys/class/input/" + event + "/device/capabilities/key", s)) return false;
    vector<unsigned long> w; std::stringstream ss(s); std::string t;
    while (ss >> t) w.insert(w.begin(), strtoul(t.c_str(), nullptr, 16));
    for (int k : keys) { size_t i = size_t(k) / (8 * sizeof(long)); if (i < w.size() && ((w[i] >> (size_t(k) % (8 * sizeof(long)))) & 1)) return true; }
    return false;
}
static void openDevice(const std::string& path) {
    for (auto& d : devs) if (d.path == path) return;
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        // a keyboard or a mouse we can't read: half an access is none — a click we don't see moves the caret,
        // and the word would be replaced in the wrong place
        if (errno == EACCES && sysHasKey(path.substr(path.rfind('/') + 1), {KEY_A, BTN_LEFT, BTN_TOUCH})) deniedNow++;
        return;
    }
    char name[128] = ""; ioctl(fd, EVIOCGNAME(sizeof name), name);
    unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1] = {}, ev[EV_MAX / (8 * sizeof(long)) + 1] = {};
    ioctl(fd, EVIOCGBIT(0, sizeof ev), ev); ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys);
    bool kb = bit(keys, KEY_A) && bit(keys, KEY_SPACE) && bit(keys, KEY_ENTER);
    bool pointer = bit(keys, BTN_LEFT) || bit(keys, BTN_TOUCH);
    if (!strcmp(name, kVirtualName) || (!kb && !pointer)) { close(fd); return; }  // our own keys are not the user's typing
    devs.push_back({fd, path, kb, bit(ev, EV_LED), bit(keys, BTN_TOOL_FINGER), 0});
    if (kb) keyboards++;
}
static void scanDevices() {
    deniedNow = 0;
    DIR* d = opendir("/dev/input"); if (!d) return;
    while (dirent* e = readdir(d)) if (!strncmp(e->d_name, "event", 5)) openDevice(std::string("/dev/input/") + e->d_name);
    closedir(d);
    denied = deniedNow;
}

static void onKey(Dev& dev, int code, int value, long long now) {
    if (value == 1) { held.insert(code); downAt[code] = now; } else if (value == 0) held.erase(code);
    auto isHeld = [](std::initializer_list<int> ks) { for (int k : ks) if (held.count(k)) return true; return false; };
    bool mods = isHeld({KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTMETA, KEY_RIGHTMETA});
    bool shift = isHeld({KEY_LEFTSHIFT, KEY_RIGHTSHIFT});
    modsHeld = mods || shift;
    if (!active) return;
    if (resetReq.exchange(false)) resetState();
    if (value == 2) return;  // the kernel's own auto-repeat: applications repeat at the desktop's rate, see the key-up below
    bool down = value == 1;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        if (down) {
            if (!shiftHeld) { shiftHeld = true; shiftDownAt = now; sawOther = false; }
            if (mods) lastShiftTap = 0;
        } else {
            shiftHeld = shift;
            if (sawOther || mods || now - shiftDownAt > 300) lastShiftTap = 0;
            else if (lastShiftTap && now - lastShiftTap < 450) {
                lastShiftTap = 0;
                // the word being typed, or the previous one right after a single space; after two or more spaces
                // double ⇧ only switches the layout. In the user's own word it converts what was typed since the edit
                bool cur = !word.empty();
                post(MSG_DSHIFT, new Snapshot{cur ? word : trailing == 1 ? lastWord : vector<Key>(), cur ? 0 : trailing, seq.load(), {}});
            } else lastShiftTap = now;
        }
        return;
    }
    if (!down) {
        // held long enough to repeat («аааааа», a long Backspace): how many times is the application's business
        // (it repeats at the desktop's rate, not the kernel's) — from here on the word is the user's own
        if ((isCharKey(code) || code == KEY_BACKSPACE || code == KEY_SPACE) && now - downAt[code] >= repeatMs - 40) { resetState(); handsOff = true; }
        return;
    }

    sawOther = true; lastShiftTap = 0; seq++;
    if (mods) { bool mid = !word.empty(); resetState(); handsOff = mid; return; }

    if (code == KEY_BACKSPACE) {
        // a typo while the word is being typed: AURA keeps watching it. Back over the space into a finished word
        // (or into older text): the word is the user's now
        if (!word.empty() && trailing == 0) word.pop_back();
        else { resetState(); handsOff = true; }
    } else if (code == KEY_LEFT || code == KEY_RIGHT || code == KEY_UP || code == KEY_DOWN || code == KEY_HOME || code == KEY_END
               || code == KEY_PAGEUP || code == KEY_PAGEDOWN || code == KEY_DELETE) {
        resetState(); handsOff = true;  // editing the text
    } else if (handsOff) {
        // the user's own word: not checked, but its keys are kept — double ⇧ is an explicit command and converts it
        if (isCharKey(code)) word.push_back({uint16_t(code), shift, false});
        else resetState();  // a space, Return, Tab…: the user's word is over
    } else if (code == KEY_SPACE) {
        if (!word.empty()) {
            lastWord = word; word.clear(); trailing = 1;
            post(MSG_AUTO, new Snapshot{lastWord, 1, seq.load(), before});
        } else if (!lastWord.empty()) trailing++;
    } else if (isCharKey(code)) {
        if (word.empty()) before = lastWord.size() == 1 && trailing == 1 ? lastWord : vector<Key>();
        if (trailing > 0 || !lastWord.empty()) { lastWord.clear(); trailing = 0; }
        bool caps = false;  // the Caps Lock light of the keyboard the key came from: the desktop keeps it in step
        if (dev.leds) { unsigned long led[LED_MAX / (8 * sizeof(long)) + 1] = {}; if (ioctl(dev.fd, EVIOCGLED(sizeof led), led) >= 0) caps = bit(led, LED_CAPSL); }
        capsNow = caps;
        word.push_back({uint16_t(code), shift, caps});
    } else resetState();
}

static void readerThread() {
    int ino = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    inotify_add_watch(ino, "/dev/input", IN_CREATE | IN_ATTRIB | IN_DELETE);  // a new node gets its permissions a moment after it appears
    scanDevices();
    post(MSG_DEVICES);
    int told = keyboards * 1000 + denied;
    for (;;) {
        vector<pollfd> p; p.push_back({ino, POLLIN, 0});
        for (auto& d : devs) p.push_back({d.fd, POLLIN, 0});
        if (poll(p.data(), nfds_t(p.size()), -1) < 0) { if (errno == EINTR) continue; break; }
        long long now = nowMs();
        for (size_t i = devs.size(); i-- > 0;) {
            if (!p[i + 1].revents) continue;
            Dev& d = devs[i]; input_event ev[64]; ssize_t n;
            while ((n = read(d.fd, ev, sizeof ev)) > 0) {
                for (size_t k = 0; k < size_t(n) / sizeof(input_event); k++) {
                    const input_event& e = ev[k];
                    if (e.type != EV_KEY) continue;
                    if (e.code == BTN_LEFT || e.code == BTN_RIGHT || e.code == BTN_MIDDLE) { if (e.value == 1) { resetState(); lastShiftTap = 0; } }
                    else if (e.code == BTN_TOUCH) {
                        // a tap on the touchpad is a click the kernel doesn't report as a button; a touch screen: any touch
                        if (e.value == 1) d.touchAt = now;
                        if (!d.touchpad ? e.value == 1 : (e.value == 0 && now - d.touchAt < 300)) { resetState(); lastShiftTap = 0; }
                    }
                    else if (e.code < BTN_MISC || e.code >= KEY_OK) { if (!(echoMode && isEcho(e.code, e.value, now))) onKey(d, e.code, e.value, now); }
                }
            }
            if (n < 0 && errno != EAGAIN) {  // unplugged
                if (d.keyboard) keyboards--;
                close(d.fd); devs.erase(devs.begin() + long(i)); held.clear();
            }
        }
        if (p[0].revents) {
            char buf[4096]; while (read(ino, buf, sizeof buf) > 0) {}
            scanDevices();
        }
        if (keyboards * 1000 + denied != told) { told = keyboards * 1000 + denied; post(MSG_DEVICES); }
    }
}

// MARK: - Synthetic input (a uinput virtual keyboard: works the same on Wayland and X11)

static int ui = -1, uiNode = -1;  // uiNode: the virtual keyboard's own event node, to see whether a remapper grabbed it
static std::string uiNodePath;
static bool openUinput() {
    ui = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (ui < 0) return false;
    ioctl(ui, UI_SET_EVBIT, EV_KEY);
    for (int k = KEY_ESC; k <= KEY_KPDOT; k++) ioctl(ui, UI_SET_KEYBIT, k);  // an ordinary keyboard, so every desktop takes it for one
    ioctl(ui, UI_SET_KEYBIT, KEY_102ND); ioctl(ui, UI_SET_KEYBIT, KEY_INSERT); ioctl(ui, UI_SET_KEYBIT, KEY_LEFT);
    uinput_setup us = {}; us.id.bustype = BUS_VIRTUAL; us.id.vendor = 0x4155; us.id.product = 0x5241;
    snprintf(us.name, sizeof us.name, "%s", kVirtualName);
    if (ioctl(ui, UI_DEV_SETUP, &us) < 0 || ioctl(ui, UI_DEV_CREATE) < 0) { close(ui); ui = -1; return false; }
    char sys[64] = "";
    if (ioctl(ui, UI_GET_SYSNAME(sizeof sys), sys) >= 0) {
        std::string dir = std::string("/sys/devices/virtual/input/") + sys;
        if (DIR* d = opendir(dir.c_str())) {
            while (dirent* e = readdir(d)) if (!strncmp(e->d_name, "event", 5)) uiNodePath = std::string("/dev/input/") + e->d_name;
            closedir(d);
        }
    }
    return true;
}
static void closeUinput() { if (ui >= 0) { ioctl(ui, UI_DEV_DESTROY); close(ui); ui = -1; } }
// a remapper that grabbed our keyboard types our keys again through its own: then the reader has to drop them
static void checkEcho() {
    // the node gets its permissions a moment after it appears: opened here, not when the keyboard is created
    if (uiNode < 0 && !uiNodePath.empty()) uiNode = open(uiNodePath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (uiNode < 0) return;
    bool grabbed = ioctl(uiNode, EVIOCGRAB, 1) < 0 && errno == EBUSY;
    if (!grabbed) ioctl(uiNode, EVIOCGRAB, 0);
    if (grabbed != echoMode) logf("virtual keyboard %s by a key remapper", grabbed ? "is grabbed" : "is no longer grabbed");
    echoMode = grabbed;
}
static void emitKey(int code, int value) {
    if (ui < 0) return;
    if (echoMode) { std::lock_guard<std::mutex> g(echoMu); echo.push_back({code, value, nowMs()}); }
    input_event e[2] = {}; e[0].type = EV_KEY; e[0].code = uint16_t(code); e[0].value = value; e[1].type = EV_SYN; e[1].code = SYN_REPORT;
    if (write(ui, e, sizeof e) != sizeof e) logf("uinput write failed");
    pauseUs(700);  // a person's order, a machine's pace: applications take real keys in order, they only need them apart
}
static void tapKey(int code, bool shift = false) {
    if (shift) emitKey(KEY_LEFTSHIFT, 1);
    emitKey(code, 1); emitKey(code, 0);
    if (shift) emitKey(KEY_LEFTSHIFT, 0);
}
// the same physical keys again: in the layout that is active now they type the right text
static void typeKeys(const vector<Key>& keys) {
    bool sh = false;
    for (auto& k : keys) {
        if (k.shift != sh) { sh = k.shift; emitKey(KEY_LEFTSHIFT, sh); }
        emitKey(k.code, 1); emitKey(k.code, 0);
    }
    if (sh) emitKey(KEY_LEFTSHIFT, 0);
}
// Replace the word: switch the desktop to `l` first and wait until it has it, erase, then press the word's own
// keys again — in the right layout they type the right letters, dead keys and all. Nothing is erased unless the
// layout really switched.
static bool replaceIn(int count, const vector<Key>& keys, const Layout& l) {
    checkEcho();
    selectLayout(l);
    bool switched = false;
    for (int i = 0; i < 40 && !(switched = deskCurrent() == l.group); i++) pauseMs(5);
    if (!switched) { logf("layout didn't switch: nothing replaced"); return false; }
    for (int i = 0; i < count; i++) tapKey(KEY_BACKSPACE);
    typeKeys(keys);
    return true;
}

// MARK: - Spell checking (Hunspell dictionaries, if the system has the library and a dictionary for the language)
// Loaded at run time: without it (or without a dictionary) the language models below do the whole job.

static bool noSpell = getenv("AURA_NO_HUNSPELL") != nullptr;  // tests: the same results on every machine
struct Checker { void* h = nullptr; iconv_t cd = iconv_t(-1); };
static std::map<wstring, Checker> checkers;
static void* (*hunCreate)(const char*, const char*) = nullptr;
static int (*hunSpell)(void*, const char*) = nullptr;
static char* (*hunEncoding)(void*) = nullptr;

static bool hunspellLoad() {
    static int state = -1;
    if (state >= 0) return state == 1;
    state = 0;
    if (noSpell) return false;
    void* lib = nullptr;
    for (const char* n : {"libhunspell-1.7.so.0", "libhunspell-1.6.so.0", "libhunspell.so"}) if ((lib = dlopen(n, RTLD_NOW | RTLD_LOCAL))) break;
    if (!lib) return false;
    hunCreate = (void* (*)(const char*, const char*))dlsym(lib, "Hunspell_create");
    hunSpell = (int (*)(void*, const char*))dlsym(lib, "Hunspell_spell");
    hunEncoding = (char* (*)(void*))dlsym(lib, "Hunspell_get_dic_encoding");
    return (state = hunCreate && hunSpell && hunEncoding) == 1;
}
// «uk-ua» → /usr/share/hunspell/uk_UA.dic; the layout's own country first (en_GB for the British layout)
static std::string dictionaryFile(const wstring& locale) {
    std::string lang = utf8(locale.substr(0, 2)), country = locale.size() > 3 ? utf8(upper(locale.substr(3))) : "";
    vector<std::string> dirs; std::stringstream ss(envOr("DICPATH", "")); std::string d;
    while (std::getline(ss, d, ':')) if (!d.empty()) dirs.push_back(d);
    for (const char* p : {"/.local/share/hunspell", "/.local/share/myspell"}) dirs.push_back(envOr("HOME", "") + p);
    for (const char* p : {"/usr/share/hunspell", "/usr/share/myspell/dicts", "/usr/share/myspell", "/usr/local/share/hunspell"}) dirs.push_back(p);
    for (auto& dir : dirs) {
        vector<std::string> found;
        if (DIR* dp = opendir(dir.c_str())) {
            while (dirent* e = readdir(dp)) {
                std::string f = e->d_name;
                if (f.size() < 5 || f.compare(f.size() - 4, 4, ".dic") || f.compare(0, lang.size(), lang)) continue;
                std::string base = f.substr(0, f.size() - 4);
                if ((base.size() == lang.size() || base[lang.size()] == '_' || base[lang.size()] == '-') && access((dir + "/" + base + ".aff").c_str(), R_OK) == 0) found.push_back(base);
            }
            closedir(dp);
        }
        if (found.empty()) continue;
        std::sort(found.begin(), found.end());
        std::string up = lang; for (auto& c : up) c = char(toupper(c));
        for (const std::string& want : {lang + "_" + country, lang + "_" + up, lang + "_US", lang})
            for (auto& f : found) if (f == want) return dir + "/" + f;
        return dir + "/" + found[0];
    }
    return "";
}
static Checker* checkerFor(const wstring& locale) {
    auto it = checkers.find(locale);
    if (it != checkers.end()) return it->second.h ? &it->second : nullptr;
    Checker c;
    if (hunspellLoad()) {
        std::string base = dictionaryFile(locale);
        if (!base.empty() && (c.h = hunCreate((base + ".aff").c_str(), (base + ".dic").c_str()))) {
            const char* enc = hunEncoding(c.h);
            c.cd = iconv_open(enc && *enc ? enc : "UTF-8", "UTF-8");
            if (c.cd == iconv_t(-1)) c.h = nullptr;
            logf("dictionary for %s: %s", utf8(locale).c_str(), c.h ? "loaded" : "unusable encoding");
        }
    }
    checkers[locale] = c;
    return c.h ? &checkers[locale] : nullptr;
}
static bool spell(const wstring& w, const wstring& locale) {
    Checker* c = checkerFor(locale);
    if (!c) return false;
    std::string in = utf8(w), out(in.size() * 4 + 8, '\0');
    char* ip = &in[0]; char* op = &out[0]; size_t il = in.size(), ol = out.size() - 1;
    iconv(c->cd, nullptr, nullptr, nullptr, nullptr);
    if (iconv(c->cd, &ip, &il, &op, &ol) == size_t(-1)) return false;  // a letter the dictionary's alphabet doesn't have
    *op = '\0';
    return hunSpell(c->h, out.c_str()) != 0;
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
        // (not "lower case only": scripts without case — Hebrew, Arabic, Hindi, Korean — have no lower-case letters at all)
        std::set<wchar_t> set, own; for (auto& kv : l.toChar) if (isAlpha(kv.second) && !iswupper(kv.second)) { set.insert(kv.second); if (kv.second >= 0x250) own.insert(kv.second); }
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
    for (wchar_t c : w) if (!isAlpha(c) && c != L'\'' && c != L'-') return false;
    wstring low = lower(w);
    auto t = two.find(locale.substr(0, 2));
    if (w.size() == 2 && t != two.end()) return t->second.find(L" " + low + L" ") != wstring::npos;
    if (w == upper(w)) w = low;  // all caps passes any spell check
    if (!dictionaryWorks(locale)) return false;
    if (spell(w, locale)) return true;
    // names typed in lower case: «вінницею» is only in the dictionary as «Вінницею». Not for Latin
    // (English names like «Tot» would make gibberish a word) and not for short words
    if (w != low || w.size() < 5 || w[0] < 0x250) return false;
    wstring cap = w; cap[0] = towupper(cap[0]);
    return spell(cap, locale);
}

// five Cyrillic consonants in a row: gibberish the spell checker let through ("brachii," → «икфсршшб»)
static bool consonantRun(const wstring& s) {
    int run = 0;
    for (wchar_t c : s) {
        wchar_t l = towlower(c);
        run = l && wcschr(L"бвгґджзйклмнпрстфхцчшщ", l) ? run + 1 : 0;
        if (run >= 5) return true;
    }
    return false;
}

static bool isCyrillic(const wstring& s) { for (wchar_t c : s) if (c >= 0x400 && c <= 0x52F) return true; return false; }
static bool isLatin(const wstring& s) { for (wchar_t c : s) if (c >= 0x250) return false; return true; }

// MARK: - Language models
// For languages whose dictionary may be missing (lang/<code>.txt, built into the binary): the most frequent
// words, and letter-trigram costs (−10·ln p) that tell a word typed in the wrong layout from an English typo.
// A new file in lang/ goes into this list too (the Windows build lists them in windows/aura.rc).
#define AURA_LANGS(X) X(ar) X(az) X(be) X(bg) X(cs) X(cy) X(da) X(de) X(el) X(en) X(es) X(et) X(fa) X(fi) X(fr) X(ga) X(he) \
    X(hi) X(hr) X(hu) X(hy) X(is) X(it) X(ka) X(kk) X(ko) X(ky) X(lt) X(lv) X(mk) X(mn) X(mt) X(nb) X(nl) X(pl) X(pt) X(ro) \
    X(ru) X(sk) X(sl) X(sq) X(sr) X(sv) X(tg) X(tr) X(tt) X(uk) X(uz) X(vi)
// the assembler copies each file into the binary as it is (paths from the repository root: the build scripts run there)
#define AURA_LM(c) extern "C" const char aura_lm_##c[], aura_lm_##c##_end[]; \
    __asm__(".pushsection .rodata\n.global aura_lm_" #c "\naura_lm_" #c ":\n.incbin \"lang/" #c ".txt\"\n" \
            ".global aura_lm_" #c "_end\naura_lm_" #c "_end:\n.byte 0\n.popsection\n");
AURA_LANGS(AURA_LM)
struct LmData { const char* code; const char* begin; const char* end; };
#define AURA_LM_ROW(c) {#c, aura_lm_##c, aura_lm_##c##_end},
static const LmData kLmData[] = { AURA_LANGS(AURA_LM_ROW) };

struct LangModel {
    std::unordered_set<wstring> words; std::unordered_map<wstring, int> tri; int floor = 100;
    bool has(const wstring& w) const { return words.count(lower(w)) > 0; }
    double cost(wstring w) const {  // average trigram cost: lower = more typical of the language
        wstring c = L"^" + lower(w) + L"$";
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
    for (auto& d : kLmData) {
        if (wide(d.code) != code) continue;
        wstring s = wide(std::string(d.begin, d.end));
        m.reset(new LangModel());
        bool first = true; int maxCost = 0; size_t pos = 0;
        while (pos < s.size()) {
            size_t e = s.find(L'\n', pos); if (e == wstring::npos) e = s.size();
            wstring line = s.substr(pos, e - pos); pos = e + 1;
            if (line.empty() || line[0] == L'#') continue;
            if (first) { first = false; size_t a = 0; while (a < line.size()) { size_t b = line.find(L' ', a); if (b == wstring::npos) b = line.size(); if (b > a) m->words.insert(line.substr(a, b - a)); a = b + 1; } continue; }
            size_t t = line.find(L'\t');
            if (t != wstring::npos) { int v = int(wcstol(line.c_str() + t + 1, nullptr, 10)); m->tri[line.substr(0, t)] = v; maxCost = std::max(maxCost, v); }
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
static wstring recentLang; static long long recentAt = 0;
static bool lastWasWord = false;  // autoFixTarget found nothing because the word is right where it was typed
// tuned on ~2000 words per language (tools/langmodel.py): a word with no dictionary must look clearly
// more typical than what was typed; a real dictionary word only somewhat more
static const double kMarginNeeded = 20, kMarginForDictWord = 5, kMarginCyrillic = 11;  // Belarusian ↔ Russian: «длінный» 13, true «сіла» 4

// Code typed in the wrong layout — «сщтіщдуюдщп(ещефд)ж» is console.log(total); — is one "word" with
// dots and brackets: starts with a letter, and every part (split on non-letters and camelCase) is a word.
static bool isCode(const wstring& t, const wstring& locale) {
    if (t.empty() || !isAlpha(t[0])) return false;
    vector<wstring> parts; wstring cur;
    for (wchar_t c : t) {
        if (!isAlpha(c)) { if (!cur.empty()) parts.push_back(cur); cur.clear(); continue; }
        if (iswupper(c) && !cur.empty() && iswlower(cur.back())) { parts.push_back(cur); cur.clear(); }
        cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    size_t end = t.find_last_not_of(L";,.)");
    bool punctuated = false; for (size_t i = 0; end != wstring::npos && i <= end; i++) if (!isAlpha(t[i])) punctuated = true;
    bool long3 = false; for (auto& p : parts) { if (p.size() >= 3) long3 = true; if (p.size() < 2 || !isWord(p, locale)) return false; }
    return (parts.size() > 1 || punctuated) && long3;
}

// The first spell check in a language loads its dictionary (can take a moment). If that happens
// while the user types on, the correction is skipped — so load all dictionaries up front.
static void warmUpDictionaries() { reliable.clear(); for (auto& l : layouts) { isWord(L"aura", l.locale); model(l.locale); } }

static int autoFixTarget(const vector<Key>& keys) {
    lastWasWord = false;
    if (keys.size() < 2 || layouts.empty()) return -1;
    int cur = curLayout();
    const wstring& curLoc = layouts[cur].locale;
    bool curDict = dictionaryWorks(curLoc);
    if (!curDict && !model(curLoc)) return -1;  // typed in a language we can't check at all: may well be right
    wstring typed = layouts[cur].text(keys);
    if (S.isUserWord(typed)) return -1;
    // dictionaries accept many short letter groups («kai», «zum», «еру»): a short word the dictionary
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
    wstring recent = nowMs() - recentAt < 600000 ? recentLang : wstring();
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

#ifndef AURA_TEST  // tests/linux.cpp includes this file for the engine above and brings its own main

// MARK: - The app in front (exclusions, per-app language, games)

static wstring curApp;             // the window that takes the keys: "org.kde.konsole", "firefox"
static wstring userApp;            // the last one that wasn't the desktop shell or our own dialog: the menu is about it
static vector<wstring> recentApps; // newest first
static bool fgGame = false, sessionLocked = false;
static bool accessOk = false;      // the keyboards can be read and the virtual keyboard exists
static void updateTray(bool force = false);
static void menuChanged();

static bool isShellApp(const wstring& a) { return a.empty() || a == L"plasmashell" || a == L"kdialog" || a == L"org.kde.kdialog" || a == L"zenity"; }
// terminals: Ctrl+Insert there is not "copy", and a selection can't be replaced by typing over it
static bool isTerminal(const wstring& a) {
    for (const wchar_t* t : {L"konsole", L"yakuake", L"kitty", L"alacritty", L"foot", L"wezterm", L"ghostty", L"xterm", L"urxvt", L"terminator",
                             L"tilix", L"gnome-terminal", L"ptyxis", L"kgx", L"xfce4-terminal", L"st", L"rio", L"warp", L"terminal"})
        if (a == t || (a.size() > wcslen(t) && a.compare(a.size() - wcslen(t), wstring::npos, t) == 0 && a[a.size() - wcslen(t) - 1] == L'.')) return true;
    return false;
}
// the app's .desktop file says «Categories=…Game…» — what the Mac version reads from the app's category
static bool desktopSaysGame(const std::string& id) {
    static std::map<std::string, bool> known;
    auto it = known.find(id); if (it != known.end()) return it->second;
    bool game = false; std::string dirs = envOr("XDG_DATA_HOME", envOr("HOME", "") + "/.local/share") + ":" + envOr("XDG_DATA_DIRS", "/usr/local/share:/usr/share");
    std::stringstream ss(dirs); std::string d, s;
    while (!id.empty() && id.find('/') == std::string::npos && std::getline(ss, d, ':')) {
        if (!readFile(d + "/applications/" + id + ".desktop", s)) continue;
        size_t c = s.find("\nCategories=");
        if (c != std::string::npos) { std::string line = s.substr(c + 12, s.find('\n', c + 1) - c - 12); game = (";" + line + ";").find(";Game;") != std::string::npos; }
        break;
    }
    return known[id] = game;
}
// A game in full screen: double ⇧ is often sprint there, and the tray is hidden — pause.
// Full screen alone is not a game (a browser, a video, an editor), so the window also has to be one by its name
// (Steam), by its .desktop category, or by running under Wine/Proton.
static bool isGame(const std::string& cls, const std::string& desktopFile, int pid, bool full) {
    if (!full) return false;
    if (cls.compare(0, 10, "steam_app_") == 0 || cls == "gamescope") return true;
    if (desktopSaysGame(desktopFile.empty() ? cls : desktopFile)) return true;
    std::string cmd;
    if (pid > 0 && readFile("/proc/" + std::to_string(pid) + "/cmdline", cmd)) {
        std::string first = cmd.substr(0, cmd.find('\0'));
        if (first.size() > 4 && !strcasecmp(first.c_str() + first.size() - 4, ".exe")) return true;
    }
    return false;
}
static void refreshActive() {
    accessOk = ui >= 0 && keyboards > 0 && denied == 0;
    active = S.enabled && accessOk && desk != DESK_NONE && !fgGame && !sessionLocked && !S.isExcluded(curApp);
}
static void onActiveWindow(const std::string& cls, const std::string& desktopFile, int pid, bool full) {
    wstring app = lower(wide(cls));
    fgGame = isGame(cls, desktopFile, pid, full);
    bool changed = app != curApp;
    curApp = app;
    refreshActive();
    if (changed) {
        resetReq = true;
        if (!isShellApp(app)) {
            userApp = app;
            vector<wstring> r = {app};
            for (auto& e : recentApps) if (e != app && r.size() < 6) r.push_back(e);
            recentApps = r;
            menuChanged();
        }
        auto b = S.bindings.find(app);
        if (S.enabled && b != S.bindings.end())
            for (auto& l : layouts) if (l.locale == b->second) { selectLayout(l); break; }
    }
    updateTray();
}

// KWin has no D-Bus call for "which window is active", but it runs scripts: this one tells us on every switch.
// Only the window's app id, pid and full-screen flag — never its title.
static const char* kKwinPlugin = "aura-switcher";
static void kwinScript(bool load) {
    sd_bus_message* r = nullptr;
    sd_bus_call_method(bus, "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript", nullptr, nullptr, "s", kKwinPlugin);
    if (!load) return;
    std::string path = runtimeDir() + "/kwin-active-window.js";
    writeFile(path, std::string(
        "var hooked = {};\n"
        "function current() { return workspace.activeWindow || workspace.activeClient; }\n"   // Plasma 6 || Plasma 5
        "function report(w) {\n"
        "    callDBus('") + kBusName + "', '/App', '" + kBusName + "', 'ActiveWindow',\n"
        "             w ? '' + w.resourceClass : '', w ? '' + (w.desktopFileName || '') : '', w ? '' + w.pid : '0', w && w.fullScreen ? '1' : '0');\n"
        "}\n"
        "function onActive(w) {\n"
        "    if (w) {\n"
        "        var id = '' + (w.internalId || w.windowId);\n"
        "        if (!hooked[id]) { hooked[id] = true; w.fullScreenChanged.connect(function () { if (current() === w) report(w); }); }\n"
        "    }\n"
        "    report(w);\n"
        "}\n"
        "(workspace.windowActivated || workspace.clientActivated).connect(onActive);\n"
        "onActive(current());\n");
    int32_t id = -1;
    if (sd_bus_call_method(bus, "org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript", nullptr, &r, "ss", path.c_str(), kKwinPlugin) >= 0) {
        sd_bus_message_read(r, "i", &id); sd_bus_message_unref(r);
    }
    if (id < 0) { logf("KWin script didn't load: exclusions and per-app language are off"); return; }
    for (std::string p : {"/Scripting/Script" + std::to_string(id), "/" + std::to_string(id)})  // Plasma 6, Plasma 5
        if (sd_bus_call_method(bus, "org.kde.KWin", p.c_str(), "org.kde.kwin.Script", "run", nullptr, nullptr, "") >= 0) break;
}
static int onKwinWindow(sd_bus_message* m, void*, sd_bus_error*) {
    const char *cls, *file, *pid, *full;
    if (sd_bus_message_read(m, "ssss", &cls, &file, &pid, &full) < 0) return -EINVAL;
    onActiveWindow(cls, file, atoi(pid), full[0] == '1');
    return sd_bus_reply_method_return(m, "");
}

#ifdef AURA_X11
static void x11Active() {
    Window root = DefaultRootWindow(dpy); unsigned long n = 0;
    std::string a = xProp(root, aActive, &n);
    Window w = n ? Window(*(const unsigned long*)a.data()) : 0;
    if (w != xActive) { xActive = w; if (w) XSelectInput(dpy, w, PropertyChangeMask); }  // its full-screen state may change
    std::string cls; int pid = 0; bool full = false;
    if (w) {
        XClassHint h = {};
        if (XGetClassHint(dpy, w, &h)) { if (h.res_class) cls = h.res_class; if (h.res_name) XFree(h.res_name); if (h.res_class) XFree(h.res_class); }
        std::string p = xProp(w, aPid, &n); if (n) pid = int(*(const unsigned long*)p.data());
        std::string st = xProp(w, aState, &n);
        for (unsigned long i = 0; i < n; i++) if (Atom(((const unsigned long*)st.data())[i]) == aFull) full = true;
    }
    onActiveWindow(cls, "", pid, full);
}
#endif

// MARK: - Clipboard (wl-clipboard on Wayland, xclip or xsel on X11) — only for converting a selection

static int clipTool() {  // 0 none, 1 wl-clipboard, 2 xclip, 3 xsel
    static int t = -1;
    if (t < 0) t = getenv("WAYLAND_DISPLAY") && have("wl-copy") && have("wl-paste") ? 1 : getenv("WAYLAND_DISPLAY") ? 0 : have("xclip") ? 2 : have("xsel") ? 3 : 0;
    return t;
}
static bool clipText(std::string& out) {
    switch (clipTool()) {
    case 1: return run({"wl-paste", "-n", "-t", "text/plain;charset=utf-8"}, nullptr, &out, 500) || run({"wl-paste", "-n"}, nullptr, &out, 500);
    case 2: return run({"xclip", "-o", "-selection", "clipboard"}, nullptr, &out, 500);
    case 3: return run({"xsel", "-b", "-o"}, nullptr, &out, 500);
    }
    return false;
}
// `hidden`: our own short-lived text — clipboard managers (Klipper) are asked not to keep it in their history
static void clipSet(const std::string& data, const std::string& mime = "", bool hidden = false) {
    vector<std::string> a;
    switch (clipTool()) {
    case 1: a = {"wl-copy"}; if (hidden) a.push_back("--sensitive"); if (!mime.empty()) { a.push_back("-t"); a.push_back(mime); } break;
    case 2: a = {"xclip", "-i", "-selection", "clipboard"}; break;
    case 3: a = {"xsel", "-b", "-i"}; break;
    default: return;
    }
    run(a, &data, nullptr, 1000);
}
// The user's clipboard, to put back after the copy: its text, or — when it holds something else (an image, files) —
// its first format.
struct Clip { bool any = false; std::string mime, data; };
static Clip clipSave() {
    Clip c; std::string types;
    if (clipTool() == 1) {
        if (!run({"wl-paste", "-l"}, nullptr, &types, 500)) return c;
        std::stringstream ss(types); std::string t, first;
        while (std::getline(ss, t)) {
            if (t.empty() || t.find("x-kde-") != std::string::npos || t == "SAVE_TARGETS" || t == "TARGETS" || t == "TIMESTAMP" || t == "MULTIPLE") continue;
            if (first.empty()) first = t;
            if (t == "text/plain;charset=utf-8") { c.mime = t; break; }
        }
        if (c.mime.empty()) c.mime = first;
        c.any = !c.mime.empty() && run({"wl-paste", "-n", "-t", c.mime}, nullptr, &c.data, 2000);
    } else c.any = clipText(c.data);
    return c;
}
static void clipRestore(const Clip& c) {
    if (c.any) clipSet(c.data, c.mime);
    else if (clipTool() == 1) run({"wl-copy", "--clear"});
}

// later(ms, fn): run on the main thread after a while (putting the clipboard back once the application has pasted)
static vector<std::pair<long long, std::function<void()>>> timers;
static void later(int ms, std::function<void()> fn) { timers.push_back({nowMs() + ms, std::move(fn)}); }

static void chord(int mod, int key) { emitKey(mod, 1); emitKey(key, 1); emitKey(key, 0); emitKey(mod, 0); }

// The selected text: press Ctrl+Insert (copy in every toolkit, whatever the layout, and — unlike Ctrl+C — not an
// interrupt in a terminal we didn't recognise) and see whether the clipboard changed. No marker is put there first:
// clipboard managers (Klipper) keep and re-offer everything that appears on the clipboard, so a marker would end up
// in the user's history and race with the application's own copy. The price: a selection that is exactly what the
// clipboard already holds looks like no selection, and double ⇧ only switches the layout.
static std::string clipBefore; static bool clipHad = false;
static bool copySelection(wstring& sel, Clip& saved) {
    if (!clipTool()) return false;
    saved = clipSave();
    std::string got;
    clipHad = clipText(clipBefore);
    checkEcho();
    long long t0 = nowMs();
    chord(KEY_LEFTCTRL, KEY_INSERT);
    for (int i = 0; i < 30; i++) {
        pauseMs(10);
        if (clipText(got) && (!clipHad || got != clipBefore)) { sel = wide(got); logf("copy: selection of %d characters after %lld ms", int(sel.size()), nowMs() - t0); return !sel.empty(); }
    }
    logf("copy: nothing copied in %lld ms", nowMs() - t0);
    return false;
}
// a slow application: if its copy lands after we gave up, still put the user's clipboard back
static void restoreLateCopy(const Clip& saved) {
    std::string now; bool has = clipText(now);
    if (has && (!clipHad || now != clipBefore)) clipRestore(saved);
}

// VS Code, JetBrains IDEs and others copy the whole current line when nothing is selected —
// a single line ending in a line break is that, not a selection to convert.
static bool isCopiedLine(const wstring& s) {
    if (s.empty() || s.back() != L'\n') return false;
    size_t end = s.size() - 1; if (end && s[end - 1] == L'\r') end--;
    return s.find_first_of(L"\r\n") == end;
}

static void cycleLayout() { if (!layouts.empty()) selectLayout(layouts[size_t(curLayout() + 1) % layouts.size()]); }

static wstring lastSelOut; static int lastSelLang = 0;

static void convertSelection(wstring text) {
    if (layouts.empty()) return;
    for (size_t i; (i = text.find(L"\r\n")) != wstring::npos;) text.erase(i, 1);
    int src;
    if (text == lastSelOut && lastSelLang < int(layouts.size())) src = lastSelLang;
    else {
        int cur = curLayout(), best = cur, bestScore = -1;
        for (size_t i = 0; i < layouts.size(); i++) {
            int sc = 0; for (wchar_t c : text) { vector<Key> k; if (isAlpha(c) && layouts[i].keysFor(c, k)) sc++; }
            if (sc > bestScore || (sc == bestScore && int(i) == cur)) { best = int(i); bestScore = sc; }
        }
        src = best;
    }
    int dst = int(size_t(src + 1) % layouts.size());
    // runs of convertible characters go through the keys (so accents compose), the rest stays
    wstring out; vector<Key> run;
    for (wchar_t c : text) {
        vector<Key> k;
        if (layouts[src].keysFor(c, k) && layouts[dst].maps(k)) { run.insert(run.end(), k.begin(), k.end()); continue; }
        out += layouts[dst].text(run) + c; run.clear();
    }
    out += layouts[dst].text(run);
    // Typed with the new layout's own keys when it has them all: keys reach the application in order, so the
    // selection is replaced before the arrows select it again. A line break is not typed (Return sends the message
    // in a chat), nor anything with Caps Lock on.
    vector<Key> keys; bool typed = !capsNow;
    for (size_t i = 0; typed && i < out.size(); i++) {
        if (out[i] == L' ') keys.push_back({KEY_SPACE, false});
        else if (out[i] < 0x20 || !layouts[dst].keysFor(out[i], keys)) typed = false;
    }
    selectLayout(layouts[dst]);
    if (typed) { for (int i = 0; i < 40 && !(typed = deskCurrent() == layouts[dst].group); i++) pauseMs(5); }
    if (typed) typeKeys(keys);
    else {
        // pasted: the text may hold anything (line breaks, emoji, letters no layout has). Shift+Insert pastes in
        // every toolkit, whatever the layout; Backspace first — some (Tk) paste next to the selection, not over it
        std::string u = utf8(out), got;
        clipSet(u, "", true);
        for (int i = 0; i < 20 && !(clipText(got) && got == u); i++) pauseMs(5);
        tapKey(KEY_BACKSPACE);
        chord(KEY_LEFTSHIFT, KEY_INSERT);
        pauseMs(200);  // the application reads the clipboard before it gets the arrows
    }
    if (out.size() <= 400) { emitKey(KEY_LEFTSHIFT, 1); for (size_t i = 0; i < out.size(); i++) { emitKey(KEY_LEFT, 1); emitKey(KEY_LEFT, 0); } emitKey(KEY_LEFTSHIFT, 0); }  // reselect
    lastSelOut = out; lastSelLang = dst;
}

// MARK: - Sounds (synthesized, identical on Mac, Windows and Linux)
// A short quiet "tick" (fast attack, quick decay) for auto-correction, two rising ticks for double ⇧ —
// soft enough to stop noticing after a while.

static std::string blip(std::initializer_list<double> freqs) {
    const int rate = 44100, toneLen = rate * 50 / 1000, gap = rate * 30 / 1000;
    vector<short> pcm; int i = 0;
    for (double f : freqs) {
        if (i++) pcm.insert(pcm.end(), gap, 0);
        for (int n = 0; n < toneLen; n++) {
            double env = std::min(1.0, n / 40.0) * exp(-n / (rate * 0.012));  // no click, ~12 ms decay
            pcm.push_back(short(sin(2 * 3.14159265358979 * f * n / rate) * env * 0.12 * 32767));
        }
    }
    std::string w;
    auto u32 = [&](uint32_t v) { for (int b = 0; b < 4; b++) w += char(v >> (8 * b)); };
    auto u16 = [&](uint16_t v) { w += char(v); w += char(v >> 8); };
    w += "RIFF"; u32(uint32_t(36 + pcm.size() * 2)); w += "WAVEfmt ";
    u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    w += "data"; u32(uint32_t(pcm.size() * 2));
    w.append((const char*)pcm.data(), pcm.size() * 2);
    return w;
}
enum { SND_AUTO, SND_SHIFT };
static void playSound(int which) {
    if (!S.sound) return;
    static const std::string files[2] = {runtimeDir() + "/auto.wav", runtimeDir() + "/shift.wav"};
    static bool written = writeFile(files[0], blip({1568})) && writeFile(files[1], blip({1318, 1760}));
    if (!written) return;
    if (have("pw-play")) launch({"pw-play", files[which]});
    else if (have("paplay")) launch({"paplay", files[which]});
    else if (have("aplay")) launch({"aplay", "-q", files[which]});
}

// MARK: - Actions (main thread)

static void onDoubleShift(Snapshot* s) {
    if (layouts.empty()) return;
    playSound(SND_SHIFT);
    if (!s->keys.empty()) {
        // first press: the most likely layout (as auto-correction would pick it), then the next ones in turn
        // remember where the word went ourselves: «again» = no real key since the previous double ⇧
        // (the reader's key counter didn't move)
        static unsigned lastSeq = ~0u; static int lastIdx = -1;
        bool again = s->seq == lastSeq && lastIdx >= 0 && lastIdx < int(layouts.size());
        int curIdx = again ? lastIdx : curLayout();
        int best = again ? -1 : autoFixTarget(s->keys);
        int nIdx = best >= 0 ? best : (curIdx + 1) % int(layouts.size());
        lastSeq = s->seq; lastIdx = nIdx;
        vector<Key> keys = s->keys; for (int i = 0; i < s->trailing; i++) keys.push_back({KEY_SPACE, false});
        replaceIn(int(layouts[curIdx].text(s->keys).size()) + s->trailing, keys, layouts[nIdx]);
        updateTray();
        return;
    }
    Clip saved; wstring sel;
    if (isTerminal(curApp) || !clipTool()) cycleLayout();
    else if (copySelection(sel, saved) && !isCopiedLine(sel)) { convertSelection(sel); later(400, [saved] { clipRestore(saved); }); }
    else { cycleLayout(); later(600, [saved] { restoreLateCopy(saved); }); }
    updateTray();
}

// one-letter words: a single letter before a fixed word is fixed too («z буду» → «я буду»),
// unless it is already a word where it was typed (English «a»)
static bool isLetterWord(const wstring& c, const wstring& locale) {
    static const std::map<wstring, wstring> w = {{L"uk", L"авзіїоуйя"}, {L"ru", L"авжикосуя"}, {L"en", L"ai"}};
    auto it = w.find(locale.substr(0, 2));
    return c.size() == 1 && it != w.end() && it->second.find(towlower(c[0])) != wstring::npos;
}

// «b tot» = «и еще»: «tot» is an English word, so alone it stays; but «b» isn't a word in English and
// «и» is in Russian, where «еще» is a real word too — the pair says which language was meant
static int pairFixTarget(const vector<Key>& keys, const vector<Key>& before) {
    if (before.size() != 1 || layouts.empty()) return -1;
    int cur = curLayout();
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

// puts the fixed word on screen: `trailing` = spaces typed after it
static void applyFix(int j, Snapshot* s, int trailing) {
    wstring out = layouts[j].text(s->keys);
    const Layout& cur = layouts[curLayout()];
    wstring b = layouts[j].text(s->before), bTyped = cur.text(s->before);
    bool fixBefore = s->before.size() == 1 && b != bTyped && isLetterWord(b, layouts[j].locale) && !isLetterWord(bTyped, cur.locale);
    // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
    int shown = int(cur.text(s->keys).size());  // on screen: accented letters are one character
    vector<Key> keys;
    if (fixBefore) { keys = s->before; keys.push_back({KEY_SPACE, false}); }
    keys.insert(keys.end(), s->keys.begin(), s->keys.end());
    for (int i = 0; i < trailing; i++) keys.push_back({KEY_SPACE, false});
    if (fixBefore) replaceIn(int(bTyped.size()) + shown + 1 + trailing, keys, layouts[j]);
    else if (out != cur.text(s->keys)) replaceIn(shown + trailing, keys, layouts[j]);
    else selectLayout(layouts[j]);
    playSound(SND_AUTO);
    updateTray();
}

// autoFixTarget + remembering the language of the last non-Latin word (fixed, or typed right)
static int autoFixRemember(const vector<Key>& keys, int pair) {
    int j = pair >= 0 ? pair : autoFixTarget(keys);
    if (j < 0 && lastWasWord && keys.size() >= 3) {
        const Layout& c = layouts[curLayout()];
        if (!isLatin(c.text(keys))) { recentLang = c.locale; recentAt = nowMs(); }
    } else if (j >= 0 && !isLatin(layouts[j].text(keys))) { recentLang = layouts[j].locale; recentAt = nowMs(); }
    return j;
}

static void onAuto(Snapshot* s) {
    if (!S.autoFix || S.bindings.count(curApp)) return;
    // «b yt»: both «и не» (ru) and «не» alone (uk) fit; the pair that makes both words a language wins
    int j = autoFixRemember(s->keys, pairFixTarget(s->keys, s->before));
    // the user kept typing meanwhile, or holds Shift/Ctrl for what comes next (our keys would be typed with it) — skip
    if (j < 0 || seq.load() != s->seq || modsHeld) return;
    logf("fix: %d keys → layout %d", int(s->keys.size()), j);
    applyFix(j, s, 1);
}

// MARK: - Tray (StatusNotifierItem + DBusMenu, spoken directly over D-Bus)

static const char* kSniPath = "/StatusNotifierItem";
static const char* kMenuPath = "/MenuBar";
static std::string sniName, iconName;
static wstring shownCode;

// Tray icon: language code in yellow on a rounded Ukrainian-blue tile (grey when off). An SVG file the tray draws
// itself, so it is sharp at any panel size and scale; one file per code and state.
static std::string makeIcon(const wstring& text, bool on) {
    std::string name = "aura-switcher"; char hex[16];
    for (wchar_t c : text) { snprintf(hex, sizeof hex, "-%x", unsigned(c)); name += hex; }
    name += on ? "-on" : "-off";
    std::string path = runtimeDir() + "/" + name + ".svg";
    if (access(path.c_str(), R_OK) != 0) {
        std::string t = utf8(text), esc;
        for (char c : t) esc += c == '<' ? "&lt;" : c == '&' ? "&amp;" : std::string(1, c);
        writeFile(path, std::string("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\" viewBox=\"0 0 64 64\">"
            "<rect width=\"64\" height=\"64\" rx=\"15\" fill=\"") + (on ? "#0057b7" : "#6b7280") + "\"/>"
            "<text x=\"32\" y=\"45\" font-family=\"sans-serif\" font-weight=\"bold\" font-size=\"36\" text-anchor=\"middle\" fill=\"" +
            (on ? "#ffd500" : "#ffffff") + "\">" + esc + "</text></svg>\n");
    }
    return name;
}
static const wchar_t* tooltip() { return L"AURA Switcher " APP_VERSION L" beta"; }
static void updateTray(bool force) {
    if (!bus) return;
    wstring code = !accessOk || desk == DESK_NONE ? L"!" : layouts.empty() ? L"--" : layouts[curLayout()].code();
    wstring key = code + (active ? L"1" : L"0");
    if (!force && key == shownCode) return;
    shownCode = key;
    iconName = makeIcon(code, active);
    sd_bus_emit_signal(bus, kSniPath, "org.kde.StatusNotifierItem", "NewIcon", "");
    if (force) sd_bus_emit_signal(bus, kSniPath, "org.kde.StatusNotifierItem", "NewToolTip", "");
}
static int sniProp(sd_bus*, const char*, const char*, const char* prop, sd_bus_message* reply, void*, sd_bus_error*) {
    std::string p = prop;
    if (p == "Category") return sd_bus_message_append(reply, "s", "ApplicationStatus");
    if (p == "Id") return sd_bus_message_append(reply, "s", "aura-switcher");
    if (p == "Title") return sd_bus_message_append(reply, "s", "AURA Switcher");
    if (p == "Status") return sd_bus_message_append(reply, "s", "Active");
    if (p == "WindowId") return sd_bus_message_append(reply, "i", 0);
    if (p == "IconThemePath") return sd_bus_message_append(reply, "s", runtimeDir().c_str());
    if (p == "IconName") return sd_bus_message_append(reply, "s", iconName.c_str());
    if (p == "Menu") return sd_bus_message_append(reply, "o", kMenuPath);
    if (p == "ItemIsMenu") return sd_bus_message_append(reply, "b", 1);
    if (p == "ToolTip") {
        sd_bus_message_open_container(reply, 'r', "sa(iiay)ss");
        sd_bus_message_append(reply, "s", "");
        sd_bus_message_open_container(reply, 'a', "(iiay)"); sd_bus_message_close_container(reply);
        sd_bus_message_append(reply, "ss", utf8(tooltip()).c_str(), "");
        return sd_bus_message_close_container(reply);
    }
    if (p.find("Pixmap") != std::string::npos) { sd_bus_message_open_container(reply, 'a', "(iiay)"); return sd_bus_message_close_container(reply); }
    return sd_bus_message_append(reply, "s", "");  // overlay, attention: none
}
static int sniNoop(sd_bus_message* m, void*, sd_bus_error*) { return sd_bus_reply_method_return(m, ""); }
static const sd_bus_vtable kSniVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category", "s", sniProp, 0, 0), SD_BUS_PROPERTY("Id", "s", sniProp, 0, 0), SD_BUS_PROPERTY("Title", "s", sniProp, 0, 0),
    SD_BUS_PROPERTY("Status", "s", sniProp, 0, 0), SD_BUS_PROPERTY("WindowId", "i", sniProp, 0, 0),
    SD_BUS_PROPERTY("IconThemePath", "s", sniProp, 0, 0), SD_BUS_PROPERTY("Menu", "o", sniProp, 0, 0), SD_BUS_PROPERTY("ItemIsMenu", "b", sniProp, 0, 0),
    SD_BUS_PROPERTY("IconName", "s", sniProp, 0, 0), SD_BUS_PROPERTY("IconPixmap", "a(iiay)", sniProp, 0, 0),
    SD_BUS_PROPERTY("OverlayIconName", "s", sniProp, 0, 0), SD_BUS_PROPERTY("OverlayIconPixmap", "a(iiay)", sniProp, 0, 0),
    SD_BUS_PROPERTY("AttentionIconName", "s", sniProp, 0, 0), SD_BUS_PROPERTY("AttentionIconPixmap", "a(iiay)", sniProp, 0, 0),
    SD_BUS_PROPERTY("AttentionMovieName", "s", sniProp, 0, 0), SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", sniProp, 0, 0),
    SD_BUS_METHOD("Activate", "ii", "", sniNoop, SD_BUS_VTABLE_UNPRIVILEGED), SD_BUS_METHOD("SecondaryActivate", "ii", "", sniNoop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ContextMenu", "ii", "", sniNoop, SD_BUS_VTABLE_UNPRIVILEGED), SD_BUS_METHOD("Scroll", "is", "", sniNoop, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("NewTitle", "", 0), SD_BUS_SIGNAL("NewIcon", "", 0), SD_BUS_SIGNAL("NewAttentionIcon", "", 0), SD_BUS_SIGNAL("NewOverlayIcon", "", 0),
    SD_BUS_SIGNAL("NewToolTip", "", 0), SD_BUS_SIGNAL("NewStatus", "s", 0),
    SD_BUS_VTABLE_END};
// the tray may start after us, or restart (plasmashell --replace): register again whenever it appears
static void registerTray() {
    sd_bus_call_method(bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                       "RegisterStatusNotifierItem", nullptr, nullptr, "s", sniName.c_str());
}

// MARK: - Login item (~/.config/autostart)

static std::string autostartFile() { return configDir() + "/autostart/aura-switcher.desktop"; }
static bool loginEnabled() { return access(autostartFile().c_str(), R_OK) == 0; }
static void setLogin(bool on) {
    if (!on) { unlink(autostartFile().c_str()); return; }
    char self[4096]; ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1); if (n <= 0) return;
    makeDirs(configDir() + "/autostart");
    writeFile(autostartFile(), "[Desktop Entry]\nType=Application\nName=AURA Switcher\nComment=Fixes text typed in the wrong keyboard layout\n"
                               "Exec=\"" + std::string(self, size_t(n)) + "\"\nIcon=aura-switcher\nX-GNOME-Autostart-enabled=true\n");
}

// MARK: - Menu

struct MenuItem { int id = 0; wstring label; bool enabled = true; int check = -1; bool sep = false; vector<MenuItem> kids; };
static MenuItem menuRoot; static unsigned menuRev = 1;
static wstring menuApp;  // the app the open menu is about
static vector<wstring> menuExDel, menuBdDel, menuRecent;

static void menuChanged() {
    if (!bus) return;
    sd_bus_emit_signal(bus, kMenuPath, "com.canonical.dbusmenu", "LayoutUpdated", "ui", ++menuRev, 0);
}
// empty submenu: tell where it gets filled — «Порожньо — ↑ «Не працювати в цій програмі»»
static wstring emptyHint(const wchar_t* fillItem) { return wstring(L_(L"Empty", L"Порожньо")) + L" — ↑ «" + fillItem + L"»"; }
static wstring langName(const Layout& l) {
    wstring c = l.locale.substr(0, 2);
    if (c == L"ru") return L"Русский";
    for (int i = 0; i < UI_N; i++) if (c == UI_LANGS[i]) return UI_NAMES[i];
    return l.name.empty() ? l.locale : l.name;
}
static const wchar_t* accessText() {
    return desk == DESK_NONE ? L_(L"⚠︎ This desktop isn't supported yet — details", L"⚠︎ Це середовище поки не підтримується — докладніше")
                             : L_(L"⚠︎ No access to the keyboard — how to grant it", L"⚠︎ Немає доступу до клавіатури — як надати");
}

static void buildMenu() {
    menuApp = userApp;
    int info = ID_INFO0, sep = ID_SEP0, sub = ID_SUB0;
    auto item = [&](MenuItem& h, int id, const wstring& t, int check = -1, bool enabled = true) {
        MenuItem m; m.id = id ? id : info++; m.label = t; m.check = check; m.enabled = enabled && id != 0; h.kids.push_back(m);
    };
    auto separator = [&](MenuItem& h) { MenuItem m; m.id = sep++; m.sep = true; h.kids.push_back(m); };
    auto submenu = [&](MenuItem& h, const wstring& t) -> MenuItem& { MenuItem m; m.id = sub++; m.label = t; h.kids.push_back(m); return h.kids.back(); };
    MenuItem m;
    if (!accessOk || desk == DESK_NONE) { item(m, ID_ACCESS, accessText()); separator(m); }
    item(m, ID_ENABLED, L_(L"Enabled", L"Увімкнено"), S.enabled);
    item(m, ID_AUTO, L_(L"Auto-correction", L"Автовиправлення"), S.autoFix, S.enabled);
    item(m, ID_SOUND, L_(L"Sound", L"Звук"), S.sound, S.enabled);
    separator(m);

    if (!menuApp.empty()) {
        item(m, 0, wstring(L_(L"App: ", L"Програма: ")) + menuApp);
        item(m, ID_EXCLUDE, L_(L"Don't work in this app", L"Не працювати в цій програмі"), S.isExcluded(menuApp));
        { MenuItem& lm = submenu(m, L_(L"Language for this app", L"Мова для цієї програми"));
          auto b = S.bindings.find(menuApp);
          item(lm, ID_NOBIND, L_(L"No binding", L"Без прив'язки"), b == S.bindings.end());
          for (size_t i = 0; i < layouts.size(); i++) item(lm, int(ID_BIND0 + i), langName(layouts[i]), b != S.bindings.end() && b->second == layouts[i].locale); }
        if (fgGame) item(m, 0, L_(L"Game — AURA paused", L"Гра — AURA на паузі"));
        separator(m);
    } else sub++;  // submenu ids stay the same whether or not this one is shown

    { MenuItem& em = submenu(m, L_(L"Exclusions", L"Винятки")); menuExDel = S.excluded;
      if (menuExDel.empty()) item(em, 0, emptyHint(L_(L"Don't work in this app", L"Не працювати в цій програмі")));
      for (size_t i = 0; i < menuExDel.size(); i++) item(em, int(ID_EXDEL0 + i), L"✕ " + menuExDel[i]); }

    // a full-screen game hides the tray — let it be excluded after leaving it
    { MenuItem& rm = submenu(m, L_(L"Don't work in (recent apps)", L"Не працювати в (останні програми)")); menuRecent = recentApps;
      if (menuRecent.empty()) item(rm, 0, L_(L"Empty", L"Порожньо"));
      for (size_t i = 0; i < menuRecent.size(); i++) item(rm, int(ID_RECENT0 + i), menuRecent[i], S.isExcluded(menuRecent[i])); }

    { MenuItem& bm = submenu(m, L_(L"Language bindings", L"Прив'язки мов")); menuBdDel.clear();
      if (S.bindings.empty()) item(bm, 0, emptyHint(L_(L"Language for this app", L"Мова для цієї програми")));
      for (auto& kv : S.bindings) { item(bm, int(ID_BDDEL0 + menuBdDel.size()), L"✕ " + kv.first + L" → " + displayCode(kv.second)); menuBdDel.push_back(kv.first); } }
    separator(m);

    item(m, ID_LOGIN, L_(L"Launch at login", L"Запускати під час входу"), loginEnabled());
    item(m, ID_WORDS, L_(L"My words…", L"Мої слова…"), -1, have("kdialog") || have("zenity"));
    { MenuItem& um = submenu(m, L_(L"Menu language", L"Мова меню"));
      item(um, ID_UI_SYS, L_(L"System", L"Системна"), S.uiLang.empty());
      // a few likely languages on top (current, English, the languages of the user's layouts), the rest by region
      separator(um);
      vector<int> top;
      auto addTop = [&](int i) { if (top.size() < 6 && std::find(top.begin(), top.end(), i) == top.end()) top.push_back(i); };
      addTop(uiIndex()); addTop(0);
      for (auto& l : layouts) for (int i = 0; i < UI_N; i++) if (l.locale.compare(0, 2, UI_LANGS[i]) == 0) addTop(i);
      for (int i : top) item(um, ID_UITOP0 + i, UI_NAMES[i], S.uiLang == UI_LANGS[i]);
      separator(um);
      MenuItem& allm = submenu(um, L_(L"All languages", L"Усі мови"));
      static const wchar_t* const regEn[] = {L"Europe", L"Caucasus & Central Asia", L"Middle East", L"Asia"};
      static const wchar_t* const regUk[] = {L"Європа", L"Кавказ і Центральна Азія", L"Близький Схід", L"Азія"};
      for (int r = 0; r < 4; r++) {
          MenuItem& rm = submenu(allm, L_(regEn[r], regUk[r]));
          for (int i = 0; i < UI_N; i++) if (uiRegion(UI_LANGS[i]) == r) item(rm, ID_UI0 + i, UI_NAMES[i], S.uiLang == UI_LANGS[i]);
      } }
    item(m, 0, L_(L"Double ⇧ — convert word/selection", L"Подвійний ⇧ — змінити мову слова/виділення"));
    item(m, ID_UPDATE, L_(L"Check for updates…", L"Перевірити оновлення…"));
    item(m, ID_REPORT, L_(L"Report a problem…", L"Повідомити про проблему…"));
    item(m, ID_DONATE, L_(L"Support AURA Switcher ☕", L"Підтримати AURA Switcher ☕"));
    item(m, 0, L"AURA Switcher " APP_VERSION L" beta · 🇺🇦");
    item(m, ID_QUIT, L_(L"Quit", L"Вихід"));
    menuRoot = m;
}
static const MenuItem* findItem(const MenuItem& from, int id) {
    if (from.id == id) return &from;
    for (auto& k : from.kids) if (const MenuItem* f = findItem(k, id)) return f;
    return nullptr;
}
// a{sv}: the item's properties, as DBusMenu names them
static void putProps(sd_bus_message* m, const MenuItem& it) {
    sd_bus_message_open_container(m, 'a', "{sv}");
    if (it.sep) sd_bus_message_append(m, "{sv}", "type", "s", "separator");
    else if (it.id) {
        std::string label; for (char c : utf8(it.label)) { label += c; if (c == '_') label += '_'; }  // «_» alone marks a shortcut letter
        sd_bus_message_append(m, "{sv}", "label", "s", label.c_str());
        sd_bus_message_append(m, "{sv}", "enabled", "b", it.enabled ? 1 : 0);  // always: left out, the tray keeps the item's old state
        if (it.check >= 0) sd_bus_message_append(m, "{sv}{sv}", "toggle-type", "s", "checkmark", "toggle-state", "i", it.check ? 1 : 0);
    }
    if (!it.kids.empty()) sd_bus_message_append(m, "{sv}", "children-display", "s", "submenu");
    sd_bus_message_close_container(m);
}
static void putItem(sd_bus_message* m, const MenuItem& it, int depth) {
    sd_bus_message_open_container(m, 'r', "ia{sv}av");
    sd_bus_message_append(m, "i", it.id);
    putProps(m, it);
    sd_bus_message_open_container(m, 'a', "v");
    if (depth != 0) for (auto& k : it.kids) {
        sd_bus_message_open_container(m, 'v', "(ia{sv}av)"); putItem(m, k, depth - 1); sd_bus_message_close_container(m);
    }
    sd_bus_message_close_container(m);
    sd_bus_message_close_container(m);
}
static int menuGetLayout(sd_bus_message* m, void*, sd_bus_error*) {
    int32_t parent = 0, depth = -1;
    if (sd_bus_message_read(m, "ii", &parent, &depth) < 0) return -EINVAL;
    if (parent == 0) buildMenu();
    const MenuItem* it = findItem(menuRoot, parent);
    if (!it) return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "no menu item %d", parent);
    sd_bus_message* r = nullptr; sd_bus_message_new_method_return(m, &r);
    sd_bus_message_append(r, "u", menuRev);
    putItem(r, *it, depth);
    int k = sd_bus_send(nullptr, r, nullptr); sd_bus_message_unref(r);
    return k;
}
static void putGroup(sd_bus_message* r, const MenuItem& it, const std::set<int>& ids) {
    if (ids.empty() || ids.count(it.id)) {
        sd_bus_message_open_container(r, 'r', "ia{sv}"); sd_bus_message_append(r, "i", it.id); putProps(r, it); sd_bus_message_close_container(r);
    }
    for (auto& k : it.kids) putGroup(r, k, ids);
}
static int menuGetGroupProperties(sd_bus_message* m, void*, sd_bus_error*) {
    const int32_t* p = nullptr; size_t n = 0; std::set<int> ids;
    if (sd_bus_message_read_array(m, 'i', (const void**)&p, &n) >= 0) for (size_t i = 0; i < n / sizeof(int32_t); i++) ids.insert(p[i]);
    sd_bus_message* r = nullptr; sd_bus_message_new_method_return(m, &r);
    sd_bus_message_open_container(r, 'a', "(ia{sv})");
    putGroup(r, menuRoot, ids);
    sd_bus_message_close_container(r);
    int k = sd_bus_send(nullptr, r, nullptr); sd_bus_message_unref(r);
    return k;
}
static int menuGetProperty(sd_bus_message* m, void*, sd_bus_error*) {
    int32_t id = 0; const char* name = "";
    if (sd_bus_message_read(m, "is", &id, &name) < 0) return -EINVAL;
    const MenuItem* it = findItem(menuRoot, id);
    if (it && !strcmp(name, "label")) return sd_bus_reply_method_return(m, "v", "s", utf8(it->label).c_str());
    if (it && !strcmp(name, "enabled")) return sd_bus_reply_method_return(m, "v", "b", it->enabled);
    return sd_bus_reply_method_errorf(m, SD_BUS_ERROR_INVALID_ARGS, "no such property");
}
static int pendingCmd = 0;  // a click is handled after the reply has gone out
static int menuEvent(sd_bus_message* m, void*, sd_bus_error*) {
    int32_t id = 0; const char* what = "";
    if (sd_bus_message_read(m, "is", &id, &what) < 0) return -EINVAL;
    if (!strcmp(what, "clicked")) pendingCmd = id;
    return sd_bus_reply_method_return(m, "");
}
static int menuEventGroup(sd_bus_message* m, void*, sd_bus_error*) {
    if (sd_bus_message_enter_container(m, 'a', "(isvu)") > 0) {
        while (sd_bus_message_enter_container(m, 'r', "isvu") > 0) {
            int32_t id = 0; const char* what = "";
            if (sd_bus_message_read(m, "is", &id, &what) >= 0 && !strcmp(what, "clicked")) pendingCmd = id;
            sd_bus_message_skip(m, "vu"); sd_bus_message_exit_container(m);
        }
    }
    return sd_bus_reply_method_return(m, "ai", 0);
}
static int menuAboutToShow(sd_bus_message* m, void*, sd_bus_error*) { return sd_bus_reply_method_return(m, "b", 0); }  // LayoutUpdated already said what changed
static int menuAboutToShowGroup(sd_bus_message* m, void*, sd_bus_error*) { return sd_bus_reply_method_return(m, "aiai", 0, 0); }
static int menuProp(sd_bus*, const char*, const char*, const char* prop, sd_bus_message* reply, void*, sd_bus_error*) {
    std::string p = prop;
    if (p == "Version") return sd_bus_message_append(reply, "u", 3);
    if (p == "TextDirection") return sd_bus_message_append(reply, "s", "ltr");
    if (p == "Status") return sd_bus_message_append(reply, "s", "normal");
    return sd_bus_message_append(reply, "as", 0);  // IconThemePath
}
static const sd_bus_vtable kMenuVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "u", menuProp, 0, 0), SD_BUS_PROPERTY("TextDirection", "s", menuProp, 0, 0),
    SD_BUS_PROPERTY("Status", "s", menuProp, 0, 0), SD_BUS_PROPERTY("IconThemePath", "as", menuProp, 0, 0),
    SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", menuGetLayout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", menuGetGroupProperties, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetProperty", "is", "v", menuGetProperty, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Event", "isvu", "", menuEvent, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", menuEventGroup, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AboutToShow", "i", "b", menuAboutToShow, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("AboutToShowGroup", "ai", "aiai", menuAboutToShowGroup, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0), SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
    SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),
    SD_BUS_VTABLE_END};

// «My words…»: the desktop's own input box (kdialog or zenity), in a thread so the menu and the keys go on
static void editWords() {
    wstring cur; for (size_t i = 0; i < S.words.size(); i++) cur += (i ? L", " : L"") + S.words[i];
    std::string title = utf8(L_(L"AURA Switcher — my words", L"AURA Switcher — мої слова"));
    std::string info = utf8(L_(L"AURA Switcher treats these words as correct: it never changes them, and fixes them when typed in the wrong layout. Separate them with commas.",
                               L"Ці слова AURA Switcher вважає правильними: не змінює їх і виправляє, коли їх набрано не в тій розкладці. Через кому."));
    vector<std::string> a = have("kdialog") ? vector<std::string>{"kdialog", "--title", title, "--inputbox", info, utf8(cur)}
                                            : vector<std::string>{"zenity", "--entry", "--title", title, "--text", info, "--entry-text", utf8(cur)};
    std::thread([a] { std::string out; if (run(a, nullptr, &out, 0)) post(MSG_WORDS, new std::string(out)); }).detach();
}

static std::string urlEncode(const wstring& w) {
    std::string r; char buf[4];
    for (unsigned char c : utf8(w)) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.') r += char(c);
        else { snprintf(buf, sizeof buf, "%%%02X", c); r += buf; }
    }
    return r;
}
static const char* deskName() { return desk == DESK_KDE ? "KDE" : desk == DESK_X11 ? "X11" : "unsupported"; }
// Opens an email to the developer with version, desktop and layouts already filled in.
static void reportProblem() {
    utsname u = {}; uname(&u);
    wstring codes; for (auto& l : layouts) codes += (codes.empty() ? L"" : L", ") + l.code();
    wstring body = wstring(L_(L"What happened and in which app:", L"Що сталося і в якій програмі:")) +
                   L"\n\n\n---\nAURA Switcher " APP_VERSION L" · Linux " + wide(u.release) + L" · " + wide(envOr("XDG_CURRENT_DESKTOP", "?")) +
                   L" (" + wide(envOr("XDG_SESSION_TYPE", "?")) + L", " + wide(deskName()) + L") · " + codes;
    openUrl("mailto:auraswitcher@gmail.com?subject=" + urlEncode(L"AURA Switcher " APP_VERSION L" — Linux") + "&body=" + urlEncode(body));
}
static void toggleExcluded(const wstring& app) {
    if (S.isExcluded(app)) { vector<wstring> r; for (auto& e : S.excluded) if (wcscasecmp(e.c_str(), app.c_str())) r.push_back(e); S.excluded = r; }
    else S.excluded.push_back(app);
}

static std::atomic<bool> quitReq{false};
static void onMenuCommand(int cmd) {
    if (!findItem(menuRoot, cmd)) return;
    if (cmd == ID_ENABLED) { S.enabled = !S.enabled; }
    else if (cmd == ID_AUTO) { S.autoFix = !S.autoFix; }
    else if (cmd == ID_SOUND) { S.sound = !S.sound; playSound(SND_AUTO); }
    else if (cmd == ID_EXCLUDE) toggleExcluded(menuApp);
    else if (cmd >= ID_RECENT0 && cmd < ID_RECENT0 + int(menuRecent.size())) toggleExcluded(menuRecent[cmd - ID_RECENT0]);
    else if (cmd == ID_NOBIND) S.bindings.erase(menuApp);
    else if (cmd >= ID_BIND0 && cmd < ID_BIND0 + int(layouts.size())) S.bindings[menuApp] = layouts[cmd - ID_BIND0].locale;
    else if (cmd >= ID_EXDEL0 && cmd < ID_EXDEL0 + int(menuExDel.size())) {
        wstring d = menuExDel[cmd - ID_EXDEL0]; vector<wstring> r; for (auto& e : S.excluded) if (e != d) r.push_back(e); S.excluded = r;
    }
    else if (cmd >= ID_BDDEL0 && cmd < ID_BDDEL0 + int(menuBdDel.size())) S.bindings.erase(menuBdDel[cmd - ID_BDDEL0]);
    else if (cmd == ID_LOGIN) setLogin(!loginEnabled());
    else if (cmd == ID_WORDS) editWords();
    else if (cmd == ID_REPORT) reportProblem();
    // no Linux packages on the releases page yet: the update page says what is new, the build is from source
    else if (cmd == ID_UPDATE) openUrl("https://klbk.github.io/aura-switcher/update/?v=" + utf8(APP_VERSION) + "&l=" + utf8(UI_LANGS[uiIndex()]));
    else if (cmd == ID_DONATE) openUrl("https://donatello.to/aura-switcher");
    else if (cmd == ID_ACCESS) openUrl("https://github.com/klbk/aura-switcher-app/blob/main/docs/linux.md");
    else if (cmd == ID_UI_SYS) S.uiLang = L"";
    else if (cmd >= ID_UI0 && cmd < ID_UI0 + UI_N) S.uiLang = UI_LANGS[cmd - ID_UI0];
    else if (cmd >= ID_UITOP0 && cmd < ID_UITOP0 + UI_N) S.uiLang = UI_LANGS[cmd - ID_UITOP0];
    else if (cmd == ID_QUIT) { quitReq = true; return; }
    S.save(); refreshActive(); updateTray(true); menuChanged();
}

// MARK: - Desktop events

static void reloadLayouts() {
    layouts = loadLayouts(); lastSelOut.clear(); warmUpDictionaries();
    repeatMs = deskRepeatMs();
    updateTray(true); menuChanged();
}
static int onLayoutChanged(sd_bus_message*, void*, sd_bus_error*) { updateTray(); return 0; }
static int onLayoutList(sd_bus_message*, void*, sd_bus_error*) { reloadLayouts(); return 0; }
// the lock screen takes the keys while the session is locked: a password is nobody's word to fix
static int onLocked(sd_bus_message* m, void*, sd_bus_error*) {
    int on = 0; if (sd_bus_message_read(m, "b", &on) >= 0) { sessionLocked = on != 0; resetReq = true; refreshActive(); updateTray(); }
    return 0;
}
static void startKde() {
    desk = DESK_KDE;
    sd_bus_match_signal(bus, nullptr, "org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts", "layoutChanged", onLayoutChanged, nullptr);
    sd_bus_match_signal(bus, nullptr, "org.kde.keyboard", "/Layouts", "org.kde.KeyboardLayouts", "layoutListChanged", onLayoutList, nullptr);
    kwinScript(true);
}
// the desktop's services may start after us (login) or restart (kwin --replace, plasmashell --replace): catch up
static int onNameOwner(sd_bus_message* m, void*, sd_bus_error*) {
    const char *name, *was, *now;
    if (sd_bus_message_read(m, "sss", &name, &was, &now) < 0 || !*now) return 0;
    if (!strcmp(name, "org.kde.StatusNotifierWatcher")) { registerTray(); updateTray(true); }
    else if (!strcmp(name, "org.kde.KWin") && desk == DESK_KDE) kwinScript(true);
    else if (!strcmp(name, "org.kde.keyboard") && desk != DESK_X11) {
        if (desk == DESK_NONE) startKde();
        reloadLayouts(); refreshActive(); updateTray(true);
    }
    return 0;
}
static const sd_bus_vtable kAppVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("ActiveWindow", "ssss", "", onKwinWindow, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

static bool nameHasOwner(const char* name) {
    sd_bus_message* r = nullptr; int has = 0;
    if (sd_bus_call_method(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner", nullptr, &r, "s", name) >= 0) {
        sd_bus_message_read(r, "b", &has); sd_bus_message_unref(r);
    }
    return has != 0;
}
static void detectDesktop() {
    if (nameHasOwner("org.kde.keyboard")) { desk = DESK_KDE; return; }
#ifdef AURA_X11
    // on Wayland the X server is only a bridge for old apps: its layout is not the session's
    if (!getenv("WAYLAND_DISPLAY") && getenv("DISPLAY")) {
        int err, major = XkbMajorVersion, minor = XkbMinorVersion, reason;
        dpy = XkbOpenDisplay(nullptr, &xkbEventBase, &err, &major, &minor, &reason);
        if (dpy) {
            XSetErrorHandler(xIgnoreError);
            aActive = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False); aRules = XInternAtom(dpy, "_XKB_RULES_NAMES", False);
            aPid = XInternAtom(dpy, "_NET_WM_PID", False); aState = XInternAtom(dpy, "_NET_WM_STATE", False); aFull = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
            XSelectInput(dpy, DefaultRootWindow(dpy), PropertyChangeMask);
            XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify, XkbGroupStateMask, XkbGroupStateMask);
            desk = DESK_X11;
        }
    }
#endif
}

static void printCheck() {
    printf("AURA Switcher %s\n", utf8(APP_VERSION).c_str());
    printf("desktop: %s (%s, %s)\n", deskName(), envOr("XDG_CURRENT_DESKTOP", "?").c_str(), envOr("XDG_SESSION_TYPE", "?").c_str());
    vector<LayoutId> names = deskLayouts();
    printf("layouts:"); for (size_t i = 0; i < names.size(); i++) printf(" %s%s%s→%s", names[i].layout.c_str(), names[i].variant.empty() ? "" : ":", names[i].variant.c_str(), utf8(groupLang[i]).c_str());
    printf("%s\n", names.empty() ? " none" : "");
    printf("current layout: %s\n", layouts.empty() ? "-" : utf8(layouts[curLayout()].locale).c_str());
    printf("keyboards readable: %d\n", keyboards.load());
    if (denied) {
        printf("keyboards or mice NOT readable: %d (see docs/linux.md):", denied.load());
        if (DIR* d = opendir("/dev/input")) {
            while (dirent* e = readdir(d)) {
                std::string ev = e->d_name, name;
                if (ev.compare(0, 5, "event") || access(("/dev/input/" + ev).c_str(), R_OK) == 0 || !sysHasKey(ev, {KEY_A, BTN_LEFT, BTN_TOUCH})) continue;
                readFile("/sys/class/input/" + ev + "/device/name", name);
                if (!name.compare(0, strlen(kVirtualName), kVirtualName)) continue;  // our own, a moment old
                printf(" %s (%s)", ev.c_str(), name.substr(0, name.find('\n')).c_str());
            }
            closedir(d);
        }
        printf("\n");
    }
    printf("virtual keyboard (/dev/uinput): %s\n", ui >= 0 ? "ok" : "no access (see docs/linux.md)");
    if (ui >= 0) { pauseMs(700); checkEcho(); printf("key remapper grabbing it: %s\n", echoMode ? "yes (handled)" : "no"); }
    for (auto& l : layouts) printf("%s: %s%s\n", utf8(l.locale).c_str(), dictionaryWorks(l.locale) ? "dictionary + " : "", model(l.locale) ? "language model" : "no language model (double Shift only)");
    printf("clipboard tool (selection conversion): %s\n", clipTool() == 1 ? "wl-clipboard" : clipTool() == 2 ? "xclip" : clipTool() == 3 ? "xsel" : "none");
    printf("tray: %s\n", nameHasOwner("org.kde.StatusNotifierWatcher") ? "ok" : "no StatusNotifierWatcher (no tray icon)");
}

// the signal may arrive on any thread: wake the main one through its pipe
static void onSignal(int) { quitReq = true; Msg m{0, nullptr}; if (write(msgPipe[1], &m, sizeof m) < 0) {} }

int main(int argc, char** argv) {
    // letters of every script count as letters whatever the system language: any UTF-8 locale will do
    if (!setlocale(LC_CTYPE, "") || !iswalpha(L'я')) { if (!setlocale(LC_CTYPE, "C.UTF-8")) setlocale(LC_CTYPE, "en_US.UTF-8"); }
    bool check = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--version") { printf("%s\n", utf8(APP_VERSION).c_str()); return 0; }
        else if (a == "--check") check = true;
        else if (a == "--debug") debugLog = true;
        else { printf("usage: aura-switcher [--check] [--debug] [--version]\n"); return a == "--help" ? 0 : 2; }
    }
    // single instance: a second copy would double every correction
    int lock = open((runtimeDir() + "/lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (!check && (lock < 0 || flock(lock, LOCK_EX | LOCK_NB) < 0)) { fprintf(stderr, "AURA Switcher is already running\n"); return 0; }

    if (sd_bus_open_user(&bus) < 0) { fprintf(stderr, "AURA Switcher: no D-Bus session bus\n"); return 1; }
    sd_bus_set_method_call_timeout(bus, 800 * 1000);  // a stuck desktop must not stick us
    detectDesktop();
    if (check) scanDevices();  // before our own keyboard appears: its node isn't readable in its first moment
    bool uinputOk = openUinput();
    layouts = loadLayouts();
    if (check) { printCheck(); closeUinput(); return 0; }
    if (!uinputOk) fprintf(stderr, "AURA Switcher: no access to /dev/uinput (see docs/linux.md)\n");
    if (desk == DESK_NONE) fprintf(stderr, "AURA Switcher: this desktop isn't supported yet (see docs/linux.md)\n");

    signal(SIGINT, onSignal); signal(SIGTERM, onSignal); signal(SIGHUP, onSignal); signal(SIGPIPE, SIG_IGN);
    if (pipe2(msgPipe, O_CLOEXEC) < 0) return 1;
    fcntl(msgPipe[0], F_SETFL, O_NONBLOCK);

    sniName = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-1";
    sd_bus_request_name(bus, kBusName, 0);
    sd_bus_request_name(bus, sniName.c_str(), 0);
    sd_bus_add_object_vtable(bus, nullptr, kSniPath, "org.kde.StatusNotifierItem", kSniVtable, nullptr);
    sd_bus_add_object_vtable(bus, nullptr, kMenuPath, "com.canonical.dbusmenu", kMenuVtable, nullptr);
    sd_bus_add_object_vtable(bus, nullptr, "/App", kBusName, kAppVtable, nullptr);
    sd_bus_match_signal(bus, nullptr, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", onNameOwner, nullptr);
    sd_bus_match_signal(bus, nullptr, nullptr, nullptr, "org.freedesktop.ScreenSaver", "ActiveChanged", onLocked, nullptr);

    warmUpDictionaries();
    repeatMs = deskRepeatMs();
    buildMenu();
    refreshActive();
    iconName = makeIcon(L"--", true);
    registerTray();
    updateTray(true);
    if (desk == DESK_KDE) startKde();
#ifdef AURA_X11
    if (desk == DESK_X11) x11Active();
#endif
    std::thread(readerThread).detach();
    logf("started: desktop %s, %d layouts", deskName(), int(layouts.size()));

    while (!quitReq) {
        while (sd_bus_process(bus, nullptr) > 0) {}
        if (pendingCmd) { int c = pendingCmd; pendingCmd = 0; onMenuCommand(c); continue; }
        long long now = nowMs();
        for (size_t i = 0; i < timers.size();) {
            if (timers[i].first > now) { i++; continue; }
            auto fn = std::move(timers[i].second); timers.erase(timers.begin() + long(i)); fn();
        }
        // sleeps until something happens: a key message, a D-Bus call or signal, an X event, a timer of ours
        int wait = -1; for (auto& t : timers) { int w = int(std::max(0LL, t.first - now)); wait = wait < 0 ? w : std::min(wait, w); }
        uint64_t usec = 0;
        if (sd_bus_get_timeout(bus, &usec) >= 0 && usec != UINT64_MAX) {
            timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
            int w = int(std::max(0LL, (long long)usec - (ts.tv_sec * 1000000LL + ts.tv_nsec / 1000)) / 1000);
            wait = wait < 0 ? w : std::min(wait, w);
        }
        pollfd p[3] = {{sd_bus_get_fd(bus), short(sd_bus_get_events(bus)), 0}, {msgPipe[0], POLLIN, 0}, {-1, 0, 0}};
#ifdef AURA_X11
        if (dpy) { p[2] = {ConnectionNumber(dpy), POLLIN, 0}; if (XPending(dpy)) wait = 0; }
#endif
        if (poll(p, 3, wait) < 0 && errno != EINTR) break;
        Msg m;
        while (read(msgPipe[0], &m, sizeof m) == sizeof m) {
            if (m.type == MSG_AUTO) { auto* s = (Snapshot*)m.p; if (active) onAuto(s); delete s; }
            else if (m.type == MSG_DSHIFT) { auto* s = (Snapshot*)m.p; if (active) onDoubleShift(s); delete s; }
            else if (m.type == MSG_WORDS) { auto* s = (std::string*)m.p; S.words = parseWords(wide(*s)); S.save(); delete s; }
            else if (m.type == MSG_DEVICES) { logf("devices: %d keyboards, %d not readable", keyboards.load(), denied.load()); refreshActive(); updateTray(); menuChanged(); }
        }
#ifdef AURA_X11
        while (dpy && XPending(dpy)) {
            XEvent e; XNextEvent(dpy, &e);
            if (e.type == PropertyNotify) {
                if (e.xproperty.atom == aActive || (e.xproperty.window == xActive && e.xproperty.atom == aState)) x11Active();
                else if (e.xproperty.atom == aRules) reloadLayouts();
            } else if (e.type == xkbEventBase) updateTray();
        }
#endif
    }
    if (desk == DESK_KDE) kwinScript(false);
    closeUinput();
    sd_bus_flush(bus);
    return 0;
}
#endif
