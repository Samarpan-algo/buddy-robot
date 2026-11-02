// learn.cpp - G1a: learning from feedback (one-step Q-learning / contextual bandit).
//
// STATE (4): is a person near (< 60 cm)?  x  is Buddy content or lonely (happy < 30)?
// ACTIONS (5): wiggle, spin dance, slow approach, speaker tune, look around (eyes only)
// WHEN: Buddy has been ignored for 20 s (DEMO_MODE; 60 s normally), it is not
//       studying / speaking / moving / scared, and the last try was >= 10 s ago.
// CHOICE: epsilon-greedy - with probability epsilon try a random move (explore),
//         otherwise the move with the best score for this state (exploit).
//         epsilon starts at 0.3 and slowly drops to 0.1.
// FEEDBACK (5 s window after the move): pet +1, praise +1, thumbs +1/-1,
//         chat started +0.5, quiz right +0.5, scared -1, ignored while near -0.2
// UPDATE: Q[state][move] += 0.3 * (reward - Q[state][move])
// The table is saved in NVS (flash), so Buddy remembers after a reboot.

#include <Preferences.h>
#include "learn.h"
#include "config.h"
#include "drive.h"
#include "speaker.h"
#include "robot.h"
#include "thoughts.h"

const int NUM_STATES = 4, NUM_MOVES = 5;
enum Move { L_WIGGLE, L_SPIN, L_APPROACH, L_TUNE, L_LOOK };
static const char* MOVE_NAMES[NUM_MOVES] = {"wiggle", "spin dance", "slow approach", "speaker tune", "look around"};
static const char* STATE_NAMES[NUM_STATES] = {"far, content", "far, lonely", "near, content", "near, lonely"};

const float ALPHA = 0.3;                                  // learning rate
const float EPS_START = 0.3, EPS_MIN = 0.1, EPS_DECAY = 0.95;
const unsigned long LONELY_MS   = DEMO_MODE ? 20000UL : 60000UL;
const unsigned long COOLDOWN_MS = 10000UL;                  // after a reaction
const unsigned long MAX_COOLDOWN_MS = 60000UL;              // after being ignored several times
const unsigned long SAY_GAP_MS = DEMO_MODE ? 120000UL : 300000UL;   // spoken line at most every 2 / 5 min
const unsigned long WINDOW_MS   = 5000UL;
const unsigned long SAVE_GAP_MS = 10000UL;                // NVS: at most one save per 10 s
const float NEAR_CM = 60;

static float Q[NUM_STATES][NUM_MOVES];
static float epsilon = EPS_START;
static Preferences prefs;

static bool windowOpen = false;
static unsigned long windowStart = 0, lastTryEnd = 0, lastSave = 0;
static int curState = 0, curMove = 0;
static float reward = 0;
static bool gotFeedback = false, personSeenNear = false, saveDue = false;
static bool chosenToExplore = false;
static bool triedOnce = false;                   // has Buddy tried any move since boot?
static unsigned long cooldown = COOLDOWN_MS;     // grows when Buddy is ignored (no nagging)

static int stateNow(float distCm) {
  bool nearby = distCm > 0 && distCm < NEAR_CM;
  bool lonely = moodState().happy < 30;
  return (nearby ? 2 : 0) + (lonely ? 1 : 0);
}

static void save() {
  prefs.putBytes("q", Q, sizeof(Q));
  prefs.putFloat("eps", epsilon);
  lastSave = millis();
  saveDue = false;
}

void learnInit() {
  prefs.begin("learn", false);
  if (prefs.getBytes("q", Q, sizeof(Q)) != sizeof(Q)) memset(Q, 0, sizeof(Q));   // first boot: all 0
  epsilon = prefs.getFloat("eps", EPS_START);
  lastTryEnd = millis();
}

// Learning MODE: off at every boot. While off, Buddy never tries tricks or
// proactive lines (the learnt scores are kept for next time).
static bool active = false;

void learnSetActive(bool on) {
  if (on == active) return;
  active = on;
  if (!on) windowOpen = false;                       // a move being rated is simply dropped
  lastTryEnd = millis();
  cooldown = COOLDOWN_MS;
  think(on ? "Learning mode ON - I will try tricks when I feel lonely" : "Learning mode OFF - no more tricks for now");
}
bool learnActive() { return active; }

void learnReset() {
  memset(Q, 0, sizeof(Q));
  epsilon = EPS_START;
  windowOpen = false;
  save();
  think("Forgot everything I learnt - starting fresh");
}

// Pick a move: explore (random) with probability epsilon, otherwise the best one
static int chooseMove(int s, bool approachOk) {
  chosenToExplore = random(1000) < epsilon * 1000;
  if (chosenToExplore) {
    int m;
    do { m = random(NUM_MOVES); } while ((m == L_APPROACH && !approachOk) || (m == L_TUNE && !speakerEffectsOn()));
    return m;
  }
  int best = -1;
  for (int m = 0; m < NUM_MOVES; m++) {
    if (m == L_APPROACH && !approachOk) continue;
    if (m == L_TUNE && !speakerEffectsOn()) continue;         // sounds switched off
    if (best < 0 || Q[s][m] > Q[s][best] || (Q[s][m] == Q[s][best] && random(2))) best = m;
  }
  return best;
}

