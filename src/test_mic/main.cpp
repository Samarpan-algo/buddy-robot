// Mic side test (diagnostic, after code freeze) - INMP441 on I2S, NO filtering.
// The recordings are sent to the laptop and listened to there.
//
// Serial commands:
//   1 = try 1: 16 kHz I2S, 2:1 averaged -> 8 kHz, >>14, no APLL (current settings)
//   2 = try 2: same but use_apll = true (cleaner I2S clock)
//   3 = try 3: same as 1 but >>11 (8x more gain - watch for clipping)
//   4 = try 4: 16 kHz, no averaging (samples stay at 16 kHz)
//   r = record 3 s with the selected try      d = dump the raw samples (hex)
// Pins: SCK GPIO14, WS GPIO18, SD GPIO35, L/R to GND (signal found in slot 0).

#include <Arduino.h>
#include <driver/i2s.h>
#include "pins.h"

const i2s_port_t I2S_PORT = I2S_NUM_0;
const int SECONDS = 3;
const int MAX_SAMPLES = 16000 * SECONDS;      // biggest case: 16 kHz for 3 s

static int16_t* rec = nullptr;
static int recCount = 0;

// settings of the selected try
static int tryNo = 1;
static bool useApll = false;
static int shift = 14;
static bool average = true;                   // average 2 frames -> 8 kHz
static int outRate = 8000;
static int clipped = 0;

static void startI2S() {
  i2s_driver_uninstall(I2S_PORT);             // fine if not installed yet
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 16000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = useApll;
  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_MIC_SCK;
  pins.ws_io_num = PIN_MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_MIC_SD;
  bool ok = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) == ESP_OK && i2s_set_pin(I2S_PORT, &pins) == ESP_OK;
  Serial.printf("[mic] try %d: apll=%d shift=%d average=%d -> %d Hz  (%s)\n",
                tryNo, useApll, shift, average, outRate, ok ? "OK" : "FAILED");
}

static void selectTry(int n) {
  tryNo = n;
  useApll = (n == 2);
  shift = (n == 3) ? 11 : 14;
  average = (n != 4);
  outRate = average ? 8000 : 16000;
  startI2S();
}

static int16_t clip16(int32_t v) {
  if (v > 32767) { clipped++; return 32767; }
  if (v < -32768) { clipped++; return -32768; }
  return (int16_t)v;
}

static void record() {
  int want = outRate * SECONDS;
  static int32_t raw[512];                    // 256 stereo frames
  i2s_zero_dma_buffer(I2S_PORT);
  recCount = 0; clipped = 0;
  Serial.printf("[mic] RECORDING %d s - talk now!\n", SECONDS);
  while (recCount < want) {
    size_t bytes = 0;
    i2s_read(I2S_PORT, raw, sizeof(raw), &bytes, portMAX_DELAY);
    int frames = bytes / (2 * sizeof(int32_t));
    for (int f = 0; f < frames && recCount < want; f += (average ? 2 : 1)) {
      int32_t a = raw[2 * f] >> shift;        // slot 0 = the mic
      if (average && f + 1 < frames) a = (a + (raw[2 * (f + 1)] >> shift)) / 2;
      rec[recCount++] = clip16(a);
    }
  }
  int peak = 0; for (int i = 0; i < recCount; i++) peak = max(peak, abs((int)rec[i]));
  Serial.printf("[mic] done: %d samples at %d Hz, peak %d, clipped %d\n", recCount, outRate, peak, clipped);
}

static void dump() {
  Serial.printf("DUMP %d %d %d\n", recCount, outRate, tryNo);
  for (int i = 0; i < recCount; i++) {
    Serial.printf("%04x", (uint16_t)rec[i]);
    if (i % 32 == 31) Serial.println();
  }
  Serial.println();
  Serial.println("END");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Buddy - mic side test (raw recordings for the laptop)");
  rec = (int16_t*)malloc(MAX_SAMPLES * sizeof(int16_t));   // 96 KB
  if (!rec) Serial.println("malloc FAILED");
  selectTry(1);
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c >= '1' && c <= '4') selectTry(c - '0');
  else if (c == 'r') record();
  else if (c == 'd') dump();
}
