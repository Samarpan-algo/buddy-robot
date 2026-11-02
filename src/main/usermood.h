// usermood.h - G1c: how the USER seems (calm / happy / stressed / tired / confused).
#pragma once
#include <Arduino.h>
#include "mood.h"

void usermoodFromAI(const char* mood, const char* why);   // AI tag [USER:x] + short quote
void usermoodOnEvent(MoodEvent ev);                        // quiz, pets, scares as extra evidence
void usermoodUpdate(unsigned long studiedMin);             // call every loop (decay, study time)
const char* usermood();                                    // current guess
String usermoodJson();                                     // {"mood":"...","evidence":[...]}
