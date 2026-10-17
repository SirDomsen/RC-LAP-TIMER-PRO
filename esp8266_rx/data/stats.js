let currentStatsModeFilter = 0;
let modeLapsCache = [];
let modeSortColumn = 'default';
let modeSortDir = 'desc';

function announceRaceResults() {
  if (!lastLapsData || lastLapsData.length === 0) return;

  let activeCars = lastLapsData.filter(car => car.completedLaps > 0);
  if (activeCars.length === 0) {
    speak("Das Rennen ist beendet!", true);
    return;
  }

  activeCars.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);

  let winner = activeCars[0];
  let p2 = activeCars[1];
  let p3 = activeCars[2];

  let winnerDriver = winner.driver || (carNamesMap[winner.id]?.driver || ("Fahrer " + winner.id));
  let winnerCar = winner.car || (carNamesMap[winner.id]?.car || ("Auto " + winner.id));

  let podiumText = "Ziel! Das Rennen ist beendet! Herzlichen Glückwunsch an " + winnerDriver + " auf " + winnerCar + " zum Sieg!";

  if (p2 && p3) {
    let p2Driver = p2.driver || (carNamesMap[p2.id]?.driver || ("Fahrer " + p2.id));
    let p3Driver = p3.driver || (carNamesMap[p3.id]?.driver || ("Fahrer " + p3.id));
    podiumText += " Platz 2 geht an " + p2Driver + ", und Platz 3 an " + p3Driver + "!";
  } else if (p2) {
    let p2Driver = p2.driver || (carNamesMap[p2.id]?.driver || ("Fahrer " + p2.id));
    podiumText += " Platz 2 geht an " + p2Driver + "!";
  }

  speak(podiumText, true);

  if (activeCars.length > 3) {
    let restText = "Auf den weiteren Plätzen: ";
    for (let i = 3; i < activeCars.length; i++) {
      let pos = i + 1;
      let driverName = activeCars[i].driver || (carNamesMap[activeCars[i].id]?.driver || ("Fahrer " + activeCars[i].id));
      restText += "Platz " + pos + " " + driverName + ". ";
    }
    speak(restText, false);
  }

  autoSaveSessionResult(currentMode, (currentMode === 4 ? "Dragrace" : "Rennen"));
}

window.autoSaveSessionResult = function(mode, modeName) {
  if (!lastLapsData || lastLapsData.length === 0) return;
  let activeCars = lastLapsData.filter(car => car.completedLaps > 0 || car.bestLap > 0);
  if (activeCars.length === 0) return;

  let sorted = [...activeCars];
  if (mode === 1 || mode === 4) {
    sorted.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
  } else {
    sorted.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);
  }

  let results = sorted.map((car, idx) => ({
    pos: idx + 1,
    id: car.id,
    driver: car.driver || (carNamesMap[car.id]?.driver || ("Fahrer " + car.id)),
    car: car.car || (carNamesMap[car.id]?.car || ("Auto " + car.id)),
    laps: car.completedLaps || 0,
    bestLap: car.bestLap || 0
  }));

  let nowSec = Math.floor(Date.now() / 1000);
  let startSec = window.sessionStartTs ? window.sessionStartTs : (nowSec - 300);

  let sessionData = {
    ts: nowSec,
    startTs: startSec,
    endTs: nowSec,
    mode: mode,
    modeName: modeName,
    results: results
  };

  fetch('/api/savesessionresult', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'data=' + encodeURIComponent(JSON.stringify(sessionData))
  }).catch(err => console.error("Fehler beim Speichern der Session:", err));
};

function sortTable(column) {
  if (currentSortColumn === column) {
    currentSortDir = (currentSortDir === 'asc') ? 'desc' : 'asc';
  } else {
    currentSortColumn = column;
    currentSortDir = 'asc';
  }
  updateSortIcons();
  updateData();
}

function updateSortIcons() {
  const columns = ['default', 'driver', 'laps', 'sz1', 'sz2', 'sz3', 'sz4', 'lastLap', 'bestLap'];
  columns.forEach(col => {
    let iconElem = document.getElementById('sort-icon-' + col);
    if (!iconElem) return;
    if (currentSortColumn === col) {
      iconElem.innerText = currentSortDir === 'asc' ? '▲' : '▼';
    } else {
      iconElem.innerText = '';
    }
  });
}

