// robot.h - things main.cpp offers to other modules (the web server).
#pragma once
#include "emotion.h"

// Show an emotion on the face for ms milliseconds, on top of the mood
// (used by the phone: "thinking" while waiting for the AI, "talking" while
// speaking, the reply's emotion tag). beep = play that emotion's rhythm.
// why = optional reason for the Mind tab (default: "that is how my answer felt").
void robotShowEmotion(Emotion e, unsigned long ms, bool beep, const char* why = nullptr);

Emotion robotFaceEmotion();     // what the face shows right now
const char* robotEmotionReason();   // why, in words (Mind tab)

// Ask the phone to say a short line through Buddy's voice (proactive lines,
// focus nudges). The phone picks it up from /mind ("say": id + text).
void robotSay(const char* text);
unsigned long robotSayId();
const char* robotSayText();
