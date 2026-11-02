// emotion.h - the list of emotions Buddy can show.
// Names start with EMO_ so they never clash with RoboEyes' macros (HAPPY, TIRED ...).
#pragma once
#include <string.h>

enum Emotion {
  EMO_NEUTRAL, EMO_HAPPY, EMO_SAD, EMO_SCARED, EMO_CURIOUS,
  EMO_SLEEPY, EMO_EXCITED, EMO_THINKING, EMO_TALKING,
  EMO_COUNT
};

static const char* const EMOTION_NAMES[] = {
  "neutral", "happy", "sad", "scared", "curious",
  "sleepy", "excited", "thinking", "talking"
};

inline const char* emotionName(Emotion e) {
  return (e >= 0 && e < EMO_COUNT) ? EMOTION_NAMES[e] : "?";
}

// "happy" -> EMO_HAPPY; unknown name -> EMO_COUNT
inline Emotion emotionFromName(const char* name) {
  for (int i = 0; i < EMO_COUNT; i++) {
    if (strcmp(name, EMOTION_NAMES[i]) == 0) return (Emotion)i;
  }
  return EMO_COUNT;
}