function sortLapData(data) {
  let defaultSorted = [...data];
  if (currentMode === 1 || currentMode === 4) {
    defaultSorted.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
  } else {
    defaultSorted.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);
  }

  defaultSorted.forEach((item, index) => {
    item._defaultPos = index + 1;
  });

  if (currentSortColumn === 'default') {
    if (currentSortDir === 'desc') {
      defaultSorted.reverse();
    }
    return defaultSorted;
  }

  let sorted = [...defaultSorted];
  sorted.sort((a, b) => {
    let valA, valB;
    if (currentSortColumn === 'driver') {
      valA = (a.driver || '').toLowerCase();
      valB = (b.driver || '').toLowerCase();
      return currentSortDir === 'asc' ? valA.localeCompare(valB) : valB.localeCompare(valA);
    } else if (currentSortColumn === 'laps') {
      valA = a.completedLaps || 0;
      valB = b.completedLaps || 0;
    } else if (currentSortColumn === 'sz1') {
      valA = a.sz1 > 0 ? a.sz1 : 99999999;
      valB = b.sz1 > 0 ? b.sz1 : 99999999;
    } else if (currentSortColumn === 'sz2') {
      valA = a.sz2 > 0 ? a.sz2 : 99999999;
      valB = b.sz2 > 0 ? b.sz2 : 99999999;
    } else if (currentSortColumn === 'sz3') {
      valA = a.sz3 > 0 ? a.sz3 : 99999999;
      valB = b.sz3 > 0 ? b.sz3 : 99999999;
    } else if (currentSortColumn === 'sz4') {
      valA = a.sz4 > 0 ? a.sz4 : 99999999;
      valB = b.sz4 > 0 ? b.sz4 : 99999999;
    } else if (currentSortColumn === 'lastLap') {
      valA = a.lastLap > 0 ? a.lastLap : 99999999;
      valB = b.lastLap > 0 ? b.lastLap : 99999999;
    } else if (currentSortColumn === 'bestLap') {
      valA = a.bestLap > 0 ? a.bestLap : 99999999;
      valB = b.bestLap > 0 ? b.bestLap : 99999999;
    }

    if (valA < valB) return currentSortDir === 'asc' ? -1 : 1;
    if (valA > valB) return currentSortDir === 'asc' ? 1 : -1;
    return 0;
  });

  return sorted;
}

function switchStatsTab(tab) {
  currentTab = tab;
  document.getElementById('tabDriversBtn').classList.toggle('active', tab === 'drivers');
  document.getElementById('tabTrainingBtn').classList.toggle('active', tab === 'training');
  document.getElementById('tabQualiBtn').classList.toggle('active', tab === 'quali');
  document.getElementById('tabRaceBtn').classList.toggle('active', tab === 'race');
  document.getElementById('tabDragBtn').classList.toggle('active', tab === 'drag');

  document.getElementById('statsTabDrivers').style.display = (tab === 'drivers') ? 'block' : 'none';
  document.getElementById('statsTabTraining').style.display = (tab === 'training') ? 'block' : 'none';
  document.getElementById('statsTabQuali').style.display = (tab === 'quali') ? 'block' : 'none';
  document.getElementById('statsTabRace').style.display = (tab === 'race') ? 'block' : 'none';
  document.getElementById('statsTabDrag').style.display = (tab === 'drag') ? 'block' : 'none';

  if (tab === 'drivers') {
    renderDriversOverview();
  } else {
    let modeFilter = 0;
    if (tab === 'training') modeFilter = 0;
    else if (tab === 'quali') modeFilter = 1;
    else if (tab === 'race') modeFilter = [2, 3];
    else if (tab === 'drag') modeFilter = 4;

    currentStatsModeFilter = modeFilter;
    loadSessionsOverviewByMode(modeFilter, 'statsTab' + tab.charAt(0).toUpperCase() + tab.slice(1));
  }
}

async function openStatsOverviewModal() {
  document.getElementById('statsOverviewModal').style.display = 'block';
  await loadCarNames();
  switchStatsTab(currentTab);
}

async function renderDriversOverview() {
  let list = document.getElementById('statsTabDrivers');
  list.innerHTML = '<p style="color:var(--text-muted);">Lade Fahrertabelle...</p>';

  let html = '';
  for (let i = 1; i <= MAX_CARS; i++) {
    let driver = carNamesMap[i]?.driver || ("Fahrer " + i);
    let car = carNamesMap[i]?.car || ("Auto " + i);
    
    try {
      let res = await fetch('/api/stats?id=' + i);
      let data = await res.json();
      let lapCount = data.laps ? data.laps.length : 0;
      let best = '---';
      if (lapCount > 0) {
        let minTime = Math.min(...data.laps.map(l => l.time));
        best = (minTime / 1000).toFixed(2) + 's';
      }

      html += `<div class="driver-select-card" onclick="openDriverStats(${i})">
        <div>
          <strong style="color:var(--neon-cyan); font-size:16px;">${driver}</strong>
          <small style="color:var(--text-muted);">(${car} - ID ${i})</small><br>
          <span style="font-size:12px; color:var(--text-muted);">Gespeicherte Runden: ${lapCount}</span>
        </div>
        <div style="text-align:right;">
          <span style="color:var(--neon-green); font-weight:bold; font-size:15px;">${best}</span><br>
          <span style="font-size:11px; color:var(--text-muted);">Bestzeit</span>
        </div>
      </div>`;
    } catch (e) { console.error(e); }
  }
  list.innerHTML = html;
}

