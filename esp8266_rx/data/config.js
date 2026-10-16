function openConfigModal() {
  document.getElementById('configModal').style.display = 'block';

  let container = document.getElementById('configRows');
  let ttsContainer = document.getElementById('ttsFilterContainer');

  let lapTimeCb = document.getElementById('cfg_announce_lap_times');
  if (lapTimeCb) lapTimeCb.checked = announceLapTimesInRace;
  
  container.innerHTML = '<p style="color:var(--text-muted);">Lade Daten...</p>';
  ttsContainer.innerHTML = '<p style="color:var(--text-muted);">Lade Daten...</p>';

  fetch('/api/deadtime').then(r => r.json()).then(data => {
    if (data.ms) document.getElementById('cfg_deadtime').value = (data.ms / 1000).toFixed(1);
  }).catch(e => console.error(e));

  fetch('/api/filter').then(r => r.json()).then(data => {
    let cb = document.getElementById('cfg_two_frames');
    if (cb) cb.checked = data.enabled;
  }).catch(e => console.error(e));

  fetch('/api/sectorconfig').then(r => r.json()).then(data => {
    if (document.getElementById('cfg_use_sz1')) document.getElementById('cfg_use_sz1').checked = data.useSz1;
    if (document.getElementById('cfg_use_sz2')) document.getElementById('cfg_use_sz2').checked = data.useSz2;
    if (document.getElementById('cfg_use_sz3')) document.getElementById('cfg_use_sz3').checked = data.useSz3;
    if (document.getElementById('cfg_checkpoint_mode')) document.getElementById('cfg_checkpoint_mode').checked = data.checkpointMode;
    if (document.getElementById('cfg_dragrace_mode')) document.getElementById('cfg_dragrace_mode').checked = data.dragraceMode;
    if (data.sectorAudio !== undefined) {
      sectorAudioEnabled = data.sectorAudio;
      localStorage.setItem('sectorAudioEnabled', JSON.stringify(sectorAudioEnabled));
    }
    if (document.getElementById('cfg_sector_audio')) document.getElementById('cfg_sector_audio').checked = sectorAudioEnabled;
  }).catch(e => console.error(e));

  fetch('/api/names').then(r => r.json()).then(names => {
    container.innerHTML = '';
    ttsContainer.innerHTML = '';

    for (let i = 1; i <= MAX_CARS; i++) {
      let driver = names[i]?.driver || ('Fahrer ' + i);
      let car = names[i]?.car || ('Auto ' + i);
      carNamesMap[i] = { driver: driver, car: car };
      
      container.innerHTML += `<div class="modal-row">
        <span>ID <strong>${i}</strong>:</span>
        <input type="text" id="cfg_driver_${i}" value="${driver}" maxlength="20" placeholder="Fahrer">
        <input type="text" id="cfg_car_${i}" value="${car}" maxlength="20" placeholder="Auto">
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
    container.innerHTML = '<p style="color:var(--neon-red);">Fehler beim Laden der Namen</p>';
  });
}

function closeConfigModal() {
  document.getElementById('configModal').style.display = 'none';
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

    let deadtimeSec = parseFloat(document.getElementById('cfg_deadtime').value);
    if (!isNaN(deadtimeSec)) {
      await fetch('/api/setdeadtime?ms=' + Math.round(deadtimeSec * 1000), { method: 'POST' });
    }
    let filterCb = document.getElementById('cfg_two_frames');
    if (filterCb) {
      await fetch('/api/setfilter?enable=' + (filterCb.checked ? "1" : "0"), { method: 'POST' });
    }

    let sz1 = document.getElementById('cfg_use_sz1').checked ? "1" : "0";
    let sz2 = document.getElementById('cfg_use_sz2').checked ? "1" : "0";
    let sz3 = document.getElementById('cfg_use_sz3').checked ? "1" : "0";
    let cp = document.getElementById('cfg_checkpoint_mode').checked ? "1" : "0";
    let dr = document.getElementById('cfg_dragrace_mode').checked ? "1" : "0";
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
        carNamesMap[i] = { driver: driverVal, car: carVal }; 
        await fetch('/api/rename?id=' + i + '&driver=' + encodeURIComponent(driverVal) + '&car=' + encodeURIComponent(carVal), { method: 'POST' });
      }
    }
  } catch (err) { 
    console.error(err); 
  } finally {
    closeConfigModal();
    updateData();
  }
}