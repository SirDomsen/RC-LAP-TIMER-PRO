let initialCacheState = null;
let pendingWifiMode = 0;

function openConfigModal() {
  let modal = document.getElementById('configModal');
  if (modal) modal.style.display = 'block';

  let container = document.getElementById('configRows');
  let ttsContainer = document.getElementById('ttsFilterContainer');

  let lapTimeCb = document.getElementById('cfg_announce_lap_times');
  if (lapTimeCb) lapTimeCb.checked = announceLapTimesInRace;
  
  if (container) container.innerHTML = '<p style="color:var(--text-muted);">Lade Daten...</p>';
  if (ttsContainer) ttsContainer.innerHTML = '<p style="color:var(--text-muted);">Lade Daten...</p>';

  fetch('/api/wificonfig').then(r => r.json()).then(data => {
    let homeS = document.getElementById('cfg_home_ssid');
    let homeP = document.getElementById('cfg_home_pass');
    let hotS = document.getElementById('cfg_hotspot_ssid');
    let hotP = document.getElementById('cfg_hotspot_pass');
    let apS = document.getElementById('cfg_ap_ssid');
    let apP = document.getElementById('cfg_ap_pass');
    
    if (homeS) homeS.value = data.homeSSID || '';
    if (homeP) homeP.value = data.homePass || '';
    if (hotS) hotS.value = data.hotspotSSID || '';
    if (hotP) hotP.value = data.hotspotPass || '';
    if (apS) apS.value = data.apSSID || '';
    if (apP) apP.value = data.apPass || '';

    // WLAN-Badge mit Fallback-Erkennung aktualisieren
    let badge = document.getElementById('activeWifiBadge');
    if (badge) {
      let configured = data.mode;
      let actual = data.runtimeMode !== undefined ? data.runtimeMode : data.mode;
      let isFallback = (configured !== actual);

      let txt = "";
      let color = "";
      let bg = "";

      if (actual === 1) { 
        txt = "🏠 Heim-WLAN"; 
        color = "var(--neon-green)"; 
        bg = "rgba(0,255,102,0.1)"; 
      }
      else if (actual === 2) { 
        txt = "📱 Hotspot"; 
        color = "var(--neon-orange)"; 
        bg = "rgba(255,153,0,0.1)"; 
      }
      else { 
        txt = "📡 AP-Modus"; 
        color = "var(--neon-purple)"; 
        bg = "rgba(181,50,255,0.1)"; 
      }

      if (isFallback) {
        let source = configured === 1 ? "Heim-WLAN" : (configured === 2 ? "Hotspot" : "unbekannt");
        txt += ` (Fallback von ${source})`;
        color = "var(--neon-red)";
        bg = "rgba(255,51,102,0.15)";
      }

      badge.innerText = txt + " aktiv";
      badge.style.borderColor = color;
      badge.style.color = color;
      badge.style.background = bg;
    }
  }).catch(e => console.error(e));

  fetch('/api/deadtime').then(r => r.json()).then(data => {
    let dtElem = document.getElementById('cfg_deadtime');
    if (dtElem && data.ms) dtElem.value = (data.ms / 1000).toFixed(1);
  }).catch(e => console.error(e));

  fetch('/api/filter').then(r => r.json()).then(data => {
    let cb = document.getElementById('cfg_two_frames');
    if (cb) cb.checked = data.enabled;
  }).catch(e => console.error(e));

  fetch('/api/cacheconfig').then(r => r.json()).then(data => {
    let cb = document.getElementById('cfg_web_cache');
    if (cb) {
      cb.checked = data.cache;
      initialCacheState = data.cache;
    }
  }).catch(e => console.error(e));

  fetch('/api/sectorconfig').then(r => r.json()).then(data => {
    let sz1 = document.getElementById('cfg_use_sz1');
    let sz2 = document.getElementById('cfg_use_sz2');
    let sz3 = document.getElementById('cfg_use_sz3');
    let cp = document.getElementById('cfg_checkpoint_mode');
    let dr = document.getElementById('cfg_dragrace_mode');
    let audio = document.getElementById('cfg_sector_audio');

    if (sz1) sz1.checked = data.useSz1;
    if (sz2) sz2.checked = data.useSz2;
    if (sz3) sz3.checked = data.useSz3;
    if (cp) cp.checked = data.checkpointMode;
    if (dr) dr.checked = data.dragraceMode;

    if (data.sectorAudio !== undefined) {
      sectorAudioEnabled = data.sectorAudio;
      localStorage.setItem('sectorAudioEnabled', JSON.stringify(sectorAudioEnabled));
    }
    if (audio) audio.checked = sectorAudioEnabled;
  }).catch(e => console.error(e));

  fetch('/api/names').then(r => r.json()).then(names => {
    if (!container || !ttsContainer) return;
    container.innerHTML = '';
    ttsContainer.innerHTML = '';

    for (let i = 1; i <= MAX_CARS; i++) {
      let driver = names[i]?.driver || ('Fahrer ' + i);
      let car = names[i]?.car || ('Auto ' + i);
      carNamesMap[i] = { driver: driver, car: car };
      
      container.innerHTML += `<div class="modal-row">
        <span>ID <strong>${i}</strong>:</span>
        <input type="text" id="cfg_driver_${i}" value="${driver}" maxlength="20" placeholder="Fahrer" style="width:30%;">
        <input type="text" id="cfg_car_${i}" value="${car}" maxlength="20" placeholder="Auto" style="width:30%;">
      </div>`;

      let isChecked = (selectedTTSCars.length === 0 || selectedTTSCars.includes(i)) ? 'checked' : '';
      ttsContainer.innerHTML += `<div class="tts-filter-item">
        <input type="checkbox" class="tts-car-cb" id="cb_tts_${i}" value="${i}" ${isChecked}>
        <label for="cb_tts_${i}"><strong>${driver}</strong></label>
      </div>`;
    }
    updateTTSFilterDisplay();
  }).catch(e => {
    console.error(e);
    if (container) container.innerHTML = '<p style="color:var(--neon-red);">Fehler beim Laden der Namen</p>';
  });
}

