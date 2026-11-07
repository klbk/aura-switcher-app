// Windows auto-correction test: the same cases as tests/main.swift, on the real engine (windows/aura.cpp)
// with Windows keyboard layouts, spell checkers and the embedded language models — no app, no key events.
// Built and run by tests/run-windows.sh (on Windows, in CI):
//   aura-test.exe <repo>            — run, compare with tests/baseline-windows.txt
//   aura-test.exe <repo> --update   — accept the current results as the new baseline
#define AURA_TEST
#include "../windows/aura.cpp"
#include <cstdio>
#include <fstream>
#include <sstream>

static std::string utf8(const wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), &s[0], n, nullptr, nullptr); return s;
}
static wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), nullptr, 0);
    wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), &w[0], n); return w;
}

static wstring root;
static std::map<wstring, Layout> cache;
// layouts the test loads stay in the user's session (and in AURA's double ⇧ cycle) — unload them on exit
static std::vector<HKL> loadedByTest;
static void unloadTestLayouts() { for (HKL h : loadedByTest) UnloadKeyboardLayout(h); }
static bool sessionHas(HKL h) {
    HKL list[64]; int n = GetKeyboardLayoutList(64, list);
    for (int i = 0; i < n; i++) if (list[i] == h) return true;
    return false;
}
static Layout& layout(const wchar_t* klid) {  // e.g. "00000422" = Ukrainian
    auto it = cache.find(klid); if (it != cache.end()) return it->second;
    HKL before[64]; int nb = GetKeyboardLayoutList(64, before);
    HKL h = LoadKeyboardLayoutW(klid, KLF_NOTELLSHELL);
    if (h && std::find(before, before + nb, h) == before + nb && sessionHas(h)) {
        if (loadedByTest.empty()) atexit(unloadTestLayouts);
        loadedByTest.push_back(h);
    }
    Layout L; if (!h || !buildLayout(h, L)) { printf("no layout %s\n", utf8(klid).c_str()); exit(2); }
    return cache[klid] = L;
}
static wstring lang(const Layout& l) { return l.locale.substr(0, 2); }

// keys that type `word` in layout `meant` (the layout the keys are pressed in doesn't matter: keys are scan codes)
static bool keysFor(const wstring& w, const Layout& meant, vector<Key>& out) {
    out.clear();
    for (wchar_t c : w) if (!meant.keysFor(c, out)) return false;
    return true;
}
// what auto-correction makes of `w` (meant in `meant`) typed while `typed` is active: fixed text, L"" = left alone, L"?" = can't type
static wstring run(const wstring& w, const Layout& meant, const Layout& typed) {
    vector<Key> k; if (!keysFor(w, meant, k) || !typed.maps(k)) return L"?";
    forcedLayout = typed.hkl;
    int j = autoFixTarget(k);
    return j < 0 ? L"" : layouts[j].text(k) + L"\x01";  // \x01: marks "changed", so a fix to the same text still differs from "left alone"
}

static vector<wstring> words(const Layout& l, size_t n) {
    wstring code = lang(l);
    std::ifstream f(utf8(root + L"\\lang\\" + code + L".txt"), std::ios::binary); std::string line;
    while (std::getline(f, line)) if (!line.empty() && line[0] != '#') break;
    vector<wstring> r; std::istringstream ss(line); std::string t;
    bool dict = dictionaryWorks(l.locale);
    while (r.size() < n && ss >> t) {
        wstring w = wide(t); bool letters = w.size() >= 2;
        for (wchar_t c : w) if (!IsCharAlphaW(c)) letters = false;
        // the lists come from film subtitles and have other languages' words: keep what the dictionary accepts
        if (letters && (!dict || isWord(w, l.locale))) r.push_back(w);
    }
    return r;
}

