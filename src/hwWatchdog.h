#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// External TLC555 hardware watchdog (schematic sheet "watchdog").
//
//   HWD_HB --1k--> TR (555 trigger, 10k pull-up to +3V3)
//   HWD_HB --2k2-> base of Q_WD_R (BC807 PNP), emitter on WD_TIME, collector GND
//   Timing: R_WD_T 9M1 + C_WD_T 1uF  =>  1.1 * R * C ~= 10 s
//
// Idle HIGH lets WD_TIME charge; once it crosses 2/3 Vcc the 555 output fires
// and power-cycles the board. Pulsing HWD_HB LOW does two things at once: it
// holds the trigger below 1/3 Vcc and it turns the PNP on to dump C_WD_T. So a
// short LOW pulse every couple of seconds is the "kick".
//
// This runs in its own FreeRTOS task rather than from loop(). The Arduino loop
// blocks for many seconds during WiFi association, MQTT connect and especially
// an OTA download - all of them longer than the ~10 s hardware timeout. A
// dedicated task keeps the board alive through those. Fit the JP_WD jumper to
// park the watchdog while flashing over serial.
// ---------------------------------------------------------------------------

#define HW_WATCHDOG_KICK_INTERVAL_MS 2000
#define HW_WATCHDOG_PULSE_MS         20

void setup_hwWatchdog();

// Kick once from the calling context. The background task calls this on its own
// schedule; exposed mainly for long blocking sections that want to be explicit.
void hwWatchdog_kick();
