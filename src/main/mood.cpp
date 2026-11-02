// mood.cpp - the mood engine (see README: Mood engine).
//
// Four fast values (0-100): happy, fear, curious, energy.
// One slow value (0-100): bond, which grows with petting and is saved in NVS.
// Every second each value moves 4% of the way back to its baseline.
// Baseline happy = 30 + bond/5, so a well-treated robot is happier by default.
// No interaction: after IDLE_SAD the happy value sinks (sad); after IDLE_SLEEPY
// the energy sinks (sleepy). Any interaction (pet, approach ...) resets this.

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"
#include "mood.h"
#include "thoughts.h"
#include "learn.h"
#include "usermood.h"

// ---------- tunable numbers ----------
const float DECAY_PER_SECOND  = 0.04;   // move 4% of the gap to the baseline each second
const float FEAR_DECAY_PER_SECOND = 0.06;  // fear fades a bit faster (one scare ~3 s)
const float FEAR_BASE = 0, CURIOUS_BASE = 10, ENERGY_BASE = 80;

const float PET_HAPPY = 15, PET_FEAR = -10;          // spec
const float PET_MIN_HAPPY = 60;                       // a pet always makes it happy
const float CLOSE_FEAR = 30;                          // spec
const float CLOSE_HAPPY = -15;                        // a fright spoils the good mood
const float APPROACH_CURIOUS = 20;                    // spec
const float CHAT_HAPPY = 10, CHAT_CURIOUS = 10;       // spec (Phase 2)
const float QUIZ_RIGHT_HAPPY = 10;                    // F2: correct answer
const float QUIZ_WRONG_CURIOUS = 10;                  // F2: wrong answer (encouraging, not sad)
const float FOCUS_DONE_HAPPY = 15;                    // F2: a finished focus session
const unsigned long BOND_PET_INTERVAL_MS = 10000;     // bond +1 at most every 10 s

const unsigned long IDLE_SAD_MS    = DEMO_MODE ? 20000UL : 60000UL;
const unsigned long IDLE_SLEEPY_MS = DEMO_MODE ? 60000UL : 300000UL;
const float SAD_FALL_PER_SECOND    = 3;    // happy sinks this much per second when ignored
const float SLEEPY_FALL_PER_SECOND = 10;   // energy sinks this much per second when ignored

// Emotion thresholds (checked in priority order)
const float SCARED_FEAR    = 25;
const float HAPPY_HAPPY    = 45;
const float CURIOUS_CUR    = 25;
const float SAD_HAPPY      = 15;   // happy at or below this = sad
const float SLEEPY_ENERGY  = 20;   // energy at or below this = sleepy

const unsigned long SAVE_INTERVAL_MS = 300000UL;      // NVS: at most every 5 min

// ---------- state ----------
static MoodState s;
static unsigned long lastInteraction = 0, lastDecay = 0, lastSave = 0, lastBondPet = 0;
static bool bondPetDone = false;     // has bond been raised at least once since boot?
static bool dirty = false;           // something to save
static int idleStage = 0;            // 0 = awake, 1 = sad stage, 2 = sleepy stage
static bool calm = false;            // focus time: never gets bored (no idle stages)
static Preferences prefs;

// ---------- event memory: ring buffer of the last 20 events ----------
struct EventRecord { MoodEvent type; unsigned long ms; };
const int MAX_EVENTS = 20;
static EventRecord events[MAX_EVENTS];
static int eventHead = 0, eventCount = 0;

static const char* eventName(MoodEvent e) {
  switch (e) {
    case EV_BOOT: return "boot";             case EV_PET: return "pet";
    case EV_SUDDEN_CLOSE: return "sudden_close"; case EV_SLOW_APPROACH: return "slow_approach";
    case EV_IDLE_SAD: return "idle_sad";     case EV_IDLE_SLEEPY: return "idle_sleepy";
    case EV_PRAISE: return "praise";         case EV_CHAT_START: return "chat_start";
    case EV_QUIZ_CORRECT: return "quiz_correct"; case EV_QUIZ_WRONG: return "quiz_wrong";
    case EV_QUIZ_STREAK: return "quiz_streak";   case EV_FOCUS_START: return "focus_start";
    case EV_FOCUS_END: return "focus_end";
    case EV_LOUD_NOISE: return "loud_noise";     case EV_HEARD_VOICE: return "heard_voice";
  }
  return "?";
}

static void remember(MoodEvent e) {
  events[eventHead] = {e, millis()};
  eventHead = (eventHead + 1) % MAX_EVENTS;
  if (eventCount < MAX_EVENTS) eventCount++;
}

static float clamp100(float v) { return constrain(v, 0.0f, 100.0f); }
static float happyBase() { return 30 + s.bond / 5.0; }

