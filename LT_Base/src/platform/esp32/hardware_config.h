#ifndef ESP32_S3_HARDWARE_CONFIG_H
#define ESP32_S3_HARDWARE_CONFIG_H

// ESP32-S3-Basisstation (z. B. N16R8)
#define IR_PIN      4           // GPIO4 - TSOP4838-Datenpin
#define LED_READY   5           // GPIO5 - grüne Ready-LED
#define LED_SIGNAL  6           // GPIO6 - gelbe Signal-LED
#define SPEAKER_PIN 7           // GPIO7 - Arcade-Speaker via BC337
#ifdef LED_BUILTIN
  #define LED_ONBOARD LED_BUILTIN
#else
  #define LED_ONBOARD 2         // Fallback, falls LED_BUILTIN nicht definiert ist
#endif

// --- Plattform-Kapazitaeten (ESP32-S3 freigeschaltet) ---
#define PLATFORM_MAX_AP_CLIENTS       8
#define PLATFORM_MAX_WS_SESSIONS      12
#define PLATFORM_MAX_STATIC_SLOTS     8
#define PLATFORM_MAX_DEBUG_PACKETS    32
#define PLATFORM_LAP_QUEUE_MAX        64
#define PLATFORM_DELETE_QUEUE_MAX     16
#define PLATFORM_LAPS_CHUNK_BUF_SIZE  1024

#endif