async function loadSessionsOverviewByMode(modeFilter, containerId) {
  let container = document.getElementById(containerId);
  container.innerHTML = '<p style="color:var(--text-muted);">Lade Historie...</p>';

  let isDrag = (modeFilter === 4);

  try {
    let res = await fetch('/api/sessions');
    let data = await res.json();
    
    let savedSessions = (data.sessions || []).filter(s => {
      if (Array.isArray(modeFilter)) return modeFilter.includes(s.mode);
      return s.mode === modeFilter;
    });

    let allFlashLaps = [];
    for (let i = 1; i <= MAX_CARS; i++) {
      try {
        let rLaps = await fetch('/api/stats?id=' + i);
        let dLaps = await rLaps.json();
        if (dLaps && dLaps.laps) {
          dLaps.laps.forEach(l => {
            let match = Array.isArray(modeFilter) ? modeFilter.includes(l.mode) : (l.mode === modeFilter);
            if (match) {
              allFlashLaps.push({
                carId: i,
                driver: carNamesMap[i]?.driver || ("Fahrer " + i),
                car: carNamesMap[i]?.car || ("Auto " + i),
                lap: l.lap,
                time: l.time,
                sz1: l.sz1 || 0,
                sz2: l.sz2 || 0,
                sz3: l.sz3 || 0,
                sz4: l.sz4 || 0,
                pos: l.pos || 1,
                mode: l.mode,
                ts: l.ts || 0
              });
            }
          });
        }
      } catch (e) {}
    }

    let orphanLaps = allFlashLaps.filter(l => {
      let lapSec = l.ts < 10000000000 ? l.ts : Math.floor(l.ts / 1000);
      for (let s of savedSessions) {
        let sStart = s.startTs ? (s.startTs < 10000000000 ? s.startTs : Math.floor(s.startTs / 1000)) : (s.ts - 3600);
        let sEnd = s.endTs ? (s.endTs < 10000000000 ? s.endTs : Math.floor(s.endTs / 1000)) : s.ts;
        if (lapSec >= (sStart - 5) && lapSec <= (sEnd + 5)) {
          return false;
        }
      }
      return true;
    });

    let orphanSessions = [];
    if (orphanLaps.length > 0) {
      orphanSessions = clusterLapsIntoSessions(orphanLaps, modeFilter);
    }

    let allSessions = [...savedSessions, ...orphanSessions];
    allSessions.sort((a, b) => a.ts - b.ts);

    if (allSessions.length === 0) {
      container.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:25px;">Keine gespeicherten Durchgänge vorhanden.</p>';
      return;
    }

    let roundWord = isDrag ? "Läufe" : "Runden";
    let roundTimeWord = isDrag ? "Laufzeiten" : "Rundenzeiten";

    let html = '';
    allSessions.slice().reverse().forEach(s => {
      let badgeClass = 'session-card';
      let headerColor = 'var(--neon-green)';
      if (s.mode === 1) { badgeClass += ' quali'; headerColor = 'var(--neon-purple)'; }
      else if (s.mode === 2 || s.mode === 3) { badgeClass += ' race'; headerColor = 'var(--neon-cyan)'; }
      else if (s.mode === 4) { badgeClass += ' drag'; headerColor = 'var(--neon-orange)'; }

      let dateStr = formatTimestamp(s.ts);

      let sessionOverallBest = 0;
      (s.results || []).forEach(r => {
        if (r.bestLap > 0 && (sessionOverallBest === 0 || r.bestLap < sessionOverallBest)) {
          sessionOverallBest = r.bestLap;
        }
      });

      let startTs = s.startTs || s.ts;
      let endTs = s.endTs || s.ts;

      html += `<div class="${badgeClass}">
        <div style="display:flex; justify-content:space-between; align-items:center;" onclick="toggleSessionDetails(${s.ts})">
          <div>
            <strong style="color:${headerColor}; font-size:16px;">${s.modeName || "Durchgang"}</strong>
            <span style="font-size:12px; color:var(--text-muted); margin-left:10px;">📅 ${dateStr}</span>
          </div>
          <div style="display:flex; align-items:center; gap:8px;">
            <span id="session_arrow_${s.ts}" style="font-size:12px; color:var(--neon-cyan); cursor:pointer;">Details 🔽</span>
            <button class="btn-icon" onclick="event.stopPropagation(); deleteSession(${s.ts})" title="Durchgang löschen">🗑</button>
          </div>
        </div>
        
        <div id="session_detail_${s.ts}" class="session-details" style="display:none; margin-top:10px;">
          <div class="table-responsive">
            <table>
              <thead>
                <tr>
                  <th>Pos</th>
                  <th>Fahrer</th>
                  <th>Fahrzeug</th>
                  <th>${roundWord}</th>
                  <th>Beste Zeit</th>
                  <th>${roundTimeWord}</th>
                </tr>
              </thead>
              <tbody>`;

      (s.results || []).forEach(r => {
        let isOverallWinner = (sessionOverallBest > 0 && r.bestLap > 0 && r.bestLap <= sessionOverallBest);
        let bestColor = isOverallWinner ? 'var(--neon-purple)' : 'var(--neon-green)';
        let bestStr = r.bestLap > 0 ? (r.bestLap / 1000).toFixed(2) + 's' : '---';
        let driverDetailId = `s_${s.ts}_driver_${r.id}`;

        html += `<tr>
          <td><strong>P${r.pos}</strong></td>
          <td style="color:var(--neon-cyan); font-weight:bold;">${r.driver}</td>
          <td style="color:var(--text-muted);">${r.car}</td>
          <td>${r.laps}</td>
          <td style="color:${bestColor}; font-weight:bold;">${bestStr}</td>
          <td>
            <button class="btn btn-purple" id="btn_arrow_${driverDetailId}" style="padding:2px 8px; font-size:11px; margin:0;" onclick="toggleDriverLapsInSession('${driverDetailId}', ${r.id}, ${startTs}, ${endTs}, ${r.laps}, ${sessionOverallBest}, ${isDrag ? 'true' : 'false'}, ${JSON.stringify(modeFilter)})">Details 🔽</button>
          </td>
        </tr>
        <tr id="${driverDetailId}_tr" style="display:none; background: rgba(0,0,0,0.35);">
          <td colspan="6">
            <div id="${driverDetailId}_content" class="driver-lap-details">Lade ${roundTimeWord}...</div>
          </td>
        </tr>`;
      });

      html += `</tbody></table></div></div></div>`;
    });

    container.innerHTML = html;
  } catch(e) {
    console.error(e);
    container.innerHTML = '<p style="color:var(--neon-red);">Fehler beim Laden der Historie.</p>';
  }
}