function closeConfigModal() {
  let modal = document.getElementById('configModal');
  if (modal) modal.style.display = 'none';
}

async function saveConfig() {
  try {
    let cbs = document.querySelectorAll('.tts-car-cb');
    let selected = [];
    let totalCount = cbs.length;

    cbs.forEach(cb => {
      if (cb.checked) selected.push(Number(cb.value));
    });

    if (selected.length === totalCount) {
      selectedTTSCars = [];
    } else {
      selectedTTSCars = selected;
    }
    localStorage.setItem('myTTSCars', JSON.stringify(selectedTTSCars));

    let lapTimeCb = document.getElementById('cfg_announce_lap_times');
    if (lapTimeCb) {
      announceLapTimesInRace = lapTimeCb.checked;
      localStorage.setItem('announceLapTimesInRace', JSON.stringify(announceLapTimesInRace));
    }

    let hS = document.getElementById('cfg_home_ssid')?.value || '';
    let hP = document.getElementById('cfg_home_pass')?.value || '';
    let hsS = document.getElementById('cfg_hotspot_ssid')?.value || '';
    let hsP = document.getElementById('cfg_hotspot_pass')?.value || '';
    let aS = document.getElementById('cfg_ap_ssid')?.value || '';
    let aP = document.getElementById('cfg_ap_pass')?.value || '';
    
    let wUrl = `/api/setwificonfig?homeSSID=${encodeURIComponent(hS)}&homePass=${encodeURIComponent(hP)}&hotspotSSID=${encodeURIComponent(hsS)}&hotspotPass=${encodeURIComponent(hsP)}&apSSID=${encodeURIComponent(aS)}&apPass=${encodeURIComponent(aP)}`;
    await fetch(wUrl, { method: 'POST' });

    let deadtimeElem = document.getElementById('cfg_deadtime');
    if (deadtimeElem) {
      let deadtimeSec = parseFloat(deadtimeElem.value);
      if (!isNaN(deadtimeSec)) {
        await fetch('/api/setdeadtime?ms=' + Math.round(deadtimeSec * 1000), { method: 'POST' });
      }
    }

    let filterCb = document.getElementById('cfg_two_frames');
    if (filterCb) {
      await fetch('/api/setfilter?enable=' + (filterCb.checked ? "1" : "0"), { method: 'POST' });
    }

    let cacheCb = document.getElementById('cfg_web_cache');
    if (cacheCb) {
      let cacheVal = cacheCb.checked;
      if (initialCacheState !== null && cacheVal !== initialCacheState) {
        await fetch('/api/setcacheconfig?cache=' + (cacheVal ? "1" : "0"), { method: 'POST' });
        alert("Info: Die Änderung des Web-Caches wird erst nach einem Neustart des ESP8266 aktiv.");
      }
    }

    let sz1 = document.getElementById('cfg_use_sz1')?.checked ? "1" : "0";
    let sz2 = document.getElementById('cfg_use_sz2')?.checked ? "1" : "0";
    let sz3 = document.getElementById('cfg_use_sz3')?.checked ? "1" : "0";
    let cp = document.getElementById('cfg_checkpoint_mode')?.checked ? "1" : "0";
    let dr = document.getElementById('cfg_dragrace_mode')?.checked ? "1" : "0";

    let audioCb = document.getElementById('cfg_sector_audio');
    if (audioCb) {
      sectorAudioEnabled = audioCb.checked;
      localStorage.setItem('sectorAudioEnabled', JSON.stringify(sectorAudioEnabled));
    }
    let audio = sectorAudioEnabled ? "1" : "0";

    await fetch(`/api/setsectorconfig?sz1=${sz1}&sz2=${sz2}&sz3=${sz3}&cp=${cp}&dr=${dr}&audio=${audio}`, { method: 'POST' });

    for (let i = 1; i <= MAX_CARS; i++) {
      let driverElem = document.getElementById('cfg_driver_' + i);
      let carElem = document.getElementById('cfg_car_' + i);
      if (driverElem && carElem) {
        let driverVal = driverElem.value.trim() || ('Fahrer ' + i);
        let carVal = carElem.value.trim() || ('Auto ' + i);
        
        let oldDriver = carNamesMap[i]?.driver;
        let oldCar = carNamesMap[i]?.car;
        
        if (driverVal !== oldDriver || carVal !== oldCar) {
          carNamesMap[i] = { driver: driverVal, car: carVal }; 
          await fetch('/api/rename?id=' + i + '&driver=' + encodeURIComponent(driverVal) + '&car=' + encodeURIComponent(carVal), { method: 'POST' });
        }
      }
    }
  } catch (err) { 
    console.error("Fehler beim Speichern der Konfiguration:", err); 
  } finally {
    closeConfigModal();
    updateData();
    if (typeof loadWifiStatus === 'function') loadWifiStatus();
  }
}

