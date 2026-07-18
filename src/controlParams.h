#pragma once

// Shared control tuning for all thermostat-like roles.
// NOTE: min on/off are short (10 s) for bench testing — restore ~2 min for
// production (see Docs/Architecture/PRODUCTION-CHECKLIST.md).
static const float TEMP_HYSTERESIS_C   = 0.3f;   // heater/cooler margin (deg C)
static const float HUM_HYSTERESIS_PCT  = 2.0f;   // humidifier margin (%RH)
static const unsigned long CTRL_MIN_ON_MS  = 10UL * 1000;
static const unsigned long CTRL_MIN_OFF_MS = 10UL * 1000;
static const unsigned long CTRL_INTERVAL_MS = 1000;  // evaluate ~1 Hz
