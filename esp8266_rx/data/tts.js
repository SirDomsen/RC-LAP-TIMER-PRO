let speechQueue = [];
let isSpeaking = false;
let selectedTTSCars = JSON.parse(localStorage.getItem('myTTSCars') || '[]');
const synth = window.speechSynthesis;

// Phonetisches Wörterbuch für korrekte deutsche Sprachausgabe im Rennsport & Modellbau
const PHONETIC_MAP = [
  // 1. Rennmodi & Renngeschehen
  { pattern: /\bOutlaps\b/gi, replacement: "Autläpps" },
  { pattern: /\bOutlap\b/gi, replacement: "Autläpp" },
  { pattern: /\bInlaps\b/gi, replacement: "Innläpps" },
  { pattern: /\bInlap\b/gi, replacement: "Innläpp" },
  { pattern: /\bDragrace\b/gi, replacement: "Drägg-Rehs" },
  { pattern: /\bDrag Race\b/gi, replacement: "Drägg-Rehs" },
  { pattern: /\bQualifying\b/gi, replacement: "Kwalifei-ing" },
  { pattern: /\bPole Position\b/gi, replacement: "Pohl Posischn" },
  { pattern: /\bPohl Posischen\b/gi, replacement: "Pohl Posischn" },
  { pattern: /\bPitstop\b/gi, replacement: "Pitt-Stopp" },

  // 2. Fahrzeug- & RC-Klassen
  { pattern: /\bCrawler\b/gi, replacement: "Krohler" },
  { pattern: /\bBasher\b/gi, replacement: "Bäscher" },
  { pattern: /\bBuggy\b/gi, replacement: "Baggi" },
  { pattern: /\bBuggies\b/gi, replacement: "Baggies" },
  { pattern: /\bTruggy\b/gi, replacement: "Traggi" },
  { pattern: /\bTruggies\b/gi, replacement: "Traggies" },
  { pattern: /\bShort Course\b/gi, replacement: "Schort Kohrs" },
  { pattern: /\bMonster Truck\b/gi, replacement: "Monster Träck" },
  { pattern: /\bBrushless\b/gi, replacement: "Brasch-less" },
  { pattern: /\bBrushed\b/gi, replacement: "Brascht" },

  // 3. Arrma Modelle
  { pattern: /\bMojave\b/gi, replacement: "Mochawie" },
  { pattern: /\bGrom\b/gi, replacement: "Gromm" },
  { pattern: /\bTyphon\b/gi, replacement: "Teifon" },
  { pattern: /\bQuake\b/gi, replacement: "Kwehk" },
  { pattern: /\bGranite\b/gi, replacement: "Grännitt" },
  { pattern: /\bGorgon\b/gi, replacement: "Gorgonn" },
  { pattern: /\bKraton\b/gi, replacement: "Krehton" },
  { pattern: /\bNotorious\b/gi, replacement: "Notorias" },
  { pattern: /\bOutcast\b/gi, replacement: "Aut-Kahst" },
  { pattern: /\bInfraction\b/gi, replacement: "Infräktschn" },
  { pattern: /\bFelony\b/gi, replacement: "Fellonie" },
  { pattern: /\bFireteam\b/gi, replacement: "Feier-Tiehm" },
  { pattern: /\bBig Rock\b/gi, replacement: "Bigg Rock" },
  { pattern: /\bBigrock\b/gi, replacement: "Bigg Rock" },
  { pattern: /\bVorteks\b/gi, replacement: "Wortäks" },
  { pattern: /\bSenton\b/gi, replacement: "Sännton" },
  { pattern: /\bFury\b/gi, replacement: "Fjuhri" },

  // 4. Traxxas Modelle
  { pattern: /\bDrag Slash\b/gi, replacement: "Drägg Schläsch" },
  { pattern: /\bSlash\b/gi, replacement: "Schläsch" },
  { pattern: /\bRustler\b/gi, replacement: "Rassler" },
  { pattern: /\bStampede\b/gi, replacement: "Stäm-pihd" },
  { pattern: /\bSledge\b/gi, replacement: "Släddsch" },
  { pattern: /\bSlayer\b/gi, replacement: "Sleier" },
  { pattern: /\bX-Maxx\b/gi, replacement: "Ex-Mäxx" },
  { pattern: /\bXMaxx\b/gi, replacement: "Ex-Mäxx" },
  { pattern: /\bMaxx\b/gi, replacement: "Mäxx" },
  { pattern: /\bXRT\b/gi, replacement: "Ex-Err-Tee" },
  { pattern: /\bE-Revo\b/gi, replacement: "Ih-Rehwo" },
  { pattern: /\bRevo\b/gi, replacement: "Rehwo" },
  { pattern: /\bBandit\b/gi, replacement: "Bänditt" },
  { pattern: /\bHoss\b/gi, replacement: "Hoss" },
  { pattern: /\bUDR\b/gi, replacement: "U-De-Er" },

  // 5. HPI / Maverick / Quantum
  { pattern: /\bMaverick\b/gi, replacement: "Mäwwerick" },
  { pattern: /\bQuantum2\b/gi, replacement: "Kwantum Zwei" },
  { pattern: /\bQuantum\b/gi, replacement: "Kwantum" },
  { pattern: /\bSavage\b/gi, replacement: "Säwwidsch" },
  { pattern: /\bJumpshot\b/gi, replacement: "Dschamp-Schott" },
  { pattern: /\bBaja\b/gi, replacement: "Bacha" },
  { pattern: /\bBullet\b/gi, replacement: "Bullitt" },
  { pattern: /\bFlux\b/gi, replacement: "Flax" },
  { pattern: /\bVorza\b/gi, replacement: "Wortsa" },

  // 6. Spezielle Namen & Rufnamen
  { pattern: /\bMike\b/gi, replacement: "Maik" },
  { pattern: /\bPapa\b/gi, replacement: "Pappa" },
  { pattern: /\bDominik\b/gi, replacement: "Domminick" },
  { pattern: /\bDominic\b/gi, replacement: "Domminick" }
];

