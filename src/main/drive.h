// drive.h - motors (L298N) and short, time-bounded behaviour moves.
#pragma once
#include "emotion.h"

void driveInit();                         // motor pins LOW = stopped (always, even if disabled)
void driveBehaviour(Emotion e, float distCm, bool rearBlocked, int bond);
void driveUpdate(float distCm, bool rearBlocked);   // call every loop (timing + safety)
void driveStop();
bool driveBusy();                         // true while a move or calibration is running

// Floor calibration ramps (no kick-start): PWM rises slowly, printed every 5 steps.
// Type 'x' (driveStop) the moment the robot starts moving.
void driveCalibrateForward();
void driveCalibrateSpin();
bool driveCalibrating();

// Remote control from the phone (D-pad or an AI [ACTION:...]).
// d = 'f' forward, 'b' back, 'l' spin left, 'r' spin right, 's' stop; speed 0-100 %.
// Must be repeated at least every 600 ms, otherwise the robot stops by itself.
// Returns false (and does not move) if that direction is blocked by an obstacle.
// stopCm: forward is refused / stopped when something is closer than this (15 normal, 25 for "go").
bool driveRemote(char d, int speedPct, float distCm, bool rearBlocked, float stopCm = 15);

// "Come here": drive slowly forward until something is closer than 25 cm, or 5 s.
void driveCome(float distCm);

// Social moves chosen by the learning module (G1a). Returns false if not possible now.
enum SocialMove { MOVE_WIGGLE, MOVE_SPIN_DANCE, MOVE_SLOW_APPROACH };
bool driveSocial(SocialMove m, float distCm, bool rearBlocked);
