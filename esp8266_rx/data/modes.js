function startRaceCountdown(onComplete) {
  cancelRaceCountdown(false);

  // Countdown und Startampel zentral ueber Basis triggern (synchronisiert Sprache fuer alle Clients)
  let muteParam = (typeof muteCountdownTts !== 'undefined' && muteCountdownTts) ? "1" : "0";
  fetch('/api/countdown?cmd=start&muteTts=' + muteParam, { method: 'POST' }).catch(() => {});

  let tComplete = setTimeout(() => {
    if (typeof onComplete === 'function') {
      onComplete();
    }
  }, 8000);
  countdownTimeouts.push(tComplete);
}

function hidePoleBanner() {
  let banner = document.getElementById('poleBanner');
  if (banner) banner.style.display = 'none';
}

function startPractice() { 
  cancelRaceCountdown(true);
  hidePoleBanner();
  sendSession(0, 0, 0, "Freies Training gestartet"); 
}

function startQuali() {
  cancelRaceCountdown(true);
  hidePoleBanner();
  let mins = parseInt(document.getElementById('qualiTimeInput').value);
  if (isNaN(mins) || mins < 1) return;
  let minStr = (mins === 1) ? "eine Minute" : mins + " Minuten";
  sendSession(1, mins, 0, "Qualifying gestartet für " + minStr + "!");
}

function startRaceLaps() {
  let laps = parseInt(document.getElementById('raceLapsInput').value);
  if (isNaN(laps) || laps < 1) return;
  hidePoleBanner();

  startRaceCountdown(() => {
    sendSession(2, 0, laps, "");
  });
}

function startRaceTime() {
  let mins = parseInt(document.getElementById('raceTimeInput').value);
  if (isNaN(mins) || mins < 1) return;
  hidePoleBanner();

  startRaceCountdown(() => {
    sendSession(3, mins, 0, "");
  });
}

function startDragrace() {
  cancelRaceCountdown(true);
  hidePoleBanner();
  sendSession(4, 0, 0, "Dragrace gestartet! Zeiterfassung startet bei Überfahrt der Basis.");
}

function sendSession(mode, duration, laps, speakText) {
  if (typeof isStorageFull !== 'undefined' && (isStorageFull || currentFsPct >= 95)) {
    if (typeof openStorageFullModal === 'function') {
      openStorageFullModal();
    }
  }

  window.sessionStartTs = Math.floor(Date.now() / 1000);
  currentMode = mode;
  targetLaps = laps;
  maxLapInRace = 0;
  currentLeaderId = null;
  sessionBestLap = 0;
  isSessionActive = true;
  isSessionFinished = false;
  qualiFinishedHold = false;

  announced60 = false;
  announced30 = false;
  announced10 = false;
  timeExpiredAnnounced = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;

  fetch(`/api/session?mode=${mode}&duration=${duration}&laps=${laps}`, { method: 'POST' })
    .then(() => {
      resetLapsDataSilent();
      if (speakText) {
        speak(speakText, true);
      }
    }).catch(err => console.error(err));
}

function stopSession() {
  cancelRaceCountdown(true);
  hidePoleBanner();
  let ghostBanner = document.getElementById('ghostBeatenBanner');
  if (ghostBanner) ghostBanner.style.display = 'none';
  let ghostHud = document.getElementById('ghostHudContainer');
  if (ghostHud) ghostHud.style.display = 'none';

  currentMode = 0;
  if (typeof syncGhostHudSlot === 'function') syncGhostHudSlot();
  isSessionActive = false;
  isSessionFinished = false;
  qualiFinishedHold = false;
  timeExpiredAnnounced = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;
  currentLeaderId = null;

  fetch('/api/session?mode=0&duration=0&laps=0', { method: 'POST' })
    .then(() => {
      speak("Durchgang gestoppt. Freies Training.", true);
      updateData(true);
    }).catch(err => console.error(err));
}

