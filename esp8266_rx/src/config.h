#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Versionsnummern ---
#define FIRMWARE_VERSION  "v4.3.3"
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

#ifndef FALLBACK_CHANNEL
  #define FALLBACK_CHANNEL 1
#endif

// --- Hardware Pins (NodeMCU ESP8266) ---
#define IR_PIN        D2    // GPIO4 (TSOP4838 IR-Empfaenger Datensignal)
#define LED_SIGNAL    D6    // GPIO12 (Gelbe Status-LED fuer IR-Impulse)
#define LED_READY     D5    // GPIO14 (Gruene Status-LED fuer Systembereitschaft)

// --- System- & Renn-Konstanten ---
#define MAX_CARS      15    // Maximale Anzahl der Transponder-IDs (1 bis 15)

#endif
