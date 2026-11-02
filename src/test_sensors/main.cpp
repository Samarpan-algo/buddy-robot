// Step 1.3 - Sensors, buzzer and RGB LED test
// Prints 5 times per second:  front distance (median of the last 5 pings), rear IR, touch.
// Type one letter in the Serial Monitor:
//   h = happy beeps (rising)   g = sad beeps (falling)   t = alert beeps
//   x = active-buzzer check (steady 0.3 s on, no tone)
//   c = cycle RGB mood colours, 1 s each     o = RGB off
//   m = pause / resume the 5 Hz printing     ? = menu
// Step 1.4a: TTP223 touch added (HIGH = touched). A "TOUCHED" line is printed
// the moment a touch starts, even when the 5 Hz printing is paused.
// No delay() in loop(): everything runs on millis() timers.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"

// ---------- Ultrasonic (HC-SR04) ----------
const unsigned long PING_INTERVAL_MS = 50;   // one ping every 50 ms (20 per second)
const unsigned long ECHO_TIMEOUT_US = 25000; // 25 ms = about 4 m; longer = no echo
const int NUM_SAMPLES = 5;                   // median of the last 5 pings
float samples[NUM_SAMPLES];
int sampleCount = 0;                         // how many valid samples are stored (max 5)
int sampleIndex = 0;                         // where the next sample goes (ring buffer)
unsigned long lastPing = 0;

// ---------- Printing ----------
const unsigned long PRINT_INTERVAL_MS = 200; // 5 Hz
unsigned long lastPrint = 0;
bool printing = true;

// ---------- Touch (TTP223) ----------
bool lastTouch = false;                      // touch state in the previous loop

// ---------- Buzzer (LEDC) ----------
// Motors use LEDC channels 0 and 1 (timer 0). Channel 2 sits on timer 1,
// so the buzzer can change frequency without disturbing the motor PWM.
const int BUZZER_CH = 2;
struct Note { int freq; int ms; };           // freq 0 = silence
const Note HAPPY_TUNE[] = {{523,90},{0,30},{659,90},{0,30},{784,90},{0,30},{1047,160}};
const Note SAD_TUNE[]   = {{784,160},{0,40},{659,160},{0,40},{523,300}};
const Note ALERT_TUNE[] = {{1500,80},{0,60},{1500,80},{0,60},{1500,80}};
const Note* tune = nullptr;                  // tune that is playing (nullptr = none)
int tuneLength = 0;
int tuneStep = 0;
unsigned long noteStart = 0;
unsigned long activeTestUntil = 0;           // end time of the 'x' test (0 = not running)

// ---------- RGB LED (WS2812) ----------
Adafruit_NeoPixel rgb(RGB_NUM_PIXELS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
struct Colour { const char* name; uint8_t r, g, b; };
const Colour MOOD_COLOURS[] = {
  {"green  (happy)",   0, 255,   0},
  {"red    (scared)", 255,   0,   0},
  {"blue   (sad)",      0,   0, 255},
  {"yellow (curious)", 255, 180,   0},
  {"purple (sleepy)",  128,   0, 255},
  {"cyan   (thinking)",  0, 255, 255},
};
const int NUM_COLOURS = 6;
int colourIndex = -1;                        // -1 = not cycling
unsigned long colourStart = 0;

// ===============================================================
// Ultrasonic: send one ping, return distance in cm, or -1 if no echo
float pingCm() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_US_TRIG, HIGH);           // 10 us trigger pulse
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  unsigned long us = pulseIn(PIN_US_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return -1;                    // timeout = nothing in range
  return us / 58.0;                          // sound: 58 us per cm (there and back)
}

// Median of the stored valid samples (-1 if there are none)
float medianCm() {
  if (sampleCount == 0) return -1;
  float sorted[NUM_SAMPLES];
  for (int i = 0; i < sampleCount; i++) sorted[i] = samples[i];
  // simple insertion sort (only 5 values)
  for (int i = 1; i < sampleCount; i++) {
    float v = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
    sorted[j + 1] = v;
  }
  return sorted[sampleCount / 2];
}

// ===============================================================
// Buzzer: start a tune (it is played step by step in updateBuzzer)
void playTune(const Note* t, int length, const char* name) {
  tune = t; tuneLength = length; tuneStep = 0; noteStart = millis();
  ledcWriteTone(BUZZER_CH, tune[0].freq);
  Serial.printf("[%lu ms] buzzer: %s\n", millis(), name);
}

void updateBuzzer() {
  unsigned long now = millis();
  if (activeTestUntil != 0 && (long)(now - activeTestUntil) >= 0) {
    digitalWrite(PIN_BUZZER, LOW);           // end of the 'x' test:
    ledcAttachPin(PIN_BUZZER, BUZZER_CH);    // give the pin back to PWM
    ledcWrite(BUZZER_CH, 0);
    activeTestUntil = 0;
  }
  if (tune == nullptr) return;
  if (now - noteStart >= (unsigned long)tune[tuneStep].ms) {
    tuneStep++;
    if (tuneStep >= tuneLength) {            // tune finished
      ledcWriteTone(BUZZER_CH, 0);
      tune = nullptr;
      return;
    }
    noteStart = now;
    ledcWriteTone(BUZZER_CH, tune[tuneStep].freq);
  }
}

