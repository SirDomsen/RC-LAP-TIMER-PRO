# 🏁 RC Lap Timer Pro – Projekt-Leitfaden für Antigravity

## 🤝 1. Arbeitsweise & Rolle
* **Rolle:** Du bist der Coding-Assistent und kreative Sparringspartner des Users.
* **Wichtigste Regel:** **Zuerst reden, dann coden!** Bei neuen Ideen, Problemen oder Features diskutieren wir immer zuerst den Ansatz, wägen Optionen ab und denken gemeinsam mit. Erst wenn der User sein klares **„Go“** gibt, wird Code geschrieben oder geändert.

---

## 🏗️ 2. Architektur: Wo wird was gemacht?

### 🔧 Backend Basisstation (`esp8266_rx/src/`) – PlatformIO Env: `basisstation`
* **Hardware:** TSOP4838 IR-Empfänger an **D2 (GPIO4)**, LEDs an **D5 (Grün)** & **D6 (Gelb/Rot)**.
* `lap_logic.cpp`: Live-Zeitmessung, Runden- & Sektorzeiten (SZ1–SZ4), Deadtime-Entprellung, 2-Frame-Filter, Dragrace-Logik.
* `esp_now_base.cpp`: Empfängt Zwischenzeiten von Sektorstationen SZ1–SZ3 via **ESP-NOW**.
* `storage.cpp`: `LittleFS`-Dateisystem (`/tracks.json`, `/sessions.json`, `/wifi.json`, `/names.json`), Streckenrekorde & Tagesbestzeiten (Top 5), EEPROM, 95%-Flash-Schreibschutz.
* `web_server.cpp` / `web_handlers.cpp`: Webserver & WebSockets, REST-APIs, WLAN-Kaskade, Geister-WLAN (`LT_<SSID>-<IP>`), SoftAP mit gelernter IP und ChipID-Suffix (`RC-LapTimer-XXXX`).

### 📡 Backend Sektorstationen (`esp8266_rx/src_sz/`) – PlatformIO Env: `sektorstation`
* TSOP4838 an **D2**, Akkumessung an **A0** (8k2/1k8 Teiler). Sendet Sektordurchfahrten via ESP-NOW an Basis.

### 🚗 Transponder (`attiny13_tx/`)
* ATtiny13 sendet 38 kHz Infrarot-Signale mit Car-IDs 1–15 (ID 0 = Startschranke).

### 🖥️ Frontend Web-App (`esp8266_rx/data/`) – HTML5 / Vanilla JS (LittleFS)
* `index.html` (Fahrer-Dashboard) & `admin.html` (Rennleiter / Vollzugriff).
* `main.js`: WebSocket-Anbindung, Live-Tabelle, Dashboard / OLED-Dimmer-Modus.
* `modes.js`: Renn- und Zeitmessmodi (Freies Training, Quali, Rundenrennen, Zeitrennen, Dragrace).
* `stats.js`: Rundenhistorie, Fahrer-Statistiken, Backup & Restore.
* `config.js`: Modals für WLAN-Wechsel, Transponder-Namen, Sensorparameter, SZ-OTA-Trigger.
* `tts.js`: Web Speech API Sprachausgabe (Fahrerauswahl, Sektor-PBs, Rundenzeiten).

---

## ⚡ 3. Die unverzichtbaren Entwicklungs-Regeln

1. **Dual-Tree-Sync:** Änderungen müssen **immer gleichzeitig** im Git-Worktree und im Haupt-Repo (`C:\#Coding\Github\RC-LAP-TIMER-PRO\esp8266_rx\`) synchron gehalten werden!
2. **UI Cache-Busting:** Bei Änderungen an `data/*.js` oder `data/*.css` **immer** die Versionsnummer `?v=X.X.X` in `admin.html` und `index.html` anheben (synchron mit `FIRMWARE_VERSION` in `config.h`). Kleine Änderungen letzte Ziffer, große Änderungen mittlere Ziffer, Milestones vordere Ziffer.
3. **WLAN & Gelernte IP:** Die Basis merkt sich im Heim-WLAN ihre DHCP-IP (`learnedApIP`) und nutzt diese auch im AP-Modus, damit der Nutzer im Browser immer dieselbe Lesezeichen-IP nutzen kann.
4. **Code-Integrität:** Bestehende Kommentare und Kernlogiken niemals eigenmächtig löschen.
