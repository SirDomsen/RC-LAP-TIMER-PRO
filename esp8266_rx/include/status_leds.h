#ifndef LAP_STATUS_LEDS_H
#define LAP_STATUS_LEDS_H

namespace LapStatusLeds {

void init();
void pulseReady();
void cancelReadyPulse();
void update();

}  // namespace LapStatusLeds

#endif
