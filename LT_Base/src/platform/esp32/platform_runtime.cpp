#include <Arduino.h>
#include <esp_attr.h>
#include <esp_freertos_hooks.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
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

static SemaphoreHandle_t sStorageMutex = nullptr;
static portMUX_TYPE sLoadMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t sTotalTicks[2] = {0, 0};
static volatile uint32_t sIdleTicks[2] = {0, 0};
static volatile bool sIdleThisTick[2] = {false, false};
static uint8_t sCoreLoadPct[2] = {0, 0};
static uint32_t sLastLoadSampleMs = 0;

static bool IRAM_ATTR onIdleCore0() {
  if (!sIdleThisTick[0]) {
    sIdleThisTick[0] = true;
    sIdleTicks[0]++;
  }
  return false;
}

static bool IRAM_ATTR onIdleCore1() {
  if (!sIdleThisTick[1]) {
    sIdleThisTick[1] = true;
    sIdleTicks[1]++;
  }
  return false;
}

static void IRAM_ATTR onTickCore0() {
  sTotalTicks[0]++;
  sIdleThisTick[0] = false;
}

static void IRAM_ATTR onTickCore1() {
  sTotalTicks[1]++;
  sIdleThisTick[1] = false;
}

} // namespace

void init() {
  setCpuFrequencyMhz(240);
  if (sStorageMutex == nullptr) {
    sStorageMutex = xSemaphoreCreateRecursiveMutex();
  }
  esp_register_freertos_idle_hook_for_cpu(onIdleCore0, 0);
  esp_register_freertos_idle_hook_for_cpu(onIdleCore1, 1);
  esp_register_freertos_tick_hook_for_cpu(onTickCore0, 0);
  esp_register_freertos_tick_hook_for_cpu(onTickCore1, 1);
  sLastLoadSampleMs = millis();
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

uint32_t getMinFreeHeapBytes() {
  return ESP.getMinFreeHeap();
}

uint32_t getMaxAllocHeapBytes() {
  return ESP.getMaxAllocHeap();
}

size_t getFreePsramBytes() {
  return ESP.getFreePsram();
}

size_t getTotalPsramBytes() {
  return ESP.getPsramSize();
}

size_t getMinFreePsramBytes() {
  return ESP.getMinFreePsram();
}

float getChipTemperatureC() {
  float t = temperatureRead();
  return isfinite(t) ? t : NAN;
}

uint32_t getLoopStackHighWaterBytes() {
  return (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
}

uint16_t getTaskCount() {
  return (uint16_t)uxTaskGetNumberOfTasks();
}

int8_t getCoreLoadPercent(uint8_t core) {
  if (core > 1) return -1;
  uint32_t now = millis();
  if (now - sLastLoadSampleMs >= 500) {
    uint32_t tot[2];
    uint32_t idl[2];
    portENTER_CRITICAL(&sLoadMux);
    tot[0] = sTotalTicks[0];
    idl[0] = sIdleTicks[0];
    tot[1] = sTotalTicks[1];
    idl[1] = sIdleTicks[1];
    sTotalTicks[0] = 0;
    sIdleTicks[0] = 0;
    sTotalTicks[1] = 0;
    sIdleTicks[1] = 0;
    portEXIT_CRITICAL(&sLoadMux);
    sLastLoadSampleMs = now;
    for (uint8_t i = 0; i < 2; i++) {
      if (tot[i] > 0) {
        uint32_t clampedIdle = (idl[i] > tot[i]) ? tot[i] : idl[i];
        sCoreLoadPct[i] = (uint8_t)(((tot[i] - clampedIdle) * 100UL) / tot[i]);
      }
    }
  }
  return (int8_t)sCoreLoadPct[core];
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

uint16_t getStorageWriteCooldownMs() {
  return 15;
}

bool supportsPsramFileCache() {
  return psramFound();
}

void* allocLargeBuffer(size_t size) {
  if (size == 0) size = 1;
  if (psramFound()) {
    void* p = ps_malloc(size);
    if (p) return p;
  }
  return malloc(size);
}

void* reallocLargeBuffer(void* ptr, size_t size) {
  if (size == 0) {
    free(ptr);
    return nullptr;
  }
  if (psramFound()) {
    void* p = ptr ? ps_realloc(ptr, size) : ps_malloc(size);
    if (p) return p;
  }
  return realloc(ptr, size);
}

void freeLargeBuffer(void* ptr) {
  free(ptr);
}

StorageLock::StorageLock() {
  if (sStorageMutex != nullptr) {
    xSemaphoreTakeRecursive(sStorageMutex, portMAX_DELAY);
  }
}

StorageLock::~StorageLock() {
  if (sStorageMutex != nullptr) {
    xSemaphoreGiveRecursive(sStorageMutex);
  }
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
