// focus.h - Pomodoro study timer (work / break) shown on the OLED.
#pragma once

enum FocusMode { FOCUS_OFF, FOCUS_WORK, FOCUS_BREAK };

void focusStartWork(int workMin, int breakMin);   // breakMin 0 = no automatic break
void focusStartBreak(int breakMin);
void focusStop();
void focusUpdate(float distCm, bool rearBlocked); // call every loop
FocusMode focusMode();
const char* focusModeName();                      // "off" / "work" / "break"
unsigned long focusSecondsLeft();
bool focusWorking();                              // true during work time (robot stays calm)

#include <Arduino.h>
String focusJson();                               // presence, score, threshold (Mind tab, G1b)
unsigned long focusStudiedMinutes();              // study time since boot (G1c)
