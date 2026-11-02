// eyes.h - animated eyes on the 1.3" SH1106 OLED (FluxGarage RoboEyes).
#pragma once
#include "emotion.h"

void eyesInit();                  // start the display and the eyes
void eyesSetEmotion(Emotion e);   // change the expression
void eyesUpdate();                // call every loop (draws when it is time)

// Show up to 3 lines of text instead of the eyes for ms milliseconds
// (used for the IP address at boot). The eyes come back by themselves.
void eyesShowText(const char* line1, const char* line2, const char* line3, unsigned long ms);

// Big MM:SS countdown with a small title (Pomodoro). Call it once per second;
// the eyes come back about 1.5 s after the last call.
void eyesShowCountdown(const char* title, unsigned long secondsLeft);

// A small title and one big word/number (e.g. "FOCUS SCORE" / "85%") for ms milliseconds
void eyesShowBig(const char* title, const char* big, unsigned long ms);
