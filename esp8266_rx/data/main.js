const MAX_CARS = 15;
let ttsEnabled = true;
let countdownTimeouts = [];
window.UI_VERSION = '3.7.5'; 
let sectorAudioEnabled = JSON.parse(localStorage.getItem('sectorAudioEnabled') ?? 'true');
let socket = null;

let userRole = localStorage.getItem('rctl_user_role') || null; 

let currentMode = 0;
let targetLaps = 0;
let maxLapInRace = 0;
let isSessionActive = false;
let isSessionFinished = false;
let qualiFinishedHold = false;
let previousLaps = {};
let previousBestLaps = {};
let lastLapDeltaMap = {};
let previousBestSzMap = {};
let lastAnnouncedLapMap = {}; 
let carNamesMap = {};
let audioUnmuted = false;
let lastLapsData = [];
let awaitingBaseCrossMap = {};
let sectorFlashUntilMap = {};

let activeUseSz1 = true;
let activeUseSz2 = true;
let activeUseSz3 = true;

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

async function forceCleanReload() {
  let btn = document.getElementById('hardReloadBtn');
  if (btn) {
    btn.innerText = "⏳ Leere Cache...";
    btn.disabled = true;
  }

  const assets = [
    '/',
    '/index.html',
    '/style.css',
    '/tts.js',
    '/websocket.js',
    '/modes.js',
    '/stats.js',
    '/config.js',
    '/admin.html',
    '/main.js'
  ];

  for (let i = 0; i < assets.length; i++) {
    try {
      await fetch(assets[i], {
        method: 'GET',
        cache: 'reload',
        headers: {
          'Pragma': 'no-cache',
          'Cache-Control': 'no-cache, no-store, must-revalidate'
        }
      });
      await new Promise(r => setTimeout(r, 50));
    } catch (e) {}
  }

  if ('caches' in window) {
    try {
      const keys = await caches.keys();
      await Promise.all(keys.map(k => caches.delete(k)));
    } catch (e) {}
  }

  window.location.replace(window.location.href.split('?')[0] + '?v=' + Date.now());
}

// --- ADMIN CONFIRMATION MODAL ---
function openAdminConfirmModal() {
  let cb = document.getElementById('adminConfirmCheckbox');
  let btn = document.getElementById('adminGoBtn');
  if (cb) cb.checked = false;
  if (btn) {
    btn.style.opacity = '0.5';
    btn.style.pointerEvents = 'none';
  }
  let modal = document.getElementById('adminConfirmModal');
  if (modal) modal.style.display = 'block';
}

function closeAdminConfirmModal() {
  let modal = document.getElementById('adminConfirmModal');
  if (modal) modal.style.display = 'none';
}

function toggleAdminConfirmBtn() {
  let cb = document.getElementById('adminConfirmCheckbox');
  let btn = document.getElementById('adminGoBtn');
  if (cb && btn) {
    if (cb.checked) {
      btn.style.opacity = '1';
      btn.style.pointerEvents = 'auto';
    } else {
      btn.style.opacity = '0.5';
      btn.style.pointerEvents = 'none';
    }
  }
}

function proceedToAdmin() {
  window.location.href = '/admin.html';
}

// --- TABLE SORTING & LIVE TIMING HELPERS ---
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

// --- LAZY-LOADING DER STATISTIKEN ---
let statsLoading = false;
async function openStatsOverviewModal() {
  if (typeof switchStatsTab !== 'function') {
    if (statsLoading) return;
    statsLoading = true;
    let btn = document.getElementById('statsOverviewBtn');
    let orig = btn ? btn.innerText : '';
    if (btn) btn.innerText = "⏳ Lade...";
    let s = document.createElement('script');
    s.src = 'stats.js?v=' + (window.UI_VERSION || '3.7.0');
    s.onload = async () => {
      statsLoading = false;
      if (btn) btn.innerText = orig;
      if (typeof openStatsOverviewModal === 'function') {
        openStatsOverviewModal();
      }
    };
    s.onerror = () => {
      statsLoading = false;
      if (btn) btn.innerText = orig;
      alert("Fehler beim Laden der Statistiken.");
    };
    document.body.appendChild(s);
  } else {
    let m = document.getElementById('statsOverviewModal');
    if (m) m.style.display = 'block';
    await loadCarNames();
    switchStatsTab(currentTab);
  }
}

function closeStatsOverviewModal() {
  let m = document.getElementById('statsOverviewModal');
  if (m) m.style.display = 'none';
}

function closeStatsModal() {
  let m = document.getElementById('statsModal');
  if (m) m.style.display = 'none';
}

// --- DASHBOARD / OLED-DIMMER ---
function activateOledDimmer() {
  if (!audioUnmuted) unmuteIOSAudio();
  let overlay = document.getElementById('oledDimOverlay');
  if (overlay) overlay.style.display = 'flex';
  updateData(true);
}

function deactivateOledDimmer() {
  let overlay = document.getElementById('oledDimOverlay');
  if (overlay) overlay.style.display = 'none';
}

// --- FAHRER-EINSTELLUNGEN MODAL ---
function openDriverConfigModal() {
  let modal = document.getElementById('driverConfigModal');
  if (modal) modal.style.display = 'block';

  let szAudioCb = document.getElementById('driver_cfg_sector_audio');
  if (szAudioCb) szAudioCb.checked = sectorAudioEnabled;

  let lapTimeCb = document.getElementById('driver_cfg_announce_lap_times');
  if (lapTimeCb) lapTimeCb.checked = announceLapTimesInRace;
}

function closeDriverConfigModal() {
  let modal = document.getElementById('driverConfigModal');
  if (modal) modal.style.display = 'none';
}

function saveDriverAudioConfig() {
  let szAudioCb = document.getElementById('driver_cfg_sector_audio');
  if (szAudioCb) {
    sectorAudioEnabled = szAudioCb.checked;
    localStorage.setItem('sectorAudioEnabled', JSON.stringify(sectorAudioEnabled));
  }

  let lapTimeCb = document.getElementById('driver_cfg_announce_lap_times');
  if (lapTimeCb) {
    announceLapTimesInRace = lapTimeCb.checked;
    localStorage.setItem('announceLapTimesInRace', JSON.stringify(announceLapTimesInRace));
  }

  fetch('/api/setsectorconfig?audio=' + (sectorAudioEnabled ? "1" : "0"), { method: 'POST' }).catch(() => {});
}

