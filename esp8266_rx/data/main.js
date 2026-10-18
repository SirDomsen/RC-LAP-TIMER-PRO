const MAX_CARS = 15;
let ttsEnabled = true;
let countdownTimeouts = [];
window.UI_VERSION = '3.0.1'; 
let sectorAudioEnabled = JSON.parse(localStorage.getItem('sectorAudioEnabled') ?? 'true');
let socket = null;

let currentMode = 0;
let targetLaps = 0;
let maxLapInRace = 0;
let isSessionActive = false;
let isSessionFinished = false;
let previousLaps = {};
let previousBestLaps = {};
let lastAnnouncedLapMap = {}; 
let carNamesMap = {};
let audioUnmuted = false;
let lastLapsData = [];
let awaitingBaseCrossMap = {};

let announced60 = false;
let announced30 = false;
let announced10 = false;
let timeExpiredAnnounced = false;

let localSecondsLeft = -1;
let localTimerInterval = null;
let isUpdatingData = false; 

let currentSortColumn = 'default'; 
let currentSortDir = 'asc';        

let statsSortColumn = 'default';   
let statsSortDir = 'desc';         
let rawDriverLapsData = [];

let currentLeaderId = null;
let sessionBestLap = 0;
let lapHistoryMap = {}; 
let statusCarsMap = {};
let activeStatsCarId = null;
let serverMillisOffset = 0;
let currentTab = 'training';
let announceLapTimesInRace = JSON.parse(localStorage.getItem('announceLapTimesInRace') ?? 'true');

function syncTimeWithServerOnce() {
  let nowSec = Math.floor(Date.now() / 1000);
  fetch('/api/synctime?ts=' + nowSec, { 
    method: 'POST'
  }).catch(e => console.error("Einmaliger Zeit-Sync fehlgeschlagen:", e));
}

async function loadVersion() {
  const fwElem = document.getElementById('firmware-version');
  const uiElem = document.getElementById('ui-version');
  
  if (uiElem) uiElem.textContent = UI_VERSION;

  try {
    let response = await fetch('/api/version');
    if (response.ok) {
      let data = await response.json();
      if (fwElem) fwElem.textContent = data.firmware;
    } else {
      if (fwElem) fwElem.textContent = "--";
    }
  } catch (err) {
    console.warn('Firmware-Version konnte nicht geladen werden:', err);
    if (fwElem) fwElem.textContent = "--";
  }
}

async function loadCarNames() {
  try {
    let response = await fetch('/api/names');
    let names = await response.json();
    for (let i = 1; i <= MAX_CARS; i++) {
      if (names[i]) carNamesMap[i] = names[i];
    }
  } catch (e) {
    console.error("Fehler beim Laden der Namen:", e);
  }
}

