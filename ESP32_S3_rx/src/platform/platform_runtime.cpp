#include <Arduino.h>
#include "platform_runtime.h"

namespace PlatformRuntime {

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

}