function resetLaps() {
  let modal = document.getElementById('resetConfirmModal');
  if (modal) {
    modal.style.display = 'block';
  }
}

function closeResetModal() {
  let modal = document.getElementById('resetConfirmModal');
  if (modal) {
    modal.style.display = 'none';
  }
}

function confirmResetModal() {
  let hardCb = document.getElementById('resetDailyBestCheckbox');
  let isHard = hardCb ? hardCb.checked : true;
  closeResetModal();
  cancelRaceCountdown(true);
  hidePoleBanner();
  qualiFinishedHold = false;
  timeExpiredAnnounced = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;
  isSessionFinished = false;
  isSessionActive = false;
  currentMode = 0;
  fetch('/api/session?mode=0&duration=0&laps=0', { method: 'POST' })
    .then(() => resetLapsData(isHard))
    .catch(err => console.error(err));
}

function resetLapsDataSilent() {
  fetch('/api/reset?hard=0', { method: 'POST' }).then(() => {
    previousLaps = {};
    previousBestLaps = {};
    lapHistoryMap = {};
    statusCarsMap = {};
    awaitingBaseCrossMap = {};
    sectorFlashUntilMap = {};
    if (typeof lastStartAnnouncedTs !== 'undefined') lastStartAnnouncedTs = {};
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;
    updateData(true);
  }).catch(err => console.error(err));
}

function resetLapsData(hardReset = true) {
  let hardParam = hardReset ? '1' : '0';
  fetch('/api/reset?hard=' + hardParam, { method: 'POST' }).then(() => {
    previousLaps = {};
    previousBestLaps = {};
    lapHistoryMap = {};
    statusCarsMap = {};
    awaitingBaseCrossMap = {};
    sectorFlashUntilMap = {};
    if (typeof lastStartAnnouncedTs !== 'undefined') lastStartAnnouncedTs = {};
    isSessionFinished = false;
    qualiFinishedHold = false;
    maxLapInRace = 0;
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
  announcedLaps5 = false;
  announcedLaps3 = false;
  announcedLaps1 = false;
    window.sessionStartTs = Math.floor(Date.now() / 1000);
    let leaderDisp = document.getElementById('leaderLapDisplay');
    if (leaderDisp) leaderDisp.innerText = "0";
    if (hardReset && typeof loadTracks === 'function') loadTracks();
    updateData(true);
  }).catch(err => console.error(err));
}

// --- GEISTERJAGD (MODUS 5) ---
function openGhostHuntModal() {
  let modal = document.getElementById('ghostHuntModal');
  if (modal) {
    modal.style.display = 'block';
    updateGhostHuntModalDisplay();
  }
}

function closeGhostHuntModal() {
  let modal = document.getElementById('ghostHuntModal');
  if (modal) modal.style.display = 'none';
}

