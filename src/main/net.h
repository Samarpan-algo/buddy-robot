// net.h - WiFi connection to the phone hotspot (details in include/secrets.h).
#pragma once
#include <Arduino.h>

void   netInit();          // start connecting (does not wait)
void   netUpdate();        // call every loop: reconnects, shows the IP when connected
bool   netConnected();
String netIp();            // "192.168.x.y" or "" when not connected
