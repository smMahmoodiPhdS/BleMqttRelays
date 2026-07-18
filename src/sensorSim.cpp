#include "sensorSim.h"
#include "roleConfig.h"
#include "mqttFunctions.h"
#include <Arduino.h>

static const bool SIMULATE = true;
static const unsigned long SIM_INTERVAL_MS = 5000;
static unsigned long lastSimMs = 0;

struct SimChannel {
    float value; float dir; float lo; float hi;
    float dayMax; float dayMin; bool init;
};

static void stepChannel(SimChannel& c) {
    c.value += c.dir;
    if (c.value >= c.hi) { c.value = c.hi; c.dir = -c.dir; }
    if (c.value <= c.lo) { c.value = c.lo; c.dir = -c.dir; }
    if (!c.init) { c.dayMax = c.value; c.dayMin = c.value; c.init = true; }
    if (c.value > c.dayMax) c.dayMax = c.value;
    if (c.value < c.dayMin) c.dayMin = c.value;
}

void sensorSim_loop() {
    if (!SIMULATE) return;
    unsigned long now = millis();
    if (now - lastSimMs < SIM_INTERVAL_MS) return;
    lastSimMs = now;

    static SimChannel tsCh = {24.0f, 0.5f, 24.0f, 30.0f, 0.0f, 0.0f, false};  // deg C
    static SimChannel hsCh = {45.0f, 1.0f, 45.0f, 75.0f, 0.0f, 0.0f, false};  // %RH

    if (roleUsesTemperature()) {
        stepChannel(tsCh);
        mqtt_publishSensorValue("ts", "cu", tsCh.value, false);
        mqtt_publishSensorValue("ts", "daily_max", tsCh.dayMax, true);
        mqtt_publishSensorValue("ts", "daily_min", tsCh.dayMin, true);
    }
    if (roleUsesHumidity()) {
        stepChannel(hsCh);
        mqtt_publishSensorValue("hs", "cu", hsCh.value, false);
        mqtt_publishSensorValue("hs", "daily_max", hsCh.dayMax, true);
        mqtt_publishSensorValue("hs", "daily_min", hsCh.dayMin, true);
    }
}
