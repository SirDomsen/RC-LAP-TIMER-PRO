#ifndef PLATFORM_RUNTIME_H
#define PLATFORM_RUNTIME_H

#include <Arduino.h>

namespace PlatformRuntime {

struct ResetDiagnosis {
  String reason;
  String info;
  bool isCrash;
  const char* tag;
};

struct RtcClockSnapshot {
  uint32_t epochSec;
  int32_t tzSec;
};

struct RtcDeleteStepSnapshot {
  uint32_t step;
  uint32_t freeHeap;
};

void init();
void logSystemMetrics(uint32_t maxLoopUs);
bool isHeapCritical(size_t minimumFreeBytes, uint8_t maximumFragmentation, size_t minimumFreeBlockBytes);
bool isHeapCriticalForFileTransfer(uint8_t wifiMode);
bool isHeapCriticalForLiveData();
bool canAcceptWebSocket(size_t currentCount, uint8_t wifiMode);
uint8_t getHeapFragmentationPercent();
size_t getFreePsramBytes();
uint16_t getChipIdSuffix16();
void feedWatchdog();
const char* getPlatformId();
const char* getPlatformName();
bool isHighPerfPlatform();
uint16_t getStorageWriteCooldownMs();
bool supportsPsramFileCache();
void* allocLargeBuffer(size_t size);
void* reallocLargeBuffer(void* ptr, size_t size);
void freeLargeBuffer(void* ptr);

class StorageLock {
 public:
  StorageLock();
  ~StorageLock();
  StorageLock(const StorageLock&) = delete;
  StorageLock& operator=(const StorageLock&) = delete;
};

ResetDiagnosis readBootResetDiagnosis();
void saveRtcClock(uint32_t epochSec, int32_t tzSec);
bool loadRtcClock(RtcClockSnapshot& out);
void saveDeleteStep(uint32_t step, uint32_t freeHeap);
bool loadDeleteStep(RtcDeleteStepSnapshot& out);

}

#endif
