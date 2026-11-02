# RC Lap Timer Pro – Bedienungsanleitung

Diese Anleitung erklärt die wichtigsten Funktionen in einfachen Schritten. Der Lap Timer wird über eine Webseite im Browser bedient; eine zusätzliche App ist nicht nötig.

## 1. Was der Lap Timer macht

Ein kleiner Sender am RC-Auto – der **Transponder** – meldet jede Durchfahrt an der Start-/Ziellinie. Der Lap Timer zählt die Runden und zeigt die Zeiten live auf Handy, Tablet oder Computer an.

Zusätzliche **Sektorstationen** können Zwischenzeiten auf der Strecke erfassen. Ohne diese Stationen misst das System weiterhin Runden an der Start-/Ziellinie; Zwischenzeiten werden dann nicht angezeigt.

Die Bedienung hat zwei Ansichten:

- **Fahreransicht:** Live-Zeiten ansehen, Ansagen für das eigene Handy einstellen und Statistiken öffnen.
- **Rennleiteransicht:** Strecke und Fahrer einrichten sowie Trainings und Rennen starten und beenden.

## 2. Mit dem Lap Timer verbinden

### Direkt mit dem Lap Timer-WLAN verbinden

Wenn der Lap Timer kein Heim-WLAN oder keinen Handy-Hotspot nutzt:

1. Öffne auf deinem Handy die WLAN-Auswahl.
2. Verbinde dich mit dem WLAN des Lap Timers. Der Standardname beginnt mit `RC-LapTimer-`; dahinter stehen meist vier Zeichen. Ein eigener WLAN-Name kann eingestellt worden sein.
3. Das Standardpasswort lautet `Rennstrecke123`, sofern es nicht geändert wurde.
4. Falls das Handy meldet, dass dieses WLAN kein Internet hat, bleibe trotzdem damit verbunden.
5. Öffne den Browser und rufe `http://192.168.0.8` auf. Wurde die IP-Adresse zuvor geändert oder vom Lap Timer gemerkt, kann eine andere Adresse gelten.

### Über Heim-WLAN oder Handy-Hotspot verbinden

Wenn der Lap Timer bereits mit einem Heim-WLAN oder Hotspot verbunden ist, müssen Handy und Lap Timer im selben Netzwerk sein. Öffne dann die vom Betreiber mitgeteilte Adresse.

Bei einer Heim-WLAN-Verbindung kann vorübergehend ein zusätzliches WLAN mit einem Namen wie `LT_WLANNAME-IPADRESSE` erscheinen. Die IP-Adresse am Ende des Namens ist die Adresse des Lap Timers. Dieses Hilfs-WLAN verschwindet nach dem ersten Aufruf der Webseite wieder.

### WLAN-Verbindung ändern

Wenn du bereits mit dem Lap Timer verbunden bist:

1. Öffne in der Rennleiteransicht **Einstellungen**.
2. Trage unter **WLAN Kontrollzentrum** den Namen und das Passwort deines Heim-WLANs oder Handy-Hotspots ein.
3. Tippe auf **Zu Heim-WLAN** oder **Zu Hotspot** und bestätige den Wechsel im nächsten Fenster.
4. Die bisherige Verbindung wird getrennt. Verbinde dein Handy mit demselben WLAN wie den Lap Timer. Bei einer Heim-WLAN-Verbindung findest du die IP-Adresse am Ende des vorübergehenden WLAN-Namens `LT_WLANNAME-IPADRESSE`; öffne diese Adresse im Browser.

Mit **Zu AP-Modus** kehrst du zum direkten WLAN des Lap Timers zurück. Name, Passwort und Adresse werden vor dem Wechsel angezeigt. Der Wechsel unterbricht die aktuelle Verbindung; die Einstellungen bleiben gespeichert.

### Fahrer und Rennleiter öffnen

- Die normale Adresse öffnet die **Fahreransicht**.
- Tippe oben auf den Rollen-Hinweis und folge der Bestätigung, um zur **Rennleiteransicht** zu wechseln.
- In der Rennleiteransicht führt der Rollen-Hinweis zurück zur Fahreransicht.
- Der Rennleiter kann über **QR-Code** einen Code anzeigen lassen, mit dem weitere Handys die Fahreransicht öffnen.

## 3. Einmalig Fahrer, Autos und Strecke einrichten

### Transponder zuordnen

Jeder Transponder hat eine eigene ID. Damit Zeiten beim richtigen Fahrer landen:

