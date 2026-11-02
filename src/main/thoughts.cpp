// thoughts.cpp - a small ring buffer of Buddy's last decisions, in words.

#include <stdarg.h>
#include "thoughts.h"

const int MAX_THOUGHTS = 15;
const int THOUGHT_LEN = 72;

struct Thought { unsigned long ms; char text[THOUGHT_LEN]; };
static Thought ring[MAX_THOUGHTS];
static int head = 0, count = 0;

void think(const char* fmt, ...) {
  Thought& t = ring[head];
  va_list args;
  va_start(args, fmt);
  vsnprintf(t.text, THOUGHT_LEN, fmt, args);
  va_end(args);
  for (char* p = t.text; *p; p++) if (*p == '"' || *p == '\\') *p = '\'';   // keep the JSON valid
  t.ms = millis();
  head = (head + 1) % MAX_THOUGHTS;
  if (count < MAX_THOUGHTS) count++;
  Serial.printf("[think] %s\n", t.text);
}

String thoughtsJson() {
  String out = "[";
  unsigned long now = millis();
  for (int i = 0; i < count; i++) {
    const Thought& t = ring[(head - count + i + MAX_THOUGHTS) % MAX_THOUGHTS];
    if (i) out += ",";
    out += "{\"t\":\"";
    out += t.text;
    out += "\",\"ago_s\":";
    out += (now - t.ms) / 1000;
    out += "}";
  }
  return out + "]";
}
