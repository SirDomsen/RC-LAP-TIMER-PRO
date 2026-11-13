# RC Lap Timer Pro 🏎️⏱️

Ein autarkes, WLAN-basiertes Infrarot-Rundenzählersystem für RC-Cars (1:10, 1:8, 1:24 u. a.) auf Basis von **ESP8266** oder **ESP32-S3**, drahtlosen **ESP-01S-Sektorstationen** und **ATtiny13A-Transpondern**.

Die komplette Steuerung und Live-Zeitnahme läuft direkt über eine im Browser bedienbare, offline-fähige Web-App – ohne Internetverbindung und ohne zusätzliche App-Installation.

> **Aktueller Stand:** Basisstation-Firmware `v4.7.18` · Offline-Statistik-App `v0.9.5`

---

## ✨ Funktionen

* **Infrarot-Zeitnahme für bis zu 15 Fahrzeuge:** 38-kHz-Transponder mit 12-Bit-Micro-Frames (6-Bit-Car-ID + 6-Bit-Prüf-ID), 2-Frame-Filter und konfigurierbarer Deadtime.
* **6 Rennmodi + persönlicher Ghosthunt:**
  * **Freies Training** – endlose Runden- und Sektorerfassung
  * **Qualifying** – zeitbasiertes Zeitfahren mit Restzeitansage
  * **Rennen (Runden)** & **Rennen (Zeit)** – mit synchronisiertem Startampel-Countdown
  * **Dragrace** – Sprint-Messung von der Basis bis zur Sektor-Zielschranke
  * **Ghost Battle** (Multiplayer) & **Ghosthunt** (Einzelspieler) – Echtzeit-Sektor- und Runden-Deltas gegen Streckenrekord, Tagesbestzeit, PB, Führenden oder Wunschzeit
* **Drahtlose Sektorzeiten über ESP-NOW:** Bis zu 3 Sektorstationen (`SZ1`–`SZ3`, Start/Ziel an der Basis = `SZ4`) mit Mikrosekunden-Zeitsynchronisation (Stratum-Beacons), automatischer Kanalverfolgung und Unicast-/Broadcast-Relay-Failover.
* **Zwei Rollen & OLED-Dimmer:** Bereinigte Fahreransicht (`index.html`), Vollzugriff für den Rennleiter (`admin.html`), stromsparender **OLED-Dimmer-Dashboard-Modus** fürs Handy an der Strecke und Offline-QR-Code zum schnellen Beitreten.
* **Sprachausgabe (TTS) & Arcade-Audio:**
  * Deutsche Live-Ansagen (Web Speech API `de-DE`) inkl. Fahrerfilter (*„Nur mein Auto“*) und iOS-Audio-Unlock.
  * Klangenidentischer Smartphone-Synthesizer und optionaler Hardware-Lautsprecher an der Basisstation mit Startampel, Rekord-/Geisterjagd-Fanfaren, Sektor-Ticks und 8 Original-MS-DOS-Chiptunes (*Grand Prix Circuit*, *Test Drive 1*, *Test Drive 3*).
* **Statistiken & eigenständige Offline-Auswertung:** LittleFS-Speicherung von Strecken, Top-5-Allzeit-/Tagesrekorden, Sessions und Einzelrunden. Über `LT_Base/offline_stats/` steht zudem eine lokale Vollbild-App (IndexedDB, JSON-/ZIP-Import & -Export, direkter WLAN-Sync) bereit.
* **Wartung, Diagnose & OTA:** Integrierter LittleFS-Dateimanager mit ZIP-Backup/Restore und Firmware-OTA (`update.html`), Live-Systemmonitor mit Audio-Testkonsole (`debug.html`) sowie eine 4-stufige OTA-Rettungskaskade für Sektorstationen.

---

## 🧩 Projektstruktur

