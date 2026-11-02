// focus.cpp - Pomodoro study timer + presence-aware focus buddy (F2 + G1b).
//
// Work time: the OLED shows a big MM:SS countdown instead of the eyes, the LED
// glows a calm purple, Buddy never gets bored and does no reactions or moves.
// G1b - Buddy watches whether you are at your desk:
//   present = something in front closer than 80 cm (median distance), and the
//             state only changes after it has been stable for 2 s (debounce)
//   away longer than the threshold (10 s DEMO / 20 s normal) -> sad eyes, blue
//             LED, sad chirp, "Hey, come back!" (max once per 60 s)
//   back again -> happy chirp + "Welcome back!"
//   end of work -> focus score = % of the time you were present: OLED, Mind tab,
//             spoken, saved in NVS. If the last score was below 70 %, the next
//             session nudges after HALF the time (adaptation).

#include <Arduino.h>
#include <Preferences.h>
#include "focus.h"
#include "eyes.h"
#include "mood.h"
#include "sound.h"
#include "speaker.h"
#include "config.h"
#include "drive.h"
#include "robot.h"
#include "thoughts.h"

const float PRESENT_CM = 80;
const unsigned long DEBOUNCE_MS = 2000;
const unsigned long AWAY_MS_NORMAL = DEMO_MODE ? 10000UL : 20000UL;
const unsigned long NUDGE_GAP_MS = 60000UL;
const unsigned long NUDGE_SHOW_MS = 4000UL;      // sad face shown this long, then countdown again
const int LOW_SCORE = 70;

static FocusMode mode = FOCUS_OFF;
static unsigned long endMs = 0, workStart = 0;   // current phase end, work start
static int nextBreakMin = 0;
static unsigned long lastDraw = 0;
static unsigned long studiedMs = 0;              // finished work time since boot (G1c: "studied today")

// presence
static bool present = true;                      // debounced state
static unsigned long rawSince = 0;               // when the raw reading last changed
static bool rawLast = true;
static unsigned long awaySince = 0;              // when the debounced state became "away"
static unsigned long presentMs = 0, lastTick = 0;
static unsigned long lastNudge = 0, faceUntil = 0;
static bool nudged = false;                      // nudged during this absence?
static unsigned long awayThreshold = AWAY_MS_NORMAL;
static int lastScore = -1;                       // from NVS (-1 = never)
static Preferences prefs;

static unsigned long minutesToMs(int m) { return (unsigned long)constrain(m, 1, 120) * 60000UL; }

static void loadScore() {
  prefs.begin("focus", false);
  lastScore = prefs.getInt("score", -1);
}

void focusStartWork(int workMin, int breakMin) {
  if (lastScore == -1) loadScore();
  mode = FOCUS_WORK;
  workStart = millis();
  endMs = workStart + minutesToMs(workMin);
  nextBreakMin = constrain(breakMin, 0, 60);
  lastDraw = 0;
  present = true; rawLast = true; rawSince = workStart;
  awaySince = 0; presentMs = 0; lastTick = workStart;
  lastNudge = 0; faceUntil = 0; nudged = false;
  // adaptation: a low score last time -> nudge after half the time
  awayThreshold = (lastScore >= 0 && lastScore < LOW_SCORE) ? AWAY_MS_NORMAL / 2 : AWAY_MS_NORMAL;
  driveStop();
  moodSetCalm(true);
  moodEvent(EV_FOCUS_START);
  rgbSetColour(110, 0, 140);                     // calm purple (focus time; not used by any emotion)
  think("Study session: %d min focus, I will stay quiet", workMin);
  if (awayThreshold < AWAY_MS_NORMAL)
    think("Last time you drifted away (%d%%), so I'll nudge sooner (%lu s)", lastScore, awayThreshold / 1000);
}

void focusStartBreak(int breakMin) {
  mode = FOCUS_BREAK;
  endMs = millis() + minutesToMs(breakMin);
  moodSetCalm(false);
  Serial.printf("[focus] break %d min\n", breakMin);
}

void focusStop() {
  if (mode == FOCUS_WORK) studiedMs += millis() - workStart;
  if (mode != FOCUS_OFF) Serial.println("[focus] stopped");
  mode = FOCUS_OFF;
  moodSetCalm(false);
  robotShowEmotion(EMO_NEUTRAL, 500, false);     // eyes + colour come back
}

// Presence while working: debounce, count time, nudge when away too long
static void trackPresence(float distCm, unsigned long now) {
  bool raw = distCm > 0 && distCm < PRESENT_CM;
  if (raw != rawLast) { rawLast = raw; rawSince = now; }
  if (present) presentMs += now - lastTick;      // count time at the desk
  lastTick = now;

  if (raw != present && now - rawSince >= DEBOUNCE_MS) {   // stable for 2 s: accept the change
    present = raw;
    if (!present) {
      awaySince = now;
      nudged = false;
    } else if (nudged) {                         // came back after a nudge
      if (SOUND_OUTPUT == SOUND_SPEAKER) speakerEffect(FX_HAPPY);
      robotSay("Welcome back!");
      think("You came back after %lu s - welcome back!", (now - awaySince) / 1000);
      eyesSetEmotion(EMO_HAPPY);
      rgbSetColour(0, 255, 0);
      faceUntil = now + 2500;
    }
  }

  if (!present && !nudged && now - awaySince >= awayThreshold && (lastNudge == 0 || now - lastNudge >= NUDGE_GAP_MS)) {
    nudged = true;
    lastNudge = now;
    if (SOUND_OUTPUT == SOUND_SPEAKER) speakerEffect(FX_SAD);
    robotSay("Hey, come back!");
    think("You left your desk %lu s ago - I miss you", (now - awaySince) / 1000);
    eyesSetEmotion(EMO_SAD);                     // sad eyes instead of the countdown for a moment
    rgbSetColour(0, 0, 255);                     // blue
    faceUntil = now + NUDGE_SHOW_MS;
  }
}