function clusterLapsIntoSessions(lapsArray, modeFilter) {
  lapsArray.sort((a, b) => a.ts - b.ts);

  let sessions = [];
  let currentCluster = [];
  let clusterTimeoutMs = 10 * 60 * 1000;

  lapsArray.forEach(l => {
    let lapTsMs = l.ts < 10000000000 ? l.ts * 1000 : l.ts;
    if (currentCluster.length === 0) {
      currentCluster.push(l);
    } else {
      let prevLap = currentCluster[currentCluster.length - 1];
      let prevTsMs = prevLap.ts < 10000000000 ? prevLap.ts * 1000 : prevLap.ts;
      if (lapTsMs - prevTsMs > clusterTimeoutMs) {
        sessions.push(buildSessionFromCluster(currentCluster, modeFilter));
        currentCluster = [l];
      } else {
        currentCluster.push(l);
      }
    }
  });

  if (currentCluster.length > 0) {
    sessions.push(buildSessionFromCluster(currentCluster, modeFilter));
  }

  return sessions;
}

function buildSessionFromCluster(clusterLaps, modeFilter) {
  let carsMap = {};
  clusterLaps.forEach(l => {
    if (!carsMap[l.carId]) {
      carsMap[l.carId] = {
        id: l.carId,
        driver: l.driver,
        car: l.car,
        laps: 0,
        bestLap: 0
      };
    }
    carsMap[l.carId].laps++;
    if (l.time > 0 && (carsMap[l.carId].bestLap === 0 || l.time < carsMap[l.carId].bestLap)) {
      carsMap[l.carId].bestLap = l.time;
    }
  });

  let results = Object.values(carsMap);
  let isRankByBest = (clusterLaps[0].mode === 1 || clusterLaps[0].mode === 4);
  if (isRankByBest) {
    results.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
  } else {
    results.sort((a, b) => b.laps - a.laps || a.bestLap - b.bestLap);
  }

  results.forEach((r, idx) => { r.pos = idx + 1; });

  let modeVal = clusterLaps[0].mode;
  let modeName = "Freies Training";
  if (modeVal === 1) modeName = "Qualifying";
  if (modeVal === 2) modeName = "Rennen (Runden)";
  if (modeVal === 3) modeName = "Rennen (Zeit)";
  if (modeVal === 4) modeName = "Dragrace";

  let firstLap = clusterLaps[0];
  let lastLap = clusterLaps[clusterLaps.length - 1];
  let startTs = firstLap.ts < 10000000000 ? firstLap.ts : Math.floor(firstLap.ts / 1000);
  let endTs = lastLap.ts < 10000000000 ? lastLap.ts : Math.floor(lastLap.ts / 1000);

  return {
    ts: endTs,
    startTs: startTs,
    endTs: endTs,
    mode: modeVal,
    modeName: modeName,
    results: results
  };
}

