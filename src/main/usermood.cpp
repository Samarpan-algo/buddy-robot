// usermood.cpp - G1c: a simple guess of how the user feels, from several clues.
//
// Each possible mood has a score. Clues add to a score and leave a short
// "evidence" sentence (the last 3 are kept):
//   AI tag from what the user said  -> +2 (the main signal; other moods are halved,
//                                      so the latest thing you said counts most)
//   2 quiz answers wrong in a row   -> confused +1
//   3 quiz answers right in a row   -> happy +1
//   studied more than 25 min today  -> tired +1 (once)
//   petted Buddy                    -> happy +0.5
//   startled Buddy                  -> stressed +0.5
// The mood with the highest score (at least 1) wins, otherwise "calm".
// No new clue for 5 minutes -> back to calm.
// Effects (on the phone): the AI prompt and the quiz difficulty adapt; Buddy's
// own mood follows lightly (happy user -> happier Buddy, stressed -> attentive).

#include "usermood.h"
#include "thoughts.h"

enum UM { UM_CALM, UM_HAPPY, UM_STRESSED, UM_TIRED, UM_CONFUSED, UM_COUNT };
static const char* UM_NAMES[UM_COUNT] = {"calm", "happy", "stressed", "tired", "confused"};
static const char* UM_PLAN[UM_COUNT]  = {"I'll keep going as usual", "let's have some fun",
                                         "I'll keep things calm", "a short break might help",
                                         "I'll explain more simply"};

const unsigned long FORGET_MS = 300000UL;     // 5 min without new clues -> calm
static float score[UM_COUNT];
static char evidence[3][48];
static int evCount = 0;
static int current = UM_CALM;
static unsigned long lastClue = 0;
static int wrongInRow = 0;
static bool tiredNoted = false;

static void addEvidence(const char* text) {
  if (evCount == 3) { memmove(evidence[0], evidence[1], sizeof(evidence[0]) * 2); evCount = 2; }
  snprintf(evidence[evCount++], sizeof(evidence[0]), "%s", text);
  for (char* p = evidence[evCount - 1]; *p; p++) if (*p == '"' || *p == '\\') *p = '\'';
}

static void decide(const char* why) {
  int best = UM_CALM;
  for (int m = 1; m < UM_COUNT; m++) if (score[m] >= 1 && score[m] > score[best]) best = m;
  if (best != current) {
    current = best;
    if (best == UM_CALM) think("You seem calm now");
    else think("You seem %s (%s) - %s", UM_NAMES[best], why, UM_PLAN[best]);
    // Buddy's own mood follows lightly
    if (best == UM_HAPPY) moodNudge(10, 0);
    else if (best != UM_CALM) moodNudge(0, 10);              // stressed / tired / confused: attentive, caring
  }
}

static void clue(int m, float amount, const char* text) {
  score[m] += amount;
  lastClue = millis();
  addEvidence(text);
  decide(text);
}

void usermoodFromAI(const char* mood, const char* why) {
  for (int m = 0; m < UM_COUNT; m++) {
    if (strcmp(mood, UM_NAMES[m]) != 0) continue;
    char text[48];
    if (why && *why) snprintf(text, sizeof(text), "you said '%s'", why);
    else snprintf(text, sizeof(text), "how you talked to me");
    if (m == UM_CALM) {                                        // calm: let the other moods fade
      for (int k = 1; k < UM_COUNT; k++) score[k] *= 0.5f;
      lastClue = millis();
      addEvidence(text);
      decide(text);
    } else {
      // What you say NOW counts more than older clues: halve the other moods first
      for (int k = 1; k < UM_COUNT; k++) if (k != m) score[k] *= 0.5f;
      clue(m, 2.0f, text);
    }
    return;
  }
}

void usermoodOnEvent(MoodEvent ev) {
  switch (ev) {
    case EV_QUIZ_WRONG:
      if (++wrongInRow >= 2) { clue(UM_CONFUSED, 1.0f, "2 quiz answers wrong in a row"); wrongInRow = 0; }
      break;
    case EV_QUIZ_CORRECT: wrongInRow = 0; break;
    case EV_QUIZ_STREAK:  clue(UM_HAPPY, 1.0f, "3 quiz answers right in a row"); break;
    case EV_PET:          clue(UM_HAPPY, 0.5f, "you petted me"); break;
    case EV_SUDDEN_CLOSE: clue(UM_STRESSED, 0.5f, "you startled me"); break;
    default: break;
  }
}

void usermoodUpdate(unsigned long studiedMin) {
  if (studiedMin > 25 && !tiredNoted) {
    tiredNoted = true;
    char text[48]; snprintf(text, sizeof(text), "you studied %lu min today", studiedMin);
    clue(UM_TIRED, 1.0f, text);
  }
  if (lastClue != 0 && millis() - lastClue > FORGET_MS) {     // nothing new for 5 min: back to calm
    for (int m = 0; m < UM_COUNT; m++) score[m] = 0;
    evCount = 0;
    lastClue = 0;
    decide("no new clues");
  }
}

const char* usermood() { return UM_NAMES[current]; }

String usermoodJson() {
  String out = "{\"mood\":\"";
  out += UM_NAMES[current];
  out += "\",\"evidence\":[";
  for (int i = evCount - 1; i >= 0; i--) {                     // newest first
    if (i != evCount - 1) out += ",";
    out += "\""; out += evidence[i]; out += "\"";
  }
  return out + "]}";
}
