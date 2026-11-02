// pins.h - the ONE place where every pin of Buddy is defined.
// This is the project's pin map (also listed in README.md). Change pins only here.
// Never use GPIO 6-11 (flash) or GPIO12 (boot strapping).
#pragma once

// ---------- Onboard ----------
#define PIN_ONBOARD_LED   2    // blue LED on the DevKit (same pin as the WS2812 data line)

// ---------- OLED 1.3" (I2C, SH1106, address 0x3C) ----------
#define PIN_OLED_SDA      21
#define PIN_OLED_SCL      22   // labelled "SCK" on the OLED
#define OLED_I2C_ADDR     0x3C

// ---------- L298N motor driver ----------
#define PIN_MOTOR_ENA     25   // left motor speed (PWM), ENA jumper removed
#define PIN_MOTOR_IN1     26   // left motor direction
#define PIN_MOTOR_IN2     27   // left motor direction
#define PIN_MOTOR_IN3     32   // right motor direction
#define PIN_MOTOR_IN4     33   // right motor direction
#define PIN_MOTOR_ENB     23   // right motor speed (PWM), ENB jumper removed

// Motor battery is 3 x 18650 (11.1-12.6 V), motors rated 3-6 V, 2 motors per
// L298N channel. PWM limits (0-255), decided in motor test step 1.2:
#define MAX_PWM           180  // highest PWM for continuous driving
#define KICK_PWM          200  // short burst to get a stopped side moving (absolute ceiling)
#define KICK_MS           150  // length of that burst
#define MIN_PWM           75   // below this -> treated as stop. 1.2 test: wheels kept turning down to 64 (+10)
// Behaviour speeds (step 1.4c; re-measure on the floor with the 'f' / 'r' calibration ramps)
#define DRIVE_PWM         140  // straight moves (forward step, scared reverse)
#define TURN_PWM          160  // spins / wiggles (4WD skid-steer needs more torque), max MAX_PWM
#define SHY_PWM_DROP      25   // low bond: this much slower (never below MIN_PWM)
#define MIN_STEP_MS       250  // no movement step shorter than this (kick 150 ms + time at speed)

// Channel A = ENA/IN1/IN2, channel B = ENB/IN3/IN4.
// 0: channel A drives the LEFT side, B the RIGHT side.
// 1: swapped (A drives RIGHT, B drives LEFT). Set from the step 1.2 test.
#define MOTOR_SIDES_SWAPPED 0

// If a side spins backwards on "forward", change its value from 1 to -1
#define LEFT_MOTOR_DIR    1
#define RIGHT_MOTOR_DIR   -1   // step 1.2: channel B ran backwards on "forward"

// ---------- HC-SR04 ultrasonic (front) ----------
#define PIN_US_TRIG       17
#define PIN_US_ECHO       16   // through the 1k / 2k voltage divider!

// ---------- IR obstacle sensor (rear) ----------
#define PIN_IR_REAR       34   // input only; usually LOW = obstacle

// ---------- TTP223 touch (top of the car) ----------
#define PIN_TOUCH         4    // HIGH = touched

// ---------- Buzzer ----------
#define PIN_BUZZER        19   // + leg; - leg to GND

// ---------- WS2812 RGB LED (replaces the red/green LEDs, step 1.3) ----------
#define PIN_RGB_LED       2    // DI through 330 ohm; 5V from the power block; GND
#define RGB_NUM_PIXELS    1
#define RGB_MAX_BRIGHTNESS 60  // 0-255, keep it low
// GPIO13 and GPIO15 are free now (red/green LEDs dropped).

// ---------- Reserved for later (do not use for anything else) ----------
#define PIN_MIC_SCK       14   // Phase 4 INMP441
#define PIN_MIC_WS        18   // Phase 4 INMP441
#define PIN_MIC_SD        35   // Phase 4 INMP441 (input only)
#define PIN_AUDIO_OUT     5    // Phase 4 PWM audio -> RC filter -> PAM8403
