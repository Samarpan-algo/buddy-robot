// sound.h - active buzzer rhythms and the WS2812 mood colour.
#pragma once
#include <stdint.h>
#include "emotion.h"

// Beep rhythms (the buzzer is ACTIVE: one fixed pitch, only on/off)
enum Rhythm { RH_HAPPY, RH_SAD, RH_SCARED, RH_GREETING, RH_ALARM };

void soundInit();
void soundPlay(Rhythm r);           // start a rhythm (non-blocking)
void rgbSetEmotion(Emotion e);      // set the LED colour for an emotion
void rgbSetColour(uint8_t r, uint8_t g, uint8_t b);   // any colour (e.g. calm focus colour)
void soundUpdate();                 // call every loop (buzzer timing + rainbow)
