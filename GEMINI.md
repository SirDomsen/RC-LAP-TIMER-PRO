# 🏁 RC Lap Timer Pro – Projekt-Leitfaden & Roadmap für Antigravity

## 🤝 1. Arbeitsweise & Rolle
* **Rolle:** Du bist der Coding-Assistent und kreative Sparringspartner des Users.
* **Wichtigste Regel:** **Zuerst reden, dann coden!** Bei neuen Ideen, Problemen oder Features diskutieren wir immer zuerst den Ansatz, wägen Optionen ab und denken gemeinsam mit. Erst wenn der User sein klares **„Go“** gibt, wird Code geschrieben oder geändert.
* Wir ändern oder kürzen nichts ungefragt!

---

## 🏗️ 2. Architektur & Roadmap: Wo wird was gemacht?

### ⚙️ PlatformIO Build-Environments (`platformio.ini`)
* `basisstation`: NodeMCU V2 (`nodemcuv2`, 160 MHz, LittleFS, LWIP2 High Bandwidth), AsyncWebServer, ArduinoJson 7.
* `sz1`, `sz2`, `sz3`: ESP-01S (`esp01_1m`, 160 MHz), Flags `-DSECTOR_STATION -DSTATION_ID=1/2/3`.

### 🔧 Backend Basisstation (`esp8266_rx/src/`)
* **Hardware:** TSOP4838 IR-Empfänger an **D2 (GPIO4)**, Status-LEDs an **D5 (Grün)** & **D6 (Gelb/Signal)**.
* `ir_decoder.cpp`: Interrupt an D2, strikte Flanken- & Timingprüfung (Leader 1.8–4.2 ms, Bitgrenze 950 µs) mit kompromisslosem Reset bei Störsignalen.
* `lap_logic.cpp`: Live-Zeitmessung für 5 Rennmodi (0=Training, 1=Quali, 2=Rundenrennen, 3=Zeitrennen, 4=Dragrace). Erfasst Sektorzeiten SZ1–SZ4, PBs und Purple Sectors. 2-Frame-Validierungsfilter (85 ms), einstellbare Deadtime (Standard 2 s), dynamische Boxen-/Outlap-Erkennung.
* `esp_now_base.cpp`: Empfängt Zwischenzeiten von Sektorstationen via **ESP-NOW**. Sendet periodische Beacons (Stratum 0) für Mikrosekunden-Zeitsynchronisation und Remote-OTA-Befehle.
* `storage.cpp`: `LittleFS`-Dateisystem (`/tracks.json` mit Top 5 Rekorden, `/sessions.json`, `/wifi.json`, `/names.json`). Session-Dateien unter `/<track>/<mode>/<timestamp>.json`. RAM-Puffer (`lapQueue[30]`) gegen Schreibblockaden, EEPROM-Speicherung, 95%-Flash-Schreibschutz.
* `web_server.cpp` / `web_handlers.cpp`: Asynchroner Webserver & WebSockets (`/ws`), REST-APIs, dynamischer Heap-Schutz (HTTP 503 Busy Guard / WS-Code 1013 bei <20 kB Heap oder >47% Fragmentierung). Dreistufige WLAN-Kaskade (Heim ➔ Hotspot ➔ AP), Geister-WLAN (`LT_<SSID>-<IP>`), SoftAP mit gelernter IP und ChipID-Suffix (`RC-LapTimer-XXXX`).

### 📡 Backend Sektorstationen (`esp8266_rx/src_sz/`)
* **Hardware:** ESP-01S, TSOP4838 an **GPIO3 (RX)**, Akkumessung an **A0** (8k2/1k8 Teiler).
* `main.cpp`: Kanal-Hopping bis zum Basis-Lock, Zeitsynchronisation mit ~1 ms Hop-Kompensation, Weiterleitung von Nachbarstationen (Relay-Queue & Cache), Akkuwarnung und Fern-Wartungsmodus für OTA.

### 🚗 Transponder (`attiny13_tx/`)
* ATtiny13A (9.6 MHz intern), IR-LED an **PB3**.
* 38 kHz Trägersignal, ultrakurzes 12-Bit Micro-Frame (~16 ms: 6-Bit Car-ID 1–15 + 6-Bit invertierte Prüf-ID, ID 0 = Startschranke).
* LFSR-Pseudozufallspause (15–30 ms) verhindert Signalüberlagerung im Pulk.

### 🖥️ Frontend Web-App (`esp8266_rx/data/`) – HTML5 / Vanilla JS (LittleFS)
* `index.html`: Fahrer-Dashboard mit zuschaltbarem **OLED-Dimmer-Modus** (stromsparende Großanzeige für Handys).
* `admin.html`: Rennleiter-Interface (Session-Steuerung, Streckenauswahl, Fahrerumbenennung, Sensor-Debug).
* `main.js`: WebSocket-Anbindung, Live-Tabelle, Delta-Zeiten, Leaderboard-Sortierung.
* `modes.js`: Rennabläufe, automatischer 3-2-1-Los Sprachcountdown, Rennabbruch.
* `stats.js`: Multi-Filter-Statistiken (Strecken & Modi), Fahrerprofile, Rundenhistorie, Backup (.json) & Restore.
* `config.js`: Modals für WLAN-Kaskade, Transponder-Namen, Sensorparameter (Deadtime, 2-Frame-Filter), SZ-OTA-Trigger.
* `tts.js`: Web Speech API Sprachausgabe (`de-DE`), Warteschlange mit Watchdog, Einzelfahrer-Filterung ("Nur mein Auto"), iOS-Audio-Unlock.
* `update.html` & `debug.html`: LittleFS-Dateimanager (Ordneransicht, ZIP-Download), Firmware-OTA-Upload und Live-Paketmonitor.

---

## ⚡ 3. Die unverzichtbaren Entwicklungs-Regeln
2. **UI Cache-Busting:** Bei Änderungen an `data/*.js` oder `data/*.css` **immer** die Versionsnummer `?v=X.X.X` in `admin.html` und `index.html` anheben (synchron mit `FIRMWARE_VERSION` in `config.h`). Kleine Änderungen letzte Ziffer, große Änderungen mittlere Ziffer, Milestones vordere Ziffer.
3. **WLAN & Gelernte IP:** Die Basis merkt sich im Heim-WLAN ihre DHCP-IP (`learnedApIP`) und nutzt diese auch im AP-Modus, damit der Nutzer im Browser immer dieselbe Lesezeichen-IP nutzen kann.
4. **Code-Integrität & Speicherschutz:** Bestehende Kernlogiken (Heap-Guard, 95%-Flash-Schutz, Sektor-Entprellung, ISR-Zeitschranken) niemals eigenmächtig löschen oder lockern. RAM auf dem ESP8266 stets schonen (keine dynamischen Riesen-Strings im Loop).
