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

function startPractice() { 
  cancelRaceCountdown();
  sendSession(0, 0, 0, "Freies Training gestartet"); 
}

function startQuali() {
  cancelRaceCountdown();
  let mins = parseInt(document.getElementById('qualiTimeInput').value);
  if (isNaN(mins) || mins < 1) return;
  let minStr = (mins === 1) ? "eine Minute" : mins + " Minuten";
  sendSession(1, mins, 0, "Qualifying gestartet für " + minStr + "!");
}

function startRaceLaps() {
  let laps = parseInt(document.getElementById('raceLapsInput').value);
  if (isNaN(laps) || laps < 1) return;

  startRaceCountdown(() => {
    sendSession(2, 0, laps, "");
  });
}

function startRaceTime() {
  let mins = parseInt(document.getElementById('raceTimeInput').value);
  if (isNaN(mins) || mins < 1) return;

  startRaceCountdown(() => {
    sendSession(3, mins, 0, "");
  });
}

function startDragrace() {
  cancelRaceCountdown();
  sendSession(4, 0, 0, "Dragrace gestartet! Zeiterfassung startet bei Überfahrt der Basis.");
}

function sendSession(mode, duration, laps, speakText) {
  currentMode = mode;
  targetLaps = laps;
  maxLapInRace = 0;
  currentLeaderId = null;
  sessionBestLap = 0;
  isSessionActive = true;
  isSessionFinished = false;

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
  currentMode = 0;
  isSessionActive = false;
  currentLeaderId = null;
  fetch('/api/session?mode=0&duration=0&laps=0', { method: 'POST' })
    .then(() => { speak("Durchgang gestoppt. Freies Training.", true); }).catch(err => console.error(err));
}

function resetLaps() {
  if (confirm("Alle Zeiten und Runden komplett zurücksetzen?")) {
    cancelRaceCountdown();
    stopSession();
    resetLapsData();
  }
}

function resetLapsDataSilent() {
  fetch('/api/reset', { method: 'POST' }).then(() => {
    previousLaps = {};
    previousBestLaps = {};
    lapHistoryMap = {};
    statusCarsMap = {};
    awaitingBaseCrossMap = {};
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
    updateData();
  }).catch(err => console.error(err));
}

function resetLapsData() {
  fetch('/api/reset', { method: 'POST' }).then(() => {
    previousLaps = {};
    previousBestLaps = {};
    lapHistoryMap = {};
    statusCarsMap = {};
    awaitingBaseCrossMap = {};
    isSessionFinished = false;
    maxLapInRace = 0;
    currentLeaderId = null;
    sessionBestLap = 0;
    timeExpiredAnnounced = false;
    document.getElementById('leaderLapDisplay').innerText = "0";
    updateData();
  }).catch(err => console.error(err));
}