#pragma once
#include "actuatorRole.h"
#include "thresholdControl.h"

// Heater/cooler role: follows the paired temperature sensor ts<A>, drives the
// heater relays (raise) and cooler relays (lower) per the function variant,
// publishes ts<A>/hOn and ts<A>/cOn.
class HeaterCoolerRole : public ActuatorRole {
public:
    void setup() override;
    void onConnect() override;
    void loop() override;
    void onSensor(const String& type, const String& field, float value) override;

private:
    ThresholdControl _heater;   // DIR_RAISE
    ThresholdControl _cooler;   // DIR_LOWER
    unsigned long _lastEval = 0;
    void publishStatus();
};
