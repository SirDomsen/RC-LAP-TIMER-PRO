function startRaceCountdown(onComplete) {
  cancelRaceCountdown();
  speak("Achtung, das Rennen beginnt gleich!", true);

  let t1 = setTimeout(() => {
    speak("3");
    let t2 = setTimeout(() => {
      speak("2");
      let t3 = setTimeout(() => {
        speak("1");
        let t4 = setTimeout(() => {
          speak("Los!");
          if (typeof onComplete === 'function') {
            onComplete();
          }
        }, 1000);
        countdownTimeouts.push(t4);
      }, 1000);
      countdownTimeouts.push(t3);
    }, 1000);
    countdownTimeouts.push(t2);
  }, 5000);
  countdownTimeouts.push(t1);
}

function hidePoleBanner() {
  let banner = document.getElementById('poleBanner');
  if (banner) banner.style.display = 'none';
}

function startPractice() { 
  cancelRaceCountdown();
  hidePoleBanner();
  sendSession(0, 0, 0, "Freies Training gestartet"); 
}

function startQuali() {
  cancelRaceCountdown();
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
  cancelRaceCountdown();
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

  fetch(`/api/session?mode=${mode}&duration=${duration}&laps=${laps}`, { method: 'POST' })
    .then(() => {
      resetLapsDataSilent();
      if (speakText) {
        speak(speakText, true);
      }
    }).catch(err => console.error(err));
}

function stopSession() {
  cancelRaceCountdown();
  hidePoleBanner();

  currentMode = 0;
  isSessionActive = false;
  isSessionFinished = false;
  qualiFinishedHold = false;
  timeExpiredAnnounced = false;
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
  cancelRaceCountdown();
  hidePoleBanner();
  qualiFinishedHold = false;
  timeExpiredAnnounced = false;
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
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
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
    isSessionFinished = false;
    qualiFinishedHold = false;
    maxLapInRace = 0;
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
    window.sessionStartTs = Math.floor(Date.now() / 1000);
    let leaderDisp = document.getElementById('leaderLapDisplay');
    if (leaderDisp) leaderDisp.innerText = "0";
    if (hardReset && typeof loadTracks === 'function') loadTracks();
    updateData(true);
  }).catch(err => console.error(err));
}