function applyPhonetics(text) {
  if (!text || typeof text !== 'string') return text;
  let result = text;
  for (let i = 0; i < PHONETIC_MAP.length; i++) {
    result = result.replace(PHONETIC_MAP[i].pattern, PHONETIC_MAP[i].replacement);
  }
  // Rennsport-Zeitangaben bereinigen, falls ein isoliertes Komma übergeben wird (z. B. "12,54" -> "12 54", "12,05" -> "12 null 5", "12,00" -> "12 null")
  result = result.replace(/\b(\d+),00\b/g, '$1 null');
  result = result.replace(/\b(\d+),0(\d)\b/g, '$1 null $2');
  result = result.replace(/\b(\d+),(\d{2})\b/g, '$1 $2');
  return result;
}

function formatTimeForTTS(ms) {
  if (!ms || isNaN(ms) || ms <= 0) return "";
  let totalHundredths = Math.round(ms / 10);
  let hundredths = totalHundredths % 100;
  let totalSeconds = Math.floor(totalHundredths / 100);
  let minutes = Math.floor(totalSeconds / 60);
  let seconds = totalSeconds % 60;

  let hStr = "";
  if (hundredths === 0) {
    hStr = "null";
  } else if (hundredths < 10) {
    hStr = "null " + hundredths;
  } else {
    hStr = String(hundredths);
  }

  if (minutes > 0) {
    let minWord = (minutes === 1) ? "eins" : String(minutes);
    if (seconds === 0 && hundredths === 0) {
      return `${minWord}, null`;
    }
    let secStr = (seconds < 10) ? ("null " + seconds) : String(seconds);
    return `${minWord}, ${secStr} ${hStr}`;
  } else {
    return `${seconds} ${hStr}`;
  }
}

// Beim Laden/Aktualisieren der Seite eventuelle Reste aus dem Browser-Sprachpuffer verwerfen
if ('speechSynthesis' in window) {
  try {
    synth.cancel();
  } catch (e) {}
}

function cancelRaceCountdown() {
  countdownTimeouts.forEach(t => clearTimeout(t));
  countdownTimeouts = [];
  if ('speechSynthesis' in window) {
    synth.cancel();
    speechQueue = [];
    isSpeaking = false;
  }
  fetch('/api/buzzer?cmd=stop', { method: 'POST' }).catch(() => {});
}

function speak(text, cancelCurrent = false) {
  if (!ttsEnabled || !('speechSynthesis' in window)) return;
  
  if (cancelCurrent) {
    try { synth.cancel(); } catch (e) {}
    speechQueue = [];
    isSpeaking = false;
    setTimeout(() => {
      speechQueue.push(text);
      processSpeechQueue();
    }, 50);
    return;
  }
  
  speechQueue.push(text);
  processSpeechQueue();
}

let speechWatchdogTimeout = null;

function processSpeechQueue() {
  if (isSpeaking || speechQueue.length === 0) return;

  isSpeaking = true;
  let rawText = speechQueue.shift();
  let text = applyPhonetics(rawText);
  let utterance = new SpeechSynthesisUtterance(text);
  utterance.lang = 'de-DE';
  utterance.rate = 1.0;

  let clearWatchdog = () => {
    if (speechWatchdogTimeout) {
      clearTimeout(speechWatchdogTimeout);
      speechWatchdogTimeout = null;
    }
  };

  utterance.onend = function() {
    clearWatchdog();
    isSpeaking = false;
    processSpeechQueue();
  };

  utterance.onerror = function() {
    clearWatchdog();
    isSpeaking = false;
    processSpeechQueue();
  };

  clearWatchdog();
  speechWatchdogTimeout = setTimeout(() => {
    if (isSpeaking) {
      try { synth.cancel(); } catch (e) {}
      isSpeaking = false;
      processSpeechQueue();
    }
  }, 7000);

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
    disp.innerText = names.join(" & ");
    disp.style.color = "var(--neon-green)";
  }
}

function selectAllTTS(checkAll) {
  let cbs = document.querySelectorAll('.tts-car-cb');
  cbs.forEach(cb => cb.checked = checkAll);
}