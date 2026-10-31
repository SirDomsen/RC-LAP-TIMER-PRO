const MAX_CARS = 15;
let ttsEnabled = true;
window.countdownTimeouts = window.countdownTimeouts || [];
window.UI_VERSION = '4.5.2'; 

function formatLapTime(ms, fallback = '---') {
  if (!ms || isNaN(ms) || ms <= 0) return fallback;
  let totalSec = Math.round(ms) / 1000;
  let min = Math.floor(totalSec / 60);
  let sec = totalSec - (min * 60);
  if (sec >= 59.995) {
    min++;
    sec = 0;
  }
  let secStr = sec.toFixed(2);
  if (min > 0) {
    if (sec < 10) secStr = '0' + secStr;
    return `${min}:${secStr}`;
  }
  return secStr;
}
window.formatLapTime = formatLapTime; 
let sectorAudioEnabled = JSON.parse(localStorage.getItem('sectorAudioEnabled') ?? 'true');
let ghostTtsDeltaEnabled = JSON.parse(localStorage.getItem('ghostTtsDeltaEnabled') ?? 'true');
let ghostAudioBeepsEnabled = JSON.parse(localStorage.getItem('ghostAudioBeepsEnabled') ?? 'true');
let currentGhostTarget = { type: 0, lap: 0, s1: 0, s2: 0, s3: 0, s4: 0, szMask: 0, driver: "", car: "" };

let userRole = localStorage.getItem('rctl_user_role') || null; 

let activeTrackId = 'standard';
let activeTrackName = 'Test-Strecke';
let allTracksList = [{ id: 'standard', name: 'Test-Strecke', bestLap: 0, recordLap: 0, allTimeRecords: [], dailyRecords: [] }];
let pendingRecordFlagsMap = {};
let lastKnownInfoCache = null;
let isInitialDataLoaded = false;
let lastStartAnnouncedTs = {};
let currentFsPct = 0;
let currentFsUsed = 0;
let currentFsTotal = 0;
let isStorageFull = false; 

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
let announcedLaps5 = false;
let announcedLaps3 = false;
let announcedLaps1 = false;

let localSecondsLeft = -1;
let localTimerInterval = null;
let isUpdatingData = false; 
let pendingUpdateRequested = false; 
let pendingUpdateFullSync = false; 

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
let muteCountdownTts = JSON.parse(localStorage.getItem('muteCountdownTts') ?? 'false');

// --- UNIVERSELLES DIALOG-SYSTEM (Vorbild Firmware-OTA) ---
let _dialogResolver = null;

function showUniversalDialog(opts) {
  return new Promise((resolve) => {
    _dialogResolver = resolve;
    const modal = document.getElementById('universalModal');
    if (!modal) {
      if (opts.isAlert) {
        alert(opts.message ? opts.message.replace(/<[^>]*>/g, '') : opts.title);
        resolve(true);
      } else {
        resolve(confirm(opts.message ? opts.message.replace(/<[^>]*>/g, '') : opts.title));
      }
      return;
    }
    const content = document.getElementById('universalModalContent');
    const title = document.getElementById('universalModalTitle');
    const box = document.getElementById('universalModalBox');
    const boxLabel = document.getElementById('universalModalBoxLabel');
    const boxContent = document.getElementById('universalModalBoxContent');
    const boxSub = document.getElementById('universalModalBoxSub');
    const msg = document.getElementById('universalModalMessage');
    const cancelBtn = document.getElementById('universalModalCancelBtn');
    const confirmBtn = document.getElementById('universalModalConfirmBtn');

    // Theme festlegen
    const theme = opts.theme || 'info';
    const cleanTheme = (theme === 'red') ? 'danger' : theme;
    if (content) content.className = 'modal-content theme-' + cleanTheme;

    const themeColors = {
      purple: 'var(--neon-purple)',
      danger: 'var(--neon-red)',
      orange: 'var(--neon-orange)',
      cyan: 'var(--neon-cyan)',
      info: 'var(--neon-cyan)',
      green: 'var(--neon-green)',
      success: 'var(--neon-green)'
    };
    const activeColor = themeColors[cleanTheme] || 'var(--neon-cyan)';
    if (title) {
      title.style.color = activeColor;
      title.innerHTML = opts.title || 'HINWEIS';
    }

    // Details-Box (optional)
    if (box) {
      if (opts.boxContent) {
        box.style.display = 'block';
        box.style.borderColor = activeColor;
        if (boxLabel) boxLabel.innerText = opts.boxLabel || 'Details:';
        if (boxContent) boxContent.innerText = opts.boxContent;
        if (boxSub) {
          if (opts.boxSub) {
            boxSub.style.display = 'block';
            boxSub.innerText = opts.boxSub;
          } else {
            boxSub.style.display = 'none';
          }
        }
      } else {
        box.style.display = 'none';
      }
    }

    // Nachricht
    if (msg) msg.innerHTML = opts.message || '';

    // Buttons
    if (opts.isAlert) {
      if (cancelBtn) cancelBtn.style.display = 'none';
      if (confirmBtn) {
        confirmBtn.innerText = opts.confirmText || 'VERSTANDEN';
        confirmBtn.className = 'btn ' + (cleanTheme === 'purple' ? 'btn-purple' : cleanTheme === 'danger' ? 'btn-red' : cleanTheme === 'orange' ? 'btn-orange' : cleanTheme === 'green' || cleanTheme === 'success' ? 'btn-green' : 'btn-cyan');
        confirmBtn.style.flex = '1';
      }
    } else {
      if (cancelBtn) {
        cancelBtn.style.display = 'block';
        cancelBtn.innerText = opts.cancelText || 'ABBRECHEN';
      }
      if (confirmBtn) {
        confirmBtn.innerText = opts.confirmText || 'BESTÄTIGEN';
        confirmBtn.className = 'btn ' + (cleanTheme === 'purple' ? 'btn-purple' : cleanTheme === 'danger' ? 'btn-red' : cleanTheme === 'orange' ? 'btn-orange' : cleanTheme === 'green' || cleanTheme === 'success' ? 'btn-green' : 'btn-cyan');
      }
    }

    modal.style.display = 'block';
  });
}

function closeUniversalDialog(result) {
  const modal = document.getElementById('universalModal');
  if (modal) modal.style.display = 'none';
  if (_dialogResolver) {
    let res = _dialogResolver;
    _dialogResolver = null;
    res(result);
  }
}

function showConfirm(opts) {
  return showUniversalDialog(Object.assign({}, opts, { isAlert: false }));
}

