// web.cpp - HTTP API for the phone (endpoints listed in README):
//   GET /                      phone page (embedded in the firmware, web_page.h)
//   GET /state                 JSON: emotion, mood values, bond, dist_cm, touched, ir, uptime ...
//   GET /emotion?e=..&ms=..    show an emotion on the face (beep=0 -> silent)
//   GET /drive?d=f|b|l|r|s&speed=0-100   repeat every 200 ms; stops after 600 ms silence
//   GET /event?type=praise|chat_start|chat_end|come
//   GET /beep?p=happy|sad|alert
//   GET /focus?mode=work|break|stop&min=N&brk=N   Pomodoro (F2)
//   /event also: quiz_correct (&streak=N), quiz_wrong
//   POST /play   body = 8 kHz 8-bit audio -> Buddy speaks on its own speaker (F4a)
//   GET /stopvoice  stop speaking
//   GET /mind    Mind tab data: mood, emotion + reason, 20 events, thoughts (G0)
// The AI itself runs on the phone (Gemini API); the API key never reaches the robot.

#include <Arduino.h>
#include <WebServer.h>
#include "web.h"
#include "web_page.h"
#include "robot.h"
#include "mood.h"
#include "sensors.h"
#include "drive.h"
#include "sound.h"
#include "focus.h"
#include "speaker.h"
#include "thoughts.h"
#include "learn.h"
#include "usermood.h"

static WebServer server(80);

// ---------- POST /play  (body = 8 kHz 8-bit unsigned mono samples, F4a) ----------
// The body arrives in small pieces (about 1.4 KB) and is copied straight into the
// speaker buffer - no big String in memory.
static void handlePlayBody() {
  HTTPRaw& raw = server.raw();
  if (raw.status == RAW_START)      speakerBeginUpload();
  else if (raw.status == RAW_WRITE) speakerAppend(raw.buf, raw.currentSize);
}
static void handlePlayDone() {
  speakerEndUpload();
  server.send(200, "application/json",
              String("{\"ok\":true,\"samples\":") + speakerReceived() +
              ",\"capacity\":" + speakerCapacity() + "}");
}

