// drive.cpp - motor control + behaviour moves.
//
// A behaviour is a short list of steps (left speed, right speed, time).
// Safety rules: PWM <= MAX_PWM, kick-start KICK_PWM for KICK_MS,
// below MIN_PWM = stop, every step 250-600 ms, forward blocked when the front
// is < 15 cm, reverse blocked when the rear IR sees something.
// Shy (low bond) = fewer steps and lower speed, never shorter steps.
// When MOTORS_ENABLED is false the moves are only printed.

#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "drive.h"
#include "thoughts.h"

const int PWM_FREQ = 1000, PWM_BITS = 8;
const int LEDC_LEFT_CH = 0, LEDC_RIGHT_CH = 1;      // timer 0 (buzzer does not use LEDC)
const unsigned long MAX_STEP_MS = 600;               // no step runs longer than this
const float FRONT_STOP_CM = 15;
const int SHY_BOND = 30;                             // bond below this = shy

// Remote control (phone) and "come"
const unsigned long REMOTE_TIMEOUT_MS = 600;         // no command this long -> stop
const unsigned long COME_MS = 5000;
const float COME_STOP_CM = 25;

// Calibration ramp
const int CAL_START_PWM = 60;
const unsigned long CAL_RAMP_MS = 15000;             // 60 -> MAX_PWM in 15 s

// ---------- one motor side ----------
struct Side {
  int en, inA, inB, ledc, dir;   // pins, LEDC channel, direction flag
  int speed;                     // current signed speed (0 = stopped)
  int target;                    // duty after the kick
  unsigned long kickUntil;       // 0 = no kick running
};
static Side leftSide, rightSide;

// ---------- behaviour sequence ----------
struct Step { int left, right; unsigned long ms; };
const int MAX_STEPS = 4;
static Step steps[MAX_STEPS];
static int numSteps = 0, stepIndex = -1;     // -1 = idle
static unsigned long stepStart = 0;

// ---------- calibration ----------
enum CalMode { CAL_NONE, CAL_FORWARD, CAL_SPIN };
static CalMode calMode = CAL_NONE;
static unsigned long calStart = 0;
static int calLastPrinted = -1;

// ---------- remote / come ----------
static bool remoteActive = false;
static unsigned long lastRemoteMs = 0;
static char remoteDir = 's';
static float remoteStopCm = 15;            // forward stop distance for the current remote drive
static bool comeActive = false;
static unsigned long comeStart = 0;

// ===============================================================
// Set one side. calibration = true: no MIN_PWM cut-off and no kick (raw ramp).
static void setSide(Side& s, int speed, bool calibration = false) {
  int duty = min(abs(speed), MAX_PWM);
  if (!calibration && duty < MIN_PWM) duty = 0;
  int newSpeed = (duty == 0) ? 0 : (speed > 0 ? duty : -duty);
  bool starting = (newSpeed != 0) && (s.speed == 0 || (s.speed > 0) != (newSpeed > 0));
  s.speed = newSpeed;
  s.target = duty;
  if (!MOTORS_ENABLED) return;               // disabled: remember only, never drive

  int physical = newSpeed * s.dir;           // apply the direction flag from pins.h
  digitalWrite(s.inA, physical > 0 ? HIGH : LOW);
  digitalWrite(s.inB, physical < 0 ? HIGH : LOW);
  if (newSpeed == 0) {
    ledcWrite(s.ledc, 0);
    s.kickUntil = 0;
  } else if (starting && !calibration) {
    ledcWrite(s.ledc, KICK_PWM);             // short strong push to get moving
    s.kickUntil = millis() + KICK_MS;
  } else if (s.kickUntil == 0) {
    ledcWrite(s.ledc, duty);
  }
}

static void updateKick(Side& s) {
  if (MOTORS_ENABLED && s.kickUntil != 0 && (long)(millis() - s.kickUntil) >= 0) {
    s.kickUntil = 0;
    ledcWrite(s.ledc, s.target);
  }
}

void driveStop() {
  bool wasCal = (calMode != CAL_NONE);
  setSide(leftSide, 0);
  setSide(rightSide, 0);
  stepIndex = -1;
  calMode = CAL_NONE;
  remoteActive = false;
  remoteDir = 's';
  comeActive = false;
  if (wasCal) Serial.printf("[drive] STOP (calibration ended at PWM %d)\n", calLastPrinted);
}