// Aktualisiert eine einzelne Fahrer-Spalte im Dashboard (colIdx = 1 oder 2)
function updateDashboardDriverCol(colIdx, carId, rowData, sessionOverallBestLap, minSz1, minSz2, minSz3, minSz4) {
  let dCar = document.getElementById(`dimmerCar${colIdx}Text`);
  let dPos = document.getElementById(`dimmerPos${colIdx}Text`);
  let dStatus = document.getElementById(`dimmerStatus${colIdx}Badge`);
  let dPb = document.getElementById(`dimmerPb${colIdx}Text`);
  let dLast = document.getElementById(`dimmerLast${colIdx}Text`);
  let dDelta = document.getElementById(`dimmerDelta${colIdx}Text`);

  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);
  let carName = carNamesMap[carId]?.car || ("Auto " + carId);
  if (dCar) dCar.innerText = `${driverName} (${carName})`;

  let mask = getVisibleSectorsMask();

  if (!rowData) {
    if (dPos) dPos.innerText = "P-";
    if (dStatus) dStatus.style.display = 'none';
    if (dPb) dPb.innerText = "--:--";
    if (dLast) {
      dLast.innerText = "--:--";
      dLast.style.color = "var(--text-main)";
    }
    if (dDelta) dDelta.innerText = "";
    for (let i = 1; i <= 4; i++) {
      let box = document.getElementById(`dimmer${colIdx}-box-${i}`);
      let valElem = document.getElementById(`dimmer${colIdx}-sz${i}-val`);
      let deltaElem = document.getElementById(`dimmer${colIdx}-sz${i}-delta`);
      let show = (i === 1 && mask.showSz1) || (i === 2 && mask.showSz2) || (i === 3 && mask.showSz3) || (i === 4 && mask.showSz4);
      if (box) box.style.display = show ? 'block' : 'none';
      if (valElem) valElem.innerText = '---';
      if (deltaElem) deltaElem.innerText = '';
    }
    return;
  }

  let pos = rowData._defaultPos || "-";
  if (dPos) dPos.innerText = `P${pos}`;

  let carState = rowData.state || statusCarsMap[carId] || null;
  if (awaitingBaseCrossMap[carId]) carState = 'outlap';

  if (dStatus) {
    if (carState === 'box' || carState === 'pause' || carState === 'outlap') {
      dStatus.innerText = carState.toUpperCase();
      dStatus.className = `dimmer-status-pill status-${carState}`;
      dStatus.style.display = 'inline-block';
    } else {
      dStatus.style.display = 'none';
    }
  }

  if (dPb) dPb.innerText = rowData.bestLap > 0 ? (rowData.bestLap / 1000).toFixed(2) + 's' : '--:--';

  if (dLast) {
    dLast.innerText = rowData.lastLap > 0 ? (rowData.lastLap / 1000).toFixed(2) + 's' : '--:--';
    let isOverall = (sessionOverallBestLap > 0 && rowData.lastLap <= sessionOverallBestLap);
    let isPb = (rowData.bestLap > 0 && rowData.lastLap <= rowData.bestLap);
    if (isOverall) dLast.style.color = 'var(--neon-purple)';
    else if (isPb) dLast.style.color = 'var(--neon-green)';
    else dLast.style.color = 'var(--text-main)';
  }

  if (dDelta) {
    let deltaInfo = lastLapDeltaMap[carId];
    if (deltaInfo && deltaInfo.diffMs !== null && rowData.lastLap > 0) {
      let diffSec = (Math.abs(deltaInfo.diffMs) / 1000).toFixed(2).replace('.', ',');
      if (deltaInfo.diffMs < 0) {
        let tag = deltaInfo.isAbsolute ? " (SCHNELLSTE RUNDE)" : " (PERSÖNLICHE BESTZEIT)";
        dDelta.innerText = `-${diffSec}s${tag}`;
        dDelta.style.color = deltaInfo.isAbsolute ? 'var(--neon-purple)' : 'var(--neon-green)';
      } else if (deltaInfo.diffMs > 0) {
        dDelta.innerText = `+${diffSec}s`;
        dDelta.style.color = 'var(--neon-red)';
      } else {
        dDelta.innerText = `±0,00s`;
        dDelta.style.color = 'var(--text-muted)';
      }
    } else if (deltaInfo && deltaInfo.diffMs === null && rowData.lastLap > 0) {
      dDelta.innerText = deltaInfo.isAbsolute ? "SCHNELLSTE RUNDE" : "PERSÖNLICHE BESTZEIT";
      dDelta.style.color = deltaInfo.isAbsolute ? 'var(--neon-purple)' : 'var(--neon-green)';
    } else if (rowData.lastLap > 0 && rowData.bestLap > 0) {
      let diff = rowData.lastLap - rowData.bestLap;
      if (diff > 0) {
        dDelta.innerText = `+${(diff / 1000).toFixed(2).replace('.', ',')}s`;
        dDelta.style.color = 'var(--neon-red)';
      } else if (sessionOverallBestLap > 0 && rowData.lastLap <= sessionOverallBestLap) {
        dDelta.innerText = "SCHNELLSTE RUNDE";
        dDelta.style.color = 'var(--neon-purple)';
      } else {
        dDelta.innerText = "PERSÖNLICHE BESTZEIT";
        dDelta.style.color = 'var(--neon-green)';
      }
    } else {
      dDelta.innerText = '';
    }
  }

  for (let i = 1; i <= 4; i++) {
    let box = document.getElementById(`dimmer${colIdx}-box-${i}`);
    let valElem = document.getElementById(`dimmer${colIdx}-sz${i}-val`);
    let deltaElem = document.getElementById(`dimmer${colIdx}-sz${i}-delta`);
    let show = (i === 1 && mask.showSz1) || (i === 2 && mask.showSz2) || (i === 3 && mask.showSz3) || (i === 4 && mask.showSz4);
    if (box) box.style.display = show ? 'block' : 'none';

    if (show && valElem) {
      let szVal = rowData[`sz${i}`] || 0;
      let splitVal = rowData[`split${i}`] || 0;
      let isDrag = (currentMode === 4);

      if (szVal > 0) {
        let minSz = (i === 1) ? minSz1 : ((i === 2) ? minSz2 : ((i === 3) ? minSz3 : minSz4));
        let bestSz = rowData[`bestSz${i}`] || 0;
        let isOverallMin = (minSz > 0 && szVal <= minSz);
        let isPbSz = (bestSz > 0 && szVal <= bestSz);

        if (isOverallMin) {
          valElem.style.color = 'var(--neon-purple)';
          valElem.style.textShadow = '0 0 8px rgba(181,50,255,0.7)';
        } else if (isPbSz) {
          valElem.style.color = 'var(--neon-green)';
          valElem.style.textShadow = 'none';
        } else {
          valElem.style.color = 'var(--neon-orange)';
          valElem.style.textShadow = 'none';
        }

        if (isDrag) {
          let splStr = splitVal > 0 ? (splitVal / 1000).toFixed(2) + 's' : '---';
          let szStr = (szVal / 1000).toFixed(2) + 's';
          valElem.innerHTML = `<div class="drag-cell"><span class="drag-split" style="font-size:11px;color:var(--text-muted);display:block;margin-bottom:3px;">Spl: ${splStr}</span><span>${szStr}</span></div>`;
        } else {
          valElem.innerText = (szVal / 1000).toFixed(2) + 's';
        }

        if (deltaElem && !isDrag) {
          let szKey = `${carId}_${i}`;
          let prevSzBest = previousBestSzMap[szKey];
          if (prevSzBest && szVal < prevSzBest) {
            let szDiff = szVal - prevSzBest;
            let szDiffSec = (Math.abs(szDiff) / 1000).toFixed(2).replace('.', ',');
            deltaElem.innerText = `-${szDiffSec}s`;
            deltaElem.className = isOverallMin ? 'dimmer-sz-delta delta-purple' : 'dimmer-sz-delta delta-green';
            previousBestSzMap[szKey] = szVal;
          } else if (isPbSz) {
            deltaElem.innerText = isOverallMin ? 'BEST' : 'PB';
            deltaElem.className = isOverallMin ? 'dimmer-sz-delta delta-purple' : 'dimmer-sz-delta delta-green';
            previousBestSzMap[szKey] = szVal;
          } else if (bestSz > 0) {
            let szDiff = szVal - bestSz;
            let szDiffSec = (szDiff / 1000).toFixed(2).replace('.', ',');
            deltaElem.innerText = `+${szDiffSec}s`;
            deltaElem.className = 'dimmer-sz-delta delta-orange';
          } else {
            deltaElem.innerText = '';
          }
        } else if (deltaElem) {
          deltaElem.innerText = '';
        }
      } else {
        valElem.innerText = '---';
        valElem.style.color = 'var(--text-main)';
        valElem.style.textShadow = 'none';
        if (deltaElem) deltaElem.innerText = '';
      }
    }
  }
}

