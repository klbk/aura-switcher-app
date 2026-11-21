// Linux auto-correction test: the same cases as tests/windows.cpp, on the real engine (linux/aura.cpp) with
// xkb keyboard layouts and the built-in language models — no app, no key events, no desktop.
// Built and run by tests/run-linux.sh:
//   aura-test <repo>            — run, compare with tests/baseline-linux.txt
//   aura-test <repo> --update   — accept the current results as the new baseline
//   aura-test <repo> --dicts    — also use the Hunspell dictionaries of this machine (no baseline: they differ everywhere)
#define AURA_TEST
#include "../linux/aura.cpp"

static std::string root;
static std::map<std::string, Layout> cache;
static Layout& layout(const char* xkb) {  // e.g. "ua" = Ukrainian
    auto it = cache.find(xkb); if (it != cache.end()) return it->second;
    Layout L; if (!buildLayout({xkb, "", ""}, int(cache.size()), L)) { printf("no layout %s\n", xkb); exit(2); }
    return cache[xkb] = L;
}
static wstring lang(const Layout& l) { return l.locale.substr(0, 2); }

// keys that type `word` in layout `meant` (the layout the keys are pressed in doesn't matter: keys are physical)
static bool keysFor(const wstring& w, const Layout& meant, vector<Key>& out) {
    out.clear();
    for (wchar_t c : w) if (!meant.keysFor(c, out)) return false;
    return true;
}
// what auto-correction makes of `w` (meant in `meant`) typed while `typed` is active: fixed text, L"" = left alone, L"?" = can't type
static wstring run(const wstring& w, const Layout& meant, const Layout& typed) {
    vector<Key> k; if (!keysFor(w, meant, k) || !typed.maps(k)) return L"?";
    forcedLayout = typed.group;
    int j = autoFixTarget(k);
    return j < 0 ? L"" : layouts[j].text(k) + L"\x01";  // \x01: marks "changed", so a fix to the same text still differs from "left alone"
}

