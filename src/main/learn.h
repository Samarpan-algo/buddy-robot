// learn.h - G1a: Buddy learns which "social moves" you like (one-step Q-learning).
#pragma once
#include <Arduino.h>
#include "mood.h"

void learnInit();                       // load the Q-table from NVS
// Call every loop. canAct = false during focus, speaking, moving or when scared.
void learnUpdate(float distCm, bool rearBlocked, bool canAct);
void learnOnEvent(MoodEvent ev);        // feedback from what happens (pet, praise, scare ...)
bool learnThumb(int dir);               // phone thumbs-up (+1) / thumbs-down (-1); false if no window open
void learnReset();                      // forget everything
void learnSetActive(bool on);           // learning mode on/off (off at boot)
bool learnActive();
String learnJson(float distCm);         // for the Mind tab
