#!/usr/bin/env python3
"""Menu translations for both apps. Edit here, then run: python3 tools/menu_i18n.py
Rewrites the generated blocks in engine.swift and windows/aura.cpp (between the BEGIN/END markers).
Russian system → Ukrainian menu (no Russian translation on purpose)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KEYS = ["enabled", "auto", "sound", "access", "app", "exclude", "game", "appLang", "noBinding", "exclusions", "recent",
        "bindings", "empty", "login", "words", "wordsTitle", "wordsInfo", "save", "cancel", "uiLang", "allLangs", "regEurope", "regCaucasus", "regMiddleEast", "regAsia", "system", "hint",
        "report", "reportBody", "update", "updateDue", "donate", "quit", "updNew", "updLatest", "updNotes", "updNow", "updLater", "updFail", "firstRun", "hotkey", "off", "settings"]
# code, native name, then one line per key (in KEYS order)
LANGS = """
en English
Enabled
Auto-correction
Sound
⚠︎ Grant access: Accessibility, Input Monitoring
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
My words…
AURA Switcher — my words
AURA Switcher treats these words as correct: it never changes them, and fixes them when typed in the wrong layout. Separate them with commas.
Save
Cancel
Menu language
All languages
Europe
Caucasus & Central Asia
Middle East
Asia
System
Double ⇧ — convert word/selection
Report a problem…
What happened and in which app:
Check for updates…
⬆︎ An update may be available — check
Support AURA Switcher ☕
Quit
AURA Switcher {v} is out
You have the latest version
What's new
Update
Later
Couldn't update automatically. Open the website to download it?
AURA Switcher is running. Its icon is in the notification area; click ^ next to the clock to open the menu

uk Українська
Увімкнено
Автовиправлення
Звук
⚠︎ Надайте доступ: Універсальний доступ, Моніторинг введення
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
Мої слова…
AURA Switcher — мої слова
Ці слова AURA Switcher вважає правильними: не змінює їх, а набрані не в тій розкладці — виправляє на них. Через кому.
Зберегти
Скасувати
Мова меню
Усі мови
Європа
Кавказ і Центральна Азія
Близький Схід
Азія
Системна
Подвійний ⇧ — змінити мову слова/виділення
Повідомити про проблему…
Що сталося і в якій програмі:
Перевірити оновлення…
⬆︎ Можливо, вийшло оновлення — перевірити
Підтримати AURA Switcher ☕
Вихід
Вийшла AURA Switcher {v}
У вас остання версія
Що нового
Оновити
Пізніше
Не вдалося оновити автоматично. Відкрити сайт, щоб завантажити?
AURA Switcher працює. Значок — в області сповіщень; натисніть ^ біля годинника, щоб відкрити меню

pl Polski
Włączone
Autokorekta
Dźwięk
⚠︎ Nadaj dostęp: Dostępność, Monitorowanie wejścia
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
Moje słowa…
AURA Switcher — moje słowa
AURA Switcher uznaje te słowa za poprawne: nigdy ich nie zmienia, a wpisane w złym układzie poprawia na nie. Oddziel je przecinkami.
Zapisz
Anuluj
Język menu
Wszystkie języki
Europa
Kaukaz i Azja Środkowa
Bliski Wschód
Azja
Systemowy
Podwójny ⇧ — zmień układ słowa/zaznaczenia
Zgłoś problem…
Co się stało i w jakiej aplikacji:
Sprawdź aktualizacje…
⬆︎ Może być dostępna aktualizacja — sprawdź
Wesprzyj AURA Switcher ☕
Zakończ
Wyszła AURA Switcher {v}
Masz najnowszą wersję
Co nowego
Aktualizuj
Później
Nie udało się zaktualizować automatycznie. Otworzyć stronę, aby pobrać?
AURA Switcher działa. Ikona jest w obszarze powiadomień; kliknij ^ obok zegara, aby otworzyć menu

de Deutsch
Aktiviert
Autokorrektur
Ton
⚠︎ Zugriff erlauben: Bedienungshilfen, Eingabeüberwachung
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
Meine Wörter…
AURA Switcher — meine Wörter
AURA Switcher hält diese Wörter für richtig: ändert sie nie und korrigiert sie, wenn sie im falschen Layout getippt wurden. Durch Kommas trennen.
Sichern
Abbrechen
Menüsprache
Alle Sprachen
Europa
Kaukasus und Zentralasien
Naher Osten
Asien
System
Doppel-⇧ — Wort/Auswahl umwandeln
Problem melden…
Was ist passiert und in welcher App:
Nach Updates suchen…
⬆︎ Möglicherweise gibt es ein Update — prüfen
AURA Switcher unterstützen ☕
Beenden
AURA Switcher {v} ist da
Du hast die neueste Version
Was ist neu
Aktualisieren
Später
Automatisches Update fehlgeschlagen. Website zum Herunterladen öffnen?
AURA Switcher läuft. Das Symbol ist im Infobereich; klicken Sie neben der Uhr auf ^, um das Menü zu öffnen

es Español
Activado
Autocorrección
Sonido
⚠︎ Conceder acceso: Accesibilidad, Monitorización de entrada
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
Mis palabras…
AURA Switcher — mis palabras
AURA Switcher considera correctas estas palabras: nunca las cambia y las corrige si se escriben con la distribución equivocada. Sepáralas con comas.
Guardar
Cancelar
Idioma del menú
Todos los idiomas
Europa
Cáucaso y Asia Central
Oriente Medio
Asia
Sistema
Doble ⇧ — convertir palabra/selección
Informar de un problema…
Qué pasó y en qué app:
Buscar actualizaciones…
⬆︎ Puede haber una actualización — comprobar
Apoyar AURA Switcher ☕
Salir
Ya está aquí AURA Switcher {v}
Tienes la última versión
Novedades
Actualizar
Más tarde
No se pudo actualizar automáticamente. ¿Abrir la web para descargarla?
AURA Switcher está funcionando. Su icono está en el área de notificaciones; haz clic en ^ junto al reloj para abrir el menú

fr Français
Activé
Correction automatique
Son
⚠︎ Autoriser l'accès : Accessibilité, Surveillance de l’entrée
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
Mes mots…
AURA Switcher — mes mots
AURA Switcher considère ces mots comme corrects : il ne les modifie jamais et les corrige s'ils sont tapés avec la mauvaise disposition. Séparez-les par des virgules.
Enregistrer
Annuler
Langue du menu
Toutes les langues
Europe
Caucase et Asie centrale
Moyen-Orient
Asie
Système
Double ⇧ — convertir le mot/la sélection
Signaler un problème…
Ce qui s'est passé et dans quelle app :
Rechercher des mises à jour…
⬆︎ Une mise à jour est peut-être disponible — vérifier
Soutenir AURA Switcher ☕
Quitter
AURA Switcher {v} est sortie
Vous avez la dernière version
Nouveautés
Mettre à jour
Plus tard
Mise à jour automatique impossible. Ouvrir le site pour la télécharger ?
AURA Switcher fonctionne. Son icône est dans la zone de notification ; cliquez sur ^ près de l’horloge pour ouvrir le menu

it Italiano
Attivo
Correzione automatica
Suono
⚠︎ Concedi l’accesso: Accessibilità, Monitoraggio input
App:
Disattiva in questa app
Gioco — AURA in pausa
Lingua per questa app
Nessuna associazione
Esclusioni
Disattiva in (app recenti)
Lingue associate
Vuoto
Apri al login
Le mie parole…
AURA Switcher — le mie parole
AURA Switcher considera corrette queste parole: non le modifica mai e le corregge se le digiti con il layout sbagliato. Separale con una virgola.
Salva
Annulla
Lingua del menu
Tutte le lingue
Europa
Caucaso e Asia centrale
Medio Oriente
Asia
Sistema
Doppio ⇧ — converti parola o selezione
Segnala un problema…
Cosa è successo e in quale app:
Controlla aggiornamenti…
⬆︎ Potrebbe essere disponibile un aggiornamento — verifica
Sostieni AURA Switcher ☕
Esci
È disponibile AURA Switcher {v}
Hai l’ultima versione
Novità
Aggiorna
Più tardi
Impossibile aggiornare automaticamente. Vuoi aprire il sito per scaricare l’aggiornamento?
AURA Switcher è in esecuzione. L’icona è nell’area di notifica; fai clic su ^ accanto all’orologio per aprire il menu

pt Português
Ativado
Correção automática
Som
⚠︎ Conceder acesso: Acessibilidade, Monitoramento de entrada
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
As minhas palavras…
AURA Switcher — as minhas palavras
O AURA Switcher considera estas palavras corretas: nunca as altera e corrige-as se forem digitadas no layout errado. Separe-as com vírgulas.
Guardar
Cancelar
Idioma do menu
Todos os idiomas
Europa
Cáucaso e Ásia Central
Médio Oriente
Ásia
Sistema
Duplo ⇧ — converter palavra/seleção
Comunicar um problema…
O que aconteceu e em que app:
Procurar atualizações…
⬆︎ Pode haver uma atualização — verificar
Apoiar o AURA Switcher ☕
Sair
Saiu o AURA Switcher {v}
Tem a versão mais recente
Novidades
Atualizar
Mais tarde
Não foi possível atualizar automaticamente. Abrir o site para baixar?
O AURA Switcher está a funcionar. O ícone está na área de notificação; clique em ^ junto ao relógio para abrir o menu

