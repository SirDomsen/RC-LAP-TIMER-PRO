🇩🇪 **Deutsch** | 🇬🇧 [English](README.en.md)

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
* **`PB3` (Pin 2):** 38-kHz-IR-LED (über BC337-Transistor)
* **Protokoll:** 12-Bit-Frame (~16 ms) aus 6-Bit-Car-ID (`1`–`15`, ID `0` = Startschranke) + 6-Bit-invertierter Car-ID sowie 15–30 ms LFSR-Pseudozufallspause gegen Überlagerungen im Pulk.

### Schaltplan & Verdrahtung (Übersicht)

#### 1. Basisstation (ESP8266 / ESP32-S3)

```text
  ESP8266 (NodeMCU) / ESP32-S3
  ============================
  3.3V -----------------------+
                              |
                         [TSOP4838] (IR-Empfänger, Stütz-C an VCC/GND empfohlen)
  D2 / GPIO4  <-- OUT --------+
  GND ------------------------+

  D5 / GPIO5  --> [220–330 Ω] --> (LED Grün: Ready)  --> GND
  D6 / GPIO6  --> [220–330 Ω] --> (LED Gelb: Signal) --> GND

  Arcade-Speaker (optional, 32 Ω):
  5V / VIN --> (+) Speaker (-) --> Collector [BC337] Emitter --> GND
                                               ^
                                             Basis
                                               |
  D1 / GPIO7 ----------------------------> [ 1 kΩ ]
```
*(Im Standby wird der Speaker-Pin fest auf `LOW` gehalten, sodass der BC337 sperrt und kein Ruhestrom fließt.)*

#### 2. Sektorstation (`SZ1`–`SZ3`, ESP-01S)

```text
  ESP-01S (3.3V)
  ==============
  3.3V -----------------------+
  GPIO3 (RX)  <-- OUT -- [TSOP4838]
  GND ------------------------+

  GPIO1 (TX)  --> Hart auf LOW geschaltet (Massebezug, kein Serial)
  GPIO0       --> Batterie-Check (INPUT_PULLUP; HIGH = Akku leer)
  A0          --> Akkumessung über Spannungsteiler (8,2 kΩ / 1,8 kΩ)
```

#### 3. IR-Transponder (ATtiny13A)

```text
  ATtiny13A (3.3V–5V, KerKo 100 nF an VCC/GND)
  =============================================
  VCC --> (+) IR-LED (940 nm) (-) --> Collector [BC337] Emitter --> GND
                                                  ^
                                                Basis
                                                  |
  PB3 (Pin 2) ------------------------------> [Basis-R]
```

> Eine ausführlichere Schaltungsdokumentation liegt zusätzlich in [`SChaltplanASCII.docx`](SChaltplanASCII.docx).

---

## 📥 Fertige Downloads (GitHub Releases)

Für alle, die nicht selbst kompilieren möchten, stehen unter [**GitHub Releases**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases) automatisch gebaute Pakete zum direkten Download bereit:

👉 [**Aktuellstes Release herunterladen**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases/latest) · [**Alle Releases anzeigen**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases)

* **Basisstation ESP8266:** `LT_Base_ESP8266_firmware.bin` + `LT_Base_ESP8266_littlefs.bin`
* **Basisstation ESP32-S3:** `LT_Base_ESP32-S3_firmware.bin`, `LT_Base_ESP32-S3_bootloader.bin`, `LT_Base_ESP32-S3_partitions.bin` + `LT_Base_ESP32-S3_littlefs.bin`
* **Sektorstationen (`SZ1`–`SZ3`):** `LT_SectorStation_SZ1_firmware.bin`, `SZ2`, `SZ3`
* **Web-Frontend (OTA-Upload über `update.html`):** `LT_Base_Frontend.zip`
* **Offline-Statistik (1-Datei-HTML):** `offline_stats_standalone.html` (direkt im Browser öffnen, kein Server nötig)

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

### IR-Transponder bauen & flashen (ATtiny13A)

Da jeder Transponder eine eigene Fahrzeug-ID benötigt, wird [`attiny13_tx/Attiny_tx.ino`](attiny13_tx/Attiny_tx.ino) direkt selbst angepasst und per ISP-Programmer (z. B. USBasp oder Arduino-as-ISP) geflasht:

1. **ID festlegen:** In [`attiny13_tx/Attiny_tx.ino`](attiny13_tx/Attiny_tx.ino#L8) `#define CAR_ID` auf die gewünschte Fahrzeug-ID (`1`–`15`) setzen.
2. **Takt & Fuses beachten (`9,6 MHz`):** Der Sketch setzt `F_CPU 9600000UL` voraus. Werksneue ATtiny13A laufen meist mit `1,2 MHz` (`CKDIV8`) – damit wäre die 38-kHz-Trägerfrequenz falsch.
3. **Flashen über Arduino IDE (mit *MicroCore*):**
   * Board: **ATtiny13** · Clock: **9.6 MHz internal osc.**
   * Einmalig **Werkzeuge → Bootloader brennen** ausführen (setzt die Fuses auf 9,6 MHz).
   * Danach **Sketch → Hochladen mit Programmer**.
4. **Alternativ per CLI (`avr-gcc` + `avrdude`, z. B. mit USBasp):**
   ```sh
   avr-gcc -mmcu=attiny13a -Os -x c attiny13_tx/Attiny_tx.ino -o attiny_tx.elf
   avr-objcopy -O ihex -R .eeprom attiny_tx.elf attiny_tx.hex
   avrdude -c usbasp -p t13 -U lfuse:w:0x7a:m -U hfuse:w:0xff:m -U flash:w:attiny_tx.hex:i
   ```

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

## 🎥 Erklärvideos (YouTube)

Aufbau, Einrichtung und Bedienung gibt es auch als deutschsprachige Video-Reihe auf YouTube:

👉 [**YouTube-Playlist: RC Lap Timer Pro – Erklärvideos (Deutsch)**](https://www.youtube.com/playlist?list=PLb4HYK9joTps)

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
