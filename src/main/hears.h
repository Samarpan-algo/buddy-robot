// hears.h - "Buddy hears": the INMP441 mic used as a SOUND SENSOR (not for words).
#pragma once

enum HearEvent { HE_NONE = 0, HE_LOUD = 1, HE_VOICE = 2 };

void hearsInit();
// Call every loop. ignore = true while Buddy speaks or moves (it must not hear itself).
// someoneInFront = front distance < 60 cm. Returns HearEvent flags.
int  hearsUpdate(bool ignore, bool someoneInFront);
int  hearsSharpLevel();    // high-pass filtered level (claps, bangs)
int  hearsRawLevel();      // overall loudness (talking near the robot)