void driveInit() {
  // FIRST: every motor pin LOW so nothing moves at boot
  int pins[] = {PIN_MOTOR_ENA, PIN_MOTOR_ENB, PIN_MOTOR_IN1, PIN_MOTOR_IN2, PIN_MOTOR_IN3, PIN_MOTOR_IN4};
  for (int p : pins) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }

  // Channel A (ENA/IN1/IN2) and B (ENB/IN3/IN4), mapped to sides by pins.h
  Side a = {PIN_MOTOR_ENA, PIN_MOTOR_IN1, PIN_MOTOR_IN2, LEDC_LEFT_CH, 1, 0, 0, 0};
  Side b = {PIN_MOTOR_ENB, PIN_MOTOR_IN3, PIN_MOTOR_IN4, LEDC_RIGHT_CH, 1, 0, 0, 0};
  leftSide  = MOTOR_SIDES_SWAPPED ? b : a;
  rightSide = MOTOR_SIDES_SWAPPED ? a : b;
  leftSide.dir  = LEFT_MOTOR_DIR;
  rightSide.dir = RIGHT_MOTOR_DIR;

  if (MOTORS_ENABLED) {
    ledcSetup(leftSide.ledc, PWM_FREQ, PWM_BITS);
    ledcSetup(rightSide.ledc, PWM_FREQ, PWM_BITS);
    ledcAttachPin(leftSide.en, leftSide.ledc);
    ledcAttachPin(rightSide.en, rightSide.ledc);
    ledcWrite(leftSide.ledc, 0);
    ledcWrite(rightSide.ledc, 0);
  }
}

// Add one step to the sequence (never shorter than MIN_STEP_MS)
static void addStep(int left, int right, unsigned long ms) {
  if (numSteps >= MAX_STEPS) return;
  steps[numSteps++] = {left, right, max(ms, (unsigned long)MIN_STEP_MS)};
}

// Start a behaviour move for an emotion (if there is one)
void driveBehaviour(Emotion e, float distCm, bool rearBlocked, int bond) {
  if (calMode != CAL_NONE || remoteActive || comeActive) return;   // phone / calibration has priority
  bool shy = (bond < SHY_BOND);
  int drive = shy ? max(DRIVE_PWM - SHY_PWM_DROP, MIN_PWM) : DRIVE_PWM;
  int turn  = shy ? max(TURN_PWM  - SHY_PWM_DROP, MIN_PWM) : TURN_PWM;
  numSteps = 0;
  const char* name = nullptr;

  switch (e) {
    case EMO_HAPPY:                           // wiggle: left/right spins (shy: 1, else 2)
      addStep(-turn,  turn, 250);
      addStep( turn, -turn, 250);
      if (!shy) { addStep(-turn, turn, 250); addStep(turn, -turn, 250); }
      name = "happy wiggle";
      break;
    case EMO_SCARED:                          // back away, only if the rear is clear
      if (rearBlocked) { think("Wanted to back away, but something is behind me"); return; }
      addStep(-drive, -drive, shy ? 300 : 600);
      name = "scared reverse";
      break;
    case EMO_CURIOUS:                         // one small step towards the object
      if (distCm > 0 && distCm <= 25) { think("Curious, but too close to step forward"); return; }
      addStep(drive, drive, 300);
      name = "curious step";
      break;
    default:
      return;                                 // other emotions do not move
  }
  think("Chose %s (%d step%s%s)", name, numSteps, numSteps > 1 ? "s" : "",
        shy ? ", shy: still getting to know you" : "");
  stepIndex = 0;
  stepStart = millis();
  setSide(leftSide, steps[0].left);
  setSide(rightSide, steps[0].right);
}

// ---------- social moves for learning (G1a) ----------
// Same safety rules as every other move (step 250-600 ms, front/rear checks).
bool driveSocial(SocialMove m, float distCm, bool rearBlocked) {
  if (calMode != CAL_NONE || remoteActive || comeActive || stepIndex >= 0) return false;
  numSteps = 0;
  switch (m) {
    case MOVE_WIGGLE:                          // small left/right shuffle
      addStep(-TURN_PWM, TURN_PWM, 250); addStep(TURN_PWM, -TURN_PWM, 250);
      addStep(-TURN_PWM, TURN_PWM, 250); addStep(TURN_PWM, -TURN_PWM, 250);
      break;
    case MOVE_SPIN_DANCE:                      // bigger turns one way, then the other
      addStep(-TURN_PWM, TURN_PWM, 500); addStep(TURN_PWM, -TURN_PWM, 500);
      addStep(-TURN_PWM, TURN_PWM, 500);
      break;
    case MOVE_SLOW_APPROACH:                   // one slow step towards the person
      if (distCm > 0 && distCm <= 25) return false;
      addStep(max(MIN_PWM, DRIVE_PWM - 20), max(MIN_PWM, DRIVE_PWM - 20), 400);
      break;
    default:
      return false;
  }
  stepIndex = 0;
  stepStart = millis();
  setSide(leftSide, steps[0].left);
  setSide(rightSide, steps[0].right);
  return true;
}

// ---------- calibration ramps ----------
static void startCalibration(CalMode m, const char* name) {
  driveStop();
  calMode = m;
  calStart = millis();
  calLastPrinted = -1;
  Serial.printf("[drive] CALIBRATION %s: PWM %d -> %d over 15 s, no kick. Type x when it starts moving.%s\n",
                name, CAL_START_PWM, MAX_PWM, MOTORS_ENABLED ? "" : "  -- MOTORS DISABLED");
}
void driveCalibrateForward() { startCalibration(CAL_FORWARD, "forward"); }
void driveCalibrateSpin()    { startCalibration(CAL_SPIN, "spin (left)"); }
bool driveCalibrating()      { return calMode != CAL_NONE; }

