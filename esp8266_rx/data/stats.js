let allSessionsCache = [];
let sessionFileCache = {};
let currentStatsTrackFilter = 'all';

let driversSortCol = 'laps';
let driversSortDir = 'desc';

let recordsSortCol = 'lap';
let recordsSortDir = 'asc';
let recordsTimeScope = 'alltime'; // 'alltime' | 'daily'
let recordsScope = 'all';         // 'all' (Offizielle Streckenrekorde) | 'sessions' (Aus Sessions berechnet)
let recordsCarFilter = 'all';
let recordsDriverFilter = 'all';

let openSessionsMap = {};
let openDriverDetailsMap = {};

let sessionResultsSortMap = {};
let embeddedLapsState = {};
let driverProfileSessionSort = { col: 'ts', dir: 'desc' };
let driverProfileAllLapsState = null;

// Dechiffriert / normalisiert Runden gem. schema.json (abwärtskompatibel zu alten Keys)
function normalizeLap(l) {
  if (!l) return null;
  return {
    carId: l.c !== undefined ? l.c : (l.carId || 0),
    lap: l.l !== undefined ? l.l : (l.lap || 0),
    time: l.t !== undefined ? l.t : (l.time || 0),
    pos: l.p !== undefined ? l.p : (l.pos || 0),
    ts: l.s !== undefined ? l.s : (l.ts || 0),
    sz1: l.s1 !== undefined ? l.s1 : (l.sz1 || 0),
    sz2: l.s2 !== undefined ? l.s2 : (l.sz2 || 0),
    sz3: l.s3 !== undefined ? l.s3 : (l.sz3 || 0),
    sz4: l.s4 !== undefined ? l.s4 : (l.sz4 || 0),
    outlap: l.o !== undefined ? Boolean(l.o) : Boolean(l.outlap),
    driver: l.driver,
    car: l.car
  };
}

function sanitizeDomId(str) {
  return String(str || '').replace(/[^a-zA-Z0-9_-]/g, '_');
}

function getCarDriverName(carId) {
  if (typeof carNamesMap !== 'undefined' && carNamesMap && carNamesMap[carId]?.driver) {
    return carNamesMap[carId].driver;
  }
  return 'Fahrer ' + (carId || '?');
}

function getCarCarName(carId) {
  if (typeof carNamesMap !== 'undefined' && carNamesMap && carNamesMap[carId]?.car) {
    return carNamesMap[carId].car;
  }
  return 'Auto ' + (carId || '?');
}

function modeToTabName(mode) {
  if (mode === 0) return 'training';
  if (mode === 1) return 'quali';
  if (mode === 2) return 'race_laps';
  if (mode === 3) return 'race_time';
  if (mode === 4) return 'drag';
  return 'training';
}

function tabNameToMode(tab) {
  if (tab === 'training') return 0;
  if (tab === 'quali') return 1;
  if (tab === 'race_laps') return 2;
  if (tab === 'race_time') return 3;
  if (tab === 'drag') return 4;
  return 0;
}

function getSortIconHtml(state, col) {
  if (!state || state.col !== col) return '';
  return `<span class="sort-icon" style="color:var(--neon-cyan); font-weight:bold; margin-left:3px;">${state.dir === 'asc' ? '▲' : '▼'}</span>`;
}

// announceRaceResults() wurde nach tts.js verlagert (global fuer Admin und Fahrer verfuegbar)

function updateStatsModalDailyBest() {
  let allTimeElem = document.getElementById('statsAllTimeBestVal');
  let dailyElem = document.getElementById('statsDailyBestVal');
  let boxElem = document.getElementById('statsDailyBestDisplay');
  if (!dailyElem && !allTimeElem) return;

  if (boxElem && typeof openTrackRecordsModal === 'function') {
    boxElem.style.cursor = 'pointer';
    boxElem.title = 'Klicken für Top 5 Bestenliste';
    boxElem.onclick = () => {
      let targetTrack = (currentStatsTrackFilter && currentStatsTrackFilter !== 'all') ? currentStatsTrackFilter : activeTrackId;
      openTrackRecordsModal(targetTrack);
    };
  }

  if (typeof allTracksList === 'undefined' || !Array.isArray(allTracksList) || allTracksList.length === 0) {
    if (allTimeElem) allTimeElem.innerText = '--:--';
    if (dailyElem) dailyElem.innerText = '--:--';
    return;
  }

  if (currentStatsTrackFilter === 'all') {
    // 1. All-Time Rekord über alle Strecken
    let recBest = 0;
    let recDriver = '';
    let recTrack = '';
    allTracksList.forEach(t => {
      let rLap = t.recordLap || 0;
      if (rLap > 0 && (recBest === 0 || rLap < recBest)) {
        recBest = rLap;
        recDriver = t.recordDriver || (carNamesMap[t.recordCarId]?.driver || ('Fahrer ' + t.recordCarId));
        recTrack = t.name;
      }
    });
    if (allTimeElem) {
      if (recBest > 0) {
        allTimeElem.innerHTML = `<span style="color:var(--neon-gold); font-weight:bold;">${(recBest / 1000).toFixed(2)}s</span> <small style="color:var(--neon-cyan);">(${recDriver} - ${recTrack})</small>`;
      } else {
        allTimeElem.innerHTML = `<span style="color:var(--text-muted); font-size:11px;">Kein Streckenrekord</span>`;
      }
    }

    // 2. Tagesbestzeit über alle Strecken
    let best = 0;
    let bestDriver = '';
    let bestTrack = '';
    allTracksList.forEach(t => {
      if (t.bestLap > 0 && (best === 0 || t.bestLap < best)) {
        best = t.bestLap;
        bestDriver = t.bestDriver || (carNamesMap[t.bestCarId]?.driver || ('Fahrer ' + t.bestCarId));
        bestTrack = t.name;
      }
    });
    if (dailyElem) {
      if (best > 0) {
        dailyElem.innerHTML = `<span style="color:var(--neon-green); font-weight:bold;">${(best / 1000).toFixed(2)}s</span> <small style="color:var(--neon-cyan);">(${bestDriver} - ${bestTrack})</small>`;
      } else {
        dailyElem.innerHTML = `<span style="color:var(--text-muted); font-size:11px;">Keine Bestzeit heute</span>`;
      }
    }
  } else {
    let t = allTracksList.find(x => x.id === currentStatsTrackFilter);
    if (allTimeElem) {
      let rLap = t ? (t.recordLap || 0) : 0;
      if (t && rLap > 0) {
        let rd = t.recordDriver || (carNamesMap[t.recordCarId]?.driver || ('Fahrer ' + t.recordCarId));
        let rc = t.recordCar || (carNamesMap[t.recordCarId]?.car || '');
        let carPart = rc ? ` (${rc})` : '';
        allTimeElem.innerHTML = `<span style="color:var(--neon-gold); font-weight:bold;">${(rLap / 1000).toFixed(2)}s</span> <small style="color:var(--neon-cyan);">(${rd}${carPart})</small>`;
      } else {
        allTimeElem.innerHTML = `<span style="color:var(--text-muted); font-size:11px;">Kein Streckenrekord</span>`;
      }
    }
    if (dailyElem) {
      if (t && t.bestLap > 0) {
        let d = t.bestDriver || (carNamesMap[t.bestCarId]?.driver || ('Fahrer ' + t.bestCarId));
        let c = t.bestCar || (carNamesMap[t.bestCarId]?.car || '');
        let carPart = c ? ` (${c})` : '';
        dailyElem.innerHTML = `<span style="color:var(--neon-green); font-weight:bold;">${(t.bestLap / 1000).toFixed(2)}s</span> <small style="color:var(--neon-cyan);">(${d}${carPart})</small>`;
      } else {
        dailyElem.innerHTML = `<span style="color:var(--text-muted); font-size:11px;">Keine Bestzeit heute</span>`;
      }
    }
  }
}

async function fetchSessionsAndStorageInfo() {
  try {
    let [sRes, tRes] = await Promise.all([
      fetch('/api/sessions?_t=' + Date.now(), { cache: 'no-store' }),
      fetch('/api/timeleft?_t=' + Date.now(), { cache: 'no-store' })
    ]);
    if (sRes.ok) {
      let sData = await sRes.json();
      allSessionsCache = sData.sessions || [];
    }
    if (tRes.ok) {
      let info = await tRes.json();
      if (typeof updateStorageStatusUI === 'function' && info.fsPct !== undefined) {
        updateStorageStatusUI(info.fsPct, info.fsFull, info.fsUsed, info.fsTotal);
      }
    }
    updateStatsModalDailyBest();
  } catch (e) {
    console.error("Fehler beim Laden der Sessions:", e);
  }
}

async function openStatsOverviewModal(initialTab) {
  let modal = document.getElementById('statsOverviewModal');
  if (modal) modal.style.display = 'block';

  sessionFileCache = {};
  try {
    let p1 = (typeof loadCarNames === 'function') ? loadCarNames() : Promise.resolve();
    let p2 = (typeof loadTracks === 'function') ? loadTracks() : Promise.resolve();
    let p3 = fetchSessionsAndStorageInfo();
    await Promise.all([p1, p2, p3]);
  } catch (err) {
    console.warn("Fehler beim Vorab-Laden der Statistik-Daten:", err);
  }

  let filterSel = document.getElementById('statsTrackFilterSelect');
  if (filterSel) {
    filterSel.value = currentStatsTrackFilter;
  }

  updateStatsModalDailyBest();

  if (initialTab) {
    currentTab = initialTab;
  } else if (!currentTab || currentTab === 'race') {
    currentTab = document.getElementById('statsTabRecords') ? 'records' : 'drivers';
  }
  switchStatsTab(currentTab);
}

function onStatsTrackFilterChange() {
  let sel = document.getElementById('statsTrackFilterSelect');
  if (sel) {
    currentStatsTrackFilter = sel.value || 'all';
  }
  updateStatsModalDailyBest();
  switchStatsTab(currentTab);
}

function switchStatsTab(tab) {
  if (tab === 'race') tab = 'race_laps';

  // Fallback: Wenn 'records' im DOM noch nicht existiert (z. B. alter Browser-Cache),
  // sicher auf 'drivers' oder 'training' zurückfallen
  if (tab === 'records' && !document.getElementById('statsTabRecords')) {
    tab = document.getElementById('statsTabDrivers') ? 'drivers' : 'training';
  }
  currentTab = tab;

  const tabs = [
    { key: 'records', btn: 'tabRecordsBtn', div: 'statsTabRecords' },
    { key: 'drivers', btn: 'tabDriversBtn', div: 'statsTabDrivers' },
    { key: 'training', btn: 'tabTrainingBtn', div: 'statsTabTraining' },
    { key: 'quali', btn: 'tabQualiBtn', div: 'statsTabQuali' },
    { key: 'race_laps', btn: 'tabRace_lapsBtn', div: 'statsTabRace_laps' },
    { key: 'race_time', btn: 'tabRace_timeBtn', div: 'statsTabRace_time' },
    { key: 'drag', btn: 'tabDragBtn', div: 'statsTabDrag' }
  ];

  tabs.forEach(t => {
    let b = document.getElementById(t.btn) || document.getElementById(t.btn.replace('_', ''));
    if (b) b.classList.toggle('active', t.key === tab);
    let d = document.getElementById(t.div);
    if (d) d.style.display = (t.key === tab) ? 'block' : 'none';
  });

  if (tab === 'records') {
    renderRecordsOverview();
  } else if (tab === 'drivers') {
    renderDriversOverview();
  } else {
    let mode = tabNameToMode(tab);
    let activeTabObj = tabs.find(t => t.key === tab);
    let targetDiv = activeTabObj ? activeTabObj.div : ('statsTab' + tab.charAt(0).toUpperCase() + tab.slice(1));
    renderSessionsByMode(mode, targetDiv);
  }
}

