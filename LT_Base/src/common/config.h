#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Versionsnummern ---
#define FIRMWARE_VERSION  "v4.8.7"
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

// --- WLAN Konfiguration ---
#ifndef HOME_SSID
  #define HOME_SSID   "DEIN_WLAN_NAME"
#endif

#ifndef HOME_PASS
  #define HOME_PASS   "DEIN_WLAN_PASSWORT"
#endif

#ifndef AP_SSID
  #define AP_SSID     "RC-LapTimer"
#endif

#ifndef AP_PASS
  #define AP_PASS     "Rennstrecke123"
#endif

// --- Hardware Pin Definitionen ---
#ifdef SECTOR_STATION
  #ifdef BOARD_ESP12F_IR
    #define IR_PIN    14          // GPIO14 für den IR-Empfänger auf dem ESP-12F IR-Board
  #else
    #define IR_PIN    3           // GPIO3 (RX) für den TSOP-Empfänger auf dem ESP-01S
  #endif
  #define LED_SIGNAL  2           // GPIO2 für die blaue Onboard-LED (Active LOW)
  #define LED_ONBOARD 2           // GPIO2
#elif defined(ESP32)
  #include <hardware_config.h>
#else
  #include <hardware_config.h>
#endif

// --- Renn- & EEPROM Parameter ---
#define MAX_CARS                 15
#define DEFAULT_MIN_LAP_MS       5000   // Standard-Entprellung / Mindestrundenzeit (5 Sekunden)
#define DEFAULT_TWO_FRAME_FILTER false  // 2-Frame-IR-Filter (nur indoors nötig)
#define DEFAULT_USE_SZ1          false  // Sektorstation 1 per Default aus
#define DEFAULT_USE_SZ2          false  // Sektorstation 2 per Default aus
#define DEFAULT_USE_SZ3          false  // Sektorstation 3 per Default aus
#define PIT_PAUSE_FACTOR_PERCENT 250    // Schwellenwert für dynamische Boxen-/Pausen-/Outlap-Erkennung (250% von AvgLap)
#define EEPROM_MAGIC        0xAC    // Magic Byte
#define EEPROM_SIZE         1024
#define NAME_LENGTH         20      // Max. Zeichen pro Fahrername

#endif