async function forceWifiMode(mode) {
  let hsSsid = document.getElementById('cfg_hotspot_ssid')?.value || '';
  
  if (mode === 2 && !hsSsid) { 
    alert("Bitte erst eine Hotspot SSID eintragen!"); 
    return; 
  }
  
  pendingWifiMode = mode;
  
  let modeName = "";
  let expectedSsid = "";
  
  if (mode === 1) { 
    modeName = "Heim-WLAN"; 
    expectedSsid = "Dein Heimnetzwerk (Basis IP: 192.168.0.8)"; 
  }
  else if (mode === 2) { 
    modeName = "Hotspot"; 
    let maxLen = 31 - 4 - 15; 
    let shortSsid = hsSsid;
    if (shortSsid.length > maxLen) shortSsid = shortSsid.substring(0, maxLen);
    expectedSsid = "LT_" + shortSsid + "-[IP-Adresse]";
  }
  else { 
    modeName = "AP-Modus"; 
    expectedSsid = (document.getElementById('cfg_ap_ssid')?.value || "RC-LapTimer") + " (Kein Geister-WLAN)"; 
  }
  
  let desc = document.getElementById('alertModalDesc');
  if (desc) desc.innerText = "Das Laptimer-System bricht alle aktuellen Verbindungen ab und wechselt sofort in: " + modeName + ".";
  
  let displaySsid = document.getElementById('alertSsidDisplay');
  if (displaySsid) displaySsid.innerText = expectedSsid;

  let displayIp = document.getElementById('alertIpDisplay');
  if (displayIp) {
    if (mode === 0 || mode === 1) displayIp.innerText = "192.168.0.8";
    else displayIp.innerText = "DIE.ANGEZEIGTE.IP";
  }

  closeConfigModal();
  let modal = document.getElementById('connectionAlertModal');
  if (modal) modal.style.display = 'block';
}