function toggleSessionDetails(ts) {
  let el = document.getElementById('session_detail_' + ts);
  let arrow = document.getElementById('session_arrow_' + ts);
  if (el) {
    let isOpen = (el.style.display === 'block');
    el.style.display = isOpen ? 'none' : 'block';
    if (arrow) arrow.innerText = isOpen ? 'Details 🔽' : 'Details 🔼';
  }
}

async function toggleDriverLapsInSession(elemId, carId, startTs, endTs, targetLapsCount, sessionOverallBest, isDrag, modeFilter) {
  let trElem = document.getElementById(elemId + '_tr');
  let contentElem = document.getElementById(elemId + '_content');
  let btnElem = document.getElementById('btn_arrow_' + elemId);

  if (!trElem || !contentElem) return;

  if (trElem.style.display === 'table-row') {
    trElem.style.display = 'none';
    if (btnElem) btnElem.innerText = 'Details 🔽';
    return;
  }

  trElem.style.display = 'table-row';
  contentElem.style.display = 'block';
  contentElem.innerHTML = 'Lade Zeiten...';
  if (btnElem) btnElem.innerText = 'Details 🔼';

  try {
    let res = await fetch('/api/stats?id=' + carId);
    let data = await res.json();

    if (!data.laps || data.laps.length === 0) {
      contentElem.innerHTML = '<span style="color:var(--text-muted);">Keine Zeiten im Speicher vorhanden.</span>';
      return;
    }

    let startMs = (startTs && startTs > 0) ? (startTs < 10000000000 ? startTs * 1000 : startTs) : 0;
    let endMs = (endTs && endTs > 0) ? (endTs < 10000000000 ? endTs * 1000 : endTs) : 0;

    let modeLaps = data.laps.filter(l => {
      if (Array.isArray(modeFilter)) return modeFilter.includes(l.mode);
      return l.mode === modeFilter;
    });

    modeLaps.sort((a, b) => a.ts - b.ts);

    let candidatesBeforeEnd = modeLaps.filter(l => {
      let lapMs = l.ts < 10000000000 ? l.ts * 1000 : l.ts;
      return endMs > 0 ? (lapMs <= endMs + 5000) : true;
    });

    let runLaps = [];
    if (candidatesBeforeEnd.length > 0) {
      for (let i = candidatesBeforeEnd.length - 1; i >= 0; i--) {
        let l = candidatesBeforeEnd[i];
        let lapMs = l.ts < 10000000000 ? l.ts * 1000 : l.ts;

        if (startMs > 0 && lapMs < (startMs - 5000)) {
          break;
        }

        runLaps.unshift(l);

        if (l.lap === 1) {
          break;
        }
      }
    }

    if (runLaps.length === 0) {
      contentElem.innerHTML = '<span style="color:var(--text-muted);">Keine Rundenzeiten in diesem Durchgang erfasst.</span>';
      return;
    }

    renderEmbeddedLapsTable(contentElem, runLaps, sessionOverallBest || 0, isDrag);
  } catch (e) {
    console.error(e);
    contentElem.innerHTML = '<span style="color:var(--neon-red);">Fehler beim Laden der Zeiten.</span>';
  }
}