// Aktualisiert die Rennleiter Großbild-Rennübersicht (Leaderboard für alle erfassten Fahrer)
function updateDimmerLeaderboard(displayData, minBestLap, minSz1, minSz2, minSz3, minSz4, leaderLaps, targetLaps) {
  let tbody = document.getElementById('dimmerLeaderboardBody');
  if (!tbody) return;

  let mask = getVisibleSectorsMask();
  let thSz1 = document.getElementById('dimmer-th-sz1');
  let thSz2 = document.getElementById('dimmer-th-sz2');
  let thSz3 = document.getElementById('dimmer-th-sz3');
  let thSz4 = document.getElementById('dimmer-th-sz4');
  let thLaps = document.getElementById('dimmer-th-laps');
  let thLast = document.getElementById('dimmer-th-last');

  let isDrag = (currentMode === 4);
  if (thLaps) thLaps.innerText = isDrag ? "Läufe" : "Runde";
  if (thLast) thLast.innerText = isDrag ? "Laufzeit" : "Letzte Zeit";

  if (thSz1) thSz1.style.display = mask.showSz1 ? '' : 'none';
  if (thSz2) thSz2.style.display = mask.showSz2 ? '' : 'none';
  if (thSz3) thSz3.style.display = mask.showSz3 ? '' : 'none';
  if (thSz4) thSz4.style.display = mask.showSz4 ? '' : 'none';

  let visibleSectorCols = (mask.showSz1 ? 1 : 0) + (mask.showSz2 ? 1 : 0) + (mask.showSz3 ? 1 : 0) + (mask.showSz4 ? 1 : 0);
  let totalCols = 7 + visibleSectorCols;

  // Nur tatsächlich erfasste Fahrzeuge anzeigen
  let activeCars = (displayData || []).filter(r => 
    r.completedLaps > 0 || r.lastLap > 0 || r.bestLap > 0 || 
    r.sz1 > 0 || r.sz2 > 0 || r.sz3 > 0 || r.sz4 > 0 || 
    r.state === 'drag_running' || r.state === 'outlap' || r.state === 'box' || r.state === 'pause'
  );

  if (activeCars.length === 0) {
    tbody.innerHTML = `<tr><td colspan="${totalCols}" class="dimmer-table-empty">🏁 Keine aktiven Fahrzeuge auf der Strecke</td></tr>`;
    return;
  }

  let html = '';
  activeCars.forEach((row, idx) => {
    let pos = row._defaultPos || (idx + 1);
    let posClass = 'dimmer-pos-cell';
    if (pos === 1) posClass += ' dimmer-pos-p1';
    else if (pos === 2) posClass += ' dimmer-pos-p2';
    else if (pos === 3) posClass += ' dimmer-pos-p3';

    // Status Badge
    let carState = row.state || statusCarsMap[row.id] || null;
    if (awaitingBaseCrossMap[row.id]) carState = 'outlap';

    let statusHtml = '';
    if (carState === 'box') {
      statusHtml = '<span class="dimmer-status-pill status-box">BOX</span>';
    } else if (carState === 'pause') {
      statusHtml = '<span class="dimmer-status-pill status-pause">PAUSE</span>';
    } else if (carState === 'outlap') {
      statusHtml = '<span class="dimmer-status-pill status-outlap">OUTLAP</span>';
    } else if (carState === 'drag_running') {
      statusHtml = '<span class="dimmer-status-pill" style="color:var(--neon-green); border-color:var(--neon-green); background:rgba(0,255,102,0.15);">START</span>';
    } else if (row.completedLaps > 0 || row.lastLap > 0) {
      statusHtml = '<span class="dimmer-status-pill" style="color:var(--neon-green); border-color:var(--neon-green); background:rgba(0,255,102,0.1);">STRECKE</span>';
    } else {
      statusHtml = '<span class="dimmer-status-pill" style="color:var(--text-muted); border-color:var(--card-border);">BEREIT</span>';
    }

    // Runden Display
    let lapsDisp = row.completedLaps;
    if (isDrag) {
      lapsDisp = row.completedLaps > 0 ? (row.completedLaps + (row.completedLaps === 1 ? ' Lauf' : ' Läufe')) : '0';
    } else if (targetLaps > 0) {
      lapsDisp = `${row.completedLaps} / ${targetLaps}`;
    }

    // Sektor Zellen
    let renderSzCell = (secNum, val, bestVal, minVal, isVisible) => {
      if (!isVisible) return `<td style="display:none;"></td>`;
      if (val <= 0) return `<td><span style="color:var(--text-muted);">---</span></td>`;

      let isOverall = (minVal > 0 && val <= minVal);
      let isPb = (bestVal > 0 && val <= bestVal);
      let colorStyle = 'color:var(--neon-orange);';
      let shadowStyle = '';
      if (isOverall) {
        colorStyle = 'color:var(--neon-purple);';
        shadowStyle = 'text-shadow:0 0 6px rgba(181,50,255,0.7);';
      } else if (isPb) {
        colorStyle = 'color:var(--neon-green);';
      }

      let secStr = (val / 1000).toFixed(2) + 's';
      return `<td><span style="font-weight:700; ${colorStyle} ${shadowStyle}">${secStr}</span></td>`;
    };

    let sz1Cell = renderSzCell(1, row.sz1, row.bestSz1, minSz1, mask.showSz1);
    let sz2Cell = renderSzCell(2, row.sz2, row.bestSz2, minSz2, mask.showSz2);
    let sz3Cell = renderSzCell(3, row.sz3, row.bestSz3, minSz3, mask.showSz3);
    let sz4Cell = renderSzCell(4, row.sz4, row.bestSz4, minSz4, mask.showSz4);

    // Letzte Runde & Delta
    let lastDisp = '---';
    let lastColor = 'color:var(--text-main);';
    if (row.lastLap > 0) {
      lastDisp = (row.lastLap / 1000).toFixed(2) + 's';
      let isOverallLast = (minBestLap > 0 && row.lastLap <= minBestLap);
      let isPbLast = (row.bestLap > 0 && row.lastLap <= row.bestLap);
      if (isOverallLast) lastColor = 'color:var(--neon-purple);';
      else if (isPbLast) lastColor = 'color:var(--neon-green);';
    }

    let deltaHtml = '';
    let deltaInfo = lastLapDeltaMap[row.id];
    if (deltaInfo && deltaInfo.diffMs !== null && row.lastLap > 0) {
      let diffSec = (Math.abs(deltaInfo.diffMs) / 1000).toFixed(2).replace('.', ',');
      if (deltaInfo.diffMs < 0) {
        let dCol = deltaInfo.isAbsolute ? 'var(--neon-purple)' : 'var(--neon-green)';
        deltaHtml = `<div style="font-size:11px; font-weight:700; color:${dCol};">-${diffSec}s</div>`;
      } else if (deltaInfo.diffMs > 0) {
        deltaHtml = `<div style="font-size:11px; font-weight:700; color:var(--neon-red);">+${diffSec}s</div>`;
      } else {
        deltaHtml = `<div style="font-size:11px; font-weight:700; color:var(--text-muted);">±0,00s</div>`;
      }
    } else if (row.lastLap > 0 && row.bestLap > 0 && row.lastLap > row.bestLap) {
      let diff = row.lastLap - row.bestLap;
      deltaHtml = `<div style="font-size:11px; font-weight:700; color:var(--neon-red);">+${(diff / 1000).toFixed(2).replace('.', ',')}s</div>`;
    }

    // Beste Runde
    let bestDisp = '---';
    let bestColor = 'color:var(--text-muted);';
    if (row.bestLap > 0) {
      bestDisp = (row.bestLap / 1000).toFixed(2) + 's';
      if (minBestLap > 0 && row.bestLap <= minBestLap) {
        bestColor = 'color:var(--neon-purple); text-shadow:0 0 8px rgba(181,50,255,0.7); font-weight:900;';
      } else {
        bestColor = 'color:var(--neon-green); font-weight:700;';
      }
    }

    // Abstand (Gap)
    let gapHtml = '---';
    if (pos === 1) {
      gapHtml = (currentMode >= 2) ? '<span style="color:var(--neon-cyan); font-weight:700;">LEADER</span>' : '<span style="color:var(--neon-purple); font-weight:700;">BEST</span>';
    } else if (currentMode >= 2 && currentMode !== 4) {
      let leaderLapCount = activeCars[0]?.completedLaps || 0;
      let diffLaps = leaderLapCount - row.completedLaps;
      if (diffLaps > 0) {
        gapHtml = `<span style="color:var(--neon-orange); font-weight:700;">+${diffLaps} ${diffLaps === 1 ? 'Rnd' : 'Rnd'}</span>`;
      } else if (minBestLap > 0 && row.bestLap > 0) {
        let diffMs = row.bestLap - minBestLap;
        gapHtml = `<span style="color:var(--text-muted);">+${(diffMs / 1000).toFixed(2).replace('.', ',')}s</span>`;
      }
    } else if (minBestLap > 0 && row.bestLap > 0) {
      let diffMs = row.bestLap - minBestLap;
      gapHtml = `<span style="color:var(--text-muted);">+${(diffMs / 1000).toFixed(2).replace('.', ',')}s</span>`;
    }

    html += `<tr>
      <td class="${posClass}">P${pos}</td>
      <td style="text-align:left; padding-left:14px;">
        <div class="dimmer-table-driver-box">
          <span class="dimmer-table-driver-name">${row.driver}</span>
          <div class="dimmer-table-driver-car">${row.car} <span style="opacity:0.6;">(ID ${row.id})</span></div>
        </div>
      </td>
      <td>${statusHtml}</td>
      <td style="font-weight:700;">${lapsDisp}</td>
      ${sz1Cell}
      ${sz2Cell}
      ${sz3Cell}
      ${sz4Cell}
      <td>
        <div style="font-weight:700; ${lastColor}">${lastDisp}</div>
        ${deltaHtml}
      </td>
      <td><span style="${bestColor}">${bestDisp}</span></td>
      <td>${gapHtml}</td>
    </tr>`;
  });

  tbody.innerHTML = html;
}

