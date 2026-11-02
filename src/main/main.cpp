// main.cpp - Buddy, the emotional robot car (Phase 1 + F1 phone/AI firmware).
//
// The cognitive loop, every pass of loop():
//   Perceive : sensorsUpdate()  -> events (sudden close, slow approach, pet)
//   Feel     : moodEvent() / moodUpdate() -> happy, fear, curious, energy, bond
//   Decide   : moodEmotion() picks ONE emotion by priority rules
//   Act      : eyes + RGB colour follow the emotion; a buzzer rhythm and a short
//              move play as a "reaction" (each emotion at most once per 3 s)
//   Adapt    : bond (saved in NVS) raises the baseline happiness and makes moves bigger
//
// F1: the phone (web page served by this robot) talks to the Gemini AI and
// tells the robot which face to show (/emotion) and where to drive (/drive).
// A face asked for by the phone is an "override" on top of the mood for a few
// seconds; afterwards the face goes back to the mood emotion (quietly).
//
// F2: study mode on the phone (Explain, Quiz, Pomodoro). During Pomodoro work
// time the OLED shows a countdown and Buddy stays calm (no reactions at all).
//
// Serial test commands: t = pet, n = sudden close, a = slow approach,
// i = skip idle time, e = event memory, w = save NVS now, ? = menu.
// Floor calibration: f = forward ramp, r = spin ramp, x = stop motors.
// No delay() in loop(): every module uses millis() timers.

#include <Arduino.h>
#include "config.h"
#include "emotion.h"
#include "eyes.h"
#include "sensors.h"
#include "mood.h"
#include "sound.h"
#include "drive.h"
#include "net.h"
#include "web.h"
#include "robot.h"
#include "focus.h"
#include "speaker.h"
#include "hears.h"
#include "thoughts.h"
#include "learn.h"
#include "usermood.h"

const unsigned long REACTION_COOLDOWN_MS = 3000;
const unsigned long STATUS_INTERVAL_MS   = 1000;
const unsigned long GREETING_MS          = 3000;
const int HIGH_BOND = 60;                  // at or above: excited greeting at boot

static Emotion shown = EMO_NEUTRAL;        // emotion currently on the face
static unsigned long lastReaction[EMO_COUNT];
static unsigned long lastStatus = 0;
static unsigned long greetingUntil = 0;    // boot greeting overrides the mood until then

// Face asked for by the phone (thinking / talking / reply emotion)
static Emotion overrideEmo = EMO_NEUTRAL;
static unsigned long overrideUntil = 0;    // 0 = no override
static bool overrideBeep = false;          // play the rhythm once when it starts

// Emotion sound: a chirp on the speaker (SOUND_OUTPUT), or the buzzer rhythm as fallback
// Mood sounds are rate-limited so the robot is not noisy: at most one every 15 s,
// and the sad sound at most once every 2 minutes. Switched off in Settings = silent.
const unsigned long MOOD_SOUND_GAP_MS = 15000UL;
const unsigned long SAD_SOUND_GAP_MS  = 120000UL;
static void playRhythmFor(Emotion e) {
  static unsigned long lastSound = 0, lastSad = 0;
  if (!speakerEffectsOn()) return;
  if (e != EMO_HAPPY && e != EMO_SCARED && e != EMO_SAD && e != EMO_CURIOUS && e != EMO_EXCITED) return;
  unsigned long now = millis();
  if (lastSound != 0 && now - lastSound < MOOD_SOUND_GAP_MS) return;
  if (e == EMO_SAD) {
    if (lastSad != 0 && now - lastSad < SAD_SOUND_GAP_MS) return;
    lastSad = now;
  }
  lastSound = now;
  if (SOUND_OUTPUT == SOUND_SPEAKER) {
    switch (e) {
      case EMO_HAPPY:   speakerEffect(FX_HAPPY);    break;
      case EMO_SCARED:  speakerEffect(FX_SCARED);   break;
      case EMO_SAD:     speakerEffect(FX_SAD);      break;
      case EMO_CURIOUS: speakerEffect(FX_CURIOUS);  break;
      case EMO_EXCITED: speakerEffect(FX_GREETING); break;
      default: break;                          // sleepy / neutral / thinking / talking: silent
    }
    return;
  }
  switch (e) {
    case EMO_HAPPY:   soundPlay(RH_HAPPY);    break;
    case EMO_SCARED:  soundPlay(RH_SCARED);   break;
    case EMO_SAD:     soundPlay(RH_SAD);      break;
    case EMO_EXCITED: soundPlay(RH_GREETING); break;
    default: break;                            // curious / sleepy / neutral / thinking / talking: silent
  }
}

