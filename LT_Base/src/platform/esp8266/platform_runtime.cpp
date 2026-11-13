#include <Arduino.h>
#include <user_interface.h>
#include "platform_runtime.h"

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
};

constexpr uint32_t RTC_CLOCK_MAGIC = 0x4C54434BUL;
constexpr uint32_t RTC_CLOCK_SLOT  = 64;
constexpr uint32_t RTC_STEP_MAGIC  = 0x4C445354UL;
constexpr uint32_t RTC_STEP_SLOT   = 72;

uint32_t calcRtcClockCheck(const RtcClockBlock& b) {
  return b.magic ^ b.epochSec ^ (uint32_t)b.tzSec ^ 0xA5A5A5A5UL;
}

} // namespace

void init() {
  system_update_cpu_freq(160);
  Serial.begin(115200);
  delay(100);
}

void logSystemMetrics(uint32_t maxLoopUs) {
  Serial.printf("[SYSTEM] Freier Heap: %u Bytes | Fragmentierung: %u%% | Max Loop: %u us\n",
                ESP.getFreeHeap(), ESP.getHeapFragmentation(), maxLoopUs);
}

bool isHeapCritical(size_t minimumFreeBytes, uint8_t maximumFragmentation, size_t minimumFreeBlockBytes) {
  return (ESP.getFreeHeap() < minimumFreeBytes ||
          ESP.getHeapFragmentation() > maximumFragmentation ||
          ESP.getMaxFreeBlockSize() < minimumFreeBlockBytes);
}

bool isHeapCriticalForFileTransfer(uint8_t wifiMode) {
  return isHeapCritical(
    wifiMode == 0 ? 5500 : 7500,
    wifiMode == 0 ? 75 : 70,
    wifiMode == 0 ? 1400 : 1800);
}

bool isHeapCriticalForLiveData() {
  return isHeapCritical(3500, 100, 0);
}

bool canAcceptWebSocket(size_t currentCount, uint8_t wifiMode) {
  size_t maxWs = (wifiMode == 0) ? 3 : 4;
  return (currentCount <= maxWs) && !isHeapCritical(4000, 100, 0);
}

uint8_t getHeapFragmentationPercent() {
  return ESP.getHeapFragmentation();
}

size_t getFreePsramBytes() {
  return 0;
}

uint16_t getChipIdSuffix16() {
  return (uint16_t)(ESP.getChipId() & 0xFFFF);
}

void feedWatchdog() {
  ESP.wdtFeed();
}

ResetDiagnosis readBootResetDiagnosis() {
  ResetDiagnosis diag;
  diag.reason = ESP.getResetReason();
  diag.info = ESP.getResetInfo();

  String rLower = diag.reason;
  rLower.toLowerCase();
  diag.isCrash = (rLower.indexOf("watchdog") >= 0 ||
                  rLower.indexOf("exception") >= 0 ||
                  rLower.indexOf("fatal") >= 0);

  if (diag.isCrash) {
    diag.tag = "[CRASH ⚠️]";
  } else if (rLower.indexOf("power") >= 0) {
    diag.tag = "[POWER-ON]";
  } else if (rLower.indexOf("restart") >= 0) {
    diag.tag = "[RESTART]";
  } else if (rLower.indexOf("external") >= 0) {
    diag.tag = "[EXT-RESET]";
  } else {
    diag.tag = "[START]";
  }
  return diag;
}

void saveRtcClock(uint32_t epochSec, int32_t tzSec) {
  if (epochSec == 0) return;
  RtcClockBlock b{RTC_CLOCK_MAGIC, epochSec, tzSec, 0};
  b.check = calcRtcClockCheck(b);
  ESP.rtcUserMemoryWrite(RTC_CLOCK_SLOT, (uint32_t*)&b, sizeof(b));
}

bool loadRtcClock(RtcClockSnapshot& out) {
  RtcClockBlock b{};
  if (!ESP.rtcUserMemoryRead(RTC_CLOCK_SLOT, (uint32_t*)&b, sizeof(b))) return false;
  if (b.magic != RTC_CLOCK_MAGIC || b.check != calcRtcClockCheck(b)) return false;
  out.epochSec = b.epochSec;
  out.tzSec = b.tzSec;
  return true;
}

void saveDeleteStep(uint32_t step, uint32_t freeHeap) {
  RtcStepBlock b{RTC_STEP_MAGIC, step, freeHeap};
  ESP.rtcUserMemoryWrite(RTC_STEP_SLOT, (uint32_t*)&b, sizeof(b));
}

bool loadDeleteStep(RtcDeleteStepSnapshot& out) {
  RtcStepBlock b{};
  if (!ESP.rtcUserMemoryRead(RTC_STEP_SLOT, (uint32_t*)&b, sizeof(b))) return false;
  if (b.magic != RTC_STEP_MAGIC || b.step == 0) return false;
  out.step = b.step;
  out.freeHeap = b.freeHeap;
  return true;
}

}
