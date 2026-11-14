# 🚫 ESP8266 Do-Nots & Post-Mortem Leitfaden (`LT_Base`)

Dieses Dokument hält hart erarbeitete Grenzen des **ESP8266** im Vergleich zum **ESP32-S3** fest, damit wiederkehrende Watchdog-/Exception-Crashes dauerhaft vermieden und neue Bugs systematisch eingekreist werden können.

---

## 1. Das harte 4-KB-Stack-Limit (`CONT_STACKSIZE = 4096`)

Auf dem ESP8266 laufen `setup()` und `loop()` auf dem fest **4096 Bytes** großen `cont`-Stack (`cont.h`).
Direkt unterhalb von `g_pcont->stack[0]` (Adresse `0x3FFFED40`) liegen im `cont_t` die Continuation-Register (`sp_suspend`, `pc_suspend`, `sp_ret`, `pc_ret`) und darunter der `sys`-Stack.

**Was passiert bei Überlauf?**
- Überlebt `loop()` bis zum Rücksprung in `loop_wrapper()`, greift `cont_check(g_pcont)`: **`Exception (5): Stack smashing detected.`**.
- Passiert der Überlauf mitten in einem Flash-/Wi-Fi-/mDNS-`yield()` (`optimistic_yield(10000)` inside `flash_hal_read/write/erase`), überschreibt der Stack `cont_t` oder den `sys`-Stack ➔ **Hardware Watchdog (`Fatal exception:4`, z. B. `cont_ret` oder `SPI_read_status`)** mit Meldung `Caution, the stack is possibly corrupt`.

### ❌ Do-Nots für den Stack
1. **Keine `struct`/`String`/`RecordEntry`-Arrays auf dem Stack in tiefen Pfaden:**
   - Ein einzelnes `RecordEntry[MAX_TRACK_RECORDS]` belegt auf dem Stack schnell **~400 B**, zwei davon (`allTime` + `daily`) **~800+ B**.
   - Temporäre Record-Arrays oder größere `String[]`-Listen gehören per `new (std::nothrow)` (`std::unique_ptr` mit OOM-Guard) oder über `PlatformRuntime::allocLargeBuffer()` auf den Heap und danach sofort wieder freigegeben.
2. **Kein blindes Vertrauen in `if/else`-Zweige gegen Stackverbrauch (GCC-Falle!):**
   - GCC reserviert den Stackplatz aller lokalen Variablen sämtlicher Zweige (`if ... else ...`) sowie geinlineter Helfer **bereits beim Funktions-Einsprung** auf einmal.
   - **Beispiel aus der Praxis:** `deleteTrackRecordEntry()` reservierte `1216 B` (inkl. der beiden `RecordEntry[5]`-Arrays aus dem inaktiven `else`-Zweig) selbst dann, wenn nur die aktive Strecke aktualisiert und `saveTracksToFS()` aufgerufen wurde!
3. **Dispatcher & mehrstufige State-Machines nicht vom Compiler zusammenfalten lassen:**
   - Selbst wenn `stepX()` „pro `loop()`-Durchlauf nur eine Phase“ ausführt, legt GCC ohne `__attribute__((noinline))` (`STORAGE_NOINLINE` in [storage.cpp](src/common/storage.cpp)) die Stack-Frames mehrerer Zweige in `processStorageQueue()` (`1072 B`) oder `stepLapDeletion()` (`608 B`) übereinander.
   - Große Phasen und selten genutzte Zweige (z. B. Crash-Resume, Rekord-Rewrites, Missing-Track-Import) konsequent als separate `STORAGE_NOINLINE`-Funktionen trennen.
4. **Flash-/LittleFS-Overhead einrechnen (~700–1000 B Reserve einplanen!):**
   - Allein `LittleFS.open(..., "w")` / `write()` / `close()` + `lfs_dir_find` + `lfs_file_relocate` + `flash_hal_*` + `optimistic_yield()` ➔ `run_scheduled_recurrent_functions()` ➔ `MDNSResponder` brauchen unterhalb der Anwendung nochmals **~600–800+ B** (mDNS allein bis zu `608 B` Frame).
   - Regel: Beim Einsprung in `saveTracksToFS()`, `filterSessionsIndexFile()` oder vergleichbare Flash-Writer sollten mindestens **1,5–2 KB `cont`-Stack** frei sein.

---

## 2. Post-Mortem: Crash beim Löschen der schnellsten Runde nach Backup-Restore (behoben in v4.8.5)