static vector<wstring> words(const Layout& l, size_t n) {
    std::ifstream f(root + "/lang/" + utf8(lang(l)) + ".txt", std::ios::binary); std::string line;
    while (std::getline(f, line)) if (!line.empty() && line[0] != '#') break;
    vector<wstring> r; std::istringstream ss(line); std::string t;
    bool dict = dictionaryWorks(l.locale);
    while (r.size() < n && ss >> t) {
        wstring w = wide(t); bool letters = w.size() >= 2;
        for (wchar_t c : w) if (!isAlpha(c)) letters = false;
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

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: aura-test <repo> [--update] [--dicts] [-v]\n"); return 2; }
    if (!setlocale(LC_CTYPE, "C.UTF-8") && !setlocale(LC_CTYPE, "en_US.UTF-8")) setlocale(LC_CTYPE, "");
    root = argv[1];
    bool update = false, verbose = false, dictsWanted = false;  // -v: print every result line (by default only what got worse)
    for (int i = 2; i < argc; i++) { std::string a = argv[i]; update |= a == "--update"; verbose |= a == "-v"; dictsWanted |= a == "--dicts"; }
    noSpell = !dictsWanted;  // the baseline is the language models alone: the same on every machine

    struct Setup { const char* name; vector<const char*> ids; };
    vector<Setup> setups = {
        {"uk+en", {"us", "ua"}}, {"uk+ru+en", {"us", "ua", "ru"}}, {"ru+en", {"us", "ru"}},
        {"de+en", {"us", "de"}}, {"el+en", {"us", "gr"}}, {"bg+en", {"us", "bg"}},
        {"he+en", {"us", "il"}}, {"ar+en", {"us", "ara"}},
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
    layouts = {layout("us"), layout("ua")}; reliable.clear();
    struct Must { int meant, typed; const wchar_t* w; bool fix; };
    vector<Must> must = {
        {1, 0, L"привіт", true}, {0, 1, L"hello", true},
        {0, 1, L"console.log(total);", true}, {0, 1, L"fetchUser(id);", true},
        {0, 0, L"test1", false}, {0, 0, L"h2o", false}, {0, 0, L"github", false},
    };
    // which dictionaries this machine has (the rest goes through the language models)
    std::string dicts;
    for (auto id : {"us", "ua", "ru", "de", "gr", "bg", "il", "ara"}) {
        layouts.push_back(layout(id)); dicts += " " + utf8(lang(layout(id))) + (dictionaryWorks(layout(id).locale) ? "+" : "-");
    }
    layouts.resize(2); reliable.clear();
    printf("dictionaries (+ works, - model only):%s\n", dicts.c_str());
    // code typed in the wrong layout is recognized by its English parts: needs the English dictionary
    bool enDict = dictionaryWorks(layouts[0].locale);
    int mustFail = 0;
    for (auto& m : must) {
        if (!enDict && wcspbrk(m.w, L".(")) { printf("skipped (no English dictionary): %s\n", utf8(m.w).c_str()); continue; }
        wstring got = run(m.w, layouts[m.meant], layouts[m.typed]);
        bool good = m.fix ? got == wstring(m.w) + L"\x01" : got.empty();
        if (!good) { mustFail++; printf("MUST FAIL: %s -> %s\n", utf8(m.w).c_str(), got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // the list order mustn't decide: with RU, UK, EN «ghbdsn» is the Ukrainian «привіт», not the Russian look-alike «привыт»
    layouts = {layout("ru"), layout("ua"), layout("us")}; reliable.clear();
    {
        wstring got = run(L"привіт", layouts[1], layouts[2]);
        if (got != L"привіт\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN order): привіт -> %s\n", got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // a Ukrainian name typed in the Russian layout: needs the Russian dictionary to say «вынницею» isn't Russian.
    // Russian words stay.
    if (dictionaryWorks(layouts[0].locale)) {
        wstring got = run(L"вінницею", layouts[1], layouts[0]);
        if (got != L"вінницею\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN): вінницею in RU -> %s\n", got.empty() ? "unchanged" : utf8(got).c_str()); }
        for (auto w : {L"привет", L"выписка", L"вынести"}) {
            wstring k = run(w, layouts[0], layouts[0]);
            if (!k.empty()) { mustFail++; printf("MUST FAIL (RU,UK,EN): %s in RU -> %s\n", utf8(w).c_str(), utf8(k).c_str()); }
        }
    } else printf("skipped (no Russian dictionary): вінницею in RU\n");
    // same keys give a frequent Russian word and a rare Ukrainian one («было» / «біло»): the frequent one wins
    for (auto w : {L"было", L"была", L"были"}) {
        wstring got = run(w, layouts[0], layouts[2]);
        if (got != wstring(w) + L"\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN): %s typed in EN -> %s\n", utf8(w).c_str(), got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // a comma stuck to the word doesn't break the layout check: «itqlthfv?» is «шейдерам,» (Russian layout, typed in EN)
    {
        wstring got = run(L"шейдерам,", layouts[0], layouts[2]);
        if (got != L"шейдерам,\x01") { mustFail++; printf("MUST FAIL (RU,UK,EN): шейдерам, typed in EN -> %s\n", got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    // «ns» is «ты» (ru) and «ті» (uk): with equal evidence the language of the last non-Latin word wins
    for (int want : {0, 1}) {
        recentLang = layouts[want].locale; recentAt = nowMs();
        wstring got = run(want == 0 ? L"ты" : L"ті", layouts[want], layouts[2]);
        if (got != (want == 0 ? L"ты\x01" : L"ті\x01")) { mustFail++; printf("MUST FAIL (RU,UK,EN): ns after %s word -> %s\n", want ? "uk" : "ru", got.empty() ? "unchanged" : utf8(got).c_str()); }
    }
    recentLang.clear(); recentAt = 0;
    // a dead key and the letter after it are one accented letter (German layout: ´ then e → é), there and back
    {
        const Layout& de = layout("de"); vector<Key> k;
        if (!keysFor(L"café", de, k) || k.size() != 5 || de.text(k) != L"café") { mustFail++; printf("MUST FAIL (dead key): café -> %d keys, %s\n", int(k.size()), utf8(de.text(k)).c_str()); }
    }

    std::string out;
    for (auto& r : results) {
        char line[256]; snprintf(line, sizeof line, "%s\t%.1f%%\t%d/%d\n", r.name.c_str(), 100.0 * r.ok / (r.total ? r.total : 1), r.ok, r.total);
        out += line;
    }
    if (verbose) fputs(out.c_str(), stdout);
    { std::ofstream f(root + "/tests/failures.txt", std::ios::binary); for (auto& l : failures) f << l << "\n"; }
    if (dictsWanted) { printf(mustFail ? "FAILED: %d must-pass\n" : "OK (with this machine's dictionaries: not compared with the baseline)\n", mustFail); return mustFail ? 1 : 0; }
    std::string baseFile = root + "/tests/baseline-linux.txt";
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