function updateGhostHuntModalDisplay() {
  let typeSelect = document.getElementById('ghostTargetTypeSelect');
  let type = typeSelect ? parseInt(typeSelect.value) : 0;
  let previewEl = document.getElementById('ghostPreviewInfo');
  let customInputBox = document.getElementById('ghostCustomTimeBox');

  if (customInputBox) {
    customInputBox.style.display = (type === 3) ? 'block' : 'none';
  }

  if (!previewEl) return;

  if (type === 0) {
    let recLap = (typeof allTimeBestLap !== 'undefined') ? allTimeBestLap : 0;
    let recDriver = (typeof allTimeBestDriver !== 'undefined') ? allTimeBestDriver : '';
    let recCar = (typeof allTimeBestCar !== 'undefined') ? allTimeBestCar : '';
    let recMask = (typeof allTimeBestSzMask !== 'undefined') ? allTimeBestSzMask : 0;
    let lapStr = recLap > 0 ? (recLap / 1000).toFixed(3) + 's' : 'Noch kein Streckenrekord';
    let driverStr = recDriver || 'Unbekannt';
    let carStr = recCar ? ` (${recCar})` : '';
    let szStr = (recMask > 0) ? ` <span style="color:var(--neon-green);">[mit Sektoren]</span>` : ` <span style="color:var(--neon-cyan);">[Start/Ziel]</span>`;
    previewEl.innerHTML = `🏆 <strong>Streckenrekord:</strong> <span style="color:var(--neon-purple);font-weight:bold;font-size:15px;">${lapStr}</span> von ${driverStr}${carStr}${szStr}`;
  } else if (type === 1) {
    let dLap = (typeof dailyBestLap !== 'undefined') ? dailyBestLap : 0;
    let dDriver = (typeof dailyBestDriver !== 'undefined') ? dailyBestDriver : '';
    let dCar = (typeof dailyBestCar !== 'undefined') ? dailyBestCar : '';
    let dMask = (typeof dailyBestSzMask !== 'undefined') ? dailyBestSzMask : 0;
    let lapStr = dLap > 0 ? (dLap / 1000).toFixed(3) + 's' : 'Noch keine Tagesbestzeit';
    let driverStr = dDriver || 'Unbekannt';
    let carStr = dCar ? ` (${dCar})` : '';
    let szStr = (dMask > 0) ? ` <span style="color:var(--neon-green);">[mit Sektoren]</span>` : ` <span style="color:var(--neon-cyan);">[Start/Ziel]</span>`;
    previewEl.innerHTML = `⏱️ <strong>Tagesbestzeit:</strong> <span style="color:var(--neon-green);font-weight:bold;font-size:15px;">${lapStr}</span> von ${driverStr}${carStr}${szStr}`;
  } else if (type === 2) {
    let activeCarId = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) ? selectedTTSCars[0] : 1;
    let row = (typeof lastLapsData !== 'undefined') ? lastLapsData.find(r => r.id === activeCarId) : null;
    let pb = (row && row.bestLap > 0) ? row.bestLap : (row && row.lastLap > 0 ? row.lastLap : 0);
    if (pb > 0) {
      previewEl.innerHTML = `👤 <strong>Deine Bestzeit:</strong> <span style="color:var(--neon-cyan);font-weight:bold;font-size:15px;">${(pb/1000).toFixed(3)}s</span> (Fahrzeug ${activeCarId})`;
    } else {
      previewEl.innerHTML = `👤 <strong>Eigener Geist:</strong> <span style="color:var(--neon-cyan);">Erste gezeitete Runde dieser Session setzt deinen Geist!</span>`;
    }
  } else if (type === 3) {
    previewEl.innerHTML = `🎯 <strong>Freie Zielzeit:</strong> Gib unten deine Wunsch-Rundenzeit in Sekunden ein.`;
  }
}