function showAlert(opts) {
  return showUniversalDialog(Object.assign({}, opts, { isAlert: true }));
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
  if (currentMode === 1 || currentMode === 4 || currentMode === 5) {
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
if (typeof openStatsOverviewModal !== 'function') {
  window.openStatsOverviewModal = async function() {
    if (typeof switchStatsTab !== 'function') {
      if (statsLoading) return;
      statsLoading = true;
      let btn = document.getElementById('statsOverviewBtn');
      let orig = btn ? btn.innerText : '';
      if (btn) btn.innerText = "⏳ Lade...";
      let s = document.createElement('script');
      s.src = 'stats.js?v=' + (window.UI_VERSION || '4.3.0');
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
        if (typeof showAlert === 'function') {
          showAlert({
            title: "❌ FEHLER",
            theme: "danger",
            message: "Fehler beim Laden der Statistiken."
          });
        }
      };
      document.body.appendChild(s);
    } else {
      let m = document.getElementById('statsOverviewModal');
      if (m) m.style.display = 'block';
      if (typeof sessionFileCache !== 'undefined') sessionFileCache = {};
      await Promise.all([
        typeof loadCarNames === 'function' ? loadCarNames() : Promise.resolve(),
        typeof loadTracks === 'function' ? loadTracks() : Promise.resolve(),
        typeof fetchSessionsAndStorageInfo === 'function' ? fetchSessionsAndStorageInfo() : Promise.resolve()
      ]);
      if (typeof updateStatsModalDailyBest === 'function') updateStatsModalDailyBest();
      if (typeof switchStatsTab === 'function') {
        switchStatsTab(typeof currentTab !== 'undefined' ? currentTab : 'training');
      }
    }
  };
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
function syncGhostHudSlot() {
  let overlay = document.getElementById('oledDimOverlay');
  let isDimmerActive = overlay && (overlay.style.display === 'flex' || overlay.style.display === 'block');
  let targetSlotId = (isDimmerActive && currentMode === 5) ? 'ghostHudSlotDimmer' : 'ghostHudSlotNormal';
  let targetSlot = document.getElementById(targetSlotId);
  let banner = document.getElementById('ghostBeatenBanner');
  let hud = document.getElementById('ghostHudContainer');

  if (targetSlot) {
    if (banner && banner.parentElement !== targetSlot) {
      targetSlot.appendChild(banner);
    }
    if (hud && hud.parentElement !== targetSlot) {
      targetSlot.appendChild(hud);
    }
  }

  // Im OLED-Dashboard bei Geisterjagd die Mehrfahrer-Container ausblenden, damit das Geister-HUD Platz hat:
  let dimTable = document.getElementById('dimmerLeaderboardContainer');
  let dimDrivers = document.getElementById('dimmerDriversContainer');
  let isGhostInDimmer = isDimmerActive && currentMode === 5;

  if (dimTable) {
    dimTable.style.display = isGhostInDimmer ? 'none' : '';
  }
  if (dimDrivers) {
    dimDrivers.style.display = isGhostInDimmer ? 'none' : '';
  }
}
window.syncGhostHudSlot = syncGhostHudSlot;

function activateOledDimmer() {
  if (!audioUnmuted) unmuteIOSAudio();
  let overlay = document.getElementById('oledDimOverlay');
  if (overlay) overlay.style.display = 'flex';
  syncGhostHudSlot();
  updateData(true);
}

function deactivateOledDimmer() {
  let overlay = document.getElementById('oledDimOverlay');
  if (overlay) overlay.style.display = 'none';
  syncGhostHudSlot();
}

// --- FAHRER-EINSTELLUNGEN MODAL ---
function openDriverConfigModal() {
  let modal = document.getElementById('driverConfigModal');
  if (modal) modal.style.display = 'block';

  let szAudioCb = document.getElementById('driver_cfg_sector_audio');
  if (szAudioCb) szAudioCb.checked = sectorAudioEnabled;

  let lapTimeCb = document.getElementById('driver_cfg_announce_lap_times');
  if (lapTimeCb) lapTimeCb.checked = announceLapTimesInRace;

  let muteCdownCb = document.getElementById('driver_cfg_mute_countdown_tts');
  if (muteCdownCb) muteCdownCb.checked = muteCountdownTts;

  let gAudioCb = document.getElementById('driver_cfg_ghost_audio');
  if (gAudioCb) gAudioCb.checked = ghostAudioBeepsEnabled;

  let gTtsCb = document.getElementById('driver_cfg_ghost_tts');
  if (gTtsCb) gTtsCb.checked = ghostTtsDeltaEnabled;
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

  let muteCdownCb = document.getElementById('driver_cfg_mute_countdown_tts');
  if (muteCdownCb) {
    muteCountdownTts = muteCdownCb.checked;
    localStorage.setItem('muteCountdownTts', JSON.stringify(muteCountdownTts));
  }

  let gAudioCb = document.getElementById('driver_cfg_ghost_audio');
  if (gAudioCb) {
    ghostAudioBeepsEnabled = gAudioCb.checked;
    localStorage.setItem('ghostAudioBeepsEnabled', JSON.stringify(ghostAudioBeepsEnabled));
  }

  let gTtsCb = document.getElementById('driver_cfg_ghost_tts');
  if (gTtsCb) {
    ghostTtsDeltaEnabled = gTtsCb.checked;
    localStorage.setItem('ghostTtsDeltaEnabled', JSON.stringify(ghostTtsDeltaEnabled));
  }

  let gAudioVal = (typeof ghostAudioBeepsEnabled !== 'undefined' && ghostAudioBeepsEnabled) ? "1" : "0";
  fetch('/api/setsectorconfig?audio=' + (sectorAudioEnabled ? "1" : "0") + '&ghostAudio=' + gAudioVal, { method: 'POST' }).catch(() => {});
}

// --- GEISTERJAGD (GHOST HUD & LIVE EVENTS) ---
function onGhostTargetLiveEvent(msg) {
  if (!msg) return;
  currentGhostTarget = {
    type: msg.targetType || 0,
    lap: msg.lap || 0,
    s1: msg.s1 || 0,
    s2: msg.s2 || 0,
    s3: msg.s3 || 0,
    s4: msg.s4 || 0,
    szMask: msg.szMask || 0,
    driver: msg.driver || "Geist",
    car: msg.car || ""
  };
  renderGhostHud();
}

function onGhostBeatenLiveEvent(msg) {
  if (!msg) return;
  currentGhostTarget.lap = msg.lapTime;
  currentGhostTarget.s1 = msg.s1 || 0;
  currentGhostTarget.s2 = msg.s2 || 0;
  currentGhostTarget.s3 = msg.s3 || 0;
  currentGhostTarget.s4 = msg.s4 || 0;
  currentGhostTarget.szMask = msg.szMask || 0;
  currentGhostTarget.driver = msg.driver || "Neuer Geist";

  let banner = document.getElementById('ghostBeatenBanner');
  let bannerText = document.getElementById('ghostBeatenBannerText');
  if (banner && bannerText) {
    bannerText.innerText = `${formatLapTime(msg.lapTime)} von ${msg.driver}`;
    banner.style.display = 'block';
    setTimeout(() => {
      if (banner) banner.style.display = 'none';
    }, 8000);
  }

  let pill = document.getElementById('ghostLiveDeltaPill');
  if (pill) {
    pill.style.display = 'inline-block';
    pill.innerText = (msg.delta <= 0 ? '' : '+') + (msg.delta / 1000).toFixed(2) + 's';
    pill.className = 'ghost-delta-pill ghost-delta-minus';
  }

  if (typeof speak === 'function') {
    speak("Geist geschlagen! Neue Bestzeit!", true);
  }

  renderGhostHud();
}

function updateGhostHudSector(msg) {
  if (!msg) return;
  let sec = msg.sector;
  let cell = document.getElementById(`ghostHunterCell${sec}`);
  let valEl = document.getElementById(`ghostHunterSz${sec}`);
  let deltaEl = document.getElementById(`ghostDeltaSz${sec}`);

  if (valEl && msg.time > 0) {
    valEl.innerText = formatLapTime(msg.time);
  }

  if (deltaEl && msg.ghostDelta !== undefined) {
    let dSec = (msg.ghostDelta / 1000).toFixed(2);
    let isAhead = (msg.ghostDelta <= 0);
    deltaEl.innerText = (isAhead ? '' : '+') + dSec + 's';
    deltaEl.style.color = isAhead ? 'var(--neon-green)' : 'var(--neon-orange)';
    
    if (cell) {
      let flashClass = isAhead ? 'sector-flash-green' : 'sector-flash-yellow';
      cell.className = 'ghost-sector-cell';
      void cell.offsetWidth;
      cell.className = `ghost-sector-cell ${flashClass}`;
    }

    let pill = document.getElementById('ghostLiveDeltaPill');
    if (pill) {
      pill.style.display = 'inline-block';
      pill.innerText = (isAhead ? '' : '+') + dSec + 's';
      pill.className = 'ghost-delta-pill ' + (isAhead ? 'ghost-delta-minus' : 'ghost-delta-plus');
    }
  }
}

function renderGhostHud() {
  syncGhostHudSlot();
  let hud = document.getElementById('ghostHudContainer');
  if (!hud) return;

  if (currentMode !== 5) {
    hud.style.display = 'none';
    return;
  }

  hud.style.display = 'block';

  // 1. Geist-Karte
  let gName = document.getElementById('ghostTargetName');
  let gTime = document.getElementById('ghostTargetTime');
  if (gName) {
    let typeLabels = ["Streckenrekord", "Tagesbestzeit", "Persönliche Bestzeit", "Zielzeit"];
    let prefix = typeLabels[currentGhostTarget.type] || "Geist";
    gName.innerText = currentGhostTarget.driver ? `${prefix} (${currentGhostTarget.driver})` : prefix;
  }
  if (gTime) {
    gTime.innerText = currentGhostTarget.lap > 0 ? formatLapTime(currentGhostTarget.lap) : "--:--";
  }

  let s1El = document.getElementById('ghostTargetSz1');
  let s2El = document.getElementById('ghostTargetSz2');
  let s3El = document.getElementById('ghostTargetSz3');
  let s4El = document.getElementById('ghostTargetSz4');
  if (s1El) s1El.innerText = `SZ1: ${currentGhostTarget.s1 > 0 ? formatLapTime(currentGhostTarget.s1) : '---'}`;
  if (s2El) s2El.innerText = `SZ2: ${currentGhostTarget.s2 > 0 ? formatLapTime(currentGhostTarget.s2) : '---'}`;
  if (s3El) s3El.innerText = `SZ3: ${currentGhostTarget.s3 > 0 ? formatLapTime(currentGhostTarget.s3) : '---'}`;
  if (s4El) s4El.innerText = `Ziel: ${currentGhostTarget.s4 > 0 ? formatLapTime(currentGhostTarget.s4) : (currentGhostTarget.lap > 0 ? formatLapTime(currentGhostTarget.lap) : '---')}`;

  // 2. Jäger-Karte
  let activeCarId = (typeof selectedTTSCars !== 'undefined' && selectedTTSCars.length > 0) ? selectedTTSCars[0] : 1;
  let row = (typeof lastLapsData !== 'undefined') ? lastLapsData.find(r => r.id === activeCarId) : null;
  let hName = document.getElementById('ghostHunterName');
  let hLapNum = document.getElementById('ghostHunterLapNum');
  let hLastTime = document.getElementById('ghostHunterLastTime');

  if (hName) {
    let dName = (row && row.driver) || (typeof carNamesMap !== 'undefined' && carNamesMap[activeCarId]?.driver) || (`Fahrer ${activeCarId}`);
    let cName = (row && row.car) || (typeof carNamesMap !== 'undefined' && carNamesMap[activeCarId]?.car) || "";
    hName.innerText = cName ? `${dName} (${cName})` : dName;
  }
  if (hLapNum) {
    hLapNum.innerText = `Runde ${row ? row.completedLaps : 0}`;
  }
  if (hLastTime) {
    if (row && row.lastLap > 0) {
      hLastTime.innerText = formatLapTime(row.lastLap);
      if (row.bestLap > 0 && row.lastLap <= row.bestLap) {
        hLastTime.style.color = 'var(--neon-green)';
      } else {
        hLastTime.style.color = 'var(--neon-cyan)';
      }
    } else {
      hLastTime.innerText = "--:--";
      hLastTime.style.color = 'var(--neon-cyan)';
    }
  }
}

// Accolade Arcade Audio Jukebox: Steuerung des Hardware-Speakers
function playGpcWebBuzzer(cmd) {
  fetch('/api/buzzer?cmd=' + encodeURIComponent(cmd), { method: 'POST' }).catch(() => {});
}

function openArcadeModal() {
  const modal = document.getElementById('arcadeModal');
  if (!modal) return;
  modal.style.display = 'block';
  updateArcadeBuzzerStatus();
}

function closeArcadeModal() {
  const modal = document.getElementById('arcadeModal');
  if (modal) modal.style.display = 'none';
}

function updateArcadeBuzzerStatus() {
  const statusEl = document.getElementById('arcadeStatusText');
  if (!statusEl) return;
  fetch('/api/buzzer')
    .then(r => r.json())
    .then(data => {
      if (data.playing) {
        statusEl.innerHTML = `Status: <span style="color:var(--neon-green); font-weight:bold;">▶ Spielt (${data.track || 'Track'})</span>`;
      } else {
        statusEl.innerHTML = 'Status: <span style="color:var(--text-muted);">Bereit</span>';
      }
    })
    .catch(() => {
      statusEl.innerHTML = 'Status: <span style="color:var(--text-muted);">Bereit</span>';
    });
}

function playSelectedArcadeTrack() {
  const trackSel = document.getElementById('arcadeTrackSelect');
  const loopCb = document.getElementById('arcadeLoopCheckbox');
  const statusEl = document.getElementById('arcadeStatusText');
  if (!trackSel) return;

  const track = trackSel.value;
  const loop = loopCb ? (loopCb.checked ? '1' : '0') : '1';

  if (statusEl) {
    const trackName = trackSel.options[trackSel.selectedIndex]?.text || track;
    statusEl.innerHTML = `Status: <span style="color:var(--neon-cyan);">Starte ${trackName}...</span>`;
  }

  fetch(`/api/buzzer?cmd=play&track=${encodeURIComponent(track)}&loop=${loop}`, { method: 'POST' })
    .then(() => {
      setTimeout(updateArcadeBuzzerStatus, 250);
    })
    .catch(() => {
      if (statusEl) statusEl.innerHTML = `<span style="color:var(--neon-red);">Fehler beim Starten</span>`;
    });
}

function stopArcadeTrack() {
  const statusEl = document.getElementById('arcadeStatusText');
  if (statusEl) {
    statusEl.innerHTML = 'Status: <span style="color:var(--neon-gold);">Stoppe...</span>';
  }
  fetch('/api/buzzer?cmd=stop', { method: 'POST' })
    .then(() => {
      setTimeout(updateArcadeBuzzerStatus, 200);
    })
    .catch(() => {});
}

function playArcadeFx(fxCmd) {
  const statusEl = document.getElementById('arcadeStatusText');
  if (statusEl) {
    statusEl.innerHTML = `Status: <span style="color:var(--neon-gold);">Soundeffekt gesendet...</span>`;
  }
  fetch(`/api/buzzer?cmd=${encodeURIComponent(fxCmd)}`, { method: 'POST' })
    .then(() => {
      setTimeout(updateArcadeBuzzerStatus, 400);
    })
    .catch(() => {});
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
  if (dPos) dPos.innerText = (currentMode === 5) ? "GHOST" : `P${pos}`;

  let pbLabel = dPb && dPb.parentElement ? dPb.parentElement.querySelector('.dimmer-label') : null;
  if (pbLabel) pbLabel.innerText = (currentMode === 5) ? "GEIST (ZIELZEIT)" : "Persönliche Bestzeit";

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

  if (dPb) {
    if (currentMode === 5 && currentGhostTarget && currentGhostTarget.lap > 0) {
      dPb.innerText = formatLapTime(currentGhostTarget.lap, '--:--');
    } else {
      dPb.innerText = formatLapTime(rowData.bestLap, '--:--');
    }
  }

  if (dLast) {
    if (rowData.lastLapIsOutLap) {
      dLast.innerText = rowData.lastLap > 0 ? "Outlap (" + formatLapTime(rowData.lastLap) + ")" : "Outlap";
      dLast.style.color = 'var(--text-muted)';
    } else {
      dLast.innerText = formatLapTime(rowData.lastLap, '--:--');
      let isOverall = (sessionOverallBestLap > 0 && rowData.lastLap <= sessionOverallBestLap);
      let isPb = (rowData.bestLap > 0 && rowData.lastLap <= rowData.bestLap);
      if (isOverall) dLast.style.color = 'var(--neon-purple)';
      else if (isPb) dLast.style.color = 'var(--neon-green)';
      else dLast.style.color = 'var(--text-main)';
    }
  }

  if (dDelta) {
    if (rowData.lastLapIsOutLap) {
      dDelta.innerText = '';
    } else if (currentMode === 5 && currentGhostTarget && currentGhostTarget.lap > 0 && rowData.lastLap > 0) {
      let gDiff = rowData.lastLap - currentGhostTarget.lap;
      let gSec = (Math.abs(gDiff) / 1000).toFixed(2).replace('.', ',');
      if (gDiff <= 0) {
        dDelta.innerText = `-${gSec}s (GEIST GESCHLAGEN!)`;
        dDelta.style.color = 'var(--neon-green)';
      } else {
        dDelta.innerText = `+${gSec}s (HINTER GEIST)`;
        dDelta.style.color = 'var(--neon-red)';
      }
    } else {
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

      let secStr = formatLapTime(val);
      return `<td><span style="font-weight:700; ${colorStyle} ${shadowStyle}">${secStr}</span></td>`;
    };

    let sz1Cell = renderSzCell(1, row.sz1, row.bestSz1, minSz1, mask.showSz1);
    let sz2Cell = renderSzCell(2, row.sz2, row.bestSz2, minSz2, mask.showSz2);
    let sz3Cell = renderSzCell(3, row.sz3, row.bestSz3, minSz3, mask.showSz3);
    let sz4Cell = renderSzCell(4, row.sz4, row.bestSz4, minSz4, mask.showSz4);

    // Letzte Runde & Delta
    let lastDisp = '---';
    let lastColor = 'color:var(--text-main);';
    if (row.lastLapIsOutLap) {
      lastDisp = row.lastLap > 0 ? "Outlap (" + formatLapTime(row.lastLap) + ")" : "Outlap";
      lastColor = 'color:var(--text-muted);';
    } else if (row.lastLap > 0) {
      lastDisp = formatLapTime(row.lastLap);
      let isOverallLast = (minBestLap > 0 && row.lastLap <= minBestLap);
      let isPbLast = (row.bestLap > 0 && row.lastLap <= row.bestLap);
      if (isOverallLast) lastColor = 'color:var(--neon-purple);';
      else if (isPbLast) lastColor = 'color:var(--neon-green);';
    }

    let deltaHtml = '';
    let deltaInfo = lastLapDeltaMap[row.id];
    if (row.lastLapIsOutLap) {
      deltaHtml = '';
    } else if (deltaInfo && deltaInfo.diffMs !== null && row.lastLap > 0) {
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
      bestDisp = formatLapTime(row.bestLap);
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

async function syncTimeWithServerOnce() {
  let nowSec = Math.floor(Date.now() / 1000);
  let tzOffsetSec = -(new Date().getTimezoneOffset() * 60);
  try {
    await fetch('/api/synctime?ts=' + nowSec + '&tz=' + tzOffsetSec, { method: 'POST' });
  } catch (e) {
    console.error("Einmaliger Zeit-Sync fehlgeschlagen:", e);
  }
}

// --- STRECKEN-VERWALTUNG & SPEICHERPLATZ-SCHUTZ ---
function formatRecordDate(dStr) {
  if (!dStr) return '';
  let parts = String(dStr).split('-');
  if (parts.length === 3) {
    let y = parts[0].length === 4 ? parts[0].slice(2) : parts[0];
    return `${parts[2]}.${parts[1]}.${y}`;
  }
  return dStr;
}

async function loadTracks() {
  try {
    let res = await fetch('/api/tracks');
    if (res.ok) {
      let data = await res.json();
      if (data && Array.isArray(data.tracks) && data.tracks.length > 0) {
        allTracksList = data.tracks;
        activeTrackId = data.active || data.tracks[0].id;
        let found = allTracksList.find(t => t.id === activeTrackId) || allTracksList[0];
        activeTrackName = found.name;
        if (found.allTimeRecords && found.allTimeRecords.length > 0) {
          let topRec = found.allTimeRecords[0];
          window.allTimeBestLap = topRec.lap;
          window.allTimeBestDriver = topRec.driver;
          window.allTimeBestCar = topRec.car;
          window.allTimeBestSzMask = topRec.szMask || 0;
          window.allTimeBestS1 = topRec.s1 || 0;
          window.allTimeBestS2 = topRec.s2 || 0;
          window.allTimeBestS3 = topRec.s3 || 0;
          window.allTimeBestS4 = topRec.s4 || 0;
        } else {
          window.allTimeBestLap = found.recordLap || 0;
          window.allTimeBestDriver = found.recordDriver || "";
          window.allTimeBestCar = found.recordCar || "";
          window.allTimeBestSzMask = 0;
          window.allTimeBestS1 = 0;
          window.allTimeBestS2 = 0;
          window.allTimeBestS3 = 0;
          window.allTimeBestS4 = 0;
        }
        if (found.dailyRecords && found.dailyRecords.length > 0) {
          let topDaily = found.dailyRecords[0];
          window.dailyBestLap = topDaily.lap;
          window.dailyBestDriver = topDaily.driver;
          window.dailyBestCar = topDaily.car;
          window.dailyBestSzMask = topDaily.szMask || 0;
          window.dailyBestS1 = topDaily.s1 || 0;
          window.dailyBestS2 = topDaily.s2 || 0;
          window.dailyBestS3 = topDaily.s3 || 0;
          window.dailyBestS4 = topDaily.s4 || 0;
        } else {
          window.dailyBestLap = found.bestLap || 0;
          window.dailyBestDriver = found.bestDriver || "";
          window.dailyBestCar = found.bestCar || "";
          window.dailyBestSzMask = 0;
          window.dailyBestS1 = 0;
          window.dailyBestS2 = 0;
          window.dailyBestS3 = 0;
          window.dailyBestS4 = 0;
        }
      }
      populateTrackSelectDropdowns();
      updateTrackRecordBar();
      if (typeof updateStatsModalDailyBest === 'function') {
        updateStatsModalDailyBest();
      }
      let trModal = document.getElementById('trackRecordsModal');
      if (trModal && trModal.style.display === 'block') {
        let sel = document.getElementById('top5ModalTrackSelect');
        renderTrackRecordsModalContent(sel ? sel.value : activeTrackId);
      }
    }
  } catch (e) {
    console.warn("Streckenliste konnte nicht geladen werden:", e);
  }
}

function populateTrackSelectDropdowns() {
  let adminSel = document.getElementById('activeTrackSelect');
  if (adminSel) {
    let html = '';
    allTracksList.forEach(t => {
      let sel = (t.id === activeTrackId) ? 'selected' : '';
      html += `<option value="${t.id}" ${sel}>🏁 ${t.name}</option>`;
    });
    adminSel.innerHTML = html;
  }

  let statsSel = document.getElementById('statsTrackFilterSelect');
  if (statsSel) {
    let prevVal = statsSel.value || 'all';
    let html = `<option value="all" ${prevVal === 'all' ? 'selected' : ''}>🌍 Alle Strecken</option>`;
    allTracksList.forEach(t => {
      let sel = (t.id === prevVal) ? 'selected' : '';
      html += `<option value="${t.id}" ${sel}>📍 ${t.name}</option>`;
    });
    statsSel.innerHTML = html;
  }

  let top5Sel = document.getElementById('top5ModalTrackSelect');
  if (top5Sel) {
    let prevVal = top5Sel.value || activeTrackId;
    if (!allTracksList.some(t => t.id === prevVal)) prevVal = activeTrackId;
    let html = '';
    allTracksList.forEach(t => {
      let sel = (t.id === prevVal) ? 'selected' : '';
      html += `<option value="${t.id}" ${sel}>🏁 ${t.name}</option>`;
    });
    top5Sel.innerHTML = html;
  }

  let badge = document.getElementById('activeTrackNameBadge');
  if (badge) badge.innerText = activeTrackName;
  let dimBadge = document.getElementById('dimmerTrackNameVal');
  if (dimBadge) dimBadge.innerText = activeTrackName;
}

function findSessionForRecord(record, trackId) {
  if (typeof allSessionsCache === 'undefined' || !Array.isArray(allSessionsCache) || allSessionsCache.length === 0) return null;
  let tId = trackId || record.trackId || (typeof activeTrackId !== 'undefined' ? activeTrackId : 'standard');
  let targetLap = record.lap;
  if (!targetLap || targetLap <= 0) return null;

  // 1. Suche nach gleicher Strecke + passender Bestzeit und Fahrer/Car
  let match = allSessionsCache.find(s => {
    let sTrack = s.trackId || 'standard';
    if (sTrack !== tId) return false;
    if (!Array.isArray(s.results)) return false;
    return s.results.some(r => {
      let carMatch = (!record.carId || r.id === record.carId);
      let driverMatch = (!record.driver || !r.driver || r.driver.toLowerCase() === record.driver.toLowerCase());
      let timeMatch = (r.bestLap === targetLap) || (Math.abs(r.bestLap - targetLap) <= 5);
      return (carMatch || driverMatch) && timeMatch;
    });
  });

  // 2. Fallback: Beliebige Strecke (falls umbenannt oder migriert)
  if (!match) {
    match = allSessionsCache.find(s => {
      if (!Array.isArray(s.results)) return false;
      return s.results.some(r => {
        let carMatch = (!record.carId || r.id === record.carId);
        let driverMatch = (!record.driver || !r.driver || r.driver.toLowerCase() === record.driver.toLowerCase());
        let timeMatch = (r.bestLap === targetLap) || (Math.abs(r.bestLap - targetLap) <= 5);
        return (carMatch || driverMatch) && timeMatch;
      });
    });
  }
  return match;
}
window.findSessionForRecord = findSessionForRecord;

window.openSessionFromRecordJump = async function(sessionId, mode) {
  if (typeof closeTrackRecordsModal === 'function') {
    closeTrackRecordsModal();
  }
  let m = document.getElementById('statsOverviewModal');
  if (!m || m.style.display !== 'block') {
    if (typeof openStatsOverviewModal === 'function') {
      await openStatsOverviewModal();
    }
  }
  if (typeof openSessionFromRecord === 'function') {
    openSessionFromRecord(mode, sessionId);
  }
};

async function ensureStatsLoaded() {
  if (typeof fetchSessionsAndStorageInfo === 'function') {
    if (typeof allSessionsCache === 'undefined' || !Array.isArray(allSessionsCache) || allSessionsCache.length === 0) {
      try { await fetchSessionsAndStorageInfo(); } catch (e) {}
    }
    return;
  }
  await new Promise((resolve) => {
    let s = document.createElement('script');
    s.src = 'stats.js?v=' + (window.UI_VERSION || '4.5.2');
    s.onload = () => resolve();
    s.onerror = () => resolve();
    document.body.appendChild(s);
  });
  if (typeof fetchSessionsAndStorageInfo === 'function') {
    try { await fetchSessionsAndStorageInfo(); } catch (e) {}
  }
}
window.ensureStatsLoaded = ensureStatsLoaded;

async function refillTrackRecordsFromSessions(targetTrackId) {
  let tId = targetTrackId || (typeof activeTrackId !== 'undefined' ? activeTrackId : 'standard');
  await ensureStatsLoaded();
  if (typeof allSessionsCache === 'undefined' || !Array.isArray(allSessionsCache) || allSessionsCache.length === 0) return;

  // Streckenliste aus /tracks.json laden (oder allTracksList)
  let tracksDoc = { active: (typeof activeTrackId !== 'undefined' ? activeTrackId : 'standard'), tracks: [] };
  try {
    let tRes = await fetch('/tracks.json?_t=' + Date.now(), { cache: 'no-store' });
    if (tRes.ok) tracksDoc = await tRes.json();
    else if (Array.isArray(allTracksList)) tracksDoc.tracks = JSON.parse(JSON.stringify(allTracksList));
  } catch (e) {
    if (Array.isArray(allTracksList)) tracksDoc.tracks = JSON.parse(JSON.stringify(allTracksList));
  }

  let trackObj = (tracksDoc.tracks || []).find(t => t.id === tId);
  if (!trackObj) return;

  let todayStr = '';
  if (typeof getCurrentLocalDateStr === 'function') {
    todayStr = getCurrentLocalDateStr();
  }
  let nowD = new Date();
  let todayFormatted = `${String(nowD.getFullYear()).slice(-2)}-${String(nowD.getMonth()+1).padStart(2,'0')}-${String(nowD.getDate()).padStart(2,'0')}`;

  // 1. Sammle alle verbleibenden Fahrer/Auto-Kombinationen aus allen Sessions dieser Strecke
  let allTimeMap = {};
  let dailyMap = {};

  allSessionsCache.forEach(s => {
    let sTrack = s.trackId || 'standard';
    if (sTrack !== tId) return;

    let sDateStr = '';
    let isToday = false;
    if (s.startTs) {
      let sD = new Date(s.startTs * 1000);
      isToday = (sD.toDateString() === nowD.toDateString());
      sDateStr = `${String(sD.getFullYear()).slice(-2)}-${String(sD.getMonth()+1).padStart(2,'0')}-${String(sD.getDate()).padStart(2,'0')}`;
    }
    if (!isToday && s.date && todayStr && s.date === todayStr) isToday = true;

    if (s.mode !== 4 && Array.isArray(s.results)) {
      s.results.forEach(r => {
        if (!r.bestLap || r.bestLap <= 0) return;
        let dName = (r.driver || '').trim() || (typeof getCarDriverName === 'function' ? getCarDriverName(r.id) : ('Fahrer ' + r.id));
        let cName = (r.car || '').trim() || (typeof getCarCarName === 'function' ? getCarCarName(r.id) : ('Auto ' + r.id));
        let comboKey = `${dName.toLowerCase()}_${cName.toLowerCase()}`;

        let entry = {
          lap: r.bestLap,
          carId: r.id || 0,
          driver: dName,
          car: cName,
          date: sDateStr || (s.date || '')
        };

        if (!allTimeMap[comboKey] || r.bestLap < allTimeMap[comboKey].lap) {
          allTimeMap[comboKey] = entry;
        }

        if (isToday) {
          if (!dailyMap[comboKey] || r.bestLap < dailyMap[comboKey].lap) {
            dailyMap[comboKey] = entry;
          }
        }
      });
    }
  });

  // 2. Bestehende Einträge der Strecke einbeziehen (falls manuelle Einträge existieren)
  let existingAllTime = Array.isArray(trackObj.allTimeRecords) ? trackObj.allTimeRecords : [];
  existingAllTime.forEach(e => {
    if (!e || !e.lap || e.lap <= 0) return;
    let dName = (e.driver || '').trim();
    let cName = (e.car || '').trim();
    let comboKey = `${dName.toLowerCase()}_${cName.toLowerCase()}`;
    if (!allTimeMap[comboKey] || e.lap < allTimeMap[comboKey].lap) {
      allTimeMap[comboKey] = e;
    }
  });

  let existingDaily = Array.isArray(trackObj.dailyRecords) ? trackObj.dailyRecords : [];
  existingDaily.forEach(e => {
    if (!e || !e.lap || e.lap <= 0) return;
    let dName = (e.driver || '').trim();
    let cName = (e.car || '').trim();
    let comboKey = `${dName.toLowerCase()}_${cName.toLowerCase()}`;
    if (!dailyMap[comboKey] || e.lap < dailyMap[comboKey].lap) {
      dailyMap[comboKey] = e;
    }
  });

  // 3. Top 5 sortieren und auffüllen
  let sortedAllTime = Object.values(allTimeMap).sort((a, b) => a.lap - b.lap).slice(0, 5);
  let sortedDaily = Object.values(dailyMap).sort((a, b) => a.lap - b.lap).slice(0, 5);

  trackObj.allTimeRecords = sortedAllTime;
  trackObj.dailyRecords = sortedDaily;

  if (sortedAllTime.length > 0) {
    trackObj.recordLap = sortedAllTime[0].lap;
    trackObj.recordCarId = sortedAllTime[0].carId;
    trackObj.recordDriver = sortedAllTime[0].driver;
    trackObj.recordCar = sortedAllTime[0].car;
    trackObj.recordDate = sortedAllTime[0].date;
  } else {
    trackObj.recordLap = 0;
    trackObj.recordCarId = 0;
    trackObj.recordDriver = "";
    trackObj.recordCar = "";
    trackObj.recordDate = "";
  }

  if (sortedDaily.length > 0) {
    trackObj.bestLap = sortedDaily[0].lap;
    trackObj.bestCarId = sortedDaily[0].carId;
    trackObj.bestDriver = sortedDaily[0].driver;
    trackObj.bestCar = sortedDaily[0].car;
    trackObj.bestDate = sortedDaily[0].date;
  } else {
    trackObj.bestLap = 0;
    trackObj.bestCarId = 0;
    trackObj.bestDriver = "";
    trackObj.bestCar = "";
    trackObj.bestDate = todayFormatted;
  }

  // 4. Zurückschreiben via /api/upload
  try {
    let tBlob = new Blob([JSON.stringify(tracksDoc)], { type: 'application/json' });
    let tFd = new FormData();
    tFd.append('data', tBlob, '/tracks.json');
    await fetch('/api/upload', { method: 'POST', body: tFd });
  } catch (upErr) {
    console.warn("Fehler beim Aktualisieren von /tracks.json:", upErr);
  }
}
window.refillTrackRecordsFromSessions = refillTrackRecordsFromSessions;

async function openTrackRecordsModal(trackId) {
  let modal = document.getElementById('trackRecordsModal');
  if (!modal) return;
  modal.style.display = 'block';

  await ensureStatsLoaded();

  await loadTracks();
  let targetId = trackId || activeTrackId;
  let sel = document.getElementById('top5ModalTrackSelect');
  if (sel) sel.value = targetId;
  renderTrackRecordsModalContent(targetId);
}

function closeTrackRecordsModal() {
  let modal = document.getElementById('trackRecordsModal');
  if (modal) modal.style.display = 'none';
}

function renderTrackRecordsModalContent(trackId) {
  let body = document.getElementById('trackRecordsModalBody');
  if (!body) return;

  let targetId = trackId || activeTrackId;
  let trackObj = allTracksList.find(t => t.id === targetId) || allTracksList[0];
  if (!trackObj) {
    body.innerHTML = '<p style="color:var(--text-muted); text-align:center;">Keine Streckendaten verfügbar.</p>';
    return;
  }

  let isAdmin = (userRole === 'admin');

  let allTimeArr = Array.isArray(trackObj.allTimeRecords) ? trackObj.allTimeRecords : [];
  if (allTimeArr.length === 0 && trackObj.recordLap > 0) {
    allTimeArr = [{
      lap: trackObj.recordLap,
      carId: trackObj.recordCarId || 0,
      driver: trackObj.recordDriver || '',
      car: trackObj.recordCar || '',
      date: trackObj.recordDate || ''
    }];
  }

  let dailyArr = Array.isArray(trackObj.dailyRecords) ? trackObj.dailyRecords : [];
  if (dailyArr.length === 0 && trackObj.bestLap > 0) {
    dailyArr = [{
      lap: trackObj.bestLap,
      carId: trackObj.bestCarId || 0,
      driver: trackObj.bestDriver || '',
      car: trackObj.bestCar || '',
      date: trackObj.bestDate || ''
    }];
  }

  let renderListHtml = (entries, typeKey, emptyText, isAllTime) => {
    if (!entries || entries.length === 0) {
      return `<div class="top5-empty-text">${emptyText}</div>`;
    }
    let rowsHtml = '';
    entries.forEach((entry, idx) => {
      let rank = idx + 1;
      let rankClass = rank === 1 ? 'top5-rank-1' : (rank === 2 ? 'top5-rank-2' : (rank === 3 ? 'top5-rank-3' : 'top5-rank-other'));
      let driverName = entry.driver || (carNamesMap[entry.carId]?.driver) || ('Fahrer ' + (entry.carId || '?'));
      let carName = entry.car || (carNamesMap[entry.carId]?.car) || ('Auto ' + (entry.carId || '?'));
      let timeStr = formatLapTime(entry.lap);
      let dateFormatted = formatRecordDate(entry.date);
      let dateHtml = (isAllTime && dateFormatted) ? `<span class="top5-entry-date">📅 ${dateFormatted}</span>` : '';
      let prevHintHtml = (isAdmin && entry.prevLap && entry.prevLap > 0)
        ? `<span style="font-size:10px; color:var(--text-muted); margin-left:4px;" title="Wird beim Löschen dieser Zeit automatisch wiederhergestellt">(vorher: ${formatLapTime(entry.prevLap)})</span>`
        : '';

      let timeColor = (rank === 1)
        ? (isAllTime ? 'color:var(--neon-gold); text-shadow:0 0 8px rgba(255,215,0,0.5);' : 'color:var(--neon-purple); text-shadow:0 0 8px rgba(181,50,255,0.6);')
        : 'color:var(--neon-green);';

      let matchedSession = (typeof findSessionForRecord === 'function') ? findSessionForRecord(entry, trackObj.id) : null;
      let sessionClickAttr = matchedSession
        ? `onclick="openSessionFromRecordJump('${matchedSession.id}', ${matchedSession.mode !== undefined ? matchedSession.mode : 0})" title="🔍 Klick öffnet Session in Statistiken"`
        : '';
      let sessionClass = matchedSession ? 'clickable' : '';
      let sessionHint = matchedSession
        ? `<span style="font-size:10px; color:var(--neon-cyan); margin-left:6px;" title="Session vorhanden">🔍</span>`
        : '';

      let delBtn = isAdmin
        ? `<button class="top5-del-btn" onclick="event.stopPropagation(); deleteTrackRecordEntryUI('${trackObj.id}', '${typeKey}', ${idx})" title="Eintrag löschen${entry.prevLap > 0 ? ' (stellt vorherige Bestzeit wieder her)' : ''}">✕</button>`
        : '';

      rowsHtml += `
        <div class="top5-entry-row ${sessionClass}" ${sessionClickAttr}>
          <div class="top5-entry-left">
            <span class="top5-rank-badge ${rankClass}">P${rank}</span>
            <div class="top5-driver-info">
              <div>
                <span class="top5-driver-name">${driverName}</span>
                <span class="top5-car-name">(${carName})</span>
                ${sessionHint}
              </div>
              <div>
                ${dateHtml}
                ${prevHintHtml}
              </div>
            </div>
          </div>
          <div class="top5-entry-right">
            <span class="top5-entry-time" style="${timeColor}">${timeStr}</span>
            ${delBtn}
          </div>
        </div>`;
    });
    return rowsHtml;
  };

  let allTimeClearBtn = (isAdmin && allTimeArr.length > 0)
    ? `<button class="btn btn-danger" style="padding:2px 8px; font-size:10px; margin:0;" onclick="deleteTrackRecordEntryUI('${trackObj.id}', 'alltime', -1)">Alle löschen</button>`
    : '';
  let dailyClearBtn = (isAdmin && dailyArr.length > 0)
    ? `<button class="btn btn-danger" style="padding:2px 8px; font-size:10px; margin:0;" onclick="deleteTrackRecordEntryUI('${trackObj.id}', 'daily', -1)">Alle löschen</button>`
    : '';

  body.innerHTML = `
    <div class="top5-section-card alltime">
      <div class="top5-section-title" style="color:var(--neon-gold);">
        <span>🏆 STRECKENREKORD – TOP 5 (ALL-TIME)</span>
        ${allTimeClearBtn}
      </div>
      ${renderListHtml(allTimeArr, 'alltime', 'Noch kein Streckenrekord auf dieser Strecke', true)}
    </div>

    <div class="top5-section-card daily">
      <div class="top5-section-title" style="color:var(--neon-green);">
        <span>⏱️ TAGESBESTZEIT – TOP 5 (HEUTE)</span>
        ${dailyClearBtn}
      </div>
      ${renderListHtml(dailyArr, 'daily', 'Heute noch keine Zeit auf dieser Strecke gefahren', false)}
    </div>
    <div style="font-size:11px; color:var(--text-muted); text-align:center; margin-top:6px;">
      💡 Pro Fahrer &amp; Fahrzeug wird jeweils die schnellste Zeit geführt.
      ${isAdmin ? '<br>Beim Löschen einer Fehlmessung rückt die vorherige Bestzeit des Fahrers automatisch nach.' : ''}
    </div>
  `;
}

async function deleteTrackRecordEntryUI(trackId, type, index) {
  let targetId = trackId || activeTrackId;
  let trackObj = allTracksList.find(t => t.id === targetId);
  let isDaily = (type === 'daily');
  let title = isDaily ? '⏱️ TAGESBESTZEIT LÖSCHEN' : '🏆 STRECKENREKORD LÖSCHEN';
  let desc = '';
  let boxInfo = '';
  let cascadeInfo = '';

  await ensureStatsLoaded();

  let allTimeArr = Array.isArray(trackObj?.allTimeRecords) ? trackObj.allTimeRecords : [];
  if (allTimeArr.length === 0 && trackObj?.recordLap > 0) {
    allTimeArr = [{ lap: trackObj.recordLap, carId: trackObj.recordCarId || 0, driver: trackObj.recordDriver || '', car: trackObj.recordCar || '', date: trackObj.recordDate || '' }];
  }

  let dailyArr = Array.isArray(trackObj?.dailyRecords) ? trackObj.dailyRecords : [];
  if (dailyArr.length === 0 && trackObj?.bestLap > 0) {
    dailyArr = [{ lap: trackObj.bestLap, carId: trackObj.bestCarId || 0, driver: trackObj.bestDriver || '', car: trackObj.bestCar || '', date: trackObj.bestDate || '' }];
  }

  let matchedSession = null;
  let matchedLap = null;

  if (index >= 0) {
    let targetEntry = isDaily ? dailyArr[index] : allTimeArr[index];
    if (targetEntry) {
      if (typeof allSessionsCache !== 'undefined' && Array.isArray(allSessionsCache) && allSessionsCache.length > 0) {
        // 1. Alle Sessions dieser Strecke sammeln
        let candidateSessions = allSessionsCache.filter(s => {
          let sTrack = s.trackId || 'standard';
          return sTrack === targetId;
        });

        // 2. Bevorzuge Sessions, an denen dieses Auto oder dieser Fahrer teilgenommen hat
        let driverFiltered = candidateSessions.filter(s => {
          if (!Array.isArray(s.results)) return false;
          return s.results.some(r => {
            let matchCar = (targetEntry.carId && r.id === targetEntry.carId);
            let matchDriver = (targetEntry.driver && r.driver && r.driver.toLowerCase() === targetEntry.driver.toLowerCase());
            return matchCar || matchDriver;
          });
        });
        if (driverFiltered.length > 0) candidateSessions = driverFiltered;

        // Wenn candidateSessions leer ist: alle Sessions durchsuchen!
        let searchSessionsList = candidateSessions.length > 0 ? [...candidateSessions] : [];
        for (let s of allSessionsCache) {
          if (!searchSessionsList.includes(s)) searchSessionsList.push(s);
        }

        // 3. Durchsuche die Sessions nach der konkreten Runde mit Zeit == targetEntry.lap
        for (let s of searchSessionsList) {
          if (!s.file) continue;
          try {
            let sData = null;
            if (typeof fetchSessionFileData === 'function') {
              sData = await fetchSessionFileData(s.file);
            } else {
              let sRes = await fetch('/api/sessionfile?file=' + encodeURIComponent(s.file) + '&_t=' + Date.now(), { cache: 'no-store' });
              if (sRes.ok) sData = await sRes.json();
            }

            if (sData && Array.isArray(sData.laps)) {
              let found = sData.laps.find(l => {
                let lCar = l.c !== undefined ? l.c : (l.carId || 0);
                let lTime = l.t !== undefined ? l.t : (l.time || 0);
                let lDriver = (l.driver || '').trim();
                let matchCar = (!targetEntry.carId || lCar === targetEntry.carId);
                let matchDriver = (!targetEntry.driver || !lDriver || lDriver.toLowerCase() === targetEntry.driver.toLowerCase());
                let timeMatch = (lTime === targetEntry.lap) || (Math.abs(lTime - targetEntry.lap) <= 5);
                return (matchCar || matchDriver) && timeMatch;
              });
              if (found) {
                matchedLap = {
                  carId: found.c !== undefined ? found.c : (found.carId || targetEntry.carId),
                  lap: found.l !== undefined ? found.l : (found.lap || 0),
                  time: found.t !== undefined ? found.t : (found.time || targetEntry.lap)
                };
                matchedSession = s;
                break;
              }
            }
          } catch (err) {}
        }
      }
    }
  }

  if (index < 0) {
    desc = isDaily
      ? 'Wirklich <strong>ALLE heutigen Tagesbestzeiten</strong> dieser Strecke löschen?'
      : 'Wirklich <strong>ALLE All-Time Streckenrekorde</strong> dieser Strecke löschen?';
    boxInfo = isDaily ? 'Alle Tagesbestzeiten' : 'Alle All-Time Rekorde';
  } else {
    let targetEntry = isDaily ? dailyArr[index] : allTimeArr[index];
    let driverName = targetEntry ? (targetEntry.driver || (carNamesMap[targetEntry.carId]?.driver) || ('Fahrer ' + (targetEntry.carId || '?'))) : ('Platz ' + (index + 1));
    let timeStr = targetEntry ? formatLapTime(targetEntry.lap) : '';

    if (matchedSession && matchedLap) {
      title = isDaily ? '⏱️ TAGESBESTZEIT & RUNDE LÖSCHEN' : '🏆 REKORD & RUNDE LÖSCHEN';
      boxInfo = `Runde ${matchedLap.lap}: ${driverName} (${timeStr}) in Session ${matchedSession.id}`;
      desc = `Möchtest du diesen Rekord und die zugehörige <strong>Runde ${matchedLap.lap}</strong> dauerhaft aus der Session löschen?<br><span style="color:var(--neon-green); font-size:12px; display:block; margin-top:6px;">ℹ️ Die Runde wird vollständig aus dem Protokoll entfernt (Rundenanzahl -1) und die nächste Bestzeit rückt nach.</span>`;
    } else {
      boxInfo = `Platz ${index + 1}: ${driverName} (${timeStr})`;
      desc = `Platz ${index + 1} löschen?<br><span style="color:var(--neon-gold); font-size:12px; display:block; margin-top:6px;">ℹ️ Bestzeit wird aus der Rekordliste entfernt (vorherige Bestzeit rückt nach).</span>`;
    }

    if (isDaily) {
      if (targetEntry) {
        let matchInAllTime = allTimeArr.some(at =>
          at.lap === targetEntry.lap &&
          ((targetEntry.driver && at.driver && at.driver.toLowerCase() === targetEntry.driver.toLowerCase()) || (targetEntry.carId && at.carId === targetEntry.carId))
        );
        if (matchInAllTime) {
          cascadeInfo = `<span style="color:var(--neon-gold); font-size:12px; display:block; margin-top:8px;">⚡ <strong>Automatische Synchronisierung:</strong> Diese Zeit ist gleichzeitig als <strong>All-Time Streckenrekord</strong> eingetragen und wird auch dort automatisch entfernt.</span>`;
        }
      }
    } else {
      if (targetEntry) {
        let matchInDaily = dailyArr.some(d =>
          d.lap === targetEntry.lap &&
          ((targetEntry.driver && d.driver && d.driver.toLowerCase() === targetEntry.driver.toLowerCase()) || (targetEntry.carId && d.carId === targetEntry.carId))
        );
        if (matchInDaily) {
          cascadeInfo = `<span style="color:var(--neon-green); font-size:12px; display:block; margin-top:8px;">⚡ <strong>Automatische Synchronisierung:</strong> Dieser Streckenrekord stammt von heute und steht in der <strong>Tagesbestzeit</strong>. Er wird auch dort entfernt.</span>`;
        } else if (targetEntry.date) {
          cascadeInfo = `<span style="color:var(--text-muted); font-size:12px; display:block; margin-top:8px;">ℹ️ Dieser Streckenrekord stammt vom <strong>${formatRecordDate(targetEntry.date)}</strong>.</span>`;
        }
      }
    }
  }

  let ok = await showConfirm({
    title: title,
    theme: "danger",
    boxLabel: "Eintrag:",
    boxContent: boxInfo,
    message: desc + cascadeInfo + '<br><span style="color:var(--neon-red); font-size:12px; display:block; margin-top:6px;">⚠️ Vorgang kann nicht rückgängig gemacht werden!</span>',
    confirmText: "LÖSCHEN 🗑️",
    cancelText: "ABBRECHEN"
  });
  if (!ok) return;

  try {
    let success = false;
    if (index >= 0 && matchedSession && matchedLap) {
      // Exakt dieselbe Routine wie im Statistiken-Modal: /api/deletelap
      try {
        let lapUrl = `/api/deletelap?sessionId=${encodeURIComponent(matchedSession.id)}${matchedSession.file ? `&file=${encodeURIComponent(matchedSession.file)}` : ''}&id=${matchedLap.carId}&lap=${matchedLap.lap}&time=${matchedLap.time}`;
        let lapRes = await fetch(lapUrl, { method: 'POST' });
        if (lapRes.ok) success = true;

        if (typeof sessionFileCache !== 'undefined' && matchedSession.file) {
          delete sessionFileCache[matchedSession.file];
        }
        let safeId = (typeof sanitizeDomId === 'function') ? sanitizeDomId(matchedSession.id) : String(matchedSession.id || '').replace(/[^a-zA-Z0-9_-]/g, '_');
        let elemId = `s_${safeId}_driver_${matchedLap.carId}`;
        if (typeof embeddedLapsState !== 'undefined' && embeddedLapsState[elemId]) {
          delete embeddedLapsState[elemId].laps;
        }
      } catch (lapErr) {
        console.warn("Fehler beim Löschen via /api/deletelap:", lapErr);
      }
    } else {
      // Kein Session-Match (z.B. manueller Rekord) oder 'Alle löschen' (index < 0)
      try {
        let url = `/api/tracks/deleterecord?trackId=${encodeURIComponent(targetId)}&type=${encodeURIComponent(type)}&index=${index}`;
        let res = await fetch(url, { method: 'POST' });
        if (res.ok) success = true;
      } catch (recErr) {
        console.warn("Fehler beim Löschen des Rekordeintrags:", recErr);
      }
    }

    if (success) {
      await loadTracks();

      // Falls der Eintrag ausnahmsweise noch in tracks.json vorhanden war:
      let targetEntry = isDaily ? dailyArr[index] : allTimeArr[index];
      let updatedTrackObj = allTracksList.find(t => t.id === targetId);
      let updatedList = isDaily ? updatedTrackObj?.dailyRecords : updatedTrackObj?.allTimeRecords;
      if (index >= 0 && Array.isArray(updatedList) && targetEntry && updatedList.some(r => r.lap === targetEntry.lap && r.carId === targetEntry.carId)) {
        try {
          await fetch(`/api/tracks/deleterecord?trackId=${encodeURIComponent(targetId)}&type=${encodeURIComponent(type)}&index=${index}`, { method: 'POST' });
          await loadTracks();
        } catch (e) {}
      }

      // Top-5 Rekorde sofort wieder aus den verbleibenden Sessions auf bis zu 5 Plätze auffüllen
      try {
        await refillTrackRecordsFromSessions(targetId);
      } catch (fillErr) {
        console.warn("Fehler beim Auffüllen der Top-5 Rekorde:", fillErr);
      }

      await loadTracks();
      await updateData(true);
      if (typeof fetchSessionsAndStorageInfo === 'function') {
        await fetchSessionsAndStorageInfo();
      }
      if (typeof renderTrackRecordsModalContent === 'function') {
        renderTrackRecordsModalContent(trackId);
      }
      if (typeof updateStatsModalDailyBest === 'function') {
        updateStatsModalDailyBest();
      }
      if (typeof renderRecordsOverview === 'function') {
        renderRecordsOverview();
      }
      if (typeof renderDriversOverview === 'function' && typeof currentTab !== 'undefined' && currentTab === 'drivers') {
        renderDriversOverview();
      }
    } else {
      await showAlert({
        title: "❌ FEHLER",
        theme: "danger",
        message: "Eintrag konnte nicht gelöscht werden."
      });
    }
  } catch (e) {
    console.error("Fehler beim Löschen des Rekord-Eintrags:", e);
  }
}

async function onTrackSelectChange(trackId) {
  if (!trackId) return;
  try {
    let res = await fetch('/api/tracks/select?id=' + encodeURIComponent(trackId), { method: 'POST' });
    if (res.ok) {
      await loadTracks();
      previousLaps = {};
      previousBestLaps = {};
      lastAnnouncedLapMap = {};
      updateData(true);
    }
  } catch (e) {
    console.error("Fehler beim Streckenwechsel:", e);
  }
}

function openNewTrackModal() {
  let modal = document.getElementById('newTrackModal');
  let inp = document.getElementById('newTrackNameInput');
  if (inp) inp.value = '';
  if (modal) {
    modal.style.display = 'block';
    setTimeout(() => { if (inp) inp.focus(); }, 100);
  }
}

function closeNewTrackModal() {
  let modal = document.getElementById('newTrackModal');
  if (modal) modal.style.display = 'none';
}

async function submitNewTrack() {
  let inp = document.getElementById('newTrackNameInput');
  if (!inp) return;
  let name = inp.value.trim();
  if (!name) {
    await showAlert({
      title: "ℹ️ HINWEIS",
      theme: "orange",
      message: "Bitte einen Streckennamen eingeben."
    });
    return;
  }
  try {
    let res = await fetch('/api/tracks/add?name=' + encodeURIComponent(name), { method: 'POST' });
    if (res.ok) {
      closeNewTrackModal();
      await loadTracks();
      previousLaps = {};
      previousBestLaps = {};
      lastAnnouncedLapMap = {};
      updateData(true);
    } else {
      await showAlert({
        title: "❌ FEHLER",
        theme: "danger",
        message: "Strecke konnte nicht angelegt werden (max. 20 Strecken erlaubt)."
      });
    }
  } catch (e) {
    await showAlert({
      title: "❌ NETZWERKFEHLER",
      theme: "danger",
      message: "Netzwerkfehler beim Anlegen der Strecke: " + e.message
    });
  }
}

async function confirmDeleteActiveTrack() {
  if (allTracksList.length <= 1 && activeTrackId === 'standard') {
    let ok = await showConfirm({
      title: "🗑️ STANDARD-STRECKE ZURÜCKSETZEN",
      theme: "danger",
      boxLabel: "Strecke:",
      boxContent: activeTrackName,
      message: `Möchtest du alle Sessions und den Rekord der Strecke <strong>"${activeTrackName}"</strong> wirklich zurücksetzen?<br><span style="color:var(--neon-red); font-size:12px; display:block; margin-top:8px;">⚠️ Alle gefahrenen Durchgänge dieser Strecke werden gelöscht!</span>`,
      confirmText: "ZURÜCKSETZEN 🗑️",
      cancelText: "ABBRECHEN"
    });
    if (!ok) return;
  } else {
    let ok = await showConfirm({
      title: "🗑️ STRECKE LÖSCHEN",
      theme: "danger",
      boxLabel: "Zu löschende Strecke:",
      boxContent: activeTrackName,
      message: `Möchtest du die Strecke <strong>"${activeTrackName}"</strong> inklusive aller auf ihr gefahrenen Sessions wirklich unwiderruflich löschen?<br><span style="color:var(--neon-red); font-size:12px; display:block; margin-top:8px;">⚠️ Das gesamte Verzeichnis und alle Statistiken werden gelöscht!</span>`,
      confirmText: "STRECKE LÖSCHEN 🗑️",
      cancelText: "ABBRECHEN"
    });
    if (!ok) return;
  }
  try {
    let res = await fetch('/api/tracks/delete?id=' + encodeURIComponent(activeTrackId), { method: 'POST' });
    if (res.ok) {
      await loadTracks();
      previousLaps = {};
      previousBestLaps = {};
      lastAnnouncedLapMap = {};
      updateData(true);
    }
  } catch (e) {
    console.error("Fehler beim Löschen der Strecke:", e);
  }
}

function openStorageFullModal() {
  let modal = document.getElementById('storageFullModal');
  let pctElem = document.getElementById('storageFullModalPct');
  if (pctElem) pctElem.innerText = currentFsPct + '%';
  if (modal) modal.style.display = 'block';
}

function closeStorageFullModal() {
  let modal = document.getElementById('storageFullModal');
  if (modal) modal.style.display = 'none';
}

function updateStorageStatusUI(pct, full, usedBytes, totalBytes) {
  if (pct === undefined) return;
  currentFsPct = pct;
  currentFsUsed = usedBytes || 0;
  currentFsTotal = totalBytes || 0;
  isStorageFull = !!full || (pct >= 95);

  let usedKb = (currentFsUsed / 1024).toFixed(0);
  let totalKb = (currentFsTotal / 1024).toFixed(0);
  let detailStr = currentFsTotal > 0 ? `${pct}% (${usedKb} / ${totalKb} KB)` : `${pct}%`;

  // 1. Footer Status
  let ft = document.getElementById('footer-storage-status');
  if (ft) {
    if (pct >= 95) {
      ft.innerHTML = `Speicher: <strong style="color:var(--neon-red);">🛑 ${detailStr} – VOLL! Keine Aufzeichnung!</strong>`;
    } else if (pct >= 90) {
      ft.innerHTML = `Speicher: <strong style="color:var(--neon-orange);">⚠️ ${detailStr} – Fast voll!</strong>`;
    } else {
      ft.innerHTML = `Speicher: <strong style="color:var(--neon-cyan);">💾 ${detailStr}</strong>`;
    }
  }

  // 2. Statistik-Modal Anzeige & Warn-Banner
  let statsTxt = document.getElementById('statsFsUsageText');
  if (statsTxt) {
    statsTxt.innerText = detailStr;
    statsTxt.style.color = (pct >= 95) ? 'var(--neon-red)' : ((pct >= 90) ? 'var(--neon-orange)' : 'var(--neon-cyan)');
  }

  let warnMsg95 = `🛑 Speicher voll (${pct}%)! Sessions werden nicht mehr gespeichert. Bitte erst bestehende Sessions löschen.`;
  let warnMsg90 = `⚠️ Speicher zu ${pct}% gefüllt! Bitte bald bestehende Sessions löschen.`;

  let statsBanner = document.getElementById('statsStorageWarningBanner');
  if (statsBanner) {
    if (pct >= 95) {
      statsBanner.className = 'storage-warning-banner storage-warning-red';
      statsBanner.innerText = warnMsg95;
      statsBanner.style.display = 'block';
    } else if (pct >= 90) {
      statsBanner.className = 'storage-warning-banner storage-warning-yellow';
      statsBanner.innerText = warnMsg90;
      statsBanner.style.display = 'block';
    } else {
      statsBanner.style.display = 'none';
    }
  }

  // 3. Hauptseiten-Banner
  let mainBanner = document.getElementById('mainStorageWarningBanner');
  if (mainBanner) {
    if (pct >= 95) {
      mainBanner.className = 'storage-warning-banner storage-warning-red';
      mainBanner.innerText = warnMsg95;
      mainBanner.style.display = 'block';
    } else if (pct >= 90) {
      mainBanner.className = 'storage-warning-banner storage-warning-yellow';
      mainBanner.innerText = warnMsg90;
      mainBanner.style.display = 'block';
    } else {
      mainBanner.style.display = 'none';
    }
  }
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
  if (info) {
    lastKnownInfoCache = info;
    if (info.activeTrackId && info.activeTrackName) {
      let trackChanged = (activeTrackId !== info.activeTrackId || activeTrackName !== info.activeTrackName);
      activeTrackId = info.activeTrackId;
      activeTrackName = info.activeTrackName;
      let badge = document.getElementById('activeTrackNameBadge');
      if (badge) badge.innerText = activeTrackName;
      let dimTrack = document.getElementById('dimmerTrackNameVal');
      if (dimTrack) dimTrack.innerText = activeTrackName;
      let sel = document.getElementById('activeTrackSelect');
      if (sel && sel.value !== activeTrackId) {
        if (trackChanged) loadTracks();
        else sel.value = activeTrackId;
      }
    }
    if (info.fsPct !== undefined) {
      updateStorageStatusUI(info.fsPct, info.fsFull, info.fsUsed, info.fsTotal);
    }
  }

  let activeTrackObj = allTracksList.find(t => t.id === activeTrackId) || null;

  let recLap = (info && info.recordLap !== undefined) ? info.recordLap : (activeTrackObj ? (activeTrackObj.recordLap || 0) : 0);
  let recCarId = (info && info.recordCarId !== undefined) ? info.recordCarId : (activeTrackObj ? (activeTrackObj.recordCarId || 0) : 0);
  let recDriver = (info && info.recordDriver !== undefined) ? info.recordDriver : (activeTrackObj ? (activeTrackObj.recordDriver || '') : '');
  let recCar = (info && info.recordCar !== undefined) ? info.recordCar : (activeTrackObj ? (activeTrackObj.recordCar || '') : '');
  let recDate = (info && info.recordDate !== undefined) ? info.recordDate : (activeTrackObj ? (activeTrackObj.recordDate || '') : '');

  let dailyLap = (info && info.dailyBestLap !== undefined) ? info.dailyBestLap : (activeTrackObj ? (activeTrackObj.bestLap || 0) : 0);
  let dailyCarId = (info && info.dailyBestCarId !== undefined) ? info.dailyBestCarId : (activeTrackObj ? (activeTrackObj.bestCarId || 0) : 0);
  let dailyDriver = (info && info.dailyBestDriver !== undefined) ? info.dailyBestDriver : (activeTrackObj ? (activeTrackObj.bestDriver || '') : '');
  let dailyCar = (info && info.dailyBestCar !== undefined) ? info.dailyBestCar : (activeTrackObj ? (activeTrackObj.bestCar || '') : '');

  let hasAllTime = (recLap && recLap > 0);
  let hasDailyBest = (dailyLap && dailyLap > 0);

  if (info) {
    if (info.recordLap !== undefined) window.allTimeBestLap = info.recordLap;
    if (info.recordDriver !== undefined) window.allTimeBestDriver = info.recordDriver;
    if (info.recordCar !== undefined) window.allTimeBestCar = info.recordCar;
    if (info.recordSzMask !== undefined) window.allTimeBestSzMask = info.recordSzMask;
    if (info.recordS1 !== undefined) window.allTimeBestS1 = info.recordS1;
    if (info.recordS2 !== undefined) window.allTimeBestS2 = info.recordS2;
    if (info.recordS3 !== undefined) window.allTimeBestS3 = info.recordS3;
    if (info.recordS4 !== undefined) window.allTimeBestS4 = info.recordS4;

    if (info.dailyBestLap !== undefined) window.dailyBestLap = info.dailyBestLap;
    if (info.dailyBestDriver !== undefined) window.dailyBestDriver = info.dailyBestDriver;
    if (info.dailyBestCar !== undefined) window.dailyBestCar = info.dailyBestCar;
    if (info.dailySzMask !== undefined) window.dailyBestSzMask = info.dailySzMask;
    if (info.dailyS1 !== undefined) window.dailyBestS1 = info.dailyS1;
    if (info.dailyS2 !== undefined) window.dailyBestS2 = info.dailyS2;
    if (info.dailyS3 !== undefined) window.dailyBestS3 = info.dailyS3;
    if (info.dailyS4 !== undefined) window.dailyBestS4 = info.dailyS4;
  }

  if (typeof updateGhostHuntModalDisplay === 'function') {
    let ghModal = document.getElementById('ghostHuntModal');
    if (ghModal && ghModal.style.display === 'block') {
      updateGhostHuntModalDisplay();
    }
  }

  let bar = document.getElementById('trackRecordBar');
  let allTimeDisp = document.getElementById('allTimeRecordDisplay');
  let dailyDisp = document.getElementById('trackRecordDisplay');
  let dimBanner = document.getElementById('dimmerTrackRecordBanner');
  let dimAllTimeVal = document.getElementById('dimmerAllTimeRecordVal');
  let dimDailyVal = document.getElementById('dimmerTrackRecordVal');

  if (dimBanner) {
    dimBanner.style.display = (currentMode === 4) ? 'none' : 'inline-flex';
  }
  if (dimAllTimeVal) {
    if (hasAllTime) {
      let timeStr = formatLapTime(recLap);
      let driver = recDriver || (carNamesMap[recCarId]?.driver) || ('Fahrer ' + recCarId);
      let dFmt = formatRecordDate(recDate);
      let dSuffix = dFmt ? ` <small style="color:var(--text-muted); font-size:10px;">(${dFmt})</small>` : '';
      dimAllTimeVal.innerHTML = `${driver} — <span style="color:var(--neon-gold); font-size:13px;">${timeStr}</span>${dSuffix}`;
    } else {
      dimAllTimeVal.innerText = '--:--';
    }
  }
  if (dimDailyVal) {
    if (hasDailyBest) {
      let timeStr = formatLapTime(dailyLap);
      let driver = dailyDriver || (carNamesMap[dailyCarId]?.driver) || ('Fahrer ' + dailyCarId);
      dimDailyVal.innerHTML = `${driver} — <span style="color:var(--neon-green); font-size:13px;">${timeStr}</span>`;
    } else {
      dimDailyVal.innerText = '--:--';
    }
  }

  if (!bar) return;

  if (currentMode === 4) {
    bar.style.display = 'none';
    return;
  }
  bar.style.display = 'flex';

  if (allTimeDisp) {
    if (hasAllTime) {
      let timeStr = formatLapTime(recLap);
      let driver = recDriver || (carNamesMap[recCarId]?.driver) || ('Fahrer ' + recCarId);
      let car = recCar || (carNamesMap[recCarId]?.car) || ('Auto ' + recCarId);
      let dFmt = formatRecordDate(recDate);
      let dateTag = dFmt ? ` <small style="color:var(--text-muted); font-size:11px;">• ${dFmt}</small>` : '';
      allTimeDisp.innerHTML = `<span style="color:var(--neon-cyan);">${driver}</span> <small style="color:var(--text-muted);">(${car})</small>${dateTag} — <span style="color:var(--neon-gold); text-shadow:0 0 8px rgba(255,215,0,0.6); font-size:16px;">${timeStr}</span>`;
    } else {
      allTimeDisp.innerHTML = `<span style="color:var(--text-muted); font-size:12px;">Noch kein Streckenrekord</span>`;
    }
  }

  if (dailyDisp) {
    if (hasDailyBest) {
      let timeStr = formatLapTime(dailyLap);
      let driver = dailyDriver || (carNamesMap[dailyCarId]?.driver) || ('Fahrer ' + dailyCarId);
      let car = dailyCar || (carNamesMap[dailyCarId]?.car) || ('Auto ' + dailyCarId);
      dailyDisp.innerHTML = `<span style="color:var(--neon-cyan);">${driver}</span> <small style="color:var(--text-muted);">(${car})</small> — <span style="color:var(--neon-purple); text-shadow:0 0 8px rgba(181,50,255,0.7); font-size:16px;">${timeStr}</span>`;
    } else {
      dailyDisp.innerHTML = `<span style="color:var(--text-muted); font-size:12px;">Noch keine Bestzeit heute</span>`;
    }
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

      if (localSecondsLeft <= 60 && localSecondsLeft > 50 && !announced60) {
        announced60 = true;
        speak("Noch eine Minute!", true);
      } else if (localSecondsLeft <= 30 && localSecondsLeft > 20 && !announced30) {
        announced30 = true;
        speak("Noch 30 Sekunden!", true);
      } else if (localSecondsLeft <= 10 && localSecondsLeft > 0 && !announced10) {
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

let lastUpdateDataTime = 0;
const MIN_UPDATE_INTERVAL_MS = 1000;
let updateDataTimer = null;

function fetchWithTimeout(url, options = {}, timeoutMs = 4500) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), timeoutMs);
  return fetch(url, { ...options, signal: controller.signal })
    .finally(() => clearTimeout(id));
}

function requestUpdateData(fullSync = false, priority = false) {
  if (fullSync) pendingUpdateFullSync = true;
  pendingUpdateRequested = true;

  if (isUpdatingData) {
    return;
  }

  let isDriver = (userRole === 'driver');
  let delay = 0;
  if (priority) {
    delay = 30 + Math.floor(Math.random() * 70);
  } else if (isDriver) {
    // Fahrer-Modus: gestaffelter Jitter über 800 - 2400 ms
    delay = 800 + Math.floor(Math.random() * 1600);
  } else {
    // Admin-Modus: moderater Versatz über 250 - 650 ms
    delay = 250 + Math.floor(Math.random() * 400);
  }

  let timeSinceLast = Date.now() - lastUpdateDataTime;
  if (timeSinceLast < MIN_UPDATE_INTERVAL_MS) {
    delay = Math.max(delay, (MIN_UPDATE_INTERVAL_MS - timeSinceLast) + Math.floor(Math.random() * 300));
  }

  if (updateDataTimer) {
    return;
  }

  updateDataTimer = setTimeout(() => {
    updateDataTimer = null;
    let doFull = pendingUpdateFullSync;
    pendingUpdateRequested = false;
    pendingUpdateFullSync = false;
    updateData(doFull);
  }, delay);
}

async function updateData(fullSync = false) {
  let now = Date.now();
  if (isUpdatingData) {
    if (now - lastUpdateDataTime > 5000) {
      console.warn("[Telemetry] Stale update lock detected (>5s), resetting lock");
      isUpdatingData = false;
    } else {
      pendingUpdateRequested = true;
      if (fullSync) pendingUpdateFullSync = true;
      return;
    }
  }
  if (now - lastUpdateDataTime < MIN_UPDATE_INTERVAL_MS) {
    pendingUpdateRequested = true;
    if (fullSync) pendingUpdateFullSync = true;
    if (!updateDataTimer) {
      let isDriver = (userRole === 'driver');
      let jitter = isDriver ? Math.floor(Math.random() * 1000) : Math.floor(Math.random() * 300);
      let delay = Math.max(50, MIN_UPDATE_INTERVAL_MS - (now - lastUpdateDataTime)) + jitter;
      updateDataTimer = setTimeout(() => {
        updateDataTimer = null;
        let doFull = pendingUpdateFullSync;
        pendingUpdateRequested = false;
        pendingUpdateFullSync = false;
        updateData(doFull);
      }, delay);
    }
    return;
  }
  isUpdatingData = true;
  lastUpdateDataTime = now;

  try {
    let response = await fetchWithTimeout('/api/laps', {}, 4500);
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
      if (currentMode === 1 || currentMode === 4 || currentMode === 5) {
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
            lastDisplay = formatLapTime(row.lastLap);
            let isOverall = (activeCarsWithLaps > 1 && minBestLap > 0 && row.lastLap <= minBestLap);
            let isPB = (row.bestLap > 0 && row.lastLap <= row.bestLap);
            if (isOverall && isPB) lastClass = 'lap-time-purple';
            else if (isPB) lastClass = 'lap-time-green';
          }
        } else {
          if (row.lastLapIsOutLap) {
            lastDisplay = row.lastLap > 0 ? '<span class="outlap-badge">Outlap</span> <span style="font-size:12px; opacity:0.85;">(' + formatLapTime(row.lastLap) + ')</span>' : '<span class="outlap-badge">Outlap</span>';
            lastClass = 'lap-time-neutral';
          } else if (row.lastLap > 0) {
            lastDisplay = formatLapTime(row.lastLap);
            let isOverall = (activeCarsWithLaps > 1 && minBestLap > 0 && row.lastLap <= minBestLap);
            let isPB = (row.bestLap > 0 && row.lastLap <= row.bestLap);

            if (isOverall && isPB) lastClass = 'lap-time-purple';
            else if (isPB) lastClass = 'lap-time-green';

            if (statusCarsMap[row.id] === 'box' || row.state === 'box') {
              lastDisplay += ' <span class="pit-badge">Box</span>';
            } else if (statusCarsMap[row.id] === 'pause' || row.state === 'pause') {
              lastDisplay += ' <span class="pause-badge">Pause</span>';
            } else if (statusCarsMap[row.id] === 'outlap' || row.state === 'outlap' || awaitingBaseCrossMap[row.id]) {
              lastDisplay += ' <span class="outlap-badge">Outlap</span>';
            }
          } else if (statusCarsMap[row.id] === 'outlap' || row.state === 'outlap' || awaitingBaseCrossMap[row.id]) {
            lastDisplay = '<span class="outlap-badge">Outlap</span>';
          } else if (statusCarsMap[row.id] === 'box' || row.state === 'box') {
            lastDisplay = '<span class="pit-badge">Box</span>';
          } else if (statusCarsMap[row.id] === 'pause' || row.state === 'pause') {
            lastDisplay = '<span class="pause-badge">Pause</span>';
          }
        }

        let bestDisplay = row.bestLap > 0 ? formatLapTime(row.bestLap) : '---';
        let bestClass = 'lap-time-green';
        if (activeCarsWithLaps > 1 && minBestLap > 0 && row.bestLap > 0 && row.bestLap <= minBestLap) {
          bestClass = 'lap-time-purple';
        }

        let formatSectorCell = (sectorNum, szVal, splitVal) => {
          let valDisp = szVal > 0 ? formatLapTime(szVal) : '---';
          if (currentMode === 4) {
            let splitDisp = splitVal > 0 ? formatLapTime(splitVal) : '---';
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
      else if (currentMode === 5) dMode.innerText = "GEISTERJAGD 👻";
    }

    let dLeaderLap = document.getElementById('dimmerLeaderLapText');
    if (dLeaderLap) dLeaderLap.innerText = leaderLaps || 0;

    let dSessionBest = document.getElementById('dimmerSessionBestText') || document.getElementById('dimmerOverallText');
    if (dSessionBest) dSessionBest.innerText = (minBestLap > 0) ? formatLapTime(minBestLap, '--:--') : '--:--';

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
    isInitialDataLoaded = true;
    if (pendingUpdateRequested) {
      let doFull = pendingUpdateFullSync;
      pendingUpdateRequested = false;
      pendingUpdateFullSync = false;
      let isDriver = (userRole === 'driver');
      let jitter = isDriver ? Math.floor(Math.random() * 800) : Math.floor(Math.random() * 300);
      let delay = Math.max(50, MIN_UPDATE_INTERVAL_MS - (Date.now() - lastUpdateDataTime)) + jitter;
      updateDataTimer = setTimeout(() => {
        updateDataTimer = null;
        updateData(doFull);
      }, delay);
    }
  }

  if (!fullSync) return;

  try {
    let r = await fetchWithTimeout('/api/timeleft', {}, 4000);
    if (r.ok) {
      let info = await r.json();

      currentMode = info.mode;
      serverMillisOffset = info.serverMillis || 0;
      if (info.targetLaps !== undefined && info.targetLaps > 0) {
        targetLaps = info.targetLaps;
      }
      updateTrackRecordBar(info);

      let modeText = "Freies Training";
      if (currentMode === 1) modeText = qualiFinishedHold ? "Qualifying (Beendet)" : "Qualifying";
      if (currentMode === 2) modeText = "Rennen (Runden)";
      if (currentMode === 3) modeText = "Rennen (Zeit)";
      if (currentMode === 4) modeText = "Dragrace";
      if (currentMode === 5) modeText = "Geisterjagd 👻";
      let modeElem = document.getElementById('modeDisplay');
      if (modeElem) modeElem.innerText = modeText;

      if (currentMode === 5 && info.ghostLap !== undefined) {
        currentGhostTarget = {
          type: info.ghostType || 0,
          lap: info.ghostLap || 0,
          s1: info.ghostS1 || 0,
          s2: info.ghostS2 || 0,
          s3: info.ghostS3 || 0,
          s4: info.ghostS4 || 0,
          szMask: info.ghostSzMask || 0,
          driver: info.ghostDriver || "Geist",
          car: info.ghostCar || ""
        };
        renderGhostHud();
      } else if (currentMode !== 5) {
        let ghHud = document.getElementById('ghostHudContainer');
        if (ghHud) ghHud.style.display = 'none';
        let ghBanner = document.getElementById('ghostBeatenBanner');
        if (ghBanner) ghBanner.style.display = 'none';
      }

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
    let resStations = await fetchWithTimeout('/api/stations', {}, 4000);
    if (resStations.ok) {
      let stations = await resStations.json();
      let sz1 = document.getElementById('sz1Status');
      let sz2 = document.getElementById('sz2Status');
      let sz3 = document.getElementById('sz3Status');
      
      let getStatusColor = (online, batLow, isUnicast) => {
        if (!online) return 'var(--neon-red)';
        if (batLow) return 'var(--neon-orange)';
        if (isUnicast) return '#d946ef'; // Neon-Lila für Unicast-Lock mit Hardware-ACK
        return 'var(--neon-green)';
      };

      if (sz1) {
        sz1.style.color = getStatusColor(stations.sz1, stations.sz1Bat, stations.sz1Uni);
        sz1.title = !stations.sz1 ? 'Offline' : (stations.sz1Uni ? 'Unicast aktiv (Hardware-ACK)' : 'Broadcast/Relay');
      }
      if (sz2) {
        sz2.style.color = getStatusColor(stations.sz2, stations.sz2Bat, stations.sz2Uni);
        sz2.title = !stations.sz2 ? 'Offline' : (stations.sz2Uni ? 'Unicast aktiv (Hardware-ACK)' : 'Broadcast/Relay');
      }
      if (sz3) {
        sz3.style.color = getStatusColor(stations.sz3, stations.sz3Bat, stations.sz3Uni);
        sz3.title = !stations.sz3 ? 'Offline' : (stations.sz3Uni ? 'Unicast aktiv (Hardware-ACK)' : 'Broadcast/Relay');
      }

    }
  } catch (err) {}

  if (userRole === 'admin') {
    try {
      let r = await fetchWithTimeout('/api/recent', {}, 4000);
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
      let poleTimeDisp = formatLapTime(pole.bestLap);
      let poleTimeSpeech = typeof formatTimeForTTS === 'function' ? formatTimeForTTS(pole.bestLap) : (pole.bestLap / 1000).toFixed(2).replace('.', ',');

      let banner = document.getElementById('poleBanner');
      let bannerTxt = document.getElementById('poleBannerText');
      if (banner && bannerTxt) {
        bannerTxt.innerHTML = `${poleDriver} <small style="color:var(--neon-cyan);">(${poleCar})</small> — <span style="color:var(--neon-green);">${poleTimeDisp}</span>`;
        banner.style.display = 'block';
      }

      let speechText = `Qualifying beendet! Herzlichen Glückwunsch ${poleDriver} auf ${poleCar} zur Pole Position mit ${poleTimeSpeech}!`;

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
  } else if (currentMode === 2 || currentMode === 3 || currentMode === 4) {
    if (timerElem) timerElem.innerText = "ZIEL";
    if (typeof announceRaceResults === 'function') announceRaceResults();
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
    let mNewTrack = document.getElementById('newTrackModal');
    let mStorageFull = document.getElementById('storageFullModal');
    let mTrackRecords = document.getElementById('trackRecordsModal');
    
    let isModalOpen = (mConfig && mConfig.style.display === 'block') ||
                      (mDriverConfig && mDriverConfig.style.display === 'block') ||
                      (mStatsOverview && mStatsOverview.style.display === 'block') ||
                      (mStatsDriver && mStatsDriver.style.display === 'block') ||
                      (mOtaAlert && mOtaAlert.style.display === 'block') ||
                      (mReset && mReset.style.display === 'block') ||
                      (mRole && mRole.style.display === 'block') ||
                      (mNewTrack && mNewTrack.style.display === 'block') ||
                      (mStorageFull && mStorageFull.style.display === 'block') ||
                      (mTrackRecords && mTrackRecords.style.display === 'block');
                      
    if (!isModalOpen) {
      await updateData(true);
    }
  } catch (e) {
  } finally {
    isPolling = false;
    clearTimeout(pollTimeout);
    let wsConnected = (socket && socket.readyState === WebSocket.OPEN);
    let baseInterval = wsConnected ? (userRole === 'driver' ? 12000 : 6500) : 6000;
    let jitter = Math.floor(Math.random() * 3500); 
    pollTimeout = setTimeout(pollData, baseInterval + jitter);
  }
}

function handleBaseCrossEvent(msg) {
  let carId = msg.carId;
  let driverName = msg.driver || (carNamesMap[carId]?.driver) || ("Fahrer " + carId);
  let now = Date.now();
  
  if (statusCarsMap[carId] === 'box') {
    if (isCarAllowedForTTS(carId)) speak(driverName + " verlässt die Box");
  } else if (statusCarsMap[carId] === 'pause') {
    if (isCarAllowedForTTS(carId)) speak(driverName + " wieder gestartet");
  } else {
    if (currentMode !== 4 && isCarAllowedForTTS(carId)) {
      if (!lastStartAnnouncedTs[carId] || (now - lastStartAnnouncedTs[carId] > 8000)) {
        lastStartAnnouncedTs[carId] = now;
        speak(driverName + " gestartet!");
      }
    }
  }

  awaitingBaseCrossMap[carId] = false;
  statusCarsMap[carId] = null;
  requestUpdateData(false);
}

function handleSectorTrackEntryTTS(carId) {
  if (currentMode === 4) return;

  let driverName = carNamesMap[carId]?.driver || ("Fahrer " + carId);
  if (awaitingBaseCrossMap[carId]) return;

  if (previousLaps[carId] === undefined || previousLaps[carId] === 0) {
    awaitingBaseCrossMap[carId] = true;
    let existingCar = lastLapsData.find(c => c.id === carId);
    let knownLaps = existingCar ? existingCar.completedLaps : 0;
    previousLaps[carId] = knownLaps;
    previousBestLaps[carId] = existingCar ? (existingCar.bestLap || 0) : 0;
    lastAnnouncedLapMap[carId] = knownLaps;
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
    lastAnnouncedLapMap[carId] = currentLaps;
    lapHistoryMap[carId] = [];
    awaitingBaseCrossMap[carId] = false;
    if (isInitialDataLoaded && currentMode !== 4 && isCarAllowedForTTS(carId)) {
      let now = Date.now();
      if (!lastStartAnnouncedTs[carId] || (now - lastStartAnnouncedTs[carId] > 8000)) {
        lastStartAnnouncedTs[carId] = now;
        speak(driverName + " gestartet!");
      }
    }
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
      if (typeof announceRaceResults === 'function') announceRaceResults();
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
            // Sprachausgabe erfolgt live in Echtzeit über WebSocket
          }
        }
      }
    }

    let recFlags = pendingRecordFlagsMap[carId];
    if (recFlags) {
      delete pendingRecordFlagsMap[carId];
    }
    let isOutLapDone = !!(car.lastLapIsOutLap || (recFlags && recFlags.isOutLap));
    let isNewTrackRecord = !!(recFlags && recFlags.isTrackRecord);
    let isNewDailyRecord = !!(recFlags && recFlags.isDailyRecord);

    let isPersonalBest = false;
    let isAbsoluteBest = false;

    if (minBestLap > 0 && (sessionBestLap === 0 || minBestLap < sessionBestLap)) sessionBestLap = minBestLap;

    let deltaMs = null;
    if (completedLapNum >= 1 && lastLapMs > 0 && !isOutLapDone) {
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
    } else if (isOutLapDone) {
      lastLapDeltaMap[carId] = {
        diffMs: null,
        isPb: false,
        isAbsolute: false
      };
    }

    // Sprachausgabe für Rundenzeiten & Outlaps erfolgt zu 100% in Echtzeit über WebSocket
    if (false) {

      if (isOutLapDone) {
        speak(driverName + ", Outlap beendet!");
      } else if (car.lastLap > 0) {
        let timeSec = typeof formatTimeForTTS === 'function' ? formatTimeForTTS(car.lastLap) : (car.lastLap / 1000).toFixed(2).replace('.', ',');
        let posText = ", ";
        if (currentMode !== 0 && position > 0) posText = ", Platz " + position + ", ";

        let lapAnnouncement = "";
        let isRaceMode = (currentMode >= 2);
        let bestLapWord = isRaceMode ? "schnellste Rennrunde! " : "schnellste Runde! ";

        if (isNewTrackRecord) {
          lapAnnouncement = driverName + posText + "neuer Streckenrekord! " + timeSec;
        } else if (isNewDailyRecord) {
          lapAnnouncement = driverName + posText + "neue Tagesbestzeit! " + timeSec;
        } else if (isRaceMode && !announceLapTimesInRace) {
          if (isAbsoluteBest) lapAnnouncement = driverName + posText + bestLapWord + timeSec;
          else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
          else lapAnnouncement = driverName + posText;
        } else {
          if (isAbsoluteBest) lapAnnouncement = driverName + posText + bestLapWord + timeSec;
          else if (isPersonalBest) lapAnnouncement = driverName + posText + "persönliche Bestzeit! " + timeSec;
          else lapAnnouncement = driverName + posText + timeSec;
        }
        speak(lapAnnouncement);
      }
    }
  }
}

window.addEventListener('DOMContentLoaded', async () => {
  window.sessionStartTs = Math.floor(Date.now() / 1000);

  // Rolle verbindlich anhand der aufgerufenen Seite festlegen
  let isAdminPage = window.location.pathname.toLowerCase().includes('admin');
  userRole = isAdminPage ? 'admin' : 'driver';
  localStorage.setItem('rctl_user_role', userRole);
  applyUserRoleUI(userRole);

  startLocalTimerTicker();

  // Entzerrter Ladevorgang verhindert TCP-Pufferstau und Heap-Kollaps beim Seitenaufruf
  try {
    await loadVersion();
    await new Promise(r => setTimeout(r, 50));
    await syncTimeWithServerOnce();
    await loadTracks();
    await new Promise(r => setTimeout(r, 50));
    await loadSectorConfigInitial();
    await new Promise(r => setTimeout(r, 50));
    await loadCarNames();
    updateTTSFilterDisplay();
    populateDriverQuickSelect();
    await new Promise(r => setTimeout(r, 50));
    await loadWifiStatus();
  } catch (e) {
    console.warn("[Init] Gestaffelter Ladevorgang:", e);
  }

  // WebSocket starten, sobald Basisdaten bereitstehen (mit Jitter 200-600ms)
  setTimeout(() => {
    initWebSocket();
  }, 200 + Math.floor(Math.random() * 400));

  // Erstes Polling erst nach 800ms starten
  setTimeout(() => {
    pollData();
  }, 800);
});

window.onclick = function(event) {
  if (event.target === document.getElementById('universalModal')) closeUniversalDialog(false);
  if (event.target === document.getElementById('adminConfirmModal')) closeAdminConfirmModal();
  if (event.target === document.getElementById('configModal')) closeConfigModal();
  if (event.target === document.getElementById('driverConfigModal')) closeDriverConfigModal();
  if (event.target === document.getElementById('statsModal')) closeStatsModal();
  if (event.target === document.getElementById('statsOverviewModal')) closeStatsOverviewModal();
  if (event.target === document.getElementById('resetConfirmModal')) closeResetModal();
  if (event.target === document.getElementById('newTrackModal')) closeNewTrackModal();
  if (event.target === document.getElementById('storageFullModal')) closeStorageFullModal();
  if (event.target === document.getElementById('trackRecordsModal')) closeTrackRecordsModal();
  if (event.target === document.getElementById('arcadeModal')) closeArcadeModal();
  if (event.target === document.getElementById('roleSelectModal') && userRole) {
    document.getElementById('roleSelectModal').style.display = 'none';
  }
  if (event.target === document.getElementById('otaAlertModal')) {
    if (typeof closeOtaAlert === 'function') closeOtaAlert();
  }
};

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') {
    closeUniversalDialog(false);
    closeAdminConfirmModal();
    closeConfigModal();
    closeDriverConfigModal();
    closeStatsModal();
    closeStatsOverviewModal();
    closeResetModal();
    closeNewTrackModal();
    closeStorageFullModal();
    closeTrackRecordsModal();
    closeArcadeModal();
    if (userRole && document.getElementById('roleSelectModal')) {
      document.getElementById('roleSelectModal').style.display = 'none';
    }
    if (typeof closeOtaAlert === 'function') closeOtaAlert();
  }
});

document.addEventListener('click', function() {
  if (!audioUnmuted) unmuteIOSAudio();
}, { once: true });
