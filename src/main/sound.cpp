// sound.cpp - buzzer rhythms + RGB LED colour.
// The buzzer is an ACTIVE buzzer, so it is simply switched on and off with
// digitalWrite (no PWM / LEDC needed). Rhythms differ only in timing
// (design decision in step 1.3).

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"
#include "sound.h"

// ---------- rhythms: on, off, on, off ... times in ms (0 ends the list) ----------
static const int HAPPY_RH[]    = {70, 70, 70, 0};                 // quick double
static const int SAD_RH[]      = {400, 250, 400, 0};              // two slow long
static const int SCARED_RH[]   = {50, 50, 50, 50, 50, 0};         // fast triple
static const int GREETING_RH[] = {70, 70, 70, 70, 300, 0};        // short-short-long
static const int ALARM_RH[]    = {250, 120, 250, 120, 250, 120, 700, 0};   // end of focus time

static const int* rhythm = nullptr;   // playing rhythm (nullptr = silent)
static int step = 0;
static unsigned long stepStart = 0;

// ---------- RGB ----------
static Adafruit_NeoPixel rgb(RGB_NUM_PIXELS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
static bool rainbow = false;          // "excited" = rainbow cycle
static bool pulse = false;            // "thinking" = white that slowly breathes
static unsigned long lastRainbow = 0;
static uint16_t hue = 0;

void soundInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  rgb.begin();
  rgb.setBrightness(RGB_MAX_BRIGHTNESS);
  rgb.clear();
  rgb.show();
}

void soundPlay(Rhythm r) {
  switch (r) {
    case RH_HAPPY:    rhythm = HAPPY_RH;    break;
    case RH_SAD:      rhythm = SAD_RH;      break;
    case RH_SCARED:   rhythm = SCARED_RH;   break;
    case RH_GREETING: rhythm = GREETING_RH; break;
    case RH_ALARM:    rhythm = ALARM_RH;    break;
  }
  step = 0;
  stepStart = millis();
  digitalWrite(PIN_BUZZER, HIGH);     // every rhythm starts with a beep
}

void rgbSetColour(uint8_t r, uint8_t g, uint8_t b) {
  rainbow = false;
  pulse = false;
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

static void setColour(uint8_t r, uint8_t g, uint8_t b) {
  rgb.setPixelColor(0, rgb.Color(r, g, b));
  rgb.show();
}

// Mood colours: happy green, scared red, sad blue, curious yellow, sleepy dim purple
void rgbSetEmotion(Emotion e) {
  if (e == EMO_TALKING) return;       // talking: keep the colour of the reply's emotion
  rainbow = false;
  pulse = false;
  switch (e) {
    case EMO_HAPPY:    setColour(0, 255, 0);    break;   // green
    case EMO_SCARED:   setColour(255, 0, 0);    break;   // red
    case EMO_SAD:      setColour(0, 0, 255);    break;   // blue
    case EMO_CURIOUS:  setColour(255, 180, 0);  break;   // yellow
    case EMO_THINKING: pulse = true;            break;   // breathing white (cyan looked like sad-blue)
    case EMO_EXCITED:  rainbow = true;          break;   // rainbow cycle
    case EMO_SLEEPY:                                       // LEDs off
    case EMO_NEUTRAL:
    default:           setColour(0, 0, 0);      break;
  }
}

void soundUpdate() {
  unsigned long now = millis();

  // Buzzer: go to the next on/off step when this one is over
  if (rhythm != nullptr && now - stepStart >= (unsigned long)rhythm[step]) {
    step++;
    stepStart = now;
    if (rhythm[step] == 0) {          // end of the list
      rhythm = nullptr;
      digitalWrite(PIN_BUZZER, LOW);
    } else {
      digitalWrite(PIN_BUZZER, (step % 2 == 0) ? HIGH : LOW);  // even steps = on
    }
  }

  // Breathing white for "thinking": brightness goes up and down every 1.6 s
  if (pulse && now - lastRainbow >= 30) {
    lastRainbow = now;
    int phase = (now / 4) % 400;                  // 0..399
    int level = phase < 200 ? phase : 400 - phase; // 0..200..0
    uint8_t v = 30 + level;                        // never fully dark
    setColour(v, v, v);
  }

  // Rainbow for "excited"
  if (rainbow && now - lastRainbow >= 20) {
    lastRainbow = now;
    hue += 600;
    rgb.setPixelColor(0, rgb.gamma32(rgb.ColorHSV(hue)));
    rgb.show();
  }
}
