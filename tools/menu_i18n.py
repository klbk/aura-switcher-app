#!/usr/bin/env python3
"""Menu translations for both apps. Edit here, then run: python3 tools/menu_i18n.py
Rewrites the generated blocks in main.swift and windows/aura.cpp (between the BEGIN/END markers).
Russian system → Ukrainian menu (no Russian translation on purpose)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KEYS = ["enabled", "auto", "sound", "access", "app", "exclude", "game", "appLang", "noBinding", "exclusions", "recent",
        "bindings", "empty", "login", "words", "wordsTitle", "wordsInfo", "save", "cancel", "uiLang", "system", "hint",
        "report", "reportBody", "donate", "quit"]
# code, native name, then one line per key (in KEYS order)
LANGS = """
en English
Enabled
Auto-correction
Sound
⚠︎ Grant access: Accessibility
App:
Don't work in this app
Game — AURA paused
Language for this app
No binding
Exclusions
Don't work in (recent apps)
Language bindings
Empty
Launch at login
Word exceptions…
AURA Switcher — word exceptions
AURA Switcher will never auto-correct these words. Separate them with commas.
Save
Cancel
Menu language
System
Double ⇧ — convert word/selection
Report a problem…
What happened and in which app:
Support AURA Switcher ☕
Quit

uk Українська
Увімкнено
Автовиправлення
Звук
⚠︎ Надайте доступ: Універсальний доступ
Програма:
Не працювати в цій програмі
Гра — AURA на паузі
Мова для цієї програми
Без прив'язки
Винятки
Не працювати в (останні програми)
Прив'язки мов
Порожньо
Запускати під час входу
Слова-винятки…
AURA Switcher — слова-винятки
Ці слова AURA Switcher ніколи не виправлятиме автоматично. Через кому.
Зберегти
Скасувати
Мова меню
Системна
Подвійний ⇧ — змінити мову слова/виділення
Повідомити про проблему…
Що сталося і в якій програмі:
Підтримати AURA Switcher ☕
Вихід

pl Polski
Włączone
Autokorekta
Dźwięk
⚠︎ Nadaj dostęp: Dostępność
Aplikacja:
Nie działaj w tej aplikacji
Gra — AURA wstrzymana
Język dla tej aplikacji
Bez przypisania
Wyjątki
Nie działaj w (ostatnie aplikacje)
Przypisania języków
Pusto
Uruchamiaj przy logowaniu
Wyjątki słów…
AURA Switcher — wyjątki słów
AURA Switcher nigdy nie poprawi automatycznie tych słów. Oddziel je przecinkami.
Zapisz
Anuluj
Język menu
Systemowy
Podwójny ⇧ — zmień układ słowa/zaznaczenia
Zgłoś problem…
Co się stało i w jakiej aplikacji:
Wesprzyj AURA Switcher ☕
Zakończ

de Deutsch
Aktiviert
Autokorrektur
Ton
⚠︎ Zugriff erlauben: Bedienungshilfen
App:
In dieser App nicht aktiv
Spiel — AURA pausiert
Sprache für diese App
Keine Zuordnung
Ausnahmen
Nicht aktiv in (letzte Apps)
Sprachzuordnungen
Leer
Bei Anmeldung starten
Wortausnahmen…
AURA Switcher — Wortausnahmen
Diese Wörter korrigiert AURA Switcher nie automatisch. Durch Kommas trennen.
Sichern
Abbrechen
Menüsprache
System
Doppel-⇧ — Wort/Auswahl umwandeln
Problem melden…
Was ist passiert und in welcher App:
AURA Switcher unterstützen ☕
Beenden

