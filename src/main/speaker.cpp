// speaker.cpp - plays 8 kHz 8-bit unsigned audio (128 = silence) on GPIO5.
//
// How: LEDC channel 4 (timer 2; motors use timer 0) makes a 312.5 kHz PWM on
// GPIO5. A hardware timer interrupt runs 8000 times per second and sets the
// PWM duty to the next sample. The RC filter turns that into an audio voltage.
// One buffer is allocated once at boot and reused for every clip.

#include <Arduino.h>
#include "soc/ledc_struct.h"
#include "pins.h"
#include "speaker.h"
#include <Preferences.h>

const int AUDIO_CH = 4;                    // LEDC channel 4 -> timer 2
const uint32_t PWM_FREQ = 312500;          // fastest 8-bit PWM (80 MHz / 256)
const int SAMPLE_RATE = 8000;

static uint8_t* buf = nullptr;
static size_t capacity = 0;
static volatile size_t length = 0;         // bytes of audio in the buffer
static volatile size_t pos = 0;            // next sample to play
static volatile bool playing = false;
static volatile bool speech = false;       // true = the buffer holds speech from the phone
static hw_timer_t* timer = nullptr;

// Set the PWM duty directly in the LEDC registers (safe inside an interrupt)
static inline void IRAM_ATTR setDuty(uint8_t v) {
  LEDC.channel_group[0].channel[AUDIO_CH].duty.duty = (uint32_t)v << 4;
  LEDC.channel_group[0].channel[AUDIO_CH].conf1.duty_start = 1;
}

// 8000 times per second: next sample, or silence
void IRAM_ATTR onAudioTimer() {
  uint8_t s = 128;
  if (playing) {
    if (pos < length) s = buf[pos++];
    else playing = false;
  }
  setDuty(s);
}

// Sound effects on/off (Settings button). Remembered in NVS. Speech is never muted.
static bool effectsOn = true;
static Preferences sfxPrefs;
void speakerSetEffects(bool on) {
  effectsOn = on;
  sfxPrefs.putBool("sfx", on);
  if (!on && playing && !speech) playing = false;   // stop a chirp that is playing now
}
bool speakerEffectsOn() { return effectsOn; }

void speakerInit() {
  sfxPrefs.begin("settings", false);
  effectsOn = sfxPrefs.getBool("sfx", true);
  // Biggest buffer that still leaves enough memory for WiFi and the web server
  const size_t sizes[] = {64000, 48000, 32000};
  for (size_t sz : sizes) {
    buf = (uint8_t*)malloc(sz);
    if (buf) { capacity = sz; break; }
  }
  Serial.printf("[speaker] buffer %u bytes (%.1f s), free heap now %u\n",
                (unsigned)capacity, capacity / (float)SAMPLE_RATE, (unsigned)ESP.getFreeHeap());

  ledcSetup(AUDIO_CH, PWM_FREQ, 8);
  ledcAttachPin(PIN_AUDIO_OUT, AUDIO_CH);
  ledcWrite(AUDIO_CH, 128);                       // 50% duty = silence

  timer = timerBegin(1, 80, true);                // hardware timer 1, 1 MHz ticks
  timerAttachInterrupt(timer, &onAudioTimer, true);
  timerAlarmWrite(timer, 1000000 / SAMPLE_RATE, true);
  timerAlarmEnable(timer);
}

size_t speakerCapacity() { return capacity; }

void speakerBeginUpload() {
  playing = false;                                // the buffer is shared: stop first
  length = 0;
  pos = 0;
}

void speakerAppend(const uint8_t* data, size_t len) {
  if (!buf) return;
  size_t room = capacity - length;
  if (len > room) len = room;                     // too long: keep the beginning only
  memcpy(buf + length, data, len);
  length += len;
}


void speakerEndUpload() {
  if (!buf || length == 0) return;
  pos = 0;
  speech = true;
  playing = true;
  Serial.printf("[speaker] playing %u samples (%.1f s)\n", (unsigned)length, length / (float)SAMPLE_RATE);
}

size_t speakerReceived() { return length; }
bool speakerPlaying() { return playing; }
bool speakerSpeaking() { return playing && speech; }
void speakerStop() { playing = false; }

// ---------- sound effects (synthesised into the same buffer) ----------
// One "note" = a tone gliding from f0 to f1 Hz over ms milliseconds (f0 = 0: silence).
// wobble = vibrato depth in Hz (for "scared").
struct Glide { uint16_t f0, f1, ms; uint16_t wobble; };

static const Glide FX_HAPPY_N[]    = {{600, 1100, 120, 0}, {0, 0, 40, 0}, {700, 1300, 140, 0}};
static const Glide FX_SAD_N[]      = {{700, 450, 350, 0}, {0, 0, 60, 0}, {500, 300, 450, 0}};
static const Glide FX_SCARED_N[]   = {{1500, 1500, 450, 300}};
static const Glide FX_CURIOUS_N[]  = {{450, 450, 120, 0}, {450, 750, 220, 0}};               // "hmm?"
static const Glide FX_GREETING_N[] = {{523, 523, 130, 0}, {659, 659, 130, 0}, {784, 784, 130, 0}, {1047, 1047, 260, 0}};
static const Glide FX_TUNE_N[]     = {{523, 523, 150, 0}, {659, 659, 150, 0}, {784, 784, 150, 0}, {1047, 1047, 200, 0},
                                      {0, 0, 60, 0}, {784, 784, 150, 0}, {1047, 1047, 350, 0}};   // little song

bool speakerEffect(SpeakerEffect fx) {
  if (!buf || speakerSpeaking() || !effectsOn) return false;   // speech has priority; muted = no effects
  const Glide* notes; int n;
  switch (fx) {
    case FX_HAPPY:    notes = FX_HAPPY_N;    n = 3; break;
    case FX_SAD:      notes = FX_SAD_N;      n = 3; break;
    case FX_SCARED:   notes = FX_SCARED_N;   n = 1; break;
    case FX_CURIOUS:  notes = FX_CURIOUS_N;  n = 2; break;
    case FX_GREETING: notes = FX_GREETING_N; n = 4; break;
    default:          notes = FX_TUNE_N;     n = 7; break;
  }
  playing = false;                                 // build the sound, then start it
  size_t len = 0;
  float phase = 0;
  for (int i = 0; i < n; i++) {
    int count = notes[i].ms * SAMPLE_RATE / 1000;
    for (int k = 0; k < count && len < capacity; k++) {
      uint8_t s = 128;
      if (notes[i].f0) {
        float t = (float)k / count;                 // 0..1 inside this note
        float f = notes[i].f0 + (notes[i].f1 - notes[i].f0) * t;
        if (notes[i].wobble) f += notes[i].wobble * sinf(2 * PI * 15 * k / SAMPLE_RATE);
        phase += 2 * PI * f / SAMPLE_RATE;
        float env = min(1.0f, min(k, count - k) / 80.0f);   // soft start/end, no clicks
        s = 128 + (int)(100 * env * sinf(phase));
      }
      buf[len++] = s;
    }
  }
  length = len;
  pos = 0;
  speech = false;
  playing = true;
  return true;
}