function updateTimerDisplayString(sec) {
  let timerElem = document.getElementById('timerDisplay');
  if (!timerElem) return;

  if (isSessionFinished) {
    timerElem.innerText = "ZIEL";
    return;
  }

  if (timeExpiredAnnounced) {
    timerElem.innerText = "LETZTE RUNDE";
    return;
  }

  if (sec >= 0) {
    let m = Math.floor(sec / 60);
    let s = sec % 60;
    timerElem.innerText = (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
  } else {
    timerElem.innerText = "--:--";
  }
}

function startLocalTimerTicker() {
  if (localTimerInterval) clearInterval(localTimerInterval);

  localTimerInterval = setInterval(() => {
    if (localSecondsLeft > 0 && isSessionActive && !isSessionFinished) {
      localSecondsLeft--;
      updateTimerDisplayString(localSecondsLeft);

      if (localSecondsLeft === 60 && !announced60) {
        announced60 = true;
        speak("Noch eine Minute!", true);
      } else if (localSecondsLeft === 30 && !announced30) {
        announced30 = true;
        speak("Noch 30 Sekunden!", true);
      } else if (localSecondsLeft === 10 && !announced10) {
        announced10 = true;
        speak("Noch 10 Sekunden!", true);
      } else if (localSecondsLeft === 0) {
        if (currentMode === 1 || currentMode === 3) {
          if (!timeExpiredAnnounced) {
            timeExpiredAnnounced = true;
            updateTimerDisplayString(0);
            speak("Die Zeit ist abgelaufen! Letzte Runde!", true);
          }
        } else {
          finishSessionNow();
        }
      }
    }
  }, 1000);
}

async function updateData() {
  if (isUpdatingData) return;
  isUpdatingData = true;

  try {
    let response = await fetch('/api/laps');
    let data = await response.json();
    
    lastLapsData = data;
    let html = '';
    let isDrag = (currentMode === 4);

    let thSz4 = document.getElementById('th-sz4-label')?.closest('th');
    if (thSz4) thSz4.style.display = isDrag ? 'none' : '';

    if (data.length === 0) {
      html = `<tr><td colspan="${isDrag ? 8 : 9}">Keine Autos erfasst</td></tr>`;
      currentLeaderId = null;
    } else {
      let leaderSearch = [...data];
      if (currentMode === 1 || currentMode === 4) {
        leaderSearch.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
      } else {
        leaderSearch.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);
      }

      let leaderLaps = leaderSearch[0].completedLaps;
      let leaderElem = document.getElementById('leaderLapDisplay');
      if (leaderElem) leaderElem.innerText = leaderLaps;

      let newLeader = leaderSearch[0];
      if ((currentMode === 2 || currentMode === 3) && isSessionActive && !isSessionFinished) {
        if (currentLeaderId === null && newLeader && newLeader.completedLaps > 0) {
          currentLeaderId = newLeader.id;
        }
      }

      let displayData = sortLapData(data);

      let thLapsLabel = document.getElementById('th-laps-label');
      if (thLapsLabel) thLapsLabel.innerText = isDrag ? "Läufe" : "Runde";
      let thLastLabel = document.getElementById('th-last-label');
      if (thLastLabel) thLastLabel.innerText = isDrag ? "Laufzeit" : "Letzte Zeit";

      let minSz1 = 0, minSz2 = 0, minSz3 = 0, minSz4 = 0;
      let minBestLap = 0;
      let activeCarsWithLaps = 0;

      data.forEach(r => {
        if (r.completedLaps > 0 || r.bestLap > 0) activeCarsWithLaps++;

        let b1 = r.bestSz1 > 0 ? r.bestSz1 : r.sz1;
        if (b1 > 0 && (minSz1 === 0 || b1 < minSz1)) minSz1 = b1;

        let b2 = r.bestSz2 > 0 ? r.bestSz2 : r.sz2;
        if (b2 > 0 && (minSz2 === 0 || b2 < minSz2)) minSz2 = b2;

        let b3 = r.bestSz3 > 0 ? r.bestSz3 : r.sz3;
        if (b3 > 0 && (minSz3 === 0 || b3 < minSz3)) minSz3 = b3;

        let b4 = r.bestSz4 > 0 ? r.bestSz4 : r.sz4;
        if (b4 > 0 && (minSz4 === 0 || b4 < minSz4)) minSz4 = b4;

        if (r.bestLap > 0 && (minBestLap === 0 || r.bestLap < minBestLap)) minBestLap = r.bestLap;
      });

      if (minBestLap > 0) {
        sessionBestLap = minBestLap;
      }

      displayData.forEach((row) => {
        carNamesMap[row.id] = { driver: row.driver, car: row.car };
        let defaultPosition = row._defaultPos;
        
        checkLapEvents(row, leaderLaps, defaultPosition, minBestLap);

        let lastDisplay = '---';
        let lastClass = 'lap-time-neutral';
        let isRaceMode = (currentMode === 2 || currentMode === 3 || currentMode === 4);

        if (serverMillisOffset > 0 && row.lastSeenMs > 0 && serverMillisOffset >= row.lastSeenMs && !isSessionFinished) {
          let idleMs = serverMillisOffset - row.lastSeenMs;
          let pauseThreshold = Math.max(35000, (row.lastLap > 0 ? row.lastLap * 2.5 : 35000));
          let hasStarted = (row.completedLaps > 0 || row.sz1 > 0 || row.sz2 > 0 || row.sz3 > 0 || awaitingBaseCrossMap[row.id]);

          if (hasStarted && idleMs > pauseThreshold) {
            statusCarsMap[row.id] = (isRaceMode && currentMode !== 4) ? 'box' : 'pause';
            awaitingBaseCrossMap[row.id] = false;
          }
        }

        if (statusCarsMap[row.id] === 'pause' || statusCarsMap[row.id] === 'box') {
          if (isRaceMode && currentMode !== 4) {
            lastDisplay = '<span class="pit-badge">Box</span>';
          } else if (currentMode !== 4) {
            lastDisplay = '<span class="pause-badge">Pause</span>';
          }
        } else if (awaitingBaseCrossMap[row.id]) {
          lastDisplay = '<span class="pit-badge" style="color:var(--neon-green); border-color:var(--neon-green);">Auf Strecke</span>';
        } else if (row.lastLap > 0) {
          lastDisplay = (row.lastLap / 1000).toFixed(2) + 's';
          
          let isOverall = (activeCarsWithLaps > 1 && minBestLap > 0 && row.lastLap <= minBestLap);
          let isPB = (row.bestLap > 0 && row.lastLap <= row.bestLap);

          if (isOverall && isPB) {
            lastClass = 'lap-time-purple';
          } else if (isPB) {
            lastClass = 'lap-time-green';
          } else {
            lastClass = 'lap-time-neutral';
          }
        } else if (currentMode === 4 && (row.sz1 > 0 || row.sz2 > 0)) {
          lastDisplay = '<span class="pit-badge" style="color:var(--neon-green); border-color:var(--neon-green);">Auf Strecke</span>';
        }

        let bestDisplay = row.bestLap > 0 ? (row.bestLap / 1000).toFixed(2) + 's' : '---';
        let bestClass = 'lap-time-green';
        if (activeCarsWithLaps > 1 && minBestLap > 0 && row.bestLap > 0 && row.bestLap <= minBestLap) {
          bestClass = 'lap-time-purple';
        }

        let formatSectorCell = (sectorNum, szVal, splitVal) => {
          let valDisp = szVal > 0 ? (szVal / 1000).toFixed(2) + 's' : '---';
          if (currentMode === 4) {
            let splitDisp = splitVal > 0 ? (splitVal / 1000).toFixed(2) + 's' : '---';
            return `<div class="drag-cell"><span class="drag-split">Spl: ${splitDisp}</span><span class="drag-sector">${valDisp}</span></div>`;
          }
          return valDisp;
        };

        let sz1Disp = formatSectorCell(1, row.sz1, row.split1);
        let sz2Disp = formatSectorCell(2, row.sz2, row.split2);
        let sz3Disp = formatSectorCell(3, row.sz3, row.split3);
        let sz4Disp = formatSectorCell(4, row.sz4, 0);

        let getSectorClass = (val, bestVal, overallMin) => {
          if (val === 0) return 'sector-cyan';
          if (overallMin > 0 && val <= overallMin) {
            return 'sector-best-purple';
          } else if (bestVal > 0 && val <= bestVal) {
            return 'sector-best-green';
          }
          return 'sector-yellow';
        };

        let sz1Class = getSectorClass(row.sz1, row.bestSz1, minSz1);
        let sz2Class = getSectorClass(row.sz2, row.bestSz2, minSz2);
        let sz3Class = getSectorClass(row.sz3, row.bestSz3, minSz3);
        let sz4Class = getSectorClass(row.sz4, row.bestSz4, minSz4);

        let lapsDisplay = row.completedLaps;
        if (currentMode === 4) {
          lapsDisplay = row.completedLaps > 0 ? (row.completedLaps + ' Lauf') : '0';
        } else if (targetLaps > 0) {
          lapsDisplay += ' / ' + targetLaps;
        }

        html += `<tr>
          <td><strong>P${defaultPosition}</strong></td>
          <td><span class="driver-name" onclick="openDriverStats(${row.id})">${row.driver}</span> <br><small style="color:var(--text-muted);">(${row.car} - ID ${row.id})</small></td>
          <td>${lapsDisplay}</td>
          <td id="sz-cell-${row.id}-1" class="${sz1Class}">${sz1Disp}</td>
          <td id="sz-cell-${row.id}-2" class="${sz2Class}">${sz2Disp}</td>
          <td id="sz-cell-${row.id}-3" class="${sz3Class}">${sz3Disp}</td>
          <td id="sz-cell-${row.id}-4" class="${sz4Class}" style="${isDrag ? 'display:none;' : ''}">${sz4Disp}</td>
          <td class="${lastClass}">${lastDisplay}</td>
          <td class="${bestClass}" style="font-weight:bold;">${bestDisplay}</td>
        </tr>`;
      });
    }
    let lapTable = document.getElementById('lapTable');
    if (lapTable) lapTable.innerHTML = html;
    updateTTSFilterDisplay();
  } catch (err) {
    console.error("Error fetching lap data:", err);
  } finally {
    isUpdatingData = false;
  }

  try {
    let resStations = await fetch('/api/stations');
    if (resStations.ok) {
      let stations = await resStations.json();
      let sz1 = document.getElementById('sz1Status');
      let sz2 = document.getElementById('sz2Status');
      let sz3 = document.getElementById('sz3Status');
      
      let getStatusColor = (online, batLow) => {
        if (!online) return 'var(--neon-red)';
        if (batLow) return 'var(--neon-orange)';
        return 'var(--neon-green)';
      };

      if (sz1) sz1.style.color = getStatusColor(stations.sz1, stations.sz1Bat);
      if (sz2) sz2.style.color = getStatusColor(stations.sz2, stations.sz2Bat);
      if (sz3) sz3.style.color = getStatusColor(stations.sz3, stations.sz3Bat);
    }
  } catch (err) {
    console.error("Error fetching station status:", err);
  }

  try {
    let r = await fetch('/api/timeleft');
    let info = await r.json();

    currentMode = info.mode;
    serverMillisOffset = info.serverMillis || 0;
    let modeText = "Freies Training";
    if (currentMode === 1) modeText = "Qualifying";
    if (currentMode === 2) modeText = "Rennen (Runden)";
    if (currentMode === 3) modeText = "Rennen (Zeit)";
    if (currentMode === 4) modeText = "Dragrace";
    let modeElem = document.getElementById('modeDisplay');
    if (modeElem) modeElem.innerText = modeText;

    let serverSecondsLeft = info.secondsLeft;

    if (serverSecondsLeft >= 0) {
      if (localSecondsLeft === -1 || Math.abs(localSecondsLeft - serverSecondsLeft) > 1) {
        localSecondsLeft = serverSecondsLeft;
        updateTimerDisplayString(localSecondsLeft);
      }

      if (serverSecondsLeft === 0 && (currentMode === 1 || currentMode === 3)) {
        if (!timeExpiredAnnounced) {
          timeExpiredAnnounced = true;
          updateTimerDisplayString(0);
          speak("Die Zeit ist abgelaufen! Letzte Runde!", true);
        }
        if (info.allFinished) {
          finishSessionNow();
        }
      }
    } else {
      localSecondsLeft = -1;
      updateTimerDisplayString(-1);
    }
  } catch (err) {
    console.error("Error fetching time left:", err);
  }

  try {
    let r = await fetch('/api/recent');
    let ids = await r.json();
    let html = '';
    ids.forEach(id => {
      if (id > 0) {
        let title = carNamesMap[id] ? (carNamesMap[id].driver + " (" + carNamesMap[id].car + ")") : ("Auto " + id);
        html += `<div class="recent-item active" title="${title}">ID ${id}</div>`;
      } else {
        html += `<div class="recent-item">-</div>`;
      }
    });
    let recentCont = document.getElementById('recentIdsContainer');
    if (recentCont) recentCont.innerHTML = html;
  } catch (err) {
    console.error("Error fetching recent IDs:", err);
  }
}

