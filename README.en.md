🇩🇪 [Deutsch](README.md) | 🇬🇧 **English**

# RC Lap Timer Pro 🏎️⏱️

A standalone, Wi-Fi-based infrared lap timing system for RC cars (1:10, 1:8, 1:24, etc.) built on **ESP8266** or **ESP32-S3**, wireless **ESP-01S sector stations**, and **ATtiny13A transponders**.

All controls and live timing run directly in a browser-based, offline-ready web app — no internet connection and no additional app installation required.

> **Current version:** Base firmware `v4.7.18` · Offline Stats App `v0.9.5`

---

## ✨ Features

* **Infrared lap timing for up to 15 cars:** 38 kHz transponders using 12-bit micro-frames (6-bit car ID + 6-bit inverted check ID), 2-frame filter, and configurable deadtime.
* **6 race modes + personal Ghosthunt:**
  * **Free Practice** – continuous lap and sector timing
  * **Qualifying** – timed qualifying with remaining-time announcements
  * **Race (Laps)** & **Race (Time)** – with synchronized starting-light countdown
  * **Drag Race** – sprint timing from the base station to the sector finish gate
  * **Ghost Battle** (multiplayer) & **Ghosthunt** (single-player) – real-time sector and lap deltas against track record, daily best, personal best (PB), live leader, or custom target time
* **Wireless sector timing via ESP-NOW:** Up to 3 sector stations (`SZ1`–`SZ3`, base start/finish = `SZ4`) with microsecond time synchronization (stratum beacons), automatic channel tracking, and unicast/broadcast relay failover.
* **Two roles & OLED dimmer:** Clean driver view (`index.html`), full race-director access (`admin.html`), power-saving **OLED dimmer dashboard mode** for trackside phones, and an offline QR code for quick joining.
* **Voice output (TTS) & arcade audio:**
  * Live voice announcements via Web Speech API (including per-car filter *"Announce my car only"* and iOS audio unlock).
  * Matching smartphone Web Audio synthesizer and optional hardware speaker on the base station with starting lights, record/ghost-victory fanfares, sector ticks, and 8 classic MS-DOS chiptunes (*Grand Prix Circuit*, *Test Drive 1*, *Test Drive 3*).
* **Statistics & standalone offline analysis:** LittleFS storage for tracks, top-5 all-time/daily records, sessions, and individual laps. `LT_Base/offline_stats/` also provides a standalone fullscreen web app (IndexedDB, JSON/ZIP import & export, direct Wi-Fi sync from the lap timer).
* **Maintenance, diagnostics & OTA:** Built-in LittleFS file manager with ZIP backup/restore and firmware OTA (`update.html`), live system monitor with audio test console (`debug.html`), plus a 4-stage OTA rescue cascade for sector stations.

---

## 🧩 Project Structure

| Directory | Hardware / Target | Description |
| :--- | :--- | :--- |
| [`LT_Base/`](LT_Base/) | ESP8266 NodeMCU V2/V3 or ESP32-S3 DevKitC-1 | Base station firmware (`src/common/` + `src/platform/`), shared `lib/LTCommon/` library, LittleFS web app (`data/`), arcade soundtracks (`Sound/`) |
| [`LT_Base/offline_stats/`](LT_Base/offline_stats/) | Browser (local / offline) | Standalone read-only statistics viewer with IndexedDB, ZIP/JSON import, and direct Wi-Fi sync from the lap timer |
| [`LT_SectorStation/`](LT_SectorStation/) | ESP-01S (`sz1`–`sz3`) | Sector station firmware for split times and ESP-NOW relay; shares `LT_Base/lib/LTCommon/` |
| [`attiny13_tx/`](attiny13_tx/) | ATtiny13A | Reference sketch (`Attiny_tx.ino`) for the 38 kHz IR transponder with LFSR pseudo-random delay |

---

## 🔧 Hardware & Pinout

### Base Station

| Function | ESP8266 (NodeMCU V2/V3) | ESP32-S3 (DevKitC-1 N16R8) | Notes |
| :--- | :--- | :--- | :--- |
| **IR Receiver (TSOP4838)** | `D2` (`GPIO4`) | `GPIO4` | Falling-edge interrupt |
| **Ready LED (Green)** | `D5` (`GPIO14`) | `GPIO5` | System / Wi-Fi status |
| **Signal LED (Yellow)** | `D6` (`GPIO12`) | `GPIO6` | IR signal / crossing |
| **Arcade Speaker** | `D1` (`GPIO5`) | `GPIO7` | Optional, driven via BC337 transistor (1 kΩ base resistor) |
| **Onboard LED** | `LED_BUILTIN` (`GPIO2`) | `LED_BUILTIN` (fallback `GPIO2`) | Active LOW |

> **ESP32-S3 note:** `GPIO33`–`GPIO37` are reserved for Octal-PSRAM/Flash, and `GPIO19`/`GPIO20` for USB-CDC.

