// sensors.cpp - reads the sensors and turns raw numbers into events.
//
// Distance: one ping every 50 ms, timeouts ignored, median of the last 5.
// In an open room the sensor often sees NOTHING (no echo within ~2 m), so a
// hand entering the beam "appears" at once. The rules are built around that:
//
//   sudden close  = 2 medians in a row < 15 cm reached FAST (0.5 s earlier it was
//                   > 25 cm or nothing; a hand slowly drifting closer is not scary),
//                   OR the distance (a real echo
//                   0.5 s ago) dropped by > 30 cm to under 30 cm, seen on 2
//                   checks in a row. Re-arms only after the front has been
//                   clear (> 25 cm) for 1.5 s, so a parked object scares once.
//   slow approach = front clear for 1 s, then something stays at 20-60 cm for
//                   0.4 s (a hand that rushes in to < 15 cm is "scared" instead)
//   pet           = within 15 s of an approach, a hand held steady (moves < 15 cm)
//                   at 15-35 cm for 2 s. Then the hand must be gone for 1 s
//                   (and at least 3 s must pass) before the next pet.
//                   A real touch on the TTP223 also counts (TOUCH_ENABLED).

#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "sensors.h"

// ---------- tunable numbers ----------
const unsigned long PING_INTERVAL_MS  = 50;
const unsigned long ECHO_TIMEOUT_US   = 12000;  // 12 ms ~ 2 m
const unsigned long NO_ECHO_AFTER_MS  = 1000;   // no valid echo this long -> nothing in range

const float CLOSE_CM                  = 15;     // sudden close: closer than this (2x)
const float DROP_CM                   = 30;     // or a drop of more than this...
const float DROP_TO_CM                = 30;     // ...ending closer than this
const unsigned long DROP_CHECK_MS     = 100;    // checked every 100 ms against 0.5 s ago
const float REARM_CM                  = 25;     // front must be clear (> 25 cm)...
const unsigned long REARM_MS          = 1500;   // ...this long before it can startle again

const float CLEAR_CM                  = 90;     // "nothing near" for the approach rule
const unsigned long APPROACH_ARM_MS   = 1000;   // front clear this long arms the approach
const float APPROACH_MIN_CM           = 20;
const float APPROACH_MAX_CM           = 60;
const unsigned long APPROACH_HOLD_MS  = 400;    // must stay in the band this long

const float PET_MIN_CM                = 15;
const float PET_MAX_CM                = 35;
const float PET_STEADY_CM             = 15;     // hand may wobble this much
const unsigned long PET_HOLD_MS       = 2000;
const unsigned long PET_AFTER_APPROACH_MS = 15000;
const float PET_GONE_CM               = 50;     // hand counts as gone beyond this...
const unsigned long PET_GONE_MS       = 1000;   // ...for this long (re-arms the pet)
const unsigned long PET_MIN_GAP_MS    = 3000;   // never two pets closer than this

// ---------- distance state ----------
const int NUM_SAMPLES = 5;
static float samples[NUM_SAMPLES];
static int sampleCount = 0, sampleIndex = 0;
static unsigned long lastPing = 0, lastValidEcho = 0;
static float median = -1;                   // -1 = nothing in range

// ---------- event state ----------
static int  closeCount = 0;
static bool suddenArmed = true;
static unsigned long clearSince = 0;        // front clear (> REARM_CM) since (0 = not clear)
const int HIST = 6;                         // medians every 100 ms -> oldest is 0.5 s ago
static float hist[HIST];                    // -1 = no echo at that moment
static int histIndex = 0, histCount = 0;
static unsigned long lastDropCheck = 0;
static int dropCount = 0;
static float halfSecAgo = -1;               // median 0.5 s ago (-1 = nothing / unknown)

static unsigned long farSince = 0;          // front "nothing near" since (0 = not)
static bool approachArmed = false;
static unsigned long approachBandSince = 0;
static unsigned long lastApproachMs = 0;

static unsigned long hoverStart = 0;        // hand in the pet band since (0 = not)
static float hoverMin = 0, hoverMax = 0;
static bool petArmed = true;
static unsigned long goneSince = 0;
static unsigned long lastPetMs = 0;

static bool touchNow = false, touchBefore = false;

// ===============================================================
static float pingCm() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  unsigned long us = pulseIn(PIN_US_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1;                   // no echo
  return us / 58.0;
}

