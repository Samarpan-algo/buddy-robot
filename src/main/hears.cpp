// hears.cpp - "Buddy hears" (design decision after the H2 mic test): the on-board mic is
// not clear enough for speech recognition, so it is used as a sound sensor.
//
// Every 100 ms two loudness levels are measured:
//   sharp level = after a ~200 Hz high-pass filter -> claps and bangs stand out
//   raw level   = overall loudness (DC removed) -> rises when someone talks nearby
// Events:
//   HE_LOUD  : sharp level jumps above HEAR_LOUD_LEVEL         -> startled
//   HE_VOICE : raw level above HEAR_VOICE_LEVEL for HEAR_VOICE_MS
//              while someone is in front (< 60 cm)              -> curious, listening
// Each event has a 3 s cooldown. Nothing is heard while Buddy speaks or moves.

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"
#include "config.h"
#include "hears.h"

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int WINDOW = 800;                       // 800 samples at 8 kHz = 100 ms
const unsigned long COOLDOWN_MS = 3000;
const unsigned long IGNORE_TAIL_MS = 500;     // keep ignoring a little after speaking/moving

static int sharpLevel = 0, rawLevel = 0;
static int sharpPeak = 0, rawPeak = 0;        // loudest window since the last status print
static float hpPrevX = 0, hpPrevY = 0;        // high-pass filter memory
static float dc = 0;                          // slow average = DC offset
static double sumSharp = 0, sumRaw = 0;
static int count = 0;
static unsigned long voiceSince = 0, lastLoud = 0, lastVoice = 0, ignoreUntil = 0;
static int micSlot = 0;                       // which stereo slot carries the mic (found in H2: 0)

void hearsInit() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;                    // averaged to 8 kHz below (as in test_mic)
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_MIC_SCK;
  pins.ws_io_num = PIN_MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_MIC_SD;
  bool ok = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) == ESP_OK && i2s_set_pin(I2S_PORT, &pins) == ESP_OK;
  Serial.printf("[hears] mic %s\n", ok ? "ready" : "FAILED");
}

int hearsUpdate(bool ignore, bool someoneInFront) {
  int events = HE_NONE;
  unsigned long now = millis();
  if (ignore) ignoreUntil = now + IGNORE_TAIL_MS;

  // Read whatever the mic has ready (never waits)
  static int32_t raw[512];
  size_t bytes = 0;
  i2s_read(I2S_PORT, raw, sizeof(raw), &bytes, 0);
  int frames = bytes / (4 * sizeof(int32_t));          // 2 stereo frames per 8 kHz sample
  for (int i = 0; i < frames; i++) {
    float x = ((raw[4 * i + micSlot] >> 14) + (raw[4 * i + 2 + micSlot] >> 14)) / 2.0f;
    dc += (x - dc) * 0.001f;                           // very slow average = DC offset
    float y = 0.864f * (hpPrevY + x - hpPrevX);        // first-order high-pass ~200 Hz
    hpPrevX = x; hpPrevY = y;
    sumSharp += (double)y * y;
    sumRaw += (double)(x - dc) * (x - dc);
    if (++count >= WINDOW) {                           // a 100 ms window is complete
      sharpLevel = (int)sqrt(sumSharp / WINDOW);
      rawLevel = (int)sqrt(sumRaw / WINDOW);
      sharpPeak = max(sharpPeak, sharpLevel);
      rawPeak = max(rawPeak, rawLevel);
      sumSharp = sumRaw = 0; count = 0;

      if ((long)(now - ignoreUntil) < 0) { voiceSince = 0; continue; }   // hearing itself: skip

      if (sharpLevel > HEAR_LOUD_LEVEL && now - lastLoud > COOLDOWN_MS) {
        lastLoud = now;
        events |= HE_LOUD;
        Serial.printf("[hears] LOUD sound (sharp %d)\n", sharpLevel);
      }
      bool voice = someoneInFront && rawLevel > HEAR_VOICE_LEVEL;
      if (!voice) voiceSince = 0;
      else if (voiceSince == 0) voiceSince = now;
      if (voiceSince != 0 && now - voiceSince >= HEAR_VOICE_MS && now - lastVoice > COOLDOWN_MS) {
        lastVoice = now;
        voiceSince = 0;
        events |= HE_VOICE;
        Serial.printf("[hears] someone is talking (raw %d)\n", rawLevel);
      }
    }
  }
  return events;
}

// For the status line: the loudest 100 ms window since the last call (then reset)
int hearsSharpLevel() { int p = sharpPeak; sharpPeak = 0; return p; }
int hearsRawLevel()   { int p = rawPeak; rawPeak = 0; return p; }
