#pragma once

// Heater/cooler hysteresis control for the FN_ICH_H12C34 configuration:
//   relays 1 & 2 = Heater, relays 3 & 4 = Cooler.
//
// Reads the current temperature and lower/upper limits (fed from MQTT
// sensors/<owner>/ts1/cu, /ll, /ul), runs anti-chatter hysteresis with minimum
// on/off run times, drives the mapped relays on state changes, and publishes the
// heater/cooler status to sensors/<owner>/ts1/hOn|cOn (1 = off, 2 = on) so the
// app icons and Grafana reflect it.
//
// TODO: this is the first, hard-wired role (FN_ICH_H12C34). The board
// multifunctionality plan (reconciliation §8b) will select the mapping at
// runtime from the function switch instead of hard-coding it here.

void heaterControl_setup();
void heaterControl_setCurrent(float value);
void heaterControl_setLowerLimit(float value);
void heaterControl_setUpperLimit(float value);
void heaterControl_loop();