es Español
Activado
Autocorrección
Sonido
⚠︎ Conceder acceso: Accesibilidad
App:
No funcionar en esta app
Juego — AURA en pausa
Idioma para esta app
Sin asignación
Excepciones
No funcionar en (apps recientes)
Idiomas asignados
Vacío
Abrir al iniciar sesión
Palabras excluidas…
AURA Switcher — palabras excluidas
AURA Switcher nunca corregirá estas palabras automáticamente. Sepáralas con comas.
Guardar
Cancelar
Idioma del menú
Sistema
Doble ⇧ — convertir palabra/selección
Informar de un problema…
Qué pasó y en qué app:
Apoyar AURA Switcher ☕
Salir

fr Français
Activé
Correction automatique
Son
⚠︎ Autoriser l'accès : Accessibilité
App :
Ne pas fonctionner dans cette app
Jeu — AURA en pause
Langue pour cette app
Aucune association
Exceptions
Ne pas fonctionner dans (apps récentes)
Langues associées
Vide
Ouvrir à la connexion
Mots exclus…
AURA Switcher — mots exclus
AURA Switcher ne corrigera jamais ces mots automatiquement. Séparez-les par des virgules.
Enregistrer
Annuler
Langue du menu
Système
Double ⇧ — convertir le mot/la sélection
Signaler un problème…
Ce qui s'est passé et dans quelle app :
Soutenir AURA Switcher ☕
Quitter

it Italiano
Attivo
Correzione automatica
Suono
⚠︎ Concedi accesso: Accessibilità
App:
Non funzionare in questa app
Gioco — AURA in pausa
Lingua per questa app
Nessuna associazione
Eccezioni
Non funzionare in (app recenti)
Lingue associate
Vuoto
Apri al login
Parole escluse…
AURA Switcher — parole escluse
AURA Switcher non correggerà mai automaticamente queste parole. Separale con virgole.
Salva
Annulla
Lingua del menu
Sistema
Doppio ⇧ — converti parola/selezione
Segnala un problema…
Cosa è successo e in quale app:
Sostieni AURA Switcher ☕
Esci

pt Português
Ativado
Correção automática
Som
⚠︎ Conceder acesso: Acessibilidade
App:
Não funcionar nesta app
Jogo — AURA em pausa
Idioma para esta app
Sem associação
Exceções
Não funcionar em (apps recentes)
Idiomas associados
Vazio
Abrir ao iniciar sessão
Palavras excluídas…
AURA Switcher — palavras excluídas
O AURA Switcher nunca corrigirá estas palavras automaticamente. Separe-as com vírgulas.
Guardar
Cancelar
Idioma do menu
Sistema
Duplo ⇧ — converter palavra/seleção
Comunicar um problema…
O que aconteceu e em que app:
Apoiar o AURA Switcher ☕
Sair

tr Türkçe
Etkin
Otomatik düzeltme
Ses
⚠︎ Erişim izni verin: Erişilebilirlik
Uygulama:
Bu uygulamada çalışma
Oyun — AURA duraklatıldı
Bu uygulama için dil
Atama yok
İstisnalar
Şurada çalışma (son uygulamalar)
Dil atamaları
Boş
Oturum açılışında başlat
Kelime istisnaları…
AURA Switcher — kelime istisnaları
AURA Switcher bu kelimeleri asla otomatik düzeltmez. Virgülle ayırın.
Kaydet
İptal
Menü dili
Sistem
Çift ⇧ — kelimeyi/seçimi dönüştür
Sorun bildir…
Ne oldu ve hangi uygulamada:
AURA Switcher'ı destekle ☕
Çık

nl Nederlands
Ingeschakeld
Autocorrectie
Geluid
⚠︎ Geef toegang: Toegankelijkheid
App:
Niet werken in deze app
Game — AURA gepauzeerd
Taal voor deze app
Geen koppeling
Uitzonderingen
Niet werken in (recente apps)
Taalkoppelingen
Leeg
Starten bij inloggen
Woorduitzonderingen…
AURA Switcher — woorduitzonderingen
AURA Switcher corrigeert deze woorden nooit automatisch. Scheid ze met komma's.
Bewaar
Annuleer
Menutaal
Systeem
Dubbel ⇧ — woord/selectie omzetten
Probleem melden…
Wat er gebeurde en in welke app:
Steun AURA Switcher ☕
Stop

