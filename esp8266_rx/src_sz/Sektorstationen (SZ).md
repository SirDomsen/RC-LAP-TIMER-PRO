## ESP-01S Sektorstationen (SZ1–SZ3)

### Hardware & Pins
* **Pin 1 (TX):** Im `setup()` als `OUTPUT` hart auf `LOW` geschaltet (Massebezug für Spannungsteiler / Schaltung).
* **GPIO 0:** `INPUT_PULLUP` zur Batterieüberwachung (HIGH = Akku leer).
* **Serielle Schnittstelle:** Komplett deaktiviert (kein `Serial`), verhindert Boot-Blockaden und schont Pegel.

### Funk & Protokoll (ESP-NOW)
* **Kanal & Sync:** Automatische Kanalsuche (1–13) alle 1,2 s bei Timeout; Sync auf Basis-Beacon (Stratum-Kaskade).
* **Dynamische Selbstheilung (Relay-Logik):** 
  * Bei direktem Empfang von Stratum-0-Beacons der Basis: Schneller **Unicast-Modus** direkt an die Basis-MAC mit Hardware-ACK.
  * **2,5s Basis-Lease:** Bleibt das Stratum-0-Beacon für >2,5 s aus, fällt die Station vollautomatisch auf **Broadcast** zurück. Relais-Stationen fangen die Pakete auf und leiten sie weiter.
  * **Sofortiger Hardware-ACK Failover:** Schlägt ein Unicast-Sendeversuch fehl (`sendStatus != 0`), wird das Sektorpaket sofort im nächsten Loop als Broadcast hinterhergeschickt.
* **Heartbeat:** 3-Byte `AlivePacket` (`stationId`, `batteryLow`, `isUnicast`) exakt alle 3 Sekunden.
* **Transponder:** IR-Erkennung mit 40 ms LED-Puls und Weiterleitung via Unicast/Broadcast (TTL = 3).

### Wartungsmodus & OTA (4-Stufen-Failsafe, 100% DHCP)
1. **Stufe 1 (Basis-Vorgabe):** Übernahme von SSID/Passwort direkt aus dem `OtaTriggerPacket` (reines DHCP, keine Subnetz-Kollisionen).
2. **Stufe 2 (Heimnetz):** Fallback auf `secrets.h` (`HOME_SSID` / `HOME_PASS` per DHCP).
3. **Stufe 3 (Basis-AP Scan):** Dynamischer WLAN-Scan nach Basis-Hotspots mit dem Präfix `RC-LapTimer*` (Passwort: `12345678`, DHCP). Findet jede Basisstation unabhängig von deren individueller Hardware-ID.
4. **Stufe 4 (Autarker Rettungs-AP):** Autarker Notfall-Hotspot `RescueSZ<ChipId>` auf `192.168.4.1` (Passwort: `12345678`), falls absolut keine Verbindung zustande kommt.
* **Kein Timeout:** Verbleibt im Wartungsmodus (schnelles Blinken) bis zum Upload oder Kaltstart.