// ---------- WHY Buddy feels what it feels (Mind tab) ----------
static char reason[80] = "I just woke up";
static bool faceFromPhone = false;         // true while the phone/AI (or a trick) chose the face
static const char* overrideWhy = nullptr;  // reason given with that face (nullptr = AI answer)

static void updateReason(Emotion e) {
  MoodEvent ev; unsigned long ago = 0;
  bool have = moodLastEvent(&ev, &ago);
  if (e == EMO_THINKING)      snprintf(reason, sizeof(reason), "waiting for the AI to answer");
  else if (e == EMO_TALKING)  snprintf(reason, sizeof(reason), "I am talking");
  else if (faceFromPhone)     snprintf(reason, sizeof(reason), "%s", overrideWhy ? overrideWhy : "that is how my answer felt");
  else if (e == EMO_SAD)      snprintf(reason, sizeof(reason), "left alone for %lu s", moodIdleSeconds());
  else if (e == EMO_SLEEPY)   snprintf(reason, sizeof(reason), "nobody around for %lu s", moodIdleSeconds());
  else if (e == EMO_NEUTRAL)  snprintf(reason, sizeof(reason), "calm, nothing special happening");
  else if (have && ago < 30)  snprintf(reason, sizeof(reason), "%s (%lu s ago)", moodEventText(ev), ago);
  else                        snprintf(reason, sizeof(reason), "my mood settled here");
}

// Change eyes + colour without any reaction (no beep, no move)
static void setFace(Emotion e) {
  if (e == shown) return;
  shown = e;
  eyesSetEmotion(e);
  rgbSetEmotion(e);
  updateReason(e);
  Serial.printf("[%6lu ms] face -> %s\n", millis(), emotionName(e));
  if (e != EMO_THINKING && e != EMO_TALKING) think("Showing %s: %s", emotionName(e), reason);
}

// Eyes + colour follow the emotion; the reaction (beeps + move) has a cooldown
static void react(Emotion e, bool force) {
  unsigned long now = millis();
  if (e != shown) {
    shown = e;
    eyesSetEmotion(e);
    rgbSetEmotion(e);
    updateReason(e);
    Serial.printf("[%6lu ms] emotion -> %s\n", now, emotionName(e));
    think("Feeling %s because %s", emotionName(e), reason);
    force = true;
  }
  if (!force) return;
  if (lastReaction[e] != 0 && now - lastReaction[e] < REACTION_COOLDOWN_MS) return;
  lastReaction[e] = now;
  playRhythmFor(e);
  driveBehaviour(e, sensorsDistance(), sensorsRearObstacle(), moodState().bond);
}

// ---------- offered to the web server (robot.h) ----------
void robotShowEmotion(Emotion e, unsigned long ms, bool beep, const char* why) {
  overrideWhy = why;
  if (overrideEmo != e) shown = EMO_COUNT;   // redraw, so the new reason is used
  overrideEmo = e;
  overrideUntil = millis() + ms;
  if (overrideUntil == 0) overrideUntil = 1;
  overrideBeep = beep;
  greetingUntil = 0;
}

