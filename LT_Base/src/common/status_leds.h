#ifndef LAP_STATUS_LEDS_H
#define LAP_STATUS_LEDS_H

#include <stdint.h>

namespace LapStatusLeds {

struct RgbLedConfig {
  uint32_t readyColor = 0x00FF00;   // Netzwerk bereit (Standard: Gruen)
  uint32_t bootColor = 0x000000;    // Boot / WLAN-Suche (Standard: Aus)
  uint32_t signalColor = 0xFFCC00;  // IR-Rohsignal (Standard: Gelb)
  uint32_t lapColor = 0x000000;     // Runden-Puls 200 ms (Standard: Aus)
  uint8_t brightness = 20;          // 0..100 %
};

void init();
void pulseReady();
void cancelReadyPulse();
void setNetworkReady(bool ready);
void update();

bool hasRgbLed();
RgbLedConfig getRgbConfig();
bool setRgbConfig(const RgbLedConfig& cfg, bool persist = true);
void previewRgbColor(uint32_t color, uint8_t brightness, uint16_t durationMs = 1500);
void triggerSignalFlash();

}  // namespace LapStatusLeds

#endif
