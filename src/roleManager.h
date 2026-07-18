#pragma once
#include <Arduino.h>

// Selects the active ActuatorRole from the function switch (roleConfig) and
// dispatches lifecycle + sensor events to it. This is the clean "function
// switching" seam — main.cpp and mqttFunctions talk only to the manager.

void roleManager_setup();
void roleManager_onConnect();
void roleManager_loop();
void roleManager_onSensor(const String& type, const String& field, float value);
