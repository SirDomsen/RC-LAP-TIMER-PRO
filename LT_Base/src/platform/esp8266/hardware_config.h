#ifndef ESP8266_HARDWARE_CONFIG_H
#define ESP8266_HARDWARE_CONFIG_H

#ifdef BOARD_ESP12F_IR
// ESP-12F IR-Transceiver-Board (Type-C)
#define IR_PIN      14          // GPIO14 - IR-Empfaenger
#define LED_ONBOARD LED_BUILTIN // GPIO2  - interne Status-LED (Active LOW, Ready-Funktion)
#define LED_SIGNAL  0xFF        // Deaktiviert (keine zweite Signal-LED)
// LED_READY und SPEAKER_PIN bewusst nicht definiert (GPIO4 = ungenutzter IR-TX, kein Speaker)
#else
// ESP8266-Basisstation (NodeMCU V2/V3)
#define IR_PIN      D2          // GPIO4 - TSOP4838-Datenpin
#define LED_READY   D5          // GPIO14 - grüne Ready-LED
#define LED_SIGNAL  D6          // GPIO12 - gelbe Signal-LED
#define LED_ONBOARD LED_BUILTIN // GPIO2 - interne Status-LED (Active LOW)
#define SPEAKER_PIN D1          // GPIO5 - Arcade-Speaker via BC337
#endif

// --- Plattform-Kapazitaeten (ESP8266 RAM-optimiert) ---
#define PLATFORM_MAX_AP_CLIENTS       4
#define PLATFORM_MAX_WS_SESSIONS      4
#define PLATFORM_MAX_STATIC_SLOTS     2
#define PLATFORM_MAX_DEBUG_PACKETS    8
#define PLATFORM_LAP_QUEUE_MAX        20
#define PLATFORM_DELETE_QUEUE_MAX     4
#define PLATFORM_LAPS_CHUNK_BUF_SIZE  560

#endif