function finishSessionNow() {
  if (isSessionFinished) return;
  isSessionFinished = true;
  isSessionActive = false;
  localSecondsLeft = 0;

  let timerElem = document.getElementById('timerDisplay');
  if (timerElem) timerElem.innerText = "ZIEL";

  if (currentMode === 1) {
    let bestDriver = null;
    let bestCar = null;
    let bestTimeMs = 0;
    let participantCount = 0;

    lastLapsData.forEach(car => {
      if (car.completedLaps > 0 || car.bestLap > 0) { participantCount++; }
      if (car.bestLap > 0 && (bestTimeMs === 0 || car.bestLap < bestTimeMs)) {
        bestTimeMs = car.bestLap;
        bestDriver = car.driver || (carNamesMap[car.id]?.driver || ("Fahrer " + car.id));
        bestCar = car.car || (carNamesMap[car.id]?.car || ("Auto " + car.id));
      }
    });

    if (participantCount > 1 && bestDriver && bestTimeMs > 0) {
      let timeSec = (bestTimeMs / 1000).toFixed(2).replace('.', ',');
      speak("Das Qualifying ist beendet! " + bestDriver + " auf " + bestCar + " holt die Pole Position mit " + timeSec, false);
    } else if (participantCount === 1 && bestDriver && bestTimeMs > 0) {
      let timeSec = (bestTimeMs / 1000).toFixed(2).replace('.', ',');
      speak("Das Qualifying ist beendet! " + bestDriver + " auf " + bestCar + ", beste Zeit: " + timeSec, false);
    } else {
      speak("Das Qualifying ist beendet!", false);
    }
    if (typeof window.autoSaveSessionResult === 'function') {
      window.autoSaveSessionResult(1, "Qualifying");
    }
  } else if (currentMode === 2 || currentMode === 3 || currentMode === 4) {
    announceRaceResults();
  } else {
    speak("Die Zeit ist abgelaufen! Durchgang beendet!", false);
  }
}

