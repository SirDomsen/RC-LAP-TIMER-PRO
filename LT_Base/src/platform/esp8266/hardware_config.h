#ifndef ESP8266_HARDWARE_CONFIG_H
#define ESP8266_HARDWARE_CONFIG_H

// ESP8266-Basisstation (NodeMCU V2/V3)
#define IR_PIN      D2          // GPIO4 - TSOP4838-Datenpin
#define LED_READY   D5          // GPIO14 - grüne Ready-LED
#define LED_SIGNAL  D6          // GPIO12 - gelbe Signal-LED
#define LED_ONBOARD LED_BUILTIN // GPIO2 - interne Status-LED (Active LOW)
#define SPEAKER_PIN D1          // GPIO5 - Arcade-Speaker via BC337

#endif
