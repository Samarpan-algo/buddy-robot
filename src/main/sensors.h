// sensors.h - front ultrasonic, rear IR, touch, and the events they detect.
#pragma once

// Things the sensors noticed (bit flags, several can happen at once)
enum SensorEvent {
  SE_NONE          = 0,
  SE_SUDDEN_CLOSE  = 1,   // something came very close, fast
  SE_SLOW_APPROACH = 2,   // something came closer slowly (100 -> 40 cm in 1-2 s)
  SE_PET           = 4    // touch, or a hand held steady in front for ~2 s
};

void  sensorsInit();
int   sensorsUpdate();          // call every loop; returns SensorEvent flags
float sensorsDistance();        // median front distance in cm, -1 = nothing in range
bool  sensorsRearObstacle();    // true = rear IR sees something
bool  sensorsTouched();         // touch pad state (always false if TOUCH_ENABLED is false)
