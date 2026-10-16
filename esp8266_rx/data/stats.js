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
}

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

function getDynamicColorStyle(val, personalBest, overallBest, worstVal) {
  if (!val || val === 0) return 'color: var(--text-muted);';
  if ((overallBest > 0 && val <= overallBest) || (overallBest === 0 && personalBest > 0 && val <= personalBest)) {
    return 'color: var(--neon-purple); font-weight: bold; text-shadow: 0 0 6px rgba(181, 50, 255, 0.6);';
  }
  if (personalBest > 0 && val <= personalBest) {
    return 'color: var(--neon-green); font-weight: bold;';
  }
  if (worstVal > 0 && val >= worstVal) {
    return 'color: var(--neon-red); font-weight: bold;';
  }
  return 'color: var(--text-main);';
}

async function loadSessionsOverviewByMode(modeFilter, containerId) {
  let container = document.getElementById(containerId);
  container.innerHTML = '<p style="color:var(--text-muted);">Lade Historie...</p>';

  try {
    let res = await fetch('/api/sessions');
    let data = await res.json();
    
    let filteredSessions = (data.sessions || []).filter(s => {
      if (Array.isArray(modeFilter)) return modeFilter.includes(s.mode);
      return s.mode === modeFilter;
    });

    if (filteredSessions.length === 0) {
      await loadDirectLapsFallback(modeFilter, containerId);
      return;
    }

    let html = '';
    filteredSessions.slice().reverse().forEach(s => {
      let isQuali = (s.mode === 1);
      let isRace = (s.mode === 2 || s.mode === 3);
      let isDrag = (s.mode === 4);
      
      let badgeClass = 'session-card';
      let headerColor = 'var(--neon-green)';
      if (isQuali) { badgeClass += ' quali'; headerColor = 'var(--neon-purple)'; }
      else if (isRace) { badgeClass += ' race'; headerColor = 'var(--neon-cyan)'; }
      else if (isDrag) { badgeClass += ' drag'; headerColor = 'var(--neon-orange)'; }

      let dateStr = formatTimestamp(s.ts);

      let sessionOverallBest = 0;
      (s.results || []).forEach(r => {
        if (r.bestLap > 0 && (sessionOverallBest === 0 || r.bestLap < sessionOverallBest)) {
          sessionOverallBest = r.bestLap;
        }
      });

      html += `<div class="${badgeClass}">
        <div style="display:flex; justify-content:space-between; align-items:center;" onclick="toggleSessionDetails(${s.ts})">
          <div>
            <strong style="color:${headerColor}; font-size:16px;">${s.modeName}</strong>
            <span style="font-size:12px; color:var(--text-muted); margin-left:10px;">📅 ${dateStr}</span>
          </div>
          <div style="display:flex; align-items:center; gap:8px;">
            <span id="session_arrow_${s.ts}" style="font-size:12px; color:var(--neon-cyan); cursor:pointer;">Details 🔽</span>
            <button class="btn-icon" onclick="event.stopPropagation(); deleteSession(${s.ts})" title="Löschen">🗑</button>
          </div>
        </div>
        
        <div id="session_detail_${s.ts}" class="session-details">
          <div class="table-responsive">
            <table>
              <thead>
                <tr>
                  <th>Pos</th>
                  <th>Fahrer</th>
                  <th>Fahrzeug</th>
                  <th>${isDrag ? 'Läufe' : 'Runden'}</th>
                  <th>Beste Zeit</th>
                  <th>Rundenzeiten</th>
                </tr>
              </thead>
              <tbody>`;

      s.results.forEach(r => {
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
            <button class="btn btn-purple" id="btn_arrow_${driverDetailId}" style="padding:2px 8px; font-size:11px; margin:0;" onclick="toggleDriverLapsInSession('${driverDetailId}', ${r.id}, ${s.ts}, ${sessionOverallBest})">Details 🔽</button>
          </td>
        </tr>
        <tr id="${driverDetailId}_tr" style="display:none; background: rgba(0,0,0,0.3);">
          <td colspan="6">
            <div id="${driverDetailId}_content" class="driver-lap-details">Lade Rundenzeiten...</div>
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

function toggleSessionDetails(ts) {
  let el = document.getElementById('session_detail_' + ts);
  let arrow = document.getElementById('session_arrow_' + ts);
  if (el) {
    let isOpen = (el.style.display === 'block');
    el.style.display = isOpen ? 'none' : 'block';
    if (arrow) arrow.innerText = isOpen ? 'Details 🔽' : 'Details 🔼';
  }
}

async function toggleDriverLapsInSession(elemId, carId, sessionTs, sessionOverallBest) {
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
  contentElem.innerHTML = 'Lade Rundenzeiten...';
  if (btnElem) btnElem.innerText = 'Details 🔼';

  try {
    let res = await fetch('/api/stats?id=' + carId);
    let data = await res.json();

    if (!data.laps || data.laps.length === 0) {
      contentElem.innerHTML = '<span style="color:var(--text-muted);">Keine Rundenzeiten im Speicher vorhanden.</span>';
      return;
    }

    let filteredLaps = data.laps.filter(l => {
      if (sessionTs && l.ts && l.ts > 0) {
        let sessionTsMs = sessionTs < 10000000000 ? sessionTs * 1000 : sessionTs;
        let lapTsMs = l.ts < 10000000000 ? l.ts * 1000 : l.ts;
        return lapTsMs <= sessionTsMs + 180000 && lapTsMs >= (sessionTsMs - 7200000);
      }
      return true;
    });

    if (filteredLaps.length === 0) {
      filteredLaps = data.laps;
    }

    renderEmbeddedLapsTable(contentElem, filteredLaps, sessionOverallBest || 0);
  } catch (e) {
    console.error(e);
    contentElem.innerHTML = '<span style="color:var(--neon-red);">Fehler beim Laden der Rundenzeiten.</span>';
  }
}

async function loadDirectLapsFallback(modeFilter, containerId) {
  let container = document.getElementById(containerId);
  container.innerHTML = '<p style="color:var(--text-muted);">Lade Rundenverlauf...</p>';

  let allLaps = [];
  try {
    for (let i = 1; i <= MAX_CARS; i++) {
      let res = await fetch('/api/stats?id=' + i);
      let data = await res.json();
      if (data && data.laps) {
        data.laps.forEach(l => {
          let match = false;
          if (Array.isArray(modeFilter)) match = modeFilter.includes(l.mode);
          else match = (l.mode === modeFilter);
          
          if (match) {
            allLaps.push({
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
              ts: l.ts
            });
          }
        });
      }
    }
  } catch (e) { console.error(e); }

  if (allLaps.length === 0) {
    container.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:20px;">Keine Einträge in diesem Bereich gespeichert.</p>';
    return;
  }

  let html = `<div class="session-card">
    <div style="display:flex; justify-content:space-between; align-items:center;" onclick="toggleSessionDetails('fallback')">
      <div>
        <strong style="color:var(--neon-green); font-size:16px;">Rundenaufzeichnung</strong>
        <span style="font-size:12px; color:var(--text-muted); margin-left:10px;">${allLaps.length} Runden erfasst</span>
      </div>
      <div>
        <span id="session_arrow_fallback" style="font-size:12px; color:var(--neon-cyan); cursor:pointer;">Details 🔽</span>
      </div>
    </div>
    <div id="session_detail_fallback" class="session-details" style="display:block;">`;

  let overallBestSz1 = 0, overallBestSz2 = 0, overallBestSz3 = 0, overallBestSz4 = 0, overallBestTime = 0;
  allLaps.forEach(l => {
    if (l.sz1 > 0 && (overallBestSz1 === 0 || l.sz1 < overallBestSz1)) overallBestSz1 = l.sz1;
    if (l.sz2 > 0 && (overallBestSz2 === 0 || l.sz2 < overallBestSz2)) overallBestSz2 = l.sz2;
    if (l.sz3 > 0 && (overallBestSz3 === 0 || l.sz3 < overallBestSz3)) overallBestSz3 = l.sz3;
    if (l.sz4 > 0 && (overallBestSz4 === 0 || l.sz4 < overallBestSz4)) overallBestSz4 = l.sz4;
    if (l.time > 0 && (overallBestTime === 0 || l.time < overallBestTime)) overallBestTime = l.time;
  });

  let carStats = {};
  allLaps.forEach(l => {
    let id = l.carId;
    if (!carStats[id]) {
      carStats[id] = {
        bestSz1: 0, worstSz1: 0,
        bestSz2: 0, worstSz2: 0,
        bestSz3: 0, worstSz3: 0,
        bestSz4: 0, worstSz4: 0,
        bestTime: 0, worstTime: 0
      };
    }
    let cs = carStats[id];
    if (l.sz1 > 0) {
      if (cs.bestSz1 === 0 || l.sz1 < cs.bestSz1) cs.bestSz1 = l.sz1;
      if (cs.worstSz1 === 0 || l.sz1 > cs.worstSz1) cs.worstSz1 = l.sz1;
    }
    if (l.sz2 > 0) {
      if (cs.bestSz2 === 0 || l.sz2 < cs.bestSz2) cs.bestSz2 = l.sz2;
      if (cs.worstSz2 === 0 || l.sz2 > cs.worstSz2) cs.worstSz2 = l.sz2;
    }
    if (l.sz3 > 0) {
      if (cs.bestSz3 === 0 || l.sz3 < cs.bestSz3) cs.bestSz3 = l.sz3;
      if (cs.worstSz3 === 0 || l.sz3 > cs.worstSz3) cs.worstSz3 = l.sz3;
    }
    if (l.sz4 > 0) {
      if (cs.bestSz4 === 0 || l.sz4 < cs.bestSz4) cs.bestSz4 = l.sz4;
      if (cs.worstSz4 === 0 || l.sz4 > cs.worstSz4) cs.worstSz4 = l.sz4;
    }
    if (l.time > 0) {
      if (cs.bestTime === 0 || l.time < cs.bestTime) cs.bestTime = l.time;
      if (cs.worstTime === 0 || l.time > cs.worstTime) cs.worstTime = l.time;
    }
  });

  html += `<div class="table-responsive"><table>
    <thead>
      <tr>
        <th>Fahrer</th>
        <th>Runde</th>
        <th>SZ1</th>
        <th>SZ2</th>
        <th>SZ3</th>
        <th>SZ4</th>
        <th>Rundenzeit</th>
        <th>Datum / Uhrzeit</th>
        <th>Aktion</th>
      </tr>
    </thead>
    <tbody>`;

  allLaps.slice().reverse().forEach(l => {
    let cs = carStats[l.carId] || {};

    let styleSz1 = getDynamicColorStyle(l.sz1, cs.bestSz1, overallBestSz1, cs.worstSz1);
    let styleSz2 = getDynamicColorStyle(l.sz2, cs.bestSz2, overallBestSz2, cs.worstSz2);
    let styleSz3 = getDynamicColorStyle(l.sz3, cs.bestSz3, overallBestSz3, cs.worstSz3);
    let styleSz4 = getDynamicColorStyle(l.sz4, cs.bestSz4, overallBestSz4, cs.worstSz4);
    let styleTime = getDynamicColorStyle(l.time, cs.bestTime, overallBestTime, cs.worstTime);

    html += `<tr>
      <td style="color:var(--neon-cyan); font-weight:bold;">${l.driver} <br><small style="color:var(--text-muted);">(${l.car})</small></td>
      <td>Runde ${l.lap}</td>
      <td style="${styleSz1}">${l.sz1 > 0 ? (l.sz1 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz2}">${l.sz2 > 0 ? (l.sz2 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz3}">${l.sz3 > 0 ? (l.sz3 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz4}">${l.sz4 > 0 ? (l.sz4 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleTime}">${(l.time / 1000).toFixed(2)}s</td>
      <td style="color:var(--text-muted); font-size:12px;">${formatTimestamp(l.ts)}</td>
      <td>
        <button class="btn-icon" title="Runde löschen" onclick="deleteSingleLapAndReload(${l.carId}, ${l.lap})">🗑</button>
      </td>
    </tr>`;
  });

  html += `</tbody></table></div></div></div>`;
  container.innerHTML = html;
}

function renderEmbeddedLapsTable(containerElem, laps, sessionOverallBest) {
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
    if (l.sz4 > 0) {
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

  let html = `<div class="table-responsive"><table style="margin:0; font-size:12px;">
    <thead>
      <tr>
        <th>Runde</th>
        <th>SZ1</th>
        <th>SZ2</th>
        <th>SZ3</th>
        <th>SZ4</th>
        <th>Zeit</th>
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
    let styleSz4 = getSectorStyle(l.sz4, minSz4, maxSz4);
    let styleTime = getTimeStyle(l.time, minTime, maxTime);

    html += `<tr>
      <td>Runde ${l.lap}</td>
      <td style="${styleSz1}">${l.sz1 > 0 ? (l.sz1 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz2}">${l.sz2 > 0 ? (l.sz2 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz3}">${l.sz3 > 0 ? (l.sz3 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleSz4}">${l.sz4 > 0 ? (l.sz4 / 1000).toFixed(2) + 's' : '---'}</td>
      <td style="${styleTime}">${(l.time / 1000).toFixed(2)}s</td>
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
  if (confirm("Möchtest du diese Rennstatistik wirklich löschen?")) {
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
      <td>Runde ${l.lap}</td>
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