static float computeMedian() {
  float s[NUM_SAMPLES];
  for (int i = 0; i < sampleCount; i++) s[i] = samples[i];
  for (int i = 1; i < sampleCount; i++) {   // insertion sort, only 5 values
    float v = s[i]; int j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  return s[sampleCount / 2];
}

// "Has this condition been true for at least ms?"  (since = 0 means not true)
static bool heldFor(unsigned long& since, bool condition, unsigned long ms, unsigned long now) {
  if (!condition) { since = 0; return false; }
  if (since == 0) since = now;
  return now - since >= ms;
}

void sensorsInit() {
  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  pinMode(PIN_IR_REAR, INPUT);
  if (TOUCH_ENABLED) pinMode(PIN_TOUCH, INPUT);
}

int sensorsUpdate() {
  int events = SE_NONE;
  unsigned long now = millis();

  // ---- touch (only when the TTP223 is fitted) ----
  if (TOUCH_ENABLED) {
    touchNow = (digitalRead(PIN_TOUCH) == HIGH);
    if (touchNow && !touchBefore && now - lastPetMs >= PET_MIN_GAP_MS) {
      events |= SE_PET;
      lastPetMs = now;
    }
    touchBefore = touchNow;
  }

  // ---- distance: one ping every 50 ms ----
  if (now - lastPing < PING_INTERVAL_MS) return events;
  lastPing = now;

  float cm = pingCm();
  if (cm > 0) {
    samples[sampleIndex] = cm;
    sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
    if (sampleCount < NUM_SAMPLES) sampleCount++;
    lastValidEcho = now;
    median = computeMedian();
  } else if (now - lastValidEcho > NO_ECHO_AFTER_MS) {
    median = -1;                            // nothing in range for a while
    sampleCount = 0;
  }
  bool seen = (median > 0);                 // is something in front at all?
  float d = median;

  // ================= SUDDEN CLOSE (-> scared) =================
  // re-arm after the front has been clear for 1.5 s
  if (heldFor(clearSince, !seen || d > REARM_CM, REARM_MS, now)) suddenArmed = true;

  // rule 1: two medians in a row closer than 15 cm, and it got there fast
  // (half a second ago it was further than 25 cm, or nothing was there)
  closeCount = (seen && d < CLOSE_CM) ? closeCount + 1 : 0;
  bool cameFast = (halfSecAgo < 0 || halfSecAgo > REARM_CM);
  if (suddenArmed && closeCount >= 2 && cameFast) {
    events |= SE_SUDDEN_CLOSE;
    suddenArmed = false;
  }

  // rule 2: a real echo 0.5 s ago, now > 30 cm closer AND under 30 cm (twice)
  if (now - lastDropCheck >= DROP_CHECK_MS) {
    lastDropCheck = now;
    hist[histIndex] = seen ? d : -1;
    histIndex = (histIndex + 1) % HIST;
    if (histCount < HIST) histCount++;
    if (histCount == HIST) {
      float old = hist[histIndex];          // the slot we write next = 0.5 s ago
      halfSecAgo = old;
      bool dropped = seen && old > 0 && (old - d) > DROP_CM && d < DROP_TO_CM;
      dropCount = dropped ? dropCount + 1 : 0;
      if (suddenArmed && dropCount >= 2) {
        events |= SE_SUDDEN_CLOSE;
        suddenArmed = false;
      }
    }
  }

  // ================= SLOW APPROACH (-> curious) =================
  if (heldFor(farSince, !seen || d > CLEAR_CM, APPROACH_ARM_MS, now)) approachArmed = true;
  bool inApproachBand = seen && d >= APPROACH_MIN_CM && d <= APPROACH_MAX_CM;
  if (approachArmed && heldFor(approachBandSince, inApproachBand, APPROACH_HOLD_MS, now)) {
    if (!(events & SE_SUDDEN_CLOSE)) {
      events |= SE_SLOW_APPROACH;
      lastApproachMs = now;
    }
    approachArmed = false;                  // one event per approach
  }
  if (seen && d < CLOSE_CM) approachArmed = false;   // rushed in: that was not "slow"

  // ================= PET (-> happy) =================
  // re-arm once the hand has been gone for 1 s
  if (heldFor(goneSince, !seen || d > PET_GONE_CM, PET_GONE_MS, now)) petArmed = true;

  bool inPetBand = seen && d >= PET_MIN_CM && d <= PET_MAX_CM;
  if (!inPetBand) {
    hoverStart = 0;
  } else if (hoverStart == 0) {
    hoverStart = now; hoverMin = hoverMax = d;
  } else {
    hoverMin = min(hoverMin, d);
    hoverMax = max(hoverMax, d);
    if (hoverMax - hoverMin > PET_STEADY_CM) {   // not steady: start the 2 s again
      hoverStart = now; hoverMin = hoverMax = d;
    }
  }
  bool recentApproach = (lastApproachMs != 0 && now - lastApproachMs <= PET_AFTER_APPROACH_MS);
  if (petArmed && recentApproach && hoverStart != 0 && now - hoverStart >= PET_HOLD_MS
      && now - lastPetMs >= PET_MIN_GAP_MS) {
    events |= SE_PET;
    petArmed = false;                       // take the hand away to pet again
    lastPetMs = now;
  }

  return events;
}

float sensorsDistance()     { return median; }
bool  sensorsRearObstacle() { return digitalRead(PIN_IR_REAR) == LOW; }   // LOW = obstacle
bool  sensorsTouched()      { return TOUCH_ENABLED && touchNow; }
