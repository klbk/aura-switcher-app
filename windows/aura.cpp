// AURA for Windows — fixes text typed in the wrong keyboard layout.
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
#include <shellapi.h>
#include <objbase.h>
#include <spellcheck.h>
#include <atomic>
#include <map>
#include <string>
#include <vector>

#define APP_VERSION L"0.4"

using std::wstring;
using std::vector;

static const ULONG_PTR kMagic = 0x4B535754;  // marks our own synthetic input
enum { WM_TRAY = WM_APP + 1, WM_AUTO, WM_DSHIFT };
enum { ID_ENABLED = 1, ID_AUTO, ID_EXCLUDE, ID_NOBIND, ID_LOGIN, ID_UI_SYS, ID_UI_EN, ID_UI_UK, ID_QUIT,
       ID_BIND0 = 100, ID_EXDEL0 = 200, ID_BDDEL0 = 300 };

// MARK: - Settings (HKCU\Software\AURA)

static const wchar_t* kRegKey = L"Software\\AURA";

static wstring regGet(const wchar_t* name, const wstring& def) {
    wchar_t buf[4096]; DWORD sz = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, name, RRF_RT_REG_SZ, nullptr, buf, &sz) == ERROR_SUCCESS) return buf;
    return def;
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

struct Settings {
    bool enabled = regGet(L"enabled", L"1") == L"1";
    bool autoFix = regGet(L"auto", L"1") == L"1";
    wstring uiLang = regGet(L"uiLang", L"");            // "" = system
    vector<wstring> excluded = split(regGet(L"excluded", L""));
    std::map<wstring, wstring> bindings;                // exe name → locale name
    Settings() { for (auto& p : split(regGet(L"bindings", L""))) { size_t e = p.find(L'='); if (e != wstring::npos) bindings[p.substr(0, e)] = p.substr(e + 1); } }
    void save() {
        regSet(L"enabled", enabled ? L"1" : L"0"); regSet(L"auto", autoFix ? L"1" : L"0");
        regSet(L"uiLang", uiLang); regSet(L"excluded", join(excluded));
        vector<wstring> b; for (auto& kv : bindings) b.push_back(kv.first + L"=" + kv.second); regSet(L"bindings", join(b));
    }
    bool isExcluded(const wstring& exe) { for (auto& e : excluded) if (!_wcsicmp(e.c_str(), exe.c_str())) return true; return false; }
};
static Settings S;

// MARK: - Localization (English, Ukrainian; Russian system → Ukrainian)

static bool uiUk() {
    if (!S.uiLang.empty()) return S.uiLang == L"uk";
    LANGID l = PRIMARYLANGID(GetUserDefaultUILanguage());
    return l == LANG_UKRAINIAN || l == LANG_RUSSIAN;
}
static const wchar_t* L_(const wchar_t* en, const wchar_t* uk) { return uiUk() ? uk : en; }

// MARK: - Layouts

struct Key { BYTE vk; bool shift; bool operator<(const Key& o) const { return vk != o.vk ? vk < o.vk : shift < o.shift; } bool operator==(const Key& o) const { return vk == o.vk && shift == o.shift; } };

struct Layout {
    HKL hkl; wstring locale;  // e.g. "uk-UA"
    std::map<Key, wchar_t> toChar;
    std::map<wchar_t, Key> toKey;
    wstring text(const vector<Key>& keys) const { wstring s; for (auto& k : keys) { auto it = toChar.find(k); if (it != toChar.end()) s += it->second; } return s; }
    wstring code() const { wstring c = locale.substr(0, 2); for (auto& ch : c) ch = towupper(ch); return c; }
};