cs Čeština
Zapnuto
Automatické opravy
Zvuk
⚠︎ Udělte přístup: Zpřístupnění
Aplikace:
Nefungovat v této aplikaci
Hra — AURA pozastavena
Jazyk pro tuto aplikaci
Bez přiřazení
Výjimky
Nefungovat v (nedávné aplikace)
Přiřazení jazyků
Prázdné
Spouštět po přihlášení
Výjimky slov…
AURA Switcher — výjimky slov
Tato slova AURA Switcher nikdy automaticky neopraví. Oddělte je čárkami.
Uložit
Zrušit
Jazyk nabídky
Systémový
Dvojitý ⇧ — převést slovo/výběr
Nahlásit problém…
Co se stalo a v jaké aplikaci:
Podpořit AURA Switcher ☕
Ukončit

sv Svenska
Aktiverad
Autokorrigering
Ljud
⚠︎ Ge åtkomst: Hjälpmedel
App:
Arbeta inte i den här appen
Spel — AURA pausad
Språk för den här appen
Ingen koppling
Undantag
Arbeta inte i (senaste appar)
Språkkopplingar
Tomt
Starta vid inloggning
Ordundantag…
AURA Switcher — ordundantag
AURA Switcher rättar aldrig dessa ord automatiskt. Separera med kommatecken.
Spara
Avbryt
Menyspråk
System
Dubbel-⇧ — konvertera ord/markering
Rapportera ett problem…
Vad hände och i vilken app:
Stöd AURA Switcher ☕
Avsluta

fi Suomi
Käytössä
Automaattinen korjaus
Ääni
⚠︎ Anna käyttöoikeus: Käyttöapu
Ohjelma:
Älä toimi tässä ohjelmassa
Peli — AURA tauolla
Kieli tälle ohjelmalle
Ei liitosta
Poikkeukset
Älä toimi ohjelmassa (viimeisimmät)
Kieliliitokset
Tyhjä
Käynnistä kirjautuessa
Sanapoikkeukset…
AURA Switcher — sanapoikkeukset
AURA Switcher ei koskaan korjaa näitä sanoja automaattisesti. Erota pilkuilla.
Tallenna
Kumoa
Valikon kieli
Järjestelmä
Kaksois-⇧ — muunna sana/valinta
Ilmoita ongelmasta…
Mitä tapahtui ja missä ohjelmassa:
Tue AURA Switcheriä ☕
Lopeta

ro Română
Activat
Corectare automată
Sunet
⚠︎ Acordați acces: Accesibilitate
Aplicație:
Nu funcționa în această aplicație
Joc — AURA în pauză
Limba pentru această aplicație
Fără asociere
Excepții
Nu funcționa în (aplicații recente)
Limbi asociate
Gol
Pornește la autentificare
Cuvinte excluse…
AURA Switcher — cuvinte excluse
AURA Switcher nu va corecta niciodată automat aceste cuvinte. Separați-le prin virgulă.
Salvează
Anulează
Limba meniului
Sistem
Dublu ⇧ — convertește cuvântul/selecția
Raportează o problemă…
Ce s-a întâmplat și în ce aplicație:
Susține AURA Switcher ☕
Ieșire

hu Magyar
Bekapcsolva
Automatikus javítás
Hang
⚠︎ Hozzáférés megadása: Kisegítő lehetőségek
Alkalmazás:
Ne működjön ebben az alkalmazásban
Játék — AURA szüneteltetve
Nyelv ehhez az alkalmazáshoz
Nincs hozzárendelés
Kivételek
Ne működjön itt (legutóbbi alkalmazások)
Nyelvi hozzárendelések
Üres
Indítás bejelentkezéskor
Szókivételek…
AURA Switcher — szókivételek
Ezeket a szavakat az AURA Switcher soha nem javítja automatikusan. Vesszővel válassza el.
Mentés
Mégse
Menü nyelve
Rendszer
Dupla ⇧ — szó/kijelölés átalakítása
Probléma bejelentése…
Mi történt és melyik alkalmazásban:
Az AURA Switcher támogatása ☕
Kilépés