tr Türkçe
Etkin
Otomatik düzeltme
Ses
⚠︎ Erişim izni verin: Erişilebilirlik, Girdi İzleme
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
Kelimelerim…
AURA Switcher — kelimelerim
AURA Switcher bu kelimeleri doğru kabul eder: asla değiştirmez, yanlış düzende yazılırsa onlara düzeltir. Virgülle ayırın.
Kaydet
İptal
Menü dili
Tüm diller
Avrupa
Kafkasya ve Orta Asya
Orta Doğu
Asya
Sistem
Çift ⇧ — kelimeyi/seçimi dönüştür
Sorun bildir…
Ne oldu ve hangi uygulamada:
Güncellemeleri denetle…
⬆︎ Güncelleme olabilir — denetle
AURA Switcher'ı destekle ☕
Çık
AURA Switcher {v} çıktı
En son sürümü kullanıyorsunuz
Yenilikler
Güncelle
Daha sonra
Otomatik güncellenemedi. İndirmek için web sitesi açılsın mı?
AURA Switcher çalışıyor. Simgesi bildirim alanında; menüyü açmak için saatin yanındaki ^ simgesine tıklayın

nl Nederlands
Ingeschakeld
Autocorrectie
Geluid
⚠︎ Geef toegang: Toegankelijkheid, Invoercontrole (Input Monitoring)
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
Mijn woorden…
AURA Switcher — mijn woorden
AURA Switcher beschouwt deze woorden als juist: verandert ze nooit en verbetert ze als ze in de verkeerde indeling zijn getypt. Scheid ze met komma's.
Bewaar
Annuleer
Menutaal
Alle talen
Europa
Kaukasus en Centraal-Azië
Midden-Oosten
Azië
Systeem
Dubbel ⇧ — woord/selectie omzetten
Probleem melden…
Wat er gebeurde en in welke app:
Zoek naar updates…
⬆︎ Er is misschien een update — controleren
Steun AURA Switcher ☕
Stop
AURA Switcher {v} is uit
Je hebt de nieuwste versie
Wat is er nieuw
Bijwerken
Later
Automatisch bijwerken is mislukt. De website openen om te downloaden?
AURA Switcher draait. Het pictogram staat in het systeemvak; klik op ^ naast de klok om het menu te openen

cs Čeština
Zapnuto
Automatické opravy
Zvuk
⚠︎ Udělte přístup: Zpřístupnění, Sledování vstupu
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
Moje slova…
AURA Switcher — moje slova
AURA Switcher považuje tato slova za správná: nikdy je nemění a opraví je, když jsou napsána ve špatném rozložení. Oddělte je čárkami.
Uložit
Zrušit
Jazyk nabídky
Všechny jazyky
Evropa
Kavkaz a Střední Asie
Blízký východ
Asie
Systémový
Dvojitý ⇧ — převést slovo/výběr
Nahlásit problém…
Co se stalo a v jaké aplikaci:
Zkontrolovat aktualizace…
⬆︎ Možná je k dispozici aktualizace — zkontrolovat
Podpořit AURA Switcher ☕
Ukončit
Vyšel AURA Switcher {v}
Máte nejnovější verzi
Co je nového
Aktualizovat
Později
Automatická aktualizace se nezdařila. Otevřít web a stáhnout?
AURA Switcher běží. Ikona je v oznamovací oblasti; menu otevřete kliknutím na ^ vedle hodin

sv Svenska
Aktiverad
Autokorrigering
Ljud
⚠︎ Ge åtkomst: Hjälpmedel, Indataövervakning (Input Monitoring)
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
Mina ord…
AURA Switcher — mina ord
AURA Switcher ser dessa ord som rätt: ändrar dem aldrig och rättar dem om de skrivs med fel layout. Separera med kommatecken.
Spara
Avbryt
Menyspråk
Alla språk
Europa
Kaukasus och Centralasien
Mellanöstern
Asien
System
Dubbel-⇧ — konvertera ord/markering
Rapportera ett problem…
Vad hände och i vilken app:
Sök efter uppdateringar…
⬆︎ Det kan finnas en uppdatering — kontrollera
Stöd AURA Switcher ☕
Avsluta
AURA Switcher {v} är här
Du har den senaste versionen
Nyheter
Uppdatera
Senare
Kunde inte uppdatera automatiskt. Öppna webbplatsen för att ladda ner?
AURA Switcher körs. Ikonen finns i aviseringsfältet; klicka på ^ bredvid klockan för att öppna menyn

fi Suomi
Käytössä
Automaattinen korjaus
Ääni
⚠︎ Anna käyttöoikeus: Käyttöapu, Syötteen valvonta (Input Monitoring)
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
Omat sanat…
AURA Switcher — omat sanat
AURA Switcher pitää näitä sanoja oikeina: ei koskaan muuta niitä ja korjaa ne, jos ne on kirjoitettu väärällä asettelulla. Erota pilkuilla.
Tallenna
Kumoa
Valikon kieli
Kaikki kielet
Eurooppa
Kaukasia ja Keski-Aasia
Lähi-itä
Aasia
Järjestelmä
Kaksois-⇧ — muunna sana/valinta
Ilmoita ongelmasta…
Mitä tapahtui ja missä ohjelmassa:
Tarkista päivitykset…
⬆︎ Päivitys saattaa olla saatavilla — tarkista
Tue AURA Switcheriä ☕
Lopeta
AURA Switcher {v} on julkaistu
Sinulla on uusin versio
Uutta
Päivitä
Myöhemmin
Automaattinen päivitys epäonnistui. Avataanko sivusto latausta varten?
AURA Switcher on käynnissä. Kuvake on ilmoitusalueella; avaa valikko napsauttamalla kellon vieressä olevaa ^-merkkiä

ro Română
Activat
Corectare automată
Sunet
⚠︎ Acordați acces: Accesibilitate, Monitorizare intrare (Input Monitoring)
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
Cuvintele mele…
AURA Switcher — cuvintele mele
AURA Switcher consideră aceste cuvinte corecte: nu le schimbă niciodată și le corectează dacă sunt tastate în aranjamentul greșit. Separați-le prin virgulă.
Salvează
Anulează
Limba meniului
Toate limbile
Europa
Caucaz și Asia Centrală
Orientul Mijlociu
Asia
Sistem
Dublu ⇧ — convertește cuvântul/selecția
Raportează o problemă…
Ce s-a întâmplat și în ce aplicație:
Caută actualizări…
⬆︎ Poate există o actualizare — verifică
Susține AURA Switcher ☕
Ieșire
A apărut AURA Switcher {v}
Ai cea mai nouă versiune
Ce e nou
Actualizează
Mai târziu
Actualizarea automată a eșuat. Deschizi site-ul pentru descărcare?
AURA Switcher rulează. Pictograma este în zona de notificare; faceți clic pe ^ de lângă ceas pentru a deschide meniul

hu Magyar
Bekapcsolva
Automatikus javítás
Hang
⚠︎ Hozzáférés megadása: Kisegítő lehetőségek, Bevitelfigyelés (Input Monitoring)
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
Saját szavak…
AURA Switcher — saját szavak
Az AURA Switcher ezeket a szavakat helyesnek tekinti: soha nem módosítja őket, és kijavítja, ha rossz kiosztással gépelték. Vesszővel válassza el.
Mentés
Mégse
Menü nyelve
Minden nyelv
Európa
Kaukázus és Közép-Ázsia
Közel-Kelet
Ázsia
Rendszer
Dupla ⇧ — szó/kijelölés átalakítása
Probléma bejelentése…
Mi történt és melyik alkalmazásban:
Frissítések keresése…
⬆︎ Lehet, hogy van frissítés — ellenőrzés
Az AURA Switcher támogatása ☕
Kilépés
Megjelent az AURA Switcher {v}
A legújabb verzió van nálad
Újdonságok
Frissítés
Később
Az automatikus frissítés nem sikerült. Megnyitod a weboldalt a letöltéshez?
Az AURA Switcher fut. Az ikonja az értesítési területen van; a menü megnyitásához kattintson az óra melletti ^ jelre

el Ελληνικά
Ενεργό
Αυτόματη διόρθωση
Ήχος
⚠︎ Δώστε πρόσβαση: Προσβασιμότητα, Παρακολούθηση εισόδου (Input Monitoring)
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
Οι λέξεις μου…
AURA Switcher — οι λέξεις μου
Το AURA Switcher θεωρεί σωστές αυτές τις λέξεις: δεν τις αλλάζει ποτέ και τις διορθώνει όταν πληκτρολογηθούν με λάθος διάταξη. Χωρίστε τις με κόμμα.
Αποθήκευση
Ακύρωση
Γλώσσα μενού
Όλες οι γλώσσες
Ευρώπη
Καύκασος και Κεντρική Ασία
Μέση Ανατολή
Ασία
Συστήματος
Διπλό ⇧ — μετατροπή λέξης/επιλογής
Αναφορά προβλήματος…
Τι συνέβη και σε ποια εφαρμογή:
Έλεγχος για ενημερώσεις…
⬆︎ Ίσως υπάρχει ενημέρωση — έλεγχος
Υποστηρίξτε το AURA Switcher ☕
Έξοδος
Κυκλοφόρησε το AURA Switcher {v}
Έχετε την τελευταία έκδοση
Τι νέο υπάρχει
Ενημέρωση
Αργότερα
Η αυτόματη ενημέρωση απέτυχε. Άνοιγμα του ιστότοπου για λήψη;
Το AURA Switcher εκτελείται. Το εικονίδιο βρίσκεται στην περιοχή ειδοποιήσεων· κάντε κλικ στο ^ δίπλα στο ρολόι για να ανοίξετε το μενού

