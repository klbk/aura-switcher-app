// Texts, language switch and the typing demo for the AURA site.
// Site languages: uk, en, pl, de, es, fr. Russian browsers get Ukrainian (as in the app).
const T = {
  uk: {
    name: "Українська", title: "AURA Switcher — розкладка виправляється сама",
    badge: "Бета 0.7 · безкоштовно · без реєстрації",
    h1: "Пишіть.<br>Розкладку <span class=\"hl\">AURA</span> виправить сама",
    lead: "Набрали «ghbdsn» замість «привіт»? AURA помітить і виправить одразу після пробілу. Для macOS і Windows.",
    dlMac: "Завантажити для Mac", dlWin: "Для Windows",
    demoWrong: "Не та розкладка…", demoFixed: "Виправлено після пробілу",
    fTitle: "Все, що потрібно. Нічого зайвого",
    f1h: "Автовиправлення", f1p: "Набрали слово не в тій розкладці — після пробілу воно стає правильним, а розкладка перемикається.",
    f2h: "Подвійний Shift", f2p: "Двічі натисніть <kbd>Shift</kbd> — останнє слово або виділений текст перекладеться в іншу розкладку.",
    f3h: "Мова для програми", f3p: "Terminal — завжди EN, Telegram — УК. А в програмах-винятках AURA просто мовчить.",
    f4h: "Приватно", f4p: "Нічого не надсилає в інтернет. Усе працює на вашому пристрої.",
    f5h: "21 мова", f5p: "Українська, англійська, німецька, польська, грецька… Усі розкладки вашої системи.",
    f6h: "Легка", f6p: "Менше 1 МБ, не гальмує набір і не підвішує систему.",
    pTitle: "Mac і Windows — однакові звички",
    pMac: "<li>Apple Silicon і Intel</li><li>Код мови в рядку меню: EN, УК, РУ…</li><li>macOS 13 і новіші</li>",
    pWin: "<li>x64 і ARM64, без встановлення</li><li>Код мови в треї</li><li>Ті самі клавіші, що й на Mac</li>",
    iTitle: "Запуск за хвилину",
    s1h: "Завантажте", s1p: "Розпакуйте архів. На Mac перетягніть AURA у «Програми», на Windows просто запустіть AURA.exe.",
    s2h: "Дозвольте", s2p: "На Mac: Системні параметри → Конфіденційність і безпека → Універсальний доступ → AURA.",
    s3h: "Пишіть", s3p: "Код мови з'явиться вгорі (Mac) або в треї (Windows). Далі AURA працює сама.",
    ctaH: "Спробуйте AURA сьогодні", ctaP: "Безкоштовна бета для macOS і Windows.",
    made: "Зроблено в Україні", foot: "AURA — щоб думати про текст, а не про розкладку."
  },
  en: {
    name: "English", title: "AURA Switcher — your keyboard layout fixes itself",
    badge: "Beta 0.7 · free · no sign-up",
    h1: "Just type.<br><span class=\"hl\">AURA</span> fixes the layout",
    lead: "Typed “ghbdsn” instead of “привіт”? AURA notices and fixes it right after Space. For macOS and Windows.",
    dlMac: "Download for Mac", dlWin: "For Windows",
    demoWrong: "Wrong layout…", demoFixed: "Fixed after Space",
    fTitle: "Everything you need. Nothing you don't",
    f1h: "Auto-correction", f1p: "Typed a word in the wrong layout — after Space it becomes right, and the layout switches.",
    f2h: "Double Shift", f2p: "Press <kbd>Shift</kbd> twice — the last word or the selected text is converted to another layout.",
    f3h: "Per-app language", f3p: "Terminal — always EN, Telegram — УК. In excluded apps AURA simply stays quiet.",
    f4h: "Private", f4p: "Sends nothing online. Everything runs on your device.",
    f5h: "21 languages", f5p: "Ukrainian, English, German, Polish, Greek… Every layout on your system.",
    f6h: "Lightweight", f6p: "Under 1 MB, never slows down your typing or freezes the system.",
    pTitle: "Mac and Windows — same habits",
    pMac: "<li>Apple Silicon and Intel</li><li>Language code in the menu bar: EN, УК, DE…</li><li>macOS 13 and later</li>",
    pWin: "<li>x64 and ARM64, no installer</li><li>Language code in the tray</li><li>Same keys as on Mac</li>",
    iTitle: "Up and running in a minute",
    s1h: "Download", s1p: "Unzip the archive. On Mac drag AURA into Applications, on Windows just run AURA.exe.",
    s2h: "Allow", s2p: "On Mac: System Settings → Privacy & Security → Accessibility → AURA.",
    s3h: "Type", s3p: "The language code appears in the menu bar (Mac) or the tray (Windows). AURA does the rest.",
    ctaH: "Try AURA today", ctaP: "Free beta for macOS and Windows.",
    made: "Made in Ukraine", foot: "AURA — think about your text, not your keyboard layout."
  },
  pl: {
    name: "Polski", title: "AURA Switcher — układ klawiatury poprawia się sam",
    badge: "Beta 0.7 · za darmo · bez rejestracji",
    h1: "Po prostu pisz.<br><span class=\"hl\">AURA</span> poprawi układ",
    lead: "Wpisałeś „ghbdsn” zamiast „привіт”? AURA to zauważy i poprawi zaraz po spacji. Dla macOS i Windows.",
    dlMac: "Pobierz na Maca", dlWin: "Na Windows",
    demoWrong: "Zły układ…", demoFixed: "Poprawione po spacji",
    fTitle: "Wszystko, czego potrzebujesz. Nic zbędnego",
    f1h: "Autokorekta", f1p: "Słowo wpisane w złym układzie po spacji staje się poprawne, a układ się przełącza.",
    f2h: "Podwójny Shift", f2p: "Naciśnij dwa razy <kbd>Shift</kbd> — ostatnie słowo lub zaznaczony tekst przejdzie na inny układ.",
    f3h: "Język dla aplikacji", f3p: "Terminal — zawsze EN, Telegram — УК. W wykluczonych aplikacjach AURA po prostu milczy.",
    f4h: "Prywatnie", f4p: "Niczego nie wysyła do internetu. Wszystko działa na Twoim urządzeniu.",
    f5h: "21 języków", f5p: "Ukraiński, angielski, niemiecki, polski, grecki… Każdy układ w Twoim systemie.",
    f6h: "Lekka", f6p: "Poniżej 1 MB, nie spowalnia pisania i nie zawiesza systemu.",
    pTitle: "Mac i Windows — te same nawyki",
    pMac: "<li>Apple Silicon i Intel</li><li>Kod języka na pasku menu: EN, УК, PL…</li><li>macOS 13 i nowsze</li>",
    pWin: "<li>x64 i ARM64, bez instalacji</li><li>Kod języka w zasobniku</li><li>Te same klawisze co na Macu</li>",
    iTitle: "Gotowe w minutę",
    s1h: "Pobierz", s1p: "Rozpakuj archiwum. Na Macu przeciągnij AURA do Aplikacji, na Windows po prostu uruchom AURA.exe.",
    s2h: "Zezwól", s2p: "Na Macu: Ustawienia systemowe → Prywatność i ochrona → Dostępność → AURA.",
    s3h: "Pisz", s3p: "Kod języka pojawi się na pasku menu (Mac) lub w zasobniku (Windows). Resztą zajmie się AURA.",
    ctaH: "Wypróbuj AURA już dziś", ctaP: "Darmowa beta dla macOS i Windows.",
    made: "Stworzono w Ukrainie", foot: "AURA — myśl o tekście, nie o układzie klawiatury."
  },
  de: {
    name: "Deutsch", title: "AURA Switcher — das Tastaturlayout korrigiert sich selbst",
    badge: "Beta 0.7 · kostenlos · ohne Anmeldung",
    h1: "Einfach tippen.<br><span class=\"hl\">AURA</span> korrigiert das Layout",
    lead: "„ghbdsn“ statt „привіт“ getippt? AURA bemerkt es und korrigiert es direkt nach der Leertaste. Für macOS und Windows.",
    dlMac: "Für Mac laden", dlWin: "Für Windows",
    demoWrong: "Falsches Layout…", demoFixed: "Nach der Leertaste korrigiert",
    fTitle: "Alles, was du brauchst. Nichts Überflüssiges",
    f1h: "Autokorrektur", f1p: "Ein Wort im falschen Layout getippt — nach der Leertaste ist es richtig, und das Layout wechselt.",
    f2h: "Doppel-Shift", f2p: "Drücke zweimal <kbd>Shift</kbd> — das letzte Wort oder der markierte Text wechselt in ein anderes Layout.",
    f3h: "Sprache pro App", f3p: "Terminal — immer EN, Telegram — УК. In ausgeschlossenen Apps bleibt AURA einfach still.",
    f4h: "Privat", f4p: "Sendet nichts ins Internet. Alles läuft auf deinem Gerät.",
    f5h: "21 Sprachen", f5p: "Ukrainisch, Englisch, Deutsch, Polnisch, Griechisch… Jedes Layout deines Systems.",
    f6h: "Leicht", f6p: "Unter 1 MB, bremst dein Tippen nicht und friert das System nie ein.",
    pTitle: "Mac und Windows — gleiche Gewohnheiten",
    pMac: "<li>Apple Silicon und Intel</li><li>Sprachcode in der Menüleiste: EN, УК, DE…</li><li>macOS 13 und neuer</li>",
    pWin: "<li>x64 und ARM64, ohne Installation</li><li>Sprachcode im Infobereich</li><li>Dieselben Tasten wie auf dem Mac</li>",
    iTitle: "In einer Minute startklar",
    s1h: "Laden", s1p: "Archiv entpacken. Auf dem Mac AURA in „Programme“ ziehen, unter Windows einfach AURA.exe starten.",
    s2h: "Erlauben", s2p: "Auf dem Mac: Systemeinstellungen → Datenschutz & Sicherheit → Bedienungshilfen → AURA.",
    s3h: "Tippen", s3p: "Der Sprachcode erscheint in der Menüleiste (Mac) oder im Infobereich (Windows). Den Rest macht AURA.",
    ctaH: "Probiere AURA noch heute", ctaP: "Kostenlose Beta für macOS und Windows.",
    made: "Hergestellt in der Ukraine", foot: "AURA — denk an deinen Text, nicht an dein Tastaturlayout."
  },
  es: {
    name: "Español", title: "AURA Switcher — la distribución del teclado se corrige sola",
    badge: "Beta 0.7 · gratis · sin registro",
    h1: "Solo escribe.<br><span class=\"hl\">AURA</span> corrige la distribución",
    lead: "¿Escribiste «ghbdsn» en lugar de «привіт»? AURA lo detecta y lo corrige justo después del espacio. Para macOS y Windows.",
    dlMac: "Descargar para Mac", dlWin: "Para Windows",
    demoWrong: "Distribución equivocada…", demoFixed: "Corregido tras el espacio",
    fTitle: "Todo lo que necesitas. Nada más",
    f1h: "Autocorrección", f1p: "¿Escribiste una palabra con la distribución equivocada? Tras el espacio queda bien y la distribución cambia.",
    f2h: "Doble Shift", f2p: "Pulsa <kbd>Shift</kbd> dos veces: la última palabra o el texto seleccionado pasa a otra distribución.",
    f3h: "Idioma por app", f3p: "Terminal — siempre EN, Telegram — УК. En las apps excluidas AURA no hace nada.",
    f4h: "Privada", f4p: "No envía nada a internet. Todo funciona en tu dispositivo.",
    f5h: "21 idiomas", f5p: "Ucraniano, inglés, alemán, polaco, griego… Todas las distribuciones de tu sistema.",
    f6h: "Ligera", f6p: "Menos de 1 MB, nunca ralentiza la escritura ni bloquea el sistema.",
    pTitle: "Mac y Windows — los mismos hábitos",
    pMac: "<li>Apple Silicon e Intel</li><li>Código de idioma en la barra de menús: EN, УК, ES…</li><li>macOS 13 o posterior</li>",
    pWin: "<li>x64 y ARM64, sin instalación</li><li>Código de idioma en la bandeja</li><li>Las mismas teclas que en Mac</li>",
    iTitle: "Listo en un minuto",
    s1h: "Descarga", s1p: "Descomprime el archivo. En Mac arrastra AURA a Aplicaciones; en Windows ejecuta AURA.exe.",
    s2h: "Permite", s2p: "En Mac: Ajustes del Sistema → Privacidad y seguridad → Accesibilidad → AURA.",
    s3h: "Escribe", s3p: "El código de idioma aparece en la barra de menús (Mac) o en la bandeja (Windows). AURA hace el resto.",
    ctaH: "Prueba AURA hoy", ctaP: "Beta gratuita para macOS y Windows.",
    made: "Hecho en Ucrania", foot: "AURA — piensa en tu texto, no en la distribución del teclado."
  },
  fr: {
    name: "Français", title: "AURA Switcher — la disposition du clavier se corrige toute seule",
    badge: "Bêta 0.7 · gratuit · sans inscription",
    h1: "Tapez, tout simplement.<br><span class=\"hl\">AURA</span> corrige la disposition",
    lead: "Vous avez tapé « ghbdsn » au lieu de « привіт » ? AURA le remarque et corrige juste après l’espace. Pour macOS et Windows.",
    dlMac: "Télécharger pour Mac", dlWin: "Pour Windows",
    demoWrong: "Mauvaise disposition…", demoFixed: "Corrigé après l’espace",
    fTitle: "Tout ce qu’il faut. Rien de superflu",
    f1h: "Correction auto", f1p: "Un mot tapé dans la mauvaise disposition devient correct après l’espace, et la disposition change.",
    f2h: "Double Maj", f2p: "Appuyez deux fois sur <kbd>Shift</kbd> : le dernier mot ou le texte sélectionné passe dans une autre disposition.",
    f3h: "Langue par app", f3p: "Terminal — toujours EN, Telegram — УК. Dans les apps exclues, AURA ne fait rien.",
    f4h: "Privée", f4p: "N’envoie rien sur Internet. Tout fonctionne sur votre appareil.",
    f5h: "21 langues", f5p: "Ukrainien, anglais, allemand, polonais, grec… Toutes les dispositions de votre système.",
    f6h: "Légère", f6p: "Moins de 1 Mo, ne ralentit jamais la frappe ni le système.",
    pTitle: "Mac et Windows — les mêmes habitudes",
    pMac: "<li>Apple Silicon et Intel</li><li>Code de langue dans la barre des menus : EN, УК, FR…</li><li>macOS 13 et plus récent</li>",
    pWin: "<li>x64 et ARM64, sans installation</li><li>Code de langue dans la zone de notification</li><li>Les mêmes touches que sur Mac</li>",
    iTitle: "Prêt en une minute",
    s1h: "Téléchargez", s1p: "Décompressez l’archive. Sur Mac, glissez AURA dans Applications ; sur Windows, lancez AURA.exe.",
    s2h: "Autorisez", s2p: "Sur Mac : Réglages Système → Confidentialité et sécurité → Accessibilité → AURA.",
    s3h: "Tapez", s3p: "Le code de langue apparaît dans la barre des menus (Mac) ou la zone de notification (Windows). AURA s’occupe du reste.",
    ctaH: "Essayez AURA dès aujourd’hui", ctaP: "Bêta gratuite pour macOS et Windows.",
    made: "Fabriqué en Ukraine", foot: "AURA — pensez à votre texte, pas à la disposition du clavier."
  }
};

