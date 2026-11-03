# RC-LAP-TIMER-PRO ↔ SpeedLinkV2 Interface

## Zweck und Geltungsbereich

Dieses Dokument hält den geplanten Datenaustausch zwischen der Basisstation von
RC-LAP-TIMER-PRO und SpeedLinkV2 fest. Es beschreibt den abgestimmten
Architekturrahmen, noch keine fertige oder implementierte ESP-NOW-Paketspezifikation.

Die Projekte bleiben eigenständige Firmware-Projekte. Gemeinsame Protokolle
werden dokumentiert und versioniert; plattformspezifischer Funk- und
Hardwarecode bleibt jeweils im zugehörigen Projekt.

## Bereits vorhanden: IR-Transponder

SpeedLinkV2 kann bereits das IR-Format senden, das die Basisstation von
RC-LAP-TIMER-PRO dekodiert. Damit kann die Basis das Fahrzeug anhand seiner
Car-ID erkennen.

Das derzeitige Format verwendet einen 12-Bit-Rahmen aus 6 Bit Car-ID und
6 Bit invertierter Car-ID. Das IR-Format ist vom nachfolgend geplanten
ESP-NOW-Telemetrieaustausch getrennt.

## Geplanter Ablauf für Zieldurchfahrten

1. Die Basisstation erkennt die Zieldurchfahrt und erfasst deren Zeit.
2. Die Basis sendet ein ESP-NOW-Ereignis, voraussichtlich per Broadcast.
3. Das Ereignis enthält mindestens die Car-ID und den an der Basis erfassten
   Zieldurchfahrtszeitpunkt.
4. SpeedLinkV2 ordnet ein passendes Ereignis seinem Fahrzeug und der lokalen
   Runde zu.
5. SpeedLinkV2 ergänzt die Runde um lokal erfasste Telemetrie, insbesondere
   Fahrstrecke, Beschleunigungswerte, Sprunghöhe und Geschwindigkeiten, und
   speichert die Rundendaten in seiner eigenen Datenbank.

Die Basisstation ist für die Erkennung der Zieldurchfahrt und deren Zeit die
maßgebliche Quelle. Der Zeitstempel bezeichnet den Erfassungszeitpunkt an der
Basis, nicht den späteren Versandzeitpunkt des Pakets.

## Ereignisidentität und Zeitbasis

Jedes Zieldurchfahrtsereignis soll eine eindeutige Identität erhalten aus:

- einer **Session-ID**, die die Session kennzeichnet, und
- einer **Eventnummer**, die ein Ereignis innerhalb der Session eindeutig
  identifiziert.

Diese Identität ermöglicht SpeedLinkV2, erneut empfangene Ereignisse zu
erkennen und nicht doppelt zu verbuchen. Die konkrete Vergabe, Breite und
Lebensdauer von Session-ID und Eventnummer sind noch festzulegen.

Die Uhren der Geräte sollen über die bestehenden Beacons der Basis synchron
gehalten werden. Ereigniszeitstempel und Beacon-Zeit müssen dieselbe definierte
Zeitbasis verwenden. Das bestehende Beacon enthält `baseTimeMs`; die konkrete
Abbildung auf den Zeitstempel im Zieldurchfahrtsereignis ist noch zu
spezifizieren.

## Zustellung und Empfängerverhalten

ESP-NOW-Broadcast ist für den ersten Ausbau vorgesehen, aber die Zustellung
eines Broadcast-Pakets wird nicht als garantiert vorausgesetzt. Für die erste
Protokollversion sind weder Bestätigungen noch Wiederholungen vereinbart.

SpeedLinkV2 soll:

- Ereignisse anhand Session-ID und Eventnummer deduplizieren,
- nur für das eigene Fahrzeug relevante Car-IDs einer Runde zuordnen und
- erkannte Ereignislücken soweit möglich feststellen oder protokollieren.

Ob Ereignisse später bestätigt oder wiederholt werden müssen, bleibt offen und
kann anhand von Praxistests entschieden werden.

## Spätere Statistikübernahme

Ein späterer Import von SpeedLinkV2-Rundendaten in die
RC-LAP-TIMER-PRO-Basisstatistik ist vorgesehen, aber nicht Teil des ersten
ESP-NOW-Ausbaus. Das Importformat, die Verknüpfung mit Laptimer-Sessions und
die Behandlung fehlender oder nachträglich importierter Daten sind noch
festzulegen.

Die stabile Ereignisidentität und ein klar definierter Zeitstempel sollen
diese spätere Verknüpfung ermöglichen, ohne die Projekte jetzt an ein
gemeinsames Datenbankschema zu koppeln.

## Noch zu spezifizieren

- Binäres ESP-NOW-Paketformat einschließlich Nachrichtentyp und
  Protokollversion
- Feldbreiten, Byte-Reihenfolge und Wertebereiche
- Vergabe und Reset-Regeln für Session-ID und Eventnummer
- Exakte Zeitbasis, Auflösung und Überlaufverhalten
- Umgang mit Neustarts oder Session-Wechseln während einer Session
- Ob und wie verlorene Ereignisse erkannt oder nachgeliefert werden
- Format und Ablauf des späteren Statistikimports

