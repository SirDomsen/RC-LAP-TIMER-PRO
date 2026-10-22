let speechQueue = [];
let isSpeaking = false;
let selectedTTSCars = JSON.parse(localStorage.getItem('myTTSCars') || '[]');
const synth = window.speechSynthesis;

function cancelRaceCountdown() {
  countdownTimeouts.forEach(t => clearTimeout(t));
  countdownTimeouts = [];
  if ('speechSynthesis' in window) {
    synth.cancel();
    speechQueue = [];
    isSpeaking = false;
  }
}

function speak(text, cancelCurrent = false) {
  if (!ttsEnabled || !('speechSynthesis' in window)) return;
  
  if (cancelCurrent) {
    synth.cancel();
    speechQueue = [];
    isSpeaking = false;
  }
  
  speechQueue.push(text);
  processSpeechQueue();
}

function processSpeechQueue() {
  if (isSpeaking || speechQueue.length === 0) return;

  isSpeaking = true;
  let text = speechQueue.shift();
  let utterance = new SpeechSynthesisUtterance(text);
  utterance.lang = 'de-DE';
  utterance.rate = 1.0;

  utterance.onend = function() {
    isSpeaking = false;
    processSpeechQueue();
  };

  utterance.onerror = function() {
    isSpeaking = false;
    processSpeechQueue();
  };

  synth.speak(utterance);
}

function isCarAllowedForTTS(carId) {
  if (!ttsEnabled) return false;
  if (selectedTTSCars.length === 0) return true;
  return selectedTTSCars.includes(Number(carId));
}

function toggleTTS() {
  ttsEnabled = !ttsEnabled;
  document.getElementById('ttsToggleBtn').innerText = ttsEnabled ? "🔊 Ton: AN" : "🔇 Ton: AUS";
  if (ttsEnabled) speak("Sprachausgabe aktiviert");
}

function unmuteIOSAudio() {
  if ('speechSynthesis' in window) {
    synth.cancel();
    speechQueue = [];
    isSpeaking = false;
    let u = new SpeechSynthesisUtterance(' ');
    u.volume = 0.01;
    synth.speak(u);
    audioUnmuted = true;
    let btn = document.getElementById('iosUnmuteBtn');
    if (btn) { btn.style.borderColor = 'var(--neon-green)'; btn.style.color = 'var(--neon-green)'; btn.innerText = '✅ iOS Ton Aktiv'; }
  }
}

function updateTTSFilterDisplay() {
  let disp = document.getElementById('ttsSelectedCarsDisplay');
  if (!disp) return;
  if (selectedTTSCars.length === 0) {
    disp.innerText = "Alle Fahrzeuge";
    disp.style.color = "var(--neon-cyan)";
  } else {
    let namesLoaded = selectedTTSCars.every(id => carNamesMap[id] !== undefined);
    if (!namesLoaded) {
      loadCarNames().then(() => updateTTSFilterDisplay());
      return;
    }
    let names = selectedTTSCars.map(id => carNamesMap[id]?.driver || ("Fahrer " + id));
    disp.innerText = names.join(", ");
    disp.style.color = "var(--neon-green)";
  }
}

function selectAllTTS(checkAll) {
  let cbs = document.querySelectorAll('.tts-car-cb');
  cbs.forEach(cb => cb.checked = checkAll);
}