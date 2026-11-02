window.offlineStatsMode = true;
window.MAX_CARS = 15;
window.carNamesMap = {};
window.allTracksList = [];
window.activeTrackId = 'standard';
window.activeTrackName = 'Standard';
window.currentTab = 'drivers';
window.activeStatsCarId = null;
window.userRole = 'driver';
window.activeUseSz1 = true;
window.activeUseSz2 = true;
window.activeUseSz3 = true;

const OFFLINE_STATS_VERSION = '0.7.0';
const OFFLINE_DB_NAME = 'rc-lap-timer-offline-stats';
const OFFLINE_DB_VERSION = 1;
const MAX_ZIP_ENTRIES = 10000;
const MAX_STAT_FILE_SIZE = 64 * 1024 * 1024;
const MAX_TOTAL_STAT_SIZE = 512 * 1024 * 1024;

let offlineDb = null;
let offlineBackup = null;
let targetDirectoryHandle = null;
let offlineInitialization = null;

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
  let normalized = {
    ...data,
    laps: Array.isArray(data.laps) ? data.laps.map(normalizeLap).filter(Boolean) : []
  };
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
  if (data.version !== 3 && data.version !== 4) {
    throw new Error('Nicht unterstützte JSON-Backup-Version. Erwartet wird Version 3 oder 4.');
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
    version: 4,
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

async function writeFileToDirectory(rootHandle, path, content) {
  let parts = normalizeStoredPath(path)?.split('/');
  if (!parts || !parts.length) throw new Error(`Ungültiger Ausgabepfad: ${path}`);

  let directory = rootHandle;
  for (let i = 0; i < parts.length - 1; i++) {
    directory = await directory.getDirectoryHandle(parts[i], { create: true });
  }
  let fileHandle = await directory.getFileHandle(parts[parts.length - 1], { create: true });
  let writable = await fileHandle.createWritable();
  try {
    await writable.write(JSON.stringify(content));
    await writable.close();
  } catch (error) {
    await writable.abort();
    throw error;
  }
}

async function exportDatabaseToTargetFolder(fileMap, dataset) {
  if (!targetDirectoryHandle) return false;
  let permission = await targetDirectoryHandle.queryPermission({ mode: 'readwrite' });
  if (permission !== 'granted') return false;

  let outputFiles = new Map(fileMap);
  outputFiles.set('tracks.json', { path: 'tracks.json', content: dataset.tracks });
  outputFiles.set('names.json', { path: 'names.json', content: dataset.names });
  outputFiles.set('sessions.json', { path: 'sessions.json', content: { sessions: dataset.sessions } });

  for (let [path, entry] of outputFiles) {
    await writeFileToDirectory(targetDirectoryHandle, path, entry.content);
  }
  return true;
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

async function updateTargetFolderLabel() {
  let label = document.getElementById('targetFolderName');
  if (!label) return;
  if (targetDirectoryHandle) {
    label.textContent = targetDirectoryHandle.name;
    label.title = targetDirectoryHandle.name;
  } else {
    label.textContent = 'Kein Zielordner gewählt';
    label.removeAttribute('title');
  }
}

async function chooseTargetFolder() {
  if (typeof window.showDirectoryPicker !== 'function') {
    setOfflineStatus('Dieser Browser unterstützt keinen direkten Ordnerzugriff. Die IndexedDB bleibt weiterhin verfügbar.', 'error');
    return;
  }

  try {
    targetDirectoryHandle = await window.showDirectoryPicker({ mode: 'readwrite' });
    let permission = await targetDirectoryHandle.requestPermission({ mode: 'readwrite' });
    if (permission !== 'granted') {
      targetDirectoryHandle = null;
      await updateTargetFolderLabel();
      throw new Error('Schreibzugriff auf den ausgewählten Ordner wurde nicht erteilt.');
    }

    if (!await offlineInitialization) {
      targetDirectoryHandle = null;
      await updateTargetFolderLabel();
      return;
    }

    let transaction = offlineDb.transaction('settings', 'readwrite');
    transaction.objectStore('settings').put({ key: 'targetDirectory', handle: targetDirectoryHandle });
    await new Promise((resolve, reject) => {
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error || new Error('Ordnerauswahl konnte nicht gespeichert werden.'));
      transaction.onabort = () => reject(transaction.error || new Error('Speichern der Ordnerauswahl wurde abgebrochen.'));
    });
    await updateTargetFolderLabel();
    setOfflineStatus(`Ausgabeordner „${targetDirectoryHandle.name}“ ausgewählt.`, 'success');
  } catch (error) {
    if (error.name !== 'AbortError') setOfflineStatus(error.message || 'Der Ausgabeordner konnte nicht ausgewählt werden.', 'error');
  }
}

async function loadOfflineFile(file) {
  if (!file) return;
  setOfflineStatus(`Lese ${file.name} …`);

  try {
    if (!await offlineInitialization) return;
    let { payload, entries } = await readImportFile(file);
    let imported = await importIntoLocalDatabase(payload, entries);
    await refreshOfflineViews(imported.dataset, imported.fileMap);

    let folderWritten = false;
    let folderError = null;
    if (targetDirectoryHandle) {
      try {
        folderWritten = await exportDatabaseToTargetFolder(imported.fileMap, imported.dataset);
      } catch (error) {
        folderError = error;
      }
    }

    let summary = `${imported.dataset.sessions.length} Sessions · ${imported.fileMap.size} Dateien · Offline v${OFFLINE_STATS_VERSION}`;
    document.getElementById('offlineBackupSummary').textContent = summary;
    let status = `${file.name}: ${imported.added} neu, ${imported.updated} aktualisiert; Datenbank jetzt ${imported.dataset.sessions.length} Sessions.`;
    if (folderWritten) status += ` In „${targetDirectoryHandle.name}“ gespiegelt.`;
    else if (folderError) status += ` Ordnerexport fehlgeschlagen: ${folderError.message}`;
    else if (targetDirectoryHandle) status += ' Ordnerzugriff fehlt; bitte den Zielordner erneut auswählen.';
    else status += ' Nur im Browser gespeichert – noch keinen Ausgabeordner gewählt.';
    if (imported.missingSessionFiles) {
      status += ` ${imported.missingSessionFiles} Session-Datei(en) für Rundendetails fehlen.`;
    }
    setOfflineStatus(status, folderError || imported.missingSessionFiles ? 'error' : 'success');
  } catch (error) {
    setOfflineStatus(error.message || 'Das Backup konnte nicht importiert werden.', 'error');
  }
}

async function restoreLocalDatabase() {
  let snapshot = await readDatabaseSnapshot();
  let fileMap = getStoredFileMap(snapshot);
  let dataset = getDatasetFromSnapshot(snapshot, fileMap);
  if (!dataset.tracks) return;

  await refreshOfflineViews(dataset, fileMap);
  document.getElementById('offlineBackupSummary').textContent =
    `${dataset.sessions.length} Sessions · ${fileMap.size} Dateien · Offline v${OFFLINE_STATS_VERSION}`;
  setOfflineStatus(
    `Lokale Datenbank geladen: ${dataset.sessions.length} Sessions, ${fileMap.size} Statistikdateien.`,
    'success'
  );
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

document.getElementById('backupFileInput').addEventListener('change', async event => {
  await loadOfflineFile(event.target.files[0]);
  event.target.value = '';
});
document.getElementById('chooseFolderButton').addEventListener('click', chooseTargetFolder);
document.getElementById('fullscreenButton').addEventListener('click', toggleOfflineFullscreen);
document.addEventListener('fullscreenchange', updateFullscreenButton);

offlineInitialization = (async function initializeOfflineStatistics() {
  try {
    offlineDb = await openOfflineDatabase();
    let settings = await new Promise((resolve, reject) => {
      let transaction = offlineDb.transaction('settings', 'readonly');
      let request = transaction.objectStore('settings').get('targetDirectory');
      request.onsuccess = () => resolve(request.result?.handle || null);
      request.onerror = () => reject(request.error || new Error('Lokale Einstellungen konnten nicht gelesen werden.'));
    });
    targetDirectoryHandle = settings;
    await updateTargetFolderLabel();
    await restoreLocalDatabase();
    return true;
  } catch (error) {
    setOfflineStatus(error.message || 'Lokale Statistikdatenbank konnte nicht initialisiert werden.', 'error');
    return false;
  }
})();
