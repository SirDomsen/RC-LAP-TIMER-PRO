# RC Lap Timer Pro – Offline-Statistik

## Zweck und aktueller Stand

Die Offline-Statistik ist eine lokale, vollbildfähige Auswertung für Lap-Timer-Statistikdaten. Sie läuft im Browser, benötigt keine Verbindung zum ESP8266 und speichert die importierte Datenbank lokal im Browser.

- Einstieg: `offline_stats/index.html`
- Implementierung des Offline-Datenadapters, ZIP-Import/-Export und IndexedDB: `offline_stats/offline_stats.js`
- Gemeinsame Statistikdarstellung: `data/stats.js`
- Aktuelle Offline-App-Version: **0.8.0**
- Speicherung: IndexedDB `rc-lap-timer-offline-stats`

Die Statistikansicht ist offline schreibgeschützt. Geräteaktionen wie Session-Löschen oder Flash-Reparaturen werden nicht als lokale Änderungen ausgeführt.

## Import, Zusammenführung und Export

Unterstützte Eingaben:

- LittleFS-ZIP-Backups des Lap Timers
- Statistik-JSON-Backups der Versionen 3 und 4

Beim ZIP-Import werden nur Statistikdaten übernommen: `tracks.json`, `sessions.json`, `names.json`, `daily_best.json` sowie JSON-Sessiondateien in den Strecken-/Modus-Unterordnern. Frontend-Dateien, WLAN-Konfigurationen und sonstige Konfigurationsdateien werden ausgelassen. Unsichere Pfade mit Traversal-Segmenten werden verworfen.

Die Daten werden nach relativem Dateipfad in IndexedDB gespeichert und mit dem vorhandenen Bestand zusammengeführt:

- Eine Statistikdatei am bereits gespeicherten Pfad wird aktualisiert.
- Neue Sessiondateien werden ergänzt.
- Sessions werden anhand ihres Dateipfads (ersatzweise ihrer ID) dedupliziert.
- Streckenrekorde und Fahrernamen werden zusammengeführt.

Über **„Stats-ZIP herunterladen“** lässt sich die kumulierte Datenbank als ZIP mit denselben relativen Pfaden sichern. Dieses ZIP kann später wieder importiert werden, um die Datenbank auf einem anderen Browser/Gerät fortzuführen. Der Export enthält ebenfalls nur Statistikdateien, keine Frontend- oder WLAN-Dateien.

Die Datenbank bleibt zunächst im Browserprofil gespeichert. Browserdaten zu löschen oder ein anderes Browserprofil zu verwenden entfernt bzw. trennt diese lokale Datenbank; daher sollte das heruntergeladene Stats-ZIP als portables Backup aufbewahrt werden.

## Gemeinsame Statistikfunktion pflegen

Die Offline-App verwendet für Darstellung und Statistikansichten dieselbe `data/stats.js` wie das Geräte-Frontend. Änderungen an gemeinsam nutzbarer Berechnungs- oder Darstellungslogik sollen daher zuerst dort umgesetzt werden, nicht als Kopie in `offline_stats/`.

Der Offline-Adapter in `offline_stats/offline_stats.js` stellt die lokale Datenquelle bereit, insbesondere Fahrer-/Streckendaten, Sessions und Sessiondateien. Er ersetzt dafür die entsprechenden Ladevorgänge, sodass die gemeinsame Statistikansicht im Offline-Modus aus dem importierten Bestand statt aus Geräte-APIs liest.

Vorgehen bei einer Statistikänderung:

1. Vorhandene Implementierung und Aufrufer in `data/stats.js` prüfen.
2. Gemeinsame Berechnung oder Darstellung dort ändern, wenn sie online und offline gelten soll.
3. Prüfen, ob die Änderung neue Daten, Felder oder Endpunkte voraussetzt. Falls ja, den Offline-Adapter und gegebenenfalls Backup-/Importformat passend erweitern; Geräte-API-Aufrufe nicht unbesehen in den Offline-Pfad übernehmen.
4. Schreibende Geräteaktionen offline weiterhin deaktivieren oder ausdrücklich durch eine sichere lokale Entsprechung ersetzen.
5. Cache-Versionen der betroffenen Geräte-Frontend-Assets nach den Projektregeln aktualisieren. Die Offline-Seite lädt dieselben Statistikdateien und muss bei geänderten Asset-Referenzen ebenfalls aktuell bleiben.
6. Mit einem Geräte-/Frontend-Durchlauf und einem importierten Offline-Datensatz prüfen, dass beide Ansichten korrekt funktionieren.

So bleibt die Statistiklogik möglichst gemeinsam und konsistent. Eine Aufteilung in getrennte Online- und Offline-Statistikimplementierungen ist nicht der Standard; sie wäre nur sinnvoll, falls klar abgegrenzte Datenadapter nötig werden.

## Versionierung

Die Offline-App hat eine eigene Versionsnummer, unabhängig von Firmware und Geräte-Frontend:

- Version/Kopfzeile und Offline-Asset-Cache-Parameter: `offline_stats/index.html`
- Versionswert für Statusmeldungen: `OFFLINE_STATS_VERSION` in `offline_stats/offline_stats.js`

Diese Angaben müssen bei einer Offline-App-Version gemeinsam angepasst werden. Eine reine Firmware- oder Geräte-Frontend-Änderung hebt die Offline-App-Version nicht automatisch an. Ändert eine gemeinsame Statistikfunktion sichtbar das Offline-Verhalten, wird separat entschieden, ob dies zugleich eine Offline-App-Versionserhöhung rechtfertigt.

Für Offline-spezifische Änderungen werden keine Firmware- oder Geräte-Frontend-Versionen angehoben. Änderungen an gemeinsam ausgelieferten `data/*.js`- oder `data/*.css`-Dateien folgen weiterhin den Cache-Busting-Regeln des Geräte-Frontends.

## Prüfung vor Änderungen/Release

- ZIP-Import und JSON-Backup-Import funktionieren.
- Wiederholter Import aktualisiert gleiche Pfade und erzeugt keine doppelten Sessions.
- Export enthält nur Statistikdateien und bewahrt Pfade sowie UTF-8-Dateinamen.
- Ein exportiertes Stats-ZIP kann erneut importiert werden.
- Die gemeinsame Statistikansicht funktioniert mit Gerätedaten und mit importierten Offline-Daten.
- Offline-spezifische Aktionen bleiben lokal bzw. schreibgeschützt; keine unerwarteten Geräte-API-Aufrufe.
- Cache- und App-Versionen stimmen mit dem geänderten Umfang überein.
