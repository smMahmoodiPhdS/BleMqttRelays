#include "heaterControl.h"
#include "relayManager.h"
#include "mqttFunctions.h"
#include "wifiManagerTools.h"
#include <Arduino.h>

// FN_ICH_H12C34 relay mapping (0-based indices): heaters = rl01,rl02 (0,1);
// coolers = rl03,rl04 (2,3).
static const uint8_t HEATER_RELAYS[] = {0, 1};
static const uint8_t COOLER_RELAYS[] = {2, 3};

// Hysteresis margin around the limits (deg C) to avoid chattering at the edge.
static const float TEMP_HYSTERESIS_C = 0.3f;

// Minimum on/off run times to protect the actuators from rapid cycling.
// NOTE: short (10 s) for bench testing — restore ~2 min for production.
static const unsigned long MIN_HEATER_ON_MS  = 10UL * 1000;
static const unsigned long MIN_COOLER_ON_MS  = 10UL * 1000;
static const unsigned long MIN_HEATER_OFF_MS = 10UL * 1000;
static const unsigned long MIN_COOLER_OFF_MS = 10UL * 1000;

static const unsigned long CONTROL_INTERVAL_MS = 1000;  // evaluate ~1 Hz

static float curTemp = 0.0f;
static float lowerLimit = 0.0f;
static float upperLimit = 0.0f;
static bool hasCurrent = false;
static bool gotLower = false;
static bool gotUpper = false;

static bool isHeaterOn = false;
static bool isCoolerOn = false;

static unsigned long heaterOnSince = 0;
static unsigned long coolerOnSince = 0;
static unsigned long heaterOffSince = 0;
static unsigned long coolerOffSince = 0;
static unsigned long lastControlMs = 0;

void heaterControl_setCurrent(float value)    { curTemp = value; hasCurrent = true; }
void heaterControl_setLowerLimit(float value) { lowerLimit = value; gotLower = true; }
void heaterControl_setUpperLimit(float value) { upperLimit = value; gotUpper = true; }

static void publishStatus() {
    String base = String("sensors/") + farmOwner + "/ts1/";
    String hTopic = base + "hOn";
    String cTopic = base + "cOn";
    mqttClient.publish(hTopic.c_str(), isHeaterOn ? "2" : "1", true);
    mqttClient.publish(cTopic.c_str(), isCoolerOn ? "2" : "1", true);
}

static void applyRelays() {
    for (uint8_t i = 0; i < 2; i++) relay_setState(HEATER_RELAYS[i], isHeaterOn);
    for (uint8_t i = 0; i < 2; i++) relay_setState(COOLER_RELAYS[i], isCoolerOn);
}

void heaterControl_setup() {
    Serial.println("HeaterControl (FN_ICH_H12C34) ready: rl01/rl02=Heater, rl03/rl04=Cooler.");
}

void heaterControl_loop() {
    // Need at least one current reading and both limits before controlling.
    if (!hasCurrent || !gotLower || !gotUpper) return;

    unsigned long now = millis();
    if (now - lastControlMs < CONTROL_INTERVAL_MS) return;
    lastControlMs = now;

    bool heaterPrev = isHeaterOn;
    bool coolerPrev = isCoolerOn;

    bool wantHeat = curTemp <= (lowerLimit - TEMP_HYSTERESIS_C);
    bool wantCool = curTemp >= (upperLimit + TEMP_HYSTERESIS_C);

    if (isHeaterOn) {
        bool ranLongEnough = (now - heaterOnSince) >= MIN_HEATER_ON_MS;
        bool minCoolerOffMet = (now - coolerOffSince) >= MIN_COOLER_OFF_MS;
        // Stop heating once back inside the band and min on-time is satisfied.
        if (ranLongEnough && curTemp >= (lowerLimit + TEMP_HYSTERESIS_C)) {
            isHeaterOn = false;
            heaterOffSince = now;
        }
        // Switch to cooling only after heater min-on and cooler min-off.
        if (isHeaterOn && ranLongEnough && wantCool && minCoolerOffMet) {
            isHeaterOn = false;
            heaterOffSince = now;
            isCoolerOn = true;
            coolerOnSince = now;
        }
    } else if (isCoolerOn) {
        bool ranLongEnough = (now - coolerOnSince) >= MIN_COOLER_ON_MS;
        bool minHeaterOffMet = (now - heaterOffSince) >= MIN_HEATER_OFF_MS;
        if (ranLongEnough && curTemp <= (upperLimit - TEMP_HYSTERESIS_C)) {
            isCoolerOn = false;
            coolerOffSince = now;
        }
        if (isCoolerOn && ranLongEnough && wantHeat && minHeaterOffMet) {
            isCoolerOn = false;
            coolerOffSince = now;
            isHeaterOn = true;
            heaterOnSince = now;
        }
    } else {
        // Both OFF — decide whether to start one (respect min-off timers).
        if (wantHeat && (now - heaterOffSince) >= MIN_HEATER_OFF_MS) {
            isHeaterOn = true;
            heaterOnSince = now;
        } else if (wantCool && (now - coolerOffSince) >= MIN_COOLER_OFF_MS) {
            isCoolerOn = true;
            coolerOnSince = now;
        }
    }

    if (isHeaterOn != heaterPrev || isCoolerOn != coolerPrev) {
        Serial.print("[CTRL] cu=");
        Serial.print(curTemp, 1);
        Serial.print(" ll=");
        Serial.print(lowerLimit, 1);
        Serial.print(" ul=");
        Serial.print(upperLimit, 1);
        Serial.print(" -> Heater ");
        Serial.print(isHeaterOn ? "ON" : "OFF");
        Serial.print(", Cooler ");
        Serial.println(isCoolerOn ? "ON" : "OFF");
        applyRelays();
        publishStatus();
    }
}