async function confirmConnectionAlert() {
  let modal = document.getElementById('connectionAlertModal');
  if (modal) modal.style.display = 'none';

  let hS = document.getElementById('cfg_home_ssid')?.value || '';
  let hP = document.getElementById('cfg_home_pass')?.value || '';
  let hsS = document.getElementById('cfg_hotspot_ssid')?.value || '';
  let hsP = document.getElementById('cfg_hotspot_pass')?.value || '';
  let aS = document.getElementById('cfg_ap_ssid')?.value || '';
  let aP = document.getElementById('cfg_ap_pass')?.value || '';
  
  let wUrl = `/api/setwificonfig?homeSSID=${encodeURIComponent(hS)}&homePass=${encodeURIComponent(hP)}&hotspotSSID=${encodeURIComponent(hsS)}&hotspotPass=${encodeURIComponent(hsP)}&apSSID=${encodeURIComponent(aS)}&apPass=${encodeURIComponent(aP)}`;
  await fetch(wUrl, { method: 'POST' });
  
  fetch('/api/switchwifi?mode=' + pendingWifiMode, { method: 'POST' });
}

function triggerSZOta() {
  closeConfigModal();
  let modal = document.getElementById('otaAlertModal');
  let statusBox = document.getElementById('otaModalStatus');
  let btns = document.getElementById('otaModalButtons');
  let desc = document.getElementById('otaModalDesc');
  
  if (modal) {
    statusBox.style.display = 'none';
    btns.innerHTML = `
      <button class="btn btn-warning" style="flex:1; padding:12px; font-weight:bold;" onclick="closeOtaAlert()">ABBRECHEN</button>
      <button class="btn btn-red" style="flex:1; padding:12px; font-weight:bold;" onclick="confirmOtaAlert()">STARTEN</button>
    `;
    btns.style.display = 'flex';
    desc.style.display = 'block';
    modal.style.display = 'block';
  }
}

function closeOtaAlert() {
  let modal = document.getElementById('otaAlertModal');
  if (modal) modal.style.display = 'none';
}

async function confirmOtaAlert() {
  let btns = document.getElementById('otaModalButtons');
  let statusBox = document.getElementById('otaModalStatus');
  let statusText = document.getElementById('otaStatusText');
  let desc = document.getElementById('otaModalDesc');
  
  btns.style.display = 'none';
  desc.style.display = 'none';
  statusBox.style.display = 'block';
  statusText.style.color = 'var(--neon-cyan)';
  statusText.innerText = "Sende Trigger-Signal an Sektorstationen...";

  try {
    let res = await fetch('/api/trigger_sz_ota', { method: 'POST' });
    let msg = await res.text();
    
    if (!res.ok) {
      statusText.style.color = 'var(--neon-orange)';
      statusText.innerText = "⚠️ Fehler: " + msg;
    } else {
      statusText.style.color = 'var(--neon-green)';
      statusText.innerHTML = "✅ Signal erfolgreich gesendet!<br><br><span style='color:var(--text-main);font-size:13px;font-weight:normal;'>Die Stationen blinken nun schnell (3Hz) und verbinden sich mit dem aktuellen Netzwerk.<br>Flash-Vorgang per OTA kann gestartet werden.</span>";
    }
  } catch(e) {
    statusText.style.color = 'var(--neon-red)';
    statusText.innerText = "❌ Fehler beim Senden: " + e.message;
  }
  
  setTimeout(() => {
    btns.innerHTML = `<button class="btn btn-cyan" style="width:100%; padding:12px; font-weight:bold;" onclick="closeOtaAlert()">SCHLIESSEN</button>`;
    btns.style.display = 'flex';
  }, 500);
}