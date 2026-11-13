#ifndef ESP8266_HARDWARE_CONFIG_H
#define ESP8266_HARDWARE_CONFIG_H

// ESP8266-Basisstation (NodeMCU V2/V3)
#define IR_PIN      D2          // GPIO4 - TSOP4838-Datenpin
#define LED_READY   D5          // GPIO14 - grüne Ready-LED
#define LED_SIGNAL  D6          // GPIO12 - gelbe Signal-LED
#define LED_ONBOARD LED_BUILTIN // GPIO2 - interne Status-LED (Active LOW)
#define SPEAKER_PIN D1          // GPIO5 - Arcade-Speaker via BC337

// --- Plattform-Kapazitaeten (ESP8266 RAM-optimiert) ---
#define PLATFORM_MAX_AP_CLIENTS       4
#define PLATFORM_MAX_WS_SESSIONS      4
#define PLATFORM_MAX_STATIC_SLOTS     2
#define PLATFORM_MAX_DEBUG_PACKETS    8
#define PLATFORM_LAP_QUEUE_MAX        20
#define PLATFORM_DELETE_QUEUE_MAX     4
#define PLATFORM_LAPS_CHUNK_BUF_SIZE  560

#endif