if (typeof MORE !== "undefined") Object.assign(T, MORE);  // i18n-more.js: the rest of AURA's languages

// support (Donatello) — [button, sentence before it]
const SUPPORT = {
  uk: ["Підтримати", "Подобається AURA Switcher? Підтримайте розробку чашкою кави."],
  en: ["Support", "Like AURA Switcher? Support development with a coffee."],
  pl: ["Wesprzyj", "Podoba Ci się AURA Switcher? Wesprzyj rozwój kawą."],
  de: ["Unterstützen", "Dir gefällt AURA Switcher? Unterstütze die Entwicklung mit einem Kaffee."],
  es: ["Apoyar", "¿Te gusta AURA Switcher? Apoya el desarrollo con un café."],
  fr: ["Soutenir", "Vous aimez AURA Switcher ? Soutenez le développement avec un café."],
  it: ["Sostieni", "Ti piace AURA Switcher? Sostieni lo sviluppo con un caffè."],
  pt: ["Apoiar", "Gosta do AURA Switcher? Apoie o desenvolvimento com um café."],
  tr: ["Destekle", "AURA Switcher’ı beğendiniz mi? Geliştirmeyi bir kahveyle destekleyin."],
  nl: ["Steunen", "Vind je AURA Switcher fijn? Steun de ontwikkeling met een koffie."],
  cs: ["Podpořit", "Líbí se vám AURA Switcher? Podpořte vývoj kávou."],
  sv: ["Stöd", "Gillar du AURA Switcher? Stöd utvecklingen med en kaffe."],
  fi: ["Tue", "Pidätkö AURA Switcheristä? Tue kehitystä kahvilla."],
  ro: ["Susține", "Îți place AURA Switcher? Susține dezvoltarea cu o cafea."],
  hu: ["Támogatás", "Tetszik az AURA Switcher? Támogasd a fejlesztést egy kávéval."],
  el: ["Υποστήριξη", "Σας αρέσει το AURA Switcher; Στηρίξτε την ανάπτυξη με έναν καφέ."],
  bg: ["Подкрепи", "Харесва ли ви AURA Switcher? Подкрепете разработката с кафе."],
  he: ["תמיכה", "אוהבים את AURA Switcher? תמכו בפיתוח בכוס קפה."],
  ar: ["ادعم", "هل أعجبك AURA Switcher؟ ادعم التطوير بفنجان قهوة."],
  hi: ["सहयोग करें", "AURA Switcher पसंद आया? एक कॉफ़ी से विकास में सहयोग करें।"],
  ko: ["후원하기", "AURA Switcher가 마음에 드시나요? 커피 한 잔으로 개발을 후원해 주세요."],
};
for (const k in SUPPORT) if (T[k]) { T[k].donate = SUPPORT[k][0]; T[k].supportP = SUPPORT[k][1]; }
// contact link
const CONTACT = { uk: "Написати нам", en: "Contact us", pl: "Napisz do nas", de: "Schreib uns", es: "Escríbenos", fr: "Nous écrire",
  it: "Scrivici", pt: "Fale connosco", tr: "Bize yazın", nl: "Mail ons", cs: "Napište nám", sv: "Skriv till oss", fi: "Kirjoita meille",
  ro: "Scrie-ne", hu: "Írj nekünk", el: "Γράψτε μας", bg: "Пишете ни", he: "כתבו לנו", ar: "راسلنا", hi: "हमें लिखें", ko: "문의하기" };
