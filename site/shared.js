// Texts, language switch and the typing demo for the AURA site.
// Site languages: uk, en, pl, de, es, fr. Russian browsers get Ukrainian (as in the app).
const T = {
  uk: {
    name: "Українська", title: "AURA Switcher — розкладка виправляється сама",
    badge: "Бета 0.9.7 · безкоштовно · без реєстрації",
    h1: "Пишіть.<br>Розкладку <span class=\"hl\">AURA</span> виправить сама",
    lead: "Набрали «ghbdsn» замість «привіт»? AURA помітить і виправить одразу після пробілу. Для macOS і Windows.",
    dlMac: "Завантажити для Mac", dlWin: "Для Windows",
    demoWrong: "Не та розкладка…", demoFixed: "Виправлено після пробілу",
    fTitle: "Все, що потрібно. Нічого зайвого",
    f1h: "Автовиправлення", f1p: "Набрали слово не в тій розкладці — після пробілу воно стає правильним, а розкладка перемикається.",
    f2h: "Подвійний Shift", f2p: "Двічі натисніть <kbd>Shift</kbd> — останнє слово або виділений текст перекладеться в іншу розкладку.",
    f3h: "Мова для програми", f3p: "Terminal — завжди EN, Telegram — УК. А в програмах-винятках AURA просто мовчить.",
    f4h: "Приватно", f4p: "Нічого не надсилає в інтернет. Усе працює на вашому пристрої.",
    f5h: "21 мова", f5p: "Усі розкладки вашої системи.",
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
    badge: "Beta 0.9.7 · free · no sign-up",
    h1: "Just type.<br><span class=\"hl\">AURA</span> fixes the layout",
    lead: "Typed “ghbdsn” instead of “привіт”? AURA notices and fixes it right after Space. For macOS and Windows.",
    dlMac: "Download for Mac", dlWin: "For Windows",
    demoWrong: "Wrong layout…", demoFixed: "Fixed after Space",
    fTitle: "Everything you need. Nothing you don't",
    f1h: "Auto-correction", f1p: "Typed a word in the wrong layout — after Space it becomes right, and the layout switches.",
    f2h: "Double Shift", f2p: "Press <kbd>Shift</kbd> twice — the last word or the selected text is converted to another layout.",
    f3h: "Per-app language", f3p: "Terminal — always EN, Telegram — УК. In excluded apps AURA simply stays quiet.",
    f4h: "Private", f4p: "Sends nothing online. Everything runs on your device.",
    f5h: "21 languages", f5p: "Every layout on your system.",
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
    badge: "Beta 0.9.7 · za darmo · bez rejestracji",
    h1: "Po prostu pisz.<br><span class=\"hl\">AURA</span> poprawi układ",
    lead: "Wpisałeś „ghbdsn” zamiast „привіт”? AURA to zauważy i poprawi zaraz po spacji. Dla macOS i Windows.",
    dlMac: "Pobierz na Maca", dlWin: "Na Windows",
    demoWrong: "Zły układ…", demoFixed: "Poprawione po spacji",
    fTitle: "Wszystko, czego potrzebujesz. Nic zbędnego",
    f1h: "Autokorekta", f1p: "Słowo wpisane w złym układzie po spacji staje się poprawne, a układ się przełącza.",
    f2h: "Podwójny Shift", f2p: "Naciśnij dwa razy <kbd>Shift</kbd> — ostatnie słowo lub zaznaczony tekst przejdzie na inny układ.",
    f3h: "Język dla aplikacji", f3p: "Terminal — zawsze EN, Telegram — УК. W wykluczonych aplikacjach AURA po prostu milczy.",
    f4h: "Prywatnie", f4p: "Niczego nie wysyła do internetu. Wszystko działa na Twoim urządzeniu.",
    f5h: "21 języków", f5p: "Każdy układ w Twoim systemie.",
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
    badge: "Beta 0.9.7 · kostenlos · ohne Anmeldung",
    h1: "Einfach tippen.<br><span class=\"hl\">AURA</span> korrigiert das Layout",
    lead: "„ghbdsn“ statt „привіт“ getippt? AURA bemerkt es und korrigiert es direkt nach der Leertaste. Für macOS und Windows.",
    dlMac: "Für Mac laden", dlWin: "Für Windows",
    demoWrong: "Falsches Layout…", demoFixed: "Nach der Leertaste korrigiert",
    fTitle: "Alles, was du brauchst. Nichts Überflüssiges",
    f1h: "Autokorrektur", f1p: "Ein Wort im falschen Layout getippt — nach der Leertaste ist es richtig, und das Layout wechselt.",
    f2h: "Doppel-Shift", f2p: "Drücke zweimal <kbd>Shift</kbd> — das letzte Wort oder der markierte Text wechselt in ein anderes Layout.",
    f3h: "Sprache pro App", f3p: "Terminal — immer EN, Telegram — УК. In ausgeschlossenen Apps bleibt AURA einfach still.",
    f4h: "Privat", f4p: "Sendet nichts ins Internet. Alles läuft auf deinem Gerät.",
    f5h: "21 Sprachen", f5p: "Jedes Layout deines Systems.",
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
    badge: "Beta 0.9.7 · gratis · sin registro",
    h1: "Solo escribe.<br><span class=\"hl\">AURA</span> corrige la distribución",
    lead: "¿Escribiste «ghbdsn» en lugar de «привіт»? AURA lo detecta y lo corrige justo después del espacio. Para macOS y Windows.",
    dlMac: "Descargar para Mac", dlWin: "Para Windows",
    demoWrong: "Distribución equivocada…", demoFixed: "Corregido tras el espacio",
    fTitle: "Todo lo que necesitas. Nada más",
    f1h: "Autocorrección", f1p: "¿Escribiste una palabra con la distribución equivocada? Tras el espacio queda bien y la distribución cambia.",
    f2h: "Doble Shift", f2p: "Pulsa <kbd>Shift</kbd> dos veces: la última palabra o el texto seleccionado pasa a otra distribución.",
    f3h: "Idioma por app", f3p: "Terminal — siempre EN, Telegram — УК. En las apps excluidas AURA no hace nada.",
    f4h: "Privada", f4p: "No envía nada a internet. Todo funciona en tu dispositivo.",
    f5h: "21 idiomas", f5p: "Todas las distribuciones de tu sistema.",
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
    badge: "Bêta 0.9.7 · gratuit · sans inscription",
    h1: "Tapez, tout simplement.<br><span class=\"hl\">AURA</span> corrige la disposition",
    lead: "Vous avez tapé « ghbdsn » au lieu de « привіт » ? AURA le remarque et corrige juste après l’espace. Pour macOS et Windows.",
    dlMac: "Télécharger pour Mac", dlWin: "Pour Windows",
    demoWrong: "Mauvaise disposition…", demoFixed: "Corrigé après l’espace",
    fTitle: "Tout ce qu’il faut. Rien de superflu",
    f1h: "Correction auto", f1p: "Un mot tapé dans la mauvaise disposition devient correct après l’espace, et la disposition change.",
    f2h: "Double Maj", f2p: "Appuyez deux fois sur <kbd>Shift</kbd> : le dernier mot ou le texte sélectionné passe dans une autre disposition.",
    f3h: "Langue par app", f3p: "Terminal — toujours EN, Telegram — УК. Dans les apps exclues, AURA ne fait rien.",
    f4h: "Privée", f4p: "N’envoie rien sur Internet. Tout fonctionne sur votre appareil.",
    f5h: "21 langues", f5p: "Toutes les dispositions de votre système.",
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
  zh: ["赞助", "喜欢 AURA Switcher 吗？请开发者喝杯咖啡以支持后续开发。"],
};
for (const k in SUPPORT) if (T[k]) { T[k].donate = SUPPORT[k][0]; T[k].supportP = SUPPORT[k][1]; }
// contact link
const CONTACT = { uk: "Написати нам", en: "Contact us", pl: "Napisz do nas", de: "Schreib uns", es: "Escríbenos", fr: "Nous écrire",
  it: "Scrivici", pt: "Fale connosco", tr: "Bize yazın", nl: "Mail ons", cs: "Napište nám", sv: "Skriv till oss", fi: "Kirjoita meille",
  ro: "Scrie-ne", hu: "Írj nekünk", el: "Γράψτε μας", bg: "Пишете ни", he: "כתבו לנו", ar: "راسلنا", hi: "हमें लिखें", ko: "문의하기", zh: "联系我们" };
for (const k in CONTACT) if (T[k]) T[k].contact = CONTACT[k];
// phones: what to do with the link
const PHONE_NOTE = {"uk": "AURA — програма для комп'ютера (Mac і Windows). Надішліть посилання собі й відкрийте його на комп'ютері.",
  "en": "AURA is a computer app (Mac and Windows). Send yourself the link and open it on your computer.",
  "pl": "AURA to program na komputer (Mac i Windows). Wyślij sobie link i otwórz go na komputerze.",
  "de": "AURA ist ein Programm für den Computer (Mac und Windows). Schick dir den Link und öffne ihn am Computer.",
  "es": "AURA es una app para ordenador (Mac y Windows). Envíate el enlace y ábrelo en tu ordenador.",
  "fr": "AURA est une application pour ordinateur (Mac et Windows). Envoyez-vous le lien et ouvrez-le sur votre ordinateur.",
  "it": "AURA è un'app per computer (Mac e Windows). Inviati il link e aprilo sul computer.",
  "pt": "AURA é um app para computador (Mac e Windows). Envie o link para você e abra no computador.",
  "tr": "AURA bir bilgisayar uygulamasıdır (Mac ve Windows). Bağlantıyı kendinize gönderin ve bilgisayarda açın.",
  "nl": "AURA is een app voor de computer (Mac en Windows). Stuur jezelf de link en open hem op je computer.",
  "cs": "AURA je program pro počítač (Mac a Windows). Pošlete si odkaz a otevřete ho na počítači.",
  "sv": "AURA är ett program för datorn (Mac och Windows). Skicka länken till dig själv och öppna den på datorn.",
  "fi": "AURA on tietokoneohjelma (Mac ja Windows). Lähetä linkki itsellesi ja avaa se tietokoneella.",
  "ro": "AURA este o aplicație pentru computer (Mac și Windows). Trimite-ți linkul și deschide-l pe computer.",
  "hu": "Az AURA számítógépes program (Mac és Windows). Küldd el magadnak a linket, és nyisd meg a számítógépen.",
  "el": "Το AURA είναι εφαρμογή για υπολογιστή (Mac και Windows). Στείλτε τον σύνδεσμο στον εαυτό σας και ανοίξτε τον στον υπολογιστή.",
  "bg": "AURA е програма за компютър (Mac и Windows). Изпратете си линка и го отворете на компютъра.",
  "he": "AURA היא תוכנה למחשב (Mac ו-Windows). שלחו לעצמכם את הקישור ופתחו אותו במחשב.",
  "ar": "AURA برنامج للكمبيوتر (Mac وWindows). أرسل الرابط إلى نفسك وافتحه على الكمبيوتر.",
  "hi": "AURA कंप्यूटर का ऐप है (Mac और Windows)। लिंक ख़ुद को भेजें और कंप्यूटर पर खोलें।",
  "ko": "AURA는 컴퓨터용 프로그램입니다(Mac, Windows). 링크를 나에게 보내고 컴퓨터에서 여세요.",
  "zh": "AURA 是电脑软件（Mac 和 Windows）。把链接发给自己，在电脑上打开。"};
for (const k in PHONE_NOTE) if (T[k]) T[k].phoneNote = PHONE_NOTE[k];
// phone visitors (nothing to install there) + "Is it safe?" block:
// [phoneBtn, phoneCopied, safeH, safe1h, safe1p, safe2h, safe2p, safe3h, safe3p]
const GH = '<a href="https://github.com/klbk/aura-switcher-app" target="_blank" rel="noopener">GitHub</a>';
const EXTRA = {
  uk: ["Надіслати посилання собі", "Посилання скопійовано",
       "Це безпечно?", "Відкритий код", `Увесь код лежить на ${GH}: будь-хто може перевірити, що робить програма.`,
       "Навіщо доступ до клавіатури", "Щоб бачити набране слово й виправити його. Текст не зберігається й нікуди не надсилається, інтернет програмі не потрібен.",
       "Попередження під час запуску — це нормально", "Бета поки без платного підпису Apple і Microsoft. Mac: «Готово» → Системні параметри → Конфіденційність і безпека → «Усе одно відкрити». Windows: «Докладніше» → «Усе одно запустити»."],
  en: ["Send the link to myself", "Link copied",
       "Is it safe?", "Open source", `All the code is on ${GH}, so anyone can check what the app does.`,
       "Why it needs keyboard access", "To see the word you typed and fix it. Text is never stored or sent anywhere; the app doesn't need the internet.",
       "A warning on first launch is normal", "The beta isn't signed with a paid Apple or Microsoft certificate yet. Mac: “Done” → System Settings → Privacy & Security → “Open Anyway”. Windows: “More info” → “Run anyway”."],
  pl: ["Wyślij link do siebie", "Link skopiowany",
       "Czy to bezpieczne?", "Otwarty kod", `Cały kod jest na ${GH} — każdy może sprawdzić, co robi program.`,
       "Po co dostęp do klawiatury", "Żeby zobaczyć wpisane słowo i je poprawić. Tekst nie jest zapisywany ani nigdzie wysyłany, program nie potrzebuje internetu.",
       "Ostrzeżenie przy uruchomieniu to normalne", "Beta nie ma jeszcze płatnego podpisu Apple i Microsoft. Mac: „Gotowe” → Ustawienia systemowe → Prywatność i ochrona → „Otwórz mimo to”. Windows: „Więcej informacji” → „Uruchom mimo to”."],
  de: ["Link an mich senden", "Link kopiert",
       "Ist das sicher?", "Offener Quellcode", `Der gesamte Code liegt auf ${GH} — jeder kann prüfen, was das Programm tut.`,
       "Wozu der Tastaturzugriff", "Um das getippte Wort zu sehen und zu korrigieren. Text wird nicht gespeichert und nirgends hingeschickt, Internet braucht das Programm nicht.",
       "Eine Warnung beim ersten Start ist normal", "Die Beta hat noch keine kostenpflichtige Signatur von Apple und Microsoft. Mac: „Fertig“ → Systemeinstellungen → Datenschutz & Sicherheit → „Dennoch öffnen“. Windows: „Weitere Informationen“ → „Trotzdem ausführen“."],
  es: ["Enviarme el enlace", "Enlace copiado",
       "¿Es seguro?", "Código abierto", `Todo el código está en ${GH}: cualquiera puede comprobar qué hace el programa.`,
       "Por qué necesita acceso al teclado", "Para ver la palabra escrita y corregirla. El texto no se guarda ni se envía a ningún sitio; el programa no necesita internet.",
       "Un aviso al abrirlo por primera vez es normal", "La beta aún no tiene firma de pago de Apple ni de Microsoft. Mac: «OK» → Ajustes del Sistema → Privacidad y seguridad → «Abrir igualmente». Windows: «Más información» → «Ejecutar de todas formas»."],
  fr: ["M’envoyer le lien", "Lien copié",
       "Est-ce sûr ?", "Code ouvert", `Tout le code est sur ${GH} : chacun peut vérifier ce que fait le logiciel.`,
       "Pourquoi l’accès au clavier", "Pour voir le mot tapé et le corriger. Le texte n’est ni enregistré ni envoyé ; le logiciel n’a pas besoin d’Internet.",
       "Un avertissement au premier lancement est normal", "La bêta n’a pas encore de signature payante Apple ou Microsoft. Mac : « OK » → Réglages Système → Confidentialité et sécurité → « Ouvrir quand même ». Windows : « Informations complémentaires » → « Exécuter quand même »."],
  it: ["Inviami il link", "Link copiato",
       "È sicuro?", "Open source", `Tutto il codice è su ${GH}: chiunque può verificare cosa fa il programma.`,
       "Perché serve l’accesso alla tastiera", "Per leggere la parola digitata e correggerla. Il testo non viene salvato né inviato da nessuna parte; il programma non ha bisogno di Internet.",
       "Un avviso al primo avvio è normale", "La beta non è ancora firmata con un certificato a pagamento di Apple o Microsoft. Mac: «Fine» → Impostazioni di Sistema → Privacy e sicurezza → «Apri comunque». Windows: «Ulteriori informazioni» → «Esegui comunque»."],
  pt: ["Enviar o link para mim", "Link copiado",
       "É seguro?", "Código aberto", `Todo o código está no ${GH}: qualquer pessoa pode verificar o que o programa faz.`,
       "Porquê acesso ao teclado", "Para ver a palavra escrita e corrigi-la. O texto não é guardado nem enviado; o programa não precisa de internet.",
       "Um aviso na primeira abertura é normal", "A beta ainda não tem assinatura paga da Apple e da Microsoft. Mac: «OK» → Definições do Sistema → Privacidade e segurança → «Abrir mesmo assim». Windows: «Mais informações» → «Executar mesmo assim»."],
  tr: ["Bağlantıyı kendime gönder", "Bağlantı kopyalandı",
       "Güvenli mi?", "Açık kaynak", `Tüm kod ${GH} üzerinde: programın ne yaptığını herkes kontrol edebilir.`,
       "Klavye erişimi neden gerekli", "Yazılan kelimeyi görüp düzeltmek için. Metin saklanmaz ve hiçbir yere gönderilmez; programın internete ihtiyacı yoktur.",
       "İlk açılışta uyarı çıkması normaldir", "Beta henüz ücretli Apple ve Microsoft imzası taşımıyor. Mac: “Bitti” → Sistem Ayarları → Gizlilik ve Güvenlik → “Yine de Aç”. Windows: “Ek bilgi” → “Yine de çalıştır”."],
  nl: ["Link naar mezelf sturen", "Link gekopieerd",
       "Is het veilig?", "Open source", `Alle code staat op ${GH}: iedereen kan nagaan wat het programma doet.`,
       "Waarom toegang tot het toetsenbord", "Om het getypte woord te zien en te verbeteren. Tekst wordt niet opgeslagen of verstuurd; het programma heeft geen internet nodig.",
       "Een waarschuwing bij de eerste start is normaal", "De bèta heeft nog geen betaalde handtekening van Apple en Microsoft. Mac: ‘Gereed’ → Systeeminstellingen → Privacy en beveiliging → ‘Open toch’. Windows: ‘Meer informatie’ → ‘Toch uitvoeren’."],
  cs: ["Poslat si odkaz", "Odkaz zkopírován",
       "Je to bezpečné?", "Otevřený kód", `Celý kód je na ${GH} — kdokoli si může ověřit, co program dělá.`,
       "Proč přístup ke klávesnici", "Aby viděl napsané slovo a opravil ho. Text se neukládá ani nikam neposílá, program nepotřebuje internet.",
       "Varování při prvním spuštění je normální", "Beta zatím nemá placený podpis Apple a Microsoft. Mac: „Hotovo“ → Nastavení systému → Soukromí a zabezpečení → „Přesto otevřít“. Windows: „Další informace“ → „Přesto spustit“."],
  sv: ["Skicka länken till mig", "Länken kopierad",
       "Är det säkert?", "Öppen källkod", `All kod finns på ${GH} — vem som helst kan kontrollera vad programmet gör.`,
       "Varför tangentbordsåtkomst", "För att se ordet du skrev och rätta det. Text sparas inte och skickas ingenstans; programmet behöver inte internet.",
       "En varning vid första start är normalt", "Betan saknar ännu betald signatur från Apple och Microsoft. Mac: ”Klar” → Systeminställningar → Integritet och säkerhet → ”Öppna ändå”. Windows: ”Mer information” → ”Kör ändå”."],
  fi: ["Lähetä linkki itselleni", "Linkki kopioitu",
       "Onko se turvallinen?", "Avoin lähdekoodi", `Kaikki koodi on ${GH}issa — kuka tahansa voi tarkistaa, mitä ohjelma tekee.`,
       "Miksi näppäimistön käyttöoikeus", "Jotta kirjoitettu sana nähdään ja korjataan. Tekstiä ei tallenneta eikä lähetetä minnekään; ohjelma ei tarvitse internetiä.",
       "Varoitus ensimmäisellä käynnistyksellä on normaali", "Betalla ei vielä ole maksullista Applen ja Microsoftin allekirjoitusta. Mac: ”Valmis” → Järjestelmäasetukset → Tietosuoja ja turvallisuus → ”Avaa silti”. Windows: ”Lisätietoja” → ”Suorita silti”."],
  ro: ["Trimite-mi linkul", "Link copiat",
       "Este sigur?", "Cod deschis", `Tot codul este pe ${GH}: oricine poate verifica ce face programul.`,
       "De ce acces la tastatură", "Ca să vadă cuvântul tastat și să-l corecteze. Textul nu este salvat și nu este trimis nicăieri; programul nu are nevoie de internet.",
       "Un avertisment la prima pornire este normal", "Beta nu are încă semnătură plătită Apple și Microsoft. Mac: „Gata” → Configurări sistem → Confidențialitate și securitate → „Deschide oricum”. Windows: „Mai multe informații” → „Rulează oricum”."],
  hu: ["Link küldése magamnak", "Link kimásolva",
       "Biztonságos?", "Nyílt forráskód", `A teljes kód a ${GH}on van — bárki ellenőrizheti, mit csinál a program.`,
       "Miért kell hozzáférés a billentyűzethez", "Hogy lássa a beírt szót és kijavítsa. A szöveget nem menti és nem küldi sehová, a programnak nem kell internet.",
       "Figyelmeztetés az első indításkor — ez normális", "A béta még nem rendelkezik fizetős Apple- és Microsoft-aláírással. Mac: „Kész” → Rendszerbeállítások → Adatvédelem és biztonság → „Megnyitás mindenképp”. Windows: „További információ” → „Futtatás mindenképp”."],
  el: ["Αποστολή συνδέσμου σε μένα", "Ο σύνδεσμος αντιγράφηκε",
       "Είναι ασφαλές;", "Ανοιχτός κώδικας", `Όλος ο κώδικας είναι στο ${GH}: ο καθένας μπορεί να ελέγξει τι κάνει το πρόγραμμα.`,
       "Γιατί χρειάζεται πρόσβαση στο πληκτρολόγιο", "Για να βλέπει τη λέξη που γράψατε και να τη διορθώνει. Το κείμενο δεν αποθηκεύεται ούτε στέλνεται πουθενά· το πρόγραμμα δεν χρειάζεται internet.",
       "Μια προειδοποίηση στο πρώτο άνοιγμα είναι φυσιολογική", "Η beta δεν έχει ακόμη πληρωμένη υπογραφή Apple και Microsoft. Mac: «Τέλος» → Ρυθμίσεις συστήματος → Απόρρητο και ασφάλεια → «Άνοιγμα ούτως ή άλλως». Windows: «Περισσότερες πληροφορίες» → «Εκτέλεση ούτως ή άλλως»."],
  bg: ["Изпрати ми линка", "Линкът е копиран",
       "Безопасно ли е?", "Отворен код", `Целият код е в ${GH} — всеки може да провери какво прави програмата.`,
       "Защо е нужен достъп до клавиатурата", "За да вижда написаната дума и да я поправи. Текстът не се пази и не се изпраща никъде; програмата не се нуждае от интернет.",
       "Предупреждение при първо стартиране е нормално", "Бетата още няма платен подпис от Apple и Microsoft. Mac: „Готово“ → Системни настройки → Поверителност и сигурност → „Отвори въпреки това“. Windows: „Повече информация“ → „Изпълни въпреки това“."],
  he: ["שלחו לי את הקישור", "הקישור הועתק",
       "האם זה בטוח?", "קוד פתוח", `כל הקוד נמצא ב-${GH}: כל אחד יכול לבדוק מה התוכנה עושה.`,
       "למה נדרשת גישה למקלדת", "כדי לראות את המילה שהוקלדה ולתקן אותה. הטקסט לא נשמר ולא נשלח לשום מקום; התוכנה לא צריכה אינטרנט.",
       "אזהרה בהפעלה הראשונה היא תקינה", "לגרסת הבטא עדיין אין חתימה בתשלום של Apple ו-Microsoft. Mac: „סיום” ← הגדרות המערכת ← פרטיות ואבטחה ← „פתח בכל זאת”. Windows: „מידע נוסף” ← „הפעל בכל זאת”."],
  ar: ["أرسل الرابط إليّ", "تم نسخ الرابط",
       "هل هو آمن؟", "مفتوح المصدر", `كل الشيفرة على ${GH}: يمكن لأي شخص التحقق مما يفعله البرنامج.`,
       "لماذا يحتاج الوصول إلى لوحة المفاتيح", "ليرى الكلمة المكتوبة ويصححها. لا يُحفظ النص ولا يُرسل إلى أي مكان، والبرنامج لا يحتاج إلى الإنترنت.",
       "ظهور تحذير عند التشغيل الأول أمر طبيعي", "النسخة التجريبية ليست موقّعة بعد بشهادة مدفوعة من Apple وMicrosoft. ‏Mac: «تم» ← إعدادات النظام ← الخصوصية والأمن ← «فتح على أي حال». ‏Windows: «مزيد من المعلومات» ← «تشغيل على أي حال»."],
  hi: ["लिंक ख़ुद को भेजें", "लिंक कॉपी हो गया",
       "क्या यह सुरक्षित है?", "ओपन सोर्स", `पूरा कोड ${GH} पर है — कोई भी जाँच सकता है कि प्रोग्राम क्या करता है।`,
       "कीबोर्ड एक्सेस क्यों चाहिए", "टाइप किया शब्द देखने और उसे ठीक करने के लिए। टेक्स्ट न सहेजा जाता है, न कहीं भेजा जाता है; प्रोग्राम को इंटरनेट की ज़रूरत नहीं।",
       "पहली बार खोलने पर चेतावनी आना सामान्य है", "बीटा पर अभी Apple और Microsoft का सशुल्क हस्ताक्षर नहीं है। Mac: “Done” → System Settings → Privacy & Security → “Open Anyway”। Windows: “More info” → “Run anyway”।"],
  ko: ["나에게 링크 보내기", "링크를 복사했습니다",
       "안전한가요?", "오픈 소스", `모든 코드가 ${GH}에 있어 누구나 프로그램이 하는 일을 확인할 수 있습니다.`,
       "키보드 접근 권한이 필요한 이유", "입력한 단어를 보고 고치기 위해서입니다. 텍스트는 저장되거나 어디로도 전송되지 않으며 인터넷이 필요 없습니다.",
       "처음 실행할 때 경고가 뜨는 것은 정상입니다", "베타에는 아직 Apple과 Microsoft의 유료 서명이 없습니다. Mac: '완료' → 시스템 설정 → 개인정보 보호 및 보안 → '그래도 열기'. Windows: '추가 정보' → '실행'."],
  zh: ["把链接发给自己", "链接已复制", "它安全吗？", "安全透明", `所有源代码均托管在 ${GH} 上，任何人都可以审查代码。`, "为什么它需要键盘访问权限", "仅用于识别你输入的文本并进行纠错。软件不会保存或向任何地方发送输入内容，且应用本身无需联网。", "首次启动时提示安全警告是正常现象", "测试版尚未购买昂贵的 Apple 或 Microsoft 开发者签名证书。Mac：点击“好” → 系统设置 → 隐私与安全性 → 点击“仍要打开”。Windows：点击“更多信息” → “仍要运行”。"],
};
const EXTRA_KEYS = ["phoneBtn", "phoneCopied", "safeH", "safe1h", "safe1p", "safe2h", "safe2p", "safe3h", "safe3p"];
for (const k in T) { const e = EXTRA[k] || EXTRA.en; EXTRA_KEYS.forEach((key, i) => T[k][key] = e[i]); }
// The app's menu, shown on the page (strings from engine.swift uiStrings)
const MENU_KEYS = ["menabled", "mauto", "msound", "mapp", "mexclude", "mappLang", "mexclusions", "mbindings"];
const MENU = {
  en: ["Enabled", "Auto-correction", "Sound", "App: ", "Don't work in this app", "Language for this app", "Exclusions", "Language bindings"],
  uk: ["Увімкнено", "Автовиправлення", "Звук", "Програма: ", "Не працювати в цій програмі", "Мова для цієї програми", "Винятки", "Прив'язки мов"],
  pl: ["Włączone", "Autokorekta", "Dźwięk", "Aplikacja: ", "Nie działaj w tej aplikacji", "Język dla tej aplikacji", "Wyjątki", "Przypisania języków"],
  de: ["Aktiviert", "Autokorrektur", "Ton", "App: ", "In dieser App nicht aktiv", "Sprache für diese App", "Ausnahmen", "Sprachzuordnungen"],
  es: ["Activado", "Autocorrección", "Sonido", "App: ", "No funcionar en esta app", "Idioma para esta app", "Excepciones", "Idiomas asignados"],
  fr: ["Activé", "Correction automatique", "Son", "App : ", "Ne pas fonctionner dans cette app", "Langue pour cette app", "Exceptions", "Langues associées"],
  it: ["Attivo", "Correzione automatica", "Suono", "App: ", "Non funzionare in questa app", "Lingua per questa app", "Eccezioni", "Lingue associate"],
  pt: ["Ativado", "Correção automática", "Som", "App: ", "Não funcionar nesta app", "Idioma para esta app", "Exceções", "Idiomas associados"],
  tr: ["Etkin", "Otomatik düzeltme", "Ses", "Uygulama: ", "Bu uygulamada çalışma", "Bu uygulama için dil", "İstisnalar", "Dil atamaları"],
  nl: ["Ingeschakeld", "Autocorrectie", "Geluid", "App: ", "Niet werken in deze app", "Taal voor deze app", "Uitzonderingen", "Taalkoppelingen"],
  cs: ["Zapnuto", "Automatické opravy", "Zvuk", "Aplikace: ", "Nefungovat v této aplikaci", "Jazyk pro tuto aplikaci", "Výjimky", "Přiřazení jazyků"],
  sv: ["Aktiverad", "Autokorrigering", "Ljud", "App: ", "Arbeta inte i den här appen", "Språk för den här appen", "Undantag", "Språkkopplingar"],
  fi: ["Käytössä", "Automaattinen korjaus", "Ääni", "Ohjelma: ", "Älä toimi tässä ohjelmassa", "Kieli tälle ohjelmalle", "Poikkeukset", "Kieliliitokset"],
  ro: ["Activat", "Corectare automată", "Sunet", "Aplicație: ", "Nu funcționa în această aplicație", "Limba pentru această aplicație", "Excepții", "Limbi asociate"],
  hu: ["Bekapcsolva", "Automatikus javítás", "Hang", "Alkalmazás: ", "Ne működjön ebben az alkalmazásban", "Nyelv ehhez az alkalmazáshoz", "Kivételek", "Nyelvi hozzárendelések"],
  el: ["Ενεργό", "Αυτόματη διόρθωση", "Ήχος", "Εφαρμογή: ", "Να μη λειτουργεί σε αυτή την εφαρμογή", "Γλώσσα για αυτή την εφαρμογή", "Εξαιρέσεις", "Αντιστοιχίσεις γλωσσών"],
  bg: ["Включено", "Автокорекция", "Звук", "Програма: ", "Да не работи в тази програма", "Език за тази програма", "Изключения", "Обвързани езици"],
  he: ["מופעל", "תיקון אוטומטי", "צליל", "יישום: ", "לא לפעול ביישום זה", "שפה ליישום זה", "חריגים", "שיוכי שפה"],
  ar: ["مفعّل", "التصحيح التلقائي", "الصوت", "التطبيق: ", "لا تعمل في هذا التطبيق", "لغة هذا التطبيق", "الاستثناءات", "ربط اللغات"],
  hi: ["चालू", "स्वतः सुधार", "ध्वनि", "ऐप: ", "इस ऐप में काम न करें", "इस ऐप की भाषा", "अपवाद", "भाषा बंधन"],
  ko: ["켜기", "자동 수정", "소리", "앱: ", "이 앱에서 작동 안 함", "이 앱의 언어", "예외", "언어 지정"],
  zh: ["已启用", "自动纠错", "音效", "应用： ", "不在该应用中启用", "此应用语言", "不生效应用列表", "语言绑定"],
};
for (const k in T) { const e = MENU[k] || MENU.en; MENU_KEYS.forEach((key, i) => T[k][key] = e[i]); }
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
  const W = "https://apps.microsoft.com/detail/9PP3MK8CFCJK";
  document.querySelectorAll('a[href$="AURA-Switcher-mac.zip"]').forEach(a => {
    const w = a.parentElement.querySelector(`a[href="${W}"]`);
    if (w) { a.before(w); w.classList.replace("ghost", "light"); a.classList.replace("light", "ghost"); }
    else { a.href = W; a.removeAttribute("data-t"); a.innerHTML = '⊞ <span data-t="dlWin"></span>'; }
  });
}
// phones and tablets: nothing to install there — offer to send the link to a computer instead of downloads
if (/Android|iPhone|iPad|iPod|Mobile/i.test(navigator.userAgent) || (navigator.platform === "MacIntel" && navigator.maxTouchPoints > 1)) {
  document.documentElement.classList.add("mobile");
  document.querySelectorAll(".sendlink").forEach(b => b.onclick = async () => {
    const url = location.href.split("#")[0];
    if (window.goatcounter && goatcounter.count) goatcounter.count({ path: "send-link", title: "Send the link to myself", event: true });
    if (navigator.share) { try { await navigator.share({ title: document.title, url }); } catch {} return; }
    try { await navigator.clipboard.writeText(url); b.textContent = T[lang].phoneCopied; } catch {}
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
