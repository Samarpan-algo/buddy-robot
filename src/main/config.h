// config.h - on/off switches for the main firmware.
#pragma once

// Motors: false = behaviours are only printed, wheels never move (step 1.4b).
// true from step 1.4c.
#define MOTORS_ENABLED  true

// TTP223 touch sensor on D4: postponed to step 3.5. Until then a "pet" is
// a hand held steady in front of the ultrasonic sensor (see sensors.cpp).
#define TOUCH_ENABLED   false

// "Buddy hears" (mic as a sound sensor). Levels are printed in the status line
// as snd=<sharp>/<raw>; quiet room was about 15 / 30-170 in the H2 test.
#define HEAR_ENABLED      false  // off until the mic test is finished (claps not detected yet)
#define HEAR_LOUD_LEVEL   150    // sharp (high-passed) level of a clap / bang
#define HEAR_VOICE_LEVEL  400    // raw level of someone talking close by
#define HEAR_VOICE_MS     800    // talking must last this long

// Sound effects: SOUND_SPEAKER = chirps on the robot speaker (default),
// SOUND_BUZZER = old beep rhythms on the active buzzer (fallback, still wired).
#define SOUND_SPEAKER     1
#define SOUND_BUZZER      2
#define SOUND_OUTPUT      SOUND_SPEAKER

// Learning (G1a): Buddy tries "social moves" when it feels lonely and learns
// from your feedback which ones you like.
#define LEARN_ENABLED     true

// Demo timing for presentations: sad after 20 s, sleepy after 60 s of no
// interaction (instead of 60 s and 5 min).
#define DEMO_MODE       true