// Move a value a little towards its baseline
static float decay(float value, float base, float rate = DECAY_PER_SECOND) {
  return value + (base - value) * rate;
}

static void interaction() {
  lastInteraction = millis();
  idleStage = 0;
}

// ===============================================================
void moodInit() {
  prefs.begin("buddy", false);
  s.bond       = prefs.getUChar("bond", 20);      // first boot: a little shy
  s.totalPets  = prefs.getULong("pets", 0);
  s.totalChats = prefs.getULong("chats", 0);
  s.happy   = happyBase();
  s.fear    = FEAR_BASE;
  s.curious = CURIOUS_BASE;
  s.energy  = ENERGY_BASE;
  lastInteraction = lastDecay = lastSave = millis();
  remember(EV_BOOT);
}

void moodEvent(MoodEvent ev) {
  unsigned long now = millis();
  switch (ev) {
    case EV_PET:
      // +15 (spec), but a pet always lifts happiness to at least PET_MIN_HAPPY
      // so even a sad or sleepy Buddy visibly cheers up
      s.happy = max(s.happy + PET_HAPPY, PET_MIN_HAPPY);
      s.fear  += PET_FEAR;
      s.energy = max(s.energy, ENERGY_BASE);      // a pet wakes it up
      s.totalPets++;
      if (!bondPetDone || now - lastBondPet >= BOND_PET_INTERVAL_MS) {
        s.bond = min(100, s.bond + 1);            // rate-limited bond growth
        lastBondPet = now;
        bondPetDone = true;
      }
      dirty = true;
      interaction();
      break;
    case EV_SUDDEN_CLOSE:
      s.fear  += CLOSE_FEAR;
      s.happy += CLOSE_HAPPY;
      s.energy = max(s.energy, 60.0f);            // startled awake
      interaction();
      break;
    case EV_SLOW_APPROACH:
      s.curious += APPROACH_CURIOUS;
      s.energy = max(s.energy, 60.0f);
      interaction();
      break;
    case EV_PRAISE:
    case EV_CHAT_START:
      s.happy   += CHAT_HAPPY;
      s.curious += CHAT_CURIOUS;
      if (ev == EV_CHAT_START) { s.totalChats++; dirty = true; }
      interaction();
      break;
    case EV_QUIZ_CORRECT:
      s.happy += QUIZ_RIGHT_HAPPY;
      interaction();
      break;
    case EV_QUIZ_WRONG:
      s.curious += QUIZ_WRONG_CURIOUS;
      interaction();
      break;
    case EV_QUIZ_STREAK:                          // 3 right in a row: Buddy likes you more
      s.bond = min(100, s.bond + 1);
      dirty = true;
      interaction();
      break;
    case EV_FOCUS_START:
      interaction();
      break;
    case EV_FOCUS_END:
      s.happy += FOCUS_DONE_HAPPY;
      s.energy = max(s.energy, ENERGY_BASE);
      interaction();
      break;
    case EV_LOUD_NOISE:                           // a clap or bang startles Buddy
      s.fear  += CLOSE_FEAR;
      s.happy += CLOSE_HAPPY;
      s.energy = max(s.energy, 60.0f);
      interaction();
      break;
    case EV_HEARD_VOICE:                          // someone is talking to Buddy
      s.curious += 15;
      s.energy = max(s.energy, 60.0f);
      interaction();
      break;
    default:
      break;
  }
  s.happy = clamp100(s.happy);  s.fear = clamp100(s.fear);
  s.curious = clamp100(s.curious);  s.energy = clamp100(s.energy);
  remember(ev);
  learnOnEvent(ev);                  // G1a: is this feedback for a move Buddy just tried?
  usermoodOnEvent(ev);               // G1c: also a clue about how the user feels
}

void moodUpdate() {
  unsigned long now = millis();
  if (now - lastDecay < 1000) return;       // once per second
  lastDecay = now;

  // ---- idle stages (not during focus time: a studying student is not "ignoring" Buddy) ----
  if (calm) interaction();
  // (an interaction can be newer than "now" - e.g. set just above - so never let this go negative)
  unsigned long idle = (long)(now - lastInteraction) > 0 ? now - lastInteraction : 0;
  if (idle >= IDLE_SLEEPY_MS && idleStage < 2) {
    idleStage = 2; remember(EV_IDLE_SLEEPY);
    think("Alone for %lu s - getting sleepy", idle / 1000);
  } else if (idle >= IDLE_SAD_MS && idleStage < 1) {
    idleStage = 1; remember(EV_IDLE_SAD);
    think("Nobody played with me for %lu s - feeling lonely", idle / 1000);
  }

  // ---- happy: sinks while ignored (sad stage), otherwise back to baseline ----
  if (idleStage == 1) s.happy = max(5.0f, s.happy - SAD_FALL_PER_SECOND);
  else                s.happy = decay(s.happy, happyBase());

  // ---- energy: sinks in the sleepy stage, otherwise back to baseline ----
  if (idleStage == 2) s.energy = max(5.0f, s.energy - SLEEPY_FALL_PER_SECOND);
  else                s.energy = decay(s.energy, ENERGY_BASE);

  s.fear    = decay(s.fear, FEAR_BASE, FEAR_DECAY_PER_SECOND);
  s.curious = decay(s.curious, CURIOUS_BASE);

  // ---- save bond + counters, at most every 5 minutes ----
  if (dirty && now - lastSave >= SAVE_INTERVAL_MS) moodSaveNow();
}

