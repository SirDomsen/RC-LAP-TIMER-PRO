#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>
#include "platform_runtime.h"
#include "hardware_config.h"

namespace PlatformRuntime {

namespace {

struct RtcClockBlock {
  uint32_t magic;
  uint32_t epochSec;
  int32_t tzSec;
  uint32_t check;
};

struct RtcStepBlock {
  uint32_t magic;
  uint32_t step;
  uint32_t freeHeap;
  uint32_t check;
};

constexpr uint32_t RTC_CLOCK_MAGIC = 0x4C54434BUL;
constexpr uint32_t RTC_STEP_MAGIC  = 0x4C445354UL;

// RTC_NOINIT behaelt Diagnosedaten ueber Software-Resets, Panics und Watchdogs hinweg.
RTC_NOINIT_ATTR static RtcClockBlock rtcClockData;
RTC_NOINIT_ATTR static RtcStepBlock rtcStepData;

uint32_t calcRtcClockCheck(const RtcClockBlock& b) {
  return b.magic ^ b.epochSec ^ (uint32_t)b.tzSec ^ 0xA5A5A5A5UL;
}

uint32_t calcRtcStepCheck(const RtcStepBlock& b) {
  return b.magic ^ b.step ^ b.freeHeap ^ 0x5A5A5A5AUL;
}

} // namespace

void init() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  uint32_t cdcWait = millis();
  while (!Serial && (millis() - cdcWait < 1500)) {
    delay(10);
  }
}

void logSystemMetrics(uint32_t maxLoopUs) {
  Serial.printf("[SYSTEM] Freier Heap: %u Bytes | PSRAM: %u Bytes | Max Loop: %u us\n",
                ESP.getFreeHeap(), ESP.getFreePsram(), maxLoopUs);
}

bool isHeapCritical(size_t minimumFreeBytes, uint8_t, size_t) {
  return ESP.getFreeHeap() < minimumFreeBytes;
}

bool isHeapCriticalForFileTransfer(uint8_t) {
  return isHeapCritical(12000, 100, 0);
}

bool isHeapCriticalForLiveData() {
  return isHeapCritical(8000, 100, 0);
}

bool canAcceptWebSocket(size_t currentCount, uint8_t) {
  return (currentCount <= PLATFORM_MAX_WS_SESSIONS) && !isHeapCritical(8000, 100, 0);
}

uint8_t getHeapFragmentationPercent() {
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap == 0) return 100;
  uint32_t maxBlock = ESP.getMaxAllocHeap();
  if (maxBlock >= freeHeap) return 0;
  return (uint8_t)(((uint64_t)(freeHeap - maxBlock) * 100ULL) / freeHeap);
}

size_t getFreePsramBytes() {
  return ESP.getFreePsram();
}

uint16_t getChipIdSuffix16() {
  return (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
}

void feedWatchdog() {
  // Auf ESP32-S3 laeuft AsyncWebServer in eigenem FreeRTOS-Task; hier kein yield() ausloesen.
}

const char* getPlatformId() {
  return "esp32s3";
}

const char* getPlatformName() {
  return "ESP32-S3";
}

bool isHighPerfPlatform() {
  return true;
}

ResetDiagnosis readBootResetDiagnosis() {
  esp_reset_reason_t r = esp_reset_reason();
  ResetDiagnosis diag;
  diag.isCrash = false;
  diag.tag = "[START]";

  switch (r) {
    case ESP_RST_POWERON:
      diag.reason = "Power On";
      diag.tag = "[POWER-ON]";
      rtcClockData.magic = 0;
      rtcStepData.magic = 0;
      break;
    case ESP_RST_EXT:
      diag.reason = "External Reset";
      diag.tag = "[EXT-RESET]";
      break;
    case ESP_RST_SW:
      diag.reason = "Software Reset";
      diag.tag = "[RESTART]";
      break;
    case ESP_RST_PANIC:
      diag.reason = "Exception/Panic";
      diag.isCrash = true;
      diag.tag = "[CRASH ⚠️]";
      break;
    case ESP_RST_INT_WDT:
      diag.reason = "Interrupt Watchdog";
      diag.isCrash = true;
      diag.tag = "[CRASH ⚠️]";
      break;
    case ESP_RST_TASK_WDT:
      diag.reason = "Task Watchdog";
      diag.isCrash = true;
      diag.tag = "[CRASH ⚠️]";
      break;
    case ESP_RST_WDT:
      diag.reason = "Watchdog Reset";
      diag.isCrash = true;
      diag.tag = "[CRASH ⚠️]";
      break;
    case ESP_RST_DEEPSLEEP:
      diag.reason = "Deep Sleep Wake";
      break;
    case ESP_RST_BROWNOUT:
      diag.reason = "Brownout (Spannungseinbruch)";
      diag.isCrash = true;
      diag.tag = "[CRASH ⚠️]";
      break;
    case ESP_RST_SDIO:
      diag.reason = "SDIO Reset";
      break;
    default:
      diag.reason = "Reset Code " + String((int)r);
      break;
  }

  diag.info = "Code " + String((int)r) +
              " | Heap " + String(ESP.getFreeHeap()) +
              " | PSRAM " + String(ESP.getFreePsram());
  return diag;
}

void saveRtcClock(uint32_t epochSec, int32_t tzSec) {
  if (epochSec == 0) return;
  rtcClockData.magic = RTC_CLOCK_MAGIC;
  rtcClockData.epochSec = epochSec;
  rtcClockData.tzSec = tzSec;
  rtcClockData.check = calcRtcClockCheck(rtcClockData);
}

bool loadRtcClock(RtcClockSnapshot& out) {
  if (rtcClockData.magic != RTC_CLOCK_MAGIC ||
      rtcClockData.check != calcRtcClockCheck(rtcClockData)) {
    return false;
  }
  out.epochSec = rtcClockData.epochSec;
  out.tzSec = rtcClockData.tzSec;
  return true;
}

void saveDeleteStep(uint32_t step, uint32_t freeHeap) {
  rtcStepData.magic = RTC_STEP_MAGIC;
  rtcStepData.step = step;
  rtcStepData.freeHeap = freeHeap;
  rtcStepData.check = calcRtcStepCheck(rtcStepData);
}

bool loadDeleteStep(RtcDeleteStepSnapshot& out) {
  if (rtcStepData.magic != RTC_STEP_MAGIC ||
      rtcStepData.check != calcRtcStepCheck(rtcStepData) ||
      rtcStepData.step == 0) {
    return false;
  }
  out.step = rtcStepData.step;
  out.freeHeap = rtcStepData.freeHeap;
  return true;
}

}