static void sendJson(const String& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

static void sendError(int code, const char* why) {
  server.send(code, "application/json", String("{\"ok\":false,\"why\":\"") + why + "\"}");
}

// ---------- GET / ----------
static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// ---------- GET /state ----------
static void handleState() {
  const MoodState& s = moodState();
  char events[400];
  moodRecentEventsJson(events, sizeof(events), 5);     // the last 5 things that happened
  char buf[1400];
  snprintf(buf, sizeof(buf),
    "{\"emotion\":\"%s\",\"mood_emotion\":\"%s\",\"happy\":%.0f,\"fear\":%.0f,\"curious\":%.0f,"
    "\"energy\":%.0f,\"bond\":%d,\"dist_cm\":%.1f,\"touched\":%d,\"ir\":%d,"
    "\"uptime\":%lu,\"idle_s\":%lu,\"pets\":%lu,\"chats\":%lu,"
    "\"focus\":\"%s\",\"focus_left_s\":%lu,\"speaking\":%d,\"audio_capacity\":%u,\"events\":%s,\"user\":%s}",
    emotionName(robotFaceEmotion()), emotionName(moodEmotion()),
    s.happy, s.fear, s.curious, s.energy, s.bond,
    sensorsDistance(), sensorsTouched() ? 1 : 0, sensorsRearObstacle() ? 1 : 0,
    millis() / 1000, moodIdleSeconds(), s.totalPets, s.totalChats,
    focusModeName(), focusSecondsLeft(), speakerPlaying() ? 1 : 0, (unsigned)speakerCapacity(), events,
    usermoodJson().c_str());
  sendJson(buf);
}

// ---------- GET /mind  (Mind tab: feelings, why, memory, thoughts - works offline) ----------
static void handleMind() {
  const MoodState& s = moodState();
  char events[1200];
  moodRecentEventsJson(events, sizeof(events), 20);     // the whole event memory
  char head[400];
  snprintf(head, sizeof(head),
    "{\"emotion\":\"%s\",\"reason\":\"%s\",\"happy\":%.0f,\"fear\":%.0f,\"curious\":%.0f,"
    "\"energy\":%.0f,\"bond\":%d,\"dist_cm\":%.1f,\"idle_s\":%lu,\"focus\":\"%s\",\"pets\":%lu,\"chats\":%lu,",
    emotionName(robotFaceEmotion()), robotEmotionReason(), s.happy, s.fear, s.curious, s.energy,
    s.bond, sensorsDistance(), moodIdleSeconds(), focusModeName(), s.totalPets, s.totalChats);
  String out = head;
  out += "\"events\":";   out += events;
  out += ",\"thoughts\":"; out += thoughtsJson();
  out += ",\"learn\":";    out += learnJson(sensorsDistance());
  out += ",\"user\":";     out += usermoodJson();
  out += ",\"say\":{\"id\":"; out += robotSayId(); out += ",\"text\":\""; out += robotSayText(); out += "\"}";
  out += ",\"focusinfo\":"; out += focusJson();
  out += "}";
  sendJson(out);
}

// ---------- GET /emotion?e=happy&ms=3000&beep=1 ----------
static void handleEmotion() {
  Emotion e = emotionFromName(server.arg("e").c_str());
  if (e == EMO_COUNT) { sendError(400, "unknown emotion"); return; }
  long ms = server.hasArg("ms") ? server.arg("ms").toInt() : 3000;
  ms = constrain(ms, 300L, 60000L);
  bool beep = !(server.hasArg("beep") && server.arg("beep") == "0");
  robotShowEmotion(e, (unsigned long)ms, beep);
  sendJson("{\"ok\":true}");
}

// ---------- GET /drive?d=f&speed=60 ----------
static void handleDrive() {
  String d = server.arg("d");
  int speed = server.hasArg("speed") ? server.arg("speed").toInt() : 60;
  float stopCm = server.hasArg("stop") ? server.arg("stop").toFloat() : 15;   // "go" uses 25
  if (d.length() != 1 || String("fblrs").indexOf(d[0]) < 0) { sendError(400, "bad direction"); return; }
  bool ok = driveRemote(d[0], speed, sensorsDistance(), sensorsRearObstacle(), stopCm);
  if (ok) sendJson("{\"ok\":true}");
  else    sendJson(d[0] == 'f' ? "{\"ok\":false,\"why\":\"obstacle in front\"}"
                               : "{\"ok\":false,\"why\":\"obstacle behind\"}");
}

// ---------- GET /event?type=praise ----------
static void handleEvent() {
  String t = server.arg("type");
  if      (t == "praise")     moodEvent(EV_PRAISE);
  else if (t == "chat_start") moodEvent(EV_CHAT_START);
  else if (t == "chat_end")   { /* nothing yet (F2) */ }
  else if (t == "come")       driveCome(sensorsDistance());
  else if (t == "quiz_correct") {
    moodEvent(EV_QUIZ_CORRECT);
    int streak = server.arg("streak").toInt();
    if (streak > 0 && streak % 3 == 0) {          // every 3 in a row: bond +1, excited
      moodEvent(EV_QUIZ_STREAK);
      robotShowEmotion(EMO_EXCITED, 3500, true);
    } else {
      robotShowEmotion(EMO_HAPPY, 2500, true);
    }
  }
  else if (t == "quiz_wrong") {                   // encouraging, not sad
    moodEvent(EV_QUIZ_WRONG);
    robotShowEmotion(EMO_CURIOUS, 2500, false);
  }
  else { sendError(400, "unknown event"); return; }
  sendJson("{\"ok\":true}");
}

// ---------- GET /beep?p=happy ----------
static void handleBeep() {
  String p = server.arg("p");
  if      (p == "happy") soundPlay(RH_HAPPY);
  else if (p == "sad")   soundPlay(RH_SAD);
  else if (p == "alert") soundPlay(RH_SCARED);
  else { sendError(400, "unknown sound"); return; }
  sendJson("{\"ok\":true}");
}

// ---------- GET /focus?mode=work&min=25&brk=5 ----------
static void handleFocus() {
  String m = server.arg("mode");
  int min = server.hasArg("min") ? server.arg("min").toInt() : (m == "break" ? 5 : 25);
  int brk = server.hasArg("brk") ? server.arg("brk").toInt() : 5;
  if      (m == "work")  focusStartWork(min, brk);
  else if (m == "break") focusStartBreak(min);
  else if (m == "stop")  focusStop();
  else { sendError(400, "unknown mode"); return; }
  sendJson(String("{\"ok\":true,\"focus\":\"") + focusModeName() + "\",\"left_s\":" + focusSecondsLeft() + "}");
}

void webInit() {
  server.on("/", handleRoot);
  server.on("/state", handleState);
  server.on("/mind", handleMind);
  // G1c: AI's guess of the user's mood: /usermood?m=stressed&why=exam tomorrow
  server.on("/usermood", []() {
    String why = server.arg("why");
    if (why.length() > 40) why = why.substring(0, 40);
    usermoodFromAI(server.arg("m").c_str(), why.c_str());
    sendJson(String("{\"ok\":true,\"user\":") + usermoodJson() + "}");
  });
  // Settings: robot sound effects on/off  (/sound?fx=on|off, no argument = just read)
  server.on("/sound", []() {
    if (server.hasArg("fx")) speakerSetEffects(server.arg("fx") == "on");
    sendJson(speakerEffectsOn() ? "{\"fx\":1}" : "{\"fx\":0}");
  });
  // G1a: /learn?thumb=1 | thumb=-1 | reset=1
  server.on("/learn", []() {
    if (server.hasArg("reset")) { learnReset(); sendJson("{\"ok\":true}"); return; }
    if (server.hasArg("mode"))  { learnSetActive(server.arg("mode") == "on"); sendJson(learnActive() ? "{\"ok\":true,\"active\":1}" : "{\"ok\":true,\"active\":0}"); return; }
    bool counted = learnThumb(server.arg("thumb").toInt());
    sendJson(counted ? "{\"ok\":true}" : "{\"ok\":false,\"why\":\"no move to rate right now\"}");
  });
  server.on("/emotion", handleEmotion);
  server.on("/drive", handleDrive);
  server.on("/event", handleEvent);
  server.on("/beep", handleBeep);
  server.on("/focus", handleFocus);
  server.on("/play", HTTP_POST, handlePlayDone, handlePlayBody);
  server.on("/stopvoice", []() { speakerStop(); sendJson("{\"ok\":true}"); });
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("[web] server started on port 80");
}

void webUpdate() {
  server.handleClient();
}