bg Български
Включено
Автокорекция
Звук
⚠︎ Дайте достъп: Достъпност, Input Monitoring
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
Моите думи…
AURA Switcher — моите думи
AURA Switcher смята тези думи за правилни: никога не ги променя и ги поправя, ако са написани с грешната подредба. Разделете ги със запетая.
Запази
Отказ
Език на менюто
Всички езици
Европа
Кавказ и Централна Азия
Близкия изток
Азия
Системен
Двоен ⇧ — смяна на езика на дума/селекция
Съобщете за проблем…
Какво се случи и в коя програма:
Проверка за актуализации…
⬆︎ Може да има актуализация — проверете
Подкрепете AURA Switcher ☕
Изход
Излезе AURA Switcher {v}
Имате най-новата версия
Какво ново
Обнови
По-късно
Автоматичното обновяване не успя. Да се отвори ли сайтът за изтегляне?
AURA Switcher работи. Иконата е в областта за известия; щракнете върху ^ до часовника, за да отворите менюто

he עברית
מופעל
תיקון אוטומטי
צליל
⚠︎ תנו גישה: נגישות, ניטור קלט (Input Monitoring)
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
המילים שלי…
AURA Switcher — המילים שלי
AURA Switcher מתייחס למילים אלה כנכונות: לעולם לא משנה אותן, ומתקן אליהן כשהן מוקלדות בפריסה הלא נכונה. הפרידו בפסיקים.
שמירה
ביטול
שפת התפריט
כל השפות
אירופה
הקווקז ומרכז אסיה
המזרח התיכון
אסיה
מערכת
⇧ כפול — המרת מילה/בחירה
דיווח על בעיה…
מה קרה ובאיזה יישום:
בדיקת עדכונים…
⬆︎ ייתכן שיש עדכון — בדיקה
תמכו ב-AURA Switcher ☕
יציאה
יצאה AURA Switcher {v}
יש לכם את הגרסה העדכנית
מה חדש
עדכן
מאוחר יותר
העדכון האוטומטי נכשל. לפתוח את האתר להורדה?
AURA Switcher פועל. הסמל נמצא באזור ההתראות; לחצו על ^ ליד השעון כדי לפתוח את התפריט

ar العربية
مفعّل
التصحيح التلقائي
الصوت
⚠︎ امنح الوصول: تسهيلات الاستخدام، مراقبة الإدخال (Input Monitoring)
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
كلماتي…
AURA Switcher — كلماتي
يعدّ AURA Switcher هذه الكلمات صحيحة: لا يغيّرها أبدًا، ويصحّحها إليها إذا كُتبت بالتخطيط الخطأ. افصل بينها بفواصل.
حفظ
إلغاء
لغة القائمة
كل اللغات
أوروبا
القوقاز وآسيا الوسطى
الشرق الأوسط
آسيا
النظام
⇧ مزدوج — تحويل الكلمة/التحديد
الإبلاغ عن مشكلة…
ماذا حدث وفي أي تطبيق:
التحقق من التحديثات…
⬆︎ قد يتوفر تحديث — تحقّق
ادعم AURA Switcher ☕
إنهاء
صدر AURA Switcher {v}
لديك أحدث إصدار
ما الجديد
تحديث
لاحقًا
تعذّر التحديث تلقائيًا. هل تريد فتح الموقع للتنزيل؟
AURA Switcher يعمل. الأيقونة في منطقة الإشعارات؛ انقر على ^ بجانب الساعة لفتح القائمة

hi हिन्दी
चालू
स्वतः सुधार
ध्वनि
⚠︎ अनुमति दें: सुलभता, इनपुट मॉनिटरिंग (Input Monitoring)
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
मेरे शब्द…
AURA Switcher — मेरे शब्द
AURA Switcher इन शब्दों को सही मानता है: इन्हें कभी नहीं बदलता, और गलत लेआउट में टाइप होने पर इन्हीं में सुधार देता है। अल्पविराम से अलग करें।
सहेजें
रद्द करें
मेनू की भाषा
सभी भाषाएँ
यूरोप
काकेशस और मध्य एशिया
मध्य पूर्व
एशिया
सिस्टम
दो बार ⇧ — शब्द/चयन बदलें
समस्या बताएँ…
क्या हुआ और किस ऐप में:
अपडेट जाँचें…
⬆︎ शायद नया अपडेट आया है — जाँचें
AURA Switcher का समर्थन करें ☕
बंद करें
AURA Switcher {v} आ गया है
आपके पास नवीनतम संस्करण है
नया क्या है
अपडेट करें
बाद में
अपने-आप अपडेट नहीं हो सका। डाउनलोड के लिए वेबसाइट खोलें?
AURA Switcher चल रहा है। इसका आइकन नोटिफ़िकेशन एरिया में है; मेनू खोलने के लिए घड़ी के पास ^ पर क्लिक करें

ko 한국어
켜기
자동 수정
소리
⚠︎ 접근 권한 허용: 손쉬운 사용, 입력 모니터링
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
내 단어…
AURA Switcher — 내 단어
AURA Switcher는 이 단어들을 올바른 단어로 봅니다. 절대 바꾸지 않고, 잘못된 자판으로 입력되면 이 단어로 고칩니다. 쉼표로 구분하세요.
저장
취소
메뉴 언어
모든 언어
유럽
캅카스·중앙아시아
중동
아시아
시스템
⇧ 두 번 — 단어/선택 영역 변환
문제 신고…
무슨 일이 어느 앱에서 있었나요:
업데이트 확인…
⬆︎ 업데이트가 있을 수 있습니다 — 확인
AURA Switcher 후원하기 ☕
종료
AURA Switcher {v} 출시
최신 버전을 사용 중입니다
새로운 기능
업데이트
나중에
자동으로 업데이트하지 못했습니다. 다운로드하려면 웹사이트를 열까요?
AURA Switcher가 실행 중입니다. 아이콘은 알림 영역에 있습니다. 시계 옆의 ^를 눌러 메뉴를 여세요

be Беларуская
Уключана
Аўтавыпраўленне
Гук
⚠︎ Дайце доступ: Спецыяльныя магчымасці, Input Monitoring
Праграма:
Не працаваць у гэтай праграме
Гульня — AURA на паўзе
Мова для гэтай праграмы
Без прывязкі
Выключэнні
Не працаваць у (нядаўнія праграмы)
Прывязкі моў
Пуста
Запускаць пры ўваходзе
Мае словы…
AURA Switcher — мае словы
AURA Switcher лічыць гэтыя словы правільнымі: не мяняе іх, а набраныя ў няправільнай раскладцы — выпраўляе на іх. Праз коску.
Захаваць
Скасаваць
Мова меню
Усе мовы
Еўропа
Каўказ і Цэнтральная Азія
Блізкі Усход
Азія
Сістэмная
Падвойны ⇧ — змяніць мову слова/вылучэння
Паведаміць пра праблему…
Што здарылася і ў якой праграме:
Праверыць абнаўленні…
⬆︎ Магчыма, выйшла абнаўленне — праверыць
Падтрымаць AURA Switcher ☕
Выхад
Выйшла AURA Switcher {v}
У вас апошняя версія
Што новага
Абнавіць
Пазней
Не ўдалося абнавіць аўтаматычна. Адкрыць сайт, каб спампаваць?
AURA Switcher працуе. Значок — у вобласці апавяшчэнняў; націсніце ^ побач з гадзіннікам, каб адкрыць меню

kk Қазақша
Қосулы
Автотүзету
Дыбыс
⚠︎ Рұқсат беріңіз: Арнайы мүмкіндіктер, Input Monitoring
Қолданба:
Бұл қолданбада жұмыс істемеу
Ойын — AURA кідіртілді
Осы қолданбаның тілі
Байланыссыз
Ерекшеліктер
Мында жұмыс істемеу (соңғы қолданбалар)
Тіл байланыстары
Бос
Кіргенде іске қосу
Менің сөздерім…
AURA Switcher — менің сөздерім
AURA Switcher бұл сөздерді дұрыс деп санайды: оларды өзгертпейді, ал қате пернетақта орналасуымен терілсе, осыларға түзетеді. Үтірмен бөліңіз.
Сақтау
Бас тарту
Мәзір тілі
Барлық тілдер
Еуропа
Кавказ және Орталық Азия
Таяу Шығыс
Азия
Жүйелік
Қос ⇧ — сөзді/таңдалғанды түрлендіру
Мәселе туралы хабарлау…
Не болды және қай қолданбада:
Жаңартуларды тексеру…
⬆︎ Жаңарту шыққан болуы мүмкін — тексеру
AURA Switcher жобасын қолдау ☕
Шығу
AURA Switcher {v} шықты
Сізде соңғы нұсқа бар
Не жаңалық
Жаңарту
Кейінірек
Автоматты түрде жаңарту мүмкін болмады. Жүктеп алу үшін сайтты ашу керек пе?
AURA Switcher жұмыс істеп тұр. Белгішесі хабарландыру аймағында; мәзірді ашу үшін сағаттың жанындағы ^ түймесін басыңыз

ky Кыргызча
Күйгүзүлгөн
Авто оңдоо
Үн
⚠︎ Мүмкүнчүлүк бериңиз: Атайын мүмкүнчүлүктөр, Input Monitoring
Колдонмо:
Бул колдонмодо иштебөө
Оюн — AURA тыныгууда
Бул колдонмонун тили
Байланышсыз
Өзгөчөлүктөр
Төмөндө иштебөө (акыркы колдонмолор)
Тил байланыштары
Бош
Кирүүдө иштетүү
Менин сөздөрүм…
AURA Switcher — менин сөздөрүм
AURA Switcher бул сөздөрдү туура деп эсептейт: аларды өзгөртпөйт, ал эми клавиатуранын туура эмес жайгашуусунда терилсе, ушуларга оңдойт. Үтүр менен бөлүңүз.
Сактоо
Жокко чыгаруу
Меню тили
Бардык тилдер
Европа
Кавказ жана Борбордук Азия
Жакынкы Чыгыш
Азия
Системалык
Кош ⇧ — сөздү/тандалганды өзгөртүү
Көйгөй тууралуу кабарлоо…
Эмне болду жана кайсы колдонмодо:
Жаңыртууларды текшерүү…
⬆︎ Жаңыртуу чыгып калышы мүмкүн — текшерүү
AURA Switcher долбоорун колдоо ☕
Чыгуу
AURA Switcher {v} чыкты
Сизде акыркы версия бар
Эмне жаңы
Жаңыртуу
Кийинчерээк
Автоматтык түрдө жаңыртуу мүмкүн болгон жок. Жүктөп алуу үчүн сайтты ачайынбы?
AURA Switcher иштеп жатат. Сөлөкөтү билдирмелер аймагында; менюну ачуу үчүн сааттын жанындагы ^ баскычын басыңыз