### Sector Stations (`SZ1`–`SZ3`, ESP-01S)

* **`GPIO3` (RX):** TSOP4838 IR receiver
* **`GPIO2`:** Blue onboard LED (Active LOW)
* **`GPIO1` (TX):** Driven hard `LOW` (ground reference, no serial console)
* **`GPIO0`:** `INPUT_PULLUP` for battery monitoring (`HIGH` = low battery)

### Transponder (`attiny13_tx`)

* **MCU:** ATtiny13A (9.6 MHz internal)
* **`PB3` (Pin 2):** 38 kHz IR LED (via BC337 transistor)
* **Protocol:** 12-bit frame (~16 ms) consisting of 6-bit car ID (`1`–`15`, ID `0` = start gate) + 6-bit inverted car ID, plus a 15–30 ms LFSR pseudo-random pause to avoid collisions in pack racing.

### Wiring & Schematic (Overview)

#### 1. Base Station (ESP8266 / ESP32-S3)

```text
  ESP8266 (NodeMCU) / ESP32-S3
  ============================
  3.3V -----------------------+
                              |
                         [TSOP4838] (IR receiver, decoupling cap on VCC/GND recommended)
  D2 / GPIO4  <-- OUT --------+
  GND ------------------------+

  D5 / GPIO5  --> [220–330 Ω] --> (Green LED: Ready)  --> GND
  D6 / GPIO6  --> [220–330 Ω] --> (Yellow LED: Signal) --> GND

  Arcade Speaker (optional, 32 Ω):
  5V / VIN --> (+) Speaker (-) --> Collector [BC337] Emitter --> GND
                                               ^
                                             Base
                                               |
  D1 / GPIO7 ----------------------------> [ 1 kΩ ]
```
*(In standby, the speaker pin is held `LOW` so the BC337 stays off and draws zero quiescent current.)*

#### 2. Sector Station (`SZ1`–`SZ3`, ESP-01S)

```text
  ESP-01S (3.3V)
  ==============
  3.3V -----------------------+
  GPIO3 (RX)  <-- OUT -- [TSOP4838]
  GND ------------------------+

  GPIO1 (TX)  --> Driven hard LOW (ground reference, no Serial)
  GPIO0       --> Battery check (INPUT_PULLUP; HIGH = battery low)
  A0          --> Battery voltage divider (8.2 kΩ / 1.8 kΩ)
```

#### 3. IR Transponder (ATtiny13A)

```text
  ATtiny13A (3.3V–5V, 100 nF ceramic cap across VCC/GND)
  =======================================================
  VCC --> (+) IR LED (940 nm) (-) --> Collector [BC337] Emitter --> GND
                                                  ^
                                                Base
                                                  |
  PB3 (Pin 2) ------------------------------> [Base R]
```

> More detailed wiring notes are available in [`SChaltplanASCII.docx`](SChaltplanASCII.docx).

---

## 📥 Pre-built Downloads (GitHub Releases)

If you prefer not to compile from source, pre-built packages are automatically published under [**GitHub Releases**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases):