function applyUserRoleUI(role) {
  userRole = role;
  document.body.classList.remove('role-admin', 'role-driver');
  document.body.classList.add(role === 'driver' ? 'role-driver' : 'role-admin');

  let badge = document.getElementById('currentRoleBadge');
  if (badge) {
    if (role === 'driver') {
      badge.innerText = "🏎️ Aktueller Modus: Fahrer. Zu Rennleiter wechseln 👑";
      badge.onclick = (typeof openAdminConfirmModal === 'function') ? openAdminConfirmModal : () => { location.href = '/admin'; };
      badge.style.borderColor = "var(--neon-green)";
      badge.style.color = "var(--neon-green)";
    } else {
      badge.innerText = "👑 Aktueller Modus: Rennleiter. Zu Fahrer wechseln 🏎️";
      badge.onclick = () => { location.href = '/'; };
      badge.style.borderColor = "var(--neon-cyan)";
      badge.style.color = "var(--neon-cyan)";
    }
  }
  populateDriverQuickSelect();
}

function openRoleModal() {
  let modal = document.getElementById('roleSelectModal');
  if (modal) modal.style.display = 'block';
}

function selectUserRole(role) {
  localStorage.setItem('rctl_user_role', role);
  applyUserRoleUI(role);
  let modal = document.getElementById('roleSelectModal');
  if (modal) modal.style.display = 'none';

  if (!audioUnmuted) unmuteIOSAudio();
  updateData(true);
}

function populateDriverQuickSelect() {
  let sel1 = document.getElementById('driverQuickSelect1');
  let sel2 = document.getElementById('driverQuickSelect2');
  if (sel1) {
    let val1 = (selectedTTSCars.length >= 1) ? String(selectedTTSCars[0]) : 'none';
    let val2 = (selectedTTSCars.length >= 2) ? String(selectedTTSCars[1]) : 'none';

    let getOptionsHtml = (selectedVal, placeholderText) => {
      let html = `<option value="none" ${selectedVal === 'none' ? 'selected' : ''}>${placeholderText}</option>`;
      for (let i = 1; i <= MAX_CARS; i++) {
        let dName = carNamesMap[i]?.driver || ('Fahrer ' + i);
        let cName = carNamesMap[i]?.car || ('Auto ' + i);
        let isSel = (String(i) === selectedVal) ? 'selected' : '';
        html += `<option value="${i}" ${isSel}>ID ${i}: ${dName} (${cName})</option>`;
      }
      return html;
    };

    sel1.innerHTML = getOptionsHtml(val1, '🎧 Fahrer 1 wählen...');
    if (sel2) sel2.innerHTML = getOptionsHtml(val2, '➕ Fahrer 2 (optional)...');
  }

  let selLegacy = document.getElementById('driverQuickSelect');
  if (selLegacy) {
    let currentVal = (selectedTTSCars.length === 1) ? String(selectedTTSCars[0]) : 'all';
    let html = `<option value="all">🔊 Alle Fahrzeuge ansagen</option>`;
    for (let i = 1; i <= MAX_CARS; i++) {
      let dName = carNamesMap[i]?.driver || ('Fahrer ' + i);
      let cName = carNamesMap[i]?.car || ('Auto ' + i);
      let selectedAttr = (String(i) === currentVal) ? 'selected' : '';
      html += `<option value="${i}" ${selectedAttr}>🎧 ID ${i}: ${dName} (${cName})</option>`;
    }
    selLegacy.innerHTML = html;
  }
}

function onDriverQuickSelectChange(val) {
  let sel1 = document.getElementById('driverQuickSelect1');
  let sel2 = document.getElementById('driverQuickSelect2');

  if (sel1) {
    let v1 = sel1.value;
    let v2 = sel2 ? sel2.value : 'none';
    let newSelected = [];
    if (v1 !== 'none') newSelected.push(Number(v1));
    if (v2 !== 'none' && v2 !== v1) newSelected.push(Number(v2));
    selectedTTSCars = newSelected;
  } else if (val !== undefined) {
    if (val === 'all') {
      selectedTTSCars = [];
    } else {
      selectedTTSCars = [Number(val)];
    }
  }

  localStorage.setItem('myTTSCars', JSON.stringify(selectedTTSCars));
  updateTTSFilterDisplay();
  updateData(true);

  if (selectedTTSCars.length === 2) {
    let n1 = carNamesMap[selectedTTSCars[0]]?.driver || ('Fahrer ' + selectedTTSCars[0]);
    let n2 = carNamesMap[selectedTTSCars[1]]?.driver || ('Fahrer ' + selectedTTSCars[1]);
    speak(`Sprachausgabe aktiv für ${n1} und ${n2}`, true);
  } else if (selectedTTSCars.length === 1) {
    let n1 = carNamesMap[selectedTTSCars[0]]?.driver || ('Fahrer ' + selectedTTSCars[0]);
    let c1 = carNamesMap[selectedTTSCars[0]]?.car || ('Auto ' + selectedTTSCars[0]);
    speak(`Sprachausgabe aktiv für ${n1} auf ${c1}`, true);
  } else {
    speak("Sprachausgabe für alle Fahrzeuge aktiv", true);
  }
}

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
    populateDriverQuickSelect();
  } catch (e) {
    console.error("Fehler beim Laden der Namen:", e);
  }
}

