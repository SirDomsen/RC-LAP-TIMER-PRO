window.offlineStatsMode = true;
window.MAX_CARS = 15;
window.carNamesMap = {};
window.allTracksList = [];
window.activeTrackId = 'standard';
window.activeTrackName = 'Standard';
window.currentTab = 'drivers';
window.userRole = 'driver';
window.activeUseSz1 = true;
window.activeUseSz2 = true;
window.activeUseSz3 = true;

const OFFLINE_STATS_VERSION = '0.9.5';
const OFFLINE_DB_NAME = 'rc-lap-timer-offline-stats';
const OFFLINE_DB_VERSION = 1;
const MAX_ZIP_ENTRIES = 10000;
const MAX_STAT_FILE_SIZE = 64 * 1024 * 1024;
const MAX_TOTAL_STAT_SIZE = 512 * 1024 * 1024;
const LAPTIMER_FETCH_TIMEOUT_MS = 8000;
const LAPTIMER_STEP_DELAY_MS = 120;
const DEFAULT_LAPTIMER_HOST = 'laptimer.local';

let offlineDb = null;
let offlineBackup = null;
let offlineInitialization = null;
let lastSyncedLaptimerUrl = null;
let linkedSaveFileHandle = null;

function setOfflineStatus(message, state = 'info') {
  let status = document.getElementById('offlineStatus');
  if (!status) return;
  status.textContent = message;
  status.dataset.state = state;
}

function normalizeStoredPath(path) {
  let normalized = String(path || '').replace(/\\/g, '/').replace(/^\/+/, '');
  let parts = normalized.split('/');
  if (!normalized || parts.some(part => !part || part === '.' || part === '..' || part.includes(':'))) {
    return null;
  }
  return normalized;
}

function isStatisticPath(path) {
  let normalized = normalizeStoredPath(path);
  if (!normalized || normalized.startsWith('__MACOSX/') || normalized.includes('/.')) return false;
  if (['tracks.json', 'sessions.json', 'names.json', 'daily_best.json'].includes(normalized)) return true;
  return normalized.toLowerCase().endsWith('.json') && normalized.split('/').length >= 3;
}

function getOfflineTrackDocument(backup) {
  let trackData = backup.tracks;
  if (Array.isArray(trackData)) {
    return { active: trackData[0]?.id || 'standard', tracks: trackData };
  }
  if (trackData && Array.isArray(trackData.tracks)) return trackData;
  throw new Error('Das Backup enthält keine gültige Streckenliste.');
}

function getSessionFileEntry(path) {
  if (!offlineBackup || !path) return null;
  let files = offlineBackup.sessionFiles;
  if (Object.prototype.hasOwnProperty.call(files, path)) return files[path];

  let normalizedPath = normalizeStoredPath(path);
  let match = Object.keys(files).find(key => normalizeStoredPath(key) === normalizedPath);
  return match ? files[match] : null;
}

function populateOfflineTrackFilter() {
  let select = document.getElementById('statsTrackFilterSelect');
  if (!select) return;

  let previousValue = select.value || 'all';
  let options = ['<option value="all">🌍 Alle Strecken</option>'];
  window.allTracksList.forEach(track => {
    options.push(`<option value="${escapeHtml(track.id)}">📍 ${escapeHtml(track.name || track.id)}</option>`);
  });
  select.innerHTML = options.join('');
  select.value = window.allTracksList.some(track => track.id === previousValue) ? previousValue : 'all';
}

window.loadCarNames = async function() {
  let names = {};
  (offlineBackup.sessions || []).forEach(session => {
    (session.results || []).forEach(result => {
      if (!result.id || names[result.id]) return;
      names[result.id] = {
        driver: result.driver || '',
        car: result.car || ''
      };
    });
  });

  if (offlineBackup.names && typeof offlineBackup.names === 'object') {
    Object.keys(offlineBackup.names).forEach(id => {
      names[id] = { ...names[id], ...offlineBackup.names[id] };
    });
  }
  window.carNamesMap = names;
};

window.loadTracks = async function() {
  let trackDocument = getOfflineTrackDocument(offlineBackup);
  window.allTracksList = trackDocument.tracks;
  window.activeTrackId = trackDocument.active || trackDocument.tracks[0]?.id || 'standard';
  let activeTrack = trackDocument.tracks.find(track => track.id === window.activeTrackId) || trackDocument.tracks[0];
  window.activeTrackName = activeTrack ? activeTrack.name : 'Standard';
  populateOfflineTrackFilter();
};

window.fetchSessionsAndStorageInfo = async function() {
  allSessionsCache = offlineBackup.sessions;
  updateStatsModalDailyBest();
};

window.fetchSessionFileData = async function(filePath) {
  if (!filePath) return null;
  if (sessionFileCache[filePath]) return sessionFileCache[filePath];

  let data = getSessionFileEntry(filePath);
  if (!data || typeof data !== 'object') {
    throw new Error('Session-Datei fehlt in der lokalen Statistikdatenbank: ' + filePath);
  }
  let normalized = normalizeSessionFile(data, filePath);
  let session = offlineBackup.sessions.find(item => item.file === filePath);
  normalized.laps = resolveSessionLapNames(normalized.laps, session);
  sessionFileCache[filePath] = normalized;
  return normalized;
};

window.getCurrentLocalDateStr = function() {
  let now = new Date();
  let month = String(now.getMonth() + 1).padStart(2, '0');
  let day = String(now.getDate()).padStart(2, '0');
  return `${now.getFullYear()}-${month}-${day}`;
};

window.findSessionForRecord = function(record, trackId) {
  if (!Array.isArray(allSessionsCache) || !record || !record.lap || record.lap <= 0) return null;
  let targetTrackId = trackId || record.trackId || window.activeTrackId || 'standard';
  let match = allSessionsCache.find(session => {
    if ((session.trackId || 'standard') !== targetTrackId) return false;
    return (session.results || []).some(result => {
      let carMatch = !record.carId || result.id === record.carId;
      let driverMatch = !record.driver || !result.driver ||
        result.driver.toLowerCase() === record.driver.toLowerCase();
      let timeMatch = result.bestLap === record.lap || Math.abs(result.bestLap - record.lap) <= 5;
      return (carMatch || driverMatch) && timeMatch;
    });
  });
  if (match) return match;

  return allSessionsCache.find(session => (session.results || []).some(result => {
    let carMatch = !record.carId || result.id === record.carId;
    let driverMatch = !record.driver || !result.driver ||
      result.driver.toLowerCase() === record.driver.toLowerCase();
    let timeMatch = result.bestLap === record.lap || Math.abs(result.bestLap - record.lap) <= 5;
    return (carMatch || driverMatch) && timeMatch;
  })) || null;
};