el Ελληνικά
Ενεργό
Αυτόματη διόρθωση
Ήχος
⚠︎ Δώστε πρόσβαση: Προσβασιμότητα
Εφαρμογή:
Να μη λειτουργεί σε αυτή την εφαρμογή
Παιχνίδι — AURA σε παύση
Γλώσσα για αυτή την εφαρμογή
Χωρίς αντιστοίχιση
Εξαιρέσεις
Να μη λειτουργεί σε (πρόσφατες εφαρμογές)
Αντιστοιχίσεις γλωσσών
Κενό
Εκκίνηση κατά τη σύνδεση
Εξαιρέσεις λέξεων…
AURA Switcher — εξαιρέσεις λέξεων
Το AURA Switcher δεν θα διορθώνει ποτέ αυτόματα αυτές τις λέξεις. Χωρίστε τις με κόμμα.
Αποθήκευση
Ακύρωση
Γλώσσα μενού
Συστήματος
Διπλό ⇧ — μετατροπή λέξης/επιλογής
Αναφορά προβλήματος…
Τι συνέβη και σε ποια εφαρμογή:
Υποστηρίξτε το AURA Switcher ☕
Έξοδος

bg Български
Включено
Автокорекция
Звук
⚠︎ Дайте достъп: Достъпност
Програма:
Да не работи в тази програма
Игра — AURA е на пауза
Език за тази програма
Без обвързване
Изключения
Да не работи в (последни програми)
Обвързани езици
Празно
Стартиране при вход
Думи-изключения…
AURA Switcher — думи-изключения
AURA Switcher никога няма да поправя автоматично тези думи. Разделете ги със запетая.
Запази
Отказ
Език на менюто
Системен
Двоен ⇧ — смяна на езика на дума/селекция
Съобщете за проблем…
Какво се случи и в коя програма:
Подкрепете AURA Switcher ☕
Изход

he עברית
מופעל
תיקון אוטומטי
צליל
⚠︎ תנו גישה: נגישות
יישום:
לא לפעול ביישום זה
משחק — AURA מושהית
שפה ליישום זה
ללא שיוך
חריגים
לא לפעול ב (יישומים אחרונים)
שיוכי שפה
ריק
הפעלה בעת התחברות
מילים חריגות…
AURA Switcher — מילים חריגות
AURA Switcher לעולם לא יתקן מילים אלה אוטומטית. הפרידו בפסיקים.
שמירה
ביטול
שפת התפריט
מערכת
⇧ כפול — המרת מילה/בחירה
דיווח על בעיה…
מה קרה ובאיזה יישום:
תמכו ב-AURA Switcher ☕
יציאה

ar العربية
مفعّل
التصحيح التلقائي
الصوت
⚠︎ امنح الوصول: تسهيلات الاستخدام
التطبيق:
لا تعمل في هذا التطبيق
لعبة — AURA متوقف مؤقتًا
لغة هذا التطبيق
بدون ربط
الاستثناءات
لا تعمل في (التطبيقات الأخيرة)
ربط اللغات
فارغ
التشغيل عند تسجيل الدخول
كلمات مستثناة…
AURA Switcher — كلمات مستثناة
لن يصحح AURA Switcher هذه الكلمات تلقائيًا أبدًا. افصل بينها بفواصل.
حفظ
إلغاء
لغة القائمة
النظام
⇧ مزدوج — تحويل الكلمة/التحديد
الإبلاغ عن مشكلة…
ماذا حدث وفي أي تطبيق:
ادعم AURA Switcher ☕
إنهاء