async function loadSectorConfigInitial() {
  try {
    let res = await fetch('/api/sectorconfig');
    if (res.ok) {
      let data = await res.json();
      activeUseSz1 = !!data.useSz1;
      activeUseSz2 = !!data.useSz2;
      activeUseSz3 = !!data.useSz3;
      if (data.sectorAudio !== undefined) {
        sectorAudioEnabled = data.sectorAudio;
      }
      applySectorColumnVisibility();
    }
  } catch (e) {
    console.warn("Sektor-Konfiguration konnte nicht geladen werden:", e);
  }
}

function getVisibleSectorsMask() {
  let isDrag = (currentMode === 4);
  if (isDrag) {
    return {
      showSz1: activeUseSz1,
      showSz2: activeUseSz2,
      showSz3: activeUseSz3,
      showSz4: false
    };
  }
  let finalSectorNum = activeUseSz3 ? 4 : (activeUseSz2 ? 3 : (activeUseSz1 ? 2 : 0));
  return {
    showSz1: activeUseSz1,
    showSz2: activeUseSz2 || finalSectorNum === 2,
    showSz3: activeUseSz3 || finalSectorNum === 3,
    showSz4: finalSectorNum === 4
  };
}

function applySectorColumnVisibility() {
  let mask = getVisibleSectorsMask();
  let th1 = document.getElementById('th-sz1');
  let th2 = document.getElementById('th-sz2');
  let th3 = document.getElementById('th-sz3');
  let th4 = document.getElementById('th-sz4');

  if (th1) th1.style.display = mask.showSz1 ? '' : 'none';
  if (th2) th2.style.display = mask.showSz2 ? '' : 'none';
  if (th3) th3.style.display = mask.showSz3 ? '' : 'none';
  if (th4) th4.style.display = mask.showSz4 ? '' : 'none';
}

async function loadWifiStatus() {
  try {
    let res = await fetch('/api/wificonfig');
    if(res.ok) {
      let data = await res.json();
      let configured = data.mode;
      let actual = data.runtimeMode !== undefined ? data.runtimeMode : data.mode;
      let isFallback = (configured !== actual);

      let txt = "Unbekannt";
      if (actual === 1) txt = "🏠 Heim-WLAN";
      else if (actual === 2) txt = "📱 Hotspot";
      else if (actual === 0) txt = "📡 AP-Modus";
      
      if (isFallback) {
         let source = configured === 1 ? "Heim-WLAN" : (configured === 2 ? "Hotspot" : "?");
         txt += ` <span style="color:var(--neon-red); font-size:11px;">(Fallback von ${source})</span>`;
      }

      let ft = document.getElementById('footer-wifi-status');
      if (ft) ft.innerHTML = `WLAN: <strong style="color:var(--neon-cyan);">${txt}</strong>`;
    }
  } catch (e) {
    console.warn("WLAN Status konnte nicht für den Footer geladen werden");
  }
}

function updateTrackRecordBar(info) {
  let bar = document.getElementById('trackRecordBar');
  let disp = document.getElementById('trackRecordDisplay');
  let dimBanner = document.getElementById('dimmerTrackRecordBanner');
  let dimVal = document.getElementById('dimmerTrackRecordVal');

  let hasDailyBest = (info && info.dailyBestLap && info.dailyBestLap > 0);

  if (dimBanner) {
    dimBanner.style.display = (currentMode === 4) ? 'none' : 'inline-flex';
  }
  if (dimVal) {
    if (hasDailyBest) {
      let timeStr = (info.dailyBestLap / 1000).toFixed(2) + 's';
      let driver = info.dailyBestDriver || (carNamesMap[info.dailyBestCarId]?.driver) || ('Fahrer ' + info.dailyBestCarId);
      dimVal.innerHTML = `${driver} — <span style="color:var(--neon-gold); font-size:13px;">${timeStr}</span>`;
    } else {
      dimVal.innerText = '--:--';
    }
  }

  if (!bar || !disp) return;

  if (currentMode === 4) {
    bar.style.display = 'none';
    return;
  }
  bar.style.display = 'flex';

  if (hasDailyBest) {
    let timeStr = (info.dailyBestLap / 1000).toFixed(2) + 's';
    let driver = info.dailyBestDriver || (carNamesMap[info.dailyBestCarId]?.driver) || ('Fahrer ' + info.dailyBestCarId);
    let car = info.dailyBestCar || (carNamesMap[info.dailyBestCarId]?.car) || ('Auto ' + info.dailyBestCarId);
    disp.innerHTML = `<span style="color:var(--neon-cyan);">${driver}</span> <small style="color:var(--text-muted);">(${car})</small> — <span style="color:var(--neon-purple); text-shadow:0 0 8px rgba(181,50,255,0.7); font-size:17px;">${timeStr}</span>`;
  } else {
    disp.innerHTML = `<span style="color:var(--text-muted); font-size:13px;">Noch keine Bestzeit gesetzt</span>`;
  }
}