// ===============================================================
// RGB LED
void showColour(int i) {
  rgb.setPixelColor(0, rgb.Color(MOOD_COLOURS[i].r, MOOD_COLOURS[i].g, MOOD_COLOURS[i].b));
  rgb.show();
  Serial.printf("[%lu ms] RGB: %s\n", millis(), MOOD_COLOURS[i].name);
}

void rgbOff() {
  rgb.clear();
  rgb.show();
}

void updateRgb() {
  if (colourIndex < 0) return;
  if (millis() - colourStart >= 1000) {
    colourIndex++;
    if (colourIndex >= NUM_COLOURS) {        // all colours shown -> off
      colourIndex = -1;
      rgbOff();
      Serial.printf("[%lu ms] RGB: cycle done, off\n", millis());
      return;
    }
    colourStart = millis();
    showColour(colourIndex);
  }
}

// ===============================================================
void printMenu() {
  Serial.println();
  Serial.println("=== Sensor test menu ===");
  Serial.println(" h = happy beeps   g = sad beeps   t = alert beeps   x = active-buzzer check");
  Serial.println(" c = cycle RGB colours   o = RGB off   m = pause/resume printing   ? = menu");
  Serial.println(" Printing: dist = median of last 5 pings (cm), ir = rear IR, touch = 0/1");
}

void handleCommand(char c) {
  switch (c) {
    case 'h': playTune(HAPPY_TUNE, sizeof(HAPPY_TUNE) / sizeof(Note), "happy (rising)"); break;
    case 'g': playTune(SAD_TUNE,   sizeof(SAD_TUNE)   / sizeof(Note), "sad (falling)");  break;
    case 't': playTune(ALERT_TUNE, sizeof(ALERT_TUNE) / sizeof(Note), "alert");          break;
    case 'x':
      tune = nullptr;
      ledcWriteTone(BUZZER_CH, 0);
      ledcDetachPin(PIN_BUZZER);             // take the pin away from PWM...
      pinMode(PIN_BUZZER, OUTPUT);
      digitalWrite(PIN_BUZZER, HIGH);        // ...and hold it steady HIGH (3.3 V)
      activeTestUntil = millis() + 300;
      Serial.printf("[%lu ms] buzzer: active check (steady HIGH 0.3 s)\n", millis());
      break;
    case 'c':
      colourIndex = 0; colourStart = millis();
      showColour(0);
      break;
    case 'o':
      colourIndex = -1; rgbOff();
      Serial.printf("[%lu ms] RGB: off\n", millis());
      break;
    case 'm':
      printing = !printing;
      Serial.printf("[%lu ms] printing %s\n", millis(), printing ? "ON" : "PAUSED");
      break;
    case '?': printMenu(); break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  pinMode(PIN_IR_REAR, INPUT);               // GPIO34: input only, no internal pull-up
  pinMode(PIN_TOUCH, INPUT);                 // TTP223 drives the pin itself

  ledcSetup(BUZZER_CH, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZER_CH);
  ledcWrite(BUZZER_CH, 0);                   // silent

  rgb.begin();
  rgb.setBrightness(RGB_MAX_BRIGHTNESS);
  rgbOff();

  Serial.println();
  Serial.println("Buddy - Step 1.3/1.4a sensor test (with touch)");
  printMenu();
}

void loop() {
  if (Serial.available()) handleCommand((char)Serial.read());

  unsigned long now = millis();

  // Ping every 50 ms; keep only valid readings (timeouts / 0 are ignored)
  if (now - lastPing >= PING_INTERVAL_MS) {
    lastPing = now;
    float cm = pingCm();
    if (cm > 0) {
      samples[sampleIndex] = cm;
      sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
      if (sampleCount < NUM_SAMPLES) sampleCount++;
    }
  }

  // Touch: report the moment a touch starts
  bool touched = (digitalRead(PIN_TOUCH) == HIGH);
  if (touched && !lastTouch) Serial.printf("[%lu ms] TOUCHED\n", now);
  lastTouch = touched;

  // Print 5 times per second
  if (printing && now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;
    float d = medianCm();
    bool irObstacle = (digitalRead(PIN_IR_REAR) == LOW);   // LOW = obstacle
    if (d < 0) Serial.printf("dist=  ---  cm (no echo)   ir=%-8s touch=%d\n", irObstacle ? "OBSTACLE" : "clear", touched);
    else       Serial.printf("dist=%6.1f cm              ir=%-8s touch=%d\n", d, irObstacle ? "OBSTACLE" : "clear", touched);
  }

  updateBuzzer();
  updateRgb();
}
