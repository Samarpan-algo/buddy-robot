// Step 1.2 - Motor test (WHEELS LIFTED!)
// Nothing moves on its own. Type one letter in the Serial Monitor:
//   a = channel A (ENA/IN1/IN2) forward 1 s     b = channel B (ENB/IN3/IN4) forward 1 s
//   f = forward 1 s   k = back 1 s   l = spin left 1 s   r = spin right 1 s
//   p = ramp UP   60 -> 180 over 12 s, NO kick-start  (find the start PWM)
//   d = ramp DOWN 180 -> 60 over 12 s, with kick-start (find the keep-turning PWM)
//   s = stop now      ? = show this menu
// Every move stops by itself.
// Kick-start: when a side starts from standstill or reverses, it gets KICK_PWM
// for KICK_MS, then drops to the requested speed (never above MAX_PWM).
// Uses the Arduino-ESP32 2.x LEDC API (ledcSetup / ledcAttachPin / ledcWrite), 1 kHz.

#include <Arduino.h>
#include "pins.h"

const int PWM_FREQ = 1000;      // 1 kHz PWM (lower frequencies did not help)
const int PWM_BITS = 8;         // duty 0-255
const int TEST_PWM = 120;       // speed for the 1-second moves (after the kick)

const unsigned long MOVE_MS = 1000;
const int RAMP_LOW = 60;
const unsigned long RAMP_MS = 12000;   // slow, so the user can press s at the right moment

// ----- One L298N channel -----
struct Channel {
  int inA, inB;               // direction pins
  int ledc;                   // LEDC channel number for the enable pin
  int speed;                  // current signed speed (-255..255, 0 = stopped)
  int target;                 // duty to use once the kick is over
  unsigned long kickUntil;    // millis() when the kick ends (0 = no kick running)
};
Channel chA = {PIN_MOTOR_IN1, PIN_MOTOR_IN2, 0, 0, 0, 0};
Channel chB = {PIN_MOTOR_IN3, PIN_MOTOR_IN4, 1, 0, 0, 0};

// What the test is doing right now
enum Mode { IDLE, TIMED_MOVE, RAMP_UP, RAMP_DOWN };
Mode mode = IDLE;
unsigned long modeStart = 0;
int lastPrintedPwm = -1;

// ---------------------------------------------------------------
// Drive one channel. speed: -255..255 (negative = backwards, 0 = stop).
// useKick: give a short KICK_PWM burst if starting from stop or reversing.
void driveChannel(Channel &ch, int speed, bool useKick) {
  int duty = min(abs(speed), MAX_PWM);
  if (duty < MIN_PWM) duty = 0;                  // too slow to turn -> stop
  int newSpeed = (duty == 0) ? 0 : (speed > 0 ? duty : -duty);

  if (newSpeed == 0) {                           // stop
    digitalWrite(ch.inA, LOW);
    digitalWrite(ch.inB, LOW);
    ledcWrite(ch.ledc, 0);
    ch.speed = 0;
    ch.kickUntil = 0;
    return;
  }

  // Starting from standstill, or changing direction?
  bool starting = (ch.speed == 0) || ((ch.speed > 0) != (newSpeed > 0));

  digitalWrite(ch.inA, newSpeed > 0 ? HIGH : LOW);
  digitalWrite(ch.inB, newSpeed > 0 ? LOW : HIGH);
  ch.target = duty;
  ch.speed = newSpeed;

  if (useKick && starting) {
    ledcWrite(ch.ledc, KICK_PWM);                // short strong push
    ch.kickUntil = millis() + KICK_MS;
  } else if (ch.kickUntil == 0) {
    ledcWrite(ch.ledc, duty);                    // normal speed
  }                                              // else: kick running, updateKick() sets target later
}

// Ends a kick when its time is up (called every loop)
void updateKick(Channel &ch) {
  if (ch.kickUntil != 0 && (long)(millis() - ch.kickUntil) >= 0) {
    ch.kickUntil = 0;
    ledcWrite(ch.ledc, ch.target);
  }
}

// Drive one SIDE of the car, using the swap flag and direction flags from pins.h
void driveLeft(int speed, bool kick)  { driveChannel(MOTOR_SIDES_SWAPPED ? chB : chA, speed * LEFT_MOTOR_DIR,  kick); }
void driveRight(int speed, bool kick) { driveChannel(MOTOR_SIDES_SWAPPED ? chA : chB, speed * RIGHT_MOTOR_DIR, kick); }

void stopAll() {
  driveChannel(chA, 0, false);
  driveChannel(chB, 0, false);
  mode = IDLE;
}

void printMenu() {
  Serial.println();
  Serial.println("=== Motor test menu (wheels lifted!) ===");
  Serial.println(" a = channel A fwd 1s   b = channel B fwd 1s");
  Serial.println(" f = forward 1s   k = back 1s   l = spin left 1s   r = spin right 1s");
  Serial.println(" p = ramp UP 60->180 (12 s, no kick)   d = ramp DOWN 180->60 (12 s, kick)");
  Serial.println(" s = stop   ? = menu");
  Serial.printf (" MAX_PWM=%d KICK=%d/%dms MIN_PWM=%d  SWAPPED=%d LEFT_DIR=%d RIGHT_DIR=%d\n",
                 MAX_PWM, KICK_PWM, KICK_MS, MIN_PWM,
                 MOTOR_SIDES_SWAPPED, LEFT_MOTOR_DIR, RIGHT_MOTOR_DIR);
}

