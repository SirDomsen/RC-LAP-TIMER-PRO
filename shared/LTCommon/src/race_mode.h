#ifndef LT_RACE_MODE_H
#define LT_RACE_MODE_H

enum RaceMode {
  MODE_PRACTICE,   // Freies Training
  MODE_QUALIFYING, // Qualifying (Zeitbasiert)
  MODE_RACE_LAPS,  // Rennen (Rundenbasiert)
  MODE_RACE_TIME,  // Rennen (Zeitbasiert)
  MODE_DRAGRACE,   // Dragrace-Modus (Modus 4)
  MODE_GHOST_HUNT  // Geisterjagd / Ghost Hunt (Modus 5)
};

#endif
