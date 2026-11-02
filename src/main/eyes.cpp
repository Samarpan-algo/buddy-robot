// eyes.cpp - the ONLY file that includes RoboEyes (its short macros such as
// N, S, ON, OFF, HAPPY, TIRED, DEFAULT must not leak into other modules).

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "pins.h"
#include "eyes.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RoboEyes must be included AFTER the display object exists
#include <FluxGarage_RoboEyes.h>
static RoboEyes<Adafruit_SH1106G> eyes(display);

static Emotion current = EMO_NEUTRAL;
static unsigned long lastLaugh = 0;   // "excited" giggles once per second

// Put the eyes back to the plain default look
static void resetEyes() {
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  eyes.setIdleMode(OFF);
  eyes.setCuriosity(OFF);
  eyes.setHFlicker(OFF);
  eyes.setVFlicker(OFF);
  eyes.setSweat(OFF);
  eyes.setAutoblinker(ON, 3, 2);    // blink every 3-5 s
  eyes.open();
}

void eyesInit() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(OLED_I2C_ADDR, true)) {
    Serial.println("[eyes] display.begin() FAILED");
  }
  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 30);   // max 30 frames per second
  resetEyes();
}

void eyesSetEmotion(Emotion e) {
  current = e;
  resetEyes();
  switch (e) {
    case EMO_NEUTRAL:  break;
    case EMO_HAPPY:    eyes.setMood(HAPPY); break;
    case EMO_SAD:      eyes.setMood(TIRED); eyes.setPosition(S); break;
    case EMO_SCARED:   eyes.setHFlicker(ON, 2); eyes.setSweat(ON); break;
    case EMO_CURIOUS:  eyes.setIdleMode(ON, 1, 1); eyes.setCuriosity(ON); break;
    case EMO_SLEEPY:   eyes.setAutoblinker(OFF); eyes.setMood(TIRED); eyes.close(); break;
    case EMO_EXCITED:  eyes.setMood(HAPPY); eyes.anim_laugh(); lastLaugh = millis(); break;
    case EMO_THINKING: eyes.setPosition(N); break;
    case EMO_TALKING:  eyes.setVFlicker(ON, 2); break;
    default: break;
  }
}

static unsigned long textUntil = 0;     // 0 = no text, show the eyes

void eyesShowText(const char* line1, const char* line2, const char* line3, unsigned long ms) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 4);  display.println(line1);
  display.setCursor(0, 24); display.println(line2);
  display.setCursor(0, 44); display.println(line3);
  display.display();
  textUntil = millis() + ms;
  if (textUntil == 0) textUntil = 1;
}

void eyesShowCountdown(const char* title, unsigned long secondsLeft) {
  char mmss[8];
  snprintf(mmss, sizeof(mmss), "%02lu:%02lu", secondsLeft / 60, secondsLeft % 60);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - 6 * strlen(title)) / 2, 2);
  display.println(title);
  display.setTextSize(3);                 // 18 x 24 pixels per character
  display.setCursor((SCREEN_WIDTH - 18 * strlen(mmss)) / 2, 24);
  display.println(mmss);
  display.display();
  textUntil = millis() + 1500;
}

void eyesShowBig(const char* title, const char* big, unsigned long ms) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - 6 * strlen(title)) / 2, 4);
  display.println(title);
  display.setTextSize(3);
  display.setCursor((SCREEN_WIDTH - 18 * strlen(big)) / 2, 26);
  display.println(big);
  display.display();
  textUntil = millis() + ms;
  if (textUntil == 0) textUntil = 1;
}

void eyesUpdate() {
  if (textUntil != 0) {                   // text on screen: leave it until time is up
    if ((long)(millis() - textUntil) < 0) return;
    textUntil = 0;
  }
  if (current == EMO_EXCITED && millis() - lastLaugh >= 1000) {
    lastLaugh = millis();
    eyes.anim_laugh();
  }
  eyes.update();
}