### Reproduktionskette (5 Schritte)
1. Statistiken löschen (`POST /api/deleteallstats`)
2. Backup-ZIP über `update.html` einspielen (`/api/upload`)
3. Neustart (`POST /api/restart`)
4. „Index neu erstellen“ (setzt über `/sessions.json` & `/tracks.json` u. a. `pendingTrackSyncFromSessions`)
5. Schnellste Rennrunde auf der MainPage löschen (`POST /api/deletelap`)

### Messbeweise (Test-ESP an `COM3`, `192.168.0.216`)
- ELF-Stack-Analyse (`xtensa-lx106-elf-objdump` auf `basisstation/firmware.elf`) vor dem Fix:
  - `processStorageQueue()`: **1072 B**
  - `stepLapDeletion()`: **608 B**
  - `deleteTrackRecordEntry()`: **1216 B**
  - `saveTracksToFS()`: **336 B**
  - Geschwister-Hotspot in Schritt 4 (`stepImportMissingTrackFromSessions()`): **1200 B** (drückte `ESP.getFreeContStack()` bis auf `336 B`).
- Live-Messung per Linker-Wrapper (`-Wl,--wrap=flash_hal_read/write/erase`) während Schritt 5:
  ```text
  [DIAG] saveTracks before open(tmp) t=16069 heap=12152 freeCont=48 delState=1
  [DIAG] saveTracks done R=3281 W=33 E=1 minSpMargin=0 freeCont=0
  Stack smashing detected.   (Exception 5 in cont_check @ 0x401004ef)
  ```
  Mit nur **48 Bytes** Rest-Stack beim Eintritt in `saveTracksToFS()` reichte der erste LittleFS-/Flash-`yield()`-Zyklus (`lfs_dir_find` bzw. `lfs_file_relocate`), um `0x3FFFED40` zu überlaufen ➔ `Hardware Watchdog` (`cont_ret` / `SPI_read_status`) oder `Stack smashing detected.`.

### Lösung (`v4.8.5`)
- Dispatcher (`processStorageQueue()`), Löschphasen (`stepLapDeletionPhase0..3()`, `stepDriverHistoryDeletionPhase0/OneSession()`), Rekord-Pfade (`deleteActive/InactiveTrackRecordEntry()`) und Track-Import (`findFirstMissingSessionTrack()`, `collectTrackRecordsFromSessions()`, `appendMissingTrackToTracksJson()`) per `STORAGE_NOINLINE` sauber getrennt.
- Temporäre `RecordEntry[MAX_TRACK_RECORDS]`- sowie Fahrer/Auto-`String[]`-Arrays per `new (std::nothrow)` auf den Heap verschoben und gemeinsame Kaskadenlogik in `removeTrackRecordPair()` dedupliziert.
- Ergebnis in `basisstation/firmware.elf`: alle kritischen Delete-/Import-Handler auf **<= 368 B** reduziert (`saveLapToHistory` `704 B`, `stepDriverHistoryDeletionOneSession` `624 B`); die kompletten Repro- und Stresstests (aktiver/inaktiver Streckenrekord & Missing-Track-Sync) auf `192.168.0.216` liefen danach ohne Watchdog-/Stack-Crash durch (`deletelapstatus: 200 ok`).

---

## 2b. Bug-Analyse #2 (Nov 2026): `LittleFS`-Reentrancy-Crash (`lfs_bd_prog`) bei schnellem Multi-File-Upload

### Symptom
Beim schnellen Hochladen vieler Statistik-Dateien über `/api/upload` (z. B. Backup-Restore) stürzte der ESP8266 (`192.168.0.8` und `192.168.0.216`) unmittelbar nach `tracks.json` während weiterer eingehender Dateichunks ab:
```text
Panic lfs.c:241 lfs_bd_prog: Assertion 'pcache->block == ((lfs_block_t)-1)' failed.
ctx: sys
```

