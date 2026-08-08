#pragma once
#include <Arduino.h>

// Reusable single-actuator hysteresis engine shared by all thermostat-like
// roles (heater, cooler, humidifier, …). One instance drives one set of relays
// in one direction. Heater/cooler independence comes for free: with lower <
// upper, a RAISE actuator and a LOWER actuator are naturally mutually exclusive.
//
//   DIR_RAISE : ON when current <= lower - hysteresis  (heater, humidifier)
//   DIR_LOWER : ON when current >= upper + hysteresis  (cooler, ventilation)

enum ThresholdDir { DIR_RAISE, DIR_LOWER };

class ThresholdControl {
public:
    void configure(ThresholdDir dir, float hysteresis,
                   unsigned long minOnMs, unsigned long minOffMs);
    void addRelay(uint8_t relayIndex);   // relays this actuator drives (0-based)

    void setCurrent(float v) { _cur = v; _hasCur = true; }
    void setLower(float v)   { _lower = v; _hasLower = true; }
    void setUpper(float v)   { _upper = v; _hasUpper = true; }

    // Recompute; drives the relays and returns true if the on/off state changed.
    bool update();

    bool isOn() const { return _on; }        // what the controller decided
    bool ready() const;

    // What the actuator is actually doing, read back from the coil-sense
    // feedback: true if any relay this controller owns is energized. Differs
    // from isOn() when an MT-102 toggle is off AUTO or a channel has failed.
    bool isActuallyOn() const;

    // True if any relay this controller owns is not following commands.
    bool isOverridden() const;

private:
    ThresholdDir _dir = DIR_RAISE;
    float _hyst = 0.3f;
    unsigned long _minOnMs = 10000, _minOffMs = 10000;

    uint8_t _relays[4];
    uint8_t _relayCount = 0;

    float _cur = 0, _lower = 0, _upper = 0;
    bool _hasCur = false, _hasLower = false, _hasUpper = false;

    bool _on = false;
    unsigned long _onSince = 0, _offSince = 0;
};