ka ქართული
ჩართულია
ავტოშესწორება
ხმა
⚠︎ მიეცით წვდომა: მისაწვდომობა, Input Monitoring
აპლიკაცია:
ამ აპლიკაციაში არ იმუშაოს
თამაში — AURA დაპაუზებულია
ამ აპლიკაციის ენა
მიბმის გარეშე
გამონაკლისები
არ იმუშაოს (ბოლო აპლიკაციები)
ენების მიბმა
ცარიელია
გაშვება შესვლისას
ჩემი სიტყვები…
AURA Switcher — ჩემი სიტყვები
AURA Switcher ამ სიტყვებს სწორად მიიჩნევს: არ ცვლის მათ, ხოლო არასწორი განლაგებით აკრეფილს ამათ ასწორებს. გამოყავით მძიმეებით.
შენახვა
გაუქმება
მენიუს ენა
ყველა ენა
ევროპა
კავკასია და ცენტრალური აზია
ახლო აღმოსავლეთი
აზია
სისტემური
ორმაგი ⇧ — სიტყვის/მონიშნულის გადაყვანა
პრობლემის შეტყობინება…
რა მოხდა და რომელ აპლიკაციაში:
განახლებების შემოწმება…
⬆︎ შესაძლოა განახლება გამოვიდა — შეამოწმეთ
AURA Switcher-ის მხარდაჭერა ☕
გასვლა
გამოვიდა AURA Switcher {v}
გაქვთ უახლესი ვერსია
რა არის ახალი
განახლება
მოგვიანებით
ავტომატურად განახლება ვერ მოხერხდა. გავხსნა საიტი ჩამოსატვირთად?
AURA Switcher მუშაობს. ხატულა შეტყობინებების არეშია; მენიუს გასახსნელად დააწკაპუნეთ საათის გვერდით ^-ზე

hy Հայերեն
Միացված է
Ինքնաուղղում
Ձայն
⚠︎ Տվեք թույլտվություն՝ Հասանելիություն, Input Monitoring
Հավելված՝
Չաշխատել այս հավելվածում
Խաղ — AURA-ն դադարի վրա է
Այս հավելվածի լեզուն
Առանց կապակցման
Բացառություններ
Չաշխատել (վերջին հավելվածներ)
Լեզուների կապակցումներ
Դատարկ է
Գործարկել մուտքի ժամանակ
Իմ բառերը…
AURA Switcher — իմ բառերը
AURA Switcher-ն այս բառերը ճիշտ է համարում՝ չի փոխում դրանք, իսկ սխալ դասավորությամբ տպվածները ուղղում է դրանց։ Բաժանեք ստորակետերով։
Պահպանել
Չեղարկել
Ցանկի լեզուն
Բոլոր լեզուները
Եվրոպա
Կովկաս և Կենտրոնական Ասիա
Մերձավոր Արևելք
Ասիա
Համակարգային
Կրկնակի ⇧ — փոխարկել բառը/նշվածը
Հաղորդել խնդրի մասին…
Ինչ է պատահել և որ հավելվածում՝
Ստուգել թարմացումները…
⬆︎ Հնարավոր է՝ թարմացում է թողարկվել — ստուգել
Աջակցել AURA Switcher-ին ☕
Ելք
Թողարկվել է AURA Switcher {v}
Դուք ունեք վերջին տարբերակը
Ինչ նոր է
Թարմացնել
Հետո
Չհաջողվեց թարմացնել ավտոմատ կերպով։ Բացե՞լ կայքը՝ ներբեռնելու համար։
AURA Switcher-ը աշխատում է։ Պատկերակը ծանուցումների տարածքում է․ մենյուն բացելու համար սեղմեք ժամացույցի կողքի ^-ը

az Azərbaycan
Aktivdir
Avtomatik düzəliş
Səs
⚠︎ İcazə verin: Əlçatımlılıq, Input Monitoring
Tətbiq:
Bu tətbiqdə işləmə
Oyun — AURA fasilədədir
Bu tətbiq üçün dil
Bağlama yoxdur
İstisnalar
Burada işləmə (son tətbiqlər)
Dil bağlamaları
Boşdur
Girişdə başlat
Mənim sözlərim…
AURA Switcher — mənim sözlərim
AURA Switcher bu sözləri düzgün sayır: onları dəyişmir, səhv düzülüşdə yığılanda isə bunlara düzəldir. Vergüllə ayırın.
Saxla
Ləğv et
Menyu dili
Bütün dillər
Avropa
Qafqaz və Mərkəzi Asiya
Yaxın Şərq
Asiya
Sistem
Qoşa ⇧ — sözü/seçilmişi çevir
Problem barədə bildir…
Nə baş verdi və hansı tətbiqdə:
Yeniləmələri yoxla…
⬆︎ Yeniləmə çıxmış ola bilər — yoxlayın
AURA Switcher-i dəstəkləyin ☕
Çıxış
AURA Switcher {v} çıxdı
Ən son versiya sizdədir
Nə yenidir
Yenilə
Sonra
Avtomatik yeniləmək alınmadı. Yükləmək üçün sayt açılsın?
AURA Switcher işləyir. Nişan bildiriş sahəsindədir; menyunu açmaq üçün saatın yanındakı ^ işarəsinə klikləyin

sr Српски
Укључено
Аутокорекција
Звук
⚠︎ Дозволите приступ: Приступачност, Input Monitoring
Апликација:
Не радити у овој апликацији
Игра — AURA паузирана
Језик за ову апликацију
Без повезивања
Изузеци
Не радити у (недавне апликације)
Повезивање језика
Празно
Покрени при пријави
Моје речи…
AURA Switcher — моје речи
AURA Switcher сматра ове речи исправним: не мења их, а када су откуцане у погрешном распореду — исправља их на ове. Одвојите зарезима.
Сачувај
Откажи
Језик менија
Сви језици
Европа
Кавказ и Централна Азија
Блиски исток
Азија
Системски
Двоструки ⇧ — претвори реч/избор
Пријави проблем…
Шта се догодило и у којој апликацији:
Провери ажурирања…
⬆︎ Можда је изашло ажурирање — провери
Подржи AURA Switcher ☕
Изађи
Изашао је AURA Switcher {v}
Имате најновију верзију
Шта је ново
Ажурирај
Касније
Аутоматско ажурирање није успело. Отворити сајт за преузимање?
AURA Switcher ради. Икона је у пољу за обавештења; кликните на ^ поред сата да бисте отворили мени

mk Македонски
Вклучено
Автокорекција
Звук
⚠︎ Дозволете пристап: Пристапност, Input Monitoring
Апликација:
Не работи во оваа апликација
Игра — AURA паузирана
Јазик за оваа апликација
Без врзување
Исклучоци
Не работи во (неодамнешни апликации)
Врзувања на јазици
Празно
Стартувај при најава
Моите зборови…
AURA Switcher — моите зборови
AURA Switcher ги смета овие зборови за точни: не ги менува, а кога се напишани во погрешен распоред — ги исправа на нив. Одвојте ги со запирки.
Зачувај
Откажи
Јазик на менито
Сите јазици
Европа
Кавказ и Централна Азија
Блиски Исток
Азија
Системски
Двоен ⇧ — претвори збор/избор
Пријави проблем…
Што се случи и во која апликација:
Провери за ажурирања…
⬆︎ Можеби има ново ажурирање — провери
Поддржи го AURA Switcher ☕
Излези
Излезе AURA Switcher {v}
Ја имате најновата верзија
Што е ново
Ажурирај
Подоцна
Автоматското ажурирање не успеа. Да го отворам сајтот за преземање?
AURA Switcher работи. Иконата е во областа за известувања; кликнете на ^ покрај часовникот за да го отворите менито

sk Slovenčina
Zapnuté
Automatické opravy
Zvuk
⚠︎ Povoľte prístup: Zjednodušenie ovládania, Monitorovanie vstupu (Input Monitoring)
Aplikácia:
V tejto aplikácii nepracovať
Hra — AURA pozastavená
Jazyk pre túto aplikáciu
Bez priradenia
Výnimky
Nepracovať v (nedávne aplikácie)
Priradenia jazykov
Prázdne
Spúšťať po prihlásení
Moje slová…
AURA Switcher — moje slová
AURA Switcher považuje tieto slová za správne: nikdy ich nemení a opravuje ich, keď sú napísané v nesprávnom rozložení. Oddeľte ich čiarkami.
Uložiť
Zrušiť
Jazyk ponuky
Všetky jazyky
Európa
Kaukaz a Stredná Ázia
Blízky východ
Ázia
Systémový
Dvojitý ⇧ — previesť slovo/výber
Nahlásiť problém…
Čo sa stalo a v ktorej aplikácii:
Skontrolovať aktualizácie…
⬆︎ Možno vyšla aktualizácia — skontrolovať
Podporiť AURA Switcher ☕
Ukončiť
Vyšiel AURA Switcher {v}
Máte najnovšiu verziu
Čo je nové
Aktualizovať
Neskôr
Automatická aktualizácia sa nepodarila. Otvoriť web na stiahnutie?
AURA Switcher beží. Ikona je v oblasti oznámení; ponuku otvoríte kliknutím na ^ vedľa hodín