function updateTimerDisplayString(sec) {
  let timerElem = document.getElementById('timerDisplay');
  let dimTimer = document.getElementById('dimmerTimerText');
  let textVal = "--:--";

  if (qualiFinishedHold) {
    textVal = "QUALI BEENDET";
  } else if (isSessionFinished) {
    textVal = (currentMode === 1) ? "QUALI BEENDET" : "ZIEL";
  } else if (timeExpiredAnnounced) {
    textVal = "LETZTE RUNDE";
  } else if (sec >= 0) {
    let m = Math.floor(sec / 60);
    let s = sec % 60;
    textVal = (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
  }

  if (timerElem) timerElem.innerText = textVal;
  if (dimTimer) dimTimer.innerText = textVal;
}

function startLocalTimerTicker() {
  if (localTimerInterval) clearInterval(localTimerInterval);

  localTimerInterval = setInterval(() => {
    if (localSecondsLeft > 0 && !isSessionFinished) {
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

async function updateData(fullSync = false) {
  if (isUpdatingData) return;
  isUpdatingData = true;

  try {
    let response = await fetch('/api/laps');
    if (response.status === 503) {
      isUpdatingData = false;
      return;
    }
    let data = await response.json();
    
    lastLapsData = data;
    let html = '';
    let isDrag = (currentMode === 4);

    applySectorColumnVisibility();
    let mask = getVisibleSectorsMask();
    let visibleSectorCols = (mask.showSz1 ? 1 : 0) + (mask.showSz2 ? 1 : 0) + (mask.showSz3 ? 1 : 0) + (mask.showSz4 ? 1 : 0);
    let totalCols = 5 + visibleSectorCols;

    let leaderLaps = 0;
    let minSz1 = 0, minSz2 = 0, minSz3 = 0, minSz4 = 0;
    let minBestLap = 0;
    let activeCarsWithLaps = 0;

    data.forEach(r => {
      if (r.completedLaps > 0 || r.bestLap > 0) activeCarsWithLaps++;
      let b1 = r.bestSz1 > 0 ? r.bestSz1 : 0;
      if (b1 > 0 && (minSz1 === 0 || b1 < minSz1)) minSz1 = b1;
      let b2 = r.bestSz2 > 0 ? r.bestSz2 : 0;
      if (b2 > 0 && (minSz2 === 0 || b2 < minSz2)) minSz2 = b2;
      let b3 = r.bestSz3 > 0 ? r.bestSz3 : 0;
      if (b3 > 0 && (minSz3 === 0 || b3 < minSz3)) minSz3 = b3;
      let b4 = r.bestSz4 > 0 ? r.bestSz4 : 0;
      if (b4 > 0 && (minSz4 === 0 || b4 < minSz4)) minSz4 = b4;
      if (r.bestLap > 0 && (minBestLap === 0 || r.bestLap < minBestLap)) minBestLap = r.bestLap;
    });

    if (minBestLap > 0) sessionBestLap = minBestLap;
    let nowTs = Date.now();

    let displayData = [];

    if (data.length === 0) {
      html = `<tr><td colspan="${totalCols}">Keine Autos erfasst</td></tr>`;
      currentLeaderId = null;
    } else {
      let leaderSearch = [...data];
      if (currentMode === 1 || currentMode === 4) {
        leaderSearch.sort((a, b) => (a.bestLap || 999999) - (b.bestLap || 999999));
      } else {
        leaderSearch.sort((a, b) => b.completedLaps - a.completedLaps || a.lastLap - b.lastLap);
      }

      leaderLaps = leaderSearch[0].completedLaps;
      let leaderElem = document.getElementById('leaderLapDisplay');
      if (leaderElem) leaderElem.innerText = leaderLaps;

      let newLeader = leaderSearch[0];
      if ((currentMode === 2 || currentMode === 3) && isSessionActive && !isSessionFinished) {
        if (currentLeaderId === null && newLeader && newLeader.completedLaps > 0) {
          currentLeaderId = newLeader.id;
        }
      }

      displayData = sortLapData(data);

      let thLapsLabel = document.getElementById('th-laps-label');
      if (thLapsLabel) thLapsLabel.innerText = isDrag ? "Läufe" : "Runde";
      let thLastLabel = document.getElementById('th-last-label');
      if (thLastLabel) thLastLabel.innerText = isDrag ? "Laufzeit" : "Letzte Zeit";

      displayData.forEach((row) => {
        carNamesMap[row.id] = { driver: row.driver, car: row.car };
        let defaultPosition = row._defaultPos;
        
        if (row.state === 'box' || row.state === 'pause' || row.state === 'outlap') {
          statusCarsMap[row.id] = row.state;
        } else if (row.state === 'normal') {
          statusCarsMap[row.id] = null;
        }

        checkLapEvents(row, leaderLaps, defaultPosition, minBestLap);

        let lastDisplay = '---';
        let lastClass = 'lap-time-neutral';

        if (isDrag) {
          if (row.state === 'drag_running') {
            lastDisplay = '<span class="pit-badge" style="color:var(--neon-green); border-color:var(--neon-green);">Auf Strecke</span>';
          } else if (row.lastLap > 0) {
            lastDisplay = (row.lastLap / 1000).toFixed(2) + 's';
            let isOverall = (activeCarsWithLaps > 1 && minBestLap > 0 && row.lastLap <= minBestLap);
            let isPB = (row.bestLap > 0 && row.lastLap <= row.bestLap);
            if (isOverall && isPB) lastClass = 'lap-time-purple';
            else if (isPB) lastClass = 'lap-time-green';
          }
        } else {
          if (statusCarsMap[row.id] === 'outlap' || row.state === 'outlap') {
            lastDisplay = '<span class="outlap-badge">Outlap</span>';
          } else if (statusCarsMap[row.id] === 'box' || row.state === 'box') {
            lastDisplay = '<span class="pit-badge">Box</span>';
          } else if (statusCarsMap[row.id] === 'pause' || row.state === 'pause') {
            lastDisplay = '<span class="pause-badge">Pause</span>';
          } else if (awaitingBaseCrossMap[row.id]) {
            lastDisplay = '<span class="outlap-badge">Outlap</span>';
          } else if (row.lastLap > 0) {
            lastDisplay = (row.lastLap / 1000).toFixed(2) + 's';
            let isOverall = (activeCarsWithLaps > 1 && minBestLap > 0 && row.lastLap <= minBestLap);
            let isPB = (row.bestLap > 0 && row.lastLap <= row.bestLap);

            if (isOverall && isPB) lastClass = 'lap-time-purple';
            else if (isPB) lastClass = 'lap-time-green';
          }
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

        let getSectorClassWithFlash = (carId, secNum, val, bestVal, overallMin) => {
          let baseClass = 'sector-cyan';
          if (val > 0) {
            if (overallMin > 0 && val <= overallMin) baseClass = 'sector-best-purple';
            else if (bestVal > 0 && val <= bestVal) baseClass = 'sector-best-green';
            else baseClass = 'sector-yellow';
          }
          let flashKey = `${carId}_${secNum}`;
          let flashInfo = sectorFlashUntilMap[flashKey];
          if (flashInfo && flashInfo.until > nowTs) {
            baseClass += ` sector-flash-${flashInfo.color}`;
          }
          return baseClass;
        };

        let sz1Class = getSectorClassWithFlash(row.id, 1, row.sz1, row.bestSz1, minSz1);
        let sz2Class = getSectorClassWithFlash(row.id, 2, row.sz2, row.bestSz2, minSz2);
        let sz3Class = getSectorClassWithFlash(row.id, 3, row.sz3, row.bestSz3, minSz3);
        let sz4Class = getSectorClassWithFlash(row.id, 4, row.sz4, row.bestSz4, minSz4);

        let lapsDisplay = row.completedLaps;
        if (currentMode === 4) lapsDisplay = row.completedLaps > 0 ? (row.completedLaps + (row.completedLaps === 1 ? ' Lauf' : ' Läufe')) : '0';
        else if (targetLaps > 0) lapsDisplay += ' / ' + targetLaps;

        let isMySelectedCar = selectedTTSCars.includes(row.id);
        let rowHighlightClass = isMySelectedCar ? ' class="my-selected-car-row"' : '';

        html += `<tr${rowHighlightClass}>
          <td><strong>P${defaultPosition}</strong></td>
          <td><span class="driver-name" onclick="openDriverStats(${row.id})">${row.driver}</span> <br><small style="color:var(--text-muted);">(${row.car} - ID ${row.id})</small></td>
          <td>${lapsDisplay}</td>
          <td id="sz-cell-${row.id}-1" class="${sz1Class}" style="${mask.showSz1 ? '' : 'display:none;'}">${sz1Disp}</td>
          <td id="sz-cell-${row.id}-2" class="${sz2Class}" style="${mask.showSz2 ? '' : 'display:none;'}">${sz2Disp}</td>
          <td id="sz-cell-${row.id}-3" class="${sz3Class}" style="${mask.showSz3 ? '' : 'display:none;'}">${sz3Disp}</td>
          <td id="sz-cell-${row.id}-4" class="${sz4Class}" style="${mask.showSz4 ? '' : 'display:none;'}">${sz4Disp}</td>
          <td class="${lastClass}">${lastDisplay}</td>
          <td class="${bestClass}" style="font-weight:bold;">${bestDisplay}</td>
        </tr>`;
      });
    }

    let lapTable = document.getElementById('lapTable');
    if (lapTable) lapTable.innerHTML = html;
    updateTTSFilterDisplay();

    // --- RENN-DASHBOARD UPDATE (1 ODER 2 FAHRER SPLITSCREEN) ---
    let dMode = document.getElementById('dimmerModeText');
    if (dMode) {
      if (currentMode === 0) dMode.innerText = "FREIES TRAINING";
      else if (currentMode === 1) dMode.innerText = qualiFinishedHold ? "QUALIFYING (BEENDET)" : "QUALIFYING";
      else if (currentMode === 2) dMode.innerText = (targetLaps > 0) ? `RENNEN (${leaderLaps}/${targetLaps} RND)` : "RENNEN (RUNDEN)";
      else if (currentMode === 3) dMode.innerText = "RENNEN (ZEIT)";
      else if (currentMode === 4) dMode.innerText = "DRAGRACE";
    }

    let dLeaderLap = document.getElementById('dimmerLeaderLapText');
    if (dLeaderLap) dLeaderLap.innerText = leaderLaps || 0;

    let dSessionBest = document.getElementById('dimmerSessionBestText') || document.getElementById('dimmerOverallText');
    if (dSessionBest) dSessionBest.innerText = (minBestLap > 0) ? (minBestLap / 1000).toFixed(2) + 's' : '--:--';

    // 1) Rennleiter Dashboard (Leaderboard-Tabelle für alle tatsächlich erfassten Fahrzeuge)
    let dLeaderboardBody = document.getElementById('dimmerLeaderboardBody');
    if (dLeaderboardBody) {
      updateDimmerLeaderboard(displayData, minBestLap, minSz1, minSz2, minSz3, minSz4, leaderLaps, targetLaps);
    }

    // 2) Fahrer Dashboard (1 oder 2 Fahrer Splitscreen)
    let dContainer = document.getElementById('dimmerDriversContainer');
    if (dContainer) {
      let isTwoDrivers = (selectedTTSCars.length >= 2);
      let targetId1 = (selectedTTSCars.length >= 1) ? selectedTTSCars[0] : (displayData[0]?.id || 1);
      let targetId2 = isTwoDrivers ? selectedTTSCars[1] : null;

      let col2 = document.getElementById('dimmerDriverCol2');
      let divLine = document.getElementById('dimmerDivider');

      dContainer.className = isTwoDrivers ? 'dimmer-drivers-container split-drivers' : 'dimmer-drivers-container single-driver';
      if (col2) col2.style.display = isTwoDrivers ? 'flex' : 'none';
      if (divLine) divLine.style.display = isTwoDrivers ? 'block' : 'none';

      let row1 = displayData.find(r => r.id === targetId1);
      let row2 = targetId2 ? displayData.find(r => r.id === targetId2) : null;

      updateDashboardDriverCol(1, targetId1, row1, minBestLap, minSz1, minSz2, minSz3, minSz4);
      if (isTwoDrivers && targetId2) {
        updateDashboardDriverCol(2, targetId2, row2, minBestLap, minSz1, minSz2, minSz3, minSz4);
      }
    }
  } catch (err) {
  } finally {
    isUpdatingData = false;
  }

  if (!fullSync) return;

  try {
    let r = await fetch('/api/timeleft');
    if (r.ok) {
      let info = await r.json();

      currentMode = info.mode;
      serverMillisOffset = info.serverMillis || 0;
      updateTrackRecordBar(info);

      let modeText = "Freies Training";
      if (currentMode === 1) modeText = qualiFinishedHold ? "Qualifying (Beendet)" : "Qualifying";
      if (currentMode === 2) modeText = "Rennen (Runden)";
      if (currentMode === 3) modeText = "Rennen (Zeit)";
      if (currentMode === 4) modeText = "Dragrace";
      let modeElem = document.getElementById('modeDisplay');
      if (modeElem) modeElem.innerText = modeText;

      let serverSecondsLeft = info.secondsLeft;
      if (qualiFinishedHold) {
        updateTimerDisplayString(0);
      } else if (serverSecondsLeft >= 0) {
        if (serverSecondsLeft > 0) {
          isSessionActive = true;
          isSessionFinished = false;
        }
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
          if (info.allFinished) finishSessionNow();
        }
      } else {
        localSecondsLeft = -1;
        updateTimerDisplayString(-1);
      }
    }
  } catch (err) {}

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
  } catch (err) {}

  if (userRole === 'admin') {
    try {
      let r = await fetch('/api/recent');
      if (r.ok) {
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
      }
    } catch (err) {}
  }
}

function finishSessionNow() {
  if (isSessionFinished) return;
  isSessionFinished = true;
  isSessionActive = false;
  timeExpiredAnnounced = false;
  localSecondsLeft = 0;

  let timerElem = document.getElementById('timerDisplay');

  if (currentMode === 1) {
    qualiFinishedHold = true;
    updateTimerDisplayString(0);
    let modeElem = document.getElementById('modeDisplay');
    if (modeElem) modeElem.innerText = "Qualifying (Beendet)";

    let activeCars = lastLapsData.filter(car => car.bestLap > 0);
    activeCars.sort((a, b) => a.bestLap - b.bestLap);

    if (activeCars.length > 0) {
      let pole = activeCars[0];
      let poleDriver = pole.driver || (carNamesMap[pole.id]?.driver || ("Fahrer " + pole.id));
      let poleCar = pole.car || (carNamesMap[pole.id]?.car || ("Auto " + pole.id));
      let poleTimeDisp = (pole.bestLap / 1000).toFixed(2) + 's';
      let poleTimeSpeech = (pole.bestLap / 1000).toFixed(2).replace('.', ',');

      let banner = document.getElementById('poleBanner');
      let bannerTxt = document.getElementById('poleBannerText');
      if (banner && bannerTxt) {
        bannerTxt.innerHTML = `${poleDriver} <small style="color:var(--neon-cyan);">(${poleCar})</small> — <span style="color:var(--neon-green);">${poleTimeDisp}</span>`;
        banner.style.display = 'block';
      }

      let speechText = `Qualifying beendet! Herzlichen Glückwunsch ${poleDriver} auf ${poleCar} zur Pohl Posischen mit ${poleTimeSpeech} Sekunden!`;

      let p2 = activeCars[1];
      let p3 = activeCars[2];
      if (p2 && p3) {
        let p2Driver = p2.driver || (carNamesMap[p2.id]?.driver || ("Fahrer " + p2.id));
        let p3Driver = p3.driver || (carNamesMap[p3.id]?.driver || ("Fahrer " + p3.id));
        speechText += ` Auf Platz 2 folgen ${p2Driver} und ${p3Driver} auf Platz 3.`;
      } else if (p2) {
        let p2Driver = p2.driver || (carNamesMap[p2.id]?.driver || ("Fahrer " + p2.id));
        speechText += ` Auf Platz 2 folgt ${p2Driver}.`;
      }

      if (activeCars.length > 3) {
        for (let i = 3; i < activeCars.length; i++) {
          let pos = i + 1;
          let dName = activeCars[i].driver || (carNamesMap[activeCars[i].id]?.driver || ("Fahrer " + activeCars[i].id));
          speechText += ` Platz ${pos} geht an ${dName}.`;
        }
      }
      speak(speechText, true);
    } else {
      speak("Das Qualifying ist beendet!", true);
    }

    if (userRole === 'admin' && typeof window.autoSaveSessionResult === 'function') {
      window.autoSaveSessionResult(1, "Qualifying");
    }
  } else if (currentMode === 2 || currentMode === 3 || currentMode === 4) {
    if (timerElem) timerElem.innerText = "ZIEL";
    announceRaceResults();
  } else {
    if (timerElem) timerElem.innerText = "ZIEL";
    speak("Die Zeit ist abgelaufen! Durchgang beendet!", false);
  }
}

let pollTimeout = null;
let isPolling = false;

async function pollData() {
  if (isPolling) return;
  if (document.hidden) {
    clearTimeout(pollTimeout);
    pollTimeout = setTimeout(pollData, 6000);
    return;
  }
  isPolling = true;
  try {
    let mConfig = document.getElementById('configModal');
    let mDriverConfig = document.getElementById('driverConfigModal');
    let mStatsOverview = document.getElementById('statsOverviewModal');
    let mStatsDriver = document.getElementById('statsModal');
    let mOtaAlert = document.getElementById('otaAlertModal');
    let mReset = document.getElementById('resetConfirmModal');
    let mRole = document.getElementById('roleSelectModal');
    
    let isModalOpen = (mConfig && mConfig.style.display === 'block') ||
                      (mDriverConfig && mDriverConfig.style.display === 'block') ||
                      (mStatsOverview && mStatsOverview.style.display === 'block') ||
                      (mStatsDriver && mStatsDriver.style.display === 'block') ||
                      (mOtaAlert && mOtaAlert.style.display === 'block') ||
                      (mReset && mReset.style.display === 'block') ||
                      (mRole && mRole.style.display === 'block');
                      
    if (!isModalOpen) {
      await updateData(true);
    }
  } catch (e) {
  } finally {
    isPolling = false;
    clearTimeout(pollTimeout);
    let wsConnected = (socket && socket.readyState === WebSocket.OPEN);
    let baseInterval = wsConnected ? (userRole === 'driver' ? 12000 : 6500) : 4000;
    let jitter = Math.floor(Math.random() * 3500); 
    pollTimeout = setTimeout(pollData, baseInterval + jitter);
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
  updateData(false);
}

function handleSectorTrackEntryTTS(carId) {
  if (currentMode === 4) return;

  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);
  if (awaitingBaseCrossMap[carId]) return;

  if (previousLaps[carId] === undefined || previousLaps[carId] === 0) {
    awaitingBaseCrossMap[carId] = true;
    previousLaps[carId] = 0;
    previousBestLaps[carId] = 0;
    lapHistoryMap[carId] = [];
    statusCarsMap[carId] = 'outlap';
    if (isCarAllowedForTTS(carId)) speak(driverName + " auf Outlap!");
  } else if (statusCarsMap[carId] === 'pause') {
    awaitingBaseCrossMap[carId] = true;
    statusCarsMap[carId] = 'outlap';
    if (isCarAllowedForTTS(carId)) speak(driverName + " wieder gestartet");
  } else if (statusCarsMap[carId] === 'box') {
    awaitingBaseCrossMap[carId] = true;
    statusCarsMap[carId] = 'outlap';
    if (isCarAllowedForTTS(carId)) speak(driverName + " verlässt die Box");
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
    awaitingBaseCrossMap[carId] = false;

    if (currentMode !== 4 && isCarAllowedForTTS(carId)) speak(driverName + " gestartet!");
    return;
  }

  if (currentLaps > prevLaps) {
    let completedLapNum = currentLaps;
    previousLaps[carId] = currentLaps;
    awaitingBaseCrossMap[carId] = false;

    if (lastAnnouncedLapMap[carId] === completedLapNum) return;
    lastAnnouncedLapMap[carId] = completedLapNum;

    let lastLapMs = car.lastLap;
    let history = lapHistoryMap[carId] || [];

    if (statusCarsMap[carId]) statusCarsMap[carId] = null;

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
          let oldLeaderId = currentLeaderId;
          currentLeaderId = carId;
          if (isCarAllowedForTTS(carId) || isCarAllowedForTTS(oldLeaderId)) {
            leadChangeOccurred = true;
            speak("Neuer Führender! " + driverName + " übernimmt die Spitze!", false);
          }
        }
      }
    }

    let isPersonalBest = false;
    let isAbsoluteBest = false;

    if (minBestLap > 0 && (sessionBestLap === 0 || minBestLap < sessionBestLap)) sessionBestLap = minBestLap;

    let deltaMs = null;
    if (completedLapNum >= 1 && lastLapMs > 0) {
      if (car.bestLap > 0 && lastLapMs <= car.bestLap && (prevBest === 0 || lastLapMs < prevBest)) {
        isPersonalBest = true;
        if (prevBest > 0) {
          deltaMs = lastLapMs - prevBest;
        }
        previousBestLaps[carId] = lastLapMs;
      } else if (prevBest > 0) {
        deltaMs = lastLapMs - prevBest;
      } else if (car.bestLap > 0) {
        deltaMs = lastLapMs - car.bestLap;
      }

      if (isPersonalBest && sessionBestLap > 0 && lastLapMs <= sessionBestLap) {
        sessionBestLap = lastLapMs;
        isAbsoluteBest = true;
      } else if (isPersonalBest && sessionBestLap === 0) {
        sessionBestLap = lastLapMs;
        isAbsoluteBest = true;
      }

      lastLapDeltaMap[carId] = {
        diffMs: deltaMs,
        isPb: isPersonalBest,
        isAbsolute: isAbsoluteBest
      };
    }

    if (!isSessionFinished && car.lastLap > 0 && isCarAllowedForTTS(carId) && !leadChangeOccurred && currentMode !== 4) {
      let timeSec = (car.lastLap / 1000).toFixed(2).replace('.', ',');
      let posText = ", ";
      if (currentMode !== 0 && position > 0) posText = ", Platz " + position + ", ";

      let lapAnnouncement = "";
      let isRaceMode = (currentMode >= 2);

      if (isRaceMode && !announceLapTimesInRace) {
        if (isAbsoluteBest) lapAnnouncement = driverName + posText + "schnellste Rennrunde! " + timeSec;
        else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        else lapAnnouncement = driverName + posText;
      } else {
        if (isAbsoluteBest) lapAnnouncement = driverName + posText + "schnellste Rennrunde! " + timeSec;
        else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
        else lapAnnouncement = driverName + posText + timeSec;
      }
      speak(lapAnnouncement);
    }
  }
}

window.addEventListener('DOMContentLoaded', () => {
  window.sessionStartTs = Math.floor(Date.now() / 1000);

  // Rolle verbindlich anhand der aufgerufenen Seite festlegen
  let isAdminPage = window.location.pathname.toLowerCase().includes('admin');
  userRole = isAdminPage ? 'admin' : 'driver';
  localStorage.setItem('rctl_user_role', userRole);
  applyUserRoleUI(userRole);

  loadVersion();
  syncTimeWithServerOnce();

  // Zufälliger Jitter (0 - 800ms) beim Start, um WebSocket-Stürme auf die ESP-CPU zu brechen
  setTimeout(() => {
    initWebSocket();
  }, Math.floor(Math.random() * 800));

  loadSectorConfigInitial();
  loadCarNames().then(() => {
    updateTTSFilterDisplay();
    populateDriverQuickSelect();
  });
  loadWifiStatus();
  startLocalTimerTicker();
  pollData();
});

window.onclick = function(event) {
  if (event.target === document.getElementById('adminConfirmModal')) closeAdminConfirmModal();
  if (event.target === document.getElementById('configModal')) closeConfigModal();
  if (event.target === document.getElementById('driverConfigModal')) closeDriverConfigModal();
  if (event.target === document.getElementById('statsModal')) closeStatsModal();
  if (event.target === document.getElementById('statsOverviewModal')) closeStatsOverviewModal();
  if (event.target === document.getElementById('resetConfirmModal')) closeResetModal();
  if (event.target === document.getElementById('roleSelectModal') && userRole) {
    document.getElementById('roleSelectModal').style.display = 'none';
  }
  if (event.target === document.getElementById('otaAlertModal')) {
    if (typeof closeOtaAlert === 'function') closeOtaAlert();
  }
};

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    closeAdminConfirmModal();
    closeConfigModal();
    closeDriverConfigModal();
    closeStatsModal();
    closeStatsOverviewModal();
    closeResetModal();
    if (userRole && document.getElementById('roleSelectModal')) {
      document.getElementById('roleSelectModal').style.display = 'none';
    }
    if (typeof closeOtaAlert === 'function') closeOtaAlert();
  }
});

document.addEventListener('click', function() {
  if (!audioUnmuted) unmuteIOSAudio();
}, { once: true });
