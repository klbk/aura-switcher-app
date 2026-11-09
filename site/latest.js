// The latest release, for the update page (/update/). Edit on every release: version + what's new.
// Notes: Ukrainian and English; other languages show the English notes.
const LATEST = {
  version: "0.9.8",
  notes: {
    uk: ["Подвійний ⇧ виправляє й слово, набране після Backspace чи стрілок, — зокрема перше слово в щойно очищеному полі.",
         "Mac: AURA знаходить слово на екрані, у якій би розкладці воно не було набране, — у Терміналі виправлення більше не пропускаються. Розкладка перемикається лише після цієї перевірки.",
         "Якщо у вас 0.9 або новіша — «Перевірити оновлення» → «Оновити»."],
    en: ["Double ⇧ fixes the word typed after Backspace or arrows too, including the first word in a field you just cleared.",
         "Mac: AURA finds the word on screen whatever layout it was typed in, so fixes in Terminal are no longer skipped. The layout switches only after this check.",
         "On 0.9 or newer: «Check for updates» → «Update»."],
  },
};
