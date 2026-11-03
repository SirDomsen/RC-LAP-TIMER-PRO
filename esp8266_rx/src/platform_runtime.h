#ifndef PLATFORM_RUNTIME_H
#define PLATFORM_RUNTIME_H

#include <Arduino.h>

namespace PlatformRuntime {
void init();
void logSystemMetrics(uint32_t maxLoopUs);
}

#endif