hr Hrvatski
Uključeno
Automatski ispravak
Zvuk
⚠︎ Dopustite pristup: Pristupačnost, Praćenje unosa (Input Monitoring)
Aplikacija:
Ne radi u ovoj aplikaciji
Igra — AURA pauzirana
Jezik za ovu aplikaciju
Bez povezivanja
Iznimke
Ne radi u (nedavne aplikacije)
Povezivanja jezika
Prazno
Pokreni pri prijavi
Moje riječi…
AURA Switcher — moje riječi
AURA Switcher ove riječi smatra ispravnima: nikad ih ne mijenja, a ispravlja ih kad su utipkane u pogrešnom rasporedu. Odvojite ih zarezima.
Spremi
Odustani
Jezik izbornika
Svi jezici
Europa
Kavkaz i Srednja Azija
Bliski istok
Azija
Sustavski
Dvostruki ⇧ — pretvori riječ/odabir
Prijavi problem…
Što se dogodilo i u kojoj aplikaciji:
Provjeri ažuriranja…
⬆︎ Možda je izašlo ažuriranje — provjeri
Podrži AURA Switcher ☕
Izađi
Izašao je AURA Switcher {v}
Imate najnoviju verziju
Što je novo
Ažuriraj
Kasnije
Automatsko ažuriranje nije uspjelo. Otvoriti web-stranicu za preuzimanje?
AURA Switcher radi. Ikona je u području obavijesti; kliknite ^ pored sata da otvorite izbornik

sl Slovenščina
Vklopljeno
Samodejni popravki
Zvok
⚠︎ Dovolite dostop: Dostopnost, Input Monitoring
Aplikacija:
V tej aplikaciji ne delajte
Igra — AURA zaustavljena
Jezik za to aplikacijo
Brez povezave
Izjeme
Ne delajte v (nedavne aplikacije)
Povezave jezikov
Prazno
Zaženi ob prijavi
Moje besede…
AURA Switcher — moje besede
AURA Switcher te besede šteje za pravilne: nikoli jih ne spremeni, popravi pa jih, če so natipkane v napačni razporeditvi. Ločite jih z vejicami.
Shrani
Prekliči
Jezik menija
Vsi jeziki
Evropa
Kavkaz in Srednja Azija
Bližnji vzhod
Azija
Sistemski
Dvojni ⇧ — pretvori besedo/izbor
Prijavi težavo…
Kaj se je zgodilo in v kateri aplikaciji:
Preverite posodobitve…
⬆︎ Morda je izšla posodobitev — preverite
Podpri AURA Switcher ☕
Končaj
Izšel je AURA Switcher {v}
Imate najnovejšo različico
Kaj je novega
Posodobi
Pozneje
Samodejna posodobitev ni uspela. Odprem spletno mesto za prenos?
AURA Switcher deluje. Ikona je v območju za obvestila; kliknite ^ ob uri, da odprete meni

lt Lietuvių
Įjungta
Automatinis taisymas
Garsas
⚠︎ Suteikite prieigą: Pritaikymas neįgaliesiems, Input Monitoring
Programa:
Šioje programoje nedirbti
Žaidimas — AURA pristabdyta
Kalba šiai programai
Be priskyrimo
Išimtys
Nedirbti (naujausios programos)
Kalbų priskyrimai
Tuščia
Paleisti prisijungiant
Mano žodžiai…
AURA Switcher — mano žodžiai
AURA Switcher laiko šiuos žodžius teisingais: niekada jų nekeičia, o surinktus netinkamu išdėstymu — pataiso į juos. Atskirkite kableliais.
Išsaugoti
Atšaukti
Meniu kalba
Visos kalbos
Europa
Kaukazas ir Vidurio Azija
Artimieji Rytai
Azija
Sistemos
Dvigubas ⇧ — konvertuoti žodį/pažymėtą tekstą
Pranešti apie problemą…
Kas nutiko ir kurioje programoje:
Tikrinti naujinimus…
⬆︎ Galbūt išleistas naujinimas — patikrinkite
Paremti AURA Switcher ☕
Išeiti
Išleistas AURA Switcher {v}
Turite naujausią versiją
Kas naujo
Atnaujinti
Vėliau
Nepavyko atnaujinti automatiškai. Atidaryti svetainę atsisiuntimui?
AURA Switcher veikia. Piktograma yra pranešimų srityje; spustelėkite ^ šalia laikrodžio, kad atidarytumėte meniu

lv Latviešu
Ieslēgts
Automātiskā labošana
Skaņa
⚠︎ Atļaujiet piekļuvi: Pieejamība, Input Monitoring
Lietotne:
Šajā lietotnē nedarboties
Spēle — AURA apturēta
Valoda šai lietotnei
Bez piesaistes
Izņēmumi
Nedarboties (nesenās lietotnes)
Valodu piesaistes
Tukšs
Palaist pieteikšanās brīdī
Mani vārdi…
AURA Switcher — mani vārdi
AURA Switcher uzskata šos vārdus par pareiziem: nekad tos nemaina un labo uz tiem, ja tie ievadīti nepareizā izkārtojumā. Atdaliet ar komatiem.
Saglabāt
Atcelt
Izvēlnes valoda
Visas valodas
Eiropa
Kaukāzs un Centrālāzija
Tuvie Austrumi
Āzija
Sistēmas
Dubults ⇧ — pārvērst vārdu/atlasi
Ziņot par problēmu…
Kas notika un kurā lietotnē:
Meklēt atjauninājumus…
⬆︎ Iespējams, iznācis atjauninājums — pārbaudīt
Atbalstīt AURA Switcher ☕
Iziet
Iznācis AURA Switcher {v}
Jums ir jaunākā versija
Kas jauns
Atjaunināt
Vēlāk
Automātiski atjaunināt neizdevās. Atvērt vietni lejupielādei?
AURA Switcher darbojas. Ikona ir paziņojumu apgabalā; noklikšķiniet uz ^ blakus pulkstenim, lai atvērtu izvēlni

et Eesti
Sees
Automaatne parandus
Heli
⚠︎ Andke juurdepääs: Juurdepääsetavus, Input Monitoring
Rakendus:
Selles rakenduses mitte töötada
Mäng — AURA peatatud
Selle rakenduse keel
Seostamata
Erandid
Mitte töötada (hiljutised rakendused)
Keeleseosed
Tühi
Käivita sisselogimisel
Minu sõnad…
AURA Switcher — minu sõnad
AURA Switcher peab neid sõnu õigeks: ei muuda neid kunagi ja parandab nendeks, kui need on sisestatud vales paigutuses. Eralda komadega.
Salvesta
Tühista
Menüü keel
Kõik keeled
Euroopa
Kaukaasia ja Kesk-Aasia
Lähis-Ida
Aasia
Süsteemne
Topelt ⇧ — teisenda sõna/valik
Teata probleemist…
Mis juhtus ja millises rakenduses:
Otsi uuendusi…
⬆︎ Uuendus võib olla saadaval — kontrolli
Toeta AURA Switcherit ☕
Välju
AURA Switcher {v} on väljas
Sul on uusim versioon
Mis on uut
Uuenda
Hiljem
Automaatne uuendamine ebaõnnestus. Avada veebileht allalaadimiseks?
AURA Switcher töötab. Ikoon on teavitusalas; menüü avamiseks klõpsake kella kõrval ^

da Dansk
Aktiveret
Automatisk rettelse
Lyd
⚠︎ Giv adgang: Tilgængelighed, Inputovervågning (Input Monitoring)
App:
Deaktiver i denne app
Spil — AURA på pause
Sprog for denne app
Ingen tilknytning
Undtagelser
Deaktiver i (seneste apps)
Sprogtilknytninger
Tom
Start ved login
Mine ord…
AURA Switcher — mine ord
AURA Switcher regner disse ord for korrekte: ændrer dem aldrig og retter til dem, når de er skrevet i forkert tastaturlayout. Adskil med komma.
Gem
Annuller
Menusprog
Alle sprog
Europa
Kaukasus og Centralasien
Mellemøsten
Asien
System
Dobbelt ⇧ — konvertér ord/markering
Rapportér et problem…
Hvad skete der, og i hvilken app:
Søg efter opdateringer…
⬆︎ Der kan være en opdatering — tjek
Støt AURA Switcher ☕
Afslut
AURA Switcher {v} er udkommet
Du har den nyeste version
Nyheder
Opdater
Senere
Kunne ikke opdatere automatisk. Åbn hjemmesiden for at hente den?
AURA Switcher kører. Ikonet er i meddelelsesområdet; klik på ^ ved siden af uret for at åbne menuen

nb Norsk bokmål
På
Autokorrigering
Lyd
⚠︎ Gi tilgang: Tilgjengelighet, Inndataovervåking (Input Monitoring)
App:
Ikke aktiv i denne appen
Spill — AURA er satt på pause
Språk for denne appen
Ingen kobling
Unntak
Ikke aktiv i (nylige apper)
Språkkoblinger
Tom
Start ved innlogging
Mine ord…
AURA Switcher — mine ord
AURA Switcher regner disse ordene som riktige: endrer dem aldri og retter til dem når de er skrevet med feil tastaturoppsett. Skill med komma.
Lagre
Avbryt
Menyspråk
Alle språk
Europa
Kaukasus og Sentral-Asia
Midtøsten
Asia
System
Dobbel ⇧ — konverter ord/markering
Rapporter et problem…
Hva skjedde, og i hvilken app:
Se etter oppdateringer…
⬆︎ En oppdatering kan være tilgjengelig — sjekk
Støtt AURA Switcher ☕
Avslutt
AURA Switcher {v} er ute
Du har den nyeste versjonen
Nytt
Oppdater
Senere
Kunne ikke oppdatere automatisk. Åpne nettstedet for å laste den ned?
AURA Switcher kjører. Ikonet er i varslingsfeltet; klikk på ^ ved siden av klokken for å åpne menyen