// Do the move. Returns false if it could not be done right now.
static bool doMove(int m, float distCm, bool rearBlocked) {
  switch (m) {
    case L_WIGGLE:   return driveSocial(MOVE_WIGGLE, distCm, rearBlocked);
    case L_SPIN:     return driveSocial(MOVE_SPIN_DANCE, distCm, rearBlocked);
    case L_APPROACH: return driveSocial(MOVE_SLOW_APPROACH, distCm, rearBlocked);
    case L_TUNE:     return speakerEffect(FX_TUNE);
    case L_LOOK:     robotShowEmotion(EMO_CURIOUS, 3000, false, "trying the look-around trick"); return true;   // eyes only
  }
  return false;
}

// A short line the phone may say through the robot voice (only when someone is near)
static void proactiveLine(int s) {
  static const char* lines[] = {"Want a quick quiz?", "Shall we study together?",
                                "I missed you! Want to chat?", "Ready for a short focus session?"};
  if (s < 2) return;                              // nobody near: nobody to talk to
  static unsigned long lastSaid = 0;
  if (lastSaid != 0 && millis() - lastSaid < SAY_GAP_MS) return;   // do not nag
  lastSaid = millis();
  robotSay(lines[random(4)]);          // the phone speaks it with the robot voice
}

static void closeWindow() {
  windowOpen = false;
  lastTryEnd = millis();
  if (!gotFeedback && personSeenNear) reward += -0.2f;     // ignored while someone was there
  // Ignored? Wait twice as long before the next try (10 -> 20 -> 40 -> 60 s). A reaction resets it.
  cooldown = gotFeedback ? COOLDOWN_MS : min(cooldown * 2, MAX_COOLDOWN_MS);
  float before = Q[curState][curMove];
  Q[curState][curMove] += ALPHA * (reward - before);
  epsilon = max(EPS_MIN, epsilon * EPS_DECAY);
  saveDue = true;
  if (reward > 0)      think("%s got %+.1f -> score %.2f (was %.2f)", MOVE_NAMES[curMove], reward, Q[curState][curMove], before);
  else if (reward < 0) think("%s got %+.1f -> score %.2f - I'll try it less", MOVE_NAMES[curMove], reward, Q[curState][curMove]);
  else                 think("No reaction to %s -> score %.2f", MOVE_NAMES[curMove], Q[curState][curMove]);
}

void learnUpdate(float distCm, bool rearBlocked, bool canAct) {
  if (!LEARN_ENABLED || !active) return;
  unsigned long now = millis();
  if (saveDue && now - lastSave >= SAVE_GAP_MS) save();

  if (windowOpen) {
    if (distCm > 0 && distCm < NEAR_CM) personSeenNear = true;
    if (now - windowStart >= WINDOW_MS) closeWindow();
    return;
  }

  // Time to seek attention?
  if (!canAct) return;
  if (moodIdleSeconds() * 1000UL < LONELY_MS) return;
  if (now - lastTryEnd < cooldown) return;

  curState = stateNow(distCm);
  bool approachOk = !(distCm > 0 && distCm <= 25);
  curMove = chooseMove(curState, approachOk);
  if (!doMove(curMove, distCm, rearBlocked)) { lastTryEnd = now; return; }

  if (chosenToExplore) think("Tried %s (exploring, %s)", MOVE_NAMES[curMove], STATE_NAMES[curState]);
  else                 think("Chose %s because it scored best (%.2f, %s)", MOVE_NAMES[curMove], Q[curState][curMove], STATE_NAMES[curState]);
  windowOpen = true;
  triedOnce = true;
  windowStart = now;
  reward = 0;
  gotFeedback = false;
  personSeenNear = distCm > 0 && distCm < NEAR_CM;
  proactiveLine(curState);
}

// Feedback from things that happen during the window
void learnOnEvent(MoodEvent ev) {
  if (!windowOpen) return;
  float r = 0;
  switch (ev) {
    case EV_PET:          r = 1.0f;  break;
    case EV_PRAISE:       r = 1.0f;  break;
    case EV_CHAT_START:   r = 0.5f;  break;
    case EV_QUIZ_CORRECT: r = 0.5f;  break;
    case EV_SUDDEN_CLOSE:
    case EV_LOUD_NOISE:   r = -1.0f; break;
    default: return;
  }
  reward += r;
  gotFeedback = true;
}

bool learnThumb(int dir) {
  if (!windowOpen) return false;
  reward += dir > 0 ? 1.0f : -1.0f;
  gotFeedback = true;
  think("You gave %s a thumbs-%s", MOVE_NAMES[curMove], dir > 0 ? "up" : "down");
  return true;                       // thumbs do not reset "lonely": next try 10 s after the window
}

String learnJson(float distCm) {
  int s = windowOpen ? curState : stateNow(distCm);
  String out = "{\"active\":";
  out += active ? 1 : 0;
  out += ",\"state\":\"";
  out += STATE_NAMES[s];
  out += "\",\"moves\":[";
  for (int m = 0; m < NUM_MOVES; m++) {
    if (m) out += ",";
    out += "{\"name\":\""; out += MOVE_NAMES[m]; out += "\",\"q\":"; out += String(Q[s][m], 2); out += "}";
  }
  out += "],\"eps\":"; out += String(epsilon, 2);
  out += ",\"window\":"; out += windowOpen ? 1 : 0;
  unsigned long left = windowOpen ? (WINDOW_MS - min(WINDOW_MS, millis() - windowStart)) / 1000 : 0;
  out += ",\"left_s\":"; out += left;
  out += ",\"move\":\""; out += windowOpen ? MOVE_NAMES[curMove] : ""; out += "\"";
  out += ",\"last\":\""; out += triedOnce ? MOVE_NAMES[curMove] : ""; out += "\"";
  out += ",\"last_mode\":\""; out += !triedOnce ? "" : chosenToExplore ? "explore" : "exploit"; out += "\"";
  out += "}";
  return out;
}
