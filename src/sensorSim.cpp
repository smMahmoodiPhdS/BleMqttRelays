#include "sensorSim.h"
#include "roleConfig.h"
#include "mqttFunctions.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Bench simulator: feeds this module's paired sensor stream so the control loop
// and the app's cards can be exercised without a sensor board on the bench.
//
// It publishes `cu` ONLY.
//
// It used to also publish `daily_max` and `daily_min`, and that was a
// two-writer conflict on a retained topic. Those fields belong to Node-RED:
//
//   * Firmware/bleMqttSensor/sensor-contract.md §3.2 makes Node-RED the single
//     producer, and the broker ACL now refuses a device account that tries to
//     write them.
//   * Docs/Architecture/APP-AND-CONTRACT.md §1.6 (daily_* has one producer) already said so:
//     "daily_* is not computed in firmware - Node-RED aggregates cu over the day
//     and republishes it."
//
// The reason it matters rather than being tidiness: Node-RED has the timezone and
// a real notion of local midnight, and it survives a board reboot. A device that
// resets at 14:00 would publish a "daily max" covering four hours, and no
// consumer could tell. With two writers on a retained topic, whichever published
// last wins - which is not a deterministic system.
//
// Once a real sensor board is publishing `cu`, set SIMULATE to false rather than
// letting both sources feed the same topic.
// ---------------------------------------------------------------------------

static const bool SIMULATE = true;
static const unsigned long SIM_INTERVAL_MS = 5000;
static unsigned long lastSimMs = 0;

struct SimChannel {
    float value; float dir; float lo; float hi;
};

static void stepChannel(SimChannel& c) {
    c.value += c.dir;
    if (c.value >= c.hi) { c.value = c.hi; c.dir = -c.dir; }
    if (c.value <= c.lo) { c.value = c.lo; c.dir = -c.dir; }
}

void sensorSim_loop() {
    if (!SIMULATE) return;
    unsigned long now = millis();
    if (now - lastSimMs < SIM_INTERVAL_MS) return;
    lastSimMs = now;

    static SimChannel tsCh = {24.0f, 0.5f, 24.0f, 30.0f};   // deg C
    static SimChannel hsCh = {45.0f, 1.0f, 45.0f, 75.0f};   // %RH

    // Not retained: a retained current value outlives the simulator and reads as
    // live long after this board stopped producing it.
    if (roleUsesTemperature()) {
        stepChannel(tsCh);
        mqtt_publishSensorValue("ts", "cu", tsCh.value, false);
    }
    if (roleUsesHumidity()) {
        stepChannel(hsCh);
        mqtt_publishSensorValue("hs", "cu", hsCh.value, false);
    }
}