### Bewiesene Ursache (`COM3` + `addr2line` auf `v4.8.5`)
1. `handleFileUpload()` in [`web_handlers.cpp`](src/common/web_handlers.cpp) läuft im `ctx: sys` (`AsyncWebServer` / LWIP) und hält beim chunked Upload ein globales/statisches `File uploadFile` auf `LittleFS` geöffnet. Sobald `tracks.json` abgeschlossen war, rief `handleFileUpload()` `scheduleReloadTracksFromFS()` auf (`pendingReloadTracks = true`), während der Client direkt weitere Dateien hinterher schickte.
2. Im `ctx: cont` (`loop()` ➔ `processStorageQueue()`) lief dadurch `loadTracksFromFS()` an. Über Arduino-ESP8266 `File`-/`Stream`-Aufrufe (`optimistic_yield(10000)` bzw. explizite `yield()`-Aufrufe in offenen Datei-Schleifen) wurde `cont_suspend()` zurück an `ctx: sys` ausgelöst, obwohl `ctx: cont` bereits mitten im selben nicht-reentranten `LittleFS`-Objekt (`lfs_t` / gemeinsamer `pcache`) steckte.
3. Im `ctx: sys` öffnete oder beschrieb `handleFileUpload()` dann die nächste Datei via `LittleFSImpl::open()` ➔ `lfs_file_rawopencfg()` ➔ `lfs_dir_commitattr()` ➔ `lfs_bd_prog()` und zerstörte den laufenden `pcache` (`Assertion 'pcache->block == ((lfs_block_t)-1)' failed`). Zusätzlich war `PlatformRuntime::StorageLock` auf ESP8266 bisher ein No-Op.

### Lösung (`v4.8.5`)
- **Kein Context-Switch innerhalb von `StorageLock` (`platform_runtime.cpp` + `platformio.ini`):**
  - `PlatformRuntime::StorageLock` pflegt auf ESP8266 einen Tiefe-Zähler (`s_storageLockDepth`).
  - Über `-Wl,--wrap=optimistic_yield` in [`platformio.ini`](platformio.ini) ruft `__wrap_optimistic_yield()` bei `s_storageLockDepth > 0` nur noch `ESP.wdtFeed()` auf statt an `ctx: sys` abzugeben.
  - Alle verbliebenen expliziten `yield()`-Aufrufe innerhalb offener `File`-Schleifen in [`storage.cpp`](src/common/storage.cpp) wurden durch `PlatformRuntime::feedWatchdog()` ersetzt.
- **Upload-Ruhefenster & Abort-Cleanup (`storage.cpp` + `web_handlers.cpp`):**
  - `notifyHttpUploadActivity(bool uploadOpen)` / `isHttpUploadActive()` halten Hintergrund-FS-Arbeit in `processStorageQueue()` pausiert, solange ein HTTP-Upload offen ist bzw. innerhalb eines 1800-ms-Ruhefensters nach dem letzten Chunk.
  - `handleFileUpload()` verfolgt `activeUploadReq` und registriert `request->onDisconnect(...)`, damit abgebrochene Uploads ihr offenes `uploadFile` sauber schließen und das Flag zurücksetzen.
- **Verifiziert (`192.168.0.216` via `COM3` + `192.168.0.8` OTA mit realen Live-Daten):** Alle 13 schnellen Folge-Uploads direkt nach `sessions.json`/`tracks.json` lieferten auf beiden Geräten `200 Upload erfolgreich!` ohne Panic oder Exception; anschließend lief auf beiden Geräten der komplette 5-Schritt-Repro-/Lösch-/Sync-Test (`deletelapstatus: 200 ok`) ohne jeden Crashlog-Eintrag durch.

---

## 3. Weitere unverhandelbare ESP8266-Regeln