let pollTimeout = null;
let isPolling = false;

// FIX: Smart Polling entlastet den ESP8266 wenn das Fenster inaktiv ist
async function pollData() {
  if (isPolling) return;
  
  if (document.hidden) {
    clearTimeout(pollTimeout);
    pollTimeout = setTimeout(pollData, 3000);
    return;
  }
  
  isPolling = true;
  try {
    let mConfig = document.getElementById('configModal');
    let mStatsOverview = document.getElementById('statsOverviewModal');
    let mStatsDriver = document.getElementById('statsModal');
    
    let isModalOpen = (mConfig && mConfig.style.display === 'block') ||
                      (mStatsOverview && mStatsOverview.style.display === 'block') ||
                      (mStatsDriver && mStatsDriver.style.display === 'block');
                      
    if (!isModalOpen) {
      await updateData();
    }
  } catch (e) {
    console.error("Polling-Fehler:", e);
  } finally {
    isPolling = false;
    clearTimeout(pollTimeout);
    
    let randomJitter = Math.floor(Math.random() * 1500);
    pollTimeout = setTimeout(pollData, 5000 + randomJitter);
  }
}

function handleBaseCrossEvent(msg) {
  let carId = msg.carId;
  let driverName = msg.driver || (carNamesMap[carId]?.driver) || ("Fahrer " + carId);
  
  if (statusCarsMap[carId] === 'box') {
    if (isCarAllowedForTTS(carId)) speak(driverName + " verlässt die Box");
  } else if (statusCarsMap[carId] === 'pause') {
    if (isCarAllowedForTTS(carId)) speak(driverName + " wieder gestartet");
  }

  awaitingBaseCrossMap[carId] = false;
  statusCarsMap[carId] = null;
  updateData();
}

