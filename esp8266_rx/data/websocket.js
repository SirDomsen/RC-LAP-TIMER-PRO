let socket = null;
let isPageUnloading = false;

// Eindeutige, tab-spezifische ID im sessionStorage (bleibt bei Page-Reloads im selben Tab erhalten)
let wsTabId = sessionStorage.getItem('lp_tab_id');
if (!wsTabId) {
  wsTabId = 't_' + Math.random().toString(36).substring(2, 8);
  sessionStorage.setItem('lp_tab_id', wsTabId);
}

// --- GRAND PRIX CIRCUIT STARTAMPEL (WEBSITE STATUS HEARTBEAT) ---
let lastHeartbeatPacketTs = 0;
let lastPingSentTs = 0;
let lastMeasuredLatency = null;
let heartbeatWatchdogTimer = null;

function isUiDataSyncing() {
  if (typeof isHeapBusy !== 'undefined' && isHeapBusy) return true;
  if (typeof isUpdatingData !== 'undefined' && isUpdatingData) return true;
  if (typeof pendingUpdateRequested !== 'undefined' && pendingUpdateRequested) return true;
  if (typeof updateDataTimer !== 'undefined' && updateDataTimer !== null) return true;
  return false;
}

function refreshStartampelStatus() {
  if (!socket || socket.readyState !== WebSocket.OPEN) return;
  if (lastHeartbeatPacketTs > 0 && (Date.now() - lastHeartbeatPacketTs > 4500)) return;
  if (isUiDataSyncing()) {
    let busyText = (typeof isHeapBusy !== 'undefined' && isHeapBusy) ? 'WARTEN...' : 'SYNC...';
    setStartampelStatus('yellow', null, busyText);
  } else {
    setStartampelStatus('green', lastMeasuredLatency, lastMeasuredLatency ? `LIVE ${lastMeasuredLatency}ms` : 'LIVE');
  }
}

function setStartampelStatus(state, latencyMs = null, customText = null) {
  const redLamps = document.querySelectorAll('.lamp-red');
  const yellowLamps = document.querySelectorAll('.lamp-yellow');
  const greenLamps = document.querySelectorAll('.lamp-green');
  const textElem = document.getElementById('heartbeatStatusText');
  const dimmerText = document.getElementById('dimmerStatusText');

  redLamps.forEach(l => l.classList.toggle('active', state === 'red'));
  yellowLamps.forEach(l => l.classList.toggle('active', state === 'yellow'));
  greenLamps.forEach(l => l.classList.toggle('active', state === 'green'));

  let label = customText;
  if (!label) {
    if (state === 'green') {
      label = (latencyMs !== null && latencyMs > 0) ? `LIVE ${latencyMs}ms` : 'LIVE';
    } else if (state === 'yellow') {
      label = 'SYNC...';
    } else {
      label = 'STANDBY';
    }
  }

  const cls = (state === 'green') ? 'status-green' : (state === 'yellow') ? 'status-yellow' : 'status-red';

  if (textElem) {
    textElem.className = 'startampel-text ' + cls;
    textElem.innerText = label;
  }
  if (dimmerText) {
    dimmerText.className = 'startampel-text ' + cls;
    dimmerText.innerText = label;
  }
}

function startHeartbeatWatchdog() {
  if (heartbeatWatchdogTimer) return;
  heartbeatWatchdogTimer = setInterval(() => {
    if (isPageUnloading || document.visibilityState === 'hidden') return;
    let now = Date.now();
    if (lastHeartbeatPacketTs > 0 && (now - lastHeartbeatPacketTs > 4500)) {
      setStartampelStatus('red', null, (!socket || socket.readyState !== WebSocket.OPEN) ? 'OFFLINE' : 'STALL');
    } else if (socket && socket.readyState === WebSocket.OPEN) {
      if (now - lastHeartbeatPacketTs >= 2500) {
        lastPingSentTs = now;
        try {
          socket.send(JSON.stringify({ ping: lastPingSentTs }));
        } catch (e) {}
      }
    }
  }, 1500);
}

function initWebSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    return;
  }
  startHeartbeatWatchdog();
  let wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  let wsUrl = wsProtocol + '//' + window.location.host + '/ws';
  try {
    socket = new WebSocket(wsUrl);
    socket.onopen = function() {
      console.log("[WebSocket] Verbunden mit Basis");
      lastHeartbeatPacketTs = Date.now();
      setStartampelStatus('yellow', null, 'SYNC...');
      hideConnectionNotice();
      try {
        socket.send(JSON.stringify({ cmd: "hello", tab: wsTabId }));
        lastPingSentTs = Date.now();
        socket.send(JSON.stringify({ ping: lastPingSentTs }));
      } catch (e) {}
      // Nach Verbindungsaufbau oder Wakeup sofort aktuelle Telemetrie abrufen
      if (typeof requestUpdateData === 'function') {
        requestUpdateData(true, true);
      }
      if (typeof loadTracks === 'function') {
        loadTracks();
      }
    };
    socket.onmessage = function(event) {
      lastHeartbeatPacketTs = Date.now();
      try {
        let msg = JSON.parse(event.data);
        if (msg.ping) {
          lastMeasuredLatency = Math.max(0, Date.now() - Number(msg.ping));
          refreshStartampelStatus();
        } else {
          refreshStartampelStatus();
        }
        handleLiveEvent(msg);
      } catch (e) {
        refreshStartampelStatus();
        console.error("[WebSocket] Parse-Fehler:", e);
      }
    };
    socket.onclose = function(event) {
      if (isPageUnloading) return;
      setStartampelStatus('yellow', null, 'SYNC...');
      if (event.code === 1013) {
        console.log("[WebSocket] Server überlastet (1013), warte auf Freigabe...");
        showConnectionNotice("⏳ Boxengasse voll: Verbinde in wenigen Sekunden...");
        setTimeout(initWebSocket, 3500);
      } else {
        console.log("[WebSocket] Getrennt, verbinde neu...");
        setTimeout(initWebSocket, 2500);
      }
    };
    socket.onerror = function(err) {
      setStartampelStatus('yellow', null, 'SYNC...');
      console.error("[WebSocket] Fehler:", err);
      try { socket.close(); } catch (e) {}
    };
  } catch (e) {
    setStartampelStatus('red', null, 'OFFLINE');
    console.error("[WebSocket] Setup-Fehler:", e);
  }
}

// Sauberes Teardown bei Reload & Verlassen der Seite (ohne Geister-Reconnects)
function closeWebSocketOnUnload() {
  isPageUnloading = true;
  if (heartbeatWatchdogTimer) clearInterval(heartbeatWatchdogTimer);
  if (socket) {
    socket.onclose = null; // Reconnect-Timer beim Verlassen der Seite unterbinden
    try {
      socket.close(1000, "page_unload");
    } catch (e) {}
  }
}
window.addEventListener('pagehide', closeWebSocketOnUnload);
window.addEventListener('beforeunload', closeWebSocketOnUnload);

// Standby-Wakeup bei entsperrtem Handy (Automatischer Re-Sync)
let wakeupDebounceTimer = null;

function handleWakeupFromStandby() {
  if (isPageUnloading) return;
  if (document.visibilityState === 'visible') {
    // Hängengebliebene Netzwerk-Sperren aus vorherigem Standby auflösen
    if (typeof isUpdatingData !== 'undefined') isUpdatingData = false;
    if (typeof isPolling !== 'undefined') isPolling = false;

    if (wakeupDebounceTimer) clearTimeout(wakeupDebounceTimer);

    let now = Date.now();
    // Wenn das Handy aus dem Schlaf kommt: Sofort Rot (STANDBY) anzeigen, wenn älter als 3.5s
    if (lastHeartbeatPacketTs > 0 && (now - lastHeartbeatPacketTs > 3500)) {
      setStartampelStatus('red', null, 'STANDBY');
    }

    if (!socket || socket.readyState !== WebSocket.OPEN) {
      showConnectionNotice("🔄 Synchronisiere mit Basisstation...");
    }

    // 150ms nach Entsperren: Startampel schaltet auf GELB (SYNC...), während WLAN assoziiert
    setTimeout(() => {
      if (!socket || socket.readyState !== WebSocket.OPEN) {
        setStartampelStatus('yellow', null, 'SYNC...');
      }
    }, 150);

    // 500ms Puffer: Dem Smartphone-WLAN Zeit geben, die Verbindung zur Basis nach Standby zu stabilisieren
    wakeupDebounceTimer = setTimeout(() => {
      if (!socket || socket.readyState === WebSocket.CLOSED || socket.readyState === WebSocket.CLOSING) {
        console.log("[WebSocket] Wakeup: Socket getrennt, verbinde neu...");
        setStartampelStatus('yellow', null, 'SYNC...');
        initWebSocket();
      } else if (socket.readyState === WebSocket.OPEN) {
        try {
          lastPingSentTs = Date.now();
          socket.send(JSON.stringify({ ping: lastPingSentTs }));
          if (typeof requestUpdateData === 'function') {
            requestUpdateData(true, true);
          }
          hideConnectionNotice();
        } catch (e) {
          if (socket) {
            socket.onclose = null;
            try { socket.close(); } catch (err) {}
          }
          setStartampelStatus('yellow', null, 'SYNC...');
          initWebSocket();
        }
      }
    }, 500);
  }
}