for (const k in CONTACT) if (T[k]) T[k].contact = CONTACT[k];
if (typeof SEO !== "undefined") for (const k in SEO) if (T[k]) Object.assign(T[k], SEO[k]);  // seo.js: title, description, "alternative to…"

function detectLang() {
  const page = document.documentElement.dataset.page;  // /<lang>/ pages show their own language
  if (page && T[page]) return page;
  const saved = localStorage.getItem("lang");
  if (saved && T[saved]) return saved;
  for (const l of navigator.languages || [navigator.language]) {
    const c = l.slice(0, 2).toLowerCase();
    if (c === "ru") return "uk";
    if (T[c]) return c;
  }
  return "en";
}

let lang = detectLang();
function setLang(l) {
  lang = l; localStorage.setItem("lang", l);
  document.documentElement.lang = l; document.documentElement.dir = T[l].dir || "ltr"; document.title = T[l].title;
  const desc = document.querySelector('meta[name="description"]'); if (desc && T[l].desc) desc.content = T[l].desc;
  document.querySelectorAll("[data-t]").forEach(e => e.innerHTML = T[l][e.dataset.t]);
  const name = document.getElementById("langName"); if (name) name.textContent = T[l].name;
  document.querySelectorAll(".langlist button").forEach(b => b.classList.toggle("on", b.dataset.l === l));
}
// language picker popover
const picker = document.getElementById("langSel");
if (picker) {
  const list = picker.querySelector(".langlist");
  list.innerHTML = Object.keys(T).map(k => `<button data-l="${k}" role="option">${T[k].name}</button>`).join("");
  picker.querySelector(".langbtn").onclick = e => { e.stopPropagation(); picker.classList.toggle("open"); };
  list.onclick = e => { const b = e.target.closest("button"); if (b) { localStorage.setItem("lang", b.dataset.l); location.href = (document.documentElement.dataset.page ? "../" : "") + b.dataset.l + "/"; } };
  document.addEventListener("click", () => picker.classList.remove("open"));
  document.addEventListener("keydown", e => { if (e.key === "Escape") picker.classList.remove("open"); });
}
// visitors on Windows get the Windows download first (pre-rendered pages are Mac-first)
if (/Windows/.test(navigator.userAgent)) {
  const W = "https://github.com/klbk/aura-switcher/releases/latest/download/AURA-Switcher-windows-x64.zip";
  document.querySelectorAll('a[href$="AURA-Switcher-mac.zip"]').forEach(a => {
    const w = a.parentElement.querySelector(`a[href="${W}"]`);
    if (w) { a.before(w); w.classList.replace("ghost", "light"); a.classList.replace("light", "ghost"); }
    else { a.href = W; a.removeAttribute("data-t"); a.innerHTML = '⊞ <span data-t="dlWin"></span>'; }
  });
}
setLang(lang);

