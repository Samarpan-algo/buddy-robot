// Step H1 - Speaker test (PAM8403 + 8 ohm speaker)
//
// How sound comes out of a digital pin:
//   GPIO5 makes a very fast PWM signal (312.5 kHz). Its duty cycle (0-255) is
//   changed 8000 times per second by a hardware timer -> that is 8 kHz audio.
//   The 2k resistor + 10 nF capacitor (RC filter, about 8 kHz) smooth the PWM
//   into a real audio voltage, the PAM8403 amplifies it, the speaker plays it.
//
// Type one letter in the Serial Monitor:
//   s = tone sweep 200 -> 3000 Hz (3 s)     m = little melody
//   b = "beep-boop" robot sample from a RAM buffer (same path F4a speech will use)
//   + / - = louder / quieter (digital volume)    x = stop    ? = menu
//
// LEDC: motors use channels 0/1 (timer 0), buzzer is digital, so audio uses
// channel 4 = LEDC timer 2 (motors use timer 0, buzzer timer 1).

#include <Arduino.h>
#include "soc/ledc_struct.h"
#include "pins.h"

const int AUDIO_CH = 4;                    // LEDC channel 4 -> timer 2
const uint32_t PWM_FREQ = 312500;          // 80 MHz / 256 = fastest 8-bit PWM
const int SAMPLE_RATE = 8000;

// ---------- shared between the timer interrupt and loop() ----------
enum Mode { SILENT, TONE, BUFFER };
static volatile Mode mode = SILENT;
static volatile uint32_t phase = 0, phaseInc = 0;    // tone generator
static volatile uint8_t volume = 128;                // 0-255 (start at half)
static uint8_t* sampleBuf = nullptr;                 // buffer playback
static volatile uint32_t bufLen = 0, bufPos = 0;
DRAM_ATTR static uint8_t sineTable[256];             // in RAM so the interrupt never waits for flash

static hw_timer_t* timer = nullptr;

// Write the PWM duty straight into the LEDC registers (safe inside an interrupt)
static inline void IRAM_ATTR setDuty(uint8_t v) {
  LEDC.channel_group[0].channel[AUDIO_CH].duty.duty = (uint32_t)v << 4;
  LEDC.channel_group[0].channel[AUDIO_CH].conf1.duty_start = 1;
}

// Scale a sample (128 = silence) by the volume
static inline uint8_t IRAM_ATTR applyVolume(uint8_t s) {
  int v = 128 + (((int)s - 128) * volume) / 255;
  return (uint8_t)v;
}

// Runs 8000 times per second
void IRAM_ATTR onTimer() {
  uint8_t s = 128;
  if (mode == TONE) {
    phase += phaseInc;
    s = sineTable[phase >> 24];
  } else if (mode == BUFFER) {
    if (bufPos < bufLen) s = sampleBuf[bufPos++];
    else mode = SILENT;
  }
  setDuty(applyVolume(s));
}

static void setToneHz(float hz) {
  phaseInc = (uint32_t)(hz * 4294967296.0 / SAMPLE_RATE);   // 2^32 * f / fs
}

// ---------- what loop() is doing ----------
enum Job { NONE, SWEEP, MELODY };
static Job job = NONE;
static unsigned long jobStart = 0;
static int noteIndex = -1;

const int MELODY_HZ[] = {523, 659, 784, 1047, 784, 1047};   // C E G C' G C'
const int MELODY_MS[] = {180, 180, 180, 350, 150, 500};
const int MELODY_LEN = 6;

// Build a 1-second "beep-boop" robot sound into the RAM buffer
static void makeBeepBoop() {
  const uint32_t n = SAMPLE_RATE;          // 1 s
  uint32_t ph = 0;
  for (uint32_t i = 0; i < n; i++) {
    float t = (float)i / SAMPLE_RATE;
    float hz;
    if (t < 0.25)      hz = 900;                         // beep
    else if (t < 0.35) hz = 0;                           // pause
    else if (t < 0.75) hz = 700 - (t - 0.35) * 800;      // falling "boop"
    else               hz = 0;
    if (hz > 0) {
      ph += (uint32_t)(hz * 4294967296.0 / SAMPLE_RATE);
      sampleBuf[i] = (ph & 0x80000000) ? 200 : 56;      // square wave = robot-ish
    } else {
      sampleBuf[i] = 128;
    }
  }
  bufLen = n;
}

static void stopAll() {
  job = NONE;
  mode = SILENT;
}

static void printMenu() {
  Serial.println();
  Serial.println("=== Speaker test (H1) ===");
  Serial.println(" s = tone sweep   m = melody   b = beep-boop sample   + / - volume   x = stop   ? = menu");
  Serial.printf (" volume %d / 255\n", volume);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Buddy - H1 speaker test. Booted OK (GPIO5 + filter did not upset the boot).");

  for (int i = 0; i < 256; i++) sineTable[i] = 128 + (int)(100 * sin(i * 2 * PI / 256));

  sampleBuf = (uint8_t*)malloc(SAMPLE_RATE);           // 8 KB, allocated once
  if (!sampleBuf) Serial.println("malloc FAILED");

  ledcSetup(AUDIO_CH, PWM_FREQ, 8);
  ledcAttachPin(PIN_AUDIO_OUT, AUDIO_CH);
  ledcWrite(AUDIO_CH, 128);                            // 50% duty = silence

  timer = timerBegin(1, 80, true);                     // hardware timer 1, 1 MHz ticks
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000 / SAMPLE_RATE, true); // every 125 us = 8 kHz
  timerAlarmEnable(timer);

  printMenu();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 's': stopAll(); job = SWEEP; jobStart = millis(); mode = TONE;
                Serial.println("sweep 200 -> 3000 Hz"); break;
      case 'm': stopAll(); job = MELODY; jobStart = millis(); noteIndex = -1; mode = TONE;
                Serial.println("melody"); break;
      case 'b': stopAll(); makeBeepBoop(); bufPos = 0; mode = BUFFER;
                Serial.println("beep-boop sample (1 s from RAM buffer)"); break;
      case '+': volume = min(255, volume + 32); Serial.printf("volume %d\n", volume); break;
      case '-': volume = max(0, volume - 32);   Serial.printf("volume %d\n", volume); break;
      case 'x': stopAll(); Serial.println("stop"); break;
      case '?': printMenu(); break;
      default: break;
    }
  }

  unsigned long t = millis() - jobStart;
  if (job == SWEEP) {
    if (t >= 3000) { stopAll(); Serial.println("sweep done"); }
    else setToneHz(200 + 2800.0 * t / 3000);
  } else if (job == MELODY) {
    unsigned long sum = 0; int idx = -1;            // which note are we in?
    for (int i = 0; i < MELODY_LEN; i++) { sum += MELODY_MS[i]; if (t < sum) { idx = i; break; } }
    if (idx < 0) { stopAll(); Serial.println("melody done"); }
    else if (idx != noteIndex) { noteIndex = idx; setToneHz(MELODY_HZ[idx]); }
  }
}