// Start a 1-second move with kick-start (signed speeds per side)
void startMove(const char* name, int left, int right) {
  stopAll();
  Serial.printf("[%lu ms] %s  kick %d for %d ms, then PWM %d\n",
                millis(), name, KICK_PWM, KICK_MS, TEST_PWM);
  driveLeft(left, true);
  driveRight(right, true);
  mode = TIMED_MOVE;
  modeStart = millis();
}

void handleCommand(char c) {
  switch (c) {
    case 'a':
      stopAll();
      Serial.printf("[%lu ms] channel A (ENA/IN1/IN2) forward  kick %d, then PWM %d\n",
                    millis(), KICK_PWM, TEST_PWM);
      driveChannel(chA, TEST_PWM, true);
      mode = TIMED_MOVE; modeStart = millis();
      break;
    case 'b':
      stopAll();
      Serial.printf("[%lu ms] channel B (ENB/IN3/IN4) forward  kick %d, then PWM %d\n",
                    millis(), KICK_PWM, TEST_PWM);
      driveChannel(chB, TEST_PWM, true);
      mode = TIMED_MOVE; modeStart = millis();
      break;
    case 'f': startMove("forward",    TEST_PWM,  TEST_PWM); break;
    case 'k': startMove("back",      -TEST_PWM, -TEST_PWM); break;
    case 'l': startMove("spin left", -TEST_PWM,  TEST_PWM); break;
    case 'r': startMove("spin right", TEST_PWM, -TEST_PWM); break;
    case 'p':
      stopAll();
      Serial.printf("[%lu ms] ramp UP %d -> %d over 12 s, NO kick. Press s when wheels START.\n",
                    millis(), RAMP_LOW, MAX_PWM);
      mode = RAMP_UP; modeStart = millis(); lastPrintedPwm = -1;
      break;
    case 'd':
      stopAll();
      Serial.printf("[%lu ms] ramp DOWN %d -> %d over 12 s, with kick. Press s when wheels STOP.\n",
                    millis(), MAX_PWM, RAMP_LOW);
      driveLeft(MAX_PWM, true);
      driveRight(MAX_PWM, true);
      mode = RAMP_DOWN; modeStart = millis(); lastPrintedPwm = -1;
      break;
    case 's':
      stopAll();
      Serial.printf("[%lu ms] STOP\n", millis());
      break;
    case '?':
      printMenu();
      break;
    default:
      break;   // ignore Enter / other keys
  }
}

void setup() {
  // FIRST: all motor pins LOW so the motors are off at boot
  pinMode(PIN_MOTOR_ENA, OUTPUT); digitalWrite(PIN_MOTOR_ENA, LOW);
  pinMode(PIN_MOTOR_ENB, OUTPUT); digitalWrite(PIN_MOTOR_ENB, LOW);
  pinMode(PIN_MOTOR_IN1, OUTPUT); digitalWrite(PIN_MOTOR_IN1, LOW);
  pinMode(PIN_MOTOR_IN2, OUTPUT); digitalWrite(PIN_MOTOR_IN2, LOW);
  pinMode(PIN_MOTOR_IN3, OUTPUT); digitalWrite(PIN_MOTOR_IN3, LOW);
  pinMode(PIN_MOTOR_IN4, OUTPUT); digitalWrite(PIN_MOTOR_IN4, LOW);

  // PWM on the two enable pins, starting at 0 (off)
  ledcSetup(chA.ledc, PWM_FREQ, PWM_BITS);
  ledcSetup(chB.ledc, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PIN_MOTOR_ENA, chA.ledc);
  ledcAttachPin(PIN_MOTOR_ENB, chB.ledc);
  ledcWrite(chA.ledc, 0);
  ledcWrite(chB.ledc, 0);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Buddy - Step 1.2 motor test (kick-start). Motors are OFF.");
  printMenu();
}

void loop() {
  // Read one command letter from the Serial Monitor
  if (Serial.available()) {
    handleCommand((char)Serial.read());
  }

  // Read the clock AFTER handling a command, so "now" is never
  // earlier than modeStart (that would make now - modeStart wrap around)
  unsigned long now = millis();

  updateKick(chA);
  updateKick(chB);

  // A 1-second move ends by itself
  if (mode == TIMED_MOVE && now - modeStart >= MOVE_MS) {
    stopAll();
    Serial.printf("[%lu ms] stopped\n", now);
  }

  // Ramps: PWM changes in a straight line over 12 s, printed every 5 steps
  if (mode == RAMP_UP || mode == RAMP_DOWN) {
    unsigned long t = now - modeStart;
    if (t >= RAMP_MS) {
      stopAll();
      Serial.printf("[%lu ms] ramp done, stopped\n", now);
    } else {
      int span = (int)((MAX_PWM - RAMP_LOW) * t / RAMP_MS);
      int pwm = (mode == RAMP_UP) ? RAMP_LOW + span : MAX_PWM - span;
      driveLeft(pwm, false);      // no new kick during the ramp
      driveRight(pwm, false);
      if (pwm / 5 != lastPrintedPwm / 5) {
        Serial.printf("  ramp PWM %d\n", pwm);
        lastPrintedPwm = pwm;
      }
    }
  }
}