document.addEventListener('visibilitychange', handleWakeupFromStandby);
window.addEventListener('focus', handleWakeupFromStandby);

function showConnectionNotice(text) {
  let notice = document.getElementById('connectionNoticeBanner');
  if (!notice) {
    notice = document.createElement('div');
    notice.id = 'connectionNoticeBanner';
    notice.style.cssText = "background:rgba(255,153,0,0.15); border:1px solid var(--neon-orange); color:var(--neon-orange); padding:10px 15px; border-radius:8px; margin-bottom:12px; font-family:var(--font-display); font-size:13px; text-align:center;";
    let wrapper = document.querySelector('.main-wrapper');
    if (wrapper) wrapper.insertBefore(notice, wrapper.firstChild);
  }
  notice.innerText = text;
  notice.style.display = 'block';
}

function hideConnectionNotice() {
  let notice = document.getElementById('connectionNoticeBanner');
  if (notice) notice.style.display = 'none';
}

let lastResetTimestamp = 0;
let lastDragStartTs = {};

function handleLiveEvent(msg) {
  if (msg.type === 'ghost_target') {
    if (typeof onGhostTargetLiveEvent === 'function') onGhostTargetLiveEvent(msg);
  } else if (msg.type === 'ghost_beaten') {
    if (typeof onGhostBeatenLiveEvent === 'function') onGhostBeatenLiveEvent(msg);
  } else if (msg.type === 'sector') {
    onSectorLiveEvent(msg);
  } else if (msg.type === 'outlap') {
    onOutlapLiveEvent(msg);
  } else if (msg.type === 'track_seen') {
    handleSectorTrackEntryTTS(msg.carId);
    requestUpdateData();
  } else if (msg.type === 'base_cross') {
    handleBaseCrossEvent(msg);
  } else if (msg.type === 'lap') {
    onLapLiveEvent(msg);
  } else if (msg.type === 'drag_start') {
    onDragStartEvent(msg);
  } else if (msg.type === 'drag_finish') {
    onDragFinishEvent(msg);
  } else if (msg.type === 'session_completed') {
    if (typeof finishSessionNow === 'function') {
      finishSessionNow();
    }
  } else if (msg.type === 'track_record') {
    if (typeof loadTracks === 'function') loadTracks();
  } else if (msg.type === 'session_start') {
    onSessionStartLiveEvent(msg);
  } else if (msg.type === 'countdown') {
    if (typeof handleCountdownEvent === 'function') handleCountdownEvent(msg);
  } else if (msg.type === 'countdown_cancel') {
    if (typeof cancelRaceCountdown === 'function') cancelRaceCountdown(false);
  } else if (msg.type === 'reset') {
    let now = Date.now();
    if (now - lastResetTimestamp > 3000) {
      lastResetTimestamp = now;
      let prevMode = currentMode;
      isSessionFinished = false;
      isSessionActive = false;
      if (typeof lastAnnouncedLapMap !== 'undefined') lastAnnouncedLapMap = {};
      if (typeof sectorFlashUntilMap !== 'undefined') sectorFlashUntilMap = {};
      if (typeof previousLaps !== 'undefined') previousLaps = {};
      if (typeof previousBestLaps !== 'undefined') previousBestLaps = {};
      if (typeof lastLapDeltaMap !== 'undefined') lastLapDeltaMap = {};
      if (typeof previousBestSzMap !== 'undefined') previousBestSzMap = {};
      if (typeof lastStartAnnouncedTs !== 'undefined') lastStartAnnouncedTs = {};
      announcedLaps5 = false;
      announcedLaps3 = false;
      announcedLaps1 = false;
      announced60 = false;
      announced30 = false;
      announced10 = false;
      timeExpiredAnnounced = false;
      if (prevMode === 5) {
        let ghHud = document.getElementById('ghostHudContainer');
        if (ghHud) ghHud.style.display = 'none';
        let ghBanner = document.getElementById('ghostBeatenBanner');
        if (ghBanner) ghBanner.style.display = 'none';
        if (typeof updateGhostButtonStates === 'function') updateGhostButtonStates();
      }
      if (typeof loadTracks === 'function') loadTracks();
      requestUpdateData(true, true);
    }
  }
}

