#pragma once
#include <Arduino.h>

#ifndef TONE_STEP_DEFINED
#define TONE_STEP_DEFINED
struct ToneStep {
  uint16_t freq;      // Frequenz in Hz (0 = Pause)
  uint16_t duration;  // Dauer in Millisekunden
};
#endif

// Original Accolade MS-DOS PC-Speaker Chiptune Datensätze (PROGMEM)
#include "gpc_titleSeq.h"
#include "gpc_menuSeq.h"
#include "TestDrive1_titleSeq.h"
#include "TestDrive_1_menuSeq.h"
#include "td3_themeSeq.h"
#include "td3_radio1Seq.h"
#include "td3_radio2Seq.h"
#include "td3_radio3Seq.h"

struct ArcadeTrack {
  const char* id;
  const char* title;
  const ToneStep* seq;
  uint16_t len;
  bool isExtracted; // true für wav2c extrahierte Tracks mit separaten Rest/Pause-Steps
};

static const ArcadeTrack ARCADE_TRACKS[] = {
  { "gpc_title",  "Grand Prix Circuit - Title Theme",    gpcTitleSeq,          sizeof(gpcTitleSeq) / sizeof(ToneStep),          false },
  { "gpc_menu",   "Grand Prix Circuit - Menu Theme",     gpcMenuSeq,           sizeof(gpcMenuSeq) / sizeof(ToneStep),           false },
  { "td1_title",  "Test Drive 1 - Title Theme",          TestDrive1_titleSeq,  sizeof(TestDrive1_titleSeq) / sizeof(ToneStep),  true  },
  { "td1_menu",   "Test Drive 1 - Menu Beat",            TestDrive_1_menuSeq,  sizeof(TestDrive_1_menuSeq) / sizeof(ToneStep),  true  },
  { "td3_theme",  "Test Drive 3 - Title Theme",          td3MenuSeq,            sizeof(td3MenuSeq) / sizeof(ToneStep),            true  },
  { "td3_radio1", "Test Drive 3 - Radio 1",              td3_radio1Seq,        sizeof(td3_radio1Seq) / sizeof(ToneStep),        true  },
  { "td3_radio2", "Test Drive 3 - Radio 2",              td3_radio2Seq,        sizeof(td3_radio2Seq) / sizeof(ToneStep),        true  },
  { "td3_radio3", "Test Drive 3 - Radio 3",              td3_radio3Seq,        sizeof(td3_radio3Seq) / sizeof(ToneStep),        true  }
};

static const size_t ARCADE_TRACK_COUNT = sizeof(ARCADE_TRACKS) / sizeof(ARCADE_TRACKS[0]);

inline const ArcadeTrack* findArcadeTrack(const String& id) {
  for (size_t i = 0; i < ARCADE_TRACK_COUNT; i++) {
    if (id.equalsIgnoreCase(ARCADE_TRACKS[i].id)) {
      return &ARCADE_TRACKS[i];
    }
  }
  // Fallback Aliases
  if (id.equalsIgnoreCase("gpc") || id.equalsIgnoreCase("grandprix") || id.equalsIgnoreCase("title")) return &ARCADE_TRACKS[0];
  if (id.equalsIgnoreCase("menu")) return &ARCADE_TRACKS[1];
  if (id.equalsIgnoreCase("td1") || id.equalsIgnoreCase("td1_intro")) return &ARCADE_TRACKS[2];
  if (id.equalsIgnoreCase("td3") || id.equalsIgnoreCase("td3_passion") || id.equalsIgnoreCase("td3_menu")) return &ARCADE_TRACKS[4];
  return nullptr;
}
