#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Versionsnummern ---
#define FIRMWARE_VERSION  "v1.3.1"  //xiaomi-fix
#define UI_VERSION        "v1.3.0"

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
#define IR_PIN      D2          // GPIO4  - TSOP4838 Data Pin
#define LED_READY   D5          // GPIO14 - Grüne Ready-LED
#define LED_SIGNAL  D6          // GPIO12 - Gelbe Signal-LED
#define LED_ONBOARD LED_BUILTIN // GPIO2  - Interne ESP Status LED (Active LOW)

// --- Renn- & EEPROM Parameter ---
#define MAX_CARS            15
#define DEFAULT_MIN_LAP_MS  2000    // Standard-Entprellung (2 Sekunden)
#define EEPROM_MAGIC        0xAC    // Magic Byte
#define EEPROM_SIZE         1024
#define NAME_LENGTH         20      // Max. Zeichen pro Fahrername

#endif