function onSessionStartLiveEvent(msg) {
  let prevMode = currentMode;
  isSessionFinished = false;
  isSessionActive = true;
  if (msg.mode !== undefined) currentMode = msg.mode;
  if (msg.laps !== undefined) targetLaps = msg.laps;
  sessionBestLap = 0;
  maxLapInRace = 0;
  currentLeaderId = null;
  qualiFinishedHold = false;
  timeExpiredAnnounced = false;
  announced60 = false;
  announced30 = false;
  announced10 = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;
  if (typeof lastAnnouncedLapMap !== 'undefined') lastAnnouncedLapMap = {};
  if (typeof previousLaps !== 'undefined') previousLaps = {};
  if (typeof previousBestLaps !== 'undefined') previousBestLaps = {};
  if (typeof lastLapDeltaMap !== 'undefined') lastLapDeltaMap = {};
  if (typeof previousBestSzMap !== 'undefined') previousBestSzMap = {};
  if (typeof lastStartAnnouncedTs !== 'undefined') lastStartAnnouncedTs = {};
  if (typeof loadTracks === 'function') loadTracks();

  if (currentMode === 5) {
    window.localGhostHuntActive = false;
    if (typeof localGhostHuntActive !== 'undefined') localGhostHuntActive = false;
    localStorage.removeItem('localGhostHuntActive');
    localStorage.removeItem('localGhostTarget');
    if (typeof speak === 'function') speak("Ghost Battle gestartet!", true);
    if (typeof updateGhostButtonStates === 'function') updateGhostButtonStates();
    if (typeof renderGhostHud === 'function') renderGhostHud();
  } else if (prevMode === 5 && currentMode !== 5) {
    if (typeof speak === 'function') speak("Ghost Battle beendet.", true);
    let ghHud = document.getElementById('ghostHudContainer');
    if (ghHud) ghHud.style.display = 'none';
    let ghBanner = document.getElementById('ghostBeatenBanner');
    if (ghBanner) ghBanner.style.display = 'none';
    if (typeof updateGhostButtonStates === 'function') updateGhostButtonStates();
  }

  requestUpdateData(true, true);
}

function onOutlapLiveEvent(msg) {
  let carId = msg.carId;
  if (typeof handleSectorTrackEntryTTS === 'function') {
    handleSectorTrackEntryTTS(carId);
  }
  if (typeof statusCarsMap !== 'undefined') {
    statusCarsMap[carId] = 'outlap';
  }
  requestUpdateData();
}

function onDragStartEvent(msg) {
  let now = Date.now();
  if (!lastDragStartTs[msg.carId] || (now - lastDragStartTs[msg.carId] > 3000)) {
    lastDragStartTs[msg.carId] = now;
    if (isCarAllowedForTTS(msg.carId)) {
      let driver = msg.driver || (carNamesMap[msg.carId]?.driver) || ("Fahrer " + msg.carId);
      speak(driver + " gestartet!");
    }
  }
  requestUpdateData();
}