function openOfflineDatabase() {
  return new Promise((resolve, reject) => {
    if (!window.indexedDB) {
      reject(new Error('Dieser Browser unterstützt keine lokale IndexedDB.'));
      return;
    }

    let request = indexedDB.open(OFFLINE_DB_NAME, OFFLINE_DB_VERSION);
    request.onupgradeneeded = () => {
      let db = request.result;
      if (!db.objectStoreNames.contains('files')) db.createObjectStore('files', { keyPath: 'path' });
      if (!db.objectStoreNames.contains('state')) db.createObjectStore('state', { keyPath: 'key' });
      if (!db.objectStoreNames.contains('settings')) db.createObjectStore('settings', { keyPath: 'key' });
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error || new Error('Lokale Statistikdatenbank konnte nicht geöffnet werden.'));
    request.onblocked = () => reject(new Error('Die lokale Statistikdatenbank ist in einem anderen Tab geöffnet.'));
  });
}

function readDatabaseSnapshot() {
  return new Promise((resolve, reject) => {
    let transaction = offlineDb.transaction(['files', 'state'], 'readonly');
    let filesRequest = transaction.objectStore('files').getAll();
    let stateRequest = transaction.objectStore('state').get('dataset');
    let files;
    let state;
    filesRequest.onsuccess = () => { files = filesRequest.result; };
    stateRequest.onsuccess = () => { state = stateRequest.result?.value || null; };
    transaction.oncomplete = () => resolve({ files: files || [], state });
    transaction.onerror = () => reject(transaction.error || new Error('Lokale Datenbank konnte nicht gelesen werden.'));
    transaction.onabort = () => reject(transaction.error || new Error('Lesen der lokalen Datenbank wurde abgebrochen.'));
  });
}

function persistImport(entries, dataset) {
  return new Promise((resolve, reject) => {
    let transaction = offlineDb.transaction(['files', 'state'], 'readwrite');
    let fileStore = transaction.objectStore('files');
    entries.forEach(entry => fileStore.put(entry));
    transaction.objectStore('state').put({ key: 'dataset', value: dataset });
    transaction.oncomplete = resolve;
    transaction.onerror = () => reject(transaction.error || new Error('Import konnte nicht lokal gespeichert werden.'));
    transaction.onabort = () => reject(transaction.error || new Error('Import in die lokale Datenbank wurde abgebrochen.'));
  });
}

function readOfflineSetting(key) {
  return new Promise(resolve => {
    if (!offlineDb) {
      resolve(null);
      return;
    }
    let transaction = offlineDb.transaction(['settings'], 'readonly');
    let request = transaction.objectStore('settings').get(key);
    request.onsuccess = () => resolve(request.result ? request.result.value : null);
    request.onerror = () => resolve(null);
  });
}

function writeOfflineSetting(key, value) {
  return new Promise(resolve => {
    if (!offlineDb) {
      resolve();
      return;
    }
    let transaction = offlineDb.transaction(['settings'], 'readwrite');
    transaction.objectStore('settings').put({ key, value });
    transaction.oncomplete = () => resolve();
    transaction.onerror = () => resolve();
  });
}

function getSessionKey(session) {
  return normalizeStoredPath(session.file) || String(session.id || '');
}

function mergeSessions(existing, incoming) {
  let sessions = new Map();
  [...(existing || []), ...(incoming || [])].forEach(session => {
    let key = getSessionKey(session);
    if (key) sessions.set(key, session);
  });
  return Array.from(sessions.values()).sort(
    (a, b) => (b.endTs || b.startTs || 0) - (a.endTs || a.startTs || 0)
  );
}

function mergeTrackRecords(oldRecords, newRecords) {
  let records = new Map();
  [...(oldRecords || []), ...(newRecords || [])].forEach(record => {
    if (!record || !(record.lap > 0)) return;
    let key = [
      record.lap,
      record.carId || '',
      (record.driver || '').toLowerCase(),
      record.date || ''
    ].join('|');
    records.set(key, record);
  });
  return Array.from(records.values()).sort((a, b) => a.lap - b.lap).slice(0, 5);
}

function mergeTrackDocuments(existing, incoming) {
  let tracks = new Map();
  (existing?.tracks || []).forEach(track => tracks.set(track.id, track));
  (incoming.tracks || []).forEach(track => {
    let previous = tracks.get(track.id);
    if (!previous) {
      tracks.set(track.id, track);
      return;
    }

    let mergedTrack = {
      ...previous,
      ...track,
      allTimeRecords: mergeTrackRecords(previous.allTimeRecords, track.allTimeRecords)
    };
    let bestAllTimeRecord = mergedTrack.allTimeRecords[0];
    if (bestAllTimeRecord && (!mergedTrack.recordLap || bestAllTimeRecord.lap <= mergedTrack.recordLap)) {
      mergedTrack.recordLap = bestAllTimeRecord.lap;
      mergedTrack.recordDriver = bestAllTimeRecord.driver || mergedTrack.recordDriver;
      mergedTrack.recordCar = bestAllTimeRecord.car || mergedTrack.recordCar;
      mergedTrack.recordCarId = bestAllTimeRecord.carId || mergedTrack.recordCarId;
      mergedTrack.recordDate = bestAllTimeRecord.date || mergedTrack.recordDate;
    }
    tracks.set(track.id, mergedTrack);
  });

  return {
    active: incoming.active || existing?.active || Array.from(tracks.keys())[0] || 'standard',
    tracks: Array.from(tracks.values())
  };
}

function parseJsonEntry(path, bytes) {
  let content;
  try {
    content = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes));
  } catch (error) {
    throw new Error(`Ungültige JSON-Datei "${path}": ${error.message}`);
  }
  return { path, content, size: bytes.byteLength, modifiedAt: Date.now() };
}

function getCrc32(bytes) {
  let crc = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) {
    crc ^= bytes[i];
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc & 1) ? (0xedb88320 ^ (crc >>> 1)) : (crc >>> 1);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function getDosTimestamp(date) {
  let year = Math.max(1980, date.getFullYear());
  return {
    date: ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate(),
    time: (date.getHours() << 11) | (date.getMinutes() << 5) | (date.getSeconds() >> 1)
  };
}