function escapeHtml(str) {
  if (str === null || str === undefined) return '';
  return String(str).replace(/[&<>"']/g, m => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[m]);
}

function setRecordsTimeScope(scope) {
  recordsTimeScope = scope;
  renderRecordsOverview();
}

function setRecordsScope(scope) {
  recordsScope = scope;
  renderRecordsOverview();
}

function onRecordsTrackFilterChange(val) {
  currentStatsTrackFilter = val;
  let topSel = document.getElementById('statsTrackFilterSelect');
  if (topSel) topSel.value = val;
  updateStatsModalDailyBest();
  renderRecordsOverview();
}

function onRecordsCarFilterChange(val) {
  recordsCarFilter = val;
  renderRecordsOverview();
}

function onRecordsDriverFilterChange(val) {
  recordsDriverFilter = val;
  renderRecordsOverview();
}

function toggleRecordsSort(col) {
  if (recordsSortCol === col) {
    recordsSortDir = (recordsSortDir === 'asc') ? 'desc' : 'asc';
  } else {
    recordsSortCol = col;
    recordsSortDir = (col === 'lap') ? 'asc' : 'asc';
    if (col === 'date') recordsSortDir = 'desc';
  }
  renderRecordsOverview();
}

function openSessionFromRecord(mode, sessionId) {
  let targetTab = modeToTabName(mode);
  switchStatsTab(targetTab);
  let safeId = sanitizeDomId(sessionId);
  toggleSessionDetails(safeId, true);
  let card = document.getElementById('session_card_' + safeId);
  if (card) {
    card.classList.remove('session-highlight-flash');
    void card.offsetWidth;
    card.classList.add('session-highlight-flash');
    setTimeout(() => {
      card.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }, 100);
  }
}

function getAggregatedRecords() {
  let records = [];

  if (recordsScope === 'all') {
    // Offizielle Streckenrekorde aus allTracksList
    if (typeof allTracksList !== 'undefined' && Array.isArray(allTracksList)) {
      allTracksList.forEach(t => {
        if (currentStatsTrackFilter !== 'all' && t.id !== currentStatsTrackFilter) return;

        let list = (recordsTimeScope === 'daily') ? (t.dailyRecords || []) : (t.allTimeRecords || []);
        if (list.length === 0) {
          let legacyLap = (recordsTimeScope === 'daily') ? t.bestLap : (t.recordLap || t.bestLap);
          if (legacyLap > 0) {
            list = [{
              lap: legacyLap,
              carId: (recordsTimeScope === 'daily') ? t.bestCarId : (t.recordCarId || t.bestCarId || 0),
              driver: (recordsTimeScope === 'daily') ? t.bestDriver : (t.recordDriver || t.bestDriver || ''),
              car: (recordsTimeScope === 'daily') ? t.bestCar : (t.recordCar || t.bestCar || ''),
              date: (recordsTimeScope === 'daily') ? t.bestDate : (t.recordDate || t.bestDate || '')
            }];
          }
        }

        list.forEach((entry, idx) => {
          if (!entry || entry.lap <= 0) return;
          let dName = entry.driver || getCarDriverName(entry.carId);
          let cName = entry.car || getCarCarName(entry.carId);
          records.push({
            lap: entry.lap,
            driver: dName,
            car: cName,
            carId: entry.carId,
            date: entry.date || '',
            prevLap: entry.prevLap || 0,
            trackId: t.id,
            trackName: t.name || t.id,
            source: 'official',
            index: idx
          });
        });
      });
    }
  } else {
    // Berechnet aus allen Sessions in allSessionsCache
    let todayStr = '';
    if (typeof getCurrentLocalDateStr === 'function') {
      todayStr = getCurrentLocalDateStr();
    }
    let comboBestMap = {};

    allSessionsCache.forEach(s => {
      let tId = s.trackId || 'standard';
      if (currentStatsTrackFilter !== 'all' && tId !== currentStatsTrackFilter) return;
      let tName = getTrackNameById(tId, s.trackName);

      let sDate = s.date || '';
      if (recordsTimeScope === 'daily') {
        let isToday = false;
        if (s.startTs) {
          let sD = new Date(s.startTs * 1000);
          let now = new Date();
          isToday = (sD.toDateString() === now.toDateString());
        }
        if (!isToday && sDate && todayStr && sDate !== todayStr) return;
      }

      (s.results || []).forEach(r => {
        if (!r.bestLap || r.bestLap <= 0) return;
        let dName = r.driver || getCarDriverName(r.id);
        let cName = r.car || getCarCarName(r.id);
        let comboKey = `${tId}_${dName.toLowerCase()}_${cName.toLowerCase()}`;

        if (!comboBestMap[comboKey] || r.bestLap < comboBestMap[comboKey].lap) {
          comboBestMap[comboKey] = {
            lap: r.bestLap,
            driver: dName,
            car: cName,
            carId: r.id,
            date: sDate || (s.startTs ? new Date(s.startTs * 1000).toLocaleDateString() : ''),
            trackId: tId,
            trackName: tName,
            source: 'session',
            sessionId: s.id,
            mode: s.mode
          };
        }
      });
    });

    records = Object.values(comboBestMap);
  }

  return records;
}

function renderRecordsOverview() {
  let container = document.getElementById('statsTabRecords');
  if (!container) return;

  let allRecords = getAggregatedRecords();

  // Alle verfügbaren Strecken sammeln (aus allTracksList und allSessionsCache)
  let tracksMap = new Map();
  if (typeof allTracksList !== 'undefined' && Array.isArray(allTracksList)) {
    allTracksList.forEach(t => {
      tracksMap.set(t.id, t.name || t.id);
    });
  }
  if (Array.isArray(allSessionsCache)) {
    allSessionsCache.forEach(s => {
      let tId = s.trackId || 'standard';
      if (!tracksMap.has(tId)) {
        tracksMap.set(tId, getTrackNameById(tId, s.trackName));
      }
    });
  }
  let sortedTracks = Array.from(tracksMap.entries()).map(([id, name]) => ({ id, name }));

  // Eindeutige Fahrzeug- und Fahrernamen sammeln
  let carsSet = new Set();
  let driversSet = new Set();
  allRecords.forEach(r => {
    if (r.car) carsSet.add(r.car);
    if (r.driver) driversSet.add(r.driver);
  });
  let sortedCars = Array.from(carsSet).sort((a, b) => a.localeCompare(b));
  let sortedDrivers = Array.from(driversSet).sort((a, b) => a.localeCompare(b));

  // Filter-Rückfall, falls gewähltes Auto / Fahrer auf gefilterter Strecke nicht existiert
  if (recordsCarFilter !== 'all' && !carsSet.has(recordsCarFilter)) {
    recordsCarFilter = 'all';
  }
  if (recordsDriverFilter !== 'all' && !driversSet.has(recordsDriverFilter)) {
    recordsDriverFilter = 'all';
  }

  // Filtern
  let filtered = allRecords.filter(r => {
    if (recordsCarFilter !== 'all' && r.car !== recordsCarFilter) return false;
    if (recordsDriverFilter !== 'all' && r.driver !== recordsDriverFilter) return false;
    return true;
  });

  // Sortieren
  filtered.sort((a, b) => {
    let res = 0;
    if (recordsSortCol === 'lap') {
      res = a.lap - b.lap;
    } else if (recordsSortCol === 'driver') {
      res = a.driver.localeCompare(b.driver);
    } else if (recordsSortCol === 'car') {
      res = a.car.localeCompare(b.car);
    } else if (recordsSortCol === 'date') {
      res = (a.date || '').localeCompare(b.date || '');
    }
    if (res === 0) res = a.lap - b.lap;
    return (recordsSortDir === 'asc') ? res : -res;
  });

  let getPillClass = (col) => (recordsSortCol === col ? 'btn btn-purple' : 'btn');
  let getArrow = (col) => (recordsSortCol === col ? (recordsSortDir === 'asc' ? ' ▲' : ' ▼') : '');
  let isAdmin = (typeof userRole !== 'undefined' && userRole === 'admin');

  let trackNameHeader = (currentStatsTrackFilter !== 'all') ? getTrackNameById(currentStatsTrackFilter) : 'Alle Strecken';

  let html = `
    <div style="background:rgba(0,0,0,0.3); border-radius:8px; padding:12px; border:1px solid rgba(255,255,255,0.08); margin-bottom:12px;">
      <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:8px; margin-bottom:10px;">
        <div style="display:flex; gap:6px; flex-wrap:wrap;">
          <button class="btn ${recordsTimeScope === 'alltime' ? 'btn-purple' : ''}" style="padding:4px 10px; font-size:11px; margin:0;" onclick="setRecordsTimeScope('alltime')">🏆 All-Time Rekorde</button>
          <button class="btn ${recordsTimeScope === 'daily' ? 'btn-purple' : ''}" style="padding:4px 10px; font-size:11px; margin:0;" onclick="setRecordsTimeScope('daily')">⏱️ Tagesbestzeiten (Heute)</button>
        </div>
        <div style="display:flex; gap:6px; flex-wrap:wrap;">
          <button class="btn ${recordsScope === 'all' ? 'btn-purple' : ''}" style="padding:4px 10px; font-size:11px; margin:0;" onclick="setRecordsScope('all')" title="Offizielle Strecken-Rekordliste">📌 Offizielle Rekorde</button>
          <button class="btn ${recordsScope === 'sessions' ? 'btn-purple' : ''}" style="padding:4px 10px; font-size:11px; margin:0;" onclick="setRecordsScope('sessions')" title="Aus allen Sessions ermittelt">📊 Aus Sessions</button>
        </div>
      </div>

      <div style="display:flex; align-items:center; flex-wrap:wrap; gap:12px; background:rgba(0,0,0,0.25); padding:8px 10px; border-radius:6px; margin-bottom:10px;">
        <div style="display:flex; align-items:center; gap:6px;">
          <span style="font-size:11px; color:var(--text-muted); font-weight:bold;">📍 STRECKE:</span>
          <select onchange="onRecordsTrackFilterChange(this.value)" style="background:#161b22; color:var(--text-main); border:1px solid rgba(255,255,255,0.2); border-radius:4px; padding:3px 6px; font-size:11px;">
            <option value="all" ${currentStatsTrackFilter === 'all' ? 'selected' : ''}>Alle Strecken (${sortedTracks.length})</option>
            ${sortedTracks.map(t => `<option value="${escapeHtml(t.id)}" ${currentStatsTrackFilter === t.id ? 'selected' : ''}>${escapeHtml(t.name)}</option>`).join('')}
          </select>
        </div>

        <div style="display:flex; align-items:center; gap:6px;">
          <span style="font-size:11px; color:var(--text-muted); font-weight:bold;">🏎️ FAHRZEUG:</span>
          <select onchange="onRecordsCarFilterChange(this.value)" style="background:#161b22; color:var(--text-main); border:1px solid rgba(255,255,255,0.2); border-radius:4px; padding:3px 6px; font-size:11px;">
            <option value="all" ${recordsCarFilter === 'all' ? 'selected' : ''}>Alle Fahrzeuge (${sortedCars.length})</option>
            ${sortedCars.map(c => `<option value="${escapeHtml(c)}" ${recordsCarFilter === c ? 'selected' : ''}>${escapeHtml(c)}</option>`).join('')}
          </select>
        </div>

        <div style="display:flex; align-items:center; gap:6px;">
          <span style="font-size:11px; color:var(--text-muted); font-weight:bold;">👤 FAHRER:</span>
          <select onchange="onRecordsDriverFilterChange(this.value)" style="background:#161b22; color:var(--text-main); border:1px solid rgba(255,255,255,0.2); border-radius:4px; padding:3px 6px; font-size:11px;">
            <option value="all" ${recordsDriverFilter === 'all' ? 'selected' : ''}>Alle Fahrer (${sortedDrivers.length})</option>
            ${sortedDrivers.map(d => `<option value="${escapeHtml(d)}" ${recordsDriverFilter === d ? 'selected' : ''}>${escapeHtml(d)}</option>`).join('')}
          </select>
        </div>
      </div>

      <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:8px;">
        <span style="font-size:11px; color:var(--text-muted); font-weight:bold;">SORTIERUNG:</span>
        <div style="display:flex; gap:6px; flex-wrap:wrap;">
          <button class="${getPillClass('lap')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleRecordsSort('lap')">⚡ Zeit${getArrow('lap')}</button>
          <button class="${getPillClass('driver')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleRecordsSort('driver')">👤 Fahrer${getArrow('driver')}</button>
          <button class="${getPillClass('car')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleRecordsSort('car')">🏎️ Fahrzeug${getArrow('car')}</button>
          <button class="${getPillClass('date')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleRecordsSort('date')">📅 Datum${getArrow('date')}</button>
        </div>
      </div>
    </div>
  `;

  if (filtered.length === 0) {
    html += `
      <div style="text-align:center; padding:30px 10px; color:var(--text-muted); background:rgba(0,0,0,0.2); border-radius:8px;">
        <div style="font-size:24px; margin-bottom:8px;">⏱️</div>
        <div>Keine Rekorde für die ausgewählten Kriterien auf <strong>${escapeHtml(trackNameHeader)}</strong> vorhanden.</div>
      </div>`;
    container.innerHTML = html;
    return;
  }

  html += `<div style="display:flex; flex-direction:column; gap:6px;">`;

  filtered.forEach((r, idx) => {
    let rank = idx + 1;
    let rankClass = rank === 1 ? 'top5-rank-1' : (rank === 2 ? 'top5-rank-2' : (rank === 3 ? 'top5-rank-3' : 'top5-rank-other'));
    let timeColor = (rank === 1)
      ? 'color:var(--neon-gold); text-shadow:0 0 8px rgba(255,215,0,0.5);'
      : (rank === 2 ? 'color:var(--neon-cyan);' : (rank === 3 ? 'color:var(--neon-purple);' : 'color:var(--neon-green);'));

    let dFmt = (typeof formatRecordDate === 'function') ? formatRecordDate(r.date) : r.date;
    let dateHtml = dFmt ? `<span style="color:var(--text-muted);">📅 ${escapeHtml(dFmt)}</span>` : '';
    let trackHtml = (currentStatsTrackFilter === 'all' && r.trackName) ? `<span style="color:var(--neon-purple);">📍 ${escapeHtml(r.trackName)}</span>` : '';
    let sessionLinkHtml = r.sessionId
      ? `<span style="color:var(--neon-cyan); cursor:pointer; text-decoration:underline;" onclick="openSessionFromRecord(${r.mode}, '${r.sessionId}')" title="Session öffnen">🔍 Session</span>`
      : '';
    let prevHint = (isAdmin && r.prevLap > 0)
      ? `<span style="font-size:10px; color:var(--text-muted);">(vorher: ${(r.prevLap/1000).toFixed(2)}s)</span>`
      : '';

    let delBtnHtml = (isAdmin && r.source === 'official' && typeof deleteTrackRecordEntryUI === 'function')
      ? `<button class="top5-del-btn" onclick="deleteTrackRecordEntryUI('${r.trackId}', '${recordsTimeScope === 'daily' ? 'daily' : 'alltime'}', ${r.index})" title="Diesen Rekordeintrag löschen">✕</button>`
      : '';

    html += `
      <div class="top5-entry-row" style="background:#0d1117; border:1px solid rgba(255,255,255,0.06); border-radius:6px; padding:8px 12px;">
        <div style="display:flex; align-items:center; gap:10px;">
          <span class="top5-rank-badge ${rankClass}">P${rank}</span>
          <div style="text-align:left;">
            <div>
              <strong style="color:var(--neon-cyan); font-size:13px;">${escapeHtml(r.driver)}</strong>
              <span style="color:var(--text-muted); font-size:12px; margin-left:4px;">(${escapeHtml(r.car)})</span>
            </div>
            <div style="font-size:11px; display:flex; gap:8px; align-items:center; flex-wrap:wrap; margin-top:3px;">
              ${dateHtml}
              ${trackHtml}
              ${prevHint}
              ${sessionLinkHtml}
            </div>
          </div>
        </div>
        <div style="display:flex; align-items:center; gap:10px;">
          <span style="${timeColor}; font-size:15px; font-weight:bold; font-family:var(--font-display);">${(r.lap/1000).toFixed(2)}s</span>
          ${delBtnHtml}
        </div>
      </div>
    `;
  });

  html += `</div>`;
  container.innerHTML = html;
}

function getTrackNameById(trackId, fallbackName) {
  if (fallbackName) return fallbackName;
  if (typeof allTracksList !== 'undefined' && Array.isArray(allTracksList)) {
    let found = allTracksList.find(t => t.id === trackId);
    if (found) return found.name;
  }
  return trackId || 'Test-Strecke';
}

function toggleDriversSort(col) {
  if (driversSortCol === col) {
    driversSortDir = (driversSortDir === 'asc') ? 'desc' : 'asc';
  } else {
    driversSortCol = col;
    driversSortDir = (col === 'driver') ? 'asc' : 'desc';
  }
  renderDriversOverview();
}

function renderDriversOverview() {
  let list = document.getElementById('statsTabDrivers');
  if (!list) return;

  let filteredSessions = allSessionsCache.filter(s => {
    if (currentStatsTrackFilter !== 'all' && s.trackId !== currentStatsTrackFilter) return false;
    return true;
  });

  let driverStatsMap = {};
  for (let i = 1; i <= MAX_CARS; i++) {
    driverStatsMap[i] = {
      id: i,
      driver: carNamesMap[i]?.driver || ("Fahrer " + i),
      car: carNamesMap[i]?.car || ("Auto " + i),
      totalLaps: 0,
      sessionsCount: 0,
      bestLap: 0,
      bestSessionId: null,
      bestMode: 0,
      bestTrackId: null,
      bestTrackName: null,
      trackBests: {}
    };
  }

  filteredSessions.forEach(s => {
    let tId = s.trackId || 'standard';
    let tName = getTrackNameById(tId, s.trackName);
    (s.results || []).forEach(r => {
      let cid = r.id;
      if (!driverStatsMap[cid]) {
        driverStatsMap[cid] = {
          id: cid,
          driver: r.driver || ("Fahrer " + cid),
          car: r.car || ("Auto " + cid),
          totalLaps: 0,
          sessionsCount: 0,
          bestLap: 0,
          bestSessionId: null,
          bestMode: 0,
          bestTrackId: null,
          bestTrackName: null,
          trackBests: {}
        };
      }
      let ds = driverStatsMap[cid];
      if (r.laps > 0 || r.bestLap > 0) {
        ds.totalLaps += (r.laps || 0);
        ds.sessionsCount += 1;
      }
      if (r.bestLap > 0) {
        if (ds.bestLap === 0 || r.bestLap < ds.bestLap) {
          ds.bestLap = r.bestLap;
          ds.bestSessionId = s.id;
          ds.bestMode = s.mode;
          ds.bestTrackId = tId;
          ds.bestTrackName = tName;
        }
        if (!ds.trackBests[tId] || r.bestLap < ds.trackBests[tId].bestLap) {
          ds.trackBests[tId] = {
            trackId: tId,
            trackName: tName,
            bestLap: r.bestLap,
            sessionId: s.id,
            mode: s.mode
          };
        }
      }
    });
  });

  let driversList = Object.values(driverStatsMap);
  driversList.sort((a, b) => {
    let res = 0;
    if (driversSortCol === 'driver') {
      res = a.driver.localeCompare(b.driver);
    } else if (driversSortCol === 'laps') {
      res = a.totalLaps - b.totalLaps;
    } else if (driversSortCol === 'sessions') {
      res = a.sessionsCount - b.sessionsCount;
    } else if (driversSortCol === 'bestLap') {
      let vA = a.bestLap > 0 ? a.bestLap : 99999999;
      let vB = b.bestLap > 0 ? b.bestLap : 99999999;
      res = vA - vB;
    }
    if (res === 0) res = a.id - b.id;
    return (driversSortDir === 'asc') ? res : -res;
  });

  let getPillClass = (col) => (driversSortCol === col ? 'btn btn-purple' : 'btn');
  let getArrow = (col) => (driversSortCol === col ? (driversSortDir === 'asc' ? ' ▲' : ' ▼') : '');

  let sortBarHtml = `
    <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:8px; margin-bottom:12px; background:rgba(0,0,0,0.3); padding:8px 12px; border-radius:8px; border:1px solid rgba(255,255,255,0.06);">
      <span style="font-size:12px; color:var(--text-muted); font-weight:bold;">SORTIERUNG:</span>
      <div style="display:flex; gap:6px; flex-wrap:wrap;">
        <button class="${getPillClass('laps')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleDriversSort('laps')">🔄 Runden${getArrow('laps')}</button>
        <button class="${getPillClass('bestLap')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleDriversSort('bestLap')">⚡ Bestzeit${getArrow('bestLap')}</button>
        <button class="${getPillClass('sessions')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleDriversSort('sessions')">📅 Sessions${getArrow('sessions')}</button>
        <button class="${getPillClass('driver')}" style="padding:3px 10px; font-size:11px; margin:0;" onclick="toggleDriversSort('driver')">👤 Name${getArrow('driver')}</button>
      </div>
    </div>`;

  let cardsHtml = '';
  driversList.forEach(ds => {
    let bestDisp = ds.bestLap > 0 ? (ds.bestLap / 1000).toFixed(2) + 's' : '---';
    let trackPillsHtml = '';
    let tbValues = Object.values(ds.trackBests);
    if (tbValues.length > 0) {
      trackPillsHtml = '<div style="margin-top:6px;">';
      tbValues.forEach(tb => {
        let tSec = (tb.bestLap / 1000).toFixed(2) + 's';
        trackPillsHtml += `<span class="driver-track-pb-pill" onclick="event.stopPropagation(); jumpToSessionInStats('${tb.sessionId}', ${tb.mode}, '${tb.trackId}', ${ds.id})" title="Direkt zur Session dieser Strecken-Bestzeit springen">📍 ${tb.trackName}: <strong>${tSec}</strong> 🔗</span>`;
      });
      trackPillsHtml += '</div>';
    }

    cardsHtml += `<div class="driver-select-card" onclick="openDriverStats(${ds.id})">
      <div style="flex:1; min-width:0;">
        <strong style="color:var(--neon-cyan); font-size:16px;">${ds.driver}</strong>
        <small style="color:var(--text-muted);">(${ds.car} - ID ${ds.id})</small><br>
        <span style="font-size:12px; color:var(--text-muted);">Gefahrene Runden: <strong style="color:var(--text-main);">${ds.totalLaps}</strong> &nbsp;|&nbsp; Sessions: <strong style="color:var(--text-main);">${ds.sessionsCount}</strong></span>
        ${trackPillsHtml}
      </div>
      <div style="text-align:right; margin-left:12px; flex-shrink:0;">
        <span style="color:var(--neon-green); font-weight:bold; font-size:16px;">${bestDisp}</span><br>
        <span style="font-size:11px; color:var(--text-muted);">${ds.bestTrackName ? ('📍 ' + ds.bestTrackName) : 'Bestzeit'}</span><br>
        <span style="font-size:11px; color:var(--neon-cyan);">Profil öffnen ▶</span>
      </div>
    </div>`;
  });

  list.innerHTML = sortBarHtml + (cardsHtml || '<p style="color:var(--text-muted); text-align:center; padding:20px;">Keine Fahrerdaten vorhanden.</p>');
}

function sortSessionResults(safeId, col) {
  openSessionsMap[safeId] = true;
  let curState = sessionResultsSortMap[safeId] || { col: 'pos', dir: 'asc' };
  if (curState.col === col) {
    curState.dir = (curState.dir === 'asc') ? 'desc' : 'asc';
  } else {
    curState.col = col;
    curState.dir = (col === 'driver' || col === 'car' || col === 'pos') ? 'asc' : 'desc';
  }
  sessionResultsSortMap[safeId] = curState;
  switchStatsTab(currentTab);
}

function renderSessionsByMode(mode, containerId) {
  let container = document.getElementById(containerId);
  if (!container) return;

  let isDrag = (mode === 4);
  let sessions = allSessionsCache.filter(s => {
    if (s.mode !== mode) return false;
    if (currentStatsTrackFilter !== 'all' && s.trackId !== currentStatsTrackFilter) return false;
    return true;
  });

  sessions.sort((a, b) => (b.endTs || b.startTs || 0) - (a.endTs || a.startTs || 0));

  if (sessions.length === 0) {
    container.innerHTML = `
      <div style="text-align:center; padding:30px 15px; color:var(--text-muted);">
        <p style="font-size:14px; margin-bottom:12px;">Keine gespeicherten Durchgänge für diese Auswahl im Index vorhanden.</p>
        <button class="btn btn-purple" style="font-size:11px; padding:6px 14px;" onclick="rebuildSessionsIndexUI()">🔄 Index aus Dateisystem neu aufbauen</button>
      </div>`;
    return;
  }

  let roundWord = isDrag ? "Läufe" : "Runden";
  let roundTimeWord = isDrag ? "Laufzeiten" : "Rundenzeiten";
  let isAdmin = (typeof userRole !== 'undefined' && userRole === 'admin');

  let html = '';
  sessions.forEach(s => {
    let safeId = sanitizeDomId(s.id);
    let badgeClass = 'session-card';
    let headerColor = 'var(--neon-green)';
    if (s.mode === 1) { badgeClass += ' quali'; headerColor = 'var(--neon-purple)'; }
    else if (s.mode === 2 || s.mode === 3) { badgeClass += ' race'; headerColor = 'var(--neon-cyan)'; }
    else if (s.mode === 4) { badgeClass += ' drag'; headerColor = 'var(--neon-orange)'; }

    let dateStr = formatTimestamp(s.endTs || s.startTs);
    let trackName = getTrackNameById(s.trackId, s.trackName);

    let sessionOverallBest = 0;
    (s.results || []).forEach(r => {
      if (r.bestLap > 0 && (sessionOverallBest === 0 || r.bestLap < sessionOverallBest)) {
        sessionOverallBest = r.bestLap;
      }
    });

    let deleteBtnHtml = isAdmin
      ? `<button class="btn-icon" onclick="event.stopPropagation(); deleteSession('${s.id}')" title="Durchgang löschen">🗑</button>`
      : '';

    let resSort = sessionResultsSortMap[safeId] || { col: 'pos', dir: 'asc' };
    let sortedResults = [...(s.results || [])];
    sortedResults.sort((a, b) => {
      let res = 0;
      if (resSort.col === 'pos') res = (a.pos || 99) - (b.pos || 99);
      else if (resSort.col === 'driver') res = (a.driver || '').localeCompare(b.driver || '');
      else if (resSort.col === 'car') res = (a.car || '').localeCompare(b.car || '');
      else if (resSort.col === 'laps') res = (a.laps || 0) - (b.laps || 0);
      else if (resSort.col === 'bestLap') {
        let vA = a.bestLap > 0 ? a.bestLap : 99999999;
        let vB = b.bestLap > 0 ? b.bestLap : 99999999;
        res = vA - vB;
      }
      else if (resSort.col === 'avgLap') {
        let vA = a.avgLap > 0 ? a.avgLap : 99999999;
        let vB = b.avgLap > 0 ? b.avgLap : 99999999;
        res = vA - vB;
      }
      return (resSort.dir === 'asc') ? res : -res;
    });

    let isSessionOpen = !!openSessionsMap[safeId];
    let sessionDetailDisplay = isSessionOpen ? 'block' : 'none';
    let sessionArrowText = isSessionOpen ? 'Details 🔼' : 'Details 🔽';

    html += `<div id="session_card_${safeId}" class="${badgeClass}">
      <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:6px;" onclick="toggleSessionDetails('${safeId}')">
        <div style="display:flex; align-items:center; flex-wrap:wrap; gap:6px;">
          <strong style="color:${headerColor}; font-size:15px;">${s.modeName || "Durchgang"}</strong>
          <span class="session-track-pill">📍 ${trackName}</span>
          <span style="font-size:12px; color:var(--text-muted);">📅 ${dateStr}</span>
          <small style="color:var(--text-muted); font-size:10px; opacity:0.7;">(${s.id})</small>
        </div>
        <div style="display:flex; align-items:center; gap:8px;">
          <span id="session_arrow_${safeId}" style="font-size:12px; color:var(--neon-cyan); cursor:pointer;">${sessionArrowText}</span>
          ${deleteBtnHtml}
        </div>
      </div>

      <div id="session_detail_${safeId}" class="session-details" style="display:${sessionDetailDisplay}; margin-top:10px;">
        <div class="table-responsive">
          <table>
            <thead>
              <tr>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'pos')">Pos ${getSortIconHtml(resSort, 'pos')}</th>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'driver')">Fahrer ${getSortIconHtml(resSort, 'driver')}</th>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'car')">Fahrzeug ${getSortIconHtml(resSort, 'car')}</th>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'laps')">${roundWord} ${getSortIconHtml(resSort, 'laps')}</th>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'bestLap')">Beste Zeit ${getSortIconHtml(resSort, 'bestLap')}</th>
                <th class="sortable" onclick="event.stopPropagation(); sortSessionResults('${safeId}', 'avgLap')">&Oslash;-Zeit ${getSortIconHtml(resSort, 'avgLap')}</th>
                <th>${roundTimeWord}</th>
              </tr>
            </thead>
            <tbody>`;

    sortedResults.forEach(r => {
      let isOverallWinner = (sessionOverallBest > 0 && r.bestLap > 0 && r.bestLap <= sessionOverallBest);
      let bestColor = isOverallWinner ? 'var(--neon-purple)' : 'var(--neon-green)';
      let bestStr = r.bestLap > 0 ? (r.bestLap / 1000).toFixed(2) + 's' : '---';
      let avgStr = r.avgLap > 0 ? (r.avgLap / 1000).toFixed(2) + 's' : '---';
      let driverDetailId = `s_${safeId}_driver_${r.id}`;
      let dName = carNamesMap[r.id]?.driver || r.driver || ("Fahrer " + r.id);
      let cName = carNamesMap[r.id]?.car || r.car || ("Auto " + r.id);

      let isDriverOpen = !!openDriverDetailsMap[driverDetailId];
      let driverBtnText = isDriverOpen ? 'Details 🔼' : 'Details 🔽';
      let driverTrDisplay = isDriverOpen ? 'table-row' : 'none';

      html += `<tr>
        <td><strong>P${r.pos}</strong></td>
        <td><span class="driver-name" onclick="openDriverStats(${r.id})" style="color:var(--neon-cyan); font-weight:bold; cursor:pointer;">${dName}</span></td>
        <td style="color:var(--text-muted);">${cName}</td>
        <td>${r.laps}</td>
        <td style="color:${bestColor}; font-weight:bold;">${bestStr}</td>
        <td style="color:var(--text-muted);">${avgStr}</td>
        <td>
          <button class="btn btn-purple" id="btn_arrow_${driverDetailId}" style="padding:2px 8px; font-size:11px; margin:0;" onclick="toggleDriverLapsInSession('${driverDetailId}', '${s.id}', '${s.file}', ${r.id}, ${sessionOverallBest}, ${isDrag ? 'true' : 'false'})">${driverBtnText}</button>
        </td>
      </tr>
      <tr id="${driverDetailId}_tr" style="display:${driverTrDisplay}; background: rgba(0,0,0,0.35);">
        <td colspan="7">
          <div id="${driverDetailId}_content" class="driver-lap-details" style="display:${isDriverOpen ? 'block' : 'none'};">Lade ${roundTimeWord}...</div>
        </td>
      </tr>`;
    });

    html += `</tbody></table></div></div></div>`;
  });

  container.innerHTML = html;

  // Bereits offene Fahrerdetails direkt wieder rendern
  Object.keys(openDriverDetailsMap).forEach(elemId => {
    if (openDriverDetailsMap[elemId] && embeddedLapsState[elemId]) {
      let contentElem = document.getElementById(elemId + '_content');
      if (contentElem) {
        renderEmbeddedLapsTable(elemId, contentElem);
      }
    }
  });
}

function toggleSessionDetails(safeId, forceOpen = null) {
  let el = document.getElementById('session_detail_' + safeId);
  let arrow = document.getElementById('session_arrow_' + safeId);
  let isCurrentlyOpen = !!openSessionsMap[safeId];
  let nextOpen = (forceOpen !== null) ? forceOpen : !isCurrentlyOpen;

  if (nextOpen) openSessionsMap[safeId] = true;
  else delete openSessionsMap[safeId];

  if (el) el.style.display = nextOpen ? 'block' : 'none';
  if (arrow) arrow.innerText = nextOpen ? 'Details 🔼' : 'Details 🔽';
}

async function fetchSessionFileData(filePath) {
  if (!filePath) return null;
  if (sessionFileCache[filePath]) return sessionFileCache[filePath];
  let res = await fetch('/api/sessionfile?file=' + encodeURIComponent(filePath) + '&_t=' + Date.now(), { cache: 'no-store' });
  if (!res.ok) throw new Error('HTTP ' + res.status);
  let data = await res.json();
  if (data && Array.isArray(data.laps)) {
    data.laps = data.laps.map(normalizeLap);
  }
  sessionFileCache[filePath] = data;
  return data;
}

async function toggleDriverLapsInSession(elemId, sessionId, sessionFile, carId, sessionOverallBest, isDrag, forceOpen = false) {
  let trElem = document.getElementById(elemId + '_tr');
  let contentElem = document.getElementById(elemId + '_content');
  let btnElem = document.getElementById('btn_arrow_' + elemId);

  if (!trElem || !contentElem) return;

  let isCurrentlyOpen = !!openDriverDetailsMap[elemId];
  if (!forceOpen && isCurrentlyOpen) {
    trElem.style.display = 'none';
    delete openDriverDetailsMap[elemId];
    if (btnElem) btnElem.innerText = 'Details 🔽';
    return;
  }

  openDriverDetailsMap[elemId] = true;
  trElem.style.display = 'table-row';
  contentElem.style.display = 'block';
  if (btnElem) btnElem.innerText = 'Details 🔼';

  if (embeddedLapsState[elemId] && embeddedLapsState[elemId].laps) {
    renderEmbeddedLapsTable(elemId, contentElem);
    return;
  }

  contentElem.innerHTML = 'Lade Zeiten...';

  try {
    let data = await fetchSessionFileData(sessionFile);
    if (!data || !data.laps || data.laps.length === 0) {
      contentElem.innerHTML = '<span style="color:var(--text-muted);">Keine Rundenzeiten in dieser Session-Datei vorhanden.</span>';
      return;
    }

    let runLaps = data.laps.filter(l => l.carId === carId);
    if (runLaps.length === 0) {
      contentElem.innerHTML = '<span style="color:var(--text-muted);">Keine Rundenzeiten für diesen Fahrer in diesem Durchgang erfasst.</span>';
      return;
    }

    let sortState = embeddedLapsState[elemId] || { col: 'lap', dir: 'desc' };
    embeddedLapsState[elemId] = {
      laps: runLaps,
      sessionOverallBest: sessionOverallBest || 0,
      isDrag: isDrag,
      sessionId: sessionId,
      sessionFile: sessionFile,
      carId: carId,
      col: sortState.col,
      dir: sortState.dir
    };

    renderEmbeddedLapsTable(elemId, contentElem);
  } catch (e) {
    console.error(e);
    contentElem.innerHTML = '<span style="color:var(--neon-red);">Fehler beim Laden der Session-Datei.</span>';
  }
}

function sortEmbeddedDriverLaps(elemId, col) {
  let state = embeddedLapsState[elemId];
  if (!state) return;
  if (state.col === col) {
    state.dir = (state.dir === 'asc') ? 'desc' : 'asc';
  } else {
    state.col = col;
    state.dir = (col === 'lap' || col === 'ts') ? 'desc' : 'asc';
  }
  let contentElem = document.getElementById(elemId + '_content');
  if (contentElem) {
    renderEmbeddedLapsTable(elemId, contentElem);
  }
}

function renderEmbeddedLapsTable(elemId, containerElem) {
  if (containerElem) containerElem.style.display = 'block';
  let state = embeddedLapsState[elemId];
  if (!state || !state.laps) return;

  let laps = state.laps;
  let sessionOverallBest = state.sessionOverallBest;
  let isDrag = state.isDrag;
  let sessionId = state.sessionId;
  let sessionFile = state.sessionFile;
  let carId = state.carId;

  let minSz1 = 0, minSz2 = 0, minSz3 = 0, minSz4 = 0, minTime = 0;
  let maxSz1 = 0, maxSz2 = 0, maxSz3 = 0, maxSz4 = 0, maxTime = 0;
  let anySz2 = false, anySz3 = false, anySz4 = false;

  laps.forEach(l => {
    if (l.outlap) return;
    if (l.sz1 > 0) {
      if (minSz1 === 0 || l.sz1 < minSz1) minSz1 = l.sz1;
      if (maxSz1 === 0 || l.sz1 > maxSz1) maxSz1 = l.sz1;
    }
    if (l.sz2 > 0) {
      anySz2 = true;
      if (minSz2 === 0 || l.sz2 < minSz2) minSz2 = l.sz2;
      if (maxSz2 === 0 || l.sz2 > maxSz2) maxSz2 = l.sz2;
    }
    if (l.sz3 > 0) {
      anySz3 = true;
      if (minSz3 === 0 || l.sz3 < minSz3) minSz3 = l.sz3;
      if (maxSz3 === 0 || l.sz3 > maxSz3) maxSz3 = l.sz3;
    }
    if (!isDrag && l.sz4 > 0) {
      anySz4 = true;
      if (minSz4 === 0 || l.sz4 < minSz4) minSz4 = l.sz4;
      if (maxSz4 === 0 || l.sz4 > maxSz4) maxSz4 = l.sz4;
    }
    if (l.time > 0) {
      if (minTime === 0 || l.time < minTime) minTime = l.time;
      if (maxTime === 0 || l.time > maxTime) maxTime = l.time;
    }
  });

  let showSz2Col = anySz2 || activeUseSz2 || activeUseSz1;
  let showSz3Col = anySz3 || activeUseSz3 || activeUseSz2;
  let showSz4Col = !isDrag && (anySz4 || activeUseSz3);

  let allowWorst = (laps.length > 1);
  let effectiveSessionBest = (sessionOverallBest > 0) ? sessionOverallBest : minTime;

  let roundLabel = isDrag ? "Lauf" : "Runde";
  let timeLabel = isDrag ? "Laufzeit" : "Zeit";
  let isAdmin = (typeof userRole !== 'undefined' && userRole === 'admin');

  let sortedLaps = [...laps];
  sortedLaps.sort((a, b) => {
    let res = 0;
    if (state.col === 'lap') res = a.lap - b.lap;
    else if (state.col === 'time') {
      let vA = a.time > 0 ? a.time : 99999999;
      let vB = b.time > 0 ? b.time : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz1') {
      let vA = a.sz1 > 0 ? a.sz1 : 99999999;
      let vB = b.sz1 > 0 ? b.sz1 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz2') {
      let vA = a.sz2 > 0 ? a.sz2 : 99999999;
      let vB = b.sz2 > 0 ? b.sz2 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz3') {
      let vA = a.sz3 > 0 ? a.sz3 : 99999999;
      let vB = b.sz3 > 0 ? b.sz3 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz4') {
      let vA = a.sz4 > 0 ? a.sz4 : 99999999;
      let vB = b.sz4 > 0 ? b.sz4 : 99999999;
      res = vA - vB;
    } else if (state.col === 'pos') {
      res = (a.pos || 99) - (b.pos || 99);
    } else if (state.col === 'ts') {
      res = a.ts - b.ts;
    }
    return (state.dir === 'asc') ? res : -res;
  });

  let html = `<div class="table-responsive"><table style="margin:0; font-size:12px;">
    <thead>
      <tr>
        <th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'lap')">${roundLabel} ${getSortIconHtml(state, 'lap')}</th>
        <th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'sz1')">SZ1 ${getSortIconHtml(state, 'sz1')}</th>
        ${showSz2Col ? `<th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'sz2')">SZ2 ${getSortIconHtml(state, 'sz2')}</th>` : ''}
        ${showSz3Col ? `<th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'sz3')">SZ3 ${getSortIconHtml(state, 'sz3')}</th>` : ''}
        ${showSz4Col ? `<th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'sz4')">SZ4 ${getSortIconHtml(state, 'sz4')}</th>` : ''}
        <th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'time')">${timeLabel} ${getSortIconHtml(state, 'time')}</th>
        <th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'pos')">Platz ${getSortIconHtml(state, 'pos')}</th>
        <th class="sortable" onclick="event.stopPropagation(); sortEmbeddedDriverLaps('${elemId}', 'ts')">Uhrzeit ${getSortIconHtml(state, 'ts')}</th>
        ${isAdmin ? '<th>Aktion</th>' : ''}
      </tr>
    </thead>
    <tbody>`;

  sortedLaps.forEach((l) => {
    let displayNum = l.lap;

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
    let styleSz4 = showSz4Col ? getSectorStyle(l.sz4, minSz4, maxSz4) : '';
    let styleTime = getTimeStyle(l.time, minTime, maxTime);

    let formatCell = (szVal, splitVal, style) => {
      let valDisp = szVal > 0 ? (szVal / 1000).toFixed(2) + 's' : '---';
      if (isDrag) {
        let splitDisp = splitVal > 0 ? (splitVal / 1000).toFixed(2) + 's' : '---';
        return `<div class="drag-cell"><span class="drag-split" style="font-size:10px; color:var(--text-muted); display:block;">Spl: ${splitDisp}</span><span class="drag-sector" style="${style}">${valDisp}</span></div>`;
      }
      return `<span style="${style}">${valDisp}</span>`;
    };

    let split1 = l.sz1 || 0;
    let split2 = (l.sz2 > 0) ? (split1 + l.sz2) : 0;
    let split3 = (l.sz3 > 0) ? (isDrag ? l.time : (split2 + l.sz3)) : 0;

    let sz1Content = formatCell(l.sz1, split1, styleSz1);
    let sz2Content = formatCell(l.sz2, split2, styleSz2);
    let sz3Content = formatCell(l.sz3, split3, styleSz3);
    let sz4Content = showSz4Col ? formatCell(l.sz4, 0, styleSz4) : '';

    let delCol = isAdmin
      ? `<td><button class="btn-icon" title="Runde löschen" onclick="deleteSingleLapFromSession('${sessionId}', '${sessionFile}', ${carId}, ${l.lap})">🗑</button></td>`
      : '';

    html += `<tr>
      <td>${roundLabel} ${displayNum}</td>
      <td>${sz1Content}</td>
      ${showSz2Col ? `<td>${sz2Content}</td>` : ''}
      ${showSz3Col ? `<td>${sz3Content}</td>` : ''}
      ${showSz4Col ? `<td>${sz4Content}</td>` : ''}
      <td>${l.outlap ? `<span class="outlap-badge">Outlap</span> <small style="color:var(--text-muted); font-weight:normal;">(${(l.time / 1000).toFixed(2)}s)</small>` : `<span style="${styleTime}; font-weight:bold;">${(l.time / 1000).toFixed(2)}s</span>`}</td>
      <td style="color:var(--neon-orange); font-weight:bold;">${l.pos ? 'P' + l.pos : '---'}</td>
      <td style="color:var(--text-muted);">${formatTimestamp(l.ts)}</td>
      ${delCol}
    </tr>`;
  });

  html += '</tbody></table></div>';
  containerElem.innerHTML = html;
}

async function deleteSingleLapFromSession(sessionId, sessionFile, carId, lapNum) {
  let ok = await showConfirm({
    title: "🗑️ RUNDE LÖSCHEN",
    theme: "danger",
    boxLabel: "Ausgewählte Runde:",
    boxContent: `Runde ${lapNum} (Auto ${carId})`,
    message: `Möchtest du Runde ${lapNum} wirklich dauerhaft aus dieser Session löschen?<br><span style="color:var(--neon-red); font-size:12px; display:block; margin-top:6px;">⚠️ Die Rundenzeit wird unwiderruflich entfernt!</span>`,
    confirmText: "LÖSCHEN 🗑️",
    cancelText: "ABBRECHEN"
  });
  if (!ok) return;
  try {
    let lapUrl = `/api/deletelap?sessionId=${encodeURIComponent(sessionId)}${sessionFile ? `&file=${encodeURIComponent(sessionFile)}` : ''}&id=${carId}&lap=${lapNum}`;
    await fetch(lapUrl, { method: 'POST' });
    delete sessionFileCache[sessionFile];
    let safeId = sanitizeDomId(sessionId);
    let elemId = `s_${safeId}_driver_${carId}`;
    if (embeddedLapsState[elemId]) {
      delete embeddedLapsState[elemId].laps;
    }
    if (typeof loadTracks === 'function') await loadTracks();
    await fetchSessionsAndStorageInfo();
    switchStatsTab(currentTab);
    if (openDriverDetailsMap[elemId]) {
      let sObj = allSessionsCache.find(s => s.id === sessionId);
      let sessionOverallBest = 0;
      if (sObj && sObj.results) {
        sObj.results.forEach(r => {
          if (r.bestLap > 0 && (sessionOverallBest === 0 || r.bestLap < sessionOverallBest)) {
            sessionOverallBest = r.bestLap;
          }
        });
      }
      toggleDriverLapsInSession(elemId, sessionId, sessionFile, carId, sessionOverallBest, sObj ? sObj.mode === 4 : false, true);
    }
  } catch (err) {
    console.error(err);
  }
}

async function deleteSession(sessionId) {
  let sObj = allSessionsCache.find(s => s.id === sessionId);
  let sessionFile = sObj ? sObj.file : '';

  let ok = await showConfirm({
    title: "🗑️ SESSION LÖSCHEN",
    theme: "danger",
    boxLabel: "Durchgang:",
    boxContent: sessionId,
    message: "Möchtest du diesen Durchgang inklusive aller gespeicherten Rundenzeiten wirklich unwiderruflich löschen?<br><span style=\"color:var(--neon-red); font-size:12px; display:block; margin-top:6px;\">⚠️ Die Datei wird dauerhaft aus dem Flash-Speicher entfernt!</span>",
    confirmText: "LÖSCHEN 🗑️",
    cancelText: "ABBRECHEN"
  });
  if (!ok) return;
  try {
    // 1. Physische Session-Datei im LittleFS löschen
    if (sessionFile) {
      let fd = new FormData();
      fd.append('file', sessionFile);
      await fetch('/api/deletefile', { method: 'POST', body: fd }).catch(() => {});
    }
    await fetch('/api/deletesession?id=' + encodeURIComponent(sessionId), { method: 'POST' }).catch(() => {});

    // 2. Client-Cache sofort aktualisieren
    allSessionsCache = allSessionsCache.filter(s => s.id !== sessionId);

    // 3. Bereinigte /sessions.json kompakt hochladen
    let idxBlob = new Blob([JSON.stringify({ sessions: allSessionsCache })], { type: 'application/json' });
    let upFd = new FormData();
    upFd.append('data', idxBlob, '/sessions.json');
    await fetch('/api/upload', { method: 'POST', body: upFd }).catch(() => {});

    delete sessionFileCache[sessionFile];
    let safeId = sanitizeDomId(sessionId);
    delete openSessionsMap[safeId];
    if (typeof loadTracks === 'function') await loadTracks();
    await fetchSessionsAndStorageInfo();
    switchStatsTab(currentTab);
  } catch (e) {
    console.error("Fehler beim Löschen der Session:", e);
  }
}

async function jumpToSessionInStats(sessionId, mode, trackId, carId = null) {
  closeStatsModal();

  let overviewModal = document.getElementById('statsOverviewModal');
  if (!overviewModal || overviewModal.style.display !== 'block') {
    if (overviewModal) overviewModal.style.display = 'block';
    await fetchSessionsAndStorageInfo();
  }

  if (currentStatsTrackFilter !== 'all' && trackId && currentStatsTrackFilter !== trackId) {
    currentStatsTrackFilter = 'all';
    let sel = document.getElementById('statsTrackFilterSelect');
    if (sel) sel.value = 'all';
    updateStatsModalDailyBest();
  }

  let targetTab = modeToTabName(mode);
  switchStatsTab(targetTab);

  let safeId = sanitizeDomId(sessionId);
  toggleSessionDetails(safeId, true);

  let card = document.getElementById('session_card_' + safeId);
  if (card) {
    card.classList.remove('session-highlight-flash');
    void card.offsetWidth;
    card.classList.add('session-highlight-flash');
    setTimeout(() => {
      card.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }, 80);
  }

  if (carId !== null) {
    let sObj = allSessionsCache.find(s => s.id === sessionId);
    if (sObj && sObj.file) {
      let sessionOverallBest = 0;
      (sObj.results || []).forEach(r => {
        if (r.bestLap > 0 && (sessionOverallBest === 0 || r.bestLap < sessionOverallBest)) {
          sessionOverallBest = r.bestLap;
        }
      });
      let driverDetailId = `s_${safeId}_driver_${carId}`;
      toggleDriverLapsInSession(driverDetailId, sObj.id, sObj.file, carId, sessionOverallBest, sObj.mode === 4, true);
    }
  }
}

async function openDriverStats(carId) {
  activeStatsCarId = carId;
  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);
  let carName = carNamesMap[carId]?.car || ("Auto " + carId);

  let titleElem = document.getElementById('statsModalTitle');
  if (titleElem) titleElem.innerText = `${driverName} (${carName} - ID ${carId})`;

  let delBtn = document.getElementById('deleteDriverStatsBtn');
  if (delBtn) delBtn.innerText = "Alle Einträge von " + driverName + " löschen";

  closeStatsOverviewModal();
  let modal = document.getElementById('statsModal');
  if (modal) modal.style.display = 'block';

  let container = document.getElementById('driverProfileContent');
  if (container) container.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:20px;">Lade Fahrer-Statistik...</p>';

  await Promise.all([
    loadCarNames(),
    typeof loadTracks === 'function' ? loadTracks() : Promise.resolve(),
    fetchSessionsAndStorageInfo()
  ]);

  driverProfileAllLapsState = null;
  renderDriverProfile(carId);
}

function sortDriverProfileSessions(col) {
  if (driverProfileSessionSort.col === col) {
    driverProfileSessionSort.dir = (driverProfileSessionSort.dir === 'asc') ? 'desc' : 'asc';
  } else {
    driverProfileSessionSort.col = col;
    driverProfileSessionSort.dir = (col === 'ts' || col === 'laps') ? 'desc' : 'asc';
  }
  if (activeStatsCarId) {
    renderDriverProfile(activeStatsCarId);
  }
}

function sortDriverProfileAllLaps(col) {
  if (!driverProfileAllLapsState) return;
  if (driverProfileAllLapsState.col === col) {
    driverProfileAllLapsState.dir = (driverProfileAllLapsState.dir === 'asc') ? 'desc' : 'asc';
  } else {
    driverProfileAllLapsState.col = col;
    driverProfileAllLapsState.dir = (col === 'time' || col.startsWith('sz') || col === 'pos') ? 'asc' : 'desc';
  }
  renderDriverProfileAllLapsTable();
}

async function toggleDriverProfileAllLaps(carId) {
  let box = document.getElementById('driverAllLapsBox');
  let btn = document.getElementById('driverAllLapsToggleBtn');
  if (!box) return;

  if (box.style.display === 'block') {
    box.style.display = 'none';
    driverProfileAllLapsState = null;
    if (btn) btn.innerText = '⏱️ Alle Runden & Sektorzeiten anzeigen 🔽';
    return;
  }

  box.style.display = 'block';
  if (btn) btn.innerText = '⏱️ Rundenzeiten ausblenden 🔼';
  box.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:15px;">Lade alle Rundenzeiten aus den Sessions...</p>';

  try {
    let allLaps = [];
    for (let s of allSessionsCache) {
      let r = (s.results || []).find(item => item.id === carId);
      if (r && s.file) {
        try {
          let sData = await fetchSessionFileData(s.file);
          if (sData && sData.laps) {
            let carLaps = sData.laps.filter(l => l.carId === carId);
            carLaps.forEach(l => {
              allLaps.push({
                ...l,
                sessionId: s.id,
                sessionFile: s.file,
                mode: s.mode,
                modeName: s.modeName || 'Durchgang',
                trackId: s.trackId || 'standard',
                trackName: getTrackNameById(s.trackId, s.trackName)
              });
            });
          }
        } catch (e) {}
      }
    }

    driverProfileAllLapsState = {
      laps: allLaps,
      col: 'ts',
      dir: 'desc'
    };

    renderDriverProfileAllLapsTable();
  } catch (err) {
    box.innerHTML = '<p style="color:var(--neon-red); text-align:center;">Fehler beim Laden der Rundenzeiten.</p>';
  }
}

function renderDriverProfileAllLapsTable() {
  let box = document.getElementById('driverAllLapsBox');
  if (!box || !driverProfileAllLapsState) return;

  let laps = driverProfileAllLapsState.laps;
  if (!laps || laps.length === 0) {
    box.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:15px;">Keine Rundenzeiten vorhanden.</p>';
    return;
  }

  let state = driverProfileAllLapsState;
  let sorted = [...laps];
  sorted.sort((a, b) => {
    let res = 0;
    if (state.col === 'lap') res = a.lap - b.lap;
    else if (state.col === 'time') {
      let vA = a.time > 0 ? a.time : 99999999;
      let vB = b.time > 0 ? b.time : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz1') {
      let vA = a.sz1 > 0 ? a.sz1 : 99999999;
      let vB = b.sz1 > 0 ? b.sz1 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz2') {
      let vA = a.sz2 > 0 ? a.sz2 : 99999999;
      let vB = b.sz2 > 0 ? b.sz2 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz3') {
      let vA = a.sz3 > 0 ? a.sz3 : 99999999;
      let vB = b.sz3 > 0 ? b.sz3 : 99999999;
      res = vA - vB;
    } else if (state.col === 'sz4') {
      let vA = a.sz4 > 0 ? a.sz4 : 99999999;
      let vB = b.sz4 > 0 ? b.sz4 : 99999999;
      res = vA - vB;
    } else if (state.col === 'track') {
      res = a.trackName.localeCompare(b.trackName);
    } else if (state.col === 'mode') {
      res = a.modeName.localeCompare(b.modeName);
    } else if (state.col === 'pos') {
      res = (a.pos || 99) - (b.pos || 99);
    } else if (state.col === 'ts') {
      res = a.ts - b.ts;
    }
    return (state.dir === 'asc') ? res : -res;
  });

  let validLaps = laps.filter(l => l.time > 0 && !l.outlap);
  let minTime = validLaps.length > 0 ? Math.min(...validLaps.map(l => l.time)) : 0;

  let html = `<div class="table-responsive" style="max-height:300px; overflow-y:auto;"><table style="margin:0; font-size:12px;">
    <thead>
      <tr>
        <th class="sortable" onclick="sortDriverProfileAllLaps('lap')">Runde ${getSortIconHtml(state, 'lap')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('time')">Zeit ${getSortIconHtml(state, 'time')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('sz1')">SZ1 ${getSortIconHtml(state, 'sz1')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('sz2')">SZ2 ${getSortIconHtml(state, 'sz2')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('sz3')">SZ3 ${getSortIconHtml(state, 'sz3')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('sz4')">SZ4 ${getSortIconHtml(state, 'sz4')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('track')">Strecke ${getSortIconHtml(state, 'track')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('mode')">Modus ${getSortIconHtml(state, 'mode')}</th>
        <th class="sortable" onclick="sortDriverProfileAllLaps('ts')">Datum ${getSortIconHtml(state, 'ts')}</th>
        <th>Session</th>
      </tr>
    </thead>
    <tbody>`;

  sorted.forEach(l => {
    let isBest = (!l.outlap && l.time > 0 && l.time === minTime);
    let timeCol = isBest ? 'var(--neon-purple)' : 'var(--text-main)';
    let szDisp = (v) => v > 0 ? (v / 1000).toFixed(2) + 's' : '---';

    html += `<tr>
      <td>${l.mode === 4 ? 'Lauf' : 'Rnd'} ${l.lap}</td>
      <td>${l.outlap ? `<span class="outlap-badge">Outlap</span> <small style="color:var(--text-muted); font-weight:normal;">(${(l.time / 1000).toFixed(2)}s)</small>` : `<span style="color:${timeCol}; font-weight:bold;">${(l.time / 1000).toFixed(2)}s</span>`}</td>
      <td style="color:var(--text-muted); font-size:11px;">${szDisp(l.sz1)}</td>
      <td style="color:var(--text-muted); font-size:11px;">${szDisp(l.sz2)}</td>
      <td style="color:var(--text-muted); font-size:11px;">${szDisp(l.sz3)}</td>
      <td style="color:var(--text-muted); font-size:11px;">${szDisp(l.sz4)}</td>
      <td><span class="session-track-pill">📍 ${l.trackName}</span></td>
      <td style="font-size:11px;">${l.modeName}</td>
      <td style="font-size:11px; color:var(--text-muted);">${formatTimestamp(l.ts)}</td>
      <td><button class="btn btn-purple" style="padding:2px 6px; font-size:10px; margin:0;" onclick="jumpToSessionInStats('${l.sessionId}', ${l.mode}, '${l.trackId}', ${activeStatsCarId})">🔗</button></td>
    </tr>`;
  });

  html += '</tbody></table></div>';
  box.innerHTML = html;
}

function renderDriverProfile(carId) {
  let container = document.getElementById('driverProfileContent');
  if (!container) return;

  let totalLaps = 0;
  let participatedSessions = [];
  let trackBestsMap = {};
  let overallBest = null;

  allSessionsCache.forEach(s => {
    let r = (s.results || []).find(item => item.id === carId);
    if (!r || (r.laps === 0 && !r.bestLap)) return;

    let tId = s.trackId || 'standard';
    let tName = getTrackNameById(tId, s.trackName);

    totalLaps += (r.laps || 0);
    participatedSessions.push({
      session: s,
      result: r,
      trackId: tId,
      trackName: tName
    });

    if (!trackBestsMap[tId]) {
      trackBestsMap[tId] = {
        trackId: tId,
        trackName: tName,
        bestLap: 0,
        sessionId: s.id,
        mode: s.mode,
        modeName: s.modeName || 'Durchgang',
        ts: s.endTs || s.startTs,
        totalTrackLaps: 0,
        trackSessionsCount: 0
      };
    }

    let tb = trackBestsMap[tId];
    tb.totalTrackLaps += (r.laps || 0);
    tb.trackSessionsCount += 1;

    if (r.bestLap > 0) {
      if (tb.bestLap === 0 || r.bestLap < tb.bestLap) {
        tb.bestLap = r.bestLap;
        tb.sessionId = s.id;
        tb.mode = s.mode;
        tb.modeName = s.modeName || 'Durchgang';
        tb.ts = s.endTs || s.startTs;
      }

      if (!overallBest || r.bestLap < overallBest.bestLap) {
        overallBest = {
          bestLap: r.bestLap,
          sessionId: s.id,
          mode: s.mode,
          modeName: s.modeName || 'Durchgang',
          trackId: tId,
          trackName: tName,
          ts: s.endTs || s.startTs
        };
      }
    }
  });

  participatedSessions.sort((a, b) => {
    let res = 0;
    let sA = a.session;
    let sB = b.session;
    let rA = a.result;
    let rB = b.result;

    if (driverProfileSessionSort.col === 'ts') {
      let tsA = sA.endTs || sA.startTs || 0;
      let tsB = sB.endTs || sB.startTs || 0;
      res = tsA - tsB;
    } else if (driverProfileSessionSort.col === 'track') {
      res = a.trackName.localeCompare(b.trackName);
    } else if (driverProfileSessionSort.col === 'mode') {
      res = (sA.modeName || '').localeCompare(sB.modeName || '');
    } else if (driverProfileSessionSort.col === 'pos') {
      res = (rA.pos || 99) - (rB.pos || 99);
    } else if (driverProfileSessionSort.col === 'laps') {
      res = (rA.laps || 0) - (rB.laps || 0);
    } else if (driverProfileSessionSort.col === 'bestLap') {
      let vA = rA.bestLap > 0 ? rA.bestLap : 99999999;
      let vB = rB.bestLap > 0 ? rB.bestLap : 99999999;
      res = vA - vB;
    }
    return (driverProfileSessionSort.dir === 'asc') ? res : -res;
  });

  if (participatedSessions.length === 0) {
    container.innerHTML = '<p style="color:var(--text-muted); text-align:center; padding:30px;">Für diesen Fahrer wurden noch keine Sessions aufgezeichnet.</p>';
    return;
  }

  let overallCardHtml = '';
  if (overallBest) {
    let obSec = (overallBest.bestLap / 1000).toFixed(2) + 's';
    overallCardHtml = `
      <div style="background:rgba(181,50,255,0.1); border:1px solid var(--neon-purple); border-radius:8px; padding:12px; text-align:center; cursor:pointer;" onclick="jumpToSessionInStats('${overallBest.sessionId}', ${overallBest.mode}, '${overallBest.trackId}', ${carId})" title="Klicken, um direkt zur Session dieser Bestzeit zu springen">
        <div style="font-size:11px; color:var(--text-muted); text-transform:uppercase;">Schnellste Runde (Gesamt)</div>
        <div style="font-size:22px; font-weight:bold; color:var(--neon-purple); margin:4px 0;">${obSec} 🔗</div>
        <div style="font-size:11px; color:var(--neon-cyan);">📍 ${overallBest.trackName} (${overallBest.modeName})</div>
      </div>`;
  } else {
    overallCardHtml = `
      <div style="background:rgba(255,255,255,0.03); border:1px solid var(--border-subtle); border-radius:8px; padding:12px; text-align:center;">
        <div style="font-size:11px; color:var(--text-muted); text-transform:uppercase;">Schnellste Runde</div>
        <div style="font-size:22px; font-weight:bold; color:var(--text-muted); margin:4px 0;">---</div>
      </div>`;
  }

  let html = `
    <div style="display:grid; grid-template-columns:repeat(auto-fit, minmax(180px, 1fr)); gap:10px; margin-bottom:18px;">
      <div style="background:rgba(0,240,255,0.07); border:1px solid rgba(0,240,255,0.3); border-radius:8px; padding:12px; text-align:center;">
        <div style="font-size:11px; color:var(--text-muted); text-transform:uppercase;">Gefahrene Runden</div>
        <div style="font-size:22px; font-weight:bold; color:var(--neon-cyan); margin:4px 0;">${totalLaps}</div>
        <div style="font-size:11px; color:var(--text-muted);">in ${participatedSessions.length} Sessions</div>
      </div>
      ${overallCardHtml}
      <div style="background:rgba(57,255,20,0.07); border:1px solid rgba(57,255,20,0.3); border-radius:8px; padding:12px; text-align:center;">
        <div style="font-size:11px; color:var(--text-muted); text-transform:uppercase;">Befahrene Strecken</div>
        <div style="font-size:22px; font-weight:bold; color:var(--neon-green); margin:4px 0;">${Object.keys(trackBestsMap).length}</div>
        <div style="font-size:11px; color:var(--text-muted);">Strecken-Profile</div>
      </div>
    </div>

    <h3 style="font-size:14px; color:var(--neon-cyan); margin:16px 0 6px 0;">🏆 Bestzeiten je Strecke <small style="color:var(--text-muted); font-weight:normal;">(Klick öffnet die zugehörige Session)</small></h3>
    <div class="driver-pb-track-grid">`;

  Object.values(trackBestsMap).forEach(tb => {
    let bestStr = tb.bestLap > 0 ? (tb.bestLap / 1000).toFixed(2) + 's' : '---';
    let dateStr = formatTimestamp(tb.ts);
    html += `
      <div class="driver-pb-track-card" onclick="jumpToSessionInStats('${tb.sessionId}', ${tb.mode}, '${tb.trackId}', ${carId})">
        <div style="display:flex; justify-content:space-between; align-items:center;">
          <strong style="color:var(--neon-cyan); font-size:14px;">📍 ${tb.trackName}</strong>
          <span style="color:var(--neon-green); font-weight:bold; font-size:16px;">${bestStr} 🔗</span>
        </div>
        <div style="font-size:11px; color:var(--text-muted); margin-top:6px; display:flex; justify-content:space-between; flex-wrap:wrap; gap:4px;">
          <span>🏁 ${tb.modeName} (${dateStr})</span>
          <span>🔄 ${tb.totalTrackLaps} Rnd. (${tb.trackSessionsCount} Sess.)</span>
        </div>
      </div>`;
  });

  html += `</div>
    <div style="display:flex; justify-content:space-between; align-items:center; margin:20px 0 8px 0; flex-wrap:wrap; gap:8px;">
      <h3 style="font-size:14px; color:var(--neon-cyan); margin:0;">📋 Teilgenommene Sessions (${participatedSessions.length})</h3>
      <button id="driverAllLapsToggleBtn" class="btn btn-purple" style="padding:4px 10px; font-size:11px; margin:0;" onclick="toggleDriverProfileAllLaps(${carId})">${(driverProfileAllLapsState ? '⏱️ Rundenzeiten ausblenden 🔼' : '⏱️ Alle Runden & Sektorzeiten anzeigen 🔽')}</button>
    </div>

    <div id="driverAllLapsBox" style="display:${driverProfileAllLapsState ? 'block' : 'none'}; margin-bottom:15px; background:rgba(0,0,0,0.4); border-radius:8px; border:1px solid rgba(181,50,255,0.3); padding:8px;"></div>

    <div class="table-responsive" style="max-height:340px; overflow-y:auto;">
      <table>
        <thead>
          <tr>
            <th class="sortable" onclick="sortDriverProfileSessions('ts')">Datum & Uhrzeit ${getSortIconHtml(driverProfileSessionSort, 'ts')}</th>
            <th class="sortable" onclick="sortDriverProfileSessions('track')">Strecke ${getSortIconHtml(driverProfileSessionSort, 'track')}</th>
            <th class="sortable" onclick="sortDriverProfileSessions('mode')">Modus ${getSortIconHtml(driverProfileSessionSort, 'mode')}</th>
            <th class="sortable" onclick="sortDriverProfileSessions('pos')">Platz ${getSortIconHtml(driverProfileSessionSort, 'pos')}</th>
            <th class="sortable" onclick="sortDriverProfileSessions('laps')">Runden ${getSortIconHtml(driverProfileSessionSort, 'laps')}</th>
            <th class="sortable" onclick="sortDriverProfileSessions('bestLap')">Bestzeit ${getSortIconHtml(driverProfileSessionSort, 'bestLap')}</th>
            <th>Session</th>
          </tr>
        </thead>
        <tbody>`;

  participatedSessions.forEach(item => {
    let s = item.session;
    let r = item.result;
    let dateStr = formatTimestamp(s.endTs || s.startTs);
    let bestStr = r.bestLap > 0 ? (r.bestLap / 1000).toFixed(2) + 's' : '---';

    let modeCol = 'var(--neon-green)';
    if (s.mode === 1) modeCol = 'var(--neon-purple)';
    else if (s.mode === 2 || s.mode === 3) modeCol = 'var(--neon-cyan)';
    else if (s.mode === 4) modeCol = 'var(--neon-orange)';

    html += `<tr style="cursor:pointer;" onclick="jumpToSessionInStats('${s.id}', ${s.mode}, '${item.trackId}', ${carId})">
      <td style="font-size:12px; color:var(--text-muted);">${dateStr}</td>
      <td><span class="session-track-pill">📍 ${item.trackName}</span></td>
      <td style="color:${modeCol}; font-weight:bold; font-size:12px;">${s.modeName || 'Durchgang'}</td>
      <td><strong style="color:var(--neon-orange);">P${r.pos}</strong></td>
      <td>${r.laps}</td>
      <td style="color:var(--neon-green); font-weight:bold;">${bestStr}</td>
      <td><button class="btn btn-purple" style="padding:2px 8px; font-size:11px; margin:0;" onclick="event.stopPropagation(); jumpToSessionInStats('${s.id}', ${s.mode}, '${item.trackId}', ${carId})">🔗 Öffnen</button></td>
    </tr>`;
  });

  html += `</tbody></table></div>`; setTimeout(() => { if (driverProfileAllLapsState) renderDriverProfileAllLapsTable(); }, 0);
  container.innerHTML = html;
}

function closeStatsModal() {
  let m = document.getElementById('statsModal');
  if (m) m.style.display = 'none';
  activeStatsCarId = null;
}

function closeStatsOverviewModal() {
  let m = document.getElementById('statsOverviewModal');
  if (m) m.style.display = 'none';
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

async function confirmDeleteDriverStats() {
  if (!activeStatsCarId) return;
  let driverName = carNamesMap[activeStatsCarId]?.driver || ("Fahrer " + activeStatsCarId);
  let ok = await showConfirm({
    title: "🗑️ FAHRER-DATEN LÖSCHEN",
    theme: "danger",
    boxLabel: "Fahrer:",
    boxContent: `${driverName} (Auto ${activeStatsCarId})`,
    message: `Möchtest du wirklich <strong>ALLE gespeicherten Einträge</strong> von <strong>${driverName}</strong> aus allen Sessions dauerhaft löschen?<br><span style="color:var(--neon-red); font-size:12px; display:block; margin-top:6px;">⚠️ Diese Aktion kann nicht rückgängig gemacht werden!</span>`,
    confirmText: "LÖSCHEN 🗑️",
    cancelText: "ABBRECHEN"
  });
  if (!ok) return;
  try {
    await fetch('/api/deletedriverstats?id=' + activeStatsCarId, { method: 'POST' });
    sessionFileCache = {};
    if (typeof loadTracks === 'function') await loadTracks();
    await fetchSessionsAndStorageInfo();
    renderDriverProfile(activeStatsCarId);
  } catch (err) {
    console.error(err);
  }
}

async function confirmDeleteAllStats() {
  let ok = await showConfirm({
    title: "🛑 ALLE STATISTIKEN LÖSCHEN",
    theme: "danger",
    boxLabel: "Aktion:",
    boxContent: "Komplett-Löschung aller Sessions & Streckenrekorde",
    message: "WARNUNG: Möchtest du wirklich <strong>ALLE Sessions und Streckenrekorde</strong> unwiderruflich löschen?<br><span style=\"color:var(--neon-red); font-size:12px; display:block; margin-top:8px;\">⚠️ Sämtliche Runden- und Renndaten aller Strecken werden dauerhaft vernichtet!</span>",
    confirmText: "ALLES LÖSCHEN 🛑",
    cancelText: "ABBRECHEN"
  });
  if (!ok) return;
  try {
    await fetch('/api/deleteallstats', { method: 'POST' });
    sessionFileCache = {};
    openSessionsMap = {};
    openDriverDetailsMap = {};
    await fetchSessionsAndStorageInfo();
    if (typeof loadTracks === 'function') await loadTracks();
    await showAlert({
      title: "✅ GELÖSCHT",
      theme: "success",
      message: "Alle Statistiken und Session-Dateien wurden erfolgreich gelöscht."
    });
    closeStatsOverviewModal();
  } catch (err) {
    console.error(err);
  }
}

async function downloadStatsBackup() {
  let btn = document.querySelector('button[onclick="downloadStatsBackup()"]');
  let originalText = btn ? btn.innerText : "💾 Backup herunterladen";

  if (btn) {
    btn.innerText = "⏳ Erstelle Backup... (Bitte warten)";
    btn.disabled = true;
  }

  let backupData = {
    version: 3,
    exportedAt: Math.floor(Date.now() / 1000),
    tracks: [],
    sessions: [],
    sessionFiles: {}
  };

  try {
    let tRes = await fetch('/api/tracks');
    if (tRes.ok) {
      let tData = await tRes.json();
      backupData.tracks = tData;
    }

    let sRes = await fetch('/api/sessions');
    if (sRes.ok) {
      let sData = await sRes.json();
      backupData.sessions = sData.sessions || [];
    }

    for (let s of backupData.sessions) {
      if (s.file) {
        try {
          let fData = await fetchSessionFileData(s.file);
          if (fData) backupData.sessionFiles[s.file] = fData;
        } catch (e) {}
      }
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
    await showAlert({
      title: "❌ BACKUP FEHLER",
      theme: "danger",
      message: "Fehler beim Erstellen des Backups: " + e.message
    });
  } finally {
    if (btn) {
      btn.innerText = originalText;
      btn.disabled = false;
    }
  }
}

function triggerRestoreBackup() {
  let inp = document.getElementById('backupFileInput');
  if (inp) inp.click();
}

async function handleRestoreBackup(event) {
  let file = event.target.files[0];
  if (!file) return;

  let ok = await showConfirm({
    title: "📦 BACKUP WIEDERHERSTELLEN",
    theme: "orange",
    boxLabel: "Ausgewählte Backup-Datei:",
    boxContent: file.name,
    boxSub: (file.size / 1024).toFixed(1) + " KB",
    message: "Soll dieses Backup jetzt auf dem ESP8266 wiederhergestellt werden?<br><span style=\"color:var(--neon-orange); font-size:12px; display:block; margin-top:6px;\">⚠️ Vorhandene Statistiken werden mit den Backup-Daten überschrieben.</span>",
    confirmText: "WIEDERHERSTELLEN 📦",
    cancelText: "ABBRECHEN"
  });

  if (!ok) {
    event.target.value = '';
    return;
  }

  try {
    let text = await file.text();
    let parsed = JSON.parse(text);

    if (parsed.version === 3 && parsed.sessionFiles) {
      // Upload /tracks.json
      if (parsed.tracks) {
        let tBlob = new Blob([JSON.stringify(parsed.tracks)], { type: 'application/json' });
        let fd = new FormData();
        fd.append('data', tBlob, '/tracks.json');
        await fetch('/api/upload', { method: 'POST', body: fd });
      }
      // Upload each session file
      let fileKeys = Object.keys(parsed.sessionFiles);
      for (let fPath of fileKeys) {
        let sBlob = new Blob([JSON.stringify(parsed.sessionFiles[fPath])], { type: 'application/json' });
        let fd = new FormData();
        fd.append('data', sBlob, fPath);
        await fetch('/api/upload', { method: 'POST', body: fd });
      }
      // Upload /sessions.json
      let idxBlob = new Blob([JSON.stringify({ sessions: parsed.sessions || [] })], { type: 'application/json' });
      let fdIdx = new FormData();
      fdIdx.append('data', idxBlob, '/sessions.json');
      await fetch('/api/upload', { method: 'POST', body: fdIdx });

      await showAlert({
        title: "✅ ERFOLG",
        theme: "success",
        message: "Backup erfolgreich wiederhergestellt!"
      });
      sessionFileCache = {};
      openSessionsMap = {};
      openDriverDetailsMap = {};
      await openStatsOverviewModal();
    } else {
      let formData = new FormData();
      formData.append('data', file, 'backup.json');
      let res = await fetch('/api/upload', { method: 'POST', body: formData });
      if (res.ok) {
        await showAlert({
          title: "✅ ERFOLG",
          theme: "success",
          message: "Backup erfolgreich hochgeladen!"
        });
        await openStatsOverviewModal();
      } else {
        await showAlert({
          title: "❌ FEHLER",
          theme: "danger",
          message: "Fehler beim Hochladen des Backups."
        });
      }
    }
  } catch (err) {
    await showAlert({
      title: "❌ FEHLER",
      theme: "danger",
      message: "Fehler beim Einlesen: " + err.message
    });
  }

  event.target.value = '';
}

async function rebuildSessionsIndexUI() {
  let ok = await showConfirm({
    title: "🔄 SYSTEM & REKORDE HEILEN",
    theme: "cyan",
    boxLabel: "Aktion:",
    boxContent: "Session-Scan, Rebuild, Rekord-Heilung & Telemetrie-Optimierung",
    message: "Das Endgerät durchsucht alle Session-Dateien im Speicher, generiert die Index-Datei (<code>/sessions.json</code>) neu, gleicht alle <strong>Streckenrekorde und Tagesbestzeiten (Option A)</strong> mit den echten Runden ab und optimiert alte Dateien automatisch auf das kompakte Minimal-Format.<br><span style=\"color:var(--neon-cyan); font-size:12px; display:block; margin-top:6px;\">⚡ 100% crashsicher, entfernt Geisterrekorde &amp; spart dauerhaft Speicherplatz.</span>",
    confirmText: "VOLLSTÄNDIG HEILEN 🔄",
    cancelText: "ABBRECHEN"
  });

  if (!ok) return;

  let btn = document.getElementById('rebuildIndexBtn');
  let origText = btn ? btn.innerHTML : '';
  if (btn) {
    btn.disabled = true;
    btn.innerText = "⏳ Lese Dateien...";
  }

  try {
    // 1. Liste aller Dateien von der Basis abfragen
    let res = await fetch('/api/files');
    if (!res.ok) throw new Error("Fehler beim Abrufen der Dateiliste");
    let fData = await res.json();
    let files = fData.files || [];

    // 2. Nur Session-Dateien herausfiltern (z.B. kirchhof/training/26-10-26_14-45-39.json)
    let sessionFiles = files.filter(f => {
      let n = f.name.startsWith('/') ? f.name.substring(1) : f.name;
      if (!n.endsWith('.json')) return false;
      if (n === 'tracks.json' || n === 'sessions.json' || n === 'wifi.json' || n === 'names.json' || n === 'daily_best.json') return false;
      return n.includes('/');
    });

    if (sessionFiles.length === 0) {
      await showAlert({
        title: "ℹ️ KEINE SESSIONS",
        theme: "cyan",
        message: "Im Dateisystem wurden keine Session-Dateien gefunden."
      });
      return;
    }

    if (btn) btn.innerText = `⏳ Verarbeite 0/${sessionFiles.length}...`;

    // 3. Jede Session-Datei laden und Metadaten aggregieren (und ggf. auf Kompakt-Schema schrumpfen)
    let builtSessions = [];
    let compressedCount = 0;
    for (let i = 0; i < sessionFiles.length; i++) {
      let sf = sessionFiles[i];
      let rawName = sf.name.startsWith('/') ? sf.name : ('/' + sf.name);
      if (btn) btn.innerText = `⏳ Lade ${i + 1}/${sessionFiles.length}...`;

      try {
        let sRes = await fetch('/api/sessionfile?file=' + encodeURIComponent(rawName));
        if (!sRes.ok) continue;
        let sData = await sRes.json();

        let sId = sData.id;
        if (!sId) {
          let parts = rawName.split('/');
          let fn = parts[parts.length - 1];
          sId = fn.replace('.json', '');
        }

        let laps = sData.laps || [];
        let normalizedLaps = laps.map(normalizeLap);
        let carLapsMap = {};
        let carBestMap = {};
        let carTimeSumMap = {};
        let carValidMap = {};
        let carDriverNameMap = {};
        let carCarNameMap = {};
        let maxTs = sData.startTs || 0;

        normalizedLaps.forEach((l, idx) => {
          let origL = laps[idx] || {};
          let c = l.carId;
          if (c >= 1 && c <= MAX_CARS) {
            if (!carLapsMap[c]) {
              carLapsMap[c] = 0;
              carBestMap[c] = 0;
              carTimeSumMap[c] = 0;
              carValidMap[c] = 0;
            }
            if (origL.driver) carDriverNameMap[c] = origL.driver;
            if (origL.car) carCarNameMap[c] = origL.car;

            if (l.lap > carLapsMap[c]) carLapsMap[c] = l.lap;
            else carLapsMap[c]++;

            if (!l.outlap && l.time > 0) {
              if (carBestMap[c] === 0 || l.time < carBestMap[c]) carBestMap[c] = l.time;
              carValidMap[c]++;
              carTimeSumMap[c] += l.time;
            }
            if (l.ts && Math.floor(l.ts / 1000) > maxTs) {
              maxTs = Math.floor(l.ts / 1000);
            }
          }
        });

        let activeCarIds = Object.keys(carLapsMap).map(Number);
        let mode = sData.mode !== undefined ? sData.mode : 0;
        let sortByBest = (mode === 1 || mode === 4);

        activeCarIds.sort((a, b) => {
          if (sortByBest) {
            let bA = carBestMap[a] > 0 ? carBestMap[a] : 99999999;
            let bB = carBestMap[b] > 0 ? carBestMap[b] : 99999999;
            return bA - bB;
          } else {
            if (carLapsMap[b] !== carLapsMap[a]) return carLapsMap[b] - carLapsMap[a];
            let bA = carBestMap[a] > 0 ? carBestMap[a] : 99999999;
            let bB = carBestMap[b] > 0 ? carBestMap[b] : 99999999;
            return bA - bB;
          }
        });

        let results = activeCarIds.map((c, idx) => {
          let avg = (carValidMap[c] > 0) ? Math.round(carTimeSumMap[c] / carValidMap[c]) : 0;
          return {
            pos: idx + 1,
            id: c,
            driver: carDriverNameMap[c] || getCarDriverName(c),
            car: carCarNameMap[c] || getCarCarName(c),
            laps: carLapsMap[c] || 0,
            bestLap: carBestMap[c] || 0,
            avgLap: avg
          };
        });

        let modeNames = ["Freies Training", "Qualifying", "Rennen (Runden)", "Rennen (Zeit)", "Dragrace"];
        builtSessions.push({
          id: sId,
          file: rawName,
          trackId: sData.trackId || "standard",
          trackName: sData.trackName || sData.trackId || "Standard",
          mode: mode,
          modeName: sData.modeName || modeNames[mode] || "Freies Training",
          startTs: sData.startTs || 0,
          endTs: maxTs || sData.startTs || 0,
          results: results
        });

        // 3b. Telemetrie-Optimierung (Alte Session-Dateien auf kompaktes Min-Schema schrumpfen)
        let needsCompression = laps.some(l => l.driver !== undefined || l.car !== undefined || l.sz1 !== undefined || l.time !== undefined || (l.outlap !== undefined && l.o === undefined));
        if (needsCompression && normalizedLaps.length > 0) {
          if (btn) btn.innerText = `⏳ Optimiere ${i + 1}/${sessionFiles.length}...`;
          let minLaps = normalizedLaps.map(nl => {
            let cl = { c: nl.carId, l: nl.lap, t: nl.time, p: nl.pos, s: nl.ts };
            if (nl.sz1 > 0) cl.s1 = nl.sz1;
            if (nl.sz2 > 0) cl.s2 = nl.sz2;
            if (nl.sz3 > 0) cl.s3 = nl.sz3;
            if (nl.sz4 > 0) cl.s4 = nl.sz4;
            if (nl.outlap) cl.o = 1;
            return cl;
          });
          let minSession = {
            id: sData.id || sId,
            trackId: sData.trackId || "standard",
            trackName: sData.trackName || "Standard",
            mode: mode,
            modeName: sData.modeName || modeNames[mode] || "Freies Training",
            startTs: sData.startTs || 0,
            laps: minLaps
          };
          try {
            let compBlob = new Blob([JSON.stringify(minSession)], { type: 'application/json' });
            let compFd = new FormData();
            compFd.append('data', compBlob, rawName);
            let cUpRes = await fetch('/api/upload', { method: 'POST', body: compFd });
            if (cUpRes.ok) compressedCount++;
            await new Promise(r => setTimeout(r, 60)); // ESP8266 Pause
          } catch (compErr) {
            console.warn("Konnte Datei nicht optimieren:", rawName, compErr);
          }
        }
      } catch (errFile) {
        console.warn("Konnte Session-Datei nicht parsen:", rawName, errFile);
      }
    }

    // 4. Sortieren nach Datum absteigend
    builtSessions.sort((a, b) => (b.endTs || b.startTs || 0) - (a.endTs || a.startTs || 0));

    if (btn) btn.innerText = "⏳ Speichere Index...";

    // 5. Index als /sessions.json hochladen (kompakt)
    let idxBlob = new Blob([JSON.stringify({ sessions: builtSessions })], { type: 'application/json' });
    let fd = new FormData();
    fd.append('data', idxBlob, '/sessions.json');
    let upRes = await fetch('/api/upload', { method: 'POST', body: fd });
    if (!upRes.ok) throw new Error("Fehler beim Hochladen von /sessions.json");

    // 5b. Rekorde heilen (Option A): Streckenrekorde & Tagesbestzeiten aus den validierten Sessions neu aufbauen
    if (btn) btn.innerText = "⏳ Synchronisiere Rekorde...";
    try {
      let todayStr = '';
      if (typeof getCurrentLocalDateStr === 'function') {
        todayStr = getCurrentLocalDateStr();
      }
      let nowD = new Date();
      let todayFormatted = `${String(nowD.getFullYear()).slice(-2)}-${String(nowD.getMonth()+1).padStart(2,'0')}-${String(nowD.getDate()).padStart(2,'0')}`;

      // Aktuelle Streckenliste laden
      let tracksDoc = { active: "standard", tracks: [] };
      let tRes = await fetch('/tracks.json');
      if (tRes.ok) {
        tracksDoc = await tRes.json();
      } else if (typeof allTracksList !== 'undefined' && Array.isArray(allTracksList)) {
        tracksDoc = { active: (typeof activeTrackId !== 'undefined' ? activeTrackId : "standard"), tracks: JSON.parse(JSON.stringify(allTracksList)) };
      }

      // Pro Strecke Rekorde aus builtSessions aggregieren
      let trackHealedRecords = {};

      builtSessions.forEach(s => {
        let tId = s.trackId || 'standard';
        if (!trackHealedRecords[tId]) {
          trackHealedRecords[tId] = { allTimeMap: {}, dailyMap: {} };
        }

        let sDateStr = '';
        let isToday = false;
        if (s.startTs) {
          let sD = new Date(s.startTs * 1000);
          isToday = (sD.toDateString() === nowD.toDateString());
          sDateStr = `${String(sD.getFullYear()).slice(-2)}-${String(sD.getMonth()+1).padStart(2,'0')}-${String(sD.getDate()).padStart(2,'0')}`;
        }
        if (!isToday && todayStr && sDateStr === todayStr) isToday = true;

        if (s.mode !== 4 && Array.isArray(s.results)) {
          s.results.forEach(r => {
            if (!r.bestLap || r.bestLap <= 0) return;
            let dName = (r.driver || '').trim() || getCarDriverName(r.id);
            let cName = (r.car || '').trim() || getCarCarName(r.id);
            let comboKey = `${dName.toLowerCase()}_${cName.toLowerCase()}`;

            let entry = {
              lap: r.bestLap,
              carId: r.id || 0,
              driver: dName,
              car: cName,
              date: sDateStr
            };

            let atMap = trackHealedRecords[tId].allTimeMap;
            if (!atMap[comboKey] || r.bestLap < atMap[comboKey].lap) {
              atMap[comboKey] = entry;
            }

            if (isToday) {
              let dMap = trackHealedRecords[tId].dailyMap;
              if (!dMap[comboKey] || r.bestLap < dMap[comboKey].lap) {
                dMap[comboKey] = entry;
              }
            }
          });
        }
      });

      if (Array.isArray(tracksDoc.tracks)) {
        tracksDoc.tracks.forEach(t => {
          let hData = trackHealedRecords[t.id];
          let healedAllTime = [];
          let healedDaily = [];

          if (hData) {
            healedAllTime = Object.values(hData.allTimeMap).sort((a, b) => a.lap - b.lap).slice(0, 5);
            healedDaily = Object.values(hData.dailyMap).sort((a, b) => a.lap - b.lap).slice(0, 5);
          }

          t.allTimeRecords = healedAllTime;
          t.dailyRecords = healedDaily;

          if (healedAllTime.length > 0) {
            t.recordLap = healedAllTime[0].lap;
            t.recordCarId = healedAllTime[0].carId;
            t.recordDriver = healedAllTime[0].driver;
            t.recordCar = healedAllTime[0].car;
            t.recordDate = healedAllTime[0].date;
          } else {
            t.recordLap = 0;
            t.recordCarId = 0;
            t.recordDriver = "";
            t.recordCar = "";
            t.recordDate = "";
          }

          if (healedDaily.length > 0) {
            t.bestLap = healedDaily[0].lap;
            t.bestCarId = healedDaily[0].carId;
            t.bestDriver = healedDaily[0].driver;
            t.bestCar = healedDaily[0].car;
            t.bestDate = healedDaily[0].date;
          } else {
            t.bestLap = 0;
            t.bestCarId = 0;
            t.bestDriver = "";
            t.bestCar = "";
            t.bestDate = todayFormatted;
          }
        });

        let tBlob = new Blob([JSON.stringify(tracksDoc)], { type: 'application/json' });
        let tFd = new FormData();
        tFd.append('data', tBlob, '/tracks.json');
        let tUpRes = await fetch('/api/upload', { method: 'POST', body: tFd });
        if (tUpRes.ok && typeof loadTracks === 'function') {
          await loadTracks();
        }
      }
    } catch (recHealErr) {
      console.warn("Fehler beim Heilen der Streckenrekorde:", recHealErr);
    }

    await showAlert({
      title: "✅ SYSTEM VOLLSTÄNDIG GEHEILT",
      theme: "success",
      message: `Es wurden <strong>${builtSessions.length} Durchgänge</strong> erfolgreich indiziert.<br><span style="color:var(--neon-gold); font-size:12px; display:block; margin-top:8px;">🏆 <strong>Streckenrekorde &amp; Tagesbestzeiten</strong> wurden auf Basis aller echten Renn-Sessions vollständig bereinigt und synchronisiert!</span>${compressedCount > 0 ? `<br><span style="color:var(--neon-green); font-size:12px; display:block; margin-top:6px;">✨ <strong>${compressedCount} alte Session-Dateien</strong> auf das kompakte Telemetrie-Format geschrumpft.</span>` : ''}`
    });

    sessionFileCache = {};
    openSessionsMap = {};
    openDriverDetailsMap = {};
    await fetchSessionsAndStorageInfo();
    switchStatsTab(currentTab);

  } catch (err) {
    await showAlert({
      title: "❌ FEHLER BEIM REBUILD",
      theme: "danger",
      message: "Der Index konnte nicht neu aufgebaut werden: " + err.message
    });
  } finally {
    if (btn) {
      btn.disabled = false;
      btn.innerHTML = origText;
    }
  }
}
