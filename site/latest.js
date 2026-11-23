// The latest release, for the update page (/update/). Edit on every release: version + what's new.
// Notes: Ukrainian and English; other languages show the English notes.
const LATEST = {
  version: "0.11",
  notes: {
    uk: ["Клавішу виправлення можна змінити: подвійний Shift, Ctrl або Alt (або вимкнути). Меню стало компактнішим — налаштування в підменю.",
         "Виділений текст переводиться за словником: у ту розкладку, де слова справжні; знаки по краях слів зберігаються. Працює і в полі пошуку Google.",
         "Виправлення: зайві Backspace після видалення слова більше не ламають наступне слово; пароль-поля не чіпаються (Mac і Windows).",
         "Оновлення Windows вручну: спершу закрийте стару AURA, потім запустіть нову.",
         "Якщо у вас 0.9 або новіша — «Перевірити оновлення» → «Оновити»."],
    en: ["The correction key is configurable: double Shift, Ctrl or Alt (or off). The menu is more compact — settings are in a submenu.",
         "A selection is converted using the dictionary: into the layout where the words are real; signs at the edges of words stay as typed. Works in the Google search field too.",
         "Fixes: extra Backspaces after a deleted word no longer break the next word; password fields are left alone (Mac and Windows).",
         "Updating Windows by hand: quit the old AURA first, then run the new one.",
         "On 0.9 or newer: «Check for updates» → «Update»."],
  },
};
