// ROS2D2, created by NandoMadrid (https://www.astromechspain.com/droides/r2-d2-nandomadrid/) · Astromech Spain (https://www.astromechspain.com)
// Just enough of Arduino.h for the controller layer's host tests: the clock is set by the test.
#ifndef ARDUINO_STUB_H
#define ARDUINO_STUB_H
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
extern uint32_t g_fakeMillis;
inline uint32_t millis() { return g_fakeMillis; }
#endif