is Íslenska
Kveikt
Sjálfvirk leiðrétting
Hljóð
⚠︎ Veittu aðgang: Algildur aðgangur, Input Monitoring
Forrit:
Ekki virkt í þessu forriti
Leikur — AURA í bið
Tungumál fyrir þetta forrit
Engin tenging
Undantekningar
Ekki virkt í (nýleg forrit)
Tungumálatengingar
Tómt
Ræsa við innskráningu
Mín orð…
AURA Switcher — mín orð
AURA Switcher telur þessi orð rétt: breytir þeim aldrei og lagfærir í þau þegar þau eru rituð með röngu lyklaborðsskipulagi. Aðskildu með kommum.
Vista
Hætta við
Tungumál valmyndar
Öll tungumál
Evrópa
Kákasus og Mið-Asía
Mið-Austurlönd
Asía
Kerfið
Tvöfalt ⇧ — umbreyta orði/vali
Tilkynna vandamál…
Hvað gerðist og í hvaða forriti:
Athuga með uppfærslur…
⬆︎ Uppfærsla gæti verið tiltæk — athuga
Styðja AURA Switcher ☕
Hætta
AURA Switcher {v} er komið út
Þú ert með nýjustu útgáfuna
Hvað er nýtt
Uppfæra
Síðar
Ekki tókst að uppfæra sjálfkrafa. Opna vefsíðuna til að sækja hana?
AURA Switcher er í gangi. Táknið er á tilkynningasvæðinu; smelltu á ^ við hliðina á klukkunni til að opna valmyndina

sq Shqip
Aktivizuar
Korrigjim automatik
Zëri
⚠︎ Jepni qasje: Përdorimi i lehtë, Input Monitoring
Aplikacioni:
Mos vepro në këtë aplikacion
Lojë — AURA në pauzë
Gjuha për këtë aplikacion
Pa lidhje
Përjashtime
Mos vepro në (aplikacionet e fundit)
Lidhjet e gjuhëve
Bosh
Hap në hyrje
Fjalët e mia…
AURA Switcher — fjalët e mia
AURA Switcher i konsideron këto fjalë të sakta: nuk i ndryshon kurrë dhe i korrigjon në to kur shkruhen me paraqitje të gabuar të tastierës. Ndajini me presje.
Ruaj
Anulo
Gjuha e menysë
Të gjitha gjuhët
Evropa
Kaukazi dhe Azia Qendrore
Lindja e Mesme
Azia
Sistemi
Dyfishi ⇧ — konverto fjalën/përzgjedhjen
Raporto një problem…
Çfarë ndodhi dhe në cilin aplikacion:
Kontrollo për përditësime…
⬆︎ Mund të ketë një përditësim — kontrollo
Mbështet AURA Switcher ☕
Mbyll
AURA Switcher {v} doli
Keni versionin më të ri
Çfarë ka të re
Përditëso
Më vonë
Nuk u përditësua automatikisht. Të hapet faqja e internetit për ta shkarkuar?
AURA Switcher po funksionon. Ikona është në zonën e njoftimeve; kliko ^ pranë orës për të hapur menynë

fa فارسی
روشن
تصحیح خودکار
صدا
⚠︎ دسترسی بدهید: دسترسی‌پذیری، Input Monitoring
برنامه:
در این برنامه کار نکن
بازی — AURA متوقف است
زبان این برنامه
بدون اتصال
استثناها
در این‌ها کار نکن (برنامه‌های اخیر)
اتصال زبان‌ها
خالی
اجرا هنگام ورود
واژه‌های من…
AURA Switcher — واژه‌های من
AURA Switcher این واژه‌ها را درست می‌داند: هرگز تغییرشان نمی‌دهد و اگر با چیدمان اشتباه تایپ شوند، به آن‌ها اصلاحشان می‌کند. با ویرگول جدا کنید.
ذخیره
لغو
زبان منو
همه زبان‌ها
اروپا
قفقاز و آسیای مرکزی
خاورمیانه
آسیا
سیستم
دوبار ⇧ — تبدیل واژه/انتخاب
گزارش مشکل…
چه اتفاقی افتاد و در کدام برنامه:
بررسی به‌روزرسانی…
⬆︎ ممکن است به‌روزرسانی موجود باشد — بررسی کنید
حمایت از AURA Switcher ☕
خروج
AURA Switcher {v} منتشر شد
جدیدترین نسخه را دارید
چه چیز تازه است
به‌روزرسانی
بعداً
به‌روزرسانی خودکار انجام نشد. وب‌سایت برای دانلود باز شود؟
AURA Switcher در حال اجراست. نماد آن در ناحیه اعلان‌هاست؛ برای باز کردن منو روی ^ کنار ساعت کلیک کنید

vi Tiếng Việt
Bật
Tự động sửa
Âm thanh
⚠︎ Cấp quyền: Trợ năng, Giám sát đầu vào (Input Monitoring)
Ứng dụng:
Không hoạt động trong ứng dụng này
Trò chơi — AURA tạm dừng
Ngôn ngữ cho ứng dụng này
Chưa gán
Ngoại lệ
Không hoạt động trong (ứng dụng gần đây)
Gán ngôn ngữ
Trống
Chạy khi đăng nhập
Từ của tôi…
AURA Switcher — từ của tôi
AURA Switcher coi những từ này là đúng: không bao giờ đổi chúng và sửa thành chúng khi gõ sai bố cục bàn phím. Cách nhau bằng dấu phẩy.
Lưu
Hủy
Ngôn ngữ menu
Tất cả ngôn ngữ
Châu Âu
Kavkaz và Trung Á
Trung Đông
Châu Á
Hệ thống
Nhấn đôi ⇧ — chuyển đổi từ/vùng chọn
Báo cáo sự cố…
Chuyện gì đã xảy ra và trong ứng dụng nào:
Kiểm tra cập nhật…
⬆︎ Có thể có bản cập nhật — kiểm tra
Ủng hộ AURA Switcher ☕
Thoát
AURA Switcher {v} đã ra mắt
Bạn đang dùng phiên bản mới nhất
Có gì mới
Cập nhật
Để sau
Không thể tự động cập nhật. Mở trang web để tải về?
AURA Switcher đang chạy. Biểu tượng nằm ở khu vực thông báo; bấm ^ cạnh đồng hồ để mở menu

tg Тоҷикӣ
Фаъол
Ислоҳи худкор
Овоз
⚠︎ Дастрасӣ диҳед: Дастрасии универсалӣ, Input Monitoring
Барнома:
Дар ин барнома кор накунед
Бозӣ — AURA таваққуф шудааст
Забон барои ин барнома
Бе пайвастшавӣ
Истисноҳо
Кор накунед дар (барномаҳои охирин)
Пайвасткунии забонҳо
Холӣ
Оғоз ҳангоми воридшавӣ
Калимаҳои ман…
AURA Switcher — калимаҳои ман
AURA Switcher ин калимаҳоро дуруст мешуморад: онҳоро ҳаргиз тағйир намедиҳад ва агар бо тарҳбандии нодурусти клавиатура навишта шаванд, ба онҳо ислоҳ мекунад. Бо вергул ҷудо кунед.
Захира кардан
Бекор кардан
Забони меню
Ҳамаи забонҳо
Аврупо
Қафқоз ва Осиёи Марказӣ
Шарқи Наздик
Осиё
Система
Дукарата ⇧ — табдили калима/интихоб
Хабар додан дар бораи мушкилот…
Чӣ шуд ва дар кадом барнома:
Санҷиши навсозиҳо…
⬆︎ Шояд навсозӣ мавҷуд бошад — санҷед
Дастгирии AURA Switcher ☕
Баромадан
AURA Switcher {v} баромад
Шумо навтарин версияро доред
Чӣ нав аст
Навсозӣ
Баъдтар
Навсозии худкор муяссар нашуд. Сайтро барои боргирӣ кушоед?
AURA Switcher кор карда истодааст. Нишона дар минтақаи огоҳиҳост; барои кушодани меню ^-ро дар назди соат пахш кунед

mn Монгол
Идэвхтэй
Автомат засвар
Дуу
⚠︎ Хандалт өгнө үү: Хүртээмж, Input Monitoring
Апп:
Энэ аппад ажиллахгүй
Тоглоом — AURA түр зогссон
Энэ аппын хэл
Холбоогүй
Үл хамаарах зүйлс
Ажиллахгүй (сүүлийн аппууд)
Хэлний холболтууд
Хоосон
Нэвтрэхэд эхлүүлэх
Миний үгс…
AURA Switcher — миний үгс
AURA Switcher эдгээр үгсийг зөв гэж үздэг: хэзээ ч өөрчлөхгүй, буруу гарын байрлалаар бичигдсэн бол тэдгээр рүү засна. Таслалаар тусгаарлана уу.
Хадгалах
Цуцлах
Цэсний хэл
Бүх хэл
Европ
Кавказ ба Төв Ази
Ойрхи Дорнод
Ази
Систем
Давхар ⇧ — үг/сонголтыг хөрвүүлэх
Асуудлын талаар мэдээлэх…
Юу болсон, ямар аппад:
Шинэчлэлт шалгах…
⬆︎ Шинэчлэлт байж магадгүй — шалгах
AURA Switcher-ийг дэмжих ☕
Гарах
AURA Switcher {v} гарлаа
Танд хамгийн сүүлийн хувилбар байна
Шинэ зүйл
Шинэчлэх
Дараа
Автоматаар шинэчилж чадсангүй. Татаж авахын тулд вэбсайтыг нээх үү?
AURA Switcher ажиллаж байна. Дүрс тэмдэг мэдэгдлийн хэсэгт байна; цэсийг нээхийн тулд цагийн хажууох ^-г дарна уу

