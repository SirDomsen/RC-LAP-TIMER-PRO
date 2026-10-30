function initWebSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    return;
  }
  let wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  let wsUrl = wsProtocol + '//' + window.location.host + '/ws';
  try {
    socket = new WebSocket(wsUrl);
    socket.onopen = function() {
      console.log("[WebSocket] Verbunden mit Basis");
      hideConnectionNotice();
    };
    socket.onmessage = function(event) {
      try {
        let msg = JSON.parse(event.data);
        handleLiveEvent(msg);
      } catch (e) {
        console.error("[WebSocket] Parse-Fehler:", e);
      }
    };
    socket.onclose = function(event) {
      if (event.code === 1013) {
        console.log("[WebSocket] Server überlastet (1013), warte auf Freigabe...");
        showConnectionNotice("⏳ Boxengasse voll: Verbinde in wenigen Sekunden...");
        setTimeout(initWebSocket, 3500);
      } else {
        console.log("[WebSocket] Getrennt, verbinde neu in 2s...");
        setTimeout(initWebSocket, 3000);
      }
    };
    socket.onerror = function(err) {
      console.error("[WebSocket] Fehler:", err);
      socket.close();
    };
  } catch (e) {
    console.error("[WebSocket] Setup-Fehler:", e);
  }
}

// Hebel 2: Frontend-Lifecycle-Hooks für sauberes Socket-Teardown bei Reload & Standby
function closeWebSocketOnUnload() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {
    try {
      socket.close(1000, "pagehide");
    } catch (e) {}
  }
}
window.addEventListener('pagehide', closeWebSocketOnUnload);
window.addEventListener('beforeunload', closeWebSocketOnUnload);

document.addEventListener('visibilitychange', function() {
  if (document.visibilityState === 'visible') {
    if (!socket || socket.readyState === WebSocket.CLOSED || socket.readyState === WebSocket.CLOSING) {
      console.log("[WebSocket] Wakeup: Socket getrennt, verbinde neu...");
      initWebSocket();
    }
  }
});

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
  if (msg.type === 'sector') {
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
      if (typeof loadTracks === 'function') loadTracks();
      requestUpdateData(true, true);
    }
  }
}

function onSessionStartLiveEvent(msg) {
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
      }
    }
  }

  if (sectorAudioEnabled && isCarAllowedForTTS(carId) && (msg.isPB || msg.isOverallBest)) {
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
      if (currentMode !== 0 && position > 0) posText = ", Platz " + position + ", ";

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