| Bereich | ❌ Do-Not | ✅ Richtig |
|---|---|---|
| **Kontext (`ctx: sys` vs. `ctx: cont`)** | Kein `yield()` / `delay()` in `AsyncWebServer`-Callbacks oder Streaming-Helfern aus Web-Handlern (`streamTracks`, `saveTracksToFS`). Führt dort zu `Panic core_esp8266_main.cpp __yield`. | Im Handler nur Flags/Queues setzen (`schedule...`, `queueFileDeletion`, `queueLapDeletion`); Arbeit häppchenweise in `processStorageQueue()` (`loop()`) erledigen. |
| **LittleFS Reentrancy (`StorageLock`)** | In `ctx: cont` bei offenem `LittleFS` `File` niemals `yield()` oder implizites `optimistic_yield()` nach `ctx: sys` durchlassen, während `ctx: sys` gleichzeitig Upload-/HTTP-Dateien auf `LittleFS` öffnet ➔ `Panic lfs.c:241 lfs_bd_prog`. | FS-Operationen immer mit `PlatformRuntime::StorageLock` (`__wrap_optimistic_yield` unterdrückt `cont_suspend` und füttert WDT), in Schleifen nur `PlatformRuntime::feedWatchdog()` nutzen und Hintergrund-FS-Jobs während aktiver HTTP-Uploads (`isHttpUploadActive()`) pausieren. |
| **ISR / ESP-NOW (`onDataRecv`)** | Keine blockierenden Aufrufe und keine kaskadierten `esp_now_send()`-Calls direkt im Callback. | Nur kompakte Flags setzen und Versand/Verarbeitung im `loop()` ausführen. |
| **HTTP-Streams & JSON** | Keine kompletten großen JSON-Dokumente (`/tracks.json`, `/sessions.json`) oder dynamische `AsyncResponseStream`/`cbuf`-Puffer bei knappem Heap allokieren. | Objektweise streamen (`streamTracks`, 560-Byte-Chunk-Puffer in `/api/laps`) und vor FS-Writes `PlatformRuntime::isHeapCritical(...)` prüfen. |
| **WLAN / PHY** | Niemals auf `WIFI_PHY_MODE_11B` festnageln; im AP-Modus Gateway immer auf die eigene IP setzen. | Mixed/`11G` nutzen; `learnedApIP` für konsistentes Lesezeichen beibehalten. |

---

## 4. Bewährtes Diagnose-Werkzeug für zukünftige ESP8266-Crashes

1. **Stack-Frames im kompilierten ELF prüfen (findet versteckte Inlining-Bomben vor dem Flashen):**
   - Mit `xtensa-lx106-elf-objdump -d LT_Base/.pio/build/basisstation/firmware.elf` pro Funktion das größte `addi a1, a1, -N` / `sub a1, a1, aN` auslesen. Funktionen > `500 B` in `loop()`/`storage`-Ketten sind sofort verdächtig.
2. **Watchdog-Ausgaben genau zuordnen:**
   - Ausnahmen (`Exception 28`, `4`, `5`) immer mit `epc1`–`epc3` und `excvaddr` gegen `xtensa-lx106-elf-addr2line -pfia -e firmware.elf <ADDR>` auflösen.
   - Bei `Hardware Watchdog` (`Fatal exception:4`) mit optionalem `-D DEBUG_ESP_HWDT` bauen – liefert den echten Stacktrace inkl. unterer `a1`-Adresse (Vergleich gegen `&g_pcont->stack[0]`).
3. **Echte Stack-Reserve im Flash-Pfad messen (ohne `Serial.print` im Hook):**
   - Temporär mit `-Wl,--wrap=flash_hal_read/write/erase` den minimalen Abstand `sp - (uintptr_t)&g_pcont->stack[0]` atomar in eine `volatile int32_t` schreiben und erst vor/nach der Operation zusammen mit `ESP.getFreeContStack()` loggen (Diagnose-Flags nach Abschluss wieder restlos entfernen!).
4. **Hardware-Gegenprobe:**
   - Auf einem zweiten Test-ESP via serieller Schnittstelle (`dtr = False, rts = False`, um versehentliche Resets beim Öffnen zu vermeiden) automatisiert in Schleife gegen `/api/...` testen, bevor Produktivgeräte angefasst werden.

---

## 5. Bekannte verbleibende Ansatzpunkte (Audit-Stand `v4.8.5`)

Falls im Rennbetrieb oder bei zukünftigen Erweiterungen erneut ein Watchdog (`Hardware Watchdog` / Software-WDT) oder ein `LittleFS`-Panic auftreten sollte, zuerst diese bereits identifizierten Stellen prüfen:

### A) Reentrancy / offene `LittleFS`-Handles zwischen `ctx: sys` und `ctx: cont`
1. **`isHttpUploadActive()` gilt bisher nur für `processStorageQueue()`, nicht für andere `ctx: sys`-Handler:**
   - Mutierende Web-Handler in [`web_handlers.cpp`](src/common/web_handlers.cpp) und [`web_server.cpp`](src/common/web_server.cpp) greifen direkt im HTTP-Callback synchron auf `LittleFS` zu (`handlePostDeleteAllStats` ➔ `deleteAllHistory()`, `handlePostDeleteSession` ➔ `deleteSessionById()`, `handlePostSelectTrack`, `handlePostAddTrack`, `handlePostDeleteTrackRecord`, `handleGetTracks`-Fallback `saveTracksToFS()`, `handlePostReset` ➔ `saveDailyBest()`, `handlePostNames` ➔ `saveNamesToEEPROM()`, `saveWifiConfigToFS()`, `/api/debug/crashlog/clear`, `flushCurrentSessionToIndex()` aus `GET /api/sessions` / `/api/sessionfile` / `/api/files`).
   - Läuft parallel zu einem offenen `/api/upload` (`uploadFile` offen) einer dieser Endpunkte, treffen beide in `ctx: sys` auf dasselbe `LittleFS`.
   - *Ansatz bei Bedarf:* Mutierende HTTP-Endpunkte während `isHttpUploadActive()` mit `503` abweisen (oder über `processStorageQueue()` serialisieren).