tt Татарча
Кабызылган
Автоматик төзәтү
Тавыш
⚠︎ Кереп җитү рөхсәт итегез: Махсус мөмкинлекләр, Input Monitoring
Кушымта:
Бу кушымтада эшләмәскә
Уен — AURA туктатылган
Бу кушымта өчен тел
Бәйләнешсез
Аерымлыклар
Эшләмәскә (соңгы кушымталар)
Телләр бәйләнеше
Буш
Керүдә эшләтеп җибәрергә
Минем сүзләр…
AURA Switcher — минем сүзләр
AURA Switcher бу сүзләрне дөрес дип саный: аларны беркайчан да үзгәртми, ә дөрес булмаган клавиатура рәвешендә язылса, алар итеп төзәтә. Өтер белән аерыгыз.
Саклау
Баш тарту
Меню теле
Барлык телләр
Европа
Кавказ һәм Үзәк Азия
Якын Көнчыгыш
Азия
Система
Икеләтә ⇧ — сүзне/сайланганны әйләндерү
Проблема турында хәбәр итү…
Нәрсә булды һәм кайсы кушымтада:
Яңартуларны тикшерү…
⬆︎ Яңарту булырга мөмкин — тикшерегез
AURA Switcher'ны хупларга ☕
Чыгу
AURA Switcher {v} чыкты
Сездә иң соңгы версия
Нәрсә яңа
Яңарту
Соңрак
Автоматик рәвештә яңартып булмады. Йөкләү өчен сайтны ачарга?
AURA Switcher эшли. Билгечә белдерүләр өлкәсендә; менюны ачу өчен сәгать янындагы ^ төймәсенә басыгыз

uz Oʻzbek
Yoqilgan
Avtomatik tuzatish
Tovush
⚠︎ Ruxsat bering: Maxsus imkoniyatlar, Input Monitoring
Ilova:
Bu ilovada ishlamasin
Oʻyin — AURA pauzada
Bu ilova uchun til
Bogʻlanmagan
Istisnolar
Ishlamasin (soʻnggi ilovalar)
Til bogʻlanishlari
Boʻsh
Kirishda ishga tushirish
Mening soʻzlarim…
AURA Switcher — mening soʻzlarim
AURA Switcher bu soʻzlarni toʻgʻri deb hisoblaydi: ularni hech qachon oʻzgartirmaydi, notoʻgʻri klaviatura terilishida yozilsa, ularga tuzatadi. Vergul bilan ajrating.
Saqlash
Bekor qilish
Menyu tili
Barcha tillar
Yevropa
Kavkaz va Markaziy Osiyo
Yaqin Sharq
Osiyo
Tizim
Ikki marta ⇧ — soʻz/belgilashni aylantirish
Muammo haqida xabar berish…
Nima boʻldi va qaysi ilovada:
Yangilanishlarni tekshirish…
⬆︎ Yangilanish boʻlishi mumkin — tekshiring
AURA Switcher’ni qoʻllab-quvvatlash ☕
Chiqish
AURA Switcher {v} chiqdi
Sizda eng soʻnggi versiya bor
Yangiliklar
Yangilash
Keyinroq
Avtomatik yangilab boʻlmadi. Yuklab olish uchun saytni ochaymi?
AURA Switcher ishlayapti. Belgisi bildirishnomalar hududida; menyuni ochish uchun soat yonidagi ^ belgisini bosing

ga Gaeilge
Cumasaithe
Ceartú uathoibríoch
Fuaim
⚠︎ Tabhair rochtain: Inrochtaineacht, Input Monitoring
Aip:
Ná oibrigh san aip seo
Cluiche — AURA ar sos
Teanga don aip seo
Gan nasc
Eisceachtaí
Ná oibrigh i (aipeanna le déanaí)
Naisc teanga
Folamh
Seol ag logáil isteach
Mo chuid focal…
AURA Switcher — mo chuid focal
Measann AURA Switcher go bhfuil na focail seo ceart: ní athraíonn sé iad choíche, agus ceartaíonn sé chucu iad nuair a chlóscríobhtar le leagan amach mícheart iad. Deighil le camóga.
Sábháil
Cealaigh
Teanga an roghchláir
Gach teanga
An Eoraip
An Chugais agus an Áise Láir
An Mheánoirthear
An Áise
Córas
⇧ faoi dhó — tiontaigh focal/roghnúchán
Tuairiscigh fadhb…
Cad a tharla agus in cén aip:
Seiceáil le haghaidh nuashonruithe…
⬆︎ B’fhéidir go bhfuil nuashonrú ar fáil — seiceáil
Tacaigh le AURA Switcher ☕
Scoir
Tá AURA Switcher {v} amuigh
Tá an leagan is déanaí agat
Cad atá nua
Nuashonraigh
Níos déanaí
Níorbh fhéidir nuashonrú go huathoibríoch. An bhfuil fonn ort an suíomh gréasáin a oscailt chun é a íoslódáil?
Tá AURA Switcher ag rith. Tá an deilbhín sa limistéar fógraí; cliceáil ^ cois an chloig chun an roghchlár a oscailt

mt Malti
Attivat
Korrezzjoni awtomatika
Ħoss
⚠︎ Agħti aċċess: Aċċessibbiltà, Input Monitoring
Applikazzjoni:
Taħdimx f’din l-applikazzjoni
Logħba — AURA fil-pawża
Lingwa għal din l-applikazzjoni
L-ebda rabta
Eċċezzjonijiet
Taħdimx fi (applikazzjonijiet reċenti)
Rabtiet tal-lingwi
Vojt
Ibda mad-dħul
Kliemi…
AURA Switcher — kliemi
AURA Switcher iqis dan il-kliem bħala korrett: ma jbiddlu qatt, u jikkoreġi għalih meta jinkiteb bi tqassim żbaljat tat-tastiera. Aqsamhom b’virgola.
Salva
Ikkanċella
Lingwa tal-menu
Il-lingwi kollha
Ewropa
Il-Kawkasu u l-Asja Ċentrali
Il-Lvant Nofsani
L-Asja
Sistema
⇧ doppju — ikkonverti kelma/għażla
Irrapporta problema…
X’ġara u f’liema applikazzjoni:
Iċċekkja għal aġġornamenti…
⬆︎ Jista’ jkun hemm aġġornament — iċċekkja
Appoġġa lil AURA Switcher ☕
Ieqaf
AURA Switcher {v} ħareġ
Għandek l-aħħar verżjoni
X’hemm ġdid
Aġġorna
Aktar tard
Ma setax jiġi aġġornat awtomatikament. Niftaħ is-sit biex tniżżlu?
AURA Switcher qed jaħdem. L-ikona tinsab fiż-żona tan-notifiki; ikklikkja fuq ^ ħdejn l-arloġġ biex tiftaħ il-menu