hi हिन्दी
चालू
स्वतः सुधार
ध्वनि
⚠︎ अनुमति दें: सुलभता
ऐप:
इस ऐप में काम न करें
गेम — AURA रुका हुआ
इस ऐप की भाषा
कोई बंधन नहीं
अपवाद
इनमें काम न करें (हाल के ऐप)
भाषा बंधन
खाली
लॉगिन पर शुरू करें
शब्द अपवाद…
AURA Switcher — शब्द अपवाद
AURA Switcher इन शब्दों को कभी अपने आप नहीं सुधारेगा। अल्पविराम से अलग करें।
सहेजें
रद्द करें
मेनू की भाषा
सिस्टम
दो बार ⇧ — शब्द/चयन बदलें
समस्या बताएँ…
क्या हुआ और किस ऐप में:
AURA Switcher का समर्थन करें ☕
बंद करें

ko 한국어
켜기
자동 수정
소리
⚠︎ 접근 권한 허용: 손쉬운 사용
앱:
이 앱에서 작동 안 함
게임 — AURA 일시 중지됨
이 앱의 언어
지정 안 함
예외
작동 안 함 (최근 앱)
언어 지정
비어 있음
로그인 시 실행
예외 단어…
AURA Switcher — 예외 단어
AURA Switcher는 이 단어들을 자동으로 수정하지 않습니다. 쉼표로 구분하세요.
저장
취소
메뉴 언어
시스템
⇧ 두 번 — 단어/선택 영역 변환
문제 신고…
무슨 일이 어느 앱에서 있었나요:
AURA Switcher 후원하기 ☕
종료
"""

langs = []  # (code, name, [strings])
for block in LANGS.strip().split("\n\n"):
    head, *lines = block.split("\n")
    code, name = head.split(" ", 1)
    assert len(lines) == len(KEYS), (code, len(lines))
    lines[KEYS.index("app")] += " "  # "App: " + app name
    langs.append((code, name, lines))

def esc(s): return s.replace("\\", "\\\\").replace('"', '\\"')

def put(path, begin, end, body):
    p = ROOT / path; s = p.read_text()
    s2, n = re.subn(re.escape(begin) + r".*?" + re.escape(end), lambda _: begin + "\n" + body + end, s, flags=re.S)
    assert n == 1, path
    p.write_text(s2)

B, E = "// BEGIN menu strings — generated by tools/menu_i18n.py, edit there", "// END menu strings"
swift = "let uiLangs = [" + ", ".join(f'"{c}"' for c, _, _ in langs) + "]\n"
swift += "let uiLangNames = [" + ", ".join(f'"{n}"' for _, n, _ in langs) + "]\n"
swift += "let uiStrings: [String: [String]] = [ // key: one string per uiLangs\n"
for i, k in enumerate(KEYS):
    swift += f'    "{k}": [' + ", ".join(f'"{esc(l[i])}"' for _, _, l in langs) + "],\n"
swift += "]\n"
put("main.swift", B, E, swift)

cpp = f"static const int UI_N = {len(langs)};\n"
cpp += "static const wchar_t* const UI_LANGS[UI_N] = {" + ", ".join(f'L"{c}"' for c, _, _ in langs) + "};\n"
cpp += "static const wchar_t* const UI_NAMES[UI_N] = {" + ", ".join(f'L"{n}"' for _, n, _ in langs) + "};\n"
cpp += "static const wchar_t* const UI_STR[][UI_N] = { // row: English first, then the other UI_LANGS\n"
for i, k in enumerate(KEYS):
    cpp += "    {" + ", ".join(f'L"{esc(l[i])}"' for _, _, l in langs) + "},\n"
cpp += "};\n"
put("windows/aura.cpp", B, E, cpp)
print(f"{len(langs)} languages × {len(KEYS)} strings")
