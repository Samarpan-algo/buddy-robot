// Step 0 - Toolchain test
// Blinks the onboard blue LED (GPIO2) once per second and prints
// information about the ESP32 chip every 2 seconds.
// Uses millis() timers instead of delay(), the same style the real robot uses.

#include <Arduino.h>
#include "pins.h"

unsigned long lastBlink = 0;   // last time the LED changed (ms)
unsigned long lastPrint = 0;   // last time we printed chip info (ms)
bool ledOn = false;

void printChipInfo() {
  Serial.println("----- ESP32 info -----");
  Serial.printf("Chip model   : %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU          : %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash size   : %lu bytes\n", (unsigned long)ESP.getFlashChipSize());
  Serial.printf("PSRAM size   : %lu bytes\n", (unsigned long)ESP.getPsramSize());
  Serial.printf("Free heap    : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
#ifdef ESP_ARDUINO_VERSION_MAJOR
  Serial.printf("Arduino core : %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
#else
  Serial.println("Arduino core : unknown (older than 2.0)");
#endif
  Serial.printf("ESP-IDF      : %s\n", ESP.getSdkVersion());
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ONBOARD_LED, OUTPUT);
  Serial.println();
  Serial.println("Buddy - Step 0 toolchain test starting");
  printChipInfo();
}

void loop() {
  unsigned long now = millis();

  // Toggle the LED every 500 ms -> one full blink per second
  if (now - lastBlink >= 500) {
    lastBlink = now;
    ledOn = !ledOn;
    digitalWrite(PIN_ONBOARD_LED, ledOn ? HIGH : LOW);
  }

  // Print chip info every 2 seconds
  if (now - lastPrint >= 2000) {
    lastPrint = now;
    printChipInfo();
  }
}
