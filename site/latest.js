// The latest release, for the update page (/update/). Edit on every release: version + what's new.
// Notes: Ukrainian and English; other languages show the English notes.
const LATEST = {
  version: "0.9.7",
  notes: {
    uk: ["Слово перед Enter теж виправляється: «z elfkbk⏎» у чаті надішлеться як «я удалил».",
         "Ваше слово — ваше: якщо повернулися до нього (Backspace після пробілу, стрілки) або самі змінили розкладку посеред слова, AURA його не чіпає.",
         "Подвійний ⇧ після двох пробілів лише перемикає розкладку, текст не змінює.",
         "У полях пароля AURA нічого не виправляє.",
         "Однолітерне слово виправляється разом із наступним: «b yt» → «и не». Mac: після виправлення наступні слова розпізнаються правильно, подвійний ⇧ без виділення перемикає одразу.",
         "Якщо у вас 0.9 або новіша — «Перевірити оновлення» → «Оновити»."],
    en: ["The word before Enter is fixed too: «z elfkbk⏎» in a chat goes out as «я удалил».",
         "Your word is yours: go back to it (Backspace after the space, arrows) or switch the layout in the middle of it yourself, and AURA leaves it alone.",
         "Double ⇧ after two spaces only switches the layout and leaves the text.",
         "AURA never changes anything in password fields.",
         "A one-letter word is fixed together with the next one: «b yt» → «и не». Mac: after a fix the next words are read right, double ⇧ with nothing selected switches at once.",
         "On 0.9 or newer: «Check for updates» → «Update»."],
  },
};