function onDragFinishEvent(msg) {
  if (isCarAllowedForTTS(msg.carId)) {
    let driver = msg.driver || (carNamesMap[msg.carId]?.driver) || ("Fahrer " + msg.carId);
    let timeSec = typeof formatTimeForTTS === 'function' ? formatTimeForTTS(msg.time) : (msg.time / 1000).toFixed(2).replace('.', ',');
    let text = "";
    if (msg.isOverallBest) {
      text = driver + ", neue absolute Bestzeit! " + timeSec + " Sekunden!";
    } else if (msg.isPB) {
      text = driver + ", persönliche Bestzeit! " + timeSec + " Sekunden!";
    } else {
      text = driver + ", Ziel! " + timeSec + " Sekunden!";
    }
    speak(text);
  }
  requestUpdateData();
}

function onSectorLiveEvent(msg) {
  let carId = msg.carId;

  if (typeof statusCarsMap !== 'undefined' && (statusCarsMap[carId] === 'pause' || statusCarsMap[carId] === 'box')) {
    if (typeof handleSectorTrackEntryTTS === 'function') {
      handleSectorTrackEntryTTS(carId);
    }
    statusCarsMap[carId] = 'outlap';
  }

  let colorType = 'yellow';
  if (msg.isOverallBest) {
    colorType = 'purple';
  } else if (msg.isPB) {
    colorType = 'green';
  }

  if (typeof sectorFlashUntilMap !== 'undefined') {
    let key = `${carId}_${msg.sector}`;
    sectorFlashUntilMap[key] = {
      until: Date.now() + 4000,
      color: colorType
    };
  }

  let cell = document.getElementById(`sz-cell-${carId}-${msg.sector}`);
  if (cell) {
    if (currentMode === 4) {
      let splitDisp = msg.split > 0 ? (msg.split / 1000).toFixed(2) + 's' : '---';
      let valDisp = msg.time > 0 ? (msg.time / 1000).toFixed(2) + 's' : '---';
      cell.innerHTML = `<div class="drag-cell"><span class="drag-split">Spl: ${splitDisp}</span><span class="drag-sector">${valDisp}</span></div>`;
    } else {
      cell.innerText = (msg.time / 1000).toFixed(2) + 's';
    }

    let textClass = (colorType === 'purple') ? 'sector-best-purple' : ((colorType === 'green') ? 'sector-best-green' : 'sector-yellow');
    let flashClass = `sector-flash-${colorType}`;

    cell.className = '';
    void cell.offsetWidth;
    cell.className = `${textClass} ${flashClass}`;
  } else {
    requestUpdateData();
  }

  let isLocalGhostActive = (typeof window.localGhostHuntActive !== 'undefined' && window.localGhostHuntActive) || (localStorage.getItem('localGhostHuntActive') === 'true');
  let isGhostHuntActive = (currentMode === 5) || isLocalGhostActive;
  let activeGhostHunterId = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) ? selectedTTSCars[0] : 1;
  let isGhostHunter = (carId === activeGhostHunterId);
  let effectiveGhostDelta = undefined;

  // Wenn der Fahrer einen persönlichen Geist gewählt hat, berechnen wir das Delta strikt lokal gegen currentGhostTarget
  if (isLocalGhostActive && isGhostHunter && typeof currentGhostTarget !== 'undefined' && currentGhostTarget) {
    let refSplit = 0;
    if (msg.sector === 1) refSplit = currentGhostTarget.s1;
    else if (msg.sector === 2) refSplit = (currentGhostTarget.s1 > 0 && currentGhostTarget.s2 > 0) ? (currentGhostTarget.s1 + currentGhostTarget.s2) : 0;
    else if (msg.sector === 3) refSplit = (currentGhostTarget.s1 > 0 && currentGhostTarget.s2 > 0 && currentGhostTarget.s3 > 0) ? (currentGhostTarget.s1 + currentGhostTarget.s2 + currentGhostTarget.s3) : 0;
    else if (msg.sector === 4) refSplit = currentGhostTarget.lap;

    if (refSplit > 0 && msg.split > 0) {
      effectiveGhostDelta = msg.split - refSplit;
    }
  } else if (isGhostHunter) {
    effectiveGhostDelta = msg.ghostDelta;
  }

  // Live-Aktualisierung des Dashboards/OLED-Dimmers fuer ausgewaehlte Fahrzeuge (Fahrer 1 & 2)
  if (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) {
    let colIdx = (selectedTTSCars[0] === carId) ? 1 : ((selectedTTSCars.length > 1 && selectedTTSCars[1] === carId) ? 2 : 0);
    if (colIdx > 0) {
      let dimBox = document.getElementById(`dimmer${colIdx}-box-${msg.sector}`);
      let dimVal = document.getElementById(`dimmer${colIdx}-sz${msg.sector}-val`);
      if (dimBox && dimVal) {
        if (currentMode === 4) {
          let splitDisp = msg.split > 0 ? (msg.split / 1000).toFixed(2) + 's' : '---';
          let valDisp = msg.time > 0 ? (msg.time / 1000).toFixed(2) + 's' : '---';
          dimVal.innerHTML = `<div class="drag-cell"><span class="drag-split" style="font-size:11px;color:var(--text-muted);display:block;margin-bottom:3px;">Spl: ${splitDisp}</span><span>${valDisp}</span></div>`;
        } else {
          dimVal.innerText = (msg.time / 1000).toFixed(2) + 's';
        }
        dimBox.className = 'dimmer-sector-box'; 
        void dimBox.offsetWidth; 
        dimBox.className = `dimmer-sector-box sector-flash-${colorType}`;
        
        let textCol = (colorType === 'purple') ? 'var(--neon-purple)' : ((colorType === 'green') ? 'var(--neon-green)' : 'var(--neon-orange)');
        dimVal.style.color = textCol;
        dimVal.style.textShadow = (colorType === 'purple') ? '0 0 8px rgba(181,50,255,0.7)' : 'none';

        let dimDelta = document.getElementById(`dimmer${colIdx}-sz${msg.sector}-delta`);
        if (dimDelta) {
          if (isGhostHuntActive && colIdx === 1 && effectiveGhostDelta !== undefined) {
            let isAhead = (effectiveGhostDelta <= 0);
            dimDelta.innerText = (isAhead ? '' : '+') + (effectiveGhostDelta / 1000).toFixed(2) + 's';
            dimDelta.className = 'dimmer-sz-delta ' + (isAhead ? 'delta-green' : 'delta-red');
          }
        }
      }
    }
  }

  // Geisterjagd Arcade-Sounds, HUD-Update & TTS: Streng NUR fuer den aktiven Jaeger (Fahrer 1)!
  // Fahrer 2 ist reiner Zuhoerer fuer seine eigenen Rundenzeiten und erzeugt keine Geister-Sounds.
  // WICHTIG: Bei Zieldurchfahrt (isFinal / sector 4) KEINEN Sektor-Beep spielen, hier spielt nur die Runden-Fanfare / Runden-Ton!
  let isFinalSector = !!msg.isFinal || (msg.sector === 4);
  if (isGhostHuntActive && isGhostHunter && effectiveGhostDelta !== undefined) {
    if (typeof updateGhostHudSector === 'function') {
      updateGhostHudSector({ ...msg, ghostDelta: effectiveGhostDelta });
    }
    if (!isFinalSector) {
      if (typeof ghostAudioBeepsEnabled !== 'undefined' && ghostAudioBeepsEnabled) {
        if (effectiveGhostDelta <= 0) {
          if (typeof playWebAudioGhostAhead === 'function') playWebAudioGhostAhead();
        } else {
          if (typeof playWebAudioGhostBehind === 'function') playWebAudioGhostBehind();
        }
      }
      if (typeof ghostTtsDeltaEnabled !== 'undefined' && ghostTtsDeltaEnabled) {
        if (typeof formatGhostDeltaForTTS === 'function') {
          let ttsText = formatGhostDeltaForTTS(effectiveGhostDelta);
          if (ttsText) speak(ttsText, false, carId);
        }
      }
    }
  } else if (!isFinalSector && sectorAudioEnabled && isCarAllowedForTTS(carId) && (msg.isPB || msg.isOverallBest)) {
    let driver = msg.driver || (carNamesMap[carId]?.driver) || ("Fahrer " + carId);
    let text = "";
    if (msg.isOverallBest) {
      text = driver + ", absolute Bestzeit Sektor " + msg.sector + "!";
    } else if (msg.isPB) {
      text = driver + ", Bestzeit Sektor " + msg.sector + "!";
    }
    if (text) speak(text, false, carId);
  }
}