function handleSectorTrackEntryTTS(carId) {
  if (currentMode === 4) return;

  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);

  if (awaitingBaseCrossMap[carId]) {
    return;
  }

  if (previousLaps[carId] === undefined || previousLaps[carId] === 0) {
    awaitingBaseCrossMap[carId] = true;
    previousLaps[carId] = 0;
    previousBestLaps[carId] = 0;
    lapHistoryMap[carId] = [];
    statusCarsMap[carId] = 'on_track';
    if (isCarAllowedForTTS(carId)) {
      speak(driverName + " gestartet!");
    }
  } else if (statusCarsMap[carId] === 'pause') {
    awaitingBaseCrossMap[carId] = true;
    statusCarsMap[carId] = 'on_track';
    if (isCarAllowedForTTS(carId)) {
      speak(driverName + " wieder gestartet");
    }
  } else if (statusCarsMap[carId] === 'box') {
    awaitingBaseCrossMap[carId] = true;
    statusCarsMap[carId] = 'on_track';
    if (isCarAllowedForTTS(carId)) {
      speak(driverName + " verlässt die Box");
    }
  }
}

function checkLapEvents(car, currentLeaderLaps, position, minBestLap = 0) {
  let carId = car.id;
  let currentLaps = car.completedLaps;
  let prevLaps = previousLaps[carId];
  let prevBest = previousBestLaps[carId] || 0;
  let driverName = car.driver || (carNamesMap[carId]?.driver) || ("Fahrer " + carId);

  if (prevLaps === undefined) {
    previousLaps[carId] = currentLaps;
    previousBestLaps[carId] = car.bestLap || 0;
    lapHistoryMap[carId] = [];
    statusCarsMap[carId] = null;
    awaitingBaseCrossMap[carId] = false;

    if (currentMode !== 4 && isCarAllowedForTTS(carId)) {
      speak(driverName + " gestartet!");
    }
    return;
  }

  if (currentLaps > prevLaps) {
    let completedLapNum = currentLaps;
    previousLaps[carId] = currentLaps;
    awaitingBaseCrossMap[carId] = false;

    if (lastAnnouncedLapMap[carId] === completedLapNum) {
      return;
    }
    lastAnnouncedLapMap[carId] = completedLapNum;

    let lastLapMs = car.lastLap;
    let history = lapHistoryMap[carId] || [];

    if (statusCarsMap[carId]) {
      statusCarsMap[carId] = null;
    }

    if (lastLapMs > 0) {
      history.push(lastLapMs);
      if (history.length > 5) history.shift();
      lapHistoryMap[carId] = history;
    }

    if (currentMode === 2 && targetLaps > 0 && currentLaps >= targetLaps && !isSessionFinished) {
      isSessionFinished = true;
      isSessionActive = false;
      announceRaceResults();
      return;
    }

    let leadChangeOccurred = false;
    if (currentMode >= 2 && isSessionActive && !isSessionFinished) {
      if (currentLeaderId !== null && currentLeaderId !== carId) {
        let leaderCar = lastLapsData.find(c => c.id === currentLeaderId);
        if (leaderCar && currentLaps > leaderCar.completedLaps) {
          currentLeaderId = carId;
          leadChangeOccurred = true;
          speak("Neuer Führender! " + driverName + " übernimmt die Spitze!", true);
        }
      }
    }

    let isPersonalBest = false;
    let isAbsoluteBest = false;

    if (minBestLap > 0 && (sessionBestLap === 0 || minBestLap < sessionBestLap)) {
      sessionBestLap = minBestLap;
    }

    if (completedLapNum > 1 && lastLapMs > 0) {
      if (car.bestLap > 0 && lastLapMs <= car.bestLap && (prevBest === 0 || lastLapMs < prevBest)) {
        isPersonalBest = true;
        previousBestLaps[carId] = lastLapMs;
      }

      if (isPersonalBest && sessionBestLap > 0 && lastLapMs <= sessionBestLap) {
        sessionBestLap = lastLapMs;
        isAbsoluteBest = true;
      } else if (isPersonalBest && sessionBestLap === 0) {
        sessionBestLap = lastLapMs;
        isAbsoluteBest = true;
      }
    } else if (completedLapNum === 1 && car.bestLap > 0) {
      previousBestLaps[carId] = car.bestLap;
      if (sessionBestLap === 0 || car.bestLap < sessionBestLap) {
        sessionBestLap = car.bestLap;
      }
    }

    if (!isSessionFinished && car.lastLap > 0 && isCarAllowedForTTS(carId) && !leadChangeOccurred && currentMode !== 4) {
      let timeSec = (car.lastLap / 1000).toFixed(2).replace('.', ',');

      let posText = ", ";
      if (currentMode !== 0 && position > 0) {
        posText = ", Platz " + position + ", ";
      }

      let lapAnnouncement = "";
      let isRaceMode = (currentMode >= 2);

      if (isRaceMode && !announceLapTimesInRace) {
        if (isAbsoluteBest) {
          lapAnnouncement = driverName + posText + "schnellste Rennrunde! " + timeSec;
        } else if (isPersonalBest) {
          lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        } else {
          lapAnnouncement = driverName + posText;
        }
      } else {
        if (isAbsoluteBest) {
          lapAnnouncement = driverName + posText + "schnellste Rennrunde! " + timeSec;
        } else if (isPersonalBest) {
          lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        } else {
          lapAnnouncement = driverName + posText + timeSec;
        }
      }

      speak(lapAnnouncement);
    }
  }
}

window.addEventListener('DOMContentLoaded', () => {
  loadVersion();
  syncTimeWithServerOnce();
  initWebSocket();
  loadCarNames().then(() => updateTTSFilterDisplay());
  startLocalTimerTicker();
  pollData();
});

window.onclick = function(event) {
  if (event.target === document.getElementById('configModal')) closeConfigModal();
  if (event.target === document.getElementById('statsModal')) closeStatsModal();
  if (event.target === document.getElementById('statsOverviewModal')) closeStatsOverviewModal();
};

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    closeConfigModal();
    closeStatsModal();
    closeStatsOverviewModal();
  }
});

document.addEventListener('click', function() { if (!audioUnmuted) unmuteIOSAudio(); }, { once: true });