function renderEmbeddedLapsTable(containerElem, laps, sessionOverallBest, isDrag) {
  let minSz1 = 0, minSz2 = 0, minSz3 = 0, minSz4 = 0, minTime = 0;
  let maxSz1 = 0, maxSz2 = 0, maxSz3 = 0, maxSz4 = 0, maxTime = 0;

  laps.forEach(l => {
    if (l.sz1 > 0) {
      if (minSz1 === 0 || l.sz1 < minSz1) minSz1 = l.sz1;
      if (maxSz1 === 0 || l.sz1 > maxSz1) maxSz1 = l.sz1;
    }
    if (l.sz2 > 0) {
      if (minSz2 === 0 || l.sz2 < minSz2) minSz2 = l.sz2;
      if (maxSz2 === 0 || l.sz2 > maxSz2) maxSz2 = l.sz2;
    }
    if (l.sz3 > 0) {
      if (minSz3 === 0 || l.sz3 < minSz3) minSz3 = l.sz3;
      if (maxSz3 === 0 || l.sz3 > maxSz3) maxSz3 = l.sz3;
    }
    if (!isDrag && l.sz4 > 0) {
      if (minSz4 === 0 || l.sz4 < minSz4) minSz4 = l.sz4;
      if (maxSz4 === 0 || l.sz4 > maxSz4) maxSz4 = l.sz4;
    }
    if (l.time > 0) {
      if (minTime === 0 || l.time < minTime) minTime = l.time;
      if (maxTime === 0 || l.time > maxTime) maxTime = l.time;
    }
  });

  let allowWorst = (laps.length > 1);
  let effectiveSessionBest = (sessionOverallBest > 0) ? sessionOverallBest : minTime;

  let roundLabel = isDrag ? "Lauf" : "Runde";
  let timeLabel = isDrag ? "Laufzeit" : "Zeit";

  let html = `<div class="table-responsive"><table style="margin:0; font-size:12px;">
    <thead>
      <tr>
        <th>${roundLabel}</th>
        <th>SZ1</th>
        <th>SZ2</th>
        <th>SZ3</th>
        ${isDrag ? '' : '<th>SZ4</th>'}
        <th>${timeLabel}</th>
        <th>Platz</th>
        <th>Uhrzeit</th>
      </tr>
    </thead>
    <tbody>`;

  laps.slice().reverse().forEach(l => {
    let getSectorStyle = (val, bestVal, worstVal) => {
      if (!val || val === 0) return 'color: var(--text-muted);';
      if (val === bestVal) return 'color: var(--neon-purple); font-weight: bold; text-shadow: 0 0 6px rgba(181, 50, 255, 0.6);';
      if (allowWorst && val === worstVal) return 'color: var(--neon-red); font-weight: bold;';
      return 'color: var(--text-main);';
    };

    let getTimeStyle = (val, bestVal, worstVal) => {
      if (!val || val === 0) return 'color: var(--text-muted);';
      if (val <= effectiveSessionBest) {
        return 'color: var(--neon-purple); font-weight: bold; text-shadow: 0 0 6px rgba(181, 50, 255, 0.6);';
      }
      if (val === bestVal) {
        return 'color: var(--neon-green); font-weight: bold;';
      }
      if (allowWorst && val === worstVal) {
        return 'color: var(--neon-red); font-weight: bold;';
      }
      return 'color: var(--text-main);';
    };

    let styleSz1 = getSectorStyle(l.sz1, minSz1, maxSz1);
    let styleSz2 = getSectorStyle(l.sz2, minSz2, maxSz2);
    let styleSz3 = getSectorStyle(l.sz3, minSz3, maxSz3);
    let styleSz4 = isDrag ? '' : getSectorStyle(l.sz4, minSz4, maxSz4);
    let styleTime = getTimeStyle(l.time, minTime, maxTime);

    let formatCell = (szVal, splitVal, style) => {
      let valDisp = szVal > 0 ? (szVal / 1000).toFixed(2) + 's' : '---';
      if (isDrag) {
        let splitDisp = splitVal > 0 ? (splitVal / 1000).toFixed(2) + 's' : '---';
        return `<div class="drag-cell"><span class="drag-split" style="font-size:10px; color:var(--text-muted); display:block;">Spl: ${splitDisp}</span><span class="drag-sector" style="${style}">${valDisp}</span></div>`;
      }
      return `<span style="${style}">${valDisp}</span>`;
    };

    let split1 = l.sz1;
    let split2 = (l.sz1 > 0 && l.sz2 > 0) ? (l.sz1 + l.sz2) : 0;
    let split3 = l.time;

    let sz1Content = formatCell(l.sz1, split1, styleSz1);
    let sz2Content = formatCell(l.sz2, split2, styleSz2);
    let sz3Content = formatCell(l.sz3, split3, styleSz3);
    let sz4Content = isDrag ? '' : formatCell(l.sz4, 0, styleSz4);

    html += `<tr>
      <td>${roundLabel} ${l.lap}</td>
      <td>${sz1Content}</td>
      <td>${sz2Content}</td>
      <td>${sz3Content}</td>
      ${isDrag ? '' : `<td>${sz4Content}</td>`}
      <td style="${styleTime}; font-weight:bold;">${(l.time / 1000).toFixed(2)}s</td>
      <td style="color:var(--neon-orange); font-weight:bold;">${l.pos ? 'P' + l.pos : '---'}</td>
      <td style="color:var(--text-muted);">${formatTimestamp(l.ts)}</td>
    </tr>`;
  });

  html += '</tbody></table></div>';
  containerElem.innerHTML = html;
}

