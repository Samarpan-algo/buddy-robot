// thoughts.h - "Buddy's thoughts": one human-readable line per decision,
// shown in the phone's Mind tab and printed on Serial.
#pragma once
#include <Arduino.h>

// Add a thought, printf-style, e.g. think("Chose WIGGLE (score %.2f)", q);
void think(const char* fmt, ...);

// JSON array of the last thoughts, oldest first: [{"t":"...","ago_s":3},...]
String thoughtsJson();