| Verzeichnis | Hardware / Ziel | Beschreibung |
| :--- | :--- | :--- |
| [`LT_Base/`](LT_Base/) | ESP8266 NodeMCU V2/V3 oder ESP32-S3 DevKitC-1 | Basisstations-Firmware (`src/common/` + `src/platform/`), gemeinsame Library `lib/LTCommon/`, LittleFS-Web-App (`data/`), Arcade-Soundtracks (`Sound/`) |
| [`LT_Base/offline_stats/`](LT_Base/offline_stats/) | Browser (lokal / offline) | Eigenständige, schreibgeschützte Statistik-Auswertung mit IndexedDB, ZIP-/JSON-Import und direktem WLAN-Abruf vom Lap Timer |
| [`LT_SectorStation/`](LT_SectorStation/) | ESP-01S (`sz1`–`sz3`) | Sektorstations-Firmware für Zwischenzeiten und ESP-NOW-Relay; nutzt `LT_Base/lib/LTCommon/` |
| [`attiny13_tx/`](attiny13_tx/) | ATtiny13A | Referenz-Sketch (`Attiny_tx.ino`) für den 38-kHz-IR-Transponder mit LFSR-Zufallspause |

---

## 🔧 Hardware & Pinbelegung

### Basisstation

| Funktion | ESP8266 (NodeMCU V2/V3) | ESP32-S3 (DevKitC-1 N16R8) | Hinweis |
| :--- | :--- | :--- | :--- |
| **IR-Empfänger (TSOP4838)** | `D2` (`GPIO4`) | `GPIO4` | Interrupt auf fallende Flanke |
| **Ready-LED (Grün)** | `D5` (`GPIO14`) | `GPIO5` | System-/WLAN-Status |
| **Signal-LED (Gelb)** | `D6` (`GPIO12`) | `GPIO6` | IR-Signal / Durchfahrt |
| **Arcade-Speaker** | `D1` (`GPIO5`) | `GPIO7` | Optional, via BC337-Transistor (1 kΩ Basiswiderstand) |
| **Onboard-LED** | `LED_BUILTIN` (`GPIO2`) | `LED_BUILTIN` (Fallback `GPIO2`) | Active LOW |

> **Hinweis zum ESP32-S3:** `GPIO33`–`GPIO37` sind für Octal-PSRAM/Flash reserviert, `GPIO19`/`GPIO20` für USB-CDC.

### Sektorstationen (`SZ1`–`SZ3`, ESP-01S)

* **`GPIO3` (RX):** TSOP4838 IR-Empfänger
* **`GPIO2`:** Blaue Onboard-LED (Active LOW)
* **`GPIO1` (TX):** Hart auf `LOW` geschaltet (Massebezug, keine serielle Konsole)
* **`GPIO0`:** `INPUT_PULLUP` zur Batterieüberwachung (`HIGH` = Akku leer)

### Transponder (`attiny13_tx`)

* **MCU:** ATtiny13A (9,6 MHz intern)
* **`PB3` (Pin 2):** 38-kHz-IR-LED (empfohlen über BC337-Transistor)
* **Protokoll:** 12-Bit-Frame (~16 ms) aus 6-Bit-Car-ID (`1`–`15`, ID `0` = Startschranke) + 6-Bit-invertierter Car-ID sowie 15–30 ms LFSR-Pseudozufallspause gegen Überlagerungen im Pulk.

---

## 🛠️ Setup & Build (PlatformIO)

1. **PlatformIO installieren** (PlatformIO Core CLI oder VS-Code-Erweiterung).
2. **Optionale WLAN-Zugangsdaten anlegen:**
   Die Firmware enthält Standardwerte und kompiliert auch ohne eigene Zugangsdaten. Für dein Heim-WLAN kannst du folgende, von Git ignorierte Dateien anlegen:

   * **`LT_Base/src/common/secrets.h`:**
     ```cpp
     #define HOME_SSID "DeinWlanName"
     #define HOME_PASS "DeinWlanPasswort"
     ```
   * **`LT_SectorStation/src/secrets.h`:** (identischer Inhalt für Stufe 2 der OTA-Rettungskaskade)

