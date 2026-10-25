function initWebSocket() {
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
        setTimeout(initWebSocket, 2500);
      } else {
        console.log("[WebSocket] Getrennt, verbinde neu in 2s...");
        setTimeout(initWebSocket, 2000);
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
    updateData();
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
    // loadTracks will update tracks without blocking lap updates
  } else if (msg.type === 'reset') {
    let now = Date.now();
    if (now - lastResetTimestamp > 3000) {
      lastResetTimestamp = now;
      if (typeof lastAnnouncedLapMap !== 'undefined') lastAnnouncedLapMap = {};
      if (typeof sectorFlashUntilMap !== 'undefined') sectorFlashUntilMap = {};
      if (typeof previousLaps !== 'undefined') previousLaps = {};
      if (typeof previousBestLaps !== 'undefined') previousBestLaps = {};
      if (typeof lastLapDeltaMap !== 'undefined') lastLapDeltaMap = {};
      if (typeof previousBestSzMap !== 'undefined') previousBestSzMap = {};
      if (typeof lastStartAnnouncedTs !== 'undefined') lastStartAnnouncedTs = {};
      if (typeof loadTracks === 'function') loadTracks();
      updateData(true);
    }
  }
}

function onOutlapLiveEvent(msg) {
  let carId = msg.carId;
  if (typeof handleSectorTrackEntryTTS === 'function') {
    handleSectorTrackEntryTTS(carId);
  }
  if (typeof statusCarsMap !== 'undefined') {
    statusCarsMap[carId] = 'outlap';
  }
  updateData();
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
  updateData();
}

function onDragFinishEvent(msg) {
  if (isCarAllowedForTTS(msg.carId)) {
    let driver = msg.driver || (carNamesMap[msg.carId]?.driver) || ("Fahrer " + msg.carId);
    let timeSec = (msg.time / 1000).toFixed(2).replace('.', ',');
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
  updateData();
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
    updateData();
  }

  let isMyCar = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length === 1 && selectedTTSCars[0] === carId);
  if (isMyCar) {
    let dimBox = document.getElementById(`dimmer-box-${msg.sector}`);
    let dimVal = document.getElementById(`dimmer-sz${msg.sector}-val`);
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

  if (sectorAudioEnabled && isCarAllowedForTTS(carId) && (msg.isPB || msg.isOverallBest)) {
    let driver = msg.driver || (carNamesMap[carId]?.driver) || ("Fahrer " + carId);
    let text = "";
    if (msg.isOverallBest) {
      text = driver + ", absolute Bestzeit Sektor " + msg.sector + "!";
    } else if (msg.isPB) {
      text = driver + ", Bestzeit Sektor " + msg.sector + "!";
    }
    if (text) speak(text);
  }
}

function onLapLiveEvent(msg) {
  if (typeof pendingRecordFlagsMap !== 'undefined' && msg && msg.carId) {
    pendingRecordFlagsMap[msg.carId] = {
      lapTime: msg.lapTime || 0,
      isTrackRecord: !!msg.isTrackRecord,
      isDailyRecord: !!msg.isDailyRecord,
      isOutLap: !!msg.isOutLap
    };
  }
  if (msg && (msg.isTrackRecord || msg.isDailyRecord)) {
    if (typeof loadTracks === 'function') loadTracks();
    updateData(true);
  } else {
    updateData();
  }
}
