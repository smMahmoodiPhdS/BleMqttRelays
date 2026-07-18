#pragma once
#include "actuatorRole.h"

// Light role (STUB): relays are manual (MQTT/BLE on/off) for now. On/off-hour
// scheduling from ls<A> is TODO — see Docs/Architecture reconciliation §4.2.
class LightRole : public ActuatorRole {
public:
    void setup() override;
    // loop()/onSensor(): TODO light scheduling.
};