struct Result { std::string name; int ok, total; };
static vector<Result> results;
static vector<std::string> failures;
struct Case { wstring w; const Layout* meant; const Layout* typed; bool fix; };
static void check(const std::string& name, const vector<Case>& cases) {
    int ok = 0, total = 0;
    for (auto& c : cases) {
        wstring got = run(c.w, *c.meant, *c.typed);
        if (got == L"?") continue;
        vector<Key> k; keysFor(c.w, *c.meant, k);
        if (c.fix && c.typed->text(k) == c.w) continue;  // types the same in both layouts: nothing to fix
        total++;
        bool good = c.fix ? got == c.w + L"\x01" : got.empty();
        if (good) ok++;
        else if (failures.size() < 400) failures.push_back(name + ": " + utf8(c.w) + " -> " + (got.empty() ? "unchanged" : utf8(got.substr(0, got.size() - 1))));
    }
    results.push_back({name, ok, total});
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { printf("usage: aura-test <repo> [--update] [-v]\n"); return 2; }
    root = argv[1];
    bool update = false, verbose = false;  // -v: print every result line (by default only what got worse)
    for (int i = 2; i < argc; i++) { update |= wstring(argv[i]) == L"--update"; verbose |= wstring(argv[i]) == L"-v"; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, __uuidof(ISpellCheckerFactory), (void**)&factory);

    const wchar_t* US = L"00000409";
    struct Setup { const char* name; vector<const wchar_t*> ids; };
    vector<Setup> setups = {
        {"uk+en", {US, L"00000422"}}, {"uk+ru+en", {US, L"00000422", L"00000419"}}, {"ru+en", {US, L"00000419"}},
        {"de+en", {US, L"00000407"}}, {"el+en", {US, L"00000408"}}, {"bg+en", {US, L"00030402"}},
        {"he+en", {US, L"0000040D"}}, {"ar+en", {US, L"00000401"}},
    };
    const size_t N = 300;
    for (auto& s : setups) {
        layouts.clear(); for (auto id : s.ids) layouts.push_back(layout(id));
        reliable.clear();
        const Layout& en = layouts[0];
        vector<wstring> enWords = words(en, N);
        for (size_t i = 1; i < layouts.size(); i++) {
            const Layout& l = layouts[i]; std::string p = std::string(s.name) + " " + utf8(lang(l));
            vector<wstring> own = words(l, N);
            vector<Case> a, b, c;
            for (auto& w : own) { a.push_back({w, &l, &en, true}); b.push_back({w, &l, &l, false}); }
            for (auto& w : enWords) c.push_back({w, &en, &l, true});
            check(p + " typed in en: fixed", a);
            check(p + " typed right: kept", b);
            check(std::string(s.name) + " en typed in " + utf8(lang(l)) + ": fixed", c);
        }
        vector<Case> k; for (auto& w : enWords) k.push_back({w, &en, &en, false});
        check(std::string(s.name) + " en typed right: kept", k);
    }

    // hand-picked cases that must always work
    layouts = {layout(US), layout(L"00000422")}; reliable.clear();
    struct Must { int meant, typed; const wchar_t* w; bool fix; };
    vector<Must> must = {
        {1, 0, L"привіт", true}, {0, 1, L"hello", true},
        {0, 1, L"console.log(total);", true}, {0, 1, L"fetchUser(id);", true},
        {0, 0, L"test1", false}, {0, 0, L"h2o", false}, {0, 0, L"github", false},
    };
    // which spell checkers this Windows has (the rest goes through the language models)
    std::string dicts;
    for (auto id : {US, L"00000422", L"00000419", L"00000407", L"00000408", L"00030402", L"0000040D", L"00000401"}) {
        layouts.push_back(layout(id)); dicts += " " + utf8(lang(layout(id))) + (dictionaryWorks(layout(id).locale) ? "+" : "-");
    }
    layouts.resize(2); reliable.clear();
    printf("spell checkers (+ works, - model only):%s\n", dicts.c_str());
    // code typed in the wrong layout is recognized by its English parts: needs the English spell checker
    bool enDict = dictionaryWorks(layouts[0].locale);
    int mustFail = 0;
    for (auto& m : must) {
        if (!enDict && wcspbrk(m.w, L".(")) { printf("skipped (no English spell checker): %s\n", utf8(m.w).c_str()); continue; }
        wstring got = run(m.w, layouts[m.meant], layouts[m.typed]);
        bool good = m.fix ? got == wstring(m.w) + L"\x01" : got.empty();
        if (!good) { mustFail++; printf("MUST FAIL: %s -> %s\n", utf8(m.w).c_str(), got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // the list order mustn't decide: with RU, UK, EN (Windows after switching the UI to Ukrainian)
    // «ghbdsn» is the Ukrainian «привіт», not the Russian look-alike «привыт»
    layouts = {layout(L"00000419"), layout(L"00000422"), layout(US)}; reliable.clear();
    {
        wstring got = run(L"привіт", layouts[1], layouts[2]);
        if (got != L"привіт\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN order): привіт -> %s\n", got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // a Ukrainian name typed in the Russian layout, with no Ukrainian dictionary (Windows has none):
    // needs the Russian spell checker to say «вынницею» isn't Russian. Russian words stay.
    if (dictionaryWorks(layouts[0].locale)) {
        wstring got = run(L"вінницею", layouts[1], layouts[0]);
        if (got != L"вінницею\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN): вінницею in RU -> %s\n", got.empty() ? "unchanged" : utf8(got).c_str()); }
        for (auto w : {L"привет", L"выписка", L"вынести"}) {
            wstring k = run(w, layouts[0], layouts[0]);
            if (!k.empty()) { mustFail++; printf("MUST FAIL (RU,UK,EN): %s in RU -> %s\n", utf8(w).c_str(), utf8(k).c_str()); }
        }
    } else printf("skipped (no Russian spell checker): вінницею in RU\n");
    // same keys give a frequent Russian word and a rare Ukrainian one («было» / «біло»): the frequent one wins
    for (auto w : {L"было", L"была", L"были"}) {
        wstring got = run(w, layouts[0], layouts[2]);
        if (got != wstring(w) + L"\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN): %s typed in EN -> %s\n", utf8(w).c_str(), got.empty() ? "unchanged" : utf8(got).c_str()); }
    }

    std::string out;
    for (auto& r : results) {
        char line[256]; snprintf(line, sizeof line, "%s\t%.1f%%\t%d/%d\n", r.name.c_str(), 100.0 * r.ok / (r.total ? r.total : 1), r.ok, r.total);
        out += line;
    }
    if (verbose) fputs(out.c_str(), stdout);
    { std::ofstream f(utf8(root + L"\\tests\\failures.txt"), std::ios::binary); for (auto& l : failures) f << l << "\n"; }
    // results depend on which spell checkers Windows has: one baseline with all of them installed, one for
    // machines without (the CI runner has only some)
    bool allDicts = dicts.find('-') == std::string::npos;
    std::string baseFile = utf8(root + (allDicts ? L"\\tests\\baseline-windows-dicts.txt" : L"\\tests\\baseline-windows.txt"));
    if (update) { std::ofstream(baseFile, std::ios::binary) << out; printf("baseline updated\n"); return mustFail ? 1 : 0; }
    int worse = 0;
    std::ifstream bf(baseFile, std::ios::binary); std::string l;
    std::map<std::string, double> old;
    while (std::getline(bf, l)) { size_t t = l.find('\t'); if (t != std::string::npos) old[l.substr(0, t)] = atof(l.c_str() + t + 1); }
    for (auto& r : results) {
        double now = 100.0 * r.ok / (r.total ? r.total : 1);
        auto it = old.find(r.name);
        if (it != old.end() && now < it->second - 1.0) { worse++; printf("WORSE: %s %.1f%% (was %.1f%%)\n", r.name.c_str(), now, it->second); }
    }
    printf(mustFail == 0 && worse == 0 ? "OK\n" : "FAILED: %d must-pass, %d worse than baseline (details: tests/failures.txt)\n", mustFail, worse);
    return mustFail == 0 && worse == 0 ? 0 : 1;
}
