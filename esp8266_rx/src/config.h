#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Versionsnummern ---
#define FIRMWARE_VERSION  "v4.4.0"
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
  #define IR_PIN      3           // GPIO3 (RX) für den TSOP-Empfänger auf dem ESP-01S
  #define LED_SIGNAL  2           // GPIO2 für die blaue Onboard-LED (Active LOW)
  #define LED_ONBOARD 2           // GPIO2
#else
  #define IR_PIN      D2          // GPIO4  - TSOP4838 Data Pin 
  #define LED_READY   D5          // GPIO14 - Grüne Ready-LED 
  #define LED_SIGNAL  D6          // GPIO12 - Gelbe Signal-LED 
  #define LED_ONBOARD LED_BUILTIN // GPIO2  - Interne ESP Status LED (Active LOW) 
  #define SPEAKER_PIN D1          // GPIO5  - Arcade Sound / BC337 Transistor Speaker
#endif

// --- Renn- & EEPROM Parameter ---
#define MAX_CARS            15
#define DEFAULT_MIN_LAP_MS  2000    // Standard-Entprellung (2 Sekunden)
#define PIT_PAUSE_FACTOR_PERCENT 250 // Schwellenwert für dynamische Boxen-/Pausen-/Outlap-Erkennung (250% von AvgLap)
#define EEPROM_MAGIC        0xAC    // Magic Byte
#define EEPROM_SIZE         1024
#define NAME_LENGTH         20      // Max. Zeichen pro Fahrername

#endif