function deleteSingleLapAndReload(carId, lapNum) {
  fetch(`/api/deletelap?id=${carId}&lap=${lapNum}`, { method: 'POST' })
    .then(() => switchStatsTab(currentTab))
    .catch(err => console.error(err));
}

function deleteSession(ts) {
  if (confirm("Möchtest du diesen Durchgang wirklich löschen?")) {
    fetch('/api/deletesession?ts=' + ts, { method: 'POST' })
      .then(() => switchStatsTab(currentTab))
      .catch(e => console.error(e));
  }
}

function openDriverStats(carId) {
  activeStatsCarId = carId;
  statsSortColumn = 'default';
  statsSortDir = 'desc';
  updateStatsSortIcons();

  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);
  document.getElementById('statsModalTitle').innerText = driverName + " (ID " + carId + ")";
  document.getElementById('deleteDriverStatsBtn').innerText = "Statistiken von " + driverName + " löschen";
  
  closeStatsOverviewModal();
  document.getElementById('statsModal').style.display = 'block';

  loadDriverStats(carId);
}

function closeStatsModal() {
  document.getElementById('statsModal').style.display = 'none';
  activeStatsCarId = null;
}

function closeStatsOverviewModal() { 
  document.getElementById('statsOverviewModal').style.display = 'none'; 
}

function backToStatsOverview() { 
  closeStatsModal(); 
  openStatsOverviewModal(); 
}

function formatTimestamp(ts) {
  if (!ts || ts === 0) return '---';
  let tsMs = ts < 10000000000 ? ts * 1000 : ts;
  let d = new Date(tsMs);

  let day = String(d.getDate()).padStart(2, '0');
  let month = String(d.getMonth() + 1).padStart(2, '0');
  let year = d.getFullYear();
  let hours = String(d.getHours()).padStart(2, '0');
  let mins = String(d.getMinutes()).padStart(2, '0');
  let secs = String(d.getSeconds()).padStart(2, '0');
  
  return `${day}.${month}.${year} ${hours}:${mins}:${secs}`;
}

function loadDriverStats(carId) {
  let tbody = document.getElementById('statsTableBody');
  tbody.innerHTML = '<tr><td colspan="5">Lade Daten...</td></tr>';

  fetch('/api/stats?id=' + carId)
    .then(r => r.json())
    .then(data => {
      rawDriverLapsData = data.laps || [];
      renderDriverStatsTable();
    }).catch(err => {
      console.error(err);
      tbody.innerHTML = '<tr><td colspan="5" style="color:var(--neon-red);">Fehler beim Laden.</td></tr>';
    });
}

function renderDriverStatsTable() {
  let tbody = document.getElementById('statsTableBody');
  if (!rawDriverLapsData || rawDriverLapsData.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5">Keine gespeicherten Runden vorhanden.</td></tr>';
    return;
  }

  let minTime = Math.min(...rawDriverLapsData.map(l => l.time));
  let maxTime = Math.max(...rawDriverLapsData.map(l => l.time));
  let allowWorst = (rawDriverLapsData.length > 1);

  let sorted = [...rawDriverLapsData];
  if (statsSortColumn !== 'default') {
    sorted.sort((a, b) => {
      let valA, valB;
      if (statsSortColumn === 'lap') {
        valA = a.lap || 0; valB = b.lap || 0;
      } else if (statsSortColumn === 'time') {
        valA = a.time || 0; valB = b.time || 0;
      } else if (statsSortColumn === 'ts') {
        valA = a.ts || 0; valB = b.ts || 0;
      } else if (statsSortColumn === 'mode') {
        valA = a.mode || 0; valB = b.mode || 0;
      }

      if (valA < valB) return statsSortDir === 'asc' ? -1 : 1;
      if (valA > valB) return statsSortDir === 'asc' ? 1 : -1;
      return 0;
    });
  }

  let html = '';
  sorted.forEach(l => {
    let modeStr = "Training";
    if (l.mode === 1) modeStr = "Quali";
    if (l.mode === 2 || l.mode === 3) modeStr = "Rennen (P" + (l.pos || '-') + ")";
    if (l.mode === 4) modeStr = "Dragrace (P" + (l.pos || '-') + ")";

    let timeSec = (l.time / 1000).toFixed(2) + 's';
    let dateStr = formatTimestamp(l.ts);

    let timeColor = 'var(--text-main)';
    if (l.time === minTime) {
      timeColor = 'var(--neon-purple)';
    } else if (allowWorst && l.time === maxTime) {
      timeColor = 'var(--neon-red)';
    }

    html += `<tr>
      <td>${l.mode === 4 ? 'Lauf' : 'Runde'} ${l.lap}</td>
      <td style="color:${timeColor}; font-weight:bold;">${timeSec}</td>
      <td style="color:var(--text-muted); font-size:12px;">${dateStr}</td>
      <td>${modeStr}</td>
      <td>
        <button class="btn-icon" title="Runde löschen" onclick="deleteSingleLap(${activeStatsCarId}, ${l.lap})">🗑</button>
      </td>
    </tr>`;
  });
  tbody.innerHTML = html;
}

