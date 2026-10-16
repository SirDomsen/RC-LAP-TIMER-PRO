function autoSaveSessionResult(mode, modeName) {
  if (!lastLapsData || lastLapsData.length === 0) return;

  let activeCars = lastLapsData.filter(car => car.completedLaps > 0 || car.bestLap > 0);
  if (activeCars.length === 0) activeCars = lastLapsData;

  if (mode === 1) {
    activeCars.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
  } else {
    activeCars.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);
  }

  let nowTs = Math.floor(Date.now() / 1000);
  let sessionData = {
    ts: nowTs,
    mode: mode,
    modeName: modeName,
    results: activeCars.map((car, idx) => ({
      pos: idx + 1,
      id: car.id,
      driver: car.driver || (carNamesMap[car.id]?.driver || ("Fahrer " + car.id)),
      car: car.car || (carNamesMap[car.id]?.car || ("Auto " + car.id)),
      laps: car.completedLaps || 0,
      bestLap: car.bestLap || 0,
      lastLap: car.lastLap || 0
    }))
  };

  let formData = new URLSearchParams();
  formData.append('data', JSON.stringify(sessionData));

  fetch('/api/savesessionresult', { 
    method: 'POST', 
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: formData.toString() 
  }).catch(e => console.error("Fehler beim Auto-Save der Session:", e));
}

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
  autoSaveSessionResult(currentMode, currentMode === 4 ? "Dragrace" : "Rennen");
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
  const columns = ['default', 'driver', 'laps', 'sz1', 'sz2', 'sz3', 'lastLap', 'bestLap'];
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

