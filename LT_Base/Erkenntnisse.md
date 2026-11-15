# 🛠️ Hardware- & Treiber-Erkenntnisse (`LT_Base`)

Dieses Dokument sammelt gelöste Plattform-Stolpersteine (ESP32-S3 & ESP8266) rund um WLAN, ESP-NOW und Feldbetrieb, damit einmal gefundene Ursachen und Gegenmaßnahmen dauerhaft nachvollziehbar bleiben.

---

## 1. ESP32-S3: ESP-NOW Unicast der Sektorstationen im Heimnetz / Hotspot (`WIFI_IF_AP` statt `WIFI_STA`)

### Symptom
- Im reinen AP-Modus verbanden sich alle 3 ESP8266-Sektorstationen (`SZ1`–`SZ3`) sofort im Unicast (`[01 00 01]`, `[02 00 01]`, `[03 00 01]`).
- Im Heim-WLAN (`WIFI_STA`, Router z. B. Kanal 1) meldete die UI direkt nach dem Boot ~7 s lang scheinbar „Broadcast“, danach riss die Verbindung ab; in `debug.html` erschienen regelmäßig Einträge **`Basisstation | Sys (0) | Akku OK`**.
- **Wichtiges A/B-Beobachtungsmuster auf `COM8`:** Solange direkt nach dem Boot noch das Ghost-WLAN aktiv war (`WIFI_AP_STA` mit `WIFI_IF_AP`, `ifidx=1`), kamen alle 3 SZ-Unicasts zuverlässig an. Exakt im Moment des Ghost-WLAN-Shutdowns (`AP_STOP` → Wechsel auf `WIFI_STA` / `ifidx=0`) brachen die 3-Byte-Unicasts sofort ab und es trafen nur noch 9-Byte-Nullpakete ein.

### Root Causes (3 zusammenwirkende Effekte)
1. **ESP32-S3 (`ESP-IDF v4.4.7`) + ESP8266-Unicast im `WIFI_STA`-Modus:**
   - Im reinen `WIFI_STA`-Betrieb (mit aktivem Router-Link auf `WIFI_IF_STA`) empfing der ESP32-S3 zwar **Broadcast**-Frames der SZs (10-Byte-Beacons und 9-Byte-Fallback-Frames), bestätigte eingehende ESP-NOW-**Unicast**-Frames der ESP8266-Stationen (`WIFI_PHY_MODE_11G`) jedoch nicht auf 802.11-MAC-Ebene (`sendStatus != 0` auf der SZ, 0 Byte in `onDataRecv` auf dem S3).
   - Im `WIFI_AP_STA`-Modus über das SoftAP-Interface (`WIFI_IF_AP`, `ifidx=1`) funktioniert derselbe Unicast-Verkehr im selben Heimnetz-Kanal einwandfrei.
2. **Irreführender 9-Byte-Broadcast-Fallback auf der Sektorstation (`LT_SectorStation/src/main.cpp`):**
   - Schlägt ein Unicast-`AlivePacket` fehl (`onDataSent` mit `sendStatus != 0`), setzt die SZ `pendingBroadcastFallback = true` und sendet im `loop()` `lastUnicastPkt` per Broadcast – im Leerlauf eine global genullte `SectorPacket`-Struktur (`9` Bytes `00 00 00 00 00 00 00 00 00`).
   - Im Paketmonitor wird `stationId = 0, carId = 0` als `Basisstation | Sys (0) | Akku OK` dargestellt, obwohl es in Wahrheit ein fehlgeschlagener Alive-Unicast einer SZ ist.
3. **7-Sekunden-Boot-Falle in `isStationOnline()` (`LT_Base/src/common/esp_now_base.cpp`):**
   - `sz1LastSeen..sz3LastSeen` starten mit `0`. Ohne Prüfung `sz*LastSeen > 0` lieferte `millis() - 0 < 7000` in den ersten 7 s nach jedem Boot falsch-positiv `true` („online / Broadcast“), auch wenn noch nie ein Paket empfangen wurde.

### Dauerhafte Lösung (ab `v4.8.6`)
1. **Verstecktes Mini-SoftAP als ESP-NOW-Anker auf dem ESP32-S3:**
   - Nach Ablauf des 15-s-Ghost-WLAN-Timers ruft `web_server.cpp` (`handleNetworkTasks()`) plattformneutral `NetworkPlatform::finishGhostAccessPoint()`.
   - **ESP8266** (`src/platform/esp8266/network_platform.cpp`): Verhalten bleibt unverändert (`stopAccessPoint(true)` + `setStationMode()`).
   - **ESP32-S3** (`src/platform/esp32/network_platform.cpp`): Statt `WIFI_AP_STA` komplett abzubauen, wechselt der S3 auf ein unsichtbares Minimal-SoftAP (`LT_INT_<ChipId>`, `hidden = 1`, `max_connections = 1`, Subnetz `172.31.254.1/28` ohne Konflikt zu `192.168.x.x` / `10.x.x.x`) auf dem aktiven Router-Kanal. Damit bleibt `WIFI_AP_STA` / `WIFI_IF_AP` dauerhaft aktiv, ohne dass ein offenes `LT_...`-WLAN sichtbar bleibt.
2. **Dynamische 3-Slot-Peer-LRU im ESP32-Transport (`src/platform/esp32/esp_now_transport.cpp`):**
   - Bevorzugt `WIFI_IF_AP` sobald `WIFI_MODE_AP` aktiv ist (sonst `WIFI_IF_STA`) und synchronisiert Broadcast- + dynamische Unicast-Peers in `EspNowTransport::maintain()` nach jedem Interface-Wechsel.
   - Merkt sich dynamisch die zuletzt aktiven **3** SZ-Unicast-MACs (LRU; bei einer neuen/getauschten SZ wird automatisch die älteste ersetzt). 10-Byte-`BeaconSyncPacket`-Frames werden dabei ignoriert, damit fremde ESPs/Beacons im Umfeld keine SZ aus den 3 Slots verdrängen.
3. **Boot-Check in `isStationOnline()` (`src/common/esp_now_base.cpp`):**
   - Zählt eine Sektorstation erst dann als online, wenn `sz*LastSeen > 0` ist.
