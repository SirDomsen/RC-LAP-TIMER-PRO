## ESP-01S Sektorstationen (SZ1–SZ3)

### Hardware & Pins
* **Pin 1 (TX):** Im `setup()` als `OUTPUT` hart auf `LOW` geschaltet (Massebezug für Spannungsteiler / Schaltung).
* **GPIO 0:** `INPUT_PULLUP` zur Batterieüberwachung (HIGH = Akku leer).
* **Serielle Schnittstelle:** Komplett deaktiviert (kein `Serial`), verhindert Boot-Blockaden und schont Pegel.

### Funk & Protokoll (ESP-NOW)
* **Kanal & Sync:** Automatische Kanalsuche (1–13) alle 1,2 s bei Timeout; Sync auf Basis-Beacon (Stratum-Kaskade).
* **Heartbeat:** 3-Byte `AlivePacket` (`stationId`, `batteryLow`, `isUnicast`) exakt alle 3 Sekunden.
* **Transponder:** IR-Erkennung mit 40 ms LED-Puls und Weiterleitung via Unicast/Broadcast (TTL = 3).

### Wartungsmodus & OTA (3-Stufen-Failsafe)
1. **Stufe 1 (Basis-Vorgabe):** Übernahme von SSID/Passwort direkt aus dem `OtaTriggerPacket` (statische IP nur bei Heimnetz-SSID, sonst DHCP).
2. **Stufe 2 (Heimnetz):** Fallback auf `secrets.h` mit fester statischer IP (`192.168.0.10 + STATION_ID`).
3. **Stufe 3 (Rettungs-AP):** Autarker Hotspot `RescueSZ<ChipId>` auf `192.168.4.1` (Passwort: `12345678`), falls keine Verbindung gelingt.
* **Kein Timeout:** Verbleibt im Wartungsmodus (schnelles Blinken) bis zum Upload oder Kaltstart.