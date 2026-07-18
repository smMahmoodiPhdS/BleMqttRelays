#pragma once
#include <Arduino.h>

// Common interface every board role implements. The MQTT plumbing (addressed
// relay command/state/presence/descriptor) is shared in mqttFunctions; a role
// only handles its control logic and status.
class ActuatorRole {
public:
    virtual ~ActuatorRole() {}

    // Called once at boot after the role is selected.
    virtual void setup() {}

    // Called after each successful MQTT (re)connect — e.g. publish initial status.
    virtual void onConnect() {}

    // Called periodically from the main loop.
    virtual void loop() {}

    // Called for each paired-sensor value: type = "ts"/"hs"/"ls",
    // field = "cu"/"ll"/"ul"/…, value already parsed. Roles use what they need.
    virtual void onSensor(const String& type, const String& field, float value) {}
};
