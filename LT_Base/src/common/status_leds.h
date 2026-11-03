#ifndef LAP_STATUS_LEDS_H
#define LAP_STATUS_LEDS_H

namespace LapStatusLeds {

void init();
void pulseReady();
void cancelReadyPulse();
void setNetworkReady(bool ready);
void update();

}  // namespace LapStatusLeds

#endif