function onLapLiveEvent(msg) {
  if (!msg || !msg.carId) return;

  let carId = msg.carId;
  let driverName = msg.driver || (typeof carNamesMap !== 'undefined' && carNamesMap[carId]?.driver) || ("Fahrer " + carId);
  let completedLapNum = msg.lap || 1;
  let position = msg.pos || 1;
  let isOutLapDone = !!msg.isOutLap;
  let isNewTrackRecord = !!msg.isTrackRecord;
  let isNewDailyRecord = !!msg.isDailyRecord;
  let isPersonalBest = !!msg.isPB;
  let isAbsoluteBest = isPersonalBest && !!msg.isOverallBest;

  if (typeof pendingRecordFlagsMap !== 'undefined') {
    pendingRecordFlagsMap[carId] = {
      lapTime: msg.lapTime || 0,
      isTrackRecord: isNewTrackRecord,
      isDailyRecord: isNewDailyRecord,
      isOutLap: isOutLapDone
    };
  }

  // Live-Aktualisierung des Dashboards/OLED-Dimmers fuer das eigene Auto (Fahrer 1 & 2) ohne auf HTTP zu warten
  if (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) {
    let colIdx = (selectedTTSCars[0] === carId) ? 1 : ((selectedTTSCars.length > 1 && selectedTTSCars[1] === carId) ? 2 : 0);
    if (colIdx > 0) {
      let dLast = document.getElementById(`dimmerLast${colIdx}Text`);
      let dPos = document.getElementById(`dimmerPos${colIdx}Text`);
      let dPb = document.getElementById(`dimmerPb${colIdx}Text`);
      let dStatus = document.getElementById(`dimmerStatus${colIdx}Badge`);
      if (dStatus) dStatus.style.display = isOutLapDone ? 'inline-block' : 'none';
      if (dPos && position > 0) dPos.innerText = "P" + position;
      if (dLast && msg.lapTime > 0) {
        dLast.innerText = (msg.lapTime / 1000).toFixed(2) + 's';
        dLast.style.color = isAbsoluteBest ? 'var(--neon-purple)' : (isPersonalBest ? 'var(--neon-green)' : 'var(--text-main)');
      }
      if (dPb && isPersonalBest && msg.lapTime > 0) {
        dPb.innerText = (msg.lapTime / 1000).toFixed(2) + 's';
      }
    }
  }

  // Geisterjagd Check (Modus 5 oder lokale Geisterjagd): Streng nur fuer den aktiven Jaeger (Fahrer 1)!
  // Fahrer 2 (Zuhoerer) beeinflusst den Geist nicht und loest keine Siegesfanfare aus.
  let isGhostHuntLap = (currentMode === 5) || (typeof window.localGhostHuntActive !== 'undefined' && window.localGhostHuntActive) || (localStorage.getItem('localGhostHuntActive') === 'true');
  let activeGhostHunterId = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) ? selectedTTSCars[0] : 1;
  let isGhostHunter = (carId === activeGhostHunterId);
  if (isGhostHuntLap && isGhostHunter && !isOutLapDone && msg.lapTime > 0) {
    if (typeof currentGhostTarget !== 'undefined' && currentGhostTarget) {
      if (!currentGhostTarget.lap || currentGhostTarget.lap === 0) {
        currentGhostTarget.lap = msg.lapTime;
        currentGhostTarget.s1 = msg.s1 || 0;
        currentGhostTarget.s2 = msg.s2 || 0;
        currentGhostTarget.s3 = msg.s3 || 0;
        currentGhostTarget.s4 = msg.s4 || 0;
        currentGhostTarget.driver = driverName;
        if (typeof renderGhostHud === 'function') renderGhostHud();
        speak("Geist gesetzt: " + (msg.lapTime / 1000).toFixed(2) + " Sekunden! Schnapp ihn dir!", true);
      } else if (msg.lapTime <= currentGhostTarget.lap) {
        let deltaToGhost = msg.lapTime - currentGhostTarget.lap;
        if (typeof onGhostBeatenLiveEvent === 'function') {
          onGhostBeatenLiveEvent({
            lapTime: msg.lapTime,
            delta: deltaToGhost,
            driver: driverName,
            s1: msg.s1 || 0,
            s2: msg.s2 || 0,
            s3: msg.s3 || 0,
            s4: msg.s4 || 0
          });
        }
      }
    }
  }

  // 1. Rundenrennen (Modus 2): Vorrang-Ansagen für verbleibende Runden basierend auf dem Führenden
  let raceFinishedByLeader = false;
  if (currentMode === 2 && targetLaps > 0 && !isSessionFinished) {
    if (position === 1) {
      let remainingLaps = targetLaps - completedLapNum;
      if (remainingLaps === 5 && !announcedLaps5) {
        announcedLaps5 = true;
        speak("Noch 5 Runden!", true);
      } else if (remainingLaps === 3 && !announcedLaps3) {
        announcedLaps3 = true;
        speak("Noch 3 Runden!", true);
      } else if (remainingLaps === 1 && !announcedLaps1) {
        announcedLaps1 = true;
        speak("Letzte Runde!", true);
      } else if (remainingLaps <= 0) {
        raceFinishedByLeader = true;
        isSessionFinished = true;
        isSessionActive = false;
        if (typeof announceRaceResults === 'function') {
          announceRaceResults();
        }
      }
    }
  }

  // 2. Führungswechsel (Modus 2 & 3)
  let leadChangeOccurred = false;
  if (!raceFinishedByLeader && currentMode >= 2 && isSessionActive && !isSessionFinished) {
    if (currentLeaderId !== null && currentLeaderId !== carId && position === 1) {
      let oldLeaderId = currentLeaderId;
      currentLeaderId = carId;
      if (isCarAllowedForTTS(carId) || isCarAllowedForTTS(oldLeaderId)) {
        leadChangeOccurred = true;
        speak("Neuer Führender! " + driverName + " übernimmt die Spitze!", false);
      }
    } else if (currentLeaderId === null && position === 1 && completedLapNum > 0) {
      currentLeaderId = carId;
    }
  }

  // 3. Rundenzeiten- & Outlap-Ansage (Echtzeit über WebSocket!)
  if (!raceFinishedByLeader && !isSessionFinished && isCarAllowedForTTS(carId) && !leadChangeOccurred && currentMode !== 4) {
    if (isOutLapDone) {
      speak(driverName + ", Outlap beendet!", false, carId);
    } else if (msg.lapTime > 0) {
      let timeSec = typeof formatTimeForTTS === 'function' ? formatTimeForTTS(msg.lapTime) : (msg.lapTime / 1000).toFixed(2).replace('.', ',');
      let posText = ", ";
      if (currentMode === 5) {
        if (position === 1) {
          posText = ", du bist der Schnellste! ";
        } else if (position > 1) {
          posText = ", du hast derzeit die " + position + ". schnellste Zeit, ";
        }
      } else if (currentMode !== 0 && position > 0) {
        posText = ", Platz " + position + ", ";
      }

      let lapAnnouncement = "";
      let isRaceMode = (currentMode >= 2);
      let bestLapWord = isRaceMode ? "schnellste Rennrunde! " : "schnellste Runde! ";

      if (isNewTrackRecord) {
        lapAnnouncement = driverName + posText + "neuer Streckenrekord! " + timeSec;
      } else if (isNewDailyRecord) {
        lapAnnouncement = driverName + posText + "neue Tagesbestzeit! " + timeSec;
      } else if (isRaceMode && !announceLapTimesInRace) {
        if (isAbsoluteBest) lapAnnouncement = driverName + posText + bestLapWord + timeSec;
        else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        else lapAnnouncement = driverName + posText;
      } else {
        if (isAbsoluteBest) lapAnnouncement = driverName + posText + bestLapWord + timeSec;
        else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        else lapAnnouncement = driverName + posText + timeSec;
      }
      speak(lapAnnouncement, false, carId);
    }
  }

  // Runden-Markierung setzen, damit der spätere HTTP-Tabellen-Update nichts doppelt ansagt
  if (typeof lastAnnouncedLapMap !== 'undefined') {
    lastAnnouncedLapMap[carId] = completedLapNum;
  }
  if (typeof previousLaps !== 'undefined') {
    previousLaps[carId] = completedLapNum;
  }

  // 4. UI-Datenaktualisierung im Hintergrund (durch gestaffelte Drossel geschützt)
  if (msg && (msg.isTrackRecord || msg.isDailyRecord)) {
    if (typeof loadTracks === 'function') loadTracks();
    requestUpdateData(true);
  } else {
    requestUpdateData();
  }
}