static bool isLetterVk(BYTE vk) {
    return (vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || (vk >= VK_OEM_1 && vk <= VK_OEM_3) || (vk >= VK_OEM_4 && vk <= VK_OEM_8) || vk == VK_OEM_102;
}

static vector<Layout> loadLayouts() {
    vector<Layout> r;
    HKL list[64]; int n = GetKeyboardLayoutList(64, list);
    for (int i = 0; i < n; i++) {
        Layout L; L.hkl = list[i];
        wchar_t name[LOCALE_NAME_MAX_LENGTH];
        if (!LCIDToLocaleName(MAKELCID(LOWORD((UINT_PTR)list[i]), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0)) continue;
        L.locale = name;
        bool dup = false; for (auto& o : r) if (o.locale.substr(0, 2) == L.locale.substr(0, 2)) dup = true;
        if (dup) continue;
        BYTE state[256] = {};
        for (int vk = 0; vk < 256; vk++) {
            if (!isLetterVk(BYTE(vk))) continue;
            for (int sh = 0; sh < 2; sh++) {
                state[VK_SHIFT] = sh ? 0x80 : 0;
                wchar_t buf[4];
                UINT sc = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, list[i]);
                int len = ToUnicodeEx(vk, sc, state, buf, 4, 4 /* don't change kbd state */, list[i]);
                if (len != 1 || buf[0] < 0x20) continue;
                Key k{BYTE(vk), sh != 0};
                L.toChar[k] = buf[0];
                if (!L.toKey.count(buf[0])) L.toKey[buf[0]] = k;
            }
        }
        if (!L.toChar.empty()) r.push_back(L);
    }
    return r;
}

static vector<Layout> layouts;
static HWND hwndMain;

static HKL fgLayout() {
    HWND fg = GetForegroundWindow();
    return GetKeyboardLayout(GetWindowThreadProcessId(fg, nullptr));
}
static int layoutIndex(HKL h) {
    for (size_t i = 0; i < layouts.size(); i++) if (LOWORD((UINT_PTR)layouts[i].hkl) == LOWORD((UINT_PTR)h)) return int(i);
    return 0;
}
static void selectLayout(const Layout& l) {
    HWND fg = GetForegroundWindow();
    PostMessageW(fg, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)l.hkl);
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

static vector<Key> word, lastWord;
static int trailing = 0;
static std::atomic<unsigned> seq{0};            // bumps on every real key; main thread uses it to detect typing
static std::atomic<bool> resetReq{false}, active{true};
static ULONGLONG shiftDownAt = 0, lastShiftTap = 0; static bool sawOther = false, shiftHeld = false;

struct Snapshot { vector<Key> keys; int trailing; unsigned seq; };

static void resetState() { word.clear(); lastWord.clear(); trailing = 0; }

static LRESULT CALLBACK kbHook(int code, WPARAM wp, LPARAM lp) {
    auto* k = (KBDLLHOOKSTRUCT*)lp;
    if (code != HC_ACTION || k->dwExtraInfo == kMagic || !active) return CallNextHookEx(nullptr, code, wp, lp);
    if (resetReq.exchange(false)) resetState();
    bool down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
    BYTE vk = BYTE(k->vkCode);
    ULONGLONG now = GetTickCount64();

    if (vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_SHIFT) {
        bool mods = (GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
        if (down) {
            if (!shiftHeld) { shiftHeld = true; shiftDownAt = now; sawOther = false; }
            if (mods) lastShiftTap = 0;
        } else {
            shiftHeld = false;
            if (sawOther || mods || now - shiftDownAt > 300) lastShiftTap = 0;
            else if (lastShiftTap && now - lastShiftTap < 450) {
                lastShiftTap = 0;
                bool cur = !word.empty();
                auto* s = new Snapshot{cur ? word : lastWord, cur ? 0 : trailing, seq.load()};
                PostMessageW(hwndMain, WM_DSHIFT, 0, (LPARAM)s);
            } else lastShiftTap = now;
        }
        return CallNextHookEx(nullptr, code, wp, lp);
    }
    if (!down) return CallNextHookEx(nullptr, code, wp, lp);

    sawOther = true; lastShiftTap = 0; seq++;
    bool ctrlAlt = (GetAsyncKeyState(VK_CONTROL) | GetAsyncKeyState(VK_MENU) | GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
    if (ctrlAlt) { resetState(); return CallNextHookEx(nullptr, code, wp, lp); }

    if (vk == VK_BACK) {
        if (trailing > 0) { if (--trailing == 0) { word = lastWord; lastWord.clear(); } }
        else if (!word.empty()) word.pop_back();
    } else if (vk == VK_SPACE) {
        if (!word.empty()) {
            lastWord = word; word.clear(); trailing = 1;
            PostMessageW(hwndMain, WM_AUTO, 0, (LPARAM)new Snapshot{lastWord, 1, seq.load()});
        } else if (!lastWord.empty()) trailing++;
    } else if (isLetterVk(vk)) {
        if (trailing > 0 || !lastWord.empty()) { lastWord.clear(); trailing = 0; }
        bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
        word.push_back({vk, (vk >= 'A' && vk <= 'Z') ? (shift != caps) : shift});
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
static void typeText(const wstring& s) {
    vector<INPUT> in;
    for (wchar_t c : s) for (int up = 0; up < 2; up++) {
        INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wScan = c; i.ki.dwFlags = KEYEVENTF_UNICODE | (up ? KEYEVENTF_KEYUP : 0); i.ki.dwExtraInfo = kMagic; in.push_back(i);
    }
    if (!in.empty()) SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
}
static void replaceText(int count, const wstring& s) { for (int i = 0; i < count; i++) sendKey(VK_BACK); typeText(s); }

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
static bool isWord(const wstring& w, const wstring& locale) {
    if (w.size() < 2) return false;
    for (wchar_t c : w) if (!IsCharAlphaW(c) && c != L'\'' && c != L'-') return false;
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

static bool isLatin(const wstring& s) { for (wchar_t c : s) if (c >= 0x250) return false; return true; }

static int autoFixTarget(const vector<Key>& keys) {
    if (keys.size() < 2 || layouts.empty()) return -1;
    int cur = layoutIndex(fgLayout());
    wstring typed = layouts[cur].text(keys);
    if (isWord(typed, layouts[cur].locale)) return -1;
    for (size_t i = 1; i < layouts.size(); i++) {
        int j = int((cur + i) % layouts.size());
        wstring t = layouts[j].text(keys);
        // near-identical Latin layouts (QWERTY/QWERTZ/AZERTY): a typo there isn't a layout mistake.
        // Cyrillic UK/RU differ in few keys but that's exactly what users mix up, so they're allowed.
        size_t same = 0; for (size_t p = 0; p < t.size() && p < typed.size(); p++) same += t[p] == typed[p];
        if (isLatin(t) && isLatin(typed) && same * 2 > keys.size()) continue;
        if (t.size() == keys.size() && isWord(t, layouts[j].locale)) return j;
    }
    return -1;
}

// MARK: - Actions (main thread)

static wstring lastSelOut; static int lastSelLang = 0;

static bool clipboardText(wstring& out) {
    if (!OpenClipboard(hwndMain)) return false;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    bool ok = false;
    if (h) { auto* p = (const wchar_t*)GlobalLock(h); if (p) { out = p; ok = true; GlobalUnlock(h); } }
    CloseClipboard();
    return ok;
}
static void setClipboardText(const wstring& s) {
    if (!OpenClipboard(hwndMain)) return;
    EmptyClipboard();
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(wchar_t));
    if (g) { memcpy(GlobalLock(g), s.c_str(), (s.size() + 1) * sizeof(wchar_t)); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); }
    CloseClipboard();
}
static bool copySelection(wstring& sel) {
    wstring saved; bool hadText = clipboardText(saved);
    DWORD before = GetClipboardSequenceNumber();
    INPUT in[4] = {};
    for (int i = 0; i < 4; i++) { in[i].type = INPUT_KEYBOARD; in[i].ki.dwExtraInfo = kMagic; }
    in[0].ki.wVk = VK_CONTROL; in[1].ki.wVk = 'C'; in[2].ki.wVk = 'C'; in[2].ki.dwFlags = KEYEVENTF_KEYUP; in[3].ki.wVk = VK_CONTROL; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
    for (int i = 0; i < 15 && GetClipboardSequenceNumber() == before; i++) Sleep(10);
    if (GetClipboardSequenceNumber() == before) return false;
    Sleep(20);
    bool ok = clipboardText(sel);
    if (hadText) setClipboardText(saved);
    return ok && !sel.empty();
}

static void cycleLayout() { if (!layouts.empty()) selectLayout(layouts[(layoutIndex(fgLayout()) + 1) % layouts.size()]); }

static void convertSelection(const wstring& text) {
    int src;
    if (text == lastSelOut) src = lastSelLang;
    else {
        int cur = layoutIndex(fgLayout()), best = cur, bestScore = -1;
        for (size_t i = 0; i < layouts.size(); i++) {
            int sc = 0; for (wchar_t c : text) if (IsCharAlphaW(c) && layouts[i].toKey.count(c)) sc++;
            if (sc > bestScore || (sc == bestScore && int(i) == cur)) { best = int(i); bestScore = sc; }
        }
        src = best;
    }
    int dst = int((src + 1) % layouts.size());
    wstring out;
    for (wchar_t c : text) {
        auto k = layouts[src].toKey.find(c);
        wchar_t r = c;
        if (k != layouts[src].toKey.end()) { auto t = layouts[dst].toChar.find(k->second); if (t != layouts[dst].toChar.end()) r = t->second; }
        out += r;
    }
    typeText(out);
    for (size_t i = 0; i < out.size(); i++) sendKey(VK_LEFT, true);  // reselect
    lastSelOut = out; lastSelLang = dst;
    selectLayout(layouts[dst]);
}

static void onDoubleShift(Snapshot* s) {
    if (layouts.empty()) return;
    if (!s->keys.empty()) {
        const Layout& n = layouts[(layoutIndex(fgLayout()) + 1) % layouts.size()];
        replaceText(int(s->keys.size()) + s->trailing, n.text(s->keys) + wstring(s->trailing, L' '));
        selectLayout(n);
        return;
    }
    wstring sel;
    if (copySelection(sel)) convertSelection(sel); else cycleLayout();
}

static void onAuto(Snapshot* s) {
    if (!S.autoFix || S.bindings.count(fgExe())) return;
    int j = autoFixTarget(s->keys);
    if (j < 0 || seq.load() != s->seq) return;  // user kept typing meanwhile — skip
    wstring out = layouts[j].text(s->keys);
    // UK/RU word spelled the same in both layouts: text is already right, only switch the layout
    if (out != layouts[layoutIndex(fgLayout())].text(s->keys)) replaceText(int(s->keys.size()) + 1, out + L" ");
    selectLayout(layouts[j]);
}

// MARK: - Tray

static NOTIFYICONDATAW nid = {};
static wstring shownCode;

static HICON makeTextIcon(const wstring& text, bool on) {
    const int sz = 32;
    BITMAPINFO bi = {}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), sz, -sz, 1, 32, BI_RGB};
    void* bits; HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(dc, color);
    RECT rc{0, 0, sz, sz};
    HBRUSH bg = CreateSolidBrush(on ? RGB(0x2f, 0x8b, 0xff) : RGB(0x70, 0x70, 0x78));
    FillRect(dc, &rc, bg); DeleteObject(bg);
    HFONT f = CreateFontW(22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, on ? RGB(0xff, 0xd5, 0x00) : RGB(0xff, 0xff, 0xff));
    DrawTextW(dc, text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of); DeleteObject(f);
    for (int i = 0; i < sz * sz; i++) ((DWORD*)bits)[i] |= 0xFF000000;  // GDI leaves alpha 0
    SelectObject(dc, old); DeleteDC(dc);
    HBITMAP mask = CreateBitmap(sz, sz, 1, 1, nullptr);
    ICONINFO ii{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask); DeleteObject(color);
    return icon;
}

static void updateTray(bool force = false) {
    wstring code = layouts.empty() ? L"--" : layouts[layoutIndex(fgLayout())].code();
    wstring key = code + (S.enabled ? L"1" : L"0");
    if (!force && key == shownCode) return;
    shownCode = key;
    HICON old = nid.hIcon;
    nid.hIcon = makeTextIcon(code, S.enabled);
    wcscpy_s(nid.szTip, L"AURA " APP_VERSION L" beta");
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

// MARK: - Menu

static wstring menuExe;  // app that was in front when the menu opened
static vector<wstring> menuExDel, menuBdDel;

static wstring langName(const wstring& locale) {
    wchar_t buf[128];
    if (GetLocaleInfoEx(locale.c_str(), LOCALE_SNATIVELANGUAGENAME, buf, 128)) { wstring s = buf; if (!s.empty()) s[0] = towupper(s[0]); return s; }
    return locale;
}

static void showMenu() {
    menuExe = fgExe();
    HMENU m = CreatePopupMenu();
    auto item = [](HMENU h, UINT id, const wstring& t, bool checked = false, bool enabled = true) {
        AppendMenuW(h, MF_STRING | (checked ? MF_CHECKED : 0) | (enabled ? 0 : MF_GRAYED), id, t.c_str());
    };
    item(m, ID_ENABLED, L_(L"Enabled", L"Увімкнено"), S.enabled);
    item(m, ID_AUTO, L_(L"Auto-correction", L"Автовиправлення"), S.autoFix, S.enabled);
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    if (!menuExe.empty()) {
        item(m, 0, wstring(L_(L"App: ", L"Програма: ")) + menuExe, false, false);
        item(m, ID_EXCLUDE, L_(L"Don't work in this app", L"Не працювати в цій програмі"), S.isExcluded(menuExe));
        HMENU lm = CreatePopupMenu();
        auto b = S.bindings.find(menuExe);
        item(lm, ID_NOBIND, L_(L"No binding", L"Без прив'язки"), b == S.bindings.end());
        for (size_t i = 0; i < layouts.size(); i++)
            item(lm, UINT(ID_BIND0 + i), langName(layouts[i].locale), b != S.bindings.end() && b->second == layouts[i].locale);
        AppendMenuW(m, MF_POPUP, (UINT_PTR)lm, L_(L"Language for this app", L"Мова для цієї програми"));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    }

    HMENU em = CreatePopupMenu(); menuExDel = S.excluded;
    if (menuExDel.empty()) item(em, 0, L_(L"Empty", L"Порожньо"), false, false);
    for (size_t i = 0; i < menuExDel.size(); i++) item(em, UINT(ID_EXDEL0 + i), L"✕ " + menuExDel[i]);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)em, L_(L"Exclusions", L"Винятки"));

    HMENU bm = CreatePopupMenu(); menuBdDel.clear();
    if (S.bindings.empty()) item(bm, 0, L_(L"Empty", L"Порожньо"), false, false);
    for (auto& kv : S.bindings) {
        wstring c = kv.second.substr(0, 2); for (auto& ch : c) ch = towupper(ch);
        item(bm, UINT(ID_BDDEL0 + menuBdDel.size()), L"✕ " + kv.first + L" → " + c);
        menuBdDel.push_back(kv.first);
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)bm, L_(L"Language bindings", L"Прив'язки мов"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    item(m, ID_LOGIN, L_(L"Launch at login", L"Запускати під час входу"), loginEnabled());
    HMENU um = CreatePopupMenu();
    item(um, ID_UI_SYS, L_(L"System", L"Системна"), S.uiLang.empty());
    item(um, ID_UI_EN, L"English", S.uiLang == L"en");
    item(um, ID_UI_UK, L"Українська", S.uiLang == L"uk");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)um, L_(L"Menu language", L"Мова меню"));
    item(m, 0, L_(L"Double ⇧ — convert word/selection", L"Подвійний ⇧ — змінити мову слова/виділення"), false, false);
    item(m, 0, L"AURA " APP_VERSION L" beta · 🇺🇦", false, false);
    item(m, ID_QUIT, L_(L"Quit", L"Вихід"));

    POINT p; GetCursorPos(&p);
    SetForegroundWindow(hwndMain);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwndMain, nullptr);
    DestroyMenu(m);

    if (cmd == ID_ENABLED) { S.enabled = !S.enabled; }
    else if (cmd == ID_AUTO) { S.autoFix = !S.autoFix; }
    else if (cmd == ID_EXCLUDE) {
        if (S.isExcluded(menuExe)) { vector<wstring> r; for (auto& e : S.excluded) if (_wcsicmp(e.c_str(), menuExe.c_str())) r.push_back(e); S.excluded = r; }
        else S.excluded.push_back(menuExe);
    }
    else if (cmd == ID_NOBIND) S.bindings.erase(menuExe);
    else if (cmd >= ID_BIND0 && cmd < ID_BIND0 + layouts.size()) S.bindings[menuExe] = layouts[cmd - ID_BIND0].locale;
    else if (cmd >= ID_EXDEL0 && cmd < ID_EXDEL0 + menuExDel.size()) {
        wstring d = menuExDel[cmd - ID_EXDEL0]; vector<wstring> r; for (auto& e : S.excluded) if (e != d) r.push_back(e); S.excluded = r;
    }
    else if (cmd >= ID_BDDEL0 && cmd < ID_BDDEL0 + menuBdDel.size()) S.bindings.erase(menuBdDel[cmd - ID_BDDEL0]);
    else if (cmd == ID_LOGIN) setLogin(!loginEnabled());
    else if (cmd == ID_UI_SYS) S.uiLang = L"";
    else if (cmd == ID_UI_EN) S.uiLang = L"en";
    else if (cmd == ID_UI_UK) S.uiLang = L"uk";
    else if (cmd == ID_QUIT) { PostQuitMessage(0); return; }
    if (cmd) { S.save(); updateTray(true); }
}

