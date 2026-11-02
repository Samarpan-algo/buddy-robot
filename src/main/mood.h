// mood.h - Buddy's feelings (mood engine) and long-term memory.
#pragma once
#include "emotion.h"

// Things that can happen to Buddy (also stored in the event memory)
enum MoodEvent {
  EV_BOOT, EV_PET, EV_SUDDEN_CLOSE, EV_SLOW_APPROACH,
  EV_IDLE_SAD, EV_IDLE_SLEEPY, EV_PRAISE, EV_CHAT_START,
  EV_QUIZ_CORRECT, EV_QUIZ_WRONG, EV_QUIZ_STREAK,   // study mode (F2)
  EV_FOCUS_START, EV_FOCUS_END,
  EV_LOUD_NOISE, EV_HEARD_VOICE                      // "Buddy hears" (mic as sound sensor)
};

struct MoodState {
  float happy, fear, curious, energy;   // 0-100, change quickly
  int   bond;                           // 0-100, long-term attachment (saved)
  unsigned long totalPets, totalChats;  // counters (saved)
};

void moodInit();                    // load bond + counters from NVS
void moodEvent(MoodEvent ev);       // something happened
void moodUpdate();                  // call every loop (decays once per second)
Emotion moodEmotion();              // strongest emotion right now (priority rules)
const MoodState& moodState();
unsigned long moodIdleSeconds();    // seconds since the last interaction
void moodSkipIdle();                // test: jump to the next idle stage
void moodSaveNow();                 // test: write bond + counters to NVS now
void moodPrintEvents();             // print the last 20 events
void moodSetCalm(bool calm);        // focus time: no idle sad/sleepy while true

// Write the newest n events as JSON, e.g. [{"e":"pet","ago_s":12},...]
void moodRecentEventsJson(char* buf, size_t len, int n);

const char* moodEventText(MoodEvent e);                 // "someone petted me" ...
bool moodLastEvent(MoodEvent* ev, unsigned long* agoS);  // newest event (false if none)
void moodNudge(float dHappy, float dCurious);            // small push (G1c: follow the user's mood)
