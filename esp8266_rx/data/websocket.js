function initWebSocket() {
  let wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  let wsUrl = wsProtocol + '//' + window.location.host + '/ws';
  try {
    socket = new WebSocket(wsUrl);
    socket.onopen = function() {
      console.log("[WebSocket] Verbunden mit Basis");
    };
    socket.onmessage = function(event) {
      try {
        let msg = JSON.parse(event.data);
        handleLiveEvent(msg);
      } catch (e) {
        console.error("[WebSocket] Parse-Fehler:", e);
      }
    };
    socket.onclose = function() {
      console.log("[WebSocket] Getrennt, verbinde neu in 2s...");
      setTimeout(initWebSocket, 2000);
    };
    socket.onerror = function(err) {
      console.error("[WebSocket] Fehler:", err);
      socket.close();
    };
  } catch (e) {
    console.error("[WebSocket] Setup-Fehler:", e);
  }
}

let lastResetTimestamp = 0;

function handleLiveEvent(msg) {
  if (msg.type === 'sector') {
    onSectorLiveEvent(msg);
  } else if (msg.type === 'lap') {
    onLapLiveEvent(msg);
  } else if (msg.type === 'drag_start') {
    onDragStartEvent(msg);
  } else if (msg.type === 'drag_finish') {
    onDragFinishEvent(msg);
  } else if (msg.type === 'reset') {
    let now = Date.now();
    if (now - lastResetTimestamp > 3000) { // Sperre: Mindestens 3 Sekunden Abstand
      lastResetTimestamp = now;
      resetLapsDataSilent();
    }
  }
}
function onDragStartEvent(msg) {
  if (isCarAllowedForTTS(msg.carId)) {
    let driver = msg.driver || (carNamesMap[msg.carId]?.driver) || ("Fahrer " + msg.carId);
    speak(driver + " gestartet!");
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
  let cell = document.getElementById(`sz-cell-${msg.carId}-${msg.sector}`);
  if (cell) {
    cell.innerText = (msg.time / 1000).toFixed(2) + 's';
    cell.classList.remove('sector-flash-purple', 'sector-flash-green', 'sector-flash-yellow');
    void cell.offsetWidth;
    if (msg.isOverallBest) {
      cell.classList.add('sector-flash-purple');
    } else if (msg.isPB) {
      cell.classList.add('sector-flash-green');
    } else {
      cell.classList.add('sector-flash-yellow');
    }
  }

  if (sectorAudioEnabled && isCarAllowedForTTS(msg.carId) && (msg.isPB || msg.isOverallBest)) {
    let driver = msg.driver || (carNamesMap[msg.carId]?.driver) || ("Fahrer " + msg.carId);
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
  updateData();
}