1. Öffne als Rennleiter **Einstellungen**.
2. Suche die Zeile mit der ID des Transponders.
3. Trage Fahrername und Auto ein.
4. Wiederhole das für die weiteren Autos und wähle **Speichern & Schließen**.

Die Zuordnung muss zur ID des Senders im Auto passen. Eine ID sollte nicht gleichzeitig für zwei Autos verwendet werden. Es können bis zu 15 IDs eingerichtet werden.

### Strecke auswählen oder anlegen

In der Rennleiteransicht steht die aktuelle Strecke über der Tabelle.

- Wähle eine vorhandene Strecke aus der Liste.
- Für eine weitere Strecke wähle **Neue Strecke**, gib einen Namen ein und bestätige.
- Die Zeiten und Statistiken werden der jeweils ausgewählten Strecke zugeordnet.
- Das Papierkorb-Symbol löscht eine Strecke samt ihrer gespeicherten Durchgänge und Statistiken. Das lässt sich nicht rückgängig machen.

### Sektorstationen

In den **Einstellungen** kann der Rennleiter angeben, welche Sektorstationen verwendet werden. SZ1 bis SZ3 sind Zwischenpunkte; die Start-/Ziellinie erfasst die Runde. Nur eingeschaltete und erreichbare Stationen liefern Zwischenzeiten.

## 4. Training oder Rennen starten

Die Rennleitung startet einen Durchgang über die Schaltflächen in der Rennleiteransicht. Ein Start setzt die aktuelle Live-Anzeige für den neuen Durchgang zurück; gespeicherte Statistiken bleiben davon getrennt.

