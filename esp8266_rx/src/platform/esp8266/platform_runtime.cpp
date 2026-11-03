#include <Arduino.h>
#include <user_interface.h>
#include "platform_runtime.h"

namespace PlatformRuntime {

void init() {
  system_update_cpu_freq(160);
  Serial.begin(115200);
  delay(100);
}

void logSystemMetrics(uint32_t maxLoopUs) {
  Serial.printf("[SYSTEM] Freier Heap: %u Bytes | Fragmentierung: %u%% | Max Loop: %u us\n",
                ESP.getFreeHeap(), ESP.getHeapFragmentation(), maxLoopUs);
}

}
