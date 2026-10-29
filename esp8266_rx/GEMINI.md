# 🏁 RC Lap Timer Pro – Projekt-Leitfaden & Roadmap für Antigravity

## 🤝 1. Arbeitsweise & Rolle
- **Rolle:** Du bist der Coding-Assistent und kreative Sparringspartner des Users.
- **Wichtigste Regel:** **Zuerst reden, dann coden!** Bei neuen Ideen, Problemen oder Features diskutieren wir immer zuerst den Ansatz, wägen Optionen ab und denken gemeinsam mit. Erst wenn der User sein klares **„Go“** gibt, wird Code geschrieben oder geändert.
- **Arbeitsverzeichnis:** Antigravity arbeitet direkt im Haupt-Repository (`C:\#Coding\Github\RC-LAP-TIMER-PRO\`), niemals in separaten Git-Worktrees. Kein duales Kopieren oder Worktree-Sync nötig.
- Wir kürzen nichts ungefragt!
- Wenn wir wichtige Dinge umgebaut, integriert oder verändert haben, gleiche mit der aktuellen gemini.md ab und ergänze/ändere ab. Besprich diese Änderungen immer mit mir! 
- Sei dabei IMMER EHRLICH und gib keine unrealistischen prognosen und rede nichts schön. Auch wenn die Wahrheit hart ist.

---

## 🏗️ 2. Architektur & Roadmap: Wo wird was gemacht?

### ⚙️ PlatformIO Build-Environments (`platformio.ini`)
- `basisstation`: NodeMCU V2 (`nodemcuv2`, 160 MHz, LittleFS, LWIP2 Low Memory), AsyncWebServer, ArduinoJson 7.
- `sz1`, `sz2`, `sz3`: ESP-01S (`esp01_1m`, 160 MHz), Flags `-DSECTOR_STATION -DSTATION_ID=1/2/3`.

### 🔧 Backend Basisstation (`esp8266_rx/src/`)
- **Hardware:** TSOP4838 IR-Empfänger an **D2 (GPIO4)**, Status-LEDs an **D5 (Grün)** & **D6 (Gelb/Signal)**, **Arcade-Speaker an D1 (GPIO5)** getrieben über **BC337-Transistor** (1kΩ Basiswiderstand).
- `ir_decoder.cpp`: Interrupt an D2, strikte Flanken- & Timingprüfung (Leader 1.8–4.2 ms, Bitgrenze 950 µs) mit kompromisslosem Reset bei Störsignalen.
- `lap_logic.cpp`: Live-Zeitmessung für 5 Rennmodi (0=Training, 1=Quali, 2=Rundenrennen, 3=Zeitrennen, 4=Dragrace). Erfasst Sektorzeiten SZ1–SZ4, PBs und Purple Sectors. 2-Frame-Validierungsfilter (85 ms), einstellbare Deadtime (Standard 2 s), dynamische Boxen-/Outlap-Erkennung (Schwellenwert 250% von AvgLap).
- `esp_now_base.cpp`: Empfängt Zwischenzeiten von Sektorstationen via **ESP-NOW**. Sendet periodische Beacons (Stratum 0) für Mikrosekunden-Zeitsynchronisation und Remote-OTA-Befehle.
- `storage.cpp`: `LittleFS`-Dateisystem (`/tracks.json` mit Top 5 Rekorden, `/sessions.json`, `/wifi.json`, `/names.json`, `/schema.json`). Session-Dateien unter `/<track>/<mode>/<timestamp>.json` mit ultrakompakten 1-Buchstaben-Keys gem. Telemetrie-Schema (`c, l, t, p, s, s1..s4, o`). RAM-Puffer (`lapQueue[30]`) gegen Schreibblockaden, EEPROM-Speicherung, 95%-Flash-Schreibschutz.
- `web_server.cpp` / `web_handlers.cpp`: Asynchroner Webserver & WebSockets (`/ws`), REST-APIs, dynamischer Heap-Schutz (HTTP 503 Busy Guard auch auf `/api/laps` / WS-Code 1013 bei <16 kB Heap, >55% Fragmentierung oder <3 kB freiem Block, ausgenommen kritische Core-Assets wie `.css` und `.js`). Dreistufige WLAN-Kaskade (Heim ➔ Hotspot ➔ AP), Geister-WLAN (`LT_<SSID>-<IP>`), SoftAP auf 802.11n mit gelernter IP, hartem 4-Stationen-Limit (`max_connection = 4` gegen Beacon-/Heap-Erschöpfung) und ChipID-Suffix (`RC-LapTimer-XXXX`).

### 📡 Backend Sektorstationen (`esp8266_rx/src_sz/`)
- **Hardware:** ESP-01S, TSOP4838 an **GPIO3 (RX)**, Akkumessung an **A0** (8k2/1k8 Teiler).
- `main.cpp`: Smart-Parent Routing (Zero-Heap Unicast an nächsten Hop mit HW-ACK & 7 Retries) mit sofortigem Broadcast-Failover, 802.11b/g PHY-Modus (-98 dBm Empfindlichkeit), beruhigtes Kanal-Handling (15s Timeout, 1200ms Verweildauer), Pre-OTA Timeout (16s), Totmannschaltung (120s Inaktivität) und Arm-and-Confirm Bestätigung.

### 🚗 Transponder (`attiny13_tx/`)
- **Hardware:** ATtiny13A (9.6 MHz intern), IR-LED an **PB3** getrieben über **BC337-Transistor** (für hohe gepulste IR-Ströme), Spannungsversorgung stabilisiert mit Keramikkondensator (KerKo).
- 38 kHz Trägersignal, ultrakurzes 12-Bit Micro-Frame (~16 ms: 6-Bit Car-ID 1–15 + 6-Bit invertierte Prüf-ID, ID 0 = Startschranke).
- LFSR-Pseudozufallspause (15–30 ms) verhindert Signalüberlagerung im Pulk.

### 🖥️ Frontend Web-App (`esp8266_rx/data/`) – HTML5 / Vanilla JS (LittleFS)
- Autarkes Offline-Design ohne externe CDNs/Fonts/Bilder für sofortiges Laden im AP-Modus ohne Internet. Dateien werden beim Upload über die Web-Oberfläche (`update.html`) automatisch im Browser gezippt (`.gz`), sodass LittleFS die Web-Assets hochkomprimiert ausliefert. Gestaffelter initialer API-Ladevorgang und intelligentes, rollenbasiertes Request-Throttling (`requestUpdateData` mit Jitter: Admin 250–650 ms, Fahrer 800–2400 ms, min. 1000 ms Mindestabstand) schützen den ESP8266 vor TCP-PCB-Erschöpfung bei simultanen Zieldurchfahrten. Direkt-Update des OLED-Dimmers (<15 ms) über WebSocket ohne HTTP-Warten.
- `index.html`: Fahrer-Dashboard mit zuschaltbarem **OLED-Dimmer-Modus** (stromsparende Großanzeige für Handys).
- `admin.html`: Rennleiter-Interface (Session-Steuerung, Streckenauswahl, Fahrerumbenennung, Sensor-Debug).
- `main.js`: WebSocket-Anbindung, Live-Tabelle, Delta-Zeiten, Leaderboard-Sortierung, universelles modulares Dialog-System (`showConfirm`/`showAlert` im Cyberpunk-Look mit dynamischen Farb-Themes, ersetzt alle nativen Browser-Popups im gesamten Frontend).
- `modes.js`: Rennabläufe, automatischer 3-2-1-Los Sprachcountdown, Rennabbruch.
- `stats.js`: Multi-Filter-Statistiken (Strecken & Modi), Fahrerprofile, Rundenhistorie, Backup (.json) & Restore, transparenter `normalizeLap()`-Parser für alte und neue Formate, Client-seitiger Session-Index-Rebuild mit automatischer Flash-Komprimierung alter Session-Dateien, browser-gesteuertes Session-Löschen mit No-Cache-Headers.
- `config.js`: Modals für WLAN-Kaskade, Transponder-Namen, Sensorparameter (Deadtime, 2-Frame-Filter), SZ-OTA-Trigger.
- `tts.js` / `websocket.js`: Web Speech API Sprachausgabe (`de-DE`) mit 100% Echtzeit-WebSocket-Anbindung (<15 ms Latenz bei Zieldurchfahrt), Vorrang-Ansagen ("Noch 1 Minute/30s/10s" und Runden-Countdown "Noch 5/3 Runden, letzte Runde"), 50 ms Audio-Puffer-Entprellung, Warteschlange mit Watchdog, Einzelfahrer-Filterung ("Nur mein Auto") und iOS-Audio-Unlock.
- `update.html` & `debug.html`: LittleFS-Dateimanager (Ordneransicht, ZIP-Download), Firmware-OTA-Upload, universelles Dialog-Modal für Datei-/Ordner-/Flash-Aktionen und Live-Paketmonitor.

---

## ⚡ 3. Die unverzichtbaren Entwicklungs-Regeln

1. **Direktes Arbeiten im Haupt-Repo:** Keine isolierten Worktrees, Änderungen finden direkt im Haupt-Repo statt.
2. **UI Cache-Busting:** Bei Änderungen an `data/*.js` oder `data/*.css` **immer** die Versionsnummer `?v=X.X.X` in `admin.html` und `index.html` anheben (wird am Backend was geändert genauso bei `FIRMWARE_VERSION` in `config.h`). Kleine Änderungen letzte Ziffer, große Änderungen mittlere Ziffer, Milestones vordere Ziffer.
3. **WLAN & Gelernte IP:** Die Basis merkt sich im Heim-WLAN ihre DHCP-IP (`learnedApIP`) und nutzt diese auch im AP-Modus, damit der Nutzer im Browser immer dieselbe Lesezeichen-IP nutzen kann. Das Gateway wird im AP-Modus immer dynamisch auf die eigene Basis-IP gesetzt, damit Mobiltelefone das Subnetz als valide anerkennen und nicht fälschlich auf Mobilfunk ausweichen.
4. **Code-Integrität & Speicherschutz:** Bestehende Kernlogiken (Heap-Guard, 95%-Flash-Schutz, Sektor-Entprellung, ISR-Zeitschranken) niemals eigenmächtig löschen oder lockern. RAM auf dem ESP8266 stets schonen (keine dynamischen Riesen-Strings im Loop).
5. **SZ-Erreichbarkeit & OTA-Lebensversicherung:** Bei Änderungen an den Sektorstationen (`src_sz/`) hat die Ausfallsicherheit für ESP-NOW und Fernwartung/OTA höchste Priorität. Ein Kontaktabriss durch Programmier- oder Logikfehler muss unter allen Umständen vermieden werden, da ein manuelles Neuflashen per seriellem FTDI-Adapter extrem aufwendig ist. Der Broadcast-Peer (`FF:FF:FF:FF:FF:FF`) für OTA-Trigger, Beacons und Notfall-Fallback muss immer aktiv und geschützt bleiben. Siehe Punkt ## 🚨 4.

---

## 🚨 4. ESP8266 Hardware-Limits & Post-Mortem (Lessons Learned)

Diese Regeln resultieren aus einem fatalen OTA-Brick und sind **absolut unverhandelbar**:

1. **Strikte ISR- & Callback-Disziplin (`onDataRecv`):**
   - Im ESP-NOW Empfangs-Callback (`onDataRecv`) dürfen **niemals** mehrere `esp_now_send()` direkt hintereinander aufgerufen werden!
   - Kaskadierende Sends (z. B. Unicast + Broadcast direkt nacheinander) oder blockierende Delays im Callback führen sofort zum Heap-Crash oder Watchdog-Reset (WDT).
   - **Lösung:** Im Callback werden *ausschließlich* Flags gesetzt (z. B. `pendingOtaAck = true`). Das tatsächliche Senden der Antwortpakete passiert asynchron und sicher im `loop()`.

2. **Die 3-stufige OTA-Rettungskaskade (Kein Restart!):**
   - Wenn eine Sektorstation den Wartungsmodus (`connectOtaNetwork()`) betritt, darf sie bei einem fehlgeschlagenen WLAN-Verbindungsversuch **niemals** über `ESP.restart()` neustarten. Ein Restart wirft sie zurück in den Rennbetrieb und sperrt sie dauerhaft aus, wenn die Basis falsche Credentials sendet.
   - **Pflicht-Ablauf:** Stufe 1 (DHCP) ➔ Stufe 2 (Statische IP) ➔ **Stufe 3 (Notfall-AP `SZ-RESCUE-X`)**.
   - Die Station MUSS als Fallback immer ihren eigenen Access Point aufspannen und endlos warten, bis der User sie per OTA rettet.

3. **Kein PHY-Mode Zwang nach unten:**
   - Den ESP8266 niemals hart auf `WIFI_PHY_MODE_11B` zwingen, da dies moderne Router (z.B. OpenWrt) oder Hotspots irritiert und den Handshake crasht. Standard ist und bleibt `11G` oder der normale Mixed-Mode.

4. **Ehrliche Fehlerkultur & Prognosen (Regel für Antigravity):**
   - Wenn der User in einer Endlosschleife hängt, die durch harte Hardware-Watchdogs oder ISR-Crashes verursacht wird: **Keine utopischen Software-Hacks über die Luft (OTA) vorschlagen!**
   - Wenn die Erfolgschance für einen Fix per Funk bei < 10% liegt, gib eine Progrnose aus und fordere den User ungeschönt und direkt auf, den FTDI-Adapter und Lötkolben für einen Hard-Flash herauszuholen. Keine falsche Hoffnung, keine Zeitverschwendung.