static void updateCalibration(float distCm) {
  unsigned long t = millis() - calStart;
  if (t >= CAL_RAMP_MS) {
    Serial.println("[drive] calibration ramp finished");
    driveStop();
    return;
  }
  int pwm = CAL_START_PWM + (int)((MAX_PWM - CAL_START_PWM) * t / CAL_RAMP_MS);
  if (calMode == CAL_FORWARD) {
    if (distCm > 0 && distCm < FRONT_STOP_CM) {
      Serial.println("[drive] STOP: obstacle in front");
      driveStop();
      return;
    }
    setSide(leftSide, pwm, true);
    setSide(rightSide, pwm, true);
  } else {
    setSide(leftSide, -pwm, true);           // spin left on the spot
    setSide(rightSide, pwm, true);
  }
  if (pwm / 5 != calLastPrinted / 5) {
    Serial.printf("  cal PWM %d\n", pwm);
  }
  calLastPrinted = pwm;
}

// ---------- remote control ----------
bool driveRemote(char d, int speedPct, float distCm, bool rearBlocked, float stopCm) {
  stepIndex = -1;                            // the phone overrides behaviours...
  calMode = CAL_NONE;                        // ...and calibration
  comeActive = false;
  speedPct = constrain(speedPct, 0, 100);
  if (d == 's' || speedPct == 0) {
    if (remoteActive) Serial.println("[drive] remote: stop");
    driveStop();
    return true;
  }
  remoteStopCm = max(stopCm, FRONT_STOP_CM);
  if (d == 'f' && distCm > 0 && distCm < remoteStopCm) { driveStop(); return false; }
  if (d == 'b' && rearBlocked)                          { driveStop(); return false; }

  // 1-100 % -> MIN_PWM..MAX_PWM; spins need at least TURN_PWM (4WD skid-steer)
  int pwm  = MIN_PWM + (MAX_PWM - MIN_PWM) * speedPct / 100;
  int turn = min(MAX_PWM, max(pwm, TURN_PWM));
  switch (d) {
    case 'f': setSide(leftSide,  pwm);  setSide(rightSide,  pwm);  break;
    case 'b': setSide(leftSide, -pwm);  setSide(rightSide, -pwm);  break;
    case 'l': setSide(leftSide, -turn); setSide(rightSide,  turn); break;
    case 'r': setSide(leftSide,  turn); setSide(rightSide, -turn); break;
    default:  driveStop(); return false;
  }
  if (d != remoteDir) Serial.printf("[drive] remote: %c at %d%% (PWM %d)\n", d, speedPct, (d == 'l' || d == 'r') ? turn : pwm);
  remoteDir = d;
  remoteActive = true;
  lastRemoteMs = millis();
  return true;
}

void driveCome(float distCm) {
  driveStop();
  if (distCm > 0 && distCm <= COME_STOP_CM) {
    Serial.println("[drive] come: already close");
    return;
  }
  Serial.println("[drive] come: driving forward (max 5 s, stops at 25 cm)");
  comeActive = true;
  comeStart = millis();
  setSide(leftSide, DRIVE_PWM);
  setSide(rightSide, DRIVE_PWM);
}

void driveUpdate(float distCm, bool rearBlocked) {
  updateKick(leftSide);
  updateKick(rightSide);

  // Safety checks for EVERY kind of movement
  bool forward = leftSide.speed > 0 && rightSide.speed > 0;
  bool reverse = leftSide.speed < 0 && rightSide.speed < 0;
  if (forward && distCm > 0 && distCm < FRONT_STOP_CM) {
    Serial.println("[drive] STOP: obstacle in front");
    driveStop(); return;
  }
  if (reverse && rearBlocked) {
    Serial.println("[drive] STOP: obstacle behind");
    driveStop(); return;
  }

  if (calMode != CAL_NONE) { updateCalibration(distCm); return; }

  // Remote forward ("go" uses 25 cm): stop as soon as something is that close
  if (remoteActive && forward && distCm > 0 && distCm < remoteStopCm) {
    Serial.println("[drive] remote: something in front -> stop");
    driveStop(); return;
  }

  // Phone watchdog: no command for 600 ms -> stop
  if (remoteActive && millis() - lastRemoteMs > REMOTE_TIMEOUT_MS) {
    Serial.println("[drive] remote: no command for 600 ms -> stop");
    driveStop(); return;
  }

  // "Come": stop when close enough or after 5 s
  if (comeActive) {
    if (distCm > 0 && distCm <= COME_STOP_CM) { Serial.println("[drive] come: arrived"); driveStop(); }
    else if (millis() - comeStart >= COME_MS) { Serial.println("[drive] come: time up");  driveStop(); }
    return;
  }

  if (stepIndex < 0) return;

  // Next step when this one is over (never longer than MAX_STEP_MS)
  unsigned long len = min(steps[stepIndex].ms, MAX_STEP_MS);
  if (millis() - stepStart >= len) {
    stepIndex++;
    if (stepIndex >= numSteps) { driveStop(); return; }
    stepStart = millis();
    setSide(leftSide, steps[stepIndex].left);
    setSide(rightSide, steps[stepIndex].right);
  }
}

bool driveBusy() { return stepIndex >= 0 || calMode != CAL_NONE || remoteActive || comeActive; }