// demo: a word typed in the wrong layout, fixed after Space — examples match the visitor's language
// [typed, fixed, layout it was typed in, layout AURA switches to]; Ukrainian layout = ЙЦУКЕН on QWERTY keys
const words = {
  uk: [["ghbdsn", "привіт", "EN", "УК"], ["руддщ", "hello", "УК", "EN"], ["ntrcn", "текст", "EN", "УК"]],
  en: [["руддщ", "hello", "УК", "EN"], ["kalhmera", "καλημερα", "EN", "ΕΛ"], ["ghbdsn", "привіт", "EN", "УК"], ["akuo", "שלום", "EN", "עב"]],
  pl: [["вщикн", "dobry", "УК", "PL"], ["ghbdsn", "привіт", "PL", "УК"], ["цшефь", "witam", "УК", "PL"]],
  de: [["рфддщ", "hallo", "УК", "DE"], ["ghbdsn", "привіт", "DE", "УК"], ["вфтлу", "danke", "УК", "DE"]],
  es: [["рщдф", "hola", "УК", "ES"], ["ghbdsn", "привіт", "ES", "УК"], ["пкфсшфі", "gracias", "УК", "ES"]],
  fr: [["ищтощгк", "bonjour", "УК", "FR"], ["ghbdsn", "привіт", "FR", "УК"], ["ійдге", "salut", "УК", "FR"]], // AZERTY keys
};
if (typeof MORE_WORDS !== "undefined") Object.assign(words, MORE_WORDS);
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
(async function loop() {
  const text = $("demoText"), chip = $("demoChip"), status = $("demoStatus");
  if (!text) return;
  for (let i = 0, cur = lang; ; i++) {
    if (cur !== lang) { cur = lang; i = 0; }  // language switched — start its own examples
    const list = words[lang] || words.en;
    const [wrong, right, from, to] = list[i % list.length];
    chip.textContent = from; chip.classList.remove("fixed"); status.textContent = "";
    for (let n = 0; n <= wrong.length; n++) { text.innerHTML = `<span class="wrong">${wrong.slice(0, n)}</span><span class="caret"></span>`; await sleep(120); }
    status.textContent = T[lang].demoWrong; await sleep(700);
    text.innerHTML = `<span class="right" dir="auto">${right}</span>&nbsp;<span class="caret"></span>`;
    chip.textContent = to; chip.classList.add("fixed"); status.textContent = T[lang].demoFixed;
    await sleep(2400);
  }
})();
