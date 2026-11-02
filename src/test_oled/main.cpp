// Step 1.1 - OLED eyes test
// 1. Scans the I2C bus and prints every address found (expect 0x3C).
// 2. Shows a test pattern for 4 s (border + text) to check the SH1106 driver.
//    If the border is cut off or shifted by 2 pixels, the driver is wrong.
// 3. Cycles the 9 robot expressions, 3 s each, with automatic blinking.
//    The current expression name is printed on Serial.
// Eyes are drawn by the FluxGarage RoboEyes library on the SH1106 display.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "pins.h"

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// The display object (-1 = no reset pin)
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RoboEyes must be included AFTER the display is created.
// Note: it defines short macros like ON, OFF, N, S, HAPPY, TIRED.
#include <FluxGarage_RoboEyes.h>
RoboEyes<Adafruit_SH1106G> eyes(display);

// ----- The 9 expressions -----
const char* expressionNames[] = {
  "neutral", "happy", "sad", "scared", "curious",
  "sleepy", "excited", "thinking", "talking"
};
const int NUM_EXPRESSIONS = 9;
const unsigned long EXPRESSION_MS = 3000;   // how long each expression is shown

int currentExpression = 0;
unsigned long expressionStart = 0;
unsigned long lastLaugh = 0;                // for the "excited" giggle

// ---------------------------------------------------------------
// Print every device that answers on the I2C bus
void scanI2C() {
  Serial.println("I2C scan (SDA=21, SCL=22):");
  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device found at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println("  NO devices found - check the 4 OLED wires");
}

// Border + text: every edge of the border must be visible
void showTestPattern() {
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(6, 8);
  display.println("Buddy OLED test");
  display.setCursor(6, 22);
  display.println("SH1106 128x64");
  display.setCursor(6, 36);
  display.println("Border on all");
  display.setCursor(6, 46);
  display.println("4 sides?");
  display.fillCircle(115, 50, 6, SH110X_WHITE);
  display.display();
}

// Put the eyes back to the plain default look
void resetEyes() {
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);     // centred
  eyes.setIdleMode(OFF);
  eyes.setCuriosity(OFF);
  eyes.setHFlicker(OFF);
  eyes.setVFlicker(OFF);
  eyes.setSweat(OFF);
  eyes.setAutoblinker(ON, 3, 2); // blink every 3-5 s
  eyes.open();
}

// Set up the look for one expression
void applyExpression(int e) {
  resetEyes();
  switch (e) {
    case 0: // neutral: plain eyes, blinking
      break;
    case 1: // happy: happy arcs (bottom of the eyes cut away)
      eyes.setMood(HAPPY);
      break;
    case 2: // sad: tired/droopy eyelids, looking down
      eyes.setMood(TIRED);
      eyes.setPosition(S);
      break;
    case 3: // scared: eyes shake left/right and sweat drops
      eyes.setHFlicker(ON, 2);
      eyes.setSweat(ON);
      break;
    case 4: // curious: eyes look around, outer eye grows at the sides
      eyes.setIdleMode(ON, 1, 1);
      eyes.setCuriosity(ON);
      break;
    case 5: // sleepy: droopy and closed, no blinking
      eyes.setAutoblinker(OFF);
      eyes.setMood(TIRED);
      eyes.close();
      break;
    case 6: // excited: happy eyes plus a giggle bounce every second (see loop)
      eyes.setMood(HAPPY);
      eyes.anim_laugh();
      lastLaugh = millis();
      break;
    case 7: // thinking: looking up
      eyes.setPosition(N);
      break;
    case 8: // talking: small up/down bounce
      eyes.setVFlicker(ON, 2);
      break;
  }
  Serial.printf("[%lu ms] Expression %d: %s\n", millis(), e, expressionNames[e]);
}

void setup() {
  Serial.begin(115200);
  delay(300);                     // let the OLED power up (one-off test, delay is OK here)
  Serial.println();
  Serial.println("Buddy - Step 1.1 OLED eyes test");

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  scanI2C();

  if (!display.begin(OLED_I2C_ADDR, true)) {
    Serial.println("display.begin() FAILED");
  } else {
    Serial.println("SH1106 display started");
  }

  showTestPattern();
  Serial.println("Showing test pattern for 4 s...");
  delay(4000);

  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 50);   // 50 frames per second max
  applyExpression(currentExpression);
  expressionStart = millis();
}

void loop() {
  unsigned long now = millis();

  // Move to the next expression every 3 s
  if (now - expressionStart >= EXPRESSION_MS) {
    expressionStart = now;
    currentExpression = (currentExpression + 1) % NUM_EXPRESSIONS;
    applyExpression(currentExpression);
  }

  // "excited" repeats its giggle once per second
  if (currentExpression == 6 && now - lastLaugh >= 1000) {
    lastLaugh = now;
    eyes.anim_laugh();
  }

  eyes.update();   // draws a new frame when it is time (non-blocking)
}