function sortStatsTable(column) {
  if (statsSortColumn === column) {
    statsSortDir = (statsSortDir === 'asc') ? 'desc' : 'asc';
  } else {
    statsSortColumn = column;
    statsSortDir = 'asc';
  }
  updateStatsSortIcons();
  renderDriverStatsTable();
}

function updateStatsSortIcons() {
  const columns = ['lap', 'time', 'ts', 'mode'];
  columns.forEach(col => {
    let iconElem = document.getElementById('sort-stats-icon-' + col);
    if (!iconElem) return;
    if (statsSortColumn === col) {
      iconElem.innerText = statsSortDir === 'asc' ? '▲' : '▼';
    } else {
      iconElem.innerText = '';
    }
  });
}

function deleteSingleLap(carId, lapNum) {
  fetch(`/api/deletelap?id=${carId}&lap=${lapNum}`, { method: 'POST' })
    .then(() => loadDriverStats(carId))
    .catch(err => console.error(err));
}

function confirmDeleteDriverStats() {
  let driverName = carNamesMap[activeStatsCarId]?.driver || ("Fahrer " + activeStatsCarId);
  if (confirm(`Wirklich ALLE gespeicherten Statistiken von ${driverName} dauerhaft löschen?`)) {
    fetch('/api/deletedriverstats?id=' + activeStatsCarId, { method: 'POST' })
      .then(() => { loadDriverStats(activeStatsCarId); }).catch(err => console.error(err));
  }
}

function confirmDeleteAllStats() {
  if (confirm("WARNUNG: Möchtest du wirklich ALLE Statistiken unwiderruflich löschen?")) {
    fetch('/api/deleteallstats', { method: 'POST' })
      .then(() => {
        alert("Alle Statistiken wurden gelöscht.");
        closeStatsOverviewModal();
      }).catch(err => console.error(err));
  }
}

async function downloadStatsBackup() {
  let backupData = {
    version: 2,
    exportedAt: Math.floor(Date.now() / 1000),
    driverStats: {},
    sessions: []
  };

  try {
    for (let i = 1; i <= MAX_CARS; i++) {
      let res = await fetch('/api/stats?id=' + i);
      let data = await res.json();
      if (data && data.laps) {
        backupData.driverStats[i] = data;
      }
    }

    let sRes = await fetch('/api/sessions');
    let sData = await sRes.json();
    if (sData && sData.sessions) {
      backupData.sessions = sData.sessions;
    }

    let dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify(backupData, null, 2));
    let downloadAnchor = document.createElement('a');
    let dateISO = new Date().toISOString().slice(0, 10);
    downloadAnchor.setAttribute("href", dataStr);
    downloadAnchor.setAttribute("download", `rctl_stats_backup_${dateISO}.json`);
    document.body.appendChild(downloadAnchor);
    downloadAnchor.click();
    downloadAnchor.remove();
  } catch (e) {
    alert("Fehler beim Erstellen des Backups: " + e.message);
  }
}

function triggerRestoreBackup() {
  document.getElementById('backupFileInput').click();
}

async function handleRestoreBackup(event) {
  let file = event.target.files[0];
  if (!file) return;

  if (!confirm("Soll das Backup eingespielt werden?")) return;

  let formData = new FormData();
  formData.append('data', file, 'backup.json');

  try {
    let res = await fetch('/api/upload', { method: 'POST', body: formData });
    if (res.ok) {
      alert("Backup erfolgreich wiederhergestellt!");
      switchStatsTab(currentTab);
    } else {
      alert("Fehler beim Hochladen des Backups.");
    }
  } catch (err) {
    alert("Fehler beim Einlesen: " + err.message);
  }
  
  event.target.value = '';
}