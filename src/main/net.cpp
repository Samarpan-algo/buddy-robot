// net.cpp - joins the phone hotspot and keeps the connection alive.
// The hotspot name and password come from include/secrets.h (never in git,
// never printed). When the connection comes up, the IP address is printed
// on Serial and shown on the OLED for 5 s, so the user can type it on the phone.

#include <WiFi.h>
#include "secrets.h"
#include "net.h"
#include "eyes.h"

const unsigned long CHECK_MS = 1000;        // look at the WiFi state once per second
const unsigned long RETRY_MS = 15000;       // not connected this long -> start again
const unsigned long SHOW_IP_MS = 5000;

static bool wasConnected = false;
static bool configured = false;             // false if secrets.h still has the example text
static unsigned long lastCheck = 0, lastBegin = 0;

void netInit() {
  configured = strcmp(WIFI_SSID, "your-hotspot-name") != 0;
  if (!configured) {
    Serial.println("[wifi] secrets.h still has the example values -> WiFi OFF");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                     // faster replies to the phone
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastBegin = millis();
  Serial.println("[wifi] connecting to the hotspot...");
}

void netUpdate() {
  if (!configured) return;
  unsigned long now = millis();
  if (now - lastCheck < CHECK_MS) return;
  lastCheck = now;

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected && !wasConnected) {
    String ip = WiFi.localIP().toString();
    Serial.printf("[wifi] connected. Open on the phone:  http://%s/\n", ip.c_str());
    String url = "http://" + ip;
    eyesShowText("WiFi connected!", "Open on phone:", url.c_str(), SHOW_IP_MS);
  }
  if (!connected && wasConnected) {
    Serial.println("[wifi] connection lost, reconnecting...");
    lastBegin = now;
  }
  if (!connected && now - lastBegin >= RETRY_MS) {   // still nothing: start again
    Serial.println("[wifi] retrying...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    lastBegin = now;
  }
  wasConnected = connected;
}

bool netConnected() { return configured && WiFi.status() == WL_CONNECTED; }

String netIp() { return netConnected() ? WiFi.localIP().toString() : String(""); }