cy Cymraeg
Wedi’i alluogi
Cywiro awtomatig
Sain
⚠︎ Rhowch fynediad: Hygyrchedd, Input Monitoring
Ap:
Peidio â gweithio yn yr ap hwn
Gêm — AURA wedi’i oedi
Iaith ar gyfer yr ap hwn
Dim cysylltiad
Eithriadau
Peidio â gweithio yn (apiau diweddar)
Cysylltiadau iaith
Gwag
Cychwyn wrth fewngofnodi
Fy ngeiriau…
AURA Switcher — fy ngeiriau
Mae AURA Switcher yn ystyried bod y geiriau hyn yn gywir: nid yw byth yn eu newid, ac mae’n cywiro iddyn nhw pan fyddan nhw wedi’u teipio gyda’r cynllun bysellfwrdd anghywir. Gwahanwch â choma.
Cadw
Diddymu
Iaith y ddewislen
Pob iaith
Ewrop
Y Cawcasws a Chanolbarth Asia
Y Dwyrain Canol
Asia
System
⇧ dwbl — trosi gair/dewisiad
Adrodd am broblem…
Beth ddigwyddodd ac ym mha ap:
Gwirio am ddiweddariadau…
⬆︎ Efallai bod diweddariad ar gael — gwiriwch
Cefnogi AURA Switcher ☕
Gadael
Mae AURA Switcher {v} allan
Mae gennych y fersiwn ddiweddaraf
Beth sy’n newydd
Diweddaru
Yn nes ymlaen
Methu diweddaru’n awtomatig. Agor y wefan i’w lawrlwytho?
Mae AURA Switcher yn rhedeg. Mae’r eicon yn yr ardal hysbysu; cliciwch ^ wrth ymyl y cloc i agor y ddewislen
"""

# New in 0.11: code → string. Merged into the language blocks below (appended keys; "app" is overridden).
NEW = {
    "app": {
        "en": "Current app:",
        "uk": "Поточна програма:",
        "pl": "Bieżąca aplikacja:",
        "de": "Aktuelle App:",
        "es": "App actual:",
        "fr": "App actuelle :",
        "it": "App attuale:",
        "pt": "App atual:",
        "tr": "Geçerli uygulama:",
        "nl": "Huidige app:",
        "cs": "Aktuální aplikace:",
        "sv": "Aktuell app:",
        "fi": "Nykyinen ohjelma:",
        "ro": "Aplicația curentă:",
        "hu": "Jelenlegi alkalmazás:",
        "el": "Τρέχουσα εφαρμογή:",
        "bg": "Текуща програма:",
        "he": "יישום נוכחי:",
        "ar": "التطبيق الحالي:",
        "hi": "वर्तमान ऐप:",
        "ko": "현재 앱:",
        "be": "Бягучая праграма:",
        "kk": "Ағымдағы қолданба:",
        "ky": "Учурдагы колдонмо:",
        "ka": "მიმდინარე აპლიკაცია:",
        "hy": "Ընթացիկ հավելված՝",
        "az": "Cari tətbiq:",
        "sr": "Тренутна апликација:",
        "mk": "Тековна апликација:",
        "sk": "Aktuálna aplikácia:",
        "hr": "Trenutna aplikacija:",
        "sl": "Trenutna aplikacija:",
        "lt": "Dabartinė programa:",
        "lv": "Pašreizējā lietotne:",
        "et": "Praegune rakendus:",
        "da": "Nuværende app:",
        "nb": "Gjeldende app:",
        "is": "Núverandi forrit:",
        "sq": "Aplikacioni aktual:",
        "fa": "برنامه فعلی:",
        "vi": "Ứng dụng hiện tại:",
        "tg": "Барномаи ҷорӣ:",
        "mn": "Одоогийн апп:",
        "tt": "Хәзерге кушымта:",
        "uz": "Joriy ilova:",
        "ga": "Aip reatha:",
        "mt": "Applikazzjoni attwali:",
        "cy": "Ap cyfredol:",
    },
    "hotkey": {
        "en": "Correction key",
        "uk": "Клавіша виправлення",
        "pl": "Klawisz poprawiania",
        "de": "Korrekturtaste",
        "es": "Tecla de corrección",
        "fr": "Touche de correction",
        "it": "Tasto di correzione",
        "pt": "Tecla de correção",
        "tr": "Düzeltme tuşu",
        "nl": "Correctietoets",
        "cs": "Klávesa opravy",
        "sv": "Korrigeringstangent",
        "fi": "Korjausnäppäin",
        "ro": "Tasta de corectare",
        "hu": "Javítás gombja",
        "el": "Πλήκτρο διόρθωσης",
        "bg": "Клавиш за поправка",
        "he": "מקש תיקון",
        "ar": "مفتاح التصحيح",
        "hi": "सुधार कुंजी",
        "ko": "수정 키",
        "be": "Клавіша выпраўлення",
        "kk": "Түзету пернесі",
        "ky": "Оңдоо баскычы",
        "ka": "შესწორების კლავიში",
        "hy": "Ուղղման ստեղն",
        "az": "Düzəliş düyməsi",
        "sr": "Тастер исправке",
        "mk": "Копче за исправка",
        "sk": "Kláves opravy",
        "hr": "Tipka ispravka",
        "sl": "Tipka popravka",
        "lt": "Taisymo klavišas",
        "lv": "Labošanas taustiņš",
        "et": "Parandusklahv",
        "da": "Rettetast",
        "nb": "Rettetast",
        "is": "Leiðréttingartakki",
        "sq": "Tasti i korrigjimit",
        "fa": "کلید اصلاح",
        "vi": "Phím sửa",
        "tg": "Тугмаи ислоҳ",
        "mn": "Засах товч",
        "tt": "Төзәтү төймәсе",
        "uz": "Tuzatish tugmasi",
        "ga": "Eochair cheartúcháin",
        "mt": "Ċavetta tal-korrezzjoni",
        "cy": "Allwedd cywiro",
    },
    "off": {
        "en": "Off",
        "uk": "Вимкнено",
        "pl": "Wyłączone",
        "de": "Aus",
        "es": "Desactivado",
        "fr": "Désactivé",
        "it": "Disattivato",
        "pt": "Desativado",
        "tr": "Kapalı",
        "nl": "Uit",
        "cs": "Vypnuto",
        "sv": "Av",
        "fi": "Pois",
        "ro": "Dezactivat",
        "hu": "Ki",
        "el": "Ανενεργό",
        "bg": "Изключено",
        "he": "כבוי",
        "ar": "متوقف",
        "hi": "बंद",
        "ko": "끔",
        "be": "Выключана",
        "kk": "Өшірулі",
        "ky": "Өчүк",
        "ka": "გამორთული",
        "hy": "Անջատված",
        "az": "Söndürülüb",
        "sr": "Искључено",
        "mk": "Исклучено",
        "sk": "Vypnuté",
        "hr": "Isključeno",
        "sl": "Izklopljeno",
        "lt": "Išjungta",
        "lv": "Izslēgts",
        "et": "Väljas",
        "da": "Fra",
        "nb": "Av",
        "is": "Slökkt",
        "sq": "Joaktiv",
        "fa": "خاموش",
        "vi": "Tắt",
        "tg": "Хомӯш",
        "mn": "Унтраасан",
        "tt": "Сүндерелгән",
        "uz": "Oʻchiq",
        "ga": "As",
        "mt": "Mitfi",
        "cy": "I ffwrdd",
    },
    "settings": {
        "en": "Settings",
        "uk": "Налаштування",
        "pl": "Ustawienia",
        "de": "Einstellungen",
        "es": "Ajustes",
        "fr": "Réglages",
        "it": "Impostazioni",
        "pt": "Definições",
        "tr": "Ayarlar",
        "nl": "Instellingen",
        "cs": "Nastavení",
        "sv": "Inställningar",
        "fi": "Asetukset",
        "ro": "Setări",
        "hu": "Beállítások",
        "el": "Ρυθμίσεις",
        "bg": "Настройки",
        "he": "הגדרות",
        "ar": "الإعدادات",
        "hi": "सेटिंग्स",
        "ko": "설정",
        "be": "Налады",
        "kk": "Параметрлер",
        "ky": "Жөндөөлөр",
        "ka": "პარამეტრები",
        "hy": "Կարգավորումներ",
        "az": "Ayarlar",
        "sr": "Подешавања",
        "mk": "Поставки",
        "sk": "Nastavenia",
        "hr": "Postavke",
        "sl": "Nastavitve",
        "lt": "Nustatymai",
        "lv": "Iestatījumi",
        "et": "Seaded",
        "da": "Indstillinger",
        "nb": "Innstillinger",
        "is": "Stillingar",
        "sq": "Cilësimet",
        "fa": "تنظیمات",
        "vi": "Cài đặt",
        "tg": "Танзимот",
        "mn": "Тохиргоо",
        "tt": "Көйләүләр",
        "uz": "Sozlamalar",
        "ga": "Socruithe",
        "mt": "Settings",
        "cy": "Gosodiadau",
    },
}

langs = []  # (code, name, [strings])
for block in LANGS.strip().split("\n\n"):
    head, *lines = block.split("\n")
    code, name = head.split(" ", 1)
    assert len(lines) == len(KEYS) - 3, (code, len(lines))
    lines += [NEW[k][code] for k in ("hotkey", "off", "settings")]
    lines[KEYS.index("app")] = NEW["app"][code] + " "  # "App: " + app name
    langs.append((code, name, lines))

SWIFT_REGIONS = """/// Menu-language picker: a few likely languages on top, the rest under "All languages" by region.
/// Regions in menu order (title key → codes); a language not listed here goes to the first region.
let uiRegions: [(String, [String])] = [("regEurope", []), ("regCaucasus", ["kk", "ky", "ka", "hy", "az", "tg", "mn", "uz"]), ("regMiddleEast", ["he", "ar", "fa", "ku"]), ("regAsia", ["hi", "ko", "zh", "ja", "vi"])]
func uiRegionIndex(_ code: String) -> Int { uiRegions.firstIndex { $0.1.contains(code) } ?? 0 }
/// the languages shown on top of the picker: the current one, English, then the languages of the user's keyboard layouts
func uiTopLangs(current: String, layoutLangs: [String]) -> [String] {
    var top: [String] = []
    for c in [current, "en"] + layoutLangs where uiLangs.contains(c) && !top.contains(c) && top.count < 6 { top.append(c) }
    return top
}
"""

def esc(s): return s.replace("\\", "\\\\").replace('"', '\\"')

def put(path, begin, end, body):
    p = ROOT / path; s = p.read_text()
    s2, n = re.subn(re.escape(begin) + r".*?" + re.escape(end), lambda _: begin + "\n" + body + end, s, flags=re.S)
    assert n == 1, path
    p.write_text(s2)

B, E = "// BEGIN menu strings — generated by tools/menu_i18n.py, edit there", "// END menu strings"
swift = "let uiLangs = [" + ", ".join(f'"{c}"' for c, _, _ in langs) + "]\n"
swift += "let uiLangNames = [" + ", ".join(f'"{n}"' for _, n, _ in langs) + "]\n"
swift += SWIFT_REGIONS
swift += "let uiStrings: [String: [String]] = [ // key: one string per uiLangs\n"
for i, k in enumerate(KEYS):
    swift += f'    "{k}": [' + ", ".join(f'"{esc(l[i])}"' for _, _, l in langs) + "],\n"
swift += "]\n"
put("engine.swift", B, E, swift)

cpp = f"static const int UI_N = {len(langs)};\n"
cpp += "static const wchar_t* const UI_LANGS[UI_N] = {" + ", ".join(f'L"{c}"' for c, _, _ in langs) + "};\n"
cpp += "static const wchar_t* const UI_NAMES[UI_N] = {" + ", ".join(f'L"{n}"' for _, n, _ in langs) + "};\n"
cpp += "static const wchar_t* const UI_STR[][UI_N] = { // row: English first, then the other UI_LANGS\n"
for i, k in enumerate(KEYS):
    cpp += "    {" + ", ".join(f'L"{esc(l[i])}"' for _, _, l in langs) + "},\n"
cpp += "};\n"
put("windows/aura.cpp", B, E, cpp)
print(f"{len(langs)} languages × {len(KEYS)} strings")