// MARK: - Foreground tracking

static wstring curExe;

static void refreshActive() {
    curExe = fgExe();
    active = S.enabled && !S.isExcluded(curExe);
}

static void CALLBACK onForeground(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    wstring prev = curExe;
    refreshActive();
    if (curExe == prev) return;
    resetReq = true;
    auto b = S.bindings.find(curExe);
    if (S.enabled && b != S.bindings.end())
        for (auto& l : layouts) if (l.locale == b->second) { selectLayout(l); break; }
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
    case WM_TIMER: refreshActive(); updateTray(); return 0;
    case WM_INPUTLANGCHANGE: case WM_SETTINGCHANGE: layouts = loadLayouts(); updateTray(true); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    if (msg == WM_TASKBARCREATED) { Shell_NotifyIconW(NIM_ADD, &nid); updateTray(true); return 0; }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // single instance: a second copy would double every correction
    CreateMutexW(nullptr, TRUE, L"Local\\AURA-single-instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, __uuidof(ISpellCheckerFactory), (void**)&factory);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW wc = {}; wc.lpfnWndProc = wndProc; wc.hInstance = inst; wc.lpszClassName = L"AURA";
    RegisterClassW(&wc);
    hwndMain = CreateWindowW(L"AURA", L"AURA", 0, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);

    layouts = loadLayouts();
    if (regGet(L"loginSet", L"") != L"1") { setLogin(true); regSet(L"loginSet", L"1"); }

    nid.cbSize = sizeof(nid); nid.hWnd = hwndMain; nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE; nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = makeTextIcon(L"--", true);
    Shell_NotifyIconW(NIM_ADD, &nid);
    refreshActive();
    updateTray(true);

    SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onForeground, 0, 0, WINEVENT_OUTOFCONTEXT);
    SetTimer(hwndMain, 1, 300, nullptr);  // tray language code follows the active layout
    CreateThread(nullptr, 0, hookThread, nullptr, 0, nullptr);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    Shell_NotifyIconW(NIM_DELETE, &nid);
    for (auto& kv : checkers) if (kv.second) kv.second->Release();
    if (factory) factory->Release();
    CoUninitialize();
    return 0;
}
