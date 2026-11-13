(function() {
  const dict = {
    // --- Allgemeine UI & Rollen ---
    "Zurück": "Back",
    "Schließen": "Close",
    "SCHLIESSEN": "CLOSE",
    "ABBRECHEN": "CANCEL",
    "STARTEN": "START",
    "BESTÄTIGEN": "CONFIRM",
    "OK": "OK",
    "Lade...": "Loading...",
    "⏳ Lade...": "⏳ Loading...",
    "Unbekannt": "Unknown",
    "Fahrer": "Driver",
    "Auto": "Car",
    "Fahrzeug": "Car",
    "Fahrzeug unbekannt": "Unknown car",
    "Alle Fahrzeuge": "All cars",
    "Alle Fahrer": "All drivers",
    "Alle Strecken": "All tracks",
    "von": "by",
    "Ziel": "Finish",
    "ZIEL": "FINISH",
    "Platz": "Place",
    "WLAN:": "Wi-Fi:",
    "Speicher:": "Storage:",
    "VOLL! Keine Aufzeichnung!": "FULL! Recording stopped!",
    "Fast voll!": "Almost full!",
    "🛑 Speicher voll ({pct}%)! Sessions werden nicht mehr gespeichert. Bitte erst bestehende Sessions löschen.": "🛑 Storage full ({pct}%)! Sessions are no longer saved. Please delete existing sessions first.",

    // --- Rollen-Badges & Navigation ---
    "🏎️ Aktueller Modus: Fahrer. Zu Rennleiter wechseln 👑": "🏎️ Current mode: Driver. Switch to Race Director 👑",
    "Klicken, um zum Rennleiter-Bereich zu wechseln": "Click to switch to Race Director view",
    "👑 Aktueller Modus: Rennleiter. Zu Fahrer wechseln 🏎️": "👑 Current mode: Race Director. Switch to Driver view 🏎️",
    "Zurück zur Fahrer-Ansicht wechseln": "Switch back to Driver view",
    "Dashboard schließen": "Close dashboard",
    "Top 5 Bestenliste öffnen": "Open Top 5 leaderboard",
    "Klicken für Geister-Einstellungen": "Click for Ghost settings",
    "← Zurück zum Rennleiter": "← Back to Race Director",
    "← Zurück": "← Back",

    // --- Status-Bar & Live-Tabelle ---
    "Modus": "Mode",
    "Restzeit": "Time Left",
    "Führende Runde": "Leading Lap",
    "🏎️ Schnellste Runde": "🏎️ Fastest Lap",
    "Status": "Status",
    "Sektorstationen": "Sector Stations",
    "Sektorstationen:": "Sector Stations:",
    "Website Status": "Website Status",
    "Verbindung zur Basisstation": "Connection to base station",
    "📋 Top 5 ▾": "📋 Top 5 ▾",
    "Klicken für Top 5 Bestenliste": "Click for Top 5 leaderboard",
    "🏆 STRECKENREKORD:": "🏆 TRACK RECORD:",
    "⏱️ TAGESBESTZEIT:": "⏱️ DAILY BEST:",
    "👑 QUALIFYING BEENDET – POLE POSITION 👑": "👑 QUALIFYING FINISHED – POLE POSITION 👑",
    "👑 GEIST GESCHLAGEN – NEUE BESTZEIT! 👑": "👑 GHOST BEATEN – NEW BEST LAP! 👑",
    "DER GEIST (ZIEL) ⚙️": "THE GHOST (TARGET) ⚙️",
    "Klicken, um Geist-Ziel zu wählen / ändern": "Click to select / change ghost target",
    "Strecke:": "Track:",
    "Strecke": "Track",
    "Aktive Strecke:": "Active Track:",
    "Runde": "Lap",
    "Letzte Zeit": "Last Lap",
    "Beste Zeit": "Best Lap",
    "Abstand": "Gap",
    "Pos": "Pos",
    "Fahrer / Auto": "Driver / Car",
    "Warte auf Transponder-Signal...": "Waiting for transponder signal...",
    "Tagesbestzeit:": "Daily Best:",
    "Streckenrekord:": "Track Record:",
    "Noch kein Streckenrekord": "No track record yet",
    "Noch keine Bestzeit heute": "No daily best yet",
    "Kein Streckenrekord": "No track record",
    "Keine Bestzeit heute": "No daily best today",
    "QUALI BEENDET": "QUALI FINISHED",
    "LETZTE RUNDE": "FINAL LAP",

    // --- Debug & Update Seiten ---
    "⚠️ TAB MINIMIERT - POLLING & AUFZEICHNUNG PAUSIERT ZUR ESP-ENTLASTUNG": "⚠️ TAB MINIMIZED - POLLING & RECORDING PAUSED TO SAVE ESP RESOURCES",
    "ESP8266 Systemlast": "ESP8266 System Load",
    "Metriken abfragen (3s)": "Poll metrics (3s)",
    "Laufzeit": "Uptime",
    "CPU Takt": "CPU Clock",
    "Freier Heap": "Free Heap",
    "Fragmentierung": "Fragmentation",
    "Max. Loop-Dauer": "Max. Loop Time",
    "Debug-Modus ist deaktiviert.": "Debug mode is disabled.",
    "🚀 LITTLEFS DATEI- & SPEICHER-MANAGER": "🚀 LITTLEFS FILE & STORAGE MANAGER",
    "LittleFS Flash-Speicher": "LittleFS Flash Storage",

    // --- Rennmodi ---
    "Freies Training": "Free Practice",
    "FREIES TRAINING": "FREE PRACTICE",
    "TRAINING": "PRACTICE",
    "Qualifying (Beendet)": "Qualifying (Finished)",
    "QUALIFYING (BEENDET)": "QUALIFYING (FINISHED)",
    "Rennen (Runden)": "Race (Laps)",
    "RENNEN (RUNDEN)": "RACE (LAPS)",
    "RENNEN ({cur}/{tot} RND)": "RACE ({cur}/{tot} LAPS)",
    "Rennen (Zeit)": "Race (Time)",
    "RENNEN (ZEIT)": "RACE (TIME)",
    "Freies Training gestartet": "Free practice started",
    "eine Minute": "one minute",
    "Minuten": "minutes",
    "Qualifying gestartet für {dur}!": "Qualifying started for {dur}!",
    "Dragrace gestartet! Zeiterfassung startet bei Überfahrt der Basis.": "Drag race started! Timing begins when crossing the base station.",
    "Durchgang gestoppt. Freies Training.": "Session stopped. Free practice.",

    // --- Buttons in Fahrer & Admin Ansicht ---
    "📱 Dashboard": "📱 Dashboard",
    "📊 Statistiken": "📊 Statistics",
    "🔊 Ton: AN": "🔊 Audio: ON",
    "🔇 Ton: AUS": "🔇 Audio: OFF",
    "Sprachausgabe aktiviert": "Voice output enabled",
    "🔊 Mein Auto ansagen": "🔊 Announce my car",
    "📱 iOS Ton": "📱 iOS Audio",
    "✅ iOS Ton Aktiv": "✅ iOS Audio Active",
    "📱 QR-Code": "📱 QR Code",
    "👻 Ghosthunt": "👻 Ghosthunt",
    "👻 Ghost Battle": "👻 Ghost Battle",
    "⚙️ Einstellungen": "⚙️ Settings",
    "📼 Arcade Audio": "📼 Arcade Audio",
    "🛠️ Web-Update": "🛠️ Web Update",
    "🩺 Diagnose": "🩺 Diagnostics",
    "🏁 Freies Training": "🏁 Free Practice",
    "⏱️ Start Quali": "⏱️ Start Quali",
    "🏎️ Start Runden": "🏎️ Start Laps",
    "⏳ Start Zeit": "⏳ Start Time",
    "🚀 Start Dragrace": "🚀 Start Drag Race",
    "⏹️ Durchgang Stoppen": "⏹️ Stop Session",
    "🔄 Komplett-Reset": "🔄 Full Reset",
    "➕ Neue Strecke": "➕ New Track",
    "💾 Speichern & Schließen": "💾 Save & Close",

    // --- Ghosthunt & Ghost Battle ---
    "Geist": "Ghost",
    "Streckenrekord": "Track Record",
    "Tagesbestzeit": "Daily Best",
    "Persönliche Bestzeit": "Personal Best",
    "Zielzeit": "Target Time",
    "Session-Führender": "Session Leader",
    "Fallback: Eigene PB": "Fallback: Personal Best",
    "mit Sektoren": "with sectors",
    "Start/Ziel": "Start/Finish",
    "Noch kein Streckenrekord (erste Runde setzt Geist)": "No track record yet (first lap sets ghost)",
    "Noch keine Tagesbestzeit": "No daily best yet",
    "Deine Bestzeit:": "Your Best Lap:",
    "Eigener Geist:": "Personal Ghost:",
    "Erste gezeitete Runde dieser Session setzt deinen Geist!": "First timed lap of this session sets your ghost!",
    "Freie Zielzeit:": "Custom Target Time:",
    "Gib unten deine Wunsch-Rundenzeit in Sekunden ein.": "Enter your desired lap time in seconds below.",
    "Session-Führender:": "Session Leader:",
    "Noch keine gezeitete Runde in dieser Session!": "No timed lap in this session yet!",
    "↩️ Admin-Geist übernehmen": "↩️ Use Admin Ghost",
    "⏹️ Ghosthunt beenden": "⏹️ Stop Ghosthunt",
    "👻 Ghosthunt starten": "👻 Start Ghosthunt",
    "🚀 Ghost Battle für alle starten": "🚀 Start Ghost Battle for all",
    "🔄 Geist Aktualisieren": "🔄 Update Ghost",
    "Bitte eine gültige Zielzeit in Sekunden eingeben (z. B. 12.5)!": "Please enter a valid target time in seconds (e.g. 12.5)!",
    "Ghosthunt aktiviert! Schnapp dir den Geist!": "Ghosthunt activated! Catch the ghost!",
    "Ghosthunt gegen den Streckenrekord!": "Ghosthunt against the track record!",
    "Ghosthunt gegen die Tagesbestzeit!": "Ghosthunt against today's best!",
    "Ghosthunt gegen deine persönliche Bestzeit!": "Ghosthunt against your personal best!",
    "Ghosthunt aktiviert! Deine erste Runde setzt den Geist!": "Ghosthunt activated! Your first lap sets the ghost!",
    "Ghosthunt gegen Zielzeit!": "Ghosthunt against target time!",
    "Ghosthunt gegen {driver}!": "Ghosthunt against {driver}!",
    "Noch keine Runden in dieser Session! Erste Runde setzt den Geist.": "No laps in this session yet! First lap sets the ghost.",
    "Zurück zum Session-Geist.": "Back to session ghost.",
    "Ghosthunt beendet.": "Ghosthunt ended.",
    "Ghost Battle gestartet!": "Ghost Battle started!",
    "Ghost Battle beendet.": "Ghost Battle ended.",
    "Geist geschlagen! Neue Bestzeit!": "Ghost beaten! New best lap!",
    "{driver} hat den Geist geschlagen!": "{driver} beat the ghost!",
    "Geist gesetzt: {sec} Sekunden! Schnapp ihn dir!": "Ghost set: {sec} seconds! Go catch it!",

    // --- TTS Ansagen ---
    "Achtung, gleich geht's los!": "Drivers ready, starting soon!",
    "Los!": "Go!",
    "Das Rennen ist beendet!": "The race has finished!",
    "Ziel! Das Rennen ist beendet! Herzlichen Glückwunsch an {driver} auf {car} zum Sieg!": "Finish! The race has ended! Congratulations to {driver} driving {car} on the victory!",
    "Platz 2 geht an {p2}, und Platz 3 an {p3}!": "Second place goes to {p2}, and third place to {p3}!",
    "Platz 2 geht an {p2}!": "Second place goes to {p2}!",
    "Auf den weiteren Plätzen:": "Followed by:",
    "Platz {pos} {driver}.": "Place {pos} {driver}.",
    "{driver} gestartet!": "{driver} started!",
    "{driver}, neue absolute Bestzeit! {time} Sekunden!": "{driver}, new overall best time! {time} seconds!",
    "{driver}, persönliche Bestzeit! {time} Sekunden!": "{driver}, personal best! {time} seconds!",
    "{driver}, Ziel! {time} Sekunden!": "{driver}, finish! {time} seconds!",
    "{driver}, absolute Bestzeit Sektor {sector}!": "{driver}, overall best sector {sector}!",
    "{driver}, Bestzeit Sektor {sector}!": "{driver}, best sector {sector}!",
    "Noch 5 Runden!": "5 laps remaining!",
    "Noch 3 Runden!": "3 laps remaining!",
    "Letzte Runde!": "Final lap!",
    "Neuer Führender! {driver} übernimmt die Spitze!": "New leader! {driver} takes the lead!",
    "{driver}, Outlap beendet!": "{driver}, outlap complete!",
    "du bist der Schnellste!": "you are the fastest!",
    "du hast derzeit die {pos}. schnellste Zeit,": "you currently have the {pos}. fastest time,",
    "schnellste Rennrunde!": "fastest race lap!",
    "schnellste Runde!": "fastest lap!",
    "neuer Streckenrekord!": "new track record!",
    "neue Tagesbestzeit!": "new daily best!",
    "persönliche Bestzeit!": "personal best!",
    "Noch eine Minute!": "One minute remaining!",
    "Noch 30 Sekunden!": "30 seconds remaining!",
    "Noch 10 Sekunden!": "10 seconds remaining!",
    "Die Zeit ist abgelaufen! Letzte Runde!": "Time is up! Final lap!",
    "Qualifying beendet! Herzlichen Glückwunsch {driver} auf {car} zur Pole Position mit {time}!": "Qualifying finished! Congratulations to {driver} driving {car} on pole position with {time}!",
    "Auf Platz 2 folgen {p2} und {p3} auf Platz 3.": "Second place is {p2} and third place is {p3}.",
    "Auf Platz 2 folgt {p2}.": "Second place is {p2}.",
    "Platz {pos} geht an {driver}.": "Place {pos} goes to {driver}.",
    "Das Qualifying ist beendet!": "Qualifying has finished!",
    "Die Zeit ist abgelaufen! Durchgang beendet!": "Time is up! Session finished!",
    "{driver} verlässt die Box": "{driver} leaving the pits",
    "{driver} wieder gestartet": "{driver} restarted",
    "{driver} auf Outlap!": "{driver} on outlap!",
    "{n1} ist aktiver Jäger. {n2} ist Zuhörer.": "{n1} is the active hunter. {n2} is listening.",
    "Sprachausgabe aktiv für {n1} und {n2}": "Voice output active for {n1} and {n2}",
    "Sprachausgabe aktiv für {n1} auf {c1}": "Voice output active for {n1} driving {c1}",
    "Sprachausgabe für alle Fahrzeuge aktiv": "Voice output active for all cars",

    // --- Config & WLAN Modals ---
    "🏠 Heim-WLAN": "🏠 Home Wi-Fi",
    "📱 Hotspot": "📱 Hotspot",
    "📡 AP-Modus": "📡 AP Mode",
    "Heim-WLAN": "Home Wi-Fi",
    "Hotspot": "Hotspot",
    "unbekannt": "unknown",
    "Fallback von": "Fallback from",
    "aktiv": "active",
    "⚠️ SESSION WIRD BEENDET": "⚠️ SESSION WILL END",
    "Aktuelle Session wird beendet und ein neues Freies Training gestartet.": "Current session will end and a new Free Practice will start.",
    "Das laufende Rennen wird beendet!": "The current race will be stopped!",
    "Namen gelten immer für die gesamte Session.": "Names always apply to the entire session.",
    "BEENDEN & NEU STARTEN": "END & RESTART",
    "❌ SPEICHERFEHLER": "❌ SAVE ERROR",
    "Die Änderungen an den Fahrer-/Auto-Zuordnungen konnten nicht vollständig gespeichert werden. Bitte prüfe die Verbindung und kontrolliere die Zuordnungen erneut.": "Changes to driver/car mappings could not be saved completely. Please check your connection and verify the mappings.",
    "⚠️ FEHLENDE EINGABE": "⚠️ MISSING INPUT",
    "Bitte erst eine Hotspot-SSID in den WLAN-Einstellungen eintragen!": "Please enter a Hotspot SSID in the Wi-Fi settings first!",
    "Heimnetzwerk": "HomeNetwork",
    "[IP-Adresse]": "[IP-Address]",
    "<IP aus Geister-WLAN>": "<IP from Helper Wi-Fi>",
    "1. Im Smartphone-WLAN nach Geister-WLAN suchen:": "1. Look for the helper Wi-Fi on your phone:",
    "2. Im Heim-WLAN im Browser öffnen:": "2. Open in browser on your home Wi-Fi:",
    "Der Laptimer verbindet sich mit deinem Heim-WLAN per DHCP. Die vom Router vergebene IP liest du einfach am Ende des Geister-WLAN-Namens ab!": "The Lap Timer connects to your home Wi-Fi via DHCP. Simply read the assigned IP from the end of the helper Wi-Fi name!",
    "💡 <strong>Super praktisch:</strong> Die Box speichert diese Heim-IP automatisch auch für den AP-Modus! Unterwegs an der Rennstrecke öffnest du den Laptimer also unter exakt derselben Adresse.": "💡 <strong>Super handy:</strong> The box saves this home IP for AP mode too! At the track, you open the Lap Timer at the exact same address.",
    "1. Geister-WLAN oder Hotspot-Liste am Handy prüfen:": "1. Check helper Wi-Fi or your phone's hotspot client list:",
    "2. Im Browser aufrufen:": "2. Open in your browser:",
    "Der Laptimer verbindet sich mit deinem Handy-Hotspot. Lies die vergebene IP aus dem Geister-WLAN oder der Hotspot-Übersicht deines Smartphones ab.": "The Lap Timer connects to your phone hotspot. Read the assigned IP from the helper Wi-Fi name or your phone's hotspot client list.",
    "📱 <strong>Tipp:</strong> Sobald dein Handy die Verbindung per Browser aufruft, schaltet sich das Geister-WLAN nach 20 Sekunden stromsparend ab.": "📱 <strong>Tip:</strong> As soon as your phone opens the page, the helper Wi-Fi turns off automatically after 20 seconds.",
    "1. Smartphone direkt mit Laptimer-WLAN verbinden:": "1. Connect your phone directly to the Lap Timer Wi-Fi:",
    "2. Im Browser öffnen (gewohnte Heim-IP):": "2. Open in browser (learned home IP):",
    "Der Laptimer spannt sein eigenes Direkt-WLAN an der Rennstrecke auf (kein Router erforderlich).": "The Lap Timer creates its own direct Wi-Fi at the track (no router needed).",
    "🎯 <strong>Dank IP-Sync:</strong> Dein Handy verbindet sich mit dem Laptimer-WLAN und nutzt dieselbe IP wie daheim ({ip})!": "🎯 <strong>Thanks to IP-Sync:</strong> Your phone connects to the Lap Timer Wi-Fi using the same IP as at home ({ip})!",
    "Sende Trigger-Signal an Sektorstationen...": "Sending trigger signal to sector stations...",
    "⚠️ Fehler:": "⚠️ Error:",
    "✅ Signal erfolgreich gesendet!<br><br><span style='color:var(--text-main);font-size:13px;font-weight:normal;'>Die Stationen blinken nun schnell (3Hz) und verbinden sich mit dem aktuellen Netzwerk.<br>Flash-Vorgang per OTA kann gestartet werden.</span>": "✅ Signal sent!<br><br><span style='color:var(--text-main);font-size:13px;font-weight:normal;'>Stations are now blinking fast (3Hz) and connecting to the current network.<br>You can start OTA flashing now.</span>",
    "❌ Fehler beim Senden:": "❌ Send error:",
    "Passwort verbergen": "Hide password",
    "Passwort anzeigen": "Show password",

    // --- Top 5 & Delete Modals ---
    "🏆 STRECKENREKORD – TOP 5 (ALL-TIME)": "🏆 TRACK RECORD – TOP 5 (ALL-TIME)",
    "Noch kein Streckenrekord auf dieser Strecke": "No track record on this track yet",
    "⏱️ TAGESBESTZEIT – TOP 5 (HEUTE)": "⏱️ DAILY BEST – TOP 5 (TODAY)",
    "Heute noch keine Zeit auf dieser Strecke gefahren": "No lap time recorded on this track today",
    "Pro Fahrer & Fahrzeug wird jeweils die schnellste Zeit geführt.": "Only the fastest lap per driver & car is listed.",
    "Beim Löschen einer Fehlmessung rückt die vorherige Bestzeit des Fahrers automatisch nach.": "Deleting a false reading automatically promotes the driver's previous best time.",
    "⏳ Runde wird gelöscht – bitte Seite nicht neu laden und das Gerät nicht trennen.": "⏳ Deleting lap – please do not reload the page or disconnect power.",
    "Löschen ist während eines laufenden Rennens gesperrt.": "Deleting is locked while a race is running.",
    "Zu wenig freier Speicher im Basisgerät. Bitte später erneut versuchen (z. B. weniger Geräte verbunden). Es wurde nichts verändert.": "Not enough free memory on the base station. Please try again later (e.g. fewer connected clients). Nothing was changed.",
    "Kein Platz im Dateisystem für das sichere Umschreiben der Session (Flash fast voll). Es wurde nichts verändert.": "Not enough filesystem space to safely rewrite the session (flash almost full). Nothing was changed.",
    "Das Basisgerät bearbeitet gerade einen anderen Löschauftrag. Bitte kurz warten und erneut versuchen.": "The base station is currently processing another delete request. Please wait a moment and try again.",
    "Keine Rückmeldung vom Basisgerät. Bitte Zustand prüfen.": "No response from the base station. Please check device status.",
    "Die Runde wurde in der Session-Datei nicht gefunden (evtl. altes Datenformat). Es wurde nichts verändert.": "Lap not found in the session file (possibly legacy format). Nothing was changed.",
    "🔒 GESPERRT": "🔒 LOCKED",
    "⏱️ TAGESBESTZEIT LÖSCHEN": "⏱️ DELETE DAILY BEST",
    "🏆 STRECKENREKORD LÖSCHEN": "🏆 DELETE TRACK RECORD",

    // --- Stats UI ---
    "🏆 All-Time Rekorde": "🏆 All-Time Records",
    "⏱️ Tagesbestzeiten (Heute)": "⏱️ Daily Bests (Today)",
    "Offizielle Strecken-Rekordliste": "Official track record list",
    "📌 Offizielle Rekorde": "📌 Official Records",
    "Aus allen Sessions ermittelt": "Calculated from all sessions",
    "📊 Aus Sessions": "📊 From Sessions",
    "📍 STRECKE:": "📍 TRACK:",
    "🏎️ FAHRZEUG:": "🏎️ CAR:",
    "👤 FAHRER:": "👤 DRIVER:",
    "SORTIERUNG:": "SORT BY:",
    "⚡ Zeit": "⚡ Time",
    "👤 Fahrer": "👤 Driver",
    "🏎️ Fahrzeug": "🏎️ Car",
    "📅 Datum": "📅 Date",
    "Keine Rekorde für die ausgewählten Kriterien auf {track} vorhanden.": "No records found for the selected criteria on {track}.",
    "🔄 Runden": "🔄 Laps",
    "⚡ Bestzeit": "⚡ Best Lap",
    "Direkt zur Session dieser Strecken-Bestzeit springen": "Jump directly to the session of this track best",
    "Gefahrene Runden:": "Completed Laps:",
    "Bestzeit": "Best Lap",
    "Profil öffnen ▶": "Open profile ▶",
    "Keine Fahrerdaten vorhanden.": "No driver data available.",
    "🔄 Index aus Dateisystem neu aufbauen": "🔄 Rebuild index from filesystem",
    "Keine gespeicherten Durchgänge für diese Auswahl im Index vorhanden.": "No saved sessions found in the index for this selection.",
    "Läufe": "Runs",
    "Runden": "Laps",
    "Laufzeiten": "Run Times",
    "Rundenzeiten": "Lap Times"
  };

  window.LT_I18N = Object.assign(window.LT_I18N || {}, dict);

  window.applyI18nDOM = function() {
    document.documentElement.lang = 'en';

    if (document.body) {
      const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
        acceptNode: function(node) {
          const tag = node.parentElement ? node.parentElement.tagName : '';
          if (tag === 'SCRIPT' || tag === 'STYLE' || tag === 'TEXTAREA') return NodeFilter.FILTER_REJECT;
          return NodeFilter.FILTER_ACCEPT;
        }
      });

      let node;
      while ((node = walker.nextNode())) {
        const raw = node.nodeValue;
        const trimmed = raw.trim();
        if (!trimmed) continue;
        if (window.LT_I18N[trimmed]) {
          const lead = raw.match(/^\s*/)[0];
          const trail = raw.match(/\s*$/)[0];
          node.nodeValue = lead + window.LT_I18N[trimmed] + trail;
        }
      }

      document.querySelectorAll('[title],[placeholder],[alt],input[type="button"],input[type="submit"]').forEach(el => {
        ['title', 'placeholder', 'alt'].forEach(attr => {
          const val = el.getAttribute(attr);
          if (val && window.LT_I18N[val.trim()]) {
            el.setAttribute(attr, window.LT_I18N[val.trim()]);
          }
        });
        if (el.tagName === 'INPUT' && (el.type === 'button' || el.type === 'submit')) {
          if (el.value && window.LT_I18N[el.value.trim()]) {
            el.value = window.LT_I18N[el.value.trim()];
          }
        }
      });

      const langBtn = document.getElementById('langToggleBtn');
      if (langBtn) langBtn.textContent = '🇩🇪 DE';
    }
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', window.applyI18nDOM);
  } else {
    window.applyI18nDOM();
  }
})();