Emotion robotFaceEmotion() { return shown; }
const char* robotEmotionReason() { return reason; }

static char sayText[96] = "";
static unsigned long sayId = 0;
void robotSay(const char* text) {
  snprintf(sayText, sizeof(sayText), "%s", text);
  for (char* p = sayText; *p; p++) if (*p == '"' || *p == '\\') *p = '\'';   // keep JSON valid
  sayId++;
  think("Saying: %s", sayText);
}
unsigned long robotSayId() { return sayId; }
const char* robotSayText() { return sayText; }

// ---------- Serial ----------
static void printMenu() {
  Serial.println();
  Serial.println("=== Buddy main firmware (Phase 1 + F1) ===");
  Serial.printf (" MOTORS_ENABLED=%d  TOUCH_ENABLED=%d  DEMO_MODE=%d\n",
                 MOTORS_ENABLED, TOUCH_ENABLED, DEMO_MODE);
  Serial.println(" t = pet   n = sudden close   a = slow approach   i = skip idle time");
  Serial.println(" e = event memory   w = save NVS now   ? = menu");
  Serial.println(" f = calibrate forward ramp   r = calibrate spin ramp   x = STOP motors");
}

static void printStatus() {
  const MoodState& s = moodState();
  float d = sensorsDistance();
  char dist[12];
  if (d < 0) snprintf(dist, sizeof(dist), "  ---");
  else       snprintf(dist, sizeof(dist), "%5.1f", d);
  Serial.printf("[%5lu s] %-8s H=%3.0f F=%3.0f C=%3.0f E=%3.0f B=%3d | dist=%s ir=%s snd=%d/%d idle=%lus | %s\n",
                millis() / 1000, emotionName(shown), s.happy, s.fear, s.curious, s.energy, s.bond,
                dist, sensorsRearObstacle() ? "OBST " : "clear", hearsSharpLevel(), hearsRawLevel(),
                moodIdleSeconds(), netConnected() ? netIp().c_str() : "no wifi");
}

// Returns true if the command was a simulated event
static bool handleSerial() {
  if (!Serial.available()) return false;
  char c = Serial.read();
  switch (c) {
    case 't': Serial.println("[test] pet");            moodEvent(EV_PET);           return true;
    case 'n': Serial.println("[test] sudden close");   moodEvent(EV_SUDDEN_CLOSE);  return true;
    case 'a': Serial.println("[test] slow approach");  moodEvent(EV_SLOW_APPROACH); return true;
    case 'i': Serial.println("[test] skip idle time"); moodSkipIdle();              return false;
    case 'e': moodPrintEvents(); return false;
    case 'w': moodSaveNow();     return false;
    case '?': printMenu();       return false;
    case 'f': driveCalibrateForward(); return false;
    case 'r': driveCalibrateSpin();    return false;
    case 'x': driveStop(); Serial.println("[test] motors stopped"); return false;
    case 'o': focusStartWork(1, 1); return false;                   // 1-minute focus demo
    default:  return false;
  }
}

void setup() {
  driveInit();                    // FIRST: motors off
  Serial.begin(115200);
  soundInit();
  eyesInit();
  sensorsInit();
  moodInit();
  netInit();                      // WiFi starts connecting in the background
  webInit();
  speakerInit();                  // after WiFi, so WiFi gets its memory first
  if (HEAR_ENABLED) hearsInit();
  learnInit();                    // G1a: load what Buddy has learnt (NVS)

  printMenu();
  const MoodState& s = moodState();
  Serial.printf("[boot] bond=%d pets=%lu chats=%lu (loaded from NVS)\n",
                s.bond, s.totalPets, s.totalChats);

  // Boot greeting: excited if Buddy likes you, otherwise a shy neutral face
  if (SOUND_OUTPUT == SOUND_SPEAKER) speakerEffect(FX_GREETING); else soundPlay(RH_GREETING);
  if (s.bond >= HIGH_BOND) {
    Serial.println("[boot] high bond -> excited greeting");
    shown = EMO_EXCITED;
    eyesSetEmotion(EMO_EXCITED);
    rgbSetEmotion(EMO_EXCITED);
  } else {
    Serial.println("[boot] low/medium bond -> shy greeting");
    eyesSetEmotion(EMO_NEUTRAL);
    rgbSetEmotion(EMO_NEUTRAL);
  }
  greetingUntil = millis() + GREETING_MS;
}