3. **Firmware & Dateisystem vom Repository-Root aus bauen:**

   ```sh
   # Basisstation (ESP8266 NodeMCU V2)
   pio run -d LT_Base -e basisstation

   # Basisstation (ESP32-S3 DevKitC-1 N16R8)
   pio run -d LT_Base -e basisstation_s3

   # LittleFS-Web-App der Basisstation bauen
   pio run -d LT_Base -e basisstation -t buildfs
   # bzw. für ESP32-S3:
   pio run -d LT_Base -e basisstation_s3 -t buildfs

   # Sektorstationen (sz1, sz2, sz3)
   pio run -d LT_SectorStation -e sz1
   pio run -d LT_SectorStation -e sz2
   pio run -d LT_SectorStation -e sz3
   ```

4. **Flashen (USB vs. OTA):**
   Die Environments in `platformio.ini` sind für OTA-Uploads vorkonfiguriert. Für das erstmalige Flashen per USB-Kabel das normale Serial-Upload-Target verwenden (`-t upload --upload-port COMx`). Web-Assets lassen sich danach ebenfalls bequem per Browser über `update.html` (inkl. automatischer `.gz`-Komprimierung) hochladen.

---

## 🚀 Erste Schritte & Bedienung

### 1. Mit dem Lap Timer verbinden

Die Basisstation nutzt eine **WLAN-Kaskade**: *Heim-WLAN → Mobiler Hotspot → Eigener SoftAP*.

* **Direkter AP-Modus:** Mit dem WLAN `RC-LapTimer-XXXX` verbinden (Standardpasswort: `Rennstrecke123`) und im Browser `http://192.168.0.8` (bzw. die zuletzt im Heimnetz gelernte IP) aufrufen.
* **Heim-WLAN / Hotspot:** Befindet sich die Basis im Heim-WLAN oder Hotspot, spannt sie beim Booten kurzzeitig ein Hilfs-WLAN namens `LT_<SSID>-<IP>` auf, damit die aktuelle IP direkt abgelesen werden kann (schaltet sich nach dem ersten Webaufruf automatisch ab).

### 2. Web-Oberflächen

* `/` bzw. `/index.html` – **Fahreransicht** (Live-Zeiten, OLED-Dimmer-Dashboard, persönlicher Ghosthunt, TTS-Filter)
* `/admin.html` – **Rennleiteransicht** (Strecken, Transponder-Zuordnung, Rennmodi, Arcade-Audio-Jukebox, QR-Code, WLAN-Kontrollzentrum)
* `/update.html` – **Systempflege** (LittleFS-Dateimanager, ZIP-Backup/Restore, Firmware-OTA)
* `/debug.html` – **Diagnose** (Heap, Loop-Zeiten, ESP-NOW-Pakete, Asset-Check, Audio-/TTS-Testkonsole)

---

## 📚 Weiterführende Dokumentation

* [**BEDIENUNGSANLEITUNG.md**](BEDIENUNGSANLEITUNG.md) – Ausführliche Schritt-für-Schritt-Anleitung für Fahrer und Rennleiter
* [**ROADMAP.md**](ROADMAP.md) – Systemarchitektur, Signalverarbeitung, Protokolle und Modulübersicht
* [**LT_Base/OfflineStats.md**](LT_Base/OfflineStats.md) – Dokumentation der eigenständigen Offline-Statistik-App (`LT_Base/offline_stats/`)
* [**LT_SectorStation/Sektorstationen (SZ).md**](LT_SectorStation/Sektorstationen%20(SZ).md) – ESP-NOW-Funkprotokoll, Relay-Logik und 4-Stufen-OTA-Rettung
* [**SpeedLinkV2-interface.md**](SpeedLinkV2-interface.md) – Architekturrahmen für den geplanten ESP-NOW-Telemetrieaustausch mit SpeedLinkV2
* [**LT_Base/GEMINI.md**](LT_Base/GEMINI.md) & [**LT_Base/CONVENTIONS.md**](LT_Base/CONVENTIONS.md) – Interne Entwicklungs- und Embedded-Richtlinien

---

## 📄 Lizenz

Dieses Projekt steht unter der **MIT License**.
