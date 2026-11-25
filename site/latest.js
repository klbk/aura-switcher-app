// The latest release, for the update page (/update/). Edit on every release: version + what's new.
// Notes: Ukrainian and English; other languages show the English notes.
const LATEST = {
  version: "0.11.3",
  notes: {
    uk: ["0.11.3 — новий пункт меню: не виправляти перше слово після ручної зміни розкладки (за замовчуванням вимкнено; Mac і Windows). Windows: у Блокноті клавіші, набрані під час заміни слова, більше не потрапляють усередину нього. Windows: подвійний Shift по виділенню спрацьовує стабільно, а без слова перемикає розкладку одразу.",
         "0.11.2 — команда, набрана в іншій розкладці, виправляється разом зі знаком на початку: «.сщьзфсе» → «/compact», а не «.compact» (Mac і Windows).",
         "0.11.1 — Windows: у терміналі VS Code (і Cursor) подвійний Shift без набраного слова лише перемикає розкладку й не перериває команду. На сайті — розділ «Часті питання». Mac — без змін.",
         "0.11 — клавішу виправлення можна змінити: подвійний Shift, Ctrl або Alt (або вимкнути). Меню стало компактнішим — налаштування в підменю.",
         "Виділений текст переводиться за словником: у ту розкладку, де слова справжні; знаки по краях слів зберігаються. Працює і в полі пошуку Google.",
         "Виправлення: зайві Backspace після видалення слова більше не ламають наступне слово; пароль-поля не чіпаються (Mac і Windows).",
         "Оновлення Windows вручну: спершу закрийте стару AURA, потім запустіть нову.",
         "Якщо у вас 0.9 або новіша — «Перевірити оновлення» → «Оновити»."],
    en: ["0.11.3 — new menu option: keep the first word after a manual layout switch (off by default; Mac and Windows). Windows: in Notepad, keys typed while a word is being replaced no longer end up inside it. Windows: double Shift on a selection works reliably, and with no word it switches the layout at once.",
         "0.11.2 — a command typed in the other layout is fixed together with the sign in front: «.сщьзфсе» → «/compact», not «.compact» (Mac and Windows).",
         "0.11.1 — Windows: in the VS Code (and Cursor) terminal, double Shift with no word typed only switches the layout and doesn't interrupt the command. The site has an FAQ. Mac: no changes.",
         "0.11 — the correction key is configurable: double Shift, Ctrl or Alt (or off). The menu is more compact — settings are in a submenu.",
         "A selection is converted using the dictionary: into the layout where the words are real; signs at the edges of words stay as typed. Works in the Google search field too.",
         "Fixes: extra Backspaces after a deleted word no longer break the next word; password fields are left alone (Mac and Windows).",
         "Updating Windows by hand: quit the old AURA first, then run the new one.",
         "On 0.9 or newer: «Check for updates» → «Update»."],
  },
};
