// speaker.h - Buddy's own voice: 8 kHz 8-bit audio played on GPIO5
// (PWM -> 2k/10 nF RC filter -> PAM8403 -> 8 ohm speaker).
#pragma once
#include <stddef.h>
#include <stdint.h>

void   speakerInit();                 // allocate the audio buffer once, start the 8 kHz timer
size_t speakerCapacity();             // buffer size in bytes (= samples)

// Receiving a clip from the phone (POST /play), piece by piece:
void   speakerBeginUpload();          // stops any playback, buffer starts empty
void   speakerAppend(const uint8_t* data, size_t len);   // extra bytes beyond the buffer are dropped
void   speakerEndUpload();            // starts playing what was received
size_t speakerReceived();

bool   speakerPlaying();                // anything playing (speech or a sound effect)
bool   speakerSpeaking();               // speech from the phone is playing
void   speakerStop();

// Sound effects made on the robot itself (G1a): short synthesised chirps/tunes.
enum SpeakerEffect { FX_HAPPY, FX_SAD, FX_SCARED, FX_CURIOUS, FX_GREETING, FX_TUNE };
bool   speakerEffect(SpeakerEffect fx); // false if speech is playing or effects are switched off
void   speakerSetEffects(bool on);      // Settings: robot sound effects on/off (saved in NVS)
bool   speakerEffectsOn();