function startGhostHuntSession() {
  let typeSelect = document.getElementById('ghostTargetTypeSelect');
  let type = typeSelect ? parseInt(typeSelect.value) : 0;
  let customLapMs = 0;

  if (type === 3) {
    let inp = document.getElementById('ghostCustomTimeInput');
    let sec = inp ? parseFloat(inp.value.replace(',', '.')) : 0;
    if (isNaN(sec) || sec <= 0) {
      if (typeof showAlert === 'function') showAlert("Bitte eine gültige Zielzeit in Sekunden eingeben (z. B. 12.5)!");
      return;
    }
    customLapMs = Math.round(sec * 1000);
  }

  closeGhostHuntModal();
  cancelRaceCountdown(true);
  hidePoleBanner();

  let params = new URLSearchParams();
  params.append('mode', '5');
  params.append('ghostType', type);

  let speakText = "Geisterjagd gestartet! Schnapp dir den Geist!";

  if (type === 0) {
    let recLap = (typeof allTimeBestLap !== 'undefined') ? allTimeBestLap : 0;
    let recDriver = (typeof allTimeBestDriver !== 'undefined') ? allTimeBestDriver : '';
    let recCar = (typeof allTimeBestCar !== 'undefined') ? allTimeBestCar : '';
    let recMask = (typeof allTimeBestSzMask !== 'undefined') ? allTimeBestSzMask : 0;
    params.append('ghostLap', recLap);
    params.append('ghostDriver', recDriver);
    params.append('ghostCar', recCar);
    params.append('ghostMask', recMask);
    params.append('ghostS1', (typeof allTimeBestS1 !== 'undefined') ? allTimeBestS1 : 0);
    params.append('ghostS2', (typeof allTimeBestS2 !== 'undefined') ? allTimeBestS2 : 0);
    params.append('ghostS3', (typeof allTimeBestS3 !== 'undefined') ? allTimeBestS3 : 0);
    params.append('ghostS4', (typeof allTimeBestS4 !== 'undefined') ? allTimeBestS4 : 0);
    speakText = "Geisterjagd gegen den Streckenrekord!";
  } else if (type === 1) {
    let dLap = (typeof dailyBestLap !== 'undefined') ? dailyBestLap : 0;
    let dDriver = (typeof dailyBestDriver !== 'undefined') ? dailyBestDriver : '';
    let dCar = (typeof dailyBestCar !== 'undefined') ? dailyBestCar : '';
    let dMask = (typeof dailyBestSzMask !== 'undefined') ? dailyBestSzMask : 0;
    params.append('ghostLap', dLap);
    params.append('ghostDriver', dDriver);
    params.append('ghostCar', dCar);
    params.append('ghostMask', dMask);
    params.append('ghostS1', (typeof dailyBestS1 !== 'undefined') ? dailyBestS1 : 0);
    params.append('ghostS2', (typeof dailyBestS2 !== 'undefined') ? dailyBestS2 : 0);
    params.append('ghostS3', (typeof dailyBestS3 !== 'undefined') ? dailyBestS3 : 0);
    params.append('ghostS4', (typeof dailyBestS4 !== 'undefined') ? dailyBestS4 : 0);
    speakText = "Geisterjagd gegen die Tagesbestzeit!";
  } else if (type === 2) {
    let activeCarId = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) ? selectedTTSCars[0] : 1;
    let row = (typeof lastLapsData !== 'undefined') ? lastLapsData.find(r => r.id === activeCarId) : null;
    let pb = (row && row.bestLap > 0) ? row.bestLap : 0;
    if (pb > 0) {
      params.append('ghostLap', pb);
      params.append('ghostDriver', row.driver || ("Fahrer " + activeCarId));
      params.append('ghostCar', row.car || "");
      let mask = (row.bestSz1 > 0 ? 1 : 0) | (row.bestSz2 > 0 ? 2 : 0) | (row.bestSz3 > 0 ? 4 : 0) | 8;
      params.append('ghostMask', mask);
      params.append('ghostS1', row.bestSz1 || 0);
      params.append('ghostS2', row.bestSz2 || 0);
      params.append('ghostS3', row.bestSz3 || 0);
      params.append('ghostS4', row.bestSz4 || 0);
      params.append('ghostCarId', activeCarId);
      speakText = "Geisterjagd gegen deine persönliche Bestzeit!";
    } else {
      params.append('ghostLap', 0);
      params.append('ghostCarId', activeCarId);
      speakText = "Geisterjagd gestartet! Deine erste Runde setzt den Geist!";
    }
  } else if (type === 3) {
    params.append('ghostLap', customLapMs);
    params.append('ghostDriver', "Zielzeit");
    speakText = "Geisterjagd gegen Zielzeit!";
  }

  window.sessionStartTs = Math.floor(Date.now() / 1000);
  currentMode = 5;
  targetLaps = 0;
  maxLapInRace = 0;
  currentLeaderId = null;
  sessionBestLap = 0;
  isSessionActive = true;
  isSessionFinished = false;

  fetch('/api/session?' + params.toString(), { method: 'POST' })
    .then(() => {
      resetLapsDataSilent();
      if (speakText) speak(speakText, true);
      updateData(true);
    }).catch(err => console.error(err));
}