// Priority: scared > happy > curious > sad > sleepy > neutral
Emotion moodEmotion() {
  if (s.fear    >= SCARED_FEAR)   return EMO_SCARED;
  if (s.happy   >= HAPPY_HAPPY)   return EMO_HAPPY;
  if (s.curious >= CURIOUS_CUR)   return EMO_CURIOUS;
  if (s.happy   <= SAD_HAPPY)     return EMO_SAD;
  if (s.energy  <= SLEEPY_ENERGY) return EMO_SLEEPY;
  return EMO_NEUTRAL;
}

const MoodState& moodState() { return s; }

unsigned long moodIdleSeconds() {
  long ms = (long)(millis() - lastInteraction);
  return ms > 0 ? ms / 1000 : 0;
}

void moodSkipIdle() {
  // Pretend nobody has interacted for a long time (jump to the next stage)
  unsigned long jump = (idleStage == 0) ? IDLE_SAD_MS : IDLE_SLEEPY_MS;
  lastInteraction = millis() - jump;
}

void moodSaveNow() {
  prefs.putUChar("bond", (uint8_t)s.bond);
  prefs.putULong("pets", s.totalPets);
  prefs.putULong("chats", s.totalChats);
  dirty = false;
  lastSave = millis();
  Serial.printf("[mood] saved to NVS: bond=%d pets=%lu chats=%lu\n",
                s.bond, s.totalPets, s.totalChats);
}

// Short sentence for an event (for the Mind tab "reason")
const char* moodEventText(MoodEvent e) {
  switch (e) {
    case EV_BOOT: return "I just woke up";
    case EV_PET: return "someone petted me";
    case EV_SUDDEN_CLOSE: return "something rushed at me";
    case EV_SLOW_APPROACH: return "someone came closer";
    case EV_IDLE_SAD: return "I was left alone";
    case EV_IDLE_SLEEPY: return "alone for a long time";
    case EV_PRAISE: return "I was praised";
    case EV_CHAT_START: return "we started talking";
    case EV_QUIZ_CORRECT: return "a quiz answer was right";
    case EV_QUIZ_WRONG: return "a quiz answer was wrong";
    case EV_QUIZ_STREAK: return "3 quiz answers in a row";
    case EV_FOCUS_START: return "a study session started";
    case EV_FOCUS_END: return "a study session finished";
    case EV_LOUD_NOISE: return "a loud noise startled me";
    case EV_HEARD_VOICE: return "I heard someone talking";
  }
  return "something happened";
}

bool moodLastEvent(MoodEvent* ev, unsigned long* agoS) {
  if (eventCount == 0) return false;
  const EventRecord& r = events[(eventHead - 1 + MAX_EVENTS) % MAX_EVENTS];
  *ev = r.type;
  *agoS = (millis() - r.ms) / 1000;
  return true;
}

void moodNudge(float dHappy, float dCurious) {
  s.happy = clamp100(s.happy + dHappy);
  s.curious = clamp100(s.curious + dCurious);
}

void moodPrintEvents() {
  Serial.printf("[mood] last %d events (oldest first):\n", eventCount);
  int start = (eventHead - eventCount + MAX_EVENTS) % MAX_EVENTS;
  for (int i = 0; i < eventCount; i++) {
    const EventRecord& r = events[(start + i) % MAX_EVENTS];
    Serial.printf("   %6lu s  %s\n", r.ms / 1000, eventName(r.type));
  }
}

void moodSetCalm(bool c) {
  calm = c;
  interaction();
}

void moodRecentEventsJson(char* buf, size_t len, int n) {
  n = min(n, eventCount);
  size_t used = snprintf(buf, len, "[");
  unsigned long now = millis();
  for (int i = 0; i < n && used < len; i++) {
    const EventRecord& r = events[(eventHead - n + i + MAX_EVENTS) % MAX_EVENTS];   // oldest of the n first
    used += snprintf(buf + used, len - used, "%s{\"e\":\"%s\",\"ago_s\":%lu}",
                     i ? "," : "", eventName(r.type), (now - r.ms) / 1000);
  }
  if (used < len) snprintf(buf + used, len - used, "]");
}
