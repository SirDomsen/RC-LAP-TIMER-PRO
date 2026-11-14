#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_idf_version.h>
#include "hardware_config.h"
#include "platform_runtime.h"
#include <status_leds.h>

namespace {
constexpr const char* LED_CONFIG_PATH = "/led.json";
constexpr uint32_t SIGNAL_FLASH_RGB_MS = 35;

uint32_t readyLedOffTime = 0;
uint32_t lastYellowFlash = 0;
bool readyLedBlinking = false;
bool networkReadyState = false;

uint32_t signalRgbUntilMs = 0;
bool signalRgbActive = false;

bool previewActive = false;
uint32_t previewUntilMs = 0;
uint32_t previewColor = 0;
uint8_t previewBrightness = 20;

LapStatusLeds::RgbLedConfig rgbConfig;
int16_t lastWrittenR = -1;
int16_t lastWrittenG = -1;
int16_t lastWrittenB = -1;

uint8_t getRgbPin() {
#ifdef RGB_BUILTIN
  return RGB_BUILTIN;
#else
  return LED_ONBOARD;
#endif
}

void writeRawRgb(uint8_t r, uint8_t g, uint8_t b) {
  if ((int16_t)r == lastWrittenR && (int16_t)g == lastWrittenG && (int16_t)b == lastWrittenB) {
    return;
  }
  lastWrittenR = r;
  lastWrittenG = g;
  lastWrittenB = b;
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
  rgbLedWrite(getRgbPin(), r, g, b);
#else
  neopixelWrite(getRgbPin(), r, g, b);
#endif
}

void applyRgbState() {
  uint32_t activeColor = rgbConfig.bootColor;
  uint8_t activeBri = rgbConfig.brightness;

  if (previewActive) {
    activeColor = previewColor;
    activeBri = previewBrightness;
  } else if (signalRgbActive) {
    activeColor = rgbConfig.signalColor;
  } else if (readyLedBlinking) {
    activeColor = rgbConfig.lapColor;
  } else if (networkReadyState) {
    activeColor = rgbConfig.readyColor;
  }

  if (activeBri > 100) activeBri = 100;
  uint8_t r = (uint8_t)((((activeColor >> 16) & 0xFFU) * activeBri) / 100U);
  uint8_t g = (uint8_t)((((activeColor >> 8) & 0xFFU) * activeBri) / 100U);
  uint8_t b = (uint8_t)(((activeColor & 0xFFU) * activeBri) / 100U);
  writeRawRgb(r, g, b);
}

void loadRgbConfigFromFS() {
  PlatformRuntime::StorageLock lock;
  if (!LittleFS.exists(LED_CONFIG_PATH)) return;
  File f = LittleFS.open(LED_CONFIG_PATH, "r");
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;

  if (doc["ready"].is<uint32_t>()) rgbConfig.readyColor = doc["ready"].as<uint32_t>() & 0xFFFFFFUL;
  if (doc["boot"].is<uint32_t>()) rgbConfig.bootColor = doc["boot"].as<uint32_t>() & 0xFFFFFFUL;
  if (doc["signal"].is<uint32_t>()) rgbConfig.signalColor = doc["signal"].as<uint32_t>() & 0xFFFFFFUL;
  if (doc["lap"].is<uint32_t>()) rgbConfig.lapColor = doc["lap"].as<uint32_t>() & 0xFFFFFFUL;
  if (doc["brightness"].is<uint8_t>()) {
    uint8_t bri = doc["brightness"].as<uint8_t>();
    rgbConfig.brightness = (bri <= 100) ? bri : 100;
  }
}

bool saveRgbConfigToFS() {
  PlatformRuntime::StorageLock lock;
  File f = LittleFS.open(LED_CONFIG_PATH, "w");
  if (!f) return false;
  JsonDocument doc;
  doc["ready"] = rgbConfig.readyColor;
  doc["boot"] = rgbConfig.bootColor;
  doc["signal"] = rgbConfig.signalColor;
  doc["lap"] = rgbConfig.lapColor;
  doc["brightness"] = rgbConfig.brightness;
  size_t written = serializeJson(doc, f);
  f.close();
  return written > 0;
}
}  // namespace

void LapStatusLeds::init() {
  pinMode(LED_READY, OUTPUT);
  pinMode(LED_SIGNAL, OUTPUT);

  digitalWrite(LED_READY, LOW);
  digitalWrite(LED_SIGNAL, LOW);

  loadRgbConfigFromFS();
  applyRgbState();
}

void LapStatusLeds::pulseReady() {
  digitalWrite(LED_READY, LOW);
  readyLedBlinking = true;
  readyLedOffTime = millis() + 200;
  applyRgbState();
}

void LapStatusLeds::cancelReadyPulse() {
  readyLedBlinking = false;
  applyRgbState();
}

void LapStatusLeds::setNetworkReady(bool ready) {
  networkReadyState = ready;
  digitalWrite(LED_READY, ready ? HIGH : LOW);
  applyRgbState();
}

void LapStatusLeds::update() {
  uint32_t now = millis();

  if (digitalRead(LED_SIGNAL) == HIGH) {
    if (!signalRgbActive) {
      signalRgbActive = true;
      signalRgbUntilMs = now + SIGNAL_FLASH_RGB_MS;
    }
    if (now - lastYellowFlash > 12) {
      digitalWrite(LED_SIGNAL, LOW);
      lastYellowFlash = now;
    }
  } else {
    lastYellowFlash = now;
  }

  if (signalRgbActive && (int32_t)(now - signalRgbUntilMs) >= 0) {
    signalRgbActive = false;
  }

  if (previewActive && (int32_t)(now - previewUntilMs) >= 0) {
    previewActive = false;
  }

  if (readyLedBlinking && (int32_t)(now - readyLedOffTime) >= 0) {
    digitalWrite(LED_READY, HIGH);
    readyLedBlinking = false;
  }

  applyRgbState();
}

bool LapStatusLeds::hasRgbLed() {
  return true;
}

LapStatusLeds::RgbLedConfig LapStatusLeds::getRgbConfig() {
  return rgbConfig;
}

bool LapStatusLeds::setRgbConfig(const RgbLedConfig& cfg, bool persist) {
  rgbConfig.readyColor = cfg.readyColor & 0xFFFFFFUL;
  rgbConfig.bootColor = cfg.bootColor & 0xFFFFFFUL;
  rgbConfig.signalColor = cfg.signalColor & 0xFFFFFFUL;
  rgbConfig.lapColor = cfg.lapColor & 0xFFFFFFUL;
  rgbConfig.brightness = (cfg.brightness <= 100) ? cfg.brightness : 100;
  previewActive = false;
  applyRgbState();
  return persist ? saveRgbConfigToFS() : true;
}

void LapStatusLeds::previewRgbColor(uint32_t color, uint8_t brightness, uint16_t durationMs) {
  previewColor = color & 0xFFFFFFUL;
  previewBrightness = (brightness <= 100) ? brightness : 100;
  previewUntilMs = millis() + durationMs;
  previewActive = true;
  applyRgbState();
}

void LapStatusLeds::triggerSignalFlash() {
  signalRgbActive = true;
  signalRgbUntilMs = millis() + SIGNAL_FLASH_RGB_MS;
  applyRgbState();
}
