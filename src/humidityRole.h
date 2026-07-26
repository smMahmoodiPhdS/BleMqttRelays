#pragma once
#include "actuatorRole.h"
#include "thresholdControl.h"

// Humidity role (humidify-only): follows the paired humidity sensor hs<A>,
// drives the humidifier relays (raise) when too dry, publishes hs<A>/hOn.
class HumidityRole : public ActuatorRole {
public:
    void setup() override;
    void onConnect() override;
    void loop() override;
    void onSensor(const String& type, const String& field, float value) override;

private:
    ThresholdControl _humidifier;   // DIR_RAISE
    unsigned long _lastEval = 0;
    bool _lastPub = false;
    bool _havePublished = false;
    void publishStatus();
};