static void finishWork(float distCm, bool rearBlocked) {
  unsigned long total = max(1UL, millis() - workStart);
  studiedMs += total;
  int score = (int)min(100UL, presentMs * 100UL / total);
  lastScore = score;
  prefs.putInt("score", score);
  think("Study session finished - you were here %d%% of the time", score);
  moodSetCalm(false);
  moodEvent(EV_FOCUS_END);
  if (SOUND_OUTPUT == SOUND_SPEAKER) speakerEffect(FX_TUNE); else soundPlay(RH_ALARM);
  char big[8]; snprintf(big, sizeof(big), "%d%%", score);
  eyesShowBig("FOCUS SCORE", big, 6000);
  char line[96];
  if (score >= LOW_SCORE) snprintf(line, sizeof(line), "You were here %d percent of the time. Great job!", score);
  else snprintf(line, sizeof(line), "You were here %d percent of the time. Let's try to stay a bit longer next time!", score);
  robotSay(line);
  robotShowEmotion(score >= LOW_SCORE ? EMO_HAPPY : EMO_NEUTRAL, 7000, false, "the study session just finished");
  if (score >= LOW_SCORE) driveBehaviour(EMO_HAPPY, distCm, rearBlocked, moodState().bond);   // happy wiggle
  if (nextBreakMin > 0) focusStartBreak(nextBreakMin);
  else mode = FOCUS_OFF;
}

void focusUpdate(float distCm, bool rearBlocked) {
  if (mode == FOCUS_OFF) return;
  unsigned long now = millis();
  long left = (long)(endMs - now);

  if (mode == FOCUS_WORK) {
    if (left <= 0) { finishWork(distCm, rearBlocked); return; }
    trackPresence(distCm, now);
    if (faceUntil != 0) {                         // showing the sad / happy face for a moment
      if ((long)(now - faceUntil) < 0) return;
      faceUntil = 0;
      rgbSetColour(110, 0, 140);                  // back to calm purple
    }
    if (now - lastDraw >= 1000) {                 // redraw the countdown once per second
      lastDraw = now;
      eyesShowCountdown(present ? "FOCUS - study time" : "FOCUS - where are you?", (left + 999) / 1000);
    }
    return;
  }

  // ---- break ----
  if (left <= 0) {
    Serial.println("[focus] break finished");
    if (SOUND_OUTPUT == SOUND_SPEAKER) speakerEffect(FX_GREETING); else soundPlay(RH_GREETING);
    robotShowEmotion(EMO_CURIOUS, 3000, false, "the break is over");
    mode = FOCUS_OFF;
  }
}

FocusMode focusMode() { return mode; }

const char* focusModeName() {
  return mode == FOCUS_WORK ? "work" : mode == FOCUS_BREAK ? "break" : "off";
}

unsigned long focusSecondsLeft() {
  if (mode == FOCUS_OFF) return 0;
  long left = (long)(endMs - millis());
  return left > 0 ? (left + 999) / 1000 : 0;
}

bool focusWorking() { return mode == FOCUS_WORK; }

unsigned long focusStudiedMinutes() {
  unsigned long ms = studiedMs + (mode == FOCUS_WORK ? millis() - workStart : 0);
  return ms / 60000UL;
}

String focusJson() {
  if (lastScore == -1) loadScore();
  unsigned long now = millis();
  unsigned long total = mode == FOCUS_WORK ? max(1UL, now - workStart) : 1;
  String out = "{\"mode\":\"";
  out += focusModeName();
  out += "\",\"present\":"; out += (mode == FOCUS_WORK && !present) ? 0 : 1;
  out += ",\"away_s\":"; out += (mode == FOCUS_WORK && !present) ? (now - awaySince) / 1000 : 0;
  out += ",\"score_now\":"; out += mode == FOCUS_WORK ? (int)min(100UL, presentMs * 100UL / total) : -1;
  out += ",\"threshold_s\":";
  unsigned long thr = (mode == FOCUS_WORK) ? awayThreshold
                    : ((lastScore >= 0 && lastScore < LOW_SCORE) ? AWAY_MS_NORMAL / 2 : AWAY_MS_NORMAL);
  out += thr / 1000;
  out += ",\"adapted\":"; out += thr < AWAY_MS_NORMAL ? 1 : 0;
  out += ",\"last_score\":"; out += lastScore;
  out += "}";
  return out;
}