function createStatisticsZip(fileMap, dataset) {
  let files = new Map(fileMap);
  files.set('tracks.json', { path: 'tracks.json', content: dataset.tracks });
  files.set('names.json', { path: 'names.json', content: dataset.names });
  files.set('sessions.json', { path: 'sessions.json', content: { sessions: dataset.sessions } });

  let localEntries = [];
  let centralEntries = [];
  let offset = 0;
  let totalSize = 0;
  let timestamp = getDosTimestamp(new Date());

  for (let [rawPath, entry] of files) {
    let path = normalizeStoredPath(rawPath);
    if (!path || !isStatisticPath(path)) continue;
    let nameBytes = new TextEncoder().encode(path);
    let dataBytes = new TextEncoder().encode(JSON.stringify(entry.content));
    if (nameBytes.length > 0xffff || dataBytes.length > MAX_STAT_FILE_SIZE) {
      throw new Error(`Datei kann nicht in das ZIP geschrieben werden: ${path}`);
    }
    totalSize += dataBytes.length;
    if (totalSize > MAX_TOTAL_STAT_SIZE || localEntries.length / 2 >= MAX_ZIP_ENTRIES) {
      throw new Error('Die kumulierte Statistikdatenbank überschreitet die ZIP-Grenzwerte.');
    }

    let crc = getCrc32(dataBytes);
    let localHeader = new Uint8Array(30 + nameBytes.length);
    let localView = new DataView(localHeader.buffer);
    localView.setUint32(0, 0x04034b50, true);
    localView.setUint16(4, 20, true);
    localView.setUint16(6, 0x0800, true);
    localView.setUint16(8, 0, true);
    localView.setUint16(10, timestamp.time, true);
    localView.setUint16(12, timestamp.date, true);
    localView.setUint32(14, crc, true);
    localView.setUint32(18, dataBytes.length, true);
    localView.setUint32(22, dataBytes.length, true);
    localView.setUint16(26, nameBytes.length, true);
    localView.setUint16(28, 0, true);
    localHeader.set(nameBytes, 30);
    localEntries.push(localHeader, dataBytes);

    let centralHeader = new Uint8Array(46 + nameBytes.length);
    let centralView = new DataView(centralHeader.buffer);
    centralView.setUint32(0, 0x02014b50, true);
    centralView.setUint16(4, 20, true);
    centralView.setUint16(6, 20, true);
    centralView.setUint16(8, 0x0800, true);
    centralView.setUint16(10, 0, true);
    centralView.setUint16(12, timestamp.time, true);
    centralView.setUint16(14, timestamp.date, true);
    centralView.setUint32(16, crc, true);
    centralView.setUint32(20, dataBytes.length, true);
    centralView.setUint32(24, dataBytes.length, true);
    centralView.setUint16(28, nameBytes.length, true);
    centralView.setUint16(30, 0, true);
    centralView.setUint16(32, 0, true);
    centralView.setUint16(34, 0, true);
    centralView.setUint16(36, 0, true);
    centralView.setUint32(38, 0, true);
    centralView.setUint32(42, offset, true);
    centralHeader.set(nameBytes, 46);
    centralEntries.push(centralHeader);

    offset += localHeader.length + dataBytes.length;
  }

  let entryCount = localEntries.length / 2;
  if (entryCount > 0xffff || offset > 0xffffffff) {
    throw new Error('Die kumulierte Statistikdatenbank ist zu groß für ein ZIP-Archiv.');
  }
  let centralDirectorySize = centralEntries.reduce((size, entry) => size + entry.length, 0);
  let endRecord = new Uint8Array(22);
  let endView = new DataView(endRecord.buffer);
  endView.setUint32(0, 0x06054b50, true);
  endView.setUint16(4, 0, true);
  endView.setUint16(6, 0, true);
  endView.setUint16(8, entryCount, true);
  endView.setUint16(10, entryCount, true);
  endView.setUint32(12, centralDirectorySize, true);
  endView.setUint32(16, offset, true);
  endView.setUint16(20, 0, true);

  return new Blob([...localEntries, ...centralEntries, endRecord], { type: 'application/zip' });
}