👉 [**Download Latest Release**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases/latest) · [**View All Releases**](https://github.com/SirDomsen/RC-LAP-TIMER-PRO/releases)

* **Base Station ESP8266:** `LT_Base_ESP8266_firmware.bin` + `LT_Base_ESP8266_littlefs.bin`
* **Base Station ESP32-S3:** `LT_Base_ESP32-S3_firmware.bin`, `LT_Base_ESP32-S3_bootloader.bin`, `LT_Base_ESP32-S3_partitions.bin` + `LT_Base_ESP32-S3_littlefs.bin`
* **Sector Stations (`SZ1`–`SZ3`):** `LT_SectorStation_SZ1_firmware.bin`, `SZ2`, `SZ3`
* **Web Frontend (OTA upload via `update.html`):** `LT_Base_Frontend.zip`
* **Offline Statistics (single-file HTML):** `offline_stats_standalone.html` (open directly in any browser, no server needed)

---

## 🛠️ Setup & Build (PlatformIO)

1. **Install PlatformIO** (PlatformIO Core CLI or the VS Code PlatformIO extension).
2. **Create optional Wi-Fi credentials:**
   The firmware ships with defaults and compiles without custom credentials. To add your home Wi-Fi, create these git-ignored files:

   * **`LT_Base/src/common/secrets.h`:**
     ```cpp
     #define HOME_SSID "YourWifiName"
     #define HOME_PASS "YourWifiPassword"
     ```
   * **`LT_SectorStation/src/secrets.h`:** (same contents, used as stage-2 fallback in the OTA rescue cascade)

3. **Build firmware & filesystem from the repository root:**

   ```sh
   # Base station (ESP8266 NodeMCU V2)
   pio run -d LT_Base -e basisstation

   # Base station (ESP32-S3 DevKitC-1 N16R8)
   pio run -d LT_Base -e basisstation_s3

   # Build LittleFS web app image for the base station
   pio run -d LT_Base -e basisstation -t buildfs
   # or for ESP32-S3:
   pio run -d LT_Base -e basisstation_s3 -t buildfs

   # Sector stations (sz1, sz2, sz3)
   pio run -d LT_SectorStation -e sz1
   pio run -d LT_SectorStation -e sz2
   pio run -d LT_SectorStation -e sz3
   ```

4. **Flashing (USB vs. OTA):**
   The environments in `platformio.ini` are pre-configured for OTA uploads. For the initial flash over USB, use the normal serial upload target (`-t upload --upload-port COMx`). Web assets can later be uploaded right from the browser via `update.html` (with automatic `.gz` compression).

### Building & Flashing the IR Transponder (ATtiny13A)

Because each transponder needs its own car ID, [`attiny13_tx/Attiny_tx.ino`](attiny13_tx/Attiny_tx.ino) is configured and flashed directly using an ISP programmer (e.g., USBasp or Arduino-as-ISP):

1. **Set the Car ID:** Edit `#define CAR_ID` in [`attiny13_tx/Attiny_tx.ino`](attiny13_tx/Attiny_tx.ino#L8) to the desired ID (`1`–`15`).
2. **Clock & Fuses (`9.6 MHz`):** The sketch requires `F_CPU 9600000UL`. Factory ATtiny13A chips usually ship at `1.2 MHz` (`CKDIV8`), which breaks the 38 kHz carrier timing.
3. **Flash via Arduino IDE (with *MicroCore*):**
   * Board: **ATtiny13** · Clock: **9.6 MHz internal osc.**
   * Run **Tools → Burn Bootloader** once (sets the fuses to 9.6 MHz).
   * Then run **Sketch → Upload Using Programmer**.
4. **Alternative via CLI (`avr-gcc` + `avrdude`, e.g., with USBasp):**
   ```sh
   avr-gcc -mmcu=attiny13a -Os -x c attiny13_tx/Attiny_tx.ino -o attiny_tx.elf
   avr-objcopy -O ihex -R .eeprom attiny_tx.elf attiny_tx.hex
   avrdude -c usbasp -p t13 -U lfuse:w:0x7a:m -U hfuse:w:0xff:m -U flash:w:attiny_tx.hex:i
   ```

---

## 🚀 Getting Started & Usage

### 1. Connect to the Lap Timer

The base station uses a **Wi-Fi cascade**: *Home Wi-Fi → Mobile Hotspot → Built-in SoftAP*.

* **Direct AP mode:** Connect to `RC-LapTimer-XXXX` (default password: `Rennstrecke123`) and open `http://192.168.0.8` (or the learned home IP) in your browser.
* **Home Wi-Fi / Hotspot:** When connected to your home network or hotspot, the base station briefly broadcasts a helper Wi-Fi named `LT_<SSID>-<IP>` at boot so you can read its IP address right away (it turns off automatically after the first web request).

### 2. Web Interfaces

* `/` or `/index.html` – **Driver View** (live timing, OLED dimmer dashboard, personal Ghosthunt, TTS filter)
* `/admin.html` – **Race Director View** (tracks, transponder mapping, race modes, arcade audio jukebox, QR code, Wi-Fi control center)
* `/update.html` – **System Maintenance** (LittleFS file manager, ZIP backup/restore, firmware OTA)
* `/debug.html` – **Diagnostics** (heap, loop times, ESP-NOW packets, asset check, audio/TTS test console)

---

## 📚 Further Documentation

* [**User Manual (EN)**](BEDIENUNGSANLEITUNG.en.md) · [**Bedienungsanleitung (DE)**](BEDIENUNGSANLEITUNG.md) – Step-by-step guide for drivers and race directors
* [**ROADMAP.md**](ROADMAP.md) – System architecture, signal processing, protocols, and module overview *(German)*
* [**LT_Base/OfflineStats.md**](LT_Base/OfflineStats.md) – Documentation for the standalone offline statistics app *(German)*
* [**LT_SectorStation/Sektorstationen (SZ).md**](LT_SectorStation/Sektorstationen%20(SZ).md) – ESP-NOW radio protocol, relay logic, and 4-stage OTA rescue *(German)*
* [**SpeedLinkV2-interface.md**](SpeedLinkV2-interface.md) – Planned ESP-NOW telemetry interface with SpeedLinkV2 *(German)*
* [**LT_Base/GEMINI.md**](LT_Base/GEMINI.md) & [**LT_Base/CONVENTIONS.md**](LT_Base/CONVENTIONS.md) – Internal development & embedded engineering guidelines *(German)*

---

## 📄 License

This project is licensed under the **MIT License**.