void loop() {
  // ---- Perceive ----
  bool eventHappened = handleSerial();
  int ev = sensorsUpdate();
  if (ev & SE_SUDDEN_CLOSE)  { Serial.println("[sensor] sudden close");  moodEvent(EV_SUDDEN_CLOSE); }
  if (ev & SE_SLOW_APPROACH) { Serial.println("[sensor] slow approach"); moodEvent(EV_SLOW_APPROACH); }
  if (ev & SE_PET)           { Serial.println("[sensor] pet");           moodEvent(EV_PET); }
  if (ev != SE_NONE) eventHappened = true;

  // "Buddy hears": never while it speaks or moves (it must not hear itself)
  if (HEAR_ENABLED) {
    float d = sensorsDistance();
    int he = hearsUpdate(speakerPlaying() || driveBusy(), d > 0 && d < 60);
    if (he & HE_LOUD)  { moodEvent(EV_LOUD_NOISE);  eventHappened = true; }
    if (he & HE_VOICE) { moodEvent(EV_HEARD_VOICE); eventHappened = true; }
  }

  // ---- Feel ----
  moodUpdate();

  // ---- Phone / WiFi ----
  netUpdate();
  webUpdate();

  // ---- Learning (G1a): when lonely, try a social move and learn from the reaction ----
  {
    float d = sensorsDistance();
    bool canAct = !focusWorking() && !speakerSpeaking() && !driveBusy() &&
                  overrideUntil == 0 && moodEmotion() != EMO_SCARED;
    learnUpdate(d, sensorsRearObstacle(), canAct);
  }

  usermoodUpdate(focusStudiedMinutes());          // G1c: user mood decays / study time clue

  // ---- Study timer ----
  static bool wasWorking = false;
  focusUpdate(sensorsDistance(), sensorsRearObstacle());
  if (wasWorking && !focusWorking()) shown = EMO_COUNT;   // force eyes + colour to redraw
  wasWorking = focusWorking();

  // ---- Own voice (F4a): motors stay still while Buddy speaks ----
  static bool wasSpeaking = false;
  bool speaking = speakerSpeaking();             // speech only (sound effects do not count)
  if (speaking && !wasSpeaking) driveStop();
  wasSpeaking = speaking;

  // ---- Decide + Act ----
  if (eventHappened) greetingUntil = 0;           // a real event ends the greeting
  if (focusWorking()) {
    // focus time: countdown on the OLED, calm colour, no reactions / beeps / moves
  } else if (speaking) {
    setFace(EMO_TALKING);                         // talking eyes, no reactions while speaking
  } else if (overrideUntil != 0) {
    // the phone asked for a face (AI thinking / talking / reply emotion)
    if ((long)(millis() - overrideUntil) < 0) {
      faceFromPhone = true;
      setFace(overrideEmo);
      if (overrideBeep) { playRhythmFor(overrideEmo); overrideBeep = false; }
    } else {
      overrideUntil = 0;
      faceFromPhone = false;
      setFace(moodEmotion());                     // back to the mood, quietly
    }
  } else if (greetingUntil == 0 || (long)(millis() - greetingUntil) >= 0) {
    greetingUntil = 0;
    react(moodEmotion(), eventHappened);
  }

  eyesUpdate();
  soundUpdate();
  driveUpdate(sensorsDistance(), sensorsRearObstacle());

  // ---- Status once per second ----
  if (millis() - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = millis();
    printStatus();
  }
}