function renderDriverStatsTable() {
  let tbody = document.getElementById('statsTableBody');
  if (!rawDriverLapsData || rawDriverLapsData.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5">Keine gespeicherten Runden vorhanden.</td></tr>';
    return;
  }

  let sorted = [...rawDriverLapsData];
  if (statsSortColumn !== 'default') {
    sorted.sort((a, b) => {
      let valA, valB;
      if (statsSortColumn === 'lap') {
        valA = a.lap || 0;
        valB = b.lap || 0;
      } else if (statsSortColumn === 'time') {
        valA = a.time || 0;
        valB = b.time || 0;
      } else if (statsSortColumn === 'ts') {
        valA = a.ts || 0;
        valB = b.ts || 0;
      } else if (statsSortColumn === 'mode') {
        valA = a.mode || 0;
        valB = b.mode || 0;
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

    html += `<tr>
      <td>Runde ${l.lap}</td>
      <td style="color:var(--neon-green); font-weight:bold;">${timeSec}</td>
      <td style="color:var(--text-muted); font-size:12px;">${dateStr}</td>
      <td>${modeStr}</td>
      <td>
        <button class="btn-icon" title="Runde löschen" onclick="deleteSingleLap(${activeStatsCarId}, ${l.lap})">🗑</button>
      </td>
    </tr>`;
  });
  tbody.innerHTML = html;
}

function switchStatsTab(tab) {
  currentTab = tab;
  document.getElementById('tabDriversBtn').classList.toggle('active', tab === 'drivers');
  document.getElementById('tabSessionsBtn').classList.toggle('active', tab === 'sessions');
  document.getElementById('statsTabDrivers').style.display = (tab === 'drivers') ? 'block' : 'none';
  document.getElementById('statsTabSessions').style.display = (tab === 'sessions') ? 'block' : 'none';

  if (tab === 'drivers') {
    renderDriversOverview();
  } else {
    loadSessionsOverview();
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

async function loadSessionsOverview() {
  let container = document.getElementById('statsTabSessions');
  container.innerHTML = '<p style="color:var(--text-muted);">Lade Renn-Historie...</p>';

  try {
    let res = await fetch('/api/sessions');
    let data = await res.json();
    
    if (!data.sessions || data.sessions.length === 0) {
      container.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:20px;">Keine absolvierten Rennen oder Qualifyings gespeichert.</p>';
      return;
    }

    let html = '';
    data.sessions.slice().reverse().forEach(s => {
      let isQuali = (s.mode === 1);
      let badgeClass = isQuali ? 'session-card quali' : 'session-card';
      let dateStr = formatTimestamp(s.ts);

      html += `<div class="${badgeClass}">
        <div style="display:flex; justify-content:space-between; align-items:center;" onclick="toggleSessionDetails(${s.ts})">
          <div>
            <strong style="color:${isQuali ? 'var(--neon-purple)' : 'var(--neon-green)'}; font-size:16px;">${s.modeName}</strong>
            <span style="font-size:12px; color:var(--text-muted); margin-left:10px;">📅 ${dateStr}</span>
          </div>
          <div>
            <span style="font-size:12px; color:var(--neon-cyan); margin-right:8px;">Details 🔽</span>
            <button class="btn-icon" onclick="event.stopPropagation(); deleteSession(${s.ts})" title="Löschen">🗑</button>
          </div>
        </div>
        
        <div id="session_detail_${s.ts}" class="session-details">
          <table>
            <thead>
              <tr><th>Pos</th><th>Fahrer</th><th>Fahrzeug</th><th>Runden</th><th>Beste Zeit</th><th>Aktion</th></tr>
            </thead>
            <tbody>`;

      s.results.forEach(r => {
        let bestStr = r.bestLap > 0 ? (r.bestLap / 1000).toFixed(2) + 's' : '---';
        let driverDetailId = `s_${s.ts}_driver_${r.id}`;
        html += `<tr>
          <td><strong>P${r.pos}</strong></td>
          <td style="color:var(--neon-cyan); font-weight:bold;">${r.driver}</td>
          <td style="color:var(--text-muted);">${r.car}</td>
          <td>${r.laps}</td>
          <td style="color:var(--neon-green);">${bestStr}</td>
          <td>
            <button class="btn btn-purple" style="padding:2px 6px; font-size:10px; margin:0;" onclick="toggleDriverLapsInSession('${driverDetailId}', ${r.id}, ${s.ts})">Details 🔽</button>
          </td>
        </tr>
        <tr id="${driverDetailId}_tr" style="display:none; background: rgba(0,0,0,0.2);">
          <td colspan="6">
            <div id="${driverDetailId}_content" class="driver-lap-details">Lade Rundenzeiten...</div>
          </td>
        </tr>`;
      });

      html += `</tbody></table></div></div>`;
    });

    container.innerHTML = html;
  } catch(e) {
    console.error(e);
    container.innerHTML = '<p style="color:var(--neon-red);">Fehler beim Laden der Rennstatistik.</p>';
  }
}

async function toggleDriverLapsInSession(elemId, carId, sessionTs) {
  let trElem = document.getElementById(elemId + '_tr');
  let contentElem = document.getElementById(elemId + '_content');

  if (!trElem || !contentElem) return;

  if (trElem.style.display === 'table-row') {
    trElem.style.display = 'none';
    return;
  }

  trElem.style.display = 'table-row';
  contentElem.style.display = 'block';
  contentElem.innerHTML = 'Lade Rundenzeiten...';

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
        return lapTsMs <= sessionTsMs + 120000 && lapTsMs >= (sessionTsMs - 7200000);
      }
      return false;
    });

    if (filteredLaps.length === 0) {
      filteredLaps = data.laps;
    }

    let minTime = Math.min(...filteredLaps.map(l => l.time));
    let maxTime = Math.max(...filteredLaps.map(l => l.time));

    let html = `<table style="margin:0; font-size:12px;">
      <thead>
        <tr><th>Runde</th><th>Zeit</th><th>Platz</th><th>Datum / Uhrzeit</th></tr>
      </thead>
      <tbody>`;

    filteredLaps.slice().reverse().forEach(l => {
      let timeSec = (l.time / 1000).toFixed(2) + 's';
      let dateStr = formatTimestamp(l.ts);
      let posStr = (l.pos && l.pos > 0) ? 'P' + l.pos : '---';

      let timeColor = 'var(--neon-cyan)';
      if (filteredLaps.length > 1) {
        if (l.time === minTime) {
          timeColor = 'var(--neon-green)';
        } else if (l.time === maxTime) {
          timeColor = 'var(--neon-red)';
        }
      }

      html += `<tr>
        <td>Runde ${l.lap}</td>
        <td style="color:${timeColor}; font-weight:bold;">${timeSec}</td>
        <td style="color:var(--neon-orange); font-weight:bold;">${posStr}</td>
        <td style="color:var(--text-muted);">${dateStr}</td>
      </tr>`;
    });

    html += '</tbody></table>';
    contentElem.innerHTML = html;
  } catch (e) {
    console.error(e);
    contentElem.innerHTML = '<span style="color:var(--neon-red);">Fehler beim Laden der Rundenzeiten.</span>';
  }
}

function toggleSessionDetails(ts) {
  let el = document.getElementById('session_detail_' + ts);
  if (el) el.style.display = (el.style.display === 'block') ? 'none' : 'block';
}

function deleteSession(ts) {
  if (confirm("Möchtest du diese Rennstatistik wirklich löschen?")) {
    fetch('/api/deletesession?ts=' + ts, { method: 'POST' })
      .then(() => loadSessionsOverview())
      .catch(e => console.error(e));
  }
}

function closeStatsOverviewModal() { document.getElementById('statsOverviewModal').style.display = 'none'; }
function backToStatsOverview() { closeStatsModal(); openStatsOverviewModal(); }

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
  if (confirm("WARNUNG: Möchtest du wirklich ALLE Statistiken (Fahrer & Rennergebnisse) unwiderruflich löschen?")) {
    fetch('/api/deleteallstats', { method: 'POST' })
      .then(() => {
        alert("Alle Statistiken wurden gelöscht.");
        closeStatsOverviewModal();
      }).catch(err => console.error(err));
  }
}

async function downloadStatsBackup() {
  let backupData = {
    version: 1,
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

  if (!confirm("Soll das Backup eingespielt werden? Bestehende Datensätze werden ergänzt/aktualisiert.")) {
    return;
  }

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
    alert("Fehler beim Einlesen der Backup-Datei: " + err.message);
  }
  
  event.target.value = '';
}