| Auswahl | Was passiert |
|---|---|
| **Freies Training** | Runden werden ohne eingestellte Laufzeit oder Rundenzahl gezählt. |
| **Qualifying** | Stelle die Dauer in Minuten ein und wähle **Start Quali**. Die schnellsten Zeiten zeigen, wer die beste Startposition erreicht hat. |
| **Rennen (Runden)** | Stelle die Rundenzahl ein und wähle **Start Runden**. Vor dem Start läuft ein gemeinsamer Countdown. |
| **Rennen (Zeit)** | Stelle die Dauer in Minuten ein und wähle **Start Zeit**. Vor dem Start läuft ein gemeinsamer Countdown. |
| **Dragrace** | Wähle **Start Dragrace**. Die Zeitmessung beginnt beim Überfahren der Basisstation; weitere Messpunkte hängen von der eingerichteten Strecke ab. |
| **Ghost Battle** | Wähle einen gespeicherten Zeitvergleich als Ziel und starte den Vergleich für die Fahrer. Details dazu stehen unter [Ghosthunt und Ghost Battle](#7-ghosthunt-und-ghost-battle). |

**Durchgang Stoppen** beendet den aktuellen Durchgang und wechselt zurück zum freien Training.

### Komplett-Reset

**Komplett-Reset** löscht die aktuellen Runden und Sektorzeiten. Im Bestätigungsfenster ist außerdem vorausgewählt, dass die Tagesbestzeiten zurückgesetzt werden. Das lässt sich dort abwählen. Der ewige Streckenrekord bleibt erhalten.

Ein Reset ist nicht dasselbe wie das Löschen einer Strecke: Er entfernt nicht automatisch die gespeicherte Rennhistorie. Strecken samt Historie lassen sich über das Papierkorb-Symbol in der Streckenauswahl löschen.

## 5. Live-Anzeige verstehen

Die Tabelle aktualisiert sich während des Fahrens. Je nach Rennart enthält sie unter anderem:

- **Pos:** aktuelle Platzierung
- **Fahrer / Auto:** Name und Fahrzeug, die zur Transponder-ID gehören
- **Runde:** gezählte Runden
- **SZ1–SZ3:** Zwischenzeiten an den eingerichteten Sektorstationen
- **SZ4:** Zeit an der Start-/Ziellinie
- **Letzte Zeit:** zuletzt gemessene Runde
- **Beste Zeit:** beste Runde des Fahrers im aktuellen Durchgang
- **Abstand:** Rückstand oder Zeitabstand zur führenden beziehungsweise besten Zeit

Ein **Sektor** ist ein Abschnitt der Strecke. Eine Zwischenzeit zeigt, wie lange das Auto bis zu diesem Messpunkt gebraucht hat.

Für eine gut lesbare Anzeige auf einem Handy gibt es **Dashboard**. Es zeigt die wichtigsten Live-Daten groß auf dunklem Hintergrund. Über **Zurück** kommst du zur normalen Ansicht.

### Ansagen und Töne

- Mit **Ton: AN/AUS** schaltest du die Sprachausgabe dieses Handys ein oder aus.
- In der Fahreransicht kannst du auswählen, für welches Auto Zeiten angesagt werden. Die Auswahl gilt nur für dieses Handy.
- In den **Einstellungen** des Rennleiters lässt sich auswählen, welche Autos auf diesem Gerät angesagt werden.
- Falls ein iPhone oder iPad zunächst stumm bleibt, tippe auf **iOS Ton**, um die Audioausgabe freizugeben.
- Der Rennleiter kann über **Arcade Audio** zusätzlich Töne und Musik des Lautsprechers an der Basisstation starten oder stoppen.

## 6. Statistiken

Öffne **Statistiken**, um gespeicherte Ergebnisse anzusehen. Die Reiter sortieren die Daten nach Rekorden, Fahrern und Durchgangsart. In einem Fahrerprofil oder Durchgang lassen sich weitere Runden und Zeiten ansehen.

- **Streckenrekord** ist die beste gespeicherte Zeit einer Strecke.
- **Tagesbestzeit** ist die beste Zeit des aktuellen Tages.
- Die Top-5-Ansicht zeigt die vorderen Rekorde und Tagesbestzeiten.

Die Statistiken werden auf dem Lap Timer gespeichert. Im aktuell sichtbaren Statistikfenster gibt es keine Schaltflächen zum Herunterladen oder Wiederherstellen eines Statistik-Backups. Wenn du eine Sicherung benötigst, kläre mit der Person, die das Gerät betreut, welche Sicherungsmöglichkeit für deine Version vorgesehen ist.

Wenn eine Warnung zum Speicherplatz erscheint, lösche keine Strecken oder Statistiken, die du behalten möchtest. Das Löschen einer Strecke entfernt auch ihre gespeicherten Durchgänge.

## 7. Ghosthunt und Ghost Battle

Ein **Geist** ist eine gespeicherte Rundenzeit, mit der deine aktuelle Runde verglichen wird. Es fährt dabei kein zusätzliches Auto.

1. Öffne **Ghosthunt** in der Fahreransicht oder **Ghost Battle** in der Rennleiteransicht.
2. Wähle das Vergleichsziel: zum Beispiel Streckenrekord, heutige Bestzeit, eigene Bestzeit, führendes Auto oder eine selbst eingegebene Zielzeit.
3. Starte den Vergleich. Die Anzeige zeigt deine aktuelle Runde und den Zeitunterschied zum Ziel, auch an den Sektorstationen.

Die Fahreransicht startet einen persönlichen Ghosthunt auf diesem Gerät. Die Rennleitung kann stattdessen einen Ghost Battle für die gesamte Session starten. Während eines gemeinsamen Ghost Battle kann ein Fahrer weiterhin ein eigenes Ziel auswählen; über **Admin-Geist übernehmen** kehrt er zum gemeinsamen Ziel zurück.

## 8. Wenn etwas nicht funktioniert

### Es werden keine Runden angezeigt

- Prüfe, ob das Auto einen eingeschalteten Transponder hat.
- Prüfe, ob die Transponder-ID in den Einstellungen dem richtigen Fahrer zugeordnet ist.
- Kontrolliere, ob die Webseite noch **LIVE** anzeigt und das Handy mit demselben WLAN wie der Lap Timer verbunden ist.
- Ohne Sektorstationen gibt es keine Zwischenzeiten; die Start-/Ziellinie sollte weiterhin Runden erfassen.

### Zwischenzeiten fehlen

Prüfe, ob die betreffende Sektorstation eingeschaltet, erreichbar und in den Einstellungen aktiviert ist. Ohne funktionierende Station wird dieser Zwischenpunkt nicht gemessen.

### Das Handy findet die Webseite nicht

Verbinde das Handy mit demselben WLAN wie den Lap Timer. Im Direkt-WLAN des Lap Timers muss das Handy trotz einer Meldung „Kein Internet“ verbunden bleiben. Nutze den angezeigten QR-Code oder prüfe, ob sich die IP-Adresse geändert hat.

### Es gibt keine Sprachansage

Prüfe **Ton: AN/AUS**, die Auswahl des angesagten Autos und – bei iPhone oder iPad – die Schaltfläche **iOS Ton**. Die Sprachausgabe läuft auf dem jeweiligen Handy und kann dort unabhängig von anderen Geräten ausgeschaltet sein.

---

**Hinweis:** Die Menüpunkte für Firmware-Updates und Debug sind für Wartung und Fehlersuche gedacht und gehören nicht zur normalen Bedienung.
