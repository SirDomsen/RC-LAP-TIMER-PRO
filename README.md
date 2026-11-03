# RC Lap Timer Pro 🏎️⏱️

A standalone, Wi-Fi-based infrared lap timing system for RC cars. The repository contains separate PlatformIO projects for the base station and sector stations, plus the ATtiny13 transmitter reference.

## Projects

| Project | Hardware | Description |
| --- | --- | --- |
| `LT_Base/` | ESP8266 NodeMCU V2 or ESP32-S3 DevKitC-1 | Base firmware, shared race logic, LittleFS web app and common `LTCommon` library |
| `LT_SectorStation/` | ESP-01S (SZ1–SZ3) | Sector station firmware; shares `LTCommon` with the base |
| `attiny13_tx/` | ATtiny13A | IR transponder transmitter reference |

The base project's `platformio.ini` provides the `basisstation` and `basisstation_s3` environments. Sector station environments `sz1`, `sz2` and `sz3` are defined separately in `LT_SectorStation/platformio.ini`.

## Features

* Infrared transponder detection and live lap timing
* Practice, qualifying, lap/time races, drag race and Ghost Battle
* Sector timing over ESP-NOW with synchronized sector stations
* Browser-based dashboard, race control, statistics and OTA updates
* LittleFS storage for tracks, settings and session history
* Arcade sound effects and optional hardware speaker output

## Hardware

* **Base:** ESP8266 NodeMCU V2 or ESP32-S3 DevKitC-1
* **Sector stations:** ESP-01S modules with TSOP4838 IR receivers
* **Transponder:** ATtiny13A with a 38 kHz IR LED

See [LT_Base/GEMINI.md](LT_Base/GEMINI.md) for the current pin assignments and architecture notes.

## Setup and builds

Install PlatformIO Core or the VS Code PlatformIO extension.

Create `LT_Base/src/common/secrets.h` with optional home-network credentials:

```cpp
#define HOME_SSID "YourWifiName"
#define HOME_PASS "YourWifiPassword"
```

For sector-station fallback access to the same home network, create `LT_SectorStation/src/secrets.h` with those credentials. Both files are local-only and ignored by Git. The firmware also has defaults and can be built without these files.

From the repository root, build a base target:

```sh
pio run -d LT_Base -e basisstation
pio run -d LT_Base -e basisstation_s3
```

Build the web filesystem image for the base:

```sh
pio run -d LT_Base -e basisstation -t buildfs
```

Build a sector station:

```sh
pio run -d LT_SectorStation -e sz1
```

The configured environments support OTA uploads when the device is reachable at its configured address. Connect over USB and use PlatformIO's normal upload target when performing a serial flash.

## Usage

Connect to the timer's Wi-Fi network or open its configured address in a browser. Configure drivers, cars and timing options in the dashboard, then start a session.

## License

This project is licensed under the MIT License.