2. **`/api/files` und `AsyncFileResponse` halten Handles über TCP-Chunks hinweg offen:**
   - `/api/files` ([`web_server.cpp`](src/common/web_server.cpp#L476-L587)) hält `st->dirs[]` (`LittleFS.openDir`) über mehrere Chunks offen, während dazwischen `ctx: cont` (`processStorageQueue()`) laufen und auf `LittleFS` schreiben/löschen kann.
   - Ebenso halten `request->beginResponse(LittleFS, ...)` / `request->send(LittleFS, ...)` (`/api/sessions`, `/api/sessionfile`, `/api/tracks`, `/api/download`, Static Files) ihr Lese-`File` über mehrere TCP-Pakete offen, während im Rennbetrieb gleichzeitig `saveLapToHistory()` oder `flushCurrentSessionToIndex()` (`"r+"`) in `loop()` schreibt.
   - *Ansatz bei Bedarf:* Aktives Chunked-/File-Streaming an `processStorageQueue()` melden und Schreibjobs solange kurz zurückstellen.
3. **Explizites `yield()` unter `StorageLock` in `deleteLapFromSession()`:**
   - In [`storage.cpp`](src/common/storage.cpp#L3149-L3157) steht noch `yield()` innerhalb von `StorageLock`. `-Wl,--wrap=optimistic_yield` fängt nur `optimistic_yield()` ab, **nicht** direktes `yield()`.
   - Die Funktion ist aktuell ungenutzt (HTTP nutzt `queueLapDeletion()`), wäre bei späterer Nutzung aber sofort eine Reentrancy-Falle ➔ bei Bedarf auf `PlatformRuntime::feedWatchdog()` umstellen oder entfernen.

### B) Synchron-blockierende FS-Läufe im Web-Callback & größere Stack-Frames
1. **`deleteAllHistory()` synchron in `ctx: sys`** ([`storage.cpp`](src/common/storage.cpp#L3493-L3583)):
   - Löscht alle Streckenordner rekursiv (`removeDirectoryRecursive` ohne `yield()`), `/sessions.json`, `/daily_best.json` und schreibt `/tracks.json` via `streamTracks()` in einem einzigen `AsyncWebServer`-Callback. Bei sehr vielen Sessions kann das an den Software-WDT (~3,2 s) laufen ➔ bei Bedarf in `processStorageQueue()` verlagern.
2. **`filterSessionsIndexFile()` byteweise in `ctx: sys`** ([`storage.cpp`](src/common/storage.cpp#L1740-L1859)):
   - Wird von `deleteSessionById()` (`/api/deletesession`) synchron im HTTP-Callback genutzt, liest `/sessions.json` zeichenweise (`fIn.read()`) mit `String currentObj` ohne `PlatformRuntime::feedWatchdog()` ➔ bei großen `/sessions.json`-Dateien in einen gepufferten Block-Read bzw. in die Queue verlagern.
3. **Alte ungenutzte Synchron-APIs & größte verbleibende Stack-Frames:**
   - `deleteDriverHistory()` ([`storage.cpp`](src/common/storage.cpp#L3374-L3491)) mit `String[MAX_CARS + 1]` auf dem Stack und `deleteTrackById()` ([`storage.cpp`](src/common/storage.cpp#L1861)) sind derzeit ungenutzt (Queue-Varianten aktiv), sollten aber nicht reaktiviert werden.
   - Im produktiven `loop()`-Pfad sind die größten eigenen Stack-Frames (`firmware.elf`, `v4.8.5`): `saveLapToHistory()` (**704 B**) und `stepDriverHistoryDeletionOneSession()` (**624 B**) — aktuell ausreichend Reserve (~2 KB frei), bei neuen lokalen Variablen oder tiefen Subcalls aber zuerst per `objdump` prüfen.