async function readZipArchive(buffer) {
  let view = new DataView(buffer);
  let eocdOffset = -1;
  let minOffset = Math.max(0, buffer.byteLength - 65557);
  for (let i = buffer.byteLength - 22; i >= minOffset; i--) {
    if (view.getUint32(i, true) === 0x06054b50) {
      eocdOffset = i;
      break;
    }
  }
  if (eocdOffset < 0) throw new Error('Ungültiges ZIP-Archiv (End-of-Central-Directory fehlt).');
  if (view.getUint16(eocdOffset + 4, true) || view.getUint16(eocdOffset + 6, true)) {
    throw new Error('Mehrteilige ZIP-Archive werden nicht unterstützt.');
  }

  let totalEntries = view.getUint16(eocdOffset + 10, true);
  let centralDirectorySize = view.getUint32(eocdOffset + 12, true);
  let centralDirectoryOffset = view.getUint32(eocdOffset + 16, true);
  if (totalEntries > MAX_ZIP_ENTRIES) throw new Error('Das ZIP-Archiv enthält zu viele Einträge.');
  if (centralDirectoryOffset + centralDirectorySize > eocdOffset) {
    throw new Error('Das ZIP-Archiv enthält ein ungültiges Dateiverzeichnis.');
  }

  let offset = centralDirectoryOffset;
  let decoder = new TextDecoder('utf-8');
  let files = [];
  let totalStatSize = 0;

  for (let i = 0; i < totalEntries; i++) {
    if (offset + 46 > eocdOffset || view.getUint32(offset, true) !== 0x02014b50) {
      throw new Error('Ungültiger Eintrag im ZIP-Dateiverzeichnis.');
    }

    let flags = view.getUint16(offset + 8, true);
    let method = view.getUint16(offset + 10, true);
    let compressedSize = view.getUint32(offset + 20, true);
    let uncompressedSize = view.getUint32(offset + 24, true);
    let nameLength = view.getUint16(offset + 28, true);
    let extraLength = view.getUint16(offset + 30, true);
    let commentLength = view.getUint16(offset + 32, true);
    let localHeaderOffset = view.getUint32(offset + 42, true);
    let entryEnd = offset + 46 + nameLength + extraLength + commentLength;
    if (entryEnd > eocdOffset) throw new Error('Abgeschnittener Eintrag im ZIP-Dateiverzeichnis.');

    let nameBytes = new Uint8Array(buffer, offset + 46, nameLength);
    let fileName = decoder.decode(nameBytes).replace(/\\/g, '/');
    offset = entryEnd;
    if (fileName.endsWith('/') || !isStatisticPath(fileName)) continue;
    let path = normalizeStoredPath(fileName);
    if (!path) continue;
    if (flags & 1) throw new Error(`Verschlüsselte ZIP-Datei wird nicht unterstützt: ${path}`);
    if (uncompressedSize > MAX_STAT_FILE_SIZE) throw new Error(`Datei ist zu groß: ${path}`);
    totalStatSize += uncompressedSize;
    if (totalStatSize > MAX_TOTAL_STAT_SIZE) {
      throw new Error('Die Statistikdateien im ZIP überschreiten die zulässige Gesamtgröße.');
    }
    if (localHeaderOffset + 30 > buffer.byteLength ||
        view.getUint32(localHeaderOffset, true) !== 0x04034b50) {
      throw new Error(`Ungültiger Datei-Header für "${path}".`);
    }

    let localNameLength = view.getUint16(localHeaderOffset + 26, true);
    let localExtraLength = view.getUint16(localHeaderOffset + 28, true);
    let dataOffset = localHeaderOffset + 30 + localNameLength + localExtraLength;
    let dataEnd = dataOffset + compressedSize;
    if (dataEnd > buffer.byteLength) throw new Error(`Abgeschnittener Dateiinhalt: ${path}`);

    let compressedData = new Uint8Array(buffer, dataOffset, compressedSize);
    let data;
    if (method === 0) {
      data = new Uint8Array(compressedData);
    } else if (method === 8) {
      if (typeof DecompressionStream === 'undefined') {
        throw new Error('Dieser Browser kann komprimierte ZIP-Dateien nicht entpacken.');
      }
      let stream = new Blob([compressedData]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
      data = new Uint8Array(await new Response(stream).arrayBuffer());
    } else {
      throw new Error(`Nicht unterstützte ZIP-Kompression (${method}) bei "${path}".`);
    }

    if (data.byteLength !== uncompressedSize) {
      throw new Error(`Die entpackte Dateigröße stimmt nicht: ${path}`);
    }
    files.push(parseJsonEntry(path, data));
  }

  return files;
}

function validateBackup(data) {
  if (!data || typeof data !== 'object') throw new Error('Die Datei enthält kein JSON-Backup.');
  if (data.version !== 3 && data.version !== 4 && data.version !== 5) {
    throw new Error('Nicht unterstützte JSON-Backup-Version. Erwartet wird Version 3, 4 oder 5.');
  }
  if (!Array.isArray(data.sessions)) throw new Error('Im Backup fehlt die Sessions-Liste.');
  if (!data.sessionFiles || typeof data.sessionFiles !== 'object' || Array.isArray(data.sessionFiles)) {
    throw new Error('Im Backup fehlen die Session-Dateien.');
  }
  getOfflineTrackDocument(data);

  let entries = [
    { path: 'tracks.json', content: data.tracks },
    { path: 'sessions.json', content: { sessions: data.sessions } }
  ];
  if (data.names) entries.push({ path: 'names.json', content: data.names });
  Object.entries(data.sessionFiles).forEach(([name, content]) => {
    let path = normalizeStoredPath(name);
    if (path && isStatisticPath(path) && path.split('/').length >= 3) {
      entries.push({ path, content });
    }
  });
  return entries.map(entry => ({
    ...entry,
    size: new Blob([JSON.stringify(entry.content)]).size,
    modifiedAt: Date.now()
  }));
}

function payloadFromZipEntries(entries) {
  let byPath = new Map(entries.map(entry => [entry.path, entry.content]));
  let tracks = byPath.get('tracks.json');
  let sessionsDocument = byPath.get('sessions.json');
  if (!tracks || !sessionsDocument) {
    throw new Error('Das ZIP enthält keine vollständige Statistik-Sicherung (tracks.json und sessions.json fehlen).');
  }

  let trackDocument = Array.isArray(tracks) ? { active: tracks[0]?.id || 'standard', tracks } : tracks;
  if (!trackDocument || !Array.isArray(trackDocument.tracks)) {
    throw new Error('tracks.json im ZIP enthält keine gültige Streckenliste.');
  }
  let sessions = Array.isArray(sessionsDocument) ? sessionsDocument : sessionsDocument.sessions;
  if (!Array.isArray(sessions)) throw new Error('sessions.json im ZIP enthält keine Sessions-Liste.');

  let names = byPath.get('names.json') || {};
  if (!names || typeof names !== 'object' || Array.isArray(names)) {
    throw new Error('names.json im ZIP enthält kein gültiges Fahrernamen-Objekt.');
  }
  let sessionFiles = {};
  entries.forEach(entry => {
    if (entry.path.split('/').length >= 3 && entry.path.toLowerCase().endsWith('.json')) {
      sessionFiles['/' + entry.path] = entry.content;
    }
  });

  return { tracks: trackDocument, sessions, names, sessionFiles };
}

async function readImportFile(file) {
  if (file.name.toLowerCase().endsWith('.zip')) {
    let entries = await readZipArchive(await file.arrayBuffer());
    return { entries, payload: payloadFromZipEntries(entries) };
  }
  if (!file.name.toLowerCase().endsWith('.json')) {
    throw new Error('Bitte ein Lap-Timer-ZIP oder ein Statistik-JSON-Backup auswählen.');
  }

  let backup;
  try {
    backup = JSON.parse(await file.text());
  } catch (error) {
    throw new Error('Die JSON-Datei konnte nicht gelesen werden: ' + error.message);
  }
  let entries = validateBackup(backup);
  let trackDocument = getOfflineTrackDocument(backup);
  return {
    entries,
    payload: {
      tracks: trackDocument,
      sessions: backup.sessions,
      names: backup.names || {},
      sessionFiles: backup.sessionFiles
    }
  };
}

function getStoredFileMap(snapshot) {
  return new Map(snapshot.files.map(file => [file.path, file]));
}

function getDatasetFromSnapshot(snapshot, fileMap) {
  let stored = snapshot.state || {};
  let trackEntry = fileMap.get('tracks.json');
  let namesEntry = fileMap.get('names.json');
  let sessionsEntry = fileMap.get('sessions.json');
  return {
    tracks: stored.tracks || (trackEntry && trackEntry.content) || null,
    names: stored.names || (namesEntry && namesEntry.content) || {},
    sessions: stored.sessions || (sessionsEntry && (sessionsEntry.content.sessions || sessionsEntry.content)) || []
  };
}

function buildOfflineBackup(dataset, fileMap) {
  let sessionFiles = {};
  fileMap.forEach((entry, path) => {
    if (path.split('/').length >= 3 && path.toLowerCase().endsWith('.json')) {
      sessionFiles['/' + path] = entry.content;
    }
  });
  return {
    version: 5,
    exportedAt: Math.floor(Date.now() / 1000),
    tracks: dataset.tracks,
    names: dataset.names,
    sessions: dataset.sessions,
    sessionFiles
  };
}

async function importIntoLocalDatabase(payload, entries) {
  let snapshot = await readDatabaseSnapshot();
  let fileMap = getStoredFileMap(snapshot);
  let current = getDatasetFromSnapshot(snapshot, fileMap);
  let entriesToStore = new Map();
  let added = 0;
  let updated = 0;

  entries.forEach(entry => {
    let path = normalizeStoredPath(entry.path);
    if (!path || !isStatisticPath(path)) return;
    if (fileMap.has(path)) updated++;
    else added++;
    let storedEntry = { ...entry, path };
    fileMap.set(path, storedEntry);
    entriesToStore.set(path, storedEntry);
  });

  let tracks = mergeTrackDocuments(
    current.tracks && getOfflineTrackDocument({ tracks: current.tracks }),
    payload.tracks
  );
  let names = { ...current.names, ...payload.names };
  let sessions = mergeSessions(current.sessions, payload.sessions);
  let dataset = { tracks, names, sessions };

  let mergedEntries = [{
    path: 'tracks.json', content: tracks, size: JSON.stringify(tracks).length, modifiedAt: Date.now()
  }, {
    path: 'names.json', content: names, size: JSON.stringify(names).length, modifiedAt: Date.now()
  }, {
    path: 'sessions.json', content: { sessions }, size: JSON.stringify(sessions).length, modifiedAt: Date.now()
  }];
  mergedEntries.forEach(entry => {
    fileMap.set(entry.path, entry);
    entriesToStore.set(entry.path, entry);
  });
  await persistImport(Array.from(entriesToStore.values()), dataset);

  let storedPaths = new Set(fileMap.keys());
  let missingSessionFiles = sessions.filter(session =>
    session.file && !storedPaths.has(normalizeStoredPath(session.file))
  ).length;
  return {
    dataset,
    fileMap,
    added,
    updated,
    missingSessionFiles,
    importedSessionFiles: entries.filter(entry => entry.path.split('/').length >= 3).length
  };
}

async function refreshOfflineViews(dataset, fileMap) {
  offlineBackup = buildOfflineBackup(dataset, fileMap);
  sessionFileCache = {};
  allSessionsCache = offlineBackup.sessions;
  await Promise.all([window.loadCarNames(), window.loadTracks()]);
  document.getElementById('offlineWelcome').hidden = true;
  document.getElementById('statsOverviewModal').style.display = 'block';
  await openStatsOverviewModal();
}

async function loadOfflineFile(file) {
  if (!file) return;
  setOfflineStatus(`Lese ${file.name} …`);

  try {
    if (!await offlineInitialization) return;
    let { payload, entries } = await readImportFile(file);
    let imported = await importIntoLocalDatabase(payload, entries);
    await refreshOfflineViews(imported.dataset, imported.fileMap);
    updateSaveButtonsUI(true);

    let summary = `${imported.dataset.sessions.length} Sessions · ${imported.fileMap.size} Dateien · Offline v${OFFLINE_STATS_VERSION}`;
    document.getElementById('offlineBackupSummary').textContent = summary;
    let status = `${file.name}: ${imported.added} neu, ${imported.updated} aktualisiert; Datenbank jetzt ${imported.dataset.sessions.length} Sessions.`;
    let autoSaveNote = await tryAutoSaveStatusNote();
    if (autoSaveNote) status += autoSaveNote;
    if (imported.missingSessionFiles) {
      status += ` ${imported.missingSessionFiles} Session-Datei(en) für Rundendetails fehlen.`;
    }
    setOfflineStatus(status, imported.missingSessionFiles ? 'error' : 'success');
  } catch (error) {
    setOfflineStatus(error.message || 'Das Backup konnte nicht importiert werden.', 'error');
  }
}

function supportsFileSystemSave() {
  return typeof window.showSaveFilePicker === 'function';
}

function updateSaveButtonsUI(hasData) {
  let saveButton = document.getElementById('exportDatabaseButton');
  let canPick = supportsFileSystemSave();

  if (saveButton) {
    saveButton.disabled = !hasData;
    saveButton.textContent = '💾 Speichern';
    saveButton.title = linkedSaveFileHandle
      ? `Speichert direkt in „${linkedSaveFileHandle.name}“`
      : (canPick ? 'Speicherort für statistics.zip wählen und speichern' : 'statistics.zip herunterladen');
  }
}

async function ensureWritePermission(handle, promptUser) {
  if (!handle || typeof handle.queryPermission !== 'function') return true;
  let opts = { mode: 'readwrite' };
  if ((await handle.queryPermission(opts)) === 'granted') return true;
  if (!promptUser || typeof handle.requestPermission !== 'function') return false;
  return (await handle.requestPermission(opts)) === 'granted';
}

async function writeArchiveToHandle(handle, archive) {
  let writable = await handle.createWritable();
  await writable.write(archive);
  await writable.close();
}

function exportStatisticsDatabase(fileMap, dataset) {
  let archive = createStatisticsZip(fileMap, dataset);
  let url = URL.createObjectURL(archive);
  let link = document.createElement('a');
  let fileName = 'statistics.zip';
  link.href = url;
  link.download = fileName;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  return { fileName, size: archive.size };
}

async function saveDatabaseToFile({ forcePicker = false, autoSaveOnly = false } = {}) {
  let snapshot = await readDatabaseSnapshot();
  let fileMap = getStoredFileMap(snapshot);
  let dataset = getDatasetFromSnapshot(snapshot, fileMap);
  if (!dataset.tracks) {
    throw new Error('Es sind noch keine Statistikdaten zum Speichern vorhanden.');
  }

  if (supportsFileSystemSave()) {
    try {
      if (forcePicker || !linkedSaveFileHandle) {
        if (autoSaveOnly) return { saved: false, reason: 'unlinked' };
        let pickedHandle = await window.showSaveFilePicker({
          suggestedName: linkedSaveFileHandle ? linkedSaveFileHandle.name : 'statistics.zip',
          startIn: linkedSaveFileHandle || 'documents',
          types: [{
            description: 'RC Lap Timer Statistik-ZIP',
            accept: { 'application/zip': ['.zip'] }
          }]
        });
        linkedSaveFileHandle = pickedHandle;
        await writeOfflineSetting('saveFileHandle', pickedHandle);
        updateSaveButtonsUI(true);
      }

      let allowed = await ensureWritePermission(linkedSaveFileHandle, !autoSaveOnly);
      if (!allowed) {
        if (autoSaveOnly) return { saved: false, reason: 'permission' };
        throw new Error('Schreibberechtigung für die Speicherdatei wurde nicht erteilt.');
      }

      let archive = createStatisticsZip(fileMap, dataset);
      try {
        await writeArchiveToHandle(linkedSaveFileHandle, archive);
      } catch (writeError) {
        if (writeError && writeError.name === 'NotFoundError') {
          linkedSaveFileHandle = null;
          await writeOfflineSetting('saveFileHandle', null);
          updateSaveButtonsUI(true);
          throw new Error('Die verknüpfte Speicherdatei wurde verschoben oder gelöscht. Bitte erneut auf „💾 Speichern“ klicken.');
        }
        throw writeError;
      }

      return { saved: true, method: 'handle', fileName: linkedSaveFileHandle.name, size: archive.size };
    } catch (error) {
      if (error && error.name === 'AbortError') {
        return { saved: false, reason: 'cancelled' };
      }
      if (autoSaveOnly) {
        return { saved: false, reason: 'error' };
      }
      if (!error || (error.name !== 'SecurityError' && error.name !== 'TypeError')) {
        throw error;
      }
    }
  }

  if (autoSaveOnly) return { saved: false, reason: 'unsupported' };
  let downloaded = exportStatisticsDatabase(fileMap, dataset);
  return { saved: true, method: 'download', fileName: downloaded.fileName, size: downloaded.size };
}

async function tryAutoSaveStatusNote() {
  if (!supportsFileSystemSave()) {
    return ' Tipp: Über „💾 Speichern“ kannst du statistics.zip als Backup herunterladen (direktes Überschreiben unterstützt dieser Browser nicht).';
  }
  if (!linkedSaveFileHandle) {
    return ' Tipp: Über „💾 Speichern“ kannst du eine lokale statistics.zip verknüpfen.';
  }
  let result = await saveDatabaseToFile({ autoSaveOnly: true });
  if (result.saved) {
    return ` Direkt in „${result.fileName}“ gespeichert.`;
  }
  if (result.reason === 'permission') {
    return ` Klicke auf „💾 Speichern“, um „${linkedSaveFileHandle.name}“ zu aktualisieren.`;
  }
  return '';
}

async function downloadStatisticsDatabase({ forcePicker = false } = {}) {
  let button = document.getElementById('exportDatabaseButton');
  if (!offlineDb || !await offlineInitialization) {
    setOfflineStatus('Die lokale Statistikdatenbank ist noch nicht verfügbar.', 'error');
    return;
  }
  if (button) button.disabled = true;
  try {
    let result = await saveDatabaseToFile({ forcePicker });
    if (!result.saved && result.reason === 'cancelled') {
      setOfflineStatus('Speichern abgebrochen.', 'info');
      return;
    }
    if (result.method === 'handle') {
      setOfflineStatus(
        `In „${result.fileName}“ gespeichert (${(result.size / 1024).toFixed(1)} KB).`,
        'success'
      );
    } else {
      let extra = !supportsFileSystemSave()
        ? ' (Dieser Browser unterstützt kein direktes Überschreiben lokaler Dateien.)'
        : '';
      setOfflineStatus(
        `Statistik-ZIP heruntergeladen: ${result.fileName} (${(result.size / 1024).toFixed(1)} KB).${extra}`,
        'success'
      );
    }
  } catch (error) {
    setOfflineStatus(error.message || 'Statistikdatenbank konnte nicht gespeichert werden.', 'error');
  } finally {
    updateSaveButtonsUI(Boolean(offlineBackup && offlineBackup.tracks));
  }
}

async function restoreLocalDatabase() {
  let snapshot = await readDatabaseSnapshot();
  let fileMap = getStoredFileMap(snapshot);
  let dataset = getDatasetFromSnapshot(snapshot, fileMap);
  if (!dataset.tracks) {
    updateSaveButtonsUI(false);
    return;
  }

  await refreshOfflineViews(dataset, fileMap);
  document.getElementById('offlineBackupSummary').textContent =
    `${dataset.sessions.length} Sessions · ${fileMap.size} Dateien · Offline v${OFFLINE_STATS_VERSION}`;
  let status = `Lokale Datenbank geladen: ${dataset.sessions.length} Sessions, ${fileMap.size} Statistikdateien.`;
  if (linkedSaveFileHandle) {
    status += ` Verknüpfte Speicherdatei: „${linkedSaveFileHandle.name}“.`;
  }
  setOfflineStatus(status, 'success');
  updateSaveButtonsUI(true);
}

async function toggleOfflineFullscreen() {
  try {
    if (document.fullscreenElement) {
      await document.exitFullscreen();
    } else if (document.documentElement.requestFullscreen) {
      await document.documentElement.requestFullscreen();
    } else {
      throw new Error('Der Browser unterstützt keinen Vollbildmodus.');
    }
  } catch (error) {
    setOfflineStatus(error.message || 'Vollbild konnte nicht aktiviert werden.', 'error');
  }
}

function updateFullscreenButton() {
  let button = document.getElementById('fullscreenButton');
  if (button) button.textContent = document.fullscreenElement ? '⛶ Vollbild verlassen' : '⛶ Vollbild';
}

function sleepMs(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function normalizeLaptimerBaseUrl(rawInput) {
  let trimmed = String(rawInput || '').trim();
  if (!trimmed) {
    throw new Error('Bitte eine IP-Adresse oder einen Hostnamen (z. B. laptimer.local) eingeben.');
  }
  let withScheme = /^https?:\/\//i.test(trimmed) ? trimmed : `http://${trimmed}`;
  let parsed;
  try {
    parsed = new URL(withScheme);
  } catch (error) {
    throw new Error(`Ungültige Lap-Timer-Adresse: "${trimmed}"`);
  }
  if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
    throw new Error('Es werden nur HTTP- oder HTTPS-Adressen unterstützt.');
  }
  if (window.location.protocol === 'https:' && parsed.protocol === 'http:') {
    throw new Error('Aus einer HTTPS-Seite heraus blockiert der Browser HTTP-Zugriffe auf lokale IPs. Bitte offline_stats/index.html lokal öffnen.');
  }
  return `${parsed.protocol}//${parsed.host}`;
}

async function fetchLaptimerJson(url, attempt = 0) {
  let controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
  let timer = controller ? setTimeout(() => controller.abort(), LAPTIMER_FETCH_TIMEOUT_MS) : null;

  try {
    let response = await fetch(url, {
      method: 'GET',
      mode: 'cors',
      cache: 'no-store',
      signal: controller ? controller.signal : undefined
    });
    if (response.status === 503 && attempt < 1) {
      await sleepMs(1000);
      return fetchLaptimerJson(url, attempt + 1);
    }
    if (!response.ok) {
      let error = new Error(`HTTP ${response.status} bei ${url}`);
      error.httpStatus = response.status;
      throw error;
    }
    return await response.json();
  } catch (error) {
    if (error && error.name === 'AbortError') {
      throw new Error(`Zeitüberschreitung beim Verbinden mit ${url}.`);
    }
    if (error && error.httpStatus) throw error;
    throw new Error(`Verbindung zum Lap Timer fehlgeschlagen (${error.message || 'Netzwerk/CORS'}). Ist der Lap Timer im selben WLAN und hat aktuelle Firmware?`);
  } finally {
    if (timer) clearTimeout(timer);
  }
}

function getSessionSignature(session) {
  if (!session || typeof session !== 'object') return '';
  let resultsSig = (session.results || [])
    .map(r => `${r.id || ''}:${r.laps || 0}:${r.bestLap || 0}`)
    .join(',');
  return `${session.startTs || 0}|${session.endTs || 0}|${resultsSig}`;
}

async function syncFromLaptimer() {
  let hostInput = document.getElementById('laptimerHostInput');
  let syncButton = document.getElementById('syncLaptimerButton');
  let clearButton = document.getElementById('clearLaptimerButton');
  let exportButton = document.getElementById('exportDatabaseButton');

  let baseUrl;
  try {
    baseUrl = normalizeLaptimerBaseUrl(hostInput ? hostInput.value : DEFAULT_LAPTIMER_HOST);
  } catch (error) {
    setOfflineStatus(error.message, 'error');
    return;
  }

  lastSyncedLaptimerUrl = null;
  if (clearButton) clearButton.disabled = true;
  if (syncButton) syncButton.disabled = true;
  if (hostInput) hostInput.disabled = true;

  try {
    if (!await offlineInitialization) return;

    setOfflineStatus(`Verbinde mit ${baseUrl} …`);
    let versionInfo = await fetchLaptimerJson(`${baseUrl}/api/version`);
    let firmwareLabel = versionInfo && versionInfo.firmware ? ` (${versionInfo.firmware})` : '';

    await sleepMs(LAPTIMER_STEP_DELAY_MS);
    setOfflineStatus(`Lade Strecken von ${baseUrl}${firmwareLabel} …`);
    let rawTracks = await fetchLaptimerJson(`${baseUrl}/api/tracks`);

    await sleepMs(LAPTIMER_STEP_DELAY_MS);
    setOfflineStatus(`Lade Fahrernamen von ${baseUrl}${firmwareLabel} …`);
    let names = await fetchLaptimerJson(`${baseUrl}/api/names`);

    await sleepMs(LAPTIMER_STEP_DELAY_MS);
    setOfflineStatus(`Lade Sessions-Index von ${baseUrl}${firmwareLabel} …`);
    let sessionsDoc = await fetchLaptimerJson(`${baseUrl}/api/sessions`);
    let sessions = Array.isArray(sessionsDoc) ? sessionsDoc : (sessionsDoc && sessionsDoc.sessions) || [];
    if (!Array.isArray(sessions)) {
      throw new Error('Der Lap Timer hat keine gültige Sessions-Liste geliefert.');
    }

    let snapshot = await readDatabaseSnapshot();
    let fileMap = getStoredFileMap(snapshot);
    let current = getDatasetFromSnapshot(snapshot, fileMap);
    let existingByKey = new Map();
    (current.sessions || []).forEach(s => {
      let key = getSessionKey(s);
      if (key) existingByKey.set(key, s);
    });

    let sessionsToFetch = sessions.filter(session => {
      let relPath = normalizeStoredPath(session.file);
      if (!relPath || !isStatisticPath(relPath)) return false;
      if (!fileMap.has(relPath)) return true;
      let existing = existingByKey.get(getSessionKey(session));
      return getSessionSignature(existing) !== getSessionSignature(session);
    });

    let sessionFiles = {};
    let entries = [];
    let skippedMissing = 0;

    for (let i = 0; i < sessionsToFetch.length; i++) {
      let session = sessionsToFetch[i];
      let relPath = normalizeStoredPath(session.file);
      setOfflineStatus(`Lade Session-Datei ${i + 1}/${sessionsToFetch.length}: ${relPath} …`);
      await sleepMs(LAPTIMER_STEP_DELAY_MS);
      try {
        let fileData = await fetchLaptimerJson(`${baseUrl}/api/sessionfile?file=${encodeURIComponent('/' + relPath)}`);
        sessionFiles['/' + relPath] = fileData;
        entries.push({
          path: relPath,
          content: fileData,
          size: JSON.stringify(fileData).length,
          modifiedAt: Date.now()
        });
      } catch (error) {
        if (error && error.httpStatus === 404) {
          skippedMissing++;
        } else {
          throw error;
        }
      }
    }

    let trackDocument = getOfflineTrackDocument({ tracks: rawTracks });
    let payload = {
      tracks: trackDocument,
      sessions,
      names: (names && typeof names === 'object' && !Array.isArray(names)) ? names : {},
      sessionFiles
    };

    let imported = await importIntoLocalDatabase(payload, entries);
    await writeOfflineSetting('laptimerHost', baseUrl.replace(/^https?:\/\//i, ''));
    await refreshOfflineViews(imported.dataset, imported.fileMap);
    updateSaveButtonsUI(true);
    lastSyncedLaptimerUrl = baseUrl;
    if (clearButton) clearButton.disabled = false;

    let summary = `${imported.dataset.sessions.length} Sessions · ${imported.fileMap.size} Dateien · Offline v${OFFLINE_STATS_VERSION}`;
    document.getElementById('offlineBackupSummary').textContent = summary;

    let status = `Lap Timer${firmwareLabel}: ${imported.added} neu, ${imported.updated} aktualisiert; Datenbank jetzt ${imported.dataset.sessions.length} Sessions.`;
    let autoSaveNote = await tryAutoSaveStatusNote();
    if (autoSaveNote) status += autoSaveNote;
    if (skippedMissing > 0) {
      status += ` ${skippedMissing} Session-Datei(en) auf dem Lap Timer nicht gefunden.`;
    } else if (imported.missingSessionFiles > 0) {
      status += ` ${imported.missingSessionFiles} Session-Datei(en) für Rundendetails fehlen.`;
    }
    setOfflineStatus(status, (skippedMissing > 0 || imported.missingSessionFiles > 0) ? 'error' : 'success');
  } catch (error) {
    setOfflineStatus(error.message || 'Daten konnten nicht vom Lap Timer geladen werden.', 'error');
  } finally {
    if (syncButton) syncButton.disabled = false;
    if (hostInput) hostInput.disabled = false;
  }
}

let _offlineDialogResolver = null;

function showUniversalDialog(opts) {
  return new Promise(resolve => {
    _offlineDialogResolver = resolve;
    const modal = document.getElementById('universalModal');
    if (!modal) {
      resolve(opts.isAlert ? true : window.confirm(opts.message ? opts.message.replace(/<[^>]*>/g, '') : opts.title));
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

    if (msg) msg.innerHTML = opts.message || '';

    const btnClass = 'btn ' + (
      cleanTheme === 'purple' ? 'btn-purple' :
      cleanTheme === 'danger' ? 'btn-red' :
      cleanTheme === 'orange' ? 'btn-orange' :
      (cleanTheme === 'green' || cleanTheme === 'success') ? 'btn-green' :
      'btn-cyan'
    );

    if (opts.isAlert) {
      if (cancelBtn) cancelBtn.style.display = 'none';
      if (confirmBtn) {
        confirmBtn.innerText = opts.confirmText || 'VERSTANDEN';
        confirmBtn.className = btnClass;
        confirmBtn.style.flex = '1';
      }
    } else {
      if (cancelBtn) {
        cancelBtn.style.display = 'block';
        cancelBtn.innerText = opts.cancelText || 'ABBRECHEN';
      }
      if (confirmBtn) {
        confirmBtn.innerText = opts.confirmText || 'BESTÄTIGEN';
        confirmBtn.className = btnClass;
      }
    }

    modal.style.display = 'block';
  });
}

window.closeUniversalDialog = function(result) {
  const modal = document.getElementById('universalModal');
  if (modal) modal.style.display = 'none';
  if (_offlineDialogResolver) {
    let res = _offlineDialogResolver;
    _offlineDialogResolver = null;
    res(result);
  }
};

window.showConfirm = function(opts) {
  return showUniversalDialog(Object.assign({}, opts, { isAlert: false }));
};

window.showAlert = function(opts) {
  return showUniversalDialog(Object.assign({}, opts, { isAlert: true }));
};

async function clearLaptimerStorage() {
  if (!lastSyncedLaptimerUrl) {
    setOfflineStatus('Bitte zuerst die Daten erfolgreich vom Lap Timer laden.', 'error');
    return;
  }

  let confirmed = await window.showConfirm({
    title: '🗑️ LAP-TIMER LEEREN?',
    theme: 'danger',
    boxLabel: 'Zielgerät im WLAN:',
    boxContent: lastSyncedLaptimerUrl,
    boxSub: 'Alle Sessions, Tagesbestzeiten & Streckenrekorde auf dem Gerät werden entfernt',
    message: 'Möchtest du die Statistikdaten auf dem Lap Timer wirklich löschen, um Speicherplatz freizugeben?<br><span style="color:var(--neon-green); font-size:12px; display:block; margin-top:8px;">✓ Deine lokale Offline-Statistikdatenbank in diesem Browser bleibt vollständig erhalten.</span>',
    confirmText: 'JETZT LÖSCHEN 🗑️',
    cancelText: 'ABBRECHEN'
  });
  if (!confirmed) return;

  let hostInput = document.getElementById('laptimerHostInput');
  let syncButton = document.getElementById('syncLaptimerButton');
  let clearButton = document.getElementById('clearLaptimerButton');
  let targetUrl = `${lastSyncedLaptimerUrl}/api/deleteallstats`;

  if (clearButton) clearButton.disabled = true;
  if (syncButton) syncButton.disabled = true;
  if (hostInput) hostInput.disabled = true;
  setOfflineStatus(`Lösche Statistikdaten auf ${lastSyncedLaptimerUrl} …`);

  let controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
  let timer = controller ? setTimeout(() => controller.abort(), LAPTIMER_FETCH_TIMEOUT_MS) : null;

  try {
    let response = await fetch(targetUrl, {
      method: 'POST',
      mode: 'cors',
      cache: 'no-store',
      signal: controller ? controller.signal : undefined
    });
    if (response.status === 423) {
      throw new Error('Löschen während eines laufenden Rennens auf dem Lap Timer gesperrt.');
    }
    if (!response.ok) {
      throw new Error(`HTTP ${response.status} beim Löschen auf dem Lap Timer.`);
    }
    lastSyncedLaptimerUrl = null;
    setOfflineStatus(
      'Speicherplatz auf dem Lap Timer wurde freigegeben. Deine lokale Offline-Datenbank bleibt erhalten.',
      'success'
    );
  } catch (error) {
    if (clearButton && lastSyncedLaptimerUrl) clearButton.disabled = false;
    if (error && error.name === 'AbortError') {
      setOfflineStatus('Zeitüberschreitung beim Löschen auf dem Lap Timer.', 'error');
    } else {
      setOfflineStatus(error.message || 'Statistikdaten auf dem Lap Timer konnten nicht gelöscht werden.', 'error');
    }
  } finally {
    if (timer) clearTimeout(timer);
    if (syncButton) syncButton.disabled = false;
    if (hostInput) hostInput.disabled = false;
  }
}

document.getElementById('backupFileInput').addEventListener('change', async event => {
  await loadOfflineFile(event.target.files[0]);
  event.target.value = '';
});
document.getElementById('syncLaptimerButton').addEventListener('click', syncFromLaptimer);
document.getElementById('clearLaptimerButton').addEventListener('click', clearLaptimerStorage);
document.getElementById('laptimerHostInput').addEventListener('input', () => {
  lastSyncedLaptimerUrl = null;
  let clearButton = document.getElementById('clearLaptimerButton');
  if (clearButton) clearButton.disabled = true;
});
document.getElementById('laptimerHostInput').addEventListener('keydown', event => {
  if (event.key === 'Enter') {
    event.preventDefault();
    syncFromLaptimer();
  }
});
document.getElementById('exportDatabaseButton').addEventListener('click', () => downloadStatisticsDatabase());
document.getElementById('fullscreenButton').addEventListener('click', toggleOfflineFullscreen);
document.addEventListener('fullscreenchange', updateFullscreenButton);

offlineInitialization = (async function initializeOfflineStatistics() {
  try {
    offlineDb = await openOfflineDatabase();
    let [savedHost, savedHandle] = await Promise.all([
      readOfflineSetting('laptimerHost'),
      readOfflineSetting('saveFileHandle')
    ]);
    let hostInput = document.getElementById('laptimerHostInput');
    if (hostInput && savedHost) {
      hostInput.value = savedHost;
    }
    if (savedHandle && typeof savedHandle.createWritable === 'function') {
      linkedSaveFileHandle = savedHandle;
    }
    await restoreLocalDatabase();
    return true;
  } catch (error) {
    setOfflineStatus(error.message || 'Lokale Statistikdatenbank konnte nicht initialisiert werden.', 'error');
    return false;
  }
})();
