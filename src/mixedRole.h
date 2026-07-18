#pragma once
#include "actuatorRole.h"
#include "thresholdControl.h"

// Versatile / mixed-role board: each relay runs its own independent control,
// possibly following a different sensor. Built from the per-relay map, e.g.
// FN_MIX_HuLgCoHt = rl1 Humidifier(hs), rl2 Light(manual), rl3 Cooler(ts),
// rl4 Heater(ts). Light relays are manual until light scheduling exists.
class MixedRole : public ActuatorRole {
public:
    void setup() override;
    void onConnect() override;
    void loop() override;
    void onSensor(const String& type, const String& field, float value) override;

private:
    struct Slot {
        bool active = false;
        ThresholdControl ctrl;
        const char* sensorType = "";   // "ts" or "hs"
        const char* statusField = "";  // "hOn" or "cOn"
    };
    Slot _slots[4];
    unsigned long _lastEval = 0;
    void publishSlot(uint8_